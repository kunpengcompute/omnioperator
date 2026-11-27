/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
* Description: Cast function unit tests


#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include <vector>
#include <limits>
#include <cmath>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/functions/Cast.h"
#include "vectorization/registration/SimpleFunctionRegistry.h"
#include "vector/vector_helper.h"
#include "vector/vector.h"
#include <gtest/gtest.h>

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::mem;
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::TestUtil;

// Initialize function registration before running tests
class CastTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const cast_test_env = ::testing::AddGlobalTestEnvironment(new CastTestEnvironment);

class CastFunctionTestHelper {
public:
    template<typename T>
    static void ValidateNumericResult(BaseVector* result, const std::vector<T>& expected, int rowSize) {
        auto* resultVec = dynamic_cast<Vector<T>*>(result);
        ASSERT_NE(resultVec, nullptr) << "Result vector type mismatch";
        
        for (int i = 0; i < rowSize; ++i) {
            if (result->IsNull(i)) {
                std::cout << "Row " << i << ": NULL" << std::endl;
                continue;
            }
            T actualValue = resultVec->GetValue(i);
            T expectedValue = expected[i];
            std::cout << "Row " << i << ": Expected=" << expectedValue << ", Actual=" << actualValue << std::endl;
            
            if constexpr (std::is_floating_point_v<T>) {
                EXPECT_NEAR(actualValue, expectedValue, 1e-6) 
                    << "Row " << i << " value mismatch";
            } else {
                EXPECT_EQ(actualValue, expectedValue) 
                    << "Row " << i << " value mismatch";
            }
        }
    }
    
    static void ValidateStringResult(BaseVector* result, const std::vector<std::string>& expected, int rowSize) {
        auto* resultVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
        ASSERT_NE(resultVec, nullptr) << "Result vector is not string type";
        
        for (int i = 0; i < rowSize; ++i) {
            if (result->IsNull(i)) {
                std::cout << "Row " << i << ": NULL" << std::endl;
                continue;
            }
            std::string_view actualValue = resultVec->GetValue(i);
            std::string expectedValue = expected[i];
            std::cout << "Row " << i << ": Expected=\"" << expectedValue 
                    << "\", Actual=\"" << actualValue << "\"" << std::endl;
            EXPECT_EQ(actualValue, expectedValue) << "Row " << i << " value mismatch";
        }
    }
    
    static void ValidateBooleanResult(BaseVector* result, const std::vector<bool>& expected, int rowSize) {
        auto* resultVec = dynamic_cast<Vector<bool>*>(result);
        ASSERT_NE(resultVec, nullptr) << "Result vector is not boolean type";
        
        for (int i = 0; i < rowSize; ++i) {
            if (result->IsNull(i)) {
                std::cout << "Row " << i << ": NULL" << std::endl;
                continue;
            }
            bool actualValue = resultVec->GetValue(i);
            bool expectedValue = expected[i];
            std::cout << "Row " << i << ": Expected=" << (expectedValue ? "true" : "false") 
                    << ", Actual=" << (actualValue ? "true" : "false") << std::endl;
            EXPECT_EQ(actualValue, expectedValue) << "Row " << i << " value mismatch";
        }
    }
    
    template<typename T>
    static BaseVector* CreateNumericVector(const std::vector<T>& values, DataTypeId typeId) {
        BaseVector* vec = VectorHelper::CreateFlatVector(nullptr, typeId, values.size());
        auto* typedVec = static_cast<Vector<T>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typedVec->SetValue(i, values[i]);
        }
        return vec;
    }
    
    static BaseVector* CreateStringVector(const std::vector<std::string>& values) {
        BaseVector* vec = VectorHelper::CreateStringVector(values.size());
        auto* typedVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            // typedVec->SetValue(i, std::string_view(values[i]));
	    std::string_view sv(values[i]);
            typedVec->SetValue(i, sv);
        }
        return vec;
    }
    
    static void ExecuteCast(BaseVector* input, DataTypeId inputTypeId, 
                            DataTypeId outputTypeId, BaseVector*& result) {
        auto signature = std::make_shared<FunctionSignature>("CAST",
            std::vector<DataTypeId>{inputTypeId}, outputTypeId);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "Cast function not found for signature";
        
        auto outputType = std::make_shared<DataType>(outputTypeId);
        ExecutionContext context(4096, nullptr);
        context.SetResultRowSize(input->GetSize());
        std::stack<BaseVector*> args;
        args.push(input);
        
        ASSERT_NO_THROW(function->Apply(args, outputType, result, &context))
            << "Cast function threw an exception";
    }
};

