/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: DateSub function unit tests
 */

#include <gtest/gtest.h>
#include <iostream>
#include <vector>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/functions/DateSub.h"
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
class DateSubTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const date_sub_test_env = ::testing::AddGlobalTestEnvironment(new DateSubTestEnvironment);

class DateSubFunctionTestHelper {
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
    
    static BaseVector* CreateInt16Vector(const std::vector<int16_t>& values) {
        BaseVector* vec = VectorHelper::CreateFlatVector(nullptr, OMNI_SHORT, values.size());
        auto* typedVec = static_cast<Vector<int16_t>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typedVec->SetValue(i, values[i]);
        }
        return vec;
    }
    
    static BaseVector* CreateInt8Vector(const std::vector<int8_t>& values) {
        BaseVector* vec = VectorHelper::CreateFlatVector(nullptr, OMNI_BYTE, values.size());
        auto* typedVec = static_cast<Vector<int8_t>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typedVec->SetValue(i, values[i]);
        }
        return vec;
    }
    
    static void ExecuteDateSub(BaseVector* dateVec, BaseVector* numDaysVec, BaseVector*& result) {
        auto signature = std::make_shared<FunctionSignature>("date_sub", 
            std::vector<DataTypeId>{OMNI_DATE32, numDaysVec->GetTypeId()}, OMNI_DATE32);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "DateSub function not found for signature";
        
        auto outputType = std::make_shared<DataType>(OMNI_DATE32);
        ExecutionContext context(4096, nullptr);
        context.SetResultRowSize(dateVec->GetSize());
        std::stack<BaseVector*> args;

        args.push(dateVec);
        args.push(numDaysVec);
        
        ASSERT_NO_THROW(function->Apply(args, outputType, result, &context))
            << "DateSub function threw an exception";
    }
    
    // Helper to convert date components to days since epoch
    static int32_t DateToDays(int year, int month, int day) {
        LocalDate date(static_cast<int32_t>(year), static_cast<int16_t>(month), static_cast<int16_t>(day));
        return date.ToDays();
    }
};

