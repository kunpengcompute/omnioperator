/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: json_string function implementation.
 *
 * Flink SQL JSON_STRING(value): serializes a value into a JSON string.
 * - Scalar types (BOOL / integral / floating-point / string) emit a JSON scalar.
 * - Composite types (ARRAY / MAP / ROW) emit a JSON array/object.
 * - NULL input -> NULL output.
 *
 * Flink-specific behavior (differs from Spark to_json / ToJsonFunction):
 * - STRING scalars are wrapped in quotes (matches to_json), but JSON_STRING also
 *   accepts bare scalars (to_json only accepts struct/array/map).
 * - ROW fields with NULL values are KEPT as "field":null (Spark to_json drops them).
 *
 * Out of scope this version (not registered, throws if reached): DECIMAL, BINARY,
 * TIMESTAMP, DATE. See docs/expression-design/json_string_design.md.
 */

#include "JsonString.h"
#include "vector/vector.h"
#include "type/data_type.h"
#include "util/type_util.h"
#include <cmath>
#include <cstdio>
#include <sstream>
#include <string>

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;

void JsonStringFunction::Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType,
    BaseVector *&result, ExecutionContext *context) const
{
    auto *inputArg = args.top();
    args.pop();
    // Input DataType carries struct field names (set by ExprEval when funcName ==
    // "json_string"); may be nullptr for scalars, in which case we fall back to the
    // vector's own type id.
    const DataType *inputType = (context != nullptr) ? context->GetToJsonInputType() : nullptr;
    int32_t rowSize = inputArg->GetSize();
    auto *stringResult = new Vector<LargeStringContainer<std::string_view>>(rowSize);
    for (int32_t row = 0; row < rowSize; ++row) {
        if (inputArg->IsNull(row)) {
            // Flink JSON_STRING: NULL input -> NULL output.
            stringResult->SetNull(row);
            continue;
        }
        std::string jsonStr;
        appendToJson(inputArg, row, inputType, jsonStr);
        std::string_view sv(jsonStr);
        stringResult->SetValue(row, sv);
    }
    result = stringResult;
}

void JsonStringFunction::appendToJson(BaseVector *vec, int32_t row, const DataType *type,
    std::string &out) const
{
    DataTypeId typeId = (type != nullptr) ? type->GetId() : vec->GetTypeId();
    switch (typeId) {
        case OMNI_BOOLEAN: {
            bool v = getValueFromVector<bool>(vec, row);
            out.append(v ? "true" : "false");
            break;
        }
        case OMNI_BYTE: {
            int8_t v = getValueFromVector<int8_t>(vec, row);
            out.append(std::to_string(v));
            break;
        }
        case OMNI_SHORT: {
            int16_t v = getValueFromVector<int16_t>(vec, row);
            out.append(std::to_string(v));
            break;
        }
        case OMNI_INT: {
            int32_t v = getValueFromVector<int32_t>(vec, row);
            out.append(std::to_string(v));
            break;
        }
        case OMNI_LONG: {
            int64_t v = getValueFromVector<int64_t>(vec, row);
            out.append(std::to_string(v));
            break;
        }
        case OMNI_FLOAT: {
            float v = getValueFromVector<float>(vec, row);
            out.append(formatDouble(static_cast<double>(v)));
            break;
        }
        case OMNI_DOUBLE: {
            double v = getValueFromVector<double>(vec, row);
            out.append(formatDouble(v));
            break;
        }
        case OMNI_VARCHAR:
        case OMNI_CHAR: {
            std::string_view sv = getStringFromVector(vec, row);
            out.push_back('"');
            escapeJsonString(sv, out);
            out.push_back('"');
            break;
        }
        case OMNI_ARRAY: {
            auto *arrVec = static_cast<ArrayVector *>(vec);
            const DataType *elemType =
                (type != nullptr) ? static_cast<const ArrayType *>(type)->ElementType().get() : nullptr;
            appendArrayToJson(arrVec, row, elemType, out);
            break;
        }
        case OMNI_MAP: {
            auto *mapVec = static_cast<MapVector *>(vec);
            const DataType *keyType = nullptr;
            const DataType *valType = nullptr;
            if (type != nullptr) {
                keyType = static_cast<const MapType *>(type)->Key().get();
                valType = static_cast<const MapType *>(type)->Value().get();
            }
            appendMapToJson(mapVec, row, keyType, valType, out);
            break;
        }
        case OMNI_ROW: {
            auto *rowVec = static_cast<RowVector *>(vec);
            const RowType *rowType = (type != nullptr) ? static_cast<const RowType *>(type) : nullptr;
            appendRowToJson(rowVec, row, rowType, out);
            break;
        }
        default:
            // Out of scope this version: DECIMAL / VARBINARY / DATE32 / TIMESTAMP / etc.
            OMNI_THROW("JsonStringFunction Error:", "Unsupported type for json_string");
    }
}

