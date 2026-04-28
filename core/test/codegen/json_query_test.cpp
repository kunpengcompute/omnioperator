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
}