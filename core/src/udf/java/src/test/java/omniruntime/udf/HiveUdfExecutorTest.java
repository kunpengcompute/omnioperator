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

package omniruntime.udf;

import static omniruntime.udf.HiveUdfTestUtil.assertUdfResultsEquals;
import static omniruntime.udf.HiveUdfTestUtil.createBatchInputs;
import static omniruntime.udf.HiveUdfTestUtil.createSingleInput;
import static omniruntime.udf.HiveUdfTestUtil.releaseBatchInputs;
import static omniruntime.udf.HiveUdfTestUtil.releaseMemory;
import static omniruntime.udf.HiveUdfTestUtil.releaseSingleInput;

import static org.testng.Assert.assertEquals;
import static org.testng.Assert.assertNotEquals;

import nova.hetu.omniruntime.type.DataType.DataTypeId;
import nova.hetu.omniruntime.utils.OmniRuntimeException;

import org.testng.annotations.Test;

import java.nio.charset.StandardCharsets;
import java.util.List;

/**
 * The hive udf executor test.
 *
 * @since 2021-09-09
 */
public class HiveUdfExecutorTest {
    private final String testJarPath = this.getClass().getProtectionDomain().getCodeSource().getLocation().getPath();

    /**
     * test AddIntUDF
     */
    @Test
    public void testSingleAddIntUdf() {
        String className = "omniruntime.udf.AddIntUDF";
        DataTypeId[] inputTypes = {DataTypeId.OMNI_INT, DataTypeId.OMNI_INT};
        DataTypeId outputType = DataTypeId.OMNI_INT;
        List<Long> inputInfoAddrs = createSingleInput(inputTypes, new Object[]{9, 299});

        long outputValueAddr = UdfUtil.UNSAFE.allocateMemory(Integer.BYTES);
        long outputNullAddr = UdfUtil.UNSAFE.allocateMemory(1);
        long outputLengthAddr = 0L;

        String jarPath = this.getClass().getProtectionDomain().getCodeSource().getLocation().getPath();
        HiveUdfExecutor.executeSingle(jarPath, className, inputTypes, outputType, inputInfoAddrs.get(0),
                inputInfoAddrs.get(1), inputInfoAddrs.get(2), outputValueAddr, outputNullAddr, outputLengthAddr);
        assertEquals(UdfUtil.UNSAFE.getInt(outputValueAddr), 308);
        assertNotEquals(UdfUtil.UNSAFE.getByte(outputNullAddr), 1);
        releaseSingleInput(inputTypes, inputInfoAddrs.get(0), inputInfoAddrs.get(1), inputInfoAddrs.get(2));

        inputInfoAddrs = createSingleInput(inputTypes, new Object[]{-88, -8});
        HiveUdfExecutor.executeSingle(jarPath, className, inputTypes, outputType, inputInfoAddrs.get(0),
                inputInfoAddrs.get(1), inputInfoAddrs.get(2), outputValueAddr, outputNullAddr, outputLengthAddr);
        assertEquals(UdfUtil.UNSAFE.getInt(outputValueAddr), -96);
        assertNotEquals(UdfUtil.UNSAFE.getByte(outputNullAddr), 1);
        releaseSingleInput(inputTypes, inputInfoAddrs.get(0), inputInfoAddrs.get(1), inputInfoAddrs.get(2));

        inputInfoAddrs = createSingleInput(inputTypes, new Object[]{34, -12});
        HiveUdfExecutor.executeSingle(jarPath, className, inputTypes, outputType, inputInfoAddrs.get(0),
                inputInfoAddrs.get(1), inputInfoAddrs.get(2), outputValueAddr, outputNullAddr, outputLengthAddr);
        assertEquals(UdfUtil.UNSAFE.getInt(outputValueAddr), 22);
        assertNotEquals(UdfUtil.UNSAFE.getByte(outputNullAddr), 1);
        releaseSingleInput(inputTypes, inputInfoAddrs.get(0), inputInfoAddrs.get(1), inputInfoAddrs.get(2));

        inputInfoAddrs = createSingleInput(inputTypes, new Object[]{null, 67});
        HiveUdfExecutor.executeSingle(jarPath, className, inputTypes, outputType, inputInfoAddrs.get(0),
                inputInfoAddrs.get(1), inputInfoAddrs.get(2), outputValueAddr, outputNullAddr, outputLengthAddr);
        assertEquals(UdfUtil.UNSAFE.getByte(outputNullAddr), 1);
        releaseSingleInput(inputTypes, inputInfoAddrs.get(0), inputInfoAddrs.get(1), inputInfoAddrs.get(2));

        releaseMemory(outputValueAddr, outputNullAddr, outputLengthAddr);
    }

