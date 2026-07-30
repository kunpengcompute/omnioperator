/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Precompiled date-time format and tzdb conversion tests.
 */

#include <array>
#include <chrono>
#include <future>
#include <functional>
#include <string>

#include <gtest/gtest.h>

#include "vectorization/functions/DateTimeZoneConversion.h"
#include "vectorization/functions/SparkDateTimeFormat.h"

namespace omniruntime::vectorization::datetime {
namespace {

std::chrono::seconds ParseLocalSeconds(std::string_view input)
{
    static const auto format =
        CompileParseFormat("yyyy-MM-dd HH:mm:ss", false);
    int64_t micros = 0;
    EXPECT_TRUE(ParseDateTimeString(input, format, micros));
    return std::chrono::seconds(micros / 1'000'000);
}

TEST(SparkDateTimeFormatTest, ClassifiesAndParsesStandardLayouts)
{
    const auto dateFormat = CompileParseFormat("yyyy-MM-dd", false);
    const auto dateTimeFormat =
        CompileParseFormat("yyyy-MM-dd HH:mm:ss", false);
    EXPECT_EQ(dateFormat.parseFormatKind, ParseFormatKind::YMD);
    EXPECT_EQ(dateTimeFormat.parseFormatKind, ParseFormatKind::YMD_HMS);

    int64_t dateMicros = 0;
    int64_t dateTimeMicros = 0;
    ASSERT_TRUE(ParseDateTimeString("2024-02-29", dateFormat, dateMicros));
    ASSERT_TRUE(ParseDateTimeString(
        "2024-02-29 12:34:56", dateTimeFormat, dateTimeMicros));
    EXPECT_EQ(dateMicros, 1'709'164'800'000'000);
    EXPECT_EQ(dateTimeMicros, 1'709'210'096'000'000);
}

TEST(SparkDateTimeFormatTest, ReusesCompiledParseFormat)
{
    const auto format = CompileParseFormat("yyyy-MM-dd HH:mm:ss.SSS", false);
    EXPECT_EQ(format.strptimeFormat, "%Y-%m-%d %H:%M:%S");
    EXPECT_TRUE(format.hasFractional);

    int64_t first = 0;
    int64_t second = 0;
    ASSERT_TRUE(ParseDateTimeString("1970-01-01 00:00:00.123", format, first));
    ASSERT_TRUE(ParseDateTimeString("2000-02-29 01:02:03.004", format, second));
    EXPECT_EQ(first, 123'000);
    EXPECT_EQ(second, 951'786'123'004'000);
}

TEST(SparkDateTimeFormatTest, PreservesBeijingTimeZoneAlias)
{
    const auto beijing = ResolveTimeZone("Asia/Beijing");
    ASSERT_NE(beijing.timeZone, nullptr);
    EXPECT_EQ(beijing.displayId, "Asia/Shanghai");

    CalendarTime calendarTime;
    ASSERT_TRUE(ToCalendar(0, 0, beijing, calendarTime));
    EXPECT_EQ(calendarTime.calendar.tm_hour, 8);
    EXPECT_TRUE(calendarTime.zoneAbbreviation.empty());
}

TEST(SparkDateTimeFormatTest, PreservesGmtimeZoneNameWithoutConfiguredTimeZone)
{
    const auto utc = ResolveTimeZone("");
    const auto format = CompileFormatPattern("z V", FormatterKind::DATE_FORMAT, false);
    CalendarTime calendarTime;
    ASSERT_TRUE(ToCalendar(0, 0, utc, calendarTime, format.requiresZoneName));
    EXPECT_EQ(FormatDateTime(calendarTime, utc, format), "GMT UTC");
}

TEST(SparkDateTimeFormatTest, TracksZoneNameRequirementAndOptionalCopy)
{
    const auto zoneIdFormat = CompileFormatPattern("V", FormatterKind::DATE_FORMAT, false);
    const auto zoneNameFormat = CompileFormatPattern("z", FormatterKind::DATE_FORMAT, false);
    const auto offsetFormat = CompileFormatPattern("XXX Z %z", FormatterKind::FROM_UNIXTIME, false);

    EXPECT_FALSE(zoneIdFormat.requiresZoneName);
    EXPECT_TRUE(zoneNameFormat.requiresZoneName);
    EXPECT_FALSE(offsetFormat.requiresZoneName);

    const auto shanghai = ResolveTimeZone("Asia/Shanghai");
    const auto infoWithoutZoneName = shanghai.timeZone->getInfo(std::chrono::seconds(0), false);
    EXPECT_TRUE(infoWithoutZoneName.abbreviation.empty());
    EXPECT_LE(infoWithoutZoneName.begin.count(), 0);
    EXPECT_GT(infoWithoutZoneName.end.count(), 0);
}

TEST(SparkDateTimeFormatTest, UsesFixedYmdHmsLayout)
{
    const auto format = CompileFormatPattern(
        "yyyy-MM-dd HH:mm:ss", FormatterKind::FROM_UNIXTIME, false);
    EXPECT_EQ(format.fastFormatKind, FastFormatKind::YMD_HMS);
    EXPECT_EQ(format.fixedResultSize, 19);
    EXPECT_EQ(format.maxResultSize, 19);

    const auto utc = ResolveTimeZone("");
    CalendarTime calendarTime;
    ASSERT_TRUE(ToCalendar(1'704'067'200, 0, utc, calendarTime));

    std::array<char, 19> output{};
    const int32_t length =
        FormatDateTimeToBuffer(calendarTime, utc, format, output.data(), output.size());
    ASSERT_EQ(length, 19);
    EXPECT_EQ(std::string(output.data(), output.size()), "2024-01-01 00:00:00");
}

TEST(SparkDateTimeFormatTest, ReportsInsufficientOutputCapacity)
{
    const auto format = CompileFormatPattern(
        "yyyy-MM-dd HH:mm:ss", FormatterKind::FROM_UNIXTIME, false);
    const auto utc = ResolveTimeZone("");
    CalendarTime calendarTime;
    ASSERT_TRUE(ToCalendar(0, 0, utc, calendarTime));

    std::array<char, 18> output{};
    EXPECT_EQ(
        FormatDateTimeToBuffer(calendarTime, utc, format, output.data(), output.size()),
        -1);
}

TEST(SparkDateTimeFormatTest, ExtendedYearFastLayoutMatchesGenericFallback)
{
    auto fastFormat = CompileFormatPattern(
        "yyyy-MM-dd HH:mm:ss", FormatterKind::FROM_UNIXTIME, false);
    auto genericFormat = fastFormat;
    genericFormat.fastFormatKind = FastFormatKind::NONE;
    const auto utc = ResolveTimeZone("");
    CalendarTime calendarTime;
    calendarTime.calendar.tm_year = -1901;
    calendarTime.calendar.tm_mon = 0;
    calendarTime.calendar.tm_mday = 1;

    const std::string fastResult = FormatDateTime(calendarTime, utc, fastFormat);
    EXPECT_EQ(fastResult, "-0001-01-01 00:00:00");
    EXPECT_EQ(fastResult, FormatDateTime(calendarTime, utc, genericFormat));
}

TEST(SparkDateTimeFormatTest, ReusesStateAndRefreshesAtDaylightSavingTransition)
{
    const auto losAngeles = ResolveTimeZone("America/Los_Angeles");
    UtcToLocalState state;
    CalendarTime before;
    CalendarTime beforeAgain;
    CalendarTime after;
    CalendarTime afterAgain;

    ASSERT_TRUE(ToCalendar(1'710'064'799, 0, losAngeles, before, false, &state));
    ASSERT_TRUE(state.valid);
    ASSERT_EQ(state.transitionLookupCount, 1);
    EXPECT_EQ(before.calendar.tm_hour, 1);
    ASSERT_TRUE(ToCalendar(1'710'064'799, 0, losAngeles, beforeAgain, false, &state));
    EXPECT_EQ(state.transitionLookupCount, 1);
    ASSERT_TRUE(ToCalendar(1'710'064'800, 0, losAngeles, after, false, &state));
    EXPECT_EQ(state.transitionLookupCount, 2);
    EXPECT_EQ(after.calendar.tm_hour, 3);
    EXPECT_NE(before.offsetSeconds, after.offsetSeconds);
    ASSERT_TRUE(ToCalendar(1'710'064'801, 0, losAngeles, afterAgain, false, &state));
    EXPECT_EQ(state.transitionLookupCount, 2);
}

TEST(SparkDateTimeFormatTest, LocalToUtcStateDoesNotCoverDaylightSavingGap)
{
    const auto losAngeles = ResolveTimeZone("America/Los_Angeles");

    LocalToUtcState state;
    const auto before = ParseLocalSeconds("2024-03-10 01:59:59");
    const auto gap = ParseLocalSeconds("2024-03-10 02:30:00");
    const auto after = ParseLocalSeconds("2024-03-10 03:00:00");

    EXPECT_EQ(
        ConvertLocalToUtc(before, losAngeles.timeZone, &state),
        losAngeles.timeZone->to_sys(before, tz::TimeZone::TChoose::kEarliest));
    ASSERT_TRUE(state.valid);
    ASSERT_EQ(state.transitionLookupCount, 1);
    ConvertLocalToUtc(before, losAngeles.timeZone, &state);
    EXPECT_EQ(state.transitionLookupCount, 1);

    EXPECT_EQ(
        ConvertLocalToUtc(gap, losAngeles.timeZone, &state),
        losAngeles.timeZone->to_sys(gap, tz::TimeZone::TChoose::kEarliest));
    EXPECT_FALSE(state.valid);
    EXPECT_EQ(state.transitionLookupCount, 2);

    EXPECT_EQ(
        ConvertLocalToUtc(after, losAngeles.timeZone, &state),
        losAngeles.timeZone->to_sys(after, tz::TimeZone::TChoose::kEarliest));
    ASSERT_TRUE(state.valid);
    EXPECT_EQ(state.transitionLookupCount, 3);
    ConvertLocalToUtc(after, losAngeles.timeZone, &state);
    EXPECT_EQ(state.transitionLookupCount, 3);
}

TEST(SparkDateTimeFormatTest, LocalToUtcStatePreservesEarliestOverlapChoice)
{
    const auto losAngeles = ResolveTimeZone("America/Los_Angeles");

    LocalToUtcState state;
    const auto afterOverlap = ParseLocalSeconds("2024-11-03 02:30:00");
    const auto overlap = ParseLocalSeconds("2024-11-03 01:30:00");

    ConvertLocalToUtc(afterOverlap, losAngeles.timeZone, &state);
    ASSERT_TRUE(state.valid);
    ASSERT_EQ(state.transitionLookupCount, 1);

    const auto expected =
        losAngeles.timeZone->to_sys(overlap, tz::TimeZone::TChoose::kEarliest);
    EXPECT_EQ(
        ConvertLocalToUtc(overlap, losAngeles.timeZone, &state), expected);
    ASSERT_TRUE(state.valid);
    EXPECT_EQ(state.transitionLookupCount, 2);
    EXPECT_EQ(
        ConvertLocalToUtc(overlap, losAngeles.timeZone, &state), expected);
    EXPECT_EQ(state.transitionLookupCount, 2);
}

TEST(SparkDateTimeFormatTest, FixedOffsetStateUpgradesAbbreviationOnce)
{
    const auto fixedOffset = ResolveTimeZone("GMT+08:00");
    UtcToLocalState state;
    CalendarTime withoutName;
    CalendarTime withName;
    CalendarTime withNameAgain;

    ASSERT_TRUE(ToCalendar(0, 0, fixedOffset, withoutName, false, &state));
    ASSERT_EQ(state.transitionLookupCount, 1);
    ASSERT_TRUE(ToCalendar(1, 0, fixedOffset, withName, true, &state));
    ASSERT_EQ(state.transitionLookupCount, 2);
    EXPECT_EQ(withName.zoneAbbreviation, "GMT+08:00");
    ASSERT_TRUE(ToCalendar(2, 0, fixedOffset, withNameAgain, true, &state));
    EXPECT_EQ(state.transitionLookupCount, 2);
}

TEST(SparkDateTimeFormatTest, DoesNotReuseStateAcrossTimeZones)
{
    const auto shanghai = ResolveTimeZone("Asia/Shanghai");
    const auto losAngeles = ResolveTimeZone("America/Los_Angeles");
    UtcToLocalState state;
    CalendarTime shanghaiTime;
    CalendarTime losAngelesTime;

    ASSERT_TRUE(ToCalendar(0, 0, shanghai, shanghaiTime, false, &state));
    ASSERT_TRUE(ToCalendar(0, 0, losAngeles, losAngelesTime, false, &state));
    EXPECT_EQ(state.transitionLookupCount, 2);
    EXPECT_EQ(shanghaiTime.calendar.tm_hour, 8);
    EXPECT_EQ(losAngelesTime.calendar.tm_hour, 16);
    EXPECT_NE(shanghaiTime.calendar.tm_mday, losAngelesTime.calendar.tm_mday);
}

TEST(SparkDateTimeFormatTest, KeepsTimeZonesIndependentAcrossThreads)
{
    const auto shanghai = ResolveTimeZone("Asia/Shanghai");
    const auto losAngeles = ResolveTimeZone("America/Los_Angeles");
    const auto format = CompileFormatPattern(
        "yyyy-MM-dd HH:mm:ss XXX", FormatterKind::DATE_FORMAT, false);

    auto checkRepeatedly = [&format](
        const ResolvedTimeZone &timeZone,
        const std::string &expected) {
        UtcToLocalState state;
        for (int i = 0; i < 2'000; ++i) {
            CalendarTime calendarTime;
            if (!ToCalendar(0, 0, timeZone, calendarTime, false, &state) ||
                FormatDateTime(calendarTime, timeZone, format) != expected) {
                return false;
            }
        }
        return state.transitionLookupCount == 1;
    };

    auto shanghaiResult = std::async(
        std::launch::async,
        checkRepeatedly,
        std::cref(shanghai),
        "1970-01-01 08:00:00 +08:00");
    auto losAngelesResult = std::async(
        std::launch::async,
        checkRepeatedly,
        std::cref(losAngeles),
        "1969-12-31 16:00:00 -08:00");

    EXPECT_TRUE(shanghaiResult.get());
    EXPECT_TRUE(losAngelesResult.get());
}

} // namespace
} // namespace omniruntime::vectorization::datetime
