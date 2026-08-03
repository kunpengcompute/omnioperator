/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2024. All rights reserved.
 * Description: function test
 */
#include <string>
#include <vector>
#include <chrono>
#include "gtest/gtest.h"
#include "expression/expressions.h"
#include "codegen/functions/stringfunctions.h"
#include "codegen/functions/mathfunctions.h"
#include "codegen/functions/murmur3_hash.h"
#include "codegen/functions/xxhash64_hash.h"
#include "codegen/functions/dictionaryfunctions.h"
#include "codegen/functions/udffunctions.h"
#ifndef EXCLUDE_DUPLICATE_CODEGEN_FUNCTIONS
#include "codegen/functions/datetime_functions.h"
#include "codegen/functions/json_functions.h"
#endif
#include "jni_mock.h"
#include "udf/cplusplus/jni_util.h"
#include "../util/test_util.h"

namespace omniruntime {
using namespace omniruntime::op;
using namespace omniruntime::expressions;
using namespace omniruntime::codegen::function;

#ifndef EXCLUDE_DUPLICATE_CODEGEN_FUNCTIONS
void Date32TruncTest(const std::string &input, const std::string &level, const std::string &expect, bool expectIsNull)
{
    int64_t date32Value;
    Date32::StringToDate32(input.c_str(), input.length(), date32Value);
    bool isNull = false;
    int32_t result = DateTruncRetNull(&isNull, static_cast<int32_t>(date32Value), level.c_str(), level.length());
    Date32 resDate(result);
    char resStr[16];
    resDate.ToString(resStr, 16);
    if (!expectIsNull) {
        EXPECT_EQ(expect, std::string(resStr, 10));
    }
    EXPECT_EQ(isNull, expectIsNull);
}
#endif

/*
 * context helper tests
 */

static void Md5StringTest(const std::string &input, const std::string &expect)
{
    auto context = new ExecutionContext();
    int64_t contextPtr = reinterpret_cast<int64_t>(context);
    int32_t outLen;
    char *res = Md5Str(contextPtr, input.c_str(), input.size(), false, &outLen);
    EXPECT_EQ(expect, std::string(res, outLen));
    delete context;
}

TEST(FunctionTest, ArenaAllocatorMalloc)
{
    auto execContext = new ExecutionContext();
    int64_t contextptr = reinterpret_cast<int64_t>(execContext);
    char *ptr;
    for (uint64_t i = 1; i <= execContext->GetArena()->TotalBytes() / 256; ++i) {
        ptr = ArenaAllocatorMalloc(contextptr, 256);
        EXPECT_NE(ptr, nullptr);
        // check 256 is allocated each call;
        EXPECT_EQ(256 * i, execContext->GetArena()->TotalBytes() - execContext->GetArena()->AvailBytes());
    }
    delete execContext;
}

TEST(FunctionTest, ArenaAllocatorReset)
{
    auto execContext = new ExecutionContext();
    int64_t contextptr = reinterpret_cast<int64_t>(execContext);

    ArenaAllocatorMalloc(contextptr, 1);
    EXPECT_EQ(execContext->GetArena()->AvailBytes(), execContext->GetArena()->TotalBytes() - 1);
    ArenaAllocatorReset(contextptr);
    EXPECT_EQ(execContext->GetArena()->AvailBytes(), execContext->GetArena()->TotalBytes());

    ArenaAllocatorMalloc(contextptr, execContext->GetArena()->TotalBytes());
    EXPECT_EQ(execContext->GetArena()->AvailBytes(), 0);
    ArenaAllocatorReset(contextptr);
    EXPECT_EQ(execContext->GetArena()->AvailBytes(), execContext->GetArena()->TotalBytes());
    delete execContext;
}

/*
 * Murmur3 hash tests
 */

TEST(FunctionTest, Mm3Int8)
{
    EXPECT_EQ(Mm3Int8(127, false, 42, false), 1135925485);
}

TEST(FunctionTest, Mm3Int16)
{
    EXPECT_EQ(Mm3Int16(32477, false, 42, false), -476749289);
}

TEST(FunctionTest, Mm3Int32)
{
    EXPECT_EQ(Mm3Int32(-2147483648, false, 42, false), 723455942);
}

TEST(FunctionTest, Mm3Int64)
{
    EXPECT_EQ(Mm3Int64(static_cast<int64_t>(-2147483648), false, 42, false), -1889108749);
}

TEST(FunctionTest, Mm3Double)
{
    EXPECT_EQ(Mm3Double(123.456, false, 42, false), -39269148);
}

TEST(FunctionTest, Mm3String)
{
    std::string value("hello world");
    EXPECT_EQ(Mm3String(const_cast<char *>(value.c_str()), 11, false, 42, false), -1528836094);
}

TEST(FunctionTest, Md5String)
{
    Md5StringTest("hellow world!", "df6a571d99e454e99be79a258ca3a57d");
    Md5StringTest("123412345892376487612983", "1eac0a767ccdf782870bac35db9af264");
    Md5StringTest("1eac0a767ccdf782870bac35db9af2642345", "582aa488923f89ea825358ffe387f5ae");
    Md5StringTest("", "d41d8cd98f00b204e9800998ecf8427e");
    Md5StringTest("  1", "37705de0752d1027f8fc3b3f390c448d");
    Md5StringTest("1  ", "42609f6cf2cbebfe205241bf26e2e0ef");
    Md5StringTest("1", "c4ca4238a0b923820dcc509a6f75849b");
    Md5StringTest("uwefbuiwef7821ho;o;lhf8923golecg288823pfhl;hsf2893fgOIHDWQIUHFGEWF7823GHQDFddj2",
        "03f2ace2c433c4b743787d96b41ed1b6");
}

static void EmptyToNullTest(const std::string &input)
{
    int32_t outLen;
    const char *res = EmptyToNull(input.c_str(), input.size(), true, &outLen);
    EXPECT_EQ(nullptr, res);
    EXPECT_EQ(outLen, 0);
    res = EmptyToNull(input.c_str(), input.size(), false, &outLen);
    EXPECT_EQ(outLen, 0);
}

static void EmptyToNullNotNullTest(const std::string &input, const std::string &expect)
{
    int32_t outLen;
    const char *res = EmptyToNull(input.c_str(), input.size(), false, &outLen);
    EXPECT_EQ(expect, std::string(res, outLen));
}

TEST(FunctionTest, EmptyToNull)
{
    EmptyToNullNotNullTest("hellow world!", "hellow world!");
    EmptyToNullNotNullTest("123412345892376487612983", "123412345892376487612983");
    EmptyToNullNotNullTest("   ", "   ");
    EmptyToNullTest("");
}

/*
 * XxHash64 hash tests
 */
TEST(FunctionTest, XxH64Short)
{
    EXPECT_EQ(XxH64Int16(1, false, 42, false), -6698625589789238999);
}

TEST(FunctionTest, XxH64Int)
{
    EXPECT_EQ(XxH64Int32(123, false, 42, false), 5513549449271372758);
}

TEST(FunctionTest, XxH64Long)
{
    EXPECT_EQ(XxH64Int64(123L, false, 42, false), -3178482946328430151);
}

TEST(FunctionTest, XxH64Float)
{
    float v = -1.47579571e9f;
    EXPECT_EQ(XxH64Float(v, false, 42, false), -5090957869082801309);
}

TEST(FunctionTest, XxH64Double)
{
    EXPECT_EQ(XxH64Double(123.456, false, 42, false), -6938331816624033461);
}
TEST(FunctionTest, XxH64Boolean)
{
    EXPECT_EQ(XxH64Boolean(true, false, 42, false), -6698625589789238999);
}

TEST(FunctionTest, XxH64String)
{
    EXPECT_EQ(XxH64String("hello world", 11, false, 42, false), 7620854247404556961);
}

TEST(FunctionTest, XxH64Decimal64)
{
    EXPECT_EQ(XxH64Decimal64(430, 7, 0, false, 42, false), -3069041474468904433);
}

TEST(FunctionTest, XxH64Decimal128)
{
    auto value1 = Decimal128(0x80111e8f827844e5, 0x7c03905da66c0000);
    auto value2 = Decimal128(0x00002bd35ae79a49, 0xf98f65489dd30001);
    EXPECT_EQ(XxH64Decimal128(value1.HighBits(), value1.LowBits(), 38, 16, false, 42, false), -216624505269361667);
    EXPECT_EQ(XxH64Decimal128(value2.HighBits(), value2.LowBits(), 38, 16, false, 42, false), 8484287969139273592);
    auto value3 = Decimal128(0x0, 0x29a2241af62bffff);
    auto value4 = Decimal128(0x8000000000000000, 0x0de0b6b3a763ffff);
    EXPECT_EQ(XxH64Decimal128(value3.HighBits(), value3.LowBits(), 38, 18, false, 42, false), -5056633277332826927);
    EXPECT_EQ(XxH64Decimal128(value4.HighBits(), value4.LowBits(), 38, 18, false, 42, false), -6640857474798889004);
}

/*
 * Math functions:
 */
TEST(FunctionTest, Abs)
{
    EXPECT_EQ(10, Abs<int32_t>(10));
    EXPECT_EQ(24, Abs<int32_t>(-24));
    EXPECT_EQ(0, Abs<int32_t>(0));
    EXPECT_EQ(10, Abs<int64_t>(10));
    EXPECT_EQ(24, Abs<int64_t>(-24));
    EXPECT_EQ(0, Abs<int64_t>(0));
}

TEST(FunctionTest, CastInt8ToInt64)
{
    int8_t test1 = 10;
    int8_t test2 = -24;
    int8_t test3 = 0;
    int8_t test4 = std::numeric_limits<int8_t>::min();
    int8_t test5 = std::numeric_limits<int8_t>::max();
    int64_t baseline = 1;
    auto result = CastInt8ToInt64(test1);
    bool isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(10, CastInt8ToInt64(result));

    result = CastInt8ToInt64(test2);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(-24, result);

    result = CastInt8ToInt64(test3);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(0, CastInt8ToInt64(result));

    result = CastInt8ToInt64(test4);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(std::numeric_limits<int8_t>::min(), result);

    result = CastInt8ToInt64(test5);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(std::numeric_limits<int8_t>::max(), result);
}

TEST(FunctionTest, CastInt8ToInt32)
{
    int8_t test1 = 10;
    int8_t test2 = -24;
    int8_t test3 = 0;
    int8_t test4 = std::numeric_limits<int8_t>::min();
    int8_t test5 = std::numeric_limits<int8_t>::max();
    int32_t baseline = 1;
    auto result = CastInt8ToInt32(test1);
    bool isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(10, CastInt8ToInt32(result));

    result = CastInt8ToInt32(test2);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(-24, result);

    result = CastInt8ToInt32(test3);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(0, CastInt8ToInt32(result));

    result = CastInt8ToInt32(test4);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(std::numeric_limits<int8_t>::min(), result);

    result = CastInt8ToInt32(test5);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(std::numeric_limits<int8_t>::max(), result);
}

TEST(FunctionTest, CastInt16ToInt64)
{
    int16_t test1 = 10;
    int16_t test2 = -24;
    int16_t test3 = 0;
    int16_t test4 = std::numeric_limits<int16_t>::min();
    int16_t test5 = std::numeric_limits<int16_t>::max();
    int64_t baseline = 1;
    auto result = CastInt16ToInt64(test1);
    bool isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(10, CastInt16ToInt64(result));

    result = CastInt16ToInt64(test2);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(-24, result);

    result = CastInt16ToInt64(test3);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(0, CastInt16ToInt64(result));

    result = CastInt16ToInt64(test4);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(std::numeric_limits<int16_t>::min(), result);

    result = CastInt16ToInt64(test5);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(std::numeric_limits<int16_t>::max(), result);
}

TEST(FunctionTest, CastInt16ToInt32)
{
    int16_t test1 = 10;
    int16_t test2 = -24;
    int16_t test3 = 0;
    int16_t test4 = std::numeric_limits<int16_t>::min();
    int16_t test5 = std::numeric_limits<int16_t>::max();
    int32_t baseline = 1;
    auto result = CastInt16ToInt32(test1);
    bool isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(10, CastInt16ToInt32(result));

    result = CastInt16ToInt32(test2);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(-24, result);

    result = CastInt16ToInt32(test3);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(0, CastInt16ToInt32(result));

    result = CastInt16ToInt32(test4);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(std::numeric_limits<int16_t>::min(), result);

    result = CastInt16ToInt32(test5);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(std::numeric_limits<int16_t>::max(), result);
}


TEST(FunctionTest, CastInt16ToDouble)
{
    int16_t test1 = 10;
    int16_t test2 = -24;
    int16_t test3 = 0;
    int16_t test4 = std::numeric_limits<int16_t>::min();
    int16_t test5 = std::numeric_limits<int16_t>::max();
    double baseline = 11.13;
    auto result = CastInt16ToDouble(test1);
    bool isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(10.00, result);

    result = CastInt16ToDouble(test2);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(-24.00, result);

    result = CastInt16ToDouble(test3);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(0.00, result);

    result = CastInt16ToDouble(test4);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(static_cast<double>(std::numeric_limits<int16_t>::min()), result);

    result = CastInt16ToDouble(test5);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(static_cast<double>(std::numeric_limits<int16_t>::max()), result);
}

TEST(FunctionTest, CastInt8ToDouble)
{
    int8_t test1 = 10;
    int8_t test2 = -24;
    int8_t test3 = 0;
    int8_t test4 = std::numeric_limits<int8_t>::min();
    int8_t test5 = std::numeric_limits<int8_t>::max();
    double baseline = 11.13;
    auto result = CastInt8ToDouble(test1);
    bool isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(10.00, result);

    result = CastInt8ToDouble(test2);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(-24.00, result);

    result = CastInt8ToDouble(test3);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(0.00, result);

    result = CastInt8ToDouble(test4);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(static_cast<double>(std::numeric_limits<int8_t>::min()), result);

    result = CastInt8ToDouble(test5);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(static_cast<double>(std::numeric_limits<int8_t>::max()), result);
}

TEST(FunctionTest, CastInt32ToInt16)
{
    int32_t test1 = 10;
    int32_t test2 = -24;
    int32_t test3 = 0;
    int16_t baseline = 1;
    auto result = CastInt32ToInt16(test1);
    bool isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(10, CastInt32ToInt16(result));

    result = CastInt32ToInt16(test2);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(-24, result);

    result = CastInt32ToInt16(test3);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(0, CastInt32ToInt16(result));
}

TEST(FunctionTest, CastInt32ToInt8)
{
    int32_t test1 = 10;
    int32_t test2 = -24;
    int32_t test3 = 0;
    int8_t baseline = 1;
    auto result = CastInt32ToInt8(test1);
    bool isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(10, CastInt32ToInt8(result));

    result = CastInt32ToInt8(test2);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(-24, result);

    result = CastInt32ToInt8(test3);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(0, CastInt32ToInt8(result));
}

TEST(FunctionTest, CastDoubleToInt16)
{
    double test1 = 10.00;
    double test2 = -24.00;
    double test3 = 0.0;
    double test4 = 113.1313;
    double test5 = -2000.989;
    int16_t baseline = 1;
    auto result = CastDoubleToInt16HalfUp(test1);
    bool isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(10, result);

    result = CastDoubleToInt16HalfUp(test2);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(-24, result);

    result = CastDoubleToInt16HalfUp(test3);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(0, result);

    result = CastDoubleToInt16HalfUp(test4);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(113, result);

    result = CastDoubleToInt16HalfUp(test5);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(-2001, result);

    result = CastDoubleToInt16Down(test4);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(113, result);
}

TEST(FunctionTest, CastDoubleToInt8)
{
    double test1 = 10.00;
    double test2 = -24.00;
    double test3 = 0.0;
    double test4 = 113.1313;
    int8_t baseline = 1;
    auto result = CastDoubleToInt8HalfUp(test1);
    bool isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(10, result);

    result = CastDoubleToInt8HalfUp(test2);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(-24, result);

    result = CastDoubleToInt8HalfUp(test3);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(0, result);

    result = CastDoubleToInt8HalfUp(test4);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(113, result);

    result = CastDoubleToInt8Down(test4);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(113, result);
}

TEST(FunctionTest, CastStringToInt16)
{
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);

    std::string s = " 23423 ";
    int16_t result = CastStringToShort(contextPtr, s.c_str(), static_cast<int32_t>(s.size()), false);
    EXPECT_EQ(result, 23423);
    s = "100123";
    result = CastStringToShort(contextPtr, s.c_str(), static_cast<int32_t>(s.size()), false);
    EXPECT_TRUE(context->HasError());
    context->ResetError();
    s = "123.123";
    result = CastStringToShort(contextPtr, s.c_str(), static_cast<int32_t>(s.size()), false);
    EXPECT_EQ(result, 0);
    s = "2147483648";
    result = CastStringToShort(contextPtr, s.c_str(), static_cast<int32_t>(s.size()), false);
    EXPECT_TRUE(context->HasError());
    context->ResetError();
    s = "2a147483648";
    result = CastStringToShort(contextPtr, s.c_str(), static_cast<int32_t>(s.size()), false);
    EXPECT_TRUE(context->HasError());
    context->ResetError();
    s = " -10078 ";
    result = CastStringToShort(contextPtr, s.c_str(), static_cast<int32_t>(s.size()), false);
    EXPECT_EQ(result, -10078);
    EXPECT_FALSE(context->HasError());
    s = "2123123123147483648";
    result = CastStringToShort(contextPtr, s.c_str(), static_cast<int32_t>(s.size()), false);
    EXPECT_TRUE(context->HasError());
    context->ResetError();
    s = "-2123123123147483648";
    result = CastStringToShort(contextPtr, s.c_str(), static_cast<int32_t>(s.size()), false);
    EXPECT_TRUE(context->HasError());
    context->ResetError();
    s = "-2123123123147-483648";
    result = CastStringToShort(contextPtr, s.c_str(), static_cast<int32_t>(s.size()), false);
    EXPECT_TRUE(context->HasError());
    delete context;
}

TEST(FunctionTest, CastStringToInt8)
{
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);

    std::string s = " 127 ";
    int8_t result = CastStringToByte(contextPtr, s.c_str(), static_cast<int32_t>(s.size()), false);
    EXPECT_EQ(result, 127);
    s = "100123";
    result = CastStringToByte(contextPtr, s.c_str(), static_cast<int32_t>(s.size()), false);
    EXPECT_TRUE(context->HasError());
    context->ResetError();
    s = "123.123";
    result = CastStringToByte(contextPtr, s.c_str(), static_cast<int32_t>(s.size()), false);
    EXPECT_EQ(result, 0);
    s = "2147483648";
    result = CastStringToByte(contextPtr, s.c_str(), static_cast<int32_t>(s.size()), false);
    EXPECT_TRUE(context->HasError());
    context->ResetError();
    s = "2a147483648";
    result = CastStringToByte(contextPtr, s.c_str(), static_cast<int32_t>(s.size()), false);
    EXPECT_TRUE(context->HasError());
    context->ResetError();
    s = " -78 ";
    result = CastStringToByte(contextPtr, s.c_str(), static_cast<int32_t>(s.size()), false);
    EXPECT_EQ(result, -78);
    context->ResetError();
    s = "2123123123147483648";
    result = CastStringToByte(contextPtr, s.c_str(), static_cast<int32_t>(s.size()), false);
    EXPECT_TRUE(context->HasError());
    context->ResetError();
    s = "-2123123123147483648";
    result = CastStringToByte(contextPtr, s.c_str(), static_cast<int32_t>(s.size()), false);
    EXPECT_TRUE(context->HasError());
    context->ResetError();
    s = "-2123123123147-483648";
    result = CastStringToByte(contextPtr, s.c_str(), static_cast<int32_t>(s.size()), false);
    EXPECT_TRUE(context->HasError());
    delete context;
}

