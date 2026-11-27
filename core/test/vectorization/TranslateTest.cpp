/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: Translate function unit tests
 *   translate(string, match, replace) -> varchar
 *   Returns a new translated string. It translates the character in string
 *   by a character in replace. The character in replace is corresponding
 *   to the character in match.
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

class TranslateTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const translate_test_env =
    ::testing::AddGlobalTestEnvironment(new TranslateTestEnvironment);

class TranslateFunctionTestHelper {
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
            std::string_view actual = resultVec->GetValue(i);
            std::string exp = expected[i];
            EXPECT_EQ(std::string(actual), exp) << "Row " << i << " expected=\"" << exp 
                << "\" actual=\"" << std::string(actual) << "\"";
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

    static void ExecuteTranslate(BaseVector* inputVec, BaseVector* matchVec,
                                 BaseVector* replaceVec, DataTypeId outputTypeId,
                                 BaseVector*& result) {
        std::vector<DataTypeId> inputTypeIds = {
            inputVec->GetTypeId(),
            matchVec->GetTypeId(),
            replaceVec->GetTypeId()
        };
        auto sig = std::make_shared<FunctionSignature>("translate", inputTypeIds, outputTypeId);
        auto fn = VectorFunction::Find(sig);
        ASSERT_NE(fn, nullptr) << "Translate function not found";
        auto outputType = std::make_shared<DataType>(outputTypeId);
        ExecutionContext ctx(4096, nullptr);
        ctx.SetResultRowSize(inputVec->GetSize());
        std::stack<BaseVector*> args;
        args.push(inputVec);
        args.push(matchVec);
        args.push(replaceVec);
        ASSERT_NO_THROW(fn->Apply(args, outputType, result, &ctx));
    }
};

// Test basic character replacement
TEST(TranslateTest, BasicReplacement) {
    std::vector<std::string> inputs = {"ab[cd]", "ab[cd]", "ab[cd]"};
    std::vector<std::string> matches = {"[]", "[]", "[]"};
    std::vector<std::string> replaces = {"##", "#", "#@$"};
    std::vector<std::string> expected = {"ab#cd#", "ab#cd", "ab#cd@"};
    
    BaseVector* inputVec = TranslateFunctionTestHelper::CreateStringVector(inputs);
    BaseVector* matchVec = TranslateFunctionTestHelper::CreateStringVector(matches);
    BaseVector* replaceVec = TranslateFunctionTestHelper::CreateStringVector(replaces);
    BaseVector* result = nullptr;
    
    TranslateFunctionTestHelper::ExecuteTranslate(inputVec, matchVec, replaceVec, OMNI_VARCHAR, result);
    TranslateFunctionTestHelper::ValidateStringResult(result, expected, 3);
    
    delete inputVec;
    delete matchVec;
    delete replaceVec;
    delete result;
}

// Test with spaces in replacement
TEST(TranslateTest, ReplacementWithSpaces) {
    std::vector<std::string> inputs = {"ab[cd]"};
    std::vector<std::string> matches = {"[]"};
    std::vector<std::string> replaces = {"  "};  // Two spaces
    std::vector<std::string> expected = {"ab cd "};
    
    BaseVector* inputVec = TranslateFunctionTestHelper::CreateStringVector(inputs);
    BaseVector* matchVec = TranslateFunctionTestHelper::CreateStringVector(matches);
    BaseVector* replaceVec = TranslateFunctionTestHelper::CreateStringVector(replaces);
    BaseVector* result = nullptr;
    
    TranslateFunctionTestHelper::ExecuteTranslate(inputVec, matchVec, replaceVec, OMNI_VARCHAR, result);
    TranslateFunctionTestHelper::ValidateStringResult(result, expected, 1);
    
    delete inputVec;
    delete matchVec;
    delete replaceVec;
    delete result;
}

// Test Unicode character replacement
TEST(TranslateTest, UnicodeReplacement) {
    // Unicode line separator \u2028 -> \u2029 paragraph separator
    std::vector<std::string> inputs = {"ab\xe2\x80\xa8"};  // "ab\u2028"
    std::vector<std::string> matches = {"\xe2\x80\xa8"};    // "\u2028"
    std::vector<std::string> replaces = {"\xe2\x80\xa9"};   // "\u2029"
    std::vector<std::string> expected = {"ab\xe2\x80\xa9"}; // "ab\u2029"
    
    BaseVector* inputVec = TranslateFunctionTestHelper::CreateStringVector(inputs);
    BaseVector* matchVec = TranslateFunctionTestHelper::CreateStringVector(matches);
    BaseVector* replaceVec = TranslateFunctionTestHelper::CreateStringVector(replaces);
    BaseVector* result = nullptr;
    
    TranslateFunctionTestHelper::ExecuteTranslate(inputVec, matchVec, replaceVec, OMNI_VARCHAR, result);
    TranslateFunctionTestHelper::ValidateStringResult(result, expected, 1);
    
    delete inputVec;
    delete matchVec;
    delete replaceVec;
    delete result;
}

