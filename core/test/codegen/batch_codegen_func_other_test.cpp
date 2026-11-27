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

TEST(BatchCodeGenTest, CastDouble)
{
    ConfigUtil::SetEnableBatchExprEvaluate(true);
    auto data1 = new FieldExpr(0, DoubleType());
    std::string castStr = "CAST";
    std::vector<Expr *> args1;
    args1.push_back(data1);
    auto castExpr1 = GetFuncExpr(castStr, args1, IntType());

    auto data2 = new FieldExpr(1, DoubleType());
    std::vector<Expr *> args2;
    args2.push_back(data2);
    auto castExpr2 = GetFuncExpr(castStr, args2, LongType());
    std::vector<Expr *> exprs = {castExpr1, castExpr2};

    const int32_t numRows = 1000;
    const int32_t numCols = 2;
    double *col1 = MakeDoubles(numRows);
    double *col2 = MakeDoubles(numRows);
    std::vector<DataTypePtr> vecOfTypes = {DoubleType(), DoubleType()};
    DataTypes inputTypes(vecOfTypes);
    VectorBatch *vecBatch = CreateVectorBatch(inputTypes, numRows, col1, col2);

    std::vector<std::unique_ptr<Projection>> projections;
    auto filter = GenerateFilterAndProjections(nullptr, exprs, inputTypes, projections, nullptr);

    int32_t numSelectedRows = numRows;
    auto ret = FilterAndProject(filter, projections, numCols, vecBatch, numSelectedRows, inputTypes);
    for (int i = 0; i < numRows; ++i) {
        int32_t val0 = (reinterpret_cast<Vector<int32_t> *>(ret->Get(0)))->GetValue(i);
        int64_t val1 = (reinterpret_cast<Vector<int64_t> *>(ret->Get(1)))->GetValue(i);
        EXPECT_EQ(val0, i);
        EXPECT_EQ(val1, i);
    }

    Expr::DeleteExprs(exprs);
    delete[] col1;
    delete[] col2;
    VectorHelper::FreeVecBatch(vecBatch);
    VectorHelper::FreeVecBatch(ret);
    ConfigUtil::SetEnableBatchExprEvaluate(false);
}

TEST(BatchCodeGenTest, ProjectSparkConfig)
{
    ConfigUtil::SetEnableBatchExprEvaluate(true);
    std::string castStr = "CAST";
    std::vector<Expr *> argLeft{new FieldExpr(0, Decimal64Type(7, 0))};
    FuncExpr *subLeft = GetFuncExpr(castStr, argLeft, Decimal64Type(8, 0));
    std::vector<Expr *> argRight{new FieldExpr(0, Decimal64Type(7, 0))};
    FuncExpr *subRight = GetFuncExpr(castStr, argRight, Decimal64Type(8, 0));
    auto *addExprs = new BinaryExpr(omniruntime::expressions::Operator::ADD, subLeft, subRight, Decimal64Type(8, 0));

    std::vector<Expr *> exprs = {addExprs};

    const int32_t numRows = 1;
    int64_t *col = new int64_t[1];
    col[0] = 123;
    const int32_t numCols = 1;
    std::vector<DataTypePtr> vecOfTypes = {LongType()};
    DataTypes inputTypes(vecOfTypes);
    VectorBatch *vecBatch = CreateVectorBatch(inputTypes, numRows, col);

    auto overflowConfig = new OverflowConfig(omniruntime::op::OVERFLOW_CONFIG_NULL);
    std::vector<std::unique_ptr<Projection>> projections;
    auto filter = GenerateFilterAndProjections(nullptr, exprs, inputTypes, projections, overflowConfig);

    int32_t numSelectedRows = numRows;
    auto ret = FilterAndProject(filter, projections, numCols, vecBatch, numSelectedRows, inputTypes);
    int64_t val0 = (reinterpret_cast<Vector<int64_t> *>(ret->Get(0)))->GetValue(0);
    EXPECT_EQ(val0, 246);

    Expr::DeleteExprs(exprs);
    delete overflowConfig;
    delete[] col;
    VectorHelper::FreeVecBatch(vecBatch);
    VectorHelper::FreeVecBatch(ret);
    ConfigUtil::SetEnableBatchExprEvaluate(false);
}

