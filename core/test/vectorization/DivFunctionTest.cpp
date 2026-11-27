/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: Unit tests for div (integral division) function
 */

#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <cstring>
#include <limits>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/ExprEval.h"
#include "vectorization/functions/Arithmetic.h"
#include "expression/expressions.h"
#include "type/data_type.h"
#include "vector/vector_helper.h"
#include "vector/vector_common.h"
#include "codegen/func_registry.h"
#include "type/decimal_operations.h"
#include "type/decimal128.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::op;
using namespace omniruntime::expressions;
using namespace omniruntime::TestUtil;
using namespace omniruntime::codegen;
using namespace omniruntime::type;

class DivFunctionTest : public ::testing::Test {
protected:
    void SetUp() override {
        RegisterFunctions::Register();
    }
};

// Helper function to test binary div operations with different input/output types
template<typename TInput, typename TOutput, DataTypeId inputTypeId, DataTypeId outputTypeId>
void TestBinaryDivOperation(
        const std::string& functionName,
        const std::vector<TInput>& leftData,
        const std::vector<TInput>& rightData,
        const std::vector<std::optional<TOutput>>& expectedResults) {

    int32_t rowSize = static_cast<int32_t>(leftData.size());
    ASSERT_EQ(rightData.size(), static_cast<size_t>(rowSize));
    ASSERT_EQ(expectedResults.size(), static_cast<size_t>(rowSize));

    // Create left vector
    BaseVector* leftVec = VectorHelper::CreateFlatVector(nullptr, inputTypeId, rowSize);
    auto* leftVector = static_cast<Vector<TInput>*>(leftVec);
    for (int32_t i = 0; i < rowSize; ++i) {
        leftVector->SetValue(i, leftData[i]);
        leftVector->SetNotNull(i);
    }

    // Create right vector
    BaseVector* rightVec = VectorHelper::CreateFlatVector(nullptr, inputTypeId, rowSize);
    auto* rightVector = static_cast<Vector<TInput>*>(rightVec);
    for (int32_t i = 0; i < rowSize; ++i) {
        rightVector->SetValue(i, rightData[i]);
        rightVector->SetNotNull(i);
    }

    // Create function signature
    std::vector<DataTypeId> argTypes = {inputTypeId, inputTypeId};
    auto signature = std::make_shared<FunctionSignature>(functionName, argTypes, outputTypeId);

    // Find vector function
    auto vectorFunction = VectorFunction::Find(signature);
    ASSERT_NE(vectorFunction, nullptr) << "Function " << functionName << " not found for input type " << static_cast<int>(inputTypeId);

    // Create execution context
    ExecutionContext context(4096, nullptr);
    context.SetResultRowSize(rowSize);

    // Prepare arguments stack
    std::stack<BaseVector*> args;
    args.push(leftVec);
    args.push(rightVec);

    // Execute function
    BaseVector* result = nullptr;
    auto resultType = std::make_shared<DataType>(outputTypeId);
    vectorFunction->Apply(args, resultType, result, &context);

    // Verify results
    ASSERT_NE(result, nullptr);
    auto* resultVector = static_cast<Vector<TOutput>*>(result);
    ASSERT_NE(resultVector, nullptr);

    for (int32_t i = 0; i < rowSize; ++i) {
        if (expectedResults[i].has_value()) {
            // Expected non-NULL result
            EXPECT_FALSE(result->IsNull(i))
                << "Expected non-NULL result at index " << i
                << " for div(" << leftData[i] << ", " << rightData[i] << ")";
            if (!result->IsNull(i)) {
                TOutput actualResult = resultVector->GetValue(i);
                EXPECT_EQ(actualResult, expectedResults[i].value())
                    << "Value mismatch at index " << i
                    << " for div(" << leftData[i] << ", " << rightData[i] << ")"
                    << ", expected=" << expectedResults[i].value() << ", actual=" << actualResult;
            }
        } else {
            // Expected NULL result (division by zero)
            EXPECT_TRUE(result->IsNull(i))
                << "Expected NULL result at index " << i
                << " for div(" << leftData[i] << ", " << rightData[i] << ")";
        }
    }

    delete result;
}

