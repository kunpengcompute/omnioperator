/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
 * KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 *
 * Description: registry  function  implementation
 */

#include "stringfunctions.h"
#include "dtoa.h"
#include "md5.h"
#include "type/string_Impl.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>
#include <ctime>
#include <functional>
#include <nlohmann/json.hpp>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <re2/re2.h>
#include <unordered_map>
#include <utility>
#include <vector>

namespace omniruntime::codegen::function {
using JsonDocument = nlohmann::ordered_json;

// Valid epoch time range constants (same as Flink's DateTimeUtils)
static constexpr int64_t MIN_EPOCH_MILLS = -62167219200000LL; // '0000-01-01 00:00:00.000 UTC+0'
static constexpr int64_t MAX_EPOCH_MILLS = 253402300799999LL; // '9999-12-31 23:59:59.999 UTC+0'
static constexpr int64_t MILLIS_PER_SECOND = 1000LL;

extern "C" DLLEXPORT int64_t CountChar(const char *str, int32_t strLen, const char *target, int32_t targetWidth,
                                       int32_t targetLen, bool isNull)
{
    if (isNull) {
        return 0;
    }
    char chr = target[0];
    int64_t count = std::count(str, str + strLen, chr);
    return count;
}

extern "C" DLLEXPORT const char *SplitIndexRetNull(const char *str, int32_t strLen, bool strIsNull, const char *target,
                                                   int32_t targetWidth, int32_t targetLen, bool targetIsNull,
                                                   int32_t index, bool indexIsNull, bool *outIsNull, int32_t *outLen)
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
 * This function is only called when apLen is equal to bpLen. When apLen and
 * bpLen are different, it will directly return false instead of calling
 * StrEquals.
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
                                                const char *bp, int32_t bWidth, int32_t bpLen, bool isNull,
                                                int32_t *outLen)
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

extern "C" DLLEXPORT const char *RegexpExtractRetNull(int64_t contextPtr, const char *str, int32_t strLen,
                                                      bool strIsNull, const char *regexToMatch, int32_t regexWidth,
                                                      int32_t regexLen, bool regexIsNull, int32_t group,
                                                      bool groupIsNull, bool *outIsNull, int32_t *outLen)
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
        std::wstring_convert<std::codecvt_utf8<wchar_t>> convert;
        std::wstring matchedWstr = match[group].str();
        std::string matchedNstr = convert.to_bytes(matchedWstr);
        *outLen = matchedNstr.size();
        auto ret = ArenaAllocatorMalloc(contextPtr, *outLen + 1);
        memcpy_s(ret, *outLen + 1, str + startIdx, *outLen + 1);
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
struct JsonCache {
    JsonDocument parsedJson;
    std::string lastJsonContent;

    // std::string equality first compares length and then content. Computing a
    // hash for this single-entry cache would add an unconditional full scan.
    bool IsCacheValid(const std::string &jsonContent) const { return lastJsonContent == jsonContent; }

    void SetCache(const std::string &jsonContent, const JsonDocument &json)
    {
        lastJsonContent = jsonContent;
        parsedJson = json;
    }
};

// Thread-local cache for JSON parsing
// This avoids re-parsing the same JSON in the same thread
thread_local JsonCache THREAD_LOCAL_JSON_CACHE;

// RapidJSON cache for target functions
struct RapidJsonCache {
    rapidjson::Document parsedJson;
    std::string lastJsonContent;

    // This is also a single-entry cache; compare directly instead of scanning
    // the JSON once for a hash and again to rule out hash collisions.
    bool IsCacheValid(const std::string &jsonContent) const
    {
        // An empty lastJsonContent also means "never populated": empty input is
        // invalid JSON and must fall through to the parse-failure path.
        return !lastJsonContent.empty() && lastJsonContent == jsonContent;
    }

    void SetCache(const std::string &jsonContent, rapidjson::Document &json)
    {
        std::string(jsonContent).swap(lastJsonContent);
        parsedJson.Swap(json);
    }
};

thread_local RapidJsonCache RAPIDJSON_CACHE;

bool IsSameCacheString(const std::string &cachedValue, const char *value, int32_t valueLen)
{
    if (valueLen < 0 || cachedValue.size() != static_cast<size_t>(valueLen)) {
        return false;
    }
    return cachedValue.empty() || memcmp(cachedValue.data(), value, cachedValue.size()) == 0;
}

struct JsonQueryResultCacheKey {
    std::string jsonContent;
    std::string rawPath;
    int32_t wrapperBehavior = 0;
    int32_t emptyBehavior = 0;
    int32_t errorBehavior = 0;

    bool operator==(const JsonQueryResultCacheKey &other) const
    {
        return wrapperBehavior == other.wrapperBehavior && emptyBehavior == other.emptyBehavior &&
               errorBehavior == other.errorBehavior && jsonContent == other.jsonContent && rawPath == other.rawPath;
    }

    bool Matches(const char *jsonStr, int32_t jsonStrLen, const char *pathStr, int32_t pathStrLen, int32_t wrapper,
                 int32_t empty, int32_t error) const
    {
        return wrapperBehavior == wrapper && emptyBehavior == empty && errorBehavior == error &&
               IsSameCacheString(jsonContent, jsonStr, jsonStrLen) && IsSameCacheString(rawPath, pathStr, pathStrLen);
    }
};

struct JsonQueryResultCacheEntry {
    JsonQueryResultCacheKey key;
    std::string value;
};

struct JsonQueryResultCache {
    static constexpr size_t CACHE_SIZE = 16;
    std::vector<JsonQueryResultCacheEntry> entries;

    bool Get(const char *jsonStr, int32_t jsonStrLen, const char *pathStr, int32_t pathStrLen, int32_t wrapperBehavior,
             int32_t emptyBehavior, int32_t errorBehavior, std::string *value)
    {
        for (size_t i = 0; i < entries.size(); ++i) {
            if (!entries[i].key.Matches(jsonStr, jsonStrLen, pathStr, pathStrLen, wrapperBehavior, emptyBehavior,
                                        errorBehavior)) {
                continue;
            }
            *value = entries[i].value;
            if (i != 0) {
                JsonQueryResultCacheEntry hit = entries[i];
                entries.erase(entries.begin() + i);
                entries.insert(entries.begin(), std::move(hit));
            }
            return true;
        }
        return false;
    }

    void Put(const JsonQueryResultCacheKey &key, const std::string &value)
    {
        for (size_t i = 0; i < entries.size(); ++i) {
            if (!(entries[i].key == key)) {
                continue;
            }
            entries[i].value = value;
            if (i != 0) {
                JsonQueryResultCacheEntry hit = entries[i];
                entries.erase(entries.begin() + i);
                entries.insert(entries.begin(), std::move(hit));
            }
            return;
        }

        entries.insert(entries.begin(), JsonQueryResultCacheEntry{key, value});
        if (entries.size() > CACHE_SIZE) {
            entries.pop_back();
        }
    }
};

thread_local JsonQueryResultCache JSON_QUERY_RESULT_CACHE;

struct JsonValueResultCacheKey {
    std::string jsonContent;
    std::string rawPath;
    int32_t emptyBehavior = 0;
    int32_t errorBehavior = 0;

    bool operator==(const JsonValueResultCacheKey &other) const
    {
        return emptyBehavior == other.emptyBehavior && errorBehavior == other.errorBehavior &&
               jsonContent == other.jsonContent && rawPath == other.rawPath;
    }

    bool Matches(const char *jsonStr, int32_t jsonStrLen, const char *pathStr, int32_t pathStrLen, int32_t empty,
                 int32_t error) const
    {
        return emptyBehavior == empty && errorBehavior == error &&
               IsSameCacheString(jsonContent, jsonStr, jsonStrLen) && IsSameCacheString(rawPath, pathStr, pathStrLen);
    }
};

struct JsonValueResultCacheEntry {
    JsonValueResultCacheKey key;
    bool isNull = false;
    std::string value;
};

struct JsonValueResultCache {
    static constexpr size_t CACHE_SIZE = 16;
    std::vector<JsonValueResultCacheEntry> entries;

    bool Get(const char *jsonStr, int32_t jsonStrLen, const char *pathStr, int32_t pathStrLen, int32_t emptyBehavior,
             int32_t errorBehavior, std::string *value, bool *isNull)
    {
        for (size_t i = 0; i < entries.size(); ++i) {
            if (!entries[i].key.Matches(jsonStr, jsonStrLen, pathStr, pathStrLen, emptyBehavior, errorBehavior)) {
                continue;
            }
            *value = entries[i].value;
            *isNull = entries[i].isNull;
            if (i != 0) {
                JsonValueResultCacheEntry hit = entries[i];
                entries.erase(entries.begin() + i);
                entries.insert(entries.begin(), std::move(hit));
            }
            return true;
        }
        return false;
    }

    void Put(const JsonValueResultCacheKey &key, const std::string &value, bool isNull)
    {
        for (size_t i = 0; i < entries.size(); ++i) {
            if (!(entries[i].key == key)) {
                continue;
            }
            entries[i].value = value;
            entries[i].isNull = isNull;
            if (i != 0) {
                JsonValueResultCacheEntry hit = entries[i];
                entries.erase(entries.begin() + i);
                entries.insert(entries.begin(), std::move(hit));
            }
            return;
        }

        entries.insert(entries.begin(), JsonValueResultCacheEntry{key, isNull, value});
        if (entries.size() > CACHE_SIZE) {
            entries.pop_back();
        }
    }
};

thread_local JsonValueResultCache JSON_VALUE_RESULT_CACHE;

// Keys use the input address rather than a JSON-content hash. Projection input
// strings are immutable for one evaluator batch, so repeated paths over one
// row share a Document without rescanning the JSON payload.
struct RowJsonDocumentKey {
    const char *data = nullptr;
    int32_t length = 0;

    bool operator==(const RowJsonDocumentKey &other) const { return data == other.data && length == other.length; }
};

struct RowJsonDocumentKeyHash {
    size_t operator()(const RowJsonDocumentKey &key) const
    {
        auto address = reinterpret_cast<uintptr_t>(key.data);
        return static_cast<size_t>(address ^ (address >> 33) ^ (static_cast<uint64_t>(key.length) << 1));
    }
};

struct RowJsonDocumentCache {
    static constexpr size_t MAX_DOCUMENTS = 4096;
    static constexpr size_t MAX_INPUT_BYTES = 64 * 1024 * 1024;

