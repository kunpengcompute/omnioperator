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

import static nova.hetu.omniruntime.constants.JoinType.OMNI_JOIN_TYPE_INNER;
import static nova.hetu.omniruntime.constants.JoinType.OMNI_JOIN_TYPE_LEFT_ANTI;
import static nova.hetu.omniruntime.constants.JoinType.OMNI_JOIN_TYPE_LEFT_SEMI;
import static nova.hetu.omniruntime.util.TestUtils.assertVecBatchEquals;
import static nova.hetu.omniruntime.util.TestUtils.createBlankVecBatch;
import static nova.hetu.omniruntime.util.TestUtils.createVecBatch;
import static nova.hetu.omniruntime.util.TestUtils.decodeAddFlag;
import static nova.hetu.omniruntime.util.TestUtils.decodeFetchFlag;
import static nova.hetu.omniruntime.util.TestUtils.freeVecBatch;
import static nova.hetu.omniruntime.util.TestUtils.getOmniJsonFieldReference;
import static nova.hetu.omniruntime.util.TestUtils.getOmniJsonLiteral;
import static nova.hetu.omniruntime.util.TestUtils.omniFunctionExpr;
import static nova.hetu.omniruntime.util.TestUtils.omniJsonFourArithmeticExpr;
import static org.testng.Assert.assertEquals;
import static org.testng.Assert.assertNotEquals;

import nova.hetu.omniruntime.operator.config.OperatorConfig;
import nova.hetu.omniruntime.operator.join.OmniSmjBufferedTableWithExprOperatorFactory;
import nova.hetu.omniruntime.operator.join.OmniSmjStreamedTableWithExprOperatorFactory;
import nova.hetu.omniruntime.type.DataType;
import nova.hetu.omniruntime.type.DoubleDataType;
import nova.hetu.omniruntime.type.IntDataType;
import nova.hetu.omniruntime.type.LongDataType;
import nova.hetu.omniruntime.utils.OmniRuntimeException;
import nova.hetu.omniruntime.vector.VecBatch;

import org.testng.annotations.Test;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.Optional;

/**
 * The type Omni sort merge join with expression operators test.
 *
 * @since 2022-1-10
 */
public class OmniSortMergeJoinWithExprOperatorsTest {
    /**
     * Test inner hash join one column 1.
     */
    @Test
    public void testSmjOneTimeEqualCondition() {
        DataType[] streamedTypes = {IntDataType.INTEGER, LongDataType.LONG};

        String[] streamedKeyExps = {
                omniJsonFourArithmeticExpr("ADD", 1, getOmniJsonFieldReference(1, 0), getOmniJsonLiteral(1, false, 5))};
        int[] streamedOutputCols = {1};
        OmniSmjStreamedTableWithExprOperatorFactory streamedBuilderWithExprOperatorFactory =
                new OmniSmjStreamedTableWithExprOperatorFactory(
                streamedTypes, streamedKeyExps, streamedOutputCols, OMNI_JOIN_TYPE_INNER, Optional.empty());

        DataType[] bufferedTypes = {LongDataType.LONG, IntDataType.INTEGER};

        int[] bufferedOutputCols = {0};
        String[] bufferedKeyExps = {
                omniJsonFourArithmeticExpr("ADD", 1, getOmniJsonFieldReference(1, 1), getOmniJsonLiteral(1, false, 5))};
        OmniSmjBufferedTableWithExprOperatorFactory bufferedWithExprOperatorFactory =
                new OmniSmjBufferedTableWithExprOperatorFactory(
                bufferedTypes, bufferedKeyExps, bufferedOutputCols, streamedBuilderWithExprOperatorFactory);
        OmniOperator bufferedTableOperator = bufferedWithExprOperatorFactory.createOperator();

        // start to add input
        Object[][] streamedDatas1 = {{0, 1, 2, 3, 4, 5}, {6600L, 5500L, 4400L, 3300L, 2200L, 1100L}};
        VecBatch streamedVecBatch1 = createVecBatch(streamedTypes, streamedDatas1);
        OmniOperator streamedTableOperator = streamedBuilderWithExprOperatorFactory.createOperator();
        int intputResult = streamedTableOperator.addInput(streamedVecBatch1);
        assertEquals(decodeAddFlag(intputResult), 3);

        Object[][] bufferedDatas1 = {{6006L, 5005L, 4004L, 3003L, 2002L, 1001L}, {0, 1, 2, 3, 4, 5}};
        VecBatch bufferedVecBatch1 = createVecBatch(bufferedTypes, bufferedDatas1);
        intputResult = bufferedTableOperator.addInput(bufferedVecBatch1);
        assertEquals(decodeAddFlag(intputResult), 3);

        VecBatch bufferedVecBatchEof = createBlankVecBatch(bufferedTypes);
        intputResult = bufferedTableOperator.addInput(bufferedVecBatchEof);
        assertEquals(decodeAddFlag(intputResult), 2);

        VecBatch streamedVecBatchEof = createBlankVecBatch(streamedTypes);
        intputResult = streamedTableOperator.addInput(streamedVecBatchEof);
        assertEquals(decodeFetchFlag(intputResult), 5);

        Iterator<VecBatch> results = bufferedTableOperator.getOutput();
        VecBatch resultVecBatch = results.next();

        int len = resultVecBatch.getRowCount();
        assertEquals(len, 6);
        Object[][] expectedDatas = {{6600L, 5500L, 4400L, 3300L, 2200L, 1100L},
                {6006L, 5005L, 4004L, 3003L, 2002L, 1001L}};
        assertVecBatchEquals(resultVecBatch, expectedDatas);

        freeVecBatch(resultVecBatch);
        bufferedTableOperator.close();
        bufferedWithExprOperatorFactory.close();
        streamedTableOperator.close();
        streamedBuilderWithExprOperatorFactory.close();
    }

