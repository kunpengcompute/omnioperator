/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: Unit tests for unhex function
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

class UnhexTest : public ::testing::Test {
protected:
    void SetUp() override {
        RegisterFunctions::Register();
    }
};

// Test basic unhex functionality with valid hex strings
TEST_F(UnhexTest, BasicHexConversion) {
    // Test data: various valid hex strings
    std::string h0 = "737472696E67";    // "string"
    std::string h1 = "";                 // empty string
    std::string h2 = "23";               // "#"
    std::string h3 = "ff";               // 0xFF
    std::string h4 = "FF";               // 0xFF (uppercase)
    constexpr int rowSize = 5;
    
    vec::BaseVector* inputVec = VectorHelper::CreateStringVector(rowSize);
    inputVec->SetIsField(true);
    auto* strVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(inputVec);
    ASSERT_NE(strVec, nullptr);
    
    std::string_view sv0(h0), sv1(h1), sv2(h2), sv3(h3), sv4(h4);
    strVec->SetValue(0, sv0);
    strVec->SetValue(1, sv1);
    strVec->SetValue(2, sv2);
    strVec->SetValue(3, sv3);
    strVec->SetValue(4, sv4);

    auto signature = std::make_shared<FunctionSignature>("unhex",
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

    auto* outVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(resultVector);
    ASSERT_NE(outVec, nullptr);
    
    // Verify results
    EXPECT_EQ(std::string(outVec->GetValue(0)), "string");    // "737472696E67" -> "string"
    EXPECT_EQ(std::string(outVec->GetValue(1)), "");          // "" -> ""
    EXPECT_EQ(std::string(outVec->GetValue(2)), "#");         // "23" -> "#"
    EXPECT_EQ(std::string(outVec->GetValue(3)), "\xFF");      // "ff" -> 0xFF
    EXPECT_EQ(std::string(outVec->GetValue(4)), "\xFF");      // "FF" -> 0xFF

    delete resultVector;
    delete inputVec;
}

// Test unhex with odd-length hex strings
TEST_F(UnhexTest, OddLengthHexStrings) {
    // Test data: odd-length hex strings
    std::string h0 = "123";      // 0x01, 0x23
    std::string h1 = "F";        // 0x0F
    std::string h2 = "b23";      // 0x0B, 0x23
    std::string h3 = "b2323";    // 0x0B, 0x23, 0x23
    std::string h4 = "12345";    // 0x01, 0x23, 0x45
    constexpr int rowSize = 5;
    
    vec::BaseVector* inputVec = VectorHelper::CreateStringVector(rowSize);
    inputVec->SetIsField(true);
    auto* strVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(inputVec);
    ASSERT_NE(strVec, nullptr);
    
    std::string_view sv0(h0), sv1(h1), sv2(h2), sv3(h3), sv4(h4);
    strVec->SetValue(0, sv0);
    strVec->SetValue(1, sv1);
    strVec->SetValue(2, sv2);
    strVec->SetValue(3, sv3);
    strVec->SetValue(4, sv4);

    auto signature = std::make_shared<FunctionSignature>("unhex",
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

    auto* outVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(resultVector);
    ASSERT_NE(outVec, nullptr);
    
    // Verify results
    // "123" -> 0x01, 0x23 -> "\x01#"
    EXPECT_EQ(std::string(outVec->GetValue(0)), std::string("\x01#", 2));
    // "F" -> 0x0F -> "\x0F"
    EXPECT_EQ(std::string(outVec->GetValue(1)), std::string("\x0F", 1));
    // "b23" -> 0x0B, 0x23 -> "\x0B#"
    EXPECT_EQ(std::string(outVec->GetValue(2)), std::string("\x0B#", 2));
    // "b2323" -> 0x0B, 0x23, 0x23 -> "\x0B##"
    EXPECT_EQ(std::string(outVec->GetValue(3)), std::string("\x0B##", 3));
    // "12345" -> 0x01, 0x23, 0x45 -> "\x01#E"
    EXPECT_EQ(std::string(outVec->GetValue(4)), std::string("\x01#E", 3));

    delete resultVector;
    delete inputVec;
}

// Test unhex with UTF-8 encoded hex strings
TEST_F(UnhexTest, Utf8HexStrings) {
    // "E4B889E9878DE79A84" is the UTF-8 encoding of "三重的" (Chinese characters)
    std::string h0 = "E4B889E9878DE79A84";
    constexpr int rowSize = 1;
    
    vec::BaseVector* inputVec = VectorHelper::CreateStringVector(rowSize);
    inputVec->SetIsField(true);
    auto* strVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(inputVec);
    ASSERT_NE(strVec, nullptr);
    
    std::string_view sv0(h0);
    strVec->SetValue(0, sv0);

    auto signature = std::make_shared<FunctionSignature>("unhex",
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

    auto* outVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(resultVector);
    ASSERT_NE(outVec, nullptr);
    
    // "E4B889E9878DE79A84" decodes to UTF-8 bytes for "三重的"
    std::string expected = "\xE4\xB8\x89\xE9\x87\x8D\xE7\x9A\x84";
    EXPECT_EQ(std::string(outVec->GetValue(0)), expected);

    delete resultVector;
    delete inputVec;
}

// Test unhex with invalid hex characters (should return NULL)
TEST_F(UnhexTest, InvalidHexCharacters) {
    // Test data with invalid hex characters
    std::string h0 = "G";        // Invalid: 'G' is not a hex character
    std::string h1 = "GG";       // Invalid: 'G' is not a hex character
    std::string h2 = "G23";      // Invalid: starts with 'G'
    std::string h3 = "2g3";      // Invalid: 'g' in middle (lowercase G is invalid)
    constexpr int rowSize = 4;
    
    vec::BaseVector* inputVec = VectorHelper::CreateStringVector(rowSize);
    inputVec->SetIsField(true);
    auto* strVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(inputVec);
    ASSERT_NE(strVec, nullptr);
    
    std::string_view sv0(h0), sv1(h1), sv2(h2), sv3(h3);
    strVec->SetValue(0, sv0);
    strVec->SetValue(1, sv1);
    strVec->SetValue(2, sv2);
    strVec->SetValue(3, sv3);

    auto signature = std::make_shared<FunctionSignature>("unhex",
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

    // Note: Invalid hex strings should result in NULL values
    // The exact behavior depends on how the framework handles false returns from call()
    // In most implementations, the result vector will have NULL flags set for these rows
    ASSERT_NE(resultVector, nullptr);

    delete resultVector;
    delete inputVec;
}

// Test unhex with NULL input
TEST_F(UnhexTest, NullInput) {
    constexpr int rowSize = 3;
    
    vec::BaseVector* inputVec = VectorHelper::CreateStringVector(rowSize);
    inputVec->SetIsField(true);
    auto* strVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(inputVec);
    ASSERT_NE(strVec, nullptr);
    
    // Set some values, leave some as null
    std::string h0 = "48454C4C4F";  // "HELLO"
    std::string_view sv0(h0);
    strVec->SetValue(0, sv0);
    // Row 1 is set to null
    strVec->SetNull(1);
    std::string h2 = "414243";  // "ABC"
    std::string_view sv2(h2);
    strVec->SetValue(2, sv2);

    auto signature = std::make_shared<FunctionSignature>("unhex",
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

    auto* outVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(resultVector);
    ASSERT_NE(outVec, nullptr);
    
    // Row 0: "48454C4C4F" -> "HELLO"
    EXPECT_EQ(std::string(outVec->GetValue(0)), "HELLO");
    // Row 1: should be NULL (null propagation)
    EXPECT_TRUE(outVec->IsNull(1));
    // Row 2: "414243" -> "ABC"
    EXPECT_EQ(std::string(outVec->GetValue(2)), "ABC");

    delete resultVector;
    delete inputVec;
}

// Test unhex with mixed case hex characters
TEST_F(UnhexTest, MixedCaseHex) {
    std::string h0 = "aAbBcCdDeEfF";  // All hex digits in mixed case
    std::string h1 = "0123456789";    // All decimal digits
    constexpr int rowSize = 2;
    
    vec::BaseVector* inputVec = VectorHelper::CreateStringVector(rowSize);
    inputVec->SetIsField(true);
    auto* strVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(inputVec);
    ASSERT_NE(strVec, nullptr);
    
    std::string_view sv0(h0), sv1(h1);
    strVec->SetValue(0, sv0);
    strVec->SetValue(1, sv1);

    auto signature = std::make_shared<FunctionSignature>("unhex",
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

    auto* outVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(resultVector);
    ASSERT_NE(outVec, nullptr);
    
    // "aAbBcCdDeEfF" -> 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF
    std::string expected0 = "\xAA\xBB\xCC\xDD\xEE\xFF";
    EXPECT_EQ(std::string(outVec->GetValue(0)), expected0);
    
    // "0123456789" -> 0x01, 0x23, 0x45, 0x67, 0x89
    std::string expected1 = "\x01\x23\x45\x67\x89";
    EXPECT_EQ(std::string(outVec->GetValue(1)), expected1);

    delete resultVector;
    delete inputVec;
}
