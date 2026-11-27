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
#include "gtest/gtest.h"

#include <iostream>
#include <string>
#include <vector>
#include "expression/jsonparser/jsonparser.h"
#include "operator/filter/filter_and_project.h"
#include "util/test_util.h"

using namespace std;
using namespace omniruntime::vec;
using namespace omniruntime::expressions;
using namespace omniruntime::mem;
using namespace omniruntime::op;
using namespace TestUtil;

const string defaultTestFunctionName = "test-function";

using FilterFunc = int32_t (*)(int64_t *, int32_t, int32_t *, int64_t *, int64_t *, int64_t, int64_t *);
using ProjectFunc = int32_t (*)(int64_t *, int32_t, int64_t, int32_t *, int32_t, int64_t *, int64_t *, int32_t *,
                                int32_t *, int64_t, int64_t *);

TEST(CodeGenTest, Operators1)
{
    // create expression objects
    FieldExpr *addLeft = new FieldExpr(0, IntType());
    LiteralExpr *addRight = new LiteralExpr(2, IntType());

    BinaryExpr *gteExpr = new BinaryExpr(omniruntime::expressions::Operator::GTE, addLeft, addRight, BooleanType());

    FieldExpr *ltLeft = new FieldExpr(1, IntType());
    LiteralExpr *ltRight = new LiteralExpr(4, IntType());
    BinaryExpr *ltExpr = new BinaryExpr(omniruntime::expressions::Operator::LT, ltLeft, ltRight, BooleanType());

    FieldExpr *eqLeft = new FieldExpr(2, IntType());
    LiteralExpr *eqRight = new LiteralExpr(2, IntType());
    BinaryExpr *eqExpr = new BinaryExpr(omniruntime::expressions::Operator::EQ, eqLeft, eqRight, BooleanType());

    BinaryExpr *andLeft = new BinaryExpr(omniruntime::expressions::Operator::AND, ltExpr, eqExpr, BooleanType());

    BinaryExpr *expr = new BinaryExpr(omniruntime::expressions::Operator::AND, gteExpr, andLeft, BooleanType());

    ExprPrinter printExprTree;
    expr->Accept(printExprTree);
    cout << endl;

    int32_t v1[1] = {2};
    int32_t v2[1] = {3};
    int32_t v3[1] = {2};
    int64_t *vals = new int64_t[3];
    vals[0] = reinterpret_cast<int64_t>(v1);
    vals[1] = reinterpret_cast<int64_t>(v2);
    vals[2] = reinterpret_cast<int64_t>(v3);
    int32_t *selected = new int32_t[1];

    auto **bitmap = new uint8_t *[3];
    for (int i = 0; i < 3; i++) {
        bitmap[i] = new uint8_t[NullsBuffer::CalculateNbytes(1)];
        BitUtil::SetBit(bitmap[i], 0, false);
    }

    auto **offsets = new int32_t *[3];
    for (int col = 0; col < 3; col++) {
        offsets[col] = new int32_t[1];
    }

    std::vector<DataTypePtr> vecOfTypes = {IntType(), IntType(), IntType()};
    DataTypes inputTypes(vecOfTypes);

    int64_t dictionaries[3] = {};
    auto context = new ExecutionContext();
    auto codegen = FilterCodeGen(defaultTestFunctionName, *expr, nullptr);
    auto func = (FilterFunc)(intptr_t)codegen.GetFunction(inputTypes);

    // number of rows that passed filter
    int32_t result = func(vals, 1, selected, (int64_t *)(bitmap), (int64_t *)(offsets),
                          reinterpret_cast<int64_t>(context), dictionaries);
    EXPECT_EQ(result, 1);

    v1[0] = 2;
    v2[0] = 4;
    v3[0] = 2;
    vals[0] = reinterpret_cast<int64_t>(v1);
    vals[1] = reinterpret_cast<int64_t>(v2);
    vals[2] = reinterpret_cast<int64_t>(v3);

    result = func(vals, 1, selected, (int64_t *)(bitmap), (int64_t *)(offsets), reinterpret_cast<int64_t>(context),
                  dictionaries);
    EXPECT_EQ(result, 0);

    for (int i = 0; i < 3; i++) {
        delete[] bitmap[i];
    }
    for (int i = 0; i < 3; i++) {
        delete[] offsets[i];
    }

    delete[] bitmap;
    delete[] offsets;
    delete[] vals;
    delete[] selected;
    delete expr;
    delete context;
}

TEST(CodeGenTest, MathFunctions1)
{
    // create the expression objects
    FieldExpr *col01 = new FieldExpr(0, IntType());
    FieldExpr *col2 = new FieldExpr(2, IntType());
    DataTypePtr retType = IntType();
    std::string funcStr = "abs";

    std::vector<Expr *> args1;
    args1.push_back(col01);
    auto abs1 = GetFuncExpr(funcStr, args1, IntType());
    std::vector<Expr *> args2;
    args2.push_back(col2);
    auto abs2 = GetFuncExpr(funcStr, args2, IntType());
    auto eq1 = new BinaryExpr(omniruntime::expressions::Operator::EQ, abs1, abs2, BooleanType());

    auto col02 = new FieldExpr(0, IntType());
    auto col1 = new FieldExpr(1, IntType());
    std::vector<Expr *> args3;
    std::vector<Expr *> args4;
    args3.push_back(col02);
    auto abs3 = GetFuncExpr(funcStr, args3, IntType());
    args4.push_back(col1);
    auto abs4 = GetFuncExpr(funcStr, args4, IntType());
    auto eq2 = new BinaryExpr(omniruntime::expressions::Operator::EQ, abs3, abs4, BooleanType());
    auto expr = new BinaryExpr(omniruntime::expressions::Operator::AND, eq1, eq2, BooleanType());

    ExprPrinter printExprTree;
    expr->Accept(printExprTree);
    cout << endl;

    int32_t v1[1] = {-123};
    int32_t v2[1] = {123};
    int32_t v3[1] = {123};
    int64_t *vals = new int64_t[3];
    vals[0] = reinterpret_cast<int64_t>(v1);
    vals[1] = reinterpret_cast<int64_t>(v2);
    vals[2] = reinterpret_cast<int64_t>(v3);
    int32_t *selected = new int32_t[1];

    auto **bitmap = new uint8_t *[3];
    for (int i = 0; i < 3; i++) {
        bitmap[i] = new uint8_t[NullsBuffer::CalculateNbytes(1)];
        BitUtil::SetBit(bitmap[i], 0, false);
    }

    auto **offsets = new int32_t *[3];
    for (int col = 0; col < 3; col++) {
        offsets[col] = new int32_t[1];
    }

    std::vector<DataTypePtr> vecOfTypes = {IntType(), IntType(), IntType()};
    DataTypes inputTypes(vecOfTypes);

    auto codegen = FilterCodeGen(defaultTestFunctionName, *expr, nullptr);

    int64_t dictionaries[3] = {};
    auto context = new ExecutionContext();
    auto func = (FilterFunc)(intptr_t)codegen.GetFunction(inputTypes);

    int32_t result = func(vals, 1, selected, (int64_t *)(bitmap), (int64_t *)(offsets),
                          reinterpret_cast<int64_t>(context), dictionaries);

    context->GetArena()->Reset();

    EXPECT_EQ(result, 1);
    std::cout << "result: " << result << std::endl;

    v1[0] = 10000;
    v2[0] = 10000;
    v3[0] = -10001;
    vals[0] = reinterpret_cast<int64_t>(v1);
    vals[1] = reinterpret_cast<int64_t>(v2);
    vals[2] = reinterpret_cast<int64_t>(v3);

    result = func(vals, 1, selected, (int64_t *)(bitmap), (int64_t *)(offsets), reinterpret_cast<int64_t>(context),
                  dictionaries);
    EXPECT_EQ(result, 0);

    context->GetArena()->Reset();
    for (int i = 0; i < 3; i++) {
        delete[] bitmap[i];
        delete[] offsets[i];
    }
    delete[] offsets;
    delete[] bitmap;
    delete[] vals;
    delete[] selected;
    delete expr;
    delete context;
}

TEST(CodeGenTest, MathFunctions2)
{
    // create expression
    FieldExpr *valueExpr = new FieldExpr(1, IntType());
    FieldExpr *lowerExpr = new FieldExpr(0, IntType());
    FieldExpr *upperExpr = new FieldExpr(2, IntType());
    BetweenExpr *expr = new BetweenExpr(valueExpr, lowerExpr, upperExpr);

    ExprPrinter printExprTree;
    expr->Accept(printExprTree);
    cout << endl;

    int32_t v1[1] = {1001};
    int32_t v2[1] = {1001};
    int32_t v3[1] = {1001};
    int64_t *vals = new int64_t[3];

    vals[0] = reinterpret_cast<int64_t>(v1);
    vals[1] = reinterpret_cast<int64_t>(v2);
    vals[2] = reinterpret_cast<int64_t>(v3);
    int32_t *selected = new int32_t[1];

    auto **bitmap = new uint8_t *[3];
    for (int i = 0; i < 3; i++) {
        bitmap[i] = new uint8_t[NullsBuffer::CalculateNbytes(1)];
        BitUtil::SetBit(bitmap[i], 0, false);
    }

    auto **offsets = new int32_t *[3];
    for (int col = 0; col < 3; col++) {
        offsets[col] = new int32_t[1];
    }

    std::vector<DataTypePtr> vecOfTypes = {IntType(), IntType(), IntType()};
    DataTypes inputTypes(vecOfTypes);

    auto codegen = FilterCodeGen(defaultTestFunctionName, *expr, nullptr);
    int64_t dictionaries[3] = {};

    auto context = new ExecutionContext();
    auto func = (FilterFunc)(intptr_t)codegen.GetFunction(inputTypes);

    int32_t result = func(vals, 1, selected, (int64_t *)(bitmap), (int64_t *)(offsets),
                          reinterpret_cast<int64_t>(context), dictionaries);

    EXPECT_EQ(result, 1);
    context->GetArena()->Reset();

    v1[0] = 100;
    v2[0] = 1245;
    v3[0] = -12356;
    vals[0] = reinterpret_cast<int64_t>(v1);
    vals[1] = reinterpret_cast<int64_t>(v2);
    vals[2] = reinterpret_cast<int64_t>(v3);

    result = func(vals, 1, selected, (int64_t *)(bitmap), (int64_t *)(offsets), reinterpret_cast<int64_t>(context),
                  dictionaries);
    EXPECT_EQ(result, 0);
    context->GetArena()->Reset();

    v1[0] = 100;
    v2[0] = 1245;
    v3[0] = 12356;
    vals[0] = reinterpret_cast<int64_t>(v1);
    vals[1] = reinterpret_cast<int64_t>(v2);
    vals[2] = reinterpret_cast<int64_t>(v3);

    result = func(vals, 1, selected, (int64_t *)(bitmap), (int64_t *)(offsets), reinterpret_cast<int64_t>(context),
                  dictionaries);
    EXPECT_EQ(result, 1);
    context->GetArena()->Reset();

    for (int i = 0; i < 3; i++) {
        delete[] bitmap[i];
        delete[] offsets[i];
    }
    delete[] offsets;
    delete[] bitmap;
    delete[] vals;
    delete[] selected;
    delete expr;
    delete context;
}

