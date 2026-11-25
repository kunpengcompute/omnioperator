/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
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

package nova.hetu.omniruntime.utils;

import nova.hetu.omniruntime.vector.OmniBuffer;

/**
 * nulls buffer util.
 *
 * @since 2025-02-15
 */
public class NullsBufHelper {
    /**
     * Byte index obtained from the bit field index position
     *
     * @param absoluteBitIndex - Bit field index position
     * @return byte index
     */
    public static int byteIndex(int absoluteBitIndex) {
        return absoluteBitIndex >> 3;
    }

    /**
     * Obtain the bit field where the actual byte position is located based on the bit field index position.
     *
     * @param absoluteBitIndex Bit field index position
     * @return Bit field in which the actual byte position is located
     */
    public static int bitIndex(int absoluteBitIndex) {
        return absoluteBitIndex & 7;
    }

    /**
     * Calculate the number of bytes occupied by the number of bits.
     *
     * @param bits Number of bit fields
     * @return number of bytes
     */
    public static int nBytes(int bits) {
        return (bits + (8 - 1)) / 8;
    }

    /**
     * Indicates whether the position specified by the bit field is marked.
     *
     * @param nullsBuf bit field memory
     * @param index Indexes
     * @return flag
     */
    public static int isSet(OmniBuffer nullsBuf, int index) {
        int byteIndex = byteIndex(index);
        int bitIndex = bitIndex(index);
        int currentByte = nullsBuf.getByte(byteIndex);
        return currentByte >> bitIndex & 1;
    }

    /**
     * Indicates whether the position specified by the bit field is marked.
     *
     * @param isNulls bit field memory
     * @param index Indexes
     * @return flag
     */
    public static int isSet(byte[] isNulls, int index) {
        return isNulls[byteIndex(index)] >> bitIndex(index) & 1;
    }

    /**
     * Indicates the flag of the specified position of the clear bit field.
     *
     * @param nullsBuf bit field memory
     * @param index Indexes
     */
    public static void unsetBit(OmniBuffer nullsBuf, int index) {
        int byteIndex = byteIndex(index);
        int bitIndex = bitIndex(index);
        int currentByte = nullsBuf.getByte(byteIndex);
        int bitMask = 1 << bitIndex;
        currentByte &= ~bitMask;
        nullsBuf.setByte(byteIndex, (byte) currentByte);
    }

    /**
     * Specifies the position of the flag bit field.
     *
     * @param nullsBuf bit field memory
     * @param index Indexes
     */
    public static void setBit(OmniBuffer nullsBuf, int index) {
        int byteIndex = byteIndex(index);
        int bitIndex = bitIndex(index);
        int currentByte = nullsBuf.getByte(byteIndex);
        int bitMask = 1 << bitIndex;
        currentByte |= bitMask;
        nullsBuf.setByte(byteIndex, (byte) currentByte);
    }

    /**
     * Bulk Mark Bit Field Memory
     *
     * @param nullsBuf bit field memory
     * @param index Mark Start Position
     * @param isNulls Tag Value array
     * @param srcStart Start position of the tag value array
     * @param length Tag Value array length
     */
    public static void setBit(OmniBuffer nullsBuf, int index, byte[] isNulls, int srcStart, int length) {
        int i = 0;
        while (i < length) {
            setValidityBit(nullsBuf, index + i, isNulls[srcStart + i]);
            i++;
        }
    }

    /**
     * Bulk Mark Bit Field Memory
     *
     * @param nullsBuf bit field memory
     * @param index Mark Start Position
     * @param isBitNulls Tag Value array (Bit)
     * @param srcStart Start position of the tag value array (Bit)
     * @param length Tag Value array length (Bit)
     */
    public static void setBitByBits(OmniBuffer nullsBuf, int index, byte[] isBitNulls, int srcStart, int length) {
        int i = 0;
        while (i < length) {
            setValidityBit(nullsBuf, index + i, isSet(isBitNulls, srcStart + i));
            i++;
        }
    }

    /**
     * Obtains bit field tags in batches and adds them to an array.
     *
     * @param nullsBuf bit field memory
     * @param index Mark Start Position
     * @param nullsArray Tag Value array
     * @param targetStart Start position of the tag value array
     * @param length Tag Value array length
     */
    public static void getBytes(OmniBuffer nullsBuf, int index, byte[] nullsArray, int targetStart, int length) {
        int i = 0;
        while (i < length) {
            nullsArray[targetStart + i] = (byte) isSet(nullsBuf, index + i);
            i++;
        }
    }

    /**
     * Marks the specified position in the bit field memory with a specified value.
     *
     * @param nullsBuf bit field memory
     * @param index Indexes
     * @param value value mark
     */
    public static void setValidityBit(OmniBuffer nullsBuf, int index, int value) {
        int byteIndex = byteIndex(index);
        int bitIndex = bitIndex(index);
        int currentByte = nullsBuf.getByte(byteIndex);
        int bitMask = 1 << bitIndex;
        if (value != 0) {
            currentByte |= bitMask;
        } else {
            currentByte &= ~bitMask;
        }
        nullsBuf.setByte(byteIndex, (byte) currentByte);
    }
}
