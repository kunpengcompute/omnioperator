/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 */

#include <gtest/gtest.h>
#include <vector>
#include <string>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/ExprEval.h"
#include "expression/expressions.h"
#include "vector/array_vector.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::mem;
using namespace omniruntime::op;
using namespace omniruntime::expressions;
using namespace omniruntime::type;
using namespace omniruntime::TestUtil;

static VectorBatch *CreateIntArrayWithElementBatch(
    int32_t rowSize,
    int32_t elemSize, int32_t *elems, std::vector<int32_t> &offsets,
    int32_t *appendElems)
{
    auto *vectorBatch = new VectorBatch(rowSize);

    auto *elemVec = new Vector<int32_t>(elemSize, OMNI_INT);
    for (int32_t i = 0; i < elemSize; ++i) {
        elemVec->SetValue(i, elems[i]);
    }
    auto *arrVec = new ArrayVector(rowSize, std::shared_ptr<BaseVector>(elemVec));
    for (size_t j = 0; j < offsets.size(); ++j) {
        arrVec->SetOffset(j, offsets[j]);
    }
    vectorBatch->Append(arrVec);

    auto *appendVec = new Vector<int32_t>(rowSize, OMNI_INT);
    for (int32_t i = 0; i < rowSize; ++i) {
        appendVec->SetValue(i, appendElems[i]);
    }
    vectorBatch->Append(appendVec);

    return vectorBatch;
}

static VectorBatch *CreateLongArrayWithElementBatch(
    int32_t rowSize,
    int32_t elemSize, int64_t *elems, std::vector<int32_t> &offsets,
    int64_t *appendElems)
{
    auto *vectorBatch = new VectorBatch(rowSize);

    auto *elemVec = new Vector<int64_t>(elemSize, OMNI_LONG);
    for (int32_t i = 0; i < elemSize; ++i) {
        elemVec->SetValue(i, elems[i]);
    }
    auto *arrVec = new ArrayVector(rowSize, std::shared_ptr<BaseVector>(elemVec));
    for (size_t j = 0; j < offsets.size(); ++j) {
        arrVec->SetOffset(j, offsets[j]);
    }
    vectorBatch->Append(arrVec);

    auto *appendVec = new Vector<int64_t>(rowSize, OMNI_LONG);
    for (int32_t i = 0; i < rowSize; ++i) {
        appendVec->SetValue(i, appendElems[i]);
    }
    vectorBatch->Append(appendVec);

    return vectorBatch;
}

static VectorBatch *CreateDoubleArrayWithElementBatch(
    int32_t rowSize,
    int32_t elemSize, double *elems, std::vector<int32_t> &offsets,
    double *appendElems)
{
    auto *vectorBatch = new VectorBatch(rowSize);

    auto *elemVec = new Vector<double>(elemSize, OMNI_DOUBLE);
    for (int32_t i = 0; i < elemSize; ++i) {
        elemVec->SetValue(i, elems[i]);
    }
    auto *arrVec = new ArrayVector(rowSize, std::shared_ptr<BaseVector>(elemVec));
    for (size_t j = 0; j < offsets.size(); ++j) {
        arrVec->SetOffset(j, offsets[j]);
    }
    vectorBatch->Append(arrVec);

    auto *appendVec = new Vector<double>(rowSize, OMNI_DOUBLE);
    for (int32_t i = 0; i < rowSize; ++i) {
        appendVec->SetValue(i, appendElems[i]);
    }
    vectorBatch->Append(appendVec);

    return vectorBatch;
}

