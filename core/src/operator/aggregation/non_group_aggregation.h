/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
 * Description: Aggregation Header
 */
#ifndef NON_GROUP_AGGREGATION_H
#define NON_GROUP_AGGREGATION_H

#include "aggregation.h"

#include <utility>
#include "type/data_types.h"

namespace omniruntime {
namespace op {
class AggregationOperator : public AggregationCommonOperator {
public:
    AggregationOperator(std::vector<std::unique_ptr<Aggregator>> aggs,
        std::vector<omniruntime::type::DataTypes> &aggsOutputTypes, std::vector<bool> &inputRaws,
        std::vector<bool> &outputPartials, const std::vector<int8_t> &hasAggFilters)
        : AggregationCommonOperator(std::move(aggs), inputRaws, outputPartials),
          aggsOutputTypes(aggsOutputTypes),
          hasAggFilters(hasAggFilters)
    {
        CalcAndSetStatesSize();
        for (uint32_t i = 0; i < aggregators.size(); i++) {
            aggregators[i]->InitState(aggsStates.get());
        }
        for (auto hasFilter : hasAggFilters) {
            if (hasFilter == 1) {
                aggFiltersCount++;
            }
        }
    }

    ~AggregationOperator() override = default;

    int32_t AddInput(omniruntime::vec::VectorBatch *vecBatch) override;

    int32_t GetOutput(omniruntime::vec::VectorBatch **outputVecBatch) override;

private:
    std::vector<omniruntime::type::DataTypes> aggsOutputTypes;
    std::unique_ptr<AggregateState[]> aggsStates;
    std::vector<int8_t> hasAggFilters;
    int32_t aggFiltersCount = 0;
    int32_t totalAggStatesSize = 0;

    void CalcAndSetStatesSize()
    {
        totalAggStatesSize = 0;
        for (auto &agg : aggregators) {
            agg->SetStateOffset(totalAggStatesSize);
            totalAggStatesSize += agg->GetStateSize();
        }
        aggsStates = std::make_unique<AggregateState[]>(totalAggStatesSize);
    }
};

class AggregationOperatorFactory : public AggregationCommonOperatorFactory {
public:
    AggregationOperatorFactory(omniruntime::type::DataTypes &sourceTypes, std::vector<uint32_t> &aggFuncTypesVector,
        std::vector<std::vector<uint32_t>> &aggsInputColsVector, std::vector<uint32_t> &maskColsVector,
        std::vector<omniruntime::type::DataTypes> &aggsOutputTypes, std::vector<bool> inputRaws,
        std::vector<bool> outputPartials, bool overflowAsNull = false, bool isStatisticalAggregate = false,
        const std::vector<std::string> &aggUdafNames = {})
        : AggregationCommonOperatorFactory(inputRaws, outputPartials, maskColsVector, overflowAsNull,
        isStatisticalAggregate),
          sourceTypes(sourceTypes),
          aggFuncTypesVector(aggFuncTypesVector),
          aggsInputColsVector(aggsInputColsVector),
          aggsOutputTypes(aggsOutputTypes),
          aggUdafNamesVector(aggUdafNames)
    {}

    // this is for AggregationWithExprOperatorFactory
    AggregationOperatorFactory(omniruntime::type::DataTypes &sourceTypes, std::vector<uint32_t> &aggFuncTypesVector,
        std::vector<std::vector<uint32_t>> &aggsInputColsVector, std::vector<uint32_t> &maskColsVector,
        std::vector<omniruntime::type::DataTypes> &aggsOutputTypes, std::vector<bool> inputRaws,
        std::vector<bool> outputPartials, const std::vector<int8_t> &hasAggFilters, bool overflowAsNull = false,
        bool isStatisticalAggregate = false, const std::vector<std::string> &aggUdafNames = {})
        : AggregationCommonOperatorFactory(inputRaws, outputPartials, maskColsVector, overflowAsNull,
        isStatisticalAggregate),
          sourceTypes(sourceTypes),
          aggFuncTypesVector(aggFuncTypesVector),
          aggsInputColsVector(aggsInputColsVector),
          aggsOutputTypes(aggsOutputTypes),
          hasAggFilters(hasAggFilters),
          aggUdafNamesVector(aggUdafNames)
    {}

    ~AggregationOperatorFactory() override = default;

    Operator *CreateOperator() override;

    OmniStatus Init() override;

    OmniStatus Close() override;

private:
    omniruntime::type::DataTypes sourceTypes;
    std::vector<uint32_t> aggFuncTypesVector;
    std::vector<std::vector<uint32_t>> aggsInputColsVector;
    std::vector<omniruntime::type::DataTypes> aggsOutputTypes;
    std::vector<omniruntime::type::DataTypesPtr> aggsInputTypes;
    std::vector<std::vector<int32_t>> aggsInputCols;
    std::vector<std::unique_ptr<AggregatorFactory>> aggregatorFactories;
    std::vector<int8_t> hasAggFilters;
    std::vector<std::string> aggUdafNamesVector;
};
} // end op
} // edn omniruntime

#endif