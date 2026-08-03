/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: flink_minute function unit tests
 *
 * flink_minute(timestamp_millis) -> int32
 * Mirrors Flink's MINUTE(timestamp) == EXTRACT(MINUTE FROM timestamp):
 *   - OMNI_LONG : Flink TimestampData = milliseconds since epoch (NOT micros);
 *                 minute of hour extracted in UTC, no session timezone
 *                 (wall-clock semantics, matching Flink EXTRACT(MINUTE FROM
 *                 <TIMESTAMP>)).
 *
 * Returns 0-59. MINUTE is a timestamp extractor (no OMNI_INT/DATE support).
 *
 * Expected values below are derived from the Flink reference semantics
 * (codegen: (millis % 3600000) / 60000), the authoritative spec. Verified
 * against ScalarFunctionsTest (EXTRACT(MINUTE FROM f18)=55 for 1996-11-10
 * 06:55:44.333) and TimeFunctionsITCase (MINUTE(f0)=22). All expected values
 * cross-checked against Python.
 */

#include <gtest/gtest.h>
#include <iostream>
#include <vector>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/functions/FlinkMinute.h"
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
class FlinkMinuteTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const flink_minute_test_env =
    ::testing::AddGlobalTestEnvironment(new FlinkMinuteTestEnvironment);

class FlinkMinuteTestHelper {
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

    static void ExecuteFlinkMinute(BaseVector* inputVec, BaseVector*& result) {
        auto signature = std::make_shared<FunctionSignature>("flink_minute",
            std::vector<DataTypeId>{OMNI_LONG}, OMNI_INT);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "flink_minute function not found for signature";

        auto outputType = std::make_shared<DataType>(OMNI_INT);
        ExecutionContext context;
        context.SetResultRowSize(inputVec->GetSize());
        std::stack<BaseVector*> args;
        args.push(inputVec);

        ASSERT_NO_THROW(function->Apply(args, outputType, result, &context))
            << "flink_minute function threw an exception";
    }

