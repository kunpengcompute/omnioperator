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

package nova.hetu.omniruntime.util;

import static nova.hetu.omniruntime.type.DataType.DataTypeId.OMNI_DATE32;
import static nova.hetu.omniruntime.type.DataType.DataTypeId.OMNI_DECIMAL128;
import static nova.hetu.omniruntime.type.DataType.DataTypeId.OMNI_DOUBLE;
import static nova.hetu.omniruntime.type.DataType.DataTypeId.OMNI_INT;
import static nova.hetu.omniruntime.vector.VecEncoding.OMNI_VEC_ENCODING_DICTIONARY;
import static org.testng.Assert.assertEquals;
import static org.testng.Assert.assertFalse;
import static org.testng.Assert.assertTrue;

import com.alibaba.fastjson.JSONArray;
import com.alibaba.fastjson.JSONObject;

import nova.hetu.omniruntime.operator.OmniOperator;
import nova.hetu.omniruntime.operator.filter.OmniFilterAndProjectOperatorFactory;
import nova.hetu.omniruntime.type.DataType;
import nova.hetu.omniruntime.type.VarcharDataType;
import nova.hetu.omniruntime.vector.BooleanVec;
import nova.hetu.omniruntime.vector.Decimal128Vec;
import nova.hetu.omniruntime.vector.DictionaryVec;
import nova.hetu.omniruntime.vector.DoubleVec;
import nova.hetu.omniruntime.vector.IntVec;
import nova.hetu.omniruntime.vector.LongVec;
import nova.hetu.omniruntime.vector.ShortVec;
import nova.hetu.omniruntime.vector.VarcharVec;
import nova.hetu.omniruntime.vector.Vec;
import nova.hetu.omniruntime.vector.VecBatch;
import nova.hetu.omniruntime.vector.VecEncoding;

import org.testng.AssertJUnit;

import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.Iterator;
import java.util.List;
import java.util.Locale;

/**
 * Test utils for data generate
 *
 * @since 2021-8-10
 */
public class TestUtils {
    /**
     * Create data for blank vec batch
     *
     * @param types dataType
     * @return VecBatch
     */
    public static VecBatch createBlankVecBatch(DataType[] types) {
        Object[] data = {};
        Vec[] vecs = new Vec[types.length];
        for (int i = 0; i < types.length; i++) {
            vecs[i] = createVec(types[i], data);
        }
        return new VecBatch(vecs, 0);
    }

    /**
     * Create vec batch data
     *
     * @param types dataType
     * @param data data
     * @return VecBatch
     */
    public static VecBatch createVecBatch(DataType[] types, Object[][] data) {
        Vec[] vecs = new Vec[types.length];
        for (int i = 0; i < types.length; i++) {
            vecs[i] = createVec(types[i], data[i]);
        }
        return new VecBatch(vecs);
    }

    /**
     * Create vec batch data
     *
     * @param types dataType
     * @param list dataList
     * @return VecBatch
     */
    public static VecBatch createVecBatch(DataType[] types, List<List<Object>> list) {
        Vec[] vecs = new Vec[types.length];
        for (int i = 0; i < types.length; i++) {
            vecs[i] = createVec(types[i], list.get(i).toArray());
        }
        return new VecBatch(vecs);
    }

    /**
     * Create vec
     *
     * @param type dataType
     * @param data data
     * @return Vec
     */
    public static Vec createVec(DataType type, Object[] data) {
        switch (type.getId()) {
            case OMNI_INT:
            case OMNI_DATE32:
                return createIntVec(data);
            case OMNI_SHORT:
                return createShortVec(data);
            case OMNI_LONG:
            case OMNI_TIMESTAMP:
            case OMNI_DECIMAL64:
                return createLongVec(data);
            case OMNI_DOUBLE:
                return createDoubleVec(data);
            case OMNI_BOOLEAN:
                return createBooleanVec(data);
            case OMNI_VARCHAR:
            case OMNI_CHAR:
                return createVarcharVec((VarcharDataType) type, data);
            default:
                throw new UnsupportedOperationException("Unsupported type : " + type.getId());
        }
    }

    /**
     * Create vec for decimal128
     *
     * @param type dataType
     * @param data data
     * @return Vec
     */
    public static Vec createVec(DataType type, Object[][] data) {
        switch (type.getId()) {
            case OMNI_DECIMAL128:
                return createDecimal128Vec(data);
            default:
                throw new UnsupportedOperationException("Unsupported type : " + type.getId());
        }
    }

    /**
     * Create short vec
     *
     * @param data data
     * @return ShortVec
     */
    public static ShortVec createShortVec(Object[] data) {
        ShortVec result = new ShortVec(data.length);
        for (int j = 0; j < data.length; j++) {
            if (data[j] == null) {
                result.setNull(j);
            } else {
                result.set(j, (short) data[j]);
            }
        }
        return result;
    }

    /**
     * Create int vec
     *
     * @param data data
     * @return IntVec
     */
    public static IntVec createIntVec(Object[] data) {
        IntVec result = new IntVec(data.length);
        for (int j = 0; j < data.length; j++) {
            if (data[j] == null) {
                result.setNull(j);
            } else {
                result.set(j, (int) data[j]);
            }
        }
        return result;
    }

    /**
     * Create long vec
     *
     * @param data data
     * @return LongVec
     */
    public static LongVec createLongVec(Object[] data) {
        LongVec result = new LongVec(data.length);
        for (int j = 0; j < data.length; j++) {
            if (data[j] == null) {
                result.setNull(j);
            } else {
                result.set(j, (long) data[j]);
            }
        }
        return result;
    }

    /**
     * Create Double vec
     *
     * @param data data
     * @return DoubleVec
     */
    public static DoubleVec createDoubleVec(Object[] data) {
        DoubleVec result = new DoubleVec(data.length);
        for (int j = 0; j < data.length; j++) {
            if (data[j] == null) {
                result.setNull(j);
            } else {
                result.set(j, (double) data[j]);
            }
        }
        return result;
    }

    /**
     * Create Boolean vec
     *
     * @param data data
     * @return BooleanVec
     */
    public static BooleanVec createBooleanVec(Object[] data) {
        BooleanVec result = new BooleanVec(data.length);
        for (int i = 0; i < data.length; i++) {
            if (data[i] == null) {
                result.setNull(i);
            } else {
                result.set(i, (boolean) data[i]);
            }
        }
        return result;
    }

