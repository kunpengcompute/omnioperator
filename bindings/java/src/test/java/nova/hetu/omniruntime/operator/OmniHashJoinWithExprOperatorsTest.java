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

import static nova.hetu.omniruntime.constants.JoinType.OMNI_JOIN_TYPE_FULL;
import static nova.hetu.omniruntime.constants.JoinType.OMNI_JOIN_TYPE_LEFT;
import static nova.hetu.omniruntime.constants.JoinType.OMNI_JOIN_TYPE_INNER;
import static nova.hetu.omniruntime.util.TestUtils.assertVecBatchEqualsIgnoreOrder;
import static nova.hetu.omniruntime.constants.BuildSide.BUILD_LEFT;
import static nova.hetu.omniruntime.util.TestUtils.assertVecBatchEquals;
import static nova.hetu.omniruntime.util.TestUtils.createVecBatch;
import static nova.hetu.omniruntime.util.TestUtils.freeVecBatch;
import static nova.hetu.omniruntime.util.TestUtils.freeVecBatches;
import static nova.hetu.omniruntime.util.TestUtils.getOmniJsonFieldReference;
import static nova.hetu.omniruntime.util.TestUtils.getOmniJsonLiteral;
import static nova.hetu.omniruntime.util.TestUtils.omniFunctionExpr;
import static nova.hetu.omniruntime.util.TestUtils.omniJsonFourArithmeticExpr;
import static nova.hetu.omniruntime.util.TestUtils.omniJsonNotEqualExpr;
import static org.testng.Assert.assertEquals;
import static org.testng.Assert.assertNotEquals;

import nova.hetu.omniruntime.operator.config.OperatorConfig;
import nova.hetu.omniruntime.operator.join.OmniHashBuilderWithExprOperatorFactory;
import nova.hetu.omniruntime.operator.join.OmniLookupJoinWithExprOperatorFactory;
import nova.hetu.omniruntime.operator.join.OmniLookupOuterJoinWithExprOperatorFactory;
import nova.hetu.omniruntime.type.DataType;
import nova.hetu.omniruntime.type.IntDataType;
import nova.hetu.omniruntime.type.LongDataType;
import nova.hetu.omniruntime.type.VarcharDataType;
import nova.hetu.omniruntime.util.TestUtils;
import nova.hetu.omniruntime.utils.OmniRuntimeException;
import nova.hetu.omniruntime.vector.Vec;
import nova.hetu.omniruntime.vector.VecBatch;

import org.testng.annotations.Test;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.Optional;

/**
 * The type Omni hash join with expression operator test.
 *
 * @since 2021-10-16
 */
public class OmniHashJoinWithExprOperatorsTest {
    /**
     * Test inner hash join one column .
     */
    @Test
    public void testInnerEqualityJoinOneColumn() {
        DataType[] buildTypes = {LongDataType.LONG, LongDataType.LONG};
        Object[][] buildDatas = {{1L, 2L, 1L, 2L, 3L, 4L, 5L, 6L, 7L, 1L},
                {79L, 79L, 70L, 70L, 70L, 70L, 70L, 70L, 70L, 70L}};
        VecBatch buildVecBatch = createVecBatch(buildTypes, buildDatas);

        String[] buildHashKeys = {omniJsonFourArithmeticExpr("ADD", 2, getOmniJsonFieldReference(2, 0),
                getOmniJsonLiteral(2, false, 50))};
        int operatorCount = 1;
        OmniHashBuilderWithExprOperatorFactory hashBuilderOperatorFactory = new OmniHashBuilderWithExprOperatorFactory(
                OMNI_JOIN_TYPE_INNER, buildTypes, buildHashKeys, operatorCount);
        OmniOperator hashBuilderOperator = hashBuilderOperatorFactory.createOperator();
        hashBuilderOperator.addInput(buildVecBatch);
        hashBuilderOperator.getOutput();

        DataType[] probeTypes = {LongDataType.LONG, LongDataType.LONG};
        Object[][] probeDatas = {{1L, 2L, 3L, 4L, 5L, 6L, 1L, 1L, 2L, 3L},
                {78L, 78L, 78L, 78L, 78L, 78L, 78L, 82L, 82L, 65L}};
        VecBatch probeVecBatch = createVecBatch(probeTypes, probeDatas);

        int[] probeOutputCols = {1};
        String[] probeHashKeys = {omniJsonFourArithmeticExpr("ADD", 2, getOmniJsonFieldReference(2, 0),
                getOmniJsonLiteral(2, false, 50))};
        int[] buildOutputCols = {1};
        DataType[] buildOutputTypes = {LongDataType.LONG};
        OmniLookupJoinWithExprOperatorFactory lookupJoinOperatorFactory = new OmniLookupJoinWithExprOperatorFactory(
                probeTypes, probeOutputCols, probeHashKeys, buildOutputCols, buildOutputTypes,
                hashBuilderOperatorFactory, false);
        OmniOperator lookupJoinOperator = lookupJoinOperatorFactory.createOperator();
        lookupJoinOperator.addInput(probeVecBatch);
        Iterator<VecBatch> results = lookupJoinOperator.getOutput();
        VecBatch resultVecBatch = results.next();

        int len = resultVecBatch.getRowCount();
        assertEquals(len, 18);
        Object[][] expectedDatas = {
                {78L, 78L, 78L, 78L, 78L, 78L, 78L, 78L, 78L, 78L, 78L, 78L, 82L, 82L, 82L, 82L, 82L, 65L},
                {79L, 70L, 70L, 79L, 70L, 70L, 70L, 70L, 70L, 79L, 70L, 70L, 79L, 70L, 70L, 79L, 70L, 70L}};
        assertVecBatchEqualsIgnoreOrder(resultVecBatch, expectedDatas);
        freeVecBatch(resultVecBatch);
        lookupJoinOperator.close();
        hashBuilderOperator.close();
        lookupJoinOperatorFactory.close();
        hashBuilderOperatorFactory.close();
    }

