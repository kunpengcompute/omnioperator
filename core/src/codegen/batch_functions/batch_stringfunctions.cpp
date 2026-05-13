/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2024. All rights reserved.
 * Description: batch string functions implementation
 */
#include "batch_stringfunctions.h"
#include <iostream>
#include <regex>
#include "codegen/functions/stringfunctions.h"
#include "type/data_operations.h"
#include "type/date32.h"
#include "codegen/functions/md5.h"

#ifdef _WIN32
#else
#define DLLEXPORT
#endif

namespace omniruntime::codegen::function {
extern "C" DLLEXPORT void BatchCountChar(uint8_t **str, int32_t *strLen, uint8_t **target, int32_t targetWidth,
                                         int32_t *targetLen, bool *isAnyNull, int64_t *output, int32_t rowCnt)
{
    for (int j = 0; j < rowCnt; ++j) {
        if (isAnyNull[j]) {
            output[j] = 0;
            continue;
        }
        char chr = target[j][0];
        output[j] = std::count(str[j], str[j] + strLen[j], chr);
    }
}

extern "C" DLLEXPORT void BatchSplitIndex(uint8_t **str, int32_t *strLen, uint8_t **target, int32_t targetWidth,
                                         int32_t *targetLen, int32_t *index, bool *isAnyNull, uint8_t **output,
                                         int32_t *outLen, int32_t rowCnt)
{
    for (int j = 0; j < rowCnt; ++j) {
        if (isAnyNull[j]) {
            output[j] = nullptr;
            outLen[j] = 0;
            continue;
        }
        size_t start = 0;
        size_t currentIndex = 0;
        bool found = false;
        for (size_t i = 0; i <= strLen[j]; ++i) {
            if (i != strLen[j] && str[j][i] != *target[j]) {
                continue;
            }
            if (currentIndex == index[j]) {
                outLen[j] = i - start;
                output[j] = str[j] + start;
                found = true;
                break;
            }
            start = i + 1;
            ++currentIndex;
        }
        if (!found) {
            outLen[j] = 0;
            output[j] = nullptr;
        }
    }
}

extern "C" DLLEXPORT void BatchStrCompare(uint8_t **ap, int32_t *apLen, uint8_t **bp, int32_t *bpLen, int32_t *res,
    int32_t rowCnt)
{
    int min = 0, result = 0;
    for (int i = 0; i < rowCnt; ++i) {
        min = bpLen[i];
        if (apLen[i] < min) {
            min = apLen[i];
        }

        result = memcmp(ap[i], bp[i], min);
        if (result != 0) {
            res[i] = result;
        } else {
            res[i] = apLen[i] - bpLen[i];
        }
    }
}

extern "C" DLLEXPORT void BatchLessThanStr(uint8_t **ap, int32_t *apLen, uint8_t **bp, int32_t *bpLen, bool *res,
    int32_t rowCnt)
{
    auto tmp = new int32_t[rowCnt];
    BatchStrCompare(ap, apLen, bp, bpLen, tmp, rowCnt);
    for (int i = 0; i < rowCnt; i++) {
        res[i] = (tmp[i] < 0);
    }
    delete[] tmp;
}

extern "C" DLLEXPORT void BatchLessThanEqualStr(uint8_t **ap, int32_t *apLen, uint8_t **bp, int32_t *bpLen, bool *res,
    int32_t rowCnt)
{
    auto tmp = new int32_t[rowCnt];
    BatchStrCompare(ap, apLen, bp, bpLen, tmp, rowCnt);
    for (int i = 0; i < rowCnt; i++) {
        res[i] = (tmp[i] <= 0);
    }
    delete[] tmp;
}

extern "C" DLLEXPORT void BatchGreaterThanStr(uint8_t **ap, int32_t *apLen, uint8_t **bp, int32_t *bpLen, bool *res,
    int32_t rowCnt)
{
    auto tmp = new int32_t[rowCnt];
    BatchStrCompare(ap, apLen, bp, bpLen, tmp, rowCnt);
    for (int i = 0; i < rowCnt; i++) {
        res[i] = (tmp[i] > 0);
    }
    delete[] tmp;
}

extern "C" DLLEXPORT void BatchGreaterThanEqualStr(uint8_t **ap, int32_t *apLen, uint8_t **bp, int32_t *bpLen,
    bool *res, int32_t rowCnt)
{
    auto tmp = new int32_t[rowCnt];
    BatchStrCompare(ap, apLen, bp, bpLen, tmp, rowCnt);
    for (int i = 0; i < rowCnt; i++) {
        res[i] = (tmp[i] >= 0);
    }
    delete[] tmp;
}

extern "C" DLLEXPORT void BatchEqualStr(uint8_t **ap, int32_t *apLen, uint8_t **bp, int32_t *bpLen, bool *res,
    int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        if (apLen[i] != bpLen[i]) {
            res[i] = false;
        } else {
            res[i] = (memcmp(ap[i], bp[i], apLen[i]) == 0);
        }
    }
}

extern "C" DLLEXPORT void BatchNotEqualStr(uint8_t **ap, int32_t *apLen, uint8_t **bp, int32_t *bpLen, bool *res,
    int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        if (apLen[i] != bpLen[i]) {
            res[i] = true;
        } else {
            res[i] = (memcmp(ap[i], bp[i], apLen[i]) != 0);
        }
    }
}

extern "C" DLLEXPORT void BatchCastStringToDateNotAllowReducePrecison(int64_t contextPtr, uint8_t **str,
    int32_t *strLen, bool *isAnyNull, int32_t *output, int32_t rowCnt)
{
    // Date is in the format 1996-02-28
    // Doesn't account for leap seconds or daylight savings
    // Should be ok just for dates
    int64_t result = 0;
    for (int i = 0; i < rowCnt; ++i) {
        if (isAnyNull[i]) {
            output[i] = 0;
            continue;
        }
        std::string s = std::string(reinterpret_cast<char *>(str[i]), strLen[i]);
        StringUtil::TrimString(s);
        if (!regex_match(s, std::regex(R"(\d{4}-\d{2}-\d{2}$)"))) {
            SetError(contextPtr, "Only support cast date\'YYYY-MM-DD\' to integer");
            output[i] = 0;
            continue;
        }
        if (Date32::StringToDate32(reinterpret_cast<char *>(str[i]), strLen[i], result) != Status::CONVERT_SUCCESS &&
            !HasError(contextPtr)) {
            SetError(contextPtr, "Value cannot be cast to date: " + s);
            continue;
        }
        output[i] = static_cast<int32_t >(result);
    }
}

extern "C" DLLEXPORT void BatchCastStringToDateAllowReducePrecison(int64_t contextPtr, uint8_t **str, int32_t *strLen,
    bool *isAnyNull, int32_t *output, int32_t rowCnt)
{
    // Date is in the format 1996-02-28
    // Doesn't account for leap seconds or daylight savings
    // Should be ok just for dates
    int64_t result = 0;
    for (int i = 0; i < rowCnt; ++i) {
        if (isAnyNull[i]) {
            output[i] = 0;
            continue;
        }
        std::string s = std::string(reinterpret_cast<char *>(str[i]), strLen[i]);
        StringUtil::TrimString(s);
        if (Date32::StringToDate32(reinterpret_cast<char *>(str[i]), strLen[i], result) != Status::CONVERT_SUCCESS &&
            !HasError(contextPtr)) {
            SetError(contextPtr, "Value cannot be cast to date: " + s);
            continue;
        }
        output[i] = static_cast<int32_t >(result);
    }
}