    /**
     * Create Varchar vec
     *
     * @param varcharVecType varchar vec type
     * @param data data
     * @return VarcharVec
     */
    public static VarcharVec createVarcharVec(VarcharDataType varcharVecType, Object[] data) {
        VarcharVec result = new VarcharVec(data.length);
        for (int j = 0; j < data.length; j++) {
            if (data[j] == null) {
                result.setNull(j);
            } else {
                result.set(j, ((String) data[j]).getBytes(StandardCharsets.UTF_8));
            }
        }
        return result;
    }

    /**
     * Create Decimal128 vec
     *
     * @param data data
     * @return Decimal128Vec
     */
    public static Decimal128Vec createDecimal128Vec(Object[][] data) {
        Decimal128Vec result = new Decimal128Vec(data.length);
        for (int i = 0; i < data.length; i++) {
            if (data[i] == null) {
                result.setNull(i);
            } else {
                result.set(i, new long[]{(long) data[i][0], (long) data[i][1]});
            }
        }
        return result;
    }

    /**
     * Create Dictionary vec
     *
     * @param dataType dataType
     * @param data input data
     * @param ids id array
     * @return DictionaryVec
     */
    public static DictionaryVec createDictionaryVec(DataType dataType, Object[] data, int[] ids) {
        Vec dictionary = createVec(dataType, data);
        DictionaryVec dictionaryVec = new DictionaryVec(dictionary, ids);
        dictionary.close();
        return dictionaryVec;
    }

    /**
     * Vec Batch equals
     *
     * @param vecBatch vecBatch
     * @param expectedDatas data
     */
    public static void assertVecBatchEqualsIgnoreOrder(VecBatch vecBatch, Object[][] expectedDatas) {
        int vectorCount = vecBatch.getVectorCount();
        assertEquals(vectorCount, expectedDatas.length);

        Vec[] vecs = vecBatch.getVectors();
        for (int i = 0; i < vectorCount; i++) {
            Vec vec = vecs[i];
            assertEquals(vec.getSize(), expectedDatas[i].length);
            VecEncoding vecEncoding = vec.getEncoding();
            if (vecEncoding.equals(OMNI_VEC_ENCODING_DICTIONARY)) {
                assertTrue(assertDictionaryVecEqualsIgnoreOrder((DictionaryVec) vec, expectedDatas[i]));
                continue;
            }
            assertTrue(assertVecEqualsIgnoreOrder(vec, expectedDatas[i]));
        }
    }

    /**
     * Vec Batch equals
     *
     * @param vecBatch vecBatch
     * @param expectedDatas data
     */
    public static void assertVecBatchEquals(VecBatch vecBatch, Object[][] expectedDatas) {
        int vectorCount = vecBatch.getVectorCount();
        assertEquals(vectorCount, expectedDatas.length);

        Vec[] vecs = vecBatch.getVectors();
        for (int i = 0; i < vectorCount; i++) {
            Vec vec = vecs[i];
            assertEquals(vec.getSize(), expectedDatas[i].length);
            VecEncoding vecEncoding = vec.getEncoding();
            if (vecEncoding.equals(OMNI_VEC_ENCODING_DICTIONARY)) {
                assertDictionaryVecEquals((DictionaryVec) vec, expectedDatas[i]);
                continue;
            }
            assertVecEquals(vec, expectedDatas[i]);
        }
    }

    /**
     * Vec Batch equals
     *
     * @param vecBatch vecBatch
     * @param expectedVecBatch expectedVecBatch
     */
    public static void assertVecBatchEqualsIgnoreOrder(VecBatch vecBatch, VecBatch expectedVecBatch) {
        int vectorCount = vecBatch.getVectorCount();
        assertEquals(vectorCount, expectedVecBatch.getVectorCount());
        Vec[] vecs = vecBatch.getVectors();
        Vec[] expectedVecs = expectedVecBatch.getVectors();
        for (int i = 0; i < vectorCount; i++) {
            Vec vec = vecs[i];
            Vec expectedVec = expectedVecs[i];
            assertEquals(vec.getSize(), expectedVec.getSize());
            VecEncoding vecEncoding = vec.getEncoding();
            if (vecEncoding.equals(OMNI_VEC_ENCODING_DICTIONARY)) {
                assertTrue(assertDictionaryVecEqualsIgnoreOrder((DictionaryVec) vec, (DictionaryVec) expectedVec));
                continue;
            }
            assertTrue(assertVecEqualsIgnoreOrder(vec, expectedVec));
        }
    }

    /**
     * Vec Batch equals
     *
     * @param vecBatch vecBatch
     * @param expectedVecBatch expectedVecBatch
     */
    public static void assertVecBatchEquals(VecBatch vecBatch, VecBatch expectedVecBatch) {
        int vectorCount = vecBatch.getVectorCount();
        assertEquals(vectorCount, expectedVecBatch.getVectorCount());
        Vec[] vecs = vecBatch.getVectors();
        Vec[] expectedVecs = expectedVecBatch.getVectors();
        for (int i = 0; i < vectorCount; i++) {
            Vec vec = vecs[i];
            Vec expectedVec = expectedVecs[i];
            assertEquals(vec.getSize(), expectedVec.getSize());
            VecEncoding vecEncoding = vec.getEncoding();
            if (vecEncoding.equals(OMNI_VEC_ENCODING_DICTIONARY)) {
                assertDictionaryVecEquals((DictionaryVec) vec, (DictionaryVec) expectedVec);
                continue;
            }
            assertVecEquals(vec, expectedVec);
        }
    }

    /**
     * Vec equals
     *
     * @param vec vec
     * @param expectedData data
     * @return boolean
     */
    public static boolean assertDecimal128VecEqualsIgnoreOrder(Vec vec, Long[][] expectedData) {
        int rowCount = vec.getSize();
        int resNUllCount = 0;
        int expNullCount = 0;
        for (int i = 0; i < rowCount; i++) {
            if (vec.isNull(i)) {
                resNUllCount++;
            }
            if (expectedData[i] == null) {
                expNullCount++;
            }
        }
        if (resNUllCount != expNullCount) {
            return false;
        }
        long[][] resArr = new long[rowCount - resNUllCount][2];
        long[][] expArr = new long[rowCount - resNUllCount][2];
        for (int i = 0, j = 0, k = 0; i < rowCount; i++) {
            if (!vec.isNull(i)) {
                resArr[j++] = ((Decimal128Vec) vec).get(i);
            }
            if (expectedData[i] != null) {
                expArr[k][0] = (long) expectedData[i][0];
                expArr[k][1] = (long) expectedData[i][1];
                k++;
            }
        }
        Arrays.sort(resArr, new Comparator<long[]>() {
            @Override
            public int compare(long[] arr1, long[] arr2) {
                if (arr1[0] == arr2[0]) {
                    return Long.compare(arr1[1], arr2[1]);
                }
                return Long.compare(arr1[0], arr2[0]);
            }
        });
        Arrays.sort(expArr, new Comparator<long[]>() {
            @Override
            public int compare(long[] arr1, long[] arr2) {
                if (arr1[0] == arr2[0]) {
                    return Long.compare(arr1[1], arr2[1]);
                }
                return Long.compare(arr1[0], arr2[0]);
            }
        });
        for (int i = 0; i < resArr.length; i++) {
            if (resArr[i][0] != expArr[i][0] || resArr[i][1] != expArr[i][1]) {
                return false;
            }
        }
        return true;
    }

