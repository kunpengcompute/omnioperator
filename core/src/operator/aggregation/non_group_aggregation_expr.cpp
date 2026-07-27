/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
 * @Description: Hash Aggregation WithExpr Source File
 */
#include "non_group_aggregation_expr.h"
#include "operator/util/operator_util.h"
#include "vector/vector_helper.h"
#include "agg_util.h"

using namespace std;
namespace omniruntime {
namespace op {
using namespace omniruntime::type;

AggregationWithExprOperatorFactory::AggregationWithExprOperatorFactory(
    std::vector<omniruntime::expressions::Expr *> &groupByKeys, uint32_t groupByNum,
    std::vector<std::vector<omniruntime::expressions::Expr *>> &aggsKeys, DataTypes &sourceDataTypes,
    std::vector<DataTypes> &aggOutputTypes, std::vector<uint32_t> &aggFuncTypes,
    std::vector<omniruntime::expressions::Expr *> &aggFilters, std::vector<uint32_t> &maskColumns,
    std::vector<bool> &inputRaws, std::vector<bool> &outputPartial, OverflowConfig *overflowConfig,
    bool isStatisticalAggregate, const std::vector<std::string> &aggUdafNames)
{
    uint32_t aggColNum = 0;
    for (auto &aggKeys : aggsKeys) {
        aggColNum += aggKeys.size();
    }

    // do  aggsKeys expression handle, and get new sourceTypes,  and agg columnar index
    uint32_t projectColNum = aggColNum;
    std::vector<omniruntime::expressions::Expr *> projectKeys(projectColNum);
    uint32_t projectColIdx = 0;
    for (auto &aggKeys : aggsKeys) {
        for (auto &aggKey : aggKeys) {
            projectKeys[projectColIdx] = aggKey;
            projectColIdx++;
        }
    }

    // do aggSimpleFilters
    this->aggFilterNum = static_cast<int32_t>(aggFilters.size());
    aggSimpleFilters.resize(aggFilterNum, nullptr);
    std::vector<int8_t> hasAggFilters(aggFilterNum, 0);
    for (int32_t i = 0; i < aggFilterNum; ++i) {
        auto aggFilter = aggFilters[i];
        if (aggFilter == nullptr) continue;
        auto simpleFilter = new SimpleFilter(*aggFilter);
        if (simpleFilter->Initialize(overflowConfig)) {
            aggSimpleFilters[i] = simpleFilter;
            hasAggFilters[i] = 1;
        } else {
            delete simpleFilter;
            for (auto it : aggSimpleFilters) {
                delete it;
            }
            aggSimpleFilters.clear();
            throw omniruntime::exception::OmniException("EXPRESSION_NOT_SUPPORT",
                "The expression is not supported yet.");
        }
    }

    std::vector<int32_t> groupByAndAggColumnarIdx;
    std::vector<DataTypePtr> newSourceTypes;
    OperatorUtil::CreateRequiredProjections(sourceDataTypes, projectKeys, newSourceTypes, this->projections,
        groupByAndAggColumnarIdx, *overflowConfig);
    uint32_t aggCols[aggColNum];
    for (uint32_t i = 0, j = groupByNum; i < aggColNum; i++, j++) {
        aggCols[i] = static_cast<uint32_t>(groupByAndAggColumnarIdx[j]);
    }
    // get agg columnar data types and index
    std::vector<std::vector<uint32_t>> aggColIdx;
    std::vector<DataTypes> aggInputDataTypes;
    std::vector<DataTypes> aggOutputDataTypes;
    uint32_t startIdx = 0;
    for (auto &aggKeys : aggsKeys) {
        // agg columnar index and data types
        std::vector<uint32_t> aggFuncColIdx;
        std::vector<DataTypePtr> aggInputTypeVec;
        uint32_t oneAggSize = aggKeys.size();
        for (uint32_t i = 0; i < oneAggSize; i++) {
            aggFuncColIdx.push_back(aggCols[startIdx + i]);
            aggInputTypeVec.push_back(newSourceTypes[aggCols[startIdx + i]]);
        }
        startIdx += oneAggSize;
        aggColIdx.push_back(aggFuncColIdx);
        aggInputDataTypes.push_back(*std::make_unique<DataTypes>(aggInputTypeVec));
    }
    originSourceTypes = std::make_unique<DataTypes>(sourceDataTypes);
    sourceTypes = std::make_unique<DataTypes>(newSourceTypes);
    aggOperatorFactory = new AggregationOperatorFactory(*sourceTypes, aggFuncTypes, aggColIdx, maskColumns,
        aggOutputTypes, inputRaws, outputPartial, hasAggFilters, overflowConfig->IsOverflowAsNull(),
        isStatisticalAggregate, aggUdafNames);
    aggOperatorFactory->Init();
}

AggregationWithExprOperatorFactory::AggregationWithExprOperatorFactory(
    std::vector<omniruntime::expressions::Expr *> &groupByKeys, uint32_t groupByNum,
    std::vector<std::vector<omniruntime::expressions::Expr *>> &aggsKeys, DataTypes &sourceDataTypes,
    std::vector<DataTypes> &aggOutputTypes, std::vector<uint32_t> &aggFuncTypes,
    std::vector<omniruntime::expressions::Expr *> &aggFilters, std::vector<uint32_t> &maskColumns,
    std::vector<bool> &inputRaws, std::vector<bool> &outputPartial, OverflowConfig *overflowConfig,
    const config::QueryConfig &queryConfig, bool isStatisticalAggregate,
    const std::vector<std::string> &aggUdafNames)
    : AggregationWithExprOperatorFactory(groupByKeys, groupByNum, aggsKeys, sourceDataTypes, aggOutputTypes, aggFuncTypes,
    aggFilters, maskColumns, inputRaws, outputPartial, overflowConfig, isStatisticalAggregate, aggUdafNames)
{
    this->queryConfig_ = queryConfig;
}

AggregationWithExprOperatorFactory::~AggregationWithExprOperatorFactory()
{
    delete aggOperatorFactory;
    for (auto it : aggSimpleFilters) {
        delete it;
    }
    aggSimpleFilters.clear();
}

Operator *AggregationWithExprOperatorFactory::CreateOperator()
{
    auto aggOperator = static_cast<AggregationOperator *>(aggOperatorFactory->CreateOperator());
    return new AggregationWithExprOperator(*originSourceTypes, *sourceTypes, projections, aggSimpleFilters,
        aggOperator, queryConfig_);
}

AggregationWithExprOperator::AggregationWithExprOperator(const DataTypes &originSourceTypes,
    const DataTypes &sourceTypes, std::vector<std::unique_ptr<Projection>> &projections,
    std::vector<SimpleFilter *> &aggSimpleFilters, AggregationOperator *aggOperator,
    const config::QueryConfig &queryConfig)
    : originTypes(originSourceTypes),
      sourceTypes(sourceTypes),
      projections(projections),
      aggOperator(aggOperator)
{
    auto aggFilterNum = aggSimpleFilters.size();
    this->aggSimpleFilters.resize(aggFilterNum, nullptr);
    for (size_t i = 0; i < aggFilterNum; ++i) {
        if (aggSimpleFilters[i] != nullptr) {
            hasAggFilter = true;
            this->aggSimpleFilters[i] = new SimpleFilter(*aggSimpleFilters[i]);
        }
    }
    executionContext->SetConfig(queryConfig);
}

AggregationWithExprOperator::~AggregationWithExprOperator()
{
    for (auto it: aggSimpleFilters) {
        delete it;
    }
    aggSimpleFilters.clear();
    delete aggOperator;
}

int32_t AggregationWithExprOperator::AddInput(VectorBatch *inputVecBatch)
{
    if (inputVecBatch->GetRowCount() <= 0) {
        VectorHelper::FreeVecBatch(inputVecBatch);
        ResetInputVecBatch();
        return 0;
    }

    auto newInputVecBatch = AggUtil::AggFilterRequiredVectors(inputVecBatch, originTypes, sourceTypes, projections,
        executionContext.get());

    // if hasAggFilter is false, then skip AddFilterColumn
    if (hasAggFilter) {
        try {
            // do filter and update newInputVecBatch
            AggUtil::AddFilterColumn(inputVecBatch, newInputVecBatch, aggSimpleFilters, executionContext.get(),
                originTypes);
        } catch (const std::exception &e) {
            VectorHelper::FreeVecBatch(inputVecBatch);
            ResetInputVecBatch();
            VectorHelper::FreeVecBatch(newInputVecBatch);
            throw e;
        }
    }
    VectorHelper::FreeVecBatch(inputVecBatch);
    ResetInputVecBatch();
    aggOperator->AddInput(newInputVecBatch);
    return 0;
}

int32_t AggregationWithExprOperator::GetOutput(VectorBatch **outputVecBatch)
{
    if (!noMoreInput_) {
        SetStatus(OMNI_STATUS_NORMAL);
        return 0;
    }
    if (isFinished() || aggOperator->isFinished()) {
        SetStatus(OMNI_STATUS_FINISHED);
        return 0;
    }
    auto status = aggOperator->GetOutput(outputVecBatch);
    SetStatus(static_cast<OmniStatus>(status));
    return 0;
}

OmniStatus AggregationWithExprOperator::Close()
{
    aggOperator->Close();
    return OMNI_STATUS_NORMAL;
}

AggregationWithExprOperatorFactory *AggregationWithExprOperatorFactory::CreateAggregationWithExprOperatorFactory(
    const std::shared_ptr<const AggregationNode> &planNode, const config::QueryConfig &queryConfig)
{
    auto groupByKeys = planNode->GetGroupByKeys();
    auto groupByNum = planNode->GetGroupByNum();
    auto aggsKeys = planNode->GetAggsKeys();
    auto sourceDataTypes = planNode->GetSourceDataTypes();
    auto aggsOutputTypes = planNode->GetAggsOutputTypes();
    auto aggFuncTypes = planNode->GetAggFuncTypes();
    auto aggFilters = planNode->GetAggFilters();
    auto maskColsVector = planNode->GetMaskColumns();
    auto inputRaws = planNode->GetInputRaws();
    auto outputPartial = planNode->GetOutputPartials();
    auto isStatisticalAggregate = planNode->GetIsStatisticalAggregate();
    auto overflowConfig = queryConfig.IsOverFlowASNull() == true ? new OverflowConfig(OVERFLOW_CONFIG_NULL)
                                                             : new OverflowConfig(OVERFLOW_CONFIG_EXCEPTION);
    auto pOperatorFactory = new AggregationWithExprOperatorFactory(groupByKeys, groupByNum, aggsKeys, *sourceDataTypes, aggsOutputTypes,
        aggFuncTypes, aggFilters, maskColsVector, inputRaws, outputPartial, overflowConfig,
        queryConfig, isStatisticalAggregate, planNode->GetAggUdafNames());

    delete overflowConfig;
    overflowConfig = nullptr;
    return pOperatorFactory;
}

}
}