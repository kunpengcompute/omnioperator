/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: TimestampAdd function unit tests
 */

#include <gtest/gtest.h>
#include <iostream>
#include <vector>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/functions/TimestampAdd.h"
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
class TimestampAddTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const timestamp_add_test_env =
    ::testing::AddGlobalTestEnvironment(new TimestampAddTestEnvironment);

class TimestampAddTestHelper {
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

    /// Create a BIGINT (OMNI_LONG) flat vector from int64_t values
    static BaseVector* CreateLongVector(const std::vector<int64_t>& values)
    {
        BaseVector* vec = VectorHelper::CreateFlatVector(OMNI_LONG, values.size());
        auto* typedVec = static_cast<Vector<int64_t>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typedVec->SetValue(i, values[i]);
        }
        return vec;
    }

    /// Execute timestampadd(unit, interval, timestamp)
    static void ExecuteTimestampAdd(BaseVector* unitVec, BaseVector* intervalVec, BaseVector* timestampVec,
                                    BaseVector*& result)
    {
        auto signature = std::make_shared<FunctionSignature>("timestampadd",
            std::vector<DataTypeId>{OMNI_VARCHAR, OMNI_LONG, OMNI_TIMESTAMP}, OMNI_TIMESTAMP);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "TimestampAdd function not found for signature";

        auto outputType = std::make_shared<DataType>(OMNI_TIMESTAMP);
        ExecutionContext context;
        context.SetResultRowSize(timestampVec->GetSize());
        std::stack<BaseVector*> args;

        // Push order: timestamp first (bottom), interval, unit last (top)
        args.push(timestampVec);
        args.push(intervalVec);
        args.push(unitVec);

        ASSERT_NO_THROW(function->Apply(args, outputType, result, &context))
            << "TimestampAdd function threw an exception";
    }

    /// Validate result against expected microsecond values
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

// Test: Add seconds
TEST(TimestampAddTest, AddSeconds) {
    std::cout << "=== Test: AddSeconds ===" << std::endl;
    int32_t rowSize = 2;

    // 2024-01-01 00:00:00
    std::vector<int64_t> timestamps = {
        TimestampAddTestHelper::ToMicros(2024, 1, 1, 0, 0, 0),
        TimestampAddTestHelper::ToMicros(2024, 1, 1, 0, 0, 0)
    };
    std::vector<int64_t> intervals = {30, -30};
    // Expected: 2024-01-01 00:00:30, 2023-12-31 23:59:30
    std::vector<int64_t> expected = {
        TimestampAddTestHelper::ToMicros(2024, 1, 1, 0, 0, 30),
        TimestampAddTestHelper::ToMicros(2023, 12, 31, 23, 59, 30)
    };

    std::string unitStr = "SECOND";
    BaseVector* unitVec = new ConstVector<std::string_view>(std::string_view(unitStr), OMNI_VARCHAR, rowSize);
    BaseVector* intervalVec = TimestampAddTestHelper::CreateLongVector(intervals);
    BaseVector* timestampVec = TimestampAddTestHelper::CreateTimestampVector(timestamps);
    BaseVector* result = nullptr;

    TimestampAddTestHelper::ExecuteTimestampAdd(unitVec, intervalVec, timestampVec, result);
    TimestampAddTestHelper::ValidateResult(result, expected, rowSize);

    delete result;
}

// Test: Add minutes
TEST(TimestampAddTest, AddMinutes) {
    std::cout << "=== Test: AddMinutes ===" << std::endl;
    int32_t rowSize = 1;

    std::vector<int64_t> timestamps = {TimestampAddTestHelper::ToMicros(2024, 1, 1, 0, 0, 0)};
    std::vector<int64_t> intervals = {5};
    std::vector<int64_t> expected = {TimestampAddTestHelper::ToMicros(2024, 1, 1, 0, 5, 0)};

    std::string unitStr = "MINUTE";
    BaseVector* unitVec = new ConstVector<std::string_view>(std::string_view(unitStr), OMNI_VARCHAR, rowSize);
    BaseVector* intervalVec = TimestampAddTestHelper::CreateLongVector(intervals);
    BaseVector* timestampVec = TimestampAddTestHelper::CreateTimestampVector(timestamps);
    BaseVector* result = nullptr;

    TimestampAddTestHelper::ExecuteTimestampAdd(unitVec, intervalVec, timestampVec, result);
    TimestampAddTestHelper::ValidateResult(result, expected, rowSize);

    delete result;
}

