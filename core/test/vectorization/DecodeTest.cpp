/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Unit tests for decode(binary, charset) function
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

class DecodeTest : public ::testing::Test {
protected:
    void SetUp() override {
        RegisterFunctions::Register();
    }
};

// Helper: create a string/varbinary vector with given values
static Vector<LargeStringContainer<std::string_view>>* CreateStringVec(
    int rowSize, const std::string* values, const std::vector<int>& nullIndices = {})
{
    BaseVector* rawVec = VectorHelper::CreateStringVector(rowSize);
    rawVec->SetIsField(true);
    auto* vec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(rawVec);
    for (int i = 0; i < rowSize; ++i) {
        bool isNull = std::find(nullIndices.begin(), nullIndices.end(), i) != nullIndices.end();
        if (isNull) {
            vec->SetNull(i);
        } else {
            std::string_view sv(values[i]);
            vec->SetValue(i, sv);
        }
    }
    return vec;
}

// Helper: run decode function via VectorFunction::Find + Apply
static BaseVector* RunDecode(BaseVector* binaryVec, BaseVector* charsetVec, int rowSize)
{
    auto signature = std::make_shared<FunctionSignature>("decode",
        std::vector<DataTypeId>{OMNI_VARBINARY, OMNI_VARCHAR}, OMNI_VARCHAR);
    auto function = VectorFunction::Find(signature);
    if (!function) {
        return nullptr;
    }

    BaseVector* resultVector = nullptr;
    auto varcharType = std::make_shared<DataType>(OMNI_VARCHAR);
    ExecutionContext context;
    context.SetResultRowSize(rowSize);
    std::stack<BaseVector*> args;
    // Apply unpacks arguments from the stack top expecting the LAST parameter on top.
    // So push in ExprEval order (left to right): binary first, charset last (on top).
    args.push(binaryVec);
    args.push(charsetVec);

    function->Apply(args, varcharType, resultVector, &context);
    return resultVector;
}

// ============================================================================
// US-ASCII charset tests
// ============================================================================

TEST_F(DecodeTest, AsciiBasic) {
    constexpr int rowSize = 3;
    std::string binaryVals[rowSize] = {"Hello", "", "World"};
    std::string charsetVals[rowSize] = {"US-ASCII", "US-ASCII", "US-ASCII"};

    auto* binaryVec = CreateStringVec(rowSize, binaryVals);
    auto* charsetVec = CreateStringVec(rowSize, charsetVals);

    auto* result = RunDecode(binaryVec, charsetVec, rowSize);
    auto* outVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    ASSERT_NE(outVec, nullptr);

    EXPECT_EQ(std::string(outVec->GetValue(0)), "Hello");
    EXPECT_EQ(std::string(outVec->GetValue(1)), "");
    EXPECT_EQ(std::string(outVec->GetValue(2)), "World");

    delete result;
    delete binaryVec;
    delete charsetVec;
}

TEST_F(DecodeTest, AsciiInvalidByte) {
    constexpr int rowSize = 1;
    // 0x80 is not valid US-ASCII
    std::string binaryVals[rowSize] = {std::string("\x80", 1)};
    std::string charsetVals[rowSize] = {"US-ASCII"};

    auto* binaryVec = CreateStringVec(rowSize, binaryVals);
    auto* charsetVec = CreateStringVec(rowSize, charsetVals);

    auto* result = RunDecode(binaryVec, charsetVec, rowSize);
    auto* outVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    ASSERT_NE(outVec, nullptr);
    EXPECT_TRUE(outVec->IsNull(0));

    delete result;
    delete binaryVec;
    delete charsetVec;
}

TEST_F(DecodeTest, AsciiAlias) {
    constexpr int rowSize = 1;
    std::string binaryVals[rowSize] = {"Test"};
    std::string charsetVals[rowSize] = {"ASCII"};

    auto* binaryVec = CreateStringVec(rowSize, binaryVals);
    auto* charsetVec = CreateStringVec(rowSize, charsetVals);

    auto* result = RunDecode(binaryVec, charsetVec, rowSize);
    auto* outVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    ASSERT_NE(outVec, nullptr);
    EXPECT_EQ(std::string(outVec->GetValue(0)), "Test");

    delete result;
    delete binaryVec;
    delete charsetVec;
}

// ============================================================================
// ISO-8859-1 charset tests
// ============================================================================

