/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Time function unit tests
 */

#include <gtest/gtest.h>
#include <iostream>
#include <vector>
#include <string>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/functions/Time.h"
#include "vectorization/VectorFunction.h"
#include "codegen/func_signature.h"
#include "vector/vector_helper.h"
#include "vector/vector.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::codegen;
using namespace omniruntime::TestUtil;

// Initialize function registration before running tests
class TimeTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const time_test_env =
    ::testing::AddGlobalTestEnvironment(new TimeTestEnvironment);

class TimeTestHelper {
public:
    /// Create a VARCHAR flat vector from string values
    static BaseVector* CreateStringVector(const std::vector<std::string>& values)
    {
        BaseVector* vec = VectorHelper::CreateFlatVector(OMNI_VARCHAR, values.size());
        auto* typedVec = static_cast<Vector<LargeStringContainer<std::string_view>>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typedVec->SetValue(i, std::string_view(values[i]));
        }
        return vec;
    }

    /// Create a constant VARCHAR vector
    static BaseVector* CreateConstStringVector(const std::string& value, int32_t size)
    {
        return new ConstVector<std::string_view>(std::string_view(value), OMNI_VARCHAR, size);
    }

    /// Execute time(string)
    static void ExecuteTime(BaseVector* inputVec, BaseVector*& result)
    {
        auto signature = std::make_shared<FunctionSignature>("time",
            std::vector<DataTypeId>{OMNI_VARCHAR}, OMNI_TIME64);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "Time function not found for signature";

        auto outputType = std::make_shared<DataType>(OMNI_TIME64);
        ExecutionContext context;
        context.SetResultRowSize(inputVec->GetSize());
        std::stack<BaseVector*> args;
        args.push(inputVec);

        ASSERT_NO_THROW(function->Apply(args, outputType, result, &context))
            << "Time function threw an exception";
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

    /// Calculate expected microseconds from time components
    static int64_t ToMicros(int32_t hour, int32_t minute, int32_t second, int32_t micros = 0)
    {
        return static_cast<int64_t>(hour) * 3600000000LL +
               static_cast<int64_t>(minute) * 60000000LL +
               static_cast<int64_t>(second) * 1000000LL +
               micros;
    }
};

// Test: Basic time parsing
TEST(TimeTest, BasicTime) {
    std::cout << "=== Test: BasicTime ===" << std::endl;
    int32_t rowSize = 3;

    std::vector<std::string> inputs = {"12:30:45", "00:00:00", "23:59:59"};
    std::vector<int64_t> expected = {
        TimeTestHelper::ToMicros(12, 30, 45),
        TimeTestHelper::ToMicros(0, 0, 0),
        TimeTestHelper::ToMicros(23, 59, 59)
    };

    BaseVector* inputVec = TimeTestHelper::CreateStringVector(inputs);
    BaseVector* result = nullptr;

    TimeTestHelper::ExecuteTime(inputVec, result);
    TimeTestHelper::ValidateResult(result, expected, rowSize);

    delete result;
}

// Test: Time with fractional seconds
TEST(TimeTest, FractionalSeconds) {
    std::cout << "=== Test: FractionalSeconds ===" << std::endl;
    int32_t rowSize = 3;

    std::vector<std::string> inputs = {"12:30:45.123", "00:00:00.5", "23:59:59.999999"};
    std::vector<int64_t> expected = {
        TimeTestHelper::ToMicros(12, 30, 45, 123000),
        TimeTestHelper::ToMicros(0, 0, 0, 500000),
        TimeTestHelper::ToMicros(23, 59, 59, 999999)
    };

    BaseVector* inputVec = TimeTestHelper::CreateStringVector(inputs);
    BaseVector* result = nullptr;

    TimeTestHelper::ExecuteTime(inputVec, result);
    TimeTestHelper::ValidateResult(result, expected, rowSize);

    delete result;
}

// Test: Single digit hours/minutes/seconds
TEST(TimeTest, SingleDigitComponents) {
    std::cout << "=== Test: SingleDigitComponents ===" << std::endl;
    int32_t rowSize = 2;

    std::vector<std::string> inputs = {"9:05:03", "1:2:3"};
    std::vector<int64_t> expected = {
        TimeTestHelper::ToMicros(9, 5, 3),
        TimeTestHelper::ToMicros(1, 2, 3)
    };

    BaseVector* inputVec = TimeTestHelper::CreateStringVector(inputs);
    BaseVector* result = nullptr;

    TimeTestHelper::ExecuteTime(inputVec, result);
    TimeTestHelper::ValidateResult(result, expected, rowSize);

    delete result;
}

