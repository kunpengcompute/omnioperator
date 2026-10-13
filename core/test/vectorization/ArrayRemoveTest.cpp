/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Unit tests for array_remove function
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
#include "type/decimal_operations.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::mem;
using namespace omniruntime::op;
using namespace omniruntime::expressions;
using namespace omniruntime::TestUtil;

class ArrayRemoveTest : public ::testing::Test {
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

    void VerifyVarcharElement(ArrayVector *arrayVec, int32_t row, int32_t elemIdx, const std::string &expected)
    {
        using VarcharVector = Vector<LargeStringContainer<std::string_view>>;
        auto elemVec = arrayVec->GetElementVector();
        auto *typedVec = dynamic_cast<VarcharVector *>(elemVec.get());
        ASSERT_NE(typedVec, nullptr);
        int64_t offset = arrayVec->GetOffset(row);
        EXPECT_EQ(typedVec->GetValue(static_cast<int32_t>(offset + elemIdx)), expected);
    }
};

TEST_F(ArrayRemoveTest, IntegerRemove)
{
    int rowSize = 3;
    auto arrayElemType = std::make_shared<DataType>(OMNI_INT);
    auto types = DataTypes({arrayElemType});

    // Array data: row0=[1,2,3,2,4], row1=[5,5,5], row2=[6,7,8]
    int32_t col[] = {1, 2, 3, 2, 4, 5, 5, 5, 6, 7, 8};
    std::vector<int32_t> offset = {0, 5, 8, 11};
    auto input = CreateArrayVectorBatch(types, offset, rowSize, 11, col);

    auto *elementVec = new Vector<int32_t>(rowSize);
    elementVec->SetValue(0, 2);
    elementVec->SetValue(1, 5);
    elementVec->SetValue(2, 99);
    input->Append(elementVec);

    auto expr = FuncExpr("array_remove", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_INT))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *arrayResult = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(arrayResult, nullptr);

    // row 0: remove 2 from [1,2,3,2,4] -> [1,3,4]
    VerifyArraySize(arrayResult, 0, 3);
    VerifyArrayElement<int32_t>(arrayResult, 0, 0, 1);
    VerifyArrayElement<int32_t>(arrayResult, 0, 1, 3);
    VerifyArrayElement<int32_t>(arrayResult, 0, 2, 4);

    // row 1: remove 5 from [5,5,5] -> []
    VerifyArraySize(arrayResult, 1, 0);

    // row 2: remove 99 from [6,7,8] -> [6,7,8] (no match)
    VerifyArraySize(arrayResult, 2, 3);
    VerifyArrayElement<int32_t>(arrayResult, 2, 0, 6);
    VerifyArrayElement<int32_t>(arrayResult, 2, 1, 7);
    VerifyArrayElement<int32_t>(arrayResult, 2, 2, 8);

    delete context;
    delete input;
    delete result;
}

TEST_F(ArrayRemoveTest, LongRemove)
{
    int rowSize = 2;
    auto arrayElemType = std::make_shared<DataType>(OMNI_LONG);
    auto types = DataTypes({arrayElemType});

    int64_t col[] = {100L, 200L, 300L, 200L, 400L, 500L};
    std::vector<int32_t> offset = {0, 3, 6};
    auto input = CreateArrayVectorBatch(types, offset, rowSize, 6, col);

    auto *elementVec = new Vector<int64_t>(rowSize);
    elementVec->SetValue(0, 200L);
    elementVec->SetValue(1, 500L);
    input->Append(elementVec);

    auto expr = FuncExpr("array_remove", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_LONG))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *arrayResult = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(arrayResult, nullptr);

    // row 0: remove 200 from [100,200,300] -> [100,300]
    VerifyArraySize(arrayResult, 0, 2);
    VerifyArrayElement<int64_t>(arrayResult, 0, 0, 100L);
    VerifyArrayElement<int64_t>(arrayResult, 0, 1, 300L);

    // row 1: remove 500 from [200,400,500] -> [200,400]
    VerifyArraySize(arrayResult, 1, 2);
    VerifyArrayElement<int64_t>(arrayResult, 1, 0, 200L);
    VerifyArrayElement<int64_t>(arrayResult, 1, 1, 400L);

    delete context;
    delete input;
    delete result;
}

