/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
 * Description: batch function test
 */
#include <string>
#include <vector>
#include "gtest/gtest.h"
#include "codegen/batch_functions/batch_mathfunctions.h"
#include "codegen/batch_functions/batch_murmur3_hash.h"
#include "codegen/batch_functions/batch_decimal_arithmetic_functions.h"
#include "codegen/batch_functions/batch_decimal_cast_functions.h"
#include "operator/execution_context.h"
#include "codegen/batch_functions/batch_stringfunctions.h"
#include "codegen/batch_functions/batch_datetime_functions.h"
#include "util/test_util.h"

using namespace omniruntime::op;
using namespace omniruntime::codegen::function;
using namespace std;
using namespace omniruntime::TestUtil;

template <typename T> bool CmpArray(T *x, T *y, int32_t rowCnt)
{
    for (int i = 0; i < rowCnt; ++i) {
        if (x[i] != y[i]) {
            return false;
        }
    }
    return true;
}

TEST(BatchFunctionTest, CastBasicTypes)
{
    ConfigUtil::SetRoundingRule(RoundingRule::HALF_UP);
    int32_t rowCnt = 2;
    int32_t col1[2] = {-10, 10};
    int64_t col2[2] = {-20L, 20L};
    double col3[2] = {-30.0, 30.0};
    bool resIsNull[2] = {false, false};

    double outDouble[2];
    BatchCastInt32ToDouble(col1, resIsNull, outDouble, rowCnt);
    double expectDouble1[2] = {-10.0, 10.0};
    EXPECT_TRUE(CmpArray<double>(outDouble, expectDouble1, rowCnt));

    BatchCastInt64ToDouble(col2, resIsNull, outDouble, rowCnt);
    double expectDouble2[2] = {-20.0, 20.0};
    EXPECT_TRUE(CmpArray<double>(outDouble, expectDouble2, rowCnt));

    int32_t outInt[2];
    BatchCastInt64ToInt32(col2, resIsNull, outInt, rowCnt);
    int32_t expectInt1[2] = {-20, 20};
    EXPECT_TRUE(CmpArray<int32_t>(outInt, expectInt1, rowCnt));

    BatchCastDoubleToInt32HalfUp(col3, resIsNull, outInt, rowCnt);
    int32_t expectInt2[2] = { -30, 30 };
    EXPECT_TRUE(CmpArray<int32_t>(outInt, expectInt2, rowCnt));

    int64_t outLong[2];
    BatchCastInt32ToInt64(col1, resIsNull, outLong, rowCnt);
    int64_t expectLong1[2] = { -10L, 10L };
    EXPECT_TRUE(CmpArray<int64_t>(outLong, expectLong1, rowCnt));

    BatchCastDoubleToInt64HalfUp(col3, resIsNull, outLong, rowCnt);
    int64_t expectLong2[2] = { -30L, 30L };
    EXPECT_TRUE(CmpArray<int64_t>(outLong, expectLong2, rowCnt));
}

TEST(BatchFunctionTest, Abs)
{
    int32_t rowCnt = 2;
    int32_t col1[2] = { -10, 10 };
    int64_t col2[2] = { -20L, 20L };
    double col3[2] = { -30.0, 30.0 };
    bool resIsNull[2] = { false, false };

    BatchAbs<int32_t>(col1, resIsNull, col1, rowCnt);
    BatchAbs<int64_t>(col2, resIsNull, col2, rowCnt);
    BatchAbs<double>(col3, resIsNull, col3, rowCnt);

    int32_t expectCol1[2] = { 10, 10 };
    int64_t expectCol2[2] = { 20L, 20L };
    double expectCol3[2] = { 30.0, 30.0 };

    EXPECT_TRUE(CmpArray<int32_t>(col1, expectCol1, rowCnt));
    EXPECT_TRUE(CmpArray<int64_t>(col2, expectCol2, rowCnt));
    EXPECT_TRUE(CmpArray<double>(col3, expectCol3, rowCnt));
}

TEST(BatchFunctionTest, IntCmp)
{
    int32_t rowCnt = 2;
    int32_t left[2] = { -10, 20 };
    int32_t right[2] = { -20, 10 };
    bool output[2];
    bool expect[2];

    BatchLessThanInt32(left, right, output, rowCnt);
    expect[0] = false;
    expect[1] = false;
    EXPECT_TRUE(CmpArray<bool>(output, expect, rowCnt));

    BatchLessThanEqualInt32(left, right, output, rowCnt);
    expect[0] = false;
    expect[1] = false;
    EXPECT_TRUE(CmpArray<bool>(output, expect, rowCnt));

    BatchGreaterThanInt32(left, right, output, rowCnt);
    expect[0] = true;
    expect[1] = true;
    EXPECT_TRUE(CmpArray<bool>(output, expect, rowCnt));

    BatchGreaterThanEqualInt32(left, right, output, rowCnt);
    expect[0] = true;
    expect[1] = true;
    EXPECT_TRUE(CmpArray<bool>(output, expect, rowCnt));

    BatchEqualInt32(left, right, output, rowCnt);
    expect[0] = false;
    expect[1] = false;
    EXPECT_TRUE(CmpArray<bool>(output, expect, rowCnt));

    BatchNotEqualInt32(left, right, output, rowCnt);
    expect[0] = true;
    expect[1] = true;
    EXPECT_TRUE(CmpArray<bool>(output, expect, rowCnt));
}

TEST(BatchFunctionTest, IntArith)
{
    int32_t rowCnt = 2;
    int32_t left[2] = { -10, 10 };
    int32_t right[2] = { -20, 20 };
    int32_t expect[2];
    bool isNull[2] = { false, false };
    auto context = new ExecutionContext();
    int64_t contextPtr = reinterpret_cast<int64_t>(context);

    expect[0] = -30;
    expect[1] = 30;
    BatchAddInt32(left, right, rowCnt);
    EXPECT_TRUE(CmpArray<int32_t>(left, expect, rowCnt));

    expect[0] = -10;
    expect[1] = 10;
    BatchSubtractInt32(left, right, rowCnt);
    EXPECT_TRUE(CmpArray<int32_t>(left, expect, rowCnt));

    expect[0] = 200;
    expect[1] = 200;
    BatchMultiplyInt32(left, right, rowCnt);
    EXPECT_TRUE(CmpArray<int32_t>(left, expect, rowCnt));

    expect[0] = -10;
    expect[1] = 10;
    BatchDivideInt32(contextPtr, left, right, rowCnt, isNull);
    EXPECT_TRUE(CmpArray<int32_t>(left, expect, rowCnt));

    expect[0] = -10;
    expect[1] = 10;
    BatchModulusInt32(contextPtr, left, right, rowCnt, isNull);
    EXPECT_TRUE(CmpArray<int32_t>(left, expect, rowCnt));

    delete context;
}

TEST(BatchFunctionTest, LongCmp)
{
    int32_t rowCnt = 2;
    int64_t left[2] = { -10L, 20L };
    int64_t right[2] = { -20L, 10L };
    bool output[2];
    bool expect[2];

    BatchLessThanInt64(left, right, output, rowCnt);
    expect[0] = false;
    expect[1] = false;
    EXPECT_TRUE(CmpArray<bool>(output, expect, rowCnt));

    BatchLessThanEqualInt64(left, right, output, rowCnt);
    expect[0] = false;
    expect[1] = false;
    EXPECT_TRUE(CmpArray<bool>(output, expect, rowCnt));

    BatchGreaterThanInt64(left, right, output, rowCnt);
    expect[0] = true;
    expect[1] = true;
    EXPECT_TRUE(CmpArray<bool>(output, expect, rowCnt));

    BatchGreaterThanEqualInt64(left, right, output, rowCnt);
    expect[0] = true;
    expect[1] = true;
    EXPECT_TRUE(CmpArray<bool>(output, expect, rowCnt));

    BatchEqualInt64(left, right, output, rowCnt);
    expect[0] = false;
    expect[1] = false;
    EXPECT_TRUE(CmpArray<bool>(output, expect, rowCnt));

    BatchNotEqualInt64(left, right, output, rowCnt);
    expect[0] = true;
    expect[1] = true;
    EXPECT_TRUE(CmpArray<bool>(output, expect, rowCnt));
}

TEST(BatchFunctionTest, LongArith)
{
    int32_t rowCnt = 2;
    int64_t left[2] = { -10L, 10L };
    int64_t right[2] = { -20L, 20L };
    int64_t expect[2];
    bool isNull[2] = { false, false };
    auto context = new ExecutionContext();
    int64_t contextPtr = reinterpret_cast<int64_t>(context);

    expect[0] = -30L;
    expect[1] = 30L;
    BatchAddInt64(left, right, rowCnt);
    EXPECT_TRUE(CmpArray<int64_t>(left, expect, rowCnt));

    expect[0] = -10L;
    expect[1] = 10L;
    BatchSubtractInt64(left, right, rowCnt);
    EXPECT_TRUE(CmpArray<int64_t>(left, expect, rowCnt));

    expect[0] = 200L;
    expect[1] = 200L;
    BatchMultiplyInt64(left, right, rowCnt);
    EXPECT_TRUE(CmpArray<int64_t>(left, expect, rowCnt));

    expect[0] = -10L;
    expect[1] = 10L;
    BatchDivideInt64(contextPtr, left, right, rowCnt, isNull);
    EXPECT_TRUE(CmpArray<int64_t>(left, expect, rowCnt));

    expect[0] = -10L;
    expect[1] = 10L;
    BatchModulusInt64(contextPtr, left, right, rowCnt, isNull);
    EXPECT_TRUE(CmpArray<int64_t>(left, expect, rowCnt));

    delete context;
}

TEST(BatchFunctionTest, DoubleCmp)
{
    int32_t rowCnt = 2;
    double left[2] = { -10.0, 20.0 };
    double right[2] = { -20.0, 10.0 };
    bool output[2];
    bool expect[2];

    BatchLessThanDouble(left, right, output, rowCnt);
    expect[0] = false;
    expect[1] = false;
    EXPECT_TRUE(CmpArray<bool>(output, expect, rowCnt));

    BatchLessThanEqualDouble(left, right, output, rowCnt);
    expect[0] = false;
    expect[1] = false;
    EXPECT_TRUE(CmpArray<bool>(output, expect, rowCnt));

    BatchGreaterThanDouble(left, right, output, rowCnt);
    expect[0] = true;
    expect[1] = true;
    EXPECT_TRUE(CmpArray<bool>(output, expect, rowCnt));

    BatchGreaterThanEqualDouble(left, right, output, rowCnt);
    expect[0] = true;
    expect[1] = true;
    EXPECT_TRUE(CmpArray<bool>(output, expect, rowCnt));

    BatchEqualDouble(left, right, output, rowCnt);
    expect[0] = false;
    expect[1] = false;
    EXPECT_TRUE(CmpArray<bool>(output, expect, rowCnt));

    BatchNotEqualDouble(left, right, output, rowCnt);
    expect[0] = true;
    expect[1] = true;
    EXPECT_TRUE(CmpArray<bool>(output, expect, rowCnt));
}

TEST(BatchFunctionTest, DoubleArith)
{
    int32_t rowCnt = 2;
    double left[2] = { -10.0, 10.0 };
    double right[2] = { -20.0, 20.0 };
    double expect[2];

    expect[0] = -30.0;
    expect[1] = 30.0;
    BatchAddDouble(left, right, rowCnt);
    EXPECT_TRUE(CmpArray<double>(left, expect, rowCnt));

    expect[0] = -10.0;
    expect[1] = 10.0;
    BatchSubtractDouble(left, right, rowCnt);
    EXPECT_TRUE(CmpArray<double>(left, expect, rowCnt));

    expect[0] = 200.0;
    expect[1] = 200.0;
    BatchMultiplyDouble(left, right, rowCnt);
    EXPECT_TRUE(CmpArray<double>(left, expect, rowCnt));

    expect[0] = -10.0;
    expect[1] = 10.0;
    BatchDivideDouble(left, right, rowCnt);
    EXPECT_TRUE(CmpArray<double>(left, expect, rowCnt));

    expect[0] = -10.0;
    expect[1] = 10.0;
    BatchModulusDouble(left, right, rowCnt);
    EXPECT_TRUE(CmpArray<double>(left, expect, rowCnt));
}

bool CompareDoubleBits(double d1, double d2)
{
    uint64_t bits1;
    uint64_t bits2;
    memcpy(&bits1, &d1, sizeof(double));
    memcpy(&bits2, &d2, sizeof(double));
    return bits1 == bits2;
}

TEST(BatchFunctionTest, BatchNormalizeNaNAndZero)
{
    const int32_t rowCnt = 3;
    uint64_t nanBits = 0xFFF8000000000001L;
    double nanDouble = 0;
    memcpy(&nanDouble, &nanBits, sizeof(nanBits));
    double value = 3.5;
    double input[rowCnt] = {-0.0, nanDouble, value};
    bool isAnyNull[rowCnt] = {false, false, false};
    double output[rowCnt];
    std::vector<double> expect = {0.0, 0.0 / 0.0, value};
    BatchNormalizeNaNAndZero(input, isAnyNull, output, rowCnt);
    for (int32_t i = 0; i < rowCnt; i++) {
        EXPECT_TRUE(CompareDoubleBits(expect[i], output[i]));
    }
    EXPECT_FALSE(CompareDoubleBits(input[0], output[0]));
    EXPECT_FALSE(CompareDoubleBits(input[1], output[1]));
}

TEST(BatchFunctionTest, BatchPowerDouble)
{
    int32_t rowCnt = 2;
    double base[2] = { -10.0, 10.0 };
    double exponent[2] = { 2.0, 3.0 };
    double output[2];
    double expect[2] = { 100.0, 1000.0 };
    BatchPowerDouble(base, exponent, output, rowCnt);
    EXPECT_TRUE(CmpArray<double>(output, expect, rowCnt));
}

TEST(BatchFunctionTest, Mm3Hash)
{
    int32_t rowCnt = 1;
    int32_t intVal[1] = { -2147483648 };
    int64_t longVal[1] = { -2147483648L };
    double doubleVal[1] = { 123.456 };
    int64_t decimal64Val[1] = { -2147483648L };
    uint8_t *strVal[1];
    bool boolVal[1] = { true };
    string str = "hello world";
    strVal[0] = reinterpret_cast<uint8_t *>(const_cast<char *>(str.c_str()));
    int32_t strLen[1] = { 11 };
    Decimal128 decimal128Val[1];
    decimal128Val[0].SetValue(0, 4000);

    int32_t seed[1] = { 42 };
    bool isValNull[1] = { false };
    bool isSeedNull[1] = { false };
    bool resIsNull[1] = { false };
    int32_t output[1];

    BatchMm3Int32(intVal, isValNull, seed, isSeedNull, resIsNull, output, rowCnt);
    EXPECT_EQ(output[0], 723455942);

    BatchMm3Int64(longVal, isValNull, seed, isSeedNull, resIsNull, output, rowCnt);
    EXPECT_EQ(output[0], -1889108749);

    BatchMm3Double(doubleVal, isValNull, seed, isSeedNull, resIsNull, output, rowCnt);
    EXPECT_EQ(output[0], -39269148);

    BatchMm3String(strVal, strLen, isValNull, seed, isSeedNull, resIsNull, output, rowCnt);
    EXPECT_EQ(output[0], -1528836094);

    BatchMm3Decimal64(decimal64Val, 8, 2, isValNull, seed, isSeedNull, resIsNull, output, rowCnt);
    EXPECT_EQ(output[0], -1889108749);

    BatchMm3Decimal128(decimal128Val, 38, 20, isValNull, seed, isSeedNull, resIsNull, output, rowCnt);
    EXPECT_EQ(output[0], 1534069305);

    BatchMm3Boolean(boolVal, isValNull, seed, isSeedNull, resIsNull, output, rowCnt);
    EXPECT_EQ(output[0], -559580957);
}


TEST(BatchFunctionTest, Decimal64Cmp)
{
    int32_t rowCnt = 2;
    int64_t col1[2] = { 12345678L, 123456L };
    int64_t col2[2] = { 12345678L, 1234567890L };
    bool output[2];

    BatchLessThanDecimal64(col1, 8, 2, col2, 9, 2, output, rowCnt);
    EXPECT_EQ(output[0], false);
    EXPECT_EQ(output[1], true);

    BatchLessThanEqualDecimal64(col1, 8, 3, col2, 9, 2, output, rowCnt);
    EXPECT_EQ(output[0], true);
    EXPECT_EQ(output[1], true);

    BatchGreaterThanDecimal64(col1, 8, 6, col2, 9, 2, output, rowCnt);
    EXPECT_EQ(output[0], false);
    EXPECT_EQ(output[1], false);

    BatchGreaterThanEqualDecimal64(col1, 8, 2, col2, 9, 2, output, rowCnt);
    EXPECT_EQ(output[0], true);
    EXPECT_EQ(output[1], false);

    BatchEqualDecimal64(col1, 8, 2, col2, 9, 2, output, rowCnt);
    EXPECT_EQ(output[0], true);
    EXPECT_EQ(output[1], false);

    BatchNotEqualDecimal64(col1, 8, 2, col2, 9, 2, output, rowCnt);
    EXPECT_EQ(output[0], false);
    EXPECT_EQ(output[1], true);
}

TEST(BatchFunctionTest, Decimal128Cmp)
{
    int32_t rowCnt = 2;
    Decimal128 col1[2];
    col1[0].SetValue(1234567L, UINT64_MAX);
    col1[1].SetValue(123456L, UINT64_MAX);
    Decimal128 col2[2];
    col2[0].SetValue(-1234567L, UINT64_MAX);
    col2[1].SetValue(1234567L, UINT64_MAX);
    bool output[2];

    BatchLessThanDecimal128(col1, 38, 2, col2, 38, 2, output, rowCnt);
    EXPECT_EQ(output[0], false);
    EXPECT_EQ(output[1], true);

    BatchLessThanEqualDecimal128(col1, 38, 4, col2, 38, 3, output, rowCnt);
    EXPECT_EQ(output[0], false);
    EXPECT_EQ(output[1], true);

    BatchGreaterThanDecimal128(col1, 38, 6, col2, 38, 2, output, rowCnt);
    EXPECT_EQ(output[0], true);
    EXPECT_EQ(output[1], false);

    BatchGreaterThanEqualDecimal128(col1, 38, 2, col2, 38, 4, output, rowCnt);
    EXPECT_EQ(output[0], true);
    EXPECT_EQ(output[1], true);

    BatchEqualDecimal128(col1, 38, 2, col2, 38, 2, output, rowCnt);
    EXPECT_EQ(output[0], false);
    EXPECT_EQ(output[1], false);

    BatchNotEqualDecimal128(col1, 38, 2, col2, 38, 2, output, rowCnt);
    EXPECT_EQ(output[0], true);
    EXPECT_EQ(output[1], true);
}

TEST(BatchFunctionTest, CastDecimalToDecimal)
{
    int32_t rowCnt = 2;
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);

    int64_t decimal64Val[2] = { 1234567L, INT64_MAX };
    Decimal128 decimal128Val[2];
    decimal128Val[0].SetValue(0, 1234567);
    decimal128Val[1].SetValue(~0, -123456);
    bool isAnyNull[2] = { false, false };
    bool overflowNull[2] = { false, false };
    int64_t output64[2];
    Decimal128 output128[2];

    BatchCastDecimal64To64(contextPtr, decimal64Val, 7, 2, isAnyNull, output64, 10, 3, rowCnt);
    EXPECT_EQ(output64[0], 12345670L);
    EXPECT_TRUE(context->HasError());

    BatchCastDecimal64To64RetNull(overflowNull, decimal64Val, 7, 2, output64, 10, 3, rowCnt);
    EXPECT_EQ(output64[0], 12345670L);
    EXPECT_FALSE(overflowNull[0]);
    EXPECT_TRUE(overflowNull[1]);

    BatchCastDecimal128To128(contextPtr, decimal128Val, 20, 10, isAnyNull, output128, 38, 18, rowCnt);
    EXPECT_EQ(output128[0].HighBits(), 0);
    EXPECT_EQ(output128[0].LowBits(), 123456700000000L);
    EXPECT_EQ(output128[1].HighBits(), ~0);
    EXPECT_EQ(output128[1].LowBits(), -12345600000000L);

    overflowNull[0] = false;
    overflowNull[1] = false;
    BatchCastDecimal128To128RetNull(overflowNull, decimal128Val, 20, 10, output128, 38, 18, rowCnt);
    EXPECT_FALSE(overflowNull[0]);
    EXPECT_FALSE(overflowNull[1]);

    BatchCastDecimal64To128(contextPtr, decimal64Val, 18, 18, isAnyNull, output128, 38, 20, rowCnt);
    EXPECT_EQ(output128[0].HighBits(), 0);
    EXPECT_EQ(output128[0].LowBits(), 123456700L);
    EXPECT_EQ(output128[1].HighBits(), 49);
    EXPECT_EQ(output128[1].LowBits(), 18446744073709551516UL);

    BatchCastDecimal64To128RetNull(overflowNull, decimal64Val, 18, 18, output128, 38, 15, rowCnt);
    EXPECT_EQ(output128[0].HighBits(), 0);
    EXPECT_EQ(output128[0].LowBits(), 1235);
    EXPECT_EQ(output128[1].HighBits(), 0);
    EXPECT_EQ(output128[1].LowBits(), std::round(INT64_MAX / 1000));

    BatchCastDecimal128To64(contextPtr, decimal128Val, 38, 10, isAnyNull, output64, 18, 9, rowCnt);
    EXPECT_EQ(output64[0], 123457);
    EXPECT_EQ(output64[1], -12346);

    overflowNull[0] = false;
    overflowNull[1] = false;
    BatchCastDecimal128To64RetNull(overflowNull, decimal128Val, 38, 3, output64, 5, 2, rowCnt);
    EXPECT_TRUE(overflowNull[0]);
    EXPECT_EQ(output64[1], -12346);

    delete context;
}

