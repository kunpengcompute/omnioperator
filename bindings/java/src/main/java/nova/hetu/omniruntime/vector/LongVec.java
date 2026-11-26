/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
 * KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 */

package nova.hetu.omniruntime.vector;

import nova.hetu.omniruntime.type.LongDataType;

import java.nio.ByteBuffer;

/**
 * long vec.
 *
 * @since 2021-07-17
 */
public class LongVec extends FixedWidthVec {
    private static final int BYTES = Long.BYTES;

    public LongVec(int size) {
        super(size * BYTES, size, VecEncoding.OMNI_VEC_ENCODING_FLAT, LongDataType.LONG);
    }

    public LongVec(long nativeVector) {
        super(nativeVector, LongDataType.LONG, BYTES);
    }

    public LongVec(long nativeVector, long nativeValueBufAddress, long nativeVectorNullBufAddress, int size) {
        super(nativeVector, nativeValueBufAddress, nativeVectorNullBufAddress, BYTES * size, size, LongDataType.LONG);
    }

    /**
     * This constructor of vector is just for shuffle compilation to pass, it will
     * be removed later.
     *
     * @param data data of vector
     * @param capacityInBytes size in bytes of data
     */
    @Deprecated
    public LongVec(ByteBuffer data, int capacityInBytes) {
        super(capacityInBytes, data.limit(), VecEncoding.OMNI_VEC_ENCODING_FLAT, LongDataType.LONG);
    }

    private LongVec(LongVec vector, int offset, int length) {
        super(vector, offset, length, length * BYTES);
    }

    private LongVec(LongVec vector, int[] positions, int offset, int length) {
        super(vector, positions, offset, length, length * BYTES);
    }

    /**
     * get the specified long at the specified absolute.
     *
     * @param index the element offset in vec
     * @return long value
     */
    public long get(int index) {
        return valuesBuf.getLong(index * BYTES);
    }

    /**
     * get long values from the specified position.
     *
     * @param index the position of element
     * @param length the number of element
     * @return long value array
     */
    public long[] get(int index, int length) {
        long[] target = new long[length];
        valuesBuf.getLongArray(index * BYTES, target, 0, length * BYTES);
        return target;
    }

    /**
     * Sets the specified long at the specified absolute.
     *
     * @param index the element offset in vec
     * @param value the value of vec
     */
    public void set(int index, long value) {
        valuesBuf.setLong(index * BYTES, value);
    }

    /**
     * Batch sets the specified long at the specified absolute.
     *
     * @param values the value of the element to be written
     * @param offset the element offset in vec
     * @param start the element index in values
     * @param length the number of elements that need to written
     */
    public void put(long[] values, int offset, int start, int length) {
        valuesBuf.setLongArray(offset * BYTES, values, start * BYTES, length * BYTES);
    }

    @Override
    public LongVec slice(int start, int length) {
        return new LongVec(this, start, length);
    }

    @Override
    public LongVec copyPositions(int[] positions, int offset, int length) {
        return new LongVec(this, positions, offset, length);
    }

    @Override
    public int getRealValueBufCapacityInBytes() {
        return size * BYTES;
    }

    @Override
    public int getCapacityInBytes() {
        return BYTES * size;
    }
}
