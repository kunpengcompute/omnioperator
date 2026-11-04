/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Cast function implementation for vectorized type conversion
 */

#include "Cast.h"
#include "vector/vector.h"
#include <exception>
#include <limits>
#include <cstring>
#include <optional>
#include <type/Conversions.h>
#include "codegen/functions/dtoa.h"

namespace omniruntime::vectorization {
    using namespace omniruntime::type;
    using namespace omniruntime::vec;

void CastFunction::Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType,
    BaseVector *&result, ExecutionContext *context) const {
    const_cast<CastFunction *>(this)->hooks_ = std::make_shared<CastHooks>(context->queryConfig());
    auto input = args.top();
    args.pop();
    DispatchCast(input, result, context);
    // Clean up input argument if it's not being used as the result
    // (same type cast or string-to-binary sets result = inputArg)
    if (input != result) {
        delete input;
    }
}

void CastFunction::DispatchCast(BaseVector* input, BaseVector*& result, ExecutionContext* context) const {
    switch (toType_->GetId()) {
        case OMNI_BOOLEAN:
            CastToBoolean(input, result, context);
            break;
        case OMNI_BYTE:
            CastToByte(input, result, context);
            break;
        case OMNI_SHORT:
            CastToShort(input, result, context);
            break;
        case OMNI_INT:
            CastToInt(input, result, context);
            break;
        case OMNI_LONG:
            CastToLong(input, result, context);
            break;
        case OMNI_FLOAT:
            CastToFloat(input, result, context);
            break;
        case OMNI_DOUBLE:
            CastToDouble(input, result, context);
            break;
        case OMNI_VARCHAR:
            CastToString(input, result, context);
            break;
        case OMNI_DATE32:
            CastToDate(input, result, context);
            break;
        case OMNI_TIMESTAMP:
            CastToTimestamp(input, result, context);
            break;
        case OMNI_DECIMAL64:
            CastToDecimal64(input, result, context);
            break;
        case OMNI_DECIMAL128:
            CastToDecimal128(input, result, context);
            break;
        case OMNI_VARBINARY:
            CastToBinary(input, result, context);
            break;
        default:
            OMNI_THROW("Cast function Error", "Unsupported cast to " + TypeUtil::TypeToString(toType_->GetId()));
    }
}

void CastFunction::CastToBoolean(BaseVector* input, BaseVector*& result, ExecutionContext* context) const {
    auto size = context->GetResultRowSize();
    result = VectorHelper::CreateFlatVector(OMNI_BOOLEAN, size);
    auto* resultVec = static_cast<Vector<bool>*>(result);
    if (input->GetEncoding() == OMNI_ENCODING_CONST) {
        bool boolValue = false;
        switch (fromType_->GetId()) {
            case OMNI_BYTE:
                boolValue = static_cast<ConstVector<int8_t> *>(input)->GetConstValue() != 0;
                break;
            case OMNI_SHORT:
                boolValue = static_cast<ConstVector<int16_t> *>(input)->GetConstValue() != 0;
                break;
            case OMNI_INT:
                boolValue = static_cast<ConstVector<int32_t> *>(input)->GetConstValue() != 0;
                break;
            case OMNI_LONG:
            case OMNI_DECIMAL64:
                boolValue = static_cast<ConstVector<int64_t> *>(input)->GetConstValue() != 0;
                break;
            case OMNI_FLOAT:
                boolValue = static_cast<ConstVector<float> *>(input)->GetConstValue() != 0;
                break;
            case OMNI_DOUBLE:
                boolValue = static_cast<ConstVector<double> *>(input)->GetConstValue() != 0;
                break;
            case OMNI_CHAR:
            case OMNI_VARCHAR:
            {
                auto boolResult = folly::tryTo<bool>(static_cast<ConstVector<std::string_view> *>(input)->GetConstValue());
                if (boolResult.hasValue()) {
                    boolValue = boolResult.value();
                } else {
                    result->SetNulls(0, true, size);
                    return;
                }
                break;
            }
            case OMNI_DECIMAL128:
            {
                Decimal128 temp(static_cast<ConstVector<std::string_view> *>(input)->GetConstValue());
                boolValue = !temp.IsZero();
                break;
            }
            default:
                OMNI_THROW("Cast function Error", "Unsupported cast to boolean from " + TypeUtil::TypeToString(fromType_->GetId()));
        }
        for (int32_t row = 0; row < size; ++row) {
            resultVec->SetValue(row, boolValue);
        }
    } else {
        switch (fromType_->GetId()) {
            case OMNI_BYTE: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    resultVec->SetValue(row, VectorHelper::GetValueFromVector<int8_t>(input, row) != 0);
                }
                break;
            }
            case OMNI_SHORT: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    resultVec->SetValue(row, VectorHelper::GetValueFromVector<int16_t>(input, row) != 0);
                }
                break;
            }
            case OMNI_INT: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    resultVec->SetValue(row, VectorHelper::GetValueFromVector<int32_t>(input, row) != 0);
                }
                break;
            }
            case OMNI_LONG:
            case OMNI_DECIMAL64: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    resultVec->SetValue(row, VectorHelper::GetValueFromVector<int64_t>(input, row) != 0);
                }
                break;
            }
            case OMNI_FLOAT: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    resultVec->SetValue(row, VectorHelper::GetValueFromVector<float>(input, row) != 0);
                }
                break;
            }
            case OMNI_DOUBLE: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    resultVec->SetValue(row, VectorHelper::GetValueFromVector<double>(input, row) != 0);
                }
                break;
            }
            case OMNI_CHAR:
            case OMNI_VARCHAR: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    auto boolResult = folly::tryTo<bool>(VectorHelper::GetStringValueFromVector(input, row));
                    if (boolResult.hasValue()) {
                        resultVec->SetValue(row, boolResult.value());
                    } else {
                        result->SetNull(row);
                    }
                }
                break;
            }
            case OMNI_DECIMAL128: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    resultVec->SetValue(row, !VectorHelper::GetValueFromVector<Decimal128>(input, row).IsZero());
                }
                break;
            }
            default:
                OMNI_THROW("Cast function Error", "Unsupported cast to boolean from " + TypeUtil::TypeToString(fromType_->GetId()));
        }
    }
}

void CastFunction::CastToByte(BaseVector* input, BaseVector*& result, ExecutionContext* context) const {
    auto size = context->GetResultRowSize();
    result = VectorHelper::CreateFlatVector(OMNI_BYTE, size);
    auto* resultVec = static_cast<Vector<int8_t>*>(result);
    if (input->GetEncoding() == OMNI_ENCODING_CONST) {
        int8_t byteValue;
        switch (fromType_->GetId()) {
            case OMNI_BOOLEAN:
                byteValue = static_cast<int8_t>(static_cast<ConstVector<bool> *>(input)->GetConstValue() ? 1 : 0);
                break;
            case OMNI_SHORT:
                byteValue = static_cast<int8_t>(static_cast<ConstVector<int16_t> *>(input)->GetConstValue());
                break;
            case OMNI_INT:
                byteValue = static_cast<int8_t>(static_cast<ConstVector<int32_t> *>(input)->GetConstValue());
                break;
            case OMNI_LONG:
                byteValue = static_cast<int8_t>(static_cast<ConstVector<int64_t> *>(input)->GetConstValue());
                break;
            case OMNI_FLOAT:
                byteValue = static_cast<int8_t>(static_cast<ConstVector<float> *>(input)->GetConstValue());
                break;
            case OMNI_DOUBLE:
                byteValue = static_cast<int8_t>(static_cast<ConstVector<double> *>(input)->GetConstValue());
                break;
            case OMNI_CHAR:
            case OMNI_VARCHAR:
            {
                auto str = static_cast<ConstVector<std::string_view> *>(input)->GetConstValue();
                type::Status status = ConvertStringToInteger<int8_t, true>(byteValue, str.data(), str.size());
                if (status == type::Status::IS_NOT_A_NUMBER || status == type::Status::CONVERT_OVERFLOW) {
                    result->SetNulls(0, true, size);
                    return;
                }
                break;
            }
            case OMNI_DECIMAL64:
            {
                auto temp = static_cast<ConstVector<int64_t> *>(input)->GetConstValue();
                auto scale = static_cast<DecimalDataType *>(fromType_.get())->GetScale();
                byteValue = static_cast<int8_t>(Decimal64(temp).ReScale(-scale, RoundingMode::ROUND_FLOOR).GetValue());
                break;
            }
            case OMNI_DECIMAL128:
            {
                Decimal128 temp(static_cast<ConstVector<std::string_view> *>(input)->GetConstValue());
                auto scale = static_cast<DecimalDataType *>(fromType_.get())->GetScale();
                try {
                    byteValue = static_cast<int8_t>(Decimal128Wrapper(temp).ReScale(-scale, RoundingMode::ROUND_FLOOR).ToInt128());
                } catch (std::overflow_error &e) {
                    result->SetNulls(0, true, size);
                    return;
                }
                break;
            }
            default:
                OMNI_THROW("Cast function Error", "Unsupported cast to byte from " + TypeUtil::TypeToString(fromType_->GetId()));
        }
        for (int32_t row = 0; row < size; ++row) {
            resultVec->SetValue(row, byteValue);
        }
    } else {
        switch (fromType_->GetId()) {
            case OMNI_BOOLEAN: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    resultVec->SetValue(row, static_cast<int8_t>(VectorHelper::GetValueFromVector<bool>(input, row) ? 1 : 0));
                }
                break;
            }
            case OMNI_SHORT: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    resultVec->SetValue(row, static_cast<int8_t>(VectorHelper::GetValueFromVector<int16_t>(input, row)));
                }
                break;
            }
            case OMNI_INT: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    resultVec->SetValue(row, static_cast<int8_t>(VectorHelper::GetValueFromVector<int32_t>(input, row)));
                }
                break;
            }
            case OMNI_LONG: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    resultVec->SetValue(row, static_cast<int8_t>(VectorHelper::GetValueFromVector<int64_t>(input, row)));
                }
                break;
            }
            case OMNI_FLOAT: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    resultVec->SetValue(row, static_cast<int8_t>(VectorHelper::GetValueFromVector<float>(input, row)));
                }
                break;
            }
            case OMNI_DOUBLE: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    resultVec->SetValue(row, static_cast<int8_t>(VectorHelper::GetValueFromVector<double>(input, row)));
                }
                break;
            }
            case OMNI_CHAR:
            case OMNI_VARCHAR: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    auto str = VectorHelper::GetStringValueFromVector(input, row);
                    int8_t byteValue;
                    type::Status status = ConvertStringToInteger<int8_t, true>(byteValue, str.data(), str.size());
                    if (status == type::Status::IS_NOT_A_NUMBER || status == type::Status::CONVERT_OVERFLOW) {
                        result->SetNull(row);
                        continue;
                    }
                    resultVec->SetValue(row, byteValue);
                }
                break;
            }
            case OMNI_DECIMAL64: {
                auto scale = static_cast<DecimalDataType *>(fromType_.get())->GetScale();
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    auto temp = VectorHelper::GetValueFromVector<int64_t>(input, row);
                    resultVec->SetValue(row, static_cast<int8_t>(Decimal64(temp).ReScale(-scale, RoundingMode::ROUND_FLOOR).GetValue()));
                }
                break;
            }
            case OMNI_DECIMAL128: {
                auto scale = static_cast<DecimalDataType *>(fromType_.get())->GetScale();
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    Decimal128 temp = VectorHelper::GetValueFromVector<Decimal128>(input, row);
                    try {
                        resultVec->SetValue(row, static_cast<int8_t>(Decimal128Wrapper(temp).ReScale(-scale, RoundingMode::ROUND_FLOOR).ToInt128()));
                    } catch (std::overflow_error &e) {
                        result->SetNull(row);
                    }
                }
                break;
            }
            default:
                OMNI_THROW("Cast function Error", "Unsupported cast to byte from " + TypeUtil::TypeToString(fromType_->GetId()));
        }
    }
}