    /**
     * Vec equals
     *
     * @param vec vec
     * @param expectedData data
     * @return boolean
     */
    public static boolean assertVecEqualsIgnoreOrder(Vec vec, Object[] expectedData) {
        int rowCount = vec.getSize();
        int resNUllCount = 0;
        int expNullCount = 0;
        List<Object> resList = new ArrayList<>();
        List<Object> expList = new ArrayList<>();
        for (int i = 0; i < rowCount; i++) {
            if (vec.isNull(i)) {
                resNUllCount++;
            }
            if (expectedData[i] == null) {
                expNullCount++;
            } else {
                expList.add(expectedData[i]);
            }
        }

        if (resNUllCount != expNullCount) {
            return false;
        }
        switch (vec.getType().getId()) {
            case OMNI_INT:
            case OMNI_DATE32:
                for (int i = 0; i < rowCount; i++) {
                    if (!vec.isNull(i)) {
                        resList.add(((IntVec) vec).get(i));
                    }
                }
                break;
            case OMNI_SHORT:
                for (int i = 0; i < rowCount; i++) {
                    if (!vec.isNull(i)) {
                        resList.add(((ShortVec) vec).get(i));
                    }
                }
                break;
            case OMNI_LONG:
            case OMNI_TIMESTAMP:
            case OMNI_DECIMAL64:
                for (int i = 0; i < rowCount; i++) {
                    if (!vec.isNull(i)) {
                        resList.add(((LongVec) vec).get(i));
                    }
                }
                break;
            case OMNI_DOUBLE:
                for (int i = 0; i < rowCount; i++) {
                    if (!vec.isNull(i)) {
                        resList.add(((DoubleVec) vec).get(i));
                    }
                }
                break;
            case OMNI_BOOLEAN:
                for (int i = 0; i < rowCount; i++) {
                    if (!vec.isNull(i)) {
                        resList.add(((BooleanVec) vec).get(i));
                    }
                }
                break;
            case OMNI_VARCHAR:
            case OMNI_CHAR:
                for (int i = 0; i < rowCount; i++) {
                    if (!vec.isNull(i)) {
                        resList.add(new String(((VarcharVec) vec).get(i)));
                    }
                }
                break;
            default:
                throw new UnsupportedOperationException("Unsupported type : " + vec.getType().getId());
        }
        Object[] resArr = resList.toArray();
        Object[] expArr = expList.toArray();
        Arrays.sort(resArr);
        Arrays.sort(expArr);
        if (vec.getType().getId() == OMNI_DOUBLE) {
            for (int i = 0; i < resArr.length; i++) {
                if (Double.compare((double) resArr[i], (double) expArr[i]) != 0) {
                    return false;
                }
            }
        } else {
            for (int i = 0; i < resArr.length; i++) {
                if (!resArr[i].equals(expArr[i])) {
                    return false;
                }
            }
        }
        return true;
    }

    /**
     * Vec equals
     *
     * @param vec vec
     * @param expectedData data
     */
    public static void assertVecEquals(Vec vec, Object[] expectedData) {
        for (int i = 0; i < vec.getSize(); i++) {
            if (vec.isNull(i)) {
                assertEquals(null, expectedData[i]);
                continue;
            }
            switch (vec.getType().getId()) {
                case OMNI_INT:
                case OMNI_DATE32:
                    assertEquals(((IntVec) vec).get(i), expectedData[i]);
                    break;
                case OMNI_SHORT:
                    assertEquals(((ShortVec) vec).get(i), expectedData[i]);
                    break;
                case OMNI_LONG:
                case OMNI_TIMESTAMP:
                case OMNI_DECIMAL64:
                    assertEquals(((LongVec) vec).get(i), expectedData[i]);
                    break;
                case OMNI_DOUBLE:
                    assertTrue(Double.compare(((DoubleVec) vec).get(i), (Double) expectedData[i]) == 0);
                    break;
                case OMNI_BOOLEAN:
                    assertEquals(((BooleanVec) vec).get(i), expectedData[i]);
                    break;
                case OMNI_VARCHAR:
                case OMNI_CHAR:
                    assertEquals(new String(((VarcharVec) vec).get(i)), expectedData[i]);
                    break;
                default:
                    throw new UnsupportedOperationException("Unsupported type : " + vec.getType().getId());
            }
        }
    }

    /**
     * Vec equals
     *
     * @param vec vec
     * @param expectedVec expectedVec
     * @return boolean
     */
    public static boolean assertDecimal128VecEqualsIgnoreOrder(Vec vec, Vec expectedVec) {
        int rowCount = vec.getSize();
        int resNUllCount = 0;
        int expNullCount = 0;
        for (int i = 0; i < rowCount; i++) {
            if (vec.isNull(i)) {
                resNUllCount++;
            }
            if (expectedVec.isNull(i)) {
                expNullCount++;
            }
        }
        if (resNUllCount != expNullCount) {
            return false;
        }
        long[][] resArr = new long[rowCount - resNUllCount][2];
        long[][] expArr = new long[rowCount - resNUllCount][2];
        for (int i = 0, j = 0, k = 0; i < rowCount; i++) {
            if (!vec.isNull(i)) {
                resArr[j++] = ((Decimal128Vec) vec).get(i);
            }
            if (expectedVec.isNull(i)) {
                expArr[k++] = ((Decimal128Vec) expectedVec).get(i);
            }
        }
        Arrays.sort(resArr, new Comparator<long[]>() {
            @Override
            public int compare(long[] arr1, long[] arr2) {
                if (arr1[0] == arr2[0]) {
                    return Long.compare(arr1[1], arr2[1]);
                }
                return Long.compare(arr1[0], arr2[0]);
            }
        });
        Arrays.sort(expArr, new Comparator<long[]>() {
            @Override
            public int compare(long[] arr1, long[] arr2) {
                if (arr1[0] == arr2[0]) {
                    return Long.compare(arr1[1], arr2[1]);
                }
                return Long.compare(arr1[0], arr2[0]);
            }
        });
        for (int i = 0; i < resArr.length; i++) {
            if (resArr[i][0] != expArr[i][0] || resArr[i][1] != expArr[i][1]) {
                return false;
            }
        }
        return true;
    }