    bool active = false;
    int64_t contextPtr = 0;
    size_t cachedInputBytes = 0;
    std::unordered_map<RowJsonDocumentKey, rapidjson::Document, RowJsonDocumentKeyHash> documents;

    void BeginBatch(int64_t newContextPtr)
    {
        documents.clear();
        cachedInputBytes = 0;
        active = true;
        contextPtr = newContextPtr;
    }

    void EndBatch(int64_t endingContextPtr)
    {
        if (active && contextPtr == endingContextPtr) {
            documents.clear();
            cachedInputBytes = 0;
            active = false;
            contextPtr = 0;
        }
    }

    rapidjson::Document *Find(int64_t lookupContextPtr, const char *data, int32_t length)
    {
        if (!active || contextPtr != lookupContextPtr || data == nullptr || length < 0) {
            return nullptr;
        }
        auto it = documents.find(RowJsonDocumentKey{data, length});
        return it == documents.end() ? nullptr : &it->second;
    }

    rapidjson::Document *Store(int64_t storeContextPtr, const char *data, int32_t length, rapidjson::Document &document)
    {
        if (!active || contextPtr != storeContextPtr || data == nullptr || length < 0) {
            return nullptr;
        }
        const RowJsonDocumentKey key{data, length};
        auto it = documents.find(key);
        if (it == documents.end()) {
            const size_t inputBytes = static_cast<size_t>(length);
            if (documents.size() >= MAX_DOCUMENTS || inputBytes > MAX_INPUT_BYTES - cachedInputBytes) {
                return nullptr;
            }
            it = documents.emplace(key, rapidjson::Document()).first;
            cachedInputBytes += inputBytes;
        }
        auto &cached = it->second;
        cached.Swap(document);
        return &cached;
    }

    rapidjson::Document *StoreCopy(int64_t storeContextPtr, const char *data, int32_t length,
                                   const rapidjson::Document &document)
    {
        if (!active || contextPtr != storeContextPtr || data == nullptr || length < 0) {
            return nullptr;
        }
        const RowJsonDocumentKey key{data, length};
        auto it = documents.find(key);
        if (it == documents.end()) {
            const size_t inputBytes = static_cast<size_t>(length);
            if (documents.size() >= MAX_DOCUMENTS || inputBytes > MAX_INPUT_BYTES - cachedInputBytes) {
                return nullptr;
            }
            it = documents.emplace(key, rapidjson::Document()).first;
            cachedInputBytes += inputBytes;
        }
        auto &cached = it->second;
        cached.CopyFrom(document, cached.GetAllocator());
        return &cached;
    }
};

thread_local RowJsonDocumentCache ROW_JSON_DOCUMENT_CACHE;

rapidjson::Document *GetParsedJsonWithCacheRapidJson(int64_t contextPtr, const char *rowJsonStr, int32_t rowJsonStrLen,
                                                     std::string &jsonContent)
{
    if (rapidjson::Document *rowDocument = ROW_JSON_DOCUMENT_CACHE.Find(contextPtr, rowJsonStr, rowJsonStrLen)) {
        return rowDocument;
    }

    if (RAPIDJSON_CACHE.IsCacheValid(jsonContent)) {
        if (rapidjson::Document *rowDocument =
                ROW_JSON_DOCUMENT_CACHE.StoreCopy(contextPtr, rowJsonStr, rowJsonStrLen, RAPIDJSON_CACHE.parsedJson)) {
            return rowDocument;
        }
        return &RAPIDJSON_CACHE.parsedJson;
    }

    rapidjson::Document parsedJson;
    parsedJson.Parse(jsonContent.c_str());
    if (parsedJson.HasParseError()) {
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
        parsedJson.Parse(fixedJsonContent.c_str());
        if (parsedJson.HasParseError()) {
            return nullptr;
        }
        jsonContent = fixedJsonContent;
    }

    if (rapidjson::Document *rowDocument =
            ROW_JSON_DOCUMENT_CACHE.Store(contextPtr, rowJsonStr, rowJsonStrLen, parsedJson)) {
        return rowDocument;
    }

    RAPIDJSON_CACHE.SetCache(jsonContent, parsedJson);
    return &RAPIDJSON_CACHE.parsedJson;
}

rapidjson::Value *ResolveJsonPathTargetRapidJson(rapidjson::Value *jsonData, const std::vector<std::string> &keys)
{
    rapidjson::Value *current = jsonData;
    for (const auto &key : keys) {
        if (current->IsObject()) {
            rapidjson::Value::MemberIterator it = current->FindMember(rapidjson::StringRef(key.c_str(), key.size()));
            if (it == current->MemberEnd()) {
                return nullptr;
            }
            current = &it->value;
            continue;
        }

        if (current->IsArray()) {
            try {
                size_t index = std::stoul(key);
                if (index >= current->Size()) {
                    return nullptr;
                }
                current = &current->GetArray()[index];
                continue;
            } catch (...) {
                return nullptr;
            }
        }

        return nullptr;
    }

    return current;
}

bool TryFormatJsonValueScalarRapidJson(const rapidjson::Value &value, std::string *result)
{
    if (result == nullptr) {
        return false;
    }

    if (value.IsString()) {
        *result = std::string(value.GetString(), value.GetStringLength());
        return true;
    }

    if (value.IsBool()) {
        *result = value.GetBool() ? "true" : "false";
        return true;
    }

    if (value.IsInt()) {
        *result = std::to_string(value.GetInt());
        return true;
    }

    if (value.IsUint()) {
        *result = std::to_string(value.GetUint());
        return true;
    }

    if (value.IsInt64()) {
        *result = std::to_string(value.GetInt64());
        return true;
    }

    if (value.IsUint64()) {
        *result = std::to_string(value.GetUint64());
        return true;
    }

    if (value.IsDouble()) {
        *result = DoubleToString::DoubleToStringConverter(value.GetDouble());
        return true;
    }

    return false;
}

std::string SerializeRapidJsonValue(const rapidjson::Value &value)
{
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    value.Accept(writer);
    return buffer.GetString();
}
} // namespace