extern "C" DLLEXPORT void BatchCastIntToString(int64_t contextPtr, int32_t *value, bool *isAnyNull, uint8_t **output,
    int32_t *outLen, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        if (isAnyNull[i]) {
            outLen[i] = 0;
            output[i] = nullptr;
            continue;
        }
        std::string str = std::to_string(value[i]);
        outLen[i] = static_cast<int32_t>(str.size());
        if (outLen[i] <= 0) {
            outLen[i] = 0;
            output[i] = (uint8_t *)"";
            continue;
        }
        auto ret = ArenaAllocatorMalloc(contextPtr, outLen[i]);
        memcpy(ret, str.c_str(), outLen[i]);
        output[i] = reinterpret_cast<uint8_t *>(ret);
    }
}

extern "C" DLLEXPORT void BatchCastLongToString(int64_t contextPtr, int64_t *value, bool *isAnyNull, uint8_t **output,
    int32_t *outLen, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        if (isAnyNull[i]) {
            outLen[i] = 0;
            output[i] = nullptr;
            continue;
        }
        std::string str = std::to_string(value[i]);
        outLen[i] = static_cast<int32_t>(strlen(str.c_str()));
        if (outLen[i] <= 0) {
            outLen[i] = 0;
            output[i] = (uint8_t *)"";
            continue;
        }
        auto ret = ArenaAllocatorMalloc(contextPtr, outLen[i]);
        memcpy(ret, str.c_str(), outLen[i]);
        output[i] = reinterpret_cast<uint8_t *>(ret);
    }
}

extern "C" DLLEXPORT void BatchCastDoubleToString(int64_t contextPtr, double *value, bool *isAnyNull, uint8_t **output,
    int32_t *outLen, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        auto ret = ArenaAllocatorMalloc(contextPtr, MAX_DATA_LENGTH);
        outLen[i] = static_cast<int32_t >(DoubleToString::DoubleToStringConverter(value[i], ret));
        output[i] = reinterpret_cast<uint8_t *>(ret);
    }
}

extern "C" DLLEXPORT void BatchCastDecimal64ToString(int64_t contextPtr, int64_t *x, int32_t precision, int32_t scale,
    bool *isAnyNull, uint8_t **output, int32_t *outLen, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        if (isAnyNull[i]) {
            outLen[i] = 0;
            output[i] = nullptr;
            continue;
        }
        std::string str = Decimal64(x[i]).SetScale(scale).ToString();
        outLen[i] = static_cast<int32_t>(str.size());
        if (outLen[i] <= 0) {
            outLen[i] = 0;
            output[i] = (uint8_t *)"";
            continue;
        }
        auto ret = ArenaAllocatorMalloc(contextPtr, outLen[i]);
        memcpy(ret, str.c_str(), outLen[i]);
        output[i] = reinterpret_cast<uint8_t *>(ret);
    }
}

extern "C" DLLEXPORT void BatchCastDecimal128ToString(int64_t contextPtr, Decimal128 *x, int32_t precision,
    int32_t scale, bool *isAnyNull, uint8_t **output, int32_t *outLen, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        if (isAnyNull[i]) {
            outLen[i] = 0;
            output[i] = nullptr;
            continue;
        }
        std::string stringDecimal = Decimal128Wrapper(x[i]).SetScale(scale).ToString();
        outLen[i] = static_cast<int32_t>(stringDecimal.length());
        if (outLen[i] <= 0) {
            outLen[i] = 0;
            output[i] = (uint8_t *)"";
            continue;
        }
        auto ret = ArenaAllocatorMalloc(contextPtr, outLen[i]);
        memcpy(ret, stringDecimal.c_str(), outLen[i]);
        output[i] = reinterpret_cast<uint8_t *>(ret);
    }
}

extern "C" DLLEXPORT void BatchCastStringToDecimal64(int64_t contextPtr, uint8_t **str, int32_t *strLen,
    bool *isAnyNull, int64_t *output, int32_t outPrecision, int32_t outScale, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        if (isAnyNull[i]) {
            output[i] = 0;
            continue;
        }
        std::string s = std::string(reinterpret_cast<const char *>(str[i]), strLen[i]);
        StringUtil::TrimString(s);
        if (!regex_match(s, g_decimalRegex)) {
            std::ostringstream errorMessage;
            errorMessage << "Cannot cast VARCHAR '" << s << "' to DECIMAL(" << outPrecision << ", " << outScale <<
                "). Value too large.";
            SetError(contextPtr, errorMessage.str());
            continue;
        }
        Decimal64 result(s);
        if (result.IsOverflow(outPrecision) != OpStatus::SUCCESS) {
            std::ostringstream errorMessage;
            errorMessage << "Cannot cast VARCHAR '" << s << "' to DECIMAL(" << outPrecision << ", " << outScale <<
                "). Value too large.";
            SetError(contextPtr, errorMessage.str());
            output[i] = 0;
            continue;
        }
        output[i] = result.GetValue();
    }
}

extern "C" DLLEXPORT void BatchCastStringToDecimal128(int64_t contextPtr, uint8_t **str, int32_t *strLen,
    bool *isAnyNull, Decimal128 *output, int32_t outPrecision, int32_t outScale, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        if (isAnyNull[i]) {
            output[i].SetValue(0, 0);
            continue;
        }
        std::string s = std::string(reinterpret_cast<const char *>(str[i]), strLen[i]);
        if (!regex_match(s, g_decimalRegex)) {
            std::ostringstream errorMessage;
            errorMessage << "Cannot cast VARCHAR '" << s << "' to DECIMAL(" << outPrecision << ", " << outScale <<
                "). Value too large.";
            SetError(contextPtr, errorMessage.str());
            continue;
        }
        StringUtil::TrimString(s);
        Decimal128Wrapper result(s.c_str());
        result.ReScale(outScale);
        if (result.IsOverflow(outPrecision) != OpStatus::SUCCESS) {
            std::ostringstream errorMessage;
            errorMessage << "Cannot cast VARCHAR '" << s << "' to DECIMAL(" << outPrecision << ", " << outScale <<
                "). Value too large.";
            SetError(contextPtr, errorMessage.str());
            continue;
        }
        output[i] = result.ToDecimal128();
    }
}

extern "C" DLLEXPORT void BatchCastStringToDecimal64RoundUp(int64_t contextPtr, uint8_t **str, int32_t *strLen,
    bool *isAnyNull, int64_t *output, int32_t outPrecision, int32_t outScale, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        if (isAnyNull[i]) {
            output[i] = 0;
            continue;
        }
        std::string s = std::string(reinterpret_cast<const char *>(str[i]), strLen[i]);
        StringUtil::TrimString(s);
        if (!regex_match(s, g_decimalRegex)) {
            std::ostringstream errorMessage;
            errorMessage << "Cannot cast VARCHAR '" << s << "' to DECIMAL(" << outPrecision << ", " << outScale <<
                         "). Value too large.";
            SetError(contextPtr, errorMessage.str());
            continue;
        }
        Decimal64<true> result(s);
        if (result.IsOverflow(outPrecision) != OpStatus::SUCCESS) {
            std::ostringstream errorMessage;
            errorMessage << "Cannot cast VARCHAR '" << s << "' to DECIMAL(" << outPrecision << ", " << outScale <<
                         "). Value too large.";
            SetError(contextPtr, errorMessage.str());
            output[i] = 0;
            continue;
        }
        output[i] = result.GetValue();
    }
}

