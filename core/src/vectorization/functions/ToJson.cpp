/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: to_json function implementation
 */

#include "ToJson.h"
#include "vector/vector.h"
#include "type/data_type.h"
#include "util/type_util.h"

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;

void ToJsonFunction::Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType,
    BaseVector *&result, ExecutionContext *context) const
{
    auto *inputArg = args.top();
    args.pop();
    // Input DataType carries struct field names (set by the evaluator); may be null if
    // unavailable, in which case we fall back to field0/field1/... key names.
    const DataType *inputType = (context != nullptr) ? context->GetToJsonInputType() : nullptr;
    int32_t rowSize = inputArg->GetSize();
    auto *stringResult = new Vector<LargeStringContainer<std::string_view>>(rowSize);
    stringResult->SetIsField(true);
    for (int32_t row = 0; row < rowSize; ++row) {
        if (inputArg->IsNull(row)) {
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

void ToJsonFunction::appendToJson(BaseVector *vec, int32_t row, const DataType *type, std::string &out) const
{
    DataTypeId typeId = (type != nullptr) ? type->GetId() : vec->GetTypeId();
    switch (typeId) {
        case OMNI_ROW: {
            auto *rowVec = static_cast<RowVector *>(vec);
            const RowType *rowType = (type != nullptr) ? static_cast<const RowType *>(type) : nullptr;
            appendRowToJson(rowVec, row, rowType, out);
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
        case OMNI_INT:
        case OMNI_DATE32: {
            int32_t v = getValueFromVector<int32_t>(vec, row);
            out.append(std::to_string(v));
            break;
        }
        case OMNI_LONG:
        case OMNI_TIMESTAMP:
        case OMNI_DECIMAL64: {
            int64_t v = getValueFromVector<int64_t>(vec, row);
            out.append(std::to_string(v));
            break;
        }
        case OMNI_FLOAT: {
            float v = getValueFromVector<float>(vec, row);
            std::ostringstream oss;
            oss << v;
            out.append(oss.str());
            break;
        }
        case OMNI_DOUBLE: {
            double v = getValueFromVector<double>(vec, row);
            std::ostringstream oss;
            oss << v;
            out.append(oss.str());
            break;
        }
        case OMNI_VARCHAR:
        case OMNI_CHAR:
        case OMNI_VARBINARY: {
            std::string_view sv = getStringFromVector(vec, row);
            out.push_back('"');
            escapeJsonString(sv, out);
            out.push_back('"');
            break;
        }
        default:
            OMNI_THROW("ToJsonFunction Error:", "Unsupported type");
    }
}

void ToJsonFunction::appendToJsonFromSlice(BaseVector *vec, int32_t startIdx, int32_t count, const DataType *type, std::string &out) const
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

void ToJsonFunction::appendArrayToJson(ArrayVector *arrVec, int32_t row, const DataType *elemType, std::string &out) const
{
    out.push_back('[');
    int64_t startOffset = arrVec->GetOffset(row);
    int64_t arrSize = arrVec->GetSize(row);
    BaseVector *elemVec = arrVec->GetElementVector().get();
    appendToJsonFromSlice(elemVec, static_cast<int32_t>(startOffset), static_cast<int32_t>(arrSize), elemType, out);
    out.push_back(']');
}

void ToJsonFunction::appendMapToJson(MapVector *mapVec, int32_t row, const DataType *keyType, const DataType *valType, std::string &out) const
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
        if (keyTypeId == OMNI_VARCHAR || keyTypeId == OMNI_CHAR) {
            std::string_view keySv = getStringFromVector(keyVec, idx);
            out.push_back('"');
            escapeJsonString(keySv, out);
            out.push_back('"');
        } else {
            out.push_back('"');
            appendToJson(keyVec, idx, keyType, out);
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

void ToJsonFunction::appendRowToJson(RowVector *rowVec, int32_t row, const RowType *rowType, std::string &out) const
{
    out.push_back('{');
    int32_t childCount = rowVec->ChildSize();
    // Use real struct field names when the RowType carries them; otherwise fall back to field{i}.
    bool hasNames = (rowType != nullptr) && (rowType->names().size() >= static_cast<size_t>(childCount));
    bool firstEmitted = false;
    for (int32_t i = 0; i < childCount; ++i) {
        BaseVector *childVec = rowVec->ChildAt(i).get();
        // Spark to_json defaults to ignoreNullFields=true: a struct field whose value is null
        // is omitted from the output (recursively for nested structs).
        if (childVec->IsNull(row)) {
            continue;
        }
        if (firstEmitted) out.push_back(',');
        firstEmitted = true;
        std::string fieldName = hasNames ? rowType->nameOf(i) : ("field" + std::to_string(i));
        out.push_back('"');
        escapeJsonString(std::string_view(fieldName), out);
        out.append("\":");
        const DataType *childType = (rowType != nullptr) ? rowType->childAt(i).get() : nullptr;
        appendToJson(childVec, row, childType, out);
    }
    out.push_back('}');
}

void ToJsonFunction::escapeJsonString(const std::string_view &s, std::string &out) const
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
                    char buf[7];
                    snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                    out.append(buf, 6);
                } else {
                    out.push_back(c);
                }
        }
    }
}

template <typename T>
T ToJsonFunction::getValueFromVector(BaseVector *vec, int32_t row) const
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
    OMNI_THROW("ToJsonFunction Error:", "Unsupported encoding");
}

std::string_view ToJsonFunction::getStringFromVector(BaseVector *vec, int32_t row) const
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
    OMNI_THROW("ToJsonFunction Error:", "Unsupported encoding for string");
}

template bool ToJsonFunction::getValueFromVector<bool>(BaseVector *, int32_t) const;
template int8_t ToJsonFunction::getValueFromVector<int8_t>(BaseVector *, int32_t) const;
template int16_t ToJsonFunction::getValueFromVector<int16_t>(BaseVector *, int32_t) const;
template int32_t ToJsonFunction::getValueFromVector<int32_t>(BaseVector *, int32_t) const;
template int64_t ToJsonFunction::getValueFromVector<int64_t>(BaseVector *, int32_t) const;
template float ToJsonFunction::getValueFromVector<float>(BaseVector *, int32_t) const;
template double ToJsonFunction::getValueFromVector<double>(BaseVector *, int32_t) const;
}