// Test: Add hours
TEST(TimestampAddTest, AddHours) {
    std::cout << "=== Test: AddHours ===" << std::endl;
    int32_t rowSize = 1;

    std::vector<int64_t> timestamps = {TimestampAddTestHelper::ToMicros(2024, 1, 1, 0, 0, 0)};
    std::vector<int64_t> intervals = {2};
    std::vector<int64_t> expected = {TimestampAddTestHelper::ToMicros(2024, 1, 1, 2, 0, 0)};

    std::string unitStr = "HOUR";
    BaseVector* unitVec = new ConstVector<std::string_view>(std::string_view(unitStr), OMNI_VARCHAR, rowSize);
    BaseVector* intervalVec = TimestampAddTestHelper::CreateLongVector(intervals);
    BaseVector* timestampVec = TimestampAddTestHelper::CreateTimestampVector(timestamps);
    BaseVector* result = nullptr;

    TimestampAddTestHelper::ExecuteTimestampAdd(unitVec, intervalVec, timestampVec, result);
    TimestampAddTestHelper::ValidateResult(result, expected, rowSize);

    delete result;
}

// Test: Add days
TEST(TimestampAddTest, AddDays) {
    std::cout << "=== Test: AddDays ===" << std::endl;
    int32_t rowSize = 2;

    std::vector<int64_t> timestamps = {
        TimestampAddTestHelper::ToMicros(2024, 1, 1, 0, 0, 0),
        TimestampAddTestHelper::ToMicros(2024, 1, 10, 0, 0, 0)
    };
    std::vector<int64_t> intervals = {10, -5};
    std::vector<int64_t> expected = {
        TimestampAddTestHelper::ToMicros(2024, 1, 11, 0, 0, 0),
        TimestampAddTestHelper::ToMicros(2024, 1, 5, 0, 0, 0)
    };

    std::string unitStr = "DAY";
    BaseVector* unitVec = new ConstVector<std::string_view>(std::string_view(unitStr), OMNI_VARCHAR, rowSize);
    BaseVector* intervalVec = TimestampAddTestHelper::CreateLongVector(intervals);
    BaseVector* timestampVec = TimestampAddTestHelper::CreateTimestampVector(timestamps);
    BaseVector* result = nullptr;

    TimestampAddTestHelper::ExecuteTimestampAdd(unitVec, intervalVec, timestampVec, result);
    TimestampAddTestHelper::ValidateResult(result, expected, rowSize);

    delete result;
}

// Test: Add months - leap year (Jan 31 + 1 month = Feb 29)
TEST(TimestampAddTest, AddMonthsLeapYear) {
    std::cout << "=== Test: AddMonthsLeapYear ===" << std::endl;
    int32_t rowSize = 1;

    // 2024-01-31 (2024 is a leap year)
    std::vector<int64_t> timestamps = {TimestampAddTestHelper::ToMicros(2024, 1, 31, 0, 0, 0)};
    std::vector<int64_t> intervals = {1};
    // Expected: 2024-02-29
    std::vector<int64_t> expected = {TimestampAddTestHelper::ToMicros(2024, 2, 29, 0, 0, 0)};

    std::string unitStr = "MONTH";
    BaseVector* unitVec = new ConstVector<std::string_view>(std::string_view(unitStr), OMNI_VARCHAR, rowSize);
    BaseVector* intervalVec = TimestampAddTestHelper::CreateLongVector(intervals);
    BaseVector* timestampVec = TimestampAddTestHelper::CreateTimestampVector(timestamps);
    BaseVector* result = nullptr;

    TimestampAddTestHelper::ExecuteTimestampAdd(unitVec, intervalVec, timestampVec, result);
    TimestampAddTestHelper::ValidateResult(result, expected, rowSize);

    delete result;
}

