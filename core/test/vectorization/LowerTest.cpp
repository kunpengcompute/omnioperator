/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: lower function unit tests.
 *   lower(string) -> string. Converts string to lowercase.
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

class LowerTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const lower_test_env =
    ::testing::AddGlobalTestEnvironment(new LowerTestEnvironment);

class LowerFunctionTestHelper {
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

    static void ExecuteLowerWithInputType(BaseVector* stringVec, DataTypeId inputTypeId, BaseVector*& result) {
        std::vector<DataTypeId> inputTypeIds = {inputTypeId};
        auto sig = std::make_shared<FunctionSignature>("lower", inputTypeIds, OMNI_VARCHAR);
        auto fn = VectorFunction::Find(sig);
        ASSERT_NE(fn, nullptr) << "lower not found";
        auto outputType = std::make_shared<DataType>(OMNI_VARCHAR);
        ExecutionContext ctx(4096, nullptr);
        ctx.SetResultRowSize(stringVec->GetSize());
        std::stack<BaseVector*> args;
        args.push(stringVec);
        ASSERT_NO_THROW(fn->Apply(args, outputType, result, &ctx));
    }

    static void ExecuteLower(BaseVector* stringVec, BaseVector*& result) {
        ExecuteLowerWithInputType(stringVec, OMNI_VARCHAR, result);
    }

    // Builds a StringView (16B fixed-width value) input column. Values <= 12 bytes are stored
    // inline; longer values are stored out-of-line (prefix + heap pointer into `values`, which
    // must outlive execution). Mirrors FillStringViewVector in the microbench.
    static BaseVector* CreateStringViewVector(const std::vector<std::string>& values) {
        auto* vec = new Vector<StringView>(values.size());
        vec->SetIsField(true);
        for (size_t i = 0; i < values.size(); ++i) {
            vec->SetValue(i, StringView(values[i]));
        }
        return vec;
    }

    // Executes lower on a StringView input column, asserting the StringView-specific registration
    // is what gets selected (SV-in / VARCHAR-out): input is OMNI_STRING_VIEW and VectorFunction::Find
    // resolves the {OMNI_STRING_VIEW}->OMNI_VARCHAR overload added in RegisterString.cpp.
    static void ExecuteLowerSV(BaseVector* stringVec, BaseVector*& result) {
        ASSERT_EQ(stringVec->GetTypeId(), OMNI_STRING_VIEW) << "input must be a StringView column";
        std::vector<DataTypeId> inputTypeIds = {OMNI_STRING_VIEW};
        auto sig = std::make_shared<FunctionSignature>("lower", inputTypeIds, OMNI_VARCHAR);
        auto fn = VectorFunction::Find(sig);
        ASSERT_NE(fn, nullptr) << "lower(StringView) overload not found — SV registration missing";
        auto outputType = std::make_shared<DataType>(OMNI_VARCHAR);
        ExecutionContext ctx(4096, nullptr);
        ctx.SetResultRowSize(stringVec->GetSize());
        std::stack<BaseVector*> args;
        args.push(stringVec);
        ASSERT_NO_THROW(fn->Apply(args, outputType, result, &ctx));
    }
};

TEST(LowerTest, BasicAscii) {
    std::vector<std::string> strings = {"ABCDEFG", "HELLO", "WORLD"};
    std::vector<std::string> expected = {"abcdefg", "hello", "world"};
    BaseVector* strVec = LowerFunctionTestHelper::CreateStringVector(strings);
    BaseVector* result = nullptr;
    LowerFunctionTestHelper::ExecuteLower(strVec, result);
    LowerFunctionTestHelper::ValidateStringResult(result, expected, 3);
    delete strVec;
    delete result;
}

TEST(LowerTest, AlreadyLower) {
    std::vector<std::string> strings = {"abcdefg", "hello", "world"};
    std::vector<std::string> expected = {"abcdefg", "hello", "world"};
    BaseVector* strVec = LowerFunctionTestHelper::CreateStringVector(strings);
    BaseVector* result = nullptr;
    LowerFunctionTestHelper::ExecuteLower(strVec, result);
    LowerFunctionTestHelper::ValidateStringResult(result, expected, 3);
    delete strVec;
    delete result;
}

