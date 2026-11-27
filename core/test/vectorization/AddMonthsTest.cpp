/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: AddMonths function unit tests
 */

#include <gtest/gtest.h>
#include <iostream>
#include <vector>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/functions/AddMonths.h"
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
class AddMonthsTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const add_months_test_env = ::testing::AddGlobalTestEnvironment(new AddMonthsTestEnvironment);

class AddMonthsFunctionTestHelper {
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
    
    static BaseVector* CreateInt32Vector(const std::vector<int32_t>& values) {
        BaseVector* vec = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, values.size());
        auto* typedVec = static_cast<Vector<int32_t>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typedVec->SetValue(i, values[i]);
        }
        return vec;
    }
    
    static void ExecuteAddMonths(BaseVector* dateVec, BaseVector* numMonthsVec, BaseVector*& result) {
        auto signature = std::make_shared<FunctionSignature>("add_months", 
            std::vector<DataTypeId>{OMNI_DATE32, OMNI_INT}, OMNI_DATE32);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "AddMonths function not found for signature";
        
        auto outputType = std::make_shared<DataType>(OMNI_DATE32);
        ExecutionContext context(4096, nullptr);
        context.SetResultRowSize(dateVec->GetSize());
        std::stack<BaseVector*> args;

        args.push(dateVec);
        args.push(numMonthsVec);
        
        ASSERT_NO_THROW(function->Apply(args, outputType, result, &context))
            << "AddMonths function threw an exception";
    }
    
    // Helper to convert date components to days since epoch
    static int32_t DateToDays(int year, int month, int day) {
        LocalDate date(static_cast<int32_t>(year), static_cast<int16_t>(month), static_cast<int16_t>(day));
        return date.ToDays();
    }
};