    @Test
    public void testInnerEqualityJoinOneColumnWithMultiVectorBatch() {
        DataType[] buildTypes = {IntDataType.INTEGER, IntDataType.INTEGER};
        Object[][] buildDatas = {{1, 2, 1, 2, 3, 4, 5, 6, 7, 1}, {79, 79, 70, 70, 70, 70, 70, 70, 70, 70}};
        VecBatch buildVecBatch = createVecBatch(buildTypes, buildDatas);

        String[] buildHashKeys = {omniJsonFourArithmeticExpr("ADD", 1, getOmniJsonFieldReference(1, 0),
                getOmniJsonLiteral(1, false, 50))};
        int operatorCount = 1;
        OmniHashBuilderWithExprOperatorFactory hashBuilderOperatorFactory = new OmniHashBuilderWithExprOperatorFactory(
                OMNI_JOIN_TYPE_INNER, buildTypes, buildHashKeys, operatorCount);
        OmniOperator hashBuilderOperator = hashBuilderOperatorFactory.createOperator();
        hashBuilderOperator.addInput(buildVecBatch);
        hashBuilderOperator.getOutput();

        DataType[] probeTypes = {IntDataType.INTEGER, IntDataType.INTEGER};

        int[] probeOutputCols = {1};
        String[] probeHashKeys = {omniJsonFourArithmeticExpr("ADD", 1, getOmniJsonFieldReference(1, 0),
                getOmniJsonLiteral(1, false, 50))};
        int[] buildOutputCols = {1};
        DataType[] buildOutputTypes = {IntDataType.INTEGER};
        OmniLookupJoinWithExprOperatorFactory lookupJoinOperatorFactory = new OmniLookupJoinWithExprOperatorFactory(
                probeTypes, probeOutputCols, probeHashKeys, buildOutputCols, buildOutputTypes,
                hashBuilderOperatorFactory, false);
        OmniOperator lookupJoinOperator = lookupJoinOperatorFactory.createOperator();

        Object[][] baseDatas = {{1, 3, 4, 5, 6, 1, 1, 2, 3}, {78, 78, 78, 78, 78, 78, 82, 82, 65}};
        int baseDataLen = 9;

        Object[][] probeDatas = new Object[2][];
        int baseRowCnt = 16;
        int maxRowCntPerBatch = 131072; // 1M / (4+4)
        // each batch of baseData will generate 16 row of records, and each vectorBatch
        // output will have a maximum of 131072 rows.The final result here will output 3
        // vectorBatch
        probeDatas[0] = new Object[3 * (maxRowCntPerBatch / baseRowCnt) * baseDataLen];
        probeDatas[1] = new Object[3 * (maxRowCntPerBatch / baseRowCnt) * baseDataLen];
        for (int i = 0; i < probeDatas[0].length; i++) {
            probeDatas[0][i] = baseDatas[0][i % baseDataLen];
            probeDatas[1][i] = baseDatas[1][i % baseDataLen];
        }

        VecBatch probeVecBatch = createVecBatch(probeTypes, probeDatas);
        lookupJoinOperator.addInput(probeVecBatch);
        Iterator<VecBatch> results = lookupJoinOperator.getOutput();
        int actualRowCnt = 0;
        Object[][] baseExpectedDatas = {{78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 82, 82, 82, 82, 82, 65},
                {79, 70, 70, 70, 70, 70, 70, 79, 70, 70, 79, 70, 70, 79, 70, 70}};
        while (results.hasNext()) {
            VecBatch resultVecBatch = results.next();
            int rowCnt = resultVecBatch.getRowCount();
            actualRowCnt += rowCnt;
            Object[][] expectedDatas = new Object[2][];
            expectedDatas[0] = new Object[rowCnt];
            expectedDatas[1] = new Object[rowCnt];
            for (int i = 0; i < rowCnt; i++) {
                expectedDatas[0][i] = baseExpectedDatas[0][i % baseRowCnt];
                expectedDatas[1][i] = baseExpectedDatas[1][i % baseRowCnt];
            }
            assertVecBatchEqualsIgnoreOrder(resultVecBatch, expectedDatas);
            freeVecBatch(resultVecBatch);
        }
        assertEquals(actualRowCnt, 393216);

        // the next batch of probe data
        VecBatch probeVecBatch1 = createVecBatch(probeTypes, probeDatas);
        lookupJoinOperator.addInput(probeVecBatch1);
        results = lookupJoinOperator.getOutput();
        actualRowCnt = 0;
        while (results.hasNext()) {
            VecBatch resultVecBatch = results.next();
            int rowCnt = resultVecBatch.getRowCount();
            actualRowCnt += rowCnt;
            Object[][] expectedDatas = new Object[2][];
            expectedDatas[0] = new Object[rowCnt];
            expectedDatas[1] = new Object[rowCnt];
            for (int i = 0; i < rowCnt; i++) {
                expectedDatas[0][i] = baseExpectedDatas[0][i % baseRowCnt];
                expectedDatas[1][i] = baseExpectedDatas[1][i % baseRowCnt];
            }
            assertVecBatchEqualsIgnoreOrder(resultVecBatch, expectedDatas);
            freeVecBatch(resultVecBatch);
        }
        assertEquals(actualRowCnt, 393216);

        lookupJoinOperator.close();
        hashBuilderOperator.close();
        lookupJoinOperatorFactory.close();
        hashBuilderOperatorFactory.close();
    }

    /**
     * Test inner hash join one dictionary column .
     */
    @Test
    public void testInnerEqualityJoinOneDictionaryColumn() {
        DataType[] buildTypes = {LongDataType.LONG, LongDataType.LONG};
        Object[][] buildDatas = {{1L, 2L, 1L, 2L, 3L, 4L, 5L, 6L, 7L, 1L},
                {79L, 79L, 70L, 70L, 70L, 70L, 70L, 70L, 70L, 70L}};
        Vec[] buildVecs = new Vec[2];
        int[] ids = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        buildVecs[0] = TestUtils.createDictionaryVec(buildTypes[0], buildDatas[0], ids);
        buildVecs[1] = TestUtils.createDictionaryVec(buildTypes[1], buildDatas[1], ids);
        VecBatch buildVecBatch = new VecBatch(buildVecs);

        String[] buildHashKeys = {omniJsonFourArithmeticExpr("ADD", 2, getOmniJsonFieldReference(2, 0),
                getOmniJsonLiteral(2, false, 50))};
        int operatorCount = 1;
        OmniHashBuilderWithExprOperatorFactory hashBuilderOperatorFactory = new OmniHashBuilderWithExprOperatorFactory(
                OMNI_JOIN_TYPE_INNER, buildTypes, buildHashKeys, operatorCount);
        OmniOperator hashBuilderOperator = hashBuilderOperatorFactory.createOperator();
        hashBuilderOperator.addInput(buildVecBatch);
        hashBuilderOperator.getOutput();

        DataType[] probeTypes = {LongDataType.LONG, LongDataType.LONG};
        Object[][] probeDatas = {{1L, 2L, 3L, 4L, 5L, 6L, 1L, 1L, 2L, 3L},
                {78L, 78L, 78L, 78L, 78L, 78L, 78L, 82L, 82L, 65L}};
        Vec[] probeVecs = new Vec[2];
        probeVecs[0] = TestUtils.createDictionaryVec(probeTypes[0], probeDatas[0], ids);
        probeVecs[1] = TestUtils.createDictionaryVec(probeTypes[1], probeDatas[1], ids);
        VecBatch probeVecBatch = new VecBatch(probeVecs);

        int[] probeOutputCols = {1};
        String[] probeHashKeys = {omniJsonFourArithmeticExpr("ADD", 2, getOmniJsonFieldReference(2, 0),
                getOmniJsonLiteral(2, false, 50))};
        int[] buildOutputCols = {1};
        DataType[] buildOutputTypes = {LongDataType.LONG};
        OmniLookupJoinWithExprOperatorFactory lookupJoinOperatorFactory = new OmniLookupJoinWithExprOperatorFactory(
                probeTypes, probeOutputCols, probeHashKeys, buildOutputCols, buildOutputTypes,
                hashBuilderOperatorFactory, false);
        OmniOperator lookupJoinOperator = lookupJoinOperatorFactory.createOperator();
        lookupJoinOperator.addInput(probeVecBatch);
        Iterator<VecBatch> results = lookupJoinOperator.getOutput();
        VecBatch resultVecBatch = results.next();

        int len = resultVecBatch.getRowCount();
        assertEquals(len, 18);
        Object[][] expectedDatas = {
                {78L, 78L, 78L, 78L, 78L, 78L, 78L, 78L, 78L, 78L, 78L, 78L, 82L, 82L, 82L, 82L, 82L, 65L},
                {79L, 70L, 70L, 79L, 70L, 70L, 70L, 70L, 70L, 79L, 70L, 70L, 79L, 70L, 70L, 79L, 70L, 70L}};
        assertVecBatchEqualsIgnoreOrder(resultVecBatch, expectedDatas);
        freeVecBatch(resultVecBatch);
        lookupJoinOperator.close();
        hashBuilderOperator.close();
        lookupJoinOperatorFactory.close();
        hashBuilderOperatorFactory.close();
    }

