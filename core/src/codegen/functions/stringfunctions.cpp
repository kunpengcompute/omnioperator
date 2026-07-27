/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2025. All rights reserved.
 * Description: registry  function  implementation
 */

#include <cstdint>
#include <limits>
#include <re2/re2.h>
#include <shared_mutex>
#include "stringfunctions.h"
#include "md5.h"
#include "dtoa.h"
#include "type/string_Impl.h"
#include "type/TimestampConversion.h"
#include <nlohmann/json.hpp>
#include <cctype>
#include <functional>
#include <unordered_map>

namespace omniruntime::codegen::function {

const char *INT64_MIN_STR = "-9223372036854775808";

constexpr int32_t INT64_MAX_LEN = 20;

extern "C" DLLEXPORT int64_t CountChar(const char *str, int32_t strLen, const char *target, int32_t targetWidth, int32_t targetLen, bool isNull)
{
    if (isNull) {
        return 0;
    }
    char chr = target[0];
    int64_t count = std::count(str, str + strLen, chr);
    return count;
}

extern "C" DLLEXPORT const char* SplitIndexRetNull(const char *str, int32_t strLen, bool strIsNull, const char *target,
                                                   int32_t targetWidth, int32_t targetLen, bool targetIsNull, int32_t index,
                                                   bool indexIsNull, bool *outIsNull, int32_t *outLen)
{
    if (strIsNull || targetIsNull || indexIsNull) {
        *outIsNull = true;
        *outLen = 0;
        return nullptr;
    }
    size_t start = 0;
    size_t currentIndex = 0;

    for (size_t i = 0; i <= strLen; ++i) {
        if (i == strLen || str[i] == *target) {
            if (currentIndex == index) {
                *outIsNull = false;
                *outLen = i - start;
                return str + start;
            }
            start = i + 1;
            ++currentIndex;
        }
    }
    *outIsNull = true;
    *outLen = 0;
    return nullptr;
}

/**
 * This function is only called when apLen is equal to bpLen. When apLen and bpLen are different,
 * it will directly return false instead of calling StrEquals.
 */
extern "C" DLLEXPORT bool StrEquals(const char *ap, int32_t apLen, const char *bp, int32_t bpLen)
{
    for (int i = 0; i < apLen; ++i) {
        if (ap[i] != bp[i]) {
            return false;
        }
    }
    return true;
}

extern "C" DLLEXPORT int32_t StrCompare(const char *ap, int32_t apLen, const char *bp, int32_t bpLen)
{
    int min = bpLen;
    if (apLen < min) {
        min = apLen;
    }

    int32_t result = memcmp(ap, bp, min);
    if (result != 0) {
        return result;
    } else {
        return apLen - bpLen;
    }
}

extern "C" DLLEXPORT bool LikeStr(const char *str, int32_t strLen, const char *regexToMatch, int32_t regexLen,
    bool isNull)
{
    if (isNull) {
        return false;
    }
    std::string s = std::string(str, strLen);
    std::string r = std::string(regexToMatch, regexLen);

    std::wregex re(StringUtil::ToWideString(r));
    return regex_match(StringUtil::ToWideString(s), re);
}

extern "C" DLLEXPORT bool LikeChar(const char *str, int32_t strWidth, int32_t strLen, const char *regexToMatch,
    int32_t regexLen, bool isNull)
{
    int32_t paddingCount = strWidth - omniruntime::Utf8Util::CountCodePoints(str, strLen);
    std::string originalStr;
    originalStr.reserve(strLen + paddingCount);
    originalStr.append(str, strLen);
    for (int i = 0; i < paddingCount; i++) {
        originalStr.append(" ");
    }
    std::string r = std::string(regexToMatch, regexLen);
    std::wregex re(StringUtil::ToWideString(r));
    return regex_match(StringUtil::ToWideString(originalStr), re);
}

extern "C" DLLEXPORT const char *ConcatStrStr(int64_t contextPtr, const char *ap, int32_t apLen, const char *bp,
    int32_t bpLen, bool isNull, int32_t *outLen)
{
    if (isNull) {
        return nullptr;
    }

    bool hasErr = false;
    const char *ret = StringUtil::ConcatStrDiffWidths(contextPtr, ap, apLen, bp, bpLen, &hasErr, outLen);
    if (hasErr) {
        SetError(contextPtr, CONCAT_ERR_MSG);
        return nullptr;
    }
    return ret;
}

extern "C" DLLEXPORT const char *ConcatCharChar(int64_t contextPtr, const char *ap, int32_t aWidth, int32_t apLen,
    const char *bp, int32_t bWidth, int32_t bpLen, bool isNull, int32_t *outLen)
{
    if (isNull) {
        return nullptr;
    }
    bool hasErr = false;
    const char *ret = StringUtil::ConcatCharDiffWidths(contextPtr, ap, aWidth, apLen, bp, bpLen, &hasErr, outLen);
    if (hasErr) {
        SetError(contextPtr, CONCAT_ERR_MSG);
        return nullptr;
    }
    return ret;
}

extern "C" DLLEXPORT const char* RegexpExtractRetNull(int64_t contextPtr, const char *str, int32_t strLen, bool strIsNull,
                                                      const char *regexToMatch, int32_t regexWidth, int32_t regexLen,
                                                      bool regexIsNull, int32_t group, bool groupIsNull, bool *outIsNull, int32_t *outLen)
{
    if (strIsNull || regexIsNull || groupIsNull) {
        *outIsNull = true;
        *outLen = 0;
        return nullptr;
    }
    std::string s = std::string(str, strLen);
    std::string r = std::string(regexToMatch, regexLen);

    std::wregex re(StringUtil::ToWideString(r));
    std::wstring ws = StringUtil::ToWideString(s);
    std::wsmatch match; // Wide string match results

    if (std::regex_search(ws, match, re) && match.size() > group) {
        int startIdx = match.position(group); // Get start position of group 2
        *outLen = match.length(group);
        auto ret = ArenaAllocatorMalloc(contextPtr, *outLen + 1);
        memcpy(ret, str + startIdx, *outLen + 1);
        return ret;
    } else {
        *outIsNull = true;
        *outLen = 0;
        return nullptr;
    }
}

// JSON parse cache to improve performance for repeated JSON queries
// Uses thread-local storage to avoid synchronization overhead
namespace {
    // Simple hash function for JSON content
    inline uint64_t HashJsonContent(const std::string& content)
    {
        // FNV-1a hash algorithm
        uint64_t hash = 14695981039346656037ULL;
        for (char c : content) {
            hash ^= static_cast<unsigned char>(c);
            hash *= 1099511628211ULL;
        }
        return hash;
    }

    struct JsonCache {
        uint64_t hash = 0;
        nlohmann::json parsedJson;
        std::string lastJsonContent;
        
        bool IsCacheValid(const std::string& jsonContent) const
        {
            return hash == HashJsonContent(jsonContent) && lastJsonContent == jsonContent;
        }
        
        void SetCache(const std::string& jsonContent, const nlohmann::json& json)
        {
            hash = HashJsonContent(jsonContent);
            lastJsonContent = jsonContent;
            parsedJson = json;
        }
    };
    
