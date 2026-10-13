/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Unit tests for array_min function
 */

#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include <vector>
#include <limits>
#include <cmath>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/ExprEval.h"
#include "expression/expressions.h"
#include "type/decimal_operations.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::mem;
using namespace omniruntime::op;
using namespace omniruntime::expressions;
using namespace omniruntime::TestUtil;

class ArrayMinTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Setup code if needed
    }

    void TearDown() override
    {
        // Cleanup code if needed
    }
};

/// Test array_min with integer arrays
TEST_F(ArrayMinTest, IntegerArrayMin)
{
    std::cout << "[ArrayMinTest] Testing integer array_min" << std::endl;
    
    int rowSize = 3;
    auto type = std::make_shared<DataType>(OMNI_INT);
    auto types = DataTypes({type});
    
    // Array data: [1,2,3], [4], [5]
    // Expected min: 1, 4, 5
    int32_t col[] = {1, 2, 3, 4, 5};
    std::vector<int32_t> offset = {0, 3, 4, 5};
    auto input = CreateArrayVectorBatch(types, offset, rowSize, 5, col);

    auto expr = FuncExpr("array_min", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY))
    }, type);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();
    
    auto resultVec = dynamic_cast<Vector<int32_t>*>(result);
    ASSERT_NE(resultVec, nullptr);
    
    // Verify results
    EXPECT_EQ(resultVec->GetValue(0), 1);  // min of [1,2,3]
    EXPECT_EQ(resultVec->GetValue(1), 4);  // min of [4]
    EXPECT_EQ(resultVec->GetValue(2), 5);  // min of [5]
    
    std::cout << "[ArrayMinTest] Integer array_min test passed" << std::endl;

    delete context;
    delete input;
    delete result;
}

/// Test array_min with long arrays
TEST_F(ArrayMinTest, LongArrayMin)
{
    std::cout << "[ArrayMinTest] Testing long array_min" << std::endl;
    
    int rowSize = 2;
    auto type = std::make_shared<DataType>(OMNI_LONG);
    auto types = DataTypes({type});
    
    // Array data: [100, 200, 300], [400, 500]
    // Expected min: 100, 400
    int64_t col[] = {100, 200, 300, 400, 500};
    std::vector<int32_t> offset = {0, 3, 5};
    auto input = CreateArrayVectorBatch(types, offset, rowSize, 5, col);

    auto expr = FuncExpr("array_min", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY))
    }, type);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();
    
    auto resultVec = dynamic_cast<Vector<int64_t>*>(result);
    ASSERT_NE(resultVec, nullptr);
    
    EXPECT_EQ(resultVec->GetValue(0), 100);
    EXPECT_EQ(resultVec->GetValue(1), 400);
    
    std::cout << "[ArrayMinTest] Long array_min test passed" << std::endl;

    delete context;
    delete input;
    delete result;
}

/// Test array_min with double arrays
TEST_F(ArrayMinTest, DoubleArrayMin)
{
    std::cout << "[ArrayMinTest] Testing double array_min" << std::endl;
    
    int rowSize = 2;
    auto type = std::make_shared<DataType>(OMNI_DOUBLE);
    auto types = DataTypes({type});
    
    // Array data: [1.1, 2.2, 3.3], [4.4, 5.5]
    // Expected min: 1.1, 4.4
    double col[] = {1.1, 2.2, 3.3, 4.4, 5.5};
    std::vector<int32_t> offset = {0, 3, 5};
    auto input = CreateArrayVectorBatch(types, offset, rowSize, 5, col);

    auto expr = FuncExpr("array_min", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY))
    }, type);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();
    
    auto resultVec = dynamic_cast<Vector<double>*>(result);
    ASSERT_NE(resultVec, nullptr);
    
    EXPECT_DOUBLE_EQ(resultVec->GetValue(0), 1.1);
    EXPECT_DOUBLE_EQ(resultVec->GetValue(1), 4.4);
    
    std::cout << "[ArrayMinTest] Double array_min test passed" << std::endl;

    delete context;
    delete input;
    delete result;
}

