/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2022. All rights reserved.
 */

package nova.hetu.omniruntime.vector.serialize;

import com.google.protobuf.ByteString;
import com.google.protobuf.InvalidProtocolBufferException;

import nova.hetu.omniruntime.type.CharDataType;
import nova.hetu.omniruntime.type.DataType;
import nova.hetu.omniruntime.type.Date32DataType;
import nova.hetu.omniruntime.type.Date64DataType;
import nova.hetu.omniruntime.type.DecimalDataType;
import nova.hetu.omniruntime.type.VarcharDataType;
import nova.hetu.omniruntime.type.VarBinaryDataType;
import nova.hetu.omniruntime.type.ArrayDataType;
import nova.hetu.omniruntime.type.StructDataType;
import nova.hetu.omniruntime.type.MapDataType;
import nova.hetu.omniruntime.utils.OmniErrorType;
import nova.hetu.omniruntime.utils.OmniRuntimeException;
import nova.hetu.omniruntime.vector.BooleanVec;
import nova.hetu.omniruntime.vector.ConstVec;
import nova.hetu.omniruntime.vector.ContainerVec;
import nova.hetu.omniruntime.vector.Decimal128Vec;
import nova.hetu.omniruntime.vector.DictionaryVec;
import nova.hetu.omniruntime.vector.DoubleVec;
import nova.hetu.omniruntime.vector.IntVec;
import nova.hetu.omniruntime.vector.JvmUtils;
import nova.hetu.omniruntime.vector.LongVec;
import nova.hetu.omniruntime.vector.OmniBuffer;
import nova.hetu.omniruntime.vector.OmniBufferFactory;
import nova.hetu.omniruntime.vector.ShortVec;
import nova.hetu.omniruntime.vector.ByteVec;
import nova.hetu.omniruntime.vector.VarcharVec;
import nova.hetu.omniruntime.vector.StringViewVec;
import nova.hetu.omniruntime.vector.FloatVec;
import nova.hetu.omniruntime.vector.ArrayVec;
import nova.hetu.omniruntime.vector.StructVec;
import nova.hetu.omniruntime.vector.MapVec;
import nova.hetu.omniruntime.vector.Vec;
import nova.hetu.omniruntime.vector.VecBatch;
import nova.hetu.omniruntime.vector.VecEncoding;
import nova.hetu.omniruntime.vector.VecFactory;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * VecBatchSerializer implementation of protobuf.
 *
 * @since 2021-09-13
 */
public class ProtoVecBatchSerializer implements VecBatchSerializer {
    @Override
    public byte[] serialize(VecBatch vecBatch) {
        VecBatchSerde.VecBatch.Builder vecBatchBuilder = VecBatchSerde.VecBatch.newBuilder();

        // set vectors
        int index = 0;
        for (Vec vector : vecBatch.getVectors()) {
            VecBatchSerde.Vec vec = buildProtoVec(vector, null);
            vecBatchBuilder.addVectors(index, vec);
            index++;
        }

        return vecBatchBuilder.setRowCount(vecBatch.getRowCount()).setVecCount(vecBatch.getVectorCount()).build()
                .toByteArray();
    }

    // Copies a StringView vector's string content into a fresh VARCHAR vector so it can be serialized
    // through the existing VARCHAR proto path (the VecBatchSerde proto schema has no StringView type
    // id). Caller closes the returned vector.
    private static VarcharVec stringViewToVarchar(StringViewVec sv) {
        int rows = sv.getSize();
        VarcharVec varcharVec = new VarcharVec(rows);
        for (int i = 0; i < rows; i++) {
            if (sv.isNull(i)) {
                varcharVec.setNull(i);
            } else {
                varcharVec.set(i, sv.get(i));
            }
        }
        return varcharVec;
    }