TEST(CodeGenTest, MathFunctions3)
{
    // create expression objects
    FieldExpr *col01 = new FieldExpr(0, IntType());
    LiteralExpr *data01 = new LiteralExpr(100, IntType());
    BinaryExpr *condition = new BinaryExpr(omniruntime::expressions::Operator::GT, col01, data01, BooleanType());

    FieldExpr *col02 = new FieldExpr(0, IntType());
    LiteralExpr *data02 = new LiteralExpr(200, IntType());
    BinaryExpr *texp = new BinaryExpr(omniruntime::expressions::Operator::GT, col02, data02, BooleanType());

    FieldExpr *col03 = new FieldExpr(0, IntType());
    LiteralExpr *data03 = new LiteralExpr(0, IntType());
    BinaryExpr *fexp = new BinaryExpr(omniruntime::expressions::Operator::LT, col03, data03, BooleanType());
    IfExpr *expr = new IfExpr(condition, texp, fexp);

    ExprPrinter printExprTree;
    expr->Accept(printExprTree);
    cout << endl;

    int32_t v1[1] = {1001};
    int32_t v2[1] = {0};
    int32_t v3[1] = {21};
    int64_t *vals = new int64_t[3];
    vals[0] = reinterpret_cast<int64_t>(v1);
    vals[1] = reinterpret_cast<int64_t>(v2);
    vals[2] = reinterpret_cast<int64_t>(v3);
    int32_t *selected = new int32_t[1];

    auto **bitmap = new uint8_t *[3];
    for (int i = 0; i < 3; i++) {
        bitmap[i] = new uint8_t[NullsBuffer::CalculateNbytes(1)];
        BitUtil::SetBit(bitmap[i], 0, false);
    }

    auto **offsets = new int32_t *[3];
    for (int col = 0; col < 3; col++) {
        offsets[col] = new int32_t[1];
    }

    std::vector<DataTypePtr> vecOfTypes = {IntType()};
    DataTypes inputTypes(vecOfTypes);

    auto codegen = FilterCodeGen(defaultTestFunctionName, *expr, nullptr);
    int64_t dictionaries[3] = {};

    auto context = new ExecutionContext();
    auto func = (FilterFunc)(intptr_t)codegen.GetFunction(inputTypes);

    int32_t result = func(vals, 1, selected, (int64_t *)(bitmap), (int64_t *)(offsets),
                          reinterpret_cast<int64_t>(context), dictionaries);
    EXPECT_EQ(result, 1);
    context->GetArena()->Reset();

    v1[0] = 100;
    v2[0] = 1245;
    v3[0] = -12356;
    vals[0] = reinterpret_cast<int64_t>(v1);
    vals[1] = reinterpret_cast<int64_t>(v2);
    vals[2] = reinterpret_cast<int64_t>(v3);

    result = func(vals, 1, selected, (int64_t *)(bitmap), (int64_t *)(offsets), reinterpret_cast<int64_t>(context),
                  dictionaries);
    EXPECT_EQ(result, 0);
    context->GetArena()->Reset();

    v1[0] = -12;
    v2[0] = 1245;
    v3[0] = 123;
    vals[0] = reinterpret_cast<int64_t>(v1);
    vals[1] = reinterpret_cast<int64_t>(v2);
    vals[2] = reinterpret_cast<int64_t>(v3);

    result = func(vals, 1, selected, (int64_t *)(bitmap), (int64_t *)(offsets), reinterpret_cast<int64_t>(context),
                  dictionaries);
    EXPECT_EQ(result, 1);
    context->GetArena()->Reset();

    v1[0] = -12222;
    v2[0] = -12312;
    v3[0] = 42;
    vals[0] = reinterpret_cast<int64_t>(v1);
    vals[1] = reinterpret_cast<int64_t>(v2);
    vals[2] = reinterpret_cast<int64_t>(v3);

    result = func(vals, 1, selected, (int64_t *)(bitmap), (int64_t *)(offsets), reinterpret_cast<int64_t>(context),
                  dictionaries);
    EXPECT_EQ(result, 1);
    context->GetArena()->Reset();

    for (int i = 0; i < 3; i++) {
        delete[] bitmap[i];
        delete[] offsets[i];
    }
    delete[] offsets;
    delete[] bitmap;
    delete[] vals;
    delete[] selected;
    delete expr;
    delete context;
}

TEST(CodeGenTest, MathFunctions4)
{
    // create expression objects
    FieldExpr *col = new FieldExpr(0, IntType());
    std::vector<Expr *> args;
    args.push_back(col);
    for (int32_t i = 1; i < 6; i++) {
        args.push_back(new LiteralExpr(i, IntType()));
    }
    InExpr *expr = new InExpr(args);

    ExprPrinter printExprTree;
    expr->Accept(printExprTree);
    cout << endl;

    int32_t v1[1] = {1};
    int32_t v2[1] = {0};
    int32_t v3[1] = {21};
    int64_t *vals = new int64_t[3];
    vals[0] = reinterpret_cast<int64_t>(v1);
    vals[1] = reinterpret_cast<int64_t>(v2);
    vals[2] = reinterpret_cast<int64_t>(v3);
    int32_t *selected = new int32_t[1];

    auto **bitmap = new uint8_t *[3];
    for (int i = 0; i < 3; i++) {
        bitmap[i] = new uint8_t[NullsBuffer::CalculateNbytes(1)];
        BitUtil::SetBit(bitmap[i], 0, false);
    }

    auto **offsets = new int32_t *[3];
    for (int col = 0; col < 3; col++) {
        offsets[col] = new int32_t[1];
    }

    std::vector<DataTypePtr> vecOfTypes = {IntType()};
    DataTypes inputTypes(vecOfTypes);

    auto codegen = FilterCodeGen(defaultTestFunctionName, *expr, nullptr);
    int64_t dictionaries[3] = {};

    auto context = new ExecutionContext();
    auto func = (FilterFunc)(intptr_t)codegen.GetFunction(inputTypes);

    int32_t result = func(vals, 1, selected, (int64_t *)(bitmap), (int64_t *)(offsets),
                          reinterpret_cast<int64_t>(context), dictionaries);
    EXPECT_EQ(result, 1);
    context->GetArena()->Reset();

    v1[0] = 3;
    v2[0] = 1245;
    v3[0] = -12356;
    vals[0] = reinterpret_cast<int64_t>(v1);
    vals[1] = reinterpret_cast<int64_t>(v2);
    vals[2] = reinterpret_cast<int64_t>(v3);

    result = func(vals, 1, selected, (int64_t *)(bitmap), (int64_t *)(offsets), reinterpret_cast<int64_t>(context),
                  dictionaries);
    EXPECT_EQ(result, 1);
    context->GetArena()->Reset();

    v1[0] = 5;
    v2[0] = 1245;
    v3[0] = 123;
    vals[0] = reinterpret_cast<int64_t>(v1);
    vals[1] = reinterpret_cast<int64_t>(v2);
    vals[2] = reinterpret_cast<int64_t>(v3);

    result = func(vals, 1, selected, (int64_t *)(bitmap), (int64_t *)(offsets), reinterpret_cast<int64_t>(context),
                  dictionaries);
    EXPECT_EQ(result, 1);
    context->GetArena()->Reset();

    v1[0] = 0;
    v2[0] = -12312;
    v3[0] = 42;
    vals[0] = reinterpret_cast<int64_t>(v1);
    vals[1] = reinterpret_cast<int64_t>(v2);
    vals[2] = reinterpret_cast<int64_t>(v3);

    result = func(vals, 1, selected, (int64_t *)(bitmap), (int64_t *)(offsets), reinterpret_cast<int64_t>(context),
                  dictionaries);
    EXPECT_EQ(result, 0);
    context->GetArena()->Reset();

    v1[0] = 123;
    v2[0] = -43;
    v3[0] = 542;
    vals[0] = reinterpret_cast<int64_t>(v1);
    vals[1] = reinterpret_cast<int64_t>(v2);
    vals[2] = reinterpret_cast<int64_t>(v3);

    result = func(vals, 1, selected, (int64_t *)(bitmap), (int64_t *)(offsets), reinterpret_cast<int64_t>(context),
                  dictionaries);
    EXPECT_EQ(result, 0);

    for (int i = 0; i < 3; i++) {
        delete[] bitmap[i];
        delete[] offsets[i];
    }
    delete[] offsets;
    delete[] bitmap;
    delete[] vals;
    delete[] selected;
    delete expr;
    delete context;
}

// For testing different types
TEST(CodeGenTest, CastNumbers1)
{
    std::string castStr = "CAST";
    std::string absStr = "abs";
    DataTypePtr retType = DoubleType();
    // create expression objects
    auto col0 = new FieldExpr(0, IntType());
    std::vector<Expr *> args01;
    args01.push_back(col0);
    auto cast0 = GetFuncExpr(castStr, args01, DoubleType());
    std::vector<Expr *> args02;
    args02.push_back(cast0);
    auto abs0 = GetFuncExpr(absStr, args02, DoubleType());

    auto col1 = new FieldExpr(1, IntType());
    std::vector<Expr *> args11;
    args11.push_back(col1);
    auto cast1 = GetFuncExpr(castStr, args11, DoubleType());
    std::vector<Expr *> args12;
    args12.push_back(cast1);
    auto abs1 = GetFuncExpr(absStr, args12, DoubleType());

    auto expr = new BinaryExpr(omniruntime::expressions::Operator::EQ, abs0, abs1, BooleanType());

    ExprPrinter printExprTree;
    expr->Accept(printExprTree);
    cout << endl;

    int32_t v1[1] = {10000};
    int64_t v2[1] = {10000};
    double v3[1] = {12.34};
    int64_t *vals = new int64_t[3];
    vals[0] = reinterpret_cast<int64_t>(v1);
    vals[1] = reinterpret_cast<int64_t>(v2);
    vals[2] = reinterpret_cast<int64_t>(v3);
    int32_t *selected = new int32_t[1];

    auto **bitmap = new uint8_t *[3];
    for (int i = 0; i < 3; i++) {
        bitmap[i] = new uint8_t[NullsBuffer::CalculateNbytes(1)];
        BitUtil::SetBit(bitmap[i], 0, false);
    }

    auto **offsets = new int32_t *[3];
    for (int col = 0; col < 3; col++) {
        offsets[col] = new int32_t[1];
    }

    std::vector<DataTypePtr> vecOfTypes = {IntType(), IntType()};
    DataTypes inputTypes(vecOfTypes);

    auto codegen = FilterCodeGen(defaultTestFunctionName, *expr, nullptr);
    int64_t dictionaries[3] = {};

    auto context = new ExecutionContext();
    auto func = (FilterFunc)(intptr_t)codegen.GetFunction(inputTypes);

    int32_t result = func(vals, 1, selected, (int64_t *)(bitmap), (int64_t *)(offsets),
                          reinterpret_cast<int64_t>(context), dictionaries);
    EXPECT_EQ(result, 1);
    context->GetArena()->Reset();

    v1[0] = 2000000000;
    v2[0] = 3000000000;
    v3[0] = -234;
    vals[0] = reinterpret_cast<int64_t>(v1);
    vals[1] = reinterpret_cast<int64_t>(v2);
    vals[2] = reinterpret_cast<int64_t>(v3);

    result = func(vals, 1, selected, (int64_t *)(bitmap), (int64_t *)(offsets), reinterpret_cast<int64_t>(context),
                  dictionaries);
    EXPECT_EQ(result, 0);
    context->GetArena()->Reset();

    v1[0] = -1000000;
    v2[0] = -1000000;
    v3[0] = 133.324234;
    vals[0] = reinterpret_cast<int64_t>(v1);
    vals[1] = reinterpret_cast<int64_t>(v2);
    vals[2] = reinterpret_cast<int64_t>(v3);

    result = func(vals, 1, selected, (int64_t *)(bitmap), (int64_t *)(offsets), reinterpret_cast<int64_t>(context),
                  dictionaries);
    EXPECT_EQ(result, 1);
    context->GetArena()->Reset();

    for (int i = 0; i < 3; i++) {
        delete[] bitmap[i];
        delete[] offsets[i];
    }
    delete[] offsets;
    delete[] bitmap;
    delete[] vals;
    delete[] selected;
    delete expr;
    delete context;
}

TEST(CodeGenTest, CastNumbers2)
{
    // create expression objects
    std::string castStr = "CAST";
    auto col1 = new FieldExpr(1, LongType());
    DataTypePtr retType = DoubleType();
    std::vector<Expr *> args;
    args.push_back(col1);
    auto cast = GetFuncExpr(castStr, args, DoubleType());
    auto col2 = new FieldExpr(2, DoubleType());
    auto expr = new BinaryExpr(omniruntime::expressions::Operator::GT, cast, col2, BooleanType());

    ExprPrinter printExprTree;
    expr->Accept(printExprTree);
    cout << endl;

    int32_t v1[1] = {324233};
    int64_t v2[1] = {12};
    double v3[1] = {12.34};
    int64_t *vals = new int64_t[3];
    vals[0] = reinterpret_cast<int64_t>(v1);
    vals[1] = reinterpret_cast<int64_t>(v2);
    vals[2] = reinterpret_cast<int64_t>(v3);
    int32_t *selected = new int32_t[1];

    auto **bitmap = new uint8_t *[3];
    for (int i = 0; i < 3; i++) {
        bitmap[i] = new uint8_t[NullsBuffer::CalculateNbytes(1)];
        BitUtil::SetBit(bitmap[i], 0, false);
    }

    auto **offsets = new int32_t *[3];
    for (int col = 0; col < 3; col++) {
        offsets[col] = new int32_t[1];
    }

    std::vector<DataTypePtr> vecOfTypes = {IntType(), LongType(), DoubleType()};
    DataTypes inputTypes(vecOfTypes);

    auto codegen = FilterCodeGen(defaultTestFunctionName, *expr, nullptr);
    int64_t dictionaries[3] = {};

    auto context = new ExecutionContext();
    auto func = (FilterFunc)(intptr_t)codegen.GetFunction(inputTypes);

    int32_t result = func(vals, 1, selected, (int64_t *)(bitmap), (int64_t *)(offsets),
                          reinterpret_cast<int64_t>(context), dictionaries);
    EXPECT_EQ(result, 0);
    context->GetArena()->Reset();

    v1[0] = 2000000000;
    v2[0] = -233;
    v3[0] = -234.2142;
    vals[0] = reinterpret_cast<int64_t>(v1);
    vals[1] = reinterpret_cast<int64_t>(v2);
    vals[2] = reinterpret_cast<int64_t>(v3);

    result = func(vals, 1, selected, (int64_t *)(bitmap), (int64_t *)(offsets), reinterpret_cast<int64_t>(context),
                  dictionaries);
    EXPECT_EQ(result, 1);
    context->GetArena()->Reset();

    v1[0] = -1000000;
    v2[0] = 12;
    v3[0] = 12;
    vals[0] = reinterpret_cast<int64_t>(v1);
    vals[1] = reinterpret_cast<int64_t>(v2);
    vals[2] = reinterpret_cast<int64_t>(v3);

    result = func(vals, 1, selected, (int64_t *)(bitmap), (int64_t *)(offsets), reinterpret_cast<int64_t>(context),
                  dictionaries);
    EXPECT_EQ(result, 0);
    context->GetArena()->Reset();

    for (int i = 0; i < 3; i++) {
        delete[] bitmap[i];
        delete[] offsets[i];
    }
    delete[] offsets;
    delete[] bitmap;
    delete[] vals;
    delete[] selected;
    delete expr;
    delete context;
}

