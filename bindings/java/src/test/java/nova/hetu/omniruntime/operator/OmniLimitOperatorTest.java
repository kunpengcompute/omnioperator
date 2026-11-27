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
import static nova.hetu.omniruntime.util.TestUtils.freeVecBatches;
import static org.testng.Assert.assertEquals;
import static org.testng.Assert.assertNotEquals;

import nova.hetu.omniruntime.operator.limit.OmniLimitOperatorFactory;
import nova.hetu.omniruntime.operator.limit.OmniLimitOperatorFactory.FactoryContext;
import nova.hetu.omniruntime.type.DataType;
import nova.hetu.omniruntime.type.DoubleDataType;
import nova.hetu.omniruntime.type.IntDataType;
import nova.hetu.omniruntime.type.LongDataType;
import nova.hetu.omniruntime.type.VarcharDataType;
import nova.hetu.omniruntime.vector.Vec;
import nova.hetu.omniruntime.vector.VecBatch;

import org.testng.annotations.Test;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.Arrays;

/**
 * The type Omni limit operator test.
 *
 * @since 2021-11-27
 */
public class OmniLimitOperatorTest {
    @Test
    public void testLimitByTwoColum() {
        DataType[] sourceTypes = {IntDataType.INTEGER, DoubleDataType.DOUBLE};
        Object[][] sourceDatas1 = {{0, 1, 2, 0, 1, 2}, {6.6, 5.5, 4.4, 3.3, 2.2, 1.1}};
        VecBatch vecBatch1 = createVecBatch(sourceTypes, sourceDatas1);

        OmniLimitOperatorFactory limitOperatorFactory = new OmniLimitOperatorFactory(4);
        OmniOperator limitOperator = limitOperatorFactory.createOperator();
        limitOperator.addInput(vecBatch1);
        Iterator<VecBatch> results = limitOperator.getOutput();

        Object[][] expectedDatas1 = {{0, 1, 2, 0}, {6.6, 5.5, 4.4, 3.3}};

        VecBatch resultVecBatch1 = results.next();
        assertVecBatchEquals(resultVecBatch1, expectedDatas1);

        resultVecBatch1.releaseAllVectors();
        resultVecBatch1.close();
        limitOperator.close();
        limitOperatorFactory.close();
    }

    @Test
    public void testLimitWithNull() {
        DataType[] sourceTypes = {IntDataType.INTEGER, DoubleDataType.DOUBLE};
        Object[][] sourceDatas1 = {{0, 1, 2, 3, 4, 5}, {6.6, 5.5, 4.4, 3.3, 2.2, 1.1}};
        VecBatch vecBatch1 = createVecBatch(sourceTypes, sourceDatas1);
        Vec[] inVectors = vecBatch1.getVectors();
        inVectors[0].setNull(2);
        inVectors[0].setNull(3);
        inVectors[1].setNull(3);
        inVectors[1].setNull(4);

        OmniLimitOperatorFactory limitOperatorFactory = new OmniLimitOperatorFactory(6);
        OmniOperator limitOperator = limitOperatorFactory.createOperator();
        limitOperator.addInput(vecBatch1);
        Iterator<VecBatch> results = limitOperator.getOutput();

        Object[][] expectedDatas1 = {{0, 1, null, null, 4, 5}, {6.6, 5.5, 4.4, null, null, 1.1}};

        VecBatch resultVecBatch1 = results.next();
        assertVecBatchEquals(resultVecBatch1, expectedDatas1);

        resultVecBatch1.releaseAllVectors();
        resultVecBatch1.close();
        limitOperator.close();
        limitOperatorFactory.close();
    }

    @Test
    public void testFactoryContextEquals() {
        FactoryContext factory1 = new FactoryContext(6, 0);
        FactoryContext factory2 = new FactoryContext(6, 0);
        FactoryContext factory3 = null;

        assertEquals(factory2, factory1);
        assertEquals(factory1, factory1);
        assertNotEquals(factory3, factory1);
    }

    private static void buildLimitExpectData(Object[][] expectedData1, Object[][] expectedData2,
            int maxRowCount) {
        for (int i = 0; i < expectedData1[0].length; i++) {
            expectedData1[0][i] = i;
            expectedData1[1][i] = i + 1;
            expectedData1[2][i] = i + 2L;
            expectedData1[3][i] = "abc" + i;
        }

        for (int i = 0; i < expectedData2[0].length; i++) {
            expectedData2[0][i] = i + maxRowCount;
            expectedData2[1][i] = i + maxRowCount + 1;
            expectedData2[2][i] = i + maxRowCount + 2L;
            expectedData2[3][i] = "abc" + (i + maxRowCount);
        }
    }