// Test: Add months - non-leap year (Jan 31 + 1 month = Feb 28)
TEST(TimestampAddTest, AddMonthsNonLeapYear) {
    std::cout << "=== Test: AddMonthsNonLeapYear ===" << std::endl;
    int32_t rowSize = 1;

    // 2023-01-31 (2023 is not a leap year)
    std::vector<int64_t> timestamps = {TimestampAddTestHelper::ToMicros(2023, 1, 31, 0, 0, 0)};
    std::vector<int64_t> intervals = {1};
    // Expected: 2023-02-28
    std::vector<int64_t> expected = {TimestampAddTestHelper::ToMicros(2023, 2, 28, 0, 0, 0)};

    std::string unitStr = "MONTH";
    BaseVector* unitVec = new ConstVector<std::string_view>(std::string_view(unitStr), OMNI_VARCHAR, rowSize);
    BaseVector* intervalVec = TimestampAddTestHelper::CreateLongVector(intervals);
    BaseVector* timestampVec = TimestampAddTestHelper::CreateTimestampVector(timestamps);
    BaseVector* result = nullptr;

    TimestampAddTestHelper::ExecuteTimestampAdd(unitVec, intervalVec, timestampVec, result);
    TimestampAddTestHelper::ValidateResult(result, expected, rowSize);

    delete result;
}

// Test: Add year - leap year Feb 29 + 1 year = Feb 28
TEST(TimestampAddTest, AddYearLeapToNonLeap) {
    std::cout << "=== Test: AddYearLeapToNonLeap ===" << std::endl;
    int32_t rowSize = 1;

    // 2024-02-29 (leap year)
    std::vector<int64_t> timestamps = {TimestampAddTestHelper::ToMicros(2024, 2, 29, 0, 0, 0)};
    std::vector<int64_t> intervals = {1};
    // Expected: 2025-02-28 (2025 is not a leap year)
    std::vector<int64_t> expected = {TimestampAddTestHelper::ToMicros(2025, 2, 28, 0, 0, 0)};

    std::string unitStr = "YEAR";
    BaseVector* unitVec = new ConstVector<std::string_view>(std::string_view(unitStr), OMNI_VARCHAR, rowSize);
    BaseVector* intervalVec = TimestampAddTestHelper::CreateLongVector(intervals);
    BaseVector* timestampVec = TimestampAddTestHelper::CreateTimestampVector(timestamps);
    BaseVector* result = nullptr;

    TimestampAddTestHelper::ExecuteTimestampAdd(unitVec, intervalVec, timestampVec, result);
    TimestampAddTestHelper::ValidateResult(result, expected, rowSize);

    delete result;
}

// Test: Zero interval (no change)
TEST(TimestampAddTest, ZeroInterval) {
    std::cout << "=== Test: ZeroInterval ===" << std::endl;
    int32_t rowSize = 1;

    std::vector<int64_t> timestamps = {TimestampAddTestHelper::ToMicros(2024, 1, 1, 0, 0, 0)};
    std::vector<int64_t> intervals = {0};
    std::vector<int64_t> expected = {TimestampAddTestHelper::ToMicros(2024, 1, 1, 0, 0, 0)};

    std::string unitStr = "HOUR";
    BaseVector* unitVec = new ConstVector<std::string_view>(std::string_view(unitStr), OMNI_VARCHAR, rowSize);
    BaseVector* intervalVec = TimestampAddTestHelper::CreateLongVector(intervals);
    BaseVector* timestampVec = TimestampAddTestHelper::CreateTimestampVector(timestamps);
    BaseVector* result = nullptr;

    TimestampAddTestHelper::ExecuteTimestampAdd(unitVec, intervalVec, timestampVec, result);
    TimestampAddTestHelper::ValidateResult(result, expected, rowSize);

    delete result;
}