TEST_F(ArrayRemoveTest, ByteRemove)
{
    int rowSize = 2;
    auto arrayElemType = std::make_shared<DataType>(OMNI_BYTE);
    auto types = DataTypes({arrayElemType});

    int8_t col[] = {1, 2, 3, 2, 4, 5};
    std::vector<int32_t> offset = {0, 4, 6};
    auto input = CreateArrayVectorBatch(types, offset, rowSize, 6, col);

    auto *elementVec = new Vector<int8_t>(rowSize);
    elementVec->SetValue(0, static_cast<int8_t>(2));
    elementVec->SetValue(1, static_cast<int8_t>(5));
    input->Append(elementVec);

    auto expr = FuncExpr("array_remove", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_BYTE))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *arrayResult = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(arrayResult, nullptr);

    // row 0: remove 2 from [1,2,3,2] -> [1,3]
    VerifyArraySize(arrayResult, 0, 2);
    VerifyArrayElement<int8_t>(arrayResult, 0, 0, 1);
    VerifyArrayElement<int8_t>(arrayResult, 0, 1, 3);

    // row 1: remove 5 from [4,5] -> [4]
    VerifyArraySize(arrayResult, 1, 1);
    VerifyArrayElement<int8_t>(arrayResult, 1, 0, 4);

    delete context;
    delete input;
    delete result;
}

TEST_F(ArrayRemoveTest, ShortRemove)
{
    int rowSize = 2;
    auto arrayElemType = std::make_shared<DataType>(OMNI_SHORT);
    auto types = DataTypes({arrayElemType});

    int16_t col[] = {1000, 2000, 3000, 2000, 4000, 5000};
    std::vector<int32_t> offset = {0, 3, 6};
    auto input = CreateArrayVectorBatch(types, offset, rowSize, 6, col);

    auto *elementVec = new Vector<int16_t>(rowSize);
    elementVec->SetValue(0, static_cast<int16_t>(2000));
    elementVec->SetValue(1, static_cast<int16_t>(5000));
    input->Append(elementVec);

    auto expr = FuncExpr("array_remove", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_SHORT))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *arrayResult = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(arrayResult, nullptr);

    // row 0: remove 2000 from [1000,2000,3000] -> [1000,3000]
    VerifyArraySize(arrayResult, 0, 2);
    VerifyArrayElement<int16_t>(arrayResult, 0, 0, 1000);
    VerifyArrayElement<int16_t>(arrayResult, 0, 1, 3000);

    // row 1: remove 5000 from [2000,4000,5000] -> [2000,4000]
    VerifyArraySize(arrayResult, 1, 2);
    VerifyArrayElement<int16_t>(arrayResult, 1, 0, 2000);
    VerifyArrayElement<int16_t>(arrayResult, 1, 1, 4000);

    delete context;
    delete input;
    delete result;
}

TEST_F(ArrayRemoveTest, DoubleRemove)
{
    int rowSize = 2;
    auto arrayElemType = std::make_shared<DataType>(OMNI_DOUBLE);
    auto types = DataTypes({arrayElemType});

    double col[] = {1.1, 2.2, 3.3, 2.2, 4.4, 5.5};
    std::vector<int32_t> offset = {0, 4, 6};
    auto input = CreateArrayVectorBatch(types, offset, rowSize, 6, col);

    auto *elementVec = new Vector<double>(rowSize);
    elementVec->SetValue(0, 2.2);
    elementVec->SetValue(1, 5.5);
    input->Append(elementVec);

    auto expr = FuncExpr("array_remove", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_DOUBLE))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *arrayResult = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(arrayResult, nullptr);

    // row 0: remove 2.2 from [1.1,2.2,3.3,2.2] -> [1.1,3.3]
    VerifyArraySize(arrayResult, 0, 2);
    VerifyArrayElement<double>(arrayResult, 0, 0, 1.1);
    VerifyArrayElement<double>(arrayResult, 0, 1, 3.3);

    // row 1: remove 5.5 from [4.4,5.5] -> [4.4]
    VerifyArraySize(arrayResult, 1, 1);
    VerifyArrayElement<double>(arrayResult, 1, 0, 4.4);

    delete context;
    delete input;
    delete result;
}