    /**
     * Vec equals
     *
     * @param vec vec
     * @param expectedVec expectedVec
     * @return boolean
     */
    public static boolean assertVecEqualsIgnoreOrder(Vec vec, Vec expectedVec) {
        int rowCount = vec.getSize();
        int resNUllCount = 0;
        int expNullCount = 0;
        List<Object> resList = new ArrayList<>();
        List<Object> expList = new ArrayList<>();
        for (int i = 0; i < rowCount; i++) {
            if (vec.isNull(i)) {
                resNUllCount++;
            }
            if (expectedVec.isNull(i)) {
                expNullCount++;
            }
        }

        if (resNUllCount != expNullCount) {
            return false;
        }
        switch (vec.getType().getId()) {
            case OMNI_INT:
            case OMNI_DATE32:
                for (int i = 0; i < rowCount; i++) {
                    if (!vec.isNull(i)) {
                        resList.add(((IntVec) vec).get(i));
                    }
                    if (!expectedVec.isNull(i)) {
                        expList.add(((IntVec) expectedVec).get(i));
                    }
                }
                break;
            case OMNI_SHORT:
                for (int i = 0; i < rowCount; i++) {
                    if (!vec.isNull(i)) {
                        resList.add(((ShortVec) vec).get(i));
                    }
                    if (!expectedVec.isNull(i)) {
                        expList.add(((ShortVec) expectedVec).get(i));
                    }
                }
                break;
            case OMNI_LONG:
            case OMNI_TIMESTAMP:
            case OMNI_DECIMAL64:
                for (int i = 0; i < rowCount; i++) {
                    if (!vec.isNull(i)) {
                        resList.add(((LongVec) vec).get(i));
                    }
                    if (!expectedVec.isNull(i)) {
                        expList.add(((LongVec) expectedVec).get(i));
                    }
                }
                break;
            case OMNI_DOUBLE:
                for (int i = 0; i < rowCount; i++) {
                    if (!vec.isNull(i)) {
                        resList.add(((DoubleVec) vec).get(i));
                    }
                    if (!expectedVec.isNull(i)) {
                        expList.add(((DoubleVec) expectedVec).get(i));
                    }
                }
                break;
            case OMNI_BOOLEAN:
                for (int i = 0; i < rowCount; i++) {
                    if (!vec.isNull(i)) {
                        resList.add(((BooleanVec) vec).get(i));
                    }
                    if (!expectedVec.isNull(i)) {
                        expList.add(((BooleanVec) expectedVec).get(i));
                    }
                }
                break;
            case OMNI_VARCHAR:
            case OMNI_CHAR:
                for (int i = 0; i < rowCount; i++) {
                    if (!vec.isNull(i)) {
                        resList.add(new String(((VarcharVec) vec).get(i)));
                    }
                    if (!expectedVec.isNull(i)) {
                        expList.add(new String(((VarcharVec) expectedVec).get(i)));
                    }
                }
                break;
            default:
                throw new UnsupportedOperationException("Unsupported type : " + vec.getType().getId());
        }
        Object[] resArr = resList.toArray();
        Object[] expArr = expList.toArray();
        Arrays.sort(resArr);
        Arrays.sort(expArr);
        if (vec.getType().getId() == OMNI_DOUBLE) {
            for (int i = 0; i < resArr.length; i++) {
                if (Double.compare((double) resArr[i], (double) expArr[i]) != 0) {
                    return false;
                }
            }
        } else {
            for (int i = 0; i < resArr.length; i++) {
                if (!resArr[i].equals(expArr[i])) {
                    return false;
                }
            }
        }
        return true;
    }

    /**
     * Vec equals
     *
     * @param vec vec
     * @param expectedVec expectedVec
     */
    public static void assertVecEquals(Vec vec, Vec expectedVec) {
        for (int i = 0; i < vec.getSize(); i++) {
            if (vec.isNull(i) && expectedVec.isNull(i)) {
                continue;
            }
            switch (vec.getType().getId()) {
                case OMNI_INT:
                case OMNI_DATE32:
                    assertEquals(((IntVec) vec).get(i), ((IntVec) expectedVec).get(i));
                    break;
                case OMNI_SHORT:
                    assertEquals(((ShortVec) vec).get(i), ((ShortVec) expectedVec).get(i));
                    break;
                case OMNI_LONG:
                case OMNI_TIMESTAMP:
                case OMNI_DECIMAL64:
                    assertEquals(((LongVec) vec).get(i), ((LongVec) expectedVec).get(i));
                    break;
                case OMNI_DOUBLE:
                    assertTrue(Double.compare(((DoubleVec) vec).get(i), ((DoubleVec) expectedVec).get(i)) == 0);
                    break;
                case OMNI_BOOLEAN:
                    assertEquals(((BooleanVec) vec).get(i), ((BooleanVec) expectedVec).get(i));
                    break;
                case OMNI_VARCHAR:
                case OMNI_CHAR:
                    assertEquals(new String(((VarcharVec) vec).get(i)), new String(((VarcharVec) expectedVec).get(i)));
                    break;
                case OMNI_DECIMAL128:
                    assertEquals(((Decimal128Vec) vec).get(i), ((Decimal128Vec) expectedVec).get(i));
                    break;
                default:
                    throw new UnsupportedOperationException("Unsupported type : " + vec.getType().getId());
            }
        }
    }

    /**
     * Vec equals
     *
     * @param vec vec
     * @param expectedData data
     */
    public static void assertVecEquals(Vec vec, Object[][] expectedData) {
        for (int i = 0; i < vec.getSize(); i++) {
            if (vec.isNull(i)) {
                assertEquals(null, expectedData[i]);
                continue;
            }
            switch (vec.getType().getId()) {
                case OMNI_DECIMAL128:
                    assertEquals(((Decimal128Vec) vec).get(i),
                            new long[]{(long) expectedData[i][0], (long) expectedData[i][1]});
                    break;
                default:
                    throw new UnsupportedOperationException("Unsupported type : " + vec.getType().getId());
            }
        }
    }

