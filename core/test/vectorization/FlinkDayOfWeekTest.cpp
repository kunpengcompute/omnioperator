/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: flink_dayofweek function unit tests
 *
 * flink_dayofweek(date) -> int32, flink_dayofweek(timestamp_millis) -> int32
 * Mirrors Flink's DAYOFWEEK(date) == EXTRACT(DOW FROM date):
 *   - OMNI_INT  : date = days since epoch; day of week extracted in UTC.
 *   - OMNI_LONG : Flink TimestampData = milliseconds since epoch (NOT micros);
 *                 day of week extracted in UTC, no session timezone (wall-clock
 *                 semantics, matching Flink EXTRACT(DOW FROM <TIMESTAMP>)).
 *
 * Returns Sunday=1, Monday=2, ..., Saturday=7 (DOW, NOT ISODOW).
 *
 * Expected values below are derived from the Flink reference semantics
 * (DateTimeUtils.julianExtract DOW = floorMod((julian + 1), 7) + 1), the
 * authoritative spec. Verified against sql_functions.yml
 * (DAYOFWEEK(DATE '1994-09-27') = 3) and ScalarFunctionsTest
 * (DAYOFWEEK(f16)=1, DAYOFWEEK(f18)=1 for 1996-11-10, a Sunday). All expected
 * values cross-checked against Python (Flink DOW = (isoweekday % 7) + 1).
 */

#include <gtest/gtest.h>
#include <iostream>
#include <vector>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/functions/FlinkDayOfWeek.h"
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
class FlinkDayOfWeekTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const flink_dayofweek_test_env =
    ::testing::AddGlobalTestEnvironment(new FlinkDayOfWeekTestEnvironment);

class FlinkDayOfWeekTestHelper {
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

    static void ExecuteFlinkDayOfWeek(BaseVector* inputVec, DataTypeId inputTypeId, BaseVector*& result) {
        auto signature = std::make_shared<FunctionSignature>("flink_dayofweek",
            std::vector<DataTypeId>{inputTypeId}, OMNI_INT);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "flink_dayofweek function not found for signature";

        auto outputType = std::make_shared<DataType>(OMNI_INT);
        ExecutionContext context;
        context.SetResultRowSize(inputVec->GetSize());
        std::stack<BaseVector*> args;
        args.push(inputVec);

        ASSERT_NO_THROW(function->Apply(args, outputType, result, &context))
            << "flink_dayofweek function threw an exception";
    }

    // Convert date components to days since epoch (matches the implementation's
    // date calculation, same helper as YearTest/FlinkYearTest).
    static int32_t DateToDays(int year, int month, int day) {
        LocalDate date(static_cast<int32_t>(year), static_cast<int16_t>(month), static_cast<int16_t>(day));
        return date.ToDays();
    }

