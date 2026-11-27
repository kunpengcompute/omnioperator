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

import static nova.hetu.omniruntime.type.DataType.DataTypeId.OMNI_VARCHAR;
import static org.testng.Assert.assertEquals;
import static org.testng.Assert.assertFalse;
import static org.testng.Assert.assertTrue;

import org.testng.Assert;
import org.testng.annotations.Test;

import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;

/**
 * test varchar vec
 *
 * @since 2021-7-2
 */
public class TestVarcharVec {
    private static final int INIT_CAPACITY_IN_BYTES = 4 * 1024; // 4K

    /**
     * test new vector
     */
    @Test
    public void testNewVector() {
        VarcharVec vec = new VarcharVec(256);
        assertEquals(vec.getSize(), 256);
        assertEquals(vec.getRealValueBufCapacityInBytes(), 0);
        assertEquals(vec.getType().getId(), OMNI_VARCHAR);
        vec.close();
    }

    /**
     * test slice
     */
    @Test
    public void testSlice() {
        int size = 10;
        VarcharVec originalVec = new VarcharVec(size);
        String tmpStr = "testvarchar";
        for (int i = 0; i < size; i++) {
            String str = tmpStr.substring(0, i) + i;
            originalVec.set(i, str.getBytes(StandardCharsets.UTF_8));
        }
        assertEquals(originalVec.getRealValueBufCapacityInBytes(), 55);

        int offset = 3;
        VarcharVec sliceVec1 = originalVec.slice(offset, 4);
        assertEquals(sliceVec1.getSize(), 4);
        assertEquals(sliceVec1.getRealValueBufCapacityInBytes(), 22);

        for (int i = 0; i < sliceVec1.getSize(); i++) {
            byte[] actualValue = sliceVec1.get(i);
            byte[] expectedValue = originalVec.get(i + offset);
            assertEquals(actualValue, expectedValue);
        }

        VarcharVec sliceVec2 = sliceVec1.slice(1, 2);
        assertEquals(sliceVec2.getSize(), 2);

        for (int i = 0; i < sliceVec2.getSize(); i++) {
            byte[] actualValue = sliceVec2.get(i);
            byte[] expectedValue = originalVec.get(i + offset + 1);
            assertEquals(actualValue, expectedValue);
        }
        sliceVec2.close();
        sliceVec1.close();
        originalVec.close();
    }

    /**
     * test set and get value
     */
    @Test
    public void testSetAndGetValue() {
        int size = 4;
        VarcharVec varcharVec = new VarcharVec(size);
        String tmpStr = "test";
        for (int i = 0; i < 4; i++) {
            String str = tmpStr.substring(0, i) + i;
            varcharVec.set(i, str.getBytes(StandardCharsets.UTF_8));
        }

        for (int i = 0; i < 4; i++) {
            String str = tmpStr.substring(0, i) + i;
            byte[] actualValue = varcharVec.get(i);
            assertEquals(actualValue, str.getBytes(StandardCharsets.UTF_8));
        }

        varcharVec.close();
    }

    @Test
    public void testPutValues() {
        int size = 100;
        int[] offsets = new int[size * 2 + 1];
        StringBuilder data = new StringBuilder();
        for (int i = 0; i < size; i++) {
            String str = "test" + i;
            offsets[i + 1] = str.length() + offsets[i];
            data.append(str);
        }

        for (int i = 0; i < size; i++) {
            String str = i + "put";
            offsets[size + i + 1] = str.length() + offsets[size + i];
            data.append(str);
        }

        VarcharVec values = new VarcharVec(size * 2);
        values.put(0, data.toString().getBytes(StandardCharsets.UTF_8), 0, offsets, 0, size);
        values.put(size, data.toString().getBytes(StandardCharsets.UTF_8), 0, offsets, size, size);
        ByteBuffer buffer = ByteBuffer.wrap(data.toString().getBytes(StandardCharsets.UTF_8));
        for (int i = 0; i < size * 2; i++) {
            assertEquals(new String(values.get(i)),
                    new String(getDataFromBuffer(buffer, offsets[i], offsets[i + 1] - offsets[i])));
        }
        values.close();
    }