TEST(BatchCodeGenTest, CastDecimal128ToString)
{
    ConfigUtil::SetEnableBatchExprEvaluate(true);
    auto data = new FieldExpr(0, Decimal128Type(38, 0));
    std::string castStr = "CAST";
    std::vector<Expr *> args;
    args.push_back(data);
    auto castExpr = GetFuncExpr(castStr, args, VarcharType(5));

    std::vector<Expr *> exprs = {castExpr};

    const int32_t numRows = 3;
    auto *col = MakeDecimals(numRows, 10);
    const int32_t numCols = 1;
    std::vector<DataTypePtr> vecOfTypes = {Decimal128Type()};
    DataTypes inputTypes(vecOfTypes);
    VectorBatch *vecBatch = CreateVectorBatch(inputTypes, numRows, col);

    std::vector<std::unique_ptr<Projection>> projections;
    auto filter = GenerateFilterAndProjections(nullptr, exprs, inputTypes, projections, nullptr);

    int32_t numSelectedRows = numRows;
    auto ret = FilterAndProject(filter, projections, numCols, vecBatch, numSelectedRows, inputTypes);

    auto *vcVec = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(ret->Get(0));
    for (int32_t i = 0; i < numRows; i++) {
        EXPECT_TRUE(vcVec->GetValue(i) == to_string(col[i * 2]));
    }

    Expr::DeleteExprs(exprs);
    delete[] col;
    VectorHelper::FreeVecBatch(vecBatch);
    VectorHelper::FreeVecBatch(ret);
    ConfigUtil::SetEnableBatchExprEvaluate(false);
}

TEST(BatchCodeGenTest, AllType)
{
    ConfigUtil::SetEnableBatchExprEvaluate(true);
    FieldExpr *eq2Left = new FieldExpr(1, LongType());
    LiteralExpr *eq2Right = new LiteralExpr(3000000000L, LongType());
    BinaryExpr *eq2Expr = new BinaryExpr(omniruntime::expressions::Operator::EQ, eq2Left, eq2Right, BooleanType());

    FieldExpr *gteLeft = new FieldExpr(2, DoubleType());
    LiteralExpr *gteRight = new LiteralExpr(0.4, DoubleType());
    BinaryExpr *gteExpr = new BinaryExpr(omniruntime::expressions::Operator::GTE, gteLeft, gteRight, BooleanType());

    BinaryExpr *innerAndExpr = new BinaryExpr(omniruntime::expressions::Operator::AND, eq2Expr, gteExpr, BooleanType());

    FieldExpr *eq1Left = new FieldExpr(0, IntType());
    LiteralExpr *eq1Right = new LiteralExpr(0, IntType());
    BinaryExpr *eq1Expr = new BinaryExpr(omniruntime::expressions::Operator::EQ, eq1Left, eq1Right, BooleanType());

    Expr *filterExpr = new BinaryExpr(omniruntime::expressions::Operator::AND, eq1Expr, innerAndExpr, BooleanType());

    auto *projExpr1 = new FieldExpr(0, IntType());
    auto *projExpr2 = new FieldExpr(1, LongType());
    auto *projExpr3 = new FieldExpr(2, DoubleType());
    std::vector<Expr *> exprs = {projExpr1, projExpr2, projExpr3};

    const int32_t numCols = 3;
    const int32_t numRows = 1000;
    int32_t *data1 = new int32_t[numRows];
    int64_t *data2 = new int64_t[numRows];
    double *data3 = new double[numRows];
    for (int32_t i = 0; i < numRows; i++) {
        data1[i] = i % 3;
        data2[i] = (i % 2 != 0) ? 3e9 : 0;
        data3[i] = i % 10 / 10.0;
    }

    DataTypes inputTypes(std::vector<DataTypePtr>({IntType(), LongType(), DoubleType()}));
    VectorBatch *vecBatch = CreateVectorBatch(inputTypes, numRows, data1, data2, data3);

    std::vector<std::unique_ptr<Projection>> projections;
    auto filter = GenerateFilterAndProjections(filterExpr, exprs, inputTypes, projections, nullptr);

    int32_t numSelectedRows = 0;
    auto ret = FilterAndProject(filter, projections, numCols, vecBatch, numSelectedRows, inputTypes);
    EXPECT_EQ(numSelectedRows, 100);

    Expr::DeleteExprs({filterExpr});
    Expr::DeleteExprs(exprs);
    VectorHelper::FreeVecBatch(vecBatch);
    VectorHelper::FreeVecBatch(ret);
    delete[] data1;
    delete[] data2;
    delete[] data3;
    ConfigUtil::SetEnableBatchExprEvaluate(false);
}