extern "C" DLLEXPORT void BatchCastStringToDecimal128RoundUp(int64_t contextPtr, uint8_t **str, int32_t *strLen,
    bool *isAnyNull, Decimal128 *output, int32_t outPrecision, int32_t outScale, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        if (isAnyNull[i]) {
            output[i].SetValue(0, 0);
            continue;
        }
        std::string s = std::string(reinterpret_cast<const char *>(str[i]), strLen[i]);
        if (!regex_match(s, g_decimalRegex)) {
            std::ostringstream errorMessage;
            errorMessage << "Cannot cast VARCHAR '" << s << "' to DECIMAL(" << outPrecision << ", " << outScale <<
                         "). Value too large.";
            SetError(contextPtr, errorMessage.str());
            continue;
        }
        StringUtil::TrimString(s);
        Decimal128Wrapper<true> result(s.c_str());
        result.ReScale(outScale);
        if (result.IsOverflow(outPrecision) != OpStatus::SUCCESS) {
            std::ostringstream errorMessage;
            errorMessage << "Cannot cast VARCHAR '" << s << "' to DECIMAL(" << outPrecision << ", " << outScale <<
                         "). Value too large.";
            SetError(contextPtr, errorMessage.str());
            continue;
        }
        output[i] = result.ToDecimal128();
    }
}

extern "C" DLLEXPORT void BatchCastStringToInt(int64_t contextPtr, uint8_t **str, int32_t *strLen, bool *isAnyNull,
    int32_t *output, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        if (isAnyNull[i]) {
            output[i] = 0;
            continue;
        }
        auto chars = reinterpret_cast<const char *>(str[i]);
        Status status = ConvertStringToInteger<int32_t, false>(output[i], chars, strLen[i]);
        if (status != Status::CONVERT_SUCCESS) {
            std::string s(chars, strLen[i]);
            std::string reason =
                status == Status::IS_NOT_A_NUMBER ? "Value is not a number." : "Value too large or too small.";
            std::ostringstream errorMessage;
            errorMessage << "Cannot cast '" << s << "' to INTEGER. " << reason;
            SetError(contextPtr, errorMessage.str());
            output[i] = 0;
            continue;
        }
    }
}

extern "C" DLLEXPORT void BatchCastStringToLong(int64_t contextPtr, uint8_t **str, int32_t *strLen, bool *isAnyNull,
    int64_t *output, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        if (isAnyNull[i]) {
            output[i] = 0;
            continue;
        }

        auto chars = reinterpret_cast<const char *>(str[i]);
        Status status = ConvertStringToInteger<int64_t, false>(output[i], chars, strLen[i]);
        if (status != Status::CONVERT_SUCCESS) {
            std::string s(chars, strLen[i]);
            std::string reason =
                status == Status::IS_NOT_A_NUMBER ? "Value is not a number." : "Value too large or too small.";
            std::ostringstream errorMessage;
            errorMessage << "Cannot cast '" << s << "' to INTEGER. " << reason;
            SetError(contextPtr, errorMessage.str());
            output[i] = 0;
            continue;
        }
    }
}

extern "C" DLLEXPORT void BatchCastStringToDouble(int64_t contextPtr, uint8_t **str, int32_t *strLen, bool *isAnyNull,
    double *output, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        if (isAnyNull[i]) {
            output[i] = 0;
            continue;
        }
        double result;
        Status status = ConvertStringToDouble(result, reinterpret_cast<const char *>(str[i]), strLen[i]);
        if (status == Status::IS_NOT_A_NUMBER) {
            std::ostringstream errorMessage;
            errorMessage << "Cannot cast '" << std::string(reinterpret_cast<const char *>(str[i]), strLen[i])
                         << "' to DOUBLE. Value is not a number.";
            SetError(contextPtr, errorMessage.str());
            continue;
        }
        if (status == Status::CONVERT_OVERFLOW) {
            std::ostringstream errorMessage;
            errorMessage << "Cannot cast '" << std::string(reinterpret_cast<const char *>(str[i]), strLen[i])
                         << "' to DOUBLE. Value is not a number.";
            SetError(contextPtr, errorMessage.str());
            continue;
        }
        output[i] = result;
    }
}

extern "C" DLLEXPORT void BatchCastStringToDateRetNullNotAllowReducePrecison(bool *isNull, uint8_t **str,
    int32_t *strLen, int32_t *output, int32_t rowCnt)
{
    // Date is in the format 1996-02-28
    // Doesn't account for leap seconds or daylight savings
    // Should be ok just for dates
    int64_t result = 0;
    for (int i = 0; i < rowCnt; ++i) {
        std::string s = std::string(reinterpret_cast<char *>(str[i]), strLen[i]);
        StringUtil::TrimString(s);
        if (!regex_match(s, std::regex(R"(\d{4}-\d{2}-\d{2}$)"))) {
            output[i] = 0;
            isNull[i] = true;
            continue;
        }
        if (Date32::StringToDate32(reinterpret_cast<char *>(str[i]), strLen[i], result) != Status::CONVERT_SUCCESS) {
            output[i] = 0;
            isNull[i] = true;
            continue;
        }
        output[i] = static_cast<int32_t >(result);
        isNull[i] = false;
    }
}

extern "C" DLLEXPORT void BatchCastStringToDateRetNullAllowReducePrecison(bool *isNull, uint8_t **str, int32_t *strLen,
    int32_t *output, int32_t rowCnt)
{
    // Date is in the format 1996-02-28
    // Doesn't account for leap seconds or daylight savings
    // Should be ok just for dates
    int64_t result = 0;
    for (int i = 0; i < rowCnt; ++i) {
        std::string s = std::string(reinterpret_cast<char *>(str[i]), strLen[i]);
        StringUtil::TrimString(s);
        if (Date32::StringToDate32(reinterpret_cast<char *>(str[i]), strLen[i], result) != Status::CONVERT_SUCCESS) {
            output[i] = 0;
            isNull[i] = true;
            continue;
        }
        output[i] = static_cast<int32_t >(result);
        isNull[i] = false;
    }
}

extern "C" DLLEXPORT void BatchCastIntToStringRetNull(bool *isNull, int64_t contextPtr, int32_t *value,
    uint8_t **output, int32_t *outLen, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        std::string str = std::to_string(value[i]);
        outLen[i] = static_cast<int32_t>(str.size());
        auto ret = ArenaAllocatorMalloc(contextPtr, outLen[i]);
        memcpy(ret, str.c_str(), outLen[i]);
        isNull[i] = false;
        output[i] = reinterpret_cast<uint8_t *>(ret);
    }
}

extern "C" DLLEXPORT void BatchCastLongToStringRetNull(bool *isNull, int64_t contextPtr, int64_t *value,
    uint8_t **output, int32_t *outLen, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        std::string str = std::to_string(value[i]);
        outLen[i] = static_cast<int32_t>(strlen(str.c_str()));
        auto ret = ArenaAllocatorMalloc(contextPtr, outLen[i]);
        memcpy(ret, str.c_str(), outLen[i]);

        isNull[i] = false;
        output[i] = reinterpret_cast<uint8_t *>(ret);
    }
}

