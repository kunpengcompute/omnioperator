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

import static org.testng.Assert.assertEquals;
import static org.testng.Assert.assertNull;

import nova.hetu.omniruntime.type.DataType.DataTypeId;

import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;

/**
 * Hive UDF test util
 *
 * @since 2022-8-3
 */
public class HiveUdfTestUtil {
    /**
     * Allocate off-heap memory and set values for single input
     *
     * @param inputTypes the data type
     * @param inputValues the address of actual values
     * @return return the value, null, length address
     */
    public static List<Long> createSingleInput(DataTypeId[] inputTypes, Object[] inputValues) {
        List<Integer> offsets = getDataTypesOffsets(inputTypes);
        int inputTypeSize = inputTypes.length;
        long valueAddr = UdfUtil.UNSAFE.allocateMemory(offsets.get(inputTypeSize));
        long nullAddr = UdfUtil.UNSAFE.allocateMemory(inputTypeSize);
        long lengthAddr = UdfUtil.UNSAFE.allocateMemory((long) inputTypeSize * Integer.BYTES);

        for (int i = 0; i < inputTypeSize; i++) {
            int offset = offsets.get(i);
            Object value = inputValues[i];
            UdfUtil.UNSAFE.putByte(nullAddr + i, (byte) (value == null ? 1 : 0));
            switch (inputTypes[i]) {
                case OMNI_INT:
                case OMNI_DATE32:
                    UdfUtil.UNSAFE.putInt(valueAddr + offset, value == null ? 0 : (int) value);
                    break;
                case OMNI_LONG:
                    UdfUtil.UNSAFE.putLong(valueAddr + offset, value == null ? 0 : (long) value);
                    break;
                case OMNI_VARCHAR:
                case OMNI_CHAR: {
                    // for char or varchar, we store pointer
                    byte[] chars = (value == null ? "" : (String) value).getBytes(StandardCharsets.UTF_8);
                    long ptr = UdfUtil.UNSAFE.allocateMemory(100);
                    UdfUtil.UNSAFE.putLong(valueAddr + offset, ptr);
                    UdfUtil.putBytes(ptr, 0, chars);
                    UdfUtil.UNSAFE.putInt(lengthAddr + (long) i * Integer.BYTES, chars.length);
                    break;
                }
                case OMNI_DOUBLE:
                    UdfUtil.UNSAFE.putDouble(valueAddr + offset, value == null ? 0 : (double) value);
                    break;
                case OMNI_BOOLEAN:
                    UdfUtil.UNSAFE.putByte(valueAddr + offset, value == null ? 0 : (byte) value);
                    break;
                case OMNI_SHORT:
                    UdfUtil.UNSAFE.putShort(valueAddr + offset, value == null ? 0 : (short) value);
                    break;
                default:
                    break;
            }
        }

        List<Long> result = new ArrayList<>();
        result.add(valueAddr);
        result.add(nullAddr);
        result.add(lengthAddr);
        return result;
    }

    /**
     * Release off-heap memory for single input
     *
     * @param inputTypes the input data types
     * @param valueAddr the address of value
     * @param nullAddr the address of null
     * @param lengthAddr the address of length
     */
    public static void releaseSingleInput(DataTypeId[] inputTypes, long valueAddr, long nullAddr, long lengthAddr) {
        List<Integer> offsets = getDataTypesOffsets(inputTypes);
        for (int i = 0; i < inputTypes.length; i++) {
            if (inputTypes[i] == DataTypeId.OMNI_VARCHAR || inputTypes[i] == DataTypeId.OMNI_CHAR) {
                long valueStrAddr = UdfUtil.UNSAFE.getLong(valueAddr + offsets.get(i));
                UdfUtil.UNSAFE.freeMemory(valueStrAddr);
            }
        }
        UdfUtil.UNSAFE.freeMemory(valueAddr);
        UdfUtil.UNSAFE.freeMemory(nullAddr);
        UdfUtil.UNSAFE.freeMemory(lengthAddr);
    }

    private static List<Integer> getDataTypesOffsets(DataTypeId[] dataTypeIds) {
        List<Integer> offsets = new ArrayList<>();
        int offset = 0;
        for (DataTypeId typeId : dataTypeIds) {
            offsets.add(offset);
            switch (typeId) {
                case OMNI_INT:
                case OMNI_DATE32:
                    offset += Integer.BYTES;
                    break;
                case OMNI_LONG:
                case OMNI_DECIMAL64:
                case OMNI_VARCHAR:
                case OMNI_CHAR:
                    offset += Long.BYTES;
                    break;
                case OMNI_DOUBLE:
                    offset += Double.BYTES;
                    break;
                case OMNI_BOOLEAN:
                    offset += Byte.BYTES;
                    break;
                case OMNI_SHORT:
                    offset += Short.BYTES;
                    break;
                case OMNI_DECIMAL128:
                    offset += 2 * Long.BYTES;
                    break;
                default:
                    break;
            }
        }
        offsets.add(offset);
        return offsets;
    }

