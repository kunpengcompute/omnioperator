/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
* Description: Unit tests for floor function
*/

#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <cmath>
#include <limits>

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

class FloorTest : public ::testing::Test {
protected:
    void SetUp() override {
        RegisterFunctions::Register();
    }
};

// Test floor function with double inputs
TEST_F(FloorTest, FloorDouble) {
    
    int32_t rowSize = 8;
    auto returnType = std::make_shared<DataType>(OMNI_LONG);
    auto inputType = std::make_shared<DataType>(OMNI_DOUBLE);
    std::vector<Expr*> args = {new FieldExpr(0, inputType)};
    auto funcExpr = new FuncExpr("floor", args, returnType);
    
    // Test values: positive decimals, negative decimals, exact integers, zero
    double col1[8] = {2.878, 1.5678, -1.5, -2.878, 0.0, 5.0, -5.0, 0.999};
    std::vector vecOfTypes = {DoubleType()};
    DataTypes inputTypes(vecOfTypes);
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1);
    
    auto context = new ExecutionContext();
    context->SetResultRowSize(rowSize);
    
    ExprEval e(input, context);
    e.Visit(*funcExpr);
    auto result = e.GetResult();
    
    auto *resultVector = dynamic_cast<Vector<int64_t> *>(result);
    ASSERT_NE(resultVector, nullptr);
    
    // Expected results: floor(2.878)=2, floor(1.5678)=1, floor(-1.5)=-2, floor(-2.878)=-3,
    // floor(0.0)=0, floor(5.0)=5, floor(-5.0)=-5, floor(0.999)=0
    std::vector<int64_t> expectedResults = {2, 1, -2, -3, 0, 5, -5, 0};
    
    for (int32_t i = 0; i < rowSize; ++i) {
        EXPECT_FALSE(result->IsNull(i)) << "Result should not be NULL at index " << i;
        int64_t actualResult = resultVector->GetValue(i);
        int64_t expectedResult = expectedResults[i];
        EXPECT_EQ(actualResult, expectedResult)
            << "Value mismatch at index " << i << " for floor(" << col1[i] << ")"
            << ", expected=" << expectedResult << ", actual=" << actualResult;
    }
    
    delete result;
    delete input;
    delete funcExpr;
    delete context;
}

// Test floor function with long inputs (should return same value)
TEST_F(FloorTest, FloorLong) {
    
    int32_t rowSize = 6;
    auto returnType = std::make_shared<DataType>(OMNI_LONG);
    auto inputType = std::make_shared<DataType>(OMNI_LONG);
    std::vector<Expr*> args = {new FieldExpr(0, inputType)};
    auto funcExpr = new FuncExpr("floor", args, returnType);
    
    // Test values: positive, negative, zero, max, min
    int64_t col1[6] = {100LL, -100LL, 0LL, 
                        std::numeric_limits<int64_t>::max(), 
                        std::numeric_limits<int64_t>::min(),
                        12345678901234LL};
    std::vector vecOfTypes = {LongType()};
    DataTypes inputTypes(vecOfTypes);
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1);
    
    auto context = new ExecutionContext();
    context->SetResultRowSize(rowSize);
    
    ExprEval e(input, context);
    e.Visit(*funcExpr);
    auto result = e.GetResult();
    
    auto *resultVector = dynamic_cast<Vector<int64_t> *>(result);
    ASSERT_NE(resultVector, nullptr);
    
    // For long input, floor returns the same value
    for (int32_t i = 0; i < rowSize; ++i) {
        EXPECT_FALSE(result->IsNull(i)) << "Result should not be NULL at index " << i;
        int64_t actualResult = resultVector->GetValue(i);
        int64_t expectedResult = col1[i];
        EXPECT_EQ(actualResult, expectedResult)
            << "Value mismatch at index " << i << " for floor(" << col1[i] << ")"
            << ", expected=" << expectedResult << ", actual=" << actualResult;
    }
    
    delete result;
    delete input;
    delete funcExpr;
    delete context;
}

