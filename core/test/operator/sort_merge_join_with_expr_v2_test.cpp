/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: ...
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
using JoinSetupResult = std::tuple<
VectorBatch*,
omniruntime::op::Operator*,
omniruntime::op::Operator*,
StreamedTableWithExprOperatorFactoryV2*,
BufferedTableWithExprOperatorFactoryV2*,
OverflowConfig*,
std::vector<Expr*>,
std::vector<Expr*>,
DataTypes,
DataTypes
>;

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
    std::vector<DataTypePtr> streamTypeVector = { IntType(), LongType() };
    DataTypes streamedTblTypes(streamTypeVector);
    auto *col0 = new FieldExpr(0, IntType());
    std::vector<Expr *> streamedEqualKeyExprs = { col0 };
    int streamedOutputCols[1] = {1};
    auto *overflowConfig = new OverflowConfig();

    auto *streamedWithExprOperatorFactory = StreamedTableWithExprOperatorFactoryV2::CreateStreamedTableWithExprOperatorFactoryV2(
        streamedTblTypes, streamedEqualKeyExprs, 1, streamedOutputCols, 1,
        JoinType::OMNI_JOIN_TYPE_INNER, filterJsonStr, overflowConfig);
    auto *streamedTblWithExprOperator = CreateTestOperator(streamedWithExprOperatorFactory);

    // Setup buffered table
    std::vector<DataTypePtr> bufferTypesVector = { DoubleType(), IntType() };
    DataTypes bufferedTblTypes(bufferTypesVector);
    auto *col1 = new FieldExpr(1, IntType());
    std::vector<Expr *> bufferedEqualKeyExprs = { col1 };
    int bufferedOutputCols[1] = {0};

    auto streamedWithExprOperatorFactoryAddr = reinterpret_cast<int64_t>(streamedWithExprOperatorFactory);
    auto *bufferedWithExprOperatorFactory = BufferedTableWithExprOperatorFactoryV2::CreateBufferedTableWithExprOperatorFactoryV2(
        bufferedTblTypes, bufferedEqualKeyExprs, 1, bufferedOutputCols, 1,
        streamedWithExprOperatorFactoryAddr, overflowConfig);
    auto *bufferedTblWithExprOperator = CreateTestOperator(bufferedWithExprOperatorFactory);
    VectorBatch *result;

    return {result, streamedTblWithExprOperator, bufferedTblWithExprOperator,
            streamedWithExprOperatorFactory, bufferedWithExprOperatorFactory,
            overflowConfig, streamedEqualKeyExprs, bufferedEqualKeyExprs,
            streamedTblTypes, bufferedTblTypes};
}

JoinSetupResult SetupAndExecuteJoin()
{
    auto [result, streamedTblWithExprOperator, bufferedTblWithExprOperator,
        streamedWithExprOperatorFactory, bufferedWithExprOperatorFactory,
        overflowConfig, streamedEqualKeyExprs, bufferedEqualKeyExprs,
        streamedTblTypes, bufferedTblTypes] = SetupOp();
    // Execute join operation
    const int32_t streamedTblDataSize = 6;
    int32_t streamedTblDataCol1[streamedTblDataSize] = {0, 1, 2, 3, 4, 5};
    long streamedTblDataCol2[streamedTblDataSize] = {6600, 5500, 4400, 3300, 2200, 1100};
    VectorBatch *streamedTblVecBatch = CreateVectorBatch(streamedTblTypes, streamedTblDataSize,
                                                         streamedTblDataCol1, streamedTblDataCol2);
    streamedTblWithExprOperator->setNoMoreInput(false);
    bufferedTblWithExprOperator->setNoMoreInput(false);
    SetOpStatus(streamedTblWithExprOperator, bufferedTblWithExprOperator);

    streamedTblWithExprOperator->AddInput(streamedTblVecBatch);
    SetOpStatus(streamedTblWithExprOperator, bufferedTblWithExprOperator);

    const int32_t bufferedTblSize = 6;
    double bufferedTblDataCol1[bufferedTblSize] = {6.6, 5.5, 4.4, 3.3, 2.2, 1.1};
    int32_t bufferedTblDataCol2[bufferedTblSize] = {0, 1, 2, 3, 4, 5};
    VectorBatch *bufferedTblVecBatch = CreateVectorBatch(bufferedTblTypes, bufferedTblSize,
                                                         bufferedTblDataCol1, bufferedTblDataCol2);
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatch);
    SetOpStatus(streamedTblWithExprOperator, bufferedTblWithExprOperator);

    bufferedTblWithExprOperator->noMoreInput();
    streamedTblWithExprOperator->noMoreInput();
    SetOpStatus(streamedTblWithExprOperator, bufferedTblWithExprOperator);

    streamedTblWithExprOperator->GetOutput(&result);
    SetOpStatus(streamedTblWithExprOperator, bufferedTblWithExprOperator);
    streamedTblWithExprOperator->noMoreInput();

    return {result, streamedTblWithExprOperator, bufferedTblWithExprOperator,
            streamedWithExprOperatorFactory, bufferedWithExprOperatorFactory,
            overflowConfig, streamedEqualKeyExprs, bufferedEqualKeyExprs,
            streamedTblTypes, bufferedTblTypes};
}

