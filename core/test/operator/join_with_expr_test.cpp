/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
 * @Description: sort operator test implementations
 */

#include "gtest/gtest.h"
#include "operator/join/hash_builder_expr.h"
#include "operator/join/lookup_join_expr.h"
#include "operator/join/lookup_outer_join_expr.h"
#include "vector/vector_helper.h"
#include "util/test_util.h"
#include "expression/jsonparser/jsonparser.h"

using namespace omniruntime::op;
using namespace omniruntime::vec;
using namespace omniruntime::expressions;
using namespace std;
using namespace TestUtil;

namespace JoinWithExprTest {
void DeleteJoinExprOperatorFactory(HashBuilderWithExprOperatorFactory *hashBuilderOperatorFactory,
    LookupJoinWithExprOperatorFactory *lookupJoinOperatorFactory,
    LookupOuterJoinWithExprOperatorFactory *lookupOuterJoinWithExprOperatorFactory)
{
    delete hashBuilderOperatorFactory;
    delete lookupJoinOperatorFactory;
    delete lookupOuterJoinWithExprOperatorFactory;
}

void DeleteJoinExprOperatorFactory(HashBuilderWithExprOperatorFactory *hashBuilderOperatorFactory,
    LookupJoinWithExprOperatorFactory *lookupJoinOperatorFactory)
{
    DeleteJoinExprOperatorFactory(hashBuilderOperatorFactory, lookupJoinOperatorFactory, nullptr);
}

std::vector<omniruntime::expressions::Expr *> CreateBuildHashKeys()
{
    auto *addLeft = new omniruntime::expressions::FieldExpr(1, LongType());
    auto *addRight = new omniruntime::expressions::LiteralExpr(50L, LongType());
    auto *addExpr = new omniruntime::expressions::BinaryExpr(omniruntime::expressions::Operator::ADD, addLeft, addRight,
        LongType());
    std::vector<omniruntime::expressions::Expr *> buildHashKeysExprs = { addExpr };
    return buildHashKeysExprs;
}

std::vector<omniruntime::expressions::Expr *> CreateProbeHashKeys()
{
    auto *addLeftProbe = new omniruntime::expressions::LiteralExpr(50L, LongType());
    auto *addRightProbe = new omniruntime::expressions::FieldExpr(1, LongType());
    auto *addExprProbe = new omniruntime::expressions::BinaryExpr(omniruntime::expressions::Operator::ADD, addLeftProbe,
        addRightProbe, LongType());
    std::vector<omniruntime::expressions::Expr *> probeHashKeysExprs = { addExprProbe };
    return probeHashKeysExprs;
}

TEST(JoinWithExprTest, TestInnerEqualityJoinOnKeyWithExpr)
{
    // construct input data
    const int32_t dataSize = 4;
    DataTypes buildTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    int64_t buildData0[] = {1, 2, 3, 4};
    int64_t buildData1[] = {111, 11, 333, 33};
    auto *buildVecBatch = new VectorBatch(dataSize);
    buildVecBatch->Append(CreateVector(dataSize, buildData0));
    int32_t ids[] = {0, 1, 2, 3};
    buildVecBatch->Append(CreateDictionaryVector(*buildTypes.GetType(1), dataSize, ids, dataSize, buildData1));

    std::vector<omniruntime::expressions::Expr *> buildHashKeys = CreateBuildHashKeys();
    int32_t hashTableCount = 1;
    HashBuilderWithExprOperatorFactory *hashBuilderWithExprOperatorFactory =
        HashBuilderWithExprOperatorFactory::CreateHashBuilderWithExprOperatorFactory(OMNI_JOIN_TYPE_INNER, buildTypes,
        buildHashKeys, hashTableCount, nullptr);
    auto *hashBuilderWithExprOperator = CreateTestOperator(hashBuilderWithExprOperatorFactory);
    hashBuilderWithExprOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuilderOutput = nullptr;
    hashBuilderWithExprOperator->GetOutput(&hashBuilderOutput);

    DataTypes probeTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    int64_t probeData0[] = {1, 2, 3, 4};
    int64_t probeData1[] = {11, 22, 33, 44};

    auto *probeVecBatch = new VectorBatch(dataSize);
    probeVecBatch->Append(CreateVector(dataSize, probeData0));
    probeVecBatch->Append(CreateDictionaryVector(*probeTypes.GetType(1), dataSize, ids, dataSize, probeData1));

    int32_t probeOutputCols[2]= {0, 1};
    int32_t probeOutputColsCount = 2;
    std::vector<omniruntime::expressions::Expr *> probeHashKeys = CreateProbeHashKeys();
    int32_t probeHashKeysCount = 1;
    int32_t buildOutputCols[2] = {0, 1};
    int32_t buildOutputColsCount = 2;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    auto overflowConfig = new OverflowConfig();
    HashBuilderOperatorFactory* hashBuilderFactory = hashBuilderWithExprOperatorFactory->GetHashBuilderOperatorFactory();
    auto lookupJoinWithExprOperatorFactory = LookupJoinWithExprOperatorFactory::CreateLookupJoinWithExprOperatorFactory(
        probeTypes, probeOutputCols, probeOutputColsCount, probeHashKeys, probeHashKeysCount, buildOutputCols,
        buildOutputColsCount, buildOutputTypes, (int64_t)hashBuilderFactory, nullptr, false, overflowConfig);
    auto lookupJoinWithExprOperator = CreateTestOperator(lookupJoinWithExprOperatorFactory);
    lookupJoinWithExprOperator->AddInput(probeVecBatch);
    VectorBatch *lookupJoinOutputVecBatch = nullptr;
    lookupJoinWithExprOperator->GetOutput(&lookupJoinOutputVecBatch);

    const int32_t expectedDataSize = 2;
    int64_t expectedDatas[4][expectedDataSize] = {
            {1, 3},
            {11, 33},
            {2, 4},
            {11, 33}};
    std::vector<DataTypePtr> expectedTypes{ LongType(), LongType(), LongType(), LongType() };
    AssertVecBatchEquals(lookupJoinOutputVecBatch, probeTypes.GetSize() + buildOutputColsCount, expectedDataSize,
        expectedDatas[0], expectedDatas[1], expectedDatas[2], expectedDatas[3]);

    Expr::DeleteExprs(buildHashKeys);
    Expr::DeleteExprs(probeHashKeys);
    VectorHelper::FreeVecBatch(lookupJoinOutputVecBatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinWithExprOperator);
    DeleteJoinExprOperatorFactory(hashBuilderWithExprOperatorFactory, lookupJoinWithExprOperatorFactory);
    delete overflowConfig;
}

TEST(JoinWithExprTest, TestInnerEqualityJoinOnKeyWithoutExpr)
{
    // construct input data
    const int32_t dataSize = 4;
    DataTypes buildTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    int64_t buildData0[] = {1, 2, 3, 4};
    int64_t buildData1[] = {111, 11, 333, 33};
    auto *buildVecBatch = new VectorBatch(dataSize);
    buildVecBatch->Append(CreateVector(dataSize, buildData0));
    int32_t ids[] = {0, 1, 2, 3};
    buildVecBatch->Append(CreateDictionaryVector(*buildTypes.GetType(1), dataSize, ids, dataSize, buildData1));

    std::vector<omniruntime::expressions::Expr *> buildHashKeys = { new omniruntime::expressions::FieldExpr(1,
        LongType()) };
    int32_t hashTableCount = 1;
    auto overflowConfig = new OverflowConfig();
    HashBuilderWithExprOperatorFactory *hashBuilderWithExprOperatorFactory =
        HashBuilderWithExprOperatorFactory::CreateHashBuilderWithExprOperatorFactory(OMNI_JOIN_TYPE_INNER, buildTypes,
        buildHashKeys, hashTableCount, overflowConfig);
    auto *hashBuilderWithExprOperator = CreateTestOperator(hashBuilderWithExprOperatorFactory);
    hashBuilderWithExprOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuilderOutput = nullptr;
    hashBuilderWithExprOperator->GetOutput(&hashBuilderOutput);

    DataTypes probeTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    int64_t probeData0[] = {1, 2, 3, 4};
    int64_t probeData1[] = {11, 22, 33, 44};
    auto *probeVecBatch = new VectorBatch(dataSize);
    probeVecBatch->Append(CreateVector(dataSize, probeData0));
    probeVecBatch->Append(CreateDictionaryVector(*probeTypes.GetType(1), dataSize, ids, dataSize, probeData1));

    int32_t probeOutputCols[2]= {0, 1};
    int32_t probeOutputColsCount = 2;
    std::vector<omniruntime::expressions::Expr *> probeHashKeys = { new omniruntime::expressions::FieldExpr(1,
        LongType()) };
    int32_t probeHashKeysCount = 1;
    int32_t buildOutputCols[2] = {0, 1};
    int32_t buildOutputColsCount = 2;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    HashBuilderOperatorFactory* hashBuilderFactory = hashBuilderWithExprOperatorFactory->GetHashBuilderOperatorFactory();
    auto lookupJoinWithExprOperatorFactory = LookupJoinWithExprOperatorFactory::CreateLookupJoinWithExprOperatorFactory(
        probeTypes, probeOutputCols, probeOutputColsCount, probeHashKeys, probeHashKeysCount, buildOutputCols,
        buildOutputColsCount, buildOutputTypes, (int64_t)hashBuilderFactory, nullptr, false, overflowConfig);
    auto lookupJoinWithExprOperator = CreateTestOperator(lookupJoinWithExprOperatorFactory);
    lookupJoinWithExprOperator->AddInput(probeVecBatch);
    VectorBatch *lookupJoinOutputVecBatch = nullptr;
    lookupJoinWithExprOperator->GetOutput(&lookupJoinOutputVecBatch);

    const int32_t expectedDataSize = 2;
    int64_t expectedDatas[4][expectedDataSize] = {
            {1, 3},
            {11, 33},
            {2, 4},
            {11, 33}};
    std::vector<DataTypePtr> expectedTypes{ LongType(), LongType(), LongType(), LongType() };
    AssertVecBatchEquals(lookupJoinOutputVecBatch, probeTypes.GetSize() + buildOutputColsCount, expectedDataSize,
        expectedDatas[0], expectedDatas[1], expectedDatas[2], expectedDatas[3]);

    Expr::DeleteExprs(buildHashKeys);
    Expr::DeleteExprs(probeHashKeys);
    VectorHelper::FreeVecBatch(lookupJoinOutputVecBatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinWithExprOperator);
    DeleteJoinExprOperatorFactory(hashBuilderWithExprOperatorFactory, lookupJoinWithExprOperatorFactory);
    delete overflowConfig;
}

TEST(JoinWithExprTest, TestInnerEqualityJoinOnKeyWithoutExprWithStructVector)
{
    // number of rows
    const int32_t dataSize = 4;

    std::vector<std::shared_ptr<omniruntime::type::DataType>> fieldTypes;
    fieldTypes.push_back(omniruntime::type::LongType());
    fieldTypes.push_back(omniruntime::type::LongType());
    auto StructColType = std::make_shared<omniruntime::type::RowType>(fieldTypes);

    DataTypes buildTypes(std::vector<DataTypePtr>({ LongType(), LongType(), StructColType }));

    int64_t buildData0[] = {1, 2, 3, 4};
    int64_t buildData1[] = {111, 11, 333, 33};
    auto *buildVecBatch = new VectorBatch(dataSize);
    buildVecBatch->Append(CreateVector(dataSize, buildData0));
    int32_t ids[] = {0, 1, 2, 3};
    buildVecBatch->Append(CreateDictionaryVector(*buildTypes.GetType(1), dataSize, ids, dataSize, buildData1));

    auto* buildChildVector0 = new vec::Vector<int64_t>(dataSize); // 0 1 2 3
    auto* buildChildVector1 = new vec::Vector<int64_t>(dataSize); // 1 2 3 4
    for (int i = 0; i < dataSize; i++) {
        buildChildVector0->SetValue(i, i);
        buildChildVector1->SetValue(i, i + 1);
    }
    auto* buildStructVec = new RowVector(dataSize);
    buildStructVec->AddChild(buildChildVector0);
    buildStructVec->AddChild(buildChildVector1);
    buildVecBatch->Append(buildStructVec);

    std::vector<omniruntime::expressions::Expr *> buildHashKeys = { new omniruntime::expressions::FieldExpr(1,
        LongType()) };
    int32_t hashTableCount = 1;
    auto overflowConfig = new OverflowConfig();
    HashBuilderWithExprOperatorFactory *hashBuilderWithExprOperatorFactory =
        HashBuilderWithExprOperatorFactory::CreateHashBuilderWithExprOperatorFactory(OMNI_JOIN_TYPE_INNER, buildTypes,
            buildHashKeys, hashTableCount, overflowConfig);
    auto *hashBuilderWithExprOperator = CreateTestOperator(hashBuilderWithExprOperatorFactory);
    hashBuilderWithExprOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuilderOutput = nullptr;
    hashBuilderWithExprOperator->GetOutput(&hashBuilderOutput);

    DataTypes probeTypes(std::vector<DataTypePtr>({ LongType(), LongType(), StructColType }));
    int64_t probeData0[] = {1, 2, 3, 4};
    int64_t probeData1[] = {11, 22, 33, 44};

    auto* probeChildVector0 = new vec::Vector<int64_t>(dataSize); // 2 3 4 5
    auto* probeChildVector1 = new vec::Vector<int64_t>(dataSize); // 3 4 5 6
    for (int i = 0; i < dataSize; i++) {
        probeChildVector0->SetValue(i, i + 2);
        probeChildVector1->SetValue(i, i + 3);
    }
    auto* probeStructVec = new RowVector(dataSize);
    probeStructVec->AddChild(probeChildVector0);
    probeStructVec->AddChild(probeChildVector1);

    auto *probeVecBatch = new VectorBatch(dataSize);
    probeVecBatch->Append(CreateVector(dataSize, probeData0));
    probeVecBatch->Append(CreateDictionaryVector(*probeTypes.GetType(1), dataSize, ids, dataSize, probeData1));
    probeVecBatch->Append(probeStructVec);

    int32_t probeOutputCols[3]= {0, 1, 2};
    int32_t probeOutputColsCount = 3;
    std::vector<omniruntime::expressions::Expr *> probeHashKeys = { new omniruntime::expressions::FieldExpr(1,
        LongType()) };
    int32_t probeHashKeysCount = 1;
    int32_t buildOutputCols[3] = {0, 1, 2};
    int32_t buildOutputColsCount = 3;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({ LongType(), LongType(), StructColType }));
    HashBuilderOperatorFactory* hashBuilderFactory = hashBuilderWithExprOperatorFactory->GetHashBuilderOperatorFactory();
    auto lookupJoinWithExprOperatorFactory = LookupJoinWithExprOperatorFactory::CreateLookupJoinWithExprOperatorFactory(
        probeTypes, probeOutputCols, probeOutputColsCount, probeHashKeys, probeHashKeysCount, buildOutputCols,
        buildOutputColsCount, buildOutputTypes, (int64_t)hashBuilderFactory, nullptr, false, overflowConfig);
    auto lookupJoinWithExprOperator = CreateTestOperator(lookupJoinWithExprOperatorFactory);
    lookupJoinWithExprOperator->AddInput(probeVecBatch);
    VectorBatch *lookupJoinOutputVecBatch = nullptr;
    lookupJoinWithExprOperator->GetOutput(&lookupJoinOutputVecBatch);

