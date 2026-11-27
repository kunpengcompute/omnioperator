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

#include <string>
#include <vector>
#include "operator/filter/filter_and_project.h"
#include "operator/projection/projection.h"
#include "util/test_util.h"
#include "util/config_util.h"
#include "codegen_util.h"

using namespace std;
using namespace omniruntime::vec;
using namespace omniruntime::expressions;
using namespace omniruntime::mem;
using namespace omniruntime::op;
using namespace TestUtil;
using namespace CodegenUtil;

TEST(BatchCodeGenTest, IntIn)
{
    ConfigUtil::SetEnableBatchExprEvaluate(true);
    std::vector<Expr *> args;
    args.push_back(new FieldExpr(0, IntType()));
    args.push_back(new LiteralExpr(1, IntType()));
    args.push_back(new LiteralExpr(3, IntType()));
    args.push_back(new LiteralExpr(5, IntType()));

    InExpr *filterExpr = new InExpr(args);

    auto *projExpr = new FieldExpr(0, IntType());
    auto *projExpr1 = new FieldExpr(1, IntType());
    auto *projExpr2 = new FieldExpr(2, IntType());
    std::vector<Expr *> exprs = {projExpr, projExpr1, projExpr2};

    const int32_t numCols = 3;
    const int32_t numRows = 10000;
    int32_t *col1 = new int32_t[numRows];
    int32_t *col2 = new int32_t[numRows];
    int32_t *col3 = new int32_t[numRows];
    for (int32_t i = 0; i < numRows; i++) {
        col1[i] = i % 10;
        col2[i] = i % 5;
        col3[i] = i % 6 + 12;
    }

    DataTypes inputTypes(std::vector<DataTypePtr>({IntType(), IntType(), IntType()}));
    VectorBatch *vecBatch = CreateVectorBatch(inputTypes, numRows, col1, col2, col3);

    std::vector<std::unique_ptr<Projection>> projections;
    auto filter = GenerateFilterAndProjections(filterExpr, exprs, inputTypes, projections, nullptr);

    int32_t numSelectedRows = 0;
    auto ret = FilterAndProject(filter, projections, numCols, vecBatch, numSelectedRows, inputTypes);
    EXPECT_EQ(3000, numSelectedRows);
    for (int i = 0; i < numSelectedRows; i++) {
        int32_t val0 = (reinterpret_cast<Vector<int32_t> *>(ret->Get(0)))->GetValue(i);
        EXPECT_TRUE(val0 == 1 || val0 == 3 || val0 == 5);
    }

    Expr::DeleteExprs({filterExpr});
    Expr::DeleteExprs(exprs);
    VectorHelper::FreeVecBatch(vecBatch);
    VectorHelper::FreeVecBatch(ret);
    delete[] col1;
    delete[] col2;
    delete[] col3;
    ConfigUtil::SetEnableBatchExprEvaluate(false);
}

TEST(BatchCodeGenTest, LongIn)
{
    ConfigUtil::SetEnableBatchExprEvaluate(true);
    std::vector<Expr *> args;
    int64_t target1 = 1;
    int64_t target2 = 3;
    int64_t target3 = 5;
    args.push_back(new FieldExpr(0, LongType()));
    args.push_back(new LiteralExpr(target1, LongType()));
    args.push_back(new LiteralExpr(target2, LongType()));
    args.push_back(new LiteralExpr(target3, LongType()));

    InExpr *filterExpr = new InExpr(args);
    auto *projExpr = new FieldExpr(0, LongType());
    auto *projExpr1 = new FieldExpr(1, LongType());
    auto *projExpr2 = new FieldExpr(2, LongType());
    std::vector<Expr *> exprs = {projExpr, projExpr1, projExpr2};

    const int32_t numCols = 3;
    const int32_t numRows = 10000;
    int64_t *col1 = new int64_t[numRows];
    int64_t *col2 = new int64_t[numRows];
    int64_t *col3 = new int64_t[numRows];
    for (int32_t i = 0; i < numRows; i++) {
        col1[i] = i % 10;
        col2[i] = i % 5;
        col3[i] = i % 6 + 12;
    }

    DataTypes inputTypes(std::vector<DataTypePtr>({LongType(), LongType(), LongType()}));
    VectorBatch *vecBatch = CreateVectorBatch(inputTypes, numRows, col1, col2, col3);

    std::vector<std::unique_ptr<Projection>> projections;
    auto filter = GenerateFilterAndProjections(filterExpr, exprs, inputTypes, projections, nullptr);

    int32_t numSelectedRows = 0;
    auto ret = FilterAndProject(filter, projections, numCols, vecBatch, numSelectedRows, inputTypes);
    EXPECT_EQ(3000, numSelectedRows);
    for (int i = 0; i < numSelectedRows; i++) {
        int32_t val0 = (reinterpret_cast<Vector<int64_t> *>(ret->Get(0)))->GetValue(i);
        EXPECT_TRUE(val0 == target1 || val0 == target2 || val0 == target3);
    }

    Expr::DeleteExprs({filterExpr});
    Expr::DeleteExprs(exprs);
    VectorHelper::FreeVecBatch(vecBatch);
    VectorHelper::FreeVecBatch(ret);
    delete[] col1;
    delete[] col2;
    delete[] col3;
    ConfigUtil::SetEnableBatchExprEvaluate(false);
}