TEST_F(ArrayRemoveTest, FloatRemove)
{
    int rowSize = 2;
    auto arrayElemType = std::make_shared<DataType>(OMNI_FLOAT);
    auto types = DataTypes({arrayElemType});

    float col[] = {1.5f, 2.5f, 3.5f, 2.5f, 4.5f, 5.5f};
    std::vector<int32_t> offset = {0, 4, 6};
    auto input = CreateArrayVectorBatch(types, offset, rowSize, 6, col);

    auto *elementVec = new Vector<float>(rowSize);
    elementVec->SetValue(0, 2.5f);
    elementVec->SetValue(1, 5.5f);
    input->Append(elementVec);

    auto expr = FuncExpr("array_remove", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_FLOAT))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *arrayResult = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(arrayResult, nullptr);

    // row 0: remove 2.5 from [1.5,2.5,3.5,2.5] -> [1.5,3.5]
    VerifyArraySize(arrayResult, 0, 2);
    VerifyArrayElement<float>(arrayResult, 0, 0, 1.5f);
    VerifyArrayElement<float>(arrayResult, 0, 1, 3.5f);

    // row 1: remove 5.5 from [4.5,5.5] -> [4.5]
    VerifyArraySize(arrayResult, 1, 1);
    VerifyArrayElement<float>(arrayResult, 1, 0, 4.5f);

    delete context;
    delete input;
    delete result;
}

TEST_F(ArrayRemoveTest, BooleanRemove)
{
    int rowSize = 2;
    auto arrayElemType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto types = DataTypes({arrayElemType});

    bool col[] = {true, false, true, true, false, false};
    std::vector<int32_t> offset = {0, 3, 6};
    auto input = CreateArrayVectorBatch(types, offset, rowSize, 6, col);

    auto *elementVec = new Vector<bool>(rowSize);
    elementVec->SetValue(0, true);
    elementVec->SetValue(1, false);
    input->Append(elementVec);

    auto expr = FuncExpr("array_remove", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_BOOLEAN))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *arrayResult = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(arrayResult, nullptr);

    // row 0: remove true from [true,false,true] -> [false]
    VerifyArraySize(arrayResult, 0, 1);
    VerifyArrayElement<bool>(arrayResult, 0, 0, false);

    // row 1: remove false from [true,false,false] -> [true]
    VerifyArraySize(arrayResult, 1, 1);
    VerifyArrayElement<bool>(arrayResult, 1, 0, true);

    delete context;
    delete input;
    delete result;
}

TEST_F(ArrayRemoveTest, FloatNaNRemove)
{
    int rowSize = 2;

    constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();

    auto *srcElementVec = new Vector<float>(6);
    srcElementVec->SetValue(0, 1.0f);
    srcElementVec->SetValue(1, kNaN);
    srcElementVec->SetValue(2, 3.0f);
    srcElementVec->SetValue(3, kNaN);
    srcElementVec->SetValue(4, 2.0f);
    srcElementVec->SetValue(5, kNaN);

    auto srcElementShared = std::shared_ptr<BaseVector>(srcElementVec);
    auto *arrayVec = new ArrayVector(rowSize, srcElementShared);
    arrayVec->SetOffset(0, 0);
    arrayVec->SetOffset(1, 3);
    arrayVec->SetOffset(2, 6);

    auto *input = new VectorBatch(rowSize);
    input->Append(arrayVec);

    auto *elementVec = new Vector<float>(rowSize);
    elementVec->SetValue(0, kNaN);
    elementVec->SetValue(1, 2.0f);
    input->Append(elementVec);

    auto expr = FuncExpr("array_remove", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_FLOAT))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *arrayResult = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(arrayResult, nullptr);

    // row 0: remove NaN from [1.0, NaN, 3.0] -> [1.0, 3.0]
    VerifyArraySize(arrayResult, 0, 2);
    VerifyArrayElement<float>(arrayResult, 0, 0, 1.0f);
    VerifyArrayElement<float>(arrayResult, 0, 1, 3.0f);

    // row 1: remove 2.0 from [NaN, 2.0, NaN] -> [NaN, NaN]
    VerifyArraySize(arrayResult, 1, 2);
    auto elemResultVec = arrayResult->GetElementVector();
    auto *floatVec = dynamic_cast<Vector<float> *>(elemResultVec.get());
    ASSERT_NE(floatVec, nullptr);
    int64_t offset1 = arrayResult->GetOffset(1);
    EXPECT_TRUE(std::isnan(floatVec->GetValue(static_cast<int32_t>(offset1))));
    EXPECT_TRUE(std::isnan(floatVec->GetValue(static_cast<int32_t>(offset1 + 1))));

    delete context;
    delete input;
    delete result;
}

