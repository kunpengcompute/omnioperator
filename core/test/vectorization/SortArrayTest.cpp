/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: SortArray function test
 */

#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
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

class SortArrayTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

// ==================== INT array tests ====================

TEST_F(SortArrayTest, IntArrayAscending)
{
    int rowSize = 1;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto arrayIntType = std::make_shared<ArrayType>(intType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto elemVec = std::shared_ptr<BaseVector>(new Vector<int32_t>(5));
    auto *ev = dynamic_cast<Vector<int32_t> *>(elemVec.get());
    ev->SetValue(0, 5);
    ev->SetValue(1, 3);
    ev->SetValue(2, 1);
    ev->SetValue(3, 4);
    ev->SetValue(4, 2);
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 5);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("sort_array", {
        new FieldExpr(0, arrayIntType),
        new LiteralExpr(true, boolType)
    }, arrayIntType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_FALSE(resultArray->IsNull(0));
    EXPECT_EQ(resultArray->GetSize(0), 5);

    auto resultElemVec = resultArray->GetElementVector();
    auto *resultEv = dynamic_cast<Vector<int32_t> *>(resultElemVec.get());
    ASSERT_NE(resultEv, nullptr);

    int32_t rOff = static_cast<int32_t>(resultArray->GetOffset(0));
    EXPECT_EQ(resultEv->GetValue(rOff + 0), 1);
    EXPECT_EQ(resultEv->GetValue(rOff + 1), 2);
    EXPECT_EQ(resultEv->GetValue(rOff + 2), 3);
    EXPECT_EQ(resultEv->GetValue(rOff + 3), 4);
    EXPECT_EQ(resultEv->GetValue(rOff + 4), 5);

    delete context;
    delete vectorBatch;
    delete result;
}

TEST_F(SortArrayTest, IntArrayDescending)
{
    int rowSize = 1;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto arrayIntType = std::make_shared<ArrayType>(intType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto elemVec = std::shared_ptr<BaseVector>(new Vector<int32_t>(5));
    auto *ev = dynamic_cast<Vector<int32_t> *>(elemVec.get());
    ev->SetValue(0, 5);
    ev->SetValue(1, 3);
    ev->SetValue(2, 1);
    ev->SetValue(3, 4);
    ev->SetValue(4, 2);
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 5);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("sort_array", {
        new FieldExpr(0, arrayIntType),
        new LiteralExpr(false, boolType)
    }, arrayIntType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_FALSE(resultArray->IsNull(0));
    EXPECT_EQ(resultArray->GetSize(0), 5);

    auto resultElemVec = resultArray->GetElementVector();
    auto *resultEv = dynamic_cast<Vector<int32_t> *>(resultElemVec.get());
    ASSERT_NE(resultEv, nullptr);

    int32_t rOff = static_cast<int32_t>(resultArray->GetOffset(0));
    EXPECT_EQ(resultEv->GetValue(rOff + 0), 5);
    EXPECT_EQ(resultEv->GetValue(rOff + 1), 4);
    EXPECT_EQ(resultEv->GetValue(rOff + 2), 3);
    EXPECT_EQ(resultEv->GetValue(rOff + 3), 2);
    EXPECT_EQ(resultEv->GetValue(rOff + 4), 1);

    delete context;
    delete vectorBatch;
    delete result;
}

TEST_F(SortArrayTest, IntArrayDefaultAscending)
{
    int rowSize = 1;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto arrayIntType = std::make_shared<ArrayType>(intType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto elemVec = std::shared_ptr<BaseVector>(new Vector<int32_t>(3));
    auto *ev = dynamic_cast<Vector<int32_t> *>(elemVec.get());
    ev->SetValue(0, 30);
    ev->SetValue(1, 10);
    ev->SetValue(2, 20);
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 3);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("sort_array", {
        new FieldExpr(0, arrayIntType)
    }, arrayIntType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_FALSE(resultArray->IsNull(0));
    EXPECT_EQ(resultArray->GetSize(0), 3);

    auto resultElemVec = resultArray->GetElementVector();
    auto *resultEv = dynamic_cast<Vector<int32_t> *>(resultElemVec.get());
    ASSERT_NE(resultEv, nullptr);

    int32_t rOff = static_cast<int32_t>(resultArray->GetOffset(0));
    EXPECT_EQ(resultEv->GetValue(rOff + 0), 10);
    EXPECT_EQ(resultEv->GetValue(rOff + 1), 20);
    EXPECT_EQ(resultEv->GetValue(rOff + 2), 30);

    delete context;
    delete vectorBatch;
    delete result;
}

// ==================== BYTE array tests ====================

TEST_F(SortArrayTest, ByteArrayAscending)
{
    int rowSize = 1;
    auto byteType = std::make_shared<DataType>(OMNI_BYTE);
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto arrayByteType = std::make_shared<ArrayType>(byteType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto elemVec = std::shared_ptr<BaseVector>(new Vector<int8_t>(3));
    auto *ev = dynamic_cast<Vector<int8_t> *>(elemVec.get());
    ev->SetValue(0, static_cast<int8_t>(30));
    ev->SetValue(1, static_cast<int8_t>(10));
    ev->SetValue(2, static_cast<int8_t>(20));
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 3);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("sort_array", {
        new FieldExpr(0, arrayByteType),
        new LiteralExpr(true, boolType)
    }, arrayByteType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);

    auto resultElemVec = resultArray->GetElementVector();
    auto *resultEv = dynamic_cast<Vector<int8_t> *>(resultElemVec.get());
    ASSERT_NE(resultEv, nullptr);

    int32_t rOff = static_cast<int32_t>(resultArray->GetOffset(0));
    EXPECT_EQ(resultEv->GetValue(rOff + 0), static_cast<int8_t>(10));
    EXPECT_EQ(resultEv->GetValue(rOff + 1), static_cast<int8_t>(20));
    EXPECT_EQ(resultEv->GetValue(rOff + 2), static_cast<int8_t>(30));

    delete context;
    delete vectorBatch;
    delete result;
}

// ==================== SHORT array tests ====================

TEST_F(SortArrayTest, ShortArrayAscending)
{
    int rowSize = 1;
    auto shortType = std::make_shared<DataType>(OMNI_SHORT);
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto arrayShortType = std::make_shared<ArrayType>(shortType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto elemVec = std::shared_ptr<BaseVector>(new Vector<int16_t>(3));
    auto *ev = dynamic_cast<Vector<int16_t> *>(elemVec.get());
    ev->SetValue(0, static_cast<int16_t>(300));
    ev->SetValue(1, static_cast<int16_t>(100));
    ev->SetValue(2, static_cast<int16_t>(200));
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 3);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("sort_array", {
        new FieldExpr(0, arrayShortType),
        new LiteralExpr(true, boolType)
    }, arrayShortType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);

    auto resultElemVec = resultArray->GetElementVector();
    auto *resultEv = dynamic_cast<Vector<int16_t> *>(resultElemVec.get());
    ASSERT_NE(resultEv, nullptr);

    int32_t rOff = static_cast<int32_t>(resultArray->GetOffset(0));
    EXPECT_EQ(resultEv->GetValue(rOff + 0), static_cast<int16_t>(100));
    EXPECT_EQ(resultEv->GetValue(rOff + 1), static_cast<int16_t>(200));
    EXPECT_EQ(resultEv->GetValue(rOff + 2), static_cast<int16_t>(300));

    delete context;
    delete vectorBatch;
    delete result;
}

// ==================== LONG array tests ====================

TEST_F(SortArrayTest, LongArrayAscending)
{
    int rowSize = 1;
    auto longType = std::make_shared<DataType>(OMNI_LONG);
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto arrayLongType = std::make_shared<ArrayType>(longType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto elemVec = std::shared_ptr<BaseVector>(new Vector<int64_t>(3));
    auto *ev = dynamic_cast<Vector<int64_t> *>(elemVec.get());
    ev->SetValue(0, 300L);
    ev->SetValue(1, 100L);
    ev->SetValue(2, 200L);
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 3);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("sort_array", {
        new FieldExpr(0, arrayLongType),
        new LiteralExpr(true, boolType)
    }, arrayLongType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);

    auto resultElemVec = resultArray->GetElementVector();
    auto *resultEv = dynamic_cast<Vector<int64_t> *>(resultElemVec.get());
    ASSERT_NE(resultEv, nullptr);

    int32_t rOff = static_cast<int32_t>(resultArray->GetOffset(0));
    EXPECT_EQ(resultEv->GetValue(rOff + 0), 100L);
    EXPECT_EQ(resultEv->GetValue(rOff + 1), 200L);
    EXPECT_EQ(resultEv->GetValue(rOff + 2), 300L);

    delete context;
    delete vectorBatch;
    delete result;
}

// ==================== FLOAT array tests ====================

TEST_F(SortArrayTest, FloatArrayAscending)
{
    int rowSize = 1;
    auto floatType = std::make_shared<DataType>(OMNI_FLOAT);
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto arrayFloatType = std::make_shared<ArrayType>(floatType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto elemVec = std::shared_ptr<BaseVector>(new Vector<float>(3));
    auto *ev = dynamic_cast<Vector<float> *>(elemVec.get());
    ev->SetValue(0, 3.3f);
    ev->SetValue(1, 1.1f);
    ev->SetValue(2, 2.2f);
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 3);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("sort_array", {
        new FieldExpr(0, arrayFloatType),
        new LiteralExpr(true, boolType)
    }, arrayFloatType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);

    auto resultElemVec = resultArray->GetElementVector();
    auto *resultEv = dynamic_cast<Vector<float> *>(resultElemVec.get());
    ASSERT_NE(resultEv, nullptr);

    int32_t rOff = static_cast<int32_t>(resultArray->GetOffset(0));
    EXPECT_FLOAT_EQ(resultEv->GetValue(rOff + 0), 1.1f);
    EXPECT_FLOAT_EQ(resultEv->GetValue(rOff + 1), 2.2f);
    EXPECT_FLOAT_EQ(resultEv->GetValue(rOff + 2), 3.3f);

    delete context;
    delete vectorBatch;
    delete result;
}

TEST_F(SortArrayTest, FloatArrayWithNaN)
{
    int rowSize = 1;
    auto floatType = std::make_shared<DataType>(OMNI_FLOAT);
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto arrayFloatType = std::make_shared<ArrayType>(floatType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto elemVec = std::shared_ptr<BaseVector>(new Vector<float>(4));
    auto *ev = dynamic_cast<Vector<float> *>(elemVec.get());
    ev->SetValue(0, std::numeric_limits<float>::quiet_NaN());
    ev->SetValue(1, 1.0f);
    ev->SetValue(2, 3.0f);
    ev->SetValue(3, 2.0f);
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 4);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("sort_array", {
        new FieldExpr(0, arrayFloatType),
        new LiteralExpr(true, boolType)
    }, arrayFloatType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);

    auto resultElemVec = resultArray->GetElementVector();
    auto *resultEv = dynamic_cast<Vector<float> *>(resultElemVec.get());
    ASSERT_NE(resultEv, nullptr);

    int32_t rOff = static_cast<int32_t>(resultArray->GetOffset(0));
    EXPECT_FLOAT_EQ(resultEv->GetValue(rOff + 0), 1.0f);
    EXPECT_FLOAT_EQ(resultEv->GetValue(rOff + 1), 2.0f);
    EXPECT_FLOAT_EQ(resultEv->GetValue(rOff + 2), 3.0f);
    EXPECT_TRUE(std::isnan(resultEv->GetValue(rOff + 3))) << "NaN should be at the end in ascending order";

    delete context;
    delete vectorBatch;
    delete result;
}

// ==================== DOUBLE array tests ====================

TEST_F(SortArrayTest, DoubleArrayAscending)
{
    int rowSize = 1;
    auto doubleType = std::make_shared<DataType>(OMNI_DOUBLE);
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto arrayDoubleType = std::make_shared<ArrayType>(doubleType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto elemVec = std::shared_ptr<BaseVector>(new Vector<double>(4));
    auto *ev = dynamic_cast<Vector<double> *>(elemVec.get());
    ev->SetValue(0, 4.44);
    ev->SetValue(1, 1.11);
    ev->SetValue(2, 3.33);
    ev->SetValue(3, 2.22);
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 4);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("sort_array", {
        new FieldExpr(0, arrayDoubleType),
        new LiteralExpr(true, boolType)
    }, arrayDoubleType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);

    auto resultElemVec = resultArray->GetElementVector();
    auto *resultEv = dynamic_cast<Vector<double> *>(resultElemVec.get());
    ASSERT_NE(resultEv, nullptr);

    int32_t rOff = static_cast<int32_t>(resultArray->GetOffset(0));
    EXPECT_DOUBLE_EQ(resultEv->GetValue(rOff + 0), 1.11);
    EXPECT_DOUBLE_EQ(resultEv->GetValue(rOff + 1), 2.22);
    EXPECT_DOUBLE_EQ(resultEv->GetValue(rOff + 2), 3.33);
    EXPECT_DOUBLE_EQ(resultEv->GetValue(rOff + 3), 4.44);

    delete context;
    delete vectorBatch;
    delete result;
}

TEST_F(SortArrayTest, DoubleArrayWithNaN)
{
    int rowSize = 1;
    auto doubleType = std::make_shared<DataType>(OMNI_DOUBLE);
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto arrayDoubleType = std::make_shared<ArrayType>(doubleType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto elemVec = std::shared_ptr<BaseVector>(new Vector<double>(3));
    auto *ev = dynamic_cast<Vector<double> *>(elemVec.get());
    ev->SetValue(0, std::numeric_limits<double>::quiet_NaN());
    ev->SetValue(1, 1.0);
    ev->SetValue(2, 2.0);
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 3);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("sort_array", {
        new FieldExpr(0, arrayDoubleType),
        new LiteralExpr(false, boolType)
    }, arrayDoubleType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);

    auto resultElemVec = resultArray->GetElementVector();
    auto *resultEv = dynamic_cast<Vector<double> *>(resultElemVec.get());
    ASSERT_NE(resultEv, nullptr);

    int32_t rOff = static_cast<int32_t>(resultArray->GetOffset(0));
    EXPECT_TRUE(std::isnan(resultEv->GetValue(rOff + 0))) << "NaN should be first in descending order";
    EXPECT_DOUBLE_EQ(resultEv->GetValue(rOff + 1), 2.0);
    EXPECT_DOUBLE_EQ(resultEv->GetValue(rOff + 2), 1.0);

    delete context;
    delete vectorBatch;
    delete result;
}

// ==================== BOOLEAN array tests ====================

TEST_F(SortArrayTest, BooleanArrayAscending)
{
    int rowSize = 1;
    auto boolElemType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto arrayBoolType = std::make_shared<ArrayType>(boolElemType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto elemVec = std::shared_ptr<BaseVector>(new Vector<bool>(4));
    auto *ev = dynamic_cast<Vector<bool> *>(elemVec.get());
    ev->SetValue(0, true);
    ev->SetValue(1, false);
    ev->SetValue(2, true);
    ev->SetValue(3, false);
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 4);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("sort_array", {
        new FieldExpr(0, arrayBoolType),
        new LiteralExpr(true, boolType)
    }, arrayBoolType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);

    auto resultElemVec = resultArray->GetElementVector();
    auto *resultEv = dynamic_cast<Vector<bool> *>(resultElemVec.get());
    ASSERT_NE(resultEv, nullptr);

    int32_t rOff = static_cast<int32_t>(resultArray->GetOffset(0));
    EXPECT_EQ(resultEv->GetValue(rOff + 0), false);
    EXPECT_EQ(resultEv->GetValue(rOff + 1), false);
    EXPECT_EQ(resultEv->GetValue(rOff + 2), true);
    EXPECT_EQ(resultEv->GetValue(rOff + 3), true);

    delete context;
    delete vectorBatch;
    delete result;
}

// ==================== VARCHAR array tests ====================

TEST_F(SortArrayTest, VarcharArrayAscending)
{
    int rowSize = 1;
    auto varcharType = std::make_shared<DataType>(OMNI_VARCHAR);
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto arrayVarcharType = std::make_shared<ArrayType>(varcharType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto *elemVecRaw = VectorHelper::CreateStringVector(3);
    auto *strVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(elemVecRaw);
    strVec->SetValue(0, std::string_view("cherry"));
    strVec->SetValue(1, std::string_view("apple"));
    strVec->SetValue(2, std::string_view("banana"));
    auto elemVec = std::shared_ptr<BaseVector>(elemVecRaw);
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 3);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("sort_array", {
        new FieldExpr(0, arrayVarcharType),
        new LiteralExpr(true, boolType)
    }, arrayVarcharType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);

    auto resultElemVec = resultArray->GetElementVector();
    auto *resultStrVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(resultElemVec.get());
    ASSERT_NE(resultStrVec, nullptr);

    int32_t rOff = static_cast<int32_t>(resultArray->GetOffset(0));
    EXPECT_EQ(std::string(resultStrVec->GetValue(rOff + 0)), "apple");
    EXPECT_EQ(std::string(resultStrVec->GetValue(rOff + 1)), "banana");
    EXPECT_EQ(std::string(resultStrVec->GetValue(rOff + 2)), "cherry");

    delete context;
    delete vectorBatch;
    delete result;
}

TEST_F(SortArrayTest, VarcharArrayDescending)
{
    int rowSize = 1;
    auto varcharType = std::make_shared<DataType>(OMNI_VARCHAR);
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto arrayVarcharType = std::make_shared<ArrayType>(varcharType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto *elemVecRaw = VectorHelper::CreateStringVector(3);
    auto *strVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(elemVecRaw);
    strVec->SetValue(0, std::string_view("cherry"));
    strVec->SetValue(1, std::string_view("apple"));
    strVec->SetValue(2, std::string_view("banana"));
    auto elemVec = std::shared_ptr<BaseVector>(elemVecRaw);
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 3);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("sort_array", {
        new FieldExpr(0, arrayVarcharType),
        new LiteralExpr(false, boolType)
    }, arrayVarcharType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);

    auto resultElemVec = resultArray->GetElementVector();
    auto *resultStrVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(resultElemVec.get());
    ASSERT_NE(resultStrVec, nullptr);

    int32_t rOff = static_cast<int32_t>(resultArray->GetOffset(0));
    EXPECT_EQ(std::string(resultStrVec->GetValue(rOff + 0)), "cherry");
    EXPECT_EQ(std::string(resultStrVec->GetValue(rOff + 1)), "banana");
    EXPECT_EQ(std::string(resultStrVec->GetValue(rOff + 2)), "apple");

    delete context;
    delete vectorBatch;
    delete result;
}

// ==================== DECIMAL128 array tests ====================

TEST_F(SortArrayTest, Decimal128ArrayAscending)
{
    int rowSize = 1;
    auto decimal128Type = std::make_shared<DataType>(OMNI_DECIMAL128);
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto arrayDecimal128Type = std::make_shared<ArrayType>(decimal128Type);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto elemVec = std::shared_ptr<BaseVector>(new Vector<Decimal128>(3));
    auto *ev = dynamic_cast<Vector<Decimal128> *>(elemVec.get());
    ev->SetValue(0, Decimal128(0, 300));
    ev->SetValue(1, Decimal128(0, 100));
    ev->SetValue(2, Decimal128(0, 200));
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 3);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("sort_array", {
        new FieldExpr(0, arrayDecimal128Type),
        new LiteralExpr(true, boolType)
    }, arrayDecimal128Type);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);

    auto resultElemVec = resultArray->GetElementVector();
    auto *resultEv = dynamic_cast<Vector<Decimal128> *>(resultElemVec.get());
    ASSERT_NE(resultEv, nullptr);

    int32_t rOff = static_cast<int32_t>(resultArray->GetOffset(0));
    EXPECT_EQ(resultEv->GetValue(rOff + 0).LowBits(), 100ULL);
    EXPECT_EQ(resultEv->GetValue(rOff + 1).LowBits(), 200ULL);
    EXPECT_EQ(resultEv->GetValue(rOff + 2).LowBits(), 300ULL);

    delete context;
    delete vectorBatch;
    delete result;
}

// ==================== DATE32 array tests ====================

TEST_F(SortArrayTest, Date32ArrayAscending)
{
    int rowSize = 1;
    auto date32Type = std::make_shared<DataType>(OMNI_DATE32);
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto arrayDate32Type = std::make_shared<ArrayType>(date32Type);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto elemVec = std::shared_ptr<BaseVector>(new Vector<int32_t>(3));
    auto *ev = dynamic_cast<Vector<int32_t> *>(elemVec.get());
    ev->SetValue(0, 19751);
    ev->SetValue(1, 0);
    ev->SetValue(2, 18628);
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 3);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("sort_array", {
        new FieldExpr(0, arrayDate32Type),
        new LiteralExpr(true, boolType)
    }, arrayDate32Type);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);

    auto resultElemVec = resultArray->GetElementVector();
    auto *resultEv = dynamic_cast<Vector<int32_t> *>(resultElemVec.get());
    ASSERT_NE(resultEv, nullptr);

    int32_t rOff = static_cast<int32_t>(resultArray->GetOffset(0));
    EXPECT_EQ(resultEv->GetValue(rOff + 0), 0);
    EXPECT_EQ(resultEv->GetValue(rOff + 1), 18628);
    EXPECT_EQ(resultEv->GetValue(rOff + 2), 19751);

    delete context;
    delete vectorBatch;
    delete result;
}

