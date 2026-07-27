/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: json_array function implementation.
 *
 * Flink SQL JSON_ARRAY([value]* [ { NULL | ABSENT } ON NULL ]):
 * builds a JSON array string from a list of values. See JsonArray.h and
 * docs/expression-design/json_array_design.md.
 *
 * Heterogeneous variadic: arguments come from the stack (LIFO), so we pop all of them
 * and reverse to restore call order: [onNullFlag, value1, value2, ...].
 * Value serialization is delegated to JsonStringFunction so json_array values match
 * json_string output exactly (escaping, ROW null-field retention, etc.).
 */

#include "JsonArray.h"
#include "vector/vector.h"
#include "util/debug.h"
#include <algorithm>

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;

void JsonArrayFunction::Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType,
    BaseVector *&result, ExecutionContext *context) const
{
    size_t argCount = inputDataTypes_.size();
    // Layout: [onNullFlag, value1, value2, ...] => argCount must be >= 1 (at least the flag).
    if (argCount < 1) {
        OMNI_THROW("JsonArrayFunction Error:",
            "expected at least the onNull flag argument, got " + std::to_string(argCount));
    }
    // Pop all arguments (stack is LIFO) and restore call order.
    std::vector<BaseVector *> allArgs;
    allArgs.reserve(argCount);
    for (size_t i = 0; i < argCount; ++i) {
        if (args.empty()) {
            OMNI_THROW("JsonArrayFunction Error:", "stack underflow while reading arguments");
        }
        allArgs.push_back(args.top());
        args.pop();
    }
    std::reverse(allArgs.begin(), allArgs.end());

    BaseVector *flagVec = allArgs[0];
    bool absentOnNull = IsAbsentOnNull(flagVec);
    size_t valueCount = argCount - 1;

    // Use the context's result row size (consistent with JsonObjectFunction); this handles
    // const-flag/const-value vectors whose own GetSize() may be 1.
    int32_t rowSize = (context != nullptr) ? context->GetResultRowSize() : 0;
    if (rowSize <= 0) {
        // Fall back to the first value vector's size if the context didn't set it.
        for (size_t i = 1; i < allArgs.size(); ++i) {
            if (allArgs[i] != nullptr && allArgs[i]->GetSize() > 0) {
                rowSize = allArgs[i]->GetSize();
                break;
            }
        }
    }
    auto *stringResult = new Vector<LargeStringContainer<std::string_view>>(rowSize);
    stringResult->SetIsField(true);

    for (int32_t row = 0; row < rowSize; ++row) {
        std::string out;
        out.push_back('[');
        bool first = true;
        for (size_t v = 0; v < valueCount; ++v) {
            BaseVector *valueVec = allArgs[1 + v];
            bool valueIsNull = (valueVec == nullptr) || valueVec->IsNull(row);
            // ABSENT ON NULL: skip null elements entirely.
            if (absentOnNull && valueIsNull) {
                continue;
            }
            if (!first) {
                out.push_back(',');
            }
            first = false;
            if (valueIsNull) {
                // NULL ON NULL (or unreachable ABSENT branch): emit JSON null.
                out.append("null");
            } else {
                int32_t valueRow = (valueVec->GetEncoding() == OMNI_ENCODING_CONST) ? 0 : row;
                bool isRaw = (v < valueIsRaw_.size()) && valueIsRaw_[v];
                if (isRaw) {
                    // Nested JSON_ARRAY / JSON_OBJECT result: insert its serialized text verbatim
                    // (raw node), e.g. JSON_ARRAY(JSON_ARRAY(1)) -> [[1]] rather than ["[1]"].
                    std::string_view rawSv = GetStringFromVector(valueVec, valueRow);
                    out.append(rawSv.data(), rawSv.size());
                } else {
                    // Serialize the value per its DataType (from inputDataTypes_), so typed values
                    // (int/bool/array/row/...) render correctly rather than as quoted strings.
                    const DataType *valueType =
                        (1 + v < inputDataTypes_.size()) ? inputDataTypes_[1 + v].get() : nullptr;
                    AppendValueToJson(valueVec, valueRow, valueType, out);
                }
            }
        }
        out.push_back(']');
        std::string_view sv(out);
        stringResult->SetValue(row, sv);
    }
    result = stringResult;

    for (auto *vec : allArgs) {
        if (vec != nullptr && !vec->GetIsField()) {
            delete vec;
        }
    }
}

bool JsonArrayFunction::IsAbsentOnNull(BaseVector *flagVec) const
{
    if (flagVec == nullptr || flagVec->IsNull(0)) {
        return false;  // default: NULL ON NULL
    }
    std::string_view flag = GetStringFromVector(flagVec, 0);
    // Case-insensitive compare to "ABSENT".
    if (flag.size() != 6) {
        return false;
    }
    const char kAbsent[] = "ABSENT";
    for (int i = 0; i < 6; ++i) {
        char c = flag[i];
        if (c >= 'a' && c <= 'z') {
            c = static_cast<char>(c - 'a' + 'A');
        }
        if (c != kAbsent[i]) {
            return false;
        }
    }
    return true;
}
}
