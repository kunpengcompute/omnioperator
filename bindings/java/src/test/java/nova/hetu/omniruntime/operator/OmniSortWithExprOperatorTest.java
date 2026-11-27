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

import static nova.hetu.omniruntime.util.TestUtils.assertVecBatchEquals;
import static nova.hetu.omniruntime.util.TestUtils.createVecBatch;
import static nova.hetu.omniruntime.util.TestUtils.freeVecBatch;
import static nova.hetu.omniruntime.util.TestUtils.getOmniJsonFieldReference;
import static nova.hetu.omniruntime.util.TestUtils.getOmniJsonLiteral;
import static nova.hetu.omniruntime.util.TestUtils.omniFunctionExpr;
import static nova.hetu.omniruntime.util.TestUtils.omniJsonFourArithmeticExpr;
import static org.testng.Assert.assertEquals;
import static org.testng.Assert.assertNotEquals;
import static org.testng.Assert.assertTrue;

import nova.hetu.omniruntime.operator.config.OperatorConfig;
import nova.hetu.omniruntime.operator.config.OverflowConfig;
import nova.hetu.omniruntime.operator.config.SparkSpillConfig;
import nova.hetu.omniruntime.operator.sort.OmniSortWithExprOperatorFactory;
import nova.hetu.omniruntime.operator.sort.OmniSortWithExprOperatorFactory.FactoryContext;
import nova.hetu.omniruntime.type.DataType;
import nova.hetu.omniruntime.type.IntDataType;
import nova.hetu.omniruntime.type.LongDataType;
import nova.hetu.omniruntime.type.VarcharDataType;
import nova.hetu.omniruntime.util.TestUtils;
import nova.hetu.omniruntime.utils.OmniRuntimeException;
import nova.hetu.omniruntime.vector.Vec;
import nova.hetu.omniruntime.vector.VecBatch;

import org.testng.annotations.Test;

import java.io.File;
import java.nio.file.Paths;
import java.util.Iterator;
import java.util.UUID;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.TimeUnit;

/**
 * The type Omni sort with expression operator test.
 *
 * @since 2021-10-16
 */
public class OmniSortWithExprOperatorTest {
    private final long MAX_SPILL_BYTES = 20 * 1024 * 1024;

    private String generateSpillPath() {
        return "/opt" + File.separator + System.currentTimeMillis();
    }

    /**
     * Test Sort by zero columns which one with expression
     */
    @Test
    public void TestSortByZeroColumnWithExpr() {
        DataType[] sourceTypes = {IntDataType.INTEGER, LongDataType.LONG};
        Object[][] sourceDatas = {{5, 3, 2, 6, 1, 4, 7, 8}, {5L, 3L, 2L, 6L, 1L, 4L, 7L, 8L}};
        VecBatch vecBatch = createVecBatch(sourceTypes, sourceDatas);

        int[] outputCols = {0, 1};
        String[] sortKeys = {getOmniJsonFieldReference(1, 0), getOmniJsonFieldReference(2, 1)};
        int[] ascendings = {1, 1};
        int[] nullFirsts = {0, 0};
        OmniSortWithExprOperatorFactory sortWithExprOperatorFactory = new OmniSortWithExprOperatorFactory(sourceTypes,
                outputCols, sortKeys, ascendings, nullFirsts);
        OmniOperator sortWithExprOperator = sortWithExprOperatorFactory.createOperator();
        sortWithExprOperator.addInput(vecBatch);
        Iterator<VecBatch> results = sortWithExprOperator.getOutput();

        VecBatch resultVecBatch = results.next();
        assertEquals(resultVecBatch.getRowCount(), sourceDatas[0].length);
        Object[][] expectedDatas = {{1, 2, 3, 4, 5, 6, 7, 8}, {1L, 2L, 3L, 4L, 5L, 6L, 7L, 8L}};
        assertVecBatchEquals(resultVecBatch, expectedDatas);

        freeVecBatch(resultVecBatch);
        sortWithExprOperator.close();
        sortWithExprOperatorFactory.close();
    }

