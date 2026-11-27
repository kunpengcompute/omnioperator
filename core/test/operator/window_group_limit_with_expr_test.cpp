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
#include "operator/window/window_group_limit_expr.h"
#include "vector/vector_helper.h"
#include "util/test_util.h"

using namespace omniruntime::vec;
using namespace omniruntime::op;
using namespace omniruntime::expressions;
using namespace std;
using namespace TestUtil;

namespace WindowGroupLimitWithExprTest {
// rank + Desc + NullLast
TEST(WindowGroupLimitWithExprOperatorTest, TestRankDescNullLast)
{
    const int32_t dataSize = 8;
    std::string data1[dataSize] = {"hi", "hi", "hi", "bye", "bye", "bye", "bye", "bye"};
    int64_t data2[dataSize] = {2L, 5L, 3L, 11L, 4L, 3L, 0L, 23L};
    int64_t data3[dataSize] = {3L, 5L, 8L, 3L, 5L, 3L, 4L, 3L};
    DataTypes sourceTypes(std::vector<DataTypePtr>({VarcharType(10), LongType(), LongType()}));
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3);

    std::vector<omniruntime::expressions::Expr *> partitionKeys = {new FieldExpr(0, VarcharType(10))};
    std::vector<omniruntime::expressions::Expr *> sortKeys = {new FieldExpr(2, LongType())};
    std::vector<int32_t> sortAscendings = {0};
    std::vector<int32_t> sortNullFirsts = {0};
    auto overflowConfig = new OverflowConfig();
    auto windowGroupLimitOperatorFactory = new WindowGroupLimitWithExprOperatorFactory(
        sourceTypes, 3, "rank", partitionKeys, sortKeys, sortAscendings, sortNullFirsts, overflowConfig);
    auto windowGroupLimitOperator =
        static_cast<WindowGroupLimitWithExprOperator *>(CreateTestOperator(windowGroupLimitOperatorFactory));

    windowGroupLimitOperator->AddInput(vecBatch);
    VectorBatch *outputVecBatch;
    windowGroupLimitOperator->GetOutput(&outputVecBatch);

    constexpr int32_t expectedDataSize = 8;
    std::string expData1[expectedDataSize] = {"bye", "bye", "bye", "bye", "bye", "hi", "hi", "hi"};
    int64_t expData2[expectedDataSize] = {4, 0, 11, 3, 23, 3, 5, 2};
    int64_t expData3[expectedDataSize] = {5, 4, 3, 3, 3, 8, 5, 3};
    DataTypes expectTypes(std::vector<DataTypePtr>({VarcharType(10), LongType(), LongType()}));
    VectorBatch *expectVectorBatch = CreateVectorBatch(expectTypes, expectedDataSize, expData1, expData2, expData3);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVectorBatch));

    Expr::DeleteExprs(partitionKeys);
    Expr::DeleteExprs(sortKeys);
    omniruntime::op::Operator::DeleteOperator(windowGroupLimitOperator);
    delete windowGroupLimitOperatorFactory;
    VectorHelper::FreeVecBatch(expectVectorBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
    delete overflowConfig;
}

