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
#include <chrono>
#include "gtest/gtest.h"
#include "operator/projection/projection.h"
#include "codegen/bloom_filter.h"
#include "util/test_util.h"
#include "util/config_util.h"

using namespace omniruntime::op;
using namespace omniruntime::vec;
using namespace omniruntime::expressions;
using namespace std;
using namespace TestUtil;

namespace ProjectionTest {
TEST(ProjectionTest, Cast)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 10;
    int64_t *col1 = MakeLongs(numRows);
    int32_t *col2 = MakeInts(numRows);
    std::vector<DataTypePtr> vecOfTypes = {LongType(), IntType()};
    DataTypes inputTypes(vecOfTypes);
    auto data1 = new FieldExpr(0, LongType());
    std::string castStr = "CAST";
    std::vector<Expr *> args1;
    args1.push_back(data1);
    auto castExpr1 = GetFuncExpr(castStr, args1, IntType());

    auto data2 = new FieldExpr(1, IntType());
    std::vector<Expr *> args2;
    args2.push_back(data2);
    auto castExpr2 = GetFuncExpr(castStr, args2, LongType());

    std::vector<Expr *> exprs = {castExpr1, castExpr2};

    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col1, col2);

    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);
    for (int32_t i = 0; i < numRows; i++) {
        int32_t val0 = (reinterpret_cast<Vector<int32_t> *>(outputVecBatch->Get(0)))->GetValue(i);
        int64_t val1 = (reinterpret_cast<Vector<int64_t> *>(outputVecBatch->Get(1)))->GetValue(i);
        EXPECT_EQ(val0, i);
        EXPECT_EQ(val1, i);
    }

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] col1;
    delete[] col2;
    omniruntime::op::Operator::DeleteOperator(op);

    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, CastDouble)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 1000;
    double *col1 = MakeDoubles(numRows);
    double *col2 = MakeDoubles(numRows);
    std::vector<DataTypePtr> vecOfTypes = {DoubleType(), DoubleType()};

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

    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col1, col2);

    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);
    for (int32_t i = 0; i < numRows; i++) {
        int32_t val0 = (reinterpret_cast<Vector<int32_t> *>(outputVecBatch->Get(0)))->GetValue(i);
        int64_t val1 = (reinterpret_cast<Vector<int64_t> *>(outputVecBatch->Get(1)))->GetValue(i);
        EXPECT_EQ(val0, i);
        EXPECT_EQ(val1, i);
    }

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] col1;
    delete[] col2;
    omniruntime::op::Operator::DeleteOperator(op);

    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, CastInt64ToDecimal128)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 1000;
    int64_t *col1 = MakeLongs(numRows);
    std::vector<DataTypePtr> vecOfTypes = {LongType()};
    auto data1 = new FieldExpr(0, LongType());
    std::string castStr = "CAST";
    std::vector<Expr *> args1;
    args1.push_back(data1);
    auto castExpr = GetFuncExpr(castStr, args1, Decimal128Type(38, 0));
    std::vector<Expr *> exprs = {castExpr};

    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col1);

    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);
    for (int32_t i = 0; i < numRows; i++) {
        Decimal128 val0 = (reinterpret_cast<Vector<Decimal128> *>(outputVecBatch->Get(0)))->GetValue(i);
        EXPECT_EQ(val0.HighBits(), 0);
        EXPECT_EQ(val0.LowBits(), i);
    }

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] col1;
    omniruntime::op::Operator::DeleteOperator(op);

    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, MakeDecimal64ToDiffScale)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 1000;
    int64_t *col1 = MakeLongs(numRows);
    std::vector<DataTypePtr> vecOfTypes = {Decimal64Type(7, 2)};
    auto data1 = new FieldExpr(0, Decimal64Type(7, 2));
    auto data2 = new FieldExpr(0, Decimal64Type(7, 2));

    std::string castStr = "CAST";
    auto makeExpr1 = GetFuncExpr(castStr, {data1}, Decimal64Type(7, 4));
    auto makeExpr2 = GetFuncExpr(castStr, {data2}, Decimal64Type(7, 0));
    std::vector<Expr *> exprs = {makeExpr1, makeExpr2};

    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col1);

    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);
    for (int32_t i = 0; i < numRows; i++) {
        long val0 = (reinterpret_cast<Vector<int64_t> *>(outputVecBatch->Get(0)))->GetValue(i);
        long val1 = (reinterpret_cast<Vector<int64_t> *>(outputVecBatch->Get(1)))->GetValue(i);
        EXPECT_EQ(val0, i * 100);
        if (i % 100 >= 50) {
            i = i + 50;
        }
        EXPECT_EQ(val1, i / 100);
    }

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] col1;
    omniruntime::op::Operator::DeleteOperator(op);

    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, MakeDecimal128ToDiffScale)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 1000;
    int64_t *col1 = MakeDecimals(numRows);
    std::vector<DataTypePtr> vecOfTypes = {Decimal128Type(38, 2)};
    auto data1 = new FieldExpr(0, Decimal128Type(38, 2));
    auto data2 = new FieldExpr(0, Decimal128Type(7, 2));

    std::string castStr = "CAST";
    auto makeExpr1 = GetFuncExpr(castStr, {data1}, Decimal128Type(38, 4));
    auto makeExpr2 = GetFuncExpr(castStr, {data2}, Decimal128Type(38, 0));
    std::vector<Expr *> exprs = {makeExpr1, makeExpr2};

    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col1);

    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);
    for (int32_t i = 0; i < numRows; i++) {
        Decimal128 val0 = (reinterpret_cast<Vector<Decimal128> *>(outputVecBatch->Get(0)))->GetValue(i);
        Decimal128 val1 = (reinterpret_cast<Vector<Decimal128> *>(outputVecBatch->Get(1)))->GetValue(i);
        EXPECT_EQ(val0.HighBits(), 0);
        EXPECT_EQ(val1.HighBits(), 0);
        EXPECT_EQ(val0.LowBits(), i * 100);
        EXPECT_EQ(val1.LowBits(), round((double)i / 100));
    }

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] col1;
    omniruntime::op::Operator::DeleteOperator(op);

    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, MakeDecimal64To128WithDiffScale)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 1000;
    int64_t *col1 = MakeLongs(numRows);
    std::vector<DataTypePtr> vecOfTypes = {Decimal64Type(7, 2)};
    auto data1 = new FieldExpr(0, Decimal64Type(7, 2));
    auto data2 = new FieldExpr(0, Decimal64Type(7, 2));
    auto data3 = new FieldExpr(0, Decimal64Type(7, 2));
    std::string castStr = "CAST";
    auto makeExpr1 = GetFuncExpr(castStr, {data1}, Decimal128Type(38, 2));
    auto makeExpr2 = GetFuncExpr(castStr, {data2}, Decimal128Type(38, 4));
    auto makeExpr3 = GetFuncExpr(castStr, {data3}, Decimal128Type(38, 0));
    std::vector<Expr *> exprs = {makeExpr1, makeExpr2, makeExpr3};

    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col1);

    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);
    for (int32_t i = 0; i < numRows; i++) {
        Decimal128 val0 = (reinterpret_cast<Vector<Decimal128> *>(outputVecBatch->Get(0)))->GetValue(i);
        Decimal128 val1 = (reinterpret_cast<Vector<Decimal128> *>(outputVecBatch->Get(1)))->GetValue(i);
        Decimal128 val2 = (reinterpret_cast<Vector<Decimal128> *>(outputVecBatch->Get(2)))->GetValue(i);
        EXPECT_EQ(val0.HighBits(), 0);
        EXPECT_EQ(val1.HighBits(), 0);
        EXPECT_EQ(val2.HighBits(), 0);
        EXPECT_EQ(val0.LowBits(), i);
        EXPECT_EQ(val1.LowBits(), i * 100);
        EXPECT_EQ(val2.LowBits(), round((double)i / 100));
    }

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] col1;
    omniruntime::op::Operator::DeleteOperator(op);

    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, Simple)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 1000;
    int32_t *col = MakeInts(numRows);
    auto *addLeft = new FieldExpr(0, IntType());
    auto *addRight = new LiteralExpr(5, IntType());
    auto *addExpr = new BinaryExpr(omniruntime::expressions::Operator::ADD, addLeft, addRight, IntType());
    std::vector<Expr *> exprs = {addExpr};
    std::vector<DataTypePtr> vecOfTypes = {IntType()};
    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col);

    for (int32_t i = 0; i < numRows; i++) {
        if (i % 2 != 0) {
            t->Get(0)->SetNull(i);
        }
    }

    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);
    auto vec = (reinterpret_cast<Vector<int32_t> *>(outputVecBatch->Get(0)));
    for (int32_t i = 0; i < numRows; i++) {
        int32_t val = vec->GetValue(i);
        bool isNull = vec->IsNull(i);
        if (i % 2 == 0) {
            EXPECT_EQ(val, i + 5);
            EXPECT_FALSE(isNull);
        } else {
            EXPECT_TRUE(isNull);
        }
    }

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] col;
    omniruntime::op::Operator::DeleteOperator(op);

    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, AbsWithNullValues)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 1000;
    int32_t *col1 = MakeInts(numRows, -5);
    int64_t *col2 = MakeLongs(numRows, -5);
    int64_t *col3 = MakeLongs(numRows, -5);
    std::vector<DataTypePtr> vecOfTypes = {IntType(), LongType(), Decimal64Type()};
    auto data1 = new FieldExpr(0, IntType());
    std::string funcStr = "abs";
    std::vector<Expr *> args1;
    args1.push_back(data1);
    auto absExpr1 = GetFuncExpr(funcStr, args1, IntType());

    auto data2 = new FieldExpr(1, LongType());
    std::vector<Expr *> args2;
    args2.push_back(data2);
    auto absExpr2 = GetFuncExpr(funcStr, args2, LongType());

    auto data3 = new FieldExpr(2, Decimal64Type(7, 2));
    std::vector<Expr *> args3;
    args3.push_back(data3);
    auto absExpr3 = GetFuncExpr(funcStr, args3, Decimal64Type(7, 2));

    std::vector<Expr *> exprs = {absExpr1, absExpr2, absExpr3};

    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col1, col2, col3);
    for (int i = 0; i < numRows; i++) {
        if (i % 2 == 0) {
            t->Get(0)->SetNull(i);
            t->Get(1)->SetNull(i);
            t->Get(2)->SetNull(i);
        }
    }

    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);
    for (int32_t i = 0; i < numRows; i++) {
        if (i % 2 == 0) {
            EXPECT_TRUE(outputVecBatch->Get(0)->IsNull(i));
            EXPECT_TRUE(outputVecBatch->Get(1)->IsNull(i));
            EXPECT_TRUE(outputVecBatch->Get(2)->IsNull(i));
        } else {
            EXPECT_FALSE(outputVecBatch->Get(0)->IsNull(i));
            EXPECT_FALSE(outputVecBatch->Get(1)->IsNull(i));
            EXPECT_FALSE(outputVecBatch->Get(2)->IsNull(i));
            int32_t val0 = (reinterpret_cast<Vector<int32_t> *>(outputVecBatch->Get(0)))->GetValue(i);
            int64_t val1 = (reinterpret_cast<Vector<int64_t> *>(outputVecBatch->Get(1)))->GetValue(i);
            int64_t val2 = (reinterpret_cast<Vector<int64_t> *>(outputVecBatch->Get(2)))->GetValue(i);
            EXPECT_EQ(val0, abs(i - 5));
            EXPECT_EQ(val1, abs(i - 5));
            EXPECT_EQ(val2, abs(i - 5));
        }
    }

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] col1;
    delete[] col2;
    delete[] col3;
    omniruntime::op::Operator::DeleteOperator(op);

    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, Negatives)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 1000;
    int32_t *col = MakeInts(numRows, -5);
    auto *subLeft = new FieldExpr(0, IntType());
    auto *subRight = new LiteralExpr(500, IntType());
    auto *subExpr = new BinaryExpr(omniruntime::expressions::Operator::SUB, subLeft, subRight, IntType());
    std::vector<Expr *> exprs = {subExpr};
    std::vector<DataTypePtr> vecOfTypes = {IntType()};
    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col);

    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);
    for (int32_t i = 0; i < numRows; i++) {
        int32_t val0 = (reinterpret_cast<Vector<int32_t> *>(outputVecBatch->Get(0)))->GetValue(i);
        EXPECT_EQ(val0, i - 505);
    }

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] col;
    omniruntime::op::Operator::DeleteOperator(op);

    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, Longs)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 10000;
    int64_t *col = MakeLongs(numRows, -5000);
    auto *mulLeft = new FieldExpr(0, LongType());
    auto *mulRight = new LiteralExpr(5000000L, LongType());
    auto *mulExpr = new BinaryExpr(omniruntime::expressions::Operator::MUL, mulLeft, mulRight, LongType());
    std::vector<Expr *> exprs = {mulExpr};
    std::vector<DataTypePtr> vecOfTypes = {LongType()};
    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col);

    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);
    for (int32_t i = 0; i < numRows; i++) {
        int64_t val0 = (reinterpret_cast<Vector<int64_t> *>(outputVecBatch->Get(0)))->GetValue(i);
        EXPECT_EQ(val0, static_cast<int64_t>(i - 5000) * 5000000);
    }

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] col;
    omniruntime::op::Operator::DeleteOperator(op);

    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, Doubles)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 10000;
    double *col = MakeDoubles(numRows, -5000.5);

    auto *divLeft = new FieldExpr(0, DoubleType());
    auto *divRight = new LiteralExpr(2.0, DoubleType());
    auto *divExpr = new BinaryExpr(omniruntime::expressions::Operator::DIV, divLeft, divRight, DoubleType());
    std::vector<Expr *> exprs = {divExpr};
    std::vector<DataTypePtr> vecOfTypes = {DoubleType()};
    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col);

    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);
    for (int32_t i = 0; i < numRows; i++) {
        double val0 = (reinterpret_cast<Vector<double> *>(outputVecBatch->Get(0)))->GetValue(i);
        double expected = (i - 5000.5) / 2;
        EXPECT_TRUE(val0 > expected - 0.1 && val0 < expected + 0.1);
    }

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] col;
    omniruntime::op::Operator::DeleteOperator(op);

    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, Doubles_DivideByZero)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    int32_t numRows = 5;
    double *col1 = MakeDoubles(numRows, -4.0);
    double *col2 = MakeDoubles(numRows, -3.0);

    auto *divLeft = new FieldExpr(0, DoubleType());
    auto *divRight = new FieldExpr(1, DoubleType());
    auto *divExpr = new BinaryExpr(omniruntime::expressions::Operator::DIV, divLeft, divRight, DoubleType());
    std::vector<Expr *> exprs = {divExpr};
    std::vector<DataTypePtr> vecOfTypes = {DoubleType(), DoubleType()};
    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col1, col2);
    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);
    EXPECT_TRUE(outputVecBatch->Get(0)->IsNull(3));

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] col1;
    delete[] col2;
    delete op;
    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, Bytes_DivideByZero)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    int32_t numRows = 5;
    int8_t *col1 = MakeBytes(numRows, -4);
    int8_t *col2 = MakeBytes(numRows, -3);

    col2[3] = 0;

    auto *divLeft = new FieldExpr(0, ByteType());
    auto *divRight = new FieldExpr(1, ByteType());
    auto *divExpr = new BinaryExpr(omniruntime::expressions::Operator::DIV, divLeft, divRight, ByteType());
    std::vector<Expr *> exprs = {divExpr};
    std::vector<DataTypePtr> vecOfTypes = {ByteType(), ByteType()};
    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col1, col2);
    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);
    EXPECT_TRUE(outputVecBatch->Get(0)->IsNull(3));

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] col1;
    delete[] col2;
    delete op;
    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, Shorts_DivideByZero)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    int32_t numRows = 5;
    short *col1 = MakeShorts(numRows, -4);
    short *col2 = MakeShorts(numRows, -3);

    col2[3] = 0;

    auto *divLeft = new FieldExpr(0, ShortType());
    auto *divRight = new FieldExpr(1, ShortType());
    auto *divExpr = new BinaryExpr(omniruntime::expressions::Operator::DIV, divLeft, divRight, ShortType());
    std::vector<Expr *> exprs = {divExpr};
    std::vector<DataTypePtr> vecOfTypes = {ShortType(), ShortType()};
    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col1, col2);
    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);
    EXPECT_TRUE(outputVecBatch->Get(0)->IsNull(3));

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] col1;
    delete[] col2;
    delete op;
    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, testModShort)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 1;
    auto left = new LiteralExpr(static_cast<int16_t>(12250), ShortType());
    auto right = new LiteralExpr(static_cast<int16_t>(125), ShortType());

    auto *modExpr = new BinaryExpr(omniruntime::expressions::Operator::MOD, left, right, ShortType());

    std::vector<Expr *> exprs = {modExpr};
    std::vector<DataTypePtr> vecOfTypes = {};
    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();

    VectorBatch *t = CreateVectorBatch(inputTypes, numRows);
    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);
    int16_t val0 = (reinterpret_cast<Vector<int16_t> *>(outputVecBatch->Get(0)))->GetValue(0);
    EXPECT_EQ(val0, 0);

    VectorHelper::FreeVecBatch(outputVecBatch);
    omniruntime::op::Operator::DeleteOperator(op);

    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, testModByte)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 1;
    auto left = new LiteralExpr(static_cast<int8_t>(100), ByteType());
    auto right = new LiteralExpr(static_cast<int8_t>(5), ByteType());

    auto *modExpr = new BinaryExpr(omniruntime::expressions::Operator::MOD, left, right, ByteType());

    std::vector<Expr *> exprs = {modExpr};
    std::vector<DataTypePtr> vecOfTypes = {};
    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();

    VectorBatch *t = CreateVectorBatch(inputTypes, numRows);
    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);
    int8_t val0 = (reinterpret_cast<Vector<int8_t> *>(outputVecBatch->Get(0)))->GetValue(0);
    EXPECT_EQ(val0, 0);

    VectorHelper::FreeVecBatch(outputVecBatch);
    omniruntime::op::Operator::DeleteOperator(op);

    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, testModDoubles)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 10000;
    double *col1 = MakeDoubles(numRows, -5000.5);
    double *col2 = MakeDoubles(numRows, -124.45);

    auto *modLeft = new FieldExpr(0, DoubleType());
    auto *modRight = new FieldExpr(1, DoubleType());
    auto *modExpr = new BinaryExpr(omniruntime::expressions::Operator::MOD, modLeft, modRight, DoubleType());
    std::vector<Expr *> exprs = {modExpr};
    std::vector<DataTypePtr> vecOfTypes = {DoubleType(), DoubleType()};
    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col1, col2);
    t->Get(0)->SetNull(5);
    t->Get(0)->SetNull(8000);
    t->Get(1)->SetNull(2456);
    t->Get(1)->SetNull(8000);
    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);
    for (int32_t i = 0; i < numRows; i++) {
        if (i == 5 || i == 2456 || i == 8000) {
            EXPECT_TRUE(outputVecBatch->Get(0)->IsNull(i));
        } else {
            double val0 = (reinterpret_cast<Vector<double> *>(outputVecBatch->Get(0)))->GetValue(i);
            double expected = std::fmod((i - 5000.5), (i - 124.45));
            EXPECT_DOUBLE_EQ(val0, expected);
        }
    }

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] col1;
    delete[] col2;
    delete op;
    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, testModDoubles2)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 10000;
    double *col = MakeDoubles(numRows, -1273.37);

    auto *modLeft = new FieldExpr(0, DoubleType());
    auto *modRight = new LiteralExpr(-45.8, DoubleType());
    auto *modExpr = new BinaryExpr(omniruntime::expressions::Operator::MOD, modLeft, modRight, DoubleType());
    std::vector<Expr *> exprs = {modExpr};
    std::vector<DataTypePtr> vecOfTypes = {DoubleType()};
    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col);
    t->Get(0)->SetNull(0);
    t->Get(0)->SetNull(9999);
    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);
    for (int32_t i = 0; i < numRows; i++) {
        if (i == 0 || i == 9999) {
            EXPECT_TRUE(outputVecBatch->Get(0)->IsNull(i));
        } else {
            double val0 = (reinterpret_cast<Vector<double> *>(outputVecBatch->Get(0)))->GetValue(i);
            double expected = std::fmod((i - 1273.37), -45.8);
            EXPECT_DOUBLE_EQ(val0, expected);
        }
    }

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] col;
    delete op;
    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, DoublesModulusByZero)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    int32_t numRows = 10;
    double *col1 = MakeDoubles(numRows, -4.0);

    auto *divLeft = new FieldExpr(0, DoubleType());
    auto *divRight = new LiteralExpr(0, DoubleType());
    auto *divExpr = new BinaryExpr(omniruntime::expressions::Operator::MOD, divLeft, divRight, DoubleType());
    std::vector<Expr *> exprs = {divExpr};
    std::vector<DataTypePtr> vecOfTypes = {DoubleType()};
    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col1);
    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);
    auto *res = (reinterpret_cast<Vector<double> *>(outputVecBatch->Get(0)));
    for (int32_t i = 0; i < numRows; i++) {
        EXPECT_TRUE(res->IsNull(i));
    }

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] col1;
    delete op;
    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, MultipleColumns)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 1000;
    int32_t *col1 = MakeInts(numRows);
    int32_t *col2 = MakeInts(numRows, -100);
    int64_t *col3 = MakeLongs(numRows, -10);
    auto *subLeft = new FieldExpr(0, IntType());
    auto *subRight = new LiteralExpr(10, IntType());
    auto *subExpr = new BinaryExpr(omniruntime::expressions::Operator::SUB, subLeft, subRight, IntType());

    auto *addLeft = new FieldExpr(2, LongType());
    auto *addRight = new LiteralExpr(1L, LongType());
    auto *addExpr = new BinaryExpr(omniruntime::expressions::Operator::ADD, addLeft, addRight, LongType());

    std::vector<Expr *> exprs = {subExpr, addExpr};

    std::vector<DataTypePtr> vecOfTypes = {IntType(), IntType(), LongType()};
    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col1, col2, col3);

    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);
    for (int32_t i = 0; i < numRows; i++) {
        int32_t val0 = (reinterpret_cast<Vector<int32_t> *>(outputVecBatch->Get(0)))->GetValue(i);
        EXPECT_EQ(val0, i - 10);
        int64_t val1 = (reinterpret_cast<Vector<int64_t> *>(outputVecBatch->Get(1)))->GetValue(i);
        EXPECT_EQ(val1, i - 9);
    }

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] col1;
    delete[] col2;
    delete[] col3;
    omniruntime::op::Operator::DeleteOperator(op);

    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, BenchmarkMultipleColumns)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 1000;
    int32_t *col1 = MakeInts(numRows);
    int32_t *col2 = MakeInts(numRows, -100);
    int64_t *col3 = MakeLongs(numRows, -10);
    auto *col4 = new string[numRows];
    for (int32_t i = 0; i < numRows; i++) {
        if (i % 2 == 0) {
            col4[i] = "hello";
        } else if (i % 3 == 0) {
            col4[i] = "world";
        } else {
            col4[i] = "!!!!!";
        }
    }

    auto *subLeft = new FieldExpr(0, IntType());
    auto *subRight = new LiteralExpr(10, IntType());
    auto *subExpr = new BinaryExpr(omniruntime::expressions::Operator::SUB, subLeft, subRight, IntType());

    auto *addLeft = new FieldExpr(2, LongType());
    auto *addRight = new LiteralExpr(1L, LongType());
    auto *addExpr = new BinaryExpr(omniruntime::expressions::Operator::ADD, addLeft, addRight, LongType());

    std::vector<Expr *> exprs = {subExpr, addExpr};
    std::vector<DataTypePtr> vecOfTypes = {IntType(), IntType(), LongType(), VarcharType(1000)};
    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col1, col2, col3, col4);

    std::cout << "[BenchmarkMultipleColumns Project without varchar]\n\n";
    for (int i = 0; i < 10; i++) {
        auto start = std::chrono::system_clock::now();
        auto copy = DuplicateVectorBatch(t);
        op->AddInput(copy);
        VectorBatch *outputVecBatch = nullptr;
        op->GetOutput(&outputVecBatch);
        VectorHelper::FreeVecBatch(outputVecBatch);

        auto end = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        std::cout << "BenchmarkMultipleColumns round " << i << " elapsed " << elapsed.count() << " ms\n";
    }

    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;

    std::cout << "\n\n\n[BenchmarkMultipleColumns Project with varchar]\n\n";
    auto *subLeft2 = new FieldExpr(0, IntType());
    auto *subRight2 = new LiteralExpr(10, IntType());
    auto *subExpr2 = new BinaryExpr(omniruntime::expressions::Operator::SUB, subLeft2, subRight2, IntType());

    auto *substData = new FieldExpr(3, VarcharType());
    auto *substrIndex = new LiteralExpr(1, IntType());
    auto *substrLen = new LiteralExpr(3, IntType());
    std::string funcStr = "substr";
    DataTypePtr retType = VarcharType();
    std::vector<Expr *> args;
    args.push_back(substData);
    args.push_back(substrIndex);
    args.push_back(substrLen);
    auto substrExpr = GetFuncExpr(funcStr, args, VarcharType());
    std::vector<Expr *> exprs2 = {subExpr2, substrExpr};

    auto exprEvaluator2 = std::make_shared<ExpressionEvaluator>(exprs2, inputTypes, overflowConfig);
    factory = new ProjectionOperatorFactory(move(exprEvaluator2));
    op = factory->CreateOperator();

    for (int i = 0; i < 10; i++) {
        auto start = std::chrono::system_clock::now();
        auto copy = DuplicateVectorBatch(t);
        op->AddInput(copy);
        VectorBatch *outputVecBatch = nullptr;
        op->GetOutput(&outputVecBatch);
        VectorHelper::FreeVecBatch(outputVecBatch);

        auto end = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        std::cout << "BenchmarkMultipleColumns round " << i << " elapsed " << elapsed.count() << " ms\n";
    }

    VectorHelper::FreeVecBatch(t);
    delete[] col1;
    delete[] col2;
    delete[] col3;
    delete[] col4;
    omniruntime::op::Operator::DeleteOperator(op);

    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, DependOtherColumn)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 1000;
    int32_t *col1 = MakeInts(numRows);
    int32_t *col2 = MakeInts(numRows, -100);
    int64_t *col3 = MakeLongs(numRows, 0);
    auto *mulLeft = new FieldExpr(0, IntType());
    auto *mulRight = new FieldExpr(1, IntType());
    auto *mulExpr = new BinaryExpr(omniruntime::expressions::Operator::MUL, mulLeft, mulRight, IntType());

    auto *conditionLeft = new FieldExpr(0, IntType());
    auto *conditionRight = new LiteralExpr(500, IntType());
    auto *condition =
        new BinaryExpr(omniruntime::expressions::Operator::LT, conditionLeft, conditionRight, BooleanType());

    auto *texp = new LiteralExpr(4000000000L, LongType());
    auto *fexp = new FieldExpr(2, LongType());
    auto *ifExpr = new IfExpr(condition, texp, fexp);

    std::vector<Expr *> exprs = {mulExpr, ifExpr};
    std::vector<DataTypePtr> vecOfTypes = {IntType(), IntType(), LongType()};
    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col1, col2, col3);

    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);
    for (int32_t i = 0; i < numRows; i++) {
        int32_t val0 = (reinterpret_cast<Vector<int32_t> *>(outputVecBatch->Get(0)))->GetValue(i);
        EXPECT_EQ(val0, i * (i - 100));
        int64_t val1 = (reinterpret_cast<Vector<int64_t> *>(outputVecBatch->Get(1)))->GetValue(i);
        EXPECT_EQ(val1, (int64_t)(i < 500 ? 4000000000 : i));
    }

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] col1;
    delete[] col2;
    delete[] col3;
    omniruntime::op::Operator::DeleteOperator(op);

    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, ProjectString1)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    std::vector<DataTypePtr> vecOfTypes = {VarcharType()};
    DataTypes inputTypes(vecOfTypes);
    const int32_t numRows = 100;
    auto cols = new std::string[numRows];
    for (int32_t i = 0; i < numRows; i++) {
        if (i % 40 == 0) {
            cols[i] = "hello";
        } else {
            cols[i] = "abcdefghijklmnopqrstuvwxyz";
        }
    }
    auto *t = CreateVectorBatch(inputTypes, numRows, cols);

    auto *substrData = new FieldExpr(0, VarcharType());
    auto *substrIndex = new LiteralExpr(1, IntType());
    auto *substrLen = new LiteralExpr(3, IntType());
    std::string funcStr = "substr";
    DataTypePtr retType = VarcharType();
    std::vector<Expr *> args;
    args.push_back(substrData);
    args.push_back(substrIndex);
    args.push_back(substrLen);
    auto substrExpr = GetFuncExpr(funcStr, args, VarcharType());
    auto *col0 = new FieldExpr(0, VarcharType());
    std::vector<Expr *> exprs = {substrExpr, col0};
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();

    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);

    auto *vcVec = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(outputVecBatch->Get(0));
    for (int32_t i = 0; i < numRows; i++) {
        if (i % 40 == 0) {
            EXPECT_TRUE(vcVec->GetValue(i) == "hel"); // codespell:ignore hel
        } else {
            EXPECT_TRUE(vcVec->GetValue(i) == "abc");
        }
    }

    VectorHelper::FreeVecBatch(outputVecBatch);
    omniruntime::op::Operator::DeleteOperator(op);
    delete[] cols;
    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, DictionaryVecTest)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
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

    auto *addLeft1 = new FieldExpr(0, IntType());
    auto *addRight1 = new LiteralExpr(1, IntType());
    auto *addExpr1 = new BinaryExpr(omniruntime::expressions::Operator::ADD, addLeft1, addRight1, IntType());

    auto *addLeft2 = new FieldExpr(1, IntType());
    auto *addRight2 = new LiteralExpr(2, IntType());
    auto *addExpr2 = new BinaryExpr(omniruntime::expressions::Operator::ADD, addLeft2, addRight2, IntType());

    auto *addLeft3 = new FieldExpr(2, IntType());
    auto *addRight3 = new LiteralExpr(10, IntType());
    auto *addExpr3 = new BinaryExpr(omniruntime::expressions::Operator::ADD, addLeft3, addRight3, IntType());

    std::vector<Expr *> exprs = {addExpr1, addExpr2, addExpr3};
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes2, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();

    auto copy = DuplicateVectorBatch(t);
    op->AddInput(copy);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);
    for (int32_t i = 0; i < numRows; i++) {
        int32_t val0 = (reinterpret_cast<Vector<int32_t> *>(outputVecBatch->Get(0)))->GetValue(i);
        EXPECT_EQ(val0, col1[i] + 1);
        int32_t val1 = (reinterpret_cast<Vector<int32_t> *>(outputVecBatch->Get(1)))->GetValue(i);
        EXPECT_EQ(val1, col2[i] + 2);
        int32_t val2 = (reinterpret_cast<Vector<int32_t> *>(outputVecBatch->Get(2)))->GetValue(i);
        EXPECT_EQ(val2, (reinterpret_cast<Vector<DictionaryContainer<int32_t>> *>(t->Get(2)))->GetValue(i) + 10);
    }

    VectorHelper::FreeVecBatch(t);
    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] col1;
    delete[] col2;
    omniruntime::op::Operator::DeleteOperator(op);

    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, Decimal128Arithmetic)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 10;
    int64_t *col1 = MakeDecimals(numRows);
    auto *addLeft = new FieldExpr(0, Decimal128Type(38, 0));
    auto *addRight = new LiteralExpr(new std::string("20"), Decimal128Type(38, 0));
    auto *addExpr = new BinaryExpr(omniruntime::expressions::Operator::ADD, addLeft, addRight, Decimal128Type(38, 0));

    std::vector<Expr *> exprs = {addExpr};
    std::vector<DataTypePtr> vecOfTypes = {Decimal128Type()};
    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col1);

    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);
    for (int64_t i = 0; i < numRows; i++) {
        Decimal128 val0 = (reinterpret_cast<Vector<Decimal128> *>(outputVecBatch->Get(0)))->GetValue(i);
        EXPECT_EQ(val0.HighBits(), 0);
        EXPECT_EQ(val0.LowBits(), i + 20);
    }

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] col1;
    omniruntime::op::Operator::DeleteOperator(op);

    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, Decimal128Arithmetic2)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 10;
    int64_t *col0 = MakeDecimals(numRows, -5);
    int64_t *col1 = MakeDecimals(numRows, -7);

    auto *subLeft0 = new FieldExpr(0, Decimal128Type(38, 0));
    auto *subRight0 = new LiteralExpr(new string("1"), Decimal128Type(38, 0));
    auto *subExpr0 =
        new BinaryExpr(omniruntime::expressions::Operator::SUB, subLeft0, subRight0, Decimal128Type(38, 0));

    auto *subLeft1 = new FieldExpr(1, Decimal128Type(38, 0));
    auto *subRight1 = new LiteralExpr(-1L, Decimal64Type(10, 0));
    auto *subExpr1 =
        new BinaryExpr(omniruntime::expressions::Operator::SUB, subLeft1, subRight1, Decimal128Type(38, 0));

    std::vector<Expr *> exprs = {subExpr0, subExpr1};
    std::vector<DataTypePtr> vecOfTypes = {Decimal128Type(), Decimal128Type()};
    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();

    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col0, col1);
    auto copy = DuplicateVectorBatch(t);
    op->AddInput(copy);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);
    for (int32_t i = 0; i < numRows; i++) {
        Decimal128 val0 = (reinterpret_cast<Vector<Decimal128> *>(outputVecBatch->Get(0)))->GetValue(i);
        Decimal128 val1 = (reinterpret_cast<Vector<Decimal128> *>(outputVecBatch->Get(1)))->GetValue(i);
        Decimal128 old0 = (reinterpret_cast<Vector<Decimal128> *>(t->Get(0)))->GetValue(i);
        Decimal128 old1 = (reinterpret_cast<Vector<Decimal128> *>(t->Get(1)))->GetValue(i);
        EXPECT_EQ(val0.ToInt128(), Decimal128(old0.ToInt128() - 1).ToInt128());
        EXPECT_EQ(val1.ToInt128(), Decimal128(old1.ToInt128() + 1).ToInt128());
    }

    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(t);
    delete[] col0;
    delete[] col1;
    omniruntime::op::Operator::DeleteOperator(op);

    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, Decimal128Arithmetic3)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 10;
    int64_t *col0 = MakeDecimals(numRows, -5);
    int64_t *col1 = MakeDecimals(numRows, -7);

    auto *addLeft0 = new FieldExpr(0, Decimal128Type(38, 0));
    auto *addRight0 = new LiteralExpr(new string("-1"), Decimal128Type(38, 0));
    auto *addExpr0 =
        new BinaryExpr(omniruntime::expressions::Operator::ADD, addLeft0, addRight0, Decimal128Type(38, 0));

    auto *addLeft1 = new FieldExpr(1, Decimal128Type(38, 0));
    auto *addRight1 = new LiteralExpr(1L, Decimal64Type(10, 0));
    auto *addExpr1 =
        new BinaryExpr(omniruntime::expressions::Operator::ADD, addLeft1, addRight1, Decimal128Type(38, 0));

    std::vector<Expr *> exprs = {addExpr0, addExpr1};
    std::vector<DataTypePtr> vecOfTypes = {Decimal128Type(), Decimal128Type()};
    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col0, col1);

    auto copy = DuplicateVectorBatch(t);
    op->AddInput(copy);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);
    for (int i = 0; i < numRows; i++) {
        Decimal128 val0 = (reinterpret_cast<Vector<Decimal128> *>(outputVecBatch->Get(0)))->GetValue(i);
        Decimal128 val1 = (reinterpret_cast<Vector<Decimal128> *>(outputVecBatch->Get(1)))->GetValue(i);
        Decimal128 old0 = (reinterpret_cast<Vector<Decimal128> *>(t->Get(0)))->GetValue(i);
        Decimal128 old1 = (reinterpret_cast<Vector<Decimal128> *>(t->Get(1)))->GetValue(i);
        EXPECT_EQ(val0.ToInt128(), Decimal128(old0.ToInt128() - 1).ToInt128());
        EXPECT_EQ(val1.ToInt128(), Decimal128(old1.ToInt128() + 1).ToInt128());
    }

    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(t);
    delete[] col0;
    delete[] col1;
    omniruntime::op::Operator::DeleteOperator(op);

    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, Decimal128Multiply)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 10;
    int64_t *col1 = MakeDecimals(numRows);
    auto *mulLeft = new FieldExpr(0, Decimal128Type(38, 0));
    auto *mulRight = new LiteralExpr(new std::string("3"), Decimal128Type(38, 0));
    auto *mulExpr = new BinaryExpr(omniruntime::expressions::Operator::MUL, mulLeft, mulRight, Decimal128Type(38, 0));

    std::vector<Expr *> exprs = {mulExpr};
    std::vector<DataTypePtr> vecOfTypes = {Decimal128Type()};
    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col1);

    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);
    for (int64_t i = 0; i < numRows; i++) {
        Decimal128 val0 = (reinterpret_cast<Vector<Decimal128> *>(outputVecBatch->Get(0)))->GetValue(i);
        EXPECT_EQ(val0.HighBits(), 0);
        EXPECT_EQ(val0.LowBits(), i * 3);
    }

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] col1;
    omniruntime::op::Operator::DeleteOperator(op);

    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, Decimal128Divide)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 10;
    int64_t *col1 = MakeDecimals(numRows);
    auto *divRight = new LiteralExpr(new std::string("20"), Decimal128Type(38, 0));
    auto *divExpr = new BinaryExpr(omniruntime::expressions::Operator::DIV, new FieldExpr(0, Decimal128Type(38, 0)),
                                   divRight, Decimal128Type(38, 0));
    std::vector<Expr *> exprs = {divExpr};

    std::vector<DataTypePtr> vecOfTypes = {Decimal128Type()};
    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col1);

    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);
    for (int64_t i = 0; i < numRows; i++) {
        Decimal128 val0 = (reinterpret_cast<Vector<Decimal128> *>(outputVecBatch->Get(0)))->GetValue(i);
        EXPECT_EQ(val0.HighBits(), 0);
        EXPECT_EQ(val0.LowBits(), i / 20);
    }

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] col1;
    omniruntime::op::Operator::DeleteOperator(op);

    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, MultipleDecimal128Columns)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 100;
    int64_t *col1 = MakeDecimals(numRows);
    int64_t *col2 = MakeDecimals(numRows, 100);
    auto *addLeft = new FieldExpr(0, Decimal128Type(38, 0));
    auto *addRight = new LiteralExpr(new std::string("50"), Decimal128Type(38, 0));
    auto *addExpr = new BinaryExpr(omniruntime::expressions::Operator::ADD, addLeft, addRight, Decimal128Type(38, 0));

    auto *mulLeft = new FieldExpr(1, Decimal128Type(38, 0));
    auto *mulRight = new LiteralExpr(new std::string("20"), Decimal128Type(38, 0));
    auto *mulExpr = new BinaryExpr(omniruntime::expressions::Operator::MUL, mulLeft, mulRight, Decimal128Type(38, 0));

    std::vector<Expr *> exprs = {addExpr, mulExpr};

    std::vector<DataTypePtr> vecOfTypes = {Decimal128Type(), Decimal128Type()};
    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col1, col2);

    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);
    for (int32_t i = 0; i < numRows; i++) {
        Decimal128 val0 = (reinterpret_cast<Vector<Decimal128> *>(outputVecBatch->Get(0)))->GetValue(i);
        EXPECT_EQ(val0.HighBits(), 0);
        EXPECT_EQ(val0.LowBits(), i + 50);
    }
    int idx = 0;
    for (int32_t i = 100; i < 100 + 100; i++) {
        Decimal128 val0 = (reinterpret_cast<Vector<Decimal128> *>(outputVecBatch->Get(1)))->GetValue(idx);
        EXPECT_EQ(val0.HighBits(), 0);
        EXPECT_EQ(val0.LowBits(), i * 20);
        idx++;
    }

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] col1;
    delete[] col2;
    omniruntime::op::Operator::DeleteOperator(op);

    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, StringSubstr)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    std::vector<DataTypePtr> vecOfTypes = {VarcharType()};
    DataTypes inputTypes(vecOfTypes);
    const int32_t numRows = 100;
    auto *strings = new std::string[numRows];
    for (int32_t i = 0; i < numRows; i++) {
        if (i % 2 == 0) {
            strings[i] = "helloasdf";
        } else {
            strings[i] = "Bonjour";
        }
    }

    auto *t = CreateVectorBatch(inputTypes, numRows, strings);

    auto *substrData = new FieldExpr(0, VarcharType());
    auto *substrIndex = new LiteralExpr(1, IntType());
    auto *substrLen = new LiteralExpr(5, IntType());
    std::string substrStr = "substr";
    DataTypePtr retType = VarcharType();
    std::vector<Expr *> args;
    args.push_back(substrData);
    args.push_back(substrIndex);
    args.push_back(substrLen);
    auto substrExpr = GetFuncExpr(substrStr, args, VarcharType());

    std::vector<Expr *> concatArgs;
    std::string concatStr = "concat";
    concatArgs.push_back(substrExpr);
    concatArgs.push_back(new LiteralExpr(new std::string(" world"), VarcharType()));
    auto concatExpr = GetFuncExpr(concatStr, concatArgs, VarcharType());

    auto col0 = new FieldExpr(0, VarcharType());
    std::vector<Expr *> exprs = {concatExpr, col0};
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();

    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);

    string expected1 = "hello world";
    string expected2 = "Bonjo world";
    auto *vcVec = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(outputVecBatch->Get(0));
    for (int32_t i = 0; i < numRows; i++) {
        if (i % 2 == 0) {
            EXPECT_TRUE(vcVec->GetValue(i) == expected1);
        } else {
            EXPECT_TRUE(vcVec->GetValue(i) == expected2);
        }
    }

    VectorHelper::FreeVecBatch(outputVecBatch);
    omniruntime::op::Operator::DeleteOperator(op);
    delete[] strings;
    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, SlicedDictionaryVecTest)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 10;

    auto vector1 = new Vector<int32_t>(numRows);
    auto vector2 = new Vector<int32_t>(numRows);
    auto dictionary = new Vector<int32_t>(numRows);

    for (int32_t i = 0; i < numRows; i++) {
        vector1->SetValue(i, i % 5);
        vector2->SetValue(i, i % 11);
        dictionary->SetValue(i, (i % 21) - 3);
    }

    int32_t ids[] = {3, 4, 5, 6, 7, 8, 9, 9, 9, 9};
    auto *dicVec = VectorHelper::CreateDictionary(ids, numRows, dictionary);

    auto slicedCol1 = vector1->Slice(5, 5);
    auto slicedCol2 = vector2->Slice(5, 5);
    auto slicedCol3 = reinterpret_cast<Vector<DictionaryContainer<int32_t>> *>(dicVec)->Slice(5, 5);

    VectorBatch *vecBatch = new VectorBatch(slicedCol1->GetSize());

    auto inputVec = std::vector<DataTypePtr>({IntType(), IntType(), IntType()});
    DataTypes inputTypes1(inputVec);
    vecBatch->Append(slicedCol1);
    vecBatch->Append(slicedCol2);
    vecBatch->Append(slicedCol3);

    FieldExpr *addLeft1 = new FieldExpr(0, IntType());
    LiteralExpr *addRight1 = new LiteralExpr(1, IntType());
    BinaryExpr *addExpr1 = new BinaryExpr(omniruntime::expressions::Operator::ADD, addLeft1, addRight1, IntType());

    FieldExpr *addLeft2 = new FieldExpr(1, IntType());
    LiteralExpr *addRight2 = new LiteralExpr(2, IntType());
    BinaryExpr *addExpr2 = new BinaryExpr(omniruntime::expressions::Operator::ADD, addLeft2, addRight2, IntType());

    FieldExpr *addLeft3 = new FieldExpr(2, IntType());
    LiteralExpr *addRight3 = new LiteralExpr(10, IntType());
    BinaryExpr *addExpr3 = new BinaryExpr(omniruntime::expressions::Operator::ADD, addLeft3, addRight3, IntType());

    std::vector<Expr *> exprs = {addExpr1, addExpr2, addExpr3};
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes1, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(std::move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();

    auto copy = DuplicateVectorBatch(vecBatch);
    op->AddInput(copy);
    VectorBatch *outputVecBatch = nullptr;
    int numReturned = op->GetOutput(&outputVecBatch);
    for (int32_t i = 0; i < numReturned; i++) {
        int32_t val0 = (reinterpret_cast<Vector<int32_t> *>(outputVecBatch->Get(0)))->GetValue(i);
        EXPECT_EQ(val0, (reinterpret_cast<Vector<int32_t> *>(vecBatch->Get(0)))->GetValue(i) + 1);
        int32_t val1 = (reinterpret_cast<Vector<int32_t> *>(outputVecBatch->Get(1)))->GetValue(i);
        EXPECT_EQ(val1, (reinterpret_cast<Vector<int32_t> *>(vecBatch->Get(1)))->GetValue(i) + 2);
        int32_t val2 = (reinterpret_cast<Vector<int32_t> *>(outputVecBatch->Get(2)))->GetValue(i);
        EXPECT_EQ(val2, (reinterpret_cast<Vector<DictionaryContainer<int32_t>> *>(vecBatch->Get(2)))->GetValue(i) + 10);
    }

    VectorHelper::FreeVecBatch(vecBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
    omniruntime::op::Operator::DeleteOperator(op);

    delete vector1;
    delete vector2;
    delete dictionary;
    delete dicVec;
    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, SlicedDictionaryVecWithNullTest)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 10;

    auto col1 = std::make_shared<Vector<int32_t>>(numRows);
    for (int32_t i = 0; i < numRows; i++) {
        if (i % 2 == 0) {
            col1->SetNull(i);
        } else {
            col1->SetValue(i, i % 5);
        }
    }

    int32_t ids[] = {3, 4, 5, 6, 7, 8, 9, 9, 9, 9};
    auto *dicVector = VectorHelper::CreateDictionary(ids, numRows, col1.get());
    auto *slicedCol1 = reinterpret_cast<Vector<DictionaryContainer<int32_t>> *>(dicVector)->Slice(5, 5);

    VectorBatch *t = new VectorBatch(slicedCol1->GetSize());
    t->Append(slicedCol1);
    auto inputVec = std::vector<DataTypePtr>({IntType()});
    DataTypes inputTypes(inputVec);

    FieldExpr *addLeft = new FieldExpr(0, IntType());
    FieldExpr *addRight = new FieldExpr(0, IntType());
    BinaryExpr *addExpr = new BinaryExpr(omniruntime::expressions::Operator::ADD, addLeft, addRight, IntType());
    std::vector<Expr *> exprs = {addExpr};
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(std::move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();

    auto copy = DuplicateVectorBatch(t);
    op->AddInput(copy);
    VectorBatch *outputVecBatch = nullptr;
    int numReturned = op->GetOutput(&outputVecBatch);
    auto retVec = reinterpret_cast<Vector<int32_t> *>(outputVecBatch->Get(0));
    for (int32_t i = 0; i < numReturned; i++) {
        if (i == 0) {
            EXPECT_TRUE(retVec->IsNull(i));
        } else {
            int64_t val0 = retVec->GetValue(i);
            EXPECT_EQ(val0, (slicedCol1->GetValue(i)) * 2);
        }
    }

    VectorHelper::FreeVecBatch(t);
    VectorHelper::FreeVecBatch(outputVecBatch);
    omniruntime::op::Operator::DeleteOperator(op);

    delete dicVector;
    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, Tpcds96)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 21;
    auto *col0 = new int64_t[21];
    auto *col1 = new int64_t[21];
    auto *col2 = new int64_t[21];
    auto *col3 = new int64_t[21];
    vector<long> v0 = {48, 9, 20, 16, 5, 20, 58, 12, 4, 71, 2, 13, 1, 1, 3, 80, 22, 22, 23, 14, 23};
    vector<long> v1 = {139650736612376, 102, 71, 23, 13, 122, 46, 2, 52, 51, 39, 3, 63, 28, 38, 43, 52, 16, 72, 54, 2};
    vector<long> v2 = {33, 17, 14, 0, 49, 21, 8, 56, 16, 2, 18, 47, 0, 15, 27, 58, 0, 4, 5, 3, 23};
    vector<long> v3(21, 0);

    for (int i = 0; i < numRows; i++) {
        col0[i] = v0[i];
        col1[i] = v1[i];
        col2[i] = v2[i];
        col3[i] = v3[i];
    }

    auto *addCol0 = new FieldExpr(0, LongType());
    auto *addCol1 = new FieldExpr(1, LongType());
    auto *addCol2_0 = new FieldExpr(2, LongType());
    auto *addCol2_1 = new FieldExpr(2, LongType());
    auto *addExpr1 = new BinaryExpr(omniruntime::expressions::Operator::ADD, addCol0, addCol1, LongType());
    auto *addExpr2 = new BinaryExpr(omniruntime::expressions::Operator::ADD, addExpr1, addCol2_0, LongType());
    auto *divExpr = new BinaryExpr(omniruntime::expressions::Operator::DIV, addCol2_1, addExpr2, LongType());
    auto *expectRes = new FieldExpr(3, LongType());
    std::vector<Expr *> exprs = {divExpr, expectRes};

    std::vector<DataTypePtr> vecOfTypes = {LongType(), LongType(), LongType(), LongType()};
    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col0, col1, col2, col3);

    for (int32_t i = 0; i < numRows; i++) {
        t->Get(0)->SetNotNull(i);

        if (i == 0) {
            t->Get(1)->SetNull(i);
            t->Get(2)->SetNotNull(i);
            t->Get(3)->SetNull(i);
        } else if (i == 3 || i == 12 || i == 16) {
            t->Get(1)->SetNotNull(i);
            t->Get(2)->SetNull(i);
            t->Get(3)->SetNull(i);
        } else {
            t->Get(1)->SetNotNull(i);
            t->Get(2)->SetNotNull(i);
            t->Get(3)->SetNotNull(i);
        }
    }

    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);
    for (int32_t i = 0; i < numRows; i++) {
        int64_t val0 = (reinterpret_cast<Vector<int64_t> *>(outputVecBatch->Get(0)))->GetValue(i);
        bool b0 = (reinterpret_cast<Vector<int64_t> *>(outputVecBatch->Get(0)))->IsNull(i);
        int64_t val1 = (reinterpret_cast<Vector<int64_t> *>(outputVecBatch->Get(1)))->GetValue(i);
        bool b1 = (reinterpret_cast<Vector<int64_t> *>(outputVecBatch->Get(1)))->IsNull(i);
        EXPECT_EQ(b0, b1);
        if (!b0) {
            EXPECT_EQ(val0, val1);
        }
    }

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] col0;
    delete[] col1;
    delete[] col2;
    delete[] col3;
    omniruntime::op::Operator::DeleteOperator(op);

    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, Round)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 1000;
    int32_t *col0 = MakeInts(numRows, -5);
    int64_t *col1 = MakeLongs(numRows, -5);
    double *col2 = MakeDoubles(numRows, -5.123);
    double *col3 = MakeDoubles(numRows, -5.123);
    double *col4 = MakeDoubles(numRows, -1001.456);
    // Make NaN and inf values
    auto *col5 = new double[numRows];
    double start = 1;
    int32_t idx = 0;
    for (int i = 1; i < start + numRows; i++) {
        if (idx % 2 == 0) {
            // NaN
            col5[idx++] = std::sqrt(-i);
        } else {
            // inf
            col5[idx++] = i / 0.0;
        }
    }
    std::vector<DataTypePtr> vecOfTypes = {IntType(),    LongType(),   DoubleType(),
                                           DoubleType(), DoubleType(), DoubleType()};
    std::string funcStr = "round";

    // rounded to tenths. since int type, no decimals, so should be same as input
    auto data0 = new FieldExpr(0, IntType());
    std::vector<Expr *> args0;
    args0.push_back(data0);
    int32_t data0Decimal = 1;
    args0.push_back(new LiteralExpr(data0Decimal, IntType()));
    auto roundExpr0 = GetFuncExpr(funcStr, args0, IntType());

    // rounded to tens
    auto data1 = new FieldExpr(1, LongType());
    std::vector<Expr *> args1;
    args1.push_back(data1);
    int32_t data1Decimal = -1;
    args1.push_back(new LiteralExpr(data1Decimal, IntType()));
    auto roundExpr1 = GetFuncExpr(funcStr, args1, LongType());
    double data1Pow = pow(10, data1Decimal);

    // rounded to ones. equivalent to round()
    auto data2 = new FieldExpr(2, DoubleType());
    std::vector<Expr *> args2;
    args2.push_back(data2);
    int32_t data2Decimal = 0;
    args2.push_back(new LiteralExpr(data2Decimal, IntType()));
    auto roundExpr2 = GetFuncExpr(funcStr, args2, DoubleType());

    // rounded to hundredths
    auto data3 = new FieldExpr(3, DoubleType());
    std::vector<Expr *> args3;
    args3.push_back(data3);
    int32_t data3Decimal = 2;
    args3.push_back(new LiteralExpr(data3Decimal, IntType()));
    auto roundExpr3 = GetFuncExpr(funcStr, args3, DoubleType());
    double data3Pow = pow(10, data3Decimal);

    // rounded to hundredths (all negatives)
    auto data4 = new FieldExpr(4, DoubleType());
    std::vector<Expr *> args4;
    args4.push_back(data4);
    int32_t data4Decimal = 2;
    args4.push_back(new LiteralExpr(data4Decimal, IntType()));
    auto roundExpr4 = GetFuncExpr(funcStr, args4, DoubleType());
    double data4Pow = pow(10, data4Decimal);

    // invalid values
    auto data5 = new FieldExpr(5, DoubleType());
    std::vector<Expr *> args5;
    args5.push_back(data5);
    int32_t data5Decimal = 2;
    args5.push_back(new LiteralExpr(data5Decimal, IntType()));
    auto roundExpr5 = GetFuncExpr(funcStr, args5, DoubleType());

    std::vector<Expr *> exprs = {roundExpr0, roundExpr1, roundExpr2, roundExpr3, roundExpr4, roundExpr5};

    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col0, col1, col2, col3, col4, col5);

    auto copy = DuplicateVectorBatch(t);
    op->AddInput(copy);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);
    for (int32_t i = 0; i < numRows; i++) {
        int32_t val0 = (reinterpret_cast<Vector<int32_t> *>(outputVecBatch->Get(0)))->GetValue(i);
        int64_t val1 = (reinterpret_cast<Vector<int64_t> *>(outputVecBatch->Get(1)))->GetValue(i);
        double val2 = (reinterpret_cast<Vector<double> *>(outputVecBatch->Get(2)))->GetValue(i);
        double val3 = (reinterpret_cast<Vector<double> *>(outputVecBatch->Get(3)))->GetValue(i);
        double val4 = (reinterpret_cast<Vector<double> *>(outputVecBatch->Get(4)))->GetValue(i);
        double val5 = (reinterpret_cast<Vector<double> *>(outputVecBatch->Get(5)))->GetValue(i);

        int32_t old0 = (reinterpret_cast<Vector<int32_t> *>(t->Get(0)))->GetValue(i);
        int64_t old1 = (reinterpret_cast<Vector<int64_t> *>(t->Get(1)))->GetValue(i);
        double old2 = (reinterpret_cast<Vector<double> *>(t->Get(2)))->GetValue(i);
        double old3 = (reinterpret_cast<Vector<double> *>(t->Get(3)))->GetValue(i);
        double old4 = (reinterpret_cast<Vector<double> *>(t->Get(4)))->GetValue(i);
        double old5 = (reinterpret_cast<Vector<double> *>(t->Get(5)))->GetValue(i);

        EXPECT_EQ(val0, round(old0));
        EXPECT_EQ(val0, old0);
        EXPECT_EQ(val1, (round(old1 * data1Pow) / data1Pow));
        EXPECT_EQ(val2, round(old2));
        EXPECT_EQ(val3, (round(old3 * data3Pow) / data3Pow));
        EXPECT_EQ(val4, -(round(-old4 * data4Pow) / data4Pow));
        if (i % 2 == 0) {
            // cannot compare NaN values
            EXPECT_TRUE(isnan(val5) && isnan(old5));
        } else {
            EXPECT_EQ(val5, old5);
        }
    }

    VectorHelper::FreeVecBatch(t);
    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] col0;
    delete[] col1;
    delete[] col2;
    delete[] col3;
    delete[] col4;
    delete[] col5;
    omniruntime::op::Operator::DeleteOperator(op);

    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, ConcatStrAndChar)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    std::vector<DataTypePtr> vecOfTypes = {CharType()};
    DataTypes inputTypes(vecOfTypes);
    const int32_t numRows = 1;
    auto *strings = new std::string[numRows];
    strings[0] = "AAAA";
    auto *t = CreateVectorBatch(inputTypes, numRows, strings);

    std::vector<Expr *> concatArgs1, concatArgs2;
    std::string concatStr = "concat";
    concatArgs1.push_back(new LiteralExpr(new std::string("store"), VarcharType()));
    concatArgs1.push_back(new FieldExpr(0, CharType(16)));
    auto concatStrChar = GetFuncExpr(concatStr, concatArgs1, CharType(21));

    concatArgs2.push_back(new FieldExpr(0, CharType(16)));
    concatArgs2.push_back(new LiteralExpr(new std::string("store"), VarcharType()));
    auto concatCharStr = GetFuncExpr(concatStr, concatArgs2, CharType(21));
    std::vector<Expr *> exprs = {concatStrChar, concatCharStr};
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();
    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);

    string expected1 = "storeAAAA";
    string expected2 = "AAAA            store";
    auto *vcVec1 = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(outputVecBatch->Get(0));
    auto *vcVec2 = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(outputVecBatch->Get(1));
    for (int32_t i = 0; i < numRows; i++) {
        EXPECT_TRUE(vcVec1->GetValue(i) == expected1);
        EXPECT_TRUE(vcVec2->GetValue(i) == expected2);
    }

    VectorHelper::FreeVecBatch(outputVecBatch);
    omniruntime::op::Operator::DeleteOperator(op);
    delete[] strings;
    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, varcharExpand)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    std::vector<DataTypePtr> vecOfTypes = {VarcharType()};
    DataTypes inputTypes(vecOfTypes);
    const int32_t numRows = 100;

    auto varcharVector = new Vector<LargeStringContainer<std::string_view>>(numRows);
    vector<string> strings;
    for (int32_t i = 0; i < numRows; i++) {
        if (i % 2 == 0) {
            std::string_view input("helloasdf");
            varcharVector->SetValue(i, input);
        } else {
            std::string_view input("Bonjour");
            varcharVector->SetValue(i, input);
        }
        varcharVector->SetNotNull(i);
    }
    vector<bool> nulls;
    for (int32_t i = 0; i < numRows; i++) {
        nulls.emplace_back(false);
    }

    auto *t = new VectorBatch(numRows);
    t->Append(varcharVector);

    FieldExpr *substrData = new FieldExpr(0, VarcharType());
    LiteralExpr *substrIndex = new LiteralExpr(1, IntType());
    LiteralExpr *substrLen = new LiteralExpr(5, IntType());

    std::string substrStr = "substr";
    DataTypePtr retType = VarcharType();
    std::vector<Expr *> args{substrData, substrIndex, substrLen};
    auto substrExpr = GetFuncExpr(substrStr, args, VarcharType());
    std::string baseStr(" world");
    int32_t avgStrLen = 200;
    int32_t strLen = 0;
    for (int i = 0; i < 1000000; i++) {
        baseStr.append(std::to_string(i));
        strLen = baseStr.length();
        if (strLen > avgStrLen) {
            break;
        }
    }
    std::vector<Expr *> concatArgs;
    std::string concatStr = "concat";
    concatArgs.push_back(substrExpr);
    concatArgs.push_back(new LiteralExpr(new std::string(baseStr), VarcharType()));
    auto concatExpr = GetFuncExpr(concatStr, concatArgs, VarcharType());

    FieldExpr *col0 = new FieldExpr(0, VarcharType());
    std::vector<Expr *> exprs = {concatExpr, col0};
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();

    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);

    auto *vcVec = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(outputVecBatch->Get(0));

    EXPECT_GT(omniruntime::vec::unsafe::UnsafeStringVector::GetCapacityInBytes(vcVec), avgStrLen * numRows);
    string expected1 = "hello" + baseStr;
    string expected2 = "Bonjo" + baseStr;
    for (int32_t i = 0; i < numRows; i++) {
        uint8_t *actualChar = (uint8_t *)vcVec->GetValue(i).data();
        int len = vcVec->GetValue(i).length();

        string actualStr(reinterpret_cast<char *>(actualChar), len);
        if (i % 2 == 0) {
            EXPECT_EQ(actualStr, expected1);
        } else {
            EXPECT_EQ(actualStr, expected2);
        }
    }

    VectorHelper::FreeVecBatch(outputVecBatch);
    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, testDivDecimal128)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 1;
    auto addLeft = new LiteralExpr(new std::string("10357"), Decimal128Type(5, 2));
    auto addRight = new LiteralExpr(new std::string("95942"), Decimal128Type(5, 2));

    auto *divExpr = new BinaryExpr(omniruntime::expressions::Operator::DIV, addLeft, addRight, Decimal128Type(38, 2));

    std::vector<Expr *> exprs = {divExpr};
    std::vector<DataTypePtr> vecOfTypes = {};
    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows);
    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);
    Decimal128 val0 = (reinterpret_cast<Vector<Decimal128> *>(outputVecBatch->Get(0)))->GetValue(0);
    EXPECT_EQ(val0.LowBits(), 11);
    EXPECT_EQ(val0.HighBits(), 0);

    VectorHelper::FreeVecBatch(outputVecBatch);
    omniruntime::op::Operator::DeleteOperator(op);

    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, testAddDecimal128)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 1;
    auto addLeft = new LiteralExpr(new std::string("478193"), Decimal128Type(6, 3));
    auto addRight = new LiteralExpr(new std::string("54356783"), Decimal128Type(8, 5));

    auto *addExpr = new BinaryExpr(omniruntime::expressions::Operator::ADD, addLeft, addRight, Decimal128Type(9, 5));

    std::vector<Expr *> exprs = {addExpr};
    std::vector<DataTypePtr> vecOfTypes = {};
    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows);
    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);
    Decimal128 val0 = (reinterpret_cast<Vector<Decimal128> *>(outputVecBatch->Get(0)))->GetValue(0);
    EXPECT_EQ(val0.LowBits(), 102176083);
    EXPECT_EQ(val0.HighBits(), 0);

    VectorHelper::FreeVecBatch(outputVecBatch);
    omniruntime::op::Operator::DeleteOperator(op);

    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, testDecimal128Between)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 1;
    auto value = new LiteralExpr(new std::string("1234"), Decimal128Type(4, 0));
    auto lowerBound = new LiteralExpr(new std::string("1234"), Decimal128Type(4, 3));
    auto upperBound = new LiteralExpr(new std::string("1234"), Decimal128Type(4, 1));

    auto expr = new BetweenExpr(value, lowerBound, upperBound);

    std::vector<Expr *> exprs = {expr};
    std::vector<DataTypePtr> vecOfTypes = {};
    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();

    VectorBatch *t = CreateVectorBatch(inputTypes, numRows);
    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);
    bool val0 = (reinterpret_cast<Vector<bool> *>(outputVecBatch->Get(0)))->GetValue(0);
    EXPECT_FALSE(val0);

    VectorHelper::FreeVecBatch(outputVecBatch);
    omniruntime::op::Operator::DeleteOperator(op);

    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, testDecimal128In)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 1;
    auto arg0 = new LiteralExpr(new std::string("1234"), Decimal128Type(6, 0));
    auto arg1 = new LiteralExpr(new std::string("12345"), Decimal128Type(6, 0));
    auto arg2 = new LiteralExpr(new std::string("123456"), Decimal128Type(6, 0));
    std::vector<Expr *> args = {arg0, arg1, arg2};
    auto expr = new InExpr(args);
    std::vector<Expr *> exprs = {expr};

    std::vector<DataTypePtr> vecOfTypes = {};
    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();

    VectorBatch *t = CreateVectorBatch(inputTypes, numRows);
    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);
    bool val0 = (reinterpret_cast<Vector<bool> *>(outputVecBatch->Get(0)))->GetValue(0);
    EXPECT_FALSE(val0);

    VectorHelper::FreeVecBatch(outputVecBatch);
    omniruntime::op::Operator::DeleteOperator(op);

    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, testDecimal128Comprehensive)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 1;
    auto condition = new LiteralExpr(true, BooleanType());
    auto v1 = new LiteralExpr(new std::string("123400"), Decimal128Type(7, 3));
    v1->isNull = true;
    auto v2 = new LiteralExpr(new std::string("1234"), Decimal128Type(7, 3));
    auto coalesce = new CoalesceExpr(v1, v2);

    auto falseExpr = new LiteralExpr(new std::string("1234000"), Decimal128Type(7, 3));
    auto ifExpr = new IfExpr(condition, coalesce, falseExpr);
    auto right = new LiteralExpr(new std::string("1234"), Decimal128Type(4, 2));
    auto expr = new BinaryExpr(omniruntime::expressions::Operator::GT, ifExpr, right, BooleanType());

    std::vector<Expr *> exprs = {expr};
    std::vector<DataTypePtr> vecOfTypes = {};
    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();

    VectorBatch *t = CreateVectorBatch(inputTypes, numRows);
    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);
    bool val0 = (reinterpret_cast<Vector<bool> *>(outputVecBatch->Get(0)))->GetValue(0);
    EXPECT_FALSE(val0);

    VectorHelper::FreeVecBatch(outputVecBatch);
    omniruntime::op::Operator::DeleteOperator(op);

    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, TestAndExprWithNull)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 9;
    bool col1[numRows] = {true, true, true, false, false, false, true, false, false};
    bool col2[numRows] = {true, false, true, true, false, false, true, false, false};

    auto andLeft = new FieldExpr(0, BooleanType());
    auto andRight = new FieldExpr(1, BooleanType());
    auto andLeft1 = new FieldExpr(0, BooleanType());
    auto andRight1 = new FieldExpr(1, BooleanType());
    auto andLeft2 = new FieldExpr(0, BooleanType());
    auto andRight2 = new FieldExpr(1, BooleanType());
    auto *andExpr = new BinaryExpr(omniruntime::expressions::Operator::AND, andLeft, andRight, BooleanType());
    auto *andExpr1 = new BinaryExpr(omniruntime::expressions::Operator::AND, andLeft1, andRight1, BooleanType());
    auto *andExpr2 = new BinaryExpr(omniruntime::expressions::Operator::AND, andLeft2, andRight2, BooleanType());
    auto isNullExpr = new IsNullExpr(andExpr1);
    auto isNullExpr1 = new IsNullExpr(andExpr2);
    auto notExpr = new UnaryExpr(omniruntime::expressions::Operator::NOT, isNullExpr1, BooleanType());
    std::vector<Expr *> exprs = {andExpr, isNullExpr, notExpr};

    std::vector<DataTypePtr> vecOfTypes = {BooleanType(), BooleanType()};
    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();

    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col1, col2);

    t->Get(0)->SetNull(6);
    t->Get(0)->SetNull(7);
    t->Get(0)->SetNull(8);

    t->Get(1)->SetNull(2);
    t->Get(1)->SetNull(5);
    t->Get(1)->SetNull(8);

    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);
    for (int32_t i = 0; i < numRows; i++) {
        bool val = (reinterpret_cast<Vector<bool> *>(outputVecBatch->Get(0)))->GetValue(i);
        bool isValNull = (reinterpret_cast<Vector<bool> *>(outputVecBatch->Get(0)))->IsNull(i);
        bool val1 = (reinterpret_cast<Vector<bool> *>(outputVecBatch->Get(1)))->GetValue(i);
        bool val2 = (reinterpret_cast<Vector<bool> *>(outputVecBatch->Get(2)))->GetValue(i);
        if (i == 0) {
            EXPECT_TRUE(val);
            EXPECT_FALSE(isValNull);
            EXPECT_FALSE(val1);
            EXPECT_TRUE(val2);
        } else if (i == 2 || i == 6 || i == 8) {
            EXPECT_TRUE(isValNull);
            EXPECT_TRUE(val1);
            EXPECT_FALSE(val2);
        } else {
            EXPECT_FALSE(val);
            EXPECT_FALSE(isValNull);
            EXPECT_FALSE(val1);
            EXPECT_TRUE(val2);
        }
    }

    VectorHelper::FreeVecBatch(outputVecBatch);
    omniruntime::op::Operator::DeleteOperator(op);

    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, TestOrExprWithNull)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 9;
    bool col1[numRows] = {true, true, true, false, false, false, true, false, false};
    bool col2[numRows] = {true, false, true, true, false, false, true, false, false};

    auto orLeft = new FieldExpr(0, BooleanType());
    auto orRight = new FieldExpr(1, BooleanType());
    auto *orExpr = new BinaryExpr(omniruntime::expressions::Operator::OR, orLeft, orRight, BooleanType());
    std::vector<Expr *> exprs = {orExpr};
    std::vector<DataTypePtr> vecOfTypes = {BooleanType(), BooleanType()};
    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col1, col2);

    t->Get(0)->SetNull(6);
    t->Get(0)->SetNull(7);
    t->Get(0)->SetNull(8);

    t->Get(1)->SetNull(2);
    t->Get(1)->SetNull(5);
    t->Get(1)->SetNull(8);

    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);
    for (int32_t i = 0; i < numRows; i++) {
        bool val = (reinterpret_cast<Vector<bool> *>(outputVecBatch->Get(0)))->GetValue(i);
        bool isValNull = (reinterpret_cast<Vector<bool> *>(outputVecBatch->Get(0)))->IsNull(i);
        if (i == 4) {
            EXPECT_FALSE(val);
            EXPECT_FALSE(isValNull);
        } else if (i == 5 || i == 7 || i == 8) {
            EXPECT_TRUE(isValNull);
        } else {
            EXPECT_TRUE(val);
            EXPECT_FALSE(isValNull);
        }
    }

    VectorHelper::FreeVecBatch(outputVecBatch);
    omniruntime::op::Operator::DeleteOperator(op);

    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, testSubDecimal64)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 1;
    auto subLeft = new LiteralExpr(4321563L, Decimal64Type(7, 3));
    auto subRight = new LiteralExpr(123468L, Decimal64Type(6, 4));

    auto *subExpr = new BinaryExpr(omniruntime::expressions::Operator::SUB, subLeft, subRight, Decimal64Type(8, 4));

    std::vector<Expr *> exprs = {subExpr};
    std::vector<DataTypePtr> vecOfTypes = {};
    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();

    VectorBatch *t = CreateVectorBatch(inputTypes, numRows);
    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);
    int64_t val0 = (reinterpret_cast<Vector<int64_t> *>(outputVecBatch->Get(0)))->GetValue(0);
    EXPECT_EQ(val0, 43092162);

    VectorHelper::FreeVecBatch(outputVecBatch);
    omniruntime::op::Operator::DeleteOperator(op);

    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, testMulDecimal64)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 1;
    auto mulLeft = new LiteralExpr(100L, Decimal64Type(7, 2));
    auto mulRight = new LiteralExpr(100L, Decimal64Type(7, 2));

    auto *mulExpr = new BinaryExpr(omniruntime::expressions::Operator::MUL, mulLeft, mulRight, Decimal64Type(7, 4));

    std::vector<Expr *> exprs = {mulExpr};
    std::vector<DataTypePtr> vecOfTypes = {};
    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();

    VectorBatch *t = CreateVectorBatch(inputTypes, numRows);
    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);
    int64_t val0 = (reinterpret_cast<Vector<int64_t> *>(outputVecBatch->Get(0)))->GetValue(0);
    EXPECT_EQ(val0, 10000L);

    VectorHelper::FreeVecBatch(outputVecBatch);
    omniruntime::op::Operator::DeleteOperator(op);

    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, testDivDecimal64)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 1;
    auto left = new LiteralExpr(1225L, Decimal64Type(4, 2));
    auto right = new LiteralExpr(125L, Decimal64Type(3, 2));

    auto *divExpr = new BinaryExpr(omniruntime::expressions::Operator::DIV, left, right, Decimal64Type(2, 1));

    std::vector<Expr *> exprs = {divExpr};
    std::vector<DataTypePtr> vecOfTypes = {};
    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();

    VectorBatch *t = CreateVectorBatch(inputTypes, numRows);
    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);
    int64_t val0 = (reinterpret_cast<Vector<int64_t> *>(outputVecBatch->Get(0)))->GetValue(0);
    EXPECT_EQ(val0, 98L);

    VectorHelper::FreeVecBatch(outputVecBatch);
    omniruntime::op::Operator::DeleteOperator(op);

    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, testModDecimal64)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 1;
    auto left = new LiteralExpr(12250L, Decimal64Type(5, 3));
    auto right = new LiteralExpr(125L, Decimal64Type(3, 2));

    auto *modExpr = new BinaryExpr(omniruntime::expressions::Operator::MOD, left, right, Decimal64Type(4, 3));

    std::vector<Expr *> exprs = {modExpr};
    std::vector<DataTypePtr> vecOfTypes = {};
    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();

    VectorBatch *t = CreateVectorBatch(inputTypes, numRows);
    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);
    int64_t val0 = (reinterpret_cast<Vector<int64_t> *>(outputVecBatch->Get(0)))->GetValue(0);
    EXPECT_EQ(val0, 1000L);

    VectorHelper::FreeVecBatch(outputVecBatch);
    omniruntime::op::Operator::DeleteOperator(op);

    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, testDecimal64ArithOutputDecimal128)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 1;
    auto addLeft = new LiteralExpr(-999999999999999999L, Decimal64Type(18, 0));
    auto addRight = new LiteralExpr(2L, Decimal64Type(18, 0));
    auto *addExpr = new BinaryExpr(omniruntime::expressions::Operator::ADD, addLeft, addRight, Decimal128Type(19, 0));
    auto subLeft = new LiteralExpr(-999999999999999999L, Decimal64Type(18, 0));
    auto subRight = new LiteralExpr(2L, Decimal64Type(18, 0));

    auto *subExpr = new BinaryExpr(omniruntime::expressions::Operator::SUB, subLeft, subRight, Decimal128Type(19, 0));
    std::vector<Expr *> exprs = {addExpr, subExpr};

    std::vector<DataTypePtr> vecOfTypes = {};
    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));

    omniruntime::op::Operator *op = factory->CreateOperator();
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows);

    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);
    Decimal128 val0 = (reinterpret_cast<Vector<Decimal128> *>(outputVecBatch->Get(0)))->GetValue(0);
    EXPECT_EQ(val0.HighBits(), ~0);
    EXPECT_EQ(val0.LowBits(), -999999999999999997L);
    Decimal128 val1 = (reinterpret_cast<Vector<Decimal128> *>(outputVecBatch->Get(1)))->GetValue(0);

    EXPECT_EQ(val1.HighBits(), ~0);
    EXPECT_EQ(val1.LowBits(), -1000000000000000001);

    VectorHelper::FreeVecBatch(outputVecBatch);
    omniruntime::op::Operator::DeleteOperator(op);

    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, testDecimal64In)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 1;
    auto arg0 = new LiteralExpr(65781L, Decimal64Type(6, 2));
    auto arg1 = new LiteralExpr(120945L, Decimal64Type(6, 2));
    auto arg2 = new LiteralExpr(65781L, Decimal64Type(6, 2));
    auto arg3 = new LiteralExpr(65781L, Decimal64Type(6, 2));
    std::vector<Expr *> args = {arg0, arg1, arg2, arg3};
    auto expr = new InExpr(args);
    std::vector<Expr *> exprs = {expr};

    std::vector<DataTypePtr> vecOfTypes = {};
    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();

    VectorBatch *t = CreateVectorBatch(inputTypes, numRows);
    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);
    bool val0 = (reinterpret_cast<Vector<bool> *>(outputVecBatch->Get(0)))->GetValue(0);
    EXPECT_TRUE(val0);

    VectorHelper::FreeVecBatch(outputVecBatch);
    omniruntime::op::Operator::DeleteOperator(op);

    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, testDecimal64Between)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 1;
    auto value = new LiteralExpr(76582L, Decimal64Type(5, 2));
    auto lowerBound = new LiteralExpr(87230L, Decimal64Type(5, 4));
    auto upperBound = new LiteralExpr(876903L, Decimal64Type(6, 1));
    auto expr = new BetweenExpr(value, lowerBound, upperBound);
    std::vector<Expr *> exprs = {expr};

    std::vector<DataTypePtr> vecOfTypes = {};
    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows);
    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);
    bool val0 = (reinterpret_cast<Vector<bool> *>(outputVecBatch->Get(0)))->GetValue(0);
    EXPECT_TRUE(val0);

    VectorHelper::FreeVecBatch(outputVecBatch);
    omniruntime::op::Operator::DeleteOperator(op);

    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, testDecimal64Comprehensive)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 1;
    auto condition = new LiteralExpr(true, BooleanType());
    auto v1 = new LiteralExpr(123400L, Decimal64Type(7, 3));
    v1->isNull = true;
    auto v2 = new LiteralExpr(1234L, Decimal64Type(7, 3));
    auto coalesce = new CoalesceExpr(v1, v2);

    auto falseExpr = new LiteralExpr(1234000L, Decimal64Type(7, 3));
    auto ifExpr = new IfExpr(condition, coalesce, falseExpr);

    auto subLeft = new LiteralExpr(12340L, Decimal64Type(7, 3));
    auto subRight = new LiteralExpr(1010L, Decimal64Type(7, 3));
    auto right = new BinaryExpr(omniruntime::expressions::Operator::SUB, subLeft, subRight, Decimal64Type(7, 3));
    auto expr = new BinaryExpr(omniruntime::expressions::Operator::GT, ifExpr, right, BooleanType());

    std::vector<Expr *> exprs = {expr};
    std::vector<DataTypePtr> vecOfTypes = {};
    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();

    VectorBatch *input = CreateVectorBatch(inputTypes, numRows);
    op->AddInput(input);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);
    bool val0 = (reinterpret_cast<Vector<bool> *>(outputVecBatch->Get(0)))->GetValue(0);
    EXPECT_FALSE(val0);

    VectorHelper::FreeVecBatch(outputVecBatch);
    omniruntime::op::Operator::DeleteOperator(op);

    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, Decimal64ColDivide)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 1000;
    int64_t *col1 = MakeLongs(numRows, -500);
    auto *divRight = new LiteralExpr(92122L, Decimal64Type(8, 4));
    auto *divExpr = new BinaryExpr(omniruntime::expressions::Operator::DIV, new FieldExpr(0, Decimal64Type(8, 4)),
                                   divRight, Decimal64Type(8, 4));
    std::vector<Expr *> exprs = {divExpr};

    std::vector<DataTypePtr> vecOfTypes = {Decimal64Type()};
    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col1);

    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);
    for (int64_t i = 0; i < numRows; i++) {
        int64_t val0 = (reinterpret_cast<Vector<int64_t> *>(outputVecBatch->Get(0)))->GetValue(i);
        int64_t expect = round(double(col1[i] * 10000) / 92122);
        EXPECT_EQ(val0, expect);
    }

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] col1;
    omniruntime::op::Operator::DeleteOperator(op);

    delete factory;
    delete overflowConfig;
}

