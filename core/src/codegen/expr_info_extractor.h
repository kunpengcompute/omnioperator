/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
 * Description: Extract essential information from the expression tree
 */
#ifndef __OMNI_RUNTIME_EXPR_INFO_EXTRACTOR__
#define __OMNI_RUNTIME_EXPR_INFO_EXTRACTOR__

#include "expression/expr_visitor.h"
#include "func_registry_string.h"
#include "func_registry_dictionary.h"
#include "func_registry_decimal.h"
#include "func_registry_context.h"
#include "function.h"

namespace omniruntime::codegen {
class ExprInfoExtractor : public ExprVisitor {
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

    std::set<int32_t> GetVectorIndexes();

private:
    std::set<int32_t> vectorIndexes;
};
}
#endif
