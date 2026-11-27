/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

/*
 * Description: Expand operator source file
 */

#include "expand.h"
#include "expression/jsonparser/jsonparser.h"
#include "util/config/QueryConfig.h"
#include "util/config_util.h"
#include "vector/vector_helper.h"

using namespace omniruntime::vec;
using namespace omniruntime::expressions;
using namespace omniruntime::codegen;

namespace omniruntime {
namespace op {

bool ExpandOperator::needsInput() { return GetStatus() != OMNI_STATUS_FINISHED && !noMoreInput_ && rowIndex_ == 0; }

int32_t ExpandOperator::AddInput(VectorBatch *vecBatch)
{
    if (vecBatch->GetRowCount() > 0) {
        SetInputVecBatch(vecBatch);
    } else {
        VectorHelper::FreeVecBatch(vecBatch);
        ResetInputVecBatch();
        SetStatus(OMNI_STATUS_NORMAL);
    }
    return 0;
}

int32_t ExpandOperator::GetOutput(VectorBatch **outputVecBatch)
{
    if (this->inputVecBatch == nullptr) {
        if (noMoreInput_) {
            SetStatus(OMNI_STATUS_FINISHED);
        }
        return 0;
    }

    projectedVecs = this->exprEvaluators[rowIndex_]->Evaluate(inputVecBatch, executionContext.get());
    int rowCount = this->projectedVecs->GetRowCount();
    *outputVecBatch = this->projectedVecs;
    this->projectedVecs = nullptr;
    ++rowIndex_;
    if (rowIndex_ == this->exprEvaluators.size()) {
        rowIndex_ = 0;
        VectorHelper::FreeVecBatch(inputVecBatch);
        ResetInputVecBatch();
    }
    return rowCount;
}

OmniStatus ExpandOperator::Close()
{
    if (projectedVecs != nullptr) {
        VectorHelper::FreeVecBatch(projectedVecs);
        projectedVecs = nullptr;
    }
    return OMNI_STATUS_NORMAL;
}

ExpandOperatorFactory *CreateExpandOperatorFactory(std::shared_ptr<const ExpandNode> expandNode,
                                                   const config::QueryConfig &queryConfig)
{
    auto projections = expandNode->GetProjections();
    auto sourceTypes = *(expandNode->InputType());
    auto overflowConfig = queryConfig.IsOverFlowASNull() == true ? new OverflowConfig(OVERFLOW_CONFIG_NULL)
                                                                 : new OverflowConfig(OVERFLOW_CONFIG_EXCEPTION);
    std::vector<std::shared_ptr<ExpressionEvaluator>> exprEvaluators;
    for (const auto &projection : projections) {
        auto exprEvaluator = std::make_shared<ExpressionEvaluator>(projection, sourceTypes, overflowConfig);
        exprEvaluators.push_back(exprEvaluator);
    }

    return new ExpandOperatorFactory(move(exprEvaluators));
}

omniruntime::op::Operator *ExpandOperatorFactory::CreateOperator() { return new ExpandOperator(this->exprEvaluators); }
} // namespace op
} // namespace omniruntime
