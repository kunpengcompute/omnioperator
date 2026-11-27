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

import static nova.hetu.omniruntime.util.TestUtils.assertVecBatchEquals;

import nova.hetu.omniruntime.type.DataType;
import nova.hetu.omniruntime.vector.serialize.OmniRowDeserializer;

import org.testng.annotations.Test;

import java.lang.reflect.InvocationTargetException;
import java.util.Arrays;

/**
 * test omni row vec
 *
 * @since 2024-5-16
 */
class GenericClass<TVec extends Vec> {
    private Class<TVec> clazz;

    /**
     * set special value
     *
     * @param clazz the class object of TVec
     *
     */
    public GenericClass(Class<TVec> clazz) {
        this.clazz = clazz;
    }

    /**
     * use reflect function to create vector
     *
     * @param rowCount row count of vector
     * @return TVec return vector in heap
     */
    public TVec createInstance(int rowCount) {
        try {
            return clazz.getDeclaredConstructor(int.class).newInstance(rowCount);
        } catch (InstantiationException | IllegalAccessException | NoSuchMethodException
                | InvocationTargetException e) {
            e.printStackTrace();
            return null;
        }
    }
}

/**
 * lambda express to set value into omniBuf
 */
@FunctionalInterface
interface OmniBufSet {
    /**
     * set special value
     *
     * @param buffer OmniBuffer which will be set value
     * @param index set special index row 's value
     */
    void operate(OmniBuffer buffer, int index);
}

/**
 * Function to test template vector
 */
public class TestOmniRow {
    /**
     * set special value
     *
     * @param clazz object of class
     * @param type  data type of new vector
     * @param vecCount vec count of vector batch
     * @param rowCount row count of vector batch
     * @param setFunc used to set value for every row
     */
    private <TVec extends Vec> void testRowBatchNewAndParse(Class<TVec> clazz, int type, int vecCount, int rowCount,
            OmniBufSet setFunc) {
        Vec[] vecArray = new Vec[vecCount];
        GenericClass<TVec> vecGenericClass = new GenericClass<>(clazz);

        for (int i = 0; i < vecCount; i++) {
            vecArray[i] = vecGenericClass.createInstance(rowCount);
            for (int j = 0; j < rowCount; ++j) {
                setFunc.operate(vecArray[i].getValuesBuf(), j);
            }
        }

        VecBatch expectVecBatch = new VecBatch(vecArray, rowCount);
        RowBatch rowBatch = new RowBatch(expectVecBatch);

        int[] types = new int[vecCount];
        Arrays.fill(types, type);

        VecBatch resultVb = new VecBatch(vecArray, rowCount);
        long[] vecAddresses = new long[vecCount];
        for (int i = 0; i < vecCount; i++) {
            vecAddresses[i] = resultVb.getVector(i).nativeVector;
        }
        OmniRowDeserializer deserializer = new OmniRowDeserializer(types, vecAddresses);

        deserializer.parseAll(rowBatch.getNativeRowBatch());

        assertVecBatchEquals(resultVb, expectVecBatch);
        deserializer.close();
        rowBatch.close();
        expectVecBatch.close();
        resultVb.close();
    }

    /**
     * test row batch, only value is positive
     */
    @Test
    public void testRowBatch() {
        testRowBatchNewAndParse(BooleanVec.class, DataType.DataTypeId.OMNI_BOOLEAN.toValue(), 10, 1024,
                (omniBuf, i) -> omniBuf.setByte(i, (i % 2 == 0) ? (byte) 1 : (byte) 0));

        testRowBatchNewAndParse(ShortVec.class, DataType.DataTypeId.OMNI_SHORT.toValue(), 10, 1024,
                (omniBuf, i) -> omniBuf.setShort(i, (short) (i + 1)));

        testRowBatchNewAndParse(IntVec.class, DataType.DataTypeId.OMNI_INT.toValue(), 10, 1024,
                (omniBuf, i) -> omniBuf.setInt(i, i));

        testRowBatchNewAndParse(LongVec.class, DataType.DataTypeId.OMNI_LONG.toValue(), 10, 1024,
                (omniBuf, i) -> omniBuf.setLong(i, i));

        testRowBatchNewAndParse(DoubleVec.class, DataType.DataTypeId.OMNI_DOUBLE.toValue(), 10, 1024,
                (omniBuf, i) -> omniBuf.setDouble(i, i));

        testRowBatchNewAndParse(Decimal128Vec.class, DataType.DataTypeId.OMNI_DECIMAL128.toValue(), 10, 1024,
                (omniBuf, i) -> {
                    omniBuf.setLong((2 * i), i);
                    omniBuf.setLong((2 * i) + 1, i);
                });

        testRowBatchNewAndParse(LongVec.class, DataType.DataTypeId.OMNI_DECIMAL64.toValue(), 10, 1024, (omniBuf, i) -> {
            omniBuf.setLong((2 * i), i);
            omniBuf.setLong((2 * i) + 1, i);
        });
    }

    /**
     * test row batch, all value is negative
     */
    @Test
    public void testNegativeRowBatch() {
        // negative short
        testRowBatchNewAndParse(ShortVec.class, DataType.DataTypeId.OMNI_SHORT.toValue(), 10, 1024,
                (omniBuf, i) -> omniBuf.setShort(i, (short) (-i)));

        // negative int
        testRowBatchNewAndParse(IntVec.class, DataType.DataTypeId.OMNI_INT.toValue(), 10, 1024,
                (omniBuf, i) -> omniBuf.setInt(i, -i));

        // negative long to test negative compress
        testRowBatchNewAndParse(LongVec.class, DataType.DataTypeId.OMNI_LONG.toValue(), 10, 1024,
                (omniBuf, i) -> omniBuf.setLong(i, -i));
    }
}