static VectorBatch *CreateBoolArrayWithElementBatch(
    int32_t rowSize,
    int32_t elemSize, bool *elems, std::vector<int32_t> &offsets,
    bool *appendElems)
{
    auto *vectorBatch = new VectorBatch(rowSize);

    auto *elemVec = new Vector<bool>(elemSize, OMNI_BOOLEAN);
    for (int32_t i = 0; i < elemSize; ++i) {
        elemVec->SetValue(i, elems[i]);
    }
    auto *arrVec = new ArrayVector(rowSize, std::shared_ptr<BaseVector>(elemVec));
    for (size_t j = 0; j < offsets.size(); ++j) {
        arrVec->SetOffset(j, offsets[j]);
    }
    vectorBatch->Append(arrVec);

    auto *appendVec = new Vector<bool>(rowSize, OMNI_BOOLEAN);
    for (int32_t i = 0; i < rowSize; ++i) {
        appendVec->SetValue(i, appendElems[i]);
    }
    vectorBatch->Append(appendVec);

    return vectorBatch;
}

TEST(ArrayAppendTest, AppendIntElements)
{
    int32_t rowSize = 3;
    int32_t elems[] = {1, 2, 3, 4, 5};
    std::vector<int32_t> offsets = {0, 3, 4, 5};
    int32_t appendElems[] = {10, 20, 30};

    auto *input = CreateIntArrayWithElementBatch(rowSize, 5, elems, offsets, appendElems);

    auto expr = FuncExpr("array_append", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, IntType())
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto *context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto *result = e.GetResult();

    auto *resultArr = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArr, nullptr);
    ASSERT_EQ(resultArr->GetSize(), 3);

    // Row 0: [1,2,3] + 10 -> [1,2,3,10]
    ASSERT_EQ(resultArr->GetOffset(1) - resultArr->GetOffset(0), 4);
    // Row 1: [4] + 20 -> [4,20]
    ASSERT_EQ(resultArr->GetOffset(2) - resultArr->GetOffset(1), 2);
    // Row 2: [5] + 30 -> [5,30]
    ASSERT_EQ(resultArr->GetOffset(3) - resultArr->GetOffset(2), 2);

    auto *elemResult = dynamic_cast<Vector<int32_t> *>(resultArr->GetElementVector().get());
    ASSERT_NE(elemResult, nullptr);
    ASSERT_EQ(elemResult->GetSize(), 8);

    ASSERT_EQ(elemResult->GetValue(0), 1);
    ASSERT_EQ(elemResult->GetValue(1), 2);
    ASSERT_EQ(elemResult->GetValue(2), 3);
    ASSERT_EQ(elemResult->GetValue(3), 10);
    ASSERT_EQ(elemResult->GetValue(4), 4);
    ASSERT_EQ(elemResult->GetValue(5), 20);
    ASSERT_EQ(elemResult->GetValue(6), 5);
    ASSERT_EQ(elemResult->GetValue(7), 30);

    delete context;
    delete input;
    delete result;
}

TEST(ArrayAppendTest, AppendToEmptyArray)
{
    int32_t rowSize = 2;
    int32_t elems[] = {10, 20};
    std::vector<int32_t> offsets = {0, 0, 2};
    int32_t appendElems[] = {99, 88};

    auto *input = CreateIntArrayWithElementBatch(rowSize, 2, elems, offsets, appendElems);

    auto expr = FuncExpr("array_append", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, IntType())
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto *context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto *result = e.GetResult();

    auto *resultArr = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArr, nullptr);
    ASSERT_EQ(resultArr->GetSize(), 2);

    // Row 0: [] + 99 -> [99]
    ASSERT_EQ(resultArr->GetOffset(1) - resultArr->GetOffset(0), 1);
    // Row 1: [10,20] + 88 -> [10,20,88]
    ASSERT_EQ(resultArr->GetOffset(2) - resultArr->GetOffset(1), 3);

    auto *elemResult = dynamic_cast<Vector<int32_t> *>(resultArr->GetElementVector().get());
    ASSERT_NE(elemResult, nullptr);
    ASSERT_EQ(elemResult->GetValue(0), 99);
    ASSERT_EQ(elemResult->GetValue(1), 10);
    ASSERT_EQ(elemResult->GetValue(2), 20);
    ASSERT_EQ(elemResult->GetValue(3), 88);

    delete context;
    delete input;
    delete result;
}

