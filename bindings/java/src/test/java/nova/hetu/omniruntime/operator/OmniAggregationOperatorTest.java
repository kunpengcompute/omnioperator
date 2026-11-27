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

package nova.hetu.omniruntime.operator;

import static java.lang.String.format;
import static nova.hetu.omniruntime.constants.FunctionType.OMNI_AGGREGATION_TYPE_AVG;
import static nova.hetu.omniruntime.constants.FunctionType.OMNI_AGGREGATION_TYPE_COUNT_ALL;
import static nova.hetu.omniruntime.constants.FunctionType.OMNI_AGGREGATION_TYPE_COUNT_COLUMN;
import static nova.hetu.omniruntime.constants.FunctionType.OMNI_AGGREGATION_TYPE_MAX;
import static nova.hetu.omniruntime.constants.FunctionType.OMNI_AGGREGATION_TYPE_MIN;
import static nova.hetu.omniruntime.constants.FunctionType.OMNI_AGGREGATION_TYPE_SUM;
import static nova.hetu.omniruntime.util.TestUtils.assertVecBatchEquals;
import static nova.hetu.omniruntime.util.TestUtils.createVec;
import static nova.hetu.omniruntime.util.TestUtils.freeVecBatch;
import static org.testng.Assert.assertEquals;
import static org.testng.Assert.assertNotEquals;
import static org.testng.Assert.assertNotNull;
import static org.testng.Assert.fail;

import com.google.common.collect.ImmutableList;

import nova.hetu.omniruntime.constants.FunctionType;
import nova.hetu.omniruntime.operator.aggregator.OmniAggregationOperatorFactory;
import nova.hetu.omniruntime.operator.aggregator.OmniAggregationOperatorFactory.FactoryContext;
import nova.hetu.omniruntime.operator.config.OperatorConfig;
import nova.hetu.omniruntime.type.CharDataType;
import nova.hetu.omniruntime.type.DataType;
import nova.hetu.omniruntime.type.Decimal128DataType;
import nova.hetu.omniruntime.type.DoubleDataType;
import nova.hetu.omniruntime.type.IntDataType;
import nova.hetu.omniruntime.type.LongDataType;
import nova.hetu.omniruntime.type.TimestampDataType;
import nova.hetu.omniruntime.type.VarcharDataType;
import nova.hetu.omniruntime.vector.Decimal128Vec;
import nova.hetu.omniruntime.vector.DoubleVec;
import nova.hetu.omniruntime.vector.IntVec;
import nova.hetu.omniruntime.vector.LongVec;
import nova.hetu.omniruntime.vector.VarcharVec;
import nova.hetu.omniruntime.vector.Vec;
import nova.hetu.omniruntime.vector.VecBatch;

import org.testng.annotations.Test;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.TimeUnit;

/**
 * The type Omni aggregation operator test.
 *
 * @since 2021-06-09
 */
