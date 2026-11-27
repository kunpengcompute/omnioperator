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
import org.apache.arrow.vector.VarCharVector;
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

import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;

/**
 * Varchar vec benchmark
 *
 * @since 2021-8-10
 */
@State(Scope.Thread)
@OutputTimeUnit(MICROSECONDS)
@Fork(1)
@Warmup(iterations = 1, batchSize = 1)
@Measurement(iterations = 10, batchSize = 1)
@BenchmarkMode(Mode.AverageTime)
public class BenchmarkVarcharVec {
    private static final int ALLOCATOR_CAPACITY = 1024 * 1024;
    private static final int COUNT = 1000;

    static {
        // this parameter affects arrow get performance
        System.setProperty("arrow.enable_null_check_for_get", "false");
        // this parameter affects arrow set performance
        System.setProperty("arrow.enable_unsafe_memory_access", "true");
    }

    @Param({"1024"})
    private int rows = 1024;

    // varcharVec set/put
    private VarcharVec vecSetData;
    private VarcharVec vecPutData;
    private VarcharVecTest putDataSource;

    // heap byteBuffer
    private VarcharVecTest vecTestSetData;

    private ByteBuffer[] byteValues;

    // varcharVec get
    private VarcharVec vecGetData;
    private VarcharVecTest varcharVecTest;
    private int[] positions;

    // arrow set
    private RootAllocator allocator1;
    private VarCharVector arrowVecSet;

    // arrow get
    private RootAllocator allocator2;
    private VarCharVector arrowVecGet;

    /**
     * init
     */
    @Setup(Level.Iteration)
    public void init() {
        // for varchar set/put
        vecPutData = new VarcharVec(rows);
        putDataSource = new VarcharVecTest(rows * 8, rows);
        initValues(putDataSource, rows);

        vecSetData = new VarcharVec(rows);
        vecTestSetData = new VarcharVecTest(rows * 8, rows);

        byteValues = new ByteBuffer[rows];
        for (int i = 0; i < rows; i++) {
            String str = String.valueOf(i * 1000);
            ByteBuffer buffer = ByteBuffer.allocate(str.length());
            buffer.put(str.getBytes(StandardCharsets.UTF_8), 0, str.length());
            byteValues[i] = buffer;
        }

        // varcharVec get
        vecGetData = new VarcharVec(rows);
        initValues(vecGetData, rows);
        varcharVecTest = new VarcharVecTest(rows * 8, rows);
        initValues(varcharVecTest, rows);

        positions = new int[rows / 2];
        for (int i = 0; i < rows / 2; i++) {
            positions[i] = i;
        }

        // arrow set
        allocator1 = new RootAllocator(ALLOCATOR_CAPACITY);
        arrowVecSet = new VarCharVector("varchar1", allocator1);
        arrowVecSet.allocateNew(rows * 8);
        arrowVecSet.setValueCount(rows);

        // arrow get
        allocator2 = new RootAllocator(ALLOCATOR_CAPACITY);
        arrowVecGet = new VarCharVector("longVector2", allocator2);
        arrowVecGet.allocateNew(rows);
        initValues(arrowVecGet, rows);
        arrowVecGet.setValueCount(rows);
    }

    /**
     * close
     */
    @TearDown(Level.Iteration)
    public void tearDown() {
        vecPutData.close();
        vecSetData.close();
        vecGetData.close();

        arrowVecSet.close();
        allocator1.close();

        arrowVecGet.close();
        allocator2.close();
    }

    private void initValues(VarcharVec vec, int rowCount) {
        for (int i = 0; i < rowCount; i++) {
            vec.set(i, String.valueOf(i * 1000).getBytes(StandardCharsets.UTF_8));
        }
    }

    private void initValues(VarcharVecTest heapByteBuf, int rowCount) {
        for (int i = 0; i < rowCount; i++) {
            heapByteBuf.set(i, String.valueOf(i * 1000).getBytes(StandardCharsets.UTF_8));
        }
    }

    private void initValues(VarCharVector arrayVec, int rowCount) {
        for (int i = 0; i < rowCount; i++) {
            arrayVec.set(i, String.valueOf(i * 1000).getBytes(StandardCharsets.UTF_8));
        }
    }

    /**
     * Create varchar vec benchmark
     *
     * @param blackhole blackhole
     */
    @Benchmark
    public void createVarcharVecBenchmark(Blackhole blackhole) {
        List<VarcharVec> vecs = new ArrayList<>();
        for (int i = 0; i < COUNT; i++) {
            VarcharVec vec = new VarcharVec(rows);
            blackhole.consume(vec);
            vecs.add(vec);
        }
        closeVec(vecs);
    }