TEST(FunctionTest, CastInt32ToInt64)
{
    int32_t test1 = 10;
    int32_t test2 = -24;
    int32_t test3 = 0;
    int32_t test4 = std::numeric_limits<int32_t>::min();
    int32_t test5 = std::numeric_limits<int32_t>::max();
    int64_t baseline = 1;
    auto result = CastInt32ToInt64(test1);
    bool isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(10, CastInt32ToInt64(result));

    result = CastInt32ToInt64(test2);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(-24, result);

    result = CastInt32ToInt64(test3);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(0, CastInt32ToInt64(result));

    result = CastInt32ToInt64(test4);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(std::numeric_limits<int32_t>::min(), result);

    result = CastInt32ToInt64(test5);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(std::numeric_limits<int32_t>::max(), result);
}

TEST(FunctionTest, CastInt64ToInt8)
{
    int64_t test1 = 10;
    int64_t test2 = -24;
    int64_t test3 = 0;
    int64_t test4 = std::numeric_limits<int64_t>::min();
    int64_t test5 = std::numeric_limits<int64_t>::max();
    int8_t baseline = 1;
    auto result = CastInt64ToInt8(test1);
    bool isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(10, result);

    result = CastInt64ToInt8(test2);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(-24, result);

    result = CastInt64ToInt8(test3);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(0, CastInt64ToInt8(result));

    result = CastInt64ToInt8(test4);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(0, result);

    result = CastInt64ToInt8(test5);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(-1, result);
}

TEST(FunctionTest, CastInt64ToInt16)
{
    int64_t test1 = 10;
    int64_t test2 = -24;
    int64_t test3 = 0;
    int64_t test4 = std::numeric_limits<int64_t>::min();
    int64_t test5 = std::numeric_limits<int64_t>::max();
    int16_t baseline = 1;
    auto result = CastInt64ToInt16(test1);
    bool isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(10, result);

    result = CastInt64ToInt16(test2);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(-24, result);

    result = CastInt64ToInt16(test3);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(0, CastInt64ToInt16(result));

    result = CastInt64ToInt16(test4);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(0, result);

    result = CastInt64ToInt16(test5);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(-1, result);
}

TEST(FunctionTest, CastInt64ToInt32)
{
    int64_t test1 = 10;
    int64_t test2 = -24;
    int64_t test3 = 0;
    int64_t test4 = std::numeric_limits<int64_t>::min();
    int64_t test5 = std::numeric_limits<int64_t>::max();
    int32_t baseline = 1;
    auto result = CastInt64ToInt32(test1);
    bool isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(10, result);

    result = CastInt64ToInt32(test2);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(-24, result);

    result = CastInt64ToInt32(test3);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(0, CastInt64ToInt32(result));

    result = CastInt64ToInt32(test4);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(0, result);

    result = CastInt64ToInt32(test5);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(-1, result);
}

TEST(FunctionTest, CastInt32ToDouble)
{
    int32_t test1 = 10;
    int32_t test2 = -24;
    int32_t test3 = 0;
    int32_t test4 = std::numeric_limits<int32_t>::min();
    int32_t test5 = std::numeric_limits<int32_t>::max();
    double baseline = 11.13;
    auto result = CastInt32ToDouble(test1);
    bool isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(10.00, result);

    result = CastInt32ToDouble(test2);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(-24.00, result);

    result = CastInt32ToDouble(test3);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(0.00, result);

    result = CastInt32ToDouble(test4);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(static_cast<double>(std::numeric_limits<int32_t>::min()), result);

    result = CastInt32ToDouble(test5);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(static_cast<double>(std::numeric_limits<int32_t>::max()), result);
}

TEST(FunctionTest, CastInt64ToDouble)
{
    int64_t test1 = 10;
    int64_t test2 = -24;
    int64_t test3 = 0;
    int64_t test4 = std::numeric_limits<int64_t>::min();
    int64_t test5 = std::numeric_limits<int64_t>::max();
    double baseline = 11.13;
    auto result = CastInt64ToDouble(test1);
    bool isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(10.00, result);

    result = CastInt64ToDouble(test2);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(-24.00, result);

    result = CastInt64ToDouble(test3);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(0.00, result);

    result = CastInt64ToDouble(test4);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(static_cast<double>(std::numeric_limits<int64_t>::min()), result);

    result = CastInt64ToDouble(test5);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(static_cast<double>(std::numeric_limits<int64_t>::max()), result);
}

TEST(FunctionTest, CastDoubleToInt32)
{
    double test1 = 10.00;
    double test2 = -24.00;
    double test3 = 0.0;
    double test4 = 113.1313;
    double test5 = -2000.989;
    int32_t baseline = 1;
    auto result = CastDoubleToInt32HalfUp(test1);
    bool isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(10, result);

    result = CastDoubleToInt32HalfUp(test2);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(-24, result);

    result = CastDoubleToInt32HalfUp(test3);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(0, result);

    result = CastDoubleToInt32HalfUp(test4);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(113, result);

    result = CastDoubleToInt32HalfUp(test5);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(-2001, result);
}

TEST(FunctionTest, CastDoubleToInt64)
{
    double test1 = 10.00;
    double test2 = -24.00;
    double test3 = 0.0;
    double test4 = 113.1313;
    double test5 = -2000.989;
    int64_t baseline = 1;
    auto result = CastDoubleToInt64HalfUp(test1);
    bool isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(10, result);

    result = CastDoubleToInt64HalfUp(test2);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(-24, result);

    result = CastDoubleToInt64HalfUp(test3);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(0, result);

    result = CastDoubleToInt64HalfUp(test4);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(113, result);

    result = CastDoubleToInt64HalfUp(test5);
    isSameType = std::is_same<decltype(baseline), decltype(result)>::value;
    EXPECT_TRUE(isSameType);
    EXPECT_EQ(-2001, result);
}

bool CompareDoubleBits(double d1, double d2)
{
    uint64_t bits1;
    uint64_t bits2;
    memcpy(&bits1, &d1, sizeof(double));
    memcpy(&bits2, &d2, sizeof(double));
    return bits1 == bits2;
}

TEST(FunctionTest, NormalizeNaNAndZero)
{
    EXPECT_FALSE(CompareDoubleBits(-0.0, NormalizeNaNAndZero(-0.0)));
    EXPECT_TRUE(CompareDoubleBits(0.0, NormalizeNaNAndZero(-0.0)));
    EXPECT_TRUE(CompareDoubleBits(0.0, NormalizeNaNAndZero(0.0)));
    uint64_t nanBits = 0xFFF8000000000001L;
    double nanDouble = 0;
    memcpy(&nanDouble, &nanBits, sizeof(nanBits));
    EXPECT_FALSE(CompareDoubleBits(nanDouble, NormalizeNaNAndZero(nanDouble)));
    EXPECT_TRUE(CompareDoubleBits(0.0 / 0.0, NormalizeNaNAndZero(nanDouble)));
    EXPECT_TRUE(CompareDoubleBits(0.0 / 0.0, NormalizeNaNAndZero(0.0 / 0.0)));
    double value = 3.5;
    EXPECT_TRUE(CompareDoubleBits(value, NormalizeNaNAndZero(value)));
}

TEST(FunctionTest, PowerDouble)
{
    for (int i = -10; i <= 10; ++i) {
        double b = i;
        EXPECT_EQ(b * b, PowerDouble(b, 2.0));
    }
}

TEST(FunctionTest, Pmod)
{
    EXPECT_EQ(0, Pmod(4589732, 0));
    int n = 100;
    for (int i = 1; i <= n; ++i) {
        EXPECT_EQ(n % i, Pmod(n, i));
    }
    n = -100;
    for (int i = -1; i >= n; --i) {
        EXPECT_EQ(n - (n / i) * i, Pmod(n, i));
    }
}

TEST(FunctionTest, Add)
{
    auto res1 = AddInt8(3, 5);
    EXPECT_EQ(res1, 8);

    auto res2 = AddInt16(111, 1000);
    EXPECT_EQ(res2, 1111);

    auto res3 = AddInt32(10240, 2);
    EXPECT_EQ(res3, 10242);

    auto res4 = AddInt64(3000000, 5000000);
    EXPECT_EQ(res4, 8000000);
}

TEST(FunctionTest, Sub)
{
    auto res1 = SubtractInt8(3, 5);
    EXPECT_EQ(res1, -2);

    auto res2 = SubtractInt16(111, 1000);
    EXPECT_EQ(res2, -889);

    auto res3 = SubtractInt32(10240, 2);
    EXPECT_EQ(res3, 10238);

    auto res4 = SubtractInt64(3000000, 5000000);
    EXPECT_EQ(res4, -2000000);
}

TEST(FunctionTest, TryAdd)
{
    bool overflowFlag = false;
    EXPECT_EQ(AddInt8RetNull(&overflowFlag, 127, 1), -128);
    EXPECT_TRUE(overflowFlag);

    overflowFlag = false;
    EXPECT_EQ(AddInt16RetNull(&overflowFlag, 32767, 1), -32768);
    EXPECT_TRUE(overflowFlag);

    overflowFlag = false;
    EXPECT_EQ(AddInt32RetNull(&overflowFlag, 2147483647, 1), -2147483648);
    EXPECT_TRUE(overflowFlag);

    overflowFlag = false;
    EXPECT_EQ(AddInt32RetNull(&overflowFlag, -2147483648, -1), 2147483647);
    EXPECT_TRUE(overflowFlag);

    overflowFlag = false;
    EXPECT_EQ(AddInt64RetNull(&overflowFlag, 9223372036854775807L, 1L), -9223372036854775808L);
    EXPECT_TRUE(overflowFlag);

    overflowFlag = false;
    EXPECT_EQ(AddInt64RetNull(&overflowFlag, -9223372036854775808L, -1L), 9223372036854775807L);
    EXPECT_TRUE(overflowFlag);
}

TEST(FunctionTest, TrySubtract)
{
    bool overflowFlag = false;
    EXPECT_EQ(SubtractInt8RetNull(&overflowFlag, 127, -1), -128);
    EXPECT_TRUE(overflowFlag);

    overflowFlag = false;
    EXPECT_EQ(SubtractInt16RetNull(&overflowFlag, 32767, -1), -32768);
    EXPECT_TRUE(overflowFlag);

    overflowFlag = false;
    EXPECT_EQ(SubtractInt32RetNull(&overflowFlag, 2147483647, -1), -2147483648);
    EXPECT_TRUE(overflowFlag);

    overflowFlag = false;
    EXPECT_EQ(SubtractInt32RetNull(&overflowFlag, -2147483648, 1), 2147483647);
    EXPECT_TRUE(overflowFlag);

    overflowFlag = false;
    EXPECT_EQ(SubtractInt64RetNull(&overflowFlag, 9223372036854775807L, -1L), -9223372036854775808L);
    EXPECT_TRUE(overflowFlag);

    overflowFlag = false;
    EXPECT_EQ(SubtractInt64RetNull(&overflowFlag, -9223372036854775808L, 1L), 9223372036854775807L);
    EXPECT_TRUE(overflowFlag);
}

TEST(FunctionTest, TryMultiply)
{
    bool overflowFlag = false;

    EXPECT_EQ(MultiplyInt8RetNull(&overflowFlag, 64, 2), -128);
    EXPECT_TRUE(overflowFlag);

    overflowFlag = false;
    EXPECT_EQ(MultiplyInt16RetNull(&overflowFlag, 16384, 2), -32768);
    EXPECT_TRUE(overflowFlag);

    overflowFlag = false;
    EXPECT_EQ(MultiplyInt32RetNull(&overflowFlag, 1073741824, 2), -2147483648);
    EXPECT_TRUE(overflowFlag);

    overflowFlag = false;
    EXPECT_EQ(MultiplyInt32RetNull(&overflowFlag, -1073741825, 2), 2147483646);
    EXPECT_TRUE(overflowFlag);

    overflowFlag = false;
    EXPECT_EQ(MultiplyInt64RetNull(&overflowFlag, 4611686018427387904L, 2L), -9223372036854775808L);
    EXPECT_TRUE(overflowFlag);

    overflowFlag = false;
    EXPECT_EQ(MultiplyInt64RetNull(&overflowFlag, -4611686018427387905L, 2L), 9223372036854775806L);
    EXPECT_TRUE(overflowFlag);
}

TEST(FunctionTest, Multiply)
{
    auto res1 = MultiplyInt8(3, 5);
    EXPECT_EQ(res1, 15);

    auto res2 = MultiplyInt16(16, 16);
    EXPECT_EQ(res2, 256);
}

TEST(FunctionTest, Divide)
{
    bool nullFlag = false;

    DivideInt8(&nullFlag, 1, 0);
    EXPECT_TRUE(nullFlag);

    nullFlag = false;
    DivideInt16(&nullFlag, 1, 0);
    EXPECT_TRUE(nullFlag);

    nullFlag = false;
    DivideInt32(&nullFlag, 1, 0);
    EXPECT_TRUE(nullFlag);

    nullFlag = false;
    EXPECT_EQ(1, DivideInt32(&nullFlag, 3, 2));
    EXPECT_FALSE(nullFlag);

    nullFlag = false;
    DivideInt64(&nullFlag, 1, 0);
    EXPECT_TRUE(nullFlag);

    nullFlag = false;
    DivideDouble(&nullFlag, 1, 0);
    EXPECT_TRUE(nullFlag);

    nullFlag = false;
    auto res1 = DivideInt8(&nullFlag, 111, 3);
    EXPECT_FALSE(nullFlag);
    EXPECT_EQ(res1, 37);

    nullFlag = false;
    auto res2 = DivideInt16(&nullFlag, 1024, 16);
    EXPECT_FALSE(nullFlag);
    EXPECT_EQ(res2, 64);

    nullFlag = false;
    auto res3 = DivideInt32(&nullFlag, 65536, 4);
    EXPECT_FALSE(nullFlag);
    EXPECT_EQ(res3, 16384);

    nullFlag = false;
    auto res4 = DivideInt64(&nullFlag, 10000000, 1000);
    EXPECT_FALSE(nullFlag);
    EXPECT_EQ(res4, 10000);

    nullFlag = false;
    auto res5 = DivideDouble(&nullFlag, 6.4, 3.2);
    EXPECT_FALSE(nullFlag);
    EXPECT_EQ(res5, 2.0);
}

TEST(FunctionTest, Mod)
{
    bool nullFlag = false;

    ModulusInt8(&nullFlag, 1, 0);
    EXPECT_TRUE(nullFlag);

    nullFlag = false;
    EXPECT_EQ(1, ModulusInt8(&nullFlag, 16, 3));
    EXPECT_FALSE(nullFlag);

    nullFlag = false;
    ModulusInt16(&nullFlag, 1, 0);
    EXPECT_TRUE(nullFlag);

    nullFlag = false;
    EXPECT_EQ(2, ModulusInt16(&nullFlag, 29, 3));
    EXPECT_FALSE(nullFlag);

    nullFlag = false;
    ModulusInt32(&nullFlag, 1, 0);
    EXPECT_TRUE(nullFlag);

    nullFlag = false;
    EXPECT_EQ(2, ModulusInt32(&nullFlag, 5, 3));
    EXPECT_FALSE(nullFlag);

    nullFlag = false;
    EXPECT_EQ(2, ModulusInt16(&nullFlag, 29, 3));
    EXPECT_FALSE(nullFlag);

    nullFlag = false;
    ModulusInt32(&nullFlag, 1, 0);
    EXPECT_TRUE(nullFlag);

    nullFlag = false;
    EXPECT_EQ(2, ModulusInt32(&nullFlag, 5, 3));
    EXPECT_FALSE(nullFlag);

    nullFlag = false;
    ModulusInt64(&nullFlag, 1, 0);
    EXPECT_TRUE(nullFlag);
}

TEST(FunctionTest, LT)
{
    bool ans = LessThanInt8(3, 7);
    EXPECT_TRUE(ans);
    ans = LessThanInt8(11, 1);
    EXPECT_FALSE(ans);

    ans = LessThanInt16(13, 10210);
    EXPECT_TRUE(ans);
    ans = LessThanInt16(12311, 1123);
    EXPECT_FALSE(ans);

    ans = LessThanInt32(123456, 1234567);
    EXPECT_TRUE(ans);
    ans = LessThanInt32(10, 2);
    EXPECT_FALSE(ans);

    ans = LessThanInt64(10, 10000000);
    EXPECT_TRUE(ans);
    ans = LessThanInt64(10000000, 1);
    EXPECT_FALSE(ans);
}

TEST(FunctionTest, GT)
{
    bool ans = GreaterThanInt8(3, 7);
    EXPECT_FALSE(ans);
    ans = GreaterThanInt8(11, 1);
    EXPECT_TRUE(ans);

    ans = GreaterThanInt16(13, 10210);
    EXPECT_FALSE(ans);
    ans = GreaterThanInt16(12311, 1123);
    EXPECT_TRUE(ans);

    ans = GreaterThanInt32(123456, 1234567);
    EXPECT_FALSE(ans);
    ans = GreaterThanInt32(10, 2);
    EXPECT_TRUE(ans);

    ans = GreaterThanInt64(10, 10000000);
    EXPECT_FALSE(ans);
    ans = GreaterThanInt64(10000000, 1);
    EXPECT_TRUE(ans);
}

TEST(FunctionTest, LTE)
{
    bool ans = LessThanEqualInt8(3, 7);
    EXPECT_TRUE(ans);
    ans = LessThanEqualInt8(11, 1);
    EXPECT_FALSE(ans);
    ans = LessThanEqualInt8(100, 100);
    EXPECT_TRUE(ans);

    ans = LessThanEqualInt16(13, 10210);
    EXPECT_TRUE(ans);
    ans = LessThanEqualInt16(12311, 1123);
    EXPECT_FALSE(ans);
    ans = LessThanEqualInt16(8192, 8192);
    EXPECT_TRUE(ans);

    ans = LessThanEqualInt32(123456, 1234567);
    EXPECT_TRUE(ans);
    ans = LessThanEqualInt32(10, 2);
    EXPECT_FALSE(ans);
    ans = LessThanEqualInt32(65536, 65536);
    EXPECT_TRUE(ans);

    ans = LessThanEqualInt64(10, 10000000);
    EXPECT_TRUE(ans);
    ans = LessThanEqualInt64(10000000, 1);
    EXPECT_FALSE(ans);
    ans = LessThanEqualInt64(123456789, 123456789);
    EXPECT_TRUE(ans);
}

