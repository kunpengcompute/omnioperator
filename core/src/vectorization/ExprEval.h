/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: visitor class for expressions
 */

#pragma once
#include "expression/expr_visitor.h"
#include "memory/allocator.h"
#include "operator/execution_context.h"
#include "vectorization/SelectivityVector.h"

namespace omniruntime::vectorization {
class ExprEval : public ExprVisitor {
public:
    ExprEval(vec::VectorBatch *vectorBatch, op::ExecutionContext *context);
    ExprEval(op::ExecutionContext *context);
    ExprEval() = default;

    ~ExprEval() override {}

    void Reset(vec::VectorBatch *vectorBatch, op::ExecutionContext *ctx);

    void Visit(const expressions::LiteralExpr &e) override;

    void Visit(const expressions::FieldExpr &e) override;

    void Visit(const expressions::UnaryExpr &e) override;

    void Visit(const expressions::BinaryExpr &e) override;

    void Visit(const expressions::InExpr &e) override;

    void Visit(const expressions::BetweenExpr &e) override;

    void Visit(const expressions::IfExpr &e) override;

    void Visit(const expressions::CoalesceExpr &e) override;

    void Visit(const expressions::IsNullExpr &e) override;

    void Visit(const expressions::FuncExpr &e) override;

    void Visit(const expressions::SwitchExpr &e) override;

    void Visit(const expressions::ParamRefExpr &e) override;

    void Visit(const expressions::LambdaExpr &e) override;

    void VisitExpr(const expressions::Expr &e);

    vec::BaseVector *GetResult();

    int32_t GetRowCount() const;

    std::vector<vec::BaseVector *> lambdaParams_;
    std::unordered_map<std::string, int32_t> paramNameToIdxMap;

private:
    bool TryEvaluateMd5ConcatWsFusion(const expressions::FuncExpr &e);

    /// Number of rows of the expression space currently being evaluated. When
    /// activeSel_ is set the evaluator works on a compacted subset of rows;
    /// otherwise it works on the full batch.
    int32_t CurrentRowCount() const;

    /// Projects a single column over the given positions and pushes the result.
    void PushProjectedColumn(vec::BaseVector *colVec, type::DataTypeId typeId, const int32_t *selectRow,
        int32_t selectSize);

    /// Evaluates branch for the rows selected by innerSel (positions in the current
    /// expression space) and returns the result vector. The result covers the whole
    /// current space when inPlace is set or innerSel selects every row, and is
    /// compacted to the selected rows otherwise. inPlace should be set for cheap
    /// (leaf-only) branches, whose full-space evaluation is cheaper than gathering.
    vec::BaseVector *EvaluateBranch(const expressions::Expr *branch, const SelectivityVector &innerSel, bool inPlace);

    /// Switches the evaluator into compacted evaluation mode over the rows of sel
    /// for the duration of the guard: the context is temporarily cleared of any
    /// outer filter state, GetResultRowSize() reports the number of selected rows,
    /// and FieldExprs project only the selected positions. All state is restored on
    /// destruction.
    struct ScopedCompactEval {
        ExprEval &eval;
        op::ExecutionContext *ctx;
        bool savedHasFilter;
        bool *savedIsSelect;
        int32_t savedRowSize;
        SelectivityVector *savedSel;
        std::vector<int32_t> savedPos;

        ScopedCompactEval(ExprEval &e, SelectivityVector &sel, const std::vector<int32_t> &pos)
            : eval(e), ctx(e.context), savedHasFilter(ctx->hasFilter),
              savedIsSelect(ctx->GetIsSelectRow()), savedRowSize(ctx->GetResultRowSize()),
              savedSel(eval.activeSel_), savedPos(std::move(eval.activePos_))
        {
            ctx->hasFilter = false;
            ctx->SetIsSelectRow(nullptr);
            ctx->SetResultRowSize(static_cast<int32_t>(pos.size()));
            eval.activeSel_ = &sel;
            eval.activePos_ = pos;
        }

        ~ScopedCompactEval()
        {
            ctx->hasFilter = savedHasFilter;
            ctx->SetIsSelectRow(savedIsSelect);
            ctx->SetResultRowSize(savedRowSize);
            eval.activeSel_ = savedSel;
            eval.activePos_ = std::move(savedPos);
        }
    };

    std::vector<type::DataTypeId> typeIds;
    op::ExecutionContext *context;
    std::vector<vec::BaseVector *> vecBatch_;
    std::stack<vec::BaseVector *> inputValues_;
    int32_t rowSize;
    mem::Allocator *allocator = mem::Allocator::GetAllocator();

    // When set, the evaluator evaluates a compacted subset of the batch rows:
    // the selection bits are over full-batch coordinates and activePos_ holds the
    // full-batch positions of the selected rows in ascending order.
    SelectivityVector *activeSel_ = nullptr;
    std::vector<int32_t> activePos_;
};
}