    /**
     * test AddLongUDF
     */
    @Test
    public void testSingleAddLongUdf() {
        String className = "omniruntime.udf.AddLongUDF";
        DataTypeId[] inputTypes = {DataTypeId.OMNI_LONG, DataTypeId.OMNI_LONG};
        DataTypeId outputType = DataTypeId.OMNI_LONG;
        List<Long> inputInfoAddrs = createSingleInput(inputTypes, new Object[]{199L, 299L});

        long outputValueAddr = UdfUtil.UNSAFE.allocateMemory(Long.BYTES);
        long outputNullAddr = UdfUtil.UNSAFE.allocateMemory(1);
        long outputLengthAddr = 0L;

        HiveUdfExecutor.executeSingle(testJarPath, className, inputTypes, outputType, inputInfoAddrs.get(0),
                inputInfoAddrs.get(1), inputInfoAddrs.get(2), outputValueAddr, outputNullAddr, outputLengthAddr);
        assertEquals(UdfUtil.UNSAFE.getLong(outputValueAddr), 498);
        assertNotEquals(UdfUtil.UNSAFE.getByte(outputNullAddr), 1);
        releaseSingleInput(inputTypes, inputInfoAddrs.get(0), inputInfoAddrs.get(1), inputInfoAddrs.get(2));

        inputInfoAddrs = createSingleInput(inputTypes, new Object[]{null, -67L});
        HiveUdfExecutor.executeSingle(testJarPath, className, inputTypes, outputType, inputInfoAddrs.get(0),
                inputInfoAddrs.get(1), inputInfoAddrs.get(2), outputValueAddr, outputNullAddr, outputLengthAddr);
        assertEquals(UdfUtil.UNSAFE.getByte(outputNullAddr), 1);
        releaseSingleInput(inputTypes, inputInfoAddrs.get(0), inputInfoAddrs.get(1), inputInfoAddrs.get(2));

        releaseMemory(outputValueAddr, outputNullAddr, outputLengthAddr);
    }

    /**
     * test AddShortUDF
     */
    @Test
    public void testSingleAddShortUdf() {
        String className = "omniruntime.udf.AddShortUDF";
        DataTypeId[] inputTypes = {DataTypeId.OMNI_SHORT, DataTypeId.OMNI_SHORT};
        DataTypeId outputType = DataTypeId.OMNI_SHORT;
        List<Long> inputInfoAddrs = createSingleInput(inputTypes, new Object[]{(short) 9, (short) 11});

        long outputValueAddr = UdfUtil.UNSAFE.allocateMemory(Short.BYTES);
        long outputNullAddr = UdfUtil.UNSAFE.allocateMemory(1);
        long outputLengthAddr = 0L;

        HiveUdfExecutor.executeSingle(testJarPath, className, inputTypes, outputType, inputInfoAddrs.get(0),
                inputInfoAddrs.get(1), inputInfoAddrs.get(2), outputValueAddr, outputNullAddr, outputLengthAddr);
        assertEquals(UdfUtil.UNSAFE.getShort(outputValueAddr), 20);
        assertNotEquals(UdfUtil.UNSAFE.getByte(outputNullAddr), 1);
        releaseSingleInput(inputTypes, inputInfoAddrs.get(0), inputInfoAddrs.get(1), inputInfoAddrs.get(2));

        inputInfoAddrs = createSingleInput(inputTypes, new Object[]{null, (short) -7});
        HiveUdfExecutor.executeSingle(testJarPath, className, inputTypes, outputType, inputInfoAddrs.get(0),
                inputInfoAddrs.get(1), inputInfoAddrs.get(2), outputValueAddr, outputNullAddr, outputLengthAddr);
        assertEquals(UdfUtil.UNSAFE.getByte(outputNullAddr), 1);
        releaseSingleInput(inputTypes, inputInfoAddrs.get(0), inputInfoAddrs.get(1), inputInfoAddrs.get(2));

        releaseMemory(outputValueAddr, outputNullAddr, outputLengthAddr);
    }

