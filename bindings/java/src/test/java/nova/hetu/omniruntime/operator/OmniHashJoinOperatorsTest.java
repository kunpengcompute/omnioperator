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
import static nova.hetu.omniruntime.constants.JoinType.OMNI_JOIN_TYPE_INNER;
import static nova.hetu.omniruntime.constants.JoinType.OMNI_JOIN_TYPE_LEFT;
import static nova.hetu.omniruntime.util.TestUtils.assertDecimal128VecEqualsIgnoreOrder;
import static nova.hetu.omniruntime.util.TestUtils.assertVecBatchEqualsIgnoreOrder;
import static nova.hetu.omniruntime.util.TestUtils.assertVecEqualsIgnoreOrder;
import static nova.hetu.omniruntime.util.TestUtils.createDictionaryVec;
import static nova.hetu.omniruntime.util.TestUtils.createLongVec;
import static nova.hetu.omniruntime.util.TestUtils.createVec;
import static nova.hetu.omniruntime.util.TestUtils.createVecBatch;
import static nova.hetu.omniruntime.util.TestUtils.freeVecBatch;
import static nova.hetu.omniruntime.util.TestUtils.getOmniJsonFieldReference;
import static nova.hetu.omniruntime.util.TestUtils.getOmniJsonLiteral;
import static nova.hetu.omniruntime.util.TestUtils.omniFunctionExpr;
import static nova.hetu.omniruntime.util.TestUtils.omniJsonGreaterThanExpr;
import static nova.hetu.omniruntime.util.TestUtils.omniJsonNotEqualExpr;
import static org.testng.Assert.assertEquals;
import static org.testng.Assert.assertNotEquals;
import static org.testng.Assert.assertTrue;

import com.google.common.collect.ImmutableList;

import nova.hetu.omniruntime.operator.config.OperatorConfig;
import nova.hetu.omniruntime.operator.join.OmniHashBuilderOperatorFactory;
import nova.hetu.omniruntime.operator.join.OmniLookupJoinOperatorFactory;
import nova.hetu.omniruntime.operator.join.OmniLookupOuterJoinOperatorFactory;
import nova.hetu.omniruntime.type.CharDataType;
import nova.hetu.omniruntime.type.DataType;
import nova.hetu.omniruntime.type.Date32DataType;
import nova.hetu.omniruntime.type.Decimal128DataType;
import nova.hetu.omniruntime.type.Decimal64DataType;
import nova.hetu.omniruntime.type.IntDataType;
import nova.hetu.omniruntime.type.LongDataType;
import nova.hetu.omniruntime.type.VarcharDataType;
import nova.hetu.omniruntime.utils.OmniRuntimeException;
import nova.hetu.omniruntime.vector.LongVec;
import nova.hetu.omniruntime.vector.Vec;
import nova.hetu.omniruntime.vector.VecBatch;

import org.testng.annotations.Test;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.Optional;

/**
 * The type Omni hash join operators test.
 *
 * @since 2021-6-2
 */
public class OmniHashJoinOperatorsTest {
    /**
     * The Page distinct count.
     */
    int pageDistinctCount = 4;

    /**
     * The Page distinct value repeat count.
     */
    int pageDistinctValueRepeatCount = 100;

    /**
     * test hash join performance whether with jit or not.
     */
    @Test
    public void testHashJoinComparePref() {
        DataType[] buildTypes = {LongDataType.LONG, LongDataType.LONG};
        int[] buildHashCols = {0};
        int operatorCount = 1;
        int buildPageCount = 10;

        OmniHashBuilderOperatorFactory hashBuilderOperatorFactoryWithoutJit = new OmniHashBuilderOperatorFactory(
                OMNI_JOIN_TYPE_INNER, buildTypes, buildHashCols, operatorCount, new OperatorConfig());
        OmniOperator hashBuilderOperatorWithoutJit = hashBuilderOperatorFactoryWithoutJit.createOperator();
        ImmutableList<VecBatch> buildVecsWithoutJit = buildVecs(buildPageCount);

        long start = System.currentTimeMillis();
        for (VecBatch vec : buildVecsWithoutJit) {
            hashBuilderOperatorWithoutJit.addInput(vec);
        }
        Iterator<VecBatch> hashBuilderOutputWithoutJit = hashBuilderOperatorWithoutJit.getOutput();
        long end = System.currentTimeMillis();
        System.out.println("HashBuilder without jit use " + (end - start) + " ms.");

        DataType[] probeTypes = {LongDataType.LONG, LongDataType.LONG};
        int[] probeOutputCols = {1};
        int[] probeHashCols = {0};
        int[] buildOutputCols = {1};
        DataType[] buildOutputTypes = {LongDataType.LONG};
        int probePageCount = 1;

        OmniLookupJoinOperatorFactory lookupJoinOperatorFactoryWithoutJit = new OmniLookupJoinOperatorFactory(
                probeTypes, probeOutputCols, probeHashCols, buildOutputCols, buildOutputTypes,
                hashBuilderOperatorFactoryWithoutJit, Optional.empty(), false, new OperatorConfig());
        OmniOperator lookupJoinOperatorWithoutJit = lookupJoinOperatorFactoryWithoutJit.createOperator();
        ImmutableList<VecBatch> probeVecsWithoutJit = buildVecs(probePageCount);

        start = System.currentTimeMillis();
        lookupJoinOperatorWithoutJit.addInput(probeVecsWithoutJit.get(0));
        Iterator<VecBatch> lookupJoinOutputWithoutJit = lookupJoinOperatorWithoutJit.getOutput();
        end = System.currentTimeMillis();
        System.out.println("LookupJoin without jit use " + (end - start) + " ms.");

        OmniHashBuilderOperatorFactory hashBuilderOperatorFactoryWithJit = new OmniHashBuilderOperatorFactory(
                OMNI_JOIN_TYPE_INNER, buildTypes, buildHashCols, operatorCount, new OperatorConfig());
        OmniOperator hashBuilderOperatorWithJit = hashBuilderOperatorFactoryWithJit.createOperator();
        ImmutableList<VecBatch> buildVecsWithJit = buildVecs(buildPageCount);

        start = System.currentTimeMillis();
        for (VecBatch vec : buildVecsWithJit) {
            hashBuilderOperatorWithJit.addInput(vec);
        }
        Iterator<VecBatch> hashBuilderOutputWithJit = hashBuilderOperatorWithJit.getOutput();
        end = System.currentTimeMillis();
        System.out.println("HashBuilder with jit use " + (end - start) + " ms.");

        OmniLookupJoinOperatorFactory lookupJoinOperatorFactoryWithJit = new OmniLookupJoinOperatorFactory(probeTypes,
                probeOutputCols, probeHashCols, buildOutputCols, buildOutputTypes,
                hashBuilderOperatorFactoryWithJit, Optional.empty(), false, new OperatorConfig());
        OmniOperator lookupJoinOperatorWithJit = lookupJoinOperatorFactoryWithJit.createOperator();
        ImmutableList<VecBatch> probeVecsWithJit = buildVecs(probePageCount);

        start = System.currentTimeMillis();
        lookupJoinOperatorWithJit.addInput(probeVecsWithJit.get(0));
        Iterator<VecBatch> lookupJoinOutputWithJit = lookupJoinOperatorWithJit.getOutput();
        end = System.currentTimeMillis();
        System.out.println("LookupJoin with jit use " + (end - start) + " ms.");

        while (hashBuilderOutputWithoutJit.hasNext()) {
            VecBatch resultWithoutJit = hashBuilderOutputWithoutJit.next();
            VecBatch resultWithJit = hashBuilderOutputWithJit.next();
            assertVecBatchEqualsIgnoreOrder(resultWithoutJit, resultWithJit);
            freeVecBatch(resultWithoutJit);
            freeVecBatch(resultWithJit);
        }

        while (lookupJoinOutputWithoutJit.hasNext() && lookupJoinOutputWithJit.hasNext()) {
            VecBatch resultWithoutJit = lookupJoinOutputWithoutJit.next();
            VecBatch resultWithJit = lookupJoinOutputWithJit.next();
            assertVecBatchEqualsIgnoreOrder(resultWithoutJit, resultWithJit);
            freeVecBatch(resultWithoutJit);
            freeVecBatch(resultWithJit);
        }

        lookupJoinOperatorWithoutJit.close();
        hashBuilderOperatorWithoutJit.close();
        lookupJoinOperatorWithJit.close();
        hashBuilderOperatorWithJit.close();
        lookupJoinOperatorFactoryWithoutJit.close();
        hashBuilderOperatorFactoryWithoutJit.close();
        lookupJoinOperatorFactoryWithJit.close();
        hashBuilderOperatorFactoryWithJit.close();
    }