TEST(BatchCodeGenTest, DoubleIn)
{
    ConfigUtil::SetEnableBatchExprEvaluate(true);
    std::vector<Expr *> args;
    double target1 = 1.0;
    double target2 = 3.0;
    double target3 = 5.0;
    args.push_back(new FieldExpr(0, DoubleType()));
    args.push_back(new LiteralExpr(target1, DoubleType()));
    args.push_back(new LiteralExpr(target2, DoubleType()));
    args.push_back(new LiteralExpr(target3, DoubleType()));

    InExpr *filterExpr = new InExpr(args);

    auto *projExpr = new FieldExpr(0, DoubleType());
    auto *projExpr1 = new FieldExpr(1, DoubleType());
    auto *projExpr2 = new FieldExpr(2, DoubleType());
    std::vector<Expr *> exprs = {projExpr, projExpr1, projExpr2};

    const int32_t numCols = 3;
    const int32_t numRows = 10000;
    double *col1 = new double[numRows];
    double *col2 = new double[numRows];
    double *col3 = new double[numRows];
    for (int32_t i = 0; i < numRows; i++) {
        col1[i] = i % 10;
        col2[i] = i % 5;
        col3[i] = i % 6 + 12;
    }
    DataTypes inputTypes(std::vector<DataTypePtr>({DoubleType(), DoubleType(), DoubleType()}));
    VectorBatch *vecBatch = CreateVectorBatch(inputTypes, numRows, col1, col2, col3);

    std::vector<std::unique_ptr<Projection>> projections;
    auto filter = GenerateFilterAndProjections(filterExpr, exprs, inputTypes, projections, nullptr);

    int32_t numSelectedRows = 0;
    auto ret = FilterAndProject(filter, projections, numCols, vecBatch, numSelectedRows, inputTypes);
    EXPECT_EQ(3000, numSelectedRows);
    for (int i = 0; i < numSelectedRows; i++) {
        int32_t val0 = (reinterpret_cast<Vector<double> *>(ret->Get(0)))->GetValue(i);
        EXPECT_TRUE(val0 == target1 || val0 == target2 || val0 == target3);
    }

    Expr::DeleteExprs({filterExpr});
    Expr::DeleteExprs(exprs);
    VectorHelper::FreeVecBatch(vecBatch);
    VectorHelper::FreeVecBatch(ret);
    delete[] col1;
    delete[] col2;
    delete[] col3;
    ConfigUtil::SetEnableBatchExprEvaluate(false);
}

TEST(BatchCodeGenTest, StringIn)
{
    ConfigUtil::SetEnableBatchExprEvaluate(true);
    std::vector<Expr *> args;
    args.push_back(new FieldExpr(0, VarcharType()));
    args.push_back(new LiteralExpr(new std::string("hello"), VarcharType()));
    args.push_back(new LiteralExpr(new std::string("bye"), VarcharType()));
    args.push_back(new LiteralExpr(new std::string("okay"), VarcharType()));

    InExpr *filterExpr = new InExpr(args);

    auto *projExpr = new FieldExpr(0, VarcharType());
    std::vector<Expr *> exprs = {projExpr};

    const int32_t numCols = 1;
    const int32_t numRows = 10;

    DataTypes inputTypes(std::vector<DataTypePtr>({VarcharType(10)}));
    auto col1 = VectorHelper::CreateStringVector(numRows);
    auto *vector = (Vector<LargeStringContainer<std::string_view>> *)col1;
    std::string value;
    for (int i = 0; i < numRows; i++) {
        if (i % 3 == 0) {
            value = "hello";
        } else {
            value = "hi";
        }
        std::string_view input(value.data(), value.size());
        vector->SetValue(i, input);
    }
    auto *t = new VectorBatch(numRows);
    t->Append(col1);

    std::vector<std::unique_ptr<Projection>> projections;
    auto filter = GenerateFilterAndProjections(filterExpr, exprs, inputTypes, projections, nullptr);

    int32_t numSelectedRows = 0;
    auto ret = FilterAndProject(filter, projections, numCols, t, numSelectedRows, inputTypes);
    EXPECT_EQ(numSelectedRows, 4);

    Expr::DeleteExprs({filterExpr});
    Expr::DeleteExprs(exprs);
    VectorHelper::FreeVecBatch(t);
    VectorHelper::FreeVecBatch(ret);
    ConfigUtil::SetEnableBatchExprEvaluate(false);
}