    /**
     * test AndBooleanUDF
     */
    @Test
    public void testSingleAndBooleanUdf() {
        String className = "omniruntime.udf.AndBooleanUDF";
        DataTypeId[] inputTypes = {DataTypeId.OMNI_BOOLEAN, DataTypeId.OMNI_BOOLEAN};
        DataTypeId outputType = DataTypeId.OMNI_BOOLEAN;
        List<Long> inputInfoAddrs = createSingleInput(inputTypes, new Object[]{(byte) 0, (byte) 1});

        long outputValueAddr = UdfUtil.UNSAFE.allocateMemory(Byte.BYTES);
        long outputNullAddr = UdfUtil.UNSAFE.allocateMemory(1);
        long outputLengthAddr = 0L;

        HiveUdfExecutor.executeSingle(testJarPath, className, inputTypes, outputType, inputInfoAddrs.get(0),
                inputInfoAddrs.get(1), inputInfoAddrs.get(2), outputValueAddr, outputNullAddr, outputLengthAddr);
        assertEquals(UdfUtil.UNSAFE.getByte(outputValueAddr), 0);
        assertNotEquals(UdfUtil.UNSAFE.getByte(outputNullAddr), 1);
        releaseSingleInput(inputTypes, inputInfoAddrs.get(0), inputInfoAddrs.get(1), inputInfoAddrs.get(2));

        inputInfoAddrs = createSingleInput(inputTypes, new Object[]{null, (byte) 1});
        HiveUdfExecutor.executeSingle(testJarPath, className, inputTypes, outputType, inputInfoAddrs.get(0),
                inputInfoAddrs.get(1), inputInfoAddrs.get(2), outputValueAddr, outputNullAddr, outputLengthAddr);
        assertEquals(UdfUtil.UNSAFE.getByte(outputNullAddr), 1);
        releaseSingleInput(inputTypes, inputInfoAddrs.get(0), inputInfoAddrs.get(1), inputInfoAddrs.get(2));

        releaseMemory(outputValueAddr, outputNullAddr, outputLengthAddr);
    }

