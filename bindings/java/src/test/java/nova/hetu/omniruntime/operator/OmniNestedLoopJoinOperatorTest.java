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
import static nova.hetu.omniruntime.constants.JoinType.OMNI_JOIN_TYPE_LEFT;
import static nova.hetu.omniruntime.constants.JoinType.OMNI_JOIN_TYPE_RIGHT;
import static nova.hetu.omniruntime.type.VarcharDataType.MAX_WIDTH;
import static nova.hetu.omniruntime.util.TestUtils.assertVecBatchEquals;
import static nova.hetu.omniruntime.util.TestUtils.createDictionaryVec;
import static nova.hetu.omniruntime.util.TestUtils.createIntVec;
import static nova.hetu.omniruntime.util.TestUtils.createVecBatch;
import static org.testng.Assert.assertEquals;
import static org.testng.Assert.assertNotEquals;
import static org.testng.Assert.assertTrue;

import nova.hetu.omniruntime.constants.JoinType;
import nova.hetu.omniruntime.operator.config.OperatorConfig;
import nova.hetu.omniruntime.operator.join.OmniNestedLoopJoinBuildOperatorFactory;
import nova.hetu.omniruntime.operator.join.OmniNestedLoopJoinLookupOperatorFactory;
import nova.hetu.omniruntime.type.DataType;
import nova.hetu.omniruntime.type.DoubleDataType;
import nova.hetu.omniruntime.type.IntDataType;
import nova.hetu.omniruntime.type.VarcharDataType;
import nova.hetu.omniruntime.util.TestUtils;
import nova.hetu.omniruntime.vector.IntVec;
import nova.hetu.omniruntime.vector.Vec;
import nova.hetu.omniruntime.vector.VecBatch;

import org.testng.annotations.Test;

import java.util.Iterator;
import java.util.Optional;

/**
 * The type Omni Nested Loop Join operators test.
 *
 * @since 2024-12-2
 */
public class OmniNestedLoopJoinOperatorTest {
    @Test
    public void testFactoryContextEquals() {
        DataType[] sourceTypes = {IntDataType.INTEGER, DoubleDataType.DOUBLE};
        int[] outputCols = new int[]{0, 1};
        int[] outputCols1 = new int[]{1, 0};

        OmniNestedLoopJoinBuildOperatorFactory.FactoryContext context1 = new OmniNestedLoopJoinBuildOperatorFactory
                .FactoryContext(sourceTypes, outputCols);
        OmniNestedLoopJoinBuildOperatorFactory.FactoryContext context2 = new OmniNestedLoopJoinBuildOperatorFactory
                .FactoryContext(sourceTypes, outputCols);
        OmniNestedLoopJoinBuildOperatorFactory.FactoryContext context3 = new OmniNestedLoopJoinBuildOperatorFactory
                .FactoryContext(sourceTypes, outputCols1);
        assertNotEquals(context1, null);
        assertNotEquals(new Object(), context1);
        assertNotEquals(context3, context1);
        assertEquals(context1, context1);
        assertEquals(context2, context1);

        OmniNestedLoopJoinBuildOperatorFactory omniNestedLoopJoinBuildOperatorFactory =
                new OmniNestedLoopJoinBuildOperatorFactory(sourceTypes, outputCols);
        OmniNestedLoopJoinLookupOperatorFactory.FactoryContext lookupContext1 =
                new OmniNestedLoopJoinLookupOperatorFactory.FactoryContext(OMNI_JOIN_TYPE_INNER, sourceTypes,
                outputCols, Optional.of("test"), omniNestedLoopJoinBuildOperatorFactory, new OperatorConfig());
        OmniNestedLoopJoinLookupOperatorFactory.FactoryContext lookupContext2 =
                new OmniNestedLoopJoinLookupOperatorFactory.FactoryContext(
                OMNI_JOIN_TYPE_INNER, sourceTypes, outputCols, Optional.of("test"),
                omniNestedLoopJoinBuildOperatorFactory, new OperatorConfig());
        OmniNestedLoopJoinLookupOperatorFactory.FactoryContext lookupContext3 =
                new OmniNestedLoopJoinLookupOperatorFactory.FactoryContext(
                OMNI_JOIN_TYPE_INNER, sourceTypes, outputCols, Optional.of("test3"),
                omniNestedLoopJoinBuildOperatorFactory, new OperatorConfig());
        OmniNestedLoopJoinLookupOperatorFactory.FactoryContext lookupContext4 =
                new OmniNestedLoopJoinLookupOperatorFactory.FactoryContext(
                OMNI_JOIN_TYPE_RIGHT, sourceTypes, outputCols, Optional.of("test"),
                omniNestedLoopJoinBuildOperatorFactory, new OperatorConfig());
        assertNotEquals(lookupContext1, null);
        assertNotEquals(new Object(), lookupContext1);
        assertNotEquals(lookupContext3, lookupContext1);
        assertNotEquals(lookupContext4, lookupContext1);
        assertEquals(lookupContext1, lookupContext1);
        assertEquals(lookupContext2, lookupContext1);
    }

