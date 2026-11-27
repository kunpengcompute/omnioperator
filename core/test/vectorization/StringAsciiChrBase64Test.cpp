/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: Unit tests for string functions ascii, chr, char, base64, unbase64
 */

#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/ExprEval.h"
#include "expression/expressions.h"
#include "vectorization/registration/SimpleFunctionRegistry.h"
#include "vector/vector_helper.h"
#include "type/data_type.h"
#include "codegen/func_signature.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::codegen;

class StringAsciiChrBase64Test : public ::testing::Test {
protected:
    void SetUp() override {
        RegisterFunctions::Register();
    }
};

// ---- ascii(string) -> int32 ----
// SetValue(int, std::string_view &) requires lvalue; use named string_view vars.
TEST_F(StringAsciiChrBase64Test, AsciiBasic) {
    std::string s0 = "", s1 = " ", s2 = "A", s3 = "VELOX", s4 = "\xE5\x93\x88";  // U+54C8 in UTF-8
    constexpr int rowSize = 5;
    vec::BaseVector* inputVec = VectorHelper::CreateStringVector(rowSize);
    inputVec->SetIsField(true);  // caller-owned, do not let Reader delete
    auto* strVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(inputVec);
    ASSERT_NE(strVec, nullptr);
    std::string_view sv0(s0), sv1(s1), sv2(s2), sv3(s3), sv4(s4);
    strVec->SetValue(0, sv0);  // ""
    strVec->SetValue(1, sv1);  // " "
    strVec->SetValue(2, sv2);  // "A"
    strVec->SetValue(3, sv3);  // "VELOX"
    strVec->SetValue(4, sv4);  // U+54C8 (first UTF-8 codepoint) 

    auto signature = std::make_shared<FunctionSignature>("ascii",
        std::vector<DataTypeId>{OMNI_VARCHAR}, OMNI_INT);
    auto function = VectorFunction::Find(signature);
    ASSERT_NE(function, nullptr);

    vec::BaseVector* resultVector = nullptr;
    auto intType = std::make_shared<DataType>(OMNI_INT);
    ExecutionContext context(4096, nullptr);
    context.SetResultRowSize(rowSize);
    std::stack<vec::BaseVector*> args;
    args.push(inputVec);

    ASSERT_NO_THROW(function->Apply(args, intType, resultVector, &context));

    auto* intVec = dynamic_cast<Vector<int32_t>*>(resultVector);
    ASSERT_NE(intVec, nullptr);
    EXPECT_EQ(intVec->GetValue(0), 0);    // empty -> 0
    EXPECT_EQ(intVec->GetValue(1), 32);   // space
    EXPECT_EQ(intVec->GetValue(2), 65);   // 'A'
    EXPECT_EQ(intVec->GetValue(3), 86);   // 'V' (first char of VELOX)
    EXPECT_EQ(intVec->GetValue(4), 21704); // U+54C8

    delete resultVector;
    delete inputVec;
}

// ---- chr(n) -> string ----
TEST_F(StringAsciiChrBase64Test, ChrBasic) {
    int rowSize = 5;
    vec::BaseVector* inputVec = VectorHelper::CreateFlatVector(nullptr, OMNI_LONG, rowSize);
    inputVec->SetIsField(true);  // caller-owned, do not let FlatVectorReader delete
    auto* longVec = dynamic_cast<Vector<int64_t>*>(inputVec);
    ASSERT_NE(longVec, nullptr);
    longVec->SetValue(0, static_cast<int64_t>(-16));
    longVec->SetValue(1, static_cast<int64_t>(0));
    longVec->SetValue(2, static_cast<int64_t>(0x20));
    longVec->SetValue(3, static_cast<int64_t>(0x80));
    longVec->SetValue(4, static_cast<int64_t>(65));

    auto signature = std::make_shared<FunctionSignature>("chr",
        std::vector<DataTypeId>{OMNI_LONG}, OMNI_VARCHAR);
    auto function = VectorFunction::Find(signature);
    ASSERT_NE(function, nullptr);

    vec::BaseVector* resultVector = nullptr;
    auto varcharType = std::make_shared<DataType>(OMNI_VARCHAR);
    ExecutionContext context(4096, nullptr);
    context.SetResultRowSize(rowSize);
    std::stack<vec::BaseVector*> args;
    args.push(inputVec);

    ASSERT_NO_THROW(function->Apply(args, varcharType, resultVector, &context));

    auto* outStrVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(resultVector);
    ASSERT_NE(outStrVec, nullptr);
    EXPECT_EQ(std::string(outStrVec->GetValue(0)), "");           // n < 0 -> ""
    EXPECT_EQ(std::string(outStrVec->GetValue(1)), std::string("\0", 1)); // chr(0)
    EXPECT_EQ(std::string(outStrVec->GetValue(2)), " ");          // 0x20
    EXPECT_EQ(std::string(outStrVec->GetValue(3)), "\xC2\x80");    // U+80
    EXPECT_EQ(std::string(outStrVec->GetValue(4)), "A");         // 65

    delete resultVector;
    delete inputVec;
}