TEST(FunctionTest, GTE)
{
    bool ans = GreaterThanEqualInt8(3, 7);
    EXPECT_FALSE(ans);
    ans = GreaterThanEqualInt8(11, 1);
    EXPECT_TRUE(ans);
    ans = GreaterThanEqualInt8(100, 100);
    EXPECT_TRUE(ans);

    ans = GreaterThanEqualInt16(13, 10210);
    EXPECT_FALSE(ans);
    ans = GreaterThanEqualInt16(12311, 1123);
    EXPECT_TRUE(ans);
    ans = GreaterThanEqualInt16(8192, 8192);
    EXPECT_TRUE(ans);

    ans = GreaterThanEqualInt32(123456, 1234567);
    EXPECT_FALSE(ans);
    ans = GreaterThanEqualInt32(10, 2);
    EXPECT_TRUE(ans);
    ans = GreaterThanEqualInt32(65536, 65536);
    EXPECT_TRUE(ans);

    ans = GreaterThanEqualInt64(10, 10000000);
    EXPECT_FALSE(ans);
    ans = GreaterThanEqualInt64(10000000, 1);
    EXPECT_TRUE(ans);
    ans = GreaterThanEqualInt64(123456789, 123456789);
    EXPECT_TRUE(ans);
}

TEST(FunctionTest, EQ)
{
    bool ans;
    ans = EqualInt8(17, 17);
    EXPECT_TRUE(ans);
    ans = EqualInt8(11, 123);
    EXPECT_FALSE(ans);

    ans = EqualInt16(171, 171);
    EXPECT_TRUE(ans);
    ans = EqualInt16(11, 12345);
    EXPECT_FALSE(ans);

    ans = EqualInt32(123456, 123456);
    EXPECT_TRUE(ans);
    ans = EqualInt32(0, 123456);
    EXPECT_FALSE(ans);

    ans = EqualInt64(123456000, 123456000);
    EXPECT_TRUE(ans);
    ans = EqualInt64(11, 12348765);
    EXPECT_FALSE(ans);
}

TEST(FunctionTest, NEQ)
{
    bool ans;
    ans = NotEqualInt8(17, 17);
    EXPECT_FALSE(ans);
    ans = NotEqualInt8(11, 123);
    EXPECT_TRUE(ans);

    ans = NotEqualInt16(171, 171);
    EXPECT_FALSE(ans);
    ans = NotEqualInt16(11, 12345);
    EXPECT_TRUE(ans);

    ans = NotEqualInt32(123456, 123456);
    EXPECT_FALSE(ans);
    ans = NotEqualInt32(0, 123456);
    EXPECT_TRUE(ans);

    ans = NotEqualInt64(123456000, 123456000);
    EXPECT_FALSE(ans);
    ans = NotEqualInt64(11, 12348765);
    EXPECT_TRUE(ans);
}

TEST(FunctionTest, LessThanEqualDouble)
{
    double left = 3.5;
    double right = 3.0;
    EXPECT_FALSE(LessThanEqualDouble(left, right));
}

TEST(FunctionTest, CastNumToString)
{
    auto context = new ExecutionContext();
    int64_t contextptr = reinterpret_cast<int64_t>(context);

    int8_t int_8 = -12;
    int16_t int_16 = 123;
    int32_t int_32 = 456;
    int64_t int_64 = 789;
    double num_double = 3.56;

    int outLen = 0;
    std::string actual;
    const char *result;

    result = CastInt8ToString(contextptr, int_8, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ("-12", actual);
    EXPECT_EQ(3, outLen);

    result = CastInt16ToString(contextptr, int_16, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ("123", actual);
    EXPECT_EQ(3, outLen);

    result = CastIntToString(contextptr, int_32, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ("456", actual);
    EXPECT_EQ(3, outLen);

    result = CastLongToString(contextptr, int_64, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ("789", actual);
    EXPECT_EQ(3, outLen);

    result = CastDoubleToString(contextptr, num_double, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ("3.56", actual);
    EXPECT_EQ(4, outLen);
    delete context;
}

TEST(FunctionTest, Round)
{
    EXPECT_EQ(10, Round<int32_t>(10, 0));
    EXPECT_EQ(10, Round<int32_t>(10, 5));
    EXPECT_EQ(-10, Round<int32_t>(-10, 0));
    EXPECT_EQ(-10, Round<int32_t>(-10, 5));

    EXPECT_EQ(10, RoundLong(10, 0));
    EXPECT_EQ(10, RoundLong(10, 5));
    EXPECT_EQ(-10, RoundLong(-10, 0));
    EXPECT_EQ(-10, RoundLong(-10, 5));

    EXPECT_EQ(10.00, Round<double>(10.12345, 0));
    EXPECT_EQ(10.12, Round<double>(10.12345, 2));
    EXPECT_EQ(10.13, Round<double>(10.12945, 2));
    EXPECT_EQ(-10.00, Round<double>(-10.12345, 0));
    EXPECT_EQ(-10.12, Round<double>(-10.12345, 2));
    EXPECT_EQ(-10.13, Round<double>(-10.12945, 2));
}

TEST(FunctionTest, CombineHash)
{
    EXPECT_EQ(342, CombineHash(10, false, 32, false));
}

/*
 * std::string functions:
 */
TEST(FunctionTest, CountChar)
{
    int64_t result;

    result = CountChar("hello", 5, "l", 1, 1, false);
    EXPECT_EQ(result, 2);

    result = CountChar("aAaA", 4, "a", 1, 1, false);
    EXPECT_EQ(result, 2);

    result = CountChar("abcd", 4, "e", 1, 1, false);
    EXPECT_EQ(result, 0);

    result = CountChar("hello", 5, "", 0, 0, false);
    EXPECT_EQ(result, 0);

    result = CountChar("", 0, "a", 1, 1, false);
    EXPECT_EQ(result, 0);
}

TEST(FunctionTest, SplitIndexRetNull)
{
    int outlen = 0;
    bool outIsNull = false;
    const char *result;
    std::string actual;

    result = SplitIndexRetNull("Jack,John,Mary", 14, false, ",", 1, 1, false, 2, false, &outIsNull, &outlen);
    EXPECT_EQ(outlen, 4);
    EXPECT_EQ(outIsNull, false);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "Mary");

    result = SplitIndexRetNull("Jack,Johnny,Mary", 16, false, ",", 1, 1, false, 1, false, &outIsNull, &outlen);
    EXPECT_EQ(outlen, 6);
    EXPECT_EQ(outIsNull, false);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "Johnny");

    result = SplitIndexRetNull("Jack,John,Mary", 14, false, ",", 1, 1, false, -1, false, &outIsNull, &outlen);
    EXPECT_EQ(outlen, 0);
    EXPECT_EQ(outIsNull, true);
    EXPECT_EQ(result, nullptr);

    result = SplitIndexRetNull("Jack,John,Mary", 14, false, ",", 1, 1, false, 3, false, &outIsNull, &outlen);
    EXPECT_EQ(outlen, 0);
    EXPECT_EQ(outIsNull, true);
    EXPECT_EQ(result, nullptr);

    result = SplitIndexRetNull(nullptr, 0, false, ",", 1, 1, false, 1, true, &outIsNull, &outlen);
    EXPECT_EQ(outlen, 0);
    EXPECT_EQ(outIsNull, true);
    EXPECT_EQ(result, nullptr);

    result = SplitIndexRetNull("Jack,John,Mary", 14, false, nullptr, 0, 0, true, 1, true, &outIsNull, &outlen);
    EXPECT_EQ(outlen, 0);
    EXPECT_EQ(outIsNull, true);
    EXPECT_EQ(result, nullptr);

    // When strings are concatinated, make sure data from next row isnt included when picking last element
    result = SplitIndexRetNull("Jack,John,MaryPaul,Nathan", 14, false, ",", 1, 1, false, 2, false, &outIsNull, &outlen);
    EXPECT_EQ(outlen, 4);
    actual = std::string(result, outlen);
    EXPECT_EQ(outIsNull, false);
    EXPECT_EQ(actual, "Mary");

    result = SplitIndexRetNull("''", 2, false, ",", 1, 1, false, 0, false, &outIsNull, &outlen);
    EXPECT_EQ(outlen, 2);
    actual = std::string(result, outlen);
    EXPECT_EQ(outIsNull, false);
    EXPECT_EQ(actual, "''");
}

TEST(FunctionTest, ConcatCharChar)
{
    auto context = new ExecutionContext();
    int64_t contextptr = reinterpret_cast<int64_t>(context);
    int outlen = 0;
    const char *result;
    std::string actual;

    result = ConcatCharChar(contextptr, "hello", 5, 5, "world", 5, 5, false, &outlen);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "helloworld");
    EXPECT_EQ(outlen, 10);

    result = ConcatCharChar(contextptr, "hello", 5, 5, "world", 10, 5, false, &outlen);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "helloworld");
    EXPECT_EQ(outlen, 10);

    result = ConcatCharChar(contextptr, "hello", 10, 5, "world", 5, 5, false, &outlen);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "hello     world");
    EXPECT_EQ(outlen, 15);

    result = ConcatCharChar(contextptr, "hello", 10, 5, "world", 5, 5, false, &outlen);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "hello     world");
    EXPECT_EQ(outlen, 15);

    result = ConcatCharChar(contextptr, "", 0, 0, "", 0, 0, false, &outlen);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "");
    EXPECT_EQ(outlen, 0);

    delete context;
}

TEST(FunctionTest, ConcatStrChar)
{
    auto context = new ExecutionContext();
    int64_t contextptr = reinterpret_cast<int64_t>(context);
    int outlen = 0;
    const char *result;
    std::string actual;

    result = ConcatStrChar(contextptr, "hello", 5, "world", 5, 5, false, &outlen);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "helloworld");
    EXPECT_EQ(outlen, 10);

    result = ConcatStrChar(contextptr, "hello", 5, "world", 10, 5, false, &outlen);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "helloworld");
    EXPECT_EQ(outlen, 10);

    result = ConcatStrChar(contextptr, "hello", 5, "world     ", 10, 10, false, &outlen);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "helloworld     ");
    EXPECT_EQ(outlen, 15);

    result = ConcatStrChar(contextptr, "", 0, "", 0, 0, false, &outlen);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "");
    EXPECT_EQ(outlen, 0);

    result = ConcatStrChar(contextptr, "hello", 5, "     ", 5, 5, false, &outlen);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "hello     ");
    EXPECT_EQ(outlen, 10);
    delete context;
}

TEST(FunctionTest, ConcatCharStr)
{
    auto context = new ExecutionContext();
    int64_t contextptr = reinterpret_cast<int64_t>(context);
    int outlen = 0;
    const char *result;
    std::string actual;

    result = ConcatCharStr(contextptr, "hello", 5, 5, "world", 5, false, &outlen);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "helloworld");
    EXPECT_EQ(outlen, 10);

    result = ConcatCharStr(contextptr, "hello", 10, 5, "world", 5, false, &outlen);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "hello     world");
    EXPECT_EQ(outlen, 15);

    result = ConcatCharStr(contextptr, "hello     ", 10, 10, "world", 5, false, &outlen);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "hello     world");
    EXPECT_EQ(outlen, 15);

    result = ConcatCharStr(contextptr, "", 0, 0, "", 0, false, &outlen);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "");
    EXPECT_EQ(outlen, 0);

    result = ConcatCharStr(contextptr, "", 5, 0, "world", 5, false, &outlen);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "     world");
    EXPECT_EQ(outlen, 10);
    delete context;
}

TEST(FunctionTest, ConcatWsWithoutStr)
{
    auto context = new ExecutionContext();
    int64_t contextPtr = reinterpret_cast<int64_t>(context);
    int32_t outlen = 0;
    const char *result;
    std::string actual;
    bool retIsNull = false;

    // concat_ws("-") => "" (Spark SQL: separator only returns empty string)
    result = ConcatWsWithoutStr(contextPtr, "-", 1, false, &retIsNull, &outlen);
    EXPECT_FALSE(retIsNull);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(outlen, 0);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "");

    // concat_ws(NULL) => NULL
    result = ConcatWsWithoutStr(contextPtr, "-", 1, true, &retIsNull, &outlen);
    EXPECT_TRUE(retIsNull);
    EXPECT_EQ(result, nullptr);
    EXPECT_EQ(outlen, 0);

    delete context;
}

TEST(FunctionTest, ConcatWsWith1Str)
{
    auto context = new ExecutionContext();
    int64_t contextPtr = reinterpret_cast<int64_t>(context);
    int32_t outlen = 0;
    const char *result;
    std::string actual;
    bool retIsNull = false;

    // concat_ws("-", "aa") => "aa" (Spark SQL: single string returns as-is)
    result = ConcatWsWith1Str(contextPtr, "-", 1, false, "aa", 2, false, &retIsNull, &outlen);
    EXPECT_FALSE(retIsNull);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(outlen, 2);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "aa");

    // concat_ws("-", null) => ""
    result = ConcatWsWith1Str(contextPtr, "-", 1, false, nullptr, 0, true, &retIsNull, &outlen);
    EXPECT_FALSE(retIsNull);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(outlen, 0);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "");

    // concat_ws(NULL, "aa") => NULL
    result = ConcatWsWith1Str(contextPtr, "-", 1, true, "aa", 2, false, &retIsNull, &outlen);
    EXPECT_TRUE(retIsNull);
    EXPECT_EQ(result, nullptr);
    EXPECT_EQ(outlen, 0);

    delete context;
}

TEST(FunctionTest, ConcatWsStr)
{
    auto context = new ExecutionContext();
    int64_t contextPtr = reinterpret_cast<int64_t>(context);
    int32_t outlen = 0;
    const char *result;
    std::string actual;
    bool retIsNull = false;

    result = ConcatWsStr(contextPtr, ",", 1, false, "hello", 5, false, "world", 5, false, &retIsNull, &outlen);
    EXPECT_FALSE(retIsNull);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(outlen, 11);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "hello,world");

    result = ConcatWsStr(contextPtr, "", 0, false, "hello", 5, false, "world", 5, false, &retIsNull, &outlen);
    EXPECT_FALSE(retIsNull);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(outlen, 10);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "helloworld");

    result = ConcatWsStr(contextPtr, "-", 1, false, "", 0, false, "world", 5, false, &retIsNull, &outlen);
    EXPECT_FALSE(retIsNull);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(outlen, 6);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "-world");

    result = ConcatWsStr(contextPtr, "-", 1, false, "hello", 5, false, "", 0, false, &retIsNull, &outlen);
    EXPECT_FALSE(retIsNull);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(outlen, 6);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "hello-");

    result = ConcatWsStr(contextPtr, "-", 1, false, "", 0, false, "", 0, false, &retIsNull, &outlen);
    EXPECT_FALSE(retIsNull);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(outlen, 1);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "-");

    result = ConcatWsStr(contextPtr, "-", 1, false, "", 0, false, "world", 5, false, &retIsNull, &outlen);
    EXPECT_FALSE(retIsNull);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(outlen, 6);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "-world");

    // separator is null => result null
    result = ConcatWsStr(contextPtr, "-", 1, true, "a", 1, false, "b", 1, false, &retIsNull, &outlen);
    EXPECT_TRUE(retIsNull);
    EXPECT_EQ(result, nullptr);
    EXPECT_EQ(outlen, 0);

    // input null skipped
    result = ConcatWsStr(contextPtr, "-", 1, false, nullptr, 0, true, "b", 1, false, &retIsNull, &outlen);
    EXPECT_FALSE(retIsNull);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(outlen, 1);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "b");

    // both inputs null => empty string (not null)
    result = ConcatWsStr(contextPtr, "-", 1, false, nullptr, 0, true, nullptr, 0, true, &retIsNull, &outlen);
    EXPECT_FALSE(retIsNull);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(outlen, 0);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "");

    delete context;
}

TEST(FunctionTest, ConcatWs3Str)
{
    auto context = new ExecutionContext();
    int64_t contextPtr = reinterpret_cast<int64_t>(context);
    int32_t outlen = 0;
    const char *result;
    std::string actual;
    bool retIsNull = false;

    result = ConcatWs3Str(contextPtr, ",", 1, false, "a", 1, false, "b", 1, false, "c", 1, false, &retIsNull, &outlen);
    EXPECT_FALSE(retIsNull);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(outlen, 5);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "a,b,c");

    result = ConcatWs3Str(contextPtr, "=", 1, false, "a", 1, false, "", 0, false, "c", 1, false, &retIsNull, &outlen);
    EXPECT_FALSE(retIsNull);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(outlen, 4);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "a==c");

    result = ConcatWs3Str(contextPtr, "=", 1, false, "", 0, false, "b", 1, false, "c", 1, false, &retIsNull, &outlen);
    EXPECT_FALSE(retIsNull);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(outlen, 4);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "=b=c");

    result = ConcatWs3Str(contextPtr, "=", 1, false, "a", 1, false, "b", 1, false, "", 0, false, &retIsNull, &outlen);
    EXPECT_FALSE(retIsNull);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(outlen, 4);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "a=b=");

    // Spark: concat_ws("-", "", null, "") => "-"
    result = ConcatWs3Str(contextPtr, "-", 1, false, "", 0, false, nullptr, 0, true, "", 0, false, &retIsNull, &outlen);
    EXPECT_FALSE(retIsNull);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(outlen, 1);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "-");

    // Spark: concat_ws("-", "", null, null) => "" (empty string)
    result = ConcatWs3Str(contextPtr, "-", 1, false, "", 0, false, nullptr, 0, true, nullptr, 0, true, &retIsNull, &outlen);
    EXPECT_FALSE(retIsNull);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(outlen, 0);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "");

    // Spark: concat_ws("-", null, null, null) => "" (empty string)
    result = ConcatWs3Str(contextPtr, "-", 1, false, nullptr, 0, true, nullptr, 0, true, nullptr, 0, true, &retIsNull, &outlen);
    EXPECT_FALSE(retIsNull);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(outlen, 0);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "");

    // separator null => null
    result = ConcatWs3Str(contextPtr, "-", 1, true, "aa", 2, false, "dd", 2, false, "bb", 2, false, &retIsNull, &outlen);
    EXPECT_TRUE(retIsNull);
    EXPECT_EQ(result, nullptr);
    EXPECT_EQ(outlen, 0);

    // Spark: concat_ws("", "aa", "", "bb") => "aabb"
    result = ConcatWs3Str(contextPtr, "", 0, false, "aa", 2, false, "", 0, false, "bb", 2, false, &retIsNull, &outlen);
    EXPECT_FALSE(retIsNull);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(outlen, 4);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "aabb");

    result = ConcatWs3Str(contextPtr, "", 0, false, "a", 1, false, "b", 1, false, "c", 1, false, &retIsNull, &outlen);
    EXPECT_FALSE(retIsNull);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(outlen, 3);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "abc");

    delete context;
}

