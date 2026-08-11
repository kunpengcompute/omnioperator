/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: visitor class for expressions
 */

#pragma once
#include "expression/expr_visitor.h"
#include "memory/allocator.h"

namespace omniruntime::vectorization {
class ExprEval : public ExprVisitor {
public:
    ExprEval(vec::VectorBatch *vectorBatch, op::ExecutionContext *context);
    ExprEval(op::ExecutionContext *context);

    ~ExprEval() override {}

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
    std::vector<type::DataTypeId> typeIds;
    op::ExecutionContext *context;
    std::vector<vec::BaseVector *> vecBatch_;
    std::stack<vec::BaseVector *> inputValues_;
    int32_t rowSize;
    // Row count of the input batch. Differs from rowSize when a filter is active, because
    // then the expression tree runs over the compacted (selected) rows only.
    int32_t inputRowSize = 0;
    mem::Allocator *allocator = mem::Allocator::GetAllocator();
};
}
