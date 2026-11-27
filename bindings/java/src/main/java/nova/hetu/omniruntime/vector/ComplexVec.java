/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

package nova.hetu.omniruntime.vector;

import static nova.hetu.omniruntime.utils.OmniErrorType.OMNI_PARAM_ERROR;

import nova.hetu.omniruntime.type.DataType;
import nova.hetu.omniruntime.type.MapDataType;
import nova.hetu.omniruntime.type.StructDataType;
import nova.hetu.omniruntime.type.ArrayDataType;
import nova.hetu.omniruntime.utils.OmniRuntimeException;

/**
 * base class of complex vec.
 *
 * @since 2025-08-29
 */
public abstract class ComplexVec extends Vec {

    public ComplexVec(long nativeVector, int capacityInBytes, int size, DataType dataType) {
        super(nativeVector, 0, getValueNullsNative(nativeVector), capacityInBytes, size,
            dataType);
    }

    public ComplexVec(Vec vec, int offset, int length, int capacityInBytes) {
        super(vec, offset, length, capacityInBytes);
    }

    public ComplexVec(long nativeVector, long nativeValueBufAddress, long nativeVectorNullBufAddress,
              int capacityInBytes, int size, DataType dataType) {
        super(nativeVector, nativeValueBufAddress, nativeVectorNullBufAddress, capacityInBytes, size,
            dataType);
    }

    public static DataType getComplexDataType(long nativeVector) {
        return getComplexDataTypeNative(nativeVector);
    }

    protected static native int getComplexCapacityNative(long nativeVector, int vecEncodingId);

    protected static native long newComplexVectorNative(int size, int vecEncodingId, DataType[] dataType, long nmmHandle);

    protected static native long newEmptyComplexVectorNative(int size, int vecEncodingId, DataType[] dataType,
            long nmmHandle);

    protected static native DataType getComplexDataTypeNative(long nativeVector);

    public static Vec createVec(long nativeVector, DataType dataType){
        switch (dataType.getId()) {
            case OMNI_BOOLEAN: {
                return new BooleanVec(nativeVector);
            }
            case OMNI_SHORT: {
                return new ShortVec(nativeVector);
            }
            case OMNI_BYTE: {
                return new ByteVec(nativeVector);
            }
            case OMNI_DATE32:
            case OMNI_INT: {
                return new IntVec(nativeVector);
            }
            case OMNI_LONG:
            case OMNI_TIMESTAMP:
            case OMNI_DECIMAL64: {
                return new LongVec(nativeVector);
            }
            case OMNI_FLOAT: {
                return new FloatVec(nativeVector);
            }
            case OMNI_DOUBLE: {
                return new DoubleVec(nativeVector);
            }
            case OMNI_VARBINARY:
            case OMNI_CHAR:
            case OMNI_VARCHAR: {
                if (getVecEncodingNative(nativeVector) == VecEncoding.OMNI_VEC_ENCODING_DICTIONARY.ordinal()) {
                    int sz = getSizeNative(nativeVector);
                    return new DictionaryVec(nativeVector, getValuesNative(nativeVector),
                        getValueNullsNative(nativeVector), sz, dataType);
                }
                return new VarcharVec(nativeVector);
            }
            case OMNI_DECIMAL128: {
                return new Decimal128Vec(nativeVector);
            }
            case OMNI_ARRAY: {
                return new ArrayVec(nativeVector, (ArrayDataType) dataType);
            }
            case OMNI_MAP: {
                return new MapVec(nativeVector, (MapDataType) dataType);
            }
            case OMNI_ROW: {
                return new StructVec(nativeVector, (StructDataType) dataType);
            }
            default: {
                throw new OmniRuntimeException(OMNI_PARAM_ERROR, "UnSupported type :" + dataType.getId());
            }
        }
    }
}