void VerifyJoinResults(VectorBatch *result, omniruntime::op::Operator *streamedTblWithExprOperator,
                       omniruntime::op::Operator *bufferedTblWithExprOperator,
                       StreamedTableWithExprOperatorFactoryV2 *streamedWithExprOperatorFactory,
                       BufferedTableWithExprOperatorFactoryV2 *bufferedWithExprOperatorFactory,
                       OverflowConfig *overflowConfig,
                       std::vector<Expr *> streamedEqualKeyExprs,
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
    auto [result, streamedTblWithExprOperator, bufferedTblWithExprOperator,
        streamedWithExprOperatorFactory, bufferedWithExprOperatorFactory,
        overflowConfig, streamedEqualKeyExprs, bufferedEqualKeyExprs,
        streamedTblTypes, bufferedTblTypes] = SetupAndExecuteJoin();

    // Verify the results
    VerifyJoinResults(result, streamedTblWithExprOperator, bufferedTblWithExprOperator,
            streamedWithExprOperatorFactory, bufferedWithExprOperatorFactory,
            overflowConfig, streamedEqualKeyExprs, bufferedEqualKeyExprs);
}

void DeleteSMJOperatorFactory(StreamedTableWithExprOperatorFactoryV2 *streamedTableWithExprOperatorFactoryV2,
                              BufferedTableWithExprOperatorFactoryV2 *bufferedTableWithExprOperatorFactoryV2)
{
    delete streamedTableWithExprOperatorFactoryV2;
    delete bufferedTableWithExprOperatorFactoryV2;
}

std::tuple<std::shared_ptr<const MergeJoinNode>, FieldExpr *, FieldExpr *> ConstructSimpleJoinKeyJoinNode(JoinType joinType, bool nullAware, bool isShuffle, ExprPtr filter)
{
    std::vector<ExprPtr> leftKeys;
    std::vector<ExprPtr> rightKeys;
    leftKeys.reserve(1);
    rightKeys.reserve(1);
    auto leftKey = new FieldExpr(0, LongType());
    auto rightKey = new FieldExpr(0, LongType());
    leftKeys.emplace_back(leftKey);
    rightKeys.emplace_back(rightKey);

    DataTypes probeTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    DataTypes buildTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    std::vector<omniruntime::expressions::Expr*> partitionKeys;
    int index = 0;
    for (; index < probeTypes.GetSize(); index++) {
        partitionKeys.push_back(new FieldExpr(index, probeTypes.GetType(index)));
    }
    for (; index < buildTypes.GetSize() + probeTypes.GetSize(); index++) {
        partitionKeys.push_back(new FieldExpr(index, buildTypes.GetType(index - probeTypes.GetSize())));
    }

    return {std::make_shared<const MergeJoinNode>("0", joinType, BuildSide::OMNI_BUILD_LEFT, leftKeys, rightKeys, filter, nullptr, nullptr,
        probeTypes.Instance(), buildTypes.Instance(), partitionKeys), leftKey, rightKey};
}

VectorBatch *ConstructSimpleStreamedVectorBatch()
{
    const int32_t dataSize = 10;
    DataTypes buildTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    int64_t buildData0[dataSize] = {1, 1, 1, 2, 2, 3, 4, 5, 6, 7};
    int64_t buildData1[dataSize] = {70, 70, 79, 79, 70, 70, 70, 70, 70, 70};
    return CreateVectorBatch(buildTypes, dataSize, buildData0, buildData1);
}

VectorBatch *ConstructSimpleBufferedVectorBatch()
{
    const int32_t dataSize = 10;
    DataTypes probeTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    int64_t probeData0[] = {1, 1, 1, 2, 2, 3, 3, 4, 5, 6};
    int64_t probeData1[] = {78, 78, 82, 78, 82, 78, 65, 78, 78, 78};
    return CreateVectorBatch(probeTypes, dataSize, probeData0, probeData1);
}

