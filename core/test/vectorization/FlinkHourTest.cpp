/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: flink_hour function unit tests
 *
 * flink_hour(timestamp_millis) -> int32
 * Mirrors Flink's HOUR(timestamp) == EXTRACT(HOUR FROM timestamp):
 *   - OMNI_LONG : Flink TimestampData = milliseconds since epoch (NOT micros);
 *                 hour of day extracted in UTC, no session timezone (wall-clock
 *                 semantics, matching Flink EXTRACT(HOUR FROM <TIMESTAMP>)).
 *
 * Returns 0-23. HOUR is a timestamp extractor (no OMNI_INT/DATE support).
 *
 * Expected values below are derived from the Flink reference semantics
 * (codegen: (millis % 86400000) / 3600000), the authoritative spec. Verified
 * against ScalarFunctionsTest (EXTRACT(HOUR FROM f18)=6 for 1996-11-10
 * 06:55:44.333) and TimeFunctionsITCase (HOUR(f0)=11, HOUR(f1)=1). All
 * expected values cross-checked against Python.
 */

#include <gtest/gtest.h>
#include <iostream>
#include <vector>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/functions/FlinkHour.h"
#include "vectorization/VectorFunction.h"
#include "codegen/func_signature.h"
#include "vector/vector_helper.h"
#include "vector/vector.h"
#include "type/Timestamp.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::codegen;
using namespace omniruntime::TestUtil;

// Initialize function registration before running tests
class FlinkHourTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const flink_hour_test_env =
    ::testing::AddGlobalTestEnvironment(new FlinkHourTestEnvironment);

class FlinkHourTestHelper {
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

    static BaseVector* CreateLongVector(const std::vector<int64_t>& values) {
        BaseVector* vec = VectorHelper::CreateFlatVector(OMNI_LONG, values.size());
        auto* typedVec = static_cast<Vector<int64_t>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typedVec->SetValue(i, values[i]);
        }
        return vec;
    }

    static void ExecuteFlinkHour(BaseVector* inputVec, BaseVector*& result) {
        auto signature = std::make_shared<FunctionSignature>("flink_hour",
            std::vector<DataTypeId>{OMNI_LONG}, OMNI_INT);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "flink_hour function not found for signature";

        auto outputType = std::make_shared<DataType>(OMNI_INT);
        ExecutionContext context;
        context.SetResultRowSize(inputVec->GetSize());
        std::stack<BaseVector*> args;
        args.push(inputVec);

        ASSERT_NO_THROW(function->Apply(args, outputType, result, &context))
            << "flink_hour function threw an exception";
    }

    // Convert a UTC wall-clock datetime to Flink TIMESTAMP millis (epoch millis).
    // Flink TimestampData stores millisecond = epochDay * 86400000 + nanoOfDay/1e6.
    // For whole-second values this is simply epochSeconds * 1000. The hour is
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
// OMNI_LONG (Flink TIMESTAMP = milliseconds since epoch) — no session tz
// ============================================================================

TEST(FlinkHourTest, LongBasicMillis) {
    std::cout << "=== Test: flink_hour from LONG (millis) - basic ===" << std::endl;

    // Flink test references:
    //   EXTRACT(HOUR FROM f18) = 6 for TIMESTAMP '1996-11-10 06:55:44.333'.
    //   HOUR(f0) = 11 for 2000-01-31 11:22:33.123456789.
    //   HOUR(f1) = 1  for 2020-02-29 01:56:59.987654321.
    std::vector<int64_t> millisValues = {
        FlinkHourTestHelper::TimestampToMillisUtc(1996, 11, 10, 6, 55, 44),
        FlinkHourTestHelper::TimestampToMillisUtc(2000, 1, 31, 11, 22, 33),
        FlinkHourTestHelper::TimestampToMillisUtc(2020, 2, 29, 1, 56, 59)
    };
    std::vector<int32_t> expected = {6, 11, 1};

    BaseVector* inputVec = FlinkHourTestHelper::CreateLongVector(millisValues);
    BaseVector* resultVec = nullptr;
    FlinkHourTestHelper::ExecuteFlinkHour(inputVec, resultVec);
    FlinkHourTestHelper::ValidateResult(resultVec, expected, millisValues.size());

    delete resultVec;
}