    /**
     * test ConcatStringUDF
     */
    @Test
    public void testSingleConcatStringUdf() {
        String className = "omniruntime.udf.ConcatStringUDF";
        DataTypeId[] inputTypes = {DataTypeId.OMNI_VARCHAR, DataTypeId.OMNI_VARCHAR};
        DataTypeId outputType = DataTypeId.OMNI_VARCHAR;
        List<Long> inputInfoAddrs = createSingleInput(inputTypes, new Object[]{null, "John"});

        long outputValueAddr = UdfUtil.UNSAFE.allocateMemory(20);
        long outputNullAddr = UdfUtil.UNSAFE.allocateMemory(1);
        long outputLengthAddr = UdfUtil.UNSAFE.allocateMemory(Integer.BYTES);

        HiveUdfExecutor.executeSingle(testJarPath, className, inputTypes, outputType, inputInfoAddrs.get(0),
                inputInfoAddrs.get(1), inputInfoAddrs.get(2), outputValueAddr, outputNullAddr, outputLengthAddr);
        assertEquals(UdfUtil.UNSAFE.getByte(outputNullAddr), 1);
        releaseSingleInput(inputTypes, inputInfoAddrs.get(0), inputInfoAddrs.get(1), inputInfoAddrs.get(2));

        inputInfoAddrs = createSingleInput(inputTypes, new Object[]{"Jimmy", null});
        HiveUdfExecutor.executeSingle(testJarPath, className, inputTypes, outputType, inputInfoAddrs.get(0),
                inputInfoAddrs.get(1), inputInfoAddrs.get(2), outputValueAddr, outputNullAddr, outputLengthAddr);
        assertEquals(UdfUtil.UNSAFE.getByte(outputNullAddr), 1);
        releaseSingleInput(inputTypes, inputInfoAddrs.get(0), inputInfoAddrs.get(1), inputInfoAddrs.get(2));

        inputInfoAddrs = createSingleInput(inputTypes, new Object[]{null, null});
        HiveUdfExecutor.executeSingle(testJarPath, className, inputTypes, outputType, inputInfoAddrs.get(0),
                inputInfoAddrs.get(1), inputInfoAddrs.get(2), outputValueAddr, outputNullAddr, outputLengthAddr);
        assertEquals(UdfUtil.UNSAFE.getByte(outputNullAddr), 1);
        releaseSingleInput(inputTypes, inputInfoAddrs.get(0), inputInfoAddrs.get(1), inputInfoAddrs.get(2));

        inputInfoAddrs = createSingleInput(inputTypes, new Object[]{"", ""});
        HiveUdfExecutor.executeSingle(testJarPath, className, inputTypes, outputType, inputInfoAddrs.get(0),
                inputInfoAddrs.get(1), inputInfoAddrs.get(2), outputValueAddr, outputNullAddr, outputLengthAddr);
        assertNotEquals(UdfUtil.UNSAFE.getByte(outputNullAddr), 1);
        assertEquals(UdfUtil.getBytes(outputValueAddr, 0, 0), "".getBytes(StandardCharsets.UTF_8));
        assertEquals(UdfUtil.UNSAFE.getInt(outputLengthAddr), 0);
        releaseSingleInput(inputTypes, inputInfoAddrs.get(0), inputInfoAddrs.get(1), inputInfoAddrs.get(2));

        inputInfoAddrs = createSingleInput(inputTypes, new Object[]{"John", "Jimmy"});
        HiveUdfExecutor.executeSingle(testJarPath, className, inputTypes, outputType, inputInfoAddrs.get(0),
                inputInfoAddrs.get(1), inputInfoAddrs.get(2), outputValueAddr, outputNullAddr, outputLengthAddr);
        assertNotEquals(UdfUtil.UNSAFE.getByte(outputNullAddr), 1);
        assertEquals(UdfUtil.getBytes(outputValueAddr, 0, 9), "JohnJimmy".getBytes(StandardCharsets.UTF_8));
        assertEquals(UdfUtil.UNSAFE.getInt(outputLengthAddr), 9);
        releaseSingleInput(inputTypes, inputInfoAddrs.get(0), inputInfoAddrs.get(1), inputInfoAddrs.get(2));

        inputInfoAddrs = createSingleInput(inputTypes, new Object[]{"hello", ""});
        HiveUdfExecutor.executeSingle(testJarPath, className, inputTypes, outputType, inputInfoAddrs.get(0),
                inputInfoAddrs.get(1), inputInfoAddrs.get(2), outputValueAddr, outputNullAddr, outputLengthAddr);
        assertNotEquals(UdfUtil.UNSAFE.getByte(outputNullAddr), 1);
        assertEquals(UdfUtil.getBytes(outputValueAddr, 0, 5), "hello".getBytes(StandardCharsets.UTF_8));
        assertEquals(UdfUtil.UNSAFE.getInt(outputLengthAddr), 5);
        releaseSingleInput(inputTypes, inputInfoAddrs.get(0), inputInfoAddrs.get(1), inputInfoAddrs.get(2));

        releaseMemory(outputValueAddr, outputNullAddr, outputLengthAddr);
    }

    /**
     * test MaxDoubleUDF
     */
    @Test
    public void testSingleMaxDoubleUdf() {
        String className = "omniruntime.udf.MaxDoubleUDF";
        DataTypeId[] inputTypes = {DataTypeId.OMNI_DOUBLE, DataTypeId.OMNI_DOUBLE};
        DataTypeId outputType = DataTypeId.OMNI_DOUBLE;
        List<Long> inputInfoAddrs = createSingleInput(inputTypes, new Object[]{0.134D, 1.542D});

        long outputValueAddr = UdfUtil.UNSAFE.allocateMemory(Double.BYTES);
        long outputNullAddr = UdfUtil.UNSAFE.allocateMemory(1);
        long outputLengthAddr = 0L;

        HiveUdfExecutor.executeSingle(testJarPath, className, inputTypes, outputType, inputInfoAddrs.get(0),
                inputInfoAddrs.get(1), inputInfoAddrs.get(2), outputValueAddr, outputNullAddr, outputLengthAddr);
        assertEquals(UdfUtil.UNSAFE.getDouble(outputValueAddr), 1.542D);
        assertNotEquals(UdfUtil.UNSAFE.getByte(outputNullAddr), 1);
        releaseSingleInput(inputTypes, inputInfoAddrs.get(0), inputInfoAddrs.get(1), inputInfoAddrs.get(2));

        inputInfoAddrs = createSingleInput(inputTypes, new Object[]{null, 1.98D});
        HiveUdfExecutor.executeSingle(testJarPath, className, inputTypes, outputType, inputInfoAddrs.get(0),
                inputInfoAddrs.get(1), inputInfoAddrs.get(2), outputValueAddr, outputNullAddr, outputLengthAddr);
        assertEquals(UdfUtil.UNSAFE.getByte(outputNullAddr), 1);
        releaseSingleInput(inputTypes, inputInfoAddrs.get(0), inputInfoAddrs.get(1), inputInfoAddrs.get(2));

        releaseMemory(outputValueAddr, outputNullAddr, outputLengthAddr);
    }