// Test: NULL timestamp -> result is NULL
TEST(TimestampAddTest, NullTimestamp) {
    std::cout << "=== Test: NullTimestamp ===" << std::endl;
    int32_t rowSize = 3;

    std::vector<int64_t> timestamps = {
        TimestampAddTestHelper::ToMicros(2024, 1, 1, 0, 0, 0),
        TimestampAddTestHelper::ToMicros(2024, 1, 1, 0, 0, 0),
        TimestampAddTestHelper::ToMicros(2024, 1, 1, 0, 0, 0)
    };
    std::vector<int64_t> intervals = {1, 1, 1};

    std::string unitStr = "DAY";
    BaseVector* unitVec = new ConstVector<std::string_view>(std::string_view(unitStr), OMNI_VARCHAR, rowSize);
    BaseVector* intervalVec = TimestampAddTestHelper::CreateLongVector(intervals);
    BaseVector* timestampVec = TimestampAddTestHelper::CreateTimestampVector(timestamps);

    // Set middle timestamp to NULL
    timestampVec->SetNull(1);

    BaseVector* result = nullptr;
    TimestampAddTestHelper::ExecuteTimestampAdd(unitVec, intervalVec, timestampVec, result);

    EXPECT_FALSE(result->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(result->IsNull(1)) << "Row 1 should be NULL (timestamp is NULL)";
    EXPECT_FALSE(result->IsNull(2)) << "Row 2 should not be NULL";

    delete result;
}

// Test: NULL interval -> result is NULL
TEST(TimestampAddTest, NullInterval) {
    std::cout << "=== Test: NullInterval ===" << std::endl;
    int32_t rowSize = 3;

    std::vector<int64_t> timestamps = {
        TimestampAddTestHelper::ToMicros(2024, 1, 1, 0, 0, 0),
        TimestampAddTestHelper::ToMicros(2024, 1, 1, 0, 0, 0),
        TimestampAddTestHelper::ToMicros(2024, 1, 1, 0, 0, 0)
    };
    std::vector<int64_t> intervals = {1, 1, 1};

    std::string unitStr = "DAY";
    BaseVector* unitVec = new ConstVector<std::string_view>(std::string_view(unitStr), OMNI_VARCHAR, rowSize);
    BaseVector* intervalVec = TimestampAddTestHelper::CreateLongVector(intervals);
    BaseVector* timestampVec = TimestampAddTestHelper::CreateTimestampVector(timestamps);

    // Set middle interval to NULL
    intervalVec->SetNull(1);

    BaseVector* result = nullptr;
    TimestampAddTestHelper::ExecuteTimestampAdd(unitVec, intervalVec, timestampVec, result);

    EXPECT_FALSE(result->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(result->IsNull(1)) << "Row 1 should be NULL (interval is NULL)";
    EXPECT_FALSE(result->IsNull(2)) << "Row 2 should not be NULL";

    delete result;
}

// Test: Invalid unit -> result is NULL
TEST(TimestampAddTest, InvalidUnit) {
    std::cout << "=== Test: InvalidUnit ===" << std::endl;
    int32_t rowSize = 1;

    std::vector<int64_t> timestamps = {TimestampAddTestHelper::ToMicros(2024, 1, 1, 0, 0, 0)};
    std::vector<int64_t> intervals = {1};

    std::string unitStr = "INVALID";
    BaseVector* unitVec = new ConstVector<std::string_view>(std::string_view(unitStr), OMNI_VARCHAR, rowSize);
    BaseVector* intervalVec = TimestampAddTestHelper::CreateLongVector(intervals);
    BaseVector* timestampVec = TimestampAddTestHelper::CreateTimestampVector(timestamps);
    BaseVector* result = nullptr;

    TimestampAddTestHelper::ExecuteTimestampAdd(unitVec, intervalVec, timestampVec, result);

    EXPECT_TRUE(result->IsNull(0)) << "Row 0 should be NULL (invalid unit)";

    delete result;
}

// Test: Case-insensitive unit
TEST(TimestampAddTest, CaseInsensitiveUnit) {
    std::cout << "=== Test: CaseInsensitiveUnit ===" << std::endl;
    int32_t rowSize = 1;

    std::vector<int64_t> timestamps = {TimestampAddTestHelper::ToMicros(2024, 1, 1, 0, 0, 0)};
    std::vector<int64_t> intervals = {30};
    std::vector<int64_t> expected = {TimestampAddTestHelper::ToMicros(2024, 1, 1, 0, 0, 30)};

    std::string unitStr = "second";
    BaseVector* unitVec = new ConstVector<std::string_view>(std::string_view(unitStr), OMNI_VARCHAR, rowSize);
    BaseVector* intervalVec = TimestampAddTestHelper::CreateLongVector(intervals);
    BaseVector* timestampVec = TimestampAddTestHelper::CreateTimestampVector(timestamps);
    BaseVector* result = nullptr;

    TimestampAddTestHelper::ExecuteTimestampAdd(unitVec, intervalVec, timestampVec, result);
    TimestampAddTestHelper::ValidateResult(result, expected, rowSize);

    delete result;
}

// Test: Add months with negative interval
TEST(TimestampAddTest, AddMonthsNegative) {
    std::cout << "=== Test: AddMonthsNegative ===" << std::endl;
    int32_t rowSize = 1;

    // 2024-03-31 - 1 month = 2024-02-29 (leap year)
    std::vector<int64_t> timestamps = {TimestampAddTestHelper::ToMicros(2024, 3, 31, 0, 0, 0)};
    std::vector<int64_t> intervals = {-1};
    std::vector<int64_t> expected = {TimestampAddTestHelper::ToMicros(2024, 2, 29, 0, 0, 0)};

    std::string unitStr = "MONTH";
    BaseVector* unitVec = new ConstVector<std::string_view>(std::string_view(unitStr), OMNI_VARCHAR, rowSize);
    BaseVector* intervalVec = TimestampAddTestHelper::CreateLongVector(intervals);
    BaseVector* timestampVec = TimestampAddTestHelper::CreateTimestampVector(timestamps);
    BaseVector* result = nullptr;

    TimestampAddTestHelper::ExecuteTimestampAdd(unitVec, intervalVec, timestampVec, result);
    TimestampAddTestHelper::ValidateResult(result, expected, rowSize);

    delete result;
}

// Test: Sub-second precision preserved for MONTH/YEAR addition
TEST(TimestampAddTest, SubSecondPrecisionPreserved) {
    std::cout << "=== Test: SubSecondPrecisionPreserved ===" << std::endl;
    int32_t rowSize = 1;

    // 2024-01-15 10:30:45.123456 (with microsecond precision)
    int64_t baseMicros = TimestampAddTestHelper::ToMicros(2024, 1, 15, 10, 30, 45) + 123456;
    std::vector<int64_t> timestamps = {baseMicros};
    std::vector<int64_t> intervals = {1};
    // Expected: 2024-02-15 10:30:45.123456
    int64_t expectedMicros = TimestampAddTestHelper::ToMicros(2024, 2, 15, 10, 30, 45) + 123456;
    std::vector<int64_t> expected = {expectedMicros};

    std::string unitStr = "MONTH";
    BaseVector* unitVec = new ConstVector<std::string_view>(std::string_view(unitStr), OMNI_VARCHAR, rowSize);
    BaseVector* intervalVec = TimestampAddTestHelper::CreateLongVector(intervals);
    BaseVector* timestampVec = TimestampAddTestHelper::CreateTimestampVector(timestamps);
    BaseVector* result = nullptr;

    TimestampAddTestHelper::ExecuteTimestampAdd(unitVec, intervalVec, timestampVec, result);
    TimestampAddTestHelper::ValidateResult(result, expected, rowSize);

    delete result;
}
