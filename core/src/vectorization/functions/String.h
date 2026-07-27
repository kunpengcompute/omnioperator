/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: visitor class for expressions
 */

#pragma once
#include "util/compiler_util.h"
#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include "vectorization/Status.h"
#include "vectorization/functions/Base64Util.h"
#include "type/string_Impl.h"
#include "folly/ssl/OpenSSLHash.h"
#include "codegen/functions/md5.h"
#include "codegen/context_helper.h"
#include "codegen/string_util.h"
#include "util/type_util.h"

namespace omniruntime::vectorization {
/// Returns the Unicode code point of the first character in UTF-8 string (aligned with
/// Velox utf8proc_codepoint). Returns 0 if empty, -1 on invalid/incomplete UTF-8.
inline int32_t Utf8FirstCodepoint(const char* data, size_t size, int& byteLen) {
    if (size == 0) {
        byteLen = 0;
        return 0;
    }
    const unsigned char* u = reinterpret_cast<const unsigned char*>(data);
    unsigned char u0 = u[0];
    if (u0 <= 127) {
        byteLen = 1;
        return u0;
    }
    if (size < 2) {
        byteLen = 1;
        return -1;
    }
    unsigned char u1 = u[1];
    if (u0 >= 192 && u0 <= 223) {
        // 0xC0/0xC1 are always invalid lead bytes (overlong encoding of <= 0x7F).
        // Continuation byte must be 10xxxxxx (0x80..0xBF).
        if (u0 < 0xC2 || (u1 & 0xC0) != 0x80) {
            byteLen = 1;
            return -1;
        }
        byteLen = 2;
        return (u0 - 192) * 64 + (u1 - 128);
    }
    if (u0 == 0xed && (u1 & 0xa0) == 0xa0) {
        byteLen = 1;
        return -1;  // surrogate U+D800..U+DFFF invalid in UTF-8
    }
    if (size < 3) {
        byteLen = 1;
        return -1;
    }
    unsigned char u2 = u[2];
    if (u0 >= 224 && u0 <= 239) {
        // Continuation bytes must be 10xxxxxx; reject overlong 3-byte (< U+0800).
        if ((u1 & 0xC0) != 0x80 || (u2 & 0xC0) != 0x80 ||
            (u0 == 0xe0 && u1 < 0xa0)) {
            byteLen = 1;
            return -1;
        }
        byteLen = 3;
        return (u0 - 224) * 4096 + (u1 - 128) * 64 + (u2 - 128);
    }
    if (size < 4) {
        byteLen = 1;
        return -1;
    }
    unsigned char u3 = u[3];
    if (u0 >= 240 && u0 <= 247) {
        // Continuation bytes must be 10xxxxxx; reject overlong (0xf0,u1<0x90)
        // and out-of-range > U+10FFFF (0xf4,u1>0x8f).
        if ((u1 & 0xC0) != 0x80 || (u2 & 0xC0) != 0x80 || (u3 & 0xC0) != 0x80 ||
            (u0 == 0xf0 && u1 < 0x90) || (u0 == 0xf4 && u1 > 0x8f)) {
            byteLen = 1;
            return -1;
        }
        byteLen = 4;
        return (u0 - 240) * 262144 + (u1 - 128) * 4096 + (u2 - 128) * 64 + (u3 - 128);
    }
    byteLen = 1;
    return -1;  // invalid lead byte
}

static ALWAYS_INLINE bool IsAsciiString(const std::string_view& input)
{
    return std::all_of(input.begin(), input.end(), [](char c) {
        return (static_cast<unsigned char>(c) & 0x80) == 0;
    });
}

static ALWAYS_INLINE void AppendUtf8Codepoint(std::string& result, int32_t codePoint)
{
    if (codePoint <= 0x7F) {
        result.push_back(static_cast<char>(codePoint));
    } else if (codePoint <= 0x7FF) {
        result.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
        result.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else if (codePoint <= 0xFFFF) {
        result.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
        result.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        result.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else {
        result.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
        result.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
        result.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        result.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    }
}

static ALWAYS_INLINE size_t PreviousUtf8Start(const std::string_view& input, size_t pos)
{
    size_t prev = pos - 1;
    while (prev > 0 && (static_cast<unsigned char>(input[prev]) & 0xC0) == 0x80) {
        --prev;
    }
    return prev;
}

static ALWAYS_INLINE bool IsCasedCodepoint(int32_t codePoint)
{
    if ((codePoint >= 'A' && codePoint <= 'Z') || (codePoint >= 'a' && codePoint <= 'z')) {
        return true;
    }
    if ((codePoint >= 0x00C0 && codePoint <= 0x02AF) || (codePoint >= 0x0370 && codePoint <= 0x03FF) ||
        (codePoint >= 0x0400 && codePoint <= 0x052F) || (codePoint >= 0x1D2C && codePoint <= 0x1D61) ||
        (codePoint >= 0x2160 && codePoint <= 0x217F) || (codePoint >= 0x24B6 && codePoint <= 0x24E9)) {
        return true;
    }
    return codePoint == 0x0345;
}

static ALWAYS_INLINE bool IsCaseIgnorableCodepoint(int32_t codePoint)
{
    if ((codePoint >= 0x0300 && codePoint <= 0x036F) || (codePoint >= 0xFE00 && codePoint <= 0xFE0F)) {
        return true;
    }
    switch (codePoint) {
    case 0x0027:
    case 0x002E:
    case 0x00B7:
    case 0x0387:
    case 0x05F4:
    case 0x2018:
    case 0x2019:
    case 0x2024:
    case 0x2027:
    case 0xFE13:
    case 0xFE52:
    case 0xFE55:
    case 0xFF07:
    case 0xFF0E:
    case 0xFF1A:
        return true;
    default:
        return false;
    }
}

static ALWAYS_INLINE bool HasCasedBefore(const std::string_view& input, size_t sigmaStart)
{
    size_t pos = sigmaStart;
    while (pos > 0) {
        size_t prev = PreviousUtf8Start(input, pos);
        int byteLen = 0;
        int32_t cp = Utf8FirstCodepoint(input.data() + prev, pos - prev, byteLen);
        if (cp < 0) {
            return false;
        }
        if (IsCasedCodepoint(cp)) {
            return true;
        }
        if (!IsCaseIgnorableCodepoint(cp)) {
            return false;
        }
        pos = prev;
    }
    return false;
}

static ALWAYS_INLINE bool HasCasedAfter(const std::string_view& input, size_t pos)
{
    while (pos < input.size()) {
        int byteLen = 0;
        int32_t cp = Utf8FirstCodepoint(input.data() + pos, input.size() - pos, byteLen);
        if (cp < 0 || byteLen <= 0) {
            return false;
        }
        if (IsCasedCodepoint(cp)) {
            return true;
        }
        if (!IsCaseIgnorableCodepoint(cp)) {
            return false;
        }
        pos += static_cast<size_t>(byteLen);
    }
    return false;
}

static ALWAYS_INLINE bool IsFinalGreekSigma(const std::string_view& input, size_t sigmaStart, size_t afterSigma)
{
    return HasCasedBefore(input, sigmaStart) && !HasCasedAfter(input, afterSigma);
}

static ALWAYS_INLINE int32_t LowerUnicodeCodepoint(int32_t codePoint)
{
    if (codePoint >= 'A' && codePoint <= 'Z') {
        return codePoint + ('a' - 'A');
    }
    if ((codePoint >= 0x00C0 && codePoint <= 0x00D6) || (codePoint >= 0x00D8 && codePoint <= 0x00DE)) {
        return codePoint + 0x20;
    }
    switch (codePoint) {
    case 0x010A:
    case 0x0116:
    case 0x0120:
    case 0x017B:
    case 0x0226:
    case 0x022E:
    case 0x1E02:
    case 0x1E0A:
    case 0x1E1E:
    case 0x1E22:
    case 0x1E40:
    case 0x1E44:
    case 0x1E56:
    case 0x1E58:
    case 0x1E60:
    case 0x1E64:
    case 0x1E66:
    case 0x1E68:
    case 0x1E6A:
    case 0x1E86:
    case 0x1E8A:
    case 0x1E8E:
        return codePoint + 1;
    case 0x0386:
        return 0x03AC;
    case 0x0388:
        return 0x03AD;
    case 0x0389:
        return 0x03AE;
    case 0x038A:
        return 0x03AF;
    case 0x038C:
        return 0x03CC;
    case 0x038E:
        return 0x03CD;
    case 0x038F:
        return 0x03CE;
    case 0x0401:
        return 0x0451;
    case 0x01C5:
        return 0x01C6;
    default:
        break;
    }
    if ((codePoint >= 0x0391 && codePoint <= 0x03A1) || (codePoint >= 0x03A3 && codePoint <= 0x03AB) ||
        (codePoint >= 0x0410 && codePoint <= 0x042F)) {
        return codePoint + 0x20;
    }
    return codePoint;
}

static ALWAYS_INLINE int32_t UpperUnicodeCodepoint(int32_t codePoint)
{
    if (codePoint >= 'a' && codePoint <= 'z') {
        return codePoint - ('a' - 'A');
    }
    if ((codePoint >= 0x00E0 && codePoint <= 0x00F6) || (codePoint >= 0x00F8 && codePoint <= 0x00FE)) {
        return codePoint - 0x20;
    }
    switch (codePoint) {
    case 0x010B:
        return 0x010A;
    case 0x0117:
        return 0x0116;
    case 0x0121:
        return 0x0120;
    case 0x017C:
        return 0x017B;
    case 0x0227:
        return 0x0226;
    case 0x022F:
        return 0x022E;
    case 0x1E03:
        return 0x1E02;
    case 0x1E0B:
        return 0x1E0A;
    case 0x1E1F:
        return 0x1E1E;
    case 0x1E23:
        return 0x1E22;
    case 0x1E41:
        return 0x1E40;
    case 0x1E45:
        return 0x1E44;
    case 0x1E57:
        return 0x1E56;
    case 0x1E59:
        return 0x1E58;
    case 0x1E61:
        return 0x1E60;
    case 0x1E65:
        return 0x1E64;
    case 0x1E67:
        return 0x1E66;
    case 0x1E69:
        return 0x1E68;
    case 0x1E6B:
        return 0x1E6A;
    case 0x1E87:
        return 0x1E86;
    case 0x1E8B:
        return 0x1E8A;
    case 0x1E8F:
        return 0x1E8E;
    case 0x03AC:
        return 0x0386;
    case 0x03AD:
        return 0x0388;
    case 0x03AE:
        return 0x0389;
    case 0x03AF:
        return 0x038A;
    case 0x03CC:
        return 0x038C;
    case 0x03CD:
        return 0x038E;
    case 0x03CE:
        return 0x038F;
    case 0x0451:
        return 0x0401;
    case 0x01C6:
        return 0x01C5;
    default:
        break;
    }
    if (codePoint >= 0x0430 && codePoint <= 0x044F) {
        return codePoint - 0x20;
    }
    if (codePoint >= 0x03B1 && codePoint <= 0x03C1) {
        return codePoint - 0x20;
    }
    if (codePoint >= 0x03C4 && codePoint <= 0x03C9) {
        return codePoint - 0x20;
    }
    return codePoint;
}

template <typename T>
struct StartsWithFunction {
    ALWAYS_INLINE Status call(bool &result, const std::string_view &str, const std::string_view &pattern)
    {
        if (pattern.length() > str.length()) {
            result = false;
            return Status::OK();
        }

        if (pattern.empty()) {
            result = true;
            return Status::OK();
        }

        result = std::equal(pattern.begin(), pattern.end(), str.begin());
        return Status::OK();
    }
};

/// endsWith function
/// endsWith(string, string) -> bool
/// Returns true if the first string ends with the second string
template <typename T>
struct EndsWithFunction {
    ALWAYS_INLINE Status call(bool &result, const std::string_view &str, const std::string_view &pattern)
    {
        if (pattern.length() > str.length()) {
            result = false;
            return Status::OK();
        }
        if (pattern.empty()) {
            result = true;
            return Status::OK();
        }
        result = std::equal(pattern.rbegin(), pattern.rend(), str.rbegin());
        return Status::OK();
    }
};

/// contains function
/// contains(string, string) -> bool
/// Searches the second argument in the first one.
/// Returns true if it is found
template <typename T>
struct ContainsFunction {
    ALWAYS_INLINE bool call(bool &result, const std::string_view &str, const std::string_view &pattern)
    {
        result = std::string_view(str).find(std::string_view(pattern)) != std::string_view::npos;
        return true;
    }
};

/// trim function
/// trim(string) -> string
/// Removes leading and trailing whitespace characters from the input string.
/// Whitespace characters include space, tab, newline, carriage return, etc.
template <typename T>
struct TrimFunction {
    ALWAYS_INLINE bool callNullable(std::string &result, const std::string_view *str)
    {
        if (str == nullptr) {
            return false;
        }
        // Find the first non-whitespace character from the beginning
        auto start = str->find_first_not_of(" ");
        if (start == std::string_view::npos) {
            // All characters are whitespace
            result.clear();
            return true;
        }
        // Find the last non-whitespace character from the end
        auto end = str->find_last_not_of(" ");
        // Extract the trimmed substring
        result = std::string(str->substr(start, end - start + 1));
        return true;
    }
};

template <typename T>
struct LTrimFunction {
    ALWAYS_INLINE bool callNullable(std::string &result, const std::string_view *str)
    {
        if (str == nullptr) {
            return false;
        }
        auto start = str->find_first_not_of(" ");
        if (start == std::string_view::npos) {
            result.clear();
            return true;
        }
        result = std::string(str->substr(start));
        return true;
    }
};

template <typename T>
struct RTrimFunction {
    ALWAYS_INLINE bool callNullable(std::string &result, const std::string_view *str)
    {
        if (str == nullptr) {
            return false;
        }
        auto end = str->find_last_not_of(" ");
        if (end == std::string_view::npos) {
            result.clear();
            return true;
        }
        result.assign(str->data(), end + 1);
        return true;
    }
};

/// trim(trimStr, string) -> string
/// Removes leading and trailing characters that appear in trimStr from the input string str.
/// - trimStr: set of characters to trim (each occurrence at head/tail is removed).
/// - str: the string to trim.
/// Returns false (result NULL) if trimStr or str is nullptr.
/// If trimStr is empty, the full str is returned (no characters are trimmed).
/// If every character of str is in trimStr, result is empty.
template <typename T>
struct TrimWithCharsFunction {
    ALWAYS_INLINE bool callNullable(std::string &result, const std::string_view *trimStr, const std::string_view *str)
    {
        if (str == nullptr || trimStr == nullptr) {
            return false;
        }
        // Find the first character not in trimStr from the beginning
        auto start = str->find_first_not_of(*trimStr);
        if (start == std::string_view::npos) {
            // Every character is in trimStr -> result empty
            result.clear();
            return true;
        }
        // Find the last character not in trimStr from the end
        auto end = str->find_last_not_of(*trimStr);
        // Extract the trimmed substring
        result = std::string(str->substr(start, end - start + 1));
        return true;
    }
};

template <typename T>
struct LTrimWithCharsFunction {
    ALWAYS_INLINE bool callNullable(std::string &result, const std::string_view *trimStr, const std::string_view *str)
    {
        if (str == nullptr || trimStr == nullptr) {
            return false;
        }
        auto start = str->find_first_not_of(*trimStr);
        if (start == std::string_view::npos) {
            result.clear();
            return true;
        }
        result = std::string(str->substr(start));
        return true;
    }
};

template <typename T>
struct RTrimWithCharsFunction {
    ALWAYS_INLINE bool callNullable(std::string &result, const std::string_view *trimStr, const std::string_view *str)
    {
        if (str == nullptr || trimStr == nullptr) {
            return false;
        }
        auto end = str->find_last_not_of(*trimStr);
        if (end == std::string_view::npos) {
            result.clear();
            return true;
        }
        result.assign(str->data(), end + 1);
        return true;
    }
};

/// locate function
/// locate(substring, string, start) -> integer
/// Returns the position of the first occurrence of substring in string starting from position start.
/// Returns 1-based position (first character is at position 1).
/// Returns 0 if substring is not found, start < 1, or start > string length.
/// Returns 1 if substring is empty.
/// Note: Under SimpleFunction null propagation, any NULL argument (substring/string/start) yields
/// result NULL; Spark's "start NULL -> 0" semantics are not supported by the current framework.
template <typename T>
struct LocateFunction {
    // Non-nullable version for better performance when all arguments are non-null
    ALWAYS_INLINE bool call(int32_t &result, const std::string_view &subString,
        const std::string_view &string, const int32_t &start)
    {
        if (start < 1) {
            result = 0;
            return true;
        }
        if (subString.empty()) {
            result = 1;
            return true;
        }
        
        // Calculate string length in characters (Unicode-aware)
        int64_t stringLength = stringImpl::length<false /*isAscii*/>(string);
        if (start > static_cast<int32_t>(stringLength)) {
            result = 0;
            return true;
        }

        // Find the start byte index of the start character for Unicode strings
        int64_t startByteIndex = stringImpl::cappedByteLengthUnicode(
            string.data(), string.size(), start - 1);

        // Search from start position
        std::string_view searchString(string.data() + startByteIndex, string.size() - startByteIndex);
        auto position = stringImpl::StringPosition<false /*isAscii*/, true /*lpos*/>(
            searchString, subString, 1 /*instance*/);
        if (position > 0) {
            result = position + start - 1;
        } else {
            result = 0;
        }
        return true;
    }

    // Nullable version supporting both ASCII and Unicode
    ALWAYS_INLINE bool callNullable(int32_t &result, const std::string_view *subString,
        const std::string_view *string, const int32_t *start)
    {
        // Call the non-nullable version for better code reuse
        return call(result, *subString, *string, *start);
    }
};

/// substr(string, start) -> varchar
/// substr(string, start, length) -> varchar
/// Spark semantics (velox/functions/sparksql/String.h). Spark + Gluten: int32_t only.
/// start=0 -> first char; start<=0 after adjustment -> start=1; length<=0 -> empty.
/// Note: SimpleFunction passes args as (last_arg, ..., first_arg); call signatures match that order.
template <typename T>
struct SubstrFunction {
    /// 2-arg: framework passes (start, string)
    ALWAYS_INLINE bool call(std::string &result, const std::string_view &input, int32_t start)
    {
        return doCall(result, input, start, std::numeric_limits<int32_t>::max());
    }

    /// 3-arg: framework passes (length, start, string)
    ALWAYS_INLINE bool call(std::string &result, const std::string_view &input, int32_t start, int32_t length)
    {
        return doCall(result, input, start, length);
    }

private:
    ALWAYS_INLINE bool doCall(std::string &result, const std::string_view &input, int32_t start, int32_t length)
    {
        if (length <= 0) {
            result.clear();
            return true;
        }
        if (start == 0) {
            start = 1;
        }
        int32_t numCharacters = static_cast<int32_t>(std::min(
            stringImpl::length<false>(input), static_cast<int64_t>(std::numeric_limits<int32_t>::max())));
        if (start < 0) {
            start = numCharacters + start + 1;
        }
        int32_t last;
        if (numCharacters - start + 1 < length) {
            last = numCharacters;
        } else {
            last = start + length - 1;
        }
        if (start <= 0) {
            start = 1;
        }
        length = last - start + 1;
        if (length <= 0) {
            result.clear();
            return true;
        }
        size_t startByte = static_cast<size_t>(stringImpl::cappedByteLengthUnicode(
            input.data(), static_cast<int64_t>(input.size()), static_cast<int64_t>(start - 1)));
        size_t segmentByteLen = static_cast<size_t>(stringImpl::cappedByteLengthUnicode(
            input.data() + startByte, static_cast<int64_t>(input.size()) - static_cast<int64_t>(startByte),
            static_cast<int64_t>(length)));
        result.assign(input.data() + startByte, segmentByteLen);
        return true;
    }
};

/// left(string, length) -> varchar
/// Returns the leftmost `length` characters of `string`. Unicode-aware (counts
/// characters, not bytes). length <= 0 -> empty string; length >= character
/// count -> the whole string. NULL arguments propagate to NULL via the
/// SimpleFunction framework (uses call, not callNullable). Semantically
/// equivalent to substr(string, 1, length).
template <typename T>
struct LeftFunction {
    ALWAYS_INLINE bool call(std::string &result, const std::string_view &input, int32_t length)
    {
        return doCall(result, input, static_cast<int64_t>(length));
    }

    ALWAYS_INLINE bool call(std::string &result, const std::string_view &input, int64_t length)
    {
        return doCall(result, input, length);
    }

private:
    ALWAYS_INLINE bool doCall(std::string &result, const std::string_view &input, int64_t length)
    {
        if (length <= 0) {
            result.clear();
            return true;
        }
        // cappedByteLengthUnicode walks up to `length` chars (or until end of input),
        // returning the byte offset; clamps length > charCount to the whole string.
        int64_t byteLen = stringImpl::cappedByteLengthUnicode(
            input.data(), static_cast<int64_t>(input.size()), length);
        result.assign(input.data(), static_cast<size_t>(byteLen));
        return true;
    }
};

/// right(string, length) -> varchar
/// Returns the rightmost `length` characters of `string`. Unicode-aware.
/// length <= 0 -> empty string; length >= character count -> the whole string.
/// NULL arguments propagate to NULL via the SimpleFunction framework.
/// Semantically equivalent to substr(string, -length, length).
template <typename T>
struct RightFunction {
    ALWAYS_INLINE bool call(std::string &result, const std::string_view &input, int32_t length)
    {
        return doCall(result, input, static_cast<int64_t>(length));
    }

    ALWAYS_INLINE bool call(std::string &result, const std::string_view &input, int64_t length)
    {
        return doCall(result, input, length);
    }

private:
    ALWAYS_INLINE bool doCall(std::string &result, const std::string_view &input, int64_t length)
    {
        if (length <= 0) {
            result.clear();
            return true;
        }
        int64_t numCharacters = stringImpl::length<false>(input);
        int64_t take = (length >= numCharacters) ? numCharacters : length;
        int64_t skipChars = numCharacters - take;
        int64_t startByte = stringImpl::cappedByteLengthUnicode(
            input.data(), static_cast<int64_t>(input.size()), skipChars);
        size_t startByteOff = static_cast<size_t>(startByte);
        result.assign(input.data() + startByteOff, input.size() - startByteOff);
        return true;
    }
};

/// position function
/// position(substring, string) -> integer
/// Returns the 1-based position of the first occurrence of substring in string.
/// Equivalent to locate(substring, string, 1). Returns 0 if not found, 1 if substring is empty.
/// Under SimpleFunction null propagation, any NULL argument yields result NULL.
template <typename T>
struct PositionFunction {
    ALWAYS_INLINE bool call(int32_t &result, const std::string_view &subString, const std::string_view &string)
    {
        auto pos = stringImpl::StringPosition<false /*isAscii*/, true /*lpos*/>(string, subString, 1 /*instance*/);
        result = static_cast<int32_t>(pos);
        return true;
    }

    ALWAYS_INLINE bool callNullable(int32_t &result, const std::string_view *subString, const std::string_view *string)
    {
        return call(result, *subString, *string);
    }
};

/// char_length / character_length / length function
/// length(string) -> integer
/// Returns the number of characters in the input string (Unicode code points).
/// Empty string returns 0. Supports CHAR and VARCHAR. NULL input yields NULL output.
template <typename T>
struct CharLengthFunction {
    ALWAYS_INLINE bool call(int32_t &result, const std::string_view &str)
    {
        result = static_cast<int32_t>(stringImpl::length<false /*isAscii*/>(str));
        return true;
    }

    ALWAYS_INLINE bool callNullable(int32_t &result, const std::string_view *str)
    {
        return call(result, *str);
    }
};

/// ascii(string) -> int32
/// Returns the ASCII/Unicode code point of the first character; 0 for empty string.
template <typename T>
struct AsciiFunction {
    ALWAYS_INLINE void call(int32_t& result, const std::string_view& s) {
        if (s.empty()) {
            result = 0;
            return;
        }
        int byteLen = 0;
        result = Utf8FirstCodepoint(s.data(), s.size(), byteLen);
    }
};

/// chr(n) / char(n) -> string
/// Returns the Unicode code point n as a single character string.
/// If n < 0, returns empty string. If n >= 256, equivalent to chr(n % 256).
/// Supports all integer types (byte/short/int/long).
template <typename T>
struct ChrFunction {
    ALWAYS_INLINE void call(std::string& result, const int64_t& n) {
        if (n < 0) {
            result.clear();
            return;
        }
        int64_t codePoint = n & 0xFF;
        result.resize(codePoint < 0x80 ? 1 : 2);
        char* p = &result[0];
        if (codePoint < 0x80) {
            p[0] = static_cast<char>(codePoint);
        } else {
            p[0] = static_cast<char>(0xC0 + (codePoint >> 6));
            p[1] = static_cast<char>(0x80 + (codePoint & 0x3F));
        }
    }
};

/// lower function
/// lower(string) -> string
/// Converts the input string to lowercase. Keeps an ASCII fast path and handles
/// Spark/Velox UTF-8 cases required by lower, including Greek pi and final sigma.
/// Empty string returns empty string. NULL input yields NULL output.
template <typename T>
struct LowerFunction {
    ALWAYS_INLINE bool call(std::string& result, const std::string_view& input)
    {
        if (IsAsciiString(input)) {
            result.resize(input.size());
            for (size_t i = 0; i < input.size(); ++i) {
                unsigned char c = static_cast<unsigned char>(input[i]);
                result[i] = (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : input[i];
            }
            return true;
        }

        result.clear();
        result.reserve(input.size() * 2);
        size_t pos = 0;
        while (pos < input.size()) {
            int byteLen = 0;
            int32_t codePoint = Utf8FirstCodepoint(input.data() + pos, input.size() - pos, byteLen);
            if (codePoint < 0 || byteLen <= 0) {
                result.append(input.data() + pos, input.size() - pos);
                return true;
            }

            size_t nextPos = pos + static_cast<size_t>(byteLen);
            if (codePoint == 0x0130) {
                result.append("i\xCC\x87", 3);
            } else if (codePoint == 0x03A3) {
                AppendUtf8Codepoint(result, IsFinalGreekSigma(input, pos, nextPos) ? 0x03C2 : 0x03C3);
            } else {
                AppendUtf8Codepoint(result, LowerUnicodeCodepoint(codePoint));
            }
            pos = nextPos;
        }
        return true;
    }

    ALWAYS_INLINE bool callNullable(std::string& result, const std::string_view* input)
    {
        if (input == nullptr) {
            return false;
        }
        return call(result, *input);
    }
};

/// upper function
/// upper(string) -> string
/// Converts the input string to uppercase. ASCII fast path; UTF-8 mirrors Spark/Velox upper,
/// including Greek sigma/final-sigma to Σ and Latin dotted specials.
/// Empty string returns empty string. NULL input yields NULL output.
template <typename T>
struct UpperFunction {
    ALWAYS_INLINE bool call(std::string& result, const std::string_view& input)
    {
        if (IsAsciiString(input)) {
            result.resize(input.size());
            for (size_t i = 0; i < input.size(); ++i) {
                unsigned char c = static_cast<unsigned char>(input[i]);
                result[i] = (c >= 'a' && c <= 'z') ? static_cast<char>(c - 32) : input[i];
            }
            return true;
        }

        result.clear();
        result.reserve(input.size() * 2);
        size_t pos = 0;
        while (pos < input.size()) {
            int byteLen = 0;
            int32_t codePoint = Utf8FirstCodepoint(input.data() + pos, input.size() - pos, byteLen);
            if (codePoint < 0 || byteLen <= 0) {
                result.append(input.data() + pos, input.size() - pos);
                return true;
            }

            size_t nextPos = pos + static_cast<size_t>(byteLen);
            if (codePoint == 0x03C2 || codePoint == 0x03C3) {
                AppendUtf8Codepoint(result, 0x03A3);
                pos = nextPos;
            } else if (codePoint == 'i') {
                result.push_back('I');
                if (nextPos < input.size()) {
                    int dotLen = 0;
                    int32_t nextCp =
                        Utf8FirstCodepoint(input.data() + nextPos, input.size() - nextPos, dotLen);
                    if (nextCp == 0x0307 && dotLen > 0) {
                        AppendUtf8Codepoint(result, 0x0307);
                        pos = nextPos + static_cast<size_t>(dotLen);
                        continue;
                    }
                }
                pos = nextPos;
            } else {
                AppendUtf8Codepoint(result, UpperUnicodeCodepoint(codePoint));
                pos = nextPos;
            }
        }
        return true;
    }

    ALWAYS_INLINE bool callNullable(std::string& result, const std::string_view* input)
    {
        if (input == nullptr) {
            return false;
        }
        return call(result, *input);
    }
};

/// soundex function
/// soundex(string) -> string
/// Returns the Soundex code using US English mapping (Spark/Velox semantics).
/// If input is empty, returns empty string. If first character is not alphabetic,
/// returns the original input unchanged.
template <typename T>
struct SoundexFunction {
    ALWAYS_INLINE bool call(std::string& result, const std::string_view& input)
    {
        const size_t inputSize = input.size();
        if (inputSize == 0) {
            result.clear();
            return true;
        }

        const unsigned char firstChar = static_cast<unsigned char>(input[0]);
        if (!IsAsciiAlpha(firstChar)) {
            result.assign(input.data(), input.size());
            return true;
        }

        result.resize(4);
        const char firstUpper = ToAsciiUpper(firstChar);
        result[0] = firstUpper;

        int32_t soundexIndex = 1;
        int32_t dataIndex = firstUpper - 'A';
        char lastCode = kUSEnglishMapping[dataIndex];
        for (size_t i = 1; i < inputSize; ++i) {
            const unsigned char currentChar = static_cast<unsigned char>(input[i]);
            if (!IsAsciiAlpha(currentChar)) {
                lastCode = '0';
                continue;
            }

            dataIndex = static_cast<int32_t>(ToAsciiUpper(currentChar)) - 'A';
            const char code = kUSEnglishMapping[dataIndex];
            if (code != kBridgeCode) {
                if (code != '0' && code != lastCode) {
                    result[soundexIndex++] = code;
                    if (soundexIndex > 3) {
                        break;
                    }
                }
                lastCode = code;
            }
        }

        for (; soundexIndex < 4; ++soundexIndex) {
            result[soundexIndex] = '0';
        }
        return true;
    }

    ALWAYS_INLINE bool callNullable(std::string& result, const std::string_view* input)
    {
        if (input == nullptr) {
            return false;
        }
        return call(result, *input);
    }

private:
    // Soundex mapping uses '7' as bridge marker (for letters like H/W).
    // It is not emitted to output, but affects adjacency dedup behavior.
    static constexpr char kBridgeCode = '7';

    ALWAYS_INLINE static bool IsAsciiAlpha(unsigned char c)
    {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
    }

    ALWAYS_INLINE static char ToAsciiUpper(unsigned char c)
    {
        return (c >= 'a' && c <= 'z') ? static_cast<char>(c - ('a' - 'A')) : static_cast<char>(c);
    }

    static constexpr char kUSEnglishMapping[26] = {
        '0', '1', '2', '3', '0', '1', '2', '7', '0', '2', '2', '4', '5',
        '5', '0', '1', '2', '6', '2', '3', '0', '1', '7', '2', '0', '2'};
};

/// base64(binary) -> varchar
/// Encodes binary data to Base64 MIME string (CRLF every 76 chars).
/// Aligned with Spark SQL base64 semantics (MIME encoding per RFC 2045).
/// Empty input returns empty string. NULL input yields NULL via framework propagation.
template <typename T>
struct Base64Function {
    ALWAYS_INLINE void call(std::string& result, const std::string_view& input) {
        if (input.empty()) {
            result.clear();
            return;
        }
        size_t encodedSize = Base64MimeEncodedSize(input.size());
        result.resize(encodedSize);
        Base64EncodeMime(input.data(), input.size(), &result[0]);
    }
};

/// unbase64(string) -> varbinary (as string)
/// Decodes Base64-encoded string to binary. Returns Status on decode error (row becomes NULL).
template <typename T>
struct UnBase64Function {
    ALWAYS_INLINE Status call(std::string& result, const std::string_view& input) {
        size_t maxDecodedSize = (input.size() / 4) * 3 + 3;
        result.resize(maxDecodedSize);
        size_t actualSize = 0;
        Status st = Base64Decode(input.data(), input.size(), &result[0], maxDecodedSize, &actualSize);
        if (!st.ok()) {
            return st;
        }
        result.resize(actualSize);
        return Status::OK();
    }
};

/// length(binary) -> integer
/// Returns the byte length of binary input, aligned with Spark SQL / Velox semantics.
/// Unlike length(string) which returns character (Unicode code point) count,
/// length(binary) returns the raw byte count.
template <typename T>
struct BinaryLengthFunction {
    ALWAYS_INLINE bool call(int32_t &result, const std::string_view &input)
    {
        result = static_cast<int32_t>(input.size());
        return true;
    }

    ALWAYS_INLINE bool callNullable(int32_t &result, const std::string_view *input)
    {
        return call(result, *input);
    }
};

/// bit_length(string/binary) -> integer
/// Returns the bit length of the input string or binary.
/// The bit length is the byte length multiplied by 8.
/// Examples:
///   bit_length("") = 0
///   bit_length("1") = 8
///   bit_length("123") = 24
///   bit_length("hello") = 40
template <typename T>
struct BitLengthFunction {
    ALWAYS_INLINE Status call(int32_t &result, const std::string_view &input)
    {

        // Bit length = byte length * 8
        result = static_cast<int32_t>(input.size() * 8);
        return Status::OK();
    }

    ALWAYS_INLINE Status callNullable(int32_t &result, const std::string_view *input)
    {
        if (input == nullptr) {
            return Status::UserError("bit_length received NULL input");
        }
        return call(result, *input);
    }
};

/// repeat(string, n) -> varchar
/// Returns the string which repeats input n times.
/// Result size must be less than or equal to 1MB.
/// If n is less than or equal to 0, or input is empty, returns empty string.
/// On integer overflow or result size > 1MB, returns error (row becomes NULL).
template <typename T>
struct RepeatFunction {
    static constexpr size_t kResultMaxSize = 1024 * 1024; // 1MB

    ALWAYS_INLINE bool call(std::string &result, const std::string_view &input, int32_t n)
    {
        return doCall(result, input, static_cast<int64_t>(n));
    }

    ALWAYS_INLINE bool call(std::string &result, const std::string_view &input, int64_t n)
    {
        return doCall(result, input, n);
    }

    ALWAYS_INLINE bool callNullable(std::string &result, const std::string_view *input, const int32_t *n)
    {
        if (input == nullptr || n == nullptr) {
            return false;
        }
        return call(result, *input, *n);
    }

    ALWAYS_INLINE bool callNullable(std::string &result, const std::string_view *input, const int64_t *n)
    {
        if (input == nullptr || n == nullptr) {
            return false;
        }
        return call(result, *input, *n);
    }

private:
    ALWAYS_INLINE bool doCall(std::string &result, const std::string_view &input, int64_t n)
    {
        size_t inputSize = input.size();
        if (inputSize == 0 || n <= 0) {
            result.clear();
            return true;
        }
        // Avoid integer overflow: result size = inputSize * n
        if (n > static_cast<int64_t>(kResultMaxSize / inputSize)) {
            result.clear();
            return true; // treat as empty to avoid throw; or use Status for strict Velox behavior
        }
        size_t newSize = inputSize * static_cast<size_t>(n);
        if (newSize > kResultMaxSize) {
            result.clear();
            return true;
        }
        result.resize(newSize);
        for (int64_t i = 0; i < n; ++i) {
            std::memcpy(result.data() + i * inputSize, input.data(), inputSize);
        }
        return true;
    }
};

/// Pad functions base template
/// pad(string, size, padString) -> varchar
/// Pads string to size characters with padString. If size is less than the length
/// of string, the result is truncated to size characters. size must not be negative
/// and padString must be non-empty.
/// Supports both ASCII and Unicode strings.
/// @tparam lpad If true, left pads the string (padding at beginning).
///              If false, right pads the string (padding at end).
/// @tparam T Type parameter for function registration.
template <bool lpad, typename T>
struct PadFunctionBase {
    static constexpr size_t kPadMaxSize = 1024 * 1024; // 1MB max size

    ALWAYS_INLINE bool call(std::string &result, const std::string_view &string,
        const int64_t &size, const std::string_view &padString)
    {
        // Validate size
        if (size < 0 || static_cast<size_t>(size) > kPadMaxSize) {
            result.clear();
            return true;
        }

        // Validate padString
        if (padString.empty()) {
            result.clear();
            return true;
        }

        int64_t padStringCharLength = stringImpl::length<false /*isAscii*/>(padString);
        if (padStringCharLength <= 0) {
            result.clear();
            return true;
        }

        int64_t stringCharLength = stringImpl::length<false /*isAscii*/>(string);

        // If string has at least size characters, truncate it if necessary
        if (stringCharLength >= size) {
            size_t prefixByteSize = static_cast<size_t>(
                stringImpl::cappedByteLengthUnicode(string.data(), string.size(), size));
            result.assign(string.data(), prefixByteSize);
            return true;
        }

        // Calculate padding needed
        int64_t fullPaddingCharLength = size - stringCharLength;
        int64_t fullPadCopies = fullPaddingCharLength / padStringCharLength;
        int64_t remainingPadChars = fullPaddingCharLength % padStringCharLength;

        // Calculate byte length of the partial pad prefix
        size_t padPrefixByteLength = static_cast<size_t>(
            stringImpl::cappedByteLengthUnicode(padString.data(), padString.size(), remainingPadChars));

        // Calculate total byte length
        int64_t fullPaddingByteLength = static_cast<int64_t>(padString.size()) * fullPadCopies +
            static_cast<int64_t>(padPrefixByteLength);
        int64_t outputByteLength = static_cast<int64_t>(string.size()) + fullPaddingByteLength;

        result.resize(static_cast<size_t>(outputByteLength));

        size_t paddingOffset;
        if constexpr (lpad) {
            // For lpad: padding comes first, then the string
            paddingOffset = 0;
            // Copy original string after the padding
            std::memcpy(result.data() + fullPaddingByteLength, string.data(), string.size());
        } else {
            // For rpad: string comes first, then the padding
            paddingOffset = string.size();
            // Copy original string at the beginning
            std::memcpy(result.data(), string.data(), string.size());
        }

        // Copy full pad strings
        for (int64_t i = 0; i < fullPadCopies; i++) {
            std::memcpy(result.data() + paddingOffset + i * padString.size(),
                padString.data(), padString.size());
        }

        // Copy partial pad prefix
        std::memcpy(result.data() + paddingOffset + fullPadCopies * padString.size(),
            padString.data(), padPrefixByteLength);

        return true;
    }

    ALWAYS_INLINE bool callNullable(std::string &result, const std::string_view *string,
        const int64_t *size, const std::string_view *padString)
    {
        if (string == nullptr || size == nullptr || padString == nullptr) {
            return false;
        }
        return call(result, *string, *size, *padString);
    }
};

/// lpad function
/// lpad(string, size, padString) -> varchar
/// Left pads string to size characters with padString.
template <typename T>
struct LPadFunction : public PadFunctionBase<true, T> {};

/// rpad function
/// rpad(string, size, padString) -> varchar
/// Right pads string to size characters with padString.
template <typename T>
struct RPadFunction : public PadFunctionBase<false, T> {};

/// overlay function
/// overlay(input, replace, pos, len) -> varchar
/// Replaces len characters of input starting at 1-based position pos with replace.
/// result = input[1..pos-1] + replace + input[(pos+effectiveLen)..end] (1-based).
/// pos: 1-based; 0 means first char; negative means from end (Spark semantics).
/// len: number of chars to replace; -1 means use replace string length (Unicode chars).
/// Part3 start in Velox is 1-based (pos+length), so 0-based index = (pos+effectiveLen)-1.
template <typename T>
struct OverlayFunction {
    ALWAYS_INLINE bool call(std::string &result, const std::string_view &input,
        const std::string_view &replace, int32_t pos, int32_t len)
    {
        int64_t numChars = stringImpl::length<false /*isAscii*/>(input);
        int64_t startChar0 = 0;
        if (pos > 0) {
            startChar0 = std::min(static_cast<int64_t>(pos - 1), numChars);
        }

        int32_t effectiveLen = (len >= 0) ? len : static_cast<int32_t>(stringImpl::length<false>(replace));
        int64_t posPlusLen = static_cast<int64_t>(pos) + static_cast<int64_t>(effectiveLen);
        int64_t part3StartChar0 = posPlusLen > 0 ? posPlusLen - 1 : numChars + posPlusLen;
        part3StartChar0 = std::max(0L, std::min(part3StartChar0, numChars));

        size_t part1ByteLen = static_cast<size_t>(stringImpl::cappedByteLengthUnicode(
            input.data(), static_cast<int64_t>(input.size()), startChar0));

        result.clear();
        result.reserve(part1ByteLen + replace.size() + (input.size() - part1ByteLen));
        result.append(input.data(), part1ByteLen);
        result.append(replace.data(), replace.size());

        if (part3StartChar0 < numChars) {
            size_t part3ByteStart = static_cast<size_t>(stringImpl::cappedByteLengthUnicode(
                input.data(), static_cast<int64_t>(input.size()), part3StartChar0));
            result.append(input.data() + part3ByteStart, input.size() - part3ByteStart);
        }
        return true;
    }
};

/// unhex function
/// unhex(string) -> varbinary
/// Converts a hexadecimal string to binary data.
/// The input string should contain only hexadecimal characters (0-9, a-f, A-F).
/// If the input string has odd length, the first character is treated as a single hex digit.
/// Returns NULL if any character in the input is not a valid hexadecimal character.
/// Edge cases:
/// - Empty string returns empty binary data.
/// - Odd-length strings: first character is treated as a single hex digit (e.g., "123" -> 0x01, 0x23).
/// - Invalid hex characters (e.g., 'G', 'g', '@') cause the function to return NULL.
template <typename T>
struct UnhexFunction {
    ALWAYS_INLINE bool call(std::string& result, const std::string_view& input)
    {
        // Empty input returns empty result
        if (input.empty()) {
            result.clear();
            return true;
        }

        const size_t resultSize = (input.size() + 1) >> 1;
        result.resize(resultSize);
        const char* inputBuffer = input.data();
        char* resultBuffer = &result[0];

        size_t i = 0;
        // Handle odd-length input: first character is a single hex digit
        if ((input.size() & 0x01) != 0) {
            const auto v = fromHex(inputBuffer[0]);
            if (v == -1) {
                return false;  // Invalid hex character, return NULL
            }
            resultBuffer[0] = static_cast<char>(v);
            i += 1;
        }

        // Process pairs of hex characters
        while (i < input.size()) {
            const auto first = fromHex(inputBuffer[i]);
            const auto second = fromHex(inputBuffer[i + 1]);
            if (first == -1 || second == -1) {
                return false;  // Invalid hex character, return NULL
            }
            resultBuffer[(i + 1) / 2] = static_cast<char>((first << 4) | second);
            i += 2;
        }

        return true;
    }

    ALWAYS_INLINE bool callNullable(std::string& result, const std::string_view* input)
    {
        if (input == nullptr) {
            return false;  // NULL input returns NULL
        }
        return call(result, *input);
    }

private:
    /// Convert a hex character to its numeric value.
    /// Returns -1 for invalid hex characters.
    /// Supports: '0'-'9' -> 0-9, 'A'-'F' -> 10-15, 'a'-'f' -> 10-15
    ALWAYS_INLINE static int8_t fromHex(char c)
    {
        if (c >= '0' && c <= '9') {
            return c - '0';
        }
        if (c >= 'A' && c <= 'F') {
            return 10 + c - 'A';
        }
        if (c >= 'a' && c <= 'f') {
            return 10 + c - 'a';
        }
        return -1;
    }
};

/// split_part function
/// split_part(string, delimiter, index) -> varchar
/// Splits string on delimiter and returns the part at index.
/// Field indexes start with 1. If the index is larger than the
/// number of fields, then null is returned.
/// When delimiter is empty, the string is split into individual characters (Unicode-aware).
/// Note: Only callNullable is implemented since this function can return NULL
/// when index is out of range.
template <typename T>
struct SplitPartFunction {
    /// Nullable version - main implementation
    /// Returns true if result is valid, false if result should be NULL
    ALWAYS_INLINE bool callNullable(std::string &result, const std::string_view *input,
        const std::string_view *delimiter, const int64_t *index)
    {
        // Handle NULL inputs
        if (input == nullptr || delimiter == nullptr || index == nullptr) {
            return false;
        }

        // Index must be greater than zero, return NULL if not
        if (*index <= 0) {
            return false;
        }

        int64_t iteration = 1;
        size_t curPos = 0;

        // Handle empty delimiter - split into individual characters (Unicode-aware)
        if (delimiter->empty()) {
            // UTF-8 character iteration
            while (curPos < input->size()) {
                int byteLen = 0;
                int codePoint = Utf8FirstCodepoint(input->data() + curPos, input->size() - curPos, byteLen);
                
                if (byteLen <= 0 || codePoint < 0) {
                    // Invalid UTF-8, treat as single byte
                    byteLen = 1;
                }
                
                if (iteration == *index) {
                    result.assign(input->data() + curPos, byteLen);
                    return true;
                }
                
                curPos += byteLen;
                iteration++;
            }
            // Index out of range
            return false;
        }

        // Non-empty delimiter
        while (curPos <= input->size()) {
            size_t start = curPos;
            size_t foundPos = input->find(*delimiter, curPos);
            
            if (iteration == *index) {
                size_t end = foundPos;
                if (end == std::string_view::npos) {
                    end = input->size();
                }
                result.assign(input->data() + start, end - start);
                return true;
            }
            
            if (foundPos == std::string_view::npos) {
                // No more delimiters and index not found
                return false;
            }
            
            curPos = foundPos + delimiter->size();
            iteration++;
        }
        
        return false;
    }
};


/// translate function
/// translate(string, match, replace) -> varchar
/// Returns a new translated string. It translates the character in `string`
/// by a character in `replace`. The character in `replace` is corresponding
/// to the character in `match`. The translation will happen when any character
/// in `string` matches with a character in `match`.
/// - If match is longer than replace, extra characters in match are deleted from result.
/// - If replace is longer than match, extra characters in replace are ignored.
/// - Only the first occurrence of each character in match is considered.
/// Supports both ASCII and Unicode (UTF-8) strings.
template <typename T>
struct TranslateFunction {
private:
    /// Returns the length of a UTF-8 character starting at 'offset'.
    /// Returns 1 for invalid UTF-8 character.
    ALWAYS_INLINE int32_t getUtf8CharLength(const std::string_view& input, int32_t offset) const
    {
        if (offset >= static_cast<int32_t>(input.size())) {
            return 1;
        }
        int byteLen = 0;
        int32_t codepoint = Utf8FirstCodepoint(input.data() + offset, input.size() - offset, byteLen);
        // For invalid UTF-8, return 1 (treat as single byte)
        if (codepoint < 0 || byteLen <= 0) {
            return 1;
        }
        return byteLen;
    }

    /// Builds a dictionary mapping each character in match to the corresponding character in replace.
    /// If match is longer than replace, extra characters map to empty string (deletion).
    /// Only the first occurrence of each character in match is considered.
    std::unordered_map<std::string, std::string> buildDictionary(
        const std::string_view& match, const std::string_view& replace) const
    {
        std::unordered_map<std::string, std::string> dictionary;
        int32_t matchIdx = 0;
        int32_t replaceIdx = 0;

        while (matchIdx < static_cast<int32_t>(match.size())) {
            std::string replaceChar;
            // If match's character size is larger than replace's, the extra
            // characters in match will be removed from input string.
            if (replaceIdx < static_cast<int32_t>(replace.size())) {
                int32_t replaceCharLength = getUtf8CharLength(replace, replaceIdx);
                replaceChar = std::string(replace.data() + replaceIdx, replaceCharLength);
                replaceIdx += replaceCharLength;
            }

            int32_t matchCharLength = getUtf8CharLength(match, matchIdx);
            std::string matchChar = std::string(match.data() + matchIdx, matchCharLength);
            // Only considers the first occurrence of a character in match.
            dictionary.emplace(matchChar, replaceChar);
            matchIdx += matchCharLength;
        }
        return dictionary;
    }

public:
    ALWAYS_INLINE bool call(std::string& result, const std::string_view& input,
        const std::string_view& match, const std::string_view& replace)
    {
        // Build the translation dictionary
        auto dictionary = buildDictionary(match, replace);

        // No need to do the translation if dictionary is empty
        if (dictionary.empty()) {
            result.assign(input.data(), input.size());
            return true;
        }

        // Reserve initial capacity (input size, may grow for Unicode replacements)
        result.clear();
        result.reserve(input.size());

        int32_t inputIdx = 0;
        while (inputIdx < static_cast<int32_t>(input.size())) {
            int32_t charLength = getUtf8CharLength(input, inputIdx);
            std::string inputChar(input.data() + inputIdx, charLength);

            auto it = dictionary.find(inputChar);
            if (it == dictionary.end()) {
                // Character not in match, append as-is
                result.append(inputChar);
            } else {
                // Character in match, append replacement (may be empty for deletion)
                result.append(it->second);
            }
            inputIdx += charLength;
        }

        return true;
    }

    ALWAYS_INLINE bool callNullable(std::string& result, const std::string_view* input,
        const std::string_view* match, const std::string_view* replace)
    {
        if (input == nullptr || match == nullptr || replace == nullptr) {
            return false;
        }
        return call(result, *input, *match, *replace);
    }
};


/// find_in_set function
/// find_in_set(str, strArray) -> int32
/// Returns the 1-based index of the first occurrence of str in strArray where
/// strArray is a comma-delimited string. Returns 0 if str is not found or if
/// str contains a comma. Both arguments are VARCHAR (string_view).
/// Aligned with Spark/Velox semantics:
///   - find_in_set("ab", "abc,b,ab,c,def") = 3
///   - find_in_set("ab,", "abc,b,ab,c,def") = 0 (needle contains comma)
///   - find_in_set("", "") = 1
///   - find_in_set("", "123") = 0
///   - find_in_set("", ",123") = 1
///   - NULL propagation handled by SimpleFunction framework.
template <typename T>
struct FindInSetFunction {
    ALWAYS_INLINE bool call(int32_t &result, const std::string_view &str,
        const std::string_view &strArray)
    {
        if (str.find(',') != std::string_view::npos) {
            result = 0;
            return true;
        }

        int32_t index = 1;
        int32_t lastComma = -1;
        const char *arrayData = strArray.data();
        const char *matchData = str.data();
        size_t arraySize = strArray.size();
        size_t matchSize = str.size();

        for (size_t i = 0; i < arraySize; i++) {
            if (arrayData[i] == ',') {
                if (i - static_cast<size_t>(lastComma + 1) == matchSize &&
                    std::memcmp(arrayData + (lastComma + 1), matchData, matchSize) == 0) {
                    result = index;
                    return true;
                }
                lastComma = static_cast<int32_t>(i);
                index++;
            }
        }

        if (arraySize - static_cast<size_t>(lastComma + 1) == matchSize &&
            std::memcmp(arrayData + (lastComma + 1), matchData, matchSize) == 0) {
            result = index;
            return true;
        }

        result = 0;
        return true;
    }

    ALWAYS_INLINE bool callNullable(int32_t &result, const std::string_view *str,
        const std::string_view *strArray)
    {
        if (str == nullptr || strArray == nullptr) {
            return false;
        }
        return call(result, *str, *strArray);
    }
};

/// levenshtein distance function
/// levenshtein(left, right) -> int32
/// levenshtein(left, right, threshold) -> int32
/// Computes the Levenshtein edit distance between two strings.
/// The distance is the minimum number of single-character edits (insertions,
/// deletions, or substitutions) required to change one string into the other.
/// When a threshold is provided, returns -1 if the distance exceeds the threshold.
/// Supports Unicode strings (measures distance in code points, not bytes).
/// Implementation based on Velox sparksql LevenshteinDistanceFunction and
/// org.apache.commons.text.similarity.LevenshteinDistance.limitedCompare.
template <typename T>
struct LevenshteinDistanceFunction {
    ALWAYS_INLINE bool call(int32_t &result, const std::string_view &left, const std::string_view &right)
    {
        return callWithThreshold(result, left, right, std::numeric_limits<int32_t>::max());
    }

    ALWAYS_INLINE bool call(int32_t &result, const std::string_view &left, const std::string_view &right,
        const int32_t &threshold)
    {
        return callWithThreshold(result, left, right, threshold);
    }

    ALWAYS_INLINE bool callNullable(int32_t &result, const std::string_view *left, const std::string_view *right)
    {
        if (left == nullptr || right == nullptr) {
            return false;
        }
        return call(result, *left, *right);
    }

    ALWAYS_INLINE bool callNullable(int32_t &result, const std::string_view *left, const std::string_view *right,
        const int32_t *threshold)
    {
        if (left == nullptr || right == nullptr || threshold == nullptr) {
            return false;
        }
        return call(result, *left, *right, *threshold);
    }

private:
    static std::vector<int32_t> StringToCodePoints(const std::string_view &str)
    {
        std::vector<int32_t> codePoints;
        codePoints.reserve(str.size());
        size_t i = 0;
        while (i < str.size()) {
            int byteLen = 0;
            int32_t cp = Utf8FirstCodepoint(str.data() + i, str.size() - i, byteLen);
            if (byteLen <= 0) {
                byteLen = 1;
            }
            codePoints.push_back(cp);
            i += static_cast<size_t>(byteLen);
        }
        return codePoints;
    }

    bool callWithThreshold(int32_t &result, const std::string_view &left, const std::string_view &right,
        int32_t threshold)
    {
        auto leftCodePoints = StringToCodePoints(left);
        auto rightCodePoints = StringToCodePoints(right);
        doCall(result, leftCodePoints.data(), rightCodePoints.data(),
            leftCodePoints.size(), rightCodePoints.size(), threshold);
        return true;
    }

    void doCall(int32_t &result, const int32_t *leftCodePoints, const int32_t *rightCodePoints,
        size_t leftCodePointsSize, size_t rightCodePointsSize, int32_t threshold)
    {
        if (leftCodePointsSize < rightCodePointsSize) {
            doCall(result, rightCodePoints, leftCodePoints,
                rightCodePointsSize, leftCodePointsSize, threshold);
            return;
        }

        if (static_cast<int64_t>(leftCodePointsSize) - static_cast<int64_t>(rightCodePointsSize) > threshold) {
            result = -1;
            return;
        }
        if (rightCodePointsSize == 0) {
            result = static_cast<int32_t>(leftCodePointsSize);
            return;
        }

        std::vector<int32_t> distances;
        distances.reserve(rightCodePointsSize);
        int32_t boundary = std::min(static_cast<int32_t>(rightCodePointsSize), threshold);
        int32_t idx = 0;
        for (; idx < boundary; idx++) {
            distances.push_back(idx + 1);
        }
        for (; idx < static_cast<int32_t>(rightCodePointsSize); idx++) {
            distances.push_back(std::numeric_limits<int32_t>::max());
        }

        for (size_t i = 0; i < leftCodePointsSize; i++) {
            int32_t lower = std::max(0, static_cast<int32_t>(i) - threshold);
            int32_t maxValueWithThreshold = 0;
            int32_t upper = static_cast<int32_t>(rightCodePointsSize);
            bool overflow = __builtin_add_overflow(static_cast<int32_t>(i) + 1, threshold, &maxValueWithThreshold);
            if (!overflow) {
                upper = std::min(static_cast<int32_t>(rightCodePointsSize), maxValueWithThreshold);
            }
            if (lower > upper) {
                result = -1;
                return;
            }
            int32_t leftUpDistance = 0;
            if (lower == 0) {
                leftUpDistance = distances[lower];
                if (leftCodePoints[i] == rightCodePoints[0]) {
                    distances[0] = static_cast<int32_t>(i);
                } else {
                    distances[0] = std::min(static_cast<int32_t>(i), distances[0]) + 1;
                }
                lower = 1;
            } else {
                leftUpDistance = distances[lower - 1];
                distances[lower - 1] = std::numeric_limits<int32_t>::max();
            }
            for (int32_t j = lower; j < upper; j++) {
                int32_t leftUpDistanceNext = distances[j];
                if (leftCodePoints[i] == rightCodePoints[j]) {
                    distances[j] = leftUpDistance;
                } else {
                    distances[j] =
                        std::min(distances[j - 1], std::min(leftUpDistance, distances[j])) + 1;
                }
                leftUpDistance = leftUpDistanceNext;
            }
        }
        result = distances[rightCodePointsSize - 1];
        if (result > threshold) {
            result = -1;
        }
    }
};

/// initcap function
/// initcap(string) -> string
/// Converts the first letter of each word to uppercase and the rest to lowercase.
/// Word boundaries are determined by whitespace characters (space, tab, newline,
/// carriage return, form feed, vertical tab) following Spark SQL semantics.
/// Non-whitespace characters like hyphens, dots, and @ are NOT word separators.
/// Examples:
///   initcap("hello world") = "Hello World"
///   initcap("HELLO WORLD") = "Hello World"
///   initcap("hello-world") = "Hello-world"
///   initcap("") = ""
///   initcap(NULL) = NULL
template <typename T>
struct InitCapFunction {
    ALWAYS_INLINE bool call(std::string &result, const std::string_view &input)
    {
        if (input.empty()) {
            result.clear();
            return true;
        }

        result.resize(input.size());
        bool isStartOfWord = true;
        for (size_t i = 0; i < input.size(); ++i) {
            unsigned char c = static_cast<unsigned char>(input[i]);
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v') {
                isStartOfWord = true;
                result[i] = input[i];
            } else if (isStartOfWord) {
                result[i] = static_cast<char>(std::toupper(c));
                isStartOfWord = false;
            } else {
                result[i] = static_cast<char>(std::tolower(c));
            }
        }
        return true;
    }

    ALWAYS_INLINE bool callNullable(std::string &result, const std::string_view *input)
    {
        if (input == nullptr) {
            return false;
        }
        return call(result, *input);
    }
};

inline void encodeDigestToBase16(uint8_t* output, int digestSize) {
    static unsigned char const kHexCodes[] = "0123456789abcdef";
    for (int i = digestSize - 1; i >= 0; --i) {
        int digestChar = output[i];
        output[i * 2] = kHexCodes[(digestChar >> 4) & 0xf];
        output[i * 2 + 1] = kHexCodes[digestChar & 0xf];
    }
}

template <typename T>
struct Sha1HexStringFunction {
    ALWAYS_INLINE bool callNullable(std::string &result, const std::string_view *input)
    {
        if (input == nullptr) {
            return false;
        }
        static const int kSha1Length = 20;
        result.resize(kSha1Length * 2);
        folly::ssl::OpenSSLHash::sha1(
            folly::MutableByteRange((uint8_t*)result.data(), 20),
            folly::ByteRange((const uint8_t*)input->data(), input->size()));
        encodeDigestToBase16((uint8_t*)result.data(), kSha1Length);
        return true;
    }
};

template <typename T>
struct Sha2HexStringFunction {
    ALWAYS_INLINE bool callNullable( std::string& result, const std::string_view *input, const int32_t *bitLength) {
        if (input == nullptr || bitLength == nullptr) {
            return false;
        }
        const int32_t nonzeroBitLength = (*bitLength == 0) ? 256 : *bitLength;
        const EVP_MD* hashAlgorithm;
        switch (nonzeroBitLength) {
        case 224:
            hashAlgorithm = EVP_sha224();
            break;
        case 256:
            hashAlgorithm = EVP_sha256();
            break;
        case 384:
            hashAlgorithm = EVP_sha384();
            break;
        case 512:
            hashAlgorithm = EVP_sha512();
            break;
        default:
            // For an unsupported bitLength, the return value is NULL.
                return false;
        }
        const int32_t digestLength = nonzeroBitLength >> 3;
        result.resize(digestLength * 2);
        auto resultBuffer =
            folly::MutableByteRange((uint8_t*)result.data(), digestLength);
        auto inputBuffer =
            folly::ByteRange((const uint8_t*)input->data(), input->size());
        folly::ssl::OpenSSLHash::hash(resultBuffer, hashAlgorithm, inputBuffer);
        encodeDigestToBase16((uint8_t*)result.data(), digestLength);
        return true;
    }
};

template <typename T>
struct Md5Function {
    ALWAYS_INLINE bool callNullable(std::string &result, const std::string_view *input) {
        if (input == nullptr) {
            return false;
        }
        codegen::function::Md5Function md5(input->data(), input->size());
        char resultBuffer[32];
        md5.FinishHex(resultBuffer);
        result = std::string(resultBuffer, 32);
        return true;
    }
};

template <typename T>
struct StaticInvokeVarcharTypeWriteSideCheckFunction {
    ALWAYS_INLINE bool callNullable( std::string& result, const std::string_view *input, const int32_t *limit) {
        if (input == nullptr) {
            return false;
        }
        auto inputData = input->data();
        auto inputSize = input->size();
        int32_t ssLen = codegen::function::StringUtil::NumChars(inputData, inputSize);
        if (ssLen <= *limit) {
            result = std::string(*input);
            return true;
        }
        int32_t numTailSpacesToTrim = ssLen - *limit;
        int32_t endIdx = inputSize - 1;
        int32_t trimTo = inputSize - numTailSpacesToTrim;
        while (endIdx >= trimTo && inputData[endIdx] == ' ') {
            endIdx--;
        }
        int32_t outByteNum = endIdx + 1;
        ssLen = codegen::function::StringUtil::NumChars(inputData, outByteNum);
        if (ssLen > *limit) {
            OMNI_THROW("RUNTIME_ERROR:", "Exceeds varchar type length limitation: " + std::to_string(*limit));
        }
        result.assign(inputData, outByteNum);
        return true;
    }
};

template <typename T>
struct StaticInvokeCharTypeWriteSideCheckFunction {
    ALWAYS_INLINE bool callNullable( std::string& result, const std::string_view *input, const int32_t *limit) {
        if (input == nullptr) {
            return false;
        }
        auto inputData = input->data();
        auto inputSize = input->size();
        int32_t ssLen = codegen::function::StringUtil::NumChars(inputData, inputSize);
        if (ssLen == *limit) {
            result = std::string(*input);
            return true;
        }
        if (ssLen < *limit) {
            int32_t numTailSpacesToAdd = *limit - ssLen;
            result = std::string(*input);
            result.append(numTailSpacesToAdd, ' ');
            return true;
        }
        int32_t numTailSpacesToTrim = ssLen - *limit;
        int32_t endIdx = inputSize - 1;
        int32_t trimTo = inputSize - numTailSpacesToTrim;
        while (endIdx >= trimTo && inputData[endIdx] == ' ') {
            endIdx--;
        }
        int32_t outByteNum = endIdx + 1;
        ssLen = codegen::function::StringUtil::NumChars(inputData, outByteNum);
        if (ssLen > *limit) {
            OMNI_THROW("RUNTIME_ERROR:", "Exceeds char type length limitation: " + std::to_string(*limit));
        }
        result.assign(inputData, outByteNum);
        return true;
    }
};

template <typename T>
struct StaticInvokeCharReadPaddingFunction {
    ALWAYS_INLINE bool callNullable( std::string& result, const std::string_view *input, const int32_t *limit) {
        if (input == nullptr) {
            return false;
        }
        int32_t ssLen = codegen::function::StringUtil::NumChars(input->data(), input->size());
        result = std::string(*input);
        if (ssLen < *limit) {
            result.append(*limit - ssLen, ' ');
        }
        return true;
    }
};

template <typename T>
struct InstrFunction {
    ALWAYS_INLINE void call(int32_t &result, const std::string_view &string, const std::string_view &subString)
    {
        if (subString.empty()) {
            result = 1;
            return;
        }
        if (subString.size() > string.size()) {
            result = 0;
            return;
        }

        auto pos = stringImpl::StringPosition<false, true>(string, subString, 1);
        result = static_cast<int32_t>(pos);
    }
};

template <typename T>
struct ReplaceFunction {
    ALWAYS_INLINE bool call(std::string &result, const std::string_view &str, 
        const std::string_view &pattern, const std::string_view &replacement)
    {
        return doReplace(result, str, pattern, replacement);
    }

private:
    ALWAYS_INLINE bool doReplace(std::string &result, const std::string_view &str, 
        const std::string_view &pattern, const std::string_view &replacement)
    {
        // If pattern is empty, return original string
        if (pattern.empty()) {
            result.assign(str.data(), str.size());
            return true;
        }

        // If str is empty, return empty string
        if (str.empty()) {
            result.clear();
            return true;
        }

        // Count occurrences to pre-allocate result buffer
        size_t count = 0;
        size_t pos = 0;
        size_t patternLen = pattern.size();
        
        while ((pos = str.find(pattern, pos)) != std::string_view::npos) {
            count++;
            pos += patternLen;
        }

        // Calculate result size and reserve memory
        size_t resultSize = str.size() + count * (replacement.size() - patternLen);
        result.reserve(resultSize);

        // Perform replacement
        pos = 0;
        size_t lastPos = 0;
        while ((pos = str.find(pattern, lastPos)) != std::string_view::npos) {
            // Append part before the match
            result.append(str.data() + lastPos, pos - lastPos);
            // Append replacement
            result.append(replacement.data(), replacement.size());
            lastPos = pos + patternLen;
        }

        // Append remaining part after last match
        result.append(str.data() + lastPos, str.size() - lastPos);

        return true;
    }
};

/// flink_replace(string, search, replacement) -> varchar
/// Flink REPLACE (SqlFunctionUtils.replace) delegates to Java String.replace(CharSequence,
/// CharSequence), whose empty-search behaviour differs from Spark's replace: instead of
/// returning the input unchanged, the replacement is inserted around every character.
///   REPLACE('abc', '', 'X') -> 'XaXbXcX'   REPLACE('', '', 'X') -> 'X'
/// Java iterates UTF-16 code units; here we iterate UTF-8 characters, which agrees for the
/// BMP and keeps surrogate pairs intact for supplementary characters.
template <typename T>
struct FlinkReplaceFunction {
    ALWAYS_INLINE bool call(std::string &result, const std::string_view &str,
        const std::string_view &search, const std::string_view &replacement)
    {
        if (!search.empty()) {
            return ReplaceFunction<T>().call(result, str, search, replacement);
        }

        result.clear();
        result.append(replacement.data(), replacement.size());
        size_t pos = 0;
        while (pos < str.size()) {
            int charSize = stringImpl::utf8proc_char_length(str.data() + pos);
            size_t step = charSize < 0 ? 1 : static_cast<size_t>(charSize);
            step = std::min(step, str.size() - pos);
            result.append(str.data() + pos, step);
            result.append(replacement.data(), replacement.size());
            pos += step;
        }
        return true;
    }
};

/// flink_substr(string, pos[, length]) -> varchar
/// Flink SUBSTRING/SUBSTR (BinaryStringDataUtil.substringSQL), character based:
///   - length < 0            -> NULL         (Spark substr returns an empty string)
///   - pos > 0, pos > chars  -> ''
///   - pos < 0               -> start counted from the end; out of range -> ''
///                              (Spark clamps the start to the beginning instead)
///   - pos = 0               -> same as pos = 1
///   - length beyond the end -> truncated at the end of the string
/// The two-argument form takes everything up to the end of the string.
template <typename T>
struct FlinkSubstrFunction {
    ALWAYS_INLINE bool call(std::string &result, const std::string_view &input, int32_t pos)
    {
        return doCall(result, input, pos, std::numeric_limits<int32_t>::max());
    }

    ALWAYS_INLINE bool call(std::string &result, const std::string_view &input, int32_t pos, int32_t length)
    {
        return doCall(result, input, pos, length);
    }

private:
    ALWAYS_INLINE bool doCall(std::string &result, const std::string_view &input, int32_t pos, int32_t length)
    {
        if (length < 0) {
            return false;
        }
        result.clear();
        if (input.empty()) {
            return true;
        }

        int64_t numChars = stringImpl::length<false>(input);
        int64_t start;
        if (pos > 0) {
            start = static_cast<int64_t>(pos) - 1;
            if (start >= numChars) {
                return true;
            }
        } else if (pos < 0) {
            start = numChars + pos;
            if (start < 0) {
                return true;
            }
        } else {
            start = 0;
        }

        int64_t charCount = std::min(static_cast<int64_t>(length), numChars - start);
        if (charCount <= 0) {
            return true;
        }
        int64_t startByte = stringImpl::cappedByteLengthUnicode(
            input.data(), static_cast<int64_t>(input.size()), start);
        int64_t byteCount = stringImpl::cappedByteLengthUnicode(
            input.data() + startByte, static_cast<int64_t>(input.size()) - startByte, charCount);
        result.assign(input.data() + startByte, static_cast<size_t>(byteCount));
        return true;
    }
};

// ============================================================================
// ENCODE(string, charset) -> varbinary
// Encodes a UTF-8 string into binary data using the specified character set.
// Supported charsets: US-ASCII, ISO-8859-1, UTF-8, UTF-16BE, UTF-16LE, UTF-16.
// Returns NULL if either argument is NULL, charset is unsupported, or a character
// cannot be represented in the target charset.
// ============================================================================

namespace encode_detail {

inline bool EqualsIgnoreCase(const std::string_view& a, const std::string_view& b)
{
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

inline bool EncodeAscii(std::string& result, const std::string_view& input)
{
    size_t pos = 0;
    while (pos < input.size()) {
        int byteLen = 0;
        int32_t cp = Utf8FirstCodepoint(input.data() + pos, input.size() - pos, byteLen);
        if (cp < 0) {
            return false;
        }
        if (cp > 0x7F) {
            return false;
        }
        result.push_back(static_cast<char>(cp));
        pos += byteLen;
    }
    return true;
}

inline bool EncodeIso88591(std::string& result, const std::string_view& input)
{
    size_t pos = 0;
    while (pos < input.size()) {
        int byteLen = 0;
        int32_t cp = Utf8FirstCodepoint(input.data() + pos, input.size() - pos, byteLen);
        if (cp < 0) {
            return false;
        }
        if (cp > 0xFF) {
            return false;
        }
        result.push_back(static_cast<char>(cp));
        pos += byteLen;
    }
    return true;
}

inline bool EncodeUtf8(std::string& result, const std::string_view& input)
{
    size_t pos = 0;
    while (pos < input.size()) {
        int byteLen = 0;
        int32_t cp = Utf8FirstCodepoint(input.data() + pos, input.size() - pos, byteLen);
        if (cp < 0) {
            return false;
        }
        pos += byteLen;
    }
    result.assign(input.data(), input.size());
    return true;
}

inline bool EncodeUtf16BE(std::string& result, const std::string_view& input)
{
    size_t pos = 0;
    while (pos < input.size()) {
        int byteLen = 0;
        int32_t cp = Utf8FirstCodepoint(input.data() + pos, input.size() - pos, byteLen);
        if (cp < 0) {
            return false;
        }
        if (cp <= 0xFFFF) {
            result.push_back(static_cast<char>((cp >> 8) & 0xFF));
            result.push_back(static_cast<char>(cp & 0xFF));
        } else {
            int32_t adjusted = cp - 0x10000;
            uint16_t high = static_cast<uint16_t>(0xD800 + (adjusted >> 10));
            uint16_t low = static_cast<uint16_t>(0xDC00 + (adjusted & 0x3FF));
            result.push_back(static_cast<char>((high >> 8) & 0xFF));
            result.push_back(static_cast<char>(high & 0xFF));
            result.push_back(static_cast<char>((low >> 8) & 0xFF));
            result.push_back(static_cast<char>(low & 0xFF));
        }
        pos += byteLen;
    }
    return true;
}

inline bool EncodeUtf16LE(std::string& result, const std::string_view& input)
{
    size_t pos = 0;
    while (pos < input.size()) {
        int byteLen = 0;
        int32_t cp = Utf8FirstCodepoint(input.data() + pos, input.size() - pos, byteLen);
        if (cp < 0) {
            return false;
        }
        if (cp <= 0xFFFF) {
            result.push_back(static_cast<char>(cp & 0xFF));
            result.push_back(static_cast<char>((cp >> 8) & 0xFF));
        } else {
            int32_t adjusted = cp - 0x10000;
            uint16_t high = static_cast<uint16_t>(0xD800 + (adjusted >> 10));
            uint16_t low = static_cast<uint16_t>(0xDC00 + (adjusted & 0x3FF));
            result.push_back(static_cast<char>(high & 0xFF));
            result.push_back(static_cast<char>((high >> 8) & 0xFF));
            result.push_back(static_cast<char>(low & 0xFF));
            result.push_back(static_cast<char>((low >> 8) & 0xFF));
        }
        pos += byteLen;
    }
    return true;
}

inline bool EncodeUtf16(std::string& result, const std::string_view& input)
{
    // Write BOM (U+FEFF) in big-endian
    result.push_back(static_cast<char>(0xFE));
    result.push_back(static_cast<char>(0xFF));
    return EncodeUtf16BE(result, input);
}

} // namespace encode_detail

template <typename T>
struct EncodeFunction {
    ALWAYS_INLINE bool call(std::string& result, const std::string_view& input,
                            const std::string_view& charset)
    {
        if (encode_detail::EqualsIgnoreCase(charset, "US-ASCII") ||
            encode_detail::EqualsIgnoreCase(charset, "ASCII")) {
            return encode_detail::EncodeAscii(result, input);
        }
        if (encode_detail::EqualsIgnoreCase(charset, "ISO-8859-1") ||
            encode_detail::EqualsIgnoreCase(charset, "LATIN1")) {
            return encode_detail::EncodeIso88591(result, input);
        }
        if (encode_detail::EqualsIgnoreCase(charset, "UTF-8")) {
            return encode_detail::EncodeUtf8(result, input);
        }
        if (encode_detail::EqualsIgnoreCase(charset, "UTF-16BE")) {
            return encode_detail::EncodeUtf16BE(result, input);
        }
        if (encode_detail::EqualsIgnoreCase(charset, "UTF-16LE")) {
            return encode_detail::EncodeUtf16LE(result, input);
        }
        if (encode_detail::EqualsIgnoreCase(charset, "UTF-16")) {
            return encode_detail::EncodeUtf16(result, input);
        }
        return false;
    }
};

// ============================================================================
// DECODE(binary, charset) -> varchar
// Decodes binary data using the specified character set into a UTF-8 string.
// Supported charsets: US-ASCII, ISO-8859-1, UTF-8, UTF-16BE, UTF-16LE, UTF-16.
// Returns NULL if either argument is NULL, charset is unsupported, or data is
// invalid for the specified charset.
// ============================================================================

namespace decode_detail {

inline bool EqualsIgnoreCase(const std::string_view& a, const std::string_view& b)
{
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

inline bool DecodeAscii(std::string& result, const std::string_view& binary)
{
    for (unsigned char c : binary) {
        if (c > 0x7F) {
            return false;
        }
    }
    result.assign(binary.data(), binary.size());
    return true;
}

inline bool DecodeIso88591(std::string& result, const std::string_view& binary)
{
    result.reserve(binary.size());
    for (unsigned char c : binary) {
        AppendUtf8Codepoint(result, c);
    }
    return true;
}

inline bool ValidateUtf8(const std::string_view& data)
{
    size_t i = 0;
    while (i < data.size()) {
        unsigned char c = static_cast<unsigned char>(data[i]);
        int expectedLen;
        unsigned int minCp;
        if (c <= 0x7F) {
            ++i;
            continue;
        } else if ((c & 0xE0) == 0xC0) {
            expectedLen = 2;
            minCp = 0x80;
        } else if ((c & 0xF0) == 0xE0) {
            expectedLen = 3;
            minCp = 0x800;
        } else if ((c & 0xF8) == 0xF0) {
            expectedLen = 4;
            minCp = 0x10000;
        } else {
            return false;
        }
        if (i + expectedLen > data.size()) {
            return false;
        }
        for (int j = 1; j < expectedLen; ++j) {
            if ((static_cast<unsigned char>(data[i + j]) & 0xC0) != 0x80) {
                return false;
            }
        }
        int byteLen = 0;
        int32_t cp = Utf8FirstCodepoint(data.data() + i, data.size() - i, byteLen);
        if (cp < 0 || static_cast<unsigned int>(cp) < minCp) {
            return false;
        }
        if (cp >= 0xD800 && cp <= 0xDFFF) {
            return false;
        }
        if (cp > 0x10FFFF) {
            return false;
        }
        i += expectedLen;
    }
    return true;
}

inline bool DecodeUtf8(std::string& result, const std::string_view& binary)
{
    if (!ValidateUtf8(binary)) {
        return false;
    }
    result.assign(binary.data(), binary.size());
    return true;
}

inline bool DecodeUtf16BE(std::string& result, const std::string_view& binary)
{
    if (binary.size() % 2 != 0) {
        return false;
    }
    const auto* data = reinterpret_cast<const unsigned char*>(binary.data());
    size_t numUnits = binary.size() / 2;
    for (size_t i = 0; i < numUnits; ++i) {
        uint16_t unit = (static_cast<uint16_t>(data[i * 2]) << 8) |
                         static_cast<uint16_t>(data[i * 2 + 1]);
        if (unit >= 0xD800 && unit <= 0xDBFF) {
            ++i;
            if (i >= numUnits) {
                return false;
            }
            uint16_t low = (static_cast<uint16_t>(data[i * 2]) << 8) |
                            static_cast<uint16_t>(data[i * 2 + 1]);
            if (low < 0xDC00 || low > 0xDFFF) {
                return false;
            }
            int32_t cp = 0x10000 + ((static_cast<int32_t>(unit) - 0xD800) << 10) +
                          (static_cast<int32_t>(low) - 0xDC00);
            AppendUtf8Codepoint(result, cp);
        } else if (unit >= 0xDC00 && unit <= 0xDFFF) {
            return false;
        } else {
            AppendUtf8Codepoint(result, unit);
        }
    }
    return true;
}

inline bool DecodeUtf16LE(std::string& result, const std::string_view& binary)
{
    if (binary.size() % 2 != 0) {
        return false;
    }
    const auto* data = reinterpret_cast<const unsigned char*>(binary.data());
    size_t numUnits = binary.size() / 2;
    for (size_t i = 0; i < numUnits; ++i) {
        uint16_t unit = static_cast<uint16_t>(data[i * 2]) |
                        (static_cast<uint16_t>(data[i * 2 + 1]) << 8);
        if (unit >= 0xD800 && unit <= 0xDBFF) {
            ++i;
            if (i >= numUnits) {
                return false;
            }
            uint16_t low = static_cast<uint16_t>(data[i * 2]) |
                           (static_cast<uint16_t>(data[i * 2 + 1]) << 8);
            if (low < 0xDC00 || low > 0xDFFF) {
                return false;
            }
            int32_t cp = 0x10000 + ((static_cast<int32_t>(unit) - 0xD800) << 10) +
                          (static_cast<int32_t>(low) - 0xDC00);
            AppendUtf8Codepoint(result, cp);
        } else if (unit >= 0xDC00 && unit <= 0xDFFF) {
            return false;
        } else {
            AppendUtf8Codepoint(result, unit);
        }
    }
    return true;
}

inline bool DecodeUtf16(std::string& result, const std::string_view& binary)
{
    if (binary.size() < 2) {
        if (binary.empty()) {
            result.clear();
            return true;
        }
        return false;
    }
    const auto* data = reinterpret_cast<const unsigned char*>(binary.data());
    uint16_t first = (static_cast<uint16_t>(data[0]) << 8) | static_cast<uint16_t>(data[1]);
    if (first == 0xFEFF) {
        std::string_view payload = binary.substr(2);
        return DecodeUtf16BE(result, payload);
    }
    if (first == 0xFFFE) {
        std::string_view payload = binary.substr(2);
        return DecodeUtf16LE(result, payload);
    }
    return DecodeUtf16BE(result, binary);
}

} // namespace decode_detail

template <typename T>
struct DecodeFunction {
    ALWAYS_INLINE bool callNullable(std::string& result, const std::string_view* binary,
                            const std::string_view* charset)
    {
        if (binary == nullptr || charset == nullptr) {
            return false;
        }
        if (decode_detail::EqualsIgnoreCase(*charset, "US-ASCII") ||
            decode_detail::EqualsIgnoreCase(*charset, "ASCII")) {
            return decode_detail::DecodeAscii(result, *binary);
        }
        if (decode_detail::EqualsIgnoreCase(*charset, "ISO-8859-1") ||
            decode_detail::EqualsIgnoreCase(*charset, "LATIN1")) {
            return decode_detail::DecodeIso88591(result, *binary);
        }
        if (decode_detail::EqualsIgnoreCase(*charset, "UTF-8")) {
            return decode_detail::DecodeUtf8(result, *binary);
        }
        if (decode_detail::EqualsIgnoreCase(*charset, "UTF-16BE")) {
            return decode_detail::DecodeUtf16BE(result, *binary);
        }
        if (decode_detail::EqualsIgnoreCase(*charset, "UTF-16LE")) {
            return decode_detail::DecodeUtf16LE(result, *binary);
        }
        if (decode_detail::EqualsIgnoreCase(*charset, "UTF-16")) {
            return decode_detail::DecodeUtf16(result, *binary);
        }
        return false;
    }
};

}