// ---- char(n) alias (same as chr, only OMNI_LONG per velox) ----
TEST_F(StringAsciiChrBase64Test, CharAlias) {
    int rowSize = 2;
    vec::BaseVector* inputVec = VectorHelper::CreateFlatVector(nullptr, OMNI_LONG, rowSize);
    inputVec->SetIsField(true);  // caller-owned, do not let FlatVectorReader delete
    auto* longVec = dynamic_cast<Vector<int64_t>*>(inputVec);
    ASSERT_NE(longVec, nullptr);
    longVec->SetValue(0, static_cast<int64_t>(97));
    longVec->SetValue(1, static_cast<int64_t>(49));

    auto signature = std::make_shared<FunctionSignature>("char",
        std::vector<DataTypeId>{OMNI_LONG}, OMNI_VARCHAR);
    auto function = VectorFunction::Find(signature);
    ASSERT_NE(function, nullptr);

    vec::BaseVector* resultVector = nullptr;
    auto varcharType = std::make_shared<DataType>(OMNI_VARCHAR);
    ExecutionContext context(4096, nullptr);
    context.SetResultRowSize(rowSize);
    std::stack<vec::BaseVector*> args;
    args.push(inputVec);

    ASSERT_NO_THROW(function->Apply(args, varcharType, resultVector, &context));

    auto* outStrVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(resultVector);
    ASSERT_NE(outStrVec, nullptr);
    EXPECT_EQ(std::string(outStrVec->GetValue(0)), "a");
    EXPECT_EQ(std::string(outStrVec->GetValue(1)), "1");

    delete resultVector;
    delete inputVec;
}

// ---- base64(binary) -> varchar ----
// Spark MIME encoding: CRLF every 76 output characters.
TEST_F(StringAsciiChrBase64Test, Base64Basic) {
    std::string b0 = "Man";
    std::string b1 = "hello world";
    std::string b2 = "Spark SQL";
    std::string b3 = "\x01";
    std::string b4(57, 'A');
    std::string b5(58, 'A');
    constexpr int rowSize = 6;
    vec::BaseVector* inputVec = VectorHelper::CreateStringVector(rowSize);
    inputVec->SetIsField(true);
    auto* strVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(inputVec);
    ASSERT_NE(strVec, nullptr);
    std::string_view sv0(b0), sv1(b1), sv2(b2), sv3(b3), sv4(b4), sv5(b5);
    strVec->SetValue(0, sv0);
    strVec->SetValue(1, sv1);
    strVec->SetValue(2, sv2);
    strVec->SetValue(3, sv3);
    strVec->SetValue(4, sv4);
    strVec->SetValue(5, sv5);

    auto signature = std::make_shared<FunctionSignature>("base64",
        std::vector<DataTypeId>{OMNI_VARBINARY}, OMNI_VARCHAR);
    auto function = VectorFunction::Find(signature);
    ASSERT_NE(function, nullptr);

    vec::BaseVector* resultVector = nullptr;
    auto varcharType = std::make_shared<DataType>(OMNI_VARCHAR);
    ExecutionContext context(4096, nullptr);
    context.SetResultRowSize(rowSize);
    std::stack<vec::BaseVector*> args;
    args.push(inputVec);

    ASSERT_NO_THROW(function->Apply(args, varcharType, resultVector, &context));

    auto* outStrVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(resultVector);
    ASSERT_NE(outStrVec, nullptr);
    EXPECT_EQ(std::string(outStrVec->GetValue(0)), "TWFu");
    EXPECT_EQ(std::string(outStrVec->GetValue(1)), "aGVsbG8gd29ybGQ=");
    EXPECT_EQ(std::string(outStrVec->GetValue(2)), "U3BhcmsgU1FM");
    EXPECT_EQ(std::string(outStrVec->GetValue(3)), "AQ==");
    // 57 bytes -> exactly 76 Base64 chars (one full MIME line, no CRLF)
    EXPECT_EQ(std::string(outStrVec->GetValue(4)),
        "QUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFB");
    // 58 bytes -> crosses MIME line boundary, includes CRLF
    EXPECT_EQ(std::string(outStrVec->GetValue(5)),
        "QUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFB\r\nQQ==");

    delete resultVector;
    delete inputVec;
}

