/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Unit tests for the Flink specific string functions
 *   flink_substr(string, pos[, length]) -> varchar   (BinaryStringDataUtil.substringSQL)
 *   flink_replace(string, search, replacement) -> varchar   (Java String.replace)
 * They exist next to Spark's "substr"/"replace" because the boundary semantics differ.
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

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::codegen;
using namespace omniruntime::TestUtil;

namespace {
using VarcharVector = Vector<LargeStringContainer<std::string_view>>;

BaseVector *MakeStringVector(const std::vector<std::string> &values, const std::vector<bool> &nulls)
{
    // CreateFlatVector keeps the OMNI_VARCHAR type id, which is what the registrations use.
    BaseVector *vec = VectorHelper::CreateFlatVector(OMNI_VARCHAR, values.size());
    vec->SetIsField(true);
    auto *typed = dynamic_cast<VarcharVector *>(vec);
    EXPECT_NE(typed, nullptr);
    for (size_t i = 0; i < values.size(); ++i) {
        std::string_view sv(values[i]);
        typed->SetValue(i, sv);
        nulls[i] ? vec->SetNull(i) : vec->SetNotNull(i);
    }
    return vec;
}

BaseVector *MakeInt32Vector(const std::vector<int32_t> &values, const std::vector<bool> &nulls)
{
    BaseVector *vec = VectorHelper::CreateFlatVector(OMNI_INT, values.size());
    vec->SetIsField(true);
    auto *typed = static_cast<Vector<int32_t> *>(vec);
    for (size_t i = 0; i < values.size(); ++i) {
        typed->SetValue(i, values[i]);
        nulls[i] ? vec->SetNull(i) : vec->SetNotNull(i);
    }
    return vec;
}

/// Applies a simple function by pushing the arguments in ExprEval order (left to right).
void ApplyStringFunction(const std::string &name, const std::vector<BaseVector *> &inputs,
    BaseVector *&result)
{
    link_register_functions();

    std::vector<DataTypeId> inputTypes;
    inputTypes.reserve(inputs.size());
    for (const auto *input : inputs) {
        inputTypes.push_back(input->GetTypeId());
    }
    auto signature = std::make_shared<FunctionSignature>(name, inputTypes, OMNI_VARCHAR);
    auto function = VectorFunction::Find(signature);
    ASSERT_NE(function, nullptr) << name << " not found for the given signature";

    ExecutionContext context;
    context.SetResultRowSize(inputs[0]->GetSize());
    std::stack<BaseVector *> args;
    for (auto *input : inputs) {
        args.push(input);
    }
    auto outputType = std::make_shared<DataType>(OMNI_VARCHAR);
    ASSERT_NO_THROW(function->Apply(args, outputType, result, &context));
    ASSERT_NE(result, nullptr);
}

void ExpectStrings(BaseVector *result, const std::vector<std::string> &expected,
    const std::vector<bool> &expectedNulls)
{
    ASSERT_EQ(result->GetSize(), static_cast<int32_t>(expected.size()));
    auto *typed = dynamic_cast<VarcharVector *>(result);
    ASSERT_NE(typed, nullptr);
    for (size_t i = 0; i < expected.size(); ++i) {
        if (expectedNulls[i]) {
            EXPECT_TRUE(result->IsNull(i)) << "Row " << i << " should be NULL";
            continue;
        }
        ASSERT_FALSE(result->IsNull(i)) << "Row " << i << " should not be NULL";
        std::string actual(typed->GetValue(i));
        EXPECT_EQ(actual, expected[i]) << "Row " << i << " mismatch";
    }
}
} // namespace

TEST(FlinkSubstrTest, ThreeArgBoundaries) {
    //                                pos  len   expected
    // hello                            1    3   hel
    // hello                            6    1   ''      (pos past the end)
    // hello                            0    3   hel     (pos = 0 behaves like 1)
    // hello                           -3    2   ll
    // hello                           -6    2   ''      (negative pos out of range)
    // hello                            1    0   ''
    // hello                            1   -1   NULL    (negative length)
    // abcdef                           4   -1   NULL
    // 你好世界                          1    2   你好     (character based)
    std::vector<std::string> strings = {"hello", "hello", "hello", "hello", "hello",
                                        "hello", "hello", "abcdef", "你好世界"};
    std::vector<int32_t> positions = {1, 6, 0, -3, -6, 1, 1, 4, 1};
    std::vector<int32_t> lengths = {3, 1, 3, 2, 2, 0, -1, -1, 2};
    std::vector<bool> noNulls(strings.size(), false);
    std::vector<std::string> expected = {"hel", "", "hel", "ll", "", "", "", "", "你好"};
    std::vector<bool> expectedNulls = {false, false, false, false, false, false, true, true, false};

    BaseVector *strVec = MakeStringVector(strings, noNulls);
    BaseVector *posVec = MakeInt32Vector(positions, noNulls);
    BaseVector *lenVec = MakeInt32Vector(lengths, noNulls);
    BaseVector *result = nullptr;
    ApplyStringFunction("flink_substr", {strVec, posVec, lenVec}, result);
    ExpectStrings(result, expected, expectedNulls);

    delete strVec;
    delete posVec;
    delete lenVec;
    delete result;
}