TEST(BatchCodeGenTest, Decimal64In)
{
    ConfigUtil::SetEnableBatchExprEvaluate(true);
    auto arg0 = new FieldExpr(0, Decimal64Type(6, 0));
    auto arg1 = new LiteralExpr(120945L, Decimal64Type(6, 0));
    auto arg2 = new LiteralExpr(65781L, Decimal64Type(6, 0));
    auto arg3 = new LiteralExpr(65781L, Decimal64Type(6, 0));
    std::vector<Expr *> args = {arg0, arg1, arg2, arg3};
    auto expr = new InExpr(args);
    std::vector<Expr *> exprs = {expr};

    const int32_t numRows = 1;
    const int32_t numCols = 1;
    auto col1 = new int64_t[numRows];
    col1[0] = 65781;
    std::vector<DataTypePtr> vecOfTypes = {Decimal64Type(6, 0)};
    DataTypes inputTypes(vecOfTypes);
    VectorBatch *vecBatch = CreateVectorBatch(inputTypes, numRows, col1);

    std::vector<std::unique_ptr<Projection>> projections;
    auto filter = GenerateFilterAndProjections(nullptr, exprs, inputTypes, projections, nullptr);

    int32_t numSelectedRows = 1;
    auto ret = FilterAndProject(filter, projections, numCols, vecBatch, numSelectedRows, inputTypes);

    bool val0 = (reinterpret_cast<Vector<bool> *>(ret->Get(0)))->GetValue(0);
    EXPECT_TRUE(val0);

    Expr::DeleteExprs(exprs);
    VectorHelper::FreeVecBatch(vecBatch);
    VectorHelper::FreeVecBatch(ret);
    delete[] col1;
    ConfigUtil::SetEnableBatchExprEvaluate(false);
}

TEST(BatchCodeGenTest, Decimal128In)
{
    ConfigUtil::SetEnableBatchExprEvaluate(true);
    std::vector<Expr *> args;
    args.push_back(new FieldExpr(0, Decimal128Type(38, 0)));
    args.push_back(new LiteralExpr(new std::string("1000"), Decimal128Type(38, 0)));
    args.push_back(new LiteralExpr(new std::string("2000"), Decimal128Type(38, 0)));
    args.push_back(new LiteralExpr(new std::string("555555"), Decimal128Type(38, 0)));

    InExpr *filterExpr = new InExpr(args);

    auto *projExpr = new FieldExpr(0, Decimal128Type(38, 0));
    std::vector<Expr *> exprs = {projExpr};

    const int32_t numCols = 1;
    const int32_t numRows = 1000;
    int64_t *data1 = new int64_t[numRows * 2];
    for (int32_t i = 0; i < numRows; i++) {
        data1[2 * i] = (i + 1) * 1000;
        data1[2 * i + 1] = 0;
    }
    DataTypes inputTypes(std::vector<DataTypePtr>({Decimal128Type()}));
    VectorBatch *vecBatch = CreateVectorBatch(inputTypes, numRows, data1);

    std::vector<std::unique_ptr<Projection>> projections;
    auto filter = GenerateFilterAndProjections(filterExpr, exprs, inputTypes, projections, nullptr);

    int32_t numSelectedRows = 0;
    auto ret = FilterAndProject(filter, projections, numCols, vecBatch, numSelectedRows, inputTypes);
    EXPECT_EQ(numSelectedRows, 2);

    Expr::DeleteExprs({filterExpr});
    Expr::DeleteExprs(exprs);
    VectorHelper::FreeVecBatch(vecBatch);
    VectorHelper::FreeVecBatch(ret);
    delete[] data1;
    ConfigUtil::SetEnableBatchExprEvaluate(false);
}

