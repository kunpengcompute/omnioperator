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
#include "type/double_utils.h"

namespace omniruntime::type {
TEST(BigInteger, Constructor)
{
    BigInteger bigInt1("1234123");
    EXPECT_EQ(bigInt1.ToHexString(), "12D4CB");

    BigInteger bigInt2(3, 4);
    EXPECT_EQ(bigInt2.ToHexString(), "51");

    BigInteger bigInt3((uint16_t)12);
    EXPECT_EQ(bigInt3.ToHexString(), "C");

    BigInteger bigInt4((uint64_t)1234123);
    EXPECT_EQ(bigInt4.ToHexString(), "12D4CB");

    const BigInteger &bigInt5(bigInt1);
    EXPECT_EQ(bigInt5.ToHexString(), "12D4CB");
}

TEST(BigInteger, Arithmetic)
{
    BigInteger bigInt1("89709237459812649382842305972398479328755981320");
    bigInt1.AddUInt64(10001);
    EXPECT_EQ(bigInt1.ToHexString(), "FB6B38AA05FD4C41758302DD1F40EEE5F264319");

    BigInteger bigInt2("1401029347897327120935479182347");
    bigInt1.AddBigInteger(bigInt2);
    EXPECT_EQ(bigInt1.ToHexString(), "FB6B38AA05FD4D5C650809D21B7F49F823EC724");

    bigInt1.ShiftLeft(8);
    EXPECT_EQ(bigInt1.ToHexString(), "FB6B38AA05FD4D5C650809D21B7F49F823EC72400");

    BigInteger bigInt3("999999233234");
    bigInt3.MultiplyByUInt32(3456345);
    EXPECT_EQ(bigInt3.ToHexString(), "2FF7650DFD804F02");

    bigInt3.MultiplyByUInt64(3456345);
    EXPECT_EQ(bigInt3.ToHexString(), "9E1BAD974F414E857F1B2");

    BigInteger bigInt4("1239987");
    bigInt4.MultiplyByPowerOfTen(10);
    EXPECT_EQ(bigInt4.ToHexString(), "2C0D9DB69C6C00");

    bigInt4.Times10();
    EXPECT_EQ(bigInt4.ToHexString(), "1B88829221C3800");

    EXPECT_EQ(BigInteger::Compare(bigInt4, bigInt3), -1);
    EXPECT_EQ(BigInteger::Compare(bigInt3, bigInt4), 1);
    EXPECT_EQ(BigInteger::Compare(bigInt3, bigInt3), 0);

    EXPECT_EQ(BigInteger::Equal(bigInt3, bigInt3), true);
}

TEST(BigInteger, AddUInt64)
{
    BigInteger bigInteger("0");
    bigInteger.AddUInt64(0xA);
    EXPECT_EQ("A", bigInteger.ToHexString());

    bigInteger.AssignUInt16(0x1);
    bigInteger.ShiftLeft(100);
    bigInteger.AddUInt64(1);
    EXPECT_EQ("10000000000000000000000001", bigInteger.ToHexString());

    bigInteger.AssignUInt16(0x1);
    bigInteger.ShiftLeft(100);
    bigInteger.AddUInt64(0xFFFF);
    EXPECT_EQ("1000000000000000000000FFFF", bigInteger.ToHexString());

    bigInteger.AssignUInt16(0x1);
    bigInteger.ShiftLeft(100);
    bigInteger.AddUInt64(DOUBLE_CONVERSION_UINT64_2PART_C(0x1, 00000000));
    EXPECT_EQ("10000000000000000100000000", bigInteger.ToHexString());

    bigInteger.AssignUInt16(0x1);
    bigInteger.ShiftLeft(100);
    bigInteger.AddUInt64(DOUBLE_CONVERSION_UINT64_2PART_C(0xFFFF, 00000000));
    EXPECT_EQ("10000000000000FFFF00000000", bigInteger.ToHexString());
}

TEST(BigInteger, MultiplyPowerOfTen)
{
    BigInteger bigInteger("1234");
    bigInteger.MultiplyByPowerOfTen(1);
    EXPECT_EQ("3034", bigInteger.ToHexString());

    bigInteger = BigInteger("1234");
    bigInteger.MultiplyByPowerOfTen(2);
    EXPECT_EQ("1E208", bigInteger.ToHexString());

    bigInteger = BigInteger("1234");
    bigInteger.MultiplyByPowerOfTen(20);
    EXPECT_EQ("1A218703F6C783200000", bigInteger.ToHexString());

    bigInteger = BigInteger("1234");
    bigInteger.MultiplyByPowerOfTen(50);
    EXPECT_EQ("149D1B4CFED03B23AB5F4E1196EF45C08000000000000", bigInteger.ToHexString());

    bigInteger = BigInteger("1234");
    bigInteger.MultiplyByPowerOfTen(100);
    EXPECT_EQ("5827249F27165024FBC47DFCA9359BF316332D1B91ACEECF471FBAB06D9B2"
              "0000000000000000000000000",
              bigInteger.ToHexString());

    bigInteger = BigInteger("1234");
    bigInteger.MultiplyByPowerOfTen(1000);
    EXPECT_EQ("1258040F99B1CD1CC9819C676D413EA50E4A6A8F114BB0C65418C62D399B81"
              "6361466CA8E095193E1EE97173553597C96673AF67FAFE27A66E7EF2E5EF2E"
              "E3F5F5070CC17FE83BA53D40A66A666A02F9E00B0E11328D2224B8694C7372"
              "F3D536A0AD1985911BD361496F268E8B23112500EAF9B88A9BC67B2AB04D38"
              "7FEFACD00F5AF4F764F9ABC3ABCDE54612DE38CD90CB6647CA389EA0E86B16"
              "BF7A1F34086E05ADBE00BD1673BE00FAC4B34AF1091E8AD50BA675E0381440"
              "EA8E9D93E75D816BAB37C9844B1441C38FC65CF30ABB71B36433AF26DD97BD"
              "ABBA96C03B4919B8F3515B92826B85462833380DC193D79F69D20DD6038C99"
              "6114EF6C446F0BA28CC772ACBA58B81C04F8FFDE7B18C4E5A3ABC51E637FDF"
              "6E37FDFF04C940919390F4FF92000000000000000000000000000000000000"
              "00000000000000000000000000000000000000000000000000000000000000"
              "00000000000000000000000000000000000000000000000000000000000000"
              "00000000000000000000000000000000000000000000000000000000000000"
              "0000000000000000000000000000",
              bigInteger.ToHexString());
}

TEST(BigInteger, DivideModuloIntBignum)
{
    BigInteger bigInteger;
    BigInteger other;
    BigInteger third;

    bigInteger.AssignUInt16(10);
    other.AssignUInt16(2);
    EXPECT_EQ(5, bigInteger.DivideModuloIntBigInteger(other));
    EXPECT_EQ("0", bigInteger.ToHexString());

    bigInteger.AssignUInt16(10);
    bigInteger.ShiftLeft(500);
    other.AssignUInt16(2);
    other.ShiftLeft(500);
    EXPECT_EQ(5, bigInteger.DivideModuloIntBigInteger(other));
    EXPECT_EQ("0", bigInteger.ToHexString());

    bigInteger.AssignUInt16(10);
    bigInteger.ShiftLeft(500);
    other.AssignUInt16(1);
    bigInteger.AddBigInteger(other);
    other.AssignUInt16(2);
    other.ShiftLeft(500);
    EXPECT_EQ(5, bigInteger.DivideModuloIntBigInteger(other));
    EXPECT_EQ("1", bigInteger.ToHexString());

    bigInteger.AssignUInt16(10);
    bigInteger.ShiftLeft(500);
    other = bigInteger;
    bigInteger.MultiplyByUInt32(0x1234);
    third.AssignUInt16(0xFFF);
    other.SubtractBigInteger(third);
    EXPECT_EQ(0x1234, bigInteger.DivideModuloIntBigInteger(other));
    EXPECT_EQ("1232DCC", bigInteger.ToHexString());
    EXPECT_EQ(0, bigInteger.DivideModuloIntBigInteger(other));
    EXPECT_EQ("1232DCC", bigInteger.ToHexString());
}

TEST(BigInteger, Square)
{
    BigInteger bigInteger;
    bigInteger.AssignUInt16(1);
    bigInteger.Square();
    EXPECT_EQ("1", bigInteger.ToHexString());

    bigInteger.AssignUInt16(2);
    bigInteger.Square();
    EXPECT_EQ("4", bigInteger.ToHexString());

    bigInteger.AssignUInt16(10);
    bigInteger.Square();
    EXPECT_EQ("64", bigInteger.ToHexString());
}
} // namespace omniruntime::type