TEST(CodeGenTest, Like)
{
    // create expression objects
    std::string funcStr = "LIKE";
    DataTypePtr retType = BooleanType();
    auto col = new FieldExpr(2, VarcharType());
    auto data = new LiteralExpr(new std::string(".*hello.*world.*"), VarcharType(17));
    std::vector<Expr *> args;
    args.push_back(col);
    args.push_back(data);
    auto expr = GetFuncExpr(funcStr, args, BooleanType());

    ExprPrinter printExprTree;
    expr->Accept(printExprTree);
    cout << endl;

    string s1[1];
    string s2[1];

    int32_t v1[1] = {8766};
    s1[0] = "asdf";
    s2[0] = "asjd fehellojdsl kfjworlddslk  jf ";
    int64_t *vals = new int64_t[3];
    vals[0] = reinterpret_cast<int64_t>(v1);
    vals[1] = reinterpret_cast<int64_t>(s1->c_str());
    vals[2] = reinterpret_cast<int64_t>(s2->c_str());
    int32_t *selected = new int32_t[1];

    auto **bitmap = new uint8_t *[3];
    for (int i = 0; i < 3; i++) {
        bitmap[i] = new uint8_t[NullsBuffer::CalculateNbytes(1)];
        BitUtil::SetBit(bitmap[i], 0, false);
    }

    auto **offsets = new int32_t *[3];
    offsets[0] = new int32_t[1];
    offsets[1] = new int32_t[2];
    offsets[1][0] = 0;
    offsets[1][1] = s1[0].length();
    offsets[2] = new int32_t[2];
    offsets[2][0] = 0;
    offsets[2][1] = s2[0].length();

    std::vector<DataTypePtr> vecOfTypes = {IntType(), VarcharType(), VarcharType()};
    DataTypes inputTypes(vecOfTypes);

    auto codegen = FilterCodeGen(defaultTestFunctionName, *expr, nullptr);
    int64_t dictionaries[3] = {};

    auto context = new ExecutionContext();
    auto func = (FilterFunc)(intptr_t)codegen.GetFunction(inputTypes);

    int32_t result = func(vals, 1, selected, (int64_t *)(bitmap), (int64_t *)(offsets),
                          reinterpret_cast<int64_t>(context), dictionaries);
    EXPECT_EQ(result, 1);
    context->GetArena()->Reset();

    v1[0] = {8766};
    s1[0] = "asdf";
    s2[0] = "asjd fehell ojdsl kfjwo rld dslk  jf ";
    vals[0] = reinterpret_cast<int64_t>(v1);
    vals[1] = reinterpret_cast<int64_t>(s1->c_str());
    vals[2] = reinterpret_cast<int64_t>(s2->c_str());

    offsets[2][1] = s2->length();

    result = func(vals, 1, selected, (int64_t *)(bitmap), (int64_t *)(offsets), reinterpret_cast<int64_t>(context),
                  dictionaries);
    EXPECT_EQ(result, 0);
    context->GetArena()->Reset();

    for (int i = 0; i < 3; i++) {
        delete[] bitmap[i];
        delete[] offsets[i];
    }
    delete[] bitmap;
    delete[] offsets;
    delete[] vals;
    delete[] selected;
    delete expr;
    delete context;
}

TEST(CodeGenTest, DISABLED_CountChar)
{
    std::string funcStr = "CountChar";
    DataTypePtr retType = LongType();
    auto col = new FieldExpr(2, VarcharType());
    auto data = new LiteralExpr(new std::string("l"), CharType(2));
    std::vector<Expr *> args;
    args.push_back(col);
    args.push_back(data);
    auto countExpr = GetFuncExpr(funcStr, args, LongType());

    auto count = new LiteralExpr(4L, LongType());
    auto expr = new BinaryExpr(omniruntime::expressions::Operator::EQ, countExpr, count, BooleanType());

    ExprPrinter printExprTree;
    expr->Accept(printExprTree);
    cout << endl;

    string s1[1];
    string s2[1];

    int32_t v1[1] = {8766};
    s1[0] = "asdf";
    s2[0] = "Ella";
    int64_t *vals = new int64_t[3];
    vals[0] = reinterpret_cast<int64_t>(v1);
    vals[1] = reinterpret_cast<int64_t>(s1->c_str());
    vals[2] = reinterpret_cast<int64_t>(s2->c_str());
    int32_t *selected = new int32_t[1];

    auto **bitmap = new bool *[3];
    for (int i = 0; i < 3; i++) {
        bitmap[i] = new bool[1];
        bitmap[i][0] = false;
    }

    auto **offsets = new int32_t *[3];
    offsets[0] = new int32_t[1];
    offsets[1] = new int32_t[2];
    offsets[1][0] = 0;
    offsets[1][1] = s1[0].length();
    offsets[2] = new int32_t[2];
    offsets[2][0] = 0;
    offsets[2][1] = s2[0].length();

    std::vector<DataTypePtr> vecOfTypes = {IntType(), VarcharType(), VarcharType()};
    DataTypes inputTypes(vecOfTypes);

    auto codegen = FilterCodeGen(defaultTestFunctionName, *expr, nullptr);
    int64_t dictionaries[3] = {};

    auto context = new ExecutionContext();
    auto func = (FilterFunc)(intptr_t)codegen.GetFunction(inputTypes);

    int32_t result = func(vals, 1, selected, (int64_t *)(bitmap), (int64_t *)(offsets),
                          reinterpret_cast<int64_t>(context), dictionaries);
    EXPECT_EQ(result, 0);
    context->GetArena()->Reset();

    v1[0] = {8766};
    s1[0] = "asdf";
    s2[0] = "lLlL";
    vals[0] = reinterpret_cast<int64_t>(v1);
    vals[1] = reinterpret_cast<int64_t>(s1->c_str());
    vals[2] = reinterpret_cast<int64_t>(s2->c_str());

    offsets[2][1] = s2->length();

    result = func(vals, 1, selected, (int64_t *)(bitmap), (int64_t *)(offsets), reinterpret_cast<int64_t>(context),
                  dictionaries);
    EXPECT_EQ(result, 1);
    context->GetArena()->Reset();

    for (int i = 0; i < 3; i++) {
        delete[] bitmap[i];
        delete[] offsets[i];
    }
    delete[] bitmap;
    delete[] offsets;
    delete[] vals;
    delete[] selected;
    delete expr;
    delete context;
}

TEST(CodeGenTest, DISABLED_SplitIndex)
{
    std::string funcStr = "SplitIndex";
    DataTypePtr retType = VarcharType();
    auto col = new FieldExpr(2, VarcharType());
    auto delimiter = new LiteralExpr(new std::string(","), CharType(2));
    auto index = new LiteralExpr(2, IntType());
    std::vector<Expr *> args;
    args.push_back(col);
    args.push_back(delimiter);
    args.push_back(index);
    auto splitExpr = GetFuncExpr(funcStr, args, VarcharType());

    auto element = new LiteralExpr(new std::string("Mary"), VarcharType());
    auto expr = new BinaryExpr(omniruntime::expressions::Operator::EQ, splitExpr, element, BooleanType());

    ExprPrinter printExprTree;
    expr->Accept(printExprTree);
    cout << endl;

    string s1[1];
    string s2[1];

    int32_t v1[1] = {8766};
    s1[0] = "asdf";
    s2[0] = "Jack,John,Mary";
    int64_t *vals = new int64_t[3];
    vals[0] = reinterpret_cast<int64_t>(v1);
    vals[1] = reinterpret_cast<int64_t>(s1->c_str());
    vals[2] = reinterpret_cast<int64_t>(s2->c_str());
    int32_t *selected = new int32_t[2];

    bool **bitmap = new bool *[3];
    for (int i = 0; i < 3; i++) {
        bitmap[i] = new bool[1];
        bitmap[i][0] = false;
    }

    auto **offsets = new int32_t *[3];
    offsets[0] = new int32_t[1];
    offsets[1] = new int32_t[2];
    offsets[1][0] = 0;
    offsets[1][1] = s1[0].length();
    offsets[2] = new int32_t[2];
    offsets[2][0] = 0;
    offsets[2][1] = s2[0].length();

    std::vector<DataTypePtr> vecOfTypes = {IntType(), VarcharType(), VarcharType()};
    DataTypes inputTypes(vecOfTypes);

    auto codegen = FilterCodeGen(defaultTestFunctionName, *expr, nullptr);
    int64_t dictionaries[3] = {};

    auto context = new ExecutionContext();
    auto func = (FilterFunc)(intptr_t)codegen.GetFunction(inputTypes);

    int32_t result = func(vals, 1, selected, (int64_t *)(bitmap), (int64_t *)(offsets),
                          reinterpret_cast<int64_t>(context), dictionaries);
    EXPECT_EQ(result, 1);
    context->GetArena()->Reset();

    v1[0] = {8766};
    s1[0] = "asdf";
    s2[0] = "Jack,John,Luke";
    vals[0] = reinterpret_cast<int64_t>(v1);
    vals[1] = reinterpret_cast<int64_t>(s1->c_str());
    vals[2] = reinterpret_cast<int64_t>(s2->c_str());

    offsets[2][1] = s2->length();

    result = func(vals, 1, selected, (int64_t *)(bitmap), (int64_t *)(offsets), reinterpret_cast<int64_t>(context),
                  dictionaries);
    EXPECT_EQ(result, 0);
    context->GetArena()->Reset();

    for (int i = 0; i < 3; i++) {
        delete[] bitmap[i];
        delete[] offsets[i];
    }
    delete[] bitmap;
    delete[] offsets;
    delete[] vals;
    delete[] selected;
    delete expr;
    delete context;
}

TEST(CodeGenTest, DateCast)
{
    // create expression objects
    std::string funcStr = "CAST";
    auto col2 = new FieldExpr(2, VarcharType());
    std::vector<Expr *> args;
    args.push_back(col2);
    DataTypePtr retType = IntType();
    auto cast = GetFuncExpr(funcStr, args, Date32Type());
    auto col0 = new FieldExpr(0, Date32Type());
    auto expr = new BinaryExpr(omniruntime::expressions::Operator::GT, cast, col0, BooleanType());

    ExprPrinter printExprTree;
    expr->Accept(printExprTree);
    cout << endl;

    string s1[1];
    string s2[1];

    int32_t v1[1] = {8766};
    s1[0] = "asdf";
    s2[0] = "1994-01-01";
    int64_t *vals = new int64_t[3];
    vals[0] = reinterpret_cast<int64_t>(v1);
    vals[1] = reinterpret_cast<int64_t>(s1->c_str());
    vals[2] = reinterpret_cast<int64_t>(s2->c_str());
    int32_t *selected = new int32_t[1];

    auto **bitmap = new uint8_t *[3];
    for (int i = 0; i < 3; i++) {
        bitmap[i] = new uint8_t[NullsBuffer::CalculateNbytes(1)];
        BitUtil::SetBit(bitmap[i], 0, false);
    }

    auto **offsets = new int32_t *[3];
    offsets[0] = new int32_t[1];
    offsets[1] = new int32_t[2];
    offsets[1][0] = 0;
    offsets[1][1] = s1[0].length();
    offsets[2] = new int32_t[2];
    offsets[2][0] = 0;
    offsets[2][1] = s2[0].length();

    std::vector<DataTypePtr> vecOfTypes = {IntType(), VarcharType(), VarcharType()};
    DataTypes inputTypes(vecOfTypes);

    auto codegen = FilterCodeGen(defaultTestFunctionName, *expr, nullptr);
    int64_t dictionaries[3] = {};

    auto context = new ExecutionContext();
    auto func = (FilterFunc)(intptr_t)codegen.GetFunction(inputTypes);

    int32_t result = func(vals, 1, selected, (int64_t *)(bitmap), (int64_t *)(offsets),
                          reinterpret_cast<int64_t>(context), dictionaries);
    EXPECT_EQ(result, 0);
    context->GetArena()->Reset();

    v1[0] = {8766};
    s1[0] = "j";
    s2[0] = "1996-01-02";
    vals[0] = reinterpret_cast<int64_t>(v1);
    vals[1] = reinterpret_cast<int64_t>(s1->c_str());
    vals[2] = reinterpret_cast<int64_t>(s2->c_str());

    offsets[1][1] = s1[0].length();
    offsets[2][1] = s2[0].length();

    result = func(vals, 1, selected, (int64_t *)(bitmap), (int64_t *)(offsets), reinterpret_cast<int64_t>(context),
                  dictionaries);
    EXPECT_EQ(result, 1);
    context->GetArena()->Reset();

    v1[0] = {8766};
    s1[0] = "j";
    s2[0] = "1993-11-12";
    vals[0] = reinterpret_cast<int64_t>(v1);
    vals[1] = reinterpret_cast<int64_t>(s1->c_str());
    vals[2] = reinterpret_cast<int64_t>(s2->c_str());

    offsets[1][1] = s1[0].length();
    offsets[2][1] = s2[0].length();

    result = func(vals, 1, selected, (int64_t *)(bitmap), (int64_t *)(offsets), reinterpret_cast<int64_t>(context),
                  dictionaries);
    EXPECT_EQ(result, 0);
    context->GetArena()->Reset();

    for (int i = 0; i < 3; i++) {
        delete[] bitmap[i];
        delete[] offsets[i];
    }
    delete[] offsets;
    delete[] bitmap;
    delete[] vals;
    delete[] selected;
    delete expr;
    delete context;
}