TEST(ArrayAppendTest, NullArrayReturnsNull)
{
    int32_t rowSize = 3;
    int32_t elems[] = {5, 10, 15};
    std::vector<int32_t> offsets = {0, 2, 2, 3};
    int32_t appendElems[] = {100, 200, 300};

    auto *input = CreateIntArrayWithElementBatch(rowSize, 3, elems, offsets, appendElems);
    auto *arrVec = dynamic_cast<ArrayVector *>(input->Get(0));
    arrVec->SetNull(1);

    auto expr = FuncExpr("array_append", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, IntType())
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto *context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto *result = e.GetResult();

    auto *resultArr = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArr, nullptr);
    ASSERT_EQ(resultArr->GetSize(), 3);

    // Row 0: [5,10] + 100 -> [5,10,100]
    ASSERT_FALSE(resultArr->IsNull(0));
    ASSERT_EQ(resultArr->GetOffset(1) - resultArr->GetOffset(0), 3);

    // Row 1: null -> null
    ASSERT_TRUE(resultArr->IsNull(1));

    // Row 2: [15] + 300 -> [15,300]
    ASSERT_FALSE(resultArr->IsNull(2));
    ASSERT_EQ(resultArr->GetOffset(3) - resultArr->GetOffset(2), 2);

    auto *elemResult = dynamic_cast<Vector<int32_t> *>(resultArr->GetElementVector().get());
    ASSERT_NE(elemResult, nullptr);
    ASSERT_EQ(elemResult->GetValue(0), 5);
    ASSERT_EQ(elemResult->GetValue(1), 10);
    ASSERT_EQ(elemResult->GetValue(2), 100);

    delete context;
    delete input;
    delete result;
}

TEST(ArrayAppendTest, NullElementAppended)
{
    int32_t rowSize = 2;
    int32_t elems[] = {1, 2, 3, 4};
    std::vector<int32_t> offsets = {0, 2, 4};
    int32_t appendElems[] = {10, 20};

    auto *input = CreateIntArrayWithElementBatch(rowSize, 4, elems, offsets, appendElems);
    auto *appendVec = dynamic_cast<Vector<int32_t> *>(input->Get(1));
    appendVec->SetNull(0);

    auto expr = FuncExpr("array_append", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, IntType())
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto *context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto *result = e.GetResult();

    auto *resultArr = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArr, nullptr);
    ASSERT_EQ(resultArr->GetSize(), 2);

    // Row 0: [1,2] + null -> [1,2,null]
    ASSERT_EQ(resultArr->GetOffset(1) - resultArr->GetOffset(0), 3);
    // Row 1: [3,4] + 20 -> [3,4,20]
    ASSERT_EQ(resultArr->GetOffset(2) - resultArr->GetOffset(1), 3);

    auto *elemResult = dynamic_cast<Vector<int32_t> *>(resultArr->GetElementVector().get());
    ASSERT_NE(elemResult, nullptr);
    ASSERT_EQ(elemResult->GetValue(0), 1);
    ASSERT_EQ(elemResult->GetValue(1), 2);
    ASSERT_TRUE(elemResult->IsNull(2));
    ASSERT_EQ(elemResult->GetValue(3), 3);
    ASSERT_EQ(elemResult->GetValue(4), 4);
    ASSERT_EQ(elemResult->GetValue(5), 20);

    delete context;
    delete input;
    delete result;
}