// Test ASCII to Unicode replacement
TEST(TranslateTest, AsciiToUnicode) {
    // Replace 'a' with Unicode \u2029
    std::vector<std::string> inputs = {"abcabc"};
    std::vector<std::string> matches = {"a"};
    std::vector<std::string> replaces = {"\xe2\x80\xa9"};  // "\u2029"
    std::vector<std::string> expected = {"\xe2\x80\xa9" "bc" "\xe2\x80\xa9" "bc"};
    
    BaseVector* inputVec = TranslateFunctionTestHelper::CreateStringVector(inputs);
    BaseVector* matchVec = TranslateFunctionTestHelper::CreateStringVector(matches);
    BaseVector* replaceVec = TranslateFunctionTestHelper::CreateStringVector(replaces);
    BaseVector* result = nullptr;
    
    TranslateFunctionTestHelper::ExecuteTranslate(inputVec, matchVec, replaceVec, OMNI_VARCHAR, result);
    TranslateFunctionTestHelper::ValidateStringResult(result, expected, 1);
    
    delete inputVec;
    delete matchVec;
    delete replaceVec;
    delete result;
}

// Test empty match and replace - should return input unchanged
TEST(TranslateTest, EmptyMatchAndReplace) {
    std::vector<std::string> inputs = {"abc"};
    std::vector<std::string> matches = {""};
    std::vector<std::string> replaces = {""};
    std::vector<std::string> expected = {"abc"};
    
    BaseVector* inputVec = TranslateFunctionTestHelper::CreateStringVector(inputs);
    BaseVector* matchVec = TranslateFunctionTestHelper::CreateStringVector(matches);
    BaseVector* replaceVec = TranslateFunctionTestHelper::CreateStringVector(replaces);
    BaseVector* result = nullptr;
    
    TranslateFunctionTestHelper::ExecuteTranslate(inputVec, matchVec, replaceVec, OMNI_VARCHAR, result);
    TranslateFunctionTestHelper::ValidateStringResult(result, expected, 1);
    
    delete inputVec;
    delete matchVec;
    delete replaceVec;
    delete result;
}

// Test match longer than replace (extra characters deleted)
TEST(TranslateTest, MatchLongerThanReplace) {
    // "translate" with "rnlt" -> "123" means r->1, n->2, l->3, t->deleted
    std::vector<std::string> inputs = {"translate"};
    std::vector<std::string> matches = {"rnlt"};
    std::vector<std::string> replaces = {"123"};
    std::vector<std::string> expected = {"1a2s3ae"};  // t is deleted
    
    BaseVector* inputVec = TranslateFunctionTestHelper::CreateStringVector(inputs);
    BaseVector* matchVec = TranslateFunctionTestHelper::CreateStringVector(matches);
    BaseVector* replaceVec = TranslateFunctionTestHelper::CreateStringVector(replaces);
    BaseVector* result = nullptr;
    
    TranslateFunctionTestHelper::ExecuteTranslate(inputVec, matchVec, replaceVec, OMNI_VARCHAR, result);
    TranslateFunctionTestHelper::ValidateStringResult(result, expected, 1);
    
    delete inputVec;
    delete matchVec;
    delete replaceVec;
    delete result;
}

// Test all matched characters deleted (empty replace)
TEST(TranslateTest, AllMatchedDeleted) {
    std::vector<std::string> inputs = {"translate"};
    std::vector<std::string> matches = {"rnlt"};
    std::vector<std::string> replaces = {""};
    std::vector<std::string> expected = {"asae"};  // r, n, l, t all deleted
    
    BaseVector* inputVec = TranslateFunctionTestHelper::CreateStringVector(inputs);
    BaseVector* matchVec = TranslateFunctionTestHelper::CreateStringVector(matches);
    BaseVector* replaceVec = TranslateFunctionTestHelper::CreateStringVector(replaces);
    BaseVector* result = nullptr;
    
    TranslateFunctionTestHelper::ExecuteTranslate(inputVec, matchVec, replaceVec, OMNI_VARCHAR, result);
    TranslateFunctionTestHelper::ValidateStringResult(result, expected, 1);
    
    delete inputVec;
    delete matchVec;
    delete replaceVec;
    delete result;
}

