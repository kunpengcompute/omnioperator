/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 */

#include "topn_sort_expr.h"
#include "operator/util/operator_util.h"

namespace omniruntime::op {
TopNSortWithExprOperatorFactory::TopNSortWithExprOperatorFactory(const type::DataTypes &sourceDataTypes, int32_t n,
    bool isStrictTopN, const std::vector<omniruntime::expressions::Expr *> &partitionKeys,
    const std::vector<omniruntime::expressions::Expr *> &sortKeys, std::vector<int32_t> &sortAscendings,
    std::vector<int32_t> &sortNullFirsts, OverflowConfig *overflowConfig)
{
    std::vector<DataTypePtr> sourceTypesForPartition;
    OperatorUtil::CreateProjections(sourceDataTypes, partitionKeys, sourceTypesForPartition, this->projections,
        this->partitionCols, overflowConfig);

    DataTypes newSourceDataTypes(sourceTypesForPartition);
    std::vector<DataTypePtr> sourceTypesForSort;
    OperatorUtil::CreateProjections(newSourceDataTypes, sortKeys, sourceTypesForSort, this->projections, this->sortCols,
        overflowConfig);

    this->sourceTypes = std::make_unique<DataTypes>(sourceTypesForSort);
    this->topNSortOperatorFactory = std::make_unique<TopNSortOperatorFactory>(*sourceTypes, n, isStrictTopN,
        this->partitionCols, this->sortCols, sortAscendings, sortNullFirsts);
}

TopNSortWithExprOperatorFactory::TopNSortWithExprOperatorFactory(const type::DataTypes &sourceDataTypes, int32_t n,
        bool isStrictTopN, const std::vector<omniruntime::expressions::Expr *> &partitionKeys,
        const std::vector<omniruntime::expressions::Expr *> &sortKeys, std::vector<int32_t> &sortAscendings,
        std::vector<int32_t> &sortNullFirsts, OverflowConfig *overflowConfig, const config::QueryConfig &queryConfig)
    : TopNSortWithExprOperatorFactory(sourceDataTypes, n, isStrictTopN, partitionKeys, sortKeys, sortAscendings,
        sortNullFirsts, overflowConfig)
{
    this->queryConfig_ = queryConfig;
    topNSortOperatorFactory->SetQueryConfig(queryConfig);
}

TopNSortWithExprOperatorFactory* TopNSortWithExprOperatorFactory::CreateTopNSortWithExprOperatorFactory(
    std::shared_ptr<const TopNSortNode> planNode, const config::QueryConfig &queryConfig)
{
    auto sourceTypes = planNode->getSourceTypes();
    auto partitionKeys = planNode->getPartitionKeys();
    auto sortKeys = planNode->getSortKeys();
    auto n = planNode->getN();
    auto isStrictTopN = planNode->getIsStrictTopN();
    auto sortAscendings = planNode->getSortAscendings();
    auto sortNullFirsts = planNode->getSortNullFirsts();
    auto overflowConfig = queryConfig.IsOverFlowASNull() == true ? new OverflowConfig(OVERFLOW_CONFIG_NULL)
                                                                         : new OverflowConfig(OVERFLOW_CONFIG_EXCEPTION);

    auto pOperatorFactory = new TopNSortWithExprOperatorFactory(*sourceTypes.get(), n, isStrictTopN, partitionKeys, sortKeys,
        sortAscendings, sortNullFirsts, overflowConfig, queryConfig);

    delete overflowConfig;
    overflowConfig = nullptr;
    return pOperatorFactory;
}

TopNSortWithExprOperatorFactory::~TopNSortWithExprOperatorFactory() = default;

Operator *TopNSortWithExprOperatorFactory::CreateOperator()
{
    auto topNSortOperator = static_cast<TopNSortOperator *>(topNSortOperatorFactory->CreateOperator());
    auto pOperator = new TopNSortWithExprOperator(*sourceTypes, partitionCols, sortCols, projections,
        topNSortOperator, queryConfig_);
    return pOperator;
}

TopNSortWithExprOperator::TopNSortWithExprOperator(const type::DataTypes &sourceTypes,
    std::vector<int32_t> &partitionCols, std::vector<int32_t> &sortCols,
    std::vector<std::unique_ptr<Projection>> &projections, TopNSortOperator *topNSortOperator,
    const config::QueryConfig &queryConfig)
    : sourceTypes(sourceTypes), projections(projections), topNSortOperator(topNSortOperator)
{
    executionContext->SetConfig(queryConfig);
}

TopNSortWithExprOperator::~TopNSortWithExprOperator()
{
    delete topNSortOperator;
}

int32_t TopNSortWithExprOperator::AddInput(VectorBatch *inputVecBatch)
{
    if (inputVecBatch->GetRowCount() <= 0) {
        VectorHelper::FreeVecBatch(inputVecBatch);
        this->ResetInputVecBatch();
        return 0;
    }
    auto *newInputVecBatch =
        OperatorUtil::ProjectVectors(inputVecBatch, sourceTypes, projections, executionContext.get());
    VectorHelper::FreeVecBatch(inputVecBatch);
    this->ResetInputVecBatch();
    topNSortOperator->AddInput(newInputVecBatch);
    SetStatus(topNSortOperator->GetStatus());
    return 0;
}

int32_t TopNSortWithExprOperator::GetOutput(VectorBatch **outputVecBatch)
{
    topNSortOperator->GetOutput(outputVecBatch);
    SetStatus(topNSortOperator->GetStatus());
    return 0;
}

OmniStatus TopNSortWithExprOperator::Close()
{
    topNSortOperator->Close();
    return OMNI_STATUS_NORMAL;
}
}
