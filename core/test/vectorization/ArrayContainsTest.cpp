/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Unit tests for array_contains function
 */

#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <limits>
#include <cmath>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/ExprEval.h"
#include "expression/expressions.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::mem;
using namespace omniruntime::op;
using namespace omniruntime::expressions;
using namespace omniruntime::TestUtil;

class ArrayContainsTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

/// Test array_contains with integer arrays, value found
TEST_F(ArrayContainsTest, IntegerArrayContainsFound)
{
    int rowSize = 3;
    auto type = std::make_shared<DataType>(OMNI_INT);
    auto types = DataTypes({type});

    // Row 0: [1, 2, 3], Row 1: [4, 5], Row 2: [6]
    int32_t col[] = {1, 2, 3, 4, 5, 6};
    std::vector<int32_t> offsets = {0, 3, 5, 6};
    auto input = CreateArrayVectorBatch(types, offsets, rowSize, 6, col);

    // Search for value 2: expected [true, false, false]
    auto expr = FuncExpr("array_contains", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new LiteralExpr(static_cast<int32_t>(2), type)
    }, std::make_shared<DataType>(OMNI_BOOLEAN));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultVec = dynamic_cast<Vector<bool> *>(result);
    ASSERT_NE(resultVec, nullptr);

    EXPECT_EQ(resultVec->GetValue(0), true);   // [1,2,3] contains 2
    EXPECT_EQ(resultVec->GetValue(1), false);   // [4,5] does not contain 2
    EXPECT_EQ(resultVec->GetValue(2), false);   // [6] does not contain 2

    delete context;
    delete input;
    delete result;
}

/// Test array_contains with integer arrays, value not found
TEST_F(ArrayContainsTest, IntegerArrayContainsNotFound)
{
    int rowSize = 2;
    auto type = std::make_shared<DataType>(OMNI_INT);
    auto types = DataTypes({type});

    // Row 0: [1, 2, 3], Row 1: [4, 5]
    int32_t col[] = {1, 2, 3, 4, 5};
    std::vector<int32_t> offsets = {0, 3, 5};
    auto input = CreateArrayVectorBatch(types, offsets, rowSize, 5, col);

    // Search for value 99
    auto expr = FuncExpr("array_contains", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new LiteralExpr(static_cast<int32_t>(99), type)
    }, std::make_shared<DataType>(OMNI_BOOLEAN));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultVec = dynamic_cast<Vector<bool> *>(result);
    ASSERT_NE(resultVec, nullptr);

    EXPECT_EQ(resultVec->GetValue(0), false);
    EXPECT_EQ(resultVec->GetValue(1), false);

    delete context;
    delete input;
    delete result;
}

/// Test array_contains with empty arrays
TEST_F(ArrayContainsTest, EmptyArray)
{
    int rowSize = 2;
    auto type = std::make_shared<DataType>(OMNI_INT);
    auto types = DataTypes({type});

    // Row 0: [], Row 1: [1]
    int32_t col[] = {1};
    std::vector<int32_t> offsets = {0, 0, 1};
    auto input = CreateArrayVectorBatch(types, offsets, rowSize, 1, col);

    auto expr = FuncExpr("array_contains", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new LiteralExpr(static_cast<int32_t>(1), type)
    }, std::make_shared<DataType>(OMNI_BOOLEAN));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultVec = dynamic_cast<Vector<bool> *>(result);
    ASSERT_NE(resultVec, nullptr);

    EXPECT_EQ(resultVec->GetValue(0), false);  // [] does not contain 1
    EXPECT_EQ(resultVec->GetValue(1), true);   // [1] contains 1

    delete context;
    delete input;
    delete result;
}

/// Test array_contains with null elements in array (three-valued logic)
TEST_F(ArrayContainsTest, ArrayWithNullElements)
{
    int rowSize = 2;
    // Row 0: [1, null, 3] search 3 -> true (found despite null)
    // Row 1: [null, 5]    search 99 -> NULL (not found, has null)
    auto *elementVec = new Vector<int32_t>(5);
    elementVec->SetValue(0, 1);
    elementVec->SetNull(1);
    elementVec->SetValue(2, 3);
    elementVec->SetNull(3);
    elementVec->SetValue(4, 5);

    auto elementShared = std::shared_ptr<BaseVector>(elementVec);
    auto *arrayVec = new ArrayVector(rowSize, elementShared);
    arrayVec->SetOffset(0, 0);
    arrayVec->SetOffset(1, 3);
    arrayVec->SetOffset(2, 5);

    auto *input = new VectorBatch(rowSize);
    input->Append(arrayVec);

    auto type = std::make_shared<DataType>(OMNI_INT);

    // Search for value 3
    auto expr = FuncExpr("array_contains", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new LiteralExpr(static_cast<int32_t>(3), type)
    }, std::make_shared<DataType>(OMNI_BOOLEAN));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultVec = dynamic_cast<Vector<bool> *>(result);
    ASSERT_NE(resultVec, nullptr);

    // Row 0: [1, null, 3] contains 3 -> true
    EXPECT_FALSE(resultVec->IsNull(0));
    EXPECT_EQ(resultVec->GetValue(0), true);

    // Row 1: [null, 5] does not contain 3, but has null -> NULL
    EXPECT_TRUE(resultVec->IsNull(1));

    delete context;
    delete input;
    delete result;
}

