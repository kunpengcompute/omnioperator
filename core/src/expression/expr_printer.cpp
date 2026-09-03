/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
 * Description: print expression tree methods
 */
#include "expr_printer.h"
#include <cstdio>
#include <iostream>
#include <string>
#include <algorithm>
#include <map>
#include "util/debug.h"

using namespace omniruntime::expressions;
using namespace omniruntime::type;
using namespace std;

string ExprPrinter::BinaryExprPrinterHelper(const Operator &op, const DataType &type) const
{
    string typeStr = TypeUtil::TypeToString(type.GetId());
    if (TypeUtil::IsDecimalType(type.GetId())) {
        typeStr += "(";
        typeStr += to_string(static_cast<const DecimalDataType &>(type).GetPrecision());
        typeStr += ", ";
        typeStr += to_string(static_cast<const DecimalDataType &>(type).GetScale());
        typeStr += ")";
    }

    switch (op) {
        case Operator::EQ:
            return "Cmp:" + typeStr + "(EQ ";
        case Operator::NEQ:
            return "Cmp:" + typeStr + "(NEQ ";
        case Operator::LT:
            return "Cmp:" + typeStr + "(LT ";
        case Operator::LTE:
            return "Cmp:" + typeStr + "(LTE ";
        case Operator::GT:
            return "Cmp:" + typeStr + "(GT ";
        case Operator::GTE:
            return "Cmp:" + typeStr + "(GTE ";
        case Operator::AND:
            return "Bin:" + typeStr + "(AND ";
        case Operator::OR:
            return "Bin:" + typeStr + "(OR ";
        case Operator::ADD:
            return "Arith:" + typeStr + "(ADD ";
        case Operator::SUB:
            return "Arith:" + typeStr + "(SUB ";
        case Operator::MUL:
            return "Arith:" + typeStr + "(MUL ";
        case Operator::DIV:
            return "Arith:" + typeStr + "(DIV ";
        case Operator::MOD:
            return "Arith:" + typeStr + "(MOD ";
        default:
            return "Invalid";
    }
}

string ExprPrinter::GenerateIndentation() const
{
    string indent = "";
    for (int i = 0; i < this->indentationDepth; i++) {
        indent.append("\t");
    }
    return indent;
}

/*
 * EXAMPLE
 *
 * Bin:bool(OR,
 * Cmp:bool(EQ,
 * #7,
 * 'Winter
 * ),
 * Cmp:bool(EQ,
 * #2,
 * 'Summer'
 * )
 * )
 *
 */
void ExprPrinter::Visit(const BinaryExpr &e)
{
    string indent = GenerateIndentation();
    string message = BinaryExprPrinterHelper(e.op, *(e.GetReturnType()));
    if (message == "Invalid") {
        message = "InvalidBinaryOperator:" + to_string(static_cast<int32_t>(e.op)) + "(";
    }
    message = indent + message;
    std::cout << message << '\n';

    this->indentationDepth++;
    (e.left)->Accept(*this);

    (e.right)->Accept(*this);
    string lastParentheses = indent + ")";
     std::cout << lastParentheses << '\n';
    this->indentationDepth--;
}

/*
 * EXAMPLE
 *
 * Unary:bool(NOT,
 * IsNull:bool(
 * #0
 * )
 * )
 *
 */
void ExprPrinter::Visit(const UnaryExpr &e)
{
    string indent = GenerateIndentation();
    string output = indent;
    switch (e.op) {
        case Operator::NOT:
            output += "Unary:" + TypeUtil::TypeToString(e.GetReturnTypeId()) + "(NOT ";
            break;
        case Operator::NEG:
            output += "Unary:" + TypeUtil::TypeToString(e.GetReturnTypeId()) + "(NEGATIVE ";
            break;
        case Operator::POS:
            output += "Unary:" + TypeUtil::TypeToString(e.GetReturnTypeId()) + "(POSITIVE ";
            break;
        default:
            output += "InvalidUnaryOperator:" + to_string(static_cast<int32_t>(e.op)) + "(";
            break;
    }
    std::cout << output << '\n';
    this->indentationDepth++;
    (e.exp)->Accept(*this);
    string lastParentheses = indent + ")";
    std::cout << lastParentheses << '\n';
    this->indentationDepth--;
}

