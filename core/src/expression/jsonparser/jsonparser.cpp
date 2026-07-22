/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2012-2018. All rights reserved.
 * Description:
 */
#include "jsonparser.h"
#include <algorithm>
#include <vector>
#include <fstream> // for testing
#include <cassert>
#include <limits>

using namespace std;
using namespace omniruntime::expressions;
using namespace omniruntime::type;

using Json = nlohmann::json;

Expr *JSONParser::ParseJSONFieldRef(const Json &jsonExpr)
{
    if (!jsonExpr.is_object() || !jsonExpr.contains("dataType") || !jsonExpr.contains("colVal")) {
        return nullptr;
    }

    auto typeId = static_cast<DataTypeId>(jsonExpr["dataType"].get<int32_t>());
    DataTypePtr retType;
    if (jsonExpr.find("input") != jsonExpr.end()) {
        int ordinal = jsonExpr["ordinal"].get<int32_t>();
        auto input = ParseJSONFieldRef(jsonExpr["input"]);
        retType = std::make_shared<DataType>(typeId);
        return new FieldExpr(-1, std::move(retType), ordinal, input);
    }
    auto colVal = jsonExpr["colVal"].get<int32_t>();
    if (TypeUtil::IsStringType(typeId)) {
        int width = jsonExpr.contains("width") ? jsonExpr["width"].get<int32_t>()
                                               : std::numeric_limits<int32_t>::max();
        if (typeId == OMNI_CHAR) {
            retType = std::make_shared<CharDataType>(width);
        } else if (typeId == OMNI_VARBINARY) {
            retType = std::make_shared<VarBinaryDataType>(width);
        } else {
            retType = std::make_shared<VarcharDataType>(width);
        }
    } else if (TypeUtil::IsDecimalType(typeId)) {
        if (!jsonExpr.contains("precision") || !jsonExpr.contains("scale")) {
            return nullptr;
        }
        int precision = jsonExpr["precision"].get<int32_t>();
        int scale = jsonExpr["scale"].get<int32_t>();
        if (typeId == OMNI_DECIMAL64) {
            retType = std::make_shared<Decimal64DataType>(precision, scale);
        } else {
            retType = std::make_shared<Decimal128DataType>(precision, scale);
        }
    } else {
        retType = std::make_shared<DataType>(typeId);
    }
    return new FieldExpr(colVal, std::move(retType));
}

Expr *JSONParser::ParseJSONLiteral(const Json &jsonExpr)
{
    auto typeId = static_cast<DataTypeId>(jsonExpr["dataType"].get<int32_t>());
    // Null check on Literals
    if (jsonExpr["isNull"].get<bool>()) {
        LiteralExpr *expr = nullptr;
        if (TypeUtil::IsDecimalType(typeId)) {
            auto precision = jsonExpr["precision"].get<int32_t>();
            auto scale = jsonExpr["scale"].get<int32_t>();
            expr = ParserHelper::GetDefaultValueForType(typeId, precision, scale);
        } else {
            expr = ParserHelper::GetDefaultValueForType(typeId);
        }

        if (expr == nullptr) {
            return nullptr;
        }
        expr->isNull = true;
        return expr;
    }
    // proceed with non-null value
    switch (typeId) {
        case OMNI_BOOLEAN: {
            bool boolVal = jsonExpr["value"].get<bool>();
            return new LiteralExpr(boolVal, std::make_shared<BooleanDataType>());
        }
        case OMNI_BYTE: {
            auto byteVal = jsonExpr["value"].get<int8_t>();
            return new LiteralExpr(byteVal, std::make_shared<ByteDataType>());
        }
        case OMNI_SHORT: {
            auto shortVal = jsonExpr["value"].get<int16_t>();
            return new LiteralExpr(shortVal, std::make_shared<ShortDataType>());
        }
        case OMNI_INT: {
            auto intVal = jsonExpr["value"].get<int32_t>();
            return new LiteralExpr(intVal, std::make_shared<IntDataType>());
        }
        case OMNI_DATE32: {
            auto intVal = jsonExpr["value"].get<int32_t>();
            return new LiteralExpr(intVal, std::make_shared<Date32DataType>());
        }
        case OMNI_LONG: {
            auto longVal = jsonExpr["value"].get<int64_t>();
            return new LiteralExpr(longVal, std::make_shared<LongDataType>());
        }
        case OMNI_TIMESTAMP: {
            auto timestampVal = jsonExpr["value"].get<int64_t>();
            return new LiteralExpr(timestampVal, std::make_shared<TimestampDataType>());
        }
        case OMNI_DOUBLE: {
            auto doubleVal = jsonExpr["value"].get<double>();
            return new LiteralExpr(doubleVal, std::make_shared<DoubleDataType>());
        }
        case OMNI_FLOAT: {
            auto floatVal = jsonExpr["value"].get<float>();
            return new LiteralExpr(floatVal, std::make_shared<FloatDataType>());
        }
        case OMNI_DECIMAL64: {
            auto decimalVal = jsonExpr["value"].get<int64_t>();
            return new LiteralExpr(decimalVal, std::make_shared<Decimal64DataType>(jsonExpr["precision"].get<int32_t>(),
                jsonExpr["scale"].get<int32_t>()));
        }
        case OMNI_DECIMAL128: {
            auto *dec128String = new string(jsonExpr["value"].get<string>());
            return new LiteralExpr(dec128String, std::make_shared<Decimal128DataType>(
                jsonExpr["precision"].get<int32_t>(), jsonExpr["scale"].get<int32_t>()));
        }
        case OMNI_CHAR: {
            auto *stringVal = new string(jsonExpr["value"].get<string>());
            auto width = jsonExpr["width"].get<int32_t>();
            return new LiteralExpr(stringVal, std::make_shared<CharDataType>(width));
        }
        case OMNI_VARCHAR: {
            auto *stringVal = new string(jsonExpr["value"].get<string>());
            auto width = jsonExpr["width"].get<int32_t>();
            return new LiteralExpr(stringVal, std::make_shared<VarcharDataType>(width));
        }
        default:
            return new LiteralExpr(0, std::make_shared<DataType>());
    }
}

