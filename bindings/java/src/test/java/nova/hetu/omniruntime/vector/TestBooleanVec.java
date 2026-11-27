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

import static nova.hetu.omniruntime.type.DataType.DataTypeId.OMNI_BOOLEAN;
import static org.testng.Assert.assertEquals;
import static org.testng.Assert.assertFalse;
import static org.testng.Assert.assertTrue;

import org.testng.annotations.Test;

/**
 * Test boolean vec
 *
 * @since 2021-7-2
 */
public class TestBooleanVec {
    /**
     * test new vector
     */
    @Test
    public void testNewVector() {
        BooleanVec vector1 = new BooleanVec(256);
        assertEquals(vector1.getSize(), 256);
        assertEquals(vector1.getRealValueBufCapacityInBytes(), 256);
        assertEquals(vector1.getType().getId(), OMNI_BOOLEAN);
        vector1.close();

        BooleanVec vector2 = new BooleanVec(251);
        assertEquals(vector2.getSize(), 251);
        assertEquals(vector2.getRealValueBufCapacityInBytes(), 251);
        assertEquals(vector2.getType().getId(), OMNI_BOOLEAN);
        vector2.close();
    }

    /**
     * test slice
     */
    @Test
    public void testSlice() {
        BooleanVec originalVec = new BooleanVec(10);
        for (int i = 0; i < originalVec.getSize(); i++) {
            originalVec.set(i, i % 2 == 0);
        }
        int offset = 3;
        BooleanVec slice1 = originalVec.slice(offset, 4);
        assertEquals(slice1.getSize(), 4);
        for (int i = 0; i < slice1.getSize(); i++) {
            assertEquals(slice1.get(i), originalVec.get(i + offset), "Error item value at: " + i);
        }

        BooleanVec slice2 = slice1.slice(1, 2);

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
        final int size = 1024;
        BooleanVec vec1 = new BooleanVec(size);
        for (int i = 0; i < size; i++) {
            vec1.set(i, i % 2 == 0);
        }

        for (int i = 0; i < size; i++) {
            if (i % 2 == 0) {
                assertTrue(vec1.get(i));
            } else {
                assertFalse(vec1.get(i));
            }
        }
        vec1.close();
    }

    /**
     * test set values
     */
    @Test
    public void testSetValues() {
        boolean[] values = {true, false, false, true, true};
        BooleanVec vector1 = new BooleanVec(values.length);
        vector1.put(values, 0, 0, values.length);
        for (int i = 0; i < values.length; i++) {
            assertEquals(vector1.get(i), values[i]);
        }
        vector1.close();

        BooleanVec vector2 = new BooleanVec(values.length);
        vector2.put(values, 1, 2, 3);
        for (int i = 0; i < 3; i++) {
            assertEquals(vector2.get(i + 1), values[i + 2]);
        }
        vector2.close();

        byte[] byteValues = {1, 0, 0, 1, 1};
        BooleanVec vector3 = new BooleanVec(byteValues.length);
        vector3.put(byteValues, 0, 0, byteValues.length);
        for (int i = 0; i < byteValues.length; i++) {
            assertEquals(vector3.get(i), byteValues[i] == Vec.NULL);
        }
        vector3.close();
        BooleanVec vector4 = new BooleanVec(byteValues.length);
        vector4.put(byteValues, 1, 2, 3);
        for (int i = 0; i < 3; i++) {
            assertEquals(vector4.get(i + 1), byteValues[i + 2] == Vec.NULL);
        }
        vector4.close();
    }

    /**
     * test value null
     */
    @Test
    public void testValueNull() {
        BooleanVec vector1 = new BooleanVec(256);
        for (int i = 0; i < vector1.getSize(); i++) {
            if (i % 5 == 0) {
                vector1.setNull(i);
            } else {
                vector1.set(i, i % 2 == 0);
            }
        }
        for (int i = 0; i < vector1.getSize(); i++) {
            if (i % 5 == 0) {
                assertTrue(vector1.isNull(i));
            } else {
                assertEquals(vector1.get(i), i % 2 == 0);
            }
        }

        vector1.close();
    }

    /**
     * test copy positions
     */
    @Test
    public void testCopyPositions() {
        BooleanVec originalVector = new BooleanVec(4);
        for (int i = 0; i < originalVector.getSize(); i++) {
            originalVector.set(i, i % 2 == 0);
        }

        int[] positions = {1, 3};
        BooleanVec copyPositionVector = originalVector.copyPositions(positions, 0, 2);
        assertEquals(copyPositionVector.getRealValueBufCapacityInBytes(), 2);
        for (int i = 0; i < copyPositionVector.getSize(); i++) {
            assertEquals(copyPositionVector.get(i), originalVector.get(positions[i]));
        }

        originalVector.close();
        copyPositionVector.close();
    }

    @Test
    public void testGetValues() {
        boolean[] values = new boolean[1024];
        for (int i = 0; i < values.length; i++) {
            values[i] = i % 3 == 0;
        }
        BooleanVec originalVec = new BooleanVec(values.length);
        originalVec.put(values, 0, 0, values.length);

        boolean[] actual = originalVec.get(0, values.length);
        assertEquals(actual, values);
        originalVec.close();
    }
}
