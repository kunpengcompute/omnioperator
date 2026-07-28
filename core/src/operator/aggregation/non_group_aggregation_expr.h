/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
 * @Description: Hash Aggregation WithExpr Header
 */

#ifndef NON_GROUP_AGGREGATION_EXPR_H
#define NON_GROUP_AGGREGATION_EXPR_H

#include "operator/operator_factory.h"
#include "operator/filter/filter_and_project.h"
#include "operator/aggregation/non_group_aggregation.h"
#include "operator/projection/projection.h"
#include "type/data_types.h"

namespace omniruntime {
namespace op {
class AggregationWithExprOperatorFactory : public OperatorFactory {
public:
    AggregationWithExprOperatorFactory(std::vector<omniruntime::expressions::Expr *> &groupByKeys, uint32_t groupByNum,
        std::vector<std::vector<omniruntime::expressions::Expr *>> &aggsKeys, DataTypes &sourceDataTypes,
        std::vector<DataTypes> &aggOutputTypes, std::vector<uint32_t> &aggFuncTypes,
        std::vector<omniruntime::expressions::Expr *> &aggFilterExprs, std::vector<uint32_t> &maskColumns,
        std::vector<bool> &inputRaws, std::vector<bool> &outputPartial, OverflowConfig *overflowConfig,
        bool isStatisticalAggregate = false, const std::vector<std::string> &aggUdafNames = {});

    AggregationWithExprOperatorFactory(std::vector<omniruntime::expressions::Expr *> &groupByKeys, uint32_t groupByNum,
        std::vector<std::vector<omniruntime::expressions::Expr *>> &aggsKeys, DataTypes &sourceDataTypes,
        std::vector<DataTypes> &aggOutputTypes, std::vector<uint32_t> &aggFuncTypes,
        std::vector<omniruntime::expressions::Expr *> &aggFilterExprs, std::vector<uint32_t> &maskColumns,
        std::vector<bool> &inputRaws, std::vector<bool> &outputPartial, OverflowConfig *overflowConfig,
        const config::QueryConfig &queryConfig, bool isStatisticalAggregate = false,
        const std::vector<std::string> &aggUdafNames = {});

    ~AggregationWithExprOperatorFactory() override;

    static AggregationWithExprOperatorFactory *CreateAggregationWithExprOperatorFactory(
        const std::shared_ptr<const AggregationNode> &planNode, const config::QueryConfig &queryConfig);

    Operator *CreateOperator() override;

private:
    std::unique_ptr<DataTypes> originSourceTypes;
    std::unique_ptr<DataTypes> sourceTypes;
    std::vector<int32_t> projectCols;
    std::vector<std::unique_ptr<Projection>> projections;
    std::vector<ProjFunc> projectFuncs;
    int32_t aggFilterNum;
    std::vector<SimpleFilter *> aggSimpleFilters;
    AggregationOperatorFactory *aggOperatorFactory;
};

class AggregationWithExprOperator : public Operator {
public:
    AggregationWithExprOperator(const DataTypes &originSourceTypes, const type::DataTypes &sourceTypes,
        std::vector<std::unique_ptr<Projection>> &projections, std::vector<SimpleFilter *> &aggSimpleFilters,
        AggregationOperator *AggOperator, const config::QueryConfig &queryConfig);

    ~AggregationWithExprOperator() override;

    int32_t AddInput(omniruntime::vec::VectorBatch *inputVecBatch) override;

    int32_t GetOutput(omniruntime::vec::VectorBatch **outputVecBatch) override;

    OmniStatus Close() override;

    AggregationOperator *GetAggregationOperator() const { return aggOperator; }

private:
    DataTypes originTypes;
    DataTypes sourceTypes;
    std::vector<std::unique_ptr<Projection>> &projections;
    std::vector<SimpleFilter *> aggSimpleFilters;
    AggregationOperator *aggOperator;
    bool hasAggFilter = false;
};
}
}
#endif // NON_GROUP_AGGREGATION_EXPR_H
