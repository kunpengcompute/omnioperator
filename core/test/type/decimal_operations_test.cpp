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
#include "operator/execution_context.h"
#include "operator/aggregation/aggregation.h"

using namespace omniruntime::type;

namespace DecimalOperationsTest {
TEST(DecimalOperations, add_with_overflow)
{
    Decimal128Wrapper left = 3;
    Decimal128Wrapper right = 6;
    Decimal128Wrapper result = 0;
    int64_t expectValue = 9;
    result = left.Add(right);
    OpStatus overflow = result.IsOverflow();
    EXPECT_EQ(result, expectValue);
    EXPECT_EQ(overflow, OpStatus::SUCCESS);

    Decimal128Wrapper left1;
    Decimal128Wrapper right1 = 2;
    Decimal128Wrapper result1 = 0;
    left1.SetValue(~0, -3);
    Decimal128Wrapper expectValue1(~0, -1);
    result1 = left1.Add(right1);
    OpStatus overflow1 = result1.IsOverflow();
    EXPECT_EQ(result1.HighBits(), expectValue1.HighBits());
    EXPECT_EQ(result1.LowBits(), expectValue1.LowBits());
    EXPECT_EQ(overflow1, OpStatus::SUCCESS);

    Decimal128Wrapper left2;
    Decimal128Wrapper right2 = 2;
    Decimal128Wrapper result2 = 0;
    left2.SetValue(~0, -1);
    int64_t expectValue3 = 1;
    result2 = left2.Add(right2);
    OpStatus overflow2 = result2.IsOverflow();
    EXPECT_EQ(result2, expectValue3);
    EXPECT_EQ(overflow2, OpStatus::SUCCESS);

    Decimal128Wrapper left3;
    Decimal128Wrapper right3;
    Decimal128Wrapper result3 = 0;
    left3.SetValue(~0, -1);
    right3.SetValue(~0, -1);
    result3 = left3.Add(right3);
    OpStatus overflow3 = result3.IsOverflow();
    EXPECT_EQ(result3.HighBits(), ~0);
    EXPECT_EQ(result3.LowBits(), -2);
    EXPECT_EQ(overflow3, OpStatus::SUCCESS);

    Decimal128Wrapper one;
    Decimal128Wrapper negativeOne;
    one.SetValue(0, 1);
    negativeOne.SetValue(~0, -1);
    Decimal128Wrapper expected(0L, 0);
    Decimal128Wrapper result4 = one.Add(negativeOne);
    OpStatus overflow4 = result4.IsOverflow();
    EXPECT_EQ(result4.HighBits(), expected.HighBits());
    EXPECT_EQ(result4.LowBits(), expected.LowBits());
    EXPECT_EQ(overflow4, OpStatus::SUCCESS);
}

TEST(DecimalOperations, exceeds_or_equal_ten_to_thirty_eight)
{
    Decimal128Wrapper result;
    int64_t c3 = 0;
    result.SetValue(c3, 1);
    EXPECT_EQ(result.IsOverflow(), OpStatus::SUCCESS);

    int64_t c4 = 1ll << 63;
    result.SetValue(c4, 1);
    EXPECT_EQ(result.IsOverflow(), OpStatus::OP_OVERFLOW);

    int64_t c5 = 0x4b3b4ca85a86c47aL;
    result.SetValue(c5, 1);
    EXPECT_EQ(result.IsOverflow(), OpStatus::SUCCESS);
}

TEST(DecimalOperations, decode_avg_decimal)
{
    using namespace omniruntime::op;
    AggregateState *state;
    ExecutionContext executionContext;
    state = executionContext.GetArena()->Allocate(32);

    int128_t oldDec = CreateInt128(2, 3);
    int64_t oldOther = 1;
    int64_t oldOverflow = 1;
    EncodeAvgDecimal(reinterpret_cast<DecimalAverageState *>(state), oldDec, oldOverflow, oldOther);

    // decode phase
    int128_t newDec = 0;
    int64_t newOverflow = 0;
    int64_t newOther = 0;
    DecodeAvgDecimal(reinterpret_cast<DecimalAverageState *>(state), newDec, newOverflow, newOther);
    EXPECT_EQ(newOverflow, oldOverflow);
    EXPECT_EQ(newOverflow, oldOverflow);
}

TEST(DecimalOperations, divide)
{
    Decimal128Wrapper dividend(~0, -200'0000'0000'0163'5618);
    Decimal128Wrapper divisor(0L, 4);
    Decimal128Wrapper result;
    result = dividend.Divide(divisor, 0);
    int64_t expectedVal = -50'0000'0000'0040'8905;
    EXPECT_EQ(expectedVal, result.LowBits());
}

TEST(DecimalOperations, rescale_decimal64)
{
    Decimal64 d(10);
    d.ReScale(2);
    EXPECT_EQ(1000LL, d.GetValue());
}

TEST(DecimalOperations, rescale_decimal128)
{
    Decimal128Wrapper val = 10LL;
    val.ReScale(2);
    Decimal128Wrapper expected(0L, 1000LL);
    EXPECT_EQ(expected, val);
}

TEST(DecimalOperations, rescale_decimal64_to_128)
{
    Decimal128Wrapper result(10);
    result.ReScale(2);
    Decimal128Wrapper expected(0L, 1000LL);
    EXPECT_EQ(expected, result);
}

TEST(DecimalOperations, rescale_decimal128_round_to_zero_when_rescale_larger_than_0)
{
    Decimal128Wrapper input("1234567891234567891234");
    Decimal128Wrapper output;
    int32_t rescale = 5;
    output = input;
    output.ReScale(rescale, RoundingMode::ROUND_FLOOR);
    EXPECT_EQ(output.IsOverflow(), OpStatus::SUCCESS);
    Decimal128Wrapper expect("123456789123456789123400000");
    EXPECT_EQ(expect, output);
    input = Decimal128Wrapper("1234567891234567891234");
    rescale = -5;
    output = input;
    output.ReScale(rescale, RoundingMode::ROUND_FLOOR);
    EXPECT_EQ(output.IsOverflow(), OpStatus::SUCCESS);
    expect = Decimal128Wrapper("12345678912345678");
    EXPECT_EQ(expect, output);
    input = Decimal128Wrapper("1234567891234567891234");
    rescale = 0;
    output = input;
    output.ReScale(rescale, RoundingMode::ROUND_FLOOR);
    EXPECT_EQ(output.IsOverflow(), OpStatus::SUCCESS);
    EXPECT_EQ(input, output);
}

TEST(DecimalOperations, rescale_decimal64_round_to_zero_when_rescale_larger_than_0)
{
    int64_t input = 1234'5678'9123L;
    int32_t rescale = 5;
    Decimal64 output = Decimal64(input).ReScale(rescale, RoundingMode::ROUND_FLOOR);
    EXPECT_EQ(output.IsOverflow(), OpStatus::SUCCESS);
    int64_t expect = 1'2345'6789'1230'0000L;
    EXPECT_EQ(expect, output.GetValue());
    input = 1234'5678'9123L;
    rescale = -5;
    output = Decimal64(input).ReScale(rescale, RoundingMode::ROUND_FLOOR);
    EXPECT_EQ(output.IsOverflow(), OpStatus::SUCCESS);
    expect = 1234567L;
    EXPECT_EQ(expect, output.GetValue());
    std::string s = "123456789123";
    Decimal64 res(s);
    rescale = 0;
    output = res.SetScale(rescale).ReScale(rescale, RoundingMode::ROUND_FLOOR);
    EXPECT_EQ(output.IsOverflow(), OpStatus::SUCCESS);
    EXPECT_EQ(input, output.GetValue());
}

TEST(DecimalOperations, is_unscaled_long_overflow)
{
    int64_t deci1 = 100'0000'0000'0000'0000L;
    EXPECT_TRUE(DecimalOperations::IsUnscaledLongOverflow(deci1, 16, 2));
    EXPECT_TRUE(DecimalOperations::IsUnscaledLongOverflow(-deci1, 16, 2));
    EXPECT_FALSE(DecimalOperations::IsUnscaledLongOverflow(deci1, 19, 2));
    EXPECT_FALSE(DecimalOperations::IsUnscaledLongOverflow(-deci1, 19, 2));

    int64_t deci2 = 10'0000'0000'0000'0000L;
    EXPECT_TRUE(DecimalOperations::IsUnscaledLongOverflow(deci2, 17, 2));
    EXPECT_TRUE(DecimalOperations::IsUnscaledLongOverflow(-deci2, 17, 2));
    EXPECT_FALSE(DecimalOperations::IsUnscaledLongOverflow(deci2, 18, 2));
    EXPECT_FALSE(DecimalOperations::IsUnscaledLongOverflow(-deci2, 18, 2));
}
} // namespace DecimalOperationsTest
