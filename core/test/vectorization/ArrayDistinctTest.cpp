/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Unit tests for array_distinct function
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

class ArrayDistinctTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}

    VectorBatch *CreateArrayWithNulls(int32_t rowSize, std::vector<int32_t> &offsets,
        int32_t elementCount, int32_t *values, std::vector<int32_t> &nullIndices) const
    {
        auto *elementVec = new Vector<int32_t>(elementCount);
        for (int32_t i = 0; i < elementCount; ++i) {
            elementVec->SetValue(i, values[i]);
        }
        for (int32_t idx : nullIndices) {
            elementVec->SetNull(idx);
        }

        auto elementShared = std::shared_ptr<BaseVector>(elementVec);
        auto *arrayVec = new ArrayVector(rowSize, elementShared);
        for (size_t j = 0; j < offsets.size(); ++j) {
            arrayVec->SetOffset(j, offsets[j]);
        }

        auto *vectorBatch = new VectorBatch(rowSize);
        vectorBatch->Append(arrayVec);
        return vectorBatch;
    }
};

/// Test array_distinct with integer arrays containing duplicates
TEST_F(ArrayDistinctTest, IntegerArrayWithDuplicates)
{
    int rowSize = 3;
    // Row 0: [1, 2, 1, 3]  -> expected: [1, 2, 3]
    // Row 1: [4, 4, 4]     -> expected: [4]
    // Row 2: [5, 6]        -> expected: [5, 6]
    auto type = std::make_shared<DataType>(OMNI_INT);
    auto types = DataTypes({type});
    int32_t col[] = {1, 2, 1, 3, 4, 4, 4, 5, 6};
    std::vector<int32_t> offsets = {0, 4, 7, 9};
    auto input = CreateArrayVectorBatch(types, offsets, rowSize, 9, col);

    auto expr = FuncExpr("array_distinct", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);

    // Row 0: [1, 2, 3] - 3 unique elements
    EXPECT_EQ(resultArray->GetSize(0), 3);
    auto resultElem = dynamic_cast<Vector<int32_t> *>(resultArray->GetElementVector().get());
    ASSERT_NE(resultElem, nullptr);
    int64_t off0 = resultArray->GetOffset(0);
    EXPECT_EQ(resultElem->GetValue(off0), 1);
    EXPECT_EQ(resultElem->GetValue(off0 + 1), 2);
    EXPECT_EQ(resultElem->GetValue(off0 + 2), 3);

    // Row 1: [4] - 1 unique element
    EXPECT_EQ(resultArray->GetSize(1), 1);
    int64_t off1 = resultArray->GetOffset(1);
    EXPECT_EQ(resultElem->GetValue(off1), 4);

    // Row 2: [5, 6] - already distinct
    EXPECT_EQ(resultArray->GetSize(2), 2);
    int64_t off2 = resultArray->GetOffset(2);
    EXPECT_EQ(resultElem->GetValue(off2), 5);
    EXPECT_EQ(resultElem->GetValue(off2 + 1), 6);

    delete context;
    delete input;
    delete result;
}

/// Test array_distinct with null elements - keeps at most one null
TEST_F(ArrayDistinctTest, IntegerArrayWithNullDuplicates)
{
    int rowSize = 2;
    // Row 0: [1, null, 2, null, 1]  -> expected: [1, null, 2]
    // Row 1: [null, null, null]      -> expected: [null]
    int32_t values[] = {1, 0, 2, 0, 1, 0, 0, 0};
    std::vector<int32_t> offsets = {0, 5, 8};
    std::vector<int32_t> nullIndices = {1, 3, 5, 6, 7};
    auto input = CreateArrayWithNulls(rowSize, offsets, 8, values, nullIndices);

    auto expr = FuncExpr("array_distinct", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);

    // Row 0: [1, null, 2] - 3 elements (one null kept)
    EXPECT_EQ(resultArray->GetSize(0), 3);

    // Row 1: [null] - only one null
    EXPECT_EQ(resultArray->GetSize(1), 1);
    auto resultElementVec = resultArray->GetElementVector();
    int64_t off1 = resultArray->GetOffset(1);
    EXPECT_TRUE(resultElementVec->IsNull(static_cast<int32_t>(off1)));

    delete context;
    delete input;
    delete result;
}

