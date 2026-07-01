/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: RegexpExtract test
 */

#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include <vector>
#include <limits>
#include <cmath>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/functions/RLike.h"
#include "vectorization/registration/SimpleFunctionRegistry.h"
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

// Initialize function registration before running tests
class RLikeTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const rlike_test_env = ::testing::AddGlobalTestEnvironment(new RLikeTestEnvironment);

class RLikeFunctionTestHelper {
public:
    static void ValidateBooleanResult(BaseVector* result, const std::vector<bool>& expected, int rowSize) {
        auto* resultVec = dynamic_cast<Vector<bool>*>(result);
        ASSERT_NE(resultVec, nullptr) << "Result vector is not boolean type";

        for (int i = 0; i < rowSize; ++i) {
            if (result->IsNull(i)) {
                std::cout << "Row " << i << ": NULL" << std::endl;
                continue;
            }
            bool actualValue = resultVec->GetValue(i);
            bool expectedValue = expected[i];
            std::cout << "Row " << i << ": Expected=" << (expectedValue ? "true" : "false")
                    << ", Actual=" << (actualValue ? "true" : "false") << std::endl;
            EXPECT_EQ(actualValue, expectedValue) << "Row " << i << " value mismatch";
        }
    }

    static BaseVector* CreateStringVector(const std::vector<std::string>& values) {
        BaseVector* vec = VectorHelper::CreateStringVector(values.size());
        auto* typedVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            std::string_view sv(values[i]);
            typedVec->SetValue(i, sv);
        }
        return vec;
    }

    static void ExecuteRLike(BaseVector* strVec, BaseVector* patternVec, BaseVector*& result) {
        auto signature = std::make_shared<FunctionSignature>("RLike",
            std::vector<DataTypeId>{OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_BOOLEAN);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "RLike function not found for signature";

        auto outputType = std::make_shared<DataType>(OMNI_BOOLEAN);
        ExecutionContext context;
        context.SetResultRowSize(strVec->GetSize());
        std::stack<BaseVector*> args;

        args.push(strVec);
        args.push(patternVec);

       function->Apply(args, outputType, result, &context);
    }
};

// Test: RLike with simple pattern match
TEST(RLikeTest, RLikeSimpleMatch) {
    std::cout << "=== Test: RLike with simple pattern match ===" << std::endl;

    std::vector<std::string> strValues = {"hello", "world", "test"};
    std::vector<std::string> patterns = {"hello", "world", "test"};
    std::vector<bool> expected = {true, true, true};  // Should match

    BaseVector* strVec = RLikeFunctionTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = RLikeFunctionTestHelper::CreateStringVector(patterns);
    BaseVector* resultVec = nullptr;

    RLikeFunctionTestHelper::ExecuteRLike(strVec, patternVec, resultVec);
    RLikeFunctionTestHelper::ValidateBooleanResult(resultVec, expected, strValues.size());

    delete resultVec;
}

// Test: RLike with regex pattern
TEST(RLikeTest, RLikeRegexPattern) {
    std::cout << "=== Test: RLike with regex pattern ===" << std::endl;

    std::vector<std::string> strValues = {"hello123", "world456", "test789"};
    std::vector<std::string> patterns = {".*123", ".*456", ".*789"};
    std::vector<bool> expected = {true, true, true};  // Should match

    BaseVector* strVec = RLikeFunctionTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = RLikeFunctionTestHelper::CreateStringVector(patterns);
    BaseVector* resultVec = nullptr;

    RLikeFunctionTestHelper::ExecuteRLike(strVec, patternVec, resultVec);
    RLikeFunctionTestHelper::ValidateBooleanResult(resultVec, expected, strValues.size());

    delete resultVec;
}

// Test: RLike with no match
TEST(RLikeTest, RLikeNoMatch) {
    std::cout << "=== Test: RLike with no match ===" << std::endl;

    std::vector<std::string> strValues = {"hello", "world", "test"};
    std::vector<std::string> patterns = {"xyz", "abc", "def"};
    std::vector<bool> expected = {false, false, false};  // Should not match

    BaseVector* strVec = RLikeFunctionTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = RLikeFunctionTestHelper::CreateStringVector(patterns);
    BaseVector* resultVec = nullptr;

    RLikeFunctionTestHelper::ExecuteRLike(strVec, patternVec, resultVec);
    RLikeFunctionTestHelper::ValidateBooleanResult(resultVec, expected, strValues.size());

    delete resultVec;
}

