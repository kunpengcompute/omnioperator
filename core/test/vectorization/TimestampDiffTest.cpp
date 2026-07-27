/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: TimestampDiff function unit tests
 */

#include <gtest/gtest.h>
#include <iostream>
#include <vector>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/functions/TimestampDiff.h"
#include "vectorization/VectorFunction.h"
#include "codegen/func_signature.h"
#include "vector/vector_helper.h"
#include "vector/vector.h"
#include "type/Timestamp.h"
#include "type/date_time_utils.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::codegen;
using namespace omniruntime::TestUtil;

// Initialize function registration before running tests
class TimestampDiffTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const timestamp_diff_test_env =
    ::testing::AddGlobalTestEnvironment(new TimestampDiffTestEnvironment);

class TimestampDiffTestHelper {
public:
    /// Convert date/time components to microseconds since epoch (UTC)
    static int64_t ToMicros(int32_t year, int month, int day, int hour = 0, int minute = 0, int second = 0)
    {
        std::tm tmValue = {};
        tmValue.tm_year = year - 1900;
        tmValue.tm_mon = month - 1;
        tmValue.tm_mday = day;
        tmValue.tm_hour = hour;
        tmValue.tm_min = minute;
        tmValue.tm_sec = second;
        tmValue.tm_isdst = 0;
        int64_t epochSeconds = Timestamp::calendarUtcToEpoch(tmValue);
        return epochSeconds * 1000000LL;
    }

    /// Create a TIMESTAMP flat vector from microsecond values
    static BaseVector* CreateTimestampVector(const std::vector<int64_t>& micros)
    {
        BaseVector* vec = VectorHelper::CreateFlatVector(OMNI_TIMESTAMP, micros.size());
        auto* typedVec = static_cast<Vector<int64_t>*>(vec);
        for (size_t i = 0; i < micros.size(); ++i) {
            typedVec->SetValue(i, micros[i]);
        }
        return vec;
    }

    /// Execute timestampdiff(unit, ts1, ts2)
    static void ExecuteTimestampDiff(BaseVector* unitVec, BaseVector* ts1Vec, BaseVector* ts2Vec,
                                     BaseVector*& result)
    {
        auto signature = std::make_shared<FunctionSignature>("timestampdiff",
            std::vector<DataTypeId>{OMNI_VARCHAR, OMNI_TIMESTAMP, OMNI_TIMESTAMP}, OMNI_LONG);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "TimestampDiff function not found for signature";

        auto outputType = std::make_shared<DataType>(OMNI_LONG);
        ExecutionContext context;
        context.SetResultRowSize(ts1Vec->GetSize());
        std::stack<BaseVector*> args;

        // Push order: ts2 first (bottom), ts1, unit last (top)
        args.push(ts2Vec);
        args.push(ts1Vec);
        args.push(unitVec);

        ASSERT_NO_THROW(function->Apply(args, outputType, result, &context))
            << "TimestampDiff function threw an exception";
    }

    /// Validate result against expected int64_t values
    static void ValidateResult(BaseVector* result, const std::vector<int64_t>& expected, int rowSize)
    {
        auto* resultVec = dynamic_cast<Vector<int64_t>*>(result);
        ASSERT_NE(resultVec, nullptr) << "Result vector type mismatch";

        for (int i = 0; i < rowSize; ++i) {
            if (result->IsNull(i)) {
                std::cout << "Row " << i << ": NULL" << std::endl;
                continue;
            }
            int64_t actualValue = resultVec->GetValue(i);
            int64_t expectedValue = expected[i];
            std::cout << "Row " << i << ": Expected=" << expectedValue << ", Actual=" << actualValue << std::endl;
            EXPECT_EQ(actualValue, expectedValue) << "Row " << i << " value mismatch";
        }
    }
};