// Legacy function for backward compatibility - converts advanced segments to
// simple keys
static std::vector<std::string> ParseJsonPath(const std::string &path)
{
    std::vector<std::string> keys;
    if (path.empty() || path[0] != '$') {
        return keys;
    }

    enum State {
        EXPECT_DOT_OR_BRACKET, // After key, expect . or [
        IN_DOT_NOTATION,       // After ., reading key until . or [
        IN_BRACKET,            // After [, reading content until ]
        IN_QUOTED_KEY          // Inside quotes within bracket
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
                        ++i; // Skip next character
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

namespace {
constexpr size_t CONSTANT_JSON_PATH_CACHE_CAPACITY = 128;

struct ConstantJsonValuePath {
    std::string rawPath;
    std::vector<std::string> keys;
};

// Codegen marks literal JSON paths with a negative CHAR width. Pointer
// identity makes lookup O(1); rawPath verification protects against stale JIT
// literal addresses being reused for a different path.
thread_local std::unordered_map<const char *, ConstantJsonValuePath> CONSTANT_JSON_VALUE_PATHS;

const std::vector<std::string> &GetConstantJsonValuePath(const char *path, int32_t length)
{
    auto it = CONSTANT_JSON_VALUE_PATHS.find(path);
    if (it != CONSTANT_JSON_VALUE_PATHS.end() && it->second.rawPath.size() == static_cast<size_t>(length) &&
        memcmp(it->second.rawPath.data(), path, static_cast<size_t>(length)) == 0) {
        return it->second.keys;
    }

    if (CONSTANT_JSON_VALUE_PATHS.size() >= CONSTANT_JSON_PATH_CACHE_CAPACITY) {
        CONSTANT_JSON_VALUE_PATHS.clear();
    }
    ConstantJsonValuePath parsedPath;
    parsedPath.rawPath.assign(path, static_cast<size_t>(length));
    parsedPath.keys = ParseJsonPath(parsedPath.rawPath);
    auto inserted = CONSTANT_JSON_VALUE_PATHS.insert_or_assign(path, std::move(parsedPath));
    return inserted.first->second.keys;
}
} // namespace

void JsonRowDocumentCacheBeginBatch(int64_t contextPtr) { ROW_JSON_DOCUMENT_CACHE.BeginBatch(contextPtr); }

void JsonRowDocumentCacheEndBatch(int64_t contextPtr) { ROW_JSON_DOCUMENT_CACHE.EndBatch(contextPtr); }

static JsonDocument *GetParsedJsonWithCache(std::string &jsonContent)
{
    if (THREAD_LOCAL_JSON_CACHE.IsCacheValid(jsonContent)) {
        return &THREAD_LOCAL_JSON_CACHE.parsedJson;
    }

    JsonDocument newJson;
    try {
        newJson = JsonDocument::parse(jsonContent);
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
        newJson = JsonDocument::parse(fixedJsonContent);
        jsonContent = fixedJsonContent;
    }

    THREAD_LOCAL_JSON_CACHE.SetCache(jsonContent, newJson);
    return &THREAD_LOCAL_JSON_CACHE.parsedJson;
}

static JsonDocument *ResolveJsonPathTarget(JsonDocument *jsonData, const std::string &pathContent)
{
    std::vector<std::string> keys = ParseJsonPath(pathContent);
    if (keys.empty()) {
        return nullptr;
    }

    JsonDocument *current = jsonData;
    for (const auto &key : keys) {
        if (current->is_object()) {
            if (!current->contains(key)) {
                return nullptr;
            }
            current = &(*current)[key];
            continue;
        }

        if (current->is_array()) {
            try {
                size_t index = std::stoul(key);
                if (index >= current->size()) {
                    return nullptr;
                }
                current = &(*current)[index];
                continue;
            } catch (...) {
                return nullptr;
            }
        }

        return nullptr;
    }

    return current;
}

static bool TryFormatJsonValueScalar(const JsonDocument &value, std::string *result)
{
    if (result == nullptr) {
        return false;
    }

    if (value.is_string()) {
        *result = value.get<std::string>();
        return true;
    }

    if (value.is_boolean()) {
        *result = value.get<bool>() ? "true" : "false";
        return true;
    }

    if (value.is_number_integer()) {
        *result = std::to_string(value.get<JsonDocument::number_integer_t>());
        return true;
    }

    if (value.is_number_unsigned()) {
        *result = std::to_string(value.get<JsonDocument::number_unsigned_t>());
        return true;
    }

    if (value.is_number_float()) {
        *result = DoubleToString::DoubleToStringConverter(value.get<double>());
        return true;
    }

    return false;
}

static const char *HandleJsonValueEmptyBehavior(int64_t contextPtr, int32_t emptyBehavior, const char *defaultOnEmpty,
                                                int32_t defaultOnEmptyLen, bool defaultOnEmptyIsNull, bool *outIsNull,
                                                int32_t *outLen)
{
    if (emptyBehavior == 2 && !defaultOnEmptyIsNull) {
        *outIsNull = false;
        *outLen = defaultOnEmptyLen;
        auto ret = ArenaAllocatorMalloc(contextPtr, *outLen + 1);
        memcpy_s(ret, *outLen + 1, defaultOnEmpty, *outLen + 1);
        return ret;
    }

    if (emptyBehavior == 1) {
        SetError(contextPtr, "JSON_VALUE error: Empty result");
    }
    *outIsNull = true;
    *outLen = 0;
    return nullptr;
}

extern "C" DLLEXPORT const char *JsonValueRetNull(int64_t contextPtr, const char *jsonStr, int32_t jsonStrLen,
                                                  bool jsonStrIsNull, const char *pathStr, int32_t pathStrWidth,
                                                  int32_t pathStrLen, bool pathStrIsNull, bool *outIsNull,
                                                  int32_t *outLen)
{
    const bool isConstantPath = pathStrWidth < 0;
    if (outIsNull == nullptr || outLen == nullptr) {
        return nullptr;
    }

    if (jsonStrIsNull || pathStrIsNull) {
        *outIsNull = true;
        *outLen = 0;
        return nullptr;
    }

    bool cachedIsNull = false;
    std::string result;
    if (JSON_VALUE_RESULT_CACHE.Get(jsonStr, jsonStrLen, pathStr, pathStrLen, 0, 0, &result, &cachedIsNull)) {
        if (cachedIsNull) {
            *outIsNull = true;
            *outLen = 0;
            return nullptr;
        }
        *outIsNull = false;
        *outLen = static_cast<int32_t>(result.size());
        auto ret = ArenaAllocatorMalloc(contextPtr, *outLen + 1);
        memcpy_s(ret, *outLen + 1, result.c_str(), *outLen + 1);
        return ret;
    }
    std::string jsonContent(jsonStr, jsonStrLen);
    std::string pathContent(pathStr, pathStrLen);
    JsonValueResultCacheKey cacheKey{jsonContent, pathContent, 0, 0};

    try {
        rapidjson::Document *jsonData = GetParsedJsonWithCacheRapidJson(contextPtr, jsonStr, jsonStrLen, jsonContent);
        if (jsonData == nullptr) {
            *outIsNull = true;
            *outLen = 0;
            return nullptr;
        }
        std::vector<std::string> dynamicKeys;
        const std::vector<std::string> *keys = nullptr;
        if (isConstantPath) {
            keys = &GetConstantJsonValuePath(pathStr, pathStrLen);
        } else {
            dynamicKeys = ParseJsonPath(pathContent);
            keys = &dynamicKeys;
        }
        rapidjson::Value *current = ResolveJsonPathTargetRapidJson(jsonData, *keys);
        if (current == nullptr || current->IsNull()) {
            *outIsNull = true;
            *outLen = 0;
            return nullptr;
        }

        if (!TryFormatJsonValueScalarRapidJson(*current, &result)) {
            JSON_VALUE_RESULT_CACHE.Put(cacheKey, "", true);
            *outIsNull = true;
            *outLen = 0;
            return nullptr;
        }
        JSON_VALUE_RESULT_CACHE.Put(cacheKey, result, false);

        *outIsNull = false;
        *outLen = static_cast<int32_t>(result.size());
        auto ret = ArenaAllocatorMalloc(contextPtr, *outLen + 1);
        memcpy_s(ret, *outLen + 1, result.c_str(), *outLen + 1);
        return ret;

    } catch (const std::exception &) {
        *outIsNull = true;
        *outLen = 0;
        return nullptr;
    }
}

namespace {
constexpr int32_t JSON_QUERY_WITHOUT_ARRAY_WRAPPER = 0;
constexpr int32_t JSON_QUERY_WITH_CONDITIONAL_ARRAY_WRAPPER = 1;
constexpr int32_t JSON_QUERY_WITH_UNCONDITIONAL_ARRAY_WRAPPER = 2;
constexpr int32_t JSON_QUERY_NULL_BEHAVIOR = 0;
constexpr int32_t JSON_QUERY_EMPTY_ARRAY_BEHAVIOR = 1;
constexpr int32_t JSON_QUERY_EMPTY_OBJECT_BEHAVIOR = 2;
constexpr int32_t JSON_QUERY_ERROR_BEHAVIOR = 3;

enum class JsonPathMode { LAX, STRICT };

std::string TrimAsciiWhitespace(const std::string &value)
{
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
        ++start;
    }

    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }
    return value.substr(start, end - start);
}

bool StartsWithIgnoreCase(const std::string &value, const std::string &prefix)
{
    if (value.size() < prefix.size()) {
        return false;
    }
    for (size_t index = 0; index < prefix.size(); ++index) {
        if (std::tolower(static_cast<unsigned char>(value[index])) !=
            std::tolower(static_cast<unsigned char>(prefix[index]))) {
            return false;
        }
    }
    return true;
}

bool ParseJsonQueryPathSpec(const std::string &rawPath, JsonPathMode *mode, std::string *normalizedPath)
{
    std::string trimmed = TrimAsciiWhitespace(rawPath);
    *mode = JsonPathMode::LAX;
    if (StartsWithIgnoreCase(trimmed, "strict ")) {
        *mode = JsonPathMode::STRICT;
        trimmed = TrimAsciiWhitespace(trimmed.substr(7));
    } else if (StartsWithIgnoreCase(trimmed, "lax ")) {
        trimmed = TrimAsciiWhitespace(trimmed.substr(4));
    }

    if (trimmed.empty()) {
        return false;
    }
    *normalizedPath = trimmed;
    return true;
}

struct ConstantJsonQueryPath {
    std::string rawPath;
    JsonPathMode mode = JsonPathMode::LAX;
    std::string normalizedPath;
    bool valid = false;
};

thread_local std::unordered_map<const char *, ConstantJsonQueryPath> CONSTANT_JSON_QUERY_PATHS;

const ConstantJsonQueryPath &GetConstantJsonQueryPath(const char *path, int32_t length)
{
    auto it = CONSTANT_JSON_QUERY_PATHS.find(path);
    if (it != CONSTANT_JSON_QUERY_PATHS.end() && it->second.rawPath.size() == static_cast<size_t>(length) &&
        memcmp(it->second.rawPath.data(), path, static_cast<size_t>(length)) == 0) {
        return it->second;
    }

    if (CONSTANT_JSON_QUERY_PATHS.size() >= CONSTANT_JSON_PATH_CACHE_CAPACITY) {
        CONSTANT_JSON_QUERY_PATHS.clear();
    }
    ConstantJsonQueryPath parsedPath;
    parsedPath.rawPath.assign(path, static_cast<size_t>(length));
    parsedPath.valid = ParseJsonQueryPathSpec(parsedPath.rawPath, &parsedPath.mode, &parsedPath.normalizedPath);
    auto inserted = CONSTANT_JSON_QUERY_PATHS.insert_or_assign(path, std::move(parsedPath));
    return inserted.first->second;
}

bool IsJsonQueryScalar(const JsonDocument &value)
{
    return !value.is_null() && !value.is_object() && !value.is_array();
}

bool IsJsonQueryScalarRapidJson(const rapidjson::Value &value)
{
    return !value.IsNull() && !value.IsObject() && !value.IsArray();
}

bool ApplyJsonQueryWrapperRapidJson(const rapidjson::Value &current, int32_t wrapperBehavior,
                                    rapidjson::Document *allocatorDoc, rapidjson::Value *wrappedValue)
{
    switch (wrapperBehavior) {
        case JSON_QUERY_WITHOUT_ARRAY_WRAPPER:
            wrappedValue->CopyFrom(current, allocatorDoc->GetAllocator());
            return true;
        case JSON_QUERY_WITH_CONDITIONAL_ARRAY_WRAPPER:
            if (current.IsArray()) {
                wrappedValue->CopyFrom(current, allocatorDoc->GetAllocator());
            } else {
                wrappedValue->SetArray();
                rapidjson::Value copiedValue;
                copiedValue.CopyFrom(current, allocatorDoc->GetAllocator());
                wrappedValue->PushBack(copiedValue, allocatorDoc->GetAllocator());
            }
            return true;
        case JSON_QUERY_WITH_UNCONDITIONAL_ARRAY_WRAPPER: {
            wrappedValue->SetArray();
            rapidjson::Value copiedValue2;
            copiedValue2.CopyFrom(current, allocatorDoc->GetAllocator());
            wrappedValue->PushBack(copiedValue2, allocatorDoc->GetAllocator());
        }
            return true;
        default: return false;
    }
}

rapidjson::Value *ResolveJsonQueryTargetRapidJson(rapidjson::Document *jsonData, const std::string &pathContent)
{
    if (pathContent == "$") {
        return jsonData;
    }
    std::vector<std::string> keys = ParseJsonPath(pathContent);
    if (keys.empty()) {
        return nullptr;
    }
    return ResolveJsonPathTargetRapidJson(jsonData, keys);
}

const char *CreateJsonQueryResult(int64_t contextPtr, const std::string &result, bool *outIsNull, int32_t *outLen)
{
    *outIsNull = false;
    *outLen = static_cast<int32_t>(result.size());
    auto ret = ArenaAllocatorMalloc(contextPtr, *outLen + 1);
    memcpy_s(ret, *outLen + 1, result.c_str(), *outLen + 1);
    return ret;
}

const char *HandleJsonQueryEmptyBehavior(int64_t contextPtr, int32_t emptyBehavior, bool *outIsNull, int32_t *outLen)
{
    switch (emptyBehavior) {
        case JSON_QUERY_NULL_BEHAVIOR:
            *outIsNull = true;
            *outLen = 0;
            return nullptr;
        case JSON_QUERY_EMPTY_ARRAY_BEHAVIOR: return CreateJsonQueryResult(contextPtr, "[]", outIsNull, outLen);
        case JSON_QUERY_EMPTY_OBJECT_BEHAVIOR: return CreateJsonQueryResult(contextPtr, "{}", outIsNull, outLen);
        case JSON_QUERY_ERROR_BEHAVIOR:
            SetError(contextPtr, "Empty result of JSON_QUERY function is not allowed");
            *outIsNull = true;
            *outLen = 0;
            return nullptr;
        default:
            SetError(contextPtr, "Illegal empty behavior in JSON_QUERY function");
            *outIsNull = true;
            *outLen = 0;
            return nullptr;
    }
}

const char *HandleJsonQueryErrorBehavior(int64_t contextPtr, int32_t errorBehavior, const char *message,
                                         bool *outIsNull, int32_t *outLen)
{
    switch (errorBehavior) {
        case JSON_QUERY_NULL_BEHAVIOR:
            *outIsNull = true;
            *outLen = 0;
            return nullptr;
        case JSON_QUERY_EMPTY_ARRAY_BEHAVIOR: return CreateJsonQueryResult(contextPtr, "[]", outIsNull, outLen);
        case JSON_QUERY_EMPTY_OBJECT_BEHAVIOR: return CreateJsonQueryResult(contextPtr, "{}", outIsNull, outLen);
        case JSON_QUERY_ERROR_BEHAVIOR:
            SetError(contextPtr, message);
            *outIsNull = true;
            *outLen = 0;
            return nullptr;
        default:
            SetError(contextPtr, "Illegal error behavior in JSON_QUERY function");
            *outIsNull = true;
            *outLen = 0;
            return nullptr;
    }
}

bool ApplyJsonQueryWrapper(const JsonDocument &current, int32_t wrapperBehavior, JsonDocument *wrappedValue)
{
    switch (wrapperBehavior) {
        case JSON_QUERY_WITHOUT_ARRAY_WRAPPER: *wrappedValue = current; return true;
        case JSON_QUERY_WITH_CONDITIONAL_ARRAY_WRAPPER:
            if (current.is_array()) {
                *wrappedValue = current;
            } else {
                *wrappedValue = JsonDocument::array();
                wrappedValue->push_back(current);
            }
            return true;
        case JSON_QUERY_WITH_UNCONDITIONAL_ARRAY_WRAPPER:
            *wrappedValue = JsonDocument::array();
            wrappedValue->push_back(current);
            return true;
        default: return false;
    }
}

JsonDocument *ResolveJsonQueryTarget(JsonDocument *jsonData, const std::string &pathContent)
{
    if (pathContent == "$") {
        return jsonData;
    }
    return ResolveJsonPathTarget(jsonData, pathContent);
}
} // namespace

