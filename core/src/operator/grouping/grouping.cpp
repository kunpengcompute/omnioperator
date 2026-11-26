/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
 * KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 */

#include "grouping.h"
#include "operator/aggregation/group_aggregation_expr.h"

namespace omniruntime::op {
Operator *GroupingOperatorFactory::CreateOperator() { return new GroupingOperator(groupingNode_, queryConfig_); }

GroupingOperatorFactory *
GroupingOperatorFactory::CreateGroupingOperatorFactory(const std::shared_ptr<const GroupingNode> &groupingNode,
                                                       const config::QueryConfig &queryConfig)
{
    return new GroupingOperatorFactory(groupingNode, queryConfig);
}

static std::vector<ExprPtr> BuildAggregateExpressions(const std::shared_ptr<const AggregationNode> &aggPlanNode,
                                                      const std::vector<ExprPtr> &projection)
{
    auto size = aggPlanNode->OutputType()->GetSize();
    auto groupByKeySize = aggPlanNode->GetGroupByKeys().size();
    std::vector<ExprPtr> expressions(size);

    for (int32_t i = 0; i < size; i++) {
        // groupByKey field is on the head of agg output, followed by aggFunc field
        // aggFunc field need to get aggOperator0 output
        if (i >= groupByKeySize) {
            expressions[i] = new FieldExpr(i, aggPlanNode->OutputType()->GetType(i));
            continue;
        }

        // null or groupingId is LiteralExpr, need to get from expand project
        auto fieldExpr = dynamic_cast<FieldExpr *>(aggPlanNode->GetGroupByKeys()[i]);
        if (!fieldExpr) {
            expressions[i] = new FieldExpr(i, aggPlanNode->OutputType()->GetType(i));
            continue;
        }

        auto literalExpr = dynamic_cast<LiteralExpr *>(projection[fieldExpr->colVal]);
        if (!literalExpr) {
            expressions[i] = new FieldExpr(i, aggPlanNode->OutputType()->GetType(i));
            continue;
        }

        expressions[i] = literalExpr->Copy();
    }

    return expressions;
}

GroupingOperator::GroupingOperator(const std::shared_ptr<const GroupingNode> &groupingNode,
                                   config::QueryConfig &queryConfig)
    : queryConfig_(queryConfig)
{
    auto expandNode = groupingNode->GetExpandPlanNode();
    aggPlanNode_ = groupingNode->GetAggregationNode();
    auto projections = expandNode->GetProjections();
    auto sourceTypes = *(expandNode->InputType());
    auto residualOutputType = aggPlanNode_->OutputType();
    auto overflowConfig = queryConfig_.IsOverFlowASNull() == true
                              ? std::make_shared<OverflowConfig>(OVERFLOW_CONFIG_NULL)
                              : std::make_shared<OverflowConfig>(OVERFLOW_CONFIG_EXCEPTION);
    aggFactor_ = CreateOperatorFactory(aggPlanNode_, queryConfig_);
    int32_t index = 0;
    aggOperators_.resize(projections.size());
    residualAggFactor_ =
        CreateResidualOperatorFactory(aggPlanNode_, residualOutputType, aggPlanNode_->GetGroupByNum(), queryConfig_);
    for (const auto &projection : projections) {
        if (index == 0) {
            auto exprEvaluator = std::make_shared<ExpressionEvaluator>(projection, sourceTypes, overflowConfig.get());
            exprEvaluator->ProjectFuncGeneration();
            expressionEvaluators_.push_back(exprEvaluator);
            aggOperators_[index] = std::shared_ptr<Operator>(aggFactor_->CreateOperator());
        } else {
            std::vector<ExprPtr> expressions = BuildAggregateExpressions(aggPlanNode_, projection);
            auto exprEvaluator =
                std::make_shared<ExpressionEvaluator>(expressions, *residualOutputType.get(), overflowConfig.get());
            exprEvaluator->ProjectFuncGeneration();
            expressionEvaluators_.push_back(exprEvaluator);
            aggOperators_[index] = std::shared_ptr<Operator>(residualAggFactor_->CreateOperator());
            for (const auto p : projection) {
                delete p;
            }
        }
        index++;
    }
}

int32_t GroupingOperator::AddInput(VectorBatch *vecBatch)
{
    if (vecBatch->GetRowCount() <= 0) {
        VectorHelper::FreeVecBatch(vecBatch);
        ResetInputVecBatch();
        return 0;
    }
    hasInput = true;
    auto projectedVecBatch = this->expressionEvaluators_[0]->Evaluate(vecBatch, executionContext.get());
    aggOperators_[0]->AddInput(projectedVecBatch);
    VectorHelper::FreeVecBatch(vecBatch);
    return 0;
}

int32_t GroupingOperator::GetOutput(VectorBatch **outputVecBatch)
{
    if (!noMoreInput_) {
        SetStatus(OMNI_STATUS_NORMAL);
        return 0;
    }

    if (!hasInput) {
        SetStatus(OMNI_STATUS_FINISHED);
        return 0;
    }

    const auto size = expressionEvaluators_.size();
    while (index_ < size) {
        aggOperators_[index_ - 1]->GetOutput(outputVecBatch);
        if (*outputVecBatch != nullptr) {
            auto projectedVecBatch =
                this->expressionEvaluators_[index_]->Evaluate(*outputVecBatch, executionContext.get());
            aggOperators_[index_]->AddInput(projectedVecBatch);
            return 0;
        }
        index_++;
    }
    aggOperators_[index_ - 1]->GetOutput(outputVecBatch);
    if (*outputVecBatch != nullptr) {
        return 0;
    }
    SetStatus(OMNI_STATUS_FINISHED);
    return 0;
}

std::shared_ptr<OperatorFactory>
GroupingOperator::CreateOperatorFactory(const std::shared_ptr<const AggregationNode> &aggregationNode,
                                        const config::QueryConfig &queryConfig)
{
    if (aggregationNode->GetGroupByKeys().empty()) {
        return std::shared_ptr<AggregationWithExprOperatorFactory>(
            AggregationWithExprOperatorFactory::CreateAggregationWithExprOperatorFactory(aggregationNode, queryConfig));
    }
    return std::shared_ptr<HashAggregationWithExprOperatorFactory>(
        HashAggregationWithExprOperatorFactory::CreateAggregationWithExprOperatorFactory(aggregationNode, queryConfig));
}

std::shared_ptr<OperatorFactory>
GroupingOperator::CreateResidualOperatorFactory(const std::shared_ptr<const AggregationNode> &aggregationNode,
                                                const DataTypesPtr &sourceTypes, int rollUpSize,
                                                const config::QueryConfig &queryConfig)
{
    auto aggsOutputTypes = aggregationNode->GetAggsOutputTypes();
    int aggsOutputTypeSize = 0;
    for (const auto &aggsOutputType : aggsOutputTypes) {
        aggsOutputTypeSize += aggsOutputType.GetSize();
    }
    auto groupBySize = aggregationNode->GetGroupByKeys().size();
    groupByKeys_.resize(groupBySize);
    std::vector<bool> inputRaws(aggregationNode->GetInputRaws().size(), false);
    for (int32_t i = 0; i < groupBySize; i++) {
        groupByKeys_[i] = new FieldExpr(i, sourceTypes->GetType(i));
    }

    int aggIndex = aggregationNode->OutputType()->GetSize() - aggsOutputTypeSize;
    for (const auto &aggsOutputType : aggsOutputTypes) {
        std::vector<ExprPtr> tmpExprs;
        for (int32_t i = 0; i < aggsOutputType.GetSize(); i++) {
            tmpExprs.push_back(new FieldExpr(aggIndex, sourceTypes->GetType(aggIndex)));
            aggIndex++;
        }
        aggsKeys_.push_back(tmpExprs);
    }

    auto residualPlanNoded = std::make_shared<const AggregationNode>(
        aggregationNode->Id(), groupByKeys_, aggregationNode->GetGroupByNum(), aggsKeys_, aggregationNode->OutputType(),
        aggregationNode->GetAggsOutputTypes(), aggregationNode->GetAggFuncTypes(), aggregationNode->GetAggFilters(),
        aggregationNode->GetMaskColumns(), inputRaws, aggregationNode->GetOutputPartials(),
        aggregationNode->GetIsStatisticalAggregate(), aggregationNode->OutputType(), aggregationNode->Sources()[0]);
    if (aggregationNode->GetGroupByKeys().empty()) {
        return std::shared_ptr<AggregationWithExprOperatorFactory>(
            AggregationWithExprOperatorFactory::CreateAggregationWithExprOperatorFactory(residualPlanNoded,
                                                                                         queryConfig));
    }
    return std::shared_ptr<HashAggregationWithExprOperatorFactory>(
        HashAggregationWithExprOperatorFactory::CreateAggregationWithExprOperatorFactory(residualPlanNoded,
                                                                                         queryConfig));
}

OmniStatus GroupingOperator::Close()
{
    for (auto aggOperator : aggOperators_) {
        aggOperator->Close();
    }
    return OMNI_STATUS_FINISHED;
}
} // namespace omniruntime::op
