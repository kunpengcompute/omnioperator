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
 *
 * Description: Inner supported aggregators source file
 */
#include "type_util.h"

using namespace omniruntime::type;

bool TypeUtil::IsStringType(omniruntime::type::DataTypeId id)
{
    return id == OMNI_CHAR || id == OMNI_VARCHAR;
}

bool TypeUtil::IsDecimalType(omniruntime::type::DataTypeId type)
{
    return type == OMNI_DECIMAL128 || type == OMNI_DECIMAL64;
}

std::string TypeUtil::TypeToString(omniruntime::type::DataTypeId id)
{
    switch (id) {
        case OMNI_BOOLEAN:
            return "Bool";
        case OMNI_DOUBLE:
            return "Double";
        case OMNI_DATE32:
            return "Date32";
        case OMNI_TIME32:
            return "Time32";
        case OMNI_INT:
            return "Int32";
        case OMNI_SHORT:
            return "Int16";
        case OMNI_BYTE:
            return "Int8";
        case OMNI_LONG:
            return "Int64";
        case OMNI_DATE64:
            return "Date64";
        case OMNI_TIME64:
            return "Time64";
        case OMNI_TIMESTAMP:
            return "Timestamp";
        case OMNI_VARCHAR:
            return "String";
        case OMNI_CHAR:
            return "Char";
        case OMNI_DECIMAL64:
            return "Decimal64";
        case OMNI_DECIMAL128:
            return "Decimal128";
        case OMNI_NONE:
            return "Void";
        case OMNI_INVALID:
            return "Invalid";
        case OMNI_CONTAINER:
            return "Container";
        case OMNI_INTERVAL_MONTHS:
            return "Interval_month";
        case OMNI_INTERVAL_DAY_TIME:
            return "Interval_day_time";
        case OMNI_ARRAY:
            return "Array";
        default:
            return "UNKNOWN";
    }
}

std::string TypeUtil::TypeToStringLog(omniruntime::type::DataTypeId id)
{
    switch (id) {
        case OMNI_BOOLEAN:
            return "bool";
        case OMNI_DOUBLE:
            return "double";
        case OMNI_DATE32:
            return "date32 (int32)";
        case OMNI_TIME32:
            return "time32 (int32)";
        case OMNI_INT:
            return "int32";
        case OMNI_SHORT:
            return "int16";
        case OMNI_BYTE:
            return "int8";
        case OMNI_LONG:
            return "int64";
        case OMNI_TIMESTAMP:
            return "Timestamp (int64)";
        case OMNI_VARCHAR:
            return "string";
        case OMNI_CHAR:
            return "char";
        case OMNI_DECIMAL64:
            return "decimal64";
        case OMNI_DECIMAL128:
            return "decimal128";
        case OMNI_NONE:
            return "void";
        case OMNI_INVALID:
            return "invalid";
        default:
            return "unknown";
    }
}

namespace omniruntime {
namespace type {
std::shared_ptr<DataType> InvalidType()
{
    return InvalidDataType::Instance();
}

std::shared_ptr<DataType> NoneType()
{
    return NoneDataType::Instance();
}

std::shared_ptr<DataType> IntType()
{
    return IntDataType::Instance();
}

std::shared_ptr<DataType> ByteType()
{
    return ByteDataType::Instance();
}

std::shared_ptr<DataType> ShortType()
{
    return ShortDataType::Instance();
}

std::shared_ptr<DataType> Date32Type()
{
    return Date32DataType::Instance();
}

std::shared_ptr<DataType> Date64Type()
{
    return Date64DataType::Instance();
}

std::shared_ptr<DataType> Date32Type(omniruntime::type::DateUnit dateUnit)
{
    return std::make_shared<Date32DataType>(dateUnit);
}

std::shared_ptr<DataType> Date64Type(omniruntime::type::DateUnit dateUnit)
{
    return std::make_shared<Date64DataType>(dateUnit);
}

std::shared_ptr<DataType> Time32Type()
{
    return Time32DataType::Instance();
}

std::shared_ptr<DataType> Time64Type()
{
    return Time64DataType::Instance();
}

std::shared_ptr<DataType> Time32Type(omniruntime::type::TimeUnit timeUnit)
{
    return std::make_shared<Time32DataType>(timeUnit);
}

std::shared_ptr<DataType> Time64Type(omniruntime::type::TimeUnit timeUnit)
{
    return std::make_shared<Time64DataType>(timeUnit);
}

std::shared_ptr<DataType> LongType()
{
    return LongDataType::Instance();
}

std::shared_ptr<omniruntime::type::DataType> TimestampType()
{
    return TimestampDataType::Instance();
}

std::shared_ptr<DataType> DoubleType()
{
    return DoubleDataType::Instance();
}

std::shared_ptr<DataType> BooleanType()
{
    return BooleanDataType::Instance();
}

std::shared_ptr<DataType> VarcharType()
{
    return std::make_shared<VarcharDataType>(INT_MAX);
}

std::shared_ptr<DataType> CharType()
{
    return std::make_shared<CharDataType>(CHAR_MAX_WIDTH);
}

std::shared_ptr<DataType> VarcharType(int32_t width)
{
    return std::make_shared<VarcharDataType>(width);
}

std::shared_ptr<DataType> CharType(int32_t width)
{
    return std::make_shared<CharDataType>(width);
}

std::shared_ptr<DataType> Decimal64Type()
{
    return std::make_shared<Decimal64DataType>(DECIMAL64_DEFAULT_PRECISION, DECIMAL64_DEFAULT_SCALE);
}

std::shared_ptr<DataType> Decimal128Type()
{
    return std::make_shared<Decimal128DataType>(DECIMAL128_DEFAULT_PRECISION, DECIMAL128_DEFAULT_SCALE);
}

std::shared_ptr<DataType> Decimal64Type(int32_t precision, int32_t scale)
{
    return std::make_shared<Decimal64DataType>(precision, scale);
}

std::shared_ptr<DataType> Decimal128Type(int32_t precision, int32_t scale)
{
    return std::make_shared<Decimal128DataType>(precision, scale);
}

std::shared_ptr<DataType> ContainerType()
{
    return std::make_shared<ContainerDataType>();
}

std::shared_ptr<ContainerDataType> ContainerType(std::vector<DataTypePtr> &fieldTypes)
{
    return std::make_shared<ContainerDataType>(fieldTypes);
}

std::shared_ptr<ContainerDataType> ContainerType(std::vector<DataTypePtr> &&fieldTypes)
{
    return std::make_shared<ContainerDataType>(fieldTypes);
}

std::shared_ptr<ArrayType> ArrayDataType(DataTypePtr child)
{
    return std::make_shared<ArrayType>(child);
}
}
}