/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <limits>
#include <optional>
#include <memory>
#include "libboundscheck/include/securec.h"
#include "util/compiler_util.h"
#include "date32.h"

namespace omniruntime::type {
static inline int32_t GetNextDateForDayOfWeek(int32_t startDay, int32_t dayOfWeek)
{
    return startDay + 1 + ((dayOfWeek - 1 - startDay) % 7 + 7) % 7;
}

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

ALWAYS_INLINE char *IntToChars(char *first, char *last, int value)
{
    const int base = 10;
    char *current = last;
    bool isNegative = value < 0;

    // Handle the negative value by converting it to positive
    unsigned int absValue = isNegative ? -value : value;

    do {
        *--current = static_cast<char>(absValue % base) + '0';
        absValue /= base;
    } while (absValue != 0);

    if (isNegative) {
        *--current = '-';
    }

    // Copy the result to the beginning of the buffer
    char *ptr = first;
    while (current != last) {
        *ptr++ = *current++;
    }
    return ptr;
}

inline int64_t LeapThroughEndOf(int64_t y)
{
    // Add a large offset to make the calculation for negative years correct.
    y += LEAP_YEAR_OFFSET;
    return y / 4 - y / 100 + y / 400;
}

bool Timestamp::EpochToUtc(int64_t epoch, std::tm &tm)
{
    static constexpr int secondsPerHour = 3600;
    static constexpr int secondsPerDay = 24 * secondsPerHour;
    static constexpr int daysPerYear = 365;
    int64_t days = epoch / secondsPerDay;
    int64_t rem = epoch % secondsPerDay;
    while (rem < 0) {
        rem += secondsPerDay;
        --days;
    }
    tm.tm_hour = static_cast<int>(rem / secondsPerHour);
    rem = rem % secondsPerHour;
    tm.tm_min = static_cast<int>(rem / 60);
    tm.tm_sec = static_cast<int>(rem % 60);
    tm.tm_wday = static_cast<int>((4 + days) % 7);
    if (tm.tm_wday < 0) {
        tm.tm_wday += 7;
    }
    int64_t y = 1970;
    if (y + days / daysPerYear <= -LEAP_YEAR_OFFSET + 10) {
        return false;
    }
    bool isLeapYear;
    while (days < 0 || days >= daysPerYear + (isLeapYear = Date32::IsLeapYear(static_cast<int>(y)))) {
        auto newY = y + days / daysPerYear - (days < 0);
        days -= (newY - y) * daysPerYear + LeapThroughEndOf(newY - 1) - LeapThroughEndOf(y - 1);
        y = newY;
    }
    y -= TM_YEAR_BASE;
    if (y > std::numeric_limits<decltype(tm.tm_year)>::max() || y < std::numeric_limits<decltype(tm.tm_year)>::min()) {
        return false;
    }
    tm.tm_year = static_cast<int>(y);
    tm.tm_yday = static_cast<int>(days);
    auto *ip = MONTH_LENGTHS[isLeapYear];
    for (tm.tm_mon = 0; days >= ip[tm.tm_mon]; ++tm.tm_mon) {
        days = days - ip[tm.tm_mon];
    }
    tm.tm_mday = static_cast<int>(days + 1);
    tm.tm_isdst = 0;
    return true;
}

int64_t Timestamp::TmToStringView(const std::tm &tmValue, char *const startPosition)
{
    const auto appendDigits = [](const int value, const std::optional<uint32_t> minWidth, char *const position) {
        const auto numDigits = CountDigits(value);
        uint32_t offset = 0;
        // Append leading zeros when there is the requirement for minimum width.
        if (minWidth.has_value() && numDigits < minWidth.value()) {
            const auto leadingZeros = minWidth.value() - numDigits;
            memset_s(position, leadingZeros, '0', leadingZeros);
            offset += leadingZeros;
        }

        IntToChars(position + offset, position + offset + numDigits, value);
        offset += numDigits;
        return offset;
    };

    char *writePosition = startPosition;
    int year = TM_YEAR_BASE + tmValue.tm_year;
    const bool leadingPositiveSign = year > 9999;
    const bool negative = year < 0;

    // Sign.
    if (negative) {
        *writePosition++ = '-';
        year = -year;
    } else if (leadingPositiveSign) {
        *writePosition++ = '+';
    }

    // Year.
    writePosition += appendDigits(year, std::optional<uint32_t>(4), writePosition);

    // Month.
    *writePosition++ = '-';
    writePosition += appendDigits(1 + tmValue.tm_mon, 2, writePosition);

    // Day.
    *writePosition++ = '-';
    writePosition += appendDigits(tmValue.tm_mday, 2, writePosition);

    return (writePosition - startPosition);
}

Date32 &Date32::operator=(const Date32 &right)
{
    value = right.value;
    return *this;
}

Date32 &Date32::operator+=(const Date32 &right)
{
    value += right.value;
    return *this;
}

Date32 &Date32::operator-=(const Date32 &right)
{
    value += right.value;
    return *this;
}

bool Date32::operator==(const Date32 &right) const { return value == right.value; }

bool Date32::operator!=(const Date32 &right) const { return value != right.value; }

bool Date32::operator<(const Date32 &right) const { return value < right.value; }

bool Date32::operator>(const Date32 &right) const { return value > right.value; }

bool Date32::operator<=(const Date32 &right) const { return value <= right.value; }

bool Date32::operator>=(const Date32 &right) const { return value >= right.value; }

int Date32::StringToTm(const char *s, int32_t len, tm &r)
{
    int offset = 0;
    int count = 0;
    int year = 0;
    int mouth = 0;
    int day = 0;

    while (s[offset] == ' ') {
        offset++;
    }
    while (s[len - 1] == ' ') {
        len--;
    }
    // year
    while (offset < len) {
        if (count >= 4) {
            break;
        }
        if (!isdigit(s[offset])) {
            return -1;
        }

        year = year * 10 + s[offset] - 48;
        offset++;
        count++;
    }
    r.tm_year = year - 1900;
    if (offset == len && count == 4) {
        return 1;
    }
    if (s[offset] != '-' || count != 4) {
        return -1;
    }
    offset++;
    count = 0;

    // month
    while (offset < len) {
        if (count >= 2 || !isdigit(s[offset])) {
            break;
        }
        mouth = mouth * 10 + s[offset] - 48;
        offset++;
        count++;
    }
    r.tm_mon = mouth - 1;
    if (count == 1 || count == 2) {
        if (offset == len) {
            return 1;
        }
        if (s[offset] != '-') {
            return -1;
        }
    } else {
        return -1;
    }
    offset++;
    count = 0;

    // day
    while (offset < len) {
        if (count >= 2 || !isdigit(s[offset])) {
            break;
        }
        day = day * 10 + s[offset] - 48;
        offset++;
        count++;
    }
    r.tm_mday = day;

    if ((offset == len || s[offset] == ' ') && (count == 1 || count == 2)) {
        return 1;
    }
    return -1;
}

Status Date32::StringToDate32(const char *buf, int32_t len, int64_t &result)
{
    constexpr std::size_t YEAR_LENGTH = 4;
    int32_t pos = 0;
    if (len == 0) {
        return Status::IS_NOT_A_NUMBER;
    }

    int32_t day = 0;
    int32_t month = -1;
    int32_t year = 0;
    bool yearNeg = false;
    // Whether a sign is included in the date string.
    bool sign = false;
    int sep;

    while (pos < len && buf[len - 1] == ' ') {
        len--;
    }
    // Skip leading spaces.
    while (pos < len && buf[pos] == ' ') {
        pos++;
    }
    auto startPos = pos;
    if (pos >= len) {
        return Status::IS_NOT_A_NUMBER;
    }
    if (buf[pos] == '-') {
        sign = true;
        yearNeg = true;
        pos++;
        if (pos >= len) {
            return Status::IS_NOT_A_NUMBER;
        }
    } else if (buf[pos] == '+') {
        sign = true;
        pos++;
        if (pos >= len) {
            return Status::IS_NOT_A_NUMBER;
        }
    }

    if (!std::isdigit(buf[pos])) {
        return Status::IS_NOT_A_NUMBER;
    }
    // First parse the year.
    for (; pos < len && std::isdigit(buf[pos]); pos++) {
        int32_t tmpValue;
        if (!MulCheckedOverflow(year, 10, tmpValue)) {
            if (AddCheckedOverflow(static_cast<int32_t>(buf[pos] - '0'), tmpValue, year)) {
                return Status::CONVERT_OVERFLOW;
            }
        } else {
            return Status::CONVERT_OVERFLOW;
        }

        if (year > MAX_YEAR) {
            break;
        }
    }

    if (pos - startPos - sign < YEAR_LENGTH) {
        return Status::IS_NOT_A_NUMBER;
    }
    if (yearNeg) {
        if (NegateCheckedOverflow(year, year)) {
            return Status::CONVERT_OVERFLOW;
        }
        if (year < MIN_YEAR) {
            return Status::IS_NOT_A_NUMBER;
        }
    }

    // No month or day.
    if (pos == len) {
        if (!DaysSinceEpochFromDate(year, 1, 1, result)) {
            return Status::CONVERT_OVERFLOW;
        }
        return ValidDate(result) ? Status::CONVERT_SUCCESS : Status::CONVERT_OVERFLOW;
    }

    if (pos >= len) {
        return Status::IS_NOT_A_NUMBER;
    }

    // Fetch the separator.
    sep = buf[pos++];
    // Only '-' is valid for cast.
    if (sep != '-') {
        return Status::IS_NOT_A_NUMBER;
    }

    // Parse the month.
    if (!ParseDigit(buf, len, pos, month)) {
        return Status::IS_NOT_A_NUMBER;
    }

    // No day.
    if (pos == len) {
        if (!DaysSinceEpochFromDate(year, month, 1, result)) {
            return Status::IS_NOT_A_NUMBER;
        }
        return ValidDate(result) ? Status::CONVERT_SUCCESS : Status::CONVERT_OVERFLOW;
    }

    if (pos >= len) {
        return Status::IS_NOT_A_NUMBER;
    }

    if (buf[pos++] != sep) {
        return Status::IS_NOT_A_NUMBER;
    }

    if (pos >= len) {
        return Status::IS_NOT_A_NUMBER;
    }

    // Now parse the day.
    if (!ParseDigit(buf, len, pos, day)) {
        return Status::IS_NOT_A_NUMBER;
    }

    // In non-standard cast mode, an optional trailing 'T' or space followed
    // by any optional characters are valid patterns.
    if (!DaysSinceEpochFromDate(year, month, day, result)) {
        return Status::IS_NOT_A_NUMBER;
    }

    if (!ValidDate(result)) {
        return Status::IS_NOT_A_NUMBER;
    }

    if (pos == len) {
        return Status::CONVERT_SUCCESS;
    }

    if (buf[pos] == 'T' || buf[pos] == ' ') {
        return Status::CONVERT_SUCCESS;
    }
    return Status::IS_NOT_A_NUMBER;
}

// Do not use the standard library function std::localtime,
// which is not thread-safe.
size_t Date32::ToString(char *res, int32_t len) const
{
    int64_t daySeconds = value * static_cast<int64_t>(86400);
    std::tm tmValue{};
    Timestamp::EpochToUtc(daySeconds, tmValue);
    return Timestamp::TmToStringView(tmValue, res);
}

bool Date32::ValidDate(int64_t daysSinceEpoch)
{
    return daysSinceEpoch >= std::numeric_limits<int32_t>::min() &&
           daysSinceEpoch <= std::numeric_limits<int32_t>::max();
}

bool Date32::IsLeapYear(int32_t year) { return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0); }