// Test: Difference in seconds
TEST(TimestampDiffTest, DiffSeconds) {
    std::cout << "=== Test: DiffSeconds ===" << std::endl;
    int32_t rowSize = 2;

    // ts1: 2024-01-01 00:01:00, ts2: 2024-01-01 00:00:00 -> 60 seconds
    // ts1: 2024-01-01 00:00:00, ts2: 2024-01-01 00:01:00 -> -60 seconds
    std::vector<int64_t> ts1 = {
        TimestampDiffTestHelper::ToMicros(2024, 1, 1, 0, 1, 0),
        TimestampDiffTestHelper::ToMicros(2024, 1, 1, 0, 0, 0)
    };
    std::vector<int64_t> ts2 = {
        TimestampDiffTestHelper::ToMicros(2024, 1, 1, 0, 0, 0),
        TimestampDiffTestHelper::ToMicros(2024, 1, 1, 0, 1, 0)
    };
    std::vector<int64_t> expected = {60, -60};

    std::string unitStr = "SECOND";
    BaseVector* unitVec = new ConstVector<std::string_view>(std::string_view(unitStr), OMNI_VARCHAR, rowSize);
    BaseVector* ts1Vec = TimestampDiffTestHelper::CreateTimestampVector(ts1);
    BaseVector* ts2Vec = TimestampDiffTestHelper::CreateTimestampVector(ts2);
    BaseVector* result = nullptr;

    TimestampDiffTestHelper::ExecuteTimestampDiff(unitVec, ts1Vec, ts2Vec, result);
    TimestampDiffTestHelper::ValidateResult(result, expected, rowSize);

    delete result;
}

// Test: Difference in minutes
TEST(TimestampDiffTest, DiffMinutes) {
    std::cout << "=== Test: DiffMinutes ===" << std::endl;
    int32_t rowSize = 2;

    // ts1: 2024-01-01 00:05:00, ts2: 2024-01-01 00:00:00 -> 5 minutes
    // ts1: 2024-01-01 00:00:30, ts2: 2024-01-01 00:00:00 -> 0 minutes (truncated)
    std::vector<int64_t> ts1 = {
        TimestampDiffTestHelper::ToMicros(2024, 1, 1, 0, 5, 0),
        TimestampDiffTestHelper::ToMicros(2024, 1, 1, 0, 0, 30)
    };
    std::vector<int64_t> ts2 = {
        TimestampDiffTestHelper::ToMicros(2024, 1, 1, 0, 0, 0),
        TimestampDiffTestHelper::ToMicros(2024, 1, 1, 0, 0, 0)
    };
    std::vector<int64_t> expected = {5, 0};

    std::string unitStr = "MINUTE";
    BaseVector* unitVec = new ConstVector<std::string_view>(std::string_view(unitStr), OMNI_VARCHAR, rowSize);
    BaseVector* ts1Vec = TimestampDiffTestHelper::CreateTimestampVector(ts1);
    BaseVector* ts2Vec = TimestampDiffTestHelper::CreateTimestampVector(ts2);
    BaseVector* result = nullptr;

    TimestampDiffTestHelper::ExecuteTimestampDiff(unitVec, ts1Vec, ts2Vec, result);
    TimestampDiffTestHelper::ValidateResult(result, expected, rowSize);

    delete result;
}

// Test: Difference in hours
TEST(TimestampDiffTest, DiffHours) {
    std::cout << "=== Test: DiffHours ===" << std::endl;
    int32_t rowSize = 1;

    // ts1: 2024-01-01 02:00:00, ts2: 2024-01-01 00:00:00 -> 2 hours
    std::vector<int64_t> ts1 = {TimestampDiffTestHelper::ToMicros(2024, 1, 1, 2, 0, 0)};
    std::vector<int64_t> ts2 = {TimestampDiffTestHelper::ToMicros(2024, 1, 1, 0, 0, 0)};
    std::vector<int64_t> expected = {2};

    std::string unitStr = "HOUR";
    BaseVector* unitVec = new ConstVector<std::string_view>(std::string_view(unitStr), OMNI_VARCHAR, rowSize);
    BaseVector* ts1Vec = TimestampDiffTestHelper::CreateTimestampVector(ts1);
    BaseVector* ts2Vec = TimestampDiffTestHelper::CreateTimestampVector(ts2);
    BaseVector* result = nullptr;

    TimestampDiffTestHelper::ExecuteTimestampDiff(unitVec, ts1Vec, ts2Vec, result);
    TimestampDiffTestHelper::ValidateResult(result, expected, rowSize);

    delete result;
}

// Test: Difference in days
TEST(TimestampDiffTest, DiffDays) {
    std::cout << "=== Test: DiffDays ===" << std::endl;
    int32_t rowSize = 2;

    // ts1: 2024-01-11, ts2: 2024-01-01 -> 10 days
    // ts1: 2024-01-05, ts2: 2024-01-10 -> -5 days
    std::vector<int64_t> ts1 = {
        TimestampDiffTestHelper::ToMicros(2024, 1, 11, 0, 0, 0),
        TimestampDiffTestHelper::ToMicros(2024, 1, 5, 0, 0, 0)
    };
    std::vector<int64_t> ts2 = {
        TimestampDiffTestHelper::ToMicros(2024, 1, 1, 0, 0, 0),
        TimestampDiffTestHelper::ToMicros(2024, 1, 10, 0, 0, 0)
    };
    std::vector<int64_t> expected = {10, -5};

    std::string unitStr = "DAY";
    BaseVector* unitVec = new ConstVector<std::string_view>(std::string_view(unitStr), OMNI_VARCHAR, rowSize);
    BaseVector* ts1Vec = TimestampDiffTestHelper::CreateTimestampVector(ts1);
    BaseVector* ts2Vec = TimestampDiffTestHelper::CreateTimestampVector(ts2);
    BaseVector* result = nullptr;

    TimestampDiffTestHelper::ExecuteTimestampDiff(unitVec, ts1Vec, ts2Vec, result);
    TimestampDiffTestHelper::ValidateResult(result, expected, rowSize);

    delete result;
}

