/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: ArrayAppend function implementation
 */

#pragma once

#include <vector>
#include <stack>
#include "vectorization/VectorFunction.h"
#include "vector/vector.h"
#include "vector/array_vector.h"
#include "vector/vector_helper.h"
#include "util/debug.h"

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;
using namespace omniruntime::op;

/// ArrayAppendFunction - Append an element to the end of an array
/// array_append(array<T>, T) -> array<T>
/// Returns a new array with the element appended at the end.
/// If input array is NULL, returns NULL.
/// If input element is NULL, appends a null element to the array.
/// If input array is empty, returns a single-element array.
class ArrayAppendFunction : public VectorFunction {
public:
    explicit ArrayAppendFunction() {}

    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        ExecutionContext *context) const override;

private:
    // Pool plumbed through to result-element CreateFlatVector.
    template <typename T>
    void ProcessAppend(ArrayVector *arrayVec, BaseVector *elementVec, BaseVector *&result,
        int32_t rowSize, DataTypeId elementTypeId, omniruntime::mem::OmniTaskMemoryPool *pool) const;

    void ProcessAppendVarchar(ArrayVector *arrayVec, BaseVector *elementVec, BaseVector *&result,
        int32_t rowSize, omniruntime::mem::OmniTaskMemoryPool *pool) const;
};

} // namespace omniruntime::vectorization