TEST_F(DecodeTest, Iso88591Basic) {
    constexpr int rowSize = 2;
    // 0xC4 = 'Ä' (U+00C4), 0xE4 = 'ä' (U+00E4) in ISO-8859-1
    std::string binaryVals[rowSize] = {
        std::string("\xC4\xE4", 2),
        std::string("\xFF", 1)  // ÿ (U+00FF)
    };
    std::string charsetVals[rowSize] = {"ISO-8859-1", "ISO-8859-1"};

    auto* binaryVec = CreateStringVec(rowSize, binaryVals);
    auto* charsetVec = CreateStringVec(rowSize, charsetVals);

    auto* result = RunDecode(binaryVec, charsetVec, rowSize);
    auto* outVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    ASSERT_NE(outVec, nullptr);

    // 0xC4 -> U+00C4 -> UTF-8: 0xC3 0x84
    std::string expected0 = "\xC3\x84\xC3\xA4";
    EXPECT_EQ(std::string(outVec->GetValue(0)), expected0);
    // 0xFF -> U+00FF -> UTF-8: 0xC3 0xBF
    std::string expected1 = "\xC3\xBF";
    EXPECT_EQ(std::string(outVec->GetValue(1)), expected1);

    delete result;
    delete binaryVec;
    delete charsetVec;
}

TEST_F(DecodeTest, Iso88591Latin1Alias) {
    constexpr int rowSize = 1;
    std::string binaryVals[rowSize] = {std::string("\xE9", 1)}; // é in ISO-8859-1
    std::string charsetVals[rowSize] = {"LATIN1"};

    auto* binaryVec = CreateStringVec(rowSize, binaryVals);
    auto* charsetVec = CreateStringVec(rowSize, charsetVals);

    auto* result = RunDecode(binaryVec, charsetVec, rowSize);
    auto* outVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    ASSERT_NE(outVec, nullptr);
    // 0xE9 -> U+00E9 -> UTF-8: 0xC3 0xA9
    EXPECT_EQ(std::string(outVec->GetValue(0)), "\xC3\xA9");

    delete result;
    delete binaryVec;
    delete charsetVec;
}

// ============================================================================
// UTF-8 charset tests
// ============================================================================

TEST_F(DecodeTest, Utf8Basic) {
    constexpr int rowSize = 2;
    std::string binaryVals[rowSize] = {"Hello", "\xE4\xB8\xAD\xE6\x96\x87"}; // "中文" in UTF-8
    std::string charsetVals[rowSize] = {"UTF-8", "UTF-8"};

    auto* binaryVec = CreateStringVec(rowSize, binaryVals);
    auto* charsetVec = CreateStringVec(rowSize, charsetVals);

    auto* result = RunDecode(binaryVec, charsetVec, rowSize);
    auto* outVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    ASSERT_NE(outVec, nullptr);

    EXPECT_EQ(std::string(outVec->GetValue(0)), "Hello");
    EXPECT_EQ(std::string(outVec->GetValue(1)), "\xE4\xB8\xAD\xE6\x96\x87");

    delete result;
    delete binaryVec;
    delete charsetVec;
}

TEST_F(DecodeTest, Utf8InvalidSequence) {
    constexpr int rowSize = 2;
    std::string binaryVals[rowSize] = {
        std::string("\xFF", 1),       // Invalid UTF-8 lead byte
        std::string("\xC0\x80", 2)    // Overlong encoding of U+0000
    };
    std::string charsetVals[rowSize] = {"UTF-8", "UTF-8"};

    auto* binaryVec = CreateStringVec(rowSize, binaryVals);
    auto* charsetVec = CreateStringVec(rowSize, charsetVals);

    auto* result = RunDecode(binaryVec, charsetVec, rowSize);
    auto* outVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    ASSERT_NE(outVec, nullptr);

    EXPECT_TRUE(outVec->IsNull(0));
    EXPECT_TRUE(outVec->IsNull(1));

    delete result;
    delete binaryVec;
    delete charsetVec;
}

TEST_F(DecodeTest, Utf8CaseInsensitive) {
    constexpr int rowSize = 1;
    std::string binaryVals[rowSize] = {"Hi"};
    std::string charsetVals[rowSize] = {"utf-8"};

    auto* binaryVec = CreateStringVec(rowSize, binaryVals);
    auto* charsetVec = CreateStringVec(rowSize, charsetVals);

    auto* result = RunDecode(binaryVec, charsetVec, rowSize);
    auto* outVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    ASSERT_NE(outVec, nullptr);
    EXPECT_EQ(std::string(outVec->GetValue(0)), "Hi");

    delete result;
    delete binaryVec;
    delete charsetVec;
}

// ============================================================================
// UTF-16BE charset tests
// ============================================================================