// Test div function for LONG (int64_t)
TEST_F(DivFunctionTest, DivLong) {
    std::cout << "=== Testing div function for LONG type ===" << std::endl;

    std::vector<int64_t> leftData = {10, 20, 7, 0, 100, -100, 100, -100};
    std::vector<int64_t> rightData = {3, 4, 2, 5, 7, 7, -7, -7};
    std::vector<std::optional<int64_t>> expectedResults = {3, 5, 3, 0, 14, -14, -14, 14};

    TestBinaryDivOperation<int64_t, int64_t, OMNI_LONG, OMNI_LONG>("div", leftData, rightData, expectedResults);
}

// Test div function with division by zero returns NULL
TEST_F(DivFunctionTest, DivLongDivisionByZero) {
    std::cout << "=== Testing div function with division by zero ===" << std::endl;

    std::vector<int64_t> leftData = {1, 0, std::numeric_limits<int64_t>::min()};
    std::vector<int64_t> rightData = {0, 0, 0};
    std::vector<std::optional<int64_t>> expectedResults = {std::nullopt, std::nullopt, std::nullopt};

    TestBinaryDivOperation<int64_t, int64_t, OMNI_LONG, OMNI_LONG>("div", leftData, rightData, expectedResults);
}

// Test div function with Long.MIN_VALUE / -1 edge case
TEST_F(DivFunctionTest, DivLongMinValueEdgeCase) {
    std::cout << "=== Testing div function with Long.MIN_VALUE / -1 ===" << std::endl;

    // Long.MIN_VALUE / -1 should return Long.MIN_VALUE itself (overflow wraps around)
    std::vector<int64_t> leftData = {std::numeric_limits<int64_t>::min()};
    std::vector<int64_t> rightData = {-1};
    std::vector<std::optional<int64_t>> expectedResults = {std::numeric_limits<int64_t>::min()};

    TestBinaryDivOperation<int64_t, int64_t, OMNI_LONG, OMNI_LONG>("div", leftData, rightData, expectedResults);
}

// Test div function with boundary values
TEST_F(DivFunctionTest, DivLongBoundaryValues) {
    std::cout << "=== Testing div function with boundary values ===" << std::endl;

    std::vector<int64_t> leftData = {
        std::numeric_limits<int8_t>::max(),
        std::numeric_limits<int16_t>::min(),
        std::numeric_limits<int32_t>::max(),
        std::numeric_limits<int64_t>::min()
    };
    std::vector<int64_t> rightData = {
        std::numeric_limits<int8_t>::min(),
        std::numeric_limits<int16_t>::max(),
        std::numeric_limits<int32_t>::min(),
        std::numeric_limits<int64_t>::max()
    };
    // INT8_MAX / INT8_MIN = 127 / -128 = 0
    // INT16_MIN / INT16_MAX = -32768 / 32767 = -1
    // INT32_MAX / INT32_MIN = 2147483647 / -2147483648 = 0
    // INT64_MIN / INT64_MAX = -9223372036854775808 / 9223372036854775807 = -1
    std::vector<std::optional<int64_t>> expectedResults = {0, -1, 0, -1};

    TestBinaryDivOperation<int64_t, int64_t, OMNI_LONG, OMNI_LONG>("div", leftData, rightData, expectedResults);
}

// Test div function for normal division
TEST_F(DivFunctionTest, DivLongNormalCases) {
    std::cout << "=== Testing div function with normal cases ===" << std::endl;

    std::vector<int64_t> leftData = {2, 3, 10, -10, 15, -15};
    std::vector<int64_t> rightData = {3, 2, 3, 3, -4, -4};
    // 2/3=0, 3/2=1, 10/3=3, -10/3=-3, 15/-4=-3, -15/-4=3
    std::vector<std::optional<int64_t>> expectedResults = {0, 1, 3, -3, -3, 3};

    TestBinaryDivOperation<int64_t, int64_t, OMNI_LONG, OMNI_LONG>("div", leftData, rightData, expectedResults);
}

// Test div function for DECIMAL64
TEST_F(DivFunctionTest, DivDecimal64) {
    std::cout << "=== Testing div function for DECIMAL64 type ===" << std::endl;

    // For DECIMAL64 (internally stored as int64_t with scale)
    // div(10, 200) with scale consideration
    std::vector<int64_t> leftData = {10, 24, 12, 91, 100, -92, 100, -100};
    std::vector<int64_t> rightData = {200, 110, 110, 200, 200, 200, -190, -190};
    // These are integer division results
    std::vector<std::optional<int64_t>> expectedResults = {0, 0, 0, 0, 0, 0, 0, 0};

    TestBinaryDivOperation<int64_t, int64_t, OMNI_DECIMAL64, OMNI_LONG>("div", leftData, rightData, expectedResults);
}

