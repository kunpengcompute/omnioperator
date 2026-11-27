/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: LPad function unit tests
 *   lpad(string, size, padString) -> varchar
 *   Left pads string to size characters with padString.
 *   If size is less than the length of string, the result is truncated to size characters.
 */

#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/VectorFunction.h"
#include "codegen/func_signature.h"
#include "vector/vector_helper.h"
#include "vector/vector.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::mem;
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::codegen;
using namespace omniruntime::TestUtil;

class LPadTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const lpad_test_env =
    ::testing::AddGlobalTestEnvironment(new LPadTestEnvironment);

class LPadFunctionTestHelper {
public:
    static void ValidateStringResult(BaseVector* result,
                                    const std::vector<std::string>& expected,
                                    int rowSize) {
        auto* resultVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
        ASSERT_NE(resultVec, nullptr) << "Result vector type mismatch";
        for (int i = 0; i < rowSize; ++i) {
            if (result->IsNull(i)) {
                continue;
            }
            std::string_view actualSv = resultVec->GetValue(i);
            std::string actual(actualSv);
            std::string exp = expected[i];
            EXPECT_EQ(actual, exp) << "Row " << i << " expected=\"" << exp << "\" actual=\"" << actual << "\"";
        }
    }

    static BaseVector* CreateStringVector(const std::vector<std::string>& values) {
        BaseVector* vec = VectorHelper::CreateStringVector(values.size());
        vec->SetIsField(true);
        auto* typed = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(vec);
        EXPECT_NE(typed, nullptr);
        for (size_t i = 0; i < values.size(); ++i) {
            std::string_view sv(values[i]);
            typed->SetValue(i, sv);
        }
        return vec;
    }

    static BaseVector* CreateInt64Vector(const std::vector<int64_t>& values) {
        BaseVector* vec = VectorHelper::CreateFlatVector(nullptr, OMNI_LONG, values.size());
        vec->SetIsField(true);
        auto* typed = static_cast<Vector<int64_t>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typed->SetValue(i, values[i]);
        }
        return vec;
    }

    static BaseVector* CreateInt32Vector(const std::vector<int32_t>& values) {
        BaseVector* vec = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, values.size());
        vec->SetIsField(true);
        auto* typed = static_cast<Vector<int32_t>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typed->SetValue(i, values[i]);
        }
        return vec;
    }

    static void ExecuteLPadInt64(BaseVector* stringVec, BaseVector* sizeVec,
                                 BaseVector* padStringVec, BaseVector*& result) {
        std::vector<DataTypeId> inputTypeIds = { OMNI_VARCHAR, OMNI_LONG, OMNI_VARCHAR };
        auto sig = std::make_shared<FunctionSignature>("lpad", inputTypeIds, OMNI_VARCHAR);
        auto fn = VectorFunction::Find(sig);
        ASSERT_NE(fn, nullptr) << "lpad(string, int64, padString) not found";
        auto outputType = std::make_shared<DataType>(OMNI_VARCHAR);
        ExecutionContext ctx(4096, nullptr);
        ctx.SetResultRowSize(stringVec->GetSize());
        std::stack<BaseVector*> args;
        args.push(stringVec);
        args.push(sizeVec);
        args.push(padStringVec);
        ASSERT_NO_THROW(fn->Apply(args, outputType, result, &ctx));
    }

    static void ExecuteLPadInt32(BaseVector* stringVec, BaseVector* sizeVec,
                                 BaseVector* padStringVec, BaseVector*& result) {
        std::vector<DataTypeId> inputTypeIds = { OMNI_VARCHAR, OMNI_INT, OMNI_VARCHAR };
        auto sig = std::make_shared<FunctionSignature>("lpad", inputTypeIds, OMNI_VARCHAR);
        auto fn = VectorFunction::Find(sig);
        ASSERT_NE(fn, nullptr) << "lpad(string, int32, padString) not found";
        auto outputType = std::make_shared<DataType>(OMNI_VARCHAR);
        ExecutionContext ctx(4096, nullptr);
        ctx.SetResultRowSize(stringVec->GetSize());
        std::stack<BaseVector*> args;
        args.push(stringVec);
        args.push(sizeVec);
        args.push(padStringVec);
        ASSERT_NO_THROW(fn->Apply(args, outputType, result, &ctx));
    }
};

