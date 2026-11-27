/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: Trunc function unit tests
 */

#include <gtest/gtest.h>
#include <iostream>
#include <vector>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/functions/Trunc.h"
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
class TruncTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const trunc_test_env = ::testing::AddGlobalTestEnvironment(new TruncTestEnvironment);

class TruncFunctionTestHelper {
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
    
    static BaseVector* CreateStringVector(const std::vector<std::string>& values) {
        BaseVector* vec = VectorHelper::CreateStringVector(values.size());
        auto* typedVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            std::string_view sv(values[i]);
            typedVec->SetValue(i, sv);
        }
        return vec;
    }
    
    static void ExecuteTrunc(BaseVector* dateVec, BaseVector* formatVec, BaseVector*& result) {
        auto signature = std::make_shared<FunctionSignature>("trunc_date", 
            std::vector<DataTypeId>{OMNI_DATE32, OMNI_VARCHAR}, OMNI_DATE32);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "Trunc function not found for signature";
        
        auto outputType = std::make_shared<DataType>(OMNI_DATE32);
        ExecutionContext context(4096, nullptr);
        context.SetResultRowSize(dateVec->GetSize());
        std::stack<BaseVector*> args;

        args.push(dateVec);
        args.push(formatVec);
        
        ASSERT_NO_THROW(function->Apply(args, outputType, result, &context))
            << "Trunc function threw an exception";
    }
    
    // Helper to convert date components to days since epoch
    // Uses LocalDate to match the implementation's date calculation
    static int32_t DateToDays(int year, int month, int day) {
        LocalDate date(static_cast<int32_t>(year), static_cast<int16_t>(month), static_cast<int16_t>(day));
        return date.ToDays();
    }
};