void CastFunction::CastToShort(BaseVector* input, BaseVector*& result, ExecutionContext* context) const {
    auto size = context->GetResultRowSize();
    result = VectorHelper::CreateFlatVector(OMNI_SHORT, size);
    auto* resultVec = static_cast<Vector<int16_t>*>(result);
    if (input->GetEncoding() == OMNI_ENCODING_CONST) {
        int16_t shortValue;
        switch (fromType_->GetId()) {
            case OMNI_BOOLEAN:
                shortValue = static_cast<int16_t>(static_cast<ConstVector<bool> *>(input)->GetConstValue() ? 1 : 0);
                break;
            case OMNI_BYTE:
                shortValue = static_cast<int16_t>(static_cast<ConstVector<int8_t> *>(input)->GetConstValue());
                break;
            case OMNI_INT:
                shortValue = static_cast<int16_t>(static_cast<ConstVector<int32_t> *>(input)->GetConstValue());
                break;
            case OMNI_LONG:
                shortValue = static_cast<int16_t>(static_cast<ConstVector<int64_t> *>(input)->GetConstValue());
                break;
            case OMNI_FLOAT:
                shortValue = static_cast<int16_t>(static_cast<ConstVector<float> *>(input)->GetConstValue());
                break;
            case OMNI_DOUBLE:
                shortValue = static_cast<int16_t>(static_cast<ConstVector<double> *>(input)->GetConstValue());
                break;
            case OMNI_CHAR:
            case OMNI_VARCHAR:
            {
                auto str = static_cast<ConstVector<std::string_view> *>(input)->GetConstValue();
                type::Status status = ConvertStringToInteger<int16_t, true>(shortValue, str.data(), str.size());
                if (status == type::Status::IS_NOT_A_NUMBER || status == type::Status::CONVERT_OVERFLOW) {
                    result->SetNulls(0, true, size);
                    return;
                }
                break;
            }
            case OMNI_DECIMAL64:
            {
                auto temp = static_cast<ConstVector<int64_t> *>(input)->GetConstValue();
                auto scale = static_cast<DecimalDataType *>(fromType_.get())->GetScale();
                shortValue = static_cast<int16_t>(Decimal64(temp).ReScale(-scale, RoundingMode::ROUND_FLOOR).GetValue());
                break;
            }
            case OMNI_DECIMAL128:
            {
                Decimal128 temp(static_cast<ConstVector<std::string_view> *>(input)->GetConstValue());
                auto scale = static_cast<DecimalDataType *>(fromType_.get())->GetScale();
                try {
                    shortValue = static_cast<int16_t>(Decimal128Wrapper(temp).ReScale(-scale, RoundingMode::ROUND_FLOOR).ToInt128());
                } catch (std::overflow_error &e) {
                    result->SetNulls(0, true, size);
                    return;
                }
                break;
            }
            default:
                OMNI_THROW("Cast function Error", "Unsupported cast to short from " + TypeUtil::TypeToString(fromType_->GetId()));
        }
        for (int32_t row = 0; row < size; ++row) {
            resultVec->SetValue(row, shortValue);
        }
    } else {
        switch (fromType_->GetId()) {
            case OMNI_BOOLEAN: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    resultVec->SetValue(row, static_cast<int16_t>(VectorHelper::GetValueFromVector<bool>(input, row) ? 1 : 0));
                }
                break;
            }
            case OMNI_BYTE: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    resultVec->SetValue(row, static_cast<int16_t>(VectorHelper::GetValueFromVector<int8_t>(input, row)));
                }
                break;
            }
            case OMNI_INT: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    resultVec->SetValue(row, static_cast<int16_t>(VectorHelper::GetValueFromVector<int32_t>(input, row)));
                }
                break;
            }
            case OMNI_LONG: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    resultVec->SetValue(row, static_cast<int16_t>(VectorHelper::GetValueFromVector<int64_t>(input, row)));
                }
                break;
            }
            case OMNI_FLOAT: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    resultVec->SetValue(row, static_cast<int16_t>(VectorHelper::GetValueFromVector<float>(input, row)));
                }
                break;
            }
            case OMNI_DOUBLE: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    resultVec->SetValue(row, static_cast<int16_t>(VectorHelper::GetValueFromVector<double>(input, row)));
                }
                break;
            }
            case OMNI_CHAR:
            case OMNI_VARCHAR: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    auto str = VectorHelper::GetStringValueFromVector(input, row);
                    int16_t shortValue;
                    type::Status status = ConvertStringToInteger<int16_t, true>(shortValue, str.data(), str.size());
                    if (status == type::Status::IS_NOT_A_NUMBER || status == type::Status::CONVERT_OVERFLOW) {
                        result->SetNull(row);
                        continue;
                    }
                    resultVec->SetValue(row, shortValue);
                }
                break;
            }
            case OMNI_DECIMAL64: {
                auto scale = static_cast<DecimalDataType *>(fromType_.get())->GetScale();
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    auto temp = VectorHelper::GetValueFromVector<int64_t>(input, row);
                    resultVec->SetValue(row, static_cast<int16_t>(Decimal64(temp).ReScale(-scale, RoundingMode::ROUND_FLOOR).GetValue()));
                }
                break;
            }
            case OMNI_DECIMAL128: {
                auto scale = static_cast<DecimalDataType *>(fromType_.get())->GetScale();
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    Decimal128 temp = VectorHelper::GetValueFromVector<Decimal128>(input, row);
                    try {
                        resultVec->SetValue(row, static_cast<int16_t>(Decimal128Wrapper(temp).ReScale(-scale, RoundingMode::ROUND_FLOOR).ToInt128()));
                    } catch (std::overflow_error &e) {
                        result->SetNull(row);
                    }
                }
                break;
            }
            default:
                OMNI_THROW("Cast function Error", "Unsupported cast to short from " + TypeUtil::TypeToString(fromType_->GetId()));
        }
    }
}

void CastFunction::CastToInt(BaseVector* input, BaseVector*& result, ExecutionContext* context) const {
    auto size = context->GetResultRowSize();
    result = VectorHelper::CreateFlatVector(OMNI_INT, size);
    auto* resultVec = static_cast<Vector<int32_t>*>(result);
    if (input->GetEncoding() == OMNI_ENCODING_CONST) {
        int32_t intValue;
        switch (fromType_->GetId()) {
            case OMNI_BOOLEAN:
                intValue = static_cast<int32_t>(static_cast<ConstVector<bool> *>(input)->GetConstValue() ? 1 : 0);
                break;
            case OMNI_BYTE:
                intValue = static_cast<int32_t>(static_cast<ConstVector<int8_t> *>(input)->GetConstValue());
                break;
            case OMNI_SHORT:
                intValue = static_cast<int32_t>(static_cast<ConstVector<int16_t> *>(input)->GetConstValue());
                break;
            case OMNI_LONG:
                intValue = static_cast<int32_t>(static_cast<ConstVector<int64_t> *>(input)->GetConstValue());
                break;
            case OMNI_FLOAT:
                intValue = static_cast<int32_t>(static_cast<ConstVector<float> *>(input)->GetConstValue());
                break;
            case OMNI_DOUBLE:
                intValue = static_cast<int32_t>(static_cast<ConstVector<double> *>(input)->GetConstValue());
                break;
            case OMNI_CHAR:
            case OMNI_VARCHAR:
            {
                auto str = static_cast<ConstVector<std::string_view> *>(input)->GetConstValue();
                type::Status status = ConvertStringToInteger<int32_t, true>(intValue, str.data(), str.size());
                if (status == type::Status::IS_NOT_A_NUMBER || status == type::Status::CONVERT_OVERFLOW) {
                    result->SetNulls(0, true, size);
                    return;
                }
                break;
            }
            case OMNI_DECIMAL64:
            {
                auto temp = static_cast<ConstVector<int64_t> *>(input)->GetConstValue();
                auto scale = static_cast<DecimalDataType *>(fromType_.get())->GetScale();
                intValue = static_cast<int32_t>(Decimal64(temp).ReScale(-scale, RoundingMode::ROUND_FLOOR).GetValue());
                break;
            }
            case OMNI_DECIMAL128:
            {
                Decimal128 temp(static_cast<ConstVector<std::string_view> *>(input)->GetConstValue());
                auto scale = static_cast<DecimalDataType *>(fromType_.get())->GetScale();
                try {
                    intValue = static_cast<int32_t>(Decimal128Wrapper(temp).ReScale(-scale, RoundingMode::ROUND_FLOOR).ToInt128());
                } catch (std::overflow_error &e) {
                    result->SetNulls(0, true, size);
                    return;
                }
                break;
            }
            default:
                OMNI_THROW("Cast function Error", "Unsupported cast to int from " + TypeUtil::TypeToString(fromType_->GetId()));
        }
        for (int32_t row = 0; row < size; ++row) {
            resultVec->SetValue(row, intValue);
        }
    } else {
        switch (fromType_->GetId()) {
            case OMNI_BOOLEAN: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    resultVec->SetValue(row, static_cast<int32_t>(VectorHelper::GetValueFromVector<bool>(input, row) ? 1 : 0));
                }
                break;
            }
            case OMNI_BYTE: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    resultVec->SetValue(row, static_cast<int32_t>(VectorHelper::GetValueFromVector<int8_t>(input, row)));
                }
                break;
            }
            case OMNI_SHORT: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    resultVec->SetValue(row, static_cast<int32_t>(VectorHelper::GetValueFromVector<int16_t>(input, row)));
                }
                break;
            }
            case OMNI_LONG: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    resultVec->SetValue(row, static_cast<int32_t>(VectorHelper::GetValueFromVector<int64_t>(input, row)));
                }
                break;
            }
            case OMNI_FLOAT: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    resultVec->SetValue(row, static_cast<int32_t>(VectorHelper::GetValueFromVector<float>(input, row)));
                }
                break;
            }
            case OMNI_DOUBLE: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    resultVec->SetValue(row, static_cast<int32_t>(VectorHelper::GetValueFromVector<double>(input, row)));
                }
                break;
            }
            case OMNI_CHAR:
            case OMNI_VARCHAR: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    auto str = VectorHelper::GetStringValueFromVector(input, row);
                    int32_t intValue;
                    type::Status status = ConvertStringToInteger<int32_t, true>(intValue, str.data(), str.size());
                    if (status == type::Status::IS_NOT_A_NUMBER || status == type::Status::CONVERT_OVERFLOW) {
                        result->SetNull(row);
                        continue;
                    }
                    resultVec->SetValue(row, intValue);
                }
                break;
            }
            case OMNI_DECIMAL64: {
                auto scale = static_cast<DecimalDataType *>(fromType_.get())->GetScale();
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    auto temp = VectorHelper::GetValueFromVector<int64_t>(input, row);
                    resultVec->SetValue(row, static_cast<int32_t>(Decimal64(temp).ReScale(-scale, RoundingMode::ROUND_FLOOR).GetValue()));
                }
                break;
            }
            case OMNI_DECIMAL128: {
                auto scale = static_cast<DecimalDataType *>(fromType_.get())->GetScale();
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    Decimal128 temp = VectorHelper::GetValueFromVector<Decimal128>(input, row);
                    try {
                        resultVec->SetValue(row, static_cast<int32_t>(Decimal128Wrapper(temp).ReScale(-scale, RoundingMode::ROUND_FLOOR).ToInt128()));
                    } catch (std::overflow_error &e) {
                        result->SetNull(row);
                    }
                }
                break;
            }
            default:
                OMNI_THROW("Cast function Error", "Unsupported cast to int from " + TypeUtil::TypeToString(fromType_->GetId()));
        }
    }
}

