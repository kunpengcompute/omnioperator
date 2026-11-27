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
import static nova.hetu.omniruntime.util.TestUtils.createVec;
import static nova.hetu.omniruntime.util.TestUtils.createVecBatch;
import static nova.hetu.omniruntime.util.TestUtils.freeVecBatch;
import static org.testng.Assert.assertEquals;
import static org.testng.Assert.assertFalse;
import static org.testng.Assert.assertNotEquals;
import static org.testng.Assert.assertTrue;

import com.google.common.collect.ImmutableList;

import nova.hetu.omniruntime.operator.config.OperatorConfig;
import nova.hetu.omniruntime.operator.project.OmniProjectOperatorFactory;
import nova.hetu.omniruntime.operator.project.OmniProjectOperatorFactory.FactoryContext;
import nova.hetu.omniruntime.type.BooleanDataType;
import nova.hetu.omniruntime.type.DataType;
import nova.hetu.omniruntime.type.Decimal128DataType;
import nova.hetu.omniruntime.type.DoubleDataType;
import nova.hetu.omniruntime.type.IntDataType;
import nova.hetu.omniruntime.type.LongDataType;
import nova.hetu.omniruntime.type.VarcharDataType;
import nova.hetu.omniruntime.vector.BooleanVec;
import nova.hetu.omniruntime.vector.Decimal128Vec;
import nova.hetu.omniruntime.vector.DoubleVec;
import nova.hetu.omniruntime.vector.IntVec;
import nova.hetu.omniruntime.vector.LongVec;
import nova.hetu.omniruntime.vector.VarcharVec;
import nova.hetu.omniruntime.vector.Vec;
import nova.hetu.omniruntime.vector.VecBatch;

import org.testng.annotations.Test;

import java.nio.charset.StandardCharsets;
import java.util.Iterator;

/**
 * The type Omni project operator test.
 *
 * @since 2021-7-6
 */
public class OmniProjectOperatorTest {
    private ImmutableList<VecBatch> makeInput(int nRows, Vec... cols) {
        return ImmutableList.copyOf(new VecBatch[]{new VecBatch(cols)});
    }

    /**
     * Simple test.
     */
    @Test
    public void simpleTest() {
        String[] exprs = {"$operator$ADD:1(#0, 5:1)"};
        DataType[] inputTypes = {IntDataType.INTEGER};
        OmniProjectOperatorFactory factory = new OmniProjectOperatorFactory(exprs, inputTypes);
        final int numRows = 1000;
        IntVec col1 = new IntVec(numRows);
        for (int i = 0; i < numRows; i++) {
            col1.set(i, i);
        }
        OmniOperator op = factory.createOperator();
        ImmutableList<VecBatch> vecBatches = makeInput(numRows, col1);
        for (VecBatch vecBatch : vecBatches) {
            op.addInput(vecBatch);
        }

        Iterator<VecBatch> vecBatchIterator = op.getOutput();
        assertTrue(vecBatchIterator.hasNext());
        VecBatch res = op.getOutput().next();
        assertFalse(vecBatchIterator.hasNext());
        assertEquals(res.getRowCount(), numRows);
        for (int i = 0; i < res.getRowCount(); i++) {
            assertEquals(((IntVec) res.getVector(0)).get(i), i + 5);
        }

        freeVecBatch(res);
        op.close();
        factory.close();
    }

    /**
     * Complex test.
     */
    @Test
    public void complexTest() {
        String[] exprs = {"$operator$MULTIPLY:1(#0, #1)", "IF:2($operator$LESS_THAN:4(#0, 500:1), 4000000000:2, #2)"};
        DataType[] inputTypes = {IntDataType.INTEGER, IntDataType.INTEGER, LongDataType.LONG};
        OmniProjectOperatorFactory factory = new OmniProjectOperatorFactory(exprs, inputTypes);
        final int numRows = 1000;
        IntVec col1 = new IntVec(numRows);
        IntVec col2 = new IntVec(numRows);
        LongVec col3 = new LongVec(numRows);
        for (int i = 0; i < numRows; i++) {
            col1.set(i, i + 1);
            col2.set(i, i - 100);
            col3.set(i, i + 3000000000L);
        }
        OmniOperator op = factory.createOperator();
        ImmutableList<VecBatch> vecBatches = makeInput(numRows, col1, col2, col3);
        for (VecBatch vecBatch : vecBatches) {
            op.addInput(vecBatch);
        }

        assertTrue(op.getOutput().hasNext());
        VecBatch res = op.getOutput().next();
        assertEquals(res.getRowCount(), numRows);
        for (int i = 0; i < res.getRowCount(); i++) {
            assertEquals(((IntVec) res.getVector(0)).get(i), (i + 1) * (i - 100));
            assertEquals(((LongVec) res.getVector(1)).get(i), (i + 1) < 500 ? 4000000000L : i + 3000000000L);
        }

        freeVecBatch(res);
        op.close();
        factory.close();
    }