bool Date32::IsValidDate(int32_t year, int32_t month, int32_t day)
{
    if (month < 1 || month > 12) {
        return false;
    }
    if (year < MIN_YEAR || year > MAX_YEAR) {
        return false;
    }
    if (day < 1) {
        return false;
    }
    return IsLeapYear(year) ? day <= LEAP_YEAR_OF_DAYS[month] : day <= NORMAL_YEAR_OF_DAYS[month];
}

bool Date32::ParseDigit(const char *buf, int32_t len, int32_t &pos, int32_t &result)
{
    if (pos < len && std::isdigit(buf[pos])) {
        result = static_cast<int32_t>(buf[pos++] - '0');
        if (pos < len && std::isdigit(buf[pos])) {
            result = static_cast<int32_t>(buf[pos++] - '0') + result * 10;
        }
        return true;
    }
    return false;
}

bool Date32::DaysSinceEpochFromDate(int32_t year, int32_t month, int32_t day, int64_t &out)
{
    int64_t daysSinceEpoch = 0;

    if (!IsValidDate(year, month, day)) {
        return false;
    }
    while (year < 1970) {
        year += YEAR_INTERVAL;
        daysSinceEpoch -= DAYS_PER_YEAR_INTERVAL;
    }
    while (year >= 2370) {
        year -= YEAR_INTERVAL;
        daysSinceEpoch += DAYS_PER_YEAR_INTERVAL;
    }
    daysSinceEpoch += CUMULATIVE_YEAR_DAYS[year - 1970];
    daysSinceEpoch += IsLeapYear(year) ? CUMULATIVE_LEAP_DAYS[month - 1] : CUMULATIVE_DAYS[month - 1];
    daysSinceEpoch += day - 1;
    out = daysSinceEpoch;
    return true;
}