Expr *GetConcatFuncExpr(DataTypePtr dataType0, DataTypePtr dataType1, DataTypePtr returnType)
{
    std::string concatStr = "concat";
    std::vector<Expr *> args;
    args.push_back(new FieldExpr(1, std::move(dataType1)));
    args.push_back(new FieldExpr(0, std::move(dataType0)));
    auto concatExpr = GetFuncExpr(concatStr, args, std::move(returnType));
    return concatExpr;
}

VectorBatch *CreateInputVecBatchForConcat(const std::vector<DataTypePtr> &inputTypes)
{
    const int32_t rowCount = 8;
    const std::string firstName = "John";
    const std::string lastName = "Rebecca";
    const std::string fullFirstName = "-John-John-John-John";
    const std::string fullLastName = "Rebecca-Rebecca-Rebecca-Rebeca";
    const std::string empty;
    auto vec0 = new std::string[rowCount];
    vec0[0] = empty;
    vec0[1] = firstName;
    vec0[2] = empty;
    vec0[3] = empty;
    vec0[4] = firstName;
    vec0[5] = empty;
    vec0[6] = firstName;
    vec0[7] = fullFirstName;

    auto vec1 = new std::string[rowCount];
    vec1[0] = empty;
    vec1[1] = empty;
    vec1[2] = lastName;
    vec1[3] = empty;
    vec1[4] = empty;
    vec1[5] = lastName;
    vec1[6] = lastName;
    vec1[7] = fullLastName;

    VectorBatch *input = CreateVectorBatch(DataTypes(inputTypes), rowCount, vec0, vec1);
    input->Get(0)->SetNull(0);
    input->Get(0)->SetNull(2);
    input->Get(1)->SetNull(0);
    input->Get(1)->SetNull(1);
    delete[] vec0;
    delete[] vec1;
    return input;
}