    private VecBatchSerde.Vec buildProtoVec(Vec vec, int[] ids) {
        // StringView (OMNI_STRING_VIEW, id 26) has no proto DataTypeId. Serialize its content as a
        // VARCHAR proto vec (offset+data): the native broadcast/shuffle consumer reads the build
        // column by its runtime vector type, so a VARCHAR payload carrying a StringView-declared
        // substrait type round-trips correctly.
        if (vec instanceof StringViewVec) {
            VarcharVec asVarchar = stringViewToVarchar((StringViewVec) vec);
            VecBatchSerde.Vec protoVec = buildProtoVec(asVarchar, ids);
            asVarchar.close();
            return protoVec;
        }
        VecBatchSerde.Vec.Builder protoVecBuilder = VecBatchSerde.Vec.newBuilder();
        VecBatchSerde.DataTypeExt.Builder protoDataTypeExtBuild = VecBatchSerde.DataTypeExt.newBuilder();
        VecBatchSerde.VecEncoding.Builder protoVecEncodingBuild = VecBatchSerde.VecEncoding.newBuilder();
        DataType dataType = vec.getType();
        VecEncoding encoding = vec.getEncoding();
        protoDataTypeExtBuild.setId(VecBatchSerde.DataTypeExt.DataTypeId.valueOf(dataType.getId().name()));
        protoVecEncodingBuild.setEncodingId(encoding.ordinal());
        switch (encoding) {
            case OMNI_VEC_ENCODING_FLAT:
                setProtoDataTypeExt(protoDataTypeExtBuild, dataType);
                break;
            case OMNI_VEC_ENCODING_DICTIONARY: {
                DictionaryVec dictionaryVec = (DictionaryVec) vec;
                Vec dictionary = dictionaryVec.expandDictionary();
                VecBatchSerde.Vec protoVec = buildProtoVec(dictionary, null);
                dictionary.close();
                return protoVec;
            }
            case OMNI_VEC_ENCODING_CONST: {
                ConstVec constVec = (ConstVec) vec;
                Vec flatVec = constVec.expandToFlat();
                VecBatchSerde.Vec protoVec = buildProtoVec(flatVec, null);
                flatVec.close();
                return protoVec;
            }
            case OMNI_VEC_ENCODING_CONTAINER: {
                ContainerVec containerVec = (ContainerVec) vec;
                int vecCount = containerVec.getDataTypes().length;
                DataType[] subVecTypes = containerVec.getDataTypes();
                for (int i = 0; i < vecCount; i++) {
                    Vec subVec = VecFactory.create(containerVec.getVector(i), containerVec.getVecEncoding(i),
                            subVecTypes[i]);
                    VecBatchSerde.Vec subProtoVec = buildProtoVec(subVec, null);
                    protoVecBuilder.addSubVectors(subProtoVec);
                }
                break;
            }
            case OMNI_ENCODING_ARRAY: {
                ArrayVec arrayVec = (ArrayVec) vec;
                Vec elementVec = arrayVec.getElementVec();
                VecBatchSerde.Vec elementProtoVec = buildProtoVec(elementVec, null);
                protoDataTypeExtBuild.addChildren(elementProtoVec.getTypeExt());
                protoVecBuilder.addSubVectors(elementProtoVec);
                break;
            }
            case OMNI_ENCODING_STRUCT: {
                StructVec structVec = (StructVec) vec;
                for (int i = 0; i < structVec.getChildren().length; i++) {
                    Vec childVec = structVec.getChild(i);
                    VecBatchSerde.Vec childProtoVec = buildProtoVec(childVec, null);
                    protoVecBuilder.addSubVectors(childProtoVec);
                    protoDataTypeExtBuild.addChildren(childProtoVec.getTypeExt());
                }
                break;
            }
            case OMNI_ENCODING_MAP: {
                MapVec mapVec = (MapVec) vec;
                Vec keyVec = mapVec.getKeyVec();
                Vec valueVec = mapVec.getValueVec();
                VecBatchSerde.Vec protoKeyVec = buildProtoVec(keyVec, null);
                VecBatchSerde.Vec protoValueVec = buildProtoVec(valueVec, null);
                protoDataTypeExtBuild.addChildren(protoKeyVec.getTypeExt());
                protoDataTypeExtBuild.addChildren(protoValueVec.getTypeExt());
                protoVecBuilder.addSubVectors(protoKeyVec);
                protoVecBuilder.addSubVectors(protoValueVec);
                break;
            }
            default:
                throw new IllegalStateException("Unexpected encoding: " + encoding);
        }

        Vec compactVec = compactVec(vec, ids);

        ByteBuffer valueBuf;
        if (compactVec instanceof MapVec) {
            MapVec mapVec = (MapVec) compactVec;
            ByteBuffer offsetsBuf = JvmUtils.directBuffer(mapVec.getOffsetsBuf());
            offsetsBuf.limit(mapVec.getRealOffsetBufCapacityInBytes());
            protoVecBuilder.setOffsets(ByteString.copyFrom(offsetsBuf));
        } else if (compactVec instanceof ArrayVec) {
            ArrayVec arrayVec = (ArrayVec) compactVec;
            ByteBuffer offsetsBuf = JvmUtils.directBuffer(arrayVec.getOffsetsBuf());
            offsetsBuf.limit(arrayVec.getRealOffsetBufCapacityInBytes());
            protoVecBuilder.setOffsets(ByteString.copyFrom(offsetsBuf));
        } else if (compactVec instanceof VarcharVec) {
            VarcharVec varcharVec = (VarcharVec) compactVec;
            valueBuf = serializeVarcharVector(protoVecBuilder, varcharVec);
            protoVecBuilder.setValues(ByteString.copyFrom(valueBuf));
        } else {
            // For fixed vector, only serialize value.
            valueBuf = JvmUtils.directBuffer(compactVec.getValuesBuf());
            // only serialize the data actually written
            valueBuf.limit(compactVec.getRealValueBufCapacityInBytes());
            protoVecBuilder.setValues(ByteString.copyFrom(valueBuf));
        }

        ByteBuffer valueNullsBuf = JvmUtils.directBuffer(compactVec.getValueNullsBuf());
        // only serialize the actual null size
        valueNullsBuf.limit(compactVec.getRealNullBufCapacityInBytes());
        VecBatchSerde.Vec protoVec = protoVecBuilder.setTypeExt(protoDataTypeExtBuild.build())
            .setVecEncoding(protoVecEncodingBuild.build()).setSize(compactVec.getSize())
            .setNulls(ByteString.copyFrom(valueNullsBuf)).build();

        if (compactVec != vec) {
            compactVec.close();
        }
        return protoVec;
    }

