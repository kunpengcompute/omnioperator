/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Unit tests for array constructor function
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
#include "vector/array_vector.h"
#include "vector/map_vector.h"
#include "vector/row_vector.h"
#include "type/data_type.h"
#include "util/type_util.h"

using namespace omniruntime;
using omniruntime::type::ArrayType;
using omniruntime::type::DataTypePtr;
using omniruntime::type::MapType;
using omniruntime::type::RowType;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::mem;
using namespace omniruntime::op;
using namespace omniruntime::expressions;
using namespace omniruntime::TestUtil;

class ArrayConstructorTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}

    template <typename T>
    void VerifyArrayElement(ArrayVector *arrayVec, int32_t row, int32_t elemIdx, T expected)
    {
        auto elemVec = arrayVec->GetElementVector();
        auto *typedVec = dynamic_cast<Vector<T> *>(elemVec.get());
        ASSERT_NE(typedVec, nullptr);
        int64_t offset = arrayVec->GetOffset(row);
        EXPECT_EQ(typedVec->GetValue(static_cast<int32_t>(offset + elemIdx)), expected);
    }

    template <typename T>
    void VerifyArrayElementNull(ArrayVector *arrayVec, int32_t row, int32_t elemIdx)
    {
        auto elemVec = arrayVec->GetElementVector();
        auto *typedVec = dynamic_cast<Vector<T> *>(elemVec.get());
        ASSERT_NE(typedVec, nullptr);
        int64_t offset = arrayVec->GetOffset(row);
        EXPECT_TRUE(typedVec->IsNull(static_cast<int32_t>(offset + elemIdx)));
    }

    void VerifyArraySize(ArrayVector *arrayVec, int32_t row, int64_t expectedSize)
    {
        EXPECT_EQ(arrayVec->GetSize(row), expectedSize);
    }

    void VerifyVarcharElement(ArrayVector *arrayVec, int32_t row, int32_t elemIdx,
        const std::string &expected)
    {
        using VarcharVector = Vector<LargeStringContainer<std::string_view>>;
        auto elemVec = arrayVec->GetElementVector();
        auto *typedVec = dynamic_cast<VarcharVector *>(elemVec.get());
        ASSERT_NE(typedVec, nullptr);
        int64_t offset = arrayVec->GetOffset(row);
        EXPECT_EQ(typedVec->GetValue(static_cast<int32_t>(offset + elemIdx)), expected);
    }

    void VerifyVarcharElementNull(ArrayVector *arrayVec, int32_t row, int32_t elemIdx)
    {
        using VarcharVector = Vector<LargeStringContainer<std::string_view>>;
        auto elemVec = arrayVec->GetElementVector();
        auto *typedVec = dynamic_cast<VarcharVector *>(elemVec.get());
        ASSERT_NE(typedVec, nullptr);
        int64_t offset = arrayVec->GetOffset(row);
        EXPECT_TRUE(typedVec->IsNull(static_cast<int32_t>(offset + elemIdx)));
    }
};

TEST_F(ArrayConstructorTest, IntegerArray)
{
    int rowSize = 2;

    auto *col0 = new Vector<int32_t>(rowSize);
    col0->SetValue(0, 1);
    col0->SetValue(1, 10);

    auto *col1 = new Vector<int32_t>(rowSize);
    col1->SetValue(0, 2);
    col1->SetValue(1, 20);

    auto *col2 = new Vector<int32_t>(rowSize);
    col2->SetValue(0, 3);
    col2->SetValue(1, 30);

    auto *input = new VectorBatch(rowSize);
    input->Append(col0);
    input->Append(col1);
    input->Append(col2);

    auto expr = FuncExpr("array", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_INT)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_INT)),
        new FieldExpr(2, std::make_shared<DataType>(OMNI_INT))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *arrayResult = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(arrayResult, nullptr);

    // row 0: array(1, 2, 3) -> [1, 2, 3]
    VerifyArraySize(arrayResult, 0, 3);
    VerifyArrayElement<int32_t>(arrayResult, 0, 0, 1);
    VerifyArrayElement<int32_t>(arrayResult, 0, 1, 2);
    VerifyArrayElement<int32_t>(arrayResult, 0, 2, 3);

    // row 1: array(10, 20, 30) -> [10, 20, 30]
    VerifyArraySize(arrayResult, 1, 3);
    VerifyArrayElement<int32_t>(arrayResult, 1, 0, 10);
    VerifyArrayElement<int32_t>(arrayResult, 1, 1, 20);
    VerifyArrayElement<int32_t>(arrayResult, 1, 2, 30);

    delete context;
    delete input;
    delete result;
}

TEST_F(ArrayConstructorTest, LongArray)
{
    int rowSize = 2;

    auto *col0 = new Vector<int64_t>(rowSize);
    col0->SetValue(0, 100L);
    col0->SetValue(1, 400L);

    auto *col1 = new Vector<int64_t>(rowSize);
    col1->SetValue(0, 200L);
    col1->SetValue(1, 500L);

    auto *input = new VectorBatch(rowSize);
    input->Append(col0);
    input->Append(col1);

    auto expr = FuncExpr("array", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_LONG)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_LONG))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *arrayResult = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(arrayResult, nullptr);

    VerifyArraySize(arrayResult, 0, 2);
    VerifyArrayElement<int64_t>(arrayResult, 0, 0, 100L);
    VerifyArrayElement<int64_t>(arrayResult, 0, 1, 200L);

    VerifyArraySize(arrayResult, 1, 2);
    VerifyArrayElement<int64_t>(arrayResult, 1, 0, 400L);
    VerifyArrayElement<int64_t>(arrayResult, 1, 1, 500L);

    delete context;
    delete input;
    delete result;
}

