/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: ArrayIntersect function test
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

class ArrayIntersectTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

/// Test: basic integer intersection
/// array_intersect([1, 2, 3], [2, 3, 4]) -> [2, 3]
TEST_F(ArrayIntersectTest, BasicIntIntersection)
{
    int rowSize = 1;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto arrayType = std::make_shared<ArrayType>(intType);

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
    rightEv->SetValue(0, 2);
    rightEv->SetValue(1, 3);
    rightEv->SetValue(2, 4);
    auto *rightArrVec = new ArrayVector(rowSize, rightElemVec);
    rightArrVec->SetOffset(0, 0);
    rightArrVec->SetOffset(1, 3);
    vectorBatch->Append(rightArrVec);

    auto expr = FuncExpr("array_intersect", {
        new FieldExpr(0, arrayType),
        new FieldExpr(1, arrayType)
    }, arrayType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_FALSE(resultArray->IsNull(0));
    EXPECT_EQ(resultArray->GetSize(0), 2);

    auto elementVector = resultArray->GetElementVector();
    auto intVector = dynamic_cast<Vector<int32_t> *>(elementVector.get());
    ASSERT_NE(intVector, nullptr);
    int32_t rOff = resultArray->GetOffset(0);
    EXPECT_EQ(intVector->GetValue(rOff), 2);
    EXPECT_EQ(intVector->GetValue(rOff + 1), 3);

    delete context;
    delete vectorBatch;
    delete result;
}

/// Test: no overlap between arrays
/// array_intersect([1, 2, 3], [4, 5, 6]) -> []
TEST_F(ArrayIntersectTest, NoOverlap)
{
    int rowSize = 1;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto arrayType = std::make_shared<ArrayType>(intType);

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
    rightEv->SetValue(0, 4);
    rightEv->SetValue(1, 5);
    rightEv->SetValue(2, 6);
    auto *rightArrVec = new ArrayVector(rowSize, rightElemVec);
    rightArrVec->SetOffset(0, 0);
    rightArrVec->SetOffset(1, 3);
    vectorBatch->Append(rightArrVec);

    auto expr = FuncExpr("array_intersect", {
        new FieldExpr(0, arrayType),
        new FieldExpr(1, arrayType)
    }, arrayType);

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

/// Test: full overlap (identical arrays)
/// array_intersect([1, 2, 3], [1, 2, 3]) -> [1, 2, 3]
TEST_F(ArrayIntersectTest, FullOverlap)
{
    int rowSize = 1;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto arrayType = std::make_shared<ArrayType>(intType);

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
    rightEv->SetValue(0, 1);
    rightEv->SetValue(1, 2);
    rightEv->SetValue(2, 3);
    auto *rightArrVec = new ArrayVector(rowSize, rightElemVec);
    rightArrVec->SetOffset(0, 0);
    rightArrVec->SetOffset(1, 3);
    vectorBatch->Append(rightArrVec);

    auto expr = FuncExpr("array_intersect", {
        new FieldExpr(0, arrayType),
        new FieldExpr(1, arrayType)
    }, arrayType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_FALSE(resultArray->IsNull(0));
    EXPECT_EQ(resultArray->GetSize(0), 3);

    auto elementVector = resultArray->GetElementVector();
    auto intVector = dynamic_cast<Vector<int32_t> *>(elementVector.get());
    ASSERT_NE(intVector, nullptr);
    int32_t rOff = resultArray->GetOffset(0);
    EXPECT_EQ(intVector->GetValue(rOff), 1);
    EXPECT_EQ(intVector->GetValue(rOff + 1), 2);
    EXPECT_EQ(intVector->GetValue(rOff + 2), 3);

    delete context;
    delete vectorBatch;
    delete result;
}

/// Test: duplicates in input arrays should produce unique result
/// array_intersect([1, 1, 2, 2, 3], [1, 2]) -> [1, 2]
TEST_F(ArrayIntersectTest, DuplicateElements)
{
    int rowSize = 1;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto arrayType = std::make_shared<ArrayType>(intType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto leftElemVec = std::shared_ptr<BaseVector>(new Vector<int32_t>(5));
    auto *leftEv = dynamic_cast<Vector<int32_t> *>(leftElemVec.get());
    leftEv->SetValue(0, 1);
    leftEv->SetValue(1, 1);
    leftEv->SetValue(2, 2);
    leftEv->SetValue(3, 2);
    leftEv->SetValue(4, 3);
    auto *leftArrVec = new ArrayVector(rowSize, leftElemVec);
    leftArrVec->SetOffset(0, 0);
    leftArrVec->SetOffset(1, 5);
    vectorBatch->Append(leftArrVec);

    auto rightElemVec = std::shared_ptr<BaseVector>(new Vector<int32_t>(2));
    auto *rightEv = dynamic_cast<Vector<int32_t> *>(rightElemVec.get());
    rightEv->SetValue(0, 1);
    rightEv->SetValue(1, 2);
    auto *rightArrVec = new ArrayVector(rowSize, rightElemVec);
    rightArrVec->SetOffset(0, 0);
    rightArrVec->SetOffset(1, 2);
    vectorBatch->Append(rightArrVec);

    auto expr = FuncExpr("array_intersect", {
        new FieldExpr(0, arrayType),
        new FieldExpr(1, arrayType)
    }, arrayType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_EQ(resultArray->GetSize(0), 2);

    auto elementVector = resultArray->GetElementVector();
    auto intVector = dynamic_cast<Vector<int32_t> *>(elementVector.get());
    ASSERT_NE(intVector, nullptr);
    int32_t rOff = resultArray->GetOffset(0);
    EXPECT_EQ(intVector->GetValue(rOff), 1);
    EXPECT_EQ(intVector->GetValue(rOff + 1), 2);

    delete context;
    delete vectorBatch;
    delete result;
}

/// Test: null elements in both arrays
/// array_intersect([1, null, 3], [null, 3, 4]) -> [null, 3]
TEST_F(ArrayIntersectTest, NullElements)
{
    int rowSize = 1;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto arrayType = std::make_shared<ArrayType>(intType);

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
    rightElemVec->SetNull(0);
    rightEv->SetValue(1, 3);
    rightEv->SetValue(2, 4);
    auto *rightArrVec = new ArrayVector(rowSize, rightElemVec);
    rightArrVec->SetOffset(0, 0);
    rightArrVec->SetOffset(1, 3);
    vectorBatch->Append(rightArrVec);

    auto expr = FuncExpr("array_intersect", {
        new FieldExpr(0, arrayType),
        new FieldExpr(1, arrayType)
    }, arrayType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_FALSE(resultArray->IsNull(0));
    EXPECT_EQ(resultArray->GetSize(0), 2);

    auto elementVector = resultArray->GetElementVector();
    int32_t rOff = resultArray->GetOffset(0);
    EXPECT_TRUE(elementVector->IsNull(rOff));
    auto intVector = dynamic_cast<Vector<int32_t> *>(elementVector.get());
    ASSERT_NE(intVector, nullptr);
    EXPECT_EQ(intVector->GetValue(rOff + 1), 3);

    delete context;
    delete vectorBatch;
    delete result;
}

/// Test: null array (entire row is null)
/// array_intersect(null, [1, 2]) -> null
TEST_F(ArrayIntersectTest, NullArray)
{
    int rowSize = 1;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto arrayType = std::make_shared<ArrayType>(intType);

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

    auto expr = FuncExpr("array_intersect", {
        new FieldExpr(0, arrayType),
        new FieldExpr(1, arrayType)
    }, arrayType);

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

/// Test: empty arrays
/// array_intersect([], [1, 2]) -> []
TEST_F(ArrayIntersectTest, EmptyLeftArray)
{
    int rowSize = 1;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto arrayType = std::make_shared<ArrayType>(intType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto leftElemVec = std::shared_ptr<BaseVector>(new Vector<int32_t>(1));
    auto *leftArrVec = new ArrayVector(rowSize, leftElemVec);
    leftArrVec->SetOffset(0, 0);
    leftArrVec->SetOffset(1, 0);
    vectorBatch->Append(leftArrVec);

    auto rightElemVec = std::shared_ptr<BaseVector>(new Vector<int32_t>(2));
    auto *rightEv = dynamic_cast<Vector<int32_t> *>(rightElemVec.get());
    rightEv->SetValue(0, 1);
    rightEv->SetValue(1, 2);
    auto *rightArrVec = new ArrayVector(rowSize, rightElemVec);
    rightArrVec->SetOffset(0, 0);
    rightArrVec->SetOffset(1, 2);
    vectorBatch->Append(rightArrVec);

    auto expr = FuncExpr("array_intersect", {
        new FieldExpr(0, arrayType),
        new FieldExpr(1, arrayType)
    }, arrayType);

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

/// Test: double type intersection
/// array_intersect([1.1, 2.2, 3.3], [2.2, 4.4]) -> [2.2]
TEST_F(ArrayIntersectTest, DoubleIntersection)
{
    int rowSize = 1;
    auto doubleType = std::make_shared<DataType>(OMNI_DOUBLE);
    auto arrayType = std::make_shared<ArrayType>(doubleType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto leftElemVec = std::shared_ptr<BaseVector>(new Vector<double>(3));
    auto *leftEv = dynamic_cast<Vector<double> *>(leftElemVec.get());
    leftEv->SetValue(0, 1.1);
    leftEv->SetValue(1, 2.2);
    leftEv->SetValue(2, 3.3);
    auto *leftArrVec = new ArrayVector(rowSize, leftElemVec);
    leftArrVec->SetOffset(0, 0);
    leftArrVec->SetOffset(1, 3);
    vectorBatch->Append(leftArrVec);

    auto rightElemVec = std::shared_ptr<BaseVector>(new Vector<double>(2));
    auto *rightEv = dynamic_cast<Vector<double> *>(rightElemVec.get());
    rightEv->SetValue(0, 2.2);
    rightEv->SetValue(1, 4.4);
    auto *rightArrVec = new ArrayVector(rowSize, rightElemVec);
    rightArrVec->SetOffset(0, 0);
    rightArrVec->SetOffset(1, 2);
    vectorBatch->Append(rightArrVec);

    auto expr = FuncExpr("array_intersect", {
        new FieldExpr(0, arrayType),
        new FieldExpr(1, arrayType)
    }, arrayType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_EQ(resultArray->GetSize(0), 1);

    auto elementVector = resultArray->GetElementVector();
    auto dblVector = dynamic_cast<Vector<double> *>(elementVector.get());
    ASSERT_NE(dblVector, nullptr);
    EXPECT_DOUBLE_EQ(dblVector->GetValue(resultArray->GetOffset(0)), 2.2);

    delete context;
    delete vectorBatch;
    delete result;
}

/// Test: double NaN handling (NaN == NaN for intersection)
/// array_intersect([NaN, 1.0], [NaN, 2.0]) -> [NaN]
TEST_F(ArrayIntersectTest, DoubleNaNHandling)
{
    int rowSize = 1;
    auto doubleType = std::make_shared<DataType>(OMNI_DOUBLE);
    auto arrayType = std::make_shared<ArrayType>(doubleType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto leftElemVec = std::shared_ptr<BaseVector>(new Vector<double>(2));
    auto *leftEv = dynamic_cast<Vector<double> *>(leftElemVec.get());
    leftEv->SetValue(0, std::numeric_limits<double>::quiet_NaN());
    leftEv->SetValue(1, 1.0);
    auto *leftArrVec = new ArrayVector(rowSize, leftElemVec);
    leftArrVec->SetOffset(0, 0);
    leftArrVec->SetOffset(1, 2);
    vectorBatch->Append(leftArrVec);

    auto rightElemVec = std::shared_ptr<BaseVector>(new Vector<double>(2));
    auto *rightEv = dynamic_cast<Vector<double> *>(rightElemVec.get());
    rightEv->SetValue(0, std::numeric_limits<double>::quiet_NaN());
    rightEv->SetValue(1, 2.0);
    auto *rightArrVec = new ArrayVector(rowSize, rightElemVec);
    rightArrVec->SetOffset(0, 0);
    rightArrVec->SetOffset(1, 2);
    vectorBatch->Append(rightArrVec);

    auto expr = FuncExpr("array_intersect", {
        new FieldExpr(0, arrayType),
        new FieldExpr(1, arrayType)
    }, arrayType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_EQ(resultArray->GetSize(0), 1);

    auto elementVector = resultArray->GetElementVector();
    auto dblVector = dynamic_cast<Vector<double> *>(elementVector.get());
    ASSERT_NE(dblVector, nullptr);
    EXPECT_TRUE(std::isnan(dblVector->GetValue(resultArray->GetOffset(0))));

    delete context;
    delete vectorBatch;
    delete result;
}

/// Test: float type intersection
/// array_intersect([1.5f, 2.5f, 3.5f], [2.5f, 4.5f]) -> [2.5f]
TEST_F(ArrayIntersectTest, FloatIntersection)
{
    int rowSize = 1;
    auto floatType = std::make_shared<DataType>(OMNI_FLOAT);
    auto arrayType = std::make_shared<ArrayType>(floatType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto leftElemVec = std::shared_ptr<BaseVector>(new Vector<float>(3));
    auto *leftEv = dynamic_cast<Vector<float> *>(leftElemVec.get());
    leftEv->SetValue(0, 1.5f);
    leftEv->SetValue(1, 2.5f);
    leftEv->SetValue(2, 3.5f);
    auto *leftArrVec = new ArrayVector(rowSize, leftElemVec);
    leftArrVec->SetOffset(0, 0);
    leftArrVec->SetOffset(1, 3);
    vectorBatch->Append(leftArrVec);

    auto rightElemVec = std::shared_ptr<BaseVector>(new Vector<float>(2));
    auto *rightEv = dynamic_cast<Vector<float> *>(rightElemVec.get());
    rightEv->SetValue(0, 2.5f);
    rightEv->SetValue(1, 4.5f);
    auto *rightArrVec = new ArrayVector(rowSize, rightElemVec);
    rightArrVec->SetOffset(0, 0);
    rightArrVec->SetOffset(1, 2);
    vectorBatch->Append(rightArrVec);

    auto expr = FuncExpr("array_intersect", {
        new FieldExpr(0, arrayType),
        new FieldExpr(1, arrayType)
    }, arrayType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_EQ(resultArray->GetSize(0), 1);

    auto elementVector = resultArray->GetElementVector();
    auto fltVector = dynamic_cast<Vector<float> *>(elementVector.get());
    ASSERT_NE(fltVector, nullptr);
    EXPECT_FLOAT_EQ(fltVector->GetValue(resultArray->GetOffset(0)), 2.5f);

    delete context;
    delete vectorBatch;
    delete result;
}

/// Test: boolean type intersection
/// array_intersect([true, false], [true]) -> [true]
TEST_F(ArrayIntersectTest, BooleanIntersection)
{
    int rowSize = 1;
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto arrayType = std::make_shared<ArrayType>(boolType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto leftElemVec = std::shared_ptr<BaseVector>(new Vector<bool>(2));
    auto *leftEv = dynamic_cast<Vector<bool> *>(leftElemVec.get());
    leftEv->SetValue(0, true);
    leftEv->SetValue(1, false);
    auto *leftArrVec = new ArrayVector(rowSize, leftElemVec);
    leftArrVec->SetOffset(0, 0);
    leftArrVec->SetOffset(1, 2);
    vectorBatch->Append(leftArrVec);

    auto rightElemVec = std::shared_ptr<BaseVector>(new Vector<bool>(1));
    auto *rightEv = dynamic_cast<Vector<bool> *>(rightElemVec.get());
    rightEv->SetValue(0, true);
    auto *rightArrVec = new ArrayVector(rowSize, rightElemVec);
    rightArrVec->SetOffset(0, 0);
    rightArrVec->SetOffset(1, 1);
    vectorBatch->Append(rightArrVec);

    auto expr = FuncExpr("array_intersect", {
        new FieldExpr(0, arrayType),
        new FieldExpr(1, arrayType)
    }, arrayType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_EQ(resultArray->GetSize(0), 1);

    auto elementVector = resultArray->GetElementVector();
    auto boolVector = dynamic_cast<Vector<bool> *>(elementVector.get());
    ASSERT_NE(boolVector, nullptr);
    EXPECT_EQ(boolVector->GetValue(resultArray->GetOffset(0)), true);

    delete context;
    delete vectorBatch;
    delete result;
}

/// Test: varchar/string type intersection
/// array_intersect(["hello", "world", "test"], ["world", "foo"]) -> ["world"]
TEST_F(ArrayIntersectTest, VarcharIntersection)
{
    int rowSize = 1;
    auto varcharType = std::make_shared<DataType>(OMNI_VARCHAR);
    auto arrayType = std::make_shared<ArrayType>(varcharType);

    using StringVector = Vector<LargeStringContainer<std::string_view>>;

    auto *vectorBatch = new VectorBatch(rowSize);

    auto *leftEv = new StringVector(3);
    leftEv->SetValue(0, std::string_view("hello"));
    leftEv->SetValue(1, std::string_view("world"));
    leftEv->SetValue(2, std::string_view("test"));
    auto leftElemVec = std::shared_ptr<BaseVector>(leftEv);
    auto *leftArrVec = new ArrayVector(rowSize, leftElemVec);
    leftArrVec->SetOffset(0, 0);
    leftArrVec->SetOffset(1, 3);
    vectorBatch->Append(leftArrVec);

    auto *rightEv = new StringVector(2);
    rightEv->SetValue(0, std::string_view("world"));
    rightEv->SetValue(1, std::string_view("foo"));
    auto rightElemVec = std::shared_ptr<BaseVector>(rightEv);
    auto *rightArrVec = new ArrayVector(rowSize, rightElemVec);
    rightArrVec->SetOffset(0, 0);
    rightArrVec->SetOffset(1, 2);
    vectorBatch->Append(rightArrVec);

    auto expr = FuncExpr("array_intersect", {
        new FieldExpr(0, arrayType),
        new FieldExpr(1, arrayType)
    }, arrayType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_EQ(resultArray->GetSize(0), 1);

    auto elementVector = resultArray->GetElementVector();
    auto strVector = dynamic_cast<StringVector *>(elementVector.get());
    ASSERT_NE(strVector, nullptr);
    EXPECT_EQ(strVector->GetValue(resultArray->GetOffset(0)), std::string_view("world"));

    delete context;
    delete vectorBatch;
    delete result;
}

/// Test: long (int64) type intersection
/// array_intersect([100, 200, 300], [200, 400]) -> [200]
TEST_F(ArrayIntersectTest, LongIntersection)
{
    int rowSize = 1;
    auto longType = std::make_shared<DataType>(OMNI_LONG);
    auto arrayType = std::make_shared<ArrayType>(longType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto leftElemVec = std::shared_ptr<BaseVector>(new Vector<int64_t>(3));
    auto *leftEv = dynamic_cast<Vector<int64_t> *>(leftElemVec.get());
    leftEv->SetValue(0, 100L);
    leftEv->SetValue(1, 200L);
    leftEv->SetValue(2, 300L);
    auto *leftArrVec = new ArrayVector(rowSize, leftElemVec);
    leftArrVec->SetOffset(0, 0);
    leftArrVec->SetOffset(1, 3);
    vectorBatch->Append(leftArrVec);

    auto rightElemVec = std::shared_ptr<BaseVector>(new Vector<int64_t>(2));
    auto *rightEv = dynamic_cast<Vector<int64_t> *>(rightElemVec.get());
    rightEv->SetValue(0, 200L);
    rightEv->SetValue(1, 400L);
    auto *rightArrVec = new ArrayVector(rowSize, rightElemVec);
    rightArrVec->SetOffset(0, 0);
    rightArrVec->SetOffset(1, 2);
    vectorBatch->Append(rightArrVec);

    auto expr = FuncExpr("array_intersect", {
        new FieldExpr(0, arrayType),
        new FieldExpr(1, arrayType)
    }, arrayType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_EQ(resultArray->GetSize(0), 1);

    auto elementVector = resultArray->GetElementVector();
    auto longVector = dynamic_cast<Vector<int64_t> *>(elementVector.get());
    ASSERT_NE(longVector, nullptr);
    EXPECT_EQ(longVector->GetValue(resultArray->GetOffset(0)), 200L);

    delete context;
    delete vectorBatch;
    delete result;
}

/// Test: short (int16) type intersection
/// array_intersect([10, 20, 30], [20, 40]) -> [20]
TEST_F(ArrayIntersectTest, ShortIntersection)
{
    int rowSize = 1;
    auto shortType = std::make_shared<DataType>(OMNI_SHORT);
    auto arrayType = std::make_shared<ArrayType>(shortType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto leftElemVec = std::shared_ptr<BaseVector>(new Vector<int16_t>(3));
    auto *leftEv = dynamic_cast<Vector<int16_t> *>(leftElemVec.get());
    leftEv->SetValue(0, static_cast<int16_t>(10));
    leftEv->SetValue(1, static_cast<int16_t>(20));
    leftEv->SetValue(2, static_cast<int16_t>(30));
    auto *leftArrVec = new ArrayVector(rowSize, leftElemVec);
    leftArrVec->SetOffset(0, 0);
    leftArrVec->SetOffset(1, 3);
    vectorBatch->Append(leftArrVec);

    auto rightElemVec = std::shared_ptr<BaseVector>(new Vector<int16_t>(2));
    auto *rightEv = dynamic_cast<Vector<int16_t> *>(rightElemVec.get());
    rightEv->SetValue(0, static_cast<int16_t>(20));
    rightEv->SetValue(1, static_cast<int16_t>(40));
    auto *rightArrVec = new ArrayVector(rowSize, rightElemVec);
    rightArrVec->SetOffset(0, 0);
    rightArrVec->SetOffset(1, 2);
    vectorBatch->Append(rightArrVec);

    auto expr = FuncExpr("array_intersect", {
        new FieldExpr(0, arrayType),
        new FieldExpr(1, arrayType)
    }, arrayType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_EQ(resultArray->GetSize(0), 1);

    auto elementVector = resultArray->GetElementVector();
    auto shortVector = dynamic_cast<Vector<int16_t> *>(elementVector.get());
    ASSERT_NE(shortVector, nullptr);
    EXPECT_EQ(shortVector->GetValue(resultArray->GetOffset(0)), 20);

    delete context;
    delete vectorBatch;
    delete result;
}

/// Test: byte (int8) type intersection
/// array_intersect([1, 2, 3], [2, 4]) -> [2]
TEST_F(ArrayIntersectTest, ByteIntersection)
{
    int rowSize = 1;
    auto byteType = std::make_shared<DataType>(OMNI_BYTE);
    auto arrayType = std::make_shared<ArrayType>(byteType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto leftElemVec = std::shared_ptr<BaseVector>(new Vector<int8_t>(3));
    auto *leftEv = dynamic_cast<Vector<int8_t> *>(leftElemVec.get());
    leftEv->SetValue(0, static_cast<int8_t>(1));
    leftEv->SetValue(1, static_cast<int8_t>(2));
    leftEv->SetValue(2, static_cast<int8_t>(3));
    auto *leftArrVec = new ArrayVector(rowSize, leftElemVec);
    leftArrVec->SetOffset(0, 0);
    leftArrVec->SetOffset(1, 3);
    vectorBatch->Append(leftArrVec);

    auto rightElemVec = std::shared_ptr<BaseVector>(new Vector<int8_t>(2));
    auto *rightEv = dynamic_cast<Vector<int8_t> *>(rightElemVec.get());
    rightEv->SetValue(0, static_cast<int8_t>(2));
    rightEv->SetValue(1, static_cast<int8_t>(4));
    auto *rightArrVec = new ArrayVector(rowSize, rightElemVec);
    rightArrVec->SetOffset(0, 0);
    rightArrVec->SetOffset(1, 2);
    vectorBatch->Append(rightArrVec);

    auto expr = FuncExpr("array_intersect", {
        new FieldExpr(0, arrayType),
        new FieldExpr(1, arrayType)
    }, arrayType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_EQ(resultArray->GetSize(0), 1);

    auto elementVector = resultArray->GetElementVector();
    auto byteVector = dynamic_cast<Vector<int8_t> *>(elementVector.get());
    ASSERT_NE(byteVector, nullptr);
    EXPECT_EQ(byteVector->GetValue(resultArray->GetOffset(0)), 2);

    delete context;
    delete vectorBatch;
    delete result;
}

/// Test: multiple rows
/// Row 0: array_intersect([1, 2, 3], [2, 4]) -> [2]
/// Row 1: array_intersect([10, 20], [10, 20, 30]) -> [10, 20]
/// Row 2: array_intersect([5], [6]) -> []
TEST_F(ArrayIntersectTest, MultipleRows)
{
    int rowSize = 3;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto arrayType = std::make_shared<ArrayType>(intType);

    auto *vectorBatch = new VectorBatch(rowSize);

    // Left: [1,2,3], [10,20], [5]
    auto leftElemVec = std::shared_ptr<BaseVector>(new Vector<int32_t>(6));
    auto *leftEv = dynamic_cast<Vector<int32_t> *>(leftElemVec.get());
    leftEv->SetValue(0, 1);
    leftEv->SetValue(1, 2);
    leftEv->SetValue(2, 3);
    leftEv->SetValue(3, 10);
    leftEv->SetValue(4, 20);
    leftEv->SetValue(5, 5);
    auto *leftArrVec = new ArrayVector(rowSize, leftElemVec);
    leftArrVec->SetOffset(0, 0);
    leftArrVec->SetOffset(1, 3);
    leftArrVec->SetOffset(2, 5);
    leftArrVec->SetOffset(3, 6);
    vectorBatch->Append(leftArrVec);

    // Right: [2,4], [10,20,30], [6]
    auto rightElemVec = std::shared_ptr<BaseVector>(new Vector<int32_t>(6));
    auto *rightEv = dynamic_cast<Vector<int32_t> *>(rightElemVec.get());
    rightEv->SetValue(0, 2);
    rightEv->SetValue(1, 4);
    rightEv->SetValue(2, 10);
    rightEv->SetValue(3, 20);
    rightEv->SetValue(4, 30);
    rightEv->SetValue(5, 6);
    auto *rightArrVec = new ArrayVector(rowSize, rightElemVec);
    rightArrVec->SetOffset(0, 0);
    rightArrVec->SetOffset(1, 2);
    rightArrVec->SetOffset(2, 5);
    rightArrVec->SetOffset(3, 6);
    vectorBatch->Append(rightArrVec);

    auto expr = FuncExpr("array_intersect", {
        new FieldExpr(0, arrayType),
        new FieldExpr(1, arrayType)
    }, arrayType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_EQ(resultArray->GetSize(), rowSize);

    auto elementVector = resultArray->GetElementVector();
    auto intVector = dynamic_cast<Vector<int32_t> *>(elementVector.get());
    ASSERT_NE(intVector, nullptr);

    // Row 0: [2]
    EXPECT_EQ(resultArray->GetSize(0), 1);
    EXPECT_EQ(intVector->GetValue(resultArray->GetOffset(0)), 2);

    // Row 1: [10, 20]
    EXPECT_EQ(resultArray->GetSize(1), 2);
    EXPECT_EQ(intVector->GetValue(resultArray->GetOffset(1)), 10);
    EXPECT_EQ(intVector->GetValue(resultArray->GetOffset(1) + 1), 20);

    // Row 2: []
    EXPECT_EQ(resultArray->GetSize(2), 0);

    delete context;
    delete vectorBatch;
    delete result;
}

/// Test: negative integers
/// array_intersect([1, -2, 3, -4], [-2, -4, 5]) -> [-2, -4]
TEST_F(ArrayIntersectTest, NegativeIntegers)
{
    int rowSize = 1;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto arrayType = std::make_shared<ArrayType>(intType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto leftElemVec = std::shared_ptr<BaseVector>(new Vector<int32_t>(4));
    auto *leftEv = dynamic_cast<Vector<int32_t> *>(leftElemVec.get());
    leftEv->SetValue(0, 1);
    leftEv->SetValue(1, -2);
    leftEv->SetValue(2, 3);
    leftEv->SetValue(3, -4);
    auto *leftArrVec = new ArrayVector(rowSize, leftElemVec);
    leftArrVec->SetOffset(0, 0);
    leftArrVec->SetOffset(1, 4);
    vectorBatch->Append(leftArrVec);

    auto rightElemVec = std::shared_ptr<BaseVector>(new Vector<int32_t>(3));
    auto *rightEv = dynamic_cast<Vector<int32_t> *>(rightElemVec.get());
    rightEv->SetValue(0, -2);
    rightEv->SetValue(1, -4);
    rightEv->SetValue(2, 5);
    auto *rightArrVec = new ArrayVector(rowSize, rightElemVec);
    rightArrVec->SetOffset(0, 0);
    rightArrVec->SetOffset(1, 3);
    vectorBatch->Append(rightArrVec);

    auto expr = FuncExpr("array_intersect", {
        new FieldExpr(0, arrayType),
        new FieldExpr(1, arrayType)
    }, arrayType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_EQ(resultArray->GetSize(0), 2);

    auto elementVector = resultArray->GetElementVector();
    auto intVector = dynamic_cast<Vector<int32_t> *>(elementVector.get());
    ASSERT_NE(intVector, nullptr);
    int32_t rOff = resultArray->GetOffset(0);
    EXPECT_EQ(intVector->GetValue(rOff), -2);
    EXPECT_EQ(intVector->GetValue(rOff + 1), -4);

    delete context;
    delete vectorBatch;
    delete result;
}

/// Test: date32 type intersection (internally int32_t)
/// array_intersect([100, 200, 300], [200, 400]) -> [200]
TEST_F(ArrayIntersectTest, Date32Intersection)
{
    int rowSize = 1;
    auto dateType = std::make_shared<DataType>(OMNI_DATE32);
    auto arrayType = std::make_shared<ArrayType>(dateType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto leftElemVec = std::shared_ptr<BaseVector>(new Vector<int32_t>(3));
    auto *leftEv = dynamic_cast<Vector<int32_t> *>(leftElemVec.get());
    leftEv->SetValue(0, 100);
    leftEv->SetValue(1, 200);
    leftEv->SetValue(2, 300);
    auto *leftArrVec = new ArrayVector(rowSize, leftElemVec);
    leftArrVec->SetOffset(0, 0);
    leftArrVec->SetOffset(1, 3);
    vectorBatch->Append(leftArrVec);

    auto rightElemVec = std::shared_ptr<BaseVector>(new Vector<int32_t>(2));
    auto *rightEv = dynamic_cast<Vector<int32_t> *>(rightElemVec.get());
    rightEv->SetValue(0, 200);
    rightEv->SetValue(1, 400);
    auto *rightArrVec = new ArrayVector(rowSize, rightElemVec);
    rightArrVec->SetOffset(0, 0);
    rightArrVec->SetOffset(1, 2);
    vectorBatch->Append(rightArrVec);

    auto expr = FuncExpr("array_intersect", {
        new FieldExpr(0, arrayType),
        new FieldExpr(1, arrayType)
    }, arrayType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_EQ(resultArray->GetSize(0), 1);

    auto elementVector = resultArray->GetElementVector();
    auto intVector = dynamic_cast<Vector<int32_t> *>(elementVector.get());
    ASSERT_NE(intVector, nullptr);
    EXPECT_EQ(intVector->GetValue(resultArray->GetOffset(0)), 200);

    delete context;
    delete vectorBatch;
    delete result;
}

/// Test: timestamp type intersection (internally int64_t)
/// array_intersect([1000, 2000, 3000], [2000, 4000]) -> [2000]
TEST_F(ArrayIntersectTest, TimestampIntersection)
{
    int rowSize = 1;
    auto tsType = std::make_shared<DataType>(OMNI_TIMESTAMP);
    auto arrayType = std::make_shared<ArrayType>(tsType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto leftElemVec = std::shared_ptr<BaseVector>(new Vector<int64_t>(3));
    auto *leftEv = dynamic_cast<Vector<int64_t> *>(leftElemVec.get());
    leftEv->SetValue(0, 1000L);
    leftEv->SetValue(1, 2000L);
    leftEv->SetValue(2, 3000L);
    auto *leftArrVec = new ArrayVector(rowSize, leftElemVec);
    leftArrVec->SetOffset(0, 0);
    leftArrVec->SetOffset(1, 3);
    vectorBatch->Append(leftArrVec);

    auto rightElemVec = std::shared_ptr<BaseVector>(new Vector<int64_t>(2));
    auto *rightEv = dynamic_cast<Vector<int64_t> *>(rightElemVec.get());
    rightEv->SetValue(0, 2000L);
    rightEv->SetValue(1, 4000L);
    auto *rightArrVec = new ArrayVector(rowSize, rightElemVec);
    rightArrVec->SetOffset(0, 0);
    rightArrVec->SetOffset(1, 2);
    vectorBatch->Append(rightArrVec);

    auto expr = FuncExpr("array_intersect", {
        new FieldExpr(0, arrayType),
        new FieldExpr(1, arrayType)
    }, arrayType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_EQ(resultArray->GetSize(0), 1);

    auto elementVector = resultArray->GetElementVector();
    auto longVector = dynamic_cast<Vector<int64_t> *>(elementVector.get());
    ASSERT_NE(longVector, nullptr);
    EXPECT_EQ(longVector->GetValue(resultArray->GetOffset(0)), 2000L);

    delete context;
    delete vectorBatch;
    delete result;
}

/// Test: decimal128 type intersection
/// array_intersect([100, 200, 300], [200, 400]) -> [200]
TEST_F(ArrayIntersectTest, Decimal128Intersection)
{
    int rowSize = 1;
    auto decimalType = std::make_shared<DataType>(OMNI_DECIMAL128);
    auto arrayType = std::make_shared<ArrayType>(decimalType);

    auto *vectorBatch = new VectorBatch(rowSize);

    Decimal128 d100(100);
    Decimal128 d200(200);
    Decimal128 d300(300);
    Decimal128 d400(400);

    auto leftElemVec = std::shared_ptr<BaseVector>(new Vector<Decimal128>(3));
    auto *leftEv = dynamic_cast<Vector<Decimal128> *>(leftElemVec.get());
    leftEv->SetValue(0, d100);
    leftEv->SetValue(1, d200);
    leftEv->SetValue(2, d300);
    auto *leftArrVec = new ArrayVector(rowSize, leftElemVec);
    leftArrVec->SetOffset(0, 0);
    leftArrVec->SetOffset(1, 3);
    vectorBatch->Append(leftArrVec);

    auto rightElemVec = std::shared_ptr<BaseVector>(new Vector<Decimal128>(2));
    auto *rightEv = dynamic_cast<Vector<Decimal128> *>(rightElemVec.get());
    rightEv->SetValue(0, d200);
    rightEv->SetValue(1, d400);
    auto *rightArrVec = new ArrayVector(rowSize, rightElemVec);
    rightArrVec->SetOffset(0, 0);
    rightArrVec->SetOffset(1, 2);
    vectorBatch->Append(rightArrVec);

    auto expr = FuncExpr("array_intersect", {
        new FieldExpr(0, arrayType),
        new FieldExpr(1, arrayType)
    }, arrayType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_EQ(resultArray->GetSize(0), 1);

    auto elementVector = resultArray->GetElementVector();
    auto decVector = dynamic_cast<Vector<Decimal128> *>(elementVector.get());
    ASSERT_NE(decVector, nullptr);
    EXPECT_EQ(decVector->GetValue(resultArray->GetOffset(0)), d200);

    delete context;
    delete vectorBatch;
    delete result;
}

/// Test: both arrays empty
/// array_intersect([], []) -> []
TEST_F(ArrayIntersectTest, BothEmptyArrays)
{
    int rowSize = 1;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto arrayType = std::make_shared<ArrayType>(intType);

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

    auto expr = FuncExpr("array_intersect", {
        new FieldExpr(0, arrayType),
        new FieldExpr(1, arrayType)
    }, arrayType);

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

/// Test: null element only in left array (not in right)
/// array_intersect([1, null, 3], [1, 3]) -> [1, 3]
TEST_F(ArrayIntersectTest, NullOnlyInLeft)
{
    int rowSize = 1;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto arrayType = std::make_shared<ArrayType>(intType);

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

    auto rightElemVec = std::shared_ptr<BaseVector>(new Vector<int32_t>(2));
    auto *rightEv = dynamic_cast<Vector<int32_t> *>(rightElemVec.get());
    rightEv->SetValue(0, 1);
    rightEv->SetValue(1, 3);
    auto *rightArrVec = new ArrayVector(rowSize, rightElemVec);
    rightArrVec->SetOffset(0, 0);
    rightArrVec->SetOffset(1, 2);
    vectorBatch->Append(rightArrVec);

    auto expr = FuncExpr("array_intersect", {
        new FieldExpr(0, arrayType),
        new FieldExpr(1, arrayType)
    }, arrayType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_EQ(resultArray->GetSize(0), 2);

    auto elementVector = resultArray->GetElementVector();
    auto intVector = dynamic_cast<Vector<int32_t> *>(elementVector.get());
    ASSERT_NE(intVector, nullptr);
    int32_t rOff = resultArray->GetOffset(0);
    EXPECT_EQ(intVector->GetValue(rOff), 1);
    EXPECT_EQ(intVector->GetValue(rOff + 1), 3);

    delete context;
    delete vectorBatch;
    delete result;
}

/// Test: float infinity values
/// array_intersect([inf, -inf, 1.0], [inf, 2.0]) -> [inf]
TEST_F(ArrayIntersectTest, FloatInfinity)
{
    int rowSize = 1;
    auto floatType = std::make_shared<DataType>(OMNI_FLOAT);
    auto arrayType = std::make_shared<ArrayType>(floatType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto leftElemVec = std::shared_ptr<BaseVector>(new Vector<float>(3));
    auto *leftEv = dynamic_cast<Vector<float> *>(leftElemVec.get());
    leftEv->SetValue(0, std::numeric_limits<float>::infinity());
    leftEv->SetValue(1, -std::numeric_limits<float>::infinity());
    leftEv->SetValue(2, 1.0f);
    auto *leftArrVec = new ArrayVector(rowSize, leftElemVec);
    leftArrVec->SetOffset(0, 0);
    leftArrVec->SetOffset(1, 3);
    vectorBatch->Append(leftArrVec);

    auto rightElemVec = std::shared_ptr<BaseVector>(new Vector<float>(2));
    auto *rightEv = dynamic_cast<Vector<float> *>(rightElemVec.get());
    rightEv->SetValue(0, std::numeric_limits<float>::infinity());
    rightEv->SetValue(1, 2.0f);
    auto *rightArrVec = new ArrayVector(rowSize, rightElemVec);
    rightArrVec->SetOffset(0, 0);
    rightArrVec->SetOffset(1, 2);
    vectorBatch->Append(rightArrVec);

    auto expr = FuncExpr("array_intersect", {
        new FieldExpr(0, arrayType),
        new FieldExpr(1, arrayType)
    }, arrayType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_EQ(resultArray->GetSize(0), 1);

    auto elementVector = resultArray->GetElementVector();
    auto fltVector = dynamic_cast<Vector<float> *>(elementVector.get());
    ASSERT_NE(fltVector, nullptr);
    EXPECT_EQ(fltVector->GetValue(resultArray->GetOffset(0)), std::numeric_limits<float>::infinity());

    delete context;
    delete vectorBatch;
    delete result;
}

/// Test: multiple rows with mixed null rows and null elements
/// Row 0: array_intersect([1, null, 3], [null, 3]) -> [null, 3]
/// Row 1: array_intersect(null, [1, 2]) -> null
/// Row 2: array_intersect([5, 6], [5, 7]) -> [5]
TEST_F(ArrayIntersectTest, MultipleRowsMixedNulls)
{
    int rowSize = 3;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto arrayType = std::make_shared<ArrayType>(intType);

    auto *vectorBatch = new VectorBatch(rowSize);

    // Left: [1, null, 3] (3 elems), null (0 elems), [5, 6] (2 elems)
    auto leftElemVec = std::shared_ptr<BaseVector>(new Vector<int32_t>(5));
    auto *leftEv = dynamic_cast<Vector<int32_t> *>(leftElemVec.get());
    leftEv->SetValue(0, 1);
    leftElemVec->SetNull(1);
    leftEv->SetValue(2, 3);
    leftEv->SetValue(3, 5);
    leftEv->SetValue(4, 6);
    auto *leftArrVec = new ArrayVector(rowSize, leftElemVec);
    leftArrVec->SetOffset(0, 0);
    leftArrVec->SetOffset(1, 3);
    leftArrVec->SetOffset(2, 3);
    leftArrVec->SetOffset(3, 5);
    leftArrVec->SetNull(1);
    vectorBatch->Append(leftArrVec);

    // Right: [null, 3] (2 elems), [1, 2] (2 elems), [5, 7] (2 elems)
    auto rightElemVec = std::shared_ptr<BaseVector>(new Vector<int32_t>(6));
    auto *rightEv = dynamic_cast<Vector<int32_t> *>(rightElemVec.get());
    rightElemVec->SetNull(0);
    rightEv->SetValue(1, 3);
    rightEv->SetValue(2, 1);
    rightEv->SetValue(3, 2);
    rightEv->SetValue(4, 5);
    rightEv->SetValue(5, 7);
    auto *rightArrVec = new ArrayVector(rowSize, rightElemVec);
    rightArrVec->SetOffset(0, 0);
    rightArrVec->SetOffset(1, 2);
    rightArrVec->SetOffset(2, 4);
    rightArrVec->SetOffset(3, 6);
    vectorBatch->Append(rightArrVec);

    auto expr = FuncExpr("array_intersect", {
        new FieldExpr(0, arrayType),
        new FieldExpr(1, arrayType)
    }, arrayType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);

    auto elementVector = resultArray->GetElementVector();
    auto intVector = dynamic_cast<Vector<int32_t> *>(elementVector.get());

    // Row 0: [null, 3]
    EXPECT_FALSE(resultArray->IsNull(0));
    EXPECT_EQ(resultArray->GetSize(0), 2);
    int32_t r0Off = resultArray->GetOffset(0);
    EXPECT_TRUE(elementVector->IsNull(r0Off));
    EXPECT_EQ(intVector->GetValue(r0Off + 1), 3);

    // Row 1: null
    EXPECT_TRUE(resultArray->IsNull(1));

    // Row 2: [5]
    EXPECT_FALSE(resultArray->IsNull(2));
    EXPECT_EQ(resultArray->GetSize(2), 1);
    EXPECT_EQ(intVector->GetValue(resultArray->GetOffset(2)), 5);

    delete context;
    delete vectorBatch;
    delete result;
}