TEST(BatchCodeGenTest, Round)
{
    ConfigUtil::SetEnableBatchExprEvaluate(true);
    std::string funcStr = "round";

    auto data0 = new FieldExpr(0, IntType());
    std::vector<Expr *> args0;
    args0.push_back(data0);
    int32_t data0Decimal = 1;
    args0.push_back(new LiteralExpr(data0Decimal, IntType()));
    auto roundExpr0 = GetFuncExpr(funcStr, args0, IntType());

    auto data1 = new FieldExpr(1, LongType());
    std::vector<Expr *> args1;
    args1.push_back(data1);
    int32_t data1Decimal = -1;
    args1.push_back(new LiteralExpr(data1Decimal, IntType()));
    auto roundExpr1 = GetFuncExpr(funcStr, args1, LongType());
    double data1Pow = pow(10, data1Decimal);

    auto data2 = new FieldExpr(2, DoubleType());
    std::vector<Expr *> args2;
    args2.push_back(data2);
    int32_t data2Decimal = 0;
    args2.push_back(new LiteralExpr(data2Decimal, IntType()));
    auto roundExpr2 = GetFuncExpr(funcStr, args2, DoubleType());

    auto data3 = new FieldExpr(3, DoubleType());
    std::vector<Expr *> args3;
    args3.push_back(data3);
    int32_t data3Decimal = 2;
    args3.push_back(new LiteralExpr(data3Decimal, IntType()));
    auto roundExpr3 = GetFuncExpr(funcStr, args3, DoubleType());
    double data3Pow = pow(10, data3Decimal);

    auto data4 = new FieldExpr(4, DoubleType());
    std::vector<Expr *> args4;
    args4.push_back(data4);
    int32_t data4Decimal = 2;
    args4.push_back(new LiteralExpr(data4Decimal, IntType()));
    auto roundExpr4 = GetFuncExpr(funcStr, args4, DoubleType());
    double data4Pow = pow(10, data4Decimal);

    auto data5 = new FieldExpr(5, DoubleType());
    std::vector<Expr *> args5;
    args5.push_back(data5);
    int32_t data5Decimal = 2;
    args5.push_back(new LiteralExpr(data5Decimal, IntType()));
    auto roundExpr5 = GetFuncExpr(funcStr, args5, DoubleType());

    std::vector<Expr *> exprs = {roundExpr0, roundExpr1, roundExpr2, roundExpr3, roundExpr4, roundExpr5};

    const int32_t numRows = 1000;
    int32_t *col0 = MakeInts(numRows, -5);
    int64_t *col1 = MakeLongs(numRows, -5);
    double *col2 = MakeDoubles(numRows, -5.123);
    double *col3 = MakeDoubles(numRows, -5.123);
    double *col4 = MakeDoubles(numRows, -1001.456);
    auto *col5 = new double[numRows];
    double start = 1;
    int32_t idx = 0;
    for (double i = start; i < start + numRows; i++) {
        if (idx % 2 == 0) {
            col5[idx++] = std::sqrt(-i);
        } else {
            col5[idx++] = i / 0.0;
        }
    }
    const int32_t numCols = 6;

    std::vector<DataTypePtr> vecOfTypes = {IntType(),    LongType(),   DoubleType(),
                                           DoubleType(), DoubleType(), DoubleType()};
    DataTypes inputTypes(vecOfTypes);
    VectorBatch *vecBatch = CreateVectorBatch(inputTypes, numRows, col0, col1, col2, col3, col4, col5);

    std::vector<std::unique_ptr<Projection>> projections;
    auto filter = GenerateFilterAndProjections(nullptr, exprs, inputTypes, projections, nullptr);

    int32_t numSelectedRows = numRows;
    auto ret = FilterAndProject(filter, projections, numCols, vecBatch, numSelectedRows, inputTypes);

    for (int32_t i = 0; i < numRows; i++) {
        int32_t val0 = (reinterpret_cast<Vector<int32_t> *>(ret->Get(0)))->GetValue(i);
        int64_t val1 = (reinterpret_cast<Vector<int64_t> *>(ret->Get(1)))->GetValue(i);
        double val2 = (reinterpret_cast<Vector<double> *>(ret->Get(2)))->GetValue(i);
        double val3 = (reinterpret_cast<Vector<double> *>(ret->Get(3)))->GetValue(i);
        double val4 = (reinterpret_cast<Vector<double> *>(ret->Get(4)))->GetValue(i);
        double val5 = (reinterpret_cast<Vector<double> *>(ret->Get(5)))->GetValue(i);

        int32_t old0 = (reinterpret_cast<Vector<int32_t> *>(vecBatch->Get(0)))->GetValue(i);
        int64_t old1 = (reinterpret_cast<Vector<int64_t> *>(vecBatch->Get(1)))->GetValue(i);
        double old2 = (reinterpret_cast<Vector<double> *>(vecBatch->Get(2)))->GetValue(i);
        double old3 = (reinterpret_cast<Vector<double> *>(vecBatch->Get(3)))->GetValue(i);
        double old4 = (reinterpret_cast<Vector<double> *>(vecBatch->Get(4)))->GetValue(i);
        double old5 = (reinterpret_cast<Vector<double> *>(vecBatch->Get(5)))->GetValue(i);

        EXPECT_EQ(val0, round(old0));
        EXPECT_EQ(val0, old0);
        EXPECT_EQ(val1, (round(old1 * data1Pow) / data1Pow));
        EXPECT_EQ(val2, round(old2));
        EXPECT_EQ(val3, (round(old3 * data3Pow) / data3Pow));
        EXPECT_EQ(val4, -(round(-old4 * data4Pow) / data4Pow));
        if (i % 2 == 0) {
            EXPECT_TRUE(isnan(val5) && isnan(old5));
        } else {
            EXPECT_EQ(val5, old5);
        }
    }

    Expr::DeleteExprs(exprs);
    delete[] col0;
    delete[] col1;
    delete[] col2;
    delete[] col3;
    delete[] col4;
    delete[] col5;
    VectorHelper::FreeVecBatch(vecBatch);
    VectorHelper::FreeVecBatch(ret);
    ConfigUtil::SetEnableBatchExprEvaluate(false);
}

