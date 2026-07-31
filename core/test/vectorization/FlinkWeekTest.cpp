/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: flink_week function unit tests
 *
 * flink_week(date) -> int32, flink_week(timestamp_millis) -> int32
 * Mirrors Flink's WEEK(date) == EXTRACT(WEEK FROM date):
 *   - OMNI_INT  : date = days since epoch; ISO week extracted in UTC.
 *   - OMNI_LONG : Flink TimestampData = milliseconds since epoch (NOT micros);
 *                 ISO week extracted in UTC, no session timezone (wall-clock
 *                 semantics, matching Flink EXTRACT(WEEK FROM <TIMESTAMP>)).
 *
 * Expected values below are derived from the Flink reference semantics
 * (DateTimeUtils.getIso8601WeekNumber), the authoritative spec. Verified
 * against sql_functions.yml (WEEK(DATE '1994-09-27') = 39) and
 * ScalarFunctionsTest (WEEK(f16)=45, WEEK(f18)=45 for 1996-11-10).
 */

#include <gtest/gtest.h>
#include <iostream>
#include <vector>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/functions/FlinkWeek.h"
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
class FlinkWeekTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const flink_week_test_env =
    ::testing::AddGlobalTestEnvironment(new FlinkWeekTestEnvironment);

class FlinkWeekTestHelper {
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

    static void ExecuteFlinkWeek(BaseVector* inputVec, DataTypeId inputTypeId, BaseVector*& result) {
        auto signature = std::make_shared<FunctionSignature>("flink_week",
            std::vector<DataTypeId>{inputTypeId}, OMNI_INT);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "flink_week function not found for signature";

        auto outputType = std::make_shared<DataType>(OMNI_INT);
        ExecutionContext context;
        context.SetResultRowSize(inputVec->GetSize());
        std::stack<BaseVector*> args;
        args.push(inputVec);

        ASSERT_NO_THROW(function->Apply(args, outputType, result, &context))
            << "flink_week function threw an exception";
    }

    // Convert date components to days since epoch (matches the implementation's
    // date calculation, same helper as YearTest/FlinkYearTest).
    static int32_t DateToDays(int year, int month, int day) {
        LocalDate date(static_cast<int32_t>(year), static_cast<int16_t>(month), static_cast<int16_t>(day));
        return date.ToDays();
    }