TEST_F(ArrayRemoveTest, DoubleNaNRemove)
{
    int rowSize = 1;

    constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

    auto *srcElementVec = new Vector<double>(4);
    srcElementVec->SetValue(0, kNaN);
    srcElementVec->SetValue(1, 1.0);
    srcElementVec->SetValue(2, kNaN);
    srcElementVec->SetValue(3, 2.0);

    auto srcElementShared = std::shared_ptr<BaseVector>(srcElementVec);
    auto *arrayVec = new ArrayVector(rowSize, srcElementShared);
    arrayVec->SetOffset(0, 0);
    arrayVec->SetOffset(1, 4);

    auto *input = new VectorBatch(rowSize);
    input->Append(arrayVec);

    auto *elementVec = new Vector<double>(rowSize);
    elementVec->SetValue(0, kNaN);
    input->Append(elementVec);

    auto expr = FuncExpr("array_remove", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_DOUBLE))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *arrayResult = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(arrayResult, nullptr);

    // remove NaN from [NaN,1.0,NaN,2.0] -> [1.0,2.0]
    VerifyArraySize(arrayResult, 0, 2);
    VerifyArrayElement<double>(arrayResult, 0, 0, 1.0);
    VerifyArrayElement<double>(arrayResult, 0, 1, 2.0);

    delete context;
    delete input;
    delete result;
}

TEST_F(ArrayRemoveTest, ArrayWithNullElements)
{
    int rowSize = 2;

    auto *srcElementVec = new Vector<int32_t>(7);
    srcElementVec->SetValue(0, 1);
    srcElementVec->SetNull(1);
    srcElementVec->SetValue(2, 3);
    srcElementVec->SetNull(3);
    srcElementVec->SetValue(4, 3);
    srcElementVec->SetNull(5);
    srcElementVec->SetValue(6, 4);

    auto srcElementShared = std::shared_ptr<BaseVector>(srcElementVec);
    auto *arrayVec = new ArrayVector(rowSize, srcElementShared);
    arrayVec->SetOffset(0, 0);
    arrayVec->SetOffset(1, 4);
    arrayVec->SetOffset(2, 7);

    auto *input = new VectorBatch(rowSize);
    input->Append(arrayVec);

    auto *elementVec = new Vector<int32_t>(rowSize);
    elementVec->SetValue(0, 3);
    elementVec->SetValue(1, 3);
    input->Append(elementVec);

    auto expr = FuncExpr("array_remove", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_INT))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *arrayResult = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(arrayResult, nullptr);

    // row 0: remove 3 from [1,null,3,null] -> [1,null,null]
    VerifyArraySize(arrayResult, 0, 3);
    VerifyArrayElement<int32_t>(arrayResult, 0, 0, 1);
    VerifyArrayElementNull<int32_t>(arrayResult, 0, 1);
    VerifyArrayElementNull<int32_t>(arrayResult, 0, 2);

    // row 1: remove 3 from [3,null,4] -> [null,4]
    VerifyArraySize(arrayResult, 1, 2);
    VerifyArrayElementNull<int32_t>(arrayResult, 1, 0);
    VerifyArrayElement<int32_t>(arrayResult, 1, 1, 4);

    delete context;
    delete input;
    delete result;
}

TEST_F(ArrayRemoveTest, EmptyArrayRemove)
{
    int rowSize = 1;
    auto arrayElemType = std::make_shared<DataType>(OMNI_INT);
    auto types = DataTypes({arrayElemType});

    int32_t col[] = {};
    std::vector<int32_t> offset = {0, 0};
    auto input = CreateArrayVectorBatch(types, offset, rowSize, 0, col);

    auto *elementVec = new Vector<int32_t>(rowSize);
    elementVec->SetValue(0, 42);
    input->Append(elementVec);

    auto expr = FuncExpr("array_remove", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_INT))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *arrayResult = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(arrayResult, nullptr);

    // remove 42 from [] -> []
    VerifyArraySize(arrayResult, 0, 0);

    delete context;
    delete input;
    delete result;
}

