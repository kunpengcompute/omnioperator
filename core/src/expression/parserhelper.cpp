/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2021. All rights reserved.
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
#include "parserhelper.h"

using namespace std;
using namespace omniruntime::expressions;
using namespace omniruntime::type;

constexpr int8_t BYTE_DEFAULT_VALUE = 0;
constexpr int16_t SHORT_DEFAULT_VALUE = 0;
constexpr int32_t INT_DEFAULT_VALUE = 0;
constexpr int64_t LONG_DEFAULT_VALUE = 0L;
constexpr double DOUBLE_DEFAULT_VALUE = 0.000;
constexpr bool BOOL_DEFAULT_VALUE = true;
constexpr char CHAR_DEFAULT_VALUE[] = "NULL";
constexpr char DECIMAL128_DEFAULT_VALUE[] = "0";
constexpr int32_t CHAR_DEFAULT_WIDTH = 50;

omniruntime::expressions::LiteralExpr *ParserHelper::GetDefaultValueForType(DataTypeId destTypeId, int32_t precision,
    int32_t scale)
{
    DataTypePtr destType;
    if (TypeUtil::IsDecimalType(destTypeId)) {
        switch (destTypeId) {
            case OMNI_DECIMAL64: {
                destType = std::make_shared<Decimal64DataType>(precision, scale);
                return new LiteralExpr(LONG_DEFAULT_VALUE, std::move(destType));
            }
            case OMNI_DECIMAL128:
            default: {
                destType = std::make_shared<Decimal128DataType>(precision, scale);
                return new LiteralExpr(new string(DECIMAL128_DEFAULT_VALUE), std::move(destType));
            }
        }
    } else {
        destType = std::make_shared<DataType>(destTypeId);
        switch (destTypeId) {
            case OMNI_BYTE:
                return new LiteralExpr(BYTE_DEFAULT_VALUE, std::move(destType));
            case OMNI_SHORT:
                return new LiteralExpr(SHORT_DEFAULT_VALUE, std::move(destType));
            case OMNI_INT:
            case OMNI_DATE32:
                return new LiteralExpr(INT_DEFAULT_VALUE, std::move(destType));
            case OMNI_TIMESTAMP:
            case OMNI_LONG:
                return new LiteralExpr(LONG_DEFAULT_VALUE, std::move(destType));
            case OMNI_DOUBLE:
                return new LiteralExpr(DOUBLE_DEFAULT_VALUE, std::move(destType));
            case OMNI_BOOLEAN:
                return new LiteralExpr(BOOL_DEFAULT_VALUE, std::move(destType));
            case OMNI_CHAR:
                return new LiteralExpr(new string(CHAR_DEFAULT_VALUE),
                                       std::make_shared<CharDataType>(CHAR_DEFAULT_WIDTH));
            case OMNI_VARCHAR:
                return new LiteralExpr(new string(CHAR_DEFAULT_VALUE),
                                       std::make_shared<VarcharDataType>(CHAR_DEFAULT_WIDTH));
            case OMNI_NONE:
                return new LiteralExpr(INT_DEFAULT_VALUE, std::move(destType));
            default:
                return nullptr;
        }
    }
}

DataTypePtr ParserHelper::GetReturnDataType(nlohmann::json jsonExpr)
{
    auto typeId = static_cast<DataTypeId>(jsonExpr["returnType"].get<int32_t>());
    int32_t precision = 0;
    int32_t scale = 0;
    uint32_t width = 0;
    switch (typeId) {
        case OMNI_BOOLEAN:
            return std::make_shared<BooleanDataType>();
        case OMNI_BYTE:
            return std::make_shared<ByteDataType>();
        case OMNI_SHORT:
            return std::make_shared<ShortDataType>();
        case OMNI_INT:
            return std::make_shared<IntDataType>();
        case OMNI_DATE32:
            return std::make_shared<Date32DataType>();
        case OMNI_LONG:
            return std::make_shared<LongDataType>();
        case OMNI_TIMESTAMP:
            return std::make_shared<TimestampDataType>();
        case OMNI_DOUBLE:
            return std::make_shared<DoubleDataType>();
        case OMNI_DECIMAL64:
            precision = jsonExpr["precision"].get<int32_t>();
            scale = jsonExpr["scale"].get<int32_t>();
            return std::make_shared<Decimal64DataType>(precision, scale);
        case OMNI_DECIMAL128:
            precision = jsonExpr["precision"].get<int32_t>();
            scale = jsonExpr["scale"].get<int32_t>();
            return std::make_shared<Decimal128DataType>(precision, scale);
        case OMNI_VARCHAR:
            width = jsonExpr["width"].get<uint32_t>();
            return std::make_shared<VarcharDataType>(width);
        case OMNI_CHAR:
            width = jsonExpr["width"].get<uint32_t>();
            return std::make_shared<CharDataType>(width);
        case OMNI_NONE:
            return std::make_shared<NoneDataType>();
        default:
            LogError("Unsupported data type %d ", typeId);
            return nullptr;
    }
}
