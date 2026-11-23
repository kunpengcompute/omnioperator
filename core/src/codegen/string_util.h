/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
 * Description: string some common operators
 */

#ifndef OMNI_RUNTIME_STRING_UTIL_H
#define OMNI_RUNTIME_STRING_UTIL_H

#include <iostream>
#include <string>
#include <memory>
#include <locale>
#include <codecvt>
#include "util/utf8_util.h"

namespace omniruntime::codegen::function {
static std::string REPLACE_ERR_MSG = "Replace failed";
static std::string CONCAT_ERR_MSG = "Concat failed";
static std::string CONCAT_WS_ERR_MSG = "ConcatWs failed";
static constexpr uint8_t EMPTY[] = "";
static constexpr int32_t NAIVE_SEARCH_THRESHOLD = 16;
static int32_t STEP = static_cast<int>('a') - static_cast<int>('A');
static uint8_t BytesOfCodePointInUTF8[] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 0x00..0F
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 0x10..1F
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 0x20..2F
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 0x30..3F
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 0x40..4F
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 0x50..5F
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 0x60..6F
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 0x70..7F
    // Consecutive bytes cannot be used as the first byte
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x80..8F
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x90..9F
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0xA0..AF
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0xB0..BF
    0, 0, // 0xC0..C1 - disallowed in UTF-8
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, // 0xC2..CF
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, // 0xD0..DF
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, // 0xE0..EF
    4, 4, 4, 4, 4, // 0xF0..F4
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 // 0xF5..FF - disallowed in UTF-8
};

static const char DIGIT_PAIRS[] =
    "00010203040506070809"
    "10111213141516171819"
    "20212223242526272829"
    "30313233343536373839"
    "40414243444546474849"
    "50515253545556575859"
    "60616263646566676869"
    "70717273747576777879"
    "80818283848586878889"
    "90919293949596979899";

static uint8_t digits[] = {
        '0' , '1' , '2' , '3' , '4' , '5' ,
        '6' , '7' , '8' , '9' , 'a' , 'b' ,
        'c' , 'd' , 'e' , 'f' , 'g' , 'h' ,
        'i' , 'j' , 'k' , 'l' , 'm' , 'n' ,
        'o' , 'p' , 'q' , 'r' , 's' , 't' ,
        'u' , 'v' , 'w' , 'x' , 'y' , 'z'
    };

class StringUtil {
public:
    static inline std::wstring ToWideString(const std::string &s)
    {
        std::wstring_convert<std::codecvt_utf8<wchar_t>> convert;
        return convert.from_bytes(s);
    }

    static inline const char *CastStrStr(bool *hasErr, const char *str, int32_t srcWidth, int32_t strLen,
        int32_t *outLen, int32_t dstWidth)
    {
        int32_t chCount = std::min(srcWidth, dstWidth);
        int32_t dstLen = 0;
        int32_t count = 0;
        while (dstLen < strLen && count < chCount) {
            int32_t charLen = omniruntime::Utf8Util::LengthOfCodePoint(str[dstLen]);
            if (charLen < 0) {
                *hasErr = true;
                *outLen = 0;
                return nullptr;
            }
            dstLen += charLen;
            count++;
        }
        *outLen = dstLen;
        return str;
    }

    static inline const char *ConcatCharDiffWidths(int64_t contextPtr, const char *ap, int32_t aWidth, int32_t apLen,
        const char *bp, int32_t bpLen, bool *hasErr, int32_t *outLen)
    {
        int32_t aPaddingCount = bpLen > 0 ? aWidth - omniruntime::Utf8Util::CountCodePoints(ap, apLen) : 0;
        *outLen = apLen + aPaddingCount + bpLen;
        if (*outLen <= 0) {
            *outLen = 0;
            return reinterpret_cast<const char *>(EMPTY);
        }

        // allocate one more byte is mainly for memcpy, when the copy source and destination are
        // both empty strings, the security function will not return an error.
        auto ret = ArenaAllocatorMalloc(contextPtr, *outLen + 1);
        memcpy(ret, ap, apLen);
        memset(ret + apLen, ' ', aPaddingCount);
        memcpy(ret + apLen + aPaddingCount, bp, bpLen);
        return ret;
    }

    static inline const char *ConcatStrDiffWidths(int64_t contextPtr, const char *ap, int32_t apLen, const char *bp,
        int32_t bpLen, bool *hasErr, int32_t *outLen)
    {
        *outLen = apLen + bpLen;
        if (*outLen <= 0) {
            *outLen = 0;
            return reinterpret_cast<const char *>(EMPTY);
        }
        // allocate one more byte is mainly for memcpy, when the copy source and destination are
        // both empty strings, the security function will not return an error.
        auto ret = ArenaAllocatorMalloc(contextPtr, *outLen + 1);
        memcpy(ret, ap, apLen);
        memcpy(ret + apLen, bp, bpLen);
        return ret;
    }