TEST_F(ArrayRemoveTest, NoMatchRemove)
{
    int rowSize = 1;
    auto arrayElemType = std::make_shared<DataType>(OMNI_INT);
    auto types = DataTypes({arrayElemType});

    int32_t col[] = {1, 2, 3};
    std::vector<int32_t> offset = {0, 3};
    auto input = CreateArrayVectorBatch(types, offset, rowSize, 3, col);

    auto *elementVec = new Vector<int32_t>(rowSize);
    elementVec->SetValue(0, 99);
    input->Append(elementVec);

    auto expr = FuncExpr("array_remove", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_INT))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *arrayResult = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(arrayResult, nullptr);

    // remove 99 from [1,2,3] -> [1,2,3]
    VerifyArraySize(arrayResult, 0, 3);
    VerifyArrayElement<int32_t>(arrayResult, 0, 0, 1);
    VerifyArrayElement<int32_t>(arrayResult, 0, 1, 2);
    VerifyArrayElement<int32_t>(arrayResult, 0, 2, 3);

    delete context;
    delete input;
    delete result;
}

TEST_F(ArrayRemoveTest, AllMatchRemove)
{
    int rowSize = 1;
    auto arrayElemType = std::make_shared<DataType>(OMNI_INT);
    auto types = DataTypes({arrayElemType});

    int32_t col[] = {5, 5, 5, 5};
    std::vector<int32_t> offset = {0, 4};
    auto input = CreateArrayVectorBatch(types, offset, rowSize, 4, col);

    auto *elementVec = new Vector<int32_t>(rowSize);
    elementVec->SetValue(0, 5);
    input->Append(elementVec);

    auto expr = FuncExpr("array_remove", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_INT))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *arrayResult = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(arrayResult, nullptr);

    // remove 5 from [5,5,5,5] -> []
    VerifyArraySize(arrayResult, 0, 0);

    delete context;
    delete input;
    delete result;
}

TEST_F(ArrayRemoveTest, NullElementToRemove)
{
    int rowSize = 1;

    auto *srcElementVec = new Vector<int32_t>(3);
    srcElementVec->SetValue(0, 1);
    srcElementVec->SetValue(1, 2);
    srcElementVec->SetValue(2, 3);

    auto srcElementShared = std::shared_ptr<BaseVector>(srcElementVec);
    auto *arrayVec = new ArrayVector(rowSize, srcElementShared);
    arrayVec->SetOffset(0, 0);
    arrayVec->SetOffset(1, 3);

    auto *input = new VectorBatch(rowSize);
    input->Append(arrayVec);

    auto *elementVec = new Vector<int32_t>(rowSize);
    elementVec->SetNull(0);
    input->Append(elementVec);

    auto expr = FuncExpr("array_remove", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_INT))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *arrayResult = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(arrayResult, nullptr);

    // remove null from [1,2,3] -> [1,2,3] (null element means return original)
    VerifyArraySize(arrayResult, 0, 3);
    VerifyArrayElement<int32_t>(arrayResult, 0, 0, 1);
    VerifyArrayElement<int32_t>(arrayResult, 0, 1, 2);
    VerifyArrayElement<int32_t>(arrayResult, 0, 2, 3);

    delete context;
    delete input;
    delete result;
}

TEST_F(ArrayRemoveTest, NullArrayRemove)
{
    int rowSize = 2;

    auto *srcElementVec = new Vector<int32_t>(3);
    srcElementVec->SetValue(0, 1);
    srcElementVec->SetValue(1, 2);
    srcElementVec->SetValue(2, 3);

    auto srcElementShared = std::shared_ptr<BaseVector>(srcElementVec);
    auto *arrayVec = new ArrayVector(rowSize, srcElementShared);
    arrayVec->SetNull(0);
    arrayVec->SetOffset(0, 0);
    arrayVec->SetOffset(1, 0);
    arrayVec->SetOffset(2, 3);

    auto *input = new VectorBatch(rowSize);
    input->Append(arrayVec);

    auto *elementVec = new Vector<int32_t>(rowSize);
    elementVec->SetValue(0, 1);
    elementVec->SetValue(1, 2);
    input->Append(elementVec);

    auto expr = FuncExpr("array_remove", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_INT))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *arrayResult = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(arrayResult, nullptr);

    // row 0: null array -> null result
    EXPECT_TRUE(arrayResult->IsNull(0));

    // row 1: remove 2 from [1,2,3] -> [1,3]
    VerifyArraySize(arrayResult, 1, 2);
    VerifyArrayElement<int32_t>(arrayResult, 1, 0, 1);
    VerifyArrayElement<int32_t>(arrayResult, 1, 1, 3);

    delete context;
    delete input;
    delete result;
}