    // Convert a UTC wall-clock datetime to Flink TIMESTAMP millis (epoch millis).
    // Flink TimestampData stores millisecond = epochDay * 86400000 + nanoOfDay/1e6.
    // For whole-second values this is simply epochSeconds * 1000. The DOW is
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
// OMNI_INT (date = days since epoch) — day of week extracted in UTC (DOW)
// ============================================================================

TEST(FlinkDayOfWeekTest, IntBasic) {
    std::cout << "=== Test: flink_dayofweek from INT (date) - basic ===" << std::endl;

    // sql_functions.yml: DAYOFWEEK(DATE '1994-09-27') = 3 (Tue; Sun=1,Tue=3).
    // ScalarFunctionsTest: DAYOFWEEK(f16) = 1 where f16 = DATE '1996-11-10' (Sun).
    // 2024-01-01 (Mon) -> DOW 2.
    std::vector<int32_t> intValues = {
        FlinkDayOfWeekTestHelper::DateToDays(1994, 9, 27),
        FlinkDayOfWeekTestHelper::DateToDays(1996, 11, 10),
        FlinkDayOfWeekTestHelper::DateToDays(2024, 1, 1)
    };
    std::vector<int32_t> expected = {3, 1, 2};

    BaseVector* inputVec = FlinkDayOfWeekTestHelper::CreateIntVector(intValues);
    BaseVector* resultVec = nullptr;
    FlinkDayOfWeekTestHelper::ExecuteFlinkDayOfWeek(inputVec, OMNI_INT, resultVec);
    FlinkDayOfWeekTestHelper::ValidateResult(resultVec, expected, intValues.size());

    delete resultVec;
}

TEST(FlinkDayOfWeekTest, IntFullWeekCoverage) {
    // 7 consecutive days 2024-03-17 (Sun) .. 2024-03-23 (Sat) -> DOW 1..7.
    // Confirms Sunday=1, Monday=2, ..., Saturday=7 (DOW, not ISODOW).
    std::vector<int32_t> dateValues = {
        FlinkDayOfWeekTestHelper::DateToDays(2024, 3, 17),  // Sun -> 1
        FlinkDayOfWeekTestHelper::DateToDays(2024, 3, 18),  // Mon -> 2
        FlinkDayOfWeekTestHelper::DateToDays(2024, 3, 19),  // Tue -> 3
        FlinkDayOfWeekTestHelper::DateToDays(2024, 3, 20),  // Wed -> 4
        FlinkDayOfWeekTestHelper::DateToDays(2024, 3, 21),  // Thu -> 5
        FlinkDayOfWeekTestHelper::DateToDays(2024, 3, 22),  // Fri -> 6
        FlinkDayOfWeekTestHelper::DateToDays(2024, 3, 23)   // Sat -> 7
    };
    std::vector<int32_t> expected = {1, 2, 3, 4, 5, 6, 7};

    BaseVector* inputVec = FlinkDayOfWeekTestHelper::CreateIntVector(dateValues);
    BaseVector* resultVec = nullptr;
    FlinkDayOfWeekTestHelper::ExecuteFlinkDayOfWeek(inputVec, OMNI_INT, resultVec);
    FlinkDayOfWeekTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

TEST(FlinkDayOfWeekTest, IntWithNull) {
    std::cout << "=== Test: flink_dayofweek from INT with NULL ===" << std::endl;

    std::vector<int32_t> intValues = {
        FlinkDayOfWeekTestHelper::DateToDays(2024, 1, 1),   // Mon -> 2
        FlinkDayOfWeekTestHelper::DateToDays(2024, 6, 20),  // Thu -> 5
        FlinkDayOfWeekTestHelper::DateToDays(2024, 12, 31)  // Tue -> 3
    };

    BaseVector* inputVec = FlinkDayOfWeekTestHelper::CreateIntVector(intValues);
    inputVec->SetNull(1);

    BaseVector* resultVec = nullptr;
    FlinkDayOfWeekTestHelper::ExecuteFlinkDayOfWeek(inputVec, OMNI_INT, resultVec);

    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";

    auto* resultVecTyped = dynamic_cast<Vector<int32_t>*>(resultVec);
    EXPECT_EQ(resultVecTyped->GetValue(0), 2) << "Row 0 DOW should be 2 (2024-01-01 Mon)";

    delete resultVec;
}

TEST(FlinkDayOfWeekTest, IntPreEpoch) {
    // Negative days (before 1970): 1969-12-31 (Wed) -> DOW 4 (Sun=1, Wed=4).
    std::vector<int32_t> dateValues = {
        FlinkDayOfWeekTestHelper::DateToDays(1969, 12, 31),
        FlinkDayOfWeekTestHelper::DateToDays(1969, 1, 1)    // Wed -> 4
    };
    std::vector<int32_t> expected = {4, 4};

    BaseVector* inputVec = FlinkDayOfWeekTestHelper::CreateIntVector(dateValues);
    BaseVector* resultVec = nullptr;
    FlinkDayOfWeekTestHelper::ExecuteFlinkDayOfWeek(inputVec, OMNI_INT, resultVec);
    FlinkDayOfWeekTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

// ============================================================================
// OMNI_LONG (Flink TIMESTAMP = milliseconds since epoch) — no session tz (DOW)
// ============================================================================

TEST(FlinkDayOfWeekTest, LongBasicMillis) {
    std::cout << "=== Test: flink_dayofweek from LONG (millis) - basic ===" << std::endl;

    // Flink test reference: DAYOFWEEK(f18) = 1 for TIMESTAMP '1996-11-10 06:55:44' (Sun).
    // Also the sql_functions.yml example date as a timestamp: 1994-09-27 (Tue) -> 3.
    std::vector<int64_t> millisValues = {
        FlinkDayOfWeekTestHelper::TimestampToMillisUtc(1994, 9, 27, 0, 0, 0),
        FlinkDayOfWeekTestHelper::TimestampToMillisUtc(1996, 11, 10, 6, 55, 44)
    };
    std::vector<int32_t> expected = {3, 1};

    BaseVector* inputVec = FlinkDayOfWeekTestHelper::CreateLongVector(millisValues);
    BaseVector* resultVec = nullptr;
    FlinkDayOfWeekTestHelper::ExecuteFlinkDayOfWeek(inputVec, OMNI_LONG, resultVec);
    FlinkDayOfWeekTestHelper::ValidateResult(resultVec, expected, millisValues.size());

    delete resultVec;
}

TEST(FlinkDayOfWeekTest, LongWeekBoundary) {
    // Wall-clock week boundary (no session tz):
    //   2024-03-16 23:59:59 (Sat) -> DOW 7; 2024-03-17 00:00:00 (Sun) -> DOW 1.
    std::vector<int64_t> millisValues = {
        FlinkDayOfWeekTestHelper::TimestampToMillisUtc(2024, 3, 16, 23, 59, 59),
        FlinkDayOfWeekTestHelper::TimestampToMillisUtc(2024, 3, 17, 0, 0, 0)
    };
    std::vector<int32_t> expected = {7, 1};

    BaseVector* inputVec = FlinkDayOfWeekTestHelper::CreateLongVector(millisValues);
    BaseVector* resultVec = nullptr;
    FlinkDayOfWeekTestHelper::ExecuteFlinkDayOfWeek(inputVec, OMNI_LONG, resultVec);
    FlinkDayOfWeekTestHelper::ValidateResult(resultVec, expected, millisValues.size());

    delete resultVec;
}

TEST(FlinkDayOfWeekTest, LongPreEpochNegativeMillis) {
    // Negative millis (before 1970): 1969-12-31 23:59:59 (Wed) -> DOW 4.
    // Timestamp::fromMillis handles negative millis via floor division.
    std::vector<int64_t> millisValues = {
        FlinkDayOfWeekTestHelper::TimestampToMillisUtc(1969, 12, 31, 23, 59, 59)
    };
    std::vector<int32_t> expected = {4};

    BaseVector* inputVec = FlinkDayOfWeekTestHelper::CreateLongVector(millisValues);
    BaseVector* resultVec = nullptr;
    FlinkDayOfWeekTestHelper::ExecuteFlinkDayOfWeek(inputVec, OMNI_LONG, resultVec);
    FlinkDayOfWeekTestHelper::ValidateResult(resultVec, expected, millisValues.size());

    delete resultVec;
}

TEST(FlinkDayOfWeekTest, LongWithNull) {
    std::cout << "=== Test: flink_dayofweek from LONG with NULL ===" << std::endl;

    std::vector<int64_t> millisValues = {
        FlinkDayOfWeekTestHelper::TimestampToMillisUtc(2024, 1, 1, 8, 0, 0),
        FlinkDayOfWeekTestHelper::TimestampToMillisUtc(2024, 6, 20, 12, 0, 0),
        FlinkDayOfWeekTestHelper::TimestampToMillisUtc(2024, 12, 31, 0, 0, 0)
    };

    BaseVector* inputVec = FlinkDayOfWeekTestHelper::CreateLongVector(millisValues);
    inputVec->SetNull(1);

    BaseVector* resultVec = nullptr;
    FlinkDayOfWeekTestHelper::ExecuteFlinkDayOfWeek(inputVec, OMNI_LONG, resultVec);

    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";

    auto* resultVecTyped = dynamic_cast<Vector<int32_t>*>(resultVec);
    EXPECT_EQ(resultVecTyped->GetValue(0), 2) << "Row 0 DOW should be 2 (2024-01-01 Mon)";

    delete resultVec;
}

TEST(FlinkDayOfWeekTest, LongEpoch) {
    // Epoch instant: 1970-01-01 00:00:00 (Thu) -> DOW 5 (Sun=1, Thu=5).
    std::vector<int64_t> millisValues = {
        FlinkDayOfWeekTestHelper::TimestampToMillisUtc(1970, 1, 1, 0, 0, 0)
    };
    std::vector<int32_t> expected = {5};

    BaseVector* inputVec = FlinkDayOfWeekTestHelper::CreateLongVector(millisValues);
    BaseVector* resultVec = nullptr;
    FlinkDayOfWeekTestHelper::ExecuteFlinkDayOfWeek(inputVec, OMNI_LONG, resultVec);
    FlinkDayOfWeekTestHelper::ValidateResult(resultVec, expected, millisValues.size());

    delete resultVec;
}

TEST(FlinkDayOfWeekTest, LongLargeYear) {
    // Large year: 9999-12-31 23:59:59 (Fri) -> DOW 6.
    std::vector<int64_t> millisValues = {
        FlinkDayOfWeekTestHelper::TimestampToMillisUtc(9999, 12, 31, 23, 59, 59)
    };
    std::vector<int32_t> expected = {6};

    BaseVector* inputVec = FlinkDayOfWeekTestHelper::CreateLongVector(millisValues);
    BaseVector* resultVec = nullptr;
    FlinkDayOfWeekTestHelper::ExecuteFlinkDayOfWeek(inputVec, OMNI_LONG, resultVec);
    FlinkDayOfWeekTestHelper::ValidateResult(resultVec, expected, millisValues.size());

    delete resultVec;
}

TEST(FlinkDayOfWeekTest, LongMillisSemanticsVsMicros) {
    // Anti-regression: prove the OMNI_LONG path interprets the value as
    // MILLIS, not micros. 2024-06-01 00:00:00 UTC = 1717200000 seconds =
    // 1717200000000 millis -> Saturday -> DOW 7. If the impl wrongly used
    // fromMicros, the value would be treated as 1000x smaller and fall in
    // 1970-01-20 (Tuesday) -> DOW 3, a clearly different result.
    int64_t millis = FlinkDayOfWeekTestHelper::TimestampToMillisUtc(2024, 6, 1, 0, 0, 0);
    ASSERT_EQ(millis, 1717200000000);

    std::vector<int64_t> millisValues = {millis};
    BaseVector* inputVec = FlinkDayOfWeekTestHelper::CreateLongVector(millisValues);
    BaseVector* resultVec = nullptr;
    FlinkDayOfWeekTestHelper::ExecuteFlinkDayOfWeek(inputVec, OMNI_LONG, resultVec);

    ASSERT_NE(resultVec, nullptr);
    EXPECT_FALSE(resultVec->IsNull(0));
    auto* resultVecTyped = dynamic_cast<Vector<int32_t>*>(resultVec);
    // Must be 7 (2024-06-01 Sat, millis semantics). A buggy fromMicros path would give 3.
    EXPECT_EQ(resultVecTyped->GetValue(0), 7);

    delete resultVec;
}

// ============================================================================
// Multi-row batch mixing timestamps across several weekdays
// ============================================================================

TEST(FlinkDayOfWeekTest, MultiRowBatchLong) {
    std::vector<int64_t> millisValues = {
        FlinkDayOfWeekTestHelper::TimestampToMillisUtc(1970, 1, 1, 0, 0, 0),    // epoch (Thu) -> 5
        FlinkDayOfWeekTestHelper::TimestampToMillisUtc(2024, 5, 15, 0, 0, 0),   // Wed -> 4
        FlinkDayOfWeekTestHelper::TimestampToMillisUtc(2024, 8, 20, 0, 0, 0),   // Tue -> 3
        FlinkDayOfWeekTestHelper::TimestampToMillisUtc(1996, 11, 10, 6, 55, 44) // Sun -> 1
    };
    std::vector<int32_t> expected = {5, 4, 3, 1};

    BaseVector* inputVec = FlinkDayOfWeekTestHelper::CreateLongVector(millisValues);
    BaseVector* resultVec = nullptr;
    FlinkDayOfWeekTestHelper::ExecuteFlinkDayOfWeek(inputVec, OMNI_LONG, resultVec);
    FlinkDayOfWeekTestHelper::ValidateResult(resultVec, expected, millisValues.size());

    delete resultVec;
}
