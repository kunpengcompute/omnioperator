/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: RegexpExtract function implementation
 */

#pragma once
#include <vector>
#include <string_view>

#include "vectorization/VectorFunction.h"
#include "vector/array_vector.h"
#include "type/data_operations.h"
#include "util/debug.h"
#include "util/type_util.h"
#include "vector/vector_helper.h"

namespace omniruntime::vectorization {
    using namespace omniruntime::type;
    using namespace omniruntime::vec;
    using namespace omniruntime::op;

class RegexpExtractFunction : public VectorFunction {
public:
    explicit RegexpExtractFunction() {}

    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
               ExecutionContext *context) const override;

private:
    // Helper: Get string value from vector with different encodings
    std::string_view GetStringValueFromVector(BaseVector *vec, int32_t row) const;

    // Helper: Get integer value from vector with different encodings
    int32_t GetIntValueFromVector(BaseVector *vec, int32_t row) const;

    // Helper: Set string value to vector
    void SetStringValueToVector(BaseVector *vec, int32_t row, std::string_view &value) const;

    // Main implementation for REGEXP_EXTRACT operation
    void ApplyRegexpExtract(BaseVector *strVec, BaseVector *patternVec, BaseVector *groupIdxVec,
        BaseVector *&result, const DataTypePtr &outputType, ExecutionContext *context) const;

    // Extract a string using regex pattern and group index.
    // The returned view points into the input string, which outlives the call.
    std::string_view ExtractRegex(const std::string_view &str, const std::string_view &pattern,
        int32_t groupIdx) const;
};
}