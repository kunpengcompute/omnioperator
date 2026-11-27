/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2021-2023. All rights reserved.
 * @Description: sort implementations
 */

#include "hash_builder_expr.h"
#include <memory>
#include <utility>
#include "operator/util/operator_util.h"
#include "vector/vector_helper.h"

namespace omniruntime {
namespace op {
using namespace omniruntime::vec;

HashBuilderWithExprOperatorFactory *HashBuilderWithExprOperatorFactory::CreateHashBuilderWithExprOperatorFactory(
    JoinType joinType, const type::DataTypes &buildTypes,
    const std::vector<omniruntime::expressions::Expr *> &buildHashKeys, int32_t hashTableCount,
    OverflowConfig *overflowConfig)
{
    return new HashBuilderWithExprOperatorFactory(joinType, buildTypes, buildHashKeys, hashTableCount, overflowConfig);
}

HashBuilderWithExprOperatorFactory *HashBuilderWithExprOperatorFactory::CreateHashBuilderWithExprOperatorFactory(
    JoinType joinType, BuildSide buildSide, const type::DataTypes &buildTypes,
    const std::vector<omniruntime::expressions::Expr *> &buildHashKeys, int32_t hashTableCount,
    OverflowConfig *overflowConfig)
{
    return new HashBuilderWithExprOperatorFactory(joinType, buildSide, buildTypes,
                                                  buildHashKeys, hashTableCount, overflowConfig);
}

HashBuilderWithExprOperatorFactory *HashBuilderWithExprOperatorFactory::CreateHashBuilderWithExprOperatorFactory(
    std::shared_ptr<const HashJoinNode> planNode, const config::QueryConfig &queryConfig, int32_t hashTableCount)
{
    auto joinType = planNode->GetJoinType();
    auto buildSide = planNode->GetBuildSide();
    auto buildTypes = planNode->RightOutputType();
    auto buildKeys = planNode->RightKeys();

    auto overflowConfig = queryConfig.IsOverFlowASNull()
                          ? new OverflowConfig(OVERFLOW_CONFIG_NULL)
                          : new OverflowConfig(OVERFLOW_CONFIG_EXCEPTION);
    // hashTableCount comes from the planner (same spill gate as the field-key path).
    auto hashBuilderWithExprOperatorFactory = new HashBuilderWithExprOperatorFactory(joinType, buildSide, *buildTypes,
        buildKeys, hashTableCount, overflowConfig, queryConfig);

    delete overflowConfig;
    overflowConfig = nullptr;
    return hashBuilderWithExprOperatorFactory;
}

HashBuilderWithExprOperatorFactory::HashBuilderWithExprOperatorFactory(JoinType joinType,
    const type::DataTypes &buildTypes, const std::vector<omniruntime::expressions::Expr *> &buildHashKeys,
    int32_t hashTableCount, OverflowConfig *overflowConfig)
{
    std::vector<DataTypePtr> newBuildTypes;
    OperatorUtil::CreateProjections(buildTypes, buildHashKeys, newBuildTypes, this->projections, this->buildHashCols,
        overflowConfig);
    this->buildTypes = std::make_unique<DataTypes>(newBuildTypes);
    this->operatorFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(joinType, *(this->buildTypes),
        this->buildHashCols.data(), buildHashKeys.size(), hashTableCount);
}

HashBuilderWithExprOperatorFactory::HashBuilderWithExprOperatorFactory(JoinType joinType, BuildSide buildSide,
    const type::DataTypes &buildTypes, const std::vector<omniruntime::expressions::Expr *> &buildHashKeys,
    int32_t hashTableCount, OverflowConfig *overflowConfig)
{
    std::vector<DataTypePtr> newBuildTypes;
    OperatorUtil::CreateProjections(buildTypes, buildHashKeys, newBuildTypes, this->projections, this->buildHashCols,
        overflowConfig);
    this->buildTypes = std::make_unique<DataTypes>(newBuildTypes);
    this->operatorFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(joinType, buildSide,
        *(this->buildTypes), this->buildHashCols.data(), buildHashKeys.size(), hashTableCount);
}

HashBuilderWithExprOperatorFactory::HashBuilderWithExprOperatorFactory(JoinType joinType, BuildSide buildSide,
    const type::DataTypes &buildTypes, const std::vector<omniruntime::expressions::Expr *> &buildHashKeys,
    int32_t hashTableCount, OverflowConfig *overflowConfig, const config::QueryConfig &queryConfig)
    : HashBuilderWithExprOperatorFactory(joinType, buildSide, buildTypes, buildHashKeys, hashTableCount, overflowConfig)
{
    this->queryConfig_ = queryConfig;
    if (this->operatorFactory != nullptr) {
        this->operatorFactory->SetDynamicFilterPushdownEnabled(queryConfig.dynamicFilterPushdownEnabled());
    }
}

HashBuilderWithExprOperatorFactory::~HashBuilderWithExprOperatorFactory()
{
    if (operatorFactory != nullptr && !operatorFactory->IsCachePinned()) {
        delete operatorFactory;
    }
}

Operator *HashBuilderWithExprOperatorFactory::CreateOperator(omniruntime::mem::OmniTaskMemoryPool *pool)
{
    auto hashBuilderOperator = static_cast<HashBuilderOperator *>(operatorFactory->CreateOperator(pool));
    return new HashBuilderWithExprOperator(pool, *buildTypes, projections, hashBuilderOperator, queryConfig_);
}

HashBuilderWithExprOperator::HashBuilderWithExprOperator(omniruntime::mem::OmniTaskMemoryPool *pool,
    const DataTypes &buildTypes, std::vector<std::unique_ptr<Projection>> &projections,
    HashBuilderOperator *hashBuilderOperator, const config::QueryConfig &queryConfig)
    : Operator(pool), buildTypes(buildTypes), projections(projections), hashBuilderOperator(hashBuilderOperator)
{
    SetOperatorName(opNameForHashBuilder);
    executionContext->SetConfig(queryConfig);
}

HashBuilderWithExprOperator::~HashBuilderWithExprOperator()
{
    delete this->hashBuilderOperator;
}

int32_t HashBuilderWithExprOperator::AddInput(VectorBatch *vecBatch)
{
    if (vecBatch->GetRowCount() <= 0) {
        VectorHelper::FreeVecBatch(vecBatch);
        ResetInputVecBatch();
        return 0;
    }
    // Projection does not mutate the inner builder. Its allocations may reclaim
    // resident build input; AddInput observes the resulting spill latch below.
    // Projected batch becomes HashBuilder resident A; must be allocated pool-bound.
    // Projection scratch in this shell executionContext is transient (S / separate acceptance),
    // not part of Build A+B+C reclaimableBytes.
    auto *newInputVecBatch = OperatorUtil::ProjectVectors(vecBatch, buildTypes, projections, executionContext.get());
    VectorHelper::FreeVecBatch(vecBatch);
    ResetInputVecBatch();

    Operator::NonReclaimableSection innerNrs(hashBuilderOperator);
    if (innerNrsObserverForTests_) {
        innerNrsObserverForTests_(hashBuilderOperator);
    }
    hashBuilderOperator->AddInput(newInputVecBatch);
    SetStatus(hashBuilderOperator->GetStatus());
    return 0;
}

int32_t HashBuilderWithExprOperator::GetOutput(VectorBatch **outputVecBatch)
{
    Operator::NonReclaimableSection innerNrs(hashBuilderOperator);
    if (innerNrsObserverForTests_) {
        innerNrsObserverForTests_(hashBuilderOperator);
    }
    hashBuilderOperator->GetOutput(outputVecBatch);
    SetStatus(hashBuilderOperator->GetStatus());
    // Propagate inner build spill to this (collected) operator's stats_ → Spark UI Spill(disk).
    UpdateSpilledMetrics(hashBuilderOperator);
    return 0;
}

BlockingReason HashBuilderWithExprOperator::IsBlocked(ContinueFuture *future)
{
    return hashBuilderOperator->IsBlocked(future);
}

OmniStatus HashBuilderWithExprOperator::Close()
{
    hashBuilderOperator->Close();
    return OMNI_STATUS_NORMAL;
}
}
}