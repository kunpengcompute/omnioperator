/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: static cost estimation for lazy AND/OR (conjunct) ordering
 *
 * The weights are relative, tunable constants — see lazy_conjunct_optimization.md
 * section 4.1. The estimator is a pure tree walk with no side effects; it is
 * called per chain evaluation (the cost of the walk is negligible next to the
 * conjunct evaluations it orders).
 *
 * NOTE: use the exprs alias, never a using-directive — omniruntime::op also
 * declares a class named Operator and this header is included from translation
 * units that have both namespaces in scope.
 */

#pragma once

#include "expression/expressions.h"
#include "util/type_util.h"

namespace omniruntime::vectorization {

inline uint64_t EstimateConjunctCost(const expressions::Expr *e)
{
    namespace exprs = omniruntime::expressions;
    if (e == nullptr) {
        return 0;
    }
    switch (e->GetType()) {
        case exprs::ExprType::LITERAL_E:
        case exprs::ExprType::FIELD_E:
        case exprs::ExprType::PARAM_REF_E:
            // Constants and bare column reads are zero-copy slices.
            return 0;
        case exprs::ExprType::UNARY_E:
            return 1 + EstimateConjunctCost(static_cast<const exprs::UnaryExpr *>(e)->exp);
        case exprs::ExprType::IS_NULL_E:
            return 1 + EstimateConjunctCost(static_cast<const exprs::IsNullExpr *>(e)->value);
        case exprs::ExprType::BINARY_E: {
            const auto *b = static_cast<const exprs::BinaryExpr *>(e);
            uint64_t childCost = EstimateConjunctCost(b->left) + EstimateConjunctCost(b->right);
            if (b->op == exprs::Operator::AND || b->op == exprs::Operator::OR) {
                // Same-op chains are flattened by the caller; count only the leaves.
                return childCost;
            }
            switch (b->op) {
                case exprs::Operator::EQ:
                case exprs::Operator::NEQ:
                case exprs::Operator::LT:
                case exprs::Operator::LTE:
                case exprs::Operator::GT:
                case exprs::Operator::GTE:
                    // One fused scalar pass; byte-wise compare for strings.
                    return ((b->left != nullptr && TypeUtil::IsStringType(b->left->GetReturnTypeId())) ? 4 : 1) +
                        childCost;
                case exprs::Operator::DIV:
                case exprs::Operator::MOD:
                case exprs::Operator::TRY_DIV:
                    return 3 + childCost;
                default:
                    return 2 + childCost;
            }
        }
        case exprs::ExprType::IN_E:
            return 2 + EstimateConjunctCost(static_cast<const exprs::InExpr *>(e)->arguments.front());
        case exprs::ExprType::BETWEEN_E: {
            const auto *b = static_cast<const exprs::BetweenExpr *>(e);
            return 2 + EstimateConjunctCost(b->value) + EstimateConjunctCost(b->lowerBound) +
                EstimateConjunctCost(b->upperBound);
        }
        case exprs::ExprType::IF_E: {
            // Branches run on subsets of rows (lazy IF), so weight them down.
            const auto *i = static_cast<const exprs::IfExpr *>(e);
            return EstimateConjunctCost(i->condition) +
                (EstimateConjunctCost(i->trueExpr) + EstimateConjunctCost(i->falseExpr)) / 2;
        }
        case exprs::ExprType::SWITCH_E: {
            // Conditions cascade on shrinking rows; results run on their subsets.
            const auto *s = static_cast<const exprs::SwitchExpr *>(e);
            uint64_t cost = 0;
            for (size_t i = 0; i < s->whenClause.size(); ++i) {
                cost += EstimateConjunctCost(s->whenClause[i].first) / (i == 0 ? 1 : 3);
                cost += EstimateConjunctCost(s->whenClause[i].second) / 2;
            }
            return cost + EstimateConjunctCost(s->falseExpr) / 2;
        }
        case exprs::ExprType::COALESCE_E: {
            uint64_t cost = 1;
            for (const exprs::Expr *arg : static_cast<const exprs::CoalesceExpr *>(e)->arguments) {
                cost += EstimateConjunctCost(arg);
            }
            return cost;
        }
        case exprs::ExprType::FUNC_E: {
            const auto *f = static_cast<const exprs::FuncExpr *>(e);
            if (f->funcName == "CAST" || f->funcName == "CAST_null") {
                return 3 + EstimateConjunctCost(f->arguments.empty() ? nullptr : f->arguments[0]);
            }
            // Pattern matchers dominate everything else; they run RE2 or equivalent
            // per-row work and are exactly the conjuncts reordering must defer.
            // Names match the registry exactly (LIKE, RLike, regexp_extract,
            // regexp_replace, regexp_extract_all) — the planner passes them verbatim.
            if (f->funcName == "RLike" || f->funcName == "LIKE" || f->funcName == "regexp_extract" ||
                f->funcName == "regexp_replace" || f->funcName == "regexp_extract_all") {
                return 100;
            }
            return 10;
        }
        default:
            return 10;
    }
}

} // namespace omniruntime::vectorization
