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
import static nova.hetu.omniruntime.util.TestUtils.freeVecBatches;
import static nova.hetu.omniruntime.util.TestUtils.getOmniJsonFieldReference;
import static nova.hetu.omniruntime.util.TestUtils.getOmniJsonLiteral;
import static nova.hetu.omniruntime.util.TestUtils.omniFunctionExpr;
import static nova.hetu.omniruntime.util.TestUtils.omniJsonFourArithmeticExpr;
import static org.testng.Assert.assertEquals;
import static org.testng.Assert.assertNotEquals;

import nova.hetu.omniruntime.operator.config.OperatorConfig;
import nova.hetu.omniruntime.operator.topn.OmniTopNWithExprOperatorFactory;
import nova.hetu.omniruntime.operator.topn.OmniTopNWithExprOperatorFactory.FactoryContext;
import nova.hetu.omniruntime.type.DataType;
import nova.hetu.omniruntime.type.IntDataType;
import nova.hetu.omniruntime.type.LongDataType;
import nova.hetu.omniruntime.utils.OmniRuntimeException;
import nova.hetu.omniruntime.vector.VecBatch;

import org.testng.annotations.Test;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.Arrays;

/**
 * The type Omni TopN with expression operator test.
 *
 * @since 2021-11-11
 */
public class OmniTopNWithExprOperatorTest {
    @Test
    public void testTopNWithAllExpr() {
        DataType[] sourceTypes = {IntDataType.INTEGER, LongDataType.LONG, LongDataType.LONG};
        String[] sortKeys = {
                omniJsonFourArithmeticExpr("ADD", 1, getOmniJsonFieldReference(1, 0), getOmniJsonLiteral(1, false, 5)),
                omniJsonFourArithmeticExpr("MODULUS", 2, getOmniJsonFieldReference(2, 2),
                        getOmniJsonLiteral(2, false, 3))};
        int[] sortAsc = {0, 1};
        int[] nullFirst = {0, 0};

        int expectedRowSize = 5;

        OmniTopNWithExprOperatorFactory omniTopNOperatorFactory = new OmniTopNWithExprOperatorFactory(sourceTypes,
                expectedRowSize, sortKeys, sortAsc, nullFirst);
        OmniOperator operator = omniTopNOperatorFactory.createOperator();

        Object[][] sourceDatas = {{5, 8, 8, 6, 8, 4, 13, 15}, {2L, 5L, 3L, 11L, 4L, 3L, 0L, 23L},
                {5L, 3L, 2L, 6L, 1L, 4L, 7L, 8L}};
        VecBatch vecBatch = createVecBatch(sourceTypes, sourceDatas);

        operator.addInput(vecBatch);
        Iterator<VecBatch> output = operator.getOutput();

        assertEquals(output.hasNext(), true);
        VecBatch resultVecBatch = output.next();
        assertEquals(output.hasNext(), false);
        assertEquals(resultVecBatch.getRowCount(), expectedRowSize);
        assertEquals(resultVecBatch.getVectorCount(), sourceTypes.length + sortKeys.length);

        Object[][] expectedDatas = {{15, 13, 8, 8, 8}, {23L, 0L, 5L, 4L, 3L}, {8L, 7L, 3L, 1L, 2L},
                {20, 18, 13, 13, 13}, {2L, 1L, 0L, 1L, 2L}};
        assertVecBatchEquals(resultVecBatch, expectedDatas);

        freeVecBatch(resultVecBatch);
        operator.close();
        omniTopNOperatorFactory.close();
    }

    @Test
    public void testTopNWithPartialExpr() {
        DataType[] sourceTypes = {IntDataType.INTEGER, LongDataType.LONG, LongDataType.LONG};
        String[] sortKeys = {getOmniJsonFieldReference(1, 0), omniJsonFourArithmeticExpr("MODULUS", 2,
                getOmniJsonFieldReference(2, 2), getOmniJsonLiteral(2, false, 3))};
        int[] sortAsc = {0, 1};
        int[] nullFirst = {0, 0};

        int expectedRowSize = 5;

        OmniTopNWithExprOperatorFactory omniTopNOperatorFactory = new OmniTopNWithExprOperatorFactory(sourceTypes,
                expectedRowSize, sortKeys, sortAsc, nullFirst);
        OmniOperator operator = omniTopNOperatorFactory.createOperator();

        Object[][] sourceDatas = {{5, 8, 8, 6, 8, 4, 13, 15}, {2L, 5L, 3L, 11L, 4L, 3L, 0L, 23L},
                {5L, 3L, 2L, 6L, 1L, 4L, 7L, 8L}};
        VecBatch vecBatch = createVecBatch(sourceTypes, sourceDatas);

        operator.addInput(vecBatch);
        Iterator<VecBatch> output = operator.getOutput();

        assertEquals(output.hasNext(), true);
        VecBatch resultVecBatch = output.next();
        assertEquals(output.hasNext(), false);
        assertEquals(resultVecBatch.getRowCount(), expectedRowSize);
        assertEquals(resultVecBatch.getVectorCount(), 4);

        Object[][] expectedDatas = {{15, 13, 8, 8, 8}, {23L, 0L, 5L, 4L, 3L}, {8L, 7L, 3L, 1L, 2L},
                {2L, 1L, 0L, 1L, 2L}};
        assertVecBatchEquals(resultVecBatch, expectedDatas);

        freeVecBatch(resultVecBatch);
        operator.close();
        omniTopNOperatorFactory.close();
    }