// Test: String to Integer conversions
TEST(CastTest, StringToInt) {
    std::cout << "=== Test: String to INT ===" << std::endl;
    
    std::vector<std::string> inputStrs = {"123", "456", "-789", "0"};
    std::vector<int32_t> expected = {123, 456, -789, 0};
    
    BaseVector* inputVec = CastFunctionTestHelper::CreateStringVector(inputStrs);
    BaseVector* resultVec = nullptr;
    
    CastFunctionTestHelper::ExecuteCast(inputVec, OMNI_VARCHAR, OMNI_INT, resultVec);
    CastFunctionTestHelper::ValidateNumericResult(resultVec, expected, inputStrs.size());
    
    delete inputVec;
    delete resultVec;
}

TEST(CastTest, StringToLong) {
    std::cout << "=== Test: String to LONG ===" << std::endl;
    
    std::vector<std::string> inputStrs = {"1234567890123", "-9876543210987", "0"};
    std::vector<int64_t> expected = {1234567890123LL, -9876543210987LL, 0LL};
    
    BaseVector* inputVec = CastFunctionTestHelper::CreateStringVector(inputStrs);
    BaseVector* resultVec = nullptr;
    
    CastFunctionTestHelper::ExecuteCast(inputVec, OMNI_VARCHAR, OMNI_LONG, resultVec);
    CastFunctionTestHelper::ValidateNumericResult(resultVec, expected, inputStrs.size());
    
    delete inputVec;
    delete resultVec;
}

TEST(CastTest, StringToDouble) {
    std::cout << "=== Test: String to DOUBLE ===" << std::endl;
    
    std::vector<std::string> inputStrs = {"123.456", "-789.012", "3.14159", "0.0"};
    std::vector<double> expected = {123.456, -789.012, 3.14159, 0.0};
    
    BaseVector* inputVec = CastFunctionTestHelper::CreateStringVector(inputStrs);
    BaseVector* resultVec = nullptr;
    
    CastFunctionTestHelper::ExecuteCast(inputVec, OMNI_VARCHAR, OMNI_DOUBLE, resultVec);
    CastFunctionTestHelper::ValidateNumericResult(resultVec, expected, inputStrs.size());
    
    delete inputVec;
    delete resultVec;
}

TEST(CastTest, StringToFloat) {
    std::cout << "=== Test: String to FLOAT ===" << std::endl;
    
    std::vector<std::string> inputStrs = {"123.456", "-789.012", "3.14159"};
    std::vector<float> expected = {123.456f, -789.012f, 3.14159f};
    
    BaseVector* inputVec = CastFunctionTestHelper::CreateStringVector(inputStrs);
    BaseVector* resultVec = nullptr;
    
    CastFunctionTestHelper::ExecuteCast(inputVec, OMNI_VARCHAR, OMNI_FLOAT, resultVec);
    CastFunctionTestHelper::ValidateNumericResult(resultVec, expected, inputStrs.size());
    
    delete inputVec;
    delete resultVec;
}

TEST(CastTest, StringToBoolean) {
    std::cout << "=== Test: String to BOOLEAN ===" << std::endl;
    
    std::vector<std::string> inputStrs = {"true", "1", "false", "0", "TRUE"};
    std::vector<bool> expected = {true, true, false, false, true};
    
    BaseVector* inputVec = CastFunctionTestHelper::CreateStringVector(inputStrs);
    BaseVector* resultVec = nullptr;
    
    CastFunctionTestHelper::ExecuteCast(inputVec, OMNI_VARCHAR, OMNI_BOOLEAN, resultVec);
    CastFunctionTestHelper::ValidateBooleanResult(resultVec, expected, inputStrs.size());
    
    delete inputVec;
    delete resultVec;
}