    /**
     * Murmur3hash&Pmod test.
     */
    @Test
    public void mm3HashAndPmodTest() {
        final int numRows = 3;
        final byte[] byteVal1 = "Wednesday".getBytes(StandardCharsets.UTF_8);
        final byte[] byteVal2 = "Hello World".getBytes(StandardCharsets.UTF_8);
        IntVec col1 = new IntVec(numRows);
        DoubleVec col2 = new DoubleVec(numRows);
        VarcharVec col3 = new VarcharVec(byteVal1.length + byteVal2.length, numRows);
        Decimal128Vec col4 = new Decimal128Vec(numRows);
        BooleanVec col5 = new BooleanVec(numRows);
        col1.set(0, Integer.MIN_VALUE);
        col2.set(0, Double.MAX_VALUE);
        col3.set(0, byteVal1);
        col4.set(0, new long[]{Long.MIN_VALUE, Long.MAX_VALUE});
        col5.set(0, true);
        col1.set(1, Integer.MAX_VALUE);
        col2.set(1, Double.MIN_VALUE);
        col3.set(1, byteVal2);
        col4.set(1, new long[]{Long.MAX_VALUE, Long.MIN_VALUE});
        col5.set(1, false);
        // null value
        col1.set(2, Integer.MIN_VALUE);
        col1.setNull(2);
        col2.set(2, Double.MAX_VALUE);
        col2.setNull(2);
        col3.setNull(2);
        col4.set(2, new long[]{Long.MAX_VALUE, Long.MAX_VALUE});
        col4.setNull(2);
        col5.setNull(2);

        String[] exprs = {"pmod:1(mm3hash:1(#0, 42:1), 42:1)", "mm3hash:1(#1, 42:1)", "mm3hash:1(#2, 42:1)",
                "mm3hash:1(#3, 42:1)", "mm3hash:1(#4, 42:1)"};
        DataType[] inputTypes = {IntDataType.INTEGER, DoubleDataType.DOUBLE, VarcharDataType.VARCHAR,
                Decimal128DataType.DECIMAL128, BooleanDataType.BOOLEAN};
        OmniProjectOperatorFactory factory = new OmniProjectOperatorFactory(exprs, inputTypes);
        OmniOperator op = factory.createOperator();
        ImmutableList<VecBatch> vecBatches = makeInput(numRows, col1, col2, col3, col4, col5);
        for (VecBatch vecBatch : vecBatches) {
            op.addInput(vecBatch);
        }

        assertTrue(op.getOutput().hasNext());
        VecBatch res = op.getOutput().next();
        assertEquals(res.getRowCount(), numRows);
        assertEquals(res.getVectors().length, exprs.length);
        assertEquals(((IntVec) res.getVector(0)).get(0), 20);
        assertEquals(((IntVec) res.getVector(1)).get(0), -508695674);
        assertEquals(((IntVec) res.getVector(2)).get(0), 613818021);
        assertEquals(((IntVec) res.getVector(3)).get(0), 1090190174);
        assertEquals(((IntVec) res.getVector(4)).get(0), -559580957);
        assertEquals(((IntVec) res.getVector(0)).get(1), 25);
        assertEquals(((IntVec) res.getVector(1)).get(1), -1712319331);
        assertEquals(((IntVec) res.getVector(2)).get(1), 352365215);
        assertEquals(((IntVec) res.getVector(3)).get(1), 1352383760);
        assertEquals(((IntVec) res.getVector(4)).get(1), 933211791);
        // null value check
        assertEquals(((IntVec) res.getVector(0)).get(2), 0);
        assertEquals(((IntVec) res.getVector(1)).get(2), 42);
        assertEquals(((IntVec) res.getVector(2)).get(2), 42);
        assertEquals(((IntVec) res.getVector(3)).get(2), 42);
        assertEquals(((IntVec) res.getVector(4)).get(2), 42);

        freeVecBatch(res);
        op.close();
        factory.close();
    }

