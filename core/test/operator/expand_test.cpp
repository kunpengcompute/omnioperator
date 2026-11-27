/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: expand operator test
 */

#include <string>
#include <vector>
#include <chrono>
#include "gtest/gtest.h"
#include "operator/expand/expand.h"
#include "codegen/bloom_filter.h"
#include "util/test_util.h"
#include "util/config_util.h"
#include <thread>

using namespace omniruntime::op;
using namespace omniruntime::vec;
using namespace omniruntime::expressions;
using namespace std;
using namespace TestUtil;

namespace ExpandTest {
    void VerifyOutput(omniruntime::op::Operator *op,
                      VectorBatch *&outputVecBatch,
                      const int32_t numRows,
                      int32_t valueOffset)
    {
        op->GetOutput(&outputVecBatch);
        auto vec = reinterpret_cast<Vector<int32_t> *>(outputVecBatch->Get(0));
        for (int32_t i = 0; i < numRows; ++i) {
            int32_t val = vec->GetValue(i);
            bool isNull = vec->IsNull(i);
            if (i % 2 == 0) {
                EXPECT_EQ(val, i + valueOffset);
                EXPECT_FALSE(isNull);
            } else {
                EXPECT_TRUE(isNull);
            }
        }
        VectorHelper::FreeVecBatch(outputVecBatch);
        outputVecBatch = nullptr;
    }

TEST(ExpandTest, Simple)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);
    const int32_t numRows = 1000;
    int32_t *col = MakeInts(numRows);
    auto *filed0 = new FieldExpr(0, IntType());
    auto *addLeft = new FieldExpr(0, IntType());
    auto *addRight = new LiteralExpr(5, IntType());
    auto *addExpr = new BinaryExpr(omniruntime::expressions::Operator::ADD, addLeft, addRight, IntType());
    std::vector<Expr *> exprs = {addExpr, filed0};
    std::vector<DataTypePtr> vecOfTypes = { IntType() };
    DataTypes inputTypes(vecOfTypes);
    auto overflowConfig = new OverflowConfig();
    std::vector<std::shared_ptr<ExpressionEvaluator>> exprEvaluators;
    for (const auto& expr : exprs) {
        std::vector<Expr *> projection = {expr};
        auto exprEvaluator = std::make_shared<ExpressionEvaluator>(projection, inputTypes, overflowConfig);
        exprEvaluators.push_back(exprEvaluator);
    }
    auto *factory = new ExpandOperatorFactory(move(exprEvaluators));
    omniruntime::op::Operator *op = factory->CreateOperator(nullptr);
    VectorBatch *t = CreateVectorBatch(inputTypes, numRows, col);

    for (int32_t i = 0; i < numRows; i++) {
        if (i % 2 != 0) {
            t->Get(0)->SetNull(i);
        }
    }

    op->AddInput(t);

    VectorBatch *outputVecBatch = nullptr;

    VerifyOutput(op, outputVecBatch, numRows, 5);

    VerifyOutput(op, outputVecBatch, numRows, 0);

    VectorHelper::FreeVecBatch(outputVecBatch);
    delete[] col;
    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    delete overflowConfig;
}
}