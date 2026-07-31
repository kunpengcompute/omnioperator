/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: flink_dayofyear function unit tests
 *
 * flink_dayofyear(date) -> int32, flink_dayofyear(timestamp_millis) -> int32
 * Mirrors Flink's DAYOFYEAR(date) == EXTRACT(DOY FROM date):
 *   - OMNI_INT  : date = days since epoch; day of year extracted in UTC.
 *   - OMNI_LONG : Flink TimestampData = milliseconds since epoch (NOT micros);
 *                 day of year extracted in UTC, no session timezone (wall-clock
 *                 semantics, matching Flink EXTRACT(DOY FROM <TIMESTAMP>)).
 *
 * Expected values below are derived from the Flink reference semantics
 * (DateTimeUtils.julianExtract DOY = (julian - ymdToJulian(year,1,1)) + 1),
 * the authoritative spec. Verified against sql_functions.yml
 * (DAYOFYEAR(DATE '1994-09-27') = 270) and ScalarFunctionsTest
 * (DAYOFYEAR(f16)=315, DAYOFYEAR(f18)=315 for 1996-11-10). All expected values
 * cross-checked against Python's date.timetuple().tm_yday.
 */

#include <gtest/gtest.h>
#include <iostream>
#include <vector>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/functions/FlinkDayOfYear.h"
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
class FlinkDayOfYearTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const flink_dayofyear_test_env =
    ::testing::AddGlobalTestEnvironment(new FlinkDayOfYearTestEnvironment);

class FlinkDayOfYearTestHelper {
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

    static void ExecuteFlinkDayOfYear(BaseVector* inputVec, DataTypeId inputTypeId, BaseVector*& result) {
        auto signature = std::make_shared<FunctionSignature>("flink_dayofyear",
            std::vector<DataTypeId>{inputTypeId}, OMNI_INT);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "flink_dayofyear function not found for signature";

        auto outputType = std::make_shared<DataType>(OMNI_INT);
        ExecutionContext context;
        context.SetResultRowSize(inputVec->GetSize());
        std::stack<BaseVector*> args;
        args.push(inputVec);

        ASSERT_NO_THROW(function->Apply(args, outputType, result, &context))
            << "flink_dayofyear function threw an exception";
    }

    // Convert date components to days since epoch (matches the implementation's
    // date calculation, same helper as YearTest/FlinkYearTest).
    static int32_t DateToDays(int year, int month, int day) {
        LocalDate date(static_cast<int32_t>(year), static_cast<int16_t>(month), static_cast<int16_t>(day));
        return date.ToDays();
    }

