/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 */

#include "IsNanFunction.h"
#include "vector/vector.h"
#include <cmath>

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;

void IsNanFunction::Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType,
                          BaseVector *&result, ExecutionContext *context) const
{
    auto inputVec = args.top();
    args.pop();
    auto inputGuard = MakeVectorArgGuard(inputVec);

    DataTypeId inputTypeId = inputVec->GetTypeId();

    switch (inputTypeId) {
        case OMNI_FLOAT:
            IsNanNumeric<float>(inputVec, result);
            break;
        case OMNI_DOUBLE:
            IsNanNumeric<double>(inputVec, result);
            break;
        default:
            OMNI_THROW("IsNan function Error", "Unsupported input type, isnan only supports FLOAT and DOUBLE");
    }
}

template<typename T>
void IsNanFunction::IsNanNumeric(BaseVector *inputVec, BaseVector *&result) const
{
    auto size = inputVec->GetSize();
    result = VectorHelper::CreateFlatVector(OMNI_BOOLEAN, size);
    auto *resultVec = static_cast<Vector<bool> *>(result);

    for (int32_t row = 0; row < size; ++row) {
        if (inputVec->IsNull(row)) {
            resultVec->SetValue(row, false);
            continue;
        }

        T value = GetValueFromVector<T>(inputVec, row);
        resultVec->SetValue(row, std::isnan(value));
    }
}

template<typename T>
T IsNanFunction::GetValueFromVector(BaseVector *vec, int32_t row) const
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
        OMNI_THROW("IsNan function Error", "Unsupported encoding type");
    }
}

template void IsNanFunction::IsNanNumeric<float>(BaseVector *, BaseVector *&) const;
template void IsNanFunction::IsNanNumeric<double>(BaseVector *, BaseVector *&) const;

template float IsNanFunction::GetValueFromVector<float>(BaseVector *, int32_t) const;
template double IsNanFunction::GetValueFromVector<double>(BaseVector *, int32_t) const;

}
