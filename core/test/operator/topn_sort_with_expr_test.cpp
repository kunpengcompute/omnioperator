/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2020. All rights reserved.
 */

#include "gtest/gtest.h"
#include <unordered_map>
#include "operator/topnsort/topn_sort_expr.h"
#include "vector/vector_helper.h"
#include "util/test_util.h"

using namespace omniruntime::vec;
using namespace omniruntime::op;
using namespace omniruntime::expressions;
using namespace std;
using namespace TestUtil;

namespace TopnWithExprTest {
TEST(TopNSortWithExprOperatorTest, TestTopNSortOutputRespectsConfiguredBatchBoundary)
{
    constexpr int32_t dataSize = 6;
    std::string partitionValues[dataSize] = {"a", "a", "b", "b", "c", "c"};
    int64_t sortValues[dataSize] = {1, 2, 1, 2, 1, 2};
    DataTypes sourceTypes(std::vector<DataTypePtr>({ VarcharType(10), LongType() }));
    VectorBatch *input = CreateVectorBatch(sourceTypes, dataSize, partitionValues, sortValues);

    std::vector<omniruntime::expressions::Expr *> partitionKeys = { new FieldExpr(0, VarcharType(10)) };
    std::vector<omniruntime::expressions::Expr *> sortKeys = { new FieldExpr(1, LongType()) };
    std::vector<int32_t> sortAscendings = { 1 };
    std::vector<int32_t> sortNullFirsts = { 0 };
    auto overflowConfig = new OverflowConfig();
    std::unordered_map<std::string, std::string> conf{{config::QueryConfig::KMaxBatchRowCount, "4"}};
    const config::QueryConfig queryConfig(conf);
    auto factory = new TopNSortWithExprOperatorFactory(sourceTypes, 2, false, partitionKeys, sortKeys,
        sortAscendings, sortNullFirsts, overflowConfig, queryConfig);
    auto op = static_cast<TopNSortWithExprOperator *>(CreateTestOperator(factory));

    op->AddInput(input);
    VectorBatch *firstOutput;
    op->GetOutput(&firstOutput);
    EXPECT_EQ(firstOutput->GetRowCount(), 4);
    VectorHelper::FreeVecBatch(firstOutput);

    VectorBatch *secondOutput;
    op->GetOutput(&secondOutput);
    EXPECT_EQ(secondOutput->GetRowCount(), 2);
    VectorHelper::FreeVecBatch(secondOutput);

    Expr::DeleteExprs(partitionKeys);
    Expr::DeleteExprs(sortKeys);
    omniruntime::op::Operator::DeleteOperator(op);
    delete factory;
    delete overflowConfig;
}

TEST(TopNSortWithExprOperatorTest, TestTopNSortDescNullLast)
{
    // construct input data
    const int32_t dataSize = 8;
    std::string data1[dataSize] = {"hi", "hi", "hi", "bye", "bye", "bye", "bye", "bye"};
    int64_t data2[dataSize] = {2L, 5L, 3L, 11L, 4L, 3L, 0L, 23L};
    int64_t data3[dataSize] = {3L, 5L, 8L, 3L, 5L, 3L, 4L, 3L};
    DataTypes sourceTypes(std::vector<DataTypePtr>({ VarcharType(10), LongType(), LongType() }));
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3);

    std::vector<omniruntime::expressions::Expr *> partitionKeys = { new FieldExpr(0, VarcharType(10)) };
    std::vector<omniruntime::expressions::Expr *> sortKeys = { new FieldExpr(2, LongType()) };
    std::vector<int32_t> sortAscendings = { 0 };
    std::vector<int32_t> sortNullFirsts = { 0 };
    auto overflowConfig = new OverflowConfig();
    auto topNSortOperatorFactory = new TopNSortWithExprOperatorFactory(sourceTypes, 3, false, partitionKeys, sortKeys,
        sortAscendings, sortNullFirsts, overflowConfig);
    auto topNSortOperator = static_cast<TopNSortWithExprOperator *>(CreateTestOperator(topNSortOperatorFactory));

    topNSortOperator->AddInput(vecBatch);
    VectorBatch *outputVecBatch;
    topNSortOperator->GetOutput(&outputVecBatch);

    constexpr int32_t expectedDataSize = 8;
    std::string expData1[expectedDataSize] = {"bye", "bye", "bye", "bye", "bye", "hi", "hi", "hi"};
    int64_t expData2[expectedDataSize] = {4, 0, 11, 3, 23, 3, 5, 2};
    int64_t expData3[expectedDataSize] = {5, 4, 3, 3, 3, 8, 5, 3};
    DataTypes expectTypes(std::vector<DataTypePtr>({ VarcharType(10), LongType(), LongType() }));
    VectorBatch *expectVectorBatch = CreateVectorBatch(expectTypes, expectedDataSize, expData1, expData2, expData3);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVectorBatch));

    Expr::DeleteExprs(partitionKeys);
    Expr::DeleteExprs(sortKeys);
    omniruntime::op::Operator::DeleteOperator(topNSortOperator);
    delete topNSortOperatorFactory;
    VectorHelper::FreeVecBatch(expectVectorBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
    delete overflowConfig;
}