TEST(BatchCodeGenTest, MultipleColumns)
{
    ConfigUtil::SetEnableBatchExprEvaluate(true);
    FieldExpr *subLeft = new FieldExpr(0, IntType());
    LiteralExpr *subRight = new LiteralExpr(10, IntType());
    BinaryExpr *subExpr = new BinaryExpr(omniruntime::expressions::Operator::SUB, subLeft, subRight, IntType());

    FieldExpr *addLeft = new FieldExpr(2, LongType());
    LiteralExpr *addRight = new LiteralExpr(1L, LongType());
    BinaryExpr *addExpr = new BinaryExpr(omniruntime::expressions::Operator::ADD, addLeft, addRight, LongType());

    std::vector<Expr *> exprs = {subExpr, addExpr};

    const int32_t numRows = 1000;
    int32_t *col1 = MakeInts(numRows);
    int32_t *col2 = MakeInts(numRows, -100);
    int64_t *col3 = MakeLongs(numRows, -10);
    const int32_t numCols = 3;
    std::vector<DataTypePtr> vecOfTypes = {IntType(), IntType(), LongType()};
    DataTypes inputTypes(vecOfTypes);
    VectorBatch *vecBatch = CreateVectorBatch(inputTypes, numRows, col1, col2, col3);

    std::vector<std::unique_ptr<Projection>> projections;
    auto filter = GenerateFilterAndProjections(nullptr, exprs, inputTypes, projections, nullptr);

    int32_t numSelectedRows = numRows;
    auto ret = FilterAndProject(filter, projections, numCols, vecBatch, numSelectedRows, inputTypes);
    for (int32_t i = 0; i < numRows; i++) {
        int32_t val0 = (reinterpret_cast<Vector<int32_t> *>(ret->Get(0)))->GetValue(i);
        EXPECT_EQ(val0, i - 10);
        int64_t val1 = (reinterpret_cast<Vector<int64_t> *>(ret->Get(1)))->GetValue(i);
        EXPECT_EQ(val1, i - 9);
    }

    Expr::DeleteExprs(exprs);
    delete[] col1;
    delete[] col2;
    delete[] col3;
    VectorHelper::FreeVecBatch(vecBatch);
    VectorHelper::FreeVecBatch(ret);
    ConfigUtil::SetEnableBatchExprEvaluate(false);
}

TEST(BatchCodeGenTest, DictionaryVecDouble)
{
    ConfigUtil::SetEnableBatchExprEvaluate(true);
    const int32_t numCols = 1;
    const int32_t numRows = 10;
    std::vector<DataTypePtr> vecOfTypes = {DoubleType()};
    DataTypes inputTypes(vecOfTypes);

    int32_t ids[] = {3, 4, 5, 6, 7, 8, 9, 9, 9, 9};
    auto dictionary = std::make_shared<Vector<double>>(numRows);
    for (int32_t i = 0; i < numRows; i++) {
        dictionary->SetValue(i, (i % 21) - 3);
    }
    auto dicVec = VectorHelper::CreateDictionary(ids, numRows, dictionary.get());
    auto *t = new VectorBatch(numRows);
    t->Append(dicVec);

    LiteralExpr *addRight = new LiteralExpr(10.0, DoubleType());
    std::vector<Expr *> exprs = {new BinaryExpr(omniruntime::expressions::Operator::ADD, new FieldExpr(0, DoubleType()),
                                                addRight, DoubleType())};

    std::vector<std::unique_ptr<Projection>> projections;
    auto filter = GenerateFilterAndProjections(nullptr, exprs, inputTypes, projections, nullptr);

    int32_t numSelectedRows = numRows;
    auto ret = FilterAndProject(filter, projections, numCols, t, numSelectedRows, inputTypes);

    for (int32_t i = 0; i < numRows; i++) {
        double val0 = (reinterpret_cast<Vector<double> *>(ret->Get(0)))->GetValue(i);
        EXPECT_EQ(val0, (reinterpret_cast<Vector<DictionaryContainer<double>> *>(t->Get(0)))->GetValue(i) + 10);
    }

    Expr::DeleteExprs(exprs);
    VectorHelper::FreeVecBatch(t);
    VectorHelper::FreeVecBatch(ret);
    ConfigUtil::SetEnableBatchExprEvaluate(false);
}