TEST_F(ArrayConstructorTest, DoubleArray)
{
    int rowSize = 2;

    auto *col0 = new Vector<double>(rowSize);
    col0->SetValue(0, 1.1);
    col0->SetValue(1, 4.4);

    auto *col1 = new Vector<double>(rowSize);
    col1->SetValue(0, 2.2);
    col1->SetValue(1, 5.5);

    auto *col2 = new Vector<double>(rowSize);
    col2->SetValue(0, 3.3);
    col2->SetValue(1, 6.6);

    auto *input = new VectorBatch(rowSize);
    input->Append(col0);
    input->Append(col1);
    input->Append(col2);

    auto expr = FuncExpr("array", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_DOUBLE)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_DOUBLE)),
        new FieldExpr(2, std::make_shared<DataType>(OMNI_DOUBLE))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *arrayResult = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(arrayResult, nullptr);

    VerifyArraySize(arrayResult, 0, 3);
    VerifyArrayElement<double>(arrayResult, 0, 0, 1.1);
    VerifyArrayElement<double>(arrayResult, 0, 1, 2.2);
    VerifyArrayElement<double>(arrayResult, 0, 2, 3.3);

    VerifyArraySize(arrayResult, 1, 3);
    VerifyArrayElement<double>(arrayResult, 1, 0, 4.4);
    VerifyArrayElement<double>(arrayResult, 1, 1, 5.5);
    VerifyArrayElement<double>(arrayResult, 1, 2, 6.6);

    delete context;
    delete input;
    delete result;
}

TEST_F(ArrayConstructorTest, FloatArray)
{
    int rowSize = 2;

    auto *col0 = new Vector<float>(rowSize);
    col0->SetValue(0, 1.5f);
    col0->SetValue(1, 4.5f);

    auto *col1 = new Vector<float>(rowSize);
    col1->SetValue(0, 2.5f);
    col1->SetValue(1, 5.5f);

    auto *input = new VectorBatch(rowSize);
    input->Append(col0);
    input->Append(col1);

    auto expr = FuncExpr("array", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_FLOAT)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_FLOAT))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *arrayResult = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(arrayResult, nullptr);

    VerifyArraySize(arrayResult, 0, 2);
    VerifyArrayElement<float>(arrayResult, 0, 0, 1.5f);
    VerifyArrayElement<float>(arrayResult, 0, 1, 2.5f);

    VerifyArraySize(arrayResult, 1, 2);
    VerifyArrayElement<float>(arrayResult, 1, 0, 4.5f);
    VerifyArrayElement<float>(arrayResult, 1, 1, 5.5f);

    delete context;
    delete input;
    delete result;
}

TEST_F(ArrayConstructorTest, BooleanArray)
{
    int rowSize = 2;

    auto *col0 = new Vector<bool>(rowSize);
    col0->SetValue(0, true);
    col0->SetValue(1, false);

    auto *col1 = new Vector<bool>(rowSize);
    col1->SetValue(0, false);
    col1->SetValue(1, true);

    auto *col2 = new Vector<bool>(rowSize);
    col2->SetValue(0, true);
    col2->SetValue(1, true);

    auto *input = new VectorBatch(rowSize);
    input->Append(col0);
    input->Append(col1);
    input->Append(col2);

    auto expr = FuncExpr("array", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_BOOLEAN)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_BOOLEAN)),
        new FieldExpr(2, std::make_shared<DataType>(OMNI_BOOLEAN))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *arrayResult = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(arrayResult, nullptr);

    // row 0: array(true, false, true) -> [true, false, true]
    VerifyArraySize(arrayResult, 0, 3);
    VerifyArrayElement<bool>(arrayResult, 0, 0, true);
    VerifyArrayElement<bool>(arrayResult, 0, 1, false);
    VerifyArrayElement<bool>(arrayResult, 0, 2, true);

    // row 1: array(false, true, true) -> [false, true, true]
    VerifyArraySize(arrayResult, 1, 3);
    VerifyArrayElement<bool>(arrayResult, 1, 0, false);
    VerifyArrayElement<bool>(arrayResult, 1, 1, true);
    VerifyArrayElement<bool>(arrayResult, 1, 2, true);

    delete context;
    delete input;
    delete result;
}

TEST_F(ArrayConstructorTest, ByteArray)
{
    int rowSize = 2;

    auto *col0 = new Vector<int8_t>(rowSize);
    col0->SetValue(0, static_cast<int8_t>(10));
    col0->SetValue(1, static_cast<int8_t>(-10));

    auto *col1 = new Vector<int8_t>(rowSize);
    col1->SetValue(0, static_cast<int8_t>(20));
    col1->SetValue(1, static_cast<int8_t>(-20));

    auto *input = new VectorBatch(rowSize);
    input->Append(col0);
    input->Append(col1);

    auto expr = FuncExpr("array", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_BYTE)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_BYTE))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *arrayResult = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(arrayResult, nullptr);

    VerifyArraySize(arrayResult, 0, 2);
    VerifyArrayElement<int8_t>(arrayResult, 0, 0, 10);
    VerifyArrayElement<int8_t>(arrayResult, 0, 1, 20);

    VerifyArraySize(arrayResult, 1, 2);
    VerifyArrayElement<int8_t>(arrayResult, 1, 0, -10);
    VerifyArrayElement<int8_t>(arrayResult, 1, 1, -20);

    delete context;
    delete input;
    delete result;
}