void CastFunction::CastToLong(BaseVector* input, BaseVector*& result, ExecutionContext* context) const {
    auto size = context->GetResultRowSize();
    result = VectorHelper::CreateFlatVector(OMNI_LONG, size);
    auto* resultVec = static_cast<Vector<int64_t>*>(result);
    if (input->GetEncoding() == OMNI_ENCODING_CONST) {
        int64_t longValue;
        switch (fromType_->GetId()) {
            case OMNI_BOOLEAN:
                longValue = static_cast<int64_t>(static_cast<ConstVector<bool> *>(input)->GetConstValue() ? 1 : 0);
                break;
            case OMNI_BYTE:
                longValue = static_cast<int64_t>(static_cast<ConstVector<int8_t> *>(input)->GetConstValue());
                break;
            case OMNI_SHORT:
                longValue = static_cast<int64_t>(static_cast<ConstVector<int16_t> *>(input)->GetConstValue());
                break;
            case OMNI_INT:
                longValue = static_cast<int64_t>(static_cast<ConstVector<int32_t> *>(input)->GetConstValue());
                break;
            case OMNI_FLOAT:
                longValue = static_cast<int64_t>(static_cast<ConstVector<float> *>(input)->GetConstValue());
                break;
            case OMNI_DOUBLE:
                longValue = static_cast<int64_t>(static_cast<ConstVector<double> *>(input)->GetConstValue());
                break;
            case OMNI_CHAR:
            case OMNI_VARCHAR:
            {
                auto str = static_cast<ConstVector<std::string_view> *>(input)->GetConstValue();
                type::Status status = ConvertStringToInteger<int64_t, true>(longValue, str.data(), str.size());
                if (status == type::Status::IS_NOT_A_NUMBER || status == type::Status::CONVERT_OVERFLOW) {
                    result->SetNulls(0, true, size);
                    return;
                }
                break;
            }
            case OMNI_DECIMAL64:
            {
                auto temp = static_cast<ConstVector<int64_t> *>(input)->GetConstValue();
                auto scale = static_cast<DecimalDataType *>(fromType_.get())->GetScale();
                longValue = static_cast<int64_t>(Decimal64(temp).ReScale(-scale, RoundingMode::ROUND_FLOOR).GetValue());
                break;
            }
            case OMNI_DECIMAL128:
            {
                Decimal128 temp(static_cast<ConstVector<std::string_view> *>(input)->GetConstValue());
                auto scale = static_cast<DecimalDataType *>(fromType_.get())->GetScale();
                try {
                    longValue = static_cast<int64_t>(Decimal128Wrapper(temp).ReScale(-scale, RoundingMode::ROUND_FLOOR).ToInt128());
                } catch (std::overflow_error &e) {
                    result->SetNulls(0, true, size);
                    return;
                }
                break;
            }
            default:
                OMNI_THROW("Cast function Error", "Unsupported cast to long from " + TypeUtil::TypeToString(fromType_->GetId()));
        }
        for (int32_t row = 0; row < size; ++row) {
            resultVec->SetValue(row, longValue);
        }
    } else {
        switch (fromType_->GetId()) {
            case OMNI_BOOLEAN: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    resultVec->SetValue(row, static_cast<int64_t>(VectorHelper::GetValueFromVector<bool>(input, row) ? 1 : 0));
                }
                break;
            }
            case OMNI_BYTE: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    resultVec->SetValue(row, static_cast<int64_t>(VectorHelper::GetValueFromVector<int8_t>(input, row)));
                }
                break;
            }
            case OMNI_SHORT: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    resultVec->SetValue(row, static_cast<int64_t>(VectorHelper::GetValueFromVector<int16_t>(input, row)));
                }
                break;
            }
            case OMNI_INT: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    resultVec->SetValue(row, static_cast<int64_t>(VectorHelper::GetValueFromVector<int32_t>(input, row)));
                }
                break;
            }
            case OMNI_FLOAT: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    resultVec->SetValue(row, static_cast<int64_t>(VectorHelper::GetValueFromVector<float>(input, row)));
                }
                break;
            }
            case OMNI_DOUBLE: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    resultVec->SetValue(row, static_cast<int64_t>(VectorHelper::GetValueFromVector<double>(input, row)));
                }
                break;
            }
            case OMNI_CHAR:
            case OMNI_VARCHAR: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    auto str = VectorHelper::GetStringValueFromVector(input, row);
                    int64_t longValue;
                    type::Status status = ConvertStringToInteger<int64_t, true>(longValue, str.data(), str.size());
                    if (status == type::Status::IS_NOT_A_NUMBER || status == type::Status::CONVERT_OVERFLOW) {
                        result->SetNull(row);
                        continue;
                    }
                    resultVec->SetValue(row, longValue);
                }
                break;
            }
            case OMNI_DECIMAL64: {
                auto scale = static_cast<DecimalDataType *>(fromType_.get())->GetScale();
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    auto temp = VectorHelper::GetValueFromVector<int64_t>(input, row);
                    resultVec->SetValue(row, static_cast<int64_t>(Decimal64(temp).ReScale(-scale, RoundingMode::ROUND_FLOOR).GetValue()));
                }
                break;
            }
            case OMNI_DECIMAL128: {
                auto scale = static_cast<DecimalDataType *>(fromType_.get())->GetScale();
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    Decimal128 temp = VectorHelper::GetValueFromVector<Decimal128>(input, row);
                    try {
                        resultVec->SetValue(row, static_cast<int64_t>(Decimal128Wrapper(temp).ReScale(-scale, RoundingMode::ROUND_FLOOR).ToInt128()));
                    } catch (std::overflow_error &e) {
                        result->SetNull(row);
                    }
                }
                break;
            }
            default:
                OMNI_THROW("Cast function Error", "Unsupported cast to long from " + TypeUtil::TypeToString(fromType_->GetId()));
        }
    }
}

void CastFunction::CastToFloat(BaseVector* input, BaseVector*& result, ExecutionContext* context) const {
    auto size = context->GetResultRowSize();
    result = VectorHelper::CreateFlatVector(OMNI_FLOAT, size);
    auto* resultVec = static_cast<Vector<float>*>(result);
    if (input->GetEncoding() == OMNI_ENCODING_CONST) {
        float floatValue;
        switch (fromType_->GetId()) {
            case OMNI_BOOLEAN:
                floatValue = static_cast<float>(static_cast<ConstVector<bool> *>(input)->GetConstValue() ? 1 : 0);
                break;
            case OMNI_BYTE:
                floatValue = static_cast<float>(static_cast<ConstVector<int8_t> *>(input)->GetConstValue());
                break;
            case OMNI_SHORT:
                floatValue = static_cast<float>(static_cast<ConstVector<int16_t> *>(input)->GetConstValue());
                break;
            case OMNI_INT:
                floatValue = static_cast<float>(static_cast<ConstVector<int32_t> *>(input)->GetConstValue());
                break;
            case OMNI_LONG:
                floatValue = static_cast<float>(static_cast<ConstVector<int64_t> *>(input)->GetConstValue());
                break;
            case OMNI_DOUBLE:
                floatValue = static_cast<float>(static_cast<ConstVector<double> *>(input)->GetConstValue());
                break;
            case OMNI_TIMESTAMP:
                floatValue = static_cast<float>(static_cast<ConstVector<int64_t> *>(input)->GetConstValue()) / 1000000.0f;
                break;
            case OMNI_CHAR:
            case OMNI_VARCHAR:
            {
                double temp;
                auto str = static_cast<ConstVector<std::string_view> *>(input)->GetConstValue();
                type::Status status = ConvertStringToDouble(temp, str.data(), str.size());
                if (status == type::Status::IS_NOT_A_NUMBER || status == type::Status::CONVERT_OVERFLOW) {
                    result->SetNulls(0, true, size);
                    return;
                }
                floatValue = static_cast<float>(temp);
                break;
            }
            case OMNI_DECIMAL64:
            {
                auto temp = static_cast<ConstVector<int64_t> *>(input)->GetConstValue();
                auto scale = static_cast<DecimalDataType *>(fromType_.get())->GetScale();
                floatValue = std::stof(Decimal64(temp).SetScale(scale).ToString());
                break;
            }
            case OMNI_DECIMAL128:
            {
                Decimal128 temp(static_cast<ConstVector<std::string_view> *>(input)->GetConstValue());
                auto scale = static_cast<DecimalDataType *>(fromType_.get())->GetScale();
                floatValue = static_cast<float>(static_cast<double>(Decimal128Wrapper(temp).SetScale(scale)));
                break;
            }
            default:
                OMNI_THROW("Cast function Error", "Unsupported cast to float from " + TypeUtil::TypeToString(fromType_->GetId()));
        }
        for (int32_t row = 0; row < size; ++row) {
            resultVec->SetValue(row, floatValue);
        }
    } else {
        switch (fromType_->GetId()) {
            case OMNI_BOOLEAN: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    resultVec->SetValue(row, static_cast<float>(VectorHelper::GetValueFromVector<bool>(input, row) ? 1 : 0));
                }
                break;
            }
            case OMNI_BYTE: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    resultVec->SetValue(row, static_cast<float>(VectorHelper::GetValueFromVector<int8_t>(input, row)));
                }
                break;
            }
            case OMNI_SHORT: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    resultVec->SetValue(row, static_cast<float>(VectorHelper::GetValueFromVector<int16_t>(input, row)));
                }
                break;
            }
            case OMNI_INT: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    resultVec->SetValue(row, static_cast<float>(VectorHelper::GetValueFromVector<int32_t>(input, row)));
                }
                break;
            }
            case OMNI_LONG: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    resultVec->SetValue(row, static_cast<float>(VectorHelper::GetValueFromVector<int64_t>(input, row)));
                }
                break;
            }
            case OMNI_DOUBLE: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    resultVec->SetValue(row, static_cast<float>(VectorHelper::GetValueFromVector<double>(input, row)));
                }
                break;
            }
            case OMNI_CHAR:
            case OMNI_VARCHAR: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    double temp;
                    auto str = VectorHelper::GetStringValueFromVector(input, row);
                    type::Status status = ConvertStringToDouble(temp, str.data(), str.size());
                    if (status == type::Status::IS_NOT_A_NUMBER || status == type::Status::CONVERT_OVERFLOW) {
                        result->SetNull(row);
                        continue;
                    }
                    resultVec->SetValue(row, static_cast<float>(temp));
                }
                break;
            }
            case OMNI_DECIMAL64: {
                auto scale = static_cast<DecimalDataType *>(fromType_.get())->GetScale();
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    auto temp = VectorHelper::GetValueFromVector<int64_t>(input, row);
                    resultVec->SetValue(row, std::stof(Decimal64(temp).SetScale(scale).ToString()));
                }
                break;
            }
            case OMNI_DECIMAL128: {
                auto scale = static_cast<DecimalDataType *>(fromType_.get())->GetScale();
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    Decimal128 temp = VectorHelper::GetValueFromVector<Decimal128>(input, row);
                    resultVec->SetValue(row, static_cast<float>(static_cast<double>(Decimal128Wrapper(temp).SetScale(scale))));
                }
                break;
            }
            default:
                OMNI_THROW("Cast function Error", "Unsupported cast to float from " + TypeUtil::TypeToString(fromType_->GetId()));
        }
    }
}

