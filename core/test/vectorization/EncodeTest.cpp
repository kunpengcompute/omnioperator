/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Unit tests for encode(string, charset) function
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

class EncodeTest : public ::testing::Test {
protected:
    void SetUp() override {
        RegisterFunctions::Register();
    }
};

// Helper: create a string vector with given values
static Vector<LargeStringContainer<std::string_view>>* CreateStringVec(
    int rowSize, const std::string* values, const std::vector<int>& nullIndices = {})
{
    // Mirror the proven-working FlinkStringTest helper: use CreateFlatVector(OMNI_VARCHAR)
    // and explicitly mark every row null / not-null. The string vector's SetValue does not
    // touch the null flag, so SetNotNull must be called for non-null rows.
    BaseVector* rawVec = VectorHelper::CreateFlatVector(OMNI_VARCHAR, rowSize);
    rawVec->SetIsField(true);
    auto* vec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(rawVec);
    for (int i = 0; i < rowSize; ++i) {
        bool isNull = std::find(nullIndices.begin(), nullIndices.end(), i) != nullIndices.end();
        if (isNull) {
            vec->SetNull(i);
        } else {
            std::string_view sv(values[i]);
            vec->SetValue(i, sv);
            vec->SetNotNull(i);
        }
    }
    return vec;
}