TEST(BatchFunctionTest, CastBasicTypeToDecimal)
{
    int32_t rowCnt = 1;
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);
    int32_t intVal[1] = { 1234567 };
    int64_t longVal[1] = { INT32_MAX };
    double doubleVal[1] = { 99999999.99 };
    bool isAnyNull[1] = { false };
    bool overflowNull[1] = { false };
    int64_t output64[1];
    Decimal128 output128[1];

    BatchCastIntToDecimal64(contextPtr, intVal, isAnyNull, output64, 9, 2, rowCnt);
    EXPECT_EQ(output64[0], 123456700);
    BatchCastIntToDecimal128(contextPtr, intVal, isAnyNull, output128, 38, 9, rowCnt);
    EXPECT_EQ(output128[0].HighBits(), 0);
    EXPECT_EQ(output128[0].LowBits(), 1'234'567'000'000'000);
    BatchCastIntToDecimal64RetNull(overflowNull, intVal, output64, 6, 0, rowCnt);
    EXPECT_TRUE(overflowNull[0]);
    overflowNull[0] = false;
    BatchCastIntToDecimal128RetNull(overflowNull, intVal, output128, 18, 12, rowCnt);
    EXPECT_TRUE(overflowNull[0]);

    overflowNull[0] = false;
    BatchCastLongToDecimal64(contextPtr, longVal, isAnyNull, output64, 13, 2, rowCnt);
    EXPECT_EQ(output64[0], INT_MAX * 100L);
    BatchCastLongToDecimal128(contextPtr, longVal, isAnyNull, output128, 38, 5, rowCnt);
    EXPECT_EQ(output128[0].HighBits(), 0);
    EXPECT_EQ(output128[0].LowBits(), INT_MAX * 100'000L);
    BatchCastLongToDecimal64RetNull(overflowNull, longVal, output64, 9, 0, rowCnt);
    EXPECT_TRUE(overflowNull[0]);
    BatchCastLongToDecimal128RetNull(overflowNull, longVal, output128, 38, 6, rowCnt);
    EXPECT_EQ(output128[0].HighBits(), 0);
    EXPECT_EQ(output128[0].LowBits(), INT_MAX * 1000'000L);

    overflowNull[0] = false;
    BatchCastDoubleToDecimal64(contextPtr, doubleVal, isAnyNull, output64, 13, 2, rowCnt);
    EXPECT_EQ(output64[0], 9'999'999'999L);
    BatchCastDoubleToDecimal128(contextPtr, doubleVal, isAnyNull, output128, 38, 5, rowCnt);
    EXPECT_EQ(output128[0].HighBits(), 0);
    EXPECT_EQ(output128[0].LowBits(), 9'999'999'999L * 1000L);
    BatchCastDoubleToDecimal64RetNull(overflowNull, doubleVal, output64, 12, 0, rowCnt);
    EXPECT_EQ(output64[0], 100'000'000L);
    BatchCastDoubleToDecimal128RetNull(overflowNull, doubleVal, output128, 19, 18, rowCnt);
    EXPECT_TRUE(overflowNull[0]);

    delete context;
}

// Return null function only adapt to Truncation Rule. Because only spark call this function.
TEST(BatchFunctionTest, CastDecimalToBasicType)
{
    int32_t rowCnt = 1;
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);
    int64_t decimal64Val[1] = { 1234567 };
    Decimal128 decimal128Val[1];
    decimal128Val[0].SetValue(0, INT64_MAX);
    int32_t outputInt[1];
    int64_t outputLong[1];
    double outputDouble[1];
    bool isAnyNull[1] = { false };
    bool overflowNull[1] = { false };

    BatchCastDecimal64ToIntHalfUp(contextPtr, decimal64Val, 7, 2, isAnyNull, outputInt, rowCnt);
    EXPECT_EQ(outputInt[0], 12346);
    BatchCastDecimal64ToLongHalfUp(decimal64Val, 7, 2, isAnyNull, outputLong, rowCnt);
    EXPECT_EQ(outputLong[0], 12346L);
    BatchCastDecimal64ToDoubleHalfUp(decimal64Val, 7, 2, isAnyNull, outputDouble, rowCnt);
    EXPECT_EQ(outputDouble[0], 12345.67);

    BatchCastDecimal128ToInt(contextPtr, decimal128Val, 19, 2, isAnyNull, outputInt, rowCnt);
    EXPECT_TRUE(context->HasError());
    BatchCastDecimal128ToLong(contextPtr, decimal128Val, 19, 2, isAnyNull, outputLong, rowCnt);
    EXPECT_EQ(outputLong[0], INT64_MAX / 100L);
    BatchCastDecimal128ToDoubleHalfUp(decimal128Val, 19, 2, isAnyNull, outputDouble, rowCnt);
    EXPECT_EQ(outputDouble[0], INT64_MAX / (double)100);

    BatchCastDecimal64ToIntRetNull(overflowNull, decimal64Val, 7, 2, outputInt, rowCnt);
    EXPECT_EQ(outputInt[0], 12345);
    BatchCastDecimal64ToLongRetNull(overflowNull, decimal64Val, 7, 2, outputLong, rowCnt);
    EXPECT_EQ(outputLong[0], 12345L);
    BatchCastDecimal64ToDoubleRetNull(overflowNull, decimal64Val, 7, 2, outputDouble, rowCnt);
    EXPECT_EQ(outputDouble[0], 12345.67);

    BatchCastDecimal128ToIntRetNull(overflowNull, decimal128Val, 7, 2, outputInt, rowCnt);
    EXPECT_EQ(outputInt[0], static_cast<int32_t>(INT64_MAX / 100L));
    BatchCastDecimal128ToLongRetNull(overflowNull, decimal128Val, 7, 2, outputLong, rowCnt);
    EXPECT_EQ(outputLong[0], INT64_MAX / 100L);
    BatchCastDecimal128ToDoubleRetNull(overflowNull, decimal128Val, 7, 2, outputDouble, rowCnt);
    EXPECT_EQ(outputDouble[0], INT64_MAX / (double)100);

    delete context;
}

TEST(BatchFunctionTest, MakeDecimal)
{
    int32_t rowCnt = 1;
    auto context = new ExecutionContext();
    int64_t contextPtr = reinterpret_cast<int64_t>(context);
    int64_t col[1] = { 12345678L };
    int64_t output[1];
    bool isAnyNull[1] = { false };
    bool overflowNull[1] = { false };

    BatchUnscaledValue64(col, 7, 2, isAnyNull, output, rowCnt);
    EXPECT_EQ(output[0], 12345678L);

    BatchMakeDecimal64(contextPtr, col, isAnyNull, output, 7, 2, rowCnt);
    EXPECT_TRUE(context->HasError());

    BatchMakeDecimal64RetNull(overflowNull, col, output, 7, 2, rowCnt);
    EXPECT_TRUE(overflowNull[0]);

    delete context;
}

TEST(BatchFunctionTest, RoundDecimal)
{
    int32_t rowCnt = 2;
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);
    bool isNull[2] = {false, true};
    int32_t round[2] = {0, 0};
    Decimal128 output128[2];
    int64_t output64[2];

    Decimal128 decimal128Val1[2];
    decimal128Val1[0] = Decimal128("22167875302138684366688952930001319804");
    decimal128Val1[1] = Decimal128("151949737170315874258868924986217725952");
    BatchRoundDecimal128(contextPtr, decimal128Val1, 38, 18, round, isNull, output128, 21, 0, rowCnt);
    EXPECT_EQ(output128[0].HighBits(), 1);
    EXPECT_EQ(output128[0].LowBits(), 3721131228429132751L);
    EXPECT_EQ(output128[1].HighBits(), 0);
    EXPECT_EQ(output128[1].LowBits(), 0);

    Decimal128 decimal128Val2[2];
    decimal128Val2[0] = Decimal128("22167875302138684366688952930001319804");
    decimal128Val2[1] = Decimal128("151949737170315874258868924986217725952");
    BatchRoundDecimal128WithoutRound(contextPtr, decimal128Val2, 38, 18, isNull, output128, 21, 0, rowCnt);
    EXPECT_EQ(output128[0].HighBits(), 1);
    EXPECT_EQ(output128[0].LowBits(), 3721131228429132751L);
    EXPECT_EQ(output128[1].HighBits(), 0);
    EXPECT_EQ(output128[1].LowBits(), 0);

    int64_t decimal64Val1[2] = { 463127424592162661L, 63636006298474304L};
    output64[1] = 0;
    BatchRoundDecimal64(contextPtr, decimal64Val1, 18, 8, round, isNull, output64, 11, 0, rowCnt);
    EXPECT_EQ(output64[0], 4631274246L);
    EXPECT_EQ(output64[1], 0);

    int64_t decimal64Val2[2] = { 463127424592162661L, 63636006298474304L};
    BatchRoundDecimal64WithoutRound(contextPtr, decimal64Val2, 18, 8, isNull, output64, 11, 0, rowCnt);
    EXPECT_EQ(output64[0], 4631274246L);
    EXPECT_EQ(output64[1], 0);
    delete context;
}

TEST(BatchFunctionTest, DecimalAdd)
{
    int32_t rowCnt = 2;
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);
    bool isAnyNull[2] = { false, false };
    Decimal128 output128[2];

    int64_t decimal64Val1[2] = { 1234567, 98765 };
    int64_t decimal64Val2[2] = { 9999999, -1111111 };
    BatchAddDec64Dec64Dec64ReScale(contextPtr, isAnyNull, decimal64Val1, 7, 2, decimal64Val2, 7, 2, 9, 2, rowCnt);
    EXPECT_EQ(decimal64Val1[0], 11234566);
    EXPECT_EQ(decimal64Val1[1], -1012346);

    BatchAddDec64Dec64Dec128ReScale(contextPtr, isAnyNull, decimal64Val1, 9, 2, decimal64Val2, 7, 2, output128, 19, 9,
        rowCnt);
    EXPECT_EQ(output128[0].HighBits(), 0);
    EXPECT_EQ(output128[0].LowBits(), 212345650000000);
    EXPECT_EQ(output128[1].HighBits(), ~0);
    EXPECT_EQ(output128[1].LowBits(), -21234570000000);

    Decimal128 decimal128Val1[2];
    decimal128Val1[0].SetValue(0, INT64_MAX);
    decimal128Val1[1].SetValue(0, 99'999'999'999'999'999UL);
    Decimal128 decimal128Val2[2];
    decimal128Val2[0].SetValue(0, 123456789);
    decimal128Val2[1].SetValue(~0, -987654321);
    BatchAddDec128Dec128Dec128ReScale(contextPtr, isAnyNull, decimal128Val1, 19, 9, decimal128Val2, 19, 9, 38, 9,
        rowCnt);
    EXPECT_EQ(decimal128Val1[0].HighBits(), 0);
    EXPECT_EQ(decimal128Val1[0].LowBits(), 9'223'372'036'978'232'596UL);
    EXPECT_EQ(decimal128Val1[1].HighBits(), 0);
    EXPECT_EQ(decimal128Val1[1].LowBits(), 99'999'999'012'345'678UL);

    BatchAddDec64Dec128Dec128ReScale(contextPtr, isAnyNull, decimal64Val1, 8, 2, decimal128Val1, 38, 9, 38, 9, rowCnt);
    EXPECT_EQ(decimal128Val1[0].HighBits(), 0);
    EXPECT_EQ(decimal128Val1[0].LowBits(), 9'223'484'382'638'232'596UL);
    EXPECT_EQ(decimal128Val1[1].HighBits(), 0);
    EXPECT_EQ(decimal128Val1[1].LowBits(), 99'989'875'552'345'678UL);

    decimal64Val1[0] = 823'484'382'638'232'596L;
    BatchAddDec128Dec64Dec128ReScale(contextPtr, isAnyNull, decimal128Val1, 19, 9, decimal64Val1, 18, 9, 19, 19,
        rowCnt);
    EXPECT_TRUE(context->HasError());

    delete context;
}

TEST(BatchFunctionTest, DecimalAddRetNull)
{
    int32_t rowCnt = 2;
    bool overflowNull[2] = { false };
    Decimal128 output128[2];

    int64_t decimal64Val1[2] = { 1234567, 98765 };
    int64_t decimal64Val2[2] = { 9999999, -1111111 };
    BatchAddDec64Dec64Dec64RetNull(overflowNull, decimal64Val1, 7, 2, decimal64Val2, 7, 2, 9, 2, rowCnt);
    EXPECT_FALSE(overflowNull[0]);
    EXPECT_EQ(decimal64Val1[0], 11234566);
    EXPECT_FALSE(overflowNull[1]);
    EXPECT_EQ(decimal64Val1[1], -1012346);

    BatchAddDec64Dec64Dec128RetNull(overflowNull, decimal64Val1, 8, 2, decimal64Val2, 7, 2, output128, 19, 9, rowCnt);
    EXPECT_FALSE(overflowNull[0]);
    EXPECT_EQ(output128[0].HighBits(), 0);
    EXPECT_EQ(output128[0].LowBits(), 212345650000000);
    EXPECT_FALSE(overflowNull[1]);
    EXPECT_EQ(output128[1].HighBits(), ~0);
    EXPECT_EQ(output128[1].LowBits(), -21234570000000);

    Decimal128 decimal128Val1[2];
    decimal128Val1[0] = Decimal128("-9223372036854775807");
    decimal128Val1[1] = Decimal128("99999999999999999");
    Decimal128 decimal128Val2[2];
    decimal128Val2[0] = Decimal128("123456789");
    decimal128Val2[1] = Decimal128("-987654321");
    BatchAddDec128Dec128Dec128RetNull(overflowNull, decimal128Val1, 19, 9, decimal128Val2, 19, 9, 38, 19, rowCnt);
    EXPECT_FALSE(overflowNull[0]);
    EXPECT_EQ(decimal128Val1[0], Decimal128("-92233720367313190180000000000"));
    EXPECT_FALSE(overflowNull[1]);
    EXPECT_EQ(decimal128Val1[1], Decimal128("999999990123456780000000000"));

    BatchAddDec64Dec128Dec128RetNull(overflowNull, decimal64Val1, 9, 2, decimal128Val1, 30, 19, 30, 21, rowCnt);
    EXPECT_TRUE(overflowNull[0]);
    EXPECT_FALSE(overflowNull[1]);
    EXPECT_EQ(decimal128Val1[1].HighBits(), 0x1'4315'AFBF);
    EXPECT_EQ(decimal128Val1[1].LowBits(), 0x53A'4894'CE2A'E000);

    decimal64Val1[0] = 823'484'382'638'232'596L;
    decimal64Val1[1] = 999'999'999'999'999'999L;
    decimal128Val1[0].SetValue(0, 9'223'484'382'638'232'596UL);
    BatchAddDec128Dec64Dec128RetNull(overflowNull, decimal128Val1, 19, 9, decimal64Val1, 18, 9, 19, 19, rowCnt);
    EXPECT_TRUE(overflowNull[0]);
    EXPECT_TRUE(overflowNull[1]);
}

TEST(BatchFunctionTest, DecimalSubtract)
{
    int32_t rowCnt = 2;
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);
    bool isAnyNull[2] = { false, false };
    Decimal128 output128[2];

    int64_t decimal64Val1[2] = { 1234567, -34567 };
    int64_t decimal64Val2[2] = { 9999999, 1111111 };
    BatchSubDec64Dec64Dec64ReScale(contextPtr, isAnyNull, decimal64Val1, 7, 2, decimal64Val2, 7, 2, 9, 2, rowCnt);
    EXPECT_EQ(decimal64Val1[0], -8765432);
    EXPECT_EQ(decimal64Val1[1], -1145678);

    BatchSubDec64Dec64Dec128ReScale(contextPtr, isAnyNull, decimal64Val1, 9, 2, decimal64Val2, 7, 2, output128, 19, 9,
        rowCnt);
    EXPECT_EQ(output128[0].HighBits(), ~0);
    EXPECT_EQ(output128[0].LowBits(), -187'654'310'000'000L);
    EXPECT_EQ(output128[1].HighBits(), ~0);
    EXPECT_EQ(output128[1].LowBits(), -0x1486'7F11'1880);

    Decimal128 decimal128Val1[2];
    decimal128Val1[0].SetValue(0, INT64_MAX);
    decimal128Val1[1].SetValue(1234, 9'999'000'000);
    Decimal128 decimal128Val2[2];
    decimal128Val2[0].SetValue(0, 100'000);
    decimal128Val2[1].SetValue(~1234, -987654321);
    BatchSubDec128Dec128Dec128ReScale(contextPtr, isAnyNull, decimal128Val1, 19, 9, decimal128Val2, 30, 9, 38, 9,
        rowCnt);
    EXPECT_EQ(decimal128Val1[0].HighBits(), 0);
    EXPECT_EQ(decimal128Val1[0].LowBits(), INT64_MAX - 100'000);
    EXPECT_EQ(decimal128Val1[1].HighBits(), 0x9A4);
    EXPECT_EQ(decimal128Val1[1].LowBits(), 0x0000'0002'8EDB'0A71);

    BatchSubDec64Dec128Dec128ReScale(contextPtr, isAnyNull, decimal64Val1, 18, 9, decimal128Val1, 38, 9, 19, 12,
        rowCnt);
    EXPECT_TRUE(context->HasError());

    context->ResetError();
    decimal64Val1[0] = 823'484'382'638'232'596L;
    decimal128Val1[0].SetValue(~0, -INT64_MAX);
    BatchSubDec128Dec64Dec128ReScale(contextPtr, isAnyNull, decimal128Val1, 19, 9, decimal64Val1, 18, 9, 19, 19,
        rowCnt);
    EXPECT_TRUE(context->HasError());

    delete context;
}

TEST(BatchFunctionTest, DecimalSubtractRetNull)
{
    int32_t rowCnt = 2;
    bool overflowNull[2] = { false, false };
    Decimal128 output128[2];

    int64_t decimal64Val1[2] = { 1234567, -34567 };
    int64_t decimal64Val2[2] = { 9999999, 1111111 };
    BatchSubDec64Dec64Dec64RetNull(overflowNull, decimal64Val1, 7, 2, decimal64Val2, 7, 2, 9, 2, rowCnt);
    EXPECT_FALSE(overflowNull[0]);
    EXPECT_EQ(decimal64Val1[0], -8765432);
    EXPECT_FALSE(overflowNull[1]);
    EXPECT_EQ(decimal64Val1[1], -1145678);

    BatchSubDec64Dec64Dec128RetNull(overflowNull, decimal64Val1, 8, 2, decimal64Val2, 7, 2, output128, 19, 9, rowCnt);
    EXPECT_FALSE(overflowNull[0]);
    EXPECT_EQ(output128[0].HighBits(), ~0);
    EXPECT_EQ(output128[0].LowBits(), -187'654'310'000'000UL);
    EXPECT_FALSE(overflowNull[1]);
    EXPECT_EQ(output128[1].HighBits(), ~0);
    EXPECT_EQ(output128[1].LowBits(), -0x1486'7F11'1880);

    Decimal128 decimal128Val1[2];
    decimal128Val1[0].SetValue(~0, -INT64_MAX);
    decimal128Val1[1].SetValue(~0, -1);
    Decimal128 decimal128Val2[2];
    decimal128Val2[0].SetValue(0, 100'000);
    decimal128Val2[1].SetValue(~1234, -100'000);
    BatchSubDec128Dec128Dec128RetNull(overflowNull, decimal128Val1, 19, 9, decimal128Val2, 19, 9, 38, 9, rowCnt);
    EXPECT_FALSE(overflowNull[0]);
    EXPECT_EQ(decimal128Val1[0].HighBits(), ~0);
    EXPECT_EQ(decimal128Val1[0].LowBits(), -9223372036854775807UL - 100'000UL);
    EXPECT_FALSE(overflowNull[1]);
    EXPECT_EQ(decimal128Val1[1].HighBits(), 1234);
    EXPECT_EQ(decimal128Val1[1].LowBits(), 99999);

    BatchSubDec64Dec128Dec128RetNull(overflowNull, decimal64Val1, 9, 2, decimal128Val1, 38, 9, 19, 12, rowCnt);
    EXPECT_TRUE(overflowNull[0]);
    EXPECT_TRUE(overflowNull[1]);

    decimal64Val1[0] = 823'484'382'638'232'596L;
    decimal64Val1[1] = 823'484'382'638'232'596L;
    decimal128Val1[0].SetValue(0, 9'223'484'382'638'232'596UL);
    BatchSubDec128Dec64Dec128RetNull(overflowNull, decimal128Val1, 19, 9, decimal64Val1, 18, 9, 19, 19, rowCnt);
    EXPECT_TRUE(overflowNull[0]);
    EXPECT_TRUE(overflowNull[1]);
}

TEST(BatchFunctionTest, DecimalMultiply)
{
    int32_t rowCnt = 2;
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);
    bool isAnyNull[2] = { false, false };
    Decimal128 output128[2];

    int64_t decimal64Val1[2] = { 1234567, -987654 };
    int64_t decimal64Val2[2] = { 9999999, 1010101 };
    BatchMulDec64Dec64Dec64ReScale(contextPtr, isAnyNull, decimal64Val1, 7, 2, decimal64Val2, 7, 2, 14, 2, rowCnt);
    EXPECT_EQ(decimal64Val1[0], 123'456'687'654L);
    EXPECT_EQ(decimal64Val1[1], -9'976'302'931L);

    BatchMulDec64Dec64Dec128ReScale(contextPtr, isAnyNull, decimal64Val1, 12, 2, decimal64Val2, 7, 2, output128, 19, 4,
        rowCnt);
    EXPECT_EQ(output128[0].HighBits(), 0);
    EXPECT_EQ(output128[0].LowBits(), 123'456'687'654L * 9999999L);
    EXPECT_EQ(output128[1].HighBits(), ~0);
    EXPECT_EQ(output128[1].LowBits(), -9'976'302'931L * 1010101);

    Decimal128 decimal128Val1[2];
    decimal128Val1[0].SetValue(0, 123456);
    decimal128Val1[1].SetValue(~0, -999999);
    Decimal128 decimal128Val2[2];
    decimal128Val2[0].SetValue(0, 10000L);
    decimal128Val2[1].SetValue(0, 314159L);
    BatchMulDec128Dec128Dec128ReScale(contextPtr, isAnyNull, decimal128Val1, 20, 9, decimal128Val2, 19, 0, 38, 9,
        rowCnt);
    EXPECT_EQ(decimal128Val1[0].HighBits(), 0);
    EXPECT_EQ(decimal128Val1[0].LowBits(), 1234560000);
    EXPECT_EQ(decimal128Val1[1].HighBits(), ~0);
    EXPECT_EQ(decimal128Val1[1].LowBits(), -314158685841);

    decimal128Val1[0].SetValue(INT64_MAX, INT64_MAX);
    BatchMulDec64Dec128Dec128ReScale(contextPtr, isAnyNull, decimal64Val1, 8, 2, decimal128Val1, 38, 9, 38, 9, rowCnt);
    EXPECT_TRUE(context->HasError());

    context->ResetError();
    decimal64Val1[0] = 123456789;
    decimal64Val1[1] = 314159;
    decimal128Val1[0].SetValue(0, 823'484'382'638'232'596UL);
    decimal128Val1[1].SetValue(~123, -1);
    BatchMulDec128Dec64Dec128ReScale(contextPtr, isAnyNull, decimal128Val1, 19, 9, decimal64Val1, 18, 9, 38, 18,
        rowCnt);
    EXPECT_EQ(decimal128Val1[0].HighBits(), 0x541858);
    EXPECT_EQ(decimal128Val1[0].LowBits(), 0x78F3'8F5D'A9D8'69A4);
    EXPECT_EQ(decimal128Val1[1].HighBits(), ~0x24D'9F95);
    EXPECT_EQ(decimal128Val1[1].LowBits(), -0x4'CB2F);

    delete context;
}

TEST(BatchFunctionTest, DecimalMultiplyRetNull)
{
    int32_t rowCnt = 2;
    bool overflowNull[2] = { false, false };
    Decimal128 output128[2];

    int64_t decimal64Val1[2] = { -1234567, -314159 };
    int64_t decimal64Val2[2] = { 9999999, 65432 };
    BatchMulDec64Dec64Dec64RetNull(overflowNull, decimal64Val1, 7, 2, decimal64Val2, 7, 2, 14, 2, rowCnt);
    EXPECT_FALSE(overflowNull[0]);
    EXPECT_EQ(decimal64Val1[0], -123'456'687'654L);
    EXPECT_FALSE(overflowNull[1]);
    EXPECT_EQ(decimal64Val1[1], -205'560'517L);

    BatchMulDec64Dec64Dec128RetNull(overflowNull, decimal64Val1, 12, 2, decimal64Val2, 7, 2, output128, 19, 4, rowCnt);
    EXPECT_FALSE(overflowNull[0]);
    EXPECT_EQ(output128[0].HighBits(), ~0);
    EXPECT_EQ(output128[0].LowBits(), -123'456'687'654L * 9999999);
    EXPECT_FALSE(overflowNull[1]);
    EXPECT_EQ(output128[1].HighBits(), ~0);
    EXPECT_EQ(output128[1].LowBits(), -13'450'235'748'344);

    Decimal128 decimal128Val1[2];
    decimal128Val1[0].SetValue(0, INT64_MAX);
    decimal128Val1[1].SetValue(~1, -9999999);
    Decimal128 decimal128Val2[2];
    decimal128Val2[0].SetValue(0, 100);
    decimal128Val2[1].SetValue(0, 31415926);
    BatchMulDec128Dec128Dec128RetNull(overflowNull, decimal128Val1, 19, 9, decimal128Val2, 19, 0, 38, 9, rowCnt);
    EXPECT_FALSE(overflowNull[0]);
    EXPECT_EQ(decimal128Val1[0].HighBits(), 49);
    EXPECT_EQ(decimal128Val1[0].LowBits(), 0xFFFFFFFFFFFFFF9C);
    EXPECT_FALSE(overflowNull[1]);
    EXPECT_EQ(decimal128Val1[1].HighBits(), ~0x1DF'5E76);
    EXPECT_EQ(decimal128Val1[1].LowBits(), -0x0001'1DB9'E539'008A);

    BatchMulDec64Dec128Dec128RetNull(overflowNull, decimal64Val1, 9, 2, decimal128Val1, 38, 9, 19, 12, rowCnt);
    EXPECT_TRUE(overflowNull[0]);
    EXPECT_TRUE(overflowNull[1]);

    decimal64Val1[0] = 823'484'382'638'232'596L;
    decimal128Val1[0].SetValue(0, 9'223'484'382'638'232'596UL);
    BatchMulDec128Dec64Dec128RetNull(overflowNull, decimal128Val1, 19, 9, decimal64Val1, 18, 9, 19, 19, rowCnt);
    EXPECT_TRUE(overflowNull[0]);
    EXPECT_TRUE(overflowNull[1]);
}

TEST(BatchFunctionTest, DecimalDivide)
{
    int32_t rowCnt = 2;
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);
    bool isAnyNull[2] = { false, false };
    Decimal128 output128[2];

    int64_t decimal64Val1[2] = { -1234567, 9999999 };
    int64_t decimal64Val2[2] = { 100000, 33333 };
    BatchDivDec64Dec64Dec64ReScale(contextPtr, isAnyNull, decimal64Val1, 7, 2, decimal64Val2, 7, 2, 14, 2, rowCnt);
    EXPECT_EQ(decimal64Val1[0], -1235);
    EXPECT_EQ(decimal64Val1[1], 30000);

    Decimal128 decimal128Val1[2];
    decimal128Val1[0].SetValue(0, 100000);
    decimal128Val1[1].SetValue(~0, -999);
    BatchDivDec64Dec128Dec64ReScale(contextPtr, isAnyNull, decimal64Val1, 7, 2, decimal128Val1, 19, 2, 14, 2, rowCnt);
    EXPECT_EQ(decimal64Val1[0], -1);
    EXPECT_EQ(decimal64Val1[1], -3003);

    decimal128Val1[0].SetValue(~0, -INT64_MAX);
    decimal128Val1[1].SetValue(0, 1234567890);
    BatchDivDec128Dec64Dec64ReScale(contextPtr, isAnyNull, decimal128Val1, 20, 6, decimal64Val2, 7, 2, 18, 8, rowCnt);
    EXPECT_EQ(decimal64Val2[0], -922337203685477581);
    EXPECT_EQ(decimal64Val2[1], 370374071);

    BatchDivDec64Dec64Dec128ReScale(contextPtr, isAnyNull, decimal64Val1, 7, 2, decimal64Val2, 18, 8, output128, 20, 2,
        rowCnt);
    EXPECT_EQ(output128[0].HighBits(), 0);
    EXPECT_EQ(output128[0].LowBits(), 0);
    EXPECT_EQ(output128[1].HighBits(), ~0);
    EXPECT_EQ(output128[1].LowBits(), -811);

    Decimal128 decimal128Val2[2];
    decimal128Val2[0].SetValue(0, 1);
    decimal128Val2[1].SetValue(123, 456);
    BatchDivDec128Dec128Dec128ReScale(contextPtr, isAnyNull, decimal128Val1, 19, 2, decimal128Val2, 19, 2, 38, 6,
        rowCnt);
    EXPECT_EQ(decimal128Val1[0].HighBits(), ~0x7A11F);
    EXPECT_EQ(decimal128Val1[0].LowBits(), -0xFFFF'FFFF'FFF0'BDC0);
    EXPECT_EQ(decimal128Val1[1].HighBits(), 0);
    EXPECT_EQ(decimal128Val1[1].LowBits(), 0);

    decimal128Val1[0].SetValue(0, 1111111);
    decimal128Val1[1].SetValue(0, 31415);
    BatchDivDec64Dec128Dec128ReScale(contextPtr, isAnyNull, decimal64Val2, 18, 2, decimal128Val1, 19, 2, 19, 6, rowCnt);
    EXPECT_EQ(decimal128Val1[0].HighBits(), ~0);
    EXPECT_EQ(decimal128Val1[0].LowBits(), -0x0B85'1ECB'A5B9'0AB8);
    EXPECT_EQ(decimal128Val1[1].HighBits(), 0);
    EXPECT_EQ(decimal128Val1[1].LowBits(), 11789720548);

    decimal64Val1[0] = 0;
    BatchDivDec128Dec64Dec128ReScale(contextPtr, isAnyNull, decimal128Val1, 38, 2, decimal64Val1, 7, 2, 38, 2, rowCnt);
    EXPECT_TRUE(context->HasError());

    delete context;
}

TEST(BatchFunctionTest, DecimalDivideRetNull)
{
    int32_t rowCnt = 2;
    bool overflowNull[2] = { false, false };
    Decimal128 output128[2];

    int64_t decimal64Val1[2] = { -9999999, 3141592 };
    int64_t decimal64Val2[2] = { 1234567, 1010 };
    BatchDivDec64Dec64Dec64RetNull(overflowNull, decimal64Val1, 7, 2, decimal64Val2, 7, 2, 14, 2, rowCnt);
    EXPECT_FALSE(overflowNull[0]);
    EXPECT_EQ(decimal64Val1[0], -810);
    EXPECT_FALSE(overflowNull[1]);
    EXPECT_EQ(decimal64Val1[1], 311049);

    BatchDivDec64Dec64Dec128RetNull(overflowNull, decimal64Val1, 7, 0, decimal64Val2, 9, 6, output128, 19, 4, rowCnt);
    EXPECT_FALSE(overflowNull[0]);
    EXPECT_EQ(output128[0].HighBits(), ~0);
    EXPECT_EQ(output128[0].LowBits(), -6561005);
    EXPECT_FALSE(overflowNull[1]);
    EXPECT_EQ(output128[1].HighBits(), 0);
    EXPECT_EQ(output128[1].LowBits(), 3079693069307);

    decimal64Val1[0] = 123456789999;
    decimal64Val1[1] = 31415926;
    Decimal128 decimal128Val1[2];
    decimal128Val1[0].SetValue(0, 100000);
    decimal128Val1[1].SetValue(~0, -54321);
    BatchDivDec64Dec128Dec64RetNull(overflowNull, decimal64Val1, 12, 2, decimal128Val1, 38, 10, 16, 0, rowCnt);
    EXPECT_FALSE(overflowNull[0]);
    EXPECT_EQ(decimal64Val1[0], 123'456'789'999'000UL);
    EXPECT_FALSE(overflowNull[1]);
    EXPECT_EQ(decimal64Val1[1], -57'833'850'629UL);

    BatchDivDec128Dec64Dec64RetNull(overflowNull, decimal128Val1, 38, 0, decimal64Val2, 7, 2, 17, 4, rowCnt);
    EXPECT_FALSE(overflowNull[0]);
    EXPECT_EQ(decimal64Val2[0], 81000);
    EXPECT_FALSE(overflowNull[1]);
    EXPECT_EQ(decimal64Val2[1], -53783168);

    decimal128Val1[0].SetValue(1, UINT64_MAX);
    decimal128Val1[1].SetValue(0, 31415666);
    Decimal128 decimal128Val2[2];
    decimal128Val2[0].SetValue(0, 1);
    decimal128Val2[1].SetValue(~0, -31415);
    BatchDivDec128Dec128Dec128RetNull(overflowNull, decimal128Val1, 38, 0, decimal128Val2, 38, 19, 38, 0, rowCnt);
    EXPECT_TRUE(overflowNull[0]);
    EXPECT_FALSE(overflowNull[1]);
    EXPECT_EQ(decimal128Val1[1].HighBits(), ~0x21E);
    EXPECT_EQ(decimal128Val1[1].LowBits(), -0x1CD1'F72F'613F'51F0);

    overflowNull[0] = false;
    overflowNull[1] = false;
    decimal64Val1[0] = INT64_MAX;
    decimal64Val1[1] = INT64_MAX;
    BatchDivDec64Dec128Dec128RetNull(overflowNull, decimal64Val1, 18, 0, decimal128Val2, 38, 19, 38, 5, rowCnt);
    EXPECT_TRUE(overflowNull[0]);
    EXPECT_TRUE(overflowNull[1]);

    overflowNull[0] = false;
    overflowNull[1] = false;
    decimal64Val1[0] = 823'484'382'638'232'596L;
    decimal64Val1[1] = 100000L;
    decimal128Val1[0].SetValue(INT64_MAX, 9'223'484'382'638'232'596UL);
    decimal128Val1[1].SetValue(0, 31415926UL);
    BatchDivDec128Dec64Dec128RetNull(overflowNull, decimal128Val1, 19, 9, decimal64Val1, 18, 9, 38, 19, rowCnt);
    EXPECT_TRUE(overflowNull[0]);
    EXPECT_FALSE(overflowNull[1]);
    EXPECT_EQ(decimal128Val1[1].HighBits(), 0xAA);
    EXPECT_EQ(decimal128Val1[1].LowBits(), 0x4E5B'00D4'3BD9'8000);
}

TEST(BatchFunctionTest, DecimalModulus)
{
    int32_t rowCnt = 2;
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);
    bool isAnyNull[2] = { false, false };
    int64_t output64[2];

    int64_t decimal64Val1[2] = { -1234567, 3141592 };
    int64_t decimal64Val2[2] = { 100000, 10101 };
    BatchModDec64Dec64Dec64ReScale(contextPtr, isAnyNull, decimal64Val1, 7, 2, decimal64Val2, 7, 2, 14, 2, rowCnt);
    EXPECT_EQ(decimal64Val1[0], -34567);
    EXPECT_EQ(decimal64Val1[1], 181);

    Decimal128 decimal128Val1[2];
    decimal128Val1[0].SetValue(12, UINT64_MAX);
    decimal128Val1[1].SetValue(0, 9'999'999'999);
    Decimal128 decimal128Val2[2];
    decimal128Val2[0].SetValue(0, 1111111);
    decimal128Val2[1].SetValue(~0, -1111111);
    BatchModDec128Dec128Dec128ReScale(contextPtr, isAnyNull, decimal128Val1, 19, 2, decimal128Val2, 19, 2, 38, 2,
        rowCnt);
    EXPECT_EQ(decimal128Val1[0].HighBits(), 0);
    EXPECT_EQ(decimal128Val1[0].LowBits(), 531573);
    EXPECT_EQ(decimal128Val1[1].HighBits(), 0);
    EXPECT_EQ(decimal128Val1[1].LowBits(), 999);

    BatchModDec64Dec128Dec64ReScale(contextPtr, isAnyNull, decimal64Val1, 7, 2, decimal128Val1, 19, 10, 14, 10, rowCnt);
    EXPECT_EQ(decimal64Val1[0], -384925);
    EXPECT_EQ(decimal64Val1[1], 118);

    decimal128Val1[0].SetValue(0, 0);
    BatchModDec64Dec128Dec128ReScale(contextPtr, isAnyNull, decimal64Val1, 18, 2, decimal128Val1, 38, 2, 38, 2, rowCnt);
    EXPECT_TRUE(context->HasError());

    decimal128Val1[0].SetValue(~0, -INT64_MAX);
    decimal128Val1[1].SetValue(0, 31415927);
    BatchModDec128Dec64Dec64ReScale(contextPtr, isAnyNull, decimal128Val1, 20, 6, decimal64Val2, 7, 2, 18, 6, rowCnt);
    EXPECT_EQ(decimal64Val2[0], -854775807);
    EXPECT_EQ(decimal64Val2[1], 31415927);

    BatchModDec128Dec64Dec128ReScale(contextPtr, isAnyNull, decimal128Val1, 19, 2, decimal64Val2, 9, 2, 20, 2, rowCnt);
    EXPECT_EQ(decimal128Val1[0].HighBits(), ~0);
    EXPECT_EQ(decimal128Val1[0].LowBits(), -698836018);
    EXPECT_EQ(decimal128Val1[1].HighBits(), 0);
    EXPECT_EQ(decimal128Val1[1].LowBits(), 0);

    BatchModDec128Dec128Dec64ReScale(contextPtr, isAnyNull, decimal128Val1, 19, 2, decimal128Val2, 19, 2, output64, 18,
        2, rowCnt);
    EXPECT_EQ(output64[0], -1058310);
    EXPECT_EQ(output64[1], 0);

    delete context;
}

TEST(BatchFunctionTest, DecimalModulusRetNull)
{
    int32_t rowCnt = 2;
    bool overflowNull[2] = { false, false };
    int64_t output64[2];

    int64_t decimal64Val1[2] = { -1234567, 3141592 };
    int64_t decimal64Val2[2] = { 100000, 10101 };
    BatchModDec64Dec64Dec64RetNull(overflowNull, decimal64Val1, 7, 2, decimal64Val2, 7, 2, 14, 2, rowCnt);
    EXPECT_FALSE(overflowNull[0]);
    EXPECT_EQ(decimal64Val1[0], -34567);
    EXPECT_FALSE(overflowNull[1]);
    EXPECT_EQ(decimal64Val1[1], 181);

    Decimal128 decimal128Val1[2];
    decimal128Val1[0].SetValue(12, UINT64_MAX);
    decimal128Val1[1].SetValue(0, 9'999'999'999);
    Decimal128 decimal128Val2[2];
    decimal128Val2[0].SetValue(0, 1111111);
    decimal128Val2[1].SetValue(~0, -1111111);
    BatchModDec128Dec128Dec128RetNull(overflowNull, decimal128Val1, 19, 2, decimal128Val2, 19, 2, 38, 2, rowCnt);
    EXPECT_FALSE(overflowNull[0]);
    EXPECT_EQ(decimal128Val1[0].HighBits(), 0);
    EXPECT_EQ(decimal128Val1[0].LowBits(), 531573);
    EXPECT_FALSE(overflowNull[1]);
    EXPECT_EQ(decimal128Val1[1].HighBits(), 0);
    EXPECT_EQ(decimal128Val1[1].LowBits(), 999);

    BatchModDec64Dec128Dec64RetNull(overflowNull, decimal64Val1, 7, 2, decimal128Val1, 19, 10, 14, 10, rowCnt);
    EXPECT_FALSE(overflowNull[0]);
    EXPECT_EQ(decimal64Val1[0], -384925);
    EXPECT_FALSE(overflowNull[1]);
    EXPECT_EQ(decimal64Val1[1], 118);

    decimal128Val1[0].SetValue(0, 0);
    BatchModDec64Dec128Dec128RetNull(overflowNull, decimal64Val1, 18, 2, decimal128Val1, 38, 2, 38, 2, rowCnt);
    EXPECT_TRUE(overflowNull[0]);

    overflowNull[0] = false;
    overflowNull[1] = false;
    decimal128Val1[0].SetValue(~0, -INT64_MAX);
    decimal128Val1[1].SetValue(0, 31415927);
    BatchModDec128Dec64Dec64RetNull(overflowNull, decimal128Val1, 20, 6, decimal64Val2, 7, 2, 18, 6, rowCnt);
    EXPECT_FALSE(overflowNull[0]);
    EXPECT_EQ(decimal64Val2[0], -854775807);
    EXPECT_FALSE(overflowNull[1]);
    EXPECT_EQ(decimal64Val2[1], 31415927);

    BatchModDec128Dec64Dec128RetNull(overflowNull, decimal128Val1, 19, 2, decimal64Val2, 7, 2, 20, 2, rowCnt);
    EXPECT_FALSE(overflowNull[0]);
    EXPECT_EQ(decimal128Val1[0].HighBits(), ~0);
    EXPECT_EQ(decimal128Val1[0].LowBits(), -698836018);
    EXPECT_FALSE(overflowNull[1]);
    EXPECT_EQ(decimal128Val1[1].HighBits(), 0);
    EXPECT_EQ(decimal128Val1[1].LowBits(), 0);

    BatchModDec128Dec128Dec64RetNull(overflowNull, decimal128Val1, 19, 2, decimal128Val2, 19, 2, output64, 18, 2,
        rowCnt);
    EXPECT_FALSE(overflowNull[0]);
    EXPECT_EQ(output64[0], -1058310);
    EXPECT_FALSE(overflowNull[1]);
    EXPECT_EQ(output64[1], 0);
}

TEST(BatchFunctionTest, CountChar)
{
    std::vector<std::string> inputStr {"hello", "aAaA", "abcd", "hello", ""};
    std::vector<int32_t> inputLen {5, 4, 4, 5, 0};
    std::vector<std::string> targetStr {"l", "a", "e", "", "a"};
    int32_t rowCnt = inputStr.size();
    int64_t output[rowCnt];
    std::vector<uint8_t *> strAddr(rowCnt);
    std::vector<uint8_t *> targetAddr(rowCnt);
    int32_t targetWidth = 10;
    std::vector<int32_t> targetLen(rowCnt);
    for(int32_t i = 0; i < rowCnt; i++) {
        strAddr[i] = reinterpret_cast<uint8_t *>(const_cast<char *>(inputStr[i].c_str()));
        targetAddr[i] = reinterpret_cast<uint8_t *>(const_cast<char *>(targetStr[i].c_str()));
        targetLen[i] = static_cast<int32_t>(targetStr[i].length());
    }

    bool isAnyNull[] = { false, false, false, false, false};
    BatchCountChar(strAddr.data(), inputLen.data(), targetAddr.data(), targetWidth, targetLen.data(), isAnyNull, output, rowCnt);
    std::vector<int32_t> result(output, output + rowCnt);
    std::vector<int32_t> expected { 2, 2, 0, 0, 0 };
    AssertIntEquals(expected, result);
}

TEST(BatchFunctionTest, SpiltIndex)
{
    std::vector<const char *> inputStr {"Jack,John,Mary", "Jack,Johnny,Mary", "Jack,John,Mary", "Jack,John,Mary",
                                        nullptr, "Jack,John,Mary", "Jack,John,MaryPaul,Nathan"};
    std::vector<int32_t> inputLen {14, 16, 14, 14, 0, 14, 14};
    std::vector<const char *> targetStr {",", ",", ",", ",", ",", nullptr, ","};
    int32_t index[] = {2, 1, -1, 3, 1, 1, 2};
    int32_t rowCnt = inputStr.size();
    int32_t targetWidth = 1;
    std::vector<int32_t> targetLen {1, 1, 1, 1, 1, 0, 1};
    uint8_t* output[rowCnt];
    int32_t outLen[rowCnt];
    std::vector<uint8_t *> strAddr(rowCnt);
    std::vector<uint8_t *> targetAddr(rowCnt);

    for(int32_t i = 0; i < rowCnt; i++) {
        strAddr[i] = reinterpret_cast<uint8_t *>(const_cast<char *>(inputStr[i]));
        targetAddr[i] = reinterpret_cast<uint8_t *>(const_cast<char *>(targetStr[i]));
    }

    bool isAnyNull[] = { false, false, false, false, true, true, false};
    BatchSplitIndex(strAddr.data(), inputLen.data(), targetAddr.data(), targetWidth, targetLen.data(),
                    index, isAnyNull, output, outLen, rowCnt);

    EXPECT_EQ("Mary", std::string(reinterpret_cast<char *>(output[0]), outLen[0]));
    EXPECT_EQ("Johnny", std::string(reinterpret_cast<char *>(output[1]), outLen[1]));
    EXPECT_TRUE(output[2] == nullptr);
    EXPECT_TRUE(output[3] == nullptr);
    EXPECT_TRUE(output[4] == nullptr);
    EXPECT_TRUE(output[5] == nullptr);
    EXPECT_EQ("Mary", std::string(reinterpret_cast<char *>(output[6]), outLen[6]));
    // Test that string above the given length isnt considered
    EXPECT_EQ(4, outLen[0]);
    EXPECT_EQ(6, outLen[1]);
    EXPECT_EQ(0, outLen[2]);
    EXPECT_EQ(0, outLen[3]);
    EXPECT_EQ(0, outLen[4]);
    EXPECT_EQ(0, outLen[5]);
    EXPECT_EQ(4, outLen[6]);
}

TEST(BatchFunctionTest, SubstrZh)
{
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);
    std::string str = "时欧基乌斯侧后解 hello! 回复哦黑色的and magic粉色的圣诞袜";
    auto strLen = static_cast<int32_t>(str.length());

    std::vector<std::string> inputStr(8, str);
    std::vector<int32_t> inputLen(8, strLen);
    std::vector<int32_t> startIndexs { 1, 1, 10, -5, 0, 37, -38, -37 };
    std::vector<int32_t> length { 37, 5, 10, 7, 0, strLen + 5, 10, 37 };
    int32_t rowCnt = inputStr.size();
    std::vector<int32_t> outLen(rowCnt);
    std::vector<uint8_t *> outResult(rowCnt);
    std::vector<uint8_t *> strAddr(rowCnt);
    for (int32_t i = 0; i < rowCnt; i++) {
        strAddr[i] = reinterpret_cast<uint8_t *>(const_cast<char *>(inputStr[i].c_str()));
    }
    bool isAnyNull[] = { false, false, false, false, false, false, false, false };
    BatchSubstrVarchar<int32_t, false, false>(contextPtr, strAddr.data(), inputLen.data(), startIndexs.data(),
        length.data(), isAnyNull, outResult.data(), outLen.data(), rowCnt);

    std::vector<std::string> expected { str, "时欧基乌斯", "hello! 回复哦", "色的圣诞袜", "", "袜", "", str };
    AssertStringEquals(expected, outResult, outLen);

    delete context;
}

TEST(BatchFunctionTest, SubstrZhWhenStartIndexEqualsZero)
{
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);
    std::string str = "时欧基乌斯侧后解 hello! 回复哦黑色的and magic粉色的圣诞袜";
    auto strLen = static_cast<int32_t>(str.length());

    std::vector<std::string> inputStr(8, str);
    std::vector<int32_t> inputLen(8, strLen);
    std::vector<int32_t> startIndexs { 0, 0, 0, 0, 0, 0, 0, 0 };
    std::vector<int32_t> length { 37, 5, 10, 7, 0, strLen + 5, 10, 37 };
    int32_t rowCnt = inputStr.size();
    std::vector<int32_t> outLen(rowCnt);
    std::vector<uint8_t *> outResult(rowCnt);
    std::vector<uint8_t *> strAddr(rowCnt);
    for (int32_t i = 0; i < rowCnt; i++) {
        strAddr[i] = reinterpret_cast<uint8_t *>(const_cast<char *>(inputStr[i].c_str()));
    }
    bool isAnyNull[] = { false, false, false, false, false, false, false, false };
    BatchSubstrVarchar<int32_t, false, false>(contextPtr, strAddr.data(), inputLen.data(), startIndexs.data(),
        length.data(), isAnyNull, outResult.data(), outLen.data(), rowCnt);

    std::vector<std::string> expected { "", "", "", "", "", "", "", "" };
    AssertStringEquals(expected, outResult, outLen);

    delete context;
}

TEST(BatchFunctionTest, SubstrCharZh)
{
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);
    std::string str = "时欧基乌斯侧后解 hello! 回复哦黑色的and magic粉色的圣诞袜";
    int32_t width = 37;
    int32_t strLen = str.length();

    std::vector<std::string> inputStr(8, str);
    std::vector<int32_t> inputLen(8, strLen);
    std::vector<int32_t> startIndexs { 1, 1, 10, -5, 0, 37, -38, -37 };
    std::vector<int32_t> length { 37, 5, 10, 7, 0, strLen + 5, 10, 37 };
    int32_t rowCnt = inputStr.size();
    std::vector<int32_t> outLen(rowCnt);
    std::vector<uint8_t *> outResult(rowCnt);
    std::vector<uint8_t *> strAddr(rowCnt);
    for (int32_t i = 0; i < rowCnt; i++) {
        strAddr[i] = reinterpret_cast<uint8_t *>(const_cast<char *>(inputStr[i].c_str()));
    }

    bool isAnyNull[] = { false, false, false, false, false, false, false, false };
    BatchSubstrChar<int32_t, false, false>(contextPtr, strAddr.data(), width, inputLen.data(), startIndexs.data(),
        length.data(), isAnyNull, outResult.data(), outLen.data(), rowCnt);

    std::vector<std::string> expected { str, "时欧基乌斯", "hello! 回复哦", "色的圣诞袜", "", "袜", "", str };
    AssertStringEquals(expected, outResult, outLen);

    delete context;
}

TEST(BatchFunctionTest, SubstrCharZhWhenStartIndexEqualsZero)
{
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);
    std::string str = "时欧基乌斯侧后解 hello! 回复哦黑色的and magic粉色的圣诞袜";
    int32_t width = 37;
    int32_t strLen = str.length();

    std::vector<std::string> inputStr(8, str);
    std::vector<int32_t> inputLen(8, strLen);
    std::vector<int32_t> startIndexs { 0, 0, 0, 0, 0, 0, 0, 0 };
    std::vector<int32_t> length { 37, 5, 10, 7, 0, strLen + 5, 10, 37 };
    int32_t rowCnt = inputStr.size();
    std::vector<int32_t> outLen(rowCnt);
    std::vector<uint8_t *> outResult(rowCnt);
    std::vector<uint8_t *> strAddr(rowCnt);
    for (int32_t i = 0; i < rowCnt; i++) {
        strAddr[i] = reinterpret_cast<uint8_t *>(const_cast<char *>(inputStr[i].c_str()));
    }

    bool isAnyNull[] = { false, false, false, false, false, false, false, false };
    BatchSubstrChar<int32_t, false, false>(contextPtr, strAddr.data(), width, inputLen.data(), startIndexs.data(),
        length.data(), isAnyNull, outResult.data(), outLen.data(), rowCnt);

    std::vector<std::string> expected { "", "", "", "", "", "", "", "" };
    AssertStringEquals(expected, outResult, outLen);

    delete context;
}

TEST(BatchFunctionTest, SubstrWithStartZh)
{
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);
    std::string str = "时欧基乌斯侧后解 hello! 回复哦黑色的and magic粉色的圣诞袜";
    auto strLen = static_cast<int32_t>(str.length());

    std::vector<std::string> inputStr(7, str);
    std::vector<int32_t> inputLen(7, strLen);
    std::vector<int32_t> startIndexs { 1, 9, -3, 0, 37, -38, -37 };
    int32_t rowCnt = inputStr.size();
    std::vector<int32_t> outLen(rowCnt);
    std::vector<uint8_t *> outResult(rowCnt);
    std::vector<uint8_t *> strAddr(rowCnt);
    for (int32_t i = 0; i < rowCnt; i++) {
        strAddr[i] = reinterpret_cast<uint8_t *>(const_cast<char *>(inputStr[i].c_str()));
    }

    bool isAnyNull[] = { false, false, false, false, false, false, false };
    BatchSubstrVarcharWithStart<int32_t, false, false>(contextPtr, strAddr.data(), inputLen.data(), startIndexs.data(),
        isAnyNull, outResult.data(), outLen.data(), rowCnt);

    std::vector<std::string> expected { str, " hello! 回复哦黑色的and magic粉色的圣诞袜", "圣诞袜", "", "袜", "", str };
    AssertStringEquals(expected, outResult, outLen);

    delete context;
}

TEST(BatchFunctionTest, SubstrWithStartZhWhenStartIndexEqualsZero)
{
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);
    std::string str = "时欧基乌斯侧后解 hello! 回复哦黑色的and magic粉色的圣诞袜";
    auto strLen = static_cast<int32_t>(str.length());

    std::vector<std::string> inputStr(7, str);
    std::vector<int32_t> inputLen(7, strLen);
    std::vector<int32_t> startIndexs { 0, 0, 0, 0, 0, 0, 0 };
    int32_t rowCnt = inputStr.size();
    std::vector<int32_t> outLen(rowCnt);
    std::vector<uint8_t *> outResult(rowCnt);
    std::vector<uint8_t *> strAddr(rowCnt);
    for (int32_t i = 0; i < rowCnt; i++) {
        strAddr[i] = reinterpret_cast<uint8_t *>(const_cast<char *>(inputStr[i].c_str()));
    }

    bool isAnyNull[] = { false, false, false, false, false, false, false };
    BatchSubstrVarcharWithStart<int32_t, false, false>(contextPtr, strAddr.data(), inputLen.data(), startIndexs.data(),
        isAnyNull, outResult.data(), outLen.data(), rowCnt);

    std::vector<std::string> expected { "", "", "", "", "", "", "" };
    AssertStringEquals(expected, outResult, outLen);

    delete context;
}

TEST(BatchFunctionTest, SubstrWithStartZhForSpark)
{
    ConfigUtil::SetNegativeStartIndexOutOfBoundsRule(NegativeStartIndexOutOfBoundsRule::INTERCEPT_FROM_BEYOND);
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);
    std::string str = "时欧基乌斯侧后解 h";
    auto strLen = static_cast<int32_t>(str.length());

    std::vector<std::string> inputStr(1, str);
    std::vector<int32_t> inputLen(1, strLen);
    std::vector<int32_t> outLen(inputStr.size());
    std::vector<uint8_t *> outResult(inputStr.size());
    std::vector<int32_t> startIndexs { -15 };
    int32_t rowCnt = inputStr.size();
    std::vector<uint8_t *> strAddr(rowCnt);
    for (int32_t i = 0; i < rowCnt; i++) {
        strAddr[i] = reinterpret_cast<uint8_t *>(const_cast<char *>(inputStr[i].c_str()));
    }

    bool isAnyNull[] = { false };
    BatchSubstrVarcharWithStart<int32_t, true, false>(contextPtr, strAddr.data(), inputLen.data(), startIndexs.data(),
        isAnyNull, outResult.data(), outLen.data(), rowCnt);

    std::vector<std::string> expected(1, str);
    AssertStringEquals(expected, outResult, outLen);

    ConfigUtil::SetNegativeStartIndexOutOfBoundsRule(NegativeStartIndexOutOfBoundsRule::EMPTY_STRING);
    delete context;
}

TEST(BatchFunctionTest, SubstrWithStartZhForSparkWhenStartIndexEqualsZero)
{
    ConfigUtil::SetZeroStartIndexSupportRule(ZeroStartIndexSupportRule::IS_SUPPORT);
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);
    std::string str = "时欧基乌斯侧后解 h";
    auto strLen = static_cast<int32_t>(str.length());

    std::vector<std::string> inputStr(1, str);
    std::vector<int32_t> inputLen(1, strLen);
    std::vector<int32_t> outLen(inputStr.size());
    std::vector<uint8_t *> outResult(inputStr.size());
    std::vector<int32_t> startIndexs { 0 };
    int32_t rowCnt = inputStr.size();
    std::vector<uint8_t *> strAddr(rowCnt);
    for (int32_t i = 0; i < rowCnt; i++) {
        strAddr[i] = reinterpret_cast<uint8_t *>(const_cast<char *>(inputStr[i].c_str()));
    }

    bool isAnyNull[] = { false };
    BatchSubstrVarcharWithStart<int32_t, true, true>(contextPtr, strAddr.data(), inputLen.data(), startIndexs.data(),
        isAnyNull, outResult.data(), outLen.data(), rowCnt);

    std::vector<std::string> expected(1, str);
    AssertStringEquals(expected, outResult, outLen);

    ConfigUtil::SetZeroStartIndexSupportRule(ZeroStartIndexSupportRule::IS_NOT_SUPPORT);
    delete context;
}

TEST(BatchFunctionTest, SubstrWithStartEnForSpark)
{
    ConfigUtil::SetNegativeStartIndexOutOfBoundsRule(NegativeStartIndexOutOfBoundsRule::INTERCEPT_FROM_BEYOND);
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);
    std::string str = "apple";
    auto strLen = static_cast<int32_t>(str.length());

    std::vector<std::string> inputStr(1, str);
    std::vector<int32_t> inputLen(1, strLen);
    std::vector<int32_t> outLen(inputStr.size());
    std::vector<uint8_t *> outResult(inputStr.size());
    std::vector<int32_t> startIndexs { -7 };
    int32_t rowCnt = inputStr.size();
    std::vector<uint8_t *> strAddr(rowCnt);
    for (int32_t i = 0; i < rowCnt; i++) {
        strAddr[i] = reinterpret_cast<uint8_t *>(const_cast<char *>(inputStr[i].c_str()));
    }

    bool isAnyNull[] = { false };
    BatchSubstrVarcharWithStart<int32_t, true, false>(contextPtr, strAddr.data(), inputLen.data(), startIndexs.data(),
        isAnyNull, outResult.data(), outLen.data(), rowCnt);

    std::vector<std::string> expected(1, str);
    AssertStringEquals(expected, outResult, outLen);

    ConfigUtil::SetNegativeStartIndexOutOfBoundsRule(NegativeStartIndexOutOfBoundsRule::EMPTY_STRING);
    delete context;
}

TEST(BatchFunctionTest, SubstrWithStartEnForSparkWhenStartIndexEqualsZero)
{
    ConfigUtil::SetZeroStartIndexSupportRule(ZeroStartIndexSupportRule::IS_SUPPORT);
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);
    std::string str = "apple";
    auto strLen = static_cast<int32_t>(str.length());

    std::vector<std::string> inputStr(1, str);
    std::vector<int32_t> inputLen(1, strLen);
    std::vector<int32_t> outLen(inputStr.size());
    std::vector<uint8_t *> outResult(inputStr.size());
    std::vector<int32_t> startIndexs { 0 };
    int32_t rowCnt = inputStr.size();
    std::vector<uint8_t *> strAddr(rowCnt);
    for (int32_t i = 0; i < rowCnt; i++) {
        strAddr[i] = reinterpret_cast<uint8_t *>(const_cast<char *>(inputStr[i].c_str()));
    }

    bool isAnyNull[] = { false };
    BatchSubstrVarcharWithStart<int32_t, true, true>(contextPtr, strAddr.data(), inputLen.data(), startIndexs.data(),
        isAnyNull, outResult.data(), outLen.data(), rowCnt);

    std::vector<std::string> expected(1, str);
    AssertStringEquals(expected, outResult, outLen);

    ConfigUtil::SetZeroStartIndexSupportRule(ZeroStartIndexSupportRule::IS_NOT_SUPPORT);
    delete context;
}

TEST(BatchFunctionTest, SubstrWithZhForSpark)
{
    ConfigUtil::SetNegativeStartIndexOutOfBoundsRule(NegativeStartIndexOutOfBoundsRule::INTERCEPT_FROM_BEYOND);
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);
    std::string str = "时欧基乌斯侧后解 h";
    auto strLen = static_cast<int32_t>(str.length());

    std::vector<std::string> inputStr(4, str);
    int32_t rowCnt = inputStr.size();
    std::vector<int32_t> inputLen(4, strLen);
    std::vector<int32_t> outLen(inputStr.size());
    std::vector<uint8_t *> outResult(inputStr.size());
    std::vector<int32_t> startIndexs { -15, -15, -15, -15 };
    std::vector<int32_t> length { 5, 6, 14, 20 };
    std::vector<uint8_t *> strAddr(rowCnt);

    for (int32_t i = 0; i < rowCnt; i++) {
        strAddr[i] = reinterpret_cast<uint8_t *>(const_cast<char *>(inputStr[i].c_str()));
    }

    bool isAnyNull1[] = { false, false, false, false };
    BatchSubstrVarchar<int32_t, true, false>(contextPtr, strAddr.data(), inputLen.data(), startIndexs.data(),
        length.data(), isAnyNull1, outResult.data(), outLen.data(), rowCnt);
    std::vector<std::string> expected { "", "时", "时欧基乌斯侧后解 ", "时欧基乌斯侧后解 h" };
    AssertStringEquals(expected, outResult, outLen);

    ConfigUtil::SetNegativeStartIndexOutOfBoundsRule(NegativeStartIndexOutOfBoundsRule::EMPTY_STRING);
    delete context;
}

TEST(BatchFunctionTest, SubstrWithZhForSparkWhenStartIndexEqualsZero)
{
    ConfigUtil::SetZeroStartIndexSupportRule(ZeroStartIndexSupportRule::IS_SUPPORT);
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);
    std::string str = "时欧基乌斯侧后解 h";
    auto strLen = static_cast<int32_t>(str.length());

    std::vector<std::string> inputStr(4, str);
    int32_t rowCnt = inputStr.size();
    std::vector<int32_t> inputLen(4, strLen);
    std::vector<int32_t> outLen(inputStr.size());
    std::vector<uint8_t *> outResult(inputStr.size());
    std::vector<int32_t> startIndexs { 0, 0, 0, 0 };
    std::vector<int32_t> length { 0, 1, 2, 5 };
    std::vector<uint8_t *> strAddr(rowCnt);

    for (int32_t i = 0; i < rowCnt; i++) {
        strAddr[i] = reinterpret_cast<uint8_t *>(const_cast<char *>(inputStr[i].c_str()));
    }

    bool isAnyNull1[] = { false, false, false, false };
    BatchSubstrVarchar<int32_t, true, true>(contextPtr, strAddr.data(), inputLen.data(), startIndexs.data(),
        length.data(), isAnyNull1, outResult.data(), outLen.data(), rowCnt);
    std::vector<std::string> expected { "", "时", "时欧", "时欧基乌斯" };
    AssertStringEquals(expected, outResult, outLen);

    ConfigUtil::SetZeroStartIndexSupportRule(ZeroStartIndexSupportRule::IS_NOT_SUPPORT);
    delete context;
}

TEST(BatchFunctionTest, SubstrWithEnForSpark)
{
    ConfigUtil::SetNegativeStartIndexOutOfBoundsRule(NegativeStartIndexOutOfBoundsRule::INTERCEPT_FROM_BEYOND);
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);
    std::string str = "apple";
    auto strLen = static_cast<int32_t>(str.length());

    std::vector<std::string> inputStr(1, str);
    int32_t rowCnt = inputStr.size();
    std::vector<int32_t> inputLen(1, strLen);
    std::vector<int32_t> outLen(inputStr.size());
    std::vector<uint8_t *> outResult(inputStr.size());
    std::vector<int32_t> startIndexs { -7 };
    std::vector<int32_t> length { 3 };
    std::vector<uint8_t *> strAddr(rowCnt);

    for (int32_t i = 0; i < rowCnt; i++) {
        strAddr[i] = reinterpret_cast<uint8_t *>(const_cast<char *>(inputStr[i].c_str()));
    }

    bool isAnyNull1[] = { false };
    BatchSubstrVarchar<int32_t, true, false>(contextPtr, strAddr.data(), inputLen.data(), startIndexs.data(),
        length.data(), isAnyNull1, outResult.data(), outLen.data(), rowCnt);
    std::vector<std::string> expected { "a" };
    AssertStringEquals(expected, outResult, outLen);

    ConfigUtil::SetNegativeStartIndexOutOfBoundsRule(NegativeStartIndexOutOfBoundsRule::EMPTY_STRING);
    delete context;
}

TEST(BatchFunctionTest, SubstrWithEnForSparkWhenStartIndexEqualsZero)
{
    ConfigUtil::SetZeroStartIndexSupportRule(ZeroStartIndexSupportRule::IS_SUPPORT);
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);
    std::string str = "apple";
    auto strLen = static_cast<int32_t>(str.length());

    std::vector<std::string> inputStr(1, str);
    int32_t rowCnt = inputStr.size();
    std::vector<int32_t> inputLen(1, strLen);
    std::vector<int32_t> outLen(inputStr.size());
    std::vector<uint8_t *> outResult(inputStr.size());
    std::vector<int32_t> startIndexs { 0 };
    std::vector<int32_t> length { 3 };
    std::vector<uint8_t *> strAddr(rowCnt);

    for (int32_t i = 0; i < rowCnt; i++) {
        strAddr[i] = reinterpret_cast<uint8_t *>(const_cast<char *>(inputStr[i].c_str()));
    }

    bool isAnyNull1[] = { false };
    BatchSubstrVarchar<int32_t, true, true>(contextPtr, strAddr.data(), inputLen.data(), startIndexs.data(),
        length.data(), isAnyNull1, outResult.data(), outLen.data(), rowCnt);
    std::vector<std::string> expected { "app" };
    AssertStringEquals(expected, outResult, outLen);

    ConfigUtil::SetZeroStartIndexSupportRule(ZeroStartIndexSupportRule::IS_NOT_SUPPORT);
    delete context;
}

TEST(BatchFunctionTest, SubstrCharWithStartZh)
{
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);
    std::string str = "时欧基乌斯侧后解 hello! 回复哦黑色的and magic粉色的圣诞袜";
    auto width = static_cast<int32_t>(str.length());
    int32_t strLen = str.length();

    std::vector<std::string> inputStr(7, str);
    std::vector<int32_t> inputLen(7, strLen);
    std::vector<int32_t> outLen(inputStr.size());
    std::vector<uint8_t *> outResult(inputStr.size());
    std::vector<int32_t> startIndexs { 1, 9, -3, 0, 37, -38, -37 };
    int32_t rowCnt = inputStr.size();
    std::vector<uint8_t *> strAddr(rowCnt);
    for (int32_t i = 0; i < rowCnt; i++) {
        strAddr[i] = reinterpret_cast<uint8_t *>(const_cast<char *>(inputStr[i].c_str()));
    }

    bool isAnyNull[] = { false, false, false, false, false, false, false };
    BatchSubstrCharWithStart<int32_t, false, false>(contextPtr, strAddr.data(), width, inputLen.data(),
        startIndexs.data(), isAnyNull, outResult.data(), outLen.data(), rowCnt);

    std::vector<std::string> expected { str, " hello! 回复哦黑色的and magic粉色的圣诞袜", "圣诞袜", "", "袜", "", str };
    AssertStringEquals(expected, outResult, outLen);

    delete context;
}

TEST(BatchFunctionTest, SubstrCharWithStartZhWhenStartIndexEqualsZero)
{
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);
    std::string str = "时欧基乌斯侧后解 hello! 回复哦黑色的and magic粉色的圣诞袜";
    auto width = static_cast<int32_t>(str.length());
    int32_t strLen = str.length();

    std::vector<std::string> inputStr(7, str);
    std::vector<int32_t> inputLen(7, strLen);
    std::vector<int32_t> outLen(inputStr.size());
    std::vector<uint8_t *> outResult(inputStr.size());
    std::vector<int32_t> startIndexs { 0, 0, 0, 0, 0, 0, 0 };
    int32_t rowCnt = inputStr.size();
    std::vector<uint8_t *> strAddr(rowCnt);
    for (int32_t i = 0; i < rowCnt; i++) {
        strAddr[i] = reinterpret_cast<uint8_t *>(const_cast<char *>(inputStr[i].c_str()));
    }

    bool isAnyNull[] = { false, false, false, false, false, false, false };
    BatchSubstrCharWithStart<int32_t, false, false>(contextPtr, strAddr.data(), width, inputLen.data(),
        startIndexs.data(), isAnyNull, outResult.data(), outLen.data(), rowCnt);

    std::vector<std::string> expected { "", "", "", "", "", "", "" };
    AssertStringEquals(expected, outResult, outLen);

    delete context;
}

TEST(BatchFunctionTest, LengthStrZh)
{
    std::string str = "时欧基乌斯侧后解 hello! 回复哦黑色的and magic粉色的圣诞袜";
    int32_t strLen = str.length();

    std::vector<std::string> inputStr { str, "解 hello! 回复哦黑色的" };
    std::vector<int32_t> inputLen { strLen, 29 };
    std::vector<int64_t> outLen(inputStr.size());
    int32_t rowCnt = inputStr.size();
    std::vector<uint8_t *> strAddr(rowCnt);
    for (int32_t i = 0; i < rowCnt; i++) {
        strAddr[i] = reinterpret_cast<uint8_t *>(const_cast<char *>(inputStr[i].c_str()));
    }

    bool isAnyNull[] = { false, false };
    BatchLengthStr(strAddr.data(), inputLen.data(), isAnyNull, outLen.data(), rowCnt);
    std::vector<int64_t> expected { 37, 15 };
    AssertLongEquals(expected, outLen);
}


TEST(BatchFunctionTest, LikeStrZh)
{
    std::string str = "时欧基乌斯侧后解 hello! 回复哦黑色的and magic粉色的圣诞袜";
    int32_t strLen = str.length();

    std::vector<std::string> inputStr(4, str);
    std::vector<int32_t> inputLen(4, strLen);
    std::vector<std::string> patternStr { "^时欧基乌斯侧后解 hello! 回复哦黑色的and magic粉色的圣诞.$",
        "^时欧基乌斯侧后解 hello! 回复哦黑色的and magic粉色的圣诞..$",
        "^时欧基乌斯侧后解 hello! 回复哦黑色的and magic粉色的圣.*$",
        "^欧时基乌斯侧后解 hello! 回复哦黑色的and magic粉色的圣诞.*$" };
    int32_t rowCnt = inputStr.size();
    bool output[rowCnt];
    std::vector<uint8_t *> strAddr(rowCnt);
    std::vector<uint8_t *> patternAddr(rowCnt);
    std::vector<int32_t> patternLen(rowCnt);
    for (int32_t i = 0; i < rowCnt; i++) {
        strAddr[i] = reinterpret_cast<uint8_t *>(const_cast<char *>(inputStr[i].c_str()));
        patternAddr[i] = reinterpret_cast<uint8_t *>(const_cast<char *>(patternStr[i].c_str()));
        patternLen[i] = static_cast<int32_t>(patternStr[i].length());
    }

    bool isAnyNull[] = { false, false, false, false };
    BatchLikeStr(strAddr.data(), inputLen.data(), patternAddr.data(), patternLen.data(), isAnyNull, output, rowCnt);

    std::vector<bool> expected { true, false, true, false };
    AssertBoolEquals(expected, output);
}

TEST(BatchFunctionTest, LikeCharZh)
{
    std::vector<std::string> inputStr { "时欧基乌斯侧后解 hello! 回复哦黑色的and magic粉色的圣诞袜", "时欧基乌",
        "时欧基乌", "时欧基乌" };
    std::vector<std::string> patternStr { "^时欧基乌斯侧后解 hello! 回复哦黑色的and magic粉色的圣诞.$", "^时欧基乌..$",
        "^时欧基乌.$", "^时欧基乌.*$" };
    int32_t batch = 4;
    std::vector<bool> expected { true, true, false, true };
    std::vector<int32_t> width { 37, 6, 6, 6 };
    for (int32_t i = 0; i < batch; i++) {
        int32_t rowCnt = 1;
        std::vector<int32_t> inputLen { static_cast<int32_t>(inputStr[i].length()) };
        bool output[rowCnt];
        std::vector<uint8_t *> strAddr { reinterpret_cast<uint8_t *>(const_cast<char *>(inputStr[i].c_str())) };
        std::vector<uint8_t *> patternAddr { reinterpret_cast<uint8_t *>(const_cast<char *>(patternStr[i].c_str())) };
        std::vector<int32_t> patternLen { static_cast<int32_t>(patternStr[i].length()) };

        bool isAnyNull[] = { false };
        BatchLikeChar(strAddr.data(), width[i], inputLen.data(), patternAddr.data(), patternLen.data(), isAnyNull,
            output, rowCnt);
        EXPECT_EQ(output[0], expected[i]);
    }
}

TEST(BatchFunctionTest, ReplaceStrStrStrZh)
{
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);
    std::vector<std::string> str { "apple", "粉色的圣诞袜", "粉色de圣诞袜", "粉色de圣诞袜", "粉色de圣诞袜",
        "",      "粉色de圣诞袜" };
    std::vector<int32_t> strLen { 5, 18, 17, 17, 17, 0, 17 };
    std::vector<std::string> searchStr { "", "", "", "", "de圣", "", "" };
    std::vector<int32_t> searchLen { 0, 0, 0, 0, 5, 0, 0 };
    std::vector<std::string> replaceStr { "*w*", "*w*", "*w*", "*的*", "*的*", "", "" };
    std::vector<int32_t> replaceLen { 3, 3, 3, 5, 5, 0, 0 };
    int32_t rowCnt = str.size();
    std::vector<uint8_t *> output(rowCnt);
    std::vector<int32_t> outLen(rowCnt);
    std::vector<uint8_t *> strAddr(rowCnt);
    std::vector<uint8_t *> searchStrAddr(rowCnt);
    std::vector<uint8_t *> replaceStrAddr(rowCnt);
    for (int32_t i = 0; i < rowCnt; i++) {
        strAddr[i] = reinterpret_cast<uint8_t *>(const_cast<char *>(str[i].c_str()));
        searchStrAddr[i] = reinterpret_cast<uint8_t *>(const_cast<char *>(searchStr[i].c_str()));
        replaceStrAddr[i] = reinterpret_cast<uint8_t *>(const_cast<char *>(replaceStr[i].c_str()));
    }

    bool isAnyNull[] = { false, false, false, false, false, false, false };
    BatchReplaceStrStrStrWithRepReplace(contextPtr, strAddr.data(), strLen.data(), searchStrAddr.data(),
        searchLen.data(), replaceStrAddr.data(), replaceLen.data(), isAnyNull, output.data(), outLen.data(), rowCnt);
    std::vector<std::string> expected { "*w*a*w*p*w*p*w*l*w*e*w*",
        "*w*粉*w*色*w*的*w*圣*w*诞*w*袜*w*",
        "*w*粉*w*色*w*d*w*e*w*圣*w*诞*w*袜*w*",
        "*的*粉*的*色*的*d*的*e*的*圣*的*诞*的*袜*的*",
        "粉色*的*诞袜",
        "",
        "粉色de圣诞袜" };
    AssertStringEquals(expected, output, outLen);
    delete context;
}

TEST(BatchFunctionTest, ReplaceWithoutRepZh)
{
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);
    std::vector<std::string> str { "apple", "apple", "粉色的圣诞袜", "粉色的圣诞袜", "粉色de圣诞袜", "粉色de圣诞袜" };
    std::vector<int32_t> strLen { 5, 5, 18, 18, 17, 17 };
    std::vector<std::string> searchStr { "", "pp", "", "圣诞", "", "色de" };
    std::vector<int32_t> searchLen { 0, 2, 0, 6, 0, 5 };
    int32_t rowCnt = str.size();
    std::vector<uint8_t *> output(rowCnt);
    std::vector<int32_t> outLen(rowCnt);
    std::vector<uint8_t *> strAddr(rowCnt);
    std::vector<uint8_t *> searchStrAddr(rowCnt);
    for (int32_t i = 0; i < rowCnt; i++) {
        strAddr[i] = reinterpret_cast<uint8_t *>(const_cast<char *>(str[i].c_str()));
        searchStrAddr[i] = reinterpret_cast<uint8_t *>(const_cast<char *>(searchStr[i].c_str()));
    }

    bool isAnyNull[] = { false, false, false, false, false, false };
    BatchReplaceStrStrWithoutRepNotReplace(contextPtr, strAddr.data(), strLen.data(), searchStrAddr.data(),
        searchLen.data(), isAnyNull, output.data(), outLen.data(), rowCnt);
    std::vector<std::string> expected {
        "apple", "ale", "粉色的圣诞袜", "粉色的袜", "粉色de圣诞袜", "粉圣诞袜",
    };
    AssertStringEquals(expected, output, outLen);
    delete context;
}

TEST(BatchFunctionTest, ConcatStrStrZh)
{
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);
    std::vector<std::string> ap { "你是Chinese?", "", "pink圣诞袜" };
    std::vector<int32_t> apLen { 14, 0, 13 };
    std::vector<std::string> bp { "Yes我是", "粉色de圣诞袜", "" };
    std::vector<int32_t> bpLen { 9, 17, 0 };
    int32_t rowCnt = ap.size();
    std::vector<uint8_t *> output(rowCnt);
    std::vector<int32_t> outLen(rowCnt);
    std::vector<uint8_t *> apAddr(rowCnt);
    std::vector<uint8_t *> bpAddr(rowCnt);
    for (int32_t i = 0; i < rowCnt; i++) {
        apAddr[i] = reinterpret_cast<uint8_t *>(const_cast<char *>(ap[i].c_str()));
        bpAddr[i] = reinterpret_cast<uint8_t *>(const_cast<char *>(bp[i].c_str()));
    }
    bool isAnyNull[] = { false, false, false };
    BatchConcatStrStr(contextPtr, apAddr.data(), apLen.data(), bpAddr.data(), bpLen.data(), isAnyNull, output.data(),
        outLen.data(), rowCnt);
    std::vector<std::string> expected { "你是Chinese?Yes我是", "粉色de圣诞袜", "pink圣诞袜" };
    AssertStringEquals(expected, output, outLen);
    delete context;
}

TEST(BatchFunctionTest, ConcatCharCharZh)
{
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);
    std::vector<std::string> ap { "粉色de圣诞袜", "*黑色*",      "Hei你好吗",    "Oh我很好",
        "Hei你好吗   ", "Oh我很好  ",  "   Hei你好吗", "   Oh我很好",
        "Hei   你好吗", "Oh   我很好", "   ",          "Oh我很好",
        "Hei你好吗",    "   ",         "Hei你好吗",    "" };
    std::vector<int32_t> aWidth { 7, 8, 10, 12, 12, 5, 8, 8 };
    std::vector<std::string> bp { "*黑色*",      "粉色de",      "Oh我很好",     "Hei你好吗",   "Oh我很好  ",
        "Hei你好吗  ", "   Oh我很好", "   Hei你好吗", "Oh   我很好", "Hei   你好",
        "Oh我很好   ", "   ",         "   ",          "Hei你好吗",   "",
        "Hei你好" };
    std::vector<int32_t> bWidth { 4, 8, 8, 12, 8, 12, 5, 5 };
    std::vector<std::string> expected { "粉色de圣诞袜*黑色*",
        "*黑色*   粉色de",
        "Hei你好吗  Oh我很好",
        "Oh我很好   Hei你好吗",
        "Hei你好吗    Oh我很好  ",
        "Oh我很好     Hei你好吗  ",
        "   Hei你好吗      Oh我很好",
        "   Oh我很好       Hei你好吗",
        "Hei   你好吗   Oh   我很好",
        "Oh   我很好    Hei   你好",
        "     Oh我很好   ",
        "Oh我很好   ",
        "Hei你好吗     ",
        "        Hei你好吗",
        "Hei你好吗",
        "        Hei你好" };
    int32_t batch = 8;
    int32_t rowCnt = 2;
    for (int32_t i = 0; i < batch; i++) {
        std::vector<uint8_t *> output(rowCnt);
        std::vector<int32_t> outLen(rowCnt);
        std::vector<int32_t> apLen(rowCnt);
        std::vector<int32_t> bpLen(rowCnt);
        std::vector<uint8_t *> apAddr(rowCnt);
        std::vector<uint8_t *> bpAddr(rowCnt);
        for (int32_t row = 0; row < rowCnt; row++) {
            int32_t index = i * rowCnt + row;
            apLen[row] = static_cast<int32_t>(ap[index].length());
            bpLen[row] = static_cast<int32_t>(bp[index].length());
            apAddr[row] = reinterpret_cast<uint8_t *>(const_cast<char *>(ap[index].c_str()));
            bpAddr[row] = reinterpret_cast<uint8_t *>(const_cast<char *>(bp[index].c_str()));
        }
        bool isAnyNull[] = { false, false };
        BatchConcatCharChar(contextPtr, apAddr.data(), aWidth[i], apLen.data(), bpAddr.data(), bWidth[i], bpLen.data(),
            isAnyNull, output.data(), outLen.data(), rowCnt);
        AssertStringEquals(expected, i * rowCnt, rowCnt, output, outLen);
    }
    delete context;
}

TEST(BatchFunctionTest, ConcatCharStrZh)
{
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);
    std::vector<std::string> ap { "*你是谁呢*", "我很OK", "*你是谁呢*", "我很OK", "*你是谁呢*", "" };
    std::vector<int32_t> aWidth { 6, 10, 10 };
    std::vector<std::string> bp { "我很OK", "*你是谁呢*", "我很OK", "*你是谁呢*", "", "*你是谁呢*" };
    std::vector<std::string> expected { "*你是谁呢*我很OK",       "我很OK  *你是谁呢*", "*你是谁呢*    我很OK",
        "我很OK      *你是谁呢*", "*你是谁呢*",         "          *你是谁呢*" };
    int32_t batch = 3;
    int32_t rowCnt = 2;
    for (int32_t i = 0; i < batch; i++) {
        std::vector<uint8_t *> output(rowCnt);
        std::vector<int32_t> outLen(rowCnt);
        std::vector<int32_t> apLen(rowCnt);
        std::vector<int32_t> bpLen(rowCnt);
        std::vector<uint8_t *> apAddr(rowCnt);
        std::vector<uint8_t *> bpAddr(rowCnt);
        for (int32_t row = 0; row < rowCnt; row++) {
            int32_t index = i * rowCnt + row;
            apLen[row] = static_cast<int32_t>(ap[index].length());
            bpLen[row] = static_cast<int32_t>(bp[index].length());
            apAddr[row] = reinterpret_cast<uint8_t *>(const_cast<char *>(ap[index].c_str()));
            bpAddr[row] = reinterpret_cast<uint8_t *>(const_cast<char *>(bp[index].c_str()));
        }

        bool isAnyNull[] = { false, false };
        BatchConcatCharStr(contextPtr, apAddr.data(), aWidth[i], apLen.data(), bpAddr.data(), bpLen.data(), isAnyNull,
            output.data(), outLen.data(), rowCnt);
        AssertStringEquals(expected, i * rowCnt, rowCnt, output, outLen);
    }
    delete context;
}

TEST(BatchFunctionTest, ConcatStrCharZh)
{
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);
    std::vector<std::string> ap { "粉色de圣诞袜", "*黑色*", "粉色de圣诞袜", "*黑色*", "粉色de圣诞袜   ", "" };
    std::vector<std::string> bp { "*黑色*", "粉色de圣诞袜", "*黑色*", "粉色de圣诞袜", "", "粉色de圣诞袜   " };
    std::vector<int32_t> bWidth { 4, 6, 5 };
    std::vector<std::string> expected { "粉色de圣诞袜*黑色*", "*黑色*粉色de圣诞袜", "粉色de圣诞袜*黑色*",
        "*黑色*粉色de圣诞袜", "粉色de圣诞袜   ",    "粉色de圣诞袜   " };
    int32_t batch = 3;
    int32_t rowCnt = 2;
    for (int32_t i = 0; i < batch; i++) {
        std::vector<uint8_t *> output(rowCnt);
        std::vector<int32_t> outLen(rowCnt);
        std::vector<int32_t> apLen(rowCnt);
        std::vector<int32_t> bpLen(rowCnt);
        std::vector<uint8_t *> apAddr(rowCnt);
        std::vector<uint8_t *> bpAddr(rowCnt);
        for (int32_t row = 0; row < rowCnt; row++) {
            int32_t index = i * rowCnt + row;
            apLen[row] = static_cast<int32_t>(ap[index].length());
            bpLen[row] = static_cast<int32_t>(bp[index].length());
            apAddr[row] = reinterpret_cast<uint8_t *>(const_cast<char *>(ap[index].c_str()));
            bpAddr[row] = reinterpret_cast<uint8_t *>(const_cast<char *>(bp[index].c_str()));
        }

        bool isAnyNull[] = { false, false };
        BatchConcatStrChar(contextPtr, apAddr.data(), apLen.data(), bpAddr.data(), bWidth[i], bpLen.data(), isAnyNull,
            output.data(), outLen.data(), rowCnt);
        AssertStringEquals(expected, i * rowCnt, rowCnt, output, outLen);
    }
    delete context;
}

TEST(BatchFunctionTest, ConcatStrStrRetNull)
{
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);
    std::vector<std::string> ap { "你是Chinese?", "", "pink圣诞袜" };
    std::vector<int32_t> apLen { 14, 0, 13 };
    std::vector<std::string> bp { "Yes我是", "粉色de圣诞袜", "" };
    std::vector<int32_t> bpLen { 9, 17, 0 };
    int32_t rowCnt = ap.size();
    std::vector<uint8_t *> output(rowCnt);
    std::vector<int32_t> outLen(rowCnt);
    std::vector<uint8_t *> apAddr(rowCnt);
    std::vector<uint8_t *> bpAddr(rowCnt);
    for (int32_t i = 0; i < rowCnt; i++) {
        apAddr[i] = reinterpret_cast<uint8_t *>(const_cast<char *>(ap[i].c_str()));
        bpAddr[i] = reinterpret_cast<uint8_t *>(const_cast<char *>(bp[i].c_str()));
    }
    bool isNull[] = { false, false, false };
    BatchConcatStrStrRetNull(isNull, contextPtr, apAddr.data(), apLen.data(), bpAddr.data(), bpLen.data(),
        output.data(), outLen.data(), rowCnt);
    std::vector<std::string> expected { "你是Chinese?Yes我是", "粉色de圣诞袜", "pink圣诞袜" };
    AssertStringEquals(expected, output, outLen);
    delete context;
}

TEST(BatchFunctionTest, ConcatCharCharRetNull)
{
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);
    std::vector<std::string> ap { "粉色de圣诞袜", "*黑色*",      "Hei你好吗",    "Oh我很好",
        "Hei你好吗   ", "Oh我很好  ",  "   Hei你好吗", "   Oh我很好",
        "Hei   你好吗", "Oh   我很好", "   ",          "Oh我很好",
        "Hei你好吗",    "   ",         "Hei你好吗",    "" };
    std::vector<int32_t> aWidth { 7, 8, 10, 12, 12, 5, 8, 8 };
    std::vector<std::string> bp { "*黑色*",      "粉色de",      "Oh我很好",     "Hei你好吗",   "Oh我很好  ",
        "Hei你好吗  ", "   Oh我很好", "   Hei你好吗", "Oh   我很好", "Hei   你好",
        "Oh我很好   ", "   ",         "   ",          "Hei你好吗",   "",
        "Hei你好" };
    std::vector<int32_t> bWidth { 4, 8, 8, 12, 8, 12, 5, 5 };
    std::vector<std::string> expected { "粉色de圣诞袜*黑色*",
        "*黑色*   粉色de",
        "Hei你好吗  Oh我很好",
        "Oh我很好   Hei你好吗",
        "Hei你好吗    Oh我很好  ",
        "Oh我很好     Hei你好吗  ",
        "   Hei你好吗      Oh我很好",
        "   Oh我很好       Hei你好吗",
        "Hei   你好吗   Oh   我很好",
        "Oh   我很好    Hei   你好",
        "     Oh我很好   ",
        "Oh我很好   ",
        "Hei你好吗     ",
        "        Hei你好吗",
        "Hei你好吗",
        "        Hei你好" };
    int32_t batch = 8;
    int32_t rowCnt = 2;
    for (int32_t i = 0; i < batch; i++) {
        std::vector<uint8_t *> output(rowCnt);
        std::vector<int32_t> outLen(rowCnt);
        std::vector<int32_t> apLen(rowCnt);
        std::vector<int32_t> bpLen(rowCnt);
        std::vector<uint8_t *> apAddr(rowCnt);
        std::vector<uint8_t *> bpAddr(rowCnt);
        for (int32_t row = 0; row < rowCnt; row++) {
            int32_t index = i * rowCnt + row;
            apLen[row] = static_cast<int32_t>(ap[index].length());
            bpLen[row] = static_cast<int32_t>(bp[index].length());
            apAddr[row] = reinterpret_cast<uint8_t *>(const_cast<char *>(ap[index].c_str()));
            bpAddr[row] = reinterpret_cast<uint8_t *>(const_cast<char *>(bp[index].c_str()));
        }
        bool isNull[] = { false, false };
        BatchConcatCharCharRetNull(isNull, contextPtr, apAddr.data(), aWidth[i], apLen.data(), bpAddr.data(), bWidth[i],
            bpLen.data(), output.data(), outLen.data(), rowCnt);
        AssertStringEquals(expected, i * rowCnt, rowCnt, output, outLen);
    }
    delete context;
}

TEST(BatchFunctionTest, ConcatCharStrRetNull)
{
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);
    std::vector<std::string> ap { "*你是谁呢*", "我很OK", "*你是谁呢*", "我很OK", "*你是谁呢*", "" };
    std::vector<int32_t> aWidth { 6, 10, 10 };
    std::vector<std::string> bp { "我很OK", "*你是谁呢*", "我很OK", "*你是谁呢*", "", "*你是谁呢*" };
    std::vector<std::string> expected { "*你是谁呢*我很OK",       "我很OK  *你是谁呢*", "*你是谁呢*    我很OK",
        "我很OK      *你是谁呢*", "*你是谁呢*",         "          *你是谁呢*" };
    int32_t batch = 3;
    int32_t rowCnt = 2;
    for (int32_t i = 0; i < batch; i++) {
        std::vector<uint8_t *> output(rowCnt);
        std::vector<int32_t> outLen(rowCnt);
        std::vector<int32_t> apLen(rowCnt);
        std::vector<int32_t> bpLen(rowCnt);
        std::vector<uint8_t *> apAddr(rowCnt);
        std::vector<uint8_t *> bpAddr(rowCnt);
        for (int32_t row = 0; row < rowCnt; row++) {
            int32_t index = i * rowCnt + row;
            apLen[row] = static_cast<int32_t>(ap[index].length());
            bpLen[row] = static_cast<int32_t>(bp[index].length());
            apAddr[row] = reinterpret_cast<uint8_t *>(const_cast<char *>(ap[index].c_str()));
            bpAddr[row] = reinterpret_cast<uint8_t *>(const_cast<char *>(bp[index].c_str()));
        }

        bool isNull[] = { false, false };
        BatchConcatCharStrRetNull(isNull, contextPtr, apAddr.data(), aWidth[i], apLen.data(), bpAddr.data(),
            bpLen.data(), output.data(), outLen.data(), rowCnt);
        AssertStringEquals(expected, i * rowCnt, rowCnt, output, outLen);
    }
    delete context;
}

TEST(BatchFunctionTest, ConcatStrCharRetNull)
{
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);
    std::vector<std::string> ap { "粉色de圣诞袜", "*黑色*", "粉色de圣诞袜", "*黑色*", "粉色de圣诞袜   ", "" };
    std::vector<std::string> bp { "*黑色*", "粉色de圣诞袜", "*黑色*", "粉色de圣诞袜", "", "粉色de圣诞袜   " };
    std::vector<int32_t> bWidth { 4, 6, 5 };
    std::vector<std::string> expected { "粉色de圣诞袜*黑色*", "*黑色*粉色de圣诞袜", "粉色de圣诞袜*黑色*",
        "*黑色*粉色de圣诞袜", "粉色de圣诞袜   ",    "粉色de圣诞袜   " };
    int32_t batch = 3;
    int32_t rowCnt = 2;
    for (int32_t i = 0; i < batch; i++) {
        std::vector<uint8_t *> output(rowCnt);
        std::vector<int32_t> outLen(rowCnt);
        std::vector<int32_t> apLen(rowCnt);
        std::vector<int32_t> bpLen(rowCnt);
        std::vector<uint8_t *> apAddr(rowCnt);
        std::vector<uint8_t *> bpAddr(rowCnt);
        for (int32_t row = 0; row < rowCnt; row++) {
            int32_t index = i * rowCnt + row;
            apLen[row] = static_cast<int32_t>(ap[index].length());
            bpLen[row] = static_cast<int32_t>(bp[index].length());
            apAddr[row] = reinterpret_cast<uint8_t *>(const_cast<char *>(ap[index].c_str()));
            bpAddr[row] = reinterpret_cast<uint8_t *>(const_cast<char *>(bp[index].c_str()));
        }

        bool isNull[] = { false, false };
        BatchConcatStrCharRetNull(isNull, contextPtr, apAddr.data(), apLen.data(), bpAddr.data(), bWidth[i],
            bpLen.data(), output.data(), outLen.data(), rowCnt);
        AssertStringEquals(expected, i * rowCnt, rowCnt, output, outLen);
    }
    delete context;
}

TEST(BatchFunctionTest, CastStrWithDiffWidths)
{
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);
    std::vector<std::string> srcStr { "cast乌s斯侧后解", "cast乌s斯解", "cast乌s斯侧后解", "cast乌s" };
    std::vector<int32_t> srcWidth { 18, 18 };
    std::vector<int32_t> dstWidth { 1024, 7 };
    std::vector<std::string> expected { "cast乌s斯侧后解", "cast乌s斯解", "cast乌s斯", "cast乌s" };
    int32_t batch = 2;
    int32_t rowCnt = 2;
    for (int32_t i = 0; i < batch; i++) {
        std::vector<uint8_t *> output(rowCnt);
        std::vector<int32_t> outLen(rowCnt);
        std::vector<int32_t> strLen(rowCnt);
        std::vector<uint8_t *> srcAddr(rowCnt);
        for (int32_t row = 0; row < rowCnt; row++) {
            int32_t index = i * rowCnt + row;
            strLen[row] = static_cast<int32_t>(srcStr[index].length());
            srcAddr[row] = reinterpret_cast<uint8_t *>(const_cast<char *>(srcStr[index].c_str()));
        }
        bool isAnyNull[] = { false, false };
        BatchCastStrWithDiffWidths(contextPtr, srcAddr.data(), srcWidth[i], strLen.data(), isAnyNull, output.data(),
            outLen.data(), dstWidth[i], rowCnt);
        AssertStringEquals(expected, i * rowCnt, rowCnt, output, outLen);
    }
    delete context;
}