void JsonStringFunction::appendToJsonFromSlice(BaseVector *vec, int32_t startIdx, int32_t count,
    const DataType *type, std::string &out) const
{
    for (int32_t i = 0; i < count; ++i) {
        if (i > 0) out.push_back(',');
        int32_t readRow = startIdx + i;
        if (vec->IsNull(readRow)) {
            out.append("null");
        } else {
            appendToJson(vec, readRow, type, out);
        }
    }
}

void JsonStringFunction::appendArrayToJson(ArrayVector *arrVec, int32_t row,
    const DataType *elemType, std::string &out) const
{
    out.push_back('[');
    int64_t startOffset = arrVec->GetOffset(row);
    int64_t arrSize = arrVec->GetSize(row);
    BaseVector *elemVec = arrVec->GetElementVector().get();
    appendToJsonFromSlice(elemVec, static_cast<int32_t>(startOffset), static_cast<int32_t>(arrSize),
        elemType, out);
    out.push_back(']');
}

void JsonStringFunction::appendMapToJson(MapVector *mapVec, int32_t row, const DataType *keyType,
    const DataType *valType, std::string &out) const
{
    out.push_back('{');
    int64_t startOffset = mapVec->GetOffset(row);
    int64_t mapSize = mapVec->GetSize(row);
    BaseVector *keyVec = mapVec->GetKeyVector().get();
    BaseVector *valVec = mapVec->GetValueVector().get();
    DataTypeId keyTypeId = (keyType != nullptr) ? keyType->GetId() : keyVec->GetTypeId();
    for (int64_t i = 0; i < mapSize; ++i) {
        if (i > 0) out.push_back(',');
        int32_t idx = static_cast<int32_t>(startOffset + i);
        // Flink JSON_STRING requires MAP keys to be character strings; the key is always
        // emitted as a JSON string (quoted). Numeric keys are coerced into a quoted form.
        if (keyTypeId == OMNI_VARCHAR || keyTypeId == OMNI_CHAR) {
            std::string_view keySv = getStringFromVector(keyVec, idx);
            out.push_back('"');
            escapeJsonString(keySv, out);
            out.push_back('"');
        } else {
            // Non-string key: serialize the scalar and wrap in quotes (Flink accepts only
            // STRING keys, so this is a best-effort fallback for native parity tests).
            std::string keyStr;
            out.push_back('"');
            appendToJson(keyVec, idx, keyType, keyStr);
            escapeJsonString(std::string_view(keyStr), out);
            out.push_back('"');
        }
        out.push_back(':');
        if (valVec->IsNull(idx)) {
            out.append("null");
        } else {
            appendToJson(valVec, idx, valType, out);
        }
    }
    out.push_back('}');
}

void JsonStringFunction::appendRowToJson(RowVector *rowVec, int32_t row, const RowType *rowType,
    std::string &out) const
{
    out.push_back('{');
    int32_t childCount = rowVec->ChildSize();
    // Use real struct field names when the RowType carries them; otherwise fall back to field{i}.
    bool hasNames = (rowType != nullptr) && (rowType->names().size() >= static_cast<size_t>(childCount));
    for (int32_t i = 0; i < childCount; ++i) {
        BaseVector *childVec = rowVec->ChildAt(i).get();
        // Flink JSON_STRING keeps NULL struct fields (emits "field":null), unlike Spark
        // to_json which skips them. So we always emit the field, then null or the value.
        if (i > 0) out.push_back(',');
        std::string fieldName = hasNames ? rowType->nameOf(i) : ("field" + std::to_string(i));
        out.push_back('"');
        escapeJsonString(std::string_view(fieldName), out);
        out.append("\":");
        if (childVec->IsNull(row)) {
            out.append("null");
        } else {
            const DataType *childType = (rowType != nullptr) ? rowType->childAt(i).get() : nullptr;
            appendToJson(childVec, row, childType, out);
        }
    }
    out.push_back('}');
}

