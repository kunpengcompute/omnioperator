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
 * Description: parser function
 */

#include "parser.h"
#include <iostream>

using namespace std;
using namespace omniruntime::expressions;
using namespace omniruntime::type;

Parser::Parser() {}

Parser::~Parser() {}

namespace {
const string OPERATOR_PREFIX = "$operator$";
const int32_t SUBSTR_LEN = 10;
const int32_t ARG2 = 2;
} // namespace

// Helper function to remove operator prefix if it is there
string DemangleOperator(string opStr)
{
    if (opStr.size() > SUBSTR_LEN && opStr.substr(0, SUBSTR_LEN) == OPERATOR_PREFIX) {
        return opStr.substr(SUBSTR_LEN);
    }
    return opStr;
}

OperatorType GetBinaryOperatorType(string opStr)
{
    vector<string> allCmpOps{"LESS_THAN", "LESS_THAN_OR_EQUAL", "GREATER_THAN", "GREATER_THAN_OR_EQUAL",
                             "EQUAL",     "NOT_EQUAL"};
    vector<string> allLogOps{"AND", "OR"};
    vector<string> allArithOps{"ADD", "SUBTRACT", "MULTIPLY", "DIVIDE", "MODULUS"};
    for (const string &cmpOp : allCmpOps) {
        if (opStr == cmpOp) {
            return OperatorType::COMPARISON;
        }
    }
    for (const string &logOp : allLogOps) {
        if (opStr == logOp) {
            return OperatorType::LOGICAL;
        }
    }
    for (const string &arithOp : allArithOps) {
        if (opStr == arithOp) {
            return OperatorType::ARITHMETIC;
        }
    }
    return OperatorType::INVALIDOPTYPE;
}

bool IsUnaryOperator(const string &opStr)
{
    vector<string> allUnaryOps{"NOT", "not"};
    for (const string &unaryOp : allUnaryOps) {
        if (opStr == unaryOp) {
            return true;
        }
    }
    return false;
}

string Parser::StripString(const string &input)
{
    // remove spaces from input but not from inside strings
    string newInput;
    bool isInString = false;
    for (char i : input) {
        if (i == '\'') {
            isInString = !isInString;
            newInput.push_back(i);
        } else if (i == ' ') {
            if (isInString) {
                newInput.push_back(i);
            }
        } else {
            newInput.push_back(i);
        }
    }
    return newInput;
}

DataTypeId ParseReturnType(const string &typeString)
{
    int endIdx = 2;
    auto widthIdx = typeString.find('[');
    if (widthIdx != string::npos) {
        if (stoi(typeString.substr(0, endIdx)) == OMNI_CHAR) {
            return OMNI_CHAR;
        }
    }
    if (typeString.find_first_not_of("0123456789") == string::npos && stoi(typeString) < INT32_MAX) {
        int typeOrdinal = stoi(typeString);
        return static_cast<DataTypeId>(typeOrdinal);
    }
    LogError("Invalid return type: %s", typeString.c_str());
    return OMNI_INVALID;
}

std::vector<omniruntime::expressions::Expr *>
Parser::ParseExpressions(const string expressions[], int32_t numberOfExpressions, DataTypes &inputTypes)
{
    std::vector<Expr *> vExprs;
    for (int32_t i = 0; i < numberOfExpressions; i++) {
        Expr *expr = ParseRowExpression(expressions[i], inputTypes, inputTypes.GetSize());
        if (expr == nullptr) {
            Expr::DeleteExprs(vExprs);
            return {};
        }
        vExprs.push_back(expr);
    }
    return vExprs;
}

