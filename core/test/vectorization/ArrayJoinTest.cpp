/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: ArrayJoin function test
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

class ArrayJoinTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

// ==================== INT array tests ====================

/// Test: array_join with int array, no null replacement
/// array_join([1, 2, 3], ',') -> "1,2,3"
TEST_F(ArrayJoinTest, IntArrayNoReplacement)
{
    int rowSize = 1;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto varcharType = VarcharType(10);
    auto arrayType = std::make_shared<ArrayType>(intType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto elemVec = std::shared_ptr<BaseVector>(new Vector<int32_t>(3));
    auto *ev = dynamic_cast<Vector<int32_t> *>(elemVec.get());
    ev->SetValue(0, 1);
    ev->SetValue(1, 2);
    ev->SetValue(2, 3);
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 3);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("array_join", {
        new FieldExpr(0, arrayType),
        new LiteralExpr(new std::string(","), varcharType)
    }, varcharType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *strResult = dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(result);
    ASSERT_NE(strResult, nullptr);
    EXPECT_FALSE(strResult->IsNull(0));
    std::string_view sv = strResult->GetValue(0);
    EXPECT_EQ(std::string(sv.data(), sv.size()), "1,2,3");

    delete context;
    delete vectorBatch;
    delete result;
}

/// Test: array_join with int array containing nulls, no replacement
/// array_join([1, null, 3], ',') -> "1,3"
TEST_F(ArrayJoinTest, IntArrayWithNullNoReplacement)
{
    int rowSize = 1;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto varcharType = VarcharType(10);
    auto arrayType = std::make_shared<ArrayType>(intType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto elemVec = std::shared_ptr<BaseVector>(new Vector<int32_t>(3));
    auto *ev = dynamic_cast<Vector<int32_t> *>(elemVec.get());
    ev->SetValue(0, 1);
    ev->SetNull(1);
    ev->SetValue(2, 3);
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 3);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("array_join", {
        new FieldExpr(0, arrayType),
        new LiteralExpr(new std::string(","), varcharType)
    }, varcharType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *strResult = dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(result);
    ASSERT_NE(strResult, nullptr);
    EXPECT_FALSE(strResult->IsNull(0));
    std::string_view sv = strResult->GetValue(0);
    EXPECT_EQ(std::string(sv.data(), sv.size()), "1,3");

    delete context;
    delete vectorBatch;
    delete result;
}

/// Test: array_join with int array containing nulls, with null replacement
/// array_join([1, null, 3], ',', '0') -> "1,0,3"
TEST_F(ArrayJoinTest, IntArrayWithNullReplacement)
{
    int rowSize = 1;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto varcharType = VarcharType(10);
    auto arrayType = std::make_shared<ArrayType>(intType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto elemVec = std::shared_ptr<BaseVector>(new Vector<int32_t>(3));
    auto *ev = dynamic_cast<Vector<int32_t> *>(elemVec.get());
    ev->SetValue(0, 1);
    ev->SetNull(1);
    ev->SetValue(2, 3);
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 3);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("array_join", {
        new FieldExpr(0, arrayType),
        new LiteralExpr(new std::string(","), varcharType),
        new LiteralExpr(new std::string("0"), varcharType)
    }, varcharType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *strResult = dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(result);
    ASSERT_NE(strResult, nullptr);
    EXPECT_FALSE(strResult->IsNull(0));
    std::string_view sv = strResult->GetValue(0);
    EXPECT_EQ(std::string(sv.data(), sv.size()), "1,0,3");

    delete context;
    delete vectorBatch;
    delete result;
}

// ==================== BYTE array tests ====================

/// Test: array_join with byte array
/// array_join([1, 2, 3], ',') -> "1,2,3"
TEST_F(ArrayJoinTest, ByteArrayNoReplacement)
{
    int rowSize = 1;
    auto byteType = std::make_shared<DataType>(OMNI_BYTE);
    auto varcharType = VarcharType(10);
    auto arrayType = std::make_shared<ArrayType>(byteType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto elemVec = std::shared_ptr<BaseVector>(new Vector<int8_t>(3));
    auto *ev = dynamic_cast<Vector<int8_t> *>(elemVec.get());
    ev->SetValue(0, 1);
    ev->SetValue(1, 2);
    ev->SetValue(2, 3);
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 3);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("array_join", {
        new FieldExpr(0, arrayType),
        new LiteralExpr(new std::string(","), varcharType)
    }, varcharType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *strResult = dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(result);
    ASSERT_NE(strResult, nullptr);
    EXPECT_FALSE(strResult->IsNull(0));
    std::string_view sv = strResult->GetValue(0);
    EXPECT_EQ(std::string(sv.data(), sv.size()), "1,2,3");

    delete context;
    delete vectorBatch;
    delete result;
}

// ==================== SHORT array tests ====================

/// Test: array_join with short array
/// array_join([100, 200, 300], '-') -> "100-200-300"
TEST_F(ArrayJoinTest, ShortArrayNoReplacement)
{
    int rowSize = 1;
    auto shortType = std::make_shared<DataType>(OMNI_SHORT);
    auto varcharType = VarcharType(20);
    auto arrayType = std::make_shared<ArrayType>(shortType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto elemVec = std::shared_ptr<BaseVector>(new Vector<int16_t>(3));
    auto *ev = dynamic_cast<Vector<int16_t> *>(elemVec.get());
    ev->SetValue(0, 100);
    ev->SetValue(1, 200);
    ev->SetValue(2, 300);
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 3);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("array_join", {
        new FieldExpr(0, arrayType),
        new LiteralExpr(new std::string("-"), varcharType)
    }, varcharType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *strResult = dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(result);
    ASSERT_NE(strResult, nullptr);
    EXPECT_FALSE(strResult->IsNull(0));
    std::string_view sv = strResult->GetValue(0);
    EXPECT_EQ(std::string(sv.data(), sv.size()), "100-200-300");

    delete context;
    delete vectorBatch;
    delete result;
}

// ==================== LONG array tests ====================

/// Test: array_join with long array
/// array_join([100000, 200000, 300000], ',') -> "100000,200000,300000"
TEST_F(ArrayJoinTest, LongArrayNoReplacement)
{
    int rowSize = 1;
    auto longType = std::make_shared<DataType>(OMNI_LONG);
    auto varcharType = VarcharType(30);
    auto arrayType = std::make_shared<ArrayType>(longType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto elemVec = std::shared_ptr<BaseVector>(new Vector<int64_t>(3));
    auto *ev = dynamic_cast<Vector<int64_t> *>(elemVec.get());
    ev->SetValue(0, 100000LL);
    ev->SetValue(1, 200000LL);
    ev->SetValue(2, 300000LL);
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 3);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("array_join", {
        new FieldExpr(0, arrayType),
        new LiteralExpr(new std::string(","), varcharType)
    }, varcharType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *strResult = dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(result);
    ASSERT_NE(strResult, nullptr);
    EXPECT_FALSE(strResult->IsNull(0));
    std::string_view sv = strResult->GetValue(0);
    EXPECT_EQ(std::string(sv.data(), sv.size()), "100000,200000,300000");

    delete context;
    delete vectorBatch;
    delete result;
}

// ==================== FLOAT array tests ====================

/// Test: array_join with float array
/// array_join([1.5, 2.5, 3.5], ',') -> "1.5,2.5,3.5"
TEST_F(ArrayJoinTest, FloatArrayNoReplacement)
{
    int rowSize = 1;
    auto floatType = std::make_shared<DataType>(OMNI_FLOAT);
    auto varcharType = VarcharType(30);
    auto arrayType = std::make_shared<ArrayType>(floatType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto elemVec = std::shared_ptr<BaseVector>(new Vector<float>(3));
    auto *ev = dynamic_cast<Vector<float> *>(elemVec.get());
    ev->SetValue(0, 1.5f);
    ev->SetValue(1, 2.5f);
    ev->SetValue(2, 3.5f);
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 3);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("array_join", {
        new FieldExpr(0, arrayType),
        new LiteralExpr(new std::string(","), varcharType)
    }, varcharType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *strResult = dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(result);
    ASSERT_NE(strResult, nullptr);
    EXPECT_FALSE(strResult->IsNull(0));
    std::string_view sv = strResult->GetValue(0);
    EXPECT_EQ(std::string(sv.data(), sv.size()), "1.5,2.5,3.5");

    delete context;
    delete vectorBatch;
    delete result;
}

// ==================== DOUBLE array tests ====================

/// Test: array_join with double array
/// array_join([1.1, 2.2, 3.3], ',') -> "1.1,2.2,3.3"
TEST_F(ArrayJoinTest, DoubleArrayNoReplacement)
{
    int rowSize = 1;
    auto doubleType = std::make_shared<DataType>(OMNI_DOUBLE);
    auto varcharType = VarcharType(30);
    auto arrayType = std::make_shared<ArrayType>(doubleType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto elemVec = std::shared_ptr<BaseVector>(new Vector<double>(3));
    auto *ev = dynamic_cast<Vector<double> *>(elemVec.get());
    ev->SetValue(0, 1.1);
    ev->SetValue(1, 2.2);
    ev->SetValue(2, 3.3);
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 3);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("array_join", {
        new FieldExpr(0, arrayType),
        new LiteralExpr(new std::string(","), varcharType)
    }, varcharType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *strResult = dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(result);
    ASSERT_NE(strResult, nullptr);
    EXPECT_FALSE(strResult->IsNull(0));
    std::string_view sv = strResult->GetValue(0);
    EXPECT_EQ(std::string(sv.data(), sv.size()), "1.1,2.2,3.3");

    delete context;
    delete vectorBatch;
    delete result;
}

// ==================== BOOLEAN array tests ====================

/// Test: array_join with boolean array
/// array_join([true, false, true], ',') -> "true,false,true"
TEST_F(ArrayJoinTest, BoolArrayNoReplacement)
{
    int rowSize = 1;
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto varcharType = VarcharType(30);
    auto arrayType = std::make_shared<ArrayType>(boolType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto elemVec = std::shared_ptr<BaseVector>(new Vector<bool>(3));
    auto *ev = dynamic_cast<Vector<bool> *>(elemVec.get());
    ev->SetValue(0, true);
    ev->SetValue(1, false);
    ev->SetValue(2, true);
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 3);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("array_join", {
        new FieldExpr(0, arrayType),
        new LiteralExpr(new std::string(","), varcharType)
    }, varcharType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *strResult = dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(result);
    ASSERT_NE(strResult, nullptr);
    EXPECT_FALSE(strResult->IsNull(0));
    std::string_view sv = strResult->GetValue(0);
    EXPECT_EQ(std::string(sv.data(), sv.size()), "true,false,true");

    delete context;
    delete vectorBatch;
    delete result;
}

/// Test: array_join with boolean array with null and replacement
/// array_join([true, null, false], ',', 'apple') -> "true,apple,false"
TEST_F(ArrayJoinTest, BoolArrayWithNullReplacement)
{
    int rowSize = 1;
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto varcharType = VarcharType(30);
    auto arrayType = std::make_shared<ArrayType>(boolType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto elemVec = std::shared_ptr<BaseVector>(new Vector<bool>(3));
    auto *ev = dynamic_cast<Vector<bool> *>(elemVec.get());
    ev->SetValue(0, true);
    ev->SetNull(1);
    ev->SetValue(2, false);
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 3);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("array_join", {
        new FieldExpr(0, arrayType),
        new LiteralExpr(new std::string(","), varcharType),
        new LiteralExpr(new std::string("apple"), varcharType)
    }, varcharType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *strResult = dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(result);
    ASSERT_NE(strResult, nullptr);
    EXPECT_FALSE(strResult->IsNull(0));
    std::string_view sv = strResult->GetValue(0);
    EXPECT_EQ(std::string(sv.data(), sv.size()), "true,apple,false");

    delete context;
    delete vectorBatch;
    delete result;
}

// ==================== VARCHAR array tests ====================

/// Test: array_join with varchar array
/// array_join(["a", "b", "c"], '-') -> "a-b-c"
TEST_F(ArrayJoinTest, VarcharArrayNoReplacement)
{
    int rowSize = 1;
    auto varcharElemType = VarcharType(10);
    auto varcharType = VarcharType(30);
    auto arrayType = std::make_shared<ArrayType>(varcharElemType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto elemVec = std::shared_ptr<BaseVector>(
        new Vector<LargeStringContainer<std::string_view>>(3));
    auto *ev = dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(elemVec.get());
    ev->SetValue(0, std::string_view("a"));
    ev->SetValue(1, std::string_view("b"));
    ev->SetValue(2, std::string_view("c"));
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 3);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("array_join", {
        new FieldExpr(0, arrayType),
        new LiteralExpr(new std::string("-"), varcharType)
    }, varcharType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *strResult = dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(result);
    ASSERT_NE(strResult, nullptr);
    EXPECT_FALSE(strResult->IsNull(0));
    std::string_view sv = strResult->GetValue(0);
    EXPECT_EQ(std::string(sv.data(), sv.size()), "a-b-c");

    delete context;
    delete vectorBatch;
    delete result;
}

/// Test: array_join with varchar array containing nulls and replacement
/// array_join(["a", null, "c"], '-', 'z') -> "a-z-c"
TEST_F(ArrayJoinTest, VarcharArrayWithNullReplacement)
{
    int rowSize = 1;
    auto varcharElemType = VarcharType(10);
    auto varcharType = VarcharType(30);
    auto arrayType = std::make_shared<ArrayType>(varcharElemType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto elemVec = std::shared_ptr<BaseVector>(
        new Vector<LargeStringContainer<std::string_view>>(3));
    auto *ev = dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(elemVec.get());
    ev->SetValue(0, std::string_view("a"));
    ev->SetNull(1);
    ev->SetValue(2, std::string_view("c"));
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 3);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("array_join", {
        new FieldExpr(0, arrayType),
        new LiteralExpr(new std::string("-"), varcharType),
        new LiteralExpr(new std::string("z"), varcharType)
    }, varcharType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *strResult = dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(result);
    ASSERT_NE(strResult, nullptr);
    EXPECT_FALSE(strResult->IsNull(0));
    std::string_view sv = strResult->GetValue(0);
    EXPECT_EQ(std::string(sv.data(), sv.size()), "a-z-c");

    delete context;
    delete vectorBatch;
    delete result;
}

// ==================== DECIMAL128 array tests ====================

/// Test: array_join with decimal128 array
TEST_F(ArrayJoinTest, Decimal128ArrayNoReplacement)
{
    int rowSize = 1;
    auto decimal128Type = Decimal128Type(38, 0);
    auto varcharType = VarcharType(60);
    auto arrayType = std::make_shared<ArrayType>(decimal128Type);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto elemVec = std::shared_ptr<BaseVector>(new Vector<Decimal128>(3));
    auto *ev = dynamic_cast<Vector<Decimal128> *>(elemVec.get());
    ev->SetValue(0, Decimal128(100));
    ev->SetValue(1, Decimal128(200));
    ev->SetValue(2, Decimal128(300));
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 3);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("array_join", {
        new FieldExpr(0, arrayType),
        new LiteralExpr(new std::string(","), varcharType)
    }, varcharType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *strResult = dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(result);
    ASSERT_NE(strResult, nullptr);
    EXPECT_FALSE(strResult->IsNull(0));
    std::string_view sv = strResult->GetValue(0);
    std::string resultStr(sv.data(), sv.size());
    EXPECT_FALSE(resultStr.empty());

    delete context;
    delete vectorBatch;
    delete result;
}

// ==================== Edge case tests ====================

/// Test: array_join with empty array
/// array_join([], ',') -> ""
TEST_F(ArrayJoinTest, EmptyArray)
{
    int rowSize = 1;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto varcharType = VarcharType(10);
    auto arrayType = std::make_shared<ArrayType>(intType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto elemVec = std::shared_ptr<BaseVector>(new Vector<int32_t>(0));
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 0);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("array_join", {
        new FieldExpr(0, arrayType),
        new LiteralExpr(new std::string(","), varcharType)
    }, varcharType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *strResult = dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(result);
    ASSERT_NE(strResult, nullptr);
    EXPECT_FALSE(strResult->IsNull(0));
    std::string_view sv = strResult->GetValue(0);
    EXPECT_EQ(std::string(sv.data(), sv.size()), "");

    delete context;
    delete vectorBatch;
    delete result;
}

/// Test: array_join with single element
/// array_join([42], ',') -> "42"
TEST_F(ArrayJoinTest, SingleElement)
{
    int rowSize = 1;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto varcharType = VarcharType(10);
    auto arrayType = std::make_shared<ArrayType>(intType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto elemVec = std::shared_ptr<BaseVector>(new Vector<int32_t>(1));
    auto *ev = dynamic_cast<Vector<int32_t> *>(elemVec.get());
    ev->SetValue(0, 42);
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 1);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("array_join", {
        new FieldExpr(0, arrayType),
        new LiteralExpr(new std::string(","), varcharType)
    }, varcharType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *strResult = dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(result);
    ASSERT_NE(strResult, nullptr);
    EXPECT_FALSE(strResult->IsNull(0));
    std::string_view sv = strResult->GetValue(0);
    EXPECT_EQ(std::string(sv.data(), sv.size()), "42");

    delete context;
    delete vectorBatch;
    delete result;
}

/// Test: array_join with null array returns null
TEST_F(ArrayJoinTest, NullArray)
{
    int rowSize = 1;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto varcharType = VarcharType(10);
    auto arrayType = std::make_shared<ArrayType>(intType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto elemVec = std::shared_ptr<BaseVector>(new Vector<int32_t>(0));
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 0);
    arrVec->SetNull(0);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("array_join", {
        new FieldExpr(0, arrayType),
        new LiteralExpr(new std::string(","), varcharType)
    }, varcharType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *strResult = dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(result);
    ASSERT_NE(strResult, nullptr);
    EXPECT_TRUE(strResult->IsNull(0));

    delete context;
    delete vectorBatch;
    delete result;
}

/// Test: array_join with all null elements and no replacement
/// array_join([null, null], ',') -> ""
TEST_F(ArrayJoinTest, AllNullElementsNoReplacement)
{
    int rowSize = 1;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto varcharType = VarcharType(10);
    auto arrayType = std::make_shared<ArrayType>(intType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto elemVec = std::shared_ptr<BaseVector>(new Vector<int32_t>(2));
    auto *ev = dynamic_cast<Vector<int32_t> *>(elemVec.get());
    ev->SetNull(0);
    ev->SetNull(1);
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 2);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("array_join", {
        new FieldExpr(0, arrayType),
        new LiteralExpr(new std::string(","), varcharType)
    }, varcharType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *strResult = dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(result);
    ASSERT_NE(strResult, nullptr);
    EXPECT_FALSE(strResult->IsNull(0));
    std::string_view sv = strResult->GetValue(0);
    EXPECT_EQ(std::string(sv.data(), sv.size()), "");

    delete context;
    delete vectorBatch;
    delete result;
}

/// Test: array_join with all null elements and replacement
/// array_join([null, null, null], ',', 'X') -> "X,X,X"
TEST_F(ArrayJoinTest, AllNullElementsWithReplacement)
{
    int rowSize = 1;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto varcharType = VarcharType(10);
    auto arrayType = std::make_shared<ArrayType>(intType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto elemVec = std::shared_ptr<BaseVector>(new Vector<int32_t>(3));
    auto *ev = dynamic_cast<Vector<int32_t> *>(elemVec.get());
    ev->SetNull(0);
    ev->SetNull(1);
    ev->SetNull(2);
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 3);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("array_join", {
        new FieldExpr(0, arrayType),
        new LiteralExpr(new std::string(","), varcharType),
        new LiteralExpr(new std::string("X"), varcharType)
    }, varcharType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *strResult = dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(result);
    ASSERT_NE(strResult, nullptr);
    EXPECT_FALSE(strResult->IsNull(0));
    std::string_view sv = strResult->GetValue(0);
    EXPECT_EQ(std::string(sv.data(), sv.size()), "X,X,X");

    delete context;
    delete vectorBatch;
    delete result;
}

// ==================== Multi-row tests ====================

/// Test: array_join with multiple rows
/// Row 0: array_join([1, 2], ',') -> "1,2"
/// Row 1: array_join([3, 4, 5], ',') -> "3,4,5"
TEST_F(ArrayJoinTest, MultipleRows)
{
    int rowSize = 2;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto varcharType = VarcharType(20);
    auto arrayType = std::make_shared<ArrayType>(intType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto elemVec = std::shared_ptr<BaseVector>(new Vector<int32_t>(5));
    auto *ev = dynamic_cast<Vector<int32_t> *>(elemVec.get());
    ev->SetValue(0, 1);
    ev->SetValue(1, 2);
    ev->SetValue(2, 3);
    ev->SetValue(3, 4);
    ev->SetValue(4, 5);
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 2);
    arrVec->SetOffset(2, 5);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("array_join", {
        new FieldExpr(0, arrayType),
        new LiteralExpr(new std::string(","), varcharType)
    }, varcharType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *strResult = dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(result);
    ASSERT_NE(strResult, nullptr);

    std::string_view sv0 = strResult->GetValue(0);
    EXPECT_EQ(std::string(sv0.data(), sv0.size()), "1,2");

    std::string_view sv1 = strResult->GetValue(1);
    EXPECT_EQ(std::string(sv1.data(), sv1.size()), "3,4,5");

    delete context;
    delete vectorBatch;
    delete result;
}

// ==================== DATE32 array tests ====================

/// Test: array_join with date32 array
TEST_F(ArrayJoinTest, Date32ArrayNoReplacement)
{
    int rowSize = 1;
    auto dateType = std::make_shared<DataType>(OMNI_DATE32);
    auto varcharType = VarcharType(40);
    auto arrayType = std::make_shared<ArrayType>(dateType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto elemVec = std::shared_ptr<BaseVector>(new Vector<int32_t>(2));
    auto *ev = dynamic_cast<Vector<int32_t> *>(elemVec.get());
    ev->SetValue(0, 0);
    ev->SetValue(1, 1);
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 2);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("array_join", {
        new FieldExpr(0, arrayType),
        new LiteralExpr(new std::string(","), varcharType)
    }, varcharType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *strResult = dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(result);
    ASSERT_NE(strResult, nullptr);
    EXPECT_FALSE(strResult->IsNull(0));
    std::string_view sv = strResult->GetValue(0);
    std::string resultStr(sv.data(), sv.size());
    EXPECT_FALSE(resultStr.empty());

    delete context;
    delete vectorBatch;
    delete result;
}

// ==================== TIMESTAMP array tests ====================

/// Test: array_join with timestamp array (OMNI_LONG)
TEST_F(ArrayJoinTest, TimestampArrayNoReplacement)
{
    int rowSize = 1;
    auto longType = std::make_shared<DataType>(OMNI_TIMESTAMP);
    auto varcharType = VarcharType(80);
    auto arrayType = std::make_shared<ArrayType>(longType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto elemVec = std::shared_ptr<BaseVector>(new Vector<int64_t>(2));
    auto *ev = dynamic_cast<Vector<int64_t> *>(elemVec.get());
    ev->SetValue(0, 0LL);
    ev->SetValue(1, 1000000LL);
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 2);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("array_join", {
        new FieldExpr(0, arrayType),
        new LiteralExpr(new std::string("~"), varcharType)
    }, varcharType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *strResult = dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(result);
    ASSERT_NE(strResult, nullptr);
    EXPECT_FALSE(strResult->IsNull(0));
    std::string_view sv = strResult->GetValue(0);
    std::string resultStr(sv.data(), sv.size());
    EXPECT_TRUE(resultStr.find("~") != std::string::npos);

    delete context;
    delete vectorBatch;
    delete result;
}

// ==================== VARBINARY array tests ====================

/// Test: array_join with varbinary array
TEST_F(ArrayJoinTest, VarbinaryArrayNoReplacement)
{
    int rowSize = 1;
    auto varbinaryType = VarBinaryType(10);
    auto varcharType = VarcharType(30);
    auto arrayType = std::make_shared<ArrayType>(varbinaryType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto elemVec = std::shared_ptr<BaseVector>(
        new Vector<LargeStringContainer<std::string_view>>(2));
    auto *ev = dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(elemVec.get());
    ev->SetValue(0, std::string_view("abc"));
    ev->SetValue(1, std::string_view("def"));
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 2);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("array_join", {
        new FieldExpr(0, arrayType),
        new LiteralExpr(new std::string(","), varcharType)
    }, varcharType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *strResult = dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(result);
    ASSERT_NE(strResult, nullptr);
    EXPECT_FALSE(strResult->IsNull(0));
    std::string_view sv = strResult->GetValue(0);
    EXPECT_EQ(std::string(sv.data(), sv.size()), "abc,def");

    delete context;
    delete vectorBatch;
    delete result;
}

// ==================== DECIMAL64 array tests ====================

/// Test: array_join with decimal64 array
TEST_F(ArrayJoinTest, Decimal64ArrayNoReplacement)
{
    int rowSize = 1;
    auto decimal64Type = std::make_shared<DataType>(OMNI_DECIMAL64);
    auto varcharType = VarcharType(40);
    auto arrayType = std::make_shared<ArrayType>(decimal64Type);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto elemVec = std::shared_ptr<BaseVector>(new Vector<int64_t>(3));
    auto *ev = dynamic_cast<Vector<int64_t> *>(elemVec.get());
    ev->SetValue(0, 100LL);
    ev->SetValue(1, 200LL);
    ev->SetValue(2, 300LL);
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 3);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("array_join", {
        new FieldExpr(0, arrayType),
        new LiteralExpr(new std::string(","), varcharType)
    }, varcharType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *strResult = dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(result);
    ASSERT_NE(strResult, nullptr);
    EXPECT_FALSE(strResult->IsNull(0));
    std::string_view sv = strResult->GetValue(0);
    EXPECT_EQ(std::string(sv.data(), sv.size()), "100,200,300");

    delete context;
    delete vectorBatch;
    delete result;
}

// ==================== Multi-character delimiter tests ====================

/// Test: array_join with multi-character delimiter
/// array_join([1, 2, 3], ' - ') -> "1 - 2 - 3"
TEST_F(ArrayJoinTest, MultiCharDelimiter)
{
    int rowSize = 1;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto varcharType = VarcharType(30);
    auto arrayType = std::make_shared<ArrayType>(intType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto elemVec = std::shared_ptr<BaseVector>(new Vector<int32_t>(3));
    auto *ev = dynamic_cast<Vector<int32_t> *>(elemVec.get());
    ev->SetValue(0, 1);
    ev->SetValue(1, 2);
    ev->SetValue(2, 3);
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 3);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("array_join", {
        new FieldExpr(0, arrayType),
        new LiteralExpr(new std::string(" - "), varcharType)
    }, varcharType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *strResult = dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(result);
    ASSERT_NE(strResult, nullptr);
    EXPECT_FALSE(strResult->IsNull(0));
    std::string_view sv = strResult->GetValue(0);
    EXPECT_EQ(std::string(sv.data(), sv.size()), "1 - 2 - 3");

    delete context;
    delete vectorBatch;
    delete result;
}

/// Test: array_join with empty delimiter
/// array_join([1, 2, 3], '') -> "123"
TEST_F(ArrayJoinTest, EmptyDelimiter)
{
    int rowSize = 1;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto varcharType = VarcharType(10);
    auto arrayType = std::make_shared<ArrayType>(intType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto elemVec = std::shared_ptr<BaseVector>(new Vector<int32_t>(3));
    auto *ev = dynamic_cast<Vector<int32_t> *>(elemVec.get());
    ev->SetValue(0, 1);
    ev->SetValue(1, 2);
    ev->SetValue(2, 3);
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 3);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("array_join", {
        new FieldExpr(0, arrayType),
        new LiteralExpr(new std::string(""), varcharType)
    }, varcharType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *strResult = dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(result);
    ASSERT_NE(strResult, nullptr);
    EXPECT_FALSE(strResult->IsNull(0));
    std::string_view sv = strResult->GetValue(0);
    EXPECT_EQ(std::string(sv.data(), sv.size()), "123");

    delete context;
    delete vectorBatch;
    delete result;
}