    /**
     * Test inner hash join with join filter expression .
     */
    @Test
    public void testInnerEqualityJoinWithCharFilter() {
        DataType[] buildTypes = {IntDataType.INTEGER, new VarcharDataType(5)};
        Object[][] buildDatas = {{19, 14, 7, 19, 1, 20, 10, 13, 20, 16},
                {"35709", "31904", "35709", "31904", "35709", "31904", "35709", "31904", "35709", "31904"}};
        VecBatch buildVecBatch = createVecBatch(buildTypes, buildDatas);

        String[] buildHashCols = {getOmniJsonFieldReference(1, 0)};
        int operatorCount = 1;
        String filterExpression = omniJsonNotEqualExpr(
                omniFunctionExpr("substr", 15,
                        getOmniJsonFieldReference(15, 1) + "," + getOmniJsonLiteral(1, false, 1) + ","
                                + getOmniJsonLiteral(1, false, 5)),
                omniFunctionExpr("substr", 15, getOmniJsonFieldReference(15, 3) + "," + getOmniJsonLiteral(1, false, 1)
                        + "," + getOmniJsonLiteral(1, false, 5)));
        OmniHashBuilderWithExprOperatorFactory hashBuilderOperatorFactory = new OmniHashBuilderWithExprOperatorFactory(
                OMNI_JOIN_TYPE_INNER, buildTypes, buildHashCols, operatorCount);
        OmniOperator hashBuilderOperator = hashBuilderOperatorFactory.createOperator();
        hashBuilderOperator.addInput(buildVecBatch);
        hashBuilderOperator.getOutput();

        DataType[] probeTypes = {IntDataType.INTEGER, new VarcharDataType(5)};
        Object[][] probeDatas = {{20, 16, 13, 4, 20, 4, 22, 19, 8, 7},
                {"35709", "35709", "31904", "12477", null, "38721", "90419", "35709", "88371", null}};
        VecBatch probeVecBatch = createVecBatch(probeTypes, probeDatas);

        int[] probeOutputCols = {0, 1};
        String[] probeHashCols = {getOmniJsonFieldReference(1, 0)};
        int[] buildOutputCols = {0, 1};
        DataType[] buildOutputTypes = {IntDataType.INTEGER, new VarcharDataType(5)};
        OmniLookupJoinWithExprOperatorFactory lookupJoinOperatorFactory = new OmniLookupJoinWithExprOperatorFactory(
                probeTypes, probeOutputCols, probeHashCols, buildOutputCols, buildOutputTypes,
                hashBuilderOperatorFactory, Optional.of(filterExpression), false);
        OmniOperator lookupJoinOperator = lookupJoinOperatorFactory.createOperator();
        lookupJoinOperator.addInput(probeVecBatch);
        Iterator<VecBatch> results = lookupJoinOperator.getOutput();

        VecBatch resultVecBatch = results.next();
        int len = resultVecBatch.getRowCount();
        assertEquals(len, 3);
        assertEquals(resultVecBatch.getVectorCount(), 4);
        Object[][] expectedDatas = {{20, 16, 19}, {"35709", "35709", "35709"}, {20, 16, 19},
                {"31904", "31904", "31904"}};
        assertVecBatchEqualsIgnoreOrder(resultVecBatch, expectedDatas);
        freeVecBatch(resultVecBatch);
        lookupJoinOperator.close();
        hashBuilderOperator.close();
        lookupJoinOperatorFactory.close();
        hashBuilderOperatorFactory.close();
    }

    /**
     * Test opposite side hash join one column .
     * It means (left outer join && BuildRight) or (right outer join && BuildLeft) .
     * This test example is (left outer join && BuildRight).
     */
    @Test
    public void testOppositeSideOuterEqualityJoinOneColumn() {
        DataType[] buildTypes = {LongDataType.LONG, LongDataType.LONG};
        Object[][] buildDatas = {{1L, 2L, 1L, 2L, 3L, 4L, 5L, 6L, 7L, 1L},
                {79L, 79L, 70L, 70L, 70L, 70L, 70L, 70L, 70L, 70L}};
        VecBatch buildVecBatch = createVecBatch(buildTypes, buildDatas);

        String[] buildHashKeys = {omniJsonFourArithmeticExpr("ADD", 2, getOmniJsonFieldReference(2, 0),
                getOmniJsonLiteral(2, false, 50))};
        int operatorCount = 1;
        OmniHashBuilderWithExprOperatorFactory hashBuilderOperatorFactory = new OmniHashBuilderWithExprOperatorFactory(
                OMNI_JOIN_TYPE_LEFT, buildTypes, buildHashKeys, operatorCount);
        OmniOperator hashBuilderOperator = hashBuilderOperatorFactory.createOperator();
        hashBuilderOperator.addInput(buildVecBatch);
        hashBuilderOperator.getOutput();

        DataType[] probeTypes = {LongDataType.LONG, LongDataType.LONG};
        Object[][] probeDatas = {{1L, 2L, 3L, 4L, 5L, 6L, 1L, 1L, 2L, 3L, 9L},
                {78L, 78L, 78L, 78L, 78L, 78L, 78L, 82L, 82L, 65L, 99L}};
        VecBatch probeVecBatch = createVecBatch(probeTypes, probeDatas);

        int[] probeOutputCols = {0, 1};
        String[] probeHashKeys = {omniJsonFourArithmeticExpr("ADD", 2, getOmniJsonFieldReference(2, 0),
                getOmniJsonLiteral(2, false, 50))};
        int[] buildOutputCols = {0, 1};
        DataType[] buildOutputTypes = {LongDataType.LONG, LongDataType.LONG};
        OmniLookupJoinWithExprOperatorFactory lookupJoinOperatorFactory = new OmniLookupJoinWithExprOperatorFactory(
                probeTypes, probeOutputCols, probeHashKeys, buildOutputCols, buildOutputTypes,
                hashBuilderOperatorFactory, false);
        OmniOperator lookupJoinOperator = lookupJoinOperatorFactory.createOperator();
        lookupJoinOperator.addInput(probeVecBatch);
        Iterator<VecBatch> results = lookupJoinOperator.getOutput();
        VecBatch resultVecBatch = results.next();
        assertEquals(resultVecBatch.getRowCount(), 19);
        Object[][] expectedDatas = {{1L, 1L, 1L, 2L, 2L, 3L, 4L, 5L, 6L, 1L, 1L, 1L, 1L, 1L, 1L, 2L, 2L, 3L, 9L},
                {78L, 78L, 78L, 78L, 78L, 78L, 78L, 78L, 78L, 78L, 78L, 78L, 82L, 82L, 82L, 82L, 82L, 65L, 99L},
                {1L, 1L, 1L, 2L, 2L, 3L, 4L, 5L, 6L, 1L, 1L, 1L, 1L, 1L, 1L, 2L, 2L, 3L, null},
                {79L, 70L, 70L, 79L, 70L, 70L, 70L, 70L, 70L, 79L, 70L, 70L, 79L, 70L, 70L, 79L, 70L, 70L, null}};
        assertVecBatchEqualsIgnoreOrder(resultVecBatch, expectedDatas);
        freeVecBatch(resultVecBatch);
        lookupJoinOperator.close();
        hashBuilderOperator.close();
        lookupJoinOperatorFactory.close();
        hashBuilderOperatorFactory.close();
    }