    /**
     * test AddIntUDF
     */
    @Test
    public void testBatchAddIntUdf() {
        String className = "omniruntime.udf.AddIntUDF";
        DataTypeId[] inputTypes = {DataTypeId.OMNI_INT, DataTypeId.OMNI_INT};
        DataTypeId outputType = DataTypeId.OMNI_INT;
        Object[][] inputValues = {{9, -88, 34, null, 98, null}, {299, -8, -12, 67, null, null}};
        List<Long> inputInfoAddrs = createBatchInputs(inputTypes, inputValues);

        int rowCount = inputValues[0].length;
        long outputValueAddr = UdfUtil.UNSAFE.allocateMemory((long) rowCount * Integer.BYTES);
        long outputNullAddr = UdfUtil.UNSAFE.allocateMemory(rowCount);
        long outputLengthAddr = UdfUtil.UNSAFE.allocateMemory((long) rowCount * Integer.BYTES);

        HiveUdfExecutor.executeBatch(testJarPath, className, inputTypes, outputType, inputInfoAddrs.get(0),
                inputInfoAddrs.get(1), inputInfoAddrs.get(2), rowCount, outputValueAddr, outputNullAddr,
                outputLengthAddr, 0);

        Object[] expectValues = {308, -96, 22, null, null, null};
        assertUdfResultsEquals(outputType, outputValueAddr, outputNullAddr, outputLengthAddr, rowCount, expectValues);

        releaseBatchInputs(inputTypes, inputInfoAddrs.get(0), inputInfoAddrs.get(1), inputInfoAddrs.get(2), rowCount);
        releaseMemory(outputValueAddr, outputNullAddr, outputLengthAddr);
    }

    /**
     * test AddLongUDF
     */
    @Test
    public void testBatchAddLongUdf() {
        String className = "omniruntime.udf.AddLongUDF";
        DataTypeId[] inputTypes = {DataTypeId.OMNI_LONG, DataTypeId.OMNI_LONG};
        DataTypeId outputType = DataTypeId.OMNI_LONG;
        Object[][] inputValues = {{9L, -88L, 34L, null, 98L, null}, {299L, -8L, -12L, 67L, null, null}};
        List<Long> inputInfoAddrs = createBatchInputs(inputTypes, inputValues);

        int rowCount = inputValues[0].length;
        long outputValueAddr = UdfUtil.UNSAFE.allocateMemory((long) rowCount * Long.BYTES);
        long outputNullAddr = UdfUtil.UNSAFE.allocateMemory(rowCount);
        long outputLengthAddr = UdfUtil.UNSAFE.allocateMemory((long) rowCount * Integer.BYTES);

        HiveUdfExecutor.executeBatch(testJarPath, className, inputTypes, outputType, inputInfoAddrs.get(0),
                inputInfoAddrs.get(1), inputInfoAddrs.get(2), rowCount, outputValueAddr, outputNullAddr,
                outputLengthAddr, 0);

        Object[] expectValues = {308L, -96L, 22L, null, null, null};
        assertUdfResultsEquals(outputType, outputValueAddr, outputNullAddr, outputLengthAddr, rowCount, expectValues);

        releaseBatchInputs(inputTypes, inputInfoAddrs.get(0), inputInfoAddrs.get(1), inputInfoAddrs.get(2), rowCount);
        releaseMemory(outputValueAddr, outputNullAddr, outputLengthAddr);
    }

