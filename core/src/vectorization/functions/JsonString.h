/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: json_string function for JSON operations.
 *
 * Implements Flink SQL `JSON_STRING(value)` semantics: serializes a scalar
 * (BOOL / integral / floating-point / string) or a composite value
 * (ARRAY / MAP / ROW) into a JSON string. A NULL input produces a NULL result.
 *
 * Key difference from Spark to_json (ToJsonFunction): Flink JSON_STRING keeps
 * NULL struct fields in the output (emits "field":null), whereas to_json drops
 * them. JSON_STRING also accepts scalar inputs, which to_json does not.
 *
 * See docs/expression-design/json_string_design.md for the full design.
 */

#pragma once
#include "vectorization/VectorFunction.h"
#include "vector/array_vector.h"
#include "vector/map_vector.h"
#include "vector/row_vector.h"
#include "vector/vector_helper.h"
#include "type/data_type.h"
#include "util/debug.h"
#include <string>
#include <string_view>
#include <sstream>

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;
using namespace omniruntime::op;

class JsonStringFunction final : public VectorFunction {
public:
    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType,
        BaseVector *&result, ExecutionContext *context) const override;

    // --- Public serialization helpers (shared with JsonObjectFunction) ---
    // Recursively serialize vec[row] (logical type `type`) as JSON text appended to `out`.
    // When `type` is nullptr, falls back to the vector's own type id.
    void appendToJson(BaseVector *vec, int32_t row, const DataType *type, std::string &out) const;
    // Escape a string view per JSON string rules (RFC 8259) and append to `out`.
    void escapeJsonString(const std::string_view &s, std::string &out) const;
    // Read a string view from a flat/const/dictionary string vector.
    std::string_view getStringFromVector(BaseVector *vec, int32_t row) const;

private:
    // Serialize a contiguous slice of `vec` (array elements / map entries), emitting `null`
    // for null elements, separated by commas.
    void appendToJsonFromSlice(BaseVector *vec, int32_t startIdx, int32_t count,
        const DataType *type, std::string &out) const;
    void appendArrayToJson(ArrayVector *arrVec, int32_t row, const DataType *elemType, std::string &out) const;
    void appendMapToJson(MapVector *mapVec, int32_t row, const DataType *keyType,
        const DataType *valType, std::string &out) const;
    // Flink JSON_STRING keeps NULL fields (emits "field":null); do NOT skip them.
    void appendRowToJson(RowVector *rowVec, int32_t row, const RowType *rowType, std::string &out) const;
    // Format a double like Flink's numberNode: shortest round-trip representation,
    // with no spurious trailing zeros. Integral doubles emit without a decimal point.
    std::string formatDouble(double v) const;
    // Read a typed scalar value from a flat/const/dictionary vector.
    template <typename T>
    T getValueFromVector(BaseVector *vec, int32_t row) const;
};
}