    /**
     * Test same side hash join one column .
     * It means (left outer join && BuildLeft) or (right outer join && BuildRight) .
     * This test example is (left outer join && BuildLeft).
     */
    @Test
    public void testSameSideOuterEqualityJoinOneColumn() {
        DataType[] buildTypes = {LongDataType.LONG, LongDataType.LONG};
        Object[][] buildDatas = {{1L, 2L, 3L, 4L, 5L, 6L, 1L, 1L, 2L, 3L, 9L},
                {78L, 78L, 78L, 78L, 78L, 78L, 78L, 82L, 82L, 65L, 99L}};
        VecBatch buildVecBatch = createVecBatch(buildTypes, buildDatas);

        String[] buildHashKeys = {omniJsonFourArithmeticExpr("ADD", 2, getOmniJsonFieldReference(2, 0),
                getOmniJsonLiteral(2, false, 50))};
        int operatorCount = 1;
        OmniHashBuilderWithExprOperatorFactory hashBuilderOperatorFactory = new OmniHashBuilderWithExprOperatorFactory(
                OMNI_JOIN_TYPE_LEFT, BUILD_LEFT, buildTypes, buildHashKeys, operatorCount, new OperatorConfig());
        OmniOperator hashBuilderOperator = hashBuilderOperatorFactory.createOperator();
        hashBuilderOperator.addInput(buildVecBatch);
        hashBuilderOperator.getOutput();

        DataType[] probeTypes = {LongDataType.LONG, LongDataType.LONG};
        Object[][] probeDatas = {{1L, 2L, 1L, 2L, 3L, 4L, 5L, 6L, 7L, 1L},
                {79L, 79L, 70L, 70L, 70L, 70L, 70L, 70L, 70L, 70L}};
        VecBatch probeVecBatch = createVecBatch(probeTypes, probeDatas);

        int[] probeOutputCols = {0, 1};
        String[] probeHashKeys = {omniJsonFourArithmeticExpr("ADD", 2, getOmniJsonFieldReference(2, 0),
                getOmniJsonLiteral(2, false, 50))};
        int[] buildOutputCols = {0, 1};
        DataType[] buildOutputTypes = {LongDataType.LONG, LongDataType.LONG};
        OmniLookupJoinWithExprOperatorFactory lookupJoinOperatorFactory = new OmniLookupJoinWithExprOperatorFactory(
                probeTypes, probeOutputCols, probeHashKeys, buildOutputCols, buildOutputTypes,
                hashBuilderOperatorFactory, false);
        OmniOperator lookupJoinOperator = lookupJoinOperatorFactory.createOperator();

        OmniLookupOuterJoinWithExprOperatorFactory lookupOuterJoinOperatorFactory =
                new OmniLookupOuterJoinWithExprOperatorFactory(
                probeTypes, probeOutputCols, probeHashKeys, buildOutputCols, buildOutputTypes,
                hashBuilderOperatorFactory, new OperatorConfig());
        OmniOperator lookupOuterJoinOperator = lookupOuterJoinOperatorFactory.createOperator();
        lookupJoinOperator.addInput(probeVecBatch);
        Iterator<VecBatch> results = lookupJoinOperator.getOutput();
        VecBatch resultVecBatch = results.next();
        assertEquals(resultVecBatch.getRowCount(), 18);
        Object[][] expectedDatas = {{1L, 1L, 1L, 2L, 2L, 1L, 1L, 1L, 2L, 2L, 3L, 3L, 4L, 5L, 6L, 1L, 1L, 1L},
                {78L, 78L, 82L, 78L, 82L, 78L, 78L, 82L, 78L, 82L, 78L, 65L, 78L, 78L, 78L, 78L, 78L, 82L},
                {1L, 1L, 1L, 2L, 2L, 1L, 1L, 1L, 2L, 2L, 3L, 3L, 4L, 5L, 6L, 1L, 1L, 1L},
                {79L, 79L, 79L, 79L, 79L, 70L, 70L, 70L, 70L, 70L, 70L, 70L, 70L, 70L, 70L, 70L, 70L, 70L}};
        assertVecBatchEqualsIgnoreOrder(resultVecBatch, expectedDatas);
        Iterator<VecBatch> appendResults = lookupOuterJoinOperator.getOutput();
        VecBatch appendBatch = appendResults.next();
        assertEquals(appendBatch.getRowCount(), 1);
        Object[][] expectedData = {{null}, {null}, {9L}, {99L}};
        assertVecBatchEquals(appendBatch, expectedData);
        freeVecBatch(resultVecBatch);
        freeVecBatch(appendBatch);
        lookupJoinOperator.close();
        hashBuilderOperator.close();
        lookupOuterJoinOperator.close();
        lookupJoinOperatorFactory.close();
        hashBuilderOperatorFactory.close();
        lookupOuterJoinOperatorFactory.close();
    }

    /**
     * Test full hash join one column .
     */
    @Test
    public void testFullOuterEqualityJoinOneColumn() {
        DataType[] buildTypes = {LongDataType.LONG, LongDataType.LONG};
        Object[][] buildDatas = {{1L, 2L, 1L, 2L, 3L, 4L, 5L, 6L, 7L, 1L},
                {79L, 79L, 70L, 70L, 70L, 70L, 70L, 70L, 70L, 70L}};
        VecBatch buildVecBatch = createVecBatch(buildTypes, buildDatas);

        String[] buildHashKeys = {omniJsonFourArithmeticExpr("ADD", 2, getOmniJsonFieldReference(2, 0),
                getOmniJsonLiteral(2, false, 50))};
        int operatorCount = 1;
        OmniHashBuilderWithExprOperatorFactory hashBuilderOperatorFactory = new OmniHashBuilderWithExprOperatorFactory(
                OMNI_JOIN_TYPE_FULL, buildTypes, buildHashKeys, operatorCount);
        OmniOperator hashBuilderOperator = hashBuilderOperatorFactory.createOperator();
        hashBuilderOperator.addInput(buildVecBatch);
        hashBuilderOperator.getOutput();

        DataType[] probeTypes = {LongDataType.LONG, LongDataType.LONG};
        Object[][] probeDatas = {{1L, 2L, 3L, 4L, 5L, 6L, 1L, 1L, 2L, 3L},
                {78L, 78L, 78L, 78L, 78L, 78L, 78L, 82L, 82L, 65L}};
        VecBatch probeVecBatch = createVecBatch(probeTypes, probeDatas);

        int[] probeOutputCols = {0, 1};
        String[] probeHashKeys = {omniJsonFourArithmeticExpr("ADD", 2, getOmniJsonFieldReference(2, 0),
                getOmniJsonLiteral(2, false, 50))};
        int[] buildOutputCols = {0, 1};
        DataType[] buildOutputTypes = {LongDataType.LONG, LongDataType.LONG};
        OmniLookupJoinWithExprOperatorFactory lookupJoinOperatorFactory = new OmniLookupJoinWithExprOperatorFactory(
                probeTypes, probeOutputCols, probeHashKeys, buildOutputCols, buildOutputTypes,
                hashBuilderOperatorFactory, false);
        OmniOperator lookupJoinOperator = lookupJoinOperatorFactory.createOperator();

        OmniLookupOuterJoinWithExprOperatorFactory lookupOuterJoinOperatorFactory =
                new OmniLookupOuterJoinWithExprOperatorFactory(
                probeTypes, probeOutputCols, probeHashKeys, buildOutputCols, buildOutputTypes,
                hashBuilderOperatorFactory, new OperatorConfig());
        OmniOperator lookupOuterJoinOperator = lookupOuterJoinOperatorFactory.createOperator();
        lookupJoinOperator.addInput(probeVecBatch);
        Iterator<VecBatch> results = lookupJoinOperator.getOutput();
        VecBatch resultVecBatch = results.next();
        assertEquals(resultVecBatch.getRowCount(), 18);
        Object[][] expectedDatas = {{1L, 1L, 1L, 2L, 2L, 3L, 4L, 5L, 6L, 1L, 1L, 1L, 1L, 1L, 1L, 2L, 2L, 3L},
                {78L, 78L, 78L, 78L, 78L, 78L, 78L, 78L, 78L, 78L, 78L, 78L, 82L, 82L, 82L, 82L, 82L, 65L},
                {1L, 1L, 1L, 2L, 2L, 3L, 4L, 5L, 6L, 1L, 1L, 1L, 1L, 1L, 1L, 2L, 2L, 3L},
                {79L, 70L, 70L, 79L, 70L, 70L, 70L, 70L, 70L, 79L, 70L, 70L, 79L, 70L, 70L, 79L, 70L, 70L}};
        assertVecBatchEqualsIgnoreOrder(resultVecBatch, expectedDatas);
        Iterator<VecBatch> appendResults = lookupOuterJoinOperator.getOutput();
        VecBatch appendBatch = appendResults.next();
        assertEquals(appendBatch.getRowCount(), 1);
        Object[][] expectedData = {{null}, {null}, {7L}, {70L}};
        assertVecBatchEqualsIgnoreOrder(appendBatch, expectedData);
        freeVecBatch(resultVecBatch);
        freeVecBatch(appendBatch);
        lookupJoinOperator.close();
        hashBuilderOperator.close();
        lookupOuterJoinOperator.close();
        lookupJoinOperatorFactory.close();
        hashBuilderOperatorFactory.close();
        lookupOuterJoinOperatorFactory.close();
    }

