/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: Second function unit tests
 */

#include <gtest/gtest.h>
#include <iostream>
#include <vector>
#include <ctime>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/functions/Second.h"
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

class SecondTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const second_test_env = ::testing::AddGlobalTestEnvironment(new SecondTestEnvironment);

class SecondFunctionTestHelper {
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

    static void ValidateDecimal64Result(BaseVector* result, const std::vector<int64_t>& expected, int rowSize) {
        auto* resultVec = dynamic_cast<Vector<int64_t>*>(result);
        ASSERT_NE(resultVec, nullptr) << "Result vector type mismatch (expected int64 for DECIMAL64)";

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

    static BaseVector* CreateTimestampVector(const std::vector<int64_t>& values) {
        BaseVector* vec = VectorHelper::CreateFlatVector(nullptr, OMNI_TIMESTAMP, values.size());
        auto* typedVec = static_cast<Vector<int64_t>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typedVec->SetValue(i, values[i]);
        }
        return vec;
    }

    static void ExecuteSecond(BaseVector* inputVec, DataTypeId inputTypeId, BaseVector*& result) {
        auto signature = std::make_shared<FunctionSignature>("second",
            std::vector<DataTypeId>{inputTypeId}, OMNI_INT);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "Second function not found for signature";

        auto outputType = std::make_shared<DataType>(OMNI_INT);
        ExecutionContext context(4096, nullptr);
        context.SetResultRowSize(inputVec->GetSize());
        std::stack<BaseVector*> args;
        args.push(inputVec);

        ASSERT_NO_THROW(function->Apply(args, outputType, result, &context))
            << "Second function threw an exception";
    }

    static void ExecuteSecondWithFraction(BaseVector* inputVec, DataTypeId inputTypeId, BaseVector*& result) {
        auto signature = std::make_shared<FunctionSignature>("second",
            std::vector<DataTypeId>{inputTypeId}, OMNI_DECIMAL64);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "SecondWithFraction function not found for signature";

        auto outputType = std::make_shared<Decimal64DataType>(8, 6);
        ExecutionContext context(4096, nullptr);
        context.SetResultRowSize(inputVec->GetSize());
        std::stack<BaseVector*> args;
        args.push(inputVec);

        ASSERT_NO_THROW(function->Apply(args, outputType, result, &context))
            << "SecondWithFraction function threw an exception";
    }

    static int64_t TimestampToMicros(int year, int month, int day, int hour, int minute, int second) {
        std::tm tm = {};
        tm.tm_year = year - 1900;
        tm.tm_mon = month - 1;
        tm.tm_mday = day;
        tm.tm_hour = hour;
        tm.tm_min = minute;
        tm.tm_sec = second;
        tm.tm_isdst = -1;

        std::time_t time = std::mktime(&tm);
        if (time == -1) {
            return 0;
        }
        return static_cast<int64_t>(time) * 1000000;
    }

    static int64_t TimestampToMicrosUtc(int year, int month, int day, int hour, int minute, int second) {
        std::tm tm = {};
        tm.tm_year = year - 1900;
        tm.tm_mon = month - 1;
        tm.tm_mday = day;
        tm.tm_hour = hour;
        tm.tm_min = minute;
        tm.tm_sec = second;
        tm.tm_isdst = 0;
        int64_t seconds = Timestamp::calendarUtcToEpoch(tm);
        return seconds * 1000000;
    }
};

// Test: Second from timestamp - basic cases
TEST(SecondTest, TimestampBasic) {
    std::cout << "=== Test: Second from TIMESTAMP - basic cases ===" << std::endl;
    
    // Create timestamps: 2024-01-01 12:30:45, 2024-01-01 15:45:20, 2024-01-01 00:00:00
    std::vector<int64_t> timestampValues = {
        SecondFunctionTestHelper::TimestampToMicros(2024, 1, 1, 12, 30, 45),
        SecondFunctionTestHelper::TimestampToMicros(2024, 1, 1, 15, 45, 20),
        SecondFunctionTestHelper::TimestampToMicros(2024, 1, 1, 0, 0, 0)
    };
    std::vector<int32_t> expected = {45, 20, 0};
    
    BaseVector* inputVec = SecondFunctionTestHelper::CreateTimestampVector(timestampValues);
    BaseVector* resultVec = nullptr;
    SecondFunctionTestHelper::ExecuteSecond(inputVec, OMNI_TIMESTAMP, resultVec);
    SecondFunctionTestHelper::ValidateResult(resultVec, expected, timestampValues.size());

    delete resultVec;
}

