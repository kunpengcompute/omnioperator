/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2024. All rights reserved.
 */

#include "gtest/gtest.h"
#include "type/data_operations.h"
#include <ctime>

using namespace omniruntime::type;

namespace DataOperationsTest {

TEST(ConvertDateStringToInteger, NormalDate)
{
    int64_t result;
    const char* dateStr = "2024-01-01";
    Status status = ConvertDateStringToInteger(result, dateStr, strlen(dateStr));
    EXPECT_EQ(status, Status::CONVERT_SUCCESS);

    struct tm timeinfo = {0};
    timeinfo.tm_year = 2024 - 1900;
    timeinfo.tm_mon = 0;
    timeinfo.tm_mday = 1;
    timeinfo.tm_hour = 0;
    timeinfo.tm_min = 0;
    timeinfo.tm_sec = 0;
    timeinfo.tm_isdst = 0;
    time_t expected = timegm(&timeinfo);

    EXPECT_EQ(result, static_cast<int64_t>(expected) * 1000);
}

TEST(ConvertDateStringToInteger, EpochDate)
{
    int64_t result;
    const char* dateStr = "1970-01-01";
    Status status = ConvertDateStringToInteger(result, dateStr, strlen(dateStr));
    EXPECT_EQ(status, Status::CONVERT_SUCCESS);
    EXPECT_EQ(result, 0);
}

TEST(ConvertDateStringToInteger, MinYear)
{
    int64_t result;
    const char* dateStr = "0000-01-01";
    Status status = ConvertDateStringToInteger(result, dateStr, strlen(dateStr));
    EXPECT_EQ(status, Status::CONVERT_SUCCESS);

    struct tm timeinfo = {0};
    timeinfo.tm_year = 0 - 1900;
    timeinfo.tm_mon = 0;
    timeinfo.tm_mday = 1;
    timeinfo.tm_hour = 0;
    timeinfo.tm_min = 0;
    timeinfo.tm_sec = 0;
    timeinfo.tm_isdst = 0;
    time_t expected = timegm(&timeinfo);

    EXPECT_EQ(result, static_cast<int64_t>(expected) * 1000);
}

TEST(ConvertDateStringToInteger, MaxYear)
{
    int64_t result;
    const char* dateStr = "9999-12-31";
    Status status = ConvertDateStringToInteger(result, dateStr, strlen(dateStr));
    EXPECT_EQ(status, Status::CONVERT_SUCCESS);

    struct tm timeinfo = {0};
    timeinfo.tm_year = 9999 - 1900;
    timeinfo.tm_mon = 11;
    timeinfo.tm_mday = 31;
    timeinfo.tm_hour = 0;
    timeinfo.tm_min = 0;
    timeinfo.tm_sec = 0;
    timeinfo.tm_isdst = 0;
    time_t expected = timegm(&timeinfo);

    EXPECT_EQ(result, static_cast<int64_t>(expected) * 1000);
}

TEST(ConvertDateStringToInteger, LeapYearFeb29)
{
    int64_t result;
    const char* dateStr = "2024-02-29";
    Status status = ConvertDateStringToInteger(result, dateStr, strlen(dateStr));
    EXPECT_EQ(status, Status::CONVERT_SUCCESS);

    struct tm timeinfo = {0};
    timeinfo.tm_year = 2024 - 1900;
    timeinfo.tm_mon = 1;
    timeinfo.tm_mday = 29;
    timeinfo.tm_hour = 0;
    timeinfo.tm_min = 0;
    timeinfo.tm_sec = 0;
    timeinfo.tm_isdst = 0;
    time_t expected = timegm(&timeinfo);

    EXPECT_EQ(result, static_cast<int64_t>(expected) * 1000);
}

TEST(ConvertDateStringToInteger, NonLeapYearFeb29Invalid)
{
    int64_t result;
    const char* dateStr = "2023-02-29";
    Status status = ConvertDateStringToInteger(result, dateStr, strlen(dateStr));
    EXPECT_EQ(status, Status::IS_NOT_A_NUMBER);
}

TEST(ConvertDateStringToInteger, InvalidFormatMissingSeparator)
{
    int64_t result;
    const char* dateStr = "20240101";
    Status status = ConvertDateStringToInteger(result, dateStr, strlen(dateStr));
    EXPECT_EQ(status, Status::IS_NOT_A_NUMBER);
}

TEST(ConvertDateStringToInteger, InvalidFormatWrongSeparator)
{
    int64_t result;
    const char* dateStr = "2024/01/01";
    Status status = ConvertDateStringToInteger(result, dateStr, strlen(dateStr));
    EXPECT_EQ(status, Status::IS_NOT_A_NUMBER);
}

TEST(ConvertDateStringToInteger, InvalidFormatTooShort)
{
    int64_t result;
    const char* dateStr = "2024-01-";
    Status status = ConvertDateStringToInteger(result, dateStr, strlen(dateStr));
    EXPECT_EQ(status, Status::IS_NOT_A_NUMBER);
}

TEST(ConvertDateStringToInteger, InvalidFormatTooLong)
{
    int64_t result;
    const char* dateStr = "2024-01-01-";
    Status status = ConvertDateStringToInteger(result, dateStr, strlen(dateStr));
    EXPECT_EQ(status, Status::IS_NOT_A_NUMBER);
}

TEST(ConvertDateStringToInteger, InvalidMonth)
{
    int64_t result;
    const char* dateStr = "2024-13-01";
    Status status = ConvertDateStringToInteger(result, dateStr, strlen(dateStr));
    EXPECT_EQ(status, Status::IS_NOT_A_NUMBER);
}

TEST(ConvertDateStringToInteger, InvalidMonthZero)
{
    int64_t result;
    const char* dateStr = "2024-00-01";
    Status status = ConvertDateStringToInteger(result, dateStr, strlen(dateStr));
    EXPECT_EQ(status, Status::IS_NOT_A_NUMBER);
}

TEST(ConvertDateStringToInteger, InvalidDay)
{
    int64_t result;
    const char* dateStr = "2024-01-32";
    Status status = ConvertDateStringToInteger(result, dateStr, strlen(dateStr));
    EXPECT_EQ(status, Status::IS_NOT_A_NUMBER);
}

TEST(ConvertDateStringToInteger, InvalidDayZero)
{
    int64_t result;
    const char* dateStr = "2024-01-00";
    Status status = ConvertDateStringToInteger(result, dateStr, strlen(dateStr));
    EXPECT_EQ(status, Status::IS_NOT_A_NUMBER);
}

TEST(ConvertDateStringToInteger, InvalidDayForMonth)
{
    int64_t result;
    const char* dateStr = "2024-04-31";
    Status status = ConvertDateStringToInteger(result, dateStr, strlen(dateStr));
    EXPECT_EQ(status, Status::IS_NOT_A_NUMBER);
}

TEST(ConvertDateStringToInteger, YearOverflowNegative)
{
    int64_t result;
    const char* dateStr = "-0001-01-01";
    Status status = ConvertDateStringToInteger(result, dateStr, strlen(dateStr));
    EXPECT_EQ(status, Status::IS_NOT_A_NUMBER);
}

TEST(ConvertDateStringToInteger, YearOverflowTooLarge)
{
    int64_t result;
    const char* dateStr = "10000-01-01";
    Status status = ConvertDateStringToInteger(result, dateStr, strlen(dateStr));
    EXPECT_EQ(status, Status::CONVERT_OVERFLOW);
}

TEST(ConvertDateStringToInteger, WithLeadingSpaces)
{
    int64_t result;
    const char* dateStr = "  2024-01-01";
    Status status = ConvertDateStringToInteger(result, dateStr, strlen(dateStr));
    EXPECT_EQ(status, Status::CONVERT_SUCCESS);

    struct tm timeinfo = {0};
    timeinfo.tm_year = 2024 - 1900;
    timeinfo.tm_mon = 0;
    timeinfo.tm_mday = 1;
    timeinfo.tm_hour = 0;
    timeinfo.tm_min = 0;
    timeinfo.tm_sec = 0;
    timeinfo.tm_isdst = 0;
    time_t expected = timegm(&timeinfo);

    EXPECT_EQ(result, static_cast<int64_t>(expected) * 1000);
}

TEST(ConvertDateStringToInteger, WithTrailingSpaces)
{
    int64_t result;
    const char* dateStr = "2024-01-01  ";
    Status status = ConvertDateStringToInteger(result, dateStr, strlen(dateStr));
    EXPECT_EQ(status, Status::CONVERT_SUCCESS);

    struct tm timeinfo = {0};
    timeinfo.tm_year = 2024 - 1900;
    timeinfo.tm_mon = 0;
    timeinfo.tm_mday = 1;
    timeinfo.tm_hour = 0;
    timeinfo.tm_min = 0;
    timeinfo.tm_sec = 0;
    timeinfo.tm_isdst = 0;
    time_t expected = timegm(&timeinfo);

    EXPECT_EQ(result, static_cast<int64_t>(expected) * 1000);
}

TEST(ConvertDateStringToInteger, WithBothSpaces)
{
    int64_t result;
    const char* dateStr = "  2024-01-01  ";
    Status status = ConvertDateStringToInteger(result, dateStr, strlen(dateStr));
    EXPECT_EQ(status, Status::CONVERT_SUCCESS);

    struct tm timeinfo = {0};
    timeinfo.tm_year = 2024 - 1900;
    timeinfo.tm_mon = 0;
    timeinfo.tm_mday = 1;
    timeinfo.tm_hour = 0;
    timeinfo.tm_min = 0;
    timeinfo.tm_sec = 0;
    timeinfo.tm_isdst = 0;
    time_t expected = timegm(&timeinfo);

    EXPECT_EQ(result, static_cast<int64_t>(expected) * 1000);
}

TEST(ConvertDateStringToInteger, OnlySpaces)
{
    int64_t result;
    const char* dateStr = "     ";
    Status status = ConvertDateStringToInteger(result, dateStr, strlen(dateStr));
    EXPECT_EQ(status, Status::IS_NOT_A_NUMBER);
}

TEST(ConvertDateStringToInteger, EmptyString)
{
    int64_t result;
    const char* dateStr = "";
    Status status = ConvertDateStringToInteger(result, dateStr, 0);
    EXPECT_EQ(status, Status::IS_NOT_A_NUMBER);
}

TEST(ConvertDateStringToInteger, NotADate)
{
    int64_t result;
    const char* dateStr = "hello-world";
    Status status = ConvertDateStringToInteger(result, dateStr, strlen(dateStr));
    EXPECT_EQ(status, Status::IS_NOT_A_NUMBER);
}

TEST(ConvertDateStringToInteger, CenturyLeapYear)
{
    int64_t result;
    const char* dateStr = "2000-02-29";
    Status status = ConvertDateStringToInteger(result, dateStr, strlen(dateStr));
    EXPECT_EQ(status, Status::CONVERT_SUCCESS);

    struct tm timeinfo = {0};
    timeinfo.tm_year = 2000 - 1900;
    timeinfo.tm_mon = 1;
    timeinfo.tm_mday = 29;
    timeinfo.tm_hour = 0;
    timeinfo.tm_min = 0;
    timeinfo.tm_sec = 0;
    timeinfo.tm_isdst = 0;
    time_t expected = timegm(&timeinfo);

    EXPECT_EQ(result, static_cast<int64_t>(expected) * 1000);
}

TEST(ConvertDateStringToInteger, CenturyNonLeapYear)
{
    int64_t result;
    const char* dateStr = "1900-02-29";
    Status status = ConvertDateStringToInteger(result, dateStr, strlen(dateStr));
    EXPECT_EQ(status, Status::IS_NOT_A_NUMBER);
}

TEST(ConvertDateStringToInteger, DifferentMonths)
{
    struct TestCase {
        const char* dateStr;
        bool shouldSucceed;
    };

    TestCase testCases[] = {
        {"2024-01-31", true},
        {"2024-02-28", true},
        {"2024-03-31", true},
        {"2024-04-30", true},
        {"2024-05-31", true},
        {"2024-06-30", true},
        {"2024-07-31", true},
        {"2024-08-31", true},
        {"2024-09-30", true},
        {"2024-10-31", true},
        {"2024-11-30", true},
        {"2024-12-31", true},
    };

    for (const auto& testCase : testCases) {
        int64_t result;
        Status status = ConvertDateStringToInteger(result, testCase.dateStr, strlen(testCase.dateStr));
        EXPECT_EQ(status, Status::CONVERT_SUCCESS);

        int year, month, day;
        sscanf(testCase.dateStr, "%d-%d-%d", &year, &month, &day);

        struct tm timeinfo = {0};
        timeinfo.tm_year = year - 1900;
        timeinfo.tm_mon = month - 1;
        timeinfo.tm_mday = day;
        timeinfo.tm_hour = 0;
        timeinfo.tm_min = 0;
        timeinfo.tm_sec = 0;
        timeinfo.tm_isdst = 0;
        time_t expected = timegm(&timeinfo);

        EXPECT_EQ(result, static_cast<int64_t>(expected) * 1000);
    }
}

TEST(ConvertDateStringToInteger, DifferentYears)
{
    struct TestCase {
        const char* dateStr;
        int64_t expectedTimestamp;
    };

    TestCase testCases[] = {
        {"1970-01-01", 0},
        {"2000-01-01", 946684800000},
        {"2020-01-01", 1577836800000},
    };

    for (const auto& testCase : testCases) {
        int64_t result;
        Status status = ConvertDateStringToInteger(result, testCase.dateStr, strlen(testCase.dateStr));
        EXPECT_EQ(status, Status::CONVERT_SUCCESS);
        EXPECT_EQ(result, testCase.expectedTimestamp);
    }
}

}