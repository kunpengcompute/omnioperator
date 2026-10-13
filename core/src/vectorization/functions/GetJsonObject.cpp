/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: get_json_object function implementation
 */

 #include "GetJsonObject.h"
 #include "vector/vector.h"
 #include "util/debug.h"
 #include "rapidjson/document.h"
 #include "rapidjson/stringbuffer.h"
 #include "rapidjson/writer.h"
 #include <sstream>
 #include <iomanip>
 #include <cctype>
 
 namespace omniruntime::vectorization {
 using namespace omniruntime::type;
 using namespace omniruntime::vec;
 using namespace omniruntime::op;
 
 void GetJsonObjectFunction::Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType,
     BaseVector *&result, ExecutionContext *context) const
 {
     if (args.size() < 2) {
         OMNI_THROW("GetJsonObjectFunction Error:", "Requires at least 2 arguments: json string and path");
     }
 
     // Pop arguments in reverse order (stack is LIFO)
     auto *pathArg = args.top();
     args.pop();
     auto *jsonArg = args.top();
     args.pop();
 
     int32_t rowSize = context->GetResultRowSize();
     result = VectorHelper::CreateFlatVector(context->GetPool(), OMNI_VARCHAR, rowSize);
     if (!result) {
         OMNI_THROW("GetJsonObjectFunction Error:", "Failed to create result vector");
     }
 
     auto *resultVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(result);
     if (!resultVec) {
         OMNI_THROW("GetJsonObjectFunction Error:", "Result vector is not a string vector");
     }
 
    bool isConstPath = pathArg->GetEncoding() == OMNI_ENCODING_CONST;
    std::string normalizedConstPath;
    bool isConstPathInvalid = false;
    if (isConstPath && rowSize > 0 && !pathArg->IsNull(0)) {
        normalizedConstPath = NormalizeJsonPath(GetStringValue(pathArg, 0));
        isConstPathInvalid = normalizedConstPath == "-1";
    }

     // Process each row
     for (int32_t row = 0; row < rowSize; ++row) {
         // Check if either argument is null
         if (jsonArg->IsNull(row) || pathArg->IsNull(row)) {
             resultVec->SetNull(row);
             continue;
         }
 
         std::string_view jsonStr = GetStringValue(jsonArg, row);
        if (isConstPath) {
            if (isConstPathInvalid) {
                resultVec->SetNull(row);
                continue;
            }
            ProcessJsonPathWithNormalizedPath(jsonStr, normalizedConstPath, resultVec, row);
        } else {
            std::string_view pathStr = GetStringValue(pathArg, row);
            ProcessJsonPath(jsonStr, pathStr, resultVec, row);
        }
     }
 
     delete jsonArg;
     delete pathArg;
 }
 
 void GetJsonObjectFunction::ProcessJsonPath(const std::string_view &jsonStr, const std::string_view &pathStr,
     Vector<LargeStringContainer<std::string_view>> *resultVec, int32_t row) const
 {
     // Validate and normalize the JSON path
     std::string normalizedPath = NormalizeJsonPath(pathStr);
     if (normalizedPath == "-1") {
         resultVec->SetNull(row);
         return;
     }
    ProcessJsonPathWithNormalizedPath(jsonStr, normalizedPath, resultVec, row);
}
 
