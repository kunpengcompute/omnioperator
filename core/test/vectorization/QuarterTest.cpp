/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: Quarter function unit tests
 */

#include <gtest/gtest.h>
#include <iostream>
#include <vector>
#include <ctime>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/functions/Quarter.h"
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
class QuarterTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const quarter_test_env = ::testing::AddGlobalTestEnvironment(new QuarterTestEnvironment);

class QuarterFunctionTestHelper {
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
    
    static BaseVector* CreateDate32Vector(const std::vector<int32_t>& values) {
        BaseVector* vec = VectorHelper::CreateFlatVector(nullptr, OMNI_DATE32, values.size());
        auto* typedVec = static_cast<Vector<int32_t>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typedVec->SetValue(i, values[i]);
        }
        return vec;
    }
    
    static BaseVector* CreateIntVector(const std::vector<int32_t>& values) {
        BaseVector* vec = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, values.size());
        auto* typedVec = static_cast<Vector<int32_t>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typedVec->SetValue(i, values[i]);
        }
        return vec;
    }
    
    static void ExecuteQuarter(BaseVector* inputVec, DataTypeId inputTypeId, BaseVector*& result) {
        auto signature = std::make_shared<FunctionSignature>("quarter", 
            std::vector<DataTypeId>{inputTypeId}, OMNI_INT);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "Quarter function not found for signature";
        
        auto outputType = std::make_shared<DataType>(OMNI_INT);
        ExecutionContext context(4096, nullptr);
        context.SetResultRowSize(inputVec->GetSize());
        std::stack<BaseVector*> args;
        args.push(inputVec);
        
        ASSERT_NO_THROW(function->Apply(args, outputType, result, &context))
            << "Quarter function threw an exception";
    }
    
    // Helper to convert date components to days since epoch
    static int32_t DateToDays(int year, int month, int day) {
        std::tm tm = {};
        tm.tm_year = year - 1900;
        tm.tm_mon = month - 1;
        tm.tm_mday = day;
        tm.tm_hour = 0;
        tm.tm_min = 0;
        tm.tm_sec = 0;
        tm.tm_isdst = -1;
        
        std::time_t time = std::mktime(&tm);
        if (time == -1) {
            return 0;
        }
        // Convert to days since epoch (1970-01-01)
        return static_cast<int32_t>(time / 86400);
    }
    
    // Helper to calculate quarter from month (1-12)
    static int32_t MonthToQuarter(int month) {
        return (month - 1) / 3 + 1;
    }
};

// Test: Quarter from DATE32 - basic cases
TEST(QuarterTest, Date32Basic) {
    std::cout << "=== Test: Quarter from DATE32 - basic cases ===" << std::endl;
    
    // Create dates: Q1 (Jan), Q2 (Apr), Q3 (Jul), Q4 (Oct)
    std::vector<int32_t> dateValues = {
        QuarterFunctionTestHelper::DateToDays(2024, 1, 15),   // Q1
        QuarterFunctionTestHelper::DateToDays(2024, 4, 20),    // Q2
        QuarterFunctionTestHelper::DateToDays(2024, 7, 10),   // Q3
        QuarterFunctionTestHelper::DateToDays(2024, 10, 31)   // Q4
    };
    std::vector<int32_t> expected = {1, 2, 3, 4};
    
    BaseVector* inputVec = QuarterFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* resultVec = nullptr;
    QuarterFunctionTestHelper::ExecuteQuarter(inputVec, OMNI_DATE32, resultVec);
    QuarterFunctionTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

// Test: Quarter from INT - basic cases
TEST(QuarterTest, IntBasic) {
    std::cout << "=== Test: Quarter from INT - basic cases ===" << std::endl;
    
    // Create dates as days since epoch: Q1 (Jan), Q2 (Apr), Q3 (Jul), Q4 (Oct)
    std::vector<int32_t> intValues = {
        QuarterFunctionTestHelper::DateToDays(2024, 1, 15),   // Q1
        QuarterFunctionTestHelper::DateToDays(2024, 4, 20),    // Q2
        QuarterFunctionTestHelper::DateToDays(2024, 7, 10),   // Q3
        QuarterFunctionTestHelper::DateToDays(2024, 10, 31)   // Q4
    };
    std::vector<int32_t> expected = {1, 2, 3, 4};
    
    BaseVector* inputVec = QuarterFunctionTestHelper::CreateIntVector(intValues);
    BaseVector* resultVec = nullptr;
    QuarterFunctionTestHelper::ExecuteQuarter(inputVec, OMNI_INT, resultVec);
    QuarterFunctionTestHelper::ValidateResult(resultVec, expected, intValues.size());

    delete resultVec;
}

// Test: Quarter from DATE32 with NULL values
TEST(QuarterTest, Date32WithNull) {
    std::cout << "=== Test: Quarter from DATE32 with NULL values ===" << std::endl;
    
    std::vector<int32_t> dateValues = {
        QuarterFunctionTestHelper::DateToDays(2024, 1, 15),   // Q1
        QuarterFunctionTestHelper::DateToDays(2024, 4, 20),  // Q2
        QuarterFunctionTestHelper::DateToDays(2024, 7, 10)   // Q3
    };
    
    BaseVector* inputVec = QuarterFunctionTestHelper::CreateDate32Vector(dateValues);
    // Set middle value to NULL
    inputVec->SetNull(1);
    
    BaseVector* resultVec = nullptr;
    QuarterFunctionTestHelper::ExecuteQuarter(inputVec, OMNI_DATE32, resultVec);
    
    // First and third should have values, second should be NULL
    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";
    
    // Validate non-null values
    auto* resultVecTyped = dynamic_cast<Vector<int32_t>*>(resultVec);
    EXPECT_EQ(resultVecTyped->GetValue(0), 1) << "Row 0 quarter should be 1";
    EXPECT_EQ(resultVecTyped->GetValue(2), 3) << "Row 2 quarter should be 3";

    delete resultVec;
}

// Test: Quarter from INT with NULL values
TEST(QuarterTest, IntWithNull) {
    std::cout << "=== Test: Quarter from INT with NULL values ===" << std::endl;
    
    std::vector<int32_t> intValues = {
        QuarterFunctionTestHelper::DateToDays(2024, 1, 15),   // Q1
        QuarterFunctionTestHelper::DateToDays(2024, 4, 20),  // Q2
        QuarterFunctionTestHelper::DateToDays(2024, 7, 10)   // Q3
    };
    
    BaseVector* inputVec = QuarterFunctionTestHelper::CreateIntVector(intValues);
    // Set middle value to NULL
    inputVec->SetNull(1);
    
    BaseVector* resultVec = nullptr;
    QuarterFunctionTestHelper::ExecuteQuarter(inputVec, OMNI_INT, resultVec);
    
    // First and third should have values, second should be NULL
    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";
    
    // Validate non-null values
    auto* resultVecTyped = dynamic_cast<Vector<int32_t>*>(resultVec);
    EXPECT_EQ(resultVecTyped->GetValue(0), 1) << "Row 0 quarter should be 1";
    EXPECT_EQ(resultVecTyped->GetValue(2), 3) << "Row 2 quarter should be 3";

    delete resultVec;
}
