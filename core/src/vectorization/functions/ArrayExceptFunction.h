/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: ArrayExcept function implementation for computing set difference of two arrays
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

/// ArrayExceptFunction - Returns an array of elements in array1 but not in array2, without duplicates
/// array_except(array<T>, array<T>) -> array<T>
/// Returns a new array containing elements from the first array that are not in the second array.
/// The result does not contain duplicates.
/// If either input array is NULL, returns NULL.
/// If both arrays are empty, returns empty array.
/// NULL elements are handled: a NULL in array2 removes NULLs from array1.
class ArrayExceptFunction : public VectorFunction {
public:
    explicit ArrayExceptFunction() {}

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
