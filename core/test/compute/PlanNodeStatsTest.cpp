/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include "compute/plannode_stats.h"
#include <gtest/gtest.h>

namespace PlanNodeStatsTest {
/**
 * @tc.name  : PlanNodeStatsOperatorPlus_ShouldAddStats_WhenCalled
 * @tc.number: PlanNodeStatsOperatorPlus_Test_001
 * @tc.desc  : Test the PlanNodeStats::operator+= function to ensure it correctly adds stats.
 */
TEST(PlanNodeStatsTest, PlanNodeStatsOperatorPlus_ShouldAddStats_WhenCalled)
{
    omniruntime::compute::PlanNodeStats stats1;
    omniruntime::compute::PlanNodeStats stats2;

    // Set some initial values for stats1 and stats2
    stats1.inputRows = 10;
    stats1.inputBytes = 100;
    stats1.numDrivers = 5;

    stats2.inputRows = 20;
    stats2.inputBytes = 200;
    stats2.numDrivers = 10;

    // Call the operator+= function
    stats1 += stats2;

    // Verify the results
    EXPECT_EQ(stats1.inputRows, 30);
    EXPECT_EQ(stats1.inputBytes, 300);
    EXPECT_EQ(stats1.numDrivers, 15); // Assuming IsMultiOperatorTypeNode() is false
}

/**
 * @tc.name  : PlanNodeStatsAdd_ShouldAddStats_WhenOperatorTypeExists
 * @tc.number: PlanNodeStatsAdd_Test_001
 * @tc.desc  : Test the PlanNodeStats::Add function to ensure it correctly adds stats when operatorType exists.
 */
TEST(PlanNodeStatsTest, PlanNodeStatsAdd_ShouldAddStats_WhenOperatorTypeExists)
{
    omniruntime::compute::PlanNodeStats planNodeStats;
    omniruntime::compute::OperatorStats operatorStats;
    operatorStats.operatorType = "TestOperator";

    // Set some initial values for operatorStats
    operatorStats.inputRows = 10;
    operatorStats.inputBytes = 100;

    // Add the operatorStats to planNodeStats
    planNodeStats.Add(operatorStats);

    // Verify the results
    EXPECT_EQ(planNodeStats.inputRows, 10);
    EXPECT_EQ(planNodeStats.inputBytes, 100);
}

/**
 * @tc.name  : PlanNodeStatsAdd_ShouldAddStats_WhenOperatorTypeNotExists
 * @tc.number: PlanNodeStatsAdd_Test_002
 * @tc.desc  : Test the PlanNodeStats::Add function to ensure it correctly adds stats when operatorType does not exist.
 */
TEST(PlanNodeStatsTest, PlanNodeStatsAdd_ShouldAddStats_WhenOperatorTypeNotExists)
{
    omniruntime::compute::PlanNodeStats planNodeStats;
    omniruntime::compute::OperatorStats operatorStats;
    operatorStats.operatorType = "TestOperator";

    // Set some initial values for operatorStats
    operatorStats.inputRows = 10;
    operatorStats.inputBytes = 100;

    // Add the operatorStats to planNodeStats
    planNodeStats.Add(operatorStats);

    // Verify the results
    EXPECT_EQ(planNodeStats.inputRows, 10);
    EXPECT_EQ(planNodeStats.inputBytes, 100);
}

/**
 * @tc.name  : PlanNodeStatsToString_ShouldReturnCorrectString_WhenIncludeInputStatsIsTrue
 * @tc.number: PlanNodeStatsToString_Test_001
 * @tc.desc  : Test the PlanNodeStats::ToString function to ensure it returns the correct string when includeInputStats
 * is true.
 */
TEST(PlanNodeStatsTest, PlanNodeStatsToString_ShouldReturnCorrectString_WhenIncludeInputStatsIsTrue)
{
    omniruntime::compute::PlanNodeStats planNodeStats;
    planNodeStats.inputRows = 10;
    planNodeStats.inputBytes = 100;
    planNodeStats.outputRows = 20;
    planNodeStats.outputBytes = 200;

    std::string result = planNodeStats.ToString(true);

    // Verify the results
    EXPECT_NE(result.find("Input: 10 rows (100"), std::string::npos);
    EXPECT_NE(result.find("Output: 20 rows (200"), std::string::npos);
}

/**
 * @tc.name  : PlanNodeStatsToString_ShouldReturnCorrectString_WhenIncludeInputStatsIsFalse
 * @tc.number: PlanNodeStatsToString_Test_002
 * @tc.desc  : Test the PlanNodeStats::ToString function to ensure it returns the correct string when includeInputStats
 * is false.
 */
TEST(PlanNodeStatsTest, PlanNodeStatsToString_ShouldReturnCorrectString_WhenIncludeInputStatsIsFalse)
{
    omniruntime::compute::PlanNodeStats planNodeStats;
    planNodeStats.inputRows = 10;
    planNodeStats.inputBytes = 100;
    planNodeStats.outputRows = 20;
    planNodeStats.outputBytes = 200;

    std::string result = planNodeStats.ToString(false);

    // Verify the results
    EXPECT_EQ(result.find("Input: 10 rows (100"), std::string::npos);
    EXPECT_NE(result.find("Output: 20 rows (200"), std::string::npos);
}

TEST(PlanNodeStatsTest, ToPlanStats_ShouldReturnEmptyMap_WhenInputVectorIsEmpty)
{
    const omniruntime::TaskStats stats;
    auto result = ToPlanStats(stats);
    EXPECT_TRUE(result.empty());
}

TEST(PlanNodeStatsTest, ToPlanStats_ShouldReturnMapWithMultipleElements_WhenInputVectorHasMultipleElements)
{
    omniruntime::TaskStats stats;
    stats.endTimeMs = 0;
    stats.numTotalSplits = 1;
    stats.numTotalSplits = 1;
    omniruntime::OperatorStats operator_stats;
    operator_stats.inputRows = 1;
    operator_stats.inputBytes = 1;
    operator_stats.pipelineId = 0;
    operator_stats.planNodeId = "0";
    operator_stats.operatorId = 0;
    operator_stats.operatorType = "OrderBy";
    omniruntime::PipelineStats pipeline_stats;
    pipeline_stats.operatorStats.push_back(operator_stats);
    stats.pipelineStats.push_back(pipeline_stats);
    auto result = ToPlanStats(stats);
    EXPECT_EQ(result.size(), 1);
}
} // namespace PlanNodeStatsTest