// Test div function for DECIMAL64 with division by zero
TEST_F(DivFunctionTest, DivDecimal64DivisionByZero) {
    std::cout << "=== Testing div function for DECIMAL64 with division by zero ===" << std::endl;

    std::vector<int64_t> leftData = {20, 100};
    std::vector<int64_t> rightData = {0, 0};
    std::vector<std::optional<int64_t>> expectedResults = {std::nullopt, std::nullopt};

    TestBinaryDivOperation<int64_t, int64_t, OMNI_DECIMAL64, OMNI_LONG>("div", leftData, rightData, expectedResults);
}

// Test div function for DECIMAL128
TEST_F(DivFunctionTest, DivDecimal128) {
    std::cout << "=== Testing div function for DECIMAL128 type ===" << std::endl;

    int32_t rowSize = 4;
    
    // Create left vector using Vector<Decimal128> directly
    auto* leftVector = new Vector<Decimal128>(rowSize);
    leftVector->SetValue(0, Decimal128("100"));
    leftVector->SetNotNull(0);
    leftVector->SetValue(1, Decimal128("200"));
    leftVector->SetNotNull(1);
    leftVector->SetValue(2, Decimal128("-300"));
    leftVector->SetNotNull(2);
    leftVector->SetValue(3, Decimal128("1000000000000000000"));  // 10^18
    leftVector->SetNotNull(3);

    // Create right vector
    auto* rightVector = new Vector<Decimal128>(rowSize);
    rightVector->SetValue(0, Decimal128("30"));
    rightVector->SetNotNull(0);
    rightVector->SetValue(1, Decimal128("50"));
    rightVector->SetNotNull(1);
    rightVector->SetValue(2, Decimal128("70"));
    rightVector->SetNotNull(2);
    rightVector->SetValue(3, Decimal128("3"));
    rightVector->SetNotNull(3);

    // Create function signature
    std::vector<DataTypeId> argTypes = {OMNI_DECIMAL128, OMNI_DECIMAL128};
    auto signature = std::make_shared<FunctionSignature>("div", argTypes, OMNI_LONG);

    // Find vector function
    auto vectorFunction = VectorFunction::Find(signature);
    ASSERT_NE(vectorFunction, nullptr) << "Function div not found for DECIMAL128";

    // Create execution context
    ExecutionContext context(4096, nullptr);
    context.SetResultRowSize(rowSize);

    // Prepare arguments stack
    std::stack<BaseVector*> args;
    args.push(leftVector);
    args.push(rightVector);

    // Execute function
    BaseVector* result = nullptr;
    auto resultType = std::make_shared<DataType>(OMNI_LONG);
    vectorFunction->Apply(args, resultType, result, &context);

    // Verify results
    ASSERT_NE(result, nullptr);
    auto* resultVector = static_cast<Vector<int64_t>*>(result);
    ASSERT_NE(resultVector, nullptr);

    // 100/30=3, 200/50=4, -300/70=-4, 10^18/3=333333333333333333
    std::vector<int64_t> expectedResults = {3, 4, -4, 333333333333333333LL};
    for (int32_t i = 0; i < rowSize; ++i) {
        EXPECT_FALSE(result->IsNull(i));
        if (!result->IsNull(i)) {
            EXPECT_EQ(resultVector->GetValue(i), expectedResults[i])
                << "Value mismatch at index " << i;
        }
    }

    delete result;
}

