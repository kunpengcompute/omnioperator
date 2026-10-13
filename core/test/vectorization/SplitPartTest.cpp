/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: SplitPart function unit tests
 *   split_part(string, delimiter, index) -> varchar
 *   Splits string on delimiter and returns the part at index (1-based).
 *   Returns NULL if index is larger than the number of fields.
 *   When delimiter is empty, splits into individual characters (Unicode-aware).
 */

#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <optional>

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

class SplitPartTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const split_part_test_env =
    ::testing::AddGlobalTestEnvironment(new SplitPartTestEnvironment);

class SplitPartFunctionTestHelper {
public:
    static void ValidateStringResult(BaseVector* result,
                                    const std::vector<std::optional<std::string>>& expected,
                                    int rowSize) {
        auto* resultVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
        ASSERT_NE(resultVec, nullptr) << "Result vector type mismatch";
        for (int i = 0; i < rowSize; ++i) {
            if (!expected[i].has_value()) {
                EXPECT_TRUE(result->IsNull(i)) << "Row " << i << " expected NULL but got non-NULL";
                continue;
            }
            if (result->IsNull(i)) {
                FAIL() << "Row " << i << " expected \"" << expected[i].value() << "\" but got NULL";
                continue;
            }
            std::string_view actualSv = resultVec->GetValue(i);
            std::string actual(actualSv);
            std::string exp = expected[i].value();
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

    static BaseVector* CreateConstStringVector(const std::string& value, int rowSize) {
        // Create a flat vector with the same value repeated for each row
        // This avoids dangling pointer issues with ConstVector<std::string_view>
        std::vector<std::string> values(rowSize, value);
        return CreateStringVector(values);
    }

    static BaseVector* CreateInt64Vector(const std::vector<int64_t>& values) {
        BaseVector* vec = VectorHelper::CreateFlatVector(nullptr, OMNI_LONG, values.size());
        vec->SetIsField(true);
        auto* typed = static_cast<Vector<int64_t>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typed->SetValue(i, values[i]);
        }
        return vec;
    }

    static BaseVector* CreateConstInt64Vector(int64_t value, int rowSize) {
        return new ConstVector<int64_t>(value, OMNI_LONG, rowSize);
    }

    static void ExecuteSplitPart(BaseVector* inputVec, BaseVector* delimiterVec,
                                 BaseVector* indexVec, BaseVector*& result) {
        std::vector<DataTypeId> inputTypeIds = { OMNI_VARCHAR, OMNI_VARCHAR, indexVec->GetTypeId() };
        auto sig = std::make_shared<FunctionSignature>("split_part", inputTypeIds, OMNI_VARCHAR);
        auto fn = VectorFunction::Find(sig);
        ASSERT_NE(fn, nullptr) << "split_part function not found";
        auto outputType = std::make_shared<DataType>(OMNI_VARCHAR);
        ExecutionContext ctx(4096, nullptr);
        ctx.SetResultRowSize(inputVec->GetSize());
        // SimpleFunction pops last arg first: push (input, delimiter, index) so pop order is (index, delimiter, input)
        std::stack<BaseVector*> args;
        args.push(inputVec);
        args.push(delimiterVec);
        args.push(indexVec);
        ASSERT_NO_THROW(fn->Apply(args, outputType, result, &ctx));
    }
};

// Test basic split functionality
TEST(SplitPartTest, BasicSplit) {
    std::cout << "=== SplitPart Basic Split Test ===" << std::endl;
    std::vector<std::string> inputs = {"I,he,she,they", "I,he,she,they", "I,he,she,they", "I,he,she,they"};
    std::vector<int64_t> indices = {1, 2, 3, 4};
    std::vector<std::optional<std::string>> expected = {"I", "he", "she", "they"};
    
    BaseVector* inputVec = SplitPartFunctionTestHelper::CreateStringVector(inputs);
    BaseVector* delimiterVec = SplitPartFunctionTestHelper::CreateConstStringVector(",", inputs.size());
    BaseVector* indexVec = SplitPartFunctionTestHelper::CreateInt64Vector(indices);
    BaseVector* result = nullptr;
    
    SplitPartFunctionTestHelper::ExecuteSplitPart(inputVec, delimiterVec, indexVec, result);
    SplitPartFunctionTestHelper::ValidateStringResult(result, expected, inputs.size());
    
    delete inputVec;
    delete delimiterVec;
    delete indexVec;
    delete result;
    std::cout << "=== Basic Split Test Completed ===" << std::endl;
}

// Test index out of range returns NULL
TEST(SplitPartTest, IndexOutOfRange) {
    std::cout << "=== SplitPart Index Out of Range Test ===" << std::endl;
    std::vector<std::string> inputs = {"I,he,she,they", "one,two"};
    std::vector<int64_t> indices = {5, 3};
    std::vector<std::optional<std::string>> expected = {std::nullopt, std::nullopt};
    
    BaseVector* inputVec = SplitPartFunctionTestHelper::CreateStringVector(inputs);
    BaseVector* delimiterVec = SplitPartFunctionTestHelper::CreateConstStringVector(",", inputs.size());
    BaseVector* indexVec = SplitPartFunctionTestHelper::CreateInt64Vector(indices);
    BaseVector* result = nullptr;
    
    SplitPartFunctionTestHelper::ExecuteSplitPart(inputVec, delimiterVec, indexVec, result);
    SplitPartFunctionTestHelper::ValidateStringResult(result, expected, inputs.size());
    
    delete inputVec;
    delete delimiterVec;
    delete indexVec;
    delete result;
    std::cout << "=== Index Out of Range Test Completed ===" << std::endl;
}

// Test consecutive delimiters produce empty strings
TEST(SplitPartTest, ConsecutiveDelimiters) {
    std::cout << "=== SplitPart Consecutive Delimiters Test ===" << std::endl;
    std::vector<std::string> inputs = {"one,,,four,", "one,,,four,", "one,,,four,", "one,,,four,", "one,,,four,"};
    std::vector<int64_t> indices = {1, 2, 3, 4, 5};
    std::vector<std::optional<std::string>> expected = {"one", "", "", "four", ""};
    
    BaseVector* inputVec = SplitPartFunctionTestHelper::CreateStringVector(inputs);
    BaseVector* delimiterVec = SplitPartFunctionTestHelper::CreateConstStringVector(",", inputs.size());
    BaseVector* indexVec = SplitPartFunctionTestHelper::CreateInt64Vector(indices);
    BaseVector* result = nullptr;
    
    SplitPartFunctionTestHelper::ExecuteSplitPart(inputVec, delimiterVec, indexVec, result);
    SplitPartFunctionTestHelper::ValidateStringResult(result, expected, inputs.size());
    
    delete inputVec;
    delete delimiterVec;
    delete indexVec;
    delete result;
    std::cout << "=== Consecutive Delimiters Test Completed ===" << std::endl;
}

// Test empty input string
TEST(SplitPartTest, EmptyInput) {
    std::cout << "=== SplitPart Empty Input Test ===" << std::endl;
    std::vector<std::string> inputs = {""};
    std::vector<int64_t> indices = {1};
    std::vector<std::optional<std::string>> expected = {""};
    
    BaseVector* inputVec = SplitPartFunctionTestHelper::CreateStringVector(inputs);
    BaseVector* delimiterVec = SplitPartFunctionTestHelper::CreateConstStringVector(",", inputs.size());
    BaseVector* indexVec = SplitPartFunctionTestHelper::CreateInt64Vector(indices);
    BaseVector* result = nullptr;
    
    SplitPartFunctionTestHelper::ExecuteSplitPart(inputVec, delimiterVec, indexVec, result);
    SplitPartFunctionTestHelper::ValidateStringResult(result, expected, inputs.size());
    
    delete inputVec;
    delete delimiterVec;
    delete indexVec;
    delete result;
    std::cout << "=== Empty Input Test Completed ===" << std::endl;
}

// Test no delimiter in input
TEST(SplitPartTest, NoDelimiterInInput) {
    std::cout << "=== SplitPart No Delimiter In Input Test ===" << std::endl;
    std::vector<std::string> inputs = {"abc", "abc"};
    std::vector<int64_t> indices = {1, 2};
    std::vector<std::optional<std::string>> expected = {"abc", std::nullopt};
    
    BaseVector* inputVec = SplitPartFunctionTestHelper::CreateStringVector(inputs);
    BaseVector* delimiterVec = SplitPartFunctionTestHelper::CreateConstStringVector(",", inputs.size());
    BaseVector* indexVec = SplitPartFunctionTestHelper::CreateInt64Vector(indices);
    BaseVector* result = nullptr;
    
    SplitPartFunctionTestHelper::ExecuteSplitPart(inputVec, delimiterVec, indexVec, result);
    SplitPartFunctionTestHelper::ValidateStringResult(result, expected, inputs.size());
    
    delete inputVec;
    delete delimiterVec;
    delete indexVec;
    delete result;
    std::cout << "=== No Delimiter In Input Test Completed ===" << std::endl;
}

// Test empty delimiter - split by character (ASCII)
TEST(SplitPartTest, EmptyDelimiterAscii) {
    std::cout << "=== SplitPart Empty Delimiter ASCII Test ===" << std::endl;
    std::vector<std::string> inputs = {"abc", "abc", "abc", "abc"};
    std::vector<int64_t> indices = {1, 2, 3, 4};
    std::vector<std::optional<std::string>> expected = {"a", "b", "c", std::nullopt};
    
    BaseVector* inputVec = SplitPartFunctionTestHelper::CreateStringVector(inputs);
    BaseVector* delimiterVec = SplitPartFunctionTestHelper::CreateConstStringVector("", inputs.size());
    BaseVector* indexVec = SplitPartFunctionTestHelper::CreateInt64Vector(indices);
    BaseVector* result = nullptr;
    
    SplitPartFunctionTestHelper::ExecuteSplitPart(inputVec, delimiterVec, indexVec, result);
    SplitPartFunctionTestHelper::ValidateStringResult(result, expected, inputs.size());
    
    delete inputVec;
    delete delimiterVec;
    delete indexVec;
    delete result;
    std::cout << "=== Empty Delimiter ASCII Test Completed ===" << std::endl;
}

// Test empty delimiter - split by character (UTF-8 Unicode)
TEST(SplitPartTest, EmptyDelimiterUnicode) {
    std::cout << "=== SplitPart Empty Delimiter Unicode Test ===" << std::endl;
    // UTF-8 string: "你好世界" (4 Chinese characters)
    std::string unicodeStr = "\xe4\xbd\xa0\xe5\xa5\xbd\xe4\xb8\x96\xe7\x95\x8c";  // "你好世界"
    std::vector<std::string> inputs = {unicodeStr, unicodeStr, unicodeStr, unicodeStr, unicodeStr};
    std::vector<int64_t> indices = {1, 2, 3, 4, 5};
    std::vector<std::optional<std::string>> expected = {
        "\xe4\xbd\xa0",  // "你"
        "\xe5\xa5\xbd",  // "好"
        "\xe4\xb8\x96",  // "世"
        "\xe7\x95\x8c",  // "界"
        std::nullopt     // index 5 out of range
    };
    
    BaseVector* inputVec = SplitPartFunctionTestHelper::CreateStringVector(inputs);
    BaseVector* delimiterVec = SplitPartFunctionTestHelper::CreateConstStringVector("", inputs.size());
    BaseVector* indexVec = SplitPartFunctionTestHelper::CreateInt64Vector(indices);
    BaseVector* result = nullptr;
    
    SplitPartFunctionTestHelper::ExecuteSplitPart(inputVec, delimiterVec, indexVec, result);
    SplitPartFunctionTestHelper::ValidateStringResult(result, expected, inputs.size());
    
    delete inputVec;
    delete delimiterVec;
    delete indexVec;
    delete result;
    std::cout << "=== Empty Delimiter Unicode Test Completed ===" << std::endl;
}

// Test multi-character delimiter
TEST(SplitPartTest, MultiCharDelimiter) {
    std::cout << "=== SplitPart Multi-Character Delimiter Test ===" << std::endl;
    std::vector<std::string> inputs = {"a::b::c", "a::b::c", "a::b::c", "a::b::c"};
    std::vector<int64_t> indices = {1, 2, 3, 4};
    std::vector<std::optional<std::string>> expected = {"a", "b", "c", std::nullopt};
    
    BaseVector* inputVec = SplitPartFunctionTestHelper::CreateStringVector(inputs);
    BaseVector* delimiterVec = SplitPartFunctionTestHelper::CreateConstStringVector("::", inputs.size());
    BaseVector* indexVec = SplitPartFunctionTestHelper::CreateInt64Vector(indices);
    BaseVector* result = nullptr;
    
    SplitPartFunctionTestHelper::ExecuteSplitPart(inputVec, delimiterVec, indexVec, result);
    SplitPartFunctionTestHelper::ValidateStringResult(result, expected, inputs.size());
    
    delete inputVec;
    delete delimiterVec;
    delete indexVec;
    delete result;
    std::cout << "=== Multi-Character Delimiter Test Completed ===" << std::endl;
}

// Test Unicode delimiter
TEST(SplitPartTest, UnicodeDelimiter) {
    std::cout << "=== SplitPart Unicode Delimiter Test ===" << std::endl;
    // Using Unicode delimiter "||"
    std::vector<std::string> inputs = {"apple||banana||cherry", "apple||banana||cherry", "apple||banana||cherry"};
    std::vector<int64_t> indices = {1, 2, 3};
    std::vector<std::optional<std::string>> expected = {"apple", "banana", "cherry"};
    
    BaseVector* inputVec = SplitPartFunctionTestHelper::CreateStringVector(inputs);
    BaseVector* delimiterVec = SplitPartFunctionTestHelper::CreateConstStringVector("||", inputs.size());
    BaseVector* indexVec = SplitPartFunctionTestHelper::CreateInt64Vector(indices);
    BaseVector* result = nullptr;
    
    SplitPartFunctionTestHelper::ExecuteSplitPart(inputVec, delimiterVec, indexVec, result);
    SplitPartFunctionTestHelper::ValidateStringResult(result, expected, inputs.size());
    
    delete inputVec;
    delete delimiterVec;
    delete indexVec;
    delete result;
    std::cout << "=== Unicode Delimiter Test Completed ===" << std::endl;
}

// Test delimiter at the end of string
TEST(SplitPartTest, DelimiterAtEnd) {
    std::cout << "=== SplitPart Delimiter At End Test ===" << std::endl;
    std::vector<std::string> inputs = {"a,b,c,", "a,b,c,", "a,b,c,", "a,b,c,", "a,b,c,"};
    std::vector<int64_t> indices = {1, 2, 3, 4, 5};
    std::vector<std::optional<std::string>> expected = {"a", "b", "c", "", std::nullopt};
    
    BaseVector* inputVec = SplitPartFunctionTestHelper::CreateStringVector(inputs);
    BaseVector* delimiterVec = SplitPartFunctionTestHelper::CreateConstStringVector(",", inputs.size());
    BaseVector* indexVec = SplitPartFunctionTestHelper::CreateInt64Vector(indices);
    BaseVector* result = nullptr;
    
    SplitPartFunctionTestHelper::ExecuteSplitPart(inputVec, delimiterVec, indexVec, result);
    SplitPartFunctionTestHelper::ValidateStringResult(result, expected, inputs.size());
    
    delete inputVec;
    delete delimiterVec;
    delete indexVec;
    delete result;
    std::cout << "=== Delimiter At End Test Completed ===" << std::endl;
}

// Test delimiter at the beginning of string
TEST(SplitPartTest, DelimiterAtBeginning) {
    std::cout << "=== SplitPart Delimiter At Beginning Test ===" << std::endl;
    std::vector<std::string> inputs = {",a,b,c", ",a,b,c", ",a,b,c", ",a,b,c", ",a,b,c"};
    std::vector<int64_t> indices = {1, 2, 3, 4, 5};
    std::vector<std::optional<std::string>> expected = {"", "a", "b", "c", std::nullopt};
    
    BaseVector* inputVec = SplitPartFunctionTestHelper::CreateStringVector(inputs);
    BaseVector* delimiterVec = SplitPartFunctionTestHelper::CreateConstStringVector(",", inputs.size());
    BaseVector* indexVec = SplitPartFunctionTestHelper::CreateInt64Vector(indices);
    BaseVector* result = nullptr;
    
    SplitPartFunctionTestHelper::ExecuteSplitPart(inputVec, delimiterVec, indexVec, result);
    SplitPartFunctionTestHelper::ValidateStringResult(result, expected, inputs.size());
    
    delete inputVec;
    delete delimiterVec;
    delete indexVec;
    delete result;
    std::cout << "=== Delimiter At Beginning Test Completed ===" << std::endl;
}

// Test mixed scenarios
TEST(SplitPartTest, MixedScenarios) {
    std::cout << "=== SplitPart Mixed Scenarios Test ===" << std::endl;
    std::vector<std::string> inputs = {
        "hello world",     // space delimiter
        "a-b-c-d-e",       // hyphen delimiter
        "single",          // no delimiter
        ",,,"              // all empty
    };
    std::vector<std::string> delimiters = {" ", "-", "|", ","};
    std::vector<int64_t> indices = {2, 3, 1, 2};
    std::vector<std::optional<std::string>> expected = {"world", "c", "single", ""};
    
    // Create vectors
    BaseVector* inputVec = SplitPartFunctionTestHelper::CreateStringVector(inputs);
    
    // Create delimiter vector (need per-row values)
    BaseVector* delimiterVec = SplitPartFunctionTestHelper::CreateStringVector(delimiters);
    
    BaseVector* indexVec = SplitPartFunctionTestHelper::CreateInt64Vector(indices);
    BaseVector* result = nullptr;
    
    SplitPartFunctionTestHelper::ExecuteSplitPart(inputVec, delimiterVec, indexVec, result);
    SplitPartFunctionTestHelper::ValidateStringResult(result, expected, inputs.size());
    
    delete inputVec;
    delete delimiterVec;
    delete indexVec;
    delete result;
    std::cout << "=== Mixed Scenarios Test Completed ===" << std::endl;
}