TEST_F(ArrayConstructorTest, ShortArray)
{
    int rowSize = 2;

    auto *col0 = new Vector<int16_t>(rowSize);
    col0->SetValue(0, static_cast<int16_t>(1000));
    col0->SetValue(1, static_cast<int16_t>(-1000));

    auto *col1 = new Vector<int16_t>(rowSize);
    col1->SetValue(0, static_cast<int16_t>(2000));
    col1->SetValue(1, static_cast<int16_t>(-2000));

    auto *input = new VectorBatch(rowSize);
    input->Append(col0);
    input->Append(col1);

    auto expr = FuncExpr("array", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_SHORT)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_SHORT))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *arrayResult = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(arrayResult, nullptr);

    VerifyArraySize(arrayResult, 0, 2);
    VerifyArrayElement<int16_t>(arrayResult, 0, 0, 1000);
    VerifyArrayElement<int16_t>(arrayResult, 0, 1, 2000);

    VerifyArraySize(arrayResult, 1, 2);
    VerifyArrayElement<int16_t>(arrayResult, 1, 0, -1000);
    VerifyArrayElement<int16_t>(arrayResult, 1, 1, -2000);

    delete context;
    delete input;
    delete result;
}

TEST_F(ArrayConstructorTest, WithNullElements)
{
    int rowSize = 2;

    auto *col0 = new Vector<int32_t>(rowSize);
    col0->SetValue(0, 1);
    col0->SetValue(1, 10);

    auto *col1 = new Vector<int32_t>(rowSize);
    col1->SetNull(0);
    col1->SetValue(1, 20);

    auto *col2 = new Vector<int32_t>(rowSize);
    col2->SetValue(0, 3);
    col2->SetNull(1);

    auto *input = new VectorBatch(rowSize);
    input->Append(col0);
    input->Append(col1);
    input->Append(col2);

    auto expr = FuncExpr("array", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_INT)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_INT)),
        new FieldExpr(2, std::make_shared<DataType>(OMNI_INT))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *arrayResult = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(arrayResult, nullptr);

    // row 0: array(1, null, 3) -> [1, null, 3]
    VerifyArraySize(arrayResult, 0, 3);
    VerifyArrayElement<int32_t>(arrayResult, 0, 0, 1);
    VerifyArrayElementNull<int32_t>(arrayResult, 0, 1);
    VerifyArrayElement<int32_t>(arrayResult, 0, 2, 3);

    // row 1: array(10, 20, null) -> [10, 20, null]
    VerifyArraySize(arrayResult, 1, 3);
    VerifyArrayElement<int32_t>(arrayResult, 1, 0, 10);
    VerifyArrayElement<int32_t>(arrayResult, 1, 1, 20);
    VerifyArrayElementNull<int32_t>(arrayResult, 1, 2);

    delete context;
    delete input;
    delete result;
}

TEST_F(ArrayConstructorTest, AllNullElements)
{
    int rowSize = 1;

    auto *col0 = new Vector<int32_t>(rowSize);
    col0->SetNull(0);

    auto *col1 = new Vector<int32_t>(rowSize);
    col1->SetNull(0);

    auto *input = new VectorBatch(rowSize);
    input->Append(col0);
    input->Append(col1);

    auto expr = FuncExpr("array", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_INT)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_INT))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *arrayResult = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(arrayResult, nullptr);

    // row 0: array(null, null) -> [null, null] (array itself is NOT null)
    EXPECT_FALSE(arrayResult->IsNull(0));
    VerifyArraySize(arrayResult, 0, 2);
    VerifyArrayElementNull<int32_t>(arrayResult, 0, 0);
    VerifyArrayElementNull<int32_t>(arrayResult, 0, 1);

    delete context;
    delete input;
    delete result;
}

TEST_F(ArrayConstructorTest, SingleElement)
{
    int rowSize = 2;

    auto *col0 = new Vector<int32_t>(rowSize);
    col0->SetValue(0, 42);
    col0->SetValue(1, 99);

    auto *input = new VectorBatch(rowSize);
    input->Append(col0);

    auto expr = FuncExpr("array", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_INT))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *arrayResult = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(arrayResult, nullptr);

    // row 0: array(42) -> [42]
    VerifyArraySize(arrayResult, 0, 1);
    VerifyArrayElement<int32_t>(arrayResult, 0, 0, 42);

    // row 1: array(99) -> [99]
    VerifyArraySize(arrayResult, 1, 1);
    VerifyArrayElement<int32_t>(arrayResult, 1, 0, 99);

    delete context;
    delete input;
    delete result;
}

TEST_F(ArrayConstructorTest, VarcharArray)
{
    int rowSize = 2;
    using VarcharVector = Vector<LargeStringContainer<std::string_view>>;

    auto *col0 = new VarcharVector(rowSize);
    col0->SetValue(0, std::string_view("hello"));
    col0->SetValue(1, std::string_view("foo"));

    auto *col1 = new VarcharVector(rowSize);
    col1->SetValue(0, std::string_view("world"));
    col1->SetValue(1, std::string_view("bar"));

    auto *input = new VectorBatch(rowSize);
    input->Append(col0);
    input->Append(col1);

    auto expr = FuncExpr("array", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_VARCHAR)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_VARCHAR))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *arrayResult = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(arrayResult, nullptr);

    VerifyArraySize(arrayResult, 0, 2);
    VerifyVarcharElement(arrayResult, 0, 0, "hello");
    VerifyVarcharElement(arrayResult, 0, 1, "world");

    VerifyArraySize(arrayResult, 1, 2);
    VerifyVarcharElement(arrayResult, 1, 0, "foo");
    VerifyVarcharElement(arrayResult, 1, 1, "bar");

    delete context;
    delete input;
    delete result;
}

