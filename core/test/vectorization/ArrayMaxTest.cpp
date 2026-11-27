/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Unit tests for array_max and array_min functions
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

class ArrayMaxTest : public ::testing::Test {
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

/// Test array_max with integer arrays
TEST_F(ArrayMaxTest, IntegerArrayMax)
{
    std::cout << "[ArrayMaxTest] Testing integer array_max" << std::endl;
    
    int rowSize = 3;
    auto type = std::make_shared<DataType>(OMNI_INT);
    auto types = DataTypes({type});
    
    // Array data: [1,2,3], [4], [5]
    // Expected max: 3, 4, 5
    int32_t col[] = {1, 2, 3, 4, 5};
    std::vector<int32_t> offset = {0, 3, 4, 5};
    auto input = CreateArrayVectorBatch(types, offset, rowSize, 5, col);

    auto expr = FuncExpr("array_max", {
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
    EXPECT_EQ(resultVec->GetValue(0), 3);  // max of [1,2,3]
    EXPECT_EQ(resultVec->GetValue(1), 4);  // max of [4]
    EXPECT_EQ(resultVec->GetValue(2), 5);  // max of [5]
    
    std::cout << "[ArrayMaxTest] Integer array_max test passed" << std::endl;

    delete context;
    delete input;
    delete result;
}

/// Test array_min with integer arrays
TEST_F(ArrayMaxTest, IntegerArrayMin)
{
    std::cout << "[ArrayMaxTest] Testing integer array_min" << std::endl;
    
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
    
    std::cout << "[ArrayMaxTest] Integer array_min test passed" << std::endl;

    delete context;
    delete input;
    delete result;
}

/// Test array_max with long arrays
TEST_F(ArrayMaxTest, LongArrayMax)
{
    std::cout << "[ArrayMaxTest] Testing long array_max" << std::endl;
    
    int rowSize = 2;
    auto type = std::make_shared<DataType>(OMNI_LONG);
    auto types = DataTypes({type});
    
    // Array data: [100, 200, 300], [400, 500]
    // Expected max: 300, 500
    int64_t col[] = {100, 200, 300, 400, 500};
    std::vector<int32_t> offset = {0, 3, 5};
    auto input = CreateArrayVectorBatch(types, offset, rowSize, 5, col);

    auto expr = FuncExpr("array_max", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY))
    }, type);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();
    
    auto resultVec = dynamic_cast<Vector<int64_t>*>(result);
    ASSERT_NE(resultVec, nullptr);
    
    EXPECT_EQ(resultVec->GetValue(0), 300);
    EXPECT_EQ(resultVec->GetValue(1), 500);
    
    std::cout << "[ArrayMaxTest] Long array_max test passed" << std::endl;

    delete context;
    delete input;
    delete result;
}

/// Test array_max with double arrays
TEST_F(ArrayMaxTest, DoubleArrayMax)
{
    std::cout << "[ArrayMaxTest] Testing double array_max" << std::endl;
    
    int rowSize = 2;
    auto type = std::make_shared<DataType>(OMNI_DOUBLE);
    auto types = DataTypes({type});
    
    // Array data: [1.1, 2.2, 3.3], [4.4, 5.5]
    // Expected max: 3.3, 5.5
    double col[] = {1.1, 2.2, 3.3, 4.4, 5.5};
    std::vector<int32_t> offset = {0, 3, 5};
    auto input = CreateArrayVectorBatch(types, offset, rowSize, 5, col);

    auto expr = FuncExpr("array_max", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY))
    }, type);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();
    
    auto resultVec = dynamic_cast<Vector<double>*>(result);
    ASSERT_NE(resultVec, nullptr);
    
    EXPECT_DOUBLE_EQ(resultVec->GetValue(0), 3.3);
    EXPECT_DOUBLE_EQ(resultVec->GetValue(1), 5.5);
    
    std::cout << "[ArrayMaxTest] Double array_max test passed" << std::endl;

    delete context;
    delete input;
    delete result;
}

/// Test array_max with double arrays containing special values
TEST_F(ArrayMaxTest, DoubleArrayMaxWithSpecialValues)
{
    std::cout << "[ArrayMaxTest] Testing double array_max with special values" << std::endl;
    
    int rowSize = 3;
    auto type = std::make_shared<DataType>(OMNI_DOUBLE);
    auto types = DataTypes({type});
    
    constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
    constexpr double kInf = std::numeric_limits<double>::infinity();
    constexpr double kNegInf = -std::numeric_limits<double>::infinity();
    
    // Array data: [1.0, NaN, 2.0], [kNegInf, 0.0, kInf], [kNegInf, -1.0]
    // Expected max: NaN (NaN is max), kInf, -1.0
    double col[] = {1.0, kNaN, 2.0, kNegInf, 0.0, kInf, kNegInf, -1.0};
    std::vector<int32_t> offset = {0, 3, 6, 8};
    auto input = CreateArrayVectorBatch(types, offset, rowSize, 8, col);

    auto expr = FuncExpr("array_max", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY))
    }, type);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();
    
    auto resultVec = dynamic_cast<Vector<double>*>(result);
    ASSERT_NE(resultVec, nullptr);
    
    // First result should be NaN (NaN is considered max for floating point)
    EXPECT_TRUE(std::isnan(resultVec->GetValue(0)));
    // Second result should be infinity
    EXPECT_EQ(resultVec->GetValue(1), kInf);
    // Third result should be -1.0
    EXPECT_DOUBLE_EQ(resultVec->GetValue(2), -1.0);
    
    std::cout << "[ArrayMaxTest] Double array_max with special values test passed" << std::endl;

    delete context;
    delete input;
    delete result;
}

