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

import static nova.hetu.omniruntime.type.DoubleDataType.DOUBLE;
import static nova.hetu.omniruntime.type.LongDataType.LONG;
import static org.testng.Assert.assertEquals;
import static org.testng.Assert.assertTrue;

import nova.hetu.omniruntime.type.ContainerDataType;
import nova.hetu.omniruntime.type.DataType;
import nova.hetu.omniruntime.type.Decimal128DataType;
import nova.hetu.omniruntime.type.IntDataType;
import nova.hetu.omniruntime.type.LongDataType;
import nova.hetu.omniruntime.type.ShortDataType;
import nova.hetu.omniruntime.type.VarcharDataType;

import org.testng.annotations.Test;

/**
 * Container vec test: Now(2023.3.4), c++ do not support append\copyPositions\slice method
 *
 * @since 2021-7-6
 */
public class TestContainerVec {
    @Test(enabled = false)
    public void testSlice() {
        int rows = 10;
        DoubleVec field1 = new DoubleVec(rows);
        double[] data1 = new double[]{0, 1.1, 2.2, 3.3, 4.4, 5.5, 6.6, 7.7, 8.8, 9.9};
        field1.put(data1, 0, 0, rows);
        LongVec field2 = new LongVec(rows);
        long[] data2 = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        field2.put(data2, 0, 0, rows);
        ContainerVec originalVec = new ContainerVec(2, rows,
                new long[]{field1.getNativeVector(), field2.getNativeVector()}, new DataType[]{DOUBLE, LONG});

        int offset = 1;
        ContainerVec sliced = originalVec.slice(offset, 4);
        DoubleVec result1 = new DoubleVec(sliced.get(0));
        LongVec result2 = new LongVec(sliced.get(1));
        for (int i = 0; i < 5; i++) {
            assertEquals(result1.get(i), data1[offset + i]);
            assertEquals(result2.get(i), data2[offset + i]);
        }
        originalVec.close();
        sliced.close();
    }

    @Test(enabled = false)
    public void testCopyPositions() {
        int rows = 10;
        DoubleVec field1 = new DoubleVec(rows);
        double[] data1 = new double[]{0, 1.1, 2.2, 3.3, 4.4, 5.5, 6.6, 7.7, 8.8, 9.9};
        field1.put(data1, 0, 0, rows);
        LongVec field2 = new LongVec(rows);
        long[] data2 = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        field2.put(data2, 0, 0, rows);
        ContainerVec originalVec = new ContainerVec(2, rows,
                new long[]{field1.getNativeVector(), field2.getNativeVector()}, new DataType[]{DOUBLE, LONG});

        int[] positions = new int[]{1, 3, 5, 7, 9};
        ContainerVec copyPositionsed = originalVec.copyPositions(positions, 0, 5);
        DoubleVec result1 = new DoubleVec(copyPositionsed.get(0));
        LongVec result2 = new LongVec(copyPositionsed.get(1));
        for (int i = 0; i < 5; i++) {
            assertEquals(result1.get(i), data1[positions[i]]);
            assertEquals(result2.get(i), data2[positions[i]]);
        }
        originalVec.close();
        copyPositionsed.close();
    }