/// Test array_distinct with empty arrays
TEST_F(ArrayDistinctTest, EmptyArray)
{
    int rowSize = 2;
    auto type = std::make_shared<DataType>(OMNI_INT);
    auto types = DataTypes({type});

    // Row 0: [] -> expected: []
    // Row 1: [1] -> expected: [1]
    int32_t col[] = {1};
    std::vector<int32_t> offsets = {0, 0, 1};
    auto input = CreateArrayVectorBatch(types, offsets, rowSize, 1, col);

    auto expr = FuncExpr("array_distinct", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);

    EXPECT_EQ(resultArray->GetSize(0), 0);
    EXPECT_EQ(resultArray->GetSize(1), 1);

    delete context;
    delete input;
    delete result;
}

/// Test array_distinct with already-distinct arrays (no change)
TEST_F(ArrayDistinctTest, AlreadyDistinct)
{
    int rowSize = 1;
    auto type = std::make_shared<DataType>(OMNI_INT);
    auto types = DataTypes({type});

    // Row 0: [10, 20, 30] -> expected: [10, 20, 30]
    int32_t col[] = {10, 20, 30};
    std::vector<int32_t> offsets = {0, 3};
    auto input = CreateArrayVectorBatch(types, offsets, rowSize, 3, col);

    auto expr = FuncExpr("array_distinct", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);

    EXPECT_EQ(resultArray->GetSize(0), 3);
    auto resultElem = dynamic_cast<Vector<int32_t> *>(resultArray->GetElementVector().get());
    ASSERT_NE(resultElem, nullptr);

    int64_t off0 = resultArray->GetOffset(0);
    EXPECT_EQ(resultElem->GetValue(off0), 10);
    EXPECT_EQ(resultElem->GetValue(off0 + 1), 20);
    EXPECT_EQ(resultElem->GetValue(off0 + 2), 30);

    delete context;
    delete input;
    delete result;
}

/// Test array_distinct with long type arrays
TEST_F(ArrayDistinctTest, LongArrayDistinct)
{
    int rowSize = 1;
    // Row 0: [100L, 200L, 100L, 300L, 200L] -> expected: [100L, 200L, 300L]
    auto *elementVec = new Vector<int64_t>(5);
    elementVec->SetValue(0, 100L);
    elementVec->SetValue(1, 200L);
    elementVec->SetValue(2, 100L);
    elementVec->SetValue(3, 300L);
    elementVec->SetValue(4, 200L);

    auto elementShared = std::shared_ptr<BaseVector>(elementVec);
    auto *arrayVec = new ArrayVector(rowSize, elementShared);
    arrayVec->SetOffset(0, 0);
    arrayVec->SetOffset(1, 5);

    auto *input = new VectorBatch(rowSize);
    input->Append(arrayVec);

    auto expr = FuncExpr("array_distinct", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);

    EXPECT_EQ(resultArray->GetSize(0), 3);
    auto resultElem = dynamic_cast<Vector<int64_t> *>(resultArray->GetElementVector().get());
    ASSERT_NE(resultElem, nullptr);

    int64_t off = resultArray->GetOffset(0);
    EXPECT_EQ(resultElem->GetValue(off), 100L);
    EXPECT_EQ(resultElem->GetValue(off + 1), 200L);
    EXPECT_EQ(resultElem->GetValue(off + 2), 300L);

    delete context;
    delete input;
    delete result;
}

/// Test array_distinct with double type arrays
TEST_F(ArrayDistinctTest, DoubleArrayDistinct)
{
    int rowSize = 1;
    // Row 0: [1.1, 2.2, 1.1, 3.3] -> expected: [1.1, 2.2, 3.3]
    auto *elementVec = new Vector<double>(4);
    elementVec->SetValue(0, 1.1);
    elementVec->SetValue(1, 2.2);
    elementVec->SetValue(2, 1.1);
    elementVec->SetValue(3, 3.3);

    auto elementShared = std::shared_ptr<BaseVector>(elementVec);
    auto *arrayVec = new ArrayVector(rowSize, elementShared);
    arrayVec->SetOffset(0, 0);
    arrayVec->SetOffset(1, 4);

    auto *input = new VectorBatch(rowSize);
    input->Append(arrayVec);

    auto expr = FuncExpr("array_distinct", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);

    EXPECT_EQ(resultArray->GetSize(0), 3);
    auto resultElem = dynamic_cast<Vector<double> *>(resultArray->GetElementVector().get());
    ASSERT_NE(resultElem, nullptr);

    int64_t off = resultArray->GetOffset(0);
    EXPECT_DOUBLE_EQ(resultElem->GetValue(off), 1.1);
    EXPECT_DOUBLE_EQ(resultElem->GetValue(off + 1), 2.2);
    EXPECT_DOUBLE_EQ(resultElem->GetValue(off + 2), 3.3);

    delete context;
    delete input;
    delete result;
}