// rank + Asc + NullLast
TEST(WindowGroupLimitWithExprOperatorTest, TestRankAscNullLast)
{
    // construct input data
    const int32_t dataSize = 8;
    std::string data1[dataSize] = {"hi", "hi", "hi", "bye", "bye", "bye", "bye", "bye"};
    int64_t data2[dataSize] = {2L, 5L, 3L, 11L, 4L, 3L, 0L, 23L};
    int64_t data3[dataSize] = {5L, 3L, 8L, 3L, 6L, 6L, 4L, 6L};
    DataTypes sourceTypes(std::vector<DataTypePtr>({VarcharType(10), LongType(), LongType()}));
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3);

    std::vector<omniruntime::expressions::Expr *> partitionKeys = {new FieldExpr(0, VarcharType(10))};
    std::vector<omniruntime::expressions::Expr *> sortKeys = {new FieldExpr(2, LongType())};
    std::vector<int32_t> sortAscendings = {1};
    std::vector<int32_t> sortNullFirsts = {0};
    auto overflowConfig = new OverflowConfig();
    auto windowGroupLimitOperatorFactory = new WindowGroupLimitWithExprOperatorFactory(
        sourceTypes, 3, "rank", partitionKeys, sortKeys, sortAscendings, sortNullFirsts, overflowConfig);
    auto windowGroupLimitOperator =
        static_cast<WindowGroupLimitWithExprOperator *>(CreateTestOperator(windowGroupLimitOperatorFactory));

    windowGroupLimitOperator->AddInput(vecBatch);
    VectorBatch *outputVecBatch;
    windowGroupLimitOperator->GetOutput(&outputVecBatch);

    constexpr int32_t expectedDataSize = 8;
    std::string expData1[expectedDataSize] = {"bye", "bye", "bye", "bye", "bye", "hi", "hi", "hi"};
    int64_t expData2[expectedDataSize] = {11, 0, 4, 3, 23, 5, 2, 3};
    int64_t expData3[expectedDataSize] = {3, 4, 6, 6, 6, 3, 5, 8};
    DataTypes expectTypes(std::vector<DataTypePtr>({VarcharType(10), LongType(), LongType()}));
    VectorBatch *expectVectorBatch = CreateVectorBatch(expectTypes, expectedDataSize, expData1, expData2, expData3);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVectorBatch));

    Expr::DeleteExprs(partitionKeys);
    Expr::DeleteExprs(sortKeys);
    omniruntime::op::Operator::DeleteOperator(windowGroupLimitOperator);
    delete windowGroupLimitOperatorFactory;
    VectorHelper::FreeVecBatch(expectVectorBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
    delete overflowConfig;
}

// rank + AscCase1
TEST(WindowGroupLimitWithExprOperatorTest, TestRankAscCase1)
{
    // construct input data
    const int32_t dataSize = 6;
    std::string data1[dataSize] = {"hello", "hello", "hello", "hello", "test", "test"};
    int64_t data2[dataSize] = {1L, 2L, 3L, 4L, 5L, 6L};
    int64_t data3[dataSize] = {4L, 3L, 2L, 1L, 4L, 3L};
    DataTypes sourceTypes(std::vector<DataTypePtr>({VarcharType(10), LongType(), LongType()}));
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3);

    std::vector<omniruntime::expressions::Expr *> partitionKeys = {new FieldExpr(0, VarcharType(10))};
    std::vector<omniruntime::expressions::Expr *> sortKeys = {new FieldExpr(2, LongType())};
    std::vector<int32_t> sortAscendings = {1};
    std::vector<int32_t> sortNullFirsts = {0};
    auto overflowConfig = new OverflowConfig();
    auto windowGroupLimitOperatorFactory = new WindowGroupLimitWithExprOperatorFactory(
        sourceTypes, 3, "rank", partitionKeys, sortKeys, sortAscendings, sortNullFirsts, overflowConfig);
    auto windowGroupLimitOperator =
        static_cast<WindowGroupLimitWithExprOperator *>(CreateTestOperator(windowGroupLimitOperatorFactory));

    windowGroupLimitOperator->AddInput(vecBatch);
    VectorBatch *outputVecBatch;
    windowGroupLimitOperator->GetOutput(&outputVecBatch);

    constexpr int32_t expectedDataSize = 5;
    std::string expData1[expectedDataSize] = {"hello", "hello", "hello", "test", "test"};
    int64_t expData2[expectedDataSize] = {4, 3, 2, 6, 5};
    int64_t expData3[expectedDataSize] = {1, 2, 3, 3, 4};
    DataTypes expectTypes(std::vector<DataTypePtr>({VarcharType(10), LongType(), LongType()}));
    VectorBatch *expectVectorBatch = CreateVectorBatch(expectTypes, expectedDataSize, expData1, expData2, expData3);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVectorBatch));

    Expr::DeleteExprs(partitionKeys);
    Expr::DeleteExprs(sortKeys);
    omniruntime::op::Operator::DeleteOperator(windowGroupLimitOperator);
    delete windowGroupLimitOperatorFactory;
    VectorHelper::FreeVecBatch(expectVectorBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
    delete overflowConfig;
}