TEST(BatchCodeGenTest, DictionaryVecDecimal128)
{
    ConfigUtil::SetEnableBatchExprEvaluate(true);
    LiteralExpr *addRight = new LiteralExpr(new std::string("20"), Decimal128Type(38, 0));
    BinaryExpr *addExpr = new BinaryExpr(omniruntime::expressions::Operator::ADD,
                                         new FieldExpr(0, Decimal128Type(38, 0)), addRight, Decimal128Type(38, 0));
    std::vector<Expr *> exprs = {addExpr};

    const int32_t numCols = 1;
    const int32_t numRows = 10;
    std::vector<DataTypePtr> vecOfTypes = {Decimal128Type()};
    DataTypes inputTypes(vecOfTypes);

    int32_t ids1[] = {3, 4, 5, 6, 7, 8, 9, 9, 9, 9};
    auto dictionary = std::make_shared<Vector<Decimal128>>(numRows);
    for (int32_t i = 0; i < numRows; i++) {
        Decimal128 decimal128(0, i);
        dictionary->SetValue(i, decimal128);
    }
    auto dicVec = VectorHelper::CreateDictionary(ids1, numRows, dictionary.get());
    auto *t = new VectorBatch(numRows);
    t->Append(dicVec);

    std::vector<std::unique_ptr<Projection>> projections;
    auto filter = GenerateFilterAndProjections(nullptr, exprs, inputTypes, projections, nullptr);

    int32_t numSelectedRows = numRows;
    auto ret = FilterAndProject(filter, projections, numCols, t, numSelectedRows, inputTypes);
    for (int32_t i = 0; i < numRows; i++) {
        Decimal128 val0 = (reinterpret_cast<Vector<Decimal128> *>(ret->Get(0)))->GetValue(i);
        Decimal128 val1 = (reinterpret_cast<Vector<DictionaryContainer<Decimal128>> *>(t->Get(0)))->GetValue(i);
        EXPECT_EQ(val0.HighBits(), val1.HighBits());
        EXPECT_EQ(val0.LowBits(), val1.LowBits() + 20);
    }

    Expr::DeleteExprs(exprs);
    VectorHelper::FreeVecBatch(t);
    VectorHelper::FreeVecBatch(ret);
    ConfigUtil::SetEnableBatchExprEvaluate(false);
}

TEST(BatchCodeGenTest, DictionaryVecDecimal64)
{
    ConfigUtil::SetEnableBatchExprEvaluate(true);

    const int32_t numCols = 1;
    const int32_t numRows = 10;

    std::vector<DataTypePtr> vecOfTypes = {Decimal64Type()};
    DataTypes inputTypes(vecOfTypes);

    int32_t ids[] = {3, 4, 5, 6, 7, 8, 9, 9, 9, 9};
    auto dictionary = std::make_shared<Vector<int64_t>>(numRows);
    for (int32_t i = 0; i < numRows; i++) {
        dictionary->SetValue(i, i);
    }
    auto dicVec = VectorHelper::CreateDictionary(ids, numRows, dictionary.get());
    auto *t = new VectorBatch(numRows);
    t->Append(dicVec);

    int32_t numSelectedRows = numRows;

    LiteralExpr *addRight = new LiteralExpr(20, Decimal64Type(8, 1));
    BinaryExpr *addExpr = new BinaryExpr(omniruntime::expressions::Operator::ADD, new FieldExpr(0, Decimal64Type(8, 0)),
                                         addRight, Decimal64Type(8, 1));
    std::vector<Expr *> exprs = {addExpr};

    std::vector<std::unique_ptr<Projection>> projections;
    auto filter = GenerateFilterAndProjections(nullptr, exprs, inputTypes, projections, nullptr);

    auto ret = FilterAndProject(filter, projections, numCols, t, numSelectedRows, inputTypes);
    for (int32_t i = 0; i < numRows; i++) {
        int64_t val0 = (reinterpret_cast<Vector<int64_t> *>(ret->Get(0)))->GetValue(i);
        EXPECT_EQ(val0 / 10, (reinterpret_cast<Vector<DictionaryContainer<int64_t>> *>(t->Get(0)))->GetValue(i));
    }

    Expr::DeleteExprs(exprs);
    VectorHelper::FreeVecBatch(t);
    VectorHelper::FreeVecBatch(ret);
    ConfigUtil::SetEnableBatchExprEvaluate(false);
}