/// Test array_distinct with boolean type arrays
TEST_F(ArrayDistinctTest, BooleanArrayDistinct)
{
    int rowSize = 1;
    // Row 0: [true, false, true, true] -> expected: [true, false]
    auto *elementVec = new Vector<bool>(4);
    elementVec->SetValue(0, true);
    elementVec->SetValue(1, false);
    elementVec->SetValue(2, true);
    elementVec->SetValue(3, true);

    auto elementShared = std::shared_ptr<BaseVector>(elementVec);
    auto *arrayVec = new ArrayVector(rowSize, elementShared);
    arrayVec->SetOffset(0, 0);
    arrayVec->SetOffset(1, 4);

    auto *input = new VectorBatch(rowSize);
    input->Append(arrayVec);

    auto expr = FuncExpr("array_distinct", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);

    EXPECT_EQ(resultArray->GetSize(0), 2);
    auto resultElem = dynamic_cast<Vector<bool> *>(resultArray->GetElementVector().get());
    ASSERT_NE(resultElem, nullptr);

    int64_t off = resultArray->GetOffset(0);
    EXPECT_EQ(resultElem->GetValue(off), true);
    EXPECT_EQ(resultElem->GetValue(off + 1), false);

    delete context;
    delete input;
    delete result;
}

/// Test array_distinct with negative numbers and boundary values
TEST_F(ArrayDistinctTest, NegativeAndBoundaryValues)
{
    int rowSize = 1;
    auto type = std::make_shared<DataType>(OMNI_INT);
    auto types = DataTypes({type});

    // Row 0: [-1, -2, -1, INT_MAX, INT_MIN, INT_MAX] -> expected: [-1, -2, INT_MAX, INT_MIN]
    constexpr int32_t kMin = std::numeric_limits<int32_t>::min();
    constexpr int32_t kMax = std::numeric_limits<int32_t>::max();
    int32_t col[] = {-1, -2, -1, kMax, kMin, kMax};
    std::vector<int32_t> offsets = {0, 6};
    auto input = CreateArrayVectorBatch(types, offsets, rowSize, 6, col);

    auto expr = FuncExpr("array_distinct", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);

    EXPECT_EQ(resultArray->GetSize(0), 4);
    auto resultElem = dynamic_cast<Vector<int32_t> *>(resultArray->GetElementVector().get());
    ASSERT_NE(resultElem, nullptr);

    int64_t off = resultArray->GetOffset(0);
    EXPECT_EQ(resultElem->GetValue(off), -1);
    EXPECT_EQ(resultElem->GetValue(off + 1), -2);
    EXPECT_EQ(resultElem->GetValue(off + 2), kMax);
    EXPECT_EQ(resultElem->GetValue(off + 3), kMin);

    delete context;
    delete input;
    delete result;
}

/// Test array_distinct with multiple rows of mixed content
TEST_F(ArrayDistinctTest, MultipleRowsMixed)
{
    int rowSize = 3;
    auto type = std::make_shared<DataType>(OMNI_INT);
    auto types = DataTypes({type});

    // Row 0: [1, 1, 2]     -> expected: [1, 2]
    // Row 1: [3, 4, 5]     -> expected: [3, 4, 5]
    // Row 2: [6, 6, 6, 6]  -> expected: [6]
    int32_t col[] = {1, 1, 2, 3, 4, 5, 6, 6, 6, 6};
    std::vector<int32_t> offsets = {0, 3, 6, 10};
    auto input = CreateArrayVectorBatch(types, offsets, rowSize, 10, col);

    auto expr = FuncExpr("array_distinct", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);

    EXPECT_EQ(resultArray->GetSize(0), 2);
    EXPECT_EQ(resultArray->GetSize(1), 3);
    EXPECT_EQ(resultArray->GetSize(2), 1);

    auto resultElem = dynamic_cast<Vector<int32_t> *>(resultArray->GetElementVector().get());
    ASSERT_NE(resultElem, nullptr);

    // Row 0: [1, 2]
    int64_t off0 = resultArray->GetOffset(0);
    EXPECT_EQ(resultElem->GetValue(off0), 1);
    EXPECT_EQ(resultElem->GetValue(off0 + 1), 2);

    // Row 1: [3, 4, 5]
    int64_t off1 = resultArray->GetOffset(1);
    EXPECT_EQ(resultElem->GetValue(off1), 3);
    EXPECT_EQ(resultElem->GetValue(off1 + 1), 4);
    EXPECT_EQ(resultElem->GetValue(off1 + 2), 5);

    // Row 2: [6]
    int64_t off2 = resultArray->GetOffset(2);
    EXPECT_EQ(resultElem->GetValue(off2), 6);

    delete context;
    delete input;
    delete result;
}