void GetJsonObjectFunction::ProcessJsonPathWithNormalizedPath(const std::string_view &jsonStr,
    const std::string_view &normalizedPath, Vector<LargeStringContainer<std::string_view>> *resultVec, int32_t row) const
{
     // If path is just "$", return the entire JSON string
     if (normalizedPath.empty()) {
         resultVec->SetValue(row, jsonStr);
         resultVec->SetNotNull(row);
         return;
     }
 
     // Parse the JSON document
     rapidjson::Document doc;
     doc.Parse<rapidjson::kParseNoFlags>(jsonStr.data(), jsonStr.size());
 
     if (doc.HasParseError()) {
         resultVec->SetNull(row);
         return;
     }
 
     // Navigate through the path and extract value
     const rapidjson::Value *currentValue = &doc;
     size_t pos = 0;
 
     while (pos < normalizedPath.length()) {
         if (normalizedPath[pos] == '.') {
             // Object field access: .fieldName
             pos++;
             size_t endPos = pos;
             while (endPos < normalizedPath.length() &&
                    normalizedPath[endPos] != '.' &&
                    normalizedPath[endPos] != '[') {
                 endPos++;
             }
            std::string_view fieldName = normalizedPath.substr(pos, endPos - pos);
 
             if (!currentValue->IsObject()) {
                 resultVec->SetNull(row);
                 return;
             }
 
            rapidjson::Value fieldNameValue(rapidjson::StringRef(fieldName.data(),
                static_cast<rapidjson::SizeType>(fieldName.size())));
            auto it = currentValue->FindMember(fieldNameValue);
             if (it == currentValue->MemberEnd()) {
                 resultVec->SetNull(row);
                 return;
             }
             currentValue = &it->value;
             pos = endPos;
         } else if (normalizedPath[pos] == '[') {
             // Array access: [index] or [fieldName] (after RemoveSingleQuotes)
             pos++;
             size_t endPos = normalizedPath.find(']', pos);
             if (endPos == std::string::npos) {
                 resultVec->SetNull(row);
                 return;
             }
 
             std::string indexStr(normalizedPath.substr(pos, endPos - pos));
 
             // Check if content is a pure integer (array index) or field name
             bool isPureInteger = true;
             if (indexStr.empty()) {
                 isPureInteger = false;
             } else {
                 size_t start = 0;
                 if (indexStr[0] == '-') {
                     start = 1;
                     if (indexStr.length() == 1) isPureInteger = false;
                 }
                 for (size_t i = start; i < indexStr.length() && isPureInteger; ++i) {
                     if (!std::isdigit(static_cast<unsigned char>(indexStr[i]))) {
                         isPureInteger = false;
                     }
                 }
             }
 
             if (isPureInteger) {
                 // It's an array index
                 if (!currentValue->IsArray()) {
                     resultVec->SetNull(row);
                     return;
                 }
 
                 int index = std::stoi(indexStr);
                 if (index < 0 || static_cast<size_t>(index) >= currentValue->Size()) {
                     resultVec->SetNull(row);
                     return;
                 }
                 currentValue = &(*currentValue)[index];
             } else {
                 // It's a field name (could be quoted like ['a'] or unquoted after RemoveSingleQuotes like [a])
                 if (!currentValue->IsObject()) {
                     resultVec->SetNull(row);
                     return;
                 }
 
// Remove single quotes if present (double quotes are not supported by native Spark/Hive)
                 std::string_view fieldName(indexStr);
                 if (fieldName.length() >= 2 &&
                     fieldName[0] == '\'' && fieldName[fieldName.length() - 1] == '\'') {
                     fieldName.remove_prefix(1);
                     fieldName.remove_suffix(1);
                  }
 
                rapidjson::Value fieldNameValue(rapidjson::StringRef(fieldName.data(),
                    static_cast<rapidjson::SizeType>(fieldName.size())));
                auto it = currentValue->FindMember(fieldNameValue);
                 if (it == currentValue->MemberEnd()) {
                     resultVec->SetNull(row);
                     return;
                 }
                 currentValue = &it->value;
             }
             pos = endPos + 1;
         } else {
             // Unexpected character in path
             resultVec->SetNull(row);
             return;
         }
     }
 
     // Extract the value as string
     this->ExtractValueAsString(*currentValue, resultVec, row);
 }
 
 std::string GetJsonObjectFunction::NormalizeJsonPath(std::string_view pathStr) const
 {
     // Check if path starts with '$'
     if (pathStr.empty() || pathStr[0] != '$') {
         return "-1";
     }
 
     // First, remove single quotes from bracket notation
     std::string path = RemoveSingleQuotes(std::string(pathStr));
     if (path == "-1") {
         return "-1";
     }
 
     std::string result;
     result.reserve(path.length());
 
     // State machine for parsing
     enum class State {
         kAfterDollar,
         kAfterDot,
         kInToken,
         kInBracket
     };
 
     State state = State::kAfterDollar;
 
     for (size_t i = 1; i < path.length(); ++i) {
         char c = path[i];
 
         if (c == ' ') {
             if (state == State::kInToken) {
                 // Spaces within tokens are preserved
                 result.push_back(c);
             }
             continue;
         }
 
         switch (state) {
             case State::kAfterDollar:
                 if (c == '.') {
                     state = State::kAfterDot;
                     result.push_back(c);
                 } else if (c == '[') {
                     state = State::kInBracket;
                     result.push_back(c);
                 } else {
                     // Invalid: must have . or [ after $
                     return "-1";
                 }
                 break;
 
             case State::kAfterDot:
                 if (c == '.') {
                     // Consecutive dots are invalid
                     return "-1";
                 }
                 result.push_back(c);
                 state = State::kInToken;
                 break;
 
             case State::kInToken:
                 if (c == '.') {
                     result.push_back(c);
                     state = State::kAfterDot;
                 } else if (c == '[') {
                     result.push_back(c);
                     state = State::kInBracket;
                 } else {
                     result.push_back(c);
                 }
                 break;
 
             case State::kInBracket:
                 if (c == ']') {
                     result.push_back(c);
                     state = State::kInToken;
                 } else {
                     result.push_back(c);
                 }
                 break;
         }
     }
 
     // Trailing dot is invalid
     if (state == State::kAfterDot) {
         return "-1";
     }
 
     // If result is empty (just "$"), return empty string
     return result;
 }
 
 // Remove single quotes from bracket notation like $['a']['b'] -> $[a][b]
 std::string GetJsonObjectFunction::RemoveSingleQuotes(const std::string &path) const
 {
     std::string result;
     result.reserve(path.size());
 
     for (size_t i = 0; i < path.size(); ++i) {
         // Skip single quotes inside brackets
         if (path[i] == '[' && i + 1 < path.size()) {
             size_t bracketEnd = path.find(']', i);
             if (bracketEnd == std::string::npos) {
                 return "-1";  // Missing closing bracket
             }
 
             // Copy the opening bracket
             result.push_back('[');
             i++;
 
             // Skip opening quote if present
             if (i < path.size() && path[i] == 0x27) {  // Single quote character
                 i++;
             }
 
             // Copy content until closing quote or bracket
             while (i < bracketEnd) {
                 if (path[i] == 0x27) {  // Skip closing quote
                     i++;
                     break;
                 }
                 result.push_back(path[i]);
                 i++;
             }
 
             // Copy closing bracket
             if (i <= bracketEnd) {
                 result.push_back(']');
                 i = bracketEnd;
             }
         } else {
             result.push_back(path[i]);
         }
     }
 
     return result;
 }
 
 void GetJsonObjectFunction::ExtractValueAsString(const rapidjson::Value &value,
     Vector<LargeStringContainer<std::string_view>> *resultVec, int32_t row) const
 {
     std::string valueStr;
 
     switch (value.GetType()) {
         case rapidjson::kNullType:
             resultVec->SetNull(row);
             return;
 
         case rapidjson::kStringType:
             valueStr = std::string(value.GetString(), value.GetStringLength());
             break;
 
         case rapidjson::kNumberType:
             if (value.IsInt()) {
                 valueStr = std::to_string(value.GetInt());
             } else if (value.IsInt64()) {
                 valueStr = std::to_string(value.GetInt64());
             } else if (value.IsUint()) {
                 valueStr = std::to_string(value.GetUint());
             } else if (value.IsUint64()) {
                 valueStr = std::to_string(value.GetUint64());
             } else if (value.IsDouble()) {
                 valueStr = FormatDouble(value.GetDouble());
             } else {
                 resultVec->SetNull(row);
                 return;
             }
             break;
 
         case rapidjson::kTrueType:
             valueStr = "true";
             break;
 
         case rapidjson::kFalseType:
             valueStr = "false";
             break;
 
         case rapidjson::kObjectType:
         case rapidjson::kArrayType:
             // For objects and arrays, convert to JSON string
             {
                 rapidjson::StringBuffer buffer;
                 rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                 value.Accept(writer);
                 valueStr = std::string(buffer.GetString(), buffer.GetSize());
             }
             break;
 
         default:
             resultVec->SetNull(row);
             return;
     }
 
     std::string_view sv(valueStr);
     resultVec->SetValue(row, sv);
     resultVec->SetNotNull(row);
 }
 
 // Format double to string, removing trailing zeros
 std::string GetJsonObjectFunction::FormatDouble(double value) const
 {
     std::ostringstream oss;
     oss << std::setprecision(15) << value;
     std::string str = oss.str();
 
     // Remove trailing zeros after decimal point
     if (str.find('.') != std::string::npos) {
         // Remove trailing zeros
         while (!str.empty() && str.back() == '0') {
             str.pop_back();
         }
         // If ends with just '.', add a '0' to make it valid
         if (!str.empty() && str.back() == '.') {
             str.push_back('0');
         }
     }
 
     return str;
 }
 
 std::string_view GetJsonObjectFunction::GetStringValue(BaseVector *vector, int32_t row) const
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
             OMNI_THROW("GetJsonObjectFunction Error:", "Unsupported encoding type");
     }
 }
 }
 