// Test: DateSub - basic positive cases (subtract positive days)
TEST(DateSubTest, DateSubPositive) {
    std::cout << "=== Test: DateSub - basic positive cases ===" << std::endl;
    
    // Create dates: 2019-03-01, 2019-03-02
    std::vector<int32_t> dateValues = {
        DateSubFunctionTestHelper::DateToDays(2019, 3, 1),
        DateSubFunctionTestHelper::DateToDays(2019, 3, 2)
    };
    
    // Subtract 0 days, 1 day
    std::vector<int32_t> numDaysValues = {0, 1};
    
    // Expected: 2019-03-01, 2019-03-01
    std::vector<int32_t> expected = {
        DateSubFunctionTestHelper::DateToDays(2019, 3, 1),
        DateSubFunctionTestHelper::DateToDays(2019, 3, 1)
    };
    
    BaseVector* dateVec = DateSubFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* numDaysVec = DateSubFunctionTestHelper::CreateInt32Vector(numDaysValues);
    BaseVector* resultVec = nullptr;
    
    DateSubFunctionTestHelper::ExecuteDateSub(dateVec, numDaysVec, resultVec);
    DateSubFunctionTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

// Test: DateSub - negative days (add days, equivalent to date_add with positive)
TEST(DateSubTest, DateSubNegative) {
    std::cout << "=== Test: DateSub - negative days ===" << std::endl;
    
    // Create dates: 2019-02-28, 2017-12-31
    std::vector<int32_t> dateValues = {
        DateSubFunctionTestHelper::DateToDays(2019, 2, 28),
        DateSubFunctionTestHelper::DateToDays(2017, 12, 31)
    };
    
    // Subtract -366 days (add 366), -395 days (add 395)
    std::vector<int32_t> numDaysValues = {-366, -395};
    
    // Expected: 2020-02-29 (leap year), 2019-01-30
    std::vector<int32_t> expected = {
        DateSubFunctionTestHelper::DateToDays(2020, 2, 29),
        DateSubFunctionTestHelper::DateToDays(2019, 1, 30)
    };
    
    BaseVector* dateVec = DateSubFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* numDaysVec = DateSubFunctionTestHelper::CreateInt32Vector(numDaysValues);
    BaseVector* resultVec = nullptr;
    
    DateSubFunctionTestHelper::ExecuteDateSub(dateVec, numDaysVec, resultVec);
    DateSubFunctionTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

// Test: DateSub - zero days (no change)
TEST(DateSubTest, DateSubZero) {
    std::cout << "=== Test: DateSub - zero days ===" << std::endl;
    
    // Create dates: 2019-03-01
    std::vector<int32_t> dateValues = {
        DateSubFunctionTestHelper::DateToDays(2019, 3, 1)
    };
    
    // Subtract 0 days
    std::vector<int32_t> numDaysValues = {0};
    
    // Expected: 2019-03-01 (unchanged)
    std::vector<int32_t> expected = {
        DateSubFunctionTestHelper::DateToDays(2019, 3, 1)
    };
    
    BaseVector* dateVec = DateSubFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* numDaysVec = DateSubFunctionTestHelper::CreateInt32Vector(numDaysValues);
    BaseVector* resultVec = nullptr;
    
    DateSubFunctionTestHelper::ExecuteDateSub(dateVec, numDaysVec, resultVec);
    DateSubFunctionTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

// Test: DateSub - leap year handling
TEST(DateSubTest, DateSubLeapYear) {
    std::cout << "=== Test: DateSub - leap year handling ===" << std::endl;
    
    // Create dates: 2020-02-29 (leap year)
    std::vector<int32_t> dateValues = {
        DateSubFunctionTestHelper::DateToDays(2020, 2, 29)
    };
    
    // Subtract 395 days (should reach 2019-01-30)
    std::vector<int32_t> numDaysValues = {395};
    
    // Expected: 2019-01-30
    std::vector<int32_t> expected = {
        DateSubFunctionTestHelper::DateToDays(2019, 1, 30)
    };
    
    BaseVector* dateVec = DateSubFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* numDaysVec = DateSubFunctionTestHelper::CreateInt32Vector(numDaysValues);
    BaseVector* resultVec = nullptr;
    
    DateSubFunctionTestHelper::ExecuteDateSub(dateVec, numDaysVec, resultVec);
    DateSubFunctionTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

// Test: DateSub - year boundary crossing
TEST(DateSubTest, DateSubYearBoundary) {
    std::cout << "=== Test: DateSub - year boundary crossing ===" << std::endl;
    
    // Create dates: 2020-01-01, 2019-12-31
    std::vector<int32_t> dateValues = {
        DateSubFunctionTestHelper::DateToDays(2020, 1, 1),
        DateSubFunctionTestHelper::DateToDays(2019, 12, 31)
    };
    
    // Subtract 1 day, -1 day (add 1)
    std::vector<int32_t> numDaysValues = {1, -1};
    
    // Expected: 2019-12-31, 2020-01-01
    std::vector<int32_t> expected = {
        DateSubFunctionTestHelper::DateToDays(2019, 12, 31),
        DateSubFunctionTestHelper::DateToDays(2020, 1, 1)
    };
    
    BaseVector* dateVec = DateSubFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* numDaysVec = DateSubFunctionTestHelper::CreateInt32Vector(numDaysValues);
    BaseVector* resultVec = nullptr;
    
    DateSubFunctionTestHelper::ExecuteDateSub(dateVec, numDaysVec, resultVec);
    DateSubFunctionTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

// Test: DateSub with SMALLINT numDays
TEST(DateSubTest, DateSubSmallint) {
    std::cout << "=== Test: DateSub with SMALLINT numDays ===" << std::endl;
    
    // Create dates: 2019-03-11
    std::vector<int32_t> dateValues = {
        DateSubFunctionTestHelper::DateToDays(2019, 3, 11)
    };
    
    // Subtract 10 days (using SMALLINT)
    std::vector<int16_t> numDaysValues = {10};
    
    // Expected: 2019-03-01
    std::vector<int32_t> expected = {
        DateSubFunctionTestHelper::DateToDays(2019, 3, 1)
    };
    
    BaseVector* dateVec = DateSubFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* numDaysVec = DateSubFunctionTestHelper::CreateInt16Vector(numDaysValues);
    BaseVector* resultVec = nullptr;
    
    DateSubFunctionTestHelper::ExecuteDateSub(dateVec, numDaysVec, resultVec);
    DateSubFunctionTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

// Test: DateSub with TINYINT numDays
TEST(DateSubTest, DateSubTinyint) {
    std::cout << "=== Test: DateSub with TINYINT numDays ===" << std::endl;
    
    // Create dates: 2019-03-06
    std::vector<int32_t> dateValues = {
        DateSubFunctionTestHelper::DateToDays(2019, 3, 6)
    };
    
    // Subtract 5 days (using TINYINT)
    std::vector<int8_t> numDaysValues = {5};
    
    // Expected: 2019-03-01
    std::vector<int32_t> expected = {
        DateSubFunctionTestHelper::DateToDays(2019, 3, 1)
    };
    
    BaseVector* dateVec = DateSubFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* numDaysVec = DateSubFunctionTestHelper::CreateInt8Vector(numDaysValues);
    BaseVector* resultVec = nullptr;
    
    DateSubFunctionTestHelper::ExecuteDateSub(dateVec, numDaysVec, resultVec);
    DateSubFunctionTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

// Test: DateSub with NULL date values
TEST(DateSubTest, DateSubWithNullDate) {
    std::cout << "=== Test: DateSub with NULL date values ===" << std::endl;
    
    std::vector<int32_t> dateValues = {
        DateSubFunctionTestHelper::DateToDays(2019, 3, 1),
        DateSubFunctionTestHelper::DateToDays(2019, 3, 1),
        DateSubFunctionTestHelper::DateToDays(2019, 3, 1)
    };
    
    std::vector<int32_t> numDaysValues = {1, 1, 1};
    
    BaseVector* dateVec = DateSubFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* numDaysVec = DateSubFunctionTestHelper::CreateInt32Vector(numDaysValues);
    
    // Set middle date to NULL
    dateVec->SetNull(1);
    
    BaseVector* resultVec = nullptr;
    DateSubFunctionTestHelper::ExecuteDateSub(dateVec, numDaysVec, resultVec);
    
    // First and third should have values, second should be NULL
    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";

    delete resultVec;
}

// Test: DateSub with NULL numDays values
TEST(DateSubTest, DateSubWithNullNumDays) {
    std::cout << "=== Test: DateSub with NULL numDays values ===" << std::endl;
    
    std::vector<int32_t> dateValues = {
        DateSubFunctionTestHelper::DateToDays(2019, 3, 1),
        DateSubFunctionTestHelper::DateToDays(2019, 3, 1),
        DateSubFunctionTestHelper::DateToDays(2019, 3, 1)
    };
    
    std::vector<int32_t> numDaysValues = {1, 1, 1};
    
    BaseVector* dateVec = DateSubFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* numDaysVec = DateSubFunctionTestHelper::CreateInt32Vector(numDaysValues);
    
    // Set middle numDays to NULL
    numDaysVec->SetNull(1);
    
    BaseVector* resultVec = nullptr;
    DateSubFunctionTestHelper::ExecuteDateSub(dateVec, numDaysVec, resultVec);
    
    // First and third should have values, second should be NULL (numDays is NULL)
    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL (numDays is NULL)";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";

    delete resultVec;
}