TEST(CodeGenTest, SubstrIn)
{
    // create the expression objects
    auto col2 = new FieldExpr(2, VarcharType());
    auto substrIndex = new LiteralExpr(1, IntType());
    auto substrLen = new LiteralExpr(2, IntType());
    DataTypePtr retType = VarcharType();
    std::string funcStr = "substr";
    std::vector<Expr *> substrArgs;
    substrArgs.push_back(col2);
    substrArgs.push_back(substrIndex);
    substrArgs.push_back(substrLen);
    auto substrExpr = GetFuncExpr(funcStr, substrArgs, VarcharType());

    std::vector<Expr *> args;
    args.push_back(substrExpr);
    args.push_back(new LiteralExpr(new std::string("12"), VarcharType(3)));
    args.push_back(new LiteralExpr(new std::string("21"), VarcharType(3)));
    args.push_back(new LiteralExpr(new std::string("13"), VarcharType(3)));
    args.push_back(new LiteralExpr(new std::string("31"), VarcharType(3)));
    args.push_back(new LiteralExpr(new std::string("34"), VarcharType(3)));
    args.push_back(new LiteralExpr(new std::string("43"), VarcharType(3)));

    InExpr *expr = new InExpr(args);

    ExprPrinter printExprTree;
    expr->Accept(printExprTree);
    cout << endl;

    string s1[1];
    string s2[1];
    int32_t v1[1] = {8766};
    s1[0] = "asdf";
    s2[0] = "2134121";

    int64_t *vals = new int64_t[3];
    vals[0] = reinterpret_cast<int64_t>(v1);
    vals[1] = reinterpret_cast<int64_t>(s1->c_str());
    vals[2] = reinterpret_cast<int64_t>(s2->c_str());
    int32_t *selected = new int32_t[1];

    auto **bitmap = new uint8_t *[3];
    for (int i = 0; i < 3; i++) {
        bitmap[i] = new uint8_t[NullsBuffer::CalculateNbytes(1)];
        BitUtil::SetBit(bitmap[i], 0, false);
    }

    auto **offsets = new int32_t *[3];
    offsets[0] = new int32_t[1];
    offsets[1] = new int32_t[2];
    offsets[1][0] = 0;
    offsets[1][1] = s1[0].length();
    offsets[2] = new int32_t[2];
    offsets[2][0] = 0;
    offsets[2][1] = s2[0].length();

    std::vector<DataTypePtr> vecOfTypes = {IntType(), VarcharType(), VarcharType()};
    DataTypes inputTypes(vecOfTypes);

    auto codegen = FilterCodeGen(defaultTestFunctionName, *expr, nullptr);
    int64_t dictionaries[3] = {};

    auto context = new ExecutionContext();
    auto func = (FilterFunc)(intptr_t)codegen.GetFunction(inputTypes);

    int32_t result = func(vals, 1, selected, (int64_t *)(bitmap), (int64_t *)(offsets),
                          reinterpret_cast<int64_t>(context), dictionaries);

    EXPECT_EQ(result, 1);
    context->GetArena()->Reset();

    v1[0] = {8766};
    s1[0] = "j";
    s2[0] = "233425";
    vals[0] = reinterpret_cast<int64_t>(v1);
    vals[1] = reinterpret_cast<int64_t>(s1->c_str());
    vals[2] = reinterpret_cast<int64_t>(s2->c_str());

    offsets[1][1] = s1[0].length();
    offsets[2][1] = s2[0].length();

    result = func(vals, 1, selected, (int64_t *)(bitmap), (int64_t *)(offsets), reinterpret_cast<int64_t>(context),
                  dictionaries);
    EXPECT_EQ(result, 0);
    context->GetArena()->Reset();

    v1[0] = {8766};
    s1[0] = "j";
    s2[0] = "424321";
    vals[0] = reinterpret_cast<int64_t>(v1);
    vals[1] = reinterpret_cast<int64_t>(s1->c_str());
    vals[2] = reinterpret_cast<int64_t>(s2->c_str());

    offsets[1][1] = s1[0].length();
    offsets[2][1] = s2[0].length();

    result = func(vals, 1, selected, (int64_t *)(bitmap), (int64_t *)(offsets), reinterpret_cast<int64_t>(context),
                  dictionaries);
    EXPECT_EQ(result, 0);
    context->GetArena()->Reset();

    for (int i = 0; i < 3; i++) {
        delete[] bitmap[i];
        delete[] offsets[i];
    }
    delete[] offsets;
    delete[] bitmap;
    delete[] vals;
    delete[] selected;
    delete expr;
    delete context;
}

TEST(CodeGenTest, ConcatStr)
{
    std::string funcStr = "concat";
    auto col1 = new FieldExpr(1, VarcharType());
    auto col2 = new FieldExpr(2, VarcharType());
    std::vector<Expr *> concatArgs;
    concatArgs.push_back(col1);
    concatArgs.push_back(col2);
    DataTypePtr retType = VarcharType();
    auto concatExpr = GetFuncExpr(funcStr, concatArgs, VarcharType());

    auto helloWorldExpr = new LiteralExpr(new std::string("helloworld"), VarcharType(11));
    auto expr = new BinaryExpr(omniruntime::expressions::Operator::EQ, concatExpr, helloWorldExpr, BooleanType());

    ExprPrinter printExprTree;
    expr->Accept(printExprTree);
    cout << endl;

    string s1[1];
    string s2[1];
    int32_t v1[1] = {8766};
    s1[0] = "hello";
    s2[0] = "world";
    int64_t *vals = new int64_t[3];
    vals[0] = reinterpret_cast<int64_t>(v1);
    vals[1] = reinterpret_cast<int64_t>(s1->c_str());
    vals[2] = reinterpret_cast<int64_t>(s2->c_str());
    int32_t *selected = new int32_t[1];

    auto **bitmap = new uint8_t *[3];
    for (int i = 0; i < 3; i++) {
        bitmap[i] = new uint8_t[NullsBuffer::CalculateNbytes(1)];
        BitUtil::SetBit(bitmap[i], 0, false);
    }

    auto **offsets = new int32_t *[3];
    offsets[0] = new int32_t[1];
    offsets[1] = new int32_t[2];
    offsets[1][0] = 0;
    offsets[1][1] = s1[0].length();
    offsets[2] = new int32_t[2];
    offsets[2][0] = 0;
    offsets[2][1] = s2[0].length();

    std::vector<DataTypePtr> vecOfTypes = {IntType(), VarcharType(), VarcharType()};
    DataTypes inputTypes(vecOfTypes);

    auto codegen = FilterCodeGen(defaultTestFunctionName, *expr, nullptr);
    int64_t dictionaries[3] = {};

    auto context = new ExecutionContext();
    auto func = (FilterFunc)(intptr_t)codegen.GetFunction(inputTypes);

    int32_t result = func(vals, 1, selected, (int64_t *)(bitmap), (int64_t *)(offsets),
                          reinterpret_cast<int64_t>(context), dictionaries);

    EXPECT_EQ(result, 1);
    context->GetArena()->Reset();

    v1[0] = {8766};
    s1[0] = "hello";
    s2[0] = "world ";
    vals[0] = reinterpret_cast<int64_t>(v1);
    vals[1] = reinterpret_cast<int64_t>(s1->c_str());
    vals[2] = reinterpret_cast<int64_t>(s2->c_str());

    offsets[1][1] = s1[0].length();
    offsets[2][1] = s2[0].length();

    result = func(vals, 1, selected, (int64_t *)(bitmap), (int64_t *)(offsets), reinterpret_cast<int64_t>(context),
                  dictionaries);
    EXPECT_EQ(result, 0);
    context->GetArena()->Reset();

    v1[0] = {8766};
    s1[0] = "hello ";
    s2[0] = "world";
    vals[0] = reinterpret_cast<int64_t>(v1);
    vals[1] = reinterpret_cast<int64_t>(s1->c_str());
    vals[2] = reinterpret_cast<int64_t>(s2->c_str());

    offsets[1][1] = s1[0].length();
    offsets[2][1] = s2[0].length();

    result = func(vals, 1, selected, (int64_t *)(bitmap), (int64_t *)(offsets), reinterpret_cast<int64_t>(context),
                  dictionaries);
    EXPECT_EQ(result, 0);
    context->GetArena()->Reset();

    for (int i = 0; i < 3; i++) {
        delete[] bitmap[i];
        delete[] offsets[i];
    }
    delete[] offsets;
    delete[] bitmap;
    delete[] vals;
    delete[] selected;
    delete expr;
    delete context;
}

TEST(CodeGenTest, ConcatChars)
{
    // create expression objects
    auto *col1 = new FieldExpr(1, CharType(30));
    auto commaExpr = new LiteralExpr(new std::string(","), CharType(2));
    std::string funcStr = "concat";
    std::vector<Expr *> innerArgs;
    innerArgs.push_back(col1);
    innerArgs.push_back(commaExpr);
    auto innerConcat = GetFuncExpr(funcStr, innerArgs, CharType(32));

    auto col2 = new FieldExpr(2, CharType(20));
    std::vector<Expr *> outerArgs;
    outerArgs.push_back(innerConcat);
    outerArgs.push_back(col2);
    auto outerConcat = GetFuncExpr(funcStr, outerArgs, CharType(52));

    auto helloExpr = new LiteralExpr(new std::string("hello                         , world"), CharType(52));
    auto expr = new BinaryExpr(omniruntime::expressions::Operator::EQ, outerConcat, helloExpr, BooleanType());

    ExprPrinter printExprTree;
    expr->Accept(printExprTree);
    cout << endl;

    int32_t v1[1] = {8766};
    string s1[1] = {"hello"};
    string s2[1] = {"world"};
    auto *vals = new int64_t[3];
    vals[0] = reinterpret_cast<int64_t>(v1);
    vals[1] = reinterpret_cast<int64_t>(s1[0].c_str());
    vals[2] = reinterpret_cast<int64_t>(s2[0].c_str());
    auto *selected = new int32_t[1];
    auto **bitmap = new uint8_t *[3];
    for (int i = 0; i < 3; i++) {
        bitmap[i] = new uint8_t[NullsBuffer::CalculateNbytes(1)];
        BitUtil::SetBit(bitmap[i], 0, false);
    }

    auto **offsets = new int32_t *[3];
    offsets[0] = new int32_t[1];
    offsets[1] = new int32_t[2];
    offsets[1][0] = 0;
    offsets[1][1] = s1[0].length();
    offsets[2] = new int32_t[2];
    offsets[2][0] = 0;
    offsets[2][1] = s2[0].length();

    std::vector<DataTypePtr> vecOfTypes = {IntType(), CharType(), CharType()};
    DataTypes inputTypes(vecOfTypes);

    auto codegen = FilterCodeGen(defaultTestFunctionName, *expr, nullptr);
    int64_t dictionaries[3] = {};

    auto context = new ExecutionContext();
    auto func = (FilterFunc)(intptr_t)codegen.GetFunction(inputTypes);

    int32_t result = func(vals, 1, selected, (int64_t *)(bitmap), (int64_t *)(offsets),
                          reinterpret_cast<int64_t>(context), dictionaries);

    EXPECT_EQ(result, 1);
    context->GetArena()->Reset();

    v1[0] = {8766};
    s1[0] = "hello";
    s2[0] = "world";
    vals[0] = reinterpret_cast<int64_t>(v1);
    vals[1] = reinterpret_cast<int64_t>(s1[0].c_str());
    vals[2] = reinterpret_cast<int64_t>(s2[0].c_str());

    offsets[1][1] = s1[0].length();
    offsets[2][1] = s2[0].length();

    result = func(vals, 1, selected, (int64_t *)(bitmap), (int64_t *)(offsets), reinterpret_cast<int64_t>(context),
                  dictionaries);
    EXPECT_EQ(result, 1);
    context->GetArena()->Reset();

    v1[0] = {8766};
    s1[0] = "hello ";
    s2[0] = "world";
    vals[0] = reinterpret_cast<int64_t>(v1);
    vals[1] = reinterpret_cast<int64_t>(s1[0].c_str());
    vals[2] = reinterpret_cast<int64_t>(s2[0].c_str());

    offsets[1][1] = s1[0].length();
    offsets[2][1] = s2[0].length();

    result = func(vals, 1, selected, (int64_t *)(bitmap), (int64_t *)(offsets), reinterpret_cast<int64_t>(context),
                  dictionaries);
    EXPECT_EQ(result, 1);
    context->GetArena()->Reset();

    for (int i = 0; i < 3; i++) {
        delete[] bitmap[i];
        delete[] offsets[i];
    }
    delete[] offsets;
    delete[] bitmap;
    delete[] vals;
    delete[] selected;
    delete expr;
    delete context;
}