    // Thread-local cache for JSON parsing
    // This avoids re-parsing the same JSON in the same thread
    thread_local JsonCache THREAD_LOCAL_JSON_CACHE;
}

// Enhanced JSON path parser supporting $.a.b, $[0], $['key'], $["key"], and nested combinations
// Also handles keys with special characters when quoted
static std::vector<std::string> ParseJsonPath(const std::string& path)
{
    std::vector<std::string> keys;
    if (path.empty() || path[0] != '$') {
        return keys;
    }
    
    enum State {
        EXPECT_DOT_OR_BRACKET,  // After key, expect . or [
        IN_DOT_NOTATION,         // After ., reading key until . or [
        IN_BRACKET,              // After [, reading content until ]
        IN_QUOTED_KEY            // Inside quotes within bracket
    };
    
    State state = EXPECT_DOT_OR_BRACKET;
    std::string currentKey;
    char quoteChar = '\0';
    
    for (size_t i = 1; i < path.size(); ++i) {
        char c = path[i];
        
        switch (state) {
            case EXPECT_DOT_OR_BRACKET:
                if (c == '.') {
                    state = IN_DOT_NOTATION;
                } else if (c == '[') {
                    state = IN_BRACKET;
                } else if (!isspace(c)) {
                    // Invalid format, should start with . or [
                    return keys;
                }
                break;
                
            case IN_DOT_NOTATION:
                if (c == '.') {
                    // Save current key when encountering next dot
                    if (!currentKey.empty()) {
                        keys.push_back(currentKey);
                        currentKey.clear();
                    }
                    // Stay in IN_DOT_NOTATION state to read next key
                } else if (c == '[') {
                    if (!currentKey.empty()) {
                        keys.push_back(currentKey);
                        currentKey.clear();
                    }
                    state = IN_BRACKET;
                } else if (isspace(c)) {
                    // Stop at whitespace
                    if (!currentKey.empty()) {
                        keys.push_back(currentKey);
                        currentKey.clear();
                    }
                    state = EXPECT_DOT_OR_BRACKET;
                } else {
                    currentKey += c;
                }
                break;
                
            case IN_BRACKET:
                if (c == '\'' || c == '"') {
                    quoteChar = c;
                    state = IN_QUOTED_KEY;
                } else if (c == ']') {
                    if (!currentKey.empty()) {
                        keys.push_back(currentKey);
                        currentKey.clear();
                    }
                    state = EXPECT_DOT_OR_BRACKET;
                } else if (!isspace(c)) {
                    currentKey += c;
                }
                break;
                
            case IN_QUOTED_KEY:
                if (c == '\\' && i + 1 < path.size()) {
                    // Handle escape sequences
                    char nextChar = path[i + 1];
                    if (nextChar == quoteChar || nextChar == '\\') {
                        currentKey += nextChar;
                        ++i;  // Skip next character
                    } else {
                        currentKey += c;
                    }
                } else if (c == quoteChar) {
                    // End of quoted key, expect ] next
                    state = IN_BRACKET;
                    quoteChar = '\0';
                } else {
                    currentKey += c;
                }
                break;
        }
    }
    
    // Handle remaining key
    if (!currentKey.empty()) {
        keys.push_back(currentKey);
    }
    
    return keys;
}

extern "C" DLLEXPORT const char* JsonValueRetNull(int64_t contextPtr, const char *jsonStr, int32_t jsonStrLen, bool jsonStrIsNull,
                                                   const char *pathStr, int32_t pathStrWidth, int32_t pathStrLen, bool pathStrIsNull,
                                                   bool *outIsNull, int32_t *outLen)
{
    if (outIsNull == nullptr || outLen == nullptr) {
        return nullptr;
    }
    
    if (jsonStrIsNull || pathStrIsNull) {
        *outIsNull = true;
        *outLen = 0;
        return nullptr;
    }
    
    std::string jsonContent(jsonStr, jsonStrLen);
    std::string pathContent(pathStr, pathStrLen);
    
    try {
        // Use cached JSON if available, otherwise parse and cache
        nlohmann::json* jsonData;
        if (THREAD_LOCAL_JSON_CACHE.IsCacheValid(jsonContent)) {
            jsonData = &THREAD_LOCAL_JSON_CACHE.parsedJson;
        } else {
            nlohmann::json newJson;
            // Try parsing the original JSON first
            try {
                newJson = nlohmann::json::parse(jsonContent);
            } catch (...) {
                // If parsing fails, try to fix escaped quotes
                // This handles cases where input has \ instead of " for JSON string delimiters
                std::string fixedJsonContent;
                fixedJsonContent.reserve(jsonContent.size());
                for (size_t j = 0; j < jsonContent.size(); j++) {
                    if (jsonContent[j] == '\\') {
                        if (j + 1 < jsonContent.size()) {
                            char next = jsonContent[j + 1];
                            if (next == '\\' || next == '"') {
                                fixedJsonContent += jsonContent[j];
                            } else {
                                fixedJsonContent += '"';
                            }
                        } else {
                            fixedJsonContent += '"';
                        }
                    } else {
                        fixedJsonContent += jsonContent[j];
                    }
                }
                newJson = nlohmann::json::parse(fixedJsonContent);
                jsonContent = fixedJsonContent; // Update for cache validation
            }
            THREAD_LOCAL_JSON_CACHE.SetCache(jsonContent, newJson);
            jsonData = &THREAD_LOCAL_JSON_CACHE.parsedJson;
        }
        
        std::vector<std::string> keys = ParseJsonPath(pathContent);
        
        // If path parsing failed (empty keys), return null
        if (keys.empty()) {
            *outIsNull = true;
            *outLen = 0;
            return nullptr;
        }
        
        nlohmann::json* current = jsonData;
        for (const auto& key : keys) {
            if (current->is_object()) {
                if (current->contains(key)) {
                    current = &(*current)[key];
                } else {
                    *outIsNull = true;
                    *outLen = 0;
                    return nullptr;
                }
            } else if (current->is_array()) {
                try {
                    size_t index = std::stoul(key);
                    if (index < current->size()) {
                        current = &(*current)[index];
                    } else {
                        *outIsNull = true;
                        *outLen = 0;
                        return nullptr;
                    }
                } catch (...) {
                    *outIsNull = true;
                    *outLen = 0;
                    return nullptr;
                }
            } else {
                *outIsNull = true;
                *outLen = 0;
                return nullptr;
            }
        }
        
        if (current->is_null()) {
            *outIsNull = true;
            *outLen = 0;
            return nullptr;
        }
        
        std::string result;
        if (current->is_string()) {
            result = current->get<std::string>();
        } else if (current->is_number_integer()) {
            result = std::to_string(current->get<int64_t>());
        } else if (current->is_number_float()) {
            result = std::to_string(current->get<double>());
        } else if (current->is_boolean()) {
            result = current->get<bool>() ? "true" : "false";
        } else {
            result = current->dump();
        }
        
        *outIsNull = false;
        *outLen = static_cast<int32_t>(result.size());
        auto ret = ArenaAllocatorMalloc(contextPtr, *outLen + 1);
        memcpy_s(ret, *outLen + 1, result.c_str(), *outLen + 1);
        return ret;
        
    } catch (const std::exception&) {
        *outIsNull = true;
        *outLen = 0;
        return nullptr;
    }
}

// Extended JSON_VALUE function with ON EMPTY/ERROR behaviors
// emptyBehavior: 0=NULL, 1=ERROR, 2=DEFAULT
// errorBehavior: 0=NULL, 1=ERROR, 2=DEFAULT
extern "C" DLLEXPORT const char* JsonValueExtended(
    int64_t contextPtr,
    const char *jsonStr, int32_t jsonStrLen, bool jsonStrIsNull,
    const char *pathStr, int32_t pathStrWidth, int32_t pathStrLen, bool pathStrIsNull,
    int32_t emptyBehavior, const char *defaultOnEmpty, int32_t defaultOnEmptyLen, bool defaultOnEmptyIsNull,
    int32_t errorBehavior, const char *defaultOnError, int32_t defaultOnErrorLen, bool defaultOnErrorIsNull,
    bool *outIsNull, int32_t *outLen)
{
    if (outIsNull == nullptr || outLen == nullptr) {
        return nullptr;
    }
    
    if (jsonStrIsNull || pathStrIsNull) {
        // Handle NULL input based on error behavior
        if (errorBehavior == 2 && !defaultOnErrorIsNull) { // DEFAULT
            *outIsNull = false;
            *outLen = defaultOnErrorLen;
            auto ret = ArenaAllocatorMalloc(contextPtr, *outLen + 1);
            memcpy_s(ret, *outLen + 1, defaultOnError, *outLen + 1);
            return ret;
        } else if (errorBehavior == 1) { // ERROR
            SetError(contextPtr, "JSON_VALUE error: NULL input");
            *outIsNull = true;
            *outLen = 0;
            return nullptr;
        } else { // NULL
            *outIsNull = true;
            *outLen = 0;
            return nullptr;
        }
    }
    
    std::string jsonContent(jsonStr, jsonStrLen);
    std::string pathContent(pathStr, pathStrLen);
    
    try {
        // Use cached JSON if available, otherwise parse and cache
        nlohmann::json* jsonData;
        if (THREAD_LOCAL_JSON_CACHE.IsCacheValid(jsonContent)) {
            jsonData = &THREAD_LOCAL_JSON_CACHE.parsedJson;
        } else {
            nlohmann::json newJson;
            try {
                newJson = nlohmann::json::parse(jsonContent);
            } catch (...) {
                std::string fixedJsonContent;
                fixedJsonContent.reserve(jsonContent.size());
                for (size_t j = 0; j < jsonContent.size(); j++) {
                    if (jsonContent[j] == '\\') {
                        if (j + 1 < jsonContent.size()) {
                            char next = jsonContent[j + 1];
                            if (next == '\\' || next == '"') {
                                fixedJsonContent += jsonContent[j];
                            } else {
                                fixedJsonContent += '"';
                            }
                        } else {
                            fixedJsonContent += '"';
                        }
                    } else {
                        fixedJsonContent += jsonContent[j];
                    }
                }
                newJson = nlohmann::json::parse(fixedJsonContent);
                jsonContent = fixedJsonContent;
            }
            THREAD_LOCAL_JSON_CACHE.SetCache(jsonContent, newJson);
            jsonData = &THREAD_LOCAL_JSON_CACHE.parsedJson;
        }
        
        std::vector<std::string> keys = ParseJsonPath(pathContent);
        
        if (keys.empty()) {
            // Invalid path - treat as error
            if (errorBehavior == 2 && !defaultOnErrorIsNull) { // DEFAULT
                *outIsNull = false;
                *outLen = defaultOnErrorLen;
                auto ret = ArenaAllocatorMalloc(contextPtr, *outLen + 1);
                memcpy_s(ret, *outLen + 1, defaultOnError, *outLen + 1);
                return ret;
            } else if (errorBehavior == 1) { // ERROR
                SetError(contextPtr, "JSON_VALUE error: Invalid JSON path");
                *outIsNull = true;
                *outLen = 0;
                return nullptr;
            } else { // NULL
                *outIsNull = true;
                *outLen = 0;
                return nullptr;
            }
        }
        
        nlohmann::json* current = jsonData;
        bool found = true;
        
        for (const auto& key : keys) {
            if (current->is_object()) {
                if (current->contains(key)) {
                    current = &(*current)[key];
                } else {
                    found = false;
                    break;
                }
            } else if (current->is_array()) {
                try {
                    size_t index = std::stoul(key);
                    if (index < current->size()) {
                        current = &(*current)[index];
                    } else {
                        found = false;
                        break;
                    }
                } catch (...) {
                    found = false;
                    break;
                }
            } else {
                found = false;
                break;
            }
        }
        
        if (!found || current->is_null()) {
            // Empty result - apply empty behavior
            if (emptyBehavior == 2 && !defaultOnEmptyIsNull) { // DEFAULT
                *outIsNull = false;
                *outLen = defaultOnEmptyLen;
                auto ret = ArenaAllocatorMalloc(contextPtr, *outLen + 1);
                memcpy_s(ret, *outLen + 1, defaultOnEmpty, *outLen + 1);
                return ret;
            } else if (emptyBehavior == 1) { // ERROR
                SetError(contextPtr, "JSON_VALUE error: Empty result");
                *outIsNull = true;
                *outLen = 0;
                return nullptr;
            } else { // NULL
                *outIsNull = true;
                *outLen = 0;
                return nullptr;
            }
        }
        
        std::string result;
        if (current->is_string()) {
            result = current->get<std::string>();
        } else if (current->is_number_integer()) {
            result = std::to_string(current->get<int64_t>());
        } else if (current->is_number_float()) {
            result = std::to_string(current->get<double>());
        } else if (current->is_boolean()) {
            result = current->get<bool>() ? "true" : "false";
        } else {
            result = current->dump();
        }
        
        *outIsNull = false;
        *outLen = static_cast<int32_t>(result.size());
        auto ret = ArenaAllocatorMalloc(contextPtr, *outLen + 1);
        memcpy_s(ret, *outLen + 1, result.c_str(), *outLen + 1);
        return ret;
        
    } catch (const std::exception& e) {
        // Error during processing - apply error behavior
        if (errorBehavior == 2 && !defaultOnErrorIsNull) { // DEFAULT
            *outIsNull = false;
            *outLen = defaultOnErrorLen;
            auto ret = ArenaAllocatorMalloc(contextPtr, *outLen + 1);
            memcpy_s(ret, *outLen + 1, defaultOnError, *outLen + 1);
            return ret;
        } else if (errorBehavior == 1) { // ERROR
            std::string errMsg = std::string("JSON_VALUE error: ") + e.what();
            SetError(contextPtr, errMsg.c_str());
            *outIsNull = true;
            *outLen = 0;
            return nullptr;
        } else { // NULL
            *outIsNull = true;
            *outLen = 0;
            return nullptr;
        }
    }
}

extern "C" DLLEXPORT const char *ConcatCharStr(int64_t contextPtr, const char *ap, int32_t aWidth, int32_t apLen,
    const char *bp, int32_t bpLen, bool isNull, int32_t *outLen)
{
    if (isNull) {
        return nullptr;
    }
    bool hasErr = false;
    const char *ret = StringUtil::ConcatCharDiffWidths(contextPtr, ap, aWidth, apLen, bp, bpLen, &hasErr, outLen);
    if (hasErr) {
        SetError(contextPtr, CONCAT_ERR_MSG);
        return nullptr;
    }
    return ret;
}

extern "C" DLLEXPORT const char *ConcatStrChar(int64_t contextPtr, const char *ap, int32_t apLen, const char *bp,
    int32_t bWidth, int32_t bpLen, bool isNull, int32_t *outLen)
{
    if (isNull) {
        return nullptr;
    }

    bool hasErr = false;
    const char *ret = StringUtil::ConcatStrDiffWidths(contextPtr, ap, apLen, bp, bpLen, &hasErr, outLen);
    if (hasErr) {
        SetError(contextPtr, CONCAT_ERR_MSG);
        return nullptr;
    }
    return ret;
}

extern "C" DLLEXPORT const char *ConcatWsWithoutStr(int64_t contextPtr, const char *separator, int32_t separatorLen,
    bool separatorIsNull, bool *retIsNull, int32_t *outLen)
{
    if (separatorIsNull) {
        *retIsNull = true;
        *outLen = 0;
        return nullptr;
    }
    *retIsNull = false;
    *outLen = 0;
    return reinterpret_cast<const char *>(EMPTY);
}

extern "C" DLLEXPORT const char *ConcatWsWith1Str(int64_t contextPtr, const char *separator, int32_t separatorLen,
    bool separatorIsNull, const char *s1, int32_t s1Len, bool s1IsNull, bool *retIsNull, int32_t *outLen)
{
    if (separatorIsNull) {
        *retIsNull = true;
        *outLen = 0;
        return nullptr;
    }
    *retIsNull = false;
    if (s1IsNull) {
        *outLen = 0;
        return reinterpret_cast<const char *>(EMPTY);
    }
    *outLen = s1Len;
    return s1;
}

extern "C" DLLEXPORT const char *ConcatWsStr(int64_t contextPtr, const char *separator, int32_t separatorLen,
    bool separatorIsNull, const char *s1, int32_t s1Len, bool s1IsNull, const char *s2, int32_t s2Len, bool s2IsNull,
    bool *retIsNull, int32_t *outLen)
{
    if (separatorIsNull) {
        *retIsNull = true;
        *outLen = 0;
        return nullptr;
    }
    *retIsNull = false;

    if (s1IsNull && s2IsNull) {
        *outLen = 0;
        return reinterpret_cast<const char *>(EMPTY);
    }
    if (s1IsNull) {
        *outLen = s2Len;
        return s2;
    }
    if (s2IsNull) {
        *outLen = s1Len;
        return s1;
    }

    bool hasErr = false;
    const char *ret = StringUtil::ConcatWsStrDiffWidths(
        contextPtr, separator, separatorLen, s1, s1Len, s2, s2Len, &hasErr, outLen);
    if (hasErr) {
        SetError(contextPtr, CONCAT_WS_ERR_MSG);
        *retIsNull = true;
        *outLen = 0;
        return nullptr;
    }
    return ret;
}

extern "C" DLLEXPORT const char *ConcatWs3Str(int64_t contextPtr, const char *separator, int32_t separatorLen,
    bool separatorIsNull, const char *s1, int32_t s1Len, bool s1IsNull, const char *s2, int32_t s2Len, bool s2IsNull,
    const char *s3, int32_t s3Len, bool s3IsNull, bool *retIsNull, int32_t *outLen)
{
    if (separatorIsNull) {
        *retIsNull = true;
        *outLen = 0;
        return nullptr;
    }
    *retIsNull = false;

    if (s1IsNull && s2IsNull && s3IsNull) {
        *outLen = 0;
        return reinterpret_cast<const char *>(EMPTY);
    }

    bool hasErr = false;
    const char *tmp = nullptr;
    int32_t tmpLen = 0;
    bool tmpSet = false;

    if (!s1IsNull &&
        !StringUtil::ConcatWsAppend(contextPtr, separator, separatorLen, tmp, tmpLen, tmpSet, s1, s1Len, &hasErr, outLen)) {
        SetError(contextPtr, CONCAT_WS_ERR_MSG);
        *retIsNull = true;
        *outLen = 0;
        return nullptr;
    }
    if (!s2IsNull &&
        !StringUtil::ConcatWsAppend(contextPtr, separator, separatorLen, tmp, tmpLen, tmpSet, s2, s2Len, &hasErr, outLen)) {
        SetError(contextPtr, CONCAT_WS_ERR_MSG);
        *retIsNull = true;
        *outLen = 0;
        return nullptr;
    }
    if (!s3IsNull &&
        !StringUtil::ConcatWsAppend(contextPtr, separator, separatorLen, tmp, tmpLen, tmpSet, s3, s3Len, &hasErr, outLen)) {
        SetError(contextPtr, CONCAT_WS_ERR_MSG);
        *retIsNull = true;
        *outLen = 0;
        return nullptr;
    }
    *outLen = tmpLen;
    return tmp;
}

extern "C" DLLEXPORT const char *ConcatWs4Str(int64_t contextPtr, const char *separator, int32_t separatorLen,
    bool separatorIsNull, const char *s1, int32_t s1Len, bool s1IsNull, const char *s2, int32_t s2Len, bool s2IsNull,
    const char *s3, int32_t s3Len, bool s3IsNull, const char *s4, int32_t s4Len, bool s4IsNull, bool *retIsNull,
    int32_t *outLen)
{
    if (separatorIsNull) {
        *retIsNull = true;
        *outLen = 0;
        return nullptr;
    }
    *retIsNull = false;

    bool hasErr = false;
    if (s1IsNull && s2IsNull && s3IsNull && s4IsNull) {
        *outLen = 0;
        return reinterpret_cast<const char *>(EMPTY);
    }

    const char *tmp = nullptr;
    int32_t tmpLen = 0;
    bool tmpSet = false;

    if (!s1IsNull &&
        !StringUtil::ConcatWsAppend(contextPtr, separator, separatorLen, tmp, tmpLen, tmpSet, s1, s1Len, &hasErr, outLen)) {
        SetError(contextPtr, CONCAT_WS_ERR_MSG);
        *retIsNull = true;
        *outLen = 0;
        return nullptr;
    }
    if (!s2IsNull &&
        !StringUtil::ConcatWsAppend(contextPtr, separator, separatorLen, tmp, tmpLen, tmpSet, s2, s2Len, &hasErr, outLen)) {
        SetError(contextPtr, CONCAT_WS_ERR_MSG);
        *retIsNull = true;
        *outLen = 0;
        return nullptr;
    }
    if (!s3IsNull &&
        !StringUtil::ConcatWsAppend(contextPtr, separator, separatorLen, tmp, tmpLen, tmpSet, s3, s3Len, &hasErr, outLen)) {
        SetError(contextPtr, CONCAT_WS_ERR_MSG);
        *retIsNull = true;
        *outLen = 0;
        return nullptr;
    }
    if (!s4IsNull &&
        !StringUtil::ConcatWsAppend(contextPtr, separator, separatorLen, tmp, tmpLen, tmpSet, s4, s4Len, &hasErr, outLen)) {
        SetError(contextPtr, CONCAT_WS_ERR_MSG);
        *retIsNull = true;
        *outLen = 0;
        return nullptr;
    }
    *outLen = tmpLen;
    return tmp;
}

extern "C" DLLEXPORT const char *ConcatWs5Str(int64_t contextPtr, const char *separator, int32_t separatorLen,
    bool separatorIsNull, const char *s1, int32_t s1Len, bool s1IsNull, const char *s2, int32_t s2Len, bool s2IsNull,
    const char *s3, int32_t s3Len, bool s3IsNull, const char *s4, int32_t s4Len, bool s4IsNull, const char *s5,
    int32_t s5Len, bool s5IsNull, bool *retIsNull, int32_t *outLen)
{
    if (separatorIsNull) {
        *retIsNull = true;
        *outLen = 0;
        return nullptr;
    }
    bool hasErr = false;
    *retIsNull = false;

    if (s1IsNull && s2IsNull && s3IsNull && s4IsNull && s5IsNull) {
        *outLen = 0;
        return reinterpret_cast<const char *>(EMPTY);
    }

    const char *tmp = nullptr;
    int32_t tmpLen = 0;
    bool tmpSet = false;

    if (!s1IsNull &&
        !StringUtil::ConcatWsAppend(contextPtr, separator, separatorLen, tmp, tmpLen, tmpSet, s1, s1Len, &hasErr, outLen)) {
        SetError(contextPtr, CONCAT_WS_ERR_MSG);
        *retIsNull = true;
        *outLen = 0;
        return nullptr;
    }
    if (!s2IsNull &&
        !StringUtil::ConcatWsAppend(contextPtr, separator, separatorLen, tmp, tmpLen, tmpSet, s2, s2Len, &hasErr, outLen)) {
        SetError(contextPtr, CONCAT_WS_ERR_MSG);
        *retIsNull = true;
        *outLen = 0;
        return nullptr;
    }
    if (!s3IsNull &&
        !StringUtil::ConcatWsAppend(contextPtr, separator, separatorLen, tmp, tmpLen, tmpSet, s3, s3Len, &hasErr, outLen)) {
        SetError(contextPtr, CONCAT_WS_ERR_MSG);
        *retIsNull = true;
        *outLen = 0;
        return nullptr;
    }
    if (!s4IsNull &&
        !StringUtil::ConcatWsAppend(contextPtr, separator, separatorLen, tmp, tmpLen, tmpSet, s4, s4Len, &hasErr, outLen)) {
        SetError(contextPtr, CONCAT_WS_ERR_MSG);
        *retIsNull = true;
        *outLen = 0;
        return nullptr;
    }
    if (!s5IsNull &&
        !StringUtil::ConcatWsAppend(contextPtr, separator, separatorLen, tmp, tmpLen, tmpSet, s5, s5Len, &hasErr, outLen)) {
        SetError(contextPtr, CONCAT_WS_ERR_MSG);
        *retIsNull = true;
        *outLen = 0;
        return nullptr;
    }
    *outLen = tmpLen;
    return tmp;
}

extern "C" DLLEXPORT int32_t CastStringToDateNotAllowReducePrecison(int64_t contextPtr, const char *str, int32_t strLen,
    bool isNull)
{
    if (isNull) {
        return 0;
    }
    // Date is in the format 1996-02-28
    // Doesn't account for leap seconds or daylight savings
    // Should be ok just for dates
    int64_t result = 0;
    std::string s(str, strLen);
    StringUtil::TrimString(s);
    if (!regex_match(s, g_dateRegex)) {
        SetError(contextPtr, "Only support cast date\'YYYY-MM-DD\' to integer");
        return -1;
    }
    if (Date32::StringToDate32(str, strLen, result) != Status::CONVERT_SUCCESS) {
        SetError(contextPtr, "Value cannot be cast to date: " + std::string(str, strLen));
        return -1;
    }
    return static_cast<int32_t >(result);
}

extern "C" DLLEXPORT int32_t CastStringToDateAllowReducePrecison(int64_t contextPtr, const char *str, int32_t strLen,
    bool isNull)
{
    if (isNull) {
        return 0;
    }
    // Date is in the format 1996-02-28
    // Doesn't account for leap seconds or daylight savings
    // Should be ok just for dates
    int64_t result = 0;
    if (Date32::StringToDate32(str, strLen, result) != Status::CONVERT_SUCCESS) {
        SetError(contextPtr, "Value cannot be cast to date: " + std::string(str, strLen));
        return -1;
    }
    return static_cast<int32_t >(result);
}

extern "C" DLLEXPORT const char *ToUpperStr(int64_t contextPtr, const char *str, int32_t strLen, bool isNull,
    int32_t *outLen)
{
    if (isNull) {
        return nullptr;
    }
    auto ret = ArenaAllocatorMalloc(contextPtr, strLen);
    for (int32_t i = 0; i < strLen; i++) {
        auto currItem = *(str + i);
        if (currItem >= static_cast<int>('a') && currItem <= static_cast<int>('z')) {
            *(ret + i) = static_cast<char>(currItem - STEP);
        } else {
            *(ret + i) = currItem;
        }
    }
    *outLen = strLen;
    return ret;
}

extern "C" DLLEXPORT const char *ToUpperChar(int64_t contextPtr, const char *str, int32_t width, int32_t strLen,
    bool isNull, int32_t *outLen)
{
    if (isNull) {
        return nullptr;
    }
    return ToUpperStr(contextPtr, str, strLen, isNull, outLen);
}

extern "C" DLLEXPORT const char *ToLowerStr(int64_t contextPtr, const char *str, int32_t strLen, bool isNull,
    int32_t *outLen)
{
    if (isNull) {
        return nullptr;
    }
    auto ret = ArenaAllocatorMalloc(contextPtr, strLen);
    for (int32_t i = 0; i < strLen; i++) {
        auto currItem = *(str + i);
        if (currItem >= static_cast<int>('A') && currItem <= static_cast<int>('Z')) {
            *(ret + i) = static_cast<char>(currItem + STEP);
        } else {
            *(ret + i) = currItem;
        }
    }
    *outLen = strLen;
    return ret;
}

extern "C" DLLEXPORT const char *ToLowerChar(int64_t contextPtr, const char *str, int32_t width, int32_t strLen,
    bool isNull, int32_t *outLen)
{
    if (isNull) {
        return nullptr;
    }
    return ToLowerStr(contextPtr, str, strLen, isNull, outLen);
}

extern "C" DLLEXPORT int64_t LengthChar(const char *str, int32_t width, int32_t strLen, bool isNull)
{
    return isNull ? 0 : width;
}

extern "C" DLLEXPORT int32_t LengthCharReturnInt32(const char *str, int32_t width, int32_t strLen, bool isNull)
{
    return isNull ? 0 : width;
}

extern "C" DLLEXPORT int32_t LengthStrReturnInt32(const char *str, int32_t strLen, bool isNull)
{
    return isNull ? 0 : omniruntime::Utf8Util::CountCodePoints(str, strLen);
}

extern "C" DLLEXPORT int64_t LengthStr(const char *str, int32_t strLen, bool isNull)
{
    return isNull ? 0 : omniruntime::Utf8Util::CountCodePoints(str, strLen);
}

extern "C" DLLEXPORT const char *ReplaceStrStrStrWithRepNotReplace(int64_t contextPtr, const char *str, int32_t strLen,
    const char *searchStr, int32_t searchLen, const char *replaceStr, int32_t replaceLen, bool isNull, int32_t *outLen)
{
    if (isNull) {
        return nullptr;
    }

    bool hasErr = false;
    char *ret;
    if (searchLen == 0) {
        *outLen = strLen;
        ret = const_cast<char *>(str);
    } else {
        auto result = StringUtil::ReplaceWithSearchNotEmpty(contextPtr, str, strLen, searchStr, searchLen, replaceStr,
            replaceLen, &hasErr, outLen);
        ret = const_cast<char *>(result);
    }

    if (hasErr) {
        SetError(contextPtr, REPLACE_ERR_MSG);
    }
    return ret;
}

extern "C" DLLEXPORT const char *ReplaceStrStrStrWithRepReplace(int64_t contextPtr, const char *str, int32_t strLen,
    const char *searchStr, int32_t searchLen, const char *replaceStr, int32_t replaceLen, bool isNull, int32_t *outLen)
{
    if (isNull) {
        return nullptr;
    }

    bool hasErr = false;
    char *ret;
    if (searchLen == 0) {
        auto result =
            StringUtil::ReplaceWithSearchEmpty(contextPtr, str, strLen, replaceStr, replaceLen, &hasErr, outLen);
        ret = (const_cast<char *>(result));
    } else {
        auto result = StringUtil::ReplaceWithSearchNotEmpty(contextPtr, str, strLen, searchStr, searchLen, replaceStr,
            replaceLen, &hasErr, outLen);
        ret = const_cast<char *>(result);
    }

    if (hasErr) {
        SetError(contextPtr, REPLACE_ERR_MSG);
    }
    return ret;
}

extern "C" DLLEXPORT const char *ReplaceStrStrWithoutRepNotReplace(int64_t contextPtr, const char *str, int32_t strLen,
    const char *searchStr, int32_t searchLen, bool isNull, int32_t *outLen)
{
    if (isNull) {
        return nullptr;
    }
    return ReplaceStrStrStrWithRepNotReplace(contextPtr, str, strLen, searchStr, searchLen, "", 0, isNull, outLen);
}

extern "C" DLLEXPORT const char *ReplaceStrStrWithoutRepReplace(int64_t contextPtr, const char *str, int32_t strLen,
    const char *searchStr, int32_t searchLen, bool isNull, int32_t *outLen)
{
    if (isNull) {
        return nullptr;
    }
    return ReplaceStrStrStrWithRepReplace(contextPtr, str, strLen, searchStr, searchLen, "", 0, isNull, outLen);
}

// Cast numeric type to std::string
extern "C" DLLEXPORT const char *CastIntToString(int64_t contextPtr, int32_t value, bool isNull, int32_t *outLen)
{
    if (isNull) {
        return nullptr;
    }
    std::string str = std::to_string(value);
    *outLen = static_cast<int32_t>(str.size());
    auto ret = ArenaAllocatorMalloc(contextPtr, *outLen);
    memcpy(ret, str.c_str(), *outLen);

    return ret;
}

extern "C" DLLEXPORT const char *CastInt16ToString(int64_t contextPtr, int16_t value, bool isNull, int32_t *outLen)
{
    return CastIntToString(contextPtr, static_cast<int32_t>(value), isNull, outLen);
}

extern "C" DLLEXPORT const char *CastInt8ToString(int64_t contextPtr, int8_t value, bool isNull, int32_t *outLen)
{
    return CastIntToString(contextPtr, static_cast<int32_t>(value), isNull, outLen);
}

extern "C" DLLEXPORT const char *CastLongToString(int64_t contextPtr, int64_t value, bool isNull, int32_t *outLen)
{
    if (isNull) {
        return nullptr;
    }
    if (value == std::numeric_limits<int64_t>::min()) {
        *outLen = INT64_MAX_LEN;
        return INT64_MIN_STR;
    }
    const uint8_t *continualBuf = nullptr;
    auto buf = ArenaAllocatorContinualMem(contextPtr, continualBuf, INT64_MAX_LEN);
    *outLen = StringUtil::toString(value, buf);
    ArenaAllocatorRollBackContinualMem(contextPtr);
    return buf;
}

extern "C" DLLEXPORT const char *CastDoubleToString(int64_t contextPtr, double value, bool isNull, int32_t *outLen)
{
    if (isNull) {
        return nullptr;
    }
    auto ret = ArenaAllocatorMalloc(contextPtr, MAX_DATA_LENGTH);
    *outLen = static_cast<int32_t >(DoubleToString::DoubleToStringConverter(value, ret));
    return ret;
}

extern "C" DLLEXPORT const char *CastDecimal64ToString(int64_t contextPtr, int64_t x, int32_t precision, int32_t scale,
    bool isNull, int32_t *outLen)
{
    if (isNull) {
        return nullptr;
    }
    std::string str = Decimal64(x).SetScale(scale).ToString();
    *outLen = static_cast<int32_t>(str.size());
    auto ret = ArenaAllocatorMalloc(contextPtr, *outLen);
    memcpy(ret, str.c_str(), *outLen);
    return ret;
}

extern "C" DLLEXPORT const char *CastDecimal128ToString(int64_t contextPtr, int64_t high, uint64_t low,
    int32_t precision, int32_t scale, bool isNull, int32_t *outLen)
{
    if (isNull) {
        return nullptr;
    }
    std::string stringDecimal = Decimal128Wrapper(high, low).SetScale(scale).ToString();
    *outLen = static_cast<int32_t>(stringDecimal.length());
    auto ret = ArenaAllocatorMalloc(contextPtr, *outLen);
    memcpy(ret, stringDecimal.c_str(), *outLen);
    return ret;
}

extern "C" DLLEXPORT const char *CastStrWithDiffWidths(int64_t contextPtr, const char *srcStr, int32_t srcLen,
    int32_t srcWidth, bool isNull, int32_t dstWidth, int32_t *outLen)
{
    if (isNull) {
        return nullptr;
    }
    bool hasErr = false;
    const char *ret = StringUtil::CastStrStr(&hasErr, srcStr, srcWidth, srcLen, outLen, dstWidth);
    if (hasErr) {
        std::ostringstream errorMessage;
        errorMessage << "cast varchar[" << srcWidth << "] to varchar[" << dstWidth << "] failed.";
        SetError(contextPtr, errorMessage.str());
    }
    return ret;
}

// Cast std::string to numeric type
extern "C" DLLEXPORT int16_t CastStringToShort(int64_t contextPtr, const char *str, int32_t strLen, bool isNull)
{
    if (isNull) {
        return 0;
    }
    int16_t result;
    Status status = ConvertStringToInteger<int16_t, false>(result, str, strLen);
    if (status == Status::IS_NOT_A_NUMBER) {
        std::string s(str, strLen);
        std::ostringstream errorMessage;
        errorMessage << "Cannot cast '" << s << "' to INTEGER. Value is not a number.";
        SetError(contextPtr, errorMessage.str());
        return 0;
    }

    if (status == Status::CONVERT_OVERFLOW) {
        std::string s(str, strLen);
        std::ostringstream errorMessage;
        errorMessage << "Cannot cast '" << s << "' to INTEGER. Value too large or too small.";
        SetError(contextPtr, errorMessage.str());
        return 0;
    }
    return result;
}

extern "C" DLLEXPORT const char *CastStringToBinary(int64_t contextPtr, const char *str, int32_t strLen, bool isNull,
    int32_t *outLen)
{
    *outLen = strLen;
    auto ret = ArenaAllocatorMalloc(contextPtr, *outLen);
    memcpy(ret, str, *outLen);
    return ret;
}

extern "C" DLLEXPORT int8_t CastStringToByte(int64_t contextPtr, const char *str, int32_t strLen, bool isNull)
{
    if (isNull) {
        return 0;
    }
    int8_t result = 0;
    Status status = ConvertStringToInteger<int8_t, false>(result, str, strLen);
    if (status == Status::IS_NOT_A_NUMBER) {
        std::string s(str, strLen);
        std::ostringstream errorMessage;
        errorMessage << "Cannot cast '" << s << "' to BYTE. Value is not a number.";
        SetError(contextPtr, errorMessage.str());
        return 0;
    }

    if (status == Status::CONVERT_OVERFLOW) {
        std::string s(str, strLen);
        std::ostringstream errorMessage;
        errorMessage << "Cannot cast '" << s << "' to BYTE. Value too large or too small.";
        SetError(contextPtr, errorMessage.str());
        return 0;
    }

    return result;
}

extern "C" DLLEXPORT int32_t CastStringToInt(int64_t contextPtr, const char *str, int32_t strLen, bool isNull)
{
    if (isNull) {
        return 0;
    }
    int32_t result;
    Status status = ConvertStringToInteger<int32_t, false>(result, str, strLen);
    if (status == Status::IS_NOT_A_NUMBER) {
        std::string s(str, strLen);
        std::ostringstream errorMessage;
        errorMessage << "Cannot cast '" << s << "' to INTEGER. Value is not a number.";
        SetError(contextPtr, errorMessage.str());
        return 0;
    }

    if (status == Status::CONVERT_OVERFLOW) {
        std::string s(str, strLen);
        std::ostringstream errorMessage;
        errorMessage << "Cannot cast '" << s << "' to INTEGER. Value too large or too small.";
        SetError(contextPtr, errorMessage.str());
        return 0;
    }
    return result;
}

extern "C" DLLEXPORT int64_t CastStringToLong(int64_t contextPtr, const char *str, int32_t strLen, bool isNull)
{
    if (isNull) {
        return 0;
    }
    int64_t result;
    Status status = ConvertStringToInteger<int64_t, false>(result, str, strLen);
    if (status == Status::IS_NOT_A_NUMBER) {
        std::string s = std::string(str, strLen);
        std::ostringstream errorMessage;
        errorMessage << "Cannot cast '" << s << "' to BIGINT. Value is not a number.";
        SetError(contextPtr, errorMessage.str());
        return 0;
    }

    if (status == Status::CONVERT_OVERFLOW) {
        std::string s = std::string(str, strLen);
        std::ostringstream errorMessage;
        errorMessage << "Cannot cast '" << s << "' to BIGINT. Value too large or too small.";
        SetError(contextPtr, errorMessage.str());
        return 0;
    }

    return result;
}

extern "C" DLLEXPORT double CastStringToDouble(int64_t contextPtr, const char *str, int32_t strLen, bool isNull)
{
    if (isNull) {
        return 0;
    }

    double result;
    Status status = ConvertStringToDouble(result, str, strLen);
    if (status == Status::IS_NOT_A_NUMBER) {
        std::ostringstream errorMessage;
        errorMessage << "Cannot cast '" << std::string(str, strLen) << "' to DOUBLE. Value is not a number.";
        SetError(contextPtr, errorMessage.str());
        return 0;
    }
    if (status == Status::CONVERT_OVERFLOW) {
        std::ostringstream errorMessage;
        errorMessage << "Cannot cast '" << std::string(str, strLen) << "' to DOUBLE. Value is not a number.";
        SetError(contextPtr, errorMessage.str());
        return 0;
    }
    return result;
}

extern "C" DLLEXPORT int64_t CastStringToDecimal64(int64_t contextPtr, const char *str, int32_t strLen, bool isNull,
    int32_t outPrecision, int32_t outScale)
{
    if (isNull) {
        return 0;
    }
    std::string s = std::string(str, strLen);
    StringUtil::TrimString(s);
    if (!regex_match(s, g_decimalRegex)) {
        std::ostringstream errorMessage;
        errorMessage << "Cannot cast VARCHAR '" << s << "' to DECIMAL(" << outPrecision << ", " << outScale <<
            "). Value is not a number.";
        SetError(contextPtr, errorMessage.str());
        return 0;
    }
    Decimal64 result(s);
    result.ReScale(outScale);
    if (result.IsOverflow(outPrecision) != OpStatus::SUCCESS) {
        std::ostringstream errorMessage;
        errorMessage << "Cannot cast VARCHAR '" << std::string(str, strLen) << "' to DECIMAL(" << outPrecision <<
            ", " << outScale << "). Value too large.";
        SetError(contextPtr, errorMessage.str());
        return 0;
    }
    return result.GetValue();
}

extern "C" DLLEXPORT int64_t CastStringToDecimal64RoundUp(int64_t contextPtr, const char *str, int32_t strLen,
    bool isNull, int32_t outPrecision, int32_t outScale)
{
    if (isNull) {
        return 0;
    }
    std::string s = std::string(str, strLen);
    Decimal64<true> result(s);
    result.ReScale(outScale);
    if (result.IsOverflow(outPrecision) == OpStatus::OP_OVERFLOW) {
        std::ostringstream errorMessage;
        errorMessage << "Cannot cast VARCHAR '" << std::string(str, strLen) << "' to DECIMAL(" << outPrecision <<
            ", " << outScale << "). Value too large.";
        SetError(contextPtr, errorMessage.str());
        return 0;
    }
    if (result.IsOverflow(outPrecision) == OpStatus::FAIL) {
        std::ostringstream errorMessage;
        errorMessage << "Cannot cast VARCHAR '" << s << "' to DECIMAL(" << outPrecision << ", " << outScale <<
            "). Value is not a number.";
        SetError(contextPtr, errorMessage.str());
        return 0;
    }
    return result.GetValue();
}

extern "C" DLLEXPORT void CastStringToDecimal128(int64_t contextPtr, const char *str, int32_t strLen, bool isNull,
    int32_t outPrecision, int32_t outScale, int64_t *outHighPtr, uint64_t *outLowPtr)
{
    if (isNull) {
        return;
    }
    std::string s = std::string(str, strLen);
    StringUtil::TrimString(s);
    if (!regex_match(s, g_decimalRegex)) {
        std::ostringstream errorMessage;
        errorMessage << "Cannot cast VARCHAR '" << s << "' to DECIMAL(" << outPrecision << ", " << outScale <<
            "). Value is not a number.";
        SetError(contextPtr, errorMessage.str());
        return;
    }
    Decimal128Wrapper result(s.c_str());
    result.ReScale(outScale);
    OpStatus status = result.IsOverflow(outPrecision);
    if (status != OpStatus::SUCCESS) {
        SetError(contextPtr, CastErrorMessage(OMNI_VARCHAR, OMNI_DECIMAL128, std::string(str, strLen).c_str(), status,
            outPrecision, outScale));
        return;
    }
    *outHighPtr = result.HighBits();
    *outLowPtr = result.LowBits();
}

extern "C" DLLEXPORT void CastStringToDecimal128RoundUp(int64_t contextPtr, const char *str, int32_t strLen,
    bool isNull, int32_t outPrecision, int32_t outScale, int64_t *outHighPtr, uint64_t *outLowPtr)
{
    if (isNull) {
        return;
    }
    std::string s = std::string(str, strLen);
    StringUtil::TrimString(s);
    if (!regex_match(s, g_decimalRegex)) {
        std::ostringstream errorMessage;
        errorMessage << "Cannot cast VARCHAR '" << s << "' to DECIMAL(" << outPrecision << ", " << outScale <<
            "). Value is not a number.";
        SetError(contextPtr, errorMessage.str());
        return;
    }
    Decimal128Wrapper<true> result(s.c_str());
    result.ReScale(outScale);
    OpStatus status = result.IsOverflow(outPrecision);
    if (status != OpStatus::SUCCESS) {
        SetError(contextPtr, CastErrorMessage(OMNI_VARCHAR, OMNI_DECIMAL128, std::string(str, strLen).c_str(), status,
            outPrecision, outScale));
        return;
    }
    *outHighPtr = result.HighBits();
    *outLowPtr = result.LowBits();
}

extern "C" DLLEXPORT const char *ConcatStrStrRetNull(int64_t contextPtr, bool *isNull, const char *ap, int32_t apLen,
    const char *bp, int32_t bpLen, int32_t *outLen)
{
    return StringUtil::ConcatStrDiffWidths(contextPtr, ap, apLen, bp, bpLen, isNull, outLen);
}

extern "C" DLLEXPORT const char *ConcatCharCharRetNull(int64_t contextPtr, bool *isNull, const char *ap, int32_t aWidth,
    int32_t apLen, const char *bp, int32_t bWidth, int32_t bpLen, int32_t *outLen)
{
    return StringUtil::ConcatCharDiffWidths(contextPtr, ap, aWidth, apLen, bp, bpLen, isNull, outLen);
}

extern "C" DLLEXPORT const char *ConcatCharStrRetNull(int64_t contextPtr, bool *isNull, const char *ap, int32_t aWidth,
    int32_t apLen, const char *bp, int32_t bpLen, int32_t *outLen)
{
    return StringUtil::ConcatCharDiffWidths(contextPtr, ap, aWidth, apLen, bp, bpLen, isNull, outLen);
}

extern "C" DLLEXPORT const char *ConcatStrCharRetNull(int64_t contextPtr, bool *isNull, const char *ap, int32_t apLen,
    const char *bp, int32_t bWidth, int32_t bpLen, int32_t *outLen)
{
    return StringUtil::ConcatStrDiffWidths(contextPtr, ap, apLen, bp, bpLen, isNull, outLen);
}

extern "C" DLLEXPORT int32_t CastStringToDateRetNullNotAllowReducePrecison(bool *isNull, const char *str,
    int32_t strLen)
{
    // Date is in the format 1996-02-28
    // Doesn't account for leap seconds or daylight savings
    // Should be ok just for dates
    int64_t result = 0;
    std::string s(str, strLen);
    StringUtil::TrimString(s);
    if (!regex_match(s, g_dateRegex)) {
        *isNull = true;
        return -1;
    }
    if (Date32::StringToDate32(str, strLen, result) != Status::CONVERT_SUCCESS) {
        *isNull = true;
        return -1;
    }
    return static_cast<int32_t >(result);
}

extern "C" DLLEXPORT int32_t CastStringToDateRetNullAllowReducePrecison(bool *isNull, const char *str, int32_t strLen)
{
    // Date is in the format 1996-02-28
    // Doesn't account for leap seconds or daylight savings
    // Should be ok just for dates
    int64_t result = 0;
    if (Date32::StringToDate32(str, strLen, result) != Status::CONVERT_SUCCESS) {
        *isNull = true;
        return -1;
    }
    return static_cast<int32_t >(result);
}

extern "C" DLLEXPORT const char *CastIntToStringRetNull(int64_t contextPtr, bool *isNull, int32_t value,
    int32_t *outLen)
{
    std::string str = std::to_string(value);
    *outLen = static_cast<int32_t>(str.size());
    auto ret = ArenaAllocatorMalloc(contextPtr, *outLen);
    memcpy(ret, str.c_str(), *outLen);

    return ret;
}

extern "C" DLLEXPORT const char *CastInt16ToStringRetNull(int64_t contextPtr, bool *isNull, int16_t value,
    int32_t *outLen)
{
    return CastIntToStringRetNull(contextPtr, isNull, static_cast<int32_t>(value), outLen);
}

extern "C" DLLEXPORT const char *CastInt8ToStringRetNull(int64_t contextPtr, bool *isNull, int8_t value,
    int32_t *outLen)
{
    return CastIntToStringRetNull(contextPtr, isNull, static_cast<int32_t>(value), outLen);
}

extern "C" DLLEXPORT const char *CastLongToStringRetNull(int64_t contextPtr, bool *isNull, int64_t value,
    int32_t *outLen)
{
    if (value == std::numeric_limits<int64_t>::min()) {
        *outLen = INT64_MAX_LEN;
        return INT64_MIN_STR;
    }
    const uint8_t *continualBuf = nullptr;
    auto buf = ArenaAllocatorContinualMem(contextPtr, continualBuf, INT64_MAX_LEN);
    *outLen = StringUtil::toString(value, buf);
    ArenaAllocatorRollBackContinualMem(contextPtr);
    return buf;
}

extern "C" DLLEXPORT const char *CastDoubleToStringRetNull(int64_t contextPtr, bool *isNull, double value,
    int32_t *outLen)
{
    auto ret = ArenaAllocatorMalloc(contextPtr, MAX_DATA_LENGTH);
    *outLen = static_cast<int32_t >(DoubleToString::DoubleToStringConverter(value, ret));
    return ret;
}

extern "C" DLLEXPORT const char *CastDecimal64ToStringRetNull(int64_t contextPtr, bool *isNull, int64_t x,
    int32_t precision, int32_t scale, int32_t *outLen)
{
    std::string str = Decimal64(x).SetScale(scale).ToString();
    *outLen = static_cast<int32_t>(str.size());
    auto ret = ArenaAllocatorMalloc(contextPtr, *outLen);
    memcpy(ret, str.c_str(), *outLen);
    return ret;
}

extern "C" DLLEXPORT const char *CastDecimal128ToStringRetNull(int64_t contextPtr, bool *isNull, int64_t high,
    uint64_t low, int32_t precision, int32_t scale, int32_t *outLen)
{
    Decimal128Wrapper inputDecimal(high, low);
    std::string stringDecimal = inputDecimal.SetScale(scale).ToString();
    *outLen = static_cast<int32_t>(stringDecimal.length());
    auto ret = ArenaAllocatorMalloc(contextPtr, *outLen); 
    memcpy(ret, stringDecimal.c_str(), *outLen);
    return ret;
}

extern "C" DLLEXPORT int8_t CastStringToByteRetNull(bool *isNull, const char *str, int32_t strLen)
{
    int8_t result = -6;
    Status status = ConvertStringToInteger<int8_t>(result, str, strLen);
    *isNull = status != Status::CONVERT_SUCCESS;
    return result;
}

extern "C" DLLEXPORT int16_t CastStringToShortRetNull(bool *isNull, const char *str, int32_t strLen)
{
    int16_t result = 0;
    Status status = ConvertStringToInteger<int16_t>(result, str, strLen);
    *isNull = status != Status::CONVERT_SUCCESS;
    return result;
}

extern "C" DLLEXPORT int32_t CastStringToIntRetNull(bool *isNull, const char *str, int32_t strLen)
{
    int32_t result = 0;
    Status status = ConvertStringToInteger<int32_t>(result, str, strLen);
    *isNull = status != Status::CONVERT_SUCCESS;
    return result;
}

extern "C" DLLEXPORT int64_t CastStringToLongRetNull(bool *isNull, const char *str, int32_t strLen)
{
    int64_t result = 0;
    Status status = ConvertStringToInteger<int64_t>(result, str, strLen);
    *isNull = status != Status::CONVERT_SUCCESS;
    return result;
}

extern "C" DLLEXPORT double CastStringToDoubleRetNull(bool *isNull, const char *str, int32_t strLen)
{
    double result;
    Status status = ConvertStringToDouble(result, str, strLen);
    if (status != Status::CONVERT_SUCCESS) {
        *isNull = true;
        return 0;
    }
    return result;
}

extern "C" DLLEXPORT int64_t CastStringToDecimal64RetNull(bool *isNull, const char *str, int32_t strLen,
    int32_t outPrecision, int32_t outScale)
{
    std::string s = std::string(str, strLen);
    StringUtil::TrimString(s);
    if (!regex_match(s, g_decimalRegex)) {
        *isNull = true;
        return 0;
    }
    Decimal64 result(std::string(str, strLen));
    result.ReScale(outScale);
    if (result.IsOverflow(outPrecision) != OpStatus::SUCCESS) {
        *isNull = true;
        return 0;
    }
    return result.GetValue();
}

extern "C" DLLEXPORT int64_t CastStringToDecimal64RoundUpRetNull(bool *isNull, const char *str, int32_t strLen,
    int32_t outPrecision, int32_t outScale)
{
    std::string s = std::string(str, strLen);
    Decimal64<true> result(std::string(str, strLen));
    result.ReScale(outScale);
    if (result.IsOverflow(outPrecision) != OpStatus::SUCCESS) {
        *isNull = true;
        return 0;
    }
    return result.GetValue();
}

extern "C" DLLEXPORT void CastStringToDecimal128RetNull(bool *isNull, const char *str, int32_t strLen,
    int32_t outPrecision, int32_t outScale, int64_t *outHighPtr, uint64_t *outLowPtr)
{
    std::string s = std::string(str, strLen);
    StringUtil::TrimString(s);
    if (!regex_match(s, g_decimalRegex)) {
        *isNull = true;
        return;
    }
    Decimal128Wrapper result(s.c_str());
    result.ReScale(outScale);
    if (result.IsOverflow(outPrecision) != OpStatus::SUCCESS) {
        *isNull = true;
        return;
    }
    *outHighPtr = result.HighBits();
    *outLowPtr = result.LowBits();
}

extern "C" DLLEXPORT void CastStringToDecimal128RoundUpRetNull(bool *isNull, const char *str, int32_t strLen,
    int32_t outPrecision, int32_t outScale, int64_t *outHighPtr, uint64_t *outLowPtr)
{
    std::string s = std::string(str, strLen);
    StringUtil::TrimString(s);
    if (!regex_match(s, g_decimalRegex)) {
        *isNull = true;
        return;
    }
    Decimal128Wrapper<true> result(s.c_str());
    result.ReScale(outScale);
    if (result.IsOverflow(outPrecision) != OpStatus::SUCCESS) {
        *isNull = true;
        return;
    }
    *outHighPtr = result.HighBits();
    *outLowPtr = result.LowBits();
}

extern "C" DLLEXPORT const char *CastStrWithDiffWidthsRetNull(int64_t contextPtr, bool *isNull, const char *srcStr,
    int32_t srcLen, int32_t srcWidth, int32_t dstWidth, int32_t *outLen)
{
    return StringUtil::CastStrStr(isNull, srcStr, srcWidth, srcLen, outLen, dstWidth);
}

extern "C" DLLEXPORT int32_t InStr(const char *srcStr, int32_t srcLen, const char *subStr, int32_t subLen, bool isNull)
{
    // currently return 0 if not found that means 1-based
    if (isNull || subLen > srcLen) {
        return 0;
    }
    if (subLen == 0) {
        return 1;
    }

    int32_t tailPos = srcLen - subLen;
    int32_t cmpLen = subLen - 1;
    for (int32_t pos = 0; pos <= tailPos; ++pos) {
        if (srcStr[pos] == subStr[0] && memcmp(srcStr + pos + 1, subStr + 1, cmpLen) == 0) {
            auto result = omniruntime::Utf8Util::CountCodePoints(srcStr, pos);
            return (result + 1);
        }
    }
    return 0;
}

extern "C" DLLEXPORT bool StartsWithStr(const char *srcStr, int32_t srcLen, const char *matchStr, int32_t matchLen,
    bool isNull)
{
    if (isNull || matchLen > srcLen) {
        return false;
    }
    if (matchLen == 0) {
        return true;
    }
    return memcmp(srcStr, matchStr, matchLen) == 0;
}

extern "C" DLLEXPORT bool EndsWithStr(const char *srcStr, int32_t srcLen, const char *matchStr, int32_t matchLen,
    bool isNull)
{
    if (isNull || matchLen > srcLen) {
        return false;
    }
    if (matchLen == 0) {
        return true;
    }
    return memcmp(srcStr + srcLen - matchLen, matchStr, matchLen) == 0;
}

extern "C" DLLEXPORT bool RLikeStr(const char *str, int32_t strLen, const char *regexToMatch, int32_t regexLen,
    bool isNull)
{
    if (isNull) {
        return false;
    }
    std::string s = std::string(str, strLen);
    std::string r = std::string(regexToMatch, regexLen);

    thread_local std::string cachedPattern;
    thread_local std::wregex cachedRegex;
    if (cachedPattern != r) {
        cachedPattern = r;
        cachedRegex = std::wregex(StringUtil::ToWideString(r));
    }

    return std::regex_search(StringUtil::ToWideString(s), cachedRegex);
}

extern "C" DLLEXPORT bool RegexMatch(const char *srcStr, int32_t srcLen, const char *matchStr, int32_t matchLen,
    bool isNull)
{
    if (isNull) {
        return false;
    }
    if (matchLen == 0) {
        return true;
    }
    std::string s = std::string(srcStr, srcLen);
    std::string r = std::string(matchStr, matchLen);

    thread_local std::string cachedPattern;
    thread_local std::unique_ptr<RE2> cachedRegex;
    if (cachedPattern != r) {
        cachedPattern = r;
        cachedRegex = std::make_unique<RE2>(re2::StringPiece(matchStr, matchLen), RE2::Quiet);
    }

    return RE2::PartialMatch(re2::StringPiece(srcStr, srcLen), *cachedRegex.get());
}

extern "C" DLLEXPORT const char *CastDateToStringRetNull(int64_t contextPtr, bool *isNull, int32_t value,
    int32_t *outLen)
{
    Date32 date(value);
    auto ret = ArenaAllocatorMalloc(contextPtr, MAX_DAY_ONLY_LENGTH);
    *outLen = static_cast<int32_t>(date.ToString(ret, MAX_DAY_ONLY_LENGTH));
    return ret;
}

extern "C" DLLEXPORT const char *CastDateToString(int64_t contextPtr, int32_t value, bool isNull, int32_t *outLen)
{
    if (isNull) {
        return nullptr;
    }
    Date32 date(value);
    auto ret = ArenaAllocatorMalloc(contextPtr, MAX_DAY_ONLY_LENGTH);
    *outLen = static_cast<int32_t>(date.ToString(ret, MAX_DAY_ONLY_LENGTH));
    return ret;
}

extern "C" DLLEXPORT char *Md5Str(int64_t contextPtr, const char *str, int32_t len, bool isNull, int32_t *outLen)
{
    if (isNull) {
        return nullptr;
    }
    Md5Function md5(str, len);
    *outLen = 32;
    char *mdString = ArenaAllocatorMalloc(contextPtr, *outLen);
    md5.FinishHex(mdString);
    return mdString;
}

extern "C" DLLEXPORT bool ContainsStr(const char *srcStr, int32_t srcLen, const char *matchStr, int32_t matchLen,
    bool isNull)
{
    if (isNull || matchLen > srcLen) {
        return false;
    }
    if (matchLen == 0) {
        return true;
    }
    return StringUtil::StrContainsStr(srcStr, srcLen, matchStr, matchLen);
}

extern "C" DLLEXPORT const char *GreatestStr(const char *lValue, int32_t lLen, bool lIsNull, const char *rValue,
    int32_t rLen, bool rIsNull, bool *retIsNull, int32_t *outLen)
{
    if (lIsNull && rIsNull) {
        *retIsNull = true;
        *outLen = 0;
        return nullptr;
    }
    if (lIsNull) {
        *outLen = rLen;
        return rValue;
    }
    if (!rIsNull) {
        int32_t cmpRet = memcmp(lValue, rValue, std::min(lLen, rLen));
        if (cmpRet < 0 || (cmpRet == 0 && rLen > lLen)) {
            *outLen = rLen;
            return rValue;
        }
    }
    *outLen = lLen;
    return lValue;
}

extern "C" DLLEXPORT const char *EmptyToNull(const char *str, int32_t len, bool isNull, int32_t *outLen)
{
    if (len == 0 || isNull) {
        *outLen = 0;
        return nullptr;
    }

    *outLen = len;
    return str;
}

extern "C" DLLEXPORT const char *StaticInvokeVarcharTypeWriteSideCheck(int64_t contextPtr, const char *str, int32_t len,
    int32_t limit, bool isNull, int32_t *outLen)
{
    if (isNull) {
        *outLen = 0;
        return nullptr;
    }
    int32_t ssLen = StringUtil::NumChars(str, len);
    if (ssLen <= limit) {
        *outLen = len;
        return str;
    }
    int32_t numTailSpacesToTrim = ssLen - limit;
    int32_t endIdx = len - 1;
    int32_t trimTo = len - numTailSpacesToTrim;
    while (endIdx >= trimTo && str[endIdx] == 0x20) {
        endIdx--;
    }
    int32_t outByteNum = endIdx + 1;
    ssLen = StringUtil::NumChars(str, outByteNum);
    if (ssLen > limit) {
        std::ostringstream errorMessage;
        errorMessage << "Exceeds varchar type length limitation: " << limit;
        SetError(contextPtr, errorMessage.str());
        *outLen = 0;
        return nullptr;
    }

    auto padded = ArenaAllocatorMalloc(contextPtr, outByteNum + 1);
    memcpy(padded, str, outByteNum);
    padded[outByteNum] = '\0';
    *outLen = outByteNum;
    return padded;
}

extern "C" DLLEXPORT const char *StaticInvokeCharTypeWriteSideCheck(int64_t contextPtr, const char *str, int32_t len,
    int32_t limit, bool isNull, int32_t *outLen)
{
    if (isNull) {
        *outLen = 0;
        return nullptr;
    }
    int32_t ssLen = StringUtil::NumChars(str, len);
    if (ssLen == limit) {
        *outLen = len;
        return str;
    }
    if (ssLen < limit) {
        int32_t numTailSpacesToAdd = limit - ssLen;
        *outLen = len + numTailSpacesToAdd;
        auto resStr = ArenaAllocatorMalloc(contextPtr, *outLen + 1);
        memcpy(resStr, str, len);
        memset(resStr + len, ' ', numTailSpacesToAdd);
        resStr[*outLen] = '\0';
        return resStr;
    }
    int32_t numTailSpacesToTrim = ssLen - limit;
    int32_t endIdx = len - 1;
    int32_t trimTo = len - numTailSpacesToTrim;
    while (endIdx >= trimTo && str[endIdx] == 0x20) {
        endIdx--;
    }
    int32_t outByteNum = endIdx + 1;
    ssLen = StringUtil::NumChars(str, outByteNum);
    if (ssLen > limit) {
        std::ostringstream errorMessage;
        errorMessage << "Exceeds char type length limitation: " << limit;
        SetError(contextPtr, errorMessage.str());
        *outLen = 0;
        return nullptr;
    }

    auto padded = ArenaAllocatorMalloc(contextPtr, outByteNum + 1);
    memcpy(padded, str, outByteNum);
    padded[outByteNum] = '\0';
    *outLen = outByteNum;
    return padded;
}

extern "C" DLLEXPORT const char *StaticInvokeCharReadPadding(int64_t contextPtr, const char *str, int32_t len,
    int32_t limit, bool isNull, int32_t *outLen)
{
    if (isNull) {
        *outLen = 0;
        return nullptr;
    }
    int32_t ssLen = StringUtil::NumChars(str, len);
    if (ssLen >= limit) {
        *outLen = len;
        return str;
    }
    int32_t diff = limit - ssLen;
    int32_t outByteNum = len + diff + 1;
    auto padded = ArenaAllocatorMalloc(contextPtr, outByteNum);
    if (len > 0) {
        memcpy(padded, str, len);
    }
    memset(padded + len, ' ', diff);
    padded[outByteNum - 1] = '\0';
    *outLen = outByteNum - 1;
    return padded;
}

extern "C" DLLEXPORT const char *SubstringIndex(int64_t contextPtr, const char *str, int32_t strLen, const char *delim,
    int32_t delimLen, int32_t count, bool isNull, int32_t *outLen)
{
    if (count == 0 || isNull) {
        *outLen = 0;
        return nullptr;
    }

    int64_t index;
    if (count > 0) {
        index = stringImpl::StringPosition<true, true>(std::string_view(str, strLen), std::string_view(delim, delimLen),
            count);
    } else {
        index = stringImpl::StringPosition<true, false>(std::string_view(str, strLen),
            std::string_view(delim, delimLen), -count);
    }

    // If 'delim' is not found or found fewer than 'count' times,
    // return the input string directly.
    if (index == 0) {
        auto result = ArenaAllocatorMalloc(contextPtr, strLen);
        memcpy(result, str, strLen);
        *outLen = strLen;
        return result;
    }

    auto start = 0;
    auto length = strLen;
    const auto delimLength = delimLen;
    if (count > 0) {
        length = index - 1;
    } else {
        start = index + delimLength - 1;
        length -= start;
    }

    auto result = ArenaAllocatorMalloc(contextPtr, length);
    memcpy(result, str + start, length);
    *outLen = length;
    return result;
}

static const re2::RE2 kRegex_FixNameGroup("[(][?]<([^>]*)>", re2::RE2::Quiet);
static std::unordered_map<std::string, std::shared_ptr<re2::RE2>> re2_cache;
static std::shared_mutex re2_mutex;
static constexpr size_t MAX_CACHE_SIZE = 128;

static constexpr std::array<bool, 128> GetMetaCharTable() {
    std::array<bool, 128> arr{};
    const char meta[] = R"(\.+*?^$()[]{}|)";
    for (char c : meta) {
        arr[static_cast<uint8_t>(c)] = true;
    }
    return arr;
}
static constexpr std::array<bool, 128> g_re_meta_chars = GetMetaCharTable();

extern "C" DLLEXPORT const char *Re2SearchAndExtract(int64_t contextPtr, const char *str, int32_t strLen,
    const char *pattern, int32_t patternLen, int32_t idx, bool isNull, int32_t *outLen)
{
    std::string newPattern(pattern, patternLen);
    std::shared_ptr<const re2::RE2> re_sp;
    {
        std::shared_lock<std::shared_mutex> read_lock(re2_mutex);
        auto cache_iter = re2_cache.find(newPattern);
        if (cache_iter != re2_cache.end()) {
            re_sp = cache_iter->second;
        }
    }

    if (!re_sp) {
        std::unique_lock<std::shared_mutex> write_lock(re2_mutex);
        auto cache_iter = re2_cache.find(newPattern);
        if (cache_iter != re2_cache.end()) {
            re_sp = cache_iter->second;
        } else {
            if (re2_cache.size() >= MAX_CACHE_SIZE) {
                re2_cache.clear();
            }
            if (newPattern.empty()) {
                SetError(contextPtr, "Regex compile failed: empty pattern");
                return nullptr;
            }
            auto re_uptr = std::make_shared<re2::RE2>(stringImpl::toStringPiece(newPattern), re2::RE2::Quiet);
            if (!re_uptr->ok()) {
                SetError(contextPtr, ("Regex compile failed: " + newPattern).c_str());
                return nullptr;
            }
            re2_cache.emplace(newPattern, re_uptr);
            re_sp = std::move(re_uptr);
        }
    }

    const re2::RE2& re = *re_sp;
    std::vector<re2::StringPiece> groups(idx + 1);
    auto input = re2::StringPiece(str, strLen);
    if (!re.Match(input, 0, strLen, RE2::UNANCHORED, groups.data(), idx + 1)) {
        *outLen = 0;
        auto result = ArenaAllocatorMalloc(contextPtr, 1);
        return result;
    } else {
        const re2::StringPiece extracted = groups[idx];
        auto result = ArenaAllocatorMalloc(contextPtr, extracted.size());
        memcpy(result, extracted.data(), extracted.size());
        *outLen = extracted.size();
        return result;
    }
}

extern "C" DLLEXPORT const char *RegexpReplace(int64_t contextPtr, const char *stringInput, int32_t stringInputLen,
    const char *pattern, int32_t patternLen, const char *replacement, int32_t replacementLen, int32_t position,
    bool isNull, int32_t *outLen)
{
    *outLen = 0;
    if (isNull || stringInputLen < 0 || patternLen <= 0 || replacementLen < 0 || stringInput == nullptr || pattern == nullptr) {
        return nullptr;
    }
    if (stringInputLen == 0) {
        return reinterpret_cast<const char *>(EMPTY);
    }

    const int32_t start_pos = std::max(position - 1, static_cast<int32_t>(0));
    const int32_t suffix_len = stringInputLen - start_pos;
    std::string newPattern(pattern, patternLen);
    const std::string replacementStr(replacement == nullptr ? "" : std::string(replacement, replacementLen));
    std::string buf;
    buf.reserve(stringInputLen);

    bool is_plain_text = true;
    for (char c : newPattern) {
        auto uc = static_cast<uint8_t>(c);
        if (uc < 128 && g_re_meta_chars[uc]) {
            is_plain_text = false;
            break;
        }
    }

    if (!is_plain_text) {
        re2::RE2::GlobalReplace(&newPattern, kRegex_FixNameGroup, R"((?P<\1>))");
    }

    if (is_plain_text) {
        const std::string_view src_sv(stringInput, stringInputLen);
        const std::string_view pattern_sv(newPattern);
        const size_t pattern_len = pattern_sv.size();
        const size_t repl_len = replacementStr.size();
        const std::string_view suffix_sv = src_sv.substr(start_pos);

        const size_t max_possible_len = start_pos + suffix_sv.size() + (suffix_sv.size() / pattern_len) * std::abs(static_cast<int>(repl_len - pattern_len));
        buf.reserve(max_possible_len);

        if (start_pos > 0) {
            buf.append(stringInput, start_pos);
        }

        size_t last_pos = 0;
        size_t curr_pos = suffix_sv.find(pattern_sv, last_pos);
        while (curr_pos != std::string_view::npos) {
            buf.append(suffix_sv.data() + last_pos, curr_pos - last_pos);
            if (!replacementStr.empty()) {
                buf.append(replacementStr);
            }
            last_pos = curr_pos + pattern_len;
            curr_pos = suffix_sv.find(pattern_sv, last_pos);
        }

        buf.append(suffix_sv.data() + last_pos, suffix_sv.size() - last_pos);
    } else {
        std::shared_ptr<const re2::RE2> re_sp;
        {
            std::shared_lock<std::shared_mutex> read_lock(re2_mutex);
            auto cache_iter = re2_cache.find(newPattern);
            if (cache_iter != re2_cache.end()) {
                re_sp = cache_iter->second;
            }
        }

        if (!re_sp) {
            std::unique_lock<std::shared_mutex> write_lock(re2_mutex);
            auto cache_iter = re2_cache.find(newPattern);
            if (cache_iter != re2_cache.end()) {
                re_sp = cache_iter->second;
            } else {
                if (re2_cache.size() >= MAX_CACHE_SIZE) {
                    re2_cache.clear();
                }
                if (newPattern.empty()) {
                    SetError(contextPtr, "Regex compile failed: empty pattern");
                    return nullptr;
                }
                auto re_uptr = std::make_shared<re2::RE2>(stringImpl::toStringPiece(newPattern), re2::RE2::Quiet);
                if (!re_uptr->ok()) {
                    SetError(contextPtr, ("Regex compile failed: " + newPattern).c_str());
                    return nullptr;
                }
                re2_cache.emplace(newPattern, re_uptr);
                re_sp = std::move(re_uptr);
            }
        }

        const re2::RE2& re = *re_sp;
        const auto& processedReplacement = stringImpl::PrepareRegexpReplaceReplacement(re, replacementStr);
        re2::StringPiece suffix_sp(stringInput + start_pos, suffix_len);
        std::string suffix(suffix_sp);
        re2::RE2::GlobalReplace(&suffix, re, processedReplacement);

        buf.append(stringInput, 0, start_pos);
        buf.append(std::move(suffix));
    }

    const size_t length = buf.size();
    if (length == 0) {
        *outLen = 0;
        return reinterpret_cast<const char *>(EMPTY);
    }

    auto result = ArenaAllocatorMalloc(contextPtr, length);
    if (result == nullptr) {
        *outLen = 0;
        return result;
    }

    memcpy(result, buf.data(), length);
    *outLen = static_cast<int32_t>(length);
    return result;
}

extern "C" DLLEXPORT const char *TrimStr(int64_t contextPtr, const char *trimStr, int32_t trimStrLen,
    const char *srcStr, int32_t srcStrLen, bool isNull, int32_t *outLen)
{
    if (isNull) {
        return nullptr;
    }
    bool hasErr = false;
    const char *ret = StringUtil::Trim(contextPtr, srcStr, srcStrLen, trimStr, trimStrLen, &hasErr, outLen);
    if (hasErr) {
        SetError(contextPtr, " trim failed ");
        return nullptr;
    }
    return ret;
}

extern "C" DLLEXPORT const char *Trim1Str(int64_t contextPtr, const char *srcStr, int32_t srcStrLen, bool isNull,
    int32_t *outLen)
{
    *outLen = 0;
    if (isNull || srcStr == nullptr || srcStrLen <= 0) {
        return nullptr;
    }
    const char trimChar = ' ';
    int32_t start = 0;
    while (start < srcStrLen && srcStr[start] == trimChar) {
        start++;
    }
    int32_t end = srcStrLen - 1;
    while (end >= start && srcStr[end] == trimChar) {
        end--;
    }
    if (start > end) {
        return nullptr;
    }
    *outLen = end - start + 1;
    char *ret = static_cast<char*>(ArenaAllocatorMalloc(contextPtr, *outLen + 1));
    if (ret == nullptr) {
        SetError(contextPtr, "Trim: malloc failed");
        return nullptr;
    }
    memcpy(ret, srcStr + start, *outLen);
    return ret;
}

extern "C" DLLEXPORT const char *LeftTrimStr(int64_t contextPtr, const char *trimStr, int32_t trimStrLen,
    const char *srcStr, int32_t srcStrLen, bool isNull, int32_t *outLen)
{
    if (isNull) {
        return nullptr;
    }
    bool hasErr = false;
    const char *ret = StringUtil::LTrim(contextPtr, srcStr, srcStrLen, trimStr, trimStrLen, &hasErr, outLen);
    if (hasErr) {
        SetError(contextPtr, " ltrim failed ");
        return nullptr;
    }
    return ret;
}

extern "C" DLLEXPORT const char *RightTrimStr(int64_t contextPtr, const char *trimStr, int32_t trimStrLen,
    const char *srcStr, int32_t srcStrLen, bool isNull, int32_t *outLen)
{
    if (isNull) {
        return nullptr;
    }
    bool hasErr = false;
    const char *ret = StringUtil::RTrim(contextPtr, srcStr, srcStrLen, trimStr, trimStrLen, &hasErr, outLen);
    if (hasErr) {
        SetError(contextPtr, " rtrim failed ");
        return nullptr;
    }
    return ret;
}

extern "C" DLLEXPORT int64_t CastStringToTimestamp(int64_t contextPtr, const char *str, int32_t strLen, bool isNull)
{
    if (isNull) {
        return 0;
    }
    std::string_view view(str, strLen);
    auto conversionResult = omniruntime::type::util::fromTimestampWithTimezoneString(view.data(), view.size());
    if (conversionResult.hasError()) {
        std::string s(str, strLen);
        std::ostringstream errorMessage;
        errorMessage << "Cannot cast '" << s << "' to Timestamp.";
        SetError(contextPtr, errorMessage.str());
        return 0;
    }

    auto sessionTimezone = tz::locateZone("Asia/Shanghai");
    return omniruntime::type::util::fromParsedTimestampWithTimeZone(conversionResult.value(), sessionTimezone).toMicros();
}

extern "C" DLLEXPORT int64_t CastStringToTimestampReturnNull(bool *isNull, const char *str, int32_t strLen)
{
    std::string_view view(str, strLen);
    auto conversionResult = omniruntime::type::util::fromTimestampWithTimezoneString(view.data(), view.size());
    if (conversionResult.hasError()) {
        *isNull = true;
        return 0;
    }
    *isNull = false;
    auto sessionTimezone = tz::locateZone("Asia/Shanghai");
    return omniruntime::type::util::fromParsedTimestampWithTimeZone(conversionResult.value(), sessionTimezone).toMicros();
}
}