    static inline const char *ConcatWsStrDiffWidths(int64_t contextPtr, const char *separator, int32_t separatorLen,
        const char *ap, int32_t apLen, const char *bp, int32_t bpLen, bool *hasErr, int32_t *outLen)
    {
        *outLen = apLen + separatorLen + bpLen;
        if (*outLen <= 0) {
            *outLen = 0;
            return reinterpret_cast<const char *>(EMPTY);
        }
        auto ret = ArenaAllocatorMalloc(contextPtr, *outLen + 1);
        memcpy(ret, ap, apLen);
        memcpy(ret + apLen, separator, separatorLen);
        memcpy(ret + apLen + separatorLen, bp, bpLen);
        return ret;
    }

    static inline bool ConcatWsAppend(int64_t contextPtr, const char *separator, int32_t separatorLen,
        const char *&tmp, int32_t &tmpLen, bool &tmpSet, const char *s, int32_t sLen, bool *hasErr, int32_t *outLen)
    {
        if (!tmpSet) {
            tmp = s;
            tmpLen = sLen;
            tmpSet = true;
            *outLen = tmpLen;
            return true;
        }
        const char *next = ConcatWsStrDiffWidths(contextPtr, separator, separatorLen, tmp, tmpLen, s, sLen, hasErr, outLen);
        if (*hasErr) {
            return false;
        }
        tmp = next;
        tmpLen = *outLen;
        return true;
    }

    static inline const char *ReplaceWithSearchNotEmpty(int64_t contextPtr, const char *str, int32_t strLen,
        const char *searchStr, int32_t searchLen, const char *replaceStr, int32_t replaceLen, bool *hasErr,
        int32_t *outLen)
    {
        if (strLen == 0) {
            *outLen = 0;
            return reinterpret_cast<const char *>(EMPTY);
        }
        std::string s = std::string(str, strLen);
        std::string search = std::string(searchStr, searchLen);
        std::string replace = std::string(replaceStr, replaceLen);
        std::string::size_type matchIndex = 0;
        if (replaceLen == 0) {
            while ((matchIndex = s.find(search, matchIndex)) != std::string::npos) {
                s = s.substr(0, matchIndex) + s.substr(matchIndex + searchLen);
            }
        } else {
            while ((matchIndex = s.find(search, matchIndex)) != std::string::npos) {
                s.replace(matchIndex, searchLen, replace);
                matchIndex += replaceLen;
            }
        }

        *outLen = static_cast<int32_t>(s.length());
        auto ret = ArenaAllocatorMalloc(contextPtr, *outLen + 1);
        memcpy(ret, s.c_str(), s.length());
        return ret;
    }

    static inline const char *ReplaceWithSearchEmpty(int64_t contextPtr, const char *str, int32_t strLen,
        const char *replaceStr, int32_t replaceLen, bool *hasErr, int32_t *outLen)
    {
        int32_t strCodePoints = omniruntime::Utf8Util::CountCodePoints(str, strLen);
        *outLen = strLen + (strCodePoints + 1) * replaceLen;
        auto ret = ArenaAllocatorMalloc(contextPtr, *outLen + 1);
        int32_t indexBuffer = 0;
        for (int32_t index = 0; index < strLen;) {
            memcpy(ret + indexBuffer, replaceStr, replaceLen);
            indexBuffer += replaceLen;
            int32_t codePointLength = omniruntime::Utf8Util::LengthOfCodePoint(*(str + index));
            memcpy(ret + indexBuffer, str + index, codePointLength);
            indexBuffer += codePointLength;
            index += codePointLength;
        }
        memcpy(ret + indexBuffer, replaceStr, replaceLen);
        return ret;
    }

    static inline void TrimString(std::string &str)
    {
        str.erase(0, str.find_first_not_of(' '));
        str.erase(str.find_last_not_of(' ') + 1);
    }