// Test: RLike with const literal pattern should behave like substring match
TEST(RLikeTest, RLikeConstLiteralContains) {
    std::cout << "=== Test: RLike const literal contains ===" << std::endl;

    std::vector<std::string> strValues = {"abc_pkg_list_pkgclick_xyz", "pkg_list_pkgclick", "pkg_list", ""};
    std::vector<bool> expected = {true, true, false, false};

    BaseVector* strVec = RLikeFunctionTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = new ConstVector<std::string_view>(
        std::string_view("pkg_list_pkgclick"),
        OMNI_VARCHAR,
        strValues.size());
    BaseVector* resultVec = nullptr;

    RLikeFunctionTestHelper::ExecuteRLike(strVec, patternVec, resultVec);
    RLikeFunctionTestHelper::ValidateBooleanResult(resultVec, expected, strValues.size());

    delete resultVec;
}

// Test: RLike with mixed matches
TEST(RLikeTest, RLikeMixedMatches) {
    std::cout << "=== Test: RLike with mixed matches ===" << std::endl;

    std::vector<std::string> strValues = {"hello", "world", "test", "hello123"};
    std::vector<std::string> patterns = {"hello", "xyz", ".*", ".*123"};
    std::vector<bool> expected = {true, false, true, true};  // Mixed results

    BaseVector* strVec = RLikeFunctionTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = RLikeFunctionTestHelper::CreateStringVector(patterns);
    BaseVector* resultVec = nullptr;

    RLikeFunctionTestHelper::ExecuteRLike(strVec, patternVec, resultVec);
    RLikeFunctionTestHelper::ValidateBooleanResult(resultVec, expected, strValues.size());

    delete resultVec;
}

// Test: RLike with NULL string values
TEST(RLikeTest, RLikeWithNullString) {
    std::cout << "=== Test: RLike with NULL string values ===" << std::endl;

    std::vector<std::string> strValues = {"hello", "world", "test"};
    std::vector<std::string> patterns = {"hello", "world", "test"};

    BaseVector* strVec = RLikeFunctionTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = RLikeFunctionTestHelper::CreateStringVector(patterns);

    // Set middle string value to NULL
    strVec->SetNull(1);

    BaseVector* resultVec = nullptr;
    RLikeFunctionTestHelper::ExecuteRLike(strVec, patternVec, resultVec);

    // When string is NULL, result should be NULL
    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL (string is NULL)";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";

    delete resultVec;
}

// Test: RLike with NULL pattern values
TEST(RLikeTest, RLikeWithNullPattern) {
    std::cout << "=== Test: RLike with NULL pattern values ===" << std::endl;

    std::vector<std::string> strValues = {"hello", "world", "test"};
    std::vector<std::string> patterns = {"hello", "world", "test"};

    BaseVector* strVec = RLikeFunctionTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = RLikeFunctionTestHelper::CreateStringVector(patterns);

    // Set middle pattern value to NULL
    patternVec->SetNull(1);

    BaseVector* resultVec = nullptr;
    RLikeFunctionTestHelper::ExecuteRLike(strVec, patternVec, resultVec);

    // When pattern is NULL, result should be NULL
    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL (pattern is NULL)";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";

    delete resultVec;
}

// Test: RLike with digit pattern
TEST(RLikeTest, RLikeDigitPattern) {
    std::cout << "=== Test: RLike with digit pattern ===" << std::endl;

    std::vector<std::string> strValues = {"abc123", "def456", "xyz789", "no_digits"};
    std::vector<std::string> patterns = {".*\\d+.*", ".*\\d+.*", ".*\\d+.*", ".*\\d+.*"};
    std::vector<bool> expected = {true, true, true, false};  // First three contain digits

    BaseVector* strVec = RLikeFunctionTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = RLikeFunctionTestHelper::CreateStringVector(patterns);
    BaseVector* resultVec = nullptr;

    RLikeFunctionTestHelper::ExecuteRLike(strVec, patternVec, resultVec);
    RLikeFunctionTestHelper::ValidateBooleanResult(resultVec, expected, strValues.size());

    delete resultVec;
}

