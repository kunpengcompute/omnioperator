/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: LIKE function unit tests.
 *   LIKE(string, pattern) -> boolean.
 *   Pattern: % = zero or more chars, _ = one char; default escape '\\', or LIKE(s, p, escape).
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
using namespace omniruntime::mem;
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::codegen;
using namespace omniruntime::TestUtil;

class LikeTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const like_test_env =
    ::testing::AddGlobalTestEnvironment(new LikeTestEnvironment);

class LikeFunctionTestHelper {
public:
    static void ValidateBooleanResult(BaseVector* result,
                                     const std::vector<bool>& expected,
                                     int rowSize) {
        auto* resultVec = dynamic_cast<Vector<bool>*>(result);
        ASSERT_NE(resultVec, nullptr) << "Result vector is not boolean type";
        for (int i = 0; i < rowSize; ++i) {
            if (result->IsNull(i)) {
                continue;
            }
            bool actualValue = resultVec->GetValue(i);
            bool expectedValue = expected[i];
            EXPECT_EQ(actualValue, expectedValue)
                << "Row " << i << " expected=" << (expectedValue ? "true" : "false")
                << " actual=" << (actualValue ? "true" : "false");
        }
    }

    static BaseVector* CreateStringVector(const std::vector<std::string>& values) {
        BaseVector* vec = VectorHelper::CreateStringVector(values.size());
        vec->SetIsField(true);
        auto* typedVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(vec);
        EXPECT_NE(typedVec, nullptr);
        for (size_t i = 0; i < values.size(); ++i) {
            std::string_view sv(values[i]);
            typedVec->SetValue(i, sv);
        }
        return vec;
    }