// Test: Trunc to YEAR - basic cases
TEST(TruncTest, TruncToYear) {
    std::cout << "=== Test: Trunc to YEAR - basic cases ===" << std::endl;
    
    // Create dates: 2024-06-15, 2024-12-31, 2025-03-20
    std::vector<int32_t> dateValues = {
        TruncFunctionTestHelper::DateToDays(2024, 6, 15),
        TruncFunctionTestHelper::DateToDays(2024, 12, 31),
        TruncFunctionTestHelper::DateToDays(2025, 3, 20)
    };
    
    // Expected: 2024-01-01, 2024-01-01, 2025-01-01
    std::vector<int32_t> expected = {
        TruncFunctionTestHelper::DateToDays(2024, 1, 1),
        TruncFunctionTestHelper::DateToDays(2024, 1, 1),
        TruncFunctionTestHelper::DateToDays(2025, 1, 1)
    };
    
    std::vector<std::string> formatValues = {"YEAR", "YEAR", "YEAR"};
    
    BaseVector* dateVec = TruncFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* formatVec = TruncFunctionTestHelper::CreateStringVector(formatValues);
    BaseVector* resultVec = nullptr;
    
    TruncFunctionTestHelper::ExecuteTrunc(dateVec, formatVec, resultVec);
    TruncFunctionTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

// Test: Trunc to MONTH - basic cases
TEST(TruncTest, TruncToMonth) {
    std::cout << "=== Test: Trunc to MONTH - basic cases ===" << std::endl;
    
    // Create dates: 2024-06-15, 2024-12-31, 2025-03-20
    std::vector<int32_t> dateValues = {
        TruncFunctionTestHelper::DateToDays(2024, 6, 15),
        TruncFunctionTestHelper::DateToDays(2024, 12, 31),
        TruncFunctionTestHelper::DateToDays(2025, 3, 20)
    };
    
    // Expected: 2024-06-01, 2024-12-01, 2025-03-01
    std::vector<int32_t> expected = {
        TruncFunctionTestHelper::DateToDays(2024, 6, 1),
        TruncFunctionTestHelper::DateToDays(2024, 12, 1),
        TruncFunctionTestHelper::DateToDays(2025, 3, 1)
    };
    
    std::vector<std::string> formatValues = {"MONTH", "MONTH", "MONTH"};
    
    BaseVector* dateVec = TruncFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* formatVec = TruncFunctionTestHelper::CreateStringVector(formatValues);
    BaseVector* resultVec = nullptr;
    
    TruncFunctionTestHelper::ExecuteTrunc(dateVec, formatVec, resultVec);
    TruncFunctionTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

// Test: Trunc to QUARTER - basic cases
TEST(TruncTest, TruncToQuarter) {
    std::cout << "=== Test: Trunc to QUARTER - basic cases ===" << std::endl;
    
    // Create dates: 2024-06-15 (Q2), 2024-12-31 (Q4), 2025-03-20 (Q1)
    std::vector<int32_t> dateValues = {
        TruncFunctionTestHelper::DateToDays(2024, 6, 15),
        TruncFunctionTestHelper::DateToDays(2024, 12, 31),
        TruncFunctionTestHelper::DateToDays(2025, 3, 20)
    };
    
    // Expected: 2024-04-01 (Q2 start), 2024-10-01 (Q4 start), 2025-01-01 (Q1 start)
    std::vector<int32_t> expected = {
        TruncFunctionTestHelper::DateToDays(2024, 4, 1),
        TruncFunctionTestHelper::DateToDays(2024, 10, 1),
        TruncFunctionTestHelper::DateToDays(2025, 1, 1)
    };
    
    std::vector<std::string> formatValues = {"QUARTER", "QUARTER", "QUARTER"};
    
    BaseVector* dateVec = TruncFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* formatVec = TruncFunctionTestHelper::CreateStringVector(formatValues);
    BaseVector* resultVec = nullptr;
    
    TruncFunctionTestHelper::ExecuteTrunc(dateVec, formatVec, resultVec);
    TruncFunctionTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

// Test: Trunc to WEEK - basic cases
TEST(TruncTest, TruncToWeek) {
    std::cout << "=== Test: Trunc to WEEK - basic cases ===" << std::endl;
    
    // Create dates: 2024-06-15 (Saturday), 2024-06-17 (Monday), 2024-06-20 (Thursday)
    std::vector<int32_t> dateValues = {
        TruncFunctionTestHelper::DateToDays(2024, 6, 15),
        TruncFunctionTestHelper::DateToDays(2024, 6, 17),
        TruncFunctionTestHelper::DateToDays(2024, 6, 20)
    };
    
    // Expected: Monday of the week for each date
    // 2024-06-15 (Saturday) -> 2024-06-10 (Monday)
    // 2024-06-17 (Monday) -> 2024-06-17 (Monday)
    // 2024-06-20 (Thursday) -> 2024-06-17 (Monday)
    std::vector<int32_t> expected = {
        TruncFunctionTestHelper::DateToDays(2024, 6, 10),
        TruncFunctionTestHelper::DateToDays(2024, 6, 17),
        TruncFunctionTestHelper::DateToDays(2024, 6, 17)
    };
    
    std::vector<std::string> formatValues = {"WEEK", "WEEK", "WEEK"};
    
    BaseVector* dateVec = TruncFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* formatVec = TruncFunctionTestHelper::CreateStringVector(formatValues);
    BaseVector* resultVec = nullptr;
    
    TruncFunctionTestHelper::ExecuteTrunc(dateVec, formatVec, resultVec);
    TruncFunctionTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

// Test: Trunc with different format strings (case insensitive)
TEST(TruncTest, TruncWithDifferentFormats) {
    std::cout << "=== Test: Trunc with different format strings ===" << std::endl;
    
    // Create dates: 2024-06-15
    std::vector<int32_t> dateValues = {
        TruncFunctionTestHelper::DateToDays(2024, 6, 15),
        TruncFunctionTestHelper::DateToDays(2024, 6, 15),
        TruncFunctionTestHelper::DateToDays(2024, 6, 15)
    };
    
    // Test different format aliases: "YEAR", "YYYY", "YY"
    std::vector<int32_t> expected = {
        TruncFunctionTestHelper::DateToDays(2024, 1, 1),
        TruncFunctionTestHelper::DateToDays(2024, 1, 1),
        TruncFunctionTestHelper::DateToDays(2024, 1, 1)
    };
    
    std::vector<std::string> formatValues = {"YEAR", "YYYY", "YY"};
    
    BaseVector* dateVec = TruncFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* formatVec = TruncFunctionTestHelper::CreateStringVector(formatValues);
    BaseVector* resultVec = nullptr;
    
    TruncFunctionTestHelper::ExecuteTrunc(dateVec, formatVec, resultVec);
    TruncFunctionTestHelper::ValidateResult(resultVec, expected, dateValues.size());

    delete resultVec;
}

// Test: Trunc with NULL date values
TEST(TruncTest, TruncWithNullDate) {
    std::cout << "=== Test: Trunc with NULL date values ===" << std::endl;
    
    std::vector<int32_t> dateValues = {
        TruncFunctionTestHelper::DateToDays(2024, 6, 15),
        TruncFunctionTestHelper::DateToDays(2024, 12, 31),
        TruncFunctionTestHelper::DateToDays(2025, 3, 20)
    };
    
    std::vector<std::string> formatValues = {"MONTH", "MONTH", "MONTH"};
    
    BaseVector* dateVec = TruncFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* formatVec = TruncFunctionTestHelper::CreateStringVector(formatValues);
    
    // Set middle date to NULL
    dateVec->SetNull(1);
    
    BaseVector* resultVec = nullptr;
    TruncFunctionTestHelper::ExecuteTrunc(dateVec, formatVec, resultVec);
    
    // First and third should have values, second should be NULL
    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";

    delete resultVec;
}

// Test: Trunc with NULL format values
TEST(TruncTest, TruncWithNullFormat) {
    std::cout << "=== Test: Trunc with NULL format values ===" << std::endl;
    
    std::vector<int32_t> dateValues = {
        TruncFunctionTestHelper::DateToDays(2024, 6, 15),
        TruncFunctionTestHelper::DateToDays(2024, 12, 31),
        TruncFunctionTestHelper::DateToDays(2025, 3, 20)
    };
    
    std::vector<std::string> formatValues = {"MONTH", "MONTH", "MONTH"};
    
    BaseVector* dateVec = TruncFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* formatVec = TruncFunctionTestHelper::CreateStringVector(formatValues);
    
    // Set middle format to NULL
    formatVec->SetNull(1);
    
    BaseVector* resultVec = nullptr;
    TruncFunctionTestHelper::ExecuteTrunc(dateVec, formatVec, resultVec);
    
    // First and third should have values, second should be NULL (format is NULL)
    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL (format is NULL)";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";

    delete resultVec;
}

// Test: Trunc with invalid format
TEST(TruncTest, TruncWithInvalidFormat) {
    std::cout << "=== Test: Trunc with invalid format ===" << std::endl;
    
    std::vector<int32_t> dateValues = {
        TruncFunctionTestHelper::DateToDays(2024, 6, 15),
        TruncFunctionTestHelper::DateToDays(2024, 12, 31)
    };
    
    // Invalid format strings
    std::vector<std::string> formatValues = {"INVALID", "SECOND"};  // SECOND is not valid for DATE trunc
    
    BaseVector* dateVec = TruncFunctionTestHelper::CreateDate32Vector(dateValues);
    BaseVector* formatVec = TruncFunctionTestHelper::CreateStringVector(formatValues);
    BaseVector* resultVec = nullptr;
    
    TruncFunctionTestHelper::ExecuteTrunc(dateVec, formatVec, resultVec);
    
    // Both should be NULL (invalid format)
    EXPECT_TRUE(resultVec->IsNull(0)) << "Row 0 should be NULL (invalid format)";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL (invalid format for DATE)";

    delete resultVec;
}
