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
    std::vector<omniruntime::expressions::Expr *> buildHashKeysExprs = {addExpr};
    return buildHashKeysExprs;
}

std::vector<omniruntime::expressions::Expr *> CreateProbeHashKeys()
{
    auto *addLeftProbe = new omniruntime::expressions::LiteralExpr(50L, LongType());
    auto *addRightProbe = new omniruntime::expressions::FieldExpr(1, LongType());
    auto *addExprProbe = new omniruntime::expressions::BinaryExpr(omniruntime::expressions::Operator::ADD, addLeftProbe,
                                                                  addRightProbe, LongType());
    std::vector<omniruntime::expressions::Expr *> probeHashKeysExprs = {addExprProbe};
    return probeHashKeysExprs;
}

TEST(JoinWithExprTest, TestInnerEqualityJoinOnKeyWithExpr)
{
    // construct input data
    const int32_t dataSize = 4;
    DataTypes buildTypes(std::vector<DataTypePtr>({LongType(), LongType()}));
    int64_t buildData0[] = {1, 2, 3, 4};
    int64_t buildData1[] = {111, 11, 333, 33};
    auto *buildVecBatch = new VectorBatch(dataSize);
    buildVecBatch->Append(CreateVector(dataSize, buildData0));
    int32_t ids[] = {0, 1, 2, 3};
    buildVecBatch->Append(CreateDictionaryVector(*buildTypes.GetType(1), dataSize, ids, dataSize, buildData1));

    std::vector<omniruntime::expressions::Expr *> buildHashKeys = CreateBuildHashKeys();
    int32_t hashTableCount = 1;
    HashBuilderWithExprOperatorFactory *hashBuilderWithExprOperatorFactory =
        HashBuilderWithExprOperatorFactory::CreateHashBuilderWithExprOperatorFactory(
            OMNI_JOIN_TYPE_INNER, buildTypes, buildHashKeys, hashTableCount, nullptr);
    auto *hashBuilderWithExprOperator = CreateTestOperator(hashBuilderWithExprOperatorFactory);
    hashBuilderWithExprOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuilderOutput = nullptr;
    hashBuilderWithExprOperator->GetOutput(&hashBuilderOutput);

    DataTypes probeTypes(std::vector<DataTypePtr>({LongType(), LongType()}));
    int64_t probeData0[] = {1, 2, 3, 4};
    int64_t probeData1[] = {11, 22, 33, 44};

    auto *probeVecBatch = new VectorBatch(dataSize);
    probeVecBatch->Append(CreateVector(dataSize, probeData0));
    probeVecBatch->Append(CreateDictionaryVector(*probeTypes.GetType(1), dataSize, ids, dataSize, probeData1));

    int32_t probeOutputCols[2] = {0, 1};
    int32_t probeOutputColsCount = 2;
    std::vector<omniruntime::expressions::Expr *> probeHashKeys = CreateProbeHashKeys();
    int32_t probeHashKeysCount = 1;
    int32_t buildOutputCols[2] = {0, 1};
    int32_t buildOutputColsCount = 2;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({LongType(), LongType()}));
    auto overflowConfig = new OverflowConfig();
    auto hashBuilderFactoryAddr = (int64_t)hashBuilderWithExprOperatorFactory;
    auto lookupJoinWithExprOperatorFactory = LookupJoinWithExprOperatorFactory::CreateLookupJoinWithExprOperatorFactory(
        probeTypes, probeOutputCols, probeOutputColsCount, probeHashKeys, probeHashKeysCount, buildOutputCols,
        buildOutputColsCount, buildOutputTypes, hashBuilderFactoryAddr, nullptr, false, overflowConfig);
    auto lookupJoinWithExprOperator = CreateTestOperator(lookupJoinWithExprOperatorFactory);
    lookupJoinWithExprOperator->AddInput(probeVecBatch);
    VectorBatch *lookupJoinOutputVecBatch = nullptr;
    lookupJoinWithExprOperator->GetOutput(&lookupJoinOutputVecBatch);

    const int32_t expectedDataSize = 2;
    int64_t expectedDatas[4][expectedDataSize] = {{1, 3}, {11, 33}, {2, 4}, {11, 33}};
    std::vector<DataTypePtr> expectedTypes{LongType(), LongType(), LongType(), LongType()};
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
    DataTypes buildTypes(std::vector<DataTypePtr>({LongType(), LongType()}));
    int64_t buildData0[] = {1, 2, 3, 4};
    int64_t buildData1[] = {111, 11, 333, 33};
    auto *buildVecBatch = new VectorBatch(dataSize);
    buildVecBatch->Append(CreateVector(dataSize, buildData0));
    int32_t ids[] = {0, 1, 2, 3};
    buildVecBatch->Append(CreateDictionaryVector(*buildTypes.GetType(1), dataSize, ids, dataSize, buildData1));

    std::vector<omniruntime::expressions::Expr *> buildHashKeys = {
        new omniruntime::expressions::FieldExpr(1, LongType())};
    int32_t hashTableCount = 1;
    auto overflowConfig = new OverflowConfig();
    HashBuilderWithExprOperatorFactory *hashBuilderWithExprOperatorFactory =
        HashBuilderWithExprOperatorFactory::CreateHashBuilderWithExprOperatorFactory(
            OMNI_JOIN_TYPE_INNER, buildTypes, buildHashKeys, hashTableCount, overflowConfig);
    auto *hashBuilderWithExprOperator = CreateTestOperator(hashBuilderWithExprOperatorFactory);
    hashBuilderWithExprOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuilderOutput = nullptr;
    hashBuilderWithExprOperator->GetOutput(&hashBuilderOutput);

    DataTypes probeTypes(std::vector<DataTypePtr>({LongType(), LongType()}));
    int64_t probeData0[] = {1, 2, 3, 4};
    int64_t probeData1[] = {11, 22, 33, 44};
    auto *probeVecBatch = new VectorBatch(dataSize);
    probeVecBatch->Append(CreateVector(dataSize, probeData0));
    probeVecBatch->Append(CreateDictionaryVector(*probeTypes.GetType(1), dataSize, ids, dataSize, probeData1));

    int32_t probeOutputCols[2] = {0, 1};
    int32_t probeOutputColsCount = 2;
    std::vector<omniruntime::expressions::Expr *> probeHashKeys = {
        new omniruntime::expressions::FieldExpr(1, LongType())};
    int32_t probeHashKeysCount = 1;
    int32_t buildOutputCols[2] = {0, 1};
    int32_t buildOutputColsCount = 2;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({LongType(), LongType()}));
    auto hashBuilderFactoryAddr = (int64_t)hashBuilderWithExprOperatorFactory;
    auto lookupJoinWithExprOperatorFactory = LookupJoinWithExprOperatorFactory::CreateLookupJoinWithExprOperatorFactory(
        probeTypes, probeOutputCols, probeOutputColsCount, probeHashKeys, probeHashKeysCount, buildOutputCols,
        buildOutputColsCount, buildOutputTypes, hashBuilderFactoryAddr, nullptr, false, overflowConfig);
    auto lookupJoinWithExprOperator = CreateTestOperator(lookupJoinWithExprOperatorFactory);
    lookupJoinWithExprOperator->AddInput(probeVecBatch);
    VectorBatch *lookupJoinOutputVecBatch = nullptr;
    lookupJoinWithExprOperator->GetOutput(&lookupJoinOutputVecBatch);

    const int32_t expectedDataSize = 2;
    int64_t expectedDatas[4][expectedDataSize] = {{1, 3}, {11, 33}, {2, 4}, {11, 33}};
    std::vector<DataTypePtr> expectedTypes{LongType(), LongType(), LongType(), LongType()};
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