// Test floor function with edge cases (infinity, NaN)
TEST_F(FloorTest, FloorEdgeCases) {
    
    constexpr double kInf = std::numeric_limits<double>::infinity();
    constexpr double kNan = std::numeric_limits<double>::quiet_NaN();
    constexpr int64_t kMax = std::numeric_limits<int64_t>::max();
    constexpr int64_t kMin = std::numeric_limits<int64_t>::min();
    
    int32_t rowSize = 4;
    auto returnType = std::make_shared<DataType>(OMNI_LONG);
    auto inputType = std::make_shared<DataType>(OMNI_DOUBLE);
    std::vector<Expr*> args = {new FieldExpr(0, inputType)};
    auto funcExpr = new FuncExpr("floor", args, returnType);
    
    // Test values: infinity, -infinity, NaN, very large number
    double col1[4] = {kInf, -kInf, kNan, 1e18};
    std::vector vecOfTypes = {DoubleType()};
    DataTypes inputTypes(vecOfTypes);
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1);
    
    auto context = new ExecutionContext();
    context->SetResultRowSize(rowSize);
    
    ExprEval e(input, context);
    e.Visit(*funcExpr);
    auto result = e.GetResult();
    
    auto *resultVector = dynamic_cast<Vector<int64_t> *>(result);
    ASSERT_NE(resultVector, nullptr);
    
    // Expected: floor(inf)=int64_max, floor(-inf)=int64_min, floor(NaN)=0, floor(1e18)=1e18
    EXPECT_EQ(resultVector->GetValue(0), kMax) << "floor(infinity) should be int64_max";
    EXPECT_EQ(resultVector->GetValue(1), kMin) << "floor(-infinity) should be int64_min";
    EXPECT_EQ(resultVector->GetValue(2), 0LL) << "floor(NaN) should be 0";
    EXPECT_EQ(resultVector->GetValue(3), static_cast<int64_t>(1e18)) << "floor(1e18) should be 1e18";
    
    delete result;
    delete input;
    delete funcExpr;
    delete context;
}

// Test floor function with NULL input
TEST_F(FloorTest, FloorWithNullInput) {
    
    int32_t rowSize = 4;
    auto returnType = std::make_shared<DataType>(OMNI_LONG);
    auto inputType = std::make_shared<DataType>(OMNI_DOUBLE);
    std::vector<Expr*> args = {new FieldExpr(0, inputType)};
    auto funcExpr = new FuncExpr("floor", args, returnType);
    
    double col1[4] = {2.5, 3.7, -1.2, 4.9};
    std::vector vecOfTypes = {DoubleType()};
    DataTypes inputTypes(vecOfTypes);
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1);
    
    // Set first and third values to NULL
    input->Get(0)->SetNull(0);
    input->Get(0)->SetNull(2);
    
    auto context = new ExecutionContext();
    context->SetResultRowSize(rowSize);
    
    ExprEval e(input, context);
    e.Visit(*funcExpr);
    auto result = e.GetResult();
    
    auto *resultVector = dynamic_cast<Vector<int64_t> *>(result);
    
    // First and third should be NULL
    EXPECT_TRUE(result->IsNull(0)) << "Result should be NULL when input is NULL at index 0";
    EXPECT_FALSE(result->IsNull(1)) << "Result should not be NULL at index 1";
    EXPECT_TRUE(result->IsNull(2)) << "Result should be NULL when input is NULL at index 2";
    EXPECT_FALSE(result->IsNull(3)) << "Result should not be NULL at index 3";
    
    // Check non-NULL values
    EXPECT_EQ(resultVector->GetValue(1), 3LL) << "floor(3.7) should be 3";
    EXPECT_EQ(resultVector->GetValue(3), 4LL) << "floor(4.9) should be 4";
    
    delete result;
    delete input;
    delete funcExpr;
    delete context;
}