// Test basic ASCII lpad with single character pad
TEST(LPadTest, BasicAsciiSingleCharPad) {
    std::vector<std::string> strings = {"text", "text", "text"};
    std::vector<int64_t> sizes = {5, 6, 7};
    std::vector<std::string> padStrings = {"x", "x", "x"};
    std::vector<std::string> expected = {"xtext", "xxtext", "xxxtext"};
    
    BaseVector* strVec = LPadFunctionTestHelper::CreateStringVector(strings);
    BaseVector* sizeVec = LPadFunctionTestHelper::CreateInt64Vector(sizes);
    BaseVector* padVec = LPadFunctionTestHelper::CreateStringVector(padStrings);
    BaseVector* result = nullptr;
    
    LPadFunctionTestHelper::ExecuteLPadInt64(strVec, sizeVec, padVec, result);
    LPadFunctionTestHelper::ValidateStringResult(result, expected, 3);
    
    delete strVec;
    delete sizeVec;
    delete padVec;
    delete result;
}

// Test ASCII lpad with multi-character pad string
TEST(LPadTest, AsciiMultiCharPad) {
    std::vector<std::string> strings = {"text", "text", "text"};
    std::vector<int64_t> sizes = {6, 7, 9};
    std::vector<std::string> padStrings = {"xy", "xy", "xyz"};
    std::vector<std::string> expected = {"xytext", "xyxtext", "xyzxytext"};
    
    BaseVector* strVec = LPadFunctionTestHelper::CreateStringVector(strings);
    BaseVector* sizeVec = LPadFunctionTestHelper::CreateInt64Vector(sizes);
    BaseVector* padVec = LPadFunctionTestHelper::CreateStringVector(padStrings);
    BaseVector* result = nullptr;
    
    LPadFunctionTestHelper::ExecuteLPadInt64(strVec, sizeVec, padVec, result);
    LPadFunctionTestHelper::ValidateStringResult(result, expected, 3);
    
    delete strVec;
    delete sizeVec;
    delete padVec;
    delete result;
}

// Test lpad with exact size (no padding needed)
TEST(LPadTest, ExactSizeNoPadding) {
    std::vector<std::string> strings = {"text"};
    std::vector<int64_t> sizes = {4};
    std::vector<std::string> padStrings = {"x"};
    std::vector<std::string> expected = {"text"};
    
    BaseVector* strVec = LPadFunctionTestHelper::CreateStringVector(strings);
    BaseVector* sizeVec = LPadFunctionTestHelper::CreateInt64Vector(sizes);
    BaseVector* padVec = LPadFunctionTestHelper::CreateStringVector(padStrings);
    BaseVector* result = nullptr;
    
    LPadFunctionTestHelper::ExecuteLPadInt64(strVec, sizeVec, padVec, result);
    LPadFunctionTestHelper::ValidateStringResult(result, expected, 1);
    
    delete strVec;
    delete sizeVec;
    delete padVec;
    delete result;
}

// Test lpad with truncation (size smaller than string length)
TEST(LPadTest, Truncation) {
    std::vector<std::string> strings = {"abc", "text", "hello"};
    std::vector<int64_t> sizes = {0, 3, 2};
    std::vector<std::string> padStrings = {"e", "xy", "z"};
    std::vector<std::string> expected = {"", "tex", "he"};
    
    BaseVector* strVec = LPadFunctionTestHelper::CreateStringVector(strings);
    BaseVector* sizeVec = LPadFunctionTestHelper::CreateInt64Vector(sizes);
    BaseVector* padVec = LPadFunctionTestHelper::CreateStringVector(padStrings);
    BaseVector* result = nullptr;
    
    LPadFunctionTestHelper::ExecuteLPadInt64(strVec, sizeVec, padVec, result);
    LPadFunctionTestHelper::ValidateStringResult(result, expected, 3);
    
    delete strVec;
    delete sizeVec;
    delete padVec;
    delete result;
}

