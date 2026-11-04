/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#pragma once
#include "plannode/planNode.h"
#include "operator/operator.h"
#include "operator/operator_factory.h"
#include "operator/expand/expand.h"
#include "operator/aggregation/non_group_aggregation_expr.h"

namespace omniruntime::op {
class GroupingOperatorFactory final : public OperatorFactory {
public:
    GroupingOperatorFactory(const std::shared_ptr<const GroupingNode> &groupingNode,
        const config::QueryConfig &queryConfig): groupingNode_(groupingNode)
    {
        this->queryConfig_ = queryConfig;
    }

    using OperatorFactory::CreateOperator;
    // Pool-bound override
    Operator *CreateOperator(omniruntime::mem::OmniTaskMemoryPool *pool) override;

    static GroupingOperatorFactory *CreateGroupingOperatorFactory(
        const std::shared_ptr<const GroupingNode> &aggPlanNode, const config::QueryConfig &queryConfig);

private:
    std::shared_ptr<const GroupingNode> groupingNode_;
};

class GroupingOperator final : public Operator {
public:
    // Pool-bound ctor chains Operator(pool)
    GroupingOperator(omniruntime::mem::OmniTaskMemoryPool *pool,
        const std::shared_ptr<const GroupingNode> &groupingNode, config::QueryConfig &queryConfig);

    ~GroupingOperator() override
    {
        for (auto expression : groupByKeys_) {
            delete expression;
        }
        for (auto expressions : aggsKeys_) {
            for (auto expression : expressions) {
                delete expression;
            }
        }
    }

    int32_t AddInput(VectorBatch *vecBatch) override;

    int32_t GetOutput(VectorBatch **resultVecBatch) override;
    /// False while any chained aggregation refuses input (memory-driven
    /// partial flush pending): the driver then drains this operator instead
    /// of feeding it, and GetOutput moves the flushed batches down the chain.
    bool needsInput() override;

    void setNoMoreInput(bool noMoreInput) override;

    void noMoreInput() override;

    OmniStatus Close() override;

    static std::shared_ptr<OperatorFactory> CreateOperatorFactory(
        const std::shared_ptr<const AggregationNode> &aggregationNode, const config::QueryConfig &queryConfig);

    std::shared_ptr<OperatorFactory> GroupingOperator::CreateResidualOperatorFactory(
        const std::shared_ptr<const AggregationNode> &aggregationNode, const DataTypesPtr &sourceTypes, int rollUpSize,
        const config::QueryConfig &queryConfig);

private:
    // Shared init body so both legacy and pool-bound ctors call it after chaining
    //         the appropriate Operator base ctor.
    void InitFromGroupingNode(const std::shared_ptr<const GroupingNode> &groupingNode);
    int32_t PumpPartialFlush(VectorBatch **resultVecBatch);

    uint32_t index_ = 1;
    std::shared_ptr<OperatorFactory> aggFactor_;
    std::shared_ptr<OperatorFactory> residualAggFactor_;
    std::vector<std::shared_ptr<ExpressionEvaluator>> expressionEvaluators_;
    std::shared_ptr<AggregationWithExprOperator> aggregationWithExprOperator_;
    std::shared_ptr<const AggregationNode> aggPlanNode_;
    std::shared_ptr<ExpandOperator> expandOperator_;
    std::shared_ptr<Operator> aggOperator_;
    std::vector<std::shared_ptr<Operator>> aggOperators_;
    bool hasInput = false;
    config::QueryConfig queryConfig_;

    std::vector<ExprPtr> groupByKeys_;
    std::vector<std::vector<ExprPtr>> aggsKeys_;
};
}
