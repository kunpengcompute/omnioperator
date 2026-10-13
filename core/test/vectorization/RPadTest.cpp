/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: RPad function unit tests
 *   rpad(string, size, padString) -> varchar
 *   Right pads string to size characters with padString.
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

class RPadTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const rpad_test_env =
    ::testing::AddGlobalTestEnvironment(new RPadTestEnvironment);

class RPadFunctionTestHelper {
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

    static void ExecuteRPadInt64(BaseVector* stringVec, BaseVector* sizeVec,
                                 BaseVector* padStringVec, BaseVector*& result) {
        std::vector<DataTypeId> inputTypeIds = { OMNI_VARCHAR, OMNI_LONG, OMNI_VARCHAR };
        auto sig = std::make_shared<FunctionSignature>("rpad", inputTypeIds, OMNI_VARCHAR);
        auto fn = VectorFunction::Find(sig);
        ASSERT_NE(fn, nullptr) << "rpad(string, int64, padString) not found";
        auto outputType = std::make_shared<DataType>(OMNI_VARCHAR);
        ExecutionContext ctx(4096, nullptr);
        ctx.SetResultRowSize(stringVec->GetSize());
        std::stack<BaseVector*> args;
        args.push(stringVec);
        args.push(sizeVec);
        args.push(padStringVec);
        ASSERT_NO_THROW(fn->Apply(args, outputType, result, &ctx));
    }

