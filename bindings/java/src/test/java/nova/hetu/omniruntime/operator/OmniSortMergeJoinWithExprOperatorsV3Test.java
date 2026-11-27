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
import static nova.hetu.omniruntime.util.TestUtils.createVecBatch;
import static nova.hetu.omniruntime.util.TestUtils.freeVecBatch;
import static nova.hetu.omniruntime.util.TestUtils.getOmniJsonFieldReference;
import static nova.hetu.omniruntime.util.TestUtils.getOmniJsonLiteral;
import static nova.hetu.omniruntime.util.TestUtils.omniFunctionExpr;
import static nova.hetu.omniruntime.util.TestUtils.omniJsonFourArithmeticExpr;
import static org.testng.Assert.assertEquals;
import static org.testng.Assert.assertNotEquals;

import nova.hetu.omniruntime.operator.config.OperatorConfig;
import nova.hetu.omniruntime.operator.join.OmniSmjBufferedTableWithExprOperatorFactoryV3;
import nova.hetu.omniruntime.operator.join.OmniSmjStreamedTableWithExprOperatorFactoryV3;
import nova.hetu.omniruntime.type.DataType;
import nova.hetu.omniruntime.type.DoubleDataType;
import nova.hetu.omniruntime.type.IntDataType;
import nova.hetu.omniruntime.type.LongDataType;
import nova.hetu.omniruntime.utils.OmniRuntimeException;
import nova.hetu.omniruntime.vector.VecBatch;

import org.testng.annotations.Test;

import java.util.Iterator;
import java.util.Optional;

/**
 * The type Omni sort merge join with expression operators test.
 *
 * @since 2022-1-10
 */
public class OmniSortMergeJoinWithExprOperatorsV3Test {
    /**
     * Test inner hash join one column 1.
     */
    @Test
    public void testSmjOneTimeEqualCondition() {
        DataType[] streamedTypes = {IntDataType.INTEGER, LongDataType.LONG};

        String[] streamedKeyExps = {
                omniJsonFourArithmeticExpr("ADD", 1, getOmniJsonFieldReference(1, 0), getOmniJsonLiteral(1, false, 5))};
        int[] streamedOutputCols = {1};
        OmniSmjStreamedTableWithExprOperatorFactoryV3 streamedBuilderWithExprOperatorFactory =
                new OmniSmjStreamedTableWithExprOperatorFactoryV3(
                streamedTypes, streamedKeyExps, streamedOutputCols, OMNI_JOIN_TYPE_INNER, Optional.empty());

        DataType[] bufferedTypes = {LongDataType.LONG, IntDataType.INTEGER};

        int[] bufferedOutputCols = {0};
        String[] bufferedKeyExps = {
                omniJsonFourArithmeticExpr("ADD", 1, getOmniJsonFieldReference(1, 1), getOmniJsonLiteral(1, false, 5))};
        OmniSmjBufferedTableWithExprOperatorFactoryV3 bufferedWithExprOperatorFactory =
                new OmniSmjBufferedTableWithExprOperatorFactoryV3(
                bufferedTypes, bufferedKeyExps, bufferedOutputCols, streamedBuilderWithExprOperatorFactory);
        OmniOperator bufferedTableOperator = bufferedWithExprOperatorFactory.createOperator();

        // start to add input
        Object[][] streamedDatas1 = {{0, 1, 2, 3, 4, 5}, {6600L, 5500L, 4400L, 3300L, 2200L, 1100L}};
        VecBatch streamedVecBatch1 = createVecBatch(streamedTypes, streamedDatas1);
        OmniOperator streamedTableOperator = streamedBuilderWithExprOperatorFactory.createOperator();
        streamedTableOperator.addInput(streamedVecBatch1);

        Object[][] bufferedDatas1 = {{6006L, 5005L, 4004L, 3003L, 2002L, 1001L}, {0, 1, 2, 3, 4, 5}};
        VecBatch bufferedVecBatch1 = createVecBatch(bufferedTypes, bufferedDatas1);
        bufferedTableOperator.addInput(bufferedVecBatch1);

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
        OmniSmjStreamedTableWithExprOperatorFactoryV3 streamedBuilderWithExprOperatorFactory =
                new OmniSmjStreamedTableWithExprOperatorFactoryV3(
                streamedTypes, streamedKeyExps, streamedOutputCols, OMNI_JOIN_TYPE_INNER, Optional.empty());
    }

