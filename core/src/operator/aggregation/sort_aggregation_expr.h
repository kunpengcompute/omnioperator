/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2021-2024. All rights reserved.
 * @Description: Sort Aggregation WithExpr Header
 */

#ifndef SORT_AGGREGATION_EXPR_H
#define SORT_AGGREGATION_EXPR_H

#include "operator/operator_factory.h"
#include "operator/projection/projection.h"
#include "operator/aggregation/sort_aggregation.h"
#include "type/data_types.h"
#include "one_row_adaptor.h"

namespace omniruntime {
namespace op {
class SortAggregationWithExprOperatorFactory : public OperatorFactory {
public:
    SortAggregationWithExprOperatorFactory(std::vector<omniruntime::expressions::Expr *> &groupByKeys,
        uint32_t groupByNum, std::vector<std::vector<omniruntime::expressions::Expr *>> &aggsKeys,
        std::vector<omniruntime::expressions::Expr *> &aggFilters, DataTypes &sourceDataTypes,
        std::vector<DataTypes> &aggOutputTypes, std::vector<uint32_t> &aggFuncTypes, std::vector<uint32_t> &maskColumns,
        std::vector<bool> &inputRaws, std::vector<bool> &outputPartial, const OperatorConfig &operatorConfig,
        config::QueryConfig queryConfig = config::QueryConfig(), AggregationNode::Step step = AggregationNode::Step::K_SINGLE);

    ~SortAggregationWithExprOperatorFactory() override;

    static SortAggregationWithExprOperatorFactory *CreateSortAggregationWithExprOperatorFactory(
        const std::shared_ptr<const AggregationNode> &planNode, const config::QueryConfig &queryConfig);

    Operator *CreateOperator() override;

private:
    std::unique_ptr<DataTypes> originSourceTypes;
    std::unique_ptr<DataTypes> sourceTypes;
    std::unique_ptr<DataTypes> groupByTypes;
    std::vector<std::unique_ptr<DataTypes>> aggTypes;
    std::vector<std::unique_ptr<Projection>> projections;
    int32_t aggFilterNum;
    std::vector<SimpleFilter *> aggSimpleFilters;
    SortAggregationOperatorFactory *sortAggOperatorFactory;
    AggregationNode::Step step;
    config::QueryConfig queryConfig_;
};

class SortAggregationWithExprOperator : public Operator {
public:
    SortAggregationWithExprOperator(const DataTypes &originSourceTypes, const type::DataTypes &sourceTypes,
        std::vector<std::unique_ptr<Projection>> &projections, std::vector<SimpleFilter *> &aggSimpleFilters,
        SortAggregationOperator *sortAggOperator, config::QueryConfig queryConfig = config::QueryConfig());

    ~SortAggregationWithExprOperator() override;

    int32_t AddInput(omniruntime::vec::VectorBatch *inputVecBatch) override;

    int32_t GetOutput(omniruntime::vec::VectorBatch **outputVecBatch) override;

    void ProcessRow(uintptr_t rowValues[], int32_t lens[]);

    OmniStatus Close() override;

    OmniStatus Init();

    uint64_t GetSpilledBytes() override;
    uint64_t GetUsedMemBytes() override;
    uint64_t GetTotalMemBytes() override;
    std::vector<uint64_t> GetSpecialMetricsInfo() override;
    uint64_t GetHashMapUniqueKeys() override;
    VectorBatch *AlignSchema(VectorBatch *inputVecBatch) override;

private:
    OneRowAdaptor oneRowAdaptor;
    DataTypes originTypes;
    DataTypes sourceTypes;
    std::vector<std::unique_ptr<Projection>> &projections;
    std::vector<SimpleFilter *> aggSimpleFilters;
    SortAggregationOperator *sortAggOperator;
    bool hasAggFilter = false;
};
}
}
#endif // SORT_AGGREGATION_EXPR_H
