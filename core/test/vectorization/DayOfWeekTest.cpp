/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: DayOfWeek function unit tests
 */

#include <gtest/gtest.h>
#include <iostream>
#include <vector>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/functions/DayOfWeek.h"
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
class DayOfWeekTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const dayofweek_test_env = ::testing::AddGlobalTestEnvironment(new DayOfWeekTestEnvironment);

class DayOfWeekFunctionTestHelper {
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
    
    static void ExecuteDayOfWeek(BaseVector* inputVec, DataTypeId inputTypeId, BaseVector*& result) {
        auto signature = std::make_shared<FunctionSignature>("dayofweek", 
            std::vector<DataTypeId>{inputTypeId}, OMNI_INT);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "DayOfWeek function not found for signature";
        
        auto outputType = std::make_shared<DataType>(OMNI_INT);
        ExecutionContext context(4096, nullptr);
        context.SetResultRowSize(inputVec->GetSize());
        std::stack<BaseVector*> args;
        args.push(inputVec);
        
        ASSERT_NO_THROW(function->Apply(args, outputType, result, &context))
            << "DayOfWeek function threw an exception";
    }
    
    // Helper to convert date components to days since epoch
    // Uses LocalDate to match the implementation's date calculation
    static int32_t DateToDays(int year, int month, int day) {
        LocalDate date(static_cast<int32_t>(year), static_cast<int16_t>(month), static_cast<int16_t>(day));
        return date.ToDays();
    }
};