VectorBatch *ConstructSimpleExpectedVectorBatch()
{
    const uint32_t originalDataSize = 10;
    const uint32_t expectedDataSize = 18;

    int64_t expectedData0[originalDataSize] = {1, 2, 3, 4, 5, 6, 1, 1, 2, 3};
    int64_t expectedData1[originalDataSize] = {78, 78, 78, 78, 78, 78, 78, 82, 82, 65};
    int64_t expectedData2[expectedDataSize] = {1, 1, 1, 2, 2, 3, 4, 5, 6, 1, 1, 1, 1, 1, 1, 2, 2, 3};
    int64_t expectedData3[expectedDataSize] = {79, 70, 70, 79, 70, 70, 70, 70, 70, 79, 70, 70, 79, 70, 70, 79, 70, 70};

    auto inputType = IntType();
    auto expectedVec0 = CreateVector<int64_t>(originalDataSize, expectedData0);
    auto expectedVec1 = CreateVector<int64_t>(originalDataSize, expectedData1);
    int32_t ids[expectedDataSize] = {0, 0, 0, 1, 1, 2, 3, 4, 5, 6, 6, 6, 7, 7, 7, 8, 8, 9};
    auto expectedDictVec0 =
        VectorHelper::CreateDictionary(ids, expectedDataSize, reinterpret_cast<Vector<int64_t> *>(expectedVec0));
    auto expectedDictVec1 =
        VectorHelper::CreateDictionary(ids, expectedDataSize, reinterpret_cast<Vector<int64_t> *>(expectedVec1));
    auto expectedVec2 = CreateVector<int64_t>(expectedDataSize, expectedData2);
    auto expectedVec3 = CreateVector<int64_t>(expectedDataSize, expectedData3);
    delete expectedVec0;
    delete expectedVec1;

    auto *vectorBatch = new VectorBatch(expectedDataSize);
    vectorBatch->Append(expectedDictVec0);
    vectorBatch->Append(expectedDictVec1);
    vectorBatch->Append(expectedVec2);
    vectorBatch->Append(expectedVec3);
    return vectorBatch;
}

TEST(SMJ_JOIN_OPERATOR_WITH_EXPR_V2_TESTCASE, TestInnerEqualityJoinWithOneBuildOp)
{
    auto *queryConfig = new config::QueryConfig();
    auto [joinNode, leftKey, rightKey] = ConstructSimpleJoinKeyJoinNode(OMNI_JOIN_TYPE_INNER, false, true, nullptr);
    StreamedTableWithExprOperatorFactoryV2 *streamedTableWithExprOperatorFactoryV2 = StreamedTableWithExprOperatorFactoryV2::CreateStreamedTableWithExprOperatorFactoryV2(joinNode, *queryConfig);
    auto *streamedTblWithExprOperator = dynamic_cast<StreamedTableWithExprOperatorV2 *>(streamedTableWithExprOperatorFactoryV2->CreateOperator(nullptr));

    BufferedTableWithExprOperatorFactoryV2 *bufferedTableWithExprOperatorFactoryV2 = BufferedTableWithExprOperatorFactoryV2::CreateBufferedTableWithExprOperatorFactoryV2(joinNode, reinterpret_cast<int64_t>(streamedTableWithExprOperatorFactoryV2), *queryConfig);
    auto *bufferedTblWithExprOperator = dynamic_cast<BufferedTableWithExprOperatorV2 *>(bufferedTableWithExprOperatorFactoryV2->CreateOperator(nullptr));


    VectorBatch *bufferedVecBatch = ConstructSimpleBufferedVectorBatch();
    bufferedTblWithExprOperator->AddInput(bufferedVecBatch);
    VectorBatch *streamedVecBatch = ConstructSimpleStreamedVectorBatch();
    streamedTblWithExprOperator->AddInput(streamedVecBatch);
    bufferedTblWithExprOperator->noMoreInput();
    streamedTblWithExprOperator->noMoreInput();
    bufferedTblWithExprOperator->noMoreInput();
    streamedTblWithExprOperator->noMoreInput();

    VectorBatch *outputVecBatch = nullptr;
    streamedTblWithExprOperator->GetOutput(&outputVecBatch);

    VectorBatch *expectVecBatch = ConstructSimpleExpectedVectorBatch();
    BaseVector **pVector = outputVecBatch->GetVectors();
    std::rotate(pVector, pVector + 2, pVector + outputVecBatch->GetVectorCount());
    EXPECT_EQ(outputVecBatch->GetRowCount(), expectVecBatch->GetRowCount());
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecBatch));

    delete queryConfig;
    delete leftKey;
    delete rightKey;
    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectVecBatch);
    omniruntime::op::Operator::DeleteOperator(streamedTblWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(bufferedTblWithExprOperator);
    DeleteSMJOperatorFactory(streamedTableWithExprOperatorFactoryV2, bufferedTableWithExprOperatorFactoryV2);
}
}