    /**
     * Create arrow vec benchmark
     *
     * @param blackhole blackhole
     */
    @Benchmark
    public void createArrowVecBenchmark(Blackhole blackhole) {
        List<VarCharVector> vecs = new ArrayList<>();
        RootAllocator rootAllocator = new RootAllocator(Long.MAX_VALUE);
        for (int i = 0; i < COUNT; i++) {
            VarCharVector arrowVec = new VarCharVector("key", rootAllocator);
            arrowVec.allocateNew(rows, rows);
            blackhole.consume(arrowVec);
            vecs.add(arrowVec);
        }
        closeArrowVec(vecs);
        rootAllocator.close();
    }

    /**
     * Put varchar vec benchmark
     *
     * @param blackhole blackhole
     */
    @Benchmark
    public void putVarcharVecBenchmark(Blackhole blackhole) {
        vecPutData.put(0, putDataSource.getData(), 0, putDataSource.offsets, 0, rows);
    }

    /**
     * Set arrow vec benchmark
     *
     * @param blackhole blackhole
     */
    @Benchmark
    public void setArrowVecBenchmark(Blackhole blackhole) {
        for (int i = 0; i < rows; i++) {
            arrowVecSet.set(i, byteValues[i].array());
        }
    }

    /**
     * Set heap bytebuffer benchmark
     *
     * @param blackhole blackhole
     */
    @Benchmark
    public void setHeapBytebufferBenchmark(Blackhole blackhole) {
        for (int i = 0; i < rows; i++) {
            vecTestSetData.set(i, byteValues[i].array());
        }
    }

    /**
     * Set varchar vec benchmark
     */
    @Benchmark
    public void setVarcharVecBenchmark() {
        for (int i = 0; i < rows; i++) {
            vecSetData.set(i, byteValues[i].array());
        }
    }

    /**
     * Get heap bytebuffer benchmark
     *
     * @return sum value
     */
    @Benchmark
    public long getHeapBytebufferBenchmark() {
        long sum = 0L;
        for (int i = 0; i < rows; i++) {
            sum += varcharVecTest.get(i).length;
        }
        return sum;
    }

    /**
     * Get varchar vec benchmark
     *
     * @return sum value
     */
    @Benchmark
    public long getVarcharVecBenchmark() {
        long sum = 0L;
        for (int i = 0; i < rows; i++) {
            sum += vecGetData.get(i).length;
        }
        return sum;
    }

    /**
     * Get arrow vec benchmark
     *
     * @return sum value
     */
    @Benchmark
    public long getArrowVecBenchmark() {
        long sum = 0L;
        for (int i = 0; i < rows; i++) {
            sum += arrowVecGet.get(i).length;
        }
        return sum;
    }

    /**
     * Get varchar slice size
     *
     * @return slice size
     */
    @Benchmark
    public int sliceVarcharVecBenchmark() {
        VarcharVec slice = vecGetData.slice(2, rows / 2);
        return slice.getSize();
    }

    /**
     * Copy position varchar vec benchmark
     *
     * @return copy position size
     */
    @Benchmark
    public int copyPositionVarcharVecBenchmark() {
        VarcharVec copyPosition = vecGetData.copyPositions(positions, 0, positions.length);
        return copyPosition.getSize();
    }

    static class VarcharVecTest {
        int[] offsets;
        ByteBuffer byteBuffer;

        public VarcharVecTest(int capacityInBytes, int size) {
            offsets = new int[size + 1];
            byteBuffer = ByteBuffer.allocate(capacityInBytes);
        }

        /**
         * Get data
         *
         * @param index index
         * @return data
         */
        public byte[] get(int index) {
            int startOffset = offsets[index];
            int dataLen = offsets[index + 1] - offsets[index];
            byteBuffer.position(startOffset);
            byte[] data = new byte[dataLen];
            byteBuffer.get(data, 0, dataLen);
            return data;
        }

        /**
         * Set data
         *
         * @param index index
         * @param value data
         */
        public void set(int index, byte[] value) {
            int startOffset = offsets[index];
            offsets[index + 1] = startOffset + value.length;
            byteBuffer.position(startOffset);
            byteBuffer.put(value, 0, value.length);
        }

        /**
         * Get Data
         *
         * @return data
         */
        public byte[] getData() {
            return byteBuffer.array();
        }
    }

    private void closeArrowVec(List<VarCharVector> vecs) {
        for (VarCharVector vec : vecs) {
            vec.close();
        }
    }

    private void closeVec(List<VarcharVec> vecs) {
        for (Vec vec : vecs) {
            vec.close();
        }
    }

    public static void main(String[] args) throws Throwable {
        Options options = new OptionsBuilder().verbosity(VerboseMode.NORMAL)
                .include(".*" + BenchmarkVarcharVec.class.getSimpleName() + ".*").build();

        new Runner(options).run();
    }
}