TEST(BatchFunctionTest, CastStrWithDiffWidthsRetNull)
{
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);
    std::vector<std::string> srcStr { "cast乌s斯侧后解", "cast乌s斯解", "cast乌s斯侧后解", "cast乌s" };
    std::vector<int32_t> srcWidth { 18, 18 };
    std::vector<int32_t> dstWidth { 1024, 7 };
    std::vector<std::string> expected { "cast乌s斯侧后解", "cast乌s斯解", "cast乌s斯", "cast乌s" };
    int32_t batch = 2;
    int32_t rowCnt = 2;
    for (int32_t i = 0; i < batch; i++) {
        std::vector<uint8_t *> output(rowCnt);
        std::vector<int32_t> outLen(rowCnt);
        std::vector<int32_t> strLen(rowCnt);
        std::vector<uint8_t *> srcAddr(rowCnt);
        for (int32_t row = 0; row < rowCnt; row++) {
            int32_t index = i * rowCnt + row;
            strLen[row] = static_cast<int32_t>(srcStr[index].length());
            srcAddr[row] = reinterpret_cast<uint8_t *>(const_cast<char *>(srcStr[index].c_str()));
        }
        bool isNull[] = { false, false };
        BatchCastStrWithDiffWidthsRetNull(isNull, contextPtr, srcAddr.data(), srcWidth[i], strLen.data(), output.data(),
            outLen.data(), dstWidth[i], rowCnt);
        AssertStringEquals(expected, i * rowCnt, rowCnt, output, outLen);
    }
    delete context;
}