// ==================== TIMESTAMP array tests ====================

TEST_F(SortArrayTest, TimestampArrayAscending)
{
    int rowSize = 1;
    auto timestampType = std::make_shared<DataType>(OMNI_TIMESTAMP);
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto arrayTimestampType = std::make_shared<ArrayType>(timestampType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto elemVec = std::shared_ptr<BaseVector>(new Vector<int64_t>(3));
    auto *ev = dynamic_cast<Vector<int64_t> *>(elemVec.get());
    ev->SetValue(0, 1706522730000000L);
    ev->SetValue(1, 0L);
    ev->SetValue(2, 1000000000000L);
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 3);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("sort_array", {
        new FieldExpr(0, arrayTimestampType),
        new LiteralExpr(true, boolType)
    }, arrayTimestampType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);

    auto resultElemVec = resultArray->GetElementVector();
    auto *resultEv = dynamic_cast<Vector<int64_t> *>(resultElemVec.get());
    ASSERT_NE(resultEv, nullptr);

    int32_t rOff = static_cast<int32_t>(resultArray->GetOffset(0));
    EXPECT_EQ(resultEv->GetValue(rOff + 0), 0L);
    EXPECT_EQ(resultEv->GetValue(rOff + 1), 1000000000000L);
    EXPECT_EQ(resultEv->GetValue(rOff + 2), 1706522730000000L);

    delete context;
    delete vectorBatch;
    delete result;
}

