/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Day function unit tests
 */

#include <gtest/gtest.h>
#include <iostream>
#include <vector>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/functions/Day.h"
#include "vectorization/VectorFunction.h"
#include "codegen/func_signature.h"
#include "vector/vector_helper.h"
#include "vector/vector.h"
#include "type/date_time_utils.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::codegen;
using namespace omniruntime::TestUtil;

// Initialize function registration before running tests
class DayTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const day_test_env = ::testing::AddGlobalTestEnvironment(new DayTestEnvironment);

class DayFunctionTestHelper {
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
    
    static void ExecuteDay(BaseVector* inputVec, DataTypeId inputTypeId, BaseVector*& result) {
        auto signature = std::make_shared<FunctionSignature>("day", 
            std::vector<DataTypeId>{inputTypeId}, OMNI_INT);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "Day function not found for signature";
        
        auto outputType = std::make_shared<DataType>(OMNI_INT);
        ExecutionContext context(4096, nullptr);
        context.SetResultRowSize(inputVec->GetSize());
        std::stack<BaseVector*> args;
        args.push(inputVec);
        
        ASSERT_NO_THROW(function->Apply(args, outputType, result, &context))
            << "Day function threw an exception";
    }
    
    static void ExecuteDayOfMonth(BaseVector* inputVec, DataTypeId inputTypeId, BaseVector*& result) {
        auto signature = std::make_shared<FunctionSignature>("dayofmonth", 
            std::vector<DataTypeId>{inputTypeId}, OMNI_INT);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "DayOfMonth function not found for signature";
        
        auto outputType = std::make_shared<DataType>(OMNI_INT);
        ExecutionContext context(4096, nullptr);
        context.SetResultRowSize(inputVec->GetSize());
        std::stack<BaseVector*> args;
        args.push(inputVec);
        
        ASSERT_NO_THROW(function->Apply(args, outputType, result, &context))
            << "DayOfMonth function threw an exception";
    }
    
    // Helper to convert date components to days since epoch
    // Uses LocalDate to match the implementation's date calculation
    static int32_t DateToDays(int year, int month, int day) {
        LocalDate date(static_cast<int32_t>(year), static_cast<int16_t>(month), static_cast<int16_t>(day));
        return date.ToDays();
    }
};

