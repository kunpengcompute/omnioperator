/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Unit tests for array_position function
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

class ArrayPositionTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

/// Test array_position with integer arrays, value found
TEST_F(ArrayPositionTest, IntegerArrayPositionFound)
{
    int rowSize = 3;
    auto type = std::make_shared<DataType>(OMNI_INT);
    auto types = DataTypes({type});

    // Row 0: [1, 2, 3], Row 1: [4, 5], Row 2: [6]
    int32_t col[] = {1, 2, 3, 4, 5, 6};
    std::vector<int32_t> offsets = {0, 3, 5, 6};
    auto input = CreateArrayVectorBatch(types, offsets, rowSize, 6, col);

    auto expr = FuncExpr("array_position", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new LiteralExpr(static_cast<int32_t>(2), type)
    }, std::make_shared<DataType>(OMNI_LONG));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultVec = dynamic_cast<Vector<int64_t> *>(result);
    ASSERT_NE(resultVec, nullptr);

    EXPECT_EQ(resultVec->GetValue(0), 2);   // [1,2,3] position of 2 -> 2
    EXPECT_EQ(resultVec->GetValue(1), 0);   // [4,5] position of 2 -> 0 (not found)
    EXPECT_EQ(resultVec->GetValue(2), 0);   // [6] position of 2 -> 0 (not found)

    delete context;
    delete input;
    delete result;
}

/// Test array_position with value not found in any row
TEST_F(ArrayPositionTest, IntegerArrayPositionNotFound)
{
    int rowSize = 2;
    auto type = std::make_shared<DataType>(OMNI_INT);
    auto types = DataTypes({type});

    // Row 0: [1, 2, 3], Row 1: [4, 5]
    int32_t col[] = {1, 2, 3, 4, 5};
    std::vector<int32_t> offsets = {0, 3, 5};
    auto input = CreateArrayVectorBatch(types, offsets, rowSize, 5, col);

    auto expr = FuncExpr("array_position", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new LiteralExpr(static_cast<int32_t>(99), type)
    }, std::make_shared<DataType>(OMNI_LONG));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultVec = dynamic_cast<Vector<int64_t> *>(result);
    ASSERT_NE(resultVec, nullptr);

    EXPECT_EQ(resultVec->GetValue(0), 0);
    EXPECT_EQ(resultVec->GetValue(1), 0);

    delete context;
    delete input;
    delete result;
}

/// Test array_position with empty arrays
TEST_F(ArrayPositionTest, EmptyArray)
{
    int rowSize = 2;
    auto type = std::make_shared<DataType>(OMNI_INT);
    auto types = DataTypes({type});

    // Row 0: [], Row 1: [1]
    int32_t col[] = {1};
    std::vector<int32_t> offsets = {0, 0, 1};
    auto input = CreateArrayVectorBatch(types, offsets, rowSize, 1, col);

    auto expr = FuncExpr("array_position", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new LiteralExpr(static_cast<int32_t>(1), type)
    }, std::make_shared<DataType>(OMNI_LONG));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultVec = dynamic_cast<Vector<int64_t> *>(result);
    ASSERT_NE(resultVec, nullptr);

    EXPECT_EQ(resultVec->GetValue(0), 0);  // [] position of 1 -> 0
    EXPECT_EQ(resultVec->GetValue(1), 1);  // [1] position of 1 -> 1

    delete context;
    delete input;
    delete result;
}

/// Test array_position with null elements in array
TEST_F(ArrayPositionTest, ArrayWithNullElements)
{
    int rowSize = 2;
    // Row 0: [1, null, 3] search 3 -> 3 (skips null, position of 3 is 3)
    // Row 1: [null, 5]    search 5 -> 2 (skips null, position of 5 is 2)
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

    auto expr = FuncExpr("array_position", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new LiteralExpr(static_cast<int32_t>(3), type)
    }, std::make_shared<DataType>(OMNI_LONG));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultVec = dynamic_cast<Vector<int64_t> *>(result);
    ASSERT_NE(resultVec, nullptr);

    EXPECT_EQ(resultVec->GetValue(0), 3);  // [1, null, 3] position of 3 -> 3
    EXPECT_EQ(resultVec->GetValue(1), 0);  // [null, 5] position of 3 -> 0

    delete context;
    delete input;
    delete result;
}

