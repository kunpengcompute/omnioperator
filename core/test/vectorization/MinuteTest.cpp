/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: Minute function unit tests
 */

#include <gtest/gtest.h>
#include <iostream>
#include <vector>
#include <ctime>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/functions/Minute.h"
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
class MinuteTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const minute_test_env = ::testing::AddGlobalTestEnvironment(new MinuteTestEnvironment);

class MinuteFunctionTestHelper {
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
    
    static BaseVector* CreateTimestampVector(const std::vector<int64_t>& values) {
        BaseVector* vec = VectorHelper::CreateFlatVector(nullptr, OMNI_TIMESTAMP, values.size());
        auto* typedVec = static_cast<Vector<int64_t>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typedVec->SetValue(i, values[i]);
        }
        return vec;
    }
    
    static void ExecuteMinute(BaseVector* inputVec, DataTypeId inputTypeId, BaseVector*& result) {
        auto signature = std::make_shared<FunctionSignature>("minute", 
            std::vector<DataTypeId>{inputTypeId}, OMNI_INT);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "Minute function not found for signature";
        
        auto outputType = std::make_shared<DataType>(OMNI_INT);
        ExecutionContext context(4096, nullptr);
        context.SetResultRowSize(inputVec->GetSize());
        std::stack<BaseVector*> args;
        args.push(inputVec);
        
        ASSERT_NO_THROW(function->Apply(args, outputType, result, &context))
            << "Minute function threw an exception";
    }
    
    // Helper to convert timestamp components to microseconds since epoch
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
};

// Test: Minute from timestamp - basic cases
TEST(MinuteTest, TimestampBasic) {
    std::cout << "=== Test: Minute from TIMESTAMP - basic cases ===" << std::endl;
    
    // Create timestamps: 2024-01-01 12:30:45, 2024-01-01 15:45:20, 2024-01-01 00:00:00
    std::vector<int64_t> timestampValues = {
        MinuteFunctionTestHelper::TimestampToMicros(2024, 1, 1, 12, 30, 45),
        MinuteFunctionTestHelper::TimestampToMicros(2024, 1, 1, 15, 45, 20),
        MinuteFunctionTestHelper::TimestampToMicros(2024, 1, 1, 0, 0, 0)
    };
    std::vector<int32_t> expected = {30, 45, 0};
    
    BaseVector* inputVec = MinuteFunctionTestHelper::CreateTimestampVector(timestampValues);
    BaseVector* resultVec = nullptr;
    MinuteFunctionTestHelper::ExecuteMinute(inputVec, OMNI_TIMESTAMP, resultVec);
    MinuteFunctionTestHelper::ValidateResult(resultVec, expected, timestampValues.size());

    delete resultVec;
}

// Test: Minute from timestamp - edge cases (59 minutes)
TEST(MinuteTest, TimestampEdgeCases) {
    std::cout << "=== Test: Minute from TIMESTAMP - edge cases ===" << std::endl;
    
    // Create timestamps with 59 minutes
    std::vector<int64_t> timestampValues = {
        MinuteFunctionTestHelper::TimestampToMicros(2024, 1, 1, 10, 59, 0),
        MinuteFunctionTestHelper::TimestampToMicros(2024, 1, 1, 23, 59, 59),
        MinuteFunctionTestHelper::TimestampToMicros(2024, 1, 1, 0, 1, 0)
    };
    std::vector<int32_t> expected = {59, 59, 1};
    
    BaseVector* inputVec = MinuteFunctionTestHelper::CreateTimestampVector(timestampValues);
    BaseVector* resultVec = nullptr;
    MinuteFunctionTestHelper::ExecuteMinute(inputVec, OMNI_TIMESTAMP, resultVec);
    MinuteFunctionTestHelper::ValidateResult(resultVec, expected, timestampValues.size());

    delete resultVec;
}

// Test: Minute from timestamp with NULL values
TEST(MinuteTest, TimestampWithNull) {
    std::cout << "=== Test: Minute from TIMESTAMP with NULL values ===" << std::endl;
    
    std::vector<int64_t> timestampValues = {
        MinuteFunctionTestHelper::TimestampToMicros(2024, 1, 1, 12, 30, 45),
        MinuteFunctionTestHelper::TimestampToMicros(2024, 1, 1, 15, 45, 20),
        MinuteFunctionTestHelper::TimestampToMicros(2024, 1, 1, 0, 0, 0)
    };
    
    BaseVector* inputVec = MinuteFunctionTestHelper::CreateTimestampVector(timestampValues);
    // Set middle value to NULL
    inputVec->SetNull(1);
    
    BaseVector* resultVec = nullptr;
    MinuteFunctionTestHelper::ExecuteMinute(inputVec, OMNI_TIMESTAMP, resultVec);
    
    // First and third should have values, second should be NULL
    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";
    
    // Validate non-null values
    auto* resultVecTyped = dynamic_cast<Vector<int32_t>*>(resultVec);
    EXPECT_EQ(resultVecTyped->GetValue(0), 30) << "Row 0 minute should be 30";
    EXPECT_EQ(resultVecTyped->GetValue(2), 0) << "Row 2 minute should be 0";

    delete resultVec;
}

// Test: Minute from timestamp - all minutes (0-59)
TEST(MinuteTest, TimestampAllMinutes) {
    std::cout << "=== Test: Minute from TIMESTAMP - all minutes (0-59) ===" << std::endl;
    
    std::vector<int64_t> timestampValues;
    std::vector<int32_t> expected;
    
    for (int m = 0; m < 60; ++m) {
        timestampValues.push_back(MinuteFunctionTestHelper::TimestampToMicros(2024, 1, 1, 12, m, 0));
        expected.push_back(m);
    }
    
    BaseVector* inputVec = MinuteFunctionTestHelper::CreateTimestampVector(timestampValues);
    BaseVector* resultVec = nullptr;
    MinuteFunctionTestHelper::ExecuteMinute(inputVec, OMNI_TIMESTAMP, resultVec);
    MinuteFunctionTestHelper::ValidateResult(resultVec, expected, timestampValues.size());

    delete resultVec;
}