// Test lpad with empty input string
TEST(LPadTest, EmptyInputString) {
    std::vector<std::string> strings = {"", ""};
    std::vector<int64_t> sizes = {3, 5};
    std::vector<std::string> padStrings = {"a", "xy"};
    std::vector<std::string> expected = {"aaa", "xyxyx"};
    
    BaseVector* strVec = LPadFunctionTestHelper::CreateStringVector(strings);
    BaseVector* sizeVec = LPadFunctionTestHelper::CreateInt64Vector(sizes);
    BaseVector* padVec = LPadFunctionTestHelper::CreateStringVector(padStrings);
    BaseVector* result = nullptr;
    
    LPadFunctionTestHelper::ExecuteLPadInt64(strVec, sizeVec, padVec, result);
    LPadFunctionTestHelper::ValidateStringResult(result, expected, 2);
    
    delete strVec;
    delete sizeVec;
    delete padVec;
    delete result;
}

// Test Unicode lpad (Chinese characters)
TEST(LPadTest, UnicodeChineseCharacters) {
    // "信念 爱 希望  " has 9 characters
    std::vector<std::string> strings = {"\u4FE1\u5FF5 \u7231 \u5E0C\u671B  "};
    std::vector<int64_t> sizes = {10};
    std::vector<std::string> padStrings = {"\u671B"};  // "望"
    // Expected: "望" + "信念 爱 希望  " = "望信念 爱 希望  "
    std::vector<std::string> expected = {"\u671B\u4FE1\u5FF5 \u7231 \u5E0C\u671B  "};
    
    BaseVector* strVec = LPadFunctionTestHelper::CreateStringVector(strings);
    BaseVector* sizeVec = LPadFunctionTestHelper::CreateInt64Vector(sizes);
    BaseVector* padVec = LPadFunctionTestHelper::CreateStringVector(padStrings);
    BaseVector* result = nullptr;
    
    LPadFunctionTestHelper::ExecuteLPadInt64(strVec, sizeVec, padVec, result);
    LPadFunctionTestHelper::ValidateStringResult(result, expected, 1);
    
    delete strVec;
    delete sizeVec;
    delete padVec;
    delete result;
}

// Test Unicode lpad with multi-char pad string
TEST(LPadTest, UnicodeMultiCharPad) {
    // "信念 爱 希望  " has 9 characters
    std::vector<std::string> strings = {"\u4FE1\u5FF5 \u7231 \u5E0C\u671B  "};
    std::vector<int64_t> sizes = {12};
    std::vector<std::string> padStrings = {"\u5E0C\u671B"};  // "希望"
    // Need 3 more chars: "希望希" + original = "希望希信念 爱 希望  "
    std::vector<std::string> expected = {"\u5E0C\u671B\u5E0C\u4FE1\u5FF5 \u7231 \u5E0C\u671B  "};
    
    BaseVector* strVec = LPadFunctionTestHelper::CreateStringVector(strings);
    BaseVector* sizeVec = LPadFunctionTestHelper::CreateInt64Vector(sizes);
    BaseVector* padVec = LPadFunctionTestHelper::CreateStringVector(padStrings);
    BaseVector* result = nullptr;
    
    LPadFunctionTestHelper::ExecuteLPadInt64(strVec, sizeVec, padVec, result);
    LPadFunctionTestHelper::ValidateStringResult(result, expected, 1);
    
    delete strVec;
    delete sizeVec;
    delete padVec;
    delete result;
}