// rank + AscCase2
TEST(WindowGroupLimitWithExprOperatorTest, TestRankAscCase2)
{
    // construct input data
    const int32_t dataSize = 4;
    std::string data1[dataSize] = {"bye", "bye", "bye", "bye"};
    int64_t data2[dataSize] = {5L, 2L, 3L, 11L};
    int64_t data3[dataSize] = {6L, 6L, 4L, 3L};
    DataTypes sourceTypes(std::vector<DataTypePtr>({VarcharType(10), LongType(), LongType()}));
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3);

    std::vector<omniruntime::expressions::Expr *> partitionKeys = {new FieldExpr(0, VarcharType(10))};
    std::vector<omniruntime::expressions::Expr *> sortKeys = {new FieldExpr(2, LongType())};
    std::vector<int32_t> sortAscendings = {1};
    std::vector<int32_t> sortNullFirsts = {0};
    auto overflowConfig = new OverflowConfig();
    auto windowGroupLimitOperatorFactory = new WindowGroupLimitWithExprOperatorFactory(
        sourceTypes, 3, "rank", partitionKeys, sortKeys, sortAscendings, sortNullFirsts, overflowConfig);
    auto windowGroupLimitOperator =
        static_cast<WindowGroupLimitWithExprOperator *>(CreateTestOperator(windowGroupLimitOperatorFactory));

    windowGroupLimitOperator->AddInput(vecBatch);
    VectorBatch *outputVecBatch;
    windowGroupLimitOperator->GetOutput(&outputVecBatch);

    constexpr int32_t expectedDataSize = 4;
    std::string expData1[expectedDataSize] = {"bye", "bye", "bye", "bye"};
    int64_t expData2[expectedDataSize] = {11, 3, 5, 2};
    int64_t expData3[expectedDataSize] = {3, 4, 6, 6};
    DataTypes expectTypes(std::vector<DataTypePtr>({VarcharType(10), LongType(), LongType()}));
    VectorBatch *expectVectorBatch = CreateVectorBatch(expectTypes, expectedDataSize, expData1, expData2, expData3);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVectorBatch));

    Expr::DeleteExprs(partitionKeys);
    Expr::DeleteExprs(sortKeys);
    omniruntime::op::Operator::DeleteOperator(windowGroupLimitOperator);
    delete windowGroupLimitOperatorFactory;
    VectorHelper::FreeVecBatch(expectVectorBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
    delete overflowConfig;
}

