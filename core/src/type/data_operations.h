/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2024. All rights reserved.
 * Description: decimal128 utils
 */

#ifndef OMNI_RUNTIME_DATA_OPERATIONS_H
#define OMNI_RUNTIME_DATA_OPERATIONS_H

#include <string>
#include <regex>
#include <limits>
#include <ctime>
#include "util/compiler_util.h"
#include "base_operations.h"
#include "double_utils.h"
#include <climits>

namespace omniruntime {
namespace type {
static std::regex g_decimalRegex("[+-]?[[:digit:]]+([.][[:digit:]]+)?([eE][+-]?[[:digit:]]+)?");
static std::regex g_doubleRegex(
    "[[:blank:]]*([+-])?[[:digit:]]+([.][[:digit:]]+)?([eE][+-]?[[:digit:]]+)?[[:blank:]]*");
static std::regex g_dateRegex(R"([[:blank:]]*\d{1,5}-\d{2}-\d{2}[[:blank:]]*$)");

template<typename T, bool allowTruncate = true>
inline Status ConvertStringToInteger(T &result, const char *bytes, int length)
{
    static_assert(std::is_same_v<T, int8_t> || std::is_same_v<T, int16_t> || std::is_same_v<T, int32_t> ||
        std::is_same_v<T, int64_t>, "Only integer type is allowed");
    int offset = 0;
    while (offset < length && bytes[offset] == ' ') offset++;
    if (offset == length) return Status::IS_NOT_A_NUMBER;

    int end = length - 1;
    while (end > offset && bytes[end] == ' ') end--;

    char b = bytes[offset];
    bool negative = b == '-';
    if (negative || b == '+') {
        if (end - offset == 0) {
            return Status::IS_NOT_A_NUMBER;
        }
        offset++;
    }

    char separator = '.';
    constexpr int radix = 10;
    constexpr T minValue = std::is_same_v<T, int8_t> ? INT8_MIN : std::is_same_v<T, int16_t> ? INT16_MIN
        : std::is_same_v<T, int32_t> ? INT32_MIN : INT64_MIN;
    constexpr T stopValue = minValue / radix;

    result = 0;
    while (offset <= end) {
        b = bytes[offset];
        offset++;
        if constexpr (allowTruncate) {
            if (b == separator) {
                break;
            }
        }

        int digit;
        if (b >= '0' && b <= '9') {
            digit = b - '0';
        } else {
            return Status::IS_NOT_A_NUMBER;
        }

        if ((result < stopValue) || (result == stopValue && digit > 8)) {
            return Status::CONVERT_OVERFLOW;
        }

        result = result * radix - digit;
    }

    while (offset <= end) {
        char currentByte = bytes[offset];
        if (currentByte < '0' || currentByte > '9') {
            return Status::IS_NOT_A_NUMBER;
        }
        offset++;
    }

    if (!negative) {
        if (result == minValue) {
            return Status::CONVERT_OVERFLOW;
        }
        result = -result;
    }
    return Status::CONVERT_SUCCESS;
}

inline Status ConvertDateStringToInteger(int64_t& result, const char* bytes, int length)
{
    int offset = 0;
    while (offset < length && bytes[offset] == ' ') offset++;
    if (offset == length) return Status::IS_NOT_A_NUMBER;

    int end = length - 1;
    while (end > offset && bytes[end] == ' ') end--;
    end++;

    int contentLen = end - offset;
    if (contentLen < 10) return Status::IS_NOT_A_NUMBER;

    int dash1 = -1, dash2 = -1;
    for (int i = offset; i < end; i++) {
        char c = bytes[i];
        if (c == '-') {
            if (dash1 == -1) dash1 = i;
            else if (dash2 == -1) dash2 = i;
            else return Status::IS_NOT_A_NUMBER;
        } else if (c < '0' || c > '9') {
            return Status::IS_NOT_A_NUMBER;
        }
    }
    if (dash1 == -1 || dash2 == -1) return Status::IS_NOT_A_NUMBER;

    int yearLen = dash1 - offset;
    int monthLen = dash2 - dash1 - 1;
    int dayLen = end - dash2 - 1;
    if (yearLen < 1 || monthLen != 2 || dayLen != 2) return Status::IS_NOT_A_NUMBER;

    int year, month, day;
    char temp[16];
    int copyLen = contentLen < 15 ? contentLen : 15;
    memcpy(temp, bytes + offset, copyLen);
    temp[copyLen] = '\0';

    if (sscanf(temp, "%d-%d-%d", &year, &month, &day) != 3) {
        return Status::IS_NOT_A_NUMBER;
    }

    if (year < 0 || year > 9999) {
        return Status::CONVERT_OVERFLOW;
    }
    if (month < 1 || month > 12) {
        return Status::IS_NOT_A_NUMBER;
    }
    if (day < 1 || day > 31) {
        return Status::IS_NOT_A_NUMBER;
    }

    static const int daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int maxDays = daysInMonth[month - 1];
    if (month == 2 && (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0))) {
        maxDays = 29;
    }
    if (day > maxDays) {
        return Status::IS_NOT_A_NUMBER;
    }