TEST(CodeGenTest, ToUpper)
{
    std::string funcStr = "upper";
    auto *col1 = new FieldExpr(1, VarcharType());
    std::vector<Expr *> toUpperArgs;
    toUpperArgs.push_back(col1);
    DataTypePtr retType = VarcharType();
    auto toUpperExpr = GetFuncExpr(funcStr, toUpperArgs, VarcharType());

    auto upperTestExpr = new LiteralExpr(
        new std::string("[\\]^_ABCDEFGHIJKLMNOPQRSTUVWXYZ{|} THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG."),
        VarcharType(80));
    auto expr = new BinaryExpr(omniruntime::expressions::Operator::EQ, toUpperExpr, upperTestExpr, BooleanType());

    ExprPrinter printExprTree;
    expr->Accept(printExprTree);
    cout << endl;

    string s1[] = {"[\\]^_abcdefghijklmnopqrstuvwxyz{|} The quick brown fox jumps over the lazy dog."};
    int32_t v1[] = {8766};
    int64_t *vals = new int64_t[2];
    vals[0] = reinterpret_cast<int64_t>(v1);
    vals[1] = reinterpret_cast<int64_t>(s1->c_str());
    int32_t *selected = new int32_t[1];

    auto **bitmap = new uint8_t *[2];
    for (int i = 0; i < 2; i++) {
        bitmap[i] = new uint8_t[NullsBuffer::CalculateNbytes(1)];
        BitUtil::SetBit(bitmap[i], 0, false);
    }

    auto **offsets = new int32_t *[2];
    offsets[0] = new int32_t[1];
    offsets[1] = new int32_t[2];
    offsets[0][0] = 0;
    offsets[1][0] = 0;
    offsets[1][1] = s1[0].length();

    std::vector<DataTypePtr> vecOfTypes = {IntType(), VarcharType()};
    DataTypes inputTypes(vecOfTypes);

    auto codegen = FilterCodeGen(defaultTestFunctionName, *expr, nullptr);
    int64_t dictionaries[2] = {};

    auto context = new ExecutionContext();

    auto func = (FilterFunc)(intptr_t)codegen.GetFunction(inputTypes);

    int32_t result = func(vals, 1, selected, (int64_t *)(bitmap), (int64_t *)(offsets),
                          reinterpret_cast<int64_t>(context), dictionaries);

    EXPECT_EQ(result, 1);

    context->GetArena()->Reset();

    for (int i = 0; i < 2; i++) {
        delete[] bitmap[i];
        delete[] offsets[i];
    }
    delete[] offsets;
    delete[] bitmap;
    delete[] vals;
    delete[] selected;
    delete expr;
    delete context;
}

TEST(CodeGenTest, ToUpperChar)
{
    std::string funcStr = "upper";
    auto *col1 = new FieldExpr(1, CharType(80));
    std::vector<Expr *> toUpperCharArgs;
    toUpperCharArgs.push_back(col1);
    auto toUpperCharExpr = GetFuncExpr(funcStr, toUpperCharArgs, CharType(80));

    auto upperCharTestExpr = new LiteralExpr(
        new std::string("[\\]^_ABCDEFGHIJKLMNOPQRSTUVWXYZ{|} THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG."),
        CharType(80));
    auto expr =
        new BinaryExpr(omniruntime::expressions::Operator::EQ, toUpperCharExpr, upperCharTestExpr, BooleanType());

    ExprPrinter printExprTree;
    expr->Accept(printExprTree);
    cout << endl;

    string s1[] = {"[\\]^_abcdefghijklmnopqrstuvwxyz{|} The quick brown fox jumps over the lazy dog."};
    int32_t v1[] = {8766};
    int64_t *vals = new int64_t[2];
    vals[0] = reinterpret_cast<int64_t>(v1);
    vals[1] = reinterpret_cast<int64_t>(s1->c_str());
    int32_t *selected = new int32_t[1];

    auto **bitmap = new uint8_t *[2];
    for (int i = 0; i < 2; i++) {
        bitmap[i] = new uint8_t[NullsBuffer::CalculateNbytes(1)];
        BitUtil::SetBit(bitmap[i], 0, false);
    }

    auto **offsets = new int32_t *[2];
    offsets[0] = new int32_t[1];
    offsets[1] = new int32_t[2];
    offsets[0][0] = 0;
    offsets[1][0] = 0;
    offsets[1][1] = s1[0].length();

    std::vector<DataTypePtr> vecOfTypes = {IntType(), CharType()};
    DataTypes inputTypes(vecOfTypes);

    auto codegen = FilterCodeGen(defaultTestFunctionName, *expr, nullptr);
    int64_t dictionaries[2] = {};

    auto context = new ExecutionContext();

    auto func = (FilterFunc)(intptr_t)codegen.GetFunction(inputTypes);

    int32_t result = func(vals, 1, selected, (int64_t *)(bitmap), (int64_t *)(offsets),
                          reinterpret_cast<int64_t>(context), dictionaries);

    EXPECT_EQ(result, 1);

    context->GetArena()->Reset();

    for (int i = 0; i < 2; i++) {
        delete[] bitmap[i];
        delete[] offsets[i];
    }
    delete[] offsets;
    delete[] bitmap;
    delete[] vals;
    delete[] selected;
    delete expr;
    delete context;
}

TEST(CodeGenTest, StringWithOps)
{
    // create expression objects
    FieldExpr *col21 = new FieldExpr(2, VarcharType());
    LiteralExpr *sunday = new LiteralExpr(new std::string("Sunday"), VarcharType(7));
    BinaryExpr *eqExpr1 = new BinaryExpr(omniruntime::expressions::Operator::EQ, col21, sunday, BooleanType());

    FieldExpr *col22 = new FieldExpr(2, VarcharType());
    LiteralExpr *saturday = new LiteralExpr(new std::string("Saturday"), VarcharType(9));
    BinaryExpr *eqExpr2 = new BinaryExpr(omniruntime::expressions::Operator::EQ, col22, saturday, BooleanType());

    BinaryExpr *expr = new BinaryExpr(omniruntime::expressions::Operator::OR, eqExpr1, eqExpr2, BooleanType());

    ExprPrinter printExprTree;
    expr->Accept(printExprTree);
    cout << endl;

    string s1[1];
    string s2[1];

    int32_t v1[1] = {8766};
    s1[0] = "asdf";
    s2[0] = "Saturday";

    int64_t *vals = new int64_t[3];
    vals[0] = reinterpret_cast<int64_t>(v1);
    vals[1] = reinterpret_cast<int64_t>(s1->c_str());
    vals[2] = reinterpret_cast<int64_t>(s2->c_str());
    int32_t *selected = new int32_t[1];

    auto **bitmap = new uint8_t *[3];
    for (int i = 0; i < 3; i++) {
        bitmap[i] = new uint8_t[NullsBuffer::CalculateNbytes(1)];
        BitUtil::SetBit(bitmap[i], 0, false);
    }

    auto **offsets = new int32_t *[3];
    offsets[0] = new int32_t[1];
    offsets[1] = new int32_t[2];
    offsets[1][0] = 0;
    offsets[1][1] = s1[0].length();
    offsets[2] = new int32_t[2];
    offsets[2][0] = 0;
    offsets[2][1] = s2[0].length();

    std::vector<DataTypePtr> vecOfTypes = {IntType(), VarcharType(), VarcharType()};
    DataTypes inputTypes(vecOfTypes);

    auto codegen = FilterCodeGen("stringTest1", *expr, nullptr);
    int64_t dictionaries[3] = {};
    auto context = new ExecutionContext();
    auto func = (FilterFunc)(intptr_t)codegen.GetFunction(inputTypes);

    int32_t result = func(vals, 1, selected, (int64_t *)(bitmap), (int64_t *)(offsets),
                          reinterpret_cast<int64_t>(context), dictionaries);
    EXPECT_EQ(result, 1);
    context->GetArena()->Reset();

    v1[0] = {8766};
    s1[0] = "j";
    s2[0] = "Sunday";
    vals[0] = reinterpret_cast<int64_t>(v1);
    vals[1] = reinterpret_cast<int64_t>(s1->c_str());
    vals[2] = reinterpret_cast<int64_t>(s2->c_str());

    offsets[1][1] = s1[0].length();
    offsets[2][1] = s2[0].length();
    result = func(vals, 1, selected, (int64_t *)(bitmap), (int64_t *)(offsets), reinterpret_cast<int64_t>(context),
                  dictionaries);
    EXPECT_EQ(result, 1);
    context->GetArena()->Reset();

    v1[0] = {8766};
    s1[0] = "j";
    s2[0] = "Monday";
    vals[0] = reinterpret_cast<int64_t>(v1);
    vals[1] = reinterpret_cast<int64_t>(s1->c_str());
    vals[2] = reinterpret_cast<int64_t>(s2->c_str());
    offsets[1][1] = s1[0].length();
    offsets[2][1] = s2[0].length();
    result = func(vals, 1, selected, (int64_t *)(bitmap), (int64_t *)(offsets), reinterpret_cast<int64_t>(context),
                  dictionaries);
    EXPECT_EQ(result, 0);
    context->GetArena()->Reset();

    for (int i = 0; i < 3; i++) {
        delete[] bitmap[i];
        delete[] offsets[i];
    }
    delete[] offsets;
    delete[] bitmap;
    delete[] vals;
    delete[] selected;
    delete expr;
    delete context;
}

TEST(CodeGenTest, Coalesce)
{
    // create expression objects
    FieldExpr *value1 = new FieldExpr(0, LongType());
    LiteralExpr *value2 = new LiteralExpr(0, LongType());
    value2->dataType = LongType();
    CoalesceExpr *coalesceExpr = new CoalesceExpr(value1, value2);

    LiteralExpr *right = new LiteralExpr(123L, LongType());
    right->dataType = LongType();
    BinaryExpr *expr = new BinaryExpr(omniruntime::expressions::Operator::EQ, coalesceExpr, right, BooleanType());

    ExprPrinter printExprTree;
    expr->Accept(printExprTree);
    cout << endl;

    int64_t v1[1] = {123};
    int64_t v2[1] = {234};
    int64_t v3[1] = {345};
    int64_t *vals = new int64_t[3];
    vals[0] = reinterpret_cast<int64_t>(v1);
    vals[1] = reinterpret_cast<int64_t>(v2);
    vals[2] = reinterpret_cast<int64_t>(v3);
    int32_t *selected = new int32_t[1];

    auto **bitmap = new uint8_t *[3];
    for (int i = 0; i < 3; i++) {
        bitmap[i] = new uint8_t[NullsBuffer::CalculateNbytes(1)];
        BitUtil::SetBit(bitmap[i], 0, false);
    }

    auto **offsets = new int32_t *[3];
    for (int col = 0; col < 3; col++) {
        offsets[col] = new int32_t[1];
    }

    std::vector<DataTypePtr> vecOfTypes = {LongType(), LongType(), LongType()};
    DataTypes inputTypes(vecOfTypes);

    auto codegen = FilterCodeGen(defaultTestFunctionName, *expr, nullptr);
    int64_t dictionaries[3] = {};

    auto context = new ExecutionContext();
    auto func = (FilterFunc)(intptr_t)codegen.GetFunction(inputTypes);

    int32_t result = func(vals, 1, selected, (int64_t *)(bitmap), (int64_t *)(offsets),
                          reinterpret_cast<int64_t>(context), dictionaries);
    EXPECT_EQ(result, 1);
    context->GetArena()->Reset();

    BitUtil::SetBit(bitmap[0], 0, true);

    result = func(vals, 1, selected, (int64_t *)(bitmap), (int64_t *)(offsets), reinterpret_cast<int64_t>(context),
                  dictionaries);
    EXPECT_EQ(result, 0);
    context->GetArena()->Reset();

    for (int i = 0; i < 3; i++) {
        delete[] bitmap[i];
        delete[] offsets[i];
    }
    delete[] offsets;
    delete[] bitmap;
    delete[] vals;
    delete[] selected;
    delete expr;
    delete context;
}

