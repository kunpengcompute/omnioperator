/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: DayOfYear function unit tests
 */

#include <gtest/gtest.h>
#include <iostream>
#include <vector>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/functions/DayOfYear.h"
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
class DayOfYearTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const dayofyear_test_env = ::testing::AddGlobalTestEnvironment(new DayOfYearTestEnvironment);

class DayOfYearFunctionTestHelper {
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
    
    static void ExecuteDayOfYear(BaseVector* inputVec, DataTypeId inputTypeId, BaseVector*& result) {
        auto signature = std::make_shared<FunctionSignature>("dayofyear", 
            std::vector<DataTypeId>{inputTypeId}, OMNI_INT);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "DayOfYear function not found for signature";
        
        auto outputType = std::make_shared<DataType>(OMNI_INT);
        ExecutionContext context(4096, nullptr);
        context.SetResultRowSize(inputVec->GetSize());
        std::stack<BaseVector*> args;
        args.push(inputVec);
        
        ASSERT_NO_THROW(function->Apply(args, outputType, result, &context))
            << "DayOfYear function threw an exception";
    }
    
    // Helper to convert date components to days since epoch
    // Uses LocalDate to match the implementation's date calculation
    static int32_t DateToDays(int year, int month, int day) {
        LocalDate date(static_cast<int32_t>(year), static_cast<int16_t>(month), static_cast<int16_t>(day));
        return date.ToDays();
    }
};

