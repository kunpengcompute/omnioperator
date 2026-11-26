/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
 * KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 */

package nova.hetu.omniruntime.vector;

import nova.hetu.omniruntime.type.DataType;
import nova.hetu.omniruntime.type.ArrayDataType;

/**
 * vec factory.
 *
 * @since 2021-08-05
 */
public class VecFactory {
    /**
     * Create vector by native vector address and data type.
     *
     * @param nativeVector native vector address
     * @param encoding vector encoding
     * @param dataType vector data type
     * @return a new {@link Vec} object instance
     */
    public static Vec create(long nativeVector, VecEncoding encoding, DataType dataType) {
        Vec vector;
        switch (encoding) {
            case OMNI_VEC_ENCODING_FLAT:
                vector = createFlatVec(nativeVector, dataType);
                break;
            case OMNI_VEC_ENCODING_DICTIONARY:
                vector = new DictionaryVec(nativeVector, dataType);
                break;
            case OMNI_VEC_ENCODING_CONTAINER:
                vector = new ContainerVec(nativeVector);
                break;
            case OMNI_ENCODING_ARRAY:
                vector = new ArrayVec(nativeVector, (ArrayDataType) dataType);
                break;
            default:
                throw new IllegalArgumentException("Not Support Vec Encoding " + encoding);
        }
        return vector;
    }

    private static Vec createFlatVec(long nativeVector, DataType dataType) {
        switch (dataType.getId()) {
            case OMNI_INT:
            case OMNI_DATE32:
                return new IntVec(nativeVector);
            case OMNI_LONG:
            case OMNI_DATE64:
            case OMNI_TIMESTAMP:
            case OMNI_DECIMAL64:
                return new LongVec(nativeVector);
            case OMNI_DOUBLE:
                return new DoubleVec(nativeVector);
            case OMNI_SHORT:
                return new ShortVec(nativeVector);
            case OMNI_BYTE:
                return new ByteVec(nativeVector);
            case OMNI_BOOLEAN:
                return new BooleanVec(nativeVector);
            case OMNI_VARCHAR:
            case OMNI_CHAR:
                return new VarcharVec(nativeVector);
            case OMNI_DECIMAL128:
                return new Decimal128Vec(nativeVector);
            default:
                throw new IllegalArgumentException("Not Support Data Type " + dataType.getId());
        }
    }

    /**
     * Create vector by native vector address and vector type.
     *
     * @param nativeVector native vector address
     * @param nativeVectorValueBufAddress native vector value buffer address
     * @param nativeVectorNullBufAddress native vector nulls buffer address
     * @param nativeVectorOffsetBufAddress native vector offset buffer address
     * @param size size of vector
     * @param encoding vector encoding type
     * @param dataType vector data type
     * @return Instance of {@link Vec}
     */
    public static Vec create(long nativeVector, long nativeVectorValueBufAddress, long nativeVectorNullBufAddress,
            long nativeVectorOffsetBufAddress, int size, VecEncoding encoding, DataType dataType) {
        Vec vector;
        switch (encoding) {
            case OMNI_VEC_ENCODING_FLAT:
                vector = createFlatVec(nativeVector, nativeVectorValueBufAddress, nativeVectorNullBufAddress,
                        nativeVectorOffsetBufAddress, size, dataType);
                break;
            case OMNI_VEC_ENCODING_DICTIONARY:
                vector = new DictionaryVec(nativeVector, nativeVectorValueBufAddress, nativeVectorNullBufAddress, size,
                        dataType);
                break;
            case OMNI_VEC_ENCODING_CONTAINER:
                vector = new ContainerVec(nativeVector, nativeVectorValueBufAddress, nativeVectorNullBufAddress, size);
                break;
            case OMNI_ENCODING_ARRAY:
                ArrayDataType arrayType = (ArrayDataType) ComplexVec.getComplexDataTypeNative(nativeVector);
                vector = new ArrayVec(nativeVector, nativeVectorValueBufAddress, nativeVectorNullBufAddress, size, arrayType);
                break;
            default:
                throw new IllegalArgumentException("Not Support Vec Encoding " + encoding);
        }
        return vector;
    }

    private static Vec createFlatVec(long nativeVector, long nativeVectorValueBufAddress,
            long nativeVectorNullBufAddress, long nativeVectorOffsetBufAddress, int size, DataType dataType) {
        switch (dataType.getId()) {
            case OMNI_INT:
            case OMNI_DATE32:
                return new IntVec(nativeVector, nativeVectorValueBufAddress, nativeVectorNullBufAddress, size);
            case OMNI_LONG:
            case OMNI_TIMESTAMP:
            case OMNI_DATE64:
            case OMNI_DECIMAL64:
                return new LongVec(nativeVector, nativeVectorValueBufAddress, nativeVectorNullBufAddress, size);
            case OMNI_DOUBLE:
                return new DoubleVec(nativeVector, nativeVectorValueBufAddress, nativeVectorNullBufAddress, size);
            case OMNI_SHORT:
                return new ShortVec(nativeVector, nativeVectorValueBufAddress, nativeVectorNullBufAddress, size);
            case OMNI_BYTE:
                return new ByteVec(nativeVector, nativeVectorValueBufAddress, nativeVectorNullBufAddress, size);
            case OMNI_BOOLEAN:
                return new BooleanVec(nativeVector, nativeVectorValueBufAddress, nativeVectorNullBufAddress, size);
            case OMNI_VARCHAR:
            case OMNI_CHAR:
                return new VarcharVec(nativeVector, nativeVectorValueBufAddress, nativeVectorNullBufAddress,
                        nativeVectorOffsetBufAddress, size);
            case OMNI_DECIMAL128:
                return new Decimal128Vec(nativeVector, nativeVectorValueBufAddress, nativeVectorNullBufAddress, size);
            default:
                throw new IllegalArgumentException("Not Support Data Type " + dataType.getId());
        }
    }

    /**
     * Create empty vector by size and vector type, only use by expandDictionaryVec.
     *
     * @param size size of vector
     * @param dataType vector data type
     * @return Instance of {@link Vec}
     */
    public static Vec createFlatVec(int size, DataType dataType) {
        switch (dataType.getId()) {
            case OMNI_INT:
            case OMNI_DATE32:
                return new IntVec(size);
            case OMNI_TIMESTAMP:
            case OMNI_LONG:
            case OMNI_DATE64:
            case OMNI_DECIMAL64:
                return new LongVec(size);
            case OMNI_DOUBLE:
                return new DoubleVec(size);
            case OMNI_SHORT:
                return new ShortVec(size);
            case OMNI_BYTE:
                return new ByteVec(size);
            case OMNI_BOOLEAN:
                return new BooleanVec(size);
            case OMNI_VARCHAR:
            case OMNI_CHAR:
                return new VarcharVec(size);
            case OMNI_DECIMAL128:
                return new Decimal128Vec(size);
            default:
                throw new IllegalArgumentException("Not Support Data Type " + dataType.getId());
        }
    }
}
