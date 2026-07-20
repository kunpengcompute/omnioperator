/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: JSON_EXISTS function for the vectorized expression framework
 *
 * json_exists(jsonValue, path [, onError]) -> boolean
 *
 * Determines whether a JSON string satisfies a given path search criterion, mirroring
 * Flink's SqlJsonUtils.jsonExists (flink-table-runtime) semantics:
 *
 *   - path may carry a `strict` / `lax` prefix (case-insensitive). When omitted the
 *     default mode is STRICT (same as Flink's jsonApiCommonSyntax).
 *       * LAX    : invalid JSON / invalid path / path-not-found are NOT errors -> FALSE.
 *       * STRICT : invalid JSON / invalid path / path-not-found ARE errors -> ON ERROR.
 *   - ON ERROR behavior (TRUE / FALSE / UNKNOWN / ERROR), default FALSE, is passed as an
 *     optional 3rd VARCHAR/CHAR literal argument (Flink models ON ERROR as an operand,
 *     not as operator identity). A missing 3rd arg means FALSE.
 *   - NULL input (jsonValue or path) -> NULL output. This matches Flink's codegen
 *     (FunctionGenerator registers JSON_EXISTS with argsNullable=false, so any NULL
 *     input short-circuits to NULL without invoking SqlJsonUtils). This is DIFFERENT
 *     from IsJson (which yields FALSE on NULL because its result type is BOOLEAN NOT NULL).
 *
 * Implemented as a VectorFunction (Path B) — like IsJson/GetJsonObject — because per-row
 * control over path evaluation, NULL propagation and the ON ERROR branch is needed.
 *
 * Path traversal reuses GetJsonObject's JSONPath normalization (dot/bracket notation).
 * Jayway advanced syntax (filters `$..b[?(@.x)]`, recursive `..`) is NOT supported —
 * same coverage boundary as the existing get_json_object in this codebase.
 */

#pragma once
#include "vectorization/VectorFunction.h"
#include "vector/vector.h"
#include "vector/vector_helper.h"
#include "util/debug.h"
#include "rapidjson/document.h"
#include <string>
#include <string_view>

namespace omniruntime::vectorization {

using namespace omniruntime::type;
using namespace omniruntime::vec;
using namespace omniruntime::op;

/// JSON path mode, mirroring Flink SqlJsonUtils.PathMode (only the two modes relevant
/// to JSON_EXISTS are used here).
enum class JsonPathMode {
    LAX,     // suppress errors; missing/invalid -> treated as "not found"
    STRICT,  // missing/invalid -> error, routed to ON ERROR
};

/// ON ERROR behavior, mirroring Flink org.apache.flink.table.api.JsonExistsOnError.
enum class JsonExistsOnError {
    FALSE,   // default
    TRUE,
    UNKNOWN, // -> NULL result
    ERROR,   // -> throw
};

/// Outcome of resolving a JSON path against a parsed document.
///   FOUND       — path resolves to a non-null value -> JSON_EXISTS = TRUE.
///   NOT_FOUND   — path does not resolve (missing key / OOB / type mismatch).
///                  In STRICT mode this is an error -> ON ERROR; in LAX -> FALSE.
///   NULL_VALUE  — path resolves to a JSON null value. Flink's Jayway-based jsonExists
///                  returns `context.obj != null`, and a JSON null leaf yields a Java
///                  null, so this is NOT an error — JSON_EXISTS = FALSE in both modes
///                  (e.g. JSON_EXISTS('{"a":null}', '$.a') = FALSE). ON ERROR does NOT
///                  apply here because no exception is thrown.
enum class JsonPathResult {
    FOUND,
    NOT_FOUND,
    NULL_VALUE,
};

class JsonExistsFunction final : public VectorFunction {
public:
    JsonExistsFunction() = default;

    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        ExecutionContext *context) const override;

private:
    // Read a string value from a const/flat/dictionary string vector.
    std::string_view GetStringValue(BaseVector *vector, int32_t row) const;

    // Parse the optional 3rd argument (ON ERROR literal) into a behavior enum.
    // nullptr / NULL / unrecognized -> FALSE (Flink default).
    JsonExistsOnError ParseOnError(BaseVector *onErrorArg) const;

    // Strip the optional strict/lax prefix (case-insensitive) from pathStr, set `mode`
    // accordingly (default STRICT when no prefix), then run GetJsonObject-style
    // normalization (RemoveSingleQuotes + state machine). Returns "-1" on invalid path.
    std::string NormalizeJsonPath(std::string_view pathStr, JsonPathMode &mode) const;

    // Remove single quotes from bracket notation like $['a']['b'] -> $[a][b]. Returns
    // "-1" on a missing closing bracket. Mirrors GetJsonObject::RemoveSingleQuotes.
    std::string RemoveSingleQuotes(const std::string &path) const;

    // Navigate an already-parsed document along normalizedPath (the substring after '$',
    // using '.' for object fields and '[index]'/'[key]' for array/object access). Returns:
    //   FOUND      — resolves to a non-null value;
    //   NULL_VALUE — resolves to a JSON null value (treated as "not exists", no error);
    //   NOT_FOUND  — does not resolve (missing key / OOB / type mismatch).
    // Mirrors GetJsonObject::ProcessJsonPathWithNormalizedPath traversal.
    JsonPathResult PathExists(const rapidjson::Value &doc, const std::string &normalizedPath) const;
};

} // namespace omniruntime::vectorization
