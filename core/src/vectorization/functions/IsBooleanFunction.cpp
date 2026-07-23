/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Unified IS [NOT] {TRUE|FALSE} function implementation.
 */

#include "IsBooleanFunction.h"
#include "vector/vector.h"

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;

void IsBooleanFunction::Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType,
                              BaseVector *&result, ExecutionContext *context) const
{
    auto inputVec = args.top();
    args.pop();

    DataTypeId inputTypeId = inputVec->GetTypeId();
    if (inputTypeId != OMNI_BOOLEAN) {
        OMNI_THROW("IsBoolean function Error", "Unsupported input type, only supports BOOLEAN");
    }

    auto size = inputVec->GetSize();
    result = VectorHelper::CreateFlatVector(OMNI_BOOLEAN, size);
    auto *resultVec = static_cast<Vector<bool> *>(result);

    for (int32_t row = 0; row < size; ++row) {
        if (inputVec->IsNull(row)) {
            resultVec->SetValue(row, nullResult_);
            continue;
        }
        bool value = GetValueFromVector(inputVec, row);
        resultVec->SetValue(row, negateValue_ ? !value : value);
    }
    delete inputVec;
}

bool IsBooleanFunction::GetValueFromVector(BaseVector *vec, int32_t row) {
    Encoding encoding = vec->GetEncoding();
    if (encoding == OMNI_ENCODING_CONST) {
        auto *constVec = static_cast<ConstVector<bool> *>(vec);
        return constVec->GetConstValue();
    } else if (encoding == OMNI_FLAT) {
        auto *flatVec = static_cast<Vector<bool> *>(vec);
        return flatVec->GetValue(row);
    } else if (encoding == OMNI_DICTIONARY) {
        auto *dictVec = static_cast<Vector<DictionaryContainer<bool>> *>(vec);
        return dictVec->GetValue(row);
    } else {
        OMNI_THROW("IsBoolean function Error", "Unsupported encoding type");
    }
}
}
