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

import nova.hetu.omniruntime.type.LongDataType;

import org.testng.annotations.Test;

import java.util.Arrays;

/**
 * test long vec
 *
 * @since 2021-7-2
 */
public class TestLongVec {
    /**
     * test new vector
     */
    @Test
    public void testNewVector() {
        LongVec vec = new LongVec(256);
        assertEquals(vec.getSize(), 256);
        assertEquals(vec.getRealValueBufCapacityInBytes(), 2048);
        assertEquals(vec.getType(), LongDataType.LONG);
        vec.close();
    }

    /**
     * test slice
     */
    @Test
    public void testSlice() {
        LongVec originalVec = new LongVec(10);
        for (int i = 0; i < originalVec.getSize(); i++) {
            originalVec.set(i, i);
        }
        int offset = 3;
        LongVec slice1 = originalVec.slice(offset, 4);
        assertEquals(slice1.getSize(), 4);
        for (int i = 0; i < slice1.getSize(); i++) {
            assertEquals(slice1.get(i), originalVec.get(i + offset), "Error item value at: " + i);
        }

        LongVec slice2 = slice1.slice(1, 2);

        for (int i = 0; i < slice2.getSize(); i++) {
            assertEquals(slice2.get(i), originalVec.get(i + offset + 1), "Error item value at: " + i);
        }

        originalVec.close();
        slice1.close();
        slice2.close();
    }

    /**
     * test set and get value
     */
    @Test
    public void testSetAndGetValue() {
        LongVec vec = new LongVec(256);
        for (int i = 0; i < vec.getSize(); i++) {
            vec.set(i, i * 2);
        }

        for (int i = 0; i < vec.getSize(); i++) {
            assertEquals(vec.get(i), i * 2);
        }
        vec.close();
    }

    /**
     * test put value
     */
    @Test
    public void testPutValues() {
        long[] values = {1, 3, 4, 6, 7};
        LongVec vec1 = new LongVec(values.length);
        vec1.put(values, 0, 0, values.length);
        for (int i = 0; i < values.length; i++) {
            assertEquals(vec1.get(i), values[i]);
        }

        LongVec vec2 = new LongVec(values.length);
        vec2.put(values, 1, 2, 3);
        for (int i = 0; i < 3; i++) {
            assertEquals(vec2.get(i + 1), values[i + 2]);
        }

        vec1.close();
        vec2.close();
    }

    /**
     * test value null
     */
    @Test
    public void testValueNull() {
        LongVec longVec = new LongVec(256);
        for (int i = 0; i < longVec.getSize(); i++) {
            if (i % 5 == 0) {
                longVec.setNull(i);
            } else {
                longVec.set(i, i);
            }
        }
        for (int i = 0; i < longVec.getSize(); i++) {
            if (i % 5 == 0) {
                assertTrue(longVec.isNull(i));
            } else {
                assertEquals(longVec.get(i), i);
            }
        }

        longVec.close();
    }

    /**
     * test copy postion
     */
    @Test
    public void testCopyPositions() {
        LongVec originalVector = new LongVec(4);
        for (int i = 0; i < originalVector.getSize(); i++) {
            originalVector.set(i, i);
        }

        int[] positions = {1, 3};
        LongVec copyPositionVector = originalVector.copyPositions(positions, 0, 2);
        assertEquals(copyPositionVector.getRealValueBufCapacityInBytes(), 16);
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
        LongVec v1 = new LongVec(0);
        long[] values = new long[0];
        v1.put(values, 0, 0, values.length);
        v1.close();
    }

    @Test
    public void testGetValues() {
        long[] values = {1, 3, 4, 6, 7};
        LongVec vec = new LongVec(values.length);
        vec.put(values, 0, 0, values.length);
        assertEquals(vec.get(0, values.length), values);
        long[] expected = {3, 4, 6};
        long[] actual = vec.get(1, 3);
        for (int i = 0; i < actual.length; i++) {
            assertEquals(actual[i], expected[i]);
        }
        vec.close();
    }

    @Test
    public void setLongMax() {
        int len = 1024 * 1024;
        long[] values = new long[len];
        Arrays.fill(values, Long.MAX_VALUE);
        LongVec max = new LongVec(len);
        max.put(values, 0, 0, values.length);

        for (int i = 0; i < max.getSize(); i++) {
            assertEquals(max.get(i), Long.MAX_VALUE);
        }

        assertEquals(max.get(0, values.length), values);
        max.close();
    }
}
