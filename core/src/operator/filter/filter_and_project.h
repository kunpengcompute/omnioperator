/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
 * Description: FilterAndProject operator header
 */
#ifndef __FILTER_AND_PROJECT_H__
#define __FILTER_AND_PROJECT_H__

#include <memory>
#include <vector>

#include "codegen/expr_evaluator.h"
#include "codegen/simple_filter_codegen.h"
#include "expression/expressions.h"
#include "operator/config/operator_config.h"
#include "operator/execution_context.h"
#include "operator/operator.h"
#include "operator/operator_factory.h"
#include "operator/status.h"
#include "plannode/planNode.h"
#include "type/data_types.h"
#include "util/config/QueryConfig.h"
#include "vector/vector_batch.h"

namespace omniruntime {
namespace op {
using namespace omniruntime::codegen;
using SimpleRowExprEvalFunc = bool (*)(int64_t *, bool *, int32_t *, bool *, int32_t *, int64_t);

/**
 * Simple Filter that can be evaluated given expression,
 * all the involved value references and types
 */
class SimpleFilter {
public:
    /* *
     * Simple Filter constructor
     * @param expression the filter expression, must return evaluates to boolean type
     */
    explicit SimpleFilter(const omniruntime::expressions::Expr &expression);

    ~SimpleFilter();

    SimpleFilter(const SimpleFilter &simpleFilter);

    SimpleFilter &operator=(const SimpleFilter &simpleFilter) = delete;

    /* *
     * Initialize the filter, this method must be called after the construct
     * @return if the expression and types can be supported or not
     */
    bool Initialize(OverflowConfig *overflowConfig);

    /* *
     * Get all the vector indexes used in the expression
     * @return set including the indexes, or empty set if filter not supported or initialized
     */
    std::set<int32_t> &GetVectorIndexes();

    /* *
     * Evaluate the filter
     * To make it consistent and simplify the evaluation logic, please make sure
     * index of the value, isNull and length matches with the index in expression
     * for example:
     * and(less_than(#0, 100), greater_than(#5, 5))
     * The input array size should be at least 6, and there should be values at
     * index 0 and index 6, for other indexes the values can simply be default value
     * such as 0 for address, false for boolean, 0 for length.
     *
     * @param values array of value addresses that will be used for evaluation
     * @param isNull array of booleans indicating if each value is null
     * @param lengths array of lengths for varchar type values, 0 for other types
     * @param the execution context
     * @return true if the data matches the expression, false if it doesn't match
     */
    bool Evaluate(int64_t *values, bool *isNulls, int32_t *lengths, int64_t executionContext);

    const expressions::Expr *GetExpression() { return this->expression; }

    /* *
     * short-circuit logic for column filter, no need to go through codegen
     * @return is short-circuit
     */
    bool IsColumnFilter() { return isColumnFilter; }

private:
    std::shared_ptr<codegen::SimpleFilterCodeGen> codegen;
    const expressions::Expr *expression;
    SimpleRowExprEvalFunc func;
    bool *isResultNull;
    int32_t *resultLength;
    bool initialized;
    bool isColumnFilter = false;
};

OperatorFactory *CreateFilterOperatorFactory(
    std::shared_ptr<const FilterNode> filterNode, const config::QueryConfig &queryConfig);

struct StringViewFilterValidationInfo {
    std::vector<int32_t> fieldIndexes;
};

StringViewFilterValidationInfo ValidateStringViewFilterForRuntime(
    const expressions::Expr *filterExpr, const type::DataTypes &sourceTypes);

void ValidateStringViewInputBatch(
    omniruntime::vec::VectorBatch *vecBatch, const std::vector<int32_t> &fieldIndexes);

class FilterAndProjectOperator : public Operator {
public:
    FilterAndProjectOperator(omniruntime::mem::OmniTaskMemoryPool *pool,
        std::shared_ptr<ExpressionEvaluator> &exprEvaluator,
        std::vector<int32_t> stringViewValidationFields = {})
        // selectedRowsBuffer is the recurring per-batch filter-mask buffer. Bind it to
        // the pool-bound Allocator (from the base ExecutionContext, which is constructed
        // by Operator(pool) above and is valid even when pool==nullptr) so AllocateReuse()
        // routes through per-task accounting instead of the no-pool bridge.
        : Operator(pool), selectedRowsBuffer(0, executionContext->GetAllocator()),
          projectedVecs(nullptr), exprEvaluator(exprEvaluator),
          stringViewValidationFields(std::move(stringViewValidationFields))
    {
        SetOperatorName(metricsNameFilter);
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

    ~FilterAndProjectOperator() override = default;

    int32_t AddInput(omniruntime::vec::VectorBatch *vecBatch) override;

    int32_t GetOutput(omniruntime::vec::VectorBatch **outputVecBatch) override;

    bool ProcessRow(int64_t valueAddrs[], const int32_t inputLens[], int64_t outValueAddrs[], int32_t outLens[]);

    OmniStatus Close() override;

private:
    omniruntime::mem::AlignedBuffer<int32_t> selectedRowsBuffer;
    omniruntime::vec::VectorBatch *projectedVecs;
    std::shared_ptr<ExpressionEvaluator> &exprEvaluator;
    std::vector<int32_t> stringViewValidationFields;
    bool stringViewValidationLogged = false;
};

class FilterAndProjectOperatorFactory : public OperatorFactory {
public:
    explicit FilterAndProjectOperatorFactory(std::shared_ptr<ExpressionEvaluator> &&exprEvaluator,
        std::vector<int32_t> stringViewValidationFields = {})
        : exprEvaluator(std::move(exprEvaluator)),
          stringViewValidationFields(std::move(stringViewValidationFields))
    {
        this->exprEvaluator->FilterFuncGeneration();
    }

    ~FilterAndProjectOperatorFactory() override = default;

    using OperatorFactory::CreateOperator;
    // W2b-03: pool-bound override
    Operator *CreateOperator(omniruntime::mem::OmniTaskMemoryPool *pool) override;

private:
    std::shared_ptr<ExpressionEvaluator> exprEvaluator;
    std::vector<int32_t> stringViewValidationFields;
};
} // namespace op
} // namespace omniruntime
#endif