    /**
     * Test Sort by one columns which one with expression
     */
    @Test
    public void TestSortByOneColumnWithExpr() {
        DataType[] sourceTypes = {IntDataType.INTEGER, LongDataType.LONG};
        Object[][] sourceDatas = {{5, 3, 2, 6, 1, 4, 7, 8}, {5L, 3L, 2L, 6L, 1L, 4L, 7L, 8L}};
        VecBatch vecBatch = createVecBatch(sourceTypes, sourceDatas);

        int[] outputCols = {0, 1};
        String[] sortKeys = {
                omniJsonFourArithmeticExpr("ADD", 1, getOmniJsonFieldReference(1, 0), getOmniJsonLiteral(1, false, 5)),
                getOmniJsonFieldReference(2, 1)};
        int[] ascendings = {1, 1};
        int[] nullFirsts = {0, 0};
        OmniSortWithExprOperatorFactory sortWithExprOperatorFactory = new OmniSortWithExprOperatorFactory(sourceTypes,
                outputCols, sortKeys, ascendings, nullFirsts);
        OmniOperator sortWithExprOperator = sortWithExprOperatorFactory.createOperator();
        sortWithExprOperator.addInput(vecBatch);
        Iterator<VecBatch> results = sortWithExprOperator.getOutput();

        VecBatch resultVecBatch = results.next();
        assertEquals(resultVecBatch.getRowCount(), sourceDatas[0].length);
        Object[][] expectedDatas = {{1, 2, 3, 4, 5, 6, 7, 8}, {1L, 2L, 3L, 4L, 5L, 6L, 7L, 8L}};
        assertVecBatchEquals(resultVecBatch, expectedDatas);

        freeVecBatch(resultVecBatch);
        sortWithExprOperator.close();
        sortWithExprOperatorFactory.close();
    }

    /**
     * Test Sort by two columns with expression
     */
    @Test
    public void TestSortByTwoColumnsWithExpr() {
        DataType[] sourceTypes = {IntDataType.INTEGER, IntDataType.INTEGER};
        Object[][] sourceDatas = {{5, 3, 2, 6, 1, 4, 7, 8}, {5, 3, 2, 6, 1, 4, 7, 8}};
        VecBatch vecBatch = createVecBatch(sourceTypes, sourceDatas);

        int[] outputCols = {0, 1};
        String[] sortKeys = {
                omniJsonFourArithmeticExpr("ADD", 1, getOmniJsonFieldReference(1, 0), getOmniJsonLiteral(1, false, 5)),
                omniJsonFourArithmeticExpr("ADD", 1, getOmniJsonLiteral(1, false, 5), getOmniJsonFieldReference(1, 1))};
        int[] ascendings = {1, 1};
        int[] nullFirsts = {0, 0};
        OmniSortWithExprOperatorFactory sortWithExprOperatorFactory = new OmniSortWithExprOperatorFactory(sourceTypes,
                outputCols, sortKeys, ascendings, nullFirsts);
        OmniOperator sortWithExprOperator = sortWithExprOperatorFactory.createOperator();
        sortWithExprOperator.addInput(vecBatch);
        Iterator<VecBatch> results = sortWithExprOperator.getOutput();

        VecBatch resultVecBatch = results.next();
        assertEquals(resultVecBatch.getRowCount(), sourceDatas[0].length);
        Object[][] expectedDatas = {{1, 2, 3, 4, 5, 6, 7, 8}, {1, 2, 3, 4, 5, 6, 7, 8}};
        assertVecBatchEquals(resultVecBatch, expectedDatas);

        freeVecBatch(resultVecBatch);
        sortWithExprOperator.close();
        sortWithExprOperatorFactory.close();
    }

