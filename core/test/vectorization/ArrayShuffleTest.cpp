/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: ArrayShuffle function test
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

class ArrayShuffleTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

// ==================== INT array tests ====================

/// Test: shuffle int array with seed
/// shuffle([1, 2, 3, 4, 5], seed) -> permutation of [1, 2, 3, 4, 5]
TEST_F(ArrayShuffleTest, IntArrayShuffle)
{
    int rowSize = 1;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto longType = std::make_shared<DataType>(OMNI_LONG);
    auto arrayIntType = std::make_shared<ArrayType>(intType);

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
    arrVec->SetOffset(1, 5);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("shuffle", {
        new FieldExpr(0, arrayIntType),
        new LiteralExpr(int64_t(42), longType)
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
    std::vector<int32_t> resultValues;
    for (int i = 0; i < 5; ++i) {
        resultValues.push_back(resultEv->GetValue(rOff + i));
    }

    std::vector<int32_t> expectedValues = {1, 2, 3, 4, 5};
    std::vector<int32_t> sortedResult = resultValues;
    std::sort(sortedResult.begin(), sortedResult.end());
    EXPECT_EQ(sortedResult, expectedValues) << "Shuffled array must be a permutation of input";

    delete context;
    delete vectorBatch;
    delete result;
}

// ==================== LONG array tests ====================

/// Test: shuffle long array with seed
/// shuffle([100, 200, 300], seed) -> permutation of [100, 200, 300]
TEST_F(ArrayShuffleTest, LongArrayShuffle)
{
    int rowSize = 1;
    auto longType = std::make_shared<DataType>(OMNI_LONG);
    auto arrayLongType = std::make_shared<ArrayType>(longType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto elemVec = std::shared_ptr<BaseVector>(new Vector<int64_t>(3));
    auto *ev = dynamic_cast<Vector<int64_t> *>(elemVec.get());
    ev->SetValue(0, 100L);
    ev->SetValue(1, 200L);
    ev->SetValue(2, 300L);
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 3);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("shuffle", {
        new FieldExpr(0, arrayLongType),
        new LiteralExpr(int64_t(99), longType)
    }, arrayLongType);

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
    auto *resultEv = dynamic_cast<Vector<int64_t> *>(resultElemVec.get());
    ASSERT_NE(resultEv, nullptr);

    int32_t rOff = static_cast<int32_t>(resultArray->GetOffset(0));
    std::vector<int64_t> resultValues;
    for (int i = 0; i < 3; ++i) {
        resultValues.push_back(resultEv->GetValue(rOff + i));
    }

    std::vector<int64_t> sortedResult = resultValues;
    std::sort(sortedResult.begin(), sortedResult.end());
    std::vector<int64_t> expectedSorted = {100L, 200L, 300L};
    EXPECT_EQ(sortedResult, expectedSorted) << "Shuffled array must be a permutation of input";

    delete context;
    delete vectorBatch;
    delete result;
}

// ==================== SHORT array tests ====================

/// Test: shuffle short array with seed
/// shuffle([10, 20, 30, 40], seed) -> permutation of [10, 20, 30, 40]
TEST_F(ArrayShuffleTest, ShortArrayShuffle)
{
    int rowSize = 1;
    auto shortType = std::make_shared<DataType>(OMNI_SHORT);
    auto longType = std::make_shared<DataType>(OMNI_LONG);
    auto arrayShortType = std::make_shared<ArrayType>(shortType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto elemVec = std::shared_ptr<BaseVector>(new Vector<int16_t>(4));
    auto *ev = dynamic_cast<Vector<int16_t> *>(elemVec.get());
    ev->SetValue(0, static_cast<int16_t>(10));
    ev->SetValue(1, static_cast<int16_t>(20));
    ev->SetValue(2, static_cast<int16_t>(30));
    ev->SetValue(3, static_cast<int16_t>(40));
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 4);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("shuffle", {
        new FieldExpr(0, arrayShortType),
        new LiteralExpr(int64_t(7), longType)
    }, arrayShortType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_FALSE(resultArray->IsNull(0));
    EXPECT_EQ(resultArray->GetSize(0), 4);

    auto resultElemVec = resultArray->GetElementVector();
    auto *resultEv = dynamic_cast<Vector<int16_t> *>(resultElemVec.get());
    ASSERT_NE(resultEv, nullptr);

    int32_t rOff = static_cast<int32_t>(resultArray->GetOffset(0));
    std::vector<int16_t> resultValues;
    for (int i = 0; i < 4; ++i) {
        resultValues.push_back(resultEv->GetValue(rOff + i));
    }

    std::vector<int16_t> sortedResult = resultValues;
    std::sort(sortedResult.begin(), sortedResult.end());
    std::vector<int16_t> expectedSorted = {10, 20, 30, 40};
    EXPECT_EQ(sortedResult, expectedSorted) << "Shuffled array must be a permutation of input";

    delete context;
    delete vectorBatch;
    delete result;
}

// ==================== BYTE array tests ====================

/// Test: shuffle byte array with seed
/// shuffle([1, 2, 3], seed) -> permutation of [1, 2, 3]
TEST_F(ArrayShuffleTest, ByteArrayShuffle)
{
    int rowSize = 1;
    auto byteType = std::make_shared<DataType>(OMNI_BYTE);
    auto longType = std::make_shared<DataType>(OMNI_LONG);
    auto arrayByteType = std::make_shared<ArrayType>(byteType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto elemVec = std::shared_ptr<BaseVector>(new Vector<int8_t>(3));
    auto *ev = dynamic_cast<Vector<int8_t> *>(elemVec.get());
    ev->SetValue(0, static_cast<int8_t>(1));
    ev->SetValue(1, static_cast<int8_t>(2));
    ev->SetValue(2, static_cast<int8_t>(3));
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 3);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("shuffle", {
        new FieldExpr(0, arrayByteType),
        new LiteralExpr(int64_t(12), longType)
    }, arrayByteType);

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
    auto *resultEv = dynamic_cast<Vector<int8_t> *>(resultElemVec.get());
    ASSERT_NE(resultEv, nullptr);

    int32_t rOff = static_cast<int32_t>(resultArray->GetOffset(0));
    std::vector<int8_t> resultValues;
    for (int i = 0; i < 3; ++i) {
        resultValues.push_back(resultEv->GetValue(rOff + i));
    }

    std::vector<int8_t> sortedResult = resultValues;
    std::sort(sortedResult.begin(), sortedResult.end());
    std::vector<int8_t> expectedSorted = {1, 2, 3};
    EXPECT_EQ(sortedResult, expectedSorted) << "Shuffled array must be a permutation of input";

    delete context;
    delete vectorBatch;
    delete result;
}

// ==================== FLOAT array tests ====================

/// Test: shuffle float array with seed
/// shuffle([1.1, 2.2, 3.3], seed) -> permutation of [1.1, 2.2, 3.3]
TEST_F(ArrayShuffleTest, FloatArrayShuffle)
{
    int rowSize = 1;
    auto floatType = std::make_shared<DataType>(OMNI_FLOAT);
    auto longType = std::make_shared<DataType>(OMNI_LONG);
    auto arrayFloatType = std::make_shared<ArrayType>(floatType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto elemVec = std::shared_ptr<BaseVector>(new Vector<float>(3));
    auto *ev = dynamic_cast<Vector<float> *>(elemVec.get());
    ev->SetValue(0, 1.1f);
    ev->SetValue(1, 2.2f);
    ev->SetValue(2, 3.3f);
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 3);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("shuffle", {
        new FieldExpr(0, arrayFloatType),
        new LiteralExpr(int64_t(55), longType)
    }, arrayFloatType);

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
    auto *resultEv = dynamic_cast<Vector<float> *>(resultElemVec.get());
    ASSERT_NE(resultEv, nullptr);

    int32_t rOff = static_cast<int32_t>(resultArray->GetOffset(0));
    std::vector<float> resultValues;
    for (int i = 0; i < 3; ++i) {
        resultValues.push_back(resultEv->GetValue(rOff + i));
    }

    std::vector<float> sortedResult = resultValues;
    std::sort(sortedResult.begin(), sortedResult.end());
    std::vector<float> expectedSorted = {1.1f, 2.2f, 3.3f};
    EXPECT_EQ(sortedResult, expectedSorted) << "Shuffled array must be a permutation of input";

    delete context;
    delete vectorBatch;
    delete result;
}

// ==================== DOUBLE array tests ====================

/// Test: shuffle double array with seed
/// shuffle([1.11, 2.22, 3.33, 4.44], seed) -> permutation of [1.11, 2.22, 3.33, 4.44]
TEST_F(ArrayShuffleTest, DoubleArrayShuffle)
{
    int rowSize = 1;
    auto doubleType = std::make_shared<DataType>(OMNI_DOUBLE);
    auto longType = std::make_shared<DataType>(OMNI_LONG);
    auto arrayDoubleType = std::make_shared<ArrayType>(doubleType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto elemVec = std::shared_ptr<BaseVector>(new Vector<double>(4));
    auto *ev = dynamic_cast<Vector<double> *>(elemVec.get());
    ev->SetValue(0, 1.11);
    ev->SetValue(1, 2.22);
    ev->SetValue(2, 3.33);
    ev->SetValue(3, 4.44);
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 4);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("shuffle", {
        new FieldExpr(0, arrayDoubleType),
        new LiteralExpr(int64_t(77), longType)
    }, arrayDoubleType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_FALSE(resultArray->IsNull(0));
    EXPECT_EQ(resultArray->GetSize(0), 4);

    auto resultElemVec = resultArray->GetElementVector();
    auto *resultEv = dynamic_cast<Vector<double> *>(resultElemVec.get());
    ASSERT_NE(resultEv, nullptr);

    int32_t rOff = static_cast<int32_t>(resultArray->GetOffset(0));
    std::vector<double> resultValues;
    for (int i = 0; i < 4; ++i) {
        resultValues.push_back(resultEv->GetValue(rOff + i));
    }

    std::vector<double> sortedResult = resultValues;
    std::sort(sortedResult.begin(), sortedResult.end());
    std::vector<double> expectedSorted = {1.11, 2.22, 3.33, 4.44};
    EXPECT_EQ(sortedResult, expectedSorted) << "Shuffled array must be a permutation of input";

    delete context;
    delete vectorBatch;
    delete result;
}

// ==================== BOOLEAN array tests ====================

/// Test: shuffle boolean array with seed
/// shuffle([true, false, true, false], seed) -> permutation of [true, false, true, false]
TEST_F(ArrayShuffleTest, BooleanArrayShuffle)
{
    int rowSize = 1;
    auto boolType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto longType = std::make_shared<DataType>(OMNI_LONG);
    auto arrayBoolType = std::make_shared<ArrayType>(boolType);

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

    auto expr = FuncExpr("shuffle", {
        new FieldExpr(0, arrayBoolType),
        new LiteralExpr(int64_t(33), longType)
    }, arrayBoolType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_FALSE(resultArray->IsNull(0));
    EXPECT_EQ(resultArray->GetSize(0), 4);

    auto resultElemVec = resultArray->GetElementVector();
    auto *resultEv = dynamic_cast<Vector<bool> *>(resultElemVec.get());
    ASSERT_NE(resultEv, nullptr);

    int32_t rOff = static_cast<int32_t>(resultArray->GetOffset(0));
    int trueCount = 0;
    int falseCount = 0;
    for (int i = 0; i < 4; ++i) {
        if (resultEv->GetValue(rOff + i)) {
            trueCount++;
        } else {
            falseCount++;
        }
    }
    EXPECT_EQ(trueCount, 2) << "Should have 2 true values";
    EXPECT_EQ(falseCount, 2) << "Should have 2 false values";

    delete context;
    delete vectorBatch;
    delete result;
}

// ==================== VARCHAR array tests ====================

/// Test: shuffle varchar array with seed
/// shuffle(["apple", "banana", "cherry"], seed) -> permutation of ["apple", "banana", "cherry"]
TEST_F(ArrayShuffleTest, VarcharArrayShuffle)
{
    int rowSize = 1;
    auto varcharType = std::make_shared<DataType>(OMNI_VARCHAR);
    auto longType = std::make_shared<DataType>(OMNI_LONG);
    auto arrayVarcharType = std::make_shared<ArrayType>(varcharType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto *elemVecRaw = VectorHelper::CreateStringVector(3);
    auto *strVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(elemVecRaw);
    strVec->SetValue(0, std::string_view("apple"));
    strVec->SetValue(1, std::string_view("banana"));
    strVec->SetValue(2, std::string_view("cherry"));
    auto elemVec = std::shared_ptr<BaseVector>(elemVecRaw);
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 3);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("shuffle", {
        new FieldExpr(0, arrayVarcharType),
        new LiteralExpr(int64_t(88), longType)
    }, arrayVarcharType);

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
    auto *resultStrVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(resultElemVec.get());
    ASSERT_NE(resultStrVec, nullptr);

    int32_t rOff = static_cast<int32_t>(resultArray->GetOffset(0));
    std::vector<std::string> resultValues;
    for (int i = 0; i < 3; ++i) {
        std::string_view sv = resultStrVec->GetValue(rOff + i);
        resultValues.push_back(std::string(sv.data(), sv.size()));
    }

    std::vector<std::string> sortedResult = resultValues;
    std::sort(sortedResult.begin(), sortedResult.end());
    std::vector<std::string> expectedSorted = {"apple", "banana", "cherry"};
    EXPECT_EQ(sortedResult, expectedSorted) << "Shuffled array must be a permutation of input";

    delete context;
    delete vectorBatch;
    delete result;
}

// ==================== DECIMAL128 array tests ====================

/// Test: shuffle decimal128 array with seed
TEST_F(ArrayShuffleTest, Decimal128ArrayShuffle)
{
    int rowSize = 1;
    auto decimal128Type = std::make_shared<DataType>(OMNI_DECIMAL128);
    auto longType = std::make_shared<DataType>(OMNI_LONG);
    auto arrayDecimal128Type = std::make_shared<ArrayType>(decimal128Type);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto elemVec = std::shared_ptr<BaseVector>(new Vector<Decimal128>(3));
    auto *ev = dynamic_cast<Vector<Decimal128> *>(elemVec.get());
    ev->SetValue(0, Decimal128(0, 100));
    ev->SetValue(1, Decimal128(0, 200));
    ev->SetValue(2, Decimal128(0, 300));
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 3);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("shuffle", {
        new FieldExpr(0, arrayDecimal128Type),
        new LiteralExpr(int64_t(11), longType)
    }, arrayDecimal128Type);

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
    auto *resultEv = dynamic_cast<Vector<Decimal128> *>(resultElemVec.get());
    ASSERT_NE(resultEv, nullptr);

    int32_t rOff = static_cast<int32_t>(resultArray->GetOffset(0));
    std::vector<int64_t> resultLowValues;
    for (int i = 0; i < 3; ++i) {
        Decimal128 val = resultEv->GetValue(rOff + i);
        resultLowValues.push_back(static_cast<int64_t>(val.LowBits()));
    }

    std::vector<int64_t> sortedResult = resultLowValues;
    std::sort(sortedResult.begin(), sortedResult.end());
    std::vector<int64_t> expectedSorted = {100, 200, 300};
    EXPECT_EQ(sortedResult, expectedSorted) << "Shuffled array must be a permutation of input";

    delete context;
    delete vectorBatch;
    delete result;
}

// ==================== DATE32 array tests ====================

/// Test: shuffle date32 array with seed
TEST_F(ArrayShuffleTest, Date32ArrayShuffle)
{
    int rowSize = 1;
    auto date32Type = std::make_shared<DataType>(OMNI_DATE32);
    auto longType = std::make_shared<DataType>(OMNI_LONG);
    auto arrayDate32Type = std::make_shared<ArrayType>(date32Type);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto elemVec = std::shared_ptr<BaseVector>(new Vector<int32_t>(3));
    auto *ev = dynamic_cast<Vector<int32_t> *>(elemVec.get());
    ev->SetValue(0, 19751); // 2024-01-29
    ev->SetValue(1, 0);     // 1970-01-01
    ev->SetValue(2, 18628); // 2020-12-31
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 3);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("shuffle", {
        new FieldExpr(0, arrayDate32Type),
        new LiteralExpr(int64_t(22), longType)
    }, arrayDate32Type);

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
    std::vector<int32_t> resultValues;
    for (int i = 0; i < 3; ++i) {
        resultValues.push_back(resultEv->GetValue(rOff + i));
    }

    std::vector<int32_t> sortedResult = resultValues;
    std::sort(sortedResult.begin(), sortedResult.end());
    std::vector<int32_t> expectedSorted = {0, 18628, 19751};
    EXPECT_EQ(sortedResult, expectedSorted) << "Shuffled array must be a permutation of input";

    delete context;
    delete vectorBatch;
    delete result;
}

// ==================== TIMESTAMP array tests ====================

/// Test: shuffle timestamp array with seed
TEST_F(ArrayShuffleTest, TimestampArrayShuffle)
{
    int rowSize = 1;
    auto timestampType = std::make_shared<DataType>(OMNI_TIMESTAMP);
    auto longType = std::make_shared<DataType>(OMNI_LONG);
    auto arrayTimestampType = std::make_shared<ArrayType>(timestampType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto elemVec = std::shared_ptr<BaseVector>(new Vector<int64_t>(3));
    auto *ev = dynamic_cast<Vector<int64_t> *>(elemVec.get());
    ev->SetValue(0, 1706522730000000L); // 2024-01-29 11:45:30
    ev->SetValue(1, 0L);                // 1970-01-01 00:00:00
    ev->SetValue(2, 1000000000000L);    // some timestamp
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 3);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("shuffle", {
        new FieldExpr(0, arrayTimestampType),
        new LiteralExpr(int64_t(66), longType)
    }, arrayTimestampType);

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
    auto *resultEv = dynamic_cast<Vector<int64_t> *>(resultElemVec.get());
    ASSERT_NE(resultEv, nullptr);

    int32_t rOff = static_cast<int32_t>(resultArray->GetOffset(0));
    std::vector<int64_t> resultValues;
    for (int i = 0; i < 3; ++i) {
        resultValues.push_back(resultEv->GetValue(rOff + i));
    }

    std::vector<int64_t> sortedResult = resultValues;
    std::sort(sortedResult.begin(), sortedResult.end());
    std::vector<int64_t> expectedSorted = {0L, 1000000000000L, 1706522730000000L};
    EXPECT_EQ(sortedResult, expectedSorted) << "Shuffled array must be a permutation of input";

    delete context;
    delete vectorBatch;
    delete result;
}

// ==================== Null / Empty / Edge cases ====================

/// Test: shuffle null array -> null result
TEST_F(ArrayShuffleTest, NullArrayShuffle)
{
    int rowSize = 1;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto longType = std::make_shared<DataType>(OMNI_LONG);
    auto arrayIntType = std::make_shared<ArrayType>(intType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto elemVec = std::shared_ptr<BaseVector>(new Vector<int32_t>(1));
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetNull(0);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("shuffle", {
        new FieldExpr(0, arrayIntType),
        new LiteralExpr(int64_t(42), longType)
    }, arrayIntType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_TRUE(resultArray->IsNull(0)) << "Null input array should produce null output";

    delete context;
    delete vectorBatch;
    delete result;
}

/// Test: shuffle empty array -> empty array
TEST_F(ArrayShuffleTest, EmptyArrayShuffle)
{
    int rowSize = 1;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto longType = std::make_shared<DataType>(OMNI_LONG);
    auto arrayIntType = std::make_shared<ArrayType>(intType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto elemVec = std::shared_ptr<BaseVector>(new Vector<int32_t>(0));
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 0);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("shuffle", {
        new FieldExpr(0, arrayIntType),
        new LiteralExpr(int64_t(42), longType)
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

/// Test: shuffle single-element array -> same array
TEST_F(ArrayShuffleTest, SingleElementArrayShuffle)
{
    int rowSize = 1;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto longType = std::make_shared<DataType>(OMNI_LONG);
    auto arrayIntType = std::make_shared<ArrayType>(intType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto elemVec = std::shared_ptr<BaseVector>(new Vector<int32_t>(1));
    auto *ev = dynamic_cast<Vector<int32_t> *>(elemVec.get());
    ev->SetValue(0, 42);
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 1);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("shuffle", {
        new FieldExpr(0, arrayIntType),
        new LiteralExpr(int64_t(99), longType)
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
    EXPECT_EQ(resultEv->GetValue(rOff), 42) << "Single-element array should remain the same";

    delete context;
    delete vectorBatch;
    delete result;
}

/// Test: shuffle array with null elements preserves nulls
TEST_F(ArrayShuffleTest, ArrayWithNullElementsShuffle)
{
    int rowSize = 1;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto longType = std::make_shared<DataType>(OMNI_LONG);
    auto arrayIntType = std::make_shared<ArrayType>(intType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto elemVec = std::shared_ptr<BaseVector>(new Vector<int32_t>(4));
    auto *ev = dynamic_cast<Vector<int32_t> *>(elemVec.get());
    ev->SetValue(0, 10);
    ev->SetNull(1);
    ev->SetValue(2, 30);
    ev->SetNull(3);
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 4);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("shuffle", {
        new FieldExpr(0, arrayIntType),
        new LiteralExpr(int64_t(42), longType)
    }, arrayIntType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);
    EXPECT_FALSE(resultArray->IsNull(0));
    EXPECT_EQ(resultArray->GetSize(0), 4);

    auto resultElemVec = resultArray->GetElementVector();
    auto *resultEv = dynamic_cast<Vector<int32_t> *>(resultElemVec.get());
    ASSERT_NE(resultEv, nullptr);

    int32_t rOff = static_cast<int32_t>(resultArray->GetOffset(0));
    int nullCount = 0;
    int nonNullCount = 0;
    std::vector<int32_t> nonNullValues;
    for (int i = 0; i < 4; ++i) {
        if (resultEv->IsNull(rOff + i)) {
            nullCount++;
        } else {
            nonNullCount++;
            nonNullValues.push_back(resultEv->GetValue(rOff + i));
        }
    }
    EXPECT_EQ(nullCount, 2) << "Should preserve 2 null elements";
    EXPECT_EQ(nonNullCount, 2) << "Should preserve 2 non-null elements";

    std::sort(nonNullValues.begin(), nonNullValues.end());
    std::vector<int32_t> expectedNonNull = {10, 30};
    EXPECT_EQ(nonNullValues, expectedNonNull) << "Non-null values must match";

    delete context;
    delete vectorBatch;
    delete result;
}

// ==================== Multiple rows tests ====================

/// Test: shuffle with multiple rows
TEST_F(ArrayShuffleTest, MultipleRowsShuffle)
{
    int rowSize = 3;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto longType = std::make_shared<DataType>(OMNI_LONG);
    auto arrayIntType = std::make_shared<ArrayType>(intType);

    auto *vectorBatch = new VectorBatch(rowSize);

    // Row 0: [1, 2, 3], Row 1: [10, 20], Row 2: [100]
    auto elemVec = std::shared_ptr<BaseVector>(new Vector<int32_t>(6));
    auto *ev = dynamic_cast<Vector<int32_t> *>(elemVec.get());
    ev->SetValue(0, 1);
    ev->SetValue(1, 2);
    ev->SetValue(2, 3);
    ev->SetValue(3, 10);
    ev->SetValue(4, 20);
    ev->SetValue(5, 100);
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 3);
    arrVec->SetOffset(2, 5);
    arrVec->SetOffset(3, 6);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("shuffle", {
        new FieldExpr(0, arrayIntType),
        new LiteralExpr(int64_t(42), longType)
    }, arrayIntType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);

    // Row 0: size 3
    EXPECT_FALSE(resultArray->IsNull(0));
    EXPECT_EQ(resultArray->GetSize(0), 3);
    // Row 1: size 2
    EXPECT_FALSE(resultArray->IsNull(1));
    EXPECT_EQ(resultArray->GetSize(1), 2);
    // Row 2: size 1
    EXPECT_FALSE(resultArray->IsNull(2));
    EXPECT_EQ(resultArray->GetSize(2), 1);

    auto resultElemVec = resultArray->GetElementVector();
    auto *resultEv = dynamic_cast<Vector<int32_t> *>(resultElemVec.get());
    ASSERT_NE(resultEv, nullptr);

    // Verify row 0 is permutation of [1, 2, 3]
    {
        int32_t rOff = static_cast<int32_t>(resultArray->GetOffset(0));
        std::vector<int32_t> vals;
        for (int i = 0; i < 3; ++i) {
            vals.push_back(resultEv->GetValue(rOff + i));
        }
        std::sort(vals.begin(), vals.end());
        EXPECT_EQ(vals, (std::vector<int32_t>{1, 2, 3}));
    }

    // Verify row 1 is permutation of [10, 20]
    {
        int32_t rOff = static_cast<int32_t>(resultArray->GetOffset(1));
        std::vector<int32_t> vals;
        for (int i = 0; i < 2; ++i) {
            vals.push_back(resultEv->GetValue(rOff + i));
        }
        std::sort(vals.begin(), vals.end());
        EXPECT_EQ(vals, (std::vector<int32_t>{10, 20}));
    }

    // Verify row 2 is [100]
    {
        int32_t rOff = static_cast<int32_t>(resultArray->GetOffset(2));
        EXPECT_EQ(resultEv->GetValue(rOff), 100);
    }

    delete context;
    delete vectorBatch;
    delete result;
}

/// Test: shuffle with multiple rows including null row
TEST_F(ArrayShuffleTest, MultipleRowsWithNullRow)
{
    int rowSize = 3;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto longType = std::make_shared<DataType>(OMNI_LONG);
    auto arrayIntType = std::make_shared<ArrayType>(intType);

    auto *vectorBatch = new VectorBatch(rowSize);

    // Row 0: [1, 2, 3], Row 1: null, Row 2: [7, 8]
    auto elemVec = std::shared_ptr<BaseVector>(new Vector<int32_t>(5));
    auto *ev = dynamic_cast<Vector<int32_t> *>(elemVec.get());
    ev->SetValue(0, 1);
    ev->SetValue(1, 2);
    ev->SetValue(2, 3);
    ev->SetValue(3, 7);
    ev->SetValue(4, 8);
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 3);
    arrVec->SetNull(1);
    arrVec->SetOffset(2, 3);
    arrVec->SetOffset(3, 5);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("shuffle", {
        new FieldExpr(0, arrayIntType),
        new LiteralExpr(int64_t(42), longType)
    }, arrayIntType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *resultArray = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArray, nullptr);

    // Row 0
    EXPECT_FALSE(resultArray->IsNull(0));
    EXPECT_EQ(resultArray->GetSize(0), 3);
    // Row 1: null
    EXPECT_TRUE(resultArray->IsNull(1)) << "Null row should remain null";
    // Row 2
    EXPECT_FALSE(resultArray->IsNull(2));
    EXPECT_EQ(resultArray->GetSize(2), 2);

    auto resultElemVec = resultArray->GetElementVector();
    auto *resultEv = dynamic_cast<Vector<int32_t> *>(resultElemVec.get());
    ASSERT_NE(resultEv, nullptr);

    // Verify row 0
    {
        int32_t rOff = static_cast<int32_t>(resultArray->GetOffset(0));
        std::vector<int32_t> vals;
        for (int i = 0; i < 3; ++i) {
            vals.push_back(resultEv->GetValue(rOff + i));
        }
        std::sort(vals.begin(), vals.end());
        EXPECT_EQ(vals, (std::vector<int32_t>{1, 2, 3}));
    }

    // Verify row 2
    {
        int32_t rOff = static_cast<int32_t>(resultArray->GetOffset(2));
        std::vector<int32_t> vals;
        for (int i = 0; i < 2; ++i) {
            vals.push_back(resultEv->GetValue(rOff + i));
        }
        std::sort(vals.begin(), vals.end());
        EXPECT_EQ(vals, (std::vector<int32_t>{7, 8}));
    }

    delete context;
    delete vectorBatch;
    delete result;
}

// ==================== Deterministic seed tests ====================

/// Test: same seed produces same result
TEST_F(ArrayShuffleTest, DeterministicWithSameSeed)
{
    int rowSize = 1;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    auto longType = std::make_shared<DataType>(OMNI_LONG);
    auto arrayIntType = std::make_shared<ArrayType>(intType);

    auto runShuffle = [&](int64_t seed) -> std::vector<int32_t> {
        auto *vectorBatch = new VectorBatch(rowSize);

        auto elemVec = std::shared_ptr<BaseVector>(new Vector<int32_t>(5));
        auto *ev = dynamic_cast<Vector<int32_t> *>(elemVec.get());
        ev->SetValue(0, 10);
        ev->SetValue(1, 20);
        ev->SetValue(2, 30);
        ev->SetValue(3, 40);
        ev->SetValue(4, 50);
        auto *arrVec = new ArrayVector(rowSize, elemVec);
        arrVec->SetOffset(0, 0);
        arrVec->SetOffset(1, 5);
        vectorBatch->Append(arrVec);

        auto expr = FuncExpr("shuffle", {
            new FieldExpr(0, arrayIntType),
            new LiteralExpr(seed, longType)
        }, arrayIntType);

        auto context = new ExecutionContext(4096, nullptr);
        context->SetResultRowSize(rowSize);

        ExprEval e(vectorBatch, context);
        e.VisitExpr(expr);
        auto result = e.GetResult();

        auto *resultArray = dynamic_cast<ArrayVector *>(result);
        auto resultElemVec = resultArray->GetElementVector();
        auto *resultEv = dynamic_cast<Vector<int32_t> *>(resultElemVec.get());

        int32_t rOff = static_cast<int32_t>(resultArray->GetOffset(0));
        std::vector<int32_t> resultValues;
        for (int i = 0; i < 5; ++i) {
            resultValues.push_back(resultEv->GetValue(rOff + i));
        }

        delete context;
        delete vectorBatch;
        delete result;
        return resultValues;
    };

    auto result1 = runShuffle(42);
    auto result2 = runShuffle(42);
    EXPECT_EQ(result1, result2) << "Same seed must produce identical shuffle results";

    auto result3 = runShuffle(99);
    std::vector<int32_t> sorted1 = result1;
    std::vector<int32_t> sorted3 = result3;
    std::sort(sorted1.begin(), sorted1.end());
    std::sort(sorted3.begin(), sorted3.end());
    EXPECT_EQ(sorted1, sorted3) << "Both are permutations of the same array";
}

// ==================== VARBINARY array tests ====================

/// Test: shuffle varbinary array with seed
TEST_F(ArrayShuffleTest, VarbinaryArrayShuffle)
{
    int rowSize = 1;
    auto varbinaryType = std::make_shared<DataType>(OMNI_VARBINARY);
    auto longType = std::make_shared<DataType>(OMNI_LONG);
    auto arrayVarbinaryType = std::make_shared<ArrayType>(varbinaryType);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto *elemVecRaw = VectorHelper::CreateStringVector(3);
    auto *strVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(elemVecRaw);
    strVec->SetValue(0, std::string_view("abc"));
    strVec->SetValue(1, std::string_view("def"));
    strVec->SetValue(2, std::string_view("ghi"));
    auto elemVec = std::shared_ptr<BaseVector>(elemVecRaw);
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 3);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("shuffle", {
        new FieldExpr(0, arrayVarbinaryType),
        new LiteralExpr(int64_t(15), longType)
    }, arrayVarbinaryType);

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
    auto *resultStrVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(resultElemVec.get());
    ASSERT_NE(resultStrVec, nullptr);

    int32_t rOff = static_cast<int32_t>(resultArray->GetOffset(0));
    std::vector<std::string> resultValues;
    for (int i = 0; i < 3; ++i) {
        std::string_view sv = resultStrVec->GetValue(rOff + i);
        resultValues.push_back(std::string(sv.data(), sv.size()));
    }

    std::vector<std::string> sortedResult = resultValues;
    std::sort(sortedResult.begin(), sortedResult.end());
    std::vector<std::string> expectedSorted = {"abc", "def", "ghi"};
    EXPECT_EQ(sortedResult, expectedSorted) << "Shuffled array must be a permutation of input";

    delete context;
    delete vectorBatch;
    delete result;
}

// ==================== DECIMAL64 array tests ====================

/// Test: shuffle decimal64 array with seed
TEST_F(ArrayShuffleTest, Decimal64ArrayShuffle)
{
    int rowSize = 1;
    auto decimal64Type = std::make_shared<DataType>(OMNI_DECIMAL64);
    auto longType = std::make_shared<DataType>(OMNI_LONG);
    auto arrayDecimal64Type = std::make_shared<ArrayType>(decimal64Type);

    auto *vectorBatch = new VectorBatch(rowSize);

    auto elemVec = std::shared_ptr<BaseVector>(new Vector<int64_t>(3));
    auto *ev = dynamic_cast<Vector<int64_t> *>(elemVec.get());
    ev->SetValue(0, 12345600L);
    ev->SetValue(1, 65432100L);
    ev->SetValue(2, 99999900L);
    auto *arrVec = new ArrayVector(rowSize, elemVec);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 3);
    vectorBatch->Append(arrVec);

    auto expr = FuncExpr("shuffle", {
        new FieldExpr(0, arrayDecimal64Type),
        new LiteralExpr(int64_t(50), longType)
    }, arrayDecimal64Type);

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
    auto *resultEv = dynamic_cast<Vector<int64_t> *>(resultElemVec.get());
    ASSERT_NE(resultEv, nullptr);

    int32_t rOff = static_cast<int32_t>(resultArray->GetOffset(0));
    std::vector<int64_t> resultValues;
    for (int i = 0; i < 3; ++i) {
        resultValues.push_back(resultEv->GetValue(rOff + i));
    }

    std::vector<int64_t> sortedResult = resultValues;
    std::sort(sortedResult.begin(), sortedResult.end());
    std::vector<int64_t> expectedSorted = {12345600L, 65432100L, 99999900L};
    EXPECT_EQ(sortedResult, expectedSorted) << "Shuffled array must be a permutation of input";

    delete context;
    delete vectorBatch;
    delete result;
}