TEST(ArrayAppendTest, NullElementsInArray)
{
    int32_t rowSize = 2;
    int32_t elems[] = {10, 0, 20, 0};
    std::vector<int32_t> offsets = {0, 2, 4};
    int32_t appendElems[] = {99, 88};

    auto *input = CreateIntArrayWithElementBatch(rowSize, 4, elems, offsets, appendElems);
    auto *arrVec = dynamic_cast<ArrayVector *>(input->Get(0));
    auto *innerElemVec = dynamic_cast<Vector<int32_t> *>(arrVec->GetElementVector().get());
    innerElemVec->SetNull(1);
    innerElemVec->SetNull(3);

    auto expr = FuncExpr("array_append", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, IntType())
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto *context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto *result = e.GetResult();

    auto *resultArr = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArr, nullptr);
    ASSERT_EQ(resultArr->GetSize(), 2);

    // Row 0: [10, null] + 99 -> [10, null, 99]
    ASSERT_EQ(resultArr->GetOffset(1) - resultArr->GetOffset(0), 3);
    // Row 1: [20, null] + 88 -> [20, null, 88]
    ASSERT_EQ(resultArr->GetOffset(2) - resultArr->GetOffset(1), 3);

    auto *elemResult = dynamic_cast<Vector<int32_t> *>(resultArr->GetElementVector().get());
    ASSERT_NE(elemResult, nullptr);
    ASSERT_EQ(elemResult->GetValue(0), 10);
    ASSERT_TRUE(elemResult->IsNull(1));
    ASSERT_EQ(elemResult->GetValue(2), 99);
    ASSERT_EQ(elemResult->GetValue(3), 20);
    ASSERT_TRUE(elemResult->IsNull(4));
    ASSERT_EQ(elemResult->GetValue(5), 88);

    delete context;
    delete input;
    delete result;
}

TEST(ArrayAppendTest, AllNullArrayRows)
{
    int32_t rowSize = 2;
    int32_t elems[] = {1};
    std::vector<int32_t> offsets = {0, 0, 1};
    int32_t appendElems[] = {10, 20};

    auto *input = CreateIntArrayWithElementBatch(rowSize, 1, elems, offsets, appendElems);
    auto *arrVec = dynamic_cast<ArrayVector *>(input->Get(0));
    arrVec->SetNull(0);
    arrVec->SetNull(1);

    auto expr = FuncExpr("array_append", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, IntType())
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto *context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto *result = e.GetResult();

    auto *resultArr = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArr, nullptr);
    ASSERT_EQ(resultArr->GetSize(), 2);

    ASSERT_TRUE(resultArr->IsNull(0));
    ASSERT_TRUE(resultArr->IsNull(1));

    delete context;
    delete input;
    delete result;
}

TEST(ArrayAppendTest, AppendLongElements)
{
    int32_t rowSize = 2;
    int64_t elems[] = {100L, 200L, 300L};
    std::vector<int32_t> offsets = {0, 2, 3};
    int64_t appendElems[] = {999L, 888L};

    auto *input = CreateLongArrayWithElementBatch(rowSize, 3, elems, offsets, appendElems);

    auto expr = FuncExpr("array_append", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, LongType())
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto *context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto *result = e.GetResult();

    auto *resultArr = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArr, nullptr);
    ASSERT_EQ(resultArr->GetSize(), 2);

    // Row 0: [100,200] + 999 -> [100,200,999]
    ASSERT_EQ(resultArr->GetOffset(1) - resultArr->GetOffset(0), 3);
    // Row 1: [300] + 888 -> [300,888]
    ASSERT_EQ(resultArr->GetOffset(2) - resultArr->GetOffset(1), 2);

    auto *elemResult = dynamic_cast<Vector<int64_t> *>(resultArr->GetElementVector().get());
    ASSERT_NE(elemResult, nullptr);
    ASSERT_EQ(elemResult->GetValue(0), 100L);
    ASSERT_EQ(elemResult->GetValue(1), 200L);
    ASSERT_EQ(elemResult->GetValue(2), 999L);
    ASSERT_EQ(elemResult->GetValue(3), 300L);
    ASSERT_EQ(elemResult->GetValue(4), 888L);

    delete context;
    delete input;
    delete result;
}