    private byte[] getDataFromBuffer(ByteBuffer buffer, int offsetInBytes, int length) {
        byte[] data = new byte[length];
        buffer.position(offsetInBytes);
        buffer.get(data, 0, length);
        return data;
    }

    /**
     * test value null
     */
    @Test
    public void testValueNull() {
        VarcharVec varcharVec = new VarcharVec(256);
        for (int i = 0; i < varcharVec.getSize(); i++) {
            if (i % 5 == 0) {
                varcharVec.setNull(i);
            } else {
                varcharVec.set(i, ("test" + i).getBytes(StandardCharsets.UTF_8));
            }
        }
        for (int i = 0; i < varcharVec.getSize(); i++) {
            if (i % 5 == 0) {
                assertTrue(varcharVec.isNull(i));
            } else {
                assertEquals("test" + i, new String(varcharVec.get(i), StandardCharsets.UTF_8));
            }
        }

        varcharVec.close();
    }

    @Test
    public void testBatchSetValueNull() {
        int size = 256;
        boolean[] isNulls = new boolean[size];
        for (int i = 0; i < size; i++) {
            isNulls[i] = i % 2 == 0;
        }
        VarcharVec varcharVec = new VarcharVec(size);
        varcharVec.setNulls(0, isNulls, 0, isNulls.length);
        assertTrue(varcharVec.hasNull());
        assertEquals(varcharVec.getValuesNulls(0, size), isNulls);
        int offset = 3;
        boolean[] acutal = varcharVec.getValuesNulls(offset, size / 2);
        for (int i = 0; i < size / 2; i++) {
            assertEquals(acutal[i], isNulls[i + offset]);
        }
        varcharVec.close();
    }

    /**
     * test copy position
     */
    @Test
    public void testCopyPositions() {
        VarcharVec originalVector = new VarcharVec(4);
        String tmpStr = "test";
        for (int i = 0; i < 4; i++) {
            String str = tmpStr.substring(0, i) + i;
            originalVector.set(i, str.getBytes(StandardCharsets.UTF_8));
        }

        int[] positions = {1, 3};
        VarcharVec copyPostionVector = originalVector.copyPositions(positions, 0, 2);

        for (int i = 0; i < copyPostionVector.getSize(); i++) {
            byte[] expectedValue = originalVector.get(positions[i]);
            byte[] actualValue = copyPostionVector.get(i);
            assertEquals(actualValue, expectedValue);
        }

        originalVector.close();
        copyPostionVector.close();
    }

    @Test
    public void testGetValues() {
        int size = 10;
        StringBuilder getData = new StringBuilder();
        VarcharVec getVec = new VarcharVec(size);
        for (int i = 0; i < size; i++) {
            String str = "gets" + i;
            getVec.set(i, str.getBytes(StandardCharsets.UTF_8));
            getData.append(str);
        }

        byte[] actual = getVec.get(0, size / 2);
        ByteBuffer buffer = ByteBuffer.wrap(getData.toString().getBytes(StandardCharsets.UTF_8));
        int total = 0;
        for (int i = 0; i < size / 2; i++) {
            total += getVec.getDataLength(i);
        }
        byte[] expected = getDataFromBuffer(buffer, 0, total);
        assertEquals(getString(actual), getString(expected));

        int getLen = 5;
        int offset = 2;
        byte[] acutal1 = getVec.get(offset, getLen);
        ByteBuffer buffer1 = ByteBuffer.wrap(acutal1);
        int[] offsets = new int[getLen + 1];
        offsets[0] = 0;

        for (int i = 1; i < offsets.length; i++) {
            offsets[i] = getVec.getDataLength(offset + i - 1) + offsets[i - 1];
        }

        for (int i = 0; i < getLen; i++) {
            assertEquals(getString(getDataFromBuffer(buffer1, offsets[i], offsets[i + 1] - offsets[i])),
                    getString(getVec.get(i + offset)));
        }
        getVec.close();
    }

