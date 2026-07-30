/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Explicit tzdb-based timestamp-to-calendar conversion.
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <ctime>
#include <string>
#include <string_view>

#include "type/tz/TimeZoneMap.h"

namespace omniruntime::vectorization::datetime {

struct ResolvedTimeZone {
    const tz::TimeZone *timeZone{nullptr};
    std::string displayId{"UTC"};
};

struct CalendarTime {
    std::tm calendar{};
    int32_t microsOfSecond{0};
    int32_t offsetSeconds{0};
    std::string zoneAbbreviation;
};

struct UtcToLocalState {
    const tz::TimeZone *timeZone{nullptr};
    std::chrono::seconds begin{};
    std::chrono::seconds end{};
    std::chrono::seconds offset{};
    std::string abbreviation;
    uint64_t transitionLookupCount{0};
    bool hasAbbreviation{false};
    bool valid{false};
};

struct LocalToUtcState {
    const tz::TimeZone *timeZone{nullptr};
    std::chrono::seconds localBegin{};
    std::chrono::seconds localEnd{};
    std::chrono::seconds offset{};
    uint64_t transitionLookupCount{0};
    bool valid{false};
};

ResolvedTimeZone ResolveTimeZone(std::string_view timeZoneName, bool failOnError = true);

std::chrono::seconds ConvertLocalToUtc(
    std::chrono::seconds localSeconds,
    const tz::TimeZone *timeZone,
    LocalToUtcState *state = nullptr);

bool ToCalendar(
    int64_t utcSeconds,
    int32_t microsOfSecond,
    const ResolvedTimeZone &timeZone,
    CalendarTime &result,
    bool requiresZoneName = false,
    UtcToLocalState *state = nullptr);

} // namespace omniruntime::vectorization::datetime
