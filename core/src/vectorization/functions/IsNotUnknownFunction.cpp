/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: IS NOT UNKNOWN function implementation.
 */

#include "IsNotUnknownFunction.h"
#include "vector/vector.h"

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;

void IsNotUnknownFunction::Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType,
                                 BaseVector *&result, ExecutionContext *context) const
{
    auto inputVec = args.top();
    args.pop();

    DataTypeId inputTypeId = inputVec->GetTypeId();
    if (inputTypeId != OMNI_BOOLEAN) {
        OMNI_THROW("IsNotUnknown function Error", "Unsupported input type, only supports BOOLEAN");
    }

    auto size = inputVec->GetSize();
    result = VectorHelper::CreateFlatVector(OMNI_BOOLEAN, size);
    auto *resultVec = static_cast<Vector<bool> *>(result);

    for (int32_t row = 0; row < size; ++row) {
        resultVec->SetValue(row, !inputVec->IsNull(row));
    }
    delete inputVec;
}
}