// Test: AddMonths - basic positive cases
TEST(AddMonthsTest, AddMonthsPositive) {
    std::cout << "=== Test: AddMonths - basic positive cases ===" << std::endl;
    
    // Create dates: 2015-01-01, 2015-01-30, 2015-01-31
    std::vector<int32_t> dateValues = {
        AddMonthsFunctionTestHelper::DateToDays(2015, 1, 1),
        AddMonthsFunctionTestHelper::DateToDays(2015, 1, 30),
        AddMonthsFunctionTestHelper::DateToDays(2015, 1, 31)
    };
    
    // Add 10 months, 1 month, 24 months
    std::vector<int32_t> numMonthsValues = {10, 1, 24};
    
    // Expected: 2015-11-01, 2015-02-28 (Jan 30 + 1 month = Feb 28), 2017-01-31
    std::vector<int32_t> expected = {
        AddMonthsFunctionTestHelper::DateToDays(2015, 11, 1),
        AddMonthsFunctionTestHelper::DateToDays(2015, 2, 28),
        AddMonthsFunctionTestHelper::DateToDays(2017, 1, 31)
    };
    
    BaseVector* dateVec = AddMonthsFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* numMonthsVec = AddMonthsFunctionTestHelper::CreateInt32Vector(numMonthsValues);
    BaseVector* resultVec = nullptr;
    
    AddMonthsFunctionTestHelper::ExecuteAddMonths(dateVec, numMonthsVec, resultVec);
    AddMonthsFunctionTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

// Test: AddMonths - negative months (subtract)
TEST(AddMonthsTest, AddMonthsNegative) {
    std::cout << "=== Test: AddMonths - negative months ===" << std::endl;
    
    // Create dates: 2015-01-30, 2015-03-31, 2015-04-20
    std::vector<int32_t> dateValues = {
        AddMonthsFunctionTestHelper::DateToDays(2015, 1, 30),
        AddMonthsFunctionTestHelper::DateToDays(2015, 3, 31),
        AddMonthsFunctionTestHelper::DateToDays(2015, 4, 20)
    };
    
    // Subtract 2 months, 1 month, 24 months
    std::vector<int32_t> numMonthsValues = {-2, -1, -24};
    
    // Expected: 2014-11-30, 2015-02-28 (Mar 31 - 1 month = Feb 28), 2013-04-20
    std::vector<int32_t> expected = {
        AddMonthsFunctionTestHelper::DateToDays(2014, 11, 30),
        AddMonthsFunctionTestHelper::DateToDays(2015, 2, 28),
        AddMonthsFunctionTestHelper::DateToDays(2013, 4, 20)
    };
    
    BaseVector* dateVec = AddMonthsFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* numMonthsVec = AddMonthsFunctionTestHelper::CreateInt32Vector(numMonthsValues);
    BaseVector* resultVec = nullptr;
    
    AddMonthsFunctionTestHelper::ExecuteAddMonths(dateVec, numMonthsVec, resultVec);
    AddMonthsFunctionTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

// Test: AddMonths - zero months (no change)
TEST(AddMonthsTest, AddMonthsZero) {
    std::cout << "=== Test: AddMonths - zero months ===" << std::endl;
    
    // Create dates: 2015-01-30
    std::vector<int32_t> dateValues = {
        AddMonthsFunctionTestHelper::DateToDays(2015, 1, 30)
    };
    
    // Add 0 months
    std::vector<int32_t> numMonthsValues = {0};
    
    // Expected: 2015-01-30 (unchanged)
    std::vector<int32_t> expected = {
        AddMonthsFunctionTestHelper::DateToDays(2015, 1, 30)
    };
    
    BaseVector* dateVec = AddMonthsFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* numMonthsVec = AddMonthsFunctionTestHelper::CreateInt32Vector(numMonthsValues);
    BaseVector* resultVec = nullptr;
    
    AddMonthsFunctionTestHelper::ExecuteAddMonths(dateVec, numMonthsVec, resultVec);
    AddMonthsFunctionTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

// Test: AddMonths - leap year handling
TEST(AddMonthsTest, AddMonthsLeapYear) {
    std::cout << "=== Test: AddMonths - leap year handling ===" << std::endl;
    
    // Create dates: 2016-03-30 (2016 is leap year)
    std::vector<int32_t> dateValues = {
        AddMonthsFunctionTestHelper::DateToDays(2016, 3, 30)
    };
    
    // Subtract 1 month
    std::vector<int32_t> numMonthsValues = {-1};
    
    // Expected: 2016-02-29 (leap year, Feb has 29 days)
    std::vector<int32_t> expected = {
        AddMonthsFunctionTestHelper::DateToDays(2016, 2, 29)
    };
    
    BaseVector* dateVec = AddMonthsFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* numMonthsVec = AddMonthsFunctionTestHelper::CreateInt32Vector(numMonthsValues);
    BaseVector* resultVec = nullptr;
    
    AddMonthsFunctionTestHelper::ExecuteAddMonths(dateVec, numMonthsVec, resultVec);
    AddMonthsFunctionTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

// Test: AddMonths - month end adjustment
TEST(AddMonthsTest, AddMonthsMonthEndAdjustment) {
    std::cout << "=== Test: AddMonths - month end adjustment ===" << std::endl;
    
    // Create dates: 2015-01-31, 2015-01-30
    std::vector<int32_t> dateValues = {
        AddMonthsFunctionTestHelper::DateToDays(2015, 1, 31),
        AddMonthsFunctionTestHelper::DateToDays(2015, 1, 30)
    };
    
    // Add 8 months, 11 months
    std::vector<int32_t> numMonthsValues = {8, 11};
    
    // Expected: 2015-09-30 (Jan 31 + 8 months = Sep 30, Sep has only 30 days),
    //           2015-12-30 (Jan 30 + 11 months = Dec 30)
    std::vector<int32_t> expected = {
        AddMonthsFunctionTestHelper::DateToDays(2015, 9, 30),
        AddMonthsFunctionTestHelper::DateToDays(2015, 12, 30)
    };
    
    BaseVector* dateVec = AddMonthsFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* numMonthsVec = AddMonthsFunctionTestHelper::CreateInt32Vector(numMonthsValues);
    BaseVector* resultVec = nullptr;
    
    AddMonthsFunctionTestHelper::ExecuteAddMonths(dateVec, numMonthsVec, resultVec);
    AddMonthsFunctionTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

// Test: AddMonths with NULL date values
TEST(AddMonthsTest, AddMonthsWithNullDate) {
    std::cout << "=== Test: AddMonths with NULL date values ===" << std::endl;
    
    std::vector<int32_t> dateValues = {
        AddMonthsFunctionTestHelper::DateToDays(2015, 1, 30),
        AddMonthsFunctionTestHelper::DateToDays(2015, 3, 31),
        AddMonthsFunctionTestHelper::DateToDays(2015, 4, 20)
    };
    
    std::vector<int32_t> numMonthsValues = {1, 1, 1};
    
    BaseVector* dateVec = AddMonthsFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* numMonthsVec = AddMonthsFunctionTestHelper::CreateInt32Vector(numMonthsValues);
    
    // Set middle date to NULL
    dateVec->SetNull(1);
    
    BaseVector* resultVec = nullptr;
    AddMonthsFunctionTestHelper::ExecuteAddMonths(dateVec, numMonthsVec, resultVec);
    
    // First and third should have values, second should be NULL
    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";

    delete resultVec;
}

// Test: AddMonths with NULL numMonths values
TEST(AddMonthsTest, AddMonthsWithNullNumMonths) {
    std::cout << "=== Test: AddMonths with NULL numMonths values ===" << std::endl;
    
    std::vector<int32_t> dateValues = {
        AddMonthsFunctionTestHelper::DateToDays(2015, 1, 30),
        AddMonthsFunctionTestHelper::DateToDays(2015, 3, 31),
        AddMonthsFunctionTestHelper::DateToDays(2015, 4, 20)
    };
    
    std::vector<int32_t> numMonthsValues = {1, 1, 1};
    
    BaseVector* dateVec = AddMonthsFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* numMonthsVec = AddMonthsFunctionTestHelper::CreateInt32Vector(numMonthsValues);
    
    // Set middle numMonths to NULL
    numMonthsVec->SetNull(1);
    
    BaseVector* resultVec = nullptr;
    AddMonthsFunctionTestHelper::ExecuteAddMonths(dateVec, numMonthsVec, resultVec);
    
    // First and third should have values, second should be NULL (numMonths is NULL)
    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL (numMonths is NULL)";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";

    delete resultVec;
}
