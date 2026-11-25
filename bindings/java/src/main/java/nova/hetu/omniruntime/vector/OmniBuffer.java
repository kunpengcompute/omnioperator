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

/**
 * encapsulate the ByteBuffer data interface and provide them to all vec.
 *
 * @since 2021-08-02
 */
public interface OmniBuffer {
    /**
     * set byte.
     *
     * @param index the byte offset of the element
     * @param value the value of the element
     */
    void setByte(int index, byte value);

    /**
     * get byte.
     *
     * @param index the byte offset of the element
     * @return the value of element
     */
    byte getByte(int index);

    /**
     * Batch setting bytes.
     *
     * @param index the byte offset of the element
     * @param src byte array
     * @param srcStart array start index
     * @param length byte size
     */
    void setBytes(int index, byte[] src, int srcStart, int length);

    /**
     * get bytes in batch.
     *
     * @param index the byte offset of the element
     * @param length byte size
     * @return byte array
     */
    byte[] getBytes(int index, int length);

    /**
     * get bytes in batch.
     *
     * @param index the byte offset of the element
     * @param target target byte array
     * @param targetIndex the offset of the target byte array
     * @param length byte size
     */
    void getBytes(int index, byte[] target, int targetIndex, int length);

    /**
     * set short value.
     *
     * @param index the byte offset of the element
     * @param value value of short
     */
    void setShort(int index, short value);

    /**
     * set short array.
     *
     * @param index the byte offset of the element
     * @param src short array
     * @param srcIndex the starting byte offset of the array
     * @param length byte size
     */
    void setShortArray(int index, short[] src, int srcIndex, int length);

    /**
     * get short value.
     *
     * @param index the byte offset of the element
     * @return short value
     */
    short getShort(int index);

    /**
     * get short array.
     *
     * @param index the byte offset of the element
     * @param target target short array
     * @param targetIndex the starting byte offset of the array
     * @param length byte size
     */
    void getShortArray(int index, short[] target, int targetIndex, int length);

    /**
     * set int value.
     *
     * @param index the byte offset of the element
     * @param value int value
     */
    void setInt(int index, int value);

    /**
     * get int value.
     *
     * @param index the byte offset of the element
     * @return int value
     */
    int getInt(int index);

    /**
     * set int array.
     *
     * @param index the byte offset of the element
     * @param src int array
     * @param srcIndex the starting byte offset of the array
     * @param length byte size
     */
    void setIntArray(int index, int[] src, int srcIndex, int length);

    /**
     * get int array.
     *
     * @param index the byte offset of the element
     * @param target target int array
     * @param targetIndex the starting byte offset of the array
     * @param length byte size
     */
    void getIntArray(int index, int[] target, int targetIndex, int length);

    /**
     * set long value.
     *
     * @param index the byte offset of the element
     * @param value long value
     */
    void setLong(int index, long value);

    /**
     * get long value.
     *
     * @param index the byte offset of the element
     * @return long value
     */
    long getLong(int index);

    /**
     * set long array.
     *
     * @param index the byte offset of the element
     * @param src long array
     * @param srcIndex the starting byte offset of the array
     * @param length byte size
     */
    void setLongArray(int index, long[] src, int srcIndex, int length);

    /**
     * get long array.
     *
     * @param index the byte offset of the element
     * @param target target long array
     * @param targetIndex the starting byte offset of the array
     * @param length byte size
     */
    void getLongArray(int index, long[] target, int targetIndex, int length);

    /**
     * set double value.
     *
     * @param index the byte offset of the element
     * @param value double value
     */
    void setDouble(int index, double value);

    /**
     * get double value.
     *
     * @param index the byte offset of the element
     * @return double value
     */
    double getDouble(int index);

    /**
     * set double array.
     *
     * @param index the byte offset of the element
     * @param src source double array
     * @param srcIndex the starting byte offset of the array
     * @param length byte size
     */
    void setDoubleArray(int index, double[] src, int srcIndex, int length);

    /**
     * get double array.
     *
     * @param index the byte offset of the element
     * @param target target double array
     * @param targetIndex the starting byte offset of the array
     * @param length byte size
     */
    void getDoubleArray(int index, double[] target, int targetIndex, int length);

    /**
     * get data capacity from omnibuf.
     *
     * @return capacity of omnibuf
     */
    int getCapacity();

    /**
     * get data address from omnibuf.
     *
     * @return data address of omnibuf
     */
    long getAddress();
}