    private void setProtoDataTypeExt(VecBatchSerde.DataTypeExt.Builder protoDataTypeExtBuild, DataType dataType) {
        switch (dataType.getId()) {
            case OMNI_BYTE:
            case OMNI_INT:
            case OMNI_LONG:
            case OMNI_TIMESTAMP:
            case OMNI_SHORT:
            case OMNI_BOOLEAN:
            case OMNI_DOUBLE:
            case OMNI_FLOAT:
                break;
            case OMNI_DATE32:
                protoDataTypeExtBuild.setDateUnit(
                        VecBatchSerde.DataTypeExt.DateUnit.valueOf(((Date32DataType) dataType).getDateUnit().name()));
                break;
            case OMNI_DATE64:
                protoDataTypeExtBuild.setDateUnit(
                        VecBatchSerde.DataTypeExt.DateUnit.valueOf(((Date64DataType) dataType).getDateUnit().name()));
                break;
            case OMNI_VARCHAR:
                protoDataTypeExtBuild.setWidth(((VarcharDataType) dataType).getWidth());
                break;
            case OMNI_CHAR:
                protoDataTypeExtBuild.setWidth(((CharDataType) dataType).getWidth());
                break;
            case OMNI_VARBINARY:
                protoDataTypeExtBuild.setWidth(((VarBinaryDataType) dataType).getWidth());
                break;
            case OMNI_INTERVAL_DAY_TIME:
                protoDataTypeExtBuild.setDateUnit(VecBatchSerde.DataTypeExt.DateUnit.DAY);
                break;
            case OMNI_INTERVAL_MONTHS:
                protoDataTypeExtBuild.setDateUnit(VecBatchSerde.DataTypeExt.DateUnit.MILLI);
                break;
            case OMNI_DECIMAL64:
            case OMNI_DECIMAL128: {
                if (dataType instanceof DecimalDataType) {
                    protoDataTypeExtBuild.setScale(((DecimalDataType) dataType).getScale());
                    protoDataTypeExtBuild.setPrecision(((DecimalDataType) dataType).getPrecision());
                } else {
                    throw new IllegalStateException("Unexpected data type: " + dataType.getId());
                }
                break;
            }
            case OMNI_TIME32:
            case OMNI_TIME64:
                break;
            default:
                throw new IllegalStateException("Unexpected data type: " + dataType.getId());
        }
    }