// rank + AscCase3
TEST(WindowGroupLimitWithExprOperatorTest, TestRankAscCase3)
{
    const int32_t dataSize = 13;
    int64_t data1[dataSize] = {21, 22, 22, 22, 22, 22, 21, 23, 21, 21, 22, 21, 21};
    std::string data2[dataSize] = {"xiaoaing", "xiaojing", "xiaohing", "xiaoling", "xiaoiing", "xiaoking", "xiaoding",
                                   "xiaoming", "xiaobing", "xiaocing", "xiaoging", "xiaoeing", "xiaofing"};
    std::string data3[dataSize] = {"henan",     "guangdong", "guangdong", "guangdong", "guangdong",
                                   "guangdong", "henan",     "guangdong", "henan",     "henan",
                                   "henan",     "henan",     "henan"};
    std::string data4[dataSize] = {"M", "F", "F", "M", "M", "M", "F", "F", "F", "F", "F", "M", "M"};

    DataTypes sourceTypes(std::vector<DataTypePtr>({LongType(), VarcharType(10), VarcharType(10), VarcharType(10)}));
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3, data4);

    std::vector<omniruntime::expressions::Expr *> partitionKeys = {new FieldExpr(3, VarcharType(10))};
    std::vector<omniruntime::expressions::Expr *> sortKeys = {new FieldExpr(2, LongType())};
    std::vector<int32_t> sortAscendings = {1};
    std::vector<int32_t> sortNullFirsts = {0};
    auto overflowConfig = new OverflowConfig();
    auto windowGroupLimitOperatorFactory = new WindowGroupLimitWithExprOperatorFactory(
        sourceTypes, 4, "rank", partitionKeys, sortKeys, sortAscendings, sortNullFirsts, overflowConfig);
    auto windowGroupLimitOperator =
        static_cast<WindowGroupLimitWithExprOperator *>(CreateTestOperator(windowGroupLimitOperatorFactory));

    windowGroupLimitOperator->AddInput(vecBatch);
    VectorBatch *outputVecBatch;
    windowGroupLimitOperator->GetOutput(&outputVecBatch);

    constexpr int32_t expectedDataSize = 13;
    int64_t expData1[expectedDataSize] = {22, 22, 23, 21, 21, 21, 22, 22, 22, 22, 21, 21, 21};
    std::string expData2[expectedDataSize] = {"xiaojing", "xiaohing", "xiaoming", "xiaoding", "xiaobing",
                                              "xiaocing", "xiaoging", "xiaoling", "xiaoiing", "xiaoking",
                                              "xiaoaing", "xiaoeing", "xiaofing"};
    std::string expData3[expectedDataSize] = {"guangdong", "guangdong", "guangdong", "henan", "henan", "henan", "henan",
                                              "guangdong", "guangdong", "guangdong", "henan", "henan", "henan"};
    std::string expData4[dataSize] = {"F", "F", "F", "F", "F", "F", "F", "M", "M", "M", "M", "M", "M"};
    DataTypes expectTypes(std::vector<DataTypePtr>({LongType(), VarcharType(10), VarcharType(10), VarcharType(10)}));
    VectorBatch *expectVectorBatch =
        CreateVectorBatch(expectTypes, expectedDataSize, expData1, expData2, expData3, expData4);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVectorBatch));

    Expr::DeleteExprs(partitionKeys);
    Expr::DeleteExprs(sortKeys);
    omniruntime::op::Operator::DeleteOperator(windowGroupLimitOperator);
    delete windowGroupLimitOperatorFactory;
    VectorHelper::FreeVecBatch(expectVectorBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
    delete overflowConfig;
}

