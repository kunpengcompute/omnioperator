/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2023. All rights reserved.
 * Description: Aggregation Base Class
 */
#ifndef AGGREGATION_H
#define AGGREGATION_H

#include <vector>
#include <thread>
#include "operator/operator_factory.h"
#include "operator/aggregation/aggregator/only_aggregator_factory.h"
#include "memory/memory_pool.h"
#include "operator/status.h"

namespace omniruntime {
namespace op {
class AggregationCommonOperator : public Operator {
public:
    explicit AggregationCommonOperator(std::vector<std::unique_ptr<Aggregator>> &&aggs, std::vector<bool> &inputRaws,
        std::vector<bool> &outputPartials, bool isOverflowAsNull = false)
        : inputRaws(inputRaws), outputPartials(outputPartials), isOverflowAsNull(isOverflowAsNull)
    {
        for (auto &agg : aggs) {
            agg->SetExecutionContext(executionContext.get());
        }
        this->aggregators = std::move(aggs);
    }

    ~AggregationCommonOperator() override {};

protected:
    std::vector<std::unique_ptr<Aggregator>> aggregators;
    std::vector<bool> inputRaws;
    std::vector<bool> outputPartials;
    bool isOverflowAsNull;
};

class AggregationCommonOperatorFactory : public OperatorFactory {
public:
    AggregationCommonOperatorFactory(std::vector<bool> &inputRaws, std::vector<bool> &outputPartials,
        std::vector<uint32_t> &maskColsContext, bool isOverflowAsNull = false, bool isStatisticalAggregate = false)
        : inputRaws(inputRaws),
          outputPartials(outputPartials),
          isOverflowAsNull(isOverflowAsNull),
          isStatisticalAggregate(isStatisticalAggregate)
    {
        for (size_t i = 0; i < maskColsContext.size(); ++i) {
            maskCols.push_back(maskColsContext[i]);
        }
    }

    ~AggregationCommonOperatorFactory() override {};

    std::vector<int32_t> &GetMaskColumns()
    {
        return maskCols;
    }

    virtual OmniStatus Init() = 0;
    virtual OmniStatus Close() = 0;

    template <class T>
    void CreateAggregatorFactory(std::vector<std::unique_ptr<AggregatorFactory>> &aggregatorFactories, int32_t maskCol);

    OmniStatus CreateAggregatorFactories(std::vector<std::unique_ptr<AggregatorFactory>> &aggregatorFactories,
        const std::vector<uint32_t> &funcTypesContext, const std::vector<int32_t> &maskCols,
        const std::vector<std::string> &udafNames = {});

protected:
    std::vector<bool> inputRaws;
    std::vector<bool> outputPartials;
    std::vector<int32_t> maskCols;
    bool isOverflowAsNull;
    bool isStatisticalAggregate;
};
}
}
#endif