void JsonStringFunction::escapeJsonString(const std::string_view &s, std::string &out) const
{
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        switch (c) {
            case '"': out.append("\\\""); break;
            case '\\': out.append("\\\\"); break;
            case '\b': out.append("\\b"); break;
            case '\f': out.append("\\f"); break;
            case '\n': out.append("\\n"); break;
            case '\r': out.append("\\r"); break;
            case '\t': out.append("\\t"); break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    // Control characters -> \uXXXX (lowercase hex, per JSON spec / Jackson default).
                    char buf[7];
                    snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                    out.append(buf, 6);
                } else {
                    out.push_back(c);
                }
        }
    }
}

std::string JsonStringFunction::formatDouble(double v) const
{
    // Match ToJsonFunction's behavior: default ostringstream precision (6 significant
    // digits) already produces compact output ("3.14", "1.5", "1" for 1.0). We guard
    // NaN/Inf explicitly since JSON has no literal for them and the default stream
    // output ("nan"/"inf") would produce invalid JSON.
    if (std::isnan(v)) {
        return "\"NaN\"";
    }
    if (std::isinf(v)) {
        return v > 0 ? "\"Infinity\"" : "\"-Infinity\"";
    }
    std::ostringstream oss;
    oss << v;
    return oss.str();
}

template <typename T>
T JsonStringFunction::getValueFromVector(BaseVector *vec, int32_t row) const
{
    Encoding encoding = vec->GetEncoding();
    if (encoding == OMNI_ENCODING_CONST) {
        auto *constVec = static_cast<ConstVector<T> *>(vec);
        return constVec->GetConstValue();
    }
    if (encoding == OMNI_FLAT) {
        auto *flatVec = static_cast<Vector<T> *>(vec);
        return flatVec->GetValue(row);
    }
    if (encoding == OMNI_DICTIONARY) {
        auto *dictVec = static_cast<Vector<DictionaryContainer<T>> *>(vec);
        return dictVec->GetValue(row);
    }
    OMNI_THROW("JsonStringFunction Error:", "Unsupported encoding");
}

std::string_view JsonStringFunction::getStringFromVector(BaseVector *vec, int32_t row) const
{
    Encoding encoding = vec->GetEncoding();
    if (encoding == OMNI_ENCODING_CONST) {
        auto *constVec = static_cast<ConstVector<std::string_view> *>(vec);
        return constVec->GetConstValue();
    }
    if (encoding == OMNI_FLAT) {
        auto *flatVec = static_cast<Vector<LargeStringContainer<std::string_view>> *>(vec);
        return flatVec->GetValue(row);
    }
    if (encoding == OMNI_DICTIONARY) {
        auto *dictVec = static_cast<Vector<DictionaryContainer<std::string_view, LargeStringContainer>> *>(vec);
        return dictVec->GetValue(row);
    }
    OMNI_THROW("JsonStringFunction Error:", "Unsupported encoding for string");
}

// Explicit template instantiations (match ToJsonFunction).
template bool JsonStringFunction::getValueFromVector<bool>(BaseVector *, int32_t) const;
template int8_t JsonStringFunction::getValueFromVector<int8_t>(BaseVector *, int32_t) const;
template int16_t JsonStringFunction::getValueFromVector<int16_t>(BaseVector *, int32_t) const;
template int32_t JsonStringFunction::getValueFromVector<int32_t>(BaseVector *, int32_t) const;
template int64_t JsonStringFunction::getValueFromVector<int64_t>(BaseVector *, int32_t) const;
template float JsonStringFunction::getValueFromVector<float>(BaseVector *, int32_t) const;
template double JsonStringFunction::getValueFromVector<double>(BaseVector *, int32_t) const;
}
