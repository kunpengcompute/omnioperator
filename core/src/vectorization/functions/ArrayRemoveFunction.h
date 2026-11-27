/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: ArrayRemove function implementation
 */

#pragma once

#include <vector>
#include <stack>
#include <cmath>
#include <type_traits>
#include "vectorization/VectorFunction.h"
#include "vector/vector.h"
#include "vector/array_vector.h"
#include "vector/vector_helper.h"
#include "util/debug.h"

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;
using namespace omniruntime::op;

/// ArrayRemoveFunction - Remove all elements equal to a given value from an array
/// array_remove(array<T>, T) -> array<T>
/// Returns a new array with all occurrences of the specified element removed.
/// Null elements in the input array are preserved in the output.
/// For floating point types, NaN is treated as equal to NaN (NaN-aware comparison).
class ArrayRemoveFunction : public VectorFunction {
public:
    explicit ArrayRemoveFunction() {}

    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        ExecutionContext *context) const override;

private:
    // Pool is plumbed through to result-element CreateFlatVector.
    template <typename T>
    void ProcessRemove(ArrayVector *arrayVec, BaseVector *elementVec, BaseVector *&result,
        int32_t rowSize, DataTypeId elementTypeId, omniruntime::mem::OmniTaskMemoryPool *pool) const;

    void ProcessRemoveVarchar(ArrayVector *arrayVec, BaseVector *elementVec, BaseVector *&result,
        int32_t rowSize, omniruntime::mem::OmniTaskMemoryPool *pool) const;
};

} // namespace omniruntime::vectorization