TEST(BatchFunctionTest, BatchInstr)
{
    std::vector<std::string> srcStrVec { "", "", "abc", "abc", "abc", "abc", "", "abc", "" };
    std::vector<std::string> subStrVec { "", "abc", "", "abcd", "bd", "bc", "ab", "", "" };
    int32_t rowCnt = static_cast<int32_t>(srcStrVec.size());
    char *srcStrs[rowCnt];
    int32_t srcLens[rowCnt];
    char *subStrs[rowCnt];
    int32_t subLens[rowCnt];
    bool isAnyNull[] = {true, true, true, false, false, false, false, false, false};
    int32_t output[rowCnt];
    for (int32_t row = 0; row < rowCnt; row++) {
        srcStrs[row] = const_cast<char *>(srcStrVec[row].c_str());
        srcLens[row] = srcStrVec[row].length();
        subStrs[row] = const_cast<char *>(subStrVec[row].c_str());
        subLens[row] = subStrVec[row].length();
    }
    BatchInStr(srcStrs, srcLens, subStrs, subLens, isAnyNull, output, rowCnt);
    std::vector<int32_t> result(output, output + rowCnt);
    std::vector<int32_t> expect { 0, 0, 0, 0, 0, 2, 0, 1, 1 };
    AssertIntEquals(expect, result);
}