// Test div function for DECIMAL128 with division by zero
TEST_F(DivFunctionTest, DivDecimal128DivisionByZero) {
    std::cout << "=== Testing div function for DECIMAL128 with division by zero ===" << std::endl;

    int32_t rowSize = 2;
    
    // Create left vector
    auto* leftVector = new Vector<Decimal128>(rowSize);
    leftVector->SetValue(0, Decimal128("100"));
    leftVector->SetNotNull(0);
    leftVector->SetValue(1, Decimal128("999999999999999999"));
    leftVector->SetNotNull(1);

    // Create right vector with zeros
    auto* rightVector = new Vector<Decimal128>(rowSize);
    rightVector->SetValue(0, Decimal128("0"));
    rightVector->SetNotNull(0);
    rightVector->SetValue(1, Decimal128("0"));
    rightVector->SetNotNull(1);

    // Create function signature
    std::vector<DataTypeId> argTypes = {OMNI_DECIMAL128, OMNI_DECIMAL128};
    auto signature = std::make_shared<FunctionSignature>("div", argTypes, OMNI_LONG);

    // Find vector function
    auto vectorFunction = VectorFunction::Find(signature);
    ASSERT_NE(vectorFunction, nullptr) << "Function div not found for DECIMAL128";

    // Create execution context
    ExecutionContext context(4096, nullptr);
    context.SetResultRowSize(rowSize);

    // Prepare arguments stack
    std::stack<BaseVector*> args;
    args.push(leftVector);
    args.push(rightVector);

    // Execute function
    BaseVector* result = nullptr;
    auto resultType = std::make_shared<DataType>(OMNI_LONG);
    vectorFunction->Apply(args, resultType, result, &context);

    // Verify results - division by zero should return NULL
    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(result->IsNull(0)) << "Expected NULL for division by zero at index 0";
    EXPECT_TRUE(result->IsNull(1)) << "Expected NULL for division by zero at index 1";

    delete result;
}

// Test div function with ExprEval path
TEST_F(DivFunctionTest, DivWithExprEval) {
    std::cout << "=== Testing div function with ExprEval ===" << std::endl;

    int rowSize = 4;
    auto returnType = std::make_shared<DataType>(OMNI_LONG);
    auto type = std::make_shared<DataType>(OMNI_LONG);
    std::vector<Expr*> args = {new FieldExpr(0, type), new FieldExpr(1, type)};
    auto funcExpr = new FuncExpr("div", args, returnType);

    int64_t col1[4] = {10, 20, 100, -100};
    int64_t col2[4] = {3, 4, 7, -7};

    std::vector vecOfTypes = {LongType(), LongType()};
    DataTypes inputTypes(vecOfTypes);
    VectorBatch* input = CreateVectorBatch(inputTypes, rowSize, col1, col2);

    std::cout << "=== div input ===" << std::endl;
    VectorHelper::PrintVecBatch(input);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.Visit(*funcExpr);
    auto result = e.GetResult();

    VectorBatch vectorBatch(rowSize);
    vectorBatch.Append(result);
    std::cout << "=== div Result ===" << std::endl;
    VectorHelper::PrintVecBatch(&vectorBatch);

    auto* resultVector = dynamic_cast<Vector<int64_t>*>(result);
    ASSERT_NE(resultVector, nullptr);

    // Expected: 10/3=3, 20/4=5, 100/7=14, -100/-7=14
    std::vector<int64_t> expectedResults = {3, 5, 14, 14};
    for (int32_t i = 0; i < rowSize; ++i) {
        EXPECT_EQ(resultVector->GetValue(i), expectedResults[i])
            << "Value mismatch at index " << i;
    }

    delete input;
    delete funcExpr;
    delete context;
}

// Test div function with NULL input values
TEST_F(DivFunctionTest, DivWithNullInput) {
    std::cout << "=== Testing div function with NULL input ===" << std::endl;

    int rowSize = 3;
    auto returnType = std::make_shared<DataType>(OMNI_LONG);
    auto type = std::make_shared<DataType>(OMNI_LONG);
    std::vector<Expr*> args = {new FieldExpr(0, type), new FieldExpr(1, type)};
    auto funcExpr = new FuncExpr("div", args, returnType);

    int64_t col1[3] = {10, 20, 30};
    int64_t col2[3] = {3, 5, 7};

    std::vector vecOfTypes = {LongType(), LongType()};
    DataTypes inputTypes(vecOfTypes);
    VectorBatch* input = CreateVectorBatch(inputTypes, rowSize, col1, col2);

    // Set first value of col1 to NULL
    input->Get(0)->SetNull(0);

    std::cout << "=== div with NULL input ===" << std::endl;
    VectorHelper::PrintVecBatch(input);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.Visit(*funcExpr);
    auto result = e.GetResult();

    VectorBatch vectorBatch(rowSize);
    vectorBatch.Append(result);
    std::cout << "=== div with NULL Result ===" << std::endl;
    VectorHelper::PrintVecBatch(&vectorBatch);

    // First row should be NULL
    EXPECT_TRUE(result->IsNull(0)) << "Result should be NULL when input is NULL";
    EXPECT_FALSE(result->IsNull(1)) << "Result should not be NULL for valid inputs";
    EXPECT_FALSE(result->IsNull(2)) << "Result should not be NULL for valid inputs";

    delete input;
    delete funcExpr;
    delete context;
}
