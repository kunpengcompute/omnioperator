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

import nova.hetu.omniruntime.type.BooleanDataType;

/**
 * boolean vec.
 *
 * @since 2021-07-17
 */
public class BooleanVec extends FixedWidthVec {
    private static final int BYTES = 1;

    public BooleanVec(int size) {
        super(size * BYTES, size, VecEncoding.OMNI_VEC_ENCODING_FLAT, BooleanDataType.BOOLEAN);
    }

    public BooleanVec(long nativeVector) {
        super(nativeVector, BooleanDataType.BOOLEAN, BYTES);
    }

    public BooleanVec(long nativeVector, long nativeValueBufAddress, long nativeVectorNullBufAddress, int size) {
        super(nativeVector, nativeValueBufAddress, nativeVectorNullBufAddress, BYTES * size, size,
                BooleanDataType.BOOLEAN);
    }

    private BooleanVec(BooleanVec vector, int offset, int length) {
        super(vector, offset, length, length * BYTES);
    }

    private BooleanVec(BooleanVec vector, int[] positions, int offset, int length) {
        super(vector, positions, offset, length, length * BYTES);
    }

    /**
     * Sets the specified boolean at the specified absolute.
     *
     * @param index the element offset in vec
     * @param value the value of vec
     */
    public void set(int index, boolean value) {
        valuesBuf.setByte(index, value ? (byte) 1 : (byte) 0);
    }

    /**
     * get the specified boolean at the specified absolute.
     *
     * @param index the element offset in vec
     * @return if the value of 1 returns true, otherwise it returns false
     */
    public boolean get(int index) {
        return valuesBuf.getByte(index * BYTES) == 1;
    }

    /**
     * get boolean values from the specified position.
     *
     * @param index the position of element
     * @param length the number of element
     * @return boolean value array
     */
    public boolean[] get(int index, int length) {
        byte[] target = valuesBuf.getBytes(index * BYTES, length);
        return transformByteToBoolean(target, 0, length);
    }

    /**
     * Batch sets the specified boolean at the specified absolute.
     *
     * @param values the value of the element to be written
     * @param index the element index in vec
     * @param start the element index in values
     * @param length the number of elements that need to written
     */
    public void put(boolean[] values, int index, int start, int length) {
        byte[] data = transformBooleanToByte(values, start, length);
        valuesBuf.setBytes(index, data, 0, length);
    }

    /**
     * Batch sets the specified byte at the specified absolute.
     *
     * @param values the value of the element to be written
     * @param index the element index in vec
     * @param start the element index in values
     * @param length the number of elements that need to written
     */
    public void put(byte[] values, int index, int start, int length) {
        valuesBuf.setBytes(index * BYTES, values, start * BYTES, length * BYTES);
    }

    @Override
    public BooleanVec slice(int startIdx, int length) {
        return new BooleanVec(this, startIdx, length);
    }

    @Override
    public BooleanVec copyPositions(int[] positions, int offset, int length) {
        return new BooleanVec(this, positions, offset, length);
    }

    @Override
    public int getRealValueBufCapacityInBytes() {
        return size * BYTES;
    }

    @Override
    public int getCapacityInBytes() {
        return size * BYTES;
    }
}