    /**
     * test AddShortUDF
     */
    @Test
    public void testBatchAddShortUdf() {
        String className = "omniruntime.udf.AddShortUDF";
        DataTypeId[] inputTypes = {DataTypeId.OMNI_SHORT, DataTypeId.OMNI_SHORT};
        DataTypeId outputType = DataTypeId.OMNI_SHORT;
        Object[][] inputValues = {{(short) 9, (short) -88, (short) 34, null, (short) 98, null},
                {(short) 299, (short) -8, (short) -12, (short) 67, null, null}};
        List<Long> inputInfoAddrs = createBatchInputs(inputTypes, inputValues);

        int rowCount = inputValues[0].length;
        long outputValueAddr = UdfUtil.UNSAFE.allocateMemory((long) rowCount * Short.BYTES);
        long outputNullAddr = UdfUtil.UNSAFE.allocateMemory(rowCount);
        long outputLengthAddr = UdfUtil.UNSAFE.allocateMemory((long) rowCount * Integer.BYTES);

        HiveUdfExecutor.executeBatch(testJarPath, className, inputTypes, outputType, inputInfoAddrs.get(0),
                inputInfoAddrs.get(1), inputInfoAddrs.get(2), rowCount, outputValueAddr, outputNullAddr,
                outputLengthAddr, 0);

        Object[] expectValues = {(short) 308, (short) -96, (short) 22, null, null, null};
        assertUdfResultsEquals(outputType, outputValueAddr, outputNullAddr, outputLengthAddr, rowCount, expectValues);

        releaseBatchInputs(inputTypes, inputInfoAddrs.get(0), inputInfoAddrs.get(1), inputInfoAddrs.get(2), rowCount);
        releaseMemory(outputValueAddr, outputNullAddr, outputLengthAddr);
    }

    /**
     * test AndBooleanUDF
     */
    @Test
    public void testBatchAndBooleanUdf() {
        String className = "omniruntime.udf.AndBooleanUDF";
        DataTypeId[] inputTypes = {DataTypeId.OMNI_BOOLEAN, DataTypeId.OMNI_BOOLEAN};
        DataTypeId outputType = DataTypeId.OMNI_BOOLEAN;
        Object[][] inputValues = {{true, false, true, null, true, null}, {true, false, false, true, null, null}};
        List<Long> inputInfoAddrs = createBatchInputs(inputTypes, inputValues);

        int rowCount = inputValues[0].length;
        long outputValueAddr = UdfUtil.UNSAFE.allocateMemory(rowCount * Byte.BYTES);
        long outputNullAddr = UdfUtil.UNSAFE.allocateMemory(rowCount);
        long outputLengthAddr = UdfUtil.UNSAFE.allocateMemory((long) rowCount * Integer.BYTES);

        HiveUdfExecutor.executeBatch(testJarPath, className, inputTypes, outputType, inputInfoAddrs.get(0),
                inputInfoAddrs.get(1), inputInfoAddrs.get(2), rowCount, outputValueAddr, outputNullAddr,
                outputLengthAddr, 0);

        Object[] expectValues = {true, false, false, null, null, null};
        assertUdfResultsEquals(outputType, outputValueAddr, outputNullAddr, outputLengthAddr, rowCount, expectValues);

        releaseBatchInputs(inputTypes, inputInfoAddrs.get(0), inputInfoAddrs.get(1), inputInfoAddrs.get(2), rowCount);
        releaseMemory(outputValueAddr, outputNullAddr, outputLengthAddr);
    }

    /**
     * test AddIntUDF with exception
     */
    @Test(expectedExceptions = OmniRuntimeException.class)
    public void testBatchAddIntUdfException() {
        String className = "omniruntime.udf.AddIntUDF";
        DataTypeId[] inputTypes = {DataTypeId.OMNI_INT, DataTypeId.OMNI_INT};
        DataTypeId outputType = DataTypeId.OMNI_INT;
        Object[][] inputValues = {{2, Integer.MIN_VALUE}, {Integer.MAX_VALUE, -8}};
        List<Long> inputInfoAddrs = createBatchInputs(inputTypes, inputValues);

        int rowCount = inputValues[0].length;
        long outputValueAddr = UdfUtil.UNSAFE.allocateMemory((long) rowCount * Integer.BYTES);
        long outputNullAddr = UdfUtil.UNSAFE.allocateMemory(rowCount);
        long outputLengthAddr = UdfUtil.UNSAFE.allocateMemory((long) rowCount * Integer.BYTES);

        HiveUdfExecutor.executeBatch(testJarPath, className, inputTypes, outputType, inputInfoAddrs.get(0),
                inputInfoAddrs.get(1), inputInfoAddrs.get(2), rowCount, outputValueAddr, outputNullAddr,
                outputLengthAddr, 0);

        releaseBatchInputs(inputTypes, inputInfoAddrs.get(0), inputInfoAddrs.get(1), inputInfoAddrs.get(2), rowCount);
        releaseMemory(outputValueAddr, outputNullAddr, outputLengthAddr);
    }

