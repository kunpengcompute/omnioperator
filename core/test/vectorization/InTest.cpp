/*
* Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: vectorization expression 'in' test
 */

#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include <vector>

#include "test/util/test_util.h"
#include "vectorization/ExprEval.h"
#include "expression/expressions.h"
#include "type/decimal_operations.h"
#include "vectorization/registration/SimpleFunctionRegistry.h"
#include "expression/parserhelper.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::mem;
using namespace omniruntime::op;
using namespace omniruntime::expressions;
using namespace omniruntime::TestUtil;

TEST(VectorizationTest, InExprInt32Test)
{
    int rowSize = 5;
    auto intType = std::make_shared<DataType>(OMNI_INT);

    expressions::InExpr* inExpr = new expressions::InExpr({
        new expressions::FieldExpr(0, intType),
        new expressions::LiteralExpr(2, intType),
        new expressions::LiteralExpr(4, intType),
        new expressions::LiteralExpr(6, intType)
    });

    int32_t col0[rowSize] = {1, 2, 3, 4, 5};
    std::vector<DataTypePtr> vecOfTypes = {intType};
    DataTypes inputTypes(vecOfTypes);
    VectorBatch* inputBatch = CreateVectorBatch(inputTypes, rowSize, col0);

    ExecutionContext* context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval exprEval(inputBatch, context);
    exprEval.VisitExpr(*inExpr);
    BaseVector* resultVec = exprEval.GetResult();

    auto* boolResult = static_cast<Vector<bool>*>(resultVec);
    std::vector<bool> expected = {false, true, false, true, false};
    for (int i = 0; i < rowSize; i++) {
        bool actual = boolResult->GetValue(i);
        bool exp = expected[i];
        std::cout << "Int Value:" << col0[i] << ", Actual Result=" << actual << ", Expected Result=" << exp << std::endl;
        ASSERT_EQ(actual, exp) << "Int Value In Expression Line " << i << " is Wrong";
    }
    delete resultVec;
    delete inputBatch;
    delete inExpr;
    delete context;
}

#ifdef STRINGVIEW_ENABLE
TEST(VectorizationTest, InExprStringViewTest)
{
    int rowSize = 5;
    auto varcharType = std::make_shared<DataType>(OMNI_VARCHAR);

    expressions::InExpr* inExpr = new expressions::InExpr({
        new expressions::FieldExpr(0, varcharType),
        new expressions::LiteralExpr(new std::string("apple"), varcharType),
        new expressions::LiteralExpr(new std::string("banana"), varcharType),
        new expressions::LiteralExpr(new std::string("cherry"), varcharType)
    });

    std::vector<std::string> col0 = {"apple", "orange", "banana", "grape", ""};
    std::vector<bool> expected = {true, false, true, false, false};

    std::vector<DataTypePtr> vecOfTypes = {varcharType};
    DataTypes inputTypes(vecOfTypes);
    VectorBatch* inputBatch = CreateVectorBatch(inputTypes, rowSize, col0.data());

    ExecutionContext* context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval exprEval(inputBatch, context);
    exprEval.VisitExpr(*inExpr);
    BaseVector* resultVec = exprEval.GetResult();

    auto* boolResult = static_cast<Vector<bool>*>(resultVec);
    for (int i = 0; i < rowSize; i++) {
        bool actual = boolResult->GetValue(i);
        bool exp = expected[i];
        std::cout << "String Value:\"" << col0[i] << "\", Actual Result=" << actual << ", Expected Result=" << exp << std::endl;
        ASSERT_EQ(actual, exp) << "String Value In Expression Line " << i << " is Wrong";
    }
    delete resultVec;
    delete inputBatch;
    delete inExpr;
    delete context;
}
#endif