Expr *Parser::ParseRowExpression(const string &inputStr, DataTypes &inputTypes, int32_t vecCount)
{
    string input = this->StripString(inputStr);
    auto firstParenInd = input.find('(');
    // Check if it is just data (i.e. 123, #4, 34.4)
    if (firstParenInd == string::npos) {
        if (input[0] == '#') {
            return GenerateFieldExpr(input, inputTypes);
        } else {
            return GenerateLiteralExpr(input);
        }
    }

    // demangled operator string
    string opStr = DemangleOperator(input.substr(0, firstParenInd));
    string exprStr = input.substr(firstParenInd + 1, input.size() - firstParenInd - 1 - 1);

    // ensure that strings and parentheses are respected
    vector<int> commaPositions; // indices of commas in exprStr
    int numCommas = 0;
    int parenCount = 0;
    bool outsideQuotes = true;
    for (uint32_t i = 0; i < exprStr.size(); i++) {
        if (exprStr[i] == ',' && parenCount == 0 && outsideQuotes) {
            commaPositions.push_back(i);
            numCommas++;
        }
        if (exprStr[i] == '\'') {
            outsideQuotes = !outsideQuotes;
        }
        if (exprStr[i] == '(') {
            parenCount++;
        }
        if (exprStr[i] == ')') {
            parenCount--;
        }
    }
    commaPositions.push_back(exprStr.size());

    // Place all of the arguments into a vector first
    vector<Expr *> args;
    auto expr = ParseRowExpression(exprStr.substr(0, commaPositions[0]), inputTypes, vecCount);
    if (expr == nullptr) {
        return nullptr;
    }
    args.push_back(expr);
    for (int i = 1; i <= numCommas; i++) {
        string currVal = exprStr.substr(commaPositions[i - 1] + 1, commaPositions[i] - commaPositions[i - 1] - 1);
        expr = ParseRowExpression(currVal, inputTypes, vecCount);
        if (expr == nullptr) {
            Expr::DeleteExprs(args);
            return nullptr;
        }
        args.push_back(expr);
    }

    return ParseRowExpressionHelper(opStr, args);
}

Expr *Parser::ParseRowExpressionHelper(string opStr, vector<Expr *> args)
{
    auto typeIdx = opStr.find(':');
    int stepSize = 4;
    int32_t width = INT32_MAX;
    omniruntime::type::DataTypePtr type;
    DataTypeId typeId;
    if (typeIdx != string::npos) {
        typeId = ParseReturnType(opStr.substr(typeIdx + 1));
        if (typeId == OMNI_CHAR) {
            width = stoi(opStr.substr(typeIdx + stepSize, opStr.size() - typeIdx - stepSize));
            type = std::make_shared<CharDataType>(width);
        } else {
            type = std::make_shared<DataType>(typeId);
        }
        opStr = opStr.substr(0, typeIdx);
    }

    // BinaryExpr
    OperatorType binRetType = GetBinaryOperatorType(opStr);
    if (binRetType != OperatorType::INVALIDOPTYPE && args.size() == ARG2) {
        return new BinaryExpr(StringToOperator(DemangleOperator(opStr)), args[0], args[1], std::move(type));
    }

    // UnaryExpr
    // only handling NOT for now
    if (IsUnaryOperator(opStr) && args.size() == 1) {
        return new UnaryExpr(StringToOperator(DemangleOperator(opStr)), args[0], std::move(type));
    }

    // Special form
    // Special forms are IN, BETWEEN, IF, COALESCE
    if (opStr == "BETWEEN") {
        return new BetweenExpr(args[0], args[1], args[ARG2]);
    }
    if (opStr == "IN") {
        return new InExpr(args);
    }
    if (opStr == "COALESCE") {
        return new CoalesceExpr(args[0], args[1]);
    }
    if (opStr == "IF") {
        if (TypeUtil::IsStringType(args[ARG2]->GetReturnTypeId()) && args[ARG2]->GetType() == ExprType::LITERAL_E &&
            static_cast<LiteralExpr *>(args[ARG2])->stringVal->compare("null") == 0) {
            return new IfExpr(args[0], args[1], ParserHelper::GetDefaultValueForType(args[1]->GetReturnTypeId()));
        }
        return new IfExpr(args[0], args[1], args[ARG2]);
    }
    if (opStr == "IS_NULL") {
        return new IsNullExpr(args[0]);
    }
    if (opStr == "IS_NOT_NULL") {
        auto isNullExpr = new IsNullExpr(args[0]);
        return new UnaryExpr(Operator::NOT, isNullExpr, std::move(type));
    }
    // When casting to the same type, the result is the argument itself
    // Treat argument as constant DataExpr instead of returning FuncExpr
    if (opStr == "CAST" && args.size() == 1 && (typeId == args[0]->GetReturnTypeId())) {
        if (args[0]->GetType() == ExprType::LITERAL_E) {
            return static_cast<LiteralExpr *>(args[0]);
        } else if (args[0]->GetType() == ExprType::FIELD_E) {
            return static_cast<FieldExpr *>(args[0]);
        } else {
            Expr::DeleteExprs(args);
            return nullptr;
        }
    }

    // Function
    // Check that the signature matches
    vector<DataTypeId> argTypes(args.size());
    std::transform(args.begin(), args.end(), argTypes.begin(),
                   [](Expr *expr) -> DataTypeId { return expr->GetReturnTypeId(); });
    for (size_t i = 0; i < argTypes.size(); i++) {
        if (argTypes[i] == omniruntime::type::OMNI_DATE32) {
            argTypes[i] = omniruntime::type::OMNI_INT;
        }
    }
    auto signature = FunctionSignature(opStr, argTypes, type->GetId());
    auto function = omniruntime::codegen::FunctionRegistry::LookupFunction(&signature);
    if (function != nullptr) {
        return new FuncExpr(opStr, args, std::move(type), function);
    }

    // No expression can be matched
    LogWarn("operator is not supported: %s", opStr.c_str());
    Expr::DeleteExprs(args);
    return nullptr;
}

