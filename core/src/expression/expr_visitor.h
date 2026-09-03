/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
 * Description: visitor class for expressions
 */
#ifndef __OMNI_RUNTIME_EXPRESSION_VISITOR_H__
#define __OMNI_RUNTIME_EXPRESSION_VISITOR_H__

#include "expressions.h"

class ExprVisitor {
public:
    virtual ~ExprVisitor() = default;
    virtual void Visit(const omniruntime::expressions::LiteralExpr &e) = 0;
    virtual void Visit(const omniruntime::expressions::FieldExpr &e) = 0;
    virtual void Visit(const omniruntime::expressions::UnaryExpr &e) = 0;
    virtual void Visit(const omniruntime::expressions::BinaryExpr &e) = 0;
    virtual void Visit(const omniruntime::expressions::InExpr &e) = 0;
    virtual void Visit(const omniruntime::expressions::InSubqueryExpr &e) = 0;
    virtual void Visit(const omniruntime::expressions::BetweenExpr &e) = 0;
    virtual void Visit(const omniruntime::expressions::IfExpr &e) = 0;
    virtual void Visit(const omniruntime::expressions::CoalesceExpr &e) = 0;
    virtual void Visit(const omniruntime::expressions::IsNullExpr &e) = 0;
    virtual void Visit(const omniruntime::expressions::FuncExpr &e) = 0;
    virtual void Visit(const omniruntime::expressions::SwitchExpr &e) = 0;
    virtual void Visit(const omniruntime::expressions::ParamRefExpr &e) = 0;
    virtual void Visit(const omniruntime::expressions::LambdaExpr &e) = 0;
};

#endif