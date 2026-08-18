/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: DateAdd function unit tests
 */

#include <gtest/gtest.h>
#include <iostream>
#include <vector>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/functions/DateAdd.h"
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
class DateAddTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const date_add_test_env = ::testing::AddGlobalTestEnvironment(new DateAddTestEnvironment);

class DateAddFunctionTestHelper {
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
        BaseVector* vec = VectorHelper::CreateFlatVector(OMNI_DATE32, values.size());
        auto* typedVec = static_cast<Vector<int32_t>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typedVec->SetValue(i, values[i]);
        }
        return vec;
    }
    
    static BaseVector* CreateInt32Vector(const std::vector<int32_t>& values) {
        BaseVector* vec = VectorHelper::CreateFlatVector(OMNI_INT, values.size());
        auto* typedVec = static_cast<Vector<int32_t>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typedVec->SetValue(i, values[i]);
        }
        return vec;
    }
    
    static BaseVector* CreateInt16Vector(const std::vector<int16_t>& values) {
        BaseVector* vec = VectorHelper::CreateFlatVector(OMNI_SHORT, values.size());
        auto* typedVec = static_cast<Vector<int16_t>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typedVec->SetValue(i, values[i]);
        }
        return vec;
    }
    
    static BaseVector* CreateInt8Vector(const std::vector<int8_t>& values) {
        BaseVector* vec = VectorHelper::CreateFlatVector(OMNI_BYTE, values.size());
        auto* typedVec = static_cast<Vector<int8_t>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typedVec->SetValue(i, values[i]);
        }
        return vec;
    }
    
    static void ExecuteDateAdd(BaseVector* dateVec, BaseVector* numDaysVec, BaseVector*& result) {
        auto signature = std::make_shared<FunctionSignature>("date_add", 
            std::vector<DataTypeId>{OMNI_DATE32, numDaysVec->GetTypeId()}, OMNI_DATE32);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "DateAdd function not found for signature";
        
        auto outputType = std::make_shared<DataType>(OMNI_DATE32);
        ExecutionContext context;
        context.SetResultRowSize(dateVec->GetSize());
        std::stack<BaseVector*> args;

        args.push(dateVec);
        args.push(numDaysVec);
        
        ASSERT_NO_THROW(function->Apply(args, outputType, result, &context))
            << "DateAdd function threw an exception";
    }
    
    // Helper to convert date components to days since epoch
    static int32_t DateToDays(int year, int month, int day) {
        LocalDate date(static_cast<int32_t>(year), static_cast<int16_t>(month), static_cast<int16_t>(day));
        return date.ToDays();
    }
};