// ==================== DECIMAL64 array tests ====================

TEST_F(SortArrayTest, Decimal64ArrayAscending)
{
    int rowSize = 1;
    auto decimal64Type = std::make_shared<DataType>(OMNI_DECIMAL64);
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto arrayDecimal64Type = std::make_shared<ArrayType>(decimal64Type);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto elemVec = std::shared_ptr<BaseVector>(new Vector<int64_t>(3));
    auto *ev = dynamic_cast<Vector<int64_t> *>(elemVec.get());
    ev->SetValue(0, 99999900L);
    ev->SetValue(1, 12345600L);
    ev->SetValue(2, 65432100L);
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 3);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("sort_array", {
        new FieldExpr(0, arrayDecimal64Type),
        new LiteralExpr(true, boolType)
    }, arrayDecimal64Type);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);

    auto resultElemVec = resultArray->GetElementVector();
    auto *resultEv = dynamic_cast<Vector<int64_t> *>(resultElemVec.get());
    ASSERT_NE(resultEv, nullptr);

    int32_t rOff = static_cast<int32_t>(resultArray->GetOffset(0));
    EXPECT_EQ(resultEv->GetValue(rOff + 0), 12345600L);
    EXPECT_EQ(resultEv->GetValue(rOff + 1), 65432100L);
    EXPECT_EQ(resultEv->GetValue(rOff + 2), 99999900L);

    delete context;
    delete vectorBatch;
    delete result;
}