TEST(ArrayAppendTest, AppendDoubleElements)
{
    int32_t rowSize = 2;
    double elems[] = {1.1, 2.2, 3.3};
    std::vector<int32_t> offsets = {0, 2, 3};
    double appendElems[] = {9.9, 8.8};

    auto *input = CreateDoubleArrayWithElementBatch(rowSize, 3, elems, offsets, appendElems);

    auto expr = FuncExpr("array_append", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, DoubleType())
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto *context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto *result = e.GetResult();

    auto *resultArr = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArr, nullptr);
    ASSERT_EQ(resultArr->GetSize(), 2);

    ASSERT_EQ(resultArr->GetOffset(1) - resultArr->GetOffset(0), 3);
    ASSERT_EQ(resultArr->GetOffset(2) - resultArr->GetOffset(1), 2);

    auto *elemResult = dynamic_cast<Vector<double> *>(resultArr->GetElementVector().get());
    ASSERT_NE(elemResult, nullptr);
    ASSERT_DOUBLE_EQ(elemResult->GetValue(0), 1.1);
    ASSERT_DOUBLE_EQ(elemResult->GetValue(1), 2.2);
    ASSERT_DOUBLE_EQ(elemResult->GetValue(2), 9.9);
    ASSERT_DOUBLE_EQ(elemResult->GetValue(3), 3.3);
    ASSERT_DOUBLE_EQ(elemResult->GetValue(4), 8.8);

    delete context;
    delete input;
    delete result;
}

TEST(ArrayAppendTest, AppendBoolElements)
{
    int32_t rowSize = 2;
    bool elems[] = {true, false, true};
    std::vector<int32_t> offsets = {0, 2, 3};
    bool appendElems[] = {false, true};

    auto *input = CreateBoolArrayWithElementBatch(rowSize, 3, elems, offsets, appendElems);

    auto expr = FuncExpr("array_append", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, BooleanType())
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto *context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto *result = e.GetResult();

    auto *resultArr = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArr, nullptr);
    ASSERT_EQ(resultArr->GetSize(), 2);

    ASSERT_EQ(resultArr->GetOffset(1) - resultArr->GetOffset(0), 3);
    ASSERT_EQ(resultArr->GetOffset(2) - resultArr->GetOffset(1), 2);

    auto *elemResult = dynamic_cast<Vector<bool> *>(resultArr->GetElementVector().get());
    ASSERT_NE(elemResult, nullptr);
    ASSERT_EQ(elemResult->GetValue(0), true);
    ASSERT_EQ(elemResult->GetValue(1), false);
    ASSERT_EQ(elemResult->GetValue(2), false);
    ASSERT_EQ(elemResult->GetValue(3), true);
    ASSERT_EQ(elemResult->GetValue(4), true);

    delete context;
    delete input;
    delete result;
}

TEST(ArrayAppendTest, SingleElementArrayAppend)
{
    int32_t rowSize = 2;
    int32_t elems[] = {10, 20};
    std::vector<int32_t> offsets = {0, 1, 2};
    int32_t appendElems[] = {11, 21};

    auto *input = CreateIntArrayWithElementBatch(rowSize, 2, elems, offsets, appendElems);

    auto expr = FuncExpr("array_append", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, IntType())
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto *context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto *result = e.GetResult();

    auto *resultArr = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArr, nullptr);
    ASSERT_EQ(resultArr->GetSize(), 2);

    // Row 0: [10] + 11 -> [10, 11]
    ASSERT_EQ(resultArr->GetOffset(1) - resultArr->GetOffset(0), 2);
    // Row 1: [20] + 21 -> [20, 21]
    ASSERT_EQ(resultArr->GetOffset(2) - resultArr->GetOffset(1), 2);

    auto *elemResult = dynamic_cast<Vector<int32_t> *>(resultArr->GetElementVector().get());
    ASSERT_NE(elemResult, nullptr);
    ASSERT_EQ(elemResult->GetValue(0), 10);
    ASSERT_EQ(elemResult->GetValue(1), 11);
    ASSERT_EQ(elemResult->GetValue(2), 20);
    ASSERT_EQ(elemResult->GetValue(3), 21);

    delete context;
    delete input;
    delete result;
}

