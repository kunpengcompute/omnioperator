/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Unit tests for array_except function
 */

#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <limits>

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

class ArrayExceptTest : public ::testing::Test {
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

/// Test array_except basic: elements in left but not in right
TEST_F(ArrayExceptTest, BasicIntegerExcept)
{
    int rowSize = 2;
    // Row 0: left=[1,2,3,4], right=[2,4]  -> expected: [1,3]
    // Row 1: left=[5,6],     right=[5,6,7] -> expected: []
    int32_t leftValues[] = {1, 2, 3, 4, 5, 6};
    std::vector<int32_t> leftOffsets = {0, 4, 6};
    int32_t rightValues[] = {2, 4, 5, 6, 7};
    std::vector<int32_t> rightOffsets = {0, 2, 5};

    auto input = CreateTwoArrayBatch(rowSize, leftOffsets, 6, leftValues,
        rightOffsets, 5, rightValues);

    auto expr = FuncExpr("array_except", {
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

    // Row 0: [1, 3]
    EXPECT_EQ(resultArray->GetSize(0), 2);
    auto resultElem = dynamic_cast<Vector<int32_t> *>(resultArray->GetElementVector().get());
    ASSERT_NE(resultElem, nullptr);
    int64_t off0 = resultArray->GetOffset(0);
    EXPECT_EQ(resultElem->GetValue(off0), 1);
    EXPECT_EQ(resultElem->GetValue(off0 + 1), 3);

    // Row 1: []
    EXPECT_EQ(resultArray->GetSize(1), 0);

    delete context;
    delete input;
    delete result;
}

/// Test array_except with duplicates in left array (result should be deduplicated)
TEST_F(ArrayExceptTest, LeftDuplicatesDeduplication)
{
    int rowSize = 1;
    // left=[1,1,2,2,3], right=[2] -> expected: [1,3]
    int32_t leftValues[] = {1, 1, 2, 2, 3};
    std::vector<int32_t> leftOffsets = {0, 5};
    int32_t rightValues[] = {2};
    std::vector<int32_t> rightOffsets = {0, 1};

    auto input = CreateTwoArrayBatch(rowSize, leftOffsets, 5, leftValues,
        rightOffsets, 1, rightValues);

    auto expr = FuncExpr("array_except", {
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

    // [1, 3] deduplicated
    EXPECT_EQ(resultArray->GetSize(0), 2);
    auto resultElem = dynamic_cast<Vector<int32_t> *>(resultArray->GetElementVector().get());
    ASSERT_NE(resultElem, nullptr);
    int64_t off0 = resultArray->GetOffset(0);
    EXPECT_EQ(resultElem->GetValue(off0), 1);
    EXPECT_EQ(resultElem->GetValue(off0 + 1), 3);

    delete context;
    delete input;
    delete result;
}

/// Test array_except with null elements
TEST_F(ArrayExceptTest, NullElementHandling)
{
    int rowSize = 2;
    // Row 0: left=[1,null,3], right=[1]        -> expected: [null,3] (null not in right, kept)
    // Row 1: left=[null,5],   right=[null,5]   -> expected: [] (null in right, removed)
    int32_t leftValues[] = {1, 0, 3, 0, 5};
    std::vector<int32_t> leftOffsets = {0, 3, 5};
    std::vector<int32_t> leftNullIndices = {1, 3};

    int32_t rightValues[] = {1, 0, 5};
    std::vector<int32_t> rightOffsets = {0, 1, 3};
    std::vector<int32_t> rightNullIndices = {1};

    auto input = CreateTwoArrayBatchWithNulls(rowSize,
        leftOffsets, 5, leftValues, leftNullIndices,
        rightOffsets, 3, rightValues, rightNullIndices);

    auto expr = FuncExpr("array_except", {
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

    // Row 0: [null, 3]
    EXPECT_EQ(resultArray->GetSize(0), 2);
    auto resultElementVec = resultArray->GetElementVector();
    int64_t off0 = resultArray->GetOffset(0);
    EXPECT_TRUE(resultElementVec->IsNull(static_cast<int32_t>(off0)));
    auto resultElem = dynamic_cast<Vector<int32_t> *>(resultElementVec.get());
    EXPECT_EQ(resultElem->GetValue(static_cast<int32_t>(off0 + 1)), 3);

    // Row 1: [] (null removed by right null, 5 removed by right 5)
    EXPECT_EQ(resultArray->GetSize(1), 0);

    delete context;
    delete input;
    delete result;
}

/// Test array_except with empty arrays
TEST_F(ArrayExceptTest, EmptyArrays)
{
    int rowSize = 3;
    // Row 0: left=[], right=[1,2]       -> expected: []
    // Row 1: left=[1,2], right=[]       -> expected: [1,2]
    // Row 2: left=[], right=[]          -> expected: []
    int32_t leftValues[] = {1, 2};
    std::vector<int32_t> leftOffsets = {0, 0, 2, 2};
    int32_t rightValues[] = {1, 2};
    std::vector<int32_t> rightOffsets = {0, 2, 2, 2};

    auto input = CreateTwoArrayBatch(rowSize, leftOffsets, 2, leftValues,
        rightOffsets, 2, rightValues);

    auto expr = FuncExpr("array_except", {
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

    // Row 0: []
    EXPECT_EQ(resultArray->GetSize(0), 0);

    // Row 1: [1, 2]
    EXPECT_EQ(resultArray->GetSize(1), 2);
    auto resultElem = dynamic_cast<Vector<int32_t> *>(resultArray->GetElementVector().get());
    ASSERT_NE(resultElem, nullptr);
    int64_t off1 = resultArray->GetOffset(1);
    EXPECT_EQ(resultElem->GetValue(off1), 1);
    EXPECT_EQ(resultElem->GetValue(off1 + 1), 2);

    // Row 2: []
    EXPECT_EQ(resultArray->GetSize(2), 0);

    delete context;
    delete input;
    delete result;
}

/// Test array_except with long type arrays
TEST_F(ArrayExceptTest, LongArrayExcept)
{
    int rowSize = 1;
    // left=[100L,200L,300L], right=[200L] -> expected: [100L,300L]
    auto *leftElemVec = new Vector<int64_t>(3);
    leftElemVec->SetValue(0, 100L);
    leftElemVec->SetValue(1, 200L);
    leftElemVec->SetValue(2, 300L);
    auto leftElemShared = std::shared_ptr<BaseVector>(leftElemVec);
    auto *leftArrayVec = new ArrayVector(rowSize, leftElemShared);
    leftArrayVec->SetOffset(0, 0);
    leftArrayVec->SetOffset(1, 3);

    auto *rightElemVec = new Vector<int64_t>(1);
    rightElemVec->SetValue(0, 200L);
    auto rightElemShared = std::shared_ptr<BaseVector>(rightElemVec);
    auto *rightArrayVec = new ArrayVector(rowSize, rightElemShared);
    rightArrayVec->SetOffset(0, 0);
    rightArrayVec->SetOffset(1, 1);

    auto *input = new VectorBatch(rowSize);
    input->Append(leftArrayVec);
    input->Append(rightArrayVec);

    auto expr = FuncExpr("array_except", {
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
    auto resultElem = dynamic_cast<Vector<int64_t> *>(resultArray->GetElementVector().get());
    ASSERT_NE(resultElem, nullptr);
    int64_t off = resultArray->GetOffset(0);
    EXPECT_EQ(resultElem->GetValue(off), 100L);
    EXPECT_EQ(resultElem->GetValue(off + 1), 300L);

    delete context;
    delete input;
    delete result;
}

/// Test array_except with double type arrays
TEST_F(ArrayExceptTest, DoubleArrayExcept)
{
    int rowSize = 1;
    // left=[1.1,2.2,3.3], right=[2.2] -> expected: [1.1,3.3]
    auto *leftElemVec = new Vector<double>(3);
    leftElemVec->SetValue(0, 1.1);
    leftElemVec->SetValue(1, 2.2);
    leftElemVec->SetValue(2, 3.3);
    auto leftElemShared = std::shared_ptr<BaseVector>(leftElemVec);
    auto *leftArrayVec = new ArrayVector(rowSize, leftElemShared);
    leftArrayVec->SetOffset(0, 0);
    leftArrayVec->SetOffset(1, 3);

    auto *rightElemVec = new Vector<double>(1);
    rightElemVec->SetValue(0, 2.2);
    auto rightElemShared = std::shared_ptr<BaseVector>(rightElemVec);
    auto *rightArrayVec = new ArrayVector(rowSize, rightElemShared);
    rightArrayVec->SetOffset(0, 0);
    rightArrayVec->SetOffset(1, 1);

    auto *input = new VectorBatch(rowSize);
    input->Append(leftArrayVec);
    input->Append(rightArrayVec);

    auto expr = FuncExpr("array_except", {
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
    auto resultElem = dynamic_cast<Vector<double> *>(resultArray->GetElementVector().get());
    ASSERT_NE(resultElem, nullptr);
    int64_t off = resultArray->GetOffset(0);
    EXPECT_DOUBLE_EQ(resultElem->GetValue(off), 1.1);
    EXPECT_DOUBLE_EQ(resultElem->GetValue(off + 1), 3.3);

    delete context;
    delete input;
    delete result;
}

/// Test array_except with boolean type arrays
TEST_F(ArrayExceptTest, BooleanArrayExcept)
{
    int rowSize = 1;
    // left=[true,false,true], right=[true] -> expected: [false]
    auto *leftElemVec = new Vector<bool>(3);
    leftElemVec->SetValue(0, true);
    leftElemVec->SetValue(1, false);
    leftElemVec->SetValue(2, true);
    auto leftElemShared = std::shared_ptr<BaseVector>(leftElemVec);
    auto *leftArrayVec = new ArrayVector(rowSize, leftElemShared);
    leftArrayVec->SetOffset(0, 0);
    leftArrayVec->SetOffset(1, 3);

    auto *rightElemVec = new Vector<bool>(1);
    rightElemVec->SetValue(0, true);
    auto rightElemShared = std::shared_ptr<BaseVector>(rightElemVec);
    auto *rightArrayVec = new ArrayVector(rowSize, rightElemShared);
    rightArrayVec->SetOffset(0, 0);
    rightArrayVec->SetOffset(1, 1);

    auto *input = new VectorBatch(rowSize);
    input->Append(leftArrayVec);
    input->Append(rightArrayVec);

    auto expr = FuncExpr("array_except", {
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

    EXPECT_EQ(resultArray->GetSize(0), 1);
    auto resultElem = dynamic_cast<Vector<bool> *>(resultArray->GetElementVector().get());
    ASSERT_NE(resultElem, nullptr);
    int64_t off = resultArray->GetOffset(0);
    EXPECT_EQ(resultElem->GetValue(off), false);

    delete context;
    delete input;
    delete result;
}

/// Test array_except: no overlap between arrays
TEST_F(ArrayExceptTest, NoOverlap)
{
    int rowSize = 1;
    // left=[1,2,3], right=[4,5,6] -> expected: [1,2,3]
    int32_t leftValues[] = {1, 2, 3};
    std::vector<int32_t> leftOffsets = {0, 3};
    int32_t rightValues[] = {4, 5, 6};
    std::vector<int32_t> rightOffsets = {0, 3};

    auto input = CreateTwoArrayBatch(rowSize, leftOffsets, 3, leftValues,
        rightOffsets, 3, rightValues);

    auto expr = FuncExpr("array_except", {
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

/// Test array_except: complete overlap
TEST_F(ArrayExceptTest, CompleteOverlap)
{
    int rowSize = 1;
    // left=[1,2,3], right=[1,2,3] -> expected: []
    int32_t leftValues[] = {1, 2, 3};
    std::vector<int32_t> leftOffsets = {0, 3};
    int32_t rightValues[] = {1, 2, 3};
    std::vector<int32_t> rightOffsets = {0, 3};

    auto input = CreateTwoArrayBatch(rowSize, leftOffsets, 3, leftValues,
        rightOffsets, 3, rightValues);

    auto expr = FuncExpr("array_except", {
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

    EXPECT_EQ(resultArray->GetSize(0), 0);

    delete context;
    delete input;
    delete result;
}

/// Test array_except with multiple rows
TEST_F(ArrayExceptTest, MultipleRows)
{
    int rowSize = 3;
    // Row 0: left=[1,2,3,4], right=[1,2]   -> expected: [3,4]
    // Row 1: left=[5,6],     right=[5,6]   -> expected: []
    // Row 2: left=[7,8,9],   right=[10]    -> expected: [7,8,9]
    int32_t leftValues[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    std::vector<int32_t> leftOffsets = {0, 4, 6, 9};
    int32_t rightValues[] = {1, 2, 5, 6, 10};
    std::vector<int32_t> rightOffsets = {0, 2, 4, 5};

    auto input = CreateTwoArrayBatch(rowSize, leftOffsets, 9, leftValues,
        rightOffsets, 5, rightValues);

    auto expr = FuncExpr("array_except", {
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

    // Row 0: [3, 4]
    EXPECT_EQ(resultArray->GetSize(0), 2);
    int64_t off0 = resultArray->GetOffset(0);
    EXPECT_EQ(resultElem->GetValue(off0), 3);
    EXPECT_EQ(resultElem->GetValue(off0 + 1), 4);

    // Row 1: []
    EXPECT_EQ(resultArray->GetSize(1), 0);

    // Row 2: [7, 8, 9]
    EXPECT_EQ(resultArray->GetSize(2), 3);
    int64_t off2 = resultArray->GetOffset(2);
    EXPECT_EQ(resultElem->GetValue(off2), 7);
    EXPECT_EQ(resultElem->GetValue(off2 + 1), 8);
    EXPECT_EQ(resultElem->GetValue(off2 + 2), 9);

    delete context;
    delete input;
    delete result;
}
