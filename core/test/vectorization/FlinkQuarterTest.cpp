/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: flink_quarter function unit tests
 *
 * flink_quarter(date) -> int32, flink_quarter(timestamp_millis) -> int32
 * Mirrors Flink's QUARTER(date) == EXTRACT(QUARTER FROM date):
 *   - OMNI_INT  : date = days since epoch; quarter extracted in UTC.
 *   - OMNI_LONG : Flink TimestampData = milliseconds since epoch (NOT micros);
 *                 quarter extracted in UTC, no session timezone (wall-clock
 *                 semantics, matching Flink EXTRACT(QUARTER FROM <TIMESTAMP>)).
 *
 * Expected values below are derived from the Flink reference semantics
 * (DateTimeUtils.julianExtract QUARTER = (month + 2) / 3), the authoritative
 * spec. Verified against sql_functions.yml (QUARTER(DATE '1994-09-27') = 3)
 * and ScalarFunctionsTest (QUARTER(f18)=4 for TIMESTAMP '1996-11-10').
 */

#include <gtest/gtest.h>
#include <iostream>
#include <vector>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/functions/FlinkQuarter.h"
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
class FlinkQuarterTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const flink_quarter_test_env =
    ::testing::AddGlobalTestEnvironment(new FlinkQuarterTestEnvironment);

class FlinkQuarterTestHelper {
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

    static void ExecuteFlinkQuarter(BaseVector* inputVec, DataTypeId inputTypeId, BaseVector*& result) {
        auto signature = std::make_shared<FunctionSignature>("flink_quarter",
            std::vector<DataTypeId>{inputTypeId}, OMNI_INT);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "flink_quarter function not found for signature";

        auto outputType = std::make_shared<DataType>(OMNI_INT);
        ExecutionContext context;
        context.SetResultRowSize(inputVec->GetSize());
        std::stack<BaseVector*> args;
        args.push(inputVec);

        ASSERT_NO_THROW(function->Apply(args, outputType, result, &context))
            << "flink_quarter function threw an exception";
    }
    // Create a const-style VARCHAR vector holding the same zone-id string for
    // every row (the OmniAdaptor passes the session zone-id as a literal).
    static BaseVector* CreateConstStringVector(const std::string& value, int32_t size) {
        BaseVector* vec = VectorHelper::CreateFlatVector(OMNI_VARCHAR, size);
        auto* typedVec = static_cast<Vector<LargeStringContainer<std::string_view>>*>(vec);
        std::string_view sv(value.data(), value.size());
        for (int32_t i = 0; i < size; ++i) {
            typedVec->SetValue(i, sv);
        }
        return vec;
    }

    // Execute flink_quarter_with_tz(inputMillis, zoneId) -> int32. The zone-id is
    // applied when decomposing the millis into wall-clock fields (for
    // TIMESTAMP_WITH_LOCAL_TIME_ZONE input on the Java side).
    static void ExecuteFlinkQuarterWithTz(BaseVector* inputVec, BaseVector* tzVec, BaseVector*& result) {
        auto signature = std::make_shared<FunctionSignature>("flink_quarter_with_tz",
            std::vector<DataTypeId>{OMNI_LONG, OMNI_VARCHAR}, OMNI_INT);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "flink_quarter_with_tz function not found for signature";

        auto outputType = std::make_shared<DataType>(OMNI_INT);
        ExecutionContext context;
        context.SetResultRowSize(inputVec->GetSize());
        std::stack<BaseVector*> args;
        args.push(inputVec);
        args.push(tzVec);

        ASSERT_NO_THROW(function->Apply(args, outputType, result, &context))
            << "flink_quarter_with_tz function threw an exception";
    }


    // Convert date components to days since epoch (matches the implementation's
    // date calculation, same helper as YearTest/FlinkYearTest).
    static int32_t DateToDays(int year, int month, int day) {
        LocalDate date(static_cast<int32_t>(year), static_cast<int16_t>(month), static_cast<int16_t>(day));
        return date.ToDays();
    }