    private static long allocateValuesMemory(DataTypeId type, int size) {
        switch (type) {
            case OMNI_INT:
                return UdfUtil.UNSAFE.allocateMemory((long) size * Integer.BYTES);
            case OMNI_LONG:
                return UdfUtil.UNSAFE.allocateMemory((long) size * Long.BYTES);
            case OMNI_DOUBLE:
                return UdfUtil.UNSAFE.allocateMemory((long) size * Double.BYTES);
            case OMNI_BOOLEAN:
                return UdfUtil.UNSAFE.allocateMemory((long) size * Byte.BYTES);
            case OMNI_SHORT:
                return UdfUtil.UNSAFE.allocateMemory((long) size * Short.BYTES);
            case OMNI_VARCHAR:
            case OMNI_CHAR: {
                long charAddrs = UdfUtil.UNSAFE.allocateMemory((long) size * Long.BYTES);
                for (int i = 0; i < size; i++) {
                    UdfUtil.UNSAFE.putLong(charAddrs + (long) i * Long.BYTES, UdfUtil.UNSAFE.allocateMemory(100));
                }
                return charAddrs;
            }
            default:
                return 0;
        }
    }

    /**
     * Allocate off heap memory and set values
     *
     * @param inputTypes the input data types
     * @param inputValues the input values
     * @return the memory addresses for values, nulls, lengths
     */
    public static List<Long> createBatchInputs(DataTypeId[] inputTypes, Object[][] inputValues) {
        int inputTypeSize = inputTypes.length;
        long inputValuesAddr = UdfUtil.UNSAFE.allocateMemory(inputTypeSize * 8L);
        long inputNullsAddr = UdfUtil.UNSAFE.allocateMemory(inputTypeSize * 8L);
        long inputLengthsAddr = UdfUtil.UNSAFE.allocateMemory(inputTypeSize * 8L);

        List<Long> inputInfoAddrs = new ArrayList<>(3); // address of input values, input nulls, input lengths
        inputInfoAddrs.add(inputValuesAddr);
        inputInfoAddrs.add(inputNullsAddr);
        inputInfoAddrs.add(inputLengthsAddr);

        int rowCount = inputValues[0].length;
        for (int i = 0; i < inputTypeSize; i++) {
            long nullsAddr = UdfUtil.UNSAFE.allocateMemory(rowCount * Byte.BYTES);
            long lengthsAddr = UdfUtil.UNSAFE.allocateMemory((long) rowCount * Integer.BYTES);
            long valuesAddr = allocateValuesMemory(inputTypes[i], rowCount);
            setValues(inputTypes[i], inputValues[i], valuesAddr, nullsAddr, lengthsAddr);

            UdfUtil.UNSAFE.putLong(inputValuesAddr + (long) i * Long.BYTES, valuesAddr);
            UdfUtil.UNSAFE.putLong(inputNullsAddr + (long) i * Long.BYTES, nullsAddr);
            UdfUtil.UNSAFE.putLong(inputLengthsAddr + (long) i * Long.BYTES, lengthsAddr);
        }

        return inputInfoAddrs;
    }

    /**
     * Release off heap memory for given values addr, nulls addr, lengths addr of
     * multiple columns
     *
     * @param inputTypes the input data types
     * @param inputValuesAddr the address of values
     * @param inputNullsAddr the address of nulls
     * @param inputLengthsAddr the address of lengths
     * @param rowCount the row count
     */
    public static void releaseBatchInputs(DataTypeId[] inputTypes, long inputValuesAddr, long inputNullsAddr,
            long inputLengthsAddr, int rowCount) {
        int inputTypeSize = inputTypes.length;
        long[] valueAddrs = UdfUtil.getLongs(inputValuesAddr, 0, inputTypeSize);
        long[] nullAddrs = UdfUtil.getLongs(inputNullsAddr, 0, inputTypeSize);
        long[] lengthAddrs = UdfUtil.getLongs(inputLengthsAddr, 0, inputTypeSize);

        for (int i = 0; i < inputTypeSize; i++) {
            if (inputTypes[i] == DataTypeId.OMNI_VARCHAR || inputTypes[i] == DataTypeId.OMNI_CHAR) {
                for (int rowIdx = 0; rowIdx < rowCount; rowIdx++) {
                    long valueAddr = UdfUtil.UNSAFE.getLong(valueAddrs[i] + (long) rowIdx * Long.BYTES);
                    UdfUtil.UNSAFE.freeMemory(valueAddr);
                }
            }
            releaseMemory(valueAddrs[i], nullAddrs[i], lengthAddrs[i]);
        }
        releaseMemory(inputValuesAddr, inputNullsAddr, inputLengthsAddr);
    }