    /**
     * Test Sort by two dictionary columns with expression
     */
    @Test
    public void TestSortByTwoDictionaryWithExpr() {
        DataType[] sourceTypes = {IntDataType.INTEGER, IntDataType.INTEGER};
        Object[][] sourceDatas = {{5, 3, 2, 6, 1, 4, 7, 8}, {5, 3, 2, 6, 1, 4, 7, 8}};
        Vec[] vecs = new Vec[2];
        int[] ids = {0, 1, 2, 3, 4, 5, 6, 7};
        vecs[0] = TestUtils.createDictionaryVec(sourceTypes[0], sourceDatas[0], ids);
        vecs[1] = TestUtils.createDictionaryVec(sourceTypes[1], sourceDatas[1], ids);
        VecBatch vecBatch = new VecBatch(vecs);

        int[] outputCols = {0, 1};
        String[] sortKeys = {
                omniJsonFourArithmeticExpr("ADD", 1, getOmniJsonFieldReference(1, 0), getOmniJsonLiteral(1, false, 5)),
                omniJsonFourArithmeticExpr("ADD", 1, getOmniJsonLiteral(1, false, 5), getOmniJsonFieldReference(1, 1))};
        int[] ascendings = {1, 1};
        int[] nullFirsts = {0, 0};
        OmniSortWithExprOperatorFactory sortWithExprOperatorFactory = new OmniSortWithExprOperatorFactory(sourceTypes,
                outputCols, sortKeys, ascendings, nullFirsts);
        OmniOperator sortWithExprOperator = sortWithExprOperatorFactory.createOperator();
        sortWithExprOperator.addInput(vecBatch);
        Iterator<VecBatch> results = sortWithExprOperator.getOutput();

        VecBatch resultVecBatch = results.next();
        assertEquals(resultVecBatch.getRowCount(), sourceDatas[0].length);
        Object[][] expectedDatas = {{1, 2, 3, 4, 5, 6, 7, 8}, {1, 2, 3, 4, 5, 6, 7, 8}};
        assertVecBatchEquals(resultVecBatch, expectedDatas);

        freeVecBatch(resultVecBatch);
        sortWithExprOperator.close();
        sortWithExprOperatorFactory.close();
    }

    /**
     * Test factory context equal
     */
    @Test
    public void testFactoryContextEquals() {
        DataType[] sourceTypes = {IntDataType.INTEGER, IntDataType.INTEGER};
        int[] outputCols = {0, 1};
        String[] sortKeys = {
                omniJsonFourArithmeticExpr("ADD", 1, getOmniJsonFieldReference(1, 0), getOmniJsonLiteral(1, false, 5)),
                omniJsonFourArithmeticExpr("ADD", 1, getOmniJsonLiteral(1, false, 5), getOmniJsonFieldReference(1, 1))};
        int[] ascendings = {1, 1};
        int[] nullFirsts = {0, 0};
        FactoryContext factory1 = new FactoryContext(sourceTypes, outputCols, sortKeys, ascendings, nullFirsts,
                new OperatorConfig());
        FactoryContext factory2 = new FactoryContext(sourceTypes, outputCols, sortKeys, ascendings, nullFirsts,
                new OperatorConfig());
        FactoryContext factory3 = null;
        assertEquals(factory2, factory1);
        assertEquals(factory1, factory1);
        assertNotEquals(factory3, factory1);
    }

