/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: from_json function implementation
 */

#include "FromJson.h"
#include "vector/vector.h"
#include "type/data_type.h"
#include "util/type_util.h"
#include <iostream>
#include <sstream>

namespace omniruntime::vectorization {
using namespace omniruntime::type;
using namespace omniruntime::vec;

void FromJsonFunction::Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, 
    BaseVector *&result, ExecutionContext *context) const
{
    if (args.empty()) {
        OMNI_THROW("FromJsonFunction Error:", "No arguments provided");
    }

    auto *inputArg = args.top();
    args.pop();
    auto inputGuard = MakeVectorArgGuard(inputArg);

    // Validate output type is ROW
    if (outputType->GetId() != OMNI_ROW) {
        OMNI_THROW("FromJsonFunction Error:", "Output type must be ROW");
    }

    const RowType &rowType = outputType->asRow();
    // Use context->GetResultRowSize() instead of inputArg->GetSize() because
    // input might be a constant vector with size 0, but we need to process the actual row count
    int32_t rowSize = context != nullptr ? context->GetResultRowSize() : inputArg->GetSize();

    // Create result RowVector
    std::vector<std::shared_ptr<BaseVector>> children;
    for (size_t i = 0; i < rowType.Size(); ++i) {
        const DataTypePtr &childType = rowType.childAt(i);
        BaseVector *childVec = VectorHelper::CreateFlatVector(childType->GetId(), rowSize);
        if (!childVec) {
            OMNI_THROW("FromJsonFunction Error:", "Failed to create child vector");
        }
        childVec->SetIsField(true);
        children.emplace_back(std::shared_ptr<BaseVector>(childVec));
    }
    result = new RowVector(rowSize, children);
    if (!result) {
        OMNI_THROW("FromJsonFunction Error:", "Failed to create result RowVector");
    }
    result->SetIsField(true);

    auto *resultVec = static_cast<RowVector *>(result);
    
    // Initialize all rows and child vectors to null first
    for (int32_t row = 0; row < rowSize; ++row) {
        resultVec->SetNull(row);
        for (size_t i = 0; i < rowType.Size(); ++i) {
            BaseVector *childVec = resultVec->ChildAt(i).get();
            childVec->SetNull(row);
        }
    }

    // Process each row
    // For constant vectors, we need to get the value once and apply it to all rows
    bool isConstVector = (inputArg->GetEncoding() == OMNI_ENCODING_CONST);
    std::string_view constJsonStr;
    if (isConstVector) {
        constJsonStr = GetStringValue(inputArg, 0);
    }
    
    for (int32_t row = 0; row < rowSize; ++row) {

        // For constant vectors, check null at index 0, otherwise check at row
        int32_t nullCheckIndex = isConstVector ? 0 : row;
        if (inputArg->IsNull(nullCheckIndex)) {
            resultVec->SetNull(row);
            // Set all child fields to null
            for (size_t i = 0; i < rowType.Size(); ++i) {
                BaseVector *childVec = resultVec->ChildAt(i).get();
                childVec->SetNull(row);
            }
            continue;
        }

        // For constant vectors, use the constant value, otherwise get value at row
        std::string_view jsonStr = isConstVector ? constJsonStr : GetStringValue(inputArg, row);
        ParseJsonToRow(jsonStr, rowType, resultVec, row);
    }
}