TEST(BatchCodeGenTest, IntBetween)
{
    ConfigUtil::SetEnableBatchExprEvaluate(true);
    BetweenExpr *filterExpr =
        new BetweenExpr(new FieldExpr(1, IntType()), new FieldExpr(0, IntType()), new FieldExpr(2, IntType()));

    auto *projExpr = new FieldExpr(0, IntType());
    auto *projExpr1 = new FieldExpr(1, IntType());
    auto *projExpr2 = new FieldExpr(2, IntType());
    std::vector<Expr *> exprs = {projExpr, projExpr1, projExpr2};

    const int32_t numCols = 3;
    const int32_t numRows = 10000;
    int32_t *col1 = new int32_t[numRows];
    int32_t *col2 = new int32_t[numRows];
    int32_t *col3 = new int32_t[numRows];
    for (int32_t i = 0; i < numRows; i++) {
        col1[i] = i % 5;
        col2[i] = i % 11;
        col3[i] = (i % 21) - 3;
    }

    DataTypes inputTypes(std::vector<DataTypePtr>({IntType(), IntType(), IntType()}));
    VectorBatch *vecBatch = CreateVectorBatch(inputTypes, numRows, col1, col2, col3);

    std::vector<std::unique_ptr<Projection>> projections;
    auto filter = GenerateFilterAndProjections(filterExpr, exprs, inputTypes, projections, nullptr);

    int32_t numSelectedRows = 0;
    auto ret = FilterAndProject(filter, projections, numCols, vecBatch, numSelectedRows, inputTypes);
    EXPECT_EQ(numSelectedRows, 4705);
    for (int i = 0; i < numSelectedRows; i++) {
        int32_t val0 = (reinterpret_cast<Vector<int32_t> *>(ret->Get(0)))->GetValue(i);
        int32_t val1 = (reinterpret_cast<Vector<int32_t> *>(ret->Get(1)))->GetValue(i);
        int32_t val2 = (reinterpret_cast<Vector<int32_t> *>(ret->Get(2)))->GetValue(i);
        EXPECT_TRUE((val0 <= val1) && (val1 <= val2));
    }

    Expr::DeleteExprs({filterExpr});
    Expr::DeleteExprs(exprs);
    VectorHelper::FreeVecBatch(vecBatch);
    VectorHelper::FreeVecBatch(ret);
    delete[] col1;
    delete[] col2;
    delete[] col3;
    ConfigUtil::SetEnableBatchExprEvaluate(false);
}

TEST(BatchCodeGenTest, LongBetween)
{
    ConfigUtil::SetEnableBatchExprEvaluate(true);
    BetweenExpr *filterExpr =
        new BetweenExpr(new FieldExpr(1, LongType()), new FieldExpr(0, LongType()), new FieldExpr(2, LongType()));

    auto *projExpr = new FieldExpr(0, LongType());
    auto *projExpr1 = new FieldExpr(1, LongType());
    auto *projExpr2 = new FieldExpr(2, LongType());
    std::vector<Expr *> exprs = {projExpr, projExpr1, projExpr2};

    const int32_t numCols = 3;
    const int32_t numRows = 10000;
    int64_t *col1 = new int64_t[numRows];
    int64_t *col2 = new int64_t[numRows];
    int64_t *col3 = new int64_t[numRows];
    for (int32_t i = 0; i < numRows; i++) {
        col1[i] = i % 5;
        col2[i] = i % 11;
        col3[i] = (i % 21) - 3;
    }

    DataTypes inputTypes(std::vector<DataTypePtr>({LongType(), LongType(), LongType()}));
    VectorBatch *vecBatch = CreateVectorBatch(inputTypes, numRows, col1, col2, col3);

    std::vector<std::unique_ptr<Projection>> projections;
    auto filter = GenerateFilterAndProjections(filterExpr, exprs, inputTypes, projections, nullptr);

    int32_t numSelectedRows = 0;
    auto ret = FilterAndProject(filter, projections, numCols, vecBatch, numSelectedRows, inputTypes);
    EXPECT_EQ(numSelectedRows, 4705);
    for (int i = 0; i < numSelectedRows; i++) {
        int64_t val0 = (reinterpret_cast<Vector<int64_t> *>(ret->Get(0)))->GetValue(i);
        int64_t val1 = (reinterpret_cast<Vector<int64_t> *>(ret->Get(1)))->GetValue(i);
        int64_t val2 = (reinterpret_cast<Vector<int64_t> *>(ret->Get(2)))->GetValue(i);
        EXPECT_TRUE((val0 <= val1) && (val1 <= val2));
    }

    Expr::DeleteExprs({filterExpr});
    delete[] col1;
    delete[] col2;
    delete[] col3;
    Expr::DeleteExprs(exprs);
    VectorHelper::FreeVecBatch(vecBatch);
    VectorHelper::FreeVecBatch(ret);
    ConfigUtil::SetEnableBatchExprEvaluate(false);
}

