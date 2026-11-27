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

#include <vector>
#include "gtest/gtest.h"
#include "codegen/functions/dtoa.h"

using namespace omniruntime::codegen::function;
namespace DtoaTest {

void ExpectEq(int *res, int *expect, int len)
{
    for (int i = 0; i < len; i++) {
        EXPECT_EQ(res[i], expect[i]);
    }
}
TEST(FDBigInteger, Mul)
{
    char digit[] = "4237896478126318254871286731";
    FDBigInteger a(4, digit, 0, 28);
    int expect1[] = {1073222603, 1669324860, -1896826123, 0};
    ExpectEq(expect1, a.GetData(), 4);

    FDBigInteger a2 = a.MulBy10();
    int expect2[] = {2142291438, -486620582, -1788392043, 5};
    ExpectEq(expect2, a2.GetData(), 4);
}

TEST(FDBigInteger, LeftShift)
{
    char digit[] = "4237896478126318254871286731";
    FDBigInteger a(4, digit, 0, 28);
    FDBigInteger a2 = a.LeftShift(123);
    int expect2[] = {1476395008, -503332706, -1424228607, 74941911};
    ExpectEq(expect2, a2.GetData(), 4);
}

TEST(FDBigInteger, AddAndCmp)
{
    char digit[] = "4237896478126318254871286731";
    FDBigInteger a(4, digit, 0, 28);
    FDBigInteger a2 = a.LeftShift(123);
    int res = a.AddAndCmp(a, a2);
    EXPECT_EQ(-1, res);
}

TEST(FDBigInteger, DoubleToStringTest)
{
    EXPECT_EQ(DoubleToString::DoubleToStringConverter(-1.7976931348623157E308), "-1.7976931348623157E308");
    EXPECT_EQ(DoubleToString::DoubleToStringConverter(1.23E34), "1.23E34");
    EXPECT_EQ(DoubleToString::DoubleToStringConverter(3.1415926535437834), "3.1415926535437833");
    EXPECT_EQ(DoubleToString::DoubleToStringConverter(-9.881086091829435E16), "-9.8810860918294352E16");
    EXPECT_EQ(DoubleToString::DoubleToStringConverter(37.9853125), "37.9853125");
    EXPECT_EQ(DoubleToString::DoubleToStringConverter(0.000001), "1.0E-6");
    EXPECT_EQ(DoubleToString::DoubleToStringConverter(0.9999999), "0.9999999");
    EXPECT_EQ(DoubleToString::DoubleToStringConverter(1.7976931348623157E308), "1.7976931348623157E308");
    EXPECT_EQ(DoubleToString::DoubleToStringConverter(std::numeric_limits<double>::infinity()), "Infinity");
    EXPECT_EQ(DoubleToString::DoubleToStringConverter(-std::numeric_limits<double>::infinity()), "-Infinity");
    EXPECT_EQ(DoubleToString::DoubleToStringConverter(std::numeric_limits<double>::quiet_NaN()), "NaN");
}
} // namespace DtoaTest