/// Test array_min with double arrays containing special values
TEST_F(ArrayMaxTest, DoubleArrayMinWithSpecialValues)
{
    std::cout << "[ArrayMaxTest] Testing double array_min with special values" << std::endl;
    
    int rowSize = 2;
    auto type = std::make_shared<DataType>(OMNI_DOUBLE);
    auto types = DataTypes({type});
    
    constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
    constexpr double kInf = std::numeric_limits<double>::infinity();
    constexpr double kNegInf = -std::numeric_limits<double>::infinity();
    
    // Array data: [kNaN, 1.0, 2.0], [kNegInf, 0.0, kInf]
    // Expected min: 1.0 (NaN is not min), kNegInf
    double col[] = {kNaN, 1.0, 2.0, kNegInf, 0.0, kInf};
    std::vector<int32_t> offset = {0, 3, 6};
    auto input = CreateArrayVectorBatch(types, offset, rowSize, 6, col);

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
    
    std::cout << "[ArrayMaxTest] Double array_min with special values test passed" << std::endl;

    delete context;
    delete input;
    delete result;
}

/// Test array_max with boolean arrays
TEST_F(ArrayMaxTest, BooleanArrayMax)
{
    std::cout << "[ArrayMaxTest] Testing boolean array_max" << std::endl;
    
    int rowSize = 3;
    auto type = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto types = DataTypes({type});
    
    // Array data: [true, false], [false, false], [true, true]
    // Expected max: true, false, true
    bool col[] = {true, false, false, false, true, true};
    std::vector<int32_t> offset = {0, 2, 4, 6};
    auto input = CreateArrayVectorBatch(types, offset, rowSize, 6, col);

    auto expr = FuncExpr("array_max", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY))
    }, type);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();
    
    auto resultVec = dynamic_cast<Vector<bool>*>(result);
    ASSERT_NE(resultVec, nullptr);
    
    EXPECT_EQ(resultVec->GetValue(0), true);
    EXPECT_EQ(resultVec->GetValue(1), false);
    EXPECT_EQ(resultVec->GetValue(2), true);
    
    std::cout << "[ArrayMaxTest] Boolean array_max test passed" << std::endl;

    delete context;
    delete input;
    delete result;
}

/// Test array_max with negative numbers
TEST_F(ArrayMaxTest, NegativeNumbersArrayMax)
{
    std::cout << "[ArrayMaxTest] Testing array_max with negative numbers" << std::endl;
    
    int rowSize = 2;
    auto type = std::make_shared<DataType>(OMNI_INT);
    auto types = DataTypes({type});
    
    // Array data: [-5, -3, -1], [-100, -50]
    // Expected max: -1, -50
    int32_t col[] = {-5, -3, -1, -100, -50};
    std::vector<int32_t> offset = {0, 3, 5};
    auto input = CreateArrayVectorBatch(types, offset, rowSize, 5, col);

    auto expr = FuncExpr("array_max", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY))
    }, type);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();
    
    auto resultVec = dynamic_cast<Vector<int32_t>*>(result);
    ASSERT_NE(resultVec, nullptr);
    
    EXPECT_EQ(resultVec->GetValue(0), -1);
    EXPECT_EQ(resultVec->GetValue(1), -50);
    
    std::cout << "[ArrayMaxTest] Negative numbers array_max test passed" << std::endl;

    delete context;
    delete input;
    delete result;
}

/// Test array_max with boundary values
TEST_F(ArrayMaxTest, BoundaryValuesArrayMax)
{
    std::cout << "[ArrayMaxTest] Testing array_max with boundary values" << std::endl;
    
    int rowSize = 2;
    auto type = std::make_shared<DataType>(OMNI_INT);
    auto types = DataTypes({type});
    
    constexpr int32_t kMin = std::numeric_limits<int32_t>::min();
    constexpr int32_t kMax = std::numeric_limits<int32_t>::max();
    
    // Array data: [kMin, 0, kMax], [kMin, kMin]
    // Expected max: kMax, kMin
    int32_t col[] = {kMin, 0, kMax, kMin, kMin};
    std::vector<int32_t> offset = {0, 3, 5};
    auto input = CreateArrayVectorBatch(types, offset, rowSize, 5, col);

    auto expr = FuncExpr("array_max", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY))
    }, type);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();
    
    auto resultVec = dynamic_cast<Vector<int32_t>*>(result);
    ASSERT_NE(resultVec, nullptr);
    
    EXPECT_EQ(resultVec->GetValue(0), kMax);
    EXPECT_EQ(resultVec->GetValue(1), kMin);
    
    std::cout << "[ArrayMaxTest] Boundary values array_max test passed" << std::endl;

    delete context;
    delete input;
    delete result;
}

