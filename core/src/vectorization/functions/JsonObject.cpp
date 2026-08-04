/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: json_object function implementation.
 *
 * Flink SQL JSON_OBJECT([[KEY] key VALUE value]* [ { NULL | ABSENT } ON NULL ]):
 * builds a JSON object string from key-value pairs. See JsonObject.h and
 * docs/expression-design/json_object_design.md.
 *
 * Heterogeneous variadic: arguments come from the stack (LIFO), so we pop all of them
 * and reverse to restore call order: [onNullFlag, key1, value1, key2, value2, ...].
 * Value serialization is delegated to JsonStringFunction so json_object values match
 * json_string output exactly (escaping, ROW null-field retention, etc.).
 */

#include "JsonObject.h"
#include "vector/vector.h"
#include "util/debug.h"
#include <algorithm>

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;

void JsonObjectFunction::Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType,
    BaseVector *&result, ExecutionContext *context) const
{
    size_t argCount = inputDataTypes_.size();
    // Layout: [onNullFlag, key1, value1, key2, value2, ...] => argCount must be odd and >= 1.
    if (argCount < 1 || (argCount - 1) % 2 != 0) {
        OMNI_THROW("JsonObjectFunction Error:",
            "expected odd arg count (flag + key/value pairs), got " + std::to_string(argCount));
    }
    // Pop all arguments (stack is LIFO) and restore call order.
    std::vector<BaseVector *> allArgs;
    allArgs.reserve(argCount);
    for (size_t i = 0; i < argCount; ++i) {
        if (args.empty()) {
            OMNI_THROW("JsonObjectFunction Error:", "stack underflow while reading arguments");
        }
        allArgs.push_back(args.top());
        args.pop();
    }
    std::reverse(allArgs.begin(), allArgs.end());

    BaseVector *flagVec = allArgs[0];
    bool absentOnNull = IsAbsentOnNull(flagVec);
    size_t pairCount = (argCount - 1) / 2;

    // Use the context's result row size (consistent with GetJsonObjectFunction); this handles
    // const-flag/const-key vectors whose own GetSize() may be 1.
    int32_t rowSize = (context != nullptr) ? context->GetResultRowSize() : 0;
    if (rowSize <= 0) {
        // Fall back to the first key/value vector's size if the context didn't set it.
        for (size_t i = 1; i < allArgs.size(); ++i) {
            if (allArgs[i] != nullptr && allArgs[i]->GetSize() > 0) {
                rowSize = allArgs[i]->GetSize();
                break;
            }
        }
    }
    auto *stringResult = new Vector<LargeStringContainer<std::string_view>>(rowSize);

    for (int32_t row = 0; row < rowSize; ++row) {
        std::string out;
        out.push_back('{');
        bool first = true;
        for (size_t p = 0; p < pairCount; ++p) {
            BaseVector *keyVec = allArgs[1 + 2 * p];
            BaseVector *valueVec = allArgs[2 + 2 * p];
            bool valueIsNull = (valueVec == nullptr) || valueVec->IsNull(row);
            // ABSENT ON NULL: skip null-valued entries entirely.
            if (absentOnNull && valueIsNull) {
                continue;
            }
            if (!first) {
                out.push_back(',');
            }
            first = false;
            // Key: Flink guarantees non-NULL string literal; read row 0 for const vectors.
            int32_t keyRow = (keyVec->GetEncoding() == OMNI_ENCODING_CONST) ? 0 : row;
            std::string_view keySv = GetStringFromVector(keyVec, keyRow);
            out.push_back('"');
            serializer_.escapeJsonString(keySv, out);
            out.append("\":");
            if (valueIsNull) {
                // NULL ON NULL (or unreachable ABSENT branch): emit JSON null.
                out.append("null");
            } else {
                int32_t valueRow = (valueVec->GetEncoding() == OMNI_ENCODING_CONST) ? 0 : row;
                bool isRaw = (p < valueIsRaw_.size()) && valueIsRaw_[p];
                if (isRaw) {
                    // Nested JSON_OBJECT / JSON_ARRAY result: insert its serialized text verbatim
                    // (raw node), e.g. {"k":{"a":"b"}} rather than {"k":"{\"a\":\"b\"}"}.
                    std::string_view rawSv = GetStringFromVector(valueVec, valueRow);
                    out.append(rawSv.data(), rawSv.size());
                } else {
                    // Serialize the value per its DataType (from inputDataTypes_), so typed values
                    // (int/bool/array/row/...) render correctly rather than as quoted strings.
                    const DataType *valueType =
                        (2 + 2 * p < inputDataTypes_.size()) ? inputDataTypes_[2 + 2 * p].get() : nullptr;
                    AppendValueToJson(valueVec, valueRow, valueType, out);
                }
            }
        }
        out.push_back('}');
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

bool JsonObjectFunction::IsAbsentOnNull(BaseVector *flagVec) const
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
