/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

package nova.hetu.omniruntime.vector;

import static java.util.concurrent.TimeUnit.MICROSECONDS;

import org.apache.arrow.memory.RootAllocator;
import org.apache.arrow.vector.SmallIntVector;
import org.openjdk.jmh.annotations.Benchmark;
import org.openjdk.jmh.annotations.BenchmarkMode;
import org.openjdk.jmh.annotations.Fork;
import org.openjdk.jmh.annotations.Level;
import org.openjdk.jmh.annotations.Measurement;
import org.openjdk.jmh.annotations.Mode;
import org.openjdk.jmh.annotations.OutputTimeUnit;
import org.openjdk.jmh.annotations.Param;
import org.openjdk.jmh.annotations.Scope;
import org.openjdk.jmh.annotations.Setup;
import org.openjdk.jmh.annotations.State;
import org.openjdk.jmh.annotations.TearDown;
import org.openjdk.jmh.annotations.Warmup;
import org.openjdk.jmh.infra.Blackhole;
import org.openjdk.jmh.runner.Runner;
import org.openjdk.jmh.runner.options.Options;
import org.openjdk.jmh.runner.options.OptionsBuilder;
import org.openjdk.jmh.runner.options.VerboseMode;

import java.util.ArrayList;
import java.util.List;
import java.util.Random;

/**
 * Short vec benchmark
 *
 * @since 2022-8-2
 */
@State(Scope.Thread)
@OutputTimeUnit(MICROSECONDS)
@Fork(1)
@Warmup(iterations = 1, batchSize = 1)
@Measurement(iterations = 10, batchSize = 1)
@BenchmarkMode(Mode.AverageTime)
public class BenchmarkShortVec {
    private static final int ALLOCATOR_CAPACITY = 1024 * 1024;
    private static final int COUNT = 1000;

    static {
        // this parameter affects arrow get performance
        System.setProperty("arrow.enable_null_check_for_get", "false");
        // this parameter affects arrow set performance
        System.setProperty("arrow.enable_unsafe_memory_access", "true");
    }

    // arrow get
    RootAllocator allocator2;
    SmallIntVector arrowShortVecGet;

    @Param({"1024"})
    private int rows = 1024;

    // ShortVec set/put
    private ShortVec vecPutData;
    private ShortVec vecSetData;
    private short[] arraySetData;
    private short[] randomData;

    // ShortVec get
    private ShortVec vecGetData;
    private ShortVec vecGetDatas;
    private short[] results;
    private short[] arrayGetData;
    private int[] positions;

    // arrow set
    private RootAllocator allocator1;
    private SmallIntVector arrowShortVecSet;

    private final Random random = new Random(0);

    /**
     * init
     */
    @Setup(Level.Iteration)
    public void init() {
        // for vec set/put
        vecPutData = new ShortVec(rows);
        vecSetData = new ShortVec(rows);
        arraySetData = new short[rows];
        randomData = new short[rows];
        initValues(randomData, rows);

        // for vec get
        vecGetData = new ShortVec(rows);
        initValues(vecGetData, rows);
        vecGetDatas = new ShortVec(rows);
        initValues(vecGetDatas, rows);
        results = new short[rows];

        positions = new int[rows / 2];
        for (int i = 0; i < rows / 2; i++) {
            positions[i] = i;
        }

        // arrow set
        allocator1 = new RootAllocator(ALLOCATOR_CAPACITY);
        arrowShortVecSet = new SmallIntVector("SmallIntVector1", allocator1);
        arrowShortVecSet.allocateNew(rows);
        arrowShortVecSet.setValueCount(rows);

        // arrow get
        allocator2 = new RootAllocator(ALLOCATOR_CAPACITY);
        arrowShortVecGet = new SmallIntVector("SmallIntVector2", allocator2);
        arrowShortVecGet.allocateNew(rows);
        initValues(arrowShortVecGet, rows);
        arrowShortVecGet.setValueCount(rows);

        arrayGetData = new short[rows];
        initValues(arrayGetData, rows);
    }

    /**
     * close
     */
    @TearDown(Level.Iteration)
    public void tearDown() {
        vecPutData.close();
        vecSetData.close();
        vecGetData.close();
        vecGetDatas.close();

        arrowShortVecSet.close();
        allocator1.close();

        arrowShortVecGet.close();
        allocator2.close();
    }

    private void initValues(ShortVec vec, int rowCount) {
        for (int i = 0; i < rowCount; i++) {
            vec.set(i, (short) random.nextInt(Short.MAX_VALUE));
        }
    }

    private void initValues(short[] array, int rowCount) {
        for (int i = 0; i < rowCount; i++) {
            array[i] = (short) random.nextInt(Short.MAX_VALUE);
        }
    }

    private void initValues(SmallIntVector arrayVec, int rowCount) {
        for (int i = 0; i < rowCount; i++) {
            arrayVec.set(i, (short) random.nextInt(Short.MAX_VALUE));
        }
    }

