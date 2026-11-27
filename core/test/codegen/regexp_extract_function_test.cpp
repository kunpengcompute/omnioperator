/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: regexp_extract codegen function tests
 */
#include <limits>
#include <string>

#include "gtest/gtest.h"
#include "codegen/functions/stringfunctions.h"
#include "operator/execution_context.h"

namespace omniruntime {
namespace {
using namespace omniruntime::codegen::function;
using namespace omniruntime::op;

void ExpectEmptyGroupResult(int32_t groupIndex)
{
    ExecutionContext context;
    const auto contextPtr = reinterpret_cast<int64_t>(&context);
    const std::string input = "a";
    const std::string pattern = "(a)";
    int32_t outLen = -1;

    const char *result = Re2SearchAndExtract(contextPtr, input.data(), static_cast<int32_t>(input.size()),
        pattern.data(), static_cast<int32_t>(pattern.size()), groupIndex, false, &outLen);

    EXPECT_NE(result, nullptr);
    EXPECT_EQ(outLen, 0);
    EXPECT_FALSE(context.HasError());
}

TEST(RegexpExtractCodegenTest, OutOfRangeGroupDoesNotGrowThreadLocalBuffer)
{
    ExpectEmptyGroupResult(2);
    ExpectEmptyGroupResult(std::numeric_limits<int32_t>::max());
}

TEST(RegexpExtractCodegenTest, OptionalUnmatchedGroupReturnsEmptyWithoutCopy)
{
    ExecutionContext context;
    const auto contextPtr = reinterpret_cast<int64_t>(&context);
    const std::string input = "b";
    const std::string pattern = "(a)?b";
    int32_t outLen = -1;

    const char *result = Re2SearchAndExtract(contextPtr, input.data(), static_cast<int32_t>(input.size()),
        pattern.data(), static_cast<int32_t>(pattern.size()), 1, false, &outLen);

    EXPECT_NE(result, nullptr);
    EXPECT_EQ(outLen, 0);
    EXPECT_FALSE(context.HasError());
}

TEST(RegexpExtractCodegenTest, CallerContextReturnsViewIntoInput)
{
    ExecutionContext context;
    const auto contextPtr = reinterpret_cast<int64_t>(&context);
    const std::string input =
        "prefix/application_1700000000000_42_JId_7_SId_8_9/suffix";
    const std::string pattern =
        R"((application_\d+_\d+_JId_\d+_SId_\d+_\d+))";
    const std::string expected = "application_1700000000000_42_JId_7_SId_8_9";
    int32_t outLen = -1;

    const char *result = Re2SearchAndExtract(contextPtr, input.data(), static_cast<int32_t>(input.size()),
        pattern.data(), static_cast<int32_t>(pattern.size()), 1, false, &outLen);

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(std::string_view(result, static_cast<size_t>(outLen)), expected);
    EXPECT_EQ(result, input.data() + input.find(expected));
    EXPECT_FALSE(context.HasError());
}

TEST(RegexpExtractCodegenTest, LiteralFastPathReturnsViewIntoInput)
{
    ExecutionContext context;
    const auto contextPtr = reinterpret_cast<int64_t>(&context);
    const std::string input = "prefix-application_-suffix";
    const std::string pattern = "application_";
    int32_t outLen = -1;

    const char *result = Re2SearchAndExtract(contextPtr, input.data(), static_cast<int32_t>(input.size()),
        pattern.data(), static_cast<int32_t>(pattern.size()), 0, false, &outLen);

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(std::string_view(result, static_cast<size_t>(outLen)), pattern);
    EXPECT_EQ(result, input.data() + 7);
    EXPECT_FALSE(context.HasError());
}

} // namespace
} // namespace omniruntime