// Test Unicode truncation
TEST(LPadTest, UnicodeTruncation) {
    // "信念 爱 希望  " has 9 characters, truncate to 5
    std::vector<std::string> strings = {"\u4FE1\u5FF5 \u7231 \u5E0C\u671B  "};
    std::vector<int64_t> sizes = {5};
    std::vector<std::string> padStrings = {"\u671B"};
    // Expected: "信念 爱 " (first 5 characters)
    std::vector<std::string> expected = {"\u4FE1\u5FF5 \u7231 "};
    
    BaseVector* strVec = LPadFunctionTestHelper::CreateStringVector(strings);
    BaseVector* sizeVec = LPadFunctionTestHelper::CreateInt64Vector(sizes);
    BaseVector* padVec = LPadFunctionTestHelper::CreateStringVector(padStrings);
    BaseVector* result = nullptr;
    
    LPadFunctionTestHelper::ExecuteLPadInt64(strVec, sizeVec, padVec, result);
    LPadFunctionTestHelper::ValidateStringResult(result, expected, 1);
    
    delete strVec;
    delete sizeVec;
    delete padVec;
    delete result;
}

// Test with INT32 size parameter
TEST(LPadTest, Int32SizeParameter) {
    std::vector<std::string> strings = {"text", "hello"};
    std::vector<int32_t> sizes = {6, 8};
    std::vector<std::string> padStrings = {"xy", "ab"};
    std::vector<std::string> expected = {"xytext", "abahello"};
    
    BaseVector* strVec = LPadFunctionTestHelper::CreateStringVector(strings);
    BaseVector* sizeVec = LPadFunctionTestHelper::CreateInt32Vector(sizes);
    BaseVector* padVec = LPadFunctionTestHelper::CreateStringVector(padStrings);
    BaseVector* result = nullptr;
    
    LPadFunctionTestHelper::ExecuteLPadInt32(strVec, sizeVec, padVec, result);
    LPadFunctionTestHelper::ValidateStringResult(result, expected, 2);
    
    delete strVec;
    delete sizeVec;
    delete padVec;
    delete result;
}

// Test null propagation: when input string is null, result should be null
// Non-null rows should still produce correct results
TEST(LPadTest, NullPropagationInStringColumn) {
    std::vector<std::string> strings = {"text", "hello", "world"};
    std::vector<int64_t> sizes = {6, 8, 7};
    std::vector<std::string> padStrings = {"x", "y", "z"};
    // Expected results for non-null rows:
    // Row 0: lpad("text", 6, "x")  -> "xxtext"
    // Row 1: null (string is null) -> null
    // Row 2: lpad("world", 7, "z") -> "zzworld"
    std::vector<std::string> expected = {"xxtext", "", "zzworld"};
    
    BaseVector* strVec = LPadFunctionTestHelper::CreateStringVector(strings);
    BaseVector* sizeVec = LPadFunctionTestHelper::CreateInt64Vector(sizes);
    BaseVector* padVec = LPadFunctionTestHelper::CreateStringVector(padStrings);
    
    // Set second row string as null to test null propagation
    strVec->SetNull(1);
    
    BaseVector* result = nullptr;
    LPadFunctionTestHelper::ExecuteLPadInt64(strVec, sizeVec, padVec, result);
    
    // Verify null propagation
    EXPECT_FALSE(result->IsNull(0));
    EXPECT_TRUE(result->IsNull(1));  // null input -> null output
    EXPECT_FALSE(result->IsNull(2));
    
    // Verify non-null rows have correct results
    auto* resultVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    ASSERT_NE(resultVec, nullptr);
    EXPECT_EQ(std::string(resultVec->GetValue(0)), expected[0]);
    EXPECT_EQ(std::string(resultVec->GetValue(2)), expected[2]);
    
    delete strVec;
    delete sizeVec;
    delete padVec;
    delete result;
}

