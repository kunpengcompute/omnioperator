/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 */

#pragma once

#include <memory>
#include <vector>

#include "expression/expressions.h"

namespace omniruntime {
namespace op {

enum class Int32JoinKeyKind {
    KEEP_INT32,
    REWRITE_WIDENING_CAST,
    INCOMPATIBLE,
};

inline Int32JoinKeyKind ClassifyInt32JoinKey(const omniruntime::expressions::Expr *expr)
{
    if (expr == nullptr) {
        return Int32JoinKeyKind::INCOMPATIBLE;
    }

    if (expr->GetReturnTypeId() == omniruntime::type::OMNI_INT) {
        return Int32JoinKeyKind::KEEP_INT32;
    }

    auto *funcExpr = dynamic_cast<const omniruntime::expressions::FuncExpr *>(expr);
    if (funcExpr == nullptr || funcExpr->arguments.size() != 1) {
        return Int32JoinKeyKind::INCOMPATIBLE;
    }

    auto *arg0 = funcExpr->arguments[0];
    if ((funcExpr->funcName == "CAST" || funcExpr->funcName == "CAST_null") &&
        expr->GetReturnTypeId() == omniruntime::type::OMNI_LONG &&
        arg0 != nullptr &&
        arg0->GetType() == omniruntime::expressions::ExprType::FIELD_E &&
        arg0->GetReturnTypeId() == omniruntime::type::OMNI_INT) {
        return Int32JoinKeyKind::REWRITE_WIDENING_CAST;
    }

    return Int32JoinKeyKind::INCOMPATIBLE;
}

inline bool ShouldRewriteJoinKeysToInt32(const std::vector<omniruntime::expressions::Expr *> &leftKeys,
    const std::vector<omniruntime::expressions::Expr *> &rightKeys)
{
    if (leftKeys.empty() || leftKeys.size() != rightKeys.size()) {
        return false;
    }

    bool sawWideningCast = false;
    for (size_t i = 0; i < leftKeys.size(); ++i) {
        auto leftKind = ClassifyInt32JoinKey(leftKeys[i]);
        auto rightKind = ClassifyInt32JoinKey(rightKeys[i]);
        if (leftKind == Int32JoinKeyKind::INCOMPATIBLE || rightKind == Int32JoinKeyKind::INCOMPATIBLE) {
            return false;
        }
        sawWideningCast = sawWideningCast ||
            leftKind == Int32JoinKeyKind::REWRITE_WIDENING_CAST ||
            rightKind == Int32JoinKeyKind::REWRITE_WIDENING_CAST;
    }

    return sawWideningCast;
}

inline omniruntime::expressions::Expr *RewriteJoinKeyExprToInt32(
    omniruntime::expressions::Expr *expr,
    std::vector<std::unique_ptr<omniruntime::expressions::Expr>> &owners)
{
    if (ClassifyInt32JoinKey(expr) != Int32JoinKeyKind::REWRITE_WIDENING_CAST) {
        return expr;
    }

    auto *funcExpr = static_cast<omniruntime::expressions::FuncExpr *>(expr);
    auto *fieldExpr = static_cast<omniruntime::expressions::FieldExpr *>(funcExpr->arguments[0]);
    owners.emplace_back(std::make_unique<omniruntime::expressions::FieldExpr>(
        fieldExpr->colVal,
        fieldExpr->GetReturnType()));
    return owners.back().get();
}

inline std::vector<omniruntime::expressions::Expr *> RewriteJoinKeyExprsToInt32(
    const std::vector<omniruntime::expressions::Expr *> &exprs,
    std::vector<std::unique_ptr<omniruntime::expressions::Expr>> &owners)
{
    std::vector<omniruntime::expressions::Expr *> rewrittenExprs;
    rewrittenExprs.reserve(exprs.size());
    for (auto *expr : exprs) {
        rewrittenExprs.emplace_back(RewriteJoinKeyExprToInt32(expr, owners));
    }
    return rewrittenExprs;
}

} // namespace op
} // namespace omniruntime