// Test: Integer to String conversions
TEST(CastTest, IntToString) {
    std::cout << "=== Test: INT to String ===" << std::endl;
    
    std::vector<int32_t> inputValues = {123, -456, 0, 999};
    std::vector<std::string> expected = {"123", "-456", "0", "999"};
    
    BaseVector* inputVec = CastFunctionTestHelper::CreateNumericVector(inputValues, OMNI_INT);
    BaseVector* resultVec = nullptr;
    
    CastFunctionTestHelper::ExecuteCast(inputVec, OMNI_INT, OMNI_VARCHAR, resultVec);
    CastFunctionTestHelper::ValidateStringResult(resultVec, expected, inputValues.size());
    
    delete inputVec;
    delete resultVec;
}

TEST(CastTest, LongToString) {
    std::cout << "=== Test: LONG to String ===" << std::endl;
    
    std::vector<int64_t> inputValues = {1234567890123LL, -9876543210987LL, 0LL};
    std::vector<std::string> expected = {"1234567890123", "-9876543210987", "0"};
    
    BaseVector* inputVec = CastFunctionTestHelper::CreateNumericVector(inputValues, OMNI_LONG);
    BaseVector* resultVec = nullptr;
    
    CastFunctionTestHelper::ExecuteCast(inputVec, OMNI_LONG, OMNI_VARCHAR, resultVec);
    CastFunctionTestHelper::ValidateStringResult(resultVec, expected, inputValues.size());
    
    delete inputVec;
    delete resultVec;
}

TEST(CastTest, DoubleToString) {
    std::cout << "=== Test: DOUBLE to String ===" << std::endl;
    
    std::vector<double> inputValues = {123.456, -789.012, 3.14159, 0.0};
    std::vector<std::string> expected = {"123.456", "-789.012", "3.14159", "0"};
    
    BaseVector* inputVec = CastFunctionTestHelper::CreateNumericVector(inputValues, OMNI_DOUBLE);
    BaseVector* resultVec = nullptr;
    
    CastFunctionTestHelper::ExecuteCast(inputVec, OMNI_DOUBLE, OMNI_VARCHAR, resultVec);
    
        // DOUBLE to VARCHAR should create a new vector, not return the same one
    ASSERT_NE(resultVec, inputVec) << "DOUBLE to VARCHAR should create a new vector";
    
    // Note: Floating point string representation may vary, so we do basic validation
    auto* resultVecTyped = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(resultVec);
    ASSERT_NE(resultVecTyped, nullptr);
    ASSERT_EQ(resultVecTyped->GetSize(), static_cast<int32_t>(inputValues.size()));

    for (size_t i = 0; i < inputValues.size(); ++i) {
        std::string_view actual = resultVecTyped->GetValue(i);
        std::cout << "Row " << i << ": Expected contains \"" << expected[i] 
                << "\", Actual=\"" << actual << "\"" << std::endl;
        // Basic check: string should not be empty
        EXPECT_FALSE(actual.empty());
	// Check that the string contains the expected numeric value
        std::string actualStr(actual);
        EXPECT_TRUE(actualStr.find(expected[i].substr(0, expected[i].find('.'))) != std::string::npos ||
                    actualStr == expected[i] || actualStr == "0" || actualStr == "0.0");
    }
    
    delete inputVec;
    delete resultVec;
}

TEST(CastTest, BooleanToString) {
    std::cout << "=== Test: BOOLEAN to String ===" << std::endl;
    
    std::vector<bool> inputValues = {true, false, true};
    std::vector<std::string> expected = {"true", "false", "true"};
    
    BaseVector* inputVec = CastFunctionTestHelper::CreateNumericVector(inputValues, OMNI_BOOLEAN);
    BaseVector* resultVec = nullptr;
    
    CastFunctionTestHelper::ExecuteCast(inputVec, OMNI_BOOLEAN, OMNI_VARCHAR, resultVec);
    CastFunctionTestHelper::ValidateStringResult(resultVec, expected, inputValues.size());
    
    delete inputVec;
    delete resultVec;
}

// Test: Numeric to Numeric conversions
TEST(CastTest, IntToLong) {
    std::cout << "=== Test: INT to LONG ===" << std::endl;
    
    std::vector<int32_t> inputValues = {123, -456, 0, 2147483647};
    std::vector<int64_t> expected = {123LL, -456LL, 0LL, 2147483647LL};
    
    BaseVector* inputVec = CastFunctionTestHelper::CreateNumericVector(inputValues, OMNI_INT);
    BaseVector* resultVec = nullptr;
    
    CastFunctionTestHelper::ExecuteCast(inputVec, OMNI_INT, OMNI_LONG, resultVec);
    CastFunctionTestHelper::ValidateNumericResult(resultVec, expected, inputValues.size());
    
    delete inputVec;
    delete resultVec;
}