    @Test
    public void testRightOutJoin() {
        DataType[] sourceTypes = {new VarcharDataType(20), new VarcharDataType(20), IntDataType.INTEGER,
                DoubleDataType.DOUBLE};
        int[] buildOutputCols = new int[]{0, 1, 2, 3};
        Object[][] sourceDatas1 = {{"abc", "yeah", "", "add"}, {"", "yeah", "Hello", "World"}, {4, 10, 1, 8},
                {2.0, 8.0, 1.0, 3.0}};
        VecBatch vecBatch = createVecBatch(sourceTypes, sourceDatas1);

        OmniNestedLoopJoinBuildOperatorFactory omniNestedLoopJoinBuildOperatorFactory =
                new OmniNestedLoopJoinBuildOperatorFactory(sourceTypes, buildOutputCols);
        OmniOperator omniOperator = omniNestedLoopJoinBuildOperatorFactory.createOperator();
        omniOperator.addInput(vecBatch);
        omniOperator.getOutput();

        DataType[] probeTypes = {new VarcharDataType(20), new VarcharDataType(20), IntDataType.INTEGER,
                DoubleDataType.DOUBLE};
        int[] probeOutputCols = new int[]{0, 1, 2, 3};

        String fieldExpr1 = TestUtils.getOmniJsonFieldReference(1, 2);
        String fieldExpr2 = TestUtils.getOmniJsonFieldReference(1, 6);
        Optional<String> filter = Optional.of(TestUtils.omniJsonGreaterThanExpr(fieldExpr1, fieldExpr2));

        OmniNestedLoopJoinLookupOperatorFactory omniNestedLoopJoinLookupOperatorFactory =
                new OmniNestedLoopJoinLookupOperatorFactory(OMNI_JOIN_TYPE_RIGHT, probeTypes, probeOutputCols, filter,
                omniNestedLoopJoinBuildOperatorFactory, new OperatorConfig());
        OmniOperator lookUpOperator = omniNestedLoopJoinLookupOperatorFactory.createOperator();
        Object[][] sourceDatas2 = {{"abc", "", "yeah", "add"}, {"", "Hello", "yeah", "World"}, {4, 2, 0, 1},
                {1.0, 2.0, 4.0, 3.0}};
        VecBatch vecBatch3 = createVecBatch(probeTypes, sourceDatas2);
        lookUpOperator.addInput(vecBatch3);
        Iterator<VecBatch> results = lookUpOperator.getOutput();

        Object[][] expectedDatas1 = {{"abc", "", "yeah", "add"}, {"", "Hello", "yeah", "World"}, {4, 2, 0, 1},
                {1.0, 2.0, 4.0, 3.0}, {"", "", null, null}, {"Hello", "Hello", null, null}, {1, 1, null, null},
                {1.0, 1.0, null, null}};
        VecBatch resultVecBatch1 = results.next();
        assertVecBatchEquals(resultVecBatch1, expectedDatas1);
        resultVecBatch1.releaseAllVectors();
        resultVecBatch1.close();
        lookUpOperator.close();
        omniNestedLoopJoinLookupOperatorFactory.close();
        omniOperator.close();
        omniNestedLoopJoinBuildOperatorFactory.close();
    }