    /**
     * Test full hash join one dictionary column .
     */
    @Test
    public void testFullOuterEqualityJoinOneDictionaryColumn() {
        DataType[] buildTypes = {LongDataType.LONG, LongDataType.LONG};
        Object[][] buildDatas = {{1L, 2L, 1L, 2L, 3L, 4L, 5L, 6L, 7L, 1L},
                {79L, 79L, 70L, 70L, 70L, 70L, 70L, 70L, 70L, 70L}};
        Vec[] buildVecs = new Vec[2];
        int[] ids = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        buildVecs[0] = TestUtils.createDictionaryVec(buildTypes[0], buildDatas[0], ids);
        buildVecs[1] = TestUtils.createDictionaryVec(buildTypes[1], buildDatas[1], ids);
        VecBatch buildVecBatch = new VecBatch(buildVecs);

        String[] buildHashKeys = {omniJsonFourArithmeticExpr("ADD", 2, getOmniJsonFieldReference(2, 0),
                getOmniJsonLiteral(2, false, 50))};
        int operatorCount = 1;
        OmniHashBuilderWithExprOperatorFactory hashBuilderOperatorFactory = new OmniHashBuilderWithExprOperatorFactory(
                OMNI_JOIN_TYPE_FULL, buildTypes, buildHashKeys, operatorCount);
        OmniOperator hashBuilderOperator = hashBuilderOperatorFactory.createOperator();
        hashBuilderOperator.addInput(buildVecBatch);
        hashBuilderOperator.getOutput();

        DataType[] probeTypes = {LongDataType.LONG, LongDataType.LONG};
        Object[][] probeDatas = {{1L, 2L, 3L, 4L, 5L, 6L, 1L, 1L, 2L, 3L},
                {78L, 78L, 78L, 78L, 78L, 78L, 78L, 82L, 82L, 65L}};
        Vec[] probeVecs = new Vec[2];
        probeVecs[0] = TestUtils.createDictionaryVec(probeTypes[0], probeDatas[0], ids);
        probeVecs[1] = TestUtils.createDictionaryVec(probeTypes[1], probeDatas[1], ids);
        VecBatch probeVecBatch = new VecBatch(probeVecs);

        int[] probeOutputCols = {1};
        String[] probeHashKeys = {omniJsonFourArithmeticExpr("ADD", 2, getOmniJsonFieldReference(2, 0),
                getOmniJsonLiteral(2, false, 50))};
        int[] buildOutputCols = {1};
        DataType[] buildOutputTypes = {LongDataType.LONG};
        OmniLookupJoinWithExprOperatorFactory lookupJoinOperatorFactory = new OmniLookupJoinWithExprOperatorFactory(
                probeTypes, probeOutputCols, probeHashKeys, buildOutputCols, buildOutputTypes,
                hashBuilderOperatorFactory, false);
        OmniOperator lookupJoinOperator = lookupJoinOperatorFactory.createOperator();
        OmniLookupOuterJoinWithExprOperatorFactory lookupOuterJoinOperatorFactory =
                new OmniLookupOuterJoinWithExprOperatorFactory(
                probeTypes, probeOutputCols, probeHashKeys, buildOutputCols, buildOutputTypes,
                hashBuilderOperatorFactory, new OperatorConfig());
        OmniOperator lookupOuterJoinOperator = lookupOuterJoinOperatorFactory.createOperator();
        lookupJoinOperator.addInput(probeVecBatch);
        Iterator<VecBatch> results = lookupJoinOperator.getOutput();
        VecBatch resultVecBatch = results.next();
        int len = resultVecBatch.getRowCount();
        assertEquals(len, 18);
        Object[][] expectedDatas = {
                {78L, 78L, 78L, 78L, 78L, 78L, 78L, 78L, 78L, 78L, 78L, 78L, 82L, 82L, 82L, 82L, 82L, 65L},
                {79L, 70L, 70L, 79L, 70L, 70L, 70L, 70L, 70L, 79L, 70L, 70L, 79L, 70L, 70L, 79L, 70L, 70L}};
        assertVecBatchEqualsIgnoreOrder(resultVecBatch, expectedDatas);

        Iterator<VecBatch> appendResults = lookupOuterJoinOperator.getOutput();
        VecBatch appendBatch = appendResults.next();
        len = appendBatch.getRowCount();
        assertEquals(len, 1);
        Object[][] expectedData = {{null}, {70L}};
        assertVecBatchEqualsIgnoreOrder(appendBatch, expectedData);
        freeVecBatch(resultVecBatch);
        freeVecBatch(appendBatch);
        lookupJoinOperator.close();
        hashBuilderOperator.close();
        lookupOuterJoinOperator.close();
        lookupJoinOperatorFactory.close();
        hashBuilderOperatorFactory.close();
        lookupOuterJoinOperatorFactory.close();
    }