/// Test array_min with double arrays containing special values (NaN, Infinity)
TEST_F(ArrayMinTest, DoubleArrayMinWithSpecialValues)
{
    std::cout << "[ArrayMinTest] Testing double array_min with special values" << std::endl;
    
    int rowSize = 3;
    auto type = std::make_shared<DataType>(OMNI_DOUBLE);
    auto types = DataTypes({type});
    
    constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
    constexpr double kInf = std::numeric_limits<double>::infinity();
    constexpr double kNegInf = -std::numeric_limits<double>::infinity();
    
    // Array data: [kNaN, 1.0, 2.0], [kNegInf, 0.0, kInf], [kInf, 1.0]
    // Expected min: 1.0 (NaN is not min), kNegInf, 1.0
    double col[] = {kNaN, 1.0, 2.0, kNegInf, 0.0, kInf, kInf, 1.0};
    std::vector<int32_t> offset = {0, 3, 6, 8};
    auto input = CreateArrayVectorBatch(types, offset, rowSize, 8, col);

    auto expr = FuncExpr("array_min", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY))
    }, type);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();
    
    auto resultVec = dynamic_cast<Vector<double>*>(result);
    ASSERT_NE(resultVec, nullptr);
    
    // First result should be 1.0 (NaN is skipped for min)
    EXPECT_DOUBLE_EQ(resultVec->GetValue(0), 1.0);
    // Second result should be negative infinity
    EXPECT_EQ(resultVec->GetValue(1), kNegInf);
    // Third result should be 1.0
    EXPECT_DOUBLE_EQ(resultVec->GetValue(2), 1.0);
    
    std::cout << "[ArrayMinTest] Double array_min with special values test passed" << std::endl;

    delete context;
    delete input;
    delete result;
}

/// Test array_min with only NaN values
TEST_F(ArrayMinTest, DoubleArrayMinOnlyNaN)
{
    std::cout << "[ArrayMinTest] Testing double array_min with only NaN values" << std::endl;
    
    int rowSize = 2;
    auto type = std::make_shared<DataType>(OMNI_DOUBLE);
    auto types = DataTypes({type});
    
    constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
    
    // Array data: [NaN, NaN], [NaN]
    // Expected min: NaN, NaN (all values are NaN)
    double col[] = {kNaN, kNaN, kNaN};
    std::vector<int32_t> offset = {0, 2, 3};
    auto input = CreateArrayVectorBatch(types, offset, rowSize, 3, col);

    auto expr = FuncExpr("array_min", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY))
    }, type);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();
    
    auto resultVec = dynamic_cast<Vector<double>*>(result);
    ASSERT_NE(resultVec, nullptr);
    
    // When all values are NaN, result should be NaN
    EXPECT_TRUE(std::isnan(resultVec->GetValue(0)));
    EXPECT_TRUE(std::isnan(resultVec->GetValue(1)));
    
    std::cout << "[ArrayMinTest] Double array_min with only NaN values test passed" << std::endl;

    delete context;
    delete input;
    delete result;
}

/// Test array_min with boolean arrays
TEST_F(ArrayMinTest, BooleanArrayMin)
{
    std::cout << "[ArrayMinTest] Testing boolean array_min" << std::endl;
    
    int rowSize = 3;
    auto type = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto types = DataTypes({type});
    
    // Array data: [true, false], [false, false], [true, true]
    // Expected min: false, false, true
    bool col[] = {true, false, false, false, true, true};
    std::vector<int32_t> offset = {0, 2, 4, 6};
    auto input = CreateArrayVectorBatch(types, offset, rowSize, 6, col);

    auto expr = FuncExpr("array_min", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY))
    }, type);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();
    
    auto resultVec = dynamic_cast<Vector<bool>*>(result);
    ASSERT_NE(resultVec, nullptr);
    
    EXPECT_EQ(resultVec->GetValue(0), false);  // min of [true, false]
    EXPECT_EQ(resultVec->GetValue(1), false);  // min of [false, false]
    EXPECT_EQ(resultVec->GetValue(2), true);   // min of [true, true]
    
    std::cout << "[ArrayMinTest] Boolean array_min test passed" << std::endl;

    delete context;
    delete input;
    delete result;
}

/// Test array_min with negative numbers
TEST_F(ArrayMinTest, NegativeNumbersArrayMin)
{
    std::cout << "[ArrayMinTest] Testing array_min with negative numbers" << std::endl;
    
    int rowSize = 2;
    auto type = std::make_shared<DataType>(OMNI_INT);
    auto types = DataTypes({type});
    
    // Array data: [-5, -3, -1], [-100, -50]
    // Expected min: -5, -100
    int32_t col[] = {-5, -3, -1, -100, -50};
    std::vector<int32_t> offset = {0, 3, 5};
    auto input = CreateArrayVectorBatch(types, offset, rowSize, 5, col);

    auto expr = FuncExpr("array_min", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY))
    }, type);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();
    
    auto resultVec = dynamic_cast<Vector<int32_t>*>(result);
    ASSERT_NE(resultVec, nullptr);
    
    EXPECT_EQ(resultVec->GetValue(0), -5);
    EXPECT_EQ(resultVec->GetValue(1), -100);
    
    std::cout << "[ArrayMinTest] Negative numbers array_min test passed" << std::endl;

    delete context;
    delete input;
    delete result;
}