    // Convert a UTC wall-clock datetime to Flink TIMESTAMP millis (epoch millis).
    // Flink TimestampData stores millisecond = epochDay * 86400000 + nanoOfDay/1e6.
    // For whole-second values this is simply epochSeconds * 1000. The quarter is
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
// OMNI_INT (date = days since epoch) — quarter extracted in UTC
// ============================================================================

TEST(FlinkQuarterTest, IntBasic) {
    std::cout << "=== Test: flink_quarter from INT (date) - basic ===" << std::endl;

    // sql_functions.yml: QUARTER(DATE '1994-09-27') = 3 (Sep -> Q3)
    // ScalarFunctionsTest: QUARTER(DATE '1997-01-27')=1, '1997-04-27'=2,
    //   '1997-12-31'=4; QUARTER(f16)=4 where f16 = DATE '1996-11-10'.
    std::vector<int32_t> intValues = {
        FlinkQuarterTestHelper::DateToDays(1994, 9, 27),
        FlinkQuarterTestHelper::DateToDays(1997, 1, 27),
        FlinkQuarterTestHelper::DateToDays(1997, 4, 27),
        FlinkQuarterTestHelper::DateToDays(1997, 12, 31),
        FlinkQuarterTestHelper::DateToDays(1996, 11, 10)
    };
    std::vector<int32_t> expected = {3, 1, 2, 4, 4};

    BaseVector* inputVec = FlinkQuarterTestHelper::CreateIntVector(intValues);
    BaseVector* resultVec = nullptr;
    FlinkQuarterTestHelper::ExecuteFlinkQuarter(inputVec, OMNI_INT, resultVec);
    FlinkQuarterTestHelper::ValidateResult(resultVec, expected, intValues.size());

    delete resultVec;
}

TEST(FlinkQuarterTest, IntAllMonths) {
    // Cover all 12 months: quarters 1,1,1,2,2,2,3,3,3,4,4,4. Uses the 15th of
    // each month in 2024 (a leap year) — confirms day-of-month is irrelevant.
    std::vector<int32_t> dateValues;
    std::vector<int32_t> expected;
    for (int m = 1; m <= 12; ++m) {
        dateValues.push_back(FlinkQuarterTestHelper::DateToDays(2024, m, 15));
        expected.push_back((m - 1) / 3 + 1); // == Flink's (m + 2) / 3
    }

    BaseVector* inputVec = FlinkQuarterTestHelper::CreateIntVector(dateValues);
    BaseVector* resultVec = nullptr;
    FlinkQuarterTestHelper::ExecuteFlinkQuarter(inputVec, OMNI_INT, resultVec);
    FlinkQuarterTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

TEST(FlinkQuarterTest, IntWithNull) {
    std::cout << "=== Test: flink_quarter from INT with NULL ===" << std::endl;

    std::vector<int32_t> intValues = {
        FlinkQuarterTestHelper::DateToDays(2024, 1, 15),
        FlinkQuarterTestHelper::DateToDays(2024, 6, 20),
        FlinkQuarterTestHelper::DateToDays(2024, 12, 31)
    };

    BaseVector* inputVec = FlinkQuarterTestHelper::CreateIntVector(intValues);
    inputVec->SetNull(1);

    BaseVector* resultVec = nullptr;
    FlinkQuarterTestHelper::ExecuteFlinkQuarter(inputVec, OMNI_INT, resultVec);

    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";

    auto* resultVecTyped = dynamic_cast<Vector<int32_t>*>(resultVec);
    EXPECT_EQ(resultVecTyped->GetValue(0), 1) << "Row 0 quarter should be 1 (Jan)";
    EXPECT_EQ(resultVecTyped->GetValue(2), 4) << "Row 2 quarter should be 4 (Dec)";

    delete resultVec;
}

TEST(FlinkQuarterTest, IntPreEpoch) {
    // Negative days (before 1970): 1969-12-31 -> Dec -> Q4.
    std::vector<int32_t> dateValues = {
        FlinkQuarterTestHelper::DateToDays(1969, 12, 31),
        FlinkQuarterTestHelper::DateToDays(1969, 1, 1)
    };
    std::vector<int32_t> expected = {4, 1};

    BaseVector* inputVec = FlinkQuarterTestHelper::CreateIntVector(dateValues);
    BaseVector* resultVec = nullptr;
    FlinkQuarterTestHelper::ExecuteFlinkQuarter(inputVec, OMNI_INT, resultVec);
    FlinkQuarterTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

TEST(FlinkQuarterTest, IntLeapDay) {
    // Leap day (2020-02-29): month=Feb -> Q1 regardless of leap day.
    std::vector<int32_t> dateValues = {
        FlinkQuarterTestHelper::DateToDays(2020, 2, 29),  // leap day -> Q1
        FlinkQuarterTestHelper::DateToDays(2024, 2, 29)   // leap day -> Q1
    };
    std::vector<int32_t> expected = {1, 1};

    BaseVector* inputVec = FlinkQuarterTestHelper::CreateIntVector(dateValues);
    BaseVector* resultVec = nullptr;
    FlinkQuarterTestHelper::ExecuteFlinkQuarter(inputVec, OMNI_INT, resultVec);
    FlinkQuarterTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

// ============================================================================
// OMNI_LONG (Flink TIMESTAMP = milliseconds since epoch) — no session tz
// ============================================================================

TEST(FlinkQuarterTest, LongBasicMillis) {
    std::cout << "=== Test: flink_quarter from LONG (millis) - basic ===" << std::endl;

    // Flink test reference: QUARTER(f18) = 4 for TIMESTAMP '1996-11-10 06:55:44'.
    // Also the sql_functions.yml example date as a timestamp: 1994-09-27 -> 3.
    std::vector<int64_t> millisValues = {
        FlinkQuarterTestHelper::TimestampToMillisUtc(1994, 9, 27, 0, 0, 0),
        FlinkQuarterTestHelper::TimestampToMillisUtc(1996, 11, 10, 6, 55, 44)
    };
    std::vector<int32_t> expected = {3, 4};

    BaseVector* inputVec = FlinkQuarterTestHelper::CreateLongVector(millisValues);
    BaseVector* resultVec = nullptr;
    FlinkQuarterTestHelper::ExecuteFlinkQuarter(inputVec, OMNI_LONG, resultVec);
    FlinkQuarterTestHelper::ValidateResult(resultVec, expected, millisValues.size());

    delete resultVec;
}

TEST(FlinkQuarterTest, LongQuarterBoundary) {
    // Wall-clock quarter boundaries (no session tz):
    //   2024-03-31 23:59:59 -> Q1; 2024-04-01 00:00:00 -> Q2.
    //   2024-12-31 23:59:59 -> Q4; 2025-01-01 00:00:00 -> Q1.
    std::vector<int64_t> millisValues = {
        FlinkQuarterTestHelper::TimestampToMillisUtc(2024, 3, 31, 23, 59, 59),
        FlinkQuarterTestHelper::TimestampToMillisUtc(2024, 4, 1, 0, 0, 0),
        FlinkQuarterTestHelper::TimestampToMillisUtc(2024, 12, 31, 23, 59, 59),
        FlinkQuarterTestHelper::TimestampToMillisUtc(2025, 1, 1, 0, 0, 0)
    };
    std::vector<int32_t> expected = {1, 2, 4, 1};

    BaseVector* inputVec = FlinkQuarterTestHelper::CreateLongVector(millisValues);
    BaseVector* resultVec = nullptr;
    FlinkQuarterTestHelper::ExecuteFlinkQuarter(inputVec, OMNI_LONG, resultVec);
    FlinkQuarterTestHelper::ValidateResult(resultVec, expected, millisValues.size());

    delete resultVec;
}

TEST(FlinkQuarterTest, LongPreEpochNegativeMillis) {
    // Negative millis (before 1970): 1969-12-31 23:59:59 -> Dec -> Q4.
    // Timestamp::fromMillis handles negative millis via floor division.
    std::vector<int64_t> millisValues = {
        FlinkQuarterTestHelper::TimestampToMillisUtc(1969, 12, 31, 23, 59, 59),
        FlinkQuarterTestHelper::TimestampToMillisUtc(1969, 1, 1, 0, 0, 0)
    };
    std::vector<int32_t> expected = {4, 1};

    BaseVector* inputVec = FlinkQuarterTestHelper::CreateLongVector(millisValues);
    BaseVector* resultVec = nullptr;
    FlinkQuarterTestHelper::ExecuteFlinkQuarter(inputVec, OMNI_LONG, resultVec);
    FlinkQuarterTestHelper::ValidateResult(resultVec, expected, millisValues.size());

    delete resultVec;
}

TEST(FlinkQuarterTest, LongWithNull) {
    std::cout << "=== Test: flink_quarter from LONG with NULL ===" << std::endl;

    std::vector<int64_t> millisValues = {
        FlinkQuarterTestHelper::TimestampToMillisUtc(2024, 1, 15, 8, 0, 0),
        FlinkQuarterTestHelper::TimestampToMillisUtc(2024, 6, 20, 12, 0, 0),
        FlinkQuarterTestHelper::TimestampToMillisUtc(2024, 12, 31, 0, 0, 0)
    };

    BaseVector* inputVec = FlinkQuarterTestHelper::CreateLongVector(millisValues);
    inputVec->SetNull(1);

    BaseVector* resultVec = nullptr;
    FlinkQuarterTestHelper::ExecuteFlinkQuarter(inputVec, OMNI_LONG, resultVec);

    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";

    auto* resultVecTyped = dynamic_cast<Vector<int32_t>*>(resultVec);
    EXPECT_EQ(resultVecTyped->GetValue(0), 1) << "Row 0 quarter should be 1 (Jan)";
    EXPECT_EQ(resultVecTyped->GetValue(2), 4) << "Row 2 quarter should be 4 (Dec)";

    delete resultVec;
}

TEST(FlinkQuarterTest, LongLargeYear) {
    // Large year: 9999-12-31 23:59:59 -> Dec -> Q4.
    std::vector<int64_t> millisValues = {
        FlinkQuarterTestHelper::TimestampToMillisUtc(9999, 12, 31, 23, 59, 59)
    };
    std::vector<int32_t> expected = {4};

    BaseVector* inputVec = FlinkQuarterTestHelper::CreateLongVector(millisValues);
    BaseVector* resultVec = nullptr;
    FlinkQuarterTestHelper::ExecuteFlinkQuarter(inputVec, OMNI_LONG, resultVec);
    FlinkQuarterTestHelper::ValidateResult(resultVec, expected, millisValues.size());

    delete resultVec;
}

TEST(FlinkQuarterTest, LongMillisSemanticsVsMicros) {
    // Anti-regression: prove the OMNI_LONG path interprets the value as
    // MILLIS, not micros. 2024-06-01 00:00:00 UTC = 1717200000 seconds =
    // 1717200000000 millis -> Jun -> Q2. If the impl wrongly used fromMicros,
    // the value would be treated as 1000x smaller and fall in 1970-01-20 ->
    // Jan -> Q1, a clearly different result.
    int64_t millis = FlinkQuarterTestHelper::TimestampToMillisUtc(2024, 6, 1, 0, 0, 0);
    ASSERT_EQ(millis, 1717200000000);

    std::vector<int64_t> millisValues = {millis};
    BaseVector* inputVec = FlinkQuarterTestHelper::CreateLongVector(millisValues);
    BaseVector* resultVec = nullptr;
    FlinkQuarterTestHelper::ExecuteFlinkQuarter(inputVec, OMNI_LONG, resultVec);

    ASSERT_NE(resultVec, nullptr);
    EXPECT_FALSE(resultVec->IsNull(0));
    auto* resultVecTyped = dynamic_cast<Vector<int32_t>*>(resultVec);
    // Must be 2 (Jun, millis semantics). A buggy fromMicros path would give 1.
    EXPECT_EQ(resultVecTyped->GetValue(0), 2);

    delete resultVec;
}

// ============================================================================
// Multi-row batch mixing timestamps across all four quarters
// ============================================================================

TEST(FlinkQuarterTest, MultiRowBatchLong) {
    std::vector<int64_t> millisValues = {
        FlinkQuarterTestHelper::TimestampToMillisUtc(1970, 1, 1, 0, 0, 0),    // epoch -> Q1
        FlinkQuarterTestHelper::TimestampToMillisUtc(2024, 5, 15, 0, 0, 0),   // -> Q2
        FlinkQuarterTestHelper::TimestampToMillisUtc(2024, 8, 20, 0, 0, 0),   // -> Q3
        FlinkQuarterTestHelper::TimestampToMillisUtc(2024, 11, 30, 0, 0, 0)   // -> Q4
    };
    std::vector<int32_t> expected = {1, 2, 3, 4};

    BaseVector* inputVec = FlinkQuarterTestHelper::CreateLongVector(millisValues);
    BaseVector* resultVec = nullptr;
    FlinkQuarterTestHelper::ExecuteFlinkQuarter(inputVec, OMNI_LONG, resultVec);
    FlinkQuarterTestHelper::ValidateResult(resultVec, expected, millisValues.size());

    delete resultVec;
}

// ============================================================================
// flink_quarter_with_tz — applies an explicit session timezone to the millis.
// The stored millis are a UTC instant; the zone shifts the wall-clock quarter.
// ============================================================================

TEST(FlinkQuarter, LongWithTzAsiaShanghai) {
    std::cout << "=== Test: flink_quarter_with_tz Asia/Shanghai (+8) ===" << std::endl;
    // Asia/Shanghai is UTC+8 (no DST): shifts the wall-clock +8h.
    std::vector<int64_t> millisValues = {
        FlinkQuarterTestHelper::TimestampToMillisUtc(1996, 11, 10, 6, 55, 44),
        FlinkQuarterTestHelper::TimestampToMillisUtc(2024, 1, 1, 2, 30, 0)
    };
    std::vector<int32_t> expected = {4, 1};

    BaseVector* inputVec = FlinkQuarterTestHelper::CreateLongVector(millisValues);
    BaseVector* tzVec = FlinkQuarterTestHelper::CreateConstStringVector("Asia/Shanghai", millisValues.size());
    BaseVector* resultVec = nullptr;
    FlinkQuarterTestHelper::ExecuteFlinkQuarterWithTz(inputVec, tzVec, resultVec);
    FlinkQuarterTestHelper::ValidateResult(resultVec, expected, millisValues.size());

    delete resultVec;
}

TEST(FlinkQuarter, LongWithTzUtcIsIdentity) {
    // "UTC" zone leaves the wall-clock unchanged.
    std::vector<int64_t> millisValues = {
        FlinkQuarterTestHelper::TimestampToMillisUtc(1996, 11, 10, 6, 55, 44),
        FlinkQuarterTestHelper::TimestampToMillisUtc(2024, 6, 1, 12, 0, 0)
    };
    std::vector<int32_t> expected = {4, 2};

    BaseVector* inputVec = FlinkQuarterTestHelper::CreateLongVector(millisValues);
    BaseVector* tzVec = FlinkQuarterTestHelper::CreateConstStringVector("UTC", millisValues.size());
    BaseVector* resultVec = nullptr;
    FlinkQuarterTestHelper::ExecuteFlinkQuarterWithTz(inputVec, tzVec, resultVec);
    FlinkQuarterTestHelper::ValidateResult(resultVec, expected, millisValues.size());

    delete resultVec;
}

TEST(FlinkQuarter, LongWithTzNegativeOffset) {
    // America/Los_Angeles: PST = UTC-8 (winter), PDT = UTC-7 (summer).
    std::vector<int64_t> millisValues = {
        FlinkQuarterTestHelper::TimestampToMillisUtc(2024, 1, 15, 16, 0, 0),
        FlinkQuarterTestHelper::TimestampToMillisUtc(2024, 7, 15, 16, 0, 0)
    };
    std::vector<int32_t> expected = {1, 3};

    BaseVector* inputVec = FlinkQuarterTestHelper::CreateLongVector(millisValues);
    BaseVector* tzVec = FlinkQuarterTestHelper::CreateConstStringVector("America/Los_Angeles", millisValues.size());
    BaseVector* resultVec = nullptr;
    FlinkQuarterTestHelper::ExecuteFlinkQuarterWithTz(inputVec, tzVec, resultVec);
    FlinkQuarterTestHelper::ValidateResult(resultVec, expected, millisValues.size());

    delete resultVec;
}

TEST(FlinkQuarter, LongWithTzCrossDayBoundary) {
    // 2024-07-16 06:00:00 UTC -> America/Los_Angeles (PDT -7) -> 2024-07-15 23:00.
    // Confirms the tz shift can roll the wall-clock across a day boundary.
    std::vector<int64_t> millisValues = {
        FlinkQuarterTestHelper::TimestampToMillisUtc(2024, 7, 16, 6, 0, 0)
    };
    std::vector<int32_t> expected = {3};

    BaseVector* inputVec = FlinkQuarterTestHelper::CreateLongVector(millisValues);
    BaseVector* tzVec = FlinkQuarterTestHelper::CreateConstStringVector("America/Los_Angeles", millisValues.size());
    BaseVector* resultVec = nullptr;
    FlinkQuarterTestHelper::ExecuteFlinkQuarterWithTz(inputVec, tzVec, resultVec);
    FlinkQuarterTestHelper::ValidateResult(resultVec, expected, millisValues.size());

    delete resultVec;
}
