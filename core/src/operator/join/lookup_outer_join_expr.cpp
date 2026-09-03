/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
 * @Description: lookup outer join implementations
 */

#include "lookup_outer_join_expr.h"
#include "hash_builder_expr.h"
#include "operator/util/operator_util.h"
#include "vector/vector_helper.h"

namespace omniruntime {
namespace op {
using namespace omniruntime::vec;

LookupOuterJoinWithExprOperatorFactory *
LookupOuterJoinWithExprOperatorFactory::CreateLookupOuterJoinWithExprOperatorFactory(const type::DataTypes &probeTypes,
    int32_t *probeOutputCols, int32_t probeOutputColsCount,
    const std::vector<omniruntime::expressions::Expr *> &probeHashKeys, int32_t probeHashKeysCount,
    int32_t *buildOutputCols, const type::DataTypes &buildOutputTypes, int64_t hashBuilderFactoryAddr)
{
    auto operatorFactory = new LookupOuterJoinWithExprOperatorFactory(probeTypes, probeOutputCols, probeOutputColsCount,
        probeHashKeys, probeHashKeysCount, buildOutputCols, buildOutputTypes, hashBuilderFactoryAddr);
    return operatorFactory;
}

LookupOuterJoinWithExprOperatorFactory *LookupOuterJoinWithExprOperatorFactory::CreateLookupOuterJoinWithExprOperatorFactory(
    std::shared_ptr<const HashJoinNode> planNode, HashBuilderOperatorFactory* hashBuilderOperatorFactory, const config::QueryConfig& queryConfig)
{
    auto buildOutputTypes = planNode->RightOutputType();
    auto buildOutputColsCount = buildOutputTypes->GetSize();
    std::vector<int32_t> buildOutputCols;
    for (int32_t index = 0; index < buildOutputColsCount; index++) {
        buildOutputCols.emplace_back(index);
    }

    auto probeOutputTypes = planNode->LeftOutputType();
    auto probeOutputColsCount = probeOutputTypes->GetSize();
    std::vector<int32_t> probeOutputCols;
    for (int32_t index = 0; index < probeOutputColsCount; index++) {
        probeOutputCols.emplace_back(index);
    }

    auto probeHashKeys = planNode->LeftKeys();
    auto probeHashKeysCount = static_cast<int>(probeHashKeys.size());
    auto buildSide = planNode->GetBuildSide();

    auto operatorFactory = new LookupOuterJoinWithExprOperatorFactory(*probeOutputTypes, probeOutputCols.data(),
        probeOutputColsCount, probeHashKeys, probeHashKeysCount, buildOutputCols.data(), *buildOutputTypes,
        reinterpret_cast<int64_t>(hashBuilderOperatorFactory), buildSide);
    operatorFactory->SetQueryConfig(queryConfig);
    return operatorFactory;
}

LookupOuterJoinWithExprOperatorFactory::LookupOuterJoinWithExprOperatorFactory(const type::DataTypes &probeTypes,
    int32_t *probeOutputCols, int32_t probeOutputColsCount,
    const std::vector<omniruntime::expressions::Expr *> &probeHashKeys, int32_t probeHashKeysCount,
    int32_t *buildOutputCols, const type::DataTypes &buildOutputTypes, int64_t hashBuilderFactoryAddr)
{
    std::vector<DataTypePtr> newProbeTypes;
    OperatorUtil::CreateProjections(probeTypes, probeHashKeys, newProbeTypes, this->projections, this->probeHashCols,
        nullptr);
    this->probeTypes = std::make_unique<DataTypes>(DataTypes(newProbeTypes));
    this->operatorFactory = LookupOuterJoinOperatorFactory::CreateLookupOuterJoinOperatorFactory(*(this->probeTypes),
        probeOutputCols, probeOutputColsCount, buildOutputCols, buildOutputTypes, hashBuilderFactoryAddr);
}

LookupOuterJoinWithExprOperatorFactory::LookupOuterJoinWithExprOperatorFactory(const type::DataTypes &probeTypes,
    int32_t *probeOutputCols, int32_t probeOutputColsCount,
    const std::vector<omniruntime::expressions::Expr *> &probeHashKeys, int32_t probeHashKeysCount,
    int32_t *buildOutputCols, const type::DataTypes &buildOutputTypes, int64_t hashBuilderFactoryAddr, BuildSide buildSide)
{
    std::vector<DataTypePtr> newProbeTypes;
    OperatorUtil::CreateProjections(probeTypes, probeHashKeys, newProbeTypes, this->projections, this->probeHashCols,
                                    nullptr);
    this->probeTypes = std::make_unique<DataTypes>(DataTypes(newProbeTypes));
    this->operatorFactory = LookupOuterJoinOperatorFactory::CreateLookupOuterJoinOperatorFactory(*(this->probeTypes),
        probeOutputCols, probeOutputColsCount, buildOutputCols, buildOutputTypes,
        hashBuilderFactoryAddr, buildSide);
}

LookupOuterJoinWithExprOperatorFactory::~LookupOuterJoinWithExprOperatorFactory()
{
    delete this->operatorFactory;
}

Operator *LookupOuterJoinWithExprOperatorFactory::CreateOperator()
{
    auto lookupOuterJoinOperator = static_cast<LookupOuterJoinOperator *>(operatorFactory->CreateOperator());
    return new LookupOuterJoinWithExprOperator(lookupOuterJoinOperator);
}

LookupOuterJoinWithExprOperator::LookupOuterJoinWithExprOperator(LookupOuterJoinOperator *lookupJoinOperator)
    : lookupOuterJoinOperator(lookupJoinOperator)
{
    SetOperatorName(opNameForLookUpJoin);
}

LookupOuterJoinWithExprOperator::~LookupOuterJoinWithExprOperator()
{
    delete lookupOuterJoinOperator;
}

int32_t LookupOuterJoinWithExprOperator::AddInput(VectorBatch *vecBatch)
{
    // since LookupOuterJoinOperator do nothing in AddInput
    return 0;
}

int32_t LookupOuterJoinWithExprOperator::GetOutput(VectorBatch **outputVecBatch)
{
    lookupOuterJoinOperator->GetOutput(outputVecBatch);
    SetStatus(lookupOuterJoinOperator->GetStatus());
    return 0;
}

OmniStatus LookupOuterJoinWithExprOperator::Close()
{
    lookupOuterJoinOperator->Close();
    return OMNI_STATUS_NORMAL;
}
}
}