VectorBatch *CreateExpectVecBatchForConcat(const DataTypes &expectDataType, const std::vector<std::string> &expectDatas)
{
    int32_t rowCount = expectDatas.size();
    auto vec = new std::string[rowCount];

    VectorBatch *expectVecBatch = CreateVectorBatch(expectDataType, rowCount, vec);
    auto *expectVec = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(expectVecBatch->Get(0));
    for (int32_t i = 0; i < rowCount; i++) {
        if (expectDatas[i] == "NULL") {
            expectVec->SetNull(i);
        } else {
            auto strView = std::string_view{expectDatas[i]};
            expectVec->SetValue(i, strView);
        }
    }
    delete[] vec;
    return expectVecBatch;
}

TEST(ProjectionTest, ConcatStrCharTest)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    auto concatExpr = GetConcatFuncExpr(CharType(20), VarcharType(30), CharType(100));
    std::vector<Expr *> exprs = {concatExpr};
    std::vector<DataTypePtr> vecOfTypes = {CharType(20), VarcharType(30)};
    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));

    auto input = CreateInputVecBatchForConcat(vecOfTypes);

    auto op = factory->CreateOperator();
    op->AddInput(input);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);

    std::vector<std::string> expectedDatas = {
        "NULL", "NULL",    "NULL",        "",
        "John", "Rebecca", "RebeccaJohn", "Rebecca-Rebecca-Rebecca-Rebeca-John-John-John-John"};
    std::vector<DataTypePtr> outTypes = {CharType(100)};
    auto expect = CreateExpectVecBatchForConcat(DataTypes{outTypes}, expectedDatas);
    EXPECT_TRUE(VecBatchMatch(outputVecBatch, expect));

    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expect);
    ;
    delete op;
    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, ConcatCharStrTest)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    auto concatExpr = GetConcatFuncExpr(VarcharType(20), CharType(30), CharType(100));
    std::vector<Expr *> exprs = {concatExpr};
    std::vector<DataTypePtr> vecOfTypes = {VarcharType(20), CharType(30)};
    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));

    auto input = CreateInputVecBatchForConcat(vecOfTypes);

    auto op = factory->CreateOperator();
    op->AddInput(input);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);

    std::vector<std::string> expectedDatas = {"NULL",
                                              "NULL",
                                              "NULL",
                                              "",
                                              "                              John",
                                              "Rebecca",
                                              "Rebecca                       John",
                                              "Rebecca-Rebecca-Rebecca-Rebeca-John-John-John-John"};
    std::vector<DataTypePtr> outTypes = {CharType(100)};
    auto expect = CreateExpectVecBatchForConcat(DataTypes{outTypes}, expectedDatas);
    EXPECT_TRUE(VecBatchMatch(outputVecBatch, expect));

    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expect);
    ;
    delete op;
    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, ConcatStrStrTest)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    auto concatExpr = GetConcatFuncExpr(VarcharType(20), VarcharType(30), VarcharType(100));
    std::vector<Expr *> exprs = {concatExpr};
    std::vector<DataTypePtr> vecOfTypes = {VarcharType(20), VarcharType(30)};
    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));

    auto input = CreateInputVecBatchForConcat(vecOfTypes);

    auto op = factory->CreateOperator();
    op->AddInput(input);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);

    std::vector<std::string> expectedDatas = {
        "NULL", "NULL",    "NULL",        "",
        "John", "Rebecca", "RebeccaJohn", "Rebecca-Rebecca-Rebecca-Rebeca-John-John-John-John"};

    std::vector<DataTypePtr> outTypes = {VarcharType(100)};
    auto expect = CreateExpectVecBatchForConcat(DataTypes{outTypes}, expectedDatas);
    EXPECT_TRUE(VecBatchMatch(outputVecBatch, expect));

    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expect);
    ;
    delete op;
    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, ConcatCharCharTest)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    auto concatExpr = GetConcatFuncExpr(CharType(20), CharType(30), CharType(51));
    std::vector<Expr *> exprs = {concatExpr};
    std::vector<DataTypePtr> vecOfTypes = {CharType(20), CharType(30)};
    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));

    auto input = CreateInputVecBatchForConcat(vecOfTypes);

    auto op = factory->CreateOperator();
    op->AddInput(input);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);

    std::vector<std::string> expectedDatas = {"NULL",
                                              "NULL",
                                              "NULL",
                                              "",
                                              "                              John",
                                              "Rebecca",
                                              "Rebecca                       John",
                                              "Rebecca-Rebecca-Rebecca-Rebeca-John-John-John-John"};
    std::vector<DataTypePtr> outTypes = {CharType(51)};
    auto expect = CreateExpectVecBatchForConcat(DataTypes{outTypes}, expectedDatas);
    EXPECT_TRUE(VecBatchMatch(outputVecBatch, expect));

    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expect);
    ;
    delete op;
    delete factory;
    delete overflowConfig;
}