TEST(TopNSortWithExprOperatorTest, TestTopNSortAscNullLast)
{
    // construct input data
    const int32_t dataSize = 8;
    std::string data1[dataSize] = {"hi", "hi", "hi", "bye", "bye", "bye", "bye", "bye"};
    int64_t data2[dataSize] = {2L, 5L, 3L, 11L, 4L, 3L, 0L, 23L};
    int64_t data3[dataSize] = {5L, 3L, 8L, 3L, 6L, 6L, 4L, 6L};
    DataTypes sourceTypes(std::vector<DataTypePtr>({ VarcharType(10), LongType(), LongType() }));
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3);

    std::vector<omniruntime::expressions::Expr *> partitionKeys = { new FieldExpr(0, VarcharType(10)) };
    std::vector<omniruntime::expressions::Expr *> sortKeys = { new FieldExpr(2, LongType()) };
    std::vector<int32_t> sortAscendings = { 1 };
    std::vector<int32_t> sortNullFirsts = { 0 };
    auto overflowConfig = new OverflowConfig();
    auto topNSortOperatorFactory = new TopNSortWithExprOperatorFactory(sourceTypes, 3, false, partitionKeys, sortKeys,
        sortAscendings, sortNullFirsts, overflowConfig);
    auto topNSortOperator = static_cast<TopNSortWithExprOperator *>(CreateTestOperator(topNSortOperatorFactory));

    topNSortOperator->AddInput(vecBatch);
    VectorBatch *outputVecBatch;
    topNSortOperator->GetOutput(&outputVecBatch);

    constexpr int32_t expectedDataSize = 8;
    std::string expData1[expectedDataSize] = {"bye", "bye", "bye", "bye", "bye", "hi", "hi", "hi"};
    int64_t expData2[expectedDataSize] = {11, 0, 4, 3, 23, 5, 2, 3};
    int64_t expData3[expectedDataSize] = {3, 4, 6, 6, 6, 3, 5, 8};
    DataTypes expectTypes(std::vector<DataTypePtr>({ VarcharType(10), LongType(), LongType() }));
    VectorBatch *expectVectorBatch = CreateVectorBatch(expectTypes, expectedDataSize, expData1, expData2, expData3);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVectorBatch));

    Expr::DeleteExprs(partitionKeys);
    Expr::DeleteExprs(sortKeys);
    omniruntime::op::Operator::DeleteOperator(topNSortOperator);
    delete topNSortOperatorFactory;
    VectorHelper::FreeVecBatch(expectVectorBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
    delete overflowConfig;
}

TEST(TopNSortWithExprOperatorTest, TestTopNSortAscCase1)
{
    // construct input data
    const int32_t dataSize = 4;
    std::string data1[dataSize] = {"bye", "bye", "bye", "bye"};
    int64_t data2[dataSize] = {2L, 5L, 3L, 11L};
    int64_t data3[dataSize] = {6L, 6L, 3L, 4L};
    DataTypes sourceTypes(std::vector<DataTypePtr>({ VarcharType(10), LongType(), LongType() }));
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3);

    std::vector<omniruntime::expressions::Expr *> partitionKeys = { new FieldExpr(0, VarcharType(10)) };
    std::vector<omniruntime::expressions::Expr *> sortKeys = { new FieldExpr(2, LongType()) };
    std::vector<int32_t> sortAscendings = { 1 };
    std::vector<int32_t> sortNullFirsts = { 0 };
    auto overflowConfig = new OverflowConfig();
    auto topNSortOperatorFactory = new TopNSortWithExprOperatorFactory(sourceTypes, 3, false, partitionKeys, sortKeys,
        sortAscendings, sortNullFirsts, overflowConfig);
    auto topNSortOperator = static_cast<TopNSortWithExprOperator *>(CreateTestOperator(topNSortOperatorFactory));

    topNSortOperator->AddInput(vecBatch);
    VectorBatch *outputVecBatch;
    topNSortOperator->GetOutput(&outputVecBatch);

    constexpr int32_t expectedDataSize = 4;
    std::string expData1[expectedDataSize] = {"bye", "bye", "bye", "bye"};
    int64_t expData2[expectedDataSize] = {3, 11, 2, 5};
    int64_t expData3[expectedDataSize] = {3, 4, 6, 6};
    DataTypes expectTypes(std::vector<DataTypePtr>({ VarcharType(10), LongType(), LongType() }));
    VectorBatch *expectVectorBatch = CreateVectorBatch(expectTypes, expectedDataSize, expData1, expData2, expData3);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVectorBatch));

    Expr::DeleteExprs(partitionKeys);
    Expr::DeleteExprs(sortKeys);
    omniruntime::op::Operator::DeleteOperator(topNSortOperator);
    delete topNSortOperatorFactory;
    VectorHelper::FreeVecBatch(expectVectorBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
    delete overflowConfig;
}

