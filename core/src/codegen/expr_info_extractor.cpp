/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
 * Description: Extract essential information from the expression tree
 */
#include <string>
#include "expr_info_extractor.h"

namespace omniruntime::codegen {
using namespace omniruntime::expressions;
using namespace std;

void ExprInfoExtractor::Visit(const LiteralExpr &e) {}

void ExprInfoExtractor::Visit(const FieldExpr &e)
{
    this->vectorIndexes.insert(e.colVal);
}

void ExprInfoExtractor::Visit(const BinaryExpr &e)
{
    e.left->Accept(*this);
    e.right->Accept(*this);
}

void ExprInfoExtractor::Visit(const UnaryExpr &e)
{
    e.exp->Accept(*this);
}

void ExprInfoExtractor::Visit(const IfExpr &e)
{
    e.condition->Accept(*this);
    e.trueExpr->Accept(*this);
    e.falseExpr->Accept(*this);
}

void ExprInfoExtractor::Visit(const InExpr &e)
{
    for (auto arg : e.arguments) {
        arg->Accept(*this);
    }
}

void ExprInfoExtractor::Visit(const InSubqueryExpr &e)
{
    e.value->Accept(*this);
    e.subqueryResult->Accept(*this);
}

void ExprInfoExtractor::Visit(const BetweenExpr &e)
{
    e.value->Accept(*this);
    e.lowerBound->Accept(*this);
    e.upperBound->Accept(*this);
}

void ExprInfoExtractor::Visit(const CoalesceExpr &e)
{
    e.value1->Accept(*this);
    e.value2->Accept(*this);
}
void ExprInfoExtractor::Visit(const SwitchExpr &e)
{
    e.falseExpr->Accept(*this);
    for (auto &when : e.whenClause) {
        when.first->Accept(*this);
        when.second->Accept(*this);
    }
}
void ExprInfoExtractor::Visit(const IsNullExpr &e)
{
    e.value->Accept(*this);
}

void ExprInfoExtractor::Visit(const FuncExpr &e)
{
    for (auto arg : e.arguments) {
        arg->Accept(*this);
    }
}

void ExprInfoExtractor::Visit(const ParamRefExpr &e)
{

}

void ExprInfoExtractor::Visit(const LambdaExpr &e)
{

}

std::set<int32_t> ExprInfoExtractor::GetVectorIndexes()
{
    return this->vectorIndexes;
}
}