// Helper: run encode function via VectorFunction::Find + Apply
static BaseVector* RunEncode(BaseVector* stringVec, BaseVector* charsetVec, int rowSize)
{
    auto signature = std::make_shared<FunctionSignature>("encode",
        std::vector<DataTypeId>{OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_VARBINARY);
    auto function = VectorFunction::Find(signature);
    if (!function) {
        return nullptr;
    }

    BaseVector* resultVector = nullptr;
    auto varbinaryType = std::make_shared<DataType>(OMNI_VARBINARY);
    ExecutionContext context;
    context.SetResultRowSize(rowSize);
    std::stack<BaseVector*> args;
    // Apply unpacks arguments from the stack top expecting the LAST parameter on top.
    // So push in ExprEval order (left to right): string first, charset last (on top).
    args.push(stringVec);
    args.push(charsetVec);

    function->Apply(args, varbinaryType, resultVector, &context);
    return resultVector;
}

// ============================================================================
// US-ASCII charset tests
// ============================================================================

TEST_F(EncodeTest, AsciiBasic) {
    constexpr int rowSize = 3;
    std::string stringVals[rowSize] = {"Hello", "", "World"};
    std::string charsetVals[rowSize] = {"US-ASCII", "US-ASCII", "US-ASCII"};

    auto* stringVec = CreateStringVec(rowSize, stringVals);
    auto* charsetVec = CreateStringVec(rowSize, charsetVals);

    auto* result = RunEncode(stringVec, charsetVec, rowSize);
    auto* outVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    ASSERT_NE(outVec, nullptr);

    EXPECT_EQ(std::string(outVec->GetValue(0)), "Hello");
    EXPECT_EQ(std::string(outVec->GetValue(1)), "");
    EXPECT_EQ(std::string(outVec->GetValue(2)), "World");

    delete result;
    delete stringVec;
    delete charsetVec;
}

TEST_F(EncodeTest, AsciiNonAsciiChar) {
    constexpr int rowSize = 1;
    // "Ä" in UTF-8 is 0xC3 0x84, which is not valid US-ASCII
    std::string stringVals[rowSize] = {"\xC3\x84"};
    std::string charsetVals[rowSize] = {"US-ASCII"};

    auto* stringVec = CreateStringVec(rowSize, stringVals);
    auto* charsetVec = CreateStringVec(rowSize, charsetVals);

    auto* result = RunEncode(stringVec, charsetVec, rowSize);
    auto* outVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    ASSERT_NE(outVec, nullptr);
    EXPECT_TRUE(outVec->IsNull(0));

    delete result;
    delete stringVec;
    delete charsetVec;
}

TEST_F(EncodeTest, AsciiAlias) {
    constexpr int rowSize = 1;
    std::string stringVals[rowSize] = {"Test"};
    std::string charsetVals[rowSize] = {"ASCII"};

    auto* stringVec = CreateStringVec(rowSize, stringVals);
    auto* charsetVec = CreateStringVec(rowSize, charsetVals);

    auto* result = RunEncode(stringVec, charsetVec, rowSize);
    auto* outVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    ASSERT_NE(outVec, nullptr);
    EXPECT_EQ(std::string(outVec->GetValue(0)), "Test");

    delete result;
    delete stringVec;
    delete charsetVec;
}

// ============================================================================
// ISO-8859-1 charset tests
// ============================================================================

TEST_F(EncodeTest, Iso88591Basic) {
    constexpr int rowSize = 2;
    // "Ää" in UTF-8 is 0xC3 0x84 0xC3 0xA4
    std::string stringVals[rowSize] = {
        "\xC3\x84\xC3\xA4",   // "Ää"
        "\xC3\xBF"             // "ÿ" (U+00FF)
    };
    std::string charsetVals[rowSize] = {"ISO-8859-1", "ISO-8859-1"};

    auto* stringVec = CreateStringVec(rowSize, stringVals);
    auto* charsetVec = CreateStringVec(rowSize, charsetVals);

    auto* result = RunEncode(stringVec, charsetVec, rowSize);
    auto* outVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    ASSERT_NE(outVec, nullptr);

    // "Ää" -> [0xC4, 0xE4]
    EXPECT_EQ(std::string(outVec->GetValue(0)), std::string("\xC4\xE4", 2));
    // "ÿ" -> [0xFF]
    EXPECT_EQ(std::string(outVec->GetValue(1)), std::string("\xFF", 1));

    delete result;
    delete stringVec;
    delete charsetVec;
}

TEST_F(EncodeTest, Iso88591Latin1Alias) {
    constexpr int rowSize = 1;
    // "é" in UTF-8 is 0xC3 0xA9
    std::string stringVals[rowSize] = {"\xC3\xA9"};
    std::string charsetVals[rowSize] = {"LATIN1"};

    auto* stringVec = CreateStringVec(rowSize, stringVals);
    auto* charsetVec = CreateStringVec(rowSize, charsetVals);

    auto* result = RunEncode(stringVec, charsetVec, rowSize);
    auto* outVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    ASSERT_NE(outVec, nullptr);
    // "é" (U+00E9) -> [0xE9]
    EXPECT_EQ(std::string(outVec->GetValue(0)), std::string("\xE9", 1));

    delete result;
    delete stringVec;
    delete charsetVec;
}

TEST_F(EncodeTest, Iso88591CodepointTooLarge) {
    constexpr int rowSize = 1;
    // "中" in UTF-8 is 0xE4 0xB8 0xAD (U+4E2D), which is > 0xFF
    std::string stringVals[rowSize] = {"\xE4\xB8\xAD"};
    std::string charsetVals[rowSize] = {"ISO-8859-1"};

    auto* stringVec = CreateStringVec(rowSize, stringVals);
    auto* charsetVec = CreateStringVec(rowSize, charsetVals);

    auto* result = RunEncode(stringVec, charsetVec, rowSize);
    auto* outVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    ASSERT_NE(outVec, nullptr);
    EXPECT_TRUE(outVec->IsNull(0));

    delete result;
    delete stringVec;
    delete charsetVec;
}

// ============================================================================
// UTF-8 charset tests
// ============================================================================

TEST_F(EncodeTest, Utf8Basic) {
    constexpr int rowSize = 2;
    std::string stringVals[rowSize] = {"Hello", "\xE4\xB8\xAD\xE6\x96\x87"}; // "中文"
    std::string charsetVals[rowSize] = {"UTF-8", "UTF-8"};

    auto* stringVec = CreateStringVec(rowSize, stringVals);
    auto* charsetVec = CreateStringVec(rowSize, charsetVals);

    auto* result = RunEncode(stringVec, charsetVec, rowSize);
    auto* outVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    ASSERT_NE(outVec, nullptr);

    EXPECT_EQ(std::string(outVec->GetValue(0)), "Hello");
    EXPECT_EQ(std::string(outVec->GetValue(1)), "\xE4\xB8\xAD\xE6\x96\x87");

    delete result;
    delete stringVec;
    delete charsetVec;
}

TEST_F(EncodeTest, Utf8InvalidInput) {
    constexpr int rowSize = 1;
    // Invalid UTF-8 byte
    std::string stringVals[rowSize] = {std::string("\xFF", 1)};
    std::string charsetVals[rowSize] = {"UTF-8"};

    auto* stringVec = CreateStringVec(rowSize, stringVals);
    auto* charsetVec = CreateStringVec(rowSize, charsetVals);

    auto* result = RunEncode(stringVec, charsetVec, rowSize);
    auto* outVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    ASSERT_NE(outVec, nullptr);
    EXPECT_TRUE(outVec->IsNull(0));

    delete result;
    delete stringVec;
    delete charsetVec;
}

TEST_F(EncodeTest, Utf8CaseInsensitive) {
    constexpr int rowSize = 1;
    std::string stringVals[rowSize] = {"Hi"};
    std::string charsetVals[rowSize] = {"utf-8"};

    auto* stringVec = CreateStringVec(rowSize, stringVals);
    auto* charsetVec = CreateStringVec(rowSize, charsetVals);

    auto* result = RunEncode(stringVec, charsetVec, rowSize);
    auto* outVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    ASSERT_NE(outVec, nullptr);
    EXPECT_EQ(std::string(outVec->GetValue(0)), "Hi");

    delete result;
    delete stringVec;
    delete charsetVec;
}

// ============================================================================
// UTF-16BE charset tests
// ============================================================================

TEST_F(EncodeTest, Utf16BEBasic) {
    constexpr int rowSize = 1;
    std::string stringVals[rowSize] = {"Hi"};
    std::string charsetVals[rowSize] = {"UTF-16BE"};

    auto* stringVec = CreateStringVec(rowSize, stringVals);
    auto* charsetVec = CreateStringVec(rowSize, charsetVals);

    auto* result = RunEncode(stringVec, charsetVec, rowSize);
    auto* outVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    ASSERT_NE(outVec, nullptr);
    // "Hi" -> H=0x0048, i=0x0069 in UTF-16BE
    EXPECT_EQ(std::string(outVec->GetValue(0)), std::string("\x00\x48\x00\x69", 4));

    delete result;
    delete stringVec;
    delete charsetVec;
}

TEST_F(EncodeTest, Utf16BESurrogatePair) {
    constexpr int rowSize = 1;
    // U+1F600 (😀) in UTF-8: F0 9F 98 80
    std::string stringVals[rowSize] = {std::string("\xF0\x9F\x98\x80", 4)};
    std::string charsetVals[rowSize] = {"UTF-16BE"};

    auto* stringVec = CreateStringVec(rowSize, stringVals);
    auto* charsetVec = CreateStringVec(rowSize, charsetVals);

    auto* result = RunEncode(stringVec, charsetVec, rowSize);
    auto* outVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    ASSERT_NE(outVec, nullptr);
    // U+1F600 in UTF-16BE: D83D DE00
    EXPECT_EQ(std::string(outVec->GetValue(0)), std::string("\xD8\x3D\xDE\x00", 4));

    delete result;
    delete stringVec;
    delete charsetVec;
}

// ============================================================================
// UTF-16LE charset tests
// ============================================================================

TEST_F(EncodeTest, Utf16LEBasic) {
    constexpr int rowSize = 1;
    std::string stringVals[rowSize] = {"Hi"};
    std::string charsetVals[rowSize] = {"UTF-16LE"};

    auto* stringVec = CreateStringVec(rowSize, stringVals);
    auto* charsetVec = CreateStringVec(rowSize, charsetVals);

    auto* result = RunEncode(stringVec, charsetVec, rowSize);
    auto* outVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    ASSERT_NE(outVec, nullptr);
    // "Hi" -> H=0x4800, i=0x6900 in UTF-16LE
    EXPECT_EQ(std::string(outVec->GetValue(0)), std::string("\x48\x00\x69\x00", 4));

    delete result;
    delete stringVec;
    delete charsetVec;
}

TEST_F(EncodeTest, Utf16LESurrogatePair) {
    constexpr int rowSize = 1;
    // U+1F600 (😀) in UTF-8: F0 9F 98 80
    std::string stringVals[rowSize] = {std::string("\xF0\x9F\x98\x80", 4)};
    std::string charsetVals[rowSize] = {"UTF-16LE"};

    auto* stringVec = CreateStringVec(rowSize, stringVals);
    auto* charsetVec = CreateStringVec(rowSize, charsetVals);

    auto* result = RunEncode(stringVec, charsetVec, rowSize);
    auto* outVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    ASSERT_NE(outVec, nullptr);
    // U+1F600 in UTF-16LE: 3DD8 00DE
    EXPECT_EQ(std::string(outVec->GetValue(0)), std::string("\x3D\xD8\x00\xDE", 4));

    delete result;
    delete stringVec;
    delete charsetVec;
}

// ============================================================================
// UTF-16 (with BOM) charset tests
// ============================================================================

TEST_F(EncodeTest, Utf16WithBom) {
    constexpr int rowSize = 1;
    std::string stringVals[rowSize] = {"H"};
    std::string charsetVals[rowSize] = {"UTF-16"};

    auto* stringVec = CreateStringVec(rowSize, stringVals);
    auto* charsetVec = CreateStringVec(rowSize, charsetVals);

    auto* result = RunEncode(stringVec, charsetVec, rowSize);
    auto* outVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    ASSERT_NE(outVec, nullptr);
    // BOM (FE FF) + "H" in UTF-16BE (00 48)
    EXPECT_EQ(std::string(outVec->GetValue(0)), std::string("\xFE\xFF\x00\x48", 4));

    delete result;
    delete stringVec;
    delete charsetVec;
}

TEST_F(EncodeTest, Utf16Empty) {
    constexpr int rowSize = 1;
    std::string stringVals[rowSize] = {""};
    std::string charsetVals[rowSize] = {"UTF-16"};

    auto* stringVec = CreateStringVec(rowSize, stringVals);
    auto* charsetVec = CreateStringVec(rowSize, charsetVals);

    auto* result = RunEncode(stringVec, charsetVec, rowSize);
    auto* outVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    ASSERT_NE(outVec, nullptr);
    // Empty string with UTF-16 should just have BOM
    EXPECT_EQ(std::string(outVec->GetValue(0)), std::string("\xFE\xFF", 2));

    delete result;
    delete stringVec;
    delete charsetVec;
}

// ============================================================================
// NULL handling tests
// ============================================================================

TEST_F(EncodeTest, NullString) {
    constexpr int rowSize = 2;
    std::string stringVals[rowSize] = {"", "Hello"};
    std::string charsetVals[rowSize] = {"UTF-8", "UTF-8"};

    auto* stringVec = CreateStringVec(rowSize, stringVals, {0}); // Row 0 is NULL
    auto* charsetVec = CreateStringVec(rowSize, charsetVals);

    auto* result = RunEncode(stringVec, charsetVec, rowSize);
    auto* outVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    ASSERT_NE(outVec, nullptr);
    EXPECT_TRUE(outVec->IsNull(0));
    EXPECT_EQ(std::string(outVec->GetValue(1)), "Hello");

    delete result;
    delete stringVec;
    delete charsetVec;
}

TEST_F(EncodeTest, NullCharset) {
    constexpr int rowSize = 2;
    std::string stringVals[rowSize] = {"Hello", "World"};
    std::string charsetVals[rowSize] = {"UTF-8", ""};

    auto* stringVec = CreateStringVec(rowSize, stringVals);
    auto* charsetVec = CreateStringVec(rowSize, charsetVals, {1}); // Row 1 is NULL

    auto* result = RunEncode(stringVec, charsetVec, rowSize);
    auto* outVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    ASSERT_NE(outVec, nullptr);
    EXPECT_EQ(std::string(outVec->GetValue(0)), "Hello");
    EXPECT_TRUE(outVec->IsNull(1));

    delete result;
    delete stringVec;
    delete charsetVec;
}

TEST_F(EncodeTest, BothNull) {
    constexpr int rowSize = 1;
    std::string stringVals[rowSize] = {""};
    std::string charsetVals[rowSize] = {""};

    auto* stringVec = CreateStringVec(rowSize, stringVals, {0});
    auto* charsetVec = CreateStringVec(rowSize, charsetVals, {0});

    auto* result = RunEncode(stringVec, charsetVec, rowSize);
    auto* outVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    ASSERT_NE(outVec, nullptr);
    EXPECT_TRUE(outVec->IsNull(0));

    delete result;
    delete stringVec;
    delete charsetVec;
}

// ============================================================================
// Invalid/unsupported charset tests
// ============================================================================

TEST_F(EncodeTest, UnsupportedCharset) {
    constexpr int rowSize = 2;
    std::string stringVals[rowSize] = {"test", "data"};
    std::string charsetVals[rowSize] = {"GBK", "INVALID-CHARSET"};

    auto* stringVec = CreateStringVec(rowSize, stringVals);
    auto* charsetVec = CreateStringVec(rowSize, charsetVals);

    auto* result = RunEncode(stringVec, charsetVec, rowSize);
    auto* outVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    ASSERT_NE(outVec, nullptr);
    EXPECT_TRUE(outVec->IsNull(0));
    EXPECT_TRUE(outVec->IsNull(1));

    delete result;
    delete stringVec;
    delete charsetVec;
}

// ============================================================================
// Empty string tests
// ============================================================================

TEST_F(EncodeTest, EmptyStringAllCharsets) {
    constexpr int rowSize = 6;
    std::string stringVals[rowSize] = {"", "", "", "", "", ""};
    std::string charsetVals[rowSize] = {"US-ASCII", "ISO-8859-1", "UTF-8", "UTF-16BE", "UTF-16LE", "UTF-16"};

    auto* stringVec = CreateStringVec(rowSize, stringVals);
    auto* charsetVec = CreateStringVec(rowSize, charsetVals);

    auto* result = RunEncode(stringVec, charsetVec, rowSize);
    auto* outVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    ASSERT_NE(outVec, nullptr);

    // Empty string should produce empty binary for all charsets except UTF-16 (which has BOM)
    EXPECT_EQ(std::string(outVec->GetValue(0)), ""); // US-ASCII
    EXPECT_EQ(std::string(outVec->GetValue(1)), ""); // ISO-8859-1
    EXPECT_EQ(std::string(outVec->GetValue(2)), ""); // UTF-8
    EXPECT_EQ(std::string(outVec->GetValue(3)), ""); // UTF-16BE
    EXPECT_EQ(std::string(outVec->GetValue(4)), ""); // UTF-16LE
    EXPECT_EQ(std::string(outVec->GetValue(5)), std::string("\xFE\xFF", 2)); // UTF-16 (BOM only)

    delete result;
    delete stringVec;
    delete charsetVec;
}

// ============================================================================
// Invalid UTF-8 input tests
// ============================================================================

TEST_F(EncodeTest, InvalidUtf8Input) {
    constexpr int rowSize = 2;
    std::string stringVals[rowSize] = {
        std::string("\xC0\x80", 2),  // Overlong encoding
        std::string("\xE4\xB8", 2)   // Truncated 3-byte sequence
    };
    std::string charsetVals[rowSize] = {"UTF-8", "UTF-16BE"};

    auto* stringVec = CreateStringVec(rowSize, stringVals);
    auto* charsetVec = CreateStringVec(rowSize, charsetVals);

    auto* result = RunEncode(stringVec, charsetVec, rowSize);
    auto* outVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    ASSERT_NE(outVec, nullptr);
    EXPECT_TRUE(outVec->IsNull(0));
    EXPECT_TRUE(outVec->IsNull(1));

    delete result;
    delete stringVec;
    delete charsetVec;
}