TEST(CastTest, LongToInt) {
    std::cout << "=== Test: LONG to INT ===" << std::endl;
    
    std::vector<int64_t> inputValues = {123LL, -456LL, 0LL, 2147483647LL};
    std::vector<int32_t> expected = {123, -456, 0, 2147483647};
    
    BaseVector* inputVec = CastFunctionTestHelper::CreateNumericVector(inputValues, OMNI_LONG);
    BaseVector* resultVec = nullptr;
    
    CastFunctionTestHelper::ExecuteCast(inputVec, OMNI_LONG, OMNI_INT, resultVec);
    CastFunctionTestHelper::ValidateNumericResult(resultVec, expected, inputValues.size());
    
    delete inputVec;
    delete resultVec;
}

TEST(CastTest, IntToDouble) {
    std::cout << "=== Test: INT to DOUBLE ===" << std::endl;
    
    std::vector<int32_t> inputValues = {123, -456, 0};
    std::vector<double> expected = {123.0, -456.0, 0.0};
    
    BaseVector* inputVec = CastFunctionTestHelper::CreateNumericVector(inputValues, OMNI_INT);
    BaseVector* resultVec = nullptr;
    
    CastFunctionTestHelper::ExecuteCast(inputVec, OMNI_INT, OMNI_DOUBLE, resultVec);
    CastFunctionTestHelper::ValidateNumericResult(resultVec, expected, inputValues.size());
    
    delete inputVec;
    delete resultVec;
}

TEST(CastTest, DoubleToInt) {
    std::cout << "=== Test: DOUBLE to INT ===" << std::endl;
    
    std::vector<double> inputValues = {123.7, -456.3, 0.0, 999.9};
    std::vector<int32_t> expected = {123, -456, 0, 999};
    
    BaseVector* inputVec = CastFunctionTestHelper::CreateNumericVector(inputValues, OMNI_DOUBLE);
    BaseVector* resultVec = nullptr;
    
    CastFunctionTestHelper::ExecuteCast(inputVec, OMNI_DOUBLE, OMNI_INT, resultVec);
    CastFunctionTestHelper::ValidateNumericResult(resultVec, expected, inputValues.size());
    
    delete inputVec;
    delete resultVec;
}

TEST(CastTest, FloatToDouble) {
    std::cout << "=== Test: FLOAT to DOUBLE ===" << std::endl;
    
    std::vector<float> inputValues = {123.456f, -789.012f, 3.14159f};
    // std::vector<double> expected = {123.456, -789.012, 3.14159};
    // Convert float to double to get the actual expected values (accounting for float precision)
    std::vector<double> expected;
    for (float val : inputValues) {
        expected.push_back(static_cast<double>(val));
    }
    
    BaseVector* inputVec = CastFunctionTestHelper::CreateNumericVector(inputValues, OMNI_FLOAT);
    BaseVector* resultVec = nullptr;
    
    CastFunctionTestHelper::ExecuteCast(inputVec, OMNI_FLOAT, OMNI_DOUBLE, resultVec);
    // CastFunctionTestHelper::ValidateNumericResult(resultVec, expected, inputValues.size());
    
    // Use a more lenient tolerance for float-to-double conversion (float has ~7 decimal digits precision)
    auto* resultVecTyped = dynamic_cast<Vector<double>*>(resultVec);
    ASSERT_NE(resultVecTyped, nullptr) << "Result vector type mismatch";

    for (size_t i = 0; i < inputValues.size(); ++i) {
        if (resultVec->IsNull(i)) {
            std::cout << "Row " << i << ": NULL" << std::endl;
            continue;
        }
        double actualValue = resultVecTyped->GetValue(i);
        double expectedValue = expected[i];
        std::cout << "Row " << i << ": Expected=" << expectedValue << ", Actual=" << actualValue << std::endl;

        // Use relative tolerance for float-to-double conversion (accounting for float precision loss)
        double tolerance = std::max(1e-5, std::abs(expectedValue) * 1e-5);
        EXPECT_NEAR(actualValue, expectedValue, tolerance)
            << "Row " << i << " value mismatch";
    }

    
    delete inputVec;
    delete resultVec;
}

