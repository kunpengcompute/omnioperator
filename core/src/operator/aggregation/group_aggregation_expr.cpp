/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2021-2024. All rights reserved.
 * @Description: Hash Aggregation WithExpr Source File
 */

#include "group_aggregation_expr.h"
#include "agg_util.h"
#include "operator/util/operator_util.h"
#include "vector/vector_helper.h"

using namespace std;
namespace omniruntime {
namespace op {
using namespace omniruntime::type;

HashAggregationWithExprOperatorFactory::HashAggregationWithExprOperatorFactory(
    std::vector<omniruntime::expressions::Expr *> &groupByKeys, uint32_t groupByNum,
    std::vector<std::vector<omniruntime::expressions::Expr *>> &aggsKeys,
    std::vector<omniruntime::expressions::Expr *> &aggFilters, DataTypes &sourceDataTypes,
    std::vector<DataTypes> &aggOutputTypes, std::vector<uint32_t> &aggFuncTypes, std::vector<uint32_t> &maskColumns,
    std::vector<bool> &inputRaws, std::vector<bool> &outputPartial, const OperatorConfig &operatorConfig,
    config::QueryConfig queryConfig, AggregationNode::Step step, bool mixedInputExpected, bool mixedOutputEnabled)
{
    this->queryConfig_ = queryConfig;
    uint32_t aggColNum = 0;
    for (auto &aggKeys : aggsKeys) {
        aggColNum += aggKeys.size();
    }

    // do groupByKeys and aggsKeys expression handle, and get new sourceTypes, groupby and agg columnar index
    uint32_t projectColNum = groupByNum + aggColNum;
    std::vector<omniruntime::expressions::Expr *> projectKeys(projectColNum);
    for (uint32_t i = 0; i < groupByNum; i++) {
        projectKeys[i] = groupByKeys[i];
    }

    uint32_t projectColIdx = groupByNum;
    for (auto &aggKeys : aggsKeys) {
        for (auto &aggKey : aggKeys) {
            projectKeys[projectColIdx] = aggKey;
            projectColIdx++;
        }
    }

    // do aggSimpleFilters
    auto overflowConfig = operatorConfig.GetOverflowConfig();
    this->aggFilterNum = static_cast<int32_t>(aggFilters.size());
    aggSimpleFilters.resize(aggFilterNum, nullptr);
    std::vector<int8_t> hasAggFilters(aggFilterNum, 0);
    for (int32_t i = 0; i < aggFilterNum; ++i) {
        auto aggFilter = aggFilters[i];
        if (aggFilter != nullptr) {
            auto simpleFilter = new SimpleFilter(*aggFilter);
            if (simpleFilter->Initialize(overflowConfig)) {
                aggSimpleFilters[i] = simpleFilter;
                hasAggFilters[i] = 1;
            } else {
                delete simpleFilter;
                throw omniruntime::exception::OmniException("EXPRESSION_NOT_SUPPORT",
                    "The expression is not supported yet.");
            }
        }
    }

    std::vector<int32_t> groupByAndAggColumnarIdx;
    std::vector<DataTypePtr> newSourceTypes;
    OperatorUtil::CreateRequiredProjections(sourceDataTypes, projectKeys, newSourceTypes, this->projections,
        groupByAndAggColumnarIdx, *overflowConfig);
    uint32_t groupByCols[groupByNum];
    for (uint32_t i = 0; i < groupByNum; i++) {
        groupByCols[i] = static_cast<uint32_t>(groupByAndAggColumnarIdx[i]);
    }
    uint32_t aggCols[aggColNum];
    for (uint32_t i = 0, j = groupByNum; i < aggColNum; i++, j++) {
        aggCols[i] = static_cast<uint32_t>(groupByAndAggColumnarIdx[j]);
    }

    // get groupby columnar data types and index
    std::vector<DataTypePtr> groupByTypeVec;
    groupByTypeVec.reserve(groupByNum);
    for (uint32_t i = 0; i < groupByNum; i++) {
        groupByTypeVec.push_back(newSourceTypes[groupByCols[i]]);
    }
    this->groupByTypes = std::make_unique<DataTypes>(groupByTypeVec);
    std::vector<uint32_t> groupByCol =
        std::vector<uint32_t>(static_cast<uint32_t *>(groupByCols), static_cast<uint32_t *>(groupByCols) + groupByNum);

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

    this->originSourceTypes = std::make_unique<DataTypes>(sourceDataTypes);
    this->sourceTypes = std::make_unique<DataTypes>(newSourceTypes);
    this->hashAggOperatorFactory =
        new HashAggregationOperatorFactory(groupByCol, *groupByTypes, aggColIdx, aggInputDataTypes, aggOutputTypes,
        aggFuncTypes, maskColumns, inputRaws, outputPartial, hasAggFilters, operatorConfig, step,
        mixedInputExpected, mixedOutputEnabled);
    this->hashAggOperatorFactory->SetNormalizedKeyEnabledForFactory(
        queryConfig.HashAggNormalizedKeyEnabled());
    this->hashAggOperatorFactory->Init();
}

HashAggregationWithExprOperatorFactory::~HashAggregationWithExprOperatorFactory()
{
    delete hashAggOperatorFactory;
    for (auto it : aggSimpleFilters) {
        delete it;
    }
    aggSimpleFilters.clear();
}

HashAggregationWithExprOperatorFactory *HashAggregationWithExprOperatorFactory::CreateAggregationWithExprOperatorFactory(
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
    auto overflowConfig = queryConfig.IsOverFlowASNull() == true
                              ? new OverflowConfig(OVERFLOW_CONFIG_NULL)
                              : new OverflowConfig(OVERFLOW_CONFIG_EXCEPTION);
    auto spillConfig = new SparkSpillConfig(queryConfig.aggregationSpillEnabled(), queryConfig.SpillDir(),
        queryConfig.SpillDirDiskReserveSize(), queryConfig.SpillHashAggRowThreshold(),
        queryConfig.memFractionPct(), queryConfig.SpillWriteBufferSize(), queryConfig.SpillEnableCompress());
    auto operatorConfig = std::make_shared<OperatorConfig>(spillConfig, overflowConfig);

    return new HashAggregationWithExprOperatorFactory(groupByKeys, groupByNum, aggsKeys, aggFilters, *sourceDataTypes,
                                                      aggsOutputTypes,
                                                      aggFuncTypes, maskColsVector, inputRaws, outputPartial,
                                                      *operatorConfig, queryConfig, planNode->GetStep(),
                                                      planNode->IsMixedInputExpected(), planNode->IsMixedOutputEnabled());
}

Operator *HashAggregationWithExprOperatorFactory::CreateOperator()
{
    auto hashAggOperator = static_cast<HashAggregationOperator *>(hashAggOperatorFactory->CreateOperator());
    auto *op = new HashAggregationWithExprOperator(*originSourceTypes, *sourceTypes, projections, aggSimpleFilters,
        hashAggOperator, queryConfig_);
    op->Init();
    return op;
}

HashAggregationWithExprOperator::HashAggregationWithExprOperator(const DataTypes &originSourceTypes,
    const DataTypes &sourceTypes, std::vector<std::unique_ptr<Projection>> &projections,
    std::vector<SimpleFilter *> &aggSimpleFilters, HashAggregationOperator *hashAggOperator,
    config::QueryConfig queryConfig)
    : originTypes(originSourceTypes),
      sourceTypes(sourceTypes),
      projections(projections),
      hashAggOperator(hashAggOperator)
{
    executionContext->SetConfig(queryConfig);
    adaptivePartialAgg = queryConfig.EnableAdaptivePartialAggregation();
    adaptivePartialAggMinRows = queryConfig.AdaptivePartialAggregationMinRows();
    adaptivePartialAggRatio = queryConfig.AdaptivePartialAggregationRatio();
    auto aggFilterNum = aggSimpleFilters.size();
    this->aggSimpleFilters.resize(aggFilterNum, nullptr);
    for (size_t i = 0; i < aggFilterNum; ++i) {
        if (aggSimpleFilters[i] != nullptr) {
            hasAggFilter = true;
            this->aggSimpleFilters[i] = new SimpleFilter(*aggSimpleFilters[i]);
        }
    }
}

HashAggregationWithExprOperator::~HashAggregationWithExprOperator()
{
    for (auto it: aggSimpleFilters) {
        delete it;
    }
    aggSimpleFilters.clear();
    delete hashAggOperator;
}

int32_t HashAggregationWithExprOperator::AddInput(VectorBatch *inputVecBatch)
{
    if (inputVecBatch->GetRowCount() <= 0) {
        VectorHelper::FreeVecBatch(inputVecBatch);
        ResetInputVecBatch();
        if (abandonedPartialAggregation) {
            isActivateAdaptivePartial = true;
            abandonedInputVecBatch = nullptr;
        }
        return 0;
    }
    numInputRows += inputVecBatch->GetRowCount();
    if (abandonedPartialAggregation) {
        isActivateAdaptivePartial = true;
        abandonedInputVecBatch = inputVecBatch;
        return 0;
    }

    VectorBatch *newInputVecBatch = inputVecBatch;
    if (inputVecBatch->MixType() == 1) {
        newInputVecBatch = inputVecBatch;
    } else {
        newInputVecBatch = AggUtil::AggFilterRequiredVectors(inputVecBatch, originTypes, sourceTypes,
                                                              projections, executionContext.get());
    }

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
    if (inputVecBatch->MixType() == 0) {
        VectorHelper::FreeVecBatch(inputVecBatch);
    }
    ResetInputVecBatch();
    hashAggOperator->AddInput(newInputVecBatch);

    if (adaptivePartialAgg && IsStepPartials() && numInputRows > adaptivePartialAggMinRows) {
        long keyNums = GetHashMapUniqueKeys();
        if (keyNums > adaptivePartialAggRatio * numInputRows) {
            abandonedPartialAggregation = true;
        }
    }
    return 0;
}

void HashAggregationWithExprOperator::ProcessRow(uintptr_t rowValues[], int32_t lens[])
{
    auto inputVecBatch = oneRowAdaptor.Trans2VectorBatch(rowValues, lens);
    VectorBatch *newInputVecBatch =
        AggUtil::AggFilterRequiredVectors(inputVecBatch, originTypes, sourceTypes, projections, executionContext.get());
    hashAggOperator->AddInput(newInputVecBatch);
    // no need to delete inputVecBatch, it will be reused when this interface call again
}

int32_t HashAggregationWithExprOperator::GetOutput(VectorBatch **outputVecBatch)
{
    if (hashAggOperator->isFinished() && isActivateAdaptivePartial) {
        if (abandonedInputVecBatch == nullptr) {
            if (!noMoreInput_) {
                SetStatus(OMNI_STATUS_NORMAL);
                return 0;
            }
            UpdateSpilledMetrics(hashAggOperator);
            SetStatus(OMNI_STATUS_FINISHED);
            return 0;
        }
        *outputVecBatch = AlignSchema(abandonedInputVecBatch);
        abandonedInputVecBatch = nullptr;
        return 1;
    }
    if (abandonedPartialAggregation) {
        if (hashAggOperator->isFinished()) {
            if (!noMoreInput_) {
                SetStatus(OMNI_STATUS_NORMAL);
                return 0;
            }
            UpdateSpilledMetrics(hashAggOperator);
            SetStatus(OMNI_STATUS_FINISHED);
            return 0;
        }
        int32_t status = hashAggOperator->GetOutput(outputVecBatch);
        return status;
    }
    if (!noMoreInput_) {
        SetStatus(OMNI_STATUS_NORMAL);
        return 0;
    }
    if (isFinished() || hashAggOperator->isFinished()) {
        UpdateSpilledMetrics(hashAggOperator);
        SetStatus(OMNI_STATUS_FINISHED);
        return 0;
    }
    int32_t status = hashAggOperator->GetOutput(outputVecBatch);
    SetStatus(hashAggOperator->GetStatus());
    if (*outputVecBatch == nullptr) {
        SetStatus(OMNI_STATUS_FINISHED);
    }

    if (status == OMNI_STATUS_FINISHED) {
        UpdateSpilledMetrics(hashAggOperator);
    }
    return status;
}

OmniStatus HashAggregationWithExprOperator::Close()
{
    hashAggOperator->Close();
    return OMNI_STATUS_NORMAL;
}

uint64_t HashAggregationWithExprOperator::GetSpilledBytes()
{
    return hashAggOperator->GetSpilledBytes();
}

uint64_t HashAggregationWithExprOperator::GetUsedMemBytes()
{
    return hashAggOperator->GetUsedMemBytes();
}

uint64_t HashAggregationWithExprOperator::GetTotalMemBytes()
{
    return hashAggOperator->GetTotalMemBytes();
}

std::vector<uint64_t> HashAggregationWithExprOperator::GetSpecialMetricsInfo()
{
    return hashAggOperator->GetSpecialMetricsInfo();
}

uint64_t HashAggregationWithExprOperator::GetHashMapUniqueKeys()
{
    return hashAggOperator->GetHashMapUniqueKeys();
}

VectorBatch *HashAggregationWithExprOperator::AlignSchema(VectorBatch *inputVecBatch)
{
    VectorBatch *newInputVecBatch =
        AggUtil::AggFilterRequiredVectors(inputVecBatch, originTypes, sourceTypes, projections, executionContext.get());

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

    VectorBatch *result = hashAggOperator->AlignSchema(newInputVecBatch);
    return result;
}

OmniStatus HashAggregationWithExprOperator::Init()
{
    oneRowAdaptor.Init(sourceTypes);
    return OMNI_STATUS_NORMAL;
}
}
}