extern "C" DLLEXPORT const char *
JsonQueryWithWrapperAndBehavior(int64_t contextPtr, const char *jsonStr, int32_t jsonStrLen, bool jsonStrIsNull,
                                const char *pathStr, int32_t pathStrWidth, int32_t pathStrLen, bool pathStrIsNull,
                                int32_t wrapperBehavior, bool wrapperBehaviorIsNull, int32_t emptyBehavior,
                                bool emptyBehaviorIsNull, int32_t errorBehavior, bool errorBehaviorIsNull,
                                bool *outIsNull, int32_t *outLen)
{
    const bool isConstantPath = pathStrWidth < 0;
    if (outIsNull == nullptr || outLen == nullptr) {
        return nullptr;
    }

    if (jsonStrIsNull || pathStrIsNull) {
        *outIsNull = true;
        *outLen = 0;
        return nullptr;
    }

    int32_t normalizedWrapper = wrapperBehaviorIsNull ? JSON_QUERY_WITHOUT_ARRAY_WRAPPER : wrapperBehavior;
    int32_t normalizedEmpty = emptyBehaviorIsNull ? JSON_QUERY_NULL_BEHAVIOR : emptyBehavior;
    int32_t normalizedError = errorBehaviorIsNull ? JSON_QUERY_NULL_BEHAVIOR : errorBehavior;
    std::string result;
    if (JSON_QUERY_RESULT_CACHE.Get(jsonStr, jsonStrLen, pathStr, pathStrLen, normalizedWrapper, normalizedEmpty,
                                    normalizedError, &result)) {
        return CreateJsonQueryResult(contextPtr, result, outIsNull, outLen);
    }
    std::string jsonContent(jsonStr, jsonStrLen);
    std::string rawPath(pathStr, pathStrLen);
    JsonQueryResultCacheKey cacheKey{jsonContent, rawPath, normalizedWrapper, normalizedEmpty, normalizedError};
    JsonPathMode pathMode = JsonPathMode::LAX;
    std::string pathContent;
    bool validPath = false;
    if (isConstantPath) {
        const ConstantJsonQueryPath &parsedPath = GetConstantJsonQueryPath(pathStr, pathStrLen);
        pathMode = parsedPath.mode;
        pathContent = parsedPath.normalizedPath;
        validPath = parsedPath.valid;
    } else {
        validPath = ParseJsonQueryPathSpec(rawPath, &pathMode, &pathContent);
    }
    if (!validPath) {
        return HandleJsonQueryErrorBehavior(contextPtr, normalizedError, "Illegal JSON path in JSON_QUERY function",
                                            outIsNull, outLen);
    }

    try {
        rapidjson::Document *jsonData = GetParsedJsonWithCacheRapidJson(contextPtr, jsonStr, jsonStrLen, jsonContent);
        if (jsonData == nullptr) {
            return HandleJsonQueryErrorBehavior(contextPtr, normalizedError, "Invalid JSON", outIsNull, outLen);
        }
        rapidjson::Value *current = ResolveJsonQueryTargetRapidJson(jsonData, pathContent);
        if (current == nullptr) {
            if (pathMode == JsonPathMode::STRICT) {
                return HandleJsonQueryErrorBehavior(contextPtr, normalizedError, "No results for path", outIsNull,
                                                    outLen);
            }
            return HandleJsonQueryEmptyBehavior(contextPtr, normalizedEmpty, outIsNull, outLen);
        }

        rapidjson::Document allocatorDoc;
        rapidjson::Value wrappedValue;
        if (!ApplyJsonQueryWrapperRapidJson(*current, normalizedWrapper, &allocatorDoc, &wrappedValue)) {
            return HandleJsonQueryErrorBehavior(contextPtr, normalizedError,
                                                "Illegal wrapper behavior in JSON_QUERY function", outIsNull, outLen);
        }

        if (wrappedValue.IsNull() || (pathMode == JsonPathMode::LAX && IsJsonQueryScalarRapidJson(wrappedValue))) {
            return HandleJsonQueryEmptyBehavior(contextPtr, normalizedEmpty, outIsNull, outLen);
        }
        if (pathMode == JsonPathMode::STRICT && IsJsonQueryScalarRapidJson(wrappedValue)) {
            return HandleJsonQueryErrorBehavior(contextPtr, normalizedError,
                                                "Array or object value required in "
                                                "strict mode of JSON_QUERY function",
                                                outIsNull, outLen);
        }

        result = SerializeRapidJsonValue(wrappedValue);
        JSON_QUERY_RESULT_CACHE.Put(cacheKey, result);
        return CreateJsonQueryResult(contextPtr, result, outIsNull, outLen);
    } catch (const std::exception &e) {
        return HandleJsonQueryErrorBehavior(contextPtr, normalizedError, e.what(), outIsNull, outLen);
    }
}

extern "C" DLLEXPORT const char *JsonQueryRetNull(int64_t contextPtr, const char *jsonStr, int32_t jsonStrLen,
                                                  bool jsonStrIsNull, const char *pathStr, int32_t pathStrWidth,
                                                  int32_t pathStrLen, bool pathStrIsNull, bool *outIsNull,
                                                  int32_t *outLen)
{
    return JsonQueryWithWrapperAndBehavior(contextPtr, jsonStr, jsonStrLen, jsonStrIsNull, pathStr, pathStrWidth,
                                           pathStrLen, pathStrIsNull, JSON_QUERY_WITHOUT_ARRAY_WRAPPER, false,
                                           JSON_QUERY_NULL_BEHAVIOR, false, JSON_QUERY_NULL_BEHAVIOR, false, outIsNull,
                                           outLen);
}

