/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Unit tests for array_union function
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

class ArrayUnionTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}

    VectorBatch *CreateTwoArrayBatch(int32_t rowSize,
        std::vector<int32_t> &leftOffsets, int32_t leftElemCount, int32_t *leftValues,
        std::vector<int32_t> &rightOffsets, int32_t rightElemCount, int32_t *rightValues) const
    {
        auto *leftElemVec = new Vector<int32_t>(leftElemCount);
        for (int32_t i = 0; i < leftElemCount; ++i) {
            leftElemVec->SetValue(i, leftValues[i]);
        }
        auto leftElemShared = std::shared_ptr<BaseVector>(leftElemVec);
        auto *leftArrayVec = new ArrayVector(rowSize, leftElemShared);
        for (size_t j = 0; j < leftOffsets.size(); ++j) {
            leftArrayVec->SetOffset(j, leftOffsets[j]);
        }

        auto *rightElemVec = new Vector<int32_t>(rightElemCount);
        for (int32_t i = 0; i < rightElemCount; ++i) {
            rightElemVec->SetValue(i, rightValues[i]);
        }
        auto rightElemShared = std::shared_ptr<BaseVector>(rightElemVec);
        auto *rightArrayVec = new ArrayVector(rowSize, rightElemShared);
        for (size_t j = 0; j < rightOffsets.size(); ++j) {
            rightArrayVec->SetOffset(j, rightOffsets[j]);
        }

        auto *vectorBatch = new VectorBatch(rowSize);
        vectorBatch->Append(leftArrayVec);
        vectorBatch->Append(rightArrayVec);
        return vectorBatch;
    }

    VectorBatch *CreateTwoArrayBatchWithNulls(int32_t rowSize,
        std::vector<int32_t> &leftOffsets, int32_t leftElemCount, int32_t *leftValues,
        std::vector<int32_t> &leftNullIndices,
        std::vector<int32_t> &rightOffsets, int32_t rightElemCount, int32_t *rightValues,
        std::vector<int32_t> &rightNullIndices) const
    {
        auto *leftElemVec = new Vector<int32_t>(leftElemCount);
        for (int32_t i = 0; i < leftElemCount; ++i) {
            leftElemVec->SetValue(i, leftValues[i]);
        }
        for (int32_t idx : leftNullIndices) {
            leftElemVec->SetNull(idx);
        }
        auto leftElemShared = std::shared_ptr<BaseVector>(leftElemVec);
        auto *leftArrayVec = new ArrayVector(rowSize, leftElemShared);
        for (size_t j = 0; j < leftOffsets.size(); ++j) {
            leftArrayVec->SetOffset(j, leftOffsets[j]);
        }

        auto *rightElemVec = new Vector<int32_t>(rightElemCount);
        for (int32_t i = 0; i < rightElemCount; ++i) {
            rightElemVec->SetValue(i, rightValues[i]);
        }
        for (int32_t idx : rightNullIndices) {
            rightElemVec->SetNull(idx);
        }
        auto rightElemShared = std::shared_ptr<BaseVector>(rightElemVec);
        auto *rightArrayVec = new ArrayVector(rowSize, rightElemShared);
        for (size_t j = 0; j < rightOffsets.size(); ++j) {
            rightArrayVec->SetOffset(j, rightOffsets[j]);
        }

        auto *vectorBatch = new VectorBatch(rowSize);
        vectorBatch->Append(leftArrayVec);
        vectorBatch->Append(rightArrayVec);
        return vectorBatch;
    }
};

/// Test array_union basic: union of two integer arrays
TEST_F(ArrayUnionTest, BasicIntegerUnion)
{
    int rowSize = 2;
    // Row 0: [1,2,3] ∪ [2,3,4] -> [1,2,3,4]
    // Row 1: [5,6]   ∪ [7,8]   -> [5,6,7,8]
    int32_t leftValues[] = {1, 2, 3, 5, 6};
    std::vector<int32_t> leftOffsets = {0, 3, 5};
    int32_t rightValues[] = {2, 3, 4, 7, 8};
    std::vector<int32_t> rightOffsets = {0, 3, 5};

    auto input = CreateTwoArrayBatch(rowSize, leftOffsets, 5, leftValues,
        rightOffsets, 5, rightValues);

    auto expr = FuncExpr("array_union", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_ARRAY))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);

    // Row 0: [1, 2, 3, 4]
    EXPECT_EQ(resultArray->GetSize(0), 4);
    auto resultElem = dynamic_cast<Vector<int32_t> *>(resultArray->GetElementVector().get());
    ASSERT_NE(resultElem, nullptr);
    int64_t off0 = resultArray->GetOffset(0);
    EXPECT_EQ(resultElem->GetValue(off0), 1);
    EXPECT_EQ(resultElem->GetValue(off0 + 1), 2);
    EXPECT_EQ(resultElem->GetValue(off0 + 2), 3);
    EXPECT_EQ(resultElem->GetValue(off0 + 3), 4);

    // Row 1: [5, 6, 7, 8]
    EXPECT_EQ(resultArray->GetSize(1), 4);
    int64_t off1 = resultArray->GetOffset(1);
    EXPECT_EQ(resultElem->GetValue(off1), 5);
    EXPECT_EQ(resultElem->GetValue(off1 + 1), 6);
    EXPECT_EQ(resultElem->GetValue(off1 + 2), 7);
    EXPECT_EQ(resultElem->GetValue(off1 + 3), 8);

    delete context;
    delete input;
    delete result;
}

