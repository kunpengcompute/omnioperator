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

import static nova.hetu.omniruntime.type.DataType.DataTypeId.OMNI_INT;
import static org.testng.Assert.assertEquals;
import static org.testng.Assert.assertFalse;
import static org.testng.Assert.assertTrue;

import org.testng.annotations.Test;

import java.util.Arrays;

/**
 * test int vec
 *
 * @since 2021-7-2
 */
public class TestIntVec {
    /**
     * test new vector
     */
    @Test
    public void testNewVector() {
        IntVec vec = new IntVec(256);
        assertEquals(vec.getSize(), 256);
        assertEquals(vec.getRealValueBufCapacityInBytes(), 1024);
        assertEquals(vec.getType().getId(), OMNI_INT);
        vec.close();
    }

    /**
     * test slice
     */
    @Test
    public void testSlice() {
        IntVec oritinalVec = new IntVec(10);
        for (int i = 0; i < oritinalVec.getSize(); i++) {
            oritinalVec.set(i, i);
        }
        int offset = 3;
        IntVec slice1 = oritinalVec.slice(offset, 4);
        assertEquals(slice1.getSize(), 4);
        for (int i = 0; i < slice1.getSize(); i++) {
            assertEquals(slice1.get(i), oritinalVec.get(i + offset), "Error item value at: " + i);
        }

        IntVec slice2 = slice1.slice(1, 2);

        for (int i = 0; i < slice2.getSize(); i++) {
            assertEquals(slice2.get(i), oritinalVec.get(i + offset + 1), "Error item value at: " + i);
        }

        oritinalVec.close();
        slice1.close();
        slice2.close();
    }

    /**
     * test set and get value
     */
    @Test
    public void testSetAndGetValue() {
        IntVec vec = new IntVec(256);
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
        int[] values = {1, 3, 4, 6, 7};
        IntVec vec1 = new IntVec(values.length);
        vec1.put(values, 0, 0, values.length);
        for (int i = 0; i < values.length; i++) {
            assertEquals(vec1.get(i), values[i]);
        }

        IntVec vec2 = new IntVec(values.length);
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
        IntVec vec = new IntVec(256);
        for (int i = 0; i < vec.getSize(); i++) {
            if (i % 5 == 0) {
                vec.setNull(i);
            } else {
                vec.set(i, i);
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
     * test copy position
     */
    @Test
    public void testCopyPositions() {
        IntVec originalVector = new IntVec(4);
        for (int i = 0; i < originalVector.getSize(); i++) {
            originalVector.set(i, i);
        }

        int[] positions = {1, 3};
        IntVec copyPositionVector = originalVector.copyPositions(positions, 0, 2);
        assertEquals(copyPositionVector.getRealValueBufCapacityInBytes(), 8);
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
        IntVec v1 = new IntVec(0);
        int[] values = new int[0];
        v1.put(values, 0, 0, values.length);
        v1.close();
    }

    @Test
    public void testGetValues() {
        int[] values = {1, 3, 4, 6, 7};
        IntVec vec = new IntVec(values.length);
        vec.put(values, 0, 0, values.length);
        assertEquals(vec.get(0, values.length), values);
        int[] expected = {3, 4, 6};
        int[] actual = vec.get(1, 3);
        for (int i = 0; i < actual.length; i++) {
            assertEquals(actual[i], expected[i]);
        }
        vec.close();
    }

    @Test
    public void setIntMax() {
        int len = 1024 * 1024;
        int[] values = new int[len];
        Arrays.fill(values, Integer.MAX_VALUE);
        IntVec max = new IntVec(len);
        max.put(values, 0, 0, values.length);

        for (int i = 0; i < max.getSize(); i++) {
            assertEquals(max.get(i), Integer.MAX_VALUE);
        }

        assertEquals(max.get(0, values.length), values);
        max.close();
    }

    @Test
    public void testNullFlagWithSet() {
        // no null value
        IntVec noNull = new IntVec(10);
        assertFalse(noNull.hasNull());
        noNull.close();

        // has null value
        IntVec hasNulls = new IntVec(10);
        byte[] nulls = new byte[] {0, 1, 0, 1, 0, 1, 0, 1, 0, 1};
        hasNulls.setNulls(0, nulls, 0, nulls.length);
        assertTrue(hasNulls.hasNull());
        hasNulls.close();

        IntVec hasNull = new IntVec(10);
        for (int i = 0; i < hasNull.size; i++) {
            if (i % 2 == 0) {
                hasNull.setNull(i);
            } else {
                hasNull.set(i, i);
            }
        }
        assertTrue(hasNull.hasNull());
        hasNull.close();
    }

    @Test
    public void testNullFlagWithCopyPosition() {
        // has null value
        IntVec hasNulls = new IntVec(10);
        byte[] nulls = new byte[] {0, 0, 1, 1, 0, 1, 0, 1, 0, 1};
        hasNulls.setNulls(0, nulls, 0, nulls.length);
        assertTrue(hasNulls.hasNull());

        int[] positions = new int[]{0, 1};
        IntVec copyPositionNoNull = hasNulls.copyPositions(positions, 0, 2);
        assertFalse(copyPositionNoNull.hasNull());
        copyPositionNoNull.close();

        positions = new int[]{1, 2, 3, 4};
        IntVec copyPositionHasNull = hasNulls.copyPositions(positions, 0, 4);
        assertTrue(copyPositionHasNull.hasNull());
        copyPositionHasNull.close();

        hasNulls.close();
    }

    @Test
    public void testNullFlagWithSlice() {
        // has null value
        IntVec hasNulls = new IntVec(10);
        byte[] nulls = new byte[] {0, 0, 1, 1, 0, 1, 0, 1, 0, 1};
        hasNulls.setNulls(0, nulls, 0, nulls.length);
        assertTrue(hasNulls.hasNull());

        IntVec sliceNoNull = hasNulls.slice(0, 1);
        assertFalse(sliceNoNull.hasNull());
        sliceNoNull.close();

        IntVec sliceHasNull = hasNulls.slice(1, 3);
        assertTrue(sliceHasNull.hasNull());
        sliceHasNull.close();

        hasNulls.close();
    }

    @Test
    public void testNullFlagWithAppend() {
        int rowCount = 5;
        IntVec src1 = new IntVec(rowCount);

        for (int i = 0; i < rowCount; i++) {
            src1.set(i, i + 1);
        }

        IntVec appended = new IntVec(15);
        appended.append(src1, 0, rowCount);
        src1.close();
        assertFalse(appended.hasNull());

        IntVec withNull = new IntVec(rowCount);
        byte[] nulls = new byte[] {0, 1, 1, 0, 1};
        withNull.setNulls(0, nulls, 0, 5);
        appended.append(withNull, 5, rowCount);
        assertTrue(appended.hasNull());

        appended.append(withNull, 10, rowCount);
        assertTrue(appended.hasNull());
        withNull.close();

        appended.close();
    }
}