TEST(FunctionTest, ConcatWs4Str)
{
    auto context = new ExecutionContext();
    int64_t contextPtr = reinterpret_cast<int64_t>(context);
    int32_t outlen = 0;
    const char *result;
    std::string actual;
    bool retIsNull = false;

    result = ConcatWs4Str(contextPtr, ",", 1, false, "a", 1, false, "b", 1, false, "c", 1, false, "d", 1, false,
        &retIsNull, &outlen);
    EXPECT_FALSE(retIsNull);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(outlen, 7);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "a,b,c,d");

    result = ConcatWs4Str(contextPtr, ",", 1, false, "a", 1, false, "b", 1, false, "", 0, false, "d", 1, false,
        &retIsNull, &outlen);
    EXPECT_FALSE(retIsNull);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(outlen, 6);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "a,b,,d");

    result = ConcatWs4Str(contextPtr, ",", 1, false, "", 0, false, "b", 1, false, "c", 1, false, "", 0, false,
        &retIsNull, &outlen);
    EXPECT_FALSE(retIsNull);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(outlen, 5);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, ",b,c,");

    result = ConcatWs4Str(contextPtr, ",", 1, false, "", 0, false, "", 0, false, "", 0, false, "", 0, false,
        &retIsNull, &outlen);
    EXPECT_FALSE(retIsNull);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(outlen, 3);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, ",,,");

    result = ConcatWs4Str(contextPtr, ",", 1, true, "a", 1, false, "b", 1, false, "c", 1, false, "d", 1, false,
        &retIsNull, &outlen);
    EXPECT_TRUE(retIsNull);
    EXPECT_EQ(result, nullptr);
    EXPECT_EQ(outlen, 0);

    result = ConcatWs4Str(contextPtr, "", 0, false, "a", 1, false, "b", 1, false, "c", 1, false, "d", 1, false,
        &retIsNull, &outlen);
    EXPECT_FALSE(retIsNull);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(outlen, 4);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "abcd");

    // skip nulls: concat_ws("-", "aa", null, "bb", null) => "aa-bb"
    result = ConcatWs4Str(contextPtr, "-", 1, false, "aa", 2, false, nullptr, 0, true, "bb", 2, false, nullptr, 0, true,
        &retIsNull, &outlen);
    EXPECT_FALSE(retIsNull);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(outlen, 5);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "aa-bb");

    delete context;
}

TEST(FunctionTest, ConcatWs5Str)
{
    auto context = new ExecutionContext();
    int64_t contextPtr = reinterpret_cast<int64_t>(context);
    int32_t outlen = 0;
    const char *result;
    std::string actual;
    bool retIsNull = false;

    result = ConcatWs5Str(contextPtr, ",", 1, false, "a", 1, false, "b", 1, false, "c", 1, false, "d", 1, false,
        "e", 1, false, &retIsNull, &outlen);
    EXPECT_FALSE(retIsNull);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(outlen, 9);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "a,b,c,d,e");

    result = ConcatWs5Str(contextPtr, ",", 1, false, "a", 1, false, "b", 1, false, "", 0, false, "d", 1, false,
        "e", 1, false, &retIsNull, &outlen);
    EXPECT_FALSE(retIsNull);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(outlen, 8);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "a,b,,d,e");

    result = ConcatWs5Str(contextPtr, ",", 1, false, "", 0, false, "b", 1, false, "c", 1, false, "d", 1, false,
        "", 0, false, &retIsNull, &outlen);
    EXPECT_FALSE(retIsNull);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(outlen, 7);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, ",b,c,d,");

    result = ConcatWs5Str(contextPtr, ",", 1, false, "", 0, false, "", 0, false, "", 0, false, "", 0, false, "", 0,
        false, &retIsNull, &outlen);
    EXPECT_FALSE(retIsNull);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(outlen, 4);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, ",,,,");

    result = ConcatWs5Str(contextPtr, ",", 1, true, "a", 1, false, "b", 1, false, "c", 1, false, "d", 1, false,
        "e", 1, false, &retIsNull, &outlen);
    EXPECT_TRUE(retIsNull);
    EXPECT_EQ(result, nullptr);
    EXPECT_EQ(outlen, 0);

    result = ConcatWs5Str(contextPtr, "", 0, false, "a", 1, false, "b", 1, false, "c", 1, false, "d", 1, false,
        "e", 1, false, &retIsNull, &outlen);
    EXPECT_FALSE(retIsNull);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(outlen, 5);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "abcde");

    // all null => empty string
    result = ConcatWs5Str(contextPtr, "-", 1, false, nullptr, 0, true, nullptr, 0, true, nullptr, 0, true, nullptr, 0,
        true, nullptr, 0, true, &retIsNull, &outlen);
    EXPECT_FALSE(retIsNull);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(outlen, 0);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "");

    delete context;
}


TEST(FunctionTest, Trim)
{
    auto context = new ExecutionContext();
    int64_t contextPtr = reinterpret_cast<int64_t>(context);
    int outLen = 0;
    const char *result;
    std::string actual;
    bool isNull = false;

    result = TrimStr(contextPtr, " ", 1, "  hello  ", 9, isNull, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, "hello");
    EXPECT_EQ(outLen, 5);

    result = TrimStr(contextPtr, "xy", 2, "xxxyyhelloxyyxx", 15, isNull, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, "hello");
    EXPECT_EQ(outLen, 5);

    result = TrimStr(contextPtr, "z", 1, "hello", 5, isNull, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, "hello");
    EXPECT_EQ(outLen, 5);

    result = TrimStr(contextPtr, "ab", 2, "ababab", 6, isNull, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, "");
    EXPECT_EQ(outLen, 0);

    result = TrimStr(contextPtr, " ", 1, "", 0, isNull, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, "");
    EXPECT_EQ(outLen, 0);

    result = TrimStr(contextPtr, "", 0, "  hello  ", 9, isNull, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, "  hello  ");
    EXPECT_EQ(outLen, 9);
    delete context;
}

TEST(FunctionTest, RTrim)
{
    auto context = new ExecutionContext();
    int64_t contextPtr = reinterpret_cast<int64_t>(context);
    int outLen = 0;
    const char *result;
    std::string actual;
    bool isNull = false;
    result = RightTrimStr(contextPtr, " ", 1, "hello    ", 9, isNull, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, "hello");
    EXPECT_EQ(outLen, 5);

    result = RightTrimStr(contextPtr, "xy", 2, "helloxyxy", 9, isNull, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, "hello");
    EXPECT_EQ(outLen, 5);

    result = RightTrimStr(contextPtr, "x", 1, "hello", 5, isNull, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, "hello");
    EXPECT_EQ(outLen, 5);

    result = RightTrimStr(contextPtr, "abc", 3, "abcabc", 6, isNull, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, "");
    EXPECT_EQ(outLen, 0);

    result = RightTrimStr(contextPtr, " ", 1, "", 0, isNull, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, "");
    EXPECT_EQ(outLen, 0);
    delete context;
}

TEST(FunctionTest, LTrim)
{
    auto context = new ExecutionContext();
    int64_t contextPtr = reinterpret_cast<int64_t>(context);
    int outLen = 0;
    const char *result;
    std::string actual;
    bool isNull = false;

    result = LeftTrimStr(contextPtr, " ", 1, "   hello", 8, isNull, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, "hello");
    EXPECT_EQ(outLen, 5);

    result = LeftTrimStr(contextPtr, "xy", 2, "xyxyhello", 9, isNull, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, "hello");
    EXPECT_EQ(outLen, 5);

    result = LeftTrimStr(contextPtr, "x", 1, "hello", 5, isNull, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, "hello");
    EXPECT_EQ(outLen, 5);

    result = LeftTrimStr(contextPtr, "abc", 3, "abcabc", 6, isNull, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, "");
    EXPECT_EQ(outLen, 0);

    result = LeftTrimStr(contextPtr, " ", 1, "", 0, isNull, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, "");
    EXPECT_EQ(outLen, 0);
    delete context;
}

TEST(FunctionTest, Substr)
{
    auto context = new ExecutionContext();
    int64_t contextptr = reinterpret_cast<int64_t>(context);
    std::string str = "Magic Johnson 123@#$";
    int32_t strlen = static_cast<int32_t>(str.length());
    int32_t outlen = 0;
    const char *result;
    std::string actual;

    result = SubstrVarchar<int32_t, false, false>(contextptr, str.c_str(), strlen, 1, strlen, false, &outlen);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, str);
    EXPECT_EQ(outlen, strlen);

    result = SubstrVarchar<int32_t, false, false>(contextptr, str.c_str(), strlen, 1, 5, false, &outlen);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "Magic");
    EXPECT_EQ(outlen, 5);

    result = SubstrVarchar<int32_t, false, false>(contextptr, str.c_str(), strlen, 10, 10, false, &outlen);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "nson 123@#");
    EXPECT_EQ(outlen, 10);

    result = SubstrVarchar<int32_t, false, false>(contextptr, str.c_str(), strlen, -5, 7, false, &outlen);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "23@#$");
    EXPECT_EQ(outlen, 5);

    result = SubstrVarchar<int32_t, false, false>(contextptr, str.c_str(), strlen, 0, 0, false, &outlen);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "");
    EXPECT_EQ(outlen, 0);

    result = SubstrVarchar<int32_t, false, false>(contextptr, str.c_str(), strlen, strlen, strlen + 5, false, &outlen);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "$");
    EXPECT_EQ(outlen, 1);
    delete context;
}

TEST(FunctionTest, SubstrZh)
{
    auto context = new ExecutionContext();
    int64_t contextPtr = reinterpret_cast<int64_t>(context);
    std::string str = "时欧基乌斯侧后解 hello! 回复哦黑色的and magic粉色的圣诞袜";
    int32_t strLen = static_cast<int32_t>(str.length());
    int32_t outLen = 0;
    const char *result;
    std::string actual;

    result = SubstrVarchar<int32_t, false, false>(contextPtr, str.c_str(), strLen, 1, 37, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, str);
    EXPECT_EQ(outLen, strLen);

    result = SubstrVarchar<int32_t, false, true>(contextPtr, str.c_str(), strLen, 0, 37, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, str);
    EXPECT_EQ(outLen, strLen);

    result = SubstrVarchar<int32_t, false, false>(contextPtr, str.c_str(), strLen, 1, 5, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, "时欧基乌斯");
    EXPECT_EQ(outLen, 15);

    result = SubstrVarchar<int32_t, false, true>(contextPtr, str.c_str(), strLen, 0, 5, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, "时欧基乌斯");
    EXPECT_EQ(outLen, 15);

    result = SubstrVarchar<int32_t, false, false>(contextPtr, str.c_str(), strLen, 10, 10, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, "hello! 回复哦");
    EXPECT_EQ(outLen, 16);

    result = SubstrVarchar<int32_t, false, false>(contextPtr, str.c_str(), strLen, -5, 7, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, "色的圣诞袜");
    EXPECT_EQ(outLen, 15);

    result = SubstrVarchar<int32_t, false, false>(contextPtr, str.c_str(), strLen, 0, 0, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, "");
    EXPECT_EQ(outLen, 0);

    result = SubstrVarchar<int32_t, false, true>(contextPtr, str.c_str(), strLen, 0, 0, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, "");
    EXPECT_EQ(outLen, 0);

    result = SubstrVarchar<int32_t, false, false>(contextPtr, str.c_str(), strLen, 37, strLen + 5, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, "袜");
    EXPECT_EQ(outLen, 3);

    result = SubstrVarchar<int32_t, false, false>(contextPtr, str.c_str(), strLen, -38, 10, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, "");
    EXPECT_EQ(outLen, 0);

    result = SubstrVarchar<int32_t, false, false>(contextPtr, str.c_str(), strLen, -37, 37, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, str);
    EXPECT_EQ(outLen, strLen);

    delete context;
}

TEST(FunctionTest, SubstrChar)
{
    auto context = new ExecutionContext();
    int64_t contextptr = reinterpret_cast<int64_t>(context);
    std::string str = "Magic Johnson 123@#$       ";
    int32_t width = static_cast<int32_t>(str.length());
    int32_t strlen = width - 7;
    int32_t outlen = 0;
    const char *result;
    std::string actual;

    result = SubstrChar<int32_t, false, false>(contextptr, str.c_str(), width, strlen, 1, strlen, false, &outlen);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "Magic Johnson 123@#$");
    EXPECT_EQ(outlen, strlen);

    result = SubstrChar<int32_t, false, true>(contextptr, str.c_str(), width, strlen, 0, strlen, false, &outlen);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "Magic Johnson 123@#$");
    EXPECT_EQ(outlen, strlen);

    result = SubstrChar<int32_t, false, false>(contextptr, str.c_str(), width, strlen, 1, 5, false, &outlen);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "Magic");
    EXPECT_EQ(outlen, 5);

    result = SubstrChar<int32_t, false, true>(contextptr, str.c_str(), width, strlen, 0, 5, false, &outlen);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "Magic");
    EXPECT_EQ(outlen, 5);

    result = SubstrChar<int32_t, false, false>(contextptr, str.c_str(), width, strlen, 10, 10, false, &outlen);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "nson 123@#");
    EXPECT_EQ(outlen, 10);

    result = SubstrChar<int32_t, false, false>(contextptr, str.c_str(), width, strlen, -5, 7, false, &outlen);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "23@#$");
    EXPECT_EQ(outlen, 5);

    result = SubstrChar<int32_t, false, false>(contextptr, str.c_str(), width, strlen, 0, 0, false, &outlen);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "");
    EXPECT_EQ(outlen, 0);

    result = SubstrChar<int32_t, false, true>(contextptr, str.c_str(), width, strlen, 0, 0, false, &outlen);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "");
    EXPECT_EQ(outlen, 0);

    result =
        SubstrChar<int32_t, false, false>(contextptr, str.c_str(), width, strlen, strlen, strlen + 5, false, &outlen);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "$");
    EXPECT_EQ(outlen, 1);

    delete context;
}

TEST(FunctionTest, SubstrCharZh)
{
    auto context = new ExecutionContext();
    int64_t contextPtr = reinterpret_cast<int64_t>(context);
    std::string str = "时欧基乌斯侧后解 hello! 回复哦黑色的and magic粉色的圣诞袜";
    int32_t width = 37;
    int32_t strLen = str.length();
    int32_t outLen = 0;
    const char *result;
    std::string actual;

    result = SubstrChar<int32_t, false, false>(contextPtr, str.c_str(), width, strLen, 1, 37, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, str);
    EXPECT_EQ(outLen, strLen);

    result = SubstrChar<int32_t, false, true>(contextPtr, str.c_str(), width, strLen, 0, 37, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, str);
    EXPECT_EQ(outLen, strLen);

    result = SubstrChar<int32_t, false, false>(contextPtr, str.c_str(), width, strLen, 1, 5, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, "时欧基乌斯");
    EXPECT_EQ(outLen, 15);

    result = SubstrChar<int32_t, false, true>(contextPtr, str.c_str(), width, strLen, 0, 5, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, "时欧基乌斯");
    EXPECT_EQ(outLen, 15);

    result = SubstrChar<int32_t, false, false>(contextPtr, str.c_str(), width, strLen, 10, 10, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, "hello! 回复哦");
    EXPECT_EQ(outLen, 16);

    result = SubstrChar<int32_t, false, false>(contextPtr, str.c_str(), width, strLen, -5, 7, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, "色的圣诞袜");
    EXPECT_EQ(outLen, 15);

    result = SubstrChar<int32_t, false, false>(contextPtr, str.c_str(), width, strLen, 0, 0, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, "");
    EXPECT_EQ(outLen, 0);

    result = SubstrChar<int32_t, false, true>(contextPtr, str.c_str(), width, strLen, 0, 0, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, "");
    EXPECT_EQ(outLen, 0);

    result = SubstrChar<int32_t, false, false>(contextPtr, str.c_str(), width, strLen, 37, strLen + 5, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, "袜");
    EXPECT_EQ(outLen, 3);

    result = SubstrChar<int32_t, false, false>(contextPtr, str.c_str(), width, strLen, -38, 10, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, "");
    EXPECT_EQ(outLen, 0);

    result = SubstrChar<int32_t, false, false>(contextPtr, str.c_str(), width, strLen, -37, 37, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, str);
    EXPECT_EQ(outLen, strLen);

    delete context;
}

TEST(FunctionTest, SubstrWithStart)
{
    auto context = new ExecutionContext();
    int64_t contextptr = reinterpret_cast<int64_t>(context);
    std::string str = "ABC efg 123 $%^";
    int32_t strlen = static_cast<int32_t>(str.length());
    int32_t outlen = 0;
    const char *result;
    std::string actual;

    result = SubstrVarcharWithStart<int32_t, false, false>(contextptr, str.c_str(), strlen, 1, false, &outlen);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, str);
    EXPECT_EQ(outlen, strlen);

    result = SubstrVarcharWithStart<int32_t, false, true>(contextptr, str.c_str(), strlen, 0, false, &outlen);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, str);
    EXPECT_EQ(outlen, strlen);

    result = SubstrVarcharWithStart<int32_t, false, false>(contextptr, str.c_str(), strlen, 9, false, &outlen);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "123 $%^");
    EXPECT_EQ(outlen, 7);

    result = SubstrVarcharWithStart<int32_t, false, false>(contextptr, str.c_str(), strlen, -3, false, &outlen);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "$%^");
    EXPECT_EQ(outlen, 3);

    result = SubstrVarcharWithStart<int32_t, false, false>(contextptr, str.c_str(), strlen, 0, false, &outlen);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "");
    EXPECT_EQ(outlen, 0);

    delete context;
}

TEST(FunctionTest, SubstrWithZh)
{
    auto context = new ExecutionContext();
    int64_t contextPtr = reinterpret_cast<int64_t>(context);
    std::string str = "时欧基乌斯侧后解 hello! 回复哦黑色的and magic粉色的圣诞袜";
    int32_t strLen = static_cast<int32_t>(str.length());
    int32_t outLen = 0;
    const char *result;
    std::string actual;

    result = SubstrVarcharWithStart<int32_t, false, false>(contextPtr, str.c_str(), strLen, 1, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, str);
    EXPECT_EQ(outLen, strLen);

    result = SubstrVarcharWithStart<int32_t, false, true>(contextPtr, str.c_str(), strLen, 0, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, str);
    EXPECT_EQ(outLen, strLen);

    result = SubstrVarcharWithStart<int32_t, false, false>(contextPtr, str.c_str(), strLen, 9, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, " hello! 回复哦黑色的and magic粉色的圣诞袜");
    EXPECT_EQ(outLen, 53);

    result = SubstrVarcharWithStart<int32_t, false, false>(contextPtr, str.c_str(), strLen, -3, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, "圣诞袜");
    EXPECT_EQ(outLen, 9);

    result = SubstrVarcharWithStart<int32_t, false, false>(contextPtr, str.c_str(), strLen, 0, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, "");
    EXPECT_EQ(outLen, 0);

    result = SubstrVarcharWithStart<int32_t, false, false>(contextPtr, str.c_str(), strLen, 37, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, "袜");
    EXPECT_EQ(outLen, 3);

    result = SubstrVarcharWithStart<int32_t, false, false>(contextPtr, str.c_str(), strLen, -38, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, "");
    EXPECT_EQ(outLen, 0);

    result = SubstrVarcharWithStart<int32_t, false, false>(contextPtr, str.c_str(), strLen, -37, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, str);
    EXPECT_EQ(outLen, strLen);

    delete context;
}

