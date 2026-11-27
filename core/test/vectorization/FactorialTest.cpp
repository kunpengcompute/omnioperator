 /*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
* Description: Unit tests for factorial function
*/

#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <cmath>
#include <limits>
#include <optional>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/ExprEval.h"
#include "vectorization/functions/MathFunctions.h"
#include "expression/expressions.h"
#include "type/data_type.h"
#include "vector/vector_helper.h"
#include "codegen/func_registry.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::op;
using namespace omniruntime::expressions;
using namespace omniruntime::TestUtil;
using namespace omniruntime::codegen;
using namespace omniruntime::type;

class FactorialTest : public ::testing::Test {
protected:
    void SetUp() override {
        RegisterFunctions::Register();
    }
};

// Helper function to test factorial operation
// Returns std::nullopt for NULL results (negative or > 20)
std::optional<int64_t> ExpectedFactorial(int32_t n) {
    static constexpr int64_t kFactorials[21] = {
        1LL,                    // 0!
        1LL,                    // 1!
        2LL,                    // 2!
        6LL,                    // 3!
        24LL,                   // 4!
        120LL,                  // 5!
        720LL,                  // 6!
        5040LL,                 // 7!
        40320LL,                // 8!
        362880LL,               // 9!
        3628800LL,              // 10!
        39916800LL,             // 11!
        479001600LL,            // 12!
        6227020800LL,           // 13!
        87178291200LL,          // 14!
        1307674368000LL,        // 15!
        20922789888000LL,       // 16!
        355687428096000LL,      // 17!
        6402373705728000LL,     // 18!
        121645100408832000LL,   // 19!
        2432902008176640000LL   // 20!
    };
    
    if (n < 0 || n > 20) {
        return std::nullopt;
    }
    return kFactorials[n];
}

// Test factorial with basic valid inputs (0-20)
TEST_F(FactorialTest, FactorialBasicValid) {
    
    int32_t rowSize = 8;
    auto returnType = std::make_shared<DataType>(OMNI_LONG);
    auto inputType = std::make_shared<DataType>(OMNI_INT);
    std::vector<Expr*> args = {new FieldExpr(0, inputType)};
    auto funcExpr = new FuncExpr("factorial", args, returnType);
    
    // Test values: 0, 1, 2, 5, 10, 15, 20, 3
    int32_t col1[8] = {0, 1, 2, 5, 10, 15, 20, 3};
    std::vector vecOfTypes = {IntType()};
    DataTypes inputTypes(vecOfTypes);
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1);
    
    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    
    ExprEval e(input, context);
    e.Visit(*funcExpr);
    auto result = e.GetResult();
    
    auto *resultVector = dynamic_cast<Vector<int64_t> *>(result);
    ASSERT_NE(resultVector, nullptr);
    
    // Expected results: 1, 1, 2, 120, 3628800, 1307674368000, 2432902008176640000, 6
    std::vector<int64_t> expectedResults = {1, 1, 2, 120, 3628800, 1307674368000LL, 2432902008176640000LL, 6};
    
    for (int32_t i = 0; i < rowSize; ++i) {
        EXPECT_FALSE(result->IsNull(i)) << "Result should not be NULL at index " << i;
        int64_t actualResult = resultVector->GetValue(i);
        int64_t expectedResult = expectedResults[i];
        EXPECT_EQ(actualResult, expectedResult)
            << "Value mismatch at index " << i << " for factorial(" << col1[i] << ")"
            << ", expected=" << expectedResult << ", actual=" << actualResult;
    }
    
    delete result;
    delete input;
    delete funcExpr;
    delete context;
}

// Test factorial with out-of-range inputs (negative and > 20)
TEST_F(FactorialTest, FactorialOutOfRange) {
    
    int32_t rowSize = 5;
    auto returnType = std::make_shared<DataType>(OMNI_LONG);
    auto inputType = std::make_shared<DataType>(OMNI_INT);
    std::vector<Expr*> args = {new FieldExpr(0, inputType)};
    auto funcExpr = new FuncExpr("factorial", args, returnType);
    
    // Test values: -1, 21, -5, 25, 100 (all should return NULL)
    int32_t col1[5] = {-1, 21, -5, 25, 100};
    std::vector vecOfTypes = {IntType()};
    DataTypes inputTypes(vecOfTypes);
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1);
    
    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    
    ExprEval e(input, context);
    e.Visit(*funcExpr);
    auto result = e.GetResult();
    
    // All results should be NULL for out-of-range inputs
    for (int32_t i = 0; i < rowSize; ++i) {
        EXPECT_TRUE(result->IsNull(i))
            << "Result should be NULL at index " << i << " for factorial(" << col1[i] << ")";
    }
    
    delete result;
    delete input;
    delete funcExpr;
    delete context;
}