TEST(FlinkSubstrTest, TwoArgBoundaries) {
    std::vector<std::string> strings = {"hello", "hello", "hello", "hello", "", "abcdef"};
    std::vector<int32_t> positions = {1, 3, -3, -6, 1, -2};
    std::vector<bool> noNulls(strings.size(), false);
    std::vector<std::string> expected = {"hello", "llo", "llo", "", "", "ef"};
    std::vector<bool> expectedNulls(strings.size(), false);

    BaseVector *strVec = MakeStringVector(strings, noNulls);
    BaseVector *posVec = MakeInt32Vector(positions, noNulls);
    BaseVector *result = nullptr;
    ApplyStringFunction("flink_substr", {strVec, posVec}, result);
    ExpectStrings(result, expected, expectedNulls);

    delete strVec;
    delete posVec;
    delete result;
}

TEST(FlinkSubstrTest, NullArgumentsPropagate) {
    std::vector<std::string> strings = {"hello", "hello"};
    std::vector<int32_t> positions = {1, 2};
    std::vector<bool> stringNulls = {false, true};
    std::vector<bool> posNulls = {true, false};

    BaseVector *strVec = MakeStringVector(strings, stringNulls);
    BaseVector *posVec = MakeInt32Vector(positions, posNulls);
    BaseVector *result = nullptr;
    ApplyStringFunction("flink_substr", {strVec, posVec}, result);
    ExpectStrings(result, {"", ""}, {true, true});

    delete strVec;
    delete posVec;
    delete result;
}

TEST(FlinkReplaceTest, EmptySearchInsertsReplacement) {
    std::vector<std::string> strings = {"abc", "", "a", "你好"};
    std::vector<std::string> searches = {"", "", "", ""};
    std::vector<std::string> replacements = {"X", "X", "", "-"};
    std::vector<bool> noNulls(strings.size(), false);
    std::vector<std::string> expected = {"XaXbXcX", "X", "a", "-你-好-"};
    std::vector<bool> expectedNulls(strings.size(), false);

    BaseVector *strVec = MakeStringVector(strings, noNulls);
    BaseVector *searchVec = MakeStringVector(searches, noNulls);
    BaseVector *replacementVec = MakeStringVector(replacements, noNulls);
    BaseVector *result = nullptr;
    ApplyStringFunction("flink_replace", {strVec, searchVec, replacementVec}, result);
    ExpectStrings(result, expected, expectedNulls);

    delete strVec;
    delete searchVec;
    delete replacementVec;
    delete result;
}

TEST(FlinkReplaceTest, NonEmptySearchMatchesSpark) {
    std::vector<std::string> strings = {"hello world", "ababab", "aaa", "abc", "mississippi",
                                        "123123123", "no-match-here", ""};
    std::vector<std::string> searches = {"world", "abab", "aa", "abc", "ss", "12", "xyz", "abc"};
    std::vector<std::string> replacements = {"flink", "z", "X", "", "s", "", "QQ", "X"};
    std::vector<bool> noNulls(strings.size(), false);
    std::vector<std::string> expected = {"hello flink", "zab", "Xa", "", "misisippi", "333",
                                         "no-match-here", ""};
    std::vector<bool> expectedNulls(strings.size(), false);

    BaseVector *strVec = MakeStringVector(strings, noNulls);
    BaseVector *searchVec = MakeStringVector(searches, noNulls);
    BaseVector *replacementVec = MakeStringVector(replacements, noNulls);
    BaseVector *result = nullptr;
    ApplyStringFunction("flink_replace", {strVec, searchVec, replacementVec}, result);
    ExpectStrings(result, expected, expectedNulls);

    delete strVec;
    delete searchVec;
    delete replacementVec;
    delete result;
}

TEST(FlinkReplaceTest, NullArgumentsPropagate) {
    std::vector<std::string> strings = {"abc", "abc", "abc"};
    std::vector<std::string> searches = {"b", "b", "b"};
    std::vector<std::string> replacements = {"X", "X", "X"};

    BaseVector *strVec = MakeStringVector(strings, {true, false, false});
    BaseVector *searchVec = MakeStringVector(searches, {false, true, false});
    BaseVector *replacementVec = MakeStringVector(replacements, {false, false, true});
    BaseVector *result = nullptr;
    ApplyStringFunction("flink_replace", {strVec, searchVec, replacementVec}, result);
    ExpectStrings(result, {"", "", ""}, {true, true, true});

    delete strVec;
    delete searchVec;
    delete replacementVec;
    delete result;
}

// Spark's "substr"/"replace" must keep their original behaviour.
TEST(SparkSemanticsUnchangedTest, SubstrAndReplace) {
    std::vector<std::string> strings = {"hello", "hello"};
    std::vector<int32_t> positions = {-6, 1};
    std::vector<int32_t> lengths = {2, -1};
    std::vector<bool> noNulls(strings.size(), false);

    BaseVector *strVec = MakeStringVector(strings, noNulls);
    BaseVector *posVec = MakeInt32Vector(positions, noNulls);
    BaseVector *lenVec = MakeInt32Vector(lengths, noNulls);
    BaseVector *substrResult = nullptr;
    ApplyStringFunction("substr", {strVec, posVec, lenVec}, substrResult);
    ExpectStrings(substrResult, {"h", ""}, {false, false});

    std::vector<std::string> replaceInputs = {"abc"};
    BaseVector *replaceStrVec = MakeStringVector(replaceInputs, {false});
    BaseVector *searchVec = MakeStringVector({""}, {false});
    BaseVector *replacementVec = MakeStringVector({"X"}, {false});
    BaseVector *replaceResult = nullptr;
    ApplyStringFunction("replace", {replaceStrVec, searchVec, replacementVec}, replaceResult);
    ExpectStrings(replaceResult, {"abc"}, {false});

    delete strVec;
    delete posVec;
    delete lenVec;
    delete substrResult;
    delete replaceStrVec;
    delete searchVec;
    delete replacementVec;
    delete replaceResult;
}
