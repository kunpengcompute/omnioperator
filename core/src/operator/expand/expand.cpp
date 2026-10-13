/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
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

        bool ExpandOperator::needsInput()
        {
            return GetStatus() != OMNI_STATUS_FINISHED && !noMoreInput_ && rowIndex_ == 0;
        }

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


        ExpandOperatorFactory *CreateExpandOperatorFactory(
            std::shared_ptr<const ExpandNode> expandNode, const config::QueryConfig &queryConfig)
        {
            auto projections = expandNode->GetProjections();
            auto sourceTypes = *(expandNode->InputType());
            std::vector<std::shared_ptr<ExpressionEvaluator>> exprEvaluators;
            for (const auto& projection : projections) {
                auto exprEvaluator = std::make_shared<ExpressionEvaluator>(projection, sourceTypes, queryConfig);
                exprEvaluators.push_back(exprEvaluator);
            }

            return new ExpandOperatorFactory(move(exprEvaluators));
        }

        omniruntime::op::Operator *ExpandOperatorFactory::CreateOperator(omniruntime::mem::OmniTaskMemoryPool *pool)
        {
            return new ExpandOperator(pool, this->exprEvaluators);
        }
    } // namespace op
} // namespace omniruntime