// Test duplicate characters in match (only first occurrence considered)
TEST(TranslateTest, DuplicateInMatch) {
    // "aba" means a->1, b->2, second a ignored
    std::vector<std::string> inputs = {"abcd"};
    std::vector<std::string> matches = {"aba"};
    std::vector<std::string> replaces = {"123"};
    std::vector<std::string> expected = {"12cd"};  // a->1, b->2, second 'a' uses first mapping
    
    BaseVector* inputVec = TranslateFunctionTestHelper::CreateStringVector(inputs);
    BaseVector* matchVec = TranslateFunctionTestHelper::CreateStringVector(matches);
    BaseVector* replaceVec = TranslateFunctionTestHelper::CreateStringVector(replaces);
    BaseVector* result = nullptr;
    
    TranslateFunctionTestHelper::ExecuteTranslate(inputVec, matchVec, replaceVec, OMNI_VARCHAR, result);
    TranslateFunctionTestHelper::ValidateStringResult(result, expected, 1);
    
    delete inputVec;
    delete matchVec;
    delete replaceVec;
    delete result;
}

// Test NULL handling - any NULL argument should result in NULL
TEST(TranslateTest, NullHandling) {
    std::vector<std::string> inputs = {"abc", "abc", "abc"};
    std::vector<std::string> matches = {"a", "a", "a"};
    std::vector<std::string> replaces = {"x", "x", "x"};
    
    BaseVector* inputVec = TranslateFunctionTestHelper::CreateStringVector(inputs);
    BaseVector* matchVec = TranslateFunctionTestHelper::CreateStringVector(matches);
    BaseVector* replaceVec = TranslateFunctionTestHelper::CreateStringVector(replaces);
    
    // Set NULL for different arguments
    inputVec->SetNull(0);
    matchVec->SetNull(1);
    replaceVec->SetNull(2);
    
    BaseVector* result = nullptr;
    TranslateFunctionTestHelper::ExecuteTranslate(inputVec, matchVec, replaceVec, OMNI_VARCHAR, result);
    
    EXPECT_TRUE(result->IsNull(0)) << "Input NULL should result in NULL";
    EXPECT_TRUE(result->IsNull(1)) << "Match NULL should result in NULL";
    EXPECT_TRUE(result->IsNull(2)) << "Replace NULL should result in NULL";
    
    delete inputVec;
    delete matchVec;
    delete replaceVec;
    delete result;
}

// Test Chinese character replacement
TEST(TranslateTest, ChineseCharacterReplacement) {
    std::vector<std::string> inputs = {"你好世界"};
    std::vector<std::string> matches = {"好世"};
    std::vector<std::string> replaces = {"美丽"};
    std::vector<std::string> expected = {"你美丽界"};
    
    BaseVector* inputVec = TranslateFunctionTestHelper::CreateStringVector(inputs);
    BaseVector* matchVec = TranslateFunctionTestHelper::CreateStringVector(matches);
    BaseVector* replaceVec = TranslateFunctionTestHelper::CreateStringVector(replaces);
    BaseVector* result = nullptr;
    
    TranslateFunctionTestHelper::ExecuteTranslate(inputVec, matchVec, replaceVec, OMNI_VARCHAR, result);
    TranslateFunctionTestHelper::ValidateStringResult(result, expected, 1);
    
    delete inputVec;
    delete matchVec;
    delete replaceVec;
    delete result;
}

// Test mixed ASCII and Unicode
TEST(TranslateTest, MixedAsciiAndUnicode) {
    std::vector<std::string> inputs = {"abåæçè", "åæçèab"};
    std::vector<std::string> matches = {"ab", "ab"};
    std::vector<std::string> replaces = {"12", "12"};
    std::vector<std::string> expected = {"12åæçè", "åæçè12"};
    
    BaseVector* inputVec = TranslateFunctionTestHelper::CreateStringVector(inputs);
    BaseVector* matchVec = TranslateFunctionTestHelper::CreateStringVector(matches);
    BaseVector* replaceVec = TranslateFunctionTestHelper::CreateStringVector(replaces);
    BaseVector* result = nullptr;
    
    TranslateFunctionTestHelper::ExecuteTranslate(inputVec, matchVec, replaceVec, OMNI_VARCHAR, result);
    TranslateFunctionTestHelper::ValidateStringResult(result, expected, 2);
    
    delete inputVec;
    delete matchVec;
    delete replaceVec;
    delete result;
}