TEST_F(ArrayConstructorTest, VarcharArrayWithNull)
{
    int rowSize = 1;
    using VarcharVector = Vector<LargeStringContainer<std::string_view>>;

    auto *col0 = new VarcharVector(rowSize);
    col0->SetValue(0, std::string_view("abc"));

    auto *col1 = new VarcharVector(rowSize);
    col1->SetNull(0);

    auto *col2 = new VarcharVector(rowSize);
    col2->SetValue(0, std::string_view("def"));

    auto *input = new VectorBatch(rowSize);
    input->Append(col0);
    input->Append(col1);
    input->Append(col2);

    auto expr = FuncExpr("array", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_VARCHAR)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_VARCHAR)),
        new FieldExpr(2, std::make_shared<DataType>(OMNI_VARCHAR))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *arrayResult = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(arrayResult, nullptr);

    // row 0: array('abc', null, 'def') -> ['abc', null, 'def']
    VerifyArraySize(arrayResult, 0, 3);
    VerifyVarcharElement(arrayResult, 0, 0, "abc");
    VerifyVarcharElementNull(arrayResult, 0, 1);
    VerifyVarcharElement(arrayResult, 0, 2, "def");

    delete context;
    delete input;
    delete result;
}

TEST_F(ArrayConstructorTest, DoubleSpecialValues)
{
    int rowSize = 1;

    constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
    constexpr double kInf = std::numeric_limits<double>::infinity();
    constexpr double kNegInf = -std::numeric_limits<double>::infinity();

    auto *col0 = new Vector<double>(rowSize);
    col0->SetValue(0, kNaN);

    auto *col1 = new Vector<double>(rowSize);
    col1->SetValue(0, kInf);

    auto *col2 = new Vector<double>(rowSize);
    col2->SetValue(0, kNegInf);

    auto *input = new VectorBatch(rowSize);
    input->Append(col0);
    input->Append(col1);
    input->Append(col2);

    auto expr = FuncExpr("array", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_DOUBLE)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_DOUBLE)),
        new FieldExpr(2, std::make_shared<DataType>(OMNI_DOUBLE))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *arrayResult = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(arrayResult, nullptr);

    // row 0: array(NaN, Inf, -Inf) -> [NaN, Inf, -Inf]
    VerifyArraySize(arrayResult, 0, 3);

    auto elemVec = arrayResult->GetElementVector();
    auto *doubleVec = dynamic_cast<Vector<double> *>(elemVec.get());
    ASSERT_NE(doubleVec, nullptr);
    int64_t offset0 = arrayResult->GetOffset(0);
    EXPECT_TRUE(std::isnan(doubleVec->GetValue(static_cast<int32_t>(offset0))));
    VerifyArrayElement<double>(arrayResult, 0, 1, kInf);
    VerifyArrayElement<double>(arrayResult, 0, 2, kNegInf);

    delete context;
    delete input;
    delete result;
}

TEST_F(ArrayConstructorTest, IntegerBoundaryValues)
{
    int rowSize = 1;

    constexpr int32_t kMin = std::numeric_limits<int32_t>::min();
    constexpr int32_t kMax = std::numeric_limits<int32_t>::max();

    auto *col0 = new Vector<int32_t>(rowSize);
    col0->SetValue(0, kMin);

    auto *col1 = new Vector<int32_t>(rowSize);
    col1->SetValue(0, 0);

    auto *col2 = new Vector<int32_t>(rowSize);
    col2->SetValue(0, kMax);

    auto *input = new VectorBatch(rowSize);
    input->Append(col0);
    input->Append(col1);
    input->Append(col2);

    auto expr = FuncExpr("array", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_INT)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_INT)),
        new FieldExpr(2, std::make_shared<DataType>(OMNI_INT))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *arrayResult = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(arrayResult, nullptr);

    VerifyArraySize(arrayResult, 0, 3);
    VerifyArrayElement<int32_t>(arrayResult, 0, 0, kMin);
    VerifyArrayElement<int32_t>(arrayResult, 0, 1, 0);
    VerifyArrayElement<int32_t>(arrayResult, 0, 2, kMax);

    delete context;
    delete input;
    delete result;
}

TEST_F(ArrayConstructorTest, Decimal128Array)
{
    int rowSize = 1;

    auto *col0 = new Vector<Decimal128>(rowSize);
    Decimal128 dec0(123456789012345678LL);
    col0->SetValue(0, dec0);

    auto *col1 = new Vector<Decimal128>(rowSize);
    Decimal128 dec1(-987654321098765432LL);
    col1->SetValue(0, dec1);

    auto *input = new VectorBatch(rowSize);
    input->Append(col0);
    input->Append(col1);

    auto expr = FuncExpr("array", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_DECIMAL128)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_DECIMAL128))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *arrayResult = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(arrayResult, nullptr);

    VerifyArraySize(arrayResult, 0, 2);
    VerifyArrayElement<Decimal128>(arrayResult, 0, 0, dec0);
    VerifyArrayElement<Decimal128>(arrayResult, 0, 1, dec1);

    delete context;
    delete input;
    delete result;
}

TEST_F(ArrayConstructorTest, DuplicateElements)
{
    int rowSize = 1;

    auto *col0 = new Vector<int32_t>(rowSize);
    col0->SetValue(0, 1);

    auto *col1 = new Vector<int32_t>(rowSize);
    col1->SetValue(0, 2);

    auto *col2 = new Vector<int32_t>(rowSize);
    col2->SetValue(0, 1);

    auto *input = new VectorBatch(rowSize);
    input->Append(col0);
    input->Append(col1);
    input->Append(col2);

    auto expr = FuncExpr("array", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_INT)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_INT)),
        new FieldExpr(2, std::make_shared<DataType>(OMNI_INT))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *arrayResult = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(arrayResult, nullptr);

    // row 0: array(1, 2, 1) -> [1, 2, 1]
    VerifyArraySize(arrayResult, 0, 3);
    VerifyArrayElement<int32_t>(arrayResult, 0, 0, 1);
    VerifyArrayElement<int32_t>(arrayResult, 0, 1, 2);
    VerifyArrayElement<int32_t>(arrayResult, 0, 2, 1);

    delete context;
    delete input;
    delete result;
}

