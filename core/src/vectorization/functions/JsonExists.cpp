/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: JSON_EXISTS function implementation
 *
 * json_exists(jsonValue, path [, onError]) -> boolean. See JsonExists.h for full semantics.
 */

#include "JsonExists.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <memory>

namespace omniruntime::vectorization {

namespace {
// RAII guard for BaseVector pointers popped from the args stack. Ownership of every
// popped arg transfers to Apply; this ensures all exception paths (including the
// ON ERROR = ERROR OMNI_THROW inside the row loop and GetStringValue throws) release
// them, mirroring the ArrayContainsFunction cleanup pattern.
using VectorGuard = std::unique_ptr<BaseVector, void (*)(BaseVector *)>;
VectorGuard MakeVectorGuard(BaseVector *p)
{
    return VectorGuard(p, [](BaseVector *v) { delete v; });
}
} // namespace

namespace {
// Case-insensitive ASCII equals for a leading keyword token.
bool StartsWithKeyword(std::string_view s, const char *keyword, size_t &consumed)
{
    size_t klen = std::strlen(keyword);
    if (s.size() < klen) {
        return false;
    }
    for (size_t i = 0; i < klen; ++i) {
        if (std::tolower(static_cast<unsigned char>(s[i])) != std::tolower(static_cast<unsigned char>(keyword[i]))) {
            return false;
        }
    }
    consumed = klen;
    return true;
}
} // namespace

void JsonExistsFunction::Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType,
    BaseVector *&result, ExecutionContext *context) const
{
    if (args.size() < 2) {
        OMNI_THROW("JsonExists function Error:", "Expected 2 or 3 arguments");
    }

    // Pop arguments in reverse order (stack is LIFO): [onError?] path json
    BaseVector *onErrorArg = nullptr;
    if (args.size() >= 3) {
        onErrorArg = args.top();
        args.pop();
    }
    auto *pathArg = args.top();
    args.pop();
    auto *jsonArg = args.top();
    args.pop();
    // Ownership of the popped args transfers here; guard them so every exception path
    // (result-creation failures, ON ERROR = ERROR throws, GetStringValue throws) releases
    // them. Mirrors the ArrayContainsFunction cleanup pattern.
    auto jsonGuard = MakeVectorGuard(jsonArg);
    auto pathGuard = MakeVectorGuard(pathArg);
    // For a missing 3rd arg, guard a null pointer (delete nullptr is a safe no-op).
    auto onErrorGuard = MakeVectorGuard(onErrorArg);

    const JsonExistsOnError onError = ParseOnError(onErrorArg);

    int32_t rowSize = context->GetResultRowSize();
    if (result == nullptr) {
        result = VectorHelper::CreateFlatVector(OMNI_BOOLEAN, rowSize);
    }
    if (result == nullptr) {
        OMNI_THROW("JsonExists function Error:", "Failed to create result vector");
    }
    auto *resultVec = dynamic_cast<Vector<bool> *>(result);
    if (resultVec == nullptr) {
        OMNI_THROW("JsonExists function Error:", "Result vector is not a FlatVector<bool>");
    }

    rapidjson::Document doc;
    for (int32_t row = 0; row < rowSize; ++row) {
        // NULL input -> NULL output (Flink argsNullable=false short-circuit semantics).
        if (jsonArg->IsNull(row) || pathArg->IsNull(row)) {
            resultVec->SetNull(row);
            continue;
        }

        std::string_view pathStr = GetStringValue(pathArg, row);
        JsonPathMode mode = JsonPathMode::STRICT; // Flink default when no prefix
        std::string normalizedPath = NormalizeJsonPath(pathStr, mode);

        // Invalid path syntax.
        if (normalizedPath == "-1") {
            if (mode == JsonPathMode::LAX) {
                resultVec->SetValue(row, false);
                result->SetNotNull(row);
            } else {
                switch (onError) {
                    case JsonExistsOnError::TRUE: resultVec->SetValue(row, true); result->SetNotNull(row); break;
                    case JsonExistsOnError::FALSE: resultVec->SetValue(row, false); result->SetNotNull(row); break;
                    case JsonExistsOnError::UNKNOWN: resultVec->SetNull(row); break;
                    case JsonExistsOnError::ERROR:
                        OMNI_THROW("JsonExists function Error:", "Invalid JSON path and ON ERROR = ERROR");
                        break;
                }
            }
            continue;
        }

        std::string_view jsonStr = GetStringValue(jsonArg, row);
        doc.Parse<rapidjson::kParseNoFlags>(jsonStr.data(), jsonStr.size());

        // Invalid JSON input.
        if (doc.HasParseError()) {
            if (mode == JsonPathMode::LAX) {
                resultVec->SetValue(row, false);
                result->SetNotNull(row);
            } else {
                switch (onError) {
                    case JsonExistsOnError::TRUE: resultVec->SetValue(row, true); result->SetNotNull(row); break;
                    case JsonExistsOnError::FALSE: resultVec->SetValue(row, false); result->SetNotNull(row); break;
                    case JsonExistsOnError::UNKNOWN: resultVec->SetNull(row); break;
                    case JsonExistsOnError::ERROR:
                        OMNI_THROW("JsonExists function Error:", "Invalid JSON input and ON ERROR = ERROR");
                        break;
                }
            }
            continue;
        }

        JsonPathResult pr = PathExists(doc, normalizedPath);
        if (pr == JsonPathResult::FOUND) {
            resultVec->SetValue(row, true);
            result->SetNotNull(row);
        } else if (pr == JsonPathResult::NULL_VALUE) {
            // A JSON null leaf is "not exists" but NOT an error — Flink's Jayway returns a
            // Java null (context.obj == null) without throwing, so jsonExists = FALSE in
            // both LAX and STRICT, and ON ERROR does NOT apply.
            resultVec->SetValue(row, false);
            result->SetNotNull(row);
        } else if (mode == JsonPathMode::LAX) {
            // NOT_FOUND: LAX suppresses -> FALSE.
            resultVec->SetValue(row, false);
            result->SetNotNull(row);
        } else {
            // NOT_FOUND in STRICT: an error -> ON ERROR.
            switch (onError) {
                case JsonExistsOnError::TRUE: resultVec->SetValue(row, true); result->SetNotNull(row); break;
                case JsonExistsOnError::FALSE: resultVec->SetValue(row, false); result->SetNotNull(row); break;
                case JsonExistsOnError::UNKNOWN: resultVec->SetNull(row); break;
                case JsonExistsOnError::ERROR:
                    OMNI_THROW("JsonExists function Error:", "Path not found in STRICT mode and ON ERROR = ERROR");
                    break;
            }
        }
    }

    // Normal path: release ownership explicitly to preserve the original delete-input
    // contract observed by sibling JSON functions. On any exception path above, the
    // guards' destructors release the args during stack unwinding.
    jsonGuard.reset();
    pathGuard.reset();
    onErrorGuard.reset();
}

