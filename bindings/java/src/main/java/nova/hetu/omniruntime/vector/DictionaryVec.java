/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2020. All rights reserved.
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

package nova.hetu.omniruntime.vector;

import static nova.hetu.omniruntime.vector.VariableWidthVec.getValueOffsetsNative;

import com.google.common.annotations.VisibleForTesting;

import nova.hetu.omniruntime.type.DataType;
import sun.misc.Unsafe;

/**
 * dictionary vec.
 *
 * @since 2021-07-17
 */
public class DictionaryVec extends FixedWidthVec {
    private static final int BYTES = Integer.BYTES;

    private Vec dictionary;

    private long dataAddress;
    private long offsetsAddress;

    /**
     * The routine will use native vector to initialize a new dictionary vector.
     *
     * @param nativeVector native dictionary vector address
     * @param dataType vector datatype
     */
    public DictionaryVec(long nativeVector, DataType dataType) {
        super(nativeVector, dataType, BYTES);
    }

    /**
     * The routine will use native vector to initialize a new dictionary vector.
     *
     * @param nativeVector native vector address
     * @param nativeValueBufAddress valueBuf address of native vector
     * @param nativeVectorNullBufAddress nullBuf address of native vector
     * @param size the actual number of value of vector(ids)
     * @param dataType the dataType of native vector
     */
    public DictionaryVec(long nativeVector, long nativeValueBufAddress, long nativeVectorNullBufAddress, int size,
            DataType dataType) {
        super(nativeVector, nativeValueBufAddress, nativeVectorNullBufAddress, size * BYTES, size, dataType);
        dataAddress = getDictionaryNative(nativeVector, getType().getId().toValue());
    }

    /**
     * The routine will use the specialized vector allocator to allocate new vector.
     *
     * @param dictionary the specialized vector
     * @param ids the int array
     */
    public DictionaryVec(Vec dictionary, int[] ids) {
        super(dictionary, ids, ids.length * BYTES, dictionary.getType());
        dataAddress = getDictionaryNative(getNativeVector(), getType().getId().toValue());
    }

    private DictionaryVec(DictionaryVec vector, int offset, int length) {
        super(vector, offset, length, length * BYTES);
        dataAddress = getDictionaryNative(vector.getNativeVector(), vector.getType().getId().toValue());
    }

    private DictionaryVec(DictionaryVec vector, int[] positions, int offset, int length) {
        super(vector, positions, offset, length, length * BYTES);
        dataAddress = getDictionaryNative(vector.getNativeVector(), vector.getType().getId().toValue());
    }

    private static native long getDictionaryNative(long nativeVector, int dataTypeId);

    public Vec getDictionary() {
        return dictionary;
    }

    /**
     * for the UT of getDictionaryNative with empty strings.
     *
     * @return the address of dictionary container
     */
    @VisibleForTesting
    public long getDataAddress() {
        return dataAddress;
    }

    /**
     * v2 need expand dictionary
     * @return expanded vector
     */
    public Vec expandDictionary() {
        int size = getSize();
        DataType dataType = getType();
        Vec vector = VecFactory.createFlatVec(size, dataType);
        vector.setNulls(0, getValuesNulls(0, size), 0, size);
        switch (dataType.getId()) {
            case OMNI_INT:
            case OMNI_DATE32:
                setValue(size, (IntVec) vector);
                break;
            case OMNI_LONG:
            case OMNI_TIMESTAMP:
            case OMNI_DATE64:
            case OMNI_DECIMAL64:
                setValue(size, (LongVec) vector);
                break;
            case OMNI_DOUBLE:
                setValue(size, (DoubleVec) vector);
                break;
            case OMNI_SHORT:
                setValue(size, (ShortVec) vector);
                break;
            case OMNI_BYTE:
                setValue(size, (ByteVec) vector);
                break;
            case OMNI_BOOLEAN:
                setValue(size, (BooleanVec) vector);
                break;
            case OMNI_VARCHAR:
            case OMNI_CHAR:
                setValue(size, (VarcharVec) vector);
                break;
            case OMNI_DECIMAL128:
                setValue(size, (Decimal128Vec) vector);
                break;
            default:
                throw new IllegalArgumentException("Not Support Data Type " + dataType.getId());
        }
        return vector;
    }

    private void setValue(int size, Decimal128Vec vector) {
        for (int i = 0; i < size; i++) {
            if (!vector.isNull(i)) {
                vector.set(i, getDecimal128(i));
            }
        }
    }

    private void setValue(int size, VarcharVec vector) {
        for (int i = 0; i < size; i++) {
            if (!vector.isNull(i)) {
                vector.set(i, getBytes(i));
            } else {
                vector.setNull(i);
            }
        }
    }

    private void setValue(int size, BooleanVec vector) {
        for (int i = 0; i < size; i++) {
            if (!vector.isNull(i)) {
                vector.set(i, getBoolean(i));
            }
        }
    }

    private void setValue(int size, ShortVec vector) {
        for (int i = 0; i < size; i++) {
            if (!vector.isNull(i)) {
                vector.set(i, getShort(i));
            }
        }
    }