// Test: Second from timestamp - edge cases (59 seconds)
TEST(SecondTest, TimestampEdgeCases) {
    std::cout << "=== Test: Second from TIMESTAMP - edge cases ===" << std::endl;
    
    // Create timestamps with 59 seconds
    std::vector<int64_t> timestampValues = {
        SecondFunctionTestHelper::TimestampToMicros(2024, 1, 1, 10, 30, 59),
        SecondFunctionTestHelper::TimestampToMicros(2024, 1, 1, 23, 59, 59),
        SecondFunctionTestHelper::TimestampToMicros(2024, 1, 1, 0, 0, 1)
    };
    std::vector<int32_t> expected = {59, 59, 1};
    
    BaseVector* inputVec = SecondFunctionTestHelper::CreateTimestampVector(timestampValues);
    BaseVector* resultVec = nullptr;
    SecondFunctionTestHelper::ExecuteSecond(inputVec, OMNI_TIMESTAMP, resultVec);
    SecondFunctionTestHelper::ValidateResult(resultVec, expected, timestampValues.size());

    delete resultVec;
}

// Test: Second from timestamp with NULL values
TEST(SecondTest, TimestampWithNull) {
    std::cout << "=== Test: Second from TIMESTAMP with NULL values ===" << std::endl;
    
    std::vector<int64_t> timestampValues = {
        SecondFunctionTestHelper::TimestampToMicros(2024, 1, 1, 12, 30, 45),
        SecondFunctionTestHelper::TimestampToMicros(2024, 1, 1, 15, 45, 20),
        SecondFunctionTestHelper::TimestampToMicros(2024, 1, 1, 0, 0, 0)
    };
    
    BaseVector* inputVec = SecondFunctionTestHelper::CreateTimestampVector(timestampValues);
    // Set middle value to NULL
    inputVec->SetNull(1);
    
    BaseVector* resultVec = nullptr;
    SecondFunctionTestHelper::ExecuteSecond(inputVec, OMNI_TIMESTAMP, resultVec);
    
    // First and third should have values, second should be NULL
    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";
    
    // Validate non-null values
    auto* resultVecTyped = dynamic_cast<Vector<int32_t>*>(resultVec);
    EXPECT_EQ(resultVecTyped->GetValue(0), 45) << "Row 0 second should be 45";
    EXPECT_EQ(resultVecTyped->GetValue(2), 0) << "Row 2 second should be 0";

    delete resultVec;
}

// Test: Second from timestamp - all seconds (0-59)
TEST(SecondTest, TimestampAllSeconds) {
    std::cout << "=== Test: Second from TIMESTAMP - all seconds (0-59) ===" << std::endl;

    std::vector<int64_t> timestampValues;
    std::vector<int32_t> expected;

    for (int s = 0; s < 60; ++s) {
        timestampValues.push_back(SecondFunctionTestHelper::TimestampToMicros(2024, 1, 1, 12, 30, s));
        expected.push_back(s);
    }

    BaseVector* inputVec = SecondFunctionTestHelper::CreateTimestampVector(timestampValues);
    BaseVector* resultVec = nullptr;
    SecondFunctionTestHelper::ExecuteSecond(inputVec, OMNI_TIMESTAMP, resultVec);
    SecondFunctionTestHelper::ValidateResult(resultVec, expected, timestampValues.size());

    delete resultVec;
}