/// Test array_position with long type arrays
TEST_F(ArrayPositionTest, LongArrayPosition)
{
    int rowSize = 2;
    auto type = std::make_shared<DataType>(OMNI_LONG);
    auto types = DataTypes({type});

    // Row 0: [100, 200, 300], Row 1: [400, 500]
    int64_t col[] = {100L, 200L, 300L, 400L, 500L};
    std::vector<int32_t> offsets = {0, 3, 5};
    auto input = CreateArrayVectorBatch(types, offsets, rowSize, 5, col);

    auto expr = FuncExpr("array_position", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new LiteralExpr(static_cast<int64_t>(300L), type)
    }, std::make_shared<DataType>(OMNI_LONG));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultVec = dynamic_cast<Vector<int64_t> *>(result);
    ASSERT_NE(resultVec, nullptr);

    EXPECT_EQ(resultVec->GetValue(0), 3);   // [100,200,300] position of 300 -> 3
    EXPECT_EQ(resultVec->GetValue(1), 0);   // [400,500] position of 300 -> 0

    delete context;
    delete input;
    delete result;
}

/// Test array_position with double type arrays
TEST_F(ArrayPositionTest, DoubleArrayPosition)
{
    int rowSize = 2;
    auto type = std::make_shared<DataType>(OMNI_DOUBLE);
    auto types = DataTypes({type});

    // Row 0: [1.1, 2.2, 3.3], Row 1: [4.4, 5.5]
    double col[] = {1.1, 2.2, 3.3, 4.4, 5.5};
    std::vector<int32_t> offsets = {0, 3, 5};
    auto input = CreateArrayVectorBatch(types, offsets, rowSize, 5, col);

    auto expr = FuncExpr("array_position", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new LiteralExpr(2.2, type)
    }, std::make_shared<DataType>(OMNI_LONG));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultVec = dynamic_cast<Vector<int64_t> *>(result);
    ASSERT_NE(resultVec, nullptr);

    EXPECT_EQ(resultVec->GetValue(0), 2);   // [1.1,2.2,3.3] position of 2.2 -> 2
    EXPECT_EQ(resultVec->GetValue(1), 0);   // [4.4,5.5] position of 2.2 -> 0

    delete context;
    delete input;
    delete result;
}

/// Test array_position with double NaN (NaN == NaN in Spark semantics)
TEST_F(ArrayPositionTest, DoubleNaNPosition)
{
    int rowSize = 1;
    double nan = std::numeric_limits<double>::quiet_NaN();

    auto *elementVec = new Vector<double>(3);
    elementVec->SetValue(0, 1.0);
    elementVec->SetValue(1, nan);
    elementVec->SetValue(2, 3.0);

    auto elementShared = std::shared_ptr<BaseVector>(elementVec);
    auto *arrayVec = new ArrayVector(rowSize, elementShared);
    arrayVec->SetOffset(0, 0);
    arrayVec->SetOffset(1, 3);

    auto *input = new VectorBatch(rowSize);
    input->Append(arrayVec);

    auto type = std::make_shared<DataType>(OMNI_DOUBLE);

    auto expr = FuncExpr("array_position", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new LiteralExpr(nan, type)
    }, std::make_shared<DataType>(OMNI_LONG));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultVec = dynamic_cast<Vector<int64_t> *>(result);
    ASSERT_NE(resultVec, nullptr);

    EXPECT_EQ(resultVec->GetValue(0), 2);  // [1.0, NaN, 3.0] position of NaN -> 2

    delete context;
    delete input;
    delete result;
}

/// Test array_position with boolean type arrays
TEST_F(ArrayPositionTest, BooleanArrayPosition)
{
    int rowSize = 2;
    auto type = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto types = DataTypes({type});

    // Row 0: [true, false], Row 1: [false, false]
    bool col[] = {true, false, false, false};
    std::vector<int32_t> offsets = {0, 2, 4};
    auto input = CreateArrayVectorBatch(types, offsets, rowSize, 4, col);

    auto expr = FuncExpr("array_position", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new LiteralExpr(true, type)
    }, std::make_shared<DataType>(OMNI_LONG));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultVec = dynamic_cast<Vector<int64_t> *>(result);
    ASSERT_NE(resultVec, nullptr);

    EXPECT_EQ(resultVec->GetValue(0), 1);   // [true,false] position of true -> 1
    EXPECT_EQ(resultVec->GetValue(1), 0);   // [false,false] position of true -> 0

    delete context;
    delete input;
    delete result;
}

