/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
 * Description: Expression Verifier
 */
#include "expr_verifier.h"
#include "codegen/func_registry.h"
#include "vectorization/registration/SimpleFunctionRegistry.h"
#include "util/debug.h"
#include "re2/re2.h"

using namespace omniruntime::expressions;
using namespace omniruntime::type;
using namespace omniruntime::vectorization;

namespace {
/** True if @p pattern compiles with RE2 (linear-time subset; excludes lookaround, backrefs, etc.). */
bool IsRe2SupportedRegexpPattern(const std::string &pattern)
{
    re2::RE2 re(pattern, re2::RE2::Quiet);
    return re.ok();
}
} // namespace

namespace omniruntime {
namespace expressions {
bool ExprVerifier::VisitExpr(Expr &e)
{
    e.Accept(*this);
    e.isAllSupportVectorization_ = isSupportVectorization_;
    return this->isSupportCodegen_ || this->isSupportVectorization_;
}

bool ExprVerifier::VisitExpr(std::shared_ptr<Expr> &e)
{
    e->Accept(*this);
    e->isAllSupportVectorization_ = isSupportVectorization_;
    return this->isSupportCodegen_ || this->isSupportVectorization_;
}

bool ExprVerifier::AreInvalidDataTypes(DataTypeId type1, DataTypeId type2)
{
    return type1 != type2 && !(TypeUtil::IsStringType(type1) && TypeUtil::IsStringType(type2)) && !(
        TypeUtil::IsDecimalType(type1) && TypeUtil::IsDecimalType(type2));
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
        case OMNI_FLOAT:
        case OMNI_ROW:
        case OMNI_ARRAY:
        case OMNI_MAP:
            break;
        default:
            this->isSupportCodegen_ = false;
            break;
    }
}

void ExprVerifier::Visit(const FieldExpr &fieldExpr)
{
    if (fieldExpr.input != nullptr){
        this->isSupportCodegen_ = false;
    }
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
        case OMNI_VARBINARY:
        case OMNI_FLOAT:
        case OMNI_DECIMAL128:
            break;
        case OMNI_ROW:
        case OMNI_ARRAY:
        case OMNI_MAP:
            this->isSupportCodegen_ = false;
            break;
        default:
            this->unSupportedReason = "unSupported FieldExpr DataTypeId: "
                                      + std::to_string(static_cast<int>(fieldExpr.GetReturnTypeId()));
            break;
    }
}

void ExprVerifier::Visit(const UnaryExpr &unaryExpr)
{
    if (unaryExpr.vectorFunction == nullptr) {
        this->isSupportVectorization_ = false;
    }
    if (!VisitExpr(*(unaryExpr.exp))) {
        this->isSupportCodegen_ = false;
        return;
    }
    switch (unaryExpr.op) {
        case omniruntime::expressions::Operator::NOT:
            break;
        default:
            this->isSupportCodegen_ = false;
            break;
    }
}

void ExprVerifier::Visit(const BinaryExpr &binaryExpr)
{
    if (binaryExpr.vectorFunction == nullptr && !binaryExpr.SupportsCheckedArithmeticVectorization()) {
        this->isSupportVectorization_ = false;
    }
    const type::DataType &leftType = *(binaryExpr.left->GetReturnType());
    const type::DataType &rightType = *(binaryExpr.right->GetReturnType());

    if (AreInvalidDataTypes(leftType.GetId(), rightType.GetId())) {
        this->isSupportCodegen_ = false;
        return;
    }

    if (!VisitExpr(*(binaryExpr.left))) {
        this->isSupportCodegen_ = false;
        return;
    }
    if (!VisitExpr(*(binaryExpr.right))) {
        this->isSupportCodegen_ = false;
        return;
    }

    if (binaryExpr.op == omniruntime::expressions::Operator::AND || binaryExpr.op ==
        omniruntime::expressions::Operator::OR) {
        if (!(binaryExpr.left->GetReturnTypeId() == binaryExpr.right->GetReturnTypeId() && binaryExpr.left->
            GetReturnTypeId() == DataTypeId::OMNI_BOOLEAN)) {
            this->isSupportCodegen_ = false;
        }
        return;
    }

    if (binaryExpr.left->GetReturnTypeId() == OMNI_BYTE || binaryExpr.left->GetReturnTypeId() == OMNI_SHORT ||
        binaryExpr.left->GetReturnTypeId() == OMNI_INT || binaryExpr.left->GetReturnTypeId() == OMNI_LONG || binaryExpr.
        left->GetReturnTypeId() == OMNI_DATE32 || binaryExpr.left->GetReturnTypeId() == OMNI_DOUBLE || binaryExpr.left->
        GetReturnTypeId() == OMNI_FLOAT) {
        return;
    } else if (TypeUtil::IsStringType(binaryExpr.left->GetReturnTypeId()) || binaryExpr.left->GetReturnTypeId() ==
        OMNI_TIMESTAMP) {
        switch (binaryExpr.op) {
            case omniruntime::expressions::Operator::LT:
            case omniruntime::expressions::Operator::GT:
            case omniruntime::expressions::Operator::LTE:
            case omniruntime::expressions::Operator::GTE:
            case omniruntime::expressions::Operator::EQ:
            case omniruntime::expressions::Operator::NEQ:
                break;
            default:
                this->isSupportCodegen_ = false;
                break;
        }
        return;
    } else if (TypeUtil::IsDecimalType(binaryExpr.left->GetReturnTypeId())) {
        return;
    }
    this->isSupportCodegen_ = false;
}