    @Test
    public void testCrossJoin() {
        DataType[] sourceTypes = {IntDataType.INTEGER, DoubleDataType.DOUBLE};
        int[] buildOutputCols = new int[]{0, 1};
        Object[][] sourceDatas1 = {{0, 1, 2}, {6.6, 5.5, 4.4}};
        VecBatch vecBatch1 = createVecBatch(sourceTypes, sourceDatas1);
        VecBatch vecBatch2 = createVecBatch(sourceTypes, sourceDatas1);

        OmniNestedLoopJoinBuildOperatorFactory omniNestedLoopJoinBuildOperatorFactory =
                new OmniNestedLoopJoinBuildOperatorFactory(sourceTypes, buildOutputCols);
        OmniOperator omniOperator = omniNestedLoopJoinBuildOperatorFactory.createOperator();
        omniOperator.addInput(vecBatch1);
        omniOperator.addInput(vecBatch2);
        omniOperator.getOutput();

        DataType[] probeTypes = {IntDataType.INTEGER, DoubleDataType.DOUBLE};
        int[] probeOutputCols = new int[]{0};
        Optional<String> filter = Optional.empty();

        OmniNestedLoopJoinLookupOperatorFactory omniNestedLoopJoinLookupOperatorFactory =
                new OmniNestedLoopJoinLookupOperatorFactory(OMNI_JOIN_TYPE_INNER, probeTypes, probeOutputCols, filter,
                omniNestedLoopJoinBuildOperatorFactory, new OperatorConfig());
        OmniOperator lookUpOperator = omniNestedLoopJoinLookupOperatorFactory.createOperator();
        VecBatch vecBatch3 = createVecBatch(sourceTypes, sourceDatas1);
        lookUpOperator.addInput(vecBatch3);
        Iterator<VecBatch> results = lookUpOperator.getOutput();

        Object[][] expectedDatas1 = {{0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2},
                {0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2},
                {6.6, 5.5, 4.4, 6.6, 5.5, 4.4, 6.6, 5.5, 4.4, 6.6, 5.5, 4.4, 6.6, 5.5, 4.4, 6.6, 5.5, 4.4}};
        VecBatch resultVecBatch1 = results.next();
        assertVecBatchEquals(resultVecBatch1, expectedDatas1);
        resultVecBatch1.releaseAllVectors();
        resultVecBatch1.close();
        lookUpOperator.close();
        omniNestedLoopJoinLookupOperatorFactory.close();
        omniOperator.close();
        omniNestedLoopJoinBuildOperatorFactory.close();
    }

    @Test
    public void testInnerJoin() {
        DataType[] sourceTypes = {IntDataType.INTEGER, DoubleDataType.DOUBLE, new VarcharDataType(20)};
        int[] buildOutputCols = new int[]{0, 1, 2};
        Object[][] sourceDatas1 = {{0, 1, 2}, {6.6, 5.5, 4.4}, {"0123test", "012test", "01test"}};
        VecBatch vecBatch1 = createVecBatch(sourceTypes, sourceDatas1);
        VecBatch vecBatch2 = createVecBatch(sourceTypes, sourceDatas1);

        OmniNestedLoopJoinBuildOperatorFactory omniNestedLoopJoinBuildOperatorFactory =
                new OmniNestedLoopJoinBuildOperatorFactory(sourceTypes, buildOutputCols);
        OmniOperator omniOperator = omniNestedLoopJoinBuildOperatorFactory.createOperator();
        omniOperator.addInput(vecBatch1);
        omniOperator.addInput(vecBatch2);
        omniOperator.getOutput();

        DataType[] probeTypes = {IntDataType.INTEGER, DoubleDataType.DOUBLE};
        int[] probeOutputCols = new int[]{1};

        String lengthExpr = TestUtils.omniFunctionExpr("length", 1, TestUtils.getOmniJsonFieldReference(15, 4));
        String fieldExpr = TestUtils.getOmniJsonFieldReference(1, 0);
        Optional<String> filter = Optional.of(TestUtils.omniJsonGreaterThanExpr(fieldExpr, lengthExpr));

        OmniNestedLoopJoinLookupOperatorFactory omniNestedLoopJoinLookupOperatorFactory =
                new OmniNestedLoopJoinLookupOperatorFactory(OMNI_JOIN_TYPE_INNER, probeTypes, probeOutputCols, filter,
                omniNestedLoopJoinBuildOperatorFactory, new OperatorConfig());
        OmniOperator lookUpOperator = omniNestedLoopJoinLookupOperatorFactory.createOperator();
        Object[][] sourceDatas2 = {{9, 7, 6}, {6.6, 5.5, 4.4}};
        VecBatch vecBatch3 = createVecBatch(probeTypes, sourceDatas2);
        lookUpOperator.addInput(vecBatch3);
        Iterator<VecBatch> results = lookUpOperator.getOutput();

        Object[][] expectedDatas1 = {{6.6, 6.6, 6.6, 6.6, 6.6, 6.6, 5.5, 5.5}, {0, 1, 2, 0, 1, 2, 2, 2},
                {6.6, 5.5, 4.4, 6.6, 5.5, 4.4, 4.4, 4.4},
                {"0123test", "012test", "01test", "0123test", "012test", "01test", "01test", "01test"}};
        VecBatch resultVecBatch1 = results.next();
        assertVecBatchEquals(resultVecBatch1, expectedDatas1);
        resultVecBatch1.releaseAllVectors();
        resultVecBatch1.close();
        lookUpOperator.close();
        omniNestedLoopJoinLookupOperatorFactory.close();
        omniOperator.close();
        omniNestedLoopJoinBuildOperatorFactory.close();
    }