// Test boundary: size = 0
TEST(LPadTest, SizeZero) {
    std::vector<std::string> strings = {"text"};
    std::vector<int64_t> sizes = {0};
    std::vector<std::string> padStrings = {"x"};
    std::vector<std::string> expected = {""};
    
    BaseVector* strVec = LPadFunctionTestHelper::CreateStringVector(strings);
    BaseVector* sizeVec = LPadFunctionTestHelper::CreateInt64Vector(sizes);
    BaseVector* padVec = LPadFunctionTestHelper::CreateStringVector(padStrings);
    BaseVector* result = nullptr;
    
    LPadFunctionTestHelper::ExecuteLPadInt64(strVec, sizeVec, padVec, result);
    LPadFunctionTestHelper::ValidateStringResult(result, expected, 1);
    
    delete strVec;
    delete sizeVec;
    delete padVec;
    delete result;
}

// ============================================================================
// BinaryFunctions style tests: two columns with different values per row
// These tests verify lpad works correctly when both string column and padString
// column have completely different values in each row (true column-to-column operation)
// ============================================================================

// Test BinaryFunctions: each row has different string and different padString (ASCII)
TEST(LPadTest, BinaryFunctionsAsciiBothColumnsDifferent) {
    // Each row: different string, different size, different padString
    std::vector<std::string> strings = {"hello", "world", "test", "abc", "x"};
    std::vector<int64_t> sizes = {8, 10, 6, 5, 4};
    std::vector<std::string> padStrings = {"*", "+-", "123", "!", "ab"};
    // Row 0: lpad("hello", 8, "*")   -> "***hello"
    // Row 1: lpad("world", 10, "+-") -> "+-+-+world"
    // Row 2: lpad("test", 6, "123")  -> "12test"
    // Row 3: lpad("abc", 5, "!")     -> "!!abc"
    // Row 4: lpad("x", 4, "ab")      -> "abax"
    std::vector<std::string> expected = {"***hello", "+-+-+world", "12test", "!!abc", "abax"};
    
    BaseVector* strVec = LPadFunctionTestHelper::CreateStringVector(strings);
    BaseVector* sizeVec = LPadFunctionTestHelper::CreateInt64Vector(sizes);
    BaseVector* padVec = LPadFunctionTestHelper::CreateStringVector(padStrings);
    BaseVector* result = nullptr;
    
    LPadFunctionTestHelper::ExecuteLPadInt64(strVec, sizeVec, padVec, result);
    LPadFunctionTestHelper::ValidateStringResult(result, expected, 5);
    
    delete strVec;
    delete sizeVec;
    delete padVec;
    delete result;
}

// Test BinaryFunctions: each row has different string and different padString (Unicode)
TEST(LPadTest, BinaryFunctionsUnicodeBothColumnsDifferent) {
    // Each row: different Unicode string, different size, different Unicode padString
    std::vector<std::string> strings = {
        "\u4F60\u597D",           // "你好" (2 chars)
        "\u4E16\u754C",           // "世界" (2 chars)
        "hello",                  // 5 chars
        "\u5E0C\u671B"            // "希望" (2 chars)
    };
    std::vector<int64_t> sizes = {5, 6, 8, 4};
    std::vector<std::string> padStrings = {
        "\u2605",                 // "★" (1 char)
        "\u2606\u2605",           // "☆★" (2 chars)
        "\u4E2D",                 // "中" (1 char)
        "\u7231"                  // "爱" (1 char)
    };
    // Row 0: lpad("你好", 5, "★")     -> "★★★你好"
    // Row 1: lpad("世界", 6, "☆★")   -> "☆★☆★世界"
    // Row 2: lpad("hello", 8, "中")  -> "中中中hello"
    // Row 3: lpad("希望", 4, "爱")   -> "爱爱希望"
    std::vector<std::string> expected = {
        "\u2605\u2605\u2605\u4F60\u597D",
        "\u2606\u2605\u2606\u2605\u4E16\u754C",
        "\u4E2D\u4E2D\u4E2Dhello",
        "\u7231\u7231\u5E0C\u671B"
    };
    
    BaseVector* strVec = LPadFunctionTestHelper::CreateStringVector(strings);
    BaseVector* sizeVec = LPadFunctionTestHelper::CreateInt64Vector(sizes);
    BaseVector* padVec = LPadFunctionTestHelper::CreateStringVector(padStrings);
    BaseVector* result = nullptr;
    
    LPadFunctionTestHelper::ExecuteLPadInt64(strVec, sizeVec, padVec, result);
    LPadFunctionTestHelper::ValidateStringResult(result, expected, 4);
    
    delete strVec;
    delete sizeVec;
    delete padVec;
    delete result;
}