extern "C" DLLEXPORT void BatchCastDoubleToStringRetNull(bool *isNull, int64_t contextPtr, double *value,
    uint8_t **output, int32_t *outLen, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        auto ret = ArenaAllocatorMalloc(contextPtr, MAX_DATA_LENGTH);
        outLen[i] = static_cast<int32_t >(DoubleToString::DoubleToStringConverter(value[i], ret));
        isNull[i] = false;
        output[i] = reinterpret_cast<uint8_t *>(ret);
    }
}

extern "C" DLLEXPORT void BatchCastDecimal64ToStringRetNull(bool *isNull, int64_t contextPtr, int64_t *x,
    int32_t precision, int32_t scale, uint8_t **output, int32_t *outLen, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        std::string str = Decimal64(x[i]).SetScale(scale).ToString();
        outLen[i] = static_cast<int32_t>(str.size());
        if (outLen[i] <= 0) {
            outLen[i] = 0;
            output[i] = (uint8_t *)"";
            continue;
        }
        auto ret = ArenaAllocatorMalloc(contextPtr, outLen[i]);
        memcpy(ret, str.c_str(), outLen[i]);

        isNull[i] = false;
        output[i] = reinterpret_cast<uint8_t *>(ret);
    }
}

extern "C" DLLEXPORT void BatchCastDecimal128ToStringRetNull(bool *isNull, int64_t contextPtr, Decimal128 *inputDecimal,
    int32_t precision, int32_t scale, uint8_t **output, int32_t *outLen, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        std::string stringDecimal = Decimal128Wrapper(inputDecimal[i]).SetScale(scale).ToString();
        outLen[i] = static_cast<int32_t>(stringDecimal.length());
        if (outLen[i] <= 0) {
            outLen[i] = 0;
            output[i] = (uint8_t *)"";
            continue;
        }
        auto ret = ArenaAllocatorMalloc(contextPtr, outLen[i]);
        memcpy(ret, stringDecimal.c_str(), outLen[i]);

        isNull[i] = false;
        output[i] = reinterpret_cast<uint8_t *>(ret);
    }
}

extern "C" DLLEXPORT void BatchCastStringToDecimal64RetNull(bool *isNull, uint8_t **str, int32_t *strLen,
    int64_t *output, int32_t outPrecision, int32_t outScale, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        std::string s = std::string(reinterpret_cast<const char *>(str[i]), strLen[i]);
        StringUtil::TrimString(s);
        if (!regex_match(s, g_decimalRegex)) {
            output[i] = 0;
            isNull[i] = true;
            continue;
        }
        Decimal64 result(s);
        result.ReScale(outScale);
        if (result.IsOverflow(outPrecision) != OpStatus::SUCCESS) {
            output[i] = 0;
            isNull[i] = true;
            continue;
        }
        isNull[i] = false;
        output[i] = result.GetValue();
    }
}

extern "C" DLLEXPORT void BatchCastStringToDecimal128RetNull(bool *isNull, uint8_t **str, int32_t *strLen,
    Decimal128 *output, int32_t outPrecision, int32_t outScale, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        std::string s = std::string(reinterpret_cast<const char *>(str[i]), strLen[i]);
        StringUtil::TrimString(s);
        if (!regex_match(s, g_decimalRegex)) {
            output[i] = 0;
            isNull[i] = true;
            continue;
        }
        Decimal128Wrapper result(s.c_str());
        result.ReScale(outScale);
        if (result.IsOverflow(outPrecision) != OpStatus::SUCCESS) {
            output[i] = 0;
            isNull[i] = true;
            continue;
        }
        output[i] = result.ToDecimal128();
    }
}

extern "C" DLLEXPORT void BatchCastStringToDecimal64RoundUpRetNull(bool *isNull, uint8_t **str, int32_t *strLen,
    int64_t *output, int32_t outPrecision, int32_t outScale, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        std::string s = std::string(reinterpret_cast<const char *>(str[i]), strLen[i]);
        StringUtil::TrimString(s);
        if (!regex_match(s, g_decimalRegex)) {
            output[i] = 0;
            isNull[i] = true;
            continue;
        }
        Decimal64<true> result(s);
        result.ReScale(outScale);
        if (result.IsOverflow(outPrecision) != OpStatus::SUCCESS) {
            output[i] = 0;
            isNull[i] = true;
            continue;
        }
        isNull[i] = false;
        output[i] = result.GetValue();
    }
}

extern "C" DLLEXPORT void BatchCastStringToDecimal128RoundUpRetNull(bool *isNull, uint8_t **str, int32_t *strLen,
    Decimal128 *output, int32_t outPrecision, int32_t outScale, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        std::string s = std::string(reinterpret_cast<const char *>(str[i]), strLen[i]);
        StringUtil::TrimString(s);
        if (!regex_match(s, g_decimalRegex)) {
            output[i] = 0;
            isNull[i] = true;
            continue;
        }
        Decimal128Wrapper<true> result(s.c_str());
        result.ReScale(outScale);
        if (result.IsOverflow(outPrecision) != OpStatus::SUCCESS) {
            output[i] = 0;
            isNull[i] = true;
            continue;
        }
        output[i] = result.ToDecimal128();
    }
}

extern "C" DLLEXPORT void BatchCastStringToIntRetNull(bool *isNull, uint8_t **str, int32_t *strLen, int32_t *output,
    int32_t rowCnt)
{
    for (int32_t i = 0; i < rowCnt; ++i) {
        Status status = ConvertStringToInteger<int32_t>(output[i], reinterpret_cast<const char *>(str[i]),
            strLen[i]);
        isNull[i] = status != Status::CONVERT_SUCCESS;
        if (isNull[i]) {
            output[i] = 0;
        }
    }
}

extern "C" DLLEXPORT void BatchCastStringToLongRetNull(bool *isNull, uint8_t **str, int32_t *strLen, int64_t *output,
    int32_t rowCnt)
{
    for (int32_t i = 0; i < rowCnt; ++i) {
        Status status = ConvertStringToInteger<int64_t>(output[i], reinterpret_cast<const char *>(str[i]),
            strLen[i]);
        isNull[i] = status != Status::CONVERT_SUCCESS;
        if (isNull[i]) {
            output[i] = 0;
        }
    }
}

extern "C" DLLEXPORT void BatchCastStringToDoubleRetNull(bool *isNull, uint8_t **str, int32_t *strLen, double *output,
    int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        double result;
        Status status = ConvertStringToDouble(result, reinterpret_cast<const char *>(str[i]), strLen[i]);
        if (status != Status::CONVERT_SUCCESS) {
            output[i] = 0;
            isNull[i] = true;
            continue;
        }
        isNull[i] = false;
        output[i] = result;
    }
}

extern "C" DLLEXPORT void BatchToUpperStr(int64_t contextPtr, uint8_t **str, int32_t *strLen, bool *isAnyNull,
    uint8_t **output, int32_t *outLen, int32_t rowCnt)
{
    char *ret;
    for (int j = 0; j < rowCnt; ++j) {
        if (isAnyNull[j]) {
            outLen[j] = 0;
            output[j] = nullptr;
            continue;
        }
        ret = ArenaAllocatorMalloc(contextPtr, strLen[j]);
        for (int i = 0; i < strLen[j]; i++) {
            if (*(str[j] + i) >= static_cast<int>('a') && *(str[j] + i) <= static_cast<int>('z')) {
                *(ret + i) = *(str[j] + i) - STEP;
            } else {
                *(ret + i) = *(str[j] + i);
            }
        }
        outLen[j] = strLen[j];
        output[j] = reinterpret_cast<uint8_t *>(ret);
    }
}