Expr *JSONParser::ParseJSONBinary(const Json &jsonExpr)
{
    using Operator = omniruntime::expressions::Operator;
    Operator op = StringToOperator(jsonExpr["operator"].get<string>());
    if (op == Operator::INVALIDOP) {
        return nullptr;
    }

    DataTypePtr dataType = ParserHelper::GetReturnDataType(jsonExpr);
    if (dataType == nullptr) {
        return nullptr;
    }

    Expr *leftExpr = ParseJSON(jsonExpr["left"]);
    if (leftExpr == nullptr) {
        return nullptr;
    }
    Expr *rightExpr = ParseJSON(jsonExpr["right"]);
    if (rightExpr == nullptr) {
        delete leftExpr;
        return nullptr;
    }
    const auto leftDataId = leftExpr->GetReturnType()->GetId();
    const auto rightDataId = rightExpr->GetReturnType()->GetId();
    if (leftDataId == rightDataId) {
        return new BinaryExpr(op, leftExpr, rightExpr, std::move(dataType));
    } else if (!TypeUtil::IsDecimalType(leftDataId) || !TypeUtil::IsDecimalType(rightDataId)) {
        // Directly cast one decimal to another have the possibility of precision overflow.
        // We only do this if one is not decimal type
        const auto returnDataId = dataType->GetId();
        auto getCastFunc = [](Expr *expr, DataTypePtr returnType)->omniruntime::expressions::FuncExpr* {
            auto signature = FunctionSignature("CAST", {expr->GetReturnTypeId()}, returnType->GetId());
            auto function = omniruntime::codegen::FunctionRegistry::LookupFunction(&signature);
            if (function) {
                return new FuncExpr("CAST", {expr}, returnType, function);
            }
            throw std::runtime_error("No matching function found for signature: " + signature.ToString());
        };
        // 1. Not comparison op and return is decimal. Cast both left and right to return type
        if (!IsComparisonOperator(op) && TypeUtil::IsDecimalType(returnDataId)) {
            // Cast non-decimal operands to result decimal type
            Expr *leftExprCasted = leftDataId != returnDataId ? getCastFunc(leftExpr, dataType) : leftExpr;
            Expr *rightExprCasted = rightDataId != returnDataId ? getCastFunc(rightExpr, dataType) : rightExpr;
            return new BinaryExpr(op, leftExprCasted, rightExprCasted, dataType);
        } else if (!IsComparisonOperator(op) && !TypeUtil::IsDecimalType(returnDataId)) {
            // 2. Not comparison and none of them are decimal. Int-like types can be casted according to Id
            Expr *binaryExpr;
            if (leftDataId > rightDataId) {
                Expr *castFunc = getCastFunc(rightExpr, leftExpr->GetReturnType());
                binaryExpr = new BinaryExpr(op, leftExpr, castFunc, std::move(leftExpr->GetReturnType()));
            } else if (leftDataId < rightDataId) {
                Expr *castFunc = getCastFunc(leftExpr, rightExpr->GetReturnType());
                binaryExpr = new BinaryExpr(op, castFunc, rightExpr, std::move(rightExpr->GetReturnType()));
            }
            if (binaryExpr->GetReturnType()->GetId() != returnDataId) {
                return getCastFunc(binaryExpr, dataType);
            }
            return binaryExpr;
        } else if (IsComparisonOperator(op) && !TypeUtil::IsDecimalType(returnDataId)) {
            // 3. Comparison between int-like values. Similar as situation 1, but returnType is bool
            if (leftDataId > rightDataId) {
                Expr *castFunc = getCastFunc(rightExpr, leftExpr->GetReturnType());
                return new BinaryExpr(op, leftExpr, castFunc, std::move(dataType));
            } else {
                Expr *castFunc = getCastFunc(leftExpr, rightExpr->GetReturnType());
                return new BinaryExpr(op, castFunc, rightExpr, std::move(dataType));
            }
        }
        return nullptr;
    } else {
        throw std::runtime_error("Unsupported operation");
    }
    return nullptr;
}