// Test BinaryFunctions: mixed scenarios (padding, truncation, exact size)
TEST(LPadTest, BinaryFunctionsMixedScenarios) {
    std::vector<std::string> strings = {"abc", "hello", "test", "x", "longstring"};
    std::vector<int64_t> sizes = {6, 5, 2, 3, 5};  // pad, exact, truncate, pad, truncate
    std::vector<std::string> padStrings = {"12", "x", "y", "ab", "z"};
    // Row 0: lpad("abc", 6, "12")      -> "121abc" (padding)
    // Row 1: lpad("hello", 5, "x")     -> "hello" (exact, no change)
    // Row 2: lpad("test", 2, "y")      -> "te" (truncation)
    // Row 3: lpad("x", 3, "ab")        -> "abx" (padding)
    // Row 4: lpad("longstring", 5, "z")-> "longs" (truncation)
    std::vector<std::string> expected = {"121abc", "hello", "te", "abx", "longs"};
    
    BaseVector* strVec = LPadFunctionTestHelper::CreateStringVector(strings);
    BaseVector* sizeVec = LPadFunctionTestHelper::CreateInt64Vector(sizes);
    BaseVector* padVec = LPadFunctionTestHelper::CreateStringVector(padStrings);
    BaseVector* result = nullptr;
    
    LPadFunctionTestHelper::ExecuteLPadInt64(strVec, sizeVec, padVec, result);
    LPadFunctionTestHelper::ValidateStringResult(result, expected, 5);
    
    delete strVec;
    delete sizeVec;
    delete padVec;
    delete result;
}

// Test BinaryFunctions: null handling in different columns
TEST(LPadTest, BinaryFunctionsNullInDifferentColumns) {
    std::vector<std::string> strings = {"hello", "world", "test", "abc"};
    std::vector<int64_t> sizes = {8, 8, 8, 8};
    std::vector<std::string> padStrings = {"*", "+", "-", "!"};
    
    BaseVector* strVec = LPadFunctionTestHelper::CreateStringVector(strings);
    BaseVector* sizeVec = LPadFunctionTestHelper::CreateInt64Vector(sizes);
    BaseVector* padVec = LPadFunctionTestHelper::CreateStringVector(padStrings);
    
    // Set nulls in different columns at different rows
    strVec->SetNull(0);    // string column null at row 0
    sizeVec->SetNull(1);   // size column null at row 1
    padVec->SetNull(2);    // padString column null at row 2
    // Row 3: all non-null
    
    BaseVector* result = nullptr;
    LPadFunctionTestHelper::ExecuteLPadInt64(strVec, sizeVec, padVec, result);
    
    // Null propagation: any null input -> null output
    EXPECT_TRUE(result->IsNull(0));   // string null
    EXPECT_TRUE(result->IsNull(1));   // size null
    EXPECT_TRUE(result->IsNull(2));   // padString null
    EXPECT_FALSE(result->IsNull(3));  // all non-null -> valid result
    
    // Verify the valid row
    auto* resultVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    ASSERT_NE(resultVec, nullptr);
    std::string actual(resultVec->GetValue(3));
    EXPECT_EQ(actual, "!!!!!abc");  // lpad("abc", 8, "!") -> "!!!!!abc"
    
    delete strVec;
    delete sizeVec;
    delete padVec;
    delete result;
}
