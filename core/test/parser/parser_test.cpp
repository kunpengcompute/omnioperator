/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <string>
#include <vector>
#include "gtest/gtest.h"
#include "test/util/test_util.h"
#include "operator/projection/projection.h"

using namespace omniruntime::op;
using namespace omniruntime::vec;
using namespace omniruntime::expressions;
using namespace std;

namespace ParserTest {
void testCmpBinaryExpressions(std::vector<Expr *> result, omniruntime::expressions::Operator op, const int projectCount,
                              bool isBoolResult = false)
{
    std::vector<DataTypeId> dataTypes = {OMNI_INT, OMNI_LONG, OMNI_DOUBLE, OMNI_DECIMAL128, OMNI_VARCHAR, OMNI_CHAR};
    for (int i = 0; i < projectCount; i++) {
        BinaryExpr *binaryExpr = static_cast<BinaryExpr *>(result.at(i));
        if (isBoolResult) {
            EXPECT_EQ(binaryExpr->GetReturnTypeId(), OMNI_BOOLEAN);
        } else {
            EXPECT_EQ(binaryExpr->GetReturnTypeId(), dataTypes[i]);
        }
        EXPECT_EQ(binaryExpr->op, op);
        FieldExpr *left = static_cast<FieldExpr *>(binaryExpr->left);
        EXPECT_EQ(left->GetType(), omniruntime::expressions::ExprType::FIELD_E);
        EXPECT_EQ(left->colVal, i);

        LiteralExpr *right = static_cast<LiteralExpr *>(binaryExpr->right);
        EXPECT_EQ(right->GetType(), omniruntime::expressions::ExprType::LITERAL_E);
        if (i == 3) { // the int LiteralExpr
            EXPECT_EQ(right->GetReturnTypeId(), OMNI_DECIMAL128);
        } else if (i == 5) { // the char LiteralExpr
            EXPECT_EQ(right->GetReturnTypeId(), OMNI_CHAR);
        } else {
            EXPECT_EQ(right->GetReturnTypeId(), dataTypes[i]);
        }
        if (i == 0) // the int LiteralExpr
            EXPECT_EQ(right->intVal, i);
        else if (i == 1) // the long LiteralExpr
            EXPECT_EQ(right->longVal, i);
        else if (i == 2) // the double LiteralExpr
            EXPECT_EQ(right->doubleVal, i);
        else if (i == 3) // the decimal128 LiteralExpr
            ASSERT_EQ(stol(*right->stringVal), i);
        else if (i == 4 || i == 5) // the varchar LiteralExpr
            ASSERT_STREQ(right->stringVal->c_str(), "hello");
        else {
            // explicit branch
        }

        if (i == 5) { // the char LiteralExpr
            EXPECT_EQ(static_cast<VarcharDataType &>(*(right->dataType)).GetWidth(), 6);
        }
    }
}

void testArithmeticBinaryExpressions(std::vector<Expr *> result, omniruntime::expressions::Operator op,
                                     const int projectCount)
{
    std::vector<DataTypeId> dataTypes = {OMNI_INT, OMNI_LONG, OMNI_DOUBLE, OMNI_DECIMAL128};
    for (int i = 0; i < projectCount; i++) {
        BinaryExpr *binaryExpr = static_cast<BinaryExpr *>(result.at(i));
        EXPECT_EQ(binaryExpr->GetReturnTypeId(), dataTypes[i]);
        EXPECT_EQ(binaryExpr->op, op);
        FieldExpr *left = static_cast<FieldExpr *>(binaryExpr->left);
        EXPECT_EQ(left->GetType(), omniruntime::expressions::ExprType::FIELD_E);
        EXPECT_EQ(left->colVal, i);

        LiteralExpr *right = static_cast<LiteralExpr *>(binaryExpr->right);
        EXPECT_EQ(right->GetType(), omniruntime::expressions::ExprType::LITERAL_E);
        if (i == 3) {
            EXPECT_EQ(right->GetReturnTypeId(), OMNI_DECIMAL128);
        } else {
            EXPECT_EQ(right->GetReturnTypeId(), dataTypes[i]);
        }
        if (i == 0)
            EXPECT_EQ(right->intVal, i);
        else if (i == 1)
            EXPECT_EQ(right->longVal, i);
        else if (i == 2)
            EXPECT_EQ(right->doubleVal, i);
        else if (i == 3)
            EXPECT_EQ(stol(*right->stringVal), i);
        else {
            // explicit braces
        };
    }
}

// Test Not expression
TEST(ParseTest, parseNotOperation)
{
    int vecTypeCount = 1;
    string expr = "$operator$NOT:4(#0)";
    std::vector<DataTypePtr> vecOfTypes = {BooleanType()};
    DataTypes inputTypes(vecOfTypes);
    Parser parser;
    Expr *result = parser.ParseRowExpression(expr, inputTypes, vecTypeCount);
    EXPECT_EQ(result->GetReturnTypeId(), OMNI_BOOLEAN);
    UnaryExpr *notExpr = static_cast<UnaryExpr *>(result);
    EXPECT_EQ(notExpr->op, omniruntime::expressions::Operator::NOT);
    FieldExpr *colExpr = static_cast<FieldExpr *>(notExpr->exp);
    EXPECT_EQ(colExpr->GetType(), omniruntime::expressions::ExprType::FIELD_E);
    EXPECT_EQ(colExpr->colVal, 0);

    delete result;
}

// Test Arithmetic Binary Operations
TEST(ParseTest, parseAddOperation)
{
    const int projectCount = 4;
    string exprs[projectCount] = {"$operator$ADD:1(#0, 0:1)", "$operator$ADD:2(#1, 1:2)", "$operator$ADD:3(#2, 2:3)",
                                  "$operator$ADD:7(#3, 3:7)"};
    std::vector<DataTypePtr> vecOfTypes = {IntType(), LongType(), DoubleType(), Decimal128Type()};
    DataTypes inputTypes(vecOfTypes);
    Parser parser;
    std::vector<Expr *> result = parser.ParseExpressions(exprs, projectCount, inputTypes);
    testArithmeticBinaryExpressions(result, omniruntime::expressions::Operator::ADD, projectCount);
    for (int i = 0; i < projectCount; i++) {
        delete result.at(i);
    }
}

TEST(ParseTest, parseSubtractOperation)
{
    const int projectCount = 4;
    string exprs[projectCount] = {"$operator$SUBTRACT:1(#0, 0:1)", "$operator$SUBTRACT:2(#1, 1:2)",
                                  "$operator$SUBTRACT:3(#2, 2:3)", "$operator$SUBTRACT:7(#3, 3:7)"};
    std::vector<DataTypePtr> vecOfTypes = {IntType(), LongType(), DoubleType(), Decimal128Type()};
    DataTypes inputTypes(vecOfTypes);
    Parser parser;
    std::vector<Expr *> result = parser.ParseExpressions(exprs, projectCount, inputTypes);
    testArithmeticBinaryExpressions(result, omniruntime::expressions::Operator::SUB, projectCount);
    for (int i = 0; i < projectCount; i++) {
        delete result.at(i);
    }
}

TEST(ParseTest, parseMultiplyOperation)
{
    const int projectCount = 4;
    string exprs[projectCount] = {"$operator$MULTIPLY:1(#0, 0:1)", "$operator$MULTIPLY:2(#1, 1:2)",
                                  "$operator$MULTIPLY:3(#2, 2:3)", "$operator$MULTIPLY:7(#3, 3:7)"};
    std::vector<DataTypePtr> vecOfTypes = {IntType(), LongType(), DoubleType(), Decimal128Type()};
    DataTypes inputTypes(vecOfTypes);
    Parser parser;
    std::vector<Expr *> result = parser.ParseExpressions(exprs, projectCount, inputTypes);
    testArithmeticBinaryExpressions(result, omniruntime::expressions::Operator::MUL, projectCount);
    for (int i = 0; i < projectCount; i++) {
        delete result.at(i);
    }
}

TEST(ParseTest, parseDivideOperation)
{
    const int projectCount = 4;
    string exprs[projectCount] = {"$operator$DIVIDE:1(#0, 0:1)", "$operator$DIVIDE:2(#1, 1:2)",
                                  "$operator$DIVIDE:3(#2, 2:3)", "$operator$DIVIDE:7(#3, 3:7)"};
    std::vector<DataTypePtr> vecOfTypes = {IntType(), LongType(), DoubleType(), Decimal128Type()};
    DataTypes inputTypes(vecOfTypes);
    Parser parser;
    std::vector<Expr *> result = parser.ParseExpressions(exprs, projectCount, inputTypes);
    testArithmeticBinaryExpressions(result, omniruntime::expressions::Operator::DIV, projectCount);
    for (int i = 0; i < projectCount; i++) {
        delete result.at(i);
    }
}

TEST(ParseTest, parseModulusOperation)
{
    const int projectCount = 4;
    string exprs[projectCount] = {"$operator$MODULUS:1(#0, 0:1)", "$operator$MODULUS:2(#1, 1:2)",
                                  "$operator$MODULUS:3(#2, 2:3)", "$operator$MODULUS:7(#3, 3:7)"};
    std::vector<DataTypePtr> vecOfTypes = {IntType(), LongType(), DoubleType(), Decimal128Type()};
    DataTypes inputTypes(vecOfTypes);
    Parser parser;
    std::vector<Expr *> result = parser.ParseExpressions(exprs, projectCount, inputTypes);
    testArithmeticBinaryExpressions(result, omniruntime::expressions::Operator::MOD, projectCount);
    for (int i = 0; i < projectCount; i++) {
        delete result.at(i);
    }
}

// Test Compare Binary Operations
TEST(ParseTest, parseLTOperation)
{
    const int projectCount = 4;
    string exprs[projectCount] = {"$operator$LESS_THAN:4(#0, 0:1)", "$operator$LESS_THAN:4(#1, 1:2)",
                                  "$operator$LESS_THAN:4(#2, 2:3)", "$operator$LESS_THAN:4(#3, 3:7)"};
    std::vector<DataTypePtr> vecOfTypes = {IntType(), LongType(), DoubleType(), Decimal128Type()};
    DataTypes inputTypes(vecOfTypes);
    Parser parser;
    std::vector<Expr *> result = parser.ParseExpressions(exprs, projectCount, inputTypes);
    testCmpBinaryExpressions(result, omniruntime::expressions::Operator::LT, projectCount, true);
    for (int i = 0; i < projectCount; i++) {
        delete result.at(i);
    }
}

TEST(ParseTest, parseLTEOperation)
{
    const int projectCount = 4;
    string exprs[projectCount] = {"$operator$LESS_THAN_OR_EQUAL:4(#0, 0:1)", "$operator$LESS_THAN_OR_EQUAL:4(#1, 1:2)",
                                  "$operator$LESS_THAN_OR_EQUAL:4(#2, 2:3)", "$operator$LESS_THAN_OR_EQUAL:4(#3, 3:7)"};
    std::vector<DataTypePtr> vecOfTypes = {IntType(), LongType(), DoubleType(), Decimal128Type()};
    DataTypes inputTypes(vecOfTypes);
    Parser parser;
    std::vector<Expr *> result = parser.ParseExpressions(exprs, projectCount, inputTypes);
    testCmpBinaryExpressions(result, omniruntime::expressions::Operator::LTE, projectCount, true);
    for (int i = 0; i < projectCount; i++) {
        delete result.at(i);
    }
}

TEST(ParseTest, parseGTOperation)
{
    const int projectCount = 4;
    string exprs[projectCount] = {"$operator$GREATER_THAN:4(#0, 0:1)", "$operator$GREATER_THAN:4(#1, 1:2)",
                                  "$operator$GREATER_THAN:4(#2, 2:3)", "$operator$GREATER_THAN:4(#3, 3:7)"};
    std::vector<DataTypePtr> vecOfTypes = {IntType(), LongType(), DoubleType(), Decimal128Type()};
    DataTypes inputTypes(vecOfTypes);
    Parser parser;
    std::vector<Expr *> result = parser.ParseExpressions(exprs, projectCount, inputTypes);
    testCmpBinaryExpressions(result, omniruntime::expressions::Operator::GT, projectCount, true);
    for (int i = 0; i < projectCount; i++) {
        delete result.at(i);
    }
}

TEST(ParseTest, parseGTEOperation)
{
    const int projectCount = 4;
    string exprs[projectCount] = {
        "$operator$GREATER_THAN_OR_EQUAL:4(#0, 0:1)", "$operator$GREATER_THAN_OR_EQUAL:4(#1, 1:2)",
        "$operator$GREATER_THAN_OR_EQUAL:4(#2, 2:3)", "$operator$GREATER_THAN_OR_EQUAL:4(#3, 3:7)"};
    std::vector<DataTypePtr> vecOfTypes = {IntType(), LongType(), DoubleType(), Decimal128Type()};
    DataTypes inputTypes(vecOfTypes);
    Parser parser;
    std::vector<Expr *> result = parser.ParseExpressions(exprs, projectCount, inputTypes);
    testCmpBinaryExpressions(result, omniruntime::expressions::Operator::GTE, projectCount, true);
    for (int i = 0; i < projectCount; i++) {
        delete result.at(i);
    }
}

TEST(ParseTest, parseEQOperation)
{
    const int projectCount = 6;
    string exprs[projectCount] = {"$operator$EQUAL:4(#0, 0:1)",        "$operator$EQUAL:4(#1, 1:2)",
                                  "$operator$EQUAL:4(#2, 2:3)",        "$operator$EQUAL:4(#3, 3:7)",
                                  "$operator$EQUAL:4(#4, 'hello':15)", "$operator$EQUAL:4(#5, 'hello':16[6])"};
    std::vector<DataTypePtr> vecOfTypes = {IntType(),        LongType(),    DoubleType(),
                                           Decimal128Type(), VarcharType(), CharType()};
    DataTypes inputTypes(vecOfTypes);
    Parser parser;
    std::vector<Expr *> result = parser.ParseExpressions(exprs, projectCount, inputTypes);
    testCmpBinaryExpressions(result, omniruntime::expressions::Operator::EQ, projectCount, true);
    for (int i = 0; i < projectCount; i++) {
        delete result.at(i);
    }
}

TEST(ParseTest, parseNEQOperation)
{
    const int projectCount = 6;
    string exprs[projectCount] = {"$operator$NOT_EQUAL:4(#0, 0:1)",        "$operator$NOT_EQUAL:4(#1, 1:2)",
                                  "$operator$NOT_EQUAL:4(#2, 2:3)",        "$operator$NOT_EQUAL:4(#3, 3:7)",
                                  "$operator$NOT_EQUAL:4(#4, 'hello':15)", "$operator$NOT_EQUAL:4(#5, 'hello':16[6])"};
    std::vector<DataTypePtr> vecOfTypes = {IntType(),        LongType(),    DoubleType(),
                                           Decimal128Type(), VarcharType(), CharType()};
    DataTypes inputTypes(vecOfTypes);
    Parser parser;
    std::vector<Expr *> result = parser.ParseExpressions(exprs, projectCount, inputTypes);
    testCmpBinaryExpressions(result, omniruntime::expressions::Operator::NEQ, projectCount, true);
    for (int i = 0; i < projectCount; i++) {
        delete result.at(i);
    }
}

// Test Logic Operations
TEST(ParseTest, parseLogicOperations)
{
    const int projectCount = 2;
    string exprs[projectCount] = {"$operator$AND:4(#0, true:4)", "$operator$OR:4(#1, false:4)"};
    std::vector<DataTypePtr> vecOfTypes = {BooleanType(), BooleanType()};
    DataTypes inputTypes(vecOfTypes);
    Parser parser;
    std::vector<Expr *> result = parser.ParseExpressions(exprs, projectCount, inputTypes);
    EXPECT_EQ(result.size(), projectCount);
    for (int i = 0; i < projectCount; i++) {
        BinaryExpr *logExpr = static_cast<BinaryExpr *>(result.at(i));
        EXPECT_EQ(logExpr->GetReturnTypeId(), OMNI_BOOLEAN);
        FieldExpr *logLeft = static_cast<FieldExpr *>(logExpr->left);
        LiteralExpr *logRight = static_cast<LiteralExpr *>(logExpr->right);
        EXPECT_EQ(logLeft->GetType(), omniruntime::expressions::ExprType::FIELD_E);
        EXPECT_EQ(logLeft->colVal, i);
        EXPECT_EQ(logRight->GetType(), omniruntime::expressions::ExprType::LITERAL_E);
        if (i == 0) {
            EXPECT_TRUE(logRight->boolVal);
            EXPECT_EQ(logExpr->op, omniruntime::expressions::Operator::AND);
        }
        if (i == 1) {
            EXPECT_FALSE(logRight->boolVal);
            EXPECT_EQ(logExpr->op, omniruntime::expressions::Operator::OR);
        }
    }

    for (int i = 0; i < projectCount; i++) {
        delete result.at(i);
    }
}

// Test Between expression
TEST(ParseTest, parseBetweenOperation)
{
    int vecTypeCount = 3;
    string expr = "$operator$BETWEEN:4(#1, #0, #2)";
    std::vector<DataTypePtr> vecOfTypes = {IntType(), DoubleType(), LongType()};
    DataTypes inputTypes(vecOfTypes);
    Parser parser;
    Expr *result = parser.ParseRowExpression(expr, inputTypes, vecTypeCount);
    EXPECT_EQ(result->GetReturnTypeId(), OMNI_BOOLEAN);
    BetweenExpr *betweenExpr = static_cast<BetweenExpr *>(result);
    FieldExpr *valueExpr = static_cast<FieldExpr *>(betweenExpr->value);
    FieldExpr *lowerExpr = static_cast<FieldExpr *>(betweenExpr->lowerBound);
    FieldExpr *upperExpr = static_cast<FieldExpr *>(betweenExpr->upperBound);

    EXPECT_EQ(valueExpr->GetType(), omniruntime::expressions::ExprType::FIELD_E);
    EXPECT_EQ(valueExpr->colVal, 1);
    EXPECT_EQ(valueExpr->GetReturnTypeId(), OMNI_DOUBLE);

    EXPECT_EQ(lowerExpr->GetType(), omniruntime::expressions::ExprType::FIELD_E);
    EXPECT_EQ(lowerExpr->colVal, 0);
    EXPECT_EQ(lowerExpr->GetReturnTypeId(), OMNI_INT);

    EXPECT_EQ(upperExpr->GetType(), omniruntime::expressions::ExprType::FIELD_E);
    EXPECT_EQ(upperExpr->colVal, 2);
    EXPECT_EQ(upperExpr->GetReturnTypeId(), OMNI_LONG);

    delete result;
}

// Test In expression
TEST(ParseTest, parseInOperation)
{
    int vecTypeCount = 3;
    string expr = "$operator$IN:4(#1, #0, #2)";
    std::vector<DataTypePtr> vecOfTypes = {IntType(), DoubleType(), LongType()};
    DataTypes inputTypes(vecOfTypes);
    Parser parser;
    Expr *result = parser.ParseRowExpression(expr, inputTypes, vecTypeCount);
    EXPECT_EQ(result->GetReturnTypeId(), OMNI_BOOLEAN);
    InExpr *inExpr = static_cast<InExpr *>(result);
    auto args = inExpr->arguments;
    FieldExpr *arg0 = static_cast<FieldExpr *>(args[0]);
    FieldExpr *arg1 = static_cast<FieldExpr *>(args[1]);
    FieldExpr *arg2 = static_cast<FieldExpr *>(args[2]);

    EXPECT_EQ(arg0->GetType(), omniruntime::expressions::ExprType::FIELD_E);
    EXPECT_EQ(arg0->colVal, 1);
    EXPECT_EQ(arg0->GetReturnTypeId(), OMNI_DOUBLE);

    EXPECT_EQ(arg1->GetType(), omniruntime::expressions::ExprType::FIELD_E);
    EXPECT_EQ(arg1->colVal, 0);
    EXPECT_EQ(arg1->GetReturnTypeId(), OMNI_INT);

    EXPECT_EQ(arg2->GetType(), omniruntime::expressions::ExprType::FIELD_E);
    EXPECT_EQ(arg2->colVal, 2);
    EXPECT_EQ(arg2->GetReturnTypeId(), OMNI_LONG);

    delete result;
}

// Test Coalesce expression
TEST(ParseTest, parseCoalesceOperation)
{
    int vecTypeCount = 3;
    string expr = "COALESCE:1(#1, #0)";
    std::vector<DataTypePtr> vecOfTypes = {IntType(), IntType()};
    DataTypes inputTypes(vecOfTypes);
    Parser parser;
    Expr *result = parser.ParseRowExpression(expr, inputTypes, vecTypeCount);
    EXPECT_EQ(result->GetReturnTypeId(), OMNI_INT);
    CoalesceExpr *coalesceExpr = static_cast<CoalesceExpr *>(result);
    FieldExpr *value1 = static_cast<FieldExpr *>(coalesceExpr->value1);
    FieldExpr *value2 = static_cast<FieldExpr *>(coalesceExpr->value2);

    EXPECT_EQ(value1->GetType(), omniruntime::expressions::ExprType::FIELD_E);
    EXPECT_EQ(value1->colVal, 1);
    EXPECT_EQ(value1->GetReturnTypeId(), OMNI_INT);

    EXPECT_EQ(value2->GetType(), omniruntime::expressions::ExprType::FIELD_E);
    EXPECT_EQ(value2->colVal, 0);
    EXPECT_EQ(value2->GetReturnTypeId(), OMNI_INT);

    delete result;
}

// Test If expression
TEST(ParseTest, parseIfOperation)
{
    int vecTypeCount = 1;
    string expr = "IF:15(true:4, #0, 'hello':15)";
    std::vector<DataTypePtr> vecOfTypes = {VarcharType()};
    DataTypes inputTypes(vecOfTypes);
    Parser parser;
    Expr *result = parser.ParseRowExpression(expr, inputTypes, vecTypeCount);
    EXPECT_EQ(result->GetReturnTypeId(), OMNI_VARCHAR);
    IfExpr *ifExpr = static_cast<IfExpr *>(result);
    LiteralExpr *condition = static_cast<LiteralExpr *>(ifExpr->condition);
    FieldExpr *tExpr = static_cast<FieldExpr *>(ifExpr->trueExpr);
    LiteralExpr *fExpr = static_cast<LiteralExpr *>(ifExpr->falseExpr);

    EXPECT_EQ(condition->GetReturnTypeId(), OMNI_BOOLEAN);
    EXPECT_EQ(condition->GetType(), omniruntime::expressions::ExprType::LITERAL_E);
    EXPECT_EQ(condition->boolVal, true);

    EXPECT_EQ(tExpr->GetType(), omniruntime::expressions::ExprType::FIELD_E);
    EXPECT_EQ(tExpr->colVal, 0);
    EXPECT_EQ(tExpr->GetReturnTypeId(), OMNI_VARCHAR);

    EXPECT_EQ(fExpr->GetType(), omniruntime::expressions::ExprType::LITERAL_E);
    EXPECT_STREQ(fExpr->stringVal->c_str(), "hello");

    delete result;
}

// Test IsNull expression
TEST(ParseTest, parseIsNullOperation)
{
    int vecTypeCount = 1;
    string expr = "IS_NULL:4(#0)";
    std::vector<DataTypePtr> vecOfTypes = {CharType()};
    DataTypes inputTypes(vecOfTypes);
    Parser parser;
    Expr *result = parser.ParseRowExpression(expr, inputTypes, vecTypeCount);
    EXPECT_EQ(result->GetReturnTypeId(), OMNI_BOOLEAN);
    IsNullExpr *isNullExpr = static_cast<IsNullExpr *>(result);
    FieldExpr *value = static_cast<FieldExpr *>(isNullExpr->value);

    EXPECT_EQ(value->GetType(), omniruntime::expressions::ExprType::FIELD_E);
    EXPECT_EQ(value->colVal, 0);
    EXPECT_EQ(value->GetReturnTypeId(), OMNI_CHAR);

    delete result;
}

// Test IsNotNull expression
TEST(ParseTest, parseIsNotNullOperation)
{
    int vecTypeCount = 1;
    string expr = "IS_NOT_NULL:4(#0)";
    std::vector<DataTypePtr> vecOfTypes = {CharType()};
    DataTypes inputTypes(vecOfTypes);
    Parser parser;
    Expr *result = parser.ParseRowExpression(expr, inputTypes, vecTypeCount);
    EXPECT_EQ(result->GetReturnTypeId(), OMNI_BOOLEAN);
    UnaryExpr *unaryExpr = static_cast<UnaryExpr *>(result);
    EXPECT_EQ(unaryExpr->op, omniruntime::expressions::Operator::NOT);
    IsNullExpr *isNullExpr = static_cast<IsNullExpr *>(unaryExpr->exp);
    FieldExpr *value = static_cast<FieldExpr *>(isNullExpr->value);

    EXPECT_EQ(value->GetType(), omniruntime::expressions::ExprType::FIELD_E);
    EXPECT_EQ(value->colVal, 0);
    EXPECT_EQ(value->GetReturnTypeId(), OMNI_CHAR);

    delete result;
}

// Test Cast expression
TEST(ParseTest, parseCastOperation)
{
    int vecTypeCount = 1;
    string expr = "CAST:2(#0)";
    std::vector<DataTypePtr> vecOfTypes = {IntType()};
    DataTypes inputTypes(vecOfTypes);
    Parser parser;
    Expr *result = parser.ParseRowExpression(expr, inputTypes, vecTypeCount);
    EXPECT_EQ(result->GetReturnTypeId(), OMNI_LONG);
    FuncExpr *castExpr = static_cast<FuncExpr *>(result);
    EXPECT_STREQ(castExpr->funcName.c_str(), "CAST");
    auto args = castExpr->arguments;
    FieldExpr *arg0 = static_cast<FieldExpr *>(args[0]);

    EXPECT_EQ(arg0->GetType(), omniruntime::expressions::ExprType::FIELD_E);
    EXPECT_EQ(arg0->colVal, 0);
    EXPECT_EQ(arg0->GetReturnTypeId(), OMNI_INT);

    delete result;
}

// Test abs expression
TEST(ParseTest, parseAbsOperation)
{
    int vecTypeCount = 1;
    string expr = "abs:3(#0)";
    std::vector<DataTypePtr> vecOfTypes = {DoubleType()};
    DataTypes inputTypes(vecOfTypes);
    Parser parser;
    Expr *result = parser.ParseRowExpression(expr, inputTypes, vecTypeCount);
    EXPECT_EQ(result->GetReturnTypeId(), OMNI_DOUBLE);
    FuncExpr *absExpr = static_cast<FuncExpr *>(result);
    EXPECT_STREQ(absExpr->funcName.c_str(), "abs");
    auto args = absExpr->arguments;
    FieldExpr *arg0 = static_cast<FieldExpr *>(args[0]);

    EXPECT_EQ(arg0->GetType(), omniruntime::expressions::ExprType::FIELD_E);
    EXPECT_EQ(arg0->colVal, 0);
    EXPECT_EQ(arg0->GetReturnTypeId(), OMNI_DOUBLE);

    delete result;
}

// test substr
TEST(ParseTest, parseSubstrOperation)
{
    int vecTypeCount = 1;
    string expr = "substr:15(#0, 1:1, 6:1)";
    std::vector<DataTypePtr> vecOfTypes = {VarcharType()};
    DataTypes inputTypes(vecOfTypes);
    Parser parser;
    Expr *result = parser.ParseRowExpression(expr, inputTypes, vecTypeCount);
    EXPECT_EQ(result->GetReturnTypeId(), OMNI_VARCHAR);
    FuncExpr *absExpr = static_cast<FuncExpr *>(result);
    EXPECT_STREQ(absExpr->funcName.c_str(), "substr");
    auto args = absExpr->arguments;
    FieldExpr *value = static_cast<FieldExpr *>(args[0]);
    LiteralExpr *index = static_cast<LiteralExpr *>(args[1]);
    LiteralExpr *length = static_cast<LiteralExpr *>(args[2]);

    EXPECT_EQ(value->GetType(), omniruntime::expressions::ExprType::FIELD_E);
    EXPECT_EQ(value->colVal, 0);
    EXPECT_EQ(value->GetReturnTypeId(), OMNI_VARCHAR);

    EXPECT_EQ(index->GetType(), omniruntime::expressions::ExprType::LITERAL_E);
    EXPECT_EQ(index->intVal, 1);
    EXPECT_EQ(index->GetReturnTypeId(), OMNI_INT);

    EXPECT_EQ(length->GetType(), omniruntime::expressions::ExprType::LITERAL_E);
    EXPECT_EQ(length->intVal, 6);
    EXPECT_EQ(length->GetReturnTypeId(), OMNI_INT);

    delete result;
}

// test concat
TEST(ParseTest, parseConcatOperation)
{
    int vecTypeCount = 1;
    string expr = "concat:15(#0, 'hello':15)";
    std::vector<DataTypePtr> vecOfTypes = {VarcharType()};
    DataTypes inputTypes(vecOfTypes);
    Parser parser;
    Expr *result = parser.ParseRowExpression(expr, inputTypes, vecTypeCount);
    EXPECT_EQ(result->GetReturnTypeId(), OMNI_VARCHAR);
    FuncExpr *absExpr = static_cast<FuncExpr *>(result);
    EXPECT_STREQ(absExpr->funcName.c_str(), "concat");
    auto args = absExpr->arguments;
    FieldExpr *value1 = static_cast<FieldExpr *>(args[0]);
    LiteralExpr *value2 = static_cast<LiteralExpr *>(args[1]);

    EXPECT_EQ(value1->GetType(), omniruntime::expressions::ExprType::FIELD_E);
    EXPECT_EQ(value1->colVal, 0);
    EXPECT_EQ(value1->GetReturnTypeId(), OMNI_VARCHAR);

    EXPECT_EQ(value2->GetType(), omniruntime::expressions::ExprType::LITERAL_E);
    EXPECT_STREQ(value2->stringVal->c_str(), "hello");
    EXPECT_EQ(value2->GetReturnTypeId(), OMNI_VARCHAR);

    delete result;
}

// Test Like
TEST(ParseTest, parseLikeOperation)
{
    int vecTypeCount = 1;
    string expr = "LIKE:4(#0, '%hello':15)";
    std::vector<DataTypePtr> vecOfTypes = {VarcharType()};
    DataTypes inputTypes(vecOfTypes);
    Parser parser;
    Expr *result = parser.ParseRowExpression(expr, inputTypes, vecTypeCount);
    EXPECT_EQ(result->GetReturnTypeId(), OMNI_BOOLEAN);
    FuncExpr *absExpr = static_cast<FuncExpr *>(result);
    EXPECT_STREQ(absExpr->funcName.c_str(), "LIKE");
    auto args = absExpr->arguments;
    FieldExpr *value = static_cast<FieldExpr *>(args[0]);
    LiteralExpr *index = static_cast<LiteralExpr *>(args[1]);

    EXPECT_EQ(value->GetType(), omniruntime::expressions::ExprType::FIELD_E);
    EXPECT_EQ(value->colVal, 0);
    EXPECT_EQ(value->GetReturnTypeId(), OMNI_VARCHAR);

    EXPECT_EQ(index->GetType(), omniruntime::expressions::ExprType::LITERAL_E);
    EXPECT_STREQ(index->stringVal->c_str(), ".*hello");
    EXPECT_EQ(index->GetReturnTypeId(), OMNI_VARCHAR);

    delete result;
}

// Test mm3hash
TEST(ParseTest, parseMM3HashOperation)
{
    int vecTypeCount = 1;
    string expr = "mm3hash:1(#0, 2:1)";
    std::vector<DataTypePtr> vecOfTypes = {VarcharType()};
    DataTypes inputTypes(vecOfTypes);
    Parser parser;
    Expr *result = parser.ParseRowExpression(expr, inputTypes, vecTypeCount);
    EXPECT_EQ(result->GetReturnTypeId(), OMNI_INT);
    FuncExpr *absExpr = static_cast<FuncExpr *>(result);
    EXPECT_STREQ(absExpr->funcName.c_str(), "mm3hash");
    auto args = absExpr->arguments;
    FieldExpr *value = static_cast<FieldExpr *>(args[0]);
    LiteralExpr *index = static_cast<LiteralExpr *>(args[1]);

    EXPECT_EQ(value->GetType(), omniruntime::expressions::ExprType::FIELD_E);
    EXPECT_EQ(value->colVal, 0);
    EXPECT_EQ(value->GetReturnTypeId(), OMNI_VARCHAR);

    EXPECT_EQ(index->GetType(), omniruntime::expressions::ExprType::LITERAL_E);
    EXPECT_EQ(index->intVal, 2);
    EXPECT_EQ(index->GetReturnTypeId(), OMNI_INT);

    delete result;
}

// Test combine hash
TEST(ParseTest, parseCombineHashOperation)
{
    int vecTypeCount = 1;
    string expr = "combine_hash:2(#0, 2:2)";
    std::vector<DataTypePtr> vecOfTypes = {LongType()};
    DataTypes inputTypes(vecOfTypes);
    Parser parser;
    Expr *result = parser.ParseRowExpression(expr, inputTypes, vecTypeCount);
    EXPECT_EQ(result->GetReturnTypeId(), OMNI_LONG);
    FuncExpr *absExpr = static_cast<FuncExpr *>(result);
    EXPECT_STREQ(absExpr->funcName.c_str(), "combine_hash");
    auto args = absExpr->arguments;
    FieldExpr *value = static_cast<FieldExpr *>(args[0]);
    LiteralExpr *index = static_cast<LiteralExpr *>(args[1]);

    EXPECT_EQ(value->GetType(), omniruntime::expressions::ExprType::FIELD_E);
    EXPECT_EQ(value->colVal, 0);
    EXPECT_EQ(value->GetReturnTypeId(), OMNI_LONG);

    EXPECT_EQ(index->GetType(), omniruntime::expressions::ExprType::LITERAL_E);
    EXPECT_EQ(index->longVal, 2);
    EXPECT_EQ(index->GetReturnTypeId(), OMNI_LONG);

    delete result;
}

// Test pmod hash
TEST(ParseTest, parsePmodOperation)
{
    int vecTypeCount = 1;
    string expr = "pmod:1(#0, 5:1)";
    std::vector<DataTypePtr> vecOfTypes = {IntType()};
    DataTypes inputTypes(vecOfTypes);
    Parser parser;
    Expr *result = parser.ParseRowExpression(expr, inputTypes, vecTypeCount);
    EXPECT_EQ(result->GetReturnTypeId(), OMNI_INT);
    FuncExpr *absExpr = static_cast<FuncExpr *>(result);
    EXPECT_STREQ(absExpr->funcName.c_str(), "pmod");
    auto args = absExpr->arguments;
    FieldExpr *value = static_cast<FieldExpr *>(args[0]);
    LiteralExpr *index = static_cast<LiteralExpr *>(args[1]);

    EXPECT_EQ(value->GetType(), omniruntime::expressions::ExprType::FIELD_E);
    EXPECT_EQ(value->colVal, 0);
    EXPECT_EQ(value->GetReturnTypeId(), OMNI_INT);

    EXPECT_EQ(index->GetType(), omniruntime::expressions::ExprType::LITERAL_E);
    EXPECT_EQ(index->intVal, 5);
    EXPECT_EQ(index->GetReturnTypeId(), OMNI_INT);

    delete result;
}

// Test null expr
TEST(ParseTest, parseNullExprOperation)
{
    int vecTypeCount = 0;
    string expr = "abs:1(2)";
    std::vector<DataTypePtr> vecOfTypes = {};
    DataTypes inputTypes(vecOfTypes);
    Parser parser;
    Expr *result = parser.ParseRowExpression(expr, inputTypes, vecTypeCount);
    EXPECT_EQ(result, nullptr);

    delete result;
}

// test if column is null
TEST(ParseTest, parseNullStringOperation)
{
    int vecTypeCount = 0;
    string expr = "null:0";
    std::vector<DataTypePtr> vecOfTypes = {};
    DataTypes inputTypes(vecOfTypes);
    Parser parser;
    Expr *result = parser.ParseRowExpression(expr, inputTypes, vecTypeCount);
    FieldExpr *nullExpr = static_cast<FieldExpr *>(result);
    EXPECT_TRUE(nullExpr->isNull);

    delete result;
}

TEST(ParseTest, ParseExpressions)
{
    string exprs[2] = {"$operator$SUBTRACT:1(#0, 10:1)", "$operator$ADD:2(#1, 1:2)"};
    std::vector<DataTypePtr> vecOfTypes = {IntType(), LongType()};
    DataTypes inputTypes(vecOfTypes);
    Parser parser;
    std::vector<Expr *> result = parser.ParseExpressions(exprs, 2, inputTypes);
    // check subtract expression
    EXPECT_EQ(result.at(0)->GetType(), omniruntime::expressions::ExprType::BINARY_E);
    BinaryExpr *subtractExpr = static_cast<BinaryExpr *>(result.at(0));
    EXPECT_EQ(subtractExpr->op, omniruntime::expressions::Operator::SUB);
    FieldExpr *subtractLeft = static_cast<FieldExpr *>(subtractExpr->left);
    LiteralExpr *subtractRight = static_cast<LiteralExpr *>(subtractExpr->right);
    EXPECT_EQ(subtractLeft->GetType(), omniruntime::expressions::ExprType::FIELD_E);
    EXPECT_EQ(subtractLeft->colVal, 0);
    EXPECT_EQ(subtractRight->GetType(), omniruntime::expressions::ExprType::LITERAL_E);
    EXPECT_EQ(subtractRight->intVal, 10);

    // check add expression
    EXPECT_EQ(result.at(1)->GetType(), omniruntime::expressions::ExprType::BINARY_E);
    BinaryExpr *addExpr = static_cast<BinaryExpr *>(result.at(1));
    EXPECT_EQ(addExpr->op, omniruntime::expressions::Operator::ADD);
    FieldExpr *addLeft = static_cast<FieldExpr *>(addExpr->left);
    LiteralExpr *addRight = static_cast<LiteralExpr *>(addExpr->right);
    EXPECT_EQ(addLeft->GetType(), omniruntime::expressions::ExprType::FIELD_E);
    EXPECT_EQ(addLeft->colVal, 1);
    EXPECT_EQ(addRight->GetType(), omniruntime::expressions::ExprType::LITERAL_E);
    EXPECT_EQ(addRight->longVal, 1);

    for (auto i = 0; i < 2; i++) {
        delete result.at(i);
    }
}
} // namespace ParserTest
