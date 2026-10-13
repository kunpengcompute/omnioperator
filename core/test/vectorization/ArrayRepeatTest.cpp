/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Unit tests for array_repeat function
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

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::mem;
using namespace omniruntime::op;
using namespace omniruntime::expressions;
using namespace omniruntime::TestUtil;

class ArrayRepeatTest : public ::testing::Test {
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
};

TEST_F(ArrayRepeatTest, IntegerRepeat)
{
    int rowSize = 3;
    auto elementType = std::make_shared<DataType>(OMNI_INT);
    auto countType = std::make_shared<DataType>(OMNI_INT);
    auto types = DataTypes({elementType, countType});

    int32_t elements[] = {10, 20, 30};
    int32_t counts[] = {1, 3, 0};
    auto input = CreateVectorBatch(types, rowSize, elements, counts);

    auto expr = FuncExpr("array_repeat", {
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

    // row 0: repeat(10, 1) -> [10]
    VerifyArraySize(arrayResult, 0, 1);
    VerifyArrayElement<int32_t>(arrayResult, 0, 0, 10);

    // row 1: repeat(20, 3) -> [20, 20, 20]
    VerifyArraySize(arrayResult, 1, 3);
    VerifyArrayElement<int32_t>(arrayResult, 1, 0, 20);
    VerifyArrayElement<int32_t>(arrayResult, 1, 1, 20);
    VerifyArrayElement<int32_t>(arrayResult, 1, 2, 20);

    // row 2: repeat(30, 0) -> []
    VerifyArraySize(arrayResult, 2, 0);

    delete context;
    delete input;
    delete result;
}

TEST_F(ArrayRepeatTest, LongRepeat)
{
    int rowSize = 2;
    auto elementType = std::make_shared<DataType>(OMNI_LONG);
    auto countType = std::make_shared<DataType>(OMNI_INT);
    auto types = DataTypes({elementType, countType});

    int64_t elements[] = {100L, 200L};
    int32_t counts[] = {2, 1};
    auto input = CreateVectorBatch(types, rowSize, elements, counts);

    auto expr = FuncExpr("array_repeat", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_LONG)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_INT))
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
    VerifyArrayElement<int64_t>(arrayResult, 0, 1, 100L);

    VerifyArraySize(arrayResult, 1, 1);
    VerifyArrayElement<int64_t>(arrayResult, 1, 0, 200L);

    delete context;
    delete input;
    delete result;
}

TEST_F(ArrayRepeatTest, DoubleRepeat)
{
    int rowSize = 2;
    auto elementType = std::make_shared<DataType>(OMNI_DOUBLE);
    auto countType = std::make_shared<DataType>(OMNI_INT);
    auto types = DataTypes({elementType, countType});

    double elements[] = {3.14, -2.71};
    int32_t counts[] = {3, 2};
    auto input = CreateVectorBatch(types, rowSize, elements, counts);

    auto expr = FuncExpr("array_repeat", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_DOUBLE)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_INT))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *arrayResult = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(arrayResult, nullptr);

    VerifyArraySize(arrayResult, 0, 3);
    VerifyArrayElement<double>(arrayResult, 0, 0, 3.14);
    VerifyArrayElement<double>(arrayResult, 0, 1, 3.14);
    VerifyArrayElement<double>(arrayResult, 0, 2, 3.14);

    VerifyArraySize(arrayResult, 1, 2);
    VerifyArrayElement<double>(arrayResult, 1, 0, -2.71);
    VerifyArrayElement<double>(arrayResult, 1, 1, -2.71);

    delete context;
    delete input;
    delete result;
}

