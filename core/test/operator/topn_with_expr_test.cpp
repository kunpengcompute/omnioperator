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
#include "operator/topn/topn_expr.h"
#include "vector/vector_helper.h"
#include "util/test_util.h"

using namespace omniruntime::vec;
using namespace omniruntime::op;
using namespace omniruntime::expressions;
using namespace std;
using namespace TestUtil;

namespace TopnWithExprTest {
TEST(NativeOmniTopNWithExprOperatorTest, TestTopNWithAllExpr)
{
    // construct input data
    const int32_t dataSize = 8;
    const int32_t expectedDataSize = 5;
    const int32_t sortKeyCnt = 2;

    // prepare data
    int32_t data1[dataSize] = {5, 8, 8, 6, 8, 4, 13, 15};
    int64_t data2[dataSize] = {2L, 5L, 3L, 11L, 4L, 3L, 0L, 23L};
    int64_t data3[dataSize] = {5L, 3L, 2L, 6L, 1L, 4L, 7L, 8L};

    DataTypes sourceTypes(std::vector<DataTypePtr>({IntType(), LongType(), LongType()}));
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3);

    int32_t ascendings[sortKeyCnt] = {false, true};
    int32_t nullFirsts[sortKeyCnt] = {false, false};

    FieldExpr *addLeft = new FieldExpr(0, IntType());
    LiteralExpr *addRight = new LiteralExpr(5, IntType());
    BinaryExpr *addExpr = new BinaryExpr(omniruntime::expressions::Operator::ADD, addLeft, addRight, IntType());
    FieldExpr *modLeft = new FieldExpr(2, LongType());
    LiteralExpr *modRight = new LiteralExpr(3L, LongType());
    BinaryExpr *modExpr = new BinaryExpr(omniruntime::expressions::Operator::MOD, modLeft, modRight, LongType());
    std::vector<Expr *> sortExprs = {addExpr, modExpr};
    auto overflowConfig = new OverflowConfig();
    auto topNWithExprOperatorFactory = new TopNWithExprOperatorFactory(
        sourceTypes, expectedDataSize, 0, sortExprs, ascendings, nullFirsts, sortKeyCnt, overflowConfig);
    auto topNWithExprOperator = static_cast<TopNWithExprOperator *>(CreateTestOperator(topNWithExprOperatorFactory));

    topNWithExprOperator->AddInput(vecBatch);
    VectorBatch *outputVecBatch;
    topNWithExprOperator->GetOutput(&outputVecBatch);

    int32_t expData1[dataSize] = {15, 13, 8, 8, 8};
    int64_t expData2[dataSize] = {23, 0, 5, 4, 3};
    int64_t expData3[dataSize] = {8, 7, 3, 1, 2};
    int32_t expData4[dataSize] = {20, 18, 13, 13, 13};
    int64_t expData5[dataSize] = {2, 1, 0, 1, 2};
    DataTypes expectTypes(std::vector<DataTypePtr>({IntType(), LongType(), LongType(), IntType(), LongType()}));
    VectorBatch *expectVectorBatch =
        CreateVectorBatch(expectTypes, expectedDataSize, expData1, expData2, expData3, expData4, expData5);

    EXPECT_TRUE(VecBatchMatch(outputVecBatch, expectVectorBatch));

    Expr::DeleteExprs(sortExprs);
    omniruntime::op::Operator::DeleteOperator(topNWithExprOperator);
    delete topNWithExprOperatorFactory;
    VectorHelper::FreeVecBatch(expectVectorBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
    delete overflowConfig;
}

TEST(NativeOmniTopNWithExprOperatorTest, TestTopNWithPartialExpr)
{
    // construct input data
    const int32_t dataSize = 8;
    const int32_t expectedDataSize = 5;
    const int32_t sortKeyCnt = 2;

    // prepare data
    int32_t data1[dataSize] = {5, 8, 8, 6, 8, 4, 13, 15};
    int64_t data2[dataSize] = {2L, 5L, 3L, 11L, 4L, 3L, 0L, 23L};
    int64_t data3[dataSize] = {5L, 3L, 2L, 6L, 1L, 4L, 7L, 8L};

    DataTypes sourceTypes(std::vector<DataTypePtr>({IntType(), LongType(), LongType()}));
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3);

    int32_t ascendings[sortKeyCnt] = {false, true};
    int32_t nullFirsts[sortKeyCnt] = {false, false};

    FieldExpr *col0 = new FieldExpr(0, IntType());
    FieldExpr *modLeft = new FieldExpr(2, LongType());
    LiteralExpr *modRight = new LiteralExpr(3L, LongType());
    BinaryExpr *modExpr = new BinaryExpr(omniruntime::expressions::Operator::MOD, modLeft, modRight, LongType());
    std::vector<Expr *> sortKeys = {col0, modExpr};
    auto overflowConfig = new OverflowConfig();
    TopNWithExprOperatorFactory *topNWithExprOperatorFactory = new TopNWithExprOperatorFactory(
        sourceTypes, expectedDataSize, 0, sortKeys, ascendings, nullFirsts, sortKeyCnt, overflowConfig);
    TopNWithExprOperator *topNWithExprOperator =
        static_cast<TopNWithExprOperator *>(CreateTestOperator(topNWithExprOperatorFactory));

    topNWithExprOperator->AddInput(vecBatch);
    VectorBatch *outputVecBatch;
    topNWithExprOperator->GetOutput(&outputVecBatch);

    int32_t expData1[dataSize] = {15, 13, 8, 8, 8};
    int64_t expData2[dataSize] = {23, 0, 5, 4, 3};
    int64_t expData3[dataSize] = {8, 7, 3, 1, 2};
    int64_t expData4[dataSize] = {2, 1, 0, 1, 2};
    DataTypes expectTypes(std::vector<DataTypePtr>({IntType(), LongType(), LongType(), LongType()}));
    VectorBatch *expectVectorBatch =
        CreateVectorBatch(expectTypes, expectedDataSize, expData1, expData2, expData3, expData4);

