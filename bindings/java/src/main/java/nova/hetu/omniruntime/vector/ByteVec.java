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

import nova.hetu.omniruntime.type.ByteDataType;

/**
 * byte vec.
 *
 * @since 2025-08-05
 */
public class ByteVec extends FixedWidthVec {
    private static final int BYTES = Byte.BYTES;

    public ByteVec(int size) {
        super(size * BYTES, size, VecEncoding.OMNI_VEC_ENCODING_FLAT, ByteDataType.BYTE);
    }

    public ByteVec(long nativeVector) {
        super(nativeVector, ByteDataType.BYTE, BYTES);
    }

    public ByteVec(long nativeVector, long nativeValueBufAddress, long nativeVectorNullBufAddress, int size) {
        super(nativeVector, nativeValueBufAddress, nativeVectorNullBufAddress, BYTES * size, size, ByteDataType.BYTE);
    }

    private ByteVec(ByteVec vector, int offset, int length) {
        super(vector, offset, length, length * BYTES);
    }

    private ByteVec(ByteVec vector, int[] positions, int offset, int length) {
        super(vector, positions, offset, length, length * BYTES);
    }

    /**
     * get the specified byte at the specified absolute.
     *
     * @param index the element offset in vec
     * @return byte value
     */
    public byte get(int index) {
        return valuesBuf.getByte(index * BYTES);
    }

    /**
     * get byte values from the specified position.
     *
     * @param index the position of element
     * @param length the number of element
     * @return byte value array
     */
    public byte[] get(int index, int length) {
        byte[] target = new byte[length];
        valuesBuf.getBytes(index * BYTES, target, 0, length * BYTES);
        return target;
    }

    /**
     * Sets the specified byte at the specified absolute.
     *
     * @param index the element offset in vec
     * @param value the value of vec
     */
    public void set(int index, byte value) {
        valuesBuf.setByte(index * BYTES, value);
    }

    /**
     * Batch sets the specified byte at the specified absolute.
     *
     * @param values the value of the element to be written
     * @param offset the element offset in vec
     * @param start the element index in values
     * @param length the number of elements that need to written
     */
    public void put(byte[] values, int offset, int start, int length) {
        valuesBuf.setBytes(offset * BYTES, values, start * BYTES, length * BYTES);
    }

    @Override
    public ByteVec slice(int start, int length) {
        return new ByteVec(this, start, length);
    }

    @Override
    public ByteVec copyPositions(int[] positions, int offset, int length) {
        return new ByteVec(this, positions, offset, length);
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