    const int32_t expectedDataSize = 2;

    int64_t expectedProbeCol0[] = {1, 3};
    int64_t expectedProbeCol1[] = {11, 33};
    auto* expectedProbeChildVector0 = new vec::Vector<int64_t>(expectedDataSize); // 2 3 4 5
    auto* expectedProbeChildVector1 = new vec::Vector<int64_t>(expectedDataSize); // 3 4 5 6
    int64_t expectedProbeChildVectorVals0[2] = {2, 4};
    int64_t expectedProbeChildVectorVals1[2] = {3, 5};
    for (int i = 0; i < expectedDataSize; i++) {
        expectedProbeChildVector0->SetValue(i, expectedProbeChildVectorVals0[i]);
        expectedProbeChildVector1->SetValue(i, expectedProbeChildVectorVals1[i]);
    }
    auto* expectedProbeStructVec = new RowVector(expectedDataSize);
    expectedProbeStructVec->AddChild(expectedProbeChildVector0);
    expectedProbeStructVec->AddChild(expectedProbeChildVector1);

    int64_t expectedBuildCol0[] = {2, 4};
    int64_t expectedBuildCol1[] = {11, 33};
    auto* expectedBuildChildVector0 = new vec::Vector<int64_t>(expectedDataSize); // 0 1 2 3
    auto* expectedBuildChildVector1 = new vec::Vector<int64_t>(expectedDataSize); // 1 2 3 4
    int64_t expectedBuildChildVectorVals0[2] = {1, 3};
    int64_t expectedBuildChildVectorVals1[2] = {2, 4};
    for (int i = 0; i < expectedDataSize; i++) {
        expectedBuildChildVector0->SetValue(i, expectedBuildChildVectorVals0[i]);
        expectedBuildChildVector1->SetValue(i, expectedBuildChildVectorVals1[i]);
    }
    auto* expectedBuildStructVec = new RowVector(expectedDataSize);
    expectedBuildStructVec->AddChild(expectedBuildChildVector0);
    expectedBuildStructVec->AddChild(expectedBuildChildVector1);

    auto *expectedVecBatch = new VectorBatch(expectedDataSize);
    expectedVecBatch->Append(CreateVector(expectedDataSize, expectedProbeCol0));
    expectedVecBatch->Append(CreateVector(expectedDataSize, expectedProbeCol1));
    expectedVecBatch->Append(expectedProbeStructVec);
    expectedVecBatch->Append(CreateVector(expectedDataSize, expectedBuildCol0));
    expectedVecBatch->Append(CreateVector(expectedDataSize, expectedBuildCol1));
    expectedVecBatch->Append(expectedBuildStructVec);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(lookupJoinOutputVecBatch, expectedVecBatch));

    Expr::DeleteExprs(buildHashKeys);
    Expr::DeleteExprs(probeHashKeys);
    VectorHelper::FreeVecBatch(lookupJoinOutputVecBatch);
    VectorHelper::FreeVecBatch(expectedVecBatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinWithExprOperator);
    DeleteJoinExprOperatorFactory(hashBuilderWithExprOperatorFactory, lookupJoinWithExprOperatorFactory);
    delete overflowConfig;
}

TEST(JoinWithExprTest, TestLeftJoinOnKeyWithoutExprWithStructVector)
{
    // construct input data
    const int32_t dataSize = 4;

    std::vector<std::shared_ptr<omniruntime::type::DataType>> fieldTypes;
    fieldTypes.push_back(omniruntime::type::LongType());
    fieldTypes.push_back(omniruntime::type::LongType());
    auto StructColType = std::make_shared<omniruntime::type::RowType>(fieldTypes);

    DataTypes buildTypes(std::vector<DataTypePtr>({ LongType(), LongType(), StructColType }));
    int64_t buildData0[] = {1, 2, 3, 4};
    int64_t buildData1[] = {22, 11, 44, 33};
    auto *buildVecBatch = new VectorBatch(dataSize);
    buildVecBatch->Append(CreateVector(dataSize, buildData0));
    int32_t ids[] = {0, 1, 2, 3};
    buildVecBatch->Append(CreateDictionaryVector(*buildTypes.GetType(1), dataSize, ids, dataSize, buildData1));

    auto* buildChildVector0 = new vec::Vector<int64_t>(dataSize); // 0 1 2 3
    auto* buildChildVector1 = new vec::Vector<int64_t>(dataSize); // 1 2 3 4
    for (int i = 0; i < dataSize; i++) {
        buildChildVector0->SetValue(i, i);
        buildChildVector1->SetValue(i, i + 1);
    }
    auto* buildStructVec = new RowVector(dataSize);
    buildStructVec->AddChild(buildChildVector0);
    buildStructVec->AddChild(buildChildVector1);
    buildVecBatch->Append(buildStructVec);

    std::vector<omniruntime::expressions::Expr *> buildHashKeys = { new omniruntime::expressions::FieldExpr(1,
        LongType()) };
    int32_t hashTableCount = 1;
    auto overflowConfig = new OverflowConfig();
    HashBuilderWithExprOperatorFactory *hashBuilderWithExprOperatorFactory =
        HashBuilderWithExprOperatorFactory::CreateHashBuilderWithExprOperatorFactory(OMNI_JOIN_TYPE_LEFT, buildTypes,
            buildHashKeys, hashTableCount, overflowConfig);
    auto *hashBuilderWithExprOperator = CreateTestOperator(hashBuilderWithExprOperatorFactory);
    hashBuilderWithExprOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuilderOutput = nullptr;
    hashBuilderWithExprOperator->GetOutput(&hashBuilderOutput);

    DataTypes probeTypes(std::vector<DataTypePtr>({ LongType(), LongType(), StructColType }));
    int64_t probeData0[] = {1, 2, 3, 4};
    int64_t probeData1[] = {11, 22, 33, 44};

    auto* probeChildVector0 = new vec::Vector<int64_t>(dataSize); // 2 3 4 5
    auto* probeChildVector1 = new vec::Vector<int64_t>(dataSize); // 3 4 5 6
    for (int i = 0; i < dataSize; i++) {
        probeChildVector0->SetValue(i, i + 2);
        probeChildVector1->SetValue(i, i + 3);
    }
    auto* probeStructVec = new RowVector(dataSize);
    probeStructVec->AddChild(probeChildVector0);
    probeStructVec->AddChild(probeChildVector1);

    auto *probeVecBatch = new VectorBatch(dataSize);
    probeVecBatch->Append(CreateVector(dataSize, probeData0));
    probeVecBatch->Append(CreateDictionaryVector(*probeTypes.GetType(1), dataSize, ids, dataSize, probeData1));
    probeVecBatch->Append(probeStructVec);

    int32_t probeOutputCols[3]= {0, 1, 2};
    int32_t probeOutputColsCount = 3;
    std::vector<omniruntime::expressions::Expr *> probeHashKeys = { new omniruntime::expressions::FieldExpr(1,
        LongType()) };
    int32_t probeHashKeysCount = 1;
    int32_t buildOutputCols[3] = {0, 1, 2};
    int32_t buildOutputColsCount = 3;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({ LongType(), LongType(), StructColType }));
    HashBuilderOperatorFactory* hashBuilderFactory = hashBuilderWithExprOperatorFactory->GetHashBuilderOperatorFactory();
    auto lookupJoinWithExprOperatorFactory = LookupJoinWithExprOperatorFactory::CreateLookupJoinWithExprOperatorFactory(
        probeTypes, probeOutputCols, probeOutputColsCount, probeHashKeys, probeHashKeysCount, buildOutputCols,
        buildOutputColsCount, buildOutputTypes, (int64_t)hashBuilderFactory, nullptr, false, overflowConfig);
    auto lookupJoinWithExprOperator = CreateTestOperator(lookupJoinWithExprOperatorFactory);
    lookupJoinWithExprOperator->AddInput(probeVecBatch);
    VectorBatch *lookupJoinOutputVecBatch = nullptr;
    lookupJoinWithExprOperator->GetOutput(&lookupJoinOutputVecBatch);

    const int32_t expectedDataSize = 4;
    int64_t expectedProbeCol0[] = {1, 2, 3, 4};
    int64_t expectedProbeCol1[] = {11, 22, 33, 44};
    auto* expectedProbeChildVector0 = new vec::Vector<int64_t>(expectedDataSize); // 2 3 4 5
    auto* expectedProbeChildVector1 = new vec::Vector<int64_t>(expectedDataSize); // 3 4 5 6
    int64_t expectedProbeChildVectorVals0[] = {2, 3, 4, 5};
    int64_t expectedProbeChildVectorVals1[] = {3, 4, 5, 6};
    for (int i = 0; i < expectedDataSize; i++) {
        expectedProbeChildVector0->SetValue(i, expectedProbeChildVectorVals0[i]);
        expectedProbeChildVector1->SetValue(i, expectedProbeChildVectorVals1[i]);
    }
    auto* expectedProbeStructVec = new RowVector(expectedDataSize);
    expectedProbeStructVec->AddChild(expectedProbeChildVector0);
    expectedProbeStructVec->AddChild(expectedProbeChildVector1);

    auto* expectedBuildChildVector0 = new vec::Vector<int64_t>(expectedDataSize); // 0 1 2 3
    auto* expectedBuildChildVector1 = new vec::Vector<int64_t>(expectedDataSize); // 1 2 3 4
    int64_t expectedBuildChildVectorVals0[] = {1, 0, 3, 2};
    int64_t expectedBuildChildVectorVals1[] = {2, 1, 4, 3};
    for (int i = 0; i < expectedDataSize; i++) {
        expectedBuildChildVector0->SetValue(i, expectedBuildChildVectorVals0[i]);
        expectedBuildChildVector1->SetValue(i, expectedBuildChildVectorVals1[i]);
    }
    auto* expectedBuildStructVec = new RowVector(expectedDataSize);
    expectedBuildStructVec->AddChild(expectedBuildChildVector0);
    expectedBuildStructVec->AddChild(expectedBuildChildVector1);

    auto *expectedVecBatch = new VectorBatch(expectedDataSize);
    expectedVecBatch->Append(CreateVector(expectedDataSize, expectedProbeCol0));
    expectedVecBatch->Append(CreateVector(expectedDataSize, expectedProbeCol1));
    expectedVecBatch->Append(expectedProbeStructVec);

    int size = 4;
    auto* expectedBuildCol0 = new vec::Vector<int64_t>(size);
    auto* expectedBuildCol1 = new vec::Vector<int64_t>(size);
    expectedBuildCol0->SetValue(0, 2);
    expectedBuildCol0->SetValue(1, 1);
    expectedBuildCol0->SetValue(2, 4);
    expectedBuildCol0->SetValue(3, 3);
    expectedBuildCol1->SetValue(0, 11);
    expectedBuildCol1->SetValue(1, 22);
    expectedBuildCol1->SetValue(2, 33);
    expectedBuildCol1->SetValue(3, 44);
    expectedVecBatch->Append(expectedBuildCol0);
    expectedVecBatch->Append(expectedBuildCol1);
    expectedVecBatch->Append(expectedBuildStructVec);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(lookupJoinOutputVecBatch, expectedVecBatch));

    Expr::DeleteExprs(buildHashKeys);
    Expr::DeleteExprs(probeHashKeys);
    VectorHelper::FreeVecBatch(lookupJoinOutputVecBatch);
    VectorHelper::FreeVecBatch(expectedVecBatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinWithExprOperator);
    DeleteJoinExprOperatorFactory(hashBuilderWithExprOperatorFactory, lookupJoinWithExprOperatorFactory);
    delete overflowConfig;
}