extern "C" DLLEXPORT void BatchToUpperChar(int64_t contextPtr, uint8_t **str, int32_t width, int32_t *strLen,
    bool *isAnyNull, uint8_t **output, int32_t *outLen, int32_t rowCnt)
{
    BatchToUpperStr(contextPtr, str, strLen, isAnyNull, output, outLen, rowCnt);
}

extern "C" DLLEXPORT void BatchToLowerStr(int64_t contextPtr, uint8_t **str, int32_t *strLen, bool *isAnyNull,
    uint8_t **output, int32_t *outLen, int32_t rowCnt)
{
    char *ret;
    char currItem;
    for (int j = 0; j < rowCnt; ++j) {
        if (isAnyNull[j]) {
            outLen[j] = 0;
            output[j] = nullptr;
            continue;
        }
        ret = ArenaAllocatorMalloc(contextPtr, strLen[j]);
        for (int32_t i = 0; i < strLen[j]; i++) {
            currItem = *(reinterpret_cast<char *>(str[j]) + i);
            if (currItem >= static_cast<int>('A') && currItem <= static_cast<int>('Z')) {
                *(ret + i) = static_cast<char>(currItem + STEP);
            } else {
                *(ret + i) = currItem;
            }
        }
        outLen[j] = strLen[j];
        output[j] = reinterpret_cast<uint8_t *>(ret);
    }
}

extern "C" DLLEXPORT void BatchToLowerChar(int64_t contextPtr, uint8_t **str, int32_t width, int32_t *strLen,
    bool *isAnyNull, uint8_t **output, int32_t *outLen, int32_t rowCnt)
{
    BatchToLowerStr(contextPtr, str, strLen, isAnyNull, output, outLen, rowCnt);
}

extern "C" DLLEXPORT void BatchJsonSplitStr(int64_t contextPtr, uint8_t **jsonStr, int32_t *jsonStrLen,
    bool *jsonStrIsNull, bool *outIsNull, uint8_t **output, int32_t *outLen, int32_t rowCnt)
{
    for (int32_t i = 0; i < rowCnt; ++i) {
        bool rowOutIsNull = false;
        int32_t rowOutLen = 0;
        const char *result = JsonSplitScalar(contextPtr,
            reinterpret_cast<const char *>(jsonStr[i]), jsonStrLen[i], jsonStrIsNull[i], &rowOutIsNull, &rowOutLen);
        outIsNull[i] = rowOutIsNull;
        outLen[i] = rowOutLen;
        output[i] = reinterpret_cast<uint8_t *>(const_cast<char *>(result));
    }
}

extern "C" DLLEXPORT void BatchJsonSplitChar(int64_t contextPtr, uint8_t **jsonStr, int32_t jsonStrWidth,
    int32_t *jsonStrLen, bool *jsonStrIsNull, bool *outIsNull, uint8_t **output, int32_t *outLen, int32_t rowCnt)
{
    static_cast<void>(jsonStrWidth);
    BatchJsonSplitStr(contextPtr, jsonStr, jsonStrLen, jsonStrIsNull, outIsNull, output, outLen, rowCnt);
}

extern "C" DLLEXPORT void BatchJsonValue(int64_t contextPtr, uint8_t **jsonStr, int32_t *jsonStrLen,
    bool *jsonStrIsNull, uint8_t **pathStr, int32_t pathStrWidth, int32_t *pathStrLen, bool *pathStrIsNull,
    bool *outIsNull, uint8_t **output, int32_t *outLen, int32_t rowCnt)
{
    for (int32_t i = 0; i < rowCnt; ++i) {
        bool rowOutIsNull = false;
        int32_t rowOutLen = 0;
        const char *result = JsonValueRetNull(contextPtr,
            reinterpret_cast<const char *>(jsonStr[i]), jsonStrLen[i], jsonStrIsNull[i],
            reinterpret_cast<const char *>(pathStr[i]), pathStrWidth, pathStrLen[i], pathStrIsNull[i],
            &rowOutIsNull, &rowOutLen);
        outIsNull[i] = rowOutIsNull;
        outLen[i] = rowOutLen;
        output[i] = reinterpret_cast<uint8_t *>(const_cast<char *>(result));
    }
}

extern "C" DLLEXPORT void BatchJsonValueWithBehaviors(int64_t contextPtr, uint8_t **jsonStr, int32_t *jsonStrLen,
    bool *jsonStrIsNull, uint8_t **pathStr, int32_t pathStrWidth, int32_t *pathStrLen, bool *pathStrIsNull,
    int32_t *emptyBehavior, bool *emptyBehaviorIsNull, uint8_t **defaultOnEmpty, int32_t *defaultOnEmptyLen,
    bool *defaultOnEmptyIsNull, int32_t *errorBehavior, bool *errorBehaviorIsNull, uint8_t **defaultOnError,
    int32_t *defaultOnErrorLen, bool *defaultOnErrorIsNull, bool *outIsNull, uint8_t **output, int32_t *outLen,
    int32_t rowCnt)
{
    for (int32_t i = 0; i < rowCnt; ++i) {
        bool rowOutIsNull = false;
        int32_t rowOutLen = 0;
        const char *result = JsonValueWithBehaviors(contextPtr,
            reinterpret_cast<const char *>(jsonStr[i]), jsonStrLen[i], jsonStrIsNull[i],
            reinterpret_cast<const char *>(pathStr[i]), pathStrWidth, pathStrLen[i], pathStrIsNull[i],
            emptyBehavior[i], emptyBehaviorIsNull[i], reinterpret_cast<const char *>(defaultOnEmpty[i]),
            defaultOnEmptyLen[i], defaultOnEmptyIsNull[i], errorBehavior[i], errorBehaviorIsNull[i],
            reinterpret_cast<const char *>(defaultOnError[i]), defaultOnErrorLen[i], defaultOnErrorIsNull[i],
            &rowOutIsNull, &rowOutLen);
        outIsNull[i] = rowOutIsNull;
        outLen[i] = rowOutLen;
        output[i] = reinterpret_cast<uint8_t *>(const_cast<char *>(result));
    }
}

extern "C" DLLEXPORT void BatchJsonQuery(int64_t contextPtr, uint8_t **jsonStr, int32_t *jsonStrLen,
    bool *jsonStrIsNull, uint8_t **pathStr, int32_t pathStrWidth, int32_t *pathStrLen, bool *pathStrIsNull,
    bool *outIsNull, uint8_t **output, int32_t *outLen, int32_t rowCnt)
{
    for (int32_t i = 0; i < rowCnt; ++i) {
        bool rowOutIsNull = false;
        int32_t rowOutLen = 0;
        const char *result = JsonQueryRetNull(contextPtr,
            reinterpret_cast<const char *>(jsonStr[i]), jsonStrLen[i], jsonStrIsNull[i],
            reinterpret_cast<const char *>(pathStr[i]), pathStrWidth, pathStrLen[i], pathStrIsNull[i],
            &rowOutIsNull, &rowOutLen);
        outIsNull[i] = rowOutIsNull;
        outLen[i] = rowOutLen;
        output[i] = reinterpret_cast<uint8_t *>(const_cast<char *>(result));
    }
}