Expr *JSONParser::ParseJSONUnary(const Json &jsonExpr)
{
    using Operator = omniruntime::expressions::Operator;
    Operator op = StringToOperator(jsonExpr["operator"].get<string>());
    if (op == Operator::INVALIDOP) {
        return nullptr;
    }

    Expr *expr = ParseJSON(jsonExpr["expr"]);
    if (expr == nullptr) {
        return nullptr;
    }

    DataTypePtr dataType = ParserHelper::GetReturnDataType(jsonExpr);
    if (dataType == nullptr) {
        delete expr;
        return nullptr;
    }
    return new UnaryExpr(op, expr, dataType);
}

Expr *JSONParser::ParseJSONIn(const Json &jsonExpr)
{
    std::vector<Expr *> args;
    for (const auto &item : jsonExpr["arguments"].items()) {
        Expr *arg = ParseJSON(item.value());
        if (arg != nullptr) {
            args.push_back(arg);
        } else {
            Expr::DeleteExprs(args);
            return nullptr;
        }
    }
    return new InExpr(args);
}

Expr *JSONParser::ParseJSONBetween(const Json &jsonExpr)
{
    Expr *val = ParseJSON(jsonExpr["value"]);
    if (val == nullptr) {
        return nullptr;
    }
    Expr *lowBoundExpr = ParseJSON(jsonExpr["lower_bound"]);
    if (lowBoundExpr == nullptr) {
        delete val;
        return nullptr;
    }
    Expr *upBoundExpr = ParseJSON(jsonExpr["upper_bound"]);
    if (upBoundExpr == nullptr) {
        delete val;
        delete lowBoundExpr;
        return nullptr;
    }

    return new BetweenExpr(val, lowBoundExpr, upBoundExpr);
}

static void DeleteWhenClause(const std::vector<std::pair<Expr *, Expr *>> &whenClause)
{
    for (auto iter = whenClause.begin(); iter != whenClause.end(); iter++) {
        delete iter->first;
        delete iter->second;
    }
}

Expr *JSONParser::ParseJSONSwitch(const Json &jsonExpr)
{
    using Operator = omniruntime::expressions::Operator;
    auto numOfCases = jsonExpr["numOfCases"].get<int32_t>();
    std::vector<std::pair<Expr *, Expr *>> whenClause;
    for (int32_t i = 0; i < numOfCases; i++) {
        Expr *left = ParseJSON(jsonExpr["input"]);
        if (left == nullptr) {
            DeleteWhenClause(whenClause);
            return nullptr;
        }
        Expr *right = ParseJSON(jsonExpr["Case" + std::to_string(i + 1)]["when"]);
        if (right == nullptr) {
            delete left;
            DeleteWhenClause(whenClause);
            return nullptr;
        }
        Expr *result = ParseJSON(jsonExpr["Case" + std::to_string(i + 1)]["result"]);
        if (result == nullptr) {
            delete left;
            delete right;
            DeleteWhenClause(whenClause);
            return nullptr;
        }
        auto *condition = new BinaryExpr(Operator::EQ, left, right, std::make_shared<BooleanDataType>());
        std::pair<Expr *, Expr *> when = make_pair(condition, result);
        whenClause.push_back(when);
    }

    Expr *elseExpr = ParseJSON(jsonExpr["else"]);
    if (elseExpr == nullptr) {
        DeleteWhenClause(whenClause);
        return nullptr;
    }
    if (TypeUtil::IsStringType(elseExpr->GetReturnTypeId()) && elseExpr->GetType() == ExprType::LITERAL_E &&
        static_cast<LiteralExpr *>(elseExpr)->stringVal->compare("null") == 0) {
        auto literalExpr = ParserHelper::GetDefaultValueForType(elseExpr->GetReturnTypeId());
        delete elseExpr;
        if (literalExpr == nullptr) {
            DeleteWhenClause(whenClause);
            return nullptr;
        }
        return new SwitchExpr(whenClause, literalExpr);
    }
    return new SwitchExpr(whenClause, elseExpr);
}