TEST(JoinWithExprTest, TestLeftJoinOnKeyWithoutExprWithStructVectorWithNull)
{
    // construct input data
    const int32_t dataSize = 4;

    std::vector<std::shared_ptr<omniruntime::type::DataType>> fieldTypes;
    fieldTypes.push_back(omniruntime::type::LongType());
    fieldTypes.push_back(omniruntime::type::LongType());
    auto StructColType = std::make_shared<omniruntime::type::RowType>(fieldTypes);

    DataTypes buildTypes(std::vector<DataTypePtr>({ LongType(), LongType(), StructColType }));
    int64_t buildData0[] = {1, 2, 3, 4};
    int64_t buildData1[] = {111, 11, 333, 33};
    auto *buildVecBatch = new VectorBatch(dataSize);
    buildVecBatch->Append(CreateVector(dataSize, buildData0));
    int32_t ids[] = {0, 1, 2, 3};
    buildVecBatch->Append(CreateDictionaryVector(*buildTypes.GetType(1), dataSize, ids, dataSize, buildData1));

    auto* buildChildVector0 = new vec::Vector<int64_t>(dataSize); // 0 1 2 3
    auto* buildChildVector1 = new vec::Vector<int64_t>(dataSize); // 1 2 3 4
    for (int i = 0; i < dataSize; i++) {
        buildChildVector0->SetValue(i, i);
        buildChildVector1->SetValue(i, i + 1);
    }
    auto* buildStructVec = new RowVector(dataSize);
    buildStructVec->AddChild(buildChildVector0);
    buildStructVec->AddChild(buildChildVector1);
    buildVecBatch->Append(buildStructVec);

    std::vector<omniruntime::expressions::Expr *> buildHashKeys = { new omniruntime::expressions::FieldExpr(1,
        LongType()) };
    int32_t hashTableCount = 1;
    auto overflowConfig = new OverflowConfig();
    HashBuilderWithExprOperatorFactory *hashBuilderWithExprOperatorFactory =
        HashBuilderWithExprOperatorFactory::CreateHashBuilderWithExprOperatorFactory(OMNI_JOIN_TYPE_LEFT, buildTypes,
            buildHashKeys, hashTableCount, overflowConfig);
    auto *hashBuilderWithExprOperator = CreateTestOperator(hashBuilderWithExprOperatorFactory);
    hashBuilderWithExprOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuilderOutput = nullptr;
    hashBuilderWithExprOperator->GetOutput(&hashBuilderOutput);

    DataTypes probeTypes(std::vector<DataTypePtr>({ LongType(), LongType(), StructColType }));
    int64_t probeData0[] = {1, 2, 3, 4};
    int64_t probeData1[] = {11, 22, 33, 44};
    auto* probeChildVector0 = new vec::Vector<int64_t>(dataSize); // 2 3 4 5
    auto* probeChildVector1 = new vec::Vector<int64_t>(dataSize); // 3 4 5 6
    for (int i = 0; i < dataSize; i++) {
        probeChildVector0->SetValue(i, i + 2);
        probeChildVector1->SetValue(i, i + 3);
    }
    auto* probeStructVec = new RowVector(dataSize);
    probeStructVec->AddChild(probeChildVector0);
    probeStructVec->AddChild(probeChildVector1);

    auto *probeVecBatch = new VectorBatch(dataSize);
    probeVecBatch->Append(CreateVector(dataSize, probeData0));
    probeVecBatch->Append(CreateDictionaryVector(*probeTypes.GetType(1), dataSize, ids, dataSize, probeData1));
    probeVecBatch->Append(probeStructVec);

    int32_t probeOutputCols[3]= {0, 1, 2};
    int32_t probeOutputColsCount = 3;
    std::vector<omniruntime::expressions::Expr *> probeHashKeys = { new omniruntime::expressions::FieldExpr(1,
        LongType()) };
    int32_t probeHashKeysCount = 1;
    int32_t buildOutputCols[3] = {0, 1, 2};
    int32_t buildOutputColsCount = 3;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({ LongType(), LongType(), StructColType }));
    HashBuilderOperatorFactory* hashBuilderFactory = hashBuilderWithExprOperatorFactory->GetHashBuilderOperatorFactory();
    auto lookupJoinWithExprOperatorFactory = LookupJoinWithExprOperatorFactory::CreateLookupJoinWithExprOperatorFactory(
        probeTypes, probeOutputCols, probeOutputColsCount, probeHashKeys, probeHashKeysCount, buildOutputCols,
        buildOutputColsCount, buildOutputTypes, (int64_t)hashBuilderFactory, nullptr, false, overflowConfig);
    auto lookupJoinWithExprOperator = CreateTestOperator(lookupJoinWithExprOperatorFactory);
    lookupJoinWithExprOperator->AddInput(probeVecBatch);
    VectorBatch *lookupJoinOutputVecBatch = nullptr;
    lookupJoinWithExprOperator->GetOutput(&lookupJoinOutputVecBatch);

    const int32_t expectedDataSize = 4;
    int64_t expectedProbeCol0[] = {1, 2, 3, 4};
    int64_t expectedProbeCol1[] = {11, 22, 33, 44};
    auto* expectedProbeChildVector0 = new vec::Vector<int64_t>(expectedDataSize); // 2 3 4 5
    auto* expectedProbeChildVector1 = new vec::Vector<int64_t>(expectedDataSize); // 3 4 5 6
    int64_t expectedProbeChildVectorVals0[] = {2, 3, 4, 5};
    int64_t expectedProbeChildVectorVals1[] = {3, 4, 5, 6};
    for (int i = 0; i < expectedDataSize; i++) {
        expectedProbeChildVector0->SetValue(i, expectedProbeChildVectorVals0[i]);
        expectedProbeChildVector1->SetValue(i, expectedProbeChildVectorVals1[i]);
    }
    auto* expectedProbeStructVec = new RowVector(expectedDataSize);
    expectedProbeStructVec->AddChild(expectedProbeChildVector0);
    expectedProbeStructVec->AddChild(expectedProbeChildVector1);

    auto* expectedBuildChildVector0 = new vec::Vector<int64_t>(expectedDataSize); // 0 1 2 3
    auto* expectedBuildChildVector1 = new vec::Vector<int64_t>(expectedDataSize); // 1 2 3 4
    expectedBuildChildVector0->SetValue(0, 1);
    expectedBuildChildVector0->SetNull(1);
    expectedBuildChildVector0->SetValue(2, 3);
    expectedBuildChildVector0->SetNull(3);
    expectedBuildChildVector1->SetValue(0, 2);
    expectedBuildChildVector1->SetNull(1);
    expectedBuildChildVector1->SetValue(2, 4);
    expectedBuildChildVector1->SetNull(3);
    auto* expectedBuildStructVec = new RowVector(expectedDataSize);
    expectedBuildStructVec->AddChild(expectedBuildChildVector0);
    expectedBuildStructVec->AddChild(expectedBuildChildVector1);
    expectedBuildStructVec->SetNull(1);
    expectedBuildStructVec->SetNull(3);

    auto *expectedVecBatch = new VectorBatch(expectedDataSize);
    expectedVecBatch->Append(CreateVector(expectedDataSize, expectedProbeCol0));
    expectedVecBatch->Append(CreateVector(expectedDataSize, expectedProbeCol1));
    expectedVecBatch->Append(expectedProbeStructVec);

    int size = 4;
    auto* expectedBuildCol0 = new vec::Vector<int64_t>(size);
    auto* expectedBuildCol1 = new vec::Vector<int64_t>(size);
    expectedBuildCol0->SetValue(0, 2);
    expectedBuildCol0->SetNull(1);
    expectedBuildCol0->SetValue(2, 4);
    expectedBuildCol0->SetNull(3);
    expectedBuildCol1->SetValue(0, 11);
    expectedBuildCol1->SetNull(1);
    expectedBuildCol1->SetValue(2, 33);
    expectedBuildCol1->SetNull(3);
    expectedVecBatch->Append(expectedBuildCol0);
    expectedVecBatch->Append(expectedBuildCol1);
    expectedVecBatch->Append(expectedBuildStructVec);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(lookupJoinOutputVecBatch, expectedVecBatch));

    Expr::DeleteExprs(buildHashKeys);
    Expr::DeleteExprs(probeHashKeys);
    VectorHelper::FreeVecBatch(lookupJoinOutputVecBatch);
    VectorHelper::FreeVecBatch(expectedVecBatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinWithExprOperator);
    DeleteJoinExprOperatorFactory(hashBuilderWithExprOperatorFactory, lookupJoinWithExprOperatorFactory);
    delete overflowConfig;
}

TEST(JoinWithExprTest, TestLeftJoinOnKeyWithoutExprWithStructVectorWithVarchar)
{
    // construct input data
    const int32_t dataSize = 4;

    std::vector<std::shared_ptr<omniruntime::type::DataType>> fieldTypes;
    fieldTypes.push_back(omniruntime::type::LongType());
    fieldTypes.push_back(omniruntime::type::VarcharType(500));
    auto StructColType = std::make_shared<omniruntime::type::RowType>(fieldTypes);

    DataTypes buildTypes(std::vector<DataTypePtr>({ LongType(), LongType(), StructColType }));
    int64_t buildData0[] = {1, 2, 3, 4};
    int64_t buildData1[] = {22, 11, 44, 33};
    auto *buildVecBatch = new VectorBatch(dataSize);
    buildVecBatch->Append(CreateVector(dataSize, buildData0));
    int32_t ids[] = {0, 1, 2, 3};
    buildVecBatch->Append(CreateDictionaryVector(*buildTypes.GetType(1), dataSize, ids, dataSize, buildData1));

    auto* buildChildVector0 = new vec::Vector<int64_t>(dataSize); // 0 1 2 3
    auto buildStringVector = VectorHelper::CreateStringVector(4);
    auto* buildChildVector1 = (Vector<LargeStringContainer<std::string_view>> *)buildStringVector; // cc aa aa cc
    std::string buildValue;
    for (int i = 0; i < dataSize; i++) {
        buildChildVector0->SetValue(i, i);
        if (i % 3 == 0) {
            buildValue = "cc";
        } else {
            buildValue = "aa";
        }
        std::string_view input(buildValue.data(), buildValue.size());
        buildChildVector1->SetValue(i, input);
    }
    auto* buildStructVec = new RowVector(dataSize);
    buildStructVec->AddChild(buildChildVector0);
    buildStructVec->AddChild(buildChildVector1);
    buildVecBatch->Append(buildStructVec);

    std::vector<omniruntime::expressions::Expr *> buildHashKeys = { new omniruntime::expressions::FieldExpr(1,
        LongType()) };
    int32_t hashTableCount = 1;
    auto overflowConfig = new OverflowConfig();
    HashBuilderWithExprOperatorFactory *hashBuilderWithExprOperatorFactory =
        HashBuilderWithExprOperatorFactory::CreateHashBuilderWithExprOperatorFactory(OMNI_JOIN_TYPE_LEFT, buildTypes,
            buildHashKeys, hashTableCount, overflowConfig);
    auto *hashBuilderWithExprOperator = CreateTestOperator(hashBuilderWithExprOperatorFactory);
    hashBuilderWithExprOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuilderOutput = nullptr;
    hashBuilderWithExprOperator->GetOutput(&hashBuilderOutput);

    DataTypes probeTypes(std::vector<DataTypePtr>({ LongType(), LongType(), StructColType }));
    int64_t probeData0[] = {1, 2, 3, 4};
    int64_t probeData1[] = {11, 22, 33, 44};

    auto* probeChildVector0 = new vec::Vector<int64_t>(dataSize); // 2 3 4 5
    auto probeStringVector = VectorHelper::CreateStringVector(4);
    auto* probeChildVector1 = (Vector<LargeStringContainer<std::string_view>> *)probeStringVector; // ab de de ab
    std::string probeValue;
    for (int i = 0; i < dataSize; i++) {
        probeChildVector0->SetValue(i, i + 2);
        if (i % 3 == 0) {
            probeValue = "ab";
        } else {
            probeValue = "de";
        }
        std::string_view input(probeValue.data(), probeValue.size());
        probeChildVector1->SetValue(i, input);
    }
    auto* probeStructVec = new RowVector(dataSize);
    probeStructVec->AddChild(probeChildVector0);
    probeStructVec->AddChild(probeChildVector1);

    auto *probeVecBatch = new VectorBatch(dataSize);
    probeVecBatch->Append(CreateVector(dataSize, probeData0));
    probeVecBatch->Append(CreateDictionaryVector(*probeTypes.GetType(1), dataSize, ids, dataSize, probeData1));
    probeVecBatch->Append(probeStructVec);

    int32_t probeOutputCols[3]= {0, 1, 2};
    int32_t probeOutputColsCount = 3;
    std::vector<omniruntime::expressions::Expr *> probeHashKeys = { new omniruntime::expressions::FieldExpr(1,
        LongType()) };
    int32_t probeHashKeysCount = 1;
    int32_t buildOutputCols[3] = {0, 1, 2};
    int32_t buildOutputColsCount = 3;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({ LongType(), LongType(), StructColType }));
    HashBuilderOperatorFactory* hashBuilderFactory = hashBuilderWithExprOperatorFactory->GetHashBuilderOperatorFactory();
    auto lookupJoinWithExprOperatorFactory = LookupJoinWithExprOperatorFactory::CreateLookupJoinWithExprOperatorFactory(
        probeTypes, probeOutputCols, probeOutputColsCount, probeHashKeys, probeHashKeysCount, buildOutputCols,
        buildOutputColsCount, buildOutputTypes, (int64_t)hashBuilderFactory, nullptr, false, overflowConfig);
    auto lookupJoinWithExprOperator = CreateTestOperator(lookupJoinWithExprOperatorFactory);
    lookupJoinWithExprOperator->AddInput(probeVecBatch);
    VectorBatch *lookupJoinOutputVecBatch = nullptr;
    lookupJoinWithExprOperator->GetOutput(&lookupJoinOutputVecBatch);

    const int32_t expectedDataSize = 4;
    int64_t expectedProbeCol0[] = {1, 2, 3, 4};
    int64_t expectedProbeCol1[] = {11, 22, 33, 44};

    auto* expectedProbeChildVector0 = new vec::Vector<int64_t>(expectedDataSize); // 2 3 4 5
    auto expectedProbeStringVector = VectorHelper::CreateStringVector(4);
    auto* expectedProbeChildVector1 = (Vector<LargeStringContainer<std::string_view>> *)expectedProbeStringVector;
    int64_t expectedProbeChildVectorVals0[] = {2, 3, 4, 5};
    std::string expectedProbeValue;
    for (int i = 0; i < expectedDataSize; i++) {
        expectedProbeChildVector0->SetValue(i, expectedProbeChildVectorVals0[i]);
        if (i % 3 == 0) {
            expectedProbeValue = "ab";
        } else {
            expectedProbeValue = "de";
        }
        std::string_view input(expectedProbeValue.data(), expectedProbeValue.size());
        expectedProbeChildVector1->SetValue(i, input);
    }
    auto* expectedProbeStructVec = new RowVector(expectedDataSize);
    expectedProbeStructVec->AddChild(expectedProbeChildVector0);
    expectedProbeStructVec->AddChild(expectedProbeChildVector1);

    auto* expectedBuildChildVector0 = new vec::Vector<int64_t>(expectedDataSize); // 0 1 2 3
    auto expectedBuildStringVector = VectorHelper::CreateStringVector(4);
    auto* expectedBuildChildVector1 = (Vector<LargeStringContainer<std::string_view>> *)expectedBuildStringVector;
    int64_t expectedBuildChildVectorVals0[] = {0, 1, 2, 3};
    std::string expectedBuildValue;
    for (int i = 0; i < expectedDataSize; i++) {
        expectedBuildChildVector0->SetValue(i, expectedBuildChildVectorVals0[i]);
        if (i % 3 == 0) {
            expectedBuildValue = "aa";
        } else {
            expectedBuildValue = "cc";
        }
        std::string_view input(expectedBuildValue.data(), expectedBuildValue.size());
        expectedBuildChildVector1->SetValue(i, input);
    }
    auto* expectedBuildStructVec = new RowVector(expectedDataSize);
    expectedBuildStructVec->AddChild(expectedBuildChildVector0);
    expectedBuildStructVec->AddChild(expectedBuildChildVector1);

    auto *expectedVecBatch = new VectorBatch(expectedDataSize);
    expectedVecBatch->Append(CreateVector(expectedDataSize, expectedProbeCol0));
    expectedVecBatch->Append(CreateVector(expectedDataSize, expectedProbeCol1));
    expectedVecBatch->Append(expectedProbeStructVec);

    int size = 4;
    auto* expectedBuildCol0 = new vec::Vector<int64_t>(size);
    auto* expectedBuildCol1 = new vec::Vector<int64_t>(size);
    expectedBuildCol0->SetValue(0, 2);
    expectedBuildCol0->SetValue(1, 1);
    expectedBuildCol0->SetValue(2, 4);
    expectedBuildCol0->SetValue(3, 3);
    expectedBuildCol1->SetValue(0, 11);
    expectedBuildCol1->SetValue(1, 22);
    expectedBuildCol1->SetValue(2, 33);
    expectedBuildCol1->SetValue(3, 44);
    expectedVecBatch->Append(expectedBuildCol0);
    expectedVecBatch->Append(expectedBuildCol1);
    expectedVecBatch->Append(expectedBuildStructVec);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(lookupJoinOutputVecBatch, expectedVecBatch));

    Expr::DeleteExprs(buildHashKeys);
    Expr::DeleteExprs(probeHashKeys);
    VectorHelper::FreeVecBatch(lookupJoinOutputVecBatch);
    VectorHelper::FreeVecBatch(expectedVecBatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinWithExprOperator);
    DeleteJoinExprOperatorFactory(hashBuilderWithExprOperatorFactory, lookupJoinWithExprOperatorFactory);
    delete overflowConfig;
}