    EXPECT_TRUE(VecBatchMatch(outputVecBatch, expectVectorBatch));

    Expr::DeleteExprs(sortKeys);
    omniruntime::op::Operator::DeleteOperator(topNWithExprOperator);
    delete topNWithExprOperatorFactory;
    VectorHelper::FreeVecBatch(expectVectorBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
    delete overflowConfig;
}

TEST(NativeOmniTopNWithExprOperatorTest, TestTopNWithNoExpr)
{
    // construct input data
    const int32_t dataSize = 8;
    const int32_t expectedDataSize = 5;
    const int32_t sortKeyCnt = 2;

    // prepare data
    int32_t data1[dataSize] = {5, 8, 8, 6, 8, 4, 13, 15};
    int64_t data2[dataSize] = {2L, 5L, 3L, 11L, 4L, 3L, 0L, 23L};
    int64_t data3[dataSize] = {5L, 3L, 2L, 6L, 1L, 4L, 7L, 8L};

    DataTypes sourceTypes(std::vector<DataTypePtr>({IntType(), LongType(), LongType()}));
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3);

    int32_t ascendings[sortKeyCnt] = {false, true};
    int32_t nullFirsts[sortKeyCnt] = {false, false};

    FieldExpr *col0 = new FieldExpr(0, IntType());
    FieldExpr *col2 = new FieldExpr(2, LongType());
    std::vector<Expr *> sortExprs = {col0, col2};
    auto overflowConfig = new OverflowConfig();
    TopNWithExprOperatorFactory *topNWithExprOperatorFactory = new TopNWithExprOperatorFactory(
        sourceTypes, expectedDataSize, 0, sortExprs, ascendings, nullFirsts, sortKeyCnt, overflowConfig);
    TopNWithExprOperator *topNWithExprOperator =
        static_cast<TopNWithExprOperator *>(CreateTestOperator(topNWithExprOperatorFactory));

    topNWithExprOperator->AddInput(vecBatch);
    VectorBatch *outputVecBatch;
    topNWithExprOperator->GetOutput(&outputVecBatch);

    int32_t expData1[dataSize] = {15, 13, 8, 8, 8};
    int64_t expData2[dataSize] = {23, 0, 4, 3, 5};
    int64_t expData3[dataSize] = {8, 7, 1, 2, 3};
    DataTypes expectTypes(std::vector<DataTypePtr>({IntType(), LongType(), LongType()}));
    VectorBatch *expectVectorBatch = CreateVectorBatch(expectTypes, expectedDataSize, expData1, expData2, expData3);

    EXPECT_TRUE(VecBatchMatch(outputVecBatch, expectVectorBatch));

    Expr::DeleteExprs(sortExprs);
    omniruntime::op::Operator::DeleteOperator(topNWithExprOperator);
    delete topNWithExprOperatorFactory;
    VectorHelper::FreeVecBatch(expectVectorBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
    delete overflowConfig;
}

TEST(NativeOmniTopNWithExprOperatorTest, TestTopNWithExprAndOffset)
{
    // construct input data
    const int32_t dataSize = 8;
    const int32_t limitDataSize = 5;
    const int32_t sortKeyCnt = 2;
    const int32_t offset = 2;
    const int32_t expectedDataSize = limitDataSize - offset;

    // prepare data
    int32_t data1[dataSize] = {5, 8, 8, 6, 8, 4, 13, 15};
    int64_t data2[dataSize] = {2L, 5L, 3L, 11L, 4L, 3L, 0L, 23L};
    int64_t data3[dataSize] = {5L, 3L, 2L, 6L, 1L, 4L, 7L, 8L};

    DataTypes sourceTypes(std::vector<DataTypePtr>({IntType(), LongType(), LongType()}));
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3);

    int32_t ascendings[sortKeyCnt] = {false, true};
    int32_t nullFirsts[sortKeyCnt] = {false, false};

    FieldExpr *col0 = new FieldExpr(0, IntType());
    FieldExpr *col2 = new FieldExpr(2, LongType());
    std::vector<Expr *> sortExprs = {col0, col2};
    auto overflowConfig = new OverflowConfig();
    TopNWithExprOperatorFactory *topNWithExprOperatorFactory = new TopNWithExprOperatorFactory(
        sourceTypes, limitDataSize, offset, sortExprs, ascendings, nullFirsts, sortKeyCnt, overflowConfig);
    TopNWithExprOperator *topNWithExprOperator =
        static_cast<TopNWithExprOperator *>(CreateTestOperator(topNWithExprOperatorFactory));

    topNWithExprOperator->AddInput(vecBatch);
    VectorBatch *outputVecBatch;
    topNWithExprOperator->GetOutput(&outputVecBatch);

    int32_t expData1[dataSize] = {8, 8, 8};
    int64_t expData2[dataSize] = {4, 3, 5};
    int64_t expData3[dataSize] = {1, 2, 3};
    DataTypes expectTypes(std::vector<DataTypePtr>({IntType(), LongType(), LongType()}));
    VectorBatch *expectVectorBatch = CreateVectorBatch(expectTypes, expectedDataSize, expData1, expData2, expData3);

    EXPECT_TRUE(VecBatchMatch(outputVecBatch, expectVectorBatch));

    Expr::DeleteExprs(sortExprs);
    omniruntime::op::Operator::DeleteOperator(topNWithExprOperator);
    delete topNWithExprOperatorFactory;
    VectorHelper::FreeVecBatch(expectVectorBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
    delete overflowConfig;
}
} // namespace TopnWithExprTest
