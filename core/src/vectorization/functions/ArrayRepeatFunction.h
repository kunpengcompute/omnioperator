/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: ArrayRepeat function implementation
 */

#pragma once

#include <vector>
#include <stack>
#include <cstdint>
#include "vectorization/VectorFunction.h"
#include "vector/vector.h"
#include "vector/array_vector.h"
#include "vector/vector_helper.h"
#include "util/debug.h"

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;
using namespace omniruntime::op;

/// ArrayRepeatFunction - Repeats an element a given number of times to form an array
/// array_repeat(element T, count INT) -> array<T>
/// Returns an array containing the element repeated count times.
/// If count is negative, it is treated as 0 (Spark behavior).
/// If count is null, the result is null.
/// If element is null, returns an array of count nulls.
class ArrayRepeatFunction : public VectorFunction {
public:
    explicit ArrayRepeatFunction() {}

    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        ExecutionContext *context) const override;

private:
    static constexpr int32_t MAX_REPEAT_COUNT = 10000;

    // Pool is plumbed through to element-vector CreateFlatVector.
    template <typename T>
    void ProcessRepeat(BaseVector *elementVec, BaseVector *countVec, BaseVector *&result,
        int32_t rowSize, DataTypeId elementTypeId, omniruntime::mem::OmniTaskMemoryPool *pool) const;

    void ProcessRepeatVarchar(BaseVector *elementVec, BaseVector *countVec, BaseVector *&result,
        int32_t rowSize, omniruntime::mem::OmniTaskMemoryPool *pool) const;

    void ProcessRepeatAllNull(BaseVector *countVec, BaseVector *&result, int32_t rowSize,
        omniruntime::mem::OmniTaskMemoryPool *pool) const;
};

} // namespace omniruntime::vectorization