TEST_F(ArrayConstructorTest, MultipleRows)
{
    int rowSize = 4;

    auto *col0 = new Vector<int32_t>(rowSize);
    col0->SetValue(0, 1);
    col0->SetValue(1, 4);
    col0->SetValue(2, 7);
    col0->SetValue(3, 10);

    auto *col1 = new Vector<int32_t>(rowSize);
    col1->SetValue(0, 2);
    col1->SetValue(1, 5);
    col1->SetValue(2, 8);
    col1->SetValue(3, 11);

    auto *input = new VectorBatch(rowSize);
    input->Append(col0);
    input->Append(col1);

    auto expr = FuncExpr("array", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_INT)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_INT))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *arrayResult = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(arrayResult, nullptr);

    for (int32_t row = 0; row < rowSize; ++row) {
        VerifyArraySize(arrayResult, row, 2);
    }
    VerifyArrayElement<int32_t>(arrayResult, 0, 0, 1);
    VerifyArrayElement<int32_t>(arrayResult, 0, 1, 2);
    VerifyArrayElement<int32_t>(arrayResult, 1, 0, 4);
    VerifyArrayElement<int32_t>(arrayResult, 1, 1, 5);
    VerifyArrayElement<int32_t>(arrayResult, 2, 0, 7);
    VerifyArrayElement<int32_t>(arrayResult, 2, 1, 8);
    VerifyArrayElement<int32_t>(arrayResult, 3, 0, 10);
    VerifyArrayElement<int32_t>(arrayResult, 3, 1, 11);

    delete context;
    delete input;
    delete result;
}

TEST_F(ArrayConstructorTest, Date32Array)
{
    int rowSize = 1;

    auto *col0 = new Vector<int32_t>(rowSize);
    col0->SetValue(0, 19751);

    auto *col1 = new Vector<int32_t>(rowSize);
    col1->SetValue(0, 0);

    auto *input = new VectorBatch(rowSize);
    input->Append(col0);
    input->Append(col1);

    auto expr = FuncExpr("array", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_DATE32)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_DATE32))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *arrayResult = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(arrayResult, nullptr);

    VerifyArraySize(arrayResult, 0, 2);
    VerifyArrayElement<int32_t>(arrayResult, 0, 0, 19751);
    VerifyArrayElement<int32_t>(arrayResult, 0, 1, 0);

    delete context;
    delete input;
    delete result;
}

TEST_F(ArrayConstructorTest, TimestampArray)
{
    int rowSize = 1;

    auto *col0 = new Vector<int64_t>(rowSize);
    col0->SetValue(0, 1706524130000000L);

    auto *col1 = new Vector<int64_t>(rowSize);
    col1->SetValue(0, 0L);

    auto *input = new VectorBatch(rowSize);
    input->Append(col0);
    input->Append(col1);

    auto expr = FuncExpr("array", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_TIMESTAMP)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_TIMESTAMP))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *arrayResult = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(arrayResult, nullptr);

    VerifyArraySize(arrayResult, 0, 2);
    VerifyArrayElement<int64_t>(arrayResult, 0, 0, 1706524130000000L);
    VerifyArrayElement<int64_t>(arrayResult, 0, 1, 0L);

    delete context;
    delete input;
    delete result;
}

TEST_F(ArrayConstructorTest, ManyElements)
{
    int rowSize = 1;
    constexpr int numCols = 10;

    std::vector<Vector<int32_t> *> columns;
    auto *input = new VectorBatch(rowSize);
    std::vector<Expr *> fieldExprs;

    for (int i = 0; i < numCols; ++i) {
        auto *col = new Vector<int32_t>(rowSize);
        col->SetValue(0, (i + 1) * 100);
        columns.push_back(col);
        input->Append(col);
        fieldExprs.push_back(new FieldExpr(i, std::make_shared<DataType>(OMNI_INT)));
    }

    auto expr = FuncExpr("array", fieldExprs, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *arrayResult = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(arrayResult, nullptr);

    VerifyArraySize(arrayResult, 0, numCols);
    for (int i = 0; i < numCols; ++i) {
        VerifyArrayElement<int32_t>(arrayResult, 0, i, (i + 1) * 100);
    }

    delete context;
    delete input;
    delete result;
}

TEST_F(ArrayConstructorTest, LongBoundaryValues)
{
    int rowSize = 1;

    constexpr int64_t kMin = std::numeric_limits<int64_t>::min();
    constexpr int64_t kMax = std::numeric_limits<int64_t>::max();

    auto *col0 = new Vector<int64_t>(rowSize);
    col0->SetValue(0, kMin);

    auto *col1 = new Vector<int64_t>(rowSize);
    col1->SetValue(0, kMax);

    auto *input = new VectorBatch(rowSize);
    input->Append(col0);
    input->Append(col1);

    auto expr = FuncExpr("array", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_LONG)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_LONG))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *arrayResult = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(arrayResult, nullptr);

    VerifyArraySize(arrayResult, 0, 2);
    VerifyArrayElement<int64_t>(arrayResult, 0, 0, kMin);
    VerifyArrayElement<int64_t>(arrayResult, 0, 1, kMax);

    delete context;
    delete input;
    delete result;
}

