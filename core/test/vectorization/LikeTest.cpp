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
        ExecutionContext context(4096, nullptr);
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
        ExecutionContext context(4096, nullptr);
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