    /**
     * Test full hash join with join filter expression .
     */
    @Test
    public void testFullEqualityJoinWithCharFilter() {
        DataType[] buildTypes = {IntDataType.INTEGER, new VarcharDataType(5)};
        Object[][] buildDatas = {{19, 14, 7, 19, 1, 20, 10, 13, 20, 16},
                {"35709", "31904", "35709", "31904", "35709", "31904", "35709", "31904", "35709", "31904"}};
        VecBatch buildVecBatch = createVecBatch(buildTypes, buildDatas);

        String[] buildHashCols = {getOmniJsonFieldReference(1, 0)};
        int operatorCount = 1;
        String filterExpression = omniJsonNotEqualExpr(
                omniFunctionExpr("substr", 15,
                        getOmniJsonFieldReference(15, 1) + "," + getOmniJsonLiteral(1, false, 1) + ","
                                + getOmniJsonLiteral(1, false, 5)),
                omniFunctionExpr("substr", 15, getOmniJsonFieldReference(15, 3) + "," + getOmniJsonLiteral(1, false, 1)
                        + "," + getOmniJsonLiteral(1, false, 5)));
        OmniHashBuilderWithExprOperatorFactory hashBuilderOperatorFactory = new OmniHashBuilderWithExprOperatorFactory(
                OMNI_JOIN_TYPE_FULL, buildTypes, buildHashCols, operatorCount);
        OmniOperator hashBuilderOperator = hashBuilderOperatorFactory.createOperator();
        hashBuilderOperator.addInput(buildVecBatch);
        hashBuilderOperator.getOutput();

        DataType[] probeTypes = {IntDataType.INTEGER, new VarcharDataType(5)};
        Object[][] probeDatas = {{20, 16, 13, 4, 20, 4, 22, 19, 8, 7},
                {"35709", "35709", "31904", "12477", null, "38721", "90419", "35709", "88371", null}};
        VecBatch probeVecBatch = createVecBatch(probeTypes, probeDatas);

        int[] probeOutputCols = {0, 1};
        String[] probeHashCols = {getOmniJsonFieldReference(1, 0)};
        int[] buildOutputCols = {0, 1};
        DataType[] buildOutputTypes = {IntDataType.INTEGER, new VarcharDataType(5)};
        OmniLookupJoinWithExprOperatorFactory lookupJoinOperatorFactory = new OmniLookupJoinWithExprOperatorFactory(
                probeTypes, probeOutputCols, probeHashCols, buildOutputCols, buildOutputTypes,
                hashBuilderOperatorFactory, Optional.of(filterExpression), false);
        OmniOperator lookupJoinOperator = lookupJoinOperatorFactory.createOperator();
        OmniLookupOuterJoinWithExprOperatorFactory lookupOuterJoinOperatorFactory =
                new OmniLookupOuterJoinWithExprOperatorFactory(
                probeTypes, probeOutputCols, probeHashCols, buildOutputCols, buildOutputTypes,
                hashBuilderOperatorFactory, new OperatorConfig());
        OmniOperator lookupOuterJoinOperator = lookupOuterJoinOperatorFactory.createOperator();
        lookupJoinOperator.addInput(probeVecBatch);
        Iterator<VecBatch> results = lookupJoinOperator.getOutput();
        VecBatch resultVecBatch = results.next();
        assertEquals(resultVecBatch.getRowCount(), 10);
        Object[][] expectedDatas = {{20, 16, 13, 4, 20, 4, 22, 19, 8, 7},
                {"35709", "35709", "31904", "12477", null, "38721", "90419", "35709", "88371", null},
                {20, 16, null, null, null, null, null, 19, null, null},
                {"31904", "31904", null, null, null, null, null, "31904", null, null}};
        assertVecBatchEqualsIgnoreOrder(resultVecBatch, expectedDatas);
        Iterator<VecBatch> appendResults = lookupOuterJoinOperator.getOutput();
        VecBatch appendBatch = appendResults.next();
        assertEquals(appendBatch.getRowCount(), 7);
        Object[][] expectedData = {{null, null, null, null, null, null, null},
                {null, null, null, null, null, null, null}, {1, 7, 10, 13, 14, 19, 20},
                {"35709", "35709", "35709", "31904", "31904", "35709", "35709"}};
        assertVecBatchEqualsIgnoreOrder(appendBatch, expectedData);
        freeVecBatch(resultVecBatch);
        freeVecBatch(appendBatch);
        lookupJoinOperator.close();
        hashBuilderOperator.close();
        lookupJoinOperatorFactory.close();
        hashBuilderOperatorFactory.close();
    }

    /**
     * Test full hash join multiple build batch.
     */
    @Test
    public void testFullOuterJoinMultipleBatch() {
        DataType[] buildTypes = {LongDataType.LONG, new VarcharDataType(5)};
        String[] buildHashKeys = {getOmniJsonFieldReference(2, 0)};
        OmniHashBuilderWithExprOperatorFactory hashBuilderOperatorFactory = new OmniHashBuilderWithExprOperatorFactory(
                OMNI_JOIN_TYPE_FULL, buildTypes, buildHashKeys, 1);
        OmniOperator hashBuilderOperator = hashBuilderOperatorFactory.createOperator();
        hashBuilderOperator.addInput(createVecBatch(buildTypes, new Object[][]{{1L}, {"35709"}}));
        hashBuilderOperator.addInput(createVecBatch(buildTypes, new Object[][]{{13L}, {"31904"}}));
        hashBuilderOperator.addInput(createVecBatch(buildTypes, new Object[][]{{16L}, {"31904"}}));
        hashBuilderOperator.addInput(createVecBatch(buildTypes, new Object[][]{{20L, 20L}, {"31904", "35709"}}));
        hashBuilderOperator.addInput(createVecBatch(buildTypes, new Object[][]{{19L, 19L}, {"35709", "31904"}}));
        hashBuilderOperator.addInput(createVecBatch(buildTypes, new Object[][]{{7L}, {"35709"}}));
        hashBuilderOperator.addInput(createVecBatch(buildTypes, new Object[][]{{10L}, {"35709"}}));
        hashBuilderOperator.addInput(createVecBatch(buildTypes, new Object[][]{{14L}, {"31904"}}));
        hashBuilderOperator.getOutput();

        DataType[] probeTypes = {LongDataType.LONG, new VarcharDataType(5)};
        int[] probeOutputCols = {0, 1};
        String[] probeHashKeys = {getOmniJsonFieldReference(2, 0)};
        int[] buildOutputCols = {0, 1};
        DataType[] buildOutputTypes = {LongDataType.LONG, new VarcharDataType(5)};
        OmniLookupJoinWithExprOperatorFactory lookupJoinOperatorFactory = new OmniLookupJoinWithExprOperatorFactory(
                probeTypes, probeOutputCols, probeHashKeys, buildOutputCols, buildOutputTypes,
                hashBuilderOperatorFactory, false);
        OmniOperator lookupJoinOperator = lookupJoinOperatorFactory.createOperator();
        OmniLookupOuterJoinWithExprOperatorFactory lookupOuterJoinOperatorFactory =
                new OmniLookupOuterJoinWithExprOperatorFactory(
                probeTypes, probeOutputCols, probeHashKeys, buildOutputCols, buildOutputTypes,
                hashBuilderOperatorFactory, new OperatorConfig());
        OmniOperator lookupOuterJoinOperator = lookupOuterJoinOperatorFactory.createOperator();
        Object[][] probeDatas = {{22L, 13L, 16L, 20L, 20L, 19L, 4L, 4L, 8L, 7L},
                {"90419", "31904", "35709", "35709", null, "35709", "12477", "38721", "88371", null}};
        VecBatch probeVecBatch = createVecBatch(probeTypes, probeDatas);
        lookupJoinOperator.addInput(probeVecBatch);
        VecBatch resultVecBatch = lookupJoinOperator.getOutput().next();
        VecBatch appendBatch = lookupOuterJoinOperator.getOutput().next();
        assertVecBatchEqualsIgnoreOrder(resultVecBatch,
                new Object[][]{{22L, 13L, 16L, 20L, 20L, 20L, 20L, 19L, 19L, 4L, 4L, 8L, 7L},
                        {"90419", "31904", "35709", "35709", "35709", null, null, "35709", "35709", "12477", "38721",
                                "88371", null},
                        {null, 13L, 16L, 20L, 20L, 20L, 20L, 19L, 19L, null, null, null, 7L}, {null, "31904", "31904",
                                "31904", "35709", "31904", "35709", "35709", "31904", null, null, null, "35709"}});
        assertVecBatchEqualsIgnoreOrder(appendBatch,
                new Object[][]{{null, null, null}, {null, null, null}, {1L, 10L, 14L}, {"35709", "35709", "31904"}});

        lookupJoinOperator.close();
        hashBuilderOperator.close();
        lookupOuterJoinOperator.close();
        lookupJoinOperatorFactory.close();
        hashBuilderOperatorFactory.close();
        lookupOuterJoinOperatorFactory.close();
        freeVecBatch(resultVecBatch);
        freeVecBatch(appendBatch);
    }

    @Test(expectedExceptions = OmniRuntimeException.class, expectedExceptionsMessageRegExp =
            ".*EXPRESSION_NOT_SUPPORT.*")
    public void testHashBuilderWithInvalidKeys() {
        DataType[] buildTypes = {LongDataType.LONG, LongDataType.LONG};
        int operatorCount = 1;

        // invalid build hash key
        String[] invalidBuildHashKeys = {omniFunctionExpr("abc", 2, getOmniJsonFieldReference(2, 1))};
        OmniHashBuilderWithExprOperatorFactory operatorFactory = new OmniHashBuilderWithExprOperatorFactory(
                OMNI_JOIN_TYPE_FULL, buildTypes, invalidBuildHashKeys, operatorCount);
    }

