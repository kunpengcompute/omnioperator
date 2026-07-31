/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: flink_dayofmonth function unit tests
 *
 * flink_dayofmonth(date) -> int32, flink_dayofmonth(timestamp_millis) -> int32
 * Mirrors Flink's DAYOFMONTH(date) == EXTRACT(DAY FROM date):
 *   - OMNI_INT  : date = days since epoch; day of month extracted in UTC.
 *   - OMNI_LONG : Flink TimestampData = milliseconds since epoch (NOT micros);
 *                 day of month extracted in UTC, no session timezone (wall-clock
 *                 semantics, matching Flink EXTRACT(DAY FROM <TIMESTAMP>)).
 *
 * Expected values below are derived from the Flink reference semantics
 * (DateTimeUtils.julianExtract DAY = `return day`), the authoritative spec.
 * Verified against sql_functions.yml (DAYOFMONTH(DATE '1994-09-27') = 27) and
 * ScalarFunctionsTest (DAY(f16)=10 for 1996-11-10). All expected values
 * cross-checked against Python's date.day.
 */

#include <gtest/gtest.h>
#include <iostream>
#include <vector>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/functions/FlinkDayOfMonth.h"
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

// Initialize function registration before running tests
class FlinkDayOfMonthTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const flink_dayofmonth_test_env =
    ::testing::AddGlobalTestEnvironment(new FlinkDayOfMonthTestEnvironment);

class FlinkDayOfMonthTestHelper {
public:
    static void ValidateResult(BaseVector* result, const std::vector<int32_t>& expected, int rowSize) {
        auto* resultVec = dynamic_cast<Vector<int32_t>*>(result);
        ASSERT_NE(resultVec, nullptr) << "Result vector type mismatch";

        for (int i = 0; i < rowSize; ++i) {
            if (result->IsNull(i)) {
                std::cout << "Row " << i << ": NULL" << std::endl;
                continue;
            }
            int32_t actualValue = resultVec->GetValue(i);
            int32_t expectedValue = expected[i];
            std::cout << "Row " << i << ": Expected=" << expectedValue << ", Actual=" << actualValue << std::endl;
            EXPECT_EQ(actualValue, expectedValue) << "Row " << i << " value mismatch";
        }
    }

    static BaseVector* CreateIntVector(const std::vector<int32_t>& values) {
        BaseVector* vec = VectorHelper::CreateFlatVector(OMNI_INT, values.size());
        auto* typedVec = static_cast<Vector<int32_t>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typedVec->SetValue(i, values[i]);
        }
        return vec;
    }

    static BaseVector* CreateLongVector(const std::vector<int64_t>& values) {
        BaseVector* vec = VectorHelper::CreateFlatVector(OMNI_LONG, values.size());
        auto* typedVec = static_cast<Vector<int64_t>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typedVec->SetValue(i, values[i]);
        }
        return vec;
    }

    static void ExecuteFlinkDayOfMonth(BaseVector* inputVec, DataTypeId inputTypeId, BaseVector*& result) {
        auto signature = std::make_shared<FunctionSignature>("flink_dayofmonth",
            std::vector<DataTypeId>{inputTypeId}, OMNI_INT);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "flink_dayofmonth function not found for signature";

        auto outputType = std::make_shared<DataType>(OMNI_INT);
        ExecutionContext context;
        context.SetResultRowSize(inputVec->GetSize());
        std::stack<BaseVector*> args;
        args.push(inputVec);

        ASSERT_NO_THROW(function->Apply(args, outputType, result, &context))
            << "flink_dayofmonth function threw an exception";
    }

    // Convert date components to days since epoch (matches the implementation's
    // date calculation, same helper as YearTest/FlinkYearTest).
    static int32_t DateToDays(int year, int month, int day) {
        LocalDate date(static_cast<int32_t>(year), static_cast<int16_t>(month), static_cast<int16_t>(day));
        return date.ToDays();
    }

    // Convert a UTC wall-clock datetime to Flink TIMESTAMP millis (epoch millis).
    // Flink TimestampData stores millisecond = epochDay * 86400000 + nanoOfDay/1e6.
    // For whole-second values this is simply epochSeconds * 1000. The day is
    // then extracted in UTC (no session tz), so this is the canonical input.
    static int64_t TimestampToMillisUtc(int year, int month, int day, int hour, int minute, int second) {
        std::tm tm = {};
        tm.tm_year = year - 1900;
        tm.tm_mon = month - 1;
        tm.tm_mday = day;
        tm.tm_hour = hour;
        tm.tm_min = minute;
        tm.tm_sec = second;
        tm.tm_isdst = 0;
        int64_t seconds = Timestamp::calendarUtcToEpoch(tm);
        return seconds * 1000;
    }
};

