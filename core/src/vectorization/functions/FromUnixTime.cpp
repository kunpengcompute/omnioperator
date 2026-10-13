/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: FromUnixTime function implementation for vectorized execution
 */

#include "FromUnixTime.h"
#include "vector/vector.h"
#include "../VectorFunction.h"
#include "vectorization/SelectivityVector.h"
#include "vector/vector_helper.h"
#include "util/bit_util.h"
#include "util/config/QueryConfig.h"
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <ctime>
#include <cstring>
#include <string>
#include <string_view>
#include <cmath>
#include <array>
#include <iomanip>
#include <limits>
#include <sstream>

namespace omniruntime::vectorization {
using namespace omniruntime::vec;
using namespace omniruntime::type;

namespace {

static constexpr std::array<const char *, 12> SHORT_MONTH_NAMES = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
static constexpr std::array<const char *, 12> FULL_MONTH_NAMES = {
    "January", "February", "March", "April", "May", "June", "July", "August",
    "September", "October", "November", "December"};
static constexpr std::array<const char *, 7> SHORT_WEEKDAY_NAMES = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
static constexpr std::array<const char *, 7> FULL_WEEKDAY_NAMES = {
    "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
static constexpr int64_t MICROS_PER_SECOND = 1000000LL;

std::string PadNumber(int64_t value, size_t width)
{
    std::ostringstream oss;
    oss << std::setw(static_cast<int>(width)) << std::setfill('0') << value;
    return oss.str();
}

std::string FormatNumber(int value, size_t count)
{
    return count >= 2 ? PadNumber(value, count) : std::to_string(value);
}

std::string FormatYear(int64_t year, size_t count)
{
    if (count == 2) {
        return PadNumber((year % 100 + 100) % 100, 2);
    }

    size_t width = std::max<size_t>(4, count);
    if (year < 0) {
        return "-" + PadNumber(-year, width);
    }

    std::string yearString = PadNumber(year, width);
    if (yearString.size() > width) {
        return "+" + yearString;
    }
    return yearString;
}

int64_t WrapToSparkLong(__int128_t value)
{
    constexpr __int128_t UINT64_MODULUS =
        static_cast<__int128_t>(std::numeric_limits<uint64_t>::max()) + 1;
    constexpr __int128_t INT64_SIGN_BIT = static_cast<__int128_t>(1) << 63;

    __int128_t wrapped = value % UINT64_MODULUS;
    if (wrapped < 0) {
        wrapped += UINT64_MODULUS;
    }
    if (wrapped >= INT64_SIGN_BIT) {
        wrapped -= UINT64_MODULUS;
    }
    return static_cast<int64_t>(wrapped);
}

int64_t FloorDiv(int64_t dividend, int64_t divisor)
{
    int64_t quotient = dividend / divisor;
    int64_t remainder = dividend % divisor;
    if (remainder != 0 && ((remainder < 0) != (divisor < 0))) {
        --quotient;
    }
    return quotient;
}

struct SparkTimestampParts {
    int64_t seconds;
    int32_t microsOfSecond;
};

SparkTimestampParts ConvertUnixSecondsToSparkTimestamp(int64_t unixSeconds)
{
    int64_t micros = WrapToSparkLong(static_cast<__int128_t>(unixSeconds) * MICROS_PER_SECOND);
    int64_t seconds = FloorDiv(micros, MICROS_PER_SECOND);
    return {seconds, static_cast<int32_t>(micros - seconds * MICROS_PER_SECOND)};
}

std::string FormatFraction(int32_t microsOfSecond, size_t count)
{
    std::string fraction = PadNumber(static_cast<int64_t>(microsOfSecond) * 1000, 9);
    if (count <= fraction.size()) {
        return fraction.substr(0, count);
    }
    fraction.append(count - fraction.size(), '0');
    return fraction;
}

std::string ToUpperAscii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    return value;
}

bool IsLegacyPolicy(const std::string &policy)
{
    return ToUpperAscii(policy) == "LEGACY";
}

void ThrowUnsupportedPattern(char token, const std::string &policy)
{
    (void)policy;
    OMNI_THROW("FromUnixTime function Error", "Illegal pattern character '" + std::string(1, token) + "'");
}

int GetLegacyDayOfWeekInMonth(const struct tm &timeInfo)
{
    return ((timeInfo.tm_mday - 1) / 7) + 1;
}

int GetGmtOffsetSeconds(const struct tm &timeInfo)
{
#if defined(__linux__) || defined(__APPLE__)
    return static_cast<int>(timeInfo.tm_gmtoff);
#else
    return 0;
#endif
}

std::string FormatRfcOffset(int offsetSeconds, bool withColon)
{
    char sign = offsetSeconds >= 0 ? '+' : '-';
    int absOffset = std::abs(offsetSeconds);
    int hours = absOffset / 3600;
    int minutes = (absOffset % 3600) / 60;
    std::ostringstream oss;
    oss << sign << PadNumber(hours, 2);
    if (withColon) {
        oss << ':';
    }
    oss << PadNumber(minutes, 2);
    return oss.str();
}

std::string FormatIsoOffset(int offsetSeconds, size_t count, bool zeroAsZ)
{
    if (offsetSeconds == 0 && zeroAsZ) {
        return "Z";
    }
    if (count == 1 && offsetSeconds % 3600 == 0) {
        char sign = offsetSeconds >= 0 ? '+' : '-';
        return std::string(1, sign) + PadNumber(std::abs(offsetSeconds) / 3600, 2);
    }
    return FormatRfcOffset(offsetSeconds, count >= 3);
}

std::string FormatLocalizedOffset(int offsetSeconds, size_t count)
{
    if (offsetSeconds == 0) {
        return "GMT";
    }
    char sign = offsetSeconds >= 0 ? '+' : '-';
    int absOffset = std::abs(offsetSeconds);
    int hours = absOffset / 3600;
    int minutes = (absOffset % 3600) / 60;
    std::ostringstream oss;
    oss << "GMT" << sign;
    if (count >= 4) {
        oss << PadNumber(hours, 2) << ':' << PadNumber(minutes, 2);
    } else {
        oss << hours;
        if (minutes != 0) {
            oss << ':' << PadNumber(minutes, 2);
        }
    }
    return oss.str();
}

std::string FormatPercentToken(char token, const struct tm &timeInfo, int offsetSeconds)
{
    const int64_t year = static_cast<int64_t>(timeInfo.tm_year) + 1900;
    switch (token) {
        case 'Y':
            return FormatYear(year, 4);
        case 'y':
            return FormatYear(year, 2);
        case 'm':
            return PadNumber(timeInfo.tm_mon + 1, 2);
        case 'd':
            return PadNumber(timeInfo.tm_mday, 2);
        case 'H':
            return PadNumber(timeInfo.tm_hour, 2);
        case 'I': {
            int clockHour = timeInfo.tm_hour % 12;
            return PadNumber(clockHour == 0 ? 12 : clockHour, 2);
        }
        case 'M':
            return PadNumber(timeInfo.tm_min, 2);
        case 'S':
            return PadNumber(timeInfo.tm_sec, 2);
        case 'p':
            return timeInfo.tm_hour < 12 ? "AM" : "PM";
        case 'a':
            return SHORT_WEEKDAY_NAMES[timeInfo.tm_wday];
        case 'A':
            return FULL_WEEKDAY_NAMES[timeInfo.tm_wday];
        case 'b':
            return SHORT_MONTH_NAMES[timeInfo.tm_mon];
        case 'B':
            return FULL_MONTH_NAMES[timeInfo.tm_mon];
        case 'z':
            return FormatRfcOffset(offsetSeconds, false);
        case '%':
            return "%";
        default:
            return "%" + std::string(1, token);
    }
}

std::string GetDisplayTimeZoneId(const std::string &timeZoneStr)
{
    if (timeZoneStr == "Asia/Beijing" || timeZoneStr == "Asia/Shanghai") {
        return "Asia/Shanghai";
    }
    if (timeZoneStr == "GMT+08:00") {
        return "GMT+08:00";
    }
    return timeZoneStr.empty() ? "UTC" : timeZoneStr;
}

std::string FormatZoneName(const struct tm &timeInfo, const std::string &timeZoneStr, int offsetSeconds)
{
    if (timeZoneStr.rfind("GMT", 0) == 0) {
        return timeZoneStr;
    }
#if defined(__linux__) || defined(__APPLE__)
    return timeInfo.tm_zone != nullptr ? timeInfo.tm_zone : "";
#else
    return FormatRfcOffset(offsetSeconds, false);
#endif
}

/// Convert "GMT+08:00" to POSIX-compatible "Etc/GMT-8".
/// POSIX/IANA use inverted sign convention: GMT+8 means UTC-8 in POSIX.
static std::string NormalizeTimeZone(const std::string &tzStr)
{
    if (tzStr == "GMT+08:00") {
        return "Etc/GMT-8";
    }
    if (tzStr == "Asia/Beijing") {
        return "Asia/Shanghai";
    }
    return tzStr;
}

/// Format from_unixtime input using Spark/Java datetime pattern tokens.
/// Spark first converts seconds to microseconds with signed long overflow semantics.
/// If timeZoneStr is non-empty, sets TZ env and uses localtime_r (local time).
/// If timeZoneStr is empty, uses gmtime_r (UTC time).
/// This matches the codegen layer's approach for consistency with Spark.
std::string FormatTimestamp(int64_t secondsSinceEpoch, const std::string &sparkFormat,
    const std::string &timeZoneStr, const std::string &policy)
{
    SparkTimestampParts timestampParts = ConvertUnixSecondsToSparkTimestamp(secondsSinceEpoch);
    time_t timeStampVal = static_cast<time_t>(timestampParts.seconds);
    struct tm ltm = {};

    if (!timeZoneStr.empty()) {
        // Set timezone environment variable and use localtime_r
        std::string normalizedTz = NormalizeTimeZone(timeZoneStr);
        setenv("TZ", normalizedTz.c_str(), 1);
        tzset();
        localtime_r(&timeStampVal, &ltm);
    } else {
        // No timezone specified: use UTC
        gmtime_r(&timeStampVal, &ltm);
    }

    std::string result;
    result.reserve(sparkFormat.size() * 2);
    const int offsetSeconds = GetGmtOffsetSeconds(ltm);
    const std::string zoneId = GetDisplayTimeZoneId(timeZoneStr);
    const int hour = ltm.tm_hour;
    const int month = ltm.tm_mon + 1;
    const int dayOfMonth = ltm.tm_mday;
    const int64_t year = static_cast<int64_t>(ltm.tm_year) + 1900;
    const bool isLegacy = IsLegacyPolicy(policy);

    size_t i = 0;
    while (i < sparkFormat.size()) {
        char token = sparkFormat[i];
        if (token == '\'') {
            ++i;
            if (i < sparkFormat.size() && sparkFormat[i] == '\'') {
                result += '\'';
                ++i;
                continue;
            }
            while (i < sparkFormat.size()) {
                if (sparkFormat[i] == '\'') {
                    ++i;
                    break;
                }
                result += sparkFormat[i++];
            }
            continue;
        }

        if (token == '%' && i + 1 < sparkFormat.size()) {
            result += FormatPercentToken(sparkFormat[i + 1], ltm, offsetSeconds);
            i += 2;
            continue;
        }

        size_t count = 1;
        while (i + count < sparkFormat.size() && sparkFormat[i + count] == token) {
            ++count;
        }

        switch (token) {
            case 'y':
            case 'Y':
                result += FormatYear(year, count);
                break;
            case 'M':
                if (count == 3) {
                    result += SHORT_MONTH_NAMES[ltm.tm_mon];
                } else if (count >= 4) {
                    result += FULL_MONTH_NAMES[ltm.tm_mon];
                } else {
                    result += FormatNumber(month, count);
                }
                break;
            case 'd':
                result += FormatNumber(dayOfMonth, count);
                break;
            case 'H':
                result += FormatNumber(hour, count);
                break;
            case 'h': {
                int clockHour = hour % 12;
                result += FormatNumber(clockHour == 0 ? 12 : clockHour, count);
                break;
            }
            case 'K':
                result += FormatNumber(hour % 12, count);
                break;
            case 'k':
                result += FormatNumber(hour == 0 ? 24 : hour, count);
                break;
            case 'm':
                result += FormatNumber(ltm.tm_min, count);
                break;
            case 's':
                result += FormatNumber(ltm.tm_sec, count);
                break;
            case 'S':
                result += FormatFraction(timestampParts.microsOfSecond, count);
                break;
            case 'a':
                result += hour < 12 ? "AM" : "PM";
                break;
            case 'D':
                result += FormatNumber(ltm.tm_yday + 1, count);
                break;
            case 'E':
                result += count >= 4 ? FULL_WEEKDAY_NAMES[ltm.tm_wday] : SHORT_WEEKDAY_NAMES[ltm.tm_wday];
                break;
            case 'F':
                result += std::to_string(isLegacy ? GetLegacyDayOfWeekInMonth(ltm) : ((dayOfMonth - 1) % 7) + 1);
                break;
            case 'q':
            case 'Q':
                if (isLegacy) {
                    ThrowUnsupportedPattern(token, policy);
                }
                result += FormatNumber(((month - 1) / 3) + 1, count);
                break;
            case 'w': {
                char buffer[8] = {};
                size_t len = strftime(buffer, sizeof(buffer), "%V", &ltm);
                std::string week = len == 0 ? "" : std::string(buffer, len);
                if (count == 1 && week.size() == 2 && week[0] == '0') {
                    week.erase(0, 1);
                }
                result += week;
                break;
            }
            case 'V':
                if (isLegacy) {
                    ThrowUnsupportedPattern(token, policy);
                }
                result += zoneId;
                break;
            case 'v':
                if (isLegacy) {
                    ThrowUnsupportedPattern(token, policy);
                }
                result += FormatZoneName(ltm, timeZoneStr, offsetSeconds);
                break;
            case 'z':
                result += FormatZoneName(ltm, timeZoneStr, offsetSeconds);
                break;
            case 'O':
                if (isLegacy) {
                    ThrowUnsupportedPattern(token, policy);
                }
                result += FormatLocalizedOffset(offsetSeconds, count);
                break;
            case 'X':
                result += FormatIsoOffset(offsetSeconds, count, true);
                break;
            case 'x':
                if (isLegacy) {
                    ThrowUnsupportedPattern(token, policy);
                }
                result += FormatIsoOffset(offsetSeconds, count, false);
                break;
            case 'Z':
                result += count >= 4 ? FormatLocalizedOffset(offsetSeconds, count) : FormatRfcOffset(offsetSeconds, false);
                break;
            default:
                if (std::isalpha(static_cast<unsigned char>(token))) {
                    ThrowUnsupportedPattern(token, policy);
                }
                result.append(count, token);
                break;
        }
        i += count;
    }
    return result;
}

std::string_view GetStringValueFromVector(BaseVector *vec, int32_t row)
{
    Encoding encoding = vec->GetEncoding();

    if (encoding == OMNI_ENCODING_CONST) {
        auto *constVec = static_cast<ConstVector<std::string_view> *>(vec);
        return constVec->GetConstValue();
    } else if (encoding == OMNI_FLAT) {
        auto *flatVec = static_cast<Vector<LargeStringContainer<std::string_view>> *>(vec);
        return flatVec->GetValue(row);
    } else if (encoding == OMNI_DICTIONARY) {
        auto *dictVec = static_cast<Vector<DictionaryContainer<std::string_view, LargeStringContainer>> *>(vec);
        return dictVec->GetValue(row);
    } else {
        return std::string_view();
    }
}

/// from_unixtime(unixtime, format) -> string
/// from_unixtime(unixtime, format, timezone) -> string
/// from_unixtime(unixtime, format, timezone, policy) -> string
/// Converts a unix timestamp (seconds since epoch) to a formatted string.
/// unixtime: int64_t (seconds since epoch)
/// format: Joda date/time format string (e.g., "yyyy-MM-dd HH:mm:ss")
/// timezone: optional timezone string (e.g., "GMT+08:00", "Asia/Shanghai")
/// Returns NULL if input is NULL or format is invalid.
class FromUnixTimeFunction : public VectorFunction {
public:
    void Apply(std::stack<BaseVector *> &args, const DataTypePtr &outputType, BaseVector *&result,
        op::ExecutionContext *context) const override
    {
        size_t argCount = args.size();
        if (argCount < 2) {
            OMNI_THROW("FromUnixTime function Error", "Expected at least 2 arguments (unixtime, format)");
        }

        // Parse arguments: stack top is last arg
        // 4-arg form: from_unixtime(unixtime, format, timezone, policy) - Gluten path
        // 3-arg form: from_unixtime(unixtime, format, timezone) - compatibility path
        // 2-arg form: from_unixtime(unixtime, format) - direct path
        BaseVector *policyArg = nullptr;
        BaseVector *tzArg = nullptr;
        BaseVector *formatArg = nullptr;
        BaseVector *inputArg = nullptr;

        if (argCount == 4) {
            policyArg = args.top();
            args.pop();
            tzArg = args.top();
            args.pop();
            formatArg = args.top();
            args.pop();
            inputArg = args.top();
            args.pop();
        } else if (argCount == 3) {
            tzArg = args.top();
            args.pop();
            formatArg = args.top();
            args.pop();
            inputArg = args.top();
            args.pop();
        } else if (argCount == 2) {
            formatArg = args.top();
            args.pop();
            inputArg = args.top();
            args.pop();
        } else {
            OMNI_THROW("FromUnixTime function Error", "Expected 2, 3, or 4 arguments");
        }

        const auto size = inputArg->GetSize();

        if (result == nullptr) {
            result = VectorHelper::CreateFlatVector(context->GetPool(), OMNI_VARCHAR, size);
        }

        bool formatIsConst = (formatArg->GetEncoding() == OMNI_ENCODING_CONST);
        std::string constSparkFormat;
        bool constFormatValid = true;

        if (formatIsConst) {
            if (formatArg->IsNull(0)) {
                constFormatValid = false;
            } else {
                auto *constFormatVec = static_cast<ConstVector<std::string_view> *>(formatArg);
                std::string_view formatView = constFormatVec->GetConstValue();
                constSparkFormat = std::string(formatView);
            }
        }

        bool policyIsConst = (policyArg == nullptr || policyArg->GetEncoding() == OMNI_ENCODING_CONST);
        std::string constPolicy = "CORRECTED";
        if (policyArg != nullptr && policyIsConst && !policyArg->IsNull(0)) {
            constPolicy = ToUpperAscii(std::string(GetStringValueFromVector(policyArg, 0)));
        }

        // Resolve timezone string: prefer explicit tzArg, fallback to session config
        std::string timeZoneStr;
        if (tzArg != nullptr) {
            bool tzIsConst = (tzArg->GetEncoding() == OMNI_ENCODING_CONST);
            if (tzIsConst && !tzArg->IsNull(0)) {
                timeZoneStr = std::string(GetStringValueFromVector(tzArg, 0));
            }
        }
        if (timeZoneStr.empty()) {
            // Fallback to session timezone from config
            if (context->queryConfig().AdjustTimestampToTimezone()) {
                timeZoneStr = context->queryConfig().SessionTimezone();
            }
        }

        auto *resultVec = static_cast<Vector<LargeStringContainer<std::string_view>> *>(result);

        for (int32_t row = 0; row < size; ++row) {
            if (inputArg->IsNull(row)) {
                result->SetNull(row);
                continue;
            }

            if (!formatIsConst && formatArg->IsNull(row)) {
                result->SetNull(row);
                continue;
            }

            // Get unix seconds value
            int64_t unixSeconds = 0;
            const auto inputTypeId = inputArg->GetTypeId();
            if (inputTypeId == OMNI_LONG || inputTypeId == OMNI_TIMESTAMP) {
                auto *inputVec = static_cast<Vector<int64_t> *>(inputArg);
                unixSeconds = inputVec->GetValue(row);
            } else {
                result->SetNull(row);
                continue;
            }

            // Get format string
            std::string sparkFormat;
            if (formatIsConst) {
                if (!constFormatValid) {
                    result->SetNull(row);
                    continue;
                }
                sparkFormat = constSparkFormat;
            } else {
                std::string_view formatView = GetStringValueFromVector(formatArg, row);
                sparkFormat = std::string(formatView);
            }

            std::string rowPolicy = constPolicy;
            if (policyArg != nullptr && !policyIsConst) {
                if (!policyArg->IsNull(row)) {
                    rowPolicy = ToUpperAscii(std::string(GetStringValueFromVector(policyArg, row)));
                } else {
                    rowPolicy = "CORRECTED";
                }
            }

            // Resolve per-row timezone if tzArg is non-const
            std::string rowTzStr = timeZoneStr;
            if (tzArg != nullptr && tzArg->GetEncoding() != OMNI_ENCODING_CONST) {
                if (!tzArg->IsNull(row)) {
                    rowTzStr = std::string(GetStringValueFromVector(tzArg, row));
                } else {
                    rowTzStr = "";
                }
                if (rowTzStr.empty()) {
                    if (context->queryConfig().AdjustTimestampToTimezone()) {
                        rowTzStr = context->queryConfig().SessionTimezone();
                    }
                }
            }

            // Format the timestamp
            std::string formatted = FormatTimestamp(unixSeconds, sparkFormat, rowTzStr, rowPolicy);
            if (formatted.empty()) {
                result->SetNull(row);
                continue;
            }

            resultVec->SetValue(row, std::string_view(formatted));
            result->SetNotNull(row);
        }

        if (inputArg != nullptr) {
            delete inputArg;
        }
        if (formatArg != nullptr) {
            delete formatArg;
        }
        if (tzArg != nullptr) {
            delete tzArg;
        }
        if (policyArg != nullptr) {
            delete policyArg;
        }
    }
};

} // namespace

void RegisterFromUnixTimeFunction(const std::string &name)
{
    auto fromUnixTimeFunction = std::make_shared<FromUnixTimeFunction>();
    // 2-arg signatures: from_unixtime(bigint, varchar) -> varchar
    VectorFunction::RegisterVectorFunction(name, {OMNI_LONG, OMNI_VARCHAR}, OMNI_VARCHAR,
        fromUnixTimeFunction);
    VectorFunction::RegisterVectorFunction(name, {OMNI_TIMESTAMP, OMNI_VARCHAR}, OMNI_VARCHAR,
        fromUnixTimeFunction);
    // 3-arg signatures: from_unixtime(bigint, varchar, varchar) -> varchar
    // The 3rd argument is timezone string, matching Gluten's calling convention
    VectorFunction::RegisterVectorFunction(name, {OMNI_LONG, OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_VARCHAR,
        fromUnixTimeFunction);
    VectorFunction::RegisterVectorFunction(name, {OMNI_TIMESTAMP, OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_VARCHAR,
        fromUnixTimeFunction);
    // 4-arg signatures: from_unixtime(bigint, varchar, varchar, varchar) -> varchar
    // The 4th argument is spark.sql.legacy.timeParserPolicy.
    VectorFunction::RegisterVectorFunction(name, {OMNI_LONG, OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_VARCHAR,
        fromUnixTimeFunction);
    VectorFunction::RegisterVectorFunction(name, {OMNI_TIMESTAMP, OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR},
        OMNI_VARCHAR, fromUnixTimeFunction);
}

}
