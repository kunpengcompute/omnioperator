/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "gtest/gtest.h"
#include "type/decimal_operations.h"
#include "vector/vector_common.h"

using namespace omniruntime::type;

namespace Decimal128Test {
#ifdef DIV_CAL_NEED_CHECK
Decimal128Wrapper Negate(const Decimal128Wrapper &value) { return Decimal128Wrapper::Negate(value); }

bool AssertDivide(Decimal128Wrapper &dividend, Decimal128Wrapper &divisor, int32_t dividendScaleFactor,
                  int32_t divisorScaleFactor, Decimal128Wrapper &expectQuotient, Decimal128Wrapper &expectRemainder)
{
    if (dividendScaleFactor >= Decimal128::MAX_LONG_PRECISION) {
        std::cout << "error" << std::endl;
    }
    if (divisorScaleFactor >= Decimal128::MAX_LONG_PRECISION) {
        std::cout << "error" << std::endl;
    }
    Decimal128Wrapper result;
    int256_t dividend256 = dividend.ToInt128() * TenOfScaleMultipliers[dividendScaleFactor];
    int256_t divisor256 = divisor.ToInt128() * TenOfScaleMultipliers[divisorScaleFactor];
    int256_t quotient256;
    int256_t remainder256;

    divide_qr(dividend256, divisor256, quotient256, remainder256);

    if (quotient256 == expectQuotient.ToInt128() && remainder256 == expectRemainder.ToInt128()) {
        return true;
    } else {
        EXPECT_EQ(quotient256, expectQuotient.ToInt128());
        EXPECT_EQ(remainder256, expectRemainder.ToInt128());
        return false;
    }
}

bool AssertDivideAllSign(Decimal128Wrapper &dividend, Decimal128Wrapper &divisor, int32_t dividendScaleFactor,
                         int32_t divisorScaleFactor, Decimal128Wrapper &expectQuotient,
                         Decimal128Wrapper &expectRemainder)
{
    Decimal128Wrapper negateDividend = Negate(dividend);
    Decimal128Wrapper negateDivisor = Negate(divisor);
    Decimal128Wrapper negateExQuotient = Negate(expectQuotient);
    Decimal128Wrapper negateExRemainder = Negate(expectRemainder);

    bool allSignResult1 =
        AssertDivide(dividend, divisor, dividendScaleFactor, divisorScaleFactor, expectQuotient, expectRemainder);
    EXPECT_EQ(allSignResult1, true);

    bool allSignResult2 = AssertDivide(dividend, negateDivisor, dividendScaleFactor, divisorScaleFactor,
                                       negateExQuotient, expectRemainder);
    EXPECT_EQ(allSignResult2, true);

    bool allSignResult3 = AssertDivide(negateDividend, divisor, dividendScaleFactor, divisorScaleFactor,
                                       negateExQuotient, expectRemainder);
    EXPECT_EQ(allSignResult3, true);

    bool allSignResult4 = AssertDivide(negateDividend, negateDivisor, dividendScaleFactor, divisorScaleFactor,
                                       expectQuotient, expectRemainder);
    EXPECT_EQ(allSignResult4, true);

    return allSignResult1 && allSignResult2 && allSignResult3 && allSignResult4;
}
#endif

static const int DECIMAL128_HALF_BIT_LENGTH = 64;
static const __int128 INT_128_MIN = __int128(1) << 127;
static const __int128 INT_128_MAX = ~INT_128_MIN;

TEST(Decimal128, abs_test)
{
    Decimal128Wrapper zero(0);
    Decimal128Wrapper one(1);
    Decimal128Wrapper negativeOne(-1);
    auto result1 = zero.Abs();
    auto result2 = one.Abs();
    auto result3 = negativeOne.Abs();
    EXPECT_EQ(result1.HighBits(), zero.HighBits());
    EXPECT_EQ(result1.LowBits(), zero.LowBits());
    EXPECT_EQ(result2.HighBits(), one.HighBits());
    EXPECT_EQ(result2.LowBits(), one.LowBits());
    EXPECT_EQ(result3.HighBits(), one.HighBits());
    EXPECT_EQ(result3.LowBits(), one.LowBits());
}

TEST(Decimal128, negate)
{
    Decimal128Wrapper value(2);
    Decimal128Wrapper result = value.Negate();
    Decimal128Wrapper positive(-2);
    EXPECT_EQ(result.HighBits(), positive.HighBits());
    EXPECT_EQ(result.LowBits(), positive.LowBits());
}

TEST(Decimal128, add_normal)
{
    Decimal128Wrapper left(2);
    Decimal128Wrapper right(2);
    Decimal128Wrapper result;
    result = left.Add(right);
    EXPECT_EQ(result.HighBits(), 0);
    EXPECT_EQ(result.LowBits(), 4);
}

TEST(Decimal128, add_negate)
{
    Decimal128Wrapper left(2);
    Decimal128Wrapper right(-2);
    Decimal128Wrapper result;
    result = left.Add(right);
    Decimal128Wrapper negativeOne(0);
    EXPECT_EQ(result.HighBits(), negativeOne.HighBits());
    EXPECT_EQ(result.LowBits(), negativeOne.LowBits());
}

TEST(Decimal128, subtract_positive_positive)
{
    Decimal128Wrapper left(2);
    Decimal128Wrapper right(1);
    Decimal128Wrapper result = left.Subtract(right);
    Decimal128Wrapper expected(1);
    EXPECT_EQ(expected, result);
}

TEST(Decimal128, subtract_negative_negative)
{
    Decimal128Wrapper left(-2);
    Decimal128Wrapper right(-1);
    Decimal128Wrapper result = left.Subtract(right);
    Decimal128Wrapper expected(-1);
    EXPECT_EQ(expected, result);
}

TEST(Decimal128, subtract_positive_negative)
{
    Decimal128Wrapper left(2);
    Decimal128Wrapper right(-1);
    Decimal128Wrapper result = left.Subtract(right);
    Decimal128Wrapper expected(3);
    EXPECT_EQ(expected, result);
}

TEST(Decimal128, subtract_negative_positive)
{
    Decimal128Wrapper left(-2);
    Decimal128Wrapper right(1);
    Decimal128Wrapper result = left.Subtract(right);
    Decimal128Wrapper expected(-3);
    EXPECT_EQ(expected, result);
}

TEST(Decimal128, multiple_positive)
{
    Decimal128Wrapper left(2);
    Decimal128Wrapper right(2);
    Decimal128Wrapper result;
    result = left.Multiply(right);
    EXPECT_EQ(result.HighBits(), 0);
    EXPECT_EQ(result.LowBits(), 4);
}

TEST(Decimal128, multiple_negate)
{
    Decimal128Wrapper left(-2);
    Decimal128Wrapper right(2);
    Decimal128Wrapper result;
    result = left.Multiply(right);
    Decimal128Wrapper expected(-4);
    EXPECT_EQ(result.HighBits(), expected.HighBits());
    EXPECT_EQ(result.LowBits(), expected.LowBits());
}

TEST(Decimal128, multiple_mix)
{
    Decimal128Wrapper left = Decimal128Wrapper(-1, -234527000012345L);
    Decimal128Wrapper right = Decimal128Wrapper(0L, 1000000);
    Decimal128Wrapper result;
    result = left.Multiply(right);
    Decimal128Wrapper expected = Decimal128Wrapper(Decimal128("-234527000012345000000"));
    EXPECT_EQ(result.HighBits(), expected.HighBits());
    EXPECT_EQ(result.LowBits(), expected.LowBits());
}

TEST(Decimal128, divide_positive1)
{
    Decimal128Wrapper left(2);
    Decimal128Wrapper right(2);
    Decimal128Wrapper result;
    result = left.Divide(right, 0);
    EXPECT_EQ(result.HighBits(), 0);
    EXPECT_EQ(result.LowBits(), 1);
}

TEST(Decimal128, divide_positive2)
{
    Decimal128Wrapper left(12, 2);
    Decimal128Wrapper right(0L, 2);
    Decimal128Wrapper result;
    result = left.Divide(right, 0);
    EXPECT_EQ(result.HighBits(), 6);
    EXPECT_EQ(result.LowBits(), 1);
}

TEST(Decimal128, divide_positive3)
{
    Decimal128Wrapper left(12, 3);
    Decimal128Wrapper right(0L, 2);
    Decimal128Wrapper result;
    result = left.Divide(right, 0);
    EXPECT_EQ(result.HighBits(), 6);
    EXPECT_EQ(result.LowBits(), 2);
}

TEST(Decimal128, divide_dividend_smaller_than_divisor)
{
    Decimal128Wrapper left(78340625600);
    Decimal128Wrapper right(2729300525);
    Decimal128Wrapper result;
    result = left.Divide(right, 0);
    Decimal128Wrapper expected(0L, 29);
    EXPECT_EQ(result, expected);
}

TEST(Decimal128, divide_positive_round_up)
{
    Decimal128Wrapper left(12, 4);
    Decimal128Wrapper right(0L, 3);
    Decimal128Wrapper result;
    result = left.Divide(right, 0);
    EXPECT_EQ(result.HighBits(), 4);
    EXPECT_EQ(result.LowBits(), 1);
}

TEST(Decimal128, divide_negative)
{
    Decimal128Wrapper left("-4");
    Decimal128Wrapper right("2");
    Decimal128Wrapper result;
    result = left.Divide(right, 0);
    Decimal128Wrapper expected("-2");
    EXPECT_EQ(result.HighBits(), expected.HighBits());
    EXPECT_EQ(result.LowBits(), expected.LowBits());
}

TEST(Decimal128, divide_negative1)
{
    Decimal128Wrapper left("-4");
    Decimal128Wrapper right("-2");
    Decimal128Wrapper result;
    result = left.Divide(right, 0);
    Decimal128Wrapper expected("2");
    EXPECT_EQ(result.HighBits(), expected.HighBits());
    EXPECT_EQ(result.LowBits(), expected.LowBits());
}

TEST(Decimal128, divide_negative2)
{
    Decimal128Wrapper left("4");
    Decimal128Wrapper right("-2");
    Decimal128Wrapper result;
    result = left.Divide(right, 0);
    Decimal128Wrapper expected("-2");
    EXPECT_EQ(result.HighBits(), expected.HighBits());
    EXPECT_EQ(result.LowBits(), expected.LowBits());
}

TEST(Decimal128, divide_negative_round_up)
{
    Decimal128Wrapper left("4");
    Decimal128Wrapper right("-3");
    Decimal128Wrapper result;
    result = left.Divide(right, 0);
    Decimal128Wrapper expected("-1");
    EXPECT_EQ(result.HighBits(), expected.HighBits());
    EXPECT_EQ(result.LowBits(), expected.LowBits());
}

TEST(Decimal128, positive_dividend_positive_divisor_and_with_scale_factor)
{
    Decimal128Wrapper left(124861912500);
    Decimal128Wrapper right(1652201977500);
    Decimal128Wrapper result;
    result = left.Divide(right, 16);
    Decimal128Wrapper expected(0L, 755730317481720);
    EXPECT_EQ(result, expected);
}

TEST(Decimal128, negative_dividend_positive_divisor_and_with_scale_factor)
{
    Decimal128Wrapper left(-124861912500);
    Decimal128Wrapper right(1652201977500);
    Decimal128Wrapper result;
    result = left.Divide(right, 16);
    Decimal128Wrapper expected(-1, -755730317481720);
    EXPECT_EQ(result, expected);
}

TEST(Decimal128, negative_dividend_negative_divisor_and_with_scale_factor)
{
    Decimal128Wrapper left(-124861912500);
    Decimal128Wrapper right(-1652201977500);
    Decimal128Wrapper result;
    result = left.Divide(right, 16);
    Decimal128Wrapper expected(0L, 755730317481720);
    EXPECT_EQ(result, expected);
}

TEST(Decimal128, compare_eq)
{
    auto left = new Decimal128Wrapper(12, 2);
    auto right = new Decimal128Wrapper(12, 2);
    auto result = *left == *right;
    EXPECT_EQ(result, true);
    delete left;
    delete right;
}

TEST(Decimal128, compare_ne)
{
    auto left = new Decimal128Wrapper(12, 3);
    auto right = new Decimal128Wrapper(12, 2);
    auto result = *left != *right;
    EXPECT_EQ(result, true);
    delete left;
    delete right;
}

TEST(Decimal128, compare_le1)
{
    auto left = new Decimal128Wrapper(12, 2);
    auto right = new Decimal128Wrapper(12, 2);
    auto result = *left <= *right;
    EXPECT_EQ(result, true);
    delete left;
    delete right;
}

TEST(Decimal128, compare_le2)
{
    auto left = new Decimal128Wrapper(-12, 2);
    auto right = new Decimal128Wrapper(12, 2);
    auto result = *left <= *right;
    EXPECT_EQ(result, true);
    delete left;
    delete right;
}

TEST(Decimal128, compare_lt)
{
    auto left = new Decimal128Wrapper(12, 1);
    auto right = new Decimal128Wrapper(12, 2);
    auto result = *left < *right;
    EXPECT_EQ(result, true);
    delete left;
    delete right;
}

TEST(Decimal128, compare_ge1)
{
    auto left = new Decimal128Wrapper(12, 3);
    auto right = new Decimal128Wrapper(12, 2);
    auto result = *left >= *right;
    EXPECT_EQ(result, true);
    delete left;
    delete right;
}

TEST(Decimal128, compare_ge2)
{
    auto left = new Decimal128Wrapper(12, 2);
    auto right = new Decimal128Wrapper(12, 2);
    auto result = *left >= *right;
    EXPECT_EQ(result, true);
    delete left;
    delete right;
}

TEST(Decimal128, compare_gt)
{
    auto left = new Decimal128Wrapper(12, 2);
    auto right = new Decimal128Wrapper(-12, 2);
    auto result = *left > *right;
    EXPECT_EQ(result, true);
    delete left;
    delete right;
}

TEST(Decimal128, compare_negative)
{
    Decimal128Wrapper left("-1");
    Decimal128Wrapper right("-2");
    auto result = left < right;
    EXPECT_EQ(result, false);
}

TEST(Decimal128, div_roundup)
{
    Decimal128Wrapper lValue(0x1381e4, 0xfddf26f775600000);
    Decimal128Wrapper rValue(0x0L, 0x13ba38720);

    Decimal128Wrapper expectValue(0x0L, 4453370194541067);
    Decimal128Wrapper result;
    result = lValue.Divide(rValue, 0);
    EXPECT_EQ(result, expectValue);
}

TEST(Decimal128, div_roundup_2)
{
    Decimal128Wrapper lValue(0x000000000c476a81, 0xd22a79fa4fc30000);
    Decimal128Wrapper rValue(0x0000000000000000L, 0x0000010473b0c563);

    Decimal128Wrapper expectValue(0x0L, 3397145127548828);
    Decimal128Wrapper result;
    result = lValue.Divide(rValue, 0);
    EXPECT_EQ(result, expectValue);
}

TEST(DecimalTest, compare_after_sum)
{
    Decimal128Wrapper d1(-1);
    Decimal128Wrapper d2(-2);
    Decimal128Wrapper d3;
    d3 = d1.Add(d2);
    Decimal128Wrapper d4(-2);
    Decimal128Wrapper d5(-3);
    Decimal128Wrapper d6 = d4.Add(d5);
    auto result = d3 > d6;
    EXPECT_EQ(result, true);
}

#ifdef DIV_CAL_NEED_CHECK
TEST(Decimal128, div_roundup_3)
{
    Decimal128Wrapper lValue1(0x2, 0x0000000300000004);
    Decimal128Wrapper rValue1(0x100000002, 0x0000000300000004);
    Decimal128Wrapper expectQuotient1(0x0L, 0x0);
    Decimal128Wrapper expectRemainder1(0x2, 0x0000000300000004);
    bool result1 = AssertDivideAllSign(lValue1, rValue1, 0, 0, expectQuotient1, expectRemainder1);
    EXPECT_EQ(result1, true);

    Decimal128Wrapper lValue2(0x0L, 0x03);
    Decimal128Wrapper rValue2(0x0L, 0xffffffff);
    Decimal128Wrapper expectQuotient2(0x0L, 0x0);
    Decimal128Wrapper expectRemainder2(0x0L, 0x03);
    bool result2 = AssertDivideAllSign(lValue2, rValue2, 0, 0, expectQuotient2, expectRemainder2);
    EXPECT_EQ(result2, true);

    Decimal128Wrapper lValue3(0x0L, 0xffffffff);
    Decimal128Wrapper rValue3(0x0L, 0x01);
    Decimal128Wrapper expectQuotient3(0x0L, 0xffffffff);
    Decimal128Wrapper expectRemainder3(0x0L, 0x00);
    bool result3 = AssertDivideAllSign(lValue3, rValue3, 0, 0, expectQuotient3, expectRemainder3);
    EXPECT_EQ(result3, true);

    Decimal128Wrapper lValue4(0x0L, 0xffffffff);
    Decimal128Wrapper rValue4(0x0L, 0xffffffff);
    Decimal128Wrapper expectQuotient4(0x0L, 0x01);
    Decimal128Wrapper expectRemainder4(0x0L, 0x00);
    bool result4 = AssertDivideAllSign(lValue4, rValue4, 0, 0, expectQuotient4, expectRemainder4);
    EXPECT_EQ(result4, true);

    Decimal128Wrapper lValue5(0x0L, 0xffffffff);
    Decimal128Wrapper rValue5(0x0L, 0x03);
    Decimal128Wrapper expectQuotient5(0x0L, 0x55555555);
    Decimal128Wrapper expectRemainder5(0x0L, 0x00);
    bool result5 = AssertDivideAllSign(lValue5, rValue5, 0, 0, expectQuotient5, expectRemainder5);
    EXPECT_EQ(result5, true);

    // 18446744073709551615/4294967295
    Decimal128Wrapper lValue6(0x0L, 0xffffffffffffffff);
    Decimal128Wrapper rValue6(0x0L, 0xffffffff);
    Decimal128Wrapper expectQuotient6(0x0L, 0x100000001);
    Decimal128Wrapper expectRemainder6(0x0L, 0x00);
    bool result6 = AssertDivideAllSign(lValue6, rValue6, 0, 0, expectQuotient6, expectRemainder6);
    EXPECT_EQ(result6, true);

    // 18446744069414584319/4294967295
    Decimal128Wrapper lValue7(0x0L, 0xfffffffeffffffff);
    Decimal128Wrapper rValue7(0x0L, 0xffffffff);
    Decimal128Wrapper expectQuotient7(0x0L, 0xffffffff);
    Decimal128Wrapper expectRemainder7(0x0L, 0xfffffffe);
    bool result7 = AssertDivideAllSign(lValue7, rValue7, 0, 0, expectQuotient7, expectRemainder7);
    EXPECT_EQ(result7, true);

    // 20014547621496/39612
    Decimal128Wrapper lValue8(0x0L, 0x0000123400005678);
    Decimal128Wrapper rValue8(0x0L, 0x00009abc);
    Decimal128Wrapper expectQuotient8(0x0L, 0x1e1dba76);
    Decimal128Wrapper expectRemainder8(0x0L, 0x6bd0);
    bool result8 = AssertDivideAllSign(lValue8, rValue8, 0, 0, expectQuotient8, expectRemainder8);
    EXPECT_EQ(result8, true);

    // 30064771072/12884901888
    Decimal128Wrapper lValue9(0x0L, 0x700000000);
    Decimal128Wrapper rValue9(0x0L, 0x300000000);
    Decimal128Wrapper expectQuotient9(0x0L, 0x02);
    Decimal128Wrapper expectRemainder9(0x0L, 0x100000000);
    bool result9 = AssertDivideAllSign(lValue9, rValue9, 0, 0, expectQuotient9, expectRemainder9);
    EXPECT_EQ(result9, true);

    // 30064771077/12884901888
    Decimal128Wrapper lValue10(0x0L, 0x700000005);
    Decimal128Wrapper rValue10(0x0L, 0x300000000);
    Decimal128Wrapper expectQuotient10(0x0L, 0x02);
    Decimal128Wrapper expectRemainder10(0x0L, 0x100000005);
    bool result10 = AssertDivideAllSign(lValue10, rValue10, 0, 0, expectQuotient10, expectRemainder10);
    EXPECT_EQ(result10, true);

    // 25769803776/8589934592
    Decimal128Wrapper lValue11(0x0L, 0x600000000);
    Decimal128Wrapper rValue11(0x0L, 0x200000000);
    Decimal128Wrapper expectQuotient11(0x0L, 0x03);
    Decimal128Wrapper expectRemainder11(0x0L, 0x00);
    bool result11 = AssertDivideAllSign(lValue11, rValue11, 0, 0, expectQuotient11, expectRemainder11);
    EXPECT_EQ(result11, true);

    // 2147483648/1073741825
    Decimal128Wrapper lValue12(0x0L, 0x80000000);
    Decimal128Wrapper rValue12(0x0L, 0x40000001);
    Decimal128Wrapper expectQuotient12(0x0L, 0x01);
    Decimal128Wrapper expectRemainder12(0x0L, 0x3fffffff);
    bool result12 = AssertDivideAllSign(lValue12, rValue12, 0, 0, expectQuotient12, expectRemainder12);
    EXPECT_EQ(result12, true);

    // 9223372036854775808/1073741825
    Decimal128Wrapper lValue13(0x0L, 0x8000000000000000);
    Decimal128Wrapper rValue13(0x0L, 0x40000001);
    Decimal128Wrapper expectQuotient13(0x0L, 0x1fffffff8);
    Decimal128Wrapper expectRemainder13(0x0L, 0x08);
    bool result13 = AssertDivideAllSign(lValue13, rValue13, 0, 0, expectQuotient13, expectRemainder13);
    EXPECT_EQ(result13, true);

    // 9223372036854775808/4611686018427387905
    Decimal128Wrapper lValue14(0x0L, 0x8000000000000000);
    Decimal128Wrapper rValue14(0x0L, 0x4000000000000001);
    Decimal128Wrapper expectQuotient14(0x0L, 0x01);
    Decimal128Wrapper expectRemainder14(0x0L, 0x3fffffffffffffff);
    bool result14 = AssertDivideAllSign(lValue14, rValue14, 0, 0, expectQuotient14, expectRemainder14);
    EXPECT_EQ(result14, true);

    // 207661668792474/207661668792474
    Decimal128Wrapper lValue15(0x0L, 0x0000bcde0000789a);
    Decimal128Wrapper rValue15(0x0L, 0x0000bcde0000789a);
    Decimal128Wrapper expectQuotient15(0x0L, 0x01);
    Decimal128Wrapper expectRemainder15(0x0L, 0x00);
    bool result15 = AssertDivideAllSign(lValue15, rValue15, 0, 0, expectQuotient15, expectRemainder15);
    EXPECT_EQ(result15, true);

    // 207661668792475/207661668792474
    Decimal128Wrapper lValue16(0x0L, 0x0000bcde0000789b);
    Decimal128Wrapper rValue16(0x0L, 0x0000bcde0000789a);
    Decimal128Wrapper expectQuotient16(0x0L, 0x01);
    Decimal128Wrapper expectRemainder16(0x0L, 0x01);
    bool result16 = AssertDivideAllSign(lValue16, rValue16, 0, 0, expectQuotient16, expectRemainder16);
    EXPECT_EQ(result16, true);

    // 207661668792473/207661668792474
    Decimal128Wrapper lValue17(0x0L, 0x0000bcde00007899);
    Decimal128Wrapper rValue17(0x0L, 0x0000bcde0000789a);
    Decimal128Wrapper expectQuotient17(0x0L, 0x00);
    Decimal128Wrapper expectRemainder17(0x0L, 0x0000bcde00007899);
    bool result17 = AssertDivideAllSign(lValue17, rValue17, 0, 0, expectQuotient17, expectRemainder17);
    EXPECT_EQ(result17, true);

    // 281470681808895/281470681808895
    Decimal128Wrapper lValue18(0x0L, 0x0000ffff0000ffff);
    Decimal128Wrapper rValue18(0x0L, 0x0000ffff0000ffff);
    Decimal128Wrapper expectQuotient18(0x0L, 0x01);
    Decimal128Wrapper expectRemainder18(0x0L, 0x00);
    bool result18 = AssertDivideAllSign(lValue18, rValue18, 0, 0, expectQuotient18, expectRemainder18);
    EXPECT_EQ(result18, true);

    // 281470681808895/281470681743360
    Decimal128Wrapper lValue19(0x0L, 0x0000ffff0000ffff);
    Decimal128Wrapper rValue19(0x0L, 0x0000ffff00000000);
    Decimal128Wrapper expectQuotient19(0x0L, 0x01);
    Decimal128Wrapper expectRemainder19(0x0L, 0xffff);
    bool result19 = AssertDivideAllSign(lValue19, rValue19, 0, 0, expectQuotient19, expectRemainder19);
    EXPECT_EQ(result19, true);

    // 5368002601758163503531/4294967296
    Decimal128Wrapper lValue20(0x0123, 0x00004567000089ab);
    Decimal128Wrapper rValue20(0x0L, 0x100000000);
    Decimal128Wrapper expectQuotient20(0x0L, 0x12300004567);
    Decimal128Wrapper expectRemainder20(0x0L, 0x89ab);
    bool result20 = AssertDivideAllSign(lValue20, rValue20, 0, 0, expectQuotient20, expectRemainder20);
    EXPECT_EQ(result20, true);

    // 604462910088780974129152/140737488420863
    Decimal128Wrapper lValue21(0x8000, 0x0000fffe00000000);
    Decimal128Wrapper rValue21(0x0L, 0x80000000ffff);
    Decimal128Wrapper expectQuotient21(0x0L, 0xffffffff);
    Decimal128Wrapper expectRemainder21(0x0L, 0x7fff0000ffff);
    bool result21 = AssertDivideAllSign(lValue21, rValue21, 0, 0, expectQuotient21, expectRemainder21);
    EXPECT_EQ(result21, true);

    // 39614081257132168796771975171/9903520314283042199192993793
    Decimal128Wrapper lValue22(0x80000000, 0x0000000000000003);
    Decimal128Wrapper rValue22(0x20000000, 0x0000000000000001);
    Decimal128Wrapper expectQuotient22(0x0L, 0x03);
    Decimal128Wrapper expectRemainder22(0x20000000, 0x00);
    bool result22 = AssertDivideAllSign(lValue22, rValue22, 0, 0, expectQuotient22, expectRemainder22);
    EXPECT_EQ(result22, true);

    // 604462909807314587353091/151115727451828646838273
    Decimal128Wrapper lValue23(0x8000, 0x0000000000000003);
    Decimal128Wrapper rValue23(0x2000, 0x0000000000000001);
    Decimal128Wrapper expectQuotient23(0x0L, 0x03);
    Decimal128Wrapper expectRemainder23(0x2000, 0x00);
    bool result23 = AssertDivideAllSign(lValue23, rValue23, 0, 0, expectQuotient23, expectRemainder23);
    EXPECT_EQ(result23, true);

    // 2596069201709362459734969208012800/604462909807314587353089
    Decimal128Wrapper lValue24(0x00007fff00008000, 0x00);
    Decimal128Wrapper rValue24(0x8000, 0x0000000000000001);
    Decimal128Wrapper expectQuotient24(0x0L, 0xfffe0000);
    Decimal128Wrapper expectRemainder24(0x7fff, 0xffffffff00020000);
    bool result24 = AssertDivideAllSign(lValue24, rValue24, 0, 0, expectQuotient24, expectRemainder24);
    EXPECT_EQ(result24, true);

    // 2596148429267413814546714551386112/604462909807314587418623
    Decimal128Wrapper lValue25(0x800000000000, 0x0000fffe00000000);
    Decimal128Wrapper rValue25(0x8000, 0xffff);
    Decimal128Wrapper expectQuotient25(0x0L, 0xffffffff);
    Decimal128Wrapper expectRemainder25(0x7fff, 0xffffffff0000ffff);
    bool result25 = AssertDivideAllSign(lValue25, rValue25, 0, 0, expectQuotient25, expectRemainder25);
    EXPECT_EQ(result25, true);

    // -18446744065119617024/39614081257132168796772040703
    Decimal128Wrapper lValue26(0x8000000000000000, 0xfffffffe00000000);
    Decimal128Wrapper rValue26(0x80000000, 0x0000ffff);
    Decimal128Wrapper expectQuotient26(0x8000000000000000, 0x00);
    Decimal128Wrapper expectRemainder26(0x00, 0xfffffffe00000000);
    bool result26 = AssertDivideAllSign(lValue26, rValue26, 0, 0, expectQuotient26, expectRemainder26);
    EXPECT_EQ(result26, true);

    // -18446744065119617024/39614081257132168801066942463
    Decimal128Wrapper lValue27(0x8000000000000000, 0xfffffffe00000000);
    Decimal128Wrapper rValue27(0x80000000, 0xffffffff);
    Decimal128Wrapper expectQuotient27(0x8000000000000000, 0x00);
    Decimal128Wrapper expectRemainder27(0x00, 0xfffffffe00000000);
    bool result27 = AssertDivideAllSign(lValue27, rValue27, 0, 0, expectQuotient27, expectRemainder27);
    EXPECT_EQ(result27, true);

    // with scale
    // 100000000000000000000000/111111111111111111111111
    Decimal128Wrapper lValue28(0x152d, 0x02c7e14af6800000);
    Decimal128Wrapper rValue28(0x1787, 0x586c4fa8a01c71c7);
    Decimal128Wrapper expectQuotient28(0x0L, 0x00);
    Decimal128Wrapper expectRemainder28(0x0000314dc6448d93, 0x38c15b0a00000000);
    bool result28 = AssertDivideAllSign(lValue28, rValue28, 10, 10, expectQuotient28, expectRemainder28);
    EXPECT_EQ(result28, true);

    // 100000000000000000000000/111111111111111111111111
    Decimal128Wrapper lValue29(0x152d, 0x02c7e14af6800000);
    Decimal128Wrapper rValue29(0x1787, 0x19debd01c7);
    Decimal128Wrapper expectQuotient29(0x0L, 0x00);
    Decimal128Wrapper expectRemainder29(0x0785ee10d5da46d9, 0x00f436a000000000);
    bool result29 = AssertDivideAllSign(lValue29, rValue29, 14, 14, expectQuotient29, expectRemainder29);
    EXPECT_EQ(result29, true);

    // 99999999999999999999999999999999999999/99999999999999999999999999999999999999
    Decimal128Wrapper lValue30(0x4b3b4ca85a86c47a, 0x098a223fffffffff);
    Decimal128Wrapper rValue30(0x4b3b4ca85a86c47a, 0x098a223fffffffff);
    Decimal128Wrapper expectQuotient30(0x0L, 0x01);
    Decimal128Wrapper expectRemainder30(0x00, 0x00);
    bool result30 = AssertDivideAllSign(lValue30, rValue30, 0, 0, expectQuotient30, expectRemainder30);
    EXPECT_EQ(result30, true);

    // test throw,omniruntme dont support decimal256,but olk is support this case
    // 99999999999999999999999999999999999999/99999999999999999999999999999999999999
    Decimal128Wrapper lValue31(0x4b3b4ca85a86c47a, 0x098a223fffffffff);
    Decimal128Wrapper rValue31(0x4b3b4ca85a86c47a, 0x098a223fffffffff);
    Decimal128Wrapper expectQuotient31(0x0L, 0x0a);
    Decimal128Wrapper expectRemainder31(0x00, 0x00);
    EXPECT_THROW(AssertDivideAllSign(lValue31, rValue31, 2, 1, expectQuotient31, expectRemainder31), std::exception);

    // test throw
    Decimal128Wrapper lValue32(0x4B3B4CA85A86C47B, 0x00);
    Decimal128Wrapper rValue32(0x00, 0x01);
    Decimal128Wrapper expectQuotient32(0x0L, 0x0a);
    Decimal128Wrapper expectRemainder32(0x0L, 0x00);
    EXPECT_THROW(AssertDivideAllSign(lValue32, rValue32, 0, 0, expectQuotient32, expectRemainder32), std::exception);
}

TEST(Decimal128, add)
{
    // 0 + 0 = 0
    Decimal128Wrapper lValue1(0x0L, 0x0);
    Decimal128Wrapper rValue1(0x0L, 0x0);
    Decimal128Wrapper result1;
    result1 = lValue1.Add(rValue1);
    EXPECT_EQ(Decimal128Wrapper(0x0L, 0x0), result1);

    // 1 + 0 = 1
    Decimal128Wrapper lValue2(0x0L, 0x1);
    Decimal128Wrapper rValue2(0x0L, 0x0);
    Decimal128Wrapper result2;
    result2 = lValue2.Add(rValue2);
    EXPECT_EQ(Decimal128Wrapper(0x0L, 0x1), result2);

    // 1 + 1 = 2
    Decimal128Wrapper lValue3(0x0L, 0x1);
    Decimal128Wrapper rValue3(0x0L, 0x1);
    Decimal128Wrapper result3;
    result3 = lValue3.Add(rValue3);
    EXPECT_EQ(Decimal128Wrapper(0x0L, 0x2), result3);

    // 4294967296 + 0 = 4294967296
    Decimal128Wrapper lValue4(0x0L, 0x100000000);
    Decimal128Wrapper rValue4(0x0L, 0x0);
    Decimal128Wrapper result4;
    result4 = lValue4.Add(rValue4);
    EXPECT_EQ(Decimal128Wrapper(0x0L, 0x100000000), result4);

    // 2147483648 + 2147483648 = 4294967296
    Decimal128Wrapper lValue5(0x0L, 0x80000000);
    Decimal128Wrapper rValue5(0x0L, 0x80000000);
    Decimal128Wrapper result5;
    result5 = lValue5.Add(rValue5);
    EXPECT_EQ(Decimal128Wrapper(0x0L, 0x100000000), result5);

    // 4294967296 + 8589934592 = 12884901888
    Decimal128Wrapper lValue6(0x0L, 0x100000000);
    Decimal128Wrapper rValue6(0x0L, 0x200000000);
    Decimal128Wrapper result6;
    result6 = lValue6.Add(rValue6);
    EXPECT_EQ(Decimal128Wrapper(0x0L, 0x300000000), result6);
}
#endif

TEST(Decimal128, addReturnOverflow)
{
    Decimal128Wrapper result;
    // 2 + 2
    Decimal128Wrapper lValue1(0x0L, 0x2);
    Decimal128Wrapper rValue1(0x0L, 0x2);
    result = lValue1.Add(rValue1);
    EXPECT_EQ(result.IsOverflow(), OpStatus::SUCCESS);

    // -99999999999999999999999999999999999999 + 99999999999999999999999999999999999999
    Decimal128Wrapper lValue3(0xCB3B4CA85A86C47A, 0x98A223FFFFFFFFF);
    Decimal128Wrapper rValue3(0x4B3B4CA85A86C47A, 0x98A223FFFFFFFFF);
    result = lValue3.Add(rValue3);
    EXPECT_EQ(result.IsOverflow(), OpStatus::SUCCESS);

    // 99999999999999999999999999999999999999 + (-99999999999999999999999999999999999999)
    Decimal128Wrapper lValue4(0x4B3B4CA85A86C47A, 0x98A223FFFFFFFFF);
    Decimal128Wrapper rValue4(0xCB3B4CA85A86C47A, 0x98A223FFFFFFFFF);
    result = lValue4.Add(rValue4);
    EXPECT_EQ(result.IsOverflow(), OpStatus::SUCCESS);

    // -99999999999999999999999999999999999998 + 1
    Decimal128Wrapper lValue6(0xCB3B4CA85A86C47A, 0x98A223FFFFFFFFE);
    Decimal128Wrapper rValue6(0x0L, 0x1);
    result = lValue6.Add(rValue6);
    EXPECT_EQ(result.IsOverflow(), OpStatus::SUCCESS);
}

TEST(Decimal128, multiply)
{
    // 0 * MAX_DECIMAL = 0
    Decimal128Wrapper lValue1(0x0L, 0x0);
    Decimal128Wrapper rValue1(0x4B3B4CA85A86C47A, 0x98A223FFFFFFFFF);
    Decimal128Wrapper result1;
    result1 = lValue1.Multiply(rValue1);
    EXPECT_EQ(Decimal128Wrapper(0x0L, 0x0), result1);

    // 1 * MAX_DECIMAL = MAX_DECIMAL
    Decimal128Wrapper lValue2(1);
    Decimal128Wrapper rValue2("99999999999999999999999999999999999999");
    Decimal128Wrapper result2;
    result2 = lValue2.Multiply(rValue2);
    EXPECT_EQ(Decimal128Wrapper("99999999999999999999999999999999999999"), result2);

    // 1 * MIN_DECIMAL = MIN_DECIMAL
    Decimal128Wrapper lValue3(1);
    Decimal128Wrapper rValue3("-99999999999999999999999999999999999999");
    Decimal128Wrapper result3;
    result3 = lValue3.Multiply(rValue3);
    EXPECT_EQ(Decimal128Wrapper("-99999999999999999999999999999999999999"), result3);

    // -1 * MAX_DECIMAL = MIN_DECIMAL
    Decimal128Wrapper lValue4("-1");
    Decimal128Wrapper rValue4("99999999999999999999999999999999999999");
    Decimal128Wrapper result4;
    result4 = lValue4.Multiply(rValue4);
    EXPECT_EQ(Decimal128Wrapper("-99999999999999999999999999999999999999"), result4);

    // -1 * MIN_DECIMAL = MAX_DECIMAL
    Decimal128Wrapper lValue5(~0, -0x1);
    Decimal128Wrapper rValue5(~0x4B3B4CA85A86C47A, -0x98A223FFFFFFFFF);
    Decimal128Wrapper result5;
    result5 = lValue5.Multiply(rValue5);
    EXPECT_EQ(Decimal128Wrapper(0x4B3B4CA85A86C47A, 0x98A223FFFFFFFFF), result5);

    // 18446744073709551615 * 72057594037927935 = 1329227995784915854385005392532865025
    Decimal128Wrapper lValue6(0x0L, 0xFFFFFFFFFFFFFFFF);
    Decimal128Wrapper rValue6(0x0L, 0xFFFFFFFFFFFFFF);
    Decimal128Wrapper result6;
    result6 = lValue6.Multiply(rValue6);
    EXPECT_EQ(Decimal128Wrapper(0xFFFFFFFFFFFFFE, 0xFF00000000000001), result6);

    // 18446742976727070720 * 4107341382742775296 = 75767070805130745462670906105260933120
    Decimal128Wrapper lValue7(0x0L, 0xFFFFFF0096BFB800);
    Decimal128Wrapper rValue7(0x0L, 0x39003539D9A51600);
    Decimal128Wrapper result7;
    result7 = lValue7.Multiply(rValue7);
    EXPECT_EQ(Decimal128Wrapper(0x39003500FB00AB76, 0x1CDBB17E11D00000), result7);

    // Integer.MAX_VALUE * Integer.MIN_VALUE = -4611686016279904256
    Decimal128Wrapper lValue8(0x0L, 0x7FFFFFFF);
    Decimal128Wrapper rValue8(-1, -0x80000000ll);
    Decimal128Wrapper result8;
    result8 = lValue8.Multiply(rValue8);
    EXPECT_EQ(Decimal128Wrapper("-4611686016279904256"), result8);

    // 99999999999999 * -1000000000000000000000000 = -99999999999999000000000000000000000000
    Decimal128Wrapper lValue9(0x0L, 0x5AF3107A3FFF);
    Decimal128Wrapper rValue9(~0xD3C2, -0x1BCECCEDA1000000ll);
    Decimal128Wrapper result9;
    result9 = lValue9.Multiply(rValue9);
    EXPECT_EQ(Decimal128Wrapper(~0x4B3B4CA85A85F0B7, -0xEDBB55525F000000), result9);

    // 12380837221737387489365741632769922889 * 3 = 37142511665212162468097224898309768667
    Decimal128Wrapper lValue10(0x950766754840EB8, 0xDF69328A17B69749);
    Decimal128Wrapper rValue10(0x0L, 0x3);
    Decimal128Wrapper result10;
    result10 = lValue10.Multiply(rValue10);
    EXPECT_EQ(Decimal128Wrapper(0x1BF16335FD8C2C2A, 0x9E3B979E4723C5DB), result10);
}

TEST(Decimal128, multiplyByInt)
{
    // 0 * 1 = 0
    Decimal128Wrapper lValue1(0x0L, 0x0);
    Decimal128Wrapper rValue1(0x0L, 0x1);
    Decimal128Wrapper result1;
    result1 = lValue1.Multiply(rValue1);
    EXPECT_EQ(Decimal128Wrapper(0x0L, 0x0), result1);

    // 2 * Integer.MAX_VALUE = 4294967294
    Decimal128Wrapper lValue2(0x0L, 0x2);
    Decimal128Wrapper rValue2(0x0L, 0x7FFFFFFF);
    Decimal128Wrapper result2;
    result2 = lValue2.Multiply(rValue2);
    EXPECT_EQ(Decimal128Wrapper(0x0L, 0xFFFFFFFE), result2);

    // Integer.MAX_VALUE * -3 = -6442450941
    Decimal128Wrapper lValue3(0x0L, 0x7FFFFFFF);
    Decimal128Wrapper rValue3(~0, -0x3);
    Decimal128Wrapper result3;
    result3 = lValue3.Multiply(rValue3);
    EXPECT_EQ(Decimal128Wrapper(~0, -0x17FFFFFFD), result3);

    // Integer.MIN_VALUE * -3 = 6442450944
    Decimal128Wrapper lValue4(~0, -0x80000000ll);
    Decimal128Wrapper rValue4(~0, -0x3);
    Decimal128Wrapper result4;
    result4 = lValue4.Multiply(rValue4);
    EXPECT_EQ(Decimal128Wrapper(0x0L, 0x180000000), result4);

    // 1267650600228229401496703205375 * 2 = 2535301200456458802993406410750
    Decimal128Wrapper lValue5(0xFFFFFFFFF, 0xFFFFFFFFFFFFFFFF);
    Decimal128Wrapper rValue5(0x0L, 0x2);
    Decimal128Wrapper result5;
    result5 = lValue5.Multiply(rValue5);
    EXPECT_EQ(Decimal128Wrapper(0x1FFFFFFFFF, 0xFFFFFFFFFFFFFFFE), result5);
}

TEST(Decimal128, multiplyOverflow)
{
    // 99999999999999 * -10000000000000000000000000
    Decimal128Wrapper lValue1(0x0L, 0x5AF3107A3FFF);
    Decimal128Wrapper rValue1(~0x108B2A, -0x161401484A000000);
    Decimal128Wrapper result1;
    result1 = lValue1.Multiply(rValue1);
    EXPECT_EQ(result1.IsOverflow(), OpStatus::OP_OVERFLOW);

    // MAX_DECIMAL * 10
    Decimal128Wrapper lValue2(0x4B3B4CA85A86C47A, 0x98A223FFFFFFFFF);
    Decimal128Wrapper rValue2(0x0L, 0xA);
    Decimal128Wrapper result2;
    result2 = lValue2.Multiply(rValue2);
    EXPECT_EQ(result1.IsOverflow(), OpStatus::OP_OVERFLOW);
}

TEST(Decimal128, rescaleLE18)
{
    // 10  (0) = 10
    Decimal128Wrapper value1(0x0L, 0xA);
    Decimal128Wrapper result1;
    result1 = value1.ReScale(0);
    EXPECT_EQ(Decimal128Wrapper(0x0L, 0xA), result1);

    // 15  (-1) = 2
    Decimal128Wrapper value3(0x0L, 0xF);
    Decimal128Wrapper result3;
    result3 = value3.ReScale(-1);
    EXPECT_EQ(Decimal128Wrapper(0x0L, 0x2), result3);

    // 1050  (-3) = 1
    Decimal128Wrapper value4(0x0L, 0x41A);
    Decimal128Wrapper result4;
    result4 = value4.ReScale(-3);
    EXPECT_EQ(Decimal128Wrapper(0x0L, 0x1), result4);

    // 15  (1) = 150
    Decimal128Wrapper value5(0x0L, 0xF);
    Decimal128Wrapper result5;
    result5 = value5.ReScale(1);
    EXPECT_EQ(Decimal128Wrapper(0x0L, 0x96), result5);

    // -14  (-1) = -1
    Decimal128Wrapper value6(~0, -0xE);
    Decimal128Wrapper result6;
    result6 = value6.ReScale(-1);
    EXPECT_EQ(Decimal128Wrapper(~0, -0x1), result6);

    // -14  (1) = -140
    Decimal128Wrapper value7(~0, -0xE);
    Decimal128Wrapper result7;
    result7 = value7.ReScale(1);
    EXPECT_EQ(Decimal128Wrapper(~0, -0x8C), result7);

    // 0  (1) = 0
    Decimal128Wrapper value8(0x0L, 0x0);
    Decimal128Wrapper result8;
    result8 = value8.ReScale(1);
    EXPECT_EQ(Decimal128Wrapper(0x0L, 0x0), result8);

    // 5  (-1) = 1
    Decimal128Wrapper value9(0x0L, 0x5);
    Decimal128Wrapper result9;
    result9 = value9.ReScale(-1);
    EXPECT_EQ(Decimal128Wrapper(0x0L, 0x1), result9);

    // 10  (10) = 100000000000
    Decimal128Wrapper value10(0x0L, 0xA);
    Decimal128Wrapper result10;
    result10 = value10.ReScale(10);
    EXPECT_EQ(Decimal128Wrapper(0x0L, 0x174876E800), result10);

    // 150500000000000000000  (-18) = 151
    Decimal128Wrapper value14(0x8, 0x289B689DE84A0000);
    Decimal128Wrapper result14;
    result14 = value14.ReScale(-18);
    EXPECT_EQ(Decimal128Wrapper(0x0L, 0x97), result14);

    // -140000000000000000000  (-18) = -140
    Decimal128Wrapper value15(~0x7, -0x96E3EA3F8AB00000);
    Decimal128Wrapper result15;
    result15 = value15.ReScale(-18);
    EXPECT_EQ(Decimal128Wrapper(~0, -0x8C), result15);

    // 9223372036854775808  (-18) = 9
    Decimal128Wrapper value16(0x0L, 0x8000000000000000);
    Decimal128Wrapper result16;
    result16 = value16.ReScale(-18);
    EXPECT_EQ(Decimal128Wrapper(0x0L, 0x9), result16);

    // 4611686018427387904  (-18) = 5
    Decimal128Wrapper value17(0x0L, 0x4000000000000000);
    Decimal128Wrapper result17;
    result17 = value17.ReScale(-18);
    EXPECT_EQ(Decimal128Wrapper(0x0L, 0x5), result17);

    // 99999999999999999999999999999999999999  (-1) = 10000000000000000000000000000000000000
    Decimal128Wrapper value19(0x4B3B4CA85A86C47A, 0x98A223FFFFFFFFF);
    Decimal128Wrapper result19;
    result19 = value19.ReScale(-1);
    EXPECT_EQ(Decimal128Wrapper(0x785EE10D5DA46D9, 0xF436A000000000), result19);

    // -99999999999999999999999999999999999999  (-10) = -10000000000000000000000000000
    Decimal128Wrapper value20("-99999999999999999999999999999999999999");
    Decimal128Wrapper result20;
    result20 = value20.ReScale(-10);
    EXPECT_EQ(Decimal128Wrapper(~0x204FCE5E, -0x3E25026110000000).ToInt128(), result20.ToInt128());
}
#ifdef DIV_CAL_NEED_CHECK
TEST(Decimal128, rescaleGT18)
{
    // 10  (-20) = 0
    Decimal128Wrapper value2(0x0L, 0xA);
    Decimal128Wrapper result2;
    result2 = value2.ReScale(-20);
    EXPECT_EQ(Decimal128Wrapper(0x0L, 0x0), result2);

    // 150000000000000000000  (-20) = 2
    Decimal128Wrapper value11(0x8, 0x21AB0D4414980000);
    Decimal128Wrapper result11;
    result11 = value11.ReScale(-20);
    EXPECT_EQ(Decimal128Wrapper(0x0L, 0x2), result11);

    // -140000000000000000000  (-20) = -1
    Decimal128Wrapper value12(0x8000000000000007, 0x96E3EA3F8AB00000);
    Decimal128Wrapper result12;
    result12 = value12.ReScale(-20);
    EXPECT_EQ(Decimal128Wrapper(0x8000000000000000, 0x1), result12);

    // 50000000000000000000  (-20) = 1
    Decimal128Wrapper value13(0x2, 0xB5E3AF16B1880000);
    Decimal128Wrapper result13;
    result13 = value13.ReScale(-19);
    EXPECT_EQ(Decimal128Wrapper(0x0L, 0x1), result13);

    // 4611686018427387904  (-19) = 0
    Decimal128Wrapper value18(0x0L, 0x4000000000000000);
    Decimal128Wrapper result18;
    result18 = value18.ReScale(-19);
    EXPECT_EQ(Decimal128Wrapper(0x0L, 0x0), result18);

    // 1  (37) = 10000000000000000000000000000000000000
    Decimal128Wrapper value21(0x0L, 0x1);
    Decimal128Wrapper result21;
    result21 = value21.ReScale(37);
    EXPECT_EQ(Decimal128Wrapper(0x785EE10D5DA46D9, 0xF436A000000000), result21);

    // -1  (37) = -10000000000000000000000000000000000000
    Decimal128Wrapper value22(0x8000000000000000, 0x1);
    Decimal128Wrapper result22;
    result22 = value22.ReScale(37);
    EXPECT_EQ(Decimal128Wrapper(0x8785EE10D5DA46D9, 0xF436A000000000), result22);

    // 10000000000000000000000000000000000000  (-37) = 1
    Decimal128Wrapper value23(0x785EE10D5DA46D9, 0xF436A000000000);
    Decimal128Wrapper result23;
    result23 = value23.ReScale(-37);
    EXPECT_EQ(Decimal128Wrapper(0x0L, 0x1), result23);
}
#endif
TEST(Decimal128, DecimalRepresentationEquivalence)
{
    Decimal128Wrapper a0("0");
    Decimal128Wrapper b0(0);
    Decimal128Wrapper c0(0L, 0);
    EXPECT_EQ(a0, b0);
    EXPECT_EQ(a0, c0);

    // Negative values represented as 2's complement
    Decimal128Wrapper a1("-1");
    Decimal128Wrapper b1(-1);
    Decimal128Wrapper c1(~0, -1);
    EXPECT_EQ(a1, b1);
    EXPECT_EQ(a1, c1);

    Decimal128Wrapper a2("1");
    Decimal128Wrapper b2(1);
    Decimal128Wrapper c2(0L, 1);
    EXPECT_EQ(a2, b2);
    EXPECT_EQ(a2, c2);

    // Bit patterns calculated from WolframAlpha
    Decimal128Wrapper a3("99999999999999999999999999999999999999");
    Decimal128Wrapper c3(0x4b3b4ca85a86c47a, 0x098a223fffffffff);
    EXPECT_EQ(a3, c3);

    Decimal128Wrapper a4("-99999999999999999999999999999999999999");
    Decimal128Wrapper c4(~0x4b3b4ca85a86c47a, -0x098a223fffffffff);
    EXPECT_EQ(a4, c4);

    // -(2^65 + 2^64)
    Decimal128Wrapper a5("-55340232221128654848");
    Decimal128Wrapper c5(-3, 0);
    EXPECT_EQ(a5, c5);
}
} // namespace Decimal128Test