// Test: Difference in months
TEST(TimestampDiffTest, DiffMonths) {
    std::cout << "=== Test: DiffMonths ===" << std::endl;
    int32_t rowSize = 3;

    // ts1: 2024-03-15, ts2: 2024-01-15 -> 2 months
    // ts1: 2024-02-29, ts2: 2024-01-31 -> 0 months (same calendar month diff based on month only)
    // ts1: 2025-06-01, ts2: 2024-06-01 -> 12 months
    std::vector<int64_t> ts1 = {
        TimestampDiffTestHelper::ToMicros(2024, 3, 15, 0, 0, 0),
        TimestampDiffTestHelper::ToMicros(2024, 2, 29, 0, 0, 0),
        TimestampDiffTestHelper::ToMicros(2025, 6, 1, 0, 0, 0)
    };
    std::vector<int64_t> ts2 = {
        TimestampDiffTestHelper::ToMicros(2024, 1, 15, 0, 0, 0),
        TimestampDiffTestHelper::ToMicros(2024, 1, 31, 0, 0, 0),
        TimestampDiffTestHelper::ToMicros(2024, 6, 1, 0, 0, 0)
    };
    std::vector<int64_t> expected = {2, 1, 12};

    std::string unitStr = "MONTH";
    BaseVector* unitVec = new ConstVector<std::string_view>(std::string_view(unitStr), OMNI_VARCHAR, rowSize);
    BaseVector* ts1Vec = TimestampDiffTestHelper::CreateTimestampVector(ts1);
    BaseVector* ts2Vec = TimestampDiffTestHelper::CreateTimestampVector(ts2);
    BaseVector* result = nullptr;

    TimestampDiffTestHelper::ExecuteTimestampDiff(unitVec, ts1Vec, ts2Vec, result);
    TimestampDiffTestHelper::ValidateResult(result, expected, rowSize);

    delete result;
}

// Test: Difference in years
TEST(TimestampDiffTest, DiffYears) {
    std::cout << "=== Test: DiffYears ===" << std::endl;
    int32_t rowSize = 2;

    // ts1: 2026-06-15, ts2: 2024-06-15 -> 2 years
    // ts1: 2024-12-31, ts2: 2025-01-01 -> -1 year (based on calendar year)
    std::vector<int64_t> ts1 = {
        TimestampDiffTestHelper::ToMicros(2026, 6, 15, 0, 0, 0),
        TimestampDiffTestHelper::ToMicros(2024, 12, 31, 0, 0, 0)
    };
    std::vector<int64_t> ts2 = {
        TimestampDiffTestHelper::ToMicros(2024, 6, 15, 0, 0, 0),
        TimestampDiffTestHelper::ToMicros(2025, 1, 1, 0, 0, 0)
    };
    std::vector<int64_t> expected = {2, -1};

    std::string unitStr = "YEAR";
    BaseVector* unitVec = new ConstVector<std::string_view>(std::string_view(unitStr), OMNI_VARCHAR, rowSize);
    BaseVector* ts1Vec = TimestampDiffTestHelper::CreateTimestampVector(ts1);
    BaseVector* ts2Vec = TimestampDiffTestHelper::CreateTimestampVector(ts2);
    BaseVector* result = nullptr;

    TimestampDiffTestHelper::ExecuteTimestampDiff(unitVec, ts1Vec, ts2Vec, result);
    TimestampDiffTestHelper::ValidateResult(result, expected, rowSize);

    delete result;
}