    // Convert a UTC wall-clock datetime to Flink TIMESTAMP millis (epoch millis).
    // Flink TimestampData stores millisecond = epochDay * 86400000 + nanoOfDay/1e6.
    // For whole-second values this is simply epochSeconds * 1000. The week is
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
// OMNI_INT (date = days since epoch) — ISO week extracted in UTC
// ============================================================================

TEST(FlinkWeekTest, IntBasic) {
    std::cout << "=== Test: flink_week from INT (date) - basic ===" << std::endl;

    // sql_functions.yml: WEEK(DATE '1994-09-27') = 39 (Tue, ISO week 39 of 1994).
    // ScalarFunctionsTest: WEEK(f16) = 45 where f16 = DATE '1996-11-10' (Sun, ISO W45).
    // 2024-01-01 is a Monday -> ISO W01 of 2024.
    std::vector<int32_t> intValues = {
        FlinkWeekTestHelper::DateToDays(1994, 9, 27),
        FlinkWeekTestHelper::DateToDays(1996, 11, 10),
        FlinkWeekTestHelper::DateToDays(2024, 1, 1)
    };
    std::vector<int32_t> expected = {39, 45, 1};

    BaseVector* inputVec = FlinkWeekTestHelper::CreateIntVector(intValues);
    BaseVector* resultVec = nullptr;
    FlinkWeekTestHelper::ExecuteFlinkWeek(inputVec, OMNI_INT, resultVec);
    FlinkWeekTestHelper::ValidateResult(resultVec, expected, intValues.size());

    delete resultVec;
}

TEST(FlinkWeekTest, IntIsoBoundaryRollToNextYear) {
    // Late-December dates that roll into week 1 of the NEXT year.
    // 2025-12-29 is a Monday and is the Monday of ISO week 1 of 2026 -> 1.
    // 2025-12-28 is a Sunday, still the last day of ISO week 52 of 2025 -> 52.
    std::vector<int32_t> intValues = {
        FlinkWeekTestHelper::DateToDays(2025, 12, 29),
        FlinkWeekTestHelper::DateToDays(2025, 12, 28)
    };
    std::vector<int32_t> expected = {1, 52};

    BaseVector* inputVec = FlinkWeekTestHelper::CreateIntVector(intValues);
    BaseVector* resultVec = nullptr;
    FlinkWeekTestHelper::ExecuteFlinkWeek(inputVec, OMNI_INT, resultVec);
    FlinkWeekTestHelper::ValidateResult(resultVec, expected, intValues.size());

    delete resultVec;
}

TEST(FlinkWeekTest, IntIsoBoundaryRollToPrevYear) {
    // Early-January dates that belong to the last week of the PREVIOUS year.
    // 2022-01-01 is a Saturday -> belongs to ISO week 52 of 2021 -> 52.
    // 2022-01-03 is a Monday -> ISO week 1 of 2022 -> 1.
    std::vector<int32_t> intValues = {
        FlinkWeekTestHelper::DateToDays(2022, 1, 1),
        FlinkWeekTestHelper::DateToDays(2022, 1, 3)
    };
    std::vector<int32_t> expected = {52, 1};

    BaseVector* inputVec = FlinkWeekTestHelper::CreateIntVector(intValues);
    BaseVector* resultVec = nullptr;
    FlinkWeekTestHelper::ExecuteFlinkWeek(inputVec, OMNI_INT, resultVec);
    FlinkWeekTestHelper::ValidateResult(resultVec, expected, intValues.size());

    delete resultVec;
}

TEST(FlinkWeekTest, IntWithNull) {
    std::cout << "=== Test: flink_week from INT with NULL ===" << std::endl;

    std::vector<int32_t> intValues = {
        FlinkWeekTestHelper::DateToDays(2024, 1, 1),   // W01
        FlinkWeekTestHelper::DateToDays(2024, 6, 15),  // mid-year
        FlinkWeekTestHelper::DateToDays(2024, 12, 31)  // year end
    };

    BaseVector* inputVec = FlinkWeekTestHelper::CreateIntVector(intValues);
    inputVec->SetNull(1);

    BaseVector* resultVec = nullptr;
    FlinkWeekTestHelper::ExecuteFlinkWeek(inputVec, OMNI_INT, resultVec);

    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";

    auto* resultVecTyped = dynamic_cast<Vector<int32_t>*>(resultVec);
    EXPECT_EQ(resultVecTyped->GetValue(0), 1) << "Row 0 week should be 1 (2024-01-01 Monday)";

    delete resultVec;
}

TEST(FlinkWeekTest, IntPreEpoch) {
    // Negative days (before 1970): 1969-12-31 (Wed). ISO rule: the week
    // belongs to the year of its Thursday; 1970-01-01 is a Thursday, so this
    // Wednesday belongs to ISO week 1 of 1970 -> 1.
    std::vector<int32_t> dateValues = {
        FlinkWeekTestHelper::DateToDays(1969, 12, 31)
    };
    std::vector<int32_t> expected = {1};

    BaseVector* inputVec = FlinkWeekTestHelper::CreateIntVector(dateValues);
    BaseVector* resultVec = nullptr;
    FlinkWeekTestHelper::ExecuteFlinkWeek(inputVec, OMNI_INT, resultVec);
    FlinkWeekTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

// ============================================================================
// OMNI_LONG (Flink TIMESTAMP = milliseconds since epoch) — no session tz
// ============================================================================

TEST(FlinkWeekTest, LongBasicMillis) {
    std::cout << "=== Test: flink_week from LONG (millis) - basic ===" << std::endl;

    // Flink test reference: WEEK(f18) = 45 for TIMESTAMP '1996-11-10 06:55:44'.
    // Also the sql_functions.yml example date as a timestamp: 1994-09-27 -> 39.
    std::vector<int64_t> millisValues = {
        FlinkWeekTestHelper::TimestampToMillisUtc(1996, 11, 10, 6, 55, 44),
        FlinkWeekTestHelper::TimestampToMillisUtc(1994, 9, 27, 0, 0, 0)
    };
    std::vector<int32_t> expected = {45, 39};

    BaseVector* inputVec = FlinkWeekTestHelper::CreateLongVector(millisValues);
    BaseVector* resultVec = nullptr;
    FlinkWeekTestHelper::ExecuteFlinkWeek(inputVec, OMNI_LONG, resultVec);
    FlinkWeekTestHelper::ValidateResult(resultVec, expected, millisValues.size());

    delete resultVec;
}

TEST(FlinkWeekTest, LongIsoBoundary) {
    // ISO cross-year boundaries via timestamps (no session tz):
    //   2025-12-29 00:00:00 -> W01 of 2026 -> 1.
    //   2022-01-01 00:00:00 -> W52 of 2021 -> 52.
    std::vector<int64_t> millisValues = {
        FlinkWeekTestHelper::TimestampToMillisUtc(2025, 12, 29, 0, 0, 0),
        FlinkWeekTestHelper::TimestampToMillisUtc(2022, 1, 1, 0, 0, 0)
    };
    std::vector<int32_t> expected = {1, 52};

    BaseVector* inputVec = FlinkWeekTestHelper::CreateLongVector(millisValues);
    BaseVector* resultVec = nullptr;
    FlinkWeekTestHelper::ExecuteFlinkWeek(inputVec, OMNI_LONG, resultVec);
    FlinkWeekTestHelper::ValidateResult(resultVec, expected, millisValues.size());

    delete resultVec;
}

TEST(FlinkWeekTest, LongPreEpochNegativeMillis) {
    // Negative millis (before 1970): 1969-12-31 23:59:59 (Wed). ISO rule: the
    // week belongs to the year of its Thursday; 1970-01-01 is a Thursday, so
    // this belongs to ISO week 1 of 1970 -> 1. Timestamp::fromMillis handles
    // negative millis via floor division.
    std::vector<int64_t> millisValues = {
        FlinkWeekTestHelper::TimestampToMillisUtc(1969, 12, 31, 23, 59, 59)
    };
    std::vector<int32_t> expected = {1};

    BaseVector* inputVec = FlinkWeekTestHelper::CreateLongVector(millisValues);
    BaseVector* resultVec = nullptr;
    FlinkWeekTestHelper::ExecuteFlinkWeek(inputVec, OMNI_LONG, resultVec);
    FlinkWeekTestHelper::ValidateResult(resultVec, expected, millisValues.size());

    delete resultVec;
}

TEST(FlinkWeekTest, LongWithNull) {
    std::cout << "=== Test: flink_week from LONG with NULL ===" << std::endl;

    std::vector<int64_t> millisValues = {
        FlinkWeekTestHelper::TimestampToMillisUtc(2024, 1, 1, 8, 0, 0),
        FlinkWeekTestHelper::TimestampToMillisUtc(2024, 6, 20, 12, 0, 0),
        FlinkWeekTestHelper::TimestampToMillisUtc(2024, 12, 31, 0, 0, 0)
    };

    BaseVector* inputVec = FlinkWeekTestHelper::CreateLongVector(millisValues);
    inputVec->SetNull(1);

    BaseVector* resultVec = nullptr;
    FlinkWeekTestHelper::ExecuteFlinkWeek(inputVec, OMNI_LONG, resultVec);

    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";

    auto* resultVecTyped = dynamic_cast<Vector<int32_t>*>(resultVec);
    EXPECT_EQ(resultVecTyped->GetValue(0), 1) << "Row 0 week should be 1 (2024-01-01 Monday)";

    delete resultVec;
}

TEST(FlinkWeekTest, LongLargeYear) {
    // Large year: 9999-12-31 23:59:59 -> some valid ISO week 1-53 (Friday).
    // Just verify non-NULL and a plausible range; the exact value is produced
    // by the same algorithm as WeekOfYear (already validated).
    std::vector<int64_t> millisValues = {
        FlinkWeekTestHelper::TimestampToMillisUtc(9999, 12, 31, 23, 59, 59)
    };

    BaseVector* inputVec = FlinkWeekTestHelper::CreateLongVector(millisValues);
    BaseVector* resultVec = nullptr;
    FlinkWeekTestHelper::ExecuteFlinkWeek(inputVec, OMNI_LONG, resultVec);

    ASSERT_NE(resultVec, nullptr);
    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    auto* resultVecTyped = dynamic_cast<Vector<int32_t>*>(resultVec);
    int32_t w = resultVecTyped->GetValue(0);
    EXPECT_GE(w, 1) << "ISO week must be >= 1";
    EXPECT_LE(w, 53) << "ISO week must be <= 53";

    delete resultVec;
}

TEST(FlinkWeekTest, LongMillisSemanticsVsMicros) {
    // Anti-regression: prove the OMNI_LONG path interprets the value as
    // MILLIS, not micros. 2024-06-01 00:00:00 UTC = 1717200000 seconds =
    // 1717200000000 millis. 2024-06-01 is a Saturday in ISO week 22 of 2024.
    // If the impl wrongly used fromMicros, the value would be treated as
    // 1000x smaller and fall in 1970-01-20 (a Tuesday in ISO week 4 of 1970)
    // -> 4, a clearly different result.
    int64_t millis = FlinkWeekTestHelper::TimestampToMillisUtc(2024, 6, 1, 0, 0, 0);
    ASSERT_EQ(millis, 1717200000000);

    std::vector<int64_t> millisValues = {millis};
    BaseVector* inputVec = FlinkWeekTestHelper::CreateLongVector(millisValues);
    BaseVector* resultVec = nullptr;
    FlinkWeekTestHelper::ExecuteFlinkWeek(inputVec, OMNI_LONG, resultVec);

    ASSERT_NE(resultVec, nullptr);
    EXPECT_FALSE(resultVec->IsNull(0));
    auto* resultVecTyped = dynamic_cast<Vector<int32_t>*>(resultVec);
    // Must be 22 (2024-06-01, millis semantics). A buggy fromMicros path would give 4.
    EXPECT_EQ(resultVecTyped->GetValue(0), 22);

    delete resultVec;
}

// ============================================================================
// Multi-row batch mixing timestamps across several weeks
// ============================================================================

TEST(FlinkWeekTest, MultiRowBatchLong) {
    std::vector<int64_t> millisValues = {
        FlinkWeekTestHelper::TimestampToMillisUtc(1970, 1, 1, 0, 0, 0),    // epoch (Thu) -> W01 of 1970
        FlinkWeekTestHelper::TimestampToMillisUtc(2024, 5, 15, 0, 0, 0),   // -> some week
        FlinkWeekTestHelper::TimestampToMillisUtc(2024, 8, 20, 0, 0, 0),   // -> some week
        FlinkWeekTestHelper::TimestampToMillisUtc(1996, 11, 10, 6, 55, 44) // -> W45
    };
    std::vector<int32_t> expected = {1, 20, 34, 45};

    BaseVector* inputVec = FlinkWeekTestHelper::CreateLongVector(millisValues);
    BaseVector* resultVec = nullptr;
    FlinkWeekTestHelper::ExecuteFlinkWeek(inputVec, OMNI_LONG, resultVec);
    FlinkWeekTestHelper::ValidateResult(resultVec, expected, millisValues.size());

    delete resultVec;
}