extern "C" DLLEXPORT void BatchLikeStr(uint8_t **str, int32_t *strLen, uint8_t **regexToMatch, int32_t *regexLen,
    bool *isAnyNull, bool *output, int32_t rowCnt)
{
    std::string s;
    for (int32_t i = 0; i < rowCnt; i++) {
        if (isAnyNull[i]) {
            output[i] = false;
            continue;
        }
        s = std::string(reinterpret_cast<char *>(str[i]), strLen[i]);
        std::string r = std::string(reinterpret_cast<char *>(regexToMatch[i]), regexLen[i]);
        std::wregex re(StringUtil::ToWideString(r));
        output[i] = regex_match(StringUtil::ToWideString(s), re);
    }
}

extern "C" DLLEXPORT void BatchLikeChar(uint8_t **str, int32_t strWidth, int32_t *strLen, uint8_t **regexToMatch,
    int32_t *regexLen, bool *isAnyNull, bool *output, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; i++) {
        if (isAnyNull[i]) {
            output[i] = false;
            continue;
        }
        int32_t paddingCount =
            strWidth - omniruntime::Utf8Util::CountCodePoints(reinterpret_cast<const char *>(str[i]), strLen[i]);
        std::string originalStr;
        originalStr.reserve(strLen[i] + paddingCount);
        originalStr.append(reinterpret_cast<char *>(str[i]), strLen[i]);
        originalStr.append(paddingCount, ' ');
        std::string r = std::string(reinterpret_cast<char *>(regexToMatch[i]), regexLen[i]);
        std::wregex re(StringUtil::ToWideString(r));
        output[i] = regex_match(StringUtil::ToWideString(originalStr), re);
    }
}

extern "C" DLLEXPORT void BatchConcatStrStr(int64_t contextPtr, uint8_t **ap, int32_t *apLen, uint8_t **bp,
    int32_t *bpLen, bool *isAnyNull, uint8_t **output, int32_t *outLen, int32_t rowCnt)
{
    bool hasErr;
    for (int i = 0; i < rowCnt; i++) {
        if (isAnyNull[i]) {
            outLen[i] = 0;
            output[i] = nullptr;
            continue;
        }
        hasErr = false;
        auto ret = StringUtil::ConcatStrDiffWidths(contextPtr, reinterpret_cast<const char *>(ap[i]), apLen[i],
            reinterpret_cast<const char *>(bp[i]), bpLen[i], &hasErr, outLen + i);
        if (hasErr) {
            SetError(contextPtr, CONCAT_ERR_MSG);
            continue;
        }
        output[i] = reinterpret_cast<uint8_t *>(const_cast<char *>(ret));
    }
}

extern "C" DLLEXPORT void BatchConcatStrStrRetNull(bool *isNull, int64_t contextPtr, uint8_t **ap, int32_t *apLen,
    uint8_t **bp, int32_t *bpLen, uint8_t **output, int32_t *outLen, int32_t rowCnt)
{
    for (int32_t i = 0; i < rowCnt; i++) {
        auto ret = StringUtil::ConcatStrDiffWidths(contextPtr, reinterpret_cast<const char *>(ap[i]), apLen[i],
            reinterpret_cast<const char *>(bp[i]), bpLen[i], isNull + i, outLen + i);
        output[i] = reinterpret_cast<uint8_t *>(const_cast<char *>(ret));
    }
}

extern "C" DLLEXPORT void BatchConcatCharChar(int64_t contextPtr, uint8_t **ap, int32_t aWidth, int32_t *apLen,
    uint8_t **bp, int32_t bWidth, int32_t *bpLen, bool *isAnyNull, uint8_t **output, int32_t *outLen, int32_t rowCnt)
{
    bool hasErr;
    for (int32_t i = 0; i < rowCnt; i++) {
        if (isAnyNull[i]) {
            outLen[i] = 0;
            output[i] = nullptr;
            continue;
        }

        hasErr = false;
        auto ret = StringUtil::ConcatCharDiffWidths(contextPtr, reinterpret_cast<const char *>(ap[i]), aWidth, apLen[i],
            reinterpret_cast<const char *>(bp[i]), bpLen[i], &hasErr, outLen + i);
        if (hasErr) {
            SetError(contextPtr, CONCAT_ERR_MSG);
            continue;
        }
        output[i] = reinterpret_cast<uint8_t *>(const_cast<char *>(ret));
    }
}

extern "C" DLLEXPORT void BatchConcatCharCharRetNull(bool *isNull, int64_t contextPtr, uint8_t **ap, int32_t aWidth,
    int32_t *apLen, uint8_t **bp, int32_t bWidth, int32_t *bpLen, uint8_t **output, int32_t *outLen, int32_t rowCnt)
{
    for (int32_t i = 0; i < rowCnt; i++) {
        auto ret = StringUtil::ConcatCharDiffWidths(contextPtr, reinterpret_cast<const char *>(ap[i]), aWidth, apLen[i],
            reinterpret_cast<const char *>(bp[i]), bpLen[i], isNull + i, outLen + i);
        output[i] = reinterpret_cast<uint8_t *>(const_cast<char *>(ret));
    }
}

extern "C" DLLEXPORT void BatchConcatCharStr(int64_t contextPtr, uint8_t **ap, int32_t aWidth, int32_t *apLen,
    uint8_t **bp, int32_t *bpLen, bool *isAnyNull, uint8_t **output, int32_t *outLen, int32_t rowCnt)
{
    bool hasErr;
    for (int i = 0; i < rowCnt; i++) {
        if (isAnyNull[i]) {
            outLen[i] = 0;
            output[i] = nullptr;
            continue;
        }
        hasErr = false;
        auto ret = StringUtil::ConcatCharDiffWidths(contextPtr, reinterpret_cast<const char *>(ap[i]), aWidth, apLen[i],
            reinterpret_cast<const char *>(bp[i]), bpLen[i], &hasErr, outLen + i);
        if (hasErr) {
            SetError(contextPtr, CONCAT_ERR_MSG);
            continue;
        }
        output[i] = reinterpret_cast<uint8_t *>(const_cast<char *>(ret));
    }
}

extern "C" DLLEXPORT void BatchConcatCharStrRetNull(bool *isNull, int64_t contextPtr, uint8_t **ap, int32_t aWidth,
    int32_t *apLen, uint8_t **bp, int32_t *bpLen, uint8_t **output, int32_t *outLen, int32_t rowCnt)
{
    for (int32_t i = 0; i < rowCnt; i++) {
        auto ret = StringUtil::ConcatCharDiffWidths(contextPtr, reinterpret_cast<const char *>(ap[i]), aWidth, apLen[i],
            reinterpret_cast<const char *>(bp[i]), bpLen[i], isNull + i, outLen + i);

        output[i] = reinterpret_cast<uint8_t *>(const_cast<char *>(ret));
    }
}

extern "C" DLLEXPORT void BatchConcatStrChar(int64_t contextPtr, uint8_t **ap, int32_t *apLen, uint8_t **bp,
    int32_t bWidth, int32_t *bpLen, bool *isAnyNull, uint8_t **output, int32_t *outLen, int32_t rowCnt)
{
    bool hasErr;
    for (int32_t i = 0; i < rowCnt; i++) {
        if (isAnyNull[i]) {
            outLen[i] = 0;
            output[i] = nullptr;
            continue;
        }
        hasErr = false;
        auto ret = StringUtil::ConcatStrDiffWidths(contextPtr, reinterpret_cast<const char *>(ap[i]), apLen[i],
            reinterpret_cast<const char *>(bp[i]), bpLen[i], &hasErr, outLen + i);
        if (hasErr) {
            SetError(contextPtr, CONCAT_ERR_MSG);
            continue;
        }
        output[i] = reinterpret_cast<uint8_t *>(const_cast<char *>(ret));
    }
}