    /**
     * Test Sort spill ascending with spill
     */
    @Test
    public void testSortAscendingWithSpill() {
        DataType[] sourceTypes = {IntDataType.INTEGER, IntDataType.INTEGER, IntDataType.INTEGER};
        int[] outputCols = {0, 1, 2};
        String[] sortKeys = {getOmniJsonFieldReference(1, 1), getOmniJsonFieldReference(1, 0)};
        int[] ascendings = {1, 1};
        int[] nullFirsts = {0, 0};
        String spillPath = generateSpillPath();
        OmniSortWithExprOperatorFactory sortWithExprOperatorFactory = new OmniSortWithExprOperatorFactory(sourceTypes,
                outputCols, sortKeys, ascendings, nullFirsts,
                new OperatorConfig(new SparkSpillConfig(true, spillPath, MAX_SPILL_BYTES, 5)));
        OmniOperator sortWithExprOperator = sortWithExprOperatorFactory.createOperator();

        Object[][] sourceData1 = {{23, 23, 23, 23, 23, 23, 23, 23, 23, 23}, {1, 1, 1, 2, 1, 1, 1, 1, 2, 2},
                {12, 12, 12, 12, 12, 12, 12, 12, 12, 12}};
        VecBatch vecBatch1 = createVecBatch(sourceTypes, sourceData1);
        Object[][] sourceData2 = {{45, 45, 45, 45, 45, 45, 45, 45, 45, 45}, {1, 1, 1, 2, 1, 1, 1, 1, 2, 2},
                {24, 24, 24, 24, 24, 24, 24, 24, 24, 24}};
        VecBatch vecBatch2 = createVecBatch(sourceTypes, sourceData2);
        Object[][] sourceData3 = {{67, 67, 67, 67, 67, 67, 67, 67, 67, 67}, {1, 1, 1, 2, 1, 1, 1, 1, 2, 2},
                {36, 36, 36, 36, 36, 36, 36, 36, 36, 36}};
        VecBatch vecBatch3 = createVecBatch(sourceTypes, sourceData3);
        Object[][] sourceData4 = {{89, 89, 89, 89, 89, 89, 89, 89, 89, 89}, {1, 1, 1, 2, 1, 1, 1, 1, 2, 2},
                {48, 48, 48, 48, 48, 48, 48, 48, 48, 48}};
        VecBatch vecBatch4 = createVecBatch(sourceTypes, sourceData4);

        sortWithExprOperator.addInput(vecBatch1);
        sortWithExprOperator.addInput(vecBatch2);
        sortWithExprOperator.addInput(vecBatch3);
        sortWithExprOperator.addInput(vecBatch4);
        Iterator<VecBatch> results = sortWithExprOperator.getOutput();
        VecBatch resultVecBatch = results.next();

        Object[][] expectedDatas = {
                {23, 23, 23, 23, 23, 23, 23, 45, 45, 45, 45, 45, 45, 45, 67, 67, 67, 67, 67, 67, 67, 89, 89, 89, 89, 89,
                        89, 89, 23, 23, 23, 45, 45, 45, 67, 67, 67, 89, 89, 89},
                {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2,
                        2, 2, 2, 2, 2, 2},
                {12, 12, 12, 12, 12, 12, 12, 24, 24, 24, 24, 24, 24, 24, 36, 36, 36, 36, 36, 36, 36, 48, 48, 48, 48, 48,
                        48, 48, 12, 12, 12, 24, 24, 24, 36, 36, 36, 48, 48, 48}};
        assertVecBatchEquals(resultVecBatch, expectedDatas);

        freeVecBatch(resultVecBatch);
        long spilledBytes = sortWithExprOperator.getSpilledBytes();
        assertTrue(spilledBytes != 0);
        sortWithExprOperator.close();
        sortWithExprOperatorFactory.close();
        File spillDir = new File(spillPath);
        spillDir.delete();
    }

    /**
     * Test Sort spill descending with spill
     */
    @Test
    public void testSortDescendingWithSpill() {
        DataType[] sourceTypes = {IntDataType.INTEGER, IntDataType.INTEGER, IntDataType.INTEGER};
        int[] outputCols = {0, 1, 2};
        String[] sortKeys = {getOmniJsonFieldReference(1, 2), getOmniJsonFieldReference(1, 1)};
        int[] ascendings = {0, 1};
        int[] nullFirsts = {0, 0};
        String spillPath = generateSpillPath();
        OmniSortWithExprOperatorFactory sortWithExprOperatorFactory = new OmniSortWithExprOperatorFactory(sourceTypes,
                outputCols, sortKeys, ascendings, nullFirsts,
                new OperatorConfig(new SparkSpillConfig(true, spillPath, MAX_SPILL_BYTES, 5)));
        OmniOperator sortWithExprOperator = sortWithExprOperatorFactory.createOperator();

        Object[][] sourceData1 = {{23, 23, 23, 23, 23, 23, 23, 23, 23, 23}, {1, 1, 1, 2, 1, 1, 1, 1, 2, 2},
                {12, 12, 12, 12, 12, 12, 12, 12, 12, 12}};
        VecBatch vecBatch1 = createVecBatch(sourceTypes, sourceData1);
        Object[][] sourceData2 = {{45, 45, 45, 45, 45, 45, 45, 45, 45, 45}, {1, 1, 1, 2, 1, 1, 1, 1, 2, 2},
                {24, 24, 24, 24, 24, 24, 24, 24, 24, 24}};
        VecBatch vecBatch2 = createVecBatch(sourceTypes, sourceData2);
        Object[][] sourceData3 = {{67, 67, 67, 67, 67, 67, 67, 67, 67, 67}, {1, 1, 1, 2, 1, 1, 1, 1, 2, 2},
                {36, 36, 36, 36, 36, 36, 36, 36, 36, 36}};
        VecBatch vecBatch3 = createVecBatch(sourceTypes, sourceData3);
        Object[][] sourceData4 = {{89, 89, 89, 89, 89, 89, 89, 89, 89, 89}, {1, 1, 1, 2, 1, 1, 1, 1, 2, 2},
                {48, 48, 48, 48, 48, 48, 48, 48, 48, 48}};
        VecBatch vecBatch4 = createVecBatch(sourceTypes, sourceData4);

        sortWithExprOperator.addInput(vecBatch1);
        sortWithExprOperator.addInput(vecBatch2);
        sortWithExprOperator.addInput(vecBatch3);
        sortWithExprOperator.addInput(vecBatch4);
        Iterator<VecBatch> results = sortWithExprOperator.getOutput();
        VecBatch resultVecBatch = results.next();

        Object[][] expectedDatas = {
                {89, 89, 89, 89, 89, 89, 89, 89, 89, 89, 67, 67, 67, 67, 67, 67, 67, 67, 67, 67, 45, 45, 45, 45, 45, 45,
                        45, 45, 45, 45, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23},
                {1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 1, 1, 1, 1,
                        1, 1, 1, 2, 2, 2},
                {48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 24, 24, 24, 24, 24, 24,
                        24, 24, 24, 24, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12}};
        assertVecBatchEquals(resultVecBatch, expectedDatas);

        freeVecBatch(resultVecBatch);
        sortWithExprOperator.close();
        sortWithExprOperatorFactory.close();
        File spillDir = new File(spillPath);
        spillDir.delete();
    }