TEST(BatchCodeGenTest, DoubleBetween)
{
    ConfigUtil::SetEnableBatchExprEvaluate(true);
    BetweenExpr *filterExpr =
        new BetweenExpr(new FieldExpr(1, DoubleType()), new FieldExpr(0, DoubleType()), new FieldExpr(2, DoubleType()));

    auto *projExpr = new FieldExpr(0, DoubleType());
    auto *projExpr1 = new FieldExpr(1, DoubleType());
    auto *projExpr2 = new FieldExpr(2, DoubleType());
    std::vector<Expr *> exprs = {projExpr, projExpr1, projExpr2};

    const int32_t numCols = 3;
    const int32_t numRows = 10000;
    double *col1 = new double[numRows];
    double *col2 = new double[numRows];
    double *col3 = new double[numRows];
    for (int32_t i = 0; i < numRows; i++) {
        col1[i] = i % 5;
        col2[i] = i % 11;
        col3[i] = (i % 21) - 3;
    }

    DataTypes inputTypes(std::vector<DataTypePtr>({DoubleType(), DoubleType(), DoubleType()}));
    VectorBatch *vecBatch = CreateVectorBatch(inputTypes, numRows, col1, col2, col3);

    std::vector<std::unique_ptr<Projection>> projections;
    auto filter = GenerateFilterAndProjections(filterExpr, exprs, inputTypes, projections, nullptr);

    int32_t numSelectedRows = 0;
    auto ret = FilterAndProject(filter, projections, numCols, vecBatch, numSelectedRows, inputTypes);
    EXPECT_EQ(numSelectedRows, 4705);

    for (int i = 0; i < numSelectedRows; i++) {
        double val0 = (reinterpret_cast<Vector<double> *>(ret->Get(0)))->GetValue(i);
        double val1 = (reinterpret_cast<Vector<double> *>(ret->Get(1)))->GetValue(i);
        double val2 = (reinterpret_cast<Vector<double> *>(ret->Get(2)))->GetValue(i);
        EXPECT_TRUE((val0 <= val1) && (val1 <= val2));
    }

    Expr::DeleteExprs({filterExpr});
    delete[] col1;
    delete[] col2;
    delete[] col3;
    Expr::DeleteExprs(exprs);
    VectorHelper::FreeVecBatch(vecBatch);
    VectorHelper::FreeVecBatch(ret);
    ConfigUtil::SetEnableBatchExprEvaluate(false);
}

TEST(BatchCodeGenTest, Decimal64Between)
{
    ConfigUtil::SetEnableBatchExprEvaluate(true);
    auto value = new FieldExpr(0, Decimal64Type(5, 2));
    auto lowerBound = new LiteralExpr(87230L, Decimal64Type(5, 4));
    auto upperBound = new LiteralExpr(876903L, Decimal64Type(6, 1));
    auto expr = new BetweenExpr(value, lowerBound, upperBound);

    std::vector<Expr *> exprs = {expr};

    const int32_t numRows = 1;
    const int32_t numCols = 1;
    auto col1 = new int64_t[numRows];
    col1[0] = 76582;
    std::vector<DataTypePtr> vecOfTypes = {Decimal64Type(5, 2)};
    DataTypes inputTypes(vecOfTypes);
    VectorBatch *vecBatch = CreateVectorBatch(inputTypes, numRows, col1);

    std::vector<std::unique_ptr<Projection>> projections;
    auto filter = GenerateFilterAndProjections(nullptr, exprs, inputTypes, projections, nullptr);

    int32_t numSelectedRows = 1;
    auto ret = FilterAndProject(filter, projections, numCols, vecBatch, numSelectedRows, inputTypes);

    bool val0 = (reinterpret_cast<Vector<bool> *>(ret->Get(0)))->GetValue(0);
    EXPECT_TRUE(val0);

    Expr::DeleteExprs(exprs);
    VectorHelper::FreeVecBatch(vecBatch);
    VectorHelper::FreeVecBatch(ret);
    delete[] col1;
    ConfigUtil::SetEnableBatchExprEvaluate(false);
}