void FromJsonFunction::ParseJsonToRow(const std::string_view &jsonStr, const RowType &rowType,
    RowVector *resultVec, int32_t row) const
{
    rapidjson::Document doc;
    doc.Parse<rapidjson::kParseNoFlags>(jsonStr.data(), jsonStr.size());

    // Empty or whitespace-only input contains no JSON token. Spark's from_json returns a
    // NULL row (whole struct null) for such input, not a row of null fields. rapidjson
    // reports kParseErrorDocumentEmpty for "" and whitespace-only strings.
    if (doc.HasParseError() && doc.GetParseError() == rapidjson::kParseErrorDocumentEmpty) {
        resultVec->SetNull(row);
        for (size_t i = 0; i < rowType.Size(); ++i) {
            BaseVector *childVec = resultVec->ChildAt(i).get();
            childVec->SetNull(row);
        }
        return;
    }

    if (doc.HasParseError() || !doc.IsObject()) {
        // Non-empty but malformed JSON or not an object: set all fields to null but keep the
        // row not null. This matches Spark SQL PERMISSIVE behavior (row with null fields).
        resultVec->SetNotNull(row);
        for (size_t i = 0; i < rowType.Size(); ++i) {
            BaseVector *childVec = resultVec->ChildAt(i).get();
            childVec->SetNull(row);
        }
        return;
    }
    
    // Debug: Print all JSON keys
    for (auto it = doc.MemberBegin(); it != doc.MemberEnd(); ++it) {
        if (it->name.IsString()) {
            std::string jsonKey(it->name.GetString(), it->name.GetStringLength());
        }
    }

    resultVec->SetNotNull(row);

    // Extract each field from JSON object
    // Always match by name (case-insensitive), never use positional matching
    // Gluten should now pass the correct field names from the schema
    for (size_t i = 0; i < rowType.Size(); ++i) {
        const std::string &fieldName = rowType.nameOf(i);
        const DataTypePtr &fieldType = rowType.childAt(i);
        BaseVector *fieldVec = resultVec->ChildAt(i).get();

        // Find field in JSON object by name (case-insensitive match)
        std::string lowerFieldName = ToLower(fieldName);
        bool fieldFound = false;
        rapidjson::Value::ConstMemberIterator it;
        
        // Match by name (case-insensitive)
        for (it = doc.MemberBegin(); it != doc.MemberEnd(); ++it) {
            if (it->name.IsString()) {
                std::string jsonKey(it->name.GetString(), it->name.GetStringLength());
                std::string lowerJsonKey = ToLower(jsonKey);
                if (lowerJsonKey == lowerFieldName) {
                    fieldFound = true;
                    break;
                }
            }
        }

        if (!fieldFound) {
            // Field not found in JSON, set to null
            fieldVec->SetNull(row);
            continue;
        }

        const rapidjson::Value &jsonValue = it->value;

        // Extract value based on field type
        if (fieldType->GetId() == OMNI_VARCHAR) {
            ExtractStringField(jsonValue, fieldVec, row);
        } else {
            // For now, only support STRING type
            // Other types can be added later
            fieldVec->SetNull(row);
        }
    }
}

void FromJsonFunction::ExtractStringField(const rapidjson::Value &jsonValue, BaseVector *fieldVec, int32_t row) const
{
    auto *stringVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(fieldVec);
    if (!stringVec) {
        OMNI_THROW("FromJsonFunction Error:", "Field vector is not a string vector");
    }

    if (jsonValue.IsNull()) {
        stringVec->SetNull(row);
        return;
    }

    std::string valueStr;
    if (jsonValue.IsString()) {
        const char* str = jsonValue.GetString();
        size_t len = jsonValue.GetStringLength();
        if (str != nullptr) {
            valueStr.assign(str, len);
        } else {
            valueStr.clear();
        }
    } else if (jsonValue.IsNumber()) {
        // Convert number to string
        if (jsonValue.IsInt()) {
            valueStr = std::to_string(jsonValue.GetInt());
        } else if (jsonValue.IsInt64()) {
            valueStr = std::to_string(jsonValue.GetInt64());
        } else if (jsonValue.IsUint()) {
            valueStr = std::to_string(jsonValue.GetUint());
        } else if (jsonValue.IsUint64()) {
            valueStr = std::to_string(jsonValue.GetUint64());
        } else if (jsonValue.IsDouble()) {
            valueStr = std::to_string(jsonValue.GetDouble());
        } else {
            stringVec->SetNull(row);
            return;
        }
    } else if (jsonValue.IsBool()) {
        valueStr = jsonValue.GetBool() ? "true" : "false";
    } else {
        // For other types (object, array), convert to JSON string
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        jsonValue.Accept(writer);
        const char* bufferStr = buffer.GetString();
        size_t bufferSize = buffer.GetSize();
        if (bufferStr != nullptr) {
            valueStr.assign(bufferStr, bufferSize);
        } else {
            valueStr.clear();
        }
    }

    // Set the value (even if empty string, it's a valid value, not null)
    std::string_view sv(valueStr);
    stringVec->SetValue(row, sv);
    stringVec->SetNotNull(row);
    
    // Verify the value was set correctly
    std::string_view verifyValue = stringVec->GetValue(row);
}

std::string_view FromJsonFunction::GetStringValue(BaseVector *vector, int32_t row) const
{
    switch (vector->GetEncoding()) {
        case OMNI_FLAT: {
            auto *stringVector = static_cast<Vector<LargeStringContainer<std::string_view>> *>(vector);
            return stringVector->GetValue(row);
        }
        case OMNI_DICTIONARY: {
            auto *dictVector =
                static_cast<Vector<DictionaryContainer<std::string_view, LargeStringContainer>> *>(vector);
            return dictVector->GetValue(row);
        }
        case OMNI_ENCODING_CONST: {
            auto *constVector = static_cast<ConstVector<std::string_view> *>(vector);
            return constVector->GetConstValue();
        }
        default:
            OMNI_THROW("FromJsonFunction Error:", "Unsupported encoding type");
    }
}

std::string FromJsonFunction::ToLower(const std::string &str) const
{
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
        [](unsigned char c) { return std::tolower(c); });
    return result;
}
}