TEST(JoinWithExprTest, TestFullEqualityJoinOnKeyWithExpr)
{
    // construct input data
    const int32_t dataSize = 4;
    DataTypes buildTypes(std::vector<DataTypePtr>({LongType(), LongType()}));
    int64_t buildData0[] = {1, 2, 3, 4};
    int64_t buildData1[] = {111, 11, 333, 33};
    auto *buildVecBatch = new VectorBatch(dataSize);
    buildVecBatch->Append(CreateVector(dataSize, buildData0));
    int32_t ids[] = {0, 1, 2, 3};
    buildVecBatch->Append(CreateDictionaryVector(*buildTypes.GetType(1), dataSize, ids, dataSize, buildData1));

    std::vector<omniruntime::expressions::Expr *> buildHashKeys = CreateBuildHashKeys();
    int32_t hashTableCount = 1;
    HashBuilderWithExprOperatorFactory *hashBuilderWithExprOperatorFactory =
        HashBuilderWithExprOperatorFactory::CreateHashBuilderWithExprOperatorFactory(
            OMNI_JOIN_TYPE_FULL, buildTypes, buildHashKeys, hashTableCount, nullptr);
    auto *hashBuilderWithExprOperator = CreateTestOperator(hashBuilderWithExprOperatorFactory);
    hashBuilderWithExprOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuilderOutput = nullptr;
    hashBuilderWithExprOperator->GetOutput(&hashBuilderOutput);

    DataTypes probeTypes(std::vector<DataTypePtr>({LongType(), LongType()}));
    int64_t probeData0[] = {1, 2, 3, 4};
    int64_t probeData1[] = {11, 22, 33, 44};
    auto *probeVecBatch = new VectorBatch(dataSize);
    probeVecBatch->Append(CreateVector(dataSize, probeData0));
    probeVecBatch->Append(CreateDictionaryVector(*probeTypes.GetType(1), dataSize, ids, dataSize, probeData1));

    int32_t probeOutputCols[2] = {0, 1};
    int32_t probeOutputColsCount = 2;
    std::vector<omniruntime::expressions::Expr *> probeHashKeys = CreateProbeHashKeys();
    int32_t probeHashKeysCount = 1;
    int32_t buildOutputCols[2] = {0, 1};
    int32_t buildOutputColsCount = 2;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({LongType(), LongType()}));
    auto hashBuilderFactoryAddr = (int64_t)hashBuilderWithExprOperatorFactory;
    auto lookupJoinWithExprOperatorFactory = LookupJoinWithExprOperatorFactory::CreateLookupJoinWithExprOperatorFactory(
        probeTypes, probeOutputCols, probeOutputColsCount, probeHashKeys, probeHashKeysCount, buildOutputCols,
        buildOutputColsCount, buildOutputTypes, hashBuilderFactoryAddr, nullptr, false, nullptr);
    auto lookupJoinWithExprOperator = CreateTestOperator(lookupJoinWithExprOperatorFactory);
    auto lookupOuterJoinFactory = LookupOuterJoinWithExprOperatorFactory::CreateLookupOuterJoinWithExprOperatorFactory(
        probeTypes, probeOutputCols, probeOutputColsCount, probeHashKeys, probeHashKeysCount, buildOutputCols,
        buildOutputTypes, hashBuilderFactoryAddr);
    auto lookupOuterJoinWithExprOperator = lookupOuterJoinFactory->CreateOperator();
    lookupJoinWithExprOperator->AddInput(probeVecBatch);
    VectorBatch *lookupJoinOutputVecBatch = nullptr;
    lookupJoinWithExprOperator->GetOutput(&lookupJoinOutputVecBatch);

    const int32_t expectedDataSize = 4;
    int64_t expectedDatas[4][expectedDataSize] = {{1, 2, 3, 4}, {11, 22, 33, 44}, {2, 0, 4, 0}, {11, 0, 33, 0}};
    DataTypes expectedDataTypes(std::vector<DataTypePtr>({LongType(), LongType(), LongType(), LongType()}));
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