TEST(LowerTest, MixedCase) {
    std::vector<std::string> strings = {"a B c D e F g", "Hello World", "AbCdEf"};
    std::vector<std::string> expected = {"a b c d e f g", "hello world", "abcdef"};
    BaseVector* strVec = LowerFunctionTestHelper::CreateStringVector(strings);
    BaseVector* result = nullptr;
    LowerFunctionTestHelper::ExecuteLower(strVec, result);
    LowerFunctionTestHelper::ValidateStringResult(result, expected, 3);
    delete strVec;
    delete result;
}

TEST(LowerTest, EmptyString) {
    std::vector<std::string> strings = {"", "ABC"};
    std::vector<std::string> expected = {"", "abc"};
    BaseVector* strVec = LowerFunctionTestHelper::CreateStringVector(strings);
    BaseVector* result = nullptr;
    LowerFunctionTestHelper::ExecuteLower(strVec, result);
    LowerFunctionTestHelper::ValidateStringResult(result, expected, 2);
    delete strVec;
    delete result;
}

TEST(LowerTest, NullPropagation) {
    std::vector<std::string> strings = {"ABC", "xyz", "Hi", "BYE"};
    BaseVector* strVec = LowerFunctionTestHelper::CreateStringVector(strings);
    strVec->SetNull(1);
    strVec->SetNull(3);
    BaseVector* result = nullptr;
    LowerFunctionTestHelper::ExecuteLower(strVec, result);
    auto* resultVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    ASSERT_NE(resultVec, nullptr);
    EXPECT_EQ(resultVec->GetValue(0), "abc");
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";
    EXPECT_EQ(resultVec->GetValue(2), "hi");
    EXPECT_TRUE(resultVec->IsNull(3)) << "Row 3 should be NULL";
    delete strVec;
    delete result;
}

TEST(LowerTest, NonLetterBytesUnchanged) {
    std::vector<std::string> strings = {"123 ABC 456", "!@#XYZ$%"};
    std::vector<std::string> expected = {"123 abc 456", "!@#xyz$%"};
    BaseVector* strVec = LowerFunctionTestHelper::CreateStringVector(strings);
    BaseVector* result = nullptr;
    LowerFunctionTestHelper::ExecuteLower(strVec, result);
    LowerFunctionTestHelper::ValidateStringResult(result, expected, 2);
    delete strVec;
    delete result;
}

TEST(LowerTest, GreekUnicode) {
    std::vector<std::string> strings = {
        u8"\u03A0",
        u8"\u03A0\u0391\u03A3",
        u8"\u03A0\u0391\u03A3\u0391",
        u8"A\u03A0B",
        u8"hello \u03A3",
        u8"hello\u03A3",
        u8"a\u0301\u03A3"};
    std::vector<std::string> expected = {
        u8"\u03C0",
        u8"\u03C0\u03B1\u03C2",
        u8"\u03C0\u03B1\u03C3\u03B1",
        u8"a\u03C0b",
        u8"hello \u03C3",
        u8"hello\u03C2",
        u8"a\u0301\u03C2"};
    BaseVector* strVec = LowerFunctionTestHelper::CreateStringVector(strings);
    BaseVector* result = nullptr;
    LowerFunctionTestHelper::ExecuteLower(strVec, result);
    LowerFunctionTestHelper::ValidateStringResult(result, expected, 7);
    delete strVec;
    delete result;
}

TEST(LowerTest, Boundary) {
    std::vector<std::string> strings = {"", "A", "a"};
    std::vector<std::string> expected = {"", "a", "a"};
    BaseVector* strVec = LowerFunctionTestHelper::CreateStringVector(strings);
    BaseVector* result = nullptr;
    LowerFunctionTestHelper::ExecuteLower(strVec, result);
    LowerFunctionTestHelper::ValidateStringResult(result, expected, 3);
    delete strVec;
    delete result;
}

