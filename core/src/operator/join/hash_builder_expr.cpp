/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2021-2023. All rights reserved.
 * @Description: sort implementations
 */

#include "hash_builder_expr.h"
#include <memory>
#include <utility>
#ifdef OMNI_SVEHT32_JOIN_KEY_REWRITE
#include "operator/join/join_key_rewrite_utils.h"
#endif
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
    std::shared_ptr<const HashJoinNode> planNode, const config::QueryConfig &queryConfig)
{
    auto joinType = planNode->GetJoinType();
    auto buildSide = planNode->GetBuildSide();
    auto buildTypes = planNode->RightOutputType();
    auto buildKeys = planNode->RightKeys();
#ifdef OMNI_SVEHT32_JOIN_KEY_REWRITE
    const bool enableInt32KeyRewrite = ShouldRewriteJoinKeysToInt32(planNode->LeftKeys(), buildKeys);
#else
    const bool enableInt32KeyRewrite = false;
#endif

    auto overflowConfig = queryConfig.IsOverFlowASNull()
                          ? new OverflowConfig(OVERFLOW_CONFIG_NULL)
                          : new OverflowConfig(OVERFLOW_CONFIG_EXCEPTION);
    auto hashBuilderWithExprOperatorFactory = new HashBuilderWithExprOperatorFactory(
        joinType, buildSide, *buildTypes, buildKeys, 1, overflowConfig, queryConfig, enableInt32KeyRewrite);

    delete overflowConfig;
    overflowConfig = nullptr;
    return hashBuilderWithExprOperatorFactory;
}

HashBuilderWithExprOperatorFactory::HashBuilderWithExprOperatorFactory(JoinType joinType,
    const type::DataTypes &buildTypes, const std::vector<omniruntime::expressions::Expr *> &buildHashKeys,
    int32_t hashTableCount, OverflowConfig *overflowConfig, bool enableInt32KeyRewrite)
{
    std::vector<DataTypePtr> newBuildTypes;
#ifdef OMNI_SVEHT32_JOIN_KEY_REWRITE
    if (enableInt32KeyRewrite) {
        effectiveBuildHashKeys = RewriteJoinKeyExprsToInt32(buildHashKeys, rewrittenBuildHashKeyOwners);
    } else {
        effectiveBuildHashKeys.assign(buildHashKeys.begin(), buildHashKeys.end());
    }
#else
    (void)enableInt32KeyRewrite;
    effectiveBuildHashKeys.assign(buildHashKeys.begin(), buildHashKeys.end());
#endif
    OperatorUtil::CreateProjections(buildTypes, effectiveBuildHashKeys, newBuildTypes, this->projections, this->buildHashCols,
        overflowConfig);
    this->buildTypes = std::make_unique<DataTypes>(newBuildTypes);
    this->operatorFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(joinType, *(this->buildTypes),
        this->buildHashCols.data(), effectiveBuildHashKeys.size(), hashTableCount);
}

HashBuilderWithExprOperatorFactory::HashBuilderWithExprOperatorFactory(JoinType joinType, BuildSide buildSide,
    const type::DataTypes &buildTypes, const std::vector<omniruntime::expressions::Expr *> &buildHashKeys,
    int32_t hashTableCount, OverflowConfig *overflowConfig, bool enableInt32KeyRewrite)
{
    std::vector<DataTypePtr> newBuildTypes;
#ifdef OMNI_SVEHT32_JOIN_KEY_REWRITE
    if (enableInt32KeyRewrite) {
        effectiveBuildHashKeys = RewriteJoinKeyExprsToInt32(buildHashKeys, rewrittenBuildHashKeyOwners);
    } else {
        effectiveBuildHashKeys.assign(buildHashKeys.begin(), buildHashKeys.end());
    }
#else
    (void)enableInt32KeyRewrite;
    effectiveBuildHashKeys.assign(buildHashKeys.begin(), buildHashKeys.end());
#endif
    OperatorUtil::CreateProjections(buildTypes, effectiveBuildHashKeys, newBuildTypes, this->projections, this->buildHashCols,
        overflowConfig);
    this->buildTypes = std::make_unique<DataTypes>(newBuildTypes);
    this->operatorFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(joinType, buildSide,
        *(this->buildTypes), this->buildHashCols.data(), effectiveBuildHashKeys.size(), hashTableCount);
}

HashBuilderWithExprOperatorFactory::HashBuilderWithExprOperatorFactory(JoinType joinType, BuildSide buildSide,
    const type::DataTypes &buildTypes, const std::vector<omniruntime::expressions::Expr *> &buildHashKeys,
    int32_t hashTableCount, OverflowConfig *overflowConfig, const config::QueryConfig &queryConfig,
    bool enableInt32KeyRewrite)
    : HashBuilderWithExprOperatorFactory(
          joinType, buildSide, buildTypes, buildHashKeys, hashTableCount, overflowConfig, enableInt32KeyRewrite)
{
    this->queryConfig_ = queryConfig;
}

HashBuilderWithExprOperatorFactory::~HashBuilderWithExprOperatorFactory()
{
    delete this->operatorFactory;
}

Operator *HashBuilderWithExprOperatorFactory::CreateOperator()
{
    auto hashBuilderOperator = static_cast<HashBuilderOperator *>(operatorFactory->CreateOperator());
    return new HashBuilderWithExprOperator(*buildTypes, projections, hashBuilderOperator, queryConfig_);
}

HashBuilderWithExprOperator::HashBuilderWithExprOperator(const DataTypes &buildTypes,
    std::vector<std::unique_ptr<Projection>> &projections, HashBuilderOperator *hashBuilderOperator
    , const config::QueryConfig &queryConfig)
    : buildTypes(buildTypes), projections(projections), hashBuilderOperator(hashBuilderOperator)
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
    auto *newInputVecBatch = OperatorUtil::ProjectVectors(vecBatch, buildTypes, projections, executionContext.get());
    VectorHelper::FreeVecBatch(vecBatch);
    ResetInputVecBatch();

    hashBuilderOperator->AddInput(newInputVecBatch);
    SetStatus(hashBuilderOperator->GetStatus());
    return 0;
}

int32_t HashBuilderWithExprOperator::GetOutput(VectorBatch **outputVecBatch)
{
    hashBuilderOperator->GetOutput(outputVecBatch);
    SetStatus(hashBuilderOperator->GetStatus());
    return 0;
}

OmniStatus HashBuilderWithExprOperator::Close()
{
    hashBuilderOperator->Close();
    return OMNI_STATUS_NORMAL;
}
}
}