    private void setValue(int size, ByteVec vector) {
        for (int i = 0; i < size; i++) {
            if (!vector.isNull(i)) {
                vector.set(i, getByte(i));
            }
        }
    }

    private void setValue(int size, DoubleVec vector) {
        for (int i = 0; i < size; i++) {
            if (!vector.isNull(i)) {
                vector.set(i, getDouble(i));
            }
        }
    }

    private void setValue(int size, LongVec vector) {
        for (int i = 0; i < size; i++) {
            if (!vector.isNull(i)) {
                vector.set(i, getLong(i));
            }
        }
    }

    private void setValue(int size, IntVec vector) {
        for (int i = 0; i < size; i++) {
            if (!vector.isNull(i)) {
                vector.set(i, getInt(i));
            }
        }
    }

    /**
     * get ids from valuesBuf
     *
     * @return ids array
     * */
    public int[] getIds() {
        int[] ids = new int[size];
        valuesBuf.getIntArray(0, ids, 0, size * BYTES);
        return ids;
    }

    /**
     * get the specified integer at the specified absolute.
     *
     * @param index the element offset in vec
     * @return int value
     */
    public int getId(int index) {
        return valuesBuf.getInt(index * BYTES);
    }

    /**
     * get the specified short at the specified absolute.
     *
     * @param index the element offset in vec
     * @return short value
     */
    public short getShort(int index) {
        int originIndex = getId(index);
        return JvmUtils.UNSAFE.getShort(dataAddress + originIndex * Short.BYTES);
    }

    /**
     * get the specified byte at the specified absolute.
     *
     * @param index the element offset in vec
     * @return byte value
     */
    public byte getByte(int index) {
        int originIndex = getId(index);
        return JvmUtils.UNSAFE.getByte(dataAddress + originIndex * Byte.BYTES);
    }

    /**
     * get the specified integer at the specified absolute.
     *
     * @param index the element offset in vec
     * @return integer value
     */
    public int getInt(int index) {
        int originIndex = getId(index);
        return JvmUtils.UNSAFE.getInt(dataAddress + originIndex * Integer.BYTES);
    }

    /**
     * get the specified long at the specified absolute.
     *
     * @param index the element offset in vec
     * @return long value
     */
    public long getLong(int index) {
        int originIndex = getId(index);
        return JvmUtils.UNSAFE.getLong(dataAddress + originIndex * Long.BYTES);
    }

    /**
     * get the specified double at the specified absolute.
     *
     * @param index the element offset in vec
     * @return double value
     */
    public double getDouble(int index) {
        int originIndex = getId(index);
        return JvmUtils.UNSAFE.getDouble(dataAddress + originIndex * Double.BYTES);
    }

    /**
     * get the specified boolean at the specified absolute.
     *
     * @param index the element offset in vec
     * @return boolean value
     */
    public boolean getBoolean(int index) {
        int originIndex = getId(index);
        return JvmUtils.UNSAFE.getByte(dataAddress + originIndex) == 1;
    }

    /**
     * get the offset value of the specified position.
     *
     * @param index the element offset in vec
     * @return offset value
     */
    public int getValueOffset(int index) {
        return JvmUtils.UNSAFE.getInt(offsetsAddress + index * Integer.BYTES);
    }

    /**
     * get the specified bytes at the specified absolute.
     *
     * @param index the element offset in vec
     * @return byte array
     */
    public byte[] getBytes(int index) {
        if (offsetsAddress == 0) {
            offsetsAddress = getValueOffsetsNative(nativeVector);
        }

        int originIndex = getId(index);
        final int stringLen = getValueOffset(originIndex + 1) - getValueOffset(originIndex);
        final long stringAddr = dataAddress + getValueOffset(originIndex);
        byte[] target = new byte[stringLen];
        JvmUtils.UNSAFE.copyMemory(null, stringAddr, target, Unsafe.ARRAY_BYTE_BASE_OFFSET, stringLen);
        return target;
    }

    /**
     * get the specified decimal at the specified absolute.
     *
     * @param index the element offset in vec
     * @return long array
     */
    public long[] getDecimal128(int index) {
        int originIndex = getId(index);
        long[] value = new long[2];
        int valueIndex = originIndex * 2;
        for (int i = 0; i < 2; i++) {
            value[i] = JvmUtils.UNSAFE.getLong(dataAddress + (valueIndex + i) * Long.BYTES);
        }
        return value;
    }

    @Override
    public DictionaryVec slice(int start, int length) {
        return new DictionaryVec(this, start, length);
    }

    @Override
    public DictionaryVec copyPositions(int[] positions, int offset, int length) {
        return new DictionaryVec(this, positions, offset, length);
    }

    @Override
    public int getRealValueBufCapacityInBytes() {
        return size * BYTES;
    }

    @Override
    public VecEncoding getEncoding() {
        return VecEncoding.OMNI_VEC_ENCODING_DICTIONARY;
    }
}