TEST(JoinWithExprTest, TestFullEqualityJoinOnKeyWithoutExpr)
{
    // construct input data
    const int32_t dataSize = 4;
    DataTypes buildTypes(std::vector<DataTypePtr>({LongType(), LongType()}));
    int64_t buildData0[] = {1, 2, 3, 4};
    int64_t buildData1[] = {111, 11, 333, 33};
    auto *buildVecBatch = new VectorBatch(dataSize);
    buildVecBatch->Append(CreateVector(dataSize, buildData0));
    int32_t ids[] = {0, 1, 2, 3};
    buildVecBatch->Append(CreateDictionaryVector(*buildTypes.GetType(1), dataSize, ids, dataSize, buildData1));

    std::vector<omniruntime::expressions::Expr *> buildHashKeys = {
        new omniruntime::expressions::FieldExpr(1, LongType())};
    int32_t hashTableCount = 1;
    HashBuilderWithExprOperatorFactory *hashBuilderWithExprOperatorFactory =
        HashBuilderWithExprOperatorFactory::CreateHashBuilderWithExprOperatorFactory(
            OMNI_JOIN_TYPE_FULL, OMNI_BUILD_RIGHT, buildTypes, buildHashKeys, hashTableCount, nullptr);
    auto *hashBuilderWithExprOperator = CreateTestOperator(hashBuilderWithExprOperatorFactory);
    hashBuilderWithExprOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuilderOutput = nullptr;
    hashBuilderWithExprOperator->GetOutput(&hashBuilderOutput);

    DataTypes probeTypes(std::vector<DataTypePtr>({LongType(), LongType()}));
    int64_t probeData0[] = {1, 2, 3, 4};
    int64_t probeData1[] = {11, 22, 33, 44};
    auto *probeVecBatch = new VectorBatch(dataSize);
    probeVecBatch->Append(CreateVector(dataSize, probeData0));
    probeVecBatch->Append(CreateDictionaryVector(*probeTypes.GetType(1), dataSize, ids, dataSize, probeData1));

    int32_t probeOutputCols[2] = {0, 1};
    int32_t probeOutputColsCount = 2;
    std::vector<omniruntime::expressions::Expr *> probeHashKeys = {
        new omniruntime::expressions::FieldExpr(1, LongType())};
    int32_t probeHashKeysCount = 1;
    int32_t buildOutputCols[2] = {0, 1};
    int32_t buildOutputColsCount = 2;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({LongType(), LongType()}));
    auto hashBuilderFactoryAddr = (int64_t)hashBuilderWithExprOperatorFactory;
    auto lookupJoinWithExprOperatorFactory = LookupJoinWithExprOperatorFactory::CreateLookupJoinWithExprOperatorFactory(
        probeTypes, probeOutputCols, probeOutputColsCount, probeHashKeys, probeHashKeysCount, buildOutputCols,
        buildOutputColsCount, buildOutputTypes, hashBuilderFactoryAddr, nullptr, false, nullptr);
    auto lookupJoinWithExprOperator = CreateTestOperator(lookupJoinWithExprOperatorFactory);
    auto lookupOuterJoinWithExprFactory =
        LookupOuterJoinWithExprOperatorFactory::CreateLookupOuterJoinWithExprOperatorFactory(
            probeTypes, probeOutputCols, probeOutputColsCount, probeHashKeys, probeHashKeysCount, buildOutputCols,
            buildOutputTypes, hashBuilderFactoryAddr);
    auto lookupOuterJoinWithExprOperator = lookupOuterJoinWithExprFactory->CreateOperator();
    lookupJoinWithExprOperator->AddInput(probeVecBatch);
    VectorBatch *lookupJoinOutputVecBatch = nullptr;
    lookupJoinWithExprOperator->GetOutput(&lookupJoinOutputVecBatch);

    const int32_t expectedDataSize = 4;
    int64_t expectedDatas[4][expectedDataSize] = {{1, 2, 3, 4}, {11, 22, 33, 44}, {2, 0, 4, 0}, {11, 0, 33, 0}};
    DataTypes expectedDataTypes(std::vector<DataTypePtr>({LongType(), LongType(), LongType(), LongType()}));
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
    DataTypes buildTypes(std::vector<DataTypePtr>({LongType(), LongType(), VarcharType(500000)}));
    int64_t buildData0[] = {1, 2, 3, 4};
    int64_t buildData1[] = {22, 11, 44, 33};
    std::string buildData2[] = {"hello", "world", "bye", "bye"};
    auto buildVecBatch = TestUtil::CreateVectorBatch(buildTypes, dataSize, buildData0, buildData1, buildData2);

    std::vector<omniruntime::expressions::Expr *> buildHashKeys = CreateBuildHashKeys();
    int32_t hashTableCount = 1;
    HashBuilderWithExprOperatorFactory *hashBuilderWithExprOperatorFactory =
        HashBuilderWithExprOperatorFactory::CreateHashBuilderWithExprOperatorFactory(
            OMNI_JOIN_TYPE_INNER, buildTypes, buildHashKeys, hashTableCount, nullptr);
    auto *hashBuilderWithExprOperator = CreateTestOperator(hashBuilderWithExprOperatorFactory);
    hashBuilderWithExprOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuilderOutput = nullptr;
    hashBuilderWithExprOperator->GetOutput(&hashBuilderOutput);

    DataTypes probeTypes(std::vector<DataTypePtr>({LongType(), LongType(), VarcharType(500000)}));
    int64_t probeData00[] = {1, 2};
    int64_t probeData01[] = {11, 22};
    std::string probeData02[] = {"join", "with"};
    auto probeVecBatch0 = TestUtil::CreateVectorBatch(probeTypes, 2, probeData00, probeData01, probeData02);

    int64_t probeData10[] = {3, 4};
    int64_t probeData11[] = {33, 44};
    std::string probeData12[] = {"expression", "test"};
    auto probeVecBatch1 = TestUtil::CreateVectorBatch(probeTypes, 2, probeData10, probeData11, probeData12);

    int32_t probeOutputCols[3] = {0, 1, 2};
    int32_t probeOutputColsCount = 3;
    std::vector<omniruntime::expressions::Expr *> probeHashKeys = CreateProbeHashKeys();
    int32_t probeHashKeysCount = 1;
    int32_t buildOutputCols[3] = {0, 1, 2};
    int32_t buildOutputColsCount = 3;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({LongType(), LongType(), VarcharType(500000)}));
    auto overflowConfig = new OverflowConfig();
    auto hashBuilderFactoryAddr = (int64_t)hashBuilderWithExprOperatorFactory;
    auto lookupJoinWithExprOperatorFactory = LookupJoinWithExprOperatorFactory::CreateLookupJoinWithExprOperatorFactory(
        probeTypes, probeOutputCols, probeOutputColsCount, probeHashKeys, probeHashKeysCount, buildOutputCols,
        buildOutputColsCount, buildOutputTypes, hashBuilderFactoryAddr, nullptr, false, overflowConfig);
    auto lookupJoinWithExprOperator = CreateTestOperator(lookupJoinWithExprOperatorFactory);

    std::vector<VectorBatch *> lookupJoinOutput;
    lookupJoinWithExprOperator->AddInput(probeVecBatch0);
    while (lookupJoinWithExprOperator->GetStatus() != OMNI_STATUS_FINISHED) {
        VectorBatch *outputVecBatch = nullptr;
        lookupJoinWithExprOperator->GetOutput(&outputVecBatch);
        lookupJoinOutput.push_back(outputVecBatch);
    }
    lookupJoinWithExprOperator->AddInput(probeVecBatch1);
    while (lookupJoinWithExprOperator->GetStatus() != OMNI_STATUS_FINISHED) {
        VectorBatch *outputVecBatch = nullptr;
        lookupJoinWithExprOperator->GetOutput(&outputVecBatch);
        lookupJoinOutput.push_back(outputVecBatch);
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
        {LongType(), LongType(), VarcharType(50000), LongType(), LongType(), VarcharType(500000)}));
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
    DataTypes buildTypes(std::vector<DataTypePtr>({Decimal64Type(18, 2)}));
    auto castExpr1 = new FuncExpr("CAST", {new FieldExpr(0, Decimal64Type(18, 2))}, VarcharType(50));
    auto substrExpr1 = new FuncExpr("substr", {castExpr1, new LiteralExpr(1, IntType()), new LiteralExpr(2, IntType())},
                                    VarcharType(50));
    std::vector<omniruntime::expressions::Expr *> buildHashKeys{substrExpr1};
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
        HashBuilderWithExprOperatorFactory::CreateHashBuilderWithExprOperatorFactory(
            OMNI_JOIN_TYPE_LEFT, buildTypes, buildHashKeys, hashTableCount, nullptr);
    auto *hashBuilderWithExprOperator = CreateTestOperator(hashBuilderWithExprOperatorFactory);

    const int32_t dataSize = 2;
    int64_t buildData[] = {111, 112};
    auto buildVecBatch = TestUtil::CreateVectorBatch(buildTypes, dataSize, buildData);
    hashBuilderWithExprOperator->AddInput(buildVecBatch);
    VectorBatch *hashBuilderOutput = nullptr;
    hashBuilderWithExprOperator->GetOutput(&hashBuilderOutput);

    DataTypes probeTypes(std::vector<DataTypePtr>({Decimal64Type(18, 2)}));
    auto filterExpr = JSONParser::ParseJSON(filter);
    int32_t probeOutputCols[] = {0};
    int32_t probeOutputColsCount = 1;
    auto castExpr2 = new FuncExpr("CAST", {new FieldExpr(0, Decimal64Type(18, 2))}, VarcharType(50));
    auto substrExpr2 = new FuncExpr("substr", {castExpr2, new LiteralExpr(1, IntType()), new LiteralExpr(2, IntType())},
                                    VarcharType(50));
    std::vector<omniruntime::expressions::Expr *> probeHashKeys{substrExpr2};
    int32_t probeHashKeysCount = 1;
    int32_t buildOutputCols[] = {0};
    int32_t buildOutputColsCount = 1;
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({Decimal64Type(18, 2)}));
    auto overflowConfig = new OverflowConfig();
    auto hashBuilderFactoryAddr = (int64_t)hashBuilderWithExprOperatorFactory;
    auto lookupJoinWithExprOperatorFactory = LookupJoinWithExprOperatorFactory::CreateLookupJoinWithExprOperatorFactory(
        probeTypes, probeOutputCols, probeOutputColsCount, probeHashKeys, probeHashKeysCount, buildOutputCols,
        buildOutputColsCount, buildOutputTypes, hashBuilderFactoryAddr, filterExpr, false, overflowConfig);
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
    std::vector<DataTypePtr> expectedTypes{LongType(), LongType()};
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
} // namespace JoinWithExprTest
