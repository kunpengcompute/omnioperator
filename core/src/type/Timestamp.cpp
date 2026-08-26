//
// Created by root on 12/11/25.
//

#include "Timestamp.h"
#include "FastDate.h"
#include <algorithm>
#include <optional>
#include <charconv>
#include "type/tzdb/exception.h"
#include "type/date32.h"

namespace omniruntime {
namespace {
ALWAYS_INLINE uint32_t CountDigits(__uint128_t n)
{
    uint32_t count = 1;
    for (;;) {
        if (n < 10) {
            return count;
        }
        if (n < 100) {
            return count + 1;
        }
        if (n < 1000) {
            return count + 2;
        }
        if (n < 10000) {
            return count + 3;
        }
        n /= 10000u;
        count += 4;
    }
}
}

std::string::size_type getMaxStringLength(const TimestampToStringOptions &options)
{
    const auto precisionWidth = static_cast<int8_t>(options.precision);
    switch (options.mode) {
        case TimestampToStringOptions::Mode::kDateOnly:
            // Date format is %y-mm-dd, where y has 10 digits at maximum for int32.
            // Possible sign is considered.
            return 17;
        case TimestampToStringOptions::Mode::kTimeOnly:
            // hh:mm:ss.precision
            return 9 + precisionWidth;
        case TimestampToStringOptions::Mode::kFull:
            // Timestamp format is %y-%m-%dT%h:%m:%s.precision, where y has 10 digits
            // at maximum for int32. Possible sign is considered.
            return 27 + precisionWidth;
        default: OMNI_FAIL("Unsupport!");
    }
}

// static
Timestamp Timestamp::fromDaysAndNanos(int32_t days, int64_t nanos)
{
    int64_t seconds = (days - kJulianToUnixEpochDays) * kSecondsInDay + nanos / kNanosInSecond;
    int64_t remainingNanos = nanos % kNanosInSecond;
    if (remainingNanos < 0) {
        remainingNanos += kNanosInSecond;
        seconds--;
    }
    return Timestamp(seconds, remainingNanos);
}

// static
Timestamp Timestamp::fromDate(int32_t date)
{
    int64_t seconds = (int64_t) date * kSecondsInDay;
    return Timestamp(seconds, 0);
}

// static
Timestamp Timestamp::now()
{
    auto now = std::chrono::system_clock::now();
    auto epochMs = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    return fromMillis(epochMs);
}

std::string_view Timestamp::tsToStringView(const Timestamp &ts, const TimestampToStringOptions &options,
    char *const startPosition)
{
    std::tm tmValue;
    OMNI_CHECK(epochToCalendarUtc(ts.getSeconds(), tmValue), "Can't convert seconds to time: {}", ts.getSeconds());
    const uint64_t nanos = ts.getNanos();
    return tmToStringView(tmValue, nanos, options, startPosition);
}

namespace {
constexpr int kTmYearBase = 1900;
constexpr int64_t kLeapYearOffset = 4000000000LL;
constexpr int64_t kSecondsPerHour = 3600;
constexpr int64_t kSecondsPerDay = 24 * kSecondsPerHour;

inline bool isLeap(int64_t y)
{
    return y % 4 == 0 && (y % 100 != 0 || y % 400 == 0);
}

inline int64_t leapThroughEndOf(int64_t y)
{
    // Add a large offset to make the calculation for negative years correct.
    y += kLeapYearOffset;
    return y / 4 - y / 100 + y / 400;
}

inline int64_t daysBetweenYears(int64_t y1, int64_t y2)
{
    return 365 * (y2 - y1) + leapThroughEndOf(y2 - 1) - leapThroughEndOf(y1 - 1);
}

const int16_t daysBeforeFirstDayOfMonth[][12] = {
    {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334},
    {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335},
};

bool epochToCalendarUtcSlow(int64_t epoch, std::tm &tm)
{
    constexpr int kDaysPerYear = 365;
    int64_t days = epoch / kSecondsPerDay;
    int64_t rem = epoch % kSecondsPerDay;
    while (rem < 0) {
        rem += kSecondsPerDay;
        --days;
    }
    tm.tm_hour = rem / kSecondsPerHour;
    rem %= kSecondsPerHour;
    tm.tm_min = rem / 60;
    tm.tm_sec = rem % 60;
    tm.tm_wday = (4 + days) % 7;
    if (tm.tm_wday < 0) {
        tm.tm_wday += 7;
    }
    int64_t y = 1970;
    if (y + days / kDaysPerYear <= -kLeapYearOffset + 10) {
        return false;
    }
    bool leapYear;
    while (days < 0 || days >= kDaysPerYear + (leapYear = isLeap(y))) {
        auto newy = y + days / kDaysPerYear - (days < 0);
        days -= daysBetweenYears(y, newy);
        y = newy;
    }
    y -= kTmYearBase;
    if (y > std::numeric_limits<decltype(tm.tm_year)>::max() ||
        y < std::numeric_limits<decltype(tm.tm_year)>::min()) {
        return false;
    }
    tm.tm_year = y;
    tm.tm_yday = days;
    auto *months = daysBeforeFirstDayOfMonth[leapYear];
    tm.tm_mon = std::upper_bound(months, months + 12, days) - months - 1;
    tm.tm_mday = days - months[tm.tm_mon] + 1;
    tm.tm_isdst = 0;
    return true;
}
} // namespace

void Timestamp::toTimezone(const tz::TimeZone &zone)
{
    try {
        seconds_ = zone.to_local(std::chrono::seconds(seconds_)).count();
    } catch (const std::invalid_argument &e) {
        // Invalid argument means we hit a conversion not supported by
        // external/date. This is a special case where we intentionally throw
        // VeloxRuntimeError to avoid it being suppressed by TRY().
        OMNI_FAIL(e.what());
    }
}

void Timestamp::toGMT(const tz::TimeZone &zone)
{
    std::chrono::seconds sysSeconds;

    try {
        sysSeconds = zone.to_sys(std::chrono::seconds(seconds_));
    } catch (const std::invalid_argument &e) {
        // Invalid argument means we hit a conversion not supported by
        // external/date. Need to throw a RuntimeError so that try() statements do
        // not suppress it.
        OMNI_FAIL(e.what());
    }
    seconds_ = sysSeconds.count();
}

std::string_view Timestamp::tmToStringView(const std::tm &tmValue, uint64_t nanos,
    const TimestampToStringOptions &options, char *const startPosition)
{
    OMNI_CHECK(nanos<1'000'000'000, "");

    const auto appendDigits = [](const int value, const std::optional<uint32_t> minWidth, char *const position) {
        const auto numDigits = CountDigits(value);
        uint32_t offset = 0;
        // Append leading zeros when there is the requirement for minumum width.
        if (minWidth.has_value() && numDigits < minWidth.value()) {
            const auto leadingZeros = minWidth.value() - numDigits;
            std::memset(position, '0', leadingZeros);
            offset += leadingZeros;
        }
        const auto [endPosition, errorCode] = std::to_chars(position + offset, position + offset + numDigits, value);
        std::ignore = endPosition;
        OMNI_CHECK(errorCode == std::errc(), "Failed to convert value to varchar: {}.",
            std::make_error_code(errorCode).message());
        offset += numDigits;
        return offset;
    };

    char *writePosition = startPosition;
    if (options.mode != TimestampToStringOptions::Mode::kTimeOnly) {
        int year = kTmYearBase + tmValue.tm_year;
        const bool leadingPositiveSign = options.leadingPositiveSign && year > 9999;
        const bool negative = year < 0;

        // Sign.
        if (negative) {
            *writePosition++ = '-';
            year = -year;
        } else if (leadingPositiveSign) {
            *writePosition++ = '+';
        }

        // Year.
        writePosition += appendDigits(year, options.zeroPaddingYear ? std::optional<uint32_t>(4) : std::nullopt,
            writePosition);

        // Month.
        *writePosition++ = '-';
        writePosition += appendDigits(1 + tmValue.tm_mon, 2, writePosition);

        // Day.
        *writePosition++ = '-';
        writePosition += appendDigits(tmValue.tm_mday, 2, writePosition);

        if (options.mode == TimestampToStringOptions::Mode::kDateOnly) {
            return std::string_view(startPosition, writePosition - startPosition);
        }
        *writePosition++ = options.dateTimeSeparator;
    }

    // Hour.
    writePosition += appendDigits(tmValue.tm_hour, 2, writePosition);

    // Minute.
    *writePosition++ = ':';
    writePosition += appendDigits(tmValue.tm_min, 2, writePosition);

    // Second.
    *writePosition++ = ':';
    writePosition += appendDigits(tmValue.tm_sec, 2, writePosition);

    if (options.precision == TimestampToStringOptions::Precision::kMilliseconds) {
        nanos /= 1'000'000;
    } else if (options.precision == TimestampToStringOptions::Precision::kMicroseconds) {
        nanos /= 1'000;
    }
    if (options.skipTrailingZeros && nanos == 0) {
        return std::string_view(startPosition, writePosition - startPosition);
    }

    // Fractional part.
    *writePosition++ = '.';
    // Append leading zeros.
    const auto numDigits = CountDigits(nanos);
    const auto precisionWidth = static_cast<int8_t>(options.precision);
    std::memset(writePosition, '0', precisionWidth - numDigits);
    writePosition += precisionWidth - numDigits;

    // Append the remaining numeric digits.
    if (options.skipTrailingZeros) {
        std::optional<uint32_t> nonZeroOffset = std::nullopt;
        int32_t offset = numDigits - 1;
        // Write non-zero digits from end to start.
        while (nanos > 0) {
            if (nonZeroOffset.has_value() || nanos % 10 != 0) {
                *(writePosition + offset) = '0' + nanos % 10;
                if (!nonZeroOffset.has_value()) {
                    nonZeroOffset = offset;
                }
            }
            --offset;
            nanos /= 10;
        }
        writePosition += nonZeroOffset.value() + 1;
    } else {
        const auto [position, errorCode] = std::to_chars(writePosition, writePosition + numDigits, nanos);
        OMNI_CHECK(errorCode == std::errc(), "Failed to convert fractional part to chars: {}.",
            std::make_error_code(errorCode).message());
        writePosition = position;
    }
    return std::string_view(startPosition, writePosition - startPosition);
}

const tz::TimeZone &Timestamp::defaultTimezone()
{
    static const tz::TimeZone *kDefault = ( {
        // TODO: We are hard-coding PST/PDT here to be aligned with the current
        // behavior in DWRF reader/writer.  Once they are fixed, we can use
        // tzdb::current_zone() here.
        //
        // See https://github.com/facebookincubator/velox/issues/8127
        auto *tz = tz::locateZone("America/Los_Angeles");
        tz;
    });
    return *kDefault;
}

bool Timestamp::epochToCalendarUtc(int64_t epoch, std::tm &tm)
{
    int64_t days = epoch / kSecondsPerDay;
    int64_t rem = epoch % kSecondsPerDay;
    if (rem < 0) {
        rem += kSecondsPerDay;
        --days;
    }
    if (LIKELY(days >= fast_date::kRataDieMin && days <= fast_date::kRataDieMax)) {
        tm.tm_hour = rem / kSecondsPerHour;
        rem %= kSecondsPerHour;
        tm.tm_min = rem / 60;
        tm.tm_sec = rem % 60;
        tm.tm_wday = (4 + days) % 7;
        if (tm.tm_wday < 0) {
            tm.tm_wday += 7;
        }

        const auto ymd = daysToYmd(static_cast<int32_t>(days));
        const int64_t year = static_cast<int64_t>(ymd.year) - kTmYearBase;
        if (year > std::numeric_limits<decltype(tm.tm_year)>::max() ||
            year < std::numeric_limits<decltype(tm.tm_year)>::min()) {
            return false;
        }
        tm.tm_year = static_cast<int>(year);
        const auto *monthOffsets = daysBeforeFirstDayOfMonth[isLeap(ymd.year)];
        tm.tm_mon = static_cast<int>(ymd.month) - 1;
        tm.tm_mday = static_cast<int>(ymd.day);
        tm.tm_yday = monthOffsets[tm.tm_mon] + tm.tm_mday - 1;
        tm.tm_isdst = 0;
        return true;
    }
    return epochToCalendarUtcSlow(epoch, tm);
}

// static
int64_t Timestamp::calendarUtcToEpoch(const std::tm &tm)
{
    static_assert(sizeof(decltype(tm.tm_year)) == 4);
    // tm_year stores number of years since 1900.
    int64_t year = tm.tm_year + 1900LL;
    int64_t month = tm.tm_mon;
    if (UNLIKELY(month > 11)) {
        year += month / 12;
        month %= 12;
    } else if (UNLIKELY(month < 0)) {
        auto yearsDiff = (-month + 11) / 12;
        year -= yearsDiff;
        month += 12 * yearsDiff;
    }
    // Getting number of days since beginning of the year.
    auto dayOfYear = -1LL + daysBeforeFirstDayOfMonth[isLeap(year)][month] + tm.tm_mday;
    // Number of days since 1970-01-01.
    auto daysSinceEpoch = daysBetweenYears(1970, year) + dayOfYear;
    return kSecondsPerDay * daysSinceEpoch + kSecondsPerHour * tm.tm_hour + 60LL * tm.tm_min + tm.tm_sec;
}

namespace {
constexpr int64_t kSecondsPerMinute = 60;
}

// static
type::Status Timestamp::FloorTime(const Timestamp &ts, type::DateTruncMode level, Timestamp &result)
{
    int64_t seconds = ts.getSeconds();
    uint64_t nanos = ts.getNanos();

    int64_t days = seconds / kSecondsInDay;
    int64_t secsOfDay = seconds % kSecondsInDay;
    if (secsOfDay < 0) {
        secsOfDay += kSecondsInDay;
        --days;
    }

    switch (level) {
        case type::DateTruncMode::TRUNC_TO_SECOND:
            nanos = 0;
            break;
        case type::DateTruncMode::TRUNC_TO_MINUTE:
            secsOfDay = secsOfDay / kSecondsPerMinute * kSecondsPerMinute;
            nanos = 0;
            break;
        case type::DateTruncMode::TRUNC_TO_HOUR:
            secsOfDay = secsOfDay / kSecondsPerHour * kSecondsPerHour;
            nanos = 0;
            break;
        case type::DateTruncMode::TRUNC_TO_DAY:
            secsOfDay = 0;
            nanos = 0;
            break;
        case type::DateTruncMode::TRUNC_TO_WEEK: {
            constexpr int64_t kSunday = 3;
            days -= ((days - kSunday) % 7 + 7) % 7;
            secsOfDay = 0;
            nanos = 0;
            break;
        }
        case type::DateTruncMode::TRUNC_TO_MONTH: {
            LocalDate ld(static_cast<int32_t>(days));
            days = days - ld.GetDay() + 1;
            secsOfDay = 0;
            nanos = 0;
            break;
        }
        case type::DateTruncMode::TRUNC_TO_QUARTER: {
            LocalDate ld(static_cast<int32_t>(days));
            ld.SetQuarter();
            days = ld.ToDays();
            secsOfDay = 0;
            nanos = 0;
            break;
        }
        case type::DateTruncMode::TRUNC_TO_YEAR: {
            LocalDate ld(static_cast<int32_t>(days));
            days = days - ld.getDayOfYear() + 1;
            secsOfDay = 0;
            nanos = 0;
            break;
        }
        default:
            return type::Status::CONVERT_OVERFLOW;
    }

    result = Timestamp(days * kSecondsInDay + secsOfDay, nanos);
    return type::Status::CONVERT_SUCCESS;
}

// static
type::Status Timestamp::CeilTime(const Timestamp &ts, type::DateTruncMode level, Timestamp &result)
{
    int64_t seconds = ts.getSeconds();
    uint64_t nanos = ts.getNanos();

    int64_t days = seconds / kSecondsInDay;
    int64_t secsOfDay = seconds % kSecondsInDay;
    if (secsOfDay < 0) {
        secsOfDay += kSecondsInDay;
        --days;
    }

    switch (level) {
        case type::DateTruncMode::TRUNC_TO_SECOND: {
            if (nanos > 0) {
                nanos = 0;
                ++secsOfDay;
                if (secsOfDay >= kSecondsInDay) {
                    secsOfDay -= kSecondsInDay;
                    ++days;
                }
            } else {
                nanos = 0;
            }
            break;
        }
        case type::DateTruncMode::TRUNC_TO_MINUTE: {
            int64_t floorSecs = secsOfDay / kSecondsPerMinute * kSecondsPerMinute;
            if (floorSecs != secsOfDay || nanos > 0) {
                floorSecs += kSecondsPerMinute;
                if (floorSecs >= kSecondsInDay) {
                    floorSecs -= kSecondsInDay;
                    ++days;
                }
            }
            secsOfDay = floorSecs;
            nanos = 0;
            break;
        }
        case type::DateTruncMode::TRUNC_TO_HOUR: {
            int64_t floorSecs = secsOfDay / kSecondsPerHour * kSecondsPerHour;
            if (floorSecs != secsOfDay || nanos > 0) {
                floorSecs += kSecondsPerHour;
                if (floorSecs >= kSecondsInDay) {
                    floorSecs -= kSecondsInDay;
                    ++days;
                }
            }
            secsOfDay = floorSecs;
            nanos = 0;
            break;
        }
        case type::DateTruncMode::TRUNC_TO_DAY: {
            if (secsOfDay != 0 || nanos != 0) {
                ++days;
            }
            secsOfDay = 0;
            nanos = 0;
            break;
        }
        case type::DateTruncMode::TRUNC_TO_WEEK: {
            constexpr int64_t kSunday = 3; // 目前定位在周天，等查看flink原生是否一致
            int64_t offset = ((days - kSunday) % 7 + 7) % 7;
            days -= offset;
            if (offset != 0) {
                days += 7;
            }
            secsOfDay = 0;
            nanos = 0;
            break;
        }
        case type::DateTruncMode::TRUNC_TO_MONTH: {
            LocalDate ld(static_cast<int32_t>(days));
            int16_t day = ld.GetDay();
            int64_t floorDays = days - day + 1;
            if (day == 1) {
                days = floorDays;
            } else {
                int32_t base = static_cast<int32_t>(floorDays);
                int32_t d = 28;
                while (LocalDate(base + d).GetDay() != 1) {
                    ++d;
                }
                days = base + d;
            }
            secsOfDay = 0;
            nanos = 0;
            break;
        }
        case type::DateTruncMode::TRUNC_TO_QUARTER: {
            LocalDate ld(static_cast<int32_t>(days));
            ld.SetQuarter();
            int32_t floorDays = ld.ToDays();
            if (static_cast<int32_t>(days) == floorDays) {
                days = floorDays;
            } else {
                int32_t base = floorDays;
                for (int i = 0; i < 3; ++i) {
                    int32_t d = 28;
                    while (LocalDate(base + d).GetDay() != 1) {
                        ++d;
                    }
                    base += d;
                }
                days = base;
            }
            secsOfDay = 0;
            nanos = 0;
            break;
        }
        case type::DateTruncMode::TRUNC_TO_YEAR: {
            LocalDate ld(static_cast<int32_t>(days));
            int32_t dayOfYear = ld.getDayOfYear();
            int32_t floorDays = static_cast<int32_t>(days) - dayOfYear + 1;
            if (dayOfYear == 1) {
                days = floorDays;
            } else {
                bool leap = LocalDate(floorDays).IsLeapYear();
                days = floorDays + 365 + (leap ? 1 : 0);
            }
            secsOfDay = 0;
            nanos = 0;
            break;
        }
        default:
            return type::Status::CONVERT_OVERFLOW;
    }

    result = Timestamp(days * kSecondsInDay + secsOfDay, nanos);
    return type::Status::CONVERT_SUCCESS;
}
}
