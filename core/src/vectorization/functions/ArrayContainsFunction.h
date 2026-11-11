/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: ArrayContains function implementation for checking if array contains a value
 */

#pragma once

#include <vector>
#include <stack>
#include <cmath>
#include <string_view>
#include <type_traits>
#include "vectorization/VectorFunction.h"
#include "vector/vector.h"
#include "vector/array_vector.h"
#include "util/debug.h"

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;
using namespace omniruntime::op;

/// ArrayContainsFunction - Checks if an array contains a given value
/// array_contains(array<T>, T) -> boolean
/// Returns true if the array contains the specified value.
/// Returns false if the array does not contain the value and has no nulls.
/// Returns NULL if the array is NULL, or if the search value is not found
/// but the array contains null elements (Spark three-valued logic).
class ArrayContainsFunction : public VectorFunction {
public:
    explicit ArrayContainsFunction() {}

    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        ExecutionContext *context) const override;

private:
    template <typename T>
    void ProcessArrayContains(ArrayVector *arrayVec, BaseVector *searchVec,
        Vector<bool> *resultVec, int32_t rowSize) const;

    void ProcessArrayContainsVarchar(ArrayVector *arrayVec, BaseVector *searchVec,
        Vector<bool> *resultVec, int32_t rowSize) const;
};

} // namespace omniruntime::vectorization
