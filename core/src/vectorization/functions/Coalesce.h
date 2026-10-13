/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: Coalesce function for vectorized conditional expressions
 */

#pragma once
#include "vectorization/VectorFunction.h"
#include "vector/array_vector.h"
#include "vector/map_vector.h"
#include "vector/row_vector.h"
#include "type/data_operations.h"
#include "util/debug.h"
#include "util/type_util.h"
#include "vector/vector_helper.h"
#include <vector>
#include <string_view>

namespace omniruntime::vectorization {
    using namespace omniruntime::type;
    using namespace omniruntime::vec;
    using namespace omniruntime::op;

class CoalesceFunction : public VectorFunction {
public:
    explicit CoalesceFunction() {}

    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
               ExecutionContext *context) const override;

private:
    // Helper: Get value from vector with different encodings
    template<typename T>
    T GetValueFromVector(BaseVector *vec, int32_t row) const;

    // Helper: Get string value from vector
    std::string_view GetStringValueFromVector(BaseVector *vec, int32_t row) const;

    // Helper: Set value to vector
    template<typename T>
    void SetValueToVector(BaseVector *vec, int32_t row, const T &value) const;

    // Helper: Set string value to vector
    void SetStringValueToVector(BaseVector *vec, int32_t row, const std::string_view &value) const;

    // Dispatch coalesce based on output type; pool is threaded through to all CoalesceXxx helpers below.
    void DispatchCoalesce(const std::vector<BaseVector *> &argVectors, const DataTypePtr &outputType,
                          BaseVector *&result, omniruntime::mem::OmniTaskMemoryPool *pool) const;

    // Template implementations for different types
    template<typename T>
    void CoalesceNumeric(const std::vector<BaseVector *> &argVectors, BaseVector *&result,
                        const DataTypePtr &outputType, omniruntime::mem::OmniTaskMemoryPool *pool) const;

    void CoalesceString(const std::vector<BaseVector *> &argVectors, BaseVector *&result,
                        const DataTypePtr &outputType, omniruntime::mem::OmniTaskMemoryPool *pool) const;

    void CoalesceBoolean(const std::vector<BaseVector *> &argVectors, BaseVector *&result,
                         const DataTypePtr &outputType, omniruntime::mem::OmniTaskMemoryPool *pool) const;

    // Complex type implementations
    void CoalesceArray(const std::vector<BaseVector *> &argVectors, BaseVector *&result,
                       const DataTypePtr &outputType, omniruntime::mem::OmniTaskMemoryPool *pool) const;

    void CoalesceMap(const std::vector<BaseVector *> &argVectors, BaseVector *&result,
                     const DataTypePtr &outputType, omniruntime::mem::OmniTaskMemoryPool *pool) const;

    void CoalesceRow(const std::vector<BaseVector *> &argVectors, BaseVector *&result,
                     const DataTypePtr &outputType, omniruntime::mem::OmniTaskMemoryPool *pool) const;
};
}
