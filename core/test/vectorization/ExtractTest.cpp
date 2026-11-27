/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Extract expression unit tests
 * Validates that all extract sub-functions (year, month, day, hour, minute, second,
 * quarter, dayofweek, dayofyear, week_of_year, weekday, year_of_week) are correctly
 * registered and produce expected results, matching Velox Spark SQL behaviour.
 */

#include <gtest/gtest.h>
#include <vector>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/VectorFunction.h"
#include "codegen/func_signature.h"
#include "vector/vector_helper.h"
#include "vector/vector.h"
#include "type/date_time_utils.h"
#include "type/Timestamp.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::codegen;
using namespace omniruntime::TestUtil;

class ExtractTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const extract_test_env =
    ::testing::AddGlobalTestEnvironment(new ExtractTestEnvironment);

class ExtractTestHelper {
public:
    static BaseVector* CreateDate32Vector(const std::vector<int32_t>& values) {
        BaseVector* vec = VectorHelper::CreateFlatVector(nullptr, OMNI_DATE32, values.size());
        auto* typedVec = static_cast<Vector<int32_t>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typedVec->SetValue(i, values[i]);
        }
        return vec;
    }

    static BaseVector* CreateTimestampVector(const std::vector<int64_t>& values) {
        BaseVector* vec = VectorHelper::CreateFlatVector(nullptr, OMNI_TIMESTAMP, values.size());
        auto* typedVec = static_cast<Vector<int64_t>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typedVec->SetValue(i, values[i]);
        }
        return vec;
    }

    static int32_t DateToDays(int year, int month, int day) {
        LocalDate date(static_cast<int32_t>(year), static_cast<int16_t>(month), static_cast<int16_t>(day));
        return date.ToDays();
    }

    static int64_t TimestampToMicrosUtc(int year, int month, int day, int hour, int minute, int second) {
        std::tm tm = {};
        tm.tm_year = year - 1900;
        tm.tm_mon = month - 1;
        tm.tm_mday = day;
        tm.tm_hour = hour;
        tm.tm_min = minute;
        tm.tm_sec = second;
        tm.tm_isdst = 0;
        int64_t seconds = Timestamp::calendarUtcToEpoch(tm);
        return seconds * 1000000;
    }

    static void ExecuteFunction(const std::string& funcName, BaseVector* inputVec,
        DataTypeId inputTypeId, BaseVector*& result) {
        auto signature = std::make_shared<FunctionSignature>(funcName,
            std::vector<DataTypeId>{inputTypeId}, OMNI_INT);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << funcName << " function not found";

        auto outputType = std::make_shared<DataType>(OMNI_INT);
        ExecutionContext context(4096, nullptr);
        context.SetResultRowSize(inputVec->GetSize());
        std::stack<BaseVector*> args;
        args.push(inputVec);

        ASSERT_NO_THROW(function->Apply(args, outputType, result, &context))
            << funcName << " function threw an exception";
    }

    static void ValidateResult(BaseVector* result, const std::vector<int32_t>& expected, size_t rowSize) {
        auto* resultVec = dynamic_cast<Vector<int32_t>*>(result);
        ASSERT_NE(resultVec, nullptr);
        for (size_t i = 0; i < rowSize; ++i) {
            if (result->IsNull(i)) {
                continue;
            }
            EXPECT_EQ(resultVec->GetValue(i), expected[i]) << "Row " << i << " mismatch";
        }
    }
};

// ===== YEAR =====
TEST(ExtractTest, YearFromDate) {
    std::vector<int32_t> dates = {
        ExtractTestHelper::DateToDays(2024, 1, 15),
        ExtractTestHelper::DateToDays(1970, 1, 1),
        ExtractTestHelper::DateToDays(1999, 12, 31)
    };
    std::vector<int32_t> expected = {2024, 1970, 1999};

    BaseVector* input = ExtractTestHelper::CreateDate32Vector(dates);
    BaseVector* result = nullptr;
    ExtractTestHelper::ExecuteFunction("year", input, OMNI_DATE32, result);
    ExtractTestHelper::ValidateResult(result, expected, dates.size());

    delete result;
}

