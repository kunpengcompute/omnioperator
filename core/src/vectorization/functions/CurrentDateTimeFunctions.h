/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Current date/time functions - current_date(), current_timestamp(), current_row_timestamp(), localtime(), localtimestamp()
 */

#pragma once
#include "util/compiler_util.h"
#include "vectorization/Status.h"
#include "util/config/QueryConfig.h"
#include "util/omni_exception.h"
#include "type/data_type.h"
#include "type/Timestamp.h"
#include "type/date32.h"
#include "type/tz/TimeZoneMap.h"
#include <chrono>
#include <cstdint>
#include <ctime>
#include <vector>

namespace omniruntime::vectorization {

namespace detail {
/// UTC microseconds since the Unix epoch for a given time_point (true UTC instant).
inline int64_t UtcMicrosSinceEpoch(std::chrono::system_clock::time_point now)
{
    return std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
}

/// Returns the session timezone from the config, or nullptr if not set.
inline const tz::TimeZone *getSessionTimeZone(const config::QueryConfig &config)
{
    const auto sessionTzName = config.SessionTimezone();
    if (!sessionTzName.empty()) {
        return tz::locateZone(sessionTzName);
    }
    return nullptr;
}

/// Days since the Unix epoch for the session-local date.
/// Uses the session timezone when available; falls back to OS local time otherwise.
inline int32_t SessionLocalDaysSinceEpoch(std::chrono::system_clock::time_point now,
    const tz::TimeZone *sessionTz)
{
    if (sessionTz != nullptr) {
        Timestamp ts = Timestamp::fromMicros(UtcMicrosSinceEpoch(now));
        ts.toTimezone(*sessionTz);
        std::tm localTm {};
        OMNI_CHECK(Timestamp::epochToCalendarUtc(ts.getSeconds(), localTm),
            "Timestamp is too large: {} seconds since epoch", ts.getSeconds());
        int32_t year = localTm.tm_year + 1900;
        int32_t month = localTm.tm_mon + 1;
        int32_t day = localTm.tm_mday;
        int64_t daysSinceEpoch = 0;
        Date32::DaysSinceEpochFromDate(year, month, day, daysSinceEpoch);
        return static_cast<int32_t>(daysSinceEpoch);
    }
    auto secsSinceEpoch = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch());
    std::time_t timeNow = static_cast<std::time_t>(secsSinceEpoch.count());
    std::tm localTm {};
    localtime_r(&timeNow, &localTm);
    int32_t year = localTm.tm_year + 1900;
    int32_t month = localTm.tm_mon + 1;
    int32_t day = localTm.tm_mday;
    int64_t daysSinceEpoch = 0;
    Date32::DaysSinceEpochFromDate(year, month, day, daysSinceEpoch);
    return static_cast<int32_t>(daysSinceEpoch);
}

/// Microseconds since the Unix epoch treating the session-local wall-clock time
/// as if it were UTC (TIMESTAMP(6) semantics: utc_micros + session_tz_offset).
/// Uses the session timezone when available; falls back to OS local time otherwise.
inline int64_t SessionLocalWallClockAsUtcMicros(std::chrono::system_clock::time_point now,
    const tz::TimeZone *sessionTz)
{
    if (sessionTz != nullptr) {
        Timestamp ts = Timestamp::fromMicros(UtcMicrosSinceEpoch(now));
        ts.toTimezone(*sessionTz);
        return ts.toMicros();
    }
    static constexpr int64_t kMicrosPerSec = 1000000LL;
    static constexpr int64_t kSecsPerDay = 86400LL;

    auto durationSinceEpoch = now.time_since_epoch();
    auto secsSinceEpoch = std::chrono::duration_cast<std::chrono::seconds>(durationSinceEpoch);
    auto microsSinceEpoch = std::chrono::duration_cast<std::chrono::microseconds>(durationSinceEpoch);
    int64_t subSecondMicros = static_cast<int64_t>((microsSinceEpoch - secsSinceEpoch).count());

    std::time_t timeNow = static_cast<std::time_t>(secsSinceEpoch.count());
    std::tm localTm {};
    localtime_r(&timeNow, &localTm);

    int32_t year = localTm.tm_year + 1900;
    int32_t month = localTm.tm_mon + 1;
    int32_t day = localTm.tm_mday;
    int64_t daysSinceEpoch = 0;
    Date32::DaysSinceEpochFromDate(year, month, day, daysSinceEpoch);
    int64_t microsSinceMidnight = static_cast<int64_t>(
        localTm.tm_hour * 3600 + localTm.tm_min * 60 + localTm.tm_sec) * kMicrosPerSec;
    return daysSinceEpoch * kSecsPerDay * kMicrosPerSec + microsSinceMidnight + subSecondMicros;
}

/// Microseconds since midnight in the session timezone (TIME(6) semantics).
/// Uses the session timezone when available; falls back to OS local time otherwise
/// (aligned with Flink's ZoneId.systemDefault()).
inline int64_t SessionLocalMicrosSinceMidnight(std::chrono::system_clock::time_point now,
    const tz::TimeZone *sessionTz)
{
    if (sessionTz != nullptr) {
        int64_t utcMicros = UtcMicrosSinceEpoch(now);
        Timestamp ts = Timestamp::fromMicros(utcMicros);
        ts.toTimezone(*sessionTz);
        std::tm localTm {};
        OMNI_CHECK(Timestamp::epochToCalendarUtc(ts.getSeconds(), localTm),
            "Timestamp is too large: {} seconds since epoch", ts.getSeconds());
        return static_cast<int64_t>(localTm.tm_hour * 3600 + localTm.tm_min * 60 + localTm.tm_sec) * 1000000
            + static_cast<int64_t>(ts.getNanos() / 1'000);
    }
    auto durationSinceEpoch = now.time_since_epoch();
    auto secsSinceEpoch = std::chrono::duration_cast<std::chrono::seconds>(durationSinceEpoch);
    auto microsSinceEpoch = std::chrono::duration_cast<std::chrono::microseconds>(durationSinceEpoch);
    int64_t subSecondMicros = static_cast<int64_t>((microsSinceEpoch - secsSinceEpoch).count());
    std::time_t timeNow = static_cast<std::time_t>(secsSinceEpoch.count());
    std::tm localTm {};
    localtime_r(&timeNow, &localTm);
    return static_cast<int64_t>(localTm.tm_hour * 3600 + localTm.tm_min * 60 + localTm.tm_sec) * 1000000
        + subSecondMicros;
}
} // namespace detail

/// current_row_timestamp() -> int64_t (TIMESTAMP_LTZ(6))
template <typename T>
struct CurrentRowTimestampFunction {
    void initialize(const std::vector<omniruntime::type::DataTypeId> & /*inputTypes*/,
        const config::QueryConfig & /*config*/)
    {
    }