Expr *GetStringFuncExpr(std::vector<DataTypePtr> inputTypes, DataTypePtr returnType, const string &funcStr)
{
    std::vector<Expr *> args;
    for (int32_t i = 0; i < (int32_t)inputTypes.size(); i++) {
        args.push_back(new FieldExpr(i, std::move(inputTypes.at(i))));
    }

    auto stringFuncExpr = GetFuncExpr(funcStr, args, std::move(returnType));
    return stringFuncExpr;
}

TEST(ProjectionTest, ReplaceStrWithRep)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    vector<DataTypePtr> vecTypes = {VarcharType(20), VarcharType(10), VarcharType(10)};
    string replaceFuncStr = "replace";
    auto replaceFuncExpr = GetStringFuncExpr(vecTypes, VarcharType(100), replaceFuncStr);
    vector<Expr *> exprs = {replaceFuncExpr};
    DataTypes inputTypes(vecTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));

    string str[] = {"varchar100", "varchar200", "varchar300"};
    string search[] = {"char1", "char2", "char3"};
    string replace[] = {"opera", "*#", "VARCHAR"};
    auto input = CreateVectorBatch(inputTypes, 3, str, search, replace);

    auto op = factory->CreateOperator();
    op->AddInput(input);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);

    vector<DataTypePtr> expectedTypes = {VarcharType(100)};
    string expectedDatas[] = {"varopera00", "var*#00", "varVARCHAR00"};

    auto expect = CreateVectorBatch(DataTypes(expectedTypes), 3, expectedDatas);
    EXPECT_TRUE(VecBatchMatch(outputVecBatch, expect));

    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expect);
    delete overflowConfig;
    delete op;
    delete factory;
}

