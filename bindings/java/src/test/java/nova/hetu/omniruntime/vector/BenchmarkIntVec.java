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
import org.apache.arrow.vector.IntVector;
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
 * Int vec benchmark
 *
 * @since 2022-4-9
 */
@State(Scope.Thread)
@OutputTimeUnit(MICROSECONDS)
@Fork(1)
@Warmup(iterations = 1, batchSize = 1)
@Measurement(iterations = 10, batchSize = 1)
@BenchmarkMode(Mode.AverageTime)
public class BenchmarkIntVec {
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
    IntVector arrowIntVecGet;

    @Param({"1024"})
    private int rows = 1024;

    // intVec set/put
    private IntVec vecPutData;
    private IntVec vecSetData;
    private int[] arraySetData;
    private int[] randomData;

    // intVec get
    private IntVec vecGetData;
    private IntVec vecGetDatas;
    private int[] results;
    private int[] arrayGetData;
    private int[] positions;

    // arrow set
    private RootAllocator allocator1;
    private IntVector arrowIntVecSet;

    private final Random random = new Random(0);

    /**
     * init
     */
    @Setup(Level.Iteration)
    public void init() {
        // for vec set/put
        vecPutData = new IntVec(rows);
        vecSetData = new IntVec(rows);
        arraySetData = new int[rows];
        randomData = new int[rows];
        initValues(randomData, rows);

        // for vec get
        vecGetData = new IntVec(rows);
        initValues(vecGetData, rows);
        vecGetDatas = new IntVec(rows);
        initValues(vecGetDatas, rows);
        results = new int[rows];

        positions = new int[rows / 2];
        for (int i = 0; i < rows / 2; i++) {
            positions[i] = i;
        }

        // arrow set
        allocator1 = new RootAllocator(ALLOCATOR_CAPACITY);
        arrowIntVecSet = new IntVector("IntVector1", allocator1);
        arrowIntVecSet.allocateNew(rows);
        arrowIntVecSet.setValueCount(rows);

        // arrow get
        allocator2 = new RootAllocator(ALLOCATOR_CAPACITY);
        arrowIntVecGet = new IntVector("IntVector2", allocator2);
        arrowIntVecGet.allocateNew(rows);
        initValues(arrowIntVecGet, rows);
        arrowIntVecGet.setValueCount(rows);

        arrayGetData = new int[rows];
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

        arrowIntVecSet.close();
        allocator1.close();

        arrowIntVecGet.close();
        allocator2.close();
    }

    private void initValues(IntVec vec, int rowCount) {
        for (int i = 0; i < rowCount; i++) {
            vec.set(i, random.nextInt());
        }
    }

    private void initValues(int[] array, int rowCount) {
        for (int i = 0; i < rowCount; i++) {
            array[i] = random.nextInt();
        }
    }

    private void initValues(IntVector arrayVec, int rowCount) {
        for (int i = 0; i < rowCount; i++) {
            arrayVec.set(i, random.nextInt());
        }
    }

    /**
     * Create int vec benchmark
     *
     * @param blackhole blackhole
     */
    @Benchmark
    public void createIntVecBenchmark(Blackhole blackhole) {
        List<IntVec> vecs = new ArrayList<>();
        for (int i = 0; i < COUNT; i++) {
            IntVec vec = new IntVec(rows);
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
        List<IntVector> vecs = new ArrayList<>();
        RootAllocator rootAllocator = new RootAllocator(Long.MAX_VALUE);
        for (int i = 0; i < COUNT; i++) {
            IntVector arrowVec = new IntVector("key", rootAllocator);
            arrowVec.allocateNew(rows);
            blackhole.consume(arrowVec);
            vecs.add(arrowVec);
        }
        closeArrowVec(vecs);
        rootAllocator.close();
    }

    /**
     * create int array benchmark
     *
     * @param benchmarkData benchmark data for test
     */
    @Benchmark
    public void createIntArrayBenchmark(Blackhole benchmarkData) {
        for (int i = 0; i < COUNT; i++) {
            benchmarkData.consume(new int[rows]);
        }
    }

    /**
     * put data for int vector
     */
    @Benchmark
    public void setIntVecBenchmark() {
        for (int i = 0; i < rows; i++) {
            vecSetData.set(i, randomData[i]);
        }
    }

    /**
     * copy benchmark data
     */
    @Benchmark
    public void copyArrayBenchmark() {
        int[] data = new int[rows];
        System.arraycopy(randomData, 0, data, 0, rows);
    }

    /**
     * new int vec put benchmark
     */
    @Benchmark
    public void newPutReleaseIntVecBenchmark() {
        IntVec vec = new IntVec(rows);
        vec.put(randomData, 0, 0, randomData.length);
        vec.close();
    }

    /**
     * Int vec put benchmark
     */
    @Benchmark
    public void putIntVecBenchmark() {
        vecPutData.put(randomData, 0, 0, randomData.length);
    }

    /**
     * Int array set benchmark
     */
    @Benchmark
    public void setIntArrayBenchmark() {
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
            arrowIntVecSet.set(i, randomData[i]);
        }
    }

    /**
     * Int array get benchmark
     *
     * @return sum value
     */
    @Benchmark
    public long getIntArrayBenchmark() {
        long sum = 0L;
        for (int i = 0; i < rows; i++) {
            sum += arrayGetData[i];
        }
        return sum;
    }

    /**
     * Int vec get benchmark
     *
     * @return sum value
     */
    @Benchmark
    public long getIntVecBenchmark() {
        long sum = 0L;
        for (int i = 0; i < rows; i++) {
            sum += vecGetData.get(i);
        }
        return sum;
    }

    /**
     * Int vecs get benchmark
     *
     * @return sum value
     */
    @Benchmark
    public long getsIntVecBenchmark() {
        long sum = 0L;
        int[] result = vecGetDatas.get(0, rows);
        for (int datum : result) {
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
            sum += arrowIntVecGet.get(i);
        }
        return sum;
    }

    /**
     * Get int vec slice size
     *
     * @return slice size
     */
    @Benchmark
    public int sliceIntVecBenchmark() {
        IntVec slice = vecGetData.slice(2, rows / 2);
        return slice.getSize();
    }

    /**
     * Copy position of int vec benchmark
     *
     * @return position size
     */
    @Benchmark
    public int copyPositionIntVecBenchmark() {
        IntVec copyPosition = vecGetData.copyPositions(positions, 0, positions.length);
        return copyPosition.getSize();
    }

    private void closeArrowVec(List<IntVector> vecs) {
        for (IntVector vec : vecs) {
            vec.close();
        }
    }

    private void closeVec(List<IntVec> vecs) {
        for (Vec vec : vecs) {
            vec.close();
        }
    }

    public static void main(String[] args) throws Throwable {
        Options options = new OptionsBuilder().verbosity(VerboseMode.NORMAL)
                .include(".*" + BenchmarkIntVec.class.getSimpleName() + ".*").build();

        new Runner(options).run();
    }
}