/// Test array_contains with long type arrays
TEST_F(ArrayContainsTest, LongArrayContains)
{
    int rowSize = 2;
    auto type = std::make_shared<DataType>(OMNI_LONG);
    auto types = DataTypes({type});

    // Row 0: [100, 200, 300], Row 1: [400, 500]
    int64_t col[] = {100L, 200L, 300L, 400L, 500L};
    std::vector<int32_t> offsets = {0, 3, 5};
    auto input = CreateArrayVectorBatch(types, offsets, rowSize, 5, col);

    auto expr = FuncExpr("array_contains", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new LiteralExpr(static_cast<int64_t>(200L), type)
    }, std::make_shared<DataType>(OMNI_BOOLEAN));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultVec = dynamic_cast<Vector<bool> *>(result);
    ASSERT_NE(resultVec, nullptr);

    EXPECT_EQ(resultVec->GetValue(0), true);   // [100,200,300] contains 200
    EXPECT_EQ(resultVec->GetValue(1), false);   // [400,500] does not contain 200

    delete context;
    delete input;
    delete result;
}

/// Test array_contains with double type arrays
TEST_F(ArrayContainsTest, DoubleArrayContains)
{
    int rowSize = 2;
    auto type = std::make_shared<DataType>(OMNI_DOUBLE);
    auto types = DataTypes({type});

    // Row 0: [1.1, 2.2, 3.3], Row 1: [4.4, 5.5]
    double col[] = {1.1, 2.2, 3.3, 4.4, 5.5};
    std::vector<int32_t> offsets = {0, 3, 5};
    auto input = CreateArrayVectorBatch(types, offsets, rowSize, 5, col);

    auto expr = FuncExpr("array_contains", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new LiteralExpr(2.2, type)
    }, std::make_shared<DataType>(OMNI_BOOLEAN));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultVec = dynamic_cast<Vector<bool> *>(result);
    ASSERT_NE(resultVec, nullptr);

    EXPECT_EQ(resultVec->GetValue(0), true);   // [1.1,2.2,3.3] contains 2.2
    EXPECT_EQ(resultVec->GetValue(1), false);   // [4.4,5.5] does not contain 2.2

    delete context;
    delete input;
    delete result;
}

/// Test array_contains with boolean type arrays
TEST_F(ArrayContainsTest, BooleanArrayContains)
{
    int rowSize = 2;
    auto type = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto types = DataTypes({type});

    // Row 0: [true, false], Row 1: [false, false]
    bool col[] = {true, false, false, false};
    std::vector<int32_t> offsets = {0, 2, 4};
    auto input = CreateArrayVectorBatch(types, offsets, rowSize, 4, col);

    auto expr = FuncExpr("array_contains", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new LiteralExpr(true, type)
    }, std::make_shared<DataType>(OMNI_BOOLEAN));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultVec = dynamic_cast<Vector<bool> *>(result);
    ASSERT_NE(resultVec, nullptr);

    EXPECT_EQ(resultVec->GetValue(0), true);   // [true,false] contains true
    EXPECT_EQ(resultVec->GetValue(1), false);   // [false,false] does not contain true

    delete context;
    delete input;
    delete result;
}

/// Test array_contains with negative numbers
TEST_F(ArrayContainsTest, NegativeNumbers)
{
    int rowSize = 2;
    auto type = std::make_shared<DataType>(OMNI_INT);
    auto types = DataTypes({type});

    // Row 0: [-5, -3, -1], Row 1: [-100, -50]
    int32_t col[] = {-5, -3, -1, -100, -50};
    std::vector<int32_t> offsets = {0, 3, 5};
    auto input = CreateArrayVectorBatch(types, offsets, rowSize, 5, col);

    auto expr = FuncExpr("array_contains", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new LiteralExpr(static_cast<int32_t>(-3), type)
    }, std::make_shared<DataType>(OMNI_BOOLEAN));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultVec = dynamic_cast<Vector<bool> *>(result);
    ASSERT_NE(resultVec, nullptr);

    EXPECT_EQ(resultVec->GetValue(0), true);   // [-5,-3,-1] contains -3
    EXPECT_EQ(resultVec->GetValue(1), false);   // [-100,-50] does not contain -3

    delete context;
    delete input;
    delete result;
}