void CastFunction::CastToDouble(BaseVector* input, BaseVector*& result, ExecutionContext* context) const {
    auto size = context->GetResultRowSize();
    result = VectorHelper::CreateFlatVector(OMNI_DOUBLE, size);
    auto* resultVec = static_cast<Vector<double>*>(result);
    if (input->GetEncoding() == OMNI_ENCODING_CONST) {
        double doubleValue;
        switch (fromType_->GetId()) {
            case OMNI_BOOLEAN:
                doubleValue = static_cast<double>(static_cast<ConstVector<bool> *>(input)->GetConstValue() ? 1 : 0);
                break;
            case OMNI_BYTE:
                doubleValue = static_cast<double>(static_cast<ConstVector<int8_t> *>(input)->GetConstValue());
                break;
            case OMNI_SHORT:
                doubleValue = static_cast<double>(static_cast<ConstVector<int16_t> *>(input)->GetConstValue());
                break;
            case OMNI_INT:
                doubleValue = static_cast<double>(static_cast<ConstVector<int32_t> *>(input)->GetConstValue());
                break;
            case OMNI_LONG:
                doubleValue = static_cast<double>(static_cast<ConstVector<int64_t> *>(input)->GetConstValue());
                break;
            case OMNI_FLOAT:
                doubleValue = static_cast<double>(static_cast<ConstVector<float> *>(input)->GetConstValue());
                break;
            case OMNI_CHAR:
            case OMNI_VARCHAR:
            {
                auto str = static_cast<ConstVector<std::string_view> *>(input)->GetConstValue();
                type::Status status = ConvertStringToDouble(doubleValue, str.data(), str.size());
                if (status == type::Status::IS_NOT_A_NUMBER || status == type::Status::CONVERT_OVERFLOW) {
                    result->SetNulls(0, true, size);
                    return;
                }
                break;
            }
            case OMNI_DECIMAL64:
            {
                auto temp = static_cast<ConstVector<int64_t> *>(input)->GetConstValue();
                auto scale = static_cast<DecimalDataType *>(fromType_.get())->GetScale();
                doubleValue = static_cast<double>(Decimal64(temp).SetScale(scale));
                break;
            }
            case OMNI_DECIMAL128:
            {
                Decimal128 temp(static_cast<ConstVector<std::string_view> *>(input)->GetConstValue());
                auto scale = static_cast<DecimalDataType *>(fromType_.get())->GetScale();
                doubleValue = static_cast<double>(Decimal128Wrapper(temp).SetScale(scale));
                break;
            }
            default:
                OMNI_THROW("Cast function Error", "Unsupported cast to double from " + TypeUtil::TypeToString(fromType_->GetId()));
        }
        for (int32_t row = 0; row < size; ++row) {
            resultVec->SetValue(row, doubleValue);
        }
    } else {
        switch (fromType_->GetId()) {
            case OMNI_BOOLEAN: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    resultVec->SetValue(row, static_cast<double>(VectorHelper::GetValueFromVector<bool>(input, row) ? 1 : 0));
                }
                break;
            }
            case OMNI_BYTE: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    resultVec->SetValue(row, static_cast<double>(VectorHelper::GetValueFromVector<int8_t>(input, row)));
                }
                break;
            }
            case OMNI_SHORT: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    resultVec->SetValue(row, static_cast<double>(VectorHelper::GetValueFromVector<int16_t>(input, row)));
                }
                break;
            }
            case OMNI_INT: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    resultVec->SetValue(row, static_cast<double>(VectorHelper::GetValueFromVector<int32_t>(input, row)));
                }
                break;
            }
            case OMNI_LONG: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    resultVec->SetValue(row, static_cast<double>(VectorHelper::GetValueFromVector<int64_t>(input, row)));
                }
                break;
            }
            case OMNI_FLOAT: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    resultVec->SetValue(row, static_cast<double>(VectorHelper::GetValueFromVector<float>(input, row)));
                }
                break;
            }
            case OMNI_CHAR:
            case OMNI_VARCHAR: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    auto str = VectorHelper::GetStringValueFromVector(input, row);
                    double doubleValue;
                    type::Status status = ConvertStringToDouble(doubleValue, str.data(), str.size());
                    if (status == type::Status::IS_NOT_A_NUMBER || status == type::Status::CONVERT_OVERFLOW) {
                        result->SetNull(row);
                        continue;
                    }
                    resultVec->SetValue(row, doubleValue);
                }
                break;
            }
            case OMNI_DECIMAL64: {
                auto scale = static_cast<DecimalDataType *>(fromType_.get())->GetScale();
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    auto temp = VectorHelper::GetValueFromVector<int64_t>(input, row);
                    resultVec->SetValue(row, static_cast<double>(Decimal64(temp).SetScale(scale)));
                }
                break;
            }
            case OMNI_DECIMAL128: {
                auto scale = static_cast<DecimalDataType *>(fromType_.get())->GetScale();
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    Decimal128 temp = VectorHelper::GetValueFromVector<Decimal128>(input, row);
                    resultVec->SetValue(row, static_cast<double>(Decimal128Wrapper(temp).SetScale(scale)));
                }
                break;
            }
            default:
                OMNI_THROW("Cast function Error", "Unsupported cast to double from " + TypeUtil::TypeToString(fromType_->GetId()));
        }
    }
}

void CastFunction::CastToString(BaseVector* input, BaseVector*& result, ExecutionContext* context) const {
    switch (fromType_->GetId()) {
        case OMNI_BOOLEAN:
            CastNumericToString<bool>(input, result, context);
            break;
        case OMNI_BYTE:
            CastNumericToString<int8_t>(input, result, context);
            break;
        case OMNI_SHORT:
            CastNumericToString<int16_t>(input, result, context);
            break;
        case OMNI_INT:
            CastNumericToString<int32_t>(input, result, context);
            break;
        case OMNI_DATE32:
            CastDateToString(input, result, context);
            break;
        case OMNI_LONG:
            CastNumericToString<int64_t>(input, result, context);
            break;
        case OMNI_TIMESTAMP:
            CastTimestampToString(input, result, context);
            break;
        case OMNI_FLOAT:
            CastNumericToString<float>(input, result, context);
            break;
        case OMNI_DOUBLE:
            CastNumericToString<double>(input, result, context);
            break;
        case OMNI_DECIMAL64:
            CastDecimal64ToString(input, result, context);
            break;
        case OMNI_DECIMAL128:
            CastDecimal128ToString(input, result, context);
            break;
        case OMNI_CHAR:
        case OMNI_VARCHAR:
        case OMNI_VARBINARY:
            // String to string, just copy
            result = input;
            break;
        case OMNI_STRING_VIEW:
            CastStringViewToOmniVarchar(input, result, context);
            break;
        default:
            OMNI_THROW("Cast function Error", "Unsupported cast to string from " + TypeUtil::TypeToString(fromType_->GetId()));
    }
}

void CastFunction::CastStringViewToOmniVarchar(BaseVector* input, BaseVector*& result, ExecutionContext* context) const {
    auto size = context->GetResultRowSize();
    result = VectorHelper::CreateFlatVector(OMNI_VARCHAR, size);
    auto *out = static_cast<Vector<LargeStringContainer<std::string_view>> *>(result);
    if (input->GetEncoding() == OMNI_ENCODING_CONST) {
        if (input->IsNull(0)) {
            result->SetNulls(0, true, size);
            return;
        }
        StringView sv = static_cast<ConstVector<StringView> *>(input)->GetConstValue();
        std::string_view view(sv.data(), sv.size());
        for (int32_t row = 0; row < size; ++row) {
            out->SetValue(row, view); // SetValue deep-copies bytes into the varchar buffer
        }
        return;
    }
    auto *src = static_cast<Vector<StringView> *>(input);
    for (int32_t row = 0; row < size; ++row) {
        if (input->IsNull(row)) {
            // Must use the VARCHAR vector's own SetNull (advances the offset buffer). BaseVector::SetNull
            // is non-virtual and only touches the null bitmap; calling it via a BaseVector* leaves
            // offsets[row+1] unset, so a null row (esp. the last) leaves offsets[size]=0 →
            // getRealValueBufCapacityInBytes()=0 → the proto serializer drops ALL values → broadcast
            // build-side StringView corruption.
            out->SetNull(row);
            continue;
        }
        StringView sv = src->GetValue(row);
        std::string_view view(sv.data(), sv.size()); // safe for inline & non-inline; SetValue copies out
        out->SetValue(row, view);
    }
}

template<typename T>
void CastFunction::CastNumericToString(BaseVector* input, BaseVector*& result, ExecutionContext* context) const {
    auto size = context->GetResultRowSize();
    result = VectorHelper::CreateFlatVector(toType_->GetId(), size);
    auto* resultVec = static_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    if (input->GetEncoding() == OMNI_ENCODING_CONST) {
        auto *constVec = static_cast<ConstVector<T> *>(input);
        std::string strValue;
        if constexpr (std::is_same_v<T, float>) {
            strValue = codegen::function::DoubleToString::FloatToStringConverter(constVec->GetConstValue());
        } else if constexpr (std::is_same_v<T, double>) {
            strValue = codegen::function::DoubleToString::DoubleToStringConverter(constVec->GetConstValue());
        } else {
            const auto castResult = type::util::Converter<OMNI_VARCHAR>::tryCast(constVec->GetConstValue());
            if (castResult.hasError()) {
                result->SetNulls(0, true, size);
                return;
            }
            strValue = castResult.value();
        }
        std::string_view strView(strValue);
        for (int32_t row = 0; row < size; ++row) {
            resultVec->SetValue(row, strView);
        }
    } else {
        for (int32_t row = 0; row < size; ++row) {
            if (input->IsNull(row)) {
                resultVec->SetNull(row);
                continue;
            }
            T value = VectorHelper::GetValueFromVector<T>(input, row);
            std::string strValue;
            if constexpr (std::is_same_v<T, float>) {
                // {:.7E} keeps only 8 significant digits. Spark Float.toString needs the
                // same round-trip digits as the codegen FloatToStringConverter.
                strValue = codegen::function::DoubleToString::FloatToStringConverter(value);
            } else if constexpr (std::is_same_v<T, double>) {
                strValue = codegen::function::DoubleToString::DoubleToStringConverter(value);
            } else {
                const auto castResult = type::util::Converter<OMNI_VARCHAR>::tryCast(value);
                if (castResult.hasError()) {
                    resultVec->SetNull(row);
                    continue;
                }
                strValue = castResult.value();
            }
            std::string_view strView(strValue);
            resultVec->SetValue(row, strView);
        }
    }
}