TEST(FunctionTest, SubstrWithZhForSpark)
{
    ConfigUtil::SetNegativeStartIndexOutOfBoundsRule(NegativeStartIndexOutOfBoundsRule::INTERCEPT_FROM_BEYOND);
    auto context = new ExecutionContext();
    int64_t contextPtr = reinterpret_cast<int64_t>(context);
    std::string str = "时欧基乌斯侧后解 h";
    int32_t strLen = static_cast<int32_t>(str.length());
    int32_t outLen = 0;
    const char *result;
    std::string actual;

    result = SubstrVarcharWithStart<int32_t, true, false>(contextPtr, str.c_str(), strLen, -15, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, str);
    EXPECT_EQ(outLen, strLen);

    result = SubstrVarcharWithStart<int32_t, true, true>(contextPtr, str.c_str(), strLen, 0, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, str);
    EXPECT_EQ(outLen, strLen);

    result = SubstrVarchar<int32_t, true, false>(contextPtr, str.c_str(), strLen, -15, 5, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, "");
    EXPECT_EQ(outLen, 0);

    result = SubstrVarchar<int32_t, true, false>(contextPtr, str.c_str(), strLen, -15, 6, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, "时");
    EXPECT_EQ(outLen, 3);

    result = SubstrVarchar<int32_t, true, false>(contextPtr, str.c_str(), strLen, -15, 14, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, "时欧基乌斯侧后解 ");
    EXPECT_EQ(outLen, 25);

    result = SubstrVarchar<int32_t, true, false>(contextPtr, str.c_str(), strLen, -15, 20, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, "时欧基乌斯侧后解 h");
    EXPECT_EQ(outLen, 26);

    std::string strEn = "apple";
    result = SubstrVarchar<int32_t, true, false>(contextPtr, strEn.c_str(), static_cast<int32_t>(strEn.length()), -7, 3,
        false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, "a");

    result = SubstrVarcharWithStart<int32_t, true, false>(contextPtr, strEn.c_str(),
        static_cast<int32_t>(strEn.length()), -7, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, "apple");

    ConfigUtil::SetNegativeStartIndexOutOfBoundsRule(NegativeStartIndexOutOfBoundsRule::EMPTY_STRING);
    delete context;
}

TEST(FunctionTest, SubstrCharWithStart)
{
    auto context = new ExecutionContext();
    int64_t contextptr = reinterpret_cast<int64_t>(context);
    std::string str = "ABC efg 123 $%^        ";
    int32_t width = static_cast<int32_t>(str.length());
    int32_t outlen = 0;
    int32_t strlen = width - 8;
    const char *result;
    std::string actual;

    result = SubstrCharWithStart<int32_t, false, false>(contextptr, str.c_str(), width, strlen, 1, false, &outlen);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "ABC efg 123 $%^");
    EXPECT_EQ(outlen, strlen);

    result = SubstrCharWithStart<int32_t, false, true>(contextptr, str.c_str(), width, strlen, 0, false, &outlen);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "ABC efg 123 $%^");
    EXPECT_EQ(outlen, strlen);

    result = SubstrCharWithStart<int32_t, false, false>(contextptr, str.c_str(), width, strlen, 9, false, &outlen);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "123 $%^");
    EXPECT_EQ(outlen, 7);

    result = SubstrCharWithStart<int32_t, false, false>(contextptr, str.c_str(), width, strlen, -3, false, &outlen);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "$%^");
    EXPECT_EQ(outlen, 3);

    result = SubstrCharWithStart<int32_t, false, false>(contextptr, str.c_str(), width, strlen, 0, false, &outlen);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "");
    EXPECT_EQ(outlen, 0);
    delete context;
}

TEST(FunctionTest, SubstrCharWithStartZh)
{
    auto context = new ExecutionContext();
    int64_t contextPtr = reinterpret_cast<int64_t>(context);
    std::string str = "时欧基乌斯侧后解 hello! 回复哦黑色的and magic粉色的圣诞袜";
    int32_t width = static_cast<int32_t>(str.length());
    int32_t outLen = 0;
    int32_t strLen = width;
    const char *result;
    std::string actual;

    result = SubstrCharWithStart<int32_t, false, false>(contextPtr, str.c_str(), width, strLen, 1, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, str);
    EXPECT_EQ(outLen, strLen);

    result = SubstrCharWithStart<int32_t, false, true>(contextPtr, str.c_str(), width, strLen, 0, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, str);
    EXPECT_EQ(outLen, strLen);

    result = SubstrCharWithStart<int32_t, false, false>(contextPtr, str.c_str(), width, strLen, 9, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, " hello! 回复哦黑色的and magic粉色的圣诞袜");
    EXPECT_EQ(outLen, 53);

    result = SubstrCharWithStart<int32_t, false, false>(contextPtr, str.c_str(), width, strLen, -3, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, "圣诞袜");
    EXPECT_EQ(outLen, 9);

    result = SubstrCharWithStart<int32_t, false, false>(contextPtr, str.c_str(), width, strLen, 0, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, "");
    EXPECT_EQ(outLen, 0);

    result = SubstrCharWithStart<int32_t, false, false>(contextPtr, str.c_str(), width, strLen, 37, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, "袜");
    EXPECT_EQ(outLen, 3);

    result = SubstrCharWithStart<int32_t, false, false>(contextPtr, str.c_str(), width, strLen, -38, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, "");
    EXPECT_EQ(outLen, 0);

    result = SubstrCharWithStart<int32_t, false, false>(contextPtr, str.c_str(), width, strLen, -37, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, str);
    EXPECT_EQ(outLen, strLen);

    delete context;
}

TEST(FunctionTest, ToUpperStr)
{
    auto context = new ExecutionContext();
    int64_t contextptr = reinterpret_cast<int64_t>(context);
    std::string test = "[\\]^_abcdefghijklmnopqrstuvwxyz ABCDEFGHIJKLMNOPQRSTUVWXYZ{|}.";
    std::string expected = "[\\]^_ABCDEFGHIJKLMNOPQRSTUVWXYZ ABCDEFGHIJKLMNOPQRSTUVWXYZ{|}.";
    int32_t outLen = 0;
    const char *result = ToUpperStr(contextptr, test.c_str(), static_cast<int32_t>(test.length()), false, &outLen);
    std::string actual = std::string(result, outLen);
    EXPECT_EQ(actual, expected);
    EXPECT_EQ(outLen, 62);
    delete context;
}

TEST(FunctionTest, ToUpperChar)
{
    auto context = new ExecutionContext();
    int64_t contextptr = reinterpret_cast<int64_t>(context);
    std::string test = "[\\]^_abcdefghijklmnopqrstuvwxyz ABCDEFGHIJKLMNOPQRSTUVWXYZ{|}.";
    std::string expected = "[\\]^_ABCDEFGHIJKLMNOPQRSTUVWXYZ ABCDEFGHIJKLMNOPQRSTUVWXYZ{|}.";
    int32_t width = 100;
    int32_t outLen = 0;
    const char *result =
        ToUpperChar(contextptr, test.c_str(), width, static_cast<int32_t>(test.length()), false, &outLen);
    std::string actual = std::string(result, outLen);
    EXPECT_EQ(actual, expected);
    EXPECT_EQ(outLen, 62);
    delete context;
}

TEST(FunctionTest, ToLowerStr)
{
    auto context = new ExecutionContext();
    int64_t contextPtr = reinterpret_cast<int64_t>(context);
    std::string test = "[\\]^_abcdefghijklmnopqrstuvwxyz ABCDEFGHIJKLMNOPQRSTUVWXYZ{|}.";
    std::string expected = "[\\]^_abcdefghijklmnopqrstuvwxyz abcdefghijklmnopqrstuvwxyz{|}.";
    int32_t outLen = 0;
    const char *result = ToLowerStr(contextPtr, test.c_str(), static_cast<int32_t>(test.length()), false, &outLen);
    std::string actual = std::string(result, outLen);
    EXPECT_EQ(actual, expected);
    EXPECT_EQ(outLen, 62);
    delete context;
}

TEST(FunctionTest, ToLowerChar)
{
    auto context = new ExecutionContext();
    int64_t contextPtr = reinterpret_cast<int64_t>(context);
    std::string test = "[\\]^_abcdefghijklmnopqrstuvwxyz ABCDEFGHIJKLMNOPQRSTUVWXYZ{|}.";
    std::string expected = "[\\]^_abcdefghijklmnopqrstuvwxyz abcdefghijklmnopqrstuvwxyz{|}.";
    int32_t width = 100;
    int32_t outLen = 0;
    const char *result =
        ToLowerChar(contextPtr, test.c_str(), width, static_cast<int32_t>(test.length()), false, &outLen);
    std::string actual = std::string(result, outLen);
    EXPECT_EQ(actual, expected);
    EXPECT_EQ(outLen, 62);
    delete context;
}

TEST(FunctionTest, StrCompare)
{
    int result = StrCompare("abcd EFGH 123 $%^", 17, "abcd EFGH 123 $%^", 17);
    EXPECT_EQ(result, std::string("abcd EFGH 123 $%^").compare(std::string("abcd EFGH 123 $%^")));

    result = StrCompare("five", 4, "four", 4);
    EXPECT_EQ(result, std::string("five").compare(std::string("four")));

    result = StrCompare("five", 4, "FIVE", 4);
    EXPECT_EQ(result, std::string("five").compare(std::string("FIVE")));

    result = StrCompare("test", 4, "testing", 7);
    EXPECT_EQ(result, std::string("test").compare(std::string("testing")));

    result = StrCompare("racecar", 7, "race", 4);
    EXPECT_EQ(result, std::string("racecar").compare(std::string("race")));
}

TEST(FunctionTest, LikeStr)
{
    bool result = LikeStr("hello", 5, "hello", 5, false);
    EXPECT_TRUE(result);

    result = LikeStr("regex", 5, "rege(x(es)?|xps?)", 17, false);
    EXPECT_TRUE(result);

    result = LikeStr("20500", 5, "\\d{5}(-\\d{4})?", 14, false);
    EXPECT_TRUE(result);
}

TEST(FunctionTest, ConcatStrStr)
{
    auto context = new ExecutionContext();
    int64_t contextptr = reinterpret_cast<int64_t>(context);
    int outlen = 0;
    const char *result;
    std::string actual;

    result = ConcatStrStr(contextptr, "abc", 3, "defghi", 6, false, &outlen);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "abcdefghi");
    EXPECT_EQ(outlen, 9);

    result = ConcatStrStr(contextptr, "hello", 5, "", 0, false, &outlen);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "hello");
    EXPECT_EQ(outlen, 5);

    result = ConcatStrStr(contextptr, "", 0, "", 0, false, &outlen);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "");
    EXPECT_EQ(outlen, 0);
    delete context;
}

TEST(FunctionTest, Instr)
{
    int32_t result = InStr("", 0, "", 0, true);
    EXPECT_EQ(result, 0);
    result = InStr("", 0, "abc", 3, true);
    EXPECT_EQ(result, 0);
    result = InStr("abc", 3, "", 0, true);
    EXPECT_EQ(result, 0);
    result = InStr("abc", 3, "abcd", 4, false);
    EXPECT_EQ(result, 0);
    result = InStr("abc", 3, "bd", 2, false);
    EXPECT_EQ(result, 0);
    result = InStr("abc", 3, "bc", 2, false);
    EXPECT_EQ(result, 2);
    result = InStr("", 0, "ab", 2, false);
    EXPECT_EQ(result, 0);
    result = InStr("abc", 3, "", 0, false);
    EXPECT_EQ(result, 1);
    result = InStr("", 0, "", 0, false);
    EXPECT_EQ(result, 1);
    std::string srcStr = "一丁丂七丄丅丆万丈三上下丌不与丏";
    std::string subStr = "万丈";
    result = InStr(srcStr.c_str(), static_cast<int32_t>(srcStr.length()), subStr.c_str(),
        static_cast<int32_t>(subStr.length()), false);
    EXPECT_EQ(result, 8);
    srcStr = "壹貳叁肆伍";
    subStr = "叁肆";
    result = InStr(srcStr.c_str(), static_cast<int32_t>(srcStr.length()), subStr.c_str(),
        static_cast<int32_t>(subStr.length()), false);
    EXPECT_EQ(result, 3);
}

TEST(FunctionTest, StartsWithStr)
{
    bool result = StartsWithStr("", 0, "", 0, true);
    EXPECT_EQ(result, false);
    result = StartsWithStr("", 0, "abc", 3, true);
    EXPECT_EQ(result, false);
    result = StartsWithStr("abc", 3, "", 0, true);
    EXPECT_EQ(result, false);
    result = StartsWithStr("abc", 3, "abcd", 4, false);
    EXPECT_EQ(result, false);
    result = StartsWithStr("abc", 3, "bd", 2, false);
    EXPECT_EQ(result, false);
    result = StartsWithStr("abc", 3, "ab", 2, false);
    EXPECT_EQ(result, true);
    result = StartsWithStr("", 0, "ab", 2, false);
    EXPECT_EQ(result, false);
    result = StartsWithStr("abc", 3, "", 0, false);
    EXPECT_EQ(result, true);
    result = StartsWithStr("", 0, "", 0, false);
    EXPECT_EQ(result, true);
}

TEST(FunctionTest, EndsWithStr)
{
    bool result = EndsWithStr("", 0, "", 0, true);
    EXPECT_EQ(result, false);
    result = EndsWithStr("", 0, "abc", 3, true);
    EXPECT_EQ(result, false);
    result = EndsWithStr("abc", 3, "", 0, true);
    EXPECT_EQ(result, false);
    result = EndsWithStr("abc", 3, "abcd", 4, false);
    EXPECT_EQ(result, false);
    result = EndsWithStr("abc", 3, "bd", 2, false);
    EXPECT_EQ(result, false);
    result = EndsWithStr("abc", 3, "bc", 2, false);
    EXPECT_EQ(result, true);
    result = EndsWithStr("", 0, "ab", 2, false);
    EXPECT_EQ(result, false);
    result = EndsWithStr("abc", 3, "", 0, false);
    EXPECT_EQ(result, true);
    result = EndsWithStr("", 0, "", 0, false);
    EXPECT_EQ(result, true);
}

// Cast
TEST(FunctionTest, CastStringToDate)
{
    ConfigUtil::SetStringToDateFormatRule(StringToDateFormatRule::ALLOW_REDUCED_PRECISION);
    // year-month-day
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);
    int32_t result = CastStringToDateAllowReducePrecison(contextPtr, "1970-01-03", 10, false);
    EXPECT_EQ(result, 2);
    result = CastStringToDateAllowReducePrecison(contextPtr, "1969-12-31", 10, false);
    EXPECT_EQ(result, -1);
    result = CastStringToDateAllowReducePrecison(contextPtr, "1980-01-01", 10, false);
    EXPECT_EQ(result, 3652);
    result = CastStringToDateAllowReducePrecison(contextPtr, "1980-01-01 12345", 16, false);
    EXPECT_EQ(result, 3652);
    result = CastStringToDateAllowReducePrecison(contextPtr, "1980-1-1", 8, false);
    EXPECT_EQ(result, 3652);
    result = CastStringToDateAllowReducePrecison(contextPtr, "1980-1-01", 9, false);
    EXPECT_EQ(result, 3652);
    result = CastStringToDateAllowReducePrecison(contextPtr, "1980-1-1 123", 12, false);
    EXPECT_EQ(result, 3652);
    result = CastStringToDateAllowReducePrecison(contextPtr, "1980-01-1", 9, false);
    EXPECT_EQ(result, 3652);
    result = CastStringToDateAllowReducePrecison(contextPtr, "1980-01", 7, false);
    EXPECT_EQ(result, 3652);
    result = CastStringToDateAllowReducePrecison(contextPtr, "1980", 4, false);
    EXPECT_EQ(result, 3652);
    result = CastStringToDateAllowReducePrecison(contextPtr, "1453-05-29", 10, false);
    EXPECT_EQ(result, -188682);
    result = CastStringToDateAllowReducePrecison(contextPtr, "   1453-05-29   ", 16, false);
    EXPECT_EQ(result, -188682);
    result = CastStringToDateAllowReducePrecison(contextPtr, "   1 453-05-29   ", 16, false);
    EXPECT_EQ(result, -1);
    result = CastStringToDateAllowReducePrecison(contextPtr, " 145", 4, false);
    EXPECT_EQ(result, -1);
    result = CastStringToDateAllowReducePrecison(contextPtr, "-145", 4, false);
    EXPECT_EQ(result, -1);

    result = CastStringToDateAllowReducePrecison(contextPtr, "1996-09  ", 9, false);
    EXPECT_EQ(result, 9740);
    result = CastStringToDateAllowReducePrecison(contextPtr, "1996-09-30", 10, false);
    EXPECT_EQ(result, 9769);

    bool isNull = false;
    result = CastStringToDateRetNullAllowReducePrecison(&isNull, "   1453- 05-29    ", 16);
    EXPECT_EQ(result, -1);
    result = CastStringToDateRetNullAllowReducePrecison(&isNull, "1453-05-29", 10);
    EXPECT_EQ(result, -188682);
    result = CastStringToDateRetNullAllowReducePrecison(&isNull, "   1453-05-29   ", 16);
    EXPECT_EQ(result, -188682);
    ConfigUtil::SetStringToDateFormatRule(StringToDateFormatRule::NOT_ALLOW_REDUCED_PRECISION);
    delete context;
}

TEST(FunctionTest, LengthChar)
{
    std::string test = "abcd";
    int32_t width = 10;
    auto len = LengthChar(test.c_str(), width, test.length(), false);
    EXPECT_EQ(len, 10);
}

TEST(FunctionTest, LengthStr)
{
    std::string test = "abcd";
    auto len = LengthStr(test.c_str(), test.length(), false);
    EXPECT_EQ(len, 4);
}

TEST(FunctionTest, LengthStrZh)
{
    std::string str = "时欧基乌斯侧后解 hello! 回复哦黑色的and magic粉色的圣诞袜";
    auto len = LengthStr(str.c_str(), str.length(), false);
    EXPECT_EQ(len, 37);
}