TEST(CodeGenTest, ProjectionCoalesce)
{
    // create expression objects
    FieldExpr *value1 = new FieldExpr(0, LongType());
    LiteralExpr *value2 = new LiteralExpr(100L, LongType());
    value2->dataType = LongType();
    CoalesceExpr *coalesceExpr = new CoalesceExpr(value1, value2);

    LiteralExpr *right = new LiteralExpr(100L, LongType());
    right->dataType = LongType();
    BinaryExpr *expr = new BinaryExpr(omniruntime::expressions::Operator::EQ, coalesceExpr, right, BooleanType());

    ExprPrinter printExprTree;
    expr->Accept(printExprTree);

    int64_t c[3] = {100, 200, 300};
    int64_t *vals = new int64_t[1];
    vals[0] = reinterpret_cast<int64_t>(c);
    auto **bitmap = new uint8_t *[1];
    bitmap[0] = new uint8_t[NullsBuffer::CalculateNbytes(3)];
    for (int i = 0; i < 3; i++) {
        BitUtil::SetBit(bitmap[0], i, false);
    }
    auto **offsets = new int32_t *[1];
    for (int col = 0; col < 1; col++) {
        offsets[col] = new int32_t[3];
    }

    std::vector<DataTypePtr> vecOfTypes = {LongType()};
    DataTypes inputTypes(vecOfTypes);

    uint8_t newNullValues[NullsBuffer::CalculateNbytes(3)] = {0};
    int newLengths[3];

    auto codegen = ProjectionCodeGen(defaultTestFunctionName, *expr, false, nullptr);
    int64_t dictionaryVectors[1] = {};

    bool oVec[3];
    auto context = new ExecutionContext();
    auto func = (ProjectFunc)(intptr_t)codegen.GetFunction(inputTypes);

    int32_t r = func(vals, 3, (int64_t)oVec, nullptr, 3, (int64_t *)(bitmap), (int64_t *)(offsets),
                     reinterpret_cast<int32_t *>(newNullValues), newLengths, reinterpret_cast<int64_t>(context),
                     dictionaryVectors);
    EXPECT_NE(r, 0);
    EXPECT_FALSE(BitUtil::IsBitSet(newNullValues, 0));
    EXPECT_FALSE(BitUtil::IsBitSet(newNullValues, 1));
    EXPECT_FALSE(BitUtil::IsBitSet(newNullValues, 2));
    EXPECT_EQ(oVec[0], true);
    EXPECT_EQ(oVec[1], false);
    EXPECT_EQ(oVec[2], false);
    context->GetArena()->Reset();

    for (int i = 0; i < 3; i++) {
        BitUtil::SetBit(bitmap[0], 0, true);
    }
    for (int i = 0; i < NullsBuffer::CalculateNbytes(3); i++) {
        newNullValues[i] = 0;
    }

    r = func(vals, 3, (int64_t)oVec, nullptr, 3, (int64_t *)(bitmap), (int64_t *)(offsets),
             reinterpret_cast<int32_t *>(newNullValues), newLengths, reinterpret_cast<int64_t>(context),
             dictionaryVectors);
    EXPECT_NE(r, 0);
    EXPECT_FALSE(BitUtil::IsBitSet(newNullValues, 0));
    EXPECT_FALSE(BitUtil::IsBitSet(newNullValues, 1));
    EXPECT_FALSE(BitUtil::IsBitSet(newNullValues, 2));
    EXPECT_EQ(oVec[0], true);
    EXPECT_EQ(oVec[1], false);
    EXPECT_EQ(oVec[2], false);
    context->GetArena()->Reset();

    for (int i = 0; i < 1; i++) {
        delete[] bitmap[i];
        delete[] offsets[i];
    }
    delete[] bitmap;
    delete[] offsets;
    delete[] vals;
    delete expr;
    delete context;
}

TEST(CodeGenTest, ProjectionIsNull)
{
    FieldExpr *col0 = new FieldExpr(0, LongType());
    IsNullExpr *expr = new IsNullExpr(col0);

    ExprPrinter printExprTree;
    expr->Accept(printExprTree);

    int64_t c[3] = {100, 200, 300};
    int64_t *vals = new int64_t[1];
    vals[0] = reinterpret_cast<int64_t>(c);
    auto **bitmap = new uint8_t *[1];
    bitmap[0] = new uint8_t[NullsBuffer::CalculateNbytes(3)];
    for (int i = 0; i < 3; i++) {
        BitUtil::SetBit(bitmap[0], i, false);
    }
    auto **offsets = new int32_t *[1];
    for (int col = 0; col < 1; col++) {
        offsets[col] = new int32_t[3];
    }

    std::vector<DataTypePtr> vecOfTypes = {LongType()};
    DataTypes inputTypes(vecOfTypes);

    uint8_t newNullValues[NullsBuffer::CalculateNbytes(3)] = {0};
    int newLengths[3];

    auto codegen = ProjectionCodeGen(defaultTestFunctionName, *expr, false, nullptr);
    int64_t dictionaryVectors[1] = {};

    bool oVec[3];
    auto context = new ExecutionContext();
    auto func = (ProjectFunc)(intptr_t)codegen.GetFunction(inputTypes);

    int32_t r = func(vals, 3, (int64_t)oVec, nullptr, 3, (int64_t *)(bitmap), (int64_t *)(offsets),
                     reinterpret_cast<int32_t *>(newNullValues), newLengths, reinterpret_cast<int64_t>(context),
                     dictionaryVectors);
    EXPECT_NE(r, 0);
    EXPECT_FALSE(BitUtil::IsBitSet(newNullValues, 0));
    EXPECT_FALSE(BitUtil::IsBitSet(newNullValues, 1));
    EXPECT_FALSE(BitUtil::IsBitSet(newNullValues, 2));
    EXPECT_EQ(oVec[0], false);
    EXPECT_EQ(oVec[1], false);
    EXPECT_EQ(oVec[2], false);
    context->GetArena()->Reset();

    for (int i = 0; i < 3; i++) {
        BitUtil::SetBit(bitmap[0], i, true);
    }
    for (int i = 0; i < NullsBuffer::CalculateNbytes(3); i++) {
        newNullValues[i] = 0;
    }

    r = func(vals, 3, (int64_t)oVec, nullptr, 3, (int64_t *)(bitmap), (int64_t *)(offsets),
             reinterpret_cast<int32_t *>(newNullValues), newLengths, reinterpret_cast<int64_t>(context),
             dictionaryVectors);
    EXPECT_NE(r, 0);
    EXPECT_FALSE(BitUtil::IsBitSet(newNullValues, 0));
    EXPECT_FALSE(BitUtil::IsBitSet(newNullValues, 1));
    EXPECT_FALSE(BitUtil::IsBitSet(newNullValues, 2));
    EXPECT_EQ(oVec[0], true);
    EXPECT_EQ(oVec[1], true);
    EXPECT_EQ(oVec[2], true);
    context->GetArena()->Reset();

    for (int i = 0; i < 1; i++) {
        delete[] bitmap[i];
        delete[] offsets[i];
    }
    delete[] bitmap;
    delete[] offsets;
    delete[] vals;
    delete expr;
    delete context;
}

TEST(CodeGenTest, IsNull)
{
    auto *col0 = new FieldExpr(0, LongType());
    auto *expr = new IsNullExpr(col0);

    ExprPrinter printExprTree;
    expr->Accept(printExprTree);

    int64_t v1[1] = {123};
    auto *vals = new int64_t[1];
    vals[0] = reinterpret_cast<int64_t>(v1);
    auto *selected = new int32_t[1];

    auto **bitmap = new uint8_t *[1];
    bitmap[0] = new uint8_t[NullsBuffer::CalculateNbytes(1)];
    BitUtil::SetBit(bitmap[0], 0, false);

    auto **offsets = new int32_t *[1];
    for (int col = 0; col < 1; col++) {
        offsets[col] = new int32_t[1];
    }

    std::vector<DataTypePtr> vecOfTypes = {LongType()};
    DataTypes inputTypes(vecOfTypes);

    auto codegen = FilterCodeGen(defaultTestFunctionName, *expr, nullptr);
    int64_t dictionaries[1] = {};

    auto context = new ExecutionContext();
    auto func = (FilterFunc)(intptr_t)codegen.GetFunction(inputTypes);

    int32_t result = func(vals, 1, selected, (int64_t *)(bitmap), (int64_t *)(offsets),
                          reinterpret_cast<int64_t>(context), dictionaries);
    EXPECT_EQ(result, false);
    context->GetArena()->Reset();

    BitUtil::SetBit(bitmap[0], 0, true);

    result = func(vals, 1, selected, (int64_t *)(bitmap), (int64_t *)(offsets), reinterpret_cast<int64_t>(context),
                  dictionaries);
    EXPECT_EQ(result, true);
    context->GetArena()->Reset();

    delete[] bitmap[0];
    delete[] bitmap;
    delete[] offsets[0];
    delete[] offsets;
    delete[] vals;
    delete[] selected;
    delete expr;
    delete context;
}

TEST(CodeGenTest, IsNotNull)
{
    auto *col0 = new FieldExpr(0, LongType());
    auto *isNull = new IsNullExpr(col0);
    auto *expr = new UnaryExpr(omniruntime::expressions::Operator::NOT, isNull, BooleanType());

    ExprPrinter printExprTree;
    expr->Accept(printExprTree);

    int64_t v1[1] = {123};
    auto *vals = new int64_t[1];
    vals[0] = reinterpret_cast<int64_t>(v1);
    auto *selected = new int32_t[1];

    auto **bitmap = new uint8_t *[1];
    bitmap[0] = new uint8_t[NullsBuffer::CalculateNbytes(1)];
    BitUtil::SetBit(bitmap[0], 0, false);

    auto **offsets = new int32_t *[1];
    for (int col = 0; col < 1; col++) {
        offsets[col] = new int32_t[1];
    }

    std::vector<DataTypePtr> vecOfTypes = {LongType()};
    DataTypes inputTypes(vecOfTypes);

    auto codegen = FilterCodeGen(defaultTestFunctionName, *expr, nullptr);
    int64_t dictionaries[1] = {};

    auto context = new ExecutionContext();
    auto func = (FilterFunc)(intptr_t)codegen.GetFunction(inputTypes);

    int32_t result = func(vals, 1, selected, (int64_t *)(bitmap), (int64_t *)(offsets),
                          reinterpret_cast<int64_t>(context), dictionaries);
    EXPECT_EQ(result, true);
    context->GetArena()->Reset();

    BitUtil::SetBit(bitmap[0], 0, true);

    result = func(vals, 1, selected, (int64_t *)(bitmap), (int64_t *)(offsets), reinterpret_cast<int64_t>(context),
                  dictionaries);
    EXPECT_EQ(result, false);
    context->GetArena()->Reset();

    delete[] bitmap[0];
    delete[] bitmap;
    delete[] offsets[0];
    delete[] offsets;
    delete[] vals;
    delete[] selected;
    delete expr;
    delete context;
}

TEST(CodeGenTest, DecimalOperators1)
{
    // create expression objects
    FieldExpr *addLeft = new FieldExpr(0, Decimal128Type(38, 0));
    LiteralExpr *addRight = new LiteralExpr(new string("5"), Decimal128Type(38, 0));
    BinaryExpr *addExpr =
        new BinaryExpr(omniruntime::expressions::Operator::ADD, addLeft, addRight, Decimal128Type(38, 0));

    LiteralExpr *equalRight = new LiteralExpr(new string("15"), Decimal128Type(38, 0));
    BinaryExpr *expr = new BinaryExpr(omniruntime::expressions::Operator::EQ, addExpr, equalRight, BooleanType());

    ExprPrinter printExprTree;
    expr->Accept(printExprTree);

    // creating decimal
    int64_t c[4] = {10, 0, 9, 0};
    int64_t *vals = new int64_t[1];
    vals[0] = reinterpret_cast<int64_t>(c);

    int32_t *selected = new int32_t[2];

    auto **bitmap = new uint8_t *[1];
    bitmap[0] = new uint8_t[NullsBuffer::CalculateNbytes(2)];
    for (int i = 0; i < 2; i++) {
        BitUtil::SetBit(bitmap[0], i, false);
    }

    auto **offsets = new int32_t *[1];
    for (int col = 0; col < 1; col++) {
        offsets[col] = new int32_t[2];
    }

    std::vector<DataTypePtr> vecOfTypes = {Decimal128Type()};
    DataTypes inputTypes(vecOfTypes);

    auto codegen = FilterCodeGen(defaultTestFunctionName, *expr, nullptr);
    int64_t dictionaries[1] = {};

    auto context = new ExecutionContext();
    auto func = (FilterFunc)(intptr_t)codegen.GetFunction(inputTypes);

    int32_t result = func(vals, 1, selected, (int64_t *)(bitmap), (int64_t *)(offsets),
                          reinterpret_cast<int64_t>(context), dictionaries);
    EXPECT_EQ(result, 1);
    context->GetArena()->Reset();

    for (int i = 0; i < 1; i++) {
        delete[] bitmap[i];
        delete[] offsets[i];
    }
    delete[] bitmap;
    delete[] offsets;
    delete[] vals;
    delete[] selected;
    delete expr;
    delete context;
}