    @Test(expectedExceptions = OmniRuntimeException.class, expectedExceptionsMessageRegExp =
            ".*EXPRESSION_NOT_SUPPORT.*")
    public void testInvalidStreamedKeys() {
        DataType[] streamedTypes = {IntDataType.INTEGER, LongDataType.LONG};
        String[] streamedKeyExps = {omniFunctionExpr("abc", 1, getOmniJsonFieldReference(1, 0))};
        int[] streamedOutputCols = {1};
        OmniSmjStreamedTableWithExprOperatorFactory streamedBuilderWithExprOperatorFactory = new OmniSmjStreamedTableWithExprOperatorFactory(
                streamedTypes, streamedKeyExps, streamedOutputCols, OMNI_JOIN_TYPE_INNER, Optional.empty());
    }

    @Test(expectedExceptions = OmniRuntimeException.class, expectedExceptionsMessageRegExp =
            ".*EXPRESSION_NOT_SUPPORT.*")
    public void testInvalidBufferedKeys() {
        DataType[] streamedTypes = {IntDataType.INTEGER, LongDataType.LONG};
        String[] streamedKeyExps = {
                omniJsonFourArithmeticExpr("ADD", 1, getOmniJsonFieldReference(1, 0), getOmniJsonLiteral(1, false, 5))};
        int[] streamedOutputCols = {1};
        OmniSmjStreamedTableWithExprOperatorFactory streamedBuilderWithExprOperatorFactory =
                new OmniSmjStreamedTableWithExprOperatorFactory(
                streamedTypes, streamedKeyExps, streamedOutputCols, OMNI_JOIN_TYPE_INNER, Optional.empty());

        DataType[] bufferedTypes = {LongDataType.LONG, IntDataType.INTEGER};
        int[] bufferedOutputCols = {0};
        String[] bufferedKeyExps = {omniFunctionExpr("abc", 2, getOmniJsonFieldReference(2, 1))};
        OmniSmjBufferedTableWithExprOperatorFactory bufferedWithExprOperatorFactory =
                new OmniSmjBufferedTableWithExprOperatorFactory(
                bufferedTypes, bufferedKeyExps, bufferedOutputCols, streamedBuilderWithExprOperatorFactory);
    }

