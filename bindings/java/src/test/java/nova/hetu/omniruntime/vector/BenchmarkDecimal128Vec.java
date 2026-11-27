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
import org.apache.arrow.vector.DecimalVector;
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

import java.math.BigDecimal;
import java.math.BigInteger;
import java.math.MathContext;
import java.util.ArrayList;
import java.util.List;
import java.util.Random;

/**
 * Decimal128 vec benchmark
 *
 * @since 2022-4-9
 */
@State(Scope.Thread)
@OutputTimeUnit(MICROSECONDS)
@Fork(1)
@Warmup(iterations = 1, batchSize = 1)
@Measurement(iterations = 10, batchSize = 1)
@BenchmarkMode(Mode.AverageTime)
public class BenchmarkDecimal128Vec {
    private static final int ALLOCATOR_CAPACITY = 1024 * 1024;
    private static final int COUNT = 1000;
    private static final int LONG_COUNT = 2; // count of long values per decimal128 value
    private static final int BITS_OF_DECIMAL = 128;
    private static final int PRECISION = 38;
    private static final int SCALE = 3;
    private static final long MIN_VALUE = 100L;

    static {
        // this parameter affects arrow get performance
        System.setProperty("arrow.enable_null_check_for_get", "false");
        // this parameter affects arrow set performance
        System.setProperty("arrow.enable_unsafe_memory_access", "true");
    }

    // arrow get
    RootAllocator allocator2;
    DecimalVector arrowDecimal128VecGet;

    @Param({"1024"})
    private int rows = 1024;

    // Decimal128Vec set/put
    private Decimal128Vec vecPutData;
    private Decimal128Vec vecSetData;
    private long[] randomData;

    // Decimal128Vec get
    private Decimal128Vec vecGetData;
    private Decimal128Vec vecGetDatas;
    private long[] results;
    private int[] positions;

    // arrow set
    private RootAllocator allocator1;
    private DecimalVector arrowDecimal128VecSet;

    private final Random random = new Random(0);

    /**
     * init
     */
    @Setup(Level.Iteration)
    public void init() {
        // for vec set/put
        vecPutData = new Decimal128Vec(rows);
        vecSetData = new Decimal128Vec(rows);
        randomData = new long[rows * LONG_COUNT];
        initValues(randomData, rows * LONG_COUNT);

        // for vec get
        vecGetData = new Decimal128Vec(rows);
        initValues(vecGetData, rows);
        vecGetDatas = new Decimal128Vec(rows);
        initValues(vecGetDatas, rows);
        results = new long[rows * LONG_COUNT];

        positions = new int[rows / 2];
        for (int i = 0; i < rows / 2; i++) {
            positions[i] = i;
        }

        // arrow set
        allocator1 = new RootAllocator(ALLOCATOR_CAPACITY);
        arrowDecimal128VecSet = new DecimalVector("decimal128Vector1", allocator1, PRECISION, SCALE);
        arrowDecimal128VecSet.allocateNew(rows);
        arrowDecimal128VecSet.setValueCount(rows);

        // arrow get
        allocator2 = new RootAllocator(ALLOCATOR_CAPACITY);
        arrowDecimal128VecGet = new DecimalVector("decimal128Vector2", allocator2, PRECISION, SCALE);
        arrowDecimal128VecGet.allocateNew(rows);
        initValues(arrowDecimal128VecGet, rows);
        arrowDecimal128VecGet.setValueCount(rows);
    }

    /**
     * close
     */
    @TearDown(Level.Iteration)
    public void tearDown() {
        vecPutData.close();
        vecSetData.close();
        vecGetData.close();

        arrowDecimal128VecSet.close();
        allocator1.close();

        arrowDecimal128VecGet.close();
        allocator2.close();
    }

    private void initValues(Decimal128Vec vec, int rowCount) {
        for (int i = 0; i < rowCount; i++) {
            long[] val = {random.nextLong(), random.nextLong()};
            vec.set(i, val);
        }
    }

    private void initValues(long[] array, int rowCount) {
        for (int i = 0; i < rowCount; i++) {
            array[i] = random.nextLong();
        }
    }

