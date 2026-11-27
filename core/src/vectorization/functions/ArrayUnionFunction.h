/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: ArrayUnion function implementation for computing set union of two arrays
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

/// ArrayUnionFunction - Returns an array of elements in the union of two arrays, without duplicates
/// array_union(array<T>, array<T>) -> array<T>
/// Returns a new array containing the distinct elements from both input arrays.
/// Elements from the first array appear first, followed by new elements from the second array.
/// At most one null element is kept in the result.
/// If either input array is NULL, returns NULL.
class ArrayUnionFunction : public VectorFunction {
public:
    explicit ArrayUnionFunction() {}

    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        ExecutionContext *context) const override;

private:
    template <typename T>
    void ProcessTyped(ArrayVector *leftArray, ArrayVector *rightArray,
        ArrayVector *resultArray, int32_t rowSize, omniruntime::mem::OmniTaskMemoryPool *pool) const;

    void ProcessString(ArrayVector *leftArray, ArrayVector *rightArray,
        ArrayVector *resultArray, int32_t rowSize, omniruntime::mem::OmniTaskMemoryPool *pool) const;
};

} // namespace omniruntime::vectorization