    @Test
    public void testFactoryContextEquals() {
        DataType[] streamedTypes = {IntDataType.INTEGER, LongDataType.LONG};

        String[] streamedKeyExps = {
                omniJsonFourArithmeticExpr("ADD", 1, getOmniJsonFieldReference(1, 0), getOmniJsonLiteral(1, false, 5))};
        int[] streamedOutputCols = {1};
        OmniSmjStreamedTableWithExprOperatorFactory.FactoryContext streamedBuilderWithExprOperatorFactory1 =
                new OmniSmjStreamedTableWithExprOperatorFactory.FactoryContext(
                streamedTypes, streamedKeyExps, streamedOutputCols, OMNI_JOIN_TYPE_INNER, Optional.empty(),
                new OperatorConfig());
        OmniSmjStreamedTableWithExprOperatorFactory.FactoryContext streamedBuilderWithExprOperatorFactory2 =
                new OmniSmjStreamedTableWithExprOperatorFactory.FactoryContext(
                streamedTypes, streamedKeyExps, streamedOutputCols, OMNI_JOIN_TYPE_INNER, Optional.empty(),
                new OperatorConfig());
        OmniSmjStreamedTableWithExprOperatorFactory.FactoryContext streamedBuilderWithExprOperatorFactory3 = null;
        assertEquals(streamedBuilderWithExprOperatorFactory2, streamedBuilderWithExprOperatorFactory1);
        assertEquals(streamedBuilderWithExprOperatorFactory1, streamedBuilderWithExprOperatorFactory1);
        assertNotEquals(streamedBuilderWithExprOperatorFactory3, streamedBuilderWithExprOperatorFactory1);

        DataType[] bufferedTypes = {LongDataType.LONG, IntDataType.INTEGER};

        OmniSmjStreamedTableWithExprOperatorFactory streamedBuilderWithExprOperatorFactory =
                new OmniSmjStreamedTableWithExprOperatorFactory(
                streamedTypes, streamedKeyExps, streamedOutputCols, OMNI_JOIN_TYPE_INNER, Optional.empty());

        int[] bufferedOutputCols = {0};
        String[] bufferedKeyExps = {
                omniJsonFourArithmeticExpr("ADD", 1, getOmniJsonFieldReference(1, 0), getOmniJsonLiteral(1, false, 5))};
        OmniSmjBufferedTableWithExprOperatorFactory.FactoryContext bufferedWithExprOperatorFactory1 =
                new OmniSmjBufferedTableWithExprOperatorFactory.FactoryContext(
                bufferedTypes, bufferedKeyExps, bufferedOutputCols, new OperatorConfig(),
                streamedBuilderWithExprOperatorFactory);
        OmniSmjBufferedTableWithExprOperatorFactory.FactoryContext bufferedWithExprOperatorFactory2 =
                new OmniSmjBufferedTableWithExprOperatorFactory.FactoryContext(
                bufferedTypes, bufferedKeyExps, bufferedOutputCols, new OperatorConfig(),
                streamedBuilderWithExprOperatorFactory);
        OmniSmjBufferedTableWithExprOperatorFactory.FactoryContext bufferedWithExprOperatorFactory3 = null;
        assertEquals(bufferedWithExprOperatorFactory2, bufferedWithExprOperatorFactory1);
        assertEquals(bufferedWithExprOperatorFactory1, bufferedWithExprOperatorFactory1);
        assertNotEquals(bufferedWithExprOperatorFactory3, bufferedWithExprOperatorFactory1);
    }

