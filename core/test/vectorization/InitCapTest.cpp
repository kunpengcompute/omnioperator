/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: initcap function unit tests.
 *   initcap(string) -> string. Capitalizes the first letter of each word;
 *   lowercases the rest. Word boundaries are whitespace characters (Spark SQL semantics).
 */

#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <stack>

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

class InitCapTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const initcap_test_env =
    ::testing::AddGlobalTestEnvironment(new InitCapTestEnvironment);

class InitCapFunctionTestHelper {
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

    static void ExecuteInitCap(BaseVector* stringVec, BaseVector*& result) {
        std::vector<DataTypeId> inputTypeIds = {OMNI_VARCHAR};
        auto sig = std::make_shared<FunctionSignature>("initcap", inputTypeIds, OMNI_VARCHAR);
        auto fn = VectorFunction::Find(sig);
        ASSERT_NE(fn, nullptr) << "initcap not found";
        auto outputType = std::make_shared<DataType>(OMNI_VARCHAR);
        ExecutionContext ctx(4096, nullptr);
        ctx.SetResultRowSize(stringVec->GetSize());
        std::stack<BaseVector*> args;
        args.push(stringVec);
        ASSERT_NO_THROW(fn->Apply(args, outputType, result, &ctx));
    }
};

TEST(InitCapTest, BasicLowercase) {
    std::vector<std::string> strings = {"hello world", "foo bar baz"};
    std::vector<std::string> expected = {"Hello World", "Foo Bar Baz"};
    BaseVector* strVec = InitCapFunctionTestHelper::CreateStringVector(strings);
    BaseVector* result = nullptr;
    InitCapFunctionTestHelper::ExecuteInitCap(strVec, result);
    InitCapFunctionTestHelper::ValidateStringResult(result, expected, 2);
    delete strVec;
    delete result;
}

TEST(InitCapTest, BasicUppercase) {
    std::vector<std::string> strings = {"HELLO WORLD", "ABCDEFG"};
    std::vector<std::string> expected = {"Hello World", "Abcdefg"};
    BaseVector* strVec = InitCapFunctionTestHelper::CreateStringVector(strings);
    BaseVector* result = nullptr;
    InitCapFunctionTestHelper::ExecuteInitCap(strVec, result);
    InitCapFunctionTestHelper::ValidateStringResult(result, expected, 2);
    delete strVec;
    delete result;
}

TEST(InitCapTest, MixedCase) {
    std::vector<std::string> strings = {"a B c D e F g", "hElLo WoRlD", "AbCdEf"};
    std::vector<std::string> expected = {"A B C D E F G", "Hello World", "Abcdef"};
    BaseVector* strVec = InitCapFunctionTestHelper::CreateStringVector(strings);
    BaseVector* result = nullptr;
    InitCapFunctionTestHelper::ExecuteInitCap(strVec, result);
    InitCapFunctionTestHelper::ValidateStringResult(result, expected, 3);
    delete strVec;
    delete result;
}

TEST(InitCapTest, NumbersOnly) {
    std::vector<std::string> strings = {"1234", "1 2 3 4", "1 2 3 4a"};
    std::vector<std::string> expected = {"1234", "1 2 3 4", "1 2 3 4a"};
    BaseVector* strVec = InitCapFunctionTestHelper::CreateStringVector(strings);
    BaseVector* result = nullptr;
    InitCapFunctionTestHelper::ExecuteInitCap(strVec, result);
    InitCapFunctionTestHelper::ValidateStringResult(result, expected, 3);
    delete strVec;
    delete result;
}

TEST(InitCapTest, EmptyString) {
    std::vector<std::string> strings = {"", "ABC"};
    std::vector<std::string> expected = {"", "Abc"};
    BaseVector* strVec = InitCapFunctionTestHelper::CreateStringVector(strings);
    BaseVector* result = nullptr;
    InitCapFunctionTestHelper::ExecuteInitCap(strVec, result);
    InitCapFunctionTestHelper::ValidateStringResult(result, expected, 2);
    delete strVec;
    delete result;
}

TEST(InitCapTest, NullPropagation) {
    std::vector<std::string> strings = {"hello world", "xyz", "FOO BAR", "test"};
    BaseVector* strVec = InitCapFunctionTestHelper::CreateStringVector(strings);
    strVec->SetNull(1);
    strVec->SetNull(3);
    BaseVector* result = nullptr;
    InitCapFunctionTestHelper::ExecuteInitCap(strVec, result);
    auto* resultVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    ASSERT_NE(resultVec, nullptr);
    EXPECT_EQ(std::string(resultVec->GetValue(0)), "Hello World");
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";
    EXPECT_EQ(std::string(resultVec->GetValue(2)), "Foo Bar");
    EXPECT_TRUE(resultVec->IsNull(3)) << "Row 3 should be NULL";
    delete strVec;
    delete result;
}

