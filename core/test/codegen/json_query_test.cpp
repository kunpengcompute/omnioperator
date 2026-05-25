/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2026. All rights reserved.
 * Description: json_query function test
 */
#include <string>
#include "gtest/gtest.h"
#include "codegen/functions/stringfunctions.h"
#include "operator/execution_context.h"

namespace omniruntime {
using namespace omniruntime::codegen::function;
using namespace omniruntime::op;

namespace {
constexpr int32_t WITHOUT_ARRAY_WRAPPER = 0;
constexpr int32_t WITH_CONDITIONAL_ARRAY_WRAPPER = 1;
constexpr int32_t WITH_UNCONDITIONAL_ARRAY_WRAPPER = 2;
constexpr int32_t NULL_BEHAVIOR = 0;
constexpr int32_t EMPTY_ARRAY_BEHAVIOR = 1;
constexpr int32_t EMPTY_OBJECT_BEHAVIOR = 2;
constexpr int32_t ERROR_BEHAVIOR = 3;
}

static void JsonQueryTest(const std::string &jsonStr, const std::string &path,
    const std::string &expectedResult, bool expectIsNull)
{
    auto context = new ExecutionContext();
    int64_t contextPtr = reinterpret_cast<int64_t>(context);

    bool outIsNull = false;
    int32_t outLen = 0;

    const char *result = JsonQueryRetNull(
        contextPtr,
        jsonStr.c_str(), static_cast<int32_t>(jsonStr.size()), false,
        path.c_str(), 0, static_cast<int32_t>(path.size()), false,
        &outIsNull, &outLen);

    if (expectIsNull) {
        EXPECT_TRUE(outIsNull);
        EXPECT_EQ(outLen, 0);
    } else {
        EXPECT_FALSE(outIsNull);
        EXPECT_EQ(expectedResult, std::string(result, outLen));
    }

    delete context;
}

static void JsonQueryWithBehaviorTest(const std::string &jsonStr, const std::string &path,
    int32_t wrapperBehavior, int32_t emptyBehavior, int32_t errorBehavior,
    const std::string &expectedResult, bool expectIsNull, bool expectError = false,
    const std::string &expectedError = "")
{
    auto context = new ExecutionContext();
    int64_t contextPtr = reinterpret_cast<int64_t>(context);

    bool outIsNull = false;
    int32_t outLen = 0;

    const char *result = JsonQueryWithWrapperAndBehavior(
        contextPtr,
        jsonStr.c_str(), static_cast<int32_t>(jsonStr.size()), false,
        path.c_str(), 0, static_cast<int32_t>(path.size()), false,
        wrapperBehavior, false,
        emptyBehavior, false,
        errorBehavior, false,
        &outIsNull, &outLen);

    if (expectIsNull) {
        EXPECT_TRUE(outIsNull);
        EXPECT_EQ(outLen, 0);
    } else {
        EXPECT_FALSE(outIsNull);
        ASSERT_NE(result, nullptr);
        EXPECT_EQ(expectedResult, std::string(result, outLen));
    }

    EXPECT_EQ(expectError, context->HasError());
    if (expectError) {
        EXPECT_EQ(expectedError, context->GetError());
    }

    delete context;
}

TEST(JsonQueryTest, ExtractObjectFragment)
{
    JsonQueryTest(R"({"roomInfos":{"id":1,"name":"A"},"meta":{"count":2}})",
        "$.roomInfos", R"({"id":1,"name":"A"})", false);
}

TEST(JsonQueryTest, ExtractArrayFragment)
{
    JsonQueryTest(R"({"roomInfos":[{"id":1},{"id":2}]})",
        "$.roomInfos", R"([{"id":1},{"id":2}])", false);
}

TEST(JsonQueryTest, ExtractNestedArrayElementObject)
{
    JsonQueryTest(R"({"roomInfos":[{"id":1,"attrs":{"status":"open"}}]})",
        "$.roomInfos[0].attrs", R"({"status":"open"})", false);
}

TEST(JsonQueryTest, PreserveObjectKeyOrder)
{
    JsonQueryTest(R"({"user":{"name":"Haley Johnson","age":20,"tags":["x","y"]}})",
        "$.user", R"({"name":"Haley Johnson","age":20,"tags":["x","y"]})", false);
}

TEST(JsonQueryTest, MissingPathReturnsNull)
{
    JsonQueryTest(R"({"roomInfos":[]})", "$.missing", "", true);
}

TEST(JsonQueryTest, ScalarResultReturnsNull)
{
    JsonQueryTest(R"({"roomInfos":{"name":"A"}})", "$.roomInfos.name", "", true);
}

TEST(JsonQueryTest, InvalidPathReturnsNull)
{
    JsonQueryTest(R"({"roomInfos":[]})", "roomInfos", "", true);
}

TEST(JsonQueryTest, NullInputReturnsNull)
{
    auto context = new ExecutionContext();
    int64_t contextPtr = reinterpret_cast<int64_t>(context);

    bool outIsNull = false;
    int32_t outLen = 0;
    std::string json = R"({"roomInfos":[]})";
    std::string path = "$.roomInfos";

    JsonQueryRetNull(
        contextPtr,
        json.c_str(), static_cast<int32_t>(json.size()), true,
        path.c_str(), 0, static_cast<int32_t>(path.size()), false,
        &outIsNull, &outLen);

    EXPECT_TRUE(outIsNull);
    EXPECT_EQ(outLen, 0);

    delete context;
}

TEST(JsonQueryTest, RootPathReturnsFullDocument)
{
    JsonQueryWithBehaviorTest(R"({"roomInfos":{"id":1},"meta":{"count":2}})", "$",
        WITHOUT_ARRAY_WRAPPER, NULL_BEHAVIOR, NULL_BEHAVIOR,
        R"({"roomInfos":{"id":1},"meta":{"count":2}})", false);
}

TEST(JsonQueryTest, ConditionalWrapperAllowsScalarResult)
{
    JsonQueryWithBehaviorTest(R"({"roomInfos":{"name":"A"}})", "$.roomInfos.name",
        WITH_CONDITIONAL_ARRAY_WRAPPER, NULL_BEHAVIOR, NULL_BEHAVIOR,
        R"(["A"])", false);
}

TEST(JsonQueryTest, EmptyArrayBehaviorReturnsArrayLiteral)
{
    JsonQueryWithBehaviorTest(R"({"roomInfos":[]})", "lax $.missing",
        WITHOUT_ARRAY_WRAPPER, EMPTY_ARRAY_BEHAVIOR, NULL_BEHAVIOR,
        "[]", false);
}

TEST(JsonQueryTest, EmptyObjectOnStrictErrorReturnsObjectLiteral)
{
    JsonQueryWithBehaviorTest(R"({"roomInfos":[]})", "strict $.missing",
        WITHOUT_ARRAY_WRAPPER, NULL_BEHAVIOR, EMPTY_OBJECT_BEHAVIOR,
        "{}", false);
}

TEST(JsonQueryTest, ErrorOnEmptySetsExecutionError)
{
    JsonQueryWithBehaviorTest(R"({"roomInfos":[]})", "lax $.missing",
        WITHOUT_ARRAY_WRAPPER, ERROR_BEHAVIOR, NULL_BEHAVIOR,
        "", true, true, "Empty result of JSON_QUERY function is not allowed");
}
}