void ExprVerifier::Visit(const InExpr &inExpr)
{
    if (inExpr.vectorFunction == nullptr) {
        this->isSupportVectorization_ = false;
    }
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
        case OMNI_DECIMAL128:
        case OMNI_ROW:
            break;
        default:
            this->isSupportCodegen_ = false;
            return;
    }

    if (!VisitExpr(*toCompare)) {
        this->isSupportCodegen_ = false;
        return;
    }
    for (size_t i = 1; i < inExpr.arguments.size(); i++) {
        if (AreInvalidDataTypes(toCompare->GetReturnTypeId(), inExpr.arguments[i]->GetReturnTypeId())) {
            this->isSupportCodegen_ = false;
            return;
        }
        if (!VisitExpr(*(inExpr.arguments[i]))) {
            this->isSupportCodegen_ = false;
            return;
        }
    }
}

void ExprVerifier::Visit(const InSubqueryExpr &inSubExpr)
{
    // InSubqueryExpr requires vectorization support
    if (inSubExpr.vectorFunction == nullptr) {
        this->isSupportVectorization_ = false;
    }

    // Check the probe value type
    DataTypeId valueTypeId = inSubExpr.value->GetReturnTypeId();
    switch (valueTypeId) {
        case OMNI_BYTE:
        case OMNI_SHORT:
        case OMNI_INT:
        case OMNI_DATE32:
        case OMNI_LONG:
        case OMNI_TIMESTAMP:
        case OMNI_DOUBLE:
        case OMNI_FLOAT:
        case OMNI_CHAR:
        case OMNI_VARCHAR:
        case OMNI_DECIMAL64:
        case OMNI_DECIMAL128:
            break;
        default:
            this->isSupportCodegen_ = false;
            return;
    }

    // Visit the probe value and subquery result expressions
    if (!VisitExpr(*(inSubExpr.value))) {
        this->isSupportCodegen_ = false;
        return;
    }
    if (!VisitExpr(*(inSubExpr.subqueryResult))) {
        this->isSupportCodegen_ = false;
        return;
    }
}

void ExprVerifier::Visit(const BetweenExpr &betweenExpr)
{
    if (betweenExpr.vectorFunction == nullptr) {
        this->isSupportVectorization_ = false;
    }
    DataTypeId valueTypeId = betweenExpr.value->GetReturnTypeId();
    if (AreInvalidDataTypes(valueTypeId, betweenExpr.lowerBound->GetReturnTypeId()) && AreInvalidDataTypes(valueTypeId,
        betweenExpr.upperBound->GetReturnTypeId())) {
        this->isSupportCodegen_ = false;
        return;
    }

    if (!VisitExpr(*betweenExpr.value)) {
        this->isSupportCodegen_ = false;
        return;
    }
    if (!VisitExpr(*betweenExpr.lowerBound)) {
        this->isSupportCodegen_ = false;
        return;
    }
    if (!VisitExpr(*betweenExpr.upperBound)) {
        this->isSupportCodegen_ = false;
        return;
    }
}

void ExprVerifier::Visit(const IfExpr &ifExpr)
{
    if (ifExpr.vectorFunction == nullptr) {
        this->isSupportVectorization_ = false;
    }
    Expr *cond = ifExpr.condition;
    Expr *ifTrue = ifExpr.trueExpr;
    Expr *ifFalse = ifExpr.falseExpr;

    if (!VisitExpr(*cond)) {
        this->isSupportCodegen_ = false;
        return;
    }
    if (!VisitExpr(*ifTrue)) {
        this->isSupportCodegen_ = false;
        return;
    }
    if (!VisitExpr(*ifFalse)) {
        this->isSupportCodegen_ = false;
        return;
    }
}

