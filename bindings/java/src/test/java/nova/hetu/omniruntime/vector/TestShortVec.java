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

import static nova.hetu.omniruntime.type.DataType.DataTypeId.OMNI_SHORT;
import static org.testng.Assert.assertEquals;
import static org.testng.Assert.assertFalse;
import static org.testng.Assert.assertTrue;

import org.testng.annotations.Test;

import java.util.Arrays;

/**
 * test short vec
 *
 * @since 2022-8-2
 */
public class TestShortVec {
    /**
     * test new vector
     */
    @Test
    public void testNewVector() {
        ShortVec vec = new ShortVec(256);
        assertEquals(vec.getSize(), 256);
        assertEquals(vec.getRealValueBufCapacityInBytes(), 512);
        assertEquals(vec.getType().getId(), OMNI_SHORT);
        vec.close();
    }

    /**
     * test slice
     */
    @Test
    public void testSlice() {
        ShortVec originalVec = new ShortVec(10);
        for (int i = 0; i < originalVec.getSize(); i++) {
            originalVec.set(i, (short) i);
        }
        int offset = 3;
        ShortVec slice1 = originalVec.slice(offset, 4);
        assertEquals(slice1.getSize(), 4);
        for (int i = 0; i < slice1.getSize(); i++) {
            assertEquals(slice1.get(i), originalVec.get(i + offset), "Error item value at: " + i);
        }

        ShortVec slice2 = slice1.slice(1, 2);

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
        ShortVec vec = new ShortVec(256);
        for (int i = 0; i < vec.getSize(); i++) {
            vec.set(i, (short) (i * 2));
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
        short[] values = {1, 3, 4, 6, 7};
        ShortVec vec1 = new ShortVec(values.length);
        vec1.put(values, 0, 0, values.length);
        for (int i = 0; i < values.length; i++) {
            assertEquals(vec1.get(i), values[i]);
        }

        ShortVec vec2 = new ShortVec(values.length);
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
        ShortVec vec = new ShortVec(256);
        for (int i = 0; i < vec.getSize(); i++) {
            if (i % 5 == 0) {
                vec.setNull(i);
            } else {
                vec.set(i, (short) i);
            }
        }
        for (int i = 0; i < vec.getSize(); i++) {
            if (i % 5 == 0) {
                assertTrue(vec.isNull(i));
            } else {
                assertEquals(vec.get(i), i);
            }
        }

        vec.close();
    }

    /**
     * test copy postion
     */
    @Test
    public void testCopyPositions() {
        ShortVec originalVector = new ShortVec(4);
        for (int i = 0; i < originalVector.getSize(); i++) {
            originalVector.set(i, (short) i);
        }

        int[] positions = {1, 3};
        ShortVec copyPositionVector = originalVector.copyPositions(positions, 0, 2);
        assertEquals(copyPositionVector.getRealValueBufCapacityInBytes(), 4);
        for (int i = 0; i < copyPositionVector.getSize(); i++) {
            assertEquals(copyPositionVector.get(i), originalVector.get(positions[i]));
        }

        originalVector.close();
        copyPositionVector.close();
    }

    /**
     * test zero size allocate
     */
    @Test
    public void testZeroSizeAllocate() {
        ShortVec v1 = new ShortVec(0);
        short[] values = new short[0];
        v1.put(values, 0, 0, values.length);
        v1.close();
    }

    @Test
    public void testGetValues() {
        short[] values = {1, 3, 4, 6, 7};
        ShortVec vec = new ShortVec(values.length);
        vec.put(values, 0, 0, values.length);
        assertEquals(vec.get(0, values.length), values);
        short[] expected = {3, 4, 6};
        short[] actual = vec.get(1, 3);
        for (int i = 0; i < actual.length; i++) {
            assertEquals(actual[i], expected[i]);
        }
        vec.close();
    }

    @Test
    public void setShortMax() {
        int len = 1024 * 1024;
        short[] values = new short[len];
        Arrays.fill(values, Short.MAX_VALUE);
        ShortVec max = new ShortVec(len);
        max.put(values, 0, 0, values.length);

        for (int i = 0; i < max.getSize(); i++) {
            assertEquals(max.get(i), Short.MAX_VALUE);
        }

        assertEquals(max.get(0, values.length), values);
        max.close();
    }

    @Test
    public void testNullFlagWithSet() {
        // no null value
        ShortVec noNull = new ShortVec(10);
        assertFalse(noNull.hasNull());
        noNull.close();

        // has null value
        ShortVec hasNulls = new ShortVec(10);
        byte[] nulls = new byte[]{0, 1, 0, 1, 0, 1, 0, 1, 0, 1};
        hasNulls.setNulls(0, nulls, 0, nulls.length);
        assertTrue(hasNulls.hasNull());
        hasNulls.close();

        ShortVec hasNull = new ShortVec(10);
        for (int i = 0; i < hasNull.size; i++) {
            if (i % 2 == 0) {
                hasNull.setNull(i);
            } else {
                hasNull.set(i, (short) i);
            }
        }
        assertTrue(hasNull.hasNull());
        hasNull.close();
    }

    @Test
    public void testNullFlagWithCopyPosition() {
        // has null value
        ShortVec hasNulls = new ShortVec(10);
        byte[] nulls = new byte[]{0, 0, 1, 1, 0, 1, 0, 1, 0, 1};
        hasNulls.setNulls(0, nulls, 0, nulls.length);
        assertTrue(hasNulls.hasNull());

        int[] positions = new int[]{0, 1};
        ShortVec copyPositionNoNull = hasNulls.copyPositions(positions, 0, 2);
        assertFalse(copyPositionNoNull.hasNull());
        copyPositionNoNull.close();

        positions = new int[]{1, 2, 3, 4};
        ShortVec copyPositionHasNull = hasNulls.copyPositions(positions, 0, 4);
        assertTrue(copyPositionHasNull.hasNull());
        copyPositionHasNull.close();

        hasNulls.close();
    }

    @Test
    public void testNullFlagWithSlice() {
        // has null value
        ShortVec hasNulls = new ShortVec(10);
        byte[] nulls = new byte[]{0, 0, 1, 1, 0, 1, 0, 1, 0, 1};
        hasNulls.setNulls(0, nulls, 0, nulls.length);
        assertTrue(hasNulls.hasNull());

        ShortVec sliceNoNull = hasNulls.slice(0, 1);
        assertFalse(sliceNoNull.hasNull());
        sliceNoNull.close();

        ShortVec sliceHasNull = hasNulls.slice(1, 4);
        assertTrue(sliceHasNull.hasNull());
        sliceHasNull.close();

        hasNulls.close();
    }

    @Test
    public void testNullFlagWithAppend() {
        int rowCount = 5;
        ShortVec src1 = new ShortVec(rowCount);

        for (int i = 0; i < rowCount; i++) {
            src1.set(i, (short) (i + 1));
        }

        ShortVec appended = new ShortVec(15);
        appended.append(src1, 0, rowCount);
        src1.close();
        assertFalse(appended.hasNull());

        ShortVec withNull = new ShortVec(rowCount);
        byte[] nulls = new byte[]{0, 1, 1, 0, 1};
        withNull.setNulls(0, nulls, 0, 5);
        appended.append(withNull, 5, rowCount);
        assertTrue(appended.hasNull());

        appended.append(withNull, 10, rowCount);
        assertTrue(appended.hasNull());
        withNull.close();

        appended.close();
    }
}