public class OmniAggregationOperatorTest {
    /**
     * test aggregation performance whether with jit or not.
     */
    @Test
    public void testAggregationComparePerf() {
        DataType[] sourceTypes = {LongDataType.LONG, LongDataType.LONG};
        FunctionType[] aggFunctionTypes = {OMNI_AGGREGATION_TYPE_COUNT_COLUMN, OMNI_AGGREGATION_TYPE_COUNT_ALL,
                OMNI_AGGREGATION_TYPE_COUNT_COLUMN};
        int[] aggInputChannels = {0, 1};
        int[] maskChannels = {-1, -1, -1};
        DataType[] aggOutputTypes = {LongDataType.LONG, LongDataType.LONG, LongDataType.LONG};
        OmniAggregationOperatorFactory factoryWithJit = new OmniAggregationOperatorFactory(sourceTypes,
                aggFunctionTypes, aggInputChannels, maskChannels, aggOutputTypes, true, false, new OperatorConfig());
        OmniOperator omniOperatorWithJit = factoryWithJit.createOperator();

        ImmutableList.Builder<VecBatch> vecBatchList1 = ImmutableList.builder();
        int rowNum = 100000;
        int pageCount = 10;
        for (int i = 0; i < pageCount; i++) {
            vecBatchList1.add(new VecBatch(buildDataForCount(rowNum)));
        }

        long start1 = System.currentTimeMillis();
        for (VecBatch vecBatch : vecBatchList1.build()) {
            omniOperatorWithJit.addInput(vecBatch);
        }

        Iterator<VecBatch> outputWithJit = omniOperatorWithJit.getOutput();
        long end1 = System.currentTimeMillis();
        System.out.println("Aggregation with jit use " + (end1 - start1) + " ms.");

        OmniAggregationOperatorFactory factoryWithoutJit = new OmniAggregationOperatorFactory(sourceTypes,
                aggFunctionTypes, aggInputChannels, maskChannels, aggOutputTypes, true, false, new OperatorConfig());
        OmniOperator omniOperatorWithoutJit = factoryWithoutJit.createOperator();

        ImmutableList.Builder<VecBatch> vecBatchList2 = ImmutableList.builder();
        for (int i = 0; i < pageCount; i++) {
            vecBatchList2.add(new VecBatch(buildDataForCount(rowNum)));
        }

        long start2 = System.currentTimeMillis();
        for (VecBatch vecBatch : vecBatchList2.build()) {
            omniOperatorWithoutJit.addInput(vecBatch);
        }

        Iterator<VecBatch> outputWithoutJit = omniOperatorWithoutJit.getOutput();
        long end2 = System.currentTimeMillis();
        System.out.println("Aggregation without jit use " + (end2 - start2) + " ms.");

        while (outputWithJit.hasNext()) {
            VecBatch resultWithJit = outputWithJit.next();
            VecBatch resultWithoutJit = outputWithoutJit.next();
            assertVecBatchEquals(resultWithJit, resultWithoutJit);
            freeVecBatch(resultWithJit);
            freeVecBatch(resultWithoutJit);
        }

        omniOperatorWithJit.close();
        omniOperatorWithoutJit.close();
        factoryWithJit.close();
        factoryWithoutJit.close();
    }

    /**
     * Test execute agg multiple page.
     */
    @Test
    public void testExecuteCountMultiplePage() {
        DataType[] sourceTypes = {LongDataType.LONG, LongDataType.LONG};
        FunctionType[] aggFunctionTypes = {OMNI_AGGREGATION_TYPE_COUNT_COLUMN, OMNI_AGGREGATION_TYPE_COUNT_ALL,
                OMNI_AGGREGATION_TYPE_COUNT_COLUMN};
        int[] aggInputChannels = {0, 1};
        int[] maskChannels = {-1, -1, -1};
        DataType[] aggOutputTypes = {LongDataType.LONG, LongDataType.LONG, LongDataType.LONG};
        OmniAggregationOperatorFactory factory = new OmniAggregationOperatorFactory(sourceTypes, aggFunctionTypes,
                aggInputChannels, maskChannels, aggOutputTypes, true, false);

        ImmutableList.Builder<VecBatch> vecBatchList = ImmutableList.builder();
        int rowNum = 100;
        int pageCount = 10;
        for (int i = 0; i < pageCount; i++) {
            vecBatchList.add(new VecBatch(buildDataForCount(rowNum)));
        }

        OmniOperator omniOperator = factory.createOperator();
        for (VecBatch vecBatch : vecBatchList.build()) {
            omniOperator.addInput(vecBatch);
        }

        Iterator<VecBatch> output = omniOperator.getOutput();
        while (output.hasNext()) {
            VecBatch vecBatch = output.next();
            if (vecBatch.getVectors().length != aggOutputTypes.length) {
                throw new IllegalArgumentException(
                        format("output vec size error: result size: %s, outputTypes size: %s,rows: %s",
                                vecBatch.getVectors().length, aggOutputTypes.length, vecBatch.getRowCount()));
            }

            assertNotNull(vecBatch);
            assertEquals(vecBatch.getVectors().length, 3);
            Vec[] vectors = vecBatch.getVectors();
            assertEquals(((LongVec) vectors[0]).get(0), 1000L);
            assertEquals(((LongVec) vectors[1]).get(0), 1000L);
            assertEquals(((LongVec) vectors[2]).get(0), 500L);

            freeVecBatch(vecBatch);
        }

        omniOperator.close();
        factory.close();
    }