    @Test(expectedExceptions = OmniRuntimeException.class, expectedExceptionsMessageRegExp =
            ".*EXPRESSION_NOT_SUPPORT.*")
    public void testLookupJoinWithInvalidKeys() {
        DataType[] buildTypes = {LongDataType.LONG, LongDataType.LONG};
        int operatorCount = 1;
        String[] buildHashCols = {getOmniJsonFieldReference(2, 0)};
        OmniHashBuilderWithExprOperatorFactory hashBuilderOperatorFactory = new OmniHashBuilderWithExprOperatorFactory(
                OMNI_JOIN_TYPE_INNER, buildTypes, buildHashCols, operatorCount);

        DataType[] probeTypes = {LongDataType.LONG, LongDataType.LONG};
        int[] probeOutputCols = {0, 1};
        String[] invalidProbeHashKeys = {omniFunctionExpr("abc", 2, getOmniJsonFieldReference(2, 1))};
        int[] buildOutputCols = {0, 1};
        DataType[] buildOutputTypes = {LongDataType.LONG, LongDataType.LONG};
        OmniLookupJoinWithExprOperatorFactory lookupJoinOperatorFactory = new OmniLookupJoinWithExprOperatorFactory(
                probeTypes, probeOutputCols, invalidProbeHashKeys, buildOutputCols, buildOutputTypes,
                hashBuilderOperatorFactory, false);

        hashBuilderOperatorFactory.close();
    }

    @Test(expectedExceptions = OmniRuntimeException.class, expectedExceptionsMessageRegExp =
            ".*EXPRESSION_NOT_SUPPORT.*")
    public void testInnerEqualityJoinWithInvalidExprs() {
        DataType[] buildTypes = {LongDataType.LONG, LongDataType.LONG};
        int operatorCount = 1;
        String[] buildHashCols = {getOmniJsonFieldReference(2, 0)};
        String filterExpression = omniJsonNotEqualExpr(
                omniFunctionExpr("substring", 15,
                        getOmniJsonFieldReference(2, 1) + "," + getOmniJsonLiteral(1, false, 1) + ","
                                + getOmniJsonLiteral(1, false, 5)),
                omniFunctionExpr("substring", 15, getOmniJsonFieldReference(2, 0) + ","
                        + getOmniJsonLiteral(1, false, 1) + "," + getOmniJsonLiteral(1, false, 5)));
        OmniHashBuilderWithExprOperatorFactory hashBuilderOperatorFactory = new OmniHashBuilderWithExprOperatorFactory(
                OMNI_JOIN_TYPE_INNER, buildTypes, buildHashCols, operatorCount);

        DataType[] probeTypes = {LongDataType.LONG, LongDataType.LONG};
        int[] probeOutputCols = {0, 1};
        String[] probeHashCols = {getOmniJsonFieldReference(2, 1)};
        int[] buildOutputCols = {0, 1};
        DataType[] buildOutputTypes = {LongDataType.LONG, LongDataType.LONG};
        OmniLookupJoinWithExprOperatorFactory lookupJoinOperatorFactory = new OmniLookupJoinWithExprOperatorFactory(
                probeTypes, probeOutputCols, probeHashCols, buildOutputCols, buildOutputTypes,
                hashBuilderOperatorFactory, Optional.of(filterExpression), false);

        hashBuilderOperatorFactory.close();
    }

    @Test
    public void testFactoryContextEquals() {
        DataType[] buildTypes = {LongDataType.LONG, LongDataType.LONG};
        String[] buildHashKeys = {omniJsonFourArithmeticExpr("ADD", 2, getOmniJsonFieldReference(2, 0),
                getOmniJsonLiteral(2, false, 50))};
        int operatorCount = 1;
        OmniHashBuilderWithExprOperatorFactory.FactoryContext hashBuilderOperatorFactory1 =
                new OmniHashBuilderWithExprOperatorFactory.FactoryContext(
                OMNI_JOIN_TYPE_INNER, buildTypes, buildHashKeys, operatorCount, new OperatorConfig());
        OmniHashBuilderWithExprOperatorFactory.FactoryContext hashBuilderOperatorFactory2 =
                new OmniHashBuilderWithExprOperatorFactory.FactoryContext(
                OMNI_JOIN_TYPE_INNER, buildTypes, buildHashKeys, operatorCount, new OperatorConfig());
        OmniHashBuilderWithExprOperatorFactory.FactoryContext hashBuilderOperatorFactory3 = null;
        assertEquals(hashBuilderOperatorFactory2, hashBuilderOperatorFactory1);
        assertEquals(hashBuilderOperatorFactory1, hashBuilderOperatorFactory1);
        assertNotEquals(hashBuilderOperatorFactory3, hashBuilderOperatorFactory1);

        DataType[] probeTypes = {LongDataType.LONG, LongDataType.LONG};
        int[] probeOutputCols = {1};
        String[] probeHashKeys = {omniJsonFourArithmeticExpr("ADD", 2, getOmniJsonFieldReference(2, 0),
                getOmniJsonLiteral(2, false, 50))};
        int[] buildOutputCols = {1};
        DataType[] buildOutputTypes = {LongDataType.LONG};

        OmniHashBuilderWithExprOperatorFactory omniHashBuilderWithExprOperatorFactory =
                new OmniHashBuilderWithExprOperatorFactory(
                OMNI_JOIN_TYPE_INNER, buildTypes, buildHashKeys, operatorCount, new OperatorConfig());
        OmniLookupJoinWithExprOperatorFactory.FactoryContext lookupJoinOperatorFactory1 =
                new OmniLookupJoinWithExprOperatorFactory.FactoryContext(
                probeTypes, probeOutputCols, probeHashKeys, buildOutputCols, buildOutputTypes,
                omniHashBuilderWithExprOperatorFactory, Optional.empty(), false, new OperatorConfig());
        OmniLookupJoinWithExprOperatorFactory.FactoryContext lookupJoinOperatorFactory2 =
                new OmniLookupJoinWithExprOperatorFactory.FactoryContext(
                probeTypes, probeOutputCols, probeHashKeys, buildOutputCols, buildOutputTypes,
                omniHashBuilderWithExprOperatorFactory, Optional.empty(), false, new OperatorConfig());
        OmniLookupJoinWithExprOperatorFactory.FactoryContext lookupJoinOperatorFactory3 = null;

        assertEquals(lookupJoinOperatorFactory2, lookupJoinOperatorFactory1);
        assertEquals(lookupJoinOperatorFactory1, lookupJoinOperatorFactory1);
        assertNotEquals(lookupJoinOperatorFactory3, lookupJoinOperatorFactory1);
    }