    // Convert a UTC wall-clock datetime to Flink TIMESTAMP millis (epoch millis).
    // Flink TimestampData stores millisecond = epochDay * 86400000 + nanoOfDay/1e6.
    // For whole-second values this is simply epochSeconds * 1000. The DOY is
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
// OMNI_INT (date = days since epoch) — day of year extracted in UTC
// ============================================================================

TEST(FlinkDayOfYearTest, IntBasic) {
    std::cout << "=== Test: flink_dayofyear from INT (date) - basic ===" << std::endl;

    // sql_functions.yml: DAYOFYEAR(DATE '1994-09-27') = 270.
    // ScalarFunctionsTest: DAYOFYEAR(f16) = 315 where f16 = DATE '1996-11-10'.
    // 2024-01-01 -> DOY 1.
    std::vector<int32_t> intValues = {
        FlinkDayOfYearTestHelper::DateToDays(1994, 9, 27),
        FlinkDayOfYearTestHelper::DateToDays(1996, 11, 10),
        FlinkDayOfYearTestHelper::DateToDays(2024, 1, 1)
    };
    std::vector<int32_t> expected = {270, 315, 1};

    BaseVector* inputVec = FlinkDayOfYearTestHelper::CreateIntVector(intValues);
    BaseVector* resultVec = nullptr;
    FlinkDayOfYearTestHelper::ExecuteFlinkDayOfYear(inputVec, OMNI_INT, resultVec);
    FlinkDayOfYearTestHelper::ValidateResult(resultVec, expected, intValues.size());

    delete resultVec;
}

TEST(FlinkDayOfYearTest, IntLeapYearAndNonLeapBoundary) {
    // Leap vs non-leap year DOY:
    //   2024-02-29 (leap) -> DOY 60 (Feb 29 is the 60th day).
    //   2023-03-01 (non-leap) -> DOY 60 (Mar 1 is the 60th day; same DOY as
    //     leap Feb 29 because the leap day shifts everything after Feb 28).
    //   2024-12-31 (leap) -> DOY 366.
    //   2023-12-31 (non-leap) -> DOY 365.
    std::vector<int32_t> dateValues = {
        FlinkDayOfYearTestHelper::DateToDays(2024, 2, 29),
        FlinkDayOfYearTestHelper::DateToDays(2023, 3, 1),
        FlinkDayOfYearTestHelper::DateToDays(2024, 12, 31),
        FlinkDayOfYearTestHelper::DateToDays(2023, 12, 31)
    };
    std::vector<int32_t> expected = {60, 60, 366, 365};

    BaseVector* inputVec = FlinkDayOfYearTestHelper::CreateIntVector(dateValues);
    BaseVector* resultVec = nullptr;
    FlinkDayOfYearTestHelper::ExecuteFlinkDayOfYear(inputVec, OMNI_INT, resultVec);
    FlinkDayOfYearTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

TEST(FlinkDayOfYearTest, IntWithNull) {
    std::cout << "=== Test: flink_dayofyear from INT with NULL ===" << std::endl;

    std::vector<int32_t> intValues = {
        FlinkDayOfYearTestHelper::DateToDays(2024, 1, 1),
        FlinkDayOfYearTestHelper::DateToDays(2024, 6, 20),
        FlinkDayOfYearTestHelper::DateToDays(2024, 12, 31)
    };

    BaseVector* inputVec = FlinkDayOfYearTestHelper::CreateIntVector(intValues);
    inputVec->SetNull(1);

    BaseVector* resultVec = nullptr;
    FlinkDayOfYearTestHelper::ExecuteFlinkDayOfYear(inputVec, OMNI_INT, resultVec);

    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";

    auto* resultVecTyped = dynamic_cast<Vector<int32_t>*>(resultVec);
    EXPECT_EQ(resultVecTyped->GetValue(0), 1) << "Row 0 DOY should be 1 (2024-01-01)";
    EXPECT_EQ(resultVecTyped->GetValue(2), 366) << "Row 2 DOY should be 366 (2024-12-31, leap)";

    delete resultVec;
}

TEST(FlinkDayOfYearTest, IntPreEpoch) {
    // Negative days (before 1970): 1969-12-31 (1969 is non-leap) -> DOY 365.
    std::vector<int32_t> dateValues = {
        FlinkDayOfYearTestHelper::DateToDays(1969, 12, 31),
        FlinkDayOfYearTestHelper::DateToDays(1969, 1, 1)
    };
    std::vector<int32_t> expected = {365, 1};

    BaseVector* inputVec = FlinkDayOfYearTestHelper::CreateIntVector(dateValues);
    BaseVector* resultVec = nullptr;
    FlinkDayOfYearTestHelper::ExecuteFlinkDayOfYear(inputVec, OMNI_INT, resultVec);
    FlinkDayOfYearTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

// ============================================================================
// OMNI_LONG (Flink TIMESTAMP = milliseconds since epoch) — no session tz
// ============================================================================

TEST(FlinkDayOfYearTest, LongBasicMillis) {
    std::cout << "=== Test: flink_dayofyear from LONG (millis) - basic ===" << std::endl;

    // Flink test reference: DAYOFYEAR(f18) = 315 for TIMESTAMP '1996-11-10 06:55:44'.
    // Also the sql_functions.yml example date as a timestamp: 1994-09-27 -> 270.
    std::vector<int64_t> millisValues = {
        FlinkDayOfYearTestHelper::TimestampToMillisUtc(1994, 9, 27, 0, 0, 0),
        FlinkDayOfYearTestHelper::TimestampToMillisUtc(1996, 11, 10, 6, 55, 44)
    };
    std::vector<int32_t> expected = {270, 315};

    BaseVector* inputVec = FlinkDayOfYearTestHelper::CreateLongVector(millisValues);
    BaseVector* resultVec = nullptr;
    FlinkDayOfYearTestHelper::ExecuteFlinkDayOfYear(inputVec, OMNI_LONG, resultVec);
    FlinkDayOfYearTestHelper::ValidateResult(resultVec, expected, millisValues.size());

    delete resultVec;
}

TEST(FlinkDayOfYearTest, LongYearBoundary) {
    // Wall-clock year boundary (no session tz):
    //   2024-12-31 23:59:59 (leap) -> DOY 366; 2025-01-01 00:00:00 -> DOY 1.
    std::vector<int64_t> millisValues = {
        FlinkDayOfYearTestHelper::TimestampToMillisUtc(2024, 12, 31, 23, 59, 59),
        FlinkDayOfYearTestHelper::TimestampToMillisUtc(2025, 1, 1, 0, 0, 0)
    };
    std::vector<int32_t> expected = {366, 1};

    BaseVector* inputVec = FlinkDayOfYearTestHelper::CreateLongVector(millisValues);
    BaseVector* resultVec = nullptr;
    FlinkDayOfYearTestHelper::ExecuteFlinkDayOfYear(inputVec, OMNI_LONG, resultVec);
    FlinkDayOfYearTestHelper::ValidateResult(resultVec, expected, millisValues.size());

    delete resultVec;
}

TEST(FlinkDayOfYearTest, LongPreEpochNegativeMillis) {
    // Negative millis (before 1970): 1969-12-31 23:59:59 (1969 non-leap) -> DOY 365.
    // Timestamp::fromMillis handles negative millis via floor division.
    std::vector<int64_t> millisValues = {
        FlinkDayOfYearTestHelper::TimestampToMillisUtc(1969, 12, 31, 23, 59, 59),
        FlinkDayOfYearTestHelper::TimestampToMillisUtc(1969, 1, 1, 0, 0, 0)
    };
    std::vector<int32_t> expected = {365, 1};

    BaseVector* inputVec = FlinkDayOfYearTestHelper::CreateLongVector(millisValues);
    BaseVector* resultVec = nullptr;
    FlinkDayOfYearTestHelper::ExecuteFlinkDayOfYear(inputVec, OMNI_LONG, resultVec);
    FlinkDayOfYearTestHelper::ValidateResult(resultVec, expected, millisValues.size());

    delete resultVec;
}

TEST(FlinkDayOfYearTest, LongWithNull) {
    std::cout << "=== Test: flink_dayofyear from LONG with NULL ===" << std::endl;

    std::vector<int64_t> millisValues = {
        FlinkDayOfYearTestHelper::TimestampToMillisUtc(2024, 1, 1, 8, 0, 0),
        FlinkDayOfYearTestHelper::TimestampToMillisUtc(2024, 6, 20, 12, 0, 0),
        FlinkDayOfYearTestHelper::TimestampToMillisUtc(2024, 12, 31, 0, 0, 0)
    };

    BaseVector* inputVec = FlinkDayOfYearTestHelper::CreateLongVector(millisValues);
    inputVec->SetNull(1);

    BaseVector* resultVec = nullptr;
    FlinkDayOfYearTestHelper::ExecuteFlinkDayOfYear(inputVec, OMNI_LONG, resultVec);

    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";

    auto* resultVecTyped = dynamic_cast<Vector<int32_t>*>(resultVec);
    EXPECT_EQ(resultVecTyped->GetValue(0), 1) << "Row 0 DOY should be 1 (2024-01-01)";
    EXPECT_EQ(resultVecTyped->GetValue(2), 366) << "Row 2 DOY should be 366 (2024-12-31, leap)";

    delete resultVec;
}

TEST(FlinkDayOfYearTest, LongLargeYear) {
    // Large year: 9999-12-31 23:59:59 (9999 non-leap, not divisible by 400) -> DOY 365.
    std::vector<int64_t> millisValues = {
        FlinkDayOfYearTestHelper::TimestampToMillisUtc(9999, 12, 31, 23, 59, 59)
    };
    std::vector<int32_t> expected = {365};

    BaseVector* inputVec = FlinkDayOfYearTestHelper::CreateLongVector(millisValues);
    BaseVector* resultVec = nullptr;
    FlinkDayOfYearTestHelper::ExecuteFlinkDayOfYear(inputVec, OMNI_LONG, resultVec);
    FlinkDayOfYearTestHelper::ValidateResult(resultVec, expected, millisValues.size());

    delete resultVec;
}

TEST(FlinkDayOfYearTest, LongMillisSemanticsVsMicros) {
    // Anti-regression: prove the OMNI_LONG path interprets the value as
    // MILLIS, not micros. 2024-06-01 00:00:00 UTC = 1717200000 seconds =
    // 1717200000000 millis -> DOY 153 (2024-06-01 is the 153rd day of 2024).
    // If the impl wrongly used fromMicros, the value would be treated as
    // 1000x smaller and fall in 1970-01-20 (Jan 20 = DOY 20), a clearly
    // different result.
    int64_t millis = FlinkDayOfYearTestHelper::TimestampToMillisUtc(2024, 6, 1, 0, 0, 0);
    ASSERT_EQ(millis, 1717200000000);

    std::vector<int64_t> millisValues = {millis};
    BaseVector* inputVec = FlinkDayOfYearTestHelper::CreateLongVector(millisValues);
    BaseVector* resultVec = nullptr;
    FlinkDayOfYearTestHelper::ExecuteFlinkDayOfYear(inputVec, OMNI_LONG, resultVec);

    ASSERT_NE(resultVec, nullptr);
    EXPECT_FALSE(resultVec->IsNull(0));
    auto* resultVecTyped = dynamic_cast<Vector<int32_t>*>(resultVec);
    // Must be 153 (2024-06-01, millis semantics). A buggy fromMicros path would give 20.
    EXPECT_EQ(resultVecTyped->GetValue(0), 153);

    delete resultVec;
}

// ============================================================================
// Multi-row batch mixing timestamps across the year
// ============================================================================

TEST(FlinkDayOfYearTest, MultiRowBatchLong) {
    std::vector<int64_t> millisValues = {
        FlinkDayOfYearTestHelper::TimestampToMillisUtc(1970, 1, 1, 0, 0, 0),    // epoch -> DOY 1
        FlinkDayOfYearTestHelper::TimestampToMillisUtc(2024, 5, 15, 0, 0, 0),   // -> DOY 136
        FlinkDayOfYearTestHelper::TimestampToMillisUtc(2024, 8, 20, 0, 0, 0),   // -> DOY 233
        FlinkDayOfYearTestHelper::TimestampToMillisUtc(1996, 11, 10, 6, 55, 44) // -> DOY 315
    };
    std::vector<int32_t> expected = {1, 136, 233, 315};

    BaseVector* inputVec = FlinkDayOfYearTestHelper::CreateLongVector(millisValues);
    BaseVector* resultVec = nullptr;
    FlinkDayOfYearTestHelper::ExecuteFlinkDayOfYear(inputVec, OMNI_LONG, resultVec);
    FlinkDayOfYearTestHelper::ValidateResult(resultVec, expected, millisValues.size());

    delete resultVec;
}