TEST(ExtractTest, YearFromTimestamp) {
    std::vector<int64_t> timestamps = {
        ExtractTestHelper::TimestampToMicrosUtc(2024, 1, 15, 10, 30, 0),
        ExtractTestHelper::TimestampToMicrosUtc(1970, 1, 1, 0, 0, 0),
        ExtractTestHelper::TimestampToMicrosUtc(2001, 8, 22, 14, 30, 45)
    };
    std::vector<int32_t> expected = {2024, 1970, 2001};

    BaseVector* input = ExtractTestHelper::CreateTimestampVector(timestamps);
    BaseVector* result = nullptr;
    ExtractTestHelper::ExecuteFunction("year", input, OMNI_TIMESTAMP, result);
    ExtractTestHelper::ValidateResult(result, expected, timestamps.size());

    delete result;
}

// ===== MONTH =====
TEST(ExtractTest, MonthFromDate) {
    std::vector<int32_t> dates = {
        ExtractTestHelper::DateToDays(2024, 1, 15),
        ExtractTestHelper::DateToDays(2024, 6, 20),
        ExtractTestHelper::DateToDays(2024, 12, 31)
    };
    std::vector<int32_t> expected = {1, 6, 12};

    BaseVector* input = ExtractTestHelper::CreateDate32Vector(dates);
    BaseVector* result = nullptr;
    ExtractTestHelper::ExecuteFunction("month", input, OMNI_DATE32, result);
    ExtractTestHelper::ValidateResult(result, expected, dates.size());

    delete result;
}

// ===== DAY =====
TEST(ExtractTest, DayFromDate) {
    std::vector<int32_t> dates = {
        ExtractTestHelper::DateToDays(2024, 1, 1),
        ExtractTestHelper::DateToDays(2024, 2, 29),
        ExtractTestHelper::DateToDays(2024, 12, 31)
    };
    std::vector<int32_t> expected = {1, 29, 31};

    BaseVector* input = ExtractTestHelper::CreateDate32Vector(dates);
    BaseVector* result = nullptr;
    ExtractTestHelper::ExecuteFunction("day", input, OMNI_DATE32, result);
    ExtractTestHelper::ValidateResult(result, expected, dates.size());

    delete result;
}

// ===== DAYOFMONTH (alias for day) =====
TEST(ExtractTest, DayOfMonthFromDate) {
    std::vector<int32_t> dates = {
        ExtractTestHelper::DateToDays(2024, 3, 15),
        ExtractTestHelper::DateToDays(2024, 7, 4)
    };
    std::vector<int32_t> expected = {15, 4};

    BaseVector* input = ExtractTestHelper::CreateDate32Vector(dates);
    BaseVector* result = nullptr;
    ExtractTestHelper::ExecuteFunction("dayofmonth", input, OMNI_DATE32, result);
    ExtractTestHelper::ValidateResult(result, expected, dates.size());

    delete result;
}

// ===== HOUR =====
TEST(ExtractTest, HourFromTimestamp) {
    std::vector<int64_t> timestamps = {
        ExtractTestHelper::TimestampToMicrosUtc(2024, 1, 1, 0, 0, 0),
        ExtractTestHelper::TimestampToMicrosUtc(2024, 1, 1, 12, 30, 0),
        ExtractTestHelper::TimestampToMicrosUtc(2024, 1, 1, 23, 59, 59)
    };
    std::vector<int32_t> expected = {0, 12, 23};

    BaseVector* input = ExtractTestHelper::CreateTimestampVector(timestamps);
    BaseVector* result = nullptr;
    ExtractTestHelper::ExecuteFunction("hour", input, OMNI_TIMESTAMP, result);
    ExtractTestHelper::ValidateResult(result, expected, timestamps.size());

    delete result;
}