/// Test array_union with complete overlap (deduplication)
TEST_F(ArrayUnionTest, CompleteOverlap)
{
    int rowSize = 1;
    // [1,2,3] ∪ [1,2,3] -> [1,2,3]
    int32_t leftValues[] = {1, 2, 3};
    std::vector<int32_t> leftOffsets = {0, 3};
    int32_t rightValues[] = {1, 2, 3};
    std::vector<int32_t> rightOffsets = {0, 3};

    auto input = CreateTwoArrayBatch(rowSize, leftOffsets, 3, leftValues,
        rightOffsets, 3, rightValues);

    auto expr = FuncExpr("array_union", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_ARRAY))
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
    int64_t off = resultArray->GetOffset(0);
    EXPECT_EQ(resultElem->GetValue(off), 1);
    EXPECT_EQ(resultElem->GetValue(off + 1), 2);
    EXPECT_EQ(resultElem->GetValue(off + 2), 3);

    delete context;
    delete input;
    delete result;
}

/// Test array_union with no overlap
TEST_F(ArrayUnionTest, NoOverlap)
{
    int rowSize = 1;
    // [1,2] ∪ [3,4] -> [1,2,3,4]
    int32_t leftValues[] = {1, 2};
    std::vector<int32_t> leftOffsets = {0, 2};
    int32_t rightValues[] = {3, 4};
    std::vector<int32_t> rightOffsets = {0, 2};

    auto input = CreateTwoArrayBatch(rowSize, leftOffsets, 2, leftValues,
        rightOffsets, 2, rightValues);

    auto expr = FuncExpr("array_union", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_ARRAY))
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
    EXPECT_EQ(resultElem->GetValue(off), 1);
    EXPECT_EQ(resultElem->GetValue(off + 1), 2);
    EXPECT_EQ(resultElem->GetValue(off + 2), 3);
    EXPECT_EQ(resultElem->GetValue(off + 3), 4);

    delete context;
    delete input;
    delete result;
}

/// Test array_union with duplicates within arrays
TEST_F(ArrayUnionTest, DuplicatesWithinArrays)
{
    int rowSize = 1;
    // [1,1,2,2] ∪ [2,3,3] -> [1,2,3]
    int32_t leftValues[] = {1, 1, 2, 2};
    std::vector<int32_t> leftOffsets = {0, 4};
    int32_t rightValues[] = {2, 3, 3};
    std::vector<int32_t> rightOffsets = {0, 3};

    auto input = CreateTwoArrayBatch(rowSize, leftOffsets, 4, leftValues,
        rightOffsets, 3, rightValues);

    auto expr = FuncExpr("array_union", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_ARRAY))
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
    int64_t off = resultArray->GetOffset(0);
    EXPECT_EQ(resultElem->GetValue(off), 1);
    EXPECT_EQ(resultElem->GetValue(off + 1), 2);
    EXPECT_EQ(resultElem->GetValue(off + 2), 3);

    delete context;
    delete input;
    delete result;
}

/// Test array_union with empty arrays
TEST_F(ArrayUnionTest, EmptyArrays)
{
    int rowSize = 3;
    // Row 0: [] ∪ [1,2]    -> [1,2]
    // Row 1: [3,4] ∪ []    -> [3,4]
    // Row 2: [] ∪ []        -> []
    int32_t leftValues[] = {3, 4};
    std::vector<int32_t> leftOffsets = {0, 0, 2, 2};
    int32_t rightValues[] = {1, 2};
    std::vector<int32_t> rightOffsets = {0, 2, 2, 2};

    auto input = CreateTwoArrayBatch(rowSize, leftOffsets, 2, leftValues,
        rightOffsets, 2, rightValues);

    auto expr = FuncExpr("array_union", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_ARRAY))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);

    // Row 0: [1, 2]
    EXPECT_EQ(resultArray->GetSize(0), 2);
    auto resultElem = dynamic_cast<Vector<int32_t> *>(resultArray->GetElementVector().get());
    ASSERT_NE(resultElem, nullptr);
    int64_t off0 = resultArray->GetOffset(0);
    EXPECT_EQ(resultElem->GetValue(off0), 1);
    EXPECT_EQ(resultElem->GetValue(off0 + 1), 2);

    // Row 1: [3, 4]
    EXPECT_EQ(resultArray->GetSize(1), 2);
    int64_t off1 = resultArray->GetOffset(1);
    EXPECT_EQ(resultElem->GetValue(off1), 3);
    EXPECT_EQ(resultElem->GetValue(off1 + 1), 4);

    // Row 2: []
    EXPECT_EQ(resultArray->GetSize(2), 0);

    delete context;
    delete input;
    delete result;
}