// Test: Zero difference
TEST(TimestampDiffTest, ZeroDiff) {
    std::cout << "=== Test: ZeroDiff ===" << std::endl;
    int32_t rowSize = 1;

    // ts1 == ts2 -> 0
    std::vector<int64_t> ts1 = {TimestampDiffTestHelper::ToMicros(2024, 1, 1, 0, 0, 0)};
    std::vector<int64_t> ts2 = {TimestampDiffTestHelper::ToMicros(2024, 1, 1, 0, 0, 0)};
    std::vector<int64_t> expected = {0};

    std::string unitStr = "HOUR";
    BaseVector* unitVec = new ConstVector<std::string_view>(std::string_view(unitStr), OMNI_VARCHAR, rowSize);
    BaseVector* ts1Vec = TimestampDiffTestHelper::CreateTimestampVector(ts1);
    BaseVector* ts2Vec = TimestampDiffTestHelper::CreateTimestampVector(ts2);
    BaseVector* result = nullptr;

    TimestampDiffTestHelper::ExecuteTimestampDiff(unitVec, ts1Vec, ts2Vec, result);
    TimestampDiffTestHelper::ValidateResult(result, expected, rowSize);

    delete result;
}

// Test: NULL ts1 -> result is NULL
TEST(TimestampDiffTest, NullTs1) {
    std::cout << "=== Test: NullTs1 ===" << std::endl;
    int32_t rowSize = 3;

    std::vector<int64_t> ts1 = {
        TimestampDiffTestHelper::ToMicros(2024, 1, 1, 0, 0, 0),
        TimestampDiffTestHelper::ToMicros(2024, 1, 1, 0, 0, 0),
        TimestampDiffTestHelper::ToMicros(2024, 1, 1, 0, 0, 0)
    };
    std::vector<int64_t> ts2 = {
        TimestampDiffTestHelper::ToMicros(2024, 1, 1, 0, 0, 0),
        TimestampDiffTestHelper::ToMicros(2024, 1, 1, 0, 0, 0),
        TimestampDiffTestHelper::ToMicros(2024, 1, 1, 0, 0, 0)
    };

    std::string unitStr = "DAY";
    BaseVector* unitVec = new ConstVector<std::string_view>(std::string_view(unitStr), OMNI_VARCHAR, rowSize);
    BaseVector* ts1Vec = TimestampDiffTestHelper::CreateTimestampVector(ts1);
    BaseVector* ts2Vec = TimestampDiffTestHelper::CreateTimestampVector(ts2);

    // Set middle ts1 to NULL
    ts1Vec->SetNull(1);

    BaseVector* result = nullptr;
    TimestampDiffTestHelper::ExecuteTimestampDiff(unitVec, ts1Vec, ts2Vec, result);

    EXPECT_FALSE(result->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(result->IsNull(1)) << "Row 1 should be NULL (ts1 is NULL)";
    EXPECT_FALSE(result->IsNull(2)) << "Row 2 should not be NULL";

    delete result;
}

// Test: NULL ts2 -> result is NULL
TEST(TimestampDiffTest, NullTs2) {
    std::cout << "=== Test: NullTs2 ===" << std::endl;
    int32_t rowSize = 3;

    std::vector<int64_t> ts1 = {
        TimestampDiffTestHelper::ToMicros(2024, 1, 1, 0, 0, 0),
        TimestampDiffTestHelper::ToMicros(2024, 1, 1, 0, 0, 0),
        TimestampDiffTestHelper::ToMicros(2024, 1, 1, 0, 0, 0)
    };
    std::vector<int64_t> ts2 = {
        TimestampDiffTestHelper::ToMicros(2024, 1, 1, 0, 0, 0),
        TimestampDiffTestHelper::ToMicros(2024, 1, 1, 0, 0, 0),
        TimestampDiffTestHelper::ToMicros(2024, 1, 1, 0, 0, 0)
    };

    std::string unitStr = "DAY";
    BaseVector* unitVec = new ConstVector<std::string_view>(std::string_view(unitStr), OMNI_VARCHAR, rowSize);
    BaseVector* ts1Vec = TimestampDiffTestHelper::CreateTimestampVector(ts1);
    BaseVector* ts2Vec = TimestampDiffTestHelper::CreateTimestampVector(ts2);

    // Set middle ts2 to NULL
    ts2Vec->SetNull(1);

    BaseVector* result = nullptr;
    TimestampDiffTestHelper::ExecuteTimestampDiff(unitVec, ts1Vec, ts2Vec, result);

    EXPECT_FALSE(result->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(result->IsNull(1)) << "Row 1 should be NULL (ts2 is NULL)";
    EXPECT_FALSE(result->IsNull(2)) << "Row 2 should not be NULL";

    delete result;
}

// Test: Invalid unit -> result is NULL
TEST(TimestampDiffTest, InvalidUnit) {
    std::cout << "=== Test: InvalidUnit ===" << std::endl;
    int32_t rowSize = 1;

    std::vector<int64_t> ts1 = {TimestampDiffTestHelper::ToMicros(2024, 1, 1, 0, 0, 0)};
    std::vector<int64_t> ts2 = {TimestampDiffTestHelper::ToMicros(2024, 1, 1, 0, 0, 0)};

    std::string unitStr = "INVALID";
    BaseVector* unitVec = new ConstVector<std::string_view>(std::string_view(unitStr), OMNI_VARCHAR, rowSize);
    BaseVector* ts1Vec = TimestampDiffTestHelper::CreateTimestampVector(ts1);
    BaseVector* ts2Vec = TimestampDiffTestHelper::CreateTimestampVector(ts2);
    BaseVector* result = nullptr;

    TimestampDiffTestHelper::ExecuteTimestampDiff(unitVec, ts1Vec, ts2Vec, result);

    EXPECT_TRUE(result->IsNull(0)) << "Row 0 should be NULL (invalid unit)";

    delete result;
}

// Test: Case-insensitive unit
TEST(TimestampDiffTest, CaseInsensitiveUnit) {
    std::cout << "=== Test: CaseInsensitiveUnit ===" << std::endl;
    int32_t rowSize = 1;

    std::vector<int64_t> ts1 = {TimestampDiffTestHelper::ToMicros(2024, 1, 1, 0, 1, 0)};
    std::vector<int64_t> ts2 = {TimestampDiffTestHelper::ToMicros(2024, 1, 1, 0, 0, 0)};
    std::vector<int64_t> expected = {60};

    std::string unitStr = "second";
    BaseVector* unitVec = new ConstVector<std::string_view>(std::string_view(unitStr), OMNI_VARCHAR, rowSize);
    BaseVector* ts1Vec = TimestampDiffTestHelper::CreateTimestampVector(ts1);
    BaseVector* ts2Vec = TimestampDiffTestHelper::CreateTimestampVector(ts2);
    BaseVector* result = nullptr;

    TimestampDiffTestHelper::ExecuteTimestampDiff(unitVec, ts1Vec, ts2Vec, result);
    TimestampDiffTestHelper::ValidateResult(result, expected, rowSize);

    delete result;
}

// Test: Integer truncation for time-based units
TEST(TimestampDiffTest, IntegerTruncation) {
    std::cout << "=== Test: IntegerTruncation ===" << std::endl;
    int32_t rowSize = 2;

    // 90 seconds = 1 minute (truncated toward zero)
    // 90 seconds = 0 hours (truncated toward zero)
    std::vector<int64_t> ts1 = {
        TimestampDiffTestHelper::ToMicros(2024, 1, 1, 0, 1, 30),
        TimestampDiffTestHelper::ToMicros(2024, 1, 1, 0, 1, 30)
    };
    std::vector<int64_t> ts2 = {
        TimestampDiffTestHelper::ToMicros(2024, 1, 1, 0, 0, 0),
        TimestampDiffTestHelper::ToMicros(2024, 1, 1, 0, 0, 0)
    };
    std::vector<int64_t> expectedMinutes = {1};
    std::vector<int64_t> expectedHours = {0};

    // Test MINUTE truncation
    {
        std::string unitStr = "MINUTE";
        BaseVector* unitVec = new ConstVector<std::string_view>(std::string_view(unitStr), OMNI_VARCHAR, rowSize);
        BaseVector* ts1Vec = TimestampDiffTestHelper::CreateTimestampVector(ts1);
        BaseVector* ts2Vec = TimestampDiffTestHelper::CreateTimestampVector(ts2);
        BaseVector* result = nullptr;

        TimestampDiffTestHelper::ExecuteTimestampDiff(unitVec, ts1Vec, ts2Vec, result);
        TimestampDiffTestHelper::ValidateResult(result, expectedMinutes, 1);

        delete result;
    }

    // Test HOUR truncation
    {
        std::string unitStr = "HOUR";
        BaseVector* unitVec = new ConstVector<std::string_view>(std::string_view(unitStr), OMNI_VARCHAR, rowSize);
        BaseVector* ts1Vec = TimestampDiffTestHelper::CreateTimestampVector(ts1);
        BaseVector* ts2Vec = TimestampDiffTestHelper::CreateTimestampVector(ts2);
        BaseVector* result = nullptr;

        TimestampDiffTestHelper::ExecuteTimestampDiff(unitVec, ts1Vec, ts2Vec, result);
        TimestampDiffTestHelper::ValidateResult(result, expectedHours, 1);

        delete result;
    }
}