// Test: DayOfYear from DATE32 - basic cases
TEST(DayOfYearTest, Date32Basic) {
    std::cout << "=== Test: DayOfYear from DATE32 - basic cases ===" << std::endl;
    
    // Create dates with known day of year:
    // 2024-01-01 is day 1
    // 2024-01-31 is day 31
    // 2024-02-01 is day 32
    std::vector<int32_t> dateValues = {
        DayOfYearFunctionTestHelper::DateToDays(2024, 1, 1),   // Day 1
        DayOfYearFunctionTestHelper::DateToDays(2024, 1, 31),  // Day 31
        DayOfYearFunctionTestHelper::DateToDays(2024, 2, 1)    // Day 32
    };
    std::vector<int32_t> expected = {1, 31, 32};
    
    BaseVector* inputVec = DayOfYearFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* resultVec = nullptr;
    DayOfYearFunctionTestHelper::ExecuteDayOfYear(inputVec, OMNI_DATE32, resultVec);
    DayOfYearFunctionTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

// Test: DayOfYear from INT - basic cases
TEST(DayOfYearTest, IntBasic) {
    std::cout << "=== Test: DayOfYear from INT - basic cases ===" << std::endl;
    
    // Same dates as above but using INT type
    std::vector<int32_t> intValues = {
        DayOfYearFunctionTestHelper::DateToDays(2024, 1, 1),   // Day 1
        DayOfYearFunctionTestHelper::DateToDays(2024, 1, 31),  // Day 31
        DayOfYearFunctionTestHelper::DateToDays(2024, 2, 1)    // Day 32
    };
    std::vector<int32_t> expected = {1, 31, 32};
    
    BaseVector* inputVec = DayOfYearFunctionTestHelper::CreateIntVector(intValues);
    BaseVector* resultVec = nullptr;
    DayOfYearFunctionTestHelper::ExecuteDayOfYear(inputVec, OMNI_INT, resultVec);
    DayOfYearFunctionTestHelper::ValidateResult(resultVec, expected, intValues.size());

    delete resultVec;
}

// Test: DayOfYear from DATE32 with NULL values
TEST(DayOfYearTest, Date32WithNull) {
    std::cout << "=== Test: DayOfYear from DATE32 with NULL values ===" << std::endl;
    
    std::vector<int32_t> dateValues = {
        DayOfYearFunctionTestHelper::DateToDays(2024, 1, 1),   // Day 1
        DayOfYearFunctionTestHelper::DateToDays(2024, 6, 15),  // Day 167 (leap year)
        DayOfYearFunctionTestHelper::DateToDays(2024, 12, 31)  // Day 366 (leap year)
    };
    
    BaseVector* inputVec = DayOfYearFunctionTestHelper::CreateDate32Vector(dateValues);
    // Set middle value to NULL
    inputVec->SetNull(1);
    
    BaseVector* resultVec = nullptr;
    DayOfYearFunctionTestHelper::ExecuteDayOfYear(inputVec, OMNI_DATE32, resultVec);
    
    // First and third should have values, second should be NULL
    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";
    
    // Validate non-null values
    auto* resultVecTyped = dynamic_cast<Vector<int32_t>*>(resultVec);
    EXPECT_EQ(resultVecTyped->GetValue(0), 1) << "Row 0 dayofyear should be 1";
    EXPECT_EQ(resultVecTyped->GetValue(2), 366) << "Row 2 dayofyear should be 366 (leap year)";

    delete resultVec;
}

// Test: DayOfYear from INT with NULL values
TEST(DayOfYearTest, IntWithNull) {
    std::cout << "=== Test: DayOfYear from INT with NULL values ===" << std::endl;
    
    std::vector<int32_t> intValues = {
        DayOfYearFunctionTestHelper::DateToDays(2024, 1, 1),   // Day 1
        DayOfYearFunctionTestHelper::DateToDays(2024, 6, 15),  // Day 167 (leap year)
        DayOfYearFunctionTestHelper::DateToDays(2024, 12, 31)  // Day 366 (leap year)
    };
    
    BaseVector* inputVec = DayOfYearFunctionTestHelper::CreateIntVector(intValues);
    // Set middle value to NULL
    inputVec->SetNull(1);
    
    BaseVector* resultVec = nullptr;
    DayOfYearFunctionTestHelper::ExecuteDayOfYear(inputVec, OMNI_INT, resultVec);
    
    // First and third should have values, second should be NULL
    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";
    
    // Validate non-null values
    auto* resultVecTyped = dynamic_cast<Vector<int32_t>*>(resultVec);
    EXPECT_EQ(resultVecTyped->GetValue(0), 1) << "Row 0 dayofyear should be 1";
    EXPECT_EQ(resultVecTyped->GetValue(2), 366) << "Row 2 dayofyear should be 366 (leap year)";

    delete resultVec;
}

// Test: DayOfYear for leap year vs non-leap year
TEST(DayOfYearTest, LeapYearVsNonLeapYear) {
    std::cout << "=== Test: DayOfYear for leap year vs non-leap year ===" << std::endl;
    
    // In leap year (2024): Feb has 29 days, so March 1st is day 61
    // In non-leap year (2023): Feb has 28 days, so March 1st is day 60
    std::vector<int32_t> dateValues = {
        DayOfYearFunctionTestHelper::DateToDays(2024, 2, 29),  // Day 60 (leap year Feb 29)
        DayOfYearFunctionTestHelper::DateToDays(2024, 3, 1),   // Day 61 (leap year Mar 1)
        DayOfYearFunctionTestHelper::DateToDays(2023, 3, 1),   // Day 60 (non-leap year Mar 1)
        DayOfYearFunctionTestHelper::DateToDays(2024, 12, 31), // Day 366 (leap year last day)
        DayOfYearFunctionTestHelper::DateToDays(2023, 12, 31)  // Day 365 (non-leap year last day)
    };
    std::vector<int32_t> expected = {60, 61, 60, 366, 365};
    
    BaseVector* inputVec = DayOfYearFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* resultVec = nullptr;
    DayOfYearFunctionTestHelper::ExecuteDayOfYear(inputVec, OMNI_DATE32, resultVec);
    DayOfYearFunctionTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

// Test: DayOfYear with epoch date
TEST(DayOfYearTest, EpochDate) {
    std::cout << "=== Test: DayOfYear with epoch date ===" << std::endl;
    
    // 1970-01-01 is day 1 of 1970
    // 1970-01-02 is day 2 of 1970
    // 1969-12-31 is day 365 of 1969 (non-leap year)
    std::vector<int32_t> dateValues = {
        0,   // 1970-01-01 - Day 1
        1,   // 1970-01-02 - Day 2
        31,  // 1970-02-01 - Day 32
        -1   // 1969-12-31 - Day 365
    };
    std::vector<int32_t> expected = {1, 2, 32, 365};
    
    BaseVector* inputVec = DayOfYearFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* resultVec = nullptr;
    DayOfYearFunctionTestHelper::ExecuteDayOfYear(inputVec, OMNI_DATE32, resultVec);
    DayOfYearFunctionTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

// Test: DayOfYear for specific months
TEST(DayOfYearTest, SpecificMonths) {
    std::cout << "=== Test: DayOfYear for specific months ===" << std::endl;
    
    // Test first day of each quarter in 2024 (leap year)
    // Jan 1 = day 1
    // Apr 1 = day 92 (31+29+31 = 91, so Apr 1 = 92)
    // Jul 1 = day 183 (91 + 30 + 31 + 30 = 182, so Jul 1 = 183)
    // Oct 1 = day 275 (182 + 31 + 31 + 30 = 274, so Oct 1 = 275)
    std::vector<int32_t> dateValues = {
        DayOfYearFunctionTestHelper::DateToDays(2024, 1, 1),   // Q1 start
        DayOfYearFunctionTestHelper::DateToDays(2024, 4, 1),   // Q2 start
        DayOfYearFunctionTestHelper::DateToDays(2024, 7, 1),   // Q3 start
        DayOfYearFunctionTestHelper::DateToDays(2024, 10, 1)   // Q4 start
    };
    std::vector<int32_t> expected = {1, 92, 183, 275};
    
    BaseVector* inputVec = DayOfYearFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* resultVec = nullptr;
    DayOfYearFunctionTestHelper::ExecuteDayOfYear(inputVec, OMNI_DATE32, resultVec);
    DayOfYearFunctionTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

// Test: DayOfYear with different years
TEST(DayOfYearTest, DifferentYears) {
    std::cout << "=== Test: DayOfYear with different years ===" << std::endl;
    
    std::vector<int32_t> dateValues = {
        DayOfYearFunctionTestHelper::DateToDays(2000, 1, 1),   // Y2K - Day 1
        DayOfYearFunctionTestHelper::DateToDays(2000, 12, 31), // Y2K end - Day 366 (leap year)
        DayOfYearFunctionTestHelper::DateToDays(1999, 12, 31), // 1999 end - Day 365
        DayOfYearFunctionTestHelper::DateToDays(2024, 6, 15)   // Mid-year 2024
    };
    // 2024-06-15: Jan(31) + Feb(29) + Mar(31) + Apr(30) + May(31) + 15 = 167
    std::vector<int32_t> expected = {1, 366, 365, 167};
    
    BaseVector* inputVec = DayOfYearFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* resultVec = nullptr;
    DayOfYearFunctionTestHelper::ExecuteDayOfYear(inputVec, OMNI_DATE32, resultVec);
    DayOfYearFunctionTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}