    // Convert a UTC wall-clock datetime to Flink TIMESTAMP millis (epoch millis).
    // Flink TimestampData stores millisecond = epochDay * 86400000 + nanoOfDay/1e6.
    // For whole-second values this is simply epochSeconds * 1000. The minute is
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

TEST(FlinkMinuteTest, LongBasicMillis) {
    std::cout << "=== Test: flink_minute from LONG (millis) - basic ===" << std::endl;

    // Flink test references:
    //   EXTRACT(MINUTE FROM f18) = 55 for TIMESTAMP '1996-11-10 06:55:44.333'.
    //   EXTRACT(MINUTE FROM f0)  = 22 for 2000-01-31 11:22:33.123456789.
    //   MINUTE(TIMESTAMP '1994-09-27 13:14:15') = 14 (sql_functions.yml example).
    std::vector<int64_t> millisValues = {
        FlinkMinuteTestHelper::TimestampToMillisUtc(1996, 11, 10, 6, 55, 44),
        FlinkMinuteTestHelper::TimestampToMillisUtc(2000, 1, 31, 11, 22, 33),
        FlinkMinuteTestHelper::TimestampToMillisUtc(1994, 9, 27, 13, 14, 15)
    };
    std::vector<int32_t> expected = {55, 22, 14};

    BaseVector* inputVec = FlinkMinuteTestHelper::CreateLongVector(millisValues);
    BaseVector* resultVec = nullptr;
    FlinkMinuteTestHelper::ExecuteFlinkMinute(inputVec, resultVec);
    FlinkMinuteTestHelper::ValidateResult(resultVec, expected, millisValues.size());

    delete resultVec;
}

TEST(FlinkMinuteTest, LongFullHourCoverage) {
    // 60 minutes of one hour (2024-01-15 10:00 .. 10:59) -> MINUTE 0..59.
    // Confirms the full 0-59 range and the wall-clock (no tz) extraction.
    std::vector<int64_t> millisValues;
    std::vector<int32_t> expected;
    for (int mi = 0; mi < 60; ++mi) {
        millisValues.push_back(FlinkMinuteTestHelper::TimestampToMillisUtc(2024, 1, 15, 10, mi, 0));
        expected.push_back(mi);
    }

    BaseVector* inputVec = FlinkMinuteTestHelper::CreateLongVector(millisValues);
    BaseVector* resultVec = nullptr;
    FlinkMinuteTestHelper::ExecuteFlinkMinute(inputVec, resultVec);
    FlinkMinuteTestHelper::ValidateResult(resultVec, expected, millisValues.size());

    delete resultVec;
}

TEST(FlinkMinuteTest, LongHourBoundary) {
    // Wall-clock hour boundary (no session tz):
    //   2024-01-15 23:59:59 -> 59; 2024-01-16 00:00:00 -> 0.
    std::vector<int64_t> millisValues = {
        FlinkMinuteTestHelper::TimestampToMillisUtc(2024, 1, 15, 23, 59, 59),
        FlinkMinuteTestHelper::TimestampToMillisUtc(2024, 1, 16, 0, 0, 0)
    };
    std::vector<int32_t> expected = {59, 0};

    BaseVector* inputVec = FlinkMinuteTestHelper::CreateLongVector(millisValues);
    BaseVector* resultVec = nullptr;
    FlinkMinuteTestHelper::ExecuteFlinkMinute(inputVec, resultVec);
    FlinkMinuteTestHelper::ValidateResult(resultVec, expected, millisValues.size());

    delete resultVec;
}

TEST(FlinkMinuteTest, LongPreEpochNegativeMillis) {
    // Negative millis (before 1970): 1969-12-31 23:59:59 -> 59; 1969-12-31 00:00:00 -> 0.
    // Timestamp::fromMillis handles negative millis via floor division.
    std::vector<int64_t> millisValues = {
        FlinkMinuteTestHelper::TimestampToMillisUtc(1969, 12, 31, 23, 59, 59),
        FlinkMinuteTestHelper::TimestampToMillisUtc(1969, 12, 31, 0, 0, 0)
    };
    std::vector<int32_t> expected = {59, 0};

    BaseVector* inputVec = FlinkMinuteTestHelper::CreateLongVector(millisValues);
    BaseVector* resultVec = nullptr;
    FlinkMinuteTestHelper::ExecuteFlinkMinute(inputVec, resultVec);
    FlinkMinuteTestHelper::ValidateResult(resultVec, expected, millisValues.size());

    delete resultVec;
}

TEST(FlinkMinuteTest, LongWithNull) {
    std::cout << "=== Test: flink_minute from LONG with NULL ===" << std::endl;

    std::vector<int64_t> millisValues = {
        FlinkMinuteTestHelper::TimestampToMillisUtc(2024, 1, 15, 8, 30, 0),
        FlinkMinuteTestHelper::TimestampToMillisUtc(2024, 6, 20, 12, 45, 0),
        FlinkMinuteTestHelper::TimestampToMillisUtc(2024, 12, 31, 23, 59, 0)
    };

    BaseVector* inputVec = FlinkMinuteTestHelper::CreateLongVector(millisValues);
    inputVec->SetNull(1);

    BaseVector* resultVec = nullptr;
    FlinkMinuteTestHelper::ExecuteFlinkMinute(inputVec, resultVec);

    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";

    auto* resultVecTyped = dynamic_cast<Vector<int32_t>*>(resultVec);
    EXPECT_EQ(resultVecTyped->GetValue(0), 30) << "Row 0 minute should be 30 (08:30:00)";
    EXPECT_EQ(resultVecTyped->GetValue(2), 59) << "Row 2 minute should be 59 (23:59:00)";

    delete resultVec;
}

TEST(FlinkMinuteTest, LongEpoch) {
    // Epoch instant: 1970-01-01 00:00:00 -> MINUTE 0.
    std::vector<int64_t> millisValues = {
        FlinkMinuteTestHelper::TimestampToMillisUtc(1970, 1, 1, 0, 0, 0)
    };
    std::vector<int32_t> expected = {0};

    BaseVector* inputVec = FlinkMinuteTestHelper::CreateLongVector(millisValues);
    BaseVector* resultVec = nullptr;
    FlinkMinuteTestHelper::ExecuteFlinkMinute(inputVec, resultVec);
    FlinkMinuteTestHelper::ValidateResult(resultVec, expected, millisValues.size());

    delete resultVec;
}

TEST(FlinkMinuteTest, LongLargeYear) {
    // Large year: 9999-12-31 23:59:59 -> MINUTE 59.
    std::vector<int64_t> millisValues = {
        FlinkMinuteTestHelper::TimestampToMillisUtc(9999, 12, 31, 23, 59, 59)
    };
    std::vector<int32_t> expected = {59};

    BaseVector* inputVec = FlinkMinuteTestHelper::CreateLongVector(millisValues);
    BaseVector* resultVec = nullptr;
    FlinkMinuteTestHelper::ExecuteFlinkMinute(inputVec, resultVec);
    FlinkMinuteTestHelper::ValidateResult(resultVec, expected, millisValues.size());

    delete resultVec;
}

TEST(FlinkMinuteTest, LongSubSecondIrrelevant) {
    // Sub-second precision must not affect MINUTE. 2024-06-15 12:30:45 (no
    // fractional) and the same instant +500ms both -> 30.
    int64_t baseMillis = FlinkMinuteTestHelper::TimestampToMillisUtc(2024, 6, 15, 12, 30, 45);
    std::vector<int64_t> millisValues = {
        baseMillis,        // whole second
        baseMillis + 500   // +500ms (sub-millisecond) — same minute
    };
    std::vector<int32_t> expected = {30, 30};

    BaseVector* inputVec = FlinkMinuteTestHelper::CreateLongVector(millisValues);
    BaseVector* resultVec = nullptr;
    FlinkMinuteTestHelper::ExecuteFlinkMinute(inputVec, resultVec);
    FlinkMinuteTestHelper::ValidateResult(resultVec, expected, millisValues.size());

    delete resultVec;
}

TEST(FlinkMinuteTest, LongMillisSemanticsVsMicros) {
    // Anti-regression: prove the OMNI_LONG path interprets the value as
    // MILLIS, not micros. 2024-06-01 12:30:45 UTC -> 1717245045000 millis
    // -> MINUTE 30. If the impl wrongly used fromMicros, the value would be
    // treated as 1000x smaller and fall in 1970-01-20 ~03:xx -> MINUTE ~0/1,
    // a clearly different result.
    int64_t millis = FlinkMinuteTestHelper::TimestampToMillisUtc(2024, 6, 1, 12, 30, 45);
    ASSERT_EQ(millis, 1717245045000);

    std::vector<int64_t> millisValues = {millis};
    BaseVector* inputVec = FlinkMinuteTestHelper::CreateLongVector(millisValues);
    BaseVector* resultVec = nullptr;
    FlinkMinuteTestHelper::ExecuteFlinkMinute(inputVec, resultVec);

    ASSERT_NE(resultVec, nullptr);
    EXPECT_FALSE(resultVec->IsNull(0));
    auto* resultVecTyped = dynamic_cast<Vector<int32_t>*>(resultVec);
    // Must be 30 (2024-06-01 12:30, millis semantics). A buggy fromMicros path would give ~0/1.
    EXPECT_EQ(resultVecTyped->GetValue(0), 30);

    delete resultVec;
}

// ============================================================================
// Multi-row batch mixing timestamps across several minutes
// ============================================================================

TEST(FlinkMinuteTest, MultiRowBatchLong) {
    std::vector<int64_t> millisValues = {
        FlinkMinuteTestHelper::TimestampToMillisUtc(1970, 1, 1, 0, 0, 0),    // epoch -> 0
        FlinkMinuteTestHelper::TimestampToMillisUtc(2024, 5, 15, 6, 30, 0),   // -> 30
        FlinkMinuteTestHelper::TimestampToMillisUtc(2024, 8, 20, 18, 45, 0),  // -> 45
        FlinkMinuteTestHelper::TimestampToMillisUtc(1996, 11, 10, 6, 55, 44)  // -> 55
    };
    std::vector<int32_t> expected = {0, 30, 45, 55};

    BaseVector* inputVec = FlinkMinuteTestHelper::CreateLongVector(millisValues);
    BaseVector* resultVec = nullptr;
    FlinkMinuteTestHelper::ExecuteFlinkMinute(inputVec, resultVec);
    FlinkMinuteTestHelper::ValidateResult(resultVec, expected, millisValues.size());

    delete resultVec;
}
