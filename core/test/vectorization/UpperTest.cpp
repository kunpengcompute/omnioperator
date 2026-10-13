/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: upper function unit tests (StringView vectorized path).
 *   upper(string) -> string. ASCII fast path; UTF-8 code points follow Spark upper (π -> Π).
 *   These tests target the {OMNI_STRING_VIEW}->OMNI_VARCHAR path (SV-in / VARCHAR-out).
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

class UpperTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const upper_test_env =
    ::testing::AddGlobalTestEnvironment(new UpperTestEnvironment);

class UpperFunctionTestHelper {
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

    // Builds a StringView (16B fixed-width) input column. Values <= 12 bytes are stored inline;
    // longer values are stored out-of-line (prefix + heap pointer into `values`, which must outlive
    // execution). Mirrors FillStringViewVector in the microbench.
    static BaseVector* CreateStringViewVector(const std::vector<std::string>& values) {
        auto* vec = new Vector<StringView>(values.size());
        vec->SetIsField(true);
        for (size_t i = 0; i < values.size(); ++i) {
            vec->SetValue(i, StringView(values[i]));
        }
        return vec;
    }

    // Asserts the {OMNI_STRING_VIEW}->OMNI_VARCHAR overload is selected (SV-in / VARCHAR-out).
    static void ExecuteUpperSV(BaseVector* stringVec, BaseVector*& result) {
        ASSERT_EQ(stringVec->GetTypeId(), OMNI_STRING_VIEW) << "input must be a StringView column";
        std::vector<DataTypeId> inputTypeIds = {OMNI_STRING_VIEW};
        auto sig = std::make_shared<FunctionSignature>("upper", inputTypeIds, OMNI_VARCHAR);
        auto fn = VectorFunction::Find(sig);
        ASSERT_NE(fn, nullptr) << "upper(StringView) overload not found — SV registration missing";
        auto outputType = std::make_shared<DataType>(OMNI_VARCHAR);
        ExecutionContext ctx(4096, nullptr);
        ctx.SetResultRowSize(stringVec->GetSize());
        std::stack<BaseVector*> args;
        args.push(stringVec);
        ASSERT_NO_THROW(fn->Apply(args, outputType, result, &ctx));
    }
};

TEST(UpperTest, SVBasicAsciiInline) {
    // all values <= 12 bytes -> stored inline in the 16B StringView
    std::vector<std::string> strings = {"abcdefg", "hello", "world"};
    std::vector<std::string> expected = {"ABCDEFG", "HELLO", "WORLD"};
    BaseVector* strVec = UpperFunctionTestHelper::CreateStringViewVector(strings);
    BaseVector* result = nullptr;
    UpperFunctionTestHelper::ExecuteUpperSV(strVec, result);
    UpperFunctionTestHelper::ValidateStringResult(result, expected, 3);
    delete strVec;
    delete result;
}

TEST(UpperTest, SVNonInlineAndMixed) {
    // all values > 12 bytes -> stored out-of-line (4-byte prefix + heap pointer)
    std::vector<std::string> strings = {"abcdefghijklmnop", "hello_world_abcde", "MixedCaseLongString"};
    std::vector<std::string> expected = {"ABCDEFGHIJKLMNOP", "HELLO_WORLD_ABCDE", "MIXEDCASELONGSTRING"};
    BaseVector* strVec = UpperFunctionTestHelper::CreateStringViewVector(strings);
    BaseVector* result = nullptr;
    UpperFunctionTestHelper::ExecuteUpperSV(strVec, result);
    UpperFunctionTestHelper::ValidateStringResult(result, expected, 3);
    delete strVec;
    delete result;
}

TEST(UpperTest, SVNonLetterBytesUnchanged) {
    std::vector<std::string> strings = {"123 abc 456", "!@#xyz$%"};
    std::vector<std::string> expected = {"123 ABC 456", "!@#XYZ$%"};
    BaseVector* strVec = UpperFunctionTestHelper::CreateStringViewVector(strings);
    BaseVector* result = nullptr;
    UpperFunctionTestHelper::ExecuteUpperSV(strVec, result);
    UpperFunctionTestHelper::ValidateStringResult(result, expected, 2);
    delete strVec;
    delete result;
}

TEST(UpperTest, SVUtf8Upper) {
    // é (U+00E9) -> É, π (U+03C0) -> Π. Uncased characters stay as-is.
    std::vector<std::string> strings = {u8"café hello world", u8"中文 abc", u8"○π△"};
    std::vector<std::string> expected = {u8"CAFÉ HELLO WORLD", u8"中文 ABC", u8"○Π△"};
    BaseVector* strVec = UpperFunctionTestHelper::CreateStringViewVector(strings);
    BaseVector* result = nullptr;
    UpperFunctionTestHelper::ExecuteUpperSV(strVec, result);
    UpperFunctionTestHelper::ValidateStringResult(result, expected, 3);
    delete strVec;
    delete result;
}

TEST(UpperTest, SVNullPropagation) {
    std::vector<std::string> strings = {"abc", "xyz", "hi", "bye"};
    BaseVector* strVec = UpperFunctionTestHelper::CreateStringViewVector(strings);
    strVec->SetNull(1);
    strVec->SetNull(3);
    BaseVector* result = nullptr;
    UpperFunctionTestHelper::ExecuteUpperSV(strVec, result);
    auto* resultVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    ASSERT_NE(resultVec, nullptr);
    EXPECT_EQ(resultVec->GetValue(0), "ABC");
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";
    EXPECT_EQ(resultVec->GetValue(2), "HI");
    EXPECT_TRUE(resultVec->IsNull(3)) << "Row 3 should be NULL";
    delete strVec;
    delete result;
}

TEST(UpperTest, SVEmptyAndBoundary) {
    std::vector<std::string> strings = {"", "a", "A", "z", "Z"};
    std::vector<std::string> expected = {"", "A", "A", "Z", "Z"};
    BaseVector* strVec = UpperFunctionTestHelper::CreateStringViewVector(strings);
    BaseVector* result = nullptr;
    UpperFunctionTestHelper::ExecuteUpperSV(strVec, result);
    UpperFunctionTestHelper::ValidateStringResult(result, expected, 5);
    delete strVec;
    delete result;
}