TEST(JoinWithExprTest, TestInnerEqualityJoinOnKeyWithoutExprWithArrayVector)
{
    // construct input data
    const int32_t dataSize = 4;

    auto arrayColType = std::make_shared<omniruntime::type::ArrayType>(LongType());
    DataTypes buildTypes(std::vector<DataTypePtr>({ LongType(), LongType(), arrayColType }));
    int64_t buildData0[] = {1, 2, 3, 4};
    int64_t buildData1[] = {111, 11, 333, 33};
    auto *buildVecBatch = new VectorBatch(dataSize);
    buildVecBatch->Append(CreateVector(dataSize, buildData0));
    int32_t ids[] = {0, 1, 2, 3};
    buildVecBatch->Append(CreateDictionaryVector(*buildTypes.GetType(1), dataSize, ids, dataSize, buildData1));

    auto buildElementVector = std::make_shared<Vector<int64_t>>(8, OMNI_LONG);
    int64_t buildVals[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    for (int i = 0; i < 8; i++) {
        buildElementVector->SetValue(i, buildVals[i]);
    }
    auto buildArrayVec = new ArrayVector(dataSize, buildElementVector);
    for (int row = 0; row <= dataSize; row++) {
        buildArrayVec->SetOffset(row, row * 2);
    }
    buildVecBatch->Append(buildArrayVec);

    std::vector<omniruntime::expressions::Expr *> buildHashKeys = { new omniruntime::expressions::FieldExpr(1,
        LongType()) };
    int32_t hashTableCount = 1;
    auto overflowConfig = new OverflowConfig();
    HashBuilderWithExprOperatorFactory *hashBuilderWithExprOperatorFactory =
        HashBuilderWithExprOperatorFactory::CreateHashBuilderWithExprOperatorFactory(OMNI_JOIN_TYPE_INNER, buildTypes,
        buildHashKeys, hashTableCount, overflowConfig);
    auto *hashBuilderWithExprOperator = CreateTestOperator(hashBuilderWithExprOperatorFactory);
    hashBuilderWithExprOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuilderOutput = nullptr;
    hashBuilderWithExprOperator->GetOutput(&hashBuilderOutput);

    DataTypes probeTypes(std::vector<DataTypePtr>({ LongType(), LongType(), arrayColType }));
    int64_t probeData0[] = {1, 2, 3, 4};
    int64_t probeData1[] = {11, 22, 33, 44};
    auto probeElementVector = std::make_shared<Vector<int64_t>>(8, OMNI_LONG);
    int64_t probeVals[8] = {8, 7, 6, 5, 4, 3, 2, 1};
    for (int i = 0; i < 8; i++) {
        probeElementVector->SetValue(i, probeVals[i]);
    }
    auto probeArrayVec = new ArrayVector(dataSize, probeElementVector);
    for (int row = 0; row <= dataSize; row++) {
        probeArrayVec->SetOffset(row, row * 2);
    }
    auto *probeVecBatch = new VectorBatch(dataSize);
    probeVecBatch->Append(CreateVector(dataSize, probeData0));
    probeVecBatch->Append(CreateDictionaryVector(*probeTypes.GetType(1), dataSize, ids, dataSize, probeData1));
    probeVecBatch->Append(probeArrayVec);

    int32_t probeOutputCols[3]= {0, 1, 2};
    int32_t probeOutputColsCount = 3;
    std::vector<omniruntime::expressions::Expr *> probeHashKeys = { new omniruntime::expressions::FieldExpr(1,
        LongType()) };
    int32_t probeHashKeysCount = 1;
    int32_t buildOutputCols[3] = {0, 1, 2};
    int32_t buildOutputColsCount = 3;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({ LongType(), LongType(), arrayColType }));
    HashBuilderOperatorFactory* hashBuilderFactory = hashBuilderWithExprOperatorFactory->GetHashBuilderOperatorFactory();
    auto lookupJoinWithExprOperatorFactory = LookupJoinWithExprOperatorFactory::CreateLookupJoinWithExprOperatorFactory(
        probeTypes, probeOutputCols, probeOutputColsCount, probeHashKeys, probeHashKeysCount, buildOutputCols,
        buildOutputColsCount, buildOutputTypes, (int64_t)hashBuilderFactory, nullptr, false, overflowConfig);
    auto lookupJoinWithExprOperator = CreateTestOperator(lookupJoinWithExprOperatorFactory);
    lookupJoinWithExprOperator->AddInput(probeVecBatch);
    VectorBatch *lookupJoinOutputVecBatch = nullptr;
    lookupJoinWithExprOperator->GetOutput(&lookupJoinOutputVecBatch);

    const int32_t expectedDataSize = 2;
    int64_t expectedProbeCol0[] = {1, 3};
    int64_t expectedProbeCol1[] = {11, 33};
    auto expectedProbeElementVector = std::make_shared<Vector<int64_t>>(4, OMNI_LONG);
    int64_t expectedProbeVals[4] = {8, 7, 4, 3};
    for (int i = 0; i < 4; i++) {
        expectedProbeElementVector->SetValue(i, expectedProbeVals[i]);
    }
    auto expectedProbeArrayVec =  new ArrayVector(expectedDataSize, expectedProbeElementVector);
    for (int row = 0; row <= expectedDataSize; row++) {
        expectedProbeArrayVec->SetOffset(row, row * 2);
    }

    int64_t expectedBuildCol0[] = {2, 4};
    int64_t expectedBuildCol1[] = {11, 33};
    auto expectedBuildElementVector = std::make_shared<Vector<int64_t>>(4, OMNI_LONG);
    int64_t expectedBuildVals[4] = {3, 4, 7, 8};
    for (int i = 0; i < 4; i++) {
        expectedBuildElementVector->SetValue(i, expectedBuildVals[i]);
    }
    auto expectedBuildArrayVec =  new ArrayVector(expectedDataSize, expectedBuildElementVector);
    for (int row = 0; row <= expectedDataSize; row++) {
        expectedBuildArrayVec->SetOffset(row, row * 2);
    }

    auto *expectedVecBatch = new VectorBatch(expectedDataSize);
    expectedVecBatch->Append(CreateVector(expectedDataSize, expectedProbeCol0));
    expectedVecBatch->Append(CreateVector(expectedDataSize, expectedProbeCol1));
    expectedVecBatch->Append(expectedProbeArrayVec);
    expectedVecBatch->Append(CreateVector(expectedDataSize, expectedBuildCol0));
    expectedVecBatch->Append(CreateVector(expectedDataSize, expectedBuildCol1));
    expectedVecBatch->Append(expectedBuildArrayVec);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(lookupJoinOutputVecBatch, expectedVecBatch));

    Expr::DeleteExprs(buildHashKeys);
    Expr::DeleteExprs(probeHashKeys);
    VectorHelper::FreeVecBatch(lookupJoinOutputVecBatch);
    VectorHelper::FreeVecBatch(expectedVecBatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinWithExprOperator);
    DeleteJoinExprOperatorFactory(hashBuilderWithExprOperatorFactory, lookupJoinWithExprOperatorFactory);
    delete overflowConfig;
}