// Test factorial with NULL input
TEST_F(FactorialTest, FactorialWithNullInput) {
    
    int32_t rowSize = 4;
    auto returnType = std::make_shared<DataType>(OMNI_LONG);
    auto inputType = std::make_shared<DataType>(OMNI_INT);
    std::vector<Expr*> args = {new FieldExpr(0, inputType)};
    auto funcExpr = new FuncExpr("factorial", args, returnType);
    
    // Test values: 5, 10, 3, 7
    int32_t col1[4] = {5, 10, 3, 7};
    std::vector vecOfTypes = {IntType()};
    DataTypes inputTypes(vecOfTypes);
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1);
    
    // Set first and third values to NULL
    input->Get(0)->SetNull(0);
    input->Get(0)->SetNull(2);
    
    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    
    ExprEval e(input, context);
    e.Visit(*funcExpr);
    auto result = e.GetResult();
    
    auto *resultVector = dynamic_cast<Vector<int64_t> *>(result);
    
    // First and third should be NULL (NULL input propagates to NULL output)
    EXPECT_TRUE(result->IsNull(0)) << "Result should be NULL when input is NULL at index 0";
    EXPECT_FALSE(result->IsNull(1)) << "Result should not be NULL at index 1";
    EXPECT_TRUE(result->IsNull(2)) << "Result should be NULL when input is NULL at index 2";
    EXPECT_FALSE(result->IsNull(3)) << "Result should not be NULL at index 3";
    
    // Check non-NULL values
    EXPECT_EQ(resultVector->GetValue(1), 3628800LL) << "factorial(10) should be 3628800";
    EXPECT_EQ(resultVector->GetValue(3), 5040LL) << "factorial(7) should be 5040";
    
    delete result;
    delete input;
    delete funcExpr;
    delete context;
}

// Test factorial with mixed inputs (valid, invalid, edge cases)
TEST_F(FactorialTest, FactorialMixedInputs) {
    
    int32_t rowSize = 7;
    auto returnType = std::make_shared<DataType>(OMNI_LONG);
    auto inputType = std::make_shared<DataType>(OMNI_INT);
    std::vector<Expr*> args = {new FieldExpr(0, inputType)};
    auto funcExpr = new FuncExpr("factorial", args, returnType);
    
    // Mixed test values: 3, 5, -3, 25, 10, 15, -1
    int32_t col1[7] = {3, 5, -3, 25, 10, 15, -1};
    std::vector vecOfTypes = {IntType()};
    DataTypes inputTypes(vecOfTypes);
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1);
    
    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    
    ExprEval e(input, context);
    e.Visit(*funcExpr);
    auto result = e.GetResult();
    
    auto *resultVector = dynamic_cast<Vector<int64_t> *>(result);
    
    // index 0: factorial(3) = 6 (valid)
    EXPECT_FALSE(result->IsNull(0));
    EXPECT_EQ(resultVector->GetValue(0), 6LL);
    
    // index 1: factorial(5) = 120 (valid)
    EXPECT_FALSE(result->IsNull(1));
    EXPECT_EQ(resultVector->GetValue(1), 120LL);
    
    // index 2: factorial(-3) = NULL (out of range)
    EXPECT_TRUE(result->IsNull(2)) << "factorial(-3) should be NULL";
    
    // index 3: factorial(25) = NULL (out of range)
    EXPECT_TRUE(result->IsNull(3)) << "factorial(25) should be NULL";
    
    // index 4: factorial(10) = 3628800 (valid)
    EXPECT_FALSE(result->IsNull(4));
    EXPECT_EQ(resultVector->GetValue(4), 3628800LL);
    
    // index 5: factorial(15) = 1307674368000 (valid)
    EXPECT_FALSE(result->IsNull(5));
    EXPECT_EQ(resultVector->GetValue(5), 1307674368000LL);
    
    // index 6: factorial(-1) = NULL (out of range)
    EXPECT_TRUE(result->IsNull(6)) << "factorial(-1) should be NULL";
    
    delete result;
    delete input;
    delete funcExpr;
    delete context;
}