// ===== MINUTE =====
TEST(ExtractTest, MinuteFromTimestamp) {
    std::vector<int64_t> timestamps = {
        ExtractTestHelper::TimestampToMicrosUtc(2024, 1, 1, 10, 0, 0),
        ExtractTestHelper::TimestampToMicrosUtc(2024, 1, 1, 10, 30, 0),
        ExtractTestHelper::TimestampToMicrosUtc(2024, 1, 1, 10, 59, 0)
    };
    std::vector<int32_t> expected = {0, 30, 59};

    BaseVector* input = ExtractTestHelper::CreateTimestampVector(timestamps);
    BaseVector* result = nullptr;
    ExtractTestHelper::ExecuteFunction("minute", input, OMNI_TIMESTAMP, result);
    ExtractTestHelper::ValidateResult(result, expected, timestamps.size());

    delete result;
}

// ===== SECOND =====
TEST(ExtractTest, SecondFromTimestamp) {
    std::vector<int64_t> timestamps = {
        ExtractTestHelper::TimestampToMicrosUtc(2024, 1, 1, 10, 30, 0),
        ExtractTestHelper::TimestampToMicrosUtc(2024, 1, 1, 10, 30, 30),
        ExtractTestHelper::TimestampToMicrosUtc(2024, 1, 1, 10, 30, 59)
    };
    std::vector<int32_t> expected = {0, 30, 59};

    BaseVector* input = ExtractTestHelper::CreateTimestampVector(timestamps);
    BaseVector* result = nullptr;
    ExtractTestHelper::ExecuteFunction("second", input, OMNI_TIMESTAMP, result);
    ExtractTestHelper::ValidateResult(result, expected, timestamps.size());

    delete result;
}

// ===== QUARTER =====
TEST(ExtractTest, QuarterFromDate) {
    std::vector<int32_t> dates = {
        ExtractTestHelper::DateToDays(2024, 1, 15),
        ExtractTestHelper::DateToDays(2024, 4, 15),
        ExtractTestHelper::DateToDays(2024, 8, 15),
        ExtractTestHelper::DateToDays(2024, 11, 15)
    };
    std::vector<int32_t> expected = {1, 2, 3, 4};

    BaseVector* input = ExtractTestHelper::CreateDate32Vector(dates);
    BaseVector* result = nullptr;
    ExtractTestHelper::ExecuteFunction("quarter", input, OMNI_DATE32, result);
    ExtractTestHelper::ValidateResult(result, expected, dates.size());

    delete result;
}

// ===== DAYOFWEEK =====
TEST(ExtractTest, DayOfWeekFromDate) {
    // Spark DayOfWeek: 1=Sunday, 2=Monday, ..., 7=Saturday
    // 2024-01-01 is Monday -> 2
    // 2024-01-06 is Saturday -> 7
    // 2024-01-07 is Sunday -> 1
    std::vector<int32_t> dates = {
        ExtractTestHelper::DateToDays(2024, 1, 1),
        ExtractTestHelper::DateToDays(2024, 1, 6),
        ExtractTestHelper::DateToDays(2024, 1, 7)
    };
    std::vector<int32_t> expected = {2, 7, 1};

    BaseVector* input = ExtractTestHelper::CreateDate32Vector(dates);
    BaseVector* result = nullptr;
    ExtractTestHelper::ExecuteFunction("dayofweek", input, OMNI_DATE32, result);
    ExtractTestHelper::ValidateResult(result, expected, dates.size());

    delete result;
}