    @Test
    public void testEmptyString() {
        String[] data = new String[]{"a", "ef", "", "ef", "", ""};
        String[] expected = new String[]{"a", "ef", "", "ef", "", ""};
        int size = 6;
        VarcharVec varcharVec = new VarcharVec(size);
        for (int i = 0; i < size; i++) {
            varcharVec.set(i, data[i].getBytes(StandardCharsets.UTF_8));
        }

        String[] result = new String[size];
        for (int i = 0; i < size; i++) {
            result[i] = getString(varcharVec.get(i));
        }

        Assert.assertEquals(result, expected);

        VarcharVec vec2 = new VarcharVec(size);
        int[] offsets = new int[]{0, 1, 3, 3, 5, 5, 5};
        StringBuilder sb = new StringBuilder();
        for (String str : data) {
            sb.append(str);
        }
        vec2.put(0, sb.toString().getBytes(StandardCharsets.UTF_8), 0, offsets, 0, size);

        String[] result1 = new String[size];
        for (int i = 0; i < size; i++) {
            result1[i] = getString(vec2.get(i));
        }

        Assert.assertEquals(result1, expected);

        // slice
        VarcharVec sliceEmpty = varcharVec.slice(2, 3);
        String emptyString = "";
        Assert.assertEquals(getString(sliceEmpty.get(0)), emptyString);

        // copyPosition
        int[] positions = new int[]{2, 4, 5};
        VarcharVec copyPosition = varcharVec.copyPositions(positions, 0, 3);
        for (int i = 0; i < copyPosition.size; i++) {
            Assert.assertEquals(getString(copyPosition.get(i)), emptyString);
        }

        varcharVec.close();
        vec2.close();
        sliceEmpty.close();
        copyPosition.close();
    }

    @Test
    public void testSetExpandCapacity() {
        int rowCount = 4;
        VarcharVec varcharVec = new VarcharVec(rowCount);
        String baseStr = "test";
        for (int i = 0; i < rowCount; i++) {
            String str = baseStr.substring(0, i);
            str += i;
            varcharVec.set(i, str.getBytes(StandardCharsets.UTF_8));
        }
        Assert.assertEquals(varcharVec.getCapacityInBytes(), INIT_CAPACITY_IN_BYTES);

        for (int i = 0; i < rowCount; i++) {
            String str = baseStr.substring(0, i);
            str += i;
            Assert.assertEquals(new String(varcharVec.get(i)), str);
        }

        // no capacity specified when created, init capacity is 32K
        rowCount = 8000;
        VarcharVec vector1 = new VarcharVec(rowCount);
        for (int i = 0; i < rowCount; i++) {
            String str = baseStr + i;
            vector1.set(i, str.getBytes(StandardCharsets.UTF_8));
        }

        // init capacity is 32K, expansion to 64k at a time
        int expectedExpandedCapacity1 = 65536;
        Assert.assertEquals(vector1.getCapacityInBytes(), expectedExpandedCapacity1);
        for (int i = 0; i < rowCount; i++) {
            Assert.assertEquals(getString(vector1.get(i)), baseStr + i);
        }

        VarcharVec initZeroCapacityVector = new VarcharVec(1);
        initZeroCapacityVector.set(0, "".getBytes(StandardCharsets.UTF_8));
        Assert.assertEquals(initZeroCapacityVector.getCapacityInBytes(), INIT_CAPACITY_IN_BYTES);
        initZeroCapacityVector.set(0, baseStr.getBytes(StandardCharsets.UTF_8));
        Assert.assertEquals(initZeroCapacityVector.getCapacityInBytes(), INIT_CAPACITY_IN_BYTES);
        Assert.assertEquals(new String(initZeroCapacityVector.get(0)), baseStr);
        initZeroCapacityVector.close();

        varcharVec.close();
        vector1.close();
    }

    @Test
    public void testAppendExpandCapacity() {
        int rowCount = 5;
        VarcharVec src1 = new VarcharVec(rowCount);
        VarcharVec src2 = new VarcharVec(rowCount);

        for (int i = 0; i < rowCount; i++) {
            src1.set(i, String.valueOf(i + 1).getBytes(StandardCharsets.UTF_8));
            src2.set(i, String.valueOf(i + 6).getBytes(StandardCharsets.UTF_8));
        }

        VarcharVec appended = new VarcharVec(10);
        appended.append(src1, 0, rowCount);
        appended.append(src2, 5, rowCount);

        int expectedExpandCapacity = 20;
        Assert.assertEquals(appended.getCapacityInBytes(), INIT_CAPACITY_IN_BYTES);

        for (int i = 0; i < 10; i++) {
            Assert.assertEquals(getString(appended.get(i)), String.valueOf(i + 1));
        }

        src1.close();
        src2.close();
        appended.close();
    }