TEST(LowerTest, UnicodeLatinAndCyrillic) {
    std::vector<std::string> strings = {
        u8"\u00C0\u00C1\u00C2\u00C3\u00C4\u00C5\u00C6\u00C7\u00C8\u00C9\u00CA\u00CB\u00CC\u00CD\u00CE\u00CF\u00D0\u00D1\u00D2\u00D3\u00D4\u00D5\u00D6\u00D8\u00D9\u00DA\u00DB\u00DC\u00DD\u00DE",
        u8"\u0410\u0411\u0412\u0413\u0414\u0415\u0416\u0417\u0418\u0419\u041A\u041B\u041C\u041D\u041E\u041F\u0420\u0421\u0422\u0423\u0424\u0425\u0426\u0427\u0428\u0429\u042A\u042B\u042C\u042D\u042E\u042F"};
    std::vector<std::string> expected = {
        u8"\u00E0\u00E1\u00E2\u00E3\u00E4\u00E5\u00E6\u00E7\u00E8\u00E9\u00EA\u00EB\u00EC\u00ED\u00EE\u00EF\u00F0\u00F1\u00F2\u00F3\u00F4\u00F5\u00F6\u00F8\u00F9\u00FA\u00FB\u00FC\u00FD\u00FE",
        u8"\u0430\u0431\u0432\u0433\u0434\u0435\u0436\u0437\u0438\u0439\u043A\u043B\u043C\u043D\u043E\u043F\u0440\u0441\u0442\u0443\u0444\u0445\u0446\u0447\u0448\u0449\u044A\u044B\u044C\u044D\u044E\u044F"};
    BaseVector* strVec = LowerFunctionTestHelper::CreateStringVector(strings);
    BaseVector* result = nullptr;
    LowerFunctionTestHelper::ExecuteLower(strVec, result);
    LowerFunctionTestHelper::ValidateStringResult(result, expected, 2);
    delete strVec;
    delete result;
}

TEST(LowerTest, UnicodeSpecialMappings) {
    std::vector<std::string> strings = {
        u8"\u0130",
        u8"I\u0307",
        u8"\u010A\u0116\u0120\u017B\u0226\u022E\u1E02\u1E0A\u1E1E\u1E22\u1E40\u1E44\u1E56\u1E58\u1E60\u1E64\u1E66\u1E68\u1E6A\u1E86\u1E8A\u1E8E"};
    std::vector<std::string> expected = {
        u8"i\u0307",
        u8"i\u0307",
        u8"\u010B\u0117\u0121\u017C\u0227\u022F\u1E03\u1E0B\u1E1F\u1E23\u1E41\u1E45\u1E57\u1E59\u1E61\u1E65\u1E67\u1E69\u1E6B\u1E87\u1E8B\u1E8F"};
    BaseVector* strVec = LowerFunctionTestHelper::CreateStringVector(strings);
    BaseVector* result = nullptr;
    LowerFunctionTestHelper::ExecuteLower(strVec, result);
    LowerFunctionTestHelper::ValidateStringResult(result, expected, 3);
    delete strVec;
    delete result;
}

TEST(LowerTest, GreekFinalSigmaContext) {
    std::vector<std::string> strings = {
        u8"\u03A0\u0391\u03A3 ",
        u8"\u03A0\u0391\u03A3.",
        u8"\u03A0\u0391\u03A3   A",
        u8"hello \u03A3",
        u8"hello \u03A3 world",
        u8"   \u03A3",
        u8"\u03A3",
        u8"ab\u03A3\u4E2D",
        u8"ab\u4E2D\u03A3\u4E2D",
        u8"\u03A3\u03A3\u03A3",
        u8"a\u03A3b\u03A3"};
    std::vector<std::string> expected = {
        u8"\u03C0\u03B1\u03C2 ",
        u8"\u03C0\u03B1\u03C2.",
        u8"\u03C0\u03B1\u03C2   a",
        u8"hello \u03C3",
        u8"hello \u03C3 world",
        u8"   \u03C3",
        u8"\u03C3",
        u8"ab\u03C2\u4E2D",
        u8"ab\u4E2D\u03C3\u4E2D",
        u8"\u03C3\u03C3\u03C2",
        u8"a\u03C3b\u03C2"};
    BaseVector* strVec = LowerFunctionTestHelper::CreateStringVector(strings);
    BaseVector* result = nullptr;
    LowerFunctionTestHelper::ExecuteLower(strVec, result);
    LowerFunctionTestHelper::ValidateStringResult(result, expected, 11);
    delete strVec;
    delete result;
}