TEST(JoinWithExprTest, TestLeftJoinOnKeyWithoutExprWithArrayVector)
{
    // construct input data
    const int32_t dataSize = 4;

    auto arrayColType = std::make_shared<omniruntime::type::ArrayType>(LongType());
    DataTypes buildTypes(std::vector<DataTypePtr>({ LongType(), LongType(), arrayColType }));
    int64_t buildData0[] = {1, 2, 3, 4};
    int64_t buildData1[] = {22, 11, 44, 33};
    auto *buildVecBatch = new VectorBatch(dataSize);
    buildVecBatch->Append(CreateVector(dataSize, buildData0));
    int32_t ids[] = {0, 1, 2, 3};
    buildVecBatch->Append(CreateDictionaryVector(*buildTypes.GetType(1), dataSize, ids, dataSize, buildData1));

    auto buildElementVector = std::make_shared<Vector<int64_t>>(8, OMNI_LONG);
    int64_t buildVals[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    for (int i = 0; i < 8; i++) {
        buildElementVector->SetValue(i, buildVals[i]);
    }
    auto buildArrayVec = new ArrayVector(dataSize, buildElementVector);
    for (int row = 0; row <= dataSize; row++) {
        buildArrayVec->SetOffset(row, row * 2);
    }
    buildVecBatch->Append(buildArrayVec);

    std::vector<omniruntime::expressions::Expr *> buildHashKeys = { new omniruntime::expressions::FieldExpr(1,
        LongType()) };
    int32_t hashTableCount = 1;
    auto overflowConfig = new OverflowConfig();
    HashBuilderWithExprOperatorFactory *hashBuilderWithExprOperatorFactory =
        HashBuilderWithExprOperatorFactory::CreateHashBuilderWithExprOperatorFactory(OMNI_JOIN_TYPE_LEFT, buildTypes,
            buildHashKeys, hashTableCount, overflowConfig);
    auto *hashBuilderWithExprOperator = CreateTestOperator(hashBuilderWithExprOperatorFactory);
    hashBuilderWithExprOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuilderOutput = nullptr;
    hashBuilderWithExprOperator->GetOutput(&hashBuilderOutput);

    DataTypes probeTypes(std::vector<DataTypePtr>({ LongType(), LongType(), arrayColType }));
    int64_t probeData0[] = {1, 2, 3, 4};
    int64_t probeData1[] = {11, 22, 33, 44};
    auto probeElementVector = std::make_shared<Vector<int64_t>>(8, OMNI_LONG);
    int64_t probeVals[8] = {8, 7, 6, 5, 4, 3, 2, 1};
    for (int i = 0; i < 8; i++) {
        probeElementVector->SetValue(i, probeVals[i]);
    }
    auto probeArrayVec = new ArrayVector(dataSize, probeElementVector);
    for (int row = 0; row <= dataSize; row++) {
        probeArrayVec->SetOffset(row, row * 2);
    }
    auto *probeVecBatch = new VectorBatch(dataSize);
    probeVecBatch->Append(CreateVector(dataSize, probeData0));
    probeVecBatch->Append(CreateDictionaryVector(*probeTypes.GetType(1), dataSize, ids, dataSize, probeData1));
    probeVecBatch->Append(probeArrayVec);

    int32_t probeOutputCols[3]= {0, 1, 2};
    int32_t probeOutputColsCount = 3;
    std::vector<omniruntime::expressions::Expr *> probeHashKeys = { new omniruntime::expressions::FieldExpr(1,
        LongType()) };
    int32_t probeHashKeysCount = 1;
    int32_t buildOutputCols[3] = {0, 1, 2};
    int32_t buildOutputColsCount = 3;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({ LongType(), LongType(), arrayColType }));
    HashBuilderOperatorFactory* hashBuilderFactory = hashBuilderWithExprOperatorFactory->GetHashBuilderOperatorFactory();
    auto lookupJoinWithExprOperatorFactory = LookupJoinWithExprOperatorFactory::CreateLookupJoinWithExprOperatorFactory(
        probeTypes, probeOutputCols, probeOutputColsCount, probeHashKeys, probeHashKeysCount, buildOutputCols,
        buildOutputColsCount, buildOutputTypes, (int64_t)hashBuilderFactory, nullptr, false, overflowConfig);
    auto lookupJoinWithExprOperator = CreateTestOperator(lookupJoinWithExprOperatorFactory);
    lookupJoinWithExprOperator->AddInput(probeVecBatch);
    VectorBatch *lookupJoinOutputVecBatch = nullptr;
    lookupJoinWithExprOperator->GetOutput(&lookupJoinOutputVecBatch);

    const int32_t expectedDataSize = 4;
    int64_t expectedProbeCol0[] = {1, 2, 3, 4};
    int64_t expectedProbeCol1[] = {11, 22, 33, 44};
    auto expectedProbeElementVector = std::make_shared<Vector<int64_t>>(8, OMNI_LONG);
    expectedProbeElementVector->SetValue(0, 8);
    expectedProbeElementVector->SetValue(1, 7);
    expectedProbeElementVector->SetValue(2, 6);
    expectedProbeElementVector->SetValue(3, 5);
    expectedProbeElementVector->SetValue(4, 4);
    expectedProbeElementVector->SetValue(5, 3);
    expectedProbeElementVector->SetValue(6, 2);
    expectedProbeElementVector->SetValue(7, 1);
    auto expectedProbeArrayVec =  new ArrayVector(expectedDataSize, expectedProbeElementVector);
    expectedProbeArrayVec->SetSize(0, 2);
    expectedProbeArrayVec->SetSize(1, 2);
    expectedProbeArrayVec->SetSize(2, 2);
    expectedProbeArrayVec->SetSize(3, 2);

    auto expectedBuildElementVector = std::make_shared<Vector<int64_t>>(8, OMNI_LONG);
    expectedBuildElementVector->SetValue(0, 3);
    expectedBuildElementVector->SetValue(1, 4);
    expectedBuildElementVector->SetValue(2, 1);
    expectedBuildElementVector->SetValue(3, 2);
    expectedBuildElementVector->SetValue(4, 7);
    expectedBuildElementVector->SetValue(5, 8);
    expectedBuildElementVector->SetValue(6, 5);
    expectedBuildElementVector->SetValue(7, 6);
    auto expectedBuildArrayVec =  new ArrayVector(expectedDataSize, expectedBuildElementVector);
    expectedBuildArrayVec->SetSize(0, 2);
    expectedBuildArrayVec->SetSize(1, 2);
    expectedBuildArrayVec->SetSize(2, 2);
    expectedBuildArrayVec->SetSize(3, 2);

    auto *expectedVecBatch = new VectorBatch(expectedDataSize);
    expectedVecBatch->Append(CreateVector(expectedDataSize, expectedProbeCol0));
    expectedVecBatch->Append(CreateVector(expectedDataSize, expectedProbeCol1));
    expectedVecBatch->Append(expectedProbeArrayVec);

    int size = 4;
    auto* expectedBuildCol0 = new vec::Vector<int64_t>(size);
    auto* expectedBuildCol1 = new vec::Vector<int64_t>(size);
    expectedBuildCol0->SetValue(0, 2);
    expectedBuildCol0->SetValue(1, 1);
    expectedBuildCol0->SetValue(2, 4);
    expectedBuildCol0->SetValue(3, 3);
    expectedBuildCol1->SetValue(0, 11);
    expectedBuildCol1->SetValue(1, 22);
    expectedBuildCol1->SetValue(2, 33);
    expectedBuildCol1->SetValue(3, 44);
    expectedVecBatch->Append(expectedBuildCol0);
    expectedVecBatch->Append(expectedBuildCol1);
    expectedVecBatch->Append(expectedBuildArrayVec);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(lookupJoinOutputVecBatch, expectedVecBatch));

    Expr::DeleteExprs(buildHashKeys);
    Expr::DeleteExprs(probeHashKeys);
    VectorHelper::FreeVecBatch(lookupJoinOutputVecBatch);
    VectorHelper::FreeVecBatch(expectedVecBatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinWithExprOperator);
    DeleteJoinExprOperatorFactory(hashBuilderWithExprOperatorFactory, lookupJoinWithExprOperatorFactory);
    delete overflowConfig;
}

TEST(JoinWithExprTest, TestLeftJoinOnKeyWithoutExprWithArrayVectorWithNull)
{
    // construct input data
    const int32_t dataSize = 4;

    auto arrayColType = std::make_shared<omniruntime::type::ArrayType>(LongType());
    DataTypes buildTypes(std::vector<DataTypePtr>({ LongType(), LongType(), arrayColType }));
    int64_t buildData0[] = {1, 2, 3, 4};
    int64_t buildData1[] = {111, 11, 333, 33};
    auto *buildVecBatch = new VectorBatch(dataSize);
    buildVecBatch->Append(CreateVector(dataSize, buildData0));
    int32_t ids[] = {0, 1, 2, 3};
    buildVecBatch->Append(CreateDictionaryVector(*buildTypes.GetType(1), dataSize, ids, dataSize, buildData1));

    auto buildElementVector = std::make_shared<Vector<int64_t>>(8, OMNI_LONG);
    int64_t buildVals[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    for (int i = 0; i < 8; i++) {
        buildElementVector->SetValue(i, buildVals[i]);
    }
    auto buildArrayVec = new ArrayVector(dataSize, buildElementVector);
    for (int row = 0; row <= dataSize; row++) {
        buildArrayVec->SetOffset(row, row * 2);
    }
    buildVecBatch->Append(buildArrayVec);

    std::vector<omniruntime::expressions::Expr *> buildHashKeys = { new omniruntime::expressions::FieldExpr(1,
        LongType()) };
    int32_t hashTableCount = 1;
    auto overflowConfig = new OverflowConfig();
    HashBuilderWithExprOperatorFactory *hashBuilderWithExprOperatorFactory =
        HashBuilderWithExprOperatorFactory::CreateHashBuilderWithExprOperatorFactory(OMNI_JOIN_TYPE_LEFT, buildTypes,
            buildHashKeys, hashTableCount, overflowConfig);
    auto *hashBuilderWithExprOperator = CreateTestOperator(hashBuilderWithExprOperatorFactory);
    hashBuilderWithExprOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuilderOutput = nullptr;
    hashBuilderWithExprOperator->GetOutput(&hashBuilderOutput);

    DataTypes probeTypes(std::vector<DataTypePtr>({ LongType(), LongType(), arrayColType }));
    int64_t probeData0[] = {1, 2, 3, 4};
    int64_t probeData1[] = {11, 22, 33, 44};
    auto probeElementVector = std::make_shared<Vector<int64_t>>(8, OMNI_LONG);
    int64_t probeVals[8] = {8, 7, 6, 5, 4, 3, 2, 1};
    for (int i = 0; i < 8; i++) {
        probeElementVector->SetValue(i, probeVals[i]);
    }
    auto probeArrayVec = new ArrayVector(dataSize, probeElementVector);
    for (int row = 0; row <= dataSize; row++) {
        probeArrayVec->SetOffset(row, row * 2);
    }
    auto *probeVecBatch = new VectorBatch(dataSize);
    probeVecBatch->Append(CreateVector(dataSize, probeData0));
    probeVecBatch->Append(CreateDictionaryVector(*probeTypes.GetType(1), dataSize, ids, dataSize, probeData1));
    probeVecBatch->Append(probeArrayVec);

    int32_t probeOutputCols[3]= {0, 1, 2};
    int32_t probeOutputColsCount = 3;
    std::vector<omniruntime::expressions::Expr *> probeHashKeys = { new omniruntime::expressions::FieldExpr(1,
        LongType()) };
    int32_t probeHashKeysCount = 1;
    int32_t buildOutputCols[3] = {0, 1, 2};
    int32_t buildOutputColsCount = 3;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({ LongType(), LongType(), arrayColType }));
    HashBuilderOperatorFactory* hashBuilderFactory = hashBuilderWithExprOperatorFactory->GetHashBuilderOperatorFactory();
    auto lookupJoinWithExprOperatorFactory = LookupJoinWithExprOperatorFactory::CreateLookupJoinWithExprOperatorFactory(
        probeTypes, probeOutputCols, probeOutputColsCount, probeHashKeys, probeHashKeysCount, buildOutputCols,
        buildOutputColsCount, buildOutputTypes, (int64_t)hashBuilderFactory, nullptr, false, overflowConfig);
    auto lookupJoinWithExprOperator = CreateTestOperator(lookupJoinWithExprOperatorFactory);
    lookupJoinWithExprOperator->AddInput(probeVecBatch);
    VectorBatch *lookupJoinOutputVecBatch = nullptr;
    lookupJoinWithExprOperator->GetOutput(&lookupJoinOutputVecBatch);

    const int32_t expectedDataSize = 4;
    int64_t expectedProbeCol0[] = {1, 2, 3, 4};
    int64_t expectedProbeCol1[] = {11, 22, 33, 44};
    auto expectedProbeElementVector = std::make_shared<Vector<int64_t>>(8, OMNI_LONG);
    expectedProbeElementVector->SetValue(0, 8);
    expectedProbeElementVector->SetValue(1, 7);
    expectedProbeElementVector->SetValue(2, 6);
    expectedProbeElementVector->SetValue(3, 5);
    expectedProbeElementVector->SetValue(4, 4);
    expectedProbeElementVector->SetValue(5, 3);
    expectedProbeElementVector->SetValue(6, 2);
    expectedProbeElementVector->SetValue(7, 1);
    auto expectedProbeArrayVec =  new ArrayVector(expectedDataSize, expectedProbeElementVector);
    expectedProbeArrayVec->SetSize(0, 2);
    expectedProbeArrayVec->SetSize(1, 2);
    expectedProbeArrayVec->SetSize(2, 2);
    expectedProbeArrayVec->SetSize(3, 2);

    auto expectedBuildElementVector = std::make_shared<Vector<int64_t>>(6, OMNI_LONG);
    expectedBuildElementVector->SetValue(0, 3);
    expectedBuildElementVector->SetValue(1, 4);
    expectedBuildElementVector->SetValue(2, 7);
    expectedBuildElementVector->SetValue(3, 8);
    auto expectedBuildArrayVec =  new ArrayVector(expectedDataSize, expectedBuildElementVector);
    expectedBuildArrayVec->SetSize(0, 2);
    expectedBuildArrayVec->SetNull(1);
    expectedBuildArrayVec->SetSize(2, 2);
    expectedBuildArrayVec->SetNull(3);

    auto *expectedVecBatch = new VectorBatch(expectedDataSize);
    expectedVecBatch->Append(CreateVector(expectedDataSize, expectedProbeCol0));
    expectedVecBatch->Append(CreateVector(expectedDataSize, expectedProbeCol1));
    expectedVecBatch->Append(expectedProbeArrayVec);

    int size = 4;
    auto* expectedBuildCol0 = new vec::Vector<int64_t>(size);
    auto* expectedBuildCol1 = new vec::Vector<int64_t>(size);
    expectedBuildCol0->SetValue(0, 2);
    expectedBuildCol0->SetNull(1);
    expectedBuildCol0->SetValue(2, 4);
    expectedBuildCol0->SetNull(3);
    expectedBuildCol1->SetValue(0, 11);
    expectedBuildCol1->SetNull(1);
    expectedBuildCol1->SetValue(2, 33);
    expectedBuildCol1->SetNull(3);
    expectedVecBatch->Append(expectedBuildCol0);
    expectedVecBatch->Append(expectedBuildCol1);
    expectedVecBatch->Append(expectedBuildArrayVec);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(lookupJoinOutputVecBatch, expectedVecBatch));

    Expr::DeleteExprs(buildHashKeys);
    Expr::DeleteExprs(probeHashKeys);
    VectorHelper::FreeVecBatch(lookupJoinOutputVecBatch);
    VectorHelper::FreeVecBatch(expectedVecBatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinWithExprOperator);
    DeleteJoinExprOperatorFactory(hashBuilderWithExprOperatorFactory, lookupJoinWithExprOperatorFactory);
    delete overflowConfig;
}

