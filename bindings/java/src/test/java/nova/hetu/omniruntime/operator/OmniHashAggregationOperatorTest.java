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
import static nova.hetu.omniruntime.constants.FunctionType.OMNI_AGGREGATION_TYPE_COUNT_ALL;
import static nova.hetu.omniruntime.constants.FunctionType.OMNI_AGGREGATION_TYPE_COUNT_COLUMN;
import static nova.hetu.omniruntime.constants.FunctionType.OMNI_AGGREGATION_TYPE_SUM;
import static nova.hetu.omniruntime.util.TestUtils.assertVecBatchEquals;
import static nova.hetu.omniruntime.util.TestUtils.createVecBatch;
import static nova.hetu.omniruntime.util.TestUtils.freeVecBatch;
import static org.testng.Assert.assertEquals;
import static org.testng.Assert.assertNotEquals;
import static org.testng.Assert.assertNotNull;
import static org.testng.Assert.assertTrue;

import com.google.common.collect.ImmutableList;

import nova.hetu.omniruntime.constants.FunctionType;
import nova.hetu.omniruntime.operator.aggregator.OmniHashAggregationOperatorFactory;
import nova.hetu.omniruntime.operator.aggregator.OmniHashAggregationOperatorFactory.FactoryContext;
import nova.hetu.omniruntime.operator.config.OperatorConfig;
import nova.hetu.omniruntime.type.DataType;
import nova.hetu.omniruntime.type.LongDataType;
import nova.hetu.omniruntime.type.VarcharDataType;
import nova.hetu.omniruntime.vector.LongVec;
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
 * The type Omni hash aggregation operator test.
 *
 * @since 2021-07-21
 */
