/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
 * Description: Expression Verifier
 */
#ifndef OMNI_RUNTIME_EXPR_VERIFIER_H
#define OMNI_RUNTIME_EXPR_VERIFIER_H

#include "expression/expr_visitor.h"

namespace omniruntime::expressions {
class ExprVerifier : public ExprVisitor {
public:
    void Visit(const omniruntime::expressions::LiteralExpr &literalExpr) override;
    void Visit(const omniruntime::expressions::FieldExpr &fieldExpr) override;
    void Visit(const omniruntime::expressions::UnaryExpr &unaryExpr) override;
    void Visit(const omniruntime::expressions::BinaryExpr &binaryExpr) override;
    void Visit(const omniruntime::expressions::InExpr &inExpr) override;
    void Visit(const omniruntime::expressions::InSubqueryExpr &inSubExpr) override;
    void Visit(const omniruntime::expressions::BetweenExpr &betweenExpr) override;
    void Visit(const omniruntime::expressions::IfExpr &ifExpr) override;
    void Visit(const omniruntime::expressions::CoalesceExpr &coalesceExpr) override;
    void Visit(const omniruntime::expressions::IsNullExpr &isNullExpr) override;
    void Visit(const omniruntime::expressions::FuncExpr &funcExpr) override;
    void Visit(const omniruntime::expressions::SwitchExpr &switchExpr) override;
    void Visit(const omniruntime::expressions::ParamRefExpr &paramRefExpr) override;
    void Visit(const omniruntime::expressions::LambdaExpr &lambdaExpr) override;
    bool VisitExpr(omniruntime::expressions::Expr &e);
    bool VisitExpr(std::shared_ptr<Expr> &e);
    std::string& GetUnSupportedReason()
    {
        return unSupportedReason;
    }
    bool IsSupportVectorization() const
    {
        return isSupportVectorization_;
    }

    bool IsSupportCodegen() const
    {
        return isSupportCodegen_;
    }

private:
    std::string unSupportedReason = "";
    bool isSupportCodegen_ = true;
    bool isSupportVectorization_ = true;

    static bool AreInvalidDataTypes(omniruntime::type::DataTypeId type1, omniruntime::type::DataTypeId type2);
};
}

#endif // OMNI_RUNTIME_EXPR_VERIFIER_H