TEST(TopNSortWithExprOperatorTest, TestTopNSortAscCase2)
{
    // construct input data
    const int32_t dataSize = 4;
    std::string data1[dataSize] = {"bye", "bye", "bye", "bye"};
    int64_t data2[dataSize] = {2L, 5L, 3L, 11L};
    int64_t data3[dataSize] = {6L, 6L, 4L, 3L};
    DataTypes sourceTypes(std::vector<DataTypePtr>({ VarcharType(10), LongType(), LongType() }));
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3);

    std::vector<omniruntime::expressions::Expr *> partitionKeys = { new FieldExpr(0, VarcharType(10)) };
    std::vector<omniruntime::expressions::Expr *> sortKeys = { new FieldExpr(2, LongType()) };
    std::vector<int32_t> sortAscendings = { 1 };
    std::vector<int32_t> sortNullFirsts = { 0 };
    auto overflowConfig = new OverflowConfig();
    auto topNSortOperatorFactory = new TopNSortWithExprOperatorFactory(sourceTypes, 3, false, partitionKeys, sortKeys,
        sortAscendings, sortNullFirsts, overflowConfig);
    auto topNSortOperator = static_cast<TopNSortWithExprOperator *>(CreateTestOperator(topNSortOperatorFactory));

    topNSortOperator->AddInput(vecBatch);
    VectorBatch *outputVecBatch;
    topNSortOperator->GetOutput(&outputVecBatch);

    constexpr int32_t expectedDataSize = 4;
    std::string expData1[expectedDataSize] = {"bye", "bye", "bye", "bye"};
    int64_t expData2[expectedDataSize] = {11, 3, 2, 5};
    int64_t expData3[expectedDataSize] = {3, 4, 6, 6};
    DataTypes expectTypes(std::vector<DataTypePtr>({ VarcharType(10), LongType(), LongType() }));
    VectorBatch *expectVectorBatch = CreateVectorBatch(expectTypes, expectedDataSize, expData1, expData2, expData3);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVectorBatch));

    Expr::DeleteExprs(partitionKeys);
    Expr::DeleteExprs(sortKeys);
    omniruntime::op::Operator::DeleteOperator(topNSortOperator);
    delete topNSortOperatorFactory;
    VectorHelper::FreeVecBatch(expectVectorBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
    delete overflowConfig;
}

TEST(TopNSortWithExprOperatorTest, TestTopNSortAscCase3)
{
    // construct input data
    const int32_t dataSize = 5;
    std::string data1[dataSize] = {"bye", "bye", "bye", "bye", "bye"};
    int64_t data2[dataSize] = {2L, 5L, 3L, 11L, 6L};
    int64_t data3[dataSize] = {6L, 6L, 4L, 3L, 5L};
    DataTypes sourceTypes(std::vector<DataTypePtr>({ VarcharType(10), LongType(), LongType() }));
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3);

    std::vector<omniruntime::expressions::Expr *> partitionKeys = { new FieldExpr(0, VarcharType(10)) };
    std::vector<omniruntime::expressions::Expr *> sortKeys = { new FieldExpr(2, LongType()) };
    std::vector<int32_t> sortAscendings = { 1 };
    std::vector<int32_t> sortNullFirsts = { 0 };
    auto overflowConfig = new OverflowConfig();
    auto topNSortOperatorFactory = new TopNSortWithExprOperatorFactory(sourceTypes, 3, false, partitionKeys, sortKeys,
        sortAscendings, sortNullFirsts, overflowConfig);
    auto topNSortOperator = static_cast<TopNSortWithExprOperator *>(CreateTestOperator(topNSortOperatorFactory));

    topNSortOperator->AddInput(vecBatch);
    VectorBatch *outputVecBatch;
    topNSortOperator->GetOutput(&outputVecBatch);

    constexpr int32_t expectedDataSize = 3;
    std::string expData1[expectedDataSize] = {"bye", "bye", "bye"};
    int64_t expData2[expectedDataSize] = {11, 3, 6};
    int64_t expData3[expectedDataSize] = {3, 4, 5};
    DataTypes expectTypes(std::vector<DataTypePtr>({ VarcharType(10), LongType(), LongType() }));
    VectorBatch *expectVectorBatch = CreateVectorBatch(expectTypes, expectedDataSize, expData1, expData2, expData3);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVectorBatch));

    Expr::DeleteExprs(partitionKeys);
    Expr::DeleteExprs(sortKeys);
    omniruntime::op::Operator::DeleteOperator(topNSortOperator);
    delete topNSortOperatorFactory;
    VectorHelper::FreeVecBatch(expectVectorBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
    delete overflowConfig;
}

