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

import static nova.hetu.omniruntime.vector.VecEncoding.OMNI_ENCODING_ARRAY;

import nova.hetu.omniruntime.type.DataType;
import nova.hetu.omniruntime.type.ArrayDataType;
import nova.hetu.omniruntime.utils.NullsBufHelper;

/**
 * array vec.
 *
 * @since 2025-09-01
 */
public class ArrayVec extends ComplexVec {
    private Vec elementVec;

    /**
     * offsets buffer.
     */
    protected OmniBuffer offsetsBuf;

    public ArrayVec(ArrayDataType type, int size) {
        this(type, size, false);
    }

    public ArrayVec(ArrayDataType type, int size, boolean isEmpty) {
        this(isEmpty ? newEmptyComplexVectorNative(size, OMNI_ENCODING_ARRAY.ordinal(), new DataType[]{type.getElementType()})
                : newComplexVectorNative(size, OMNI_ENCODING_ARRAY.ordinal(), new DataType[]{type.getElementType()}), type, size, isEmpty);
    }

    public ArrayVec(long nativeVector, ArrayDataType type) {
        this(nativeVector, type, getSizeNative(nativeVector), false);
    }

    public ArrayVec(long nativeVector, ArrayDataType type, int size) {
        this(nativeVector, type, size, false);
    }

    public ArrayVec(long nativeVector, long nativeValueBufAddress, long nativeVectorNullBufAddress, int size, ArrayDataType type) {
        super(nativeVector, nativeValueBufAddress, nativeVectorNullBufAddress,
            getComplexCapacityNative(nativeVector, OMNI_ENCODING_ARRAY.ordinal()), size, type);
        this.elementVec = createVec(getElementsAddrNative(nativeVector), type.getElementType());
        this.offsetsBuf = OmniBufferFactory.create(getValueOffsetsNative(getNativeVector()),
            (size + 1) * Long.BYTES);
    }

    public ArrayVec(long nativeVector, long nativeValueBufAddress, long nativeVectorNullBufAddress,
        long nativeVectorOffsetBufAddress, int size, ArrayDataType type) {
            super(nativeVector, nativeValueBufAddress, nativeVectorNullBufAddress,
                getComplexCapacityNative(nativeVector, OMNI_ENCODING_ARRAY.ordinal()), size, type);
            this.elementVec = createVec(getElementsAddrNative(nativeVector), type.getElementType());
            this.offsetsBuf = OmniBufferFactory.create(nativeVectorOffsetBufAddress, (size + 1) * Long.BYTES);
        }

    public ArrayVec(long nativeVector, ArrayDataType type, int size, boolean isEmpty) {
        super(nativeVector, getComplexCapacityNative(nativeVector, OMNI_ENCODING_ARRAY.ordinal()), size, type);
        if (!isEmpty){
            this.elementVec = createVec(getElementsAddrNative(nativeVector), type.getElementType());
        }
        this.offsetsBuf = OmniBufferFactory.create(getValueOffsetsNative(getNativeVector()),
            (size + 1) * Long.BYTES);
    }

    private ArrayVec(ArrayVec vector, int offset, int length) {
        super(vector, offset, length, getComplexCapacityNative(vector.getNativeVector(), OMNI_ENCODING_ARRAY.ordinal()));
        this.elementVec = createVec(getElementsAddrNative(nativeVector), ((ArrayDataType) getType()).getElementType());
        this.offsetsBuf = OmniBufferFactory.create(getValueOffsetsNative(getNativeVector()),
            (length + 1) * Long.BYTES);
    }

    @Override
    public ArrayVec slice(int start, int length) {
        return new ArrayVec(this, start, length);
    }

    @Override
    public VecEncoding getEncoding() {
        return VecEncoding.OMNI_ENCODING_ARRAY;
    }

    @Override
    public Vec copyPositions(int[] positions, int offset, int length) {
        return null;
    }

    @Override
    public int getRealValueBufCapacityInBytes() {
        return 0;
    }

    @Override
    public int getRealOffsetBufCapacityInBytes() {
        return (size + 1) * Long.BYTES;
    }

    @Override
    public void append(Vec other, int offset, int length) {
        super.append(other, offset, length);

        int newSize = getSizeNative(nativeVector);
        if (newSize != size) {
            size = newSize;
            long newOffsetsAddress = getValueOffsetsNative(nativeVector);
            long newNullsAddress = getValueNullsNative(nativeVector);
            long newElementsAddress = getElementsAddrNative(nativeVector);
            // update offset buffer
            offsetsBuf = OmniBufferFactory.create(newOffsetsAddress, (size + 1) * Long.BYTES);
            // update null buffer
            nullsBuf = OmniBufferFactory.create(newNullsAddress, NullsBufHelper.nBytes(size));
            // update element vector
            ArrayDataType arrayType = (ArrayDataType) getType();
            elementVec = createVec(newElementsAddress, arrayType.getElementType());
        }
    }

    public OmniBuffer getOffsetsBuf() {
        return offsetsBuf;
    }

    public void setOffsetsBuf(byte[] buf) {
        offsetsBuf.setBytes(0, buf, 0, buf.length);
    }

    public long getOffset(long rowId) {
        return getOffsetNative(nativeVector, rowId);
    }

    public long getSize(long rowId) {
        return getSizeNative(nativeVector, rowId);
    }

    public Vec getElementVec() {
        return elementVec;
    }

    public void addElements(Vec elements) {
        this.elementVec = elements;
        addElementsNative(this.nativeVector, elements.nativeVector);
    }

    public void addOffsets(int[] offsets) {
        addOffsetsNative(this.nativeVector, offsets);
    }

    public void setSize(int index, int size) {
        setSizeByIndexNative(this.nativeVector, index, size);
    }

    /**
     * get value offset buffer from native vector.
     *
     * @param nativeVector native vector address
     * @return value offset buffer
     */
    protected static native long getValueOffsetsNative(long nativeVector);

    protected static native long setSizeByIndexNative(long nativeVector, int index, int size);

    protected static native long getElementsAddrNative(long nativeVector);

    protected static native long getOffsetNative(long nativeVector, long rowId);

    protected static native long getSizeNative(long nativeVector, long rowId);

    protected static native void addElementsNative(long nativeVector, long elements);

    protected static native void addOffsetsNative(long nativeVector, int[] offsets);
}