DateTruncMode Date32::ParseTruncLevel(const std::string &format)
{
    std::string upperFormat = format;
    std::transform(upperFormat.begin(), upperFormat.end(), upperFormat.begin(), ::toupper);
    if (upperFormat.empty()) {
        return DateTruncMode::TRUNC_INVALID;
    } else {
        if (upperFormat == "MICROSECOND") {
            return DateTruncMode::TRUNC_TO_MICROSECOND;
        } else if (upperFormat == "MILLISECOND") {
            return DateTruncMode::TRUNC_TO_MILLISECOND;
        } else if (upperFormat == "SECOND") {
            return DateTruncMode::TRUNC_TO_SECOND;
        } else if (upperFormat == "MINUTE") {
            return DateTruncMode::TRUNC_TO_MINUTE;
        } else if (upperFormat == "HOUR") {
            return DateTruncMode::TRUNC_TO_HOUR;
        } else if (upperFormat == "DAY" || upperFormat == "DD") {
            return DateTruncMode::TRUNC_TO_DAY;
        } else if (upperFormat == "WEEK") {
            return DateTruncMode::TRUNC_TO_WEEK;
        } else if (upperFormat == "MON" || upperFormat == "MONTH" || upperFormat == "MM") {
            return DateTruncMode::TRUNC_TO_MONTH;
        } else if (upperFormat == "QUARTER") {
            return DateTruncMode::TRUNC_TO_QUARTER;
        } else if (upperFormat == "YEAR" || upperFormat == "YYYY" || upperFormat == "YY") {
            return DateTruncMode::TRUNC_TO_YEAR;
        } else {
            return DateTruncMode::TRUNC_INVALID;
        }
    }
}

Status Date32::TruncDate(int32_t days, DateTruncMode level, int32_t &result)
{
    switch (level) {
        case DateTruncMode::TRUNC_TO_WEEK:
            result = GetNextDateForDayOfWeek(days - 7, MONDAY);
            return Status::CONVERT_SUCCESS;
        case DateTruncMode::TRUNC_TO_MONTH:
            result = days - LocalDate(days).GetDay() + 1;
            return Status::CONVERT_SUCCESS;
        case DateTruncMode::TRUNC_TO_QUARTER:
            result = LocalDate(days).SetQuarter().ToDays();
            return Status::CONVERT_SUCCESS;
        case DateTruncMode::TRUNC_TO_YEAR:
            result = days - LocalDate(days).getDayOfYear() + 1;
            return Status::CONVERT_SUCCESS;
        default:
            // caller make sure that this should never be reached
            return Status::CONVERT_OVERFLOW;
    }
}

Date32 operator+(const Date32 &left, const Date32 &right) { return Date32(left.Value() + right.Value()); }

Date32 operator-(const Date32 &left, const Date32 &right) { return Date32(left.Value() - right.Value()); }
} // namespace omniruntime::type