TEST(TopNSortWithExprOperatorTest, TestTopNSortAscCase4)
{
    // construct input data
    const int32_t dataSize = 5;
    std::string data1[dataSize] = {"bye", "bye", "bye", "bye", "bye"};
    int64_t data2[dataSize] = {2L, 5L, 3L, 11L, 6L};
    int64_t data3[dataSize] = {6L, 6L, 3L, 6L, 3L};
    DataTypes sourceTypes(std::vector<DataTypePtr>({ VarcharType(10), LongType(), LongType() }));
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3);

    std::vector<omniruntime::expressions::Expr *> partitionKeys = { new FieldExpr(0, VarcharType(10)) };
    std::vector<omniruntime::expressions::Expr *> sortKeys = { new FieldExpr(2, LongType()) };
    std::vector<int32_t> sortAscendings = { 1 };
    std::vector<int32_t> sortNullFirsts = { 0 };
    auto overflowConfig = new OverflowConfig();
    auto topNSortOperatorFactory = new TopNSortWithExprOperatorFactory(sourceTypes, 3, false, partitionKeys, sortKeys,
        sortAscendings, sortNullFirsts, overflowConfig);
    auto topNSortOperator = static_cast<TopNSortWithExprOperator *>(CreateTestOperator(topNSortOperatorFactory));

    topNSortOperator->AddInput(vecBatch);
    VectorBatch *outputVecBatch;
    topNSortOperator->GetOutput(&outputVecBatch);

    constexpr int32_t expectedDataSize = 5;
    std::string expData1[expectedDataSize] = {"bye", "bye", "bye", "bye", "bye"};
    int64_t expData2[expectedDataSize] = {3, 6, 2, 5, 11};
    int64_t expData3[expectedDataSize] = {3, 3, 6, 6, 6};
    DataTypes expectTypes(std::vector<DataTypePtr>({ VarcharType(10), LongType(), LongType() }));
    VectorBatch *expectVectorBatch = CreateVectorBatch(expectTypes, expectedDataSize, expData1, expData2, expData3);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVectorBatch));

    Expr::DeleteExprs(partitionKeys);
    Expr::DeleteExprs(sortKeys);
    omniruntime::op::Operator::DeleteOperator(topNSortOperator);
    delete topNSortOperatorFactory;
    VectorHelper::FreeVecBatch(expectVectorBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
    delete overflowConfig;
}

TEST(TopNSortWithExprOperatorTest, TestTopNSortAscCase5)
{
    // construct input data
    const int32_t dataSize = 5;
    std::string data1[dataSize] = {"bye", "bye", "bye", "bye", "bye"};
    int64_t data2[dataSize] = {2L, 5L, 3L, 11L, 6L};
    int64_t data3[dataSize] = {6L, 3L, 5L, 6L, 4L};
    DataTypes sourceTypes(std::vector<DataTypePtr>({ VarcharType(10), LongType(), LongType() }));
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3);

    std::vector<omniruntime::expressions::Expr *> partitionKeys = { new FieldExpr(0, VarcharType(10)) };
    std::vector<omniruntime::expressions::Expr *> sortKeys = { new FieldExpr(2, LongType()) };
    std::vector<int32_t> sortAscendings = { 1 };
    std::vector<int32_t> sortNullFirsts = { 0 };
    auto overflowConfig = new OverflowConfig();
    auto topNSortOperatorFactory = new TopNSortWithExprOperatorFactory(sourceTypes, 3, false, partitionKeys, sortKeys,
        sortAscendings, sortNullFirsts, overflowConfig);
    auto topNSortOperator = static_cast<TopNSortWithExprOperator *>(CreateTestOperator(topNSortOperatorFactory));

    topNSortOperator->AddInput(vecBatch);
    VectorBatch *outputVecBatch;
    topNSortOperator->GetOutput(&outputVecBatch);

    constexpr int32_t expectedDataSize = 3;
    std::string expData1[expectedDataSize] = {"bye", "bye", "bye"};
    int64_t expData2[expectedDataSize] = {5, 6, 3};
    int64_t expData3[expectedDataSize] = {3, 4, 5};
    DataTypes expectTypes(std::vector<DataTypePtr>({ VarcharType(10), LongType(), LongType() }));
    VectorBatch *expectVectorBatch = CreateVectorBatch(expectTypes, expectedDataSize, expData1, expData2, expData3);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVectorBatch));

    Expr::DeleteExprs(partitionKeys);
    Expr::DeleteExprs(sortKeys);
    omniruntime::op::Operator::DeleteOperator(topNSortOperator);
    delete topNSortOperatorFactory;
    VectorHelper::FreeVecBatch(expectVectorBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
    delete overflowConfig;
}

