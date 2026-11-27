/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
 * Description: Projection operator header
 */
#ifndef __PROJECTION_H__
#define __PROJECTION_H__

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
#include "vectorization/registration/Register.h"

namespace omniruntime {
namespace op {
using namespace vec;
using namespace codegen;

class ProjectionOperator : public Operator {
public:
    ProjectionOperator(omniruntime::mem::OmniTaskMemoryPool *pool,
        std::shared_ptr<ExpressionEvaluator> &exprEvaluator)
        : Operator(pool), projectedVecs(nullptr), exprEvaluator(exprEvaluator)
    {
        const auto &projs = exprEvaluator->GetProjections();
        for (uint32_t out = 0; out < projs.size(); ++out) {
            if (!projs[out]) {
                continue;
            }
            if (projs[out]->IsColumnProjection()) {
                identityProjections_.emplace_back(
                    static_cast<uint32_t>(projs[out]->GetColumnProjectionIndex()), out);
                continue;
            }
            const auto *expr = projs[out]->GetExpr();
            while (expr != nullptr && expr->GetType() == expressions::ExprType::FUNC_E) {
                const auto *func = static_cast<const expressions::FuncExpr *>(expr);
                if (func->arguments.size() != 1) {
                    expr = nullptr;
                    break;
                }
                const auto &name = func->funcName;
                if (name != "CAST" && name != "cast" && name != "CAST_null" && name != "alias" && name != "ALIAS") {
                    expr = nullptr;
                    break;
                }
                expr = func->arguments[0];
            }
            if (expr != nullptr && expr->GetType() == expressions::ExprType::FIELD_E) {
                identityProjections_.emplace_back(
                    static_cast<uint32_t>(static_cast<const expressions::FieldExpr *>(expr)->colVal), out);
            }
        }
    }

    ~ProjectionOperator() override = default;

    int32_t AddInput(VectorBatch *vecBatch) override;

    int32_t GetOutput(VectorBatch **outputVecBatch) override;

    OmniStatus Close() override;

private:
    VectorBatch *projectedVecs = nullptr;
    std::shared_ptr<ExpressionEvaluator> &exprEvaluator;
};

class ProjectionOperatorFactory : public OperatorFactory {
public:
    explicit ProjectionOperatorFactory(std::shared_ptr<ExpressionEvaluator> &&exprEvaluator)
        : exprEvaluator(std::move(exprEvaluator))
    {
        link_register_functions();
        this->exprEvaluator->ProjectFuncGeneration();
    }

    ~ProjectionOperatorFactory() override = default;

    using OperatorFactory::CreateOperator;
    // W2b-03: pool-bound override
    omniruntime::op::Operator *CreateOperator(omniruntime::mem::OmniTaskMemoryPool *pool) override;

private:
    std::shared_ptr<ExpressionEvaluator> exprEvaluator;
};

OperatorFactory *CreateProjectOperatorFactory(
    std::shared_ptr<const ProjectNode> projectNode, const config::QueryConfig &queryConfig);
} // namespace op
} // namespace omniruntime

#endif