// ===== DAYOFYEAR =====
TEST(ExtractTest, DayOfYearFromDate) {
    // 2024-01-01 -> 1
    // 2024-02-29 -> 31+29 = 60 (leap year)
    // 2024-12-31 -> 366 (leap year)
    std::vector<int32_t> dates = {
        ExtractTestHelper::DateToDays(2024, 1, 1),
        ExtractTestHelper::DateToDays(2024, 2, 29),
        ExtractTestHelper::DateToDays(2024, 12, 31)
    };
    std::vector<int32_t> expected = {1, 60, 366};

    BaseVector* input = ExtractTestHelper::CreateDate32Vector(dates);
    BaseVector* result = nullptr;
    ExtractTestHelper::ExecuteFunction("dayofyear", input, OMNI_DATE32, result);
    ExtractTestHelper::ValidateResult(result, expected, dates.size());

    delete result;
}

// ===== WEEK_OF_YEAR =====
TEST(ExtractTest, WeekOfYearFromDate) {
    // 2024-01-01 is Monday -> ISO week 1
    // 2024-02-20 -> ISO week 8
    std::vector<int32_t> dates = {
        ExtractTestHelper::DateToDays(2024, 1, 1),
        ExtractTestHelper::DateToDays(2024, 2, 20)
    };
    std::vector<int32_t> expected = {1, 8};

    BaseVector* input = ExtractTestHelper::CreateDate32Vector(dates);
    BaseVector* result = nullptr;
    ExtractTestHelper::ExecuteFunction("week_of_year", input, OMNI_DATE32, result);
    ExtractTestHelper::ValidateResult(result, expected, dates.size());

    delete result;
}

// ===== WEEKDAY =====
TEST(ExtractTest, WeekdayFromDate) {
    // Spark Weekday: 0=Monday, 1=Tuesday, ..., 6=Sunday
    // 2024-01-01 is Monday -> 0
    // 2024-01-06 is Saturday -> 5
    // 2024-01-07 is Sunday -> 6
    std::vector<int32_t> dates = {
        ExtractTestHelper::DateToDays(2024, 1, 1),
        ExtractTestHelper::DateToDays(2024, 1, 6),
        ExtractTestHelper::DateToDays(2024, 1, 7)
    };
    std::vector<int32_t> expected = {0, 5, 6};

    BaseVector* input = ExtractTestHelper::CreateDate32Vector(dates);
    BaseVector* result = nullptr;
    ExtractTestHelper::ExecuteFunction("weekday", input, OMNI_DATE32, result);
    ExtractTestHelper::ValidateResult(result, expected, dates.size());

    delete result;
}

// ===== YEAR_OF_WEEK =====
TEST(ExtractTest, YearOfWeekFromDate) {
    // 2021-01-01 is Friday -> belongs to ISO week 53 of 2020
    // 2024-06-15 -> belongs to 2024
    std::vector<int32_t> dates = {
        ExtractTestHelper::DateToDays(2021, 1, 1),
        ExtractTestHelper::DateToDays(2024, 6, 15)
    };
    std::vector<int32_t> expected = {2020, 2024};

    BaseVector* input = ExtractTestHelper::CreateDate32Vector(dates);
    BaseVector* result = nullptr;
    ExtractTestHelper::ExecuteFunction("year_of_week", input, OMNI_DATE32, result);
    ExtractTestHelper::ValidateResult(result, expected, dates.size());

    delete result;
}

// ===== NULL propagation for all extract types =====
TEST(ExtractTest, NullPropagationYear) {
    std::vector<int32_t> dates = {ExtractTestHelper::DateToDays(2024, 1, 15)};

    BaseVector* input = ExtractTestHelper::CreateDate32Vector(dates);
    input->SetNull(0);

    BaseVector* result = nullptr;
    ExtractTestHelper::ExecuteFunction("year", input, OMNI_DATE32, result);
    EXPECT_TRUE(result->IsNull(0));

    delete result;
}

TEST(ExtractTest, NullPropagationHour) {
    std::vector<int64_t> timestamps = {0LL};

    BaseVector* input = ExtractTestHelper::CreateTimestampVector(timestamps);
    input->SetNull(0);

    BaseVector* result = nullptr;
    ExtractTestHelper::ExecuteFunction("hour", input, OMNI_TIMESTAMP, result);
    EXPECT_TRUE(result->IsNull(0));

    delete result;
}

