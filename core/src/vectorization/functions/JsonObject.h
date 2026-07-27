/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: json_object function for JSON operations.
 *
 * Implements Flink SQL `JSON_OBJECT([[KEY] key VALUE value]* [ { NULL | ABSENT } ON NULL ])`:
 * builds a JSON object string from a list of key-value pairs.
 *
 * Heterogeneous variadic dispatch (like named_struct / concat_ws): not registered in the
 * signature table; instead the FuncExpr constructor in expressions.cpp special-cases
 * funcName == "json_object" and constructs this function with the per-argument DataType
 * list (inputDataTypes_). Apply reads the onNull flag + key/value vectors from the stack
 * and serializes each value (delegating to JsonStringFunction) per row.
 *
 * Native argument layout (aligned with OmniAdaptor JSON_EXISTS symbol-literal convention):
 *   arg[0]            : OMNI_VARCHAR constant, "NULL" or "ABSENT" (ON NULL behavior)
 *   arg[1,3,5,...]    : OMNI_VARCHAR key (non-NULL string literal)
 *   arg[2,4,6,...]    : value of any supported JSON type (nullable)
 *
 * NULL ON NULL  : null value -> "key":null
 * ABSENT ON NULL: null value -> key omitted
 * Empty key set : "{}"
 * Return is always non-NULL (Flink declares STRING().notNull()).
 *
 * Nested JSON constructors: when a value argument is itself a JSON_OBJECT / JSON_ARRAY call
 * (Flink's JsonGenerateUtils.isJsonFunctionOperand), its already-serialized JSON text must be
 * inserted as a raw node (verbatim), not re-quoted as a string. e.g.
 * JSON_OBJECT(KEY 'k' VALUE JSON_OBJECT(KEY 'a' VALUE 'b')) -> {"k":{"a":"b"}} rather than
 * {"k":"{\"a\":\"b\"}"}. The per-value "raw" flags are computed in FuncExpr's constructor
 * (expressions.cpp) by inspecting whether each value child Expr is a json_object / json_array
 * FuncExpr, mirroring Flink's RexNode-level operand check.
 *
 * See docs/expression-design/json_object_design.md for the full design.
 */

#pragma once
#include "vectorization/VectorFunction.h"
#include "vectorization/functions/JsonString.h"
#include "vector/vector_helper.h"
#include "type/data_type.h"
#include <string>
#include <string_view>
#include <vector>

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;
using namespace omniruntime::op;

class JsonObjectFunction final : public VectorFunction {
public:
    explicit JsonObjectFunction() {}

    // Constructed by the FuncExpr special-case dispatch with the per-argument DataType list,
    // so Apply knows each value argument's type and can serialize it via JsonStringFunction.
    explicit JsonObjectFunction(const std::vector<DataTypePtr> &inputDataTypes)
        : inputDataTypes_(inputDataTypes) {}

    // valueIsRaw[p] marks whether the p-th pair's value (allArgs[2 + 2*p]) is a nested JSON
    // constructor result that must be inserted verbatim (raw node) instead of quoted as a string.
    JsonObjectFunction(const std::vector<DataTypePtr> &inputDataTypes, const std::vector<bool> &valueIsRaw)
        : inputDataTypes_(inputDataTypes), valueIsRaw_(valueIsRaw) {}

    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType,
        BaseVector *&result, ExecutionContext *context) const override;

private:
    // Parse the onNull flag (OMNI_VARCHAR constant at arg[0]) -> true means ABSENT ON NULL.
    // Defaults to NULL ON NULL when the flag is null or unrecognized.
    bool IsAbsentOnNull(BaseVector *flagVec) const;
    // Read a string view from a flat/const/dictionary string vector (delegates to serializer_).
    std::string_view GetStringFromVector(BaseVector *vec, int32_t row) const
    {
        return serializer_.getStringFromVector(vec, row);
    }
    // Serialize valueVec[row] (logical type `type`) as JSON text appended to `out`, delegating
    // to JsonStringFunction so json_object values serialize identically to json_string output.
    void AppendValueToJson(BaseVector *valueVec, int32_t row, const DataType *type, std::string &out) const
    {
        serializer_.appendToJson(valueVec, row, type, out);
    }

    // Whether each pair's value should be inserted as a raw (already-serialized) JSON node. Empty
    // means "no value is raw" (backward compatible with the inputDataTypes-only constructor).
    std::vector<bool> valueIsRaw_;
    std::vector<DataTypePtr> inputDataTypes_;
    JsonStringFunction serializer_;
};
}