    struct tm timeinfo = {0};
    timeinfo.tm_year = year - 1900;
    timeinfo.tm_mon = month - 1;
    timeinfo.tm_mday = day;
    timeinfo.tm_hour = 0;
    timeinfo.tm_min = 0;
    timeinfo.tm_sec = 0;
    timeinfo.tm_isdst = 0;

    time_t timestamp = timegm(&timeinfo);
    if (timestamp == -1) {
        return Status::CONVERT_OVERFLOW;
    }

    result = static_cast<int64_t>(timestamp) * 1000;
    return Status::CONVERT_SUCCESS;
}

inline int StringToDouble(const std::string &s, double &result)
{
    if (!regex_match(s, g_doubleRegex)) {
        return -1;
    }
    int status = 0;
    try {
        result = stod(s);
    } catch (std::exception &e) {
        status = 1;
    }
    return status;
}

inline Status ConvertStringToDouble(double &result, const char *bytes, int32_t length)
{
    const int kMaxSignificantDigits = 772;
    int offset = 0;
    while (offset < length && bytes[offset] == ' ') offset++;
    if (offset == length) return Status::IS_NOT_A_NUMBER;

    int end = length - 1;
    while (end > offset && bytes[end] == ' ') end--;
    end++;

    int exponent = 0;
    int significantDigits = 0;
    int insignificantDigits = 0;
    bool nonzeroDigitDropped = false;

    bool sign = false;
    if (bytes[offset] == '+' || bytes[offset] == '-') {
        sign = (bytes[offset] == '-');
        offset++;
        if (offset == end) {
            return Status::IS_NOT_A_NUMBER;
        }
        if (bytes[offset] == ' ') {
            return Status::IS_NOT_A_NUMBER;
        }
    }

    const char *nanSymbol = "nan\0";
    if (std::tolower(bytes[offset]) == 'n') {
        if (!ConsumeSubString(bytes, offset, end, nanSymbol)) {
            return Status::IS_NOT_A_NUMBER;
        }
        if (offset != end) {
            return Status::IS_NOT_A_NUMBER;
        }
        result = sign ? -Double::NaN() : Double::NaN();
        return Status::CONVERT_SUCCESS;
    }

    const char *infinitySymbol = "infinity\0";
    if (std::tolower(bytes[offset]) == 'i') {
        if (!ConsumeSubString(bytes, offset, end, infinitySymbol)) {
            return Status::IS_NOT_A_NUMBER;
        }
        if (offset != end) {
            return Status::IS_NOT_A_NUMBER;
        }
        result = sign ? -Double::Infinity() : Double::Infinity();
        return Status::CONVERT_SUCCESS;
    }

    bool leadingZero = false;
    if (bytes[offset] == '0') {
        offset++;
        if (offset == end) {
            result = sign ? -0.0 : 0.0;
            return Status::CONVERT_SUCCESS;
        }
    }
    leadingZero = true;

    while (offset < length && bytes[offset] == '0') offset++;
    if (offset == length) {
        result = sign ? -0.0 : 0.0;
        return Status::CONVERT_SUCCESS;
    }

    const int kBufferSize = kMaxSignificantDigits + 10;
    char buffer[kBufferSize];
    int bufferPos = 0;
    while (bytes[offset] >= '0' && bytes[offset] <= '9') {
        if (significantDigits < kMaxSignificantDigits) {
            buffer[bufferPos++] = static_cast<char>(bytes[offset]);
            significantDigits++;
            // Will later check if it's an octal in the buffer.
        } else {
            insignificantDigits++;  // Move the digit into the exponential part.
            nonzeroDigitDropped = nonzeroDigitDropped || bytes[offset] != '0';
        }
        offset++;
        if (offset == end) goto parsing_done;
    }

    if (bytes[offset] == '.') {
        offset++;
        if (length == 1) return Status::IS_NOT_A_NUMBER;
        if (offset == end) {
            if (significantDigits == 0 && !leadingZero) {
                return Status::IS_NOT_A_NUMBER;
            } else {
                goto parsing_done;
            }
        }

        if (significantDigits == 0) {
            // Integer part consists of 0 or is absent. Significant digits start after
            // leading zeros (if any).
            while (bytes[offset] == '0') {
                offset++;
                if (offset == end) {
                    result = sign ? -0.0 : 0.0;
                    return Status::CONVERT_SUCCESS;
                }
                exponent--;  // Move this 0 into the exponent.
            }
        }

        // There is a fractional part.
        // We don't emit a '.', but adjust the exponent instead.
        while (bytes[offset] >= '0' && bytes[offset] <= '9') {
            if (significantDigits < kMaxSignificantDigits) {
                buffer[bufferPos++] = static_cast<char>(bytes[offset]);
                significantDigits++;
                exponent--;
            } else {
                // Ignore insignificant digits in the fractional part.
                nonzeroDigitDropped = nonzeroDigitDropped || bytes[offset] != '0';
            }
            offset++;
            if (offset == end) goto parsing_done;
        }
    }

    if (bytes[offset] == 'e' || bytes[offset] == 'E') {
        offset++;
        if (offset == end) {
            return Status::IS_NOT_A_NUMBER;
        }
        char exponentSign = '+';
        if (bytes[offset] == '+' || bytes[offset] == '-') {
            exponentSign = static_cast<char>(bytes[offset]);
            offset++;
            if (offset == end) {
                return Status::IS_NOT_A_NUMBER;
            }
        }

        if (bytes[offset] < '0' || bytes[offset] > '9') {
            return Status::IS_NOT_A_NUMBER;
        }

        const int maxExponent = INT_MAX / 2;
        int num = 0;
        do {
            // Check overflow.
            int digit = bytes[offset] - '0';
            if (num >= maxExponent / 10
                && !(num == maxExponent / 10 && digit <= maxExponent % 10)) {
                num = maxExponent;
            } else {
                num = num * 10 + digit;
            }
            offset++;
        } while (offset != end && bytes[offset] >= '0' && bytes[offset] <= '9');

        exponent += (exponentSign == '-' ? -num : num);
    }

    if (offset != end) {
        return Status::IS_NOT_A_NUMBER;
    }

    parsing_done:
    exponent += insignificantDigits;

    if (nonzeroDigitDropped) {
        buffer[bufferPos++] = '1';
        exponent--;
    }

    buffer[bufferPos] = '\0';
    std::string_view chars(buffer, bufferPos);
    exponent += bufferPos - chars.length();
    double converted = StrtodTrimmed(chars, exponent);
    result = sign ? -converted : converted;
    return Status::CONVERT_SUCCESS;
}

inline Status ConvertStringToDouble(double &result, std::string s)
{
    return ConvertStringToDouble(result, s.c_str(), static_cast<int32_t>(s.length()));
}
}
}
#endif // OMNI_RUNTIME_DATA_OPERATIONS_H
