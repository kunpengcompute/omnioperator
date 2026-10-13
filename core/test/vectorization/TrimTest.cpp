/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: Trim function unit tests
 *   Trim(string) -> string  (trim leading/trailing space only, per TrimFunction)
 *   Trim(trimStr, string) -> string  (trim leading/trailing chars in trimStr, per TrimWithCharsFunction)
 */

#include <gtest/gtest.h>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <stdexcept>

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

class TrimTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const trim_test_env =
    ::testing::AddGlobalTestEnvironment(new TrimTestEnvironment);

class TrimFunctionTestHelper {
public:
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

    static void ValidateStringResult(BaseVector* result,
                                    const std::vector<std::string>& expected,
                                    int rowSize) {
        auto* resultVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
        ASSERT_NE(resultVec, nullptr) << "Result vector type mismatch";
        for (int i = 0; i < rowSize; ++i) {
            if (result->IsNull(i)) {
                EXPECT_TRUE(expected[static_cast<size_t>(i)].empty() || expected[static_cast<size_t>(i)] == "<null>")
                    << "Row " << i << " result is NULL";
                continue;
            }
            std::string_view actual = resultVec->GetValue(i);
            std::string actualStr(actual);
            const std::string& exp = expected[static_cast<size_t>(i)];
            EXPECT_EQ(actualStr, exp) << "Row " << i << " expected=\"" << exp << "\" actual=\"" << actualStr << "\"";
        }
    }

    // Use OMNI_VARCHAR in signature: Trim is registered for OMNI_VARCHAR; CreateStringVector may return OMNI_CHAR.
    static void ExecuteTrimOneArg(BaseVector* strVec, BaseVector*& result) {
        std::vector<DataTypeId> inputTypeIds = { OMNI_VARCHAR };
        auto sig = std::make_shared<FunctionSignature>("Trim", inputTypeIds, OMNI_VARCHAR);
        auto fn = VectorFunction::Find(sig);
        ASSERT_NE(fn, nullptr) << "Trim(string) function not found";
        auto outputType = std::make_shared<DataType>(OMNI_VARCHAR);
        ExecutionContext ctx(4096, nullptr);
        ctx.SetResultRowSize(strVec->GetSize());
        std::stack<BaseVector*> args;
        args.push(strVec);
        ASSERT_NO_THROW(fn->Apply(args, outputType, result, &ctx));
    }