    static void ExecuteLike(BaseVector* strVec, BaseVector* patternVec, BaseVector*& result) {
        auto signature = std::make_shared<FunctionSignature>("LIKE",
            std::vector<DataTypeId>{OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_BOOLEAN);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "LIKE function not found for signature";

        auto outputType = std::make_shared<DataType>(OMNI_BOOLEAN);
        ExecutionContext context;
        context.SetResultRowSize(strVec->GetSize());
        std::stack<BaseVector*> args;
        // Apply pops last arg first: push (str, pattern) -> pop order (pattern, str)
        args.push(strVec);
        args.push(patternVec);

        ASSERT_NO_THROW(function->Apply(args, outputType, result, &context));
    }

    static void ExecuteLike3(BaseVector* strVec, BaseVector* patternVec, BaseVector* escapeVec, BaseVector*& result) {
        auto signature = std::make_shared<FunctionSignature>("LIKE",
            std::vector<DataTypeId>{OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_BOOLEAN);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "LIKE(str,pattern,escape) not found";

        auto outputType = std::make_shared<DataType>(OMNI_BOOLEAN);
        ExecutionContext context;
        context.SetResultRowSize(strVec->GetSize());
        std::stack<BaseVector*> args;
        args.push(strVec);
        args.push(patternVec);
        args.push(escapeVec);

        function->Apply(args, outputType, result, &context);
    }
};

TEST(LikeTest, BasicMatch) {
    std::vector<std::string> strValues = {"hello", "world", "test"};
    std::vector<std::string> patterns = {"hello", "world", "test"};
    std::vector<bool> expected = {true, true, true};

    BaseVector* strVec = LikeFunctionTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = LikeFunctionTestHelper::CreateStringVector(patterns);
    BaseVector* resultVec = nullptr;

    LikeFunctionTestHelper::ExecuteLike(strVec, patternVec, resultVec);
    LikeFunctionTestHelper::ValidateBooleanResult(resultVec, expected, strValues.size());

    delete resultVec;
}

TEST(LikeTest, NoMatch) {
    std::vector<std::string> strValues = {"hello", "world", "test"};
    std::vector<std::string> patterns = {"xyz", "abc", "def"};
    std::vector<bool> expected = {false, false, false};

    BaseVector* strVec = LikeFunctionTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = LikeFunctionTestHelper::CreateStringVector(patterns);
    BaseVector* resultVec = nullptr;

    LikeFunctionTestHelper::ExecuteLike(strVec, patternVec, resultVec);
    LikeFunctionTestHelper::ValidateBooleanResult(resultVec, expected, strValues.size());

    delete resultVec;
}

TEST(LikeTest, PercentSuffix) {
    std::vector<std::string> strValues = {"hello", "heat", "hex"};
    std::vector<std::string> patterns = {"he%", "he%", "he%"};
    std::vector<bool> expected = {true, true, true};

    BaseVector* strVec = LikeFunctionTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = LikeFunctionTestHelper::CreateStringVector(patterns);
    BaseVector* resultVec = nullptr;

    LikeFunctionTestHelper::ExecuteLike(strVec, patternVec, resultVec);
    LikeFunctionTestHelper::ValidateBooleanResult(resultVec, expected, strValues.size());

    delete resultVec;
}

TEST(LikeTest, PercentPrefix) {
    std::vector<std::string> strValues = {"hello", "world", "cold"};
    std::vector<std::string> patterns = {"%lo", "%ld", "%old"};
    std::vector<bool> expected = {true, true, true};

    BaseVector* strVec = LikeFunctionTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = LikeFunctionTestHelper::CreateStringVector(patterns);
    BaseVector* resultVec = nullptr;

    LikeFunctionTestHelper::ExecuteLike(strVec, patternVec, resultVec);
    LikeFunctionTestHelper::ValidateBooleanResult(resultVec, expected, strValues.size());

    delete resultVec;
}

TEST(LikeTest, PercentBoth) {
    std::vector<std::string> strValues = {"hello", "world", "abc"};
    std::vector<std::string> patterns = {"%el%", "%or%", "%b%"};
    std::vector<bool> expected = {true, true, true};

    BaseVector* strVec = LikeFunctionTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = LikeFunctionTestHelper::CreateStringVector(patterns);
    BaseVector* resultVec = nullptr;

    LikeFunctionTestHelper::ExecuteLike(strVec, patternVec, resultVec);
    LikeFunctionTestHelper::ValidateBooleanResult(resultVec, expected, strValues.size());

    delete resultVec;
}

TEST(LikeTest, ConstCallerContextContainsLiteralUnderscores) {
    std::vector<std::string> strValues = {
        "prefix/mr_attempt_1700000000000_42_m_7_8/suffix",
        "prefix/mrXattempt_1700000000000_42_m_7_8/suffix",
        "prefix/application_99_1_JId_2_SId_3_4/suffix"
    };
    const std::string pattern = "%mr\\_attempt\\_%";
    std::vector<bool> expected = {true, false, false};

    BaseVector* strVec = LikeFunctionTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = new ConstVector<std::string_view>(
        std::string_view(pattern), OMNI_VARCHAR, static_cast<int32_t>(strValues.size()));
    BaseVector* resultVec = nullptr;

    LikeFunctionTestHelper::ExecuteLike(strVec, patternVec, resultVec);
    LikeFunctionTestHelper::ValidateBooleanResult(resultVec, expected, strValues.size());

    delete resultVec;
}

TEST(LikeTest, ConstCallerContextPreservesUnderscoreWildcard) {
    std::vector<std::string> strValues = {
        "prefix/mr_attempt_1700000000000_42_m_7_8/suffix",
        "prefix/mrXattempt_1700000000000_42_m_7_8/suffix",
        "prefix/mrattempt_1700000000000_42_m_7_8/suffix"
    };
    // The first underscore is deliberately unescaped and therefore matches
    // exactly one code point, as in the SQL from the reported workload.
    const std::string pattern = "%mr_attempt\\_%";
    std::vector<bool> expected = {true, true, false};

    BaseVector* strVec = LikeFunctionTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = new ConstVector<std::string_view>(
        std::string_view(pattern), OMNI_VARCHAR, static_cast<int32_t>(strValues.size()));
    BaseVector* resultVec = nullptr;

    LikeFunctionTestHelper::ExecuteLike(strVec, patternVec, resultVec);
    LikeFunctionTestHelper::ValidateBooleanResult(resultVec, expected, strValues.size());

    delete resultVec;
}

TEST(LikeTest, UnderscoreWildcard) {
    std::vector<std::string> strValues = {"a", "ab", "abc", "x"};
    std::vector<std::string> patterns = {"_", "__", "___", "_"};
    std::vector<bool> expected = {true, true, true, true};

    BaseVector* strVec = LikeFunctionTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = LikeFunctionTestHelper::CreateStringVector(patterns);
    BaseVector* resultVec = nullptr;

    LikeFunctionTestHelper::ExecuteLike(strVec, patternVec, resultVec);
    LikeFunctionTestHelper::ValidateBooleanResult(resultVec, expected, strValues.size());

    delete resultVec;
}

TEST(LikeTest, EscapePercent) {
    std::vector<std::string> strValues = {"100%", "50%"};
    std::vector<std::string> patterns = {"100\\%", "50\\%"};
    std::vector<bool> expected = {true, true};

    BaseVector* strVec = LikeFunctionTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = LikeFunctionTestHelper::CreateStringVector(patterns);
    BaseVector* resultVec = nullptr;

    LikeFunctionTestHelper::ExecuteLike(strVec, patternVec, resultVec);
    LikeFunctionTestHelper::ValidateBooleanResult(resultVec, expected, strValues.size());

    delete resultVec;
}

TEST(LikeTest, EmptyString) {
    std::vector<std::string> strValues = {"", "", "a"};
    std::vector<std::string> patterns = {"%", "x", "%"};
    std::vector<bool> expected = {true, false, true};

    BaseVector* strVec = LikeFunctionTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = LikeFunctionTestHelper::CreateStringVector(patterns);
    BaseVector* resultVec = nullptr;

    LikeFunctionTestHelper::ExecuteLike(strVec, patternVec, resultVec);
    LikeFunctionTestHelper::ValidateBooleanResult(resultVec, expected, strValues.size());

    delete resultVec;
}

TEST(LikeTest, NullString) {
    std::vector<std::string> strValues = {"hello", "world", "test"};
    std::vector<std::string> patterns = {"he%", "w%", "t%"};

    BaseVector* strVec = LikeFunctionTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = LikeFunctionTestHelper::CreateStringVector(patterns);
    strVec->SetNull(1);

    BaseVector* resultVec = nullptr;
    LikeFunctionTestHelper::ExecuteLike(strVec, patternVec, resultVec);

    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL (string is NULL)";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";

    delete resultVec;
}

TEST(LikeTest, NullPattern) {
    std::vector<std::string> strValues = {"hello", "world", "test"};
    std::vector<std::string> patterns = {"he%", "w%", "t%"};

    BaseVector* strVec = LikeFunctionTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = LikeFunctionTestHelper::CreateStringVector(patterns);
    patternVec->SetNull(1);

    BaseVector* resultVec = nullptr;
    LikeFunctionTestHelper::ExecuteLike(strVec, patternVec, resultVec);

    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL (pattern is NULL)";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";

    delete resultVec;
}

TEST(LikeTest, EscapeThreeArg_CustomPercent) {
    std::vector<std::string> strValues = {"100%", "50%"};
    std::vector<std::string> patterns = {"100#%", "50#%"};
    std::vector<bool> expected = {true, true};

    BaseVector* strVec = LikeFunctionTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = LikeFunctionTestHelper::CreateStringVector(patterns);
    BaseVector* escapeVec = new ConstVector<std::string_view>(std::string_view("#"), OMNI_VARCHAR, 2);
    BaseVector* resultVec = nullptr;

    LikeFunctionTestHelper::ExecuteLike3(strVec, patternVec, escapeVec, resultVec);
    LikeFunctionTestHelper::ValidateBooleanResult(resultVec, expected, strValues.size());

    delete resultVec;
}

TEST(LikeTest, EscapeThreeArg_CustomUnderscore) {
    // Exact match for "a_b" / "x_y": literal chars after escaped underscore.
    std::vector<std::string> strValues = {"a_b", "x_y"};
    std::vector<std::string> patterns = {"a$_b", "x$_y"};
    std::vector<bool> expected = {true, true};

    BaseVector* strVec = LikeFunctionTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = LikeFunctionTestHelper::CreateStringVector(patterns);
    BaseVector* escapeVec = new ConstVector<std::string_view>(std::string_view("$"), OMNI_VARCHAR, 2);
    BaseVector* resultVec = nullptr;

    LikeFunctionTestHelper::ExecuteLike3(strVec, patternVec, escapeVec, resultVec);
    LikeFunctionTestHelper::ValidateBooleanResult(resultVec, expected, strValues.size());

    delete resultVec;
}

// a$_ is only 3 pattern chars: a, $, _. The pair $_ is one literal '_'; there is NO trailing wildcard.
// So it matches the 2-char string "a_" only — not "a_b". For "a_ + any one char" use a$__ (four pattern chars).
TEST(LikeTest, EscapeThreeArg_LiteralUnderscoreOnlyTwoChars) {
    std::vector<std::string> strValues = {"a_", "a_b"};
    std::vector<std::string> patterns = {"a$_", "a$_"};
    std::vector<bool> expected = {true, false};

    BaseVector* strVec = LikeFunctionTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = LikeFunctionTestHelper::CreateStringVector(patterns);
    BaseVector* escapeVec = new ConstVector<std::string_view>(std::string_view("$"), OMNI_VARCHAR, 2);
    BaseVector* resultVec = nullptr;

    LikeFunctionTestHelper::ExecuteLike3(strVec, patternVec, escapeVec, resultVec);
    LikeFunctionTestHelper::ValidateBooleanResult(resultVec, expected, strValues.size());

    delete resultVec;
}

TEST(LikeTest, EscapeThreeArg_LiteralUnderscoreThenWildcard) {
    std::vector<std::string> strValues = {"a_b", "a_", "x_y"};
    std::vector<std::string> patterns = {"a$__", "a$__", "x$__"};
    std::vector<bool> expected = {true, false, true};

    BaseVector* strVec = LikeFunctionTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = LikeFunctionTestHelper::CreateStringVector(patterns);
    BaseVector* escapeVec = new ConstVector<std::string_view>(std::string_view("$"), OMNI_VARCHAR, 3);
    BaseVector* resultVec = nullptr;

    LikeFunctionTestHelper::ExecuteLike3(strVec, patternVec, escapeVec, resultVec);
    LikeFunctionTestHelper::ValidateBooleanResult(resultVec, expected, strValues.size());

    delete resultVec;
}

TEST(LikeTest, EscapeThreeArg_BackslashSameAsTwoArg) {
    std::vector<std::string> strValues = {"100%", "x"};
    std::vector<std::string> patterns = {"100\\%", "x"};
    std::vector<bool> expected = {true, true};

    BaseVector* strVec = LikeFunctionTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = LikeFunctionTestHelper::CreateStringVector(patterns);
    BaseVector* escapeVec = new ConstVector<std::string_view>(std::string_view("\\", 1), OMNI_VARCHAR, 2);
    BaseVector* resultVec = nullptr;

    LikeFunctionTestHelper::ExecuteLike3(strVec, patternVec, escapeVec, resultVec);
    LikeFunctionTestHelper::ValidateBooleanResult(resultVec, expected, strValues.size());

    delete resultVec;
}

// Spark / Gluten may pass ESCAPE as CHAR(n): '/' right-padded with spaces — must accept after trim.
TEST(LikeTest, EscapeThreeArg_EscapeCharPaddedTrailingSpaces) {
    std::string escStorage = "/   ";
    std::vector<std::string> strValues = {"\xe6\xb0\xb4%"};  // 水 + %
    std::vector<std::string> patterns = {"\xe6\xb0\xb4/%"};     // 水/%  ( / escapes % )
    std::vector<bool> expected = {true};

    BaseVector* strVec = LikeFunctionTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = LikeFunctionTestHelper::CreateStringVector(patterns);
    BaseVector* escapeVec = new ConstVector<std::string_view>(
        std::string_view(escStorage.data(), escStorage.size()), OMNI_VARCHAR, 1);
    BaseVector* resultVec = nullptr;

    LikeFunctionTestHelper::ExecuteLike3(strVec, patternVec, escapeVec, resultVec);
    LikeFunctionTestHelper::ValidateBooleanResult(resultVec, expected, strValues.size());

    delete resultVec;
}

TEST(LikeTest, EscapeThreeArg_PerRowEscapeColumn) {
    std::vector<std::string> strValues = {"100%", "50$"};
    std::vector<std::string> patterns = {"100#%", "50$$"};
    std::vector<std::string> escapes = {"#", "$"};
    std::vector<bool> expected = {true, true};

    BaseVector* strVec = LikeFunctionTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = LikeFunctionTestHelper::CreateStringVector(patterns);
    BaseVector* escapeVec = LikeFunctionTestHelper::CreateStringVector(escapes);
    BaseVector* resultVec = nullptr;

    LikeFunctionTestHelper::ExecuteLike3(strVec, patternVec, escapeVec, resultVec);
    LikeFunctionTestHelper::ValidateBooleanResult(resultVec, expected, strValues.size());

    delete resultVec;
}

TEST(LikeTest, EscapeThreeArg_NullEscape) {
    std::vector<std::string> strValues = {"a", "b"};
    std::vector<std::string> patterns = {"a", "b"};
    BaseVector* strVec = LikeFunctionTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = LikeFunctionTestHelper::CreateStringVector(patterns);
    BaseVector* escapeVec = new ConstVector<std::string_view>(std::string_view("#"), OMNI_VARCHAR, 2);
    escapeVec->SetNull(0);

    BaseVector* resultVec = nullptr;
    LikeFunctionTestHelper::ExecuteLike3(strVec, patternVec, escapeVec, resultVec);

    EXPECT_TRUE(resultVec->IsNull(0));
    EXPECT_TRUE(resultVec->IsNull(1));

    delete resultVec;
}

TEST(LikeTest, EscapeThreeArg_InvalidEscapeLength) {
    std::vector<std::string> strValues = {"a"};
    std::vector<std::string> patterns = {"a"};
    BaseVector* strVec = LikeFunctionTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = LikeFunctionTestHelper::CreateStringVector(patterns);
    BaseVector* escapeVec = new ConstVector<std::string_view>(std::string_view("##"), OMNI_VARCHAR, 1);
    BaseVector* resultVec = nullptr;

    ASSERT_THROW(LikeFunctionTestHelper::ExecuteLike3(strVec, patternVec, escapeVec, resultVec),
        omniruntime::exception::OmniException);

    delete resultVec;
    delete strVec;
    delete patternVec;
    delete escapeVec;
}

// SQL LIKE wildcards must match newline. "a%b" has an internal '%', so it is classified as generic
// and compiled to a regex: only the "(?s)" flag added in LikePatternToRegex makes `.*` consume '\n'.
// The relaxed fast paths below walk code points and match newline without any flag.
TEST(LikeTest, NewlineMatchesPercentThroughRegexPath) {
    std::vector<std::string> strValues = {"a\nb", "a\n\nb", "ab", "aXc"};
    const std::string pattern = "a%b";
    std::vector<bool> expected = {true, true, true, false};

    BaseVector* strVec = LikeFunctionTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = new ConstVector<std::string_view>(
        std::string_view(pattern), OMNI_VARCHAR, static_cast<int32_t>(strValues.size()));
    BaseVector* resultVec = nullptr;

    LikeFunctionTestHelper::ExecuteLike(strVec, patternVec, resultVec);
    LikeFunctionTestHelper::ValidateBooleanResult(resultVec, expected, strValues.size());

    delete resultVec;
}

// Same check for '#'/'_' (regex `.`): "x_%z" is generic as well, so the single-character wildcard
// must also consume '\n'.
TEST(LikeTest, NewlineMatchesUnderscoreThroughRegexPath) {
    std::vector<std::string> strValues = {"x\nyQz", "xz", "x\nz"};
    const std::string pattern = "x_%z";
    std::vector<bool> expected = {true, false, true};

    BaseVector* strVec = LikeFunctionTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = new ConstVector<std::string_view>(
        std::string_view(pattern), OMNI_VARCHAR, static_cast<int32_t>(strValues.size()));
    BaseVector* resultVec = nullptr;

    LikeFunctionTestHelper::ExecuteLike(strVec, patternVec, resultVec);
    LikeFunctionTestHelper::ValidateBooleanResult(resultVec, expected, strValues.size());

    delete resultVec;
}

// `_` consumes exactly one UTF-8 code point, so 3- and 4-byte characters are one wildcard match.
// This exercises the byte span (1..4 bytes per `_`) used to bound the relaxed suffix matcher.
TEST(LikeTest, RelaxedSuffixWithMultibyteUnderscore) {
    std::vector<std::string> strValues = {"x\xe4\xb8\xad\xf0\x9f\x98\x80",  // x + 中 + 😀
                                          "\xf0\x9f\x98\x80\xf0\x9f\x98\x80",  // 😀😀
                                          "\xf0\x9f\x98\x80",                  // 😀 only
                                          "\xe4\xb8\xad"};                     // 中 only
    const std::string pattern = "%_\xf0\x9f\x98\x80";  // "%_😀"
    std::vector<bool> expected = {true, true, false, false};

    BaseVector* strVec = LikeFunctionTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = new ConstVector<std::string_view>(
        std::string_view(pattern), OMNI_VARCHAR, static_cast<int32_t>(strValues.size()));
    BaseVector* resultVec = nullptr;

    LikeFunctionTestHelper::ExecuteLike(strVec, patternVec, resultVec);
    LikeFunctionTestHelper::ValidateBooleanResult(resultVec, expected, strValues.size());

    delete resultVec;
}

TEST(LikeTest, RelaxedSuffixWithMultibyteUnderscoreCjk) {
    std::vector<std::string> strValues = {"ab\xe4\xb8\xad", "\xe4\xb8\xad", "x\xe4\xb8\xad"};  // ab中, 中, x中
    const std::string pattern = "%_\xe4\xb8\xad";  // "%_中"
    std::vector<bool> expected = {true, false, true};

    BaseVector* strVec = LikeFunctionTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = new ConstVector<std::string_view>(
        std::string_view(pattern), OMNI_VARCHAR, static_cast<int32_t>(strValues.size()));
    BaseVector* resultVec = nullptr;

    LikeFunctionTestHelper::ExecuteLike(strVec, patternVec, resultVec);
    LikeFunctionTestHelper::ValidateBooleanResult(resultVec, expected, strValues.size());

    delete resultVec;
}

// Substring shape ("%<body>%"): the body starts with a multi-byte literal followed by `_`, so the
// anchor lookup and the code-point walk must agree on the byte offsets.
TEST(LikeTest, RelaxedSubstringWithMultibyteBody) {
    std::vector<std::string> strValues = {
        "xx\xf0\x9f\x98\x80\xe4\xb8\xadyy",  // xx😀中yy
        "xx\xf0\x9f\x98\x80yy",              // xx😀yy
        "xx\xf0\x9f\x98\x80"                 // xx😀 (no trailing code point for `_`)
    };
    const std::string pattern = "%\xf0\x9f\x98\x80_%";  // "%😀_%"
    std::vector<bool> expected = {true, true, false};

    BaseVector* strVec = LikeFunctionTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = new ConstVector<std::string_view>(
        std::string_view(pattern), OMNI_VARCHAR, static_cast<int32_t>(strValues.size()));
    BaseVector* resultVec = nullptr;

    LikeFunctionTestHelper::ExecuteLike(strVec, patternVec, resultVec);
    LikeFunctionTestHelper::ValidateBooleanResult(resultVec, expected, strValues.size());

    delete resultVec;
}

// Candidate budget: "%<30 underscores>abc" has a 91-wide start window (4 bytes per `_`), so both
// rows exceed the 64-attempt budget and must fall back to RE2. The first row really matches at the
// last window position (the 91st candidate), the second one never matches.
TEST(LikeTest, RelaxedSuffixCandidateBudgetFallsBackToRegex) {
    std::string tailMatch(196, 'x');
    tailMatch += "abc";
    std::string noMatch(300, 'x');
    std::vector<std::string> strValues = {tailMatch, noMatch};
    std::string pattern = "%";
    pattern.append(30, '_');
    pattern += "abc";
    std::vector<bool> expected = {true, false};

    BaseVector* strVec = LikeFunctionTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = new ConstVector<std::string_view>(
        std::string_view(pattern), OMNI_VARCHAR, static_cast<int32_t>(strValues.size()));
    BaseVector* resultVec = nullptr;

    LikeFunctionTestHelper::ExecuteLike(strVec, patternVec, resultVec);
    LikeFunctionTestHelper::ValidateBooleanResult(resultVec, expected, strValues.size());

    delete resultVec;
}
