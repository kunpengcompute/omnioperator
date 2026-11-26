/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: base operations implementation
 */

#include "CastHooks.h"

#include "type/data_type.h"
#include "type/TimestampConversion.h"
#include "type/tz/TimeZoneMap.h"
#include "type/Conversions.h"
#include "type/string_Impl.h"

namespace omniruntime::vectorization {
CastHooks::CastHooks(const config::QueryConfig &config)
    : config_(config)
{
    const auto sessionTzName = config.SessionTimezone();
    if (!sessionTzName.empty()) {
        timestampToStringOptions_.timeZone = tz::locateZone(sessionTzName);
    }
}

namespace {
bool isTimeOnlyTimestamp(const std::string_view &view)
{
    if (view.empty() || view.find(':') == std::string_view::npos || view.find('-') != std::string_view::npos) {
        return false;
    }
    const char first = view.front();
    return first != '+' && first != '-';
}
}

Expected<int64_t> CastHooks::castStringToTimestamp(const std::string_view &view) const
{
    const tz::TimeZone *sessionTimezone = timestampToStringOptions_.timeZone;
    const char *text = view.data();
    size_t textLen = view.size();
    if (isTimeOnlyTimestamp(view)) {
        return folly::makeUnexpected(Status::UserError(
            "Time-only timestamp strings are not converted with the system date"));
    }
    auto conversionResult = util::fromTimestampWithTimezoneString(text, textLen);
    if (conversionResult.hasError()) {
        return folly::makeUnexpected(conversionResult.error());
    }

    return util::fromParsedTimestampWithTimeZone(conversionResult.value(), sessionTimezone).toMicros();
}

template <typename T>
Expected<int64_t> CastHooks::castNumberToTimestamp(T seconds) const
{
    // Spark internally use microsecond precision for timestamp.
    // To avoid overflow, we need to check the range of seconds.
    static constexpr int64_t maxSeconds = std::numeric_limits<int64_t>::max() / Timestamp::kMicrosecondsInSecond;
    if (seconds > maxSeconds) {
        return Timestamp::fromMicrosNoError(std::numeric_limits<int64_t>::max()).toMicros();
    }
    if (seconds < -maxSeconds) {
        return Timestamp::fromMicrosNoError(std::numeric_limits<int64_t>::min()).toMicros();
    }

    if constexpr (std::is_floating_point_v<T>) {
        return Timestamp::fromMicrosNoError(static_cast<int64_t>(seconds * Timestamp::kMicrosecondsInSecond)).
            toMicros();
    }

    return Timestamp(seconds, 0).toMicros();
}

Expected<int64_t> CastHooks::castIntToTimestamp(int64_t seconds) const
{
    return castNumberToTimestamp(seconds);
}

Expected<std::optional<int64_t>> CastHooks::castDoubleToTimestamp(double value) const
{
    if (FOLLY_UNLIKELY(std::isnan(value) || std::isinf(value))) {
        return std::nullopt;
    }
    return castNumberToTimestamp(value);
}

Expected<int32_t> CastHooks::castStringToDate(const std::string_view &dateString) const
{
    // Allows all patterns supported by Spark:
    // `[+-]yyyy*`
    // `[+-]yyyy*-[m]m`
    // `[+-]yyyy*-[m]m-[d]d`
    // `[+-]yyyy*-[m]m-[d]d *`
    // `[+-]yyyy*-[m]m-[d]dT*`
    // The asterisk `*` in `yyyy*` stands for any numbers.
    // For the last two patterns, the trailing `*` can represent none or any
    // sequence of characters, e.g:
    //   "1970-01-01 123"
    //   "1970-01-01 (BC)"
    return util::fromDateString(removeWhiteSpaces(dateString));
}

Expected<float> CastHooks::castStringToReal(const std::string_view &data) const
{
    return util::Converter<OMNI_FLOAT>::tryCast(data);
}

Expected<double> CastHooks::castStringToDouble(const std::string_view &data)
{
    return util::Converter<OMNI_DOUBLE>::tryCast(data);
}

std::string_view CastHooks::removeWhiteSpaces(const std::string_view &view) const
{
    std::string_view output;
    stringImpl::TrimUnicodeWhiteSpace<true, true, std::string_view, std::string_view>(output, view);
    return output;
}
} // namespace facebook::velox::functions::sparksql