/// Test array_max with single element arrays
TEST_F(ArrayMaxTest, SingleElementArrayMax)
{
    std::cout << "[ArrayMaxTest] Testing array_max with single element arrays" << std::endl;
    
    int rowSize = 3;
    auto type = std::make_shared<DataType>(OMNI_INT);
    auto types = DataTypes({type});
    
    // Array data: [42], [100], [-5]
    // Expected max: 42, 100, -5
    int32_t col[] = {42, 100, -5};
    std::vector<int32_t> offset = {0, 1, 2, 3};
    auto input = CreateArrayVectorBatch(types, offset, rowSize, 3, col);

    auto expr = FuncExpr("array_max", {
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
    
    std::cout << "[ArrayMaxTest] Single element array_max test passed" << std::endl;

    delete context;
    delete input;
    delete result;
}

/// Test array_max with float arrays
TEST_F(ArrayMaxTest, FloatArrayMax)
{
    std::cout << "[ArrayMaxTest] Testing float array_max" << std::endl;
    
    int rowSize = 2;
    auto type = std::make_shared<DataType>(OMNI_FLOAT);
    auto types = DataTypes({type});
    
    // Array data: [1.5f, 2.5f, 3.5f], [0.1f, 0.2f]
    // Expected max: 3.5f, 0.2f
    float col[] = {1.5f, 2.5f, 3.5f, 0.1f, 0.2f};
    std::vector<int32_t> offset = {0, 3, 5};
    auto input = CreateArrayVectorBatch(types, offset, rowSize, 5, col);

    auto expr = FuncExpr("array_max", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY))
    }, type);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();
    
    auto resultVec = dynamic_cast<Vector<float>*>(result);
    ASSERT_NE(resultVec, nullptr);
    
    EXPECT_FLOAT_EQ(resultVec->GetValue(0), 3.5f);
    EXPECT_FLOAT_EQ(resultVec->GetValue(1), 0.2f);
    
    std::cout << "[ArrayMaxTest] Float array_max test passed" << std::endl;

    delete context;
    delete input;
    delete result;
}

/// Test array_max/min with byte (int8) arrays
TEST_F(ArrayMaxTest, ByteArrayMax)
{
    std::cout << "[ArrayMaxTest] Testing byte array_max" << std::endl;
    
    int rowSize = 2;
    auto type = std::make_shared<DataType>(OMNI_BYTE);
    auto types = DataTypes({type});
    
    // Array data: [10, 20, 30], [5, 15]
    // Expected max: 30, 15
    int8_t col[] = {10, 20, 30, 5, 15};
    std::vector<int32_t> offset = {0, 3, 5};
    auto input = CreateArrayVectorBatch(types, offset, rowSize, 5, col);

    auto expr = FuncExpr("array_max", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY))
    }, type);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();
    
    auto resultVec = dynamic_cast<Vector<int8_t>*>(result);
    ASSERT_NE(resultVec, nullptr);
    
    EXPECT_EQ(resultVec->GetValue(0), 30);
    EXPECT_EQ(resultVec->GetValue(1), 15);
    
    std::cout << "[ArrayMaxTest] Byte array_max test passed" << std::endl;

    delete context;
    delete input;
    delete result;
}

/// Test array_max with short (int16) arrays
TEST_F(ArrayMaxTest, ShortArrayMax)
{
    std::cout << "[ArrayMaxTest] Testing short array_max" << std::endl;
    
    int rowSize = 2;
    auto type = std::make_shared<DataType>(OMNI_SHORT);
    auto types = DataTypes({type});
    
    // Array data: [1000, 2000, 3000], [500, 1500]
    // Expected max: 3000, 1500
    int16_t col[] = {1000, 2000, 3000, 500, 1500};
    std::vector<int32_t> offset = {0, 3, 5};
    auto input = CreateArrayVectorBatch(types, offset, rowSize, 5, col);

    auto expr = FuncExpr("array_max", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY))
    }, type);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();
    
    auto resultVec = dynamic_cast<Vector<int16_t>*>(result);
    ASSERT_NE(resultVec, nullptr);
    
    EXPECT_EQ(resultVec->GetValue(0), 3000);
    EXPECT_EQ(resultVec->GetValue(1), 1500);
    
    std::cout << "[ArrayMaxTest] Short array_max test passed" << std::endl;

    delete context;
    delete input;
    delete result;
}
