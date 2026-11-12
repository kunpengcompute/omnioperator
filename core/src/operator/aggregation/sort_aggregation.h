/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2024. All rights reserved.
 * Description: Sort Aggregation Header
 */
#ifndef SORT_AGGREGATION_H
#define SORT_AGGREGATION_H

#include "definitions.h"
#include "aggregation.h"
#include "type/data_types.h"
#include "operator/execution_context.h"
#include "operator/aggregation/aggregator/only_aggregator_factory.h"
#include "operator/aggregation/aggregator/aggregator_factory.h"
#include "operator/config/operator_config.h"
#include "operator/util/operator_util.h"
#include "vector/vector_helper.h"

namespace omniruntime::op {
using namespace vec;

class SortAggregationOperatorFactory;

class SortAggregationOperator : public AggregationCommonOperator {
public:
    SortAggregationOperator(std::vector<ColumnIndex> &groupByCols,
        std::vector<std::vector<int32_t>> &aggInputCols, uint32_t aggInputColsSize,
        std::vector<DataTypes> &aggInputTypes, std::vector<DataTypes> &aggOutputTypes,
        std::vector<std::unique_ptr<Aggregator>> &&aggs, std::vector<bool> &inputRaws,
        std::vector<bool> &outputPartials, const std::vector<int8_t> &hasAggFilters,
        const OperatorConfig &operatorConfig, std::vector<uint32_t> aggFuncTypesVector,
        AggregationNode::Step step)
        : AggregationCommonOperator(std::move(aggs), inputRaws, outputPartials),
          groupByCols(groupByCols),
          aggInputCols(aggInputCols),
          aggInputColsSize(aggInputColsSize),
          aggInputTypes(aggInputTypes),
          aggOutputTypes(aggOutputTypes),
          hasAggFilters(hasAggFilters),
          operatorConfig(operatorConfig)
    {
        for (auto hasFilter : hasAggFilters) {
            if (hasFilter == 1) {
                aggFiltersCount++;
            }
        }
    }

    ~SortAggregationOperator() override = default;

    int32_t AddInput(VectorBatch *data) override;

    int32_t GetOutput(VectorBatch **outputVecBatch) override;

    OmniStatus Init() override;

    OmniStatus Close() override;

    void SetGroupKeyTypes(const std::vector<type::DataTypePtr> &types)
    {
        groupKeyTypes = types;
    }

    friend class SortAggregationOperatorFactory;

private:
    bool CompareGroupKey(VectorBatch *vecBatch, int32_t rowIdx);

    void CacheGroupKey(VectorBatch *vecBatch, int32_t rowIdx);

    void FinalizeCurrentGroup();

    void ResetAggStates();

    void OutputGroup(VectorBatch *output, int32_t outputRowIdx);

    std::vector<ColumnIndex> groupByCols;
    std::vector<std::vector<int32_t>> aggInputCols;
    uint32_t aggInputColsSize;
    std::vector<DataTypes> aggInputTypes;
    std::vector<DataTypes> aggOutputTypes;
    std::vector<int8_t> hasAggFilters;
    int32_t aggFiltersCount = 0;
    OperatorConfig operatorConfig;

    bool isInited = false;
    bool hasGroupStarted = false;
    struct PendingOutputGroup {
        std::vector<uint8_t> keyData;
        std::vector<bool> keyNulls;
        std::vector<uint32_t> stringLengths;
        AggregateState *state = nullptr;
    };

    std::vector<uint8_t> cachedKeyData;
    std::vector<bool> cachedKeyNulls;
    std::vector<uint32_t> cachedKeyStringLengths;
    std::vector<type::DataTypePtr> groupKeyTypes;
    AggregateState *currentGroupState = nullptr;
    int32_t totalAggStatesSize = 0;

    std::vector<PendingOutputGroup> pendingOutputGroups;
    std::vector<type::DataTypePtr> outputTypes;
    int32_t outputCursor = 0;
};

class SortAggregationOperatorFactory : public AggregationCommonOperatorFactory {
public:
    SortAggregationOperatorFactory(std::vector<uint32_t> &groupByCol, const DataTypes &groupInputTypes,
        std::vector<std::vector<uint32_t>> &aggsCols, std::vector<DataTypes> &aggInputTypes,
        std::vector<DataTypes> &aggOutputTypes, std::vector<uint32_t> &aggFuncTypes,
        std::vector<uint32_t> &maskColsVector, std::vector<bool> inputRaws, std::vector<bool> outputPartials,
        const std::vector<int8_t> &hasAggFilters, const OperatorConfig &operatorConfig, AggregationNode::Step step)
        : AggregationCommonOperatorFactory(inputRaws, outputPartials, maskColsVector,
            operatorConfig.GetOverflowConfig()->IsOverflowAsNull(), operatorConfig.IsStatisticalAggregate()),
          groupByColsVector(groupByCol),
          groupByTypes(groupInputTypes),
          aggsInputColsVector(aggsCols),
          aggInputTypes(aggInputTypes),
          aggOutputTypes(aggOutputTypes),
          aggFuncTypesVector(aggFuncTypes),
          hasAggFilters(hasAggFilters),
          operatorConfig(operatorConfig),
          step(step)
    {}

    ~SortAggregationOperatorFactory() override = default;

    Operator *CreateOperator() override;

    OmniStatus Init() override;

    OmniStatus Close() override;

private:
    std::vector<uint32_t> groupByColsVector;
    std::vector<int32_t> groupByColIndices;
    DataTypes groupByTypes;
    std::vector<std::vector<uint32_t>> aggsInputColsVector;
    std::vector<std::vector<int32_t>> aggsInputCols;
    std::vector<DataTypes> aggInputTypes;
    std::vector<DataTypes> aggOutputTypes;
    std::vector<uint32_t> aggFuncTypesVector;
    std::vector<std::unique_ptr<AggregatorFactory>> aggregatorFactories;
    std::vector<int8_t> hasAggFilters;
    OperatorConfig operatorConfig;
    AggregationNode::Step step;
};
} // namespace omniruntime::op
#endif