Expr *JSONParser::ParseJSONSwitchGeneral(const Json &jsonExpr)
{
    auto numOfCases = jsonExpr["numOfCases"].get<int32_t>();
    std::vector<std::pair<Expr *, Expr *>> whenClause;
    for (int32_t i = 0; i < numOfCases; i++) {
        Expr *right = ParseJSON(jsonExpr["Case" + std::to_string(i + 1)]["when"]);
        if (right == nullptr) {
            DeleteWhenClause(whenClause);
            return nullptr;
        }
        Expr *result = ParseJSON(jsonExpr["Case" + std::to_string(i + 1)]["result"]);
        if (result == nullptr) {
            // delete left;
            delete right;
            DeleteWhenClause(whenClause);
            return nullptr;
        }
        std::pair<Expr *, Expr *> when = make_pair(right, result);
        whenClause.push_back(when);
    }

    Expr *elseExpr = ParseJSON(jsonExpr["else"]);
    if (elseExpr == nullptr) {
        DeleteWhenClause(whenClause);
        return nullptr;
    }
    if (TypeUtil::IsStringType(elseExpr->GetReturnTypeId()) && elseExpr->GetType() == ExprType::LITERAL_E &&
        static_cast<LiteralExpr *>(elseExpr)->stringVal->compare("null") == 0) {
        auto literalExpr = ParserHelper::GetDefaultValueForType(elseExpr->GetReturnTypeId());
        delete elseExpr;
        if (literalExpr == nullptr) {
            DeleteWhenClause(whenClause);
            return nullptr;
        }
        return new SwitchExpr(whenClause, literalExpr);
    }
    return new SwitchExpr(whenClause, elseExpr);
}

Expr *JSONParser::ParseJSONIf(const Json &jsonExpr)
{
    Expr *cond = ParseJSON(jsonExpr["condition"]);
    if (cond == nullptr) {
        return nullptr;
    }
    Expr *trueExpr = ParseJSON(jsonExpr["if_true"]);
    if (trueExpr == nullptr) {
        delete cond;
        return nullptr;
    }
    Expr *falseExpr = ParseJSON(jsonExpr["if_false"]);
    if (falseExpr == nullptr) {
        delete cond;
        delete trueExpr;
        return nullptr;
    }
    if (TypeUtil::IsStringType(falseExpr->GetReturnTypeId()) && falseExpr->GetType() == ExprType::LITERAL_E &&
        static_cast<LiteralExpr *>(falseExpr)->stringVal->compare("null") == 0) {
        delete falseExpr;
        auto literalExpr = ParserHelper::GetDefaultValueForType(trueExpr->GetReturnTypeId());
        if (literalExpr == nullptr) {
            delete cond;
            delete trueExpr;
            return nullptr;
        }
        return new IfExpr(cond, trueExpr, literalExpr);
    }

    return new IfExpr(cond, trueExpr, falseExpr);
}

Expr *JSONParser::ParseJSONCoalesce(const Json &jsonExpr)
{
    Expr *val1 = ParseJSON(jsonExpr["value1"]);
    if (val1 == nullptr) {
        return nullptr;
    }
    Expr *val2 = ParseJSON(jsonExpr["value2"]);
    if (val2 == nullptr) {
        delete val1;
        return nullptr;
    }

    return new CoalesceExpr(val1, val2);
}

Expr *JSONParser::ParseJsonIsNull(const Json &jsonExpr)
{
    Expr *val = ParseJSON(jsonExpr["arguments"].at(0));
    if (val == nullptr) {
        return nullptr;
    }
    return new IsNullExpr(val);
}

Expr *JSONParser::ParseJsonIsNotNull(const Json &jsonExpr)
{
    using Operator = omniruntime::expressions::Operator;
    Expr *val = ParseJSON(jsonExpr["arguments"].at(0));
    auto isNullExpr = new IsNullExpr(val);
    return new UnaryExpr(Operator::NOT, isNullExpr, std::make_shared<BooleanDataType>());
}