TEST(BatchCodeGenTest, DictionaryVecBoolean)
{
    ConfigUtil::SetEnableBatchExprEvaluate(true);

    const int32_t numCols = 1;
    const int32_t numRows = 6;
    std::vector<DataTypePtr> vecOfTypes = {BooleanType()};
    DataTypes inputTypes(vecOfTypes);

    int32_t ids[] = {1, 3, 3, 4, 4, 4};
    auto dictionary = std::make_shared<Vector<bool>>(numRows);
    for (int32_t i = 0; i < numRows; i++) {
        if (i % 2 == 0) {
            dictionary->SetValue(i, false);
        } else {
            dictionary->SetValue(i, true);
        }
    }
    auto dicVec = VectorHelper::CreateDictionary(ids, numRows, dictionary.get());
    auto *t = new VectorBatch(numRows);
    t->Append(dicVec);

    LiteralExpr *andRight = new LiteralExpr(true, BooleanType());
    BinaryExpr *andExpr = new BinaryExpr(omniruntime::expressions::Operator::OR, new FieldExpr(0, BooleanType()),
                                         andRight, BooleanType());
    std::vector<Expr *> exprs = {andExpr};

    std::vector<std::unique_ptr<Projection>> projections;
    auto filter = GenerateFilterAndProjections(nullptr, exprs, inputTypes, projections, nullptr);

    int32_t numSelectedRows = numRows;
    auto ret = FilterAndProject(filter, projections, numCols, t, numSelectedRows, inputTypes);
    for (int32_t i = 0; i < numRows; i++) {
        bool val0 = (reinterpret_cast<Vector<bool> *>(ret->Get(0)))->GetValue(i);
        if (i >= 3) {
            EXPECT_FALSE(val0 && (reinterpret_cast<Vector<DictionaryContainer<bool>> *>(t->Get(0)))->GetValue(i));
        } else {
            EXPECT_TRUE(val0 && (reinterpret_cast<Vector<DictionaryContainer<bool>> *>(t->Get(0)))->GetValue(i));
        }
    }

    Expr::DeleteExprs(exprs);
    VectorHelper::FreeVecBatch(t);
    VectorHelper::FreeVecBatch(ret);
    ConfigUtil::SetEnableBatchExprEvaluate(false);
}

TEST(BatchCodeGenTest, DictionaryVarchar)
{
    ConfigUtil::SetEnableBatchExprEvaluate(true);
    BinaryExpr *filterExpr = new BinaryExpr(omniruntime::expressions::Operator::LT, new FieldExpr(0, IntType()),
                                            new LiteralExpr(6, IntType()), BooleanType());

    auto *projExpr = new FieldExpr(0, IntType());
    auto *projExpr1 = new FieldExpr(1, VarcharType());
    std::vector<Expr *> exprs = {projExpr, projExpr1};

    const int32_t numCols = 2;
    const int32_t numRows = 3;
    std::vector<DataTypePtr> vecOfTypes = {IntType(), VarcharType()};
    DataTypes inputTypes(vecOfTypes);

    auto *col1 = new int32_t[numRows];
    for (int32_t i = 0; i < numRows; i++) {
        col1[i] = i * 3;
    }

    DataTypes inputTypes1(std::vector<DataTypePtr>({IntType()}));
    auto *t = CreateVectorBatch(inputTypes1, numRows, col1);

    int32_t ids[] = {0, 1, 2};
    std::string_view strView = "test";
    auto dictionary = std::make_unique<Vector<LargeStringContainer<std::string_view>>>(numRows);
    for (int32_t i = 0; i < numRows; i++) {
        dictionary->SetValue(i, strView);
    }

    auto dicVec = VectorHelper::CreateStringDictionary(ids, numRows, dictionary.get());
    t->Append(dicVec);

    std::vector<std::unique_ptr<Projection>> projections;
    auto filter = GenerateFilterAndProjections(filterExpr, exprs, inputTypes, projections, nullptr);

    int32_t numSelectedRows = numRows;
    auto ret = FilterAndProject(filter, projections, numCols, t, numSelectedRows, inputTypes);
    EXPECT_EQ(numSelectedRows, 2);

    for (int i = 0; i < numSelectedRows; i++) {
        int32_t val0 = (reinterpret_cast<Vector<int32_t> *>(ret->Get(0)))->GetValue(i);
        EXPECT_EQ(val0, col1[i]);
        std::string_view val1 =
            (reinterpret_cast<Vector<DictionaryContainer<std::string_view>> *>(ret->Get(1)))->GetValue(i);
        EXPECT_TRUE(val1 ==
                    (reinterpret_cast<Vector<DictionaryContainer<std::string_view>> *>(t->Get(1)))->GetValue(i));
    }

    Expr::DeleteExprs({filterExpr});
    Expr::DeleteExprs(exprs);
    VectorHelper::FreeVecBatch(t);
    VectorHelper::FreeVecBatch(ret);
    delete[] col1;
    ConfigUtil::SetEnableBatchExprEvaluate(false);
}

