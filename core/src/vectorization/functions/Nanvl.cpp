/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
* Description: NaNvl function implementation for vectorized conditional expressions
*/

#include "Nanvl.h"
#include "vector/vector.h"
#include <limits>
#include <cmath>

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;

void NanvlFunction::Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType,
                            BaseVector *&result, ExecutionContext *context) const
{
    if (args.size() < 2) {
        OMNI_THROW("Nanvl function Error", "Nanvl requires exactly 2 arguments");
    }

    // Pop arguments from stack (stack order: expr2 on top, expr1 below)
    auto expr2Vec = args.top();
    args.pop();
    auto expr1Vec = args.top();
    args.pop();

    DataTypeId outputTypeId = outputType->GetId();

    switch (outputTypeId) {
        case OMNI_FLOAT:
            NanvlNumeric<float>(expr1Vec, expr2Vec, result, outputType, context->GetPool());
            break;
        case OMNI_DOUBLE:
            NanvlNumeric<double>(expr1Vec, expr2Vec, result, outputType, context->GetPool());
            break;
        default:
            delete expr1Vec;
            delete expr2Vec;
            OMNI_THROW("Nanvl function Error",
                        "Unsupported output type, nanvl only supports FLOAT and DOUBLE");
    }

    delete expr1Vec;
    delete expr2Vec;
}

template<typename T>
void NanvlFunction::NanvlNumeric(BaseVector *expr1Vec, BaseVector *expr2Vec, BaseVector *&result,
                                    const DataTypePtr &outputType,
                                    omniruntime::mem::OmniTaskMemoryPool *pool) const
{
    auto size = expr1Vec->GetSize();
    result = VectorHelper::CreateFlatVector(pool, outputType->GetId(), size);

    for (int32_t row = 0; row < size; ++row) {
        // If expr1 is NULL, result is NULL
        if (expr1Vec->IsNull(row)) {
            result->SetNull(row);
            continue;
        }

        T expr1Value = GetValueFromVector<T>(expr1Vec, row);

        // If expr1 is not NaN, return expr1
        if (!std::isnan(expr1Value)) {
            SetValueToVector(result, row, expr1Value);
        } else {
            // expr1 is NaN, return expr2
            if (expr2Vec->IsNull(row)) {
                result->SetNull(row);
            } else {
                T expr2Value = GetValueFromVector<T>(expr2Vec, row);
                SetValueToVector(result, row, expr2Value);
            }
        }
    }
}

template<typename T>
T NanvlFunction::GetValueFromVector(BaseVector *vec, int32_t row) const
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
        OMNI_THROW("Nanvl function Error", "Unsupported encoding type");
    }
}

template<typename T>
void NanvlFunction::SetValueToVector(BaseVector *vec, int32_t row, const T &value) const
{
    auto *resultVec = static_cast<Vector<T> *>(vec);
    resultVec->SetValue(row, value);
}

// Explicit template instantiations
template void NanvlFunction::NanvlNumeric<float>(BaseVector *, BaseVector *, BaseVector *&, const DataTypePtr &, omniruntime::mem::OmniTaskMemoryPool *) const;
template void NanvlFunction::NanvlNumeric<double>(BaseVector *, BaseVector *, BaseVector *&, const DataTypePtr &, omniruntime::mem::OmniTaskMemoryPool *) const;

template float NanvlFunction::GetValueFromVector<float>(BaseVector *, int32_t) const;
template double NanvlFunction::GetValueFromVector<double>(BaseVector *, int32_t) const;

template void NanvlFunction::SetValueToVector<float>(BaseVector *, int32_t, const float &) const;
template void NanvlFunction::SetValueToVector<double>(BaseVector *, int32_t, const double &) const;

}