void ExprPrinter::Visit(const LiteralExpr &e)
{
    string output = GenerateIndentation();
    if(e.isNull){
        output += "Literal:isnull " + to_string(e.GetReturnTypeId());
        std::cout << output << '\n';
        return;
    }
    switch (e.GetReturnTypeId()) {
        case OMNI_BOOLEAN:
            output += GetBoolValOutput(e);
            break;
        case OMNI_BYTE:
            output += GetByteValOutput(e);
            break;
        case OMNI_SHORT:
            output += GetShortValOutput(e);
            break;
        case OMNI_INT:
        case OMNI_DATE32:
            output += GetIntValOutput(e);
            break;
        case OMNI_TIMESTAMP:
        case OMNI_LONG:
            output += GetLongValOutput(e);
            break;
        case OMNI_DOUBLE:
            output += GetDoubleValOutput(e);
            break;
        case OMNI_FLOAT:
            output += GetFloatValOutput(e);
            break;
        case OMNI_CHAR:
            output += GetCharValOutput(e);
            break;
        case OMNI_VARCHAR:
            output += GetCharValOutput(e);
            break;
        case OMNI_VARBINARY:
            output += GetCharValOutput(e);
            break;
        case OMNI_DECIMAL64:
            output += GetDecimal64ValOutput(e);
            break;
        case OMNI_DECIMAL128:
            output += GetDecimal128ValOutput(e);
            break;
        default:
            output += "Literal:invalid DataType " + to_string(e.GetReturnTypeId());
    }
    std::cout << output << '\n';
}

void ExprPrinter::Visit(const FieldExpr &e)
{
    string output = GenerateIndentation() + "Field:";
    output += TypeUtil::TypeToString(e.GetReturnTypeId());
    if (e.GetReturnTypeId() == OMNI_CHAR) {
        output += '[' + to_string(static_cast<CharDataType &>(*(e.GetReturnType())).GetWidth()) + ']';
    } else if (e.GetReturnTypeId() == OMNI_DECIMAL64 || e.GetReturnTypeId() == OMNI_DECIMAL128) {
        output += "(";
        output += to_string(static_cast<DecimalDataType *>(e.dataType.get())->GetPrecision());
        output += ", ";
        output += to_string(static_cast<DecimalDataType *>(e.dataType.get())->GetScale());
        output += ")";
    }
    output += ":#" + to_string(e.colVal);
    std::cout << output << '\n';
}

/*
 * EXAMPLE
 *
 * In:bool(
 * 1,
 * 2,
 * 3,
 * 4
 * )
 *
 */
void ExprPrinter::Visit(const InExpr &e)
{
    string indent = GenerateIndentation();
    string output = indent + "In:" + TypeUtil::TypeToString(e.GetReturnTypeId()) + "(";
    std::cout << output << '\n';
    this->indentationDepth++;
    for (uint32_t i = 0; i < e.arguments.size(); i++) {
        (e.arguments[i])->Accept(*this);
        if (i == e.arguments.size() - 1) {
            std::cout << indent + ")" << '\n';
        }
    }
    this->indentationDepth--;
}

void ExprPrinter::Visit(const InSubqueryExpr &e)
{
    string indent = GenerateIndentation();
    string output = indent + "InSubquery:" + TypeUtil::TypeToString(e.GetReturnTypeId()) + "(";
    std::cout << output << '\n';
    this->indentationDepth++;

    // Print probe value
    std::cout << GenerateIndentation() << "ProbeValue: ";
    e.value->Accept(*this);

    // Print subquery column index
    std::cout << GenerateIndentation() << "SubqueryColIdx: " << e.subqueryColIdx << '\n';

    this->indentationDepth--;
    std::cout << indent << ")" << '\n';
}

/*
 * Example:switch:bool(
 * Cmp:bool(EQ
 * #0,
 * 100,
 * ),
 * Cmp:bool(EQ
 * #0,
 * 200,
 * ),
 * Cmp:bool(),
 *
 *
 * )
 */
void ExprPrinter::Visit(const SwitchExpr &e)
{
    string indent = GenerateIndentation();
    string output = indent + "Switch:" + TypeUtil::TypeToString(e.GetReturnTypeId()) + "(";
    std::cout << output << '\n';
    this->indentationDepth++;
    for (const auto &i : e.whenClause) {
        (i.first)->Accept(*this);
        (i.second)->Accept(*this);
    }
    e.falseExpr->Accept(*this);
    std::cout << indent + ")" << '\n';
    this->indentationDepth--;
}
/*
 * EXAMPLE
 *
 * Between:bool(
 * 5,
 * -1,
 * 7,
 * )
 *
 */