TEST(JoinWithExprTest, TestInnerEqualityJoinOnKeyWithoutExprWithMapVector)
{
    // construct input data
    const int32_t dataSize = 4;

    auto mapColType = std::make_shared<omniruntime::type::MapType>(LongType(), LongType());
    DataTypes buildTypes(std::vector<DataTypePtr>({ LongType(), LongType(), mapColType }));
    int64_t buildData0[] = {1, 2, 3, 4};
    int64_t buildData1[] = {111, 11, 333, 33};
    auto *buildVecBatch = new VectorBatch(dataSize);
    buildVecBatch->Append(CreateVector(dataSize, buildData0));
    int32_t ids[] = {0, 1, 2, 3};
    buildVecBatch->Append(CreateDictionaryVector(*buildTypes.GetType(1), dataSize, ids, dataSize, buildData1));

    auto buildMapKeyElementVector = std::make_shared<Vector<int64_t>>(8, OMNI_LONG);
    auto buildMapValueElementVector = std::make_shared<Vector<int64_t>>(8, OMNI_LONG);

    int64_t buildKeyVals[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    int64_t buildValueVals[8] = {10, 20, 30, 40, 50, 60, 70, 80};

    for (int i = 0; i < 8; i++) {
        buildMapKeyElementVector->SetValue(i, buildKeyVals[i]);
        buildMapValueElementVector->SetValue(i, buildValueVals[i]);
    }
    auto buildMapVec = new omniruntime::vec::MapVector(dataSize, buildMapKeyElementVector, buildMapValueElementVector);
    for (int row = 0; row <= dataSize; row++) {
        buildMapVec->SetOffset(row, row * 2);
    }
    buildVecBatch->Append(buildMapVec);

    std::vector<omniruntime::expressions::Expr *> buildHashKeys = { new omniruntime::expressions::FieldExpr(1,
                                                                                                            LongType()) };
    int32_t hashTableCount = 1;
    auto overflowConfig = new OverflowConfig();
    HashBuilderWithExprOperatorFactory *hashBuilderWithExprOperatorFactory =
        HashBuilderWithExprOperatorFactory::CreateHashBuilderWithExprOperatorFactory(OMNI_JOIN_TYPE_INNER, buildTypes,
            buildHashKeys, hashTableCount, overflowConfig);
    auto *hashBuilderWithExprOperator = CreateTestOperator(hashBuilderWithExprOperatorFactory);
    hashBuilderWithExprOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuilderOutput = nullptr;
    hashBuilderWithExprOperator->GetOutput(&hashBuilderOutput);

    DataTypes probeTypes(std::vector<DataTypePtr>({ LongType(), LongType(), mapColType }));
    int64_t probeData0[] = {1, 2, 3, 4};
    int64_t probeData1[] = {11, 22, 33, 44};
    auto probeMapKeyElementVector = std::make_shared<Vector<int64_t>>(8, OMNI_LONG);
    auto probeMapValueElementVector = std::make_shared<Vector<int64_t>>(8, OMNI_LONG);

    int64_t probeKeyVals[8] = {8, 7, 6, 5, 4, 3, 2, 1};
    int64_t probeValueVals[8] = {80, 70, 60, 50, 40, 30, 20, 10};
    for (int i = 0; i < 8; i++) {
        probeMapKeyElementVector->SetValue(i, probeKeyVals[i]);
        probeMapValueElementVector->SetValue(i, probeValueVals[i]);
    }
    auto probeMapVec = new omniruntime::vec::MapVector(dataSize, probeMapKeyElementVector, probeMapValueElementVector);
    for (int row = 0; row <= dataSize; row++) {
        probeMapVec->SetOffset(row, row * 2);
    }
    auto *probeVecBatch = new VectorBatch(dataSize);
    probeVecBatch->Append(CreateVector(dataSize, probeData0));
    probeVecBatch->Append(CreateDictionaryVector(*probeTypes.GetType(1), dataSize, ids, dataSize, probeData1));
    probeVecBatch->Append(probeMapVec);
    int32_t probeOutputCols[3]= {0, 1, 2};
    int32_t probeOutputColsCount = 3;
    std::vector<omniruntime::expressions::Expr *> probeHashKeys = { new omniruntime::expressions::FieldExpr(1,
                                                                                                            LongType()) };
    int32_t probeHashKeysCount = 1;
    int32_t buildOutputCols[3] = {0, 1, 2};
    int32_t buildOutputColsCount = 3;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({ LongType(), LongType(), mapColType }));
    HashBuilderOperatorFactory* hashBuilderFactory = hashBuilderWithExprOperatorFactory->GetHashBuilderOperatorFactory();
    auto lookupJoinWithExprOperatorFactory = LookupJoinWithExprOperatorFactory::CreateLookupJoinWithExprOperatorFactory(
        probeTypes, probeOutputCols, probeOutputColsCount, probeHashKeys, probeHashKeysCount, buildOutputCols,
        buildOutputColsCount, buildOutputTypes, (int64_t)hashBuilderFactory, nullptr, false, overflowConfig);
    auto lookupJoinWithExprOperator = CreateTestOperator(lookupJoinWithExprOperatorFactory);
    lookupJoinWithExprOperator->AddInput(probeVecBatch);
    VectorBatch *lookupJoinOutputVecBatch = nullptr;
    lookupJoinWithExprOperator->GetOutput(&lookupJoinOutputVecBatch);

    const int32_t expectedDataSize = 2;
    int64_t expectedProbeCol0[] = {1, 3};
    int64_t expectedProbeCol1[] = {11, 33};
    auto expectedProbeMapKeyElementVector = std::make_shared<Vector<int64_t>>(4, OMNI_LONG);
    auto expectedProbeMapValueElementVector = std::make_shared<Vector<int64_t>>(4, OMNI_LONG);
    int64_t expectedprobeKeyVals[4] = {8, 7, 4, 3};
    int64_t expectedprobeValueVals[4] = {80, 70, 40, 30};
    for (int i = 0; i < 4; i++) {
        expectedProbeMapKeyElementVector->SetValue(i, expectedprobeKeyVals[i]);
        expectedProbeMapValueElementVector->SetValue(i, expectedprobeValueVals[i]);
    }
    auto expectedProbeMapVec = new omniruntime::vec::MapVector(expectedDataSize, expectedProbeMapKeyElementVector, expectedProbeMapValueElementVector);
    for (int row = 0; row <= expectedDataSize; row++) {
        expectedProbeMapVec->SetOffset(row, row * 2);
    }

    int64_t expectedBuildCol0[] = {2, 4};
    int64_t expectedBuildCol1[] = {11, 33};
    auto expectedBuildMapKeyElementVector = std::make_shared<Vector<int64_t>>(4, OMNI_LONG);
    auto expectedBuildMapValueElementVector = std::make_shared<Vector<int64_t>>(4, OMNI_LONG);
    int64_t expectedBuildKeyVals[4] = {3, 4, 7, 8};
    int64_t expectedBuildValueVals[4] = {30, 40, 70, 80};
    for (int i = 0; i < 4; i++) {
        expectedBuildMapKeyElementVector->SetValue(i, expectedBuildKeyVals[i]);
        expectedBuildMapValueElementVector->SetValue(i, expectedBuildValueVals[i]);
    }
    auto expectedBuildMapVec = new omniruntime::vec::MapVector(expectedDataSize, expectedBuildMapKeyElementVector, expectedBuildMapValueElementVector);
    for (int row = 0; row <= expectedDataSize; row++) {
        expectedBuildMapVec->SetOffset(row, row * 2);
    }

    auto *expectedVecBatch = new VectorBatch(expectedDataSize);
    expectedVecBatch->Append(CreateVector(expectedDataSize, expectedProbeCol0));
    expectedVecBatch->Append(CreateVector(expectedDataSize, expectedProbeCol1));
    expectedVecBatch->Append(expectedProbeMapVec);
    expectedVecBatch->Append(CreateVector(expectedDataSize, expectedBuildCol0));
    expectedVecBatch->Append(CreateVector(expectedDataSize, expectedBuildCol1));
    expectedVecBatch->Append(expectedBuildMapVec);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(lookupJoinOutputVecBatch, expectedVecBatch));

    Expr::DeleteExprs(buildHashKeys);
    Expr::DeleteExprs(probeHashKeys);
    VectorHelper::FreeVecBatch(lookupJoinOutputVecBatch);
    VectorHelper::FreeVecBatch(expectedVecBatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinWithExprOperator);
    DeleteJoinExprOperatorFactory(hashBuilderWithExprOperatorFactory, lookupJoinWithExprOperatorFactory);
    delete overflowConfig;
}

TEST(JoinWithExprTest, TestFullEqualityJoinOnKeyWithExpr)
{
    // construct input data
    const int32_t dataSize = 4;
    DataTypes buildTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    int64_t buildData0[] = {1, 2, 3, 4};
    int64_t buildData1[] = {111, 11, 333, 33};
    auto *buildVecBatch = new VectorBatch(dataSize);
    buildVecBatch->Append(CreateVector(dataSize, buildData0));
    int32_t ids[] = {0, 1, 2, 3};
    buildVecBatch->Append(CreateDictionaryVector(*buildTypes.GetType(1), dataSize, ids, dataSize, buildData1));

    std::vector<omniruntime::expressions::Expr *> buildHashKeys = CreateBuildHashKeys();
    int32_t hashTableCount = 1;
    HashBuilderWithExprOperatorFactory *hashBuilderWithExprOperatorFactory =
        HashBuilderWithExprOperatorFactory::CreateHashBuilderWithExprOperatorFactory(OMNI_JOIN_TYPE_FULL, buildTypes,
        buildHashKeys, hashTableCount, nullptr);
    auto *hashBuilderWithExprOperator = CreateTestOperator(hashBuilderWithExprOperatorFactory);
    hashBuilderWithExprOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuilderOutput = nullptr;
    hashBuilderWithExprOperator->GetOutput(&hashBuilderOutput);

    DataTypes probeTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    int64_t probeData0[] = {1, 2, 3, 4};
    int64_t probeData1[] = {11, 22, 33, 44};
    auto *probeVecBatch = new VectorBatch(dataSize);
    probeVecBatch->Append(CreateVector(dataSize, probeData0));
    probeVecBatch->Append(CreateDictionaryVector(*probeTypes.GetType(1), dataSize, ids, dataSize, probeData1));

    int32_t probeOutputCols[2]= {0, 1};
    int32_t probeOutputColsCount = 2;
    std::vector<omniruntime::expressions::Expr *> probeHashKeys = CreateProbeHashKeys();
    int32_t probeHashKeysCount = 1;
    int32_t buildOutputCols[2] = {0, 1};
    int32_t buildOutputColsCount = 2;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    HashBuilderOperatorFactory* hashBuilderFactory = hashBuilderWithExprOperatorFactory->GetHashBuilderOperatorFactory();
    auto lookupJoinWithExprOperatorFactory = LookupJoinWithExprOperatorFactory::CreateLookupJoinWithExprOperatorFactory(
        probeTypes, probeOutputCols, probeOutputColsCount, probeHashKeys, probeHashKeysCount, buildOutputCols,
        buildOutputColsCount, buildOutputTypes, (int64_t)hashBuilderFactory, nullptr, false, nullptr);
    auto lookupJoinWithExprOperator = CreateTestOperator(lookupJoinWithExprOperatorFactory);
    auto lookupOuterJoinFactory = LookupOuterJoinWithExprOperatorFactory::CreateLookupOuterJoinWithExprOperatorFactory(
        probeTypes, probeOutputCols, probeOutputColsCount, probeHashKeys, probeHashKeysCount, buildOutputCols,
        buildOutputTypes, (int64_t)hashBuilderFactory);
    auto lookupOuterJoinWithExprOperator = lookupOuterJoinFactory->CreateOperator();
    lookupJoinWithExprOperator->AddInput(probeVecBatch);
    VectorBatch *lookupJoinOutputVecBatch = nullptr;
    lookupJoinWithExprOperator->GetOutput(&lookupJoinOutputVecBatch);

    const int32_t expectedDataSize = 4;
    int64_t expectedDatas[4][expectedDataSize] = {
        {1, 2, 3, 4},
        {11, 22, 33, 44},
        {2, 0, 4, 0},
        {11, 0, 33, 0}};
    DataTypes expectedDataTypes(std::vector<DataTypePtr>({ LongType(), LongType(), LongType(), LongType() }));
    auto *expectedVecbatch = CreateVectorBatch(expectedDataTypes, expectedDataSize, expectedDatas[0], expectedDatas[1],
        expectedDatas[2], expectedDatas[3]);
    expectedVecbatch->Get(2)->SetNull(1);
    expectedVecbatch->Get(2)->SetNull(3);
    expectedVecbatch->Get(3)->SetNull(1);
    expectedVecbatch->Get(3)->SetNull(3);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(lookupJoinOutputVecBatch, expectedVecbatch));

    VectorBatch *appendOutput;
    lookupOuterJoinWithExprOperator->GetOutput(&appendOutput);

    const int32_t expectedDatasSize = 2;
    int64_t expectedData0[expectedDatasSize] = {0, 0};
    int64_t expectedData1[expectedDatasSize] = {0, 0};
    int64_t expectedData2[expectedDatasSize] = {1, 3};
    int64_t expectedData3[expectedDatasSize] = {111, 333};
    auto expectedVec0 = CreateVector(expectedDatasSize, expectedData0);
    auto expectedVec1 = CreateVector(expectedDatasSize, expectedData1);
    auto expectedVec2 = CreateVector(expectedDatasSize, expectedData2);
    auto expectedVec3 = CreateVector(expectedDatasSize, expectedData3);
    auto vectorBatch = new VectorBatch(expectedDatasSize);
    vectorBatch->Append(expectedVec0);
    vectorBatch->Append(expectedVec1);
    vectorBatch->Append(expectedVec2);
    vectorBatch->Append(expectedVec3);
    vectorBatch->Get(0)->SetNull(0);
    vectorBatch->Get(0)->SetNull(1);
    vectorBatch->Get(1)->SetNull(0);
    vectorBatch->Get(1)->SetNull(1);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(appendOutput, vectorBatch));

    Expr::DeleteExprs(buildHashKeys);
    Expr::DeleteExprs(probeHashKeys);
    VectorHelper::FreeVecBatch(lookupJoinOutputVecBatch);
    VectorHelper::FreeVecBatch(expectedVecbatch);
    VectorHelper::FreeVecBatch(appendOutput);
    VectorHelper::FreeVecBatch(vectorBatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(lookupOuterJoinWithExprOperator);
    DeleteJoinExprOperatorFactory(hashBuilderWithExprOperatorFactory, lookupJoinWithExprOperatorFactory,
        lookupOuterJoinFactory);
}