    @Test
    public void testInnerJoinWithLargeRowSize() {
        DataType[] sourceTypes = {IntDataType.INTEGER, DoubleDataType.DOUBLE, new VarcharDataType(MAX_WIDTH)};
        int[] buildOutputCols = new int[]{0, 1, 2};
        Object[][] sourceDatas1 = {{0, 1, 2}, {6.6, 5.5, 4.4}, {"0123test", "012test", "01test"}};
        VecBatch vecBatch1 = createVecBatch(sourceTypes, sourceDatas1);
        VecBatch vecBatch2 = createVecBatch(sourceTypes, sourceDatas1);

        OmniNestedLoopJoinBuildOperatorFactory omniNestedLoopJoinBuildOperatorFactory =
                new OmniNestedLoopJoinBuildOperatorFactory(sourceTypes, buildOutputCols);
        OmniOperator omniOperator = omniNestedLoopJoinBuildOperatorFactory.createOperator();
        omniOperator.addInput(vecBatch2);
        omniOperator.addInput(vecBatch1);
        omniOperator.getOutput();

        DataType[] probeTypes = {IntDataType.INTEGER, DoubleDataType.DOUBLE};
        int[] probeOutputCols = new int[]{0, 1};

        String fieldExpr = TestUtils.getOmniJsonFieldReference(1, 0);
        String lengthExpr = TestUtils.omniFunctionExpr("length", 1, TestUtils.getOmniJsonFieldReference(15, 4));
        Optional<String> filter = Optional.of(TestUtils.omniJsonGreaterThanExpr(fieldExpr, lengthExpr));

        OmniNestedLoopJoinLookupOperatorFactory omniNestedLoopJoinLookupOperatorFactory =
                new OmniNestedLoopJoinLookupOperatorFactory(OMNI_JOIN_TYPE_INNER, probeTypes, probeOutputCols, filter,
                omniNestedLoopJoinBuildOperatorFactory, new OperatorConfig());
        Object[][] sourceDatas2 = {{9, 7, 6}, {6.6, 5.5, 4.4}};
        OmniOperator lookUpOperator = omniNestedLoopJoinLookupOperatorFactory.createOperator();
        VecBatch vecBatch3 = createVecBatch(probeTypes, sourceDatas2);
        lookUpOperator.addInput(vecBatch3);
        Iterator<VecBatch> results = lookUpOperator.getOutput();

        Object[][] expectedDatas1 = {{9, 9, 9, 9, 9, 9}, {6.6, 6.6, 6.6, 6.6, 6.6, 6.6}, {0, 1, 2, 0, 1, 2},
                {6.6, 5.5, 4.4, 6.6, 5.5, 4.4}, {"0123test", "012test", "01test", "0123test", "012test", "01test"}};
        VecBatch resultVecBatch1 = results.next();
        assertEquals(resultVecBatch1.getRowCount(), vecBatch1.getRowCount() + vecBatch1.getRowCount());
        assertVecBatchEquals(resultVecBatch1, expectedDatas1);
        resultVecBatch1.releaseAllVectors();
        resultVecBatch1.close();
        assertTrue(results.hasNext());
        VecBatch resultVecBatch2 = results.next();
        Object[][] expectedDatas2 = {{7, 7}, {5.5, 5.5}, {2, 2}, {4.4, 4.4}, {"01test", "01test"}};
        assertVecBatchEquals(resultVecBatch2, expectedDatas2);
        resultVecBatch2.releaseAllVectors();
        resultVecBatch2.close();
        lookUpOperator.close();
        omniNestedLoopJoinLookupOperatorFactory.close();
        omniOperator.close();
        omniNestedLoopJoinBuildOperatorFactory.close();
    }

