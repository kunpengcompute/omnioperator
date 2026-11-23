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
#include "vectorization/functions/RegexpExtract.h"
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
class RegexpExtractTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const regexp_extract_test_env = ::testing::AddGlobalTestEnvironment(new RegexpExtractTestEnvironment);

class RegexpExtractFunctionTestHelper {
public:
    static void ValidateStringResult(BaseVector* result, const std::vector<std::string>& expected, int rowSize) {
        auto* resultVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
        ASSERT_NE(resultVec, nullptr) << "Result vector is not string type";

        for (int i = 0; i < rowSize; ++i) {
            if (result->IsNull(i)) {
                std::cout << "Row " << i << ": NULL" << std::endl;
                continue;
            }
            std::string_view actualValue = resultVec->GetValue(i);
            std::string expectedValue = expected[i];
            std::cout << "Row " << i << ": Expected=\"" << expectedValue
                    << "\", Actual=\"" << actualValue << "\"" << std::endl;
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

    template<typename T>
    static BaseVector* CreateNumericVector(const std::vector<T>& values, DataTypeId typeId) {
        BaseVector* vec = VectorHelper::CreateFlatVector(typeId, values.size());
        auto* typedVec = static_cast<Vector<T>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typedVec->SetValue(i, values[i]);
        }
        return vec;
    }

    static void ExecuteRegexpExtract(BaseVector* strVec, BaseVector* patternVec,
                                     BaseVector* groupIdxVec, BaseVector*& result) {
        std::vector<DataTypeId> inputTypeIds;
        inputTypeIds.push_back(OMNI_VARCHAR);
        inputTypeIds.push_back(OMNI_VARCHAR);
        inputTypeIds.push_back(OMNI_INT);

        auto signature = std::make_shared<FunctionSignature>("regexp_extract", inputTypeIds, OMNI_VARCHAR);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "RegexpExtract function not found for signature";

        auto outputType = std::make_shared<DataType>(OMNI_VARCHAR);
        ExecutionContext context;
        context.SetResultRowSize(strVec->GetSize());
        std::stack<BaseVector*> args;

        args.push(strVec);
        args.push(patternVec);
        args.push(groupIdxVec);

        function->Apply(args, outputType, result, &context);
    }
};

// Test: RegexpExtract with simple pattern (2 arguments, default group 0)
TEST(RegexpExtractTest, RegexpExtractSimplePattern) {
    std::cout << "=== Test: RegexpExtract with simple pattern (2 args) ===" << std::endl;

    std::vector<std::string> strValues = {"hello123", "world456", "test789"};
    std::vector<std::string> patterns = {"\\d+", "\\d+", "\\d+"};
    std::vector<int32_t> groupIndices = {0, 0, 0};
    std::vector<std::string> expected = {"123", "456", "789"};  // Extract digits

    BaseVector* strVec = RegexpExtractFunctionTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = RegexpExtractFunctionTestHelper::CreateStringVector(patterns);
    BaseVector* groupIdxVec = RegexpExtractFunctionTestHelper::CreateNumericVector(groupIndices, OMNI_INT);
    BaseVector* resultVec = nullptr;

    RegexpExtractFunctionTestHelper::ExecuteRegexpExtract(strVec, patternVec, groupIdxVec, resultVec);
    RegexpExtractFunctionTestHelper::ValidateStringResult(resultVec, expected, strValues.size());

    delete resultVec;
}

// Test: RegexpExtract with group index 0 (entire match)
TEST(RegexpExtractTest, RegexpExtractGroupZero) {
    std::cout << "=== Test: RegexpExtract with group index 0 ===" << std::endl;

    std::vector<std::string> strValues = {"hello123", "world456"};
    std::vector<std::string> patterns = {"\\d+", "\\d+"};
    std::vector<int32_t> groupIndices = {0, 0};
    std::vector<std::string> expected = {"123", "456"};  // Group 0 is entire match

    BaseVector* strVec = RegexpExtractFunctionTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = RegexpExtractFunctionTestHelper::CreateStringVector(patterns);
    BaseVector* groupIdxVec = RegexpExtractFunctionTestHelper::CreateNumericVector(groupIndices, OMNI_INT);
    BaseVector* resultVec = nullptr;

    RegexpExtractFunctionTestHelper::ExecuteRegexpExtract(strVec, patternVec, groupIdxVec, resultVec);
    RegexpExtractFunctionTestHelper::ValidateStringResult(resultVec, expected, strValues.size());

    delete resultVec;
}

// Test: RegexpExtract with capture groups
TEST(RegexpExtractTest, RegexpExtractCaptureGroups) {
    std::cout << "=== Test: RegexpExtract with capture groups ===" << std::endl;

    std::vector<std::string> strValues = {"hello123world", "test456foo"};
    std::vector<std::string> patterns = {"(\\w+)(\\d+)(\\w+)", "(\\w+)(\\d+)(\\w+)"};
    std::vector<int32_t> groupIndices = {1, 2};  // Extract first and second groups
    std::vector<std::string> expected = {"hello12", "6"};  // First group and second group

    BaseVector* strVec = RegexpExtractFunctionTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = RegexpExtractFunctionTestHelper::CreateStringVector(patterns);
    BaseVector* groupIdxVec = RegexpExtractFunctionTestHelper::CreateNumericVector(groupIndices, OMNI_INT);
    BaseVector* resultVec = nullptr;

    RegexpExtractFunctionTestHelper::ExecuteRegexpExtract(strVec, patternVec, groupIdxVec, resultVec);
    RegexpExtractFunctionTestHelper::ValidateStringResult(resultVec, expected, strValues.size());

    delete resultVec;
}

TEST(RegexpExtractTest, ConstCallerContextPatternAndGroup) {
    std::vector<std::string> strValues = {
        "prefix/mr_attempt_1700000000000_42_m_7_8/suffix",
        "prefix/mr_attempt_99_1_r_2_3/suffix",
        "prefix/application_99_1_JId_2_SId_3_4/suffix"
    };
    const std::string pattern = R"((mr_attempt_\d+_\d+_[a-z]_\d+_\d+))";
    std::vector<std::string> expected = {
        "mr_attempt_1700000000000_42_m_7_8",
        "mr_attempt_99_1_r_2_3",
        ""
    };

    BaseVector* strVec = RegexpExtractFunctionTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = new ConstVector<std::string_view>(
        std::string_view(pattern), OMNI_VARCHAR, static_cast<int32_t>(strValues.size()));
    BaseVector* groupIdxVec = new ConstVector<int32_t>(1, OMNI_INT, static_cast<int32_t>(strValues.size()));
    BaseVector* resultVec = nullptr;

    RegexpExtractFunctionTestHelper::ExecuteRegexpExtract(strVec, patternVec, groupIdxVec, resultVec);
    RegexpExtractFunctionTestHelper::ValidateStringResult(resultVec, expected, strValues.size());

    delete resultVec;
}

// The literal-prefix pre-location loop must not stop at a prefix occurrence that fails to match:
// here "mr_attempt_" appears once with a non-numeric tail (no match) before the real match.
TEST(RegexpExtractTest, ConstPatternPrefixPreLocation) {
    std::vector<std::string> strValues = {
        "noise mr_attempt_abc then mr_attempt_1700000000000_42_m_7_8 tail",
        "mr_attempt_1_2_x_3_4",
        "nothing relevant here"
    };
    const std::string pattern = R"((mr_attempt_\d+_\d+_[a-z]_\d+_\d+))";
    std::vector<std::string> expected = {
        "mr_attempt_1700000000000_42_m_7_8",
        "mr_attempt_1_2_x_3_4",
        ""
    };

    BaseVector* strVec = RegexpExtractFunctionTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = new ConstVector<std::string_view>(
        std::string_view(pattern), OMNI_VARCHAR, static_cast<int32_t>(strValues.size()));
    BaseVector* groupIdxVec = new ConstVector<int32_t>(1, OMNI_INT, static_cast<int32_t>(strValues.size()));
    BaseVector* resultVec = nullptr;

    RegexpExtractFunctionTestHelper::ExecuteRegexpExtract(strVec, patternVec, groupIdxVec, resultVec);
    RegexpExtractFunctionTestHelper::ValidateStringResult(resultVec, expected, strValues.size());

    delete resultVec;
}

// Same pre-location behavior for the second production pattern, including an early false prefix.
TEST(RegexpExtractTest, ConstApplicationPatternPrefixPreLocation) {
    std::vector<std::string> strValues = {
        "application_nope then application_1700000000000_42_JId_7_SId_8_9 done",
        "prefix/application_99_1_JId_2_SId_3_4/suffix"
    };
    const std::string pattern = R"((application_\d+_\d+_JId_\d+_SId_\d+_\d+))";
    std::vector<std::string> expected = {
        "application_1700000000000_42_JId_7_SId_8_9",
        "application_99_1_JId_2_SId_3_4"
    };

    BaseVector* strVec = RegexpExtractFunctionTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = new ConstVector<std::string_view>(
        std::string_view(pattern), OMNI_VARCHAR, static_cast<int32_t>(strValues.size()));
    BaseVector* groupIdxVec = new ConstVector<int32_t>(1, OMNI_INT, static_cast<int32_t>(strValues.size()));
    BaseVector* resultVec = nullptr;

    RegexpExtractFunctionTestHelper::ExecuteRegexpExtract(strVec, patternVec, groupIdxVec, resultVec);
    RegexpExtractFunctionTestHelper::ValidateStringResult(resultVec, expected, strValues.size());

    delete resultVec;
}

// Test: RegexpExtract with no match
TEST(RegexpExtractTest, RegexpExtractNoMatch) {
    std::cout << "=== Test: RegexpExtract with no match ===" << std::endl;

    std::vector<std::string> strValues = {"hello", "world"};
    std::vector<std::string> patterns = {"\\d+", "\\d+"};  // No digits in strings
    std::vector<int32_t> groupIndices = {0, 0};
    std::vector<std::string> expected = {"", ""};
    BaseVector* strVec = RegexpExtractFunctionTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = RegexpExtractFunctionTestHelper::CreateStringVector(patterns);
    BaseVector* groupIdxVec = RegexpExtractFunctionTestHelper::CreateNumericVector(groupIndices, OMNI_INT);
    BaseVector* resultVec = nullptr;

    RegexpExtractFunctionTestHelper::ExecuteRegexpExtract(strVec, patternVec, groupIdxVec, resultVec);
    RegexpExtractFunctionTestHelper::ValidateStringResult(resultVec, expected, strValues.size());

    delete resultVec;
}

// Test: RegexpExtract with NULL string values
TEST(RegexpExtractTest, RegexpExtractWithNullString) {
    std::cout << "=== Test: RegexpExtract with NULL string values ===" << std::endl;

    std::vector<std::string> strValues = {"hello123", "world456", "test789"};
    std::vector<std::string> patterns = {"\\d+", "\\d+", "\\d+"};
    std::vector<int32_t> groupIndices = {0, 0, 0};

    BaseVector* strVec = RegexpExtractFunctionTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = RegexpExtractFunctionTestHelper::CreateStringVector(patterns);
    BaseVector* groupIdxVec = RegexpExtractFunctionTestHelper::CreateNumericVector(groupIndices, OMNI_INT);

    // Set middle string value to NULL
    strVec->SetNull(1);

    BaseVector* resultVec = nullptr;
    RegexpExtractFunctionTestHelper::ExecuteRegexpExtract(strVec, patternVec, groupIdxVec, resultVec);

    // When string is NULL, result should be NULL
    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL (string is NULL)";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";

    delete resultVec;
}

// Test: RegexpExtract with NULL pattern values
TEST(RegexpExtractTest, RegexpExtractWithNullPattern) {
    std::cout << "=== Test: RegexpExtract with NULL pattern values ===" << std::endl;

    std::vector<std::string> strValues = {"hello123", "world456", "test789"};
    std::vector<std::string> patterns = {"\\d+", "\\d+", "\\d+"};
    std::vector<int32_t> groupIndices = {0, 0, 0};

    BaseVector* strVec = RegexpExtractFunctionTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = RegexpExtractFunctionTestHelper::CreateStringVector(patterns);
    BaseVector* groupIdxVec = RegexpExtractFunctionTestHelper::CreateNumericVector(groupIndices, OMNI_INT);

    // Set middle pattern value to NULL
    patternVec->SetNull(1);

    BaseVector* resultVec = nullptr;
    RegexpExtractFunctionTestHelper::ExecuteRegexpExtract(strVec, patternVec, groupIdxVec, resultVec);

    // When pattern is NULL, result should be NULL
    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL (pattern is NULL)";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";

    delete resultVec;
}

TEST(RegexpExtractTest, RegexpExtractWithNullGroupIndex) {
    std::vector<std::string> strValues = {"a1", "b2", "c3"};
    std::vector<std::string> patterns = {"(\\d+)", "(\\d+)", "(\\d+)"};
    std::vector<int32_t> groupIndices = {1, 1, 1};

    BaseVector* strVec = RegexpExtractFunctionTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = RegexpExtractFunctionTestHelper::CreateStringVector(patterns);
    BaseVector* groupIdxVec = RegexpExtractFunctionTestHelper::CreateNumericVector(groupIndices, OMNI_INT);
    groupIdxVec->SetNull(1);

    BaseVector* resultVec = nullptr;
    RegexpExtractFunctionTestHelper::ExecuteRegexpExtract(strVec, patternVec, groupIdxVec, resultVec);

    auto* typedResult = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(resultVec);
    ASSERT_NE(typedResult, nullptr);
    EXPECT_FALSE(resultVec->IsNull(0));
    EXPECT_TRUE(resultVec->IsNull(1));
    EXPECT_FALSE(resultVec->IsNull(2));
    EXPECT_EQ(typedResult->GetValue(0), "1");
    EXPECT_EQ(typedResult->GetValue(2), "3");
    delete resultVec;
}

// Test: RegexpExtract with email pattern
TEST(RegexpExtractTest, RegexpExtractEmailPattern) {
    std::cout << "=== Test: RegexpExtract with email pattern ===" << std::endl;

    std::vector<std::string> strValues = {"Contact: user@example.com", "Email: test@domain.org"};
    std::vector<std::string> patterns = {"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,})",
                                         "([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,})"};
    std::vector<int32_t> groupIndices = {1, 1};  // Extract email from group 1
    std::vector<std::string> expected = {"user@example.com", "test@domain.org"};

    BaseVector* strVec = RegexpExtractFunctionTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = RegexpExtractFunctionTestHelper::CreateStringVector(patterns);
    BaseVector* groupIdxVec = RegexpExtractFunctionTestHelper::CreateNumericVector(groupIndices, OMNI_INT);
    BaseVector* resultVec = nullptr;

    RegexpExtractFunctionTestHelper::ExecuteRegexpExtract(strVec, patternVec, groupIdxVec, resultVec);
    RegexpExtractFunctionTestHelper::ValidateStringResult(resultVec, expected, strValues.size());

    delete resultVec;
}

// Test: RegexpExtract with out of range group index
TEST(RegexpExtractTest, RegexpExtractOutOfRangeGroup) {
    std::cout << "=== Test: RegexpExtract with out of range group index ===" << std::endl;

    std::vector<std::string> strValues = {"hello123"};
    std::vector<std::string> patterns = {"(\\w+)(\\d+)"};  // 2 capture groups
    std::vector<int32_t> groupIndices = {5};  // Group 5 doesn't exist
    std::vector<std::string> expected = {""};
    BaseVector* strVec = RegexpExtractFunctionTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = RegexpExtractFunctionTestHelper::CreateStringVector(patterns);
    BaseVector* groupIdxVec = RegexpExtractFunctionTestHelper::CreateNumericVector(groupIndices, OMNI_INT);
    BaseVector* resultVec = nullptr;
    RegexpExtractFunctionTestHelper::ExecuteRegexpExtract(strVec, patternVec, groupIdxVec, resultVec);
    RegexpExtractFunctionTestHelper::ValidateStringResult(resultVec, expected, strValues.size());
    delete resultVec;
}

// Test: RegexpExtract with invalid regex pattern
TEST(RegexpExtractTest, RegexpExtractInvalidPattern) {
    std::cout << "=== Test: RegexpExtract with invalid regex pattern ===" << std::endl;

    std::vector<std::string> strValues = {"hello", "world"};
    std::vector<std::string> patterns = {"[", "("};  // Invalid regex patterns
    std::vector<int32_t> groupIndices = {0, 0};

    BaseVector* strVec = RegexpExtractFunctionTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = RegexpExtractFunctionTestHelper::CreateStringVector(patterns);
    BaseVector* groupIdxVec = RegexpExtractFunctionTestHelper::CreateNumericVector(groupIndices, OMNI_INT);
    BaseVector* resultVec = nullptr;

    // Invalid regex should return NULL (not throw)
    ASSERT_THROW({RegexpExtractFunctionTestHelper::ExecuteRegexpExtract(strVec, patternVec, groupIdxVec, resultVec);}, omniruntime::exception::OmniException)
         << "RegexpExtract should handle invalid regex through throw exception";

    // Apply releases the inputs and the partially built result through RAII on the exception
    // path, so no manual cleanup is required here.
}

// Test: RegexpExtract with date pattern
TEST(RegexpExtractTest, RegexpExtractDatePattern) {
    std::cout << "=== Test: RegexpExtract with date pattern ===" << std::endl;

    std::vector<std::string> strValues = {"Date: 2024-01-15", "Date: 2023-12-25"};
    std::vector<std::string> patterns = {"(\\d{4})-(\\d{2})-(\\d{2})", "(\\d{4})-(\\d{2})-(\\d{2})"};
    std::vector<int32_t> groupIndices = {1, 2};  // Extract year and month
    std::vector<std::string> expected = {"2024", "12"};

    BaseVector* strVec = RegexpExtractFunctionTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = RegexpExtractFunctionTestHelper::CreateStringVector(patterns);
    BaseVector* groupIdxVec = RegexpExtractFunctionTestHelper::CreateNumericVector(groupIndices, OMNI_INT);
    BaseVector* resultVec = nullptr;

    RegexpExtractFunctionTestHelper::ExecuteRegexpExtract(strVec, patternVec, groupIdxVec, resultVec);
    RegexpExtractFunctionTestHelper::ValidateStringResult(resultVec, expected, strValues.size());

    delete resultVec;
}

// `\Q...\E` quoting keeps the leading literal run literal even when it contains RE2
// metacharacters such as '.', so the derived prefix ("mr.attempt_") must still drive
// pre-location correctly: the first occurrence with a non-numeric tail must be skipped and the
// real match found afterwards.
TEST(RegexpExtractTest, RegexpExtractQuotedLiteralPrefix) {
    std::vector<std::string> strValues = {
        "noise mr.attempt_abc then mr.attempt_1700000000000 tail",
        "mr.attempt_abc",
        "nothing relevant here"
    };
    const std::string pattern = R"(\Qmr.attempt_\E\d+)";
    std::vector<std::string> expected = {
        "mr.attempt_1700000000000",
        "",
        ""
    };

    BaseVector* strVec = RegexpExtractFunctionTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = new ConstVector<std::string_view>(
        std::string_view(pattern), OMNI_VARCHAR, static_cast<int32_t>(strValues.size()));
    BaseVector* groupIdxVec = new ConstVector<int32_t>(0, OMNI_INT, static_cast<int32_t>(strValues.size()));
    BaseVector* resultVec = nullptr;

    RegexpExtractFunctionTestHelper::ExecuteRegexpExtract(strVec, patternVec, groupIdxVec, resultVec);
    RegexpExtractFunctionTestHelper::ValidateStringResult(resultVec, expected, strValues.size());

    delete resultVec;
}

// An input with more than MAX_ANCHORED_CANDIDATES (64) occurrences of the literal prefix that
// never complete a match must fall back to a single unanchored RE2 search and still return the
// real match that follows, instead of degenerating into quadratic anchored scanning.
TEST(RegexpExtractTest, RegexpExtractOver64AnchoredCandidates) {
    std::string noisy;
    for (int32_t i = 0; i < 70; ++i) {
        noisy += "mr_attempt_x ";
    }
    noisy += "mr_attempt_1_2_a_3_4";

    std::vector<std::string> strValues = {
        noisy,
        "mr_attempt_5_6_b_7_8"
    };
    const std::string pattern = R"((mr_attempt_\d+_\d+_[a-z]_\d+_\d+))";
    std::vector<std::string> expected = {
        "mr_attempt_1_2_a_3_4",
        "mr_attempt_5_6_b_7_8"
    };

    BaseVector* strVec = RegexpExtractFunctionTestHelper::CreateStringVector(strValues);
    BaseVector* patternVec = new ConstVector<std::string_view>(
        std::string_view(pattern), OMNI_VARCHAR, static_cast<int32_t>(strValues.size()));
    BaseVector* groupIdxVec = new ConstVector<int32_t>(1, OMNI_INT, static_cast<int32_t>(strValues.size()));
    BaseVector* resultVec = nullptr;

    RegexpExtractFunctionTestHelper::ExecuteRegexpExtract(strVec, patternVec, groupIdxVec, resultVec);
    RegexpExtractFunctionTestHelper::ValidateStringResult(resultVec, expected, strValues.size());

    delete resultVec;
}

// The 64th anchored attempt can fail because there is not enough input remaining, even though
// the pattern matches starting at that candidate. The unanchored fallback must include that
// candidate instead of starting one byte later.
TEST(RegexpExtractTest, RegexpExtractFallbackRetriesLastAnchoredCandidate) {
    const std::string input = std::string(64, 'a') + "b";
    const std::string pattern = R"(a{1,100}b)";
    const std::vector<std::string> expected = {input};

    BaseVector* strVec = RegexpExtractFunctionTestHelper::CreateStringVector({input});
    BaseVector* patternVec = new ConstVector<std::string_view>(
        std::string_view(pattern), OMNI_VARCHAR, 1);
    BaseVector* groupIdxVec = new ConstVector<int32_t>(0, OMNI_INT, 1);
    BaseVector* resultVec = nullptr;

    RegexpExtractFunctionTestHelper::ExecuteRegexpExtract(strVec, patternVec, groupIdxVec, resultVec);
    RegexpExtractFunctionTestHelper::ValidateStringResult(resultVec, expected, 1);

    delete resultVec;
}

// An empty batch (size == 0) must return an empty result before touching row 0 of the pattern /
// group vectors, compiling the regex or rejecting the (deliberately empty) pattern.
TEST(RegexpExtractTest, RegexpExtractEmptyBatchEarlyReturn) {
    BaseVector* strVec = RegexpExtractFunctionTestHelper::CreateStringVector({});
    // Empty pattern would throw ("patternStr must not empty") if the row loop were entered.
    BaseVector* patternVec = new ConstVector<std::string_view>(
        std::string_view(""), OMNI_VARCHAR, 0);
    BaseVector* groupIdxVec = new ConstVector<int32_t>(0, OMNI_INT, 0);
    BaseVector* resultVec = nullptr;

    ASSERT_NO_THROW(
        RegexpExtractFunctionTestHelper::ExecuteRegexpExtract(strVec, patternVec, groupIdxVec, resultVec));
    ASSERT_NE(resultVec, nullptr);
    EXPECT_EQ(resultVec->GetSize(), 0);

    delete resultVec;
}
