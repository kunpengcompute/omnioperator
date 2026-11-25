/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2024. All rights reserved.
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

package nova.hetu.omniruntime.vector;

import sun.misc.Unsafe;

/**
 * jdk8 unsafe interface implementation.
 *
 * @since 2021-08-10
 */
public class OmniBufferUnsafeV8 implements OmniBuffer {
    private final long address;

    private final int capacity;

    public OmniBufferUnsafeV8(long address, int capacity) {
        this.address = address;
        this.capacity = capacity;
    }

    @Override
    public void setByte(int index, byte value) {
        JvmUtils.UNSAFE.putByte(addr(index), value);
    }

    @Override
    public byte getByte(int index) {
        return JvmUtils.UNSAFE.getByte(addr(index));
    }

    @Override
    public void setBytes(int index, byte[] src, int srcStart, int length) {
        JvmUtils.UNSAFE.copyMemory(src, Unsafe.ARRAY_BYTE_BASE_OFFSET + srcStart, null, addr(index), length);
    }

    @Override
    public byte[] getBytes(int index, int length) {
        byte[] target = new byte[length];
        getBytes(index, target, 0, length);
        return target;
    }

    @Override
    public void getBytes(int index, byte[] target, int targetIndex, int length) {
        JvmUtils.UNSAFE.copyMemory(null, addr(index), target, Unsafe.ARRAY_BYTE_BASE_OFFSET + targetIndex, length);
    }

    @Override
    public void setShort(int index, short value) {
        JvmUtils.UNSAFE.putShort(addr(index), value);
    }

    @Override
    public void setShortArray(int index, short[] src, int srcIndex, int length) {
        JvmUtils.UNSAFE.copyMemory(src, Unsafe.ARRAY_LONG_BASE_OFFSET + srcIndex, null, addr(index), length);
    }

    @Override
    public short getShort(int index) {
        return JvmUtils.UNSAFE.getShort(addr(index));
    }

    @Override
    public void getShortArray(int index, short[] target, int targetIndex, int length) {
        JvmUtils.UNSAFE.copyMemory(null, addr(index), target, Unsafe.ARRAY_SHORT_BASE_OFFSET + targetIndex, length);
    }

    @Override
    public void setInt(int index, int value) {
        JvmUtils.UNSAFE.putInt(addr(index), value);
    }

    @Override
    public int getInt(int index) {
        return JvmUtils.UNSAFE.getInt(addr(index));
    }

    @Override
    public void setIntArray(int index, int[] src, int srcIndex, int length) {
        JvmUtils.UNSAFE.copyMemory(src, Unsafe.ARRAY_INT_BASE_OFFSET + srcIndex, null, addr(index), length);
    }

    @Override
    public void getIntArray(int index, int[] target, int targetIndex, int length) {
        JvmUtils.UNSAFE.copyMemory(null, addr((long) index), target, Unsafe.ARRAY_INT_BASE_OFFSET + targetIndex,
                length);
    }

    @Override
    public void setLong(int index, long value) {
        JvmUtils.UNSAFE.putLong(addr(index), value);
    }

    @Override
    public long getLong(int index) {
        return JvmUtils.UNSAFE.getLong(addr(index));
    }

    @Override
    public void setLongArray(int index, long[] src, int srcIndex, int length) {
        JvmUtils.UNSAFE.copyMemory(src, Unsafe.ARRAY_LONG_BASE_OFFSET + srcIndex, null, addr(index), length);
    }

    @Override
    public void getLongArray(int index, long[] target, int targetIndex, int length) {
        JvmUtils.UNSAFE.copyMemory(null, addr(index), target, Unsafe.ARRAY_LONG_BASE_OFFSET + targetIndex, length);
    }

    @Override
    public void setDouble(int index, double value) {
        JvmUtils.UNSAFE.putDouble(addr(index), value);
    }

    @Override
    public double getDouble(int index) {
        return JvmUtils.UNSAFE.getDouble(addr(index));
    }

    @Override
    public void setDoubleArray(int index, double[] src, int srcIndex, int length) {
        JvmUtils.UNSAFE.copyMemory(src, Unsafe.ARRAY_DOUBLE_BASE_OFFSET + srcIndex, null, addr(index), length);
    }

    @Override
    public void getDoubleArray(int index, double[] target, int targetIndex, int length) {
        JvmUtils.UNSAFE.copyMemory(null, addr(index), target, Unsafe.ARRAY_LONG_BASE_OFFSET + targetIndex, length);
    }

    @Override
    public int getCapacity() {
        return capacity;
    }

    @Override
    public long getAddress() {
        return address;
    }

    private long addr(long offsetInBytes) {
        return address + offsetInBytes;
    }
}