// Extended JSON_VALUE function with ON EMPTY/ERROR behaviors
// emptyBehavior: 0=NULL, 1=ERROR, 2=DEFAULT
// errorBehavior: 0=NULL, 1=ERROR, 2=DEFAULT
extern "C" DLLEXPORT const char *JsonValueExtended(int64_t contextPtr, const char *jsonStr, int32_t jsonStrLen,
                                                   bool jsonStrIsNull, const char *pathStr, int32_t pathStrWidth,
                                                   int32_t pathStrLen, bool pathStrIsNull, int32_t emptyBehavior,
                                                   const char *defaultOnEmpty, int32_t defaultOnEmptyLen,
                                                   bool defaultOnEmptyIsNull, int32_t errorBehavior,
                                                   const char *defaultOnError, int32_t defaultOnErrorLen,
                                                   bool defaultOnErrorIsNull, bool *outIsNull, int32_t *outLen)
{
    const bool isConstantPath = pathStrWidth < 0;
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

    bool cachedIsNull = false;
    std::string result;
    if (JSON_VALUE_RESULT_CACHE.Get(jsonStr, jsonStrLen, pathStr, pathStrLen, emptyBehavior, errorBehavior, &result,
                                    &cachedIsNull)) {
        if (cachedIsNull) {
            return HandleJsonValueEmptyBehavior(contextPtr, emptyBehavior, defaultOnEmpty, defaultOnEmptyLen,
                                                defaultOnEmptyIsNull, outIsNull, outLen);
        }
        *outIsNull = false;
        *outLen = static_cast<int32_t>(result.size());
        auto ret = ArenaAllocatorMalloc(contextPtr, *outLen + 1);
        memcpy_s(ret, *outLen + 1, result.c_str(), *outLen + 1);
        return ret;
    }
    std::string jsonContent(jsonStr, jsonStrLen);
    std::string pathContent(pathStr, pathStrLen);
    JsonValueResultCacheKey cacheKey{jsonContent, pathContent, emptyBehavior, errorBehavior};

    try {
        rapidjson::Document *jsonData = GetParsedJsonWithCacheRapidJson(contextPtr, jsonStr, jsonStrLen, jsonContent);
        if (jsonData == nullptr) {
            if (errorBehavior == 2 && !defaultOnErrorIsNull) {
                *outIsNull = false;
                *outLen = defaultOnErrorLen;
                auto ret = ArenaAllocatorMalloc(contextPtr, *outLen + 1);
                memcpy_s(ret, *outLen + 1, defaultOnError, *outLen + 1);
                return ret;
            } else if (errorBehavior == 1) {
                SetError(contextPtr, "JSON_VALUE error: Invalid JSON");
                *outIsNull = true;
                *outLen = 0;
                return nullptr;
            } else {
                *outIsNull = true;
                *outLen = 0;
                return nullptr;
            }
        }

        std::vector<std::string> dynamicKeys;
        const std::vector<std::string> *keys = nullptr;
        if (isConstantPath) {
            keys = &GetConstantJsonValuePath(pathStr, pathStrLen);
        } else {
            dynamicKeys = ParseJsonPath(pathContent);
            keys = &dynamicKeys;
        }

        if (keys->empty()) {
            if (errorBehavior == 2 && !defaultOnErrorIsNull) {
                *outIsNull = false;
                *outLen = defaultOnErrorLen;
                auto ret = ArenaAllocatorMalloc(contextPtr, *outLen + 1);
                memcpy_s(ret, *outLen + 1, defaultOnError, *outLen + 1);
                return ret;
            } else if (errorBehavior == 1) {
                SetError(contextPtr, "JSON_VALUE error: Invalid JSON path");
                *outIsNull = true;
                *outLen = 0;
                return nullptr;
            } else {
                *outIsNull = true;
                *outLen = 0;
                return nullptr;
            }
        }

        rapidjson::Value *current = ResolveJsonPathTargetRapidJson(jsonData, *keys);
        if (current == nullptr || current->IsNull()) {
            return HandleJsonValueEmptyBehavior(contextPtr, emptyBehavior, defaultOnEmpty, defaultOnEmptyLen,
                                                defaultOnEmptyIsNull, outIsNull, outLen);
        }

        if (!TryFormatJsonValueScalarRapidJson(*current, &result)) {
            JSON_VALUE_RESULT_CACHE.Put(cacheKey, "", true);
            return HandleJsonValueEmptyBehavior(contextPtr, emptyBehavior, defaultOnEmpty, defaultOnEmptyLen,
                                                defaultOnEmptyIsNull, outIsNull, outLen);
        }
        JSON_VALUE_RESULT_CACHE.Put(cacheKey, result, false);

        *outIsNull = false;
        *outLen = static_cast<int32_t>(result.size());
        auto ret = ArenaAllocatorMalloc(contextPtr, *outLen + 1);
        memcpy_s(ret, *outLen + 1, result.c_str(), *outLen + 1);
        return ret;

    } catch (const std::exception &e) {
        if (errorBehavior == 2 && !defaultOnErrorIsNull) {
            *outIsNull = false;
            *outLen = defaultOnErrorLen;
            auto ret = ArenaAllocatorMalloc(contextPtr, *outLen + 1);
            memcpy_s(ret, *outLen + 1, defaultOnError, *outLen + 1);
            return ret;
        } else if (errorBehavior == 1) {
            std::string errMsg = std::string("JSON_VALUE error: ") + e.what();
            SetError(contextPtr, errMsg.c_str());
            *outIsNull = true;
            *outLen = 0;
            return nullptr;
        } else {
            *outIsNull = true;
            *outLen = 0;
            return nullptr;
        }
    }
}

extern "C" DLLEXPORT const char *JsonValueWithBehaviors(int64_t contextPtr, const char *jsonStr, int32_t jsonStrLen,
                                                        bool jsonStrIsNull, const char *pathStr, int32_t pathStrWidth,
                                                        int32_t pathStrLen, bool pathStrIsNull, int32_t emptyBehavior,
                                                        bool emptyBehaviorIsNull, const char *defaultOnEmpty,
                                                        int32_t defaultOnEmptyLen, bool defaultOnEmptyIsNull,
                                                        int32_t errorBehavior, bool errorBehaviorIsNull,
                                                        const char *defaultOnError, int32_t defaultOnErrorLen,
                                                        bool defaultOnErrorIsNull, bool *outIsNull, int32_t *outLen)
{
    static_cast<void>(pathStrWidth);
    int32_t normalizedEmptyBehavior = emptyBehaviorIsNull ? 0 : emptyBehavior;
    int32_t normalizedErrorBehavior = errorBehaviorIsNull ? 0 : errorBehavior;
    return JsonValueExtended(contextPtr, jsonStr, jsonStrLen, jsonStrIsNull, pathStr, pathStrWidth, pathStrLen,
                             pathStrIsNull, normalizedEmptyBehavior, defaultOnEmpty, defaultOnEmptyLen,
                             defaultOnEmptyIsNull, normalizedErrorBehavior, defaultOnError, defaultOnErrorLen,
                             defaultOnErrorIsNull, outIsNull, outLen);
}

