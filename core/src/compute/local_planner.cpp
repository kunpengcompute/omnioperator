/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
 * KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 */

#include "local_planner.h"

#include <operator/aggregation/group_aggregation_expr.h>

#include "operator/join/hash_builder_expr.h"
#include "operator/join/lookup_join_expr.h"
#include "operator/join/lookup_join_wrapper.h"
#include "operator/join/nest_loop_join_builder.h"
#include "operator/join/nest_loop_join_lookup_wrapper.h"
#include "operator/limit/limit.h"
#include "operator/sort/sort_expr.h"
#include "operator/topn/topn_expr.h"
#include "operator/topnsort/topn_sort_expr.h"
#include "operator/union/union.h"
#include "operator/window/window_expr.h"
#include "operator/join/sortmergejoin/sort_merge_join_expr_v2.h"
#include <operator/aggregation/non_group_aggregation_expr.h>
#include "operator/expand/expand.h"
#include "operator/grouping/grouping.h"

namespace omniruntime::compute {

// Returns true if source nodes must run in a separate pipeline
bool MustStartNewPipeline(int sourceId) { return sourceId != 0; }

std::shared_ptr<omniruntime::op::Operator> createOperator(OperatorFactory *factory,
                                                          const std::shared_ptr<const PlanNode> &planNode)
{
    std::shared_ptr<omniruntime::op::Operator> operatorPtr(factory->CreateOperator());
    operatorPtr->setNoMoreInput(false);
    operatorPtr->SetPlanNodeId(planNode->Id());
    if (operatorPtr->operatorType().empty()) {
        operatorPtr->SetOperatorType(string(planNode->Name()));
    }
    return std::move(operatorPtr);
}

std::shared_ptr<omniruntime::op::Operator>
createUnionBuildOperator(std::shared_ptr<omniruntime::op::Operator> unionOperator,
                         const std::shared_ptr<const PlanNode> &planNode)
{
    std::shared_ptr<omniruntime::op::Operator> unionBuildOperator = std::make_shared<UnionBuildOperator>(unionOperator);
    unionBuildOperator->setNoMoreInput(false);
    unionBuildOperator->SetPlanNodeId(planNode->Id());
    if (unionBuildOperator->operatorType().empty()) {
        unionBuildOperator->SetOperatorType(string(planNode->Name()));
    }
    return std::move(unionBuildOperator);
}

OperatorFactory *createOperatorFactory(const std::shared_ptr<const PlanNode> &planNode,
                                       const config::QueryConfig &queryConfig)
{
    if (auto orderByNode = std::dynamic_pointer_cast<const OrderByNode>(planNode)) {
        return SortWithExprOperatorFactory::CreateSortWithExprOperatorFactory(orderByNode, queryConfig);
    } else if (auto projectNode = std::dynamic_pointer_cast<const ProjectNode>(planNode)) {
        return CreateProjectOperatorFactory(projectNode, queryConfig);
    } else if (auto filterNode = std::dynamic_pointer_cast<const FilterNode>(planNode)) {
        return CreateFilterOperatorFactory(filterNode, queryConfig);
    } else if (auto windowNode = std::dynamic_pointer_cast<const WindowNode>(planNode)) {
        return WindowWithExprOperatorFactory::CreateWindowWithExprOperatorFactory(windowNode, queryConfig);
    } else if (auto topNNode = std::dynamic_pointer_cast<const TopNNode>(planNode)) {
        return TopNWithExprOperatorFactory::CreateTopNWithExprOperatorFactory(topNNode, queryConfig);
    } else if (auto topNSortNode = std::dynamic_pointer_cast<const TopNSortNode>(planNode)) {
        return TopNSortWithExprOperatorFactory::CreateTopNSortWithExprOperatorFactory(topNSortNode, queryConfig);
    } else if (auto limitNode = std::dynamic_pointer_cast<const LimitNode>(planNode)) {
        return LimitOperatorFactory::CreateLimitOperatorFactory(limitNode);
    } else if (auto unionNode = std::dynamic_pointer_cast<const UnionNode>(planNode)) {
        return UnionOperatorFactory::CreateUnionOperatorFactory(unionNode);
    } else if (auto valueStreamNode = std::dynamic_pointer_cast<const ValueStreamNode>(planNode)) {
        return ValueStreamFactory::CreateValueStreamFactory(valueStreamNode);
    } else if (auto aggregationNode = std::dynamic_pointer_cast<const AggregationNode>(planNode)) {
        if (aggregationNode->GetGroupByKeys().empty()) {
            return AggregationWithExprOperatorFactory::CreateAggregationWithExprOperatorFactory(aggregationNode,
                                                                                                queryConfig);
        }
        return HashAggregationWithExprOperatorFactory::CreateAggregationWithExprOperatorFactory(aggregationNode,
                                                                                                queryConfig);
    } else if (auto expandNode = std::dynamic_pointer_cast<const ExpandNode>(planNode)) {
        return CreateExpandOperatorFactory(expandNode, queryConfig);
    } else if (auto groupingNode = std::dynamic_pointer_cast<const GroupingNode>(planNode)) {
        return GroupingOperatorFactory::CreateGroupingOperatorFactory(groupingNode, queryConfig);
    } else {
        throw omniruntime::exception::OmniException("PLANNODE_NOT_SUPPORT",
                                                    "The plannode is not supported yet." + planNode->Id());
    }
}

void planDetail(const std::shared_ptr<const PlanNode> &planNode,
                std::vector<std::shared_ptr<omniruntime::op::Operator>> *currentOperators,
                std::vector<std::shared_ptr<OmniDriver>> *drivers, std::vector<OperatorFactory *> *factories,
                const config::QueryConfig &queryConfig)
{
    OperatorFactory *factory = nullptr;
    if (!currentOperators) {
        drivers->emplace_back(std::make_unique<OmniDriver>());
        currentOperators = drivers->back()->operators();
    }

    const auto &sources = planNode->Sources();
    std::vector<std::vector<std::shared_ptr<OmniDriver>>> builderDrivers(sources.size());
    if (sources.empty()) {
        drivers->back()->inputDriver = true;
    } else {
        for (int32_t i = 0; i < sources.size(); ++i) {
            planDetail(sources[i], MustStartNewPipeline(i) ? nullptr : currentOperators,
                       MustStartNewPipeline(i) ? &builderDrivers[i] : drivers, factories, queryConfig);
        }
    }

    // JoinNode and UnionNode has multiple sources, so we need to create a builder driver for each source
    if (auto joinNode = std::dynamic_pointer_cast<const HashJoinNode>(planNode)) {
        auto hashBuilderOperatorFactory =
            HashBuilderWithExprOperatorFactory::CreateHashBuilderWithExprOperatorFactory(joinNode, queryConfig);
        auto builderDriver = builderDrivers[1][0];
        builderDriver->operators()->emplace_back(createOperator(hashBuilderOperatorFactory, joinNode));
        factories->emplace_back(hashBuilderOperatorFactory);

        auto joinType = joinNode->GetJoinType();
        if (joinNode->IsFullJoin() || (joinNode->IsLeftJoin() && joinNode->IsBuildLeft()) ||
            (joinNode->IsRightJoin() && joinNode->IsBuildRight())) {
            factory = LookupJoinWrapperOperatorFactory::CreateLookupJoinWrapperOperatorFactory(
                joinNode, hashBuilderOperatorFactory, queryConfig);
        } else {
            factory = LookupJoinWithExprOperatorFactory::CreateLookupJoinWithExprOperatorFactory(
                joinNode, hashBuilderOperatorFactory, queryConfig);
        }
    } else if (auto sortMergejoinNode = std::dynamic_pointer_cast<const MergeJoinNode>(planNode)) {
        auto streamedTableWithExprOperatorFactoryV2 =
            StreamedTableWithExprOperatorFactoryV2::CreateStreamedTableWithExprOperatorFactoryV2(sortMergejoinNode,
                                                                                                 queryConfig);
        auto bufferedTableWithExprOperatorFactoryV2 =
            BufferedTableWithExprOperatorFactoryV2::CreateBufferedTableWithExprOperatorFactoryV2(
                sortMergejoinNode, reinterpret_cast<int64_t>(streamedTableWithExprOperatorFactoryV2), queryConfig);
        auto builderDriver = builderDrivers[1][0];
        builderDriver->operators()->emplace_back(
            createOperator(bufferedTableWithExprOperatorFactoryV2, sortMergejoinNode));
        factories->emplace_back(bufferedTableWithExprOperatorFactoryV2);
        factory = streamedTableWithExprOperatorFactoryV2;
    } else if (auto nestedLoopJoinNode = std::dynamic_pointer_cast<const NestedLoopJoinNode>(planNode)) {
        auto nestedLoopJoinBuilderOperatorFactory =
            NestedLoopJoinBuildOperatorFactory::CreateNestedLoopJoinBuildOperatorFactory(nestedLoopJoinNode);
        auto nestedLoopJoinLookupWrapperOperatorFactory =
            NestLoopJoinLookupWrapperOperatorFactory::CreateNestLoopJoinLookupWrapperOperatorFactory(
                nestedLoopJoinNode, nestedLoopJoinBuilderOperatorFactory, queryConfig);
        auto builderDriver = builderDrivers[1][0];
        builderDriver->operators()->emplace_back(
            createOperator(nestedLoopJoinBuilderOperatorFactory, nestedLoopJoinNode));
        factories->emplace_back(nestedLoopJoinBuilderOperatorFactory);
        factory = nestedLoopJoinLookupWrapperOperatorFactory;
    } else {
        factory = createOperatorFactory(planNode, queryConfig);
    }

    auto currentOperator = createOperator(factory, planNode);
    currentOperator->setInputOperatorCnt(sources.size());
    currentOperators->emplace_back(currentOperator);
    factories->emplace_back(factory);

    if (auto unionNode = std::dynamic_pointer_cast<const UnionNode>(planNode)) {
        for (auto i = 1; i < sources.size(); i++) {
            auto builderDriver = builderDrivers[i][0];
            builderDriver->operators()->emplace_back(createUnionBuildOperator(currentOperator, planNode));
        }
    }

    for (auto builderDriver : builderDrivers) {
        drivers->insert(drivers->end(), builderDriver.begin(), builderDriver.end());
    }
}

void LocalPlanner::buildOperatorStats(std::vector<std::shared_ptr<OmniDriver>> *drivers)
{
    if (drivers->empty()) {
        LogError("drivers is empty");
        return;
    }

    for (auto index = 0; index < drivers->size(); ++index) {
        const auto operators = (*drivers)[index]->operators();
        if (operators->empty()) {
            LogError("operators is empty");
            return;
        }
        for (auto i = 0; i < operators->size(); ++i) {
            (*operators)[i]->SetOperatorId(i);
            (*operators)[i]->stats_ =
                OperatorStats(i, index, (*operators)[i]->planNodeId(), (*operators)[i]->operatorType());
        }
    }
}

void LocalPlanner::plan(const PlanFragment &fragment, std::vector<std::shared_ptr<OmniDriver>> *drivers,
                        std::vector<OperatorFactory *> *factories, const config::QueryConfig &queryConfig)
{
    planDetail(fragment.planNode, nullptr, drivers, factories, queryConfig);
    (*drivers)[0]->outputDriver = true;

    buildOperatorStats(drivers);
}
} // namespace omniruntime::compute