void ExprVerifier::Visit(const CoalesceExpr &coalesceExpr)
{
    if (coalesceExpr.vectorFunction == nullptr) {
        this->isSupportVectorization_ = false;
    }
    Expr *value1Expr = coalesceExpr.value1;
    Expr *value2Expr = coalesceExpr.value2;
    if (!VisitExpr(*value1Expr)) {
        this->isSupportCodegen_ = false;
        return;
    }
    if (!VisitExpr(*value2Expr)) {
        this->isSupportCodegen_ = false;
        return;
    }

}

void ExprVerifier::Visit(const IsNullExpr &isNullExpr)
{
    if (isNullExpr.vectorFunction == nullptr) {
        this->isSupportVectorization_ = false;
    }
    Expr *valueExpr = isNullExpr.value;
    if (!VisitExpr(*valueExpr)) {
        this->isSupportCodegen_ = false;
        return;
    }
}

void ExprVerifier::Visit(const FuncExpr &funcExpr)
{
    if (funcExpr.vectorFunction == nullptr) {
        this->isSupportVectorization_ = false;
    }

    if (funcExpr.functionType == HIVE_UDF) {
        for (auto arg : funcExpr.arguments) {
            switch (arg->dataType->GetId()) {
                case OMNI_INT:
                case OMNI_LONG:
                case OMNI_SHORT:
                case OMNI_BOOLEAN:
                case OMNI_DOUBLE:
                case OMNI_VARCHAR:
                case OMNI_CHAR:
                    break;
                default:
                    this->isSupportCodegen_ = false;
                    break;
            }
        }
        return;
    }

    if (funcExpr.funcName == "DateFormat") {
        if (funcExpr.arguments.size() >= 2) {
            auto literalArg = dynamic_cast<LiteralExpr *>(funcExpr.arguments[1]);
            if (literalArg != nullptr) {
                std::string fmtStr = *(literalArg->stringVal);
                if (fmtStr != "yyyy-MM-dd") {
                    this->isSupportCodegen_ = false;
                    LogWarn("date_format fallback, due to unsupported formatStr, only support yyyy-MM-dd.");
                    return;
                }
            }
        }
    }

    if (funcExpr.funcName == "regexp_replace") {
        auto *literalArg = dynamic_cast<LiteralExpr *>(funcExpr.arguments[1]);
        if (literalArg != nullptr && literalArg->stringVal != nullptr) {
            if (!IsRe2SupportedRegexpPattern(*literalArg->stringVal)) {
                this->isSupportCodegen_ = false;
                this->isSupportVectorization_ = false;
                return;
            }
        } else {
            this->isSupportCodegen_ = false;
            this->isSupportVectorization_ = false;
            return;
        }
    }

    int numArgs = funcExpr.arguments.size();
    std::vector<DataTypeId> params;
    for (int i = 0; i < numArgs; i++) {
        params.push_back(funcExpr.arguments[i]->GetReturnTypeId());
        if (!VisitExpr(*funcExpr.arguments[i])) {
            this->isSupportCodegen_ = false;
            return;
        }
    }
    if (funcExpr.vectorFunction != nullptr && (funcExpr.funcName == "transform" || funcExpr.funcName == "filter" ||
            funcExpr.funcName == "zip_with" || funcExpr.funcName == "forall" || funcExpr.funcName == "exists" ||
            funcExpr.funcName == "transform_keys" || funcExpr.funcName == "transform_values")) {
        this->isSupportCodegen_ = false;
        return;
    }
    auto signature = std::make_shared<FunctionSignature>(funcExpr.funcName, params, funcExpr.GetReturnTypeId());
    auto function = codegen::FunctionRegistry::LookupFunction(signature.get());
    if (function == nullptr) {
        this->isSupportCodegen_ = false;
        return;
    }
}

void ExprVerifier::Visit(const SwitchExpr &switchExpr)
{
    if (switchExpr.vectorFunction == nullptr) {
        this->isSupportVectorization_ = false;
    }
    std::vector<std::pair<Expr *, Expr *>> whenClause = switchExpr.whenClause;
    auto size = whenClause.size();

    for (size_t i = 0; i < size; i++) {
        Expr *cond = whenClause[i].first;
        Expr *resExpr = whenClause[i].second;
        if (!VisitExpr(*cond)) {
            this->isSupportCodegen_ = false;
            return;
        }
        if (!VisitExpr(*resExpr)) {
            this->isSupportCodegen_ = false;
            return;
        }
    }

    Expr *elseExpr = switchExpr.falseExpr;
    if (!VisitExpr(*elseExpr)) {
        this->isSupportCodegen_ = false;
        return;
    }
}

void ExprVerifier::Visit(const ParamRefExpr &paramRefExpr) {
    this->isSupportCodegen_ = false;
}

void ExprVerifier::Visit(const LambdaExpr &lambdaExpr) {
    this->isSupportCodegen_ = false;
    VisitExpr(*lambdaExpr.body_);
}
}
}
