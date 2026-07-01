/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Precompiled Spark date-time format helpers.
 */

#include "SparkDateTimeFormat.h"

#include <ctime>
#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>

#include "type/Timestamp.h"
#include "type/date32.h"
#include "DateTimeZoneConversion.h"
#include "util/omni_exception.h"

namespace omniruntime::vectorization::datetime {
namespace {

std::string ConvertJodaToStrptime(std::string_view jodaFormat)
{
    std::string result;
    result.reserve(jodaFormat.size() * 2);
    size_t i = 0;
    while (i < jodaFormat.size()) {
        const char c = jodaFormat[i];
        if (c == '\'') {
            ++i;
            while (i < jodaFormat.size() && jodaFormat[i] != '\'') {
                result += jodaFormat[i++];
            }
            if (i < jodaFormat.size()) {
                ++i;
            }
        } else if (c == 'y' || c == 'Y') {
            while (i < jodaFormat.size() && (jodaFormat[i] == 'y' || jodaFormat[i] == 'Y')) {
                ++i;
            }
            result += "%Y";
        } else if (c == 'M') {
            while (i < jodaFormat.size() && jodaFormat[i] == 'M') {
                ++i;
            }
            result += "%m";
        } else if (c == 'd') {
            while (i < jodaFormat.size() && jodaFormat[i] == 'd') {
                ++i;
            }
            result += "%d";
        } else if (c == 'H') {
            while (i < jodaFormat.size() && jodaFormat[i] == 'H') {
                ++i;
            }
            result += "%H";
        } else if (c == 'h') {
            while (i < jodaFormat.size() && jodaFormat[i] == 'h') {
                ++i;
            }
            result += "%I";
        } else if (c == 'm') {
            while (i < jodaFormat.size() && jodaFormat[i] == 'm') {
                ++i;
            }
            result += "%M";
        } else if (c == 's') {
            while (i < jodaFormat.size() && jodaFormat[i] == 's') {
                ++i;
            }
            result += "%S";
        } else if (c == 'S') {
            while (i < jodaFormat.size() && jodaFormat[i] == 'S') {
                ++i;
            }
            if (!result.empty() && result.back() == '.') {
                result.pop_back();
            }
        } else if (c == 'a') {
            while (i < jodaFormat.size() && jodaFormat[i] == 'a') {
                ++i;
            }
            result += "%p";
        } else {
            result += c;
            ++i;
        }
    }
    return result;
}

bool ParseDigits(std::string_view input, size_t offset, size_t length, int32_t &result)
{
    result = 0;
    if (offset + length > input.size()) {
        return false;
    }
    for (size_t i = offset; i < offset + length; ++i) {
        const char c = input[i];
        if (c < '0' || c > '9') {
            return false;
        }
        result = result * 10 + (c - '0');
    }
    return true;
}

bool HasValidStandardSuffix(
    std::string_view input,
    size_t standardLength,
    bool allowTrailingWhitespace)
{
    if (input.size() == standardLength) {
        return true;
    }
    if (!allowTrailingWhitespace || input.size() < standardLength) {
        return false;
    }
    for (size_t i = standardLength; i < input.size(); ++i) {
        if (input[i] != ' ') {
            return false;
        }
    }
    return true;
}

bool TryParseStandardDateTime(
    std::string_view input,
    const CompiledParseFormat &format,
    int64_t &resultMicros)
{
    constexpr size_t kYmdLength = 10;
    constexpr size_t kYmdHmsLength = 19;
    const bool hasTime = format.parseFormatKind == ParseFormatKind::YMD_HMS;
    if (!hasTime && format.parseFormatKind != ParseFormatKind::YMD) {
        return false;
    }

    const size_t standardLength = hasTime ? kYmdHmsLength : kYmdLength;
    if (!HasValidStandardSuffix(input, standardLength, format.allowTrailingWhitespace) ||
        input[4] != '-' || input[7] != '-' ||
        (hasTime && (input[10] != ' ' || input[13] != ':' || input[16] != ':'))) {
        return false;
    }

    int32_t year;
    int32_t month;
    int32_t day;
    if (!ParseDigits(input, 0, 4, year) ||
        !ParseDigits(input, 5, 2, month) ||
        !ParseDigits(input, 8, 2, day) ||
        !type::Date32::IsValidDate(year, month, day)) {
        return false;
    }

    int32_t hour = 0;
    int32_t minute = 0;
    int32_t second = 0;
    if (hasTime &&
        (!ParseDigits(input, 11, 2, hour) ||
         !ParseDigits(input, 14, 2, minute) ||
         !ParseDigits(input, 17, 2, second) ||
         hour > 23 || minute > 59 || second > 59)) {
        return false;
    }

    std::tm timeInfo = {};
    timeInfo.tm_year = year - 1900;
    timeInfo.tm_mon = month - 1;
    timeInfo.tm_mday = day;
    timeInfo.tm_hour = hour;
    timeInfo.tm_min = minute;
    timeInfo.tm_sec = second;
    timeInfo.tm_isdst = -1;
    resultMicros = Timestamp::calendarUtcToEpoch(timeInfo) * Timestamp::kMicrosecondsInSecond;
    return true;
}

int32_t ParseMillisFromString(std::string_view input)
{
    const size_t dotInInput = input.rfind('.');
    if (dotInInput == std::string_view::npos) {
        return 0;
    }

    std::string fraction(input.substr(dotInInput + 1));
    while (fraction.size() < 3) {
        fraction += '0';
    }
    if (fraction.size() > 3) {
        fraction.resize(3);
    }

    int32_t millis = 0;
    for (const char c : fraction) {
        if (c < '0' || c > '9') {
            return 0;
        }
        millis = millis * 10 + (c - '0');
    }
    return millis;
}

constexpr std::array<const char *, 12> kShortMonthNames = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
constexpr std::array<const char *, 12> kFullMonthNames = {
    "January", "February", "March", "April", "May", "June", "July", "August",
    "September", "October", "November", "December"};
constexpr std::array<const char *, 7> kShortWeekdayNames = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
constexpr std::array<const char *, 7> kFullWeekdayNames = {
    "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

std::string PadNumber(int64_t value, size_t width)
{
    std::array<char, 32> digits{};
    auto [end, error] = std::to_chars(digits.data(), digits.data() + digits.size(), value);
    if (error != std::errc{}) {
        return {};
    }
    const size_t digitCount = static_cast<size_t>(end - digits.data());
    if (digitCount >= width) {
        return std::string(digits.data(), digitCount);
    }
    std::string result(width - digitCount, '0');
    result.append(digits.data(), digitCount);
    return result;
}

std::string FormatYear(int64_t year, size_t width)
{
    if (width == 2) {
        return PadNumber((year % 100 + 100) % 100, 2);
    }
    const size_t outputWidth = std::max<size_t>(4, width);
    if (year < 0) {
        return "-" + PadNumber(-year, outputWidth);
    }
    std::string result = PadNumber(year, outputWidth);
    return result.size() > outputWidth ? "+" + result : result;
}

std::string FormatRfcOffset(int offsetSeconds, bool withColon)
{
    const char sign = offsetSeconds >= 0 ? '+' : '-';
    const int absoluteOffset = std::abs(offsetSeconds);
    std::string result(1, sign);
    result += PadNumber(absoluteOffset / 3600, 2);
    if (withColon) {
        result += ':';
    }
    result += PadNumber((absoluteOffset % 3600) / 60, 2);
    return result;
}

std::string FormatIsoOffset(int offsetSeconds, size_t width, bool zeroAsZ)
{
    if (offsetSeconds == 0 && zeroAsZ) {
        return "Z";
    }
    if (width == 1 && offsetSeconds % 3600 == 0) {
        const char sign = offsetSeconds >= 0 ? '+' : '-';
        return std::string(1, sign) + PadNumber(std::abs(offsetSeconds) / 3600, 2);
    }
    return FormatRfcOffset(offsetSeconds, width >= 3);
}

std::string FormatLocalizedOffset(int offsetSeconds, size_t width)
{
    if (offsetSeconds == 0) {
        return "GMT";
    }
    const char sign = offsetSeconds >= 0 ? '+' : '-';
    const int absoluteOffset = std::abs(offsetSeconds);
    const int hours = absoluteOffset / 3600;
    const int minutes = (absoluteOffset % 3600) / 60;
    std::string result = "GMT" + std::string(1, sign);
    if (width >= 4) {
        return result + PadNumber(hours, 2) + ":" + PadNumber(minutes, 2);
    }
    result += std::to_string(hours);
    if (minutes != 0) {
        result += ":" + PadNumber(minutes, 2);
    }
    return result;
}

std::string FormatFraction(int32_t microsOfSecond, size_t width)
{
    std::string fraction = PadNumber(static_cast<int64_t>(microsOfSecond) * 1000, 9);
    if (width <= fraction.size()) {
        fraction.resize(width);
        return fraction;
    }
    fraction.append(width - fraction.size(), '0');
    return fraction;
}

std::string FormatPercentToken(char token, const std::tm &timeInfo, int offsetSeconds)
{
    const int64_t year = static_cast<int64_t>(timeInfo.tm_year) + 1900;
    switch (token) {
        case 'Y': return FormatYear(year, 4);
        case 'y': return FormatYear(year, 2);
        case 'm': return PadNumber(timeInfo.tm_mon + 1, 2);
        case 'd': return PadNumber(timeInfo.tm_mday, 2);
        case 'H': return PadNumber(timeInfo.tm_hour, 2);
        case 'I': {
            const int hour = timeInfo.tm_hour % 12;
            return PadNumber(hour == 0 ? 12 : hour, 2);
        }
        case 'M': return PadNumber(timeInfo.tm_min, 2);
        case 'S': return PadNumber(timeInfo.tm_sec, 2);
        case 'p': return timeInfo.tm_hour < 12 ? "AM" : "PM";
        case 'a': return kShortWeekdayNames[timeInfo.tm_wday];
        case 'A': return kFullWeekdayNames[timeInfo.tm_wday];
        case 'b': return kShortMonthNames[timeInfo.tm_mon];
        case 'B': return kFullMonthNames[timeInfo.tm_mon];
        case 'z': return FormatRfcOffset(offsetSeconds, false);
        case '%': return "%";
        default: return "%" + std::string(1, token);
    }
}

class BufferWriter {
public:
    BufferWriter(char *output, size_t capacity) : output_(output), capacity_(capacity) {}

    void Append(std::string_view value)
    {
        if (value.empty()) {
            return;
        }
        if (value.size() > capacity_ - std::min(position_, capacity_)) {
            overflow_ = true;
            return;
        }
        std::memcpy(output_ + position_, value.data(), value.size());
        position_ += value.size();
    }

    void AppendRepeated(char value, size_t count)
    {
        if (count == 0) {
            return;
        }
        if (count > capacity_ - std::min(position_, capacity_)) {
            overflow_ = true;
            return;
        }
        std::memset(output_ + position_, value, count);
        position_ += count;
    }

    void AppendNumber(int64_t value)
    {
        std::array<char, 32> buffer{};
        auto [end, error] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
        if (error != std::errc{}) {
            overflow_ = true;
            return;
        }
        Append(std::string_view(buffer.data(), static_cast<size_t>(end - buffer.data())));
    }

    void AppendPaddedNumber(int64_t value, size_t width)
    {
        std::array<char, 32> buffer{};
        auto [end, error] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
        if (error != std::errc{}) {
            overflow_ = true;
            return;
        }
        const size_t length = static_cast<size_t>(end - buffer.data());
        if (length < width) {
            AppendRepeated('0', width - length);
        }
        Append(std::string_view(buffer.data(), length));
    }

    void AppendFormattedNumber(int64_t value, size_t width)
    {
        if (width >= 2) {
            AppendPaddedNumber(value, width);
        } else {
            AppendNumber(value);
        }
    }

    int32_t Result() const
    {
        if (overflow_ || position_ > static_cast<size_t>(std::numeric_limits<int32_t>::max())) {
            return -1;
        }
        return static_cast<int32_t>(position_);
    }

private:
    char *output_;
    size_t capacity_;
    size_t position_{0};
    bool overflow_{false};
};

bool IsPatternToken(const CompiledFormatToken &token, char symbol, uint32_t width)
{
    return token.kind == CompiledTokenKind::PATTERN && token.symbol == symbol && token.width == width;
}

bool IsYmdHmsLayout(const std::vector<CompiledFormatToken> &tokens)
{
    return tokens.size() == 11 &&
        IsPatternToken(tokens[0], 'y', 4) &&
        IsPatternToken(tokens[1], '-', 1) &&
        IsPatternToken(tokens[2], 'M', 2) &&
        IsPatternToken(tokens[3], '-', 1) &&
        IsPatternToken(tokens[4], 'd', 2) &&
        IsPatternToken(tokens[5], ' ', 1) &&
        IsPatternToken(tokens[6], 'H', 2) &&
        IsPatternToken(tokens[7], ':', 1) &&
        IsPatternToken(tokens[8], 'm', 2) &&
        IsPatternToken(tokens[9], ':', 1) &&
        IsPatternToken(tokens[10], 's', 2);
}

void WriteTwoDigits(char *output, int value)
{
    output[0] = static_cast<char>('0' + value / 10);
    output[1] = static_cast<char>('0' + value % 10);
}

int32_t FormatYmdHms(const std::tm &timeInfo, char *output, size_t capacity)
{
    constexpr size_t kResultSize = 19;
    const int year = timeInfo.tm_year + 1900;
    if (capacity < kResultSize || year < 0 || year > 9999) {
        return -1;
    }
    std::memcpy(output, "0000-00-00 00:00:00", kResultSize);
    output[0] = static_cast<char>('0' + year / 1000);
    output[1] = static_cast<char>('0' + (year / 100) % 10);
    output[2] = static_cast<char>('0' + (year / 10) % 10);
    output[3] = static_cast<char>('0' + year % 10);
    WriteTwoDigits(output + 5, timeInfo.tm_mon + 1);
    WriteTwoDigits(output + 8, timeInfo.tm_mday);
    WriteTwoDigits(output + 11, timeInfo.tm_hour);
    WriteTwoDigits(output + 14, timeInfo.tm_min);
    WriteTwoDigits(output + 17, timeInfo.tm_sec);
    return static_cast<int32_t>(kResultSize);
}

[[noreturn]] void ThrowUnsupportedPattern(char token, FormatterKind kind)
{
    const std::string functionName = kind == FormatterKind::DATE_FORMAT ? "DateFormat" : "FromUnixTime";
    OMNI_THROW(functionName + " function Error", "Illegal pattern character '" + std::string(1, token) + "'");
}

} // namespace

CompiledParseFormat CompileParseFormat(std::string_view jodaFormat, bool allowTrailingWhitespace)
{
    ParseFormatKind parseFormatKind = ParseFormatKind::GENERAL;
    if (jodaFormat == "yyyy-MM-dd") {
        parseFormatKind = ParseFormatKind::YMD;
    } else if (jodaFormat == "yyyy-MM-dd HH:mm:ss") {
        parseFormatKind = ParseFormatKind::YMD_HMS;
    }
    return {
        ConvertJodaToStrptime(jodaFormat),
        parseFormatKind,
        jodaFormat.find('S') != std::string_view::npos,
        allowTrailingWhitespace,
        jodaFormat.empty()};
}

bool ParseDateTimeString(
    std::string_view input,
    const CompiledParseFormat &format,
    int64_t &resultMicros)
{
    if (input.empty() || format.emptySourceFormat) {
        return false;
    }

    if (TryParseStandardDateTime(input, format, resultMicros)) {
        return true;
    }

    std::string strptimeInput(input);
    if (format.hasFractional) {
        const size_t dotPos = input.rfind('.');
        if (dotPos != std::string::npos) {
            strptimeInput.resize(dotPos);
        }
    }

    std::tm timeInfo = {};
    timeInfo.tm_year = 70;
    timeInfo.tm_mday = 1;
    timeInfo.tm_isdst = -1;

    char *parseEnd = strptime(strptimeInput.c_str(), format.strptimeFormat.c_str(), &timeInfo);
    if (parseEnd == nullptr) {
        return false;
    }

    if (format.allowTrailingWhitespace) {
        while (*parseEnd == ' ') {
            ++parseEnd;
        }
    }
    if (*parseEnd != '\0') {
        return false;
    }

    const int64_t seconds = Timestamp::calendarUtcToEpoch(timeInfo);
    const int32_t millis = format.hasFractional ? ParseMillisFromString(input) : 0;
    resultMicros = seconds * Timestamp::kMicrosecondsInSecond +
        static_cast<int64_t>(millis) * Timestamp::kMicrosecondsInMillisecond;
    return true;
}

CompiledFormatPattern CompileFormatPattern(
    std::string_view format,
    FormatterKind formatterKind,
    bool legacyPolicy)
{
    CompiledFormatPattern result;
    result.formatterKind = formatterKind;
    result.legacyPolicy = legacyPolicy;

    size_t i = 0;
    while (i < format.size()) {
        const char symbol = format[i];
        if (symbol == '\'') {
            ++i;
            std::string literal;
            if (i < format.size() && format[i] == '\'') {
                literal += '\'';
                ++i;
            } else {
                while (i < format.size()) {
                    if (format[i] == '\'') {
                        ++i;
                        break;
                    }
                    literal += format[i++];
                }
            }
            result.tokens.push_back({CompiledTokenKind::LITERAL, 0, 0, std::move(literal)});
            continue;
        }

        if (formatterKind == FormatterKind::FROM_UNIXTIME && symbol == '%' && i + 1 < format.size()) {
            result.tokens.push_back({CompiledTokenKind::PERCENT, format[i + 1], 1, {}});
            i += 2;
            continue;
        }

        size_t width = 1;
        while (i + width < format.size() && format[i + width] == symbol) {
            ++width;
        }
        result.tokens.push_back({CompiledTokenKind::PATTERN, symbol, static_cast<uint32_t>(width), {}});
        result.requiresZoneName = result.requiresZoneName || symbol == 'v' || symbol == 'z';
        i += width;
    }
    size_t maxResultSize = 0;
    for (const auto &token : result.tokens) {
        size_t tokenSize = 0;
        if (token.kind == CompiledTokenKind::LITERAL) {
            tokenSize = token.literal.size();
        } else if (token.kind == CompiledTokenKind::PERCENT) {
            tokenSize = 32;
        } else if (std::isalpha(static_cast<unsigned char>(token.symbol))) {
            tokenSize = std::max<size_t>(token.width, 64);
        } else {
            tokenSize = token.width;
        }
        if (tokenSize > std::numeric_limits<size_t>::max() - maxResultSize) {
            maxResultSize = std::numeric_limits<size_t>::max();
            break;
        }
        maxResultSize += tokenSize;
    }
    result.maxResultSize = std::max<size_t>(maxResultSize, 1);
    if (IsYmdHmsLayout(result.tokens)) {
        result.fastFormatKind = FastFormatKind::YMD_HMS;
        result.fixedResultSize = 19;
        result.maxResultSize = 19;
    }
    return result;
}

int32_t FormatDateTimeToBuffer(
    const CalendarTime &calendarTime,
    const ResolvedTimeZone &timeZone,
    const CompiledFormatPattern &format,
    char *output,
    size_t capacity)
{
    const std::tm &timeInfo = calendarTime.calendar;
    if (format.fastFormatKind == FastFormatKind::YMD_HMS) {
        const int year = timeInfo.tm_year + 1900;
        if (year >= 0 && year <= 9999) {
            return FormatYmdHms(timeInfo, output, capacity);
        }
    }

    const int hour = timeInfo.tm_hour;
    const int month = timeInfo.tm_mon + 1;
    const int dayOfMonth = timeInfo.tm_mday;
    const int64_t year = static_cast<int64_t>(timeInfo.tm_year) + 1900;
    BufferWriter writer(output, capacity);

    for (const auto &compiledToken : format.tokens) {
        if (compiledToken.kind == CompiledTokenKind::LITERAL) {
            writer.Append(compiledToken.literal);
            continue;
        }
        if (compiledToken.kind == CompiledTokenKind::PERCENT) {
            writer.Append(FormatPercentToken(compiledToken.symbol, timeInfo, calendarTime.offsetSeconds));
            continue;
        }

        const char token = compiledToken.symbol;
        const size_t width = compiledToken.width;
        switch (token) {
            case 'y':
            case 'Y':
                if (format.formatterKind == FormatterKind::DATE_FORMAT) {
                    writer.AppendPaddedNumber(
                        width == 2 ? (timeInfo.tm_year + 1900) % 100 : timeInfo.tm_year + 1900,
                        width == 2 ? 2 : std::max<size_t>(4, width));
                } else {
                    writer.Append(FormatYear(year, width));
                }
                break;
            case 'M':
                if (width == 3) {
                    writer.Append(kShortMonthNames[timeInfo.tm_mon]);
                } else if (width >= 4) {
                    writer.Append(kFullMonthNames[timeInfo.tm_mon]);
                } else {
                    writer.AppendFormattedNumber(month, width);
                }
                break;
            case 'd': writer.AppendFormattedNumber(dayOfMonth, width); break;
            case 'H': writer.AppendFormattedNumber(hour, width); break;
            case 'h': {
                const int clockHour = hour % 12;
                writer.AppendFormattedNumber(clockHour == 0 ? 12 : clockHour, width);
                break;
            }
            case 'K': writer.AppendFormattedNumber(hour % 12, width); break;
            case 'k': writer.AppendFormattedNumber(hour == 0 ? 24 : hour, width); break;
            case 'm': writer.AppendFormattedNumber(timeInfo.tm_min, width); break;
            case 's': writer.AppendFormattedNumber(timeInfo.tm_sec, width); break;
            case 'S': writer.Append(FormatFraction(calendarTime.microsOfSecond, width)); break;
            case 'n':
                if (format.formatterKind == FormatterKind::DATE_FORMAT) {
                    const int64_t nanos = static_cast<int64_t>(calendarTime.microsOfSecond) * 1000;
                    if (width <= 1) {
                        writer.AppendNumber(nanos);
                    } else {
                        writer.AppendPaddedNumber(nanos, width);
                    }
                } else {
                    ThrowUnsupportedPattern(token, format.formatterKind);
                }
                break;
            case 'a': writer.Append(hour < 12 ? "AM" : "PM"); break;
            case 'D': writer.AppendFormattedNumber(timeInfo.tm_yday + 1, width); break;
            case 'E':
                writer.Append(
                    width >= 4 ? kFullWeekdayNames[timeInfo.tm_wday] : kShortWeekdayNames[timeInfo.tm_wday]);
                break;
            case 'F':
                writer.AppendNumber(
                    format.formatterKind == FormatterKind::FROM_UNIXTIME && !format.legacyPolicy
                        ? ((dayOfMonth - 1) % 7) + 1
                        : ((dayOfMonth - 1) / 7) + 1);
                break;
            case 'q':
            case 'Q':
                if (format.formatterKind == FormatterKind::FROM_UNIXTIME && format.legacyPolicy) {
                    ThrowUnsupportedPattern(token, format.formatterKind);
                }
                writer.AppendFormattedNumber(((month - 1) / 3) + 1, width);
                break;
            case 'w': {
                char buffer[8] = {};
                const size_t length = strftime(buffer, sizeof(buffer), "%V", &timeInfo);
                std::string week = length == 0 ? "" : std::string(buffer, length);
                if (width == 1 && week.size() == 2 && week[0] == '0') {
                    week.erase(0, 1);
                }
                writer.Append(week);
                break;
            }
            case 'V':
                if (format.formatterKind == FormatterKind::FROM_UNIXTIME && format.legacyPolicy) {
                    ThrowUnsupportedPattern(token, format.formatterKind);
                }
                writer.Append(timeZone.displayId);
                break;
            case 'v':
                if (format.formatterKind == FormatterKind::FROM_UNIXTIME && format.legacyPolicy) {
                    ThrowUnsupportedPattern(token, format.formatterKind);
                }
                writer.Append(calendarTime.zoneAbbreviation);
                break;
            case 'z': writer.Append(calendarTime.zoneAbbreviation); break;
            case 'O':
                if (format.formatterKind == FormatterKind::FROM_UNIXTIME && format.legacyPolicy) {
                    ThrowUnsupportedPattern(token, format.formatterKind);
                }
                writer.Append(FormatLocalizedOffset(calendarTime.offsetSeconds, width));
                break;
            case 'X': writer.Append(FormatIsoOffset(calendarTime.offsetSeconds, width, true)); break;
            case 'x':
                if (format.formatterKind == FormatterKind::FROM_UNIXTIME && format.legacyPolicy) {
                    ThrowUnsupportedPattern(token, format.formatterKind);
                }
                writer.Append(FormatIsoOffset(calendarTime.offsetSeconds, width, false));
                break;
            case 'Z':
                writer.Append(
                    width >= 4 ? FormatLocalizedOffset(calendarTime.offsetSeconds, width)
                               : FormatRfcOffset(calendarTime.offsetSeconds, false));
                break;
            default:
                if (std::isalpha(static_cast<unsigned char>(token))) {
                    ThrowUnsupportedPattern(token, format.formatterKind);
                }
                writer.AppendRepeated(token, width);
                break;
        }
    }
    return writer.Result();
}

std::string FormatDateTime(
    const CalendarTime &calendarTime,
    const ResolvedTimeZone &timeZone,
    const CompiledFormatPattern &format)
{
    size_t capacity = std::max<size_t>(format.maxResultSize, 64);
    bool retry = true;
    while (retry) {
        std::string result(capacity, '\0');
        const int32_t length =
            FormatDateTimeToBuffer(calendarTime, timeZone, format, result.data(), result.size());
        if (length >= 0) {
            result.resize(static_cast<size_t>(length));
            return result;
        }
        if (capacity > std::numeric_limits<size_t>::max() / 2) {
            retry = false;
        } else {
            capacity *= 2;
        }
    }
    OMNI_FAIL("Date-time formatted result is too large");
}

} // namespace omniruntime::vectorization::datetime
