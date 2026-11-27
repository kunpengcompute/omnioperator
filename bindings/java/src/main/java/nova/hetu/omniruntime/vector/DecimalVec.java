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

package nova.hetu.omniruntime.vector;

import static nova.hetu.omniruntime.utils.OmniErrorType.OMNI_PARAM_ERROR;

import nova.hetu.omniruntime.type.DataType;
import nova.hetu.omniruntime.utils.OmniRuntimeException;

/**
 * base class of decimal vec.
 *
 * @since 2021-07-17
 */
public abstract class DecimalVec extends FixedWidthVec {
    private final int typeWidth;

    /**
     * Ihe routine will use GLOBAL memory pool when there is no specialized vector
     * allocator.
     *
     * @param size the actual number of value of vector
     * @param typeLength the length of this data type
     * @param type the data type of this vector
     */
    public DecimalVec(int size, int typeLength, DataType type) {
        super(size * typeLength, size, VecEncoding.OMNI_VEC_ENCODING_FLAT, type);
        this.typeWidth = getTypeWidth(typeLength);
    }

    /**
     * The routine will use native vector to initialize a new vector.
     *
     * @param nativeVector native vector address
     * @param typeLength the length of this data type
     * @param type the type of this vector
     */
    public DecimalVec(long nativeVector, int typeLength, DataType type) {
        super(nativeVector, type, typeLength);
        this.typeWidth = getTypeWidth(typeLength);
    }

    /**
     * The routine will use native vector to initialize a new vector.
     *
     * @param nativeVector native vector address
     * @param nativeValueBufAddress valueBuf address of native vector
     * @param nativeVectorNullBufAddress nullBuf address of native vector
     * @param capacityInBytes capacity in bytes of vector
     * @param size the actual number of value of vector
     * @param typeLength the length of this data type
     * @param type the type of this vector
     */
    public DecimalVec(long nativeVector, long nativeValueBufAddress, long nativeVectorNullBufAddress,
            int capacityInBytes, int size, int typeLength, DataType type) {
        super(nativeVector, nativeValueBufAddress, nativeVectorNullBufAddress, capacityInBytes, size, type);
        this.typeWidth = getTypeWidth(typeLength);
    }

    /**
     * The routine is just for slicing and copyRegion vector operator.
     *
     * @param vector the vector need to be sliced or copyRegion
     * @param offset When a vector has been sliced or copyRegion, this value will
     *            point to where is the new slice {@link Vec} start
     * @param length the number of value
     */
    protected DecimalVec(DecimalVec vector, int offset, int length) {
        super(vector, offset, length, length * vector.typeWidth * Long.BYTES);
        this.typeWidth = vector.typeWidth;
    }

    /**
     * The routine is just for copyPosition vector operator.
     *
     * @param vector the vector need to be copy
     * @param positions the original vector positions
     * @param offset offset of positions in the input parameter
     * @param length number of elements copied
     */
    protected DecimalVec(DecimalVec vector, int[] positions, int offset, int length) {
        super(vector, positions, offset, length, length * vector.typeWidth * Long.BYTES);
        this.typeWidth = vector.typeWidth;
    }

    private int getTypeWidth(int typeLength) {
        return typeLength / Long.BYTES;
    }

    /**
     * get the specified decimal at the specified absolute
     *
     * @param index the element offset in vec
     * @return decimal value, from high to low
     */
    public long[] get(int index) {
        long[] value = new long[this.typeWidth];
        int offset = index * this.typeWidth;
        for (int i = 0; i < this.typeWidth; i++) {
            value[i] = valuesBuf.getLong((offset + i) * Long.BYTES);
        }
        return value;
    }

    /**
     * get long values from the specified position with element number
     *
     * @param index the position of element
     * @param length the number of element
     * @return long value array
     */
    public long[] get(int index, int length) {
        long[] value = new long[this.typeWidth * length];
        int offset = index * this.typeWidth;
        valuesBuf.getLongArray(offset * Long.BYTES, value, 0, value.length * Long.BYTES);
        return value;
    }

    /**
     * Sets the specified decimal value at the specified absolute
     *
     * @param index the element offset in vec
     * @param value the value of the element to be written, from high to low
     */
    public void set(int index, long[] value) {
        int offset = index * this.typeWidth;
        for (int i = 0; i < this.typeWidth; i++) {
            valuesBuf.setLong((offset + i) * Long.BYTES, value[i]);
        }
    }

    /**
     * Batch sets the specified long at the specified absolute
     *
     * @param values the value of the element to be written
     * @param offset the element offset in vec
     * @param start the element index in values
     * @param length the number of elements that need to written
     */
    public void put(long[] values, int offset, int start, int length) {
        if (length % this.typeWidth != 0) {
            throw new OmniRuntimeException(OMNI_PARAM_ERROR, "length " + length + "is error.");
        }
        valuesBuf.setLongArray(offset * typeWidth * Long.BYTES, values, start * Long.BYTES, length * Long.BYTES);
    }

    @Override
    public int getRealValueBufCapacityInBytes() {
        return size * typeWidth * Long.BYTES;
    }

    @Override
    public int getCapacityInBytes() {
        return size * typeWidth * Long.BYTES;
    }
}