TEST(BatchFunctionTest, BatchStartsWithStr)
{
    std::vector<std::string> srcStrVec { "", "", "abc", "abc", "abc", "abc", "", "abc", "" };
    std::vector<std::string> matchStrVec { "ab", "ab", "ab", "ab", "ab", "ab", "ab", "ab", "ab" };
    int32_t rowCnt = static_cast<int32_t>(srcStrVec.size());
    char *srcStrs[rowCnt];
    int32_t srcLens[rowCnt];
    char *matchStrs[rowCnt];
    int32_t matchLens[rowCnt];
    bool isAnyNull[] = {true, true, false, false, false, false, false, false, false};
    bool output[rowCnt];
    for (int32_t row = 0; row < rowCnt; row++) {
        srcStrs[row] = const_cast<char *>(srcStrVec[row].c_str());
        srcLens[row] = srcStrVec[row].length();
        matchStrs[row] = const_cast<char *>(matchStrVec[row].c_str());
        matchLens[row] = matchStrVec[row].length();
    }
    BatchStartsWithStr(srcStrs, srcLens, matchStrs, matchLens, isAnyNull, output, rowCnt);
    std::vector<bool> expect { false, false, true, true, true, true, false, true, false };
    AssertBoolEquals(expect, output);
}

TEST(BatchFunctionTest, BatchEndsWithStr)
{
    std::vector<std::string> srcStrVec { "", "", "abc", "abc", "abc", "abc", "", "abc", "" };
    std::vector<std::string> matchStrVec { "bc", "bc", "bc", "bc", "bc", "bc", "bc", "bc", "bc" };
    int32_t rowCnt = static_cast<int32_t>(srcStrVec.size());
    char *srcStrs[rowCnt];
    int32_t srcLens[rowCnt];
    char *matchStrs[rowCnt];
    int32_t matchLens[rowCnt];
    bool isAnyNull[] = {true, true, false, false, false, false, false, false, false};
    bool output[rowCnt];
    for (int32_t row = 0; row < rowCnt; row++) {
        srcStrs[row] = const_cast<char *>(srcStrVec[row].c_str());
        srcLens[row] = srcStrVec[row].length();
        matchStrs[row] = const_cast<char *>(matchStrVec[row].c_str());
        matchLens[row] = matchStrVec[row].length();
    }
    BatchEndsWithStr(srcStrs, srcLens, matchStrs, matchLens, isAnyNull, output, rowCnt);
    std::vector<bool> expect { false, false, true, true, true, true, false, true, false };
    AssertBoolEquals(expect, output);
}

