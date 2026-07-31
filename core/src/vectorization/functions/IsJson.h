/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: IS JSON function for the vectorized expression framework
 *
 * is_json_value(string)  -> boolean   (any valid JSON)
 * is_json_scalar(string) -> boolean   (valid JSON whose top-level value is a scalar)
 * is_json_array(string)  -> boolean   (valid JSON whose top-level value is an array)
 * is_json_object(string) -> boolean   (valid JSON whose top-level value is an object)
 *
 * Mirrors Flink's SqlJsonUtils.isJsonValue/isJsonScalar/isJsonArray/isJsonObject
 * (which use Jackson's JacksonJsonProvider.parse). Here we reuse OmniOperator's
 * existing rapidjson (rapidjson/document.h) for parsing.
 *
 *   - NULL input  -> FALSE (NOT null; Flink generateCallIfArgsNullable with
 *                  resultNullable=false keeps nullTerm=false and calls
 *                  isJsonValue(null) -> false). Result type is BOOLEAN NOT NULL.
 *   - empty string-> FALSE.
 *   - otherwise   -> parse with rapidjson; HasParseError() -> FALSE; else apply
 *                  the type constraint (VALUE/SCALAR/ARRAY/OBJECT).
 *
 * Implemented as a VectorFunction (Path B) rather than a SimpleFunction, because
 * the SimpleFunction framework skips NULL input rows (IntersectNull) and would
 * propagate NULL — but Flink requires IS JSON(NULL) = FALSE. Path B gives full
 * per-row control so NULL inputs yield an explicit FALSE result.
 *
 * The four JSON-type variants share one class, distinguished by the `JsonType`
 * constructor argument (the JSON type is part of the Calcite operator identity,
 * not a runtime parameter).
 */

#pragma once
#include "vectorization/VectorFunction.h"
#include "vector/vector.h"
#include "vector/vector_helper.h"
#include "util/debug.h"
#include "rapidjson/document.h"
#include <string_view>

namespace omniruntime::vectorization {

using namespace omniruntime::type;
using namespace omniruntime::vec;
using namespace omniruntime::op;

/// JSON type constraint, mirroring Flink's org.apache.flink.table.api.JsonType.
enum class JsonType {
    VALUE,   // any valid JSON (default for `x IS JSON`)
    SCALAR,  // valid JSON scalar (number/string/bool/null)
    ARRAY,   // valid JSON array
    OBJECT,  // valid JSON object
};

class IsJsonFunction final : public VectorFunction {
public:
    explicit IsJsonFunction(JsonType type) : type_(type) {}

    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        ExecutionContext *context) const override;

private:
    JsonType type_;

    // Read a string value from a const/flat/dictionary string vector.
    std::string_view GetStringValue(BaseVector *vector, int32_t row) const;

    // Classify a parsed rapidjson document under the configured type constraint.
    // doc must already be parsed (no further error checking here).
    bool Classify(const rapidjson::Document &doc) const;
};

} // namespace omniruntime::vectorization
