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

package nova.hetu.omniruntime.type;

import static nova.hetu.omniruntime.utils.OmniErrorType.OMNI_INNER_ERROR;
import static nova.hetu.omniruntime.utils.OmniErrorType.OMNI_NOSUPPORT;

import com.fasterxml.jackson.annotation.JsonTypeInfo;
import com.fasterxml.jackson.core.JsonProcessingException;
import com.fasterxml.jackson.databind.DatabindContext;
import com.fasterxml.jackson.databind.JavaType;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.jsontype.impl.TypeIdResolverBase;

import nova.hetu.omniruntime.utils.OmniRuntimeException;

import java.io.IOException;

/**
 * Data type serializer, used for serialize and deserialize the data type.
 *
 * @since 2021-08-05
 */
public class DataTypeSerializer {
    /**
     * Object mapper singleton.
     */
    private static final ObjectMapper OBJECT_MAPPER = new ObjectMapper();

    /**
     * Serialize a single data type.
     *
     * @param dataType serialize a single data type
     * @return return the string after serialization
     */
    public static String serializeSingle(DataType dataType) {
        try {
            return OBJECT_MAPPER.writeValueAsString(dataType);
        } catch (JsonProcessingException e) {
            throw new OmniRuntimeException(OMNI_INNER_ERROR, "Serialization failed.", e);
        }
    }

    /**
     * Serialize data types.
     *
     * @param dataTypes serialize data types
     * @return return the string after serialization
     */
    public static String serialize(DataType[] dataTypes) {
        try {
            return OBJECT_MAPPER.writeValueAsString(dataTypes);
        } catch (JsonProcessingException e) {
            throw new OmniRuntimeException(OMNI_INNER_ERROR, "Serialization failed.", e);
        }
    }

    /**
     * Serialize data types.
     *
     * @param dataTypes serialize data types[][]
     * @return return the string[] after serialization
     */
    public static String[] serialize(DataType[][] dataTypes) {
        String[] strings = new String[dataTypes.length];
        for (int i = 0; i < dataTypes.length; i++) {
            strings[i] = serialize(dataTypes[i]);
        }
        return strings;
    }

    /**
     * Deserialize a single data type.
     *
     * @param type the string need to be deserialization
     * @return return the vector type
     */
    public static DataType deserializeSingle(String type) {
        try {
            return OBJECT_MAPPER.readerFor(DataType.class).readValue(type);
        } catch (JsonProcessingException e) {
            throw new OmniRuntimeException(OMNI_INNER_ERROR, "Deserialization failed.", e);
        }
    }

    /**
     * Deserialize data types.
     *
     * @param types the string need to be deserialization
     * @return return the vector types
     */
    public static DataType[] deserialize(String types) {
        try {
            return OBJECT_MAPPER.readerFor(DataType[].class).readValue(types);
        } catch (JsonProcessingException e) {
            throw new OmniRuntimeException(OMNI_INNER_ERROR, "Deserialization failed.", e);
        }
    }

    static class DataTypeResolver extends TypeIdResolverBase {
        private JavaType superType;

        @Override
        public JavaType typeFromId(DatabindContext context, String id) throws IOException {
            Class<?> subType = null;
            DataType.DataTypeId dataTypeId = DataType.DataTypeId.fromValue(Integer.parseInt(id));
            switch (dataTypeId) {
                case OMNI_INT:
                    subType = IntDataType.class;
                    break;
                case OMNI_LONG:
                    subType = LongDataType.class;
                    break;
                case OMNI_DOUBLE:
                    subType = DoubleDataType.class;
                    break;
                case OMNI_TIMESTAMP:
                    subType = TimestampDataType.class;
                    break;
                case OMNI_BOOLEAN:
                    subType = BooleanDataType.class;
                    break;
                case OMNI_SHORT:
                    subType = ShortDataType.class;
                    break;
                case OMNI_BYTE:
                    subType = ByteDataType.class;
                    break;
                case OMNI_CONTAINER:
                    subType = ContainerDataType.class;
                    break;
                case OMNI_NONE:
                    subType = NoneDataType.class;
                    break;
                case OMNI_INVALID:
                    subType = InvalidDataType.class;
                    break;
                case OMNI_VARCHAR:
                    subType = VarcharDataType.class;
                    break;
                case OMNI_CHAR:
                    subType = CharDataType.class;
                    break;
                case OMNI_DECIMAL64:
                    subType = Decimal64DataType.class;
                    break;
                case OMNI_DECIMAL128:
                    subType = Decimal128DataType.class;
                    break;
                case OMNI_DATE32:
                    subType = Date32DataType.class;
                    break;
                case OMNI_DATE64:
                    subType = Date64DataType.class;
                    break;
                default:
                    throw new OmniRuntimeException(OMNI_NOSUPPORT, "Unsupported data type : " + id);
            }
            return context.constructSpecializedType(superType, subType);
        }

        @Override
        public JsonTypeInfo.Id getMechanism() {
            return JsonTypeInfo.Id.CUSTOM;
        }

        @Override
        public String idFromValue(Object value) {
            return value.getClass().toString();
        }

        @Override
        public String idFromValueAndType(Object value, Class<?> suggestedType) {
            return null;
        }

        @Override
        public void init(JavaType baseType) {
            superType = baseType;
        }
    }
}
