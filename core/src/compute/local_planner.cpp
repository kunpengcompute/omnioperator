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
#include <operator/aggregation/sort_aggregation_expr.h>

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
#include "util/telemetry.h"
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
#include "connectors/hive/HiveConnector.h"
#include "util/debug.h"

#include "expression/expressions.h"

#include <cstdio>
#include <cstdlib>

namespace omniruntime::compute {

namespace {

bool ResolveDynamicFilterPushdownEnabled(const config::QueryConfig &queryConfig)
{
    if (queryConfig.dynamicFilterPushdownEnabled()) {
        return true;
    }
    // Gluten getOmniConf forwards Spark keys containing "omni" onto HiveConnector, even when
    // GetQueryContextConf did not copy them into QueryConfig (cpp-omni not rebuilt).
    for (const auto &[id, connector] : omniruntime::connector::getAllConnectors()) {
        auto hive = std::dynamic_pointer_cast<omniruntime::connector::hive::HiveConnector>(connector);
        if (hive == nullptr || hive->connectorConfig() == nullptr) {
            continue;
        }
        const auto &cfg = hive->connectorConfig();
        if (cfg->Get<bool>(config::QueryConfig::kDynamicFilterPushdownEnabledSpark, false) ||
            cfg->Get<bool>(config::QueryConfig::kDynamicFilterPushdownEnabled, false)) {
            LogDebug("DFP: enabled via connector config id=%s (QueryConfig key was missing)", id.c_str());
            return true;
        }
    }
    return false;
}

} // namespace

// Returns ture if source nodes must run in a separate pipeline
bool MustStartNewPipeline(int sourceId) { return sourceId != 0; }

/// Spill key gate: INT / LONG / DATE32 / VARCHAR keys — accepts BOTH plain field keys (FIELD_E)
/// and expression keys (cast / arithmetic / substr / ...). The WithExpr build & probe operators
/// project every key expression into a materialized column BEFORE the sub-partitioner runs
/// (OperatorUtil::CreateProjections + ProjectVectors), so what the spiller actually hashes is a
/// real column and the only thing that matters here is the key's *return type* — not whether it
/// is a bare field. Return type is gated against the set HashKeyColumn can hash
/// (BOOLEAN/INT/LONG/DATE32/VARCHAR). DATE32 shares the int32 HashPrimitiveColumn path with OMNI_INT.
/// No OMNI_CHAR: Spark normalizes char(N)/varchar(N) to StringType (CharVarcharUtils), so a char
/// key reaches native as OMNI_VARCHAR. Target restore keeps one resident table slot;
/// JoinSpillState serializes the projected build schema for expression keys.
bool AreJoinSpillKeysSupported(const std::shared_ptr<const HashJoinNode> &joinNode)
{
    auto keysOk = [](const std::vector<omniruntime::expressions::Expr *> &keys) {
        if (keys.empty()) {
            return false;
        }
        for (auto *key : keys) {
            if (key == nullptr) {
                return false;
            }
            // Field or expression key alike — gate on the projected return type only.
            const auto id = key->GetReturnTypeId();
            if (id != omniruntime::type::OMNI_INT && id != omniruntime::type::OMNI_LONG &&
                id != omniruntime::type::OMNI_DATE32 && id != omniruntime::type::OMNI_VARCHAR &&
                id != omniruntime::type::OMNI_BOOLEAN) {
                return false;
            }
        }
        return true;
    };
    return keysOk(joinNode->LeftKeys()) && keysOk(joinNode->RightKeys());
}

/// V1 base: INNER and LEFT_SEMI. V2-A Step 1: LEFT/RIGHT Opposite
/// (Gluten forces LEFT->buildRight, RIGHT->buildLeft).
/// V2-A Step 2 (S2-1): FULL + SameSide LEFT/RIGHT (build = outer side) — SameSide/Full + visited.
bool IsJoinSpillSupported(const std::shared_ptr<const HashJoinNode> &joinNode)
{
    if (joinNode->IsNullAware()) {
        return false;
    }
    const bool probeOnlyBuildRight =
        (joinNode->IsLeftSemi() || joinNode->IsLeftAnti() || joinNode->IsExistence()) && joinNode->IsBuildRight();
    const bool joinTypeSupported = joinNode->IsInnerJoin() || joinNode->IsFullJoin() ||
        (joinNode->IsLeftJoin() && joinNode->IsBuildRight()) ||
        (joinNode->IsRightJoin() && joinNode->IsBuildLeft()) ||
        (joinNode->IsLeftJoin() && joinNode->IsBuildLeft()) ||
        (joinNode->IsRightJoin() && joinNode->IsBuildRight()) || probeOnlyBuildRight;
    if (!joinTypeSupported) {
        return false;
    }
    return AreJoinSpillKeysSupported(joinNode);
}

namespace {
bool UseJoinSpillSubPartitions(const std::shared_ptr<const HashJoinNode> &joinNode,
    const config::QueryConfig &queryConfig, const op::JoinSubPartitionConfig &subPartCfg)
{
    // Broadcast joins do not create a JoinSpillState or partition their build input.
    // Opening N table slots for them makes the probe route to unbuilt slots 1..N-1.
    return !joinNode->IsBHJ() && joinNode->IsShuffle() && queryConfig.joinSpillEffective() && subPartCfg.IsEnabled() &&
        IsJoinSpillSupported(joinNode);
}
} // namespace

// Pool is threaded through from OmniTask::Next → LocalPlanner::plan(..., pool)
// → planDetail(..., pool) → createOperator(..., pool).
// The production JNI path (gluten WholeStageResultIterator → OmniTask::Next)
// supplies the per-task pool decoded from nmmHandle; C++ unit-test scaffolds
// that do not have a JNI handle pass nullptr and hit the no-pool path inside
// individual operator factories.
//
// NOTE: OmniTask::Next() is the production JNI plan-exec path — gluten's
// WholeStageResultIterator::Next() drives it. Passing nullptr unconditionally
// causes a SIGSEGV when codegen .so's CreateFlatVector(pool, ...) dereferences it.
std::shared_ptr<omniruntime::op::Operator> createOperator(
    OperatorFactory* factory, const std::shared_ptr<const PlanNode>& planNode,
    omniruntime::mem::OmniTaskMemoryPool* pool)
{
    std::shared_ptr<omniruntime::op::Operator> operatorPtr(factory->CreateOperator(pool));
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
        if (aggregationNode->IsSorted()) {
            return SortAggregationWithExprOperatorFactory::CreateSortAggregationWithExprOperatorFactory(
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
        // Target spill restores one sub-partition at a time into slot 0.
        const int32_t hashTableCount = 1;
        auto hashBuilderWithExprOperatorFactory =
            HashBuilderWithExprOperatorFactory::CreateHashBuilderWithExprOperatorFactory(
                joinNode, queryConfig, hashTableCount);
        auto hashBuilderOperatorFactory = hashBuilderWithExprOperatorFactory->GetHashBuilderOperatorFactory();
        return std::make_pair(hashBuilderWithExprOperatorFactory, hashBuilderOperatorFactory);
    } else {
        const int32_t hashTableCount = 1;

        /// Keep one resident table slot for the target's restore protocol.
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
    std::shared_ptr<SplitsStore> splitsStore,
    omniruntime::mem::OmniTaskMemoryPool* pool)
{
    OperatorFactory* factory = nullptr;
    if (!currentOperators) {
        drivers->emplace_back(std::make_unique<OmniDriver>(ResolveDynamicFilterPushdownEnabled(queryConfig)));
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
                newPipeline ? &builderDrivers[i] : drivers, newPipeline ? nullptr : factories, queryConfig, splitsStore,
                pool);
        }
    }

    // JoinNode and UnionNode has multiple sources, so we need to create a builder driver for each source
    if (auto joinNode = std::dynamic_pointer_cast<const HashJoinNode>(planNode)) {
        const bool isBHJ = joinNode->IsBHJ();
        const bool buildOnce = queryConfig.buildHashTableOncePerExecutor();
        const std::string& htId = joinNode->BuildHashTableId();

        // Determine factory pair: either fresh build or cache-reuse for BHJ.
        std::pair<OperatorFactory*, op::HashBuilderOperatorFactory*> res;
        std::shared_ptr<op::BroadcastCacheLeaseSlot> cacheLeaseSlot;

        // Outer/full joins mutate per-row visited flags in the shared table.
        // Dynamic filter generation also mutates cached table metadata. Keep
        // the first cache rollout on read-only inner probes.
        if (isBHJ && joinNode->IsInnerJoin() &&
            !ResolveDynamicFilterPushdownEnabled(queryConfig) && buildOnce && !htId.empty()) {
            auto& cache = op::BroadcastHashTableCache::getInstance();
            auto acquisition = cache.acquire(htId);
            if (acquisition.builder != nullptr) {
                cacheLeaseSlot = std::make_shared<op::BroadcastCacheLeaseSlot>();
                res = createHashBuilderOperatorPairFactory(joinNode, queryConfig);
                res.second->SetBroadcastHashTableId(htId);
                res.second->SetCacheBuildTicket(std::move(acquisition.builder));
                res.second->SetCacheLeaseSlot(cacheLeaseSlot);
            } else if (acquisition.reader) {
                cacheLeaseSlot = std::make_shared<op::BroadcastCacheLeaseSlot>();
                cacheLeaseSlot->lease = std::move(acquisition.reader);
                res = createHashBuilderFromCachedVariants(
                    joinNode, queryConfig, cacheLeaseSlot->lease.get(), htId);
            } else {
                // Shutdown has stopped cache admission. Build for this task only.
                res = createHashBuilderOperatorPairFactory(joinNode, queryConfig);
            }
        } else {
            res = createHashBuilderOperatorPairFactory(joinNode, queryConfig);
        }

        auto subPartCfg = op::JoinSubPartitionConfig::FromQueryConfig(queryConfig);
        const bool joinSpillV1Enabled = UseJoinSpillSubPartitions(joinNode, queryConfig, subPartCfg);
        std::shared_ptr<op::JoinSpillState> joinSpillState = nullptr;
        if (joinSpillV1Enabled) {
            // Spill/replay must use the schema HashBuilder actually writes: GetBuildTypes() is
            // RightOutputType for field keys, or the projected schema (outputs + expr key columns)
            // for expression keys. RightOutputType alone would omit expr keys and break restore.
            joinSpillState = std::make_shared<op::JoinSpillState>(queryConfig.SpillDir(),
                res.second->GetBuildTypes(), *joinNode->LeftOutputType(), subPartCfg.numSubPartitions);
            auto spillConfig = std::make_shared<op::SparkSpillConfig>(true, queryConfig.SpillDir(),
                queryConfig.maxSpillBytes(), INT32_MAX, queryConfig.memFraction(),
                queryConfig.SpillWriteBufferSize(), queryConfig.SpillEnableCompress());
            res.second->SetSpillConfig(spillConfig);
        }
        // maxSpillRunRows is unused; pass 0. Pool-driven reclaim and the debug-only EOB force
        // are independent knobs.
        res.second->SetJoinSpillSubPartitionPolicy(joinSpillV1Enabled, /*maxSpillRunRows=*/0, subPartCfg,
            queryConfig.passiveReclaimEffective(), queryConfig.forceBuildGroupSpillEffective(),
            queryConfig.spillableReservationGrowthPct());
        res.second->SetJoinSpillState(joinSpillState);
        res.second->SetDynamicFilterPushdownEnabled(ResolveDynamicFilterPushdownEnabled(queryConfig));
        if (isBHJ && !res.second->IsPrebuilt() && !HasExprBuildKeys(joinNode)) {
            res.second->SetBroadcastParallelBuildPolicy(
                op::BroadcastParallelBuildPolicy::FromQueryConfig(queryConfig));
        }

        // The target join spill protocol always hands off one resident slot. The pool
        // layer only adds passive reclaim and reservation admission around that path.
        if (joinSpillV1Enabled) {
            res.second->SetJoinSpillHandoffSessionEnabled(true);
            if (joinSpillState != nullptr) {
                joinSpillState->SetInitialRuntimeLayout(op::JoinBuildRuntimeLayout::Single);
            }
        }
        if (omniruntime::telemetry::IsReclaimVerboseEnabled()) {
            std::fprintf(stderr,
                "[OmniRuntime][JoinSpill][passive-reclaim] event=plan "
                "planNodeId=%s spillEnabled=%d joinSpill=%d handoff=%d "
                "passiveEnabled=%d passiveEffective=%d forceEob=%d prebuilt=%d isBHJ=%d "
                "isShuffle=%d subPartitions=%u keysSupported=%d "
                "v1=%d handoffSession=%d\n",
                joinNode->Id().c_str(),
                queryConfig.spillEnabled() ? 1 : 0,
                queryConfig.joinSpillEnabled() ? 1 : 0,
                queryConfig.joinSpillHandoffEnabled() ? 1 : 0,
                queryConfig.passiveReclaimEnabled() ? 1 : 0,
                queryConfig.passiveReclaimEffective() ? 1 : 0,
                queryConfig.forceBuildGroupSpillEffective() ? 1 : 0,
                res.second->IsPrebuilt() ? 1 : 0,
                isBHJ ? 1 : 0,
                joinNode->IsShuffle() ? 1 : 0,
                subPartCfg.numSubPartitions,
                AreJoinSpillKeysSupported(joinNode) ? 1 : 0,
                joinSpillV1Enabled ? 1 : 0,
                joinSpillV1Enabled ? 1 : 0);
            std::fflush(stderr);
        }
        auto builderDriver = builderDrivers[1][0];
        if (cacheLeaseSlot != nullptr) {
            builderDriver->HoldLifetime(cacheLeaseSlot);
        }
        builderDriver->operators()->emplace_back(createOperator(res.first, joinNode, pool));
        // With handoff, keep the HashBuilder factory on the build driver: operators hold a raw
        // JoinSpillState* while the factory owns the shared_ptr; destroying the factory with the
        // probe driver would free the state while build is still blocked on restore.
        // Without spill/handoff, register on `factories` as other joins do (unchanged lifetime).
        if (joinSpillV1Enabled) {
            builderDriver->operatorFactories()->emplace_back(res.first);
        } else {
            factories->emplace_back(res.first);
        }

        if (joinNode->IsFullJoin() || (joinNode->IsLeftJoin() && joinNode->IsBuildLeft()) || (joinNode->IsRightJoin() && joinNode->IsBuildRight())) {
            factory =
                LookupJoinWrapperOperatorFactory::CreateLookupJoinWrapperOperatorFactory(joinNode, res.second, queryConfig);
        } else {
            factory =
                LookupJoinWithExprOperatorFactory::CreateLookupJoinWithExprOperatorFactory(joinNode, res.second, queryConfig);
        }
        if (cacheLeaseSlot != nullptr) {
            factory->HoldLifetime(cacheLeaseSlot);
        }
    } else if (auto sortMergejoinNode = std::dynamic_pointer_cast<const MergeJoinNode>(planNode)) {
        auto streamedTableWithExprOperatorFactoryV2 =
            StreamedTableWithExprOperatorFactoryV2::CreateStreamedTableWithExprOperatorFactoryV2(
                sortMergejoinNode, queryConfig);
        auto bufferedTableWithExprOperatorFactoryV2 =
            BufferedTableWithExprOperatorFactoryV2::CreateBufferedTableWithExprOperatorFactoryV2(
                sortMergejoinNode, reinterpret_cast<int64_t>(streamedTableWithExprOperatorFactoryV2), queryConfig);
        auto builderDriver = builderDrivers[1][0];
        builderDriver->operators()->emplace_back(createOperator(bufferedTableWithExprOperatorFactoryV2, sortMergejoinNode, pool));
        factories->emplace_back(bufferedTableWithExprOperatorFactoryV2);
        factory = streamedTableWithExprOperatorFactoryV2;
    } else if (auto nestedLoopJoinNode = std::dynamic_pointer_cast<const NestedLoopJoinNode>(planNode)) {
        auto nestedLoopJoinBuilderOperatorFactory =
            NestedLoopJoinBuildOperatorFactory::CreateNestedLoopJoinBuildOperatorFactory(nestedLoopJoinNode);
        auto nestedLoopJoinLookupWrapperOperatorFactory =
            NestLoopJoinLookupWrapperOperatorFactory::CreateNestLoopJoinLookupWrapperOperatorFactory(nestedLoopJoinNode, nestedLoopJoinBuilderOperatorFactory, queryConfig);
        auto builderDriver = builderDrivers[1][0];
        builderDriver->operators()->emplace_back(createOperator(nestedLoopJoinBuilderOperatorFactory, nestedLoopJoinNode, pool));
        factories->emplace_back(nestedLoopJoinBuilderOperatorFactory);
        factory = nestedLoopJoinLookupWrapperOperatorFactory;
    } else {
        factory = createOperatorFactory(planNode, queryConfig, splitsStore);
    }

    auto currentOperator = createOperator(factory, planNode, pool);
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
    std::shared_ptr<SplitsStore> splitsStore,
    omniruntime::mem::OmniTaskMemoryPool* pool)
{
    planDetail(fragment.planNode,
        nullptr,
        drivers,
        nullptr,
        queryConfig,
        splitsStore,
        pool);
    (*drivers)[0]->outputDriver = true;

    buildOperatorStats(drivers);
}
} // namespace omniruntime::compute