// Helper function to turn all % to .* for regex wildcard matching
string *FixString(const string &dataStr)
{
    auto *fixedStr = new string("");
    for (char i : dataStr) {
        if (i == '%') {
            fixedStr->push_back('.');
            fixedStr->push_back('*');
        } else {
            fixedStr->push_back(i);
        }
    }
    return fixedStr;
}

LiteralExpr *Parser::GenerateLiteralExprHelper(const string &literalStr, DataTypePtr currType)
{
    switch (currType->GetId()) {
        // handle boolean as int32
        case OMNI_BOOLEAN: {
            return new LiteralExpr(stoi(literalStr), std::move(currType));
        }
        case OMNI_BYTE: {
            return new LiteralExpr(static_cast<int8_t>(stoi(literalStr)), std::move(currType));
        }
        case OMNI_SHORT: {
            return new LiteralExpr(static_cast<int16_t>(stoi(literalStr)), std::move(currType));
        }
        case OMNI_INT:
        case OMNI_DATE32: {
            LiteralExpr *e = new LiteralExpr(stoi(literalStr), std::move(currType));
            e->longVal = e->intVal;
            e->doubleVal = e->intVal;
            return e;
        }
        // Need to handle decimals properly
        case OMNI_DECIMAL128: {
            string *dec128String = new string(literalStr);
            return new LiteralExpr(dec128String, std::move(currType));
        }
        case OMNI_DECIMAL64:
        case OMNI_TIMESTAMP:
        case OMNI_LONG: {
            return new LiteralExpr(stol(literalStr), std::move(currType));
        }
        case OMNI_DOUBLE: {
            return new LiteralExpr(stod(literalStr), std::move(currType));
        }
        case OMNI_CHAR:
        case OMNI_VARCHAR: {
            return new LiteralExpr(FixString(literalStr), std::move(currType));
        }
        case OMNI_NONE: {
            return new LiteralExpr(0, std::move(currType));
        }
        default: {
            LogError("type %u is not supported", currType->GetId());
            return nullptr;
        }
    }
}

FieldExpr *Parser::GenerateFieldExpr(string fieldStr, const DataTypes &inputTypes)
{
    int colIdx = stoi(fieldStr.substr(1));
    const DataTypePtr &colType = inputTypes.GetType(colIdx);
    return new FieldExpr(colIdx, colType);
}

LiteralExpr *Parser::GenerateLiteralExpr(string literalStr)
{
    auto typeIdx = literalStr.find(':');
    int stepSize = 4;
    int32_t width = INT32_MAX;
    DataTypePtr currType;
    DataTypeId currTypeId;
    if (typeIdx != string::npos) {
        currTypeId = ParseReturnType(literalStr.substr(typeIdx + 1));
        if (currTypeId == OMNI_CHAR) {
            width = stoi(literalStr.substr(typeIdx + stepSize, literalStr.size() - typeIdx - stepSize));
        }
        literalStr = literalStr.substr(0, typeIdx);
    } else {
        LogError("Unknown constant type for expr: %s", literalStr.c_str());
        return nullptr;
    }

    // Case with boolean true/false
    if (literalStr == "true" || literalStr == "false") {
        currType = BooleanType();
        return new LiteralExpr(literalStr == "true", std::move(currType));
    }

    // trim the single quotes for string values if there is any
    if (TypeUtil::IsStringType(currTypeId) && literalStr[0] == '\'' && literalStr[literalStr.size() - 1] == '\'') {
        literalStr = literalStr.substr(1, literalStr.size() - 1 - 1);
    }

    // case with null constants
    if (IsNullLiteral(literalStr)) {
        auto expr = ParserHelper::GetDefaultValueForType(currTypeId);
        expr->isNull = true;
        return expr;
    }

    if (TypeUtil::IsStringType(currTypeId)) {
        if (currTypeId == OMNI_CHAR) {
            currType = std::make_shared<CharDataType>(width);
        } else {
            currType = std::make_shared<VarcharDataType>(width);
        }
    } else {
        currType = std::make_shared<DataType>(currTypeId);
    }

    // Case with regular data (int, long, double, string ...)
    return GenerateLiteralExprHelper(literalStr, std::move(currType));
}