// Test: DateAdd - basic positive cases
TEST(DateAddTest, DateAddPositive) {
    std::cout << "=== Test: DateAdd - basic positive cases ===" << std::endl;
    
    // Create dates: 2019-03-01, 2019-02-28
    std::vector<int32_t> dateValues = {
        DateAddFunctionTestHelper::DateToDays(2019, 3, 1),
        DateAddFunctionTestHelper::DateToDays(2019, 2, 28)
    };
    
    // Add 0 days, 1 day
    std::vector<int32_t> numDaysValues = {0, 1};
    
    // Expected: 2019-03-01, 2019-03-01 (Feb 28 + 1 day = Mar 1)
    std::vector<int32_t> expected = {
        DateAddFunctionTestHelper::DateToDays(2019, 3, 1),
        DateAddFunctionTestHelper::DateToDays(2019, 3, 1)
    };
    
    BaseVector* dateVec = DateAddFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* numDaysVec = DateAddFunctionTestHelper::CreateInt32Vector(numDaysValues);
    BaseVector* resultVec = nullptr;
    
    DateAddFunctionTestHelper::ExecuteDateAdd(dateVec, numDaysVec, resultVec);
    DateAddFunctionTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

// Test: DateAdd with constant date literal
TEST(DateAddTest, DateAddConstantDate) {
    std::cout << "=== Test: DateAdd with constant date literal ===" << std::endl;

    const int32_t baseDate = DateAddFunctionTestHelper::DateToDays(1, 1, 1);
    std::vector<int32_t> numDaysValues = {0, 1, 31, 2922897};
    std::vector<int32_t> expected = {
        baseDate,
        baseDate + 1,
        baseDate + 31,
        baseDate + 2922897
    };

    BaseVector* dateVec = new ConstVector<int32_t>(baseDate, OMNI_DATE32, numDaysValues.size());
    BaseVector* numDaysVec = DateAddFunctionTestHelper::CreateInt32Vector(numDaysValues);

    BaseVector* resultVec = nullptr;
    DateAddFunctionTestHelper::ExecuteDateAdd(dateVec, numDaysVec, resultVec);
    DateAddFunctionTestHelper::ValidateResult(resultVec, expected, numDaysValues.size());

    delete resultVec;
}

// Test: DateAdd with NULL constant date literal
TEST(DateAddTest, DateAddNullConstantDate) {
    std::cout << "=== Test: DateAdd with NULL constant date literal ===" << std::endl;

    std::vector<int32_t> numDaysValues = {0, 1, 31};
    BaseVector* dateVec = new ConstVector<int32_t>(0, OMNI_DATE32, numDaysValues.size());
    dateVec->SetNulls(0, true, numDaysValues.size());
    BaseVector* numDaysVec = DateAddFunctionTestHelper::CreateInt32Vector(numDaysValues);

    BaseVector* resultVec = nullptr;
    DateAddFunctionTestHelper::ExecuteDateAdd(dateVec, numDaysVec, resultVec);

    for (size_t i = 0; i < numDaysValues.size(); ++i) {
        EXPECT_TRUE(resultVec->IsNull(i)) << "Row " << i << " should be NULL";
    }

    delete resultVec;
}

// Test: DateAdd - negative days (subtract)
TEST(DateAddTest, DateAddNegative) {
    std::cout << "=== Test: DateAdd - negative days ===" << std::endl;
    
    // Create dates: 2020-02-29 (leap year), 2019-01-30
    std::vector<int32_t> dateValues = {
        DateAddFunctionTestHelper::DateToDays(2020, 2, 29),
        DateAddFunctionTestHelper::DateToDays(2019, 1, 30)
    };
    
    // Subtract 366 days, 395 days
    std::vector<int32_t> numDaysValues = {-366, -395};
    
    // Expected: 2019-02-28, 2018-01-01
    std::vector<int32_t> expected = {
        DateAddFunctionTestHelper::DateToDays(2019, 2, 28),
        DateAddFunctionTestHelper::DateToDays(2017, 12, 31)
    };
    
    BaseVector* dateVec = DateAddFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* numDaysVec = DateAddFunctionTestHelper::CreateInt32Vector(numDaysValues);
    BaseVector* resultVec = nullptr;
    
    DateAddFunctionTestHelper::ExecuteDateAdd(dateVec, numDaysVec, resultVec);
    DateAddFunctionTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

// Test: DateAdd - zero days (no change)
TEST(DateAddTest, DateAddZero) {
    std::cout << "=== Test: DateAdd - zero days ===" << std::endl;
    
    // Create dates: 2019-03-01
    std::vector<int32_t> dateValues = {
        DateAddFunctionTestHelper::DateToDays(2019, 3, 1)
    };
    
    // Add 0 days
    std::vector<int32_t> numDaysValues = {0};
    
    // Expected: 2019-03-01 (unchanged)
    std::vector<int32_t> expected = {
        DateAddFunctionTestHelper::DateToDays(2019, 3, 1)
    };
    
    BaseVector* dateVec = DateAddFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* numDaysVec = DateAddFunctionTestHelper::CreateInt32Vector(numDaysValues);
    BaseVector* resultVec = nullptr;
    
    DateAddFunctionTestHelper::ExecuteDateAdd(dateVec, numDaysVec, resultVec);
    DateAddFunctionTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

// Test: DateAdd - leap year handling
TEST(DateAddTest, DateAddLeapYear) {
    std::cout << "=== Test: DateAdd - leap year handling ===" << std::endl;
    
    // Create dates: 2019-01-30
    std::vector<int32_t> dateValues = {
        DateAddFunctionTestHelper::DateToDays(2019, 1, 30)
    };
    
    // Add 395 days (should reach 2020-02-29, leap year)
    std::vector<int32_t> numDaysValues = {395};
    
    // Expected: 2020-02-29 (leap year, Feb has 29 days)
    std::vector<int32_t> expected = {
        DateAddFunctionTestHelper::DateToDays(2020, 2, 29)
    };
    
    BaseVector* dateVec = DateAddFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* numDaysVec = DateAddFunctionTestHelper::CreateInt32Vector(numDaysValues);
    BaseVector* resultVec = nullptr;
    
    DateAddFunctionTestHelper::ExecuteDateAdd(dateVec, numDaysVec, resultVec);
    DateAddFunctionTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

// Test: DateAdd - year boundary crossing
TEST(DateAddTest, DateAddYearBoundary) {
    std::cout << "=== Test: DateAdd - year boundary crossing ===" << std::endl;
    
    // Create dates: 2019-12-31, 2020-01-01
    std::vector<int32_t> dateValues = {
        DateAddFunctionTestHelper::DateToDays(2019, 12, 31),
        DateAddFunctionTestHelper::DateToDays(2020, 1, 1)
    };
    
    // Add 1 day, -1 day
    std::vector<int32_t> numDaysValues = {1, -1};
    
    // Expected: 2020-01-01, 2019-12-31
    std::vector<int32_t> expected = {
        DateAddFunctionTestHelper::DateToDays(2020, 1, 1),
        DateAddFunctionTestHelper::DateToDays(2019, 12, 31)
    };
    
    BaseVector* dateVec = DateAddFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* numDaysVec = DateAddFunctionTestHelper::CreateInt32Vector(numDaysValues);
    BaseVector* resultVec = nullptr;
    
    DateAddFunctionTestHelper::ExecuteDateAdd(dateVec, numDaysVec, resultVec);
    DateAddFunctionTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

// Test: DateAdd with SMALLINT numDays
TEST(DateAddTest, DateAddSmallint) {
    std::cout << "=== Test: DateAdd with SMALLINT numDays ===" << std::endl;
    
    // Create dates: 2019-03-01
    std::vector<int32_t> dateValues = {
        DateAddFunctionTestHelper::DateToDays(2019, 3, 1)
    };
    
    // Add 10 days (using SMALLINT)
    std::vector<int16_t> numDaysValues = {10};
    
    // Expected: 2019-03-11
    std::vector<int32_t> expected = {
        DateAddFunctionTestHelper::DateToDays(2019, 3, 11)
    };
    
    BaseVector* dateVec = DateAddFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* numDaysVec = DateAddFunctionTestHelper::CreateInt16Vector(numDaysValues);
    BaseVector* resultVec = nullptr;
    
    DateAddFunctionTestHelper::ExecuteDateAdd(dateVec, numDaysVec, resultVec);
    DateAddFunctionTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

// Test: DateAdd with TINYINT numDays
TEST(DateAddTest, DateAddTinyint) {
    std::cout << "=== Test: DateAdd with TINYINT numDays ===" << std::endl;
    
    // Create dates: 2019-03-01
    std::vector<int32_t> dateValues = {
        DateAddFunctionTestHelper::DateToDays(2019, 3, 1)
    };
    
    // Add 5 days (using TINYINT)
    std::vector<int8_t> numDaysValues = {5};
    
    // Expected: 2019-03-06
    std::vector<int32_t> expected = {
        DateAddFunctionTestHelper::DateToDays(2019, 3, 6)
    };
    
    BaseVector* dateVec = DateAddFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* numDaysVec = DateAddFunctionTestHelper::CreateInt8Vector(numDaysValues);
    BaseVector* resultVec = nullptr;
    
    DateAddFunctionTestHelper::ExecuteDateAdd(dateVec, numDaysVec, resultVec);
    DateAddFunctionTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

// Test: DateAdd with NULL date values
TEST(DateAddTest, DateAddWithNullDate) {
    std::cout << "=== Test: DateAdd with NULL date values ===" << std::endl;
    
    std::vector<int32_t> dateValues = {
        DateAddFunctionTestHelper::DateToDays(2019, 3, 1),
        DateAddFunctionTestHelper::DateToDays(2019, 3, 1),
        DateAddFunctionTestHelper::DateToDays(2019, 3, 1)
    };
    
    std::vector<int32_t> numDaysValues = {1, 1, 1};
    
    BaseVector* dateVec = DateAddFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* numDaysVec = DateAddFunctionTestHelper::CreateInt32Vector(numDaysValues);
    
    // Set middle date to NULL
    dateVec->SetNull(1);
    
    BaseVector* resultVec = nullptr;
    DateAddFunctionTestHelper::ExecuteDateAdd(dateVec, numDaysVec, resultVec);
    
    // First and third should have values, second should be NULL
    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";

    delete resultVec;
}

// Test: DateAdd with NULL numDays values
TEST(DateAddTest, DateAddWithNullNumDays) {
    std::cout << "=== Test: DateAdd with NULL numDays values ===" << std::endl;
    
    std::vector<int32_t> dateValues = {
        DateAddFunctionTestHelper::DateToDays(2019, 3, 1),
        DateAddFunctionTestHelper::DateToDays(2019, 3, 1),
        DateAddFunctionTestHelper::DateToDays(2019, 3, 1)
    };
    
    std::vector<int32_t> numDaysValues = {1, 1, 1};
    
    BaseVector* dateVec = DateAddFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* numDaysVec = DateAddFunctionTestHelper::CreateInt32Vector(numDaysValues);
    
    // Set middle numDays to NULL
    numDaysVec->SetNull(1);
    
    BaseVector* resultVec = nullptr;
    DateAddFunctionTestHelper::ExecuteDateAdd(dateVec, numDaysVec, resultVec);
    
    // First and third should have values, second should be NULL (numDays is NULL)
    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL (numDays is NULL)";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";

    delete resultVec;
}
