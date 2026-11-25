/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "data_type_serializer.h"
#include <nlohmann/json.hpp>
#include "util/type_util.h"

namespace omniruntime {
namespace type {
using json = nlohmann::json;

std::string Serialize(const std::vector<DataTypePtr> &types) { return json(types).dump(); }

std::string SerializeSingle(const DataTypePtr &type) { return json(type).dump(); }

DataTypes Deserialize(const std::string &dataTypes)
{
    auto dataTypeJsons = nlohmann::json::parse(dataTypes);
    std::vector<DataTypePtr> types;
    for (const auto &dataTypeJson : dataTypeJsons) {
        types.push_back(DataTypeJsonParser(dataTypeJson));
    }
    return DataTypes(types);
}

DataTypePtr DeserializeSingle(const std::string &dataType)
{
    auto dataTypeJson = nlohmann::json::parse(dataType);
    return omniruntime::type::DataTypeJsonParser(dataTypeJson);
}

DataTypePtr DataTypeJsonParser(const nlohmann::json &dataTypeJson)
{
    int dataTypeId = dataTypeJson[ID].get<int>();
    switch (dataTypeId) {
        case OMNI_NONE: return NoneType();
        case OMNI_INVALID: return InvalidType();
        case OMNI_INT: return IntType();
        case OMNI_LONG: return LongType();
        case OMNI_TIMESTAMP: return TimestampType();
        case OMNI_DOUBLE: return DoubleType();
        case OMNI_BOOLEAN: return BooleanType();
        case OMNI_BYTE: return ByteType();
        case OMNI_SHORT: return ShortType();
        case OMNI_DECIMAL64:
            return Decimal64Type(dataTypeJson[PRECISION].get<int32_t>(), dataTypeJson[SCALE].get<int32_t>());
        case OMNI_DECIMAL128:
            return Decimal128Type(dataTypeJson[PRECISION].get<int32_t>(), dataTypeJson[SCALE].get<int32_t>());
        case OMNI_DATE32: return Date32Type(dataTypeJson[DATE_UNIT].get<DateUnit>());
        case OMNI_DATE64: return Date64Type(dataTypeJson[DATE_UNIT].get<DateUnit>());
        case OMNI_TIME32: return Time32Type(dataTypeJson[TIME_UNIT].get<TimeUnit>());
        case OMNI_TIME64: return Time64Type(dataTypeJson[TIME_UNIT].get<TimeUnit>());
        case OMNI_VARCHAR: return VarcharType(dataTypeJson[WIDTH].get<uint32_t>());
        case OMNI_CHAR: return CharType(dataTypeJson[WIDTH].get<uint32_t>());
        case OMNI_CONTAINER: {
            std::vector<DataTypePtr> fieldTypes;
            for (const auto &fieldJson : dataTypeJson[FIELD_TYPES]) {
                fieldTypes.push_back(DataTypeJsonParser(fieldJson));
            }
            return ContainerType(fieldTypes);
        }
        default: LogError("Not Supported Data Type : %d", dataTypeId); return nullptr;
    }
}
} // namespace type
} // namespace omniruntime