/// Test array_position with first element match
TEST_F(ArrayPositionTest, FirstElementMatch)
{
    int rowSize = 1;
    auto type = std::make_shared<DataType>(OMNI_INT);
    auto types = DataTypes({type});

    // Row 0: [42, 100, 200]
    int32_t col[] = {42, 100, 200};
    std::vector<int32_t> offsets = {0, 3};
    auto input = CreateArrayVectorBatch(types, offsets, rowSize, 3, col);

    auto expr = FuncExpr("array_position", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new LiteralExpr(static_cast<int32_t>(42), type)
    }, std::make_shared<DataType>(OMNI_LONG));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultVec = dynamic_cast<Vector<int64_t> *>(result);
    ASSERT_NE(resultVec, nullptr);

    EXPECT_EQ(resultVec->GetValue(0), 1);  // [42,100,200] position of 42 -> 1

    delete context;
    delete input;
    delete result;
}

/// Test array_position with last element match
TEST_F(ArrayPositionTest, LastElementMatch)
{
    int rowSize = 1;
    auto type = std::make_shared<DataType>(OMNI_INT);
    auto types = DataTypes({type});

    // Row 0: [10, 20, 30]
    int32_t col[] = {10, 20, 30};
    std::vector<int32_t> offsets = {0, 3};
    auto input = CreateArrayVectorBatch(types, offsets, rowSize, 3, col);

    auto expr = FuncExpr("array_position", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new LiteralExpr(static_cast<int32_t>(30), type)
    }, std::make_shared<DataType>(OMNI_LONG));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultVec = dynamic_cast<Vector<int64_t> *>(result);
    ASSERT_NE(resultVec, nullptr);

    EXPECT_EQ(resultVec->GetValue(0), 3);  // [10,20,30] position of 30 -> 3

    delete context;
    delete input;
    delete result;
}

/// Test array_position with duplicate values (returns first occurrence)
TEST_F(ArrayPositionTest, DuplicateValuesReturnsFirstOccurrence)
{
    int rowSize = 1;
    auto type = std::make_shared<DataType>(OMNI_INT);
    auto types = DataTypes({type});

    // Row 0: [5, 3, 5, 3, 5]
    int32_t col[] = {5, 3, 5, 3, 5};
    std::vector<int32_t> offsets = {0, 5};
    auto input = CreateArrayVectorBatch(types, offsets, rowSize, 5, col);

    auto expr = FuncExpr("array_position", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new LiteralExpr(static_cast<int32_t>(3), type)
    }, std::make_shared<DataType>(OMNI_LONG));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultVec = dynamic_cast<Vector<int64_t> *>(result);
    ASSERT_NE(resultVec, nullptr);

    EXPECT_EQ(resultVec->GetValue(0), 2);  // [5,3,5,3,5] first position of 3 -> 2

    delete context;
    delete input;
    delete result;
}

/// Test array_position with boundary values
TEST_F(ArrayPositionTest, BoundaryValues)
{
    int rowSize = 1;
    auto type = std::make_shared<DataType>(OMNI_INT);
    auto types = DataTypes({type});

    constexpr int32_t kMin = std::numeric_limits<int32_t>::min();
    constexpr int32_t kMax = std::numeric_limits<int32_t>::max();

    // Row 0: [kMin, 0, kMax]
    int32_t col[] = {kMin, 0, kMax};
    std::vector<int32_t> offsets = {0, 3};
    auto input = CreateArrayVectorBatch(types, offsets, rowSize, 3, col);

    auto expr = FuncExpr("array_position", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new LiteralExpr(kMax, type)
    }, std::make_shared<DataType>(OMNI_LONG));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultVec = dynamic_cast<Vector<int64_t> *>(result);
    ASSERT_NE(resultVec, nullptr);

    EXPECT_EQ(resultVec->GetValue(0), 3);  // [kMin, 0, kMax] position of kMax -> 3

    delete context;
    delete input;
    delete result;
}