// ==================== Null / Empty / Edge cases ====================

TEST_F(SortArrayTest, NullArraySort)
{
    int rowSize = 1;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto arrayIntType = std::make_shared<ArrayType>(intType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto elemVec = std::shared_ptr<BaseVector>(new Vector<int32_t>(1));
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetNull(0);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("sort_array", {
        new FieldExpr(0, arrayIntType),
        new LiteralExpr(true, boolType)
    }, arrayIntType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_TRUE(resultArray->IsNull(0)) << "Null input should produce null output";

    delete context;
    delete vectorBatch;
    delete result;
}

TEST_F(SortArrayTest, EmptyArraySort)
{
    int rowSize = 1;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto arrayIntType = std::make_shared<ArrayType>(intType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto elemVec = std::shared_ptr<BaseVector>(new Vector<int32_t>(0));
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 0);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("sort_array", {
        new FieldExpr(0, arrayIntType),
        new LiteralExpr(true, boolType)
    }, arrayIntType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_FALSE(resultArray->IsNull(0));
    EXPECT_EQ(resultArray->GetSize(0), 0) << "Empty input array should produce empty output";

    delete context;
    delete vectorBatch;
    delete result;
}

TEST_F(SortArrayTest, SingleElementArraySort)
{
    int rowSize = 1;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto arrayIntType = std::make_shared<ArrayType>(intType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto elemVec = std::shared_ptr<BaseVector>(new Vector<int32_t>(1));
    auto *ev = dynamic_cast<Vector<int32_t> *>(elemVec.get());
    ev->SetValue(0, 42);
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 1);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("sort_array", {
        new FieldExpr(0, arrayIntType),
        new LiteralExpr(true, boolType)
    }, arrayIntType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_FALSE(resultArray->IsNull(0));
    EXPECT_EQ(resultArray->GetSize(0), 1);

    auto resultElemVec = resultArray->GetElementVector();
    auto *resultEv = dynamic_cast<Vector<int32_t> *>(resultElemVec.get());
    ASSERT_NE(resultEv, nullptr);
    int32_t rOff = static_cast<int32_t>(resultArray->GetOffset(0));
    EXPECT_EQ(resultEv->GetValue(rOff), 42);

    delete context;
    delete vectorBatch;
    delete result;
}

TEST_F(SortArrayTest, ArrayWithNullElementsAscending)
{
    int rowSize = 1;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto arrayIntType = std::make_shared<ArrayType>(intType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto elemVec = std::shared_ptr<BaseVector>(new Vector<int32_t>(5));
    auto *ev = dynamic_cast<Vector<int32_t> *>(elemVec.get());
    ev->SetValue(0, 3);
    ev->SetNull(1);
    ev->SetValue(2, 1);
    ev->SetNull(3);
    ev->SetValue(4, 2);
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 5);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("sort_array", {
        new FieldExpr(0, arrayIntType),
        new LiteralExpr(true, boolType)
    }, arrayIntType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_EQ(resultArray->GetSize(0), 5);

    auto resultElemVec = resultArray->GetElementVector();
    auto *resultEv = dynamic_cast<Vector<int32_t> *>(resultElemVec.get());
    ASSERT_NE(resultEv, nullptr);

    int32_t rOff = static_cast<int32_t>(resultArray->GetOffset(0));
    EXPECT_TRUE(resultEv->IsNull(rOff + 0)) << "Null should be first in ascending";
    EXPECT_TRUE(resultEv->IsNull(rOff + 1)) << "Null should be first in ascending";
    EXPECT_EQ(resultEv->GetValue(rOff + 2), 1);
    EXPECT_EQ(resultEv->GetValue(rOff + 3), 2);
    EXPECT_EQ(resultEv->GetValue(rOff + 4), 3);

    delete context;
    delete vectorBatch;
    delete result;
}

TEST_F(SortArrayTest, ArrayWithNullElementsDescending)
{
    int rowSize = 1;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto arrayIntType = std::make_shared<ArrayType>(intType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto elemVec = std::shared_ptr<BaseVector>(new Vector<int32_t>(5));
    auto *ev = dynamic_cast<Vector<int32_t> *>(elemVec.get());
    ev->SetValue(0, 3);
    ev->SetNull(1);
    ev->SetValue(2, 1);
    ev->SetNull(3);
    ev->SetValue(4, 2);
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 5);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("sort_array", {
        new FieldExpr(0, arrayIntType),
        new LiteralExpr(false, boolType)
    }, arrayIntType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_EQ(resultArray->GetSize(0), 5);

    auto resultElemVec = resultArray->GetElementVector();
    auto *resultEv = dynamic_cast<Vector<int32_t> *>(resultElemVec.get());
    ASSERT_NE(resultEv, nullptr);

    int32_t rOff = static_cast<int32_t>(resultArray->GetOffset(0));
    EXPECT_EQ(resultEv->GetValue(rOff + 0), 3);
    EXPECT_EQ(resultEv->GetValue(rOff + 1), 2);
    EXPECT_EQ(resultEv->GetValue(rOff + 2), 1);
    EXPECT_TRUE(resultEv->IsNull(rOff + 3)) << "Null should be last in descending";
    EXPECT_TRUE(resultEv->IsNull(rOff + 4)) << "Null should be last in descending";

    delete context;
    delete vectorBatch;
    delete result;
}

// ==================== Multiple rows tests ====================

TEST_F(SortArrayTest, MultipleRows)
{
    int rowSize = 3;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto arrayIntType = std::make_shared<ArrayType>(intType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto elemVec = std::shared_ptr<BaseVector>(new Vector<int32_t>(8));
    auto *ev = dynamic_cast<Vector<int32_t> *>(elemVec.get());
    ev->SetValue(0, 3);
    ev->SetValue(1, 1);
    ev->SetValue(2, 2);
    ev->SetValue(3, 50);
    ev->SetValue(4, 30);
    ev->SetValue(5, 40);
    ev->SetValue(6, 200);
    ev->SetValue(7, 100);
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 3);
    arrVec->SetOffset(2, 6);
    arrVec->SetOffset(3, 8);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("sort_array", {
        new FieldExpr(0, arrayIntType),
        new LiteralExpr(true, boolType)
    }, arrayIntType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);

    auto resultElemVec = resultArray->GetElementVector();
    auto *resultEv = dynamic_cast<Vector<int32_t> *>(resultElemVec.get());
    ASSERT_NE(resultEv, nullptr);

    // Row 0: [3, 1, 2] -> [1, 2, 3]
    {
        int32_t rOff = static_cast<int32_t>(resultArray->GetOffset(0));
        EXPECT_EQ(resultEv->GetValue(rOff + 0), 1);
        EXPECT_EQ(resultEv->GetValue(rOff + 1), 2);
        EXPECT_EQ(resultEv->GetValue(rOff + 2), 3);
    }

    // Row 1: [50, 30, 40] -> [30, 40, 50]
    {
        int32_t rOff = static_cast<int32_t>(resultArray->GetOffset(1));
        EXPECT_EQ(resultEv->GetValue(rOff + 0), 30);
        EXPECT_EQ(resultEv->GetValue(rOff + 1), 40);
        EXPECT_EQ(resultEv->GetValue(rOff + 2), 50);
    }

    // Row 2: [200, 100] -> [100, 200]
    {
        int32_t rOff = static_cast<int32_t>(resultArray->GetOffset(2));
        EXPECT_EQ(resultEv->GetValue(rOff + 0), 100);
        EXPECT_EQ(resultEv->GetValue(rOff + 1), 200);
    }

    delete context;
    delete vectorBatch;
    delete result;
}

TEST_F(SortArrayTest, MultipleRowsWithNullRow)
{
    int rowSize = 3;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto arrayIntType = std::make_shared<ArrayType>(intType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto elemVec = std::shared_ptr<BaseVector>(new Vector<int32_t>(5));
    auto *ev = dynamic_cast<Vector<int32_t> *>(elemVec.get());
    ev->SetValue(0, 3);
    ev->SetValue(1, 1);
    ev->SetValue(2, 2);
    ev->SetValue(3, 8);
    ev->SetValue(4, 7);
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 3);
    arrVec->SetNull(1);
    arrVec->SetOffset(2, 3);
    arrVec->SetOffset(3, 5);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("sort_array", {
        new FieldExpr(0, arrayIntType),
        new LiteralExpr(true, boolType)
    }, arrayIntType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);

    EXPECT_FALSE(resultArray->IsNull(0));
    EXPECT_TRUE(resultArray->IsNull(1)) << "Null row should remain null";
    EXPECT_FALSE(resultArray->IsNull(2));

    auto resultElemVec = resultArray->GetElementVector();
    auto *resultEv = dynamic_cast<Vector<int32_t> *>(resultElemVec.get());
    ASSERT_NE(resultEv, nullptr);

    // Row 0: [3, 1, 2] -> [1, 2, 3]
    {
        int32_t rOff = static_cast<int32_t>(resultArray->GetOffset(0));
        EXPECT_EQ(resultEv->GetValue(rOff + 0), 1);
        EXPECT_EQ(resultEv->GetValue(rOff + 1), 2);
        EXPECT_EQ(resultEv->GetValue(rOff + 2), 3);
    }

    // Row 2: [8, 7] -> [7, 8]
    {
        int32_t rOff = static_cast<int32_t>(resultArray->GetOffset(2));
        EXPECT_EQ(resultEv->GetValue(rOff + 0), 7);
        EXPECT_EQ(resultEv->GetValue(rOff + 1), 8);
    }

    delete context;
    delete vectorBatch;
    delete result;
}

// ==================== VARBINARY array tests ====================

TEST_F(SortArrayTest, VarbinaryArrayAscending)
{
    int rowSize = 1;
    auto varbinaryType = std::make_shared<DataType>(OMNI_VARBINARY);
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto arrayVarbinaryType = std::make_shared<ArrayType>(varbinaryType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto *elemVecRaw = VectorHelper::CreateStringVector(3);
    auto *strVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(elemVecRaw);
    strVec->SetValue(0, std::string_view("ghi"));
    strVec->SetValue(1, std::string_view("abc"));
    strVec->SetValue(2, std::string_view("def"));
    auto elemVec = std::shared_ptr<BaseVector>(elemVecRaw);
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 3);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("sort_array", {
        new FieldExpr(0, arrayVarbinaryType),
        new LiteralExpr(true, boolType)
    }, arrayVarbinaryType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);

    auto resultElemVec = resultArray->GetElementVector();
    auto *resultStrVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(resultElemVec.get());
    ASSERT_NE(resultStrVec, nullptr);

    int32_t rOff = static_cast<int32_t>(resultArray->GetOffset(0));
    EXPECT_EQ(std::string(resultStrVec->GetValue(rOff + 0)), "abc");
    EXPECT_EQ(std::string(resultStrVec->GetValue(rOff + 1)), "def");
    EXPECT_EQ(std::string(resultStrVec->GetValue(rOff + 2)), "ghi");

    delete context;
    delete vectorBatch;
    delete result;
}