    /**
     * Test left semi join one column 1.
     */
    @Test
    public void testSmjOneTimeEqualConditionForLeftSemiJoin() {
        DataType[] streamedTypes = {IntDataType.INTEGER, LongDataType.LONG};
        String[] streamedKeyExps = {
                omniJsonFourArithmeticExpr("ADD", 1, getOmniJsonFieldReference(1, 0), getOmniJsonLiteral(1, false, 5))};
        int[] streamedOutputCols = {0, 1};
        OmniSmjStreamedTableWithExprOperatorFactory streamedBuilderWithExprOperatorFactory = new OmniSmjStreamedTableWithExprOperatorFactory(
                streamedTypes, streamedKeyExps, streamedOutputCols, OMNI_JOIN_TYPE_LEFT_SEMI, Optional.empty());

        DataType[] bufferedTypes = {LongDataType.LONG, IntDataType.INTEGER};
        int[] bufferedOutputCols = {};
        String[] bufferedKeyExps = {
                omniJsonFourArithmeticExpr("ADD", 1, getOmniJsonFieldReference(1, 1), getOmniJsonLiteral(1, false, 5))};
        OmniSmjBufferedTableWithExprOperatorFactory bufferedWithExprOperatorFactory = new OmniSmjBufferedTableWithExprOperatorFactory(
                bufferedTypes, bufferedKeyExps, bufferedOutputCols, streamedBuilderWithExprOperatorFactory);
        OmniOperator bufferedTableOperator = bufferedWithExprOperatorFactory.createOperator();

        // start to add input
        Object[][] streamedDatas1 = {{0, 1, 2, 2, 2, 3, 4, 5},
                {8800L, 7700L, 6600L, 5500L, 4400L, 3300L, 2200L, 1100L}};
        VecBatch streamedVecBatch1 = createVecBatch(streamedTypes, streamedDatas1); // 0, 1, 2, 2, 2, 3, 4, 5
        OmniOperator streamedTableOperator = streamedBuilderWithExprOperatorFactory.createOperator();
        int intputResult = streamedTableOperator.addInput(streamedVecBatch1);
        assertEquals(decodeAddFlag(intputResult), 3);

        Object[][] bufferedDatas1 = {{8008L, 7007L, 6006L, 5005L, 4004L, 3003L, 2002L, 1001L},
                {0, 1, 2, 2, 3, 3, 4, 5}};
        VecBatch bufferedVecBatch1 = createVecBatch(bufferedTypes, bufferedDatas1); // 0, 1, 2, 2, 3, 3, 4, 5
        intputResult = bufferedTableOperator.addInput(bufferedVecBatch1);
        assertEquals(decodeAddFlag(intputResult), 3);

        VecBatch bufferedVecBatchEof = createBlankVecBatch(bufferedTypes);
        intputResult = bufferedTableOperator.addInput(bufferedVecBatchEof);
        assertEquals(decodeAddFlag(intputResult), 2);

        VecBatch streamedVecBatchEof = createBlankVecBatch(streamedTypes);
        intputResult = streamedTableOperator.addInput(streamedVecBatchEof);
        assertEquals(decodeFetchFlag(intputResult), 5);

        Iterator<VecBatch> results = bufferedTableOperator.getOutput();
        VecBatch resultVecBatch = results.next();

        int len = resultVecBatch.getRowCount();
        assertEquals(len, 8);
        Object[][] expectedDatas = {{0, 1, 2, 2, 2, 3, 4, 5}, {8800L, 7700L, 6600L, 5500L, 4400L, 3300L, 2200L, 1100L}};
        assertVecBatchEquals(resultVecBatch, expectedDatas);

        freeVecBatch(resultVecBatch);
        bufferedTableOperator.close();
        bufferedWithExprOperatorFactory.close();
        streamedTableOperator.close();
        streamedBuilderWithExprOperatorFactory.close();
    }

    /**
     * Test left anti join one column 1.
     */
    @Test
    public void testSmjOneTimeEqualConditionForLeftAntiJoin() {
        DataType[] streamedTypes = {IntDataType.INTEGER, LongDataType.LONG};
        String[] streamedKeyExps = {
                omniJsonFourArithmeticExpr("ADD", 1, getOmniJsonFieldReference(1, 0), getOmniJsonLiteral(1, false, 5))};
        int[] streamedOutputCols = {0, 1};
        OmniSmjStreamedTableWithExprOperatorFactory streamedBuilderWithExprOperatorFactory = new OmniSmjStreamedTableWithExprOperatorFactory(
                streamedTypes, streamedKeyExps, streamedOutputCols, OMNI_JOIN_TYPE_LEFT_ANTI, Optional.empty());

        DataType[] bufferedTypes = {IntDataType.INTEGER, DoubleDataType.DOUBLE};
        int[] bufferedOutputCols = {};
        String[] bufferedKeyExps = {
                omniJsonFourArithmeticExpr("ADD", 1, getOmniJsonFieldReference(1, 0), getOmniJsonLiteral(1, false, 5))};
        OmniSmjBufferedTableWithExprOperatorFactory bufferedWithExprOperatorFactory = new OmniSmjBufferedTableWithExprOperatorFactory(
                bufferedTypes, bufferedKeyExps, bufferedOutputCols, streamedBuilderWithExprOperatorFactory);
        OmniOperator bufferedTableOperator = bufferedWithExprOperatorFactory.createOperator();

        // start to add input
        Object[][] streamedDatas1 = {{1, 1, 2, 3}, {40L, 25L, 35L, 30L}};
        VecBatch streamedVecBatch1 = createVecBatch(streamedTypes, streamedDatas1);
        OmniOperator streamedTableOperator = streamedBuilderWithExprOperatorFactory.createOperator();
        int intputResult = streamedTableOperator.addInput(streamedVecBatch1);
        assertEquals(decodeAddFlag(intputResult), 3);

        Object[][] bufferedDatas1 = {{3, 3, 4, 4}, {3.3, 3.5, 4.4, 4.5}};
        VecBatch bufferedVecBatch1 = createVecBatch(bufferedTypes, bufferedDatas1);
        intputResult = bufferedTableOperator.addInput(bufferedVecBatch1);
        assertEquals(decodeAddFlag(intputResult), 2);

        VecBatch streamedVecBatchEof = createBlankVecBatch(streamedTypes);
        intputResult = streamedTableOperator.addInput(streamedVecBatchEof);
        assertEquals(decodeFetchFlag(intputResult), 5);

        Iterator<VecBatch> results = bufferedTableOperator.getOutput();
        VecBatch resultVecBatch = results.next();

        int len = resultVecBatch.getRowCount();
        assertEquals(len, 3);
        Object[][] expectedDatas = {{1, 1, 2}, {40L, 25L, 35L}};
        assertVecBatchEquals(resultVecBatch, expectedDatas);

        freeVecBatch(resultVecBatch);
        bufferedTableOperator.close();
        bufferedWithExprOperatorFactory.close();
        streamedTableOperator.close();
        streamedBuilderWithExprOperatorFactory.close();
    }

