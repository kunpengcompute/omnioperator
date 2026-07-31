/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: IS_ALPHA function unit tests.
 *   IS_ALPHA(string) -> boolean.
 *   True iff the string is non-empty and every character is a Unicode letter
 *   (Java Character.isLetter semantics); NULL/empty input -> false (output NOT null);
 *   numeric input -> false.
 */

#include <gtest/gtest.h>
#include <stack>
#include <string>
#include <vector>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/VectorFunction.h"
#include "codegen/func_signature.h"
#include "vector/vector_helper.h"
#include "vector/vector.h"
#include "omni_exception.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::codegen;
using namespace omniruntime::TestUtil;

class IsAlphaTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const is_alpha_test_env =
    ::testing::AddGlobalTestEnvironment(new IsAlphaTestEnvironment);

namespace {

BaseVector* CreateStringVector(const std::vector<std::string>& values) {
    BaseVector* vec = VectorHelper::CreateStringVector(values.size());
    vec->SetIsField(true);
    auto* typedVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(vec);
    if (typedVec == nullptr) {
        ADD_FAILURE() << "CreateStringVector: vector is not string type";
        delete vec;
        return nullptr;
    }
    for (size_t i = 0; i < values.size(); ++i) {
        typedVec->SetValue(i, std::string_view(values[i]));
    }
    return vec;
}

BaseVector* CreateIntVector(const std::vector<int32_t>& values) {
    BaseVector* vec = VectorHelper::CreateFlatVector(OMNI_INT, values.size());
    auto* typedVec = static_cast<Vector<int32_t>*>(vec);
    for (size_t i = 0; i < values.size(); ++i) {
        typedVec->SetValue(i, values[i]);
    }
    return vec;
}

// Runs is_alpha on a single input vector of the given input type.
BaseVector* ExecuteIsAlpha(BaseVector* inputVec, DataTypeId inputTypeId) {
    if (inputVec == nullptr) {
        ADD_FAILURE() << "ExecuteIsAlpha: input vector is null";
        return nullptr;
    }
    auto signature = std::make_shared<FunctionSignature>("is_alpha",
        std::vector<DataTypeId>{inputTypeId}, OMNI_BOOLEAN);
    auto function = VectorFunction::Find(signature);
    if (function == nullptr) {
        ADD_FAILURE() << "is_alpha not found for type " << static_cast<int>(inputTypeId);
        return nullptr;
    }

    auto outputType = std::make_shared<DataType>(OMNI_BOOLEAN);
    ExecutionContext context;
    context.SetResultRowSize(inputVec->GetSize());
    std::stack<BaseVector*> args;
    args.push(inputVec);

    BaseVector* result = nullptr;
    function->Apply(args, outputType, result, &context);
    return result;
}

void ExpectAllNotNull(BaseVector* result, int rowSize) {
    ASSERT_NE(result, nullptr);
    for (int i = 0; i < rowSize; ++i) {
        EXPECT_FALSE(result->IsNull(i)) << "Row " << i << " must be non-NULL (IS_ALPHA never returns NULL)";
    }
}

void ValidateBool(BaseVector* result, const std::vector<bool>& expected) {
    ASSERT_NE(result, nullptr);
    auto* resultVec = dynamic_cast<Vector<bool>*>(result);
    ASSERT_NE(resultVec, nullptr) << "Result vector is not boolean type";
    for (size_t i = 0; i < expected.size(); ++i) {
        EXPECT_EQ(resultVec->GetValue(i), expected[i])
            << "Row " << i << " expected=" << (expected[i] ? "true" : "false")
            << " actual=" << (resultVec->GetValue(i) ? "true" : "false");
    }
}

}  // namespace

TEST(IsAlphaTest, AllAsciiLetters) {
    std::vector<std::string> in = {"abcXYZ", "Hello", "abcdefghijklmnopqrstuvwxyz", "A"};
    std::vector<bool> expected = {true, true, true, true};
    BaseVector* input = CreateStringVector(in);
    BaseVector* result = ExecuteIsAlpha(input, OMNI_VARCHAR);
    ValidateBool(result, expected);
    ExpectAllNotNull(result, in.size());
    delete result;
}

TEST(IsAlphaTest, DigitsSpacesSymbols) {
    std::vector<std::string> in = {"abc123", "abc ", "ab-cd", "a1", "123", "!!"};
    std::vector<bool> expected = {false, false, false, false, false, false};
    BaseVector* input = CreateStringVector(in);
    BaseVector* result = ExecuteIsAlpha(input, OMNI_VARCHAR);
    ValidateBool(result, expected);
    ExpectAllNotNull(result, in.size());
    delete result;
}