    ALWAYS_INLINE Status call(int64_t &result)
    {
        result = detail::UtcMicrosSinceEpoch(std::chrono::system_clock::now());
        return Status::OK();
    }
};

/// current_timestamp() -> int64_t (TIMESTAMP_LTZ(6))
template <typename T>
struct CurrentTimestampFunction {

    void initialize(const std::vector<omniruntime::type::DataTypeId> & /*inputTypes*/,
        const config::QueryConfig & /*config*/)
    {
        microsSinceEpoch_ = detail::UtcMicrosSinceEpoch(std::chrono::system_clock::now());
    }

    ALWAYS_INLINE Status call(int64_t &result)
    {
        result = microsSinceEpoch_;
        return Status::OK();
    }

private:
    int64_t microsSinceEpoch_ = 0;
};

/// localtime() -> int64_t (TIME(6))
template <typename T>
struct LocalTimeFunction {
    void initialize(const std::vector<omniruntime::type::DataTypeId> & /*inputTypes*/,
        const config::QueryConfig &config)
    {
        const tz::TimeZone *sessionTz = detail::getSessionTimeZone(config);
        microsSinceMidnight_ = detail::SessionLocalMicrosSinceMidnight(std::chrono::system_clock::now(), sessionTz);
    }

    ALWAYS_INLINE Status call(int64_t &result)
    {
        result = microsSinceMidnight_;
        return Status::OK();
    }

private:
    int64_t microsSinceMidnight_ = 0;
};

/// localtimestamp() -> int64_t (TIMESTAMP(6))
template <typename T>
struct LocalTimestampFunction {
    void initialize(const std::vector<omniruntime::type::DataTypeId> & /*inputTypes*/,
        const config::QueryConfig &config)
    {
        const tz::TimeZone *sessionTz = detail::getSessionTimeZone(config);
        microsSinceEpoch_ = detail::SessionLocalWallClockAsUtcMicros(std::chrono::system_clock::now(), sessionTz);
    }

    ALWAYS_INLINE Status call(int64_t &result)
    {
        result = microsSinceEpoch_;
        return Status::OK();
    }

private:
    int64_t microsSinceEpoch_ = 0;
};

/// current_date() -> int32_t (DATE)
template <typename T>
struct CurrentDateFunction {
    void initialize(const std::vector<omniruntime::type::DataTypeId> & /*inputTypes*/,
        const config::QueryConfig &config)
    {
        const tz::TimeZone *sessionTz = detail::getSessionTimeZone(config);
        daysSinceEpoch_ = detail::SessionLocalDaysSinceEpoch(std::chrono::system_clock::now(), sessionTz);
    }

    ALWAYS_INLINE Status call(int32_t &result)
    {
        result = daysSinceEpoch_;
        return Status::OK();
    }

private:
    int32_t daysSinceEpoch_ = 0;
};

} // namespace omniruntime::vectorization
