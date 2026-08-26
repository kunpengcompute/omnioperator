/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
* Description: NullIf function implementation for vectorized conditional expressions
*/

#include "NullIf.h"
#include "vector/vector.h"

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;

void NullIfFunction::Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType,
                           BaseVector *&result, ExecutionContext *context) const
{
    if (args.size() < 2) {
        OMNI_THROW("NullIf function Error", "NullIf requires exactly 2 arguments");
    }

    // Pop arguments from stack (stack order: expr2 on top, expr1 below)
    auto *expr2Vec = args.top();
    args.pop();
    auto *expr1Vec = args.top();
    args.pop();

    DispatchNullIf(expr1Vec, expr2Vec, outputType, result);

    delete expr1Vec;
    delete expr2Vec;
}

void NullIfFunction::DispatchNullIf(BaseVector *expr1Vec, BaseVector *expr2Vec,
                                    const DataTypePtr &outputType, BaseVector *&result) const
{
    DataTypeId outputTypeId = outputType->GetId();

    if (TypeUtil::IsStringType(outputTypeId)) {
        NullIfString(expr1Vec, expr2Vec, result, outputType);
        return;
    }

    switch (outputTypeId) {
        case OMNI_BOOLEAN:
            NullIfNumeric<bool>(expr1Vec, expr2Vec, result, outputType);
            break;
        case OMNI_BYTE:
            NullIfNumeric<int8_t>(expr1Vec, expr2Vec, result, outputType);
            break;
        case OMNI_SHORT:
            NullIfNumeric<int16_t>(expr1Vec, expr2Vec, result, outputType);
            break;
        case OMNI_INT:
        case OMNI_DATE32:
            NullIfNumeric<int32_t>(expr1Vec, expr2Vec, result, outputType);
            break;
        case OMNI_LONG:
        case OMNI_DATE64:
        case OMNI_TIMESTAMP:
        case OMNI_DECIMAL64:
            NullIfNumeric<int64_t>(expr1Vec, expr2Vec, result, outputType);
            break;
        case OMNI_FLOAT:
            NullIfNumeric<float>(expr1Vec, expr2Vec, result, outputType);
            break;
        case OMNI_DOUBLE:
            NullIfNumeric<double>(expr1Vec, expr2Vec, result, outputType);
            break;
        case OMNI_DECIMAL128:
            NullIfNumeric<Decimal128>(expr1Vec, expr2Vec, result, outputType);
            break;
        default:
            OMNI_THROW("NullIf function Error",
                       "Unsupported output type: " + TypeUtil::TypeToString(outputTypeId));
    }
}

template<typename T>
void NullIfFunction::NullIfNumeric(BaseVector *expr1Vec, BaseVector *expr2Vec, BaseVector *&result,
                                   const DataTypePtr &outputType) const
{
    auto size = expr1Vec->GetSize();
    result = VectorHelper::CreateFlatVector(outputType->GetId(), size);

    for (int32_t row = 0; row < size; ++row) {
        // If expr1 is NULL, result is NULL
        if (expr1Vec->IsNull(row)) {
            result->SetNull(row);
            continue;
        }

        T expr1Value = GetValueFromVector<T>(expr1Vec, row);

        // If expr2 is NULL, expr1 != expr2, return expr1
        if (expr2Vec->IsNull(row)) {
            SetValueToVector(result, row, expr1Value);
            continue;
        }

        T expr2Value = GetValueFromVector<T>(expr2Vec, row);

        // If equal, return NULL; otherwise return expr1
        if (expr1Value == expr2Value) {
            result->SetNull(row);
        } else {
            SetValueToVector(result, row, expr1Value);
        }
    }
}

void NullIfFunction::NullIfString(BaseVector *expr1Vec, BaseVector *expr2Vec, BaseVector *&result,
                                  const DataTypePtr &outputType) const
{
    auto size = expr1Vec->GetSize();
    result = VectorHelper::CreateFlatVector(outputType->GetId(), size);

    for (int32_t row = 0; row < size; ++row) {
        // If expr1 is NULL, result is NULL
        if (expr1Vec->IsNull(row)) {
            result->SetNull(row);
            continue;
        }

        std::string_view expr1Value = GetStringValueFromVector(expr1Vec, row);

        // If expr2 is NULL, expr1 != expr2, return expr1
        if (expr2Vec->IsNull(row)) {
            SetStringValueToVector(result, row, expr1Value);
            continue;
        }

        std::string_view expr2Value = GetStringValueFromVector(expr2Vec, row);

        // If equal, return NULL; otherwise return expr1
        if (expr1Value == expr2Value) {
            result->SetNull(row);
        } else {
            SetStringValueToVector(result, row, expr1Value);
        }
    }
}

