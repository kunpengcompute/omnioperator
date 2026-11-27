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

import nova.hetu.omniruntime.type.VarcharDataType;
import nova.hetu.omniruntime.utils.NullsBufHelper;

/**
 * varchar vec.
 *
 * @since 2021-07-17
 */
public class VarcharVec extends VariableWidthVec {
    /**
     * The default capacity in bytes.
     */
    public static final int INIT_CAPACITY_IN_BYTES = 32 * 1024; // 32K
    private static final int EXPAND_FACTOR = 2;

    /**
     * Warning: If this constructor is called, consider the capacity expansion scenario and update the vector info.
     *
     * @param size row count
     */
    public VarcharVec(int size) {
        super(4 * 1024, size, VarcharDataType.VARCHAR);
    }

    public VarcharVec(int capacityInBytes, int size) {
        super(capacityInBytes, size, VarcharDataType.VARCHAR);
    }

    public VarcharVec(long nativeVector) {
        super(nativeVector, VarcharDataType.VARCHAR);
    }

    public VarcharVec(long nativeVector, long nativeValueBufAddress, long nativeVectorNullBufAddress,
            long nativeVectorOffsetBufAddress, int size) {
        super(nativeVector, nativeValueBufAddress, nativeVectorNullBufAddress, nativeVectorOffsetBufAddress,
                getCapacityInBytesNative(nativeVector), size, VarcharDataType.VARCHAR);
    }

    private VarcharVec(VarcharVec vector, int offset, int length) {
        super(vector, offset, length);
    }

    private VarcharVec(VarcharVec vector, int[] positions, int offset, int length) {
        super(vector, positions, offset, length);
    }

    /**
     * get the specified bytes at the specified absolute.
     *
     * @param index the element offset in vec
     * @return byte array
     */
    public byte[] get(int index) {
        final int startOffset = getValueOffset(index);
        final int dataLen = getValueOffset(index + 1) - startOffset;
        return getData(startOffset, dataLen);
    }

    /**
     * Batch gets the specified bytes at the specified absolute.
     *
     * @param index the element offset in vec
     * @param length the number of element
     * @return byte array
     */
    public byte[] get(int index, int length) {
        final int startOffset = getValueOffset(index);
        final int dataLen = getValueOffset(index + length) - startOffset;
        return getData(startOffset, dataLen);
    }

    /**
     * according to the specified offset and length, read data from the buffer.
     *
     * @param offsetInBytes offset bytes in buffer
     * @param length the length of the data to be read
     * @return byte array
     */
    public byte[] getData(int offsetInBytes, int length) {
        return valuesBuf.getBytes(offsetInBytes, length);
    }

    /**
     * Sets the specified bytes at the specified absolute.
     *
     * @param index the element offset in vec
     * @param value byte array
     */
    public void set(int index, byte[] value) {
        fillSlots(index);
        final int startOffset = getValueOffset(index);
        setValueOffset(index + 1, startOffset + value.length);
        setData(startOffset, value, 0, value.length);
        lastOffsetPosition = index;
    }

    private void checkCapacity(int needCapacityInBytes) {
        if (needCapacityInBytes < 0) {
            return;
        }
        int toCapacityInBytes = (capacityInBytes > 0) ? capacityInBytes : INIT_CAPACITY_IN_BYTES;
        // the capacity is doubled for each calculation
        while (toCapacityInBytes < needCapacityInBytes) {
            toCapacityInBytes *= EXPAND_FACTOR;
        }
        if (toCapacityInBytes != capacityInBytes) {
            // expand Data Capacity
            long newValuesAddress = expandDataCapacity(getNativeVector(), toCapacityInBytes);
            capacityInBytes = toCapacityInBytes;
            // update data address
            valuesBuf = OmniBufferFactory.create(newValuesAddress, capacityInBytes);
        }
    }

    /**
     * Batch sets the specified bytes at the specified absolute.
     *
     * @param index the value of the element to be written
     * @param values the bytes array
     * @param offsetInArray the element offset in bytes array
     * @param offsets offsets array
     * @param offsetsIndex the offset of the offsets array
     * @param length the number of elements
     */
    public void put(int index, byte[] values, int offsetInArray, int[] offsets, int offsetsIndex, int length) {
        if (values == null || length == 0) {
            return;
        }
        fillSlots(index);
        int startOffset = getValueOffset(index);
        int[] newOffsets = compactOffsets(startOffset, offsets, offsetsIndex, length);
        int dataLength = offsets[offsetsIndex + length] - offsets[offsetsIndex];
        // set offsets
        offsetsBuf.setIntArray((index + 1) * Integer.BYTES, newOffsets, Integer.BYTES,
                (newOffsets.length - 1) * Integer.BYTES);
        // set data
        setData(startOffset, values, offsetInArray + offsets[offsetsIndex], dataLength);
        lastOffsetPosition = index + length - 1;
    }

    private int[] compactOffsets(int startOffset, int[] srcOffsets, int offsetIndex, int length) {
        int[] newOffsets = new int[length + 1];
        for (int i = 1; i <= length; i++) {
            newOffsets[i] = srcOffsets[offsetIndex + i] - srcOffsets[offsetIndex] + startOffset;
        }
        return newOffsets;
    }

    private void setData(int offsetInBytes, byte[] data, int start, int length) {
        checkCapacity(offsetInBytes + length);
        valuesBuf.setBytes(offsetInBytes, data, start, length);
    }

    @Override
    public VarcharVec slice(int start, int length) {
        return new VarcharVec(this, start, length);
    }

    @Override
    public VarcharVec copyPositions(int[] positions, int offset, int length) {
        return new VarcharVec(this, positions, offset, length);
    }

    @Override
    public void append(Vec other, int offset, int length) {
        super.append(other, offset, length);
        int newCapacityInBytes = getCapacityInBytesNative(nativeVector);
        // check expand, update initial value if expansion happened.
        if (newCapacityInBytes != capacityInBytes) {
            capacityInBytes = newCapacityInBytes;
            size = getSizeNative(nativeVector);
            nullsBuf = OmniBufferFactory.create(getValueNullsNative(nativeVector), NullsBufHelper.nBytes(size));
            valuesBuf = OmniBufferFactory.create(getValuesNative(nativeVector), capacityInBytes);
            offsetsBuf = OmniBufferFactory.create(getValueOffsetsNative(nativeVector), (size + 1) * Integer.BYTES);
        }
    }

    /**
     * set offsets buffer.
     *
     * @param offsets offset of buf
     * @param length the number of element
     */
    public void setOffsetBuf(int[] offsets, int length) {
        offsetsBuf.setIntArray(Integer.BYTES, offsets, Integer.BYTES, length * Integer.BYTES);
        lastOffsetPosition = length - 1;
    }

    private static native long expandDataCapacity(long nativeVector, int toCapacityInBytes);
}