// ============================================================================
// OMNI_INT (date = days since epoch) — day of month extracted in UTC
// ============================================================================

TEST(FlinkDayOfMonthTest, IntBasic) {
    std::cout << "=== Test: flink_dayofmonth from INT (date) - basic ===" << std::endl;

    // sql_functions.yml: DAYOFMONTH(DATE '1994-09-27') = 27.
    // ScalarFunctionsTest: DAY(f16) = 10 where f16 = DATE '1996-11-10'.
    // 2024-01-01 -> day 1.
    std::vector<int32_t> intValues = {
        FlinkDayOfMonthTestHelper::DateToDays(1994, 9, 27),
        FlinkDayOfMonthTestHelper::DateToDays(1996, 11, 10),
        FlinkDayOfMonthTestHelper::DateToDays(2024, 1, 1)
    };
    std::vector<int32_t> expected = {27, 10, 1};

    BaseVector* inputVec = FlinkDayOfMonthTestHelper::CreateIntVector(intValues);
    BaseVector* resultVec = nullptr;
    FlinkDayOfMonthTestHelper::ExecuteFlinkDayOfMonth(inputVec, OMNI_INT, resultVec);
    FlinkDayOfMonthTestHelper::ValidateResult(resultVec, expected, intValues.size());

    delete resultVec;
}