    /**
     * Release off heap memory for given values addr, nulls addr, lengths addr of
     * single column
     *
     * @param valueAddr the address of values
     * @param nullAddr the address of nulls
     * @param lengthAddr the address of lengths
     */
    public static void releaseMemory(long valueAddr, long nullAddr, long lengthAddr) {
        UdfUtil.UNSAFE.freeMemory(valueAddr);
        UdfUtil.UNSAFE.freeMemory(nullAddr);
        UdfUtil.UNSAFE.freeMemory(lengthAddr);
    }

    private static void setValues(DataTypeId inputType, Object[] inputValue, long valuesAddr, long nullsAddr,
            long lengthsAddr) {
        switch (inputType) {
            case OMNI_INT:
                setIntValues(inputValue, valuesAddr, nullsAddr);
                break;
            case OMNI_LONG:
                setLongValues(inputValue, valuesAddr, nullsAddr);
                break;
            case OMNI_DOUBLE:
                setDoubleValues(inputValue, valuesAddr, nullsAddr);
                break;
            case OMNI_BOOLEAN:
                setBooleanValues(inputValue, valuesAddr, nullsAddr);
                break;
            case OMNI_SHORT:
                setShortValues(inputValue, valuesAddr, nullsAddr);
                break;
            case OMNI_VARCHAR:
            case OMNI_CHAR:
                setStringValues(inputValue, valuesAddr, nullsAddr, lengthsAddr);
                break;
            default:
                break;
        }
    }

    private static void setIntValues(Object[] inputValue, long valuesAddr, long nullsAddr) {
        for (int i = 0; i < inputValue.length; i++) {
            boolean isNull = inputValue[i] == null;
            UdfUtil.UNSAFE.putInt(valuesAddr + (long) i * Integer.BYTES, isNull ? 0 : (int) inputValue[i]);
            UdfUtil.UNSAFE.putByte(nullsAddr + i, (byte) (isNull ? 1 : 0));
        }
    }

    private static void setLongValues(Object[] inputValue, long valuesAddr, long nullsAddr) {
        for (int i = 0; i < inputValue.length; i++) {
            boolean isNull = inputValue[i] == null;
            UdfUtil.UNSAFE.putLong(valuesAddr + (long) i * Long.BYTES, isNull ? 0 : (long) inputValue[i]);
            UdfUtil.UNSAFE.putByte(nullsAddr + i, (byte) (isNull ? 1 : 0));
        }
    }

    private static void setDoubleValues(Object[] inputValue, long valuesAddr, long nullsAddr) {
        for (int i = 0; i < inputValue.length; i++) {
            boolean isNull = inputValue[i] == null;
            UdfUtil.UNSAFE.putDouble(valuesAddr + (long) i * Double.BYTES, isNull ? 0 : (double) inputValue[i]);
            UdfUtil.UNSAFE.putByte(nullsAddr + i, (byte) (isNull ? 1 : 0));
        }
    }

    private static void setBooleanValues(Object[] inputValue, long valuesAddr, long nullsAddr) {
        for (int i = 0; i < inputValue.length; i++) {
            boolean isNull = inputValue[i] == null;
            UdfUtil.UNSAFE.putByte(valuesAddr + i, (byte) (isNull ? 0 : (boolean) inputValue[i] ? 1 : 0));
            UdfUtil.UNSAFE.putByte(nullsAddr + i, (byte) (isNull ? 1 : 0));
        }
    }

    private static void setShortValues(Object[] inputValue, long valuesAddr, long nullsAddr) {
        for (int i = 0; i < inputValue.length; i++) {
            boolean isNull = inputValue[i] == null;
            UdfUtil.UNSAFE.putShort(valuesAddr + (long) i * Short.BYTES, isNull ? 0 : (short) inputValue[i]);
            UdfUtil.UNSAFE.putByte(nullsAddr + i, (byte) (isNull ? 1 : 0));
        }
    }

    private static void setStringValues(Object[] inputValue, long valuesAddr, long nullsAddr, long lengthsAddr) {
        for (int i = 0; i < inputValue.length; i++) {
            boolean isNull = inputValue[i] == null;
            long valueAddr = UdfUtil.UNSAFE.getLong(valuesAddr + (long) i * Long.BYTES);
            byte[] chars = (isNull ? "" : (String) inputValue[i]).getBytes(StandardCharsets.UTF_8);
            UdfUtil.putBytes(valueAddr, 0, chars);
            UdfUtil.UNSAFE.putByte(nullsAddr + i, (byte) (isNull ? 1 : 0));
            UdfUtil.UNSAFE.putInt(lengthsAddr + (long) i * Integer.BYTES, chars.length);
        }
    }

