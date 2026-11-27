/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: ArrayDistinct function implementation for removing duplicate elements from array
 */

#pragma once

#include <vector>
#include <stack>
#include "vectorization/VectorFunction.h"
#include "vector/vector.h"
#include "vector/array_vector.h"
#include "util/debug.h"

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;
using namespace omniruntime::op;

/// ArrayDistinctFunction - Removes duplicate elements from an array
/// array_distinct(array<T>) -> array<T>
/// Returns a new array with duplicate elements removed.
/// Preserves the order of first occurrence.
/// If input array is NULL, returns NULL.
/// If input array is empty, returns empty array.
/// At most one null element is kept in the result.
class ArrayDistinctFunction : public VectorFunction {
public:
    explicit ArrayDistinctFunction() {}

    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        ExecutionContext *context) const override;

private:
    template <typename T>
    void ProcessTyped(ArrayVector *arrayVec, ArrayVector *resultArray, int32_t rowSize,
        omniruntime::mem::OmniTaskMemoryPool *pool) const;

    void ProcessString(ArrayVector *arrayVec, ArrayVector *resultArray, int32_t rowSize,
        omniruntime::mem::OmniTaskMemoryPool *pool) const;
};

} // namespace omniruntime::vectorization