    private Vec compactVec(Vec vec, int[] ids) {
        // original vec is dictionary vec
        if (ids != null) {
            return vec.copyPositions(ids, 0, ids.length);
        }
        // original vec
        return vec;
    }

    // For String types, copying is generally costly,
    // so we try to reuse the original Vec as much as possible.
    // We need to handle the offset case separately,
    // and generate Vec based on the slice characteristics.
    private ByteBuffer serializeVarcharVector(VecBatchSerde.Vec.Builder protoVecBuilder, VarcharVec varcharVec) {
        ByteBuffer valueBuf;
        ByteBuffer offsetBuf;
        int size = varcharVec.getSize();
        int startOffset = varcharVec.getValueOffset(0);
        int realOffsetBufCapacity = varcharVec.getRealOffsetBufCapacityInBytes();
        int realValueBufCapacity = varcharVec.getRealValueBufCapacityInBytes();
        if (startOffset > 0) {
            // For sliced varchar vector, offset the value base address and offsets to
            // ensure that the serialized value is correct.
            offsetBuf = ByteBuffer.allocate(realOffsetBufCapacity).order(ByteOrder.LITTLE_ENDIAN);
            for (int i = 0; i < size + 1; i++) {
                offsetBuf.putInt(varcharVec.getValueOffset(i) - startOffset);
            }
            offsetBuf.flip();
            protoVecBuilder.setOffsets(ByteString.copyFrom(offsetBuf));

            long valueBufAddress = varcharVec.getValuesBuf().getAddress() + startOffset;
            OmniBuffer omniValueBuf = OmniBufferFactory.create(valueBufAddress, realValueBufCapacity);
            valueBuf = JvmUtils.directBuffer(omniValueBuf);
            // only serialize the data actually written
            valueBuf.limit(realValueBufCapacity);
        } else {
            // For not sliced varchar vector, serialize value and offset.
            offsetBuf = JvmUtils.directBuffer(varcharVec.getOffsetsBuf());
            // only serialize the actual offset size
            offsetBuf.limit(realOffsetBufCapacity);
            protoVecBuilder.setOffsets(ByteString.copyFrom(offsetBuf));

            valueBuf = JvmUtils.directBuffer(varcharVec.getValuesBuf());
            // only serialize the data actually written
            valueBuf.limit(realValueBufCapacity);
        }
        return valueBuf;
    }

    @Override
    public VecBatch deserialize(byte[] bytes) {
        return deserialize(bytes, 0L);
    }

