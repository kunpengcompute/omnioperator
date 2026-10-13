/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 */

#pragma once
#include "vectorization/VectorFunction.h"
#include "type/data_operations.h"
#include "vector/vector_helper.h"
#include <cmath>

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;
using namespace omniruntime::op;

class IsNanFunction : public VectorFunction {
public:
    explicit IsNanFunction() {}

    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        ExecutionContext *context) const override;

private:
    // Pool is threaded through to the implementation.
    template<typename T>
    void IsNanNumeric(BaseVector *inputVec, BaseVector *&result,
        omniruntime::mem::OmniTaskMemoryPool *pool) const;

    template<typename T>
    T GetValueFromVector(BaseVector *vec, int32_t row) const;
};
}