TEST(BatchCodeGenTest, Decimal128Between)
{
    ConfigUtil::SetEnableBatchExprEvaluate(true);
    auto lowerBound = new LiteralExpr(new std::string("999"), Decimal128Type(38, 0));
    auto upperBound = new LiteralExpr(new std::string("3001"), Decimal128Type(38, 0));

    auto filterExpr = new BetweenExpr(new FieldExpr(0, Decimal128Type()), lowerBound, upperBound);

    auto *projExpr = new FieldExpr(0, Decimal128Type(38, 0));
    std::vector<Expr *> exprs = {projExpr};

    const int32_t numCols = 1;
    const int32_t numRows = 100;
    int64_t *data1 = new int64_t[numRows * 2];
    for (int32_t i = 0; i < numRows; i++) {
        data1[2 * i] = (i + 1) * 1000;
        data1[2 * i + 1] = 0;
    }
    DataTypes inputTypes(std::vector<DataTypePtr>({Decimal128Type()}));

    VectorBatch *vecBatch = CreateVectorBatch(inputTypes, numRows, data1);

    std::vector<std::unique_ptr<Projection>> projections;
    auto filter = GenerateFilterAndProjections(filterExpr, exprs, inputTypes, projections, nullptr);

    int32_t numSelectedRows = 0;
    auto ret = FilterAndProject(filter, projections, numCols, vecBatch, numSelectedRows, inputTypes);
    EXPECT_EQ(numSelectedRows, 3);

    Expr::DeleteExprs({filterExpr});
    Expr::DeleteExprs(exprs);
    VectorHelper::FreeVecBatch(vecBatch);
    VectorHelper::FreeVecBatch(ret);
    delete[] data1;
    ConfigUtil::SetEnableBatchExprEvaluate(false);
}

TEST(BatchCodeGenTest, DoubleCoalesce)
{
    ConfigUtil::SetEnableBatchExprEvaluate(true);
    CoalesceExpr *coalesceExpr = new CoalesceExpr(new FieldExpr(1, DoubleType()), new FieldExpr(0, DoubleType()));
    BinaryExpr *filterExpr = new BinaryExpr(omniruntime::expressions::Operator::EQ, new LiteralExpr(21.0, DoubleType()),
                                            coalesceExpr, BooleanType());

    auto *projExpr = new FieldExpr(0, DoubleType());
    auto *projExpr1 = new FieldExpr(1, DoubleType());
    auto *projExpr2 = new FieldExpr(2, DoubleType());
    std::vector<Expr *> exprs = {projExpr, projExpr1, projExpr2};

    const int32_t numCols = 3;
    const int32_t numRows = 1000;
    double *col1 = new double[numRows];
    double *col2 = new double[numRows];
    double *col3 = new double[numRows];

    for (int32_t i = 0; i < numRows; i++) {
        col1[i] = 100.0;
        col2[i] = 21.0;
        col3[i] = -1.0;
    }

    DataTypes inputTypes(std::vector<DataTypePtr>({DoubleType(), DoubleType(), DoubleType()}));
    VectorBatch *vecBatch = CreateVectorBatch(inputTypes, numRows, col1, col2, col3);
    for (int32_t i = 0; i < numRows; i++) {
        if (i % 2 != 0) {
            vecBatch->Get(1)->SetNull(i);
        }
    }

    std::vector<std::unique_ptr<Projection>> projections;
    auto filter = GenerateFilterAndProjections(filterExpr, exprs, inputTypes, projections, nullptr);

    int32_t numSelectedRows = 0;
    auto ret = FilterAndProject(filter, projections, numCols, vecBatch, numSelectedRows, inputTypes);
    EXPECT_EQ(numSelectedRows, 500);

    Expr::DeleteExprs({filterExpr});
    Expr::DeleteExprs(exprs);
    VectorHelper::FreeVecBatch(vecBatch);
    VectorHelper::FreeVecBatch(ret);
    delete[] col1;
    delete[] col2;
    delete[] col3;
    ConfigUtil::SetEnableBatchExprEvaluate(false);
}

TEST(BatchCodeGenTest, IntCoalesce)
{
    ConfigUtil::SetEnableBatchExprEvaluate(true);
    CoalesceExpr *coalesceExpr = new CoalesceExpr(new FieldExpr(1, IntType()), new FieldExpr(0, IntType()));
    BinaryExpr *filterExpr = new BinaryExpr(omniruntime::expressions::Operator::EQ, new LiteralExpr(21, IntType()),
                                            coalesceExpr, BooleanType());

    auto *projExpr = new FieldExpr(0, IntType());
    auto *projExpr1 = new FieldExpr(1, IntType());
    auto *projExpr2 = new FieldExpr(2, IntType());
    std::vector<Expr *> exprs = {projExpr, projExpr1, projExpr2};

    const int32_t numCols = 3;
    const int32_t numRows = 1000;
    auto *col1 = new int32_t[numRows];
    auto *col2 = new int32_t[numRows];
    auto *col3 = new int32_t[numRows];

    for (int32_t i = 0; i < numRows; i++) {
        col1[i] = 100;
        col2[i] = 21;
        col3[i] = -1;
    }

    DataTypes inputTypes(std::vector<DataTypePtr>({IntType(), IntType(), IntType()}));
    VectorBatch *vecBatch = CreateVectorBatch(inputTypes, numRows, col1, col2, col3);
    for (int32_t i = 0; i < numRows; i++) {
        if (i % 2 != 0) {
            vecBatch->Get(1)->SetNull(i);
        }
    }

    std::vector<std::unique_ptr<Projection>> projections;
    auto filter = GenerateFilterAndProjections(filterExpr, exprs, inputTypes, projections, nullptr);

    int32_t numSelectedRows = 0;
    auto ret = FilterAndProject(filter, projections, numCols, vecBatch, numSelectedRows, inputTypes);

    EXPECT_EQ(numSelectedRows, 500);

    Expr::DeleteExprs({filterExpr});
    Expr::DeleteExprs(exprs);
    VectorHelper::FreeVecBatch(vecBatch);
    VectorHelper::FreeVecBatch(ret);
    delete[] col1;
    delete[] col2;
    delete[] col3;
    ConfigUtil::SetEnableBatchExprEvaluate(false);
}