TEST(JoinWithExprTest, TestLeftJoinOnKeyWithoutExprWithArrayVectorWithVarchar)
{
    // construct input data
    const int32_t dataSize = 4;

    auto arrayColType = std::make_shared<omniruntime::type::ArrayType>(VarcharType(500));
    DataTypes buildTypes(std::vector<DataTypePtr>({ LongType(), LongType(), arrayColType }));
    int64_t buildData0[] = {1, 2, 3, 4};
    int64_t buildData1[] = {22, 11, 44, 33};
    auto *buildVecBatch = new VectorBatch(dataSize);
    buildVecBatch->Append(CreateVector(dataSize, buildData0));
    int32_t ids[] = {0, 1, 2, 3};
    buildVecBatch->Append(CreateDictionaryVector(*buildTypes.GetType(1), dataSize, ids, dataSize, buildData1));

    auto buildStringVector = VectorHelper::CreateStringVector(4);
    auto *buildElementVector = (Vector<LargeStringContainer<std::string_view>> *)buildStringVector;
    std::string buildValue;
    for (int i = 0; i < 4; i++) {
        if (i % 3 == 0) {
            buildValue = "he";
        } else {
            buildValue = "hi";
        }
        std::string_view input(buildValue.data(), buildValue.size());
        buildElementVector->SetValue(i, input);
    }
    for (int i = 0; i < 4; i++)  {
        omniruntime::vec::VectorHelper::PrintVectorValue(buildElementVector, i);
    }

    auto buildElementVectorPtr = std::shared_ptr<BaseVector>(buildElementVector);
    auto buildArrayVec = new ArrayVector(dataSize, buildElementVectorPtr);
    for (int row = 0; row < dataSize; row++) {
        buildArrayVec->SetSize(row, 1);
    }
    buildVecBatch->Append(buildArrayVec);

    std::vector<omniruntime::expressions::Expr *> buildHashKeys = { new omniruntime::expressions::FieldExpr(1,
        LongType()) };
    int32_t hashTableCount = 1;
    auto overflowConfig = new OverflowConfig();
    HashBuilderWithExprOperatorFactory *hashBuilderWithExprOperatorFactory =
        HashBuilderWithExprOperatorFactory::CreateHashBuilderWithExprOperatorFactory(OMNI_JOIN_TYPE_LEFT, buildTypes,
            buildHashKeys, hashTableCount, overflowConfig);
    auto *hashBuilderWithExprOperator = CreateTestOperator(hashBuilderWithExprOperatorFactory);
    hashBuilderWithExprOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuilderOutput = nullptr;
    hashBuilderWithExprOperator->GetOutput(&hashBuilderOutput);

    DataTypes probeTypes(std::vector<DataTypePtr>({ LongType(), LongType(), arrayColType }));
    int64_t probeData0[] = {1, 2, 3, 4};
    int64_t probeData1[] = {11, 22, 33, 44};

    auto probeStringVector = VectorHelper::CreateStringVector(4);
    auto *probeElementVector = (Vector<LargeStringContainer<std::string_view>> *)probeStringVector;
    std::string probeValue;
    for (int i = 0; i < 4; i++) {
        if (i % 3 == 0) {
            probeValue = "ab";
        } else {
            probeValue = "de";
        }
        std::string_view input(probeValue.data(), probeValue.size());
        probeElementVector->SetValue(i, input);
    }
    auto probeElementVectorPtr = std::shared_ptr<BaseVector>(probeElementVector);
    auto probeArrayVec = new ArrayVector(dataSize, probeElementVectorPtr);
    for (int row = 0; row < dataSize; row++) {
        probeArrayVec->SetSize(row, 1);
    }
    auto *probeVecBatch = new VectorBatch(dataSize);
    probeVecBatch->Append(CreateVector(dataSize, probeData0));
    probeVecBatch->Append(CreateDictionaryVector(*probeTypes.GetType(1), dataSize, ids, dataSize, probeData1));
    probeVecBatch->Append(probeArrayVec);

    for (int i = 0; i < dataSize; i++) {
        omniruntime::vec::VectorHelper::PrintArrayVectorOffsetsAndNulls(probeArrayVec, i);
    }

    int32_t probeOutputCols[3]= {0, 1, 2};
    int32_t probeOutputColsCount = 3;
    std::vector<omniruntime::expressions::Expr *> probeHashKeys = { new omniruntime::expressions::FieldExpr(1,
                                                                                                                LongType()) };
    int32_t probeHashKeysCount = 1;
    int32_t buildOutputCols[3] = {0, 1, 2};
    int32_t buildOutputColsCount = 3;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({ LongType(), LongType(), arrayColType }));
    HashBuilderOperatorFactory* hashBuilderFactory = hashBuilderWithExprOperatorFactory->GetHashBuilderOperatorFactory();
    auto lookupJoinWithExprOperatorFactory = LookupJoinWithExprOperatorFactory::CreateLookupJoinWithExprOperatorFactory(
        probeTypes, probeOutputCols, probeOutputColsCount, probeHashKeys, probeHashKeysCount, buildOutputCols,
        buildOutputColsCount, buildOutputTypes, (int64_t)hashBuilderFactory, nullptr, false, overflowConfig);
    auto lookupJoinWithExprOperator = CreateTestOperator(lookupJoinWithExprOperatorFactory);
    lookupJoinWithExprOperator->AddInput(probeVecBatch);
    VectorBatch *lookupJoinOutputVecBatch = nullptr;
    lookupJoinWithExprOperator->GetOutput(&lookupJoinOutputVecBatch);

    const int32_t expectedDataSize = 4;
    int64_t expectedProbeCol0[] = {1, 2, 3, 4};
    int64_t expectedProbeCol1[] = {11, 22, 33, 44};

    auto expectedProbeStringVector = VectorHelper::CreateStringVector(4);
    auto *expectedProbeElementVector = (Vector<LargeStringContainer<std::string_view>> *)expectedProbeStringVector;
    std::string expectedProbeValue;
    for (int i = 0; i < 4; i++) {
        if (i % 3 == 0) {
            expectedProbeValue = "ab";
        } else {
            expectedProbeValue = "de";
        }
        std::string_view input(expectedProbeValue.data(), expectedProbeValue.size());
        expectedProbeElementVector->SetValue(i, input);
    }
    auto expectedProbeElementVectorPtr = std::shared_ptr<BaseVector>(expectedProbeElementVector);
    auto expectedProbeArrayVec = new ArrayVector(dataSize, expectedProbeElementVectorPtr);
    for (int row = 0; row < dataSize; row++) {
        expectedProbeArrayVec->SetSize(row, 1);
    }


    auto expectedBuildStringVector = VectorHelper::CreateStringVector(4);
    auto *expectedBuildElementVector = (Vector<LargeStringContainer<std::string_view>> *)expectedBuildStringVector;
    std::string expectedBuildValue;
    for (int i = 0; i < 4; i++) {
        if (i % 3 == 0) {
            expectedBuildValue = "hi";
        } else {
            expectedBuildValue = "he";
        }
        std::string_view input(expectedBuildValue.data(), expectedBuildValue.size());
        expectedBuildElementVector->SetValue(i, input);
    }
    auto expectedBuildElementVectorPtr = std::shared_ptr<BaseVector>(expectedBuildElementVector);
    auto expectedBuildArrayVec = new ArrayVector(dataSize, expectedBuildElementVectorPtr);
    for (int row = 0; row < dataSize; row++) {
        expectedBuildArrayVec->SetSize(row, 1);
    }

    auto *expectedVecBatch = new VectorBatch(expectedDataSize);
    expectedVecBatch->Append(CreateVector(expectedDataSize, expectedProbeCol0));
    expectedVecBatch->Append(CreateVector(expectedDataSize, expectedProbeCol1));
    expectedVecBatch->Append(expectedProbeArrayVec);

    int size = 4;
    auto* expectedBuildCol0 = new vec::Vector<int64_t>(size);
    auto* expectedBuildCol1 = new vec::Vector<int64_t>(size);
    expectedBuildCol0->SetValue(0, 2);
    expectedBuildCol0->SetValue(1, 1);
    expectedBuildCol0->SetValue(2, 4);
    expectedBuildCol0->SetValue(3, 3);
    expectedBuildCol1->SetValue(0, 11);
    expectedBuildCol1->SetValue(1, 22);
    expectedBuildCol1->SetValue(2, 33);
    expectedBuildCol1->SetValue(3, 44);
    expectedVecBatch->Append(expectedBuildCol0);
    expectedVecBatch->Append(expectedBuildCol1);
    expectedVecBatch->Append(expectedBuildArrayVec);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(lookupJoinOutputVecBatch, expectedVecBatch));

    Expr::DeleteExprs(buildHashKeys);
    Expr::DeleteExprs(probeHashKeys);
    VectorHelper::FreeVecBatch(lookupJoinOutputVecBatch);
    VectorHelper::FreeVecBatch(expectedVecBatch);
    omniruntime::op::Operator::DeleteOperator(hashBuilderWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinWithExprOperator);
    DeleteJoinExprOperatorFactory(hashBuilderWithExprOperatorFactory, lookupJoinWithExprOperatorFactory);
    delete overflowConfig;
}

TEST(JoinWithExprTest, TestFullEqualityJoinOnKeyWithoutExpr)
{
    // construct input data
    const int32_t dataSize = 4;
    DataTypes buildTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    int64_t buildData0[] = {1, 2, 3, 4};
    int64_t buildData1[] = {111, 11, 333, 33};
    auto *buildVecBatch = new VectorBatch(dataSize);
    buildVecBatch->Append(CreateVector(dataSize, buildData0));
    int32_t ids[] = {0, 1, 2, 3};
    buildVecBatch->Append(CreateDictionaryVector(*buildTypes.GetType(1), dataSize, ids, dataSize, buildData1));

    std::vector<omniruntime::expressions::Expr *> buildHashKeys = { new omniruntime::expressions::FieldExpr(1,
        LongType()) };
    int32_t hashTableCount = 1;
    HashBuilderWithExprOperatorFactory *hashBuilderWithExprOperatorFactory =
        HashBuilderWithExprOperatorFactory::CreateHashBuilderWithExprOperatorFactory(OMNI_JOIN_TYPE_FULL,
        OMNI_BUILD_RIGHT, buildTypes,
        buildHashKeys, hashTableCount, nullptr);
    auto *hashBuilderWithExprOperator = CreateTestOperator(hashBuilderWithExprOperatorFactory);
    hashBuilderWithExprOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuilderOutput = nullptr;
    hashBuilderWithExprOperator->GetOutput(&hashBuilderOutput);

    DataTypes probeTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    int64_t probeData0[] = {1, 2, 3, 4};
    int64_t probeData1[] = {11, 22, 33, 44};
    auto *probeVecBatch = new VectorBatch(dataSize);
    probeVecBatch->Append(CreateVector(dataSize, probeData0));
    probeVecBatch->Append(CreateDictionaryVector(*probeTypes.GetType(1), dataSize, ids, dataSize, probeData1));

    int32_t probeOutputCols[2]= {0, 1};
    int32_t probeOutputColsCount = 2;
    std::vector<omniruntime::expressions::Expr *> probeHashKeys = { new omniruntime::expressions::FieldExpr(1,
        LongType()) };
    int32_t probeHashKeysCount = 1;
    int32_t buildOutputCols[2] = {0, 1};
    int32_t buildOutputColsCount = 2;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    HashBuilderOperatorFactory* hashBuilderFactory = hashBuilderWithExprOperatorFactory->GetHashBuilderOperatorFactory();
    auto lookupJoinWithExprOperatorFactory = LookupJoinWithExprOperatorFactory::CreateLookupJoinWithExprOperatorFactory(
        probeTypes, probeOutputCols, probeOutputColsCount, probeHashKeys, probeHashKeysCount, buildOutputCols,
        buildOutputColsCount, buildOutputTypes, (int64_t)hashBuilderFactory, nullptr, false, nullptr);
    auto lookupJoinWithExprOperator = CreateTestOperator(lookupJoinWithExprOperatorFactory);
    auto lookupOuterJoinWithExprFactory =
        LookupOuterJoinWithExprOperatorFactory::CreateLookupOuterJoinWithExprOperatorFactory(probeTypes,
        probeOutputCols, probeOutputColsCount, probeHashKeys, probeHashKeysCount, buildOutputCols, buildOutputTypes,
        (int64_t)hashBuilderFactory);
    auto lookupOuterJoinWithExprOperator = lookupOuterJoinWithExprFactory->CreateOperator();
    lookupJoinWithExprOperator->AddInput(probeVecBatch);
    VectorBatch *lookupJoinOutputVecBatch = nullptr;
    lookupJoinWithExprOperator->GetOutput(&lookupJoinOutputVecBatch);

    const int32_t expectedDataSize = 4;
    int64_t expectedDatas[4][expectedDataSize] = {
        {1, 2, 3, 4},
        {11, 22, 33, 44},
        {2, 0, 4, 0},
        {11, 0, 33, 0}};
    DataTypes expectedDataTypes(std::vector<DataTypePtr>({ LongType(), LongType(), LongType(), LongType() }));
    auto *expectedVecbatch = CreateVectorBatch(expectedDataTypes, expectedDataSize, expectedDatas[0], expectedDatas[1],
        expectedDatas[2], expectedDatas[3]);
    expectedVecbatch->Get(2)->SetNull(1);
    expectedVecbatch->Get(2)->SetNull(3);
    expectedVecbatch->Get(3)->SetNull(1);
    expectedVecbatch->Get(3)->SetNull(3);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(lookupJoinOutputVecBatch, expectedVecbatch));

    VectorBatch *appendOutput;
    lookupOuterJoinWithExprOperator->GetOutput(&appendOutput);

    const int32_t expectedDatasSize = 2;
    int64_t expectedData0[expectedDatasSize] = {0, 0};
    int64_t expectedData1[expectedDatasSize] = {0, 0};
    int64_t expectedData2[expectedDatasSize] = {3, 1};
    int64_t expectedData3[expectedDatasSize] = {333, 111};
    auto expectedVec0 = CreateVector(expectedDatasSize, expectedData0);
    auto expectedVec1 = CreateVector(expectedDatasSize, expectedData1);
    auto expectedVec2 = CreateVector(expectedDatasSize, expectedData2);
    auto expectedVec3 = CreateVector(expectedDatasSize, expectedData3);
    auto vectorBatch = new VectorBatch(expectedDatasSize);
    vectorBatch->Append(expectedVec0);
    vectorBatch->Append(expectedVec1);
    vectorBatch->Append(expectedVec2);
    vectorBatch->Append(expectedVec3);
    vectorBatch->Get(0)->SetNull(0);
    vectorBatch->Get(0)->SetNull(1);
    vectorBatch->Get(1)->SetNull(0);
    vectorBatch->Get(1)->SetNull(1);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(appendOutput, vectorBatch));

    Expr::DeleteExprs(buildHashKeys);
    Expr::DeleteExprs(probeHashKeys);
    VectorHelper::FreeVecBatch(lookupJoinOutputVecBatch);
    VectorHelper::FreeVecBatch(expectedVecbatch);
    VectorHelper::FreeVecBatch(appendOutput);
    VectorHelper::FreeVecBatch(vectorBatch);
    omniruntime::op::Operator::DeleteOperator(lookupOuterJoinWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(hashBuilderWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinWithExprOperator);
    DeleteJoinExprOperatorFactory(hashBuilderWithExprOperatorFactory, lookupJoinWithExprOperatorFactory,
        lookupOuterJoinWithExprFactory);
}