    private static void buildLimitExpectData2(List<List<Object>> expectedData1, List<List<Object>> expectedData2,
            int dataSize1, int dataSize2, int offset) {
        for (int i = 0; i < dataSize1; i++) {
            expectedData1.get(0).add(offset + i);
            expectedData1.get(1).add(offset + i + 1);
            expectedData1.get(2).add(offset + i + 2L);
            expectedData1.get(3).add("abc" + (offset + i));
        }
        for (int i = 0; i < dataSize2; i++) {
            expectedData2.get(0).add(i + 32768);
            expectedData2.get(1).add(i + 32768 + 1);
            expectedData2.get(2).add(i + 32768 + 2L);
            expectedData2.get(3).add("abc" + (i + 32768));
        }
    }

    @Test
    public void testLimitMultiBatchGetOutput() {
        int dataSize = 32800;
        int maxRowCount = 32768; // 1M / (4 + 4 + 8 + 8)
        DataType[] sourceTypes = {IntDataType.INTEGER, IntDataType.INTEGER, LongDataType.LONG, new VarcharDataType(8)};
        Object[][] sourceData1 = new Object[sourceTypes.length][maxRowCount];
        Object[][] sourceData2 = new Object[sourceTypes.length][dataSize - maxRowCount];

        for (int i = 0; i < maxRowCount; i++) {
            sourceData1[0][i] = i;
            sourceData1[1][i] = i + 1;
            sourceData1[2][i] = i + 2L;
            sourceData1[3][i] = "abc" + i;
        }
        for (int i = 0; i < dataSize - maxRowCount; i++) {
            sourceData2[0][i] = i + maxRowCount;
            sourceData2[1][i] = i + maxRowCount + 1;
            sourceData2[2][i] = i + maxRowCount + 2L;
            sourceData2[3][i] = "abc" + (i + maxRowCount);
        }

        VecBatch vecBatch1 = createVecBatch(sourceTypes, sourceData1);
        VecBatch vecBatch2 = createVecBatch(sourceTypes, sourceData2);

        int limitSize = 32780;
        OmniLimitOperatorFactory limitOperatorFactory = new OmniLimitOperatorFactory(limitSize);
        OmniOperator limitOperator = limitOperatorFactory.createOperator();
        limitOperator.addInput(vecBatch1);

        List<VecBatch> resultList = new ArrayList<>();
        Iterator<VecBatch> limitIterator = limitOperator.getOutput();
        resultList.add(limitIterator.next());
        limitOperator.addInput(vecBatch2);
        while (limitIterator.hasNext()) {
            resultList.add(limitIterator.next());
        }
        assertEquals(resultList.size(), 2);

        Object[][] expectedData1 = new Object[sourceTypes.length][maxRowCount];
        Object[][] expectedData2 = new Object[sourceTypes.length][limitSize - maxRowCount];

        buildLimitExpectData(expectedData1, expectedData2, maxRowCount);
        assertVecBatchEquals(resultList.get(0), expectedData1);
        assertVecBatchEquals(resultList.get(1), expectedData2);

        freeVecBatches(resultList);
        limitOperator.close();
        limitOperatorFactory.close();
    }

    @Test
    public void testOnlyOffsetByTwoColumn1() {
        DataType[] sourceTypes = {IntDataType.INTEGER, DoubleDataType.DOUBLE};
        List<List<Object>> sourceDatas1 = Arrays.asList(Arrays.asList(0, 1, 2, 0, 1, 2),
            Arrays.asList(1.1, 1.2, 1.3, 2.1, 2.2, 2.3));
        VecBatch vecBatch1 = createVecBatch(sourceTypes, sourceDatas1);

        OmniLimitOperatorFactory limitOperatorFactory = new OmniLimitOperatorFactory(-1, 2);
        OmniOperator limitOperator = limitOperatorFactory.createOperator();
        limitOperator.addInput(vecBatch1);
        Iterator<VecBatch> results = limitOperator.getOutput();

        List<List<Object>> expectedDatas1 = Arrays.asList(Arrays.asList(2, 0, 1, 2),
            Arrays.asList(1.3, 2.1, 2.2, 2.3));
        VecBatch expectedVecBatch1 = createVecBatch(sourceTypes, expectedDatas1);

        VecBatch resultVecBatch1 = results.next();
        assertVecBatchEquals(resultVecBatch1, expectedVecBatch1);

        resultVecBatch1.releaseAllVectors();
        resultVecBatch1.close();
        limitOperator.close();
        limitOperatorFactory.close();
    }

