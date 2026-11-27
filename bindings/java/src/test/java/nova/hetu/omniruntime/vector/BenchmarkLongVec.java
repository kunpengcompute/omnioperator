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
import org.apache.arrow.vector.BigIntVector;
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
 * Long vec benchmark
 *
 * @since 2021-8-10
 */
@State(Scope.Thread)
@OutputTimeUnit(MICROSECONDS)
@Fork(1)
@Warmup(iterations = 1, batchSize = 1)
@Measurement(iterations = 10, batchSize = 1)
@BenchmarkMode(Mode.AverageTime)
public class BenchmarkLongVec {
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
    BigIntVector arrowLongVecGet;

    @Param({"1024"})
    private int rows = 1024;

    // longVec set/put
    private LongVec vecPutData;
    private LongVec vecSetData;
    private long[] arraySetData;
    private long[] randomData;

    // longVec get
    private LongVec vecGetData;
    private LongVec vecGetDatas;
    private long[] results;
    private long[] arrayGetData;
    private int[] positions;

    // arrow set
    private RootAllocator allocator1;
    private BigIntVector arrowLongVecSet;

    private final Random random = new Random(0);

    /**
     * init
     */
    @Setup(Level.Iteration)
    public void init() {
        // for vec set/put
        vecPutData = new LongVec(rows);
        vecSetData = new LongVec(rows);
        arraySetData = new long[rows];
        randomData = new long[rows];
        initValues(randomData, rows);

        // for vec get
        vecGetData = new LongVec(rows);
        initValues(vecGetData, rows);
        vecGetDatas = new LongVec(rows);
        initValues(vecGetDatas, rows);
        results = new long[rows];

        positions = new int[rows / 2];
        for (int i = 0; i < rows / 2; i++) {
            positions[i] = i;
        }

        // arrow set
        allocator1 = new RootAllocator(ALLOCATOR_CAPACITY);
        arrowLongVecSet = new BigIntVector("longVector1", allocator1);
        arrowLongVecSet.allocateNew(rows);
        arrowLongVecSet.setValueCount(rows);

        // arrow get
        allocator2 = new RootAllocator(ALLOCATOR_CAPACITY);
        arrowLongVecGet = new BigIntVector("longVector2", allocator2);
        arrowLongVecGet.allocateNew(rows);
        initValues(arrowLongVecGet, rows);
        arrowLongVecGet.setValueCount(rows);

        arrayGetData = new long[rows];
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

        arrowLongVecSet.close();
        allocator1.close();

        arrowLongVecGet.close();
        allocator2.close();
    }

    private void initValues(LongVec vec, int rowCount) {
        for (int i = 0; i < rowCount; i++) {
            vec.set(i, random.nextLong());
        }
    }

    private void initValues(long[] array, int rowCount) {
        for (int i = 0; i < rowCount; i++) {
            array[i] = random.nextLong();
        }
    }

    private void initValues(BigIntVector arrayVec, int rowCount) {
        for (int i = 0; i < rowCount; i++) {
            arrayVec.set(i, random.nextLong());
        }
    }

    /**
     * Create long vec benchmark
     *
     * @param blackhole blackhole
     */
    @Benchmark
    public void createLongVecBenchmark(Blackhole blackhole) {
        List<LongVec> vecs = new ArrayList<>();
        for (int i = 0; i < COUNT; i++) {
            LongVec vec = new LongVec(rows);
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
        List<BigIntVector> vecs = new ArrayList<>();
        RootAllocator rootAllocator = new RootAllocator(Long.MAX_VALUE);
        for (int i = 0; i < COUNT; i++) {
            BigIntVector arrowVec = new BigIntVector("key", rootAllocator);
            arrowVec.allocateNew(rows);
            blackhole.consume(arrowVec);
            vecs.add(arrowVec);
        }
        closeArrowVec(vecs);
        rootAllocator.close();
    }

    /**
     * create long array benchmark
     *
     * @param benchmarkData benchmark data for test
     */
    @Benchmark
    public void createLongArrayBenchmark(Blackhole benchmarkData) {
        for (int i = 0; i < COUNT; i++) {
            benchmarkData.consume(new long[rows]);
        }
    }

    /**
     * put data for long vector
     */
    @Benchmark
    public void setLongVecBenchmark() {
        for (int i = 0; i < rows; i++) {
            vecSetData.set(i, randomData[i]);
        }
    }

    /**
     * copy benchmark data
     */
    @Benchmark
    public void copyArrayBenchmark() {
        long[] data = new long[rows];
        System.arraycopy(randomData, 0, data, 0, rows);
    }

    /**
     * new long vec put benchmark
     */
    @Benchmark
    public void newPutReleaseLongVecBenchmark() {
        LongVec vec = new LongVec(rows);
        vec.put(randomData, 0, 0, randomData.length);
        vec.close();
    }

    /**
     * Long vec put benchmark
     */
    @Benchmark
    public void putLongVecBenchmark() {
        vecPutData.put(randomData, 0, 0, randomData.length);
    }

    /**
     * Long array set benchmark
     */
    @Benchmark
    public void setLongArrayBenchmark() {
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
            arrowLongVecSet.set(i, randomData[i]);
        }
    }

    /**
     * Long array get benchmark
     *
     * @return sum value
     */
    @Benchmark
    public long getLongArrayBenchmark() {
        long sum = 0L;
        for (int i = 0; i < rows; i++) {
            sum += arrayGetData[i];
        }
        return sum;
    }

    /**
     * Long vec get benchmark
     *
     * @return sum value
     */
    @Benchmark
    public long getLongVecBenchmark() {
        long sum = 0L;
        for (int i = 0; i < rows; i++) {
            sum += vecGetData.get(i);
        }
        return sum;
    }

    /**
     * Long vecs get benchmark
     *
     * @return sum value
     */
    @Benchmark
    public long getsLongVecBenchmark() {
        long sum = 0L;
        long[] result = vecGetDatas.get(0, rows);
        for (long datum : result) {
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
            sum += arrowLongVecGet.get(i);
        }
        return sum;
    }

    /**
     * Get long vec slice size
     *
     * @return slice size
     */
    @Benchmark
    public int sliceLongVecBenchmark() {
        LongVec slice = vecGetData.slice(2, rows / 2);
        return slice.getSize();
    }

    /**
     * Copy position of long vec benchmark
     *
     * @return position size
     */
    @Benchmark
    public int copyPositionLongVecBenchmark() {
        LongVec copyPosition = vecGetData.copyPositions(positions, 0, positions.length);
        return copyPosition.getSize();
    }

    private void closeArrowVec(List<BigIntVector> vecs) {
        for (BigIntVector vec : vecs) {
            vec.close();
        }
    }

    private void closeVec(List<LongVec> vecs) {
        for (Vec vec : vecs) {
            vec.close();
        }
    }

    public static void main(String[] args) throws Throwable {
        Options options = new OptionsBuilder().verbosity(VerboseMode.NORMAL)
                .include(".*" + BenchmarkLongVec.class.getSimpleName() + ".*").build();

        new Runner(options).run();
    }
}
