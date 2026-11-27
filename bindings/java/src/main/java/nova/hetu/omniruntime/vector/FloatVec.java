/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
 */

package nova.hetu.omniruntime.vector;

import nova.hetu.omniruntime.type.FloatDataType;

/**
 * float vec.
 *
 * @since 2021-07-17
 */
public class FloatVec extends FixedWidthVec {
    private static final int BYTES = Float.BYTES;

    public FloatVec(int size) {
        super(size * BYTES, size, VecEncoding.OMNI_VEC_ENCODING_FLAT, FloatDataType.FLOAT);
    }

    public FloatVec(int size, long nmmHandle) {
        super(size * BYTES, size, VecEncoding.OMNI_VEC_ENCODING_FLAT, FloatDataType.FLOAT, nmmHandle);
    }

    public FloatVec(long nativeVector) {
        super(nativeVector, FloatDataType.FLOAT, BYTES);
    }

    public FloatVec(long nativeVector, long nativeValueBufAddress, long nativeVectorNullBufAddress, int size) {
        super(nativeVector, nativeValueBufAddress, nativeVectorNullBufAddress, BYTES * size, size,
                FloatDataType.FLOAT);
    }

    private FloatVec(FloatVec vector, int offset, int length) {
        super(vector, offset, length, length * BYTES);
    }

    private FloatVec(FloatVec vector, int[] positions, int offset, int length) {
        super(vector, positions, offset, length, length * BYTES);
    }

    /**
     * get the specified float at the specified absolute.
     *
     * @param index the element offset in vec
     * @return float value
     */
    public float get(int index) {
        return valuesBuf.getFloat(index * BYTES);
    }

    /**
     * get float values from the specified position.
     *
     * @param index the position of element
     * @param length the number of element
     * @return float value array
     */
    public float[] get(int index, int length) {
        float[] target = new float[length];
        valuesBuf.getFloatArray(index * BYTES, target, 0, length * BYTES);
        return target;
    }

    /**
     * Sets the specified float at the specified absolute.
     *
     * @param index the element offset in vec
     * @param value the value of vec
     */
    public void set(int index, float value) {
        valuesBuf.setFloat(index * BYTES, value);
    }

    /**
     * Batch sets the specified float at the specified absolute.
     *
     * @param values the value of the element to be written
     * @param offset the element offset in vec
     * @param start the element index in values
     * @param length the number of elements that need to written
     */
    public void put(float[] values, int offset, int start, int length) {
        valuesBuf.setFloatArray(offset * BYTES, values, start * BYTES, length * BYTES);
    }

    @Override
    public FloatVec slice(int start, int length) {
        return new FloatVec(this, start, length);
    }

    @Override
    public FloatVec copyPositions(int[] positions, int offset, int length) {
        return new FloatVec(this, positions, offset, length);
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