TEST_F(ArrayRemoveTest, VarcharRemove)
{
    using VarcharVec = Vector<LargeStringContainer<std::string_view>>;

    int rowSize = 2;

    auto *srcElementVec = new VarcharVec(6);
    std::string_view vals[] = {"apple", "banana", "cherry", "banana", "date", "fig"};
    for (int i = 0; i < 6; ++i) {
        srcElementVec->SetValue(i, vals[i]);
    }

    auto srcElementShared = std::shared_ptr<BaseVector>(srcElementVec);
    auto *arrayVec = new ArrayVector(rowSize, srcElementShared);
    arrayVec->SetOffset(0, 0);
    arrayVec->SetOffset(1, 4);
    arrayVec->SetOffset(2, 6);

    auto *input = new VectorBatch(rowSize);
    input->Append(arrayVec);

    auto *elementVec = new VarcharVec(rowSize);
    std::string_view rem0("banana");
    std::string_view rem1("fig");
    elementVec->SetValue(0, rem0);
    elementVec->SetValue(1, rem1);
    input->Append(elementVec);

    auto expr = FuncExpr("array_remove", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_VARCHAR))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *arrayResult = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(arrayResult, nullptr);

    // row 0: remove "banana" from ["apple","banana","cherry","banana"] -> ["apple","cherry"]
    VerifyArraySize(arrayResult, 0, 2);
    VerifyVarcharElement(arrayResult, 0, 0, "apple");
    VerifyVarcharElement(arrayResult, 0, 1, "cherry");

    // row 1: remove "fig" from ["date","fig"] -> ["date"]
    VerifyArraySize(arrayResult, 1, 1);
    VerifyVarcharElement(arrayResult, 1, 0, "date");

    delete context;
    delete input;
    delete result;
}

TEST_F(ArrayRemoveTest, Decimal128Remove)
{
    int rowSize = 1;

    Decimal128 val1(100L);
    Decimal128 val2(200L);
    Decimal128 val3(100L);
    Decimal128 val4(300L);

    auto *srcElementVec = new Vector<Decimal128>(4);
    srcElementVec->SetValue(0, val1);
    srcElementVec->SetValue(1, val2);
    srcElementVec->SetValue(2, val3);
    srcElementVec->SetValue(3, val4);

    auto srcElementShared = std::shared_ptr<BaseVector>(srcElementVec);
    auto *arrayVec = new ArrayVector(rowSize, srcElementShared);
    arrayVec->SetOffset(0, 0);
    arrayVec->SetOffset(1, 4);

    auto *input = new VectorBatch(rowSize);
    input->Append(arrayVec);

    auto *elementVec = new Vector<Decimal128>(rowSize);
    elementVec->SetValue(0, val1);
    input->Append(elementVec);

    auto expr = FuncExpr("array_remove", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_DECIMAL128))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *arrayResult = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(arrayResult, nullptr);

    // remove 100 from [100,200,100,300] -> [200,300]
    VerifyArraySize(arrayResult, 0, 2);
    VerifyArrayElement<Decimal128>(arrayResult, 0, 0, val2);
    VerifyArrayElement<Decimal128>(arrayResult, 0, 1, val4);

    delete context;
    delete input;
    delete result;
}

TEST_F(ArrayRemoveTest, IntegerBoundaryValues)
{
    int rowSize = 2;
    auto arrayElemType = std::make_shared<DataType>(OMNI_INT);
    auto types = DataTypes({arrayElemType});

    constexpr int32_t kMin = std::numeric_limits<int32_t>::min();
    constexpr int32_t kMax = std::numeric_limits<int32_t>::max();

    int32_t col[] = {kMin, 0, kMax, kMin, kMax};
    std::vector<int32_t> offset = {0, 3, 5};
    auto input = CreateArrayVectorBatch(types, offset, rowSize, 5, col);

    auto *elementVec = new Vector<int32_t>(rowSize);
    elementVec->SetValue(0, kMin);
    elementVec->SetValue(1, kMax);
    input->Append(elementVec);

    auto expr = FuncExpr("array_remove", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_INT))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *arrayResult = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(arrayResult, nullptr);

    // row 0: remove MIN from [MIN,0,MAX] -> [0,MAX]
    VerifyArraySize(arrayResult, 0, 2);
    VerifyArrayElement<int32_t>(arrayResult, 0, 0, 0);
    VerifyArrayElement<int32_t>(arrayResult, 0, 1, kMax);

    // row 1: remove MAX from [MIN,MAX] -> [MIN]
    VerifyArraySize(arrayResult, 1, 1);
    VerifyArrayElement<int32_t>(arrayResult, 1, 0, kMin);

    delete context;
    delete input;
    delete result;
}