TEST(IsAlphaTest, EmptyString) {
    std::vector<std::string> in = {"", "a"};
    std::vector<bool> expected = {false, true};
    BaseVector* input = CreateStringVector(in);
    BaseVector* result = ExecuteIsAlpha(input, OMNI_VARCHAR);
    ValidateBool(result, expected);
    ExpectAllNotNull(result, in.size());
    delete result;
}

// NULL input -> false (output is NOT null), matching Flink isAlpha(null)=false.
TEST(IsAlphaTest, NullInputBecomesFalseNotNull) {
    std::vector<std::string> in = {"abc", "xxx", "yyy"};
    BaseVector* input = CreateStringVector(in);
    input->SetNull(1);
    BaseVector* result = ExecuteIsAlpha(input, OMNI_VARCHAR);

    ASSERT_NE(result, nullptr);
    auto* resultVec = dynamic_cast<Vector<bool>*>(result);
    ASSERT_NE(resultVec, nullptr);
    ExpectAllNotNull(result, in.size());  // including the NULL-input row
    EXPECT_TRUE(resultVec->GetValue(0));
    EXPECT_FALSE(resultVec->GetValue(1));  // NULL input -> false
    EXPECT_TRUE(resultVec->GetValue(2));
    delete result;
}

// Unicode letters (Chinese/Greek/accented) are letters -> true.
TEST(IsAlphaTest, UnicodeLetters) {
    std::vector<std::string> in = {
        "\xE4\xB8\xAD\xE6\x96\x87",              // 中文
        "\xCE\xB1\xCE\xB2\xCE\xB3",              // αβγ
        "caf\xC3\xA9",                            // café (é is a letter)
        "abc\xE4\xB8\xAD",                        // abc中 (all letters)
    };
    std::vector<bool> expected = {true, true, true, true};
    BaseVector* input = CreateStringVector(in);
    BaseVector* result = ExecuteIsAlpha(input, OMNI_VARCHAR);
    ValidateBool(result, expected);
    ExpectAllNotNull(result, in.size());
    delete result;
}

// Unicode letters mixed with non-letters -> false.
TEST(IsAlphaTest, UnicodeMixedWithNonLetter) {
    std::vector<std::string> in = {
        "\xE4\xB8\xAD" "1",                       // 中1 (digit)
        "\xE4\xB8\xAD" " ",                       // 中<space>
    };
    std::vector<bool> expected = {false, false};
    BaseVector* input = CreateStringVector(in);
    BaseVector* result = ExecuteIsAlpha(input, OMNI_VARCHAR);
    ValidateBool(result, expected);
    ExpectAllNotNull(result, in.size());
    delete result;
}

// Invalid UTF-8 (incl. overlong encodings) makes the whole string non-alpha (RFC 3629).
// Overlong forms could otherwise decode to letters (e.g. \xC1\x81 -> 'A') and be wrongly accepted.
TEST(IsAlphaTest, InvalidUtf8IsFalse) {
    std::vector<std::string> in = {
        std::string("\xC1\x81"),                 // overlong 2-byte 'A' (U+0041)
        std::string("\xE0\x80\x81"),             // overlong 3-byte 'A'
        std::string("\xF0\x80\x80\x81"),         // overlong 4-byte 'A'
        std::string("A\xC1\x81"),                // valid 'A' + overlong 'A'
        std::string("\xC3"),                     // truncated 2-byte lead
        std::string("\x80"),                     // lone continuation byte
    };
    std::vector<bool> expected = {false, false, false, false, false, false};
    BaseVector* input = CreateStringVector(in);
    BaseVector* result = ExecuteIsAlpha(input, OMNI_VARCHAR);
    ValidateBool(result, expected);
    ExpectAllNotNull(result, in.size());
    delete result;
}

// Constant-encoded input.
TEST(IsAlphaTest, ConstEncoding) {
    BaseVector* input = new ConstVector<std::string_view>(std::string_view("Hello"), OMNI_VARCHAR, 3);
    BaseVector* result = ExecuteIsAlpha(input, OMNI_VARCHAR);
    ASSERT_NE(result, nullptr);
    auto* resultVec = dynamic_cast<Vector<bool>*>(result);
    ASSERT_NE(resultVec, nullptr);
    for (int i = 0; i < 3; ++i) {
        EXPECT_FALSE(result->IsNull(i));
        EXPECT_TRUE(resultVec->GetValue(i));
    }
    delete result;
}

// Numeric input -> always false (Flink: value is not a String instance).
TEST(IsAlphaTest, NumericInputIsFalse) {
    std::vector<int32_t> in = {123, -7, 0};
    std::vector<bool> expected = {false, false, false};
    BaseVector* input = CreateIntVector(in);
    BaseVector* result = ExecuteIsAlpha(input, OMNI_INT);
    ValidateBool(result, expected);
    ExpectAllNotNull(result, in.size());
    delete result;
}
