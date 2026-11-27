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

import static nova.hetu.omniruntime.constants.FunctionType.OMNI_AGGREGATION_TYPE_AVG;
import static nova.hetu.omniruntime.constants.FunctionType.OMNI_AGGREGATION_TYPE_COUNT_ALL;
import static nova.hetu.omniruntime.constants.FunctionType.OMNI_AGGREGATION_TYPE_COUNT_COLUMN;
import static nova.hetu.omniruntime.constants.FunctionType.OMNI_AGGREGATION_TYPE_SUM;
import static nova.hetu.omniruntime.util.TestUtils.assertVecBatchEquals;
import static nova.hetu.omniruntime.util.TestUtils.createVecBatch;
import static nova.hetu.omniruntime.util.TestUtils.freeVecBatch;
import static nova.hetu.omniruntime.util.TestUtils.getOmniJsonFieldReference;
import static nova.hetu.omniruntime.util.TestUtils.getOmniJsonLiteral;
import static nova.hetu.omniruntime.util.TestUtils.omniFunctionExpr;
import static nova.hetu.omniruntime.util.TestUtils.omniJsonFourArithmeticExpr;
import static org.testng.Assert.assertEquals;
import static org.testng.Assert.assertNotEquals;

import com.google.common.collect.ImmutableList;

import nova.hetu.omniruntime.constants.FunctionType;
import nova.hetu.omniruntime.operator.aggregator.OmniHashAggregationOperatorFactory;
import nova.hetu.omniruntime.operator.aggregator.OmniHashAggregationWithExprOperatorFactory;
import nova.hetu.omniruntime.operator.aggregator.OmniHashAggregationWithExprOperatorFactory.FactoryContext;
import nova.hetu.omniruntime.operator.config.OperatorConfig;
import nova.hetu.omniruntime.type.DataType;
import nova.hetu.omniruntime.type.DoubleDataType;
import nova.hetu.omniruntime.type.IntDataType;
import nova.hetu.omniruntime.type.LongDataType;
import nova.hetu.omniruntime.utils.OmniRuntimeException;
import nova.hetu.omniruntime.vector.LongVec;
import nova.hetu.omniruntime.vector.Vec;
import nova.hetu.omniruntime.vector.VecBatch;

import org.testng.annotations.Test;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

/**
 * The type Omni hash aggregation with expression operator test.
 *
 * @since 2021-11-11
 */
public class OmniHashAggregationWithExprOperatorTest {
    /**
     * test hashAggregationWithExpr performance whether with jit or not.
     */
    @Test
    public void testHashAggregationOutputlMultiVectorBatch() {
        String[] groupByChannel = {"#0", "#1"};
        DataType[] groupByTypes = {LongDataType.LONG, LongDataType.LONG};
        String[] aggChannels = {"#3"};
        DataType[] aggTypes = {LongDataType.LONG};
        FunctionType[] aggFunctionTypes = {OMNI_AGGREGATION_TYPE_COUNT_COLUMN, OMNI_AGGREGATION_TYPE_COUNT_ALL};
        DataType[] aggOutputTypes = {LongDataType.LONG, LongDataType.LONG};

        OmniHashAggregationOperatorFactory operatorFactory = new OmniHashAggregationOperatorFactory(groupByChannel,
                groupByTypes, aggChannels, aggTypes, aggFunctionTypes, aggOutputTypes, true, false,
                new OperatorConfig());
        OmniOperator omniOperator = operatorFactory.createOperator();

        ImmutableList.Builder<VecBatch> vecBatchList1 = ImmutableList.builder();
        int rowNum = 100000;
        int pageCount = 10;
        for (int i = 0; i < pageCount; i++) {
            vecBatchList1.add(new VecBatch(buildDataForOutputMultiVectorBatch(rowNum)));
        }

        for (VecBatch vecBatch : vecBatchList1.build()) {
            omniOperator.addInput(vecBatch);
        }

        Iterator<VecBatch> outputVecBatch = omniOperator.getOutput();

        int vecBatchCount = 0;
        int totalRowcount = 0;
        long col1Sum = 0L;
        long col2Sum = 0L;
        long col3Sum = 0L;
        long col4Sum = 0L;
        while (outputVecBatch.hasNext()) {
            VecBatch result = outputVecBatch.next();
            Vec[] vectors = result.getVectors();
            int vecBatchRowCurrent = result.getRowCount();
            for (int i = 0; i < vecBatchRowCurrent; ++i) {
                col1Sum += ((LongVec) vectors[0]).get(i);
                col2Sum += ((LongVec) vectors[1]).get(i);
                col3Sum += ((LongVec) vectors[2]).get(i);
                col4Sum += ((LongVec) vectors[3]).get(i);
            }
            totalRowcount += vecBatchRowCurrent;
            freeVecBatch(result);
            vecBatchCount++;
        }
        omniOperator.close();
        operatorFactory.close();
        assertEquals(totalRowcount, rowNum);
        // each row contains four columns, each of which contains 8 bytes.
        int rowSize = 4 * 8;
        // single vecBatch is 1MB, calculate the maximum number of rows in single
        // vecBatch.
        int rowsPerBatch = (1024 * 1024 + rowSize - 1) / rowSize;
        int expectedBatchCount = (rowNum + rowsPerBatch - 1) / rowsPerBatch;
        assertEquals(vecBatchCount, expectedBatchCount);
        // sum of an arithmetic sequence with a step of 1
        assertEquals(col1Sum, (((long) rowNum - 1) * rowNum) / 2);
        assertEquals(col2Sum, (long) rowNum);
        assertEquals(col3Sum, (long) (rowNum / 2 * pageCount));
        assertEquals(col4Sum, (long) (rowNum * pageCount));
    }