    @Test
    public void testFactoryContextEquals() {
        DataType[] sourceTypes = {LongDataType.LONG, LongDataType.LONG, LongDataType.LONG, LongDataType.LONG};
        FunctionType[] aggFunctionTypes = {OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_SUM,
                OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_SUM};
        int[] aggInputChannels = {0, 1, 2, 3};
        int[] maskChannels = {-1, -1, -1, -1};
        DataType[] aggOutputTypes = {LongDataType.LONG, LongDataType.LONG, LongDataType.LONG, LongDataType.LONG};
        FactoryContext factory1 = new FactoryContext(sourceTypes, aggFunctionTypes, aggInputChannels, maskChannels,
                aggOutputTypes, true, false, new OperatorConfig());
        FactoryContext factory2 = new FactoryContext(sourceTypes, aggFunctionTypes, aggInputChannels, maskChannels,
                aggOutputTypes, true, false, new OperatorConfig());
        FactoryContext factory3 = null;

        assertEquals(factory2, factory1);
        assertEquals(factory1, factory1);
        assertNotEquals(factory3, factory1);
    }

    @Test
    public void testExecuteMinMax() {
        DataType[] sourceTypes = {IntDataType.INTEGER, LongDataType.LONG, DoubleDataType.DOUBLE, new CharDataType(20),
                new VarcharDataType(20), new Decimal128DataType(20, 5), TimestampDataType.TIMESTAMP};
        FunctionType minFn = OMNI_AGGREGATION_TYPE_MIN;
        FunctionType maxFn = OMNI_AGGREGATION_TYPE_MAX;
        FunctionType[] aggFunctionTypes = {minFn, minFn, minFn, minFn, minFn, minFn, minFn, maxFn, maxFn, maxFn, maxFn,
                maxFn, maxFn, maxFn};
        int[] aggInputChannels = {0, 1, 2, 3, 4, 5, 6, 0, 1, 2, 3, 4, 5, 6};
        int[] maskChannels = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
        DataType[] aggOutputTypes = {IntDataType.INTEGER, LongDataType.LONG, DoubleDataType.DOUBLE,
                new CharDataType(20), new VarcharDataType(20), new Decimal128DataType(20, 5),
                TimestampDataType.TIMESTAMP, IntDataType.INTEGER, LongDataType.LONG, DoubleDataType.DOUBLE,
                new CharDataType(20), new VarcharDataType(20), new Decimal128DataType(20, 5),
                TimestampDataType.TIMESTAMP};
        OmniAggregationOperatorFactory factory = new OmniAggregationOperatorFactory(sourceTypes, aggFunctionTypes,
                aggInputChannels, maskChannels, aggOutputTypes, true, false);

        Object[][] sampleDatas = {{2, 1, 5, 3, 1}, {3L, 10L, 2L, 7L, 3L}, {12.3, 7.2, 20.5, 6.1, 12.3},
                {"hello", "world", "c++", "shell", "golang"}, {"operator", "vectorBatch", "udf", "expression", "omni"}};
        Object[][] decimalDatas = {{4000L, 0L}, {2000L, 0L}, {1000L, 0L}, {2000L, 0L}, {5000L, 0L}};
        Object[] timestampDatas = {3000L, 1000L, 5000L, 2000L, 4000L};

        Vec[] buildVecs = new Vec[sourceTypes.length];
        for (int i = 0; i < 5; i++) {
            buildVecs[i] = createVec(sourceTypes[i], sampleDatas[i]);
        }
        buildVecs[5] = createVec(sourceTypes[5], decimalDatas);
        buildVecs[6] = createVec(sourceTypes[6], timestampDatas);

        VecBatch inputData = new VecBatch(buildVecs);
        OmniOperator omniOperator = factory.createOperator();
        omniOperator.addInput(inputData);
        Iterator<VecBatch> output = omniOperator.getOutput();

        while (output.hasNext()) {
            VecBatch vecBatch = output.next();
            if (vecBatch.getVectors().length != aggOutputTypes.length) {
                throw new IllegalArgumentException(
                        format("output vec size error: result size: %s, outputTypes size: %s,rows: %s",
                                vecBatch.getVectors().length, aggOutputTypes.length, vecBatch.getRowCount()));
            }

            assertNotNull(vecBatch);
            assertEquals(vecBatch.getVectors().length, 14);
            Vec[] vectors = vecBatch.getVectors();
            assertEquals(((IntVec) vectors[0]).get(0), 1);
            assertEquals(((LongVec) vectors[1]).get(0), 2L);
            assertEquals(((DoubleVec) vectors[2]).get(0), 6.1);
            assertEquals(new String(((VarcharVec) vectors[3]).get(0)), "c++");
            assertEquals(new String(((VarcharVec) vectors[4]).get(0)), "expression");
            assertEquals(((Decimal128Vec) vectors[5]).get(0), new Object[]{1000L, 0L});
            assertEquals(((LongVec) vectors[6]).get(0), 1000L);
            assertEquals(((IntVec) vectors[7]).get(0), 5);
            assertEquals(((LongVec) vectors[8]).get(0), 10L);
            assertEquals(((DoubleVec) vectors[9]).get(0), 20.5);
            assertEquals(new String(((VarcharVec) vectors[10]).get(0)), "world");
            assertEquals(new String(((VarcharVec) vectors[11]).get(0)), "vectorBatch");
            assertEquals(((Decimal128Vec) vectors[12]).get(0), new Object[]{5000L, 0L});
            assertEquals(((LongVec) vectors[13]).get(0), 5000L);

            freeVecBatch(vecBatch);
        }
        omniOperator.close();
        factory.close();
    }