TEST(CodeGenTest, DecimalOperators2)
{
    string unparsed = "BETWEEN:4(#0, #1, #2)";
    FieldExpr *col0 = new FieldExpr(0, Decimal128Type(38, 0));
    FieldExpr *col1 = new FieldExpr(1, Decimal128Type(38, 0));
    FieldExpr *col2 = new FieldExpr(2, Decimal128Type(38, 0));

    BetweenExpr *expr = new BetweenExpr(col0, col1, col2);

    ExprPrinter printExprTree;
    expr->Accept(printExprTree);

    // creating decimal
    int64_t c1[2] = {4000, 0};

    int64_t d1[2] = {5000, 0};

    int64_t e1[2] = {15000, 0};

    int64_t *vals = new int64_t[3];
    vals[0] = reinterpret_cast<int64_t>(c1);
    vals[1] = reinterpret_cast<int64_t>(d1);
    vals[2] = reinterpret_cast<int64_t>(e1);

    int32_t *selected = new int32_t[1];

    auto **bitmap = new uint8_t *[3];
    for (int i = 0; i < 3; i++) {
        bitmap[i] = new uint8_t[NullsBuffer::CalculateNbytes(1)];
        BitUtil::SetBit(bitmap[i], 0, false);
    }

    auto **offsets = new int32_t *[3];
    for (int col = 0; col < 3; col++) {
        offsets[col] = new int32_t[1];
    }

    std::vector<DataTypePtr> vecOfTypes = {Decimal128Type(), Decimal128Type(), Decimal128Type()};
    DataTypes inputTypes(vecOfTypes);

    auto codegen = FilterCodeGen(defaultTestFunctionName, *expr, nullptr);
    int64_t dictionaries[3] = {};
    auto context = new ExecutionContext();
    auto func = (FilterFunc)(intptr_t)codegen.GetFunction(inputTypes);

    int32_t result = func(vals, 1, selected, (int64_t *)(bitmap), (int64_t *)(offsets),
                          reinterpret_cast<int64_t>(context), dictionaries);
    EXPECT_EQ(result, 0);

    for (int i = 0; i < 3; i++) {
        delete[] bitmap[i];
        delete[] offsets[i];
    }
    delete[] bitmap;
    delete[] offsets;
    delete[] vals;
    delete[] selected;
    delete expr;
    delete context;
}

TEST(CodeGenTest, DecimalOperators3)
{
    // create expression objects
    FieldExpr *col01 = new FieldExpr(0, Decimal128Type(38, 0));
    LiteralExpr *data01 = new LiteralExpr(new string("100"), Decimal128Type(38, 0));
    BinaryExpr *condition = new BinaryExpr(omniruntime::expressions::Operator::GT, col01, data01, BooleanType());

    FieldExpr *col02 = new FieldExpr(0, Decimal128Type(38, 0));
    LiteralExpr *data02 = new LiteralExpr(new string("200"), Decimal128Type(38, 0));
    BinaryExpr *texp = new BinaryExpr(omniruntime::expressions::Operator::GT, col02, data02, BooleanType());

    FieldExpr *col03 = new FieldExpr(0, Decimal128Type(38, 0));
    LiteralExpr *data03 = new LiteralExpr(new string("0"), Decimal128Type(38, 0));
    BinaryExpr *fexp = new BinaryExpr(omniruntime::expressions::Operator::LT, col03, data03, BooleanType());

    IfExpr *expr = new IfExpr(condition, texp, fexp);

    ExprPrinter printExprTree;
    expr->Accept(printExprTree);

    // creating decimal
    int64_t c1[2] = {-12222, -1};

    int64_t d1[2] = {-12312, -1};

    int64_t e1[2] = {42, 0};

    int64_t *vals = new int64_t[3];
    vals[0] = reinterpret_cast<int64_t>(c1);
    vals[1] = reinterpret_cast<int64_t>(d1);
    vals[2] = reinterpret_cast<int64_t>(e1);

    int32_t *selected = new int32_t[1];

    auto **bitmap = new uint8_t *[3];
    for (int i = 0; i < 3; i++) {
        bitmap[i] = new uint8_t[NullsBuffer::CalculateNbytes(1)];
        BitUtil::SetBit(bitmap[i], 0, false);
    }

    auto **offsets = new int32_t *[3];
    for (int col = 0; col < 3; col++) {
        offsets[col] = new int32_t[1];
    }

    std::vector<DataTypePtr> vecOfTypes = {Decimal128Type(), Decimal128Type(), Decimal128Type()};
    DataTypes inputTypes(vecOfTypes);

    auto codegen = FilterCodeGen(defaultTestFunctionName, *expr, nullptr);
    int64_t dictionaries[3] = {};

    auto context = new ExecutionContext();
    auto func = (FilterFunc)(intptr_t)codegen.GetFunction(inputTypes);

    int32_t result = func(vals, 1, selected, (int64_t *)(bitmap), (int64_t *)(offsets),
                          reinterpret_cast<int64_t>(context), dictionaries);
    EXPECT_EQ(result, 1);
    context->GetArena()->Reset();

    for (int i = 0; i < 3; i++) {
        delete[] bitmap[i];
        delete[] offsets[i];
    }
    delete[] bitmap;
    delete[] offsets;
    delete[] vals;
    delete[] selected;
    delete expr;
    delete context;
}

TEST(CodeGenTest, DecimalNegate)
{
    // currently fails
    FieldExpr *col0 = new FieldExpr(0, Decimal128Type(38, 0));
    FieldExpr *col1 = new FieldExpr(1, Decimal128Type(38, 0));
    LiteralExpr *mulRight0 = new LiteralExpr(new string("-1"), Decimal128Type(38, 0));

    LiteralExpr *mulRight1 = new LiteralExpr(new string("-1"), Decimal128Type(38, 0));

    BinaryExpr *mulExpr0 =
        new BinaryExpr(omniruntime::expressions::Operator::MUL, col0, mulRight0, Decimal128Type(38, 0));
    BinaryExpr *mulExpr1 =
        new BinaryExpr(omniruntime::expressions::Operator::MUL, col1, mulRight1, Decimal128Type(38, 0));

    FieldExpr *col2 = new FieldExpr(2, Decimal128Type(38, 0));
    FieldExpr *col3 = new FieldExpr(3, Decimal128Type(38, 0));

    BinaryExpr *gteExpr = new BinaryExpr(omniruntime::expressions::Operator::EQ, mulExpr0, col2, BooleanType());
    BinaryExpr *lteExpr = new BinaryExpr(omniruntime::expressions::Operator::EQ, mulExpr1, col3, BooleanType());

    BinaryExpr *expr = new BinaryExpr(omniruntime::expressions::Operator::AND, lteExpr, gteExpr, BooleanType());

    ExprPrinter printExprTree;
    expr->Accept(printExprTree);

    // creating decimal
    int64_t c1[2] = {3, 0};
    int64_t d1[2] = {4, 0};
    int64_t e1[2] = {-3, ~0};
    int64_t f1[2] = {-4, ~0};

    int64_t *vals = new int64_t[4];
    vals[0] = reinterpret_cast<int64_t>(c1);
    vals[1] = reinterpret_cast<int64_t>(d1);
    vals[2] = reinterpret_cast<int64_t>(e1);
    vals[3] = reinterpret_cast<int64_t>(f1);

    int32_t *selected = new int32_t[1];

    auto **bitmap = new uint8_t *[4];
    for (int i = 0; i < 4; i++) {
        bitmap[i] = new uint8_t[NullsBuffer::CalculateNbytes(1)];
        BitUtil::SetBit(bitmap[i], 0, false);
    }

    auto **offsets = new int32_t *[4];
    for (int col = 0; col < 4; col++) {
        offsets[col] = new int32_t[1];
    }

    std::vector<DataTypePtr> vecOfTypes = {Decimal128Type(), Decimal128Type(), Decimal128Type(), Decimal128Type()};
    DataTypes inputTypes(vecOfTypes);

    auto codegen = FilterCodeGen(defaultTestFunctionName, *expr, nullptr);
    int64_t dictionaries[4] = {};
    auto context = new ExecutionContext();
    auto func = (FilterFunc)(intptr_t)codegen.GetFunction(inputTypes);

    bool result = func(vals, 1, selected, (int64_t *)(bitmap), (int64_t *)(offsets), reinterpret_cast<int64_t>(context),
                       dictionaries);
    EXPECT_TRUE(result);

    for (int i = 0; i < 4; i++) {
        delete[] bitmap[i];
        delete[] offsets[i];
    }
    delete[] bitmap;
    delete[] offsets;
    delete[] vals;
    delete[] selected;
    delete expr;
    delete context;
}

TEST(CodeGenTest, Decimal128AbsAndCompare)
{
    // currently fails
    FieldExpr *col0 = new FieldExpr(0, Decimal128Type(38, 0));
    FieldExpr *col1 = new FieldExpr(1, Decimal128Type(38, 0));
    std::string absFuncStr = "abs";
    std::vector<Expr *> absArgs;
    absArgs.push_back(col0);
    auto absExpr = GetFuncExpr(absFuncStr, absArgs, Decimal128Type(38, 0));

    std::string compFuncStr = "Decimal128Compare";
    std::vector<Expr *> compArgs;
    compArgs.push_back(absExpr);
    compArgs.push_back(col1);
    auto compExpr = GetFuncExpr(compFuncStr, compArgs, IntType());

    LiteralExpr *eqRight = new LiteralExpr(0, IntType());

    BinaryExpr *expr = new BinaryExpr(omniruntime::expressions::Operator::EQ, compExpr, eqRight, BooleanType());

    ExprPrinter printExprTree;
    expr->Accept(printExprTree);

    // creating decimal
    int64_t c1[2] = {-3, ~0};
    int64_t d1[2] = {3, 0};

    int64_t *vals = new int64_t[2];
    vals[0] = reinterpret_cast<int64_t>(c1);
    vals[1] = reinterpret_cast<int64_t>(d1);

    int32_t *selected = new int32_t[1];

    auto **bitmap = new uint8_t *[2];
    for (int i = 0; i < 2; i++) {
        bitmap[i] = new uint8_t[NullsBuffer::CalculateNbytes(1)];
        BitUtil::SetBit(bitmap[i], 0, false);
    }

    auto **offsets = new int32_t *[2];
    for (int col = 0; col < 2; col++) {
        offsets[col] = new int32_t[1];
    }

    std::vector<DataTypePtr> vecOfTypes = {Decimal128Type(), Decimal128Type()};
    DataTypes inputTypes(vecOfTypes);

    auto codegen = FilterCodeGen(defaultTestFunctionName, *expr, nullptr);
    int64_t dictionaries[2] = {};
    auto context = new ExecutionContext();
    auto func = (FilterFunc)(intptr_t)codegen.GetFunction(inputTypes);

    bool result = func(vals, 1, selected, (int64_t *)(bitmap), (int64_t *)(offsets), reinterpret_cast<int64_t>(context),
                       dictionaries);
    EXPECT_TRUE(result);

    for (int i = 0; i < 2; i++) {
        delete[] bitmap[i];
        delete[] offsets[i];
    }
    delete[] bitmap;
    delete[] offsets;
    delete[] vals;
    delete[] selected;
    delete expr;
    delete context;
}

TEST(CodeGenTest, ProjectionSubtractNulls)
{
    // create expression objects
    FieldExpr *subLeft = new FieldExpr(0, LongType());
    LiteralExpr *subRight = new LiteralExpr(100L, LongType());

    BinaryExpr *expr = new BinaryExpr(omniruntime::expressions::Operator::SUB, subLeft, subRight, LongType());

    ExprPrinter printExprTree;
    expr->Accept(printExprTree);

    int64_t c[3] = {10, 20, 30};

    int64_t *vals = new int64_t[1];
    vals[0] = reinterpret_cast<int64_t>(c);

    auto **bitmap = new uint8_t *[1];
    bitmap[0] = new uint8_t[NullsBuffer::CalculateNbytes(3)];
    for (int i = 0; i < 3; i++) {
        BitUtil::SetBit(bitmap[0], i, false);
    }
    auto **offsets = new int32_t *[1];
    for (int col = 0; col < 1; col++) {
        offsets[col] = new int32_t[3];
    }

    std::vector<DataTypePtr> vecOfTypes = {LongType()};
    DataTypes inputTypes(vecOfTypes);

    uint8_t newNullValues[NullsBuffer::CalculateNbytes(3)] = {0};
    int newLengths[3];

    auto codegen = ProjectionCodeGen(defaultTestFunctionName, *expr, false, nullptr);
    int64_t dictionaryVectors[1] = {};

    vector<int64_t> oVec(6);
    auto ov = oVec.data();
    void *vecVals = &ov;
    auto cvecVals = static_cast<int64_t *>(vecVals);
    auto context = new ExecutionContext();
    auto func = (ProjectFunc)(intptr_t)codegen.GetFunction(inputTypes);

    int32_t r = func(vals, 3, *cvecVals, nullptr, 3, (int64_t *)(bitmap), (int64_t *)(offsets),
                     reinterpret_cast<int32_t *>(newNullValues), newLengths, reinterpret_cast<int64_t>(context),
                     dictionaryVectors);
    EXPECT_NE(r, 0);
    EXPECT_FALSE(BitUtil::IsBitSet(newNullValues, 0));
    EXPECT_FALSE(BitUtil::IsBitSet(newNullValues, 1));
    EXPECT_FALSE(BitUtil::IsBitSet(newNullValues, 2));
    EXPECT_EQ(oVec[0], -90);
    EXPECT_EQ(oVec[1], -80);
    EXPECT_EQ(oVec[2], -70);
    context->GetArena()->Reset();

    for (int i = 0; i < 3; i++) {
        BitUtil::SetBit(bitmap[0], i, true);
    }
    for (int i = 0; i < NullsBuffer::CalculateNbytes(3); i++) {
        newNullValues[i] = 0;
    }

    r = func(vals, 3, *cvecVals, nullptr, 3, (int64_t *)(bitmap), (int64_t *)(offsets),
             reinterpret_cast<int32_t *>(newNullValues), newLengths, reinterpret_cast<int64_t>(context),
             dictionaryVectors);
    EXPECT_TRUE(BitUtil::IsBitSet(newNullValues, 0));
    EXPECT_TRUE(BitUtil::IsBitSet(newNullValues, 1));
    EXPECT_TRUE(BitUtil::IsBitSet(newNullValues, 2));

    context->GetArena()->Reset();
    for (int i = 0; i < 1; i++) {
        delete[] bitmap[i];
        delete[] offsets[i];
    }
    delete[] bitmap;
    delete[] offsets;
    delete[] vals;
    delete expr;
    delete context;
}