TEST(LowerTest, CharInputSignature) {
    std::vector<std::string> strings = {"ABC", u8"\u0130", u8"\u03A0\u0391\u03A3"};
    std::vector<std::string> expected = {"abc", u8"i\u0307", u8"\u03C0\u03B1\u03C2"};
    BaseVector* strVec = LowerFunctionTestHelper::CreateStringVector(strings);
    BaseVector* result = nullptr;
    LowerFunctionTestHelper::ExecuteLowerWithInputType(strVec, OMNI_CHAR, result);
    LowerFunctionTestHelper::ValidateStringResult(result, expected, 3);
    delete strVec;
    delete result;
}

TEST(LowerTest, InvalidUtf8PreservesRemainingBytes) {
    std::string invalid = u8"\u03A8";
    invalid.append("\xFF\xFF", 2);
    invalid.append(u8"\u03A3\u0393\u0394A");
    std::string expectedInvalid = u8"\u03C8";
    expectedInvalid.append("\xFF\xFF", 2);
    expectedInvalid.append(u8"\u03A3\u0393\u0394A");
    std::vector<std::string> strings = {invalid};
    std::vector<std::string> expected = {expectedInvalid};
    BaseVector* strVec = LowerFunctionTestHelper::CreateStringVector(strings);
    BaseVector* result = nullptr;
    LowerFunctionTestHelper::ExecuteLower(strVec, result);
    LowerFunctionTestHelper::ValidateStringResult(result, expected, 1);
    delete strVec;
    delete result;
}

// ---------------- StringView (SV-in / VARCHAR-out) variants ----------------

#ifdef STRINGVIEW_ENABLE
TEST(LowerTest, SVBasicAsciiInline) {
    // all values <= 12 bytes -> stored inline in the 16B StringView
    std::vector<std::string> strings = {"ABCDEFG", "HELLO", "WORLD"};
    std::vector<std::string> expected = {"abcdefg", "hello", "world"};
    BaseVector* strVec = LowerFunctionTestHelper::CreateStringViewVector(strings);
    BaseVector* result = nullptr;
    LowerFunctionTestHelper::ExecuteLowerSV(strVec, result);
    LowerFunctionTestHelper::ValidateStringResult(result, expected, 3);
    delete strVec;
    delete result;
}

TEST(LowerTest, SVNonInline) {
    // all values > 12 bytes -> stored out-of-line (4-byte prefix + heap pointer)
    std::vector<std::string> strings = {"ABCDEFGHIJKLMNOP", "HELLO_WORLD_ABCDE", "MixedCaseLongString"};
    std::vector<std::string> expected = {"abcdefghijklmnop", "hello_world_abcde", "mixedcaselongstring"};
    BaseVector* strVec = LowerFunctionTestHelper::CreateStringViewVector(strings);
    BaseVector* result = nullptr;
    LowerFunctionTestHelper::ExecuteLowerSV(strVec, result);
    LowerFunctionTestHelper::ValidateStringResult(result, expected, 3);
    delete strVec;
    delete result;
}

TEST(LowerTest, SVNullPropagation) {
    std::vector<std::string> strings = {"ABC", "xyz", "Hi", "BYE"};
    BaseVector* strVec = LowerFunctionTestHelper::CreateStringViewVector(strings);
    strVec->SetNull(1);
    strVec->SetNull(3);
    BaseVector* result = nullptr;
    LowerFunctionTestHelper::ExecuteLowerSV(strVec, result);
    auto* resultVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    ASSERT_NE(resultVec, nullptr);
    EXPECT_EQ(resultVec->GetValue(0), "abc");
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";
    EXPECT_EQ(resultVec->GetValue(2), "hi");
    EXPECT_TRUE(resultVec->IsNull(3)) << "Row 3 should be NULL";
    delete strVec;
    delete result;
}

TEST(LowerTest, SVGreekUnicodeNonInline) {
    // multi-byte UTF-8, > 12 bytes -> exercises non-inline SV read + unicode delegation
    // (final-sigma semantics identical to the VARCHAR path; both delegate to the same call()).
    std::vector<std::string> strings = {u8"ΠΑΣΑ HELLO", u8"hello Σ WORLD"};
    std::vector<std::string> expected = {u8"πασα hello", u8"hello σ world"};
    BaseVector* strVec = LowerFunctionTestHelper::CreateStringViewVector(strings);
    BaseVector* result = nullptr;
    LowerFunctionTestHelper::ExecuteLowerSV(strVec, result);
    LowerFunctionTestHelper::ValidateStringResult(result, expected, 2);
    delete strVec;
    delete result;
}
#endif