extern "C" DLLEXPORT void BatchConcatStrCharRetNull(bool *isNull, int64_t contextPtr, uint8_t **ap, int32_t *apLen,
    uint8_t **bp, int32_t bWidth, int32_t *bpLen, uint8_t **output, int32_t *outLen, int32_t rowCnt)
{
    for (int32_t i = 0; i < rowCnt; i++) {
        auto ret = StringUtil::ConcatStrDiffWidths(contextPtr, reinterpret_cast<const char *>(ap[i]), apLen[i],
            reinterpret_cast<const char *>(bp[i]), bpLen[i], isNull + i, outLen + i);
        output[i] = reinterpret_cast<uint8_t *>(const_cast<char *>(ret));
    }
}

extern "C" DLLEXPORT void BatchCastStrWithDiffWidths(int64_t contextPtr, uint8_t **str, int32_t srcWidth,
    int32_t *strLen, bool *isAnyNull, uint8_t **output, int32_t *outLen, int32_t dstWidth, int32_t rowCnt)
{
    for (int32_t i = 0; i < rowCnt; i++) {
        if (isAnyNull[i]) {
            outLen[i] = 0;
            output[i] = nullptr;
            continue;
        }
        bool hasErr = false;
        const char *ret = StringUtil::CastStrStr(&hasErr, reinterpret_cast<const char *>(str[i]), srcWidth, strLen[i],
            outLen + i, dstWidth);
        if (hasErr) {
            std::ostringstream errorMessage;
            errorMessage << "cast varchar[" << srcWidth << "] to varchar[" << dstWidth << "] failed.";
            SetError(contextPtr, errorMessage.str());
            continue;
        }
        output[i] = reinterpret_cast<uint8_t *>(const_cast<char *>(ret));
    }
}

extern "C" DLLEXPORT void BatchCastStrWithDiffWidthsRetNull(bool *isNull, int64_t contextPtr, uint8_t **srcStr,
    int32_t srcWidth, int32_t *strLen, uint8_t **output, int32_t *outLen, int32_t dstWidth, int32_t rowCnt)
{
    for (int32_t i = 0; i < rowCnt; ++i) {
        auto ret = StringUtil::CastStrStr(isNull + i, reinterpret_cast<const char *>(srcStr[i]), srcWidth, strLen[i],
            outLen + i, dstWidth);
        output[i] = reinterpret_cast<uint8_t *>(const_cast<char *>(ret));
    }
}

extern "C" DLLEXPORT void BatchLengthChar(uint8_t **str, const int32_t width, int32_t *strLen, bool *isAnyNull,
    int64_t *output, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        output[i] = width;
    }
}

extern "C" DLLEXPORT void BatchLengthCharReturnInt32(uint8_t **str, const int32_t width, int32_t *strLen,
    bool *isAnyNull, int32_t *output, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        output[i] = width;
    }
}

extern "C" DLLEXPORT void BatchLengthStr(uint8_t **str, int32_t *strLen, bool *isAnyNull, int64_t *output,
    int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        if (isAnyNull[i]) {
            output[i] = 0;
            continue;
        }
        output[i] = omniruntime::Utf8Util::CountCodePoints(reinterpret_cast<const char *>(str[i]), strLen[i]);
    }
}

extern "C" DLLEXPORT void BatchLengthStrReturnInt32(uint8_t **str, int32_t *strLen, bool *isAnyNull, int32_t *output,
    int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        if (isAnyNull[i]) {
            output[i] = 0;
            continue;
        }
        output[i] = omniruntime::Utf8Util::CountCodePoints(reinterpret_cast<const char *>(str[i]), strLen[i]);
    }
}

extern "C" DLLEXPORT void BatchReplaceStrStrStrWithRepNotReplace(int64_t contextPtr, uint8_t **str, int32_t *strLen,
    uint8_t **searchStr, int32_t *searchLen, uint8_t **replaceStr, int32_t *replaceLen, bool *isAnyNull,
    uint8_t **output, int32_t *outLen, int32_t rowCnt)
{
    ReplaceWithReplaceNotEmpty(contextPtr, str, strLen, searchStr, searchLen, replaceStr, replaceLen, isAnyNull, output,
        outLen, rowCnt, [str, strLen, outLen](bool *hasErr, int32_t i) -> uint8_t * {
            outLen[i] = strLen[i];
            return str[i];
        });
}

extern "C" DLLEXPORT void BatchReplaceStrStrWithoutRepNotReplace(int64_t contextPtr, uint8_t **str, int32_t *strLen,
    uint8_t **searchStr, int32_t *searchLen, bool *isAnyNull, uint8_t **output, int32_t *outLen, int32_t rowCnt)
{
    ReplaceWithReplaceEmpty(contextPtr, str, strLen, searchStr, searchLen, isAnyNull, output, outLen, rowCnt,
        [str, strLen, outLen](bool *hasErr, int32_t index) -> uint8_t * {
            outLen[index] = strLen[index];
            return str[index];
        });
}

extern "C" DLLEXPORT void BatchReplaceStrStrStrWithRepReplace(int64_t contextPtr, uint8_t **str, int32_t *strLen,
    uint8_t **searchStr, int32_t *searchLen, uint8_t **replaceStr, int32_t *replaceLen, bool *isAnyNull,
    uint8_t **output, int32_t *outLen, int32_t rowCnt)
{
    ReplaceWithReplaceNotEmpty(contextPtr, str, strLen, searchStr, searchLen, replaceStr, replaceLen, isAnyNull, output,
        outLen, rowCnt,
        [contextPtr, str, strLen, replaceStr, replaceLen, outLen](bool *hasErr, int32_t index) -> uint8_t * {
            auto result = StringUtil::ReplaceWithSearchEmpty(contextPtr, reinterpret_cast<const char *>(str[index]),
                strLen[index], reinterpret_cast<const char *>(replaceStr[index]), replaceLen[index], hasErr,
                outLen + index);
            return reinterpret_cast<uint8_t *>(const_cast<char *>(result));
        });
}

extern "C" DLLEXPORT void BatchReplaceStrStrWithoutRepReplace(int64_t contextPtr, uint8_t **str, int32_t *strLen,
    uint8_t **searchStr, int32_t *searchLen, bool *isAnyNull, uint8_t **output, int32_t *outLen, int32_t rowCnt)
{
    ReplaceWithReplaceEmpty(contextPtr, str, strLen, searchStr, searchLen, isAnyNull, output, outLen, rowCnt,
        [contextPtr, str, strLen, outLen](bool *hasErr, int32_t index) -> uint8_t * {
            auto result = StringUtil::ReplaceWithSearchEmpty(contextPtr, reinterpret_cast<const char *>(str[index]),
                strLen[index], reinterpret_cast<const char *>(EMPTY), 0, hasErr, outLen + index);
            return reinterpret_cast<uint8_t *>(const_cast<char *>(result));
        });
}