    @Test
    public void testTopNWithNoExpr() {
        DataType[] sourceTypes = {IntDataType.INTEGER, LongDataType.LONG, LongDataType.LONG};
        String[] sortKeys = {getOmniJsonFieldReference(1, 0), getOmniJsonFieldReference(2, 2)};
        int[] sortAsc = {0, 1};
        int[] nullFirst = {0, 0};

        int expectedRowSize = 5;

        OmniTopNWithExprOperatorFactory omniTopNOperatorFactory = new OmniTopNWithExprOperatorFactory(sourceTypes,
                expectedRowSize, sortKeys, sortAsc, nullFirst);
        OmniOperator operator = omniTopNOperatorFactory.createOperator();

        Object[][] sourceDatas = {{5, 8, 8, 6, 8, 4, 13, 15}, {2L, 5L, 3L, 11L, 4L, 3L, 0L, 23L},
                {5L, 3L, 2L, 6L, 1L, 4L, 7L, 8L}};
        VecBatch vecBatch = createVecBatch(sourceTypes, sourceDatas);

        operator.addInput(vecBatch);
        Iterator<VecBatch> output = operator.getOutput();

        assertEquals(output.hasNext(), true);
        VecBatch resultVecBatch = output.next();
        assertEquals(output.hasNext(), false);
        assertEquals(resultVecBatch.getRowCount(), expectedRowSize);
        assertEquals(resultVecBatch.getVectorCount(), 3);

        Object[][] expectedDatas = {{15, 13, 8, 8, 8}, {23L, 0L, 4L, 3L, 5L}, {8L, 7L, 1L, 2L, 3L}};
        assertVecBatchEquals(resultVecBatch, expectedDatas);

        freeVecBatch(resultVecBatch);
        operator.close();
        omniTopNOperatorFactory.close();
    }

    @Test(expectedExceptions = OmniRuntimeException.class, expectedExceptionsMessageRegExp = ".*EXPRESSION_NOT_SUPPORT.*")
    public void testTopNWithInvalidKeys() {
        DataType[] sourceTypes = {IntDataType.INTEGER, LongDataType.LONG, LongDataType.LONG};
        String[] sortKeys = {omniFunctionExpr("abc", 2, getOmniJsonFieldReference(2, 1))};
        int[] sortAsc = {0};
        int[] nullFirst = {0};
        int expectedRowSize = 5;

        OmniTopNWithExprOperatorFactory omniTopNOperatorFactory = new OmniTopNWithExprOperatorFactory(sourceTypes,
                expectedRowSize, sortKeys, sortAsc, nullFirst);
    }

    @Test
    public void testFactoryContextEquals() {
        DataType[] sourceTypes = {IntDataType.INTEGER, LongDataType.LONG, LongDataType.LONG};
        String[] sortKeys = {getOmniJsonFieldReference(1, 0), getOmniJsonFieldReference(2, 2)};
        int[] sortAsc = {0, 1};
        int[] nullFirst = {0, 0};
        int expectedRowSize = 5;
        FactoryContext factory1 = new FactoryContext(sourceTypes, expectedRowSize, 0, sortKeys, sortAsc, nullFirst,
                new OperatorConfig());
        FactoryContext factory2 = new FactoryContext(sourceTypes, expectedRowSize, 0, sortKeys, sortAsc, nullFirst,
                new OperatorConfig());
        FactoryContext factory3 = null;
        assertEquals(factory2, factory1);
        assertEquals(factory1, factory1);
        assertNotEquals(factory3, factory1);
    }

    private void buildIterativeExpectedData(Object[][] expectedData1, Object[][] expectedData2, int sourceDataSize,
            int maxRowCount, int expectedRowSize) {
        for (int i = 0; i < maxRowCount; i++) {
            if (i % 2 == 0) {
                expectedData1[0][i] = sourceDataSize - 1L - i;
                expectedData1[1][i] = sourceDataSize - 1L - i;
            } else {
                expectedData1[0][i] = expectedData1[0][i - 1];
                expectedData1[1][i] = (long) expectedData1[1][i - 1] + 1L;
            }
            expectedData1[2][i] = (long) expectedData1[0][i] + 5L;
            expectedData1[3][i] = (long) expectedData1[1][i] + 2L;
        }

        for (int i = 0; i < expectedRowSize - maxRowCount; i++) {
            if (i % 2 == 0) {
                expectedData2[0][i] = sourceDataSize - maxRowCount - 1L - i;
                expectedData2[1][i] = sourceDataSize - maxRowCount - 1L - i;
            } else {
                expectedData2[0][i] = expectedData2[0][i - 1];
                expectedData2[1][i] = (long) expectedData2[1][i - 1] + 1L;
            }
            expectedData2[2][i] = (long) expectedData2[0][i] + 5L;
            expectedData2[3][i] = (long) expectedData2[1][i] + 2L;
        }
    }