// rank + AscCase4
TEST(WindowGroupLimitWithExprOperatorTest, TestRankAscCase4)
{
    const int32_t dataSize = 23;
    std::string data1[dataSize] = {"henan", "guangdong", "shanxi", "hebei", "hebei",  "hebei", "hebei", "hebei",
                                   "hebei", "shanxi",    "hebei",  "hebei", "shanxi", "hebei", "hebei", "hebei",
                                   "hebei", "hebei",     "hebei",  "hebei", "hebei",  "hebei", "hebei"};
    std::string data2[dataSize] = {"M", "M", "M", "M", "F", "G", "H", "I", "I", "J", "I", "I",
                                   "I", "I", "I", "I", "I", "I", "I", "I", "I", "I", "I"};
    std::string data3[dataSize] = {"student0",  "student0", "student0", "student0", "student0", "student0",
                                   "student0",  "student0", "student0", "student1", "student2", "student3",
                                   "student4",  "student5", "student6", "student7", "student8", "student9",
                                   "student10", "student2", "student2", "student2", "student2"};
    std::string data4[dataSize] = {"name1", "name1", "name1", "name1", "name1", "name1", "name1", "name1",
                                   "name1", "name1", "name1", "name1", "name1", "name1", "name1", "name1",
                                   "name1", "name1", "name1", "name1", "name1", "name1", "name1"};
    int64_t data5[dataSize] = {21, 22, 23, 23, 21, 22, 22, 23, 20, 21, 22, 23,
                               20, 21, 23, 23, 22, 23, 22, 22, 22, 23, 23};

    DataTypes sourceTypes(
        std::vector<DataTypePtr>({VarcharType(10), VarcharType(15), VarcharType(10), VarcharType(10), LongType()}));
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3, data4, data5);

    std::vector<omniruntime::expressions::Expr *> partitionKeys = {
        new FieldExpr(0, VarcharType(10)), new FieldExpr(1, VarcharType(15)), new FieldExpr(2, VarcharType(10))};
    std::vector<omniruntime::expressions::Expr *> sortKeys = {new FieldExpr(4, LongType())};
    std::vector<int32_t> sortAscendings = {1};
    std::vector<int32_t> sortNullFirsts = {0};
    auto overflowConfig = new OverflowConfig();
    auto windowGroupLimitOperatorFactory = new WindowGroupLimitWithExprOperatorFactory(
        sourceTypes, 2, "rank", partitionKeys, sortKeys, sortAscendings, sortNullFirsts, overflowConfig);
    auto windowGroupLimitOperator =
        static_cast<WindowGroupLimitWithExprOperator *>(CreateTestOperator(windowGroupLimitOperatorFactory));

    windowGroupLimitOperator->AddInput(vecBatch);
    VectorBatch *outputVecBatch;
    windowGroupLimitOperator->GetOutput(&outputVecBatch);

    constexpr int32_t expectedDataSize = 21;
    std::string expData1[expectedDataSize] = {"hebei", "hebei",     "hebei", "hebei",  "hebei",  "hebei", "hebei",
                                              "hebei", "guangdong", "henan", "shanxi", "shanxi", "hebei", "hebei",
                                              "hebei", "hebei",     "hebei", "hebei",  "shanxi", "hebei", "hebei"};
    std::string expData2[expectedDataSize] = {"G", "F", "I", "I", "I", "I", "M", "I", "M", "M", "J",
                                              "M", "H", "I", "I", "I", "I", "I", "I", "I", "I"};
    std::string expData3[expectedDataSize] = {"student0", "student0", "student0", "student0", "student10", "student9",
                                              "student0", "student5", "student0", "student0", "student1",  "student0",
                                              "student0", "student2", "student2", "student2", "student6",  "student3",
                                              "student4", "student7", "student8"};
    std::string expData4[expectedDataSize] = {"name1", "name1", "name1", "name1", "name1", "name1", "name1",
                                              "name1", "name1", "name1", "name1", "name1", "name1", "name1",
                                              "name1", "name1", "name1", "name1", "name1", "name1", "name1"};
    int64_t expData5[expectedDataSize] = {22, 21, 20, 23, 22, 23, 23, 21, 22, 21, 21,
                                          23, 22, 22, 22, 22, 23, 23, 20, 23, 22};
    DataTypes expectTypes(
        std::vector<DataTypePtr>({VarcharType(10), VarcharType(15), VarcharType(10), VarcharType(10), LongType()}));
    VectorBatch *expectVectorBatch =
        CreateVectorBatch(expectTypes, expectedDataSize, expData1, expData2, expData3, expData4, expData5);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVectorBatch));

    Expr::DeleteExprs(partitionKeys);
    Expr::DeleteExprs(sortKeys);
    omniruntime::op::Operator::DeleteOperator(windowGroupLimitOperator);
    delete windowGroupLimitOperatorFactory;
    VectorHelper::FreeVecBatch(expectVectorBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
    delete overflowConfig;
}