    /**
     * xxHash64 test.
     */
    @Test
    public void xxHash64StringTest() {
        DataType[] inputTypes = {new VarcharDataType(50)};
        Object[][] datas = {{"hello world", "abcdefghijklmnopqrstuvwxyz ABCDEFGHIJKLMNOPQRSTUVWXYZ", "china"}};
        VecBatch vecBatch = createVecBatch(inputTypes, datas);
        String[] expressions = {"{\"exprType\":\"FUNCTION\",\"returnType\":2,\"function_name\":\"xxhash64\","
                + "\"arguments\":[{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":15,\"colVal\":0,\"width\":50},"
                + "{\"exprType\":\"LITERAL\",\"dataType\":2,\"isNull\":false,\"value\":42}]}"};

        OmniProjectOperatorFactory factory = new OmniProjectOperatorFactory(expressions, inputTypes, 1,
                new OperatorConfig());

        OmniOperator op = factory.createOperator();
        op.addInput(vecBatch);

        Iterator<VecBatch> results = op.getOutput();
        VecBatch resultVecBatch = results.next();

        Object[][] expectDatas = {{7620854247404556961L, -8961370173016112133L, 1148854020565811068L}};
        assertVecBatchEquals(resultVecBatch, expectDatas);

        freeVecBatch(resultVecBatch);
        op.close();
        factory.close();
    }

    @Test
    public void xxHash64Decimal128Test() {
        DataType[] inputTypes = {new Decimal128DataType(38, 16)};
        Object[][] datas = {{4000L, 0L}, {2000L, 0L}, {1000L, 0L}};
        Vec[] buildVecs = new Vec[inputTypes.length];
        buildVecs[0] = createVec(inputTypes[0], datas);
        VecBatch vecBatch = new VecBatch(buildVecs);
        String[] expressions = {"{\"exprType\":\"FUNCTION\",\"returnType\":2,\"function_name\":\"xxhash64\","
                + "\"arguments\":[{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":7,\"precision\":38,\"scale\":16,"
                + "\"colVal\":0},{\"exprType\":\"LITERAL\",\"dataType\":2,\"isNull\":false,\"value\":42}]}"};

        OmniProjectOperatorFactory factory = new OmniProjectOperatorFactory(expressions, inputTypes, 1,
                new OperatorConfig());

        OmniOperator op = factory.createOperator();
        op.addInput(vecBatch);

        Iterator<VecBatch> results = op.getOutput();
        VecBatch resultVecBatch = results.next();

        Object[][] expectDatas = {{4122469761574251967L, -1100376009453395183L, -1241606273492999864L}};
        assertVecBatchEquals(resultVecBatch, expectDatas);

        freeVecBatch(resultVecBatch);
        op.close();
        factory.close();
    }

    /**
     * Unsupported expression.
     */
    @Test
    public void unsupportedCast() {
        DataType[] types = {};
        String[] projectionsJSON = {"{\"exprType\": \"FUNCTION\", \"returnType\": 2, \"function_name\": \"CAST\", "
                + "\"arguments\": [{\"exprType\": \"IF\", \"returnType\": 1, \"condition\": {\"exprType\": "
                + "\"FUNCTION\", \"returnType\": 4, \"function_name\": \"not\", \"arguments\": "
                + "[{ \"exprType\": \"LITERAL\", \"dataType\": 1, \"isNull\": true}]}, \"if_true\": "
                + "{ \"exprType\": \"LITERAL\", \"dataType\": 1, \"isNull\": false, \"value\": 1}, "
                + "\"if_false\": { \"exprType\": \"LITERAL\", \"dataType\": 1, \"isNull\": false, \"value\": 0}}]}"};

        OmniProjectOperatorFactory factory = new OmniProjectOperatorFactory(projectionsJSON, types, 1);

        assertFalse(factory.isSupported());
        factory.close();
    }