/// Test array_min with boundary values
TEST_F(ArrayMinTest, BoundaryValuesArrayMin)
{
    std::cout << "[ArrayMinTest] Testing array_min with boundary values" << std::endl;
    
    int rowSize = 2;
    auto type = std::make_shared<DataType>(OMNI_INT);
    auto types = DataTypes({type});
    
    constexpr int32_t kMin = std::numeric_limits<int32_t>::min();
    constexpr int32_t kMax = std::numeric_limits<int32_t>::max();
    
    // Array data: [kMin, 0, kMax], [kMax, kMax]
    // Expected min: kMin, kMax
    int32_t col[] = {kMin, 0, kMax, kMax, kMax};
    std::vector<int32_t> offset = {0, 3, 5};
    auto input = CreateArrayVectorBatch(types, offset, rowSize, 5, col);

    auto expr = FuncExpr("array_min", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY))
    }, type);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();
    
    auto resultVec = dynamic_cast<Vector<int32_t>*>(result);
    ASSERT_NE(resultVec, nullptr);
    
    EXPECT_EQ(resultVec->GetValue(0), kMin);
    EXPECT_EQ(resultVec->GetValue(1), kMax);
    
    std::cout << "[ArrayMinTest] Boundary values array_min test passed" << std::endl;

    delete context;
    delete input;
    delete result;
}

/// Test array_min with single element arrays
TEST_F(ArrayMinTest, SingleElementArrayMin)
{
    std::cout << "[ArrayMinTest] Testing array_min with single element arrays" << std::endl;
    
    int rowSize = 3;
    auto type = std::make_shared<DataType>(OMNI_INT);
    auto types = DataTypes({type});
    
    // Array data: [42], [100], [-5]
    // Expected min: 42, 100, -5
    int32_t col[] = {42, 100, -5};
    std::vector<int32_t> offset = {0, 1, 2, 3};
    auto input = CreateArrayVectorBatch(types, offset, rowSize, 3, col);

    auto expr = FuncExpr("array_min", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY))
    }, type);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();
    
    auto resultVec = dynamic_cast<Vector<int32_t>*>(result);
    ASSERT_NE(resultVec, nullptr);
    
    EXPECT_EQ(resultVec->GetValue(0), 42);
    EXPECT_EQ(resultVec->GetValue(1), 100);
    EXPECT_EQ(resultVec->GetValue(2), -5);
    
    std::cout << "[ArrayMinTest] Single element array_min test passed" << std::endl;

    delete context;
    delete input;
    delete result;
}

/// Test array_min with float arrays
TEST_F(ArrayMinTest, FloatArrayMin)
{
    std::cout << "[ArrayMinTest] Testing float array_min" << std::endl;
    
    int rowSize = 2;
    auto type = std::make_shared<DataType>(OMNI_FLOAT);
    auto types = DataTypes({type});
    
    // Array data: [1.5f, 2.5f, 3.5f], [0.1f, 0.2f]
    // Expected min: 1.5f, 0.1f
    float col[] = {1.5f, 2.5f, 3.5f, 0.1f, 0.2f};
    std::vector<int32_t> offset = {0, 3, 5};
    auto input = CreateArrayVectorBatch(types, offset, rowSize, 5, col);

    auto expr = FuncExpr("array_min", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY))
    }, type);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();
    
    auto resultVec = dynamic_cast<Vector<float>*>(result);
    ASSERT_NE(resultVec, nullptr);
    
    EXPECT_FLOAT_EQ(resultVec->GetValue(0), 1.5f);
    EXPECT_FLOAT_EQ(resultVec->GetValue(1), 0.1f);
    
    std::cout << "[ArrayMinTest] Float array_min test passed" << std::endl;

    delete context;
    delete input;
    delete result;
}