TEST_F(ArrayRemoveTest, MultipleRowsMixedResults)
{
    int rowSize = 4;
    auto arrayElemType = std::make_shared<DataType>(OMNI_INT);
    auto types = DataTypes({arrayElemType});

    // row0=[1,2,3], row1=[4,4,4], row2=[5], row3=[6,7,8,9]
    int32_t col[] = {1, 2, 3, 4, 4, 4, 5, 6, 7, 8, 9};
    std::vector<int32_t> offset = {0, 3, 6, 7, 11};
    auto input = CreateArrayVectorBatch(types, offset, rowSize, 11, col);

    auto *elementVec = new Vector<int32_t>(rowSize);
    elementVec->SetValue(0, 2);
    elementVec->SetValue(1, 4);
    elementVec->SetValue(2, 5);
    elementVec->SetValue(3, 7);
    input->Append(elementVec);

    auto expr = FuncExpr("array_remove", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_INT))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *arrayResult = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(arrayResult, nullptr);

    // row 0: remove 2 from [1,2,3] -> [1,3]
    VerifyArraySize(arrayResult, 0, 2);
    VerifyArrayElement<int32_t>(arrayResult, 0, 0, 1);
    VerifyArrayElement<int32_t>(arrayResult, 0, 1, 3);

    // row 1: remove 4 from [4,4,4] -> []
    VerifyArraySize(arrayResult, 1, 0);

    // row 2: remove 5 from [5] -> []
    VerifyArraySize(arrayResult, 2, 0);

    // row 3: remove 7 from [6,7,8,9] -> [6,8,9]
    VerifyArraySize(arrayResult, 3, 3);
    VerifyArrayElement<int32_t>(arrayResult, 3, 0, 6);
    VerifyArrayElement<int32_t>(arrayResult, 3, 1, 8);
    VerifyArrayElement<int32_t>(arrayResult, 3, 2, 9);

    delete context;
    delete input;
    delete result;
}

TEST_F(ArrayRemoveTest, VarcharWithNullElements)
{
    using VarcharVec = Vector<LargeStringContainer<std::string_view>>;

    int rowSize = 1;

    auto *srcElementVec = new VarcharVec(4);
    std::string_view v0("hello");
    std::string_view v2("hello");
    std::string_view v3("world");
    srcElementVec->SetValue(0, v0);
    srcElementVec->SetNull(1);
    srcElementVec->SetValue(2, v2);
    srcElementVec->SetValue(3, v3);

    auto srcElementShared = std::shared_ptr<BaseVector>(srcElementVec);
    auto *arrayVec = new ArrayVector(rowSize, srcElementShared);
    arrayVec->SetOffset(0, 0);
    arrayVec->SetOffset(1, 4);

    auto *input = new VectorBatch(rowSize);
    input->Append(arrayVec);

    auto *elementVec = new VarcharVec(rowSize);
    std::string_view rem("hello");
    elementVec->SetValue(0, rem);
    input->Append(elementVec);

    auto expr = FuncExpr("array_remove", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_VARCHAR))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *arrayResult = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(arrayResult, nullptr);

    // remove "hello" from ["hello",null,"hello","world"] -> [null,"world"]
    VerifyArraySize(arrayResult, 0, 2);

    using VarcharResultVec = Vector<LargeStringContainer<std::string_view>>;
    auto elemResultVec = arrayResult->GetElementVector();
    auto *typedResultVec = dynamic_cast<VarcharResultVec *>(elemResultVec.get());
    ASSERT_NE(typedResultVec, nullptr);

    int64_t offset0 = arrayResult->GetOffset(0);
    EXPECT_TRUE(typedResultVec->IsNull(static_cast<int32_t>(offset0)));
    EXPECT_EQ(typedResultVec->GetValue(static_cast<int32_t>(offset0 + 1)), "world");

    delete context;
    delete input;
    delete result;
}
