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
#include <vector>
#include "gtest/gtest.h"
#include "vector/vector_helper.h"
#include "operator/join/sortmergejoin/sort_merge_join_expr_v2.h"
#include "util/test_util.h"

using namespace omniruntime::op;
using namespace omniruntime::expressions;
using namespace std;
using namespace TestUtil;

namespace SortMergeJoinWithExprV2Test {
// Type alias at class scope or in a header
using JoinSetupResult = std::tuple<VectorBatch *, omniruntime::op::Operator *, omniruntime::op::Operator *,
                                   StreamedTableWithExprOperatorFactoryV2 *, BufferedTableWithExprOperatorFactoryV2 *,
                                   OverflowConfig *, std::vector<Expr *>, std::vector<Expr *>, DataTypes, DataTypes>;

void SetOpStatus(omniruntime::op::Operator *streamedTblWithExprOperator,
                 omniruntime::op::Operator *bufferedTblWithExprOperator)
{
    bufferedTblWithExprOperator->IsBlocked(nullptr);
    bufferedTblWithExprOperator->needsInput();
    streamedTblWithExprOperator->IsBlocked(nullptr);
    streamedTblWithExprOperator->needsInput();
}

JoinSetupResult SetupOp()
{
    string filterJsonStr = "{\"exprType\":\"BINARY\","
                           "\"returnType\":4,"
                           "\"operator\":\"GREATER_THAN\","
                           "\"left\":{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":1,\"colVal\":0},"
                           "\"right\":{\"exprType\":\"LITERAL\",\"dataType\":1, \"isNull\":false, \"value\":2}}";

    // Setup streamed table
    std::vector<DataTypePtr> streamTypeVector = {IntType(), LongType()};
    DataTypes streamedTblTypes(streamTypeVector);
    auto *col0 = new FieldExpr(0, IntType());
    std::vector<Expr *> streamedEqualKeyExprs = {col0};
    int streamedOutputCols[1] = {1};
    auto *overflowConfig = new OverflowConfig();

    auto *streamedWithExprOperatorFactory =
        StreamedTableWithExprOperatorFactoryV2::CreateStreamedTableWithExprOperatorFactoryV2(
            streamedTblTypes, streamedEqualKeyExprs, 1, streamedOutputCols, 1, JoinType::OMNI_JOIN_TYPE_INNER,
            filterJsonStr, overflowConfig);
    auto *streamedTblWithExprOperator = CreateTestOperator(streamedWithExprOperatorFactory);

    // Setup buffered table
    std::vector<DataTypePtr> bufferTypesVector = {DoubleType(), IntType()};
    DataTypes bufferedTblTypes(bufferTypesVector);
    auto *col1 = new FieldExpr(1, IntType());
    std::vector<Expr *> bufferedEqualKeyExprs = {col1};
    int bufferedOutputCols[1] = {0};

    auto streamedWithExprOperatorFactoryAddr = reinterpret_cast<int64_t>(streamedWithExprOperatorFactory);
    auto *bufferedWithExprOperatorFactory =
        BufferedTableWithExprOperatorFactoryV2::CreateBufferedTableWithExprOperatorFactoryV2(
            bufferedTblTypes, bufferedEqualKeyExprs, 1, bufferedOutputCols, 1, streamedWithExprOperatorFactoryAddr,
            overflowConfig);
    auto *bufferedTblWithExprOperator = CreateTestOperator(bufferedWithExprOperatorFactory);
    VectorBatch *result;

    return {result,
            streamedTblWithExprOperator,
            bufferedTblWithExprOperator,
            streamedWithExprOperatorFactory,
            bufferedWithExprOperatorFactory,
            overflowConfig,
            streamedEqualKeyExprs,
            bufferedEqualKeyExprs,
            streamedTblTypes,
            bufferedTblTypes};
}

JoinSetupResult SetupAndExecuteJoin()
{
    auto [result, streamedTblWithExprOperator, bufferedTblWithExprOperator, streamedWithExprOperatorFactory,
          bufferedWithExprOperatorFactory, overflowConfig, streamedEqualKeyExprs, bufferedEqualKeyExprs,
          streamedTblTypes, bufferedTblTypes] = SetupOp();
    // Execute join operation
    const int32_t streamedTblDataSize = 6;
    int32_t streamedTblDataCol1[streamedTblDataSize] = {0, 1, 2, 3, 4, 5};
    long streamedTblDataCol2[streamedTblDataSize] = {6600, 5500, 4400, 3300, 2200, 1100};
    VectorBatch *streamedTblVecBatch =
        CreateVectorBatch(streamedTblTypes, streamedTblDataSize, streamedTblDataCol1, streamedTblDataCol2);
    streamedTblWithExprOperator->setNoMoreInput(false);
    bufferedTblWithExprOperator->setNoMoreInput(false);
    SetOpStatus(streamedTblWithExprOperator, bufferedTblWithExprOperator);

    streamedTblWithExprOperator->AddInput(streamedTblVecBatch);
    SetOpStatus(streamedTblWithExprOperator, bufferedTblWithExprOperator);

    const int32_t bufferedTblSize = 6;
    double bufferedTblDataCol1[bufferedTblSize] = {6.6, 5.5, 4.4, 3.3, 2.2, 1.1};
    int32_t bufferedTblDataCol2[bufferedTblSize] = {0, 1, 2, 3, 4, 5};
    VectorBatch *bufferedTblVecBatch =
        CreateVectorBatch(bufferedTblTypes, bufferedTblSize, bufferedTblDataCol1, bufferedTblDataCol2);
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatch);
    SetOpStatus(streamedTblWithExprOperator, bufferedTblWithExprOperator);