    static inline bool StrContainsStr(const char *srcStr, int32_t srcLen, const char *matchStr, int32_t matchLen)
    {
        if (matchLen <= NAIVE_SEARCH_THRESHOLD) {
            const char *end = srcStr + (srcLen - matchLen);
            for (const char *p = srcStr; p <= end; ++p) {
                if (memcmp(p, matchStr, matchLen) == 0) {
                    return true;
                }
            }
            return false;
        }

        int next[matchLen];
        next[0] = -1;
        int i = 0;
        int j = -1;
        while (i < matchLen - 1) {
            if (j == -1 || matchStr[i] == matchStr[j]) {
                i++;
                j++;
                next[i] = j;
            } else {
                j = next[j];
            }
        }

        i = 0;
        j = 0;
        while (i < srcLen && j < matchLen) {
            if (j == -1 || srcStr[i] == matchStr[j]) {
                i++;
                j++;
            } else {
                j = next[j];
            }
        }

        return j == matchLen;
    }

    static inline int32_t NumChars(const char *str, int32_t strLen)
    {
        int32_t len = 0;
        int32_t i = 0;

        while (i < strLen) {
            len += 1;
            int32_t offset = str[i] & 0xFF;
            uint8_t numBytes = BytesOfCodePointInUTF8[offset];
            i += numBytes == 0 ? 1 : numBytes;
        }
        return len;
    }

    static inline void CopyChars(const char *srcStr, int32_t srcStrLen, int32_t startIdx, int32_t copyLen,
        char *outStr, int32_t outStrLen, bool *hasErr)
    {
        int32_t maxCopy = std::min({srcStrLen - startIdx, copyLen, outStrLen});
        memcpy(outStr, srcStr + startIdx, maxCopy);
    }

    static inline int32_t FindChar(const char *str, int32_t strLen, const char *searchChar, int32_t searchCharLen,
        int32_t start)
    {
        while (start <= strLen - searchCharLen) {
            if (std::memcmp(str + start, searchChar, searchCharLen) == 0) {
                return start;
            }
            start += 1;
        }
        return -1;
    }

    static inline std::string StringTrimLeft(const std::string &srcStr, const std::string &trimStr, bool *hasErr)
    {
        int32_t searchIdx = 0;
        int32_t trimIdx = 0;
        while (searchIdx < srcStr.size()) {
            int32_t searchCharBytes = NumBytesForFirstByte(srcStr[searchIdx]);
            char searchChar[searchCharBytes] = {0};
            CopyChars(srcStr.c_str(), srcStr.size(), searchIdx, searchCharBytes, searchChar, searchCharBytes, hasErr);
            if (FindChar(trimStr.c_str(), trimStr.size(), searchChar, searchCharBytes, 0) >= 0) {
                trimIdx += searchCharBytes;
            } else {
                break;
            }
            searchIdx += searchCharBytes;
        }
        if (searchIdx == 0) {
            return srcStr;
        }
        if (trimIdx >= srcStr.size()) {
            return "";
        }
        return srcStr.substr(trimIdx);
    }

    static inline std::string StringTrimRight(const std::string &srcStr, const std::string &trimStr, bool *hasErr)
    {
        int32_t charIdx = 0;
        int32_t numChars = 0;
        int32_t stringCharLen[srcStr.size()] = {};
        int32_t stringCharPos[srcStr.size()] = {};

        while (charIdx < srcStr.size()) {
            stringCharPos[numChars] = charIdx;
            stringCharLen[numChars] = NumBytesForFirstByte(srcStr[charIdx]);
            charIdx += stringCharLen[numChars];
            numChars++;
        }

        int32_t trimEnd = srcStr.size() - 1;
        while (numChars > 0) {
            int32_t searchIdx = stringCharPos[numChars - 1];
            int32_t searchCharBytes = stringCharLen[numChars - 1];
            char searchChar[searchCharBytes] = {0};
            CopyChars(srcStr.c_str(), srcStr.size(), searchIdx, searchCharBytes, searchChar, searchCharBytes, hasErr);
            if (FindChar(trimStr.c_str(), trimStr.size(), searchChar, searchCharBytes, 0) >= 0) {
                trimEnd -= searchCharBytes;
            } else {
                break;
            }
            numChars --;
        }
        if (trimEnd == srcStr.size() - 1) {
            return srcStr;
        }
        if (trimEnd < 0) {
            return "";
        }
        return srcStr.substr(0, trimEnd + 1);
    }

    static inline const char *Trim(int64_t contextPtr, const char *srcStr, int32_t srcStrLen, const char *trimStr,
        int32_t trimStrLen, bool *hasErr, int32_t *outLen)
    {
        std::string src = std::string(srcStr, srcStrLen);
        std::string trim = std::string(trimStr, trimStrLen);
        std::string result = StringTrimRight(StringTrimLeft(src, trim, hasErr), trim, hasErr);
        if (*hasErr) {
        *outLen = 0;
        return nullptr;
        }
        *outLen = static_cast<int32_t>(result.length());
        auto ret = ArenaAllocatorMalloc(contextPtr, *outLen + 1);
        memcpy(ret, result.c_str(), result.length());
        return ret;
    }