    /**
     * Dictionary vec equals
     *
     * @param vec dictionary vec
     * @param expectedData data
     * @return boolean
     */
    public static boolean assertDecimal128DictionaryVecEqualsIgnoreOrder(DictionaryVec vec, Long[][] expectedData) {
        int rowCount = vec.getSize();
        int resNUllCount = 0;
        int expNullCount = 0;
        for (int i = 0; i < rowCount; i++) {
            if (vec.isNull(i)) {
                resNUllCount++;
            }
            if (expectedData[i] == null) {
                expNullCount++;
            }
        }
        if (resNUllCount != expNullCount) {
            return false;
        }
        long[][] resArr = new long[rowCount - resNUllCount][2];
        long[][] expArr = new long[rowCount - resNUllCount][2];
        for (int i = 0, j = 0, k = 0; i < rowCount; i++) {
            if (!vec.isNull(i)) {
                resArr[j++] = vec.getDecimal128(i);
            }
            if (expectedData[i] != null) {
                expArr[k][0] = (long) expectedData[i][0];
                expArr[k][1] = (long) expectedData[i][1];
                k++;
            }
        }
        Arrays.sort(resArr, new Comparator<long[]>() {
            @Override
            public int compare(long[] arr1, long[] arr2) {
                if (arr1[0] == arr2[0]) {
                    return Long.compare(arr1[1], arr2[1]);
                }
                return Long.compare(arr1[0], arr2[0]);
            }
        });
        Arrays.sort(expArr, new Comparator<long[]>() {
            @Override
            public int compare(long[] arr1, long[] arr2) {
                if (arr1[0] == arr2[0]) {
                    return Long.compare(arr1[1], arr2[1]);
                }
                return Long.compare(arr1[0], arr2[0]);
            }
        });
        for (int i = 0; i < resArr.length; i++) {
            if (resArr[i][0] != expArr[i][0] || resArr[i][1] != expArr[i][1]) {
                return false;
            }
        }
        return true;
    }

    /**
     * Dictionary vec equals
     *
     * @param vec dictionary vec
     * @param expectedData data
     * @return boolean
     */
    public static boolean assertDictionaryVecEqualsIgnoreOrder(DictionaryVec vec, Object[] expectedData) {
        int rowCount = vec.getSize();
        int resNUllCount = 0;
        int expNullCount = 0;
        List<Object> resList = new ArrayList<>();
        List<Object> expList = new ArrayList<>();
        for (int i = 0; i < rowCount; i++) {
            if (vec.isNull(i)) {
                resNUllCount++;
            }
            if (expectedData[i] == null) {
                expNullCount++;
            } else {
                expList.add(expectedData[i]);
            }
        }

        if (resNUllCount != expNullCount) {
            return false;
        }
        switch (vec.getType().getId()) {
            case OMNI_INT:
            case OMNI_DATE32:
                for (int i = 0; i < rowCount; i++) {
                    if (!vec.isNull(i)) {
                        resList.add(vec.getInt(i));
                    }
                }
                break;
            case OMNI_SHORT:
                for (int i = 0; i < rowCount; i++) {
                    if (!vec.isNull(i)) {
                        resList.add(vec.getShort(i));
                    }
                }
                break;
            case OMNI_LONG:
            case OMNI_TIMESTAMP:
            case OMNI_DECIMAL64:
                for (int i = 0; i < rowCount; i++) {
                    if (!vec.isNull(i)) {
                        resList.add(vec.getLong(i));
                    }
                }
                break;
            case OMNI_DOUBLE:
                for (int i = 0; i < rowCount; i++) {
                    if (!vec.isNull(i)) {
                        resList.add(vec.getDouble(i));
                    }
                }
                break;
            case OMNI_BOOLEAN:
                for (int i = 0; i < rowCount; i++) {
                    if (!vec.isNull(i)) {
                        resList.add(vec.getBoolean(i));
                    }
                }
                break;
            case OMNI_VARCHAR:
            case OMNI_CHAR:
                for (int i = 0; i < rowCount; i++) {
                    if (!vec.isNull(i)) {
                        resList.add(new String(vec.getBytes(i)));
                    }
                }
                break;
            default:
                throw new UnsupportedOperationException("Unsupported type : " + vec.getType().getId());
        }
        Object[] resArr = resList.toArray();
        Object[] expArr = expList.toArray();
        Arrays.sort(resArr);
        Arrays.sort(expArr);
        if (vec.getType().getId() == OMNI_DOUBLE) {
            for (int i = 0; i < resArr.length; i++) {
                if (Double.compare((double) resArr[i], (double) expArr[i]) != 0) {
                    return false;
                }
            }
        } else {
            for (int i = 0; i < resArr.length; i++) {
                if (!resArr[i].equals(expArr[i])) {
                    return false;
                }
            }
        }

        return true;
    }

    /**
     * Dictionary vec equals
     *
     * @param vec dictionary vec
     * @param expectedData data
     */
    public static void assertDictionaryVecEquals(DictionaryVec vec, Object[] expectedData) {
        DataType.DataTypeId typeId = vec.getType().getId();

        for (int i = 0; i < vec.getSize(); i++) {
            if (vec.isNull(i)) {
                assertEquals(null, expectedData[i]);
                continue;
            }
            switch (typeId) {
                case OMNI_INT:
                case OMNI_DATE32:
                    assertEquals(vec.getInt(i), expectedData[i]);
                    break;
                case OMNI_SHORT:
                    assertEquals(vec.getShort(i), expectedData[i]);
                    break;
                case OMNI_LONG:
                case OMNI_TIMESTAMP:
                case OMNI_DECIMAL64:
                    assertEquals(vec.getLong(i), expectedData[i]);
                    break;
                case OMNI_BOOLEAN:
                    assertEquals(vec.getBoolean(i), expectedData[i]);
                    break;
                case OMNI_DOUBLE:
                    assertEquals(Double.compare(vec.getDouble(i), (Double) expectedData[i]), 0);
                    break;
                case OMNI_VARCHAR:
                case OMNI_CHAR:
                    assertEquals(vec.getBytes(i), ((String) (expectedData[i])).getBytes(StandardCharsets.UTF_8));
                    break;
                case OMNI_DECIMAL128:
                    assertEquals(vec.getDecimal128(i),
                            new long[]{(long) expectedData[i * 2], (long) expectedData[i * 2 + 1]});
                    break;
                default:
                    throw new UnsupportedOperationException("Unsupported type : " + typeId);
            }
        }
    }