extern "C" DLLEXPORT void BatchInStr(char **srcStrs, int32_t *srcLens, char **subStrs, int32_t *subLens,
    bool *isAnyNull, int32_t *output, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        auto srcLen = srcLens[i];
        auto subLen = subLens[i];
        // currently return 0 if not found that means 1-based
        if (isAnyNull[i] || subLen > srcLen) {
            output[i] = 0;
            continue;
        }
        if (subLen == 0) {
            output[i] = 1;
            continue;
        }
        int32_t tailPos = srcLen - subLen;
        int32_t cmpLen = subLen - 1;
        auto srcStr = srcStrs[i];
        auto subStr = subStrs[i];
        int32_t result = 0;
        int32_t pos = 0;
        for (; pos <= tailPos; ++pos) {
            if (srcStr[pos] == subStr[0] && memcmp(srcStr + pos + 1, subStr + 1, cmpLen) == 0) {
                result = pos + 1;
                break;
            }
        }
        output[i] = result;
    }
}

extern "C" DLLEXPORT void BatchStartsWithStr(char **srcStrs, int32_t *srcLens, char **matchStrs, int32_t *matchLens,
    bool *isAnyNull, bool *output, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        auto srcLen = srcLens[i];
        auto matchLen = matchLens[i];
        if (isAnyNull[i] || matchLen > srcLen) {
            output[i] = false;
            continue;
        }
        if (matchLen == 0) {
            output[i] = true;
            continue;
        }
        output[i] = memcmp(srcStrs[i], matchStrs[i], matchLen) == 0;
    }
}

extern "C" DLLEXPORT void BatchEndsWithStr(char **srcStrs, int32_t *srcLens, char **matchStrs, int32_t *matchLens,
    bool *isAnyNull, bool *output, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        auto srcLen = srcLens[i];
        auto matchLen = matchLens[i];
        if (isAnyNull[i] || matchLen > srcLen) {
            output[i] = false;
            continue;
        }
        if (matchLen == 0) {
            output[i] = true;
            continue;
        }
        output[i] = memcmp(srcStrs[i] + srcLen - matchLen, matchStrs[i], matchLen) == 0;
    }
}

extern "C" DLLEXPORT void BatchMd5Str(int64_t contextPtr, uint8_t **str, int32_t *strLen, bool *isAnyNull,
    uint8_t **output, int32_t *outLen, int32_t rowCnt)
{
    for (int32_t i = 0; i < rowCnt; i++) {
        if (isAnyNull[i]) {
            outLen[i] = 0;
            output[i] = nullptr;
            continue;
        }
        Md5Function md5(reinterpret_cast<const char *>(str[i]), strLen[i]);
        outLen[i] = 32;
        char *mdString = ArenaAllocatorMalloc(contextPtr, 32);
        md5.FinishHex(mdString);
        output[i] = reinterpret_cast<uint8_t *>(const_cast<char *>(mdString));
    }
}

extern "C" DLLEXPORT void BatchEmptyToNull(char **str, int32_t *strLen, bool *isAnyNull, char **output, int32_t *outLen,
    int32_t rowCnt)
{
    for (int32_t i = 0; i < rowCnt; i++) {
        if (strLen[i] == 0 || isAnyNull[i]) {
            output[i] = nullptr;
            outLen[i] = 0;
            continue;
        }
        output[i] = str[i];
        outLen[i] = strLen[i];
    }
}

extern "C" DLLEXPORT void BatchContainsStr(char **srcStrs, int32_t *srcLens, char **matchStrs, int32_t *matchLens,
    bool *isAnyNull, bool *output, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        auto srcLen = srcLens[i];
        auto matchLen = matchLens[i];
        if (isAnyNull[i] || matchLen > srcLen) {
            output[i] = false;
            continue;
        }
        if (matchLen == 0) {
            output[i] = true;
            continue;
        }
        output[i] = StringUtil::StrContainsStr(srcStrs[i], srcLen, matchStrs[i], matchLen);
    }
}

extern "C" DLLEXPORT void BatchGreatestStr(uint8_t **xStr, int32_t *xStrLen, bool *xIsNull, uint8_t **yStr,
    int32_t *yStrLen, bool *yIsNull, bool *retIsNull, uint8_t **outStr, int32_t *outStrLen, int32_t rowCnt)
{
    int32_t cmpRet;
    for (int i = 0; i < rowCnt; ++i) {
        if (xIsNull[i] && yIsNull[i]) {
            retIsNull[i] = true;
            outStr[i] = nullptr;
            outStrLen[i] = 0;
            continue;
        }
        if (xIsNull[i]) {
            outStr[i] = yStr[i];
            outStrLen[i] = yStrLen[i];
            continue;
        }
        if (!yIsNull[i]) {
            cmpRet = memcmp(xStr[i], yStr[i], std::min(xStrLen[i], yStrLen[i]));
            if (cmpRet < 0 || (cmpRet == 0 && yStrLen[i] > xStrLen[i])) {
                outStr[i] = yStr[i];
                outStrLen[i] = yStrLen[i];
                continue;
            }
        }
        outStr[i] = xStr[i];
        outStrLen[i] = xStrLen[i];
    }
}

extern "C" DLLEXPORT void BatchStaticInvokeVarcharTypeWriteSideCheck(int64_t contextPtr, char **str, int32_t *strLen,
    int32_t limit, bool *isAnyNull, char **outputStr, int32_t *outputLen, int32_t rowCnt)
{
    for (int32_t i = 0; i < rowCnt; ++i) {
        if (isAnyNull[i]) {
            outputLen[i] = 0;
            outputStr[i] = nullptr;
            continue;
        }
        char *ss = str[i];
        int32_t len = strLen[i];
        int32_t ssLen = StringUtil::NumChars(ss, len);
        if (ssLen <= limit) {
            outputStr[i] = ss;
            outputLen[i] = len;
            continue;
        }
        int32_t numTailSpacesToTrim = ssLen - limit;
        int32_t endIdx = len - 1;
        int32_t trimTo = len - numTailSpacesToTrim;
        while (endIdx >= trimTo && ss[endIdx] == 0x20) {
            endIdx--;
        }
        int32_t outByteNum = endIdx + 1;
        if (ssLen > limit) {
            std::ostringstream errorMessage;
            errorMessage << "Exceeds varchar type length limitation: " << limit;
            SetError(contextPtr, errorMessage.str());
            outputLen[i] = 0;
            outputStr[i] = nullptr;
            continue;
        }
        auto padded = ArenaAllocatorMalloc(contextPtr, outByteNum);
        memcpy(padded, ss, outByteNum);

        padded[outByteNum] = '\0';
        outputLen[i] = outByteNum;
        outputStr[i] = padded;
    }
}

extern "C" DLLEXPORT void BatchStaticInvokeCharReadPadding(int64_t contextPtr, char **str,
    int32_t *strLen, int32_t limit, bool *isAnyNull, char **outputStr, int32_t *outputLen, int32_t rowCnt)
{
    for (int32_t i = 0; i < rowCnt; ++i) {
        if (isAnyNull[i]) {
            outputLen[i] = 0;
            outputStr[i] = nullptr;
            continue;
        } else if (strLen[i] == 0) {
            outputLen[i] = 0;
            outputStr[i] = "";
            continue;
        }
        char *ss = str[i];
        int32_t len = strLen[i];
        int32_t ssLen = StringUtil::NumChars(ss, len);
        if (ssLen >= limit) {
            outputStr[i] = ss;
            outputLen[i] = len;
            continue;
        }
        int32_t diff = limit - ssLen;
        int32_t outByteNum = len + diff + 1;
        auto padded = ArenaAllocatorMalloc(contextPtr, outByteNum);
        memcpy(padded, ss, len);
        memset(padded + len, ' ', diff);
        padded[outByteNum] = '\0';
        outputLen[i] = outByteNum - 1;
        outputStr[i] = padded;
    }
}
}