// Test floor function using VectorFunction directly for double
TEST_F(FloorTest, FloorVectorFunctionDouble) {
    
    int32_t rowSize = 5;
    
    // Create input vector
    BaseVector* rawInput = VectorHelper::CreateFlatVector(OMNI_DOUBLE, rowSize);
    auto* inputVector = static_cast<Vector<double>*>(rawInput);
    std::vector<double> inputData = {2.5, -2.5, 0.0, 100.999, -100.001};
    for (int32_t i = 0; i < rowSize; ++i) {
        inputVector->SetValue(i, inputData[i]);
        inputVector->SetNotNull(i);
    }
    
    // Create function signature: floor(double) -> long
    std::vector<DataTypeId> argTypes = {OMNI_DOUBLE};
    auto signature = std::make_shared<FunctionSignature>("floor", argTypes, OMNI_LONG);
    auto vectorFunction = VectorFunction::Find(signature);
    ASSERT_NE(vectorFunction, nullptr) << "Function floor(double) not found";
    
    ExecutionContext context;
    context.SetResultRowSize(rowSize);
    
    std::stack<BaseVector*> args;
    args.push(rawInput);
    
    BaseVector* rawResult = nullptr;
    auto resultType = std::make_shared<DataType>(OMNI_LONG);
    vectorFunction->Apply(args, resultType, rawResult, &context);
    ASSERT_NE(rawResult, nullptr);
    
    auto* resultVector = static_cast<Vector<int64_t>*>(rawResult);
    ASSERT_NE(resultVector, nullptr);
    
    std::vector<int64_t> expectedResults = {2, -3, 0, 100, -101};
    
    for (int32_t i = 0; i < rowSize; ++i) {
        EXPECT_FALSE(rawResult->IsNull(i)) << "Result should not be NULL at index " << i;
        int64_t actual = resultVector->GetValue(i);
        int64_t expected = expectedResults[i];
        EXPECT_EQ(actual, expected)
            << "Value mismatch at index " << i << " for floor(" << inputData[i] << ")"
            << ", expected=" << expected << ", actual=" << actual;
    }
    
    // Note: rawInput is already freed by FlatVectorReader's destructor inside Apply(),
    // because GetIsField() is false for vectors created by VectorHelper::CreateFlatVector.
    // Do NOT delete rawInput here to avoid double-free (heap-use-after-free).
    delete rawResult;
}

// Test floor function using VectorFunction directly for long
TEST_F(FloorTest, FloorVectorFunctionLong) {
    
    int32_t rowSize = 4;
    
    // Create input vector
    BaseVector* rawInput = VectorHelper::CreateFlatVector(OMNI_LONG, rowSize);
    auto* inputVector = static_cast<Vector<int64_t>*>(rawInput);
    std::vector<int64_t> inputData = {100LL, -100LL, 0LL, 9223372036854775807LL};
    for (int32_t i = 0; i < rowSize; ++i) {
        inputVector->SetValue(i, inputData[i]);
        inputVector->SetNotNull(i);
    }
    
    // Create function signature: floor(long) -> long
    std::vector<DataTypeId> argTypes = {OMNI_LONG};
    auto signature = std::make_shared<FunctionSignature>("floor", argTypes, OMNI_LONG);
    auto vectorFunction = VectorFunction::Find(signature);
    ASSERT_NE(vectorFunction, nullptr) << "Function floor(long) not found";
    
    ExecutionContext context;
    context.SetResultRowSize(rowSize);
    
    std::stack<BaseVector*> args;
    args.push(rawInput);
    
    BaseVector* rawResult = nullptr;
    auto resultType = std::make_shared<DataType>(OMNI_LONG);
    vectorFunction->Apply(args, resultType, rawResult, &context);
    ASSERT_NE(rawResult, nullptr);
    
    auto* resultVector = static_cast<Vector<int64_t>*>(rawResult);
    ASSERT_NE(resultVector, nullptr);
    
    // For long input, floor returns the same value
    for (int32_t i = 0; i < rowSize; ++i) {
        EXPECT_FALSE(rawResult->IsNull(i)) << "Result should not be NULL at index " << i;
        int64_t actual = resultVector->GetValue(i);
        int64_t expected = inputData[i];
        EXPECT_EQ(actual, expected)
            << "Value mismatch at index " << i << " for floor(" << inputData[i] << ")"
            << ", expected=" << expected << ", actual=" << actual;
    }
    
    // Note: rawInput is already freed by FlatVectorReader's destructor inside Apply(),
    // because GetIsField() is false for vectors created by VectorHelper::CreateFlatVector.
    // Do NOT delete rawInput here to avoid double-free (heap-use-after-free).
    delete rawResult;
}