TEST_F(DecodeTest, Utf16BEBasic) {
    constexpr int rowSize = 1;
    // "Hi" in UTF-16BE: H=0x0048, i=0x0069
    std::string binaryVals[rowSize] = {std::string("\x00\x48\x00\x69", 4)};
    std::string charsetVals[rowSize] = {"UTF-16BE"};

    auto* binaryVec = CreateStringVec(rowSize, binaryVals);
    auto* charsetVec = CreateStringVec(rowSize, charsetVals);

    auto* result = RunDecode(binaryVec, charsetVec, rowSize);
    auto* outVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    ASSERT_NE(outVec, nullptr);
    EXPECT_EQ(std::string(outVec->GetValue(0)), "Hi");

    delete result;
    delete binaryVec;
    delete charsetVec;
}

TEST_F(DecodeTest, Utf16BESurrogatePair) {
    constexpr int rowSize = 1;
    // U+1F600 (😀) in UTF-16BE: D83D DE00
    std::string binaryVals[rowSize] = {std::string("\xD8\x3D\xDE\x00", 4)};
    std::string charsetVals[rowSize] = {"UTF-16BE"};

    auto* binaryVec = CreateStringVec(rowSize, binaryVals);
    auto* charsetVec = CreateStringVec(rowSize, charsetVals);

    auto* result = RunDecode(binaryVec, charsetVec, rowSize);
    auto* outVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    ASSERT_NE(outVec, nullptr);
    // U+1F600 in UTF-8: F0 9F 98 80
    EXPECT_EQ(std::string(outVec->GetValue(0)), std::string("\xF0\x9F\x98\x80", 4));

    delete result;
    delete binaryVec;
    delete charsetVec;
}

TEST_F(DecodeTest, Utf16BEOddBytes) {
    constexpr int rowSize = 1;
    // 3 bytes is odd — invalid for UTF-16
    std::string binaryVals[rowSize] = {std::string("\x00\x48\x00", 3)};
    std::string charsetVals[rowSize] = {"UTF-16BE"};

    auto* binaryVec = CreateStringVec(rowSize, binaryVals);
    auto* charsetVec = CreateStringVec(rowSize, charsetVals);

    auto* result = RunDecode(binaryVec, charsetVec, rowSize);
    auto* outVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    ASSERT_NE(outVec, nullptr);
    EXPECT_TRUE(outVec->IsNull(0));

    delete result;
    delete binaryVec;
    delete charsetVec;
}

// ============================================================================
// UTF-16LE charset tests
// ============================================================================

TEST_F(DecodeTest, Utf16LEBasic) {
    constexpr int rowSize = 1;
    // "Hi" in UTF-16LE: H=0x4800, i=0x6900
    std::string binaryVals[rowSize] = {std::string("\x48\x00\x69\x00", 4)};
    std::string charsetVals[rowSize] = {"UTF-16LE"};

    auto* binaryVec = CreateStringVec(rowSize, binaryVals);
    auto* charsetVec = CreateStringVec(rowSize, charsetVals);

    auto* result = RunDecode(binaryVec, charsetVec, rowSize);
    auto* outVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    ASSERT_NE(outVec, nullptr);
    EXPECT_EQ(std::string(outVec->GetValue(0)), "Hi");

    delete result;
    delete binaryVec;
    delete charsetVec;
}

TEST_F(DecodeTest, Utf16LESurrogatePair) {
    constexpr int rowSize = 1;
    // U+1F600 (😀) in UTF-16LE: 3DD8 00DE
    std::string binaryVals[rowSize] = {std::string("\x3D\xD8\x00\xDE", 4)};
    std::string charsetVals[rowSize] = {"UTF-16LE"};

    auto* binaryVec = CreateStringVec(rowSize, binaryVals);
    auto* charsetVec = CreateStringVec(rowSize, charsetVals);

    auto* result = RunDecode(binaryVec, charsetVec, rowSize);
    auto* outVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    ASSERT_NE(outVec, nullptr);
    // U+1F600 in UTF-8: F0 9F 98 80
    EXPECT_EQ(std::string(outVec->GetValue(0)), std::string("\xF0\x9F\x98\x80", 4));

    delete result;
    delete binaryVec;
    delete charsetVec;
}

// ============================================================================
// UTF-16 (with BOM) charset tests
// ============================================================================

