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

import static org.testng.Assert.assertEquals;
import static org.testng.Assert.assertTrue;

import nova.hetu.omniruntime.type.DataType;
import nova.hetu.omniruntime.type.Decimal128DataType;

import org.testng.annotations.Test;

import java.math.BigInteger;

/**
 * test decimal 128-bit vec
 *
 * @since 2021-6-23
 */
public class TestDecimal128Vec {
    /**
     * test new vector
     */
    @Test
    public void testNewVector() {
        Decimal128Vec vec = new Decimal128Vec(256);
        assertEquals(vec.getSize(), 256);
        assertEquals(vec.getRealValueBufCapacityInBytes(), 4096);
        assertEquals(vec.getType().getId(), DataType.DataTypeId.OMNI_DECIMAL128);
        assertEquals(((Decimal128DataType) (vec.getType())).getPrecision(), 38);
        assertEquals(((Decimal128DataType) (vec.getType())).getScale(), 0);
        vec.close();
    }

    /**
     * test slice
     */
    @Test
    public void testSlice() {
        final int size = 10;
        Decimal128Vec vec1 = new Decimal128Vec(size);
        for (int i = 0; i < size; i++) {
            long[] value = {1 << (i + 1), 3};
            vec1.set(i, value);
        }
        Decimal128Vec slice1 = vec1.slice(3, 5);
        Decimal128Vec slice2 = vec1.slice(0, vec1.getSize());
        for (int i = 0; i < slice1.getSize(); i++) {
            assertEquals(vec1.get(i + 3)[0], slice1.get(i)[0], "Error item value at: " + i);
            assertEquals(vec1.get(i + 3)[1], slice1.get(i)[1], "Error item value at: " + i);
        }
        for (int i = 0; i < slice2.getSize(); i++) {
            assertEquals(vec1.get(i)[0], slice2.get(i)[0], "Error item value at: " + i);
            assertEquals(vec1.get(i)[1], slice2.get(i)[1], "Error item value at: " + i);
        }
        vec1.close();
        slice1.close();
        slice2.close();
    }

    /**
     * test set and get value
     */
    @Test
    public void testSetAndGetValue() {
        final int size = 1024;
        Decimal128Vec vec1 = new Decimal128Vec(size);
        long[] values = new long[size * 2];
        for (int i = 0; i < size; i++) {
            long[] value = {i, 3};
            vec1.set(i, value);
            values[i * 2] = i;
            values[i * 2 + 1] = 3;
        }

        for (int i = 0; i < size; i++) {
            assertEquals(values[i * 2], vec1.get(i)[0]);
            assertEquals(values[i * 2 + 1], vec1.get(i)[1]);
        }
        vec1.close();
    }

    /**
     * test put value
     */
    @Test
    public void testPutValues() {
        long[] values = {1, 3, 4, 6, 7, 8};
        Decimal128Vec vec1 = new Decimal128Vec(values.length / 2);
        vec1.put(values, 0, 0, values.length);
        for (int i = 0; i < values.length / 2; i++) {
            assertEquals(vec1.get(i)[0], values[i * 2]);
            assertEquals(vec1.get(i)[1], values[i * 2 + 1]);
        }

        Decimal128Vec vec2 = new Decimal128Vec(values.length / 2);
        vec2.put(values, 1, 2, 4);
        for (int i = 0; i < 2; i++) {
            assertEquals(vec2.get(i + 1)[0], values[i * 2 + 2]);
            assertEquals(vec2.get(i + 1)[1], values[i * 2 + 3]);
        }

        vec1.close();
        vec2.close();
    }

    /**
     * test value null
     */
    @Test
    public void testValueNull() {
        final int size = 10;
        Decimal128Vec vec = new Decimal128Vec(size);
        for (int i = 0; i < size; i++) {
            long[] value = {i, 3};
            vec.set(i, value);
        }

        for (int i = 0; i < size; i++) {
            vec.setNull(i);
        }

        for (int i = 0; i < size; i++) {
            assertTrue(vec.isNull(i));
        }
        vec.close();
    }