// row_number + Desc + NullLast
TEST(WindowGroupLimitWithExprOperatorTest, TestRowNumberDescNullLast)
{
    const int32_t dataSize = 8;
    std::string data1[dataSize] = {"hi", "hi", "hi", "bye", "bye", "bye", "bye", "bye"};
    int64_t data2[dataSize] = {2L, 5L, 3L, 11L, 4L, 3L, 0L, 23L};
    int64_t data3[dataSize] = {3L, 5L, 8L, 3L, 5L, 3L, 4L, 3L};
    DataTypes sourceTypes(std::vector<DataTypePtr>({VarcharType(10), LongType(), LongType()}));
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3);

    std::vector<omniruntime::expressions::Expr *> partitionKeys = {new FieldExpr(0, VarcharType(10))};
    std::vector<omniruntime::expressions::Expr *> sortKeys = {new FieldExpr(2, LongType())};
    std::vector<int32_t> sortAscendings = {0};
    std::vector<int32_t> sortNullFirsts = {0};
    auto overflowConfig = new OverflowConfig();
    auto windowGroupLimitOperatorFactory = new WindowGroupLimitWithExprOperatorFactory(
        sourceTypes, 3, "row_number", partitionKeys, sortKeys, sortAscendings, sortNullFirsts, overflowConfig);
    auto windowGroupLimitOperator =
        static_cast<WindowGroupLimitWithExprOperator *>(CreateTestOperator(windowGroupLimitOperatorFactory));

    windowGroupLimitOperator->AddInput(vecBatch);
    VectorBatch *outputVecBatch;
    windowGroupLimitOperator->GetOutput(&outputVecBatch);

    constexpr int32_t expectedDataSize = 6;
    std::string expData1[expectedDataSize] = {"bye", "bye", "bye", "hi", "hi", "hi"};
    int64_t expData2[expectedDataSize] = {4, 0, 11, 3, 5, 2};
    int64_t expData3[expectedDataSize] = {5, 4, 3, 8, 5, 3};
    DataTypes expectTypes(std::vector<DataTypePtr>({VarcharType(10), LongType(), LongType()}));
    VectorBatch *expectVectorBatch = CreateVectorBatch(expectTypes, expectedDataSize, expData1, expData2, expData3);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVectorBatch));

    Expr::DeleteExprs(partitionKeys);
    Expr::DeleteExprs(sortKeys);
    omniruntime::op::Operator::DeleteOperator(windowGroupLimitOperator);
    delete windowGroupLimitOperatorFactory;
    VectorHelper::FreeVecBatch(expectVectorBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
    delete overflowConfig;
}

// row_number + Asc + NullLast
TEST(WindowGroupLimitWithExprOperatorTest, TestRowNumberAscNullLast)
{
    const int32_t dataSize = 8;
    std::string data1[dataSize] = {"hi", "hi", "hi", "bye", "bye", "bye", "bye", "bye"};
    int64_t data2[dataSize] = {2L, 5L, 3L, 11L, 4L, 3L, 0L, 23L};
    int64_t data3[dataSize] = {5L, 3L, 8L, 3L, 6L, 6L, 4L, 6L};
    DataTypes sourceTypes(std::vector<DataTypePtr>({VarcharType(10), LongType(), LongType()}));
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3);

    std::vector<omniruntime::expressions::Expr *> partitionKeys = {new FieldExpr(0, VarcharType(10))};
    std::vector<omniruntime::expressions::Expr *> sortKeys = {new FieldExpr(2, LongType())};
    std::vector<int32_t> sortAscendings = {1};
    std::vector<int32_t> sortNullFirsts = {0};
    auto overflowConfig = new OverflowConfig();
    auto windowGroupLimitOperatorFactory = new WindowGroupLimitWithExprOperatorFactory(
        sourceTypes, 3, "row_number", partitionKeys, sortKeys, sortAscendings, sortNullFirsts, overflowConfig);
    auto windowGroupLimitOperator =
        static_cast<WindowGroupLimitWithExprOperator *>(CreateTestOperator(windowGroupLimitOperatorFactory));

    windowGroupLimitOperator->AddInput(vecBatch);
    VectorBatch *outputVecBatch;
    windowGroupLimitOperator->GetOutput(&outputVecBatch);

    constexpr int32_t expectedDataSize = 6;
    std::string expData1[expectedDataSize] = {"bye", "bye", "bye", "hi", "hi", "hi"};
    int64_t expData2[expectedDataSize] = {11, 0, 4, 5, 2, 3};
    int64_t expData3[expectedDataSize] = {3, 4, 6, 3, 5, 8};
    DataTypes expectTypes(std::vector<DataTypePtr>({VarcharType(10), LongType(), LongType()}));
    VectorBatch *expectVectorBatch = CreateVectorBatch(expectTypes, expectedDataSize, expData1, expData2, expData3);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVectorBatch));

    Expr::DeleteExprs(partitionKeys);
    Expr::DeleteExprs(sortKeys);
    omniruntime::op::Operator::DeleteOperator(windowGroupLimitOperator);
    delete windowGroupLimitOperatorFactory;
    VectorHelper::FreeVecBatch(expectVectorBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
    delete overflowConfig;
}