TEST_F(DecodeTest, Utf16BomBE) {
    constexpr int rowSize = 1;
    // BOM (FE FF) + "H" in UTF-16BE (00 48)
    std::string binaryVals[rowSize] = {std::string("\xFE\xFF\x00\x48", 4)};
    std::string charsetVals[rowSize] = {"UTF-16"};

    auto* binaryVec = CreateStringVec(rowSize, binaryVals);
    auto* charsetVec = CreateStringVec(rowSize, charsetVals);

    auto* result = RunDecode(binaryVec, charsetVec, rowSize);
    auto* outVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    ASSERT_NE(outVec, nullptr);
    EXPECT_EQ(std::string(outVec->GetValue(0)), "H");

    delete result;
    delete binaryVec;
    delete charsetVec;
}

TEST_F(DecodeTest, Utf16BomLE) {
    constexpr int rowSize = 1;
    // BOM (FF FE) + "H" in UTF-16LE (48 00)
    std::string binaryVals[rowSize] = {std::string("\xFF\xFE\x48\x00", 4)};
    std::string charsetVals[rowSize] = {"UTF-16"};

    auto* binaryVec = CreateStringVec(rowSize, binaryVals);
    auto* charsetVec = CreateStringVec(rowSize, charsetVals);

    auto* result = RunDecode(binaryVec, charsetVec, rowSize);
    auto* outVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    ASSERT_NE(outVec, nullptr);
    EXPECT_EQ(std::string(outVec->GetValue(0)), "H");

    delete result;
    delete binaryVec;
    delete charsetVec;
}

TEST_F(DecodeTest, Utf16NoBomDefaultsToBE) {
    constexpr int rowSize = 1;
    // No BOM, defaults to BE: "H" = 0x0048
    std::string binaryVals[rowSize] = {std::string("\x00\x48", 2)};
    std::string charsetVals[rowSize] = {"UTF-16"};

    auto* binaryVec = CreateStringVec(rowSize, binaryVals);
    auto* charsetVec = CreateStringVec(rowSize, charsetVals);

    auto* result = RunDecode(binaryVec, charsetVec, rowSize);
    auto* outVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    ASSERT_NE(outVec, nullptr);
    EXPECT_EQ(std::string(outVec->GetValue(0)), "H");

    delete result;
    delete binaryVec;
    delete charsetVec;
}

TEST_F(DecodeTest, Utf16Empty) {
    constexpr int rowSize = 1;
    std::string binaryVals[rowSize] = {""};
    std::string charsetVals[rowSize] = {"UTF-16"};

    auto* binaryVec = CreateStringVec(rowSize, binaryVals);
    auto* charsetVec = CreateStringVec(rowSize, charsetVals);

    auto* result = RunDecode(binaryVec, charsetVec, rowSize);
    auto* outVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    ASSERT_NE(outVec, nullptr);
    EXPECT_EQ(std::string(outVec->GetValue(0)), "");

    delete result;
    delete binaryVec;
    delete charsetVec;
}

// ============================================================================
// NULL handling tests
// ============================================================================

TEST_F(DecodeTest, NullBinary) {
    constexpr int rowSize = 2;
    std::string binaryVals[rowSize] = {"", "Hello"};
    std::string charsetVals[rowSize] = {"UTF-8", "UTF-8"};

    auto* binaryVec = CreateStringVec(rowSize, binaryVals, {0}); // Row 0 is NULL
    auto* charsetVec = CreateStringVec(rowSize, charsetVals);

    auto* result = RunDecode(binaryVec, charsetVec, rowSize);
    auto* outVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    ASSERT_NE(outVec, nullptr);
    EXPECT_TRUE(outVec->IsNull(0));
    EXPECT_EQ(std::string(outVec->GetValue(1)), "Hello");

    delete result;
    delete binaryVec;
    delete charsetVec;
}

TEST_F(DecodeTest, NullCharset) {
    constexpr int rowSize = 2;
    std::string binaryVals[rowSize] = {"Hello", "World"};
    std::string charsetVals[rowSize] = {"UTF-8", ""};

    auto* binaryVec = CreateStringVec(rowSize, binaryVals);
    auto* charsetVec = CreateStringVec(rowSize, charsetVals, {1}); // Row 1 is NULL

    auto* result = RunDecode(binaryVec, charsetVec, rowSize);
    auto* outVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    ASSERT_NE(outVec, nullptr);
    EXPECT_EQ(std::string(outVec->GetValue(0)), "Hello");
    EXPECT_TRUE(outVec->IsNull(1));

    delete result;
    delete binaryVec;
    delete charsetVec;
}