TEST(BatchFunctionTest, BatchJsonSplitStr)
{
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);
    std::vector<std::string> jsonStr { "[\"a\",\"b\"]", "invalid json", "[\"only\"]" };
    std::vector<int32_t> jsonStrLen {
        static_cast<int32_t>(jsonStr[0].length()),
        static_cast<int32_t>(jsonStr[1].length()),
        static_cast<int32_t>(jsonStr[2].length()) };
    std::vector<uint8_t *> jsonStrAddr {
        reinterpret_cast<uint8_t *>(const_cast<char *>(jsonStr[0].c_str())),
        reinterpret_cast<uint8_t *>(const_cast<char *>(jsonStr[1].c_str())),
        reinterpret_cast<uint8_t *>(const_cast<char *>(jsonStr[2].c_str())) };
    int32_t rowCnt = static_cast<int32_t>(jsonStr.size());
    bool jsonStrIsNull[] = { false, false, true };
    bool outIsNull[] = { false, false, false };
    std::vector<uint8_t *> output(rowCnt);
    std::vector<int32_t> outLen(rowCnt);

    BatchJsonSplitStr(contextPtr, jsonStrAddr.data(), jsonStrLen.data(), jsonStrIsNull, outIsNull, output.data(),
        outLen.data(), rowCnt);

    EXPECT_FALSE(outIsNull[0]);
    EXPECT_EQ(std::string(reinterpret_cast<char *>(output[0]), outLen[0]), "a\r\nb");
    EXPECT_TRUE(outIsNull[1]);
    EXPECT_EQ(output[1], nullptr);
    EXPECT_TRUE(outIsNull[2]);
    EXPECT_EQ(output[2], nullptr);
    delete context;
}

TEST(BatchFunctionTest, BatchJsonValue)
{
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);
    std::vector<std::string> jsonStr { R"({"name":"John","age":30})", R"({"name":"John"})", R"({"value":null})" };
    std::vector<int32_t> jsonStrLen {
        static_cast<int32_t>(jsonStr[0].length()),
        static_cast<int32_t>(jsonStr[1].length()),
        static_cast<int32_t>(jsonStr[2].length()) };
    std::vector<uint8_t *> jsonStrAddr {
        reinterpret_cast<uint8_t *>(const_cast<char *>(jsonStr[0].c_str())),
        reinterpret_cast<uint8_t *>(const_cast<char *>(jsonStr[1].c_str())),
        reinterpret_cast<uint8_t *>(const_cast<char *>(jsonStr[2].c_str())) };
    std::vector<std::string> pathStr { "$.name", "$.age", "$.value" };
    std::vector<int32_t> pathStrLen {
        static_cast<int32_t>(pathStr[0].length()),
        static_cast<int32_t>(pathStr[1].length()),
        static_cast<int32_t>(pathStr[2].length()) };
    std::vector<uint8_t *> pathStrAddr {
        reinterpret_cast<uint8_t *>(const_cast<char *>(pathStr[0].c_str())),
        reinterpret_cast<uint8_t *>(const_cast<char *>(pathStr[1].c_str())),
        reinterpret_cast<uint8_t *>(const_cast<char *>(pathStr[2].c_str())) };
    int32_t rowCnt = static_cast<int32_t>(jsonStr.size());
    bool jsonStrIsNull[] = { false, false, false };
    bool pathStrIsNull[] = { false, false, false };
    bool outIsNull[] = { false, false, false };
    std::vector<uint8_t *> output(rowCnt);
    std::vector<int32_t> outLen(rowCnt);

    BatchJsonValue(contextPtr, jsonStrAddr.data(), jsonStrLen.data(), jsonStrIsNull, pathStrAddr.data(), 0,
        pathStrLen.data(), pathStrIsNull, outIsNull, output.data(), outLen.data(), rowCnt);

    EXPECT_FALSE(outIsNull[0]);
    EXPECT_EQ(std::string(reinterpret_cast<char *>(output[0]), outLen[0]), "John");
    EXPECT_TRUE(outIsNull[1]);
    EXPECT_EQ(output[1], nullptr);
    EXPECT_TRUE(outIsNull[2]);
    EXPECT_EQ(output[2], nullptr);
    delete context;
}

TEST(BatchFunctionTest, BatchJsonValueWithBehaviors)
{
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);
    std::vector<std::string> jsonStr { R"({"name":"John"})", "invalid json" };
    std::vector<int32_t> jsonStrLen {
        static_cast<int32_t>(jsonStr[0].length()),
        static_cast<int32_t>(jsonStr[1].length()) };
    std::vector<uint8_t *> jsonStrAddr {
        reinterpret_cast<uint8_t *>(const_cast<char *>(jsonStr[0].c_str())),
        reinterpret_cast<uint8_t *>(const_cast<char *>(jsonStr[1].c_str())) };
    std::vector<std::string> pathStr { "$.age", "$.name" };
    std::vector<int32_t> pathStrLen {
        static_cast<int32_t>(pathStr[0].length()),
        static_cast<int32_t>(pathStr[1].length()) };
    std::vector<uint8_t *> pathStrAddr {
        reinterpret_cast<uint8_t *>(const_cast<char *>(pathStr[0].c_str())),
        reinterpret_cast<uint8_t *>(const_cast<char *>(pathStr[1].c_str())) };
    std::vector<std::string> defaultOnEmpty { "missing", "" };
    std::vector<std::string> defaultOnError { "", "bad_json" };
    std::vector<int32_t> defaultOnEmptyLen {
        static_cast<int32_t>(defaultOnEmpty[0].length()),
        static_cast<int32_t>(defaultOnEmpty[1].length()) };
    std::vector<int32_t> defaultOnErrorLen {
        static_cast<int32_t>(defaultOnError[0].length()),
        static_cast<int32_t>(defaultOnError[1].length()) };
    std::vector<uint8_t *> defaultOnEmptyAddr {
        reinterpret_cast<uint8_t *>(const_cast<char *>(defaultOnEmpty[0].c_str())),
        reinterpret_cast<uint8_t *>(const_cast<char *>(defaultOnEmpty[1].c_str())) };
    std::vector<uint8_t *> defaultOnErrorAddr {
        reinterpret_cast<uint8_t *>(const_cast<char *>(defaultOnError[0].c_str())),
        reinterpret_cast<uint8_t *>(const_cast<char *>(defaultOnError[1].c_str())) };
    int32_t emptyBehavior[] = { 2, 0 };
    int32_t errorBehavior[] = { 0, 2 };
    bool jsonStrIsNull[] = { false, false };
    bool pathStrIsNull[] = { false, false };
    bool emptyBehaviorIsNull[] = { false, false };
    bool errorBehaviorIsNull[] = { false, false };
    bool defaultOnEmptyIsNull[] = { false, true };
    bool defaultOnErrorIsNull[] = { true, false };
    bool outIsNull[] = { false, false };
    std::vector<uint8_t *> output(2);
    std::vector<int32_t> outLen(2);

    BatchJsonValueWithBehaviors(contextPtr, jsonStrAddr.data(), jsonStrLen.data(), jsonStrIsNull, pathStrAddr.data(), 0,
        pathStrLen.data(), pathStrIsNull, emptyBehavior, emptyBehaviorIsNull, defaultOnEmptyAddr.data(),
        defaultOnEmptyLen.data(), defaultOnEmptyIsNull, errorBehavior, errorBehaviorIsNull, defaultOnErrorAddr.data(),
        defaultOnErrorLen.data(), defaultOnErrorIsNull, outIsNull, output.data(), outLen.data(), 2);

    EXPECT_FALSE(outIsNull[0]);
    EXPECT_EQ(std::string(reinterpret_cast<char *>(output[0]), outLen[0]), "missing");
    EXPECT_FALSE(outIsNull[1]);
    EXPECT_EQ(std::string(reinterpret_cast<char *>(output[1]), outLen[1]), "bad_json");
    delete context;
}