    /**
     * test copy position
     */
    @Test
    public void testCopyPositions() {
        Decimal128Vec originalVector = new Decimal128Vec(4);
        for (int i = 0; i < originalVector.getSize(); i++) {
            long[] value = {0, i};
            originalVector.set(i, value);
        }

        int[] positions = {1, 3};
        Decimal128Vec copyPositionVector = originalVector.copyPositions(positions, 0, 2);
        assertEquals(copyPositionVector.getRealValueBufCapacityInBytes(), 32);
        for (int i = 0; i < copyPositionVector.getSize(); i++) {
            assertEquals(copyPositionVector.get(i), originalVector.get(positions[i]));
        }

        originalVector.close();
        copyPositionVector.close();
    }

    /**
     * test zero sized allocate
     */
    @Test
    public void testZeroSizeAllocate() {
        Decimal128Vec v1 = new Decimal128Vec(0);
        long[] values = new long[0];
        v1.put(values, 0, 0, values.length);
        v1.close();
    }

    /**
     * test BigInteger to long array and vice versa
     */
    @Test
    public void testBigIntegerTrans() {
        BigInteger bigInteger = new BigInteger("11111111111111111111111111111111111111");

        long[] longs = Decimal128Vec.putDecimal(bigInteger);
        assertEquals(longs[1], 602334540269724685L);
        assertEquals(longs[0], -8122175193715281465L);

        BigInteger newBigInteger = Decimal128Vec.getDecimal(longs);
        assertEquals(newBigInteger, bigInteger);
    }

    /**
     * test Decimal128Vec set/get BigInteger
     */
    @Test
    public void testSetGetBigInteger() {
        final int size = 1024;
        BigInteger decimal128 = new BigInteger("11111111111111111111111111111111111111");
        Decimal128Vec vec1 = new Decimal128Vec(size);
        BigInteger decimal64 = new BigInteger("111111");
        Decimal128Vec vec2 = new Decimal128Vec(size);

        for (int i = 0; i < size; ++i) {
            vec1.setBigInteger(i, decimal128);
        }

        for (int i = 0; i < size; ++i) {
            BigInteger val = vec1.getBigInteger(i);
            assertEquals(val, decimal128);
        }

        for (int i = 0; i < size; ++i) {
            vec2.setBigInteger(i, decimal64);
        }

        for (int i = 0; i < size; ++i) {
            BigInteger val = vec2.getBigInteger(i);
            assertEquals(val, decimal64);
        }
        vec1.close();
        vec2.close();
    }

    /**
     * test Decimal128Vec set/get BigInteger using bytes
     */
    @Test
    public void testSetGetBigIntegerBytes() {
        final int size = 1024;
        BigInteger decimal128 = new BigInteger("11111111111111111111111111111111111111");
        byte[] bytes = decimal128.toByteArray();
        boolean isNegative = decimal128.compareTo(new BigInteger("0")) == -1;
        Decimal128Vec vec1 = new Decimal128Vec(size);

        for (int i = 0; i < size; ++i) {
            vec1.setBigInteger(i, bytes, isNegative);
        }

        for (int i = 0; i < size; ++i) {
            byte[] val = vec1.getBytes(i);
            BigInteger bigInteger = new BigInteger(val);
            assertEquals(bigInteger, decimal128);
        }
        vec1.close();
    }

    /**
     * test Decimal128Vec set/get BigInteger
     */
    @Test
    public void testBigIntegerByteLengthBetweenEightAndSixteen() {
        BigInteger decimal1 = new BigInteger("111311100000000000000000000");
        BigInteger decimal2 = new BigInteger("-99999999999999999999999999");
        Decimal128Vec vec = new Decimal128Vec(2);

        vec.setBigInteger(0, decimal1);
        vec.setBigInteger(1, decimal2);

        BigInteger val1 = vec.getBigInteger(0);
        BigInteger val2 = vec.getBigInteger(1);
        assertEquals(val1, decimal1);
        assertEquals(val2, decimal2);
        vec.close();
    }
}