TEST_F(DecodeTest, BothNull) {
    constexpr int rowSize = 1;
    std::string binaryVals[rowSize] = {""};
    std::string charsetVals[rowSize] = {""};

    auto* binaryVec = CreateStringVec(rowSize, binaryVals, {0});
    auto* charsetVec = CreateStringVec(rowSize, charsetVals, {0});

    auto* result = RunDecode(binaryVec, charsetVec, rowSize);
    auto* outVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    ASSERT_NE(outVec, nullptr);
    EXPECT_TRUE(outVec->IsNull(0));

    delete result;
    delete binaryVec;
    delete charsetVec;
}

// ============================================================================
// Invalid/unsupported charset tests
// ============================================================================

TEST_F(DecodeTest, UnsupportedCharset) {
    constexpr int rowSize = 2;
    std::string binaryVals[rowSize] = {"test", "data"};
    std::string charsetVals[rowSize] = {"GBK", "INVALID-CHARSET"};

    auto* binaryVec = CreateStringVec(rowSize, binaryVals);
    auto* charsetVec = CreateStringVec(rowSize, charsetVals);

    auto* result = RunDecode(binaryVec, charsetVec, rowSize);
    auto* outVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    ASSERT_NE(outVec, nullptr);
    EXPECT_TRUE(outVec->IsNull(0));
    EXPECT_TRUE(outVec->IsNull(1));

    delete result;
    delete binaryVec;
    delete charsetVec;
}

// ============================================================================
// Empty binary tests
// ============================================================================

TEST_F(DecodeTest, EmptyBinaryAllCharsets) {
    constexpr int rowSize = 6;
    std::string binaryVals[rowSize] = {"", "", "", "", "", ""};
    std::string charsetVals[rowSize] = {"US-ASCII", "ISO-8859-1", "UTF-8", "UTF-16BE", "UTF-16LE", "UTF-16"};

    auto* binaryVec = CreateStringVec(rowSize, binaryVals);
    auto* charsetVec = CreateStringVec(rowSize, charsetVals);

    auto* result = RunDecode(binaryVec, charsetVec, rowSize);
    auto* outVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    ASSERT_NE(outVec, nullptr);

    for (int i = 0; i < rowSize; ++i) {
        EXPECT_EQ(std::string(outVec->GetValue(i)), "") << "Failed for charset index " << i;
    }

    delete result;
    delete binaryVec;
    delete charsetVec;
}

// ============================================================================
// UTF-16BE/LE orphan surrogate tests
// ============================================================================

TEST_F(DecodeTest, Utf16BEOrphanHighSurrogate) {
    constexpr int rowSize = 1;
    // High surrogate D800 without a following low surrogate
    std::string binaryVals[rowSize] = {std::string("\xD8\x00\x00\x48", 4)};
    std::string charsetVals[rowSize] = {"UTF-16BE"};

    auto* binaryVec = CreateStringVec(rowSize, binaryVals);
    auto* charsetVec = CreateStringVec(rowSize, charsetVals);

    auto* result = RunDecode(binaryVec, charsetVec, rowSize);
    auto* outVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    ASSERT_NE(outVec, nullptr);
    EXPECT_TRUE(outVec->IsNull(0));

    delete result;
    delete binaryVec;
    delete charsetVec;
}

TEST_F(DecodeTest, Utf16BEOrphanLowSurrogate) {
    constexpr int rowSize = 1;
    // Low surrogate DC00 without a preceding high surrogate
    std::string binaryVals[rowSize] = {std::string("\xDC\x00\x00\x48", 4)};
    std::string charsetVals[rowSize] = {"UTF-16BE"};

    auto* binaryVec = CreateStringVec(rowSize, binaryVals);
    auto* charsetVec = CreateStringVec(rowSize, charsetVals);

    auto* result = RunDecode(binaryVec, charsetVec, rowSize);
    auto* outVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    ASSERT_NE(outVec, nullptr);
    EXPECT_TRUE(outVec->IsNull(0));

    delete result;
    delete binaryVec;
    delete charsetVec;
}

// ============================================================================
// UTF-8 truncated sequence test
// ============================================================================

TEST_F(DecodeTest, Utf8TruncatedSequence) {
    constexpr int rowSize = 1;
    // 0xE4 starts a 3-byte sequence, but only 1 byte follows
    std::string binaryVals[rowSize] = {std::string("\xE4\xB8", 2)};
    std::string charsetVals[rowSize] = {"UTF-8"};

    auto* binaryVec = CreateStringVec(rowSize, binaryVals);
    auto* charsetVec = CreateStringVec(rowSize, charsetVals);

    auto* result = RunDecode(binaryVec, charsetVec, rowSize);
    auto* outVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    ASSERT_NE(outVec, nullptr);
    EXPECT_TRUE(outVec->IsNull(0));

    delete result;
    delete binaryVec;
    delete charsetVec;
}