/// Test array_contains with boundary values
TEST_F(ArrayContainsTest, BoundaryValues)
{
    int rowSize = 2;
    auto type = std::make_shared<DataType>(OMNI_INT);
    auto types = DataTypes({type});

    constexpr int32_t kMin = std::numeric_limits<int32_t>::min();
    constexpr int32_t kMax = std::numeric_limits<int32_t>::max();

    // Row 0: [kMin, 0, kMax], Row 1: [kMax, kMax]
    int32_t col[] = {kMin, 0, kMax, kMax, kMax};
    std::vector<int32_t> offsets = {0, 3, 5};
    auto input = CreateArrayVectorBatch(types, offsets, rowSize, 5, col);

    auto expr = FuncExpr("array_contains", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new LiteralExpr(kMin, type)
    }, std::make_shared<DataType>(OMNI_BOOLEAN));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultVec = dynamic_cast<Vector<bool> *>(result);
    ASSERT_NE(resultVec, nullptr);

    EXPECT_EQ(resultVec->GetValue(0), true);   // [kMin,0,kMax] contains kMin
    EXPECT_EQ(resultVec->GetValue(1), false);   // [kMax,kMax] does not contain kMin

    delete context;
    delete input;
    delete result;
}

/// Test array_contains with single element arrays
TEST_F(ArrayContainsTest, SingleElementArray)
{
    int rowSize = 3;
    auto type = std::make_shared<DataType>(OMNI_INT);
    auto types = DataTypes({type});

    // Row 0: [42], Row 1: [100], Row 2: [42]
    int32_t col[] = {42, 100, 42};
    std::vector<int32_t> offsets = {0, 1, 2, 3};
    auto input = CreateArrayVectorBatch(types, offsets, rowSize, 3, col);

    auto expr = FuncExpr("array_contains", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new LiteralExpr(static_cast<int32_t>(42), type)
    }, std::make_shared<DataType>(OMNI_BOOLEAN));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultVec = dynamic_cast<Vector<bool> *>(result);
    ASSERT_NE(resultVec, nullptr);

    EXPECT_EQ(resultVec->GetValue(0), true);
    EXPECT_EQ(resultVec->GetValue(1), false);
    EXPECT_EQ(resultVec->GetValue(2), true);

    delete context;
    delete input;
    delete result;
}

/// Test array_contains with float arrays
TEST_F(ArrayContainsTest, FloatArrayContains)
{
    int rowSize = 2;
    auto type = std::make_shared<DataType>(OMNI_FLOAT);
    auto types = DataTypes({type});

    // Row 0: [1.5f, 2.5f, 3.5f], Row 1: [0.1f, 0.2f]
    float col[] = {1.5f, 2.5f, 3.5f, 0.1f, 0.2f};
    std::vector<int32_t> offsets = {0, 3, 5};
    auto input = CreateArrayVectorBatch(types, offsets, rowSize, 5, col);

    auto expr = FuncExpr("array_contains", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new LiteralExpr(2.5f, type)
    }, std::make_shared<DataType>(OMNI_BOOLEAN));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultVec = dynamic_cast<Vector<bool> *>(result);
    ASSERT_NE(resultVec, nullptr);

    EXPECT_EQ(resultVec->GetValue(0), true);
    EXPECT_EQ(resultVec->GetValue(1), false);

    delete context;
    delete input;
    delete result;
}

/// Test array_contains with byte type arrays
TEST_F(ArrayContainsTest, ByteArrayContains)
{
    int rowSize = 2;
    auto type = std::make_shared<DataType>(OMNI_BYTE);
    auto types = DataTypes({type});

    // Row 0: [10, 20, 30], Row 1: [5, 15]
    int8_t col[] = {10, 20, 30, 5, 15};
    std::vector<int32_t> offsets = {0, 3, 5};
    auto input = CreateArrayVectorBatch(types, offsets, rowSize, 5, col);

    auto expr = FuncExpr("array_contains", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new LiteralExpr(static_cast<int8_t>(20), type)
    }, std::make_shared<DataType>(OMNI_BOOLEAN));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultVec = dynamic_cast<Vector<bool> *>(result);
    ASSERT_NE(resultVec, nullptr);

    EXPECT_EQ(resultVec->GetValue(0), true);
    EXPECT_EQ(resultVec->GetValue(1), false);

    delete context;
    delete input;
    delete result;
}

/// Test array_contains with short type arrays
TEST_F(ArrayContainsTest, ShortArrayContains)
{
    int rowSize = 2;
    auto type = std::make_shared<DataType>(OMNI_SHORT);
    auto types = DataTypes({type});

    // Row 0: [1000, 2000, 3000], Row 1: [500, 1500]
    int16_t col[] = {1000, 2000, 3000, 500, 1500};
    std::vector<int32_t> offsets = {0, 3, 5};
    auto input = CreateArrayVectorBatch(types, offsets, rowSize, 5, col);

    auto expr = FuncExpr("array_contains", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new LiteralExpr(static_cast<int16_t>(2000), type)
    }, std::make_shared<DataType>(OMNI_BOOLEAN));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultVec = dynamic_cast<Vector<bool> *>(result);
    ASSERT_NE(resultVec, nullptr);

    EXPECT_EQ(resultVec->GetValue(0), true);
    EXPECT_EQ(resultVec->GetValue(1), false);

    delete context;
    delete input;
    delete result;
}