    private void buildAddInputData(Object[][] streamedData1, Object[][] bufferedData1, Object[][] streamedData2,
            Object[][] bufferedData2, int tableSize) {
        for (int i = 0; i < tableSize; i++) {
            streamedData1[0][i] = i;
            streamedData2[0][i] = i + tableSize;
            for (int j = 1; j < streamedData1.length; j++) {
                streamedData1[j][i] = i + 1001L;
                streamedData2[j][i] = i + 1001L + tableSize;
            }

            bufferedData1[bufferedData1.length - 1][i] = i;
            bufferedData2[bufferedData2.length - 1][i] = i + tableSize;
            for (int k = 0; k < bufferedData1.length - 1; k++) {
                bufferedData1[k][i] = i + 1003L;
                bufferedData2[k][i] = i + 1003L + tableSize;
            }
        }
    }

    private void buildExpectedData(Object[][] expectedData1, Object[][] expectedData2, Object[][] expectedData3,
            Object[][] expectedData4, int maxRowCount, int remainCount) {
        for (int i = 0; i < maxRowCount; i++) {
            for (int j = 0; j < 4; j++) {
                expectedData1[j][i] = i + 1001L;
                expectedData1[j + 4][i] = i + 1003L;
                expectedData3[j][i] = i + 1001L + maxRowCount + remainCount;
                expectedData3[j + 4][i] = i + 1003L + maxRowCount + remainCount;
            }
        }

        for (int i = 0; i < remainCount; i++) {
            for (int j = 0; j < 4; j++) {
                expectedData2[j][i] = i + 1001L + maxRowCount;
                expectedData4[j][i] = i + 1001L + 2 * maxRowCount + remainCount;
                expectedData2[j + 4][i] = i + 1003L + maxRowCount;
                expectedData4[j + 4][i] = i + 1003L + 2 * maxRowCount + remainCount;
            }
        }
        for (int j = 0; j < 4; j++) {
            expectedData4[j][remainCount] = remainCount + 1001L + 2 * maxRowCount + remainCount;
            expectedData4[j + 4][remainCount] = remainCount + 1003L + 2 * maxRowCount + remainCount;
        }
    }