TEST_F(ArrayRepeatTest, FloatRepeat)
{
    int rowSize = 2;
    auto elementType = std::make_shared<DataType>(OMNI_FLOAT);
    auto countType = std::make_shared<DataType>(OMNI_INT);
    auto types = DataTypes({elementType, countType});

    float elements[] = {1.5f, -0.5f};
    int32_t counts[] = {2, 3};
    auto input = CreateVectorBatch(types, rowSize, elements, counts);

    auto expr = FuncExpr("array_repeat", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_FLOAT)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_INT))
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
    VerifyArrayElement<float>(arrayResult, 0, 1, 1.5f);

    VerifyArraySize(arrayResult, 1, 3);
    VerifyArrayElement<float>(arrayResult, 1, 0, -0.5f);
    VerifyArrayElement<float>(arrayResult, 1, 1, -0.5f);
    VerifyArrayElement<float>(arrayResult, 1, 2, -0.5f);

    delete context;
    delete input;
    delete result;
}

TEST_F(ArrayRepeatTest, BooleanRepeat)
{
    int rowSize = 2;
    auto elementType = std::make_shared<DataType>(OMNI_BOOLEAN);
    auto countType = std::make_shared<DataType>(OMNI_INT);
    auto types = DataTypes({elementType, countType});

    bool elements[] = {true, false};
    int32_t counts[] = {3, 2};
    auto input = CreateVectorBatch(types, rowSize, elements, counts);

    auto expr = FuncExpr("array_repeat", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_BOOLEAN)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_INT))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *arrayResult = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(arrayResult, nullptr);

    VerifyArraySize(arrayResult, 0, 3);
    VerifyArrayElement<bool>(arrayResult, 0, 0, true);
    VerifyArrayElement<bool>(arrayResult, 0, 1, true);
    VerifyArrayElement<bool>(arrayResult, 0, 2, true);

    VerifyArraySize(arrayResult, 1, 2);
    VerifyArrayElement<bool>(arrayResult, 1, 0, false);
    VerifyArrayElement<bool>(arrayResult, 1, 1, false);

    delete context;
    delete input;
    delete result;
}

TEST_F(ArrayRepeatTest, ByteRepeat)
{
    int rowSize = 2;
    auto elementType = std::make_shared<DataType>(OMNI_BYTE);
    auto countType = std::make_shared<DataType>(OMNI_INT);
    auto types = DataTypes({elementType, countType});

    int8_t elements[] = {42, -1};
    int32_t counts[] = {2, 1};
    auto input = CreateVectorBatch(types, rowSize, elements, counts);

    auto expr = FuncExpr("array_repeat", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_BYTE)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_INT))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *arrayResult = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(arrayResult, nullptr);

    VerifyArraySize(arrayResult, 0, 2);
    VerifyArrayElement<int8_t>(arrayResult, 0, 0, 42);
    VerifyArrayElement<int8_t>(arrayResult, 0, 1, 42);

    VerifyArraySize(arrayResult, 1, 1);
    VerifyArrayElement<int8_t>(arrayResult, 1, 0, -1);

    delete context;
    delete input;
    delete result;
}

TEST_F(ArrayRepeatTest, ShortRepeat)
{
    int rowSize = 2;
    auto elementType = std::make_shared<DataType>(OMNI_SHORT);
    auto countType = std::make_shared<DataType>(OMNI_INT);
    auto types = DataTypes({elementType, countType});

    int16_t elements[] = {1000, -500};
    int32_t counts[] = {2, 3};
    auto input = CreateVectorBatch(types, rowSize, elements, counts);

    auto expr = FuncExpr("array_repeat", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_SHORT)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_INT))
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
    VerifyArrayElement<int16_t>(arrayResult, 0, 1, 1000);

    VerifyArraySize(arrayResult, 1, 3);
    VerifyArrayElement<int16_t>(arrayResult, 1, 0, -500);
    VerifyArrayElement<int16_t>(arrayResult, 1, 1, -500);
    VerifyArrayElement<int16_t>(arrayResult, 1, 2, -500);

    delete context;
    delete input;
    delete result;
}