// Test floor function with negative decimals close to integer
TEST_F(FloorTest, FloorNegativeDecimalsCloseToInteger) {
    
    int32_t rowSize = 6;
    auto returnType = std::make_shared<DataType>(OMNI_LONG);
    auto inputType = std::make_shared<DataType>(OMNI_DOUBLE);
    std::vector<Expr*> args = {new FieldExpr(0, inputType)};
    auto funcExpr = new FuncExpr("floor", args, returnType);
    
    // Test values that are very close to integers
    double col1[6] = {-0.001, -0.999, -1.001, -1.999, 0.001, 0.999};
    std::vector vecOfTypes = {DoubleType()};
    DataTypes inputTypes(vecOfTypes);
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1);
    
    auto context = new ExecutionContext();
    context->SetResultRowSize(rowSize);
    
    ExprEval e(input, context);
    e.Visit(*funcExpr);
    auto result = e.GetResult();
    
    auto *resultVector = dynamic_cast<Vector<int64_t> *>(result);
    ASSERT_NE(resultVector, nullptr);
    
    // Expected: floor(-0.001)=-1, floor(-0.999)=-1, floor(-1.001)=-2, 
    //          floor(-1.999)=-2, floor(0.001)=0, floor(0.999)=0
    std::vector<int64_t> expectedResults = {-1, -1, -2, -2, 0, 0};
    
    for (int32_t i = 0; i < rowSize; ++i) {
        EXPECT_FALSE(result->IsNull(i)) << "Result should not be NULL at index " << i;
        int64_t actualResult = resultVector->GetValue(i);
        int64_t expectedResult = expectedResults[i];
        EXPECT_EQ(actualResult, expectedResult)
            << "Value mismatch at index " << i << " for floor(" << col1[i] << ")"
            << ", expected=" << expectedResult << ", actual=" << actualResult;
    }
    
    delete result;
    delete input;
    delete funcExpr;
    delete context;
}

// Test floor(double) -> double (Flink semantics)
TEST_F(FloorTest, FloorDoubleReturnDouble) {

    int32_t rowSize = 5;

    // Create input vector
    BaseVector* rawInput = VectorHelper::CreateFlatVector(OMNI_DOUBLE, rowSize);
    auto* inputVector = static_cast<Vector<double>*>(rawInput);
    std::vector<double> inputData = {2.5, -2.5, 0.0, 100.999, -100.001};
    for (int32_t i = 0; i < rowSize; ++i) {
        inputVector->SetValue(i, inputData[i]);
        inputVector->SetNotNull(i);
    }

    // Create function signature: floor(double) -> double (Flink semantics)
    std::vector<DataTypeId> argTypes = {OMNI_DOUBLE};
    auto signature = std::make_shared<FunctionSignature>("floor", argTypes, OMNI_DOUBLE);
    auto vectorFunction = VectorFunction::Find(signature);
    ASSERT_NE(vectorFunction, nullptr) << "Function floor(double)->double not found";

    ExecutionContext context;
    context.SetResultRowSize(rowSize);

    std::stack<BaseVector*> args;
    args.push(rawInput);

    BaseVector* rawResult = nullptr;
    auto resultType = std::make_shared<DataType>(OMNI_DOUBLE);
    vectorFunction->Apply(args, resultType, rawResult, &context);
    ASSERT_NE(rawResult, nullptr);

    auto* resultVector = static_cast<Vector<double>*>(rawResult);
    ASSERT_NE(resultVector, nullptr);

    std::vector<double> expectedResults = {2.0, -3.0, 0.0, 100.0, -101.0};

    for (int32_t i = 0; i < rowSize; ++i) {
        EXPECT_FALSE(rawResult->IsNull(i)) << "Result should not be NULL at index " << i;
        double actual = resultVector->GetValue(i);
        double expected = expectedResults[i];
        EXPECT_EQ(actual, expected)
            << "Value mismatch at index " << i << " for floor(" << inputData[i] << ")"
            << ", expected=" << expected << ", actual=" << actual;
    }

    delete rawResult;
}