    @Test
    public void testFactoryContextEquals() {
        DataType[] types = {};
        String[] projectionsJSON = {"{\"exprType\": \"FUNCTION\", \"returnType\": 2, \"function_name\": \"CAST\", "
                + "\"arguments\": [{\"exprType\": \"IF\", \"returnType\": 1, \"condition\": {\"exprType\": "
                + "\"FUNCTION\", \"returnType\": 4, \"function_name\": \"not\", \"arguments\": "
                + "[{ \"exprType\": \"LITERAL\", \"dataType\": 1, \"isNull\": true}]}, \"if_true\": "
                + "{ \"exprType\": \"LITERAL\", \"dataType\": 1, \"isNull\": false, \"value\": 1}, "
                + "\"if_false\": { \"exprType\": \"LITERAL\", \"dataType\": 1, \"isNull\": false, \"value\": 0}}]}"};
        FactoryContext factory1 = new FactoryContext(projectionsJSON, types, 1, new OperatorConfig());
        FactoryContext factory2 = new FactoryContext(projectionsJSON, types, 1, new OperatorConfig());
        FactoryContext factory3 = null;
        assertEquals(factory2, factory1);
        assertEquals(factory1, factory1);
        assertNotEquals(factory3, factory1);
    }

    @Test
    public void testReplaceWithRep() {
        DataType[] types = {new VarcharDataType(20), new VarcharDataType(10), new VarcharDataType(10)};
        Object[][] datas = {{"varchar100", "varchar200", "varchar300"}, {"char1", "char", "char3"},
                {"opera", "*#", "VARCHAR"}};
        VecBatch vecBatch = createVecBatch(types, datas);
        String[] expressions = {"{\"exprType\":\"FUNCTION\",\"returnType\":15,\"function_name\":\"replace\","
                + "\"arguments\":[{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":15,\"colVal\":0,\"width\":20},"
                + "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":15,\"colVal\":1,\"width\":10},{\"exprType\":"
                + "\"FIELD_REFERENCE\",\"dataType\":15,\"colVal\":2,\"width\":10}],\"width\":100}"};
        DataType[] inputTypes = {new VarcharDataType(20), new VarcharDataType(10), new VarcharDataType(10)};

        OmniProjectOperatorFactory factory = new OmniProjectOperatorFactory(expressions, inputTypes, 1,
                new OperatorConfig());

        OmniOperator op = factory.createOperator();
        op.addInput(vecBatch);

        Iterator<VecBatch> results = op.getOutput();
        VecBatch resultVecBatch = results.next();

        Object[][] expectDatas = {{"varopera00", "var*#200", "varVARCHAR00"}};
        assertVecBatchEquals(resultVecBatch, expectDatas);

        freeVecBatch(resultVecBatch);
        op.close();
        factory.close();
    }

    @Test
    public void testReplaceWithoutRep() {
        DataType[] types = {new VarcharDataType(20), new VarcharDataType(10)};
        Object[][] datas = {{"varchar100", "varchar200", "varchar300"}, {"char1", "char2", "char3"}};
        VecBatch vecBatch = createVecBatch(types, datas);
        String[] expressions = {"{\"exprType\":\"FUNCTION\",\"returnType\":15,\"function_name\":\"replace\","
                + "\"arguments\":[{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":15,\"colVal\":0,\"width\":20},"
                + "{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":15,\"colVal\":1,\"width\":10}],\"width\":100}"};
        DataType[] inputTypes = {new VarcharDataType(20), new VarcharDataType(10)};

        OmniProjectOperatorFactory factory = new OmniProjectOperatorFactory(expressions, inputTypes, 1,
                new OperatorConfig());

        OmniOperator op = factory.createOperator();
        op.addInput(vecBatch);

        Iterator<VecBatch> results = op.getOutput();
        VecBatch resultVecBatch = results.next();

        Object[][] expectDatas = {{"var00", "var00", "var00"}};
        assertVecBatchEquals(resultVecBatch, expectDatas);

        freeVecBatch(resultVecBatch);
        op.close();
        factory.close();
    }
}