TEST(ProjectionTest, ReplaceStrWithoutRep)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    vector<DataTypePtr> vecTypes = {VarcharType(20), VarcharType(10)};
    string replaceFuncStr = "replace";
    auto replaceFuncExpr = GetStringFuncExpr(vecTypes, VarcharType(100), replaceFuncStr);
    vector<Expr *> exprs = {replaceFuncExpr};
    DataTypes inputTypes(vecTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));

    string str[] = {"varchar100", "varchar200", "varchar300"};
    string search[] = {"char1", "char2", "char3"};
    auto input = CreateVectorBatch(inputTypes, 3, str, search);

    auto op = factory->CreateOperator();
    op->AddInput(input);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);

    vector<DataTypePtr> expectedTypes = {VarcharType(100)};
    string expectedDatas[] = {"var00", "var00", "var00"};

    auto expect = CreateVectorBatch(DataTypes(expectedTypes), 3, expectedDatas);
    EXPECT_TRUE(VecBatchMatch(outputVecBatch, expect));

    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expect);
    delete overflowConfig;
    delete op;
    delete factory;
}

TEST(ProjectionTest, LowerStr)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    vector<DataTypePtr> vecTypes = {VarcharType(20)};
    string lowerFuncStr = "lower";
    auto lowerFuncExpr = GetStringFuncExpr(vecTypes, VarcharType(20), lowerFuncStr);
    vector<Expr *> exprs = {lowerFuncExpr};
    DataTypes inputTypes(vecTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));

    string str[] = {"VARchar100", "Char200", "var**VAR"};
    auto input = CreateVectorBatch(inputTypes, 3, str);

    auto op = factory->CreateOperator();
    op->AddInput(input);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);

    vector<DataTypePtr> expectedTypes = {VarcharType(20)};
    string expectedDatas[] = {"varchar100", "char200", "var**var"};

    auto expect = CreateVectorBatch(DataTypes(expectedTypes), 3, expectedDatas);
    EXPECT_TRUE(VecBatchMatch(outputVecBatch, expect));

    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expect);
    delete overflowConfig;
    delete op;
    delete factory;
}

