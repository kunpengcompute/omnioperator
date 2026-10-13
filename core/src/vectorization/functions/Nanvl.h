/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
* Description: NaNvl function for conditional expressions
*/

#pragma once
#include "vectorization/VectorFunction.h"
#include "vector/array_vector.h"
#include "type/data_operations.h"
#include "util/debug.h"
#include "util/type_util.h"
#include "vector/vector_helper.h"
#include <vector>
#include <cmath>

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;
using namespace omniruntime::op;

/// nanvl function
/// nanvl(expr1, expr2) -> same type as expr1
/// Returns expr1 if it is not NaN, or expr2 otherwise.
/// If expr1 is NULL, returns NULL.
/// Supports Float and Double types only.
class NanvlFunction : public VectorFunction {
public:
    explicit NanvlFunction() {}

    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        ExecutionContext *context) const override;

private:
    // Pool is threaded through to the implementation.
    template<typename T>
    void NanvlNumeric(BaseVector *expr1Vec, BaseVector *expr2Vec, BaseVector *&result,
                        const DataTypePtr &outputType,
                        omniruntime::mem::OmniTaskMemoryPool *pool) const;

    template<typename T>
    T GetValueFromVector(BaseVector *vec, int32_t row) const;

    template<typename T>
    void SetValueToVector(BaseVector *vec, int32_t row, const T &value) const;
};
}