namespace {
bool TryParseJson(const std::string &jsonContent, JsonDocument *jsonData)
{
    try {
        *jsonData = JsonDocument::parse(jsonContent);
        return true;
    } catch (const std::exception &) {
        return false;
    }
}

std::string RepairEscapedQuotes(const std::string &jsonContent)
{
    std::string fixedJsonContent;
    fixedJsonContent.reserve(jsonContent.size());
    for (size_t index = 0; index < jsonContent.size(); index++) {
        if (jsonContent[index] == '\\') {
            if (index + 1 < jsonContent.size()) {
                char next = jsonContent[index + 1];
                if (next == '\\' || next == '"') {
                    fixedJsonContent += jsonContent[index];
                } else {
                    fixedJsonContent += '"';
                }
            } else {
                fixedJsonContent += '"';
            }
        } else {
            fixedJsonContent += jsonContent[index];
        }
    }
    return fixedJsonContent;
}

std::string NormalizeSingleQuotedJsonLike(const std::string &jsonContent)
{
    std::string normalized;
    normalized.reserve(jsonContent.size() + 8);
    bool inSingleQuotedString = false;
    bool inDoubleQuotedString = false;
    for (size_t index = 0; index < jsonContent.size(); index++) {
        char current = jsonContent[index];
        if (inSingleQuotedString) {
            if (current == '\\') {
                if (index + 1 < jsonContent.size()) {
                    char next = jsonContent[index + 1];
                    if (next == '\'') {
                        normalized += '\'';
                        index++;
                        continue;
                    }
                    if (next == '"') {
                        normalized += "\\\"";
                        index++;
                        continue;
                    }
                    if (next == '\\') {
                        normalized += "\\\\";
                        index++;
                        continue;
                    }
                }
                normalized += "\\\\";
                continue;
            }
            if (current == '\'') {
                inSingleQuotedString = false;
                normalized += '"';
                continue;
            }
            if (current == '"') {
                normalized += "\\\"";
                continue;
            }
            normalized += current;
            continue;
        }
        if (inDoubleQuotedString) {
            normalized += current;
            if (current == '\\' && index + 1 < jsonContent.size()) {
                normalized += jsonContent[++index];
                continue;
            }
            if (current == '"') {
                inDoubleQuotedString = false;
            }
            continue;
        }
        if (current == '\'') {
            inSingleQuotedString = true;
            normalized += '"';
            continue;
        }
        if (current == '"') {
            inDoubleQuotedString = true;
        }
        normalized += current;
    }
    return normalized;
}

uint32_t DecodeUtf8CodePoint(const std::string &value, size_t *index)
{
    unsigned char firstByte = static_cast<unsigned char>(value[*index]);
    if (firstByte < 0x80) {
        ++(*index);
        return firstByte;
    }

    if ((firstByte & 0xE0) == 0xC0 && *index + 1 < value.size()) {
        uint32_t codePoint = (static_cast<uint32_t>(firstByte & 0x1F) << 6) |
                             static_cast<uint32_t>(static_cast<unsigned char>(value[*index + 1]) & 0x3F);
        *index += 2;
        return codePoint;
    }

    if ((firstByte & 0xF0) == 0xE0 && *index + 2 < value.size()) {
        uint32_t codePoint = (static_cast<uint32_t>(firstByte & 0x0F) << 12) |
                             (static_cast<uint32_t>(static_cast<unsigned char>(value[*index + 1]) & 0x3F) << 6) |
                             static_cast<uint32_t>(static_cast<unsigned char>(value[*index + 2]) & 0x3F);
        *index += 3;
        return codePoint;
    }

    if ((firstByte & 0xF8) == 0xF0 && *index + 3 < value.size()) {
        uint32_t codePoint = (static_cast<uint32_t>(firstByte & 0x07) << 18) |
                             (static_cast<uint32_t>(static_cast<unsigned char>(value[*index + 1]) & 0x3F) << 12) |
                             (static_cast<uint32_t>(static_cast<unsigned char>(value[*index + 2]) & 0x3F) << 6) |
                             static_cast<uint32_t>(static_cast<unsigned char>(value[*index + 3]) & 0x3F);
        *index += 4;
        return codePoint;
    }

    ++(*index);
    return firstByte;
}

uint32_t JavaStringHash(const std::string &value)
{
    uint32_t hash = 0;
    for (size_t index = 0; index < value.size();) {
        uint32_t codePoint = DecodeUtf8CodePoint(value, &index);
        if (codePoint <= 0xFFFF) {
            hash = 31U * hash + codePoint;
            continue;
        }

        codePoint -= 0x10000;
        uint32_t highSurrogate = 0xD800U + (codePoint >> 10);
        uint32_t lowSurrogate = 0xDC00U + (codePoint & 0x3FFU);
        hash = 31U * hash + highSurrogate;
        hash = 31U * hash + lowSurrogate;
    }
    return hash;
}

size_t JavaHashMapCapacity(size_t size)
{
    size_t capacity = 16;
    while (size > (capacity * 3) / 4) {
        capacity <<= 1;
    }
    return capacity;
}

size_t JavaHashMapBucket(const std::string &key, size_t capacity)
{
    uint32_t hash = JavaStringHash(key);
    hash ^= (hash >> 16);
    return static_cast<size_t>(hash & static_cast<uint32_t>(capacity - 1));
}

std::string SerializeJsonSplitValueRapidJson(const rapidjson::Value &value);

struct JsonSplitObjectEntryRapidJson {
    std::string key;
    const rapidjson::Value *value;
    size_t insertionIndex;
    size_t bucket;
};

std::string SerializeJsonSplitObjectRapidJson(const rapidjson::Value &value)
{
    std::vector<JsonSplitObjectEntryRapidJson> entries;
    size_t capacity = JavaHashMapCapacity(value.MemberCount());
    size_t insertionIndex = 0;
    for (auto it = value.MemberBegin(); it != value.MemberEnd(); ++it, ++insertionIndex) {
        entries.push_back({std::string(it->name.GetString(), it->name.GetStringLength()), &it->value, insertionIndex,
                           JavaHashMapBucket(std::string(it->name.GetString(), it->name.GetStringLength()), capacity)});
    }

    std::stable_sort(entries.begin(), entries.end(),
                     [](const JsonSplitObjectEntryRapidJson &left, const JsonSplitObjectEntryRapidJson &right) {
                         if (left.bucket != right.bucket) {
                             return left.bucket < right.bucket;
                         }
                         return left.insertionIndex < right.insertionIndex;
                     });

    std::string result = "{";
    for (size_t index = 0; index < entries.size(); ++index) {
        if (index > 0) {
            result += ",";
        }
        rapidjson::Document keyDoc;
        keyDoc.SetString(entries[index].key.c_str(), entries[index].key.size());
        result += SerializeRapidJsonValue(keyDoc);
        result += ":";
        result += SerializeJsonSplitValueRapidJson(*entries[index].value);
    }
    result += "}";
    return result;
}

std::string SerializeJsonSplitArrayRapidJson(const rapidjson::Value &value)
{
    std::string result = "[";
    for (size_t index = 0; index < value.Size(); ++index) {
        if (index > 0) {
            result += ",";
        }
        result += SerializeJsonSplitValueRapidJson(value[index]);
    }
    result += "]";
    return result;
}

std::string SerializeJsonSplitNumberRapidJson(const rapidjson::Value &value)
{
    std::string result = SerializeRapidJsonValue(value);
    std::replace(result.begin(), result.end(), 'e', 'E');
    return result;
}

std::string SerializeJsonSplitValueRapidJson(const rapidjson::Value &value)
{
    if (value.IsObject()) {
        return SerializeJsonSplitObjectRapidJson(value);
    }
    if (value.IsArray()) {
        return SerializeJsonSplitArrayRapidJson(value);
    }
    if (value.IsNumber()) {
        return SerializeJsonSplitNumberRapidJson(value);
    }
    return SerializeRapidJsonValue(value);
}

std::string SerializeJsonSplitValue(const JsonDocument &value);

struct JsonSplitObjectEntry {
    std::string key;
    const JsonDocument *value;
    size_t insertionIndex;
    size_t bucket;
};

std::string SerializeJsonSplitObject(const JsonDocument &value)
{
    std::vector<JsonSplitObjectEntry> entries;
    entries.reserve(value.size());
    size_t capacity = JavaHashMapCapacity(value.size());
    size_t insertionIndex = 0;
    for (auto it = value.begin(); it != value.end(); ++it, ++insertionIndex) {
        entries.push_back({it.key(), &it.value(), insertionIndex, JavaHashMapBucket(it.key(), capacity)});
    }

    std::stable_sort(entries.begin(), entries.end(),
                     [](const JsonSplitObjectEntry &left, const JsonSplitObjectEntry &right) {
                         if (left.bucket != right.bucket) {
                             return left.bucket < right.bucket;
                         }
                         return left.insertionIndex < right.insertionIndex;
                     });

    std::string result = "{";
    for (size_t index = 0; index < entries.size(); ++index) {
        if (index > 0) {
            result += ",";
        }
        result += JsonDocument(entries[index].key).dump();
        result += ":";
        result += SerializeJsonSplitValue(*entries[index].value);
    }
    result += "}";
    return result;
}

std::string SerializeJsonSplitArray(const JsonDocument &value)
{
    std::string result = "[";
    for (size_t index = 0; index < value.size(); ++index) {
        if (index > 0) {
            result += ",";
        }
        result += SerializeJsonSplitValue(value[index]);
    }
    result += "]";
    return result;
}

std::string SerializeJsonSplitNumber(const JsonDocument &value)
{
    std::string result = value.dump();
    std::replace(result.begin(), result.end(), 'e', 'E');
    return result;
}

std::string TrimJsonSplitToken(const std::string &token)
{
    size_t start = 0;
    while (start < token.size() && std::isspace(static_cast<unsigned char>(token[start]))) {
        ++start;
    }

    size_t end = token.size();
    while (end > start && std::isspace(static_cast<unsigned char>(token[end - 1]))) {
        --end;
    }
    return token.substr(start, end - start);
}

bool ExtractJsonSplitTopLevelElements(const std::string &jsonContent, std::vector<std::string> *elements)
{
    if (elements == nullptr) {
        return false;
    }

    elements->clear();
    std::string trimmedContent = TrimJsonSplitToken(jsonContent);
    if (trimmedContent.size() < 2 || trimmedContent.front() != '[' || trimmedContent.back() != ']') {
        return false;
    }

    size_t contentStart = 1;
    size_t contentEnd = trimmedContent.size() - 1;
    size_t elementStart = contentStart;
    int32_t nestedDepth = 0;
    bool inString = false;
    bool isEscaped = false;

    for (size_t index = contentStart; index < contentEnd; ++index) {
        char current = trimmedContent[index];
        if (inString) {
            if (isEscaped) {
                isEscaped = false;
                continue;
            }
            if (current == '\\') {
                isEscaped = true;
                continue;
            }
            if (current == '"') {
                inString = false;
            }
            continue;
        }

        if (current == '"') {
            inString = true;
            continue;
        }
        if (current == '[' || current == '{') {
            ++nestedDepth;
            continue;
        }
        if (current == ']' || current == '}') {
            --nestedDepth;
            continue;
        }
        if (current == ',' && nestedDepth == 0) {
            elements->push_back(TrimJsonSplitToken(trimmedContent.substr(elementStart, index - elementStart)));
            elementStart = index + 1;
        }
    }

    std::string lastElement = TrimJsonSplitToken(trimmedContent.substr(elementStart, contentEnd - elementStart));
    if (!lastElement.empty()) {
        elements->push_back(lastElement);
    }
    return true;
}

std::string NormalizeJsonSplitNumberToken(const std::string &token)
{
    std::string normalized = TrimJsonSplitToken(token);
    std::replace(normalized.begin(), normalized.end(), 'e', 'E');
    return normalized;
}

std::string SerializeJsonSplitValue(const JsonDocument &value)
{
    if (value.is_object()) {
        return SerializeJsonSplitObject(value);
    }
    if (value.is_array()) {
        return SerializeJsonSplitArray(value);
    }
    if (value.is_number()) {
        return SerializeJsonSplitNumber(value);
    }
    return value.dump();
}

bool TryParseJsonSplitContentRapidJson(const std::string &jsonContent, rapidjson::Document *jsonData,
                                       std::string *parsedJsonContent)
{
    jsonData->Parse(jsonContent.c_str());
    if (!jsonData->HasParseError()) {
        if (parsedJsonContent != nullptr) {
            *parsedJsonContent = jsonContent;
        }
        return true;
    }

    std::string repairedJsonContent = RepairEscapedQuotes(jsonContent);
    if (repairedJsonContent != jsonContent) {
        jsonData->Parse(repairedJsonContent.c_str());
        if (!jsonData->HasParseError()) {
            if (parsedJsonContent != nullptr) {
                *parsedJsonContent = repairedJsonContent;
            }
            return true;
        }
    }

    std::string normalizedJsonContent = NormalizeSingleQuotedJsonLike(jsonContent);
    if (normalizedJsonContent != jsonContent) {
        jsonData->Parse(normalizedJsonContent.c_str());
        if (!jsonData->HasParseError()) {
            if (parsedJsonContent != nullptr) {
                *parsedJsonContent = normalizedJsonContent;
            }
            return true;
        }
    }

    std::string repairedNormalizedJsonContent = RepairEscapedQuotes(normalizedJsonContent);
    if (normalizedJsonContent != repairedJsonContent) {
        jsonData->Parse(repairedNormalizedJsonContent.c_str());
        if (!jsonData->HasParseError()) {
            if (parsedJsonContent != nullptr) {
                *parsedJsonContent = repairedNormalizedJsonContent;
            }
            return true;
        }
    }

    return false;
}

bool TryParseJsonSplitContent(const std::string &jsonContent, JsonDocument *jsonData,
                              std::string *parsedJsonContent = nullptr)
{
    if (TryParseJson(jsonContent, jsonData)) {
        if (parsedJsonContent != nullptr) {
            *parsedJsonContent = jsonContent;
        }
        return true;
    }
    std::string repairedJsonContent = RepairEscapedQuotes(jsonContent);
    if (repairedJsonContent != jsonContent && TryParseJson(repairedJsonContent, jsonData)) {
        if (parsedJsonContent != nullptr) {
            *parsedJsonContent = repairedJsonContent;
        }
        return true;
    }
    std::string normalizedJsonContent = NormalizeSingleQuotedJsonLike(jsonContent);
    if (normalizedJsonContent != jsonContent && TryParseJson(normalizedJsonContent, jsonData)) {
        if (parsedJsonContent != nullptr) {
            *parsedJsonContent = normalizedJsonContent;
        }
        return true;
    }
    std::string repairedNormalizedJsonContent = RepairEscapedQuotes(normalizedJsonContent);
    if (normalizedJsonContent != repairedJsonContent && TryParseJson(repairedNormalizedJsonContent, jsonData)) {
        if (parsedJsonContent != nullptr) {
            *parsedJsonContent = repairedNormalizedJsonContent;
        }
        return true;
    }
    return false;
}
} // namespace

// JSON_SPLIT_SCALAR function: splits JSON array and joins all elements with
// CRLF Matches the semantics of the jsontest UDF (1 STRING argument -> STRING
// result)
extern "C" DLLEXPORT const char *JsonSplitScalar(int64_t contextPtr, const char *jsonStr, int32_t jsonStrLen,
                                                 bool jsonStrIsNull, bool *outIsNull, int32_t *outLen)
{
    if (outIsNull == nullptr || outLen == nullptr) {
        return nullptr;
    }

    // Handle NULL input
    if (jsonStrIsNull || jsonStr == nullptr || jsonStrLen <= 0) {
        *outIsNull = true;
        *outLen = 0;
        return nullptr;
    }

    std::string jsonContent(jsonStr, jsonStrLen);

    try {
        rapidjson::Document jsonData;
        std::string parsedJsonContent;
        if (!TryParseJsonSplitContentRapidJson(jsonContent, &jsonData, &parsedJsonContent)) {
            *outIsNull = true;
            *outLen = 0;
            return nullptr;
        }

        if (!jsonData.IsArray()) {
            *outIsNull = true;
            *outLen = 0;
            return nullptr;
        }

        std::vector<std::string> rawElements;
        bool hasRawElements =
            ExtractJsonSplitTopLevelElements(parsedJsonContent, &rawElements) && rawElements.size() == jsonData.Size();

        std::string result;
        for (size_t i = 0; i < jsonData.Size(); i++) {
            if (i > 0) {
                result += "\r\n";
            }
            const rapidjson::Value &element = jsonData[i];
            if (element.IsString()) {
                result += std::string(element.GetString(), element.GetStringLength());
            } else if (hasRawElements && element.IsNumber()) {
                result += NormalizeJsonSplitNumberToken(rawElements[i]);
            } else {
                result += SerializeJsonSplitValueRapidJson(element);
            }
        }

        *outIsNull = false;
        *outLen = static_cast<int32_t>(result.size());
        auto ret = ArenaAllocatorMalloc(contextPtr, *outLen + 1);
        memcpy_s(ret, *outLen + 1, result.c_str(), *outLen + 1);
        return ret;

    } catch (const std::exception &) {
        *outIsNull = true;
        *outLen = 0;
        return nullptr;
    }
}

