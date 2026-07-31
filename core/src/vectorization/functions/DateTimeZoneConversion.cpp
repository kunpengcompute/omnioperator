/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Explicit tzdb-based timestamp-to-calendar conversion.
 */

#include "DateTimeZoneConversion.h"

#include <limits>
#include <utility>

#include "type/Timestamp.h"
#include "util/omni_exception.h"

namespace omniruntime::vectorization::datetime {

namespace {

bool TrySubtract(
    std::chrono::seconds value,
    std::chrono::seconds offset,
    std::chrono::seconds &result)
{
    const int64_t valueCount = value.count();
    const int64_t offsetCount = offset.count();
    if ((offsetCount > 0 &&
            valueCount < std::numeric_limits<int64_t>::min() + offsetCount) ||
        (offsetCount < 0 &&
            valueCount > std::numeric_limits<int64_t>::max() + offsetCount)) {
        return false;
    }
    result = std::chrono::seconds(valueCount - offsetCount);
    return true;
}

std::chrono::seconds SaturatingAdd(
    std::chrono::seconds value,
    std::chrono::seconds offset)
{
    const int64_t valueCount = value.count();
    const int64_t offsetCount = offset.count();
    if (offsetCount > 0 &&
        valueCount > std::numeric_limits<int64_t>::max() - offsetCount) {
        return std::chrono::seconds::max();
    }
    if (offsetCount < 0 &&
        valueCount < std::numeric_limits<int64_t>::min() - offsetCount) {
        return std::chrono::seconds::min();
    }
    return std::chrono::seconds(valueCount + offsetCount);
}

} // namespace

ResolvedTimeZone ResolveTimeZone(std::string_view timeZoneName, bool failOnError)
{
    if (timeZoneName.empty()) {
        return {};
    }
    std::string normalizedId = timeZoneName == "Asia/Beijing"
        ? "Asia/Shanghai"
        : std::string(timeZoneName);
    const tz::TimeZone *timeZone = tz::locateZone(normalizedId, failOnError);
    return {timeZone, std::move(normalizedId)};
}

std::chrono::seconds ConvertLocalToUtc(
    std::chrono::seconds localSeconds,
    const tz::TimeZone *timeZone,
    LocalToUtcState *state)
{
    if (timeZone == nullptr) {
        return localSeconds;
    }

    if (state != nullptr && state->timeZone != timeZone) {
        state->valid = false;
    }

    std::chrono::seconds utcSeconds;
    const bool canReuseState = state != nullptr &&
        state->valid &&
        localSeconds >= state->localBegin &&
        localSeconds < state->localEnd &&
        TrySubtract(localSeconds, state->offset, utcSeconds);
    if (canReuseState) {
        tz::validateRange(tz::time_point<std::chrono::seconds>{localSeconds});
        return utcSeconds;
    }

    utcSeconds = timeZone->to_sys(localSeconds, tz::TimeZone::TChoose::kEarliest);
    if (state == nullptr) {
        return utcSeconds;
    }

    ++state->transitionLookupCount;
    state->timeZone = timeZone;
    state->valid = false;

    try {
        const auto info = timeZone->getInfo(utcSeconds, false);
        std::chrono::seconds expectedUtc;
        if (!TrySubtract(localSeconds, info.offset, expectedUtc) ||
            expectedUtc != utcSeconds) {
            // A nonexistent local time is not part of a reusable linear
            // local-time interval.
            return utcSeconds;
        }

        auto localBegin = SaturatingAdd(info.begin, info.offset);
        const auto localEnd = SaturatingAdd(info.end, info.offset);
        if (info.begin != std::chrono::seconds::min()) {
            const auto previousInfo =
                timeZone->getInfo(info.begin - std::chrono::seconds(1), false);
            const auto previousLocalEnd =
                SaturatingAdd(previousInfo.end, previousInfo.offset);
            if (previousLocalEnd > localBegin) {
                // The overlap belongs to the previous rule under kEarliest.
                localBegin = previousLocalEnd;
            }
        }

        if (localBegin < localEnd &&
            localSeconds >= localBegin &&
            localSeconds < localEnd) {
            state->localBegin = localBegin;
            state->localEnd = localEnd;
            state->offset = info.offset;
            state->valid = true;
        }
    } catch (...) {
        // State population is an optimization and must not change successful
        // conversion behavior at the supported-range boundary.
        state->valid = false;
    }
    return utcSeconds;
}

bool ToCalendar(
    int64_t utcSeconds,
    int32_t microsOfSecond,
    const ResolvedTimeZone &timeZone,
    CalendarTime &result,
    bool requiresZoneName,
    UtcToLocalState *state)
{
    int64_t localSeconds = utcSeconds;
    result = {};
    result.microsOfSecond = microsOfSecond;

    if (timeZone.timeZone != nullptr) {
        const auto instant = std::chrono::seconds(utcSeconds);
        const bool canReuseState = state != nullptr &&
            state->valid &&
            state->timeZone == timeZone.timeZone &&
            instant >= state->begin &&
            instant < state->end &&
            (!requiresZoneName || state->hasAbbreviation);

        std::chrono::seconds offset;
        std::string abbreviation;
        if (canReuseState) {
            offset = state->offset;
            if (requiresZoneName) {
                abbreviation = state->abbreviation;
            }
        } else {
            auto info = timeZone.timeZone->getInfo(instant, requiresZoneName);
            if (state != nullptr) {
                ++state->transitionLookupCount;
            }
            offset = info.offset;
            if (requiresZoneName) {
                abbreviation = info.abbreviation;
            }
            if (state != nullptr) {
                state->timeZone = timeZone.timeZone;
                state->begin = info.begin;
                state->end = info.end;
                state->offset = info.offset;
                state->abbreviation = abbreviation;
                state->hasAbbreviation = requiresZoneName;
                state->valid = true;
            }
        }

        const int64_t offsetSeconds = offset.count();
        if ((offsetSeconds > 0 && utcSeconds > std::numeric_limits<int64_t>::max() - offsetSeconds) ||
            (offsetSeconds < 0 && utcSeconds < std::numeric_limits<int64_t>::min() - offsetSeconds)) {
            OMNI_FAIL("Timestamp is outside the supported timezone conversion range: {}", utcSeconds);
        }
        localSeconds += offsetSeconds;
        result.offsetSeconds = static_cast<int32_t>(offsetSeconds);
        if (requiresZoneName) {
            if (timeZone.displayId.rfind("GMT", 0) == 0) {
                result.zoneAbbreviation = timeZone.displayId;
            } else {
                result.zoneAbbreviation = std::move(abbreviation);
            }
        }
    } else if (requiresZoneName) {
        result.zoneAbbreviation = "GMT";
    }
    return Timestamp::epochToCalendarUtc(localSeconds, result.calendar);
}

} // namespace omniruntime::vectorization::datetime