// Test: DayOfWeek from DATE32 - basic cases
// Spark SQL convention: 1 = Sunday, 2 = Monday, ..., 7 = Saturday
TEST(DayOfWeekTest, Date32Basic) {
    std::cout << "=== Test: DayOfWeek from DATE32 - basic cases ===" << std::endl;
    
    // Create dates with known day of week:
    // 2024-01-01 is Monday (dayofweek = 2)
    // 2024-01-06 is Saturday (dayofweek = 7)
    // 2024-01-07 is Sunday (dayofweek = 1)
    std::vector<int32_t> dateValues = {
        DayOfWeekFunctionTestHelper::DateToDays(2024, 1, 1),   // Monday
        DayOfWeekFunctionTestHelper::DateToDays(2024, 1, 6),   // Saturday
        DayOfWeekFunctionTestHelper::DateToDays(2024, 1, 7)    // Sunday
    };
    std::vector<int32_t> expected = {2, 7, 1};  // Monday=2, Saturday=7, Sunday=1
    
    BaseVector* inputVec = DayOfWeekFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* resultVec = nullptr;
    DayOfWeekFunctionTestHelper::ExecuteDayOfWeek(inputVec, OMNI_DATE32, resultVec);
    DayOfWeekFunctionTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

// Test: DayOfWeek from INT - basic cases
TEST(DayOfWeekTest, IntBasic) {
    std::cout << "=== Test: DayOfWeek from INT - basic cases ===" << std::endl;
    
    // Same dates as above but using INT type
    std::vector<int32_t> intValues = {
        DayOfWeekFunctionTestHelper::DateToDays(2024, 1, 1),   // Monday
        DayOfWeekFunctionTestHelper::DateToDays(2024, 1, 6),   // Saturday
        DayOfWeekFunctionTestHelper::DateToDays(2024, 1, 7)    // Sunday
    };
    std::vector<int32_t> expected = {2, 7, 1};  // Monday=2, Saturday=7, Sunday=1
    
    BaseVector* inputVec = DayOfWeekFunctionTestHelper::CreateIntVector(intValues);
    BaseVector* resultVec = nullptr;
    DayOfWeekFunctionTestHelper::ExecuteDayOfWeek(inputVec, OMNI_INT, resultVec);
    DayOfWeekFunctionTestHelper::ValidateResult(resultVec, expected, intValues.size());

    delete resultVec;
}

// Test: DayOfWeek from DATE32 with NULL values
TEST(DayOfWeekTest, Date32WithNull) {
    std::cout << "=== Test: DayOfWeek from DATE32 with NULL values ===" << std::endl;
    
    std::vector<int32_t> dateValues = {
        DayOfWeekFunctionTestHelper::DateToDays(2024, 1, 1),   // Monday
        DayOfWeekFunctionTestHelper::DateToDays(2024, 1, 6),   // Saturday
        DayOfWeekFunctionTestHelper::DateToDays(2024, 1, 7)    // Sunday
    };
    
    BaseVector* inputVec = DayOfWeekFunctionTestHelper::CreateDate32Vector(dateValues);
    // Set middle value to NULL
    inputVec->SetNull(1);
    
    BaseVector* resultVec = nullptr;
    DayOfWeekFunctionTestHelper::ExecuteDayOfWeek(inputVec, OMNI_DATE32, resultVec);
    
    // First and third should have values, second should be NULL
    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";
    
    // Validate non-null values
    auto* resultVecTyped = dynamic_cast<Vector<int32_t>*>(resultVec);
    EXPECT_EQ(resultVecTyped->GetValue(0), 2) << "Row 0 dayofweek should be 2 (Monday)";
    EXPECT_EQ(resultVecTyped->GetValue(2), 1) << "Row 2 dayofweek should be 1 (Sunday)";

    delete resultVec;
}

// Test: DayOfWeek from INT with NULL values
TEST(DayOfWeekTest, IntWithNull) {
    std::cout << "=== Test: DayOfWeek from INT with NULL values ===" << std::endl;
    
    std::vector<int32_t> intValues = {
        DayOfWeekFunctionTestHelper::DateToDays(2024, 1, 1),   // Monday
        DayOfWeekFunctionTestHelper::DateToDays(2024, 1, 6),   // Saturday
        DayOfWeekFunctionTestHelper::DateToDays(2024, 1, 7)    // Sunday
    };
    
    BaseVector* inputVec = DayOfWeekFunctionTestHelper::CreateIntVector(intValues);
    // Set middle value to NULL
    inputVec->SetNull(1);
    
    BaseVector* resultVec = nullptr;
    DayOfWeekFunctionTestHelper::ExecuteDayOfWeek(inputVec, OMNI_INT, resultVec);
    
    // First and third should have values, second should be NULL
    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";
    
    // Validate non-null values
    auto* resultVecTyped = dynamic_cast<Vector<int32_t>*>(resultVec);
    EXPECT_EQ(resultVecTyped->GetValue(0), 2) << "Row 0 dayofweek should be 2 (Monday)";
    EXPECT_EQ(resultVecTyped->GetValue(2), 1) << "Row 2 dayofweek should be 1 (Sunday)";

    delete resultVec;
}

// Test: DayOfWeek for all days of a week
TEST(DayOfWeekTest, AllDaysOfWeek) {
    std::cout << "=== Test: DayOfWeek for all days of a week ===" << std::endl;
    
    // Test a full week starting from Sunday 2024-01-07
    // Sunday=1, Monday=2, Tuesday=3, Wednesday=4, Thursday=5, Friday=6, Saturday=7
    std::vector<int32_t> dateValues = {
        DayOfWeekFunctionTestHelper::DateToDays(2024, 1, 7),   // Sunday
        DayOfWeekFunctionTestHelper::DateToDays(2024, 1, 8),   // Monday
        DayOfWeekFunctionTestHelper::DateToDays(2024, 1, 9),   // Tuesday
        DayOfWeekFunctionTestHelper::DateToDays(2024, 1, 10),  // Wednesday
        DayOfWeekFunctionTestHelper::DateToDays(2024, 1, 11),  // Thursday
        DayOfWeekFunctionTestHelper::DateToDays(2024, 1, 12),  // Friday
        DayOfWeekFunctionTestHelper::DateToDays(2024, 1, 13)   // Saturday
    };
    std::vector<int32_t> expected = {1, 2, 3, 4, 5, 6, 7};
    
    BaseVector* inputVec = DayOfWeekFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* resultVec = nullptr;
    DayOfWeekFunctionTestHelper::ExecuteDayOfWeek(inputVec, OMNI_DATE32, resultVec);
    DayOfWeekFunctionTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

// Test: DayOfWeek with epoch date
TEST(DayOfWeekTest, EpochDate) {
    std::cout << "=== Test: DayOfWeek with epoch date ===" << std::endl;
    
    // 1970-01-01 is Thursday (dayofweek = 5)
    std::vector<int32_t> dateValues = {
        0,  // 1970-01-01 (Unix epoch) - Thursday
        1,  // 1970-01-02 - Friday
        2,  // 1970-01-03 - Saturday
        3,  // 1970-01-04 - Sunday
        -1  // 1969-12-31 - Wednesday
    };
    std::vector<int32_t> expected = {5, 6, 7, 1, 4};  // Thu, Fri, Sat, Sun, Wed
    
    BaseVector* inputVec = DayOfWeekFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* resultVec = nullptr;
    DayOfWeekFunctionTestHelper::ExecuteDayOfWeek(inputVec, OMNI_DATE32, resultVec);
    DayOfWeekFunctionTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

// Test: DayOfWeek with different years
TEST(DayOfWeekTest, DifferentYears) {
    std::cout << "=== Test: DayOfWeek with different years ===" << std::endl;
    
    std::vector<int32_t> dateValues = {
        DayOfWeekFunctionTestHelper::DateToDays(2000, 1, 1),   // Y2K - Saturday
        DayOfWeekFunctionTestHelper::DateToDays(2020, 2, 29),  // Leap year - Saturday
        DayOfWeekFunctionTestHelper::DateToDays(1999, 12, 31), // Friday
        DayOfWeekFunctionTestHelper::DateToDays(2024, 12, 25)  // Christmas 2024 - Wednesday
    };
    std::vector<int32_t> expected = {7, 7, 6, 4};  // Sat, Sat, Fri, Wed
    
    BaseVector* inputVec = DayOfWeekFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* resultVec = nullptr;
    DayOfWeekFunctionTestHelper::ExecuteDayOfWeek(inputVec, OMNI_DATE32, resultVec);
    DayOfWeekFunctionTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}
