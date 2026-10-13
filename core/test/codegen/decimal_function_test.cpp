/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2024. All rights reserved.
 * Description: function test
 */
#include <string>
#include <vector>
#include "gtest/gtest.h"
#include "expression/expressions.h"
#include "codegen/functions/stringfunctions.h"
#include "codegen/functions/decimal_arithmetic_functions.h"
#include "codegen/functions/decimal_cast_functions.h"
#include "codegen/functions/dictionaryfunctions.h"
#include "codegen/functions/varcharVectorfunctions.h"
#include "vector/vector_helper.h"

namespace omniruntime {
using namespace omniruntime::op;
using namespace omniruntime::vec;
using namespace omniruntime::expressions;
using namespace omniruntime::codegen::function;

TEST(FunctionTest, GetDecimalFromDictionaryVector)
{
    ConfigUtil::SetRoundingRule(RoundingRule::HALF_UP);
    ConfigUtil::SetEmptySearchStrReplaceRule(EmptySearchStrReplaceRule::REPLACE);
    ConfigUtil::SetNegativeStartIndexOutOfBoundsRule(NegativeStartIndexOutOfBoundsRule::EMPTY_STRING);
    int size = 5;
    auto *decimalVec = new Vector<Decimal128>(size * 2);

    for (int i = 0; i < decimalVec->GetSize(); i++) {
        Decimal128 x(i * 11, i * 13);
        decimalVec->SetValue(i, x);
    }
    int32_t ids[] = { 0, 2, 4, 6, 8 };
    auto dictionary = VectorHelper::CreateDictionary(ids, size, decimalVec);
    int64_t outHigh = 0;
    uint64_t outLow = 0;
    int64_t dictptr =
        reinterpret_cast<int64_t>(reinterpret_cast<Vector<DictionaryContainer<Decimal128>> *>(dictionary));
    for (int i = 0; i < size; i++) {
        GetDecimalFromDictionaryVector(dictptr, i, 38, 0, &outHigh, &outLow);
        Decimal128 expected = decimalVec->GetValue(ids[i]);
        EXPECT_EQ(expected.HighBits(), outHigh);
        EXPECT_EQ(expected.LowBits(), outLow);
    }
    delete decimalVec;
    delete dictionary;
}

/*
 * Decimal Functions:
 */
TEST(FunctionTest, Decimal128Compare)
{
    Decimal128 op1;
    Decimal128 op2;

    op1 = 0;
    op2 = 0;
    EXPECT_EQ(0, Decimal128Compare(op1.HighBits(), op1.LowBits(), 38, 0, op2.HighBits(), op2.LowBits(), 38, 0, false));

    op1 = 1;
    op2 = 5;
    EXPECT_EQ(-1, Decimal128Compare(op1.HighBits(), op1.LowBits(), 38, 4, op2.HighBits(), op2.LowBits(), 38, 4, false));

    op1 = 6;
    op2 = -8;
    EXPECT_EQ(1,
        Decimal128Compare(op1.HighBits(), op1.LowBits(), 38, 17, op2.HighBits(), op2.LowBits(), 38, 17, false));
}

TEST(FunctionTest, AddDec128)
{
    ConfigUtil::SetCheckReScaleRule(CheckReScaleRule::CHECK_RESCALE);

    Decimal128 op1;
    Decimal128 op2;
    Decimal128 expected;
    int64_t zHigh = 0;
    uint64_t zLow = 0;
    auto context = new ExecutionContext(4096, nullptr);
    auto contextPtr = reinterpret_cast<int64_t>(context);

    op1 = Decimal128(0);
    op2 = Decimal128(0);
    expected = Decimal128(0);
    AddDec128Dec128Dec128ReScale(contextPtr, op1.HighBits(), op1.LowBits(), 38, 9, op2.HighBits(), op2.LowBits(), 38, 9,
        38, 9, &zHigh, &zLow);
    EXPECT_EQ(zHigh, expected.HighBits());
    EXPECT_EQ(zLow, expected.LowBits());

    op1 = Decimal128(5);
    op2 = Decimal128(10);
    expected = Decimal128(15);
    AddDec128Dec128Dec128ReScale(contextPtr, op1.HighBits(), op1.LowBits(), 38, 12, op2.HighBits(), op2.LowBits(), 38,
        12, 38, 12, &zHigh, &zLow);
    EXPECT_EQ(zHigh, expected.HighBits());
    EXPECT_EQ(zLow, expected.LowBits());

    op1 = Decimal128(-1);
    op2 = Decimal128(1);
    expected = Decimal128(0);
    AddDec128Dec128Dec128ReScale(contextPtr, op1.HighBits(), op1.LowBits(), 38, 0, op2.HighBits(), op2.LowBits(), 38, 0,
        38, 0, &zHigh, &zLow);
    EXPECT_EQ(zHigh, expected.HighBits());
    EXPECT_EQ(zLow, expected.LowBits());

    op1 = Decimal128(-3);
    op2 = Decimal128(-4);
    expected = Decimal128(-7);
    AddDec128Dec128Dec128ReScale(contextPtr, op1.HighBits(), op1.LowBits(), 38, 0, op2.HighBits(), op2.LowBits(), 38, 0,
        38, 0, &zHigh, &zLow);
    EXPECT_EQ(zHigh, expected.HighBits());
    EXPECT_EQ(zLow, expected.LowBits());

    delete context;
}

TEST(FunctionTest, SubDec128)
{
    ConfigUtil::SetCheckReScaleRule(CheckReScaleRule::CHECK_RESCALE);

    Decimal128 op1;
    Decimal128 op2;
    Decimal128 expected;
    int64_t zHigh = 0;
    uint64_t zLow = 0;

    auto context = new ExecutionContext(4096, nullptr);
    auto contextPtr = reinterpret_cast<int64_t>(context);

    op1 = Decimal128(0);
    op2 = Decimal128(0);
    expected = Decimal128(0);
    SubDec128Dec128Dec128ReScale(contextPtr, op1.HighBits(), op1.LowBits(), 38, 0, op2.HighBits(), op2.LowBits(), 38, 0,
        38, 0, &zHigh, &zLow);
    EXPECT_EQ(zHigh, expected.HighBits());
    EXPECT_EQ(zLow, expected.LowBits());

    op1 = Decimal128(10);
    op2 = Decimal128(5);
    expected = Decimal128(5);
    SubDec128Dec128Dec128ReScale(contextPtr, op1.HighBits(), op1.LowBits(), 38, 0, op2.HighBits(), op2.LowBits(), 38, 0,
        38, 0, &zHigh, &zLow);
    EXPECT_EQ(zHigh, expected.HighBits());
    EXPECT_EQ(zLow, expected.LowBits());

    op1 = Decimal128(5);
    op2 = Decimal128(10);
    expected = Decimal128(-5);
    SubDec128Dec128Dec128ReScale(contextPtr, op1.HighBits(), op1.LowBits(), 38, 0, op2.HighBits(), op2.LowBits(), 38, 0,
        38, 0, &zHigh, &zLow);
    EXPECT_EQ(zHigh, expected.HighBits());
    EXPECT_EQ(zLow, expected.LowBits());

    op1 = Decimal128(-3);
    op2 = Decimal128(-4);
    expected = Decimal128(1);
    SubDec128Dec128Dec128ReScale(contextPtr, op1.HighBits(), op1.LowBits(), 38, 0, op2.HighBits(), op2.LowBits(), 38, 0,
        38, 0, &zHigh, &zLow);
    EXPECT_EQ(zHigh, expected.HighBits());
    EXPECT_EQ(zLow, expected.LowBits());

    delete context;
}

TEST(FunctionTest, MulDec128)
{
    ConfigUtil::SetCheckReScaleRule(CheckReScaleRule::CHECK_RESCALE);

    Decimal128 op1;
    Decimal128 op2;
    Decimal128 expected;
    int64_t zHigh = 0;
    uint64_t zLow = 0;

    auto context = new ExecutionContext(4096, nullptr);
    auto contextPtr = reinterpret_cast<int64_t>(context);
    op1 = Decimal128(0);
    op2 = Decimal128(500);
    expected = Decimal128(0);
    MulDec128Dec128Dec128ReScale(contextPtr, op1.HighBits(), op1.LowBits(), 38, 7, op2.HighBits(), op2.LowBits(), 38, 7,
        38, 14, &zHigh, &zLow);
    EXPECT_EQ(zHigh, expected.HighBits());
    EXPECT_EQ(zLow, expected.LowBits());

    op1 = Decimal128(1);
    op2 = Decimal128(500);
    expected = Decimal128(500);
    MulDec128Dec128Dec128ReScale(contextPtr, op1.HighBits(), op1.LowBits(), 38, 1, op2.HighBits(), op2.LowBits(), 38, 1,
        38, 2, &zHigh, &zLow);
    EXPECT_EQ(zHigh, expected.HighBits());
    EXPECT_EQ(zLow, expected.LowBits());

    op1 = Decimal128(3);
    op2 = Decimal128(5);
    expected = Decimal128(15);
    MulDec128Dec128Dec128ReScale(contextPtr, op1.HighBits(), op1.LowBits(), 38, 0, op2.HighBits(), op2.LowBits(), 38, 0,
        38, 0, &zHigh, &zLow);
    EXPECT_EQ(zHigh, expected.HighBits());
    EXPECT_EQ(zLow, expected.LowBits());

    op1 = Decimal128(-3);
    op2 = Decimal128(-4);
    expected = Decimal128(12);
    MulDec128Dec128Dec128ReScale(contextPtr, op1.HighBits(), op1.LowBits(), 38, 0, op2.HighBits(), op2.LowBits(), 38, 0,
        38, 0, &zHigh, &zLow);
    EXPECT_EQ(zHigh, expected.HighBits());
    EXPECT_EQ(zLow, expected.LowBits());

    op1 = Decimal128(-3);
    op2 = Decimal128(4);
    expected = Decimal128(-12);
    MulDec128Dec128Dec128ReScale(contextPtr, op1.HighBits(), op1.LowBits(), 38, 3, op2.HighBits(), op2.LowBits(), 38, 3,
        38, 6, &zHigh, &zLow);
    EXPECT_EQ(zHigh, expected.HighBits());
    EXPECT_EQ(zLow, expected.LowBits());

    delete context;
}

TEST(FunctionTest, DivDec128)
{
    ConfigUtil::SetCheckReScaleRule(CheckReScaleRule::CHECK_RESCALE);

    Decimal128 op1;
    Decimal128 op2;
    Decimal128 expected;
    int64_t zHigh = 0;
    uint64_t zLow = 0;

    auto context = new ExecutionContext(4096, nullptr);
    int64_t contextptr = reinterpret_cast<int64_t>(context);
    // for simplicity using precision = 0 scale = 0
    int32_t precision = 18;
    int32_t scale = 0;

    op1 = Decimal128(10);
    op2 = Decimal128(2);
    expected = Decimal128(5);
    DivDec128Dec128Dec128ReScale(contextptr, op1.HighBits(), op1.LowBits(), precision, scale, op2.HighBits(),
        op2.LowBits(), precision, scale, precision, scale, &zHigh, &zLow);
    EXPECT_EQ(zHigh, expected.HighBits());
    EXPECT_EQ(zLow, expected.LowBits());

    op1 = Decimal128(-10);
    op2 = Decimal128(2);
    expected = Decimal128(-5);
    DivDec128Dec128Dec128ReScale(contextptr, op1.HighBits(), op1.LowBits(), precision, scale, op2.HighBits(),
        op2.LowBits(), precision, scale, precision, scale, &zHigh, &zLow);
    EXPECT_EQ(zHigh, expected.HighBits());
    EXPECT_EQ(zLow, expected.LowBits());

    op1 = Decimal128(-10);
    op2 = Decimal128(-2);
    expected = Decimal128(5);
    DivDec128Dec128Dec128ReScale(contextptr, op1.HighBits(), op1.LowBits(), precision, scale, op2.HighBits(),
        op2.LowBits(), precision, scale, precision, scale, &zHigh, &zLow);
    EXPECT_EQ(zHigh, expected.HighBits());
    EXPECT_EQ(zLow, expected.LowBits());

    op1 = Decimal128(7);
    op2 = Decimal128(3);
    expected = Decimal128(2);
    DivDec128Dec128Dec128ReScale(contextptr, op1.HighBits(), op1.LowBits(), precision, scale, op2.HighBits(),
        op2.LowBits(), precision, scale, precision, scale, &zHigh, &zLow);
    EXPECT_EQ(zHigh, expected.HighBits());
    EXPECT_EQ(zLow, expected.LowBits());

    op1 = Decimal128(8);
    op2 = Decimal128(3);
    expected = Decimal128(3);
    DivDec128Dec128Dec128ReScale(contextptr, op1.HighBits(), op1.LowBits(), precision, scale, op2.HighBits(),
        op2.LowBits(), precision, scale, precision, scale, &zHigh, &zLow);
    EXPECT_EQ(zHigh, expected.HighBits());
    EXPECT_EQ(zLow, expected.LowBits());

    delete context;
}

TEST(FunctionTest, AbsDecimal128)
{
    int64_t outHigh = 0;
    uint64_t outLow = 0;

    Decimal128 test;
    Decimal128 expected;

    test = Decimal128(3);
    AbsDecimal128(test.HighBits(), test.LowBits(), 38, 9, false, 38, 9, &outHigh, &outLow);
    EXPECT_EQ(outHigh, test.HighBits());
    EXPECT_EQ(outLow, test.LowBits());

    test = Decimal128(-1);
    expected = Decimal128(1);
    AbsDecimal128(test.HighBits(), test.LowBits(), 38, 0, false, 38, 0, &outHigh, &outLow);
    EXPECT_EQ(outHigh, expected.HighBits());
    EXPECT_EQ(outLow, expected.LowBits());

    test = Decimal128(0);
    expected = Decimal128(0);
    AbsDecimal128(test.HighBits(), test.LowBits(), 38, 0, false, 38, 0, &outHigh, &outLow);
    EXPECT_EQ(outHigh, expected.HighBits());
    EXPECT_EQ(outLow, expected.LowBits());
}

TEST(FunctionTest, RoundDecimal)
{
    int64_t outHigh = 0;
    uint64_t outLow = 0;

    auto context = new ExecutionContext(4096, nullptr);
    auto contextPtr = reinterpret_cast<int64_t>(context);
    Decimal128 test;
    Decimal128 expected;
    test = Decimal128("22167875302138684366688952930001319804");
    expected = Decimal128("22167875302138684367");
    RoundDecimal128(contextPtr, test.HighBits(), test.LowBits(), 38, 18, 0, false, 21, 0, &outHigh, &outLow);
    EXPECT_EQ(outHigh, expected.HighBits());
    EXPECT_EQ(outLow, expected.LowBits());

    test = Decimal128("151949737170315874258868924986217725952");
    expected = Decimal128(0);
    RoundDecimal128(contextPtr, test.HighBits(), test.LowBits(), 38, 18, 0, true, 21, 0, &outHigh, &outLow);
    EXPECT_EQ(outHigh, expected.HighBits());
    EXPECT_EQ(outLow, expected.LowBits());

    test = Decimal128("76051882560490807662482874031011839644");
    expected = Decimal128("76051882560490807662");
    RoundDecimal128WithoutRound(contextPtr, test.HighBits(), test.LowBits(), 38, 18, false, 21, 0, &outHigh, &outLow);
    EXPECT_EQ(outHigh, expected.HighBits());
    EXPECT_EQ(outLow, expected.LowBits());

    test = Decimal128("151949737170315874258868924986217725952");
    expected = Decimal128(0);
    RoundDecimal128WithoutRound(contextPtr, test.HighBits(), test.LowBits(), 38, 18, true, 21, 0, &outHigh, &outLow);
    EXPECT_EQ(outHigh, expected.HighBits());
    EXPECT_EQ(outLow, expected.LowBits());

    int64_t result = RoundDecimal64(contextPtr, 463127424592162661L, 18, 8, 0, false, 11, 0);
    EXPECT_EQ(result, 4631274246L);
    result = RoundDecimal64(contextPtr, 63636006298474304L, 18, 8, 0, true, 11, 0);
    EXPECT_EQ(result, 0);
    result = RoundDecimal64WithoutRound(contextPtr, 920526845191220634L, 18, 8, false, 11, 0);
    EXPECT_EQ(result, 9205268452L);
    result = RoundDecimal64WithoutRound(contextPtr, 63636006298474304L, 18, 8, true, 11, 0);
    EXPECT_EQ(result, 0);
    delete context;
}

TEST(FunctionTest, CastDecimal64To64)
{
    auto context = new ExecutionContext(4096, nullptr);
    auto contextPtr = reinterpret_cast<int64_t>(context);
    int64_t input = 12345;
    int64_t result = CastDecimal64To64(contextPtr, input, 18, 0, false, 18, 3);
    EXPECT_EQ(result, 12345000);
    input = -19501040780019L;
    result = CastDecimal64To64(contextPtr, input, 17, 2, false, 18, 6);
    EXPECT_EQ(result, -195010407800190000L);
    input = 12395;
    result = CastDecimal64To64(contextPtr, input, 18, 2, false, 18, 0);
    EXPECT_EQ(result, 124);
    input = 100;
    result = CastDecimal64To64(contextPtr, input, 7, 2, false, 7, 0);
    EXPECT_EQ(result, 1);
    input = 12395;
    result = CastDecimal64To64(contextPtr, input, 18, 2, false, 5, 4);
    std::string message = context->GetError();
    EXPECT_EQ(message, "Cannot cast DECIMAL(18, 2) '123.95' to DECIMAL(5, 4)");
    delete context;
}

TEST(FunctionTest, CastDecimal128To128)
{
    int64_t high = 0;
    uint64_t low = 0;
    auto context = new ExecutionContext(4096, nullptr);
    auto contextPtr = reinterpret_cast<int64_t>(context);
    CastDecimal128To128(contextPtr, 0, 123, 38, 0, false, 20, 0, &high, &low);
    EXPECT_EQ(high, 0);
    EXPECT_EQ(low, 123);
    CastDecimal128To128(contextPtr, 0, 129, 38, 2, false, 20, 1, &high, &low);
    EXPECT_EQ(high, 0);
    EXPECT_EQ(low, 13);
    CastDecimal128To128(contextPtr, 0, 129234454, 38, 0, false, 38, 30, &high, &low);
    std::string message = context->GetError();
    EXPECT_EQ(message, "Cannot cast DECIMAL(38, 0) '129234454' to DECIMAL(38, 30)");
    CastDecimal128To128(contextPtr, 0, 123, 38, 0, true, 20, 0, &high, &low);
    EXPECT_EQ(high, 0);
    EXPECT_EQ(low, 0);
    delete context;
}

TEST(FunctionTest, CastDecimal64To128)
{
    int64_t high = 0;
    uint64_t low = 0;
    auto context = new ExecutionContext(4096, nullptr);
    auto contextPtr = reinterpret_cast<int64_t>(context);
    CastDecimal64To128(contextPtr, 123123, 17, 3, false, 38, 4, &high, &low);
    EXPECT_EQ(high, 0);
    EXPECT_EQ(low, 1231230);
    CastDecimal64To128(contextPtr, -123123, 17, 3, false, 38, 4, &high, &low);
    EXPECT_EQ(high, ~0);
    EXPECT_EQ(low, -1231230);
    CastDecimal64To128(contextPtr, 123125, 17, 3, false, 38, 2, &high, &low);
    EXPECT_EQ(high, 0);
    EXPECT_EQ(low, 12313);
    CastDecimal64To128(contextPtr, 123123, 17, 3, false, 38, 37, &high, &low);
    std::string message = context->GetError();
    EXPECT_EQ(message, "Cannot cast DECIMAL(17, 3) '123.123' to DECIMAL(38, 37)");
    CastDecimal64To128(contextPtr, 123125, 17, 3, true, 38, 2, &high, &low);
    EXPECT_EQ(high, 0);
    EXPECT_EQ(low, 0);
    delete context;
}

TEST(FunctionTest, CastDecimal128To64)
{
    auto context = new ExecutionContext(4096, nullptr);
    auto contextPtr = reinterpret_cast<int64_t>(context);
    int64_t result = CastDecimal128To64(contextPtr, 0, 123, 38, 1, false, 18, 3);
    EXPECT_EQ(result, 12300);
    result = CastDecimal128To64(contextPtr, 1, 123, 38, 1, false, 18, 3);
    EXPECT_EQ(result, 0);
    result = CastDecimal128To64(contextPtr, 0, 123, 38, 1, false, 4, 3);
    EXPECT_EQ(result, 0);
    result = CastDecimal128To64(contextPtr, ~0, -123, 38, 1, false, 18, 3);
    EXPECT_EQ(result, -12300);
    result = CastDecimal128To64(contextPtr, 0, 12366, 38, 2, false, 18, 1);
    EXPECT_EQ(result, 1237);
    Decimal128Wrapper x("12345120000000000000000");
    result = CastDecimal128To64(contextPtr, x.HighBits(), x.LowBits(), 38, 18, false, 7, 2);
    EXPECT_EQ(result, 1234512);
    delete context;
}

TEST(FunctionTest, CastInt8ToDecimal64)
{
    auto context = new ExecutionContext(4096, nullptr);
    auto contextPtr = reinterpret_cast<int64_t>(context);
    int8_t s = 111;
    int64_t result = CastInt8ToDecimal64(contextPtr, s, false, 17, 0);
    EXPECT_EQ(result, 111);
    s = -12;
    result = CastInt8ToDecimal64(contextPtr, s, false, 17, 3);
    EXPECT_EQ(result, -12000);
    s = 0;
    result = CastInt8ToDecimal64(contextPtr, s, false, 17, 0);
    EXPECT_EQ(result, 0);
    s = 127;
    result = CastInt8ToDecimal64(contextPtr, s, false, 18, 0);
    EXPECT_EQ(result, 127);
    s = 127;
    result = CastInt8ToDecimal64(contextPtr, s, false, 5, 4);
    EXPECT_EQ(result, 0);
    s = 123;
    result = CastInt8ToDecimal64(contextPtr, s, false, 18, 19);
    EXPECT_EQ(result, 0);
    delete context;
}

TEST(FunctionTest, CastInt16ToDecimal64)
{
    auto context = new ExecutionContext(4096, nullptr);
    auto contextPtr = reinterpret_cast<int64_t>(context);
    int16_t s = 9123;
    int64_t result = CastInt16ToDecimal64(contextPtr, s, false, 17, 0);
    EXPECT_EQ(result, 9123);
    s = -4559;
    result = CastInt16ToDecimal64(contextPtr, s, false, 17, 3);
    EXPECT_EQ(result, -4559000);
    s = 0;
    result = CastInt16ToDecimal64(contextPtr, s, false, 17, 0);
    EXPECT_EQ(result, 0);
    s = 32767;
    result = CastInt16ToDecimal64(contextPtr, s, false, 18, 0);
    EXPECT_EQ(result, 32767);
    s = 32767;
    result = CastInt16ToDecimal64(contextPtr, s, false, 5, 4);
    EXPECT_EQ(result, 0);
    s = 123;
    result = CastInt16ToDecimal64(contextPtr, s, false, 18, 19);
    EXPECT_EQ(result, 0);
    delete context;
}

TEST(FunctionTest, CastIntToDecimal64)
{
    auto context = new ExecutionContext(4096, nullptr);
    auto contextPtr = reinterpret_cast<int64_t>(context);
    int32_t s = 9123;
    int64_t result = CastIntToDecimal64(contextPtr, s, false, 17, 0);
    EXPECT_EQ(result, 9123);
    s = -45594;
    result = CastIntToDecimal64(contextPtr, s, false, 17, 3);
    EXPECT_EQ(result, -45594000);
    s = 0;
    result = CastIntToDecimal64(contextPtr, s, false, 17, 0);
    EXPECT_EQ(result, 0);
    s = 21'4748'3647;
    result = CastIntToDecimal64(contextPtr, s, false, 18, 0);
    EXPECT_EQ(result, 21'4748'3647);
    s = 21'4748'3647;
    result = CastIntToDecimal64(contextPtr, s, false, 5, 4);
    EXPECT_EQ(result, 0);
    s = 123;
    result = CastIntToDecimal64(contextPtr, s, false, 18, 19);
    EXPECT_EQ(result, 0);
    delete context;
}

TEST(FunctionTest, CastLongToDecimal64)
{
    auto context = new ExecutionContext(4096, nullptr);
    auto contextPtr = reinterpret_cast<int64_t>(context);
    int64_t s = 9123;
    int64_t result = CastLongToDecimal64(contextPtr, s, false, 17, 0);
    EXPECT_EQ(result, 9123);
    s = -45594;
    result = CastLongToDecimal64(contextPtr, s, false, 17, 3);
    EXPECT_EQ(result, -45594000);
    s = 0;
    result = CastLongToDecimal64(contextPtr, s, false, 17, 0);
    EXPECT_EQ(result, 0);
    s = 123;
    result = CastLongToDecimal64(contextPtr, s, false, 18, 3);
    EXPECT_EQ(result, 123000);
    s = 922'3372'0368'5477'5807;
    result = CastLongToDecimal64(contextPtr, s, false, 18, 3);
    EXPECT_EQ(result, 0);
    s = 9223372036854775807;
    result = CastLongToDecimal64(contextPtr, s, false, 18, 0);
    EXPECT_EQ(result, 0);
    s = INT64_MIN;
    result = CastLongToDecimal64(contextPtr, s, false, 18, 0);
    EXPECT_EQ(result, 0);
    delete context;
}

TEST(FunctionTest, CastDoubleToDecimal64)
{
    auto context = new ExecutionContext(4096, nullptr);
    auto contextPtr = reinterpret_cast<int64_t>(context);
    double s = 9123;
    int64_t result = CastDoubleToDecimal64(contextPtr, s, false, 18, 0);
    EXPECT_EQ(result, 9123);
    s = -9123.973;
    result = CastDoubleToDecimal64(contextPtr, s, false, 18, 3);
    EXPECT_EQ(result, -9123973);
    s = 0;
    result = CastDoubleToDecimal64(contextPtr, s, false, 18, 3);
    EXPECT_EQ(result, 0);
    s = 1.11E100;
    result = CastDoubleToDecimal64(contextPtr, s, false, 18, 3);
    EXPECT_EQ(result, 0);
    s = -45.7;
    result = CastDoubleToDecimal64(contextPtr, s, false, 18, 0);
    EXPECT_EQ(result, -46);
    s = 1.17549E-38;
    result = CastDoubleToDecimal64(contextPtr, s, false, 18, 0);
    EXPECT_EQ(result, 0.00);
    result = CastDoubleToDecimal64(contextPtr, 0, false, 17, 16);
    EXPECT_EQ(result, 0);
    delete context;
}

TEST(FunctionTest, CastInt8ToDecimal128)
{
    int64_t high = 0;
    uint64_t low = 0;

    auto context = new ExecutionContext(4096, nullptr);
    auto contextPtr = reinterpret_cast<int64_t>(context);
    int8_t s = 123;
    CastInt8ToDecimal128(contextPtr, s, false, 38, 0, &high, &low);
    EXPECT_EQ(low, 123);
    s = -1;
    CastInt8ToDecimal128(contextPtr, s, false, 38, 2, &high, &low);
    EXPECT_EQ(low, -100);
    s = 0;
    CastInt8ToDecimal128(contextPtr, s, false, 38, 0, &high, &low);
    EXPECT_EQ(low, 0);
    s = 127;
    CastInt8ToDecimal128(contextPtr, s, false, 38, 0, &high, &low);
    EXPECT_EQ(low, 127);
    delete context;
}

TEST(FunctionTest, CastInt16ToDecimal128)
{
    int64_t high = 0;
    uint64_t low = 0;

    auto context = new ExecutionContext(4096, nullptr);
    auto contextPtr = reinterpret_cast<int64_t>(context);
    int16_t s = 9123;
    CastInt16ToDecimal128(contextPtr, s, false, 38, 0, &high, &low);
    EXPECT_EQ(low, 9123);
    s = -4;
    CastInt16ToDecimal128(contextPtr, s, false, 38, 3, &high, &low);
    EXPECT_EQ(low, -4000);
    s = 0;
    CastInt16ToDecimal128(contextPtr, s, false, 38, 0, &high, &low);
    EXPECT_EQ(low, 0);
    s = 32767 ;
    CastInt16ToDecimal128(contextPtr, s, false, 38, 0, &high, &low);
    EXPECT_EQ(low, 32767);
    delete context;
}

TEST(FunctionTest, CastIntToDecimal128)
{
    int64_t high = 0;
    uint64_t low = 0;

    auto context = new ExecutionContext(4096, nullptr);
    auto contextPtr = reinterpret_cast<int64_t>(context);
    int32_t s = 9123;
    CastIntToDecimal128(contextPtr, s, false, 38, 0, &high, &low);
    EXPECT_EQ(low, 9123);
    s = -45594;
    CastIntToDecimal128(contextPtr, s, false, 38, 3, &high, &low);
    EXPECT_EQ(low, -4559'4000);
    s = 0;
    CastIntToDecimal128(contextPtr, s, false, 38, 0, &high, &low);
    EXPECT_EQ(low, 0);
    s = 21'4748'3647;
    CastIntToDecimal128(contextPtr, s, false, 38, 0, &high, &low);
    EXPECT_EQ(low, 21'4748'3647);
    delete context;
}

TEST(FunctionTest, CastLongToDecimal128)
{
    int64_t outHigh = 0;
    uint64_t outLow = 0;
    Decimal128 expected;

    auto context = new ExecutionContext(4096, nullptr);
    auto contextPtr = reinterpret_cast<int64_t>(context);

    int64_t x = 999;
    CastLongToDecimal128(contextPtr, x, false, 2, 0, &outHigh, &outLow);
    EXPECT_TRUE(context->HasError());
    x = 10;
    CastLongToDecimal128(contextPtr, x, false, 2, 0, &outHigh, &outLow);
    EXPECT_EQ(outLow, 10);

    delete context;
}

TEST(FunctionTest, CastDoubleToDecimal128)
{
    int64_t high = 0;
    uint64_t low = 0;

    auto context = new ExecutionContext(4096, nullptr);
    auto contextPtr = reinterpret_cast<int64_t>(context);
    double s = 9123;
    CastDoubleToDecimal128(contextPtr, s, false, 18, 0, &high, &low);
    EXPECT_EQ(low, 9123);
    s = -9123.973;
    CastDoubleToDecimal128(contextPtr, s, false, 18, 3, &high, &low);
    EXPECT_EQ(low, -9123973);
    s = 0;
    CastDoubleToDecimal128(contextPtr, s, false, 18, 0, &high, &low);
    EXPECT_EQ(low, 0);
    s = 123.1119;
    CastDoubleToDecimal128(contextPtr, s, false, 18, 3, &high, &low);
    EXPECT_EQ(low, 123112);
    s = 1.11E100;
    CastDoubleToDecimal128(contextPtr, s, false, 18, 0, &high, &low);
    EXPECT_EQ(context->GetError(), "Cannot cast DOUBLE '1.11e+100' to DECIMAL(18, 0). Value too large.");
    EXPECT_TRUE(context->HasError());
    delete context;
}

TEST(FunctionTest, CastDecimal64ToInt16)
{
    auto context = new ExecutionContext(4096, nullptr);
    auto contextPtr = reinterpret_cast<int64_t>(context);
    int16_t result = CastDecimal64ToInt16Down(contextPtr, 100, 38, 0, false);
    EXPECT_EQ(result, 100);
    result = CastDecimal64ToInt16HalfUp(contextPtr, 99, 38, 1, false);
    EXPECT_EQ(result, 10);
    result = CastDecimal64ToInt16Down(contextPtr, 100, 38, 0, false);
    EXPECT_EQ(result, 100);
    result = CastDecimal64ToInt16HalfUp(contextPtr, 8888, 38, 2, false);
    EXPECT_EQ(result, 89);
    delete context;
}

TEST(FunctionTest, CastDecimal64ToInt8)
{
    auto context = new ExecutionContext(4096, nullptr);
    auto contextPtr = reinterpret_cast<int64_t>(context);
    int8_t result = CastDecimal64ToInt8Down(contextPtr, 100, 38, 0, false);
    EXPECT_EQ(result, 100);
    result = CastDecimal64ToInt8HalfUp(contextPtr, 99, 38, 1, false);
    EXPECT_EQ(result, 10);
    result = CastDecimal64ToInt8Down(contextPtr, 100, 38, 0, false);
    EXPECT_EQ(result, 100);
    result = CastDecimal64ToInt8HalfUp(contextPtr, 8888, 38, 2, false);
    EXPECT_EQ(result, 89);
    delete context;
}

TEST(FunctionTest, CastDecimal128ToInt16)
{
    auto context = new ExecutionContext(4096, nullptr);
    auto contextPtr = reinterpret_cast<int64_t>(context);
    int16_t result = CastDecimal128ToInt16HalfUp(contextPtr, 0, 100, 38, 0, false);
    EXPECT_EQ(result, 100);
    result = CastDecimal128ToInt16HalfUp(contextPtr, 0, 99, 38, 1, false);
    EXPECT_EQ(result, 10);
    result = CastDecimal128ToInt16HalfUp(contextPtr, 1, 100, 38, 0, false);
    EXPECT_EQ(result, 0);
    result = CastDecimal128ToInt16HalfUp(contextPtr, 0, 8888, 38, 2, false);
    EXPECT_EQ(result, 89);
    delete context;
}

TEST(FunctionTest, CastDecimal128ToInt8)
{
    auto context = new ExecutionContext(4096, nullptr);
    auto contextPtr = reinterpret_cast<int64_t>(context);
    int8_t result = CastDecimal128ToInt8HalfUp(contextPtr, 0, 100, 38, 0, false);
    EXPECT_EQ(result, 100);
    result = CastDecimal128ToInt8HalfUp(contextPtr, 0, 99, 38, 1, false);
    EXPECT_EQ(result, 10);
    result = CastDecimal128ToInt8HalfUp(contextPtr, 1, 100, 38, 0, false);
    EXPECT_EQ(result, 0);
    result = CastDecimal128ToInt8HalfUp(contextPtr, 0, 8888, 38, 2, false);
    EXPECT_EQ(result, 89);
    delete context;
}

TEST(FunctionTest, CastDecimal64ToInt)
{
    auto context = new ExecutionContext(4096, nullptr);
    auto contextPtr = reinterpret_cast<int64_t>(context);
    int32_t result = CastDecimal64ToIntDown(contextPtr, 100, 38, 0, false);
    EXPECT_EQ(result, 100);
    result = CastDecimal64ToIntHalfUp(contextPtr, 99, 38, 1, false);
    EXPECT_EQ(result, 10);
    result = CastDecimal64ToIntDown(contextPtr, 100, 38, 0, false);
    EXPECT_EQ(result, 100);
    result = CastDecimal64ToIntHalfUp(contextPtr, 8888, 38, 2, false);
    EXPECT_EQ(result, 89);
    result = CastDecimal64ToIntDown(contextPtr, -1736879480, 15, 0, false);
    EXPECT_EQ(result, -1736879480);
    result = CastDecimal64ToIntHalfUp(contextPtr, -19501040780034LL, 17, 2, false);
    EXPECT_EQ(context->GetError(), "Cannot cast DECIMAL(17, 2) '-195010407800.34' to INTEGER");
    delete context;
}

TEST(FunctionTest, CastDecimal64ToLong)
{
    int64_t result = CastDecimal64ToLongHalfUp(100, 38, 0, false);
    EXPECT_EQ(result, 100);
    result = CastDecimal64ToLongHalfUp(99, 38, 1, false);
    EXPECT_EQ(result, 10);
    result = CastDecimal64ToLongHalfUp(100, 38, 0, false);
    EXPECT_EQ(result, 100);
    result = CastDecimal64ToLongHalfUp(-8888, 38, 2, false);
    EXPECT_EQ(result, -89);
    result = CastDecimal64ToLongHalfUp(INT64_MIN, 22, 0, false);
    EXPECT_EQ(result, INT64_MIN);
}

TEST(FunctionTest, CastDecimal64ToDouble)
{
    double result = CastDecimal64ToDoubleHalfUp(100, 38, 0, false);
    EXPECT_EQ(result, 100);
    result = CastDecimal64ToDoubleHalfUp(99, 38, 1, false);
    EXPECT_EQ(result, 9.9);
    result = CastDecimal64ToDoubleHalfUp(100, 38, 0, false);
    EXPECT_EQ(result, 100);
    result = CastDecimal64ToDoubleHalfUp(-8888, 38, 2, false);
    EXPECT_EQ(result, -88.88);
}

TEST(FunctionTest, CastDecimal128ToInt)
{
    auto context = new ExecutionContext(4096, nullptr);
    auto contextPtr = reinterpret_cast<int64_t>(context);
    int32_t result = CastDecimal128ToIntHalfUp(contextPtr, 0, 100, 38, 0, false);
    EXPECT_EQ(result, 100);
    result = CastDecimal128ToIntHalfUp(contextPtr, 0, 99, 38, 1, false);
    EXPECT_EQ(result, 10);
    result = CastDecimal128ToIntHalfUp(contextPtr, 1, 100, 38, 0, false);
    EXPECT_EQ(result, 0);
    result = CastDecimal128ToIntHalfUp(contextPtr, 0, 8888, 38, 2, false);
    EXPECT_EQ(result, 89);
    delete context;
}

TEST(FunctionTest, CastDecimal128ToLong)
{
    auto context = new ExecutionContext(4096, nullptr);
    auto contextPtr = reinterpret_cast<int64_t>(context);
    int64_t result = CastDecimal128ToLongDown(contextPtr, 0, 100, 38, 0, false);
    EXPECT_EQ(result, 100);
    result = CastDecimal128ToLongHalfUp(contextPtr, 0, 99, 38, 0, false);
    EXPECT_EQ(result, 99);
    result = CastDecimal128ToLongHalfUp(contextPtr, 1, 100, 38, 0, false);
    EXPECT_EQ(result, 0);
    result = CastDecimal128ToLongHalfUp(contextPtr, 0, 8888, 38, 2, false);
    EXPECT_EQ(result, 89);
    result = CastDecimal128ToLongHalfUp(contextPtr, 1, 1, 38, 20, false);
    EXPECT_EQ(result, 0);
    result = CastDecimal128ToLongHalfUp(contextPtr, ~0, -9223372036854775808UL, 22, 0, false);
    EXPECT_EQ(result, INT64_MIN);
    delete context;
}

TEST(FunctionTest, CastDecimal128ToDouble)
{
    double result = CastDecimal128ToDoubleHalfUp(0, 100, 38, 0, false);
    EXPECT_EQ(result, 100);
    result = CastDecimal128ToDoubleHalfUp(0, 534, 38, 3, false);
    EXPECT_EQ(result, 0.534);
    result = CastDecimal128ToDoubleHalfUp(0, 123, 38, 5, false);
    EXPECT_EQ(result, 0.00123);
    result = CastDecimal128ToDoubleHalfUp(0, 1234, 38, 2, false);
    EXPECT_EQ(result, 12.34);
    result = CastDecimal128ToDoubleHalfUp(0, 1234, 38, 2, false);
    EXPECT_EQ(result, 12.34);
}

TEST(FunctionTest, CastStringToDecimal64)
{
    auto context = new ExecutionContext(4096, nullptr);
    auto contextPtr = reinterpret_cast<int64_t>(context);
    std::string s = "23423";
    int64_t result = CastStringToDecimal64(contextPtr, s.c_str(), static_cast<int32_t>(s.size()), false, 17, 0);
    EXPECT_EQ(result, 23423);
    s = "9223372036854775807";
    result = CastStringToDecimal64(contextPtr, s.c_str(), static_cast<int32_t>(s.size()), false, 17, 2);
    EXPECT_EQ(result, 0);
    s = "-10078";
    result = CastStringToDecimal64(contextPtr, s.c_str(), static_cast<int32_t>(s.size()), false, 17, 3);
    EXPECT_EQ(result, -10078000);
    s = "123.129";
    result = CastStringToDecimal64(contextPtr, s.c_str(), static_cast<int32_t>(s.size()), false, 17, 2);
    EXPECT_EQ(result, 12313);
    s = "-10.11";
    result = CastStringToDecimal64(contextPtr, s.c_str(), static_cast<int32_t>(s.size()), false, 16, 2);
    EXPECT_EQ(result, -1011);
    s = "123";
    result = CastStringToDecimal64(contextPtr, s.c_str(), static_cast<int32_t>(s.size()), false, 5, 4);
    EXPECT_EQ(result, 0);
    s = "999999999999999999";
    result = CastStringToDecimal64(contextPtr, s.c_str(), static_cast<int32_t>(s.size()), false, 18, 0);
    EXPECT_EQ(result, 99'9999'9999'9999'9999);
    s = "9999999999999999999";
    result = CastStringToDecimal64(contextPtr, s.c_str(), static_cast<int32_t>(s.size()), false, 18, 0);
    EXPECT_TRUE(context->HasError());
    context->ResetError();
    s = "123a";
    result = CastStringToDecimal64(contextPtr, s.c_str(), static_cast<int32_t>(s.size()), false, 18, 0);
    EXPECT_TRUE(context->HasError());
    context->ResetError();
    delete context;
}

TEST(FunctionTest, CastStringToDecimal128)
{
    int64_t high = 0;
    uint64_t low = 0;
    auto context = new ExecutionContext(4096, nullptr);
    auto contextPtr = reinterpret_cast<int64_t>(context);
    bool *isNull = new bool(false);
    std::string s = "23423";
    CastStringToDecimal128(contextPtr, s.c_str(), static_cast<int32_t>(s.size()), false, 17, 0, &high, &low);
    EXPECT_EQ(low, 23423);
    EXPECT_EQ(high, 0);
    s = "-36893488147419103230";
    CastStringToDecimal128(contextPtr, s.c_str(), static_cast<int32_t>(s.size()), false, 37, 0, &high, &low);
    EXPECT_EQ(low, -1844'6744'0737'0955'1614UL);
    EXPECT_EQ(high, ~1);
    s = "-10078";
    CastStringToDecimal128(contextPtr, s.c_str(), static_cast<int32_t>(s.size()), false, 17, 0, &high, &low);
    EXPECT_EQ(low, -10078);
    EXPECT_EQ(high, ~0);
    s = "123.999";
    CastStringToDecimal128(contextPtr, s.c_str(), static_cast<int32_t>(s.size()), false, 17, 0, &high, &low);
    EXPECT_EQ(low, 124);
    EXPECT_EQ(high, 0);
    s = "-10.11";
    CastStringToDecimal128(contextPtr, s.c_str(), static_cast<int32_t>(s.size()), false, 17, 0, &high, &low);
    EXPECT_EQ(low, -10);
    EXPECT_EQ(high, ~0);
    s = "-10.1a1";
    CastStringToDecimal128(contextPtr, s.c_str(), static_cast<int32_t>(s.size()), false, 21, 1, &high, &low);
    EXPECT_TRUE(isNull);
    EXPECT_EQ(context->GetError(), "Cannot cast VARCHAR '-10.1a1' to DECIMAL(21, 1). Value is not a number.");
    s = "9999999999999999999999999999999999999999";
    context->ResetError();
    CastStringToDecimal128(contextPtr, s.c_str(), static_cast<int32_t>(s.size()), false, 21, 1, &high, &low);
    EXPECT_TRUE(isNull);
    EXPECT_EQ(context->GetError(),
        "Cannot cast VARCHAR '9999999999999999999999999999999999999999' to DECIMAL(21, 1). Value too large.");
    context->ResetError();
    CastStringToDecimal128RetNull(isNull, s.c_str(), static_cast<int32_t>(s.size()), 17, 0, &high, &low);
    EXPECT_TRUE(isNull);
    delete context;
    delete isNull;
}

TEST(FunctionTest, CastStringToDecimal128RoundUp)
{
    int64_t high = 0;
    uint64_t low = 0;
    auto context = new ExecutionContext(4096, nullptr);
    bool *isNull = new bool(false);
    std::string s = "312423423423542352333243423423.123412342";
    CastStringToDecimal128RoundUpRetNull(isNull, s.c_str(), static_cast<int32_t>(s.size()), 38, 8, &high, &low);
    EXPECT_FALSE(*isNull);
    EXPECT_EQ(ToString((int128_t(high) << 64) + low), "31242342342354235233324342342312341234");
    *isNull = false;

    s = "312423423423542352333243423423.123412349";
    CastStringToDecimal128RoundUpRetNull(isNull, s.c_str(), static_cast<int32_t>(s.size()), 38, 8, &high, &low);
    EXPECT_FALSE(*isNull);
    EXPECT_EQ(ToString((int128_t(high) << 64) + low), "31242342342354235233324342342312341235");
    *isNull = false;

    s = "312423423423542352333243423423.123412349123324123";
    CastStringToDecimal128RoundUpRetNull(isNull, s.c_str(), static_cast<int32_t>(s.size()), 38, 8, &high, &low);
    EXPECT_FALSE(*isNull);
    EXPECT_EQ(ToString((int128_t(high) << 64) + low), "31242342342354235233324342342312341235");
    *isNull = false;

    s = "-76519271657238389219223372036854775808.01";
    CastStringToDecimal128RoundUpRetNull(isNull, s.c_str(), static_cast<int32_t>(s.size()), 38, 0, &high, &low);
    EXPECT_FALSE(*isNull);
    EXPECT_EQ(ToString((int128_t(high) << 64) + low), "-76519271657238389219223372036854775808");
    *isNull = false;

    s = "312423423423542352333243423123124431243.123412349";
    CastStringToDecimal128RoundUpRetNull(isNull, s.c_str(), static_cast<int32_t>(s.size()), 38, 8, &high, &low);
    EXPECT_TRUE(*isNull);
    *isNull = false;

    s = "312423423423542352333243423423.123412349123324123a";
    CastStringToDecimal128RoundUpRetNull(isNull, s.c_str(), static_cast<int32_t>(s.size()), 38, 8, &high, &low);
    EXPECT_TRUE(*isNull);
    *isNull = false;

    s = "2.34e-39";
    CastStringToDecimal128RoundUpRetNull(isNull, s.c_str(), static_cast<int32_t>(s.size()), 38, 8, &high, &low);
    EXPECT_FALSE(*isNull);
    *isNull = false;

    s = "2.34e39";
    CastStringToDecimal128RoundUpRetNull(isNull, s.c_str(), static_cast<int32_t>(s.size()), 38, 8, &high, &low);
    EXPECT_TRUE(*isNull);

    delete context;
    delete isNull;
}

TEST(FunctionTest, CastStringToDecimal64RoundUp)
{
    auto context = new ExecutionContext(4096, nullptr);
    bool *isNull = new bool(false);
    std::string s = "-97179.3541993373211719314416";
    int64_t res = CastStringToDecimal64RoundUpRetNull(isNull, s.c_str(), static_cast<int32_t>(s.size()), 7, 0);
    EXPECT_EQ(res, -97179);
    *isNull = false;

    s = "+97179.3541993373211719314416";
    res = CastStringToDecimal64RoundUpRetNull(isNull, s.c_str(), static_cast<int32_t>(s.size()), 7, 0);
    EXPECT_EQ(res, 97179);
    *isNull = false;

    s = "865336.1947182416341553640412669021910285.80122768";
    res = CastStringToDecimal64RoundUpRetNull(isNull, s.c_str(), static_cast<int32_t>(s.size()), 7, 0);
    EXPECT_EQ(res, 0);
    EXPECT_TRUE(*isNull);
    *isNull = false;

    s = "  +2147483648";
    res = CastStringToDecimal64RoundUpRetNull(isNull, s.c_str(), static_cast<int32_t>(s.size()), 18, 2);
    EXPECT_EQ(res, 214748364800);
    EXPECT_FALSE(*isNull);
    *isNull = false;

    s = "  ";
    res = CastStringToDecimal64RoundUpRetNull(isNull, s.c_str(), static_cast<int32_t>(s.size()), 18, 2);
    EXPECT_EQ(res, 0);
    EXPECT_TRUE(*isNull);

    delete context;
    delete isNull;
}

TEST(FunctionTest, CastDecimal64ToString)
{
    auto context = new ExecutionContext(4096, nullptr);
    auto contextPtr = reinterpret_cast<int64_t>(context);
    int64_t input = 123;
    int32_t outLen = 0;
    const char *s = CastDecimal64ToString(contextPtr, input, 3, 0, false, &outLen);
    EXPECT_EQ(std::string(s, outLen), "123");
    input = 123423;
    s = CastDecimal64ToString(contextPtr, input, 6, 3, false, &outLen);
    EXPECT_EQ(std::string(s, outLen), "123.423");
    input = 1;
    s = CastDecimal64ToString(contextPtr, input, 10, 6, false, &outLen);
    EXPECT_EQ(std::string(s, outLen), "0.000001");
    delete context;
}

TEST(FunctionTest, CastDecimal128ToString)
{
    auto context = new ExecutionContext(4096, nullptr);
    auto contextPtr = reinterpret_cast<int64_t>(context);

    int32_t outLen = 0;
    const char *s = CastDecimal128ToString(contextPtr, 0, 123, 3, 0, false, &outLen);
    EXPECT_EQ(std::string(s, outLen), "123");
    s = CastDecimal128ToString(contextPtr, 0, 123423, 6, 3, false, &outLen);
    EXPECT_EQ(std::string(s, outLen), "123.423");
    s = CastDecimal128ToString(contextPtr, 0, 1, 10, 6, false, &outLen);
    EXPECT_EQ(std::string(s, outLen), "0.000001");
    s = CastDecimal128ToString(contextPtr, 1, 0, 38, 0, false, &outLen);
    EXPECT_EQ(std::string(s, outLen), "18446744073709551616");
    delete context;
}

// Decimal Operation
TEST(FunctionTest, DecimalAddOpeartion)
{
    ConfigUtil::SetCheckReScaleRule(CheckReScaleRule::CHECK_RESCALE);

    auto context = new ExecutionContext(4096, nullptr);
    auto contextPtr = reinterpret_cast<int64_t>(context);
    int64_t high;
    uint64_t low;

    // dec64 add dec64 return dec64
    int64_t result = AddDec64Dec64Dec64ReScale(contextPtr, 123, 3, 2, 321, 3, 1, 4, 2);
    EXPECT_EQ(result, 3333);
    result = AddDec64Dec64Dec64ReScale(contextPtr, 123, 3, 2, 321, 3, 1, 5, 3);
    EXPECT_EQ(result, 33330);
    // dec64 add dec128 return dec128
    AddDec64Dec128Dec128ReScale(contextPtr, 123, 3, 2, 0, 321, 3, 1, 4, 2, &high, &low);
    EXPECT_EQ(high, 0);
    EXPECT_EQ(low, 3333);
    AddDec64Dec128Dec128ReScale(contextPtr, 123, 3, 2, 0, 321, 3, 1, 5, 3, &high, &low);
    EXPECT_EQ(high, 0);
    EXPECT_EQ(low, 33330);
    // dec128 add dec64 return dec128
    AddDec128Dec64Dec128ReScale(contextPtr, 0, 123, 3, 2, 321, 3, 1, 4, 2, &high, &low);
    EXPECT_EQ(high, 0);
    EXPECT_EQ(low, 3333);
    AddDec128Dec64Dec128ReScale(contextPtr, 0, 123, 3, 2, 321, 3, 1, 5, 3, &high, &low);
    EXPECT_EQ(high, 0);
    EXPECT_EQ(low, 33330);
    // dec64 add dec64 return dec128
    AddDec64Dec128Dec128ReScale(contextPtr, 123, 3, 2, 0, 321, 3, 1, 4, 2, &high, &low);
    EXPECT_EQ(high, 0);
    EXPECT_EQ(low, 3333);
    AddDec64Dec128Dec128ReScale(contextPtr, 123, 3, 2, 0, 321, 3, 1, 5, 3, &high, &low);
    EXPECT_EQ(high, 0);
    EXPECT_EQ(low, 33330);
    // dec128 add dec128 return dec128
    AddDec128Dec128Dec128ReScale(contextPtr, 0, 123, 3, 2, 0, 321, 3, 1, 4, 2, &high, &low);
    EXPECT_EQ(high, 0);
    EXPECT_EQ(low, 3333);
    AddDec128Dec128Dec128ReScale(contextPtr, 0, 123, 3, 2, 0, 321, 3, 1, 5, 3, &high, &low);
    EXPECT_EQ(high, 0);
    EXPECT_EQ(low, 33330);
    Decimal128 right = Decimal128("99999999999999999999980000000000000000");
    AddDec128Dec64Dec128ReScale(contextPtr, right.HighBits(), right.LowBits(), 38, 6, 999999999999999999L, 18, 6, 38, 6,
        &high, &low);
    EXPECT_TRUE(context->HasError());
    delete context;
}

TEST(FunctionTest, DecimalMulOpeartion)
{
    ConfigUtil::SetCheckReScaleRule(CheckReScaleRule::CHECK_RESCALE);

    auto context = new ExecutionContext(4096, nullptr);
    auto contextPtr = reinterpret_cast<int64_t>(context);
    int64_t high;
    uint64_t low;

    // dec64 mul dec64 return dec64
    int64_t result = MulDec64Dec64Dec64ReScale(contextPtr, 123, 3, 2, 321, 3, 1, 5, 3);
    EXPECT_EQ(result, 39483);
    result = MulDec64Dec64Dec64ReScale(contextPtr, 123, 3, 2, 321, 3, 1, 6, 4);
    EXPECT_EQ(result, 394830);
    // dec64 mul dec64 return dec128
    MulDec64Dec64Dec128ReScale(contextPtr, 123, 3, 2, 321, 3, 1, 5, 3, &high, &low);
    EXPECT_EQ(high, 0);
    EXPECT_EQ(low, 39483);
    MulDec64Dec64Dec128ReScale(contextPtr, 123, 3, 2, 321, 3, 1, 6, 4, &high, &low);
    EXPECT_EQ(high, 0);
    EXPECT_EQ(low, 394830);
    // dec128 mul dec128 return dec128
    Decimal128 l = Decimal128("123456789123456456789");
    Decimal128 r = Decimal128("1234567891234567567891");
    bool isNull = false;
    MulDec128Dec128Dec128RetNull(&isNull, l.HighBits(), l.LowBits(), 22, 6, r.HighBits(), r.LowBits(), 22, 6, 38, 6,
        &high, &low);
    EXPECT_EQ(Decimal128(high, low).ToString(), "152415787806736335252649604807436065");

    l = Decimal128("12345678912345623");
    r = Decimal128("1234567891234567567891");
    MulDec128Dec128Dec128ReScale(contextPtr, l.HighBits(), l.LowBits(), 17, 2, r.HighBits(), r.LowBits(), 22, 6, 38, 6,
        &high, &low);
    EXPECT_EQ(Decimal128(high, low).ToString(), "152415787806736055266232119611091911");

    l = Decimal128("1164203084928762410");
    r = Decimal128("1000000000000000000000");
    MulDec128Dec128Dec128ReScale(contextPtr, l.HighBits(), l.LowBits(), 38, 19, r.HighBits(), r.LowBits(), 38, 19, 38,
        15, &high, &low);
    EXPECT_EQ(Decimal128(high, low).ToString(), "11642030849287624");

    MulDec128Dec128Dec128ReScale(contextPtr, 0, 123, 3, 2, 0, 321, 3, 1, 5, 3, &high, &low);
    EXPECT_EQ(high, 0);
    EXPECT_EQ(low, 39483);
    // dec64 mul dec128 return dec128
    MulDec64Dec128Dec128ReScale(contextPtr, 123, 3, 2, 0, 321, 3, 1, 5, 3, &high, &low);
    EXPECT_EQ(high, 0);
    EXPECT_EQ(low, 39483);
    MulDec64Dec128Dec128ReScale(contextPtr, 123, 3, 2, 0, 321, 3, 1, 6, 4, &high, &low);
    EXPECT_EQ(high, 0);
    EXPECT_EQ(low, 394830);
    // dec128 mul dec64 return dec128
    MulDec128Dec64Dec128ReScale(contextPtr, 0, 123, 3, 2, 321, 3, 1, 5, 3, &high, &low);
    EXPECT_EQ(high, 0);
    EXPECT_EQ(low, 39483);
    MulDec128Dec64Dec128ReScale(contextPtr, 0, 123, 3, 2, 321, 3, 1, 6, 4, &high, &low);
    EXPECT_EQ(high, 0);
    EXPECT_EQ(low, 394830);
    delete context;
}

TEST(FunctionTest, DecimalModOpeartion)
{
    ConfigUtil::SetCheckReScaleRule(CheckReScaleRule::CHECK_RESCALE);

    auto context = new ExecutionContext(4096, nullptr);
    auto contextPtr = reinterpret_cast<int64_t>(context);
    // dec64 mod dec64 return dec64
    int64_t result = ModDec64Dec64Dec64ReScale(contextPtr, -1234500, 7, 2, 1234512, 7, 2, 7, 2);
    EXPECT_EQ(result, -1234500);

    Decimal128 inputValue(-23452700000965473);
    result = ModDec64Dec128Dec64ReScale(contextPtr, 500009700000012345, 18, 18, inputValue.HighBits(),
        inputValue.LowBits(), 18, 5, 18, 18);
    EXPECT_EQ(result, 500009700000012345);

    int64_t high;
    uint64_t low;
    ModDec64Dec128Dec128ReScale(contextPtr, -1234500, 7, 2, 0, 1234512, 7, 2, 7, 2, &high, &low);
    EXPECT_EQ(high, ~0);
    EXPECT_EQ(low, -1234500);

    Decimal128 right = Decimal128("250009700094102345239493000152399025");
    ModDec128Dec64Dec128ReScale(contextPtr, right.HighBits(), right.LowBits(), 36, 36, 7, 10, 0, 36, 36, &high, &low);
    EXPECT_EQ(Decimal128(high, low).ToString(), "250009700094102345239493000152399025");
    delete context;
}

TEST(FunctionTest, DecimalDivOpeartion)
{
    ConfigUtil::SetCheckReScaleRule(CheckReScaleRule::CHECK_RESCALE);

    auto context = new ExecutionContext(4096, nullptr);
    auto contextPtr = reinterpret_cast<int64_t>(context);
    Decimal128 right = Decimal128("9999999999999999999999");
    Decimal128 left = Decimal128("99999999999999999999999999999999999999");
    int64_t high = 0;
    uint64_t low = 0;
    // dec128 mul dec128 return dec128
    DivDec128Dec128Dec128ReScale(contextPtr, left.HighBits(), left.LowBits(), 38, 16, right.HighBits(), right.LowBits(),
        22, 6, 38, 16, &high, &low);
    EXPECT_EQ(Decimal128(high, low).ToString(), "10000000000000000000001");

    DivDec64Dec64Dec128ReScale(contextPtr, -111, 7, 2, 50'0009'7000'0001'2345, 18, 18, 19, 18, &high, &low);
    EXPECT_EQ(Decimal128(high, low).ToString(), "-2219956932835448182");

    DivDec64Dec64Dec128ReScale(contextPtr, 1111111, 7, 2, 50'0009'7000'0001'2345, 18, 18, 25, 18, &high, &low);
    EXPECT_EQ(Decimal128(high, low).ToString(), "22221788897294843824062");

    DivDec64Dec64Dec128ReScale(contextPtr, 50'0009'7000'0001'2345, 18, 18, 1111111, 7, 2, 19, 18, &high, &low);
    EXPECT_EQ(Decimal128(high, low).ToString(), "45000877500089");

    int64_t result = DivDec64Dec64Dec64ReScale(contextPtr, -5000000, 7, 0, -5600014, 7, 2, 7, 2);
    EXPECT_EQ(result, 8929);

    DivDec128Dec128Dec128ReScale(contextPtr, 0, 100, 38, 0, 0, 3, 38, 0, 38, 3, &high, &low);
    right = Decimal128("-99999999999999999999999999999999999999");
    DivDec128Dec128Dec128ReScale(contextPtr, right.HighBits(), right.LowBits(), 38, 16, 0, 999999, 22, 6, 38, 16, &high,
        &low);
    EXPECT_TRUE(context->HasError());
    delete context;
}

TEST(FunctionTest, DecimalAddOpeartionForSpark)
{
    ConfigUtil::SetCheckReScaleRule(CheckReScaleRule::NOT_CHECK_RESCALE);

    auto context = new ExecutionContext(4096, nullptr);
    auto contextPtr = reinterpret_cast<int64_t>(context);
    int64_t high;
    uint64_t low;

    // dec64 add dec64 return dec64
    int64_t result = AddDec64Dec64Dec64NotReScale(contextPtr, 123, 3, 2, 321, 3, 1, 4, 2);
    EXPECT_EQ(result, 3333);
    result = AddDec64Dec64Dec64NotReScale(contextPtr, 123, 3, 2, 321, 3, 1, 5, 3);
    EXPECT_EQ(result, 3333);
    // dec64 add dec128 return dec128
    AddDec64Dec128Dec128NotReScale(contextPtr, 123, 3, 2, 0, 321, 3, 1, 4, 2, &high, &low);
    EXPECT_EQ(high, 0);
    EXPECT_EQ(low, 3333);
    AddDec64Dec128Dec128NotReScale(contextPtr, 123, 3, 2, 0, 321, 3, 1, 5, 3, &high, &low);
    EXPECT_EQ(high, 0);
    EXPECT_EQ(low, 3333);
    // dec128 add dec64 return dec128
    AddDec128Dec64Dec128NotReScale(contextPtr, 0, 123, 3, 2, 321, 3, 1, 4, 2, &high, &low);
    EXPECT_EQ(high, 0);
    EXPECT_EQ(low, 3333);
    AddDec128Dec64Dec128NotReScale(contextPtr, 0, 123, 3, 2, 321, 3, 1, 5, 3, &high, &low);
    EXPECT_EQ(high, 0);
    EXPECT_EQ(low, 3333);
    // dec64 add dec64 return dec128
    AddDec64Dec128Dec128NotReScale(contextPtr, 123, 3, 2, 0, 321, 3, 1, 4, 2, &high, &low);
    EXPECT_EQ(high, 0);
    EXPECT_EQ(low, 3333);
    AddDec64Dec128Dec128NotReScale(contextPtr, 123, 3, 2, 0, 321, 3, 1, 5, 3, &high, &low);
    EXPECT_EQ(high, 0);
    EXPECT_EQ(low, 3333);
    // dec128 add dec128 return dec128
    AddDec128Dec128Dec128NotReScale(contextPtr, 0, 123, 3, 2, 0, 321, 3, 1, 4, 2, &high, &low);
    EXPECT_EQ(high, 0);
    EXPECT_EQ(low, 3333);
    AddDec128Dec128Dec128NotReScale(contextPtr, 0, 123, 3, 2, 0, 321, 3, 1, 5, 3, &high, &low);
    EXPECT_EQ(high, 0);
    EXPECT_EQ(low, 3333);
    Decimal128 right = Decimal128("99999999999999999999980000000000000000");
    AddDec128Dec64Dec128ReScale(contextPtr, right.HighBits(), right.LowBits(), 38, 6, 999999999999999999L, 18, 6, 38, 6,
        &high, &low);
    EXPECT_TRUE(context->HasError());
    delete context;
}

TEST(FunctionTest, DecimalMulOpeartionForSpark)
{
    ConfigUtil::SetCheckReScaleRule(CheckReScaleRule::NOT_CHECK_RESCALE);

    auto context = new ExecutionContext(4096, nullptr);
    auto contextPtr = reinterpret_cast<int64_t>(context);
    int64_t high;
    uint64_t low;

    // dec64 mul dec64 return dec64
    int64_t result = MulDec64Dec64Dec64NotReScale(contextPtr, 123, 3, 2, 321, 3, 1, 5, 3);
    EXPECT_EQ(result, 39483);
    result = MulDec64Dec64Dec64NotReScale(contextPtr, 123, 3, 2, 321, 3, 1, 6, 4);
    EXPECT_EQ(result, 39483);
    // dec64 mul dec64 return dec128
    MulDec64Dec64Dec128NotReScale(contextPtr, 123, 3, 2, 321, 3, 1, 5, 3, &high, &low);
    EXPECT_EQ(high, 0);
    EXPECT_EQ(low, 39483);
    MulDec64Dec64Dec128NotReScale(contextPtr, 123, 3, 2, 321, 3, 1, 6, 4, &high, &low);
    EXPECT_EQ(high, 0);
    EXPECT_EQ(low, 39483);
    // dec128 mul dec128 return dec128
    Decimal128 l = Decimal128("123456789123456456789");
    Decimal128 r = Decimal128("1234567891234567567891");
    bool isNull = false;
    MulDec128Dec128Dec128RetNull(&isNull, l.HighBits(), l.LowBits(), 22, 6, r.HighBits(), r.LowBits(), 22, 6, 38, 6,
        &high, &low);
    EXPECT_EQ(Decimal128(high, low).ToString(), "152415787806736335252649604807436065");

    l = Decimal128("12345678912345623");
    r = Decimal128("1234567891234567567891");
    MulDec128Dec128Dec128NotReScale(contextPtr, l.HighBits(), l.LowBits(), 17, 2, r.HighBits(), r.LowBits(), 22, 6, 38,
        6, &high, &low);
    EXPECT_EQ(Decimal128(high, low).ToString(), "15241578780673605526623211961109191093");

    MulDec128Dec128Dec128NotReScale(contextPtr, 0, 123, 3, 2, 0, 321, 3, 1, 5, 3, &high, &low);
    EXPECT_EQ(high, 0);
    EXPECT_EQ(low, 39483);
    // dec64 mul dec128 return dec128
    MulDec64Dec128Dec128NotReScale(contextPtr, 123, 3, 2, 0, 321, 3, 1, 5, 3, &high, &low);
    EXPECT_EQ(high, 0);
    EXPECT_EQ(low, 39483);
    MulDec64Dec128Dec128NotReScale(contextPtr, 123, 3, 2, 0, 321, 3, 1, 6, 4, &high, &low);
    EXPECT_EQ(high, 0);
    EXPECT_EQ(low, 39483);
    // dec128 mul dec64 return dec128
    MulDec128Dec64Dec128NotReScale(contextPtr, 0, 123, 3, 2, 321, 3, 1, 5, 3, &high, &low);
    EXPECT_EQ(high, 0);
    EXPECT_EQ(low, 39483);
    MulDec128Dec64Dec128NotReScale(contextPtr, 0, 123, 3, 2, 321, 3, 1, 6, 4, &high, &low);
    EXPECT_EQ(high, 0);
    EXPECT_EQ(low, 39483);
    delete context;
}

TEST(FunctionTest, DecimalModOpeartionForSpark)
{
    ConfigUtil::SetCheckReScaleRule(CheckReScaleRule::NOT_CHECK_RESCALE);

    auto context = new ExecutionContext(4096, nullptr);
    auto contextPtr = reinterpret_cast<int64_t>(context);
    // dec64 mod dec64 return dec64
    int64_t result = ModDec64Dec64Dec64NotReScale(contextPtr, -1234500, 7, 2, 1234512, 7, 2, 7, 2);
    EXPECT_EQ(result, -1234500);

    Decimal128 inputValue(-23452700000965473);
    result = ModDec64Dec128Dec64NotReScale(contextPtr, 500009700000012345, 18, 18, inputValue.HighBits(),
        inputValue.LowBits(), 18, 5, 18, 18);
    EXPECT_EQ(result, 500009700000012345);

    int64_t high;
    uint64_t low;
    ModDec64Dec128Dec128NotReScale(contextPtr, -1234500, 7, 2, 0, 1234512, 7, 2, 7, 2, &high, &low);
    EXPECT_EQ(high, ~0);
    EXPECT_EQ(low, -1234500);

    Decimal128 right = Decimal128("250009700094102345239493000152399025");
    ModDec128Dec64Dec128NotReScale(contextPtr, right.HighBits(), right.LowBits(), 36, 36, 7, 10, 0, 36, 36, &high,
        &low);
    EXPECT_EQ(Decimal128(high, low).ToString(), "250009700094102345239493000152399025");
    delete context;
}

TEST(FunctionTest, DecimalDivOpeartionForSpark)
{
    ConfigUtil::SetCheckReScaleRule(CheckReScaleRule::NOT_CHECK_RESCALE);

    auto context = new ExecutionContext(4096, nullptr);
    auto contextPtr = reinterpret_cast<int64_t>(context);
    Decimal128 right = Decimal128("9999999999999999999999");
    Decimal128 left = Decimal128("99999999999999999999999999999999999999");
    int64_t high = 0;
    uint64_t low = 0;
    // dec128 mul dec128 return dec128
    DivDec128Dec128Dec128NotReScale(contextPtr, left.HighBits(), left.LowBits(), 38, 16, right.HighBits(),
        right.LowBits(), 22, 6, 38, 16, &high, &low);
    EXPECT_EQ(Decimal128(high, low).ToString(), "10000000000000000000001");

    DivDec64Dec64Dec128NotReScale(contextPtr, -111, 7, 2, 50'0009'7000'0001'2345, 18, 18, 19, 18, &high, &low);
    EXPECT_EQ(Decimal128(high, low).ToString(), "-2219956932835448182");

    DivDec64Dec64Dec128NotReScale(contextPtr, 1111111, 7, 2, 50'0009'7000'0001'2345, 18, 18, 25, 18, &high, &low);
    EXPECT_EQ(Decimal128(high, low).ToString(), "22221788897294843824062");

    DivDec64Dec64Dec128NotReScale(contextPtr, 50'0009'7000'0001'2345, 18, 18, 1111111, 7, 2, 19, 18, &high, &low);
    EXPECT_EQ(Decimal128(high, low).ToString(), "45000877500089");

    int64_t result = DivDec64Dec64Dec64NotReScale(contextPtr, -5000000, 7, 0, -5600014, 7, 2, 7, 2);
    EXPECT_EQ(result, 8929);

    DivDec128Dec128Dec128NotReScale(contextPtr, 0, 100, 38, 0, 0, 3, 38, 0, 38, 3, &high, &low);
    right = Decimal128("-99999999999999999999999999999999999999");
    DivDec128Dec128Dec128NotReScale(contextPtr, right.HighBits(), right.LowBits(), 38, 16, 0, 999999, 22, 6, 38, 16,
        &high, &low);
    EXPECT_TRUE(context->HasError());
    delete context;
}

TEST(FunctionTest, CastDecimal64ToDoubleRetNull)
{
    bool isNull = false;
    double result = CastDecimal64ToDoubleRetNull(&isNull, 100, 38, 0);
    EXPECT_EQ(result, 100);
    result = CastDecimal64ToDoubleRetNull(&isNull, 99, 38, 1);
    EXPECT_EQ(result, 9.9);
    result = CastDecimal64ToDoubleRetNull(&isNull, 100, 38, 0);
    EXPECT_EQ(result, 100);
    result = CastDecimal64ToDoubleRetNull(&isNull, -8888, 38, 2);
    EXPECT_EQ(result, -88.88);
    result = CastDecimal64ToDoubleRetNull(&isNull, 12'3456'7890'1234'5612L, 18, 2);
    EXPECT_EQ(result, 1.234567890123456E15);
}

TEST(FunctionTest, CastDecimal128ToDoubleRetNull)
{
    bool isNull = false;
    double result = CastDecimal128ToDoubleRetNull(&isNull, 0, 100, 38, 0);
    EXPECT_EQ(result, 100);
    result = CastDecimal128ToDoubleRetNull(&isNull, 0, 534, 38, 3);
    EXPECT_EQ(result, 0.534);
    result = CastDecimal128ToDoubleRetNull(&isNull, 0, 123, 38, 5);
    EXPECT_EQ(result, 0.00123);
    result = CastDecimal128ToDoubleRetNull(&isNull, 0, 1234, 38, 2);
    EXPECT_EQ(result, 12.34);
    result = CastDecimal128ToDoubleRetNull(&isNull, 0, 1234, 38, 2);
    EXPECT_EQ(result, 12.34);
}

TEST(FunctionTest, CastDecimal64ToLongRetNull)
{
    bool isNull = false;
    int64_t result = CastDecimal64ToLongRetNull(&isNull, 100, 38, 0);
    EXPECT_EQ(result, 100);
    result = CastDecimal64ToLongRetNull(&isNull, 123999, 38, 3);
    EXPECT_EQ(result, 123);
    result = CastDecimal64ToLongRetNull(&isNull, 123111, 38, 3);
    EXPECT_EQ(result, 123);
}

TEST(FunctionTest, CastDecimal64ToInt_roundingRule_is_down)
{
    ConfigUtil::SetRoundingRule(RoundingRule::DOWN);

    auto context = new ExecutionContext(4096, nullptr);
    auto contextPtr = reinterpret_cast<int64_t>(context);
    bool isNull = false;

    auto result = CastDecimal64ToIntDown(contextPtr, 123456, 4, 2, false);
    ASSERT_EQ(result, 1234);
    ASSERT_FALSE(context->HasError());

    result = CastDecimal64ToIntDown(contextPtr, -214748364799, 15, 2, false);
    ASSERT_EQ(result, -2147483647);

    result = CastDecimal64ToIntRetNull(&isNull, -214748364799, 15, 2);
    ASSERT_EQ(result, -2147483647);

    result = CastDecimal64ToIntRetNull(&isNull, -19501040780034, 17, 2);
    ASSERT_EQ(result, -1736879480);

    result = CastDecimal64ToIntDown(contextPtr, 1232147483667, 11, 2, false);
    ASSERT_EQ(result, -563427052);

    result = CastDecimal64ToIntDown(contextPtr, -1232147483667, 11, 2, false);
    ASSERT_EQ(result, 563427052);

    auto result2 = CastDecimal64ToLongDown(123456, 4, 2, false);
    ASSERT_EQ(result2, 1234);

    ConfigUtil::SetRoundingRule(RoundingRule::HALF_UP);
    delete context;
}

TEST(FunctionTest, CastDecimal128ToInt_roundingRule_is_down)
{
    ConfigUtil::SetRoundingRule(RoundingRule::DOWN);
    auto context = new ExecutionContext(4096, nullptr);
    auto contextPtr = reinterpret_cast<int64_t>(context);

    Decimal128 deci("1234.56");
    auto result = CastDecimal128ToIntDown(contextPtr, deci.HighBits(), deci.LowBits(), 38, 2, false);
    ASSERT_EQ(result, 1234);

    deci = Decimal128("12321474836.67");
    result = CastDecimal128ToIntDown(contextPtr, deci.HighBits(), deci.LowBits(), 38, 2, false);
    ASSERT_EQ(result, -563427052);

    deci = Decimal128("-12321474836.67");
    result = CastDecimal128ToIntDown(contextPtr, deci.HighBits(), deci.LowBits(), 13, 2, false);
    ASSERT_EQ(result, 563427052);

    deci = Decimal128("12321474836.67");
    auto result2 = CastDecimal128ToLongDown(contextPtr, deci.HighBits(), deci.LowBits(), 13, 2, false);
    ASSERT_EQ(result2, 12321474836);

    deci = Decimal128("12345678912345678912345.67");
    result2 = CastDecimal128ToLongDown(contextPtr, deci.HighBits(), deci.LowBits(), 25, 2, false);
    ASSERT_EQ(result2, 4807127033988881241);

    ConfigUtil::SetRoundingRule(RoundingRule::HALF_UP);
    delete context;
}

TEST(FunctionTest, GreatestDecimal64)
{
    auto context = new ExecutionContext(4096, nullptr);
    auto contextPtr = reinterpret_cast<int64_t>(context);
    int64_t x64Value = 12708;
    int64_t y64Value = 12800;
    bool retIsNull = false;
    auto ret = GreatestDecimal64(contextPtr, x64Value, 18, 1, false, y64Value, 18, 1, false, &retIsNull, 18, 1);
    EXPECT_EQ(ret, 12800);
    EXPECT_FALSE(retIsNull);

    x64Value = 0;
    y64Value = 12800;
    retIsNull = false;
    ret = GreatestDecimal64(contextPtr, x64Value, 18, 1, true, y64Value, 18, 1, false, &retIsNull, 18, 1);
    EXPECT_EQ(ret, 12800);
    EXPECT_FALSE(retIsNull);

    x64Value = 12800;
    y64Value = 0;
    retIsNull = false;
    ret = GreatestDecimal64(contextPtr, x64Value, 18, 1, false, y64Value, 18, 1, true, &retIsNull, 18, 1);
    EXPECT_EQ(ret, 12800);
    EXPECT_FALSE(retIsNull);

    x64Value = 0;
    y64Value = 0;
    retIsNull = false;
    ret = GreatestDecimal64(contextPtr, x64Value, 18, 1, true, y64Value, 18, 1, true, &retIsNull, 18, 1);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(retIsNull);

    x64Value = 12708;
    y64Value = 12800;
    retIsNull = false;
    ret = GreatestDecimal64(contextPtr, x64Value, 18, 2, false, y64Value, 18, 1, false, &retIsNull, 4, 2);
    EXPECT_TRUE(context->HasError());

    delete context;
}

TEST(FunctionTest, GreatestDecimal64RetNull)
{
    bool isNull = false;
    int64_t x64Value = 12708;
    int64_t y64Value = 12800;
    bool retIsNull = false;
    auto ret = GreatestDecimal64RetNull(&isNull, x64Value, 18, 1, false, y64Value, 18, 1, false, &retIsNull, 18, 1);
    EXPECT_EQ(ret, 12800);
    EXPECT_FALSE(isNull);
    EXPECT_FALSE(retIsNull);

    x64Value = 0;
    y64Value = 12800;
    retIsNull = false;
    ret = GreatestDecimal64RetNull(&isNull, x64Value, 18, 1, true, y64Value, 18, 1, false, &retIsNull, 18, 1);
    EXPECT_EQ(ret, 12800);
    EXPECT_FALSE(isNull);
    EXPECT_FALSE(retIsNull);

    x64Value = 12800;
    y64Value = 0;
    retIsNull = false;
    ret = GreatestDecimal64RetNull(&isNull, x64Value, 18, 1, false, y64Value, 18, 1, true, &retIsNull, 18, 1);
    EXPECT_EQ(ret, 12800);
    EXPECT_FALSE(isNull);
    EXPECT_FALSE(retIsNull);

    x64Value = 0;
    y64Value = 0;
    retIsNull = false;
    ret = GreatestDecimal64RetNull(&isNull, x64Value, 18, 1, true, y64Value, 18, 1, true, &retIsNull, 18, 1);
    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(isNull);
    EXPECT_TRUE(retIsNull);

    x64Value = 12708;
    y64Value = 12800;
    retIsNull = false;
    ret = GreatestDecimal64RetNull(&isNull, x64Value, 18, 2, false, y64Value, 18, 1, false, &retIsNull, 4, 2);
    EXPECT_TRUE(isNull);
}

TEST(FunctionTest, GreatestDecimal128)
{
    auto context = new ExecutionContext(4096, nullptr);
    auto contextPtr = reinterpret_cast<int64_t>(context);
    int64_t high = 0;
    uint64_t low = 0;
    bool retIsNull = false;
    GreatestDecimal128(contextPtr, 0, 128, 38, 1, false, 0, 127, 38, 1, false, &retIsNull, 38, 1, &high, &low);
    EXPECT_EQ(high, 0);
    EXPECT_EQ(low, 128);
    EXPECT_FALSE(retIsNull);

    retIsNull = false;
    GreatestDecimal128(contextPtr, 0, 128, 38, 1, false, 0, 0, 38, 1, true, &retIsNull, 38, 1, &high, &low);
    EXPECT_EQ(high, 0);
    EXPECT_EQ(low, 128);
    EXPECT_FALSE(retIsNull);

    retIsNull = false;
    GreatestDecimal128(contextPtr, 0, 0, 38, 1, true, 0, 128, 38, 1, false, &retIsNull, 38, 1, &high, &low);
    EXPECT_EQ(high, 0);
    EXPECT_EQ(low, 128);
    EXPECT_FALSE(retIsNull);

    retIsNull = false;
    GreatestDecimal128(contextPtr, 0, 0, 38, 1, true, 0, 0, 38, 1, true, &retIsNull, 38, 1, &high, &low);
    EXPECT_EQ(high, 0);
    EXPECT_EQ(low, 0);
    EXPECT_TRUE(retIsNull);

    retIsNull = false;
    GreatestDecimal128(contextPtr, 0, 1280, 38, 1, false, 0, 1270, 38, 0, false, &retIsNull, 3, 1, &high, &low);
    EXPECT_TRUE(context->HasError());

    delete context;
}

TEST(FunctionTest, GreatestDecimal128RetNull)
{
    bool isNull = false;
    int64_t high = 0;
    uint64_t low = 0;
    bool retIsNull = false;
    GreatestDecimal128RetNull(&isNull, 0, 128, 38, 1, false, 0, 127, 38, 1, false, &retIsNull, 38, 1, &high, &low);
    EXPECT_EQ(high, 0);
    EXPECT_EQ(low, 128);
    EXPECT_FALSE(isNull);
    EXPECT_FALSE(retIsNull);

    retIsNull = false;
    GreatestDecimal128RetNull(&isNull, 0, 128, 38, 1, false, 0, 0, 38, 1, true, &retIsNull, 38, 1, &high, &low);
    EXPECT_EQ(high, 0);
    EXPECT_EQ(low, 128);
    EXPECT_FALSE(isNull);
    EXPECT_FALSE(retIsNull);

    retIsNull = false;
    GreatestDecimal128RetNull(&isNull, 0, 0, 38, 1, true, 0, 128, 38, 1, false, &retIsNull, 38, 1, &high, &low);
    EXPECT_EQ(high, 0);
    EXPECT_EQ(low, 128);
    EXPECT_FALSE(isNull);
    EXPECT_FALSE(retIsNull);

    retIsNull = false;
    GreatestDecimal128RetNull(&isNull, 0, 0, 38, 1, true, 0, 0, 38, 1, true, &retIsNull, 38, 1, &high, &low);
    EXPECT_EQ(high, 0);
    EXPECT_EQ(low, 0);
    EXPECT_FALSE(isNull);
    EXPECT_TRUE(retIsNull);

    retIsNull = false;
    GreatestDecimal128RetNull(&isNull, 0, 1280, 38, 1, false, 0, 1270, 38, 0, false, &retIsNull, 3, 1, &high, &low);
    EXPECT_TRUE(isNull);
}
}