extern "C" DLLEXPORT const char *JsonSplitScalarChar(int64_t contextPtr, const char *jsonStr, int32_t jsonStrWidth,
                                                     int32_t jsonStrLen, bool jsonStrIsNull, bool *outIsNull,
                                                     int32_t *outLen)
{
    static_cast<void>(jsonStrWidth);
    return JsonSplitScalar(contextPtr, jsonStr, jsonStrLen, jsonStrIsNull, outIsNull, outLen);
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
                                                  bool separatorIsNull, const char *s1, int32_t s1Len, bool s1IsNull,
                                                  bool *retIsNull, int32_t *outLen)
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
                                             bool separatorIsNull, const char *s1, int32_t s1Len, bool s1IsNull,
                                             const char *s2, int32_t s2Len, bool s2IsNull, bool *retIsNull,
                                             int32_t *outLen)
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
    const char *ret =
        StringUtil::ConcatWsStrDiffWidths(contextPtr, separator, separatorLen, s1, s1Len, s2, s2Len, &hasErr, outLen);
    if (hasErr) {
        SetError(contextPtr, CONCAT_WS_ERR_MSG);
        *retIsNull = true;
        *outLen = 0;
        return nullptr;
    }
    return ret;
}

extern "C" DLLEXPORT const char *ConcatWs3Str(int64_t contextPtr, const char *separator, int32_t separatorLen,
                                              bool separatorIsNull, const char *s1, int32_t s1Len, bool s1IsNull,
                                              const char *s2, int32_t s2Len, bool s2IsNull, const char *s3,
                                              int32_t s3Len, bool s3IsNull, bool *retIsNull, int32_t *outLen)
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

    if (!s1IsNull && !StringUtil::ConcatWsAppend(contextPtr, separator, separatorLen, tmp, tmpLen, tmpSet, s1, s1Len,
                                                 &hasErr, outLen)) {
        SetError(contextPtr, CONCAT_WS_ERR_MSG);
        *retIsNull = true;
        *outLen = 0;
        return nullptr;
    }
    if (!s2IsNull && !StringUtil::ConcatWsAppend(contextPtr, separator, separatorLen, tmp, tmpLen, tmpSet, s2, s2Len,
                                                 &hasErr, outLen)) {
        SetError(contextPtr, CONCAT_WS_ERR_MSG);
        *retIsNull = true;
        *outLen = 0;
        return nullptr;
    }
    if (!s3IsNull && !StringUtil::ConcatWsAppend(contextPtr, separator, separatorLen, tmp, tmpLen, tmpSet, s3, s3Len,
                                                 &hasErr, outLen)) {
        SetError(contextPtr, CONCAT_WS_ERR_MSG);
        *retIsNull = true;
        *outLen = 0;
        return nullptr;
    }
    *outLen = tmpLen;
    return tmp;
}

extern "C" DLLEXPORT const char *ConcatWs4Str(int64_t contextPtr, const char *separator, int32_t separatorLen,
                                              bool separatorIsNull, const char *s1, int32_t s1Len, bool s1IsNull,
                                              const char *s2, int32_t s2Len, bool s2IsNull, const char *s3,
                                              int32_t s3Len, bool s3IsNull, const char *s4, int32_t s4Len,
                                              bool s4IsNull, bool *retIsNull, int32_t *outLen)
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

    if (!s1IsNull && !StringUtil::ConcatWsAppend(contextPtr, separator, separatorLen, tmp, tmpLen, tmpSet, s1, s1Len,
                                                 &hasErr, outLen)) {
        SetError(contextPtr, CONCAT_WS_ERR_MSG);
        *retIsNull = true;
        *outLen = 0;
        return nullptr;
    }
    if (!s2IsNull && !StringUtil::ConcatWsAppend(contextPtr, separator, separatorLen, tmp, tmpLen, tmpSet, s2, s2Len,
                                                 &hasErr, outLen)) {
        SetError(contextPtr, CONCAT_WS_ERR_MSG);
        *retIsNull = true;
        *outLen = 0;
        return nullptr;
    }
    if (!s3IsNull && !StringUtil::ConcatWsAppend(contextPtr, separator, separatorLen, tmp, tmpLen, tmpSet, s3, s3Len,
                                                 &hasErr, outLen)) {
        SetError(contextPtr, CONCAT_WS_ERR_MSG);
        *retIsNull = true;
        *outLen = 0;
        return nullptr;
    }
    if (!s4IsNull && !StringUtil::ConcatWsAppend(contextPtr, separator, separatorLen, tmp, tmpLen, tmpSet, s4, s4Len,
                                                 &hasErr, outLen)) {
        SetError(contextPtr, CONCAT_WS_ERR_MSG);
        *retIsNull = true;
        *outLen = 0;
        return nullptr;
    }
    *outLen = tmpLen;
    return tmp;
}

extern "C" DLLEXPORT const char *ConcatWs5Str(int64_t contextPtr, const char *separator, int32_t separatorLen,
                                              bool separatorIsNull, const char *s1, int32_t s1Len, bool s1IsNull,
                                              const char *s2, int32_t s2Len, bool s2IsNull, const char *s3,
                                              int32_t s3Len, bool s3IsNull, const char *s4, int32_t s4Len,
                                              bool s4IsNull, const char *s5, int32_t s5Len, bool s5IsNull,
                                              bool *retIsNull, int32_t *outLen)
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

    if (!s1IsNull && !StringUtil::ConcatWsAppend(contextPtr, separator, separatorLen, tmp, tmpLen, tmpSet, s1, s1Len,
                                                 &hasErr, outLen)) {
        SetError(contextPtr, CONCAT_WS_ERR_MSG);
        *retIsNull = true;
        *outLen = 0;
        return nullptr;
    }
    if (!s2IsNull && !StringUtil::ConcatWsAppend(contextPtr, separator, separatorLen, tmp, tmpLen, tmpSet, s2, s2Len,
                                                 &hasErr, outLen)) {
        SetError(contextPtr, CONCAT_WS_ERR_MSG);
        *retIsNull = true;
        *outLen = 0;
        return nullptr;
    }
    if (!s3IsNull && !StringUtil::ConcatWsAppend(contextPtr, separator, separatorLen, tmp, tmpLen, tmpSet, s3, s3Len,
                                                 &hasErr, outLen)) {
        SetError(contextPtr, CONCAT_WS_ERR_MSG);
        *retIsNull = true;
        *outLen = 0;
        return nullptr;
    }
    if (!s4IsNull && !StringUtil::ConcatWsAppend(contextPtr, separator, separatorLen, tmp, tmpLen, tmpSet, s4, s4Len,
                                                 &hasErr, outLen)) {
        SetError(contextPtr, CONCAT_WS_ERR_MSG);
        *retIsNull = true;
        *outLen = 0;
        return nullptr;
    }
    if (!s5IsNull && !StringUtil::ConcatWsAppend(contextPtr, separator, separatorLen, tmp, tmpLen, tmpSet, s5, s5Len,
                                                 &hasErr, outLen)) {
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
    return static_cast<int32_t>(result);
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
    return static_cast<int32_t>(result);
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

extern "C" DLLEXPORT int32_t CharLengthStr(const char *str, int32_t strLen, bool isNull)
{
    return isNull ? 0 : omniruntime::Utf8Util::CountCodePoints(str, strLen);
}

extern "C" DLLEXPORT int32_t CharLengthChar(const char *str, int32_t strWidth, int32_t strLen, bool isNull)
{
    return isNull ? 0 : strWidth;
}

extern "C" DLLEXPORT bool IsNullStr(const char *str, int32_t strLen, bool isNull, bool retIsNull)
{
    (void)str;
    (void)strLen;
    (void)retIsNull;
    return isNull;
}

extern "C" DLLEXPORT bool IsNullChar(const char *str, int32_t strWidth, int32_t strLen, bool isNull, bool retIsNull)
{
    (void)str;
    (void)strWidth;
    (void)strLen;
    (void)retIsNull;
    return isNull;
}

extern "C" DLLEXPORT const char *ReplaceStrStrStrWithRepNotReplace(int64_t contextPtr, const char *str, int32_t strLen,
                                                                   const char *searchStr, int32_t searchLen,
                                                                   const char *replaceStr, int32_t replaceLen,
                                                                   bool isNull, int32_t *outLen)
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
                                                                const char *searchStr, int32_t searchLen,
                                                                const char *replaceStr, int32_t replaceLen, bool isNull,
                                                                int32_t *outLen)
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
                                                                   const char *searchStr, int32_t searchLen,
                                                                   bool isNull, int32_t *outLen)
{
    if (isNull) {
        return nullptr;
    }
    return ReplaceStrStrStrWithRepNotReplace(contextPtr, str, strLen, searchStr, searchLen, "", 0, isNull, outLen);
}

extern "C" DLLEXPORT const char *ReplaceStrStrWithoutRepReplace(int64_t contextPtr, const char *str, int32_t strLen,
                                                                const char *searchStr, int32_t searchLen, bool isNull,
                                                                int32_t *outLen)
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
    errno_t res = memcpy_s(ret, *outLen, str.c_str(), *outLen);
    if (res != EOK) {
        SetError(contextPtr, "cast failed");
        *outLen = 0;
        return nullptr;
    }
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
    std::string str = std::to_string(value);
    *outLen = static_cast<int32_t>(strlen(str.c_str()));
    auto ret = ArenaAllocatorMalloc(contextPtr, *outLen);
    errno_t res = memcpy_s(ret, *outLen, str.c_str(), *outLen);
    if (res != EOK) {
        SetError(contextPtr, "cast failed");
        *outLen = 0;
        return nullptr;
    }
    return ret;
}