TEST(FunctionTest, ReplaceStrStrStrWithRep)
{
    auto context = new ExecutionContext();
    int64_t contextPtr = reinterpret_cast<int64_t>(context);
    int32_t outLen = 0;

    std::string str = "operator1";
    std::string searchStr = "o";
    std::string replaceStr = "**";
    auto result = ReplaceStrStrStrWithRepReplace(contextPtr, str.c_str(), str.length(), searchStr.c_str(),
        searchStr.length(), replaceStr.c_str(), replaceStr.length(), false, &outLen);
    std::string expected = "**perat**r1";
    EXPECT_EQ(outLen, 11);
    EXPECT_EQ(std::string(result, outLen), expected);

    str = "operator2";
    searchStr = "";
    replaceStr = "*";
    result = ReplaceStrStrStrWithRepReplace(contextPtr, str.c_str(), str.length(), searchStr.c_str(),
        searchStr.length(), replaceStr.c_str(), replaceStr.length(), false, &outLen);
    expected = "*o*p*e*r*a*t*o*r*2*";
    EXPECT_EQ(outLen, 19);
    EXPECT_EQ(std::string(result, outLen), expected);

    str = "operator3";
    searchStr = "era";
    replaceStr = "ER";
    result = ReplaceStrStrStrWithRepReplace(contextPtr, str.c_str(), str.length(), searchStr.c_str(),
        searchStr.length(), replaceStr.c_str(), replaceStr.length(), false, &outLen);
    expected = "opERtor3";
    EXPECT_EQ(outLen, 8);
    EXPECT_EQ(std::string(result, outLen), expected);
    delete context;
}

TEST(FunctionTest, ReplaceStrStrWithoutRep)
{
    auto context = new ExecutionContext();
    int64_t contextPtr = reinterpret_cast<int64_t>(context);
    int32_t outLen = 0;

    std::string str = "operator1";
    std::string searchStr = "o";
    auto result = ReplaceStrStrWithoutRepReplace(contextPtr, str.c_str(), str.length(), searchStr.c_str(),
        searchStr.length(), false, &outLen);
    std::string expected = "peratr1";
    EXPECT_EQ(outLen, 7);
    EXPECT_EQ(std::string(result, outLen), expected);

    str = "operator2";
    searchStr = "";
    result = ReplaceStrStrWithoutRepReplace(contextPtr, str.c_str(), str.length(), searchStr.c_str(),
        searchStr.length(), false, &outLen);
    expected = "operator2";
    EXPECT_EQ(outLen, 9);
    EXPECT_EQ(std::string(result, outLen), expected);

    str = "operator3";
    searchStr = "era";
    result = ReplaceStrStrWithoutRepReplace(contextPtr, str.c_str(), str.length(), searchStr.c_str(),
        searchStr.length(), false, &outLen);
    expected = "optor3";
    EXPECT_EQ(outLen, 6);
    EXPECT_EQ(std::string(result, outLen), expected);
    delete context;
}

TEST(FunctionTest, ReplaceStrCharStr)
{
    auto context = new ExecutionContext();
    int64_t contextPtr = reinterpret_cast<int64_t>(context);
    int32_t outLen = 0;

    std::string str[] = { "", " varchar2", "", "varchar4", "varchar5", "varchar6", "varchar7" };
    std::string searchStr[] =
                { "          ", " char200  ", "char300   ", "char400   ", "char500   ", "char600   ", "char700   " };
    std::string replaceStr[] = { "", " varchar2", "", "varchar4", "varchar5", "varchar6", "varchar7" };
    int32_t resultLen[] = { 0, 9, 0, 8, 8, 8, 8 };
    std::string expected[] = { "", " varchar2", "", "varchar4", "varchar5", "varchar6", "varchar7" };

    for (int32_t i = 0; i < 7; i++) {
        auto result = ReplaceStrStrStrWithRepReplace(contextPtr, str[i].c_str(), str[i].length(), searchStr[i].c_str(),
            searchStr[i].length(), replaceStr[i].c_str(), replaceStr[i].length(), false, &outLen);
        EXPECT_EQ(outLen, resultLen[i]);
        EXPECT_EQ(std::string(result, outLen), expected[i]);
    }
    delete context;
}

TEST(FunctionTest, ReplaceCharCharChar)
{
    auto context = new ExecutionContext();
    int64_t contextPtr = reinterpret_cast<int64_t>(context);
    int32_t outLen = 0;

    std::string str[] = { "          ", " char200  ", "          ", "char400   ", "char500   ", "char600   ",
                              "char700   " };
    std::string searchStr[] =
                { "cha1     ", " char2     ", "char3     ", "char4     ", "char5     ", "char6     ", "char7     " };
    std::string replaceStr[] =
                { "varchar100", "varchar200", "varchar300", "varchar400", "varchar500", "varchar600", "varchar700" };
    int32_t resultLen[] = { 10, 10, 10, 10, 10, 10, 10 };
    std::string expected[] =
                { "          ", " char200  ", "          ", "char400   ", "char500   ", "char600   ", "char700   " };

    for (int32_t i = 0; i < 7; i++) {
        auto result = ReplaceStrStrStrWithRepReplace(contextPtr, str[i].c_str(), str[i].length(), searchStr[i].c_str(),
            searchStr[i].length(), replaceStr[i].c_str(), replaceStr[i].length(), false, &outLen);
        EXPECT_EQ(outLen, resultLen[i]);
        EXPECT_EQ(std::string(result, outLen), expected[i]);
    }
    delete context;
}

TEST(FunctionTest, ReplaceStrStrStrZh)
{
    auto context = new ExecutionContext();
    int64_t contextPtr = reinterpret_cast<int64_t>(context);
    int32_t outLen = 0;

    std::vector<std::string> str { "", "粉色的圣诞袜", "apple", "粉色de圣诞袜" };
    std::vector<std::string> searchStr { "", "粉色", "pp", "de圣" };
    std::vector<std::string> replaceStr { "", "黑色", "*w*", "*的*" };

    auto result1 = ReplaceStrStrStrWithRepReplace(contextPtr, str[2].c_str(), str[2].length(), searchStr[0].c_str(),
        searchStr[0].length(), replaceStr[2].c_str(), replaceStr[2].length(), false, &outLen);
    std::string expected = "*w*a*w*p*w*p*w*l*w*e*w*";
    EXPECT_EQ(outLen, 23);
    EXPECT_EQ(std::string(result1, outLen), expected);

    auto result2 = ReplaceStrStrStrWithRepReplace(contextPtr, str[1].c_str(), str[1].length(), searchStr[0].c_str(),
        searchStr[0].length(), replaceStr[2].c_str(), replaceStr[2].length(), false, &outLen);
    expected = "*w*粉*w*色*w*的*w*圣*w*诞*w*袜*w*";
    EXPECT_EQ(outLen, 39);
    EXPECT_EQ(std::string(result2, outLen), expected);

    auto result3 = ReplaceStrStrStrWithRepReplace(contextPtr, str[3].c_str(), str[3].length(), searchStr[0].c_str(),
        searchStr[0].length(), replaceStr[2].c_str(), replaceStr[2].length(), false, &outLen);
    expected = "*w*粉*w*色*w*d*w*e*w*圣*w*诞*w*袜*w*";
    EXPECT_EQ(outLen, 41);
    EXPECT_EQ(std::string(result3, outLen), expected);

    auto result4 = ReplaceStrStrStrWithRepReplace(contextPtr, str[3].c_str(), str[3].length(), searchStr[0].c_str(),
        searchStr[0].length(), replaceStr[3].c_str(), replaceStr[3].length(), false, &outLen);
    expected = "*的*粉*的*色*的*d*的*e*的*圣*的*诞*的*袜*的*";
    EXPECT_EQ(outLen, 57);
    EXPECT_EQ(std::string(result4, outLen), expected);

    auto result5 = ReplaceStrStrStrWithRepReplace(contextPtr, str[3].c_str(), str[3].length(), searchStr[3].c_str(),
        searchStr[3].length(), replaceStr[3].c_str(), replaceStr[3].length(), false, &outLen);
    expected = "粉色*的*诞袜";
    EXPECT_EQ(outLen, 17);
    EXPECT_EQ(std::string(result5, outLen), expected);

    auto result6 = ReplaceStrStrStrWithRepReplace(contextPtr, str[0].c_str(), str[0].length(), searchStr[0].c_str(),
        searchStr[0].length(), replaceStr[0].c_str(), replaceStr[0].length(), false, &outLen);
    expected = "";
    EXPECT_EQ(outLen, 0);
    EXPECT_EQ(std::string(result6, outLen), expected);

    auto result7 = ReplaceStrStrStrWithRepReplace(contextPtr, str[3].c_str(), str[3].length(), searchStr[0].c_str(),
        searchStr[0].length(), replaceStr[0].c_str(), replaceStr[0].length(), false, &outLen);
    expected = "粉色de圣诞袜";
    EXPECT_EQ(outLen, 17);
    EXPECT_EQ(std::string(result7, outLen), expected);
    delete context;
}

TEST(FunctionTest, ConcatStrStrZh)
{
    auto context = new ExecutionContext();
    int64_t contextPtr = reinterpret_cast<int64_t>(context);
    int outLen = 0;
    const char *result;
    std::string actual;

    result = ConcatStrStr(contextPtr, "你是Chinese?", 14, "Yes我是", 9, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, "你是Chinese?Yes我是");
    EXPECT_EQ(outLen, 23);
    delete context;
}

TEST(FunctionTest, ConcatCharCharZh)
{
    auto context = new ExecutionContext();
    int64_t contextPtr = reinterpret_cast<int64_t>(context);
    int32_t outLen = 0;
    const char *result;
    std::string actual;

    result = ConcatCharChar(contextPtr, "粉色de圣诞袜", 7, 17, "*黑色*", 4, 8, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, "粉色de圣诞袜*黑色*");
    EXPECT_EQ(outLen, 25);

    result = ConcatCharChar(contextPtr, "Hei你好吗", 8, 12, "Oh我很好", 8, 11, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, "Hei你好吗  Oh我很好");
    EXPECT_EQ(outLen, 25);

    result = ConcatCharChar(contextPtr, "Hei你好吗   ", 10, 15, "Oh我很好  ", 8, 13, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, "Hei你好吗    Oh我很好  ");
    EXPECT_EQ(outLen, 29);

    result = ConcatCharChar(contextPtr, "   Hei你好吗", 12, 15, "   Oh我很好", 12, 14, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, "   Hei你好吗      Oh我很好");
    EXPECT_EQ(outLen, 32);

    result = ConcatCharChar(contextPtr, "Hei   你好吗", 12, 15, "Oh   我很好", 8, 14, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, "Hei   你好吗   Oh   我很好");
    EXPECT_EQ(outLen, 32);

    result = ConcatCharChar(contextPtr, "   ", 5, 3, "Oh我很好   ", 12, 14, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, "     Oh我很好   ");
    EXPECT_EQ(outLen, 19);

    result = ConcatCharChar(contextPtr, "Hei你好吗", 8, 12, "   ", 5, 3, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, "Hei你好吗     ");
    EXPECT_EQ(outLen, 17);

    result = ConcatCharChar(contextPtr, "Hei你好吗", 8, 12, "", 5, 0, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, "Hei你好吗");
    EXPECT_EQ(outLen, 12);
    delete context;
}

TEST(FunctionTest, ConcatCharStrZh)
{
    auto context = new ExecutionContext();
    int64_t contextPtr = reinterpret_cast<int64_t>(context);
    int outLen = 0;
    const char *result;
    std::string actual;

    result = ConcatCharStr(contextPtr, "*你是谁呢*", 6, 14, "我很OK", 8, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, "*你是谁呢*我很OK");
    EXPECT_EQ(outLen, 22);

    result = ConcatCharStr(contextPtr, "*你是谁呢*", 10, 14, "我很OK", 8, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, "*你是谁呢*    我很OK");
    EXPECT_EQ(outLen, 26);
    delete context;
}

TEST(FunctionTest, ConcatStrCharZh)
{
    auto context = new ExecutionContext();
    int64_t contextPtr = reinterpret_cast<int64_t>(context);
    int outLen = 0;
    const char *result;
    std::string actual;

    result = ConcatStrChar(contextPtr, "粉色de圣诞袜", 17, "*黑色*", 4, 8, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, "粉色de圣诞袜*黑色*");
    EXPECT_EQ(outLen, 25);

    result = ConcatStrChar(contextPtr, "粉色de圣诞袜", 17, "*黑色*", 6, 8, false, &outLen);
    actual = std::string(result, outLen);
    EXPECT_EQ(actual, "粉色de圣诞袜*黑色*");
    EXPECT_EQ(outLen, 25);
    delete context;
}

TEST(FunctionTest, LikeStrZh)
{
    std::string str = "时欧基乌斯侧后解 hello! 回复哦黑色的and magic粉色的圣诞袜";
    // like "xxx_"
    std::string pattern = "^时欧基乌斯侧后解 hello! 回复哦黑色的and magic粉色的圣诞.$";
    bool isMatch = LikeStr(str.c_str(), str.length(), pattern.c_str(), pattern.length(), false);
    EXPECT_TRUE(isMatch);
    pattern = "^时欧基乌斯侧后解 hello! 回复哦黑色的and magic粉色的圣诞..$";
    isMatch = LikeStr(str.c_str(), str.length(), pattern.c_str(), pattern.length(), false);
    EXPECT_FALSE(isMatch);

    // like "xxx%"
    pattern = "^时欧基乌斯侧后解 hello! 回复哦黑色的and magic粉色的圣.*$";
    isMatch = LikeStr(str.c_str(), str.length(), pattern.c_str(), pattern.length(), false);
    EXPECT_TRUE(isMatch);
    pattern = "^欧时基乌斯侧后解 hello! 回复哦黑色的and magic粉色的圣诞.*$";
    isMatch = LikeStr(str.c_str(), str.length(), pattern.c_str(), pattern.length(), false);
    EXPECT_FALSE(isMatch);
}

TEST(FunctionTest, LikeCharZh)
{
    // like "xxx_"
    std::string str = "时欧基乌斯侧后解 hello! 回复哦黑色的and magic粉色的圣诞袜";
    std::string pattern = "^时欧基乌斯侧后解 hello! 回复哦黑色的and magic粉色的圣诞.$";
    bool isMatch = LikeChar(str.c_str(), 37, str.length(), pattern.c_str(), pattern.length(), false);
    EXPECT_TRUE(isMatch);

    pattern = "^时欧基乌..$";
    str = "时欧基乌";
    isMatch = LikeChar(str.c_str(), 6, str.length(), pattern.c_str(), pattern.length(), false);
    EXPECT_TRUE(isMatch);

    pattern = "^时欧基乌.$";
    str = "时欧基乌";
    isMatch = LikeChar(str.c_str(), 6, str.length(), pattern.c_str(), pattern.length(), false);
    EXPECT_FALSE(isMatch);

    // like "xxx%"
    pattern = "^时欧基乌.*$";
    str = "时欧基乌";
    isMatch = LikeChar(str.c_str(), 6, str.length(), pattern.c_str(), pattern.length(), false);
    EXPECT_TRUE(isMatch);
}

TEST(FunctionTest, CastStringToLong)
{
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);
    std::string s = "23423";
    int64_t result = CastStringToLong(contextPtr, s.c_str(), static_cast<int32_t>(s.size()), false);

    EXPECT_EQ(result, 23423);
    s = "100123";
    result = CastStringToLong(contextPtr, s.c_str(), static_cast<int32_t>(s.size()), false);
    EXPECT_EQ(result, 100123);
    s = "-10078";
    result = CastStringToLong(contextPtr, s.c_str(), static_cast<int32_t>(s.size()), false);
    EXPECT_EQ(result, -10078);
    s = "123.123";
    result = CastStringToLong(contextPtr, s.c_str(), static_cast<int32_t>(s.size()), false);
    EXPECT_EQ(result, 0);
    s = "9223372036854775807";
    result = CastStringToLong(contextPtr, s.c_str(), static_cast<int32_t>(s.size()), false);
    EXPECT_EQ(result, 922'3372'0368'5477'5807);
    s = "9223372036854775808";
    result = CastStringToLong(contextPtr, s.c_str(), static_cast<int32_t>(s.size()), false);
    EXPECT_EQ(result, 0);
    s = "-9223372036854775808";
    result = CastStringToLong(contextPtr, s.c_str(), static_cast<int32_t>(s.size()), false);
    EXPECT_EQ(result, INT64_MIN);
    delete context;
}

TEST(FunctionTest, CastStringToInt)
{
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);

    std::string s = " 23423 ";
    int32_t result = CastStringToInt(contextPtr, s.c_str(), static_cast<int32_t>(s.size()), false);
    EXPECT_EQ(result, 23423);
    s = "100123";
    result = CastStringToInt(contextPtr, s.c_str(), static_cast<int32_t>(s.size()), false);
    EXPECT_EQ(result, 100123);
    s = "123.123";
    result = CastStringToInt(contextPtr, s.c_str(), static_cast<int32_t>(s.size()), false);
    EXPECT_EQ(result, 0);
    s = "2147483648";
    result = CastStringToInt(contextPtr, s.c_str(), static_cast<int32_t>(s.size()), false);
    EXPECT_EQ(result, 0);
    s = "2a147483648";
    result = CastStringToInt(contextPtr, s.c_str(), static_cast<int32_t>(s.size()), false);
    EXPECT_TRUE(context->HasError());
    context->ResetError();
    s = " -10078 ";
    result = CastStringToInt(contextPtr, s.c_str(), static_cast<int32_t>(s.size()), false);
    EXPECT_EQ(result, -10078);
    EXPECT_FALSE(context->HasError());
    s = "2123123123147483648";
    result = CastStringToInt(contextPtr, s.c_str(), static_cast<int32_t>(s.size()), false);
    EXPECT_TRUE(context->HasError());
    context->ResetError();
    s = "-2123123123147483648";
    result = CastStringToInt(contextPtr, s.c_str(), static_cast<int32_t>(s.size()), false);
    EXPECT_TRUE(context->HasError());
    context->ResetError();
    s = "-2123123123147-483648";
    result = CastStringToInt(contextPtr, s.c_str(), static_cast<int32_t>(s.size()), false);
    EXPECT_TRUE(context->HasError());
    delete context;
}