// Test: NULL input
TEST(TimeTest, NullInput) {
    std::cout << "=== Test: NullInput ===" << std::endl;
    int32_t rowSize = 3;

    std::vector<std::string> inputs = {"12:30:45", "00:00:00", "23:59:59"};

    BaseVector* inputVec = TimeTestHelper::CreateStringVector(inputs);
    // Set middle input to NULL
    inputVec->SetNull(1);

    BaseVector* result = nullptr;
    TimeTestHelper::ExecuteTime(inputVec, result);

    EXPECT_FALSE(result->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(result->IsNull(1)) << "Row 1 should be NULL (input is NULL)";
    EXPECT_FALSE(result->IsNull(2)) << "Row 2 should not be NULL";

    delete result;
}

// Test: Invalid format
TEST(TimeTest, InvalidFormat) {
    std::cout << "=== Test: InvalidFormat ===" << std::endl;
    int32_t rowSize = 5;

    std::vector<std::string> inputs = {
        "invalid",
        "12:30",       // Missing seconds
        "12:30:45:00", // Extra components
        "",            // Empty string
        "12-30-45"     // Wrong separator
    };

    BaseVector* inputVec = TimeTestHelper::CreateStringVector(inputs);
    BaseVector* result = nullptr;

    TimeTestHelper::ExecuteTime(inputVec, result);

    for (int i = 0; i < rowSize; ++i) {
        EXPECT_TRUE(result->IsNull(i)) << "Row " << i << " should be NULL (invalid format)";
    }

    delete result;
}

// Test: Out of range values
TEST(TimeTest, OutOfRange) {
    std::cout << "=== Test: OutOfRange ===" << std::endl;
    int32_t rowSize = 4;

    std::vector<std::string> inputs = {
        "25:00:00",    // Hour > 23
        "12:60:00",    // Minute > 59
        "12:30:60",    // Second > 59
        "12:30:45.9999999" // Too many fractional digits (should still work, truncated)
    };

    BaseVector* inputVec = TimeTestHelper::CreateStringVector(inputs);
    BaseVector* result = nullptr;

    TimeTestHelper::ExecuteTime(inputVec, result);

    // First 3 should be NULL (out of range)
    EXPECT_TRUE(result->IsNull(0)) << "Row 0 should be NULL (hour > 23)";
    EXPECT_TRUE(result->IsNull(1)) << "Row 1 should be NULL (minute > 59)";
    EXPECT_TRUE(result->IsNull(2)) << "Row 2 should be NULL (second > 59)";
    // Last one should succeed (extra digits truncated)
    EXPECT_FALSE(result->IsNull(3)) << "Row 3 should not be NULL (extra digits truncated)";

    delete result;
}

// Test: Midnight boundary
TEST(TimeTest, MidnightBoundary) {
    std::cout << "=== Test: MidnightBoundary ===" << std::endl;
    int32_t rowSize = 1;

    std::vector<std::string> inputs = {"00:00:00"};
    std::vector<int64_t> expected = {0};

    BaseVector* inputVec = TimeTestHelper::CreateStringVector(inputs);
    BaseVector* result = nullptr;

    TimeTestHelper::ExecuteTime(inputVec, result);
    TimeTestHelper::ValidateResult(result, expected, rowSize);

    delete result;
}

// Test: End of day boundary
TEST(TimeTest, EndOfDayBoundary) {
    std::cout << "=== Test: EndOfDayBoundary ===" << std::endl;
    int32_t rowSize = 1;

    std::vector<std::string> inputs = {"23:59:59"};
    std::vector<int64_t> expected = {TimeTestHelper::ToMicros(23, 59, 59)};

    BaseVector* inputVec = TimeTestHelper::CreateStringVector(inputs);
    BaseVector* result = nullptr;

    TimeTestHelper::ExecuteTime(inputVec, result);
    TimeTestHelper::ValidateResult(result, expected, rowSize);

    delete result;
}
