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

// Test: RegexpExtract with out of range group index returns an empty string
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
    ASSERT_FALSE(resultVec->IsNull(0));
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

    // Apply threw before reaching its delete statements, so args are leaked - clean up here
    delete strVec;
    delete patternVec;
    delete groupIdxVec;
    delete resultVec;
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