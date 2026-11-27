/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 * Description: Year function unit tests
 */

#include <gtest/gtest.h>
#include <iostream>
#include <vector>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/functions/Year.h"
#include "vectorization/VectorFunction.h"
#include "codegen/func_signature.h"
#include "vector/vector_helper.h"
#include "vector/vector.h"
#include "type/date_time_utils.h"
#include "type/Timestamp.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::codegen;
using namespace omniruntime::TestUtil;

// Initialize function registration before running tests
class YearTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const year_test_env = ::testing::AddGlobalTestEnvironment(new YearTestEnvironment);

class YearFunctionTestHelper {
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
    
    static void ExecuteYear(BaseVector* inputVec, DataTypeId inputTypeId, BaseVector*& result) {
        auto signature = std::make_shared<FunctionSignature>("year", 
            std::vector<DataTypeId>{inputTypeId}, OMNI_INT);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "Year function not found for signature";
        
        auto outputType = std::make_shared<DataType>(OMNI_INT);
        ExecutionContext context(4096, nullptr);
        context.SetResultRowSize(inputVec->GetSize());
        std::stack<BaseVector*> args;
        args.push(inputVec);
        
        ASSERT_NO_THROW(function->Apply(args, outputType, result, &context))
            << "Year function threw an exception";
    }
    
    // Helper to convert date components to days since epoch
    // Uses LocalDate to match the implementation's date calculation
    static int32_t DateToDays(int year, int month, int day) {
        LocalDate date(static_cast<int32_t>(year), static_cast<int16_t>(month), static_cast<int16_t>(day));
        return date.ToDays();
    }

    static BaseVector* CreateTimestampVector(const std::vector<int64_t>& values) {
        BaseVector* vec = VectorHelper::CreateFlatVector(nullptr, OMNI_TIMESTAMP, values.size());
        auto* typedVec = static_cast<Vector<int64_t>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typedVec->SetValue(i, values[i]);
        }
        return vec;
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

// Test: Year from DATE32 - basic cases
TEST(YearTest, Date32Basic) {
    std::cout << "=== Test: Year from DATE32 - basic cases ===" << std::endl;
    
    // Create dates: 2024-01-15, 2023-06-20, 2025-12-31
    std::vector<int32_t> dateValues = {
        YearFunctionTestHelper::DateToDays(2024, 1, 15),
        YearFunctionTestHelper::DateToDays(2023, 6, 20),
        YearFunctionTestHelper::DateToDays(2025, 12, 31)
    };
    std::vector<int32_t> expected = {2024, 2023, 2025};
    
    BaseVector* inputVec = YearFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* resultVec = nullptr;
    YearFunctionTestHelper::ExecuteYear(inputVec, OMNI_DATE32, resultVec);
    YearFunctionTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

// Test: Year from INT - basic cases
TEST(YearTest, IntBasic) {
    std::cout << "=== Test: Year from INT - basic cases ===" << std::endl;
    
    // Create dates as days since epoch: 2024-01-15, 2023-06-20, 2025-12-31
    std::vector<int32_t> intValues = {
        YearFunctionTestHelper::DateToDays(2024, 1, 15),
        YearFunctionTestHelper::DateToDays(2023, 6, 20),
        YearFunctionTestHelper::DateToDays(2025, 12, 31)
    };
    std::vector<int32_t> expected = {2024, 2023, 2025};
    
    BaseVector* inputVec = YearFunctionTestHelper::CreateIntVector(intValues);
    BaseVector* resultVec = nullptr;
    YearFunctionTestHelper::ExecuteYear(inputVec, OMNI_INT, resultVec);
    YearFunctionTestHelper::ValidateResult(resultVec, expected, intValues.size());

    delete resultVec;
}

// Test: Year from DATE32 with NULL values
TEST(YearTest, Date32WithNull) {
    std::cout << "=== Test: Year from DATE32 with NULL values ===" << std::endl;
    
    std::vector<int32_t> dateValues = {
        YearFunctionTestHelper::DateToDays(2024, 1, 15),
        YearFunctionTestHelper::DateToDays(2023, 6, 20),
        YearFunctionTestHelper::DateToDays(2025, 12, 31)
    };
    
    BaseVector* inputVec = YearFunctionTestHelper::CreateDate32Vector(dateValues);
    // Set middle value to NULL
    inputVec->SetNull(1);
    
    BaseVector* resultVec = nullptr;
    YearFunctionTestHelper::ExecuteYear(inputVec, OMNI_DATE32, resultVec);
    
    // First and third should have values, second should be NULL
    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";
    
    // Validate non-null values
    auto* resultVecTyped = dynamic_cast<Vector<int32_t>*>(resultVec);
    EXPECT_EQ(resultVecTyped->GetValue(0), 2024) << "Row 0 year should be 2024";
    EXPECT_EQ(resultVecTyped->GetValue(2), 2025) << "Row 2 year should be 2025";

    delete resultVec;
}

// Test: Year from INT with NULL values
TEST(YearTest, IntWithNull) {
    std::cout << "=== Test: Year from INT with NULL values ===" << std::endl;
    
    std::vector<int32_t> intValues = {
        YearFunctionTestHelper::DateToDays(2024, 1, 15),
        YearFunctionTestHelper::DateToDays(2023, 6, 20),
        YearFunctionTestHelper::DateToDays(2025, 12, 31)
    };
    
    BaseVector* inputVec = YearFunctionTestHelper::CreateIntVector(intValues);
    // Set middle value to NULL
    inputVec->SetNull(1);
    
    BaseVector* resultVec = nullptr;
    YearFunctionTestHelper::ExecuteYear(inputVec, OMNI_INT, resultVec);
    
    // First and third should have values, second should be NULL
    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";
    
    // Validate non-null values
    auto* resultVecTyped = dynamic_cast<Vector<int32_t>*>(resultVec);
    EXPECT_EQ(resultVecTyped->GetValue(0), 2024) << "Row 0 year should be 2024";
    EXPECT_EQ(resultVecTyped->GetValue(2), 2025) << "Row 2 year should be 2025";

    delete resultVec;
}

// Test: Year with different years including leap year
TEST(YearTest, DifferentYears) {
    std::vector<int32_t> dateValues = {
        YearFunctionTestHelper::DateToDays(2020, 2, 29),
        YearFunctionTestHelper::DateToDays(2021, 3, 15),
        YearFunctionTestHelper::DateToDays(2024, 2, 29),
        YearFunctionTestHelper::DateToDays(2025, 1, 1),
        YearFunctionTestHelper::DateToDays(1999, 12, 31)
    };
    std::vector<int32_t> expected = {2020, 2021, 2024, 2025, 1999};

    BaseVector* inputVec = YearFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* resultVec = nullptr;
    YearFunctionTestHelper::ExecuteYear(inputVec, OMNI_DATE32, resultVec);
    YearFunctionTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}