TEST(BatchCodeGenTest, DictionaryVec)
{
    ConfigUtil::SetEnableBatchExprEvaluate(true);
    const int32_t numCols = 3;
    const int32_t numRows = 10;

    auto *col1 = new int32_t[numRows];
    auto *col2 = new int32_t[numRows];
    for (int32_t i = 0; i < numRows; i++) {
        col1[i] = i % 5;
        col2[i] = i % 11;
    }

    DataTypes inputTypes1(std::vector<DataTypePtr>({IntType(), IntType()}));
    auto *t = CreateVectorBatch(inputTypes1, numRows, col1, col2);

    int32_t ids[] = {3, 4, 5, 6, 7, 8, 9, 9, 9, 9};
    auto dictionary = std::make_shared<Vector<int32_t>>(numRows);
    for (int32_t i = 0; i < numRows; i++) {
        dictionary->SetValue(i, (i % 21) - 3);
    }
    auto dicVec = VectorHelper::CreateDictionary(ids, numRows, dictionary.get());
    t->Append(dicVec);

    std::vector<DataTypePtr> vecOfTypes({IntType(), IntType(), IntType()});
    DataTypes inputTypes2(vecOfTypes);

    BetweenExpr *filterExpr =
        new BetweenExpr(new FieldExpr(1, IntType()), new FieldExpr(0, IntType()), new FieldExpr(2, IntType()));

    auto *projExpr = new FieldExpr(0, IntType());
    auto *projExpr1 = new FieldExpr(1, IntType());
    auto *projExpr2 = new FieldExpr(2, IntType());
    std::vector<Expr *> exprs = {projExpr, projExpr1, projExpr2};

    std::vector<std::unique_ptr<Projection>> projections;
    auto filter = GenerateFilterAndProjections(filterExpr, exprs, inputTypes2, projections, nullptr);

    int32_t numSelectedRows = numRows;
    auto ret = FilterAndProject(filter, projections, numCols, t, numSelectedRows, inputTypes2);
    EXPECT_EQ(numSelectedRows, 7);

    for (int i = 0; i < numSelectedRows; i++) {
        int32_t val0 = (reinterpret_cast<Vector<int32_t> *>(ret->Get(0)))->GetValue(i);
        EXPECT_EQ(val0, col1[i]);
        int32_t val1 = (reinterpret_cast<Vector<int32_t> *>(ret->Get(1)))->GetValue(i);
        EXPECT_EQ(val1, col2[i]);
        int32_t val2 = (reinterpret_cast<Vector<DictionaryContainer<int32_t>> *>(ret->Get(2)))->GetValue(i);
        EXPECT_EQ(val2, (reinterpret_cast<Vector<DictionaryContainer<int32_t>> *>(t->Get(2)))->GetValue(i));
    }

    Expr::DeleteExprs({filterExpr});
    Expr::DeleteExprs(exprs);
    VectorHelper::FreeVecBatch(t);
    VectorHelper::FreeVecBatch(ret);
    delete[] col1;
    delete[] col2;
    ConfigUtil::SetEnableBatchExprEvaluate(false);
}

TEST(BatchCodeGenTest, AddDecimalReturnNull)
{
    ConfigUtil::SetEnableBatchExprEvaluate(true);
    FieldExpr *right1 = new FieldExpr(0, Decimal128Type(38, 0));
    FieldExpr *left1 = new FieldExpr(0, Decimal128Type(38, 0));
    BinaryExpr *addExpr1 =
        new BinaryExpr(omniruntime::expressions::Operator::ADD, right1, left1, Decimal128Type(38, 0));
    FieldExpr *right2 = new FieldExpr(1, Decimal64Type(18, 0));
    FieldExpr *left2 = new FieldExpr(1, Decimal64Type(18, 0));
    BinaryExpr *addExpr2 =
        new BinaryExpr(omniruntime::expressions::Operator::ADD, right2, left2, Decimal128Type(38, 0));
    FieldExpr *right3 = new FieldExpr(1, Decimal64Type(18, 0));
    FieldExpr *left3 = new FieldExpr(0, Decimal128Type(38, 0));
    BinaryExpr *addExpr3 =
        new BinaryExpr(omniruntime::expressions::Operator::ADD, right3, left3, Decimal128Type(38, 0));
    FieldExpr *right4 = new FieldExpr(0, Decimal128Type(38, 0));
    FieldExpr *left4 = new FieldExpr(1, Decimal64Type(18, 0));
    BinaryExpr *addExpr4 =
        new BinaryExpr(omniruntime::expressions::Operator::ADD, right4, left4, Decimal128Type(38, 0));
    std::vector<Expr *> exprs = {addExpr1, addExpr2, addExpr3, addExpr4};

    const int32_t numRows = 1;
    const int32_t numCols = 2;
    int64_t *col1 = new int64_t[2]{-1, INT64_MAX};
    int64_t *col2 = new int64_t[1]{INT64_MAX};
    std::vector<DataTypePtr> vecOfTypes = {Decimal128Type(), LongType()};
    DataTypes inputTypes(vecOfTypes);

    VectorBatch *vecBatch = CreateVectorBatch(inputTypes, numRows, col1, col2);

    auto overflowConfig = new OverflowConfig(omniruntime::op::OVERFLOW_CONFIG_NULL);
    std::vector<std::unique_ptr<Projection>> projections;
    auto filter = GenerateFilterAndProjections(nullptr, exprs, inputTypes, projections, overflowConfig);

    int32_t numSelectedRows = numRows;
    auto ret = FilterAndProject(filter, projections, numCols, vecBatch, numSelectedRows, inputTypes);
    for (int32_t i = 0; i < numRows; i++) {
        bool isNull = (ret->Get(0))->IsNull(i);
        EXPECT_TRUE(isNull);
        isNull = (ret->Get(1))->IsNull(i);
        EXPECT_FALSE(isNull);
        isNull = (ret->Get(2))->IsNull(i);
        EXPECT_TRUE(isNull);
        isNull = (ret->Get(3))->IsNull(i);
        EXPECT_TRUE(isNull);
    }

    Expr::DeleteExprs(exprs);
    delete overflowConfig;
    delete[] col1;
    delete[] col2;
    VectorHelper::FreeVecBatch(vecBatch);
    VectorHelper::FreeVecBatch(ret);
    ConfigUtil::SetEnableBatchExprEvaluate(false);
}

