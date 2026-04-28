/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All rights reserved.
 * Description: date time functions implementation
 */

#include "datetime_functions.h"
#include "codegen/context_helper.h"
#include "type/date32.h"
#include "codegen/time_util.h"
#include "util/TimeUtils.h"
#include "type/tzdb/tzdb_list.h"
#include <algorithm>

namespace omniruntime::codegen::function {
namespace {
    constexpr std::pair<const char*, const char*> kOmniTimeFormatMap[] = {
        {"yyyy-MM-dd", "%Y-%m-%d"},
        {"yyyy-MM-dd HH:mm:ss", "%Y-%m-%d %H:%M:%S"},
        {"%Y-%m-%d %H:%M:%S", "%Y-%m-%d %H:%M:%S"},
        {"%Y-%m-%d", "%Y-%m-%d"},
    };

    constexpr size_t kOmniTimeFormatMapSize = std::size(kOmniTimeFormatMap);

    const char* FindOmniTimeFormat(std::string_view format) {
        for (size_t i = 0; i < kOmniTimeFormatMapSize; ++i) {
            if (format == kOmniTimeFormatMap[i].first) {
                return kOmniTimeFormatMap[i].second;
            }
        }
        return nullptr;
    }
} // anonymous namespace

extern "C" DLLEXPORT int64_t UnixTimestampFromStr(const char *timeStr, int32_t timeLen, bool isNullTimeStr,
    const char *fmtStr, int32_t fmtLen, bool isNullFmtStr, const char *tzStr, int32_t tzLen, bool isNullTzStr,
    const char *policyStr, int32_t policyLen, bool isNullPolStr, bool *retIsNull)
{
    if (isNullTimeStr || isNullFmtStr || fmtLen == 0 || timeLen == 0) {
        *retIsNull = true;
        return 0;
    }
    std::string timeStr1(timeStr, timeLen);
    std::string fmtStr1(fmtStr, fmtLen);
    std::string fmtOmniTimeStr = toOmniTimeFormat(fmtStr1);
    int fmtOmniTimeStrLen = fmtOmniTimeStr.length();
    if (!TimeUtil::IsTimeValid(timeStr, timeLen, fmtOmniTimeStr.c_str(),
                               fmtOmniTimeStrLen, policyStr)) {
        *retIsNull = true;
        return 0;
    }
    setenv("TZ", TimeZoneUtil::GetTZ(tzStr), 1);
    tzset();
    struct tm timeInfo = { 0 };
    strptime(timeStr1.c_str(), fmtOmniTimeStr.c_str(), &timeInfo);
    time_t timeStamp = mktime(&timeInfo);
    if (TimeZoneUtil::JudgeDSTByUnixTimestampFromStr(tzStr, tzLen, &timeInfo, timeStr, timeLen, fmtStr, fmtLen)) {
        timeStamp -= type::SECOND_OF_HOUR;
    }
    return timeStamp;
}

extern "C" DLLEXPORT int64_t UnixTimestampFromDate(int32_t date, const char *fmtStr, int32_t fmtLen,
   const char *tzStr, int32_t tzLen, const char *policyStr, int32_t policyLen, bool isNull)
{
    if (isNull) {
        return 0;
    }
    setenv("TZ", TimeZoneUtil::GetTZ(tzStr), 1);
    tzset();
    time_t desiredTime = type::SECOND_OF_DAY * date;
    struct tm ltm;
    localtime_r(&desiredTime, &ltm);
    time_t result = desiredTime - ltm.tm_gmtoff;
    result += TimeZoneUtil::AdjustDSTByUnixTimestampFromDate(tzStr, tzLen, &ltm, desiredTime) * type::SECOND_OF_HOUR;
    return static_cast<int64_t>(result);
}

extern "C" DLLEXPORT char *FromUnixTimeWithoutTz(int64_t contextPtr, int64_t timestamp, const char *fmtStr,
                                                   int32_t fmtLen, bool isNull, int32_t *outLen)
{
    if (isNull) {
        *outLen = 0;
        return nullptr;
    }

    struct tm ltm;
    int64_t adjusted_seconds = (timestamp >= 0) ? (timestamp / 1000) : ((timestamp - 999) / 1000);
    gmtime_r(&adjusted_seconds, &ltm);
    std::string fmt(fmtStr, fmtLen);
    int32_t resultLen = fmtLen + 3;
    auto result = ArenaAllocatorMalloc(contextPtr, resultLen);
    int ret = strftime(result, resultLen, fmt.c_str(), &ltm);
    *outLen = ret;
    return result;
}

extern "C" DLLEXPORT char *FromUnixTimeWithTz(int64_t contextPtr, int64_t timestamp, const char *fmtStr,
                                                       int32_t fmtLen, int64_t zoneOffsetSeconds, bool isNull, int32_t *outLen)
{
    if (isNull) {
        *outLen = 0;
        return nullptr;
    }

    struct tm ltm;
    int64_t adjusted_seconds = (timestamp >= 0) ? (timestamp / 1000) : ((timestamp - 999) / 1000);
    adjusted_seconds += zoneOffsetSeconds;
    gmtime_r(&adjusted_seconds, &ltm);
    std::string fmt(fmtStr, fmtLen);
    int32_t resultLen = fmtLen + 3;
    auto result = ArenaAllocatorMalloc(contextPtr, resultLen);
    int ret = strftime(result, resultLen, fmt.c_str(), &ltm);
    *outLen = ret;
    return result;
}

extern "C" DLLEXPORT char *FromUnixTime(int64_t contextPtr, bool *isNull, int64_t timestamp, const char *fmtStr,
    int32_t fmtLen, const char *tzStr, int32_t tzLen, int32_t *outLen)
{
    time_t timeStampVal = timestamp;
    setenv("TZ", TimeZoneUtil::GetTZ(tzStr), 1);
    tzset();
    struct tm ltm;
    localtime_r(&timeStampVal, &ltm);
    if (!TimeZoneUtil::JudgeDSTByFromUnixTime(tzStr, tzLen, &ltm)) {
        timeStampVal -= type::SECOND_OF_HOUR;
        localtime_r(&timeStampVal, &ltm);
    }
    int32_t resultLen = fmtLen + 3;
    auto result = ArenaAllocatorMalloc(contextPtr, resultLen);

    std::string localFmt;
    const char* finalFormat = nullptr;

    std::string_view fmtView(fmtStr, fmtLen);
    const char* cached = FindOmniTimeFormat(fmtView);
    if (cached) {
        finalFormat = cached;
    } else {
        localFmt = toOmniTimeFormat(std::string(fmtStr, fmtLen));
        finalFormat = localFmt.c_str();
    }

    auto ret = strftime(result, resultLen, finalFormat, &ltm);
    *isNull = static_cast<int32_t>(ret) == 0;
    *outLen = ret;
    return result;
}

std::string toOmniTimeFormat(const std::string &format)
{
    std::string result = format;
    const std::pair<std::string, std::string> replacements[] = {
        {"yyyy", "%Y"}, {"MM", "%m"}, {"dd", "%d"},
        {"HH", "%H"},   {"mm", "%M"}, {"ss", "%S"}};
    for (const auto &[from, to] : replacements) {
        size_t pos = 0;
        while ((pos = result.find(from, pos)) != std::string::npos) {
            result.replace(pos, from.length(), to);
            pos += to.length();
        }
    }
    return result;
}

extern "C" DLLEXPORT char *FromUnixTimeRetNull(int64_t contextPtr, bool *isNull, int64_t timestamp, const char *fmtStr,
    int32_t fmtLen, const char *tzStr, int32_t tzLen, int32_t *outLen)
{
    return FromUnixTime(contextPtr, isNull, timestamp, fmtStr, fmtLen, tzStr, tzLen, outLen);
}

extern "C" DLLEXPORT int32_t DateTrunc(int64_t contextPtr, int32_t days, const char *levelStr, int32_t len)
{
    type::DateTruncMode level = type::Date32::ParseTruncLevel(std::string(levelStr, len));
    int32_t result;
    if (type::Date32::TruncDate(days, level, result) != type::Status::CONVERT_SUCCESS) {
        std::ostringstream errorMessage;
        errorMessage << "The level is not supported yet: " << std::string(levelStr, len);
        SetError(contextPtr, errorMessage.str());
        return 0;
    }
    return result;
}

extern "C" DLLEXPORT int32_t DateTruncRetNull(bool *isNull, int32_t days, const char *levelStr, int32_t len)
{
    type::DateTruncMode level = type::Date32::ParseTruncLevel(std::string(levelStr, len));
    int32_t result = 0;
    if (type::Date32::TruncDate(days, level, result) != type::Status::CONVERT_SUCCESS) {
        *isNull = true;
    }
    return result;
}

extern "C" DLLEXPORT int32_t GetHourFromTimestamp(int64_t timestamp, bool isNull)
{
    if (isNull) {
        return 0;
    }
    int64_t totalHours = timestamp / type::SECOND_OF_HOUR / 1000;
    int32_t result = totalHours % 24;

    return result;
}

extern "C" DLLEXPORT int32_t GetHourFromTimestampWithTz(int64_t timestamp, int64_t zoneOffsetSeconds, bool isNull)
{
    return GetHourFromTimestamp(timestamp + zoneOffsetSeconds * 1000, isNull);
}

extern "C" DLLEXPORT int32_t DateAdd(int32_t right, int32_t left)
{
    return right + left;
}

extern "C" DLLEXPORT int32_t HourFunction(int64_t timestamp)
{
    auto t = Timestamp::fromMicros(timestamp);
    tz::TimeZone tz("Asia/Shanghai", 1980, tzdb::locate_zone("Asia/Shanghai"));
    return util::GetDateTime(t, &tz).tm_hour;
}

extern "C" DLLEXPORT char *DateFormat(int64_t contextPtr, int64_t timestamp, const char *fmtStr, int32_t fmtLen,
    bool isNull, int32_t *outLen)
{
    if (isNull) {
        *outLen = 0;
        return nullptr;
    }
    if (fmtStr == nullptr || std::memcmp(fmtStr, "yyyy-MM-dd", fmtLen) != 0 ) {
        *outLen = 0;
        SetError(contextPtr, " Error: date_format now only support formatStr = \"yyyy-MM-dd\" ! ");
        return nullptr;
    }
    const auto &queryConfig = GetQueryConfig(contextPtr);
    std::string sessionTimeZone = queryConfig.SessionTimezone();
    const char *tzStr = sessionTimeZone.empty() ? "UTC" : sessionTimeZone.c_str();
    time_t timeStampVal = timestamp / 1e6;
    setenv("TZ",TimeZoneUtil::GetTZ(tzStr), 1);
    tzset();
    struct tm ltm;
    localtime_r(&timeStampVal, &ltm);
    int32_t resultLen = fmtLen + 3;
    auto result = ArenaAllocatorMalloc(contextPtr, resultLen);
    std::string fmtStr1(fmtStr, fmtLen);
    std::string fmtOmniTimeStr = toOmniTimeFormat(fmtStr1);
    auto ret = strftime(result, resultLen, fmtOmniTimeStr.c_str(), &ltm);
    if (ret == 0) {
        SetError(contextPtr, " Error: date_format strftime failed ! ");
        *outLen = 0;
        return nullptr;
    }
    *outLen = ret;
    return result;
}

extern "C" DLLEXPORT int32_t DateDiff(int32_t endDate, bool endIsNull, int32_t startDate, bool startIsNull, bool *retIsNull)
{
    if (endIsNull || startIsNull) {
        *retIsNull = true;
        return 0;
    }
    return endDate - startDate;
}
extern "C" DLLEXPORT int64_t ToTimestampLtz(int64_t numeric, bool isNull1,
                                            int32_t precision, bool isNull2,
                                            bool* retIsNull) {
  if (isNull1 || isNull2) {
    *retIsNull = true;
    return 0;
  }
  *retIsNull = false;
  if (precision == 3) {
    return numeric;
  } else if (precision == 0) {
    return numeric * 1000;
  } else {
    return numeric;
  }
}
}