    /**
     * test ConcatStringUDF
     */
    @Test
    public void testBatchConcatStringUdf() {
        DataTypeId[] inputTypes = {DataTypeId.OMNI_VARCHAR, DataTypeId.OMNI_VARCHAR};
        Object[][] inputValues = {{null, "Jimmy", null, "", "John", "", "world"},
                {"John", null, null, "", "Jimmy", "hello", ""}};
        List<Long> inputInfoAddrs = createBatchInputs(inputTypes, inputValues);

        DataTypeId outputType = DataTypeId.OMNI_VARCHAR;
        int rowCount = inputValues[0].length;
        long outputValueAddr = UdfUtil.UNSAFE.allocateMemory(10);
        long outputNullAddr = UdfUtil.UNSAFE.allocateMemory(rowCount);
        long outputLengthAddr = UdfUtil.UNSAFE.allocateMemory((long) rowCount * Integer.BYTES);
        long outputStateAddr = UdfUtil.UNSAFE.allocateMemory(2 * Integer.BYTES);
        UdfUtil.UNSAFE.putInt(outputStateAddr, 10);
        UdfUtil.UNSAFE.putInt(outputStateAddr + Integer.BYTES, 0);

        String className = "omniruntime.udf.ConcatStringUDF";
        HiveUdfExecutor.executeBatch(testJarPath, className, inputTypes, outputType, inputInfoAddrs.get(0),
                inputInfoAddrs.get(1), inputInfoAddrs.get(2), rowCount, outputValueAddr, outputNullAddr,
                outputLengthAddr, outputStateAddr);

        Object[] expectValues = {null, null, null, "", "JohnJimmy"};
        assertUdfResultsEquals(outputType, outputValueAddr, outputNullAddr, outputLengthAddr, expectValues.length,
                expectValues);
        assertEquals(UdfUtil.UNSAFE.getInt(outputStateAddr + Integer.BYTES), expectValues.length);

        HiveUdfExecutor.executeBatch(testJarPath, className, inputTypes, outputType, inputInfoAddrs.get(0),
                inputInfoAddrs.get(1), inputInfoAddrs.get(2), rowCount, outputValueAddr, outputNullAddr,
                outputLengthAddr, outputStateAddr);
        assertEquals(UdfUtil.UNSAFE.getInt(outputStateAddr + Integer.BYTES), rowCount);
        expectValues = new Object[]{"hello", "world"};
        assertEquals(UdfUtil.getBytes(outputValueAddr, 0, 5),
                ((String) expectValues[0]).getBytes(StandardCharsets.UTF_8));
        assertEquals(UdfUtil.getBytes(outputValueAddr, 5, 5),
                ((String) expectValues[1]).getBytes(StandardCharsets.UTF_8));
        assertEquals(UdfUtil.UNSAFE.getByte(outputNullAddr + 5), 0);
        assertEquals(UdfUtil.UNSAFE.getByte(outputNullAddr + 6), 0);
        assertEquals(UdfUtil.UNSAFE.getInt(outputLengthAddr + 5 * Integer.BYTES), 5);
        assertEquals(UdfUtil.UNSAFE.getInt(outputLengthAddr + 6 * Integer.BYTES), 5);

        releaseBatchInputs(inputTypes, inputInfoAddrs.get(0), inputInfoAddrs.get(1), inputInfoAddrs.get(2), rowCount);
        releaseMemory(outputValueAddr, outputNullAddr, outputLengthAddr);
    }