JsonExistsOnError JsonExistsFunction::ParseOnError(BaseVector *onErrorArg) const
{
    if (onErrorArg == nullptr || onErrorArg->IsNull(0)) {
        return JsonExistsOnError::FALSE; // Flink default
    }
    std::string_view sv = GetStringValue(onErrorArg, 0);
    // Case-insensitive compare, tolerating surrounding whitespace.
    auto eq = [&](const char *kw) {
        size_t klen = std::strlen(kw);
        return sv.size() == klen &&
            std::equal(sv.begin(), sv.end(), kw,
                [](char a, char b) {
                    return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
                });
    };
    if (eq("TRUE")) return JsonExistsOnError::TRUE;
    if (eq("UNKNOWN")) return JsonExistsOnError::UNKNOWN;
    if (eq("ERROR")) return JsonExistsOnError::ERROR;
    return JsonExistsOnError::FALSE; // includes "FALSE" and anything unrecognized
}

std::string JsonExistsFunction::NormalizeJsonPath(std::string_view pathStr, JsonPathMode &mode) const
{
    // Strip leading whitespace, then an optional strict/lax prefix (case-insensitive),
    // then whitespace before '$'. Mirrors Flink's JSON_PATH_BASE regex. Default mode is
    // STRICT (set by caller); a recognized prefix overrides it.
    size_t i = 0;
    while (i < pathStr.size() && std::isspace(static_cast<unsigned char>(pathStr[i]))) {
        ++i;
    }

    // Optional strict/lax prefix (case-insensitive), followed by mandatory whitespace —
    // mirrors Flink's JSON_PATH_BASE regex `^(strict|lax)\s+(.+)$`. When absent, default
    // STRICT (set by caller).
    size_t consumed = 0;
    if (StartsWithKeyword(pathStr.substr(i), "strict", consumed)) {
        size_t after = i + consumed;
        if (after < pathStr.size() && std::isspace(static_cast<unsigned char>(pathStr[after]))) {
            mode = JsonPathMode::STRICT;
            i = after;
        }
    } else if (StartsWithKeyword(pathStr.substr(i), "lax", consumed)) {
        size_t after = i + consumed;
        if (after < pathStr.size() && std::isspace(static_cast<unsigned char>(pathStr[after]))) {
            mode = JsonPathMode::LAX;
            i = after;
        }
    }
    while (i < pathStr.size() && std::isspace(static_cast<unsigned char>(pathStr[i]))) {
        ++i;
    }

    std::string_view rest = pathStr.substr(i);
    if (rest.empty() || rest[0] != '$') {
        return "-1";
    }

    // Reuse GetJsonObject-style normalization on the "$..." remainder.
    std::string path = RemoveSingleQuotes(std::string(rest));
    if (path == "-1") {
        return "-1";
    }

    std::string result;
    result.reserve(path.length());

    enum class State {
        kAfterDollar,
        kAfterDot,
        kInToken,
        kInBracket
    };

    State state = State::kAfterDollar;
    for (size_t k = 1; k < path.length(); ++k) {
        char c = path[k];
        if (c == ' ') {
            // Spaces inside dot-tokens and bracket keys are significant key characters;
            // spaces outside keys (after '$', after '.') are insignificant and dropped.
            if (state == State::kInToken || state == State::kInBracket) {
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
                    return "-1";
                }
                break;
            case State::kAfterDot:
                if (c == '.') {
                    return "-1"; // consecutive dots
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

    if (state == State::kAfterDot) {
        return "-1"; // trailing dot
    }
    // If result is empty (just "$"), return empty string -> PathExists treats it as root.
    return result;
}

std::string JsonExistsFunction::RemoveSingleQuotes(const std::string &path) const
{
    std::string result;
    result.reserve(path.size());
    for (size_t i = 0; i < path.size(); ++i) {
        if (path[i] == '[' && i + 1 < path.size()) {
            size_t bracketEnd = path.find(']', i);
            if (bracketEnd == std::string::npos) {
                return "-1"; // missing closing bracket
            }
            result.push_back('[');
            i++;
            if (i < path.size() && path[i] == 0x27) { // single quote
                i++;
            }
            while (i < bracketEnd) {
                if (path[i] == 0x27) { // skip closing quote
                    i++;
                    break;
                }
                result.push_back(path[i]);
                i++;
            }
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

JsonPathResult JsonExistsFunction::PathExists(const rapidjson::Value &doc, const std::string &normalizedPath) const
{
    // Empty path means "$" (root): the document itself. Flink's Jayway returns the whole
    // parsed document; for a successfully-parsed input the root always exists, so FOUND.
    // (A JSON-null document "null" reaches HasParseError()==false here only when the input
    //  is the literal string "null"; that case is handled by the caller as NULL_VALUE below.)
    if (normalizedPath.empty()) {
        return doc.IsNull() ? JsonPathResult::NULL_VALUE : JsonPathResult::FOUND;
    }

    const rapidjson::Value *currentValue = &doc;
    size_t pos = 0;

    while (pos < normalizedPath.length()) {
        if (normalizedPath[pos] == '.') {
            pos++;
            size_t endPos = pos;
            while (endPos < normalizedPath.length() &&
                   normalizedPath[endPos] != '.' &&
                   normalizedPath[endPos] != '[') {
                endPos++;
            }
            std::string_view fieldName = normalizedPath.substr(pos, endPos - pos);
            if (!currentValue->IsObject()) {
                return JsonPathResult::NOT_FOUND;
            }
            rapidjson::Value fieldNameValue(rapidjson::StringRef(fieldName.data(),
                static_cast<rapidjson::SizeType>(fieldName.size())));
            auto it = currentValue->FindMember(fieldNameValue);
            if (it == currentValue->MemberEnd()) {
                return JsonPathResult::NOT_FOUND;
            }
            currentValue = &it->value;
            pos = endPos;
        } else if (normalizedPath[pos] == '[') {
            pos++;
            size_t endPos = normalizedPath.find(']', pos);
            if (endPos == std::string::npos) {
                return JsonPathResult::NOT_FOUND;
            }
            std::string indexStr(normalizedPath.substr(pos, endPos - pos));

            // Pure integer -> array index; otherwise -> object field name.
            bool isPureInteger = true;
            if (indexStr.empty()) {
                isPureInteger = false;
            } else {
                size_t start = 0;
                if (indexStr[0] == '-') {
                    start = 1;
                    if (indexStr.length() == 1) isPureInteger = false;
                }
                for (size_t s = start; s < indexStr.length() && isPureInteger; ++s) {
                    if (!std::isdigit(static_cast<unsigned char>(indexStr[s]))) {
                        isPureInteger = false;
                    }
                }
            }

            if (isPureInteger) {
                if (!currentValue->IsArray()) {
                    return JsonPathResult::NOT_FOUND;
                }
                int index = std::stoi(indexStr);
                if (index < 0 || static_cast<size_t>(index) >= currentValue->Size()) {
                    return JsonPathResult::NOT_FOUND;
                }
                currentValue = &(*currentValue)[index];
            } else {
                if (!currentValue->IsObject()) {
                    return JsonPathResult::NOT_FOUND;
                }
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
                    return JsonPathResult::NOT_FOUND;
                }
                currentValue = &it->value;
            }
            pos = endPos + 1;
        } else {
            return JsonPathResult::NOT_FOUND; // unexpected character
        }
    }
    // Leaf resolved. A JSON null leaf is "not exists" but NOT an error (Flink returns
    // `context.obj != null`, and a JSON null yields a Java null). Any other leaf type
    // means the path exists.
    return currentValue->IsNull() ? JsonPathResult::NULL_VALUE : JsonPathResult::FOUND;
}

std::string_view JsonExistsFunction::GetStringValue(BaseVector *vector, int32_t row) const
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
            OMNI_THROW("JsonExists function Error:", "Unsupported encoding type");
    }
}

} // namespace omniruntime::vectorization