    @Test
    public void testExecuteSumAvgMultipleStage() {
        DataType[] sourceTypes = {LongDataType.LONG, new Decimal128DataType(20, 5)};
        FunctionType sumFn = OMNI_AGGREGATION_TYPE_SUM;
        FunctionType avgFn = OMNI_AGGREGATION_TYPE_AVG;
        FunctionType[] aggFunctionTypes = {sumFn, avgFn};
        int[] aggInputChannels = {0, 1};
        int[] maskChannels = {-1, -1};

        Object[][] sampleDatas = {{3L, 10L, 2L, 7L, 3L}};
        Object[][] decimalDatas = {{4000L, 0L}, {2000L, 0L}, {1000L, 0L}, {3000L, 0L}, {5000L, 0L}};

        Vec[] buildVecs = new Vec[sourceTypes.length];
        for (int i = 0; i < 1; i++) {
            buildVecs[i] = createVec(sourceTypes[i], sampleDatas[i]);
        }
        buildVecs[1] = createVec(sourceTypes[1], decimalDatas);
        VecBatch inputData = new VecBatch(buildVecs);

        DataType[] finalAggOutputTypes = {LongDataType.LONG, new Decimal128DataType(20, 5)};
        OmniAggregationOperatorFactory factory = new OmniAggregationOperatorFactory(sourceTypes,
                aggFunctionTypes, aggInputChannels, maskChannels, finalAggOutputTypes, true, false);

        OmniOperator omniOperator = factory.createOperator();
        omniOperator.addInput(inputData);

        Iterator<VecBatch> finalOutput = omniOperator.getOutput();
        while (finalOutput.hasNext()) {
            VecBatch finalVecBatch = finalOutput.next();
            if (finalVecBatch.getVectors().length != finalAggOutputTypes.length) {
                throw new IllegalArgumentException(format(
                        "output vec size error: result size: %s, outputTypes size: %s,rows: %s",
                        finalVecBatch.getVectors().length, finalAggOutputTypes.length, finalVecBatch.getRowCount()));
            }

            assertNotNull(finalVecBatch);
            assertEquals(finalVecBatch.getVectors().length, 2);
            Vec[] vectors = finalVecBatch.getVectors();
            assertEquals(((LongVec) vectors[0]).get(0), 25L);
            assertEquals(((Decimal128Vec) vectors[1]).get(0), new Object[]{3000L, 0L});
            freeVecBatch(finalVecBatch);
        }
        omniOperator.close();
        factory.close();
    }