    /**
     * Dictionary vec equals
     *
     * @param vec dictionary vec
     * @param expectedVec expectedVec
     * @return boolean
     */
    public static boolean assertDecimal128DictionaryVecEqualsIgnoreOrder(DictionaryVec vec,
            DictionaryVec expectedVec) {
        int rowCount = vec.getSize();
        int resNUllCount = 0;
        int expNullCount = 0;
        for (int i = 0; i < rowCount; i++) {
            if (vec.isNull(i)) {
                resNUllCount++;
            }
            if (expectedVec.isNull(i)) {
                expNullCount++;
            }
        }
        if (resNUllCount != expNullCount) {
            return false;
        }
        long[][] resArr = new long[rowCount - resNUllCount][2];
        long[][] expArr = new long[rowCount - resNUllCount][2];
        for (int i = 0, j = 0, k = 0; i < rowCount; i++) {
            if (!vec.isNull(i)) {
                resArr[j++] = vec.getDecimal128(i);
            }
            if (expectedVec.isNull(i)) {
                expArr[k++] = expectedVec.getDecimal128(i);
            }
        }
        Arrays.sort(resArr, new Comparator<long[]>() {
            @Override
            public int compare(long[] arr1, long[] arr2) {
                if (arr1[0] == arr2[0]) {
                    return Long.compare(arr1[1], arr2[1]);
                }
                return Long.compare(arr1[0], arr2[0]);
            }
        });
        Arrays.sort(expArr, new Comparator<long[]>() {
            @Override
            public int compare(long[] arr1, long[] arr2) {
                if (arr1[0] == arr2[0]) {
                    return Long.compare(arr1[1], arr2[1]);
                }
                return Long.compare(arr1[0], arr2[0]);
            }
        });
        for (int i = 0; i < resArr.length; i++) {
            if (resArr[i][0] != expArr[i][0] || resArr[i][1] != expArr[i][1]) {
                return false;
            }
        }
        return true;
    }

    /**
     * Dictionary vec equals
     *
     * @param vec dictionary vec
     * @param expectedVec expectedVec
     * @return boolean
     */
    public static boolean assertDictionaryVecEqualsIgnoreOrder(DictionaryVec vec, DictionaryVec expectedVec) {
        int rowCount = vec.getSize();
        int resNUllCount = 0;
        int expNullCount = 0;
        List<Object> resList = new ArrayList<>();
        List<Object> expList = new ArrayList<>();
        for (int i = 0; i < rowCount; i++) {
            if (vec.isNull(i)) {
                resNUllCount++;
            }
            if (expectedVec.isNull(i)) {
                expNullCount++;
            }
        }

        if (resNUllCount != expNullCount) {
            return false;
        }
        switch (vec.getType().getId()) {
            case OMNI_INT:
            case OMNI_DATE32:
                for (int i = 0; i < rowCount; i++) {
                    if (!vec.isNull(i)) {
                        resList.add(vec.getInt(i));
                    }
                    if (!expectedVec.isNull(i)) {
                        expList.add(expectedVec.getInt(i));
                    }
                }
                break;
            case OMNI_SHORT:
                for (int i = 0; i < rowCount; i++) {
                    if (!vec.isNull(i)) {
                        resList.add(vec.getShort(i));
                    }
                    if (!expectedVec.isNull(i)) {
                        expList.add(expectedVec.getShort(i));
                    }
                }
                break;
            case OMNI_LONG:
            case OMNI_TIMESTAMP:
            case OMNI_DECIMAL64:
                for (int i = 0; i < rowCount; i++) {
                    if (!vec.isNull(i)) {
                        resList.add(vec.getLong(i));
                    }
                    if (!expectedVec.isNull(i)) {
                        expList.add(expectedVec.getLong(i));
                    }
                }
                break;
            case OMNI_DOUBLE:
                for (int i = 0; i < rowCount; i++) {
                    if (!vec.isNull(i)) {
                        resList.add(vec.getDouble(i));
                    }
                    if (!expectedVec.isNull(i)) {
                        expList.add(expectedVec.getDouble(i));
                    }
                }
                break;
            case OMNI_BOOLEAN:
                for (int i = 0; i < rowCount; i++) {
                    if (!vec.isNull(i)) {
                        resList.add(vec.getBoolean(i));
                    }
                    if (!expectedVec.isNull(i)) {
                        expList.add(expectedVec.getBoolean(i));
                    }
                }
                break;
            case OMNI_VARCHAR:
            case OMNI_CHAR:
                for (int i = 0; i < rowCount; i++) {
                    if (!vec.isNull(i)) {
                        resList.add(new String(vec.getBytes(i)));
                    }
                    if (!expectedVec.isNull(i)) {
                        expList.add(new String(expectedVec.getBytes(i)));
                    }
                }
                break;
            case OMNI_DECIMAL128:
                for (int i = 0; i < rowCount; i++) {
                    if (!vec.isNull(i)) {
                        resList.add(vec.getDecimal128(i));
                    }
                    if (!expectedVec.isNull(i)) {
                        expList.add(expectedVec.getDecimal128(i));
                    }
                }
                break;
            default:
                throw new UnsupportedOperationException("Unsupported type : " + vec.getType().getId());
        }
        Object[] resArr = resList.toArray();
        Object[] expArr = expList.toArray();
        Arrays.sort(resArr);
        Arrays.sort(expArr);
        if (vec.getType().getId() == OMNI_DOUBLE) {
            for (int i = 0; i < resArr.length; i++) {
                if (Double.compare((double) resArr[i], (double) expArr[i]) != 0) {
                    return false;
                }
            }
        } else {
            for (int i = 0; i < resArr.length; i++) {
                if (!resArr[i].equals(expArr[i])) {
                    return false;
                }
            }
        }
        return true;
    }

    /**
     * Dictionary vec equals
     *
     * @param vec dictionary vec
     * @param expectedVec expectedVec
     */
    public static void assertDictionaryVecEquals(DictionaryVec vec, DictionaryVec expectedVec) {
        DataType.DataTypeId typeId = vec.getType().getId();

        for (int i = 0; i < vec.getSize(); i++) {
            if (vec.isNull(i) && expectedVec.isNull(i)) {
                continue;
            }
            switch (typeId) {
                case OMNI_INT:
                case OMNI_DATE32:
                    assertEquals(vec.getInt(i), expectedVec.getInt(i));
                    break;
                case OMNI_SHORT:
                    assertEquals(vec.getShort(i), expectedVec.getShort(i));
                    break;
                case OMNI_LONG:
                case OMNI_TIMESTAMP:
                case OMNI_DECIMAL64:
                    assertEquals(vec.getLong(i), expectedVec.getLong(i));
                    break;
                case OMNI_BOOLEAN:
                    assertEquals(vec.getBoolean(i), expectedVec.getBoolean(i));
                    break;
                case OMNI_DOUBLE:
                    assertEquals(Double.compare(vec.getDouble(i), expectedVec.getDouble(i)), 0);
                    break;
                case OMNI_VARCHAR:
                case OMNI_CHAR:
                    assertEquals(vec.getBytes(i), expectedVec.getBytes(i));
                    break;
                case OMNI_DECIMAL128:
                    assertEquals(vec.getDecimal128(i), expectedVec.getDecimal128(i));
                    break;
                default:
                    throw new UnsupportedOperationException("Unsupported type : " + typeId);
            }
        }
    }