// Test: RLike with character class pattern
TEST(RLikeTest, RLikeCharacterClass) {
    std::cout << "=== Test: RLike with character class pattern ===" << std::endl;

    std::vector<std::string> strValues = {"Hello", "world", "Test123"};
    std::vector<std::string> patterns = {"^[A-Z].*", "^[a-z].*", "^[A-Z].*"};
    std::vector<bool> expected = {true, true, true};  // All start with letter

    BaseVector* strVec = RLikeFunctionTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = RLikeFunctionTestHelper::CreateStringVector(patterns);
    BaseVector* resultVec = nullptr;

    RLikeFunctionTestHelper::ExecuteRLike(strVec, patternVec, resultVec);
    RLikeFunctionTestHelper::ValidateBooleanResult(resultVec, expected, strValues.size());

    delete resultVec;
}

// Test: literal characters that are regex metacharacters must still use regex semantics.
TEST(RLikeTest, RLikeRegexMetaStillUsesRegex) {
    std::cout << "=== Test: RLike regex meta still uses regex ===" << std::endl;

    std::vector<std::string> strValues = {"abc123", "abc", "123"};
    std::vector<std::string> patterns = {".*123", ".*123", ".*123"};
    std::vector<bool> expected = {true, false, true};

    BaseVector* strVec = RLikeFunctionTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = RLikeFunctionTestHelper::CreateStringVector(patterns);
    BaseVector* resultVec = nullptr;

    RLikeFunctionTestHelper::ExecuteRLike(strVec, patternVec, resultVec);
    RLikeFunctionTestHelper::ValidateBooleanResult(resultVec, expected, strValues.size());

    delete resultVec;
}

// Test: RLike with empty string
TEST(RLikeTest, RLikeEmptyString) {
    std::cout << "=== Test: RLike with empty string ===" << std::endl;

    std::vector<std::string> strValues = {"", "hello", ""};
    std::vector<std::string> patterns = {".*", ".*", "^$"};
    std::vector<bool> expected = {true, true, true};  // Empty string matches .* and ^$

    BaseVector* strVec = RLikeFunctionTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = RLikeFunctionTestHelper::CreateStringVector(patterns);
    BaseVector* resultVec = nullptr;

    RLikeFunctionTestHelper::ExecuteRLike(strVec, patternVec, resultVec);
    RLikeFunctionTestHelper::ValidateBooleanResult(resultVec, expected, strValues.size());

    delete resultVec;
}

// Test: RLike with invalid regex pattern
TEST(RLikeTest, RLikeInvalidPattern) {
    std::cout << "=== Test: RLike with invalid regex pattern ===" << std::endl;

    std::vector<std::string> strValues = {"hello", "world"};
    std::vector<std::string> patterns = {"[", "("};  // Invalid regex patterns

    BaseVector* strVec = RLikeFunctionTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = RLikeFunctionTestHelper::CreateStringVector(patterns);
    BaseVector* resultVec = nullptr;

    // Invalid regex should return false (not throw)
    ASSERT_THROW({RLikeFunctionTestHelper::ExecuteRLike(strVec, patternVec, resultVec);}, omniruntime::exception::OmniException)
        << "RLike should handle invalid regex through throw exception";

    // Apply threw before reaching its delete statements, so args are leaked - clean up here
    delete strVec;
    delete patternVec;
    delete resultVec;
}

// Test: RLike with quantifier pattern
TEST(RLikeTest, RLikeQuantifierPattern) {
    std::cout << "=== Test: RLike with quantifier pattern ===" << std::endl;

    std::vector<std::string> strValues = {"a", "aa", "aaa", "b"};
    std::vector<std::string> patterns = {"a+", "a+", "a+", "a+"};
    std::vector<bool> expected = {true, true, true, false};  // First three match a+

    BaseVector* strVec = RLikeFunctionTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = RLikeFunctionTestHelper::CreateStringVector(patterns);
    BaseVector* resultVec = nullptr;

    RLikeFunctionTestHelper::ExecuteRLike(strVec, patternVec, resultVec);
    RLikeFunctionTestHelper::ValidateBooleanResult(resultVec, expected, strValues.size());

    delete resultVec;
}