namespace {
constexpr int32_t JSON_VALUE_NULL_BEHAVIOR = 0;
constexpr int32_t JSON_VALUE_ERROR_BEHAVIOR = 1;
constexpr int32_t JSON_VALUE_DEFAULT_BEHAVIOR = 2;
constexpr int32_t JSON_QUERY_WITHOUT_ARRAY_WRAPPER = 0;
constexpr int32_t JSON_QUERY_WITH_CONDITIONAL_ARRAY_WRAPPER = 1;
constexpr int32_t JSON_QUERY_WITH_UNCONDITIONAL_ARRAY_WRAPPER = 2;
constexpr int32_t JSON_QUERY_NULL_BEHAVIOR = 0;
constexpr int32_t JSON_QUERY_EMPTY_ARRAY_BEHAVIOR = 1;
constexpr int32_t JSON_QUERY_EMPTY_OBJECT_BEHAVIOR = 2;
constexpr int32_t JSON_QUERY_ERROR_BEHAVIOR = 3;

LiteralExpr *CreateJsonValueBehaviorLiteral(int32_t behavior)
{
    return new LiteralExpr(behavior, std::make_shared<IntDataType>());
}

LiteralExpr *CreateJsonQueryWrapperLiteral(int32_t wrapperBehavior)
{
    return new LiteralExpr(wrapperBehavior, std::make_shared<IntDataType>());
}

LiteralExpr *CreateJsonQueryBehaviorLiteral(int32_t behavior)
{
    return new LiteralExpr(behavior, std::make_shared<IntDataType>());
}

LiteralExpr *CreateNullableVarcharLiteral()
{
    auto *expr = new LiteralExpr(new string(""), std::make_shared<VarcharDataType>(std::numeric_limits<int32_t>::max()));
    expr->isNull = true;
    return expr;
}

LiteralExpr *CreateVarcharLiteral(const std::string &value)
{
    auto width = std::max<int32_t>(1, static_cast<int32_t>(value.size()));
    return new LiteralExpr(new string(value), std::make_shared<VarcharDataType>(width));
}

Expr *CreateJsonValueDefaultExpr(const Json &defaultValueJson)
{
    if (!defaultValueJson.is_object() || !defaultValueJson.contains("exprType")) {
        return nullptr;
    }
    if (defaultValueJson["exprType"].get<string>() != "LITERAL") {
        Expr *defaultExpr = JSONParser::ParseJSON(defaultValueJson);
        if (defaultExpr == nullptr) {
            return nullptr;
        }
        if (defaultExpr->GetReturnTypeId() == OMNI_VARCHAR) {
            return defaultExpr;
        }
        if (defaultExpr->GetReturnTypeId() == OMNI_CHAR && defaultExpr->GetType() == ExprType::LITERAL_E) {
            auto literalExpr = static_cast<LiteralExpr *>(defaultExpr);
            auto *converted = CreateVarcharLiteral(literalExpr->stringVal == nullptr ? "" : *literalExpr->stringVal);
            converted->isNull = literalExpr->isNull;
            delete defaultExpr;
            return converted;
        }
        delete defaultExpr;
        return nullptr;
    }
    if (defaultValueJson["isNull"].get<bool>()) {
        return CreateNullableVarcharLiteral();
    }

    auto typeId = static_cast<DataTypeId>(defaultValueJson["dataType"].get<int32_t>());
    switch (typeId) {
        case OMNI_CHAR:
        case OMNI_VARCHAR:
            return CreateVarcharLiteral(defaultValueJson["value"].get<string>());
        case OMNI_BOOLEAN:
            return CreateVarcharLiteral(defaultValueJson["value"].get<bool>() ? "true" : "false");
        case OMNI_BYTE:
        case OMNI_SHORT:
        case OMNI_INT:
        case OMNI_DATE32:
            return CreateVarcharLiteral(std::to_string(defaultValueJson["value"].get<int32_t>()));
        case OMNI_LONG:
        case OMNI_TIMESTAMP:
            return CreateVarcharLiteral(std::to_string(defaultValueJson["value"].get<int64_t>()));
        case OMNI_DOUBLE:
            return CreateVarcharLiteral(defaultValueJson["value"].dump());
        default:
            return nullptr;
    }
}

bool ParseJsonValueBehavior(const Json &jsonExpr, const char *behaviorKey, int32_t *behaviorCode, Expr **defaultExpr)
{
    *behaviorCode = JSON_VALUE_NULL_BEHAVIOR;
    *defaultExpr = CreateNullableVarcharLiteral();
    if (!jsonExpr.contains(behaviorKey)) {
        return true;
    }

    const auto &behaviorJson = jsonExpr[behaviorKey];
    if (!behaviorJson.is_object() || !behaviorJson.contains("type")) {
        return false;
    }

    const auto behaviorType = behaviorJson["type"].get<string>();
    if (behaviorType == "NULL") {
        return true;
    }
    if (behaviorType == "ERROR") {
        *behaviorCode = JSON_VALUE_ERROR_BEHAVIOR;
        return true;
    }
    if (behaviorType != "DEFAULT") {
        return false;
    }

    *behaviorCode = JSON_VALUE_DEFAULT_BEHAVIOR;
    delete *defaultExpr;
    *defaultExpr = CreateNullableVarcharLiteral();
    if (!behaviorJson.contains("defaultValue")) {
        return true;
    }

    Expr *parsedDefaultExpr = CreateJsonValueDefaultExpr(behaviorJson["defaultValue"]);
    if (parsedDefaultExpr == nullptr) {
        return false;
    }
    delete *defaultExpr;
    *defaultExpr = parsedDefaultExpr;
    return true;
}

bool BuildJsonValueArguments(const Json &jsonExpr, std::vector<Expr *> *args)
{
    for (const auto &item : jsonExpr["arguments"].items()) {
        Expr *arg = JSONParser::ParseJSON(item.value());
        if (arg != nullptr) {
            args->push_back(arg);
        } else {
            return false;
        }
    }

    int32_t emptyBehavior = JSON_VALUE_NULL_BEHAVIOR;
    int32_t errorBehavior = JSON_VALUE_NULL_BEHAVIOR;
    Expr *defaultOnEmpty = nullptr;
    Expr *defaultOnError = nullptr;
    if (!ParseJsonValueBehavior(jsonExpr, "emptyBehavior", &emptyBehavior, &defaultOnEmpty)) {
        delete defaultOnEmpty;
        return false;
    }
    if (!ParseJsonValueBehavior(jsonExpr, "errorBehavior", &errorBehavior, &defaultOnError)) {
        delete defaultOnEmpty;
        delete defaultOnError;
        return false;
    }

    args->push_back(CreateJsonValueBehaviorLiteral(emptyBehavior));
    args->push_back(defaultOnEmpty);
    args->push_back(CreateJsonValueBehaviorLiteral(errorBehavior));
    args->push_back(defaultOnError);
    return true;
}

bool ParseJsonQueryWrapper(const Json &jsonExpr, int32_t *wrapperBehavior)
{
    *wrapperBehavior = JSON_QUERY_WITHOUT_ARRAY_WRAPPER;
    if (!jsonExpr.contains("wrapperBehavior")) {
        return true;
    }

    const auto &wrapperJson = jsonExpr["wrapperBehavior"];
    if (!wrapperJson.is_object() || !wrapperJson.contains("type")) {
        return false;
    }

    const auto wrapperType = wrapperJson["type"].get<string>();
    if (wrapperType == "WITHOUT_ARRAY") {
        return true;
    }
    if (wrapperType == "WITH_CONDITIONAL_ARRAY") {
        *wrapperBehavior = JSON_QUERY_WITH_CONDITIONAL_ARRAY_WRAPPER;
        return true;
    }
    if (wrapperType == "WITH_UNCONDITIONAL_ARRAY") {
        *wrapperBehavior = JSON_QUERY_WITH_UNCONDITIONAL_ARRAY_WRAPPER;
        return true;
    }
    return false;
}

bool ParseJsonQueryBehavior(const Json &jsonExpr, const char *behaviorKey, int32_t *behaviorCode)
{
    *behaviorCode = JSON_QUERY_NULL_BEHAVIOR;
    if (!jsonExpr.contains(behaviorKey)) {
        return true;
    }

    const auto &behaviorJson = jsonExpr[behaviorKey];
    if (!behaviorJson.is_object() || !behaviorJson.contains("type")) {
        return false;
    }

    const auto behaviorType = behaviorJson["type"].get<string>();
    if (behaviorType == "NULL") {
        return true;
    }
    if (behaviorType == "EMPTY_ARRAY") {
        *behaviorCode = JSON_QUERY_EMPTY_ARRAY_BEHAVIOR;
        return true;
    }
    if (behaviorType == "EMPTY_OBJECT") {
        *behaviorCode = JSON_QUERY_EMPTY_OBJECT_BEHAVIOR;
        return true;
    }
    if (behaviorType == "ERROR") {
        *behaviorCode = JSON_QUERY_ERROR_BEHAVIOR;
        return true;
    }
    return false;
}

bool BuildJsonQueryArguments(const Json &jsonExpr, std::vector<Expr *> *args)
{
    for (const auto &item : jsonExpr["arguments"].items()) {
        Expr *arg = JSONParser::ParseJSON(item.value());
        if (arg != nullptr) {
            args->push_back(arg);
        } else {
            return false;
        }
    }

    int32_t wrapperBehavior = JSON_QUERY_WITHOUT_ARRAY_WRAPPER;
    int32_t emptyBehavior = JSON_QUERY_NULL_BEHAVIOR;
    int32_t errorBehavior = JSON_QUERY_NULL_BEHAVIOR;
    if (!ParseJsonQueryWrapper(jsonExpr, &wrapperBehavior)) {
        return false;
    }
    if (!ParseJsonQueryBehavior(jsonExpr, "emptyBehavior", &emptyBehavior)) {
        return false;
    }
    if (!ParseJsonQueryBehavior(jsonExpr, "errorBehavior", &errorBehavior)) {
        return false;
    }

    args->push_back(CreateJsonQueryWrapperLiteral(wrapperBehavior));
    args->push_back(CreateJsonQueryBehaviorLiteral(emptyBehavior));
    args->push_back(CreateJsonQueryBehaviorLiteral(errorBehavior));
    return true;
}
} // namespace

