/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2021. All rights reserved.
 */

#include "topn_expr.h"
#include "operator/util/operator_util.h"

namespace omniruntime::op {
TopNWithExprOperatorFactory::TopNWithExprOperatorFactory(const type::DataTypes &sourceDataTypes, int32_t limit,
    int32_t offset, const std::vector<omniruntime::expressions::Expr *> &sortKeys, int32_t *sortAsc,
    int32_t *sortNullFirsts, int32_t sortKeyCount, OverflowConfig *overflowConfig)
{
    std::vector<DataTypePtr> newSourceTypes;
    OperatorUtil::CreateProjections(sourceDataTypes, sortKeys, newSourceTypes, this->projections, this->sortCols,
        overflowConfig);

    this->sourceTypes = std::make_unique<DataTypes>(newSourceTypes);
    this->topNOperatorFactory = std::make_unique<TopNOperatorFactory>(*sourceTypes,
    limit, offset, this->sortCols.data(), sortAsc, sortNullFirsts, sortKeyCount);
}

TopNWithExprOperatorFactory::TopNWithExprOperatorFactory(const type::DataTypes &sourceDataTypes, int32_t limit,
        int32_t offset, const std::vector<omniruntime::expressions::Expr *> &sortKeys, int32_t *sortAsc,
        int32_t *sortNullFirsts, int32_t sortKeyCount, OverflowConfig *overflowConfig, const config::QueryConfig &queryConfig)
    : TopNWithExprOperatorFactory(sourceDataTypes, limit, offset, sortKeys, sortAsc, sortNullFirsts, sortKeyCount, overflowConfig)
{
    this->queryConfig_ = queryConfig;
    topNOperatorFactory->SetQueryConfig(queryConfig);
}

TopNWithExprOperatorFactory *TopNWithExprOperatorFactory::CreateTopNWithExprOperatorFactory(
    std::shared_ptr<const TopNNode> planNode, const config::QueryConfig &queryConfig)
{
    auto dataTypes = planNode->GetSourceTypes();
    int32_t cnt = planNode->Count();
    auto sortCols = planNode->GetSortCols();
    auto sortAscending = planNode->GetSortAscending();
    auto sortNullFirsts = planNode->GetNullFirsts();
    size_t sortColCnt = sortCols.size();
    auto overflowConfig = queryConfig.IsOverFlowASNull() == true ? new OverflowConfig(OVERFLOW_CONFIG_NULL)
                                                                         : new OverflowConfig(OVERFLOW_CONFIG_EXCEPTION);
    auto pOperatorFactory = new TopNWithExprOperatorFactory(*dataTypes.get(), cnt, 0, sortCols, sortAscending.data(),
        sortNullFirsts.data(), sortColCnt, overflowConfig, queryConfig);
    delete overflowConfig;
    overflowConfig = nullptr;
    return pOperatorFactory;
}

TopNWithExprOperatorFactory::~TopNWithExprOperatorFactory() = default;

Operator *TopNWithExprOperatorFactory::CreateOperator()
{
    auto topNOperator = static_cast<TopNOperator *>(topNOperatorFactory->CreateOperator());
    auto pOperator = new TopNWithExprOperator(*sourceTypes, projections, topNOperator, queryConfig_);
    return pOperator;
}

TopNWithExprOperator::TopNWithExprOperator(const type::DataTypes &sourceTypes,
    std::vector<std::unique_ptr<Projection>> &projections, TopNOperator *topNOperator, const config::QueryConfig &queryConfig)
    : sourceTypes(sourceTypes), projections(projections), topNOperator(topNOperator)
{
    executionContext->SetConfig(queryConfig);
}

TopNWithExprOperator::~TopNWithExprOperator()
{
    delete topNOperator;
}

int32_t TopNWithExprOperator::AddInput(VectorBatch *inputVecBatch)
{
    if (inputVecBatch->GetRowCount() <= 0) {
        VectorHelper::FreeVecBatch(inputVecBatch);
        this->ResetInputVecBatch();
        return 0;
    }

    auto *newInputVecBatch =
        OperatorUtil::ProjectVectors(inputVecBatch, sourceTypes, projections, executionContext.get());
    VectorHelper::FreeVecBatch(inputVecBatch);
    this->inputVecBatch = nullptr;

    topNOperator->SetInputVecBatch(newInputVecBatch);
    topNOperator->AddInput(newInputVecBatch);
    SetStatus(topNOperator->GetStatus());
    return 0;
}

int32_t TopNWithExprOperator::GetOutput(VectorBatch **outputVecBatch)
{
    topNOperator->GetOutput(outputVecBatch);
    SetStatus(topNOperator->GetStatus());
    return 0;
}

OmniStatus TopNWithExprOperator::Close()
{
    topNOperator->Close();
    return OMNI_STATUS_NORMAL;
}
}
