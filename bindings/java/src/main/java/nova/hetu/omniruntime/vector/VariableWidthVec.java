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

import static nova.hetu.omniruntime.vector.VecEncoding.OMNI_VEC_ENCODING_FLAT;

import nova.hetu.omniruntime.type.DataType;

/**
 * base class of variable width vec.
 *
 * @since 2021-07-17
 */
public abstract class VariableWidthVec extends Vec {
    /**
     * offsets buffer.
     */
    protected OmniBuffer offsetsBuf;

    /**
     * last set index.
     */
    protected int lastOffsetPosition = -1;

    /**
     * Ihe routine will use GLOBAL memory pool when there is no specialized vector
     * allocator.
     *
     * @param capacityInBytes the number of value of vector
     * @param size the actual number of value of vector
     * @param type the data type of this vector
     */
    public VariableWidthVec(int capacityInBytes, int size, DataType type) {
        super(capacityInBytes, size, OMNI_VEC_ENCODING_FLAT, type);
        this.offsetsBuf = OmniBufferFactory.create(getValueOffsetsNative(getNativeVector()),
                (size + 1) * Integer.BYTES);
    }

    /**
     * The routine is just for slicing and copyRegion vector operator.
     *
     * @param vec the vector need to be sliced or copyRegion
     * @param offset When a vector has been sliced or copyRegion, this value will
     *            point to where is the new slice {@link Vec} start
     * @param length the number of value
     */
    protected VariableWidthVec(Vec vec, int offset, int length) {
        super(vec, offset, length, vec.getCapacityInBytes());
        this.offsetsBuf = OmniBufferFactory.create(getValueOffsetsNative(getNativeVector()),
                (length + 1) * Integer.BYTES);
    }

    /**
     * The routine is just for copyPosition vector operator.
     *
     * @param vec the vector need to be copy
     * @param positions the original vector positions
     * @param offset offset of positions in the input parameter
     * @param length number of elements copied
     */
    protected VariableWidthVec(Vec vec, int[] positions, int offset, int length) {
        super(vec, positions, offset, length, vec.getCapacityInBytes());
        this.offsetsBuf = OmniBufferFactory.create(getValueOffsetsNative(getNativeVector()),
                (length + 1) * Integer.BYTES);
        this.capacityInBytes = getCapacityInBytesNative(nativeVector);
    }

    /**
     * The routine will use native vector to initialize a new vector.
     *
     * @param nativeVector native vector address
     * @param dataType the type of this vector
     */
    protected VariableWidthVec(long nativeVector, DataType dataType) {
        super(nativeVector, dataType);
        this.offsetsBuf = OmniBufferFactory.create(getValueOffsetsNative(getNativeVector()),
                (size + 1) * Integer.BYTES);
    }

    /**
     * The routine will use native vector to initialize a new vector.
     *
     * @param nativeVector native vector address
     * @param nativeValueBufAddress valueBuf address of native vector
     * @param nativeVectorNullBufAddress nullBuf address of native vector
     * @param nativeVectorOffsetBufAddress offsetBuf address of native vector
     * @param capacityInBytes capacity in bytes of vector
     * @param size the actual number of value of vector
     * @param dataType the type of this vector
     */
    protected VariableWidthVec(long nativeVector, long nativeValueBufAddress, long nativeVectorNullBufAddress,
            long nativeVectorOffsetBufAddress, int capacityInBytes, int size, DataType dataType) {
        super(nativeVector, nativeValueBufAddress, nativeVectorNullBufAddress, capacityInBytes, size, dataType);
        this.offsetsBuf = OmniBufferFactory.create(nativeVectorOffsetBufAddress, (size + 1) * Integer.BYTES);
    }

    /**
     * get value offset buffer from native vector.
     *
     * @param nativeVector native vector address
     * @return value offset buffer
     */
    protected static native long getValueOffsetsNative(long nativeVector);

    /**
     * get the offset value of the specified position.
     *
     * @param index the element offset in vec
     * @return offset value
     */
    public int getValueOffset(int index) {
        return offsetsBuf.getInt(index * Integer.BYTES);
    }

    /**
     * set the offset value of the specified position.
     *
     * @param index the element offset in vec
     * @param offset offset value
     */
    public void setValueOffset(int index, int offset) {
        offsetsBuf.setInt(index * Integer.BYTES, offset);
    }

    /**
     * get the data length of the specified position.
     *
     * @param index the element offset in vec
     * @return data length in bytes
     */
    public int getDataLength(int index) {
        return getValueOffset(index + 1) - getValueOffset(index);
    }

    /**
     * return null value array from 0 to size + offset length.
     *
     * @return raw value of offsets
     */
    public int[] getRawValueOffset() {
        // the length of the array is size + offset, so that the caller
        // and vec can have the same offset.
        int[] rawValueOffset = new int[size + 1];
        offsetsBuf.getIntArray(0, rawValueOffset, 0, rawValueOffset.length * Integer.BYTES);
        return rawValueOffset;
    }

    /**
     * get the value offset of the specified position.
     *
     * @param index the offset of element
     * @param length the number of element
     * @return the offsets
     */
    public int[] getValueOffset(int index, int length) {
        int startOffset = getValueOffset(index);
        int[] offsets = new int[length + 1];
        for (int i = 1; i <= length; i++) {
            offsets[i] = getValueOffset(index + i) - startOffset;
        }
        return offsets;
    }

    /**
     * get the offsets of variablewidthvec.
     *
     * @return offsets byte buffer
     */
    public OmniBuffer getOffsetsBuf() {
        return offsetsBuf;
    }

    /**
     * set offsets buffer.
     *
     * @param buf buf of offset
     */
    public void setOffsetsBuf(byte[] buf) {
        offsetsBuf.setBytes(0, buf, 0, buf.length);
    }

    @Override
    public void setNull(int index) {
        super.setNull(index);
        fillSlots(index);
        setValueOffset(index + 1, getValueOffset(index));
        lastOffsetPosition = index;
    }

    /**
     * fill offset.
     *
     * @param index index of want to set
     */
    protected void fillSlots(int index) {
        for (int i = lastOffsetPosition + 1; i < index; i++) {
            setValueOffset(i + 1, getValueOffset(i));
        }
        lastOffsetPosition = index - 1;
    }

    @Override
    public int getRealValueBufCapacityInBytes() {
        return getValueOffset(size) - getValueOffset(0);
    }

    /**
     * returns the number of bytes of the offsets.
     *
     * @return length in bytes
     */
    @Override
    public int getRealOffsetBufCapacityInBytes() {
        return (size + 1) * Integer.BYTES;
    }
}