TEST(TopNSortWithExprOperatorTest, TestTopNSortAscCase6)
{
    // construct input data
    const int32_t dataSize = 5;
    std::string data1[dataSize] = {"bye", "bye", "bye", "bye", "bye"};
    int64_t data2[dataSize] = {2L, 5L, 3L, 11L, 6L};
    int64_t data3[dataSize] = {7L, 3L, 5L, 7L, 6L};
    DataTypes sourceTypes(std::vector<DataTypePtr>({ VarcharType(10), LongType(), LongType() }));
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3);

    std::vector<omniruntime::expressions::Expr *> partitionKeys = { new FieldExpr(0, VarcharType(10)) };
    std::vector<omniruntime::expressions::Expr *> sortKeys = { new FieldExpr(2, LongType()) };
    std::vector<int32_t> sortAscendings = { 1 };
    std::vector<int32_t> sortNullFirsts = { 0 };
    auto overflowConfig = new OverflowConfig();
    auto topNSortOperatorFactory = new TopNSortWithExprOperatorFactory(sourceTypes, 4, false, partitionKeys, sortKeys,
        sortAscendings, sortNullFirsts, overflowConfig);
    auto topNSortOperator = static_cast<TopNSortWithExprOperator *>(CreateTestOperator(topNSortOperatorFactory));

    topNSortOperator->AddInput(vecBatch);
    VectorBatch *outputVecBatch;
    topNSortOperator->GetOutput(&outputVecBatch);

    constexpr int32_t expectedDataSize = 5;
    std::string expData1[expectedDataSize] = {"bye", "bye", "bye", "bye", "bye"};
    int64_t expData2[expectedDataSize] = {5, 3, 6, 2, 11};
    int64_t expData3[expectedDataSize] = {3, 5, 6, 7, 7};
    DataTypes expectTypes(std::vector<DataTypePtr>({ VarcharType(10), LongType(), LongType() }));
    VectorBatch *expectVectorBatch = CreateVectorBatch(expectTypes, expectedDataSize, expData1, expData2, expData3);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVectorBatch));

    Expr::DeleteExprs(partitionKeys);
    Expr::DeleteExprs(sortKeys);
    omniruntime::op::Operator::DeleteOperator(topNSortOperator);
    delete topNSortOperatorFactory;
    VectorHelper::FreeVecBatch(expectVectorBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
    delete overflowConfig;
}

TEST(TopNSortWithExprOperatorTest, TestTopNSortAscCase7)
{
    // construct input data
    const int32_t dataSize = 5;
    std::string data1[dataSize] = {"bye", "bye", "bye", "bye", "bye"};
    int64_t data2[dataSize] = {2L, 5L, 3L, 11L, 6L};
    int64_t data3[dataSize] = {8L, 3L, 5L, 7L, 6L};
    DataTypes sourceTypes(std::vector<DataTypePtr>({ VarcharType(10), LongType(), LongType() }));
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3);

    std::vector<omniruntime::expressions::Expr *> partitionKeys = { new FieldExpr(0, VarcharType(10)) };
    std::vector<omniruntime::expressions::Expr *> sortKeys = { new FieldExpr(2, LongType()) };
    std::vector<int32_t> sortAscendings = { 1 };
    std::vector<int32_t> sortNullFirsts = { 0 };
    auto overflowConfig = new OverflowConfig();
    auto topNSortOperatorFactory = new TopNSortWithExprOperatorFactory(sourceTypes, 4, false, partitionKeys, sortKeys,
        sortAscendings, sortNullFirsts, overflowConfig);
    auto topNSortOperator = static_cast<TopNSortWithExprOperator *>(CreateTestOperator(topNSortOperatorFactory));

    topNSortOperator->AddInput(vecBatch);
    VectorBatch *outputVecBatch;
    topNSortOperator->GetOutput(&outputVecBatch);

    constexpr int32_t expectedDataSize = 4;
    std::string expData1[expectedDataSize] = {"bye", "bye", "bye", "bye"};
    int64_t expData2[expectedDataSize] = {5, 3, 6, 11};
    int64_t expData3[expectedDataSize] = {3, 5, 6, 7};
    DataTypes expectTypes(std::vector<DataTypePtr>({ VarcharType(10), LongType(), LongType() }));
    VectorBatch *expectVectorBatch = CreateVectorBatch(expectTypes, expectedDataSize, expData1, expData2, expData3);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVectorBatch));

    Expr::DeleteExprs(partitionKeys);
    Expr::DeleteExprs(sortKeys);
    omniruntime::op::Operator::DeleteOperator(topNSortOperator);
    delete topNSortOperatorFactory;
    VectorHelper::FreeVecBatch(expectVectorBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
    delete overflowConfig;
}