    @Test
    public void testOuterJoin() {
        DataType[] sourceTypes = {IntDataType.INTEGER, DoubleDataType.DOUBLE, new VarcharDataType(20)};
        int[] buildOutputCols = new int[]{0, 1, 2};
        Object[][] sourceDatas1 = {{0, 1, 2}, {6.6, 5.5, 4.4}, {"0123test", "012test", "01test"}};
        VecBatch vecBatch1 = createVecBatch(sourceTypes, sourceDatas1);
        VecBatch vecBatch2 = createVecBatch(sourceTypes, sourceDatas1);

        OmniNestedLoopJoinBuildOperatorFactory omniNestedLoopJoinBuildOperatorFactory =
                new OmniNestedLoopJoinBuildOperatorFactory(sourceTypes, buildOutputCols);
        OmniOperator omniOperator = omniNestedLoopJoinBuildOperatorFactory.createOperator();
        omniOperator.addInput(vecBatch2);
        omniOperator.addInput(vecBatch1);
        omniOperator.getOutput();

        outerJoin(OMNI_JOIN_TYPE_LEFT, omniNestedLoopJoinBuildOperatorFactory);

        outerJoin(OMNI_JOIN_TYPE_RIGHT, omniNestedLoopJoinBuildOperatorFactory);

        omniOperator.close();
        omniNestedLoopJoinBuildOperatorFactory.close();
    }

    @Test
    public void testOuterJoinWithDictionaryVec() {
        DataType[] sourceTypes = {IntDataType.INTEGER, DoubleDataType.DOUBLE, new VarcharDataType(20)};
        int[] buildOutputCols = new int[]{0, 1, 2};
        Object[] filed1 = {0, 1, 2};
        Object[] filed2 = {6.6, 5.5, 4.4};
        Object[] filed3 = {"0123test", "012test", "01test"};
        IntVec intVec = createIntVec(filed1);
        Vec doubleVec = createDictionaryVec(DoubleDataType.DOUBLE, filed2, new int[]{0, 1, 2});
        Vec varcharVec = createDictionaryVec(new VarcharDataType(20), filed3, new int[]{0, 1, 2});
        Vec[] vecs = {intVec, doubleVec, varcharVec};
        VecBatch vecBatch1 = new VecBatch(vecs);
        IntVec intVec2 = createIntVec(filed1);
        Vec doubleVec2 = createDictionaryVec(DoubleDataType.DOUBLE, filed2, new int[]{0, 1, 2});
        Vec varcharVec2 = createDictionaryVec(new VarcharDataType(20), filed3, new int[]{0, 1, 2});
        Vec[] vecs2 = {intVec2, doubleVec2, varcharVec2};
        VecBatch vecBatch2 = new VecBatch(vecs2);

        OmniNestedLoopJoinBuildOperatorFactory omniNestedLoopJoinBuildOperatorFactory =
                new OmniNestedLoopJoinBuildOperatorFactory(sourceTypes, buildOutputCols);
        OmniOperator omniOperator = omniNestedLoopJoinBuildOperatorFactory.createOperator();
        omniOperator.addInput(vecBatch1);
        omniOperator.addInput(vecBatch2);
        omniOperator.getOutput();

        outerJoin(OMNI_JOIN_TYPE_LEFT, omniNestedLoopJoinBuildOperatorFactory);

        outerJoin(OMNI_JOIN_TYPE_RIGHT, omniNestedLoopJoinBuildOperatorFactory);

        omniOperator.close();
        omniNestedLoopJoinBuildOperatorFactory.close();
    }