/// Test array_min with float arrays containing special values
TEST_F(ArrayMinTest, FloatArrayMinWithSpecialValues)
{
    std::cout << "[ArrayMinTest] Testing float array_min with special values" << std::endl;
    
    int rowSize = 3;
    auto type = std::make_shared<DataType>(OMNI_FLOAT);
    auto types = DataTypes({type});
    
    constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();
    constexpr float kInf = std::numeric_limits<float>::infinity();
    constexpr float kNegInf = -std::numeric_limits<float>::infinity();
    
    // Array data: [kNaN, 1.0f, 2.0f], [kNegInf, 0.0f], [kInf, kNaN]
    // Expected min: 1.0f (NaN skipped), kNegInf, kInf (NaN skipped)
    float col[] = {kNaN, 1.0f, 2.0f, kNegInf, 0.0f, kInf, kNaN};
    std::vector<int32_t> offset = {0, 3, 5, 7};
    auto input = CreateArrayVectorBatch(types, offset, rowSize, 7, col);

    auto expr = FuncExpr("array_min", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY))
    }, type);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();
    
    auto resultVec = dynamic_cast<Vector<float>*>(result);
    ASSERT_NE(resultVec, nullptr);
    
    EXPECT_FLOAT_EQ(resultVec->GetValue(0), 1.0f);
    EXPECT_EQ(resultVec->GetValue(1), kNegInf);
    EXPECT_EQ(resultVec->GetValue(2), kInf);
    
    std::cout << "[ArrayMinTest] Float array_min with special values test passed" << std::endl;

    delete context;
    delete input;
    delete result;
}

/// Test array_min with byte (int8) arrays
TEST_F(ArrayMinTest, ByteArrayMin)
{
    std::cout << "[ArrayMinTest] Testing byte array_min" << std::endl;
    
    int rowSize = 2;
    auto type = std::make_shared<DataType>(OMNI_BYTE);
    auto types = DataTypes({type});
    
    // Array data: [10, 20, 30], [5, 15]
    // Expected min: 10, 5
    int8_t col[] = {10, 20, 30, 5, 15};
    std::vector<int32_t> offset = {0, 3, 5};
    auto input = CreateArrayVectorBatch(types, offset, rowSize, 5, col);

    auto expr = FuncExpr("array_min", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY))
    }, type);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();
    
    auto resultVec = dynamic_cast<Vector<int8_t>*>(result);
    ASSERT_NE(resultVec, nullptr);
    
    EXPECT_EQ(resultVec->GetValue(0), 10);
    EXPECT_EQ(resultVec->GetValue(1), 5);
    
    std::cout << "[ArrayMinTest] Byte array_min test passed" << std::endl;

    delete context;
    delete input;
    delete result;
}

/// Test array_min with byte boundary values
TEST_F(ArrayMinTest, ByteBoundaryArrayMin)
{
    std::cout << "[ArrayMinTest] Testing byte array_min with boundary values" << std::endl;
    
    int rowSize = 2;
    auto type = std::make_shared<DataType>(OMNI_BYTE);
    auto types = DataTypes({type});
    
    constexpr int8_t kMin = std::numeric_limits<int8_t>::min();  // -128
    constexpr int8_t kMax = std::numeric_limits<int8_t>::max();  // 127
    
    // Array data: [kMin, 0, kMax], [kMax, 0]
    // Expected min: kMin, 0
    int8_t col[] = {kMin, 0, kMax, kMax, 0};
    std::vector<int32_t> offset = {0, 3, 5};
    auto input = CreateArrayVectorBatch(types, offset, rowSize, 5, col);

    auto expr = FuncExpr("array_min", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY))
    }, type);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();
    
    auto resultVec = dynamic_cast<Vector<int8_t>*>(result);
    ASSERT_NE(resultVec, nullptr);
    
    EXPECT_EQ(resultVec->GetValue(0), kMin);
    EXPECT_EQ(resultVec->GetValue(1), 0);
    
    std::cout << "[ArrayMinTest] Byte boundary array_min test passed" << std::endl;

    delete context;
    delete input;
    delete result;
}