Expr *JSONParser::ParseJSONFunc(const Json &jsonExpr)
{
    string funcName = jsonExpr["function_name"];
    auto retTypeId = static_cast<DataTypeId>(jsonExpr["returnType"].get<int32_t>());
    std::vector<Expr *> args;
    DataTypePtr retType;
    int32_t width = INT32_MAX;
    int32_t precision;
    int32_t scale;

    if (funcName == "json_value") {
        if (!BuildJsonValueArguments(jsonExpr, &args)) {
            Expr::DeleteExprs(args);
            return nullptr;
        }
    } else if (funcName == "json_query") {
        if (!BuildJsonQueryArguments(jsonExpr, &args)) {
            Expr::DeleteExprs(args);
            return nullptr;
        }
    } else {
        for (const auto &item : jsonExpr["arguments"].items()) {
            Expr *arg = ParseJSON(item.value());
            if (arg != nullptr) {
                args.push_back(arg);
            } else {
                Expr::DeleteExprs(args);
                return nullptr;
            }
        }
    }

    if (TypeUtil::IsStringType(retTypeId)) {
        width = jsonExpr.contains("width") ? jsonExpr["width"].get<int32_t>() : width;
        if (retTypeId == OMNI_CHAR) {
            retType = std::make_shared<CharDataType>(width);
        } else {
            retType = std::make_shared<VarcharDataType>(width);
        }
    } else if (TypeUtil::IsDecimalType(retTypeId)) {
        precision = jsonExpr["precision"].get<int32_t>();
        scale = jsonExpr["scale"].get<int32_t>();
        if (retTypeId == OMNI_DECIMAL64) {
            retType = std::make_shared<Decimal64DataType>(precision, scale);
        } else {
            retType = std::make_shared<Decimal128DataType>(precision, scale);
        }
    } else {
        retType = std::make_shared<DataType>(retTypeId);
    }

    // CAST short-circuit - Convert CAST function of a type to its own type to DataExpr
    if (funcName == "CAST" && args.size() == 1) {
        auto argReturnType = args[0]->GetReturnType().get();
        if (retTypeId == argReturnType->GetId()) {
            if (TypeUtil::IsStringType(retTypeId)) {
                auto argWidth = static_cast<VarcharDataType *>(argReturnType)->GetWidth();
                auto retWidth = static_cast<VarcharDataType *>(retType.get())->GetWidth();
                if (argWidth <= retWidth) {
                    return args[0];
                }
            } else if (TypeUtil::IsDecimalType(retTypeId)) {
                auto argScale = static_cast<DecimalDataType *>(argReturnType)->GetScale();
                auto argPrecision = static_cast<DecimalDataType *>(argReturnType)->GetPrecision();
                auto retScale = static_cast<DecimalDataType *>(retType.get())->GetScale();
                auto retPrecision = static_cast<DecimalDataType *>(retType.get())->GetPrecision();
                if (argScale == retScale && argPrecision <= retPrecision) {
                    return args[0];
                }
            } else {
                return args[0];
            }
        }
    }

    // check rlike since we only support ^d+$ currently, all other regex are fallback
    if (funcName == "RLike" && args.size() == 2) {
        auto secondArg = args[1];
        if (secondArg->GetType() != ExprType::LITERAL_E) {
            Expr::DeleteExprs(args);
            return nullptr;
        }

        auto literalExpr = static_cast<LiteralExpr *>(secondArg);
        if (*(literalExpr->stringVal) != "^\\d+$") {
            Expr::DeleteExprs(args);
            return nullptr;
        }
    }

    // Check that the signature matches
    vector<DataTypeId> argTypes(args.size());
    std::transform(args.begin(), args.end(), argTypes.begin(),
        [](Expr *expr) -> DataTypeId { return expr->GetReturnTypeId(); });
    auto signature = std::make_shared<FunctionSignature>(funcName, argTypes, retTypeId);
    auto function = omniruntime::codegen::FunctionRegistry::LookupFunction(signature.get());
    auto vectorFunction = VectorFunction::Find(signature);
    if (function != nullptr || vectorFunction != nullptr) {
        return new FuncExpr(funcName, args, std::move(retType), function);
    }

    if (funcName == "named_struct" || funcName == "map" || funcName == "concat_ws"
        || funcName == "json_object") {
        return new FuncExpr(funcName, args, std::move(retType), nullptr);
    }

    auto &hiveUdfClass = omniruntime::codegen::FunctionRegistry::LookupHiveUdf(funcName);
    if (!hiveUdfClass.empty()) {
        return new FuncExpr(hiveUdfClass, args, std::move(retType), HIVE_UDF);
    }
    LogWarn("Function not supported: %s", funcName.c_str());

    Expr::DeleteExprs(args);
    // if operator is not supported, return nullptr
    return nullptr;
}