    private static void outerJoin(JoinType joinType,
                                  OmniNestedLoopJoinBuildOperatorFactory omniNestedLoopJoinBuildOperatorFactory) {
        DataType[] probeTypes = {IntDataType.INTEGER, DoubleDataType.DOUBLE};
        int[] probeOutputCols = new int[]{1};
        String lengthExpr = TestUtils.omniFunctionExpr("length", 1, TestUtils.getOmniJsonFieldReference(15, 4));
        String fieldExpr = TestUtils.getOmniJsonFieldReference(1, 0);
        Optional<String> filter = Optional.of(TestUtils.omniJsonGreaterThanExpr(fieldExpr, lengthExpr));

        OmniNestedLoopJoinLookupOperatorFactory omniNestedLoopJoinLookupOperatorFactory =
                new OmniNestedLoopJoinLookupOperatorFactory(joinType, probeTypes, probeOutputCols, filter,
                omniNestedLoopJoinBuildOperatorFactory, new OperatorConfig());
        OmniOperator lookUpOperator = omniNestedLoopJoinLookupOperatorFactory.createOperator();
        Object[][] sourceDatas2 = {{9, 7, 6}, {6.6, 5.5, 4.4}};
        VecBatch vecBatch3 = createVecBatch(probeTypes, sourceDatas2);
        lookUpOperator.addInput(vecBatch3);
        Iterator<VecBatch> results = lookUpOperator.getOutput();

        Object[][] expectedDatas1 = {{6.6, 6.6, 6.6, 6.6, 6.6, 6.6, 5.5, 5.5, 4.4}, {0, 1, 2, 0, 1, 2, 2, 2, null},
                {6.6, 5.5, 4.4, 6.6, 5.5, 4.4, 4.4, 4.4, null},
                {"0123test", "012test", "01test", "0123test", "012test", "01test", "01test", "01test", null}};
        VecBatch resultVecBatch1 = results.next();
        assertVecBatchEquals(resultVecBatch1, expectedDatas1);
        resultVecBatch1.releaseAllVectors();
        resultVecBatch1.close();
        lookUpOperator.close();
        omniNestedLoopJoinLookupOperatorFactory.close();
    }

    @Test
    public void testRightOutJoinWithShared() {
        int builderNodeId = 10;
        DataType[] sourceTypes = {new VarcharDataType(20), new VarcharDataType(20), IntDataType.INTEGER,
                DoubleDataType.DOUBLE};
        int[] buildOutputCols = new int[]{0, 1, 2, 3};
        OmniNestedLoopJoinBuildOperatorFactory omniNestedLoopJoinBuildOperatorFactory =
                new OmniNestedLoopJoinBuildOperatorFactory(sourceTypes, buildOutputCols);
        OmniOperator omniOperator = omniNestedLoopJoinBuildOperatorFactory.createOperator();
        OmniNestedLoopJoinBuildOperatorFactory tempOmniNestedLoopJoinBuildOperatorFactory =
                OmniNestedLoopJoinBuildOperatorFactory.getNestedLoopJoinBuilderOperatorFactory(builderNodeId);
        assertEquals(tempOmniNestedLoopJoinBuildOperatorFactory == null, true);
        OmniNestedLoopJoinBuildOperatorFactory.saveNestedLoopJoinBuilderOperatorAndFactory(builderNodeId,
                omniNestedLoopJoinBuildOperatorFactory, omniOperator);
        tempOmniNestedLoopJoinBuildOperatorFactory = OmniNestedLoopJoinBuildOperatorFactory
                .getNestedLoopJoinBuilderOperatorFactory(builderNodeId);
        assertEquals(omniNestedLoopJoinBuildOperatorFactory == tempOmniNestedLoopJoinBuildOperatorFactory, true);
        OmniNestedLoopJoinBuildOperatorFactory.dereferenceNestedBuilderOperatorAndFactory(builderNodeId);
        tempOmniNestedLoopJoinBuildOperatorFactory = OmniNestedLoopJoinBuildOperatorFactory
                .getNestedLoopJoinBuilderOperatorFactory(builderNodeId);
        assertEquals(omniNestedLoopJoinBuildOperatorFactory == tempOmniNestedLoopJoinBuildOperatorFactory, true);
        OmniNestedLoopJoinBuildOperatorFactory.dereferenceNestedBuilderOperatorAndFactory(builderNodeId);
        OmniNestedLoopJoinBuildOperatorFactory.dereferenceNestedBuilderOperatorAndFactory(builderNodeId);
        tempOmniNestedLoopJoinBuildOperatorFactory = OmniNestedLoopJoinBuildOperatorFactory
                .getNestedLoopJoinBuilderOperatorFactory(builderNodeId);
        assertEquals(tempOmniNestedLoopJoinBuildOperatorFactory == null, true);
    }
}