TEST(ArrayAppendTest, MixedNullAndNonNullRows)
{
    // SetNull on ArrayVector calls SetSize(row, 0), which sets offsets[row+1] = offsets[row].
    // This collapses the null row's offset range, so subsequent rows absorb those elements.
    // To get clean per-row separation, place the null row at the end or use zero-length ranges.
    int32_t rowSize = 4;
    int32_t elems[] = {1, 2, 5, 6, 7};
    std::vector<int32_t> offsets = {0, 2, 2, 4, 5};
    int32_t appendElems[] = {10, 20, 30, 40};

    auto *input = CreateIntArrayWithElementBatch(rowSize, 5, elems, offsets, appendElems);
    auto *arrVec = dynamic_cast<ArrayVector *>(input->Get(0));
    arrVec->SetNull(1);
    auto *appendVec = dynamic_cast<Vector<int32_t> *>(input->Get(1));
    appendVec->SetNull(2);

    auto expr = FuncExpr("array_append", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, IntType())
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto *context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto *result = e.GetResult();

    auto *resultArr = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArr, nullptr);
    ASSERT_EQ(resultArr->GetSize(), 4);

    // Row 0: [1,2] + 10 -> [1,2,10]
    ASSERT_FALSE(resultArr->IsNull(0));
    ASSERT_EQ(resultArr->GetOffset(1) - resultArr->GetOffset(0), 3);

    // Row 1: null -> null
    ASSERT_TRUE(resultArr->IsNull(1));

    // Row 2: [5,6] + null -> [5,6,null]
    ASSERT_FALSE(resultArr->IsNull(2));
    ASSERT_EQ(resultArr->GetOffset(3) - resultArr->GetOffset(2), 3);

    // Row 3: [7] + 40 -> [7,40]
    ASSERT_FALSE(resultArr->IsNull(3));
    ASSERT_EQ(resultArr->GetOffset(4) - resultArr->GetOffset(3), 2);

    delete context;
    delete input;
    delete result;
}

TEST(ArrayAppendTest, AppendDuplicateValues)
{
    int32_t rowSize = 2;
    int32_t elems[] = {1, 2, 3, 1, 2};
    std::vector<int32_t> offsets = {0, 3, 5};
    int32_t appendElems[] = {2, 1};

    auto *input = CreateIntArrayWithElementBatch(rowSize, 5, elems, offsets, appendElems);

    auto expr = FuncExpr("array_append", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, IntType())
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto *context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto *result = e.GetResult();

    auto *resultArr = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArr, nullptr);

    // Row 0: [1,2,3] + 2 -> [1,2,3,2]
    ASSERT_EQ(resultArr->GetOffset(1) - resultArr->GetOffset(0), 4);
    // Row 1: [1,2] + 1 -> [1,2,1]
    ASSERT_EQ(resultArr->GetOffset(2) - resultArr->GetOffset(1), 3);

    auto *elemResult = dynamic_cast<Vector<int32_t> *>(resultArr->GetElementVector().get());
    ASSERT_NE(elemResult, nullptr);
    ASSERT_EQ(elemResult->GetValue(0), 1);
    ASSERT_EQ(elemResult->GetValue(1), 2);
    ASSERT_EQ(elemResult->GetValue(2), 3);
    ASSERT_EQ(elemResult->GetValue(3), 2);
    ASSERT_EQ(elemResult->GetValue(4), 1);
    ASSERT_EQ(elemResult->GetValue(5), 2);
    ASSERT_EQ(elemResult->GetValue(6), 1);

    delete context;
    delete input;
    delete result;
}
