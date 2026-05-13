/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2024. All rights reserved.
 * Description: batch string functions implementation
 */
#ifndef OMNI_RUNTIME_BATCH_STRINGFUNCTIONS_H
#define OMNI_RUNTIME_BATCH_STRINGFUNCTIONS_H

#include <iostream>
#include <string>
#include <memory>
#include <locale>
#include <codecvt>
#include "util/utf8_util.h"
#include "codegen/context_helper.h"
#include "codegen/string_util.h"
#include "type/decimal128.h"
#include "type/decimal_operations.h"
#include "util/config_util.h"

#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

using namespace omniruntime::type;
namespace omniruntime::codegen::function {
extern "C" DLLEXPORT void BatchCountChar(uint8_t **str, int32_t *strLen, uint8_t **target, int32_t targetWidth,
                                         int32_t *targetLen, bool *isAnyNull, int64_t *output, int32_t rowCnt);

extern "C" DLLEXPORT void BatchSplitIndex(uint8_t **str, int32_t *strLen, uint8_t **target, int32_t targetWidth,
                                          int32_t *targetLen, int32_t *index, bool *isAnyNull, uint8_t **output,
                                          int32_t *outLen, int32_t rowCnt);

// string compare functions
extern "C" DLLEXPORT void BatchStrCompare(uint8_t **ap, int32_t *apLen, uint8_t **bp, int32_t *bpLen, int32_t *res,
    int32_t rowCnt);

extern "C" DLLEXPORT void BatchLessThanStr(uint8_t **ap, int32_t *apLen, uint8_t **bp, int32_t *bpLen, bool *res,
    int32_t rowCnt);

extern "C" DLLEXPORT void BatchLessThanEqualStr(uint8_t **ap, int32_t *apLen, uint8_t **bp, int32_t *bpLen, bool *res,
    int32_t rowCnt);

extern "C" DLLEXPORT void BatchGreaterThanStr(uint8_t **ap, int32_t *apLen, uint8_t **bp, int32_t *bpLen, bool *res,
    int32_t rowCnt);

extern "C" DLLEXPORT void BatchGreaterThanEqualStr(uint8_t **ap, int32_t *apLen, uint8_t **bp, int32_t *bpLen,
    bool *res, int32_t rowCnt);

extern "C" DLLEXPORT void BatchEqualStr(uint8_t **ap, int32_t *apLen, uint8_t **bp, int32_t *bpLen, bool *res,
    int32_t rowCnt);

extern "C" DLLEXPORT void BatchNotEqualStr(uint8_t **ap, int32_t *apLen, uint8_t **bp, int32_t *bpLen, bool *res,
    int32_t rowCnt);

extern "C" DLLEXPORT void BatchCastStringToDateAllowReducePrecison(int64_t contextPtr, uint8_t **str, int32_t *strLen,
    bool *isAnyNull, int32_t *output, int32_t rowCnt);

extern "C" DLLEXPORT void BatchCastStringToDateNotAllowReducePrecison(int64_t contextPtr, uint8_t **str,
    int32_t *strLen, bool *isAnyNull, int32_t *output, int32_t rowCnt);

extern "C" DLLEXPORT void BatchCastStringToDateRetNullNotAllowReducePrecison(bool *isNull, uint8_t **str,
    int32_t *strLen, int32_t *output, int32_t rowCnt);

extern "C" DLLEXPORT void BatchCastStringToDateRetNullAllowReducePrecison(bool *isNull, uint8_t **str, int32_t *strLen,
    int32_t *output, int32_t rowCnt);

extern "C" DLLEXPORT void BatchCastIntToString(int64_t contextPtr, int32_t *value, bool *isAnyNull, uint8_t **output,
    int32_t *outLen, int32_t rowCnt);

extern "C" DLLEXPORT void BatchCastLongToString(int64_t contextPtr, int64_t *value, bool *isAnyNull, uint8_t **output,
    int32_t *outLen, int32_t rowCnt);

extern "C" DLLEXPORT void BatchCastDoubleToString(int64_t contextPtr, double *value, bool *isAnyNull, uint8_t **output,
    int32_t *outLen, int32_t rowCnt);

extern "C" DLLEXPORT void BatchCastDecimal64ToString(int64_t contextPtr, int64_t *x, int32_t precision, int32_t scale,
    bool *isAnyNull, uint8_t **output, int32_t *outLen, int32_t rowCnt);

extern "C" DLLEXPORT void BatchCastDecimal128ToString(int64_t contextPtr, Decimal128 *x, int32_t precision,
    int32_t scale, bool *isAnyNull, uint8_t **output, int32_t *outLen, int32_t rowCnt);

extern "C" DLLEXPORT void BatchCastIntToStringRetNull(bool *isNull, int64_t contextPtr, int32_t *value,
    uint8_t **output, int32_t *outLen, int32_t rowCnt);

extern "C" DLLEXPORT void BatchCastLongToStringRetNull(bool *isNull, int64_t contextPtr, int64_t *value,
    uint8_t **output, int32_t *outLen, int32_t rowCnt);

extern "C" DLLEXPORT void BatchCastDoubleToStringRetNull(bool *isNull, int64_t contextPtr, double *value,
    uint8_t **output, int32_t *outLen, int32_t rowCnt);

extern "C" DLLEXPORT void BatchCastDecimal64ToStringRetNull(bool *isNull, int64_t contextPtr, int64_t *x,
    int32_t precision, int32_t scale, uint8_t **output, int32_t *outLen, int32_t rowCnt);

extern "C" DLLEXPORT void BatchCastDecimal128ToStringRetNull(bool *isNull, int64_t contextPtr, Decimal128 *inputDecimal,
    int32_t precision, int32_t scale, uint8_t **output, int32_t *outLen, int32_t rowCnt);

extern "C" DLLEXPORT void BatchCastStringToDecimal64(int64_t contextPtr, uint8_t **str, int32_t *strLen,
    bool *isAnyNull, int64_t *output, int32_t outPrecision, int32_t outScale, int32_t rowCnt);

extern "C" DLLEXPORT void BatchCastStringToDecimal128(int64_t contextPtr, uint8_t **str, int32_t *strLen,
    bool *isAnyNull, Decimal128 *output, int32_t outPrecision, int32_t outScale, int32_t rowCnt);

extern "C" DLLEXPORT void BatchCastStringToDecimal64RoundUp(int64_t contextPtr, uint8_t **str, int32_t *strLen,
    bool *isAnyNull, int64_t *output, int32_t outPrecision, int32_t outScale, int32_t rowCnt);

extern "C" DLLEXPORT void BatchCastStringToDecimal128RoundUp(int64_t contextPtr, uint8_t **str, int32_t *strLen,
    bool *isAnyNull, Decimal128 *output, int32_t outPrecision, int32_t outScale, int32_t rowCnt);

extern "C" DLLEXPORT void BatchCastStringToInt(int64_t contextPtr, uint8_t **str, int32_t *strLen, bool *isAnyNull,
    int32_t *output, int32_t rowCnt);

extern "C" DLLEXPORT void BatchCastStringToLong(int64_t contextPtr, uint8_t **str, int32_t *strLen, bool *isAnyNull,
    int64_t *output, int32_t rowCnt);

extern "C" DLLEXPORT void BatchCastStringToDouble(int64_t contextPtr, uint8_t **str, int32_t *strLen, bool *isAnyNull,
    double *output, int32_t rowCnt);

extern "C" DLLEXPORT void BatchCastStringToDecimal64RetNull(bool *isNull, uint8_t **str, int32_t *strLen,
    int64_t *output, int32_t outPrecision, int32_t outScale, int32_t rowCnt);

extern "C" DLLEXPORT void BatchCastStringToDecimal128RetNull(bool *isNull, uint8_t **str, int32_t *strLen,
    Decimal128 *output, int32_t outPrecision, int32_t outScale, int32_t rowCnt);

extern "C" DLLEXPORT void BatchCastStringToDecimal64RoundUpRetNull(bool *isNull, uint8_t **str, int32_t *strLen,
    int64_t *output, int32_t outPrecision, int32_t outScale, int32_t rowCnt);

extern "C" DLLEXPORT void BatchCastStringToDecimal128RoundUpRetNull(bool *isNull, uint8_t **str, int32_t *strLen,
    Decimal128 *output, int32_t outPrecision, int32_t outScale, int32_t rowCnt);

extern "C" DLLEXPORT void BatchCastStringToIntRetNull(bool *isNull, uint8_t **str, int32_t *strLen, int32_t *output,
    int32_t rowCnt);

extern "C" DLLEXPORT void BatchCastStringToLongRetNull(bool *isNull, uint8_t **str, int32_t *strLen, int64_t *output,
    int32_t rowCnt);

extern "C" DLLEXPORT void BatchCastStringToDoubleRetNull(bool *isNull, uint8_t **str, int32_t *strLen, double *output,
    int32_t rowCnt);

extern "C" DLLEXPORT void BatchToUpperStr(int64_t contextPtr, uint8_t **str, int32_t *strLen, bool *isAnyNull,
    uint8_t **output, int32_t *outLen, int32_t rowCnt);

extern "C" DLLEXPORT void BatchToUpperChar(int64_t contextPtr, uint8_t **str, int32_t width, int32_t *strLen,
    bool *isAnyNull, uint8_t **output, int32_t *outLen, int32_t rowCnt);

extern "C" DLLEXPORT void BatchToLowerStr(int64_t contextPtr, uint8_t **str, int32_t *strLen, bool *isAnyNull,
    uint8_t **output, int32_t *outLen, int32_t rowCnt);

extern "C" DLLEXPORT void BatchToLowerChar(int64_t contextPtr, uint8_t **str, int32_t width, int32_t *strLen,
    bool *isAnyNull, uint8_t **output, int32_t *outLen, int32_t rowCnt);

extern "C" DLLEXPORT void BatchJsonSplitStr(int64_t contextPtr, uint8_t **jsonStr, int32_t *jsonStrLen,
    bool *jsonStrIsNull, bool *outIsNull, uint8_t **output, int32_t *outLen, int32_t rowCnt);

extern "C" DLLEXPORT void BatchJsonSplitChar(int64_t contextPtr, uint8_t **jsonStr, int32_t jsonStrWidth,
    int32_t *jsonStrLen, bool *jsonStrIsNull, bool *outIsNull, uint8_t **output, int32_t *outLen, int32_t rowCnt);

extern "C" DLLEXPORT void BatchJsonValue(int64_t contextPtr, uint8_t **jsonStr, int32_t *jsonStrLen,
    bool *jsonStrIsNull, uint8_t **pathStr, int32_t pathStrWidth, int32_t *pathStrLen, bool *pathStrIsNull,
    bool *outIsNull, uint8_t **output, int32_t *outLen, int32_t rowCnt);

extern "C" DLLEXPORT void BatchJsonValueWithBehaviors(int64_t contextPtr, uint8_t **jsonStr, int32_t *jsonStrLen,
    bool *jsonStrIsNull, uint8_t **pathStr, int32_t pathStrWidth, int32_t *pathStrLen, bool *pathStrIsNull,
    int32_t *emptyBehavior, bool *emptyBehaviorIsNull, uint8_t **defaultOnEmpty, int32_t *defaultOnEmptyLen,
    bool *defaultOnEmptyIsNull, int32_t *errorBehavior, bool *errorBehaviorIsNull, uint8_t **defaultOnError,
    int32_t *defaultOnErrorLen, bool *defaultOnErrorIsNull, bool *outIsNull, uint8_t **output, int32_t *outLen,
    int32_t rowCnt);

extern "C" DLLEXPORT void BatchJsonQuery(int64_t contextPtr, uint8_t **jsonStr, int32_t *jsonStrLen,
    bool *jsonStrIsNull, uint8_t **pathStr, int32_t pathStrWidth, int32_t *pathStrLen, bool *pathStrIsNull,
    bool *outIsNull, uint8_t **output, int32_t *outLen, int32_t rowCnt);

extern "C" DLLEXPORT void BatchLikeStr(uint8_t **str, int32_t *strLen, uint8_t **regexToMatch, int32_t *regexLen,
    bool *isAnyNull, bool *output, int32_t rowCnt);

extern "C" DLLEXPORT void BatchLikeChar(uint8_t **str, int32_t strWidth, int32_t *strLen, uint8_t **regexToMatch,
    int32_t *regexLen, bool *isAnyNull, bool *output, int32_t rowCnt);

extern "C" DLLEXPORT void BatchConcatStrStr(int64_t contextPtr, uint8_t **ap, int32_t *apLen, uint8_t **bp,
    int32_t *bpLen, bool *isAnyNull, uint8_t **output, int32_t *outLen, int32_t rowCnt);

extern "C" DLLEXPORT void BatchConcatStrStrRetNull(bool *isNull, int64_t contextPtr, uint8_t **ap, int32_t *apLen,
    uint8_t **bp, int32_t *bpLen, uint8_t **output, int32_t *outLen, int32_t rowCnt);

extern "C" DLLEXPORT void BatchConcatCharChar(int64_t contextPtr, uint8_t **ap, int32_t aWidth, int32_t *apLen,
    uint8_t **bp, int32_t bWidth, int32_t *bpLen, bool *isAnyNull, uint8_t **output, int32_t *outLen, int32_t rowCnt);

extern "C" DLLEXPORT void BatchConcatCharCharRetNull(bool *isNull, int64_t contextPtr, uint8_t **ap, int32_t aWidth,
    int32_t *apLen, uint8_t **bp, int32_t bWidth, int32_t *bpLen, uint8_t **output, int32_t *outLen, int32_t rowCnt);

extern "C" DLLEXPORT void BatchConcatCharStr(int64_t contextPtr, uint8_t **ap, int32_t aWidth, int32_t *apLen,
    uint8_t **bp, int32_t *bpLen, bool *isAnyNull, uint8_t **output, int32_t *outLen, int32_t rowCnt);

extern "C" DLLEXPORT void BatchConcatCharStrRetNull(bool *isNull, int64_t contextPtr, uint8_t **ap, int32_t aWidth,
    int32_t *apLen, uint8_t **bp, int32_t *bpLen, uint8_t **output, int32_t *outLen, int32_t rowCnt);

extern "C" DLLEXPORT void BatchConcatStrChar(int64_t contextPtr, uint8_t **ap, int32_t *apLen, uint8_t **bp,
    int32_t bWidth, int32_t *bpLen, bool *isAnyNull, uint8_t **output, int32_t *outLen, int32_t rowCnt);

extern "C" DLLEXPORT void BatchConcatStrCharRetNull(bool *isNull, int64_t contextPtr, uint8_t **ap, int32_t *apLen,
    uint8_t **bp, int32_t bWidth, int32_t *bpLen, uint8_t **output, int32_t *outLen, int32_t rowCnt);

extern "C" DLLEXPORT void BatchCastStrWithDiffWidths(int64_t contextPtr, uint8_t **str, int32_t srcWidth,
    int32_t *strLen, bool *isAnyNull, uint8_t **output, int32_t *outLen, int32_t dstWidth, int32_t rowCnt);

extern "C" DLLEXPORT void BatchCastStrWithDiffWidthsRetNull(bool *isNull, int64_t contextPtr, uint8_t **str,
    int32_t srcWidth, int32_t *strLen, uint8_t **output, int32_t *outLen, int32_t dstWidth, int32_t rowCnt);

/**
 * If isSupportNegativeIndex is false,the result of substr is "" when start index is negative
 * If isSupportNegativeIndex is true,the substr rule is as follows:
 * e.g., str="apple", strLength=5, startIndex=-7, subStringLength=3, Result="a".
 * If isSupportZeroIndex is false,the result of substr is "" when start index is 0
 * If isSupportZeroIndex is true,it refers to the first element when the start index is 0
 */
template <typename T, bool isSupportNegativeIndex, bool isSupportZeroIndex>
extern DLLEXPORT void BatchSubstrVarchar(int64_t contextPtr, uint8_t **str, int32_t *strLen, T *startIdx, T *length,
    bool *isAnyNull, uint8_t **output, int32_t *outLen, int32_t rowCnt)
{
    for (int32_t i = 0; i < rowCnt; i++) {
        if (isAnyNull[i]) {
            outLen[i] = 0;
            output[i] = nullptr;
            continue;
        }

        int64_t endIdx;
        int64_t startIndex;
        if constexpr (isSupportZeroIndex) {
            startIdx[i] = (startIdx[i] == 0) ? 1 : startIdx[i];
        }
        int64_t startCodePoint = startIdx[i];
        int64_t lengthCodePoint = length[i];
        int32_t len = strLen[i];
        output[i] = const_cast<uint8_t *>(EMPTY);
        if (startCodePoint == 0 || (lengthCodePoint <= 0) || (len == 0) || startCodePoint > len) {
            outLen[i] = 0;
            continue;
        }
        const char *charStr = reinterpret_cast<const char *>(str[i]);
        if (startCodePoint > 0) {
            startIndex = omniruntime::Utf8Util::OffsetOfCodePoint(charStr, len, startCodePoint - 1);
            if (startIndex < 0) {
                // before beginning of string
                outLen[i] = 0;
                continue;
            }
            endIdx = omniruntime::Utf8Util::OffsetOfCodePoint(charStr, len, startIndex, lengthCodePoint);
            if (endIdx < 0) {
                // after end of string
                endIdx = len;
            }
        } else {
            // negative start is relative to end of string
            int32_t codePoints = omniruntime::Utf8Util::CountCodePoints(charStr, len);
            startCodePoint += codePoints;
            // before beginning of string
            if (startCodePoint < 0) {
                if constexpr (!isSupportNegativeIndex) {
                    outLen[i] = 0;
                    continue;
                } else {
                    lengthCodePoint += startCodePoint;
                    startCodePoint = 0;
                }
            }
            startIndex = omniruntime::Utf8Util::OffsetOfCodePoint(charStr, len, startCodePoint);
            endIdx = startCodePoint + lengthCodePoint < codePoints ?
                omniruntime::Utf8Util::OffsetOfCodePoint(charStr, len, startIndex, lengthCodePoint) :
                len;
        }

        outLen[i] = endIdx - startIndex;
        output[i] = str[i] + startIndex;
    }
}

template <typename T, bool isSupportNegativeIndex, bool isSupportZeroIndex>
extern DLLEXPORT void BatchSubstrChar(int64_t contextPtr, uint8_t **str, int32_t width, int32_t *strLen, T *startIdx,
    T *length, bool *isAnyNull, uint8_t **output, int32_t *outLen, int32_t rowCnt)
{
    BatchSubstrVarchar<T, isSupportNegativeIndex, isSupportZeroIndex>(contextPtr, str, strLen, startIdx, length,
        isAnyNull, output, outLen, rowCnt);
}

template <typename T, bool isSupportNegativeIndex, bool isSupportZeroIndex>
extern DLLEXPORT void BatchSubstrVarcharWithStart(int64_t contextPtr, uint8_t **str, int32_t *strLen, T *startIdx,
    bool *isAnyNull, uint8_t **output, int32_t *outLen, int32_t rowCnt)
{
    int64_t startIndex;
    for (int32_t i = 0; i < rowCnt; i++) {
        if (isAnyNull[i]) {
            outLen[i] = 0;
            output[i] = nullptr;
            continue;
        }
        if constexpr (isSupportZeroIndex) {
            startIdx[i] = (startIdx[i] == 0) ? 1 : startIdx[i];
        }
        int64_t startCodePoint = startIdx[i];
        int32_t len = strLen[i];
        output[i] = const_cast<uint8_t *>(EMPTY);
        if (startCodePoint == 0 || len == 0 || startCodePoint > len) {
            outLen[i] = 0;
            continue;
        }

        const char *charStr = reinterpret_cast<const char *>(str[i]);
        if (startCodePoint > 0) {
            startIndex = omniruntime::Utf8Util::OffsetOfCodePoint(charStr, len, startCodePoint - 1);
            if (startIndex < 0) {
                outLen[i] = 0;
                continue;
            }
        } else {
            // negative start is relative to end of string
            int32_t codePoints = omniruntime::Utf8Util::CountCodePoints(charStr, len);
            startCodePoint += codePoints;
            if (startCodePoint < 0) {
                if constexpr (!isSupportNegativeIndex) {
                    outLen[i] = 0;
                    continue;
                } else {
                    startCodePoint = 0;
                }
            }

            startIndex = omniruntime::Utf8Util::OffsetOfCodePoint(charStr, len, startCodePoint);
        }

        outLen[i] = len - startIndex;
        output[i] = str[i] + startIndex;
    }
}

template <typename T, bool isSupportNegativeIndex, bool isSupportZeroIndex>
extern DLLEXPORT void BatchSubstrCharWithStart(int64_t contextPtr, uint8_t **str, int32_t width, int32_t *strLen,
    T *startIdx, bool *isAnyNull, uint8_t **output, int32_t *outLen, int32_t rowCnt)
{
    BatchSubstrVarcharWithStart<T, isSupportNegativeIndex, isSupportZeroIndex>(contextPtr, str, strLen, startIdx,
        isAnyNull, output, outLen, rowCnt);
}

extern "C" DLLEXPORT void BatchLengthChar(uint8_t **str, const int32_t width, int32_t *strLen, bool *isAnyNull,
    int64_t *output, int32_t rowCnt);

extern "C" DLLEXPORT void BatchLengthCharReturnInt32(uint8_t **str, const int32_t width, int32_t *strLen,
    bool *isAnyNull, int32_t *output, int32_t rowCnt);

extern "C" DLLEXPORT void BatchLengthStr(uint8_t **str, int32_t *strLen, bool *isAnyNull, int64_t *output,
    int32_t rowCnt);

extern "C" DLLEXPORT void BatchLengthStrReturnInt32(uint8_t **str, int32_t *strLen, bool *isAnyNull, int32_t *output,
    int32_t rowCnt);

extern "C" DLLEXPORT void BatchReplaceStrStrStrWithRepNotReplace(int64_t contextPtr, uint8_t **str, int32_t *strLen,
    uint8_t **searchStr, int32_t *searchLen, uint8_t **replaceStr, int32_t *replaceLen, bool *isAnyNull,
    uint8_t **output, int32_t *outLen, int32_t rowCnt);

extern "C" DLLEXPORT void BatchReplaceStrStrWithoutRepNotReplace(int64_t contextPtr, uint8_t **str, int32_t *strLen,
    uint8_t **searchStr, int32_t *searchLen, bool *isAnyNull, uint8_t **output, int32_t *outLen, int32_t rowCnt);

extern "C" DLLEXPORT void BatchReplaceStrStrStrWithRepReplace(int64_t contextPtr, uint8_t **str, int32_t *strLen,
    uint8_t **searchStr, int32_t *searchLen, uint8_t **replaceStr, int32_t *replaceLen, bool *isAnyNull,
    uint8_t **output, int32_t *outLen, int32_t rowCnt);

extern "C" DLLEXPORT void BatchReplaceStrStrWithoutRepReplace(int64_t contextPtr, uint8_t **str, int32_t *strLen,
    uint8_t **searchStr, int32_t *searchLen, bool *isAnyNull, uint8_t **output, int32_t *outLen, int32_t rowCnt);

template <typename L>
static inline void ReplaceWithReplaceNotEmpty(int64_t contextPtr, uint8_t **str, int32_t *strLen, uint8_t **searchStr,
    int32_t *searchLen, uint8_t **replaceStr, int32_t *replaceLen, bool *isAnyNull, uint8_t **output, int32_t *outLen,
    int32_t rowCnt, L lambda)
{
    bool hasErr;
    uint8_t *ret;
    for (int32_t i = 0; i < rowCnt; i++) {
        if (isAnyNull[i]) {
            outLen[i] = 0;
            output[i] = nullptr;
            continue;
        }
        hasErr = false;
        if (searchLen[i] == 0) {
            ret = lambda(&hasErr, i);
        } else {
            auto result = StringUtil::ReplaceWithSearchNotEmpty(contextPtr, reinterpret_cast<const char *>(str[i]),
                strLen[i], reinterpret_cast<const char *>(searchStr[i]), searchLen[i],
                reinterpret_cast<const char *>(replaceStr[i]), replaceLen[i], &hasErr, outLen + i);
            ret = reinterpret_cast<uint8_t *>(const_cast<char *>(result));
        }

        if (hasErr) {
            SetError(contextPtr, REPLACE_ERR_MSG);
        }
        output[i] = ret;
    }
}

template <typename L>
static inline void ReplaceWithReplaceEmpty(int64_t contextPtr, uint8_t **str, int32_t *strLen, uint8_t **searchStr,
    int32_t *searchLen, bool *isAnyNull, uint8_t **output, int32_t *outLen, int32_t rowCnt, L lambda)
{
    bool hasErr;
    uint8_t *ret;
    for (int32_t i = 0; i < rowCnt; i++) {
        if (isAnyNull[i]) {
            outLen[i] = 0;
            output[i] = nullptr;
            continue;
        }
        hasErr = false;
        if (searchLen[i] == 0) {
            ret = lambda(&hasErr, i);
        } else {
            auto result = StringUtil::ReplaceWithSearchNotEmpty(contextPtr, reinterpret_cast<char *>(str[i]), strLen[i],
                reinterpret_cast<char *>(searchStr[i]), searchLen[i], reinterpret_cast<const char *>(EMPTY), 0, &hasErr,
                outLen + i);
            ret = reinterpret_cast<uint8_t *>(const_cast<char *>(result));
        }

        if (hasErr) {
            SetError(contextPtr, REPLACE_ERR_MSG);
        }
        output[i] = ret;
    }
}

extern "C" DLLEXPORT void BatchInStr(char **srcStrs, int32_t *srcLens, char **subStrs, int32_t *subLens,
    bool *isAnyNull, int32_t *output, int32_t rowCnt);

extern "C" DLLEXPORT void BatchStartsWithStr(char **srcStrs, int32_t *srcLens, char **matchStrs, int32_t *matchLens,
    bool *isAnyNull, bool *output, int32_t rowCnt);

extern "C" DLLEXPORT void BatchEndsWithStr(char **srcStrs, int32_t *srcLens, char **matchStrs, int32_t *matchLens,
    bool *isAnyNull, bool *output, int32_t rowCnt);

extern "C" DLLEXPORT void BatchMd5Str(int64_t contextPtr, uint8_t **str, int32_t *strLen, bool *isAnyNull,
    uint8_t **output, int32_t *outLen, int32_t rowCnt);

extern "C" DLLEXPORT void BatchEmptyToNull(char **str, int32_t *strLen, bool *isAnyNull, char **output, int32_t *outLen,
    int32_t rowCnt);

extern "C" DLLEXPORT void BatchContainsStr(char **srcStrs, int32_t *srcLens, char **matchStrs, int32_t *matchLens,
    bool *isAnyNull, bool *output, int32_t rowCnt);

extern "C" DLLEXPORT void BatchGreatestStr(uint8_t **xStr, int32_t *xStrLen, bool *xIsNull, uint8_t **yStr,
    int32_t *yStrLen, bool *yIsNull, bool *retIsNull, uint8_t **outStr, int32_t *outStrLen, int32_t rowCnt);

extern "C" DLLEXPORT void BatchStaticInvokeVarcharTypeWriteSideCheck(int64_t contextPtr, char **str, int32_t *strLen,
    int32_t limit, bool *isAnyNull, char **outputStr, int32_t *outputLen, int32_t rowCnt);
}

extern "C" DLLEXPORT void BatchStaticInvokeCharReadPadding(int64_t contextPtr, char **str,
    int32_t *strLen, int32_t limit, bool *isAnyNull, char **outputStr, int32_t *outputLen, int32_t rowCnt);
#endif // OMNI_RUNTIME_BATCH_STRINGFUNCTIONS_H