// ---- base64: empty input ----
TEST_F(StringAsciiChrBase64Test, Base64Empty) {
    std::string b0;
    constexpr int rowSize = 1;
    vec::BaseVector* inputVec = VectorHelper::CreateStringVector(rowSize);
    inputVec->SetIsField(true);
    auto* strVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(inputVec);
    ASSERT_NE(strVec, nullptr);
    std::string_view sv0(b0);
    strVec->SetValue(0, sv0);

    auto signature = std::make_shared<FunctionSignature>("base64",
        std::vector<DataTypeId>{OMNI_VARBINARY}, OMNI_VARCHAR);
    auto function = VectorFunction::Find(signature);
    ASSERT_NE(function, nullptr);

    vec::BaseVector* resultVector = nullptr;
    auto varcharType = std::make_shared<DataType>(OMNI_VARCHAR);
    ExecutionContext context(4096, nullptr);
    context.SetResultRowSize(rowSize);
    std::stack<vec::BaseVector*> args;
    args.push(inputVec);

    ASSERT_NO_THROW(function->Apply(args, varcharType, resultVector, &context));

    auto* outStrVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(resultVector);
    ASSERT_NE(outStrVec, nullptr);
    EXPECT_EQ(std::string(outStrVec->GetValue(0)), "");

    delete resultVector;
    delete inputVec;
}

// ---- base64: binary data with special characters ----
TEST_F(StringAsciiChrBase64Test, Base64BinaryData) {
    std::string b0 = "\xff\xee";
    std::string b1 = std::string("\x00\x01\x02", 3);
    constexpr int rowSize = 2;
    vec::BaseVector* inputVec = VectorHelper::CreateStringVector(rowSize);
    inputVec->SetIsField(true);
    auto* strVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(inputVec);
    ASSERT_NE(strVec, nullptr);
    std::string_view sv0(b0), sv1(b1.data(), b1.size());
    strVec->SetValue(0, sv0);
    strVec->SetValue(1, sv1);

    auto signature = std::make_shared<FunctionSignature>("base64",
        std::vector<DataTypeId>{OMNI_VARBINARY}, OMNI_VARCHAR);
    auto function = VectorFunction::Find(signature);
    ASSERT_NE(function, nullptr);

    vec::BaseVector* resultVector = nullptr;
    auto varcharType = std::make_shared<DataType>(OMNI_VARCHAR);
    ExecutionContext context(4096, nullptr);
    context.SetResultRowSize(rowSize);
    std::stack<vec::BaseVector*> args;
    args.push(inputVec);

    ASSERT_NO_THROW(function->Apply(args, varcharType, resultVector, &context));

    auto* outStrVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(resultVector);
    ASSERT_NE(outStrVec, nullptr);
    EXPECT_EQ(std::string(outStrVec->GetValue(0)), "/+4=");
    EXPECT_EQ(std::string(outStrVec->GetValue(1)), "AAEC");

    delete resultVector;
    delete inputVec;
}