template void CastFunction::CastNumericToString<bool>(BaseVector*, BaseVector*&, ExecutionContext*) const;
template void CastFunction::CastNumericToString<int8_t>(BaseVector*, BaseVector*&, ExecutionContext*) const;
template void CastFunction::CastNumericToString<int16_t>(BaseVector*, BaseVector*&, ExecutionContext*) const;
template void CastFunction::CastNumericToString<int32_t>(BaseVector*, BaseVector*&, ExecutionContext*) const;
template void CastFunction::CastNumericToString<int64_t>(BaseVector*, BaseVector*&, ExecutionContext*) const;
template void CastFunction::CastNumericToString<float>(BaseVector*, BaseVector*&, ExecutionContext*) const;
template void CastFunction::CastNumericToString<double>(BaseVector*, BaseVector*&, ExecutionContext*) const;

void CastFunction::CastDateToString(BaseVector* input, BaseVector*& result, ExecutionContext* context) const {
    auto size = context->GetResultRowSize();
    result = VectorHelper::CreateFlatVector(OMNI_VARCHAR, size);
    auto* resultVec = static_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    if (input->GetEncoding() == OMNI_ENCODING_CONST) {
        std::string strValue = type::util::ToIso8601(static_cast<ConstVector<int32_t> *>(input)->GetConstValue());
        std::string_view strView(strValue);
        for (int32_t row = 0; row < size; ++row) {
            resultVec->SetValue(row, strView);
        }
    } else {
        for (int32_t row = 0; row < size; ++row) {
            if (input->IsNull(row)) {
                result->SetNull(row);
                continue;
            }
            int32_t value = VectorHelper::GetValueFromVector<int32_t>(input, row);
            std::string strValue = type::util::ToIso8601(value);
            std::string_view strView(strValue);
            resultVec->SetValue(row, strView);
        }
    }
}

void CastFunction::CastTimestampToString(BaseVector* input, BaseVector*& result, ExecutionContext* context) const {
    auto options = hooks_->timestampToStringOptions();
    auto size = context->GetResultRowSize();
    result = VectorHelper::CreateFlatVector(OMNI_VARCHAR, size);
    auto* resultVec = static_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    if (input->GetEncoding() == OMNI_ENCODING_CONST) {
        Timestamp inputValue = Timestamp::fromMicros(static_cast<ConstVector<int64_t> *>(input)->GetConstValue());
        if (options.timeZone) {
            inputValue.toTimezone(*(options.timeZone));
        }
        char buf[64];
        auto strView = Timestamp::tsToStringView(inputValue, options, buf);
        for (int32_t row = 0; row < size; ++row) {
            resultVec->SetValue(row, strView);
        }
    } else {
        // Do not format into the varchar vector's own buffer. That buffer is only the
        // initial allocation; walking it by string length overwrites the heap once the
        // batch is larger than a few rows. SetValue copies the bytes and grows the buffer.
        for (int32_t row = 0; row < size; ++row) {
            if (input->IsNull(row)) {
                resultVec->SetNull(row);
                continue;
            }
            int64_t value = VectorHelper::GetValueFromVector<int64_t>(input, row);
            Timestamp inputValue = Timestamp::fromMicros(value);
            if (options.timeZone) {
                inputValue.toTimezone(*(options.timeZone));
            }
            char buf[64];
            auto strView = Timestamp::tsToStringView(inputValue, options, buf);
            resultVec->SetValue(row, strView);
        }
    }
}

void CastFunction::CastDecimal64ToString(BaseVector* input, BaseVector*& result, ExecutionContext* context) const {
    auto dataType = static_cast<DecimalDataType *>(fromType_.get());
    auto size = context->GetResultRowSize();
    result = VectorHelper::CreateFlatVector(OMNI_VARCHAR, size);
    auto* resultVec = static_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    if (input->GetEncoding() == OMNI_ENCODING_CONST) {
        std::string str = Decimal64(static_cast<ConstVector<int64_t> *>(input)->GetConstValue()).SetScale(dataType->GetScale()).ToString();
        auto strView(str);
        for (int32_t row = 0; row < size; ++row) {
            resultVec->SetValue(row, strView);
        }
    } else {
        for (int32_t row = 0; row < size; ++row) {
            if (input->IsNull(row)) {
                result->SetNull(row);
                continue;
            }
            auto value = VectorHelper::GetValueFromVector<int64_t>(input, row);
            std::string str = Decimal64(value).SetScale(dataType->GetScale()).ToString();
            auto strView(str);
            resultVec->SetValue(row, strView);
        }
    }
}

void CastFunction::CastDecimal128ToString(BaseVector* input, BaseVector*& result, ExecutionContext* context) const {
    auto dataType = static_cast<DecimalDataType *>(fromType_.get());
    auto size = context->GetResultRowSize();
    result = VectorHelper::CreateFlatVector(OMNI_VARCHAR, size);
    auto* resultVec = static_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    if (input->GetEncoding() == OMNI_ENCODING_CONST) {
        Decimal128 value(static_cast<ConstVector<std::string_view> *>(input)->GetConstValue());
        std::string str = Decimal128Wrapper(value).SetScale(dataType->GetScale()).ToString();
        auto strView(str);
        for (int32_t row = 0; row < size; ++row) {
            resultVec->SetValue(row, strView);
        }
    } else {
        for (int32_t row = 0; row < size; ++row) {
            if (input->IsNull(row)) {
                result->SetNull(row);
                continue;
            }
            auto value = VectorHelper::GetValueFromVector<Decimal128>(input, row);
            std::string str = Decimal128Wrapper(value).SetScale(dataType->GetScale()).ToString();
            auto strView(str);
            resultVec->SetValue(row, strView);
        }
    }
}

void CastFunction::CastToDate(BaseVector* input, BaseVector*& result, ExecutionContext* context) const {
    const auto size = context->GetResultRowSize();
    result = VectorHelper::CreateFlatVector(OMNI_DATE32, size);
    auto* resultVec = static_cast<Vector<int32_t>*>(result);
    if (input->GetEncoding() == OMNI_ENCODING_CONST) {
        int32_t dateValue;
        switch (fromType_->GetId()) {
            case OMNI_CHAR:
            case OMNI_VARCHAR:
            {
                const auto str = static_cast<ConstVector<std::string_view> *>(input)->GetConstValue();
                const auto resultValue = hooks_->castStringToDate(str);
                if (resultValue.hasError()) {
                    result->SetNulls(0, true, size);
                    return;
                }
                dateValue = resultValue.value();
                break;
            }
            case OMNI_TIMESTAMP:
            {
                auto temp = static_cast<ConstVector<int64_t> *>(input)->GetConstValue();
                auto config = context->queryConfig();
                const tz::TimeZone *timeZone = nullptr;
                if (config.AdjustTimestampToTimezone()) {
                    const auto sessionTzName = config.SessionTimezone();
                    if (!sessionTzName.empty()) {
                        timeZone = tz::locateZone(sessionTzName);
                    }
                }
                dateValue = type::util::toDate(Timestamp::fromMicros(temp), timeZone);
                break;
            }
            default:
                OMNI_THROW("Cast function Error", "Unsupported cast to date from " + TypeUtil::TypeToString(fromType_->GetId()));
        }
        for (int32_t row = 0; row < size; ++row) {
            resultVec->SetValue(row, dateValue);
        }
    } else {
        switch (fromType_->GetId()) {
            case OMNI_CHAR:
            case OMNI_VARCHAR: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    const auto str = VectorHelper::GetStringValueFromVector(input, row);
                    const auto resultValue = hooks_->castStringToDate(str);
                    if (resultValue.hasError()) {
                        result->SetNull(row);
                        continue;
                    }
                    resultVec->SetValue(row, resultValue.value());
                }
                break;
            }
            case OMNI_TIMESTAMP: {
                auto config = context->queryConfig();
                const tz::TimeZone *timeZone = nullptr;
                if (config.AdjustTimestampToTimezone()) {
                    const auto sessionTzName = config.SessionTimezone();
                    if (!sessionTzName.empty()) {
                        timeZone = tz::locateZone(sessionTzName);
                    }
                }
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    auto temp = VectorHelper::GetValueFromVector<int64_t>(input, row);
                    resultVec->SetValue(row, type::util::toDate(Timestamp::fromMicros(temp), timeZone));
                }
                break;
            }
            default:
                OMNI_THROW("Cast function Error", "Unsupported cast to date from " + TypeUtil::TypeToString(fromType_->GetId()));
        }
    }
}

