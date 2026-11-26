/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
 * KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 */

package nova.hetu.omniruntime.vector;

import nova.hetu.omniruntime.type.Decimal128DataType;

import java.math.BigInteger;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * 128-bit decimal vec.
 *
 * @since 2021-07-17
 */
public class Decimal128Vec extends DecimalVec {
    private static final int BYTES = Long.BYTES * 2;

    public Decimal128Vec(int size) {
        super(size, BYTES, Decimal128DataType.DECIMAL128);
    }

    public Decimal128Vec(long nativeVector) {
        super(nativeVector, BYTES, Decimal128DataType.DECIMAL128);
    }

    public Decimal128Vec(long nativeVector, long nativeValueBufAddress, long nativeVectorNullBufAddress, int size) {
        super(nativeVector, nativeValueBufAddress, nativeVectorNullBufAddress, size * BYTES, size, BYTES,
                Decimal128DataType.DECIMAL128);
    }

    private Decimal128Vec(Decimal128Vec vector, int offset, int length) {
        super(vector, offset, length);
    }

    private Decimal128Vec(Decimal128Vec vector, int[] positions, int offset, int length) {
        super(vector, positions, offset, length);
    }

    /**
     * split a vec into two vec according to the specified index and length.
     *
     * @param start starting index
     * @param length slice length
     * @return new vec
     */
    @Override
    public Decimal128Vec slice(int start, int length) {
        return new Decimal128Vec(this, start, length);
    }

    /**
     * copy a new vec based on the positions.
     *
     * @param positions all positions in vec
     * @param offset position offset
     * @param length the number of elements to be copied
     * @return new vec
     */
    @Override
    public Decimal128Vec copyPositions(int[] positions, int offset, int length) {
        return new Decimal128Vec(this, positions, offset, length);
    }

    /**
     * transfer long to bytes
     *
     * @param input input long
     * @return new bytes
     */
    public static byte[] longToBytes(long input) {
        ByteBuffer buffer = ByteBuffer.allocate(Long.BYTES);
        buffer.putLong(input);
        return buffer.array();
    }

    /**
     * transfer bytes to long
     *
     * @param bytes input bytes
     * @return new long
     */
    public static long bytesToLong(byte[] bytes) {
        ByteBuffer buffer = ByteBuffer.allocate(Long.BYTES);
        buffer.put(bytes);
        buffer.flip(); // need flip
        return buffer.getLong();
    }

    /**
     * get long array from 128-bit BigInteger
     *
     * @param bigInteger input
     * @return new long array
     */
    public static long[] putDecimal(BigInteger bigInteger) {
        return putDecimal(bigInteger.toByteArray(), bigInteger.compareTo(BigInteger.ZERO) == -1);
    }

    /**
     * get long array from 128-bit BigInteger bytes
     *
     * @param bytes BigInteger bytes
     * @param isNegative isNegative
     * @return new long array
     */
    public static long[] putDecimal(byte[] bytes, boolean isNegative) {
        ByteBuffer d128Buffer = ByteBuffer.allocate(Long.BYTES * 2);
        int byteArrayLength = bytes.length;
        for (int i = 0; i < byteArrayLength; i++) {
            d128Buffer.put(bytes[byteArrayLength - i - 1]);
        }
        if (isNegative) {
            for (int i = byteArrayLength; i < 2 * Long.BYTES; i++) {
                d128Buffer.put((byte) -1);
            }
        }
        d128Buffer.clear();
        d128Buffer.order(ByteOrder.LITTLE_ENDIAN);
        long[] result = new long[2];
        result[0] = d128Buffer.getLong();
        result[1] = d128Buffer.getLong();
        return result;
    }

    /**
     * get 128-bit BigInteger from long array
     *
     * @param longs input
     * @return new BigInteger
     */
    public static BigInteger getDecimal(long[] longs) {
        byte[] bytes = new byte[Long.BYTES * 2];
        byte[] highBytes = longToBytes(longs[1]);
        byte[] lowBytes = longToBytes(longs[0]);
        System.arraycopy(highBytes, 0, bytes, 0, Long.BYTES);
        System.arraycopy(lowBytes, 0, bytes, 8, Long.BYTES);
        return new BigInteger(bytes);
    }

    /**
     * please use this method to set jdk BigInteger to Decimal128Vec
     *
     * @param index row index
     * @param decimal input value
     */
    public void setBigInteger(int index, BigInteger decimal) {
        super.set(index, putDecimal(decimal));
    }

    /**
     * set BigInteger bytes to Decimal128Vec
     *
     * @param index row index
     * @param bigIntegerBytes BigInteger bytes
     * @param isNegative isNegative
     */
    public void setBigInteger(int index, byte[] bigIntegerBytes, boolean isNegative) {
        super.set(index, putDecimal(bigIntegerBytes, isNegative));
    }

    /**
     * please use this method to get jdk BigInteger from Decimal128Vec
     *
     * @param index row index
     * @return new BigInteger
     */
    public BigInteger getBigInteger(int index) {
        return getDecimal(super.get(index));
    }

    /**
     * use this method to get jdk BigInteger bytes and isNegative from Decimal128Vec
     *
     * @param index row index
     * @return isNegative and BigInteger bytes
     */
    public byte[] getBytes(int index) {
        long[] longs = super.get(index);
        byte[] bytes = new byte[Long.BYTES * 2];
        byte[] highBytes = longToBytes(longs[1]);
        byte[] lowBytes = longToBytes(longs[0]);
        System.arraycopy(highBytes, 0, bytes, 0, Long.BYTES);
        System.arraycopy(lowBytes, 0, bytes, 8, Long.BYTES);
        return bytes;
    }
}