    /**
     * Test inner hash join one column 1.
     */
    @Test
    public void testInnerEqualityJoinOneColumn1() {
        DataType[] buildTypes = {LongDataType.LONG, LongDataType.LONG};
        Object[][] buildDatas = {{1L, 2L, 1L, 2L, 3L, 4L, 5L, 6L, 7L, 1L},
                {79L, 79L, 70L, 70L, 70L, 70L, 70L, 70L, 70L, 70L}};
        VecBatch buildVecBatch = createVecBatch(buildTypes, buildDatas);

        int[] buildHashCols = {0};
        int operatorCount = 1;
        OmniHashBuilderOperatorFactory hashBuilderOperatorFactory = new OmniHashBuilderOperatorFactory(
                OMNI_JOIN_TYPE_INNER, buildTypes, buildHashCols, operatorCount);
        OmniOperator hashBuilderOperator = hashBuilderOperatorFactory.createOperator();
        hashBuilderOperator.addInput(buildVecBatch);
        hashBuilderOperator.getOutput();

        DataType[] probeTypes = {LongDataType.LONG, LongDataType.LONG};
        Object[][] probeDatas = {{1L, 2L, 3L, 4L, 5L, 6L, 1L, 1L, 2L, 3L},
                {78L, 78L, 78L, 78L, 78L, 78L, 78L, 82L, 82L, 65L}};
        VecBatch probeVecBatch = createVecBatch(probeTypes, probeDatas);

        int[] probeOutputCols = {1};
        int[] probeHashCols = {0};
        int[] buildOutputCols = {1};
        DataType[] buildOutputTypes = {LongDataType.LONG};
        OmniLookupJoinOperatorFactory lookupJoinOperatorFactory = new OmniLookupJoinOperatorFactory(probeTypes,
                probeOutputCols, probeHashCols, buildOutputCols, buildOutputTypes,
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
     * Test inner hash join one column 2.
     */
    @Test(enabled = false)
    public void testInnerEqualityJoinOneColumn2() {
        DataType[] buildTypes = {LongDataType.LONG, LongDataType.LONG};
        Object[][] buildDatas1 = {{1L, 1L, 3L, 6L, 7L, 1L}, {79L, 70L, 70L, 70L, 70L, 70L}};
        VecBatch buildVecBatch1 = createVecBatch(buildTypes, buildDatas1);
        Object[][] buildDatas2 = {{2L, 2L, 4L, 5L}, {79L, 70L, 70L, 70L}};
        VecBatch buildVecBatch2 = createVecBatch(buildTypes, buildDatas2);

        int[] buildHashCols = {0};
        int operatorCount = 2;
        OmniHashBuilderOperatorFactory hashBuilderOperatorFactory = new OmniHashBuilderOperatorFactory(
                OMNI_JOIN_TYPE_INNER, buildTypes, buildHashCols, operatorCount);
        OmniOperator hashBuilderOperator1 = hashBuilderOperatorFactory.createOperator();
        hashBuilderOperator1.addInput(buildVecBatch1);
        hashBuilderOperator1.getOutput();
        OmniOperator hashBuilderOperator2 = hashBuilderOperatorFactory.createOperator();
        hashBuilderOperator2.addInput(buildVecBatch2);
        hashBuilderOperator2.getOutput();

        DataType[] probeTypes = {LongDataType.LONG, LongDataType.LONG};
        Object[][] probeDatas = {{1L, 2L, 3L, 4L, 5L, 6L, 1L, 1L, 2L, 3L},
                {78L, 78L, 78L, 78L, 78L, 78L, 78L, 82L, 82L, 65L}};
        VecBatch probeVecBatch = createVecBatch(probeTypes, probeDatas);

        int[] probeOutputCols = {1};
        int[] probeHashCols = {0};
        int[] buildOutputCols = {1};
        DataType[] buildOutputTypes = {LongDataType.LONG};
        OmniLookupJoinOperatorFactory lookupJoinOperatorFactory = new OmniLookupJoinOperatorFactory(probeTypes,
                probeOutputCols, probeHashCols, buildOutputCols, buildOutputTypes,
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
        hashBuilderOperator1.close();
        hashBuilderOperator2.close();
        lookupJoinOperatorFactory.close();
        hashBuilderOperatorFactory.close();
    }

    /**
     * Test left join
     */
    @Test
    public void testLeftEqualityJoin() {
        DataType[] buildTypes = {LongDataType.LONG, LongDataType.LONG};
        Object[][] buildDatas = {{1L, 2L, 3L, 4L}, {111L, 11L, 333L, 33L}};
        VecBatch buildVecBatch = createVecBatch(buildTypes, buildDatas);

        int[] buildHashCols = {1};
        int operatorCount = 1;
        OmniHashBuilderOperatorFactory hashBuilderOperatorFactory = new OmniHashBuilderOperatorFactory(
                OMNI_JOIN_TYPE_LEFT, buildTypes, buildHashCols, operatorCount);
        OmniOperator hashBuilderOperator = hashBuilderOperatorFactory.createOperator();
        hashBuilderOperator.addInput(buildVecBatch);
        hashBuilderOperator.getOutput();

        DataType[] probeTypes = {LongDataType.LONG, LongDataType.LONG};
        Object[][] probeDatas = {{1L, 2L, 3L, 4L}, {11L, 22L, 33L, 44L}};
        VecBatch probeVecBatch = createVecBatch(probeTypes, probeDatas);

        int[] probeOutputCols = {0, 1};
        int[] probeHashCols = {1};
        int[] buildOutputCols = {0, 1};
        DataType[] buildOutputTypes = {LongDataType.LONG, LongDataType.LONG};
        OmniLookupJoinOperatorFactory lookupJoinOperatorFactory = new OmniLookupJoinOperatorFactory(probeTypes,
                probeOutputCols, probeHashCols, buildOutputCols, buildOutputTypes,
                hashBuilderOperatorFactory, false);
        OmniOperator lookupJoinOperator = lookupJoinOperatorFactory.createOperator();
        lookupJoinOperator.addInput(probeVecBatch);
        Iterator<VecBatch> results = lookupJoinOperator.getOutput();

        VecBatch resultVecBatch = results.next();
        int len = resultVecBatch.getRowCount();
        assertEquals(len, 4);
        assertEquals(resultVecBatch.getVectorCount(), 4);
        Object[][] expectedDatas = {{1L, 2L, 3L, 4L}, {11L, 22L, 33L, 44L}, {2L, null, 4L, null},
                {11L, null, 33L, null}};
        assertVecBatchEqualsIgnoreOrder(resultVecBatch, expectedDatas);
        freeVecBatch(resultVecBatch);
        lookupJoinOperator.close();
        hashBuilderOperator.close();
        lookupJoinOperatorFactory.close();
        hashBuilderOperatorFactory.close();
    }

    /**
     * Test left join with varchar join key
     */
    @Test
    public void testLeftEqualityJoinVarchar() {
        DataType[] buildTypes = {LongDataType.LONG, new VarcharDataType(3)};
        Object[][] buildDatas = {{1L, 2L, 3L, 4L}, {"aaa", "11", "ccc", "33"}};
        VecBatch buildVecBatch = createVecBatch(buildTypes, buildDatas);

        int[] buildHashCols = {1};
        int operatorCount = 1;
        OmniHashBuilderOperatorFactory hashBuilderOperatorFactory = new OmniHashBuilderOperatorFactory(
                OMNI_JOIN_TYPE_LEFT, buildTypes, buildHashCols, operatorCount);
        OmniOperator hashBuilderOperator = hashBuilderOperatorFactory.createOperator();
        hashBuilderOperator.addInput(buildVecBatch);
        hashBuilderOperator.getOutput();

        DataType[] probeTypes = {LongDataType.LONG, new VarcharDataType(2)};
        Object[][] probeDatas = {{1L, 2L, 3L, 4L}, {"11", "22", "33", "44"}};
        VecBatch probeVecBatch = createVecBatch(probeTypes, probeDatas);

        int[] probeOutputCols = {0, 1};
        int[] probeHashCols = {1};
        int[] buildOutputCols = {0, 1};
        DataType[] buildOutputTypes = {LongDataType.LONG, new VarcharDataType(2)};
        OmniLookupJoinOperatorFactory lookupJoinOperatorFactory = new OmniLookupJoinOperatorFactory(probeTypes,
                probeOutputCols, probeHashCols, buildOutputCols, buildOutputTypes,
                hashBuilderOperatorFactory, false);
        OmniOperator lookupJoinOperator = lookupJoinOperatorFactory.createOperator();
        lookupJoinOperator.addInput(probeVecBatch);
        Iterator<VecBatch> results = lookupJoinOperator.getOutput();

        VecBatch resultVecBatch = results.next();
        int len = resultVecBatch.getRowCount();
        assertEquals(len, 4);
        assertEquals(resultVecBatch.getVectorCount(), 4);
        Object[][] expectedDatas = {{1L, 2L, 3L, 4L}, {"11", "22", "33", "44"}, {2L, null, 4L, null},
                {"11", null, "33", null}};
        assertVecBatchEqualsIgnoreOrder(resultVecBatch, expectedDatas);
        freeVecBatch(resultVecBatch);
        lookupJoinOperator.close();
        hashBuilderOperator.close();
        lookupJoinOperatorFactory.close();
        hashBuilderOperatorFactory.close();
    }

    /**
     * Test left join with char join key
     */
    @Test
    public void testLeftEqualityJoinChar() {
        DataType[] buildTypes = {LongDataType.LONG, new CharDataType(3)};
        Object[][] buildDatas = {{1L, 2L, 3L, 4L}, {"aaa", "11", "ccc", "33"}};
        VecBatch buildVecBatch = createVecBatch(buildTypes, buildDatas);

        int[] buildJoinCols = {1};
        int operatorCount = 1;
        OmniHashBuilderOperatorFactory hashBuilderOperatorFactory = new OmniHashBuilderOperatorFactory(
                OMNI_JOIN_TYPE_LEFT, buildTypes, buildJoinCols, operatorCount);
        OmniOperator hashBuilderOperator = hashBuilderOperatorFactory.createOperator();
        hashBuilderOperator.addInput(buildVecBatch);
        hashBuilderOperator.getOutput();

        DataType[] probeTypes = {LongDataType.LONG, new CharDataType(2)};
        Object[][] probeDatas = {{1L, 2L, 3L, 4L}, {"11", "22", "33", "44"}};
        VecBatch probeVecBatch = createVecBatch(probeTypes, probeDatas);

        int[] probeOutputCols = {0, 1};
        int[] probeHashCols = {1};
        int[] buildOutputCols = {0, 1};
        DataType[] buildOutputTypes = {LongDataType.LONG, new CharDataType(2)};
        OmniLookupJoinOperatorFactory lookupJoinOperatorFactory = new OmniLookupJoinOperatorFactory(probeTypes,
                probeOutputCols, probeHashCols, buildOutputCols, buildOutputTypes,
                hashBuilderOperatorFactory, false);
        OmniOperator lookupJoinOperator = lookupJoinOperatorFactory.createOperator();
        lookupJoinOperator.addInput(probeVecBatch);
        Iterator<VecBatch> results = lookupJoinOperator.getOutput();

        VecBatch resultVecBatch = results.next();
        int len = resultVecBatch.getRowCount();
        assertEquals(len, 4);
        assertEquals(resultVecBatch.getVectorCount(), 4);
        Object[][] expectedDatas = {{1L, 2L, 3L, 4L}, {"11", "22", "33", "44"}, {2L, null, 4L, null},
                {"11", null, "33", null}};
        assertVecBatchEqualsIgnoreOrder(resultVecBatch, expectedDatas);
        freeVecBatch(resultVecBatch);
        lookupJoinOperator.close();
        hashBuilderOperator.close();
        lookupJoinOperatorFactory.close();
        hashBuilderOperatorFactory.close();
    }

    /**
     * Test left join with date32 join key
     */
    @Test
    public void testLeftEqualityJoinDate32() {
        DataType[] buildTypes = {LongDataType.LONG, new Date32DataType(DataType.DateUnit.DAY)};
        Object[][] buildDatas = {{1L, 2L, 3L, 4L}, {123, 11, 321, 33}};
        VecBatch buildVecBatch = createVecBatch(buildTypes, buildDatas);

        int[] buildHashCols = {1};
        int operatorCount = 1;
        OmniHashBuilderOperatorFactory hashBuilderOperatorFactory = new OmniHashBuilderOperatorFactory(
                OMNI_JOIN_TYPE_LEFT, buildTypes, buildHashCols, operatorCount);
        OmniOperator hashBuilderOperator = hashBuilderOperatorFactory.createOperator();
        hashBuilderOperator.addInput(buildVecBatch);
        hashBuilderOperator.getOutput();

        DataType[] probeTypes = {LongDataType.LONG, new Date32DataType(DataType.DateUnit.DAY)};
        Object[][] probeDatas = {{1L, 2L, 3L, 4L}, {11, 22, 33, 44}};
        VecBatch probeVecBatch = createVecBatch(probeTypes, probeDatas);

        int[] probeOutputCols = {0, 1};
        int[] probeHashCols = {1};
        int[] buildOutputCols = {0, 1};
        DataType[] buildOutputTypes = {LongDataType.LONG, new Date32DataType(DataType.DateUnit.DAY)};
        OmniLookupJoinOperatorFactory lookupJoinOperatorFactory = new OmniLookupJoinOperatorFactory(probeTypes,
                probeOutputCols, probeHashCols, buildOutputCols, buildOutputTypes,
                hashBuilderOperatorFactory, false);
        OmniOperator lookupJoinOperator = lookupJoinOperatorFactory.createOperator();
        lookupJoinOperator.addInput(probeVecBatch);
        Iterator<VecBatch> results = lookupJoinOperator.getOutput();

        VecBatch resultVecBatch = results.next();
        int len = resultVecBatch.getRowCount();
        assertEquals(len, 4);
        assertEquals(resultVecBatch.getVectorCount(), 4);
        Object[][] expectedDatas = {{1L, 2L, 3L, 4L}, {11, 22, 33, 44}, {2L, null, 4L, null}, {11, null, 33, null}};
        assertVecBatchEqualsIgnoreOrder(resultVecBatch, expectedDatas);
        freeVecBatch(resultVecBatch);
        lookupJoinOperator.close();
        hashBuilderOperator.close();
        lookupJoinOperatorFactory.close();
        hashBuilderOperatorFactory.close();
    }

    /**
     * Test left join with decimal64 join key
     */
    @Test
    public void testLeftEqualityJoinDecimal64() {
        DataType[] buildTypes = {LongDataType.LONG, new Decimal64DataType(3, 0)};
        Object[][] buildDatas = {{1L, 2L, 3L, 4L}, {123L, 11L, 321L, 33L}};
        VecBatch buildVecBatch = createVecBatch(buildTypes, buildDatas);

        int[] buildHashCols = {1};
        int operatorCount = 1;
        OmniHashBuilderOperatorFactory hashBuilderOperatorFactory = new OmniHashBuilderOperatorFactory(
                OMNI_JOIN_TYPE_LEFT, buildTypes, buildHashCols, operatorCount);
        OmniOperator hashBuilderOperator = hashBuilderOperatorFactory.createOperator();
        hashBuilderOperator.addInput(buildVecBatch);
        hashBuilderOperator.getOutput();

        DataType[] probeTypes = {LongDataType.LONG, new Decimal64DataType(2, 0)};
        Object[][] probeDatas = {{1L, 2L, 3L, 4L}, {11L, 22L, 33L, 44L}};
        VecBatch probeVecBatch = createVecBatch(probeTypes, probeDatas);

        int[] probeOutputCols = {0, 1};
        int[] probeHashCols = {1};
        int[] buildOutputCols = {0, 1};
        DataType[] buildOutputTypes = {LongDataType.LONG, new Decimal64DataType(3, 0)};
        OmniLookupJoinOperatorFactory lookupJoinOperatorFactory = new OmniLookupJoinOperatorFactory(probeTypes,
                probeOutputCols, probeHashCols, buildOutputCols, buildOutputTypes,
                hashBuilderOperatorFactory, false);
        OmniOperator lookupJoinOperator = lookupJoinOperatorFactory.createOperator();
        lookupJoinOperator.addInput(probeVecBatch);
        Iterator<VecBatch> results = lookupJoinOperator.getOutput();

        VecBatch resultVecBatch = results.next();
        int len = resultVecBatch.getRowCount();
        assertEquals(len, 4);
        assertEquals(resultVecBatch.getVectorCount(), 4);
        Object[][] expectedDatas = {{1L, 2L, 3L, 4L}, {11L, 22L, 33L, 44L}, {2L, null, 4L, null},
                {11L, null, 33L, null}};
        assertVecBatchEqualsIgnoreOrder(resultVecBatch, expectedDatas);
        freeVecBatch(resultVecBatch);
        lookupJoinOperator.close();
        hashBuilderOperator.close();
        lookupJoinOperatorFactory.close();
        hashBuilderOperatorFactory.close();
    }

    /**
     * Test left join with decimal128 join key
     */
    @Test
    public void testLeftEqualityJoinDecimal128() {
        DataType[] buildTypes = {LongDataType.LONG, new Decimal128DataType(3, 0)};
        Vec[] buildVecs = new Vec[buildTypes.length];
        buildVecs[0] = createVec(buildTypes[0], new Object[]{1L, 2L, 3L, 4L});
        buildVecs[1] = createVec(buildTypes[1], new Object[][]{{123L, 0L}, {11L, 0L}, {321L, 0L}, {33L, 0L}});
        VecBatch buildVecBatch = new VecBatch(buildVecs);

        int[] buildHashCols = {1};
        int operatorCount = 1;
        OmniHashBuilderOperatorFactory hashBuilderOperatorFactory = new OmniHashBuilderOperatorFactory(
                OMNI_JOIN_TYPE_LEFT, buildTypes, buildHashCols, operatorCount);
        OmniOperator hashBuilderOperator = hashBuilderOperatorFactory.createOperator();
        hashBuilderOperator.addInput(buildVecBatch);
        hashBuilderOperator.getOutput();

        DataType[] probeTypes = {LongDataType.LONG, new Decimal128DataType(2, 0)};
        Vec[] probeVecs = new Vec[probeTypes.length];
        probeVecs[0] = createVec(probeTypes[0], new Object[]{1L, 2L, 3L, 4L});
        probeVecs[1] = createVec(probeTypes[1], new Object[][]{{11L, 0L}, {22L, 0L}, {33L, 0L}, {44L, 0L}});
        VecBatch probeVecBatch = new VecBatch(probeVecs);

        int[] probeOutputCols = {0, 1};
        int[] probeHashCols = {1};
        int[] buildOutputCols = {0, 1};
        DataType[] buildOutputTypes = {LongDataType.LONG, new Decimal128DataType(3, 0)};
        OmniLookupJoinOperatorFactory lookupJoinOperatorFactory = new OmniLookupJoinOperatorFactory(probeTypes,
                probeOutputCols, probeHashCols, buildOutputCols, buildOutputTypes,
                hashBuilderOperatorFactory, false);
        OmniOperator lookupJoinOperator = lookupJoinOperatorFactory.createOperator();
        lookupJoinOperator.addInput(probeVecBatch);
        Iterator<VecBatch> results = lookupJoinOperator.getOutput();

        VecBatch resultVecBatch = results.next();
        int len = resultVecBatch.getRowCount();
        assertEquals(len, 4);
        assertEquals(resultVecBatch.getVectorCount(), 4);
        assertVecEqualsIgnoreOrder(resultVecBatch.getVectors()[0], new Object[]{1L, 2L, 3L, 4L});
        assertDecimal128VecEqualsIgnoreOrder(resultVecBatch.getVectors()[1],
                new Long[][]{{11L, 0L}, {22L, 0L}, {33L, 0L}, {44L, 0L}});
        assertVecEqualsIgnoreOrder(resultVecBatch.getVectors()[2], new Object[]{2L, null, 4L, null});
        assertDecimal128VecEqualsIgnoreOrder(resultVecBatch.getVectors()[3], new Long[][]{{11L, 0L}, null, {33L, 0L},
                null});
        freeVecBatch(resultVecBatch);
        lookupJoinOperator.close();
        hashBuilderOperator.close();
        lookupJoinOperatorFactory.close();
        hashBuilderOperatorFactory.close();
    }

    /**
     * Test inner join with dictionary join key
     */
    @Test
    public void testInnerEqualityJoinDictionary() {
        DataType[] buildTypes = {LongDataType.LONG, LongDataType.LONG};
        Object[][] buildDatas = {{1L, null, 3L, null}, {111L, 11L, 333L, 33L}};
        Vec[] vecs = new Vec[2];
        int[] ids = {0, 1, 2, 3};
        vecs[0] = createLongVec(buildDatas[0]);
        vecs[1] = createDictionaryVec(buildTypes[1], buildDatas[1], ids);
        VecBatch buildVecBatch = new VecBatch(vecs);

        int[] buildHashCols = {1};
        int operatorCount = 1;
        OmniHashBuilderOperatorFactory hashBuilderOperatorFactory = new OmniHashBuilderOperatorFactory(
                OMNI_JOIN_TYPE_INNER, buildTypes, buildHashCols, operatorCount);
        OmniOperator hashBuilderOperator = hashBuilderOperatorFactory.createOperator();
        hashBuilderOperator.addInput(buildVecBatch);
        hashBuilderOperator.getOutput();

        DataType[] probeTypes = {LongDataType.LONG, LongDataType.LONG};
        Object[][] probeDatas = {{null, 2L, null, 4L}, {11L, 22L, 33L, 44L}};
        Vec[] probeVecs = new Vec[2];
        probeVecs[0] = createLongVec(probeDatas[0]);
        probeVecs[1] = createDictionaryVec(probeTypes[1], probeDatas[1], ids);
        VecBatch probeVecBatch = new VecBatch(probeVecs);

        int[] probeOutputCols = {0, 1};
        int[] probeHashCols = {1};
        int[] buildOutputCols = {0, 1};
        DataType[] buildOutputTypes = {LongDataType.LONG, LongDataType.LONG};
        OmniLookupJoinOperatorFactory lookupJoinOperatorFactory = new OmniLookupJoinOperatorFactory(probeTypes,
                probeOutputCols, probeHashCols, buildOutputCols, buildOutputTypes,
                hashBuilderOperatorFactory, false);
        OmniOperator lookupJoinOperator = lookupJoinOperatorFactory.createOperator();
        lookupJoinOperator.addInput(probeVecBatch);
        Iterator<VecBatch> results = lookupJoinOperator.getOutput();

        VecBatch resultVecBatch = results.next();
        int len = resultVecBatch.getRowCount();
        assertEquals(len, 2);
        assertEquals(resultVecBatch.getVectorCount(), 4);
        Object[][] expectedDatas = {{null, null}, {11L, 33L}, {null, null}, {11L, 33L}};
        assertVecBatchEqualsIgnoreOrder(resultVecBatch, expectedDatas);
        freeVecBatch(resultVecBatch);
        lookupJoinOperator.close();
        hashBuilderOperator.close();
        lookupJoinOperatorFactory.close();
        hashBuilderOperatorFactory.close();
    }

    /**
     * Test inner join with join filter on int column
     */
    @Test
    public void testInnerEqualityJoinWithIntFilter() {
        DataType[] buildTypes = {IntDataType.INTEGER, IntDataType.INTEGER};
        Object[][] buildDatas = {{19, 14, 7, 19, 1, 20, 10, 13, 20, 16},
                {35709, 31904, 35709, 31904, 35709, null, 35709, 31904, null, 31904}};
        VecBatch buildVecBatch = createVecBatch(buildTypes, buildDatas);

        int[] buildHashCols = {0};
        int operatorCount = 1;
        String filterExpression = omniJsonGreaterThanExpr(getOmniJsonFieldReference(1, 1),
                getOmniJsonFieldReference(1, 3));
        OmniHashBuilderOperatorFactory hashBuilderOperatorFactory = new OmniHashBuilderOperatorFactory(
                OMNI_JOIN_TYPE_INNER, buildTypes, buildHashCols, operatorCount);
        OmniOperator hashBuilderOperator = hashBuilderOperatorFactory.createOperator();
        hashBuilderOperator.addInput(buildVecBatch);
        hashBuilderOperator.getOutput();

        DataType[] probeTypes = {IntDataType.INTEGER, IntDataType.INTEGER};
        Object[][] probeDatas = {{20, 16, 13, 4, 20, 4, 22, 19, 8, 7},
                {35709, 35709, 31904, 12477, null, 38721, 90419, 35709, 88371, null}};
        VecBatch probeVecBatch = createVecBatch(probeTypes, probeDatas);

        int[] probeOutputCols = {0, 1};
        int[] probeHashCols = {0};
        int[] buildOutputCols = {0, 1};
        DataType[] buildOutputTypes = {IntDataType.INTEGER, IntDataType.INTEGER};
        OmniLookupJoinOperatorFactory lookupJoinOperatorFactory = new OmniLookupJoinOperatorFactory(probeTypes,
                probeOutputCols, probeHashCols, buildOutputCols, buildOutputTypes,
                hashBuilderOperatorFactory, Optional.of(filterExpression), false);
        OmniOperator lookupJoinOperator = lookupJoinOperatorFactory.createOperator();
        lookupJoinOperator.addInput(probeVecBatch);
        Iterator<VecBatch> results = lookupJoinOperator.getOutput();

        VecBatch resultVecBatch = results.next();
        int len = resultVecBatch.getRowCount();
        assertEquals(len, 2);
        assertEquals(resultVecBatch.getVectorCount(), 4);
        Object[][] expectedDatas = {{16, 19}, {35709, 35709}, {16, 19}, {31904, 31904}};
        assertVecBatchEqualsIgnoreOrder(resultVecBatch, expectedDatas);
        freeVecBatch(resultVecBatch);
        lookupJoinOperator.close();
        hashBuilderOperator.close();
        lookupJoinOperatorFactory.close();
        hashBuilderOperatorFactory.close();
    }

    /**
     * Test inner join with join filter on varchar column
     */
    @Test
    public void testInnerEqualityJoinWithCharFilter() {
        DataType[] buildTypes = {IntDataType.INTEGER, new VarcharDataType(5)};
        Object[][] buildDatas = {{19, 14, 7, 19, 1, 20, 10, 13, 20, 16},
                {"35709", "31904", "35709", "31904", "35709", "31904", "35709", "31904", "35709", "31904"}};
        VecBatch buildVecBatch = createVecBatch(buildTypes, buildDatas);

        int[] buildHashCols = {0};
        int operatorCount = 1;
        String filterExpression = omniJsonNotEqualExpr(
                omniFunctionExpr("substr", 15,
                        getOmniJsonFieldReference(15, 1) + "," + getOmniJsonLiteral(1, false, 1) + ","
                                + getOmniJsonLiteral(1, false, 5)),
                omniFunctionExpr("substr", 15, getOmniJsonFieldReference(15, 3) + "," + getOmniJsonLiteral(1, false, 1)
                        + "," + getOmniJsonLiteral(1, false, 5)));
        OmniHashBuilderOperatorFactory hashBuilderOperatorFactory = new OmniHashBuilderOperatorFactory(
                OMNI_JOIN_TYPE_INNER, buildTypes, buildHashCols, operatorCount);
        OmniOperator hashBuilderOperator = hashBuilderOperatorFactory.createOperator();
        hashBuilderOperator.addInput(buildVecBatch);
        hashBuilderOperator.getOutput();

        DataType[] probeTypes = {IntDataType.INTEGER, new VarcharDataType(5)};
        Object[][] probeDatas = {{20, 16, 13, 4, 20, 4, 22, 19, 8, 7},
                {"35709", "35709", "31904", "12477", null, "38721", "90419", "35709", "88371", null}};
        VecBatch probeVecBatch = createVecBatch(probeTypes, probeDatas);

        int[] probeOutputCols = {0, 1};
        int[] probeHashCols = {0};
        int[] buildOutputCols = {0, 1};
        DataType[] buildOutputTypes = {IntDataType.INTEGER, new VarcharDataType(5)};
        OmniLookupJoinOperatorFactory lookupJoinOperatorFactory = new OmniLookupJoinOperatorFactory(probeTypes,
                probeOutputCols, probeHashCols, buildOutputCols, buildOutputTypes,
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
     * Test inner join without output
     */
    @Test
    public void testInnerEqualityJoinWithNoOutput() {
        DataType[] buildTypes = {LongDataType.LONG, LongDataType.LONG};
        Object[][] buildDatas = {{1L, 2L, 3L, 4L}, {111L, 11L, 333L, 33L}};
        VecBatch buildVecBatch = createVecBatch(buildTypes, buildDatas);

        int[] buildHashCols = {0};
        int operatorCount = 1;
        OmniHashBuilderOperatorFactory hashBuilderOperatorFactory = new OmniHashBuilderOperatorFactory(
                OMNI_JOIN_TYPE_INNER, buildTypes, buildHashCols, operatorCount);
        OmniOperator hashBuilderOperator = hashBuilderOperatorFactory.createOperator();
        hashBuilderOperator.addInput(buildVecBatch);
        hashBuilderOperator.getOutput();

        DataType[] probeTypes = {LongDataType.LONG, LongDataType.LONG};
        Object[][] probeDatas = {{0L, 5L, 6L, 7L}, {11L, 22L, 33L, 44L}};
        VecBatch probeVecBatch = createVecBatch(probeTypes, probeDatas);

        int[] probeOutputCols = {0, 1};
        int[] probeHashCols = {0};
        int[] buildOutputCols = {0, 1};
        DataType[] buildOutputTypes = {LongDataType.LONG, LongDataType.LONG};
        OmniLookupJoinOperatorFactory lookupJoinOperatorFactory = new OmniLookupJoinOperatorFactory(probeTypes,
                probeOutputCols, probeHashCols, buildOutputCols, buildOutputTypes,
                hashBuilderOperatorFactory, false);
        OmniOperator lookupJoinOperator = lookupJoinOperatorFactory.createOperator();
        lookupJoinOperator.addInput(probeVecBatch);
        Iterator<VecBatch> results = lookupJoinOperator.getOutput();
        assertTrue(results != null);
        assertEquals(results.hasNext(), false);
        lookupJoinOperator.close();
        hashBuilderOperator.close();
        lookupJoinOperatorFactory.close();
        hashBuilderOperatorFactory.close();
    }

    @Test(expectedExceptions = OmniRuntimeException.class, expectedExceptionsMessageRegExp =
            ".*EXPRESSION_NOT_SUPPORT.*")
    public void testInnerEqualityJoinWithInvalidFilter() {
        DataType[] buildTypes = {LongDataType.LONG, LongDataType.LONG};
        int[] buildHashCols = {0};
        int operatorCount = 1;
        String filterExpression = omniJsonNotEqualExpr(
                omniFunctionExpr("substring", 15,
                        getOmniJsonFieldReference(2, 1) + "," + getOmniJsonLiteral(1, false, 1) + ","
                                + getOmniJsonLiteral(1, false, 5)),
                omniFunctionExpr("substring", 15, getOmniJsonFieldReference(2, 0) + ","
                        + getOmniJsonLiteral(1, false, 1) + "," + getOmniJsonLiteral(1, false, 5)));
        OmniHashBuilderOperatorFactory hashBuilderOperatorFactory = new OmniHashBuilderOperatorFactory(
                OMNI_JOIN_TYPE_INNER, buildTypes, buildHashCols, operatorCount);

        DataType[] probeTypes = {LongDataType.LONG, LongDataType.LONG};
        int[] probeOutputCols = {0, 1};
        int[] probeHashCols = {0};
        int[] buildOutputCols = {0, 1};
        DataType[] buildOutputTypes = {LongDataType.LONG, LongDataType.LONG};
        OmniLookupJoinOperatorFactory lookupJoinOperatorFactory = new OmniLookupJoinOperatorFactory(probeTypes,
                probeOutputCols, probeHashCols, buildOutputCols, buildOutputTypes,
                hashBuilderOperatorFactory, Optional.of(filterExpression), false);

        hashBuilderOperatorFactory.close();
        lookupJoinOperatorFactory.close();
    }

    @Test
    public void testFactoryContextEquals() {
        DataType[] buildTypes = {LongDataType.LONG, LongDataType.LONG};
        int[] buildHashCols = {0};
        int operatorCount = 1;
        OmniHashBuilderOperatorFactory.FactoryContext hashBuilderOperatorFactory1 =
                new OmniHashBuilderOperatorFactory.FactoryContext(
                OMNI_JOIN_TYPE_INNER, buildTypes, buildHashCols, operatorCount, new OperatorConfig());
        OmniHashBuilderOperatorFactory.FactoryContext hashBuilderOperatorFactory2 =
                new OmniHashBuilderOperatorFactory.FactoryContext(
                OMNI_JOIN_TYPE_INNER, buildTypes, buildHashCols, operatorCount, new OperatorConfig());
        OmniHashBuilderOperatorFactory.FactoryContext hashBuilderOperatorFactory3 = null;
        assertEquals(hashBuilderOperatorFactory2, hashBuilderOperatorFactory1);
        assertEquals(hashBuilderOperatorFactory1, hashBuilderOperatorFactory1);
        assertNotEquals(hashBuilderOperatorFactory3, hashBuilderOperatorFactory1);

        DataType[] probeTypes = {LongDataType.LONG, LongDataType.LONG};
        int[] probeOutputCols = {0, 1};
        int[] probeHashCols = {0};
        int[] buildOutputCols = {0, 1};
        DataType[] buildOutputTypes = {LongDataType.LONG, LongDataType.LONG};

        OmniHashBuilderOperatorFactory omniHashBuilderOperatorFactory = new OmniHashBuilderOperatorFactory(
                OMNI_JOIN_TYPE_INNER, buildTypes, buildHashCols, operatorCount, new OperatorConfig());
        OmniLookupJoinOperatorFactory.FactoryContext lookupJoinOperatorFactory1 =
                new OmniLookupJoinOperatorFactory.FactoryContext(
                probeTypes, probeOutputCols, probeHashCols, buildOutputCols, buildOutputTypes,
                        omniHashBuilderOperatorFactory, Optional.empty(), false, new OperatorConfig());
        OmniLookupJoinOperatorFactory.FactoryContext lookupJoinOperatorFactory2 =
                new OmniLookupJoinOperatorFactory.FactoryContext(
                probeTypes, probeOutputCols, probeHashCols, buildOutputCols, buildOutputTypes,
                        omniHashBuilderOperatorFactory, Optional.empty(), false, new OperatorConfig());
        OmniLookupJoinOperatorFactory.FactoryContext lookupJoinOperatorFactory3 = null;

        assertEquals(lookupJoinOperatorFactory2, lookupJoinOperatorFactory1);
        assertEquals(lookupJoinOperatorFactory1, lookupJoinOperatorFactory1);
        assertNotEquals(lookupJoinOperatorFactory3, lookupJoinOperatorFactory1);
    }

    private ImmutableList<VecBatch> buildVecs(int pageCount) {
        ImmutableList.Builder<VecBatch> vecBatchList = ImmutableList.builder();
        int positionCount = pageDistinctCount * pageDistinctValueRepeatCount;
        List<Vec> vecs = new ArrayList<>();
        for (int i = 0; i < pageCount; i++) {
            LongVec longVec1 = new LongVec(positionCount);
            LongVec longVec2 = new LongVec(positionCount);
            int idx = 0;
            for (int j = 0; j < pageDistinctCount; j++) {
                for (int k = 0; k < pageDistinctValueRepeatCount; k++) {
                    longVec1.set(idx, j);
                    longVec2.set(idx, j);
                    idx++;
                }
            }
            vecs.add(longVec1);
            vecs.add(longVec2);
            VecBatch vecBatch = new VecBatch(new Vec[]{longVec1, longVec2});
            vecBatchList.add(vecBatch);
        }
        return vecBatchList.build();
    }

    /**
     * Test full join
     */
    @Test
    public void testFullEqualityJoin() {
        DataType[] buildTypes = {LongDataType.LONG, LongDataType.LONG};
        Object[][] buildDatas = {{1L, 2L, 3L, 4L}, {111L, 11L, 333L, 33L}};
        VecBatch buildVecBatch = createVecBatch(buildTypes, buildDatas);

        int[] buildHashCols = {1};
        int operatorCount = 1;
        OmniHashBuilderOperatorFactory hashBuilderOperatorFactory = new OmniHashBuilderOperatorFactory(
                OMNI_JOIN_TYPE_FULL, buildTypes, buildHashCols, operatorCount);
        OmniOperator hashBuilderOperator = hashBuilderOperatorFactory.createOperator();
        hashBuilderOperator.addInput(buildVecBatch);
        hashBuilderOperator.getOutput();

        DataType[] probeTypes = {LongDataType.LONG, LongDataType.LONG};
        Object[][] probeDatas = {{1L, 2L, 3L, 4L}, {11L, 22L, 33L, 44L}};
        int[] probeOutputCols = {0, 1};
        int[] probeHashCols = {1};
        int[] buildOutputCols = {0, 1};
        VecBatch probeVecBatch = createVecBatch(probeTypes, probeDatas);
        DataType[] buildOutputTypes = {LongDataType.LONG, LongDataType.LONG};
        OmniLookupJoinOperatorFactory lookupJoinOperatorFactory = new OmniLookupJoinOperatorFactory(probeTypes,
                probeOutputCols, probeHashCols, buildOutputCols, buildOutputTypes,
                hashBuilderOperatorFactory, false);
        OmniOperator lookupJoinOperator = lookupJoinOperatorFactory.createOperator();
        OmniLookupOuterJoinOperatorFactory lookupOuterJoinOperatorFactory = new OmniLookupOuterJoinOperatorFactory(
                probeTypes, probeOutputCols, buildOutputCols, buildOutputTypes, hashBuilderOperatorFactory,
                new OperatorConfig());
        OmniOperator lookupOuterJoinOperator = lookupOuterJoinOperatorFactory.createOperator();
        lookupJoinOperator.addInput(probeVecBatch);
        Iterator<VecBatch> results = lookupJoinOperator.getOutput();

        VecBatch resultVecBatch = results.next();
        int len = resultVecBatch.getRowCount();
        assertEquals(len, 4);
        assertEquals(resultVecBatch.getVectorCount(), 4);
        Object[][] expectedDatas = {{1L, 2L, 3L, 4L}, {11L, 22L, 33L, 44L}, {2L, null, 4L, null},
                {11L, null, 33L, null}};
        assertVecBatchEqualsIgnoreOrder(resultVecBatch, expectedDatas);

        Iterator<VecBatch> appendResults = lookupOuterJoinOperator.getOutput();
        VecBatch appendBatch = appendResults.next();
        len = appendBatch.getRowCount();
        assertEquals(len, 2);
        Object[][] expectedData = {{null, null}, {null, null}, {1L, 3L}, {111L, 333L}};
        assertVecBatchEqualsIgnoreOrder(appendBatch, expectedData);

        freeVecBatch(resultVecBatch);
        freeVecBatch(appendBatch);
        lookupOuterJoinOperator.close();
        lookupOuterJoinOperatorFactory.close();
        lookupJoinOperator.close();
        hashBuilderOperator.close();
        lookupJoinOperatorFactory.close();
        hashBuilderOperatorFactory.close();
    }

    /**
     * Test full join with varchar join key
     */
    @Test
    public void testFullEqualityJoinVarchar() {
        DataType[] buildTypes = {LongDataType.LONG, new VarcharDataType(3)};
        Object[][] buildDatas = {{1L, 2L, 3L, 4L}, {"aaa", "11", "ccc", "33"}};
        VecBatch buildVecBatch = createVecBatch(buildTypes, buildDatas);

        int[] buildHashCols = {1};
        int operatorCount = 1;
        OmniHashBuilderOperatorFactory hashBuilderOperatorFactory = new OmniHashBuilderOperatorFactory(
                OMNI_JOIN_TYPE_FULL, buildTypes, buildHashCols, operatorCount);
        OmniOperator hashBuilderOperator = hashBuilderOperatorFactory.createOperator();
        hashBuilderOperator.addInput(buildVecBatch);
        hashBuilderOperator.getOutput();

        DataType[] probeTypes = {LongDataType.LONG, new VarcharDataType(2)};
        Object[][] probeDatas = {{1L, 2L, 3L, 4L}, {"11", "22", "33", "44"}};
        int[] probeOutputCols = {0, 1};
        int[] probeHashCols = {1};
        int[] buildOutputCols = {0, 1};
        VecBatch probeVecBatch = createVecBatch(probeTypes, probeDatas);
        DataType[] buildOutputTypes = {LongDataType.LONG, new VarcharDataType(2)};
        OmniLookupJoinOperatorFactory lookupJoinOperatorFactory = new OmniLookupJoinOperatorFactory(probeTypes,
                probeOutputCols, probeHashCols, buildOutputCols, buildOutputTypes,
                hashBuilderOperatorFactory, false);
        OmniOperator lookupJoinOperator = lookupJoinOperatorFactory.createOperator();
        OmniLookupOuterJoinOperatorFactory lookupOuterJoinOperatorFactory = new OmniLookupOuterJoinOperatorFactory(
                probeTypes, probeOutputCols, buildOutputCols, buildOutputTypes, hashBuilderOperatorFactory,
                new OperatorConfig());
        OmniOperator lookupOuterJoinOperator = lookupOuterJoinOperatorFactory.createOperator();
        lookupJoinOperator.addInput(probeVecBatch);
        Iterator<VecBatch> results = lookupJoinOperator.getOutput();

        VecBatch resultVecBatch = results.next();
        int len = resultVecBatch.getRowCount();
        assertEquals(len, 4);
        assertEquals(resultVecBatch.getVectorCount(), 4);
        Object[][] expectedDatas = {{1L, 2L, 3L, 4L}, {"11", "22", "33", "44"}, {2L, null, 4L, null},
                {"11", null, "33", null}};
        assertVecBatchEqualsIgnoreOrder(resultVecBatch, expectedDatas);
        Iterator<VecBatch> appendResults = lookupOuterJoinOperator.getOutput();
        VecBatch appendBatch = appendResults.next();
        len = appendBatch.getRowCount();
        assertEquals(len, 2);
        Object[][] expectedData = {{null, null}, {null, null}, {1L, 3L}, {"aaa", "ccc"}};
        assertVecBatchEqualsIgnoreOrder(appendBatch, expectedData);

        freeVecBatch(resultVecBatch);
        freeVecBatch(appendBatch);
        lookupOuterJoinOperator.close();
        lookupOuterJoinOperatorFactory.close();
        lookupJoinOperator.close();
        hashBuilderOperator.close();
        lookupJoinOperatorFactory.close();
        hashBuilderOperatorFactory.close();
    }

    /**
     * Test full join with char join key
     */
    @Test
    public void testFullEqualityJoinChar() {
        DataType[] buildTypes = {LongDataType.LONG, new CharDataType(3)};
        Object[][] buildDatas = {{1L, 2L, 3L, 4L}, {"aaa", "11", "ccc", "33"}};
        VecBatch buildVecBatch = createVecBatch(buildTypes, buildDatas);

        int[] buildJoinCols = {1};
        int operatorCount = 1;
        OmniHashBuilderOperatorFactory hashBuilderOperatorFactory = new OmniHashBuilderOperatorFactory(
                OMNI_JOIN_TYPE_FULL, buildTypes, buildJoinCols, operatorCount);
        OmniOperator hashBuilderOperator = hashBuilderOperatorFactory.createOperator();
        hashBuilderOperator.addInput(buildVecBatch);
        hashBuilderOperator.getOutput();

        DataType[] probeTypes = {LongDataType.LONG, new CharDataType(2)};
        Object[][] probeDatas = {{1L, 2L, 3L, 4L}, {"11", "22", "33", "44"}};
        int[] probeOutputCols = {0, 1};
        int[] probeHashCols = {1};
        int[] buildOutputCols = {0, 1};
        VecBatch probeVecBatch = createVecBatch(probeTypes, probeDatas);
        DataType[] buildOutputTypes = {LongDataType.LONG, new CharDataType(2)};
        OmniLookupJoinOperatorFactory lookupJoinOperatorFactory = new OmniLookupJoinOperatorFactory(probeTypes,
                probeOutputCols, probeHashCols, buildOutputCols, buildOutputTypes,
                hashBuilderOperatorFactory, false);
        OmniOperator lookupJoinOperator = lookupJoinOperatorFactory.createOperator();
        OmniLookupOuterJoinOperatorFactory lookupOuterJoinOperatorFactory = new OmniLookupOuterJoinOperatorFactory(
                probeTypes, probeOutputCols, buildOutputCols, buildOutputTypes, hashBuilderOperatorFactory,
                new OperatorConfig());
        OmniOperator lookupOuterJoinOperator = lookupOuterJoinOperatorFactory.createOperator();
        lookupJoinOperator.addInput(probeVecBatch);
        Iterator<VecBatch> results = lookupJoinOperator.getOutput();

        VecBatch resultVecBatch = results.next();
        int len = resultVecBatch.getRowCount();
        assertEquals(len, 4);
        assertEquals(resultVecBatch.getVectorCount(), 4);
        Object[][] expectedDatas = {{1L, 2L, 3L, 4L}, {"11", "22", "33", "44"}, {2L, null, 4L, null},
                {"11", null, "33", null}};
        assertVecBatchEqualsIgnoreOrder(resultVecBatch, expectedDatas);

        Iterator<VecBatch> appendResults = lookupOuterJoinOperator.getOutput();
        VecBatch appendBatch = appendResults.next();
        len = appendBatch.getRowCount();
        assertEquals(len, 2);
        Object[][] expectedData = {{null, null}, {null, null}, {1L, 3L}, {"aaa", "ccc"}};
        assertVecBatchEqualsIgnoreOrder(appendBatch, expectedData);

        freeVecBatch(appendBatch);
        freeVecBatch(resultVecBatch);
        lookupJoinOperator.close();
        hashBuilderOperator.close();
        lookupOuterJoinOperator.close();
        lookupOuterJoinOperatorFactory.close();
        lookupJoinOperatorFactory.close();
        hashBuilderOperatorFactory.close();
    }

    /**
     * Test full join with date32 join key
     */
    @Test
    public void testFullEqualityJoinDate32() {
        DataType[] buildTypes = {LongDataType.LONG, new Date32DataType(DataType.DateUnit.DAY)};
        Object[][] buildDatas = {{1L, 2L, 3L, 4L}, {123, 11, 321, 33}};
        VecBatch buildVecBatch = createVecBatch(buildTypes, buildDatas);

        int[] buildHashCols = {1};
        int operatorCount = 1;
        OmniHashBuilderOperatorFactory hashBuilderOperatorFactory = new OmniHashBuilderOperatorFactory(
                OMNI_JOIN_TYPE_FULL, buildTypes, buildHashCols, operatorCount);
        OmniOperator hashBuilderOperator = hashBuilderOperatorFactory.createOperator();
        hashBuilderOperator.addInput(buildVecBatch);
        hashBuilderOperator.getOutput();

        DataType[] probeTypes = {LongDataType.LONG, new Date32DataType(DataType.DateUnit.DAY)};
        Object[][] probeDatas = {{1L, 2L, 3L, 4L}, {11, 22, 33, 44}};
        int[] probeOutputCols = {0, 1};
        int[] probeHashCols = {1};
        int[] buildOutputCols = {0, 1};
        VecBatch probeVecBatch = createVecBatch(probeTypes, probeDatas);
        DataType[] buildOutputTypes = {LongDataType.LONG, new Date32DataType(DataType.DateUnit.DAY)};
        OmniLookupJoinOperatorFactory lookupJoinOperatorFactory = new OmniLookupJoinOperatorFactory(probeTypes,
                probeOutputCols, probeHashCols, buildOutputCols, buildOutputTypes,
                hashBuilderOperatorFactory, false);
        OmniOperator lookupJoinOperator = lookupJoinOperatorFactory.createOperator();
        OmniLookupOuterJoinOperatorFactory lookupOuterJoinOperatorFactory = new OmniLookupOuterJoinOperatorFactory(
                probeTypes, probeOutputCols, buildOutputCols, buildOutputTypes, hashBuilderOperatorFactory,
                new OperatorConfig());
        OmniOperator lookupOuterJoinOperator = lookupOuterJoinOperatorFactory.createOperator();
        lookupJoinOperator.addInput(probeVecBatch);
        Iterator<VecBatch> results = lookupJoinOperator.getOutput();

        VecBatch resultVecBatch = results.next();
        int len = resultVecBatch.getRowCount();
        assertEquals(len, 4);
        assertEquals(resultVecBatch.getVectorCount(), 4);
        Object[][] expectedDatas = {{1L, 2L, 3L, 4L}, {11, 22, 33, 44}, {2L, null, 4L, null}, {11, null, 33, null}};
        assertVecBatchEqualsIgnoreOrder(resultVecBatch, expectedDatas);

        Iterator<VecBatch> appendResults = lookupOuterJoinOperator.getOutput();
        VecBatch appendBatch = appendResults.next();
        len = appendBatch.getRowCount();
        assertEquals(len, 2);
        Object[][] expectedData = {{null, null}, {null, null}, {1L, 3L}, {123, 321}};
        assertVecBatchEqualsIgnoreOrder(appendBatch, expectedData);

        freeVecBatch(resultVecBatch);
        freeVecBatch(appendBatch);
        lookupOuterJoinOperator.close();
        lookupJoinOperator.close();
        hashBuilderOperator.close();
        lookupOuterJoinOperatorFactory.close();
        lookupJoinOperatorFactory.close();
        hashBuilderOperatorFactory.close();
    }

    /**
     * Test full join with decimal64 join key
     */
    @Test
    public void testFullEqualityJoinDecimal64() {
        DataType[] buildTypes = {LongDataType.LONG, new Decimal64DataType(3, 0)};
        Object[][] buildDatas = {{1L, 2L, 3L, 4L}, {123L, 11L, 321L, 33L}};
        VecBatch buildVecBatch = createVecBatch(buildTypes, buildDatas);

        int[] buildHashCols = {1};
        int operatorCount = 1;
        OmniHashBuilderOperatorFactory hashBuilderOperatorFactory = new OmniHashBuilderOperatorFactory(
                OMNI_JOIN_TYPE_FULL, buildTypes, buildHashCols, operatorCount);
        OmniOperator hashBuilderOperator = hashBuilderOperatorFactory.createOperator();
        hashBuilderOperator.addInput(buildVecBatch);
        hashBuilderOperator.getOutput();

        DataType[] probeTypes = {LongDataType.LONG, new Decimal64DataType(2, 0)};
        Object[][] probeDatas = {{1L, 2L, 3L, 4L}, {11L, 22L, 33L, 44L}};
        int[] probeOutputCols = {0, 1};
        int[] probeHashCols = {1};
        int[] buildOutputCols = {0, 1};
        VecBatch probeVecBatch = createVecBatch(probeTypes, probeDatas);
        DataType[] buildOutputTypes = {LongDataType.LONG, new Decimal64DataType(3, 0)};
        OmniLookupJoinOperatorFactory lookupJoinOperatorFactory = new OmniLookupJoinOperatorFactory(probeTypes,
                probeOutputCols, probeHashCols, buildOutputCols, buildOutputTypes,
                hashBuilderOperatorFactory, false);
        OmniOperator lookupJoinOperator = lookupJoinOperatorFactory.createOperator();
        OmniLookupOuterJoinOperatorFactory lookupOuterJoinOperatorFactory = new OmniLookupOuterJoinOperatorFactory(
                probeTypes, probeOutputCols, buildOutputCols, buildOutputTypes, hashBuilderOperatorFactory,
                new OperatorConfig());
        OmniOperator lookupOuterJoinOperator = lookupOuterJoinOperatorFactory.createOperator();
        lookupJoinOperator.addInput(probeVecBatch);
        Iterator<VecBatch> results = lookupJoinOperator.getOutput();

        VecBatch resultVecBatch = results.next();
        int len = resultVecBatch.getRowCount();
        assertEquals(len, 4);
        assertEquals(resultVecBatch.getVectorCount(), 4);
        Object[][] expectedDatas = {{1L, 2L, 3L, 4L}, {11L, 22L, 33L, 44L}, {2L, null, 4L, null},
                {11L, null, 33L, null}};
        assertVecBatchEqualsIgnoreOrder(resultVecBatch, expectedDatas);

        Iterator<VecBatch> appendResults = lookupOuterJoinOperator.getOutput();
        VecBatch appendBatch = appendResults.next();
        len = appendBatch.getRowCount();
        assertEquals(len, 2);
        Object[][] expectedData = {{null, null}, {null, null}, {1L, 3L}, {123L, 321L}};
        assertVecBatchEqualsIgnoreOrder(appendBatch, expectedData);

        freeVecBatch(resultVecBatch);
        freeVecBatch(appendBatch);
        lookupOuterJoinOperator.close();
        lookupJoinOperator.close();
        hashBuilderOperator.close();
        lookupJoinOperatorFactory.close();
        hashBuilderOperatorFactory.close();
        lookupOuterJoinOperatorFactory.close();
    }

    /**
     * Test full join with decimal128 join key
     */
    @Test
    public void testFullEqualityJoinDecimal128() {
        DataType[] buildTypes = {LongDataType.LONG, new Decimal128DataType(3, 0)};
        Vec[] buildVecs = new Vec[buildTypes.length];
        buildVecs[0] = createVec(buildTypes[0], new Object[]{1L, 2L, 3L, 4L});
        buildVecs[1] = createVec(buildTypes[1], new Object[][]{{123L, 0L}, {11L, 0L}, {321L, 0L}, {33L, 0L}});
        VecBatch buildVecBatch = new VecBatch(buildVecs);

        int[] buildHashCols = {1};
        int operatorCount = 1;
        OmniHashBuilderOperatorFactory hashBuilderOperatorFactory = new OmniHashBuilderOperatorFactory(
                OMNI_JOIN_TYPE_FULL, buildTypes, buildHashCols, operatorCount);
        OmniOperator hashBuilderOperator = hashBuilderOperatorFactory.createOperator();
        hashBuilderOperator.addInput(buildVecBatch);
        hashBuilderOperator.getOutput();

        DataType[] probeTypes = {LongDataType.LONG, new Decimal128DataType(2, 0)};
        Vec[] probeVecs = new Vec[probeTypes.length];
        probeVecs[0] = createVec(probeTypes[0], new Object[]{1L, 2L, 3L, 4L});
        probeVecs[1] = createVec(probeTypes[1], new Object[][]{{11L, 0L}, {22L, 0L}, {33L, 0L}, {44L, 0L}});

        int[] probeOutputCols = {0, 1};
        int[] probeHashCols = {1};
        int[] buildOutputCols = {0, 1};
        VecBatch probeVecBatch = new VecBatch(probeVecs);
        DataType[] buildOutputTypes = {LongDataType.LONG, new Decimal128DataType(3, 0)};
        OmniLookupJoinOperatorFactory lookupJoinOperatorFactory = new OmniLookupJoinOperatorFactory(probeTypes,
                probeOutputCols, probeHashCols, buildOutputCols, buildOutputTypes,
                hashBuilderOperatorFactory, false);
        OmniOperator lookupJoinOperator = lookupJoinOperatorFactory.createOperator();
        OmniLookupOuterJoinOperatorFactory lookupOuterJoinOperatorFactory = new OmniLookupOuterJoinOperatorFactory(
                probeTypes, probeOutputCols, buildOutputCols, buildOutputTypes, hashBuilderOperatorFactory,
                new OperatorConfig());
        OmniOperator lookupOuterJoinOperator = lookupOuterJoinOperatorFactory.createOperator();
        lookupJoinOperator.addInput(probeVecBatch);
        Iterator<VecBatch> results = lookupJoinOperator.getOutput();

        VecBatch resultVecBatch = results.next();
        int len = resultVecBatch.getRowCount();
        assertEquals(len, 4);
        assertEquals(resultVecBatch.getVectorCount(), 4);
        assertVecEqualsIgnoreOrder(resultVecBatch.getVectors()[0], new Object[]{1L, 2L, 3L, 4L});
        assertDecimal128VecEqualsIgnoreOrder(resultVecBatch.getVectors()[1],
                new Long[][]{{11L, 0L}, {22L, 0L}, {33L, 0L}, {44L, 0L}});
        assertVecEqualsIgnoreOrder(resultVecBatch.getVectors()[2], new Object[]{2L, null, 4L, null});
        assertDecimal128VecEqualsIgnoreOrder(resultVecBatch.getVectors()[3], new Long[][]{{11L, 0L}, null, {33L, 0L},
                null});

        Iterator<VecBatch> appendResults = lookupOuterJoinOperator.getOutput();
        VecBatch appendBatch = appendResults.next();
        len = appendBatch.getRowCount();
        assertEquals(len, 2);
        assertVecEqualsIgnoreOrder(appendBatch.getVector(0), new Object[]{null, null});
        assertDecimal128VecEqualsIgnoreOrder(appendBatch.getVector(1), new Long[][]{null, null});
        assertVecEqualsIgnoreOrder(appendBatch.getVector(2), new Object[]{1L, 3L});
        assertDecimal128VecEqualsIgnoreOrder(appendBatch.getVector(3), new Long[][]{{123L, 0L}, {321L, 0L}});

        freeVecBatch(resultVecBatch);
        freeVecBatch(appendBatch);
        lookupOuterJoinOperator.close();
        lookupJoinOperator.close();
        hashBuilderOperator.close();
        lookupJoinOperatorFactory.close();
        hashBuilderOperatorFactory.close();
        lookupOuterJoinOperatorFactory.close();
    }
}