TEST(ProjectionTest, LowerChar)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    vector<DataTypePtr> vecTypes = {CharType(20)};
    string lowerFuncStr = "lower";
    auto lowerFuncExpr = GetStringFuncExpr(vecTypes, CharType(20), lowerFuncStr);
    vector<Expr *> exprs = {lowerFuncExpr};
    DataTypes inputTypes(vecTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));

    string str[] = {"VARchar100", "Char200", "var**VAR"};
    auto input = CreateVectorBatch(inputTypes, 3, str);

    auto op = factory->CreateOperator();
    op->AddInput(input);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);

    vector<DataTypePtr> expectedTypes = {CharType(20)};
    string expectedDatas[] = {"varchar100", "char200", "var**var"};

    auto expect = CreateVectorBatch(DataTypes(expectedTypes), 3, expectedDatas);
    EXPECT_TRUE(VecBatchMatch(outputVecBatch, expect));

    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expect);
    delete overflowConfig;
    delete op;
    delete factory;
}

TEST(ProjectionTest, LengthChar)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    vector<DataTypePtr> vecTypes = {CharType(20)};
    string lenFuncStr = "length";
    auto lenFuncExpr = GetStringFuncExpr(vecTypes, LongType(), lenFuncStr);
    vector<Expr *> exprs = {lenFuncExpr};
    DataTypes inputTypes(vecTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));

    string str[] = {"VARchar100", "Char200", "var**VAR"};
    auto input = CreateVectorBatch(inputTypes, 3, str);

    auto op = factory->CreateOperator();
    op->AddInput(input);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);

    vector<DataTypePtr> expectedTypes = {LongType()};
    int64_t expectedDatas[] = {20, 20, 20};

    auto expect = CreateVectorBatch(DataTypes(expectedTypes), 3, expectedDatas);
    EXPECT_TRUE(VecBatchMatch(outputVecBatch, expect));

    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expect);
    ;
    delete overflowConfig;
    delete op;
    delete factory;
}

