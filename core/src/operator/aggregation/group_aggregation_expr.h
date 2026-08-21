/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2021-2024. All rights reserved.
 * @Description: Hash Aggregation WithExpr Header
 */

#ifndef GROUP_AGGREGATION_EXPR_H
#define GROUP_AGGREGATION_EXPR_H

#include "operator/operator_factory.h"
#include "operator/projection/projection.h"
#include "operator/aggregation/group_aggregation.h"
#include "type/data_types.h"
#include "one_row_adaptor.h"

namespace omniruntime {
namespace op {
class HashAggregationWithExprOperatorFactory : public OperatorFactory {
public:
    HashAggregationWithExprOperatorFactory(std::vector<omniruntime::expressions::Expr *> &groupByKeys,
        uint32_t groupByNum, std::vector<std::vector<omniruntime::expressions::Expr *>> &aggsKeys,
        std::vector<omniruntime::expressions::Expr *> &aggFilters, DataTypes &sourceDataTypes,
        std::vector<DataTypes> &aggOutputTypes, std::vector<uint32_t> &aggFuncTypes, std::vector<uint32_t> &maskColumns,
        std::vector<bool> &inputRaws, std::vector<bool> &outputPartial, const OperatorConfig &operatorConfig,
        config::QueryConfig queryConfig = config::QueryConfig(), AggregationNode::Step step = AggregationNode::Step::K_SINGLE,
        bool mixedInputExpected = false, bool mixedOutputEnabled = false);

    ~HashAggregationWithExprOperatorFactory() override;

    static HashAggregationWithExprOperatorFactory *CreateAggregationWithExprOperatorFactory(
        const std::shared_ptr<const AggregationNode> &planNode, const config::QueryConfig &queryConfig);

    Operator *CreateOperator() override;

private:
    // originalSourceTypes is used to store raw input type which is not handled by projection function
    std::unique_ptr<DataTypes> originSourceTypes;
    // sourceTypes is used to store input type which has been handled by projection function
    std::unique_ptr<DataTypes> sourceTypes;
    std::unique_ptr<DataTypes> groupByTypes;
    std::vector<std::unique_ptr<DataTypes>> aggTypes;
    std::vector<std::unique_ptr<Projection>> projections;
    int32_t aggFilterNum;
    std::vector<SimpleFilter *> aggSimpleFilters;
    HashAggregationOperatorFactory *hashAggOperatorFactory;
    AggregationNode::Step step;
};

class HashAggregationWithExprOperator : public Operator {
public:
    HashAggregationWithExprOperator(const DataTypes &originSourceTypes, const type::DataTypes &sourceTypes,
        std::vector<std::unique_ptr<Projection>> &projections, std::vector<SimpleFilter *> &aggSimpleFilters,
        HashAggregationOperator *hashAggOperator, config::QueryConfig queryConfig = config::QueryConfig());

    ~HashAggregationWithExprOperator() override;

    int32_t AddInput(omniruntime::vec::VectorBatch *inputVecBatch) override;

    int32_t GetOutput(omniruntime::vec::VectorBatch **outputVecBatch) override;

    OmniStatus Close() override;

    void ProcessRow(uintptr_t rowValues[], int32_t lens[]);

    OmniStatus Init();

    uint64_t GetSpilledBytes() override;

    uint64_t GetUsedMemBytes() override;

    uint64_t GetTotalMemBytes() override;

    std::vector<uint64_t> GetSpecialMetricsInfo() override;

    uint64_t GetHashMapUniqueKeys() override;

    VectorBatch *AlignSchema(VectorBatch *inputVecBatch) override;

    bool IsStepPartials() const
    {
        return hashAggOperator->IsStepPartials();
    }

private:
    OneRowAdaptor oneRowAdaptor;
    DataTypes originTypes;
    DataTypes sourceTypes;
    std::vector<std::unique_ptr<Projection>> &projections;
    std::vector<SimpleFilter *> aggSimpleFilters;
    HashAggregationOperator *hashAggOperator;
    bool adaptivePartialAgg = true;
    bool hasAggFilter = false;
    bool abandonedPartialAggregation = false;
    long adaptivePartialAggMinRows = 500000;
    long numInputRows = 0;
    double adaptivePartialAggRatio = 0.8;
    bool isActivateAdaptivePartial = false;
    VectorBatch * abandonedInputVecBatch = nullptr;
};
}
}
#endif // GROUP_AGGREGATION_EXPR_H