    private BigDecimal getOneBigDecimalValue() {
        MathContext mc = new MathContext(PRECISION);
        BigInteger bigInteger;
        while (true) {
            bigInteger = new BigInteger(String.valueOf(random.nextLong()));
            if (bigInteger.compareTo(BigInteger.valueOf(MIN_VALUE)) > 0) {
                break;
            }
        }

        return new BigDecimal(bigInteger, SCALE, mc);
    }

    private void initValues(DecimalVector arrayVec, int rowCount) {
        for (int i = 0; i < rowCount; i++) {
            arrayVec.set(i, getOneBigDecimalValue());
        }
    }

    /**
     * Create long vec benchmark
     *
     * @param blackhole blackhole
     */
    @Benchmark
    public void createDecimal128VecBenchmark(Blackhole blackhole) {
        List<Decimal128Vec> vecs = new ArrayList<>();
        for (int i = 0; i < COUNT; i++) {
            Decimal128Vec vec = new Decimal128Vec(rows);
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
        List<DecimalVector> vecs = new ArrayList<>();
        RootAllocator rootAllocator = new RootAllocator(Long.MAX_VALUE);
        for (int i = 0; i < COUNT; i++) {
            DecimalVector arrowVec = new DecimalVector("key", rootAllocator, PRECISION, SCALE);
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
    public void createDecimal128ArrayBenchmark(Blackhole benchmarkData) {
        for (int i = 0; i < COUNT; i++) {
            benchmarkData.consume(new long[rows]);
        }
    }

    /**
     * put data for decimal128 vector
     */
    @Benchmark
    public void setDecimal128VecBenchmark() {
        for (int i = 0; i < rows; i++) {
            long[] valArray = {randomData[i], randomData[i] / 2};
            vecSetData.set(i, valArray);
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
     * new decimal128 vec put benchmark
     */
    @Benchmark
    public void newPutReleaseDecimal128VecBenchmark() {
        Decimal128Vec vec = new Decimal128Vec(rows);
        vec.put(randomData, 0, 0, randomData.length);
        vec.close();
    }

    /**
     * Decimal128 vec put benchmark
     */
    @Benchmark
    public void putDecimal128VecBenchmark() {
        vecPutData.put(randomData, 0, 0, randomData.length);
    }

    /**
     * Arrow vec set benchmark
     */
    @Benchmark
    public void setArrowVecBenchmark() {
        for (int i = 0; i < rows; i++) {
            arrowDecimal128VecSet.set(i, getOneBigDecimalValue());
        }
    }

    /**
     * Decimal128 vec get benchmark
     *
     * @return sum value
     */
    @Benchmark
    public long getDecimal128VecBenchmark() {
        long sum = 0L;
        for (int i = 0; i < rows; i++) {
            sum += vecGetData.get(i)[0];
        }
        return sum;
    }

    /**
     * Decimal128 vecs get benchmark
     *
     * @return sum value
     */
    @Benchmark
    public long getsDecimal128VecBenchmark() {
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
            sum += arrowDecimal128VecGet.get(i).getLong(0);
        }
        return sum;
    }

    /**
     * Get decimal128 vec slice size
     *
     * @return slice size
     */
    @Benchmark
    public int sliceDecimal128VecBenchmark() {
        Decimal128Vec slice = vecGetData.slice(2, rows / 2);
        return slice.getSize();
    }

    /**
     * Copy position of decimal128 vec benchmark
     *
     * @return position size
     */
    @Benchmark
    public int copyPositionDecimal128VecBenchmark() {
        Decimal128Vec copyPosition = vecGetData.copyPositions(positions, 0, positions.length);
        return copyPosition.getSize();
    }

    private void closeArrowVec(List<DecimalVector> vecs) {
        for (DecimalVector vec : vecs) {
            vec.close();
        }
    }

    private void closeVec(List<Decimal128Vec> vecs) {
        for (Vec vec : vecs) {
            vec.close();
        }
    }

    public static void main(String[] args) throws Throwable {
        Options options = new OptionsBuilder().verbosity(VerboseMode.NORMAL)
                .include(".*" + BenchmarkDecimal128Vec.class.getSimpleName() + ".*").build();

        new Runner(options).run();
    }
}
