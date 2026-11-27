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

package nova.hetu.omniruntime.vector.serialize;

import com.google.protobuf.ByteString;
import com.google.protobuf.InvalidProtocolBufferException;

import nova.hetu.omniruntime.type.CharDataType;
import nova.hetu.omniruntime.type.DataType;
import nova.hetu.omniruntime.type.Date32DataType;
import nova.hetu.omniruntime.type.Date64DataType;
import nova.hetu.omniruntime.type.DecimalDataType;
import nova.hetu.omniruntime.type.VarcharDataType;
import nova.hetu.omniruntime.utils.OmniErrorType;
import nova.hetu.omniruntime.utils.OmniRuntimeException;
import nova.hetu.omniruntime.vector.BooleanVec;
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

    private VecBatchSerde.Vec buildProtoVec(Vec vec, int[] ids) {
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
            default:
                throw new IllegalStateException("Unexpected encoding: " + encoding);
        }

        Vec compactVec = compactVec(vec, ids);

        ByteBuffer valueBuf;
        if (compactVec instanceof VarcharVec) {
            VarcharVec varcharVec = (VarcharVec) compactVec;
            valueBuf = serializeVarcharVector(protoVecBuilder, varcharVec);
        } else {
            // For fixed vector, only serialize value.
            valueBuf = JvmUtils.directBuffer(compactVec.getValuesBuf());
            // only serialize the data actually written
            valueBuf.limit(compactVec.getRealValueBufCapacityInBytes());
        }

        ByteBuffer valueNullsBuf = JvmUtils.directBuffer(compactVec.getValueNullsBuf());
        // only serialize the actual null size
        valueNullsBuf.limit(compactVec.getRealNullBufCapacityInBytes());
        VecBatchSerde.Vec protoVec = protoVecBuilder.setTypeExt(protoDataTypeExtBuild.build())
                .setVecEncoding(protoVecEncodingBuild.build()).setSize(compactVec.getSize())
                .setValues(ByteString.copyFrom(valueBuf)).setNulls(ByteString.copyFrom(valueNullsBuf)).build();

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
                vecs[i] = buildVec(protoVecBatch.getVectors(i));
            }
        } catch (Exception e) {
            for (Vec v : vecs) {
                if (v == null) {
                    continue;
                }
                v.close();
            }
        }

        return new VecBatch(vecs, rowCount);
    }

    private Vec buildVec(VecBatchSerde.Vec protoVec) {
        VecBatchSerde.DataTypeExt protoTypeExt = protoVec.getTypeExt();
        VecEncoding vecEncoding = VecEncoding.values()[protoVec.getVecEncoding().getEncodingId()];
        VecBatchSerde.DataTypeExt.DataTypeId dataTypeId = protoTypeExt.getId();
        int vecSize = protoVec.getSize();
        Vec vec;
        switch (vecEncoding) {
            case OMNI_VEC_ENCODING_FLAT:
                vec = createFlatVec(vecSize, dataTypeId, protoVec);
                vec.setValuesBuf(protoVec.getValues().toByteArray());
                vec.setNullsBuf(protoVec.getNulls().toByteArray());
                return vec;
            case OMNI_VEC_ENCODING_CONTAINER:
                int vecCount = protoVec.getSubVectorsCount();
                long[] subVecAddresses = new long[vecCount];
                DataType[] subDataTypes = new DataType[vecCount];
                for (int i = 0; i < vecCount; i++) {
                    Vec subVec = buildVec(protoVec.getSubVectors(i));
                    subVecAddresses[i] = subVec.getNativeVector();
                    subDataTypes[i] = subVec.getType();
                }
                return new ContainerVec(vecCount, protoVec.getSize(), subVecAddresses, subDataTypes);
            default:
                throw new IllegalStateException("Unexpected encoding: " + vecEncoding);
        }
    }

    private Vec createFlatVec(int vecSize, VecBatchSerde.DataTypeExt.DataTypeId dataTypeId,
            VecBatchSerde.Vec protoVec) {
        Vec vec;
        switch (dataTypeId) {
            case OMNI_BYTE:
                vec = new ByteVec(vecSize);
                break;
            case OMNI_INT:
            case OMNI_DATE32:
                vec = new IntVec(vecSize);
                break;
            case OMNI_LONG:
            case OMNI_TIMESTAMP:
            case OMNI_DATE64:
            case OMNI_DECIMAL64:
                vec = new LongVec(vecSize);
                break;
            case OMNI_SHORT:
                vec = new ShortVec(vecSize);
                break;
            case OMNI_BOOLEAN:
                vec = new BooleanVec(vecSize);
                break;
            case OMNI_DOUBLE:
                vec = new DoubleVec(vecSize);
                break;
            case OMNI_VARCHAR:
            case OMNI_CHAR:
                VarcharVec varcharVec = new VarcharVec(protoVec.getValues().size(), vecSize);
                varcharVec.setOffsetsBuf(protoVec.getOffsets().toByteArray());
                vec = varcharVec;
                break;
            case OMNI_DECIMAL128:
                vec = new Decimal128Vec(vecSize);
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