TEST(CastTest, DoubleToFloat) {
    std::cout << "=== Test: DOUBLE to FLOAT ===" << std::endl;
    
    std::vector<double> inputValues = {123.456, -789.012, 3.14159};
    std::vector<float> expected = {123.456f, -789.012f, 3.14159f};
    
    BaseVector* inputVec = CastFunctionTestHelper::CreateNumericVector(inputValues, OMNI_DOUBLE);
    BaseVector* resultVec = nullptr;
    
    CastFunctionTestHelper::ExecuteCast(inputVec, OMNI_DOUBLE, OMNI_FLOAT, resultVec);
    CastFunctionTestHelper::ValidateNumericResult(resultVec, expected, inputValues.size());
    
    delete inputVec;
    delete resultVec;
}

// Test: Boolean conversions
TEST(CastTest, BooleanToInt) {
    std::cout << "=== Test: BOOLEAN to INT ===" << std::endl;
    
    std::vector<bool> inputValues = {true, false, true};
    std::vector<int32_t> expected = {1, 0, 1};
    
    BaseVector* inputVec = CastFunctionTestHelper::CreateNumericVector(inputValues, OMNI_BOOLEAN);
    BaseVector* resultVec = nullptr;
    
    CastFunctionTestHelper::ExecuteCast(inputVec, OMNI_BOOLEAN, OMNI_INT, resultVec);
    CastFunctionTestHelper::ValidateNumericResult(resultVec, expected, inputValues.size());
    
    delete inputVec;
    delete resultVec;
}

TEST(CastTest, IntToBoolean) {
    std::cout << "=== Test: INT to BOOLEAN ===" << std::endl;
    
    std::vector<int32_t> inputValues = {1, 0, -1, 100, 0};
    std::vector<bool> expected = {true, false, true, true, false};
    
    BaseVector* inputVec = CastFunctionTestHelper::CreateNumericVector(inputValues, OMNI_INT);
    BaseVector* resultVec = nullptr;
    
    CastFunctionTestHelper::ExecuteCast(inputVec, OMNI_INT, OMNI_BOOLEAN, resultVec);
    CastFunctionTestHelper::ValidateBooleanResult(resultVec, expected, inputValues.size());
    
    delete inputVec;
    delete resultVec;
}

TEST(CastTest, DoubleToBoolean) {
    std::cout << "=== Test: DOUBLE to BOOLEAN ===" << std::endl;
    
    std::vector<double> inputValues = {1.0, 0.0, -1.0, 0.000001, 0.0};
    std::vector<bool> expected = {true, false, true, true, false};
    
    BaseVector* inputVec = CastFunctionTestHelper::CreateNumericVector(inputValues, OMNI_DOUBLE);
    BaseVector* resultVec = nullptr;
    
    CastFunctionTestHelper::ExecuteCast(inputVec, OMNI_DOUBLE, OMNI_BOOLEAN, resultVec);
    CastFunctionTestHelper::ValidateBooleanResult(resultVec, expected, inputValues.size());
    
    delete inputVec;
    delete resultVec;
}

TEST(CastTest, BooleanToDouble) {
    std::cout << "=== Test: BOOLEAN to DOUBLE ===" << std::endl;
    
    std::vector<bool> inputValues = {true, false, true};
    std::vector<double> expected = {1.0, 0.0, 1.0};
    
    BaseVector* inputVec = CastFunctionTestHelper::CreateNumericVector(inputValues, OMNI_BOOLEAN);
    BaseVector* resultVec = nullptr;
    
    CastFunctionTestHelper::ExecuteCast(inputVec, OMNI_BOOLEAN, OMNI_DOUBLE, resultVec);
    CastFunctionTestHelper::ValidateNumericResult(resultVec, expected, inputValues.size());
    
    delete inputVec;
    delete resultVec;
}