TEST(TopNSortWithExprOperatorTest, TestTopNSortAscCase8)
{
    // construct input data
    const int32_t dataSize = 5;
    std::string data1[dataSize] = {"bye", "bye", "bye", "bye", "bye"};
    int64_t data2[dataSize] = {2L, 5L, 3L, 11L, 6L};
    int64_t data3[dataSize] = {7L, 3L, 5L, 7L, 9L};
    DataTypes sourceTypes(std::vector<DataTypePtr>({ VarcharType(10), LongType(), LongType() }));
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3);

    std::vector<omniruntime::expressions::Expr *> partitionKeys = { new FieldExpr(0, VarcharType(10)) };
    std::vector<omniruntime::expressions::Expr *> sortKeys = { new FieldExpr(2, LongType()) };
    std::vector<int32_t> sortAscendings = { 1 };
    std::vector<int32_t> sortNullFirsts = { 0 };
    auto overflowConfig = new OverflowConfig();
    auto topNSortOperatorFactory = new TopNSortWithExprOperatorFactory(sourceTypes, 4, false, partitionKeys, sortKeys,
        sortAscendings, sortNullFirsts, overflowConfig);
    auto topNSortOperator = static_cast<TopNSortWithExprOperator *>(CreateTestOperator(topNSortOperatorFactory));

    topNSortOperator->AddInput(vecBatch);
    VectorBatch *outputVecBatch;
    topNSortOperator->GetOutput(&outputVecBatch);

    constexpr int32_t expectedDataSize = 4;
    std::string expData1[expectedDataSize] = {"bye", "bye", "bye", "bye"};
    int64_t expData2[expectedDataSize] = {5, 3, 2, 11};
    int64_t expData3[expectedDataSize] = {3, 5, 7, 7};
    DataTypes expectTypes(std::vector<DataTypePtr>({ VarcharType(10), LongType(), LongType() }));
    VectorBatch *expectVectorBatch = CreateVectorBatch(expectTypes, expectedDataSize, expData1, expData2, expData3);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVectorBatch));

    Expr::DeleteExprs(partitionKeys);
    Expr::DeleteExprs(sortKeys);
    omniruntime::op::Operator::DeleteOperator(topNSortOperator);
    delete topNSortOperatorFactory;
    VectorHelper::FreeVecBatch(expectVectorBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
    delete overflowConfig;
}

TEST(TopNSortWithExprOperatorTest, TestTopNSortAscCase9)
{
    const int32_t dataSize = 13;
    int64_t data1[dataSize] = { 21, 22, 22, 22, 22, 22, 21, 23, 21, 21, 22, 21, 21 };
    std::string data2[dataSize] = {
        "xiaoaing", "xiaojing", "xiaohing", "xiaoling", "xiaoiing", "xiaoking", "xiaoding",
        "xiaoming", "xiaobing", "xiaocing", "xiaoging", "xiaoeing", "xiaofing"
    };
    std::string data3[dataSize] = {
        "henan",     "guangdong", "guangdong", "guangdong", "guangdong", "guangdong", "henan",
        "guangdong", "henan",     "henan",     "henan",     "henan",     "henan"
    };
    std::string data4[dataSize] = {
        "M", "F", "F", "M", "M", "M", "F", "F", "F", "F", "F", "M", "M"
    };

    DataTypes sourceTypes(std::vector<DataTypePtr>({ LongType(), VarcharType(10), VarcharType(10), VarcharType(10) }));
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3, data4);

    std::vector<omniruntime::expressions::Expr *> partitionKeys = { new FieldExpr(3, VarcharType(10)) };
    std::vector<omniruntime::expressions::Expr *> sortKeys = { new FieldExpr(2, LongType()) };
    std::vector<int32_t> sortAscendings = { 1 };
    std::vector<int32_t> sortNullFirsts = { 0 };
    auto overflowConfig = new OverflowConfig();
    auto topNSortOperatorFactory = new TopNSortWithExprOperatorFactory(sourceTypes, 4, false, partitionKeys, sortKeys,
        sortAscendings, sortNullFirsts, overflowConfig);
    auto topNSortOperator = static_cast<TopNSortWithExprOperator *>(CreateTestOperator(topNSortOperatorFactory));

    topNSortOperator->AddInput(vecBatch);
    VectorBatch *outputVecBatch;
    topNSortOperator->GetOutput(&outputVecBatch);

    constexpr int32_t expectedDataSize = 13;
    int64_t expData1[expectedDataSize] = { 22, 22, 23, 21, 21, 21, 22, 22, 22, 22, 21, 21, 21 };
    std::string expData2[expectedDataSize] = {
        "xiaojing", "xiaohing", "xiaoming", "xiaoding", "xiaobing", "xiaocing", "xiaoging",
        "xiaoling", "xiaoiing", "xiaoking", "xiaoaing", "xiaoeing", "xiaofing"
    };
    std::string expData3[expectedDataSize] = {
        "guangdong", "guangdong", "guangdong", "henan", "henan", "henan", "henan",
        "guangdong", "guangdong", "guangdong", "henan", "henan", "henan"
    };
    std::string expData4[dataSize] = {
        "F", "F", "F", "F", "F", "F", "F", "M", "M", "M", "M", "M", "M"
    };
    DataTypes expectTypes(std::vector<DataTypePtr>({ LongType(), VarcharType(10), VarcharType(10), VarcharType(10) }));
    VectorBatch *expectVectorBatch =
        CreateVectorBatch(expectTypes, expectedDataSize, expData1, expData2, expData3, expData4);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVectorBatch));

    Expr::DeleteExprs(partitionKeys);
    Expr::DeleteExprs(sortKeys);
    omniruntime::op::Operator::DeleteOperator(topNSortOperator);
    delete topNSortOperatorFactory;
    VectorHelper::FreeVecBatch(expectVectorBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
    delete overflowConfig;
}

