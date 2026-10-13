/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
 */

package nova.hetu.omniruntime.vector;

import nova.hetu.omniruntime.type.ShortDataType;

/**
 * short vec.
 *
 * @since 2021-07-17
 */
public class ShortVec extends FixedWidthVec {
    private static final int BYTES = Short.BYTES;

    public ShortVec(int size) {
        super(size * BYTES, size, VecEncoding.OMNI_VEC_ENCODING_FLAT, ShortDataType.SHORT);
    }

    public ShortVec(int size, long nmmHandle) {
        super(size * BYTES, size, VecEncoding.OMNI_VEC_ENCODING_FLAT, ShortDataType.SHORT, nmmHandle);
    }

    public ShortVec(long nativeVector) {
        super(nativeVector, ShortDataType.SHORT, BYTES);
    }

    public ShortVec(long nativeVector, long nativeValueBufAddress, long nativeVectorNullBufAddress, int size) {
        super(nativeVector, nativeValueBufAddress, nativeVectorNullBufAddress, BYTES * size, size, ShortDataType.SHORT);
    }

    private ShortVec(ShortVec vector, int offset, int length) {
        super(vector, offset, length, length * BYTES);
    }

    private ShortVec(ShortVec vector, int[] positions, int offset, int length) {
        super(vector, positions, offset, length, length * BYTES);
    }

    /**
     * get the specified short at the specified absolute.
     *
     * @param index the element offset in vec
     * @return short value
     */
    public short get(int index) {
        return valuesBuf.getShort(index * BYTES);
    }

    /**
     * get short values from the specified position.
     *
     * @param index the position of element
     * @param length the number of element
     * @return short value array
     */
    public short[] get(int index, int length) {
        short[] target = new short[length];
        valuesBuf.getShortArray(index * BYTES, target, 0, length * BYTES);
        return target;
    }

    /**
     * Sets the specified short at the specified absolute.
     *
     * @param index the element offset in vec
     * @param value the value of vec
     */
    public void set(int index, short value) {
        valuesBuf.setShort(index * BYTES, value);
    }

    /**
     * Batch sets the specified short at the specified absolute.
     *
     * @param values the value of the element to be written
     * @param offset the element offset in vec
     * @param start the element index in values
     * @param length the number of elements that need to written
     */
    public void put(short[] values, int offset, int start, int length) {
        valuesBuf.setShortArray(offset * BYTES, values, start * BYTES, length * BYTES);
    }

    @Override
    public ShortVec slice(int start, int length) {
        return new ShortVec(this, start, length);
    }

    @Override
    public ShortVec copyPositions(int[] positions, int offset, int length) {
        return new ShortVec(this, positions, offset, length);
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