    private void buildLookupJoinExpectedData(Object[][] lookupJoinData1, Object[][] lookupJoinData2,
            Object[][] lookupJoinData3, int tableSize, int maxRowCount) {
        for (int i = 0; i < maxRowCount; i++) {
            lookupJoinData1[0][i] = tableSize / 2 + i + 1L;
            lookupJoinData1[1][i] = i + 200L;
            lookupJoinData1[2][i] = i + 201L;
            lookupJoinData1[3][i] = i + 202L;

            lookupJoinData1[4][i] = tableSize / 2 + i + 1L;
            lookupJoinData1[5][i] = tableSize / 2 + i + 100L;
            lookupJoinData1[6][i] = tableSize / 2 + i + 101L;
            lookupJoinData1[7][i] = tableSize / 2 + i + 102L;

            lookupJoinData2[0][i] = tableSize / 2 + maxRowCount + i + 1L;
            lookupJoinData2[1][i] = maxRowCount + i + 200L;
            lookupJoinData2[2][i] = maxRowCount + i + 201L;
            lookupJoinData2[3][i] = maxRowCount + i + 202L;

            if (i < 6) {
                lookupJoinData2[4][i] = tableSize / 2 + maxRowCount + i + 1L;
                lookupJoinData2[5][i] = tableSize / 2 + maxRowCount + i + 100L;
                lookupJoinData2[6][i] = tableSize / 2 + maxRowCount + i + 101L;
                lookupJoinData2[7][i] = tableSize / 2 + maxRowCount + i + 102L;
            } else {
                lookupJoinData2[4][i] = null;
                lookupJoinData2[5][i] = null;
                lookupJoinData2[6][i] = null;
                lookupJoinData2[7][i] = null;
            }
        }

        for (int i = 0; i < tableSize - 2 * maxRowCount; i++) {
            lookupJoinData3[0][i] = tableSize / 2 + 2 * maxRowCount + i + 1L;
            lookupJoinData3[1][i] = 2 * maxRowCount + i + 200L;
            lookupJoinData3[2][i] = 2 * maxRowCount + i + 201L;
            lookupJoinData3[3][i] = 2 * maxRowCount + i + 202L;
            lookupJoinData3[4][i] = null;
            lookupJoinData3[5][i] = null;
            lookupJoinData3[6][i] = null;
            lookupJoinData3[7][i] = null;
        }
    }

    private void buildFullOuterExpectedData(Object[][] fullOuterData1, Object[][] fullOuterData2, int tableSize,
            int maxRowCount) {
        for (int i = 0; i < maxRowCount; i++) {
            fullOuterData1[0][i] = null;
            fullOuterData1[1][i] = null;
            fullOuterData1[2][i] = null;
            fullOuterData1[3][i] = null;
            fullOuterData1[4][i] = i + 1L;
            fullOuterData1[5][i] = i + 100L;
            fullOuterData1[6][i] = i + 101L;
            fullOuterData1[7][i] = i + 102L;
        }

        for (int i = 0; i < tableSize / 2 - maxRowCount; i++) {
            fullOuterData2[0][i] = null;
            fullOuterData2[1][i] = null;
            fullOuterData2[2][i] = null;
            fullOuterData2[3][i] = null;
            fullOuterData2[4][i] = maxRowCount + i + 1L;
            fullOuterData2[5][i] = maxRowCount + i + 100L;
            fullOuterData2[6][i] = maxRowCount + i + 101L;
            fullOuterData2[7][i] = maxRowCount + i + 102L;
        }
    }

    private void buildFullOuterAddInputData(Object[][] buildData, Object[][] probeData) {
        int tableSize = buildData[0].length;
        for (int i = 0; i < tableSize; i++) {
            buildData[0][i] = i + 1L;
            buildData[1][i] = i + 100L;
            buildData[2][i] = i + 101L;
            buildData[3][i] = i + 102L;
            probeData[0][i] = tableSize / 2 + i + 1L;
            probeData[1][i] = i + 200L;
            probeData[2][i] = i + 201L;
            probeData[3][i] = i + 202L;
        }
    }

    /**
     * Test full hash join multiple build batch.
     */
    @Test
    public void testFullOuterJoinIterativeGetOutput() {
        DataType[] buildTypes = {LongDataType.LONG, LongDataType.LONG, LongDataType.LONG, LongDataType.LONG};
        String[] buildHashKeys = {getOmniJsonFieldReference(2, 0)};
        OmniHashBuilderWithExprOperatorFactory hashBuilderOperatorFactory = new OmniHashBuilderWithExprOperatorFactory(
                OMNI_JOIN_TYPE_FULL, buildTypes, buildHashKeys, 1);
        OmniOperator hashBuilderOperator = hashBuilderOperatorFactory.createOperator();
        int tableSize = 32780;
        Object[][] buildData = new Object[4][tableSize];
        Object[][] probeData = new Object[4][tableSize];
        buildFullOuterAddInputData(buildData, probeData);
        hashBuilderOperator.addInput(createVecBatch(buildTypes, buildData));
        hashBuilderOperator.getOutput();

        DataType[] probeTypes = {LongDataType.LONG, LongDataType.LONG, LongDataType.LONG, LongDataType.LONG};
        int[] probeOutputCols = {0, 1, 2, 3};
        String[] probeHashKeys = {getOmniJsonFieldReference(2, 0)};
        int[] buildOutputCols = {0, 1, 2, 3};
        OmniLookupJoinWithExprOperatorFactory lookupJoinOperatorFactory = new OmniLookupJoinWithExprOperatorFactory(
                probeTypes, probeOutputCols, probeHashKeys,
                buildOutputCols, buildTypes, hashBuilderOperatorFactory, false);
        OmniOperator lookupJoinOperator = lookupJoinOperatorFactory.createOperator();

        OmniLookupOuterJoinWithExprOperatorFactory lookupOuterJoinOperatorFactory =
                new OmniLookupOuterJoinWithExprOperatorFactory(
                probeTypes, probeOutputCols, probeHashKeys, buildOutputCols, buildTypes, hashBuilderOperatorFactory,
                new OperatorConfig());
        OmniOperator lookupOuterJoinOperator = lookupOuterJoinOperatorFactory.createOperator();
        VecBatch probeVecBatch = createVecBatch(probeTypes, probeData);
        lookupJoinOperator.addInput(probeVecBatch);

        VecBatch vecBatch = null;
        List<VecBatch> lookupJoinList = new ArrayList<>();
        Iterator<VecBatch> resultIterator = lookupJoinOperator.getOutput();
        while (resultIterator.hasNext()) {
            vecBatch = resultIterator.next();
            lookupJoinList.add(vecBatch);
        }

        List<VecBatch> fullOuterList = new ArrayList<>();
        Iterator<VecBatch> fullOuterIterator = lookupOuterJoinOperator.getOutput();
        while (fullOuterIterator.hasNext()) {
            vecBatch = fullOuterIterator.next();
            fullOuterList.add(vecBatch);
        }

        int maxRowCount = 16384; // 1M / (8 * 8)
        Object[][] lookupJoinData1 = new Object[8][maxRowCount];
        Object[][] lookupJoinData2 = new Object[8][maxRowCount];
        Object[][] lookupJoinData3 = new Object[8][tableSize - 2 * maxRowCount];
        Object[][] fullOuterData1 = new Object[8][maxRowCount];
        Object[][] fullOuterData2 = new Object[8][tableSize / 2 - maxRowCount];

        buildLookupJoinExpectedData(lookupJoinData1, lookupJoinData2, lookupJoinData3, tableSize, maxRowCount);
        buildFullOuterExpectedData(fullOuterData1, fullOuterData2, tableSize, maxRowCount);

        assertEquals(lookupJoinList.size(), 3);
        assertEquals(fullOuterList.size(), 2);

        assertVecBatchEqualsIgnoreOrder(lookupJoinList.get(0), lookupJoinData1);
        assertVecBatchEqualsIgnoreOrder(lookupJoinList.get(1), lookupJoinData2);
        assertVecBatchEqualsIgnoreOrder(lookupJoinList.get(2), lookupJoinData3);
        assertEquals(fullOuterList.get(0).getVector(4).getSize(), maxRowCount);
        assertEquals(fullOuterList.get(1).getVector(4).getSize(), tableSize / 2 - maxRowCount);

        lookupJoinOperator.close();
        hashBuilderOperator.close();
        lookupOuterJoinOperator.close();
        lookupJoinOperatorFactory.close();
        hashBuilderOperatorFactory.close();
        lookupOuterJoinOperatorFactory.close();

        freeVecBatches(lookupJoinList);
        freeVecBatches(fullOuterList);
    }
}