TEST_F(ArrayConstructorTest, EmptyStringInVarcharArray)
{
    int rowSize = 1;
    using VarcharVec = Vector<LargeStringContainer<std::string_view>>;

    auto *col0 = new VarcharVec(rowSize);
    col0->SetValue(0, std::string_view(""));

    auto *col1 = new VarcharVec(rowSize);
    col1->SetValue(0, std::string_view("non-empty"));

    auto *input = new VectorBatch(rowSize);
    input->Append(col0);
    input->Append(col1);

    auto expr = FuncExpr("array", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_VARCHAR)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_VARCHAR))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *arrayResult = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(arrayResult, nullptr);

    VerifyArraySize(arrayResult, 0, 2);
    VerifyVarcharElement(arrayResult, 0, 0, "");
    VerifyVarcharElement(arrayResult, 0, 1, "non-empty");

    delete context;
    delete input;
    delete result;
}

TEST_F(ArrayConstructorTest, NestedArrayBasic)
{
    int rowSize = 2;

    auto innerElem0 = std::shared_ptr<BaseVector>(new Vector<int32_t>(4));
    auto *typedInner0 = dynamic_cast<Vector<int32_t> *>(innerElem0.get());
    typedInner0->SetValue(0, 1);
    typedInner0->SetValue(1, 2);
    typedInner0->SetValue(2, 10);
    typedInner0->SetValue(3, 20);
    auto *arr0 = new ArrayVector(rowSize, innerElem0);
    arr0->SetOffset(0, 0);
    arr0->SetOffset(1, 2);
    arr0->SetOffset(2, 4);

    auto innerElem1 = std::shared_ptr<BaseVector>(new Vector<int32_t>(4));
    auto *typedInner1 = dynamic_cast<Vector<int32_t> *>(innerElem1.get());
    typedInner1->SetValue(0, 3);
    typedInner1->SetValue(1, 4);
    typedInner1->SetValue(2, 30);
    typedInner1->SetValue(3, 40);
    auto *arr1 = new ArrayVector(rowSize, innerElem1);
    arr1->SetOffset(0, 0);
    arr1->SetOffset(1, 2);
    arr1->SetOffset(2, 4);

    auto *input = new VectorBatch(rowSize);
    input->Append(arr0);
    input->Append(arr1);

    auto expr = FuncExpr("array", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_ARRAY))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *outerResult = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(outerResult, nullptr);

    EXPECT_EQ(outerResult->GetSize(0), 2);
    EXPECT_EQ(outerResult->GetSize(1), 2);

    auto *middleVec = dynamic_cast<ArrayVector *>(outerResult->GetElementVector().get());
    ASSERT_NE(middleVec, nullptr);

    auto innerElems = middleVec->GetElementVector();
    auto *typedInnerElems = dynamic_cast<Vector<int32_t> *>(innerElems.get());
    ASSERT_NE(typedInnerElems, nullptr);

    // row 0: array([1,2], [3,4])
    // middleIdx 0 -> [1,2], middleIdx 1 -> [3,4]
    EXPECT_EQ(middleVec->GetSize(0), 2);
    int64_t off0 = middleVec->GetOffset(0);
    EXPECT_EQ(typedInnerElems->GetValue(static_cast<int32_t>(off0)), 1);
    EXPECT_EQ(typedInnerElems->GetValue(static_cast<int32_t>(off0 + 1)), 2);

    EXPECT_EQ(middleVec->GetSize(1), 2);
    int64_t off1 = middleVec->GetOffset(1);
    EXPECT_EQ(typedInnerElems->GetValue(static_cast<int32_t>(off1)), 3);
    EXPECT_EQ(typedInnerElems->GetValue(static_cast<int32_t>(off1 + 1)), 4);

    // row 1: array([10,20], [30,40])
    // middleIdx 2 -> [10,20], middleIdx 3 -> [30,40]
    EXPECT_EQ(middleVec->GetSize(2), 2);
    int64_t off2 = middleVec->GetOffset(2);
    EXPECT_EQ(typedInnerElems->GetValue(static_cast<int32_t>(off2)), 10);
    EXPECT_EQ(typedInnerElems->GetValue(static_cast<int32_t>(off2 + 1)), 20);

    EXPECT_EQ(middleVec->GetSize(3), 2);
    int64_t off3 = middleVec->GetOffset(3);
    EXPECT_EQ(typedInnerElems->GetValue(static_cast<int32_t>(off3)), 30);
    EXPECT_EQ(typedInnerElems->GetValue(static_cast<int32_t>(off3 + 1)), 40);

    delete context;
    delete input;
    delete result;
}