// row_number + AscCase1
TEST(WindowGroupLimitWithExprOperatorTest, TestRowNumberAscCase1)
{
    // construct input data
    const int32_t dataSize = 6;
    std::string data1[dataSize] = {"hello", "hello", "hello", "hello", "test", "test"};
    int64_t data2[dataSize] = {1L, 2L, 3L, 4L, 5L, 6L};
    int64_t data3[dataSize] = {4L, 3L, 2L, 1L, 4L, 3L};
    DataTypes sourceTypes(std::vector<DataTypePtr>({VarcharType(10), LongType(), LongType()}));
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3);

    std::vector<omniruntime::expressions::Expr *> partitionKeys = {new FieldExpr(0, VarcharType(10))};
    std::vector<omniruntime::expressions::Expr *> sortKeys = {new FieldExpr(2, LongType())};
    std::vector<int32_t> sortAscendings = {1};
    std::vector<int32_t> sortNullFirsts = {0};
    auto overflowConfig = new OverflowConfig();
    auto windowGroupLimitOperatorFactory = new WindowGroupLimitWithExprOperatorFactory(
        sourceTypes, 3, "row_number", partitionKeys, sortKeys, sortAscendings, sortNullFirsts, overflowConfig);
    auto windowGroupLimitOperator =
        static_cast<WindowGroupLimitWithExprOperator *>(CreateTestOperator(windowGroupLimitOperatorFactory));

    windowGroupLimitOperator->AddInput(vecBatch);
    VectorBatch *outputVecBatch;
    windowGroupLimitOperator->GetOutput(&outputVecBatch);

    constexpr int32_t expectedDataSize = 5;
    std::string expData1[expectedDataSize] = {"hello", "hello", "hello", "test", "test"};
    int64_t expData2[expectedDataSize] = {4, 3, 2, 6, 5};
    int64_t expData3[expectedDataSize] = {1, 2, 3, 3, 4};
    DataTypes expectTypes(std::vector<DataTypePtr>({VarcharType(10), LongType(), LongType()}));
    VectorBatch *expectVectorBatch = CreateVectorBatch(expectTypes, expectedDataSize, expData1, expData2, expData3);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVectorBatch));

    Expr::DeleteExprs(partitionKeys);
    Expr::DeleteExprs(sortKeys);
    omniruntime::op::Operator::DeleteOperator(windowGroupLimitOperator);
    delete windowGroupLimitOperatorFactory;
    VectorHelper::FreeVecBatch(expectVectorBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
    delete overflowConfig;
}

// row_number + AscCase2
TEST(WindowGroupLimitWithExprOperatorTest, TestRowNumberAscCase2)
{
    // construct input data
    const int32_t dataSize = 4;
    std::string data1[dataSize] = {"bye", "bye", "bye", "bye"};
    int64_t data2[dataSize] = {5L, 2L, 3L, 11L};
    int64_t data3[dataSize] = {6L, 6L, 4L, 3L};
    DataTypes sourceTypes(std::vector<DataTypePtr>({VarcharType(10), LongType(), LongType()}));
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3);

    std::vector<omniruntime::expressions::Expr *> partitionKeys = {new FieldExpr(0, VarcharType(10))};
    std::vector<omniruntime::expressions::Expr *> sortKeys = {new FieldExpr(2, LongType())};
    std::vector<int32_t> sortAscendings = {1};
    std::vector<int32_t> sortNullFirsts = {0};
    auto overflowConfig = new OverflowConfig();
    auto windowGroupLimitOperatorFactory = new WindowGroupLimitWithExprOperatorFactory(
        sourceTypes, 3, "row_number", partitionKeys, sortKeys, sortAscendings, sortNullFirsts, overflowConfig);
    auto windowGroupLimitOperator =
        static_cast<WindowGroupLimitWithExprOperator *>(CreateTestOperator(windowGroupLimitOperatorFactory));

    windowGroupLimitOperator->AddInput(vecBatch);
    VectorBatch *outputVecBatch;
    windowGroupLimitOperator->GetOutput(&outputVecBatch);

    constexpr int32_t expectedDataSize = 3;
    std::string expData1[expectedDataSize] = {"bye", "bye", "bye"};
    int64_t expData2[expectedDataSize] = {11, 3, 5};
    int64_t expData3[expectedDataSize] = {3, 4, 6};
    DataTypes expectTypes(std::vector<DataTypePtr>({VarcharType(10), LongType(), LongType()}));
    VectorBatch *expectVectorBatch = CreateVectorBatch(expectTypes, expectedDataSize, expData1, expData2, expData3);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVectorBatch));

    Expr::DeleteExprs(partitionKeys);
    Expr::DeleteExprs(sortKeys);
    omniruntime::op::Operator::DeleteOperator(windowGroupLimitOperator);
    delete windowGroupLimitOperatorFactory;
    VectorHelper::FreeVecBatch(expectVectorBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
    delete overflowConfig;
}