    static inline const char *LTrim(int64_t contextPtr, const char *srcStr, int32_t srcStrLen, const char *trimStr,
        int32_t trimStrLen, bool *hasErr, int32_t *outLen)
    {
        std::string src = std::string(srcStr, srcStrLen);
        std::string trim = std::string(trimStr, trimStrLen);
        std::string result = StringTrimLeft(src, trim, hasErr);
        if (*hasErr) {
        *outLen = 0;
        return nullptr;
        }
        *outLen = static_cast<int32_t>(result.length());
        auto ret = ArenaAllocatorMalloc(contextPtr, *outLen + 1);
        memcpy(ret, result.c_str(), result.length()); 
        return ret;
    }

    static inline const char *RTrim(int64_t contextPtr, const char *srcStr, int32_t srcStrLen, const char *trimStr,
        int32_t trimStrLen, bool *hasErr, int32_t *outLen)
    {
        std::string src = std::string(srcStr, srcStrLen);
        std::string trim = std::string(trimStr, trimStrLen);
        std::string result = StringTrimRight(src, trim, hasErr);
        if (*hasErr) {
        *outLen = 0;
        return nullptr;
        }
        *outLen = static_cast<int32_t>(result.length());
        auto ret = ArenaAllocatorMalloc(contextPtr, *outLen + 1);
        memcpy(ret, result.c_str(), result.length()); 
        return ret;
    }

    static inline int32_t NumBytesForFirstByte(const char c)
    {
        uint8_t offset = c & 0xFF;
        uint8_t numBytes = BytesOfCodePointInUTF8[offset];
        return (numBytes == 0) ? 1 : numBytes;
    }

// Requires positive x
    static int stringSize(int64_t x) {
        if (x < 10) return 1;
        if (x < 100) return 2;
        if (x < 1000) return 3;
        if (x < 10000) return 4;
        if (x < 100000) return 5;
        if (x < 1000000) return 6;
        if (x < 10000000) return 7;
        if (x < 100000000) return 8;
        if (x < 1000000000) return 9;
        if (x < 10000000000LL) return 10;
        if (x < 100000000000LL) return 11;
        if (x < 1000000000000LL) return 12;
        if (x < 10000000000000LL) return 13;
        if (x < 100000000000000LL) return 14;
        if (x < 1000000000000000LL) return 15;
        if (x < 10000000000000000LL) return 16;
        if (x < 100000000000000000LL) return 17;
        if (x < 1000000000000000000LL) return 18;
        return 19;
    }

    static void getChars(int64_t i, int index, char *buf) {
         int64_t q;
         int32_t r;
         int32_t charPos = index;
         char sign = 0;

         if (i < 0) {
             sign = '-';
             i = -i;
         }

         // Get 2 digits/iteration using longs until quotient fits into an int
         while (i > std::numeric_limits<int32_t>::max()) {
             q = i / 100;
             // really: r = i - (q * 100);
             r = static_cast<int32_t>(i - ((q << 6) + (q << 5) + (q << 2)));
             i = q;
             buf[--charPos] = DIGIT_PAIRS[r<<1 | 1];
             buf[--charPos] = DIGIT_PAIRS[r<<1];
         }

         // Get 2 digits/iteration using ints
         int q2;
         int i2 = (int)i;
         while (i2 >= 65536) {
             q2 = i2 / 100;
             // really: r = i2 - (q * 100);
             r = i2 - ((q2 << 6) + (q2 << 5) + (q2 << 2));
             i2 = q2;
             buf[--charPos] = DIGIT_PAIRS[r<<1 | 1];
             buf[--charPos] = DIGIT_PAIRS[r<<1];
         }

         // Fall thru to fast mode for smaller numbers
         // assert(i2 <= 65536, i2);
         for (;;) {
             q2 = (i2 * 52429U) >> 19;
             r = i2 - ((q2 << 3) + (q2 << 1));  // r = i2-(q2*10) ...
             buf[--charPos] = digits[r];
             i2 = q2;
             if (i2 == 0) break;
         }
         if (sign != 0) {
             buf[--charPos] = sign;
         }
    }

    /*
     params: i a long to be converted into buf
     returns: actual use size
    */
    static int32_t toString(int64_t i, char *buf) {
        int32_t size = (i < 0) ? stringSize(-i) + 1 : stringSize(i);
        getChars(i, size, buf);
        return size;
    }
}; // class stringUtils
} // namespace codegen function

#endif // OMNI_RUNTIME_STRING_UTIL_H