    /**
     * free the vecBatches.
     *
     * @param vecBatches vecBatch list.
     */
    public static void freeVecBatches(List<VecBatch> vecBatches) {
        for (int i = 0; i < vecBatches.size(); i++) {
            freeVecBatch(vecBatches.get(i));
        }
    }

    /**
     * Vec batch free
     *
     * @param vecBatch vecBatch
     */
    public static void freeVecBatch(VecBatch vecBatch) {
        vecBatch.releaseAllVectors();
        vecBatch.close();
    }

    /**
     * match filter operator with json.
     *
     * @param inputData input data
     * @param types the type array of input data
     * @param projectIndices the list of project indices
     * @param resultKeepRowIdxs the row ids of result
     * @param filterExpression filter expression
     * @param parFormat the format to parse expression
     */
    public static void filterOperatorMatchWithJson(Object[][] inputData, DataType[] types, List<String> projectIndices,
            int[] resultKeepRowIdxs, String filterExpression, int parFormat) {
        VecBatch vecBatch = createVecBatch(types, inputData);
        OmniFilterAndProjectOperatorFactory factory = new OmniFilterAndProjectOperatorFactory(filterExpression, types,
                projectIndices, parFormat);
        OmniOperator op = factory.createOperator();
        op.addInput(vecBatch);
        Iterator<VecBatch> results = op.getOutput();

        if (resultKeepRowIdxs.length == 0) {
            assertFalse(results.hasNext());
            return;
        }

        AssertJUnit.assertTrue(results.hasNext());

        List<Integer> keepedColumns = new ArrayList<>();
        for (int resultKeepRowIdx : resultKeepRowIdxs) {
            keepedColumns.add(resultKeepRowIdx);
        }
        Object[][] expectedDatas = new Object[inputData.length][resultKeepRowIdxs.length];
        for (int i = 0; i < inputData.length; i++) {
            for (int j = 0, m = 0; j < inputData[0].length && m < resultKeepRowIdxs.length; j++) {
                if (keepedColumns.contains(j)) {
                    expectedDatas[i][m] = inputData[i][j];
                    m++;
                }
            }
        }

        VecBatch resultVecBatch = results.next();
        assertVecBatchEquals(resultVecBatch, expectedDatas);
        op.close();
        factory.close();
        freeVecBatch(resultVecBatch);
    }

    /**
     * match filter operator.
     *
     * @param inputData input data
     * @param types the type array of input data
     * @param projectIndices the list of project indices
     * @param resultKeepRowIdxs the row ids of result
     * @param filterExpression filter expression
     */
    public static void filterOperatorMatch(Object[][] inputData, DataType[] types, List<String> projectIndices,
            int[] resultKeepRowIdxs, String filterExpression) {
        filterOperatorMatchWithJson(inputData, types, projectIndices, resultKeepRowIdxs, filterExpression, 0);
    }

    /**
     * generating "is not null" json expression.
     *
     * @param arguments arguments
     * @return the formatted "is not null" json expression
     */
    public static String omniJsonIsNotNullExpr(String arguments) {
        return String.format(Locale.ROOT, "{\"exprType\":\"UNARY\",\"returnType\":4,\"operator\":\"not\","
                + "\"expr\":{\"exprType\":\"IS_NULL\",\"returnType\":4," + "\"arguments\":[%s]}}", arguments);
    }

    /**
     * generating "is null" json expression.
     *
     * @param arguments arguments
     * @return the formatted "is null" json expression
     */
    public static String omniJsonIsNullExpr(String arguments) {
        return String.format(Locale.ROOT, "{\"exprType\":\"IS_NULL\",\"returnType\":4,\"arguments\":[%s]}", arguments);
    }

    /**
     * generating "less than or equal" json expression.
     *
     * @param left the left argument
     * @param right the right argument
     * @return the formatted "less than or equal" json expression
     */
    public static String omniJsonLessThanOrEqualExpr(String left, String right) {
        return String.format(Locale.ROOT, "{\"exprType\":\"BINARY\",\"returnType\":4,"
                + "\"operator\":\"LESS_THAN_OR_EQUAL\",\"left\":%s,\"right\":%s}", left, right);
    }

    /**
     * generating "equal" json expression.
     *
     * @param left the left argument
     * @param right the right argument
     * @return the formatted "equal" json expression
     */
    public static String omniJsonEqualExpr(String left, String right) {
        return String.format(Locale.ROOT,
                "{\"exprType\":\"BINARY\",\"returnType\":4,\"operator\":\"EQUAL\",\"left\":%s,\"right\":%s}", left,
                right);
    }

    /**
     * generating "not equal" json expression.
     *
     * @param left the left argument
     * @param right the right argument
     * @param <T> the generic parameter
     * @return the formatted "not equal" json expression
     */
    public static <T> String omniJsonNotEqualExpr(String left, String right) {
        return String.format(Locale.ROOT,
                "{\"exprType\":\"BINARY\",\"returnType\":4,\"operator\":\"NOT_EQUAL\",\"left\":%s,\"right\":%s}", left,
                right);
    }

    /**
     * generating "greater than or equal" json expression.
     *
     * @param left the left argument
     * @param right the right argument
     * @return the formatted "greater than or equal" json expression
     */
    public static String omniJsonGreaterThanOrEqualExpr(String left, String right) {
        return String.format(Locale.ROOT, "{\"exprType\":\"BINARY\",\"returnType\":4,\"operator\":"
                + "\"GREATER_THAN_OR_EQUAL\",\"left\":%s,\"right\":%s}", left, right);
    }

    /**
     * generating "greater than" json expression.
     *
     * @param left the left argument
     * @param right the right argument
     * @return the formatted "greater than" json expression
     */
    public static String omniJsonGreaterThanExpr(String left, String right) {
        return String.format(Locale.ROOT, "{\"exprType\":\"BINARY\",\"returnType\":4,\"operator\":\"GREATER_THAN\","
                + "\"left\":%s,\"right\":%s}", left, right);
    }

    /**
     * generating "and" json expression.
     *
     * @param left the left argument
     * @param right the right argument
     * @return the formatted "and" json expression
     */
    public static String omniJsonAndExpr(String left, String right) {
        return String.format(Locale.ROOT,
                "{\"exprType\":\"BINARY\",\"returnType\":4,\"operator\":\"AND\",\"left\":%s,\"right\":%s}", left,
                right);
    }