void CastFunction::CastToTimestamp(BaseVector* input, BaseVector*& result, ExecutionContext* context) const {
    const auto size = context->GetResultRowSize();
    result = VectorHelper::CreateFlatVector(OMNI_TIMESTAMP, size);
    auto* resultVec = static_cast<Vector<int64_t>*>(result);
    const auto parseString = [this](const std::string_view inputValue) -> std::optional<int64_t> {
        try {
            const auto trimmedValue = hooks_->removeWhiteSpaces(inputValue);
            if (trimmedValue.empty()) {
                return std::nullopt;
            }
            const auto parsedValue = hooks_->castStringToTimestamp(trimmedValue);
            if (parsedValue.hasError()) {
                return std::nullopt;
            }
            return parsedValue.value();
        } catch (const std::exception&) {
            return std::nullopt;
        }
    };
    if (input->GetEncoding() == OMNI_ENCODING_CONST) {
        int64_t timestampValue;
        switch (fromType_->GetId()) {
            case OMNI_VARCHAR:
            {
                if (input->IsNull(0)) {
                    result->SetNulls(0, true, size);
                    return;
                }
                const auto parsedValue = parseString(
                    static_cast<ConstVector<std::string_view> *>(input)->GetConstValue());
                if (!parsedValue.has_value()) {
                    result->SetNulls(0, true, size);
                    return;
                }
                timestampValue = parsedValue.value();
                break;
            }
            case OMNI_DATE32:
            {
                static constexpr int64_t kMillisPerDay{86'400'000};
                auto temp = static_cast<ConstVector<int32_t> *>(input)->GetConstValue();
                auto config = context->queryConfig();
                const tz::TimeZone *timeZone = nullptr;
                if (config.AdjustTimestampToTimezone()) {
                    const auto sessionTzName = config.SessionTimezone();
                    if (!sessionTzName.empty()) {
                        timeZone = tz::locateZone(sessionTzName);
                    }
                }
                auto timestamp = Timestamp::fromMillis(temp * kMillisPerDay);
                if (timeZone) {
                    timestamp.toGMT(*timeZone);
                }
                timestampValue = timestamp.toMicros();
                break;
            }
            default:
                OMNI_THROW("Cast function Error", "Unsupported cast to timestamp from " + TypeUtil::TypeToString(fromType_->GetId()));
        }
        for (int32_t row = 0; row < size; ++row) {
            resultVec->SetValue(row, timestampValue);
        }
    } else {
        switch (fromType_->GetId()) {
            case OMNI_VARCHAR: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    const auto parsedValue = parseString(VectorHelper::GetStringValueFromVector(input, row));
                    if (!parsedValue.has_value()) {
                        result->SetNull(row);
                        continue;
                    }
                    resultVec->SetValue(row, parsedValue.value());
                }
                break;
            }
            case OMNI_DATE32: {
                static constexpr int64_t kMillisPerDay{86'400'000};
                auto config = context->queryConfig();
                const tz::TimeZone *timeZone = nullptr;
                if (config.AdjustTimestampToTimezone()) {
                    const auto sessionTzName = config.SessionTimezone();
                    if (!sessionTzName.empty()) {
                        timeZone = tz::locateZone(sessionTzName);
                    }
                }
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    auto temp = VectorHelper::GetValueFromVector<int32_t>(input, row);
                    auto timestamp = Timestamp::fromMillis(temp * kMillisPerDay);
                    if (timeZone) {
                        timestamp.toGMT(*timeZone);
                    }
                    resultVec->SetValue(row, timestamp.toMicros());
                }
                break;
            }
            default:
                OMNI_THROW("Cast function Error", "Unsupported cast to timestamp from " + TypeUtil::TypeToString(fromType_->GetId()));
        }
    }
}

void CastFunction::CastToDecimal64(BaseVector* input, BaseVector*& result, ExecutionContext* context) const {
    auto size = context->GetResultRowSize();
    auto toDataType = static_cast<DecimalDataType *>(toType_.get());
    int toPrecision = toDataType->GetPrecision();
    int toScale = toDataType->GetScale();
    result = VectorHelper::CreateFlatVector(OMNI_DECIMAL64, size);
    auto* resultVec = static_cast<Vector<int64_t>*>(result);
    if (input->GetEncoding() == OMNI_ENCODING_CONST) {
        int64_t decimal64Value;
        switch (fromType_->GetId()) {
            case OMNI_BOOLEAN:
            {
                auto temp = DecimalOperations::rescaleInt<bool, int64_t>(static_cast<ConstVector<bool> *>(input)->GetConstValue(), toPrecision, toScale);
                if (temp.has_value()) {
                    decimal64Value = temp.value();
                } else {
                    result->SetNulls(0, true, size);
                    return;
                }
                break;
            }
            case OMNI_BYTE:
            {
                auto temp = DecimalOperations::rescaleInt<int8_t, int64_t>(static_cast<ConstVector<int8_t> *>(input)->GetConstValue(), toPrecision, toScale);
                if (temp.has_value()) {
                    decimal64Value = temp.value();
                } else {
                    result->SetNulls(0, true, size);
                    return;
                }
                break;
            }
            case OMNI_SHORT:
            {
                auto temp = DecimalOperations::rescaleInt<int16_t, int64_t>(static_cast<ConstVector<int16_t> *>(input)->GetConstValue(), toPrecision, toScale);
                if (temp.has_value()) {
                    decimal64Value = temp.value();
                } else {
                    result->SetNulls(0, true, size);
                    return;
                }
                break;
            }
            case OMNI_INT:
            {
                auto temp = DecimalOperations::rescaleInt<int32_t, int64_t>(static_cast<ConstVector<int32_t> *>(input)->GetConstValue(), toPrecision, toScale);
                if (temp.has_value()) {
                    decimal64Value = temp.value();
                } else {
                    result->SetNulls(0, true, size);
                    return;
                }
                break;
            }
            case OMNI_LONG:
            {
                auto temp = DecimalOperations::rescaleInt<int64_t, int64_t>(static_cast<ConstVector<int64_t> *>(input)->GetConstValue(), toPrecision, toScale);
                if (temp.has_value()) {
                    decimal64Value = temp.value();
                } else {
                    result->SetNulls(0, true, size);
                    return;
                }
                break;
            }
            case OMNI_FLOAT:
            {
                auto status = DecimalOperations::rescaleFloatingPoint<float, int64_t>(static_cast<ConstVector<float> *>(input)->GetConstValue(), toPrecision, toScale, decimal64Value);
                if (!status.ok()) {
                    result->SetNulls(0, true, size);
                    return;
                }
                break;
            }
            case OMNI_DOUBLE:
            {
                auto status = DecimalOperations::rescaleFloatingPoint<double, int64_t>(static_cast<ConstVector<double> *>(input)->GetConstValue(), toPrecision, toScale, decimal64Value);
                if (!status.ok()) {
                    result->SetNulls(0, true, size);
                    return;
                }
                break;
            }
            case OMNI_CHAR:
            case OMNI_VARCHAR:
            {
                auto str = static_cast<ConstVector<std::string_view> *>(input)->GetConstValue();
                bool isNull = false;
                decimal64Value = StringToDecimal64(std::string(str), &isNull, toPrecision, toScale);
                if (isNull) {
                    result->SetNulls(0, true, size);
                    return;
                }
                break;
            }
            case OMNI_DECIMAL64:
            {
                auto temp = static_cast<ConstVector<int64_t> *>(input)->GetConstValue();
                auto fromPrecision = static_cast<DecimalDataType *>(fromType_.get())->GetPrecision();
                auto fromScale = static_cast<DecimalDataType *>(fromType_.get())->GetScale();
                const auto status = DecimalOperations::rescaleWithRoundUp<int64_t, int64_t>(temp, fromPrecision, fromScale, toPrecision, toScale, decimal64Value);
                if (!status.ok()) {
                    result->SetNulls(0, true, size);
                    return;
                }
                break;
            }
            case OMNI_DECIMAL128:
            {
                Decimal128 temp(static_cast<ConstVector<std::string_view> *>(input)->GetConstValue());
                auto fromPrecision = static_cast<DecimalDataType *>(fromType_.get())->GetPrecision();
                auto fromScale = static_cast<DecimalDataType *>(fromType_.get())->GetScale();
                const auto status = DecimalOperations::rescaleWithRoundUp<int128_t, int64_t>(temp.ToInt128(), fromPrecision, fromScale, toPrecision, toScale, decimal64Value);
                if (!status.ok()) {
                    result->SetNulls(0, true, size);
                    return;
                }
                break;
            }
            default:
                OMNI_THROW("Cast function Error", "Unsupported cast to decimal64 from " + TypeUtil::TypeToString(fromType_->GetId()));
        }
        for (int32_t row = 0; row < size; ++row) {
            resultVec->SetValue(row, decimal64Value);
        }
    } else {
        switch (fromType_->GetId()) {
            case OMNI_BOOLEAN: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    auto temp = DecimalOperations::rescaleInt<bool, int64_t>(VectorHelper::GetValueFromVector<bool>(input, row), toPrecision, toScale);
                    if (temp.has_value()) {
                        resultVec->SetValue(row, temp.value());
                    } else {
                        result->SetNull(row);
                    }
                }
                break;
            }
            case OMNI_BYTE: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    auto temp = DecimalOperations::rescaleInt<int8_t, int64_t>(VectorHelper::GetValueFromVector<int8_t>(input, row), toPrecision, toScale);
                    if (temp.has_value()) {
                        resultVec->SetValue(row, temp.value());
                    } else {
                        result->SetNull(row);
                    }
                }
                break;
            }
            case OMNI_SHORT: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    auto temp = DecimalOperations::rescaleInt<int16_t, int64_t>(VectorHelper::GetValueFromVector<int16_t>(input, row), toPrecision, toScale);
                    if (temp.has_value()) {
                        resultVec->SetValue(row, temp.value());
                    } else {
                        result->SetNull(row);
                    }
                }
                break;
            }
            case OMNI_INT: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    auto temp = DecimalOperations::rescaleInt<int32_t, int64_t>(VectorHelper::GetValueFromVector<int32_t>(input, row), toPrecision, toScale);
                    if (temp.has_value()) {
                        resultVec->SetValue(row, temp.value());
                    } else {
                        result->SetNull(row);
                    }
                }
                break;
            }
            case OMNI_LONG: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    auto temp = DecimalOperations::rescaleInt<int64_t, int64_t>(VectorHelper::GetValueFromVector<int64_t>(input, row), toPrecision, toScale);
                    if (temp.has_value()) {
                        resultVec->SetValue(row, temp.value());
                    } else {
                        result->SetNull(row);
                    }
                }
                break;
            }
            case OMNI_FLOAT: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    int64_t decimal64Value;
                    auto status = DecimalOperations::rescaleFloatingPoint<float, int64_t>(VectorHelper::GetValueFromVector<float>(input, row), toPrecision, toScale, decimal64Value);
                    if (status.ok()) {
                        resultVec->SetValue(row, decimal64Value);
                    } else {
                        result->SetNull(row);
                    }
                }
                break;
            }
            case OMNI_DOUBLE: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    int64_t decimal64Value;
                    auto status = DecimalOperations::rescaleFloatingPoint<double, int64_t>(VectorHelper::GetValueFromVector<double>(input, row), toPrecision, toScale, decimal64Value);
                    if (status.ok()) {
                        resultVec->SetValue(row, decimal64Value);
                    } else {
                        result->SetNull(row);
                    }
                }
                break;
            }
            case OMNI_CHAR:
            case OMNI_VARCHAR: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    auto str = VectorHelper::GetStringValueFromVector(input, row);
                    bool isNull = false;
                    int64_t decimal64Value = StringToDecimal64(std::string(str), &isNull, toPrecision, toScale);
                    if (isNull) {
                        result->SetNull(row);
                        continue;
                    }
                    resultVec->SetValue(row, decimal64Value);
                }
                break;
            }
            case OMNI_DECIMAL64: {
                auto fromPrecision = static_cast<DecimalDataType *>(fromType_.get())->GetPrecision();
                auto fromScale = static_cast<DecimalDataType *>(fromType_.get())->GetScale();
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    auto temp = VectorHelper::GetValueFromVector<int64_t>(input, row);
                    int64_t decimal64Value;
                    const auto status = DecimalOperations::rescaleWithRoundUp<int64_t, int64_t>(temp, fromPrecision, fromScale, toPrecision, toScale, decimal64Value);
                    if (status.ok()) {
                        resultVec->SetValue(row, decimal64Value);
                    } else {
                        result->SetNull(row);
                    }
                }
                break;
            }
            case OMNI_DECIMAL128: {
                auto fromPrecision = static_cast<DecimalDataType *>(fromType_.get())->GetPrecision();
                auto fromScale = static_cast<DecimalDataType *>(fromType_.get())->GetScale();
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    Decimal128 temp = VectorHelper::GetValueFromVector<Decimal128>(input, row);
                    int64_t decimal64Value;
                    const auto status = DecimalOperations::rescaleWithRoundUp<int128_t, int64_t>(temp.ToInt128(), fromPrecision, fromScale, toPrecision, toScale, decimal64Value);
                    if (status.ok()) {
                        resultVec->SetValue(row, decimal64Value);
                    } else {
                        result->SetNull(row);
                    }
                }
                break;
            }
            default:
                OMNI_THROW("Cast function Error", "Unsupported cast to decimal64 from " + TypeUtil::TypeToString(fromType_->GetId()));
        }
    }
}

