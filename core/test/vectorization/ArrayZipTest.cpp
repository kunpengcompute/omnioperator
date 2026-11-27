/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: ArrayZip function test
 */

#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <limits>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/ExprEval.h"
#include "expression/expressions.h"
#include "vectorization/registration/SimpleFunctionRegistry.h"
#include "type/decimal128.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::mem;
using namespace omniruntime::op;
using namespace omniruntime::expressions;
using namespace omniruntime::TestUtil;
using namespace omniruntime::type;

class ArrayZipTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

/// Test: basic integer zip with same-length arrays
/// arrays_zip([1, 2, 3], [10, 20, 30]) -> [(1,10), (2,20), (3,30)]
TEST_F(ArrayZipTest, BasicIntSameLength)
{
    int rowSize = 1;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto arrayIntType = std::make_shared<ArrayType>(intType);

    std::vector<std::shared_ptr<DataType>> rowChildren = {intType, intType};
    auto rowType = std::make_shared<RowType>(rowChildren);
    auto outputType = std::make_shared<ArrayType>(std::shared_ptr<DataType>(rowType));

    auto *vectorBatch = new VectorBatch(rowSize);

    auto leftElemVec = std::shared_ptr<BaseVector>(new Vector<int32_t>(3));
    auto *leftEv = dynamic_cast<Vector<int32_t> *>(leftElemVec.get());
    leftEv->SetValue(0, 1);
    leftEv->SetValue(1, 2);
    leftEv->SetValue(2, 3);
    auto *leftArrVec = new ArrayVector(rowSize, leftElemVec);
    leftArrVec->SetOffset(0, 0);
    leftArrVec->SetOffset(1, 3);
    vectorBatch->Append(leftArrVec);

    auto rightElemVec = std::shared_ptr<BaseVector>(new Vector<int32_t>(3));
    auto *rightEv = dynamic_cast<Vector<int32_t> *>(rightElemVec.get());
    rightEv->SetValue(0, 10);
    rightEv->SetValue(1, 20);
    rightEv->SetValue(2, 30);
    auto *rightArrVec = new ArrayVector(rowSize, rightElemVec);
    rightArrVec->SetOffset(0, 0);
    rightArrVec->SetOffset(1, 3);
    vectorBatch->Append(rightArrVec);

    auto expr = FuncExpr("arrays_zip", {
        new FieldExpr(0, arrayIntType),
        new FieldExpr(1, arrayIntType)
    }, outputType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_FALSE(resultArray->IsNull(0));
    EXPECT_EQ(resultArray->GetSize(0), 3);

    auto elemVec = resultArray->GetElementVector();
    auto *resultRow = dynamic_cast<RowVector *>(elemVec.get());
    ASSERT_NE(resultRow, nullptr);
    EXPECT_EQ(resultRow->ChildSize(), 2);

    auto *child0 = dynamic_cast<Vector<int32_t> *>(resultRow->ChildAt(0).get());
    auto *child1 = dynamic_cast<Vector<int32_t> *>(resultRow->ChildAt(1).get());
    ASSERT_NE(child0, nullptr);
    ASSERT_NE(child1, nullptr);

    int32_t rOff = resultArray->GetOffset(0);
    EXPECT_EQ(child0->GetValue(rOff), 1);
    EXPECT_EQ(child0->GetValue(rOff + 1), 2);
    EXPECT_EQ(child0->GetValue(rOff + 2), 3);
    EXPECT_EQ(child1->GetValue(rOff), 10);
    EXPECT_EQ(child1->GetValue(rOff + 1), 20);
    EXPECT_EQ(child1->GetValue(rOff + 2), 30);

    delete context;
    delete vectorBatch;
    delete result;
}

/// Test: arrays of different lengths (shorter padded with nulls)
/// arrays_zip([1, 2, 3, 4], [10, 20]) -> [(1,10), (2,20), (3,null), (4,null)]
TEST_F(ArrayZipTest, DifferentLengthArrays)
{
    int rowSize = 1;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto arrayIntType = std::make_shared<ArrayType>(intType);

    std::vector<std::shared_ptr<DataType>> rowChildren = {intType, intType};
    auto rowType = std::make_shared<RowType>(rowChildren);
    auto outputType = std::make_shared<ArrayType>(std::shared_ptr<DataType>(rowType));

    auto *vectorBatch = new VectorBatch(rowSize);

    auto leftElemVec = std::shared_ptr<BaseVector>(new Vector<int32_t>(4));
    auto *leftEv = dynamic_cast<Vector<int32_t> *>(leftElemVec.get());
    leftEv->SetValue(0, 1);
    leftEv->SetValue(1, 2);
    leftEv->SetValue(2, 3);
    leftEv->SetValue(3, 4);
    auto *leftArrVec = new ArrayVector(rowSize, leftElemVec);
    leftArrVec->SetOffset(0, 0);
    leftArrVec->SetOffset(1, 4);
    vectorBatch->Append(leftArrVec);

    auto rightElemVec = std::shared_ptr<BaseVector>(new Vector<int32_t>(2));
    auto *rightEv = dynamic_cast<Vector<int32_t> *>(rightElemVec.get());
    rightEv->SetValue(0, 10);
    rightEv->SetValue(1, 20);
    auto *rightArrVec = new ArrayVector(rowSize, rightElemVec);
    rightArrVec->SetOffset(0, 0);
    rightArrVec->SetOffset(1, 2);
    vectorBatch->Append(rightArrVec);

    auto expr = FuncExpr("arrays_zip", {
        new FieldExpr(0, arrayIntType),
        new FieldExpr(1, arrayIntType)
    }, outputType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_EQ(resultArray->GetSize(0), 4);

    auto elemVec = resultArray->GetElementVector();
    auto *resultRow = dynamic_cast<RowVector *>(elemVec.get());
    ASSERT_NE(resultRow, nullptr);

    auto *child0 = dynamic_cast<Vector<int32_t> *>(resultRow->ChildAt(0).get());
    auto *child1 = dynamic_cast<Vector<int32_t> *>(resultRow->ChildAt(1).get());
    ASSERT_NE(child0, nullptr);
    ASSERT_NE(child1, nullptr);

    int32_t rOff = resultArray->GetOffset(0);
    EXPECT_EQ(child0->GetValue(rOff), 1);
    EXPECT_EQ(child0->GetValue(rOff + 1), 2);
    EXPECT_EQ(child0->GetValue(rOff + 2), 3);
    EXPECT_EQ(child0->GetValue(rOff + 3), 4);

    EXPECT_EQ(child1->GetValue(rOff), 10);
    EXPECT_EQ(child1->GetValue(rOff + 1), 20);
    EXPECT_TRUE(child1->IsNull(rOff + 2));
    EXPECT_TRUE(child1->IsNull(rOff + 3));

    delete context;
    delete vectorBatch;
    delete result;
}

/// Test: null input array row
/// arrays_zip(null, [1, 2]) -> null
TEST_F(ArrayZipTest, NullArrayRow)
{
    int rowSize = 1;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto arrayIntType = std::make_shared<ArrayType>(intType);

    std::vector<std::shared_ptr<DataType>> rowChildren = {intType, intType};
    auto rowType = std::make_shared<RowType>(rowChildren);
    auto outputType = std::make_shared<ArrayType>(std::shared_ptr<DataType>(rowType));

    auto *vectorBatch = new VectorBatch(rowSize);

    auto leftElemVec = std::shared_ptr<BaseVector>(new Vector<int32_t>(1));
    auto *leftArrVec = new ArrayVector(rowSize, leftElemVec);
    leftArrVec->SetOffset(0, 0);
    leftArrVec->SetOffset(1, 0);
    leftArrVec->SetNull(0);
    vectorBatch->Append(leftArrVec);

    auto rightElemVec = std::shared_ptr<BaseVector>(new Vector<int32_t>(2));
    auto *rightEv = dynamic_cast<Vector<int32_t> *>(rightElemVec.get());
    rightEv->SetValue(0, 1);
    rightEv->SetValue(1, 2);
    auto *rightArrVec = new ArrayVector(rowSize, rightElemVec);
    rightArrVec->SetOffset(0, 0);
    rightArrVec->SetOffset(1, 2);
    vectorBatch->Append(rightArrVec);

    auto expr = FuncExpr("arrays_zip", {
        new FieldExpr(0, arrayIntType),
        new FieldExpr(1, arrayIntType)
    }, outputType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_TRUE(resultArray->IsNull(0));

    delete context;
    delete vectorBatch;
    delete result;
}

/// Test: both empty arrays
/// arrays_zip([], []) -> []
TEST_F(ArrayZipTest, BothEmptyArrays)
{
    int rowSize = 1;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto arrayIntType = std::make_shared<ArrayType>(intType);

    std::vector<std::shared_ptr<DataType>> rowChildren = {intType, intType};
    auto rowType = std::make_shared<RowType>(rowChildren);
    auto outputType = std::make_shared<ArrayType>(std::shared_ptr<DataType>(rowType));

    auto *vectorBatch = new VectorBatch(rowSize);

    auto leftElemVec = std::shared_ptr<BaseVector>(new Vector<int32_t>(1));
    auto *leftArrVec = new ArrayVector(rowSize, leftElemVec);
    leftArrVec->SetOffset(0, 0);
    leftArrVec->SetOffset(1, 0);
    vectorBatch->Append(leftArrVec);

    auto rightElemVec = std::shared_ptr<BaseVector>(new Vector<int32_t>(1));
    auto *rightArrVec = new ArrayVector(rowSize, rightElemVec);
    rightArrVec->SetOffset(0, 0);
    rightArrVec->SetOffset(1, 0);
    vectorBatch->Append(rightArrVec);

    auto expr = FuncExpr("arrays_zip", {
        new FieldExpr(0, arrayIntType),
        new FieldExpr(1, arrayIntType)
    }, outputType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_FALSE(resultArray->IsNull(0));
    EXPECT_EQ(resultArray->GetSize(0), 0);

    delete context;
    delete vectorBatch;
    delete result;
}

/// Test: double type arrays
/// arrays_zip([1.1, 2.2], [10.5, 20.5]) -> [(1.1,10.5), (2.2,20.5)]
TEST_F(ArrayZipTest, DoubleTypeArrays)
{
    int rowSize = 1;
    auto doubleType = std::make_shared<DataType>(OMNI_DOUBLE);
    auto arrayDoubleType = std::make_shared<ArrayType>(doubleType);

    std::vector<std::shared_ptr<DataType>> rowChildren = {doubleType, doubleType};
    auto rowType = std::make_shared<RowType>(rowChildren);
    auto outputType = std::make_shared<ArrayType>(std::shared_ptr<DataType>(rowType));

    auto *vectorBatch = new VectorBatch(rowSize);

    auto leftElemVec = std::shared_ptr<BaseVector>(new Vector<double>(2));
    auto *leftEv = dynamic_cast<Vector<double> *>(leftElemVec.get());
    leftEv->SetValue(0, 1.1);
    leftEv->SetValue(1, 2.2);
    auto *leftArrVec = new ArrayVector(rowSize, leftElemVec);
    leftArrVec->SetOffset(0, 0);
    leftArrVec->SetOffset(1, 2);
    vectorBatch->Append(leftArrVec);

    auto rightElemVec = std::shared_ptr<BaseVector>(new Vector<double>(2));
    auto *rightEv = dynamic_cast<Vector<double> *>(rightElemVec.get());
    rightEv->SetValue(0, 10.5);
    rightEv->SetValue(1, 20.5);
    auto *rightArrVec = new ArrayVector(rowSize, rightElemVec);
    rightArrVec->SetOffset(0, 0);
    rightArrVec->SetOffset(1, 2);
    vectorBatch->Append(rightArrVec);

    auto expr = FuncExpr("arrays_zip", {
        new FieldExpr(0, arrayDoubleType),
        new FieldExpr(1, arrayDoubleType)
    }, outputType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_EQ(resultArray->GetSize(0), 2);

    auto elemVec = resultArray->GetElementVector();
    auto *resultRow = dynamic_cast<RowVector *>(elemVec.get());
    ASSERT_NE(resultRow, nullptr);

    auto *child0 = dynamic_cast<Vector<double> *>(resultRow->ChildAt(0).get());
    auto *child1 = dynamic_cast<Vector<double> *>(resultRow->ChildAt(1).get());
    ASSERT_NE(child0, nullptr);
    ASSERT_NE(child1, nullptr);

    int32_t rOff = resultArray->GetOffset(0);
    EXPECT_DOUBLE_EQ(child0->GetValue(rOff), 1.1);
    EXPECT_DOUBLE_EQ(child0->GetValue(rOff + 1), 2.2);
    EXPECT_DOUBLE_EQ(child1->GetValue(rOff), 10.5);
    EXPECT_DOUBLE_EQ(child1->GetValue(rOff + 1), 20.5);

    delete context;
    delete vectorBatch;
    delete result;
}

/// Test: float type arrays
/// arrays_zip([1.5f, 2.5f], [10.0f]) -> [(1.5,10.0), (2.5,null)]
TEST_F(ArrayZipTest, FloatDifferentLengths)
{
    int rowSize = 1;
    auto floatType = std::make_shared<DataType>(OMNI_FLOAT);
    auto arrayFloatType = std::make_shared<ArrayType>(floatType);

    std::vector<std::shared_ptr<DataType>> rowChildren = {floatType, floatType};
    auto rowType = std::make_shared<RowType>(rowChildren);
    auto outputType = std::make_shared<ArrayType>(std::shared_ptr<DataType>(rowType));

    auto *vectorBatch = new VectorBatch(rowSize);

    auto leftElemVec = std::shared_ptr<BaseVector>(new Vector<float>(2));
    auto *leftEv = dynamic_cast<Vector<float> *>(leftElemVec.get());
    leftEv->SetValue(0, 1.5f);
    leftEv->SetValue(1, 2.5f);
    auto *leftArrVec = new ArrayVector(rowSize, leftElemVec);
    leftArrVec->SetOffset(0, 0);
    leftArrVec->SetOffset(1, 2);
    vectorBatch->Append(leftArrVec);

    auto rightElemVec = std::shared_ptr<BaseVector>(new Vector<float>(1));
    auto *rightEv = dynamic_cast<Vector<float> *>(rightElemVec.get());
    rightEv->SetValue(0, 10.0f);
    auto *rightArrVec = new ArrayVector(rowSize, rightElemVec);
    rightArrVec->SetOffset(0, 0);
    rightArrVec->SetOffset(1, 1);
    vectorBatch->Append(rightArrVec);

    auto expr = FuncExpr("arrays_zip", {
        new FieldExpr(0, arrayFloatType),
        new FieldExpr(1, arrayFloatType)
    }, outputType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_EQ(resultArray->GetSize(0), 2);

    auto elemVec = resultArray->GetElementVector();
    auto *resultRow = dynamic_cast<RowVector *>(elemVec.get());
    ASSERT_NE(resultRow, nullptr);

    auto *child0 = dynamic_cast<Vector<float> *>(resultRow->ChildAt(0).get());
    auto *child1 = dynamic_cast<Vector<float> *>(resultRow->ChildAt(1).get());
    ASSERT_NE(child0, nullptr);
    ASSERT_NE(child1, nullptr);

    int32_t rOff = resultArray->GetOffset(0);
    EXPECT_FLOAT_EQ(child0->GetValue(rOff), 1.5f);
    EXPECT_FLOAT_EQ(child0->GetValue(rOff + 1), 2.5f);
    EXPECT_FLOAT_EQ(child1->GetValue(rOff), 10.0f);
    EXPECT_TRUE(child1->IsNull(rOff + 1));

    delete context;
    delete vectorBatch;
    delete result;
}

/// Test: long (int64) type arrays
/// arrays_zip([100L, 200L], [1000L, 2000L]) -> [(100,1000), (200,2000)]
TEST_F(ArrayZipTest, LongTypeArrays)
{
    int rowSize = 1;
    auto longType = std::make_shared<DataType>(OMNI_LONG);
    auto arrayLongType = std::make_shared<ArrayType>(longType);

    std::vector<std::shared_ptr<DataType>> rowChildren = {longType, longType};
    auto rowType = std::make_shared<RowType>(rowChildren);
    auto outputType = std::make_shared<ArrayType>(std::shared_ptr<DataType>(rowType));

    auto *vectorBatch = new VectorBatch(rowSize);

    auto leftElemVec = std::shared_ptr<BaseVector>(new Vector<int64_t>(2));
    auto *leftEv = dynamic_cast<Vector<int64_t> *>(leftElemVec.get());
    leftEv->SetValue(0, 100L);
    leftEv->SetValue(1, 200L);
    auto *leftArrVec = new ArrayVector(rowSize, leftElemVec);
    leftArrVec->SetOffset(0, 0);
    leftArrVec->SetOffset(1, 2);
    vectorBatch->Append(leftArrVec);

    auto rightElemVec = std::shared_ptr<BaseVector>(new Vector<int64_t>(2));
    auto *rightEv = dynamic_cast<Vector<int64_t> *>(rightElemVec.get());
    rightEv->SetValue(0, 1000L);
    rightEv->SetValue(1, 2000L);
    auto *rightArrVec = new ArrayVector(rowSize, rightElemVec);
    rightArrVec->SetOffset(0, 0);
    rightArrVec->SetOffset(1, 2);
    vectorBatch->Append(rightArrVec);

    auto expr = FuncExpr("arrays_zip", {
        new FieldExpr(0, arrayLongType),
        new FieldExpr(1, arrayLongType)
    }, outputType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_EQ(resultArray->GetSize(0), 2);

    auto elemVec = resultArray->GetElementVector();
    auto *resultRow = dynamic_cast<RowVector *>(elemVec.get());
    ASSERT_NE(resultRow, nullptr);

    auto *child0 = dynamic_cast<Vector<int64_t> *>(resultRow->ChildAt(0).get());
    auto *child1 = dynamic_cast<Vector<int64_t> *>(resultRow->ChildAt(1).get());
    ASSERT_NE(child0, nullptr);
    ASSERT_NE(child1, nullptr);

    int32_t rOff = resultArray->GetOffset(0);
    EXPECT_EQ(child0->GetValue(rOff), 100L);
    EXPECT_EQ(child0->GetValue(rOff + 1), 200L);
    EXPECT_EQ(child1->GetValue(rOff), 1000L);
    EXPECT_EQ(child1->GetValue(rOff + 1), 2000L);

    delete context;
    delete vectorBatch;
    delete result;
}

/// Test: short (int16) type arrays
/// arrays_zip([10, 20], [30, 40]) -> [(10,30), (20,40)]
TEST_F(ArrayZipTest, ShortTypeArrays)
{
    int rowSize = 1;
    auto shortType = std::make_shared<DataType>(OMNI_SHORT);
    auto arrayShortType = std::make_shared<ArrayType>(shortType);

    std::vector<std::shared_ptr<DataType>> rowChildren = {shortType, shortType};
    auto rowType = std::make_shared<RowType>(rowChildren);
    auto outputType = std::make_shared<ArrayType>(std::shared_ptr<DataType>(rowType));

    auto *vectorBatch = new VectorBatch(rowSize);

    auto leftElemVec = std::shared_ptr<BaseVector>(new Vector<int16_t>(2));
    auto *leftEv = dynamic_cast<Vector<int16_t> *>(leftElemVec.get());
    leftEv->SetValue(0, static_cast<int16_t>(10));
    leftEv->SetValue(1, static_cast<int16_t>(20));
    auto *leftArrVec = new ArrayVector(rowSize, leftElemVec);
    leftArrVec->SetOffset(0, 0);
    leftArrVec->SetOffset(1, 2);
    vectorBatch->Append(leftArrVec);

    auto rightElemVec = std::shared_ptr<BaseVector>(new Vector<int16_t>(2));
    auto *rightEv = dynamic_cast<Vector<int16_t> *>(rightElemVec.get());
    rightEv->SetValue(0, static_cast<int16_t>(30));
    rightEv->SetValue(1, static_cast<int16_t>(40));
    auto *rightArrVec = new ArrayVector(rowSize, rightElemVec);
    rightArrVec->SetOffset(0, 0);
    rightArrVec->SetOffset(1, 2);
    vectorBatch->Append(rightArrVec);

    auto expr = FuncExpr("arrays_zip", {
        new FieldExpr(0, arrayShortType),
        new FieldExpr(1, arrayShortType)
    }, outputType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_EQ(resultArray->GetSize(0), 2);

    auto elemVec = resultArray->GetElementVector();
    auto *resultRow = dynamic_cast<RowVector *>(elemVec.get());
    ASSERT_NE(resultRow, nullptr);

    auto *child0 = dynamic_cast<Vector<int16_t> *>(resultRow->ChildAt(0).get());
    auto *child1 = dynamic_cast<Vector<int16_t> *>(resultRow->ChildAt(1).get());
    ASSERT_NE(child0, nullptr);
    ASSERT_NE(child1, nullptr);

    int32_t rOff = resultArray->GetOffset(0);
    EXPECT_EQ(child0->GetValue(rOff), 10);
    EXPECT_EQ(child0->GetValue(rOff + 1), 20);
    EXPECT_EQ(child1->GetValue(rOff), 30);
    EXPECT_EQ(child1->GetValue(rOff + 1), 40);

    delete context;
    delete vectorBatch;
    delete result;
}

/// Test: byte (int8) type arrays
/// arrays_zip([1, 2], [3, 4]) -> [(1,3), (2,4)]
TEST_F(ArrayZipTest, ByteTypeArrays)
{
    int rowSize = 1;
    auto byteType = std::make_shared<DataType>(OMNI_BYTE);
    auto arrayByteType = std::make_shared<ArrayType>(byteType);

    std::vector<std::shared_ptr<DataType>> rowChildren = {byteType, byteType};
    auto rowType = std::make_shared<RowType>(rowChildren);
    auto outputType = std::make_shared<ArrayType>(std::shared_ptr<DataType>(rowType));

    auto *vectorBatch = new VectorBatch(rowSize);

    auto leftElemVec = std::shared_ptr<BaseVector>(new Vector<int8_t>(2));
    auto *leftEv = dynamic_cast<Vector<int8_t> *>(leftElemVec.get());
    leftEv->SetValue(0, static_cast<int8_t>(1));
    leftEv->SetValue(1, static_cast<int8_t>(2));
    auto *leftArrVec = new ArrayVector(rowSize, leftElemVec);
    leftArrVec->SetOffset(0, 0);
    leftArrVec->SetOffset(1, 2);
    vectorBatch->Append(leftArrVec);

    auto rightElemVec = std::shared_ptr<BaseVector>(new Vector<int8_t>(2));
    auto *rightEv = dynamic_cast<Vector<int8_t> *>(rightElemVec.get());
    rightEv->SetValue(0, static_cast<int8_t>(3));
    rightEv->SetValue(1, static_cast<int8_t>(4));
    auto *rightArrVec = new ArrayVector(rowSize, rightElemVec);
    rightArrVec->SetOffset(0, 0);
    rightArrVec->SetOffset(1, 2);
    vectorBatch->Append(rightArrVec);

    auto expr = FuncExpr("arrays_zip", {
        new FieldExpr(0, arrayByteType),
        new FieldExpr(1, arrayByteType)
    }, outputType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_EQ(resultArray->GetSize(0), 2);

    auto elemVec = resultArray->GetElementVector();
    auto *resultRow = dynamic_cast<RowVector *>(elemVec.get());
    ASSERT_NE(resultRow, nullptr);

    auto *child0 = dynamic_cast<Vector<int8_t> *>(resultRow->ChildAt(0).get());
    auto *child1 = dynamic_cast<Vector<int8_t> *>(resultRow->ChildAt(1).get());
    ASSERT_NE(child0, nullptr);
    ASSERT_NE(child1, nullptr);

    int32_t rOff = resultArray->GetOffset(0);
    EXPECT_EQ(child0->GetValue(rOff), 1);
    EXPECT_EQ(child0->GetValue(rOff + 1), 2);
    EXPECT_EQ(child1->GetValue(rOff), 3);
    EXPECT_EQ(child1->GetValue(rOff + 1), 4);

    delete context;
    delete vectorBatch;
    delete result;
}

/// Test: boolean type arrays
/// arrays_zip([true, false], [false, true]) -> [(true,false), (false,true)]
TEST_F(ArrayZipTest, BooleanTypeArrays)
{
    int rowSize = 1;
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto arrayBoolType = std::make_shared<ArrayType>(boolType);

    std::vector<std::shared_ptr<DataType>> rowChildren = {boolType, boolType};
    auto rowType = std::make_shared<RowType>(rowChildren);
    auto outputType = std::make_shared<ArrayType>(std::shared_ptr<DataType>(rowType));

    auto *vectorBatch = new VectorBatch(rowSize);

    auto leftElemVec = std::shared_ptr<BaseVector>(new Vector<bool>(2));
    auto *leftEv = dynamic_cast<Vector<bool> *>(leftElemVec.get());
    leftEv->SetValue(0, true);
    leftEv->SetValue(1, false);
    auto *leftArrVec = new ArrayVector(rowSize, leftElemVec);
    leftArrVec->SetOffset(0, 0);
    leftArrVec->SetOffset(1, 2);
    vectorBatch->Append(leftArrVec);

    auto rightElemVec = std::shared_ptr<BaseVector>(new Vector<bool>(2));
    auto *rightEv = dynamic_cast<Vector<bool> *>(rightElemVec.get());
    rightEv->SetValue(0, false);
    rightEv->SetValue(1, true);
    auto *rightArrVec = new ArrayVector(rowSize, rightElemVec);
    rightArrVec->SetOffset(0, 0);
    rightArrVec->SetOffset(1, 2);
    vectorBatch->Append(rightArrVec);

    auto expr = FuncExpr("arrays_zip", {
        new FieldExpr(0, arrayBoolType),
        new FieldExpr(1, arrayBoolType)
    }, outputType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_EQ(resultArray->GetSize(0), 2);

    auto elemVec = resultArray->GetElementVector();
    auto *resultRow = dynamic_cast<RowVector *>(elemVec.get());
    ASSERT_NE(resultRow, nullptr);

    auto *child0 = dynamic_cast<Vector<bool> *>(resultRow->ChildAt(0).get());
    auto *child1 = dynamic_cast<Vector<bool> *>(resultRow->ChildAt(1).get());
    ASSERT_NE(child0, nullptr);
    ASSERT_NE(child1, nullptr);

    int32_t rOff = resultArray->GetOffset(0);
    EXPECT_EQ(child0->GetValue(rOff), true);
    EXPECT_EQ(child0->GetValue(rOff + 1), false);
    EXPECT_EQ(child1->GetValue(rOff), false);
    EXPECT_EQ(child1->GetValue(rOff + 1), true);

    delete context;
    delete vectorBatch;
    delete result;
}

/// Test: varchar/string type arrays with different types (int + string)
/// arrays_zip([1, 2], ["hello", "world"]) -> [(1,"hello"), (2,"world")]
TEST_F(ArrayZipTest, MixedIntAndVarcharArrays)
{
    int rowSize = 1;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto varcharType = std::make_shared<DataType>(OMNI_VARCHAR);
    auto arrayIntType = std::make_shared<ArrayType>(intType);
    auto arrayVarcharType = std::make_shared<ArrayType>(varcharType);

    std::vector<std::shared_ptr<DataType>> rowChildren = {intType, varcharType};
    auto rowType = std::make_shared<RowType>(rowChildren);
    auto outputType = std::make_shared<ArrayType>(std::shared_ptr<DataType>(rowType));

    using StringVector = Vector<LargeStringContainer<std::string_view>>;

    auto *vectorBatch = new VectorBatch(rowSize);

    auto leftElemVec = std::shared_ptr<BaseVector>(new Vector<int32_t>(2));
    auto *leftEv = dynamic_cast<Vector<int32_t> *>(leftElemVec.get());
    leftEv->SetValue(0, 1);
    leftEv->SetValue(1, 2);
    auto *leftArrVec = new ArrayVector(rowSize, leftElemVec);
    leftArrVec->SetOffset(0, 0);
    leftArrVec->SetOffset(1, 2);
    vectorBatch->Append(leftArrVec);

    auto *rightStrEv = new StringVector(2);
    rightStrEv->SetValue(0, std::string_view("hello"));
    rightStrEv->SetValue(1, std::string_view("world"));
    auto rightElemVec = std::shared_ptr<BaseVector>(rightStrEv);
    auto *rightArrVec = new ArrayVector(rowSize, rightElemVec);
    rightArrVec->SetOffset(0, 0);
    rightArrVec->SetOffset(1, 2);
    vectorBatch->Append(rightArrVec);

    auto expr = FuncExpr("arrays_zip", {
        new FieldExpr(0, arrayIntType),
        new FieldExpr(1, arrayVarcharType)
    }, outputType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_EQ(resultArray->GetSize(0), 2);

    auto elemVec = resultArray->GetElementVector();
    auto *resultRow = dynamic_cast<RowVector *>(elemVec.get());
    ASSERT_NE(resultRow, nullptr);
    EXPECT_EQ(resultRow->ChildSize(), 2);

    auto *child0 = dynamic_cast<Vector<int32_t> *>(resultRow->ChildAt(0).get());
    auto *child1 = dynamic_cast<StringVector *>(resultRow->ChildAt(1).get());
    ASSERT_NE(child0, nullptr);
    ASSERT_NE(child1, nullptr);

    int32_t rOff = resultArray->GetOffset(0);
    EXPECT_EQ(child0->GetValue(rOff), 1);
    EXPECT_EQ(child0->GetValue(rOff + 1), 2);
    EXPECT_EQ(child1->GetValue(rOff), std::string_view("hello"));
    EXPECT_EQ(child1->GetValue(rOff + 1), std::string_view("world"));

    delete context;
    delete vectorBatch;
    delete result;
}

/// Test: date32 type arrays (internally int32_t)
/// arrays_zip([100, 200], [300]) -> [(100,300), (200,null)]
TEST_F(ArrayZipTest, Date32TypeArrays)
{
    int rowSize = 1;
    auto dateType = std::make_shared<DataType>(OMNI_DATE32);
    auto arrayDateType = std::make_shared<ArrayType>(dateType);

    std::vector<std::shared_ptr<DataType>> rowChildren = {dateType, dateType};
    auto rowType = std::make_shared<RowType>(rowChildren);
    auto outputType = std::make_shared<ArrayType>(std::shared_ptr<DataType>(rowType));

    auto *vectorBatch = new VectorBatch(rowSize);

    auto leftElemVec = std::shared_ptr<BaseVector>(new Vector<int32_t>(2));
    auto *leftEv = dynamic_cast<Vector<int32_t> *>(leftElemVec.get());
    leftEv->SetValue(0, 100);
    leftEv->SetValue(1, 200);
    auto *leftArrVec = new ArrayVector(rowSize, leftElemVec);
    leftArrVec->SetOffset(0, 0);
    leftArrVec->SetOffset(1, 2);
    vectorBatch->Append(leftArrVec);

    auto rightElemVec = std::shared_ptr<BaseVector>(new Vector<int32_t>(1));
    auto *rightEv = dynamic_cast<Vector<int32_t> *>(rightElemVec.get());
    rightEv->SetValue(0, 300);
    auto *rightArrVec = new ArrayVector(rowSize, rightElemVec);
    rightArrVec->SetOffset(0, 0);
    rightArrVec->SetOffset(1, 1);
    vectorBatch->Append(rightArrVec);

    auto expr = FuncExpr("arrays_zip", {
        new FieldExpr(0, arrayDateType),
        new FieldExpr(1, arrayDateType)
    }, outputType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_EQ(resultArray->GetSize(0), 2);

    auto elemVec = resultArray->GetElementVector();
    auto *resultRow = dynamic_cast<RowVector *>(elemVec.get());
    ASSERT_NE(resultRow, nullptr);

    auto *child0 = dynamic_cast<Vector<int32_t> *>(resultRow->ChildAt(0).get());
    auto *child1 = dynamic_cast<Vector<int32_t> *>(resultRow->ChildAt(1).get());
    ASSERT_NE(child0, nullptr);
    ASSERT_NE(child1, nullptr);

    int32_t rOff = resultArray->GetOffset(0);
    EXPECT_EQ(child0->GetValue(rOff), 100);
    EXPECT_EQ(child0->GetValue(rOff + 1), 200);
    EXPECT_EQ(child1->GetValue(rOff), 300);
    EXPECT_TRUE(child1->IsNull(rOff + 1));

    delete context;
    delete vectorBatch;
    delete result;
}

/// Test: timestamp type arrays (internally int64_t)
/// arrays_zip([1000L, 2000L], [3000L, 4000L]) -> [(1000,3000), (2000,4000)]
TEST_F(ArrayZipTest, TimestampTypeArrays)
{
    int rowSize = 1;
    auto tsType = std::make_shared<DataType>(OMNI_TIMESTAMP);
    auto arrayTsType = std::make_shared<ArrayType>(tsType);

    std::vector<std::shared_ptr<DataType>> rowChildren = {tsType, tsType};
    auto rowType = std::make_shared<RowType>(rowChildren);
    auto outputType = std::make_shared<ArrayType>(std::shared_ptr<DataType>(rowType));

    auto *vectorBatch = new VectorBatch(rowSize);

    auto leftElemVec = std::shared_ptr<BaseVector>(new Vector<int64_t>(2));
    auto *leftEv = dynamic_cast<Vector<int64_t> *>(leftElemVec.get());
    leftEv->SetValue(0, 1000L);
    leftEv->SetValue(1, 2000L);
    auto *leftArrVec = new ArrayVector(rowSize, leftElemVec);
    leftArrVec->SetOffset(0, 0);
    leftArrVec->SetOffset(1, 2);
    vectorBatch->Append(leftArrVec);

    auto rightElemVec = std::shared_ptr<BaseVector>(new Vector<int64_t>(2));
    auto *rightEv = dynamic_cast<Vector<int64_t> *>(rightElemVec.get());
    rightEv->SetValue(0, 3000L);
    rightEv->SetValue(1, 4000L);
    auto *rightArrVec = new ArrayVector(rowSize, rightElemVec);
    rightArrVec->SetOffset(0, 0);
    rightArrVec->SetOffset(1, 2);
    vectorBatch->Append(rightArrVec);

    auto expr = FuncExpr("arrays_zip", {
        new FieldExpr(0, arrayTsType),
        new FieldExpr(1, arrayTsType)
    }, outputType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_EQ(resultArray->GetSize(0), 2);

    auto elemVec = resultArray->GetElementVector();
    auto *resultRow = dynamic_cast<RowVector *>(elemVec.get());
    ASSERT_NE(resultRow, nullptr);

    auto *child0 = dynamic_cast<Vector<int64_t> *>(resultRow->ChildAt(0).get());
    auto *child1 = dynamic_cast<Vector<int64_t> *>(resultRow->ChildAt(1).get());
    ASSERT_NE(child0, nullptr);
    ASSERT_NE(child1, nullptr);

    int32_t rOff = resultArray->GetOffset(0);
    EXPECT_EQ(child0->GetValue(rOff), 1000L);
    EXPECT_EQ(child0->GetValue(rOff + 1), 2000L);
    EXPECT_EQ(child1->GetValue(rOff), 3000L);
    EXPECT_EQ(child1->GetValue(rOff + 1), 4000L);

    delete context;
    delete vectorBatch;
    delete result;
}

/// Test: decimal128 type arrays
/// arrays_zip([Decimal128(100), Decimal128(200)], [Decimal128(300)]) -> [(100,300), (200,null)]
TEST_F(ArrayZipTest, Decimal128TypeArrays)
{
    int rowSize = 1;
    auto decimalType = std::make_shared<DataType>(OMNI_DECIMAL128);
    auto arrayDecType = std::make_shared<ArrayType>(decimalType);

    std::vector<std::shared_ptr<DataType>> rowChildren = {decimalType, decimalType};
    auto rowType = std::make_shared<RowType>(rowChildren);
    auto outputType = std::make_shared<ArrayType>(std::shared_ptr<DataType>(rowType));

    Decimal128 d100(100);
    Decimal128 d200(200);
    Decimal128 d300(300);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto leftElemVec = std::shared_ptr<BaseVector>(new Vector<Decimal128>(2));
    auto *leftEv = dynamic_cast<Vector<Decimal128> *>(leftElemVec.get());
    leftEv->SetValue(0, d100);
    leftEv->SetValue(1, d200);
    auto *leftArrVec = new ArrayVector(rowSize, leftElemVec);
    leftArrVec->SetOffset(0, 0);
    leftArrVec->SetOffset(1, 2);
    vectorBatch->Append(leftArrVec);

    auto rightElemVec = std::shared_ptr<BaseVector>(new Vector<Decimal128>(1));
    auto *rightEv = dynamic_cast<Vector<Decimal128> *>(rightElemVec.get());
    rightEv->SetValue(0, d300);
    auto *rightArrVec = new ArrayVector(rowSize, rightElemVec);
    rightArrVec->SetOffset(0, 0);
    rightArrVec->SetOffset(1, 1);
    vectorBatch->Append(rightArrVec);

    auto expr = FuncExpr("arrays_zip", {
        new FieldExpr(0, arrayDecType),
        new FieldExpr(1, arrayDecType)
    }, outputType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_EQ(resultArray->GetSize(0), 2);

    auto elemVec = resultArray->GetElementVector();
    auto *resultRow = dynamic_cast<RowVector *>(elemVec.get());
    ASSERT_NE(resultRow, nullptr);

    auto *child0 = dynamic_cast<Vector<Decimal128> *>(resultRow->ChildAt(0).get());
    auto *child1 = dynamic_cast<Vector<Decimal128> *>(resultRow->ChildAt(1).get());
    ASSERT_NE(child0, nullptr);
    ASSERT_NE(child1, nullptr);

    int32_t rOff = resultArray->GetOffset(0);
    EXPECT_EQ(child0->GetValue(rOff), d100);
    EXPECT_EQ(child0->GetValue(rOff + 1), d200);
    EXPECT_EQ(child1->GetValue(rOff), d300);
    EXPECT_TRUE(child1->IsNull(rOff + 1));

    delete context;
    delete vectorBatch;
    delete result;
}

/// Test: null elements in input arrays
/// arrays_zip([1, null, 3], [10, 20, 30]) -> [(1,10), (null,20), (3,30)]
TEST_F(ArrayZipTest, NullElementsInInput)
{
    int rowSize = 1;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto arrayIntType = std::make_shared<ArrayType>(intType);

    std::vector<std::shared_ptr<DataType>> rowChildren = {intType, intType};
    auto rowType = std::make_shared<RowType>(rowChildren);
    auto outputType = std::make_shared<ArrayType>(std::shared_ptr<DataType>(rowType));

    auto *vectorBatch = new VectorBatch(rowSize);

    auto leftElemVec = std::shared_ptr<BaseVector>(new Vector<int32_t>(3));
    auto *leftEv = dynamic_cast<Vector<int32_t> *>(leftElemVec.get());
    leftEv->SetValue(0, 1);
    leftElemVec->SetNull(1);
    leftEv->SetValue(2, 3);
    auto *leftArrVec = new ArrayVector(rowSize, leftElemVec);
    leftArrVec->SetOffset(0, 0);
    leftArrVec->SetOffset(1, 3);
    vectorBatch->Append(leftArrVec);

    auto rightElemVec = std::shared_ptr<BaseVector>(new Vector<int32_t>(3));
    auto *rightEv = dynamic_cast<Vector<int32_t> *>(rightElemVec.get());
    rightEv->SetValue(0, 10);
    rightEv->SetValue(1, 20);
    rightEv->SetValue(2, 30);
    auto *rightArrVec = new ArrayVector(rowSize, rightElemVec);
    rightArrVec->SetOffset(0, 0);
    rightArrVec->SetOffset(1, 3);
    vectorBatch->Append(rightArrVec);

    auto expr = FuncExpr("arrays_zip", {
        new FieldExpr(0, arrayIntType),
        new FieldExpr(1, arrayIntType)
    }, outputType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_EQ(resultArray->GetSize(0), 3);

    auto elemVec = resultArray->GetElementVector();
    auto *resultRow = dynamic_cast<RowVector *>(elemVec.get());
    ASSERT_NE(resultRow, nullptr);

    auto *child0 = dynamic_cast<Vector<int32_t> *>(resultRow->ChildAt(0).get());
    auto *child1 = dynamic_cast<Vector<int32_t> *>(resultRow->ChildAt(1).get());
    ASSERT_NE(child0, nullptr);
    ASSERT_NE(child1, nullptr);

    int32_t rOff = resultArray->GetOffset(0);
    EXPECT_EQ(child0->GetValue(rOff), 1);
    EXPECT_TRUE(child0->IsNull(rOff + 1));
    EXPECT_EQ(child0->GetValue(rOff + 2), 3);
    EXPECT_EQ(child1->GetValue(rOff), 10);
    EXPECT_EQ(child1->GetValue(rOff + 1), 20);
    EXPECT_EQ(child1->GetValue(rOff + 2), 30);

    delete context;
    delete vectorBatch;
    delete result;
}

/// Test: multiple rows
/// Row 0: arrays_zip([1, 2], [10, 20]) -> [(1,10), (2,20)]
/// Row 1: arrays_zip([3], [30, 40, 50]) -> [(3,30), (null,40), (null,50)]
/// Row 2: arrays_zip([], [100]) -> [(null,100)]
TEST_F(ArrayZipTest, MultipleRows)
{
    int rowSize = 3;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto arrayIntType = std::make_shared<ArrayType>(intType);

    std::vector<std::shared_ptr<DataType>> rowChildren = {intType, intType};
    auto rowType = std::make_shared<RowType>(rowChildren);
    auto outputType = std::make_shared<ArrayType>(std::shared_ptr<DataType>(rowType));

    auto *vectorBatch = new VectorBatch(rowSize);

    // Left: [1,2], [3], []
    auto leftElemVec = std::shared_ptr<BaseVector>(new Vector<int32_t>(3));
    auto *leftEv = dynamic_cast<Vector<int32_t> *>(leftElemVec.get());
    leftEv->SetValue(0, 1);
    leftEv->SetValue(1, 2);
    leftEv->SetValue(2, 3);
    auto *leftArrVec = new ArrayVector(rowSize, leftElemVec);
    leftArrVec->SetOffset(0, 0);
    leftArrVec->SetOffset(1, 2);
    leftArrVec->SetOffset(2, 3);
    leftArrVec->SetOffset(3, 3);
    vectorBatch->Append(leftArrVec);

    // Right: [10,20], [30,40,50], [100]
    auto rightElemVec = std::shared_ptr<BaseVector>(new Vector<int32_t>(6));
    auto *rightEv = dynamic_cast<Vector<int32_t> *>(rightElemVec.get());
    rightEv->SetValue(0, 10);
    rightEv->SetValue(1, 20);
    rightEv->SetValue(2, 30);
    rightEv->SetValue(3, 40);
    rightEv->SetValue(4, 50);
    rightEv->SetValue(5, 100);
    auto *rightArrVec = new ArrayVector(rowSize, rightElemVec);
    rightArrVec->SetOffset(0, 0);
    rightArrVec->SetOffset(1, 2);
    rightArrVec->SetOffset(2, 5);
    rightArrVec->SetOffset(3, 6);
    vectorBatch->Append(rightArrVec);

    auto expr = FuncExpr("arrays_zip", {
        new FieldExpr(0, arrayIntType),
        new FieldExpr(1, arrayIntType)
    }, outputType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);

    auto elemVec = resultArray->GetElementVector();
    auto *resultRow = dynamic_cast<RowVector *>(elemVec.get());
    ASSERT_NE(resultRow, nullptr);

    auto *child0 = dynamic_cast<Vector<int32_t> *>(resultRow->ChildAt(0).get());
    auto *child1 = dynamic_cast<Vector<int32_t> *>(resultRow->ChildAt(1).get());
    ASSERT_NE(child0, nullptr);
    ASSERT_NE(child1, nullptr);

    // Row 0: [(1,10), (2,20)]
    EXPECT_EQ(resultArray->GetSize(0), 2);
    int32_t r0Off = resultArray->GetOffset(0);
    EXPECT_EQ(child0->GetValue(r0Off), 1);
    EXPECT_EQ(child0->GetValue(r0Off + 1), 2);
    EXPECT_EQ(child1->GetValue(r0Off), 10);
    EXPECT_EQ(child1->GetValue(r0Off + 1), 20);

    // Row 1: [(3,30), (null,40), (null,50)]
    EXPECT_EQ(resultArray->GetSize(1), 3);
    int32_t r1Off = resultArray->GetOffset(1);
    EXPECT_EQ(child0->GetValue(r1Off), 3);
    EXPECT_TRUE(child0->IsNull(r1Off + 1));
    EXPECT_TRUE(child0->IsNull(r1Off + 2));
    EXPECT_EQ(child1->GetValue(r1Off), 30);
    EXPECT_EQ(child1->GetValue(r1Off + 1), 40);
    EXPECT_EQ(child1->GetValue(r1Off + 2), 50);

    // Row 2: [(null,100)]
    EXPECT_EQ(resultArray->GetSize(2), 1);
    int32_t r2Off = resultArray->GetOffset(2);
    EXPECT_TRUE(child0->IsNull(r2Off));
    EXPECT_EQ(child1->GetValue(r2Off), 100);

    delete context;
    delete vectorBatch;
    delete result;
}

/// Test: multiple rows with null rows mixed
/// Row 0: arrays_zip([1, 2], [10, 20]) -> [(1,10), (2,20)]
/// Row 1: arrays_zip(null, [1, 2]) -> null
/// Row 2: arrays_zip([5], [50]) -> [(5,50)]
TEST_F(ArrayZipTest, MultipleRowsWithNullRow)
{
    int rowSize = 3;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto arrayIntType = std::make_shared<ArrayType>(intType);

    std::vector<std::shared_ptr<DataType>> rowChildren = {intType, intType};
    auto rowType = std::make_shared<RowType>(rowChildren);
    auto outputType = std::make_shared<ArrayType>(std::shared_ptr<DataType>(rowType));

    auto *vectorBatch = new VectorBatch(rowSize);

    // Left: [1,2], null, [5]
    auto leftElemVec = std::shared_ptr<BaseVector>(new Vector<int32_t>(3));
    auto *leftEv = dynamic_cast<Vector<int32_t> *>(leftElemVec.get());
    leftEv->SetValue(0, 1);
    leftEv->SetValue(1, 2);
    leftEv->SetValue(2, 5);
    auto *leftArrVec = new ArrayVector(rowSize, leftElemVec);
    leftArrVec->SetOffset(0, 0);
    leftArrVec->SetOffset(1, 2);
    leftArrVec->SetOffset(2, 2);
    leftArrVec->SetOffset(3, 3);
    leftArrVec->SetNull(1);
    vectorBatch->Append(leftArrVec);

    // Right: [10,20], [1,2], [50]
    auto rightElemVec = std::shared_ptr<BaseVector>(new Vector<int32_t>(5));
    auto *rightEv = dynamic_cast<Vector<int32_t> *>(rightElemVec.get());
    rightEv->SetValue(0, 10);
    rightEv->SetValue(1, 20);
    rightEv->SetValue(2, 1);
    rightEv->SetValue(3, 2);
    rightEv->SetValue(4, 50);
    auto *rightArrVec = new ArrayVector(rowSize, rightElemVec);
    rightArrVec->SetOffset(0, 0);
    rightArrVec->SetOffset(1, 2);
    rightArrVec->SetOffset(2, 4);
    rightArrVec->SetOffset(3, 5);
    vectorBatch->Append(rightArrVec);

    auto expr = FuncExpr("arrays_zip", {
        new FieldExpr(0, arrayIntType),
        new FieldExpr(1, arrayIntType)
    }, outputType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);

    auto elemVec = resultArray->GetElementVector();
    auto *resultRow = dynamic_cast<RowVector *>(elemVec.get());
    ASSERT_NE(resultRow, nullptr);

    auto *child0 = dynamic_cast<Vector<int32_t> *>(resultRow->ChildAt(0).get());
    auto *child1 = dynamic_cast<Vector<int32_t> *>(resultRow->ChildAt(1).get());

    // Row 0: [(1,10), (2,20)]
    EXPECT_FALSE(resultArray->IsNull(0));
    EXPECT_EQ(resultArray->GetSize(0), 2);
    int32_t r0Off = resultArray->GetOffset(0);
    EXPECT_EQ(child0->GetValue(r0Off), 1);
    EXPECT_EQ(child0->GetValue(r0Off + 1), 2);
    EXPECT_EQ(child1->GetValue(r0Off), 10);
    EXPECT_EQ(child1->GetValue(r0Off + 1), 20);

    // Row 1: null
    EXPECT_TRUE(resultArray->IsNull(1));

    // Row 2: [(5,50)]
    EXPECT_FALSE(resultArray->IsNull(2));
    EXPECT_EQ(resultArray->GetSize(2), 1);
    int32_t r2Off = resultArray->GetOffset(2);
    EXPECT_EQ(child0->GetValue(r2Off), 5);
    EXPECT_EQ(child1->GetValue(r2Off), 50);

    delete context;
    delete vectorBatch;
    delete result;
}

/// Test: three-array zip (arity 3)
/// arrays_zip([1, 2], [10, 20], [100, 200]) -> [(1,10,100), (2,20,200)]
TEST_F(ArrayZipTest, ThreeArrayZip)
{
    int rowSize = 1;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto arrayIntType = std::make_shared<ArrayType>(intType);

    std::vector<std::shared_ptr<DataType>> rowChildren = {intType, intType, intType};
    auto rowType = std::make_shared<RowType>(rowChildren);
    auto outputType = std::make_shared<ArrayType>(std::shared_ptr<DataType>(rowType));

    auto *vectorBatch = new VectorBatch(rowSize);

    auto elem0 = std::shared_ptr<BaseVector>(new Vector<int32_t>(2));
    auto *ev0 = dynamic_cast<Vector<int32_t> *>(elem0.get());
    ev0->SetValue(0, 1);
    ev0->SetValue(1, 2);
    auto *arr0 = new ArrayVector(rowSize, elem0);
    arr0->SetOffset(0, 0);
    arr0->SetOffset(1, 2);
    vectorBatch->Append(arr0);

    auto elem1 = std::shared_ptr<BaseVector>(new Vector<int32_t>(2));
    auto *ev1 = dynamic_cast<Vector<int32_t> *>(elem1.get());
    ev1->SetValue(0, 10);
    ev1->SetValue(1, 20);
    auto *arr1 = new ArrayVector(rowSize, elem1);
    arr1->SetOffset(0, 0);
    arr1->SetOffset(1, 2);
    vectorBatch->Append(arr1);

    auto elem2 = std::shared_ptr<BaseVector>(new Vector<int32_t>(2));
    auto *ev2 = dynamic_cast<Vector<int32_t> *>(elem2.get());
    ev2->SetValue(0, 100);
    ev2->SetValue(1, 200);
    auto *arr2 = new ArrayVector(rowSize, elem2);
    arr2->SetOffset(0, 0);
    arr2->SetOffset(1, 2);
    vectorBatch->Append(arr2);

    auto expr = FuncExpr("arrays_zip", {
        new FieldExpr(0, arrayIntType),
        new FieldExpr(1, arrayIntType),
        new FieldExpr(2, arrayIntType)
    }, outputType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_EQ(resultArray->GetSize(0), 2);

    auto elemVec = resultArray->GetElementVector();
    auto *resultRow = dynamic_cast<RowVector *>(elemVec.get());
    ASSERT_NE(resultRow, nullptr);
    EXPECT_EQ(resultRow->ChildSize(), 3);

    auto *child0 = dynamic_cast<Vector<int32_t> *>(resultRow->ChildAt(0).get());
    auto *child1 = dynamic_cast<Vector<int32_t> *>(resultRow->ChildAt(1).get());
    auto *child2 = dynamic_cast<Vector<int32_t> *>(resultRow->ChildAt(2).get());
    ASSERT_NE(child0, nullptr);
    ASSERT_NE(child1, nullptr);
    ASSERT_NE(child2, nullptr);

    int32_t rOff = resultArray->GetOffset(0);
    EXPECT_EQ(child0->GetValue(rOff), 1);
    EXPECT_EQ(child0->GetValue(rOff + 1), 2);
    EXPECT_EQ(child1->GetValue(rOff), 10);
    EXPECT_EQ(child1->GetValue(rOff + 1), 20);
    EXPECT_EQ(child2->GetValue(rOff), 100);
    EXPECT_EQ(child2->GetValue(rOff + 1), 200);

    delete context;
    delete vectorBatch;
    delete result;
}

/// Test: varchar type arrays
/// arrays_zip(["a", "b"], ["x", "y"]) -> [("a","x"), ("b","y")]
TEST_F(ArrayZipTest, VarcharTypeArrays)
{
    int rowSize = 1;
    auto varcharType = std::make_shared<DataType>(OMNI_VARCHAR);
    auto arrayVarcharType = std::make_shared<ArrayType>(varcharType);

    std::vector<std::shared_ptr<DataType>> rowChildren = {varcharType, varcharType};
    auto rowType = std::make_shared<RowType>(rowChildren);
    auto outputType = std::make_shared<ArrayType>(std::shared_ptr<DataType>(rowType));

    using StringVector = Vector<LargeStringContainer<std::string_view>>;

    auto *vectorBatch = new VectorBatch(rowSize);

    auto *leftStrEv = new StringVector(2);
    leftStrEv->SetValue(0, std::string_view("a"));
    leftStrEv->SetValue(1, std::string_view("b"));
    auto leftElemVec = std::shared_ptr<BaseVector>(leftStrEv);
    auto *leftArrVec = new ArrayVector(rowSize, leftElemVec);
    leftArrVec->SetOffset(0, 0);
    leftArrVec->SetOffset(1, 2);
    vectorBatch->Append(leftArrVec);

    auto *rightStrEv = new StringVector(2);
    rightStrEv->SetValue(0, std::string_view("x"));
    rightStrEv->SetValue(1, std::string_view("y"));
    auto rightElemVec = std::shared_ptr<BaseVector>(rightStrEv);
    auto *rightArrVec = new ArrayVector(rowSize, rightElemVec);
    rightArrVec->SetOffset(0, 0);
    rightArrVec->SetOffset(1, 2);
    vectorBatch->Append(rightArrVec);

    auto expr = FuncExpr("arrays_zip", {
        new FieldExpr(0, arrayVarcharType),
        new FieldExpr(1, arrayVarcharType)
    }, outputType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_EQ(resultArray->GetSize(0), 2);

    auto elemVec = resultArray->GetElementVector();
    auto *resultRow = dynamic_cast<RowVector *>(elemVec.get());
    ASSERT_NE(resultRow, nullptr);

    auto *child0 = dynamic_cast<StringVector *>(resultRow->ChildAt(0).get());
    auto *child1 = dynamic_cast<StringVector *>(resultRow->ChildAt(1).get());
    ASSERT_NE(child0, nullptr);
    ASSERT_NE(child1, nullptr);

    int32_t rOff = resultArray->GetOffset(0);
    EXPECT_EQ(child0->GetValue(rOff), std::string_view("a"));
    EXPECT_EQ(child0->GetValue(rOff + 1), std::string_view("b"));
    EXPECT_EQ(child1->GetValue(rOff), std::string_view("x"));
    EXPECT_EQ(child1->GetValue(rOff + 1), std::string_view("y"));

    delete context;
    delete vectorBatch;
    delete result;
}
