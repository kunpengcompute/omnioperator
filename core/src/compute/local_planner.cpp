/*
* You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "local_planner.h"

#include <operator/aggregation/group_aggregation_expr.h>

#include "operator/join/broadcast_hash_table_cache.h"
#include "operator/join/hash_builder_expr.h"
#include "operator/join/join_spill_state.h"
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
#include "operator/window/window_group_limit_expr.h"
#include "operator/join/sortmergejoin/sort_merge_join_expr_v2.h"
#include <operator/aggregation/non_group_aggregation_expr.h>
#include "operator/expand/expand.h"
#include "operator/grouping/grouping.h"
#include "operator/unnest/unnest.h"
#include "operator/tablescan/TableScan.h"
#include "compute/task.h"
#include "operator/join/join_sub_partitioner.h"

namespace omniruntime::compute {

// Returns ture if source nodes must run in a separate pipeline
bool MustStartNewPipeline(int sourceId) { return sourceId != 0; }

/// todo: only support Long and Varchar for first version of Join Spill
bool IsLongOrVarcharTypes(const omniruntime::type::DataTypes &types)
{
    auto ids = types.GetIds();
    for (int32_t i = 0; i < types.GetSize(); ++i) {
        if (ids[i] != omniruntime::type::OMNI_LONG && ids[i] != omniruntime::type::OMNI_VARCHAR) {
            return false;
        }
    }
    return true;
}

/// todo: only support inner join for first version
bool IsJoinSpillV1Supported(const std::shared_ptr<const HashJoinNode> &joinNode)
{
    if (!joinNode->IsInnerJoin() || !IsLongOrVarcharTypes(*joinNode->LeftOutputType()) ||
        !IsLongOrVarcharTypes(*joinNode->RightOutputType())) {
        return false;
    }

    /// todo: only support field key, not support expression key for first version
    for (auto *key : joinNode->LeftKeys()) {
        if (key->GetType() != omniruntime::expressions::ExprType::FIELD_E) {
            return false;
        }
    }
    for (auto *key : joinNode->RightKeys()) {
        if (key->GetType() != omniruntime::expressions::ExprType::FIELD_E) {
            return false;
        }
    }
    return true;
}

std::shared_ptr<omniruntime::op::Operator> createOperator(
    OperatorFactory* factory, const std::shared_ptr<const PlanNode>& planNode)
{
    std::shared_ptr<omniruntime::op::Operator> operatorPtr(factory->CreateOperator());
    operatorPtr->setNoMoreInput(false);
    operatorPtr->SetPlanNodeId(planNode->Id());
    if (operatorPtr->operatorType().empty()) {
        operatorPtr->SetOperatorType(string(planNode->Name()));
    }
    return std::move(operatorPtr);
}

std::shared_ptr<omniruntime::op::Operator> createUnionBuildOperator(
    std::shared_ptr<omniruntime::op::Operator> unionOperator,
    const std::shared_ptr<const PlanNode>& planNode)
{
    std::shared_ptr<omniruntime::op::Operator> unionBuildOperator = std::make_shared<UnionBuildOperator>(unionOperator);
    unionBuildOperator->setNoMoreInput(false);
    unionBuildOperator->SetPlanNodeId(planNode->Id());
    if (unionBuildOperator->operatorType().empty()) {
        unionBuildOperator->SetOperatorType(string(planNode->Name()));
    }
    return std::move(unionBuildOperator);
}

OperatorFactory* createOperatorFactory(
    const std::shared_ptr<const PlanNode>& planNode,
    const config::QueryConfig& queryConfig,
    std::shared_ptr<SplitsStore> splitsStore)
{
    if (auto orderByNode = std::dynamic_pointer_cast<const OrderByNode>(planNode)) {
        return SortWithExprOperatorFactory::CreateSortWithExprOperatorFactory(orderByNode, queryConfig);
    } else if (auto projectNode = std::dynamic_pointer_cast<const ProjectNode>(planNode)) {
        return CreateProjectOperatorFactory(projectNode, queryConfig);
    } else if (auto filterNode = std::dynamic_pointer_cast<const FilterNode>(planNode)) {
        return CreateFilterOperatorFactory(filterNode, queryConfig);
    } else if (auto windowNode = std::dynamic_pointer_cast<const WindowNode>(planNode)) {
        return WindowWithExprOperatorFactory::CreateWindowWithExprOperatorFactory(windowNode, queryConfig);
    } else if (auto windowGroupLimitNode = std::dynamic_pointer_cast<const WindowGroupLimitNode>(planNode)) {
        return WindowGroupLimitWithExprOperatorFactory::CreateWindowGroupLimitWithExprOperatorFactory(windowGroupLimitNode, queryConfig);
    } else if (auto topNNode = std::dynamic_pointer_cast<const TopNNode>(planNode)) {
        return TopNWithExprOperatorFactory::CreateTopNWithExprOperatorFactory(topNNode, queryConfig);
    } else if (auto topNSortNode = std::dynamic_pointer_cast<const TopNSortNode>(planNode)) {
        return TopNSortWithExprOperatorFactory::CreateTopNSortWithExprOperatorFactory(
            topNSortNode, queryConfig);
    } else if (auto limitNode = std::dynamic_pointer_cast<const LimitNode>(planNode)) {
        return LimitOperatorFactory::CreateLimitOperatorFactory(limitNode);
    } else if (auto unionNode = std::dynamic_pointer_cast<const UnionNode>(planNode)) {
        return UnionOperatorFactory::CreateUnionOperatorFactory(unionNode);
    } else if (auto valueStreamNode = std::dynamic_pointer_cast<const ValueStreamNode>(planNode)) {
        return ValueStreamFactory::CreateValueStreamFactory(valueStreamNode);
    } else if (auto aggregationNode = std::dynamic_pointer_cast<const AggregationNode>(planNode)) {
        if (aggregationNode->GetGroupByKeys().empty()) {
            return AggregationWithExprOperatorFactory::CreateAggregationWithExprOperatorFactory(
                aggregationNode, queryConfig);
        }
        return HashAggregationWithExprOperatorFactory::CreateAggregationWithExprOperatorFactory(
            aggregationNode, queryConfig);
    } else if (auto expandNode = std::dynamic_pointer_cast<const ExpandNode>(planNode)) {
        return CreateExpandOperatorFactory(expandNode, queryConfig);
    } else if (auto groupingNode = std::dynamic_pointer_cast<const GroupingNode>(planNode)) {
        return GroupingOperatorFactory::CreateGroupingOperatorFactory(groupingNode, queryConfig);
    } else if (auto unnestNode = std::dynamic_pointer_cast<const UnnestNode>(planNode)) {
        return UnnestOperatorFactory::CreateUnnestOperatorFactory(unnestNode);
    } else if (auto tableScanNode = std::dynamic_pointer_cast<const TableScanNode>(planNode)) {
        return TableScanOperatorFactory::CreateTableScanOperatorFactory(tableScanNode, queryConfig, splitsStore);
    } else {
        throw omniruntime::exception::OmniException(
            "PLANNODE_NOT_SUPPORT", "The plannode is not supported yet." + planNode->Id());
    }
}

bool HasExprBuildKeys(const std::shared_ptr<const HashJoinNode>& joinNode)
{
    for (auto* expr : joinNode->RightKeys()) {
        if (expr->GetType() != omniruntime::expressions::ExprType::FIELD_E) {
            return true;
        }
    }
    return false;
}

std::pair<OperatorFactory*, HashBuilderOperatorFactory*> createHashBuilderFromCachedVariants(
    const std::shared_ptr<const HashJoinNode>& joinNode,
    const config::QueryConfig& queryConfig,
    op::HashTableVariants* cachedVariants,
    const std::string& htId)
{
    if (HasExprBuildKeys(joinNode)) {
        auto* exprFactory =
            HashBuilderWithExprOperatorFactory::CreateHashBuilderWithExprOperatorFactory(joinNode, queryConfig);
        auto* innerFactory = exprFactory->GetHashBuilderOperatorFactory();
        innerFactory->InjectCachedVariants(cachedVariants, htId);
        return std::make_pair(exprFactory, innerFactory);
    }
    auto* cachedFactory = op::HashBuilderOperatorFactory::CreateFromCachedVariants(joinNode, cachedVariants);
    cachedFactory->SetBroadcastHashTableId(htId);
    return std::make_pair(cachedFactory, cachedFactory);
}

std::pair<OperatorFactory*, HashBuilderOperatorFactory*> createHashBuilderOperatorPairFactory(
    const std::shared_ptr<const HashJoinNode>& joinNode,
    const config::QueryConfig& queryConfig)
{
    if (HasExprBuildKeys(joinNode)) {
        auto hashBuilderWithExprOperatorFactory =
            HashBuilderWithExprOperatorFactory::CreateHashBuilderWithExprOperatorFactory(joinNode, queryConfig);
        auto hashBuilderOperatorFactory = hashBuilderWithExprOperatorFactory->GetHashBuilderOperatorFactory();
        return std::make_pair(hashBuilderWithExprOperatorFactory, hashBuilderOperatorFactory);
    } else {
        const auto subPartCfg = op::JoinSubPartitionConfig::FromQueryConfig(queryConfig);
        const int32_t hashTableCount = queryConfig.joinSpillEnabled() && queryConfig.maxSpillRunRows() != 0
                && subPartCfg.IsEnabled() && IsJoinSpillV1Supported(joinNode)
            ? static_cast<int32_t>(subPartCfg.numSubPartitions)
            : 1;

        /// hashTableCount is equal to join sub partitions count.
        auto hashBuilderOperatorFactory =
            HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(joinNode, hashTableCount);
        return std::make_pair(hashBuilderOperatorFactory, hashBuilderOperatorFactory);
    }
}

void planDetail(
    const std::shared_ptr<const PlanNode>& planNode,
    std::vector<std::shared_ptr<omniruntime::op::Operator>>* currentOperators,
    std::vector<std::shared_ptr<OmniDriver>>* drivers,
    std::vector<OperatorFactory*>* factories,
    const config::QueryConfig& queryConfig,
    std::shared_ptr<SplitsStore> splitsStore)
{
    OperatorFactory* factory = nullptr;
    if (!currentOperators) {
        drivers->emplace_back(std::make_unique<OmniDriver>());
        currentOperators = drivers->back()->operators();
        factories = drivers->back()->operatorFactories();
    }

    const auto &sources = planNode->Sources();
    std::vector<std::vector<std::shared_ptr<OmniDriver>>> builderDrivers(sources.size());
    if (sources.empty()) {
        drivers->back()->inputDriver = true;
    } else {
        for (int32_t i = 0; i < sources.size(); ++i) {
            bool newPipeline = MustStartNewPipeline(i);
            planDetail(sources[i], newPipeline ? nullptr : currentOperators,
                newPipeline ? &builderDrivers[i] : drivers, newPipeline ? nullptr : factories, queryConfig, splitsStore);
        }
    }

    // JoinNode and UnionNode has multiple sources, so we need to create a builder driver for each source
    if (auto joinNode = std::dynamic_pointer_cast<const HashJoinNode>(planNode)) {
        const bool isBHJ = joinNode->IsBHJ();
        const bool buildOnce = queryConfig.buildHashTableOncePerExecutor();
        const std::string& htId = joinNode->BuildHashTableId();

        // Determine factory pair: either fresh build or cache-reuse for BHJ.
        std::pair<OperatorFactory*, op::HashBuilderOperatorFactory*> res;

        if (isBHJ && buildOnce && !htId.empty()) {
            auto& cache = op::BroadcastHashTableCache::getInstance();
            if (cache.tryClaimBuild(htId)) {
                // This task wins the build race. Build normally, then publish to cache.
                res = createHashBuilderOperatorPairFactory(joinNode, queryConfig);
                // Mark the factory so operators know to publish after build.
                res.second->SetBroadcastHashTableId(htId);
            } else {
                // Another task is building (or already done). Wait for the cached table.
                op::HashTableVariants* cachedVariants = cache.waitAndGet(htId);
                if (cachedVariants != nullptr) {
                    // Inject the pre-built hash table. No spill for BHJ.
                    res = createHashBuilderFromCachedVariants(joinNode, queryConfig, cachedVariants, htId);
                } else {
                    // Fallback: cache entry was invalidated during wait; build from scratch.
                    res = createHashBuilderOperatorPairFactory(joinNode, queryConfig);
                    res.second->SetBroadcastHashTableId(htId);
                    // Re-try claiming (will succeed if nobody else is trying now).
                    cache.tryClaimBuild(htId);
                }
            }
        } else {
            res = createHashBuilderOperatorPairFactory(joinNode, queryConfig);
        }

        auto subPartCfg = op::JoinSubPartitionConfig::FromQueryConfig(queryConfig);
        const bool joinSpillV1Enabled = !isBHJ && queryConfig.joinSpillEnabled()
            && queryConfig.maxSpillRunRows() != 0
            && subPartCfg.IsEnabled() && IsJoinSpillV1Supported(joinNode);
        std::shared_ptr<op::JoinSpillState> joinSpillState = nullptr;
        if (joinSpillV1Enabled) {
            joinSpillState = std::make_shared<op::JoinSpillState>(queryConfig.SpillDir(),
                *joinNode->RightOutputType(), *joinNode->LeftOutputType(), subPartCfg.numSubPartitions);
        }
        res.second->SetJoinSpillSubPartitionPolicy(joinSpillV1Enabled, queryConfig.maxSpillRunRows(), subPartCfg);
        res.second->SetJoinSpillState(joinSpillState);
        if (isBHJ && !res.second->IsPrebuilt() && !HasExprBuildKeys(joinNode)) {
            res.second->SetBroadcastParallelBuildPolicy(
                op::BroadcastParallelBuildPolicy::FromQueryConfig(queryConfig));
        }
        auto builderDriver = builderDrivers[1][0];
        builderDriver->operators()->emplace_back(createOperator(res.first, joinNode));
        factories->emplace_back(res.first);

        if (joinNode->IsFullJoin() || (joinNode->IsLeftJoin() && joinNode->IsBuildLeft()) || (joinNode->IsRightJoin() && joinNode->IsBuildRight())) {
            factory =
                LookupJoinWrapperOperatorFactory::CreateLookupJoinWrapperOperatorFactory(joinNode, res.second, queryConfig);
        } else {
            factory =
                LookupJoinWithExprOperatorFactory::CreateLookupJoinWithExprOperatorFactory(joinNode, res.second, queryConfig);
        }
    } else if (auto sortMergejoinNode = std::dynamic_pointer_cast<const MergeJoinNode>(planNode)) {
        auto streamedTableWithExprOperatorFactoryV2 =
            StreamedTableWithExprOperatorFactoryV2::CreateStreamedTableWithExprOperatorFactoryV2(
                sortMergejoinNode, queryConfig);
        auto bufferedTableWithExprOperatorFactoryV2 =
            BufferedTableWithExprOperatorFactoryV2::CreateBufferedTableWithExprOperatorFactoryV2(
                sortMergejoinNode, reinterpret_cast<int64_t>(streamedTableWithExprOperatorFactoryV2), queryConfig);
        auto builderDriver = builderDrivers[1][0];
        builderDriver->operators()->emplace_back(createOperator(bufferedTableWithExprOperatorFactoryV2, sortMergejoinNode));
        factories->emplace_back(bufferedTableWithExprOperatorFactoryV2);
        factory = streamedTableWithExprOperatorFactoryV2;
    } else if (auto nestedLoopJoinNode = std::dynamic_pointer_cast<const NestedLoopJoinNode>(planNode)) {
        auto nestedLoopJoinBuilderOperatorFactory =
            NestedLoopJoinBuildOperatorFactory::CreateNestedLoopJoinBuildOperatorFactory(nestedLoopJoinNode);
        auto nestedLoopJoinLookupWrapperOperatorFactory =
            NestLoopJoinLookupWrapperOperatorFactory::CreateNestLoopJoinLookupWrapperOperatorFactory(nestedLoopJoinNode, nestedLoopJoinBuilderOperatorFactory, queryConfig);
        auto builderDriver = builderDrivers[1][0];
        builderDriver->operators()->emplace_back(createOperator(nestedLoopJoinBuilderOperatorFactory, nestedLoopJoinNode));
        factories->emplace_back(nestedLoopJoinBuilderOperatorFactory);
        factory = nestedLoopJoinLookupWrapperOperatorFactory;
    } else {
        factory = createOperatorFactory(planNode, queryConfig, splitsStore);
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

void LocalPlanner::buildOperatorStats(std::vector<std::shared_ptr<OmniDriver>>* drivers)
{
    if (drivers->empty()) {
        LogError("drivers is empty");
        return;
    }

    for (auto index = 0; index <drivers->size(); ++index) {
        const auto operators = (*drivers)[index]->operators();
        if (operators->empty()) {
            LogError("operators is empty");
            return;
        }
        for (auto i = 0; i < operators->size(); ++i) {
            (*operators)[i]->SetOperatorId(i);
            (*operators)[i]->stats_ = OperatorStats(
                i,
                index,
                (*operators)[i]->planNodeId(),
                (*operators)[i]->operatorType());
        }
    }
}

void LocalPlanner::plan(
    const PlanFragment& fragment,
    std::vector<std::shared_ptr<OmniDriver>>* drivers,
    const config::QueryConfig& queryConfig,
    std::shared_ptr<SplitsStore> splitsStore)
{
    planDetail(fragment.planNode,
        nullptr,
        drivers,
        nullptr,
        queryConfig,
        splitsStore);
    (*drivers)[0]->outputDriver = true;

    buildOperatorStats(drivers);
}
} // namespace omniruntime::compute