int64_t CastFunction::StringToDecimal64(std::string str, bool *isNull, int32_t outPrecision, int32_t outScale) const{
    str.erase(0, str.find_first_not_of(' '));
    str.erase(str.find_last_not_of(' ') + 1);
    if (str.empty() || (str.size() == 1 && !isdigit(str[0]))) {
        *isNull = true;
        return 0;
    }
    fixDecimalStringInPlace(str);
    Decimal64<true> result(str);
    result.ReScale(outScale);
    if (result.IsOverflow(outPrecision) != OpStatus::SUCCESS) {
        *isNull = true;
        return 0;
    }
    return result.GetValue();
}

void CastFunction::CastToDecimal128(BaseVector* input, BaseVector*& result, ExecutionContext* context) const {
    auto size = context->GetResultRowSize();
    auto toDataType = static_cast<DecimalDataType *>(toType_.get());
    int toPrecision = toDataType->GetPrecision();
    int toScale = toDataType->GetScale();
    result = VectorHelper::CreateFlatVector(OMNI_DECIMAL128, size);
    auto* resultVec = static_cast<Vector<Decimal128>*>(result);
    if (input->GetEncoding() == OMNI_ENCODING_CONST) {
        int128_t decimal128Value;
        switch (fromType_->GetId()) {
            case OMNI_BOOLEAN:
            {
                auto temp = DecimalOperations::rescaleInt<bool, int128_t>(static_cast<ConstVector<bool> *>(input)->GetConstValue(), toPrecision, toScale);
                if (temp.has_value()) {
                    decimal128Value = temp.value();
                } else {
                    result->SetNulls(0, true, size);
                    return;
                }
                break;
            }
            case OMNI_BYTE:
            {
                auto temp = DecimalOperations::rescaleInt<int8_t, int128_t>(static_cast<ConstVector<int8_t> *>(input)->GetConstValue(), toPrecision, toScale);
                if (temp.has_value()) {
                    decimal128Value = temp.value();
                } else {
                    result->SetNulls(0, true, size);
                    return;
                }
                break;
            }
            case OMNI_SHORT:
            {
                auto temp = DecimalOperations::rescaleInt<int16_t, int128_t>(static_cast<ConstVector<int16_t> *>(input)->GetConstValue(), toPrecision, toScale);
                if (temp.has_value()) {
                    decimal128Value = temp.value();
                } else {
                    result->SetNulls(0, true, size);
                    return;
                }
                break;
            }
            case OMNI_INT:
            {
                auto temp = DecimalOperations::rescaleInt<int32_t, int128_t>(static_cast<ConstVector<int32_t> *>(input)->GetConstValue(), toPrecision, toScale);
                if (temp.has_value()) {
                    decimal128Value = temp.value();
                } else {
                    result->SetNulls(0, true, size);
                    return;
                }
                break;
            }
            case OMNI_LONG:
            {
                auto temp = DecimalOperations::rescaleInt<int64_t, int128_t>(static_cast<ConstVector<int64_t> *>(input)->GetConstValue(), toPrecision, toScale);
                if (temp.has_value()) {
                    decimal128Value = temp.value();
                } else {
                    result->SetNulls(0, true, size);
                    return;
                }
                break;
            }
            case OMNI_FLOAT:
            {
                auto status = DecimalOperations::rescaleFloatingPoint<float, int128_t>(static_cast<ConstVector<float> *>(input)->GetConstValue(), toPrecision, toScale, decimal128Value);
                if (!status.ok()) {
                    result->SetNulls(0, true, size);
                    return;
                }
                break;
            }
            case OMNI_DOUBLE:
            {
                auto status = DecimalOperations::rescaleFloatingPoint<double, int128_t>(static_cast<ConstVector<double> *>(input)->GetConstValue(), toPrecision, toScale, decimal128Value);
                if (!status.ok()) {
                    result->SetNulls(0, true, size);
                    return;
                }
                break;
            }
            case OMNI_CHAR:
            case OMNI_VARCHAR:
            {
                auto str = static_cast<ConstVector<std::string_view> *>(input)->GetConstValue();
                bool isNull = false;
                decimal128Value = StringToDecimal128(std::string(str), &isNull, toPrecision, toScale);
                if (isNull) {
                    result->SetNulls(0, true, size);
                    return;
                }
                break;
            }
            case OMNI_DECIMAL64:
            {
                auto temp = static_cast<ConstVector<int64_t> *>(input)->GetConstValue();
                auto fromPrecision = static_cast<DecimalDataType *>(fromType_.get())->GetPrecision();
                auto fromScale = static_cast<DecimalDataType *>(fromType_.get())->GetScale();
                const auto status = DecimalOperations::rescaleWithRoundUp<int64_t, int128_t>(temp, fromPrecision, fromScale, toPrecision, toScale, decimal128Value);
                if (!status.ok()) {
                    result->SetNulls(0, true, size);
                    return;
                }
                break;
            }
            case OMNI_DECIMAL128:
            {
                Decimal128 temp(static_cast<ConstVector<std::string_view> *>(input)->GetConstValue());
                auto fromPrecision = static_cast<DecimalDataType *>(fromType_.get())->GetPrecision();
                auto fromScale = static_cast<DecimalDataType *>(fromType_.get())->GetScale();
                const auto status = DecimalOperations::rescaleWithRoundUp<int128_t, int128_t>(temp.ToInt128(), fromPrecision, fromScale, toPrecision, toScale, decimal128Value);
                if (!status.ok()) {
                    result->SetNulls(0, true, size);
                    return;
                }
                break;
            }
            default:
                OMNI_THROW("Cast function Error", "Unsupported cast to decimal128 from " + TypeUtil::TypeToString(fromType_->GetId()));
        }
        for (int32_t row = 0; row < size; ++row) {
            resultVec->SetValue(row, Decimal128(decimal128Value));
        }
    } else {
        switch (fromType_->GetId()) {
            case OMNI_BOOLEAN: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    auto temp = DecimalOperations::rescaleInt<bool, int128_t>(VectorHelper::GetValueFromVector<bool>(input, row), toPrecision, toScale);
                    if (temp.has_value()) {
                        resultVec->SetValue(row, Decimal128(temp.value()));
                    } else {
                        result->SetNull(row);
                    }
                }
                break;
            }
            case OMNI_BYTE: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    auto temp = DecimalOperations::rescaleInt<int8_t, int128_t>(VectorHelper::GetValueFromVector<int8_t>(input, row), toPrecision, toScale);
                    if (temp.has_value()) {
                        resultVec->SetValue(row, Decimal128(temp.value()));
                    } else {
                        result->SetNull(row);
                    }
                }
                break;
            }
            case OMNI_SHORT: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    auto temp = DecimalOperations::rescaleInt<int16_t, int128_t>(VectorHelper::GetValueFromVector<int16_t>(input, row), toPrecision, toScale);
                    if (temp.has_value()) {
                        resultVec->SetValue(row, Decimal128(temp.value()));
                    } else {
                        result->SetNull(row);
                    }
                }
                break;
            }
            case OMNI_INT: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    auto temp = DecimalOperations::rescaleInt<int32_t, int128_t>(VectorHelper::GetValueFromVector<int32_t>(input, row), toPrecision, toScale);
                    if (temp.has_value()) {
                        resultVec->SetValue(row, Decimal128(temp.value()));
                    } else {
                        result->SetNull(row);
                    }
                }
                break;
            }
            case OMNI_LONG: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    auto temp = DecimalOperations::rescaleInt<int64_t, int128_t>(VectorHelper::GetValueFromVector<int64_t>(input, row), toPrecision, toScale);
                    if (temp.has_value()) {
                        resultVec->SetValue(row, Decimal128(temp.value()));
                    } else {
                        result->SetNull(row);
                    }
                }
                break;
            }
            case OMNI_FLOAT: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    int128_t decimal128Value;
                    auto status = DecimalOperations::rescaleFloatingPoint<float, int128_t>(VectorHelper::GetValueFromVector<float>(input, row), toPrecision, toScale, decimal128Value);
                    if (status.ok()) {
                        resultVec->SetValue(row, Decimal128(decimal128Value));
                    } else {
                        result->SetNull(row);
                    }
                }
                break;
            }
            case OMNI_DOUBLE: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    int128_t decimal128Value;
                    auto status = DecimalOperations::rescaleFloatingPoint<double, int128_t>(VectorHelper::GetValueFromVector<double>(input, row), toPrecision, toScale, decimal128Value);
                    if (status.ok()) {
                        resultVec->SetValue(row, Decimal128(decimal128Value));
                    } else {
                        result->SetNull(row);
                    }
                }
                break;
            }
            case OMNI_CHAR:
            case OMNI_VARCHAR: {
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    auto str = VectorHelper::GetStringValueFromVector(input, row);
                    bool isNull = false;
                    int128_t decimal128Value = StringToDecimal128(std::string(str), &isNull, toPrecision, toScale);
                    if (isNull) {
                        result->SetNull(row);
                        continue;
                    }
                    resultVec->SetValue(row, Decimal128(decimal128Value));
                }
                break;
            }
            case OMNI_DECIMAL64: {
                auto fromPrecision = static_cast<DecimalDataType *>(fromType_.get())->GetPrecision();
                auto fromScale = static_cast<DecimalDataType *>(fromType_.get())->GetScale();
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    auto temp = VectorHelper::GetValueFromVector<int64_t>(input, row);
                    int128_t decimal128Value;
                    const auto status = DecimalOperations::rescaleWithRoundUp<int64_t, int128_t>(temp, fromPrecision, fromScale, toPrecision, toScale, decimal128Value);
                    if (status.ok()) {
                        resultVec->SetValue(row, Decimal128(decimal128Value));
                    } else {
                        result->SetNull(row);
                    }
                }
                break;
            }
            case OMNI_DECIMAL128: {
                auto fromPrecision = static_cast<DecimalDataType *>(fromType_.get())->GetPrecision();
                auto fromScale = static_cast<DecimalDataType *>(fromType_.get())->GetScale();
                for (int32_t row = 0; row < size; ++row) {
                    if (input->IsNull(row)) { result->SetNull(row); continue; }
                    Decimal128 temp = VectorHelper::GetValueFromVector<Decimal128>(input, row);
                    int128_t decimal128Value;
                    const auto status = DecimalOperations::rescaleWithRoundUp<int128_t, int128_t>(temp.ToInt128(), fromPrecision, fromScale, toPrecision, toScale, decimal128Value);
                    if (status.ok()) {
                        resultVec->SetValue(row, Decimal128(decimal128Value));
                    } else {
                        result->SetNull(row);
                    }
                }
                break;
            }
            default:
                OMNI_THROW("Cast function Error", "Unsupported cast to decimal128 from " + TypeUtil::TypeToString(fromType_->GetId()));
        }
    }
}