    /**
     * generating "or" json expression.
     *
     * @param left the left argument
     * @param right the right argument
     * @return the formatted "or" json expression
     */
    public static String omniJsonOrExpr(String left, String right) {
        return String.format(Locale.ROOT,
                "{\"exprType\":\"BINARY\",\"returnType\":4,\"operator\":\"OR\",\"left\":%s,\"right\":%s}", left, right);
    }

    /**
     * generating "if" json expression.
     *
     * @param condition the condition
     * @param returnType the type of result
     * @param trueOpr the operator of true
     * @param falseOpr the operator of false
     * @return the formatted "if" json expression
     */
    public static String omniJsonIfExpr(String condition, int returnType, String trueOpr, String falseOpr) {
        return String.format(Locale.ROOT,
                "{\"exprType\":\"IF\",\"returnType\":%d,\"condition\":%s,\"if_true\":%s, \"if_false\":%s}", returnType,
                condition, trueOpr, falseOpr);
    }

    /**
     * get the formatted field reference.
     *
     * @param dataType the type of data
     * @param colVal the column
     * @return the formatted field reference
     */
    public static String getOmniJsonFieldReference(int dataType, int colVal) {
        if (dataType == 15) {
            return String.format(Locale.ROOT,
                    "{\"exprType\": \"FIELD_REFERENCE\",\"dataType\": %d,\"colVal\": %d,\"width\":50}", dataType,
                    colVal);
        }
        return String.format(Locale.ROOT, "{\"exprType\": \"FIELD_REFERENCE\",\"dataType\": %d,\"colVal\": %d}",
                dataType, colVal);
    }

    /**
     * get the formatted literal.
     *
     * @param dataType the type of data
     * @param isNull whether is null
     * @param value the value
     * @param <T> the generic parameter
     * @return the formatted literal
     */
    public static <T> String getOmniJsonLiteral(int dataType, boolean isNull, T value) {
        if (value instanceof String) {
            return String.format(Locale.ROOT,
                    "{\"exprType\":\"LITERAL\",\"dataType\":%d,\"isNull\":%b,\"value\":\"%s\",\"width\":50}", dataType,
                    isNull, value);
        }
        return String.format(Locale.ROOT, "{\"exprType\":\"LITERAL\",\"dataType\":%d,\"isNull\":%b,", dataType, isNull)
                + "\"value\":" + value + "}";
    }

    /**
     * generating "arithmetic" json expression.
     *
     * @param opr the operator
     * @param returnType the type of result
     * @param left the left argument
     * @param right the right argument
     * @return the formatted "arithmetic" json expression
     */
    public static String omniJsonFourArithmeticExpr(String opr, int returnType, String left, String right) {
        return String.format(Locale.ROOT,
                "{\"exprType\":\"BINARY\",\"returnType\":%d,\"operator\":\"%s\",\"left\":%s,\"right\":%s}", returnType,
                opr, left, right);
    }

    /**
     * generating "in" json expression.
     *
     * @param argDataType the type of data
     * @param colVal the column
     * @param values the list of value
     * @param <T> the generic parameter
     * @return the formatted "in" json expression
     */
    public static <T> String omniJsonInExpr(int argDataType, int colVal, List<T> values) {
        JSONArray jsonArray = new JSONArray();
        JSONObject jsonObject = new JSONObject();
        jsonObject.put("exprType", "FIELD_REFERENCE");
        jsonObject.put("dataType", argDataType);
        jsonObject.put("colVal", colVal);
        if (argDataType == 15) {
            jsonObject.put("width", 50);
        }
        jsonArray.add(jsonObject);
        for (T value : values) {
            JSONObject object = new JSONObject();
            object.put("exprType", "LITERAL");
            object.put("dataType", argDataType);
            object.put("isNull", false);
            object.put("value", value);
            if (argDataType == 15) {
                object.put("width", 50);
            }
            jsonArray.add(object);
        }
        String argString = jsonArray.toJSONString();
        return String.format(Locale.ROOT, "{\"exprType\":\"IN\",\"returnType\":4,\"arguments\":%s}", argString);
    }

    /**
     * generating "abs" json expression.
     *
     * @param dataType the type of data
     * @param arguments the arguments
     * @return the formatted "abs" json expression
     */
    public static String omniJsonAbsExpr(int dataType, String arguments) {
        return String.format(Locale.ROOT,
                "{\"exprType\":\"FUNCTION\",\"returnType\":%d,\"function_name\":\"abs\",\"arguments\":[%s]}", dataType,
                arguments);
    }

    /**
     * generating "cast" json expression.
     *
     * @param returnType the type of result
     * @param arguments the arguments
     * @return the formatted "cast" json expression
     */
    public static String omniJsonCastExpr(int returnType, String arguments) {
        if (returnType == 15) {
            return String.format(Locale.ROOT, "{\"exprType\":\"FUNCTION\",\"returnType\":%d,\"width\":50,"
                    + "\"function_name\":\"CAST\",\"arguments\":[%s]}", returnType, arguments);
        }
        return String.format(Locale.ROOT,
                "{\"exprType\":\"FUNCTION\",\"returnType\":%d,\"function_name\":\"CAST\",\"arguments\":[%s]}",
                returnType, arguments);
    }

    /**
     * generating general function json expression.
     *
     * @param function the name of function
     * @param returnType the type of result
     * @param arguments the arguments
     * @return the formatted "cast" json expression
     */
    public static String omniFunctionExpr(String function, int returnType, String arguments) {
        if (returnType == 15) {
            return String.format(Locale.ROOT, "{\"exprType\":\"FUNCTION\",\"returnType\":%d,\"width\":50,"
                    + "\"function_name\":\"%s\",\"arguments\":[%s]}", returnType, function, arguments);
        }
        return String.format(Locale.ROOT,
                "{\"exprType\":\"FUNCTION\",\"returnType\":%d,\"function_name\":\"%s\",\"arguments\":[%s]}", returnType,
                function, arguments);
    }

    /**
     * get the corresponding addFlag enumerated value in resultCode.
     *
     * @param resultCode the resultCode
     * @return the corresponding addFlag enumerated value in resultCode
     */
    public static int decodeAddFlag(int resultCode) {
        return resultCode >> 16;
    }

    /**
     * get the corresponding fetchFlag enumerated value in resultCode.
     *
     * @param resultCode the resultCode
     * @return the corresponding fetchFlag enumerated value in resultCode
     */
    public static int decodeFetchFlag(int resultCode) {
        return resultCode & Short.MAX_VALUE;
    }
}