TEST(CodeGenTest, ProjectionCodeGen)
{
    // create expression objects
    FieldExpr *addLeft = new FieldExpr(0, Decimal128Type(38, 0));
    LiteralExpr *addRight = new LiteralExpr(new string("100"), Decimal128Type(38, 0));

    BinaryExpr *expr =
        new BinaryExpr(omniruntime::expressions::Operator::ADD, addLeft, addRight, Decimal128Type(38, 0));

    ExprPrinter printExprTree;
    expr->Accept(printExprTree);

    // creating decimal
    int64_t c1[6] = {10, 0, 20, 0, 30, 0};

    int64_t *vals = new int64_t[1];
    vals[0] = reinterpret_cast<int64_t>(c1);

    auto **bitmap = new uint8_t *[1];
    bitmap[0] = new uint8_t[NullsBuffer::CalculateNbytes(3)];
    for (int i = 0; i < 3; i++) {
        BitUtil::SetBit(bitmap[0], i, false);
    }
    auto **offsets = new int32_t *[1];
    for (int col = 0; col < 1; col++) {
        offsets[col] = new int32_t[3];
    }

    std::vector<DataTypePtr> vecOfTypes = {Decimal128Type()};
    DataTypes inputTypes(vecOfTypes);

    uint8_t newNullValues[NullsBuffer::CalculateNbytes(3)] = {0};
    int newLengths[3];

    auto codegen = ProjectionCodeGen(defaultTestFunctionName, *expr, false, nullptr);
    int64_t dictionaryVectors[1] = {};

    vector<int64_t> oVec(6);
    auto ov = oVec.data();
    void *vecVals = &ov;
    auto cvecVals = static_cast<int64_t *>(vecVals);
    auto context = new ExecutionContext();
    auto func = (ProjectFunc)(intptr_t)codegen.GetFunction(inputTypes);

    int32_t r = func(vals, 3, *cvecVals, nullptr, 3, (int64_t *)(bitmap), (int64_t *)(offsets),
                     reinterpret_cast<int32_t *>(newNullValues), newLengths, reinterpret_cast<int64_t>(context),
                     dictionaryVectors);
    EXPECT_NE(r, 0);
    EXPECT_FALSE(BitUtil::IsBitSet(newNullValues, 0));
    EXPECT_FALSE(BitUtil::IsBitSet(newNullValues, 1));
    EXPECT_FALSE(BitUtil::IsBitSet(newNullValues, 2));

    EXPECT_EQ(oVec.at(0), 110);
    EXPECT_EQ(oVec.at(1), 0);
    EXPECT_EQ(oVec.at(2), 120);
    EXPECT_EQ(oVec.at(3), 0);
    EXPECT_EQ(oVec.at(4), 130);
    EXPECT_EQ(oVec.at(5), 0);
    context->GetArena()->Reset();

    for (int i = 0; i < 3; i++) {
        BitUtil::SetBit(bitmap[0], i, true);
    }
    for (int i = 0; i < NullsBuffer::CalculateNbytes(3); i++) {
        newNullValues[i] = 0;
    }

    r = func(vals, 3, *cvecVals, nullptr, 3, (int64_t *)(bitmap), (int64_t *)(offsets),
             reinterpret_cast<int32_t *>(newNullValues), newLengths, reinterpret_cast<int64_t>(context),
             dictionaryVectors);
    EXPECT_TRUE(BitUtil::IsBitSet(newNullValues, 0));
    EXPECT_TRUE(BitUtil::IsBitSet(newNullValues, 1));
    EXPECT_TRUE(BitUtil::IsBitSet(newNullValues, 2));

    context->GetArena()->Reset();
    for (int i = 0; i < 1; i++) {
        delete[] bitmap[i];
        delete[] offsets[i];
    }
    delete[] bitmap;
    delete[] offsets;
    delete[] vals;
    delete expr;
    delete context;
}

TEST(CodeGenTest, Pmod)
{
    // create expression objects
    FieldExpr *col0 = new FieldExpr(0, IntType());
    LiteralExpr *data1 = new LiteralExpr(42, IntType());

    std::string hashStr = "mm3hash";
    DataTypePtr retType = IntType();
    std::vector<Expr *> hashArgs;
    hashArgs.push_back(col0);
    hashArgs.push_back(data1);
    auto mhash = GetFuncExpr(hashStr, hashArgs, IntType());

    std::string pmodStr = "pmod";
    std::vector<Expr *> pmodArgs;
    pmodArgs.push_back(mhash);
    auto pmodData1 = new LiteralExpr(42, IntType());
    pmodArgs.push_back(pmodData1);
    auto pmod = GetFuncExpr(pmodStr, pmodArgs, IntType());

    auto equalRight = new LiteralExpr(20, IntType());

    auto expr = new BinaryExpr(omniruntime::expressions::Operator::EQ, pmod, equalRight, BooleanType());
    ExprPrinter printExprTree;
    expr->Accept(printExprTree);

    int32_t v1[1] = {-2147483648};
    auto *vals = new int64_t[1];
    vals[0] = reinterpret_cast<int64_t>(v1);
    auto *selected = new int32_t[1];

    auto **bitmap = new uint8_t *[1];
    bitmap[0] = new uint8_t[NullsBuffer::CalculateNbytes(1)];
    BitUtil::SetBit(bitmap[0], 0, false);
    auto **offsets = new int32_t *[1];
    offsets[0] = new int32_t[1];
    offsets[0][0] = 0;

    std::vector<DataTypePtr> vecOfTypes = {IntType()};
    DataTypes inputTypes(vecOfTypes);

    string testName = "pmodTest";
    auto codegen = FilterCodeGen(testName, *expr, nullptr);
    int32_t (*func)(int64_t *, int32_t, int32_t *, int64_t *, int64_t *, int64_t, int64_t *);
    int64_t dictionaries[1] = {};

    auto context = new ExecutionContext();

    func = (int32_t(*)(int64_t *, int32_t, int32_t *, int64_t *, int64_t *, int64_t, int64_t *))(
               intptr_t)codegen.GetFunction(inputTypes);
    bool result = func(vals, 1, selected, (int64_t *)(bitmap), (int64_t *)(offsets), reinterpret_cast<int64_t>(context),
                       dictionaries);
    EXPECT_EQ(result, true);
    context->GetArena()->Reset();

    delete[] bitmap[0];
    delete[] bitmap;
    delete[] offsets[0];
    delete[] offsets;
    delete[] vals;
    delete[] selected;
    delete context;
    delete expr;
}

TEST(CodeGenTest, CombineHash)
{
    FieldExpr *col0 = new FieldExpr(0, LongType());
    FieldExpr *col1 = new FieldExpr(1, LongType());
    std::string funcStr = "combine_hash";
    DataTypePtr retType = LongType();
    std::vector<Expr *> args;
    args.push_back(col0);
    args.push_back(col1);
    auto combineHash = GetFuncExpr(funcStr, args, LongType());

    FieldExpr *col2 = new FieldExpr(2, LongType());
    BinaryExpr *expr = new BinaryExpr(omniruntime::expressions::Operator::EQ, combineHash, col2, BooleanType());

    ExprPrinter printExprTree;
    expr->Accept(printExprTree);

    int64_t v1[1] = {12};
    int64_t v2[1] = {123};
    int64_t v3[1] = {495};
    int64_t *vals = new int64_t[3];
    vals[0] = reinterpret_cast<int64_t>(v1);
    vals[1] = reinterpret_cast<int64_t>(v2);
    vals[2] = reinterpret_cast<int64_t>(v3);

    auto *selected = new int32_t[1];
    auto **bitmap = new uint8_t *[3];
    for (int i = 0; i < 3; i++) {
        bitmap[i] = new uint8_t[NullsBuffer::CalculateNbytes(1)];
        BitUtil::SetBit(bitmap[i], 0, false);
    }
    int32_t **offsets = new int32_t *[3];
    for (int i = 0; i < 3; i++) {
        offsets[i] = new int32_t[1];
        offsets[i][0] = false;
    }

    std::vector<DataTypePtr> vecOfTypes = {LongType(), LongType(), LongType()};
    DataTypes inputTypes(vecOfTypes);

    auto codegen = FilterCodeGen(defaultTestFunctionName, *expr, nullptr);
    int64_t dictionaries[3] = {};
    auto context = new ExecutionContext();
    auto func = (FilterFunc)(intptr_t)codegen.GetFunction(inputTypes);

    bool result = func(vals, 1, selected, (int64_t *)(bitmap), (int64_t *)(offsets), reinterpret_cast<int64_t>(context),
                       dictionaries);
    EXPECT_EQ(result, true);
    context->GetArena()->Reset();

    for (int i = 0; i < 3; i++) {
        delete[] bitmap[i];
    }
    delete[] bitmap;

    for (int i = 0; i < 3; i++) {
        delete[] offsets[i];
    }
    delete[] offsets;

    delete[] vals;
    delete[] selected;
    delete context;
    delete expr;
}
TEST(CodeGenTest, JSONFunc)
{
    std::string unparsed = R"(
        {
        "exprType": "BINARY",
        "returnType": 4,
        "operator": "EQUAL",
        "left": {
            "exprType": "BINARY",
            "returnType": 2,
            "operator": "ADD",
            "left": {
                "exprType": "LITERAL",
                "dataType": 2,
                "isNull": false,
                "value": 122
            },
            "right": {
                "exprType": "FUNC",
                "returnType": 2,
                "function_name": "abs",
                "arguments": [
                    {
                        "exprType": "FIELD_REFERENCE",
                        "dataType": 2,
                        "colVal": 0
                    }
                ]
            }
        },
        "right": {
            "exprType": "LITERAL",
            "dataType": 2,
            "isNull": false,
            "value": 123
        }
    }
    )";

    nlohmann::json unparsedJSON = nlohmann::json::parse(unparsed);
    std::cout << "parsed into JSON" << std::endl;

    Expr *expr = JSONParser::ParseJSON(unparsedJSON);
    ExprPrinter printer;
    expr->Accept(printer);
    cout << endl;

    int64_t v1[1] = {1};
    int64_t v2[1] = {234};
    int64_t v3[1] = {345};
    int64_t *vals = new int64_t[3];
    vals[0] = reinterpret_cast<int64_t>(v1);
    vals[1] = reinterpret_cast<int64_t>(v2);
    vals[2] = reinterpret_cast<int64_t>(v3);
    int32_t *selected = new int32_t[1];

    auto **bitmap = new uint8_t *[3];
    for (int i = 0; i < 3; i++) {
        bitmap[i] = new uint8_t[NullsBuffer::CalculateNbytes(1)];
        BitUtil::SetBit(bitmap[i], 0, false);
    }

    auto **offsets = new int32_t *[3];
    for (int col = 0; col < 3; col++) {
        offsets[col] = new int32_t[1];
    }

    std::vector<DataTypePtr> vecOfTypes = {LongType(), LongType(), LongType()};
    DataTypes inputTypes(vecOfTypes);

    auto codegen = FilterCodeGen(defaultTestFunctionName, *expr, nullptr);
    int64_t dictionaries[3] = {};
    auto context = new ExecutionContext();
    auto func = (FilterFunc)(intptr_t)codegen.GetFunction(inputTypes);

    int32_t result = func(vals, 1, selected, (int64_t *)(bitmap), (int64_t *)(offsets),
                          reinterpret_cast<int64_t>(context), dictionaries);
    EXPECT_EQ(result, 1);

    v1[0] = -1;
    result = func(vals, 1, selected, (int64_t *)(bitmap), (int64_t *)(offsets), reinterpret_cast<int64_t>(context),
                  dictionaries);
    EXPECT_EQ(result, 1);

    v1[0] = 2;
    result = func(vals, 1, selected, (int64_t *)(bitmap), (int64_t *)(offsets), reinterpret_cast<int64_t>(context),
                  dictionaries);
    EXPECT_EQ(result, 0);

    for (int i = 0; i < 3; i++) {
        delete[] bitmap[i];
        delete[] offsets[i];
    }

    delete context;
    delete[] bitmap;
    delete[] offsets;
    delete[] vals;
    delete[] selected;
    delete expr;
}
