/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: flink_year function unit tests
 *
 * flink_year(date) -> int32, flink_year(timestamp_millis) -> int32
 * Mirrors Flink's YEAR(date) == EXTRACT(YEAR FROM date):
 *   - OMNI_INT  : date = days since epoch; year extracted in UTC.
 *   - OMNI_LONG : Flink TimestampData = milliseconds since epoch (NOT micros);
 *                 year extracted in UTC, no session timezone (wall-clock
 *                 semantics, matching Flink EXTRACT(YEAR FROM <TIMESTAMP>)).
 *
 * Expected values below are derived from the Flink reference semantics
 * (DateTimeUtils.extractFromDate / ExtractCallGen), the authoritative spec.
 */

#include <gtest/gtest.h>
#include <iostream>
#include <vector>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/functions/FlinkYear.h"
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
class FlinkYearTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const flink_year_test_env =
    ::testing::AddGlobalTestEnvironment(new FlinkYearTestEnvironment);

class FlinkYearTestHelper {
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

    static void ExecuteFlinkYear(BaseVector* inputVec, DataTypeId inputTypeId, BaseVector*& result) {
        auto signature = std::make_shared<FunctionSignature>("flink_year",
            std::vector<DataTypeId>{inputTypeId}, OMNI_INT);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "flink_year function not found for signature";

        auto outputType = std::make_shared<DataType>(OMNI_INT);
        ExecutionContext context;
        context.SetResultRowSize(inputVec->GetSize());
        std::stack<BaseVector*> args;
        args.push(inputVec);

        ASSERT_NO_THROW(function->Apply(args, outputType, result, &context))
            << "flink_year function threw an exception";
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

    // Execute flink_year_with_tz(inputMillis, zoneId) -> int32. The zone-id is
    // applied when decomposing the millis into wall-clock fields (for
    // TIMESTAMP_WITH_LOCAL_TIME_ZONE input on the Java side).
    static void ExecuteFlinkYearWithTz(BaseVector* inputVec, BaseVector* tzVec, BaseVector*& result) {
        auto signature = std::make_shared<FunctionSignature>("flink_year_with_tz",
            std::vector<DataTypeId>{OMNI_LONG, OMNI_VARCHAR}, OMNI_INT);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "flink_year_with_tz function not found for signature";

        auto outputType = std::make_shared<DataType>(OMNI_INT);
        ExecutionContext context;
        context.SetResultRowSize(inputVec->GetSize());
        std::stack<BaseVector*> args;
        args.push(inputVec);
        args.push(tzVec);

        ASSERT_NO_THROW(function->Apply(args, outputType, result, &context))
            << "flink_year_with_tz function threw an exception";
    }


    // Convert date components to days since epoch (matches the implementation's
    // date calculation, same helper as YearTest).
    static int32_t DateToDays(int year, int month, int day) {
        LocalDate date(static_cast<int32_t>(year), static_cast<int16_t>(month), static_cast<int16_t>(day));
        return date.ToDays();
    }

    // Convert a UTC wall-clock datetime to Flink TIMESTAMP millis (epoch millis).
    // Flink TimestampData stores millisecond = epochDay * 86400000 + nanoOfDay/1e6.
    // For whole-second values this is simply epochSeconds * 1000. The year is
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
// OMNI_INT (date = days since epoch) — year extracted in UTC
// ============================================================================

TEST(FlinkYearTest, IntBasic) {
    std::cout << "=== Test: flink_year from INT (date) - basic ===" << std::endl;

    // 2024-01-15, 2023-06-20, 2025-12-31
    std::vector<int32_t> intValues = {
        FlinkYearTestHelper::DateToDays(2024, 1, 15),
        FlinkYearTestHelper::DateToDays(2023, 6, 20),
        FlinkYearTestHelper::DateToDays(2025, 12, 31)
    };
    std::vector<int32_t> expected = {2024, 2023, 2025};

    BaseVector* inputVec = FlinkYearTestHelper::CreateIntVector(intValues);
    BaseVector* resultVec = nullptr;
    FlinkYearTestHelper::ExecuteFlinkYear(inputVec, OMNI_INT, resultVec);
    FlinkYearTestHelper::ValidateResult(resultVec, expected, intValues.size());

    delete resultVec;
}

TEST(FlinkYearTest, IntWithNull) {
    std::cout << "=== Test: flink_year from INT with NULL ===" << std::endl;

    std::vector<int32_t> intValues = {
        FlinkYearTestHelper::DateToDays(2024, 1, 15),
        FlinkYearTestHelper::DateToDays(2023, 6, 20),
        FlinkYearTestHelper::DateToDays(2025, 12, 31)
    };

    BaseVector* inputVec = FlinkYearTestHelper::CreateIntVector(intValues);
    inputVec->SetNull(1);

    BaseVector* resultVec = nullptr;
    FlinkYearTestHelper::ExecuteFlinkYear(inputVec, OMNI_INT, resultVec);

    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";

    auto* resultVecTyped = dynamic_cast<Vector<int32_t>*>(resultVec);
    EXPECT_EQ(resultVecTyped->GetValue(0), 2024) << "Row 0 year should be 2024";
    EXPECT_EQ(resultVecTyped->GetValue(2), 2025) << "Row 2 year should be 2025";

    delete resultVec;
}

TEST(FlinkYearTest, IntLeapYearAndBoundary) {
    // Leap-year dates and year boundaries. Year extraction is independent of
    // whether the day is Feb-29; verify a spread of years incl. pre-2000.
    std::vector<int32_t> dateValues = {
        FlinkYearTestHelper::DateToDays(2020, 2, 29),  // leap day
        FlinkYearTestHelper::DateToDays(2021, 3, 15),
        FlinkYearTestHelper::DateToDays(2024, 2, 29),  // leap day
        FlinkYearTestHelper::DateToDays(2025, 1, 1),   // year start
        FlinkYearTestHelper::DateToDays(1999, 12, 31)  // pre-2000 year end
    };
    std::vector<int32_t> expected = {2020, 2021, 2024, 2025, 1999};

    BaseVector* inputVec = FlinkYearTestHelper::CreateIntVector(dateValues);
    BaseVector* resultVec = nullptr;
    FlinkYearTestHelper::ExecuteFlinkYear(inputVec, OMNI_INT, resultVec);
    FlinkYearTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

TEST(FlinkYearTest, IntPreEpoch) {
    // Negative days (before 1970): -1 day = 1969-12-31.
    std::vector<int32_t> dateValues = {
        FlinkYearTestHelper::DateToDays(1969, 12, 31),
        FlinkYearTestHelper::DateToDays(1900, 1, 1)
    };
    std::vector<int32_t> expected = {1969, 1900};

    BaseVector* inputVec = FlinkYearTestHelper::CreateIntVector(dateValues);
    BaseVector* resultVec = nullptr;
    FlinkYearTestHelper::ExecuteFlinkYear(inputVec, OMNI_INT, resultVec);
    FlinkYearTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

// ============================================================================
// OMNI_LONG (Flink TIMESTAMP = milliseconds since epoch) — no session tz
// ============================================================================

TEST(FlinkYearTest, LongBasicMillis) {
    std::cout << "=== Test: flink_year from LONG (millis) - basic ===" << std::endl;

    // Flink test reference: YEAR(f18) = 1996 for TIMESTAMP '1996-11-10 06:55:44'.
    // Also a 2024 timestamp. Inputs are epoch MILLIS (seconds * 1000).
    std::vector<int64_t> millisValues = {
        FlinkYearTestHelper::TimestampToMillisUtc(2024, 1, 15, 8, 0, 0),
        FlinkYearTestHelper::TimestampToMillisUtc(1996, 11, 10, 6, 55, 44)
    };
    std::vector<int32_t> expected = {2024, 1996};

    BaseVector* inputVec = FlinkYearTestHelper::CreateLongVector(millisValues);
    BaseVector* resultVec = nullptr;
    FlinkYearTestHelper::ExecuteFlinkYear(inputVec, OMNI_LONG, resultVec);
    FlinkYearTestHelper::ValidateResult(resultVec, expected, millisValues.size());

    delete resultVec;
}

TEST(FlinkYearTest, LongYearBoundary) {
    // Wall-clock year boundary: 2024-12-31 23:59:59 -> 2024;
    // 2025-01-01 00:00:00 -> 2025. (No session tz, so the stored wall-clock
    // millis directly determine the year.)
    std::vector<int64_t> millisValues = {
        FlinkYearTestHelper::TimestampToMillisUtc(2024, 12, 31, 23, 59, 59),
        FlinkYearTestHelper::TimestampToMillisUtc(2025, 1, 1, 0, 0, 0)
    };
    std::vector<int32_t> expected = {2024, 2025};

    BaseVector* inputVec = FlinkYearTestHelper::CreateLongVector(millisValues);
    BaseVector* resultVec = nullptr;
    FlinkYearTestHelper::ExecuteFlinkYear(inputVec, OMNI_LONG, resultVec);
    FlinkYearTestHelper::ValidateResult(resultVec, expected, millisValues.size());

    delete resultVec;
}

TEST(FlinkYearTest, LongPreEpochNegativeMillis) {
    // Negative millis (before 1970): 1969-12-31 23:59:59 -> 1969.
    // Timestamp::fromMillis handles negative millis via floor division.
    std::vector<int64_t> millisValues = {
        FlinkYearTestHelper::TimestampToMillisUtc(1969, 12, 31, 23, 59, 59),
        FlinkYearTestHelper::TimestampToMillisUtc(1900, 1, 1, 0, 0, 0)
    };
    std::vector<int32_t> expected = {1969, 1900};

    BaseVector* inputVec = FlinkYearTestHelper::CreateLongVector(millisValues);
    BaseVector* resultVec = nullptr;
    FlinkYearTestHelper::ExecuteFlinkYear(inputVec, OMNI_LONG, resultVec);
    FlinkYearTestHelper::ValidateResult(resultVec, expected, millisValues.size());

    delete resultVec;
}

TEST(FlinkYearTest, LongWithNull) {
    std::cout << "=== Test: flink_year from LONG with NULL ===" << std::endl;

    std::vector<int64_t> millisValues = {
        FlinkYearTestHelper::TimestampToMillisUtc(2024, 1, 15, 8, 0, 0),
        FlinkYearTestHelper::TimestampToMillisUtc(2023, 6, 20, 12, 0, 0),
        FlinkYearTestHelper::TimestampToMillisUtc(2025, 12, 31, 0, 0, 0)
    };

    BaseVector* inputVec = FlinkYearTestHelper::CreateLongVector(millisValues);
    inputVec->SetNull(1);

    BaseVector* resultVec = nullptr;
    FlinkYearTestHelper::ExecuteFlinkYear(inputVec, OMNI_LONG, resultVec);

    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";

    auto* resultVecTyped = dynamic_cast<Vector<int32_t>*>(resultVec);
    EXPECT_EQ(resultVecTyped->GetValue(0), 2024) << "Row 0 year should be 2024";
    EXPECT_EQ(resultVecTyped->GetValue(2), 2025) << "Row 2 year should be 2025";

    delete resultVec;
}

TEST(FlinkYearTest, LongLargeYear) {
    // Large year formatting / decomposition: 9999-12-31 23:59:59 -> 9999.
    std::vector<int64_t> millisValues = {
        FlinkYearTestHelper::TimestampToMillisUtc(9999, 12, 31, 23, 59, 59)
    };
    std::vector<int32_t> expected = {9999};

    BaseVector* inputVec = FlinkYearTestHelper::CreateLongVector(millisValues);
    BaseVector* resultVec = nullptr;
    FlinkYearTestHelper::ExecuteFlinkYear(inputVec, OMNI_LONG, resultVec);
    FlinkYearTestHelper::ValidateResult(resultVec, expected, millisValues.size());

    delete resultVec;
}

TEST(FlinkYearTest, LongMillisSemanticsVsMicros) {
    // Anti-regression: prove the OMNI_LONG path interprets the value as
    // MILLIS, not micros. The same epoch instant expressed as millis yields
    // year 2024; if the impl wrongly used fromMicros it would treat the value
    // as 1000x smaller and return 1970 (epoch year).
    // 2024-01-01 00:00:00 UTC = 1704067200 seconds = 1704067200000 millis.
    int64_t millis = FlinkYearTestHelper::TimestampToMillisUtc(2024, 1, 1, 0, 0, 0);
    ASSERT_EQ(millis, 1704067200000);

    std::vector<int64_t> millisValues = {millis};
    BaseVector* inputVec = FlinkYearTestHelper::CreateLongVector(millisValues);
    BaseVector* resultVec = nullptr;
    FlinkYearTestHelper::ExecuteFlinkYear(inputVec, OMNI_LONG, resultVec);

    ASSERT_NE(resultVec, nullptr);
    EXPECT_FALSE(resultVec->IsNull(0));
    auto* resultVecTyped = dynamic_cast<Vector<int32_t>*>(resultVec);
    // Must be 2024 (millis semantics). A buggy fromMicros path would give 1970.
    EXPECT_EQ(resultVecTyped->GetValue(0), 2024);

    delete resultVec;
}

// ============================================================================
// Multi-row batch mixing both supported input types
// ============================================================================

TEST(FlinkYearTest, MultiRowBatchLong) {
    std::vector<int64_t> millisValues = {
        FlinkYearTestHelper::TimestampToMillisUtc(1970, 1, 1, 0, 0, 0),    // epoch -> 1970
        FlinkYearTestHelper::TimestampToMillisUtc(2024, 1, 15, 0, 0, 0),   // -> 2024
        FlinkYearTestHelper::TimestampToMillisUtc(2024, 7, 15, 0, 0, 0),   // -> 2024
        FlinkYearTestHelper::TimestampToMillisUtc(1999, 12, 31, 23, 59, 59) // -> 1999
    };
    std::vector<int32_t> expected = {1970, 2024, 2024, 1999};

    BaseVector* inputVec = FlinkYearTestHelper::CreateLongVector(millisValues);
    BaseVector* resultVec = nullptr;
    FlinkYearTestHelper::ExecuteFlinkYear(inputVec, OMNI_LONG, resultVec);
    FlinkYearTestHelper::ValidateResult(resultVec, expected, millisValues.size());

    delete resultVec;
}

// ============================================================================
// flink_year_with_tz — applies an explicit session timezone to the millis.
// The stored millis are a UTC instant; the zone shifts the wall-clock year.
// ============================================================================

TEST(FlinkYear, LongWithTzAsiaShanghai) {
    std::cout << "=== Test: flink_year_with_tz Asia/Shanghai (+8) ===" << std::endl;
    // Asia/Shanghai is UTC+8 (no DST): shifts the wall-clock +8h.
    std::vector<int64_t> millisValues = {
        FlinkYearTestHelper::TimestampToMillisUtc(1996, 11, 10, 6, 55, 44),
        FlinkYearTestHelper::TimestampToMillisUtc(2024, 1, 1, 2, 30, 0)
    };
    std::vector<int32_t> expected = {1996, 2024};

    BaseVector* inputVec = FlinkYearTestHelper::CreateLongVector(millisValues);
    BaseVector* tzVec = FlinkYearTestHelper::CreateConstStringVector("Asia/Shanghai", millisValues.size());
    BaseVector* resultVec = nullptr;
    FlinkYearTestHelper::ExecuteFlinkYearWithTz(inputVec, tzVec, resultVec);
    FlinkYearTestHelper::ValidateResult(resultVec, expected, millisValues.size());

    delete resultVec;
}

TEST(FlinkYear, LongWithTzUtcIsIdentity) {
    // "UTC" zone leaves the wall-clock unchanged.
    std::vector<int64_t> millisValues = {
        FlinkYearTestHelper::TimestampToMillisUtc(1996, 11, 10, 6, 55, 44),
        FlinkYearTestHelper::TimestampToMillisUtc(2024, 6, 1, 12, 0, 0)
    };
    std::vector<int32_t> expected = {1996, 2024};

    BaseVector* inputVec = FlinkYearTestHelper::CreateLongVector(millisValues);
    BaseVector* tzVec = FlinkYearTestHelper::CreateConstStringVector("UTC", millisValues.size());
    BaseVector* resultVec = nullptr;
    FlinkYearTestHelper::ExecuteFlinkYearWithTz(inputVec, tzVec, resultVec);
    FlinkYearTestHelper::ValidateResult(resultVec, expected, millisValues.size());

    delete resultVec;
}

TEST(FlinkYear, LongWithTzNegativeOffset) {
    // America/Los_Angeles: PST = UTC-8 (winter), PDT = UTC-7 (summer).
    std::vector<int64_t> millisValues = {
        FlinkYearTestHelper::TimestampToMillisUtc(2024, 1, 15, 16, 0, 0),
        FlinkYearTestHelper::TimestampToMillisUtc(2024, 7, 15, 16, 0, 0)
    };
    std::vector<int32_t> expected = {2024, 2024};

    BaseVector* inputVec = FlinkYearTestHelper::CreateLongVector(millisValues);
    BaseVector* tzVec = FlinkYearTestHelper::CreateConstStringVector("America/Los_Angeles", millisValues.size());
    BaseVector* resultVec = nullptr;
    FlinkYearTestHelper::ExecuteFlinkYearWithTz(inputVec, tzVec, resultVec);
    FlinkYearTestHelper::ValidateResult(resultVec, expected, millisValues.size());

    delete resultVec;
}

TEST(FlinkYear, LongWithTzCrossDayBoundary) {
    // 2024-07-16 06:00:00 UTC -> America/Los_Angeles (PDT -7) -> 2024-07-15 23:00.
    // Confirms the tz shift can roll the wall-clock across a day boundary.
    std::vector<int64_t> millisValues = {
        FlinkYearTestHelper::TimestampToMillisUtc(2024, 7, 16, 6, 0, 0)
    };
    std::vector<int32_t> expected = {2024};

    BaseVector* inputVec = FlinkYearTestHelper::CreateLongVector(millisValues);
    BaseVector* tzVec = FlinkYearTestHelper::CreateConstStringVector("America/Los_Angeles", millisValues.size());
    BaseVector* resultVec = nullptr;
    FlinkYearTestHelper::ExecuteFlinkYearWithTz(inputVec, tzVec, resultVec);
    FlinkYearTestHelper::ValidateResult(resultVec, expected, millisValues.size());

    delete resultVec;
}
