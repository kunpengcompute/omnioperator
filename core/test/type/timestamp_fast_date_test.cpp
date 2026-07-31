/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Timestamp UTC calendar fast-path tests.
 */

#include <cstdint>
#include <ctime>

#include <gtest/gtest.h>

#include "type/FastDate.h"
#include "type/Timestamp.h"

namespace omniruntime {
namespace {

constexpr int64_t kSecondsPerDay = 86'400;

void ExpectCalendar(
    int64_t epochSeconds,
    int64_t year,
    int month,
    int day,
    int hour,
    int minute,
    int second)
{
    std::tm calendar{};
    ASSERT_TRUE(Timestamp::epochToCalendarUtc(epochSeconds, calendar));
    EXPECT_EQ(calendar.tm_year, year - 1900);
    EXPECT_EQ(calendar.tm_mon, month - 1);
    EXPECT_EQ(calendar.tm_mday, day);
    EXPECT_EQ(calendar.tm_hour, hour);
    EXPECT_EQ(calendar.tm_min, minute);
    EXPECT_EQ(calendar.tm_sec, second);
    EXPECT_EQ(calendar.tm_isdst, 0);
}

TEST(TimestampFastDateTest, ConvertsRepresentativeEpochs)
{
    ExpectCalendar(0, 1970, 1, 1, 0, 0, 0);
    ExpectCalendar(-1, 1969, 12, 31, 23, 59, 59);
    ExpectCalendar(951'782'400, 2000, 2, 29, 0, 0, 0);
}

TEST(TimestampFastDateTest, CoversFastPathBoundaries)
{
    ExpectCalendar(
        static_cast<int64_t>(fast_date::kRataDieMin) * kSecondsPerDay,
        -32'800, 3, 1, 0, 0, 0);
    ExpectCalendar(
        static_cast<int64_t>(fast_date::kRataDieMax) * kSecondsPerDay,
        2'907'005, 6, 5, 0, 0, 0);
}

TEST(TimestampFastDateTest, FallsBackOutsideFastPath)
{
    ExpectCalendar(
        static_cast<int64_t>(fast_date::kRataDieMin - 1) * kSecondsPerDay,
        -32'800, 2, 29, 0, 0, 0);
    ExpectCalendar(
        static_cast<int64_t>(fast_date::kRataDieMax + 1) * kSecondsPerDay,
        2'907'005, 6, 6, 0, 0, 0);
}

} // namespace
} // namespace omniruntime