/// Test array_union with null elements (at most one null in result)
TEST_F(ArrayUnionTest, NullElementHandling)
{
    int rowSize = 2;
    // Row 0: [1, null, 3] ∪ [null, 4] -> [1, null, 3, 4] (only one null)
    // Row 1: [null] ∪ [null]           -> [null]
    int32_t leftValues[] = {1, 0, 3, 0};
    std::vector<int32_t> leftOffsets = {0, 3, 4};
    std::vector<int32_t> leftNullIndices = {1, 3};

    int32_t rightValues[] = {0, 4, 0};
    std::vector<int32_t> rightOffsets = {0, 2, 3};
    std::vector<int32_t> rightNullIndices = {0, 2};

    auto input = CreateTwoArrayBatchWithNulls(rowSize,
        leftOffsets, 4, leftValues, leftNullIndices,
        rightOffsets, 3, rightValues, rightNullIndices);

    auto expr = FuncExpr("array_union", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_ARRAY))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);

    // Row 0: [1, null, 3, 4] - 4 elements, only 1 null
    EXPECT_EQ(resultArray->GetSize(0), 4);
    auto resultElementVec = resultArray->GetElementVector();
    auto resultElem = dynamic_cast<Vector<int32_t> *>(resultElementVec.get());
    ASSERT_NE(resultElem, nullptr);
    int64_t off0 = resultArray->GetOffset(0);
    EXPECT_EQ(resultElem->GetValue(off0), 1);
    EXPECT_TRUE(resultElementVec->IsNull(static_cast<int32_t>(off0 + 1)));
    EXPECT_EQ(resultElem->GetValue(off0 + 2), 3);
    EXPECT_EQ(resultElem->GetValue(off0 + 3), 4);

    // Row 1: [null] - only one null
    EXPECT_EQ(resultArray->GetSize(1), 1);
    int64_t off1 = resultArray->GetOffset(1);
    EXPECT_TRUE(resultElementVec->IsNull(static_cast<int32_t>(off1)));

    delete context;
    delete input;
    delete result;
}

/// Test array_union with long type arrays
TEST_F(ArrayUnionTest, LongArrayUnion)
{
    int rowSize = 1;
    // [100L, 200L] ∪ [200L, 300L] -> [100L, 200L, 300L]
    auto *leftElemVec = new Vector<int64_t>(2);
    leftElemVec->SetValue(0, 100L);
    leftElemVec->SetValue(1, 200L);
    auto leftElemShared = std::shared_ptr<BaseVector>(leftElemVec);
    auto *leftArrayVec = new ArrayVector(rowSize, leftElemShared);
    leftArrayVec->SetOffset(0, 0);
    leftArrayVec->SetOffset(1, 2);

    auto *rightElemVec = new Vector<int64_t>(2);
    rightElemVec->SetValue(0, 200L);
    rightElemVec->SetValue(1, 300L);
    auto rightElemShared = std::shared_ptr<BaseVector>(rightElemVec);
    auto *rightArrayVec = new ArrayVector(rowSize, rightElemShared);
    rightArrayVec->SetOffset(0, 0);
    rightArrayVec->SetOffset(1, 2);

    auto *input = new VectorBatch(rowSize);
    input->Append(leftArrayVec);
    input->Append(rightArrayVec);

    auto expr = FuncExpr("array_union", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_ARRAY))
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

/// Test array_union with double type arrays
TEST_F(ArrayUnionTest, DoubleArrayUnion)
{
    int rowSize = 1;
    // [1.1, 2.2] ∪ [2.2, 3.3] -> [1.1, 2.2, 3.3]
    auto *leftElemVec = new Vector<double>(2);
    leftElemVec->SetValue(0, 1.1);
    leftElemVec->SetValue(1, 2.2);
    auto leftElemShared = std::shared_ptr<BaseVector>(leftElemVec);
    auto *leftArrayVec = new ArrayVector(rowSize, leftElemShared);
    leftArrayVec->SetOffset(0, 0);
    leftArrayVec->SetOffset(1, 2);

    auto *rightElemVec = new Vector<double>(2);
    rightElemVec->SetValue(0, 2.2);
    rightElemVec->SetValue(1, 3.3);
    auto rightElemShared = std::shared_ptr<BaseVector>(rightElemVec);
    auto *rightArrayVec = new ArrayVector(rowSize, rightElemShared);
    rightArrayVec->SetOffset(0, 0);
    rightArrayVec->SetOffset(1, 2);

    auto *input = new VectorBatch(rowSize);
    input->Append(leftArrayVec);
    input->Append(rightArrayVec);

    auto expr = FuncExpr("array_union", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_ARRAY))
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

