/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: Expand operator header
 */
#ifndef __EXPAND_H__
#define __EXPAND_H__

#include <vector>
#include "codegen/expr_evaluator.h"
#include "expression/expressions.h"
#include "operator/execution_context.h"
#include "operator/operator.h"
#include "operator/operator_factory.h"
#include "operator/status.h"
#include "plannode/planNode.h"
#include "type/data_types.h"
#include "util/config/QueryConfig.h"
#include "vector/vector_common.h"

namespace omniruntime {
    namespace op {
        using namespace vec;
        using namespace codegen;

        class ExpandOperator : public Operator {
        public:
            // Pool-bound ctor chains Operator(pool)
            ExpandOperator(omniruntime::mem::OmniTaskMemoryPool *pool,
                std::vector<std::shared_ptr<ExpressionEvaluator>> &exprEvaluators)
                : Operator(pool), projectedVecs(nullptr), exprEvaluators(exprEvaluators)
            {
            }

            ~ExpandOperator() override = default;

            int32_t AddInput(VectorBatch *vecBatch) override;

            int32_t GetOutput(VectorBatch **outputVecBatch) override;

            OmniStatus Close() override;

            bool needsInput() override;

        private:
            VectorBatch *projectedVecs = nullptr;
            std::vector<std::shared_ptr<ExpressionEvaluator>> &exprEvaluators;
            // Used to indicate the index of fieldProjections_.
            size_t rowIndex_{0};
        };

        class ExpandOperatorFactory : public OperatorFactory {
        public:
            explicit ExpandOperatorFactory(std::vector<std::shared_ptr<ExpressionEvaluator>> &&exprEvaluators)
                : exprEvaluators(std::move(exprEvaluators))
            {
                for (const auto& evaluator : this->exprEvaluators) {
                    evaluator->ProjectFuncGeneration();
                }
            }

            ~ExpandOperatorFactory() override = default;

            using OperatorFactory::CreateOperator;
            // Pool-bound override
            omniruntime::op::Operator *CreateOperator(omniruntime::mem::OmniTaskMemoryPool *pool) override;

        private:
            std::vector<std::shared_ptr<ExpressionEvaluator>> exprEvaluators;
        };

        ExpandOperatorFactory *CreateExpandOperatorFactory(
            std::shared_ptr<const ExpandNode> expandNode, const config::QueryConfig &queryConfig);
    } // namespace op
} // namespace omniruntime

#endif
