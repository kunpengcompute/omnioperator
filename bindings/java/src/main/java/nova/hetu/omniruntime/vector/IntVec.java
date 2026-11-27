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

import nova.hetu.omniruntime.type.IntDataType;

/**
 * int vec.
 *
 * @since 2021-07-17
 */
public class IntVec extends FixedWidthVec {
    private static final int BYTES = Integer.BYTES;

    public IntVec(int size) {
        super(size * BYTES, size, VecEncoding.OMNI_VEC_ENCODING_FLAT, IntDataType.INTEGER);
    }

    public IntVec(long nativeVector) {
        super(nativeVector, IntDataType.INTEGER, BYTES);
    }

    public IntVec(long nativeVector, long nativeValueBufAddress, long nativeVectorNullBufAddress, int size) {
        super(nativeVector, nativeValueBufAddress, nativeVectorNullBufAddress, BYTES * size, size, IntDataType.INTEGER);
    }

    private IntVec(IntVec vector, int offset, int length) {
        super(vector, offset, length, length * BYTES);
    }

    private IntVec(IntVec vector, int[] positions, int offset, int length) {
        super(vector, positions, offset, length, length * BYTES);
    }

    /**
     * get the specified integer at the specified absolute.
     *
     * @param index the element offset in vec
     * @return int value
     */
    public int get(int index) {
        return valuesBuf.getInt(index * BYTES);
    }

    /**
     * get int values from the specified position.
     *
     * @param index the position of element
     * @param length the number of element
     * @return int value array
     */
    public int[] get(int index, int length) {
        int[] target = new int[length];
        valuesBuf.getIntArray(index * BYTES, target, 0, length * BYTES);
        return target;
    }

    /**
     * Sets the specified integer at the specified absolute.
     *
     * @param index the element offset in vec
     * @param value the value of vec
     */
    public void set(int index, int value) {
        valuesBuf.setInt(index * BYTES, value);
    }

    /**
     * Batch sets the specified integer at the specified absolute.
     *
     * @param values the value of the element to be written
     * @param offset the element offset in vec
     * @param start the element index in values
     * @param length the number of elements that need to written
     */
    public void put(int[] values, int offset, int start, int length) {
        valuesBuf.setIntArray(offset * BYTES, values, start * BYTES, length * BYTES);
    }

    @Override
    public IntVec slice(int start, int length) {
        return new IntVec(this, start, length);
    }

    @Override
    public IntVec copyPositions(int[] positions, int offset, int length) {
        return new IntVec(this, positions, offset, length);
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
