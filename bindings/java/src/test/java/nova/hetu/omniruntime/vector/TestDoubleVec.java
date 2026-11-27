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

import static nova.hetu.omniruntime.type.DataType.DataTypeId.OMNI_DOUBLE;
import static org.testng.Assert.assertEquals;
import static org.testng.Assert.assertTrue;

import org.testng.annotations.Test;

import java.util.Arrays;

/**
 * test double vec
 *
 * @since 2021-7-2
 */
public class TestDoubleVec {
    /**
     * test new vector
     */
    @Test
    public void testNewVector() {
        DoubleVec vec = new DoubleVec(256);
        assertEquals(vec.getSize(), 256);
        assertEquals(vec.getRealValueBufCapacityInBytes(), 2048);
        assertEquals(vec.getType().getId(), OMNI_DOUBLE);
        vec.close();
    }

    /**
     * test slice
     */
    @Test
    public void testSlice() {
        DoubleVec originalVec = new DoubleVec(10);
        for (int i = 0; i < originalVec.getSize(); i++) {
            originalVec.set(i, (double) i / 3);
        }
        int offset = 3;
        DoubleVec slice1 = originalVec.slice(offset, 4);
        assertEquals(slice1.getSize(), 4);
        for (int i = 0; i < slice1.getSize(); i++) {
            assertEquals(slice1.get(i), originalVec.get(i + offset), "Error item value at: " + i);
        }

        DoubleVec slice2 = slice1.slice(1, 3);

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
        DoubleVec vec = new DoubleVec(256);
        for (int i = 0; i < vec.getSize(); i++) {
            vec.set(i, (double) i / 3);
        }

        for (int i = 0; i < vec.getSize(); i++) {
            assertEquals(vec.get(i), (double) i / 3);
        }
        vec.close();
    }

    /**
     * test put value
     */
    @Test
    public void testPutValues() {
        double[] values = {1.13, 3.33, 4.44, 6.66, 7.81};
        DoubleVec doubleVec1 = new DoubleVec(values.length);
        doubleVec1.put(values, 0, 0, values.length);
        for (int i = 0; i < values.length; i++) {
            assertEquals(doubleVec1.get(i), values[i]);
        }

        DoubleVec doubleVec2 = new DoubleVec(values.length);
        doubleVec2.put(values, 1, 2, 3);
        for (int i = 0; i < 3; i++) {
            assertEquals(doubleVec2.get(i + 1), values[i + 2]);
        }

        doubleVec1.close();
        doubleVec2.close();
    }

    /**
     * test value null
     */
    @Test
    public void testValueNull() {
        DoubleVec doubleVec = new DoubleVec(256);
        for (int i = 0; i < doubleVec.getSize(); i++) {
            if (i % 5 == 0) {
                doubleVec.setNull(i);
            } else {
                doubleVec.set(i, (double) i / 3);
            }
        }
        for (int i = 0; i < doubleVec.getSize(); i++) {
            if (i % 5 == 0) {
                assertTrue(doubleVec.isNull(i));
            } else {
                assertEquals(doubleVec.get(i), (double) i / 3);
            }
        }

        doubleVec.close();
    }

    /**
     * test copy positions
     */
    @Test
    public void testCopyPositions() {
        DoubleVec originalVector = new DoubleVec(4);
        for (int i = 0; i < originalVector.getSize(); i++) {
            originalVector.set(i, i);
        }

        int[] positions = {1, 3};
        DoubleVec copyPositionVector = originalVector.copyPositions(positions, 0, 2);
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
        DoubleVec v1 = new DoubleVec(0);
        double[] values = new double[0];
        v1.put(values, 0, 0, values.length);
        v1.close();
    }

    @Test
    public void testGetValues() {
        double[] values = {1.13, 3.33, 4.44, 6.66, 7.81};
        DoubleVec doubleVec1 = new DoubleVec(values.length);
        doubleVec1.put(values, 0, 0, values.length);
        assertEquals(doubleVec1.get(0, values.length), values);
        double[] expected = {3.33, 4.44, 6.66};
        double[] actual = doubleVec1.get(1, 3);
        for (int i = 0; i < actual.length; i++) {
            assertEquals(actual[i], expected[i]);
        }
        doubleVec1.close();
    }

    @Test
    public void setDoubleMax() {
        int len = 1024 * 1024;
        double[] values = new double[len];
        Arrays.fill(values, Double.MAX_VALUE);
        DoubleVec max = new DoubleVec(len);
        max.put(values, 0, 0, values.length);

        for (int i = 0; i < max.getSize(); i++) {
            assertEquals(max.get(i), Double.MAX_VALUE);
        }

        assertEquals(max.get(0, values.length), values);
        max.close();
    }
}