    /**
     * Test smj iterative getOutput.
     */
    @Test
    public void testSmjIterativeGetOutput() {
        DataType[] streamedTypes = {IntDataType.INTEGER, LongDataType.LONG, LongDataType.LONG, LongDataType.LONG,
                LongDataType.LONG};

        String[] streamedKeyExps = {
                omniJsonFourArithmeticExpr("ADD", 1, getOmniJsonFieldReference(1, 0), getOmniJsonLiteral(1, false, 5))};
        int[] streamedOutputCols = {1, 2, 3, 4};
        OmniSmjStreamedTableWithExprOperatorFactory streamedBuilderWithExprOperatorFactory =
                new OmniSmjStreamedTableWithExprOperatorFactory(streamedTypes, streamedKeyExps, streamedOutputCols,
                OMNI_JOIN_TYPE_INNER, Optional.empty());

        DataType[] bufferedTypes = {LongDataType.LONG, LongDataType.LONG, LongDataType.LONG, LongDataType.LONG,
                IntDataType.INTEGER};

        int[] bufferedOutputCols = {0, 1, 2, 3};

        String[] bufferedKeyExps = {
                omniJsonFourArithmeticExpr("ADD", 1, getOmniJsonFieldReference(1, 4), getOmniJsonLiteral(1, false, 5))};
        OmniSmjBufferedTableWithExprOperatorFactory bufferedWithExprOperatorFactory =
                new OmniSmjBufferedTableWithExprOperatorFactory(bufferedTypes, bufferedKeyExps, bufferedOutputCols,
                streamedBuilderWithExprOperatorFactory);
        OmniOperator bufferedTableOperator = bufferedWithExprOperatorFactory.createOperator();

        // construct addInput data
        int tableSize = 20000;
        Object[][] streamedData1 = new Object[5][tableSize];
        Object[][] bufferedData1 = new Object[5][tableSize];
        Object[][] streamedData2 = new Object[5][tableSize];
        Object[][] bufferedData2 = new Object[5][tableSize];
        buildAddInputData(streamedData1, bufferedData1, streamedData2, bufferedData2, tableSize);

        // start to add input
        VecBatch streamedVecBatch1 = createVecBatch(streamedTypes, streamedData1);
        OmniOperator streamedTableOperator = streamedBuilderWithExprOperatorFactory.createOperator();
        int intputResult = streamedTableOperator.addInput(streamedVecBatch1);
        assertEquals(decodeAddFlag(intputResult), 3);

        VecBatch bufferedVecBatch1 = createVecBatch(bufferedTypes, bufferedData1);
        intputResult = bufferedTableOperator.addInput(bufferedVecBatch1);
        assertEquals(decodeAddFlag(intputResult), 3);
        assertEquals(decodeFetchFlag(intputResult), 5);

        Iterator<VecBatch> results = bufferedTableOperator.getOutput();
        VecBatch resultVecBatch = null;

        List<VecBatch> result = new ArrayList<>();
        while (results.hasNext()) {
            resultVecBatch = results.next();
            result.add(resultVecBatch);
        }

        VecBatch bufferedVecBatch2 = createVecBatch(bufferedTypes, bufferedData2);
        intputResult = bufferedTableOperator.addInput(bufferedVecBatch2);
        assertEquals(decodeAddFlag(intputResult), 2);

        VecBatch streamedVecBatch2 = createVecBatch(streamedTypes, streamedData2);
        intputResult = streamedTableOperator.addInput(streamedVecBatch2);
        assertEquals(decodeAddFlag(intputResult), 3);
        assertEquals(decodeFetchFlag(intputResult), 5);

        results = streamedTableOperator.getOutput();
        while (results.hasNext()) {
            resultVecBatch = results.next();
            result.add(resultVecBatch);
        }

        VecBatch bufferedVecBatchEof = createBlankVecBatch(bufferedTypes);
        intputResult = bufferedTableOperator.addInput(bufferedVecBatchEof);
        assertEquals(decodeAddFlag(intputResult), 2);

        VecBatch streamedVecBatchEof = createBlankVecBatch(streamedTypes);
        intputResult = streamedTableOperator.addInput(streamedVecBatchEof);
        assertEquals(decodeFetchFlag(intputResult), 5);

        results = bufferedTableOperator.getOutput();

        while (results.hasNext()) {
            resultVecBatch = results.next();
            result.add(resultVecBatch);
        }

        int rowCount = 0;
        for (int i = 0; i < result.size(); i++) {
            rowCount += result.get(i).getRowCount();
        }

        assertEquals(rowCount, 2 * tableSize);

        int maxRowCount = 16384; // 1M / (8 * 8)
        int remainCount = tableSize - maxRowCount - 1;
        // construct expected data
        Object[][] expectedData1 = new Object[8][maxRowCount];
        Object[][] expectedData2 = new Object[8][remainCount];
        Object[][] expectedData3 = new Object[8][maxRowCount];
        Object[][] expectedData4 = new Object[8][remainCount + 1];
        buildExpectedData(expectedData1, expectedData2, expectedData3, expectedData4, maxRowCount, remainCount);
        Object[][] expectedData5 = {{41000L}, {41000L}, {41000L}, {41000L}, {41002L}, {41002L}, {41002L}, {41002L}};

        assertVecBatchEquals(result.get(0), expectedData1);
        assertVecBatchEquals(result.get(1), expectedData2);
        assertVecBatchEquals(result.get(2), expectedData3);
        assertVecBatchEquals(result.get(3), expectedData4);
        assertVecBatchEquals(result.get(4), expectedData5);

        for (int i = 0; i < result.size(); i++) {
            freeVecBatch(result.get(i));
        }

        bufferedTableOperator.close();
        bufferedWithExprOperatorFactory.close();
        streamedTableOperator.close();
        streamedBuilderWithExprOperatorFactory.close();
    }
}