/// Test array_union with boolean type arrays
TEST_F(ArrayUnionTest, BooleanArrayUnion)
{
    int rowSize = 1;
    // [true] ∪ [false] -> [true, false]
    auto *leftElemVec = new Vector<bool>(1);
    leftElemVec->SetValue(0, true);
    auto leftElemShared = std::shared_ptr<BaseVector>(leftElemVec);
    auto *leftArrayVec = new ArrayVector(rowSize, leftElemShared);
    leftArrayVec->SetOffset(0, 0);
    leftArrayVec->SetOffset(1, 1);

    auto *rightElemVec = new Vector<bool>(1);
    rightElemVec->SetValue(0, false);
    auto rightElemShared = std::shared_ptr<BaseVector>(rightElemVec);
    auto *rightArrayVec = new ArrayVector(rowSize, rightElemShared);
    rightArrayVec->SetOffset(0, 0);
    rightArrayVec->SetOffset(1, 1);

    auto *input = new VectorBatch(rowSize);
    input->Append(leftArrayVec);
    input->Append(rightArrayVec);

    auto expr = FuncExpr("array_union", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_ARRAY))
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

/// Test array_union with multiple rows
TEST_F(ArrayUnionTest, MultipleRows)
{
    int rowSize = 3;
    // Row 0: [1,2] ∪ [3,4]     -> [1,2,3,4]
    // Row 1: [5,6] ∪ [5,6]     -> [5,6]
    // Row 2: [7]   ∪ [8,9,10]  -> [7,8,9,10]
    int32_t leftValues[] = {1, 2, 5, 6, 7};
    std::vector<int32_t> leftOffsets = {0, 2, 4, 5};
    int32_t rightValues[] = {3, 4, 5, 6, 8, 9, 10};
    std::vector<int32_t> rightOffsets = {0, 2, 4, 7};

    auto input = CreateTwoArrayBatch(rowSize, leftOffsets, 5, leftValues,
        rightOffsets, 7, rightValues);

    auto expr = FuncExpr("array_union", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_ARRAY))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);

    auto resultElem = dynamic_cast<Vector<int32_t> *>(resultArray->GetElementVector().get());
    ASSERT_NE(resultElem, nullptr);

    // Row 0: [1,2,3,4]
    EXPECT_EQ(resultArray->GetSize(0), 4);
    int64_t off0 = resultArray->GetOffset(0);
    EXPECT_EQ(resultElem->GetValue(off0), 1);
    EXPECT_EQ(resultElem->GetValue(off0 + 1), 2);
    EXPECT_EQ(resultElem->GetValue(off0 + 2), 3);
    EXPECT_EQ(resultElem->GetValue(off0 + 3), 4);

    // Row 1: [5,6]
    EXPECT_EQ(resultArray->GetSize(1), 2);
    int64_t off1 = resultArray->GetOffset(1);
    EXPECT_EQ(resultElem->GetValue(off1), 5);
    EXPECT_EQ(resultElem->GetValue(off1 + 1), 6);

    // Row 2: [7,8,9,10]
    EXPECT_EQ(resultArray->GetSize(2), 4);
    int64_t off2 = resultArray->GetOffset(2);
    EXPECT_EQ(resultElem->GetValue(off2), 7);
    EXPECT_EQ(resultElem->GetValue(off2 + 1), 8);
    EXPECT_EQ(resultElem->GetValue(off2 + 2), 9);
    EXPECT_EQ(resultElem->GetValue(off2 + 3), 10);

    delete context;
    delete input;
    delete result;
}

/// Test array_union with negative numbers
TEST_F(ArrayUnionTest, NegativeNumbers)
{
    int rowSize = 1;
    // [-1,-2,-3] ∪ [-2,-4] -> [-1,-2,-3,-4]
    int32_t leftValues[] = {-1, -2, -3};
    std::vector<int32_t> leftOffsets = {0, 3};
    int32_t rightValues[] = {-2, -4};
    std::vector<int32_t> rightOffsets = {0, 2};

    auto input = CreateTwoArrayBatch(rowSize, leftOffsets, 3, leftValues,
        rightOffsets, 2, rightValues);

    auto expr = FuncExpr("array_union", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_ARRAY))
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
    EXPECT_EQ(resultElem->GetValue(off + 2), -3);
    EXPECT_EQ(resultElem->GetValue(off + 3), -4);

    delete context;
    delete input;
    delete result;
}