int128_t CastFunction::StringToDecimal128(std::string str, bool *isNull, int32_t outPrecision, int32_t outScale) const {
    str.erase(0, str.find_first_not_of(' '));
    str.erase(str.find_last_not_of(' ') + 1);
    if (str.empty() || (str.size() == 1 && !isdigit(str[0]))) {
        *isNull = true;
        return 0;
    }
    fixDecimalStringInPlace(str);
    if (!regex_match(str, g_decimalRegex)) {
        *isNull = true;
        return 0;
    }
    Decimal128Wrapper<true> result(str.c_str());
    result.ReScale(outScale);
    if (result.IsOverflow(outPrecision) != OpStatus::SUCCESS) {
        *isNull = true;
        return 0;
    }
    return result.ToInt128();
}

void CastFunction::CastToBinary(BaseVector* input, BaseVector*& result, ExecutionContext* context) const {
    switch (fromType_->GetId()) {
        case OMNI_BYTE:
            CastIntegerToBinary<int8_t>(input, result, context);
            break;
        case OMNI_SHORT:
            CastIntegerToBinary<int16_t>(input, result, context);
            break;
        case OMNI_INT:
            CastIntegerToBinary<int32_t>(input, result, context);
            break;
        case OMNI_LONG:
            CastIntegerToBinary<int64_t>(input, result, context);
            break;
        case OMNI_CHAR:
        case OMNI_VARCHAR:
        case OMNI_VARBINARY:
            // String to string, just copy
            result = input;
            break;
        default:
            OMNI_THROW("Cast function Error", "Unsupported cast to binary from " + TypeUtil::TypeToString(fromType_->GetId()));
    }
}

template <typename TInput>
void CastFunction::CastIntegerToBinary(BaseVector* input, BaseVector*& result, ExecutionContext* context) const
{
    const auto size = context->GetResultRowSize();
    result = VectorHelper::CreateFlatVector(OMNI_VARBINARY, size);
    auto *out = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(result);

    // Keep consistent with CastExpr::applyIntToBinaryCast: encode in big-endian.
    char inlined[sizeof(TInput)];

    auto setRow = [&](int32_t row, TInput v) {
        if constexpr (std::is_same_v<TInput, int8_t>) {
            inlined[0] = static_cast<char>(v & 0xFF);
        } else {
            for (int i = sizeof(TInput) - 1; i >= 0; --i) {
                inlined[i] = static_cast<char>(v & 0xFF);
                v >>= 8;
            }
        }
        out->SetValue(row, std::string_view(inlined, sizeof(TInput)));
    };

    if (input->GetEncoding() == OMNI_ENCODING_CONST) {
        auto *constVec = static_cast<ConstVector<TInput> *>(input);
        const auto v = constVec->GetConstValue();
        for (int32_t row = 0; row < size; ++row) {
            setRow(row, v);
        }
        return;
    }

    for (int32_t row = 0; row < size; ++row) {
        if (input->IsNull(row)) {
            result->SetNull(row);
            continue;
        }
        setRow(row, VectorHelper::GetValueFromVector<TInput>(input, row));
    }
}

template void CastFunction::CastIntegerToBinary<int8_t>(BaseVector*, BaseVector*&, ExecutionContext*) const;
template void CastFunction::CastIntegerToBinary<int16_t>(BaseVector*, BaseVector*&, ExecutionContext*) const;
template void CastFunction::CastIntegerToBinary<int32_t>(BaseVector*, BaseVector*&, ExecutionContext*) const;
template void CastFunction::CastIntegerToBinary<int64_t>(BaseVector*, BaseVector*&, ExecutionContext*) const;

void CastFunction::fixDecimalStringInPlace(std::string& str) const{
    if (!str.empty()) {
        if (str.front() == '.') {
            str.insert(str.begin(), '0');
        }
        if (str.back() == '.') {
            str.push_back('0');
        }
    }
}

std::string_view CastFunction::extractDigits(const char *s, size_t start, size_t size) const
{
    size_t pos = start;
    for (; pos < size; ++pos) {
        if (!std::isdigit(s[pos])) {
            break;
        }
    }
    return std::string_view(s + start, pos - start);
}

Status CastFunction::parseDecimalComponents(const char *s, size_t size, DecimalComponents &out) const
{
    if (size == 0) {
        return Status::UserError("Input is empty.");
    }

    size_t pos = 0;

    // Sign of the number.
    if (s[pos] == '-') {
        out.sign = -1;
        ++pos;
    } else if (s[pos] == '+') {
        out.sign = 1;
        ++pos;
    }

    // Extract the whole digits.
    out.wholeDigits = extractDigits(s, pos, size);
    pos += out.wholeDigits.size();
    if (pos == size) {
        return out.wholeDigits.empty() ? Status::UserError("Extracted digits are empty.") : Status::OK();
    }

    // Optional dot (if given in fractional form).
    if (s[pos] == '.') {
        // Extract the fractional digits.
        ++pos;
        out.fractionalDigits = extractDigits(s, pos, size);
        pos += out.fractionalDigits.size();
    }

    if (out.wholeDigits.empty() && out.fractionalDigits.empty()) {
        return Status::UserError("Extracted digits are empty.");
    }
    if (pos == size) {
        return Status::OK();
    }
    // Optional exponent.
    if (s[pos] == 'e' || s[pos] == 'E') {
        ++pos;
        bool withSign = pos < size && (s[pos] == '+' || s[pos] == '-');
        if (withSign && pos == size - 1) {
            return Status::UserError("The exponent part only contains sign.");
        }
        // Make sure all chars after sign are digits, as as folly::tryTo allows
        // leading and trailing whitespaces.
        for (auto i = static_cast<size_t>(withSign); i < size - pos; ++i) {
            if (!std::isdigit(s[pos + i])) {
                return Status::UserError("Non-digit character '{}' is not allowed in the exponent part.", s[pos + i]);
            }
        }
        out.exponent = folly::to<int32_t>(folly::StringPiece(s + pos, size - pos));
        return Status::OK();
    }
    return pos == size ? Status::OK() : Status::UserError("Chars '{}' are invalid.", std::string(s + pos, size - pos));
}

Status CastFunction::parseHugeInt(const DecimalComponents &decimalComponents, int128_t &out) const
{
    // Parse the whole digits.
    if (decimalComponents.wholeDigits.size() > 0) {
        const auto tryValue = folly::tryTo<int128_t>(folly::StringPiece(decimalComponents.wholeDigits.data(),
            decimalComponents.wholeDigits.size()));
        if (tryValue.hasError()) {
            return Status::UserError("Value too large.");
        }
        out = tryValue.value();
    }

    // Parse the fractional digits.
    if (decimalComponents.fractionalDigits.size() > 0) {
        const auto length = decimalComponents.fractionalDigits.size();
        bool overflow = __builtin_mul_overflow(out, TenOfScaleMultipliers[length], &out);
        if (overflow) {
            return Status::UserError("Value too large.");
        }
        const auto tryValue = folly::tryTo<int128_t>(folly::StringPiece(decimalComponents.fractionalDigits.data(),
            length));
        if (tryValue.hasError()) {
            return Status::UserError("Value too large.");
        }
        overflow = __builtin_add_overflow(out, tryValue.value(), &out);
    }
    return Status::OK();
}



template <typename T>
Status CastFunction::toDecimalValue(const std::string_view s, int toPrecision, int toScale, T &decimalValue) const
{
    DecimalComponents decimalComponents;
    auto checkStatus = parseDecimalComponents(s.data(), s.size(), decimalComponents);
    if ( !checkStatus.ok() || !regex_match(s.data(), g_decimalRegex)) {
        return Status::UserError("Value is not a number. " + std::string(s.data(), s.size()));
    }

    // Count number of significant digits (without leading zeros).
    const size_t firstNonZero = decimalComponents.wholeDigits.find_first_not_of('0');
    size_t significantDigits = decimalComponents.fractionalDigits.size();
    if (firstNonZero != std::string::npos) {
        significantDigits += decimalComponents.wholeDigits.size() - firstNonZero;
    }
    int32_t parsedPrecision = static_cast<int32_t>(significantDigits);

    int32_t parsedScale = 0;
    bool roundUp = false;
    const int32_t fractionSize = decimalComponents.fractionalDigits.size();
    if (!decimalComponents.exponent.has_value()) {
        if (fractionSize > toScale) {
            if (decimalComponents.fractionalDigits[toScale] >= '5') {
                roundUp = true;
            }
            parsedScale = toScale;
            decimalComponents.fractionalDigits = std::string_view(decimalComponents.fractionalDigits.data(), toScale);
        } else {
            parsedScale = fractionSize;
        }
    } else {
        const auto exponent = decimalComponents.exponent.value();
        parsedScale = -exponent + fractionSize;
        // Truncate the fractionalDigits.
        if (parsedScale > toScale) {
            if (-exponent >= toScale) {
                // The fractional digits could be dropped.
                if (fractionSize > 0 && decimalComponents.fractionalDigits[0] >= '5') {
                    roundUp = true;
                }
                decimalComponents.fractionalDigits = "";
                parsedScale -= fractionSize;
            } else {
                const auto reduceDigits = exponent + toScale;
                if (fractionSize > reduceDigits && decimalComponents.fractionalDigits[reduceDigits] >= '5') {
                    roundUp = true;
                }
                decimalComponents.fractionalDigits = std::string_view(decimalComponents.fractionalDigits.data(),
                    std::min(reduceDigits, fractionSize));
                parsedScale -= fractionSize - decimalComponents.fractionalDigits.size();
            }
        }
    }

    int128_t out = 0;
    if (auto status = parseHugeInt(decimalComponents, out); !status.ok()) {
        return status;
    }

    if (roundUp) {
        bool overflow = __builtin_add_overflow(out, 1, &out);
        if (UNLIKELY(overflow)) {
            return Status::UserError("Value too large.");
        }
    }
    out *= decimalComponents.sign;

    if (parsedScale < 0) {
        /// Force the scale to be zero, to avoid negative scales (due to
        /// compatibility issues with external systems such as databases).
        if (-parsedScale + toScale > Decimal128Wrapper<>::kMaxPrecision) {
            return Status::UserError("Value too large.");
        }

        bool overflow = __builtin_mul_overflow(out, kPowersOfTen[-parsedScale + toScale], &out);
        if (UNLIKELY(overflow)) {
            return Status::UserError("Value too large.");
        }
        parsedPrecision -= parsedScale;
        parsedScale = toScale;
    }
    const auto status = DecimalOperations::rescaleWithRoundUp<int128_t, T>(out,
        std::min(static_cast<uint8_t>(parsedPrecision), Decimal128Wrapper<>::kMaxPrecision), parsedScale, toPrecision,
        toScale, decimalValue);
    if (!status.ok()) {
        return Status::UserError("Value too large.");
    }
    return status;
}
}
