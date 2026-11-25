/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 *
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
        case Operator::EQ: return "Cmp:" + typeStr + "(EQ ";
        case Operator::NEQ: return "Cmp:" + typeStr + "(NEQ ";
        case Operator::LT: return "Cmp:" + typeStr + "(LT ";
        case Operator::LTE: return "Cmp:" + typeStr + "(LTE ";
        case Operator::GT: return "Cmp:" + typeStr + "(GT ";
        case Operator::GTE: return "Cmp:" + typeStr + "(GTE ";
        case Operator::AND: return "Bin:" + typeStr + "(AND ";
        case Operator::OR: return "Bin:" + typeStr + "(OR ";
        case Operator::ADD: return "Arith:" + typeStr + "(ADD ";
        case Operator::SUB: return "Arith:" + typeStr + "(SUB ";
        case Operator::MUL: return "Arith:" + typeStr + "(MUL ";
        case Operator::DIV: return "Arith:" + typeStr + "(DIV ";
        case Operator::MOD: return "Arith:" + typeStr + "(MOD ";
        default: return "Invalid";
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

std::string GetBoolValOutput(const LiteralExpr &e)
{
    string output = "Literal:bool:";
    e.boolVal ? output += "true" : output += "false";
    return output;
}

std::string GetShortValOutput(const LiteralExpr &e)
{
    string output = "Literal:" + TypeUtil::TypeToString(e.GetReturnTypeId()) + ":" + to_string(e.shortVal);
    return output;
}

std::string GetByteValOutput(const LiteralExpr &e)
{
    string output = "Literal:" + TypeUtil::TypeToString(e.GetReturnTypeId()) + ":" + to_string(e.byteVal);
    return output;
}

std::string GetIntValOutput(const LiteralExpr &e)
{
    string output = "Literal:" + TypeUtil::TypeToString(e.GetReturnTypeId()) + ":" + to_string(e.intVal);
    return output;
}

std::string GetLongValOutput(const LiteralExpr &e)
{
    string output = "Literal:" + TypeUtil::TypeToString(e.GetReturnTypeId()) + ":" + to_string(e.longVal);
    return output;
}

std::string GetDoubleValOutput(const LiteralExpr &e)
{
    string output = "Literal:" + TypeUtil::TypeToString(e.GetReturnTypeId()) + ":" + to_string(e.doubleVal);
    return output;
}

std::string GetCharValOutput(const LiteralExpr &e)
{
    string output = "Literal:";
    if (e.GetReturnTypeId() == OMNI_CHAR) {
        // meant to look like "%s[%d]:'%s'"
        output += TypeUtil::TypeToString(e.GetReturnTypeId()) + +"[" +
                  to_string(static_cast<CharDataType *>(e.dataType.get())->GetWidth()) + "]" + ":'" + *(e.stringVal) +
                  "'";
    } else {
        std::string tmp = e.stringVal == nullptr ? "null" : *e.stringVal;
        // meant to look like "%s:'%s'"
        output += TypeUtil::TypeToString(e.GetReturnTypeId()) + ":'" + tmp + "'";
    }
    return output;
}

std::string GetDecimal64ValOutput(const LiteralExpr &e)
{
    // meant to look like "Literal:%s(%d, %d):%ld"
    string output = "Literal:";
    output += TypeUtil::TypeToString(e.GetReturnTypeId());
    output += "(";
    output += to_string(static_cast<Decimal64DataType *>(e.dataType.get())->GetPrecision());
    output += ", ";
    output += to_string(static_cast<Decimal64DataType *>(e.dataType.get())->GetScale());
    output += "):";
    output += to_string(e.longVal);
    return output;
}

