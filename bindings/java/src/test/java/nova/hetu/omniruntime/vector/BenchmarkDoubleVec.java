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
import org.apache.arrow.vector.Float8Vector;
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
 * Double vec benchmark
 *
 * @since 2022-4-9
 */
@State(Scope.Thread)
@OutputTimeUnit(MICROSECONDS)
@Fork(1)
@Warmup(iterations = 1, batchSize = 1)
@Measurement(iterations = 10, batchSize = 1)
@BenchmarkMode(Mode.AverageTime)
public class BenchmarkDoubleVec {
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
    Float8Vector arrowDoubleVecGet;

    @Param({"1024"})
    private int rows = 1024;

    // doubleVec set/put
    private DoubleVec vecPutData;
    private DoubleVec vecSetData;
    private double[] arraySetData;
    private double[] randomData;

    // doubleVec get
    private DoubleVec vecGetData;
    private DoubleVec vecGetDatas;
    private double[] results;
    private double[] arrayGetData;
    private int[] positions;

    // arrow set
    private RootAllocator allocator1;
    private Float8Vector arrowDoubleVecSet;

    private final Random random = new Random(0);

    /**
     * init
     */
    @Setup(Level.Iteration)
    public void init() {
        // for vec set/put
        vecPutData = new DoubleVec(rows);
        vecSetData = new DoubleVec(rows);
        arraySetData = new double[rows];
        randomData = new double[rows];
        initValues(randomData, rows);

        // for vec get
        vecGetData = new DoubleVec(rows);
        initValues(vecGetData, rows);
        vecGetDatas = new DoubleVec(rows);
        initValues(vecGetDatas, rows);
        results = new double[rows];

        positions = new int[rows / 2];
        for (int i = 0; i < rows / 2; i++) {
            positions[i] = i;
        }

        // arrow set
        allocator1 = new RootAllocator(ALLOCATOR_CAPACITY);
        arrowDoubleVecSet = new Float8Vector("doubleVector1", allocator1);
        arrowDoubleVecSet.allocateNew(rows);
        arrowDoubleVecSet.setValueCount(rows);

        // arrow get
        allocator2 = new RootAllocator(ALLOCATOR_CAPACITY);
        arrowDoubleVecGet = new Float8Vector("doubleVector2", allocator2);
        arrowDoubleVecGet.allocateNew(rows);
        initValues(arrowDoubleVecGet, rows);
        arrowDoubleVecGet.setValueCount(rows);

        arrayGetData = new double[rows];
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

        arrowDoubleVecSet.close();
        allocator1.close();

        arrowDoubleVecGet.close();
        allocator2.close();
    }

    private void initValues(DoubleVec vec, int rowCount) {
        for (int i = 0; i < rowCount; i++) {
            vec.set(i, random.nextDouble());
        }
    }

    private void initValues(double[] array, int rowCount) {
        for (int i = 0; i < rowCount; i++) {
            array[i] = random.nextDouble();
        }
    }

    private void initValues(Float8Vector arrayVec, int rowCount) {
        for (int i = 0; i < rowCount; i++) {
            arrayVec.set(i, random.nextDouble());
        }
    }

    /**
     * Create double vec benchmark
     *
     * @param blackhole blackhole
     */
    @Benchmark
    public void createDoubleVecBenchmark(Blackhole blackhole) {
        List<DoubleVec> vecs = new ArrayList<>();
        for (int i = 0; i < COUNT; i++) {
            DoubleVec vec = new DoubleVec(rows);
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
        List<Float8Vector> vecs = new ArrayList<>();
        RootAllocator rootAllocator = new RootAllocator(Long.MAX_VALUE);
        for (int i = 0; i < COUNT; i++) {
            Float8Vector arrowVec = new Float8Vector("key", rootAllocator);
            arrowVec.allocateNew(rows);
            blackhole.consume(arrowVec);
            vecs.add(arrowVec);
        }
        closeArrowVec(vecs);
        rootAllocator.close();
    }

    /**
     * create double array benchmark
     *
     * @param benchmarkData benchmark data for test
     */
    @Benchmark
    public void createDoubleArrayBenchmark(Blackhole benchmarkData) {
        for (int i = 0; i < COUNT; i++) {
            benchmarkData.consume(new double[rows]);
        }
    }

    /**
     * put data for double vector
     */
    @Benchmark
    public void setDoubleVecBenchmark() {
        for (int i = 0; i < rows; i++) {
            vecSetData.set(i, randomData[i]);
        }
    }

    /**
     * copy benchmark data
     */
    @Benchmark
    public void copyArrayBenchmark() {
        double[] data = new double[rows];
        System.arraycopy(randomData, 0, data, 0, rows);
    }

    /**
     * new double vec put benchmark
     */
    @Benchmark
    public void newPutReleaseDoubleVecBenchmark() {
        DoubleVec vec = new DoubleVec(rows);
        vec.put(randomData, 0, 0, randomData.length);
        vec.close();
    }

    /**
     * Double vec put benchmark
     */
    @Benchmark
    public void putDoubleVecBenchmark() {
        vecPutData.put(randomData, 0, 0, randomData.length);
    }

    /**
     * Double array set benchmark
     */
    @Benchmark
    public void setDoubleArrayBenchmark() {
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
            arrowDoubleVecSet.set(i, randomData[i]);
        }
    }

    /**
     * Double array get benchmark
     *
     * @return sum value
     */
    @Benchmark
    public double getDoubleArrayBenchmark() {
        double sum = 0.0d;
        for (int i = 0; i < rows; i++) {
            sum += arrayGetData[i];
        }
        return sum;
    }

    /**
     * Double vec get benchmark
     *
     * @return sum value
     */
    @Benchmark
    public double getDoubleVecBenchmark() {
        double sum = 0.0d;
        for (int i = 0; i < rows; i++) {
            sum += vecGetData.get(i);
        }
        return sum;
    }

    /**
     * Double vecs get benchmark
     *
     * @return sum value
     */
    @Benchmark
    public double getsDoubleVecBenchmark() {
        double sum = 0.0d;
        double[] result = vecGetDatas.get(0, rows);
        for (double datum : result) {
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
    public double getArrowVecBenchmark() {
        double sum = 0.0d;
        int rowsTemp = this.rows;
        for (int i = 0; i < rowsTemp; i++) {
            sum += arrowDoubleVecGet.get(i);
        }
        return sum;
    }

    /**
     * Get double vec slice size
     *
     * @return slice size
     */
    @Benchmark
    public int sliceDoubleVecBenchmark() {
        DoubleVec slice = vecGetData.slice(2, rows / 2);
        return slice.getSize();
    }

    /**
     * Copy position of double vec benchmark
     *
     * @return position size
     */
    @Benchmark
    public int copyPositionDoubleVecBenchmark() {
        DoubleVec copyPosition = vecGetData.copyPositions(positions, 0, positions.length);
        return copyPosition.getSize();
    }

    private void closeArrowVec(List<Float8Vector> vecs) {
        for (Float8Vector vec : vecs) {
            vec.close();
        }
    }

    private void closeVec(List<DoubleVec> vecs) {
        for (Vec vec : vecs) {
            vec.close();
        }
    }

    public static void main(String[] args) throws Throwable {
        Options options = new OptionsBuilder().verbosity(VerboseMode.NORMAL)
                .include(".*" + BenchmarkDoubleVec.class.getSimpleName() + ".*").build();

        new Runner(options).run();
    }
}
