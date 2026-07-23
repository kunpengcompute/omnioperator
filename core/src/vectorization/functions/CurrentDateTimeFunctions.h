/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Current date/time functions - current_timestamp(), current_row_timestamp(),localtime(), localtimestamp()
 */

#pragma once
#include "util/compiler_util.h"
#include "vectorization/Status.h"
#include "util/config/QueryConfig.h"
#include "type/data_type.h"
#include "type/date32.h"
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

/// Microseconds since midnight in UTC for a given time_point.
inline int64_t UtcMicrosSinceMidnight(std::chrono::system_clock::time_point now)
{
    auto durationSinceEpoch = now.time_since_epoch();
    auto secsSinceEpoch = std::chrono::duration_cast<std::chrono::seconds>(durationSinceEpoch);
    auto microsSinceEpoch = std::chrono::duration_cast<std::chrono::microseconds>(durationSinceEpoch);
    int64_t subSecondMicros = static_cast<int64_t>((microsSinceEpoch - secsSinceEpoch).count());

    std::time_t timeNow = static_cast<std::time_t>(secsSinceEpoch.count());
    std::tm tmVal {};
    gmtime_r(&timeNow, &tmVal);
    return static_cast<int64_t>(tmVal.tm_hour * 3600 + tmVal.tm_min * 60 + tmVal.tm_sec) * 1000000 + subSecondMicros;
}

/// Microseconds since the Unix epoch treating the local wall-clock time as if it were UTC (TIMESTAMP(6) semantics).
inline int64_t LocalWallClockAsUtcMicros(std::chrono::system_clock::time_point now)
{
    static constexpr int64_t kMicrosPerSec = 1000000LL;
    static constexpr int64_t kSecsPerDay = 86400LL;

    auto durationSinceEpoch = now.time_since_epoch();
    auto secsSinceEpoch = std::chrono::duration_cast<std::chrono::seconds>(durationSinceEpoch);
    auto microsSinceEpoch = std::chrono::duration_cast<std::chrono::microseconds>(durationSinceEpoch);
    int64_t subSecondMicros = static_cast<int64_t>((microsSinceEpoch - secsSinceEpoch).count());

    std::time_t timeNow = static_cast<std::time_t>(secsSinceEpoch.count());
    std::tm localTm {};
    localtime_r(&timeNow, &localTm);

    // Decompose local time into date + time-of-day, then compute epoch micros
    // treating the local wall-clock time as if it were UTC (TIMESTAMP semantics).
    int32_t year = localTm.tm_year + 1900;
    int32_t month = localTm.tm_mon + 1;
    int32_t day = localTm.tm_mday;
    int64_t daysSinceEpoch = 0;
    Date32::DaysSinceEpochFromDate(year, month, day, daysSinceEpoch);
    int64_t microsSinceMidnight = static_cast<int64_t>(
        localTm.tm_hour * 3600 + localTm.tm_min * 60 + localTm.tm_sec) * kMicrosPerSec;
    return daysSinceEpoch * kSecsPerDay * kMicrosPerSec + microsSinceMidnight + subSecondMicros;
}
} // namespace detail

/// current_row_timestamp() -> int64_t (TIMESTAMP_LTZ(6))
template <typename T>
struct CurrentRowTimestampFunction {
    /// Initialize the function by reading the current UTC timestamp from the system clock.
    void initialize(const std::vector<omniruntime::type::DataTypeId> & /*inputTypes*/,
        const config::QueryConfig & /*config*/)
    {
    }

    /// call() method for no-input function.
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
        const config::QueryConfig & /*config*/)
    {
        microsSinceMidnight_ = detail::UtcMicrosSinceMidnight(std::chrono::system_clock::now());
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
        const config::QueryConfig & /*config*/)
    {
        microsSinceEpoch_ = detail::LocalWallClockAsUtcMicros(std::chrono::system_clock::now());
    }

    ALWAYS_INLINE Status call(int64_t &result)
    {
        result = microsSinceEpoch_;
        return Status::OK();
    }

private:
    int64_t microsSinceEpoch_ = 0;
};

} // namespace omniruntime::vectorization