    @Test
    public void testHashAggregationWithExprComparePref() {
        String[] groupByChannel = {getOmniJsonFieldReference(2, 0), getOmniJsonFieldReference(2, 1)};
        String[][] aggChannels = {{getOmniJsonFieldReference(2, 3)}};
        DataType[] sourceTypes = {LongDataType.LONG, LongDataType.LONG, LongDataType.LONG, LongDataType.LONG};
        FunctionType[] aggFunctionTypes = {OMNI_AGGREGATION_TYPE_COUNT_ALL, OMNI_AGGREGATION_TYPE_COUNT_COLUMN};
        DataType[][] aggOutputTypes = {{LongDataType.LONG}, {LongDataType.LONG}};
        String[] aggChannelsfilter = {null, null};
        OmniHashAggregationWithExprOperatorFactory factoryWithJit = new OmniHashAggregationWithExprOperatorFactory(
                groupByChannel, aggChannels, aggChannelsfilter, sourceTypes, aggFunctionTypes, aggOutputTypes,
                new boolean[]{true, true}, new boolean[]{false, false}, new OperatorConfig());
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
        System.out.println("HashAggregationWithExpr with jit use " + (end1 - start1) + " ms.");

        OmniHashAggregationWithExprOperatorFactory factoryWithoutJit = new OmniHashAggregationWithExprOperatorFactory(
                groupByChannel, aggChannels, aggChannelsfilter, sourceTypes, aggFunctionTypes, aggOutputTypes,
                new boolean[]{true, true}, new boolean[]{false, false}, new OperatorConfig());
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
        System.out.println("HashAggregationWithExpr without jit use " + (end2 - start2) + " ms.");

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

    @Test
    public void testHashAggWithPartialExpr() {
        String[] groupByChanel = {omniJsonFourArithmeticExpr("MODULUS", 2, getOmniJsonFieldReference(2, 0),
                getOmniJsonLiteral(2, false, 3)), getOmniJsonFieldReference(1, 2)};
        String[][] aggChannels = {{omniJsonFourArithmeticExpr("MULTIPLY", 2, getOmniJsonFieldReference(2, 1),
                getOmniJsonLiteral(2, false, 5))}, {getOmniJsonFieldReference(1, 3)}};

        FunctionType[] aggFunctionTypes = {OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_AVG};
        DataType[][] aggOutputTypes = {{LongDataType.LONG}, {DoubleDataType.DOUBLE}};

        DataType[] sourceTypes = {LongDataType.LONG, LongDataType.LONG, IntDataType.INTEGER, IntDataType.INTEGER};
        String[] aggChannelsfilter = {null, null};
        OmniHashAggregationWithExprOperatorFactory factory = new OmniHashAggregationWithExprOperatorFactory(
                groupByChanel, aggChannels, aggChannelsfilter, sourceTypes, aggFunctionTypes, aggOutputTypes,
                new boolean[]{true, true}, new boolean[]{false, false});

        OmniOperator omniOperator = factory.createOperator();

        Object[][] sourceDatas = {{2L, 5L, 8L, 11L, 14L, 17L, 20L, 23L}, {5L, 3L, 2L, 6L, 1L, 4L, 7L, 8L},
                {5, 5, 5, 5, 5, 5, 5, 5}, {5, 3, 2, 6, 1, 4, 7, 8}};
        VecBatch vecBatch = createVecBatch(sourceTypes, sourceDatas);
        omniOperator.addInput(vecBatch);

        Iterator<VecBatch> results = omniOperator.getOutput();

        assertEquals(results.hasNext(), true);
        VecBatch resultVecBatch = results.next();
        assertEquals(results.hasNext(), false);

        // should return false when multiple invoke hasNext()
        assertEquals(results.hasNext(), false);
        assertEquals(resultVecBatch.getRowCount(), 1);
        assertEquals(resultVecBatch.getVectorCount(), 4);

        Object[][] expectedDatas = {{2L}, {5}, {180L}, {4.5}};
        assertVecBatchEquals(resultVecBatch, expectedDatas);

        freeVecBatch(resultVecBatch);
        omniOperator.close();
        factory.close();
    }

    @Test
    public void testHashAggWithAllExpr() {
        String[] groupByChanel = {
                omniJsonFourArithmeticExpr("MODULUS", 2, getOmniJsonFieldReference(2, 0),
                        getOmniJsonLiteral(2, false, 3)),
                omniJsonFourArithmeticExpr("ADD", 1, getOmniJsonFieldReference(1, 2), getOmniJsonLiteral(1, false, 5))};
        String[][] aggChannels = {
                {omniJsonFourArithmeticExpr("MULTIPLY", 2, getOmniJsonFieldReference(2, 1),
                        getOmniJsonLiteral(2, false, 5))},
                {omniJsonFourArithmeticExpr("ADD", 1, getOmniJsonFieldReference(1, 3),
                        getOmniJsonLiteral(1, false, 5))}};

        FunctionType[] aggFunctionTypes = {OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_AVG};
        DataType[][] aggOutputTypes = {{LongDataType.LONG}, {DoubleDataType.DOUBLE}};

        DataType[] sourceTypes = {LongDataType.LONG, LongDataType.LONG, IntDataType.INTEGER, IntDataType.INTEGER};
        String[] aggChannelsfilter = {null, null};
        OmniHashAggregationWithExprOperatorFactory factory = new OmniHashAggregationWithExprOperatorFactory(
                groupByChanel, aggChannels, aggChannelsfilter, sourceTypes, aggFunctionTypes, aggOutputTypes,
                new boolean[]{true, true}, new boolean[]{false, false});

        OmniOperator omniOperator = factory.createOperator();

        Object[][] sourceDatas = {{2L, 5L, 8L, 11L, 14L, 17L, 20L, 23L}, {5L, 3L, 2L, 6L, 1L, 4L, 7L, 8L},
                {5, 5, 5, 5, 5, 5, 5, 5}, {5, 3, 2, 6, 1, 4, 7, 8}};
        VecBatch vecBatch = createVecBatch(sourceTypes, sourceDatas);
        omniOperator.addInput(vecBatch);

        Iterator<VecBatch> results = omniOperator.getOutput();

        assertEquals(results.hasNext(), true);
        VecBatch resultVecBatch = results.next();
        assertEquals(results.hasNext(), false);
        assertEquals(resultVecBatch.getRowCount(), 1);
        assertEquals(resultVecBatch.getVectorCount(), 4);

        Object[][] expectedDatas = {{2L}, {10}, {180L}, {9.5}};
        assertVecBatchEquals(resultVecBatch, expectedDatas);

        freeVecBatch(resultVecBatch);
        omniOperator.close();
        factory.close();
    }

    @Test
    public void testHashAggWithNoExpr() {
        String[] groupByChanel = {getOmniJsonFieldReference(2, 0), getOmniJsonFieldReference(1, 2)};
        String[][] aggChannels = {{getOmniJsonFieldReference(2, 1)}, {getOmniJsonFieldReference(1, 3)}};

        FunctionType[] aggFunctionTypes = {OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_AVG,
                OMNI_AGGREGATION_TYPE_COUNT_ALL};
        DataType[][] aggOutputTypes = {{LongDataType.LONG}, {DoubleDataType.DOUBLE}, {LongDataType.LONG}};

        DataType[] sourceTypes = {LongDataType.LONG, LongDataType.LONG, IntDataType.INTEGER, IntDataType.INTEGER};
        String[] aggChannelsfilter = {null, null, null};
        OmniHashAggregationWithExprOperatorFactory factory = new OmniHashAggregationWithExprOperatorFactory(
                groupByChanel, aggChannels, aggChannelsfilter, sourceTypes, aggFunctionTypes, aggOutputTypes,
                new boolean[]{true, true, true}, new boolean[]{false, false, false});

        OmniOperator omniOperator = factory.createOperator();

        Object[][] sourceDatas = {{2L, 2L, 2L, 2L, 2L, 2L, 2L, 2L}, {5L, 3L, 2L, 6L, 1L, 4L, 7L, 8L},
                {5, 5, 5, 5, 5, 5, 5, 5}, {5, 3, 2, 6, 1, 4, 7, 8}};
        VecBatch vecBatch = createVecBatch(sourceTypes, sourceDatas);
        omniOperator.addInput(vecBatch);

        Iterator<VecBatch> results = omniOperator.getOutput();

        assertEquals(results.hasNext(), true);
        VecBatch resultVecBatch = results.next();
        assertEquals(results.hasNext(), false);
        assertEquals(resultVecBatch.getRowCount(), 1);
        assertEquals(resultVecBatch.getVectorCount(), 5);

        Object[][] expectedDatas = {{2L}, {5}, {36L}, {4.5}, {8L}};
        assertVecBatchEquals(resultVecBatch, expectedDatas);

        freeVecBatch(resultVecBatch);
        omniOperator.close();
        factory.close();
    }

    @Test(expectedExceptions = OmniRuntimeException.class, expectedExceptionsMessageRegExp = ".*EXPRESSION_NOT_SUPPORT.*")
    public void testHashAggWithInvalidGroupByKeys() {
        String[] groupByChanel = {omniFunctionExpr("abc", 2, getOmniJsonFieldReference(2, 0)),
                getOmniJsonFieldReference(1, 2)};
        String[][] aggChannels = {{getOmniJsonFieldReference(2, 1)}, {getOmniJsonFieldReference(1, 3)}};
        FunctionType[] aggFunctionTypes = {OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_AVG};
        DataType[][] aggOutputTypes = {{LongDataType.LONG}, {DoubleDataType.DOUBLE}};
        DataType[] sourceTypes = {LongDataType.LONG, LongDataType.LONG, IntDataType.INTEGER, IntDataType.INTEGER};
        String[] aggChannelsfilter = {null, null};
        OmniHashAggregationWithExprOperatorFactory factory = new OmniHashAggregationWithExprOperatorFactory(
                groupByChanel, aggChannels, aggChannelsfilter, sourceTypes, aggFunctionTypes, aggOutputTypes,
                new boolean[]{true, true}, new boolean[]{false, false});
    }

    @Test(expectedExceptions = OmniRuntimeException.class, expectedExceptionsMessageRegExp = ".*EXPRESSION_NOT_SUPPORT.*")
    public void testHashAggWithInvalidAggKeys() {
        String[] groupByChanel = {getOmniJsonFieldReference(2, 0), getOmniJsonFieldReference(1, 2)};
        String[][] aggChannels = {{omniFunctionExpr("abc", 2, getOmniJsonFieldReference(2, 1))},
                {getOmniJsonFieldReference(1, 3)}};
        FunctionType[] aggFunctionTypes = {OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_AVG};
        DataType[][] aggOutputTypes = {{LongDataType.LONG}, {DoubleDataType.DOUBLE}};
        DataType[] sourceTypes = {LongDataType.LONG, LongDataType.LONG, IntDataType.INTEGER, IntDataType.INTEGER};
        String[] aggChannelsfilter = {null, null};
        OmniHashAggregationWithExprOperatorFactory factory = new OmniHashAggregationWithExprOperatorFactory(
                groupByChanel, aggChannels, aggChannelsfilter, sourceTypes, aggFunctionTypes, aggOutputTypes,
                new boolean[]{true, true}, new boolean[]{false, false});
    }

    @Test
    public void testFactoryContextEquals() {
        String[] groupByChanel = {getOmniJsonFieldReference(2, 0), getOmniJsonFieldReference(1, 2)};
        String[][] aggChannels = {{getOmniJsonFieldReference(2, 1)}, {getOmniJsonFieldReference(1, 3)}};

        FunctionType[] aggFunctionTypes = {OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_AVG};
        DataType[][] aggOutputTypes = {{LongDataType.LONG}, {DoubleDataType.DOUBLE}};

        DataType[] sourceTypes = {LongDataType.LONG, LongDataType.LONG, IntDataType.INTEGER, IntDataType.INTEGER};
        String[] aggChannelsfilter = {null, null};
        FactoryContext factory1 = new FactoryContext(groupByChanel, aggChannels, aggChannelsfilter, sourceTypes,
                aggFunctionTypes, aggOutputTypes, new boolean[]{true, true}, new boolean[]{false, false},
                new OperatorConfig());
        FactoryContext factory2 = new FactoryContext(groupByChanel, aggChannels, aggChannelsfilter, sourceTypes,
                aggFunctionTypes, aggOutputTypes, new boolean[]{true, true}, new boolean[]{false, false},
                new OperatorConfig());
        FactoryContext factory3 = null;

        assertEquals(factory2, factory1);
        assertEquals(factory1, factory1);
        assertNotEquals(factory3, factory1);
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

    private List<Vec> buildDataForOutputMultiVectorBatch(int rowNum) {
        LongVec c1 = new LongVec(rowNum);
        LongVec c2 = new LongVec(rowNum);
        for (int i = 0; i < rowNum; i++) {
            c1.set(i, i);
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