    @Test
    public void testNullFlagWithSet() {
        // no null value
        VarcharVec noNull = new VarcharVec(10);
        assertFalse(noNull.hasNull());
        noNull.close();

        // has null value
        VarcharVec hasNulls = new VarcharVec(10);
        byte[] nulls = new byte[]{0, 1, 0, 1, 0, 1, 0, 1, 0, 1};
        hasNulls.setNulls(0, nulls, 0, nulls.length);
        assertTrue(hasNulls.hasNull());
        hasNulls.close();

        VarcharVec hasNull = new VarcharVec(10);
        for (int i = 0; i < hasNull.size; i++) {
            if (i % 2 == 0) {
                hasNull.setNull(i);
            } else {
                hasNull.set(i, String.valueOf(i).getBytes(StandardCharsets.UTF_8));
            }
        }
        assertTrue(hasNull.hasNull());
        hasNull.close();
    }

    @Test
    public void testNullFlagWithCopyPosition() {
        // has null value
        VarcharVec hasNulls = new VarcharVec(10);
        byte[] nulls = new byte[]{0, 0, 1, 1, 0, 1, 0, 1, 0, 1};
        hasNulls.setNulls(0, nulls, 0, nulls.length);
        for (int i = 0; i < 10; i++) {
            if (nulls[i] == 0) {
                hasNulls.set(i, "".getBytes(StandardCharsets.UTF_8));
            }
        }
        assertTrue(hasNulls.hasNull());

        int[] positions = new int[]{0, 1};
        VarcharVec copyPositionNoNull = hasNulls.copyPositions(positions, 0, 2);
        assertFalse(copyPositionNoNull.hasNull());
        copyPositionNoNull.close();

        positions = new int[]{1, 2, 3, 4};
        VarcharVec copyPositionHasNull = hasNulls.copyPositions(positions, 0, 4);
        assertTrue(copyPositionHasNull.hasNull());
        copyPositionHasNull.close();

        hasNulls.close();
    }

    @Test
    public void testNullFlagWithSlice() {
        // has null value
        VarcharVec hasNulls = new VarcharVec(10);
        byte[] nulls = new byte[]{0, 0, 1, 1, 0, 1, 0, 1, 0, 1};
        hasNulls.setNulls(0, nulls, 0, nulls.length);
        assertTrue(hasNulls.hasNull());

        VarcharVec sliceNoNull = hasNulls.slice(0, 1);
        assertFalse(sliceNoNull.hasNull());
        sliceNoNull.close();

        VarcharVec sliceHasNull = hasNulls.slice(1, 4);
        assertTrue(sliceHasNull.hasNull());
        sliceHasNull.close();

        hasNulls.close();
    }

    @Test
    public void testNullFlagWithAppend() {
        int rowCount = 5;
        VarcharVec src = new VarcharVec(rowCount);

        for (int i = 0; i < rowCount; i++) {
            src.set(i, String.valueOf(i + 1).getBytes(StandardCharsets.UTF_8));
        }

        VarcharVec appended = new VarcharVec(15);
        appended.append(src, 0, rowCount);
        src.close();
        assertFalse(appended.hasNull());

        VarcharVec withNull = new VarcharVec(rowCount);
        byte[] nulls = new byte[]{0, 1, 1, 0, 1};
        withNull.setNulls(0, nulls, 0, 5);
        int[] offsets = new int[]{0, 2, 2, 2, 3, 3};
        withNull.put(0, "abe".getBytes(StandardCharsets.UTF_8), 0, offsets, 0, rowCount);
        appended.append(withNull, 5, rowCount);
        assertTrue(appended.hasNull());

        appended.append(withNull, 10, rowCount);
        assertTrue(appended.hasNull());
        withNull.close();

        appended.close();
    }

    private String getString(byte[] strInBytes) {
        return new String(strInBytes, StandardCharsets.UTF_8);
    }
}