// Test: Day from DATE32 - basic cases
TEST(DayTest, Date32Basic) {
    std::cout << "=== Test: Day from DATE32 - basic cases ===" << std::endl;
    
    // Create dates: 2024-01-15, 2024-06-20, 2024-12-31
    std::vector<int32_t> dateValues = {
        DayFunctionTestHelper::DateToDays(2024, 1, 15),
        DayFunctionTestHelper::DateToDays(2024, 6, 20),
        DayFunctionTestHelper::DateToDays(2024, 12, 31)
    };
    std::vector<int32_t> expected = {15, 20, 31};
    
    BaseVector* inputVec = DayFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* resultVec = nullptr;
    DayFunctionTestHelper::ExecuteDay(inputVec, OMNI_DATE32, resultVec);
    DayFunctionTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

// Test: Day from INT - basic cases
TEST(DayTest, IntBasic) {
    std::cout << "=== Test: Day from INT - basic cases ===" << std::endl;
    
    // Create dates as days since epoch: 2024-01-15, 2024-06-20, 2024-12-31
    std::vector<int32_t> intValues = {
        DayFunctionTestHelper::DateToDays(2024, 1, 15),
        DayFunctionTestHelper::DateToDays(2024, 6, 20),
        DayFunctionTestHelper::DateToDays(2024, 12, 31)
    };
    std::vector<int32_t> expected = {15, 20, 31};
    
    BaseVector* inputVec = DayFunctionTestHelper::CreateIntVector(intValues);
    BaseVector* resultVec = nullptr;
    DayFunctionTestHelper::ExecuteDay(inputVec, OMNI_INT, resultVec);
    DayFunctionTestHelper::ValidateResult(resultVec, expected, intValues.size());

    delete resultVec;
}

// Test: Day from DATE32 with NULL values
TEST(DayTest, Date32WithNull) {
    std::cout << "=== Test: Day from DATE32 with NULL values ===" << std::endl;
    
    std::vector<int32_t> dateValues = {
        DayFunctionTestHelper::DateToDays(2024, 1, 15),
        DayFunctionTestHelper::DateToDays(2024, 6, 20),
        DayFunctionTestHelper::DateToDays(2024, 12, 31)
    };
    
    BaseVector* inputVec = DayFunctionTestHelper::CreateDate32Vector(dateValues);
    // Set middle value to NULL
    inputVec->SetNull(1);
    
    BaseVector* resultVec = nullptr;
    DayFunctionTestHelper::ExecuteDay(inputVec, OMNI_DATE32, resultVec);
    
    // First and third should have values, second should be NULL
    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";
    
    // Validate non-null values
    auto* resultVecTyped = dynamic_cast<Vector<int32_t>*>(resultVec);
    EXPECT_EQ(resultVecTyped->GetValue(0), 15) << "Row 0 day should be 15";
    EXPECT_EQ(resultVecTyped->GetValue(2), 31) << "Row 2 day should be 31";

    delete resultVec;
}

// Test: Day from INT with NULL values
TEST(DayTest, IntWithNull) {
    std::cout << "=== Test: Day from INT with NULL values ===" << std::endl;
    
    std::vector<int32_t> intValues = {
        DayFunctionTestHelper::DateToDays(2024, 1, 15),
        DayFunctionTestHelper::DateToDays(2024, 6, 20),
        DayFunctionTestHelper::DateToDays(2024, 12, 31)
    };
    
    BaseVector* inputVec = DayFunctionTestHelper::CreateIntVector(intValues);
    // Set middle value to NULL
    inputVec->SetNull(1);
    
    BaseVector* resultVec = nullptr;
    DayFunctionTestHelper::ExecuteDay(inputVec, OMNI_INT, resultVec);
    
    // First and third should have values, second should be NULL
    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";
    
    // Validate non-null values
    auto* resultVecTyped = dynamic_cast<Vector<int32_t>*>(resultVec);
    EXPECT_EQ(resultVecTyped->GetValue(0), 15) << "Row 0 day should be 15";
    EXPECT_EQ(resultVecTyped->GetValue(2), 31) << "Row 2 day should be 31";

    delete resultVec;
}

// Test: Day with first day of month
TEST(DayTest, FirstDayOfMonth) {
    std::cout << "=== Test: Day with first day of month ===" << std::endl;
    
    // Test first days of different months
    std::vector<int32_t> dateValues = {
        DayFunctionTestHelper::DateToDays(2024, 1, 1),   // January 1st
        DayFunctionTestHelper::DateToDays(2024, 6, 1),   // June 1st
        DayFunctionTestHelper::DateToDays(2024, 12, 1)   // December 1st
    };
    std::vector<int32_t> expected = {1, 1, 1};
    
    BaseVector* inputVec = DayFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* resultVec = nullptr;
    DayFunctionTestHelper::ExecuteDay(inputVec, OMNI_DATE32, resultVec);
    DayFunctionTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

// Test: Day with last day of month
TEST(DayTest, LastDayOfMonth) {
    std::cout << "=== Test: Day with last day of month ===" << std::endl;
    
    // Test last days of different months
    std::vector<int32_t> dateValues = {
        DayFunctionTestHelper::DateToDays(2024, 1, 31),  // January has 31 days
        DayFunctionTestHelper::DateToDays(2024, 4, 30),  // April has 30 days
        DayFunctionTestHelper::DateToDays(2024, 2, 29),  // February 2024 (leap year) has 29 days
        DayFunctionTestHelper::DateToDays(2023, 2, 28)   // February 2023 (non-leap) has 28 days
    };
    std::vector<int32_t> expected = {31, 30, 29, 28};
    
    BaseVector* inputVec = DayFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* resultVec = nullptr;
    DayFunctionTestHelper::ExecuteDay(inputVec, OMNI_DATE32, resultVec);
    DayFunctionTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

// Test: Day with different years including leap year
TEST(DayTest, DifferentYears) {
    std::cout << "=== Test: Day with different years including leap year ===" << std::endl;
    
    // Test various dates across different years
    std::vector<int32_t> dateValues = {
        DayFunctionTestHelper::DateToDays(2020, 2, 29),  // Leap year
        DayFunctionTestHelper::DateToDays(2021, 3, 15),  // Non-leap year
        DayFunctionTestHelper::DateToDays(2024, 2, 29),  // Leap year
        DayFunctionTestHelper::DateToDays(2025, 1, 1),   // New year
        DayFunctionTestHelper::DateToDays(1999, 12, 25)  // Christmas 1999
    };
    std::vector<int32_t> expected = {29, 15, 29, 1, 25};
    
    BaseVector* inputVec = DayFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* resultVec = nullptr;
    DayFunctionTestHelper::ExecuteDay(inputVec, OMNI_DATE32, resultVec);
    DayFunctionTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

// Test: DayOfMonth function alias
TEST(DayTest, DayOfMonthAlias) {
    std::cout << "=== Test: DayOfMonth function alias ===" << std::endl;
    
    // Test that "dayofmonth" function works the same as "day"
    std::vector<int32_t> dateValues = {
        DayFunctionTestHelper::DateToDays(2024, 1, 15),
        DayFunctionTestHelper::DateToDays(2024, 6, 20),
        DayFunctionTestHelper::DateToDays(2024, 12, 31)
    };
    std::vector<int32_t> expected = {15, 20, 31};
    
    BaseVector* inputVec = DayFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* resultVec = nullptr;
    DayFunctionTestHelper::ExecuteDayOfMonth(inputVec, OMNI_DATE32, resultVec);
    DayFunctionTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

// Test: Day with edge dates
TEST(DayTest, EdgeDates) {
    std::cout << "=== Test: Day with edge dates ===" << std::endl;
    
    // Test edge cases - epoch and dates around it
    std::vector<int32_t> dateValues = {
        0,  // 1970-01-01 (Unix epoch)
        1,  // 1970-01-02
        -1, // 1969-12-31
        DayFunctionTestHelper::DateToDays(2000, 1, 1),   // Y2K
        DayFunctionTestHelper::DateToDays(1970, 1, 1)    // Unix epoch explicit
    };
    std::vector<int32_t> expected = {1, 2, 31, 1, 1};
    
    BaseVector* inputVec = DayFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* resultVec = nullptr;
    DayFunctionTestHelper::ExecuteDay(inputVec, OMNI_DATE32, resultVec);
    DayFunctionTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}