    /**
     * Allocate off heap memory and set values
     *
     * @param type the data type
     * @param actualValues the address of actual values
     * @param actualNulls the address of actual nulls
     * @param actualLengths the address of actual lengths
     * @param rowCount the row count
     * @param expectValues the expected values
     */
    public static void assertUdfResultsEquals(DataTypeId type, long actualValues, long actualNulls, long actualLengths,
            int rowCount, Object[] expectValues) {
        assertEquals(rowCount, expectValues.length);

        switch (type) {
            case OMNI_INT:
                assertIntResultsEquals(actualValues, actualNulls, expectValues);
                break;
            case OMNI_LONG:
                assertLongResultsEquals(actualValues, actualNulls, expectValues);
                break;
            case OMNI_DOUBLE:
                assertDoubleResultsEquals(actualValues, actualNulls, expectValues);
                break;
            case OMNI_BOOLEAN:
                assertBooleanResultsEquals(actualValues, actualNulls, expectValues);
                break;
            case OMNI_SHORT:
                assertShortResultsEquals(actualValues, actualNulls, expectValues);
                break;
            case OMNI_VARCHAR:
            case OMNI_CHAR:
                assertStringResultsEquals(actualValues, actualNulls, actualLengths, expectValues);
                break;
            default:
                break;
        }
    }

    private static void assertIntResultsEquals(long actualValues, long actualNulls, Object[] expectValues) {
        int rowCount = expectValues.length;
        for (int i = 0; i < rowCount; i++) {
            if (UdfUtil.UNSAFE.getByte(actualNulls + i) == 1) {
                assertNull(expectValues[i]);
            } else {
                assertEquals(UdfUtil.UNSAFE.getInt(actualValues + (long) i * Integer.BYTES), (int) expectValues[i]);
            }
        }
    }

    private static void assertLongResultsEquals(long actualValues, long actualNulls, Object[] expectValues) {
        int rowCount = expectValues.length;
        for (int i = 0; i < rowCount; i++) {
            if (UdfUtil.UNSAFE.getByte(actualNulls + i) == 1) {
                assertNull(expectValues[i]);
            } else {
                assertEquals(UdfUtil.UNSAFE.getLong(actualValues + (long) i * Long.BYTES), (long) expectValues[i]);
            }
        }
    }

    private static void assertDoubleResultsEquals(long actualValues, long actualNulls, Object[] expectValues) {
        int rowCount = expectValues.length;
        for (int i = 0; i < rowCount; i++) {
            if (UdfUtil.UNSAFE.getByte(actualNulls + i) == 1) {
                assertNull(expectValues[i]);
            } else {
                assertEquals(UdfUtil.UNSAFE.getDouble(actualValues + (long) i * Double.BYTES), expectValues[i]);
            }
        }
    }

    private static void assertBooleanResultsEquals(long actualValues, long actualNulls, Object[] expectValues) {
        int rowCount = expectValues.length;
        for (int i = 0; i < rowCount; i++) {
            if (UdfUtil.UNSAFE.getByte(actualNulls + i) == 1) {
                assertNull(expectValues[i]);
            } else {
                assertEquals(UdfUtil.UNSAFE.getByte(actualValues + i), (boolean) expectValues[i] ? 1 : 0);
            }
        }
    }

    private static void assertShortResultsEquals(long actualValues, long actualNulls, Object[] expectValues) {
        int rowCount = expectValues.length;
        for (int i = 0; i < rowCount; i++) {
            if (UdfUtil.UNSAFE.getByte(actualNulls + i) == 1) {
                assertNull(expectValues[i]);
            } else {
                assertEquals(UdfUtil.UNSAFE.getShort(actualValues + (long) i * Short.BYTES), (short) expectValues[i]);
            }
        }
    }

    private static void assertStringResultsEquals(long actualValues, long actualNulls, long actualLengths,
            Object[] expectValues) {
        int rowCount = expectValues.length;
        int offset = 0;
        for (int i = 0; i < rowCount; i++) {
            if (UdfUtil.UNSAFE.getByte(actualNulls + i) == 1) {
                assertNull(expectValues[i]);
            } else {
                int length = UdfUtil.UNSAFE.getInt(actualLengths + (long) i * Integer.BYTES);
                assertEquals(UdfUtil.getBytes(actualValues, offset, length),
                        ((String) expectValues[i]).getBytes(StandardCharsets.UTF_8));
                offset += length;
            }
        }
    }
}