TEST(FlinkHourTest, LongFullDayCoverage) {
    // 24 hours of one day (2024-01-15 00:00 .. 23:00) -> HOUR 0..23.
    // Confirms the full 0-23 range and the wall-clock (no tz) extraction.
    std::vector<int64_t> millisValues;
    std::vector<int32_t> expected;
    for (int h = 0; h < 24; ++h) {
        millisValues.push_back(FlinkHourTestHelper::TimestampToMillisUtc(2024, 1, 15, h, 0, 0));
        expected.push_back(h);
    }

    BaseVector* inputVec = FlinkHourTestHelper::CreateLongVector(millisValues);
    BaseVector* resultVec = nullptr;
    FlinkHourTestHelper::ExecuteFlinkHour(inputVec, resultVec);
    FlinkHourTestHelper::ValidateResult(resultVec, expected, millisValues.size());

    delete resultVec;
}

TEST(FlinkHourTest, LongDayBoundary) {
    // Wall-clock day boundary (no session tz):
    //   2024-01-15 23:59:59 -> 23; 2024-01-16 00:00:00 -> 0.
    std::vector<int64_t> millisValues = {
        FlinkHourTestHelper::TimestampToMillisUtc(2024, 1, 15, 23, 59, 59),
        FlinkHourTestHelper::TimestampToMillisUtc(2024, 1, 16, 0, 0, 0)
    };
    std::vector<int32_t> expected = {23, 0};

    BaseVector* inputVec = FlinkHourTestHelper::CreateLongVector(millisValues);
    BaseVector* resultVec = nullptr;
    FlinkHourTestHelper::ExecuteFlinkHour(inputVec, resultVec);
    FlinkHourTestHelper::ValidateResult(resultVec, expected, millisValues.size());

    delete resultVec;
}

TEST(FlinkHourTest, LongPreEpochNegativeMillis) {
    // Negative millis (before 1970): 1969-12-31 23:59:59 -> 23; 1969-12-31 00:00:00 -> 0.
    // Timestamp::fromMillis handles negative millis via floor division.
    std::vector<int64_t> millisValues = {
        FlinkHourTestHelper::TimestampToMillisUtc(1969, 12, 31, 23, 59, 59),
        FlinkHourTestHelper::TimestampToMillisUtc(1969, 12, 31, 0, 0, 0)
    };
    std::vector<int32_t> expected = {23, 0};

    BaseVector* inputVec = FlinkHourTestHelper::CreateLongVector(millisValues);
    BaseVector* resultVec = nullptr;
    FlinkHourTestHelper::ExecuteFlinkHour(inputVec, resultVec);
    FlinkHourTestHelper::ValidateResult(resultVec, expected, millisValues.size());

    delete resultVec;
}

TEST(FlinkHourTest, LongWithNull) {
    std::cout << "=== Test: flink_hour from LONG with NULL ===" << std::endl;

    std::vector<int64_t> millisValues = {
        FlinkHourTestHelper::TimestampToMillisUtc(2024, 1, 15, 8, 0, 0),
        FlinkHourTestHelper::TimestampToMillisUtc(2024, 6, 20, 12, 0, 0),
        FlinkHourTestHelper::TimestampToMillisUtc(2024, 12, 31, 23, 0, 0)
    };

    BaseVector* inputVec = FlinkHourTestHelper::CreateLongVector(millisValues);
    inputVec->SetNull(1);

    BaseVector* resultVec = nullptr;
    FlinkHourTestHelper::ExecuteFlinkHour(inputVec, resultVec);

    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";

    auto* resultVecTyped = dynamic_cast<Vector<int32_t>*>(resultVec);
    EXPECT_EQ(resultVecTyped->GetValue(0), 8) << "Row 0 hour should be 8 (08:00:00)";
    EXPECT_EQ(resultVecTyped->GetValue(2), 23) << "Row 2 hour should be 23 (23:00:00)";

    delete resultVec;
}

TEST(FlinkHourTest, LongEpoch) {
    // Epoch instant: 1970-01-01 00:00:00 -> HOUR 0.
    std::vector<int64_t> millisValues = {
        FlinkHourTestHelper::TimestampToMillisUtc(1970, 1, 1, 0, 0, 0)
    };
    std::vector<int32_t> expected = {0};

    BaseVector* inputVec = FlinkHourTestHelper::CreateLongVector(millisValues);
    BaseVector* resultVec = nullptr;
    FlinkHourTestHelper::ExecuteFlinkHour(inputVec, resultVec);
    FlinkHourTestHelper::ValidateResult(resultVec, expected, millisValues.size());

    delete resultVec;
}