TEST(BatchFunctionTest, BatchJsonValueComplexTypesReturnNull)
{
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);
    std::vector<std::string> jsonStr { R"({"data":{"x":1}})", R"({"items":[1,2,3]})", R"({"flag":true})" };
    std::vector<int32_t> jsonStrLen {
        static_cast<int32_t>(jsonStr[0].length()),
        static_cast<int32_t>(jsonStr[1].length()),
        static_cast<int32_t>(jsonStr[2].length()) };
    std::vector<uint8_t *> jsonStrAddr {
        reinterpret_cast<uint8_t *>(const_cast<char *>(jsonStr[0].c_str())),
        reinterpret_cast<uint8_t *>(const_cast<char *>(jsonStr[1].c_str())),
        reinterpret_cast<uint8_t *>(const_cast<char *>(jsonStr[2].c_str())) };
    std::vector<std::string> pathStr { "$.data", "$.items", "$.flag" };
    std::vector<int32_t> pathStrLen {
        static_cast<int32_t>(pathStr[0].length()),
        static_cast<int32_t>(pathStr[1].length()),
        static_cast<int32_t>(pathStr[2].length()) };
    std::vector<uint8_t *> pathStrAddr {
        reinterpret_cast<uint8_t *>(const_cast<char *>(pathStr[0].c_str())),
        reinterpret_cast<uint8_t *>(const_cast<char *>(pathStr[1].c_str())),
        reinterpret_cast<uint8_t *>(const_cast<char *>(pathStr[2].c_str())) };
    int32_t rowCnt = static_cast<int32_t>(jsonStr.size());
    bool jsonStrIsNull[] = { false, false, false };
    bool pathStrIsNull[] = { false, false, false };
    bool outIsNull[] = { false, false, false };
    std::vector<uint8_t *> output(rowCnt);
    std::vector<int32_t> outLen(rowCnt);

    BatchJsonValue(contextPtr, jsonStrAddr.data(), jsonStrLen.data(), jsonStrIsNull, pathStrAddr.data(), 0,
        pathStrLen.data(), pathStrIsNull, outIsNull, output.data(), outLen.data(), rowCnt);

    EXPECT_TRUE(outIsNull[0]);
    EXPECT_EQ(output[0], nullptr);
    EXPECT_TRUE(outIsNull[1]);
    EXPECT_EQ(output[1], nullptr);
    EXPECT_FALSE(outIsNull[2]);
    EXPECT_EQ(std::string(reinterpret_cast<char *>(output[2]), outLen[2]), "true");
    delete context;
}

TEST(BatchFunctionTest, BatchJsonValueWithBehaviorsComplexTypesUseEmptyBehavior)
{
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);
    std::vector<std::string> jsonStr { R"({"data":{"x":1}})", R"({"items":[1,2,3]})" };
    std::vector<int32_t> jsonStrLen {
        static_cast<int32_t>(jsonStr[0].length()),
        static_cast<int32_t>(jsonStr[1].length()) };
    std::vector<uint8_t *> jsonStrAddr {
        reinterpret_cast<uint8_t *>(const_cast<char *>(jsonStr[0].c_str())),
        reinterpret_cast<uint8_t *>(const_cast<char *>(jsonStr[1].c_str())) };
    std::vector<std::string> pathStr { "$.data", "$.items" };
    std::vector<int32_t> pathStrLen {
        static_cast<int32_t>(pathStr[0].length()),
        static_cast<int32_t>(pathStr[1].length()) };
    std::vector<uint8_t *> pathStrAddr {
        reinterpret_cast<uint8_t *>(const_cast<char *>(pathStr[0].c_str())),
        reinterpret_cast<uint8_t *>(const_cast<char *>(pathStr[1].c_str())) };
    std::vector<std::string> defaultOnEmpty { "missing", "" };
    std::vector<std::string> defaultOnError { "", "" };
    std::vector<int32_t> defaultOnEmptyLen {
        static_cast<int32_t>(defaultOnEmpty[0].length()),
        static_cast<int32_t>(defaultOnEmpty[1].length()) };
    std::vector<int32_t> defaultOnErrorLen {
        static_cast<int32_t>(defaultOnError[0].length()),
        static_cast<int32_t>(defaultOnError[1].length()) };
    std::vector<uint8_t *> defaultOnEmptyAddr {
        reinterpret_cast<uint8_t *>(const_cast<char *>(defaultOnEmpty[0].c_str())),
        reinterpret_cast<uint8_t *>(const_cast<char *>(defaultOnEmpty[1].c_str())) };
    std::vector<uint8_t *> defaultOnErrorAddr {
        reinterpret_cast<uint8_t *>(const_cast<char *>(defaultOnError[0].c_str())),
        reinterpret_cast<uint8_t *>(const_cast<char *>(defaultOnError[1].c_str())) };
    int32_t emptyBehavior[] = { 2, 0 };
    int32_t errorBehavior[] = { 0, 0 };
    bool jsonStrIsNull[] = { false, false };
    bool pathStrIsNull[] = { false, false };
    bool emptyBehaviorIsNull[] = { false, false };
    bool errorBehaviorIsNull[] = { false, false };
    bool defaultOnEmptyIsNull[] = { false, true };
    bool defaultOnErrorIsNull[] = { true, true };
    bool outIsNull[] = { false, false };
    std::vector<uint8_t *> output(2);
    std::vector<int32_t> outLen(2);

    BatchJsonValueWithBehaviors(contextPtr, jsonStrAddr.data(), jsonStrLen.data(), jsonStrIsNull, pathStrAddr.data(), 0,
        pathStrLen.data(), pathStrIsNull, emptyBehavior, emptyBehaviorIsNull, defaultOnEmptyAddr.data(),
        defaultOnEmptyLen.data(), defaultOnEmptyIsNull, errorBehavior, errorBehaviorIsNull, defaultOnErrorAddr.data(),
        defaultOnErrorLen.data(), defaultOnErrorIsNull, outIsNull, output.data(), outLen.data(), 2);

    EXPECT_FALSE(outIsNull[0]);
    EXPECT_EQ(std::string(reinterpret_cast<char *>(output[0]), outLen[0]), "missing");
    EXPECT_TRUE(outIsNull[1]);
    EXPECT_EQ(output[1], nullptr);
    delete context;
}

TEST(BatchFunctionTest, BatchJsonQuery)
{
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);
    std::vector<std::string> jsonStr {
        R"({"roomInfos":{"id":1,"name":"A"}})",
        R"({"roomInfos":{"name":"A"}})",
        R"({"roomInfos":[]})" };
    std::vector<int32_t> jsonStrLen {
        static_cast<int32_t>(jsonStr[0].length()),
        static_cast<int32_t>(jsonStr[1].length()),
        static_cast<int32_t>(jsonStr[2].length()) };
    std::vector<uint8_t *> jsonStrAddr {
        reinterpret_cast<uint8_t *>(const_cast<char *>(jsonStr[0].c_str())),
        reinterpret_cast<uint8_t *>(const_cast<char *>(jsonStr[1].c_str())),
        reinterpret_cast<uint8_t *>(const_cast<char *>(jsonStr[2].c_str())) };
    std::vector<std::string> pathStr { "$.roomInfos", "$.roomInfos.name", "$.missing" };
    std::vector<int32_t> pathStrLen {
        static_cast<int32_t>(pathStr[0].length()),
        static_cast<int32_t>(pathStr[1].length()),
        static_cast<int32_t>(pathStr[2].length()) };
    std::vector<uint8_t *> pathStrAddr {
        reinterpret_cast<uint8_t *>(const_cast<char *>(pathStr[0].c_str())),
        reinterpret_cast<uint8_t *>(const_cast<char *>(pathStr[1].c_str())),
        reinterpret_cast<uint8_t *>(const_cast<char *>(pathStr[2].c_str())) };
    int32_t rowCnt = static_cast<int32_t>(jsonStr.size());
    bool jsonStrIsNull[] = { false, false, false };
    bool pathStrIsNull[] = { false, false, false };
    bool outIsNull[] = { false, false, false };
    std::vector<uint8_t *> output(rowCnt);
    std::vector<int32_t> outLen(rowCnt);

    BatchJsonQuery(contextPtr, jsonStrAddr.data(), jsonStrLen.data(), jsonStrIsNull, pathStrAddr.data(), 0,
        pathStrLen.data(), pathStrIsNull, outIsNull, output.data(), outLen.data(), rowCnt);

    EXPECT_FALSE(outIsNull[0]);
    EXPECT_EQ(std::string(reinterpret_cast<char *>(output[0]), outLen[0]), R"({"id":1,"name":"A"})");
    EXPECT_TRUE(outIsNull[1]);
    EXPECT_EQ(output[1], nullptr);
    EXPECT_TRUE(outIsNull[2]);
    EXPECT_EQ(output[2], nullptr);
    delete context;
}

TEST(BatchFunctionTest, BatchJsonQueryWithWrapperAndBehavior)
{
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);
    std::vector<std::string> jsonStr {
        R"({"roomInfos":{"name":"A"}})",
        R"({"roomInfos":[]})",
        R"({"roomInfos":[]})" };
    std::vector<int32_t> jsonStrLen {
        static_cast<int32_t>(jsonStr[0].length()),
        static_cast<int32_t>(jsonStr[1].length()),
        static_cast<int32_t>(jsonStr[2].length()) };
    std::vector<uint8_t *> jsonStrAddr {
        reinterpret_cast<uint8_t *>(const_cast<char *>(jsonStr[0].c_str())),
        reinterpret_cast<uint8_t *>(const_cast<char *>(jsonStr[1].c_str())),
        reinterpret_cast<uint8_t *>(const_cast<char *>(jsonStr[2].c_str())) };
    std::vector<std::string> pathStr {
        "$.roomInfos.name",
        "lax $.missing",
        "strict $.missing" };
    std::vector<int32_t> pathStrLen {
        static_cast<int32_t>(pathStr[0].length()),
        static_cast<int32_t>(pathStr[1].length()),
        static_cast<int32_t>(pathStr[2].length()) };
    std::vector<uint8_t *> pathStrAddr {
        reinterpret_cast<uint8_t *>(const_cast<char *>(pathStr[0].c_str())),
        reinterpret_cast<uint8_t *>(const_cast<char *>(pathStr[1].c_str())),
        reinterpret_cast<uint8_t *>(const_cast<char *>(pathStr[2].c_str())) };
    int32_t wrapperBehavior[] = { 1, 0, 0 };
    int32_t emptyBehavior[] = { 0, 1, 0 };
    int32_t errorBehavior[] = { 0, 0, 2 };
    bool jsonStrIsNull[] = { false, false, false };
    bool pathStrIsNull[] = { false, false, false };
    bool wrapperBehaviorIsNull[] = { false, false, false };
    bool emptyBehaviorIsNull[] = { false, false, false };
    bool errorBehaviorIsNull[] = { false, false, false };
    bool outIsNull[] = { false, false, false };
    std::vector<uint8_t *> output(3);
    std::vector<int32_t> outLen(3);

    BatchJsonQueryWithWrapperAndBehavior(contextPtr, jsonStrAddr.data(), jsonStrLen.data(), jsonStrIsNull,
        pathStrAddr.data(), 0, pathStrLen.data(), pathStrIsNull, wrapperBehavior, wrapperBehaviorIsNull,
        emptyBehavior, emptyBehaviorIsNull, errorBehavior, errorBehaviorIsNull, outIsNull, output.data(),
        outLen.data(), 3);

    EXPECT_FALSE(outIsNull[0]);
    EXPECT_EQ(std::string(reinterpret_cast<char *>(output[0]), outLen[0]), R"(["A"])");
    EXPECT_FALSE(outIsNull[1]);
    EXPECT_EQ(std::string(reinterpret_cast<char *>(output[1]), outLen[1]), "[]");
    EXPECT_FALSE(outIsNull[2]);
    EXPECT_EQ(std::string(reinterpret_cast<char *>(output[2]), outLen[2]), "{}");
    EXPECT_FALSE(context->HasError());
    delete context;
}

TEST(BatchFunctionTest, BatchJsonSplitChar)
{
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);
    std::vector<std::string> jsonStr { "[\"'dIPFMXtJL\"]" };
    std::vector<int32_t> jsonStrLen { static_cast<int32_t>(jsonStr[0].length()) };
    std::vector<uint8_t *> jsonStrAddr {
        reinterpret_cast<uint8_t *>(const_cast<char *>(jsonStr[0].c_str())) };
    int32_t rowCnt = 1;
    bool jsonStrIsNull[] = { false };
    bool outIsNull[] = { false };
    std::vector<uint8_t *> output(rowCnt);
    std::vector<int32_t> outLen(rowCnt);

    BatchJsonSplitChar(contextPtr, jsonStrAddr.data(), 1, jsonStrLen.data(), jsonStrIsNull, outIsNull, output.data(),
        outLen.data(), rowCnt);

    EXPECT_FALSE(outIsNull[0]);
    EXPECT_EQ(std::string(reinterpret_cast<char *>(output[0]), outLen[0]), "'dIPFMXtJL");
    delete context;
}

TEST(BatchFunctionTest, BatchJsonSplitFloatFormatting)
{
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);
    std::vector<std::string> jsonStr { "[1.2,3.14,-2.5]" };
    std::vector<int32_t> jsonStrLen { static_cast<int32_t>(jsonStr[0].length()) };
    std::vector<uint8_t *> jsonStrAddr {
        reinterpret_cast<uint8_t *>(const_cast<char *>(jsonStr[0].c_str())) };
    bool jsonStrIsNull[] = { false };
    bool outIsNull[] = { false };
    std::vector<uint8_t *> output(1);
    std::vector<int32_t> outLen(1);

    BatchJsonSplitStr(contextPtr, jsonStrAddr.data(), jsonStrLen.data(), jsonStrIsNull, outIsNull, output.data(),
        outLen.data(), 1);

    EXPECT_FALSE(outIsNull[0]);
    EXPECT_EQ(std::string(reinterpret_cast<char *>(output[0]), outLen[0]), "1.2\r\n3.14\r\n-2.5");
    delete context;
}

TEST(BatchFunctionTest, BatchJsonSplitScientificNotationFormatting)
{
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);
    std::vector<std::string> jsonStr { "[1.171815966525443e+77]" };
    std::vector<int32_t> jsonStrLen { static_cast<int32_t>(jsonStr[0].length()) };
    std::vector<uint8_t *> jsonStrAddr {
        reinterpret_cast<uint8_t *>(const_cast<char *>(jsonStr[0].c_str())) };
    bool jsonStrIsNull[] = { false };
    bool outIsNull[] = { false };
    std::vector<uint8_t *> output(1);
    std::vector<int32_t> outLen(1);

    BatchJsonSplitStr(contextPtr, jsonStrAddr.data(), jsonStrLen.data(), jsonStrIsNull, outIsNull, output.data(),
        outLen.data(), 1);

    EXPECT_FALSE(outIsNull[0]);
    EXPECT_EQ(std::string(reinterpret_cast<char *>(output[0]), outLen[0]), "1.171815966525443E+77");
    delete context;
}

TEST(BatchFunctionTest, BatchCastStringToDate)
{
    // year-month-day
    ConfigUtil::SetStringToDateFormatRule(StringToDateFormatRule::ALLOW_REDUCED_PRECISION);
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);
    std::vector<std::string> srcStr { "1970-01-03", "1969-12-31", "1453-05-29", "   1453-05-29   ",
        "   1 453-05-29   " };
    bool isNull[] = { false, false, false, false, false };
    std::vector<int32_t> expected { 2, -1, -188682, -188682, 0 };
    int32_t rowCnt = 5;
    std::vector<int32_t> output(rowCnt);

    std::vector<int32_t> strLen(rowCnt);
    std::vector<uint8_t *> srcAddr(rowCnt);
    for (int32_t row = 0; row < rowCnt; row++) {
        strLen[row] = static_cast<int32_t>(srcStr[row].length());
        srcAddr[row] = reinterpret_cast<uint8_t *>(const_cast<char *>(srcStr[row].c_str()));
    }
    BatchCastStringToDateAllowReducePrecison(contextPtr, srcAddr.data(), strLen.data(), isNull, output.data(), rowCnt);
    AssertIntEquals(expected, output);

    BatchCastStringToDateRetNullAllowReducePrecison(isNull, srcAddr.data(), strLen.data(), output.data(), rowCnt);
    AssertIntEquals(expected, output);
    ConfigUtil::SetStringToDateFormatRule(StringToDateFormatRule::NOT_ALLOW_REDUCED_PRECISION);
    delete context;
}

TEST(BatchFunctionTest, BatchCastStringToInt)
{
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);
    std::vector<std::string> srcStr = { "2147483648",
        "2123123123147483648",
        "-2123123123147483648",
        "-2123123123147-483648",
        "+45",
        "-45",
        "3.14159",
        "31337 a",
        "+12345678901",
        "-12345678901",
        "2147483647.2",
        "2147483648.2",
        "     2147483647.2    ",
        "    a 2147483647.2    ",
        "     2147483647.2   a ",
        ".",
        ".2",
        "0.",
        "2.3e3",
        "-1e+2",
        "+.",
        "-.",
        "- .",
        "",
        "    ",
        "  +   ",
        "-",
        "-123.a",
        "-123." };
    bool isAnyNull[] = { false, false, false, false, false, false, false, false, false, false, false, false, false,
                        false, false, false, false, false, false, false, false, false, false, false, false, false,
                        false, false, false };

    std::vector<int32_t> expected { 0, 0, 0, 0, 45, -45, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,  0,   0, 0, 0, 0, 0, 0, 0, 0 };
    int32_t rowCnt = static_cast<int32_t>(srcStr.size());
    std::vector<int32_t> output(rowCnt);

    std::vector<int32_t> strLen(rowCnt);
    std::vector<uint8_t *> srcAddr(rowCnt);
    for (int32_t row = 0; row < rowCnt; row++) {
        strLen[row] = static_cast<int32_t>(srcStr[row].length());
        srcAddr[row] = reinterpret_cast<uint8_t *>(const_cast<char *>(srcStr[row].c_str()));
    }
    BatchCastStringToInt(contextPtr, srcAddr.data(), strLen.data(), isAnyNull, output.data(), rowCnt);
    AssertIntEquals(expected, output);
    EXPECT_TRUE(context->HasError());
    delete context;
}

TEST(BatchFunctionTest, BatchCastStringToLong)
{
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);

    std::vector<std::string> srcStr { "23423",
        "100123",
        "-10078",
        "123.123",
        "9223372036854775807",
        "9223372036854775808",
        "-9223372036854775808",
        "-9223372036854775809" };
    bool isAnyNull[] = { false, false, false, false, false, false, false, false };
    std::vector<int64_t> expected { 23423, 100123, -10078, 0, INT64_MAX, 0, INT64_MIN, 0 };
    int32_t rowCnt = 8;
    std::vector<int64_t> output(rowCnt);

    std::vector<int32_t> strLen(rowCnt);
    std::vector<uint8_t *> srcAddr(rowCnt);
    for (int32_t row = 0; row < rowCnt; row++) {
        strLen[row] = static_cast<int32_t>(srcStr[row].length());
        srcAddr[row] = reinterpret_cast<uint8_t *>(const_cast<char *>(srcStr[row].c_str()));
    }
    BatchCastStringToLong(contextPtr, srcAddr.data(), strLen.data(), isAnyNull, output.data(), rowCnt);
    AssertLongEquals(expected, output);
    delete context;
}


TEST(BatchFunctionTest, BatchCastStringToIntRetNull)
{
    std::vector<std::string> srcStr = { "2147483648",
        "2123123123147483648",
        "-2123123123147483648",
        "-2123123123147-483648",
        "+45",
        "-45",
        "3.14159",
        "31337 a",
        "+12345678901",
        "-12345678901",
        "2147483647.2",
        "2147483648.2",
        "     2147483647.2    ",
        "    a 2147483647.2    ",
        "     2147483647.2   a ",
        ".",
        ".2",
        "0.",
        "2.3e3",
        "-1e+2",
        "+.",
        "-.",
        "- .",
        "",
        "    ",
        "  +   ",
        "-",
        "-123.a",
        "-123." };
    int32_t rowCnt = static_cast<int32_t>(srcStr.size());
    std::vector<int32_t> strLen(rowCnt);
    std::vector<uint8_t *> srcAddr(rowCnt);
    for (int32_t row = 0; row < rowCnt; row++) {
        strLen[row] = static_cast<int32_t>(srcStr[row].length());
        srcAddr[row] = reinterpret_cast<uint8_t *>(const_cast<char *>(srcStr[row].c_str()));
    }

    bool isNull[rowCnt];
    std::vector<int32_t> output(rowCnt);
    BatchCastStringToIntRetNull(isNull, srcAddr.data(), strLen.data(), output.data(), rowCnt);

    std::vector<int32_t> expected { 0, 0, 0, 0, 45, -45, 3, 0, 0, 0, 2147483647, 0, 2147483647, 0,   0,
        0, 0, 0, 0, 0,  0,   0, 0, 0, 0, 0,          0, 0,          -123 };
    std::vector<bool> expectedNulls { true,  true,  true,  true, false, false, false, true,  true, true,
        false, true,  false, true, true,  false, false, false, true, true,
        false, false, true,  true, true,  true,  true,  true,  false };

    AssertIntEquals(expected, output);
    AssertBoolEquals(expectedNulls, isNull);
}