/// Test array_min with short (int16) arrays
TEST_F(ArrayMinTest, ShortArrayMin)
{
    std::cout << "[ArrayMinTest] Testing short array_min" << std::endl;
    
    int rowSize = 2;
    auto type = std::make_shared<DataType>(OMNI_SHORT);
    auto types = DataTypes({type});
    
    // Array data: [1000, 2000, 3000], [500, 1500]
    // Expected min: 1000, 500
    int16_t col[] = {1000, 2000, 3000, 500, 1500};
    std::vector<int32_t> offset = {0, 3, 5};
    auto input = CreateArrayVectorBatch(types, offset, rowSize, 5, col);

    auto expr = FuncExpr("array_min", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY))
    }, type);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();
    
    auto resultVec = dynamic_cast<Vector<int16_t>*>(result);
    ASSERT_NE(resultVec, nullptr);
    
    EXPECT_EQ(resultVec->GetValue(0), 1000);
    EXPECT_EQ(resultVec->GetValue(1), 500);
    
    std::cout << "[ArrayMinTest] Short array_min test passed" << std::endl;

    delete context;
    delete input;
    delete result;
}

/// Test array_min with short boundary values
TEST_F(ArrayMinTest, ShortBoundaryArrayMin)
{
    std::cout << "[ArrayMinTest] Testing short array_min with boundary values" << std::endl;
    
    int rowSize = 2;
    auto type = std::make_shared<DataType>(OMNI_SHORT);
    auto types = DataTypes({type});
    
    constexpr int16_t kMin = std::numeric_limits<int16_t>::min();  // -32768
    constexpr int16_t kMax = std::numeric_limits<int16_t>::max();  // 32767
    
    // Array data: [kMin, 0, kMax], [kMax, 100]
    // Expected min: kMin, 100
    int16_t col[] = {kMin, 0, kMax, kMax, 100};
    std::vector<int32_t> offset = {0, 3, 5};
    auto input = CreateArrayVectorBatch(types, offset, rowSize, 5, col);

    auto expr = FuncExpr("array_min", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY))
    }, type);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();
    
    auto resultVec = dynamic_cast<Vector<int16_t>*>(result);
    ASSERT_NE(resultVec, nullptr);
    
    EXPECT_EQ(resultVec->GetValue(0), kMin);
    EXPECT_EQ(resultVec->GetValue(1), 100);
    
    std::cout << "[ArrayMinTest] Short boundary array_min test passed" << std::endl;

    delete context;
    delete input;
    delete result;
}

/// Test array_min with long boundary values
TEST_F(ArrayMinTest, LongBoundaryArrayMin)
{
    std::cout << "[ArrayMinTest] Testing long array_min with boundary values" << std::endl;
    
    int rowSize = 2;
    auto type = std::make_shared<DataType>(OMNI_LONG);
    auto types = DataTypes({type});
    
    constexpr int64_t kMin = std::numeric_limits<int64_t>::min();
    constexpr int64_t kMax = std::numeric_limits<int64_t>::max();
    
    // Array data: [kMin, 0, kMax], [kMax, 0]
    // Expected min: kMin, 0
    int64_t col[] = {kMin, 0, kMax, kMax, 0};
    std::vector<int32_t> offset = {0, 3, 5};
    auto input = CreateArrayVectorBatch(types, offset, rowSize, 5, col);

    auto expr = FuncExpr("array_min", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY))
    }, type);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();
    
    auto resultVec = dynamic_cast<Vector<int64_t>*>(result);
    ASSERT_NE(resultVec, nullptr);
    
    EXPECT_EQ(resultVec->GetValue(0), kMin);
    EXPECT_EQ(resultVec->GetValue(1), 0);
    
    std::cout << "[ArrayMinTest] Long boundary array_min test passed" << std::endl;

    delete context;
    delete input;
    delete result;
}

/// Test array_min with mixed positive and negative numbers
TEST_F(ArrayMinTest, MixedPositiveNegativeArrayMin)
{
    std::cout << "[ArrayMinTest] Testing array_min with mixed positive and negative numbers" << std::endl;
    
    int rowSize = 3;
    auto type = std::make_shared<DataType>(OMNI_INT);
    auto types = DataTypes({type});
    
    // Array data: [-10, 5, 0], [100, -200, 50], [-1, -2, -3]
    // Expected min: -10, -200, -3
    int32_t col[] = {-10, 5, 0, 100, -200, 50, -1, -2, -3};
    std::vector<int32_t> offset = {0, 3, 6, 9};
    auto input = CreateArrayVectorBatch(types, offset, rowSize, 9, col);

    auto expr = FuncExpr("array_min", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY))
    }, type);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();
    
    auto resultVec = dynamic_cast<Vector<int32_t>*>(result);
    ASSERT_NE(resultVec, nullptr);
    
    EXPECT_EQ(resultVec->GetValue(0), -10);
    EXPECT_EQ(resultVec->GetValue(1), -200);
    EXPECT_EQ(resultVec->GetValue(2), -3);
    
    std::cout << "[ArrayMinTest] Mixed positive and negative array_min test passed" << std::endl;

    delete context;
    delete input;
    delete result;
}