    /**
     * test MaxDoubleUDF
     */
    @Test
    public void testBatchMaxDoubleUdf() {
        String className = "omniruntime.udf.MaxDoubleUDF";
        DataTypeId[] inputTypes = {DataTypeId.OMNI_DOUBLE, DataTypeId.OMNI_DOUBLE};
        DataTypeId outputType = DataTypeId.OMNI_DOUBLE;
        Object[][] inputValues = {{9D, -88D, 34D, null, 98D, null}, {299D, -8D, -12D, 67D, null, null}};
        List<Long> inputInfoAddrs = createBatchInputs(inputTypes, inputValues);

        int rowCount = inputValues[0].length;
        long outputValueAddr = UdfUtil.UNSAFE.allocateMemory((long) rowCount * Double.BYTES);
        long outputNullAddr = UdfUtil.UNSAFE.allocateMemory(rowCount);
        long outputLengthAddr = UdfUtil.UNSAFE.allocateMemory((long) rowCount * Integer.BYTES);

        HiveUdfExecutor.executeBatch(testJarPath, className, inputTypes, outputType, inputInfoAddrs.get(0),
                inputInfoAddrs.get(1), inputInfoAddrs.get(2), rowCount, outputValueAddr, outputNullAddr,
                outputLengthAddr, 0);

        Object[] expectValues = {299D, -8D, 34D, null, null, null};
        assertUdfResultsEquals(outputType, outputValueAddr, outputNullAddr, outputLengthAddr, rowCount, expectValues);

        releaseBatchInputs(inputTypes, inputInfoAddrs.get(0), inputInfoAddrs.get(1), inputInfoAddrs.get(2), rowCount);
        releaseMemory(outputValueAddr, outputNullAddr, outputLengthAddr);
    }

    /**
     * test the UDF which is not loaded
     */
    @Test(expectedExceptions = OmniRuntimeException.class)
    public void testBatchExecuteUdfWithoutLoad() {
        DataTypeId[] inputTypes = {DataTypeId.OMNI_VARCHAR, DataTypeId.OMNI_VARCHAR};
        DataTypeId outputType = DataTypeId.OMNI_VARCHAR;
        Object[][] inputValues = {{"John"}, {"Jimmy"}};
        List<Long> inputInfoAddrs = createBatchInputs(inputTypes, inputValues);

        int rowCount = inputValues[0].length;
        long outputValueAddr = UdfUtil.UNSAFE.allocateMemory(20);
        long outputNullAddr = UdfUtil.UNSAFE.allocateMemory(rowCount);
        long outputLengthAddr = UdfUtil.UNSAFE.allocateMemory((long) rowCount * Integer.BYTES);

        HiveUdfExecutor.executeBatch(testJarPath, "NonLoadedUDF", inputTypes, outputType, inputInfoAddrs.get(0),
                inputInfoAddrs.get(1), inputInfoAddrs.get(2), rowCount, outputValueAddr, outputNullAddr,
                outputLengthAddr, 0);

        releaseBatchInputs(inputTypes, inputInfoAddrs.get(0), inputInfoAddrs.get(1), inputInfoAddrs.get(2), rowCount);
        releaseMemory(outputValueAddr, outputNullAddr, outputLengthAddr);
    }

    /**
     * test the UDF with unmatched param types or return type
     */
    @Test(expectedExceptions = OmniRuntimeException.class)
    public void testBatchExecuteUdfWithUnmatchedDataTypes() {
        String className = "omniruntime.udf.ConcatStringUDF";
        DataTypeId[] inputTypes = {DataTypeId.OMNI_INT, DataTypeId.OMNI_LONG};
        DataTypeId outputType = DataTypeId.OMNI_DOUBLE;
        Object[][] inputValues = {{1, 2, 3}, {-1L, -2L, -3L}};
        List<Long> inputInfoAddrs = createBatchInputs(inputTypes, inputValues);

        int rowCount = inputValues[0].length;
        long outputValueAddr = UdfUtil.UNSAFE.allocateMemory((long) rowCount * Double.BYTES);
        long outputNullAddr = UdfUtil.UNSAFE.allocateMemory(rowCount);
        long outputLengthAddr = UdfUtil.UNSAFE.allocateMemory((long) rowCount * Integer.BYTES);

        HiveUdfExecutor.executeBatch(testJarPath, className, inputTypes, outputType, inputInfoAddrs.get(0),
                inputInfoAddrs.get(1), inputInfoAddrs.get(2), rowCount, outputValueAddr, outputNullAddr,
                outputLengthAddr, 0);

        releaseBatchInputs(inputTypes, inputInfoAddrs.get(0), inputInfoAddrs.get(1), inputInfoAddrs.get(2), rowCount);
        releaseMemory(outputValueAddr, outputNullAddr, outputLengthAddr);
    }
}
