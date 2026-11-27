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
import static org.testng.Assert.assertEquals;
import static org.testng.Assert.assertNotEquals;

import nova.hetu.omniruntime.operator.config.OperatorConfig;
import nova.hetu.omniruntime.operator.window.OmniWindowGroupLimitWithExprOperatorFactory;
import nova.hetu.omniruntime.operator.window.OmniWindowGroupLimitWithExprOperatorFactory.FactoryContext;
import nova.hetu.omniruntime.type.DataType;
import nova.hetu.omniruntime.type.IntDataType;
import nova.hetu.omniruntime.type.LongDataType;
import nova.hetu.omniruntime.type.VarcharDataType;
import nova.hetu.omniruntime.vector.VecBatch;

import org.testng.annotations.Test;

import java.util.Iterator;

/**
 * The omni WindowGroupLimit with expression operator test.
 *
 * @since 2025-1-23
 */
public class OmniWindowGroupLimitWithExprOperatorTest {
    // rank + Desc + NullLast
    @Test
    public void testWindowGroupLimitRankDescNullLast() {
        DataType[] sourceTypes = {new VarcharDataType(10), LongDataType.LONG, LongDataType.LONG};
        Object[][] sourceDatas = {{"hi", "hi", "hi", "bye", "bye", "bye", "bye", "bye"},
                {2L, 5L, 3L, 11L, 4L, 3L, 0L, 23L}, {3L, 5L, 8L, 3L, 5L, 3L, 4L, 3L}};
        VecBatch vecBatch = createVecBatch(sourceTypes, sourceDatas);

        String[] partitionKeys = {getOmniJsonFieldReference(15, 0)};
        String[] sortKeys = {getOmniJsonFieldReference(2, 2)};
        int[] ascendings = {0};
        int[] nullFirsts = {0};
        OmniWindowGroupLimitWithExprOperatorFactory windowGroupLimitOperatorFactory =
        new OmniWindowGroupLimitWithExprOperatorFactory(sourceTypes, 3, "rank", partitionKeys,
            sortKeys, ascendings, nullFirsts);
        OmniOperator windowGroupLimitOperator = windowGroupLimitOperatorFactory.createOperator();
        windowGroupLimitOperator.addInput(vecBatch);
        Iterator<VecBatch> results = windowGroupLimitOperator.getOutput();

        VecBatch resultVecBatch = results.next();
        assertEquals(resultVecBatch.getRowCount(), sourceDatas[0].length);
        Object[][] expectedDatas = {{"bye", "bye", "bye", "bye", "bye", "hi", "hi", "hi"},
                {4L, 0L, 11L, 3L, 23L, 3L, 5L, 2L}, {5L, 4L, 3L, 3L, 3L, 8L, 5L, 3L}};
        assertVecBatchEquals(resultVecBatch, expectedDatas);

        freeVecBatch(resultVecBatch);
        windowGroupLimitOperator.close();
        windowGroupLimitOperatorFactory.close();
    }

    // rank + Asc + NullLast
    @Test
    public void testWindowGroupLimitRankAscNullLast() {
        DataType[] sourceTypes = {new VarcharDataType(10), LongDataType.LONG, LongDataType.LONG};
        Object[][] sourceDatas = {{"hi", "hi", "hi", "bye", "bye", "bye", "bye", "bye"},
                {2L, 5L, 3L, 11L, 4L, 3L, 0L, 23L}, {5L, 3L, 8L, 3L, 6L, 6L, 4L, 6L}};
        VecBatch vecBatch = createVecBatch(sourceTypes, sourceDatas);

        String[] partitionKeys = {getOmniJsonFieldReference(15, 0)};
        String[] sortKeys = {getOmniJsonFieldReference(2, 2)};
        int[] ascendings = {1};
        int[] nullFirsts = {0};
        OmniWindowGroupLimitWithExprOperatorFactory windowGroupLimitOperatorFactory =
        new OmniWindowGroupLimitWithExprOperatorFactory(sourceTypes, 3, "rank", partitionKeys,
            sortKeys, ascendings, nullFirsts);
        OmniOperator windowGroupLimitOperator = windowGroupLimitOperatorFactory.createOperator();
        windowGroupLimitOperator.addInput(vecBatch);
        Iterator<VecBatch> results = windowGroupLimitOperator.getOutput();

        VecBatch resultVecBatch = results.next();
        assertEquals(resultVecBatch.getRowCount(), sourceDatas[0].length);
        Object[][] expectedDatas = {{"bye", "bye", "bye", "bye", "bye", "hi", "hi", "hi"},
                {11L, 0L, 4L, 3L, 23L, 5L, 2L, 3L}, {3L, 4L, 6L, 6L, 6L, 3L, 5L, 8L}};
        assertVecBatchEquals(resultVecBatch, expectedDatas);

        freeVecBatch(resultVecBatch);
        windowGroupLimitOperator.close();
        windowGroupLimitOperatorFactory.close();
    }

