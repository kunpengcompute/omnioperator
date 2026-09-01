/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
 * Description: print expression tree visitor for expressions
 */
#ifndef __OMNI_RUNTIME_EXPRESSION_PRINTER_H__
#define __OMNI_RUNTIME_EXPRESSION_PRINTER_H__

#include "expr_visitor.h"
#include "util/type_util.h"
#include "expressions.h"

class ExprPrinter : public ExprVisitor {
public:
    void Visit(const omniruntime::expressions::LiteralExpr &e) override;
    void Visit(const omniruntime::expressions::FieldExpr &e) override;
    void Visit(const omniruntime::expressions::UnaryExpr &e) override;
    void Visit(const omniruntime::expressions::BinaryExpr &e) override;
    void Visit(const omniruntime::expressions::InExpr &e) override;
    void Visit(const omniruntime::expressions::InSubqueryExpr &e) override;
    void Visit(const omniruntime::expressions::BetweenExpr &e) override;
    void Visit(const omniruntime::expressions::IfExpr &e) override;
    void Visit(const omniruntime::expressions::CoalesceExpr &e) override;
    void Visit(const omniruntime::expressions::IsNullExpr &e) override;
    void Visit(const omniruntime::expressions::FuncExpr &e) override;
    void Visit(const omniruntime::expressions::SwitchExpr &e) override;
    void Visit(const omniruntime::expressions::ParamRefExpr &e) override;
    void Visit(const omniruntime::expressions::LambdaExpr &e) override;

private:
    std::string BinaryExprPrinterHelper(const omniruntime::expressions::Operator &op,
        const omniruntime::type::DataType &type) const;
    std::string GenerateIndentation() const;
    int indentationDepth = 0;
};

#endif