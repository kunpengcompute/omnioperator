/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 * Modifications Copyright (c) Huawei Technologies Co., Ltd. 2026-2026.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

// Adapted from the supplementary implementation for:
// Cassio Neri and Lorenz Schneider,
// "Euclidean Affine Functions and their Application to Calendar Algorithms"
// (2022).
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2022 Cassio Neri <cassio.neri@gmail.com>
// SPDX-FileCopyrightText: 2022 Lorenz Schneider <schneider@em-lyon.com>

#include <cstdint>

namespace omniruntime {

struct YearMonthDay {
    int32_t year;
    uint32_t month;
    uint32_t day;
};

namespace fast_date {

inline constexpr uint32_t kEraShift = 82u;
inline constexpr uint32_t kEpochOffset = 719468u + 146097u * kEraShift;
inline constexpr uint32_t kYearOffset = 400u * kEraShift;

// The forward conversion is exact for this epoch-day range.
inline constexpr int32_t kRataDieMin = -12'699'422; // 1 Mar -32800.
inline constexpr int32_t kRataDieMax = 1'061'042'401; // 5 Jun 2907005.

} // namespace fast_date

// Converts days since 1970-01-01 to a proleptic-Gregorian date. Callers must
// check fast_date::kRataDieMin/kRataDieMax before invoking this function.
inline YearMonthDay daysToYmd(int32_t dayNumber)
{
    using namespace fast_date;
    const uint32_t shiftedDay = static_cast<uint32_t>(dayNumber) + kEpochOffset;

    const uint32_t centuryNumerator = 4u * shiftedDay + 3u;
    const uint32_t century = centuryNumerator / 146097u;
    const uint32_t dayWithinCentury = centuryNumerator % 146097u / 4u;

    const uint32_t yearNumerator = 4u * dayWithinCentury + 3u;
    const uint64_t yearProduct = uint64_t{2939745u} * yearNumerator;
    const uint32_t yearWithinCentury = static_cast<uint32_t>(yearProduct >> 32);
    const uint32_t dayWithinYear =
        static_cast<uint32_t>(yearProduct & 0xFFFF'FFFFull) / 2939745u / 4u;
    const uint32_t yearWithinEra = 100u * century + yearWithinCentury;

    const uint32_t monthNumerator = 2141u * dayWithinYear + 197913u;
    const uint32_t monthFromMarch = monthNumerator / 65536u;
    const uint32_t dayOfMonthZeroBased = monthNumerator % 65536u / 2141u;

    const uint32_t janFebAdjust = dayWithinYear >= 306u ? 1u : 0u;
    return {
        static_cast<int32_t>(yearWithinEra - kYearOffset) + static_cast<int32_t>(janFebAdjust),
        janFebAdjust ? monthFromMarch - 12u : monthFromMarch,
        dayOfMonthZeroBased + 1u};
}

} // namespace omniruntime