TEST(JoinWithExprTest, TestInnerEqualityJoinAddInputTwoVecBatch)
{
    // construct input data
    const int32_t dataSize = 4;
    DataTypes buildTypes(std::vector<DataTypePtr>({ LongType(), LongType(), VarcharType(500000) }));
    int64_t buildData0[] = {1, 2, 3, 4};
    int64_t buildData1[] = {22, 11, 44, 33};
    std::string buildData2[] = {"hello", "world", "bye", "bye"};
    auto buildVecBatch = TestUtil::CreateVectorBatch(buildTypes, dataSize, buildData0, buildData1, buildData2);

    std::vector<omniruntime::expressions::Expr *> buildHashKeys = CreateBuildHashKeys();
    int32_t hashTableCount = 1;
    HashBuilderWithExprOperatorFactory *hashBuilderWithExprOperatorFactory =
        HashBuilderWithExprOperatorFactory::CreateHashBuilderWithExprOperatorFactory(OMNI_JOIN_TYPE_INNER, buildTypes,
        buildHashKeys, hashTableCount, nullptr);
    auto *hashBuilderWithExprOperator = CreateTestOperator(hashBuilderWithExprOperatorFactory);
    hashBuilderWithExprOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuilderOutput = nullptr;
    hashBuilderWithExprOperator->GetOutput(&hashBuilderOutput);

    DataTypes probeTypes(std::vector<DataTypePtr>({ LongType(), LongType(), VarcharType(500000) }));
    int64_t probeData00[] = {1, 2};
    int64_t probeData01[] = {11, 22};
    std::string probeData02[] = {"join", "with"};
    auto probeVecBatch0 = TestUtil::CreateVectorBatch(probeTypes, 2, probeData00, probeData01, probeData02);

    int64_t probeData10[] = {3, 4};
    int64_t probeData11[] = {33, 44};
    std::string probeData12[] = {"expression", "test"};
    auto probeVecBatch1 = TestUtil::CreateVectorBatch(probeTypes, 2, probeData10, probeData11, probeData12);

    int32_t probeOutputCols[3]= {0, 1, 2};
    int32_t probeOutputColsCount = 3;
    std::vector<omniruntime::expressions::Expr *> probeHashKeys = CreateProbeHashKeys();
    int32_t probeHashKeysCount = 1;
    int32_t buildOutputCols[3] = {0, 1, 2};
    int32_t buildOutputColsCount = 3;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({ LongType(), LongType(), VarcharType(500000) }));
    auto overflowConfig = new OverflowConfig();
    HashBuilderOperatorFactory* hashBuilderFactory = hashBuilderWithExprOperatorFactory->GetHashBuilderOperatorFactory();
    auto lookupJoinWithExprOperatorFactory = LookupJoinWithExprOperatorFactory::CreateLookupJoinWithExprOperatorFactory(
        probeTypes, probeOutputCols, probeOutputColsCount, probeHashKeys, probeHashKeysCount, buildOutputCols,
        buildOutputColsCount, buildOutputTypes, (int64_t)hashBuilderFactory, nullptr, false, overflowConfig);
    auto lookupJoinWithExprOperator = CreateTestOperator(lookupJoinWithExprOperatorFactory);

    std::vector<VectorBatch *> lookupJoinOutput;
    lookupJoinWithExprOperator->setNoMoreInput(false);
    lookupJoinWithExprOperator->AddInput(probeVecBatch0);
    while (!lookupJoinWithExprOperator->needsInput() &&
        lookupJoinWithExprOperator->GetStatus() != OMNI_STATUS_FINISHED) {
        VectorBatch *outputVecBatch = nullptr;
        lookupJoinWithExprOperator->GetOutput(&outputVecBatch);
        if (outputVecBatch != nullptr) {
            lookupJoinOutput.push_back(outputVecBatch);
        }
    }
    lookupJoinWithExprOperator->AddInput(probeVecBatch1);
    lookupJoinWithExprOperator->noMoreInput();
    while (lookupJoinWithExprOperator->GetStatus() != OMNI_STATUS_FINISHED) {
        VectorBatch *outputVecBatch = nullptr;
        lookupJoinWithExprOperator->GetOutput(&outputVecBatch);
        if (outputVecBatch != nullptr) {
            lookupJoinOutput.push_back(outputVecBatch);
        }
    }

    EXPECT_EQ(lookupJoinOutput.size(), 2);
    const int32_t expectedDataSize = 2;
    int64_t expectedData00[] = {1, 2};
    int64_t expectedData01[] = {11, 22};
    std::string expectedData02[] = {"join", "with"};
    int64_t expectedData03[] = {2, 1};
    int64_t expectedData04[] = {11, 22};
    std::string expectedData05[] = {"world", "hello"};
    DataTypes expectedDataTypes(std::vector<DataTypePtr>(
        { LongType(), LongType(), VarcharType(50000), LongType(), LongType(), VarcharType(500000) }));
    auto *expectedVecbatch = CreateVectorBatch(expectedDataTypes, expectedDataSize, expectedData00, expectedData01,
        expectedData02, expectedData03, expectedData04, expectedData05);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(lookupJoinOutput[0], expectedVecbatch));

    int64_t expectedData10[] = {3, 4};
    int64_t expectedData11[] = {33, 44};
    std::string expectedData12[] = {"expression", "test"};
    int64_t expectedData13[] = {4, 3};
    int64_t expectedData14[] = {33, 44};
    std::string expectedData15[] = {"bye", "bye"};
    auto *expectedVecbatch1 = CreateVectorBatch(expectedDataTypes, expectedDataSize, expectedData10, expectedData11,
        expectedData12, expectedData13, expectedData14, expectedData15);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(lookupJoinOutput[1], expectedVecbatch1));

    Expr::DeleteExprs(buildHashKeys);
    Expr::DeleteExprs(probeHashKeys);
    VectorHelper::FreeVecBatches(lookupJoinOutput);
    VectorHelper::FreeVecBatch(expectedVecbatch);
    VectorHelper::FreeVecBatch(expectedVecbatch1);
    omniruntime::op::Operator::DeleteOperator(hashBuilderWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinWithExprOperator);
    DeleteJoinExprOperatorFactory(hashBuilderWithExprOperatorFactory, lookupJoinWithExprOperatorFactory);
    delete overflowConfig;
}

TEST(JoinWithExprTest, TestBothJoinKeyAndFilterWithExpr)
{
    // construct input data
    DataTypes buildTypes(std::vector<DataTypePtr>({ Decimal64Type(18, 2) }));
    auto castExpr1 = new FuncExpr("CAST", { new FieldExpr(0, Decimal64Type(18, 2)) }, VarcharType(50));
    auto substrExpr1 = new FuncExpr("substr",
        { castExpr1, new LiteralExpr(1, IntType()), new LiteralExpr(2, IntType()) }, VarcharType(50));
    std::vector<omniruntime::expressions::Expr *> buildHashKeys{ substrExpr1 };
    std::string filter =
        "{\"exprType\":\"IF\",\"returnType\":4,\"condition\":{\"exprType\":\"BINARY\",\"returnType\":4,\"operator\":"
        "\"EQUAL\",\"left\":{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":6,\"colVal\":0,\"precision\":18, "
        "\"scale\":2},\"right\":{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":6,\"colVal\":1,\"precision\":18, "
        "\"scale\":2}},\"if_true\":{\"exprType\":\"BINARY\",\"returnType\":4,\"operator\":\"EQUAL\",\"left\":{"
        "\"exprType\":\"FUNCTION\",\"returnType\":6,\"precision\":18,\"scale\":2,\"function_name\":\"abs\", "
        "\"arguments\":[{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":6,\"colVal\":0,\"precision\":18, "
        "\"scale\":2}]},\"right\":{\"exprType\":\"FUNCTION\",\"returnType\":6,\"precision\":18,\"scale\":2,\"function_"
        "name\":\"abs\", "
        "\"arguments\":[{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":6,\"colVal\":1,\"precision\":18, "
        "\"scale\":2}]}},\"if_false\":{\"exprType\":\"BINARY\",\"returnType\":4,\"operator\":\"LESS_THAN\",\"left\":{"
        "\"exprType\":\"FIELD_REFERENCE\",\"dataType\":6,\"colVal\":0,\"precision\":18, "
        "\"scale\":2},\"right\":{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":6,\"colVal\":1,\"precision\":18, "
        "\"scale\":2}}}";
    int32_t hashTableCount = 1;
    HashBuilderWithExprOperatorFactory *hashBuilderWithExprOperatorFactory =
        HashBuilderWithExprOperatorFactory::CreateHashBuilderWithExprOperatorFactory(OMNI_JOIN_TYPE_LEFT, buildTypes,
        buildHashKeys, hashTableCount, nullptr);
    auto *hashBuilderWithExprOperator = CreateTestOperator(hashBuilderWithExprOperatorFactory);

    const int32_t dataSize = 2;
    int64_t buildData[] = {111, 112};
    auto buildVecBatch = TestUtil::CreateVectorBatch(buildTypes, dataSize, buildData);
    hashBuilderWithExprOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuilderOutput = nullptr;
    hashBuilderWithExprOperator->GetOutput(&hashBuilderOutput);

    DataTypes probeTypes(std::vector<DataTypePtr>({ Decimal64Type(18, 2) }));
    auto filterExpr = JSONParser::ParseJSON(filter);
    int32_t probeOutputCols[]= {0};
    int32_t probeOutputColsCount = 1;
    auto castExpr2 = new FuncExpr("CAST", { new FieldExpr(0, Decimal64Type(18, 2)) }, VarcharType(50));
    auto substrExpr2 = new FuncExpr("substr",
        { castExpr2, new LiteralExpr(1, IntType()), new LiteralExpr(2, IntType()) }, VarcharType(50));
    std::vector<omniruntime::expressions::Expr *> probeHashKeys{ substrExpr2 };
    int32_t probeHashKeysCount = 1;
    int32_t buildOutputCols[] = {0};
    int32_t buildOutputColsCount = 1;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({ Decimal64Type(18, 2) }));
    auto overflowConfig = new OverflowConfig();
    HashBuilderOperatorFactory* hashBuilderFactory = hashBuilderWithExprOperatorFactory->GetHashBuilderOperatorFactory();
    auto lookupJoinWithExprOperatorFactory = LookupJoinWithExprOperatorFactory::CreateLookupJoinWithExprOperatorFactory(
        probeTypes, probeOutputCols, probeOutputColsCount, probeHashKeys, probeHashKeysCount, buildOutputCols,
        buildOutputColsCount, buildOutputTypes, (int64_t)hashBuilderFactory, filterExpr, false, overflowConfig);
    auto lookupJoinWithExprOperator = CreateTestOperator(lookupJoinWithExprOperatorFactory);

    int64_t probeData[] = {111, 112};
    auto probeVecBatch = TestUtil::CreateVectorBatch(probeTypes, dataSize, probeData);
    std::vector<VectorBatch *> lookupJoinOutput;
    lookupJoinWithExprOperator->AddInput(probeVecBatch);
    while (lookupJoinWithExprOperator->GetStatus() != OMNI_STATUS_FINISHED) {
        VectorBatch *outputVecBatch = nullptr;
        lookupJoinWithExprOperator->GetOutput(&outputVecBatch);
        lookupJoinOutput.push_back(outputVecBatch);
    }

    EXPECT_EQ(lookupJoinOutput.size(), 1);
    const int32_t expectedDataSize = 3;
    int64_t expectedData0[] = {111, 111, 112};
    int64_t expectedData1[] = {112, 111, 112};
    std::vector<DataTypePtr> expectedTypes{ LongType(), LongType() };
    AssertVecBatchEquals(lookupJoinOutput[0], probeTypes.GetSize() + buildOutputColsCount, expectedDataSize,
        expectedData0, expectedData1);

    delete filterExpr;
    Expr::DeleteExprs(buildHashKeys);
    Expr::DeleteExprs(probeHashKeys);
    VectorHelper::FreeVecBatches(lookupJoinOutput);
    omniruntime::op::Operator::DeleteOperator(hashBuilderWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(lookupJoinWithExprOperator);
    DeleteJoinExprOperatorFactory(hashBuilderWithExprOperatorFactory, lookupJoinWithExprOperatorFactory);
    delete overflowConfig;
}
}