// Test factorial boundary values (0 and 20)
TEST_F(FactorialTest, FactorialBoundaryValues) {
    
    int32_t rowSize = 4;
    auto returnType = std::make_shared<DataType>(OMNI_LONG);
    auto inputType = std::make_shared<DataType>(OMNI_INT);
    std::vector<Expr*> args = {new FieldExpr(0, inputType)};
    auto funcExpr = new FuncExpr("factorial", args, returnType);
    
    // Test boundary values: 0, 20, -1 (just below), 21 (just above)
    int32_t col1[4] = {0, 20, -1, 21};
    std::vector vecOfTypes = {IntType()};
    DataTypes inputTypes(vecOfTypes);
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1);
    
    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    
    ExprEval e(input, context);
    e.Visit(*funcExpr);
    auto result = e.GetResult();
    
    auto *resultVector = dynamic_cast<Vector<int64_t> *>(result);
    
    // factorial(0) = 1
    EXPECT_FALSE(result->IsNull(0));
    EXPECT_EQ(resultVector->GetValue(0), 1LL) << "factorial(0) should be 1";
    
    // factorial(20) = 2432902008176640000
    EXPECT_FALSE(result->IsNull(1));
    EXPECT_EQ(resultVector->GetValue(1), 2432902008176640000LL) << "factorial(20) should be 2432902008176640000";
    
    // factorial(-1) = NULL
    EXPECT_TRUE(result->IsNull(2)) << "factorial(-1) should be NULL";
    
    // factorial(21) = NULL
    EXPECT_TRUE(result->IsNull(3)) << "factorial(21) should be NULL";
    
    delete result;
    delete input;
    delete funcExpr;
    delete context;
}

// Test factorial using VectorFunction directly
TEST_F(FactorialTest, FactorialVectorFunction) {
    
    int32_t rowSize = 5;
    
    // Create input vector
    BaseVector* rawInput = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, rowSize);
    auto* inputVector = static_cast<Vector<int32_t>*>(rawInput);
    std::vector<int32_t> inputData = {0, 5, 10, 15, 20};
    for (int32_t i = 0; i < rowSize; ++i) {
        inputVector->SetValue(i, inputData[i]);
        inputVector->SetNotNull(i);
    }
    
    // Create function signature: factorial(int) -> long
    std::vector<DataTypeId> argTypes = {OMNI_INT};
    auto signature = std::make_shared<FunctionSignature>("factorial", argTypes, OMNI_LONG);
    auto vectorFunction = VectorFunction::Find(signature);
    ASSERT_NE(vectorFunction, nullptr) << "Function factorial not found";
    
    ExecutionContext context(4096, nullptr);
    context.SetResultRowSize(rowSize);
    
    std::stack<BaseVector*> args;
    args.push(rawInput);
    
    BaseVector* rawResult = nullptr;
    auto resultType = std::make_shared<DataType>(OMNI_LONG);
    vectorFunction->Apply(args, resultType, rawResult, &context);
    ASSERT_NE(rawResult, nullptr);
    
    auto* resultVector = static_cast<Vector<int64_t>*>(rawResult);
    ASSERT_NE(resultVector, nullptr);
    
    std::vector<int64_t> expectedResults = {1, 120, 3628800, 1307674368000LL, 2432902008176640000LL};
    
    for (int32_t i = 0; i < rowSize; ++i) {
        EXPECT_FALSE(rawResult->IsNull(i)) << "Result should not be NULL at index " << i;
        int64_t actual = resultVector->GetValue(i);
        int64_t expected = expectedResults[i];
        EXPECT_EQ(actual, expected)
            << "Value mismatch at index " << i << " for factorial(" << inputData[i] << ")"
            << ", expected=" << expected << ", actual=" << actual;
    }
    
    // Note: rawInput is already freed by FlatVectorReader's destructor inside Apply(),
    // because GetIsField() is false for vectors created by VectorHelper::CreateFlatVector.
    // Do NOT delete rawInput here to avoid double-free (heap-use-after-free).
    delete rawResult;
}