    @Test(expectedExceptions = OmniRuntimeException.class, expectedExceptionsMessageRegExp =
            ".*EXPRESSION_NOT_SUPPORT.*")
    public void testInvalidBufferedKeys() {
        DataType[] streamedTypes = {IntDataType.INTEGER, LongDataType.LONG};
        String[] streamedKeyExps = {
                omniJsonFourArithmeticExpr("ADD", 1, getOmniJsonFieldReference(1, 0), getOmniJsonLiteral(1, false, 5))};
        int[] streamedOutputCols = {1};
        OmniSmjStreamedTableWithExprOperatorFactoryV3 streamedBuilderWithExprOperatorFactory =
                new OmniSmjStreamedTableWithExprOperatorFactoryV3(
                streamedTypes, streamedKeyExps, streamedOutputCols, OMNI_JOIN_TYPE_INNER, Optional.empty());

        DataType[] bufferedTypes = {LongDataType.LONG, IntDataType.INTEGER};
        int[] bufferedOutputCols = {0};
        String[] bufferedKeyExps = {omniFunctionExpr("abc", 2, getOmniJsonFieldReference(2, 1))};
        OmniSmjBufferedTableWithExprOperatorFactoryV3 bufferedWithExprOperatorFactory =
                new OmniSmjBufferedTableWithExprOperatorFactoryV3(
                bufferedTypes, bufferedKeyExps, bufferedOutputCols, streamedBuilderWithExprOperatorFactory);
    }