    // Row_number + Desc + NullLast
    @Test
    public void testWindowGroupLimitRowNumberDescNullLast() {
        DataType[] sourceTypes = {new VarcharDataType(10), LongDataType.LONG, LongDataType.LONG};
        Object[][] sourceDatas = {{"hi", "hi", "hi", "bye", "bye", "bye", "bye", "bye"},
                {2L, 5L, 3L, 11L, 4L, 3L, 0L, 23L}, {3L, 5L, 8L, 3L, 5L, 3L, 4L, 3L}};
        VecBatch vecBatch = createVecBatch(sourceTypes, sourceDatas);

        String[] partitionKeys = {getOmniJsonFieldReference(15, 0)};
        String[] sortKeys = {getOmniJsonFieldReference(2, 2)};
        int[] ascendings = {0};
        int[] nullFirsts = {0};
        OmniWindowGroupLimitWithExprOperatorFactory windowGroupLimitOperatorFactory =
        new OmniWindowGroupLimitWithExprOperatorFactory(sourceTypes, 3, "row_number", partitionKeys,
            sortKeys, ascendings, nullFirsts);
        OmniOperator windowGroupLimitOperator = windowGroupLimitOperatorFactory.createOperator();
        windowGroupLimitOperator.addInput(vecBatch);
        Iterator<VecBatch> results = windowGroupLimitOperator.getOutput();

        VecBatch resultVecBatch = results.next();
        assertEquals(resultVecBatch.getRowCount(), 6);
        Object[][] expectedDatas = {{"bye", "bye", "bye", "hi", "hi", "hi"}, {4L, 0L, 11L, 3L, 5L, 2L},
                {5L, 4L, 3L, 8L, 5L, 3L}};
        assertVecBatchEquals(resultVecBatch, expectedDatas);

        freeVecBatch(resultVecBatch);
        windowGroupLimitOperator.close();
        windowGroupLimitOperatorFactory.close();
    }

    // Row_number + Asc + NullLast
    @Test
    public void testWindowGroupLimitRowNumberAscNullLast() {
        DataType[] sourceTypes = {new VarcharDataType(10), LongDataType.LONG, LongDataType.LONG};
        Object[][] sourceDatas = {{"hi", "hi", "hi", "bye", "bye", "bye", "bye", "bye"},
                {2L, 5L, 3L, 11L, 4L, 3L, 0L, 23L}, {5L, 3L, 8L, 3L, 6L, 6L, 4L, 6L}};
        VecBatch vecBatch = createVecBatch(sourceTypes, sourceDatas);

        String[] partitionKeys = {getOmniJsonFieldReference(15, 0)};
        String[] sortKeys = {getOmniJsonFieldReference(2, 2)};
        int[] ascendings = {1};
        int[] nullFirsts = {0};
        OmniWindowGroupLimitWithExprOperatorFactory windowGroupLimitOperatorFactory = new
        OmniWindowGroupLimitWithExprOperatorFactory(sourceTypes, 3, "row_number", partitionKeys, sortKeys,
        ascendings, nullFirsts);
        OmniOperator windowGroupLimitOperator = windowGroupLimitOperatorFactory.createOperator();
        windowGroupLimitOperator.addInput(vecBatch);
        Iterator<VecBatch> results = windowGroupLimitOperator.getOutput();

        VecBatch resultVecBatch = results.next();
        assertEquals(resultVecBatch.getRowCount(), 6);
        Object[][] expectedDatas = {{"bye", "bye", "bye", "hi", "hi", "hi"}, {11L, 0L, 4L, 5L, 2L, 3L},
                {3L, 4L, 6L, 3L, 5L, 8L}};
        assertVecBatchEquals(resultVecBatch, expectedDatas);

        freeVecBatch(resultVecBatch);
        windowGroupLimitOperator.close();
        windowGroupLimitOperatorFactory.close();
    }

    @Test
    public void testFactoryContextEquals() {
        DataType[] sourceTypes = {IntDataType.INTEGER, LongDataType.LONG};
        String[] partitionKeys = {getOmniJsonFieldReference(1, 0)};
        String[] sortKeys = {getOmniJsonFieldReference(2, 1)};
        int[] sortAscendings = {1};
        int[] sortNullFirsts = {0};
        OperatorConfig operatorConfig = new OperatorConfig();
        FactoryContext factory1 = new FactoryContext(sourceTypes, 10, "rank", partitionKeys, sortKeys, sortAscendings,
                sortNullFirsts, operatorConfig);
        FactoryContext factory2 = new FactoryContext(sourceTypes, 10, "rank", partitionKeys, sortKeys, sortAscendings,
                sortNullFirsts, operatorConfig);
        FactoryContext factory3 = null;
        assertEquals(factory2, factory1);
        assertEquals(factory1, factory1);
        assertNotEquals(factory3, factory1);
        FactoryContext factory4 = new FactoryContext(sourceTypes, 10, "row_number", partitionKeys, sortKeys,
                sortAscendings, sortNullFirsts, operatorConfig);
        FactoryContext factory5 = new FactoryContext(sourceTypes, 10, "row_number", partitionKeys, sortKeys,
                sortAscendings, sortNullFirsts, operatorConfig);
        assertEquals(factory4, factory5);
        assertEquals(factory4, factory4);
        assertNotEquals(factory4, factory3);
        assertNotEquals(factory4, factory2);
    }
}