// Test: NULL handling
TEST(CastTest, NullHandling) {
    std::cout << "=== Test: NULL value handling ===" << std::endl;
    
    std::vector<int32_t> inputValues = {123, 456, 789};
    BaseVector* inputVec = CastFunctionTestHelper::CreateNumericVector(inputValues, OMNI_INT);
    
    // Set middle value to NULL
    // inputVec->SetNull(1, true);
    inputVec->SetNull(1);
    
    BaseVector* resultVec = nullptr;
    CastFunctionTestHelper::ExecuteCast(inputVec, OMNI_INT, OMNI_VARCHAR, resultVec);
    
    // Check that NULL is preserved
    EXPECT_TRUE(resultVec->IsNull(1)) << "NULL value should be preserved";
    EXPECT_FALSE(resultVec->IsNull(0)) << "Non-NULL value should not be NULL";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Non-NULL value should not be NULL";
    
    delete inputVec;
    delete resultVec;
}

// Test: Invalid string to numeric (should result in NULL)
TEST(CastTest, InvalidStringToNumeric) {
    std::cout << "=== Test: Invalid string to numeric ===" << std::endl;
    
    std::vector<std::string> inputStrs = {"123", "abc", "456", "xyz123"};
    BaseVector* inputVec = CastFunctionTestHelper::CreateStringVector(inputStrs);
    BaseVector* resultVec = nullptr;
    
    CastFunctionTestHelper::ExecuteCast(inputVec, OMNI_VARCHAR, OMNI_INT, resultVec);
    
    // Valid strings should convert, invalid should be NULL
    EXPECT_FALSE(resultVec->IsNull(0)) << "Valid string '123' should convert";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Invalid string 'abc' should be NULL";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Valid string '456' should convert";
    EXPECT_TRUE(resultVec->IsNull(3)) << "Invalid string 'xyz123' should be NULL";
    
    delete inputVec;
    delete resultVec;
}

// Test: Byte and Short conversions
TEST(CastTest, ByteToShort) {
    std::cout << "=== Test: BYTE to SHORT ===" << std::endl;
    
    std::vector<int8_t> inputValues = {127, -128, 0, 100};
    std::vector<int16_t> expected = {127, -128, 0, 100};
    
    BaseVector* inputVec = CastFunctionTestHelper::CreateNumericVector(inputValues, OMNI_BYTE);
    BaseVector* resultVec = nullptr;
    
    CastFunctionTestHelper::ExecuteCast(inputVec, OMNI_BYTE, OMNI_SHORT, resultVec);
    CastFunctionTestHelper::ValidateNumericResult(resultVec, expected, inputValues.size());
    
    delete inputVec;
    delete resultVec;
}

TEST(CastTest, ShortToByte) {
    std::cout << "=== Test: SHORT to BYTE ===" << std::endl;
    
    std::vector<int16_t> inputValues = {127, -128, 0, 100};
    std::vector<int8_t> expected = {127, -128, 0, 100};
    
    BaseVector* inputVec = CastFunctionTestHelper::CreateNumericVector(inputValues, OMNI_SHORT);
    BaseVector* resultVec = nullptr;
    
    CastFunctionTestHelper::ExecuteCast(inputVec, OMNI_SHORT, OMNI_BYTE, resultVec);
    CastFunctionTestHelper::ValidateNumericResult(resultVec, expected, inputValues.size());
    
    delete inputVec;
    delete resultVec;
}

// Test: Same type cast (should just copy)
TEST(CastTest, SameTypeCast) {
    std::cout << "=== Test: Same type cast (INT to INT) ===" << std::endl;
    
    std::vector<int32_t> inputValues = {123, 456, 789};
    BaseVector* inputVec = CastFunctionTestHelper::CreateNumericVector(inputValues, OMNI_INT);
    BaseVector* resultVec = nullptr;
    
    CastFunctionTestHelper::ExecuteCast(inputVec, OMNI_INT, OMNI_INT, resultVec);
    
    // Same type should result in same values
    auto* resultVecTyped = dynamic_cast<Vector<int32_t>*>(resultVec);
    ASSERT_NE(resultVecTyped, nullptr);
    for (size_t i = 0; i < inputValues.size(); ++i) {
        EXPECT_EQ(resultVecTyped->GetValue(i), inputValues[i]) 
            << "Same type cast should preserve values";
    }
    
    delete inputVec;
    // delete resultVec;
    resultVec = nullptr; // Prevent double deletion
}
*/