// row_number + AscCase3
TEST(WindowGroupLimitWithExprOperatorTest, TestRowNumberAscCase3)
{
    const int32_t dataSize = 10;
    std::string data1[dataSize] = {"test1", "test1", "test1", "test1", "test1",
                                   "test1", "test2", "test2", "test2", "test2"};
    std::string data2[dataSize] = {"M", "M", "M", "M", "F", "F", "M", "M", "M", "M"};
    std::string data3[dataSize] = {"FuJian", "FuJian", "FuJian", "ShanDong", "FuJian",
                                   "FuJian", "FuJian", "FuJian", "FuJian",   "FuJian"};
    int64_t data4[dataSize] = {176, 176, 175, 185, 165, 163, 173, 173, 173, 173};
    int64_t data5[dataSize] = {70, 65, 72, 80, 50, 50, 63, 62, 61, 60};

    DataTypes sourceTypes(
        std::vector<DataTypePtr>({VarcharType(10), VarcharType(15), VarcharType(10), LongType(), LongType()}));
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3, data4, data5);

    std::vector<omniruntime::expressions::Expr *> partitionKeys = {
        new FieldExpr(0, VarcharType(10)), new FieldExpr(1, VarcharType(15)), new FieldExpr(2, VarcharType(10))};
    std::vector<omniruntime::expressions::Expr *> sortKeys = {new FieldExpr(3, LongType())};
    std::vector<int32_t> sortAscendings = {1};
    std::vector<int32_t> sortNullFirsts = {0};
    auto overflowConfig = new OverflowConfig();
    auto windowGroupLimitOperatorFactory = new WindowGroupLimitWithExprOperatorFactory(
        sourceTypes, 2, "row_number", partitionKeys, sortKeys, sortAscendings, sortNullFirsts, overflowConfig);
    auto windowGroupLimitOperator =
        static_cast<WindowGroupLimitWithExprOperator *>(CreateTestOperator(windowGroupLimitOperatorFactory));

    windowGroupLimitOperator->AddInput(vecBatch);
    VectorBatch *outputVecBatch;
    windowGroupLimitOperator->GetOutput(&outputVecBatch);

    constexpr int32_t expectedDataSize = 7;
    std::string expData1[expectedDataSize] = {"test1", "test1", "test1", "test1", "test1", "test2", "test2"};
    std::string expData2[expectedDataSize] = {"M", "M", "M", "F", "F", "M", "M"};
    std::string expData3[expectedDataSize] = {"FuJian", "FuJian", "ShanDong", "FuJian", "FuJian", "FuJian", "FuJian"};
    int64_t expData4[dataSize] = {175, 176, 185, 163, 165, 173, 173};
    int64_t expData5[expectedDataSize] = {72, 70, 80, 50, 50, 63, 62};
    DataTypes expectTypes(
        std::vector<DataTypePtr>({VarcharType(10), VarcharType(15), VarcharType(10), LongType(), LongType()}));
    VectorBatch *expectVectorBatch =
        CreateVectorBatch(expectTypes, expectedDataSize, expData1, expData2, expData3, expData4, expData5);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVectorBatch));

    Expr::DeleteExprs(partitionKeys);
    Expr::DeleteExprs(sortKeys);
    omniruntime::op::Operator::DeleteOperator(windowGroupLimitOperator);
    delete windowGroupLimitOperatorFactory;
    VectorHelper::FreeVecBatch(expectVectorBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
    delete overflowConfig;
}
} // namespace WindowGroupLimitWithExprTest