    static void ExecuteRPadInt32(BaseVector* stringVec, BaseVector* sizeVec,
                                 BaseVector* padStringVec, BaseVector*& result) {
        std::vector<DataTypeId> inputTypeIds = { OMNI_VARCHAR, OMNI_INT, OMNI_VARCHAR };
        auto sig = std::make_shared<FunctionSignature>("rpad", inputTypeIds, OMNI_VARCHAR);
        auto fn = VectorFunction::Find(sig);
        ASSERT_NE(fn, nullptr) << "rpad(string, int32, padString) not found";
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

// Test basic ASCII rpad with single character pad
TEST(RPadTest, BasicAsciiSingleCharPad) {
    std::vector<std::string> strings = {"text", "text", "text"};
    std::vector<int64_t> sizes = {5, 6, 7};
    std::vector<std::string> padStrings = {"x", "x", "x"};
    std::vector<std::string> expected = {"textx", "textxx", "textxxx"};
    
    BaseVector* strVec = RPadFunctionTestHelper::CreateStringVector(strings);
    BaseVector* sizeVec = RPadFunctionTestHelper::CreateInt64Vector(sizes);
    BaseVector* padVec = RPadFunctionTestHelper::CreateStringVector(padStrings);
    BaseVector* result = nullptr;
    
    RPadFunctionTestHelper::ExecuteRPadInt64(strVec, sizeVec, padVec, result);
    RPadFunctionTestHelper::ValidateStringResult(result, expected, 3);
    
    delete strVec;
    delete sizeVec;
    delete padVec;
    delete result;
}

// Test ASCII rpad with multi-character pad string
TEST(RPadTest, AsciiMultiCharPad) {
    std::vector<std::string> strings = {"text", "text", "text"};
    std::vector<int64_t> sizes = {6, 7, 9};
    std::vector<std::string> padStrings = {"xy", "xy", "xyz"};
    std::vector<std::string> expected = {"textxy", "textxyx", "textxyzxy"};
    
    BaseVector* strVec = RPadFunctionTestHelper::CreateStringVector(strings);
    BaseVector* sizeVec = RPadFunctionTestHelper::CreateInt64Vector(sizes);
    BaseVector* padVec = RPadFunctionTestHelper::CreateStringVector(padStrings);
    BaseVector* result = nullptr;
    
    RPadFunctionTestHelper::ExecuteRPadInt64(strVec, sizeVec, padVec, result);
    RPadFunctionTestHelper::ValidateStringResult(result, expected, 3);
    
    delete strVec;
    delete sizeVec;
    delete padVec;
    delete result;
}

// Test rpad with exact size (no padding needed)
TEST(RPadTest, ExactSizeNoPadding) {
    std::vector<std::string> strings = {"text"};
    std::vector<int64_t> sizes = {4};
    std::vector<std::string> padStrings = {"x"};
    std::vector<std::string> expected = {"text"};
    
    BaseVector* strVec = RPadFunctionTestHelper::CreateStringVector(strings);
    BaseVector* sizeVec = RPadFunctionTestHelper::CreateInt64Vector(sizes);
    BaseVector* padVec = RPadFunctionTestHelper::CreateStringVector(padStrings);
    BaseVector* result = nullptr;
    
    RPadFunctionTestHelper::ExecuteRPadInt64(strVec, sizeVec, padVec, result);
    RPadFunctionTestHelper::ValidateStringResult(result, expected, 1);
    
    delete strVec;
    delete sizeVec;
    delete padVec;
    delete result;
}

// Test rpad with truncation (size smaller than string length)
TEST(RPadTest, Truncation) {
    std::vector<std::string> strings = {"abc", "text", "hello"};
    std::vector<int64_t> sizes = {0, 3, 2};
    std::vector<std::string> padStrings = {"e", "xy", "z"};
    std::vector<std::string> expected = {"", "tex", "he"};
    
    BaseVector* strVec = RPadFunctionTestHelper::CreateStringVector(strings);
    BaseVector* sizeVec = RPadFunctionTestHelper::CreateInt64Vector(sizes);
    BaseVector* padVec = RPadFunctionTestHelper::CreateStringVector(padStrings);
    BaseVector* result = nullptr;
    
    RPadFunctionTestHelper::ExecuteRPadInt64(strVec, sizeVec, padVec, result);
    RPadFunctionTestHelper::ValidateStringResult(result, expected, 3);
    
    delete strVec;
    delete sizeVec;
    delete padVec;
    delete result;
}

// Test rpad with empty input string
TEST(RPadTest, EmptyInputString) {
    std::vector<std::string> strings = {"", ""};
    std::vector<int64_t> sizes = {3, 5};
    std::vector<std::string> padStrings = {"a", "xy"};
    std::vector<std::string> expected = {"aaa", "xyxyx"};
    
    BaseVector* strVec = RPadFunctionTestHelper::CreateStringVector(strings);
    BaseVector* sizeVec = RPadFunctionTestHelper::CreateInt64Vector(sizes);
    BaseVector* padVec = RPadFunctionTestHelper::CreateStringVector(padStrings);
    BaseVector* result = nullptr;
    
    RPadFunctionTestHelper::ExecuteRPadInt64(strVec, sizeVec, padVec, result);
    RPadFunctionTestHelper::ValidateStringResult(result, expected, 2);
    
    delete strVec;
    delete sizeVec;
    delete padVec;
    delete result;
}

// Test Unicode rpad (Chinese characters)
TEST(RPadTest, UnicodeChineseCharacters) {
    // "信念 爱 希望  " has 9 characters
    std::vector<std::string> strings = {"\u4FE1\u5FF5 \u7231 \u5E0C\u671B  "};
    std::vector<int64_t> sizes = {10};
    std::vector<std::string> padStrings = {"\u671B"};  // "望"
    // Expected: "信念 爱 希望  " + "望" = "信念 爱 希望  望"
    std::vector<std::string> expected = {"\u4FE1\u5FF5 \u7231 \u5E0C\u671B  \u671B"};
    
    BaseVector* strVec = RPadFunctionTestHelper::CreateStringVector(strings);
    BaseVector* sizeVec = RPadFunctionTestHelper::CreateInt64Vector(sizes);
    BaseVector* padVec = RPadFunctionTestHelper::CreateStringVector(padStrings);
    BaseVector* result = nullptr;
    
    RPadFunctionTestHelper::ExecuteRPadInt64(strVec, sizeVec, padVec, result);
    RPadFunctionTestHelper::ValidateStringResult(result, expected, 1);
    
    delete strVec;
    delete sizeVec;
    delete padVec;
    delete result;
}

// Test Unicode rpad with multi-char pad string
TEST(RPadTest, UnicodeMultiCharPad) {
    // "信念 爱 希望  " has 9 characters
    std::vector<std::string> strings = {"\u4FE1\u5FF5 \u7231 \u5E0C\u671B  "};
    std::vector<int64_t> sizes = {12};
    std::vector<std::string> padStrings = {"\u5E0C\u671B"};  // "希望"
    // Need 3 more chars: original + "希望希" = "信念 爱 希望  希望希"
    std::vector<std::string> expected = {"\u4FE1\u5FF5 \u7231 \u5E0C\u671B  \u5E0C\u671B\u5E0C"};
    
    BaseVector* strVec = RPadFunctionTestHelper::CreateStringVector(strings);
    BaseVector* sizeVec = RPadFunctionTestHelper::CreateInt64Vector(sizes);
    BaseVector* padVec = RPadFunctionTestHelper::CreateStringVector(padStrings);
    BaseVector* result = nullptr;
    
    RPadFunctionTestHelper::ExecuteRPadInt64(strVec, sizeVec, padVec, result);
    RPadFunctionTestHelper::ValidateStringResult(result, expected, 1);
    
    delete strVec;
    delete sizeVec;
    delete padVec;
    delete result;
}

// Test Unicode truncation
TEST(RPadTest, UnicodeTruncation) {
    // "信念 爱 希望  " has 9 characters, truncate to 5
    std::vector<std::string> strings = {"\u4FE1\u5FF5 \u7231 \u5E0C\u671B  "};
    std::vector<int64_t> sizes = {5};
    std::vector<std::string> padStrings = {"\u671B"};
    // Expected: "信念 爱 " (first 5 characters)
    std::vector<std::string> expected = {"\u4FE1\u5FF5 \u7231 "};
    
    BaseVector* strVec = RPadFunctionTestHelper::CreateStringVector(strings);
    BaseVector* sizeVec = RPadFunctionTestHelper::CreateInt64Vector(sizes);
    BaseVector* padVec = RPadFunctionTestHelper::CreateStringVector(padStrings);
    BaseVector* result = nullptr;
    
    RPadFunctionTestHelper::ExecuteRPadInt64(strVec, sizeVec, padVec, result);
    RPadFunctionTestHelper::ValidateStringResult(result, expected, 1);
    
    delete strVec;
    delete sizeVec;
    delete padVec;
    delete result;
}

// Test with INT32 size parameter
TEST(RPadTest, Int32SizeParameter) {
    std::vector<std::string> strings = {"text", "hello"};
    std::vector<int32_t> sizes = {6, 8};
    std::vector<std::string> padStrings = {"xy", "ab"};
    std::vector<std::string> expected = {"textxy", "helloaba"};
    
    BaseVector* strVec = RPadFunctionTestHelper::CreateStringVector(strings);
    BaseVector* sizeVec = RPadFunctionTestHelper::CreateInt32Vector(sizes);
    BaseVector* padVec = RPadFunctionTestHelper::CreateStringVector(padStrings);
    BaseVector* result = nullptr;
    
    RPadFunctionTestHelper::ExecuteRPadInt32(strVec, sizeVec, padVec, result);
    RPadFunctionTestHelper::ValidateStringResult(result, expected, 2);
    
    delete strVec;
    delete sizeVec;
    delete padVec;
    delete result;
}

// Test null propagation: when input string is null, result should be null
// Non-null rows should still produce correct results
TEST(RPadTest, NullPropagationInStringColumn) {
    std::vector<std::string> strings = {"text", "hello", "world"};
    std::vector<int64_t> sizes = {6, 8, 7};
    std::vector<std::string> padStrings = {"x", "y", "z"};
    // Expected results for non-null rows:
    // Row 0: rpad("text", 6, "x")  -> "textxx"
    // Row 1: null (string is null) -> null
    // Row 2: rpad("world", 7, "z") -> "worldzz"
    std::vector<std::string> expected = {"textxx", "", "worldzz"};
    
    BaseVector* strVec = RPadFunctionTestHelper::CreateStringVector(strings);
    BaseVector* sizeVec = RPadFunctionTestHelper::CreateInt64Vector(sizes);
    BaseVector* padVec = RPadFunctionTestHelper::CreateStringVector(padStrings);
    
    // Set second row string as null to test null propagation
    strVec->SetNull(1);
    
    BaseVector* result = nullptr;
    RPadFunctionTestHelper::ExecuteRPadInt64(strVec, sizeVec, padVec, result);
    
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
TEST(RPadTest, SizeZero) {
    std::vector<std::string> strings = {"text"};
    std::vector<int64_t> sizes = {0};
    std::vector<std::string> padStrings = {"x"};
    std::vector<std::string> expected = {""};
    
    BaseVector* strVec = RPadFunctionTestHelper::CreateStringVector(strings);
    BaseVector* sizeVec = RPadFunctionTestHelper::CreateInt64Vector(sizes);
    BaseVector* padVec = RPadFunctionTestHelper::CreateStringVector(padStrings);
    BaseVector* result = nullptr;
    
    RPadFunctionTestHelper::ExecuteRPadInt64(strVec, sizeVec, padVec, result);
    RPadFunctionTestHelper::ValidateStringResult(result, expected, 1);
    
    delete strVec;
    delete sizeVec;
    delete padVec;
    delete result;
}

// Test rpad matches velox test case from StringFunctionsTest.cpp
TEST(RPadTest, VeloxTestCases) {
    // From velox: rpad("\u4FE1\u5FF5 \u7231 \u5E0C\u671B  ", 11, "\u671B") = "\u4FE1\u5FF5 \u7231 \u5E0C\u671B  \u671B\u671B"
    std::vector<std::string> strings = {"\u4FE1\u5FF5 \u7231 \u5E0C\u671B  "};
    std::vector<int64_t> sizes = {11};
    std::vector<std::string> padStrings = {"\u671B"};
    std::vector<std::string> expected = {"\u4FE1\u5FF5 \u7231 \u5E0C\u671B  \u671B\u671B"};
    
    BaseVector* strVec = RPadFunctionTestHelper::CreateStringVector(strings);
    BaseVector* sizeVec = RPadFunctionTestHelper::CreateInt64Vector(sizes);
    BaseVector* padVec = RPadFunctionTestHelper::CreateStringVector(padStrings);
    BaseVector* result = nullptr;
    
    RPadFunctionTestHelper::ExecuteRPadInt64(strVec, sizeVec, padVec, result);
    RPadFunctionTestHelper::ValidateStringResult(result, expected, 1);
    
    delete strVec;
    delete sizeVec;
    delete padVec;
    delete result;
}

// ============================================================================
// BinaryFunctions style tests: two columns with different values per row
// These tests verify rpad works correctly when both string column and padString
// column have completely different values in each row (true column-to-column operation)
// ============================================================================

// Test BinaryFunctions: each row has different string and different padString (ASCII)
TEST(RPadTest, BinaryFunctionsAsciiBothColumnsDifferent) {
    // Each row: different string, different size, different padString
    std::vector<std::string> strings = {"hello", "world", "test", "abc", "x"};
    std::vector<int64_t> sizes = {8, 10, 6, 5, 4};
    std::vector<std::string> padStrings = {"*", "+-", "123", "!", "ab"};
    // Row 0: rpad("hello", 8, "*")   -> "hello***"
    // Row 1: rpad("world", 10, "+-") -> "world+-+-+"
    // Row 2: rpad("test", 6, "123")  -> "test12"
    // Row 3: rpad("abc", 5, "!")     -> "abc!!"
    // Row 4: rpad("x", 4, "ab")      -> "xaba"
    std::vector<std::string> expected = {"hello***", "world+-+-+", "test12", "abc!!", "xaba"};
    
    BaseVector* strVec = RPadFunctionTestHelper::CreateStringVector(strings);
    BaseVector* sizeVec = RPadFunctionTestHelper::CreateInt64Vector(sizes);
    BaseVector* padVec = RPadFunctionTestHelper::CreateStringVector(padStrings);
    BaseVector* result = nullptr;
    
    RPadFunctionTestHelper::ExecuteRPadInt64(strVec, sizeVec, padVec, result);
    RPadFunctionTestHelper::ValidateStringResult(result, expected, 5);
    
    delete strVec;
    delete sizeVec;
    delete padVec;
    delete result;
}

// Test BinaryFunctions: each row has different string and different padString (Unicode)
TEST(RPadTest, BinaryFunctionsUnicodeBothColumnsDifferent) {
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
    // Row 0: rpad("你好", 5, "★")     -> "你好★★★"
    // Row 1: rpad("世界", 6, "☆★")   -> "世界☆★☆★"
    // Row 2: rpad("hello", 8, "中")  -> "hello中中中"
    // Row 3: rpad("希望", 4, "爱")   -> "希望爱爱"
    std::vector<std::string> expected = {
        "\u4F60\u597D\u2605\u2605\u2605",
        "\u4E16\u754C\u2606\u2605\u2606\u2605",
        "hello\u4E2D\u4E2D\u4E2D",
        "\u5E0C\u671B\u7231\u7231"
    };
    
    BaseVector* strVec = RPadFunctionTestHelper::CreateStringVector(strings);
    BaseVector* sizeVec = RPadFunctionTestHelper::CreateInt64Vector(sizes);
    BaseVector* padVec = RPadFunctionTestHelper::CreateStringVector(padStrings);
    BaseVector* result = nullptr;
    
    RPadFunctionTestHelper::ExecuteRPadInt64(strVec, sizeVec, padVec, result);
    RPadFunctionTestHelper::ValidateStringResult(result, expected, 4);
    
    delete strVec;
    delete sizeVec;
    delete padVec;
    delete result;
}

// Test BinaryFunctions: mixed scenarios (padding, truncation, exact size)
TEST(RPadTest, BinaryFunctionsMixedScenarios) {
    std::vector<std::string> strings = {"abc", "hello", "test", "x", "longstring"};
    std::vector<int64_t> sizes = {6, 5, 2, 3, 5};  // pad, exact, truncate, pad, truncate
    std::vector<std::string> padStrings = {"12", "x", "y", "ab", "z"};
    // Row 0: rpad("abc", 6, "12")      -> "abc121" (padding)
    // Row 1: rpad("hello", 5, "x")     -> "hello" (exact, no change)
    // Row 2: rpad("test", 2, "y")      -> "te" (truncation)
    // Row 3: rpad("x", 3, "ab")        -> "xab" (padding)
    // Row 4: rpad("longstring", 5, "z")-> "longs" (truncation)
    std::vector<std::string> expected = {"abc121", "hello", "te", "xab", "longs"};
    
    BaseVector* strVec = RPadFunctionTestHelper::CreateStringVector(strings);
    BaseVector* sizeVec = RPadFunctionTestHelper::CreateInt64Vector(sizes);
    BaseVector* padVec = RPadFunctionTestHelper::CreateStringVector(padStrings);
    BaseVector* result = nullptr;
    
    RPadFunctionTestHelper::ExecuteRPadInt64(strVec, sizeVec, padVec, result);
    RPadFunctionTestHelper::ValidateStringResult(result, expected, 5);
    
    delete strVec;
    delete sizeVec;
    delete padVec;
    delete result;
}

// Test BinaryFunctions: null handling in different columns
TEST(RPadTest, BinaryFunctionsNullInDifferentColumns) {
    std::vector<std::string> strings = {"hello", "world", "test", "abc"};
    std::vector<int64_t> sizes = {8, 8, 8, 8};
    std::vector<std::string> padStrings = {"*", "+", "-", "!"};
    
    BaseVector* strVec = RPadFunctionTestHelper::CreateStringVector(strings);
    BaseVector* sizeVec = RPadFunctionTestHelper::CreateInt64Vector(sizes);
    BaseVector* padVec = RPadFunctionTestHelper::CreateStringVector(padStrings);
    
    // Set nulls in different columns at different rows
    strVec->SetNull(0);    // string column null at row 0
    sizeVec->SetNull(1);   // size column null at row 1
    padVec->SetNull(2);    // padString column null at row 2
    // Row 3: all non-null
    
    BaseVector* result = nullptr;
    RPadFunctionTestHelper::ExecuteRPadInt64(strVec, sizeVec, padVec, result);
    
    // Null propagation: any null input -> null output
    EXPECT_TRUE(result->IsNull(0));   // string null
    EXPECT_TRUE(result->IsNull(1));   // size null
    EXPECT_TRUE(result->IsNull(2));   // padString null
    EXPECT_FALSE(result->IsNull(3));  // all non-null -> valid result
    
    // Verify the valid row
    auto* resultVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    ASSERT_NE(resultVec, nullptr);
    std::string actual(resultVec->GetValue(3));
    EXPECT_EQ(actual, "abc!!!!!");  // rpad("abc", 8, "!") -> "abc!!!!!"
    
    delete strVec;
    delete sizeVec;
    delete padVec;
    delete result;
}