    /**
     * Test Sort spill with multi records
     */
    @Test
    public void testSortSpillWithMultiRecords() {
        DataType[] sourceTypes = {IntDataType.INTEGER, LongDataType.LONG};
        int[] outputCols = {0, 1};
        String[] sortKeys = {getOmniJsonFieldReference(1, 0), getOmniJsonFieldReference(2, 1)};
        int[] ascendings = {1, 1};
        int[] nullFirsts = {0, 0};
        String spillPath = generateSpillPath();
        OmniSortWithExprOperatorFactory sortWithExprOperatorFactory = new OmniSortWithExprOperatorFactory(sourceTypes,
                outputCols, sortKeys, ascendings, nullFirsts,
                new OperatorConfig(new SparkSpillConfig(true, spillPath, MAX_SPILL_BYTES, 5)));
        OmniOperator sortWithExprOperator = sortWithExprOperatorFactory.createOperator();

        Object[][] sourceDatas1 = {{5, 3, 2, 6, 1}, {5L, 3L, 2L, 6L, 1L}};
        VecBatch vecBatch1 = createVecBatch(sourceTypes, sourceDatas1);
        sortWithExprOperator.addInput(vecBatch1);

        Object[][] sourceDatas2 = {{4}, {4L}};
        VecBatch vecBatch2 = createVecBatch(sourceTypes, sourceDatas2);
        sortWithExprOperator.addInput(vecBatch2);

        Object[][] sourceDatas3 = {{15, 13, 12, 16, 11}, {15L, 13L, 12L, 16L, 11L}};
        VecBatch vecBatch3 = createVecBatch(sourceTypes, sourceDatas3);
        sortWithExprOperator.addInput(vecBatch3);
        Iterator<VecBatch> results = sortWithExprOperator.getOutput();

        VecBatch resultVecBatch = results.next();
        assertEquals(resultVecBatch.getRowCount(),
                sourceDatas1[0].length + sourceDatas2[0].length + sourceDatas3[0].length);
        Object[][] expectedDatas = {{1, 2, 3, 4, 5, 6, 11, 12, 13, 15, 16},
                {1L, 2L, 3L, 4L, 5L, 6L, 11L, 12L, 13L, 15L, 16L}};
        assertVecBatchEquals(resultVecBatch, expectedDatas);

        freeVecBatch(resultVecBatch);
        sortWithExprOperator.close();
        sortWithExprOperatorFactory.close();
        File spillDir = new File(spillPath);
        spillDir.delete();
    }