// ---- base64 + unbase64 roundtrip ----
TEST_F(StringAsciiChrBase64Test, Base64UnBase64Roundtrip) {
    std::string original = "Spark SQL roundtrip test!";
    constexpr int rowSize = 1;

    vec::BaseVector* inputVec = VectorHelper::CreateStringVector(rowSize);
    inputVec->SetIsField(true);
    auto* strVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(inputVec);
    ASSERT_NE(strVec, nullptr);
    std::string_view sv0(original);
    strVec->SetValue(0, sv0);

    auto base64Sig = std::make_shared<FunctionSignature>("base64",
        std::vector<DataTypeId>{OMNI_VARBINARY}, OMNI_VARCHAR);
    auto base64Func = VectorFunction::Find(base64Sig);
    ASSERT_NE(base64Func, nullptr);

    vec::BaseVector* encodedVector = nullptr;
    auto varcharType = std::make_shared<DataType>(OMNI_VARCHAR);
    ExecutionContext ctx1(4096, nullptr);
    ctx1.SetResultRowSize(rowSize);
    std::stack<vec::BaseVector*> encodeArgs;
    encodeArgs.push(inputVec);

    ASSERT_NO_THROW(base64Func->Apply(encodeArgs, varcharType, encodedVector, &ctx1));
    auto* encodedStrVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(encodedVector);
    ASSERT_NE(encodedStrVec, nullptr);
    EXPECT_EQ(std::string(encodedStrVec->GetValue(0)), "U3BhcmsgU1FMIHJvdW5kdHJpcCB0ZXN0IQ==");

    encodedVector->SetIsField(true);
    auto unbase64Sig = std::make_shared<FunctionSignature>("unbase64",
        std::vector<DataTypeId>{OMNI_VARCHAR}, OMNI_VARBINARY);
    auto unbase64Func = VectorFunction::Find(unbase64Sig);
    ASSERT_NE(unbase64Func, nullptr);

    vec::BaseVector* decodedVector = nullptr;
    auto varbinaryType = std::make_shared<DataType>(OMNI_VARBINARY);
    ExecutionContext ctx2(4096, nullptr);
    ctx2.SetResultRowSize(rowSize);
    std::stack<vec::BaseVector*> decodeArgs;
    decodeArgs.push(encodedVector);

    ASSERT_NO_THROW(unbase64Func->Apply(decodeArgs, varbinaryType, decodedVector, &ctx2));
    auto* decodedStrVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(decodedVector);
    ASSERT_NE(decodedStrVec, nullptr);
    EXPECT_EQ(std::string(decodedStrVec->GetValue(0)), original);

    delete decodedVector;
    delete encodedVector;
    delete inputVec;
}

// ---- unbase64(string) -> varbinary ----
TEST_F(StringAsciiChrBase64Test, Unbase64Basic) {
    std::string u0 = "TWFu";
    std::string u1 = "aGVsbG8gd29ybGQ=";
    std::string u2 = "U3BhcmsgU1FM";
    constexpr int rowSize = 3;
    vec::BaseVector* inputVec = VectorHelper::CreateStringVector(rowSize);
    inputVec->SetIsField(true);  // caller-owned, do not let Reader delete
    auto* strVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(inputVec);
    ASSERT_NE(strVec, nullptr);
    std::string_view uv0(u0), uv1(u1), uv2(u2);
    strVec->SetValue(0, uv0);  // "TWFu" (Base64 of "Man")
    strVec->SetValue(1, uv1);  // "aGVsbG8gd29ybGQ=" (Base64 of "hello world")
    strVec->SetValue(2, uv2);  // "U3BhcmsgU1FM" (Base64 of "Spark SQL")

    auto signature = std::make_shared<FunctionSignature>("unbase64",
        std::vector<DataTypeId>{OMNI_VARCHAR}, OMNI_VARBINARY);
    auto function = VectorFunction::Find(signature);
    ASSERT_NE(function, nullptr);

    vec::BaseVector* resultVector = nullptr;
    auto varbinaryType = std::make_shared<DataType>(OMNI_VARBINARY);
    ExecutionContext context(4096, nullptr);
    context.SetResultRowSize(rowSize);
    std::stack<vec::BaseVector*> args;
    args.push(inputVec);

    ASSERT_NO_THROW(function->Apply(args, varbinaryType, resultVector, &context));

    auto* outStrVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(resultVector);
    ASSERT_NE(outStrVec, nullptr);
    EXPECT_EQ(std::string(outStrVec->GetValue(0)), "Man");
    EXPECT_EQ(std::string(outStrVec->GetValue(1)), "hello world");
    EXPECT_EQ(std::string(outStrVec->GetValue(2)), "Spark SQL");

    delete resultVector;
    delete inputVec;
}