    @Test
    public void testExecuteAggMultiplePage() {
        DataType[] sourceTypes = {LongDataType.LONG, LongDataType.LONG, LongDataType.LONG, LongDataType.LONG};
        FunctionType[] aggFunctionTypes = {OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_SUM,
                OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_SUM};
        int[] aggInputChannels = {0, 1, 2, 3};
        int[] maskChannels = {-1, -1, -1, -1};
        DataType[] aggOutputTypes = {LongDataType.LONG, LongDataType.LONG, LongDataType.LONG, LongDataType.LONG};
        OmniAggregationOperatorFactory factory = new OmniAggregationOperatorFactory(sourceTypes, aggFunctionTypes,
                aggInputChannels, maskChannels, aggOutputTypes, true, false);

        List<Vec> inputData = new ArrayList<>();
        ImmutableList.Builder<VecBatch> vecBatchList = ImmutableList.builder();
        int rowNum = 40000;
        int pageCount = 10;
        for (int i = 0; i < pageCount; i++) {
            inputData.addAll(build4Columns(rowNum));
            vecBatchList.add(new VecBatch(build4Columns(rowNum)));
        }

        OmniOperator omniOperator = factory.createOperator();
        for (VecBatch vecBatch : vecBatchList.build()) {
            omniOperator.addInput(vecBatch);
        }
        // release input data memory
        releaseVecMemory(inputData.toArray(new Vec[0]));

        Iterator<VecBatch> output = omniOperator.getOutput();
        while (output.hasNext()) {
            VecBatch vecBatch = output.next();
            if (vecBatch.getVectors().length != aggOutputTypes.length) {
                throw new IllegalArgumentException(
                        format("output vec size error: result size: %s, outputTypes size: %s,rows: %s",
                                vecBatch.getVectors().length, aggOutputTypes.length, vecBatch.getRowCount()));
            }

            assertNotNull(vecBatch);
            assertEquals(vecBatch.getVectors().length, 4);
            Vec[] vectors = vecBatch.getVectors();
            assertEquals(((LongVec) vectors[0]).get(0), 0);
            assertEquals(((LongVec) vectors[1]).get(0), 0);
            assertEquals(((LongVec) vectors[2]).get(0), rowNum * pageCount);
            assertEquals(((LongVec) vectors[3]).get(0), rowNum * pageCount);

            freeVecBatch(vecBatch);
        }
        omniOperator.close();
        factory.close();
    }

    /**
     * Test execute agg multiple thread.
     */
    @Test
    public void testExecuteAggMultipleThread() {
        int pageCount = 10;
        int threadCount = 10;
        int rowNum = 100;
        multiThreadExecution(threadCount, rowNum, pageCount);
    }