public class OmniHashAggregationOperatorTest {
    /**
     * test hashAggregation performance whether with jit or not.
     */
    @Test
    public void testHashAggregationComparePref() {
        String[] groupByChannel = {"#0", "#1"};
        DataType[] groupByTypes = {LongDataType.LONG, LongDataType.LONG};
        String[] aggChannels = {"#3"};
        DataType[] aggTypes = {LongDataType.LONG};
        FunctionType[] aggFunctionTypes = {OMNI_AGGREGATION_TYPE_COUNT_COLUMN, OMNI_AGGREGATION_TYPE_COUNT_ALL};
        DataType[] aggOutputTypes = {LongDataType.LONG, LongDataType.LONG};

        OmniHashAggregationOperatorFactory factoryWithJit = new OmniHashAggregationOperatorFactory(groupByChannel,
                groupByTypes, aggChannels, aggTypes, aggFunctionTypes, aggOutputTypes, true, false,
                new OperatorConfig());
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
        System.out.println("HashAggregation with jit use " + (end1 - start1) + " ms.");

        OmniHashAggregationOperatorFactory factoryWithoutJit = new OmniHashAggregationOperatorFactory(groupByChannel,
                groupByTypes, aggChannels, aggTypes, aggFunctionTypes, aggOutputTypes, true, false,
                new OperatorConfig());
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
        System.out.println("HashAggregation without jit use " + (end2 - start2) + " ms.");

        while (outputWithJit.hasNext() && outputWithoutJit.hasNext()) {
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
        String[] groupByChannel = {"#0", "#1"};
        DataType[] groupByTypes = {LongDataType.LONG, LongDataType.LONG};
        String[] aggChannels = {"#3"};
        DataType[] aggTypes = {LongDataType.LONG};
        FunctionType[] aggFunctionTypes = {OMNI_AGGREGATION_TYPE_COUNT_COLUMN, OMNI_AGGREGATION_TYPE_COUNT_ALL};
        DataType[] aggOutputTypes = {LongDataType.LONG, LongDataType.LONG};

        OmniHashAggregationOperatorFactory factory = new OmniHashAggregationOperatorFactory(groupByChannel,
                groupByTypes, aggChannels, aggTypes, aggFunctionTypes, aggOutputTypes, true, false);
        int rowNum = 100;
        int pageCount = 10;

        OmniOperator omniOperator = factory.createOperator();

        for (int i = 0; i < pageCount; i++) {
            VecBatch vecBatch = new VecBatch(buildDataForCount(rowNum));
            omniOperator.addInput(vecBatch);
        }

        Iterator<VecBatch> output = omniOperator.getOutput();
        VecBatch vecBatch = null;
        while (output.hasNext()) {
            vecBatch = output.next();
            if (vecBatch.getVectors().length != aggOutputTypes.length + groupByTypes.length) {
                throw new IllegalArgumentException(
                        format("output vec size error: result size: %s, outputTypes size: %s,rows: %s",
                                vecBatch.getVectors().length, aggOutputTypes.length, vecBatch.getRowCount()));
            }
            assertNotNull(vecBatch);
            assertEquals(vecBatch.getVectors().length, 4);
            Vec[] vectors = vecBatch.getVectors();
            assertEquals(((LongVec) vectors[0]).get(0), 1);
            assertEquals(((LongVec) vectors[1]).get(0), 1);
            assertEquals(((LongVec) vectors[2]).get(0), 500L);
            assertEquals(((LongVec) vectors[3]).get(0), 1000L);

            freeVecBatch(vecBatch);
        }

        omniOperator.close();
        factory.close();
    }

    @Test
    public void testExecuteAggMultiplePage() {
        String[] groupByChanel = {"#0", "#1"};
        DataType[] groupByTypes = {LongDataType.LONG, LongDataType.LONG};
        String[] aggChannels = {"#2", "#3"};
        DataType[] aggTypes = {LongDataType.LONG, LongDataType.LONG};
        FunctionType[] aggFunctionTypes = {OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_SUM};
        DataType[] aggOutputTypes = {LongDataType.LONG, LongDataType.LONG};

        DataType[] inputTypes = {LongDataType.LONG, LongDataType.LONG, LongDataType.LONG, LongDataType.LONG};
        OmniHashAggregationOperatorFactory factory = new OmniHashAggregationOperatorFactory(groupByChanel, groupByTypes,
                aggChannels, aggTypes, aggFunctionTypes, aggOutputTypes, true, false);
        int rowNum = 40000;
        int pageCount = 10;

        OmniOperator omniOperator = factory.createOperator();

        for (int i = 0; i < pageCount; i++) {
            VecBatch vecBatch = new VecBatch(build4Columns(rowNum));
            omniOperator.addInput(vecBatch);
        }

        Iterator<VecBatch> output = omniOperator.getOutput();
        VecBatch vecBatch = null;
        while (output.hasNext()) {
            vecBatch = output.next();
            if (vecBatch.getVectors().length != aggOutputTypes.length + groupByTypes.length) {
                throw new IllegalArgumentException(
                        format("output vec size error: result size: %s, outputTypes size: %s,rows: %s",
                                vecBatch.getVectors().length, aggOutputTypes.length, vecBatch.getRowCount()));
            }
            assertNotNull(vecBatch);
            assertEquals(vecBatch.getVectors().length, 4);
            Vec[] vectors = vecBatch.getVectors();
            assertEquals(((LongVec) vectors[0]).get(0), 1);
            assertEquals(((LongVec) vectors[1]).get(0), 1);
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
        int threadCount = 1;
        int rowNum = 100;
        multiThreadExecution(threadCount, rowNum, pageCount);
    }

    @Test
    public void testFactoryContextEquals() {
        String[] groupByChannel = {"#0", "#1"};
        DataType[] groupByTypes = {LongDataType.LONG, LongDataType.LONG};
        String[] aggChannels = {"#3"};
        DataType[] aggTypes = {LongDataType.LONG};
        FunctionType[] aggFunctionTypes = {OMNI_AGGREGATION_TYPE_COUNT_COLUMN, OMNI_AGGREGATION_TYPE_COUNT_ALL};
        DataType[] aggOutputTypes = {LongDataType.LONG, LongDataType.LONG};

        FactoryContext factory1 = new FactoryContext(groupByChannel, groupByTypes, aggChannels, aggTypes,
                aggFunctionTypes, aggOutputTypes, true, false, new OperatorConfig());
        FactoryContext factory2 = new FactoryContext(groupByChannel, groupByTypes, aggChannels, aggTypes,
                aggFunctionTypes, aggOutputTypes, true, false, new OperatorConfig());
        FactoryContext factory3 = null;

        assertEquals(factory2, factory1);
        assertNotEquals(factory3, factory1);
        assertEquals(factory1, factory1);
    }

    @Test
    public void testExecuteHashAggEmptyString() {
        String[] groupByChanel = {"#0"};
        int varcharWidth = 10;
        DataType[] groupByTypes = {new VarcharDataType(varcharWidth)};
        String[] aggChannels = {"#1"};
        DataType[] aggTypes = {LongDataType.LONG};
        FunctionType[] aggFunctionTypes = {OMNI_AGGREGATION_TYPE_SUM};
        DataType[] aggOutputTypes = {LongDataType.LONG};

        OmniHashAggregationOperatorFactory factory = new OmniHashAggregationOperatorFactory(groupByChanel, groupByTypes,
                aggChannels, aggTypes, aggFunctionTypes, aggOutputTypes, true, false);

        Object[][] inputData = {{"", null, "", null}, {1L, 2L, 3L, 4L}};
        DataType[] sourceTypes = {new VarcharDataType(varcharWidth), LongDataType.LONG};
        VecBatch vecBatch = createVecBatch(sourceTypes, inputData);

        OmniOperator omniOperator = factory.createOperator();
        omniOperator.addInput(vecBatch);

        Iterator<VecBatch> output = omniOperator.getOutput();
        VecBatch result = output.next();
        // adjust the output sequence in the vector.
        Object[][] expectedDatas = {{null, ""}, {6L, 4L}};
        assertVecBatchEquals(result, expectedDatas);

        freeVecBatch(result);
        omniOperator.close();
        factory.close();
    }

    private void multiThreadExecution(int threadCount, int rowNum, int pageCount) {
        String[] groupByChanel = {"#0", "#1"};
        DataType[] groupByTypes = {LongDataType.LONG, LongDataType.LONG};
        String[] aggChannels = {"#2", "#3"};
        DataType[] aggTypes = {LongDataType.LONG, LongDataType.LONG};
        FunctionType[] aggFunctionTypes = {OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_SUM};
        DataType[] aggOutputTypes = {LongDataType.LONG, LongDataType.LONG, LongDataType.LONG, LongDataType.LONG};
        OmniHashAggregationOperatorFactory factory = new OmniHashAggregationOperatorFactory(groupByChanel, groupByTypes,
                aggChannels, aggTypes, aggFunctionTypes, aggOutputTypes, true, false);

        CountDownLatch downLatch = new CountDownLatch(threadCount);
        final int corePoolSize = 10;
        final int maximumPoolSize = 50;
        ThreadPoolExecutor threadPool = new ThreadPoolExecutor(corePoolSize, maximumPoolSize, 60L, TimeUnit.SECONDS,
                new LinkedBlockingQueue<>(threadCount));

        for (int tIdx = 0; tIdx < threadCount; tIdx++) {
            CompletableFuture.runAsync(() -> {
                try {
                    OmniOperator omniOperator = factory.createOperator();
                    for (int i = 0; i < pageCount; i++) {
                        omniOperator.addInput(new VecBatch(build4Columns(rowNum)));
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
            assertTrue(false);
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
            assertEquals(((LongVec) vectors[0]).get(0), 1);
            assertEquals(((LongVec) vectors[1]).get(0), 1);
            assertEquals(((LongVec) vectors[2]).get(0), rowNum * pageCount);
            assertEquals(((LongVec) vectors[3]).get(0), rowNum * pageCount);
            freeVecBatch(vecBatch);
        }
    }

    private List<Vec> build4Columns(int rowNum) {
        LongVec c1 = new LongVec(rowNum);
        LongVec c2 = new LongVec(rowNum);
        for (int i = 0; i < rowNum; i++) {
            c1.set(i, 1);
            c2.set(i, 1);
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

    private List<Vec> build2Columns(int rowNum) {
        LongVec c1 = new LongVec(rowNum);
        for (int i = 0; i < rowNum; i++) {
            c1.set(i, 0);
        }

        LongVec c2 = new LongVec(rowNum);
        for (int i = 0; i < rowNum; i++) {
            c2.set(i, 1);
        }

        List<Vec> columns = new ArrayList<>();
        columns.add(c1);
        columns.add(c2);

        return columns;
    }

    private List<Vec> buildDataForCount(int rowNum) {
        LongVec c1 = new LongVec(rowNum);
        LongVec c2 = new LongVec(rowNum);
        for (int i = 0; i < rowNum; i++) {
            c1.set(i, 1);
            c2.set(i, 1);
        }

        LongVec c3 = new LongVec(rowNum);
        LongVec c4 = new LongVec(rowNum);
        for (int i = 0; i < rowNum; i++) {
            if (i % 2 == 0) {
                c3.set(i, 1);
                c4.set(i, 1);
            } else {
                c3.setNull(i);
                c4.setNull(i);
            }
        }

        List<Vec> columns = new ArrayList<>();
        columns.add(c1);
        columns.add(c2);
        columns.add(c3);
        columns.add(c4);

        return columns;
    }
}