    // TrimWithCharsFunction(trimStr, str): first arg = trimStr, second = str. Use OMNI_VARCHAR for lookup.
    static void ExecuteTrimTwoArgs(BaseVector* strVec, BaseVector* trimStrVec, BaseVector*& result) {
        std::vector<DataTypeId> inputTypeIds = { OMNI_VARCHAR, OMNI_VARCHAR };
        auto sig = std::make_shared<FunctionSignature>("Trim", inputTypeIds, OMNI_VARCHAR);
        auto fn = VectorFunction::Find(sig);
        ASSERT_NE(fn, nullptr) << "Trim(trimStr, string) function not found";
        auto outputType = std::make_shared<DataType>(OMNI_VARCHAR);
        ExecutionContext ctx(4096, nullptr);
        ctx.SetResultRowSize(strVec->GetSize());
        std::stack<BaseVector*> args;
        args.push(trimStrVec);
        args.push(strVec);
        ASSERT_NO_THROW(fn->Apply(args, outputType, result, &ctx));
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

    // 1-arg SV: fnName in {"Trim","LTrim","RTrim"}. Asserts the {OMNI_STRING_VIEW}->OMNI_VARCHAR
    // overload is selected (SV-in / VARCHAR-out).
    static void ExecuteOneArgSV(const std::string& fnName, BaseVector* strVec, BaseVector*& result) {
        ASSERT_EQ(strVec->GetTypeId(), OMNI_STRING_VIEW) << "input must be a StringView column";
        std::vector<DataTypeId> inputTypeIds = { OMNI_STRING_VIEW };
        auto sig = std::make_shared<FunctionSignature>(fnName, inputTypeIds, OMNI_VARCHAR);
        auto fn = VectorFunction::Find(sig);
        ASSERT_NE(fn, nullptr) << fnName << "(StringView) overload not found — SV registration missing";
        auto outputType = std::make_shared<DataType>(OMNI_VARCHAR);
        ExecutionContext ctx(4096, nullptr);
        ctx.SetResultRowSize(strVec->GetSize());
        std::stack<BaseVector*> args;
        args.push(strVec);
        ASSERT_NO_THROW(fn->Apply(args, outputType, result, &ctx));
    }

    // 2-arg SV: Trim(trimStr, str). str is the SV column; trimStrTypeId selects the {SV,SV} or {VARCHAR,SV}
    // overload (trimStrVec's element type must match trimStrTypeId).
    static void ExecuteTrimTwoArgsSV(BaseVector* strVec, BaseVector* trimStrVec, DataTypeId trimStrTypeId,
        BaseVector*& result) {
        ASSERT_EQ(strVec->GetTypeId(), OMNI_STRING_VIEW) << "str arg must be a StringView column";
        std::vector<DataTypeId> inputTypeIds = { trimStrTypeId, OMNI_STRING_VIEW };
        auto sig = std::make_shared<FunctionSignature>("Trim", inputTypeIds, OMNI_VARCHAR);
        auto fn = VectorFunction::Find(sig);
        ASSERT_NE(fn, nullptr) << "Trim({trimStr, SV}) overload not found — SV registration missing";
        auto outputType = std::make_shared<DataType>(OMNI_VARCHAR);
        ExecutionContext ctx(4096, nullptr);
        ctx.SetResultRowSize(strVec->GetSize());
        std::stack<BaseVector*> args;
        args.push(trimStrVec);
        args.push(strVec);
        ASSERT_NO_THROW(fn->Apply(args, outputType, result, &ctx));
    }

    // ---- SV-out (SV-in / SV-OUT, zero-copy sub-view) helpers ----
    // 1-arg fnName in {"Trim","LTrim","RTrim"}; result is OMNI_STRING_VIEW. Apply() frees the pushed
    // input vector internally, so callers must NOT delete strVec; validating after Apply also proves
    // the zero-copy output retained the input's string buffer.
    static void ExecuteOneArgSVOut(const std::string& fnName, BaseVector* strVec, BaseVector*& result) {
        ASSERT_EQ(strVec->GetTypeId(), OMNI_STRING_VIEW) << "input must be a StringView column";
        std::vector<DataTypeId> inputTypeIds = { OMNI_STRING_VIEW };
        auto sig = std::make_shared<FunctionSignature>(fnName, inputTypeIds, OMNI_STRING_VIEW);
        auto fn = VectorFunction::Find(sig);
        ASSERT_NE(fn, nullptr) << fnName << " SV-out {SV}->OMNI_STRING_VIEW not found";
        auto outputType = std::make_shared<DataType>(OMNI_STRING_VIEW);
        ExecutionContext ctx(4096, nullptr);
        ctx.SetResultRowSize(strVec->GetSize());
        std::stack<BaseVector*> args;
        args.push(strVec);
        ASSERT_NO_THROW(fn->Apply(args, outputType, result, &ctx));
    }

    static void ValidateStringViewResult(BaseVector* result, const std::vector<std::string>& expected,
                                         int rowSize) {
        ASSERT_NE(result, nullptr);
        ASSERT_EQ(result->GetTypeId(), OMNI_STRING_VIEW) << "SV-out result must be a StringView vector";
        auto* resultVec = dynamic_cast<Vector<StringView>*>(result);
        ASSERT_NE(resultVec, nullptr) << "result is not Vector<StringView>";
        for (int i = 0; i < rowSize; ++i) {
            if (result->IsNull(i)) {
                continue;
            }
            const StringView& sv = resultVec->GetValueRef(i);
            std::string actual(sv.data(), sv.size());
            EXPECT_EQ(actual, expected[static_cast<size_t>(i)])
                << "Row " << i << " expected=\"" << expected[static_cast<size_t>(i)]
                << "\" actual=\"" << actual << "\"";
        }
    }
};

// --- TrimFunction (single arg: trim space only) ---
// String.h TrimFunction uses find_first_not_of(" ") / find_last_not_of(" "), so only space is trimmed.

TEST(TrimTest, SingleArgTrimSpace) {
    std::vector<std::string> strings = {
        "  hello world  ",
        "   ",
        "no_spaces",
        "",
        " \t leading and trailing "
    };
    std::vector<std::string> expected = {
        "hello world",
        "",
        "no_spaces",
        "",
        "\t leading and trailing"   // only trailing space trimmed; leading \t not trimmed
    };
    std::unique_ptr<BaseVector> strVec(TrimFunctionTestHelper::CreateStringVector(strings));
    BaseVector* result = nullptr;
    TrimFunctionTestHelper::ExecuteTrimOneArg(strVec.get(), result);
    std::unique_ptr<BaseVector> resultHolder(result);
    TrimFunctionTestHelper::ValidateStringResult(resultHolder.get(), expected, static_cast<int>(strings.size()));
}

TEST(TrimTest, SingleArgEdgeCases) {
    std::vector<std::string> strings = {
        "  leading only",
        "trailing only  ",
        "  both  ",
        "single_char",
        "\t\ntest\n\t"   // TrimFunction only trims space, so tab/newline remain
    };
    std::vector<std::string> expected = {
        "leading only",
        "trailing only",
        "both",
        "single_char",
        "\t\ntest\n\t"
    };
    std::unique_ptr<BaseVector> strVec(TrimFunctionTestHelper::CreateStringVector(strings));
    BaseVector* result = nullptr;
    TrimFunctionTestHelper::ExecuteTrimOneArg(strVec.get(), result);
    std::unique_ptr<BaseVector> resultHolder(result);
    TrimFunctionTestHelper::ValidateStringResult(resultHolder.get(), expected, static_cast<int>(strings.size()));
}

TEST(TrimTest, SingleArgConstVector) {
    int rowSize = 3;
    std::string_view constVal("  constant value  ");
    std::unique_ptr<BaseVector> inputVec(new ConstVector<std::string_view>(constVal, OMNI_VARCHAR, rowSize));
    inputVec->SetIsField(true);  // Caller owns; prevent ConstVectorReader from deleting in Apply()
    std::vector<std::string> expected(rowSize, "constant value");
    BaseVector* result = nullptr;
    TrimFunctionTestHelper::ExecuteTrimOneArg(inputVec.get(), result);
    std::unique_ptr<BaseVector> resultHolder(result);
    TrimFunctionTestHelper::ValidateStringResult(resultHolder.get(), expected, rowSize);
}

// --- TrimWithCharsFunction (two args: trim chars in trimStr from string) ---

TEST(TrimTest, TwoArgsTrimChars) {
    std::vector<std::string> strings = {
        "xxxyyhelloyyxx",
        "ababab",
        "xyz",
        "",
        "abcd"
    };
    std::vector<std::string> trimStrs = {
        "xy",
        "ab",
        "xyz",
        "a",
        "abcd"
    };
    std::vector<std::string> expected = {
        "hello",
        "",
        "",
        "",
        ""
    };
    std::unique_ptr<BaseVector> strVec(TrimFunctionTestHelper::CreateStringVector(strings));
    std::unique_ptr<BaseVector> trimStrVec(TrimFunctionTestHelper::CreateStringVector(trimStrs));
    BaseVector* result = nullptr;
    TrimFunctionTestHelper::ExecuteTrimTwoArgs(strVec.get(), trimStrVec.get(), result);
    std::unique_ptr<BaseVector> resultHolder(result);
    TrimFunctionTestHelper::ValidateStringResult(resultHolder.get(), expected, static_cast<int>(strings.size()));
}

TEST(TrimTest, TwoArgsEmptyTrimStr) {
    // Empty trimStr: find_first_not_of("") returns 0, so full string returned.
    std::vector<std::string> strings = { "  hello  ", "data", "" };
    std::vector<std::string> trimStrs = { "", "", "" };
    std::vector<std::string> expected = { "  hello  ", "data", "" };
    std::unique_ptr<BaseVector> strVec(TrimFunctionTestHelper::CreateStringVector(strings));
    std::unique_ptr<BaseVector> trimStrVec(TrimFunctionTestHelper::CreateStringVector(trimStrs));
    BaseVector* result = nullptr;
    TrimFunctionTestHelper::ExecuteTrimTwoArgs(strVec.get(), trimStrVec.get(), result);
    std::unique_ptr<BaseVector> resultHolder(result);
    TrimFunctionTestHelper::ValidateStringResult(resultHolder.get(), expected, 3);
}

TEST(TrimTest, TwoArgsSingleCharTrim) {
    std::vector<std::string> strings = { "zzhellozz", "aaa", "bbb" };
    std::vector<std::string> trimStrs = { "z", "a", "b" };
    std::vector<std::string> expected = { "hello", "", "" };
    std::unique_ptr<BaseVector> strVec(TrimFunctionTestHelper::CreateStringVector(strings));
    std::unique_ptr<BaseVector> trimStrVec(TrimFunctionTestHelper::CreateStringVector(trimStrs));
    BaseVector* result = nullptr;
    TrimFunctionTestHelper::ExecuteTrimTwoArgs(strVec.get(), trimStrVec.get(), result);
    std::unique_ptr<BaseVector> resultHolder(result);
    TrimFunctionTestHelper::ValidateStringResult(resultHolder.get(), expected, 3);
}

TEST(TrimTest, TwoArgsTrimSpace) {
    // trimStr = " " should behave like single-arg trim for space
    std::vector<std::string> strings = { "  both  ", " x ", "a b" };
    std::vector<std::string> trimStrs = { " ", " ", " " };
    std::vector<std::string> expected = { "both", "x", "a b" };
    std::unique_ptr<BaseVector> strVec(TrimFunctionTestHelper::CreateStringVector(strings));
    std::unique_ptr<BaseVector> trimStrVec(TrimFunctionTestHelper::CreateStringVector(trimStrs));
    BaseVector* result = nullptr;
    TrimFunctionTestHelper::ExecuteTrimTwoArgs(strVec.get(), trimStrVec.get(), result);
    std::unique_ptr<BaseVector> resultHolder(result);
    TrimFunctionTestHelper::ValidateStringResult(resultHolder.get(), expected, 3);
}

// ---------------- StringView (SV-in / VARCHAR-out) variants ----------------

#ifdef STRINGVIEW_ENABLE
TEST(TrimTest, SVSingleArgTrimSpace) {
    // mix of inline (<=12B) and non-inline (>12B) StringView values
    std::vector<std::string> strings = {
        "  hello world  ",                 // 15B non-inline -> "hello world"
        "   ",                             // inline -> ""
        "no_spaces",                       // inline unchanged
        "",                                // inline empty
        "  a longer non-inline string  "   // non-inline -> "a longer non-inline string"
    };
    std::vector<std::string> expected = {
        "hello world", "", "no_spaces", "", "a longer non-inline string"
    };
    std::unique_ptr<BaseVector> strVec(TrimFunctionTestHelper::CreateStringViewVector(strings));
    BaseVector* result = nullptr;
    TrimFunctionTestHelper::ExecuteOneArgSV("Trim", strVec.get(), result);
    std::unique_ptr<BaseVector> resultHolder(result);
    TrimFunctionTestHelper::ValidateStringResult(resultHolder.get(), expected, static_cast<int>(strings.size()));
}

TEST(TrimTest, SVLTrimLeadingSpace) {
    std::vector<std::string> strings = { "  leading and trailing spaces  ", "  x  ", "none" };
    std::vector<std::string> expected = { "leading and trailing spaces  ", "x  ", "none" };
    std::unique_ptr<BaseVector> strVec(TrimFunctionTestHelper::CreateStringViewVector(strings));
    BaseVector* result = nullptr;
    TrimFunctionTestHelper::ExecuteOneArgSV("LTrim", strVec.get(), result);
    std::unique_ptr<BaseVector> resultHolder(result);
    TrimFunctionTestHelper::ValidateStringResult(resultHolder.get(), expected, static_cast<int>(strings.size()));
}

TEST(TrimTest, SVRTrimTrailingSpace) {
    std::vector<std::string> strings = { "  leading and trailing spaces  ", "  x  ", "none" };
    std::vector<std::string> expected = { "  leading and trailing spaces", "  x", "none" };
    std::unique_ptr<BaseVector> strVec(TrimFunctionTestHelper::CreateStringViewVector(strings));
    BaseVector* result = nullptr;
    TrimFunctionTestHelper::ExecuteOneArgSV("RTrim", strVec.get(), result);
    std::unique_ptr<BaseVector> resultHolder(result);
    TrimFunctionTestHelper::ValidateStringResult(resultHolder.get(), expected, static_cast<int>(strings.size()));
}

TEST(TrimTest, SVNullPropagation) {
    std::vector<std::string> strings = { "  a  ", "  b  ", "  c  " };
    std::unique_ptr<BaseVector> strVec(TrimFunctionTestHelper::CreateStringViewVector(strings));
    strVec->SetNull(1);
    BaseVector* result = nullptr;
    TrimFunctionTestHelper::ExecuteOneArgSV("Trim", strVec.get(), result);
    std::unique_ptr<BaseVector> resultHolder(result);
    auto* resultVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(resultHolder.get());
    ASSERT_NE(resultVec, nullptr);
    EXPECT_EQ(resultVec->GetValue(0), "a");
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";
    EXPECT_EQ(resultVec->GetValue(2), "c");
}

TEST(TrimTest, SVTwoArgsVarcharTrimStr) {
    // str is the SV column; trimStr is a VARCHAR literal-like column -> {VARCHAR, SV} overload
    std::vector<std::string> strings = { "xxxyyhelloyyxx", "ababab", "xxlonger_non_inline_xx" };
    std::vector<std::string> trimStrs = { "xy", "ab", "x" };
    std::vector<std::string> expected = { "hello", "", "longer_non_inline_" };
    std::unique_ptr<BaseVector> strVec(TrimFunctionTestHelper::CreateStringViewVector(strings));
    std::unique_ptr<BaseVector> trimStrVec(TrimFunctionTestHelper::CreateStringVector(trimStrs));
    BaseVector* result = nullptr;
    TrimFunctionTestHelper::ExecuteTrimTwoArgsSV(strVec.get(), trimStrVec.get(), OMNI_VARCHAR, result);
    std::unique_ptr<BaseVector> resultHolder(result);
    TrimFunctionTestHelper::ValidateStringResult(resultHolder.get(), expected, static_cast<int>(strings.size()));
}

TEST(TrimTest, SVTwoArgsBothStringView) {
    // both str and trimStr are SV columns -> {SV, SV} overload
    std::vector<std::string> strings = { "xxxyyhelloyyxx", "ababab", "xxlonger_non_inline_xx" };
    std::vector<std::string> trimStrs = { "xy", "ab", "x" };
    std::vector<std::string> expected = { "hello", "", "longer_non_inline_" };
    std::unique_ptr<BaseVector> strVec(TrimFunctionTestHelper::CreateStringViewVector(strings));
    std::unique_ptr<BaseVector> trimStrVec(TrimFunctionTestHelper::CreateStringViewVector(trimStrs));
    BaseVector* result = nullptr;
    TrimFunctionTestHelper::ExecuteTrimTwoArgsSV(strVec.get(), trimStrVec.get(), OMNI_STRING_VIEW, result);
    std::unique_ptr<BaseVector> resultHolder(result);
    TrimFunctionTestHelper::ValidateStringResult(resultHolder.get(), expected, static_cast<int>(strings.size()));
}

// ---------------- StringView SV-out (SV-in / SV-OUT, zero-copy sub-view) ----------------
// Apply() frees the pushed input vector internally; tests must NOT delete strVec (no unique_ptr on it).

TEST(TrimTest, SVOutTrimSpace) {
    // mix inline + non-inline (>12B) results to exercise arena aliasing after input freed
    std::vector<std::string> strings = {
        "  hello  ", "   ", "  a longer non-inline string value  ", "no_spaces"
    };
    std::vector<std::string> expected = {"hello", "", "a longer non-inline string value", "no_spaces"};
    BaseVector* strVec = TrimFunctionTestHelper::CreateStringViewVector(strings);
    BaseVector* result = nullptr;
    TrimFunctionTestHelper::ExecuteOneArgSVOut("Trim", strVec, result);
    std::unique_ptr<BaseVector> resultHolder(result);
    TrimFunctionTestHelper::ValidateStringViewResult(resultHolder.get(), expected, static_cast<int>(strings.size()));
}

TEST(TrimTest, SVOutLTrim) {
    std::vector<std::string> strings = {"  leading and trailing spaces  ", "  x  ", "none"};
    std::vector<std::string> expected = {"leading and trailing spaces  ", "x  ", "none"};
    BaseVector* strVec = TrimFunctionTestHelper::CreateStringViewVector(strings);
    BaseVector* result = nullptr;
    TrimFunctionTestHelper::ExecuteOneArgSVOut("LTrim", strVec, result);
    std::unique_ptr<BaseVector> resultHolder(result);
    TrimFunctionTestHelper::ValidateStringViewResult(resultHolder.get(), expected, static_cast<int>(strings.size()));
}

TEST(TrimTest, SVOutRTrim) {
    std::vector<std::string> strings = {"  leading and trailing spaces  ", "  x  ", "none"};
    std::vector<std::string> expected = {"  leading and trailing spaces", "  x", "none"};
    BaseVector* strVec = TrimFunctionTestHelper::CreateStringViewVector(strings);
    BaseVector* result = nullptr;
    TrimFunctionTestHelper::ExecuteOneArgSVOut("RTrim", strVec, result);
    std::unique_ptr<BaseVector> resultHolder(result);
    TrimFunctionTestHelper::ValidateStringViewResult(resultHolder.get(), expected, static_cast<int>(strings.size()));
}

TEST(TrimTest, SVOutNullPropagation) {
    std::vector<std::string> strings = {"  a  ", "  b  ", "  c  "};
    std::vector<std::string> expected = {"a", "", "c"};
    BaseVector* strVec = TrimFunctionTestHelper::CreateStringViewVector(strings);
    strVec->SetNull(1);
    BaseVector* result = nullptr;
    TrimFunctionTestHelper::ExecuteOneArgSVOut("Trim", strVec, result);
    std::unique_ptr<BaseVector> resultHolder(result);
    ASSERT_EQ(result->GetTypeId(), OMNI_STRING_VIEW);
    auto* resultVec = dynamic_cast<Vector<StringView>*>(result);
    ASSERT_NE(resultVec, nullptr);
    EXPECT_EQ(std::string(resultVec->GetValueRef(0).data(), resultVec->GetValueRef(0).size()), "a");
    EXPECT_TRUE(result->IsNull(1)) << "Row 1 should be NULL";
    EXPECT_EQ(std::string(resultVec->GetValueRef(2).data(), resultVec->GetValueRef(2).size()), "c");
}
#endif