TEST(FunctionTest, CastStringToIntRetNull)
{
    bool isNull = false;
    std::string s = "23423";
    int32_t result = CastStringToIntRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_EQ(result, 23423);
    EXPECT_FALSE(isNull);
    s = "100123";
    result = CastStringToIntRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_EQ(result, 100123);
    EXPECT_FALSE(isNull);
    s = "123.123";
    result = CastStringToIntRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_EQ(result, 123);
    EXPECT_FALSE(isNull);
    s = "2147483648";
    result = CastStringToIntRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_TRUE(isNull);
    s = "2a147483648";
    result = CastStringToIntRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_TRUE(isNull);
    s = "-10078";
    result = CastStringToIntRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_EQ(result, -10078);
    EXPECT_FALSE(isNull);
    s = "2123123123147483648";
    result = CastStringToIntRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_TRUE(isNull);
    s = "-2123123123147483648";
    result = CastStringToIntRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_TRUE(isNull);
    s = "-2123123123147-483648";
    result = CastStringToIntRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_TRUE(isNull);
    s = "+45";
    result = CastStringToIntRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_EQ(result, 45);
    EXPECT_FALSE(isNull);
    s = "-45";
    result = CastStringToIntRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_EQ(result, -45);
    EXPECT_FALSE(isNull);
    s = "3.14159";
    result = CastStringToIntRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_EQ(result, 3);
    EXPECT_FALSE(isNull);
    s = "31337 with words";
    result = CastStringToIntRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_TRUE(isNull);
    s = "+12345678901";
    result = CastStringToIntRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_TRUE(isNull);
    s = "-12345678901";
    result = CastStringToIntRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_TRUE(isNull);
    s = "2147483647.2";
    result = CastStringToIntRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_EQ(result, 2147483647);
    EXPECT_FALSE(isNull);
    s = "2147483648.2";
    result = CastStringToIntRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_TRUE(isNull);
    s = "     2147483647.2    ";
    result = CastStringToIntRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_EQ(result, 2147483647);
    EXPECT_FALSE(isNull);
    s = "    a 2147483647.2    ";
    result = CastStringToIntRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_TRUE(isNull);
    s = "     2147483647.2   a ";
    result = CastStringToIntRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_TRUE(isNull);
    s = ".";
    result = CastStringToIntRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(isNull);
    s = ".2";
    result = CastStringToIntRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(isNull);
    s = "0.";
    result = CastStringToIntRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(isNull);
    s = "2.3e3";
    result = CastStringToIntRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_TRUE(isNull);
    s = "-1e+2";
    result = CastStringToIntRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_TRUE(isNull);
    s = "+.";
    result = CastStringToIntRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(isNull);
    s = "-.";
    result = CastStringToIntRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(isNull);
    s = "- .";
    result = CastStringToIntRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_TRUE(isNull);
    s = "";
    result = CastStringToIntRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_TRUE(isNull);
    s = "    ";
    result = CastStringToIntRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_TRUE(isNull);
    s = "  +   ";
    result = CastStringToIntRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_TRUE(isNull);
    s = "-";
    result = CastStringToIntRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_TRUE(isNull);
    s = "-123.a";
    result = CastStringToIntRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_TRUE(isNull);
    s = "-123.";
    result = CastStringToIntRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_EQ(result, -123);
}

TEST(FunctionTest, CastStringToShortRetNull)
{
    bool isNull = false;
    std::string s = "23423";
    int32_t result = CastStringToShortRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_EQ(result, 23423);
    EXPECT_FALSE(isNull);
    s = "123.123";
    result = CastStringToShortRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_EQ(result, 123);
    EXPECT_FALSE(isNull);
    s = "2a147483648";
    result = CastStringToShortRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_TRUE(isNull);
    s = "-10078";
    result = CastStringToShortRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_EQ(result, -10078);
    EXPECT_FALSE(isNull);
    s = "2123123123147483648";
    result = CastStringToShortRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_TRUE(isNull);
    s = "-2123123123147483648";
    result = CastStringToShortRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_TRUE(isNull);
    s = "-2123123123147-483648";
    result = CastStringToShortRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_TRUE(isNull);
    s = "+45";
    result = CastStringToShortRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_EQ(result, 45);
    EXPECT_FALSE(isNull);
    s = "-45";
    result = CastStringToShortRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_EQ(result, -45);
    EXPECT_FALSE(isNull);
    s = "3.14159";
    result = CastStringToShortRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_EQ(result, 3);
    EXPECT_FALSE(isNull);
}

TEST(FunctionTest, CastStringToByteRetNull)
{
    bool isNull = false;
    std::string s = "123";
    int8_t result = CastStringToByteRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_EQ(result, 123);
    EXPECT_FALSE(isNull);
    s = "123.123";
    result = CastStringToByteRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_EQ(result, 123);
    EXPECT_FALSE(isNull);
    s = "2147483648";
    result = CastStringToByteRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_TRUE(isNull);
    s = "2a147483648";
    result = CastStringToByteRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_TRUE(isNull);
    s = "2123123123147483648";
    result = CastStringToByteRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_TRUE(isNull);
    s = "-2123123123147483648";
    result = CastStringToIntRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_TRUE(isNull);
    s = "-2123123123147-483648";
    result = CastStringToByteRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_TRUE(isNull);
    s = "+45";
    result = CastStringToByteRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_EQ(result, 45);
    EXPECT_FALSE(isNull);
    s = "-45";
    result = CastStringToByteRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_EQ(result, -45);
    EXPECT_FALSE(isNull);
}

TEST(FunctionTest, CastStringToLongRetNull)
{
    bool isNull = false;
    std::string s = "23423";
    int64_t result = CastStringToLongRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_EQ(result, 23423);
    EXPECT_FALSE(isNull);
    s = "123.123";
    result = CastStringToLongRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_EQ(result, 123);
    EXPECT_FALSE(isNull);
    s = "2147483648";
    result = CastStringToLongRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_EQ(result, 2147483648);
    EXPECT_FALSE(isNull);
    s = "2a147483648";
    result = CastStringToLongRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_TRUE(isNull);
    s = "-10078";
    result = CastStringToLongRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_EQ(result, -10078);
    EXPECT_FALSE(isNull);
    s = std::to_string(std::numeric_limits<int64_t>::min());
    result = CastStringToLongRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_EQ(result, std::numeric_limits<int64_t>::min());
    EXPECT_FALSE(isNull);
    s = std::to_string(std::numeric_limits<int64_t>::max());
    result = CastStringToLongRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_EQ(result, std::numeric_limits<int64_t>::max());
    EXPECT_FALSE(isNull);
    s = std::to_string(std::numeric_limits<uint64_t>::max());
    result = CastStringToLongRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_TRUE(isNull);
    s = "-9223372036854775808";
    result = CastStringToLongRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_EQ(result, INT64_MIN);
    EXPECT_FALSE(isNull);
    s = "9223372036854775807";
    result = CastStringToLongRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_EQ(result, INT64_MAX);
    EXPECT_FALSE(isNull);
    s = "-9223372036854775818";
    result = CastStringToLongRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_TRUE(isNull);
    s = "9223372036854775817";
    result = CastStringToLongRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_TRUE(isNull);
    s = "-2123123123147-483648";
    result = CastStringToLongRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_TRUE(isNull);
    s = "     2147483647.2    ";
    result = CastStringToLongRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_EQ(result, 2147483647);
    EXPECT_FALSE(isNull);
    s = "    a 2147483647.2    ";
    result = CastStringToLongRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_TRUE(isNull);
    s = "     2147483647.2   a ";
    result = CastStringToLongRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_TRUE(isNull);
    s = ".";
    result = CastStringToLongRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(isNull);
    s = ".2";
    result = CastStringToLongRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(isNull);
    s = "0.";
    result = CastStringToLongRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(isNull);
    s = "2.3e3";
    result = CastStringToLongRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_TRUE(isNull);
    s = "-1e+2";
    result = CastStringToLongRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_TRUE(isNull);
}

TEST(FunctionTest, CastStringToDouble)
{
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);
    std::string s = "23423";
    double result = CastStringToDouble(contextPtr, s.c_str(), static_cast<int32_t>(s.size()), false);
    EXPECT_EQ(result, 23423);
    s = "100123";
    result = CastStringToDouble(contextPtr, s.c_str(), static_cast<int32_t>(s.size()), false);
    EXPECT_EQ(result, 100123);
    s = "-10078";
    result = CastStringToDouble(contextPtr, s.c_str(), static_cast<int32_t>(s.size()), false);
    EXPECT_EQ(result, -10078);
    s = "-923.4123";
    result = CastStringToDouble(contextPtr, s.c_str(), static_cast<int32_t>(s.size()), false);
    EXPECT_EQ(result, -923.4123);
    s = "123.123";
    result = CastStringToDouble(contextPtr, s.c_str(), static_cast<int32_t>(s.size()), false);
    EXPECT_EQ(result, 123.123);
    s = "-10.11";
    result = CastStringToDouble(contextPtr, s.c_str(), static_cast<int32_t>(s.size()), false);
    EXPECT_EQ(result, -10.11);
    s = "999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999";
    result = CastStringToDouble(contextPtr, s.c_str(), static_cast<int32_t>(s.size()), false);
    EXPECT_EQ(result, 1e+108);
    s = "1.111e202";
    result = CastStringToDouble(contextPtr, s.c_str(), static_cast<int32_t>(s.size()), false);
    EXPECT_EQ(result, 1.111e202);
    s = "1.111e-202";
    result = CastStringToDouble(contextPtr, s.c_str(), static_cast<int32_t>(s.size()), false);
    EXPECT_EQ(result, 1.111e-202);
    s = "62229.33";
    bool isNull = false;
    result = CastStringToDoubleRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_EQ(result, 62229.33);
    isNull = false;
    s = "1234ee231";
    result = CastStringToDoubleRetNull(&isNull, s.c_str(), static_cast<int32_t>(s.size()));
    EXPECT_TRUE(isNull);
    delete context;
}

TEST(FunctionTest, EvaluateHiveUdfSingle)
{
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);
    int32_t inputTypes[] = { OMNI_INT, OMNI_INT };
    int32_t retType = OMNI_INT;
    int32_t vecCount = 2;

    int32_t inputValue[2] = { 3, 5 };
    uint8_t inputNull[2] = { 0, 0 };
    int64_t inputLength = 0;
    int32_t outputValue;
    int8_t outputNull;
    int32_t outputLength;

    using namespace omniruntime::mock;
    using namespace testing::internal;
    using testing::_;
    using testing::Assign;
    using testing::DoAll;
    using testing::Return;
    JNIEnvMock *env = CreateJNIEnvMock();
    JniUtil::SetEnv(env);

    // for InitHiveUdf
    EXPECT_CALL(*env, FindClass(_)).WillRepeatedly(Return(nullptr));
    EXPECT_CALL(*env, ExceptionCheck()).WillRepeatedly(Return(false));
    EXPECT_CALL(*env, NewGlobalRef(_)).WillRepeatedly(Return(nullptr));
    EXPECT_CALL(*env, DeleteLocalRef(_)).WillRepeatedly(Return());
    EXPECT_CALL(*env, GetStaticMethodID(_, _, _)).WillRepeatedly(Return(nullptr));
    EXPECT_CALL(*env, GetStaticFieldID(_, _, _)).WillRepeatedly(Return(nullptr));

    // for EvaluateHiveUdfSingle
    EXPECT_CALL(*env, NewStringUTF(_)).WillOnce(Return(jstring("AddIntUDF")));
    EXPECT_CALL(*env, NewObjectArray(vecCount, _, _)).WillOnce(Return(nullptr));
    EXPECT_CALL(*env, GetStaticObjectField(_, _)).WillRepeatedly(Return(nullptr));
    EXPECT_CALL(*env, SetObjectArrayElement(_, _, _)).WillRepeatedly(Return());

    InAndOutputInfos infos {};
    EXPECT_CALL(*env, CallStaticVoidMethodV(_, _, infos))
        .WillOnce(DoAll(Assign((int32_t *)(&outputValue), 8), Assign((int8_t *)(&outputNull), 0)))
        .WillRepeatedly(Return());

    EvaluateHiveUdfSingle(contextPtr, "omniruntime.udf.AddIntUDF", inputTypes, retType, vecCount,
        reinterpret_cast<int64_t>(inputValue), reinterpret_cast<int64_t>(inputNull), inputLength,
        reinterpret_cast<int64_t>(&outputValue), reinterpret_cast<int64_t>(&outputNull),
        reinterpret_cast<int64_t>(&outputLength));

    ASSERT_EQ(outputValue, 8);
    ASSERT_EQ(outputNull, 0);

    delete context;
    DestroyJNIEnvMock(env);
}

// date time functions
#ifndef EXCLUDE_DUPLICATE_CODEGEN_FUNCTIONS
TEST(FunctionTest, UnixTimestampFromStr)
{
    const int32_t rowCnt = 6;
    std::string timeStrs[] = {"2024-10-12", "1948-01-12", "2023-12-09", "",
                              "1989-07-10 11:10:09", "1985-06-29 00:04:49"};
    std::string fmtStrs[] = {"%Y-%m-%d", "%Y-%m-%d", "%Y-%m-%d", "", "%Y-%m-%d %H:%M:%S", "%Y-%m-%d %H:%M:%S"};
    bool isNullTimeStr[] = {false, false, false, true, false, false};
    bool isNullFmtStr[] = {false, false, false, true, false, false};
    bool* retIsNull[rowCnt];
    for (int i = 0; i < rowCnt; ++i) {
        retIsNull[i] = new bool(false);
    }
    int64_t output[rowCnt];
    for (int32_t i = 0; i < rowCnt; i++) {
        output[i] = UnixTimestampFromStr(timeStrs[i].c_str(), timeStrs[i].length(), isNullTimeStr[i],
                                         fmtStrs[i].c_str(), fmtStrs[i].length(), isNullFmtStr[i],
                                         "Asia/Shanghai", 13, false, "CORRECTED", 9, false, retIsNull[i]);
    }
    std::vector<bool> expectIsNull = {false, false, false, true, false, false};
    bool resultIsNull[rowCnt];
    for (int32_t i = 0; i < rowCnt; i++) {
        resultIsNull[i] = *retIsNull[i];
    }
    TestUtil::AssertBoolEquals(expectIsNull, resultIsNull);
    std::vector<int64_t> result(output, output + rowCnt);
    std::vector<int64_t> expect = { 1728662400, -693388800, 1702051200, 0, 616039809, 488822689 };
    TestUtil::AssertLongEquals(expect, result);
    for (int i = 0; i < rowCnt; ++i) {
    delete retIsNull[i];
    }
}

TEST(FunctionTest, UnixTimestampFromDate)
{
    const int32_t rowCnt = 3;
    int32_t dates[] = {7130, 5658, 0};
    std::string fmtStrs[] = {"%Y-%m-%d", "%Y-%m-%d", "%Y-%m-%d"};
    bool isNull[] = {false, false, false};
    int64_t output[rowCnt];
    for (int32_t i = 0; i < rowCnt; i++) {
        output[i] = UnixTimestampFromDate(dates[i], fmtStrs[i].c_str(), fmtStrs[i].length(),
                                          "Asia/Shanghai", 13, "CORRECTED", 9, isNull[i]);
    }
    std::vector<int64_t> result(output, output + rowCnt);
    std::vector<int64_t> expect = { 615999600, 488822400, -28800 };
    TestUtil::AssertLongEquals(expect, result);
}

TEST(FunctionTest, FromUnixTimeRetNull)
{
    auto context = new ExecutionContext();
    int64_t contextPtr = reinterpret_cast<int64_t>(context);
    bool isNull = false;
    std::string fmtStr = "%Y-%m-%d %H:%M:%S";
    std::string tzStr = "Asia/Shanghai";
    int32_t outlen = 0;
    char *result = nullptr;
    std::string actual;

    result = FromUnixTimeRetNull(contextPtr, &isNull, 615999600, fmtStr.c_str(), fmtStr.length(),
                                 tzStr.c_str(), tzStr.length(), &outlen);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "1989-07-10 00:00:00");
    EXPECT_EQ(outlen, 19);

    result = FromUnixTimeRetNull(contextPtr, &isNull, 488822400, fmtStr.c_str(), fmtStr.length(),
                                 tzStr.c_str(), tzStr.length(), &outlen);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "1985-06-29 00:00:00");
    EXPECT_EQ(outlen, 19);

    result = FromUnixTimeRetNull(contextPtr, &isNull, 0, fmtStr.c_str(), fmtStr.length(),
                                 tzStr.c_str(), tzStr.length(), &outlen);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "1970-01-01 08:00:00");
    EXPECT_EQ(outlen, 19);

    result = FromUnixTimeRetNull(contextPtr, &isNull, -100, fmtStr.c_str(), fmtStr.length(),
                                 tzStr.c_str(), tzStr.length(), &outlen);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "1970-01-01 07:58:20");
    EXPECT_EQ(outlen, 19);
    delete context;
}

TEST(FunctionTest, FromUnixTimeWithoutTz)
{
    auto context = new ExecutionContext();
    int64_t contextPtr = reinterpret_cast<int64_t>(context);
    int32_t outlen = 0;
    std::string actual;
    std::string format;
    char *result = nullptr;

    int64_t timestamp = 1740484215000;

    format = "%y-%m-%d %H:%M:%S";
    result = FromUnixTimeWithoutTz(contextPtr, timestamp, format.c_str(), format.length(), false, &outlen);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "25-02-25 11:50:15");
    EXPECT_EQ(outlen, 17);

    format = "%Y-%m";
    result = FromUnixTimeWithoutTz(contextPtr, timestamp, format.c_str(), format.length(), false, &outlen);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "2025-02");
    EXPECT_EQ(outlen, 7);

    format = "%H:%M";
    result = FromUnixTimeWithoutTz(contextPtr, timestamp, format.c_str(), format.length(), false, &outlen);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "11:50");
    EXPECT_EQ(outlen, 5);

    format = "%m-%H";
    result = FromUnixTimeWithoutTz(contextPtr, timestamp, format.c_str(), format.length(), false, &outlen);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "02-11");
    EXPECT_EQ(outlen, 5);

    delete context;
}

TEST(FunctionTest, DateFormat)
{
    auto context = new ExecutionContext();
    int64_t contextPtr = reinterpret_cast<int64_t>(context);
    bool isNull = false;
    std::string fmtStr = "yyyy-MM-dd";
    int32_t outlen = 0;
    char *result = nullptr;
    std::string actual;

    result = DateFormat(contextPtr, 0, fmtStr.c_str(), fmtStr.length(), isNull, &outlen);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "1970-01-01");
    EXPECT_EQ(outlen, 10);

    result = DateFormat(contextPtr, 1767196799000000, fmtStr.c_str(), fmtStr.length(), isNull, &outlen);
    actual = std::string(result, outlen);
    EXPECT_EQ(actual, "2025-12-31");
    EXPECT_EQ(outlen, 10);

    result = DateFormat(contextPtr, 1767196799000000, "yy-MM-dd", 8, isNull, &outlen);
    EXPECT_EQ(result, nullptr);
    EXPECT_EQ(outlen, 0);

    result = DateFormat(contextPtr, 0, nullptr, 0, true, &outlen);
    EXPECT_EQ(result, nullptr);
    EXPECT_EQ(outlen, 0);

    delete context;
}
#endif

