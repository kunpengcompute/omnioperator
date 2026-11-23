/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: LIKE function implementation.
 * LIKE(string, pattern) -> boolean; LIKE(string, pattern, escape) -> boolean.
 * Pattern: % = zero or more chars, _ = one char; default escape is backslash, overridable via 3-arg form.
 */

#pragma once
#include <vector>
#include <string_view>
#include <string>

#include "vectorization/VectorFunction.h"
#include "vector/vector.h"
#include "type/data_operations.h"
#include "util/debug.h"
#include "vector/vector_helper.h"

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;
using namespace omniruntime::op;

class LikeFunction : public VectorFunction {
public:
    explicit LikeFunction(int32_t argumentCount) : argumentCount_(argumentCount) {}

    void Apply(std::stack<BaseVector*>& args, const DataTypePtr& outputType, BaseVector*& result,
               ExecutionContext* context) const override;

private:
    int32_t argumentCount_;

    /// Get string value from vector (flat / const / dictionary)
    static std::string_view GetStringValueFromVector(BaseVector* vec, int32_t row);

    /// Set boolean value to result vector
    static void SetBooleanValueToVector(BaseVector* vec, int32_t row, bool value);

    /// Main implementation: match str against LIKE pattern (% _ escape)
    void ApplyLike(BaseVector* strVec, BaseVector* patternVec, BaseVector*& result,
                   const DataTypePtr& outputType) const;

    void ApplyLikeWithEscape(BaseVector* strVec, BaseVector* patternVec, BaseVector* escapeVec, BaseVector*& result,
                             const DataTypePtr& outputType) const;

    /// Convert LIKE pattern to an RE2 regex body; match str full-string via RE2::ANCHOR_BOTH
    bool MatchLike(const std::string_view& str, const std::string_view& pattern) const;
    bool MatchLike(const std::string_view& str, const std::string_view& pattern,
                   const std::string_view& escapeSeq) const;
};

}  // namespace omniruntime::vectorization
