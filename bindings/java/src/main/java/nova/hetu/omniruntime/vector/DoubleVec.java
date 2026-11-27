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

import nova.hetu.omniruntime.type.DoubleDataType;

/**
 * double vec.
 *
 * @since 2021-07-17
 */
public class DoubleVec extends FixedWidthVec {
    private static final int BYTES = Double.BYTES;

    public DoubleVec(int size) {
        super(size * BYTES, size, VecEncoding.OMNI_VEC_ENCODING_FLAT, DoubleDataType.DOUBLE);
    }

    public DoubleVec(long nativeVector) {
        super(nativeVector, DoubleDataType.DOUBLE, BYTES);
    }

    public DoubleVec(long nativeVector, long nativeValueBufAddress, long nativeVectorNullBufAddress, int size) {
        super(nativeVector, nativeValueBufAddress, nativeVectorNullBufAddress, BYTES * size, size,
                DoubleDataType.DOUBLE);
    }

    private DoubleVec(DoubleVec vector, int offset, int length) {
        super(vector, offset, length, length * BYTES);
    }

    private DoubleVec(DoubleVec vector, int[] positions, int offset, int length) {
        super(vector, positions, offset, length, length * BYTES);
    }

    /**
     * get the specified double at the specified absolute.
     *
     * @param index the element offset in vec
     * @return double value
     */
    public double get(int index) {
        return valuesBuf.getDouble(index * BYTES);
    }

    /**
     * get double values from the specified position.
     *
     * @param index the position of element
     * @param length the number of element
     * @return double value array
     */
    public double[] get(int index, int length) {
        double[] target = new double[length];
        valuesBuf.getDoubleArray(index * BYTES, target, 0, length * BYTES);
        return target;
    }

    /**
     * Sets the specified double at the specified absolute.
     *
     * @param index the element offset in vec
     * @param value the value of vec
     */
    public void set(int index, double value) {
        valuesBuf.setDouble(index * BYTES, value);
    }

    /**
     * Batch sets the specified double at the specified absolute.
     *
     * @param values the value of the element to be written
     * @param offset the element offset in vec
     * @param start the element index in values
     * @param length the number of elements that need to written
     */
    public void put(double[] values, int offset, int start, int length) {
        valuesBuf.setDoubleArray(offset * BYTES, values, start * BYTES, length * BYTES);
    }

    @Override
    public DoubleVec slice(int start, int length) {
        return new DoubleVec(this, start, length);
    }

    @Override
    public DoubleVec copyPositions(int[] positions, int offset, int length) {
        return new DoubleVec(this, positions, offset, length);
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