    /**
     * Test Sort spill with return multi batch
     */
    @Test
    public void testSortSpillWithReturnMultiBatch() {
        DataType[] sourceTypes = {IntDataType.INTEGER, IntDataType.INTEGER};
        int[] outputCols = {0, 1};
        String[] sortKeys = {getOmniJsonFieldReference(1, 0), getOmniJsonFieldReference(1, 1)};
        int[] ascendings = {1, 1};
        int[] nullFirsts = {0, 0};
        String spillPath = generateSpillPath();
        OmniSortWithExprOperatorFactory sortWithExprOperatorFactory = new OmniSortWithExprOperatorFactory(sourceTypes,
                outputCols, sortKeys, ascendings, nullFirsts,
                new OperatorConfig(new SparkSpillConfig(true, spillPath, MAX_SPILL_BYTES, 10000)));
        OmniOperator sortWithExprOperator = sortWithExprOperatorFactory.createOperator();

        int maxRowCntPerBatch = 131072; // 1M / (4+4)
        Object[][] sourceDatas1 = new Object[2][];
        sourceDatas1[0] = new Object[maxRowCntPerBatch];
        sourceDatas1[1] = new Object[maxRowCntPerBatch];
        for (int i = 0; i < maxRowCntPerBatch; i++) {
            sourceDatas1[0][maxRowCntPerBatch - i - 1] = i;
            sourceDatas1[1][maxRowCntPerBatch - i - 1] = i;
        }
        VecBatch vecBatch1 = createVecBatch(sourceTypes, sourceDatas1);
        sortWithExprOperator.addInput(vecBatch1);

        Object[][] sourceDatas2 = new Object[2][];
        sourceDatas2[0] = new Object[maxRowCntPerBatch];
        sourceDatas2[1] = new Object[maxRowCntPerBatch];
        for (int i = 0; i < maxRowCntPerBatch; i++) {
            sourceDatas2[0][maxRowCntPerBatch - i - 1] = maxRowCntPerBatch + i;
            sourceDatas2[1][maxRowCntPerBatch - i - 1] = maxRowCntPerBatch + i;
        }

        VecBatch vecBatch2 = createVecBatch(sourceTypes, sourceDatas2);
        sortWithExprOperator.addInput(vecBatch2);

        Object[][] sourceDatas3 = new Object[2][];
        sourceDatas3[0] = new Object[maxRowCntPerBatch];
        sourceDatas3[1] = new Object[maxRowCntPerBatch];
        for (int i = 0; i < maxRowCntPerBatch; i++) {
            sourceDatas3[0][maxRowCntPerBatch - i - 1] = maxRowCntPerBatch * 2 + i;
            sourceDatas3[1][maxRowCntPerBatch - i - 1] = maxRowCntPerBatch * 2 + i;
        }
        VecBatch vecBatch3 = createVecBatch(sourceTypes, sourceDatas3);
        sortWithExprOperator.addInput(vecBatch3);
        Iterator<VecBatch> results = sortWithExprOperator.getOutput();

        int baseValue = 0;
        while (results.hasNext()) {
            VecBatch resultVecBatch = results.next();
            assertEquals(resultVecBatch.getRowCount(), maxRowCntPerBatch);

            Object[][] expectedDatas = new Object[2][];
            expectedDatas[0] = new Object[maxRowCntPerBatch];
            expectedDatas[1] = new Object[maxRowCntPerBatch];
            for (int i = 0; i < maxRowCntPerBatch; i++) {
                expectedDatas[0][i] = baseValue + i;
                expectedDatas[1][i] = baseValue + i;
            }
            baseValue += maxRowCntPerBatch;
            assertVecBatchEquals(resultVecBatch, expectedDatas);
            freeVecBatch(resultVecBatch);
        }

        long spilledBytes = sortWithExprOperator.getSpilledBytes();
        assertTrue(spilledBytes != 0);
        sortWithExprOperator.close();
        sortWithExprOperatorFactory.close();
        File spillDir = new File(spillPath);
        spillDir.delete();
    }