TEST(BatchCodeGenTest, StringCoalesce)
{
    ConfigUtil::SetEnableBatchExprEvaluate(true);
    CoalesceExpr *coalesceExpr =
        new CoalesceExpr(new FieldExpr(0, VarcharType()), new LiteralExpr(new std::string("bye"), VarcharType()));
    BinaryExpr *filterExpr = new BinaryExpr(omniruntime::expressions::Operator::EQ, coalesceExpr,
                                            new LiteralExpr(new std::string("hello"), VarcharType()), BooleanType());

    auto *projExpr = new FieldExpr(0, VarcharType());
    std::vector<Expr *> exprs = {projExpr};

    const int32_t numCols = 1;
    const int32_t numRows = 1000;

    DataType dataType(OMNI_VARCHAR);
    DataTypes inputTypes(std::vector<DataTypePtr>({VarcharType(30)}));
    auto col1 = VectorHelper::CreateStringVector(numRows);
    auto *vector = (Vector<LargeStringContainer<std::string_view>> *)col1;
    std::string value;
    for (int i = 0; i < numRows; i++) {
        value = "hello";
        std::string_view input(value.data(), value.size());
        vector->SetValue(i, input);
        if (i % 2 != 0) {
            vector->SetNull(i);
        }
    }
    auto *t = new VectorBatch(numRows);
    t->Append(col1);

    std::vector<std::unique_ptr<Projection>> projections;
    auto filter = GenerateFilterAndProjections(filterExpr, exprs, inputTypes, projections, nullptr);

    int32_t numSelectedRows = 0;
    auto ret = FilterAndProject(filter, projections, numCols, t, numSelectedRows, inputTypes);
    EXPECT_EQ(numSelectedRows, 500);

    Expr::DeleteExprs({filterExpr});
    Expr::DeleteExprs(exprs);
    VectorHelper::FreeVecBatch(t);
    VectorHelper::FreeVecBatch(ret);
    ConfigUtil::SetEnableBatchExprEvaluate(false);
}

TEST(BatchCodeGenTest, LongCoalesce)
{
    ConfigUtil::SetEnableBatchExprEvaluate(true);
    int64_t targetValue = 21;
    CoalesceExpr *coalesceExpr = new CoalesceExpr(new FieldExpr(1, LongType()), new FieldExpr(0, LongType()));
    BinaryExpr *filterExpr = new BinaryExpr(omniruntime::expressions::Operator::EQ,
                                            new LiteralExpr(targetValue, LongType()), coalesceExpr, BooleanType());

    auto *projExpr = new FieldExpr(0, LongType());
    auto *projExpr1 = new FieldExpr(1, LongType());
    auto *projExpr2 = new FieldExpr(2, LongType());
    std::vector<Expr *> exprs = {projExpr, projExpr1, projExpr2};

    const int32_t numCols = 3;
    const int32_t numRows = 1000;
    auto *col1 = new int64_t[numRows];
    auto *col2 = new int64_t[numRows];
    auto *col3 = new int64_t[numRows];

    for (int32_t i = 0; i < numRows; i++) {
        col1[i] = 100;
        col2[i] = 21;
        col3[i] = -1;
    }

    DataTypes inputTypes(std::vector<DataTypePtr>({LongType(), LongType(), LongType()}));
    VectorBatch *vecBatch = CreateVectorBatch(inputTypes, numRows, col1, col2, col3);
    for (int32_t i = 0; i < numRows; i++) {
        if (i % 2 != 0) {
            vecBatch->Get(1)->SetNull(i);
        }
    }

    std::vector<std::unique_ptr<Projection>> projections;
    auto filter = GenerateFilterAndProjections(filterExpr, exprs, inputTypes, projections, nullptr);

    int32_t numSelectedRows = 0;
    auto ret = FilterAndProject(filter, projections, numCols, vecBatch, numSelectedRows, inputTypes);
    EXPECT_EQ(numSelectedRows, 500);

    Expr::DeleteExprs({filterExpr});
    Expr::DeleteExprs(exprs);
    VectorHelper::FreeVecBatch(vecBatch);
    VectorHelper::FreeVecBatch(ret);
    delete[] col1;
    delete[] col2;
    delete[] col3;
    ConfigUtil::SetEnableBatchExprEvaluate(false);
}

