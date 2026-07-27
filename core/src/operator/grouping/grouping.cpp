/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#include "grouping.h"
#include "operator/aggregation/group_aggregation_expr.h"
#include <algorithm>

namespace omniruntime::op {
Operator *GroupingOperatorFactory::CreateOperator()
{
    return new GroupingOperator(groupingNode_, queryConfig_);
}

GroupingOperatorFactory *GroupingOperatorFactory::CreateGroupingOperatorFactory(
    const std::shared_ptr<const GroupingNode> &groupingNode, const config::QueryConfig &queryConfig)
{
    return new GroupingOperatorFactory(groupingNode, queryConfig);
}

GroupingOperator::GroupingOperator(const std::shared_ptr<const GroupingNode> &groupingNode,
    config::QueryConfig &queryConfig): queryConfig_(queryConfig)
{
    auto expandNode = groupingNode->GetExpandPlanNode();
    aggPlanNode_ = groupingNode->GetAggregationNode();
    auto projections = expandNode->GetProjections();
    auto sourceTypes = *(expandNode->InputType());
    auto residualOutputType = aggPlanNode_->OutputType();
    aggFactor_ = CreateOperatorFactory(aggPlanNode_, queryConfig_);
    int32_t index = 0;
    const auto &groupByKeys = aggPlanNode_->GetGroupByKeys();
    const auto groupingKeySize = static_cast<int32_t>(groupByKeys.size());
    std::vector<int32_t> groupBySourceCols(groupingKeySize, -1);
    for (int32_t i = 0; i < groupingKeySize; ++i) {
        auto fieldExpr = dynamic_cast<FieldExpr *>(groupByKeys[i]);
        if (fieldExpr != nullptr) {
            groupBySourceCols[i] = fieldExpr->colVal;
        }
    }

    auto isGroupingIdSourceCol = [&](int32_t sourceCol) -> bool {
        if (projections.empty() || sourceCol < 0 || sourceCol >= static_cast<int32_t>(projections[0].size())) {
            return false;
        }
        std::vector<int64_t> gids;
        gids.reserve(projections.size());
        for (const auto &projection : projections) {
            if (sourceCol >= static_cast<int32_t>(projection.size())) {
                return false;
            }
            auto literal = dynamic_cast<LiteralExpr *>(projection[sourceCol]);
            if (literal == nullptr || literal->isNull) {
                return false;
            }
            if (literal->GetReturnTypeId() != OMNI_INT && literal->GetReturnTypeId() != OMNI_LONG) {
                return false;
            }
            int64_t gid = literal->GetReturnTypeId() == OMNI_INT ? literal->intVal : literal->longVal;
            if (std::find(gids.begin(), gids.end(), gid) != gids.end()) {
                return false;
            }
            gids.push_back(gid);
        }
        return true;
    };

    int32_t groupingIdKeyIdx = -1;
    for (int32_t i = 0; i < groupingKeySize; ++i) {
        if (isGroupingIdSourceCol(groupBySourceCols[i])) {
            groupingIdKeyIdx = i;
            break;
        }
    }

    aggOperators_.resize(projections.size());
    residualAggFactor_ = CreateResidualOperatorFactory(aggPlanNode_, residualOutputType, groupingKeySize, queryConfig_);
    for (const auto &projection : projections) {
        if (index == 0) {
            auto exprEvaluator = std::make_shared<ExpressionEvaluator>(projection, sourceTypes, queryConfig);
            exprEvaluator->ProjectFuncGeneration();
            expressionEvaluators_.push_back(exprEvaluator);
            aggOperators_[index] = std::shared_ptr<Operator>(aggFactor_->CreateOperator());
        } else {
            auto size = aggPlanNode_->OutputType()->GetSize();
            std::vector<ExprPtr> expressions(size);
            for (unsigned int i = 0; i < size; i++) {
                if (i < static_cast<unsigned int>(groupingKeySize)) {
                    int32_t sourceCol = groupBySourceCols[i];
                    Expr *sourceExpr = nullptr;
                    if (sourceCol >= 0 && sourceCol < static_cast<int32_t>(projection.size())) {
                        sourceExpr = projection[sourceCol];
                    }
                    auto literal = dynamic_cast<LiteralExpr *>(sourceExpr);
                    if (static_cast<int32_t>(i) == groupingIdKeyIdx && literal != nullptr) {
                        if (literal->GetReturnTypeId() == OMNI_INT) {
                            expressions[i] = new LiteralExpr(literal->intVal, literal->dataType, literal->isNull);
                        } else if (literal->GetReturnTypeId() == OMNI_LONG) {
                            expressions[i] = new LiteralExpr(literal->longVal, literal->dataType, literal->isNull);
                        } else {
                            expressions[i] = new FieldExpr(i, aggPlanNode_->OutputType()->GetType(i));
                        }
                    } else if (literal != nullptr && literal->isNull) {
                        expressions[i] = new LiteralExpr(0, aggPlanNode_->OutputType()->GetType(i), true);
                    } else {
                        expressions[i] = new FieldExpr(i, aggPlanNode_->OutputType()->GetType(i));
                    }
                    continue;
                }
                expressions[i] = new FieldExpr(i, aggPlanNode_->OutputType()->GetType(i));
            }
            auto exprEvaluator = std::make_shared<ExpressionEvaluator>(expressions, *residualOutputType.get(),
                queryConfig);
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
            auto projectedVecBatch = this->expressionEvaluators_[index_]->Evaluate(*outputVecBatch,
                executionContext.get());
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

std::shared_ptr<OperatorFactory> GroupingOperator::CreateOperatorFactory(
    const std::shared_ptr<const AggregationNode> &aggregationNode, const config::QueryConfig &queryConfig)
{
    if (aggregationNode->GetGroupByKeys().empty()) {
        return std::shared_ptr<AggregationWithExprOperatorFactory>(
            AggregationWithExprOperatorFactory::CreateAggregationWithExprOperatorFactory(aggregationNode, queryConfig));
    }
    return std::shared_ptr<HashAggregationWithExprOperatorFactory>(
        HashAggregationWithExprOperatorFactory::CreateAggregationWithExprOperatorFactory(aggregationNode, queryConfig));
}

std::shared_ptr<OperatorFactory> GroupingOperator::CreateResidualOperatorFactory(
    const std::shared_ptr<const AggregationNode> &aggregationNode, const DataTypesPtr &sourceTypes, int rollUpSize,
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
    for (unsigned int i = 0; i < groupBySize; i++) {
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

    auto residualPlanNoded = std::make_shared<const AggregationNode>(aggregationNode->Id(), groupByKeys_,
        aggregationNode->GetGroupByNum(), aggsKeys_, aggregationNode->OutputType(),
        aggregationNode->GetAggsOutputTypes(), aggregationNode->GetAggFuncTypes(), aggregationNode->GetAggFilters(),
        aggregationNode->GetMaskColumns(), inputRaws, aggregationNode->GetOutputPartials(),
        aggregationNode->GetIsStatisticalAggregate(), aggregationNode->OutputType(), aggregationNode->Sources()[0],
        aggregationNode->GetStep(), aggregationNode->GetAggUdafNames());
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
}