    /**
     * Test Sort spill with one record
     */
    @Test
    public void testSortSpillWithOneRecord() {
        DataType[] sourceTypes = {IntDataType.INTEGER, LongDataType.LONG};
        int[] outputCols = {0, 1};
        String[] sortKeys = {getOmniJsonFieldReference(1, 0), getOmniJsonFieldReference(2, 1)};
        int[] ascendings = {1, 1};
        int[] nullFirsts = {0, 0};
        String spillPath = generateSpillPath();
        OmniSortWithExprOperatorFactory sortWithExprOperatorFactory = new OmniSortWithExprOperatorFactory(sourceTypes,
                outputCols, sortKeys, ascendings, nullFirsts,
                new OperatorConfig(new SparkSpillConfig(true, spillPath, MAX_SPILL_BYTES, 1)));
        OmniOperator sortWithExprOperator = sortWithExprOperatorFactory.createOperator();

        Object[][] sourceDatas1 = {{5}, {3L}};
        VecBatch vecBatch1 = createVecBatch(sourceTypes, sourceDatas1);
        sortWithExprOperator.addInput(vecBatch1);

        Object[][] sourceDatas2 = {{15}, {13L}};
        VecBatch vecBatch2 = createVecBatch(sourceTypes, sourceDatas2);
        sortWithExprOperator.addInput(vecBatch2);

        Object[][] sourceDatas3 = {{10}, {8L}};
        VecBatch vecBatch3 = createVecBatch(sourceTypes, sourceDatas3);
        sortWithExprOperator.addInput(vecBatch3);

        Iterator<VecBatch> results = sortWithExprOperator.getOutput();

        VecBatch resultVecBatch = results.next();
        assertEquals(resultVecBatch.getRowCount(),
                sourceDatas1[0].length + sourceDatas2[0].length + sourceDatas3[0].length);
        Object[][] expectedDatas = {{5, 10, 15}, {3L, 8L, 13L}};
        assertVecBatchEquals(resultVecBatch, expectedDatas);

        freeVecBatch(resultVecBatch);
        sortWithExprOperator.close();
        sortWithExprOperatorFactory.close();
        File spillDir = new File(spillPath);
        spillDir.delete();
    }

    /**
     * Test sort spill with null path
     */
    @Test(expectedExceptions = OmniRuntimeException.class, expectedExceptionsMessageRegExp = "Enable spill but do not config spill path.")
    public void testSortSpillWithNullPath() {
        DataType[] sourceTypes = {IntDataType.INTEGER, LongDataType.LONG};
        int[] outputCols = {0, 1};
        String[] sortKeys = {getOmniJsonFieldReference(1, 0), getOmniJsonFieldReference(2, 1)};
        int[] ascendings = {1, 1};
        int[] nullFirsts = {0, 0};

        OmniSortWithExprOperatorFactory sortWithExprOperatorFactory = new OmniSortWithExprOperatorFactory(sourceTypes,
                outputCols, sortKeys, ascendings, nullFirsts, new OperatorConfig(new SparkSpillConfig(null, 1)));
    }

    /**
     * Test sort spill with empty path
     */
    @Test(expectedExceptions = OmniRuntimeException.class, expectedExceptionsMessageRegExp = "Enable spill but do not config spill path.")
    public void testSortSpillWithEmptyPath() {
        DataType[] sourceTypes = {IntDataType.INTEGER, LongDataType.LONG};
        int[] outputCols = {0, 1};
        String[] sortKeys = {getOmniJsonFieldReference(1, 0), getOmniJsonFieldReference(2, 1)};
        int[] ascendings = {1, 1};
        int[] nullFirsts = {0, 0};

        OmniSortWithExprOperatorFactory sortWithExprOperatorFactory = new OmniSortWithExprOperatorFactory(sourceTypes,
                outputCols, sortKeys, ascendings, nullFirsts, new OperatorConfig(new SparkSpillConfig("", 1)));
    }

    /**
     * Test Sort spill with existed path
     */
    @Test
    public void testSortSpillWithExistedPath() {
        DataType[] sourceTypes = {IntDataType.INTEGER, LongDataType.LONG};
        int[] outputCols = {0, 1};
        String[] sortKeys = {getOmniJsonFieldReference(1, 0), getOmniJsonFieldReference(2, 1)};
        int[] ascendings = {1, 1};
        int[] nullFirsts = {0, 0};
        OmniSortWithExprOperatorFactory sortWithExprOperatorFactory = new OmniSortWithExprOperatorFactory(sourceTypes,
                outputCols, sortKeys, ascendings, nullFirsts,
                new OperatorConfig(new SparkSpillConfig(true, "/opt", MAX_SPILL_BYTES, 1)));
        OmniOperator sortWithExprOperator = sortWithExprOperatorFactory.createOperator();
        Object[][] sourceDatas1 = {{5, 3}, {5L, 3L}};
        VecBatch vecBatch = createVecBatch(sourceTypes, sourceDatas1);
        sortWithExprOperator.addInput(vecBatch);
        Iterator<VecBatch> results = sortWithExprOperator.getOutput();

        VecBatch resultVecBatch = results.next();
        Object[][] expectedDatas = {{3, 5}, {3L, 5L}};
        assertVecBatchEquals(resultVecBatch, expectedDatas);

        freeVecBatch(resultVecBatch);
        sortWithExprOperator.close();
        sortWithExprOperatorFactory.close();
    }