TEST(FunctionTest, RegexMatch)
{
    std::string input("");
    std::string pattern(R"(^\d+$)");
    auto result = RegexMatch(input.c_str(), input.length(), pattern.c_str(), pattern.length(), false);
    EXPECT_FALSE(result);

    input = std::string("");
    pattern = std::string("");
    result = RegexMatch(input.c_str(), input.length(), pattern.c_str(), pattern.length(), false);
    EXPECT_TRUE(result);

    input = std::string("123");
    pattern = std::string("");
    result = RegexMatch(input.c_str(), input.length(), pattern.c_str(), pattern.length(), false);
    EXPECT_TRUE(result);

    input = std::string("123");
    pattern = std::string(R"(^\d+$)");
    result = RegexMatch(input.c_str(), input.length(), pattern.c_str(), pattern.length(), false);
    EXPECT_TRUE(result);

    input = std::string("abc123d");
    pattern = std::string(R"(^\d+$)");
    result = RegexMatch(input.c_str(), input.length(), pattern.c_str(), pattern.length(), false);
    EXPECT_FALSE(result);

    input = std::string("abc123");
    pattern = std::string(R"(^\d+$)");
    result = RegexMatch(input.c_str(), input.length(), pattern.c_str(), pattern.length(), false);
    EXPECT_FALSE(result);

    input = std::string("123d");
    pattern = std::string(R"(^\d+$)");
    result = RegexMatch(input.c_str(), input.length(), pattern.c_str(), pattern.length(), false);
    EXPECT_FALSE(result);
}

#ifndef EXCLUDE_DUPLICATE_CODEGEN_FUNCTIONS
TEST(FunctionTest, Date32Trunc)
{
    Date32TruncTest("0086-03-14", "YEAR", "0086-01-01", false);
    Date32TruncTest("0987-12-27", "YEAR", "0987-01-01", false);

    Date32TruncTest("0086-03-14", "MONTH", "0086-03-01", false);
    Date32TruncTest("0987-12-27", "MONTH", "0987-12-01", false);

    Date32TruncTest("0086-03-14", "QUARTER", "0086-01-01", false);
    Date32TruncTest("0987-12-27", "QUARTER", "0987-10-01", false);

    Date32TruncTest("0086-03-14", "WEeK", "0086-03-11", false);
    Date32TruncTest("0987-12-27", "WEeK", "0987-12-24", false);

    Date32TruncTest("0086-03-14", "ww", "0086-03-11", true);
    Date32TruncTest("0987-12-27", "weeek", "0987-12-24", true);
}
#endif

TEST(FunctionTest, ContainsStr)
{
    std::string src = std::string("abc");
    std::string match = std::string("abc");
    bool result = ContainsStr(src.c_str(), src.length(), match.c_str(), match.length(), true);
    EXPECT_EQ(result, false);

    src = std::string("");
    match = std::string("abc");
    result = ContainsStr(src.c_str(), src.length(), match.c_str(), match.length(), false);
    EXPECT_EQ(result, false);

    src = std::string("abc");
    match = std::string("abd");
    result = ContainsStr(src.c_str(), src.length(), match.c_str(), match.length(), false);
    EXPECT_EQ(result, false);

    src = std::string("abcd");
    match = std::string("abd");
    result = ContainsStr(src.c_str(), src.length(), match.c_str(), match.length(), false);
    EXPECT_EQ(result, false);

    src = std::string("abcd");
    match = std::string("acd");
    result = ContainsStr(src.c_str(), src.length(), match.c_str(), match.length(), false);
    EXPECT_EQ(result, false);

    src = std::string("");
    match = std::string("");
    result = ContainsStr(src.c_str(), src.length(), match.c_str(), match.length(), false);
    EXPECT_EQ(result, true);

    src = std::string("abc");
    match = std::string("");
    result = ContainsStr(src.c_str(), src.length(), match.c_str(), match.length(), false);
    EXPECT_EQ(result, true);

    src = std::string("abc");
    match = std::string("abc");
    result = ContainsStr(src.c_str(), src.length(), match.c_str(), match.length(), false);
    EXPECT_EQ(result, true);

    src = std::string("abcd");
    match = std::string("abc");
    result = ContainsStr(src.c_str(), src.length(), match.c_str(), match.length(), false);
    EXPECT_EQ(result, true);

    src = std::string("abcd");
    match = std::string("bcd");
    result = ContainsStr(src.c_str(), src.length(), match.c_str(), match.length(), false);
    EXPECT_EQ(result, true);

    src = std::string("abcde");
    match = std::string("bcd");
    result = ContainsStr(src.c_str(), src.length(), match.c_str(), match.length(), false);
    EXPECT_EQ(result, true);
}

TEST(FunctionTest, GreatestStr)
{
    const char* lValue = "abc";
    const char* rValue = "abcd";
    bool retIsNull = false;
    int32_t outLen = 0;
    const char* result = GreatestStr(lValue, strlen(lValue), false, rValue, strlen(rValue), false, &retIsNull, &outLen);
    EXPECT_EQ(result, rValue);
    EXPECT_EQ(outLen, strlen(rValue));
    EXPECT_FALSE(retIsNull);

    lValue = "abcd";
    rValue = "abc";
    retIsNull = false;
    outLen = 0;
    result = GreatestStr(lValue, strlen(lValue), false, rValue, strlen(rValue), false, &retIsNull, &outLen);
    EXPECT_EQ(result, lValue);
    EXPECT_EQ(outLen, strlen(lValue));
    EXPECT_FALSE(retIsNull);

    lValue = "abc";
    rValue = "";
    retIsNull = false;
    outLen = 0;
    result = GreatestStr(lValue, strlen(lValue), false, rValue, strlen(rValue), false, &retIsNull, &outLen);
    EXPECT_EQ(result, lValue);
    EXPECT_EQ(outLen, strlen(lValue));
    EXPECT_FALSE(retIsNull);

    lValue = "";
    rValue = "abc";
    retIsNull = false;
    outLen = 0;
    result = GreatestStr(lValue, strlen(lValue), false, rValue, strlen(rValue), false, &retIsNull, &outLen);
    EXPECT_EQ(result, rValue);
    EXPECT_EQ(outLen, strlen(rValue));
    EXPECT_FALSE(retIsNull);

    lValue = "";
    rValue = "";
    retIsNull = false;
    outLen = 0;
    result = GreatestStr(lValue, strlen(lValue), false, rValue, strlen(rValue), false, &retIsNull, &outLen);
    EXPECT_EQ(result, lValue);
    EXPECT_EQ(outLen, strlen(lValue));
    EXPECT_FALSE(retIsNull);

    lValue = "abc";
    rValue = nullptr;
    retIsNull = false;
    outLen = 0;
    result = GreatestStr(lValue, strlen(lValue), false, rValue, 0, true, &retIsNull, &outLen);
    EXPECT_EQ(result, lValue);
    EXPECT_EQ(outLen, strlen(lValue));
    EXPECT_FALSE(retIsNull);

    lValue = nullptr;
    rValue = "abc";
    retIsNull = false;
    outLen = 0;
    result = GreatestStr(lValue, 0, true, rValue, strlen(rValue), false, &retIsNull, &outLen);
    EXPECT_EQ(result, rValue);
    EXPECT_EQ(outLen, strlen(rValue));
    EXPECT_FALSE(retIsNull);

    lValue = nullptr;
    rValue = nullptr;
    retIsNull = false;
    outLen = 0;
    result = GreatestStr(lValue, 0, true, rValue, 0, true, &retIsNull, &outLen);
    EXPECT_EQ(result, nullptr);
    EXPECT_EQ(outLen, 0);
    EXPECT_TRUE(retIsNull);

    lValue = "1234";
    rValue = "2";
    retIsNull = false;
    outLen = 0;
    result = GreatestStr(lValue, strlen(lValue), false, rValue, strlen(rValue), false, &retIsNull, &outLen);
    EXPECT_EQ(result, rValue);
    EXPECT_EQ(outLen, strlen(rValue));
    EXPECT_FALSE(retIsNull);
}

TEST(FunctionTest, Greatest)
{
    int32_t lInt32 = 10;
    int32_t rInt32 = 5;
    bool retIsNull = false;
    auto resultInt32 = Greatest<int32_t>(lInt32, false, rInt32, false, &retIsNull);
    EXPECT_EQ(resultInt32, lInt32);
    EXPECT_FALSE(retIsNull);

    lInt32 = 5;
    rInt32 = 10;
    retIsNull = false;
    resultInt32 = Greatest<int32_t>(lInt32, false, rInt32, false, &retIsNull);
    EXPECT_EQ(resultInt32, rInt32);
    EXPECT_FALSE(retIsNull);

    lInt32 = 0;
    rInt32 = 10;
    retIsNull = false;
    resultInt32 = Greatest<int32_t>(lInt32, true, rInt32, false, &retIsNull);
    EXPECT_EQ(resultInt32, rInt32);
    EXPECT_FALSE(retIsNull);

    lInt32 = 10;
    rInt32 = 0;
    retIsNull = false;
    resultInt32 = Greatest<int32_t>(lInt32, false, rInt32, true, &retIsNull);
    EXPECT_EQ(resultInt32, lInt32);
    EXPECT_FALSE(retIsNull);

    lInt32 = 0;
    rInt32 = 0;
    retIsNull = false;
    resultInt32 = Greatest<int32_t>(lInt32, true, rInt32, true, &retIsNull);
    EXPECT_EQ(resultInt32, 0);
    EXPECT_TRUE(retIsNull);

    int64_t lInt64 = 10;
    int64_t rInt64 = 5;
    retIsNull = false;
    auto resultInt64 = Greatest<int64_t>(lInt64, false, rInt64, false, &retIsNull);
    EXPECT_EQ(resultInt64, lInt64);
    EXPECT_FALSE(retIsNull);

    bool lBool = true;
    bool rBool = false;
    retIsNull = false;
    auto resultBool = Greatest<bool>(lBool, false, rBool, false, &retIsNull);
    EXPECT_EQ(resultBool, lBool);
    EXPECT_FALSE(retIsNull);

    double lDouble = 10.00;
    double rDouble = 5.00;
    retIsNull = false;
    auto resultDouble = Greatest<double>(lDouble, false, rDouble, false, &retIsNull);
    EXPECT_EQ(resultDouble, lDouble);
    EXPECT_FALSE(retIsNull);
}

TEST(FunctionTest, StaticInvokeVarcharTypeWriteSideCheck)
{
    auto context = new ExecutionContext();
    int64_t contextPtr = reinterpret_cast<int64_t>(context);
    std::string src = "abc";
    int outLen;
    const char* cs1 = StaticInvokeVarcharTypeWriteSideCheck(contextPtr, src.c_str(), src.size(), 4, false, &outLen);
    std::string ss1(cs1, outLen);
    EXPECT_EQ(ss1, src);

    const char* cs2 = StaticInvokeVarcharTypeWriteSideCheck(contextPtr, src.c_str(), src.size(), 2, false, &outLen);
    EXPECT_TRUE(cs2 == nullptr);

    src = "abc   ";
    const char* cs3 = StaticInvokeVarcharTypeWriteSideCheck(contextPtr, src.c_str(), src.size(), 4, false, &outLen);
    std::string ss3(cs3, outLen);
    EXPECT_EQ(ss3, "abc ");

    const char* cs4 = StaticInvokeVarcharTypeWriteSideCheck(contextPtr, src.c_str(), src.size(), 2, false, &outLen);
    EXPECT_TRUE(cs4 == nullptr);

    src = "你好";
    const char* cs5 = StaticInvokeVarcharTypeWriteSideCheck(contextPtr, src.c_str(), src.size(), 2, false, &outLen);
    std::string ss5(cs5, outLen);
    EXPECT_EQ(ss5, src);

    const char* cs6 = StaticInvokeVarcharTypeWriteSideCheck(contextPtr, src.c_str(), src.size(), 1, false, &outLen);
    EXPECT_TRUE(cs6 == nullptr);

    const char* cs7 = StaticInvokeVarcharTypeWriteSideCheck(contextPtr, nullptr, 0, 1, true, &outLen);
    EXPECT_TRUE(cs7 == nullptr);
    delete context;
}

TEST(FunctionTest, StaticInvokeCharReadPadding)
{
    auto context = new ExecutionContext();
    int64_t contextPtr = reinterpret_cast<int64_t>(context);
    std::string src = "abc";
    int outLen;
    const char* cs1 = StaticInvokeCharReadPadding(contextPtr, src.c_str(), src.size(), 2, false, &outLen);
    std::string ss1(cs1, outLen);
    EXPECT_EQ(ss1, src);

    const char* cs2 = StaticInvokeCharReadPadding(contextPtr, src.c_str(), src.size(), src.size(), false, &outLen);
    std::string ss2(cs2, outLen);
    EXPECT_EQ(ss2, src);

    const char* cs3 = StaticInvokeCharReadPadding(contextPtr, src.c_str(), src.size(), 6, false, &outLen);
    std::string ss3(cs3, outLen);
    EXPECT_EQ(ss3, "abc   ");

    src = "你好";
    const char* cs4 = StaticInvokeCharReadPadding(contextPtr, src.c_str(), src.size(), 1, false, &outLen);
    std::string ss4(cs4, outLen);
    EXPECT_EQ(ss4, src);

    const char* cs5 = StaticInvokeCharReadPadding(contextPtr, src.c_str(), src.size(), 2, false, &outLen);
    std::string ss5(cs5, outLen);
    EXPECT_EQ(ss5, src);

    const char* cs6 = StaticInvokeCharReadPadding(contextPtr, src.c_str(), src.size(), 4, false, &outLen);
    std::string ss6(cs6, outLen);
    EXPECT_EQ(ss6, "你好  ");

    const char* cs7 = StaticInvokeCharReadPadding(contextPtr, nullptr, 0, 4, true, &outLen);
    std::string ss7(cs7, outLen);
    EXPECT_TRUE(cs7 == nullptr);
    delete context;
}

std::string RegexpReplaceTest(const std::string &stringInput, const std::string &pattern,
    const std::string &replacement, int32_t position)
{
    auto context = new ExecutionContext();
    auto contextPtr = reinterpret_cast<int64_t>(context);
    int32_t outLen = 0;
    auto res = RegexpReplace(contextPtr, stringInput.data(), stringInput.size(), pattern.data(), pattern.size(),
        replacement.data(), replacement.size(), position, false, &outLen);
    auto temp = std::string(res, static_cast<size_t>(outLen));
    delete context;
    return temp;
}

TEST(FunctionTest, RegexpReplace)
{
    EXPECT_EQ(RegexpReplaceTest("abc123xyz", "[0-9]", "X", 1), "abcXXXxyz");
    EXPECT_EQ(RegexpReplaceTest("abc123xyz", "[A-Za-z]", "", 1), "123");
    EXPECT_EQ(RegexpReplaceTest("abc123xyz", "$2-$1", "", 1), "abc123xyz");
}

#ifndef EXCLUDE_DUPLICATE_CODEGEN_FUNCTIONS
TEST(FunctionTest, GetJsonObject)
{
    auto context = new ExecutionContext();
    int64_t contextPtr = reinterpret_cast<int64_t>(context);

    auto expectNull = [&](const char *jsonStr, int32_t jsonLen, bool jsonIsNull, const char *pathStr, int32_t pathLen,
                          bool pathIsNull) {
        bool retIsNull = false;
        int32_t outLen = -1;
        const char *ret = GetJsonObject(contextPtr, jsonStr, jsonLen, jsonIsNull, pathStr, pathLen, pathIsNull,
            &retIsNull, &outLen);
        EXPECT_TRUE(retIsNull);
        EXPECT_EQ(ret, nullptr);
        EXPECT_EQ(outLen, 0);
    };

    auto expectValue = [&](const char *jsonStr, int32_t jsonLen, const char *pathStr, int32_t pathLen,
                           const std::string &expect) {
        bool retIsNull = true;
        int32_t outLen = 0;
        const char *ret = GetJsonObject(contextPtr, jsonStr, jsonLen, false, pathStr, pathLen, false, &retIsNull, &outLen);
        EXPECT_FALSE(retIsNull);
        ASSERT_NE(ret, nullptr);
        EXPECT_EQ(outLen, static_cast<int32_t>(expect.size()));
        EXPECT_EQ(std::string(ret, outLen), expect);
    };

    expectNull(nullptr, 0, true, "$.a", 3, false);
    expectNull("{\"a\":1}", 7, false, nullptr, 0, true);
    expectNull(nullptr, 0, false, "$.a", 3, false);
    expectNull("{\"a\":1}", 0, false, "$.a", 3, false);
    expectNull("{\"a\":1}", 7, false, "$.a", 0, false);

    expectNull("{\"a\":1}", 7, false, "a", 1, false);
    expectNull("{\"a\":1}", 7, false, "$.", 2, false);
    expectNull("{\"a\":1}", 7, false, "$[", 2, false);

    expectNull("{", 1, false, "$.a", 3, false);

    expectNull("{\"a\":1}", 7, false, "$.b", 3, false);
    expectNull("{\"a\":1}", 7, false, "$.a.b", 5, false);

    expectValue("{\"a\":\"x\"}", 9, "$.a", 3, "x");
    expectValue("{\"a\":20}", 8, "$.a", 3, "20");
    expectValue("{\"a\":1}", 7, "$.a", 3, "1");
    expectValue("{\"a\":{\"b\":1}}", 13, "$.a", 3, "{\"b\":1}");
    expectValue("{\"a\":[1,2]}", 11, "$.a", 3, "[1,2]");
    expectValue("{\"a\":[10,20]}", 13, "$.a[1]", 6, "20");
    expectNull("{\"a\":[1]}", 9, false, "$.a[2]", 6, false);
    expectValue("{\"a\":[{\"b\":\"c\"}]}", 17, "$.a[0].b", 8, "c");

    expectValue("{\"a\":true}", 10, "$.a", 3, "true");

    expectValue("{\"a\":1}", 7, "$", 1, "{\"a\":1}");

    expectNull("{\"a\":null}", 10, false, "$.a", 3, false);

    expectValue("{\"a\":1}", 7, "$['a']", 6, "1");

    expectValue("{\"a\":{\"b\":{\"c\":1}}}", 19, "$.a.b", 5, "{\"c\":1}");
    expectValue("{\"a\":[[1,2],[3,4]]}", 19, "$.a[1][0]", 9, "3");
    expectValue("{\"a\":{\"b\":[{\"c\":null},{\"c\":\"d\"}]}}", 35, "$.a.b[1].c", 10, "d");
    expectNull("{\"a\":{\"b\":[{\"c\":null},{\"c\":\"d\"}]}}", 35, false, "$.a.b[0].c", 10, false);
    expectValue("{\"a\":{\"b\":\"c\"}}", 16, "$['a']['b']", 11, "c");
    expectValue("[{\"a\":1},{\"a\":2}]", 17, "$[0].a", 6, "1");
    expectValue("[{\"a\":1},{\"a\":2}]", 17, "$[1].a", 6, "2");
    expectValue("[{\"a\":1},{\"a\":2}]", 17, "$[0]", 4, "{\"a\":1}");
    expectNull("[{\"a\":1},{\"a\":2}]", 17, false, "$[2].a", 6, false);

    delete context;
}
#endif
}