    @Test
    public void testLimitOffsetByTwoColumn() {
        DataType[] sourceTypes = {IntDataType.INTEGER, DoubleDataType.DOUBLE};
        List<List<Object>> sourceDatas1 = Arrays.asList(Arrays.asList(0, 1, 2, 0, 1, 2),
            Arrays.asList(1.1, 1.2, 1.3, 2.1, 2.2, 2.3));
        VecBatch vecBatch1 = createVecBatch(sourceTypes, sourceDatas1);

        OmniLimitOperatorFactory limitOperatorFactory = new OmniLimitOperatorFactory(4, 2);
        OmniOperator limitOperator = limitOperatorFactory.createOperator();
        limitOperator.addInput(vecBatch1);
        Iterator<VecBatch> results = limitOperator.getOutput();

        List<List<Object>> expectedDatas1 = Arrays.asList(Arrays.asList(2, 0),
            Arrays.asList(1.3, 2.1));
        VecBatch expectedVecBatch1 = createVecBatch(sourceTypes, expectedDatas1);

        VecBatch resultVecBatch1 = results.next();
        assertVecBatchEquals(resultVecBatch1, expectedVecBatch1);

        resultVecBatch1.releaseAllVectors();
        resultVecBatch1.close();
        limitOperator.close();
        limitOperatorFactory.close();
    }

    @Test
    public void testLimitOffsetMultiBatchGetOutput() {
        int offset = 20;
        int dataSize = 32800;
        int maxRowCount = 32768; // 1M / (4 + 4 + 8 + 8)
        DataType[] sourceTypes = {IntDataType.INTEGER, IntDataType.INTEGER, LongDataType.LONG, new VarcharDataType(8)};
        List<List<Object>> sourceData1 = new ArrayList<>();
        List<List<Object>> sourceData2 = new ArrayList<>();

        for (int i = 0; i < sourceTypes.length; i++) {
            sourceData1.add(new ArrayList<>());
            sourceData2.add(new ArrayList<>());
        }

        buildLimitExpectData2(sourceData1, sourceData2, maxRowCount, dataSize - maxRowCount, 0);
        VecBatch vecBatch1 = createVecBatch(sourceTypes, sourceData1);
        VecBatch vecBatch2 = createVecBatch(sourceTypes, sourceData2);

        int limitSize = 32780;
        OmniLimitOperatorFactory limitOperatorFactory = new OmniLimitOperatorFactory(limitSize, offset);
        OmniOperator limitOperator = limitOperatorFactory.createOperator();
        limitOperator.addInput(vecBatch1);

        List<VecBatch> resultList = new ArrayList<>();
        Iterator<VecBatch> limitIterator = limitOperator.getOutput();
        resultList.add(limitIterator.next());
        limitOperator.addInput(vecBatch2);
        while (limitIterator.hasNext()) {
            resultList.add(limitIterator.next());
        }
        assertEquals(resultList.size(), 2);

        List<List<Object>> expectedData1 = new ArrayList<>();
        List<List<Object>> expectedData2 = new ArrayList<>();

        for (int i = 0; i < sourceTypes.length; i++) {
            expectedData1.add(new ArrayList<>());
            expectedData2.add(new ArrayList<>());
        }

        buildLimitExpectData2(expectedData1, expectedData2, maxRowCount - offset, limitSize - maxRowCount, offset);
        VecBatch expectedVecBatch1 = createVecBatch(sourceTypes, expectedData1);
        VecBatch expectedVecBatch2 = createVecBatch(sourceTypes, expectedData2);
        assertVecBatchEquals(resultList.get(0), expectedVecBatch1);
        assertVecBatchEquals(resultList.get(1), expectedVecBatch2);

        freeVecBatches(resultList);
        limitOperator.close();
        limitOperatorFactory.close();
    }
}