    /**
     * Test sort spill with invalid path
     */
    @Test
    public void testSortSpillWithInvalidPath() {
        DataType[] sourceTypes = {IntDataType.INTEGER, LongDataType.LONG};
        int[] outputCols = {0, 1};
        String[] sortKeys = {getOmniJsonFieldReference(1, 0), getOmniJsonFieldReference(2, 1)};
        int[] ascendings = {1, 1};
        int[] nullFirsts = {0, 0};
        String spillPath = "/opt/+-ab23";
        OmniSortWithExprOperatorFactory sortWithExprOperatorFactory = new OmniSortWithExprOperatorFactory(sourceTypes,
                outputCols, sortKeys, ascendings, nullFirsts,
                new OperatorConfig(new SparkSpillConfig(true, spillPath, MAX_SPILL_BYTES, 1)));
        OmniOperator sortWithExprOperator = sortWithExprOperatorFactory.createOperator();
        Object[][] sourceDatas1 = {{5, 3}, {5L, 3L}};
        VecBatch vecBatch = createVecBatch(sourceTypes, sourceDatas1);
        sortWithExprOperator.addInput(vecBatch);
        Iterator<VecBatch> results = sortWithExprOperator.getOutput();

        VecBatch resultVecBatch = results.next();
        Object[][] expectedDatas = {{3, 5}, {3L, 5L}};
        assertVecBatchEquals(resultVecBatch, expectedDatas);

        freeVecBatch(resultVecBatch);
        sortWithExprOperator.close();
        sortWithExprOperatorFactory.close();
        File spillDir = new File(spillPath);
        spillDir.delete();
    }

    /**
     * Test sort spill with invalid keys
     */
    @Test(expectedExceptions = OmniRuntimeException.class, expectedExceptionsMessageRegExp = ".*EXPRESSION_NOT_SUPPORT.*")
    public void testSortWithInvalidKeys() {
        DataType[] sourceTypes = {IntDataType.INTEGER, LongDataType.LONG};
        int[] outputCols = {0, 1};
        String[] sortKeys = {omniFunctionExpr("abc", 2, getOmniJsonFieldReference(2, 1))};
        int[] ascendings = {1};
        int[] nullFirsts = {0};
        OmniSortWithExprOperatorFactory sortWithExprOperatorFactory = new OmniSortWithExprOperatorFactory(sourceTypes,
                outputCols, sortKeys, ascendings, nullFirsts);
    }

    /**
     * Test sort multi threads
     */
    @Test
    public void testSortWithExprOperatorFactoryMultiThreads() {
        final int threadNum = 100;
        final int corePoolSize = 50;
        final int maximumPoolSize = 100;
        CountDownLatch countDownLatch = new CountDownLatch(threadNum);
        ThreadPoolExecutor threadPool = new ThreadPoolExecutor(corePoolSize, maximumPoolSize, 60L, TimeUnit.SECONDS,
                new LinkedBlockingQueue<>(threadNum));
        for (int i = 0; i < threadNum; i++) {
            CompletableFuture.runAsync(() -> {
                try {
                    DataType[] sourceTypes = {VarcharDataType.VARCHAR, IntDataType.INTEGER};
                    int[] outputCols = {0, 1};
                    String[] sortKeys = {getOmniJsonFieldReference(1, 1)};
                    int[] ascendings = {1};
                    int[] nullFirsts = {0};
                    String spillPath = Paths.get("").toAbsolutePath() + File.separator + UUID.randomUUID();
                    SparkSpillConfig spillConfig = new SparkSpillConfig(false, spillPath, Long.MAX_VALUE,
                            Integer.MAX_VALUE);
                    OmniSortWithExprOperatorFactory sortWithExprOperatorFactory = new OmniSortWithExprOperatorFactory(
                            sourceTypes, outputCols, sortKeys, ascendings, nullFirsts,
                            new OperatorConfig(spillConfig, new OverflowConfig(), true));
                    sortWithExprOperatorFactory.close();
                } finally {
                    countDownLatch.countDown();
                }
            }, threadPool);
        }

        // This will wait until all future ready.
        try {
            countDownLatch.await();
        } catch (InterruptedException e) {
            assertTrue(false);
        }

        threadPool.shutdown();
    }
}
