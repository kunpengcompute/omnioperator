/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
 * KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 */

/*
 * Description: Expression Verifier
 */

#include "expr_verifier.h"
#include "codegen/func_registry.h"

using namespace omniruntime::expressions;
using namespace omniruntime::type;

namespace omniruntime {
namespace expressions {
bool ExprVerifier::VisitExpr(const Expr &e)
{
    e.Accept(*this);
    return this->supportedFlag;
}

bool ExprVerifier::VisitExpr(const std::shared_ptr<const Expr> &e)
{
    e->Accept(*this);
    return this->supportedFlag;
}

bool ExprVerifier::AreInvalidDataTypes(DataTypeId type1, DataTypeId type2)
{
    return type1 != type2 && !(TypeUtil::IsStringType(type1) && TypeUtil::IsStringType(type2)) &&
           !(TypeUtil::IsDecimalType(type1) && TypeUtil::IsDecimalType(type2));
}

void ExprVerifier::Visit(const LiteralExpr &literalExpr)
{
    switch (literalExpr.GetReturnTypeId()) {
        case OMNI_BYTE:
        case OMNI_SHORT:
        case OMNI_INT:
        case OMNI_DATE32:
        case OMNI_LONG:
        case OMNI_TIMESTAMP:
        case OMNI_DOUBLE:
        case OMNI_CHAR:
        case OMNI_VARCHAR:
        case OMNI_BOOLEAN:
        case OMNI_DECIMAL64:
        case OMNI_DECIMAL128:
        case OMNI_ARRAY: this->supportedFlag = true; break;
        default: this->supportedFlag = false; break;
    }
}

void ExprVerifier::Visit(const FieldExpr &fieldExpr)
{
    switch (fieldExpr.GetReturnTypeId()) {
        case OMNI_BYTE:
        case OMNI_SHORT:
        case OMNI_INT:
        case OMNI_DATE32:
        case OMNI_LONG:
        case OMNI_TIMESTAMP:
        case OMNI_DOUBLE:
        case OMNI_CHAR:
        case OMNI_VARCHAR:
        case OMNI_BOOLEAN:
        case OMNI_DECIMAL64:
        case OMNI_DECIMAL128:
        case OMNI_ARRAY: this->supportedFlag = true; break;
        default:
            this->supportedFlag = false;
            this->unSupportedReason =
                "unSupported FieldExpr DataTypeId: " + std::to_string(static_cast<int>(fieldExpr.GetReturnTypeId()));
            break;
    }
}

void ExprVerifier::Visit(const UnaryExpr &unaryExpr)
{
    if (!VisitExpr(*(unaryExpr.exp))) {
        this->supportedFlag = false;
        return;
    }
    switch (unaryExpr.op) {
        case omniruntime::expressions::Operator::NOT: this->supportedFlag = true; break;
        default: this->supportedFlag = false; break;
    }
}

void ExprVerifier::Visit(const BinaryExpr &binaryExpr)
{
    const type::DataType &leftType = *(binaryExpr.left->GetReturnType());
    const type::DataType &rightType = *(binaryExpr.right->GetReturnType());

    if (AreInvalidDataTypes(leftType.GetId(), rightType.GetId())) {
        this->supportedFlag = false;
        return;
    }

    if (!VisitExpr(*(binaryExpr.left))) {
        this->supportedFlag = false;
        return;
    }
    if (!VisitExpr(*(binaryExpr.right))) {
        this->supportedFlag = false;
        return;
    }

    if (binaryExpr.op == omniruntime::expressions::Operator::AND ||
        binaryExpr.op == omniruntime::expressions::Operator::OR) {
        this->supportedFlag = (binaryExpr.left->GetReturnTypeId() == binaryExpr.right->GetReturnTypeId() &&
                               binaryExpr.left->GetReturnTypeId() == DataTypeId::OMNI_BOOLEAN);
        return;
    }

    if (binaryExpr.left->GetReturnTypeId() == OMNI_BYTE || binaryExpr.left->GetReturnTypeId() == OMNI_SHORT ||
        binaryExpr.left->GetReturnTypeId() == OMNI_INT || binaryExpr.left->GetReturnTypeId() == OMNI_LONG ||
        binaryExpr.left->GetReturnTypeId() == OMNI_DATE32 || binaryExpr.left->GetReturnTypeId() == OMNI_DOUBLE) {
        this->supportedFlag = true;
        return;
    } else if (TypeUtil::IsStringType(binaryExpr.left->GetReturnTypeId()) ||
               binaryExpr.left->GetReturnTypeId() == OMNI_TIMESTAMP) {
        switch (binaryExpr.op) {
            case omniruntime::expressions::Operator::LT:
            case omniruntime::expressions::Operator::GT:
            case omniruntime::expressions::Operator::LTE:
            case omniruntime::expressions::Operator::GTE:
            case omniruntime::expressions::Operator::EQ:
            case omniruntime::expressions::Operator::NEQ: this->supportedFlag = true; break;
            default: this->supportedFlag = false; break;
        }
        return;
    } else if (TypeUtil::IsDecimalType(binaryExpr.left->GetReturnTypeId())) {
        this->supportedFlag = true;
        return;
    }
    this->supportedFlag = false;
}

void ExprVerifier::Visit(const InExpr &inExpr)
{
    Expr *toCompare = inExpr.arguments[0];
    switch (toCompare->GetReturnTypeId()) {
        case OMNI_BYTE:
        case OMNI_SHORT:
        case OMNI_INT:
        case OMNI_DATE32:
        case OMNI_LONG:
        case OMNI_TIMESTAMP:
        case OMNI_DOUBLE:
        case OMNI_CHAR:
        case OMNI_VARCHAR:
        case OMNI_DECIMAL64:
        case OMNI_DECIMAL128: break;
        default: this->supportedFlag = false; return;
    }

    if (!VisitExpr(*toCompare)) {
        this->supportedFlag = false;
        return;
    }
    for (size_t i = 1; i < inExpr.arguments.size(); i++) {
        if (AreInvalidDataTypes(toCompare->GetReturnTypeId(), inExpr.arguments[i]->GetReturnTypeId())) {
            this->supportedFlag = false;
            return;
        }
        if (!VisitExpr(*(inExpr.arguments[i]))) {
            this->supportedFlag = false;
            return;
        }
    }
    this->supportedFlag = true;
}

void ExprVerifier::Visit(const BetweenExpr &betweenExpr)
{
    DataTypeId valueTypeId = betweenExpr.value->GetReturnTypeId();
    if (AreInvalidDataTypes(valueTypeId, betweenExpr.lowerBound->GetReturnTypeId()) &&
        AreInvalidDataTypes(valueTypeId, betweenExpr.upperBound->GetReturnTypeId())) {
        this->supportedFlag = false;
        return;
    }

    if (!VisitExpr(*betweenExpr.value)) {
        this->supportedFlag = false;
        return;
    }
    if (!VisitExpr(*betweenExpr.lowerBound)) {
        this->supportedFlag = false;
        return;
    }
    if (!VisitExpr(*betweenExpr.upperBound)) {
        this->supportedFlag = false;
        return;
    }

    this->supportedFlag = true;
}

void ExprVerifier::Visit(const IfExpr &ifExpr)
{
    Expr *cond = ifExpr.condition;
    Expr *ifTrue = ifExpr.trueExpr;
    Expr *ifFalse = ifExpr.falseExpr;

    if (!VisitExpr(*cond)) {
        this->supportedFlag = false;
        return;
    }
    if (!VisitExpr(*ifTrue)) {
        this->supportedFlag = false;
        return;
    }
    if (!VisitExpr(*ifFalse)) {
        this->supportedFlag = false;
        return;
    }
    this->supportedFlag = true;
}

void ExprVerifier::Visit(const CoalesceExpr &coalesceExpr)
{
    Expr *value1Expr = coalesceExpr.value1;
    Expr *value2Expr = coalesceExpr.value2;
    if (!VisitExpr(*value1Expr)) {
        this->supportedFlag = false;
        return;
    }
    if (!VisitExpr(*value2Expr)) {
        this->supportedFlag = false;
        return;
    }

    this->supportedFlag = true;
}

void ExprVerifier::Visit(const IsNullExpr &isNullExpr)
{
    Expr *valueExpr = isNullExpr.value;
    if (!VisitExpr(*valueExpr)) {
        this->supportedFlag = false;
        return;
    }
    this->supportedFlag = true;
}

void ExprVerifier::Visit(const FuncExpr &funcExpr)
{
    if (funcExpr.funcName == "LIKE") {
        this->supportedFlag = false;
        return;
    }
    int numArgs = funcExpr.arguments.size();
    std::vector<DataTypeId> params;
    for (int i = 0; i < numArgs; i++) {
        params.push_back(funcExpr.arguments[i]->GetReturnTypeId());
        if (!VisitExpr(*funcExpr.arguments[i])) {
            this->supportedFlag = false;
            return;
        }
    }
    auto signature = FunctionSignature(funcExpr.funcName, params, funcExpr.GetReturnTypeId());
    auto function = codegen::FunctionRegistry::LookupFunction(&signature);
    if (function == nullptr) {
        this->supportedFlag = false;
        return;
    }
    this->supportedFlag = true;
}

void ExprVerifier::Visit(const SwitchExpr &switchExpr)
{
    std::vector<std::pair<Expr *, Expr *>> whenClause = switchExpr.whenClause;
    auto size = whenClause.size();

    for (size_t i = 0; i < size; i++) {
        Expr *cond = whenClause[i].first;
        Expr *resExpr = whenClause[i].second;
        if (!VisitExpr(*cond)) {
            this->supportedFlag = false;
            return;
        }
        if (!VisitExpr(*resExpr)) {
            this->supportedFlag = false;
            return;
        }
    }

    Expr *elseExpr = switchExpr.falseExpr;
    if (!VisitExpr(*elseExpr)) {
        this->supportedFlag = false;
        return;
    }

    this->supportedFlag = true;
}
} // namespace expressions
} // namespace omniruntime