TEST(TopNSortWithExprOperatorTest, TestTopNSortAscCase10)
{
    const int32_t dataSize = 13;
    int64_t data1[dataSize] = { 21, 22, 22, 22, 22, 22, 21, 23, 21, 21, 22, 21, 21 };
    std::string data2[dataSize] = {
        "xiaoaing", "xiaojing", "xiaohing", "xiaoling", "xiaoiing", "xiaoking", "xiaoding",
        "xiaoming", "xiaobing", "xiaocing", "xiaoging", "xiaoeing", "xiaofing"
    };
    std::string data3[dataSize] = {
        "henan",     "guangdong", "guangdong", "guangdong", "guangdong", "guangdong", "henan",
        "guangdong", "henan",     "henan",     "henan",     "henan",     "henan"
    };
    std::string data4[dataSize] = {
        "M", "F", "F", "M", "M", "M", "F", "F", "F", "F", "F", "M", "M"
    };

    DataTypes sourceTypes(std::vector<DataTypePtr>({ LongType(), VarcharType(10), VarcharType(10), VarcharType(10) }));
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3, data4);

    std::vector<omniruntime::expressions::Expr *> partitionKeys = { new FieldExpr(3, VarcharType(10)) };
    std::vector<omniruntime::expressions::Expr *> sortKeys = { new FieldExpr(2, LongType()) };
    std::vector<int32_t> sortAscendings = { 1 };
    std::vector<int32_t> sortNullFirsts = { 0 };
    auto overflowConfig = new OverflowConfig();
    auto topNSortOperatorFactory = new TopNSortWithExprOperatorFactory(sourceTypes, 1, false, partitionKeys, sortKeys,
        sortAscendings, sortNullFirsts, overflowConfig);
    auto topNSortOperator = static_cast<TopNSortWithExprOperator *>(CreateTestOperator(topNSortOperatorFactory));

    topNSortOperator->AddInput(vecBatch);
    VectorBatch *outputVecBatch;
    topNSortOperator->GetOutput(&outputVecBatch);

    constexpr int32_t expectedDataSize = 6;
    int64_t expData1[expectedDataSize] = { 22, 22, 23, 22, 22, 22 };
    std::string expData2[expectedDataSize] = {
        "xiaojing", "xiaohing", "xiaoming", "xiaoling", "xiaoiing", "xiaoking"
    };
    std::string expData3[expectedDataSize] = {
        "guangdong", "guangdong", "guangdong", "guangdong", "guangdong", "guangdong"
    };
    std::string expData4[expectedDataSize] = {
        "F", "F", "F", "M", "M", "M"
    };
    DataTypes expectTypes(std::vector<DataTypePtr>({ LongType(), VarcharType(10), VarcharType(10), VarcharType(10) }));
    VectorBatch *expectVectorBatch =
        CreateVectorBatch(expectTypes, expectedDataSize, expData1, expData2, expData3, expData4);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVectorBatch));

    Expr::DeleteExprs(partitionKeys);
    Expr::DeleteExprs(sortKeys);
    omniruntime::op::Operator::DeleteOperator(topNSortOperator);
    delete topNSortOperatorFactory;
    VectorHelper::FreeVecBatch(expectVectorBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
    delete overflowConfig;
}
TEST(TopNSortWithExprOperatorTest, TestTopNSortAscCase11)
{
    const int32_t dataSize = 23;
    std::string data1[dataSize] = {
        "henan", "guangdong", "shanxi", "hebei", "hebei",  "hebei", "hebei", "hebei",
        "hebei", "shanxi",    "hebei",  "hebei", "shanxi", "hebei", "hebei", "hebei",
        "hebei", "hebei",     "hebei",  "hebei", "hebei",  "hebei", "hebei"
    };
    std::string data2[dataSize] = {
        "M", "M", "M", "M", "F", "G", "H", "I", "I", "J", "I", "I",
        "I", "I", "I", "I", "I", "I", "I", "I", "I", "I", "I"
    };
    std::string data3[dataSize] = {
        "student0", "student0", "student0",  "student0", "student0", "student0", "student0", "student0",
        "student0", "student1", "student2",  "student3", "student4", "student5", "student6", "student7",
        "student8", "student9", "student10", "student2", "student2", "student2", "student2"
    };
    std::string data4[dataSize] = {
        "name1", "name1", "name1", "name1", "name1", "name1", "name1", "name1", "name1", "name1", "name1", "name1",
        "name1", "name1", "name1", "name1", "name1", "name1", "name1", "name1", "name1", "name1", "name1"
    };
    int64_t data5[dataSize] = {
        21, 22, 23, 23, 21, 22, 22, 23, 20, 21, 22, 23, 20, 21, 23, 23, 22, 23, 22, 22, 22, 23, 23
    };

    DataTypes sourceTypes(
        std::vector<DataTypePtr>({ VarcharType(10), VarcharType(15), VarcharType(10), VarcharType(10), LongType() }));
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3, data4, data5);

    std::vector<omniruntime::expressions::Expr *> partitionKeys = { new FieldExpr(0, VarcharType(10)),
        new FieldExpr(1, VarcharType(15)), new FieldExpr(2, VarcharType(10)) };
    std::vector<omniruntime::expressions::Expr *> sortKeys = { new FieldExpr(4, LongType()) };
    std::vector<int32_t> sortAscendings = { 1 };
    std::vector<int32_t> sortNullFirsts = { 0 };
    auto overflowConfig = new OverflowConfig();
    auto topNSortOperatorFactory = new TopNSortWithExprOperatorFactory(sourceTypes, 2, false, partitionKeys, sortKeys,
        sortAscendings, sortNullFirsts, overflowConfig);
    auto topNSortOperator = static_cast<TopNSortWithExprOperator *>(CreateTestOperator(topNSortOperatorFactory));

    topNSortOperator->AddInput(vecBatch);
    VectorBatch *outputVecBatch;
    topNSortOperator->GetOutput(&outputVecBatch);

    constexpr int32_t expectedDataSize = 21;
    std::string expData1[expectedDataSize] = {
        "hebei",  "hebei", "hebei", "hebei", "hebei", "hebei", "hebei", "hebei",  "guangdong", "henan", "shanxi",
        "shanxi", "hebei", "hebei", "hebei", "hebei", "hebei", "hebei", "shanxi", "hebei",     "hebei"
    };
    std::string expData2[expectedDataSize] = {
        "G", "F", "I", "I", "I", "I", "M", "I", "M", "M", "J", "M", "H", "I", "I", "I", "I", "I", "I", "I", "I"
    };
    std::string expData3[expectedDataSize] = {
        "student0", "student0", "student0", "student0", "student10", "student9", "student0",
        "student5", "student0", "student0", "student1", "student0",  "student0", "student2",
        "student2", "student2", "student6", "student3", "student4",  "student7", "student8"
    };
    std::string expData4[expectedDataSize] = {
        "name1", "name1", "name1", "name1", "name1", "name1", "name1", "name1", "name1", "name1", "name1",
        "name1", "name1", "name1", "name1", "name1", "name1", "name1", "name1", "name1", "name1"
    };
    int64_t expData5[expectedDataSize] = {
        22, 21, 20, 23, 22, 23, 23, 21, 22, 21, 21, 23, 22, 22, 22, 22, 23, 23, 20, 23, 22
    };
    DataTypes expectTypes(
        std::vector<DataTypePtr>({ VarcharType(10), VarcharType(15), VarcharType(10), VarcharType(10), LongType() }));
    VectorBatch *expectVectorBatch =
        CreateVectorBatch(expectTypes, expectedDataSize, expData1, expData2, expData3, expData4, expData5);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVectorBatch));

    Expr::DeleteExprs(partitionKeys);
    Expr::DeleteExprs(sortKeys);
    omniruntime::op::Operator::DeleteOperator(topNSortOperator);
    delete topNSortOperatorFactory;
    VectorHelper::FreeVecBatch(expectVectorBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
    delete overflowConfig;
}
}  // namespace TopnWithExprTest
