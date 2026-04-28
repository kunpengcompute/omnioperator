/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2025. All rights reserved.
 * Description: registry  function
 */
#ifndef __STRINGFUNCTIONS_H__
#define __STRINGFUNCTIONS_H__

#include <iostream>
#include <string>
#include <memory>
#include <algorithm>
#include <cstring>
#include <regex>
#include <cmath>
#include "codegen/context_helper.h"
#include "codegen/functions/decimal_arithmetic_functions.h"
#include "codegen/functions/decimal_cast_functions.h"
#include "util/utf8_util.h"
#include "codegen/string_util.h"
#include "type/date32.h"

// All extern functions go here temporarily
#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

namespace omniruntime::codegen::function {
extern "C" DLLEXPORT int64_t CountChar(const char *str, int32_t strLen, const char *target, int32_t targetWidth, int32_t targetLen, bool isNull);

extern "C" DLLEXPORT const char* SplitIndexRetNull(const char *str, int32_t strLen, bool strIsNull, const char *target,
                                                   int32_t targetWidth, int32_t targetLen, bool targetIsNull, int32_t index,
                                                   bool indexIsNull, bool *outIsNull, int32_t *outLen);

extern "C" DLLEXPORT bool StrEquals(const char *ap, int32_t apLen, const char *bp, int32_t bpLen);

extern "C" DLLEXPORT int32_t StrCompare(const char *ap, int32_t apLen, const char *bp, int32_t bpLen);

extern "C" DLLEXPORT bool LikeStr(const char *str, int32_t strLen, const char *regexToMatch, int32_t regexLen,
    bool isNull);

extern "C" DLLEXPORT bool LikeChar(const char *str, int32_t strWidth, int32_t strLen, const char *regexToMatch,
    int32_t regexLen, bool isNull);

extern "C" DLLEXPORT const char* RegexpExtractRetNull(int64_t contextPtr, const char *str, int32_t strLen, bool strIsNull,
                                                      const char *regexToMatch, int32_t regexWidth, int32_t regexLen,
                                                      bool regexIsNull, int32_t group, bool groupIsNull, bool *outIsNull, int32_t *outLen);

extern "C" DLLEXPORT const char* JsonValueRetNull(int64_t contextPtr, const char *jsonStr, int32_t jsonStrLen, bool jsonStrIsNull,
                                                   const char *pathStr, int32_t pathStrWidth, int32_t pathStrLen, bool pathStrIsNull,
                                                   bool *outIsNull, int32_t *outLen);

extern "C" DLLEXPORT const char* JsonQueryRetNull(int64_t contextPtr, const char *jsonStr, int32_t jsonStrLen, bool jsonStrIsNull,
                                                   const char *pathStr, int32_t pathStrWidth, int32_t pathStrLen, bool pathStrIsNull,
                                                   bool *outIsNull, int32_t *outLen);

// Extended JSON_VALUE function with ON EMPTY/ERROR behaviors
// emptyBehavior: 0=NULL, 1=ERROR, 2=DEFAULT
// errorBehavior: 0=NULL, 1=ERROR, 2=DEFAULT
extern "C" DLLEXPORT const char* JsonValueExtended(
    int64_t contextPtr,
    const char *jsonStr, int32_t jsonStrLen, bool jsonStrIsNull,
    const char *pathStr, int32_t pathStrWidth, int32_t pathStrLen, bool pathStrIsNull,
    int32_t emptyBehavior, const char *defaultOnEmpty, int32_t defaultOnEmptyLen, bool defaultOnEmptyIsNull,
    int32_t errorBehavior, const char *defaultOnError, int32_t defaultOnErrorLen, bool defaultOnErrorIsNull,
    bool *outIsNull, int32_t *outLen);


// JSON_SPLIT_SCALAR function for splitting JSON array and joining all elements
// Returns all elements joined with CRLF delimiter (matches jsontest UDF semantics)
// This function takes only 1 argument (the JSON string) and returns a single VARCHAR
extern "C" DLLEXPORT const char* JsonSplitScalar(
    int64_t contextPtr,
    const char *jsonStr, int32_t jsonStrLen, bool jsonStrIsNull,
    bool *outIsNull, int32_t *outLen);

extern "C" DLLEXPORT const char *ConcatStrStr(int64_t contextPtr, const char *ap, int32_t apLen, const char *bp,
    int32_t bpLen, bool isNull, int32_t *outLen);

extern "C" DLLEXPORT const char *ConcatCharChar(int64_t contextPtr, const char *ap, int32_t aWidth, int32_t apLen,
    const char *bp, int32_t bWidth, int32_t bpLen, bool isNull, int32_t *outLen);

extern "C" DLLEXPORT const char *ConcatCharStr(int64_t contextPtr, const char *ap, int32_t aWidth, int32_t apLen,
    const char *bp, int32_t bpLen, bool isNull, int32_t *outLen);

extern "C" DLLEXPORT const char *ConcatStrChar(int64_t contextPtr, const char *ap, int32_t apLen, const char *bp,
    int32_t bWidth, int32_t bpLen, bool isNull, int32_t *outLen);

extern "C" DLLEXPORT const char *ConcatWsWithoutStr(int64_t contextPtr, const char *separator, int32_t separatorLen,
    bool separatorIsNull, bool *retIsNull, int32_t *outLen);

extern "C" DLLEXPORT const char *ConcatWsWith1Str(int64_t contextPtr, const char *separator, int32_t separatorLen,
    bool separatorIsNull, const char *s1, int32_t s1Len, bool s1IsNull, bool *retIsNull, int32_t *outLen);

extern "C" DLLEXPORT const char *ConcatWsStr(int64_t contextPtr, const char *separator, int32_t separatorLen,
    bool separatorIsNull, const char *s1, int32_t s1Len, bool s1IsNull, const char *s2, int32_t s2Len, bool s2IsNull,
    bool *retIsNull, int32_t *outLen);

extern "C" DLLEXPORT const char *ConcatWs3Str(int64_t contextPtr, const char *separator, int32_t separatorLen,
    bool separatorIsNull, const char *s1, int32_t s1Len, bool s1IsNull, const char *s2, int32_t s2Len, bool s2IsNull,
    const char *s3, int32_t s3Len, bool s3IsNull, bool *retIsNull, int32_t *outLen);

extern "C" DLLEXPORT const char *ConcatWs4Str(int64_t contextPtr, const char *separator, int32_t separatorLen,
    bool separatorIsNull, const char *s1, int32_t s1Len, bool s1IsNull, const char *s2, int32_t s2Len, bool s2IsNull,
    const char *s3, int32_t s3Len, bool s3IsNull, const char *s4, int32_t s4Len, bool s4IsNull, bool *retIsNull,
    int32_t *outLen);

extern "C" DLLEXPORT const char *ConcatWs5Str(int64_t contextPtr, const char *separator, int32_t separatorLen,
    bool separatorIsNull, const char *s1, int32_t s1Len, bool s1IsNull, const char *s2, int32_t s2Len, bool s2IsNull,
    const char *s3, int32_t s3Len, bool s3IsNull, const char *s4, int32_t s4Len, bool s4IsNull, const char *s5,
    int32_t s5Len, bool s5IsNull, bool *retIsNull, int32_t *outLen);

extern "C" DLLEXPORT int32_t CastStringToDateNotAllowReducePrecison(int64_t contextPtr, const char *str, int32_t strLen,
    bool isNull);

extern "C" DLLEXPORT int32_t CastStringToDateAllowReducePrecison(int64_t contextPtr, const char *str, int32_t strLen,
    bool isNull);

// Cast numeric type to string
extern "C" DLLEXPORT const char *CastIntToString(int64_t contextPtr, int32_t value, bool isNull, int32_t *outLen);

extern "C" DLLEXPORT const char *CastInt16ToString(int64_t contextPtr, int16_t value, bool isNull, int32_t *outLen);

extern "C" DLLEXPORT const char *CastInt8ToString(int64_t contextPtr, int8_t value, bool isNull, int32_t *outLen);

extern "C" DLLEXPORT const char *CastLongToString(int64_t contextPtr, int64_t value, bool isNull, int32_t *outLen);

extern "C" DLLEXPORT const char *CastDoubleToString(int64_t contextPtr, double value, bool isNull, int32_t *outLen);

extern "C" DLLEXPORT const char *CastDecimal64ToString(int64_t contextPtr, int64_t x, int32_t precision, int32_t scale,
    bool isNull, int32_t *outLen);

extern "C" DLLEXPORT const char *CastDecimal128ToString(int64_t contextPtr, int64_t high, uint64_t low,
    int32_t precision, int32_t scale, bool isNull, int32_t *outLen);

extern "C" DLLEXPORT const char *CastStrWithDiffWidths(int64_t contextPtr, const char *srcStr, int32_t srcLen,
    int32_t srcWidth, bool isNull, int32_t dstWidth, int32_t *outLen);

// Cast string to numeric type

extern "C" DLLEXPORT const char *CastStringToBinary(int64_t contextPtr, const char *str, int32_t strLen, bool isNull,
    int32_t *outLen);

extern "C" DLLEXPORT int8_t CastStringToByte(int64_t contextPtr, const char *str, int32_t strLen, bool isNull);

extern "C" DLLEXPORT int16_t CastStringToShort(int64_t contextPtr, const char *str, int32_t strLen, bool isNull);

extern "C" DLLEXPORT int32_t CastStringToInt(int64_t contextPtr, const char *str, int32_t strLen, bool isNull);

extern "C" DLLEXPORT int64_t CastStringToLong(int64_t contextPtr, const char *str, int32_t strLen, bool isNull);

extern "C" DLLEXPORT double CastStringToDouble(int64_t contextPtr, const char *str, int32_t strLen, bool isNull);

extern "C" DLLEXPORT int64_t CastStringToDecimal64(int64_t contextPtr, const char *str, int32_t strLen, bool isNull,
    int32_t precision, int32_t scale);

extern "C" DLLEXPORT int64_t CastStringToDecimal64RoundUp(int64_t contextPtr, const char *str, int32_t strLen,
    bool isNull, int32_t outPrecision, int32_t outScale);

extern "C" DLLEXPORT void CastStringToDecimal128(int64_t contextPtr, const char *str, int32_t strLen, bool isNull,
    int32_t outPrecision, int32_t outScale, int64_t *outHighPtr, uint64_t *outLowPtr);

extern "C" DLLEXPORT void CastStringToDecimal128RoundUp(int64_t contextPtr, const char *str, int32_t strLen,
    bool isNull, int32_t outPrecision, int32_t outScale, int64_t *outHighPtr, uint64_t *outLowPtr);

extern "C" DLLEXPORT const char *EmptyToNull(const char *str, int32_t len, bool isNull, int32_t *outLen);

extern "C" DLLEXPORT const char *StaticInvokeVarcharTypeWriteSideCheck(int64_t contextPtr, const char *str, int32_t len,
    int32_t limit, bool isNull, int32_t *outLen);

extern "C" DLLEXPORT const char *StaticInvokeCharTypeWriteSideCheck(int64_t contextPtr, const char *str, int32_t len,
    int32_t limit, bool isNull, int32_t *outLen);

extern "C" DLLEXPORT const char *StaticInvokeCharReadPadding(int64_t contextPtr, const char *str,
    int32_t len, int32_t limit, bool isNull, int32_t *outLen);

extern "C" DLLEXPORT int64_t CastStringToTimestamp(int64_t contextPtr, const char *str, int32_t strLen, bool isNull);

extern "C" DLLEXPORT int64_t CastStringToTimestampReturnNull(bool *isNull, const char *str, int32_t strLen);

/* *
 * If isSupportNegativeIndex is false,the result of substr is "" when start index is negative
 * If isSupportNegativeIndex is true,the substr rule is as follows:
 * e.g., str="apple", strLength=5, startIndex=-7, subStringLength=3, Result="a".
 * If isSupportZeroIndex is false,the result of substr is "" when start index is 0
 * If isSupportZeroIndex is true,it refers to the first element when the start index is 0
 */
template <typename T, bool isSupportNegativeIndex, bool isSupportZeroIndex>
extern DLLEXPORT const char *SubstrVarcharWithStart(int64_t contextPtr, const char *str, int32_t strLen, T startIdx,
    bool isNull, int32_t *outLen)
{
    if (isNull) {
        return nullptr;
    }
    if constexpr (isSupportZeroIndex) {
        startIdx = (startIdx == 0) ? 1 : startIdx;
    }
    if (startIdx == 0 || strLen == 0 || startIdx > strLen) {
        *outLen = 0;
        return reinterpret_cast<const char *>(EMPTY);
    }

    int64_t startCodePoint = startIdx;
    int64_t startIndex;
    if (startCodePoint > 0) {
        startIndex = omniruntime::Utf8Util::OffsetOfCodePoint(str, strLen, startCodePoint - 1);
        if (startIndex < 0) {
            *outLen = 0;
            return reinterpret_cast<const char *>(EMPTY);
        }
    } else {
        // negative start is relative to end of string
        int32_t codePoints = omniruntime::Utf8Util::CountCodePoints(str, strLen);
        startCodePoint += codePoints;
        if (startCodePoint < 0) {
            if constexpr (isSupportNegativeIndex) {
                startCodePoint = 0;
            } else {
                *outLen = 0;
                return reinterpret_cast<const char *>(EMPTY);
            }
        }

        startIndex = omniruntime::Utf8Util::OffsetOfCodePoint(str, strLen, startCodePoint);
    }

    *outLen = strLen - startIndex;
    return str + startIndex;
}

template <typename T, bool isSupportNegativeIndex, bool isSupportZeroIndex>
extern DLLEXPORT const char *SubstrVarchar(int64_t contextPtr, const char *str, int32_t strLen, T startIdx, T length,
    bool isNull, int32_t *outLen)
{
    if (isNull) {
        return nullptr;
    }

    if constexpr (isSupportZeroIndex) {
        startIdx = (startIdx == 0) ? 1 : startIdx;
    }
    if (startIdx == 0 || (length <= 0) || (strLen == 0) || startIdx > strLen) {
        *outLen = 0;
        return reinterpret_cast<const char *>(EMPTY);
    }

    int64_t endIdx;
    int64_t startIndex;
    int64_t startCodePoint = startIdx;
    int64_t lengthCodePoint = length;
    if (startCodePoint > 0) {
        startIndex = omniruntime::Utf8Util::OffsetOfCodePoint(str, strLen, startCodePoint - 1);
        if (startIndex < 0) {
            // before beginning of string
            *outLen = 0;
            return reinterpret_cast<const char *>(EMPTY);
        }
        endIdx = omniruntime::Utf8Util::OffsetOfCodePoint(str, strLen, startIndex, lengthCodePoint);
        if (endIdx < 0) {
            // after end of string
            endIdx = strLen;
        }
    } else {
        // negative start is relative to end of string
        int32_t codePoints = omniruntime::Utf8Util::CountCodePoints(str, strLen);
        startCodePoint += codePoints;
        // before beginning of string
        if (startCodePoint < 0) {
            if constexpr (!isSupportNegativeIndex) {
                *outLen = 0;
                return reinterpret_cast<const char *>(EMPTY);
            }
            if (startCodePoint + lengthCodePoint <= 0) {
                *outLen = 0;
                return reinterpret_cast<const char *>(EMPTY);
            }
            lengthCodePoint += startCodePoint;
            startCodePoint = 0;
        }
        startIndex = omniruntime::Utf8Util::OffsetOfCodePoint(str, strLen, startCodePoint);
        if (startCodePoint + lengthCodePoint < codePoints) {
            endIdx = omniruntime::Utf8Util::OffsetOfCodePoint(str, strLen, startIndex, lengthCodePoint);
        } else {
            endIdx = strLen;
        }
    }

    *outLen = endIdx - startIndex;
    return str + startIndex;
}

template <typename T, bool isSupportNegativeIndex, bool isSupportZeroIndex>
extern DLLEXPORT const char *SubstrChar(int64_t contextPtr, const char *str, int32_t width, int32_t strLen, T startIdx,
    T length, bool isNull, int32_t *outLen)
{
    return SubstrVarchar<T, isSupportNegativeIndex, isSupportZeroIndex>(contextPtr, str, strLen, startIdx, length,
        isNull, outLen);
}

template <typename T, bool isSupportNegativeIndex, bool isSupportZeroIndex>
extern DLLEXPORT const char *SubstrCharWithStart(int64_t contextPtr, const char *str, int32_t width, int32_t strLen,
    T startIdx, bool isNull, int32_t *outLen)
{
    return SubstrVarcharWithStart<T, isSupportNegativeIndex, isSupportZeroIndex>(contextPtr, str, strLen, startIdx,
        isNull, outLen);
}

extern "C" DLLEXPORT const char *ToUpperStr(int64_t contextPtr, const char *str, int32_t strLen, bool isNull,
    int32_t *outLen);

extern "C" DLLEXPORT const char *ToUpperChar(int64_t contextPtr, const char *str, int32_t width, int32_t strLen,
    bool isNull, int32_t *outLen);

extern "C" DLLEXPORT const char *ToLowerStr(int64_t contextPtr, const char *str, int32_t strLen, bool isNull,
    int32_t *outLen);

extern "C" DLLEXPORT const char *ToLowerChar(int64_t contextPtr, const char *str, int32_t width, int32_t strLen,
    bool isNull, int32_t *outLen);

extern "C" DLLEXPORT int64_t LengthChar(const char *str, int32_t width, int32_t strLen, bool isNull);

extern "C" DLLEXPORT int32_t LengthCharReturnInt32(const char *str, int32_t width, int32_t strLen, bool isNull);

extern "C" DLLEXPORT int32_t LengthStrReturnInt32(const char *str, int32_t strLen, bool isNull);

extern "C" DLLEXPORT int64_t LengthStr(const char *str, int32_t strLen, bool isNull);

extern "C" DLLEXPORT const char *ReplaceStrStrStrWithRepNotReplace(int64_t contextPtr, const char *str, int32_t strLen,
    const char *searchStr, int32_t searchLen, const char *replaceStr, int32_t replaceLen, bool isNull, int32_t *outLen);

extern "C" DLLEXPORT const char *ReplaceStrStrStrWithRepReplace(int64_t contextPtr, const char *str, int32_t strLen,
    const char *searchStr, int32_t searchLen, const char *replaceStr, int32_t replaceLen, bool isNull, int32_t *outLen);

extern "C" DLLEXPORT const char *ReplaceStrStrWithoutRepReplace(int64_t contextPtr, const char *str, int32_t strLen,
    const char *searchStr, int32_t searchLen, bool isNull, int32_t *outLen);

extern "C" DLLEXPORT const char *ReplaceStrStrWithoutRepNotReplace(int64_t contextPtr, const char *str, int32_t strLen,
    const char *searchStr, int32_t searchLen, bool isNull, int32_t *outLen);

extern "C" DLLEXPORT const char *ConcatStrStrRetNull(int64_t contextPtr, bool *isNull, const char *ap, int32_t apLen,
    const char *bp, int32_t bpLen, int32_t *outLen);

extern "C" DLLEXPORT const char *ConcatCharCharRetNull(int64_t contextPtr, bool *isNull, const char *ap, int32_t aWidth,
    int32_t apLen, const char *bp, int32_t bWidth, int32_t bpLen, int32_t *outLen);

extern "C" DLLEXPORT const char *ConcatCharStrRetNull(int64_t contextPtr, bool *isNull, const char *ap, int32_t aWidth,
    int32_t apLen, const char *bp, int32_t bpLen, int32_t *outLen);

extern "C" DLLEXPORT const char *ConcatStrCharRetNull(int64_t contextPtr, bool *isNull, const char *ap, int32_t apLen,
    const char *bp, int32_t bWidth, int32_t bpLen, int32_t *outLen);

extern "C" DLLEXPORT int32_t CastStringToDateRetNullAllowReducePrecison(bool *isNull, const char *str, int32_t strLen);

extern "C" DLLEXPORT int32_t CastStringToDateRetNullNotAllowReducePrecison(bool *isNull, const char *str,
    int32_t strLen);

extern "C" DLLEXPORT const char *CastIntToStringRetNull(int64_t contextPtr, bool *isNull, int32_t value,
    int32_t *outLen);

extern "C" DLLEXPORT const char *CastInt16ToStringRetNull(int64_t contextPtr, bool *isNull, int16_t value,
    int32_t *outLen);

extern "C" DLLEXPORT const char *CastInt8ToStringRetNull(int64_t contextPtr, bool *isNull, int8_t value,
    int32_t *outLen);

extern "C" DLLEXPORT const char *CastLongToStringRetNull(int64_t contextPtr, bool *isNull, int64_t value,
    int32_t *outLen);

extern "C" DLLEXPORT const char *CastDoubleToStringRetNull(int64_t contextPtr, bool *isNull, double value,
    int32_t *outLen);

extern "C" DLLEXPORT const char *CastDecimal64ToStringRetNull(int64_t contextPtr, bool *isNull, int64_t x,
    int32_t precision, int32_t scale, int32_t *outLen);

extern "C" DLLEXPORT const char *CastDecimal128ToStringRetNull(int64_t contextPtr, bool *isNull, int64_t high,
    uint64_t low, int32_t precision, int32_t scale, int32_t *outLen);


extern "C" DLLEXPORT int8_t CastStringToByteRetNull(bool *isNull, const char *str, int32_t strLen);

extern "C" DLLEXPORT int16_t CastStringToShortRetNull(bool *isNull, const char *str, int32_t strLen);

extern "C" DLLEXPORT int32_t CastStringToIntRetNull(bool *isNull, const char *str, int32_t strLen);

extern "C" DLLEXPORT int64_t CastStringToLongRetNull(bool *isNull, const char *str, int32_t strLen);

extern "C" DLLEXPORT double CastStringToDoubleRetNull(bool *isNull, const char *str, int32_t strLen);

extern "C" DLLEXPORT int64_t CastStringToDecimal64RetNull(bool *isNull, const char *str, int32_t strLen,
    int32_t outPrecision, int32_t outScale);

extern "C" DLLEXPORT int64_t CastStringToDecimal64RoundUpRetNull(bool *isNull, const char *str, int32_t strLen,
    int32_t outPrecision, int32_t outScale);

extern "C" DLLEXPORT void CastStringToDecimal128RetNull(bool *isNull, const char *str, int32_t strLen,
    int32_t outPrecision, int32_t outScale, int64_t *outHighPtr, uint64_t *outLowPtr);

extern "C" DLLEXPORT void CastStringToDecimal128RoundUpRetNull(bool *isNull, const char *str, int32_t strLen,
    int32_t outPrecision, int32_t outScale, int64_t *outHighPtr, uint64_t *outLowPtr);

extern "C" DLLEXPORT const char *CastStrWithDiffWidthsRetNull(int64_t contextPtr, bool *isNull, const char *srcStr,
    int32_t srcLen, int32_t srcWidth, int32_t dstWidth, int32_t *outLen);

extern "C" DLLEXPORT int32_t InStr(const char *srcStr, int32_t srcLen, const char *subStr, int32_t subLen, bool isNull);

extern "C" DLLEXPORT bool StartsWithStr(const char *srcStr, int32_t srcLen, const char *matchStr, int32_t matchLen,
    bool isNull);

extern "C" DLLEXPORT bool EndsWithStr(const char *srcStr, int32_t srcLen, const char *matchStr, int32_t matchLen,
    bool isNull);

extern "C" DLLEXPORT bool RegexMatch(const char *srcStr, int32_t srcLen, const char *matchStr, int32_t matchLen,
    bool isNull);

extern "C" DLLEXPORT const char *CastDateToStringRetNull(int64_t contextPtr, bool *isNull, int32_t value,
    int32_t *outLen);

extern "C" DLLEXPORT const char *CastDateToString(int64_t contextPtr, int32_t value, bool isNull, int32_t *outLen);

extern "C" DLLEXPORT char *Md5Str(int64_t contextPtr, const char *str, int32_t len, bool isNull, int32_t *outLen);

extern "C" DLLEXPORT bool ContainsStr(const char *srcStr, int32_t srcLen, const char *matchStr, int32_t matchLen,
    bool isNull);

extern "C" DLLEXPORT const char *GreatestStr(const char *lValue, int32_t lLen, bool lIsNull, const char *rValue,
    int32_t rLen, bool rIsNull, bool *retIsNull, int32_t *outLen);

extern "C" DLLEXPORT const char *SubstringIndex(int64_t contextPtr, const char *str, int32_t strLen, const char *delim,
    int32_t delimLen, int32_t count, bool isNull, int32_t *outLen);

extern "C" DLLEXPORT const char *Re2SearchAndExtract(int64_t contextPtr, const char *str, int32_t strLen, const char *pattern,
    int32_t patternLen, int32_t idx, bool isNull, int32_t *outLen);

extern "C" DLLEXPORT const char *RegexpReplace(int64_t contextPtr, const char *stringInput, int32_t stringInputLen,
    const char *pattern, int32_t patternLen, const char *replacement, int32_t replacementLen, int32_t count,
    bool isNull, int32_t *outLen);

extern "C" DLLEXPORT const char *TrimStr(int64_t contextPtr, const char *trimStr, int32_t trimStrLen,
    const char *srcStr, int32_t srcStrLen, bool isNull, int32_t *outLen);

extern "C" DLLEXPORT const char *Trim1Str(int64_t contextPtr, const char *srcStr, int32_t srcStrLen, bool isNull,
    int32_t *outLen);

extern "C" DLLEXPORT const char *LeftTrimStr(int64_t contextPtr, const char *trimStr, int32_t trimStrLen,
    const char *srcStr, int32_t srcStrLen, bool isNull, int32_t *outLen);

extern "C" DLLEXPORT const char *RightTrimStr(int64_t contextPtr, const char *trimStr, int32_t trimStrLen,
    const char *srcStr, int32_t srcStrLen, bool isNull, int32_t *outLen);
}
#endif