TEST_F(ArrayRepeatTest, NegativeCountTreatedAsZero)
{
    int rowSize = 3;
    auto elementType = std::make_shared<DataType>(OMNI_INT);
    auto countType = std::make_shared<DataType>(OMNI_INT);
    auto types = DataTypes({elementType, countType});

    int32_t elements[] = {10, 20, 30};
    int32_t counts[] = {-1, -5, -100};
    auto input = CreateVectorBatch(types, rowSize, elements, counts);

    auto expr = FuncExpr("array_repeat", {
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

    VerifyArraySize(arrayResult, 0, 0);
    VerifyArraySize(arrayResult, 1, 0);
    VerifyArraySize(arrayResult, 2, 0);

    delete context;
    delete input;
    delete result;
}

TEST_F(ArrayRepeatTest, NullCountReturnsNull)
{
    int rowSize = 2;
    auto elementType = std::make_shared<DataType>(OMNI_INT);
    auto countType = std::make_shared<DataType>(OMNI_INT);

    auto *elementVec = new Vector<int32_t>(rowSize);
    elementVec->SetValue(0, 10);
    elementVec->SetValue(1, 20);

    auto *countVecObj = new Vector<int32_t>(rowSize);
    countVecObj->SetNull(0);
    countVecObj->SetValue(1, 2);

    auto *input = new VectorBatch(rowSize);
    input->Append(elementVec);
    input->Append(countVecObj);

    auto expr = FuncExpr("array_repeat", {
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

    EXPECT_TRUE(arrayResult->IsNull(0));

    VerifyArraySize(arrayResult, 1, 2);
    VerifyArrayElement<int32_t>(arrayResult, 1, 0, 20);
    VerifyArrayElement<int32_t>(arrayResult, 1, 1, 20);

    delete context;
    delete input;
    delete result;
}

TEST_F(ArrayRepeatTest, NullElementReturnsArrayOfNulls)
{
    int rowSize = 2;

    auto *elementVec = new Vector<int32_t>(rowSize);
    elementVec->SetNull(0);
    elementVec->SetValue(1, 42);

    auto *countVecObj = new Vector<int32_t>(rowSize);
    countVecObj->SetValue(0, 3);
    countVecObj->SetValue(1, 2);

    auto *input = new VectorBatch(rowSize);
    input->Append(elementVec);
    input->Append(countVecObj);

    auto expr = FuncExpr("array_repeat", {
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

    // row 0: repeat(null, 3) -> [null, null, null]
    EXPECT_FALSE(arrayResult->IsNull(0));
    VerifyArraySize(arrayResult, 0, 3);
    VerifyArrayElementNull<int32_t>(arrayResult, 0, 0);
    VerifyArrayElementNull<int32_t>(arrayResult, 0, 1);
    VerifyArrayElementNull<int32_t>(arrayResult, 0, 2);

    // row 1: repeat(42, 2) -> [42, 42]
    VerifyArraySize(arrayResult, 1, 2);
    VerifyArrayElement<int32_t>(arrayResult, 1, 0, 42);
    VerifyArrayElement<int32_t>(arrayResult, 1, 1, 42);

    delete context;
    delete input;
    delete result;
}

TEST_F(ArrayRepeatTest, ZeroCountReturnsEmptyArray)
{
    int rowSize = 2;
    auto elementType = std::make_shared<DataType>(OMNI_INT);
    auto countType = std::make_shared<DataType>(OMNI_INT);
    auto types = DataTypes({elementType, countType});

    int32_t elements[] = {10, 20};
    int32_t counts[] = {0, 0};
    auto input = CreateVectorBatch(types, rowSize, elements, counts);

    auto expr = FuncExpr("array_repeat", {
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

    VerifyArraySize(arrayResult, 0, 0);
    VerifyArraySize(arrayResult, 1, 0);

    delete context;
    delete input;
    delete result;
}

TEST_F(ArrayRepeatTest, DoubleSpecialValues)
{
    int rowSize = 3;
    auto elementType = std::make_shared<DataType>(OMNI_DOUBLE);
    auto countType = std::make_shared<DataType>(OMNI_INT);
    auto types = DataTypes({elementType, countType});

    constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
    constexpr double kInf = std::numeric_limits<double>::infinity();
    constexpr double kNegInf = -std::numeric_limits<double>::infinity();

    double elements[] = {kNaN, kInf, kNegInf};
    int32_t counts[] = {2, 1, 2};
    auto input = CreateVectorBatch(types, rowSize, elements, counts);

    auto expr = FuncExpr("array_repeat", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_DOUBLE)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_INT))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *arrayResult = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(arrayResult, nullptr);

    // row 0: repeat(NaN, 2) -> [NaN, NaN]
    VerifyArraySize(arrayResult, 0, 2);
    auto elemVec = arrayResult->GetElementVector();
    auto *doubleVec = dynamic_cast<Vector<double> *>(elemVec.get());
    ASSERT_NE(doubleVec, nullptr);
    int64_t offset0 = arrayResult->GetOffset(0);
    EXPECT_TRUE(std::isnan(doubleVec->GetValue(static_cast<int32_t>(offset0))));
    EXPECT_TRUE(std::isnan(doubleVec->GetValue(static_cast<int32_t>(offset0 + 1))));

    // row 1: repeat(Inf, 1) -> [Inf]
    VerifyArraySize(arrayResult, 1, 1);
    VerifyArrayElement<double>(arrayResult, 1, 0, kInf);

    // row 2: repeat(-Inf, 2) -> [-Inf, -Inf]
    VerifyArraySize(arrayResult, 2, 2);
    VerifyArrayElement<double>(arrayResult, 2, 0, kNegInf);
    VerifyArrayElement<double>(arrayResult, 2, 1, kNegInf);

    delete context;
    delete input;
    delete result;
}

TEST_F(ArrayRepeatTest, VarcharRepeat)
{
    int rowSize = 2;
    auto elementType = std::make_shared<DataType>(OMNI_VARCHAR);
    auto countType = std::make_shared<DataType>(OMNI_INT);

    using VarcharVector = Vector<LargeStringContainer<std::string_view>>;

    auto *elementVec = new VarcharVector(rowSize);
    std::string_view val0("hello");
    std::string_view val1("world");
    elementVec->SetValue(0, val0);
    elementVec->SetValue(1, val1);

    auto *countVecObj = new Vector<int32_t>(rowSize);
    countVecObj->SetValue(0, 2);
    countVecObj->SetValue(1, 1);

    auto *input = new VectorBatch(rowSize);
    input->Append(elementVec);
    input->Append(countVecObj);

    auto expr = FuncExpr("array_repeat", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_VARCHAR)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_INT))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *arrayResult = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(arrayResult, nullptr);

    VerifyArraySize(arrayResult, 0, 2);
    auto elemResultVec = arrayResult->GetElementVector();
    auto *varcharResultVec = dynamic_cast<VarcharVector *>(elemResultVec.get());
    ASSERT_NE(varcharResultVec, nullptr);

    int64_t offset0 = arrayResult->GetOffset(0);
    EXPECT_EQ(varcharResultVec->GetValue(static_cast<int32_t>(offset0)), "hello");
    EXPECT_EQ(varcharResultVec->GetValue(static_cast<int32_t>(offset0 + 1)), "hello");

    VerifyArraySize(arrayResult, 1, 1);
    int64_t offset1 = arrayResult->GetOffset(1);
    EXPECT_EQ(varcharResultVec->GetValue(static_cast<int32_t>(offset1)), "world");

    delete context;
    delete input;
    delete result;
}

TEST_F(ArrayRepeatTest, IntegerBoundaryValues)
{
    int rowSize = 2;
    auto elementType = std::make_shared<DataType>(OMNI_INT);
    auto countType = std::make_shared<DataType>(OMNI_INT);
    auto types = DataTypes({elementType, countType});

    constexpr int32_t kMin = std::numeric_limits<int32_t>::min();
    constexpr int32_t kMax = std::numeric_limits<int32_t>::max();

    int32_t elements[] = {kMin, kMax};
    int32_t counts[] = {2, 1};
    auto input = CreateVectorBatch(types, rowSize, elements, counts);

    auto expr = FuncExpr("array_repeat", {
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

    VerifyArraySize(arrayResult, 0, 2);
    VerifyArrayElement<int32_t>(arrayResult, 0, 0, kMin);
    VerifyArrayElement<int32_t>(arrayResult, 0, 1, kMin);

    VerifyArraySize(arrayResult, 1, 1);
    VerifyArrayElement<int32_t>(arrayResult, 1, 0, kMax);

    delete context;
    delete input;
    delete result;
}

TEST_F(ArrayRepeatTest, LargeRepeatCount)
{
    int rowSize = 1;
    auto elementType = std::make_shared<DataType>(OMNI_INT);
    auto countType = std::make_shared<DataType>(OMNI_INT);
    auto types = DataTypes({elementType, countType});

    int32_t elements[] = {7};
    int32_t counts[] = {1000};
    auto input = CreateVectorBatch(types, rowSize, elements, counts);

    auto expr = FuncExpr("array_repeat", {
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

    VerifyArraySize(arrayResult, 0, 1000);
    VerifyArrayElement<int32_t>(arrayResult, 0, 0, 7);
    VerifyArrayElement<int32_t>(arrayResult, 0, 999, 7);

    delete context;
    delete input;
    delete result;
}

TEST_F(ArrayRepeatTest, MixedCountValues)
{
    int rowSize = 4;
    auto elementType = std::make_shared<DataType>(OMNI_INT);
    auto countType = std::make_shared<DataType>(OMNI_INT);
    auto types = DataTypes({elementType, countType});

    int32_t elements[] = {1, 2, 3, 4};
    int32_t counts[] = {2, 0, -3, 1};
    auto input = CreateVectorBatch(types, rowSize, elements, counts);

    auto expr = FuncExpr("array_repeat", {
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

    // repeat(1, 2) -> [1, 1]
    VerifyArraySize(arrayResult, 0, 2);
    VerifyArrayElement<int32_t>(arrayResult, 0, 0, 1);
    VerifyArrayElement<int32_t>(arrayResult, 0, 1, 1);

    // repeat(2, 0) -> []
    VerifyArraySize(arrayResult, 1, 0);

    // repeat(3, -3) -> [] (negative treated as 0)
    VerifyArraySize(arrayResult, 2, 0);

    // repeat(4, 1) -> [4]
    VerifyArraySize(arrayResult, 3, 1);
    VerifyArrayElement<int32_t>(arrayResult, 3, 0, 4);

    delete context;
    delete input;
    delete result;
}

TEST_F(ArrayRepeatTest, Decimal128Repeat)
{
    int rowSize = 1;

    auto *elementVec = new Vector<Decimal128>(rowSize);
    Decimal128 decVal(123456789012345678LL);
    elementVec->SetValue(0, decVal);

    auto *countVecObj = new Vector<int32_t>(rowSize);
    countVecObj->SetValue(0, 2);

    auto *input = new VectorBatch(rowSize);
    input->Append(elementVec);
    input->Append(countVecObj);

    auto expr = FuncExpr("array_repeat", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_DECIMAL128)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_INT))
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto *arrayResult = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(arrayResult, nullptr);

    VerifyArraySize(arrayResult, 0, 2);
    VerifyArrayElement<Decimal128>(arrayResult, 0, 0, decVal);
    VerifyArrayElement<Decimal128>(arrayResult, 0, 1, decVal);

    delete context;
    delete input;
    delete result;
}

TEST_F(ArrayRepeatTest, SingleRowSingleElement)
{
    int rowSize = 1;
    auto elementType = std::make_shared<DataType>(OMNI_INT);
    auto countType = std::make_shared<DataType>(OMNI_INT);
    auto types = DataTypes({elementType, countType});

    int32_t elements[] = {99};
    int32_t counts[] = {1};
    auto input = CreateVectorBatch(types, rowSize, elements, counts);

    auto expr = FuncExpr("array_repeat", {
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

    VerifyArraySize(arrayResult, 0, 1);
    VerifyArrayElement<int32_t>(arrayResult, 0, 0, 99);

    delete context;
    delete input;
    delete result;
}