    @Override
    public VecBatch deserialize(byte[] bytes, long nmmHandle) {
        VecBatchSerde.VecBatch protoVecBatch;
        try {
            protoVecBatch = VecBatchSerde.VecBatch.parseFrom(bytes);
        } catch (Exception e) {
            throw new OmniRuntimeException(OmniErrorType.OMNI_INNER_ERROR, "deserialize failed." + e.getCause());
        }
        int vecCount = protoVecBatch.getVecCount();
        int rowCount = protoVecBatch.getRowCount();
        Vec[] vecs = new Vec[vecCount];
        try {
            for (int i = 0; i < vecCount; i++) {
                vecs[i] = buildVec(protoVecBatch.getVectors(i), nmmHandle);
            }
        } catch (RuntimeException e) {
            for (Vec v : vecs) {
                if (v == null) {
                    continue;
                }
                try {
                    v.close();
                } catch (RuntimeException cleanupFailure) {
                    e.addSuppressed(cleanupFailure);
                }
            }
            throw e;
        }

        return new VecBatch(vecs, rowCount);
    }

    private static void closeAfterFailure(Vec vector, RuntimeException failure) {
        if (vector == null) {
            return;
        }
        try {
            vector.close();
        } catch (RuntimeException cleanupFailure) {
            if (cleanupFailure != failure) {
                failure.addSuppressed(cleanupFailure);
            }
        }
    }

    private Vec buildVec(VecBatchSerde.Vec protoVec, long nmmHandle) {
        VecBatchSerde.DataTypeExt protoTypeExt = protoVec.getTypeExt();
        VecEncoding vecEncoding = VecEncoding.values()[protoVec.getVecEncoding().getEncodingId()];
        VecBatchSerde.DataTypeExt.DataTypeId dataTypeId = protoTypeExt.getId();
        int vecSize = protoVec.getSize();
        switch (vecEncoding) {
            case OMNI_VEC_ENCODING_FLAT: {
                Vec vector = createFlatVec(vecSize, dataTypeId, protoVec, nmmHandle);
                try {
                    vector.setValuesBuf(protoVec.getValues().toByteArray());
                    vector.setNullsBuf(protoVec.getNulls().toByteArray());
                    return vector;
                } catch (RuntimeException failure) {
                    closeAfterFailure(vector, failure);
                    throw failure;
                }
            }
            case OMNI_VEC_ENCODING_CONTAINER: {
                int vecCount = protoVec.getSubVectorsCount();
                Vec[] children = new Vec[vecCount];
                long[] addresses = new long[vecCount];
                DataType[] types = new DataType[vecCount];
                try {
                    for (int i = 0; i < vecCount; i++) {
                        children[i] = buildVec(protoVec.getSubVectors(i), nmmHandle);
                        addresses[i] = children[i].getNativeVector();
                        types[i] = children[i].getType();
                    }
                    // The existing container constructor takes ownership of all children.
                    return new ContainerVec(vecCount, vecSize, addresses, types);
                } catch (RuntimeException failure) {
                    for (Vec child : children) {
                        closeAfterFailure(child, failure);
                    }
                    throw failure;
                }
            }
            case OMNI_ENCODING_ARRAY: {
                Vec element = null;
                ArrayVec array = null;
                boolean transferred = false;
                try {
                    element = buildVec(protoVec.getSubVectors(0), nmmHandle);
                    array = new ArrayVec(new ArrayDataType(element.getType()), vecSize, nmmHandle);
                    array.addElements(element);
                    transferred = true;
                    array.setNullsBuf(protoVec.getNulls().toByteArray());
                    array.setOffsetsBuf(protoVec.getOffsets().toByteArray());
                    return array;
                } catch (RuntimeException failure) {
                    closeAfterFailure(array, failure);
                    if (!transferred) {
                        closeAfterFailure(element, failure);
                    }
                    throw failure;
                }
            }
            case OMNI_ENCODING_STRUCT: {
                int fieldCount = protoVec.getSubVectorsCount();
                Vec[] fields = new Vec[fieldCount];
                DataType[] types = new DataType[fieldCount];
                StructVec struct = null;
                int transferred = 0;
                try {
                    for (int i = 0; i < fieldCount; i++) {
                        fields[i] = buildVec(protoVec.getSubVectors(i), nmmHandle);
                        types[i] = fields[i].getType();
                    }
                    struct = new StructVec(new StructDataType(types), vecSize, nmmHandle);
                    for (; transferred < fieldCount; transferred++) {
                        struct.setChild(transferred, fields[transferred]);
                    }
                    struct.setNullsBuf(protoVec.getNulls().toByteArray());
                    return struct;
                } catch (RuntimeException failure) {
                    closeAfterFailure(struct, failure);
                    for (int i = transferred; i < fieldCount; i++) {
                        closeAfterFailure(fields[i], failure);
                    }
                    throw failure;
                }
            }
            case OMNI_ENCODING_MAP: {
                if (protoVec.getSubVectorsCount() != 2) {
                    throw new IllegalStateException("MapVec must have exactly two subVectors");
                }
                Vec key = null;
                Vec value = null;
                MapVec map = null;
                boolean keyTransferred = false;
                boolean valueTransferred = false;
                try {
                    key = buildVec(protoVec.getSubVectors(0), nmmHandle);
                    value = buildVec(protoVec.getSubVectors(1), nmmHandle);
                    map = new MapVec(new MapDataType(key.getType(), value.getType()), vecSize, nmmHandle);
                    map.AddKeys(key);
                    keyTransferred = true;
                    map.AddValues(value);
                    valueTransferred = true;
                    map.setNullsBuf(protoVec.getNulls().toByteArray());
                    map.setOffsetsBuf(protoVec.getOffsets().toByteArray());
                    return map;
                } catch (RuntimeException failure) {
                    closeAfterFailure(map, failure);
                    if (!keyTransferred) {
                        closeAfterFailure(key, failure);
                    }
                    if (!valueTransferred) {
                        closeAfterFailure(value, failure);
                    }
                    throw failure;
                }
            }
            default:
                throw new IllegalStateException("Unexpected encoding: " + vecEncoding);
        }
    }