TEST(ProjectionTest, LengthStr)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    vector<DataTypePtr> vecTypes = {VarcharType(20)};
    string lenFuncStr = "length";
    auto lenFuncExpr = GetStringFuncExpr(vecTypes, LongType(), lenFuncStr);
    vector<Expr *> exprs = {lenFuncExpr};
    DataTypes inputTypes(vecTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));

    string str[] = {"VARchar100", "Char200", "var**VAR"};
    auto input = CreateVectorBatch(inputTypes, 3, str);

    auto op = factory->CreateOperator();
    op->AddInput(input);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);

    vector<DataTypePtr> expectedTypes = {LongType()};
    int64_t expectedDatas[] = {10, 7, 8};

    auto expect = CreateVectorBatch(DataTypes(expectedTypes), 3, expectedDatas);
    EXPECT_TRUE(VecBatchMatch(outputVecBatch, expect));

    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expect);
    ;
    delete overflowConfig;
    delete op;
    delete factory;
}

TEST(ProjectionTest, MightContain)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);

    int32_t versionJava = 1;
    int32_t hashFuncNum = 6;
    int32_t numWords = 1048576; // 1MBytes

    int64_t byteLength = numWords * sizeof(int64_t) + sizeof(versionJava) + sizeof(hashFuncNum) + sizeof(numWords);
    auto *in = new int8_t[byteLength]{0};
    (reinterpret_cast<int32_t *>(in))[0] = 1;
    (reinterpret_cast<int32_t *>(in))[1] = hashFuncNum;
    (reinterpret_cast<int32_t *>(in))[2] = numWords;
    auto *bloomFilter = new BloomFilter(in, versionJava);
    for (int64_t i = 1; i < 100; i += 2) {
        bloomFilter->PutLong(i);
    }

    auto bloomFilterAddr = new LiteralExpr(reinterpret_cast<int64_t>(bloomFilter), LongType());
    auto hashValue = new FieldExpr(0, LongType());
    std::vector<Expr *> hashArgs;
    hashArgs.push_back(bloomFilterAddr);
    hashArgs.push_back(hashValue);
    string mightContainFuncStr = "might_contain";
    auto mightContainFuncExpr = GetFuncExpr(mightContainFuncStr, hashArgs, BooleanType());
    vector<Expr *> exprs = {mightContainFuncExpr};
    std::vector<DataTypePtr> vecTypes = {LongType()};
    DataTypes inputTypes(vecTypes);
    auto overflowConfig = new OverflowConfig();

    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto factory = new ProjectionOperatorFactory(move(exprEvaluator));

    int64_t value[] = {11L, 30L, 55L};
    auto input = CreateVectorBatch(inputTypes, 3, value);

    auto op = factory->CreateOperator();
    op->AddInput(input);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);

    vector<DataTypePtr> expectedTypes = {BooleanType()};
    bool expectedDatas[] = {true, false, true};

    auto expect = CreateVectorBatch(DataTypes(expectedTypes), 3, expectedDatas);
    EXPECT_TRUE(VecBatchMatch(outputVecBatch, expect));

    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expect);
    delete overflowConfig;
    delete op;
    delete factory;
    delete bloomFilter;
    delete[] in;
}

