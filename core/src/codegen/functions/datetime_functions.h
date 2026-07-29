/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * Description: date time functions implementation
 */

#ifndef OMNI_RUNTIME_DATETIME_FUNCTIONS_H
#define OMNI_RUNTIME_DATETIME_FUNCTIONS_H

#include <cstdint>
#include <string>
// All extern functions go here temporarily
#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

namespace omniruntime::codegen::function {
extern "C" DLLEXPORT int64_t UnixTimestampFromStr(const char *timeStr, int32_t timeLen, bool isNullTimeStr,
    const char *fmtStr, int32_t fmtLen, bool isNullFmtStr, const char *tzStr, int32_t tzLen,
    bool isNullTzStr, const char *policyStr, int32_t policyLen, bool isNullPolStr, bool *retIsNull);

extern "C" DLLEXPORT int64_t UnixTimestampFromDate(int32_t date, const char *fmtStr, int32_t fmtLen,
    const char *tzStr, int32_t tzLen, const char *policyStr, int32_t policyLen, bool isNull);

extern "C" DLLEXPORT char *FromUnixTime(int64_t contextPtr, bool *isNull, int64_t timestamp, const char *fmtStr,
    int32_t fmtLen, const char *tzStr, int32_t tzLen, int32_t *outLen);

extern "C" DLLEXPORT char *FromUnixTimeRetNull(int64_t contextPtr, bool *isNull, int64_t timestamp, const char *fmtStr,
    int32_t fmtLen, const char *tzStr, int32_t tzLen, int32_t *outLen);

extern "C" DLLEXPORT char *FromUnixTimeWithoutTz(int64_t contextPtr, int64_t timestamp, const char *fmtStr,
                                                 int32_t fmtLen, bool isNull, int32_t *outLen);

extern "C" DLLEXPORT char *FromUnixTimeWithTz(int64_t contextPtr, int64_t timestamp, const char *fmtStr,
    int32_t fmtLen, const char *tzStr, int32_t tzLen, bool isNull, int32_t *outLen);

extern "C" DLLEXPORT int32_t DateTrunc(int64_t contextPtr, int32_t days, const char *levelStr, int32_t len);

extern "C" DLLEXPORT int32_t DateTruncRetNull(bool *isNull, int32_t days, const char *levelStr, int32_t len);

extern "C" DLLEXPORT int32_t DateAdd(int32_t right, int32_t left);

extern "C" DLLEXPORT int32_t HourFunction(int64_t timestamp);

extern "C" DLLEXPORT char *DateFormat(int64_t contextPtr, int64_t timestamp, const char *fmtStr, int32_t fmtLen,
    bool isNull, int32_t *outLen);

std::string toOmniTimeFormat(const std::string& format);

extern "C" DLLEXPORT int32_t GetHourFromTimestamp(int64_t timestamp, bool isNull);

extern "C" DLLEXPORT int32_t DateDiff(int32_t endDate, bool endIsNull, int32_t startDate, bool startIsNull, bool *retIsNull);

extern "C" DLLEXPORT int32_t GetHourFromTimestampWithTz(int64_t timestamp, const char *tzStr, int32_t tzLen, bool isNull);

extern "C" DLLEXPORT int64_t ToTimestampLtz(int64_t numeric, bool isNull1,
                                            int32_t precision, bool isNull2,
                                            bool* retIsNull);

extern "C" DLLEXPORT int64_t ToTimestampLtzInt(int32_t numeric, bool isNull1,
                                            int32_t precision, bool isNull2,
                                            bool* retIsNull);

extern "C" DLLEXPORT int64_t CurrentTimestamp();
extern "C" DLLEXPORT int64_t DateAddDays(int64_t timestamp, bool isNullTimestamp, int32_t days, bool isNullDays, bool* retIsNull);
}
#endif // OMNI_RUNTIME_DATETIME_FUNCTIONS_H