// ========================= SecondWithFraction tests =========================
// extract(SECOND FROM ...) in Spark produces SecondWithFraction → Decimal(8,6)
// Underlying int64 value = second_of_minute * 1000000 + microsecond_fraction

// Test: SecondWithFraction function signature can be found
TEST(SecondWithFractionTest, SignatureFound) {
    auto signature = std::make_shared<FunctionSignature>("second",
        std::vector<DataTypeId>{OMNI_TIMESTAMP}, OMNI_DECIMAL64);
    auto function = VectorFunction::Find(signature);
    ASSERT_NE(function, nullptr) << "SecondWithFraction function not found";
}

// Test: SecondWithFraction from timestamp - basic integer seconds (no fractional micros)
TEST(SecondWithFractionTest, TimestampBasicNoFraction) {
    std::cout << "=== Test: SecondWithFraction from TIMESTAMP - basic (no sub-second) ===" << std::endl;

    // 12:30:45.000000 → 45000000, 15:45:20.000000 → 20000000, 00:00:00.000000 → 0
    std::vector<int64_t> timestampValues = {
        SecondFunctionTestHelper::TimestampToMicrosUtc(2024, 1, 1, 12, 30, 45),
        SecondFunctionTestHelper::TimestampToMicrosUtc(2024, 1, 1, 15, 45, 20),
        SecondFunctionTestHelper::TimestampToMicrosUtc(2024, 1, 1, 0, 0, 0)
    };
    std::vector<int64_t> expected = {45000000, 20000000, 0};

    BaseVector* inputVec = SecondFunctionTestHelper::CreateTimestampVector(timestampValues);
    BaseVector* resultVec = nullptr;
    SecondFunctionTestHelper::ExecuteSecondWithFraction(inputVec, OMNI_TIMESTAMP, resultVec);
    SecondFunctionTestHelper::ValidateDecimal64Result(resultVec, expected, timestampValues.size());

    delete resultVec;
}

// Test: SecondWithFraction from timestamp - with microsecond fractions
TEST(SecondWithFractionTest, TimestampWithMicroseconds) {
    std::cout << "=== Test: SecondWithFraction from TIMESTAMP - with microseconds ===" << std::endl;

    // Base: 2024-01-01 12:30:30 UTC → add fractional micros manually
    int64_t base = SecondFunctionTestHelper::TimestampToMicrosUtc(2024, 1, 1, 12, 30, 30);
    std::vector<int64_t> timestampValues = {
        base + 123456,  // 30.123456s → 30123456
        base + 0,       // 30.000000s → 30000000
        base + 999999   // 30.999999s → 30999999
    };
    std::vector<int64_t> expected = {30123456, 30000000, 30999999};

    BaseVector* inputVec = SecondFunctionTestHelper::CreateTimestampVector(timestampValues);
    BaseVector* resultVec = nullptr;
    SecondFunctionTestHelper::ExecuteSecondWithFraction(inputVec, OMNI_TIMESTAMP, resultVec);
    SecondFunctionTestHelper::ValidateDecimal64Result(resultVec, expected, timestampValues.size());

    delete resultVec;
}

// Test: SecondWithFraction with NULL values
TEST(SecondWithFractionTest, TimestampWithNull) {
    std::cout << "=== Test: SecondWithFraction from TIMESTAMP with NULL values ===" << std::endl;

    std::vector<int64_t> timestampValues = {
        SecondFunctionTestHelper::TimestampToMicrosUtc(2024, 1, 1, 12, 30, 45),
        SecondFunctionTestHelper::TimestampToMicrosUtc(2024, 1, 1, 15, 45, 20),
        SecondFunctionTestHelper::TimestampToMicrosUtc(2024, 1, 1, 0, 0, 0)
    };

    BaseVector* inputVec = SecondFunctionTestHelper::CreateTimestampVector(timestampValues);
    inputVec->SetNull(1);

    BaseVector* resultVec = nullptr;
    SecondFunctionTestHelper::ExecuteSecondWithFraction(inputVec, OMNI_TIMESTAMP, resultVec);

    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";

    auto* resultVecTyped = dynamic_cast<Vector<int64_t>*>(resultVec);
    EXPECT_EQ(resultVecTyped->GetValue(0), 45000000) << "Row 0 should be 45.000000";
    EXPECT_EQ(resultVecTyped->GetValue(2), 0) << "Row 2 should be 0.000000";

    delete resultVec;
}