/// Test array_position with negative numbers
TEST_F(ArrayPositionTest, NegativeNumbers)
{
    int rowSize = 2;
    auto type = std::make_shared<DataType>(OMNI_INT);
    auto types = DataTypes({type});

    // Row 0: [-5, -3, -1], Row 1: [-100, -50]
    int32_t col[] = {-5, -3, -1, -100, -50};
    std::vector<int32_t> offsets = {0, 3, 5};
    auto input = CreateArrayVectorBatch(types, offsets, rowSize, 5, col);

    auto expr = FuncExpr("array_position", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new LiteralExpr(static_cast<int32_t>(-3), type)
    }, std::make_shared<DataType>(OMNI_LONG));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultVec = dynamic_cast<Vector<int64_t> *>(result);
    ASSERT_NE(resultVec, nullptr);

    EXPECT_EQ(resultVec->GetValue(0), 2);   // [-5,-3,-1] position of -3 -> 2
    EXPECT_EQ(resultVec->GetValue(1), 0);   // [-100,-50] position of -3 -> 0

    delete context;
    delete input;
    delete result;
}

/// Test array_position with float type arrays
TEST_F(ArrayPositionTest, FloatArrayPosition)
{
    int rowSize = 2;
    auto type = std::make_shared<DataType>(OMNI_FLOAT);
    auto types = DataTypes({type});

    // Row 0: [1.5f, 2.5f, 3.5f], Row 1: [0.1f, 0.2f]
    float col[] = {1.5f, 2.5f, 3.5f, 0.1f, 0.2f};
    std::vector<int32_t> offsets = {0, 3, 5};
    auto input = CreateArrayVectorBatch(types, offsets, rowSize, 5, col);

    auto expr = FuncExpr("array_position", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new LiteralExpr(2.5f, type)
    }, std::make_shared<DataType>(OMNI_LONG));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultVec = dynamic_cast<Vector<int64_t> *>(result);
    ASSERT_NE(resultVec, nullptr);

    EXPECT_EQ(resultVec->GetValue(0), 2);  // [1.5,2.5,3.5] position of 2.5 -> 2
    EXPECT_EQ(resultVec->GetValue(1), 0);  // [0.1,0.2] position of 2.5 -> 0

    delete context;
    delete input;
    delete result;
}

/// Test array_position with byte type arrays
TEST_F(ArrayPositionTest, ByteArrayPosition)
{
    int rowSize = 2;
    auto type = std::make_shared<DataType>(OMNI_BYTE);
    auto types = DataTypes({type});

    // Row 0: [10, 20, 30], Row 1: [5, 15]
    int8_t col[] = {10, 20, 30, 5, 15};
    std::vector<int32_t> offsets = {0, 3, 5};
    auto input = CreateArrayVectorBatch(types, offsets, rowSize, 5, col);

    auto expr = FuncExpr("array_position", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new LiteralExpr(static_cast<int8_t>(20), type)
    }, std::make_shared<DataType>(OMNI_LONG));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultVec = dynamic_cast<Vector<int64_t> *>(result);
    ASSERT_NE(resultVec, nullptr);

    EXPECT_EQ(resultVec->GetValue(0), 2);  // [10,20,30] position of 20 -> 2
    EXPECT_EQ(resultVec->GetValue(1), 0);  // [5,15] position of 20 -> 0

    delete context;
    delete input;
    delete result;
}

/// Test array_position with short type arrays
TEST_F(ArrayPositionTest, ShortArrayPosition)
{
    int rowSize = 2;
    auto type = std::make_shared<DataType>(OMNI_SHORT);
    auto types = DataTypes({type});

    // Row 0: [1000, 2000, 3000], Row 1: [500, 1500]
    int16_t col[] = {1000, 2000, 3000, 500, 1500};
    std::vector<int32_t> offsets = {0, 3, 5};
    auto input = CreateArrayVectorBatch(types, offsets, rowSize, 5, col);

    auto expr = FuncExpr("array_position", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new LiteralExpr(static_cast<int16_t>(2000), type)
    }, std::make_shared<DataType>(OMNI_LONG));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultVec = dynamic_cast<Vector<int64_t> *>(result);
    ASSERT_NE(resultVec, nullptr);

    EXPECT_EQ(resultVec->GetValue(0), 2);  // [1000,2000,3000] position of 2000 -> 2
    EXPECT_EQ(resultVec->GetValue(1), 0);  // [500,1500] position of 2000 -> 0

    delete context;
    delete input;
    delete result;
}