TEST(FlinkHourTest, LongLargeYear) {
    // Large year: 9999-12-31 23:59:59 -> HOUR 23.
    std::vector<int64_t> millisValues = {
        FlinkHourTestHelper::TimestampToMillisUtc(9999, 12, 31, 23, 59, 59)
    };
    std::vector<int32_t> expected = {23};

    BaseVector* inputVec = FlinkHourTestHelper::CreateLongVector(millisValues);
    BaseVector* resultVec = nullptr;
    FlinkHourTestHelper::ExecuteFlinkHour(inputVec, resultVec);
    FlinkHourTestHelper::ValidateResult(resultVec, expected, millisValues.size());

    delete resultVec;
}

TEST(FlinkHourTest, LongSubSecondIrrelevant) {
    // Sub-second precision must not affect HOUR. 2024-06-15 12:30:45 (no
    // fractional) and the same instant expressed with .500 millis both -> 12.
    int64_t baseMillis = FlinkHourTestHelper::TimestampToMillisUtc(2024, 6, 15, 12, 30, 45);
    std::vector<int64_t> millisValues = {
        baseMillis,        // whole second
        baseMillis + 500   // +500ms (sub-millisecond) — same hour
    };
    std::vector<int32_t> expected = {12, 12};

    BaseVector* inputVec = FlinkHourTestHelper::CreateLongVector(millisValues);
    BaseVector* resultVec = nullptr;
    FlinkHourTestHelper::ExecuteFlinkHour(inputVec, resultVec);
    FlinkHourTestHelper::ValidateResult(resultVec, expected, millisValues.size());

    delete resultVec;
}

TEST(FlinkHourTest, LongMillisSemanticsVsMicros) {
    // Anti-regression: prove the OMNI_LONG path interprets the value as
    // MILLIS, not micros. 2024-06-01 12:00:00 UTC -> 1717243200000 millis
    // -> HOUR 12. If the impl wrongly used fromMicros, the value would be
    // treated as 1000x smaller and fall in 1970-01-20 ~03:00 -> HOUR ~3,
    // a clearly different result.
    int64_t millis = FlinkHourTestHelper::TimestampToMillisUtc(2024, 6, 1, 12, 0, 0);
    ASSERT_EQ(millis, 1717243200000);

    std::vector<int64_t> millisValues = {millis};
    BaseVector* inputVec = FlinkHourTestHelper::CreateLongVector(millisValues);
    BaseVector* resultVec = nullptr;
    FlinkHourTestHelper::ExecuteFlinkHour(inputVec, resultVec);

    ASSERT_NE(resultVec, nullptr);
    EXPECT_FALSE(resultVec->IsNull(0));
    auto* resultVecTyped = dynamic_cast<Vector<int32_t>*>(resultVec);
    // Must be 12 (2024-06-01 12:00, millis semantics). A buggy fromMicros path would give ~3.
    EXPECT_EQ(resultVecTyped->GetValue(0), 12);

    delete resultVec;
}

// ============================================================================
// Multi-row batch mixing timestamps across several hours
// ============================================================================

TEST(FlinkHourTest, MultiRowBatchLong) {
    std::vector<int64_t> millisValues = {
        FlinkHourTestHelper::TimestampToMillisUtc(1970, 1, 1, 0, 0, 0),    // epoch -> 0
        FlinkHourTestHelper::TimestampToMillisUtc(2024, 5, 15, 6, 0, 0),   // -> 6
        FlinkHourTestHelper::TimestampToMillisUtc(2024, 8, 20, 18, 0, 0),  // -> 18
        FlinkHourTestHelper::TimestampToMillisUtc(1996, 11, 10, 6, 55, 44) // -> 6
    };
    std::vector<int32_t> expected = {0, 6, 18, 6};

    BaseVector* inputVec = FlinkHourTestHelper::CreateLongVector(millisValues);
    BaseVector* resultVec = nullptr;
    FlinkHourTestHelper::ExecuteFlinkHour(inputVec, resultVec);
    FlinkHourTestHelper::ValidateResult(resultVec, expected, millisValues.size());

    delete resultVec;
}
