/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: ArrayInsert function test
 */

#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include <vector>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/ExprEval.h"
#include "expression/expressions.h"
#include "vectorization/registration/SimpleFunctionRegistry.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::mem;
using namespace omniruntime::op;
using namespace omniruntime::expressions;
using namespace omniruntime::TestUtil;
using namespace omniruntime::type;

class ArrayInsertTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

/// Test: insert integer element at positive position 1 (front)
/// array_insert([1, 2, 3], 1, 0, false) -> [0, 1, 2, 3]
TEST_F(ArrayInsertTest, InsertIntAtFront)
{
    int rowSize = 1;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto arrayType = std::make_shared<ArrayType>(intType);
    auto types = DataTypes({arrayType});

    int32_t col[] = {1, 2, 3};
    std::vector<int32_t> offset = {0, 3};
    auto input = CreateArrayVectorBatch(types, offset, rowSize, 3, col);

    auto expr = FuncExpr("array_insert", {
        new FieldExpr(0, arrayType),
        new LiteralExpr(1, intType),
        new LiteralExpr(0, intType),
        new LiteralExpr(false, boolType)
    }, arrayType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_EQ(resultArray->GetSize(), rowSize);
    EXPECT_FALSE(resultArray->IsNull(0));
    EXPECT_EQ(resultArray->GetSize(0), 4);

    auto elementVector = resultArray->GetElementVector();
    auto intVector = dynamic_cast<Vector<int32_t> *>(elementVector.get());
    ASSERT_NE(intVector, nullptr);
    int32_t rOff = resultArray->GetOffset(0);
    EXPECT_EQ(intVector->GetValue(rOff), 0);
    EXPECT_EQ(intVector->GetValue(rOff + 1), 1);
    EXPECT_EQ(intVector->GetValue(rOff + 2), 2);
    EXPECT_EQ(intVector->GetValue(rOff + 3), 3);

    delete context;
    delete input;
    delete result;
}

/// Test: insert at middle position
/// array_insert([1, 2, 3], 2, 99, false) -> [1, 99, 2, 3]
TEST_F(ArrayInsertTest, InsertIntAtMiddle)
{
    int rowSize = 1;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto arrayType = std::make_shared<ArrayType>(intType);
    auto types = DataTypes({arrayType});

    int32_t col[] = {1, 2, 3};
    std::vector<int32_t> offset = {0, 3};
    auto input = CreateArrayVectorBatch(types, offset, rowSize, 3, col);

    auto expr = FuncExpr("array_insert", {
        new FieldExpr(0, arrayType),
        new LiteralExpr(2, intType),
        new LiteralExpr(99, intType),
        new LiteralExpr(false, boolType)
    }, arrayType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_EQ(resultArray->GetSize(0), 4);

    auto elementVector = resultArray->GetElementVector();
    auto intVector = dynamic_cast<Vector<int32_t> *>(elementVector.get());
    int32_t rOff = resultArray->GetOffset(0);
    EXPECT_EQ(intVector->GetValue(rOff), 1);
    EXPECT_EQ(intVector->GetValue(rOff + 1), 99);
    EXPECT_EQ(intVector->GetValue(rOff + 2), 2);
    EXPECT_EQ(intVector->GetValue(rOff + 3), 3);

    delete context;
    delete input;
    delete result;
}

/// Test: insert at position exceeding array size, padding with nulls
/// array_insert([1], 3, 0, false) -> [1, null, 0]
TEST_F(ArrayInsertTest, InsertPosExceedsSize)
{
    int rowSize = 1;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto arrayType = std::make_shared<ArrayType>(intType);
    auto types = DataTypes({arrayType});

    int32_t col[] = {1};
    std::vector<int32_t> offset = {0, 1};
    auto input = CreateArrayVectorBatch(types, offset, rowSize, 1, col);

    auto expr = FuncExpr("array_insert", {
        new FieldExpr(0, arrayType),
        new LiteralExpr(3, intType),
        new LiteralExpr(0, intType),
        new LiteralExpr(false, boolType)
    }, arrayType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_EQ(resultArray->GetSize(0), 3);

    auto elementVector = resultArray->GetElementVector();
    auto intVector = dynamic_cast<Vector<int32_t> *>(elementVector.get());
    int32_t rOff = resultArray->GetOffset(0);
    EXPECT_EQ(intVector->GetValue(rOff), 1);
    EXPECT_TRUE(elementVector->IsNull(rOff + 1));
    EXPECT_EQ(intVector->GetValue(rOff + 2), 0);

    delete context;
    delete input;
    delete result;
}

/// Test: negative position insert (non-legacy)
/// array_insert([1, 2, 3], -1, 0, false) -> [1, 2, 3, 0]
/// array_insert([1, 2, 3], -2, 0, false) -> [1, 2, 0, 3]
TEST_F(ArrayInsertTest, NegativePosition)
{
    int rowSize = 1;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto arrayType = std::make_shared<ArrayType>(intType);
    auto types = DataTypes({arrayType});

    {
        int32_t col[] = {1, 2, 3};
        std::vector<int32_t> offset = {0, 3};
        auto input = CreateArrayVectorBatch(types, offset, rowSize, 3, col);

        auto expr = FuncExpr("array_insert", {
            new FieldExpr(0, arrayType),
            new LiteralExpr(-1, intType),
            new LiteralExpr(0, intType),
            new LiteralExpr(false, boolType)
        }, arrayType);

        auto context = new ExecutionContext(4096, nullptr);
        context->SetResultRowSize(rowSize);

        ExprEval e(input, context);
        e.VisitExpr(expr);
        auto result = e.GetResult();

        auto resultArray = dynamic_cast<ArrayVector *>(result);
        ASSERT_NE(resultArray, nullptr);
        EXPECT_EQ(resultArray->GetSize(0), 4);

        auto elementVector = resultArray->GetElementVector();
        auto intVector = dynamic_cast<Vector<int32_t> *>(elementVector.get());
        int32_t rOff = resultArray->GetOffset(0);
        EXPECT_EQ(intVector->GetValue(rOff), 1);
        EXPECT_EQ(intVector->GetValue(rOff + 1), 2);
        EXPECT_EQ(intVector->GetValue(rOff + 2), 3);
        EXPECT_EQ(intVector->GetValue(rOff + 3), 0);

        delete context;
        delete input;
        delete result;
    }

    {
        int32_t col[] = {1, 2, 3};
        std::vector<int32_t> offset = {0, 3};
        auto input = CreateArrayVectorBatch(types, offset, rowSize, 3, col);

        auto expr = FuncExpr("array_insert", {
            new FieldExpr(0, arrayType),
            new LiteralExpr(-2, intType),
            new LiteralExpr(0, intType),
            new LiteralExpr(false, boolType)
        }, arrayType);

        auto context = new ExecutionContext(4096, nullptr);
        context->SetResultRowSize(rowSize);

        ExprEval e(input, context);
        e.VisitExpr(expr);
        auto result = e.GetResult();

        auto resultArray = dynamic_cast<ArrayVector *>(result);
        ASSERT_NE(resultArray, nullptr);
        EXPECT_EQ(resultArray->GetSize(0), 4);

        auto elementVector = resultArray->GetElementVector();
        auto intVector = dynamic_cast<Vector<int32_t> *>(elementVector.get());
        int32_t rOff = resultArray->GetOffset(0);
        EXPECT_EQ(intVector->GetValue(rOff), 1);
        EXPECT_EQ(intVector->GetValue(rOff + 1), 2);
        EXPECT_EQ(intVector->GetValue(rOff + 2), 0);
        EXPECT_EQ(intVector->GetValue(rOff + 3), 3);

        delete context;
        delete input;
        delete result;
    }
}

/// Test: negative position with legacy index
/// array_insert([1, 2, 3], -1, 0, true) -> [1, 2, 0, 3]
TEST_F(ArrayInsertTest, NegativePositionLegacy)
{
    int rowSize = 1;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto arrayType = std::make_shared<ArrayType>(intType);
    auto types = DataTypes({arrayType});

    int32_t col[] = {1, 2, 3};
    std::vector<int32_t> offset = {0, 3};
    auto input = CreateArrayVectorBatch(types, offset, rowSize, 3, col);

    auto expr = FuncExpr("array_insert", {
        new FieldExpr(0, arrayType),
        new LiteralExpr(-1, intType),
        new LiteralExpr(0, intType),
        new LiteralExpr(true, boolType)
    }, arrayType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_EQ(resultArray->GetSize(0), 4);

    auto elementVector = resultArray->GetElementVector();
    auto intVector = dynamic_cast<Vector<int32_t> *>(elementVector.get());
    int32_t rOff = resultArray->GetOffset(0);
    EXPECT_EQ(intVector->GetValue(rOff), 1);
    EXPECT_EQ(intVector->GetValue(rOff + 1), 2);
    EXPECT_EQ(intVector->GetValue(rOff + 2), 0);
    EXPECT_EQ(intVector->GetValue(rOff + 3), 3);

    delete context;
    delete input;
    delete result;
}

/// Test: negative pos extends array to the left
/// array_insert([1], -3, 0, false) -> [0, null, 1]
TEST_F(ArrayInsertTest, NegativePosExtendsLeft)
{
    int rowSize = 1;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto arrayType = std::make_shared<ArrayType>(intType);
    auto types = DataTypes({arrayType});

    int32_t col[] = {1};
    std::vector<int32_t> offset = {0, 1};
    auto input = CreateArrayVectorBatch(types, offset, rowSize, 1, col);

    auto expr = FuncExpr("array_insert", {
        new FieldExpr(0, arrayType),
        new LiteralExpr(-3, intType),
        new LiteralExpr(0, intType),
        new LiteralExpr(false, boolType)
    }, arrayType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_EQ(resultArray->GetSize(0), 3);

    auto elementVector = resultArray->GetElementVector();
    auto intVector = dynamic_cast<Vector<int32_t> *>(elementVector.get());
    int32_t rOff = resultArray->GetOffset(0);
    EXPECT_EQ(intVector->GetValue(rOff), 0);
    EXPECT_TRUE(elementVector->IsNull(rOff + 1));
    EXPECT_EQ(intVector->GetValue(rOff + 2), 1);

    delete context;
    delete input;
    delete result;
}

/// Test: null item insertion
/// array_insert([1, 2], 2, null, false) -> [1, null, 2]
TEST_F(ArrayInsertTest, NullItemInsert)
{
    int rowSize = 1;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto arrayType = std::make_shared<ArrayType>(intType);
    auto types = DataTypes({arrayType});

    int32_t col[] = {1, 2};
    std::vector<int32_t> offset = {0, 2};
    auto input = CreateArrayVectorBatch(types, offset, rowSize, 2, col);

    auto expr = FuncExpr("array_insert", {
        new FieldExpr(0, arrayType),
        new LiteralExpr(2, intType),
        new LiteralExpr(0, intType, true),
        new LiteralExpr(false, boolType)
    }, arrayType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_EQ(resultArray->GetSize(0), 3);

    auto elementVector = resultArray->GetElementVector();
    auto intVector = dynamic_cast<Vector<int32_t> *>(elementVector.get());
    int32_t rOff = resultArray->GetOffset(0);
    EXPECT_EQ(intVector->GetValue(rOff), 1);
    EXPECT_TRUE(elementVector->IsNull(rOff + 1));
    EXPECT_EQ(intVector->GetValue(rOff + 2), 2);

    delete context;
    delete input;
    delete result;
}

/// Test: multiple rows with different arrays
/// Row 0: array_insert([1, 2, 3, 4, 5], 1, 0, false) -> [0, 1, 2, 3, 4, 5]
/// Row 1: array_insert([10, 20, 30], 1, 0, false) -> [0, 10, 20, 30]
/// Row 2: array_insert([100, 200], 1, 0, false) -> [0, 100, 200]
TEST_F(ArrayInsertTest, MultipleRows)
{
    int rowSize = 3;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto arrayType = std::make_shared<ArrayType>(intType);
    auto types = DataTypes({arrayType});

    int32_t col[] = {1, 2, 3, 4, 5, 10, 20, 30, 100, 200};
    std::vector<int32_t> offset = {0, 5, 8, 10};
    auto input = CreateArrayVectorBatch(types, offset, rowSize, 10, col);

    auto expr = FuncExpr("array_insert", {
        new FieldExpr(0, arrayType),
        new LiteralExpr(1, intType),
        new LiteralExpr(0, intType),
        new LiteralExpr(false, boolType)
    }, arrayType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_EQ(resultArray->GetSize(), rowSize);

    auto elementVector = resultArray->GetElementVector();
    auto intVector = dynamic_cast<Vector<int32_t> *>(elementVector.get());

    EXPECT_EQ(resultArray->GetSize(0), 6);
    EXPECT_EQ(intVector->GetValue(resultArray->GetOffset(0)), 0);
    EXPECT_EQ(intVector->GetValue(resultArray->GetOffset(0) + 1), 1);

    EXPECT_EQ(resultArray->GetSize(1), 4);
    EXPECT_EQ(intVector->GetValue(resultArray->GetOffset(1)), 0);
    EXPECT_EQ(intVector->GetValue(resultArray->GetOffset(1) + 1), 10);

    EXPECT_EQ(resultArray->GetSize(2), 3);
    EXPECT_EQ(intVector->GetValue(resultArray->GetOffset(2)), 0);
    EXPECT_EQ(intVector->GetValue(resultArray->GetOffset(2) + 1), 100);

    delete context;
    delete input;
    delete result;
}

/// Test: insert with double type array
/// array_insert([1.1, 2.2, 3.3], 2, 9.9, false) -> [1.1, 9.9, 2.2, 3.3]
TEST_F(ArrayInsertTest, DoubleArrayInsert)
{
    int rowSize = 1;
    auto doubleType = std::make_shared<DataType>(OMNI_DOUBLE);
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto arrayType = std::make_shared<ArrayType>(doubleType);
    auto types = DataTypes({arrayType});

    double col[] = {1.1, 2.2, 3.3};
    std::vector<int32_t> offset = {0, 3};
    auto input = CreateArrayVectorBatch(types, offset, rowSize, 3, col);

    auto expr = FuncExpr("array_insert", {
        new FieldExpr(0, arrayType),
        new LiteralExpr(2, intType),
        new LiteralExpr(9.9, doubleType),
        new LiteralExpr(false, boolType)
    }, arrayType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_EQ(resultArray->GetSize(0), 4);

    auto elementVector = resultArray->GetElementVector();
    auto dblVector = dynamic_cast<Vector<double> *>(elementVector.get());
    ASSERT_NE(dblVector, nullptr);
    int32_t rOff = resultArray->GetOffset(0);
    EXPECT_DOUBLE_EQ(dblVector->GetValue(rOff), 1.1);
    EXPECT_DOUBLE_EQ(dblVector->GetValue(rOff + 1), 9.9);
    EXPECT_DOUBLE_EQ(dblVector->GetValue(rOff + 2), 2.2);
    EXPECT_DOUBLE_EQ(dblVector->GetValue(rOff + 3), 3.3);

    delete context;
    delete input;
    delete result;
}

/// Test: insert with float type array
/// array_insert([1.5f, 2.5f], 1, 0.5f, false) -> [0.5f, 1.5f, 2.5f]
TEST_F(ArrayInsertTest, FloatArrayInsert)
{
    int rowSize = 1;
    auto floatType = std::make_shared<DataType>(OMNI_FLOAT);
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto arrayType = std::make_shared<ArrayType>(floatType);
    auto types = DataTypes({arrayType});

    float col[] = {1.5f, 2.5f};
    std::vector<int32_t> offset = {0, 2};
    auto input = CreateArrayVectorBatch(types, offset, rowSize, 2, col);

    auto expr = FuncExpr("array_insert", {
        new FieldExpr(0, arrayType),
        new LiteralExpr(1, intType),
        new LiteralExpr(0.5f, floatType),
        new LiteralExpr(false, boolType)
    }, arrayType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_EQ(resultArray->GetSize(0), 3);

    auto elementVector = resultArray->GetElementVector();
    auto fltVector = dynamic_cast<Vector<float> *>(elementVector.get());
    ASSERT_NE(fltVector, nullptr);
    int32_t rOff = resultArray->GetOffset(0);
    EXPECT_FLOAT_EQ(fltVector->GetValue(rOff), 0.5f);
    EXPECT_FLOAT_EQ(fltVector->GetValue(rOff + 1), 1.5f);
    EXPECT_FLOAT_EQ(fltVector->GetValue(rOff + 2), 2.5f);

    delete context;
    delete input;
    delete result;
}

/// Test: insert with OMNI_LONG type array
/// array_insert([100L, 200L, 300L], 2, 999L, false) -> [100L, 999L, 200L, 300L]
TEST_F(ArrayInsertTest, LongArrayInsert)
{
    int rowSize = 1;
    auto longType = std::make_shared<DataType>(OMNI_LONG);
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto arrayType = std::make_shared<ArrayType>(longType);
    auto types = DataTypes({arrayType});

    int64_t col[] = {100L, 200L, 300L};
    std::vector<int32_t> offset = {0, 3};
    auto input = CreateArrayVectorBatch(types, offset, rowSize, 3, col);

    auto expr = FuncExpr("array_insert", {
        new FieldExpr(0, arrayType),
        new LiteralExpr(2, intType),
        new LiteralExpr(static_cast<int64_t>(999), longType),
        new LiteralExpr(false, boolType)
    }, arrayType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_EQ(resultArray->GetSize(0), 4);

    auto elementVector = resultArray->GetElementVector();
    auto longVector = dynamic_cast<Vector<int64_t> *>(elementVector.get());
    ASSERT_NE(longVector, nullptr);
    int32_t rOff = resultArray->GetOffset(0);
    EXPECT_EQ(longVector->GetValue(rOff), 100L);
    EXPECT_EQ(longVector->GetValue(rOff + 1), 999L);
    EXPECT_EQ(longVector->GetValue(rOff + 2), 200L);
    EXPECT_EQ(longVector->GetValue(rOff + 3), 300L);

    delete context;
    delete input;
    delete result;
}

/// Test: insert at end position (append)
/// array_insert([1, 2, 3], 4, 4, false) -> [1, 2, 3, 4]
TEST_F(ArrayInsertTest, InsertAtEnd)
{
    int rowSize = 1;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto arrayType = std::make_shared<ArrayType>(intType);
    auto types = DataTypes({arrayType});

    int32_t col[] = {1, 2, 3};
    std::vector<int32_t> offset = {0, 3};
    auto input = CreateArrayVectorBatch(types, offset, rowSize, 3, col);

    auto expr = FuncExpr("array_insert", {
        new FieldExpr(0, arrayType),
        new LiteralExpr(4, intType),
        new LiteralExpr(4, intType),
        new LiteralExpr(false, boolType)
    }, arrayType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_EQ(resultArray->GetSize(0), 4);

    auto elementVector = resultArray->GetElementVector();
    auto intVector = dynamic_cast<Vector<int32_t> *>(elementVector.get());
    int32_t rOff = resultArray->GetOffset(0);
    EXPECT_EQ(intVector->GetValue(rOff), 1);
    EXPECT_EQ(intVector->GetValue(rOff + 1), 2);
    EXPECT_EQ(intVector->GetValue(rOff + 2), 3);
    EXPECT_EQ(intVector->GetValue(rOff + 3), 4);

    delete context;
    delete input;
    delete result;
}

/// Test: insert with empty array
/// array_insert([], 1, 42, false) -> [42]
TEST_F(ArrayInsertTest, InsertIntoEmptyArray)
{
    int rowSize = 1;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto arrayType = std::make_shared<ArrayType>(intType);
    auto types = DataTypes({arrayType});

    int32_t col[] = {};
    std::vector<int32_t> offset = {0, 0};
    auto input = CreateArrayVectorBatch(types, offset, rowSize, 0, col);

    auto expr = FuncExpr("array_insert", {
        new FieldExpr(0, arrayType),
        new LiteralExpr(1, intType),
        new LiteralExpr(42, intType),
        new LiteralExpr(false, boolType)
    }, arrayType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_EQ(resultArray->GetSize(0), 1);

    auto elementVector = resultArray->GetElementVector();
    auto intVector = dynamic_cast<Vector<int32_t> *>(elementVector.get());
    ASSERT_NE(intVector, nullptr);
    int32_t rOff = resultArray->GetOffset(0);
    EXPECT_EQ(intVector->GetValue(rOff), 42);

    delete context;
    delete input;
    delete result;
}

/// Test: negative pos extends left with legacy mode
/// array_insert([1], -3, 0, true) -> [0, null, null, 1]
TEST_F(ArrayInsertTest, NegativePosExtendsLeftLegacy)
{
    int rowSize = 1;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto arrayType = std::make_shared<ArrayType>(intType);
    auto types = DataTypes({arrayType});

    int32_t col[] = {1};
    std::vector<int32_t> offset = {0, 1};
    auto input = CreateArrayVectorBatch(types, offset, rowSize, 1, col);

    auto expr = FuncExpr("array_insert", {
        new FieldExpr(0, arrayType),
        new LiteralExpr(-3, intType),
        new LiteralExpr(0, intType),
        new LiteralExpr(true, boolType)
    }, arrayType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_EQ(resultArray->GetSize(0), 4);

    auto elementVector = resultArray->GetElementVector();
    auto intVector = dynamic_cast<Vector<int32_t> *>(elementVector.get());
    int32_t rOff = resultArray->GetOffset(0);
    EXPECT_EQ(intVector->GetValue(rOff), 0);
    EXPECT_TRUE(elementVector->IsNull(rOff + 1));
    EXPECT_TRUE(elementVector->IsNull(rOff + 2));
    EXPECT_EQ(intVector->GetValue(rOff + 3), 1);

    delete context;
    delete input;
    delete result;
}

/// Test: insert with per-row varying items via column reference
TEST_F(ArrayInsertTest, InsertWithColumnItem)
{
    int rowSize = 2;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto arrayType = std::make_shared<ArrayType>(intType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto elemVec = std::shared_ptr<BaseVector>(new Vector<int32_t>(5));
    auto *ev = dynamic_cast<Vector<int32_t> *>(elemVec.get());
    ev->SetValue(0, 1);
    ev->SetValue(1, 2);
    ev->SetValue(2, 3);
    ev->SetValue(3, 10);
    ev->SetValue(4, 20);

    auto *arrayVec = new ArrayVector(rowSize, elemVec);
    arrayVec->SetOffset(0, 0);
    arrayVec->SetOffset(1, 3);
    arrayVec->SetOffset(2, 5);
    vectorBatch->Append(arrayVec);

    auto *itemVec = new Vector<int32_t>(rowSize);
    itemVec->SetValue(0, 99);
    itemVec->SetValue(1, 88);
    vectorBatch->Append(itemVec);

    auto expr = FuncExpr("array_insert", {
        new FieldExpr(0, arrayType),
        new LiteralExpr(1, intType),
        new FieldExpr(1, intType),
        new LiteralExpr(false, boolType)
    }, arrayType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);

    auto resultElem = resultArray->GetElementVector();
    auto resIntVec = dynamic_cast<Vector<int32_t> *>(resultElem.get());
    ASSERT_NE(resIntVec, nullptr);

    EXPECT_EQ(resultArray->GetSize(0), 4);
    EXPECT_EQ(resIntVec->GetValue(resultArray->GetOffset(0)), 99);
    EXPECT_EQ(resIntVec->GetValue(resultArray->GetOffset(0) + 1), 1);

    EXPECT_EQ(resultArray->GetSize(1), 3);
    EXPECT_EQ(resIntVec->GetValue(resultArray->GetOffset(1)), 88);
    EXPECT_EQ(resIntVec->GetValue(resultArray->GetOffset(1) + 1), 10);

    delete context;
    delete vectorBatch;
    delete result;
}

/// Test: insert with OMNI_SHORT type
/// array_insert([10, 20], 1, 5, false) -> [5, 10, 20]
TEST_F(ArrayInsertTest, ShortArrayInsert)
{
    int rowSize = 1;
    auto shortType = std::make_shared<DataType>(OMNI_SHORT);
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto arrayType = std::make_shared<ArrayType>(shortType);
    auto types = DataTypes({arrayType});

    int16_t col[] = {10, 20};
    std::vector<int32_t> offset = {0, 2};
    auto input = CreateArrayVectorBatch(types, offset, rowSize, 2, col);

    auto expr = FuncExpr("array_insert", {
        new FieldExpr(0, arrayType),
        new LiteralExpr(1, intType),
        new LiteralExpr(static_cast<int16_t>(5), shortType),
        new LiteralExpr(false, boolType)
    }, arrayType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_EQ(resultArray->GetSize(0), 3);

    auto elementVector = resultArray->GetElementVector();
    auto shortVector = dynamic_cast<Vector<int16_t> *>(elementVector.get());
    ASSERT_NE(shortVector, nullptr);
    int32_t rOff = resultArray->GetOffset(0);
    EXPECT_EQ(shortVector->GetValue(rOff), 5);
    EXPECT_EQ(shortVector->GetValue(rOff + 1), 10);
    EXPECT_EQ(shortVector->GetValue(rOff + 2), 20);

    delete context;
    delete input;
    delete result;
}

/// Test: insert with OMNI_BYTE type
/// array_insert([1, 2], 3, 3, false) -> [1, 2, 3]
TEST_F(ArrayInsertTest, ByteArrayInsert)
{
    int rowSize = 1;
    auto byteType = std::make_shared<DataType>(OMNI_BYTE);
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto arrayType = std::make_shared<ArrayType>(byteType);
    auto types = DataTypes({arrayType});

    int8_t col[] = {1, 2};
    std::vector<int32_t> offset = {0, 2};
    auto input = CreateArrayVectorBatch(types, offset, rowSize, 2, col);

    auto expr = FuncExpr("array_insert", {
        new FieldExpr(0, arrayType),
        new LiteralExpr(3, intType),
        new LiteralExpr(static_cast<int8_t>(3), byteType),
        new LiteralExpr(false, boolType)
    }, arrayType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_EQ(resultArray->GetSize(0), 3);

    auto elementVector = resultArray->GetElementVector();
    auto byteVector = dynamic_cast<Vector<int8_t> *>(elementVector.get());
    ASSERT_NE(byteVector, nullptr);
    int32_t rOff = resultArray->GetOffset(0);
    EXPECT_EQ(byteVector->GetValue(rOff), 1);
    EXPECT_EQ(byteVector->GetValue(rOff + 1), 2);
    EXPECT_EQ(byteVector->GetValue(rOff + 2), 3);

    delete context;
    delete input;
    delete result;
}