std::string GetDecimal128ValOutput(const LiteralExpr &e)
{
    // meant to look like "%s(%d, %d):'%s'"
    string output = "Literal:";
    output += TypeUtil::TypeToString(e.GetReturnTypeId());
    output += "(";
    output += to_string(static_cast<Decimal128DataType *>(e.dataType.get())->GetPrecision());
    output += ", ";
    output += to_string(static_cast<Decimal128DataType *>(e.dataType.get())->GetScale());
    output += "):";
    output += "'";
    output += *(e.stringVal);
    output += "'";
    return output;
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
    printf("%s\n", message.c_str());

    this->indentationDepth++;
    (e.left)->Accept(*this);

    (e.right)->Accept(*this);
    string lastParentheses = indent + ")";
    printf("%s\n", lastParentheses.c_str());
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
        case Operator::NOT: output += "Unary:" + TypeUtil::TypeToString(e.GetReturnTypeId()) + "(NOT "; break;
        default: output += "InvalidUnaryOperator:" + to_string(static_cast<int32_t>(e.op)) + "("; break;
    }
    printf("%s\n", output.c_str());
    this->indentationDepth++;
    (e.exp)->Accept(*this);
    string lastParentheses = indent + ")";
    printf("%s\n", lastParentheses.c_str());
    this->indentationDepth--;
}

void ExprPrinter::Visit(const LiteralExpr &e)
{
    string output = GenerateIndentation();
    switch (e.GetReturnTypeId()) {
        case OMNI_BOOLEAN: output += GetBoolValOutput(e); break;
        case OMNI_BYTE: output += GetByteValOutput(e); break;
        case OMNI_SHORT: output += GetShortValOutput(e); break;
        case OMNI_INT:
        case OMNI_DATE32: output += GetIntValOutput(e); break;
        case OMNI_TIMESTAMP:
        case OMNI_LONG: output += GetLongValOutput(e); break;
        case OMNI_DOUBLE: output += GetDoubleValOutput(e); break;
        case OMNI_CHAR: output += GetCharValOutput(e); break;
        case OMNI_VARCHAR: output += GetCharValOutput(e); break;
        case OMNI_DECIMAL64: output += GetDecimal64ValOutput(e); break;
        case OMNI_DECIMAL128: output += GetDecimal128ValOutput(e); break;
        default: output += "Literal:invalid DataType " + to_string(e.GetReturnTypeId());
    }
    printf("%s\n", output.c_str());
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
    printf("%s\n", output.c_str());
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
    printf("%s\n", output.c_str());
    this->indentationDepth++;
    for (uint32_t i = 0; i < e.arguments.size(); i++) {
        (e.arguments[i])->Accept(*this);
        if (i == e.arguments.size() - 1) {
            printf("%s\n", (indent + ")").c_str());
        }
    }
    this->indentationDepth--;
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
    printf("%s\n", output.c_str());
    this->indentationDepth++;
    for (const auto &i : e.whenClause) {
        (i.first)->Accept(*this);
        (i.second)->Accept(*this);
    }
    e.falseExpr->Accept(*this);
    printf("%s\n", (indent + ")").c_str());
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
    printf("%s\n", output.c_str());
    this->indentationDepth++;
    (e.value)->Accept(*this);

    (e.lowerBound)->Accept(*this);

    (e.upperBound)->Accept(*this);
    printf("%s\n", (indent + ")").c_str());
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
    printf("%s\n", output.c_str());
    this->indentationDepth++;
    e.condition->Accept(*this);

    e.trueExpr->Accept(*this);

    e.falseExpr->Accept(*this);
    printf("%s\n", (indent + ")").c_str());
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
    printf("%s\n", output.c_str());
    this->indentationDepth++;
    e.value1->Accept(*this);

    e.value2->Accept(*this);
    printf("%s\n", (indent + ")").c_str());
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
    printf("%s\n", output.c_str());
    this->indentationDepth++;
    e.value->Accept(*this);
    printf("%s\n", (indent + ")").c_str());
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
    printf("%s\n", output.c_str());
    this->indentationDepth++;
    for (uint32_t i = 0; i < e.arguments.size(); i++) {
        (e.arguments[i])->Accept(*this);
        if (i == e.arguments.size() - 1) {
            printf("%s\n", (indent + ")").c_str());
        }
    }
    this->indentationDepth--;
}
