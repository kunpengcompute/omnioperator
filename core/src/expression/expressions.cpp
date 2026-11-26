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
 *
 * Description:
 */

#include "expressions.h"
#include <string>
#include <algorithm>
#include <utility>
#include "type/data_type.h"
#include "codegen/func_registry.h"
#include "util/type_util.h"
#include "expr_verifier.h"
#include "expr_printer.h"

using namespace std;
using namespace omniruntime::type;
using namespace omniruntime::codegen;

namespace omniruntime {
namespace expressions {

// Prevent ExprVerifier from being optimized out by the compiler.
static ExprVerifier globalExprVerifier;
static ExprPrinter globalExprPrinter;

bool IsNullLiteral(const std::string &value)
{
    const std::string loweredNullValue = "null";
    if (value.size() != loweredNullValue.size()) {
        return false;
    }
    for (uint32_t i = 0; i < loweredNullValue.size(); i++) {
        if (tolower(value[i]) != loweredNullValue[i]) {
            return false;
        }
    }
    return true;
}

bool IsComparisonOperator(Operator op)
{
    return op == Operator::GT || op == Operator::GTE || op == Operator::LT || op == Operator::LTE ||
           op == Operator::EQ || op == Operator::NEQ;
}

bool IsLogicalOperator(Operator op) { return op == Operator::AND || op == Operator::OR || op == Operator::NOT; }

Operator StringToOperator(const std::string &opStr)
{
    auto opItr = OPERATOR_FROM_STRING.find(opStr);
    if (opItr != OPERATOR_FROM_STRING.end()) {
        return opItr->second;
    }
    return Operator::INVALIDOP;
}

ExprType Expr::GetType() const { return ExprType::INVALID_E; }

DataTypePtr Expr::GetReturnType() const { return dataType; }

DataTypeId Expr::GetReturnTypeId() const { return dataType->GetId(); }

void Expr::DeleteExprs(const std::vector<Expr *> &exprs)
{
    for (Expr *exp : exprs) {
        delete exp;
    }
}

void Expr::DeleteExprs(const std::vector<std::vector<Expr *>> &exprs)
{
    for (const std::vector<Expr *> &expr : exprs) {
        Expr::DeleteExprs(expr);
    }
}

// Literal Expression methods
LiteralExpr::LiteralExpr() = default;

LiteralExpr::~LiteralExpr() { delete stringVal; }

ExprType LiteralExpr::GetType() const { return ExprType::LITERAL_E; }

// Helper constructors for different data types
LiteralExpr::LiteralExpr(bool val, DataTypePtr dt)
{
    dataType = std::move(dt);
    boolVal = val;
}

LiteralExpr::LiteralExpr(int8_t val, DataTypePtr dt, bool isNulls)
{
    dataType = std::move(dt);
    byteVal = val;
    isNull = isNulls;
}

LiteralExpr::LiteralExpr(int16_t val, DataTypePtr dt, bool isNulls)
{
    dataType = std::move(dt);
    shortVal = val;
    isNull = isNulls;
}

LiteralExpr::LiteralExpr(int32_t val, DataTypePtr dt, bool isNulls)
{
    dataType = std::move(dt);
    intVal = val;
    isNull = isNulls;
}

LiteralExpr::LiteralExpr(int64_t val, DataTypePtr dt)
{
    dataType = std::move(dt);
    longVal = val;
}

LiteralExpr::LiteralExpr(double val, DataTypePtr dt)
{
    dataType = std::move(dt);
    doubleVal = val;
}

LiteralExpr::LiteralExpr(std::string *val, DataTypePtr dt)
{
    dataType = std::move(dt);
    stringVal = val;
}

// FieldExpr
FieldExpr::FieldExpr() = default;

FieldExpr::~FieldExpr() = default;

ExprType FieldExpr::GetType() const { return ExprType::FIELD_E; }

// Helper constructors
FieldExpr::FieldExpr(int32_t colIdx, DataTypePtr colType)
{
    dataType = std::move(colType);
    colVal = colIdx;
}

BinaryExpr::BinaryExpr() { dataType = BooleanType(); }

BinaryExpr::BinaryExpr(Operator bop, Expr *leftExpr, Expr *rightExpr, DataTypePtr dt)
{
    op = bop;
    left = leftExpr;
    right = rightExpr;
    dataType = std::move(dt);
}

BinaryExpr::~BinaryExpr()
{
    delete left;
    delete right;
}

ExprType BinaryExpr::GetType() const { return ExprType::BINARY_E; }

UnaryExpr::UnaryExpr() { dataType = BooleanType(); }

UnaryExpr::UnaryExpr(Operator logOp, Expr *bodyExpr) : op(logOp), exp(bodyExpr) {}

UnaryExpr::UnaryExpr(Operator uop, Expr *expr, DataTypePtr dt) : op(uop), exp(expr) { dataType = std::move(dt); }

UnaryExpr::~UnaryExpr() { delete exp; }

ExprType UnaryExpr::GetType() const { return ExprType::UNARY_E; }

InExpr::InExpr() { dataType = BooleanType(); }

InExpr::~InExpr() { DeleteExprs(arguments); }

InExpr::InExpr(std::vector<Expr *> args)
{
    dataType = BooleanType();
    arguments = std::move(args);
}

ExprType InExpr::GetType() const { return ExprType::IN_E; }

BetweenExpr::BetweenExpr() { dataType = BooleanType(); }

BetweenExpr::~BetweenExpr()
{
    delete value;
    delete lowerBound;
    delete upperBound;
}

BetweenExpr::BetweenExpr(Expr *val, Expr *lowBound, Expr *upBound)
{
    dataType = BooleanType();
    value = val;
    lowerBound = lowBound;
    upperBound = upBound;
}

ExprType BetweenExpr::GetType() const { return ExprType::BETWEEN_E; }

SwitchExpr::SwitchExpr() : whenClause(), falseExpr() {}

SwitchExpr::~SwitchExpr()
{
    for (std::pair<Expr *, Expr *> &vec : whenClause) {
        delete vec.first;
        delete vec.second;
    }
    delete falseExpr;
}

SwitchExpr::SwitchExpr(const std::vector<std::pair<Expr *, Expr *>> &whens, Expr *fexp)
{
    dataType = fexp->GetReturnType();
    whenClause = whens;
    falseExpr = fexp;
}

ExprType SwitchExpr::GetType() const { return ExprType::SWITCH_E; }

IfExpr::IfExpr() : condition(), trueExpr(), falseExpr() {}

IfExpr::~IfExpr()
{
    delete condition;
    delete trueExpr;
    delete falseExpr;
}

IfExpr::IfExpr(Expr *cond, Expr *texp, Expr *fexp)
{
    dataType = texp->GetReturnType();
    condition = cond;
    trueExpr = texp;
    falseExpr = fexp;
}

ExprType IfExpr::GetType() const { return ExprType::IF_E; }

CoalesceExpr::CoalesceExpr() : value1(), value2() {}

CoalesceExpr::~CoalesceExpr()
{
    delete value1;
    delete value2;
}

CoalesceExpr::CoalesceExpr(Expr *val1, Expr *val2)
{
    dataType = val1->GetReturnType();
    value1 = val1;
    value2 = val2;
}

ExprType CoalesceExpr::GetType() const { return ExprType::COALESCE_E; }

IsNullExpr::IsNullExpr() : value() {}

IsNullExpr::~IsNullExpr() { delete value; }

IsNullExpr::IsNullExpr(Expr *value)
{
    dataType = BooleanType();

    this->value = value;
}

ExprType IsNullExpr::GetType() const { return ExprType::IS_NULL_E; }

FuncExpr::FuncExpr() : function(nullptr) {}

FuncExpr::~FuncExpr() { DeleteExprs(arguments); }

FuncExpr::FuncExpr(const std::string &fnName, const std::vector<Expr *> &args, DataTypePtr returnType)
    : funcName(fnName), arguments(args), functionType(BUILTIN)
{
    dataType = std::move(returnType);

    std::vector<DataTypeId> argTypes(arguments.size());
    std::transform(arguments.begin(), arguments.end(), argTypes.begin(),
                   [](Expr *expr) -> DataTypeId { return expr->GetReturnTypeId(); });
    auto signature = FunctionSignature(funcName, argTypes, dataType->GetId());
    this->function = FunctionRegistry::LookupFunction(&signature);
}

FuncExpr::FuncExpr(const std::string &fnName, const std::vector<Expr *> &args, DataTypePtr returnType,
                   const Function *function)
    : funcName(fnName), arguments(args), function(function), functionType(BUILTIN)
{
    dataType = std::move(returnType);
}

FuncExpr::FuncExpr(const std::string &fnName, const std::vector<Expr *> &args, DataTypePtr returnType,
                   ExprFunctionType functionType)
    : funcName(fnName), arguments(args), function(nullptr), functionType(functionType)
{
    dataType = std::move(returnType);
}

ExprType FuncExpr::GetType() const { return ExprType::FUNC_E; }
} // namespace expressions
} // namespace omniruntime