    private void multiThreadExecution(int threadCount, int rowNum, int pageCount) {
        DataType[] sourceTypes = {LongDataType.LONG, LongDataType.LONG, LongDataType.LONG, LongDataType.LONG};
        FunctionType[] aggFunctionTypes = {OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_SUM,
                OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_SUM};
        int[] aggInputChannels = {0, 1, 2, 3};
        int[] maskChannels = {-1, -1, -1, -1};
        DataType[] aggOutputTypes = {LongDataType.LONG, LongDataType.LONG, LongDataType.LONG, LongDataType.LONG};
        OmniAggregationOperatorFactory factory = new OmniAggregationOperatorFactory(sourceTypes, aggFunctionTypes,
                aggInputChannels, maskChannels, aggOutputTypes, true, false);

        CountDownLatch downLatch = new CountDownLatch(threadCount);
        final int corePoolSize = 10;
        final int maximumPoolSize = 50;
        ThreadPoolExecutor threadPool = new ThreadPoolExecutor(corePoolSize, maximumPoolSize, 60L, TimeUnit.SECONDS,
                new LinkedBlockingQueue<>(threadCount));

        for (int tIdx = 0; tIdx < threadCount; tIdx++) {
            CompletableFuture.runAsync(() -> {
                try {
                    ImmutableList.Builder<VecBatch> vecBatchList = ImmutableList.builder();
                    for (int i = 0; i < pageCount; i++) {
                        vecBatchList.add(new VecBatch(build4Columns(rowNum)));
                    }

                    OmniOperator omniOperator = factory.createOperator();
                    for (VecBatch vecBatch : vecBatchList.build()) {
                        omniOperator.addInput(vecBatch);
                    }

                    assertResult(rowNum, pageCount, aggOutputTypes, omniOperator);
                    omniOperator.close();
                } finally {
                    downLatch.countDown();
                }
            }, threadPool);
        }

        try {
            downLatch.await();
        } catch (InterruptedException ex) {
            fail();
        }

        threadPool.shutdown();
        factory.close();
    }

    private void assertResult(int rowNum, int pageCount, DataType[] aggOutputTypes, OmniOperator omniOperator) {
        Iterator<VecBatch> output = omniOperator.getOutput();
        while (output.hasNext()) {
            VecBatch vecBatch = output.next();
            if (vecBatch.getVectors().length != aggOutputTypes.length) {
                throw new IllegalArgumentException(
                        format("output vec size error: result size: %s, outputTypes size: %s,rows: %s",
                                vecBatch.getVectors().length, aggOutputTypes.length, vecBatch.getRowCount()));
            }

            assertNotNull(vecBatch);
            assertEquals(vecBatch.getVectors().length, 4);
            Vec[] vectors = vecBatch.getVectors();
            assertEquals(((LongVec) vectors[0]).get(0), 0);
            assertEquals(((LongVec) vectors[1]).get(0), 0);
            assertEquals(((LongVec) vectors[2]).get(0), rowNum * pageCount);
            assertEquals(((LongVec) vectors[3]).get(0), rowNum * pageCount);

            freeVecBatch(vecBatch);
        }
    }

    private List<Vec> buildDataForCount(int rowNum) {
        LongVec c1 = new LongVec(rowNum);
        for (int i = 0; i < rowNum; i++) {
            c1.set(i, 0);
        }

        LongVec c2 = new LongVec(rowNum);
        for (int i = 0; i < rowNum; i++) {
            if (i % 2 == 0) {
                c2.set(i, 1);
            } else {
                c2.setNull(i);
            }
        }

        List<Vec> columns = new ArrayList<>();
        columns.add(c1);
        columns.add(c2);

        return columns;
    }

    private List<Vec> build4Columns(int rowNum) {
        LongVec c1 = new LongVec(rowNum);
        LongVec c2 = new LongVec(rowNum);
        for (int i = 0; i < rowNum; i++) {
            c1.set(i, 0);
            c2.set(i, 0);
        }

        LongVec c3 = new LongVec(rowNum);
        LongVec c4 = new LongVec(rowNum);
        for (int i = 0; i < rowNum; i++) {
            c3.set(i, 1);
            c4.set(i, 1);
        }

        List<Vec> columns = new ArrayList<>();
        columns.add(c1);
        columns.add(c2);
        columns.add(c3);
        columns.add(c4);

        return columns;
    }

    private void releaseVecMemory(Vec[] vecs) {
        for (Vec vec : vecs) {
            vec.close();
        }
    }
}