TEST(BatchCodeGenTest, IsNullExpr)
{
    ConfigUtil::SetEnableBatchExprEvaluate(true);
    FieldExpr *expr = new FieldExpr(0, IntType());
    IsNullExpr *isNullExpr = new IsNullExpr(expr);

    std::vector<Expr *> exprs = {isNullExpr};

    const int32_t numRows = 4;
    int32_t col1[numRows] = {1, 2, 3, 4};
    const int32_t numCols = 1;
    std::vector<DataTypePtr> vecOfTypes = {IntType()};
    DataTypes inputTypes(vecOfTypes);
    VectorBatch *vecBatch = CreateVectorBatch(inputTypes, numRows, col1);

    for (int32_t i = 0; i < numRows; i++) {
        vecBatch->Get(0)->SetNull(i);
    }

    std::vector<std::unique_ptr<Projection>> projections;
    auto filter = GenerateFilterAndProjections(nullptr, exprs, inputTypes, projections, nullptr);

    int32_t numSelectedRows = numRows;
    auto ret = FilterAndProject(filter, projections, numCols, vecBatch, numSelectedRows, inputTypes);

    for (int32_t i = 0; i < numRows; i++) {
        bool isValNull = (reinterpret_cast<Vector<bool> *>(ret->Get(0)))->IsNull(i);
        EXPECT_FALSE(isValNull);
    }
    Expr::DeleteExprs(exprs);
    VectorHelper::FreeVecBatch(vecBatch);
    VectorHelper::FreeVecBatch(ret);
    ConfigUtil::SetEnableBatchExprEvaluate(false);
}

TEST(BatchCodeGenTest, UnaryExpr)
{
    ConfigUtil::SetEnableBatchExprEvaluate(true);
    FieldExpr *expr = new FieldExpr(0, IntType());
    IsNullExpr *isNullExpr = new IsNullExpr(expr);
    auto notExpr = new UnaryExpr(omniruntime::expressions::Operator::NOT, isNullExpr, BooleanType());
    std::vector<Expr *> exprs = {notExpr};

    const int32_t numRows = 4;
    int32_t col1[numRows] = {1, 2, 3, 4};
    const int32_t numCols = 1;
    std::vector<DataTypePtr> vecOfTypes = {IntType()};
    DataTypes inputTypes(vecOfTypes);
    VectorBatch *vecBatch = CreateVectorBatch(inputTypes, numRows, col1);

    for (int32_t i = 0; i < numRows; i++) {
        vecBatch->Get(0)->SetNull(i);
    }

    std::vector<std::unique_ptr<Projection>> projections;
    auto filter = GenerateFilterAndProjections(nullptr, exprs, inputTypes, projections, nullptr);

    int32_t numSelectedRows = numRows;
    auto ret = FilterAndProject(filter, projections, numCols, vecBatch, numSelectedRows, inputTypes);
    for (int32_t i = 0; i < numRows; i++) {
        bool val = (reinterpret_cast<Vector<bool> *>(ret->Get(0)))->GetValue(i);
        EXPECT_FALSE(val);
    }

    Expr::DeleteExprs(exprs);
    VectorHelper::FreeVecBatch(vecBatch);
    VectorHelper::FreeVecBatch(ret);
    ConfigUtil::SetEnableBatchExprEvaluate(false);
}