    @Test
    public void testFactoryContextEquals() {
        DataType[] streamedTypes = {IntDataType.INTEGER, LongDataType.LONG};

        String[] streamedKeyExps = {
                omniJsonFourArithmeticExpr("ADD", 1, getOmniJsonFieldReference(1, 0), getOmniJsonLiteral(1, false, 5))};
        int[] streamedOutputCols = {1};
        OmniSmjStreamedTableWithExprOperatorFactoryV3.FactoryContext streamedBuilderWithExprOperatorFactory1 =
                new OmniSmjStreamedTableWithExprOperatorFactoryV3.FactoryContext(
                streamedTypes, streamedKeyExps, streamedOutputCols, OMNI_JOIN_TYPE_INNER, Optional.empty(),
                new OperatorConfig());
        OmniSmjStreamedTableWithExprOperatorFactoryV3.FactoryContext streamedBuilderWithExprOperatorFactory2 =
                new OmniSmjStreamedTableWithExprOperatorFactoryV3.FactoryContext(
                streamedTypes, streamedKeyExps, streamedOutputCols, OMNI_JOIN_TYPE_INNER, Optional.empty(),
                new OperatorConfig());
        OmniSmjStreamedTableWithExprOperatorFactoryV3.FactoryContext streamedBuilderWithExprOperatorFactory3 = null;
        assertEquals(streamedBuilderWithExprOperatorFactory2, streamedBuilderWithExprOperatorFactory1);
        assertEquals(streamedBuilderWithExprOperatorFactory1, streamedBuilderWithExprOperatorFactory1);
        assertNotEquals(streamedBuilderWithExprOperatorFactory3, streamedBuilderWithExprOperatorFactory1);

        DataType[] bufferedTypes = {LongDataType.LONG, IntDataType.INTEGER};

        OmniSmjStreamedTableWithExprOperatorFactoryV3 streamedBuilderWithExprOperatorFactory =
                new OmniSmjStreamedTableWithExprOperatorFactoryV3(
                streamedTypes, streamedKeyExps, streamedOutputCols, OMNI_JOIN_TYPE_INNER, Optional.empty());

        int[] bufferedOutputCols = {0};
        String[] bufferedKeyExps = {
                omniJsonFourArithmeticExpr("ADD", 1, getOmniJsonFieldReference(1, 0), getOmniJsonLiteral(1, false, 5))};
        OmniSmjBufferedTableWithExprOperatorFactoryV3.FactoryContext bufferedWithExprOperatorFactory1 =
                new OmniSmjBufferedTableWithExprOperatorFactoryV3.FactoryContext(
                bufferedTypes, bufferedKeyExps, bufferedOutputCols, new OperatorConfig(),
                streamedBuilderWithExprOperatorFactory);
        OmniSmjBufferedTableWithExprOperatorFactoryV3.FactoryContext bufferedWithExprOperatorFactory2 =
                new OmniSmjBufferedTableWithExprOperatorFactoryV3.FactoryContext(
                bufferedTypes, bufferedKeyExps, bufferedOutputCols, new OperatorConfig(),
                streamedBuilderWithExprOperatorFactory);
        OmniSmjBufferedTableWithExprOperatorFactoryV3.FactoryContext bufferedWithExprOperatorFactory3 = null;
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
        OmniSmjStreamedTableWithExprOperatorFactoryV3 streamedBuilderWithExprOperatorFactory =
                new OmniSmjStreamedTableWithExprOperatorFactoryV3(
                streamedTypes, streamedKeyExps, streamedOutputCols, OMNI_JOIN_TYPE_LEFT_SEMI, Optional.empty());

        DataType[] bufferedTypes = {LongDataType.LONG, IntDataType.INTEGER};
        int[] bufferedOutputCols = {};
        String[] bufferedKeyExps = {
                omniJsonFourArithmeticExpr("ADD", 1, getOmniJsonFieldReference(1, 1), getOmniJsonLiteral(1, false, 5))};
        OmniSmjBufferedTableWithExprOperatorFactoryV3 bufferedWithExprOperatorFactory =
                new OmniSmjBufferedTableWithExprOperatorFactoryV3(
                bufferedTypes, bufferedKeyExps, bufferedOutputCols, streamedBuilderWithExprOperatorFactory);
        OmniOperator bufferedTableOperator = bufferedWithExprOperatorFactory.createOperator();

        // start to add input
        Object[][] streamedDatas1 = {{0, 1, 2, 2, 2, 3, 4, 5},
                {8800L, 7700L, 6600L, 5500L, 4400L, 3300L, 2200L, 1100L}};
        VecBatch streamedVecBatch1 = createVecBatch(streamedTypes, streamedDatas1); // 0, 1, 2, 2, 2, 3, 4, 5
        OmniOperator streamedTableOperator = streamedBuilderWithExprOperatorFactory.createOperator();
        streamedTableOperator.addInput(streamedVecBatch1);

        Object[][] bufferedDatas1 = {{8008L, 7007L, 6006L, 5005L, 4004L, 3003L, 2002L, 1001L},
                {0, 1, 2, 2, 3, 3, 4, 5}};
        VecBatch bufferedVecBatch1 = createVecBatch(bufferedTypes, bufferedDatas1); // 0, 1, 2, 2, 3, 3, 4, 5
        bufferedTableOperator.addInput(bufferedVecBatch1);

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
        OmniSmjStreamedTableWithExprOperatorFactoryV3 streamedBuilderWithExprOperatorFactory =
                new OmniSmjStreamedTableWithExprOperatorFactoryV3(
                streamedTypes, streamedKeyExps, streamedOutputCols, OMNI_JOIN_TYPE_LEFT_ANTI, Optional.empty());

        DataType[] bufferedTypes = {IntDataType.INTEGER, DoubleDataType.DOUBLE};
        int[] bufferedOutputCols = {};
        String[] bufferedKeyExps = {
                omniJsonFourArithmeticExpr("ADD", 1, getOmniJsonFieldReference(1, 0), getOmniJsonLiteral(1, false, 5))};
        OmniSmjBufferedTableWithExprOperatorFactoryV3 bufferedWithExprOperatorFactory =
                new OmniSmjBufferedTableWithExprOperatorFactoryV3(
                bufferedTypes, bufferedKeyExps, bufferedOutputCols, streamedBuilderWithExprOperatorFactory);
        OmniOperator bufferedTableOperator = bufferedWithExprOperatorFactory.createOperator();

        // start to add input
        Object[][] streamedDatas1 = {{1, 1, 2, 3}, {40L, 25L, 35L, 30L}};
        VecBatch streamedVecBatch1 = createVecBatch(streamedTypes, streamedDatas1);
        OmniOperator streamedTableOperator = streamedBuilderWithExprOperatorFactory.createOperator();
        streamedTableOperator.addInput(streamedVecBatch1);

        Object[][] bufferedDatas1 = {{3, 3, 4, 4}, {3.3, 3.5, 4.4, 4.5}};
        VecBatch bufferedVecBatch1 = createVecBatch(bufferedTypes, bufferedDatas1);
        bufferedTableOperator.addInput(bufferedVecBatch1);

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
}