/// Test array_min with double precision boundary values
TEST_F(ArrayMinTest, DoubleBoundaryArrayMin)
{
    std::cout << "[ArrayMinTest] Testing double array_min with boundary values" << std::endl;
    
    int rowSize = 2;
    auto type = std::make_shared<DataType>(OMNI_DOUBLE);
    auto types = DataTypes({type});
    
    constexpr double kMin = std::numeric_limits<double>::lowest();
    constexpr double kMax = std::numeric_limits<double>::max();
    constexpr double kSmallest = std::numeric_limits<double>::min();  // smallest positive
    
    // Array data: [kMin, 0.0, kMax], [kSmallest, -kSmallest]
    // Expected min: kMin, -kSmallest
    double col[] = {kMin, 0.0, kMax, kSmallest, -kSmallest};
    std::vector<int32_t> offset = {0, 3, 5};
    auto input = CreateArrayVectorBatch(types, offset, rowSize, 5, col);

    auto expr = FuncExpr("array_min", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY))
    }, type);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();
    
    auto resultVec = dynamic_cast<Vector<double>*>(result);
    ASSERT_NE(resultVec, nullptr);
    
    EXPECT_EQ(resultVec->GetValue(0), kMin);
    EXPECT_EQ(resultVec->GetValue(1), -kSmallest);
    
    std::cout << "[ArrayMinTest] Double boundary array_min test passed" << std::endl;

    delete context;
    delete input;
    delete result;
}

/// Test array_min with large arrays
TEST_F(ArrayMinTest, LargeArrayMin)
{
    std::cout << "[ArrayMinTest] Testing array_min with large arrays" << std::endl;
    
    int rowSize = 2;
    auto type = std::make_shared<DataType>(OMNI_INT);
    auto types = DataTypes({type});
    
    // Create arrays with 100 and 50 elements
    const int size1 = 100;
    const int size2 = 50;
    int32_t col[size1 + size2];
    
    // First array: 100 to 1 (descending), min should be 1
    for (int i = 0; i < size1; ++i) {
        col[i] = size1 - i;
    }
    // Second array: 1 to 50 (ascending), min should be 1
    for (int i = 0; i < size2; ++i) {
        col[size1 + i] = i + 1;
    }
    
    std::vector<int32_t> offset = {0, size1, size1 + size2};
    auto input = CreateArrayVectorBatch(types, offset, rowSize, size1 + size2, col);

    auto expr = FuncExpr("array_min", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY))
    }, type);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();
    
    auto resultVec = dynamic_cast<Vector<int32_t>*>(result);
    ASSERT_NE(resultVec, nullptr);
    
    EXPECT_EQ(resultVec->GetValue(0), 1);  // min of [100, 99, ..., 1]
    EXPECT_EQ(resultVec->GetValue(1), 1);  // min of [1, 2, ..., 50]
    
    std::cout << "[ArrayMinTest] Large array_min test passed" << std::endl;

    delete context;
    delete input;
    delete result;
}

/// Test array_min with identical elements
TEST_F(ArrayMinTest, IdenticalElementsArrayMin)
{
    std::cout << "[ArrayMinTest] Testing array_min with identical elements" << std::endl;
    
    int rowSize = 2;
    auto type = std::make_shared<DataType>(OMNI_INT);
    auto types = DataTypes({type});
    
    // Array data: [5, 5, 5], [0, 0]
    // Expected min: 5, 0
    int32_t col[] = {5, 5, 5, 0, 0};
    std::vector<int32_t> offset = {0, 3, 5};
    auto input = CreateArrayVectorBatch(types, offset, rowSize, 5, col);

    auto expr = FuncExpr("array_min", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY))
    }, type);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();
    
    auto resultVec = dynamic_cast<Vector<int32_t>*>(result);
    ASSERT_NE(resultVec, nullptr);
    
    EXPECT_EQ(resultVec->GetValue(0), 5);
    EXPECT_EQ(resultVec->GetValue(1), 0);
    
    std::cout << "[ArrayMinTest] Identical elements array_min test passed" << std::endl;

    delete context;
    delete input;
    delete result;
}