// Test Unicode match with Unicode replace
TEST(TranslateTest, UnicodeMatchAndReplace) {
    std::vector<std::string> inputs = {"abåæçè", "åæçèac"};
    std::vector<std::string> matches = {"aå", "çc"};
    std::vector<std::string> replaces = {"åa", "cç"};
    std::vector<std::string> expected = {"åbaæçè", "åæcèaç"};
    
    BaseVector* inputVec = TranslateFunctionTestHelper::CreateStringVector(inputs);
    BaseVector* matchVec = TranslateFunctionTestHelper::CreateStringVector(matches);
    BaseVector* replaceVec = TranslateFunctionTestHelper::CreateStringVector(replaces);
    BaseVector* result = nullptr;
    
    TranslateFunctionTestHelper::ExecuteTranslate(inputVec, matchVec, replaceVec, OMNI_VARCHAR, result);
    TranslateFunctionTestHelper::ValidateStringResult(result, expected, 2);
    
    delete inputVec;
    delete matchVec;
    delete replaceVec;
    delete result;
}

// Test single row
TEST(TranslateTest, SingleRow) {
    std::vector<std::string> inputs = {"hello"};
    std::vector<std::string> matches = {"el"};
    std::vector<std::string> replaces = {"EL"};
    std::vector<std::string> expected = {"hELLo"};
    
    BaseVector* inputVec = TranslateFunctionTestHelper::CreateStringVector(inputs);
    BaseVector* matchVec = TranslateFunctionTestHelper::CreateStringVector(matches);
    BaseVector* replaceVec = TranslateFunctionTestHelper::CreateStringVector(replaces);
    BaseVector* result = nullptr;
    
    TranslateFunctionTestHelper::ExecuteTranslate(inputVec, matchVec, replaceVec, OMNI_VARCHAR, result);
    TranslateFunctionTestHelper::ValidateStringResult(result, expected, 1);
    
    delete inputVec;
    delete matchVec;
    delete replaceVec;
    delete result;
}

// Test empty input string
TEST(TranslateTest, EmptyInput) {
    std::vector<std::string> inputs = {""};
    std::vector<std::string> matches = {"ab"};
    std::vector<std::string> replaces = {"12"};
    std::vector<std::string> expected = {""};
    
    BaseVector* inputVec = TranslateFunctionTestHelper::CreateStringVector(inputs);
    BaseVector* matchVec = TranslateFunctionTestHelper::CreateStringVector(matches);
    BaseVector* replaceVec = TranslateFunctionTestHelper::CreateStringVector(replaces);
    BaseVector* result = nullptr;
    
    TranslateFunctionTestHelper::ExecuteTranslate(inputVec, matchVec, replaceVec, OMNI_VARCHAR, result);
    TranslateFunctionTestHelper::ValidateStringResult(result, expected, 1);
    
    delete inputVec;
    delete matchVec;
    delete replaceVec;
    delete result;
}

// Test no match found (input unchanged)
TEST(TranslateTest, NoMatchFound) {
    std::vector<std::string> inputs = {"hello"};
    std::vector<std::string> matches = {"xyz"};
    std::vector<std::string> replaces = {"123"};
    std::vector<std::string> expected = {"hello"};
    
    BaseVector* inputVec = TranslateFunctionTestHelper::CreateStringVector(inputs);
    BaseVector* matchVec = TranslateFunctionTestHelper::CreateStringVector(matches);
    BaseVector* replaceVec = TranslateFunctionTestHelper::CreateStringVector(replaces);
    BaseVector* result = nullptr;
    
    TranslateFunctionTestHelper::ExecuteTranslate(inputVec, matchVec, replaceVec, OMNI_VARCHAR, result);
    TranslateFunctionTestHelper::ValidateStringResult(result, expected, 1);
    
    delete inputVec;
    delete matchVec;
    delete replaceVec;
    delete result;
}

// Test multiple rows with different patterns
TEST(TranslateTest, MultipleRowsDifferentPatterns) {
    std::vector<std::string> inputs = {"abcd", "cdab"};
    std::vector<std::string> matches = {"ab", "ca"};
    std::vector<std::string> replaces = {"#", "@$"};
    std::vector<std::string> expected = {"#cd", "@d$b"};
    
    BaseVector* inputVec = TranslateFunctionTestHelper::CreateStringVector(inputs);
    BaseVector* matchVec = TranslateFunctionTestHelper::CreateStringVector(matches);
    BaseVector* replaceVec = TranslateFunctionTestHelper::CreateStringVector(replaces);
    BaseVector* result = nullptr;
    
    TranslateFunctionTestHelper::ExecuteTranslate(inputVec, matchVec, replaceVec, OMNI_VARCHAR, result);
    TranslateFunctionTestHelper::ValidateStringResult(result, expected, 2);
    
    delete inputVec;
    delete matchVec;
    delete replaceVec;
    delete result;
}