TEST(BatchCodeGenTest, Decimal64Coalesce)
{
    ConfigUtil::SetEnableBatchExprEvaluate(true);
    auto condition = new LiteralExpr(true, BooleanType());
    auto v1 = new LiteralExpr(123400L, Decimal64Type(7, 2));
    v1->isNull = true;
    auto v2 = new LiteralExpr(1234L, Decimal64Type(7, 2));
    auto coalesce = new CoalesceExpr(v1, v2);

    auto falseExpr = new LiteralExpr(1234000L, Decimal64Type(7, 2));
    auto ifExpr = new IfExpr(condition, coalesce, falseExpr);

    auto subLeft = new LiteralExpr(12340L, Decimal64Type(7, 2));
    auto subRight = new LiteralExpr(1010L, Decimal64Type(7, 2));
    auto right = new BinaryExpr(omniruntime::expressions::Operator::SUB, subLeft, subRight, Decimal64Type(7, 2));
    auto expr = new BinaryExpr(omniruntime::expressions::Operator::GT, ifExpr, right, BooleanType());

    std::vector<Expr *> exprs = {expr};

    const int32_t numRows = 1;
    const int32_t numCols = 1;
    auto col1 = new int64_t[numRows];
    col1[0] = 1234;
    std::vector<DataTypePtr> vecOfTypes = {Decimal64Type(7, 2)};
    DataTypes inputTypes(vecOfTypes);

    VectorBatch *vecBatch = CreateVectorBatch(inputTypes, numRows, col1);

    std::vector<std::unique_ptr<Projection>> projections;
    auto filter = GenerateFilterAndProjections(nullptr, exprs, inputTypes, projections, nullptr);

    int32_t numSelectedRows = numRows;
    auto ret = FilterAndProject(filter, projections, numCols, vecBatch, numSelectedRows, inputTypes);
    bool val0 = (reinterpret_cast<Vector<bool> *>(ret->Get(0)))->GetValue(0);
    EXPECT_FALSE(val0);

    Expr::DeleteExprs(exprs);
    VectorHelper::FreeVecBatch(vecBatch);
    VectorHelper::FreeVecBatch(ret);
    delete[] col1;
    ConfigUtil::SetEnableBatchExprEvaluate(false);
}

TEST(BatchCodeGenTest, Decimal128Coalesce)
{
    ConfigUtil::SetEnableBatchExprEvaluate(true);
    auto v1 = new LiteralExpr(new std::string("500000"), Decimal128Type(38, 0));
    v1->isNull = false;
    auto v2 = new LiteralExpr(new std::string("1234"), Decimal128Type(38, 0));
    auto coalesce = new CoalesceExpr(v1, v2);
    BinaryExpr *filterExpr = new BinaryExpr(omniruntime::expressions::Operator::LTE,
                                            new FieldExpr(0, Decimal128Type(38, 0)), coalesce, BooleanType());

    auto *projExpr = new FieldExpr(0, Decimal128Type(38, 0));
    std::vector<Expr *> exprs = {projExpr};

    const int32_t numCols = 1;
    const int32_t numRows = 1000;
    int64_t *data1 = new int64_t[numRows * 2];
    for (int32_t i = 0; i < numRows; i++) {
        data1[2 * i] = (i + 1) * 1000;
        data1[2 * i + 1] = 0;
    }
    std::vector<DataTypePtr> vecOfTypes = {Decimal128Type()};
    DataTypes inputTypes(vecOfTypes);

    VectorBatch *vecBatch = CreateVectorBatch(inputTypes, numRows, data1);

    std::vector<std::unique_ptr<Projection>> projections;
    auto filter = GenerateFilterAndProjections(filterExpr, exprs, inputTypes, projections, nullptr);

    int32_t numSelectedRows = 0;
    auto ret = FilterAndProject(filter, projections, numCols, vecBatch, numSelectedRows, inputTypes);
    EXPECT_EQ(numSelectedRows, 500);

    delete[] data1;
    Expr::DeleteExprs({filterExpr});
    Expr::DeleteExprs(exprs);
    VectorHelper::FreeVecBatch(vecBatch);
    VectorHelper::FreeVecBatch(ret);
    ConfigUtil::SetEnableBatchExprEvaluate(false);
}