TEST(InitCapTest, NonLetterDelimiters) {
    std::vector<std::string> strings = {
        "urna.Ut@egetdictumplacerat.edu",
        "nibh.enim@egestas.ca",
        "in@Donecat.ca",
        "john_doe-123@example-site.com",
        "MIXED.case-EMAIL_42@domain.NET",
        "...weird..case@@"
    };
    std::vector<std::string> expected = {
        "Urna.ut@egetdictumplacerat.edu",
        "Nibh.enim@egestas.ca",
        "In@donecat.ca",
        "John_doe-123@example-site.com",
        "Mixed.case-email_42@domain.net",
        "...weird..case@@"
    };
    BaseVector* strVec = InitCapFunctionTestHelper::CreateStringVector(strings);
    BaseVector* result = nullptr;
    InitCapFunctionTestHelper::ExecuteInitCap(strVec, result);
    InitCapFunctionTestHelper::ValidateStringResult(result, expected, 6);
    delete strVec;
    delete result;
}

TEST(InitCapTest, WhitespaceDelimiters) {
    std::vector<std::string> strings = {"YQ\tY", "YQ\nY", "hello\tworld", "foo\nbar"};
    std::vector<std::string> expected = {"Yq\tY", "Yq\nY", "Hello\tWorld", "Foo\nBar"};
    BaseVector* strVec = InitCapFunctionTestHelper::CreateStringVector(strings);
    BaseVector* result = nullptr;
    InitCapFunctionTestHelper::ExecuteInitCap(strVec, result);
    InitCapFunctionTestHelper::ValidateStringResult(result, expected, 4);
    delete strVec;
    delete result;
}

TEST(InitCapTest, SingleCharacter) {
    std::vector<std::string> strings = {"a", "A", "1", " "};
    std::vector<std::string> expected = {"A", "A", "1", " "};
    BaseVector* strVec = InitCapFunctionTestHelper::CreateStringVector(strings);
    BaseVector* result = nullptr;
    InitCapFunctionTestHelper::ExecuteInitCap(strVec, result);
    InitCapFunctionTestHelper::ValidateStringResult(result, expected, 4);
    delete strVec;
    delete result;
}

TEST(InitCapTest, MultipleSpaces) {
    std::vector<std::string> strings = {"  hello  world  ", "  HELLO  ", "a  b  c"};
    std::vector<std::string> expected = {"  Hello  World  ", "  Hello  ", "A  B  C"};
    BaseVector* strVec = InitCapFunctionTestHelper::CreateStringVector(strings);
    BaseVector* result = nullptr;
    InitCapFunctionTestHelper::ExecuteInitCap(strVec, result);
    InitCapFunctionTestHelper::ValidateStringResult(result, expected, 3);
    delete strVec;
    delete result;
}

TEST(InitCapTest, SpecialCharacters) {
    std::vector<std::string> strings = {
        "user-name+filter@sub.mail.org",
        "CAPS_LOCK@DOMAIN.COM",
        "__init__.py@example.dev",
        "sodales@blanditviverraDonec.ca"
    };
    std::vector<std::string> expected = {
        "User-name+filter@sub.mail.org",
        "Caps_lock@domain.com",
        "__init__.py@example.dev",
        "Sodales@blanditviverradonec.ca"
    };
    BaseVector* strVec = InitCapFunctionTestHelper::CreateStringVector(strings);
    BaseVector* result = nullptr;
    InitCapFunctionTestHelper::ExecuteInitCap(strVec, result);
    InitCapFunctionTestHelper::ValidateStringResult(result, expected, 4);
    delete strVec;
    delete result;
}

TEST(InitCapTest, NumberMixed) {
    std::vector<std::string> strings = {"123", "1abc", "a1b2c3"};
    std::vector<std::string> expected = {"123", "1abc", "A1b2c3"};
    BaseVector* strVec = InitCapFunctionTestHelper::CreateStringVector(strings);
    BaseVector* result = nullptr;
    InitCapFunctionTestHelper::ExecuteInitCap(strVec, result);
    InitCapFunctionTestHelper::ValidateStringResult(result, expected, 3);
    delete strVec;
    delete result;
}