TEST(ExtractTest, NullPropagationMinute) {
    std::vector<int64_t> timestamps = {0LL};

    BaseVector* input = ExtractTestHelper::CreateTimestampVector(timestamps);
    input->SetNull(0);

    BaseVector* result = nullptr;
    ExtractTestHelper::ExecuteFunction("minute", input, OMNI_TIMESTAMP, result);
    EXPECT_TRUE(result->IsNull(0));

    delete result;
}

TEST(ExtractTest, NullPropagationSecond) {
    std::vector<int64_t> timestamps = {0LL};

    BaseVector* input = ExtractTestHelper::CreateTimestampVector(timestamps);
    input->SetNull(0);

    BaseVector* result = nullptr;
    ExtractTestHelper::ExecuteFunction("second", input, OMNI_TIMESTAMP, result);
    EXPECT_TRUE(result->IsNull(0));

    delete result;
}

// ===== Epoch boundary tests =====
TEST(ExtractTest, EpochTimestamp) {
    // epoch = 1970-01-01 00:00:00 UTC
    std::vector<int64_t> timestamps = {0LL};
    BaseVector* input = ExtractTestHelper::CreateTimestampVector(timestamps);

    // year
    BaseVector* yearResult = nullptr;
    ExtractTestHelper::ExecuteFunction("year", input, OMNI_TIMESTAMP, yearResult);
    EXPECT_EQ(dynamic_cast<Vector<int32_t>*>(yearResult)->GetValue(0), 1970);
    delete yearResult;

    // hour
    input = ExtractTestHelper::CreateTimestampVector(timestamps);
    BaseVector* hourResult = nullptr;
    ExtractTestHelper::ExecuteFunction("hour", input, OMNI_TIMESTAMP, hourResult);
    EXPECT_EQ(dynamic_cast<Vector<int32_t>*>(hourResult)->GetValue(0), 0);
    delete hourResult;

    // minute
    input = ExtractTestHelper::CreateTimestampVector(timestamps);
    BaseVector* minResult = nullptr;
    ExtractTestHelper::ExecuteFunction("minute", input, OMNI_TIMESTAMP, minResult);
    EXPECT_EQ(dynamic_cast<Vector<int32_t>*>(minResult)->GetValue(0), 0);
    delete minResult;

    // second
    input = ExtractTestHelper::CreateTimestampVector(timestamps);
    BaseVector* secResult = nullptr;
    ExtractTestHelper::ExecuteFunction("second", input, OMNI_TIMESTAMP, secResult);
    EXPECT_EQ(dynamic_cast<Vector<int32_t>*>(secResult)->GetValue(0), 0);
    delete secResult;
}

// ===== Epoch date boundary =====
TEST(ExtractTest, EpochDate) {
    // 1970-01-01 = day 0
    std::vector<int32_t> dates = {0};

    BaseVector* input = ExtractTestHelper::CreateDate32Vector(dates);
    BaseVector* result = nullptr;
    ExtractTestHelper::ExecuteFunction("year", input, OMNI_DATE32, result);
    EXPECT_EQ(dynamic_cast<Vector<int32_t>*>(result)->GetValue(0), 1970);
    delete result;

    input = ExtractTestHelper::CreateDate32Vector(dates);
    result = nullptr;
    ExtractTestHelper::ExecuteFunction("month", input, OMNI_DATE32, result);
    EXPECT_EQ(dynamic_cast<Vector<int32_t>*>(result)->GetValue(0), 1);
    delete result;

    input = ExtractTestHelper::CreateDate32Vector(dates);
    result = nullptr;
    ExtractTestHelper::ExecuteFunction("day", input, OMNI_DATE32, result);
    EXPECT_EQ(dynamic_cast<Vector<int32_t>*>(result)->GetValue(0), 1);
    delete result;
}