    @Test
    public void testAppend() {
        int rows = 5;
        DoubleVec field1 = new DoubleVec(rows);
        double[] doubles = new double[]{1.1, 2.2, 3.3, 4.4, 5.5};
        field1.put(doubles, 0, 0, rows);
        LongVec field2 = new LongVec(rows);
        long[] longs = new long[]{1, 2, 3, 4, 5};
        field2.put(longs, 0, 0, rows);

        DoubleVec field11 = new DoubleVec(rows);
        double[] doubles1 = new double[]{6.6, 7.7, 8.8, 9.9, 10.1};
        field11.put(doubles1, 0, 0, rows);
        LongVec field22 = new LongVec(rows);
        long[] longs1 = new long[]{6, 7, 8, 9, 10};
        field22.put(longs1, 0, 0, rows);
        ContainerVec originalVec1 = new ContainerVec(2, rows,
                new long[]{field11.getNativeVector(), field22.getNativeVector()}, new DataType[]{DOUBLE, LONG});

        DoubleVec appendedDouble = new DoubleVec(rows * 2);
        LongVec appendedLong = new LongVec(rows * 2);
        ContainerVec appended = new ContainerVec(2, rows * 2,
                new long[]{appendedDouble.getNativeVector(), appendedLong.getNativeVector()},
                new DataType[]{DOUBLE, LONG});

        ContainerVec originalVec = new ContainerVec(2, rows,
                new long[]{field1.getNativeVector(), field2.getNativeVector()}, new DataType[]{DOUBLE, LONG});
        appended.append(originalVec, 0, 5);
        appended.append(originalVec1, 5, 5);

        double[] expected1 = new double[]{1.1, 2.2, 3.3, 4.4, 5.5, 6.6, 7.7, 8.8, 9.9, 10.1};
        DoubleVec result1 = new DoubleVec(appended.get(0));
        for (int i = 0; i < result1.getSize(); i++) {
            assertEquals(result1.get(i), expected1[i]);
        }

        long[] expected2 = new long[]{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        LongVec result2 = new LongVec(appended.get(1));
        for (int i = 0; i < result2.getSize(); i++) {
            assertEquals(result2.get(i), expected2[i]);
        }

        originalVec.close();
        originalVec1.close();
        appended.close();
    }

    @Test
    public void TestContainerVecSerialize() {
        int rows = 3;
        ShortVec shortVec = new ShortVec(rows);
        IntVec intVec = new IntVec(rows);
        LongVec longVec = new LongVec(rows);
        Decimal128Vec decimal128Vec = new Decimal128Vec(rows);
        VarcharVec varcharVec = new VarcharVec(10);

        long[] subAddr = new long[]{shortVec.getNativeVector(), intVec.getNativeVector(), longVec.getNativeVector()};
        DataType[] subTypes = new DataType[]{ShortDataType.SHORT, IntDataType.INTEGER, LongDataType.LONG};
        ContainerVec subContainerVec = new ContainerVec(3, rows, subAddr, subTypes);

        long[] addr = new long[]{decimal128Vec.getNativeVector(), varcharVec.getNativeVector(),
                subContainerVec.getNativeVector()};
        DataType[] dataTypes = new DataType[]{Decimal128DataType.DECIMAL128, VarcharDataType.VARCHAR,
                new ContainerDataType(new DataType[]{ShortDataType.SHORT, IntDataType.INTEGER, LongDataType.LONG})};

        ContainerVec vec = new ContainerVec(dataTypes.length, rows, addr, dataTypes);
        ContainerVec vecFromNative = new ContainerVec(vec.getNativeVector());
        DataType[] dataTypesFromNative = vecFromNative.getDataTypes();

        for (int i = 0; i < dataTypes.length; i++) {
            assertEquals(dataTypes[i], dataTypesFromNative[i]);
        }

        vec.close();
    }

    @Test
    public void testNullFlagWithSet() {
        int rows = 10;
        IntVec sub1 = new IntVec(rows);
        LongVec sub2 = new LongVec(rows);

        long[] subAddrs = new long[]{sub1.slice(0, rows).getNativeVector(), sub2.slice(0, rows).getNativeVector()};
        DataType[] subTypes = new DataType[]{IntDataType.INTEGER, LONG};
        ContainerVec hasNulls = new ContainerVec(2, rows, subAddrs, subTypes);
        byte[] nulls = new byte[]{1, 0, 1, 0, 1, 0, 1, 0, 1, 0};
        hasNulls.setNulls(0, nulls, 0, rows);
        assertTrue(hasNulls.hasNull());
        hasNulls.close();

        subAddrs = new long[]{sub1.getNativeVector(), sub2.getNativeVector()};
        ContainerVec hasNull = new ContainerVec(2, rows, subAddrs, subTypes);
        for (int i = 0; i < rows; i++) {
            if (i % 2 == 0) {
                hasNull.setNull(i);
            }
        }
        assertTrue(hasNull.hasNull());
        hasNull.close();
    }
}