TEST_F(ArrayConstructorTest, NestedArrayWithNull)
{
    int rowSize = 2;

    auto innerElem0 = std::shared_ptr<BaseVector>(new Vector<int32_t>(3));
    auto *typedInner0 = dynamic_cast<Vector<int32_t> *>(innerElem0.get());
    typedInner0->SetValue(0, 1);
    typedInner0->SetValue(1, 2);
    typedInner0->SetValue(2, 5);
    auto *arr0 = new ArrayVector(rowSize, innerElem0);
    arr0->SetOffset(0, 0);
    arr0->SetOffset(1, 2);
    arr0->SetOffset(2, 3);

    auto innerElem1 = std::shared_ptr<BaseVector>(new Vector<int32_t>(2));
    auto *typedInner1 = dynamic_cast<Vector<int32_t> *>(innerElem1.get());
    typedInner1->SetValue(0, 3);
    typedInner1->SetValue(1, 4);
    auto *arr1 = new ArrayVector(rowSize, innerElem1);
    arr1->SetOffset(0, 0);
    arr1->SetOffset(1, 2);
    arr1->SetNull(1);
    arr1->SetOffset(2, 2);

    auto *input = new VectorBatch(rowSize);
    input->Append(arr0);
    input->Append(arr1);

    auto expr = FuncExpr("array", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_ARRAY))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *outerResult = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(outerResult, nullptr);

    EXPECT_EQ(outerResult->GetSize(0), 2);
    EXPECT_EQ(outerResult->GetSize(1), 2);

    auto *middleVec = dynamic_cast<ArrayVector *>(outerResult->GetElementVector().get());
    ASSERT_NE(middleVec, nullptr);

    auto innerElems = middleVec->GetElementVector();
    auto *typedInnerElems = dynamic_cast<Vector<int32_t> *>(innerElems.get());
    ASSERT_NE(typedInnerElems, nullptr);

    // row 0: array([1,2], [3,4])
    EXPECT_EQ(middleVec->GetSize(0), 2);
    EXPECT_FALSE(middleVec->IsNull(0));
    int64_t off0 = middleVec->GetOffset(0);
    EXPECT_EQ(typedInnerElems->GetValue(static_cast<int32_t>(off0)), 1);
    EXPECT_EQ(typedInnerElems->GetValue(static_cast<int32_t>(off0 + 1)), 2);

    EXPECT_EQ(middleVec->GetSize(1), 2);
    EXPECT_FALSE(middleVec->IsNull(1));
    int64_t off1 = middleVec->GetOffset(1);
    EXPECT_EQ(typedInnerElems->GetValue(static_cast<int32_t>(off1)), 3);
    EXPECT_EQ(typedInnerElems->GetValue(static_cast<int32_t>(off1 + 1)), 4);

    // row 1: array([5], NULL)
    EXPECT_EQ(middleVec->GetSize(2), 1);
    EXPECT_FALSE(middleVec->IsNull(2));
    int64_t off2 = middleVec->GetOffset(2);
    EXPECT_EQ(typedInnerElems->GetValue(static_cast<int32_t>(off2)), 5);

    EXPECT_TRUE(middleVec->IsNull(3));

    delete context;
    delete input;
    delete result;
}

TEST_F(ArrayConstructorTest, NestedArrayDifferentSizes)
{
    int rowSize = 1;

    auto innerElem0 = std::shared_ptr<BaseVector>(new Vector<int32_t>(1));
    auto *typedInner0 = dynamic_cast<Vector<int32_t> *>(innerElem0.get());
    typedInner0->SetValue(0, 100);
    auto *arr0 = new ArrayVector(rowSize, innerElem0);
    arr0->SetOffset(0, 0);
    arr0->SetOffset(1, 1);

    auto innerElem1 = std::shared_ptr<BaseVector>(new Vector<int32_t>(3));
    auto *typedInner1 = dynamic_cast<Vector<int32_t> *>(innerElem1.get());
    typedInner1->SetValue(0, 200);
    typedInner1->SetValue(1, 300);
    typedInner1->SetValue(2, 400);
    auto *arr1 = new ArrayVector(rowSize, innerElem1);
    arr1->SetOffset(0, 0);
    arr1->SetOffset(1, 3);

    auto *input = new VectorBatch(rowSize);
    input->Append(arr0);
    input->Append(arr1);

    auto expr = FuncExpr("array", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_ARRAY))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *outerResult = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(outerResult, nullptr);

    EXPECT_EQ(outerResult->GetSize(0), 2);

    auto *middleVec = dynamic_cast<ArrayVector *>(outerResult->GetElementVector().get());
    ASSERT_NE(middleVec, nullptr);

    auto innerElems = middleVec->GetElementVector();
    auto *typedInnerElems = dynamic_cast<Vector<int32_t> *>(innerElems.get());
    ASSERT_NE(typedInnerElems, nullptr);

    // middleIdx 0 -> [100], middleIdx 1 -> [200, 300, 400]
    EXPECT_EQ(middleVec->GetSize(0), 1);
    int64_t off0 = middleVec->GetOffset(0);
    EXPECT_EQ(typedInnerElems->GetValue(static_cast<int32_t>(off0)), 100);

    EXPECT_EQ(middleVec->GetSize(1), 3);
    int64_t off1 = middleVec->GetOffset(1);
    EXPECT_EQ(typedInnerElems->GetValue(static_cast<int32_t>(off1)), 200);
    EXPECT_EQ(typedInnerElems->GetValue(static_cast<int32_t>(off1 + 1)), 300);
    EXPECT_EQ(typedInnerElems->GetValue(static_cast<int32_t>(off1 + 2)), 400);

    delete context;
    delete input;
    delete result;
}

namespace {
using VarcharVector = Vector<LargeStringContainer<std::string_view>>;

MapVector *BuildStringIntMapColumn(int32_t mapRows, const std::vector<int32_t> &offsets,
    const std::vector<std::string> &keys, const std::vector<int32_t> &vals)
{
    auto keyVector = std::make_shared<VarcharVector>(static_cast<int32_t>(keys.size()));
    for (int32_t i = 0; i < static_cast<int32_t>(keys.size()); ++i) {
        keyVector->SetValue(i, std::string_view(keys[i].data(), keys[i].size()));
    }
    auto valueVector = std::make_shared<Vector<int32_t>>(static_cast<int32_t>(vals.size()));
    for (int32_t i = 0; i < static_cast<int32_t>(vals.size()); ++i) {
        valueVector->SetValue(i, vals[i]);
    }
    auto *mapVector = new MapVector(mapRows, keyVector, valueVector);
    for (size_t i = 0; i < offsets.size(); ++i) {
        mapVector->SetOffset(static_cast<int32_t>(i), offsets[i]);
    }
    return mapVector;
}

MapVector *BuildAllNullMapColumn(int32_t numRows)
{
    auto keyVector = std::make_shared<VarcharVector>(0);
    auto valueVector = std::make_shared<Vector<int32_t>>(0);
    auto *mapVector = new MapVector(numRows, keyVector, valueVector);
    for (int32_t i = 0; i <= numRows; ++i) {
        mapVector->SetOffset(i, 0);
    }
    for (int32_t r = 0; r < numRows; ++r) {
        mapVector->SetNull(r);
    }
    return mapVector;
}
} // namespace