TEST(FlinkDayOfMonthTest, IntMonthEndAndLeapBoundary) {
    // Month-end and leap-year boundaries:
    //   2024-02-29 (leap Feb end) -> 29.
    //   2023-02-28 (non-leap Feb end) -> 28.
    //   2024-04-30 (Apr end, 30-day month) -> 30.
    //   2024-01-31 (Jan end, 31-day month) -> 31.
    std::vector<int32_t> dateValues = {
        FlinkDayOfMonthTestHelper::DateToDays(2024, 2, 29),
        FlinkDayOfMonthTestHelper::DateToDays(2023, 2, 28),
        FlinkDayOfMonthTestHelper::DateToDays(2024, 4, 30),
        FlinkDayOfMonthTestHelper::DateToDays(2024, 1, 31)
    };
    std::vector<int32_t> expected = {29, 28, 30, 31};

    BaseVector* inputVec = FlinkDayOfMonthTestHelper::CreateIntVector(dateValues);
    BaseVector* resultVec = nullptr;
    FlinkDayOfMonthTestHelper::ExecuteFlinkDayOfMonth(inputVec, OMNI_INT, resultVec);
    FlinkDayOfMonthTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

TEST(FlinkDayOfMonthTest, IntWithNull) {
    std::cout << "=== Test: flink_dayofmonth from INT with NULL ===" << std::endl;

    std::vector<int32_t> intValues = {
        FlinkDayOfMonthTestHelper::DateToDays(2024, 1, 1),
        FlinkDayOfMonthTestHelper::DateToDays(2024, 6, 20),
        FlinkDayOfMonthTestHelper::DateToDays(2024, 12, 31)
    };

    BaseVector* inputVec = FlinkDayOfMonthTestHelper::CreateIntVector(intValues);
    inputVec->SetNull(1);

    BaseVector* resultVec = nullptr;
    FlinkDayOfMonthTestHelper::ExecuteFlinkDayOfMonth(inputVec, OMNI_INT, resultVec);

    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";

    auto* resultVecTyped = dynamic_cast<Vector<int32_t>*>(resultVec);
    EXPECT_EQ(resultVecTyped->GetValue(0), 1) << "Row 0 day should be 1 (2024-01-01)";
    EXPECT_EQ(resultVecTyped->GetValue(2), 31) << "Row 2 day should be 31 (2024-12-31)";

    delete resultVec;
}

TEST(FlinkDayOfMonthTest, IntPreEpoch) {
    // Negative days (before 1970): 1969-12-31 -> day 31.
    std::vector<int32_t> dateValues = {
        FlinkDayOfMonthTestHelper::DateToDays(1969, 12, 31),
        FlinkDayOfMonthTestHelper::DateToDays(1969, 1, 1)
    };
    std::vector<int32_t> expected = {31, 1};

    BaseVector* inputVec = FlinkDayOfMonthTestHelper::CreateIntVector(dateValues);
    BaseVector* resultVec = nullptr;
    FlinkDayOfMonthTestHelper::ExecuteFlinkDayOfMonth(inputVec, OMNI_INT, resultVec);
    FlinkDayOfMonthTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

// ============================================================================
// OMNI_LONG (Flink TIMESTAMP = milliseconds since epoch) — no session tz
// ============================================================================

TEST(FlinkDayOfMonthTest, LongBasicMillis) {
    std::cout << "=== Test: flink_dayofmonth from LONG (millis) - basic ===" << std::endl;

    // Flink test reference: DAY(f18) = 10 for TIMESTAMP '1996-11-10 06:55:44'.
    // Also the sql_functions.yml example date as a timestamp: 1994-09-27 -> 27.
    std::vector<int64_t> millisValues = {
        FlinkDayOfMonthTestHelper::TimestampToMillisUtc(1994, 9, 27, 0, 0, 0),
        FlinkDayOfMonthTestHelper::TimestampToMillisUtc(1996, 11, 10, 6, 55, 44)
    };
    std::vector<int32_t> expected = {27, 10};

    BaseVector* inputVec = FlinkDayOfMonthTestHelper::CreateLongVector(millisValues);
    BaseVector* resultVec = nullptr;
    FlinkDayOfMonthTestHelper::ExecuteFlinkDayOfMonth(inputVec, OMNI_LONG, resultVec);
    FlinkDayOfMonthTestHelper::ValidateResult(resultVec, expected, millisValues.size());

    delete resultVec;
}

TEST(FlinkDayOfMonthTest, LongMonthAndYearBoundary) {
    // Wall-clock month/year boundaries (no session tz):
    //   2024-01-31 23:59:59 -> 31; 2024-02-01 00:00:00 -> 1.
    //   2024-12-31 23:59:59 -> 31; 2025-01-01 00:00:00 -> 1.
    std::vector<int64_t> millisValues = {
        FlinkDayOfMonthTestHelper::TimestampToMillisUtc(2024, 1, 31, 23, 59, 59),
        FlinkDayOfMonthTestHelper::TimestampToMillisUtc(2024, 2, 1, 0, 0, 0),
        FlinkDayOfMonthTestHelper::TimestampToMillisUtc(2024, 12, 31, 23, 59, 59),
        FlinkDayOfMonthTestHelper::TimestampToMillisUtc(2025, 1, 1, 0, 0, 0)
    };
    std::vector<int32_t> expected = {31, 1, 31, 1};

    BaseVector* inputVec = FlinkDayOfMonthTestHelper::CreateLongVector(millisValues);
    BaseVector* resultVec = nullptr;
    FlinkDayOfMonthTestHelper::ExecuteFlinkDayOfMonth(inputVec, OMNI_LONG, resultVec);
    FlinkDayOfMonthTestHelper::ValidateResult(resultVec, expected, millisValues.size());

    delete resultVec;
}

TEST(FlinkDayOfMonthTest, LongPreEpochNegativeMillis) {
    // Negative millis (before 1970): 1969-12-31 23:59:59 -> day 31.
    // Timestamp::fromMillis handles negative millis via floor division.
    std::vector<int64_t> millisValues = {
        FlinkDayOfMonthTestHelper::TimestampToMillisUtc(1969, 12, 31, 23, 59, 59),
        FlinkDayOfMonthTestHelper::TimestampToMillisUtc(1969, 1, 1, 0, 0, 0)
    };
    std::vector<int32_t> expected = {31, 1};

    BaseVector* inputVec = FlinkDayOfMonthTestHelper::CreateLongVector(millisValues);
    BaseVector* resultVec = nullptr;
    FlinkDayOfMonthTestHelper::ExecuteFlinkDayOfMonth(inputVec, OMNI_LONG, resultVec);
    FlinkDayOfMonthTestHelper::ValidateResult(resultVec, expected, millisValues.size());

    delete resultVec;
}

TEST(FlinkDayOfMonthTest, LongWithNull) {
    std::cout << "=== Test: flink_dayofmonth from LONG with NULL ===" << std::endl;

    std::vector<int64_t> millisValues = {
        FlinkDayOfMonthTestHelper::TimestampToMillisUtc(2024, 1, 1, 8, 0, 0),
        FlinkDayOfMonthTestHelper::TimestampToMillisUtc(2024, 6, 20, 12, 0, 0),
        FlinkDayOfMonthTestHelper::TimestampToMillisUtc(2024, 12, 31, 0, 0, 0)
    };

    BaseVector* inputVec = FlinkDayOfMonthTestHelper::CreateLongVector(millisValues);
    inputVec->SetNull(1);

    BaseVector* resultVec = nullptr;
    FlinkDayOfMonthTestHelper::ExecuteFlinkDayOfMonth(inputVec, OMNI_LONG, resultVec);

    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";

    auto* resultVecTyped = dynamic_cast<Vector<int32_t>*>(resultVec);
    EXPECT_EQ(resultVecTyped->GetValue(0), 1) << "Row 0 day should be 1 (2024-01-01)";
    EXPECT_EQ(resultVecTyped->GetValue(2), 31) << "Row 2 day should be 31 (2024-12-31)";

    delete resultVec;
}

TEST(FlinkDayOfMonthTest, LongLargeYear) {
    // Large year: 9999-12-31 23:59:59 -> day 31.
    std::vector<int64_t> millisValues = {
        FlinkDayOfMonthTestHelper::TimestampToMillisUtc(9999, 12, 31, 23, 59, 59)
    };
    std::vector<int32_t> expected = {31};

    BaseVector* inputVec = FlinkDayOfMonthTestHelper::CreateLongVector(millisValues);
    BaseVector* resultVec = nullptr;
    FlinkDayOfMonthTestHelper::ExecuteFlinkDayOfMonth(inputVec, OMNI_LONG, resultVec);
    FlinkDayOfMonthTestHelper::ValidateResult(resultVec, expected, millisValues.size());

    delete resultVec;
}

TEST(FlinkDayOfMonthTest, LongMillisSemanticsVsMicros) {
    // Anti-regression: prove the OMNI_LONG path interprets the value as
    // MILLIS, not micros. 2024-06-01 00:00:00 UTC = 1717200000 seconds =
    // 1717200000000 millis -> day 1 (2024-06-01). If the impl wrongly used
    // fromMicros, the value would be treated as 1000x smaller and fall in
    // 1970-01-20 (Jan 20 = day 20), a clearly different result.
    int64_t millis = FlinkDayOfMonthTestHelper::TimestampToMillisUtc(2024, 6, 1, 0, 0, 0);
    ASSERT_EQ(millis, 1717200000000);

    std::vector<int64_t> millisValues = {millis};
    BaseVector* inputVec = FlinkDayOfMonthTestHelper::CreateLongVector(millisValues);
    BaseVector* resultVec = nullptr;
    FlinkDayOfMonthTestHelper::ExecuteFlinkDayOfMonth(inputVec, OMNI_LONG, resultVec);

    ASSERT_NE(resultVec, nullptr);
    EXPECT_FALSE(resultVec->IsNull(0));
    auto* resultVecTyped = dynamic_cast<Vector<int32_t>*>(resultVec);
    // Must be 1 (2024-06-01, millis semantics). A buggy fromMicros path would give 20.
    EXPECT_EQ(resultVecTyped->GetValue(0), 1);

    delete resultVec;
}

// ============================================================================
// Multi-row batch mixing timestamps across several days
// ============================================================================

TEST(FlinkDayOfMonthTest, MultiRowBatchLong) {
    std::vector<int64_t> millisValues = {
        FlinkDayOfMonthTestHelper::TimestampToMillisUtc(1970, 1, 1, 0, 0, 0),    // epoch -> day 1
        FlinkDayOfMonthTestHelper::TimestampToMillisUtc(2024, 5, 15, 0, 0, 0),   // -> day 15
        FlinkDayOfMonthTestHelper::TimestampToMillisUtc(2024, 8, 20, 0, 0, 0),   // -> day 20
        FlinkDayOfMonthTestHelper::TimestampToMillisUtc(1996, 11, 10, 6, 55, 44) // -> day 10
    };
    std::vector<int32_t> expected = {1, 15, 20, 10};

    BaseVector* inputVec = FlinkDayOfMonthTestHelper::CreateLongVector(millisValues);
    BaseVector* resultVec = nullptr;
    FlinkDayOfMonthTestHelper::ExecuteFlinkDayOfMonth(inputVec, OMNI_LONG, resultVec);
    FlinkDayOfMonthTestHelper::ValidateResult(resultVec, expected, millisValues.size());

    delete resultVec;
}