    private Vec createFlatVec(int vecSize, VecBatchSerde.DataTypeExt.DataTypeId dataTypeId,
            VecBatchSerde.Vec protoVec, long nmmHandle) {
        Vec vec;
        switch (dataTypeId) {
            case OMNI_BYTE:
                vec = new ByteVec(vecSize, nmmHandle);
                break;
            case OMNI_INT:
            case OMNI_DATE32:
                vec = new IntVec(vecSize, nmmHandle);
                break;
            case OMNI_LONG:
            case OMNI_TIMESTAMP:
            case OMNI_DATE64:
            case OMNI_DECIMAL64:
                vec = new LongVec(vecSize, nmmHandle);
                break;
            case OMNI_SHORT:
                vec = new ShortVec(vecSize, nmmHandle);
                break;
            case OMNI_BOOLEAN:
                vec = new BooleanVec(vecSize, nmmHandle);
                break;
            case OMNI_FLOAT:
                vec = new FloatVec(vecSize, nmmHandle);
                break;
            case OMNI_DOUBLE:
                vec = new DoubleVec(vecSize, nmmHandle);
                break;
            case OMNI_VARBINARY:
            case OMNI_VARCHAR:
            case OMNI_CHAR:
                VarcharVec varcharVec = new VarcharVec(protoVec.getValues().size(), vecSize, nmmHandle);
                varcharVec.setOffsetsBuf(protoVec.getOffsets().toByteArray());
                vec = varcharVec;
                break;
            case OMNI_DECIMAL128:
                vec = new Decimal128Vec(vecSize, nmmHandle);
                break;
            case OMNI_TIME32:
            case OMNI_TIME64:
            case OMNI_INTERVAL_DAY_TIME:
            case OMNI_INTERVAL_MONTHS:
            default:
                throw new IllegalStateException("Unexpected data type: " + protoVec.getTypeExt().getId());
        }
        return vec;
    }
}