template<typename T>
T NullIfFunction::GetValueFromVector(BaseVector *vec, int32_t row) const
{
    Encoding encoding = vec->GetEncoding();

    if (encoding == OMNI_ENCODING_CONST) {
        auto *constVec = static_cast<ConstVector<T> *>(vec);
        return constVec->GetConstValue();
    } else if (encoding == OMNI_FLAT) {
        auto *flatVec = static_cast<Vector<T> *>(vec);
        return flatVec->GetValue(row);
    } else if (encoding == OMNI_DICTIONARY) {
        auto *dictVec = static_cast<Vector<DictionaryContainer<T>> *>(vec);
        return dictVec->GetValue(row);
    } else {
        OMNI_THROW("NullIf function Error", "Unsupported encoding type");
    }
}

std::string_view NullIfFunction::GetStringValueFromVector(BaseVector *vec, int32_t row) const
{
    Encoding encoding = vec->GetEncoding();

    if (encoding == OMNI_ENCODING_CONST) {
        auto *constVec = static_cast<ConstVector<std::string_view> *>(vec);
        return constVec->GetConstValue();
    } else if (encoding == OMNI_FLAT) {
        auto *flatVec = static_cast<Vector<LargeStringContainer<std::string_view>> *>(vec);
        return flatVec->GetValue(row);
    } else if (encoding == OMNI_DICTIONARY) {
        auto *dictVec = static_cast<Vector<DictionaryContainer<std::string_view, LargeStringContainer>> *>(vec);
        return dictVec->GetValue(row);
    } else {
        OMNI_THROW("NullIf function Error", "Unsupported encoding type for string");
    }
}

template<typename T>
void NullIfFunction::SetValueToVector(BaseVector *vec, int32_t row, const T &value) const
{
    auto *resultVec = static_cast<Vector<T> *>(vec);
    resultVec->SetValue(row, value);
}

void NullIfFunction::SetStringValueToVector(BaseVector *vec, int32_t row,
                                            const std::string_view &value) const
{
    auto *resultVec = static_cast<Vector<LargeStringContainer<std::string_view>> *>(vec);
    resultVec->SetValue(row, value);
    resultVec->SetNotNull(row);
}

// Explicit template instantiations
template void NullIfFunction::NullIfNumeric<bool>(BaseVector *, BaseVector *, BaseVector *&, const DataTypePtr &) const;
template void NullIfFunction::NullIfNumeric<int8_t>(BaseVector *, BaseVector *, BaseVector *&, const DataTypePtr &) const;
template void NullIfFunction::NullIfNumeric<int16_t>(BaseVector *, BaseVector *, BaseVector *&, const DataTypePtr &) const;
template void NullIfFunction::NullIfNumeric<int32_t>(BaseVector *, BaseVector *, BaseVector *&, const DataTypePtr &) const;
template void NullIfFunction::NullIfNumeric<int64_t>(BaseVector *, BaseVector *, BaseVector *&, const DataTypePtr &) const;
template void NullIfFunction::NullIfNumeric<float>(BaseVector *, BaseVector *, BaseVector *&, const DataTypePtr &) const;
template void NullIfFunction::NullIfNumeric<double>(BaseVector *, BaseVector *, BaseVector *&, const DataTypePtr &) const;
template void NullIfFunction::NullIfNumeric<Decimal128>(BaseVector *, BaseVector *, BaseVector *&, const DataTypePtr &) const;

template bool NullIfFunction::GetValueFromVector<bool>(BaseVector *, int32_t) const;
template int8_t NullIfFunction::GetValueFromVector<int8_t>(BaseVector *, int32_t) const;
template int16_t NullIfFunction::GetValueFromVector<int16_t>(BaseVector *, int32_t) const;
template int32_t NullIfFunction::GetValueFromVector<int32_t>(BaseVector *, int32_t) const;
template int64_t NullIfFunction::GetValueFromVector<int64_t>(BaseVector *, int32_t) const;
template float NullIfFunction::GetValueFromVector<float>(BaseVector *, int32_t) const;
template double NullIfFunction::GetValueFromVector<double>(BaseVector *, int32_t) const;
template Decimal128 NullIfFunction::GetValueFromVector<Decimal128>(BaseVector *, int32_t) const;

template void NullIfFunction::SetValueToVector<bool>(BaseVector *, int32_t, const bool &) const;
template void NullIfFunction::SetValueToVector<int8_t>(BaseVector *, int32_t, const int8_t &) const;
template void NullIfFunction::SetValueToVector<int16_t>(BaseVector *, int32_t, const int16_t &) const;
template void NullIfFunction::SetValueToVector<int32_t>(BaseVector *, int32_t, const int32_t &) const;
template void NullIfFunction::SetValueToVector<int64_t>(BaseVector *, int32_t, const int64_t &) const;
template void NullIfFunction::SetValueToVector<float>(BaseVector *, int32_t, const float &) const;
template void NullIfFunction::SetValueToVector<double>(BaseVector *, int32_t, const double &) const;
template void NullIfFunction::SetValueToVector<Decimal128>(BaseVector *, int32_t, const Decimal128 &) const;

}