    bufferedTblWithExprOperator->noMoreInput();
    streamedTblWithExprOperator->noMoreInput();
    SetOpStatus(streamedTblWithExprOperator, bufferedTblWithExprOperator);

    streamedTblWithExprOperator->GetOutput(&result);
    SetOpStatus(streamedTblWithExprOperator, bufferedTblWithExprOperator);
    streamedTblWithExprOperator->noMoreInput();

    return {result,
            streamedTblWithExprOperator,
            bufferedTblWithExprOperator,
            streamedWithExprOperatorFactory,
            bufferedWithExprOperatorFactory,
            overflowConfig,
            streamedEqualKeyExprs,
            bufferedEqualKeyExprs,
            streamedTblTypes,
            bufferedTblTypes};
}

void VerifyJoinResults(VectorBatch *result, omniruntime::op::Operator *streamedTblWithExprOperator,
                       omniruntime::op::Operator *bufferedTblWithExprOperator,
                       StreamedTableWithExprOperatorFactoryV2 *streamedWithExprOperatorFactory,
                       BufferedTableWithExprOperatorFactoryV2 *bufferedWithExprOperatorFactory,
                       OverflowConfig *overflowConfig, std::vector<Expr *> streamedEqualKeyExprs,
                       std::vector<Expr *> bufferedEqualKeyExprs)
{
    long resultCol1[] = {3300, 2200, 1100};
    double resultCol2[] = {3.3, 2.2, 1.1};
    int32_t index = 0;
    ASSERT_EQ(result->GetVectorCount(), 2);

    for (auto j = 0; j < result->GetRowCount(); j++) {
        long longValue = (reinterpret_cast<Vector<int64_t> *>(result->Get(0)))->GetValue(j);
        ASSERT_EQ(longValue, resultCol1[index]);

        double doubleValue = (reinterpret_cast<Vector<double> *>(result->Get(1)))->GetValue(j);
        ASSERT_EQ(doubleValue, resultCol2[index]);
        index++;
    }

    VectorHelper::FreeVecBatch(result);
    Expr::DeleteExprs(streamedEqualKeyExprs);
    Expr::DeleteExprs(bufferedEqualKeyExprs);
    omniruntime::op::Operator::DeleteOperator(bufferedTblWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(streamedTblWithExprOperator);
    delete bufferedWithExprOperatorFactory;
    delete streamedWithExprOperatorFactory;
    delete overflowConfig;
}

TEST(SMJ_JOIN_OPERATOR_WITH_EXPR_V2_TESTCASE, testSmjInnerJoinExprGreaterThanCondition)
{
    // Setup and execute the join operation
    auto [result, streamedTblWithExprOperator, bufferedTblWithExprOperator, streamedWithExprOperatorFactory,
          bufferedWithExprOperatorFactory, overflowConfig, streamedEqualKeyExprs, bufferedEqualKeyExprs,
          streamedTblTypes, bufferedTblTypes] = SetupAndExecuteJoin();

    // Verify the results
    VerifyJoinResults(result, streamedTblWithExprOperator, bufferedTblWithExprOperator, streamedWithExprOperatorFactory,
                      bufferedWithExprOperatorFactory, overflowConfig, streamedEqualKeyExprs, bufferedEqualKeyExprs);
}
} // namespace SortMergeJoinWithExprV2Test