TEST(BatchFunctionTest, BatchCastStringToLongRetNull)
{
    std::vector<std::string> srcStr = { "23423",
        "123.123",
        "2147483648",
        "2a147483648",
        "-10078",
        std::to_string(std::numeric_limits<int64_t>::min()),
        std::to_string(std::numeric_limits<int64_t>::max()),
        std::to_string(std::numeric_limits<uint64_t>::max()),
        "-9223372036854775808",
        "9223372036854775807",
        "-9223372036854775818",
        "9223372036854775817",
        "-2123123123147-483648",
        "     2147483647.2    ",
        "    a 2147483647.2    ",
        "     2147483647.2   a ",
        ".",
        ".2",
        "0.",
        "2.3e3",
        "-1e+2" };


    int32_t rowCnt = static_cast<int32_t>(srcStr.size());
    std::vector<int32_t> strLen(rowCnt);
    std::vector<uint8_t *> srcAddr(rowCnt);
    for (int32_t row = 0; row < rowCnt; row++) {
        strLen[row] = static_cast<int32_t>(srcStr[row].length());
        srcAddr[row] = reinterpret_cast<uint8_t *>(const_cast<char *>(srcStr[row].c_str()));
    }

    bool isNull[rowCnt];
    std::vector<int64_t> output(rowCnt);
    BatchCastStringToLongRetNull(isNull, srcAddr.data(), strLen.data(), output.data(), rowCnt);

    std::vector<int64_t> expected { 23423,
        123,
        2147483648,
        0,
        -10078,
        std::numeric_limits<int64_t>::min(),
        std::numeric_limits<int64_t>::max(),
        0,
        INT64_MIN,
        INT64_MAX,
        0,
        0,
        0,
        2147483647,
        0,
        0,
        0,
        0,
        0,
        0,
        0 };
    std::vector<bool> expectedNulls { false, false, false, true, false, false, false, true,  false, false, true,
        true,  true,  false, true, true,  false, false, false, true,  true };
    AssertLongEquals(expected, output);
    AssertBoolEquals(expectedNulls, isNull);
}

// date time functions
TEST(BatchFunctionTest, BatchUnixTimestampFromStr)
{
    const int32_t rowCnt = 6;
    std::string timeStrs[] = {"2024-10-12", "1948-01-12", "2023-12-09", "",
                              "1989-07-10 11:10:09", "1985-06-29 00:04:49"};
    std::string fmtStrs[] = {"%Y-%m-%d", "%Y-%m-%d", "%Y-%m-%d", "", "%Y-%m-%d %H:%M:%S", "%Y-%m-%d %H:%M:%S"};
    std::string tzStrs[] = {"Asia/Shanghai", "Asia/Shanghai", "Asia/Shanghai", "Asia/Shanghai",
                          "Asia/Shanghai", "Asia/Shanghai"};
    std::string policyStrs[] = {"CORRECTED", "CORRECTED", "CORRECTED", "CORRECTED", "CORRECTED", "CORRECTED"};
    const char *timeStrPtrs[rowCnt];
    int32_t timeLens[rowCnt];
    const char *fmtStrPtrs[rowCnt];
    int32_t fmtLens[rowCnt];
    const char *tzStrPtrs[rowCnt];
    int32_t tzLens[rowCnt];
    const char *policyStrPtrs[rowCnt];
    int32_t policyLens[rowCnt];
    for (int32_t i = 0; i < rowCnt; i++) {
        timeStrPtrs[i] = const_cast<char *>(timeStrs[i].c_str());
        timeLens[i] = timeStrs[i].length();
        fmtStrPtrs[i] = fmtStrs[i].c_str();
        fmtLens[i] = fmtStrs[i].length();
        tzStrPtrs[i] = tzStrs[i].c_str();
        tzLens[i] = tzStrs[i].length();
        policyStrPtrs[i] = policyStrs[i].c_str();
        policyLens[i] = policyStrs[i].length();
    }
    bool isNullTimeStr[] = {false, false, false, true, false, false};
    bool isNullFmtStr[] = {false, false, false, true, false, false};
    bool isNullTzStr[] = {false, false, false, false, false, false};
    bool isNullPolStr[] = {false, false, false, false, false, false};
    bool retIsNull[rowCnt] = {false};
    int64_t output[rowCnt];
    BatchUnixTimestampFromStr(timeStrPtrs, timeLens, isNullTimeStr, fmtStrPtrs, fmtLens, isNullFmtStr, tzStrPtrs,
                              tzLens, isNullTzStr, policyStrPtrs, policyLens, isNullPolStr, retIsNull, output, rowCnt);
    std::vector<bool> expectIsNull = {false, false, false, true, false, false};
    AssertBoolEquals(expectIsNull, retIsNull);
    std::vector<int64_t> result(output, output + rowCnt);
    std::vector<int64_t> expect = { 1728662400, -693388800, 1702051200, 0, 616039809, 488822689 };
    AssertLongEquals(expect, result);
}

TEST(BatchFunctionTest, BatchUnixTimestampFromDate)
{
    const int32_t rowCnt = 3;
    int32_t dates[] = {7130, 5658, 0};
    std::string fmtStrs[] = {"%Y-%m-%d", "%Y-%m-%d", "%Y-%m-%d"};
    std::string tzStrs[] = {"Asia/Shanghai", "Asia/Shanghai", "Asia/Shanghai"};
    std::string policyStrs[] = {"CORRECTED", "CORRECTED", "CORRECTED"};
    const char *fmtStrPtrs[rowCnt];
    int32_t fmtLens[rowCnt];
    const char *tzStrPtrs[rowCnt];
    int32_t tzLens[rowCnt];
    const char *policyStrPtrs[rowCnt];
    int32_t policyLens[rowCnt];
    for (int32_t i = 0; i < rowCnt; i++) {
        fmtStrPtrs[i] = fmtStrs[i].c_str();
        fmtLens[i] = fmtStrs[i].length();
        tzStrPtrs[i] = tzStrs[i].c_str();
        tzLens[i] = tzStrs[i].length();
        policyStrPtrs[i] = policyStrs[i].c_str();
        policyLens[i] = policyStrs[i].length();
    }

    bool isAnyNull[] = {false, false, false};
    int64_t output[rowCnt];
    BatchUnixTimestampFromDate(dates, fmtStrPtrs, fmtLens, tzStrPtrs, tzLens, policyStrPtrs, policyLens,
                               isAnyNull, output, rowCnt);
    std::vector<int64_t> result(output, output + rowCnt);
    std::vector<int64_t> expect = { 615999600, 488822400, -28800 };
    AssertLongEquals(expect, result);
}

TEST(BatchFunctionTest, BatchFromUnixTimeRetNull)
{
    const int32_t rowCnt = 4;
    bool outputNull[rowCnt];
    auto context = new ExecutionContext();
    int64_t contextPtr = reinterpret_cast<int64_t>(context);
    int64_t timestamps[rowCnt] = {615999600, 488822400, 0, -100};
    std::string fmtStrs[] = {"%Y-%m-%d %H:%M:%S", "%Y-%m-%d %H:%M:%S", "%Y-%m-%d %H:%M:%S", "%Y-%m-%d %H:%M:%S"};
    std::string tzStrs[] = {"Asia/Shanghai", "Asia/Shanghai", "Asia/Shanghai", "Asia/Shanghai"};
    const char *fmtStrPtrs[rowCnt];
    int32_t fmtLens[rowCnt];
    const char *tzStrPtrs[rowCnt];
    int32_t tzLens[rowCnt];
    char *output[rowCnt];
    int32_t outLens[rowCnt];
    for (int32_t i = 0; i < rowCnt; i++) {
        fmtStrPtrs[i] = fmtStrs[i].c_str();
        fmtLens[i] = fmtStrs[i].length();
        tzStrPtrs[i] = tzStrs[i].c_str();
        tzLens[i] = tzStrs[i].length();
    }

    BatchFromUnixTimeRetNull(outputNull, contextPtr, timestamps, fmtStrPtrs, fmtLens, tzStrPtrs, tzLens,
                             output, outLens, rowCnt);
    std::vector<uint8_t *> result(rowCnt);
    std::vector<int32_t> resultLen(outLens, outLens + rowCnt);
    for (int32_t i = 0; i < rowCnt; i++) {
        result[i] = reinterpret_cast<uint8_t *>(output[i]);
    }
    std::vector<std::string> expect = { "1989-07-10 00:00:00", "1985-06-29 00:00:00", "1970-01-01 08:00:00",
        "1970-01-01 07:58:20" };
    AssertStringEquals(expect, result, resultLen);
    delete context;
}

TEST(BatchFunctionTest, BatchContainsStr)
{
    std::vector<std::string> srcStrVec{ "", "ab", "", "abc", "abc", "abcd", "", "ab", "" };
    std::vector<std::string> matchStrVec{ "ab", "", "", "ab", "bc", "bc", "ab", "", "" };
    int32_t rowCnt = static_cast<int32_t>(srcStrVec.size());
    char *srcStrs[rowCnt];
    int32_t srcLens[rowCnt];
    char *matchStrs[rowCnt];
    int32_t matchLens[rowCnt];
    bool isAnyNull[] = {true, true, true, false, false, false, false, false, false};
    bool output[rowCnt];
    for (int32_t row = 0; row < rowCnt; row++) {
        srcStrs[row] = const_cast<char *>(srcStrVec[row].c_str());
        srcLens[row] = srcStrVec[row].length();
        matchStrs[row] = const_cast<char *>(matchStrVec[row].c_str());
        matchLens[row] = matchStrVec[row].length();
    }
    BatchContainsStr(srcStrs, srcLens, matchStrs, matchLens, isAnyNull, output, rowCnt);
    std::vector<bool> expect{ false, false, false, true, true, true, false, true, true };
    AssertBoolEquals(expect, output);
}

TEST(BatchFunctionTest, BatchGreatestStr)
{
    std::vector<std::string> xStrVec{ "abc", "abcd", "abc", "", "", "abc", "", "", "1234" };
    std::vector<std::string> yStrVec{ "abcd", "abc", "", "abc", "", "", "abc", "", "2" };
    int32_t rowCnt = static_cast<int32_t>(xStrVec.size());
    uint8_t *xStrs[rowCnt];
    int32_t xLens[rowCnt];
    uint8_t *yStrs[rowCnt];
    int32_t yLens[rowCnt];
    bool xIsNull[] = { false, false, false, false, false, false, true, true, false };
    bool yIsNull[] = { false, false, false, false, false, true, false, true, false };
    bool retIsNull[] = { false, false, false, false, false, false, false, false, false };
    std::vector<uint8_t *> output(rowCnt);
    std::vector<int32_t> outLens(rowCnt);
    for (int32_t row = 0; row < rowCnt; row++) {
        xStrs[row] = reinterpret_cast<uint8_t *>(const_cast<char *>(xStrVec[row].c_str()));
        xLens[row] = xStrVec[row].length();
        yStrs[row] = reinterpret_cast<uint8_t *>(const_cast<char *>(yStrVec[row].c_str()));
        yLens[row] = yStrVec[row].length();
    }
    BatchGreatestStr(xStrs, xLens, xIsNull, yStrs, yLens, yIsNull, retIsNull, output.data(), outLens.data(), rowCnt);
    std::vector<std::string> expect = { "abcd", "abcd", "abc", "abc", "", "abc", "abc", "", "2" };
    std::vector<bool> expectRetNull{ false, false, false, false, false, false, false, true, false };
    AssertStringEquals(expect, output, outLens);
    AssertBoolEquals(expectRetNull, retIsNull);
}

TEST(BatchFunctionTest, BatchGreatest)
{
    int32_t xValue[] = {10, 5, 0, 10, 0};
    bool xIsNull[] = {false, false, true, false, true};
    int32_t yValue[] = {5, 10, 10, 0, 0};
    bool yIsNull[] = {false, false, false, true, true};
    bool retIsNull[] = {false, false, false, false, false};
    int32_t rowCnt = sizeof(xValue) / sizeof(int32_t);
    int32_t output[rowCnt];
    BatchGreatest<int32_t>(xValue, xIsNull, yValue, yIsNull, retIsNull, output, rowCnt);
    int32_t expect[] = {10, 10, 10, 10, 0};
    EXPECT_TRUE(CmpArray<int32_t>(output, expect, rowCnt));
    bool expectRetNull[] = {false, false, false, false, true};
    EXPECT_TRUE(CmpArray<bool>(retIsNull, expectRetNull, rowCnt));

    bool xBool[] = {true, false, true, false, true, false, false};
    bool xBoolIsNull[] = {false, false, false, false, false, true, true};
    bool yBool[] = {false, true, true, false, false, true, false};
    bool yBoolIsNull[] = {false, false, false, false, true, false, true};
    bool boolRetIsNull[] = {false, false, false, false, false, false, false};
    int32_t boolRowCnt = sizeof(xBool) / sizeof(bool);
    bool boolOutput[boolRowCnt];
    BatchGreatest<bool>(xBool, xBoolIsNull, yBool, yBoolIsNull, boolRetIsNull, boolOutput, boolRowCnt);
    bool boolExpect[] = {true, true, true, false, true, true, false};
    bool boolExpectRetNull[] = {false, false, false, false, false, false, true};
    EXPECT_TRUE(CmpArray<bool>(boolOutput, boolExpect, boolRowCnt));
    EXPECT_TRUE(CmpArray<bool>(boolRetIsNull, boolExpectRetNull, boolRowCnt));
}

TEST(BatchFunctionTest, BatchGreatestDecimal64)
{
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);
    int64_t xValue[] = {127, 0, 128, 0, 12800};
    bool xIsNull[] = {false, true, false, true, false};
    int64_t yValue[] = {128, 128, 0, 0, 12708};
    bool yIsNull[] = {false, false, true, true, false};
    bool retIsNull[] = {false, false, false, false, false};
    int32_t rowCnt = sizeof(xValue) / sizeof(int64_t);
    int64_t output[] = {0, 0, 0, 0, 0};
    BatchGreatestDecimal64(contextPtr, xValue, 18, 1, xIsNull, yValue, 18, 1, yIsNull, retIsNull, output, 18, 1,
        rowCnt);
    int64_t expect[] = {128, 128, 128, 0, 12800};
    bool expectRetNull[] = {false, false, false, true, false};
    EXPECT_TRUE(CmpArray<int64_t>(output, expect, rowCnt));
    EXPECT_TRUE(CmpArray<bool>(retIsNull, expectRetNull, rowCnt));

    for (int i = 0; i < rowCnt; i++) {
        retIsNull[i] = false;
        output[i] = 0;
    }
    bool overflowNull[] = {false, false, false, false, false};
    BatchGreatestDecimal64RetNull(overflowNull, xValue, 18, 1, xIsNull, yValue, 18, 1, yIsNull, retIsNull, output, 18,
        1, rowCnt);
    int64_t expectRet[] = {128, 128, 128, 0, 12800};
    bool expectRetNull2[] = {false, false, false, true, false};
    bool expectOverflowNull[] = {false, false, false, false, false};
    EXPECT_TRUE(CmpArray<int64_t>(output, expectRet, rowCnt));
    EXPECT_TRUE(CmpArray<bool>(retIsNull, expectRetNull2, rowCnt));
    EXPECT_TRUE(CmpArray<bool>(overflowNull, expectOverflowNull, rowCnt));
    delete context;
}

TEST(BatchFunctionTest, BatchGreatestDecimal128)
{
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);
    Decimal128 xValue[] = {Decimal128(0, 127), Decimal128(0, 0), Decimal128(0, 128), Decimal128(0, 0),
                           Decimal128(0, 12800)};
    bool xIsNull[] = {false, true, false, true, false};
    Decimal128 yValue[] = {Decimal128(0, 128), Decimal128(0, 128), Decimal128(0, 0), Decimal128(0, 0),
                           Decimal128(0, 12708)};
    bool yIsNull[] = {false, false, true, true, false};
    bool retIsNull[] = {false, false, false, false, false};
    int32_t rowCnt = sizeof(xValue) / sizeof(Decimal128);
    Decimal128 output[] = {Decimal128(0, 0), Decimal128(0, 0), Decimal128(0, 0), Decimal128(0, 0), Decimal128(0, 0)};
    BatchGreatestDecimal128(contextPtr, xValue, 38, 1, xIsNull, yValue, 38, 1, yIsNull, retIsNull, output, 38, 1,
        rowCnt);
    Decimal128 expect[] = {Decimal128(0, 128), Decimal128(0, 128), Decimal128(0, 128), Decimal128(0, 0),
                           Decimal128(0, 12800)};
    bool expectRetNull[] = {false, false, false, true, false};
    EXPECT_TRUE(CmpArray<Decimal128>(output, expect, rowCnt));
    EXPECT_TRUE(CmpArray<bool>(retIsNull, expectRetNull, rowCnt));

    for (int i = 0; i < rowCnt; i++) {
        retIsNull[i] = false;
        output[i] = Decimal128(0, 0);
    }
    bool overflowNull[] = {false, false, false, false, false};
    BatchGreatestDecimal128RetNull(overflowNull, xValue, 38, 1, xIsNull, yValue, 38, 1, yIsNull, retIsNull, output, 38,
        1, rowCnt);
    Decimal128 expectRet[] = {Decimal128(0, 128), Decimal128(0, 128), Decimal128(0, 128), Decimal128(0, 0),
                              Decimal128(0, 12800)};
    bool expectRetNull2[] = {false, false, false, true, false};
    bool expectOverflowNull[] = {false, false, false, false, false};
    EXPECT_TRUE(CmpArray<Decimal128>(output, expectRet, rowCnt));
    EXPECT_TRUE(CmpArray<bool>(retIsNull, expectRetNull2, rowCnt));
    EXPECT_TRUE(CmpArray<bool>(overflowNull, expectOverflowNull, rowCnt));
    delete context;
}

TEST(BatchFunctionTest, BatchEmptyToNull)
{
    std::vector<std::string> inString{ "", "abc", "abc123", "", "国家" };
    int32_t rowCnt = static_cast<int32_t>(inString.size());
    bool isAnyNull[] = {false, false, false, false, true};
    int32_t inLens[rowCnt];
    std::vector<char *> strAddr(rowCnt);
    for (int i = 0; i < rowCnt; ++i) {
        inLens[i] = inString[i].size();
        strAddr[i] = reinterpret_cast<char *>(const_cast<char *>(inString[i].c_str()));
    }
    strAddr[3] = nullptr;
    std::vector<int32_t> outLen(rowCnt);
    std::vector<char *> outResult(rowCnt);

    BatchEmptyToNull(strAddr.data(), inLens, isAnyNull, outResult.data(), outLen.data(), rowCnt);
    EXPECT_EQ(outResult[0], nullptr);
    EXPECT_EQ(outLen[0], 0);
    EXPECT_EQ("abc", std::string(outResult[1], outLen[1]));
    EXPECT_EQ("abc123", std::string(outResult[2], outLen[2]));
    EXPECT_EQ(outResult[3], nullptr);
    EXPECT_EQ(outLen[3], 0);
}

TEST(BatchFunctionTest, BatchStaticInvokeVarcharTypeWriteSideCheck)
{
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);
    int32_t  limit = 4;
    std::vector<std::string> srcStrVec{ "abc", "abcd", "abcde", "abc ", "abcd ", "abced ", "测试超过长度", "测试长度"};
    std::vector<std::string> matchStrVec{ "abc", "abcd", "", "abc ", "abcd", "", "", "测试长度"};
    int32_t rowCnt = static_cast<int32_t>(srcStrVec.size()) + 1;
    char *srcStrs[rowCnt];
    int32_t srcLens[rowCnt];
    char *output[rowCnt];
    int32_t outputLen[rowCnt];
    for (int32_t row = 0; row < rowCnt - 1; ++row) {
        srcStrs[row] = const_cast<char *>(srcStrVec[row].c_str());
        srcLens[row] = srcStrVec[row].size();
    }
    srcStrs[rowCnt - 1] = nullptr;
    srcLens[rowCnt - 1] = 0;
    bool isAnyNull[] = {false, false, false, false, false, false, false, false, true};
    BatchStaticInvokeVarcharTypeWriteSideCheck(contextPtr, srcStrs, srcLens, limit,
        isAnyNull, output, outputLen, rowCnt);
    for (int32_t row = 0; row < rowCnt; ++row) {
        if (outputLen[row] == 0) {
            EXPECT_TRUE(output[row] == nullptr);
        } else {
            std::string outStr (output[row], outputLen[row]);
            EXPECT_EQ(outStr, matchStrVec[row]);
        }
    }
    delete context;
}

TEST(BatchFunctionTest, BatchStaticInvokeCharReadPadding)
{
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);
    int32_t  limit = 4;
    std::vector<std::string> srcStrVec{ "1", "12", "123", "1234", "12345", "123456", "测试超过长度", "测试"};
    std::vector<std::string> matchStrVec{ "1   ", "12  ", "123 ", "1234", "12345", "123456", "测试超过长度", "测试  "};
    int32_t rowCnt = static_cast<int32_t>(srcStrVec.size()) + 1;
    char *srcStrs[rowCnt];
    int32_t srcLens[rowCnt];
    char *matchStrs[rowCnt];
    int32_t matchLens[rowCnt];
    char *output[rowCnt];
    int32_t outputLen[rowCnt];
    for (int32_t row = 0; row < rowCnt - 1; ++row) {
        srcStrs[row] = const_cast<char *>(srcStrVec[row].c_str());
        srcLens[row] = srcStrVec[row].length();
        matchStrs[row] = const_cast<char *>(matchStrVec[row].c_str());
        matchLens[row] = matchStrVec[row].length();
    }
    srcStrs[rowCnt - 1] = nullptr;
    srcLens[rowCnt - 1] = 0;
    bool isAnyNull[] = {false, false, false, false, false, false, false, false, true};
    BatchStaticInvokeCharReadPadding(contextPtr, srcStrs, srcLens, limit, isAnyNull, output, outputLen, rowCnt);
    for (int32_t row = 0; row < rowCnt; ++row) {
        if (outputLen[row] == 0) {
            EXPECT_TRUE(output[row] == nullptr);
        } else {
            std::string outStr (output[row], outputLen[row]);
            EXPECT_EQ(outStr, matchStrVec[row]);
        }
    }
    delete context;
}