TEST_F(ArrayConstructorTest, MapArrayBasic)
{
    int rowSize = 1;
    auto *mapCol0 = BuildStringIntMapColumn(1, { 0, 1 }, { "a" }, { 1 });
    auto *mapCol1 = BuildStringIntMapColumn(1, { 0, 2 }, { "b", "c" }, { 2, 3 });

    auto *input = new VectorBatch(rowSize);
    input->Append(mapCol0);
    input->Append(mapCol1);

    auto mapType = std::make_shared<MapType>(omniruntime::type::VarcharType(10), omniruntime::type::IntType());
    auto expr = FuncExpr("array", {
        new FieldExpr(0, mapType),
        new FieldExpr(1, mapType)
    }, std::make_shared<ArrayType>(mapType));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *outerResult = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(outerResult, nullptr);
    EXPECT_EQ(outerResult->GetSize(0), 2);

    auto *middleMap = dynamic_cast<MapVector *>(outerResult->GetElementVector().get());
    ASSERT_NE(middleMap, nullptr);
    EXPECT_EQ(middleMap->GetSize(0), 1);
    EXPECT_EQ(middleMap->GetSize(1), 2);
    auto *kvec = dynamic_cast<VarcharVector *>(middleMap->GetKeyVector().get());
    auto *vvec = dynamic_cast<Vector<int32_t> *>(middleMap->GetValueVector().get());
    ASSERT_NE(kvec, nullptr);
    ASSERT_NE(vvec, nullptr);
    EXPECT_EQ(kvec->GetValue(static_cast<int32_t>(middleMap->GetOffset(0))), std::string_view("a"));
    EXPECT_EQ(vvec->GetValue(static_cast<int32_t>(middleMap->GetOffset(0))), 1);
    int64_t o1 = middleMap->GetOffset(1);
    EXPECT_EQ(kvec->GetValue(static_cast<int32_t>(o1)), std::string_view("b"));
    EXPECT_EQ(vvec->GetValue(static_cast<int32_t>(o1)), 2);
    EXPECT_EQ(kvec->GetValue(static_cast<int32_t>(o1 + 1)), std::string_view("c"));
    EXPECT_EQ(vvec->GetValue(static_cast<int32_t>(o1 + 1)), 3);

    delete context;
    delete input;
    delete result;
}

TEST_F(ArrayConstructorTest, MapArraySecondArgumentNull)
{
    int rowSize = 1;
    auto *mapCol0 = BuildStringIntMapColumn(1, { 0, 1 }, { "x" }, { 9 });
    auto *mapCol1 = BuildAllNullMapColumn(1);

    auto *input = new VectorBatch(rowSize);
    input->Append(mapCol0);
    input->Append(mapCol1);

    auto mapType = std::make_shared<MapType>(omniruntime::type::VarcharType(10), omniruntime::type::IntType());
    auto expr = FuncExpr("array", {
        new FieldExpr(0, mapType),
        new FieldExpr(1, mapType)
    }, std::make_shared<ArrayType>(mapType));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *outerResult = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(outerResult, nullptr);
    auto *middleMap = dynamic_cast<MapVector *>(outerResult->GetElementVector().get());
    ASSERT_NE(middleMap, nullptr);
    EXPECT_FALSE(middleMap->IsNull(0));
    EXPECT_TRUE(middleMap->IsNull(1));

    delete context;
    delete input;
    delete result;
}

TEST_F(ArrayConstructorTest, StructArrayBasic)
{
    int rowSize = 1;

    auto name0 = std::make_shared<VarcharVector>(1);
    name0->SetValue(0, std::string_view("aa"));
    auto age0 = std::make_shared<Vector<int32_t>>(1);
    age0->SetValue(0, 10);
    std::vector<std::shared_ptr<BaseVector>> kids0 = { name0, age0 };
    auto *row0 = new RowVector(1, kids0);

    auto name1 = std::make_shared<VarcharVector>(1);
    name1->SetValue(0, std::string_view("bb"));
    auto age1 = std::make_shared<Vector<int32_t>>(1);
    age1->SetValue(0, 20);
    std::vector<std::shared_ptr<BaseVector>> kids1 = { name1, age1 };
    auto *row1 = new RowVector(1, kids1);

    auto *input = new VectorBatch(rowSize);
    input->Append(row0);
    input->Append(row1);

    std::vector<DataTypePtr> rowFieldTypes = { omniruntime::type::VarcharType(10), omniruntime::type::IntType() };
    auto structType = std::make_shared<RowType>(rowFieldTypes);
    auto expr = FuncExpr("array", {
        new FieldExpr(0, structType),
        new FieldExpr(1, structType)
    }, std::make_shared<ArrayType>(structType));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *outerResult = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(outerResult, nullptr);
    EXPECT_EQ(outerResult->GetSize(0), 2);

    auto *elemRow = dynamic_cast<RowVector *>(outerResult->GetElementVector().get());
    ASSERT_NE(elemRow, nullptr);
    EXPECT_EQ(elemRow->GetSize(), 2);
    auto *nOut = dynamic_cast<VarcharVector *>(elemRow->ChildAt(0).get());
    auto *aOut = dynamic_cast<Vector<int32_t> *>(elemRow->ChildAt(1).get());
    ASSERT_NE(nOut, nullptr);
    ASSERT_NE(aOut, nullptr);
    EXPECT_EQ(nOut->GetValue(0), std::string_view("aa"));
    EXPECT_EQ(aOut->GetValue(0), 10);
    EXPECT_EQ(nOut->GetValue(1), std::string_view("bb"));
    EXPECT_EQ(aOut->GetValue(1), 20);

    delete context;
    delete input;
    delete result;
}