TEST(ProjectionTest, testDecimal128NegativeLiteral)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 1;
    auto literalLeft = new LiteralExpr(new std::string("0"), Decimal128Type(38, 16));
    auto literalRight =
        new LiteralExpr(new std::string("-99999999999999999999999999999999999999"), Decimal128Type(38, 16));

    auto *subExpr =
        new BinaryExpr(omniruntime::expressions::Operator::SUB, literalLeft, literalRight, Decimal128Type(38, 16));

    std::vector<Expr *> exprs = {subExpr};
    std::vector<DataTypePtr> vecOfTypes = {};
    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();

    VectorBatch *t = CreateVectorBatch(inputTypes, numRows);
    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);
    Decimal128 val0 = (reinterpret_cast<Vector<Decimal128> *>(outputVecBatch->Get(0)))->GetValue(0);
    EXPECT_EQ(val0.LowBits(), 687399551400673279);
    EXPECT_EQ(val0.HighBits(), 5421010862427522170);

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete op;
    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, ProjectCastIntToString)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 3;
    auto *col = new int[3]{123, 312, 456};
    std::vector<DataTypePtr> vecOfTypes = {IntType()};

    auto data = new FieldExpr(0, IntType());
    std::string castStr = "CAST";
    std::vector<Expr *> args;
    args.push_back(data);
    auto castExpr = GetFuncExpr(castStr, args, VarcharType(4));

    std::vector<Expr *> exprs = {castExpr};
    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col);

    auto copy = DuplicateVectorBatch(t);
    op->AddInput(copy);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);
    auto *vcVec = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(outputVecBatch->Get(0));
    for (int32_t i = 0; i < numRows; i++) {
        EXPECT_TRUE(vcVec->GetValue(i) == to_string(col[i]));
    }

    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(t);
    delete[] col;
    omniruntime::op::Operator::DeleteOperator(op);

    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, ProjectCastDecimal128ToString)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 3;
    auto *col = MakeDecimals(numRows, 10);
    std::vector<DataTypePtr> vecOfTypes = {Decimal128Type()};

    auto data = new FieldExpr(0, Decimal128Type(38, 0));
    std::string castStr = "CAST";
    std::vector<Expr *> args;
    args.push_back(data);
    auto castExpr = GetFuncExpr(castStr, args, VarcharType(5));

    std::vector<Expr *> exprs = {castExpr};
    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col);

    auto copy = DuplicateVectorBatch(t);
    op->AddInput(copy);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);
    auto *vcVec = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(outputVecBatch->Get(0));
    for (int32_t i = 0; i < numRows; i++) {
        EXPECT_TRUE(vcVec->GetValue(i) == to_string(col[i * 2]));
    }

    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(t);
    delete[] col;
    omniruntime::op::Operator::DeleteOperator(op);

    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, ProjectCastDoubleToString)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 3;
    auto *col = MakeDoubles(numRows, 10);
    std::vector<DataTypePtr> vecOfTypes = {DoubleType()};

    auto data = new FieldExpr(0, DoubleType());
    std::string castStr = "CAST";
    std::vector<Expr *> args;
    args.push_back(data);
    auto castExpr = GetFuncExpr(castStr, args, VarcharType(5));

    std::vector<Expr *> exprs = {castExpr};
    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig(omniruntime::op::OVERFLOW_CONFIG_NULL);
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();

    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col);

    auto copy = DuplicateVectorBatch(t);
    op->AddInput(copy);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);
    string res[] = {"10.0", "11.0", "12.0"};
    auto *vcVec = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(outputVecBatch->Get(0));
    for (int32_t i = 0; i < numRows; i++) {
        EXPECT_TRUE(vcVec->GetValue(i) == res[i]);
    }

    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(t);
    delete[] col;
    omniruntime::op::Operator::DeleteOperator(op);

    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, ProjectCastStringToString)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 1;
    auto strings = new std::string[1];
    strings[0] = "CastStringToString";

    std::string castStr = "CAST";
    auto data1 = new FieldExpr(0, VarcharType(18));
    std::vector<Expr *> args1;
    args1.push_back(data1);
    auto castExpr1 = GetFuncExpr(castStr, args1, VarcharType(1024));

    auto data2 = new FieldExpr(0, VarcharType(18));
    std::vector<Expr *> args2;
    args2.push_back(data2);
    auto castExpr2 = GetFuncExpr(castStr, args2, VarcharType(10));
    std::vector<Expr *> exprs = {castExpr1, castExpr2};

    std::vector<DataTypePtr> vecOfTypes = {VarcharType(18)};
    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();

    auto *t = CreateVectorBatch(inputTypes, numRows, strings);

    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);
    string expected1 = "CastStringToString";
    string expected2 = "CastString";

    auto *vcVec1 = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(outputVecBatch->Get(0));
    EXPECT_TRUE(vcVec1->GetValue(0) == expected1);

    auto *vcVec2 = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(outputVecBatch->Get(1));
    EXPECT_TRUE(vcVec2->GetValue(0) == expected2);

    VectorHelper::FreeVecBatch(outputVecBatch);
    omniruntime::op::Operator::DeleteOperator(op);
    delete[] strings;
    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, ProjectCastStrStrZh)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 1;

    std::string castStr = "CAST";
    std::vector<Expr *> args1 = {new FieldExpr(0, VarcharType(18))};
    auto castExpr1 = GetFuncExpr(castStr, args1, VarcharType(1024));

    std::vector<Expr *> args2 = {new FieldExpr(0, VarcharType(18))};
    auto castExpr2 = GetFuncExpr(castStr, args2, VarcharType(7));
    std::vector<Expr *> exprs = {castExpr1, castExpr2};

    std::vector<DataTypePtr> vecOfTypes = {VarcharType(18)};
    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();

    auto *col0 = new string[numRows];
    for (int32_t i = 0; i < numRows; i++) {
        col0[i] = "cast乌s斯侧后解";
    }

    VectorBatch *input = CreateVectorBatch(inputTypes, numRows, col0);

    op->AddInput(input);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);

    auto *expected1 = new string[numRows];
    for (int32_t i = 0; i < numRows; i++) {
        expected1[i] = "cast乌s斯侧后解";
    }
    auto *expected2 = new string[numRows];
    for (int32_t i = 0; i < numRows; i++) {
        expected2[i] = "cast乌s斯";
    }

    DataTypes outputTypes({VarcharType(1024), VarcharType(7)});
    VectorBatch *expectedRet = CreateVectorBatch(outputTypes, numRows, expected1, expected2);

    EXPECT_TRUE(VecBatchMatch(outputVecBatch, expectedRet));

    delete[] col0;
    delete[] expected1;
    delete[] expected2;
    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectedRet);
    omniruntime::op::Operator::DeleteOperator(op);

    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, ProjectCastStrStrWithOverflowConfig)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 1;

    std::string castStr = "CAST";
    std::vector<Expr *> args1 = {new FieldExpr(0, VarcharType(18))};
    auto castExpr1 = GetFuncExpr(castStr, args1, VarcharType(1024));

    std::vector<Expr *> args2 = {new FieldExpr(0, VarcharType(18))};
    auto castExpr2 = GetFuncExpr(castStr, args2, VarcharType(7));
    std::vector<Expr *> exprs = {castExpr1, castExpr2};

    std::vector<DataTypePtr> vecOfTypes = {VarcharType(18)};
    DataTypes inputTypes(vecOfTypes);

    auto overflowConfig = new OverflowConfig(omniruntime::op::OVERFLOW_CONFIG_NULL);
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();

    auto *col0 = new string[numRows];
    for (int32_t i = 0; i < numRows; i++) {
        col0[i] = "cast乌s斯侧后解";
    }

    VectorBatch *input = CreateVectorBatch(inputTypes, numRows, col0);

    op->AddInput(input);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);

    auto *expected1 = new string[numRows];
    for (int32_t i = 0; i < numRows; i++) {
        expected1[i] = "cast乌s斯侧后解";
    }
    auto *expected2 = new string[numRows];
    for (int32_t i = 0; i < numRows; i++) {
        expected2[i] = "cast乌s斯";
    }

    DataTypes outputTypes({VarcharType(1024), VarcharType(7)});
    VectorBatch *expectedRet = CreateVectorBatch(outputTypes, numRows, expected1, expected2);

    EXPECT_TRUE(VecBatchMatch(outputVecBatch, expectedRet));

    delete[] col0;
    delete[] expected1;
    delete[] expected2;
    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectedRet);
    omniruntime::op::Operator::DeleteOperator(op);

    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, ProjectCastIntToDecimal)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 10;
    auto *col1 = MakeInts(numRows);
    std::vector<DataTypePtr> vecOfTypes = {IntType()};

    auto data = new FieldExpr(0, IntType());
    std::string castStr = "CAST";
    std::vector<Expr *> args;
    args.push_back(data);
    auto castExpr = GetFuncExpr(castStr, args, Decimal128Type(38, 0));

    std::vector<Expr *> exprs = {castExpr};
    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col1);

    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);
    for (int32_t i = 0; i < numRows; i++) {
        Decimal128 val0 = (reinterpret_cast<Vector<Decimal128> *>(outputVecBatch->Get(0)))->GetValue(i);
        EXPECT_EQ(val0.HighBits(), 0);
        EXPECT_EQ(val0.LowBits(), i);
    }

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] col1;
    omniruntime::op::Operator::DeleteOperator(op);

    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, ProjectCastIntToDecimal64)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 3;
    auto *col = new int[3]{123, 312, 456};
    std::vector<DataTypePtr> vecOfTypes = {IntType()};

    auto data = new FieldExpr(0, IntType());
    std::string castStr = "CAST";
    std::vector<Expr *> args;
    args.push_back(data);
    auto castExpr = GetFuncExpr(castStr, args, Decimal64Type(18, 2));

    std::vector<Expr *> exprs = {castExpr};
    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col);

    auto copy = DuplicateVectorBatch(t);
    op->AddInput(copy);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);
    for (int32_t i = 0; i < numRows; i++) {
        int64_t val0 = (reinterpret_cast<Vector<int64_t> *>(outputVecBatch->Get(0)))->GetValue(i);
        EXPECT_EQ(val0, col[i] * 100);
    }

    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(t);
    delete[] col;
    omniruntime::op::Operator::DeleteOperator(op);

    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, ProjectSparkConfig)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    std::string castStr = "CAST";
    std::vector<Expr *> argLeft{new FieldExpr(0, Decimal64Type(7, 0))};
    FuncExpr *subLeft = GetFuncExpr(castStr, argLeft, Decimal64Type(8, 0));
    auto *col = new int64_t[1];
    col[0] = 123;
    std::vector<Expr *> argRight{new FieldExpr(0, Decimal64Type(7, 0))};
    FuncExpr *subRight = GetFuncExpr(castStr, argRight, Decimal64Type(8, 0));
    auto *addExprs = new BinaryExpr(omniruntime::expressions::Operator::ADD, subLeft, subRight, Decimal64Type(8, 0));

    std::vector<Expr *> exprs = {addExprs};
    std::vector<DataTypePtr> vecOfTypes = {Decimal64Type()};
    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig(OVERFLOW_CONFIG_NULL);
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();

    VectorBatch *t = CreateVectorBatch(inputTypes, 1, col);
    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);
    int64_t val0 = (reinterpret_cast<Vector<int64_t> *>(outputVecBatch->Get(0)))->GetValue(0);
    EXPECT_EQ(val0, 246);

    VectorHelper::FreeVecBatch(outputVecBatch);
    omniruntime::op::Operator::DeleteOperator(op);

    delete[] col;
    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, ProjectMulDecimal64)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    auto *col = new int64_t[1];
    col[0] = 999999;
    auto *mulLeft = new FieldExpr(0, Decimal64Type(7, 0));
    auto *mulRight = new FieldExpr(0, Decimal64Type(7, 0));
    auto *mulExprs1 = new BinaryExpr(omniruntime::expressions::Operator::MUL, mulLeft, mulRight, Decimal64Type(7, 0));
    auto *mulRight2 = new FieldExpr(0, Decimal64Type(7, 0));
    auto *mulExprs2 =
        new BinaryExpr(omniruntime::expressions::Operator::MUL, mulExprs1, mulRight2, Decimal64Type(7, 0));

    std::vector<Expr *> exprs = {mulExprs2};
    std::vector<DataTypePtr> vecOfTypes = {Decimal64Type()};
    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig(OVERFLOW_CONFIG_NULL);
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();

    VectorBatch *t = CreateVectorBatch(inputTypes, 1, col);
    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);
    bool isNull = (reinterpret_cast<Vector<int64_t> *>(outputVecBatch->Get(0)))->IsNull(0);
    EXPECT_EQ(isNull, true);

    VectorHelper::FreeVecBatch(outputVecBatch);
    omniruntime::op::Operator::DeleteOperator(op);

    delete[] col;
    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, AddDecimal)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 2;
    auto *col1 = new int64_t[4]{1, 0, 2, 0};
    auto *col2 = new int64_t[2]{1, 2};
    auto *right1 = new FieldExpr(0, Decimal128Type(38, 0));
    auto *left1 = new FieldExpr(0, Decimal128Type(38, 0));
    auto *addExpr1 = new BinaryExpr(omniruntime::expressions::Operator::ADD, right1, left1, Decimal128Type(38, 0));
    auto *right2 = new FieldExpr(1, Decimal64Type(18, 0));
    auto *left2 = new FieldExpr(1, Decimal64Type(18, 0));
    auto *addExpr2 = new BinaryExpr(omniruntime::expressions::Operator::ADD, right2, left2, Decimal128Type(38, 0));
    auto *right3 = new FieldExpr(1, Decimal64Type(18, 0));
    auto *left3 = new FieldExpr(0, Decimal128Type(38, 0));
    auto *addExpr3 = new BinaryExpr(omniruntime::expressions::Operator::ADD, right3, left3, Decimal128Type(38, 0));
    auto *right4 = new FieldExpr(0, Decimal128Type(38, 0));
    auto *left4 = new FieldExpr(1, Decimal64Type(18, 0));
    auto *addExpr4 = new BinaryExpr(omniruntime::expressions::Operator::ADD, right4, left4, Decimal128Type(38, 0));
    std::vector<Expr *> exprs = {addExpr1, addExpr2, addExpr3, addExpr4};

    std::vector<DataTypePtr> vecOfTypes = {Decimal128Type(), Decimal64Type()};
    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig();
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();

    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col1, col2);
    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);
    for (int32_t i = 0; i < numRows; i++) {
        for (int32_t j = 0; i < numRows; i++) {
            Decimal128 val0 = (reinterpret_cast<Vector<Decimal128> *>(outputVecBatch->Get(j)))->GetValue(i);
            EXPECT_EQ(val0.HighBits(), 0);
            EXPECT_EQ(val0.LowBits(), (i + 1) * 2);
        }
    }

    VectorHelper::FreeVecBatch(outputVecBatch);
    omniruntime::op::Operator::DeleteOperator(op);

    delete[] col1;
    delete[] col2;
    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, AddDecimalReturnNull)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 1;
    auto *col1 = new int64_t[2]{-1, INT64_MAX};
    auto *col2 = new int64_t[1]{INT64_MAX};
    auto *right1 = new FieldExpr(0, Decimal128Type(38, 0));
    auto *left1 = new FieldExpr(0, Decimal128Type(38, 0));
    auto *addExpr1 = new BinaryExpr(omniruntime::expressions::Operator::ADD, right1, left1, Decimal128Type(38, 0));
    auto *right2 = new FieldExpr(1, Decimal64Type(18, 0));
    auto *left2 = new FieldExpr(1, Decimal64Type(18, 0));
    auto *addExpr2 = new BinaryExpr(omniruntime::expressions::Operator::ADD, right2, left2, Decimal128Type(38, 0));
    auto *right3 = new FieldExpr(1, Decimal64Type(18, 0));
    auto *left3 = new FieldExpr(0, Decimal128Type(38, 0));
    auto *addExpr3 = new BinaryExpr(omniruntime::expressions::Operator::ADD, right3, left3, Decimal128Type(38, 0));
    auto *right4 = new FieldExpr(0, Decimal128Type(38, 0));
    auto *left4 = new FieldExpr(1, Decimal64Type(18, 0));
    auto *addExpr4 = new BinaryExpr(omniruntime::expressions::Operator::ADD, right4, left4, Decimal128Type(38, 0));
    std::vector<Expr *> exprs = {addExpr1, addExpr2, addExpr3, addExpr4};

    std::vector<DataTypePtr> vecOfTypes = {Decimal128Type(), Decimal64Type()};
    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig(OVERFLOW_CONFIG_NULL);
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();

    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col1, col2);
    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);
    for (int32_t i = 0; i < numRows; i++) {
        bool isNull = (reinterpret_cast<Vector<Decimal128> *>(outputVecBatch->Get(0)))->IsNull(i);
        EXPECT_TRUE(isNull);
        isNull = (reinterpret_cast<Vector<Decimal128> *>(outputVecBatch->Get(1)))->IsNull(i);
        EXPECT_FALSE(isNull);
        isNull = (reinterpret_cast<Vector<Decimal128> *>(outputVecBatch->Get(2)))->IsNull(i);
        EXPECT_TRUE(isNull);
        isNull = (reinterpret_cast<Vector<Decimal128> *>(outputVecBatch->Get(3)))->IsNull(i);
        EXPECT_TRUE(isNull);
    }

    VectorHelper::FreeVecBatch(outputVecBatch);
    omniruntime::op::Operator::DeleteOperator(op);

    delete[] col1;
    delete[] col2;
    delete factory;
    delete overflowConfig;
}

TEST(ProjectionTest, Nulls)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 100;
    auto nullExpr = new LiteralExpr(true, BooleanType());
    nullExpr->isNull = true;
    std::vector<Expr *> exprs = {nullExpr};

    std::vector<DataTypePtr> vecOfTypes = {};
    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig(OVERFLOW_CONFIG_NULL);
    auto exprEvaluator = std::make_shared<ExpressionEvaluator>(exprs, inputTypes, overflowConfig);
    auto *factory = new ProjectionOperatorFactory(move(exprEvaluator));
    omniruntime::op::Operator *op = factory->CreateOperator();

    VectorBatch *t = CreateVectorBatch(inputTypes, numRows);
    op->AddInput(t);
    VectorBatch *outputVecBatch = nullptr;
    op->GetOutput(&outputVecBatch);
    for (int32_t i = 0; i < numRows; i++) {
        EXPECT_TRUE(outputVecBatch->Get(0)->IsNull(i));
    }

    VectorHelper::FreeVecBatch(outputVecBatch);
    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    delete overflowConfig;
}
} // namespace ProjectionTest
