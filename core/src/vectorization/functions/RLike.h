/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: RLike function implementation
 */

#pragma once
#include <vector>
#include <string_view>
#include <codecvt>

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

class RLikeFunction : public VectorFunction {
public:
    explicit RLikeFunction() {}

    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
               ExecutionContext *context) const override;

private:
    // Returns true if the pattern requires regex evaluation instead of literal substring matching.
    bool HasRegexMetaChars(const std::string_view &pattern) const;

    // Helper: Get string value from vector with different encodings
    std::string_view GetStringValueFromVector(BaseVector *vec, int32_t row) const;

    // Helper: Set boolean value to vector
    void SetBooleanValueToVector(BaseVector *vec, int32_t row, bool value) const;

    // Main implementation for RLIKE operation
    void ApplyRLike(BaseVector *strVec, BaseVector *patternVec, BaseVector *&result,
                    const DataTypePtr &outputType) const;

    // Match a string against a regex pattern
    bool MatchRegex(const std::string_view &str, const std::string_view &pattern) const;
};
}