Expr *JSONParser::ParseJsonMultiAndOr(const Json &jsonExpr)
{
    using Operator = omniruntime::expressions::Operator;
    std::vector<Expr*> conditions;
    BinaryExpr* binaryExpression;
    DataTypePtr returnType = std::make_shared<DataType>(OMNI_BOOLEAN);
    Operator op = (jsonExpr["operator"] == "OR" ? Operator::OR : Operator::AND);
    for (const auto &item : jsonExpr["conditions"].items()) {
        Expr *condition = ParseJSON(item.value());
        if (condition != nullptr) {
            conditions.push_back(condition);
        } else {
            Expr::DeleteExprs(conditions);
            return nullptr;
        }
    }
    constexpr int minConditionSize = 2;
    if (conditions.size() < minConditionSize) {
        Expr::DeleteExprs(conditions);
        return nullptr;
    }
    binaryExpression = new BinaryExpr(op, conditions[0], conditions[1], returnType);
    for (size_t i = 2; i < conditions.size(); i++) { // chain them together
        BinaryExpr* oldExpr = binaryExpression;
        binaryExpression = new BinaryExpr(op, oldExpr, conditions[i], returnType);
    }
    return binaryExpression;
}

Expr *JSONParser::ParseJSON(const Json &jsonExpr)
{
    string exprTypeStr = jsonExpr["exprType"].get<string>();
    if (exprTypeStr == "FIELD_REFERENCE") {
        return ParseJSONFieldRef(jsonExpr);
    } else if (exprTypeStr == "LITERAL") {
        return ParseJSONLiteral(jsonExpr);
    } else if (exprTypeStr == "BINARY") {
        return ParseJSONBinary(jsonExpr);
    } else if (exprTypeStr == "UNARY") {
        return ParseJSONUnary(jsonExpr);
    } else if (exprTypeStr == "IN") {
        return ParseJSONIn(jsonExpr);
    } else if (exprTypeStr == "BETWEEN") {
        return ParseJSONBetween(jsonExpr);
    } else if (exprTypeStr == "IF") {
        return ParseJSONIf(jsonExpr);
    } else if (exprTypeStr == "COALESCE") {
        return ParseJSONCoalesce(jsonExpr);
    } else if (exprTypeStr == "IS_NULL") {
        return ParseJsonIsNull(jsonExpr);
    } else if (exprTypeStr == "IS_NOT_NULL") {
        return ParseJsonIsNotNull(jsonExpr);
    }else if (exprTypeStr == "FUNC" || exprTypeStr == "FUNCTION") {
        return ParseJSONFunc(jsonExpr);
    } else if (exprTypeStr == "SWITCH") {
        return ParseJSONSwitch(jsonExpr);
    } else if (exprTypeStr == "SWITCH_GENERAL") {
        return ParseJSONSwitchGeneral(jsonExpr);
    } else if (exprTypeStr == "MULTIPLE_AND_OR") {
        return ParseJsonMultiAndOr(jsonExpr);
    } else {
        // return nullptr if ExprType not supported
        return nullptr;
    }
}

std::vector<omniruntime::expressions::Expr *> JSONParser::ParseJSON(nlohmann::json *expressions,
    int32_t numberOfExpressions)
{
    std::vector<Expr *> result;
    for (int32_t i = 0; i < numberOfExpressions; i++) {
        Expr *expression = ParseJSON(expressions[i]);
        if (expression == nullptr) {
            LogWarn("The %d-th expression is not supported: %s", i, expressions[i].dump(1).c_str());
            Expr::DeleteExprs(result);
            result.clear();
            break;
        }
        result.push_back(expression);
    }
    return result;
}

Expr *JSONParser::ParseJSON(const std::string &exprStr)
{
    omniruntime::expressions::Expr *expr = nullptr;
    if (!exprStr.empty()) {
        expr = JSONParser::ParseJSON(nlohmann::json::parse(exprStr));
        if (expr == nullptr) {
            LogWarn("The expression is not supported: %s", exprStr.c_str());
        }
    }
    return expr;
}
