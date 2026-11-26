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

/*
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
    return new HashBuilderWithExprOperatorFactory(joinType, buildSide, buildTypes, buildHashKeys, hashTableCount,
                                                  overflowConfig);
}

HashBuilderWithExprOperatorFactory *HashBuilderWithExprOperatorFactory::CreateHashBuilderWithExprOperatorFactory(
    std::shared_ptr<const HashJoinNode> planNode, const config::QueryConfig &queryConfig)
{
    auto joinType = planNode->GetJoinType();
    auto buildSide = planNode->GetBuildSide();
    auto buildTypes = planNode->RightOutputType();
    auto buildKeys = planNode->RightKeys();

    auto overflowConfig = queryConfig.IsOverFlowASNull() ? new OverflowConfig(OVERFLOW_CONFIG_NULL)
                                                         : new OverflowConfig(OVERFLOW_CONFIG_EXCEPTION);
    auto hashBuilderWithExprOperatorFactory =
        new HashBuilderWithExprOperatorFactory(joinType, buildSide, *buildTypes, buildKeys, 1, overflowConfig);

    delete overflowConfig;
    overflowConfig = nullptr;
    return hashBuilderWithExprOperatorFactory;
}

HashBuilderWithExprOperatorFactory::HashBuilderWithExprOperatorFactory(
    JoinType joinType, const type::DataTypes &buildTypes,
    const std::vector<omniruntime::expressions::Expr *> &buildHashKeys, int32_t hashTableCount,
    OverflowConfig *overflowConfig)
{
    std::vector<DataTypePtr> newBuildTypes;
    OperatorUtil::CreateProjections(buildTypes, buildHashKeys, newBuildTypes, this->projections, this->buildHashCols,
                                    overflowConfig);
    this->buildTypes = std::make_unique<DataTypes>(newBuildTypes);
    this->operatorFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
        joinType, *(this->buildTypes), this->buildHashCols.data(), buildHashKeys.size(), hashTableCount);
}

HashBuilderWithExprOperatorFactory::HashBuilderWithExprOperatorFactory(
    JoinType joinType, BuildSide buildSide, const type::DataTypes &buildTypes,
    const std::vector<omniruntime::expressions::Expr *> &buildHashKeys, int32_t hashTableCount,
    OverflowConfig *overflowConfig)
{
    std::vector<DataTypePtr> newBuildTypes;
    OperatorUtil::CreateProjections(buildTypes, buildHashKeys, newBuildTypes, this->projections, this->buildHashCols,
                                    overflowConfig);
    this->buildTypes = std::make_unique<DataTypes>(newBuildTypes);
    this->operatorFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
        joinType, buildSide, *(this->buildTypes), this->buildHashCols.data(), buildHashKeys.size(), hashTableCount);
}

HashBuilderWithExprOperatorFactory::~HashBuilderWithExprOperatorFactory() { delete this->operatorFactory; }

Operator *HashBuilderWithExprOperatorFactory::CreateOperator()
{
    auto hashBuilderOperator = static_cast<HashBuilderOperator *>(operatorFactory->CreateOperator());
    return new HashBuilderWithExprOperator(*buildTypes, projections, hashBuilderOperator);
}

HashBuilderWithExprOperator::HashBuilderWithExprOperator(const DataTypes &buildTypes,
                                                         std::vector<std::unique_ptr<Projection>> &projections,
                                                         HashBuilderOperator *hashBuilderOperator)
    : buildTypes(buildTypes), projections(projections), hashBuilderOperator(hashBuilderOperator)
{
    SetOperatorName(opNameForHashBuilder);
}

HashBuilderWithExprOperator::~HashBuilderWithExprOperator() { delete this->hashBuilderOperator; }

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
} // namespace op
} // namespace omniruntime