    @Test
    public void testTopNWithExprAndOffset() {
        DataType[] sourceTypes = {IntDataType.INTEGER, LongDataType.LONG, LongDataType.LONG};
        String[] sortKeys = {getOmniJsonFieldReference(1, 0), getOmniJsonFieldReference(2, 2)};
        int[] sortAsc = {0, 1};
        int[] nullFirst = {0, 0};

        int limitSize = 5;
        int offsetSize = 2;
        int expectedRowSize = limitSize - offsetSize;

        OmniTopNWithExprOperatorFactory omniTopNOperatorFactory = new OmniTopNWithExprOperatorFactory(sourceTypes,
            limitSize, offsetSize, sortKeys, sortAsc, nullFirst);
        OmniOperator operator = omniTopNOperatorFactory.createOperator();

        List<List<Object>> sourceDatas = Arrays.asList(Arrays.asList(5, 8, 8, 6, 8, 4, 13, 15),
            Arrays.asList(2L, 5L, 3L, 11L, 4L, 3L, 0L, 23L),
            Arrays.asList(5L, 3L, 2L, 6L, 1L, 4L, 7L, 8L));
        VecBatch vecBatch = createVecBatch(sourceTypes, sourceDatas);

        operator.addInput(vecBatch);
        Iterator<VecBatch> output = operator.getOutput();

        assertEquals(output.hasNext(), true);
        VecBatch resultVecBatch = output.next();
        assertEquals(output.hasNext(), false);
        assertEquals(resultVecBatch.getRowCount(), expectedRowSize);
        assertEquals(resultVecBatch.getVectorCount(), 3);

        List<List<Object>> expectedDatas = new ArrayList<>();
        expectedDatas.add(Arrays.asList(8, 8, 8));
        expectedDatas.add(Arrays.asList(4L, 3L, 5L));
        expectedDatas.add(Arrays.asList(1L, 2L, 3L));

        VecBatch expectedVecBatch = createVecBatch(sourceTypes, expectedDatas);
        assertVecBatchEquals(resultVecBatch, expectedVecBatch);

        freeVecBatch(resultVecBatch);
        operator.close();
        omniTopNOperatorFactory.close();
    }

    @Test
    public void testTopNIterativeGetOutput() {
        DataType[] sourceTypes = {LongDataType.LONG, LongDataType.LONG};
        String[] sortKeys = {
                omniJsonFourArithmeticExpr("ADD", 2, getOmniJsonFieldReference(2, 0), getOmniJsonLiteral(2, false, 5)),
                omniJsonFourArithmeticExpr("ADD", 2, getOmniJsonFieldReference(2, 1), getOmniJsonLiteral(2, false, 2))};
        int[] sortAsc = {0, 1};
        int[] nullFirst = {0, 0};

        int sourceDataSize = 33000;
        int expectedRowSize = 32800;

        OmniTopNWithExprOperatorFactory omniTopNOperatorFactory = new OmniTopNWithExprOperatorFactory(sourceTypes,
                expectedRowSize, sortKeys, sortAsc, nullFirst);
        OmniOperator operator = omniTopNOperatorFactory.createOperator();

        Object[][] sourceData = new Object[2][sourceDataSize];
        for (int i = 0; i < sourceDataSize; i++) {
            if (i % 2 == 0) {
                sourceData[0][i] = i + 1L;
            } else {
                sourceData[0][i] = sourceData[0][i - 1];
            }
            sourceData[1][i] = i + 1L;
        }
        VecBatch vecBatch = createVecBatch(sourceTypes, sourceData);

        operator.addInput(vecBatch);
        Iterator<VecBatch> topNIterator = operator.getOutput();
        List<VecBatch> resultList = new ArrayList<>();
        while (topNIterator.hasNext()) {
            resultList.add(topNIterator.next());
        }

        int resultRowCount = 0;
        int resultVectorCount = 0;
        for (int i = 0; i < resultList.size(); i++) {
            resultRowCount += resultList.get(i).getRowCount();
            resultVectorCount = resultList.get(i).getVectorCount();
        }
        assertEquals(resultRowCount, expectedRowSize);
        assertEquals(resultVectorCount, sourceTypes.length + sortKeys.length);

        int maxRowCount = 32768; // 1M / (4 * 8)
        Object[][] expectedData1 = new Object[resultVectorCount][maxRowCount];
        Object[][] expectedData2 = new Object[resultVectorCount][expectedRowSize - maxRowCount];
        buildIterativeExpectedData(expectedData1, expectedData2, sourceDataSize, maxRowCount, expectedRowSize);

        assertVecBatchEquals(resultList.get(0), expectedData1);
        assertVecBatchEquals(resultList.get(1), expectedData2);

        freeVecBatches(resultList);
        operator.close();
        omniTopNOperatorFactory.close();
    }
}