// Test: SecondWithFraction - all NULLs
TEST(SecondWithFractionTest, TimestampAllNulls) {
    std::vector<int64_t> timestampValues = {0, 0, 0};

    BaseVector* inputVec = SecondFunctionTestHelper::CreateTimestampVector(timestampValues);
    inputVec->SetNull(0);
    inputVec->SetNull(1);
    inputVec->SetNull(2);

    BaseVector* resultVec = nullptr;
    SecondFunctionTestHelper::ExecuteSecondWithFraction(inputVec, OMNI_TIMESTAMP, resultVec);

    EXPECT_TRUE(resultVec->IsNull(0)) << "Row 0 should be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";
    EXPECT_TRUE(resultVec->IsNull(2)) << "Row 2 should be NULL";

    delete resultVec;
}

// Test: SecondWithFraction - edge case: 59 seconds
TEST(SecondWithFractionTest, TimestampEdge59Seconds) {
    std::cout << "=== Test: SecondWithFraction - edge: 59 seconds ===" << std::endl;

    int64_t ts59 = SecondFunctionTestHelper::TimestampToMicrosUtc(2024, 1, 1, 23, 59, 59);
    std::vector<int64_t> timestampValues = { ts59 + 999999 };
    std::vector<int64_t> expected = { 59999999 };  // 59.999999

    BaseVector* inputVec = SecondFunctionTestHelper::CreateTimestampVector(timestampValues);
    BaseVector* resultVec = nullptr;
    SecondFunctionTestHelper::ExecuteSecondWithFraction(inputVec, OMNI_TIMESTAMP, resultVec);
    SecondFunctionTestHelper::ValidateDecimal64Result(resultVec, expected, timestampValues.size());

    delete resultVec;
}

// Test: SecondWithFraction - epoch timestamp (1970-01-01 00:00:00)
TEST(SecondWithFractionTest, TimestampEpoch) {
    std::cout << "=== Test: SecondWithFraction - epoch ===" << std::endl;

    std::vector<int64_t> timestampValues = { 0 };  // epoch = 1970-01-01 00:00:00.000000
    std::vector<int64_t> expected = { 0 };           // 0.000000

    BaseVector* inputVec = SecondFunctionTestHelper::CreateTimestampVector(timestampValues);
    BaseVector* resultVec = nullptr;
    SecondFunctionTestHelper::ExecuteSecondWithFraction(inputVec, OMNI_TIMESTAMP, resultVec);
    SecondFunctionTestHelper::ValidateDecimal64Result(resultVec, expected, timestampValues.size());

    delete resultVec;
}

// Test: Original SecondFunction (INT return) still works after adding SecondWithFraction
TEST(SecondWithFractionTest, OriginalSecondStillWorks) {
    std::cout << "=== Test: Original Second(TIMESTAMP->INT) still works ===" << std::endl;

    std::vector<int64_t> timestampValues = {
        SecondFunctionTestHelper::TimestampToMicros(2024, 1, 1, 12, 30, 45),
        SecondFunctionTestHelper::TimestampToMicros(2024, 1, 1, 0, 0, 0)
    };
    std::vector<int32_t> expected = {45, 0};

    BaseVector* inputVec = SecondFunctionTestHelper::CreateTimestampVector(timestampValues);
    BaseVector* resultVec = nullptr;
    SecondFunctionTestHelper::ExecuteSecond(inputVec, OMNI_TIMESTAMP, resultVec);
    SecondFunctionTestHelper::ValidateResult(resultVec, expected, timestampValues.size());

    delete resultVec;
}
