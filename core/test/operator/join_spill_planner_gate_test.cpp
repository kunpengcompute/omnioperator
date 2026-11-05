/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 * Description: Planner join-spill eligibility for BuildRight LeftSemi / LeftAnti / Existence.
 */
#include "gtest/gtest.h"
#include "compute/local_planner.h"
#include "expression/expressions.h"
#include "plannode/planNode.h"
#include "type/data_type.h"
#include "type/data_types.h"
#include "util/type_util.h"

#include <memory>
#include <vector>

using namespace omniruntime::expressions;
using namespace omniruntime::type;
using omniruntime::BuildSide;
using omniruntime::HashJoinNode;
using omniruntime::JoinType;
using omniruntime::OMNI_BUILD_LEFT;
using omniruntime::OMNI_BUILD_RIGHT;
using omniruntime::OMNI_JOIN_TYPE_EXISTENCE;
using omniruntime::OMNI_JOIN_TYPE_FULL;
using omniruntime::OMNI_JOIN_TYPE_INNER;
using omniruntime::OMNI_JOIN_TYPE_LEFT;
using omniruntime::OMNI_JOIN_TYPE_LEFT_ANTI;
using omniruntime::OMNI_JOIN_TYPE_LEFT_SEMI;
using omniruntime::compute::IsJoinSpillSupported;

namespace {

// HashJoinNode does not own its key expressions.
bool Supported(JoinType joinType, BuildSide buildSide, bool nullAware, DataTypePtr keyType, bool emptyKeys = false)
{
    std::vector<Expr *> leftKeys;
    std::vector<Expr *> rightKeys;
    if (!emptyKeys) {
        leftKeys.emplace_back(new FieldExpr(0, keyType));
        rightKeys.emplace_back(new FieldExpr(0, keyType));
    }
    DataTypes probeTypes(std::vector<DataTypePtr>({keyType, LongType()}));
    DataTypes buildTypes(std::vector<DataTypePtr>({keyType, LongType()}));
    std::vector<Expr *> partitionKeys;
    const bool supported = IsJoinSpillSupported(std::make_shared<const HashJoinNode>("0", joinType, buildSide,
        nullAware, true, leftKeys, rightKeys, nullptr, nullptr, nullptr, probeTypes.Instance(), buildTypes.Instance(),
        partitionKeys));
    Expr::DeleteExprs(leftKeys);
    Expr::DeleteExprs(rightKeys);
    return supported;
}

} // namespace

TEST(JoinSpillPlannerGateTest, PlainSemiAntiBuildRightAccepted)
{
    EXPECT_TRUE(Supported(OMNI_JOIN_TYPE_LEFT_SEMI, OMNI_BUILD_RIGHT, false, LongType()));
    EXPECT_TRUE(Supported(OMNI_JOIN_TYPE_LEFT_ANTI, OMNI_BUILD_RIGHT, false, IntType()));
    EXPECT_TRUE(Supported(OMNI_JOIN_TYPE_LEFT_SEMI, OMNI_BUILD_RIGHT, false, Date32Type()));
    EXPECT_TRUE(Supported(OMNI_JOIN_TYPE_LEFT_ANTI, OMNI_BUILD_RIGHT, false, VarcharType(16)));
}

TEST(JoinSpillPlannerGateTest, BooleanKeyAccepted)
{
    EXPECT_TRUE(Supported(OMNI_JOIN_TYPE_LEFT_SEMI, OMNI_BUILD_RIGHT, false, BooleanType()));
    EXPECT_TRUE(Supported(OMNI_JOIN_TYPE_LEFT_ANTI, OMNI_BUILD_RIGHT, false, BooleanType()));
    EXPECT_TRUE(Supported(OMNI_JOIN_TYPE_EXISTENCE, OMNI_BUILD_RIGHT, false, BooleanType()));
    EXPECT_TRUE(Supported(OMNI_JOIN_TYPE_INNER, OMNI_BUILD_RIGHT, false, BooleanType()));
    EXPECT_FALSE(Supported(OMNI_JOIN_TYPE_LEFT_SEMI, OMNI_BUILD_LEFT, false, BooleanType()));
    EXPECT_FALSE(Supported(OMNI_JOIN_TYPE_LEFT_ANTI, OMNI_BUILD_RIGHT, true, BooleanType()));
}

TEST(JoinSpillPlannerGateTest, BuildLeftSemiAntiRejected)
{
    EXPECT_FALSE(Supported(OMNI_JOIN_TYPE_LEFT_SEMI, OMNI_BUILD_LEFT, false, LongType()));
    EXPECT_FALSE(Supported(OMNI_JOIN_TYPE_LEFT_ANTI, OMNI_BUILD_LEFT, false, LongType()));
}

TEST(JoinSpillPlannerGateTest, ExistenceBuildRightAcceptedBuildLeftRejected)
{
    EXPECT_TRUE(Supported(OMNI_JOIN_TYPE_EXISTENCE, OMNI_BUILD_RIGHT, false, LongType()));
    EXPECT_TRUE(Supported(OMNI_JOIN_TYPE_EXISTENCE, OMNI_BUILD_RIGHT, false, VarcharType(16)));
    EXPECT_FALSE(Supported(OMNI_JOIN_TYPE_EXISTENCE, OMNI_BUILD_LEFT, false, LongType()));
}

TEST(JoinSpillPlannerGateTest, NullAwareAntiAndSemiRejected)
{
    EXPECT_FALSE(Supported(OMNI_JOIN_TYPE_LEFT_ANTI, OMNI_BUILD_RIGHT, true, LongType()));
    EXPECT_FALSE(Supported(OMNI_JOIN_TYPE_LEFT_SEMI, OMNI_BUILD_RIGHT, true, LongType()));
}

TEST(JoinSpillPlannerGateTest, UnsupportedAndEmptyKeysRejected)
{
    EXPECT_FALSE(Supported(OMNI_JOIN_TYPE_LEFT_SEMI, OMNI_BUILD_RIGHT, false, DoubleType()));
    EXPECT_FALSE(Supported(OMNI_JOIN_TYPE_LEFT_ANTI, OMNI_BUILD_RIGHT, false, LongType(), true));
    EXPECT_FALSE(Supported(OMNI_JOIN_TYPE_EXISTENCE, OMNI_BUILD_RIGHT, false, DoubleType()));
    EXPECT_FALSE(Supported(OMNI_JOIN_TYPE_EXISTENCE, OMNI_BUILD_RIGHT, false, LongType(), true));
}

TEST(JoinSpillPlannerGateTest, ExistingInnerAndOuterStayAccepted)
{
    EXPECT_TRUE(Supported(OMNI_JOIN_TYPE_INNER, OMNI_BUILD_RIGHT, false, LongType()));
    EXPECT_TRUE(Supported(OMNI_JOIN_TYPE_FULL, OMNI_BUILD_LEFT, false, LongType()));
    EXPECT_TRUE(Supported(OMNI_JOIN_TYPE_LEFT, OMNI_BUILD_RIGHT, false, LongType()));
}