extern "C" DLLEXPORT const char *CastDoubleToString(int64_t contextPtr, double value, bool isNull, int32_t *outLen)
{
    if (isNull) {
        return nullptr;
    }
    auto ret = ArenaAllocatorMalloc(contextPtr, MAX_DATA_LENGTH);
    *outLen = static_cast<int32_t>(DoubleToString::DoubleToStringConverter(value, ret));
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
    errno_t res = memcpy_s(ret, *outLen, str.c_str(), *outLen);
    if (res != EOK) {
        SetError(contextPtr, "cast failed");
        *outLen = 0;
        return nullptr;
    }
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
    errno_t res = memcpy_s(ret, *outLen, stringDecimal.c_str(), *outLen);
    if (res != EOK) {
        SetError(contextPtr, "cast failed");
        *outLen = 0;
        return nullptr;
    }
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
    Status status = ConvertDateStringToInteger(result, str, strLen);
    if (status == Status::CONVERT_SUCCESS) {
        return result;
    }

    if (status == Status::CONVERT_OVERFLOW) {
        std::string s(str, strLen);
        std::ostringstream errorMessage;
        errorMessage << "Cannot cast '" << s << "' to BIGINT. Date year out of range (0-9999).";
        SetError(contextPtr, errorMessage.str());
        return 0;
    }

    status = ConvertStringToInteger<int64_t, false>(result, str, strLen);
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
        errorMessage << "Cannot cast VARCHAR '" << s << "' to DECIMAL(" << outPrecision << ", " << outScale
                     << "). Value is not a number.";
        SetError(contextPtr, errorMessage.str());
        return 0;
    }
    Decimal64 result(s);
    result.ReScale(outScale);
    if (result.IsOverflow(outPrecision) != OpStatus::SUCCESS) {
        std::ostringstream errorMessage;
        errorMessage << "Cannot cast VARCHAR '" << std::string(str, strLen) << "' to DECIMAL(" << outPrecision << ", "
                     << outScale << "). Value too large.";
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
        errorMessage << "Cannot cast VARCHAR '" << std::string(str, strLen) << "' to DECIMAL(" << outPrecision << ", "
                     << outScale << "). Value too large.";
        SetError(contextPtr, errorMessage.str());
        return 0;
    }
    if (result.IsOverflow(outPrecision) == OpStatus::FAIL) {
        std::ostringstream errorMessage;
        errorMessage << "Cannot cast VARCHAR '" << s << "' to DECIMAL(" << outPrecision << ", " << outScale
                     << "). Value is not a number.";
        SetError(contextPtr, errorMessage.str());
        return 0;
    }
    return result.GetValue();
}

extern "C" DLLEXPORT void CastStringToDecimal128(int64_t contextPtr, const char *str, int32_t strLen, bool isNull,
                                                 int32_t outPrecision, int32_t outScale, int64_t *outHighPtr,
                                                 uint64_t *outLowPtr)
{
    if (isNull) {
        return;
    }
    std::string s = std::string(str, strLen);
    StringUtil::TrimString(s);
    if (!regex_match(s, g_decimalRegex)) {
        std::ostringstream errorMessage;
        errorMessage << "Cannot cast VARCHAR '" << s << "' to DECIMAL(" << outPrecision << ", " << outScale
                     << "). Value is not a number.";
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
                                                        bool isNull, int32_t outPrecision, int32_t outScale,
                                                        int64_t *outHighPtr, uint64_t *outLowPtr)
{
    if (isNull) {
        return;
    }
    std::string s = std::string(str, strLen);
    StringUtil::TrimString(s);
    if (!regex_match(s, g_decimalRegex)) {
        std::ostringstream errorMessage;
        errorMessage << "Cannot cast VARCHAR '" << s << "' to DECIMAL(" << outPrecision << ", " << outScale
                     << "). Value is not a number.";
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
                                                       int32_t apLen, const char *bp, int32_t bWidth, int32_t bpLen,
                                                       int32_t *outLen)
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
    return static_cast<int32_t>(result);
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
    return static_cast<int32_t>(result);
}

extern "C" DLLEXPORT const char *CastIntToStringRetNull(int64_t contextPtr, bool *isNull, int32_t value,
                                                        int32_t *outLen)
{
    std::string str = std::to_string(value);
    *outLen = static_cast<int32_t>(str.size());
    auto ret = ArenaAllocatorMalloc(contextPtr, *outLen);
    errno_t res = memcpy_s(ret, *outLen, str.c_str(), *outLen);
    if (res != EOK) {
        *isNull = true;
        *outLen = 0;
        return nullptr;
    }
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
    std::string str = std::to_string(value);
    *outLen = static_cast<int32_t>(strlen(str.c_str()));
    auto ret = ArenaAllocatorMalloc(contextPtr, *outLen);
    errno_t res = memcpy_s(ret, *outLen, str.c_str(), *outLen);
    if (res != EOK) {
        *isNull = true;
        *outLen = 0;
        return nullptr;
    }
    return ret;
}

extern "C" DLLEXPORT const char *CastDoubleToStringRetNull(int64_t contextPtr, bool *isNull, double value,
                                                           int32_t *outLen)
{
    auto ret = ArenaAllocatorMalloc(contextPtr, MAX_DATA_LENGTH);
    *outLen = static_cast<int32_t>(DoubleToString::DoubleToStringConverter(value, ret));
    return ret;
}

extern "C" DLLEXPORT const char *CastDecimal64ToStringRetNull(int64_t contextPtr, bool *isNull, int64_t x,
                                                              int32_t precision, int32_t scale, int32_t *outLen)
{
    std::string str = Decimal64(x).SetScale(scale).ToString();
    *outLen = static_cast<int32_t>(str.size());
    auto ret = ArenaAllocatorMalloc(contextPtr, *outLen);
    errno_t res = memcpy_s(ret, *outLen, str.c_str(), *outLen);
    if (res != EOK) {
        *isNull = true;
        *outLen = 0;
        return nullptr;
    }
    return ret;
}

extern "C" DLLEXPORT const char *CastDecimal128ToStringRetNull(int64_t contextPtr, bool *isNull, int64_t high,
                                                               uint64_t low, int32_t precision, int32_t scale,
                                                               int32_t *outLen)
{
    Decimal128Wrapper inputDecimal(high, low);
    std::string stringDecimal = inputDecimal.SetScale(scale).ToString();
    *outLen = static_cast<int32_t>(stringDecimal.length());
    auto ret = ArenaAllocatorMalloc(contextPtr, *outLen);
    errno_t res = memcpy_s(ret, *outLen, stringDecimal.c_str(), *outLen);
    if (res != EOK) {
        *isNull = true;
        *outLen = 0;
        return nullptr;
    }
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
                                                        int32_t outPrecision, int32_t outScale, int64_t *outHighPtr,
                                                        uint64_t *outLowPtr)
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
                                                               int32_t outPrecision, int32_t outScale,
                                                               int64_t *outHighPtr, uint64_t *outLowPtr)
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
                                                              int32_t srcLen, int32_t srcWidth, int32_t dstWidth,
                                                              int32_t *outLen)
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

inline const char *ExtremeStr(const char *lValue, int32_t lLen, bool lIsNull, const char *rValue, int32_t rLen,
                              bool rIsNull, bool *retIsNull, int32_t *outLen, bool pickGreater)
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
    if (rIsNull) {
        *outLen = lLen;
        return lValue;
    }
    int32_t cmpRet = memcmp(lValue, rValue, std::min(lLen, rLen));
    bool pickRight = cmpRet == 0 ? (pickGreater ? rLen > lLen : rLen < lLen) : (pickGreater ? cmpRet < 0 : cmpRet > 0);
    *outLen = pickRight ? rLen : lLen;
    return pickRight ? rValue : lValue;
}

extern "C" DLLEXPORT const char *GreatestStr(const char *lValue, int32_t lLen, bool lIsNull, const char *rValue,
                                             int32_t rLen, bool rIsNull, bool *retIsNull, int32_t *outLen)
{
    return ExtremeStr(lValue, lLen, lIsNull, rValue, rLen, rIsNull, retIsNull, outLen, true);
}

extern "C" DLLEXPORT const char *LeastStr(const char *lValue, int32_t lLen, bool lIsNull, const char *rValue,
                                          int32_t rLen, bool rIsNull, bool *retIsNull, int32_t *outLen)
{
    return ExtremeStr(lValue, lLen, lIsNull, rValue, rLen, rIsNull, retIsNull, outLen, false);
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
    errno_t res = memcpy_s(padded, outByteNum, str, outByteNum);
    if (res != EOK) {
        SetError(contextPtr, "varcharTypeWriteSideCheck failed：memcpy_s error");
        *outLen = 0;
        return nullptr;
    }
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
        errno_t res = memcpy_s(resStr, len, str, len);
        errno_t res1 = memset_s(resStr + len, numTailSpacesToAdd + 1, ' ', numTailSpacesToAdd);
        if (res != EOK || res1 != EOK) {
            SetError(contextPtr, "charTypeWriteSideCheck failed：memcpy_s error");
            *outLen = 0;
            return nullptr;
        }
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
    errno_t res = memcpy_s(padded, outByteNum, str, outByteNum);
    if (res != EOK) {
        SetError(contextPtr, "varcharTypeWriteSideCheck failed：memcpy_s error");
        *outLen = 0;
        return nullptr;
    }
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
        errno_t res = memcpy_s(padded, len, str, len);
        if (res != EOK) {
            SetError(contextPtr, "charReadPadding failed：memcpy_s error");
            *outLen = 0;
            return nullptr;
        }
    }
    errno_t res = memset_s(padded + len, diff, ' ', diff);
    if (res != EOK) {
        SetError(contextPtr, "charReadPadding failed：memset_s error");
        *outLen = 0;
        return nullptr;
    }
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
        errno_t res = memcpy_s(result, strLen, str, strLen);
        if (res != EOK) {
            SetError(contextPtr, "charReadPadding failed：memcpy_s error");
            *outLen = 0;
            return nullptr;
        }
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
    errno_t res = memcpy_s(result, length, str + start, length);
    if (res != EOK) {
        SetError(contextPtr, "charReadPadding failed：memcpy_s error");
        *outLen = 0;
        return nullptr;
    }
    *outLen = length;
    return result;
}
} // namespace omniruntime::codegen::function