void ExprPrinter::Visit(const BetweenExpr &e)
{
    string indent = GenerateIndentation();
    string output = indent + "Between:" + TypeUtil::TypeToString(e.GetReturnTypeId()) + "(";
    std::cout << output << '\n';
    this->indentationDepth++;
    (e.value)->Accept(*this);

    (e.lowerBound)->Accept(*this);

    (e.upperBound)->Accept(*this);
    std::cout << indent + ")" << '\n';
    this->indentationDepth--;
}

/*
 * EXAMPLE
 *
 * If:bool(
 * Cmp:bool(GT,
 * #0,
 * 100,
 * ),
 * Cmp:bool(GT,
 * #0,
 * 200
 * ),
 * Cmp:bool(LT,
 * #0,
 * 0
 * )
 * )
 *
 */
void ExprPrinter::Visit(const IfExpr &e)
{
    string indent = GenerateIndentation();
    string output = indent + "If:" + TypeUtil::TypeToString(e.GetReturnTypeId()) + "(";
    std::cout << output << '\n';
    this->indentationDepth++;
    e.condition->Accept(*this);

    e.trueExpr->Accept(*this);

    e.falseExpr->Accept(*this);
    std::cout << indent + ")" << '\n';
    this->indentationDepth--;
}

/*
 * EXAMPLE
 *
 * Cmp:bool(EQ,
 * Coalesce:int64(
 * #0,
 * 0
 * ),
 * 123
 * )
 *
 */
void ExprPrinter::Visit(const CoalesceExpr &e)
{
    string indent = GenerateIndentation();
    string output = indent + "Coalesce:" + TypeUtil::TypeToString(e.GetReturnTypeId()) + "(";
    std::cout << output << '\n';
    this->indentationDepth++;
    e.value1->Accept(*this);

    e.value2->Accept(*this);
    std::cout << indent + ")" << '\n';
    this->indentationDepth--;
}

/*
 * EXAMPLE
 *
 * Printed Pared Expression
 *
 * IsNull:bool(
 * #0
 * )
 *
 */
void ExprPrinter::Visit(const IsNullExpr &e)
{
    string indent = GenerateIndentation();
    string output = indent + "IsNull:" + TypeUtil::TypeToString(e.GetReturnTypeId()) + "(";
    std::cout << output << '\n';
    this->indentationDepth++;
    e.value->Accept(*this);
    std::cout << indent + ")" << '\n';
    this->indentationDepth--;
}

/*
 * EXAMPLE
 * concat:string(
 * #1,
 * #2
 * )
 *
 */
void ExprPrinter::Visit(const FuncExpr &e)
{
    string indent = GenerateIndentation();
    string typeStr = TypeUtil::TypeToString(e.GetReturnTypeId());
    if (TypeUtil::IsDecimalType(e.GetReturnTypeId())) {
        auto decimalDataType = static_cast<DecimalDataType *>(e.GetReturnType().get());
        typeStr += "(";
        typeStr += to_string(decimalDataType->GetPrecision());
        typeStr += ", ";
        typeStr += to_string(decimalDataType->GetScale());
        typeStr += ")";
    } else if (TypeUtil::IsStringType(e.GetReturnTypeId())) {
        typeStr += "[";
        typeStr += to_string(static_cast<VarcharDataType *>(e.GetReturnType().get())->GetWidth());
        typeStr += "]";
    }

    string output = indent + "Function:" + ":" + e.funcName + ":" + typeStr + "(";
    std::cout << output << '\n';
    this->indentationDepth++;
    for (uint32_t i = 0; i < e.arguments.size(); i++) {
        (e.arguments[i])->Accept(*this);
        if (i == e.arguments.size() - 1) {
            std::cout << indent + ")" << '\n';
        }
    }
    this->indentationDepth--;
}

void ExprPrinter::Visit(const ParamRefExpr &e)
{
    string indent = GenerateIndentation();
    string output = indent + "ParamRef:" + TypeUtil::TypeToString(e.GetReturnTypeId()) + "(";
    std::cout << output << '\n';
    std::cout<< e.paramName_<<'\n';
    std::cout << indent + ")" << '\n';
}

void ExprPrinter::Visit(const LambdaExpr &e)
{
    string indent = GenerateIndentation();
    string output = indent + "Lambda:" + TypeUtil::TypeToString(e.GetReturnTypeId()) + "(";
    std::cout << output << '\n';
    this->indentationDepth++;
    e.GetBody()->Accept(*this);
    std::cout << indent + ")" << '\n';
    this->indentationDepth--;
}