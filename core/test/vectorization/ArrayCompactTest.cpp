/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Unit tests for array_compact function
 */

#include <gtest/gtest.h>
#include <string>
#include <vector>

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

class ArrayCompactTest : public ::testing::Test {
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

/// Test array_compact with integer arrays containing nulls
TEST_F(ArrayCompactTest, IntegerArrayWithNulls)
{
    int rowSize = 2;
    // Row 0: [1, null, 3]  -> expected: [1, 3]
    // Row 1: [null, 5]     -> expected: [5]
    int32_t values[] = {1, 0, 3, 0, 5};
    std::vector<int32_t> offsets = {0, 3, 5};
    std::vector<int32_t> nullIndices = {1, 3};
    auto input = CreateArrayWithNulls(rowSize, offsets, 5, values, nullIndices);

    auto expr = FuncExpr("array_compact", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);

    // Row 0: [1, 3] - 2 elements
    EXPECT_EQ(resultArray->GetSize(0), 2);
    auto resultElementVec = dynamic_cast<Vector<int32_t> *>(resultArray->GetElementVector().get());
    ASSERT_NE(resultElementVec, nullptr);

    int64_t offset0 = resultArray->GetOffset(0);
    EXPECT_EQ(resultElementVec->GetValue(offset0), 1);
    EXPECT_EQ(resultElementVec->GetValue(offset0 + 1), 3);

    // Row 1: [5] - 1 element
    EXPECT_EQ(resultArray->GetSize(1), 1);
    int64_t offset1 = resultArray->GetOffset(1);
    EXPECT_EQ(resultElementVec->GetValue(offset1), 5);

    delete context;
    delete input;
    delete result;
}

/// Test array_compact with no null elements (should return same content)
TEST_F(ArrayCompactTest, NoNullElements)
{
    int rowSize = 2;
    auto type = std::make_shared<DataType>(OMNI_INT);
    auto types = DataTypes({type});

    // Row 0: [1, 2, 3] -> expected: [1, 2, 3]
    // Row 1: [4, 5]    -> expected: [4, 5]
    int32_t col[] = {1, 2, 3, 4, 5};
    std::vector<int32_t> offsets = {0, 3, 5};
    auto input = CreateArrayVectorBatch(types, offsets, rowSize, 5, col);

    auto expr = FuncExpr("array_compact", {
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
    EXPECT_EQ(resultArray->GetSize(1), 2);

    auto resultElementVec = dynamic_cast<Vector<int32_t> *>(resultArray->GetElementVector().get());
    ASSERT_NE(resultElementVec, nullptr);

    int64_t off0 = resultArray->GetOffset(0);
    EXPECT_EQ(resultElementVec->GetValue(off0), 1);
    EXPECT_EQ(resultElementVec->GetValue(off0 + 1), 2);
    EXPECT_EQ(resultElementVec->GetValue(off0 + 2), 3);

    int64_t off1 = resultArray->GetOffset(1);
    EXPECT_EQ(resultElementVec->GetValue(off1), 4);
    EXPECT_EQ(resultElementVec->GetValue(off1 + 1), 5);

    delete context;
    delete input;
    delete result;
}

/// Test array_compact with all null elements
TEST_F(ArrayCompactTest, AllNullElements)
{
    int rowSize = 1;
    // Row 0: [null, null, null] -> expected: []
    int32_t values[] = {0, 0, 0};
    std::vector<int32_t> offsets = {0, 3};
    std::vector<int32_t> nullIndices = {0, 1, 2};
    auto input = CreateArrayWithNulls(rowSize, offsets, 3, values, nullIndices);

    auto expr = FuncExpr("array_compact", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);

    // Row 0: [] - empty array
    EXPECT_EQ(resultArray->GetSize(0), 0);

    delete context;
    delete input;
    delete result;
}

/// Test array_compact with empty arrays
TEST_F(ArrayCompactTest, EmptyArray)
{
    int rowSize = 2;
    auto type = std::make_shared<DataType>(OMNI_INT);
    auto types = DataTypes({type});

    // Row 0: [] -> expected: []
    // Row 1: [1] -> expected: [1]
    int32_t col[] = {1};
    std::vector<int32_t> offsets = {0, 0, 1};
    auto input = CreateArrayVectorBatch(types, offsets, rowSize, 1, col);

    auto expr = FuncExpr("array_compact", {
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

    auto resultElementVec = dynamic_cast<Vector<int32_t> *>(resultArray->GetElementVector().get());
    ASSERT_NE(resultElementVec, nullptr);
    int64_t off1 = resultArray->GetOffset(1);
    EXPECT_EQ(resultElementVec->GetValue(off1), 1);

    delete context;
    delete input;
    delete result;
}

/// Test array_compact with long arrays containing nulls
TEST_F(ArrayCompactTest, LongArrayWithNulls)
{
    int rowSize = 2;
    // Row 0: [100L, null, 300L] -> expected: [100L, 300L]
    // Row 1: [null, null]       -> expected: []
    auto *elementVec = new Vector<int64_t>(5);
    elementVec->SetValue(0, 100L);
    elementVec->SetNull(1);
    elementVec->SetValue(2, 300L);
    elementVec->SetNull(3);
    elementVec->SetNull(4);

    auto elementShared = std::shared_ptr<BaseVector>(elementVec);
    auto *arrayVec = new ArrayVector(rowSize, elementShared);
    arrayVec->SetOffset(0, 0);
    arrayVec->SetOffset(1, 3);
    arrayVec->SetOffset(2, 5);

    auto *input = new VectorBatch(rowSize);
    input->Append(arrayVec);

    auto expr = FuncExpr("array_compact", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);

    // Row 0: [100, 300]
    EXPECT_EQ(resultArray->GetSize(0), 2);
    auto resultElem = dynamic_cast<Vector<int64_t> *>(resultArray->GetElementVector().get());
    ASSERT_NE(resultElem, nullptr);
    EXPECT_EQ(resultElem->GetValue(resultArray->GetOffset(0)), 100L);
    EXPECT_EQ(resultElem->GetValue(resultArray->GetOffset(0) + 1), 300L);

    // Row 1: []
    EXPECT_EQ(resultArray->GetSize(1), 0);

    delete context;
    delete input;
    delete result;
}

/// Test array_compact with double arrays containing nulls
TEST_F(ArrayCompactTest, DoubleArrayWithNulls)
{
    int rowSize = 1;
    // Row 0: [1.1, null, 3.3, null, 5.5] -> expected: [1.1, 3.3, 5.5]
    auto *elementVec = new Vector<double>(5);
    elementVec->SetValue(0, 1.1);
    elementVec->SetNull(1);
    elementVec->SetValue(2, 3.3);
    elementVec->SetNull(3);
    elementVec->SetValue(4, 5.5);

    auto elementShared = std::shared_ptr<BaseVector>(elementVec);
    auto *arrayVec = new ArrayVector(rowSize, elementShared);
    arrayVec->SetOffset(0, 0);
    arrayVec->SetOffset(1, 5);

    auto *input = new VectorBatch(rowSize);
    input->Append(arrayVec);

    auto expr = FuncExpr("array_compact", {
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
    EXPECT_DOUBLE_EQ(resultElem->GetValue(off + 1), 3.3);
    EXPECT_DOUBLE_EQ(resultElem->GetValue(off + 2), 5.5);

    delete context;
    delete input;
    delete result;
}

/// Test array_compact with multiple rows of mixed content
TEST_F(ArrayCompactTest, MultipleRowsMixed)
{
    int rowSize = 3;
    // Row 0: [1, null, 3]       -> expected: [1, 3]
    // Row 1: [4, 5, 6]          -> expected: [4, 5, 6]
    // Row 2: [null, null, null]  -> expected: []
    int32_t values[] = {1, 0, 3, 4, 5, 6, 0, 0, 0};
    std::vector<int32_t> offsets = {0, 3, 6, 9};
    std::vector<int32_t> nullIndices = {1, 6, 7, 8};
    auto input = CreateArrayWithNulls(rowSize, offsets, 9, values, nullIndices);

    auto expr = FuncExpr("array_compact", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);

    // Row 0: [1, 3]
    EXPECT_EQ(resultArray->GetSize(0), 2);
    // Row 1: [4, 5, 6]
    EXPECT_EQ(resultArray->GetSize(1), 3);
    // Row 2: []
    EXPECT_EQ(resultArray->GetSize(2), 0);

    auto resultElem = dynamic_cast<Vector<int32_t> *>(resultArray->GetElementVector().get());
    ASSERT_NE(resultElem, nullptr);

    // Verify Row 0
    int64_t off0 = resultArray->GetOffset(0);
    EXPECT_EQ(resultElem->GetValue(off0), 1);
    EXPECT_EQ(resultElem->GetValue(off0 + 1), 3);

    // Verify Row 1
    int64_t off1 = resultArray->GetOffset(1);
    EXPECT_EQ(resultElem->GetValue(off1), 4);
    EXPECT_EQ(resultElem->GetValue(off1 + 1), 5);
    EXPECT_EQ(resultElem->GetValue(off1 + 2), 6);

    delete context;
    delete input;
    delete result;
}

/// Test array_compact with boolean arrays
TEST_F(ArrayCompactTest, BooleanArrayWithNulls)
{
    int rowSize = 1;
    // Row 0: [true, null, false, null] -> expected: [true, false]
    auto *elementVec = new Vector<bool>(4);
    elementVec->SetValue(0, true);
    elementVec->SetNull(1);
    elementVec->SetValue(2, false);
    elementVec->SetNull(3);

    auto elementShared = std::shared_ptr<BaseVector>(elementVec);
    auto *arrayVec = new ArrayVector(rowSize, elementShared);
    arrayVec->SetOffset(0, 0);
    arrayVec->SetOffset(1, 4);

    auto *input = new VectorBatch(rowSize);
    input->Append(arrayVec);

    auto expr = FuncExpr("array_compact", {
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

/// array_compact on array<array<int>> removes only null outer elements (Velox/Spark semantics).
TEST_F(ArrayCompactTest, NestedArrayOfIntRemovesNullOuterElements)
{
    using omniruntime::type::ArrayType;
    const int rowSize = 1;

    auto *intVec = new Vector<int32_t>(7);
    intVec->SetValue(0, 1);
    intVec->SetValue(1, 2);
    intVec->SetValue(2, 3);
    intVec->SetNull(3);
    intVec->SetValue(4, 4);
    intVec->SetNull(5);
    intVec->SetValue(6, 6);

    auto intShared = std::shared_ptr<BaseVector>(intVec);
    auto *middle = new ArrayVector(3, intShared);
    middle->SetOffset(0, 0);
    middle->SetOffset(1, 4);
    middle->SetOffset(2, 4);
    middle->SetOffset(3, 7);
    middle->SetNotNull(0);
    middle->SetNull(1);
    middle->SetNotNull(2);

    auto middleShared = std::shared_ptr<BaseVector>(middle);
    auto *outer = new ArrayVector(rowSize, middleShared);
    outer->SetOffset(0, 0);
    outer->SetOffset(1, 3);

    auto *input = new VectorBatch(rowSize);
    input->Append(outer);

    auto innerArrTy = std::make_shared<ArrayType>(omniruntime::type::IntType());
    auto outerArrTy = std::make_shared<ArrayType>(innerArrTy);

    auto expr = FuncExpr("array_compact", {
        new FieldExpr(0, outerArrTy)
    }, outerArrTy);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto *result = e.GetResult();

    auto *resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_EQ(resultArray->GetSize(0), 2);

    auto *outMiddle = dynamic_cast<ArrayVector *>(resultArray->GetElementVector().get());
    ASSERT_NE(outMiddle, nullptr);
    EXPECT_EQ(outMiddle->GetSize(0), 4);
    EXPECT_EQ(outMiddle->GetSize(1), 3);

    auto *outInts = dynamic_cast<Vector<int32_t> *>(outMiddle->GetElementVector().get());
    ASSERT_NE(outInts, nullptr);
    const int64_t m0 = outMiddle->GetOffset(0);
    EXPECT_EQ(outInts->GetValue(m0), 1);
    EXPECT_EQ(outInts->GetValue(m0 + 1), 2);
    EXPECT_EQ(outInts->GetValue(m0 + 2), 3);
    EXPECT_TRUE(outInts->IsNull(m0 + 3));

    const int64_t m1 = outMiddle->GetOffset(1);
    EXPECT_EQ(outInts->GetValue(m1), 4);
    EXPECT_TRUE(outInts->IsNull(m1 + 1));
    EXPECT_EQ(outInts->GetValue(m1 + 2), 6);

    delete context;
    delete input;
    delete result;
}