    /**
     * Create short vec benchmark
     *
     * @param blackhole blackhole
     */
    @Benchmark
    public void createShortVecBenchmark(Blackhole blackhole) {
        List<ShortVec> vecs = new ArrayList<>();
        for (int i = 0; i < COUNT; i++) {
            ShortVec vec = new ShortVec(rows);
            blackhole.consume(vec);
            vecs.add(vec);
        }
        closeVec(vecs);
    }

    /**
     * create benchmark for arrow vector
     *
     * @param blackhole blackhole
     */
    @Benchmark
    public void createArrowVecBenchmark(Blackhole blackhole) {
        List<SmallIntVector> vecs = new ArrayList<>();
        RootAllocator rootAllocator = new RootAllocator(Long.MAX_VALUE);
        for (int i = 0; i < COUNT; i++) {
            SmallIntVector arrowVec = new SmallIntVector("key", rootAllocator);
            arrowVec.allocateNew(rows);
            blackhole.consume(arrowVec);
            vecs.add(arrowVec);
        }
        closeArrowVec(vecs);
        rootAllocator.close();
    }

    /**
     * create short array benchmark
     *
     * @param benchmarkData benchmark data for test
     */
    @Benchmark
    public void createShortArrayBenchmark(Blackhole benchmarkData) {
        for (int i = 0; i < COUNT; i++) {
            benchmarkData.consume(new short[rows]);
        }
    }

    /**
     * put data for short vector
     */
    @Benchmark
    public void setShortVecBenchmark() {
        for (int i = 0; i < rows; i++) {
            vecSetData.set(i, randomData[i]);
        }
    }

    /**
     * copy benchmark data
     */
    @Benchmark
    public void copyArrayBenchmark() {
        short[] data = new short[rows];
        System.arraycopy(randomData, 0, data, 0, rows);
    }

    /**
     * new short vec put benchmark
     */
    @Benchmark
    public void newPutReleaseShortVecBenchmark() {
        ShortVec vec = new ShortVec(rows);
        vec.put(randomData, 0, 0, randomData.length);
        vec.close();
    }

    /**
     * Short vec put benchmark
     */
    @Benchmark
    public void putShortVecBenchmark() {
        vecPutData.put(randomData, 0, 0, randomData.length);
    }

    /**
     * Short array set benchmark
     */
    @Benchmark
    public void setShortArrayBenchmark() {
        if (rows >= 0) {
            System.arraycopy(randomData, 0, arraySetData, 0, rows);
        }
    }

    /**
     * Arrow vec set benchmark
     */
    @Benchmark
    public void setArrowVecBenchmark() {
        for (int i = 0; i < rows; i++) {
            arrowShortVecSet.set(i, randomData[i]);
        }
    }

    /**
     * Short array get benchmark
     *
     * @return sum value
     */
    @Benchmark
    public long getShortArrayBenchmark() {
        long sum = 0L;
        for (int i = 0; i < rows; i++) {
            sum += arrayGetData[i];
        }
        return sum;
    }

    /**
     * Short vec get benchmark
     *
     * @return sum value
     */
    @Benchmark
    public long getShortVecBenchmark() {
        long sum = 0L;
        for (int i = 0; i < rows; i++) {
            sum += vecGetData.get(i);
        }
        return sum;
    }

    /**
     * Short vecs get benchmark
     *
     * @return sum value
     */
    @Benchmark
    public long getsShortVecBenchmark() {
        long sum = 0L;
        short[] result = vecGetDatas.get(0, rows);
        for (short datum : result) {
            sum += datum;
        }
        return sum;
    }

    /**
     * Arrow vec get benchmark
     *
     * @return sum value
     */
    @Benchmark
    public long getArrowVecBenchmark() {
        long sum = 0L;
        int rowsTemp = this.rows;
        for (int i = 0; i < rowsTemp; i++) {
            sum += arrowShortVecGet.get(i);
        }
        return sum;
    }

    /**
     * Get Short vec slice size
     *
     * @return slice size
     */
    @Benchmark
    public int sliceShortVecBenchmark() {
        ShortVec slice = vecGetData.slice(2, rows / 2);
        int size = slice.getSize();
        slice.close();
        return size;
    }

    /**
     * Copy position of int vec benchmark
     *
     * @return position size
     */
    @Benchmark
    public int copyPositionShortVecBenchmark() {
        ShortVec copyPosition = vecGetData.copyPositions(positions, 0, positions.length);
        int size = copyPosition.getSize();
        copyPosition.close();
        return size;
    }

    private void closeArrowVec(List<SmallIntVector> vecs) {
        for (SmallIntVector vec : vecs) {
            vec.close();
        }
    }

    private void closeVec(List<ShortVec> vecs) {
        for (Vec vec : vecs) {
            vec.close();
        }
    }

    public static void main(String[] args) throws Throwable {
        Options options = new OptionsBuilder().verbosity(VerboseMode.NORMAL)
                .include(".*" + BenchmarkShortVec.class.getSimpleName() + ".*").build();

        new Runner(options).run();
    }
}
