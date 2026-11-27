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
#include "operator/join/sortmergejoin/sort_merge_join_expr_v3.h"
#include "util/test_util.h"

using namespace omniruntime::op;
using namespace omniruntime::expressions;
using namespace std;
using namespace TestUtil;

namespace SortMergeJoinWithExprV3Test {
TEST(SortMergeJoinWithExprV3Test, testSmjInnerJoinExprGreaterThanCondition)
{
    // select t1.b, t2.c from t1 full join t2 where t1.a = t2.d and t1.a > 2;
    // streamedTbl t1:  int a, long b;
    // bufferedTbl t2: double c, int d;
    string filterJsonStr = "{\"exprType\":\"BINARY\","
                           "\"returnType\":4,"
                           "\"operator\":\"GREATER_THAN\","
                           "\"left\":{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":1,\"colVal\":0},"
                           "\"right\":{\"exprType\":\"LITERAL\",\"dataType\":1, \"isNull\":false, \"value\":2}}";
    std::vector<DataTypePtr> streamTypeVector = {IntType(), LongType()};
    DataTypes streamedTblTypes(streamTypeVector);
    auto *col0 = new FieldExpr(0, IntType());
    std::vector<Expr *> streamedEqualKeyExprs = {col0};
    std::vector<int32_t> streamedOutputCols = {1};
    OperatorConfig operatorConfig;
    auto streamedWithExprOperatorFactory =
        StreamedTableWithExprOperatorFactoryV3::CreateStreamedTableWithExprOperatorFactory(
            streamedTblTypes, streamedEqualKeyExprs, streamedOutputCols, JoinType::OMNI_JOIN_TYPE_INNER, filterJsonStr,
            operatorConfig);
    omniruntime::op::Operator *streamedTblWithExprOperator = CreateTestOperator(streamedWithExprOperatorFactory);

    std::vector<DataTypePtr> bufferTypesVector = {DoubleType(), IntType()};
    DataTypes bufferedTblTypes(bufferTypesVector);
    auto *col1 = new FieldExpr(1, IntType());
    std::vector<Expr *> bufferedEqualKeyExprs = {col1};
    std::vector<int32_t> bufferedOutputCols = {0};
    auto bufferedWithExprOperatorFactory =
        BufferedTableWithExprOperatorFactoryV3::CreateBufferedTableWithExprOperatorFactory(
            bufferedTblTypes, bufferedEqualKeyExprs, bufferedOutputCols, streamedWithExprOperatorFactory,
            operatorConfig);
    omniruntime::op::Operator *bufferedTblWithExprOperator = CreateTestOperator(bufferedWithExprOperatorFactory);

    // construct data
    const int32_t streamedTblDataSize = 6;
    int32_t streamedTblDataCol1[streamedTblDataSize] = {0, 1, 2, 3, 4, 5};
    long streamedTblDataCol2[streamedTblDataSize] = {6600, 5500, 4400, 3300, 2200, 1100};
    VectorBatch *streamedTblVecBatch1 =
        CreateVectorBatch(streamedTblTypes, streamedTblDataSize, streamedTblDataCol1, streamedTblDataCol2);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatch1);

    const int32_t bufferedTblSize = 6;
    double bufferedTblDataCol1[bufferedTblSize] = {6.6, 5.5, 4.4, 3.3, 2.2, 1.1};
    int32_t bufferedTblDataCol2[bufferedTblSize] = {0, 1, 2, 3, 4, 5};
    VectorBatch *bufferedTblVecBatch1 =
        CreateVectorBatch(bufferedTblTypes, bufferedTblSize, bufferedTblDataCol1, bufferedTblDataCol2);
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatch1);

    // add eof flag to buffered table , need add streamed table data
    VectorBatch *bufferedTblVecBatchEof = CreateEmptyVectorBatch(bufferedTblTypes);
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchEof);

    // add eof flag to streamed table
    VectorBatch *streamedTblVecBatchEof = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof);
    auto *streamedTblVecBatchEof1 = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof1);

    VectorBatch *result = nullptr;
    bufferedTblWithExprOperator->GetOutput(&result);

    // check the join result
    long resultCol1[] = {3300, 2200, 1100};
    double resultCol2[] = {3.3, 2.2, 1.1};
    AssertVecBatchEquals(result, 2, sizeof(resultCol1) / sizeof(resultCol1[0]), resultCol1, resultCol2);
    VectorHelper::FreeVecBatch(result);

    Expr::DeleteExprs(streamedEqualKeyExprs);
    Expr::DeleteExprs(bufferedEqualKeyExprs);
    omniruntime::op::Operator::DeleteOperator(bufferedTblWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(streamedTblWithExprOperator);
    delete bufferedWithExprOperatorFactory;
    delete streamedWithExprOperatorFactory;
}

TEST(SortMergeJoinWithExprV3Test, testSmjInnerJoinExprEqualCondition)
{
    using namespace omniruntime::expressions;
    // select t1.b, t2.c from t1 full join t2 where t1.a = t2.d and t1.a = 4;
    // streamedTbl t1:  int a, long b;
    // bufferedTbl t2: double c, int d;
    string filterJsonStr = "{\"exprType\":\"BINARY\","
                           "\"returnType\":4,"
                           "\"operator\":\"EQUAL\","
                           "\"left\":{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":1,\"colVal\":0},"
                           "\"right\":{\"exprType\":\"LITERAL\",\"dataType\":1, \"isNull\":false, \"value\":4}}";
    std::vector<DataTypePtr> streamTypeVector = {IntType(), LongType()};
    DataTypes streamedTblTypes(streamTypeVector);
    auto *col0 = new FieldExpr(0, IntType());
    std::vector<Expr *> streamedEqualKeyExprs = {col0};

    std::vector<int32_t> streamedOutputCols = {1};
    OperatorConfig operatorConfig;
    StreamedTableWithExprOperatorFactoryV3 *streamedWithExprOperatorFactory =
        StreamedTableWithExprOperatorFactoryV3::CreateStreamedTableWithExprOperatorFactory(
            streamedTblTypes, streamedEqualKeyExprs, streamedOutputCols, JoinType::OMNI_JOIN_TYPE_INNER, filterJsonStr,
            operatorConfig);
    omniruntime::op::Operator *streamedTblWithExprOperator = CreateTestOperator(streamedWithExprOperatorFactory);

    std::vector<DataTypePtr> bufferTypesVector = {DoubleType(), IntType()};
    DataTypes bufferedTblTypes(bufferTypesVector);
    auto *col1 = new FieldExpr(1, IntType());
    std::vector<Expr *> bufferedEqualKeyExprs = {col1};
    std::vector<int32_t> bufferedOutputCols = {0};
    BufferedTableWithExprOperatorFactoryV3 *bufferedWithExprOperatorFactory =
        BufferedTableWithExprOperatorFactoryV3::CreateBufferedTableWithExprOperatorFactory(
            bufferedTblTypes, bufferedEqualKeyExprs, bufferedOutputCols, streamedWithExprOperatorFactory,
            operatorConfig);
    omniruntime::op::Operator *bufferedTblWithExprOperator = CreateTestOperator(bufferedWithExprOperatorFactory);

    // construct data
    const int32_t streamedTblDataSize = 6;
    int32_t streamedTblDataCol1[streamedTblDataSize] = {0, 1, 2, 3, 4, 5};
    long streamedTblDataCol2[streamedTblDataSize] = {6600, 5500, 4400, 3300, 2200, 1100};
    VectorBatch *streamedTblVecBatch1 =
        CreateVectorBatch(streamedTblTypes, streamedTblDataSize, streamedTblDataCol1, streamedTblDataCol2);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatch1);

    const int32_t bufferedTblSize = 6;
    double bufferedTblDataCol1[bufferedTblSize] = {6.6, 5.5, 4.4, 3.3, 2.2, 1.1};
    int32_t bufferedTblDataCol2[bufferedTblSize] = {0, 1, 2, 3, 4, 5};
    VectorBatch *bufferedTblVecBatch1 =
        CreateVectorBatch(bufferedTblTypes, bufferedTblSize, bufferedTblDataCol1, bufferedTblDataCol2);
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatch1);

    // add eof flag to buffered table , need add streamed table data
    VectorBatch *bufferedTblVecBatchEof = CreateEmptyVectorBatch(bufferedTblTypes);
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchEof);

    // add eof flag to streamed table
    VectorBatch *streamedTblVecBatchEof = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof);
    auto *streamedTblVecBatchEof1 = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof1);

    VectorBatch *result;
    bufferedTblWithExprOperator->GetOutput(&result);

    // check the join result
    long resultCol1[] = {2200};
    double resultCol2[] = {2.2};
    AssertVecBatchEquals(result, 2, sizeof(resultCol1) / sizeof(resultCol1[0]), resultCol1, resultCol2);
    VectorHelper::FreeVecBatch(result);

    Expr::DeleteExprs(streamedEqualKeyExprs);
    Expr::DeleteExprs(bufferedEqualKeyExprs);
    omniruntime::op::Operator::DeleteOperator(bufferedTblWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(streamedTblWithExprOperator);
    delete bufferedWithExprOperatorFactory;
    delete streamedWithExprOperatorFactory;
}

TEST(SortMergeJoinWithExprV3Test, testSmjLeftJoinstreamedGreaterThanBuffered)
{
    // select t1.b, t2.c from t1 left join t2 where t1.a = t2.d
    // streamedTbl t1:  int a, long b;
    // bufferedTbl t2: double c, int d;
    std::string blank;
    std::vector<DataTypePtr> streamTypeVector = {IntType(), LongType()};
    DataTypes streamedTblTypes(streamTypeVector);
    auto *col0 = new FieldExpr(0, IntType());
    std::vector<Expr *> streamedEqualKeyExprs = {col0};
    OperatorConfig operatorConfig;
    std::vector<int32_t> streamedOutputCols = {1};
    StreamedTableWithExprOperatorFactoryV3 *streamedWithExprOperatorFactory =
        StreamedTableWithExprOperatorFactoryV3::CreateStreamedTableWithExprOperatorFactory(
            streamedTblTypes, streamedEqualKeyExprs, streamedOutputCols, JoinType::OMNI_JOIN_TYPE_LEFT, blank,
            operatorConfig);
    omniruntime::op::Operator *streamedTblWithExprOperator = CreateTestOperator(streamedWithExprOperatorFactory);

    std::vector<DataTypePtr> bufferTypesVector = {DoubleType(), IntType()};
    DataTypes bufferedTblTypes(bufferTypesVector);
    auto *col1 = new FieldExpr(1, IntType());
    std::vector<Expr *> bufferedEqualKeyExprs = {col1};
    std::vector<int32_t> bufferedOutputCols = {0};
    BufferedTableWithExprOperatorFactoryV3 *bufferedWithExprOperatorFactory =
        BufferedTableWithExprOperatorFactoryV3::CreateBufferedTableWithExprOperatorFactory(
            bufferedTblTypes, bufferedEqualKeyExprs, bufferedOutputCols, streamedWithExprOperatorFactory,
            operatorConfig);
    omniruntime::op::Operator *bufferedTblWithExprOperator = CreateTestOperator(bufferedWithExprOperatorFactory);

    // construct data
    const int32_t streamedTblDataSize = 6;
    int32_t streamedTblDataCol1[streamedTblDataSize] = {1, 2, 3, 4, 5, 8};
    long streamedTblDataCol2[streamedTblDataSize] = {1100, 2200, 3300, 4400, 5500, 8800};
    VectorBatch *streamedTblVecBatch1 =
        CreateVectorBatch(streamedTblTypes, streamedTblDataSize, streamedTblDataCol1, streamedTblDataCol2);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatch1);

    const int32_t bufferedTblSize = 6;
    double bufferedTblDataCol1[bufferedTblSize] = {1.1, 2.2, 3.3, 4.4, 5.5, 6.6};
    int32_t bufferedTblDataCol2[bufferedTblSize] = {1, 2, 3, 4, 5, 6};
    VectorBatch *bufferedTblVecBatch1 =
        CreateVectorBatch(bufferedTblTypes, bufferedTblSize, bufferedTblDataCol1, bufferedTblDataCol2);
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatch1);

    // add eof flag to buffered table , need add streamed table data
    VectorBatch *bufferedTblVecBatchEof = CreateEmptyVectorBatch(bufferedTblTypes);
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchEof);

    // add eof flag to streamed table
    VectorBatch *streamedTblVecBatchEof = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof);
    auto *streamedTblVecBatchEof1 = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof1);

    VectorBatch *result;
    bufferedTblWithExprOperator->GetOutput(&result);

    // check the join result
    long resultCol1[] = {1100, 2200, 3300, 4400, 5500, 8800};
    double resultCol2[] = {1.1, 2.2, 3.3, 4.4, 5.5, 0};
    AssertVecBatchEquals(result, 2, sizeof(resultCol1) / sizeof(resultCol1[0]), resultCol1, resultCol2);
    EXPECT_TRUE(result->Get(1)->IsNull(5));
    VectorHelper::FreeVecBatch(result);

    Expr::DeleteExprs(streamedEqualKeyExprs);
    Expr::DeleteExprs(bufferedEqualKeyExprs);
    omniruntime::op::Operator::DeleteOperator(bufferedTblWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(streamedTblWithExprOperator);
    delete bufferedWithExprOperatorFactory;
    delete streamedWithExprOperatorFactory;
}

TEST(SortMergeJoinWithExprV3Test, testSmjLeftJoinstreamedLessThanBuffered)
{
    // select t1.b, t2.c from t1 left join t2 where t1.a = t2.d
    // streamedTbl t1:  int a, long b;
    // bufferedTbl t2: double c, int d;
    std::string blank;
    std::vector<DataTypePtr> streamTypeVector = {IntType(), LongType()};
    DataTypes streamedTblTypes(streamTypeVector);
    auto *col0 = new FieldExpr(0, IntType());
    std::vector<Expr *> streamedEqualKeyExprs = {col0};
    OperatorConfig operatorConfig;
    std::vector<int32_t> streamedOutputCols = {1};
    StreamedTableWithExprOperatorFactoryV3 *streamedWithExprOperatorFactory =
        StreamedTableWithExprOperatorFactoryV3::CreateStreamedTableWithExprOperatorFactory(
            streamedTblTypes, streamedEqualKeyExprs, streamedOutputCols, JoinType::OMNI_JOIN_TYPE_LEFT, blank,
            operatorConfig);
    omniruntime::op::Operator *streamedTblWithExprOperator = CreateTestOperator(streamedWithExprOperatorFactory);

    std::vector<DataTypePtr> bufferTypesVector = {DoubleType(), IntType()};
    DataTypes bufferedTblTypes(bufferTypesVector);
    auto *col1 = new FieldExpr(1, IntType());
    std::vector<Expr *> bufferedEqualKeyExprs = {col1};
    std::vector<int32_t> bufferedOutputCols = {0};
    BufferedTableWithExprOperatorFactoryV3 *bufferedWithExprOperatorFactory =
        BufferedTableWithExprOperatorFactoryV3::CreateBufferedTableWithExprOperatorFactory(
            bufferedTblTypes, bufferedEqualKeyExprs, bufferedOutputCols, streamedWithExprOperatorFactory,
            operatorConfig);
    omniruntime::op::Operator *bufferedTblWithExprOperator = CreateTestOperator(bufferedWithExprOperatorFactory);

    // construct data
    const int32_t streamedTblDataSize = 5;
    int32_t streamedTblDataCol1[streamedTblDataSize] = {1, 2, 3, 4, 5};
    long streamedTblDataCol2[streamedTblDataSize] = {1100, 2200, 3300, 4400, 5500};
    VectorBatch *streamedTblVecBatch1 =
        CreateVectorBatch(streamedTblTypes, streamedTblDataSize, streamedTblDataCol1, streamedTblDataCol2);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatch1);

    const int32_t bufferedTblSize = 5;
    double bufferedTblDataCol1[bufferedTblSize] = {1.1, 2.2, 3.3, 4.4, 8.8};
    int32_t bufferedTblDataCol2[bufferedTblSize] = {1, 2, 3, 4, 8};
    VectorBatch *bufferedTblVecBatch1 =
        CreateVectorBatch(bufferedTblTypes, bufferedTblSize, bufferedTblDataCol1, bufferedTblDataCol2);
    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatch1);

    // add eof flag to streamed table
    VectorBatch *streamedTblVecBatchEof = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof);
    auto *streamedTblVecBatchEof1 = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof1);
    VectorBatch *result;
    bufferedTblWithExprOperator->GetOutput(&result);

    // check the join result
    long resultCol1[] = {1100, 2200, 3300, 4400, 5500};
    double resultCol2[] = {1.1, 2.2, 3.3, 4.4, 0};
    AssertVecBatchEquals(result, 2, sizeof(resultCol1) / sizeof(resultCol1[0]), resultCol1, resultCol2);
    EXPECT_TRUE(result->Get(1)->IsNull(4));
    VectorHelper::FreeVecBatch(result);

    Expr::DeleteExprs(streamedEqualKeyExprs);
    Expr::DeleteExprs(bufferedEqualKeyExprs);
    omniruntime::op::Operator::DeleteOperator(bufferedTblWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(streamedTblWithExprOperator);
    delete bufferedWithExprOperatorFactory;
    delete streamedWithExprOperatorFactory;
}

TEST(SortMergeJoinWithExprV3Test, testSmjLeftJoinMixGreaterLessThanBuffered)
{
    // select t1.b, t2.c from t1 left join t2 where t1.a = t2.d
    // streamedTbl t1:  int a, long b;
    // bufferedTbl t2: double c, int d;
    std::string blank;
    std::vector<DataTypePtr> streamTypeVector = {IntType(), LongType()};
    DataTypes streamedTblTypes(streamTypeVector);
    auto *col0 = new FieldExpr(0, IntType());
    std::vector<Expr *> streamedEqualKeyExprs = {col0};
    OperatorConfig operatorConfig;
    std::vector<int32_t> streamedOutputCols = {1};
    StreamedTableWithExprOperatorFactoryV3 *streamedWithExprOperatorFactory =
        StreamedTableWithExprOperatorFactoryV3::CreateStreamedTableWithExprOperatorFactory(
            streamedTblTypes, streamedEqualKeyExprs, streamedOutputCols, JoinType::OMNI_JOIN_TYPE_LEFT, blank,
            operatorConfig);
    omniruntime::op::Operator *streamedTblWithExprOperator = CreateTestOperator(streamedWithExprOperatorFactory);

    std::vector<DataTypePtr> bufferTypesVector = {DoubleType(), IntType()};
    DataTypes bufferedTblTypes(bufferTypesVector);
    auto *col1 = new FieldExpr(1, IntType());
    std::vector<Expr *> bufferedEqualKeyExprs = {col1};
    std::vector<int32_t> bufferedOutputCols = {0};
    BufferedTableWithExprOperatorFactoryV3 *bufferedWithExprOperatorFactory =
        BufferedTableWithExprOperatorFactoryV3::CreateBufferedTableWithExprOperatorFactory(
            bufferedTblTypes, bufferedEqualKeyExprs, bufferedOutputCols, streamedWithExprOperatorFactory,
            operatorConfig);
    omniruntime::op::Operator *bufferedTblWithExprOperator = CreateTestOperator(bufferedWithExprOperatorFactory);

    // construct data
    const int32_t streamedTblDataSize = 5;
    int32_t streamedTblDataCol1[streamedTblDataSize] = {1, 3, 3, 4, 5};
    long streamedTblDataCol2[streamedTblDataSize] = {1100, 3300, 3300, 4400, 5500};
    VectorBatch *streamedTblVecBatch1 =
        CreateVectorBatch(streamedTblTypes, streamedTblDataSize, streamedTblDataCol1, streamedTblDataCol2);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatch1);

    const int32_t bufferedTblSize = 5;
    double bufferedTblDataCol1[bufferedTblSize] = {1.1, 2.2, 4.4, 4.4, 8.8};
    int32_t bufferedTblDataCol2[bufferedTblSize] = {1, 2, 4, 4, 8};
    VectorBatch *bufferedTblVecBatch1 =
        CreateVectorBatch(bufferedTblTypes, bufferedTblSize, bufferedTblDataCol1, bufferedTblDataCol2);
    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatch1);

    // add eof flag to streamed table
    VectorBatch *streamedTblVecBatchEof = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof);
    auto *streamedTblVecBatchEof1 = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof1);

    VectorBatch *result;
    bufferedTblWithExprOperator->GetOutput(&result);

    // check the join result
    long resultCol1[] = {1100, 3300, 3300, 4400, 4400, 5500};
    double resultCol2[] = {1.1, 0, 0, 4.4, 4.4, 0};
    AssertVecBatchEquals(result, 2, sizeof(resultCol1) / sizeof(resultCol1[0]), resultCol1, resultCol2);
    EXPECT_TRUE(result->Get(1)->IsNull(1));
    EXPECT_TRUE(result->Get(1)->IsNull(2));
    EXPECT_TRUE(result->Get(1)->IsNull(5));
    VectorHelper::FreeVecBatch(result);

    Expr::DeleteExprs(streamedEqualKeyExprs);
    Expr::DeleteExprs(bufferedEqualKeyExprs);
    omniruntime::op::Operator::DeleteOperator(bufferedTblWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(streamedTblWithExprOperator);
    delete bufferedWithExprOperatorFactory;
    delete streamedWithExprOperatorFactory;
}

TEST(SortMergeJoinWithExprV3Test, testSmjLeftJoinStreamedWithNullJoinKeyFirst)
{
    // select t1.b, t2.c from t1 left join t2 where t1.a = t2.d
    // streamedTbl t1:  int a, long b;
    // bufferedTbl t2: double c, int d;
    std::string blank;
    std::vector<DataTypePtr> streamTypeVector = {IntType(), LongType()};
    DataTypes streamedTblTypes(streamTypeVector);
    auto *col0 = new FieldExpr(0, IntType());
    std::vector<Expr *> streamedEqualKeyExprs = {col0};
    OperatorConfig operatorConfig;
    std::vector<int32_t> streamedOutputCols = {1};
    StreamedTableWithExprOperatorFactoryV3 *streamedWithExprOperatorFactory =
        StreamedTableWithExprOperatorFactoryV3::CreateStreamedTableWithExprOperatorFactory(
            streamedTblTypes, streamedEqualKeyExprs, streamedOutputCols, JoinType::OMNI_JOIN_TYPE_LEFT, blank,
            operatorConfig);
    omniruntime::op::Operator *streamedTblWithExprOperator = CreateTestOperator(streamedWithExprOperatorFactory);

    std::vector<DataTypePtr> bufferTypesVector = {DoubleType(), IntType()};
    DataTypes bufferedTblTypes(bufferTypesVector);
    auto *col1 = new FieldExpr(1, IntType());
    std::vector<Expr *> bufferedEqualKeyExprs = {col1};
    std::vector<int32_t> bufferedOutputCols = {0};
    BufferedTableWithExprOperatorFactoryV3 *bufferedWithExprOperatorFactory =
        BufferedTableWithExprOperatorFactoryV3::CreateBufferedTableWithExprOperatorFactory(
            bufferedTblTypes, bufferedEqualKeyExprs, bufferedOutputCols, streamedWithExprOperatorFactory,
            operatorConfig);
    omniruntime::op::Operator *bufferedTblWithExprOperator = CreateTestOperator(bufferedWithExprOperatorFactory);

    // construct data
    const int32_t streamedTblDataSize = 7;
    int32_t streamedTblDataCol1[streamedTblDataSize] = {1, 2, 3, 4, 5, 6, 7};
    long streamedTblDataCol2[streamedTblDataSize] = {1100, 2200, 3300, 4400, 5500, 6600, 7700};
    VectorBatch *streamedTblVecBatch1 =
        CreateVectorBatch(streamedTblTypes, streamedTblDataSize, streamedTblDataCol1, streamedTblDataCol2);
    streamedTblVecBatch1->Get(0)->SetNull(0); // NULL, NULL, 4, 5, 6, 7
    streamedTblVecBatch1->Get(0)->SetNull(1); // NULL, NULL, 4, 5, 6, 7
    streamedTblWithExprOperator->AddInput(streamedTblVecBatch1);

    const int32_t bufferedTblSize = 6;
    double bufferedTblDataCol1[bufferedTblSize] = {1.1, 2.2, 3.3, 4.4, 6.6, 8.8};
    int32_t bufferedTblDataCol2[bufferedTblSize] = {1, 2, 3, 4, 6, 8};
    VectorBatch *bufferedTblVecBatch1 =
        CreateVectorBatch(bufferedTblTypes, bufferedTblSize, bufferedTblDataCol1, bufferedTblDataCol2);
    bufferedTblVecBatch1->Get(1)->SetNull(0); // NULL, NULL, 3, 4, 6, 8
    bufferedTblVecBatch1->Get(1)->SetNull(1); // NULL, NULL, 3, 4, 6, 8
    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatch1);

    // add eof flag to streamed table
    VectorBatch *streamedTblVecBatchEof = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof);
    auto *streamedTblVecBatchEof1 = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof1);

    VectorBatch *result;
    bufferedTblWithExprOperator->GetOutput(&result);

    // check the join result
    long resultCol1[] = {1100, 2200, 3300, 4400, 5500, 6600, 7700};
    double resultCol2[] = {0, 0, 3.3, 4.4, 0, 6.6, 0};
    AssertVecBatchEquals(result, 2, sizeof(resultCol1) / sizeof(resultCol1[0]), resultCol1, resultCol2);
    EXPECT_TRUE(result->Get(1)->IsNull(0));
    EXPECT_TRUE(result->Get(1)->IsNull(1));
    EXPECT_TRUE(result->Get(1)->IsNull(4));
    EXPECT_TRUE(result->Get(1)->IsNull(6));
    VectorHelper::FreeVecBatch(result);

    Expr::DeleteExprs(streamedEqualKeyExprs);
    Expr::DeleteExprs(bufferedEqualKeyExprs);
    omniruntime::op::Operator::DeleteOperator(bufferedTblWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(streamedTblWithExprOperator);
    delete bufferedWithExprOperatorFactory;
    delete streamedWithExprOperatorFactory;
}

TEST(SortMergeJoinWithExprV3Test, testSmjLeftJoinMultiColumBatch)
{
    // select t1.b, t2.c from t1 left join t2 where t1.a = t2.d
    // streamedTbl t1:  int a, long b;
    // bufferedTbl t2: double c, int d;
    std::string blank;
    std::vector<DataTypePtr> streamTypeVector = {IntType(), LongType()};
    DataTypes streamedTblTypes(streamTypeVector);
    auto *col0 = new FieldExpr(0, IntType());
    std::vector<Expr *> streamedEqualKeyExprs = {col0};
    OperatorConfig operatorConfig;
    std::vector<int32_t> streamedOutputCols = {1};
    StreamedTableWithExprOperatorFactoryV3 *streamedWithExprOperatorFactory =
        StreamedTableWithExprOperatorFactoryV3::CreateStreamedTableWithExprOperatorFactory(
            streamedTblTypes, streamedEqualKeyExprs, streamedOutputCols, JoinType::OMNI_JOIN_TYPE_LEFT, blank,
            operatorConfig);
    omniruntime::op::Operator *streamedTblWithExprOperator = CreateTestOperator(streamedWithExprOperatorFactory);

    std::vector<DataTypePtr> bufferTypesVector = {DoubleType(), IntType()};
    DataTypes bufferedTblTypes(bufferTypesVector);
    auto *col1 = new FieldExpr(1, IntType());
    std::vector<Expr *> bufferedEqualKeyExprs = {col1};
    std::vector<int32_t> bufferedOutputCols = {0};
    BufferedTableWithExprOperatorFactoryV3 *bufferedWithExprOperatorFactory =
        BufferedTableWithExprOperatorFactoryV3::CreateBufferedTableWithExprOperatorFactory(
            bufferedTblTypes, bufferedEqualKeyExprs, bufferedOutputCols, streamedWithExprOperatorFactory,
            operatorConfig);
    omniruntime::op::Operator *bufferedTblWithExprOperator = CreateTestOperator(bufferedWithExprOperatorFactory);

    int32_t streamedCol1Row1[1] = {1};
    long streamedCol2Row1[1] = {1100};
    // construct streamed data
    VectorBatch *streamedTblVecBatchRow1 = CreateVectorBatch(streamedTblTypes, 1, streamedCol1Row1, streamedCol2Row1);
    int32_t streamedCol1Row2[1] = {2};
    long streamedCol2Row2[1] = {2200};
    VectorBatch *streamedTblVecBatchRow2 = CreateVectorBatch(streamedTblTypes, 1, streamedCol1Row2, streamedCol2Row2);
    int32_t streamedCol1Row3[1] = {3};
    long streamedCol2Row3[1] = {3300};
    VectorBatch *streamedTblVecBatchRow3 = CreateVectorBatch(streamedTblTypes, 1, streamedCol1Row3, streamedCol2Row3);
    int32_t streamedCol1Row4[1] = {4};
    long streamedCol2Row4[1] = {4400};
    VectorBatch *streamedTblVecBatchRow4 = CreateVectorBatch(streamedTblTypes, 1, streamedCol1Row4, streamedCol2Row4);
    int32_t streamedCol1Row5[1] = {5};
    long streamedCol2Row5[1] = {5500};
    VectorBatch *streamedTblVecBatchRow5 = CreateVectorBatch(streamedTblTypes, 1, streamedCol1Row5, streamedCol2Row5);
    int32_t streamedCol1Row6[1] = {8};
    long streamedCol2Row6[1] = {8800};
    VectorBatch *streamedTblVecBatchRow6 = CreateVectorBatch(streamedTblTypes, 1, streamedCol1Row6, streamedCol2Row6);

    // construct buffered data
    double bufferedCol1Row1[1] = {1.1};
    int32_t bufferedCol2Row1[1] = {1};
    VectorBatch *bufferedTblVecBatchRow1 = CreateVectorBatch(bufferedTblTypes, 1, bufferedCol1Row1, bufferedCol2Row1);
    double bufferedCol1Row2[1] = {2.2};
    int32_t bufferedCol2Row2[1] = {2};
    VectorBatch *bufferedTblVecBatchRow2 = CreateVectorBatch(bufferedTblTypes, 1, bufferedCol1Row2, bufferedCol2Row2);
    double bufferedCol1Row3[1] = {3.3};
    int32_t bufferedCol2Row3[1] = {3};
    VectorBatch *bufferedTblVecBatchRow3 = CreateVectorBatch(bufferedTblTypes, 1, bufferedCol1Row3, bufferedCol2Row3);
    double bufferedCol1Row4[1] = {4.4};
    int32_t bufferedCol2Row4[1] = {4};
    VectorBatch *bufferedTblVecBatchRow4 = CreateVectorBatch(bufferedTblTypes, 1, bufferedCol1Row4, bufferedCol2Row4);
    double bufferedCol1Row5[1] = {5.5};
    int32_t bufferedCol2Row5[1] = {5};
    VectorBatch *bufferedTblVecBatchRow5 = CreateVectorBatch(bufferedTblTypes, 1, bufferedCol1Row5, bufferedCol2Row5);
    double bufferedCol1Row6[1] = {6.6};
    int32_t bufferedCol2Row6[1] = {6};
    VectorBatch *bufferedTblVecBatchRow6 = CreateVectorBatch(bufferedTblTypes, 1, bufferedCol1Row6, bufferedCol2Row6);

    // join start add streamed table data
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchRow1);

    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchRow1);

    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchRow2);

    // need add streamed table data
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchRow2);

    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchRow3);

    // need add streamed table data
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchRow3);

    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchRow4);

    // need add streamed table data
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchRow4);

    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchRow5);

    // need add streamed table data
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchRow5);

    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchRow6);

    // need add streamed table data
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchRow6);

    // add eof flag to buffered table , need add streamed table data
    VectorBatch *bufferedTblVecBatchEof = CreateEmptyVectorBatch(bufferedTblTypes);
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchEof);

    // add eof flag to streamed table
    VectorBatch *streamedTblVecBatchEof = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof);
    VectorBatch *streamedTblVecBatchEof1 = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof1);

    VectorBatch *result;
    bufferedTblWithExprOperator->GetOutput(&result);

    // check the join result
    long resultCol1[] = {1100, 2200, 3300, 4400, 5500, 8800};
    double resultCol2[] = {1.1, 2.2, 3.3, 4.4, 5.5, 0};
    AssertVecBatchEquals(result, 2, sizeof(resultCol1) / sizeof(resultCol1[0]), resultCol1, resultCol2);
    EXPECT_TRUE(result->Get(1)->IsNull(5));
    VectorHelper::FreeVecBatch(result);

    Expr::DeleteExprs(streamedEqualKeyExprs);
    Expr::DeleteExprs(bufferedEqualKeyExprs);
    omniruntime::op::Operator::DeleteOperator(bufferedTblWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(streamedTblWithExprOperator);
    delete bufferedWithExprOperatorFactory;
    delete streamedWithExprOperatorFactory;
}

TEST(SortMergeJoinWithExprV3Test, testSmjLeftJoinNullFirstWithRepeatRowsAndMultiColumBatch)
{
    // select t1.b, t2.c from t1 left join t2 where t1.a = t2.d
    // streamedTbl t1:  int a, long b;
    // bufferedTbl t2: double c, int d;
    std::string blank;
    std::vector<DataTypePtr> streamTypeVector = {IntType(), LongType()};
    DataTypes streamedTblTypes(streamTypeVector);
    auto *col0 = new FieldExpr(0, IntType());
    std::vector<Expr *> streamedEqualKeyExprs = {col0};
    OperatorConfig operatorConfig;
    std::vector<int32_t> streamedOutputCols = {1};
    StreamedTableWithExprOperatorFactoryV3 *streamedWithExprOperatorFactory =
        StreamedTableWithExprOperatorFactoryV3::CreateStreamedTableWithExprOperatorFactory(
            streamedTblTypes, streamedEqualKeyExprs, streamedOutputCols, JoinType::OMNI_JOIN_TYPE_LEFT, blank,
            operatorConfig);
    omniruntime::op::Operator *streamedTblWithExprOperator = CreateTestOperator(streamedWithExprOperatorFactory);

    std::vector<DataTypePtr> bufferTypesVector = {DoubleType(), IntType()};
    DataTypes bufferedTblTypes(bufferTypesVector);
    auto *col1 = new FieldExpr(1, IntType());
    std::vector<Expr *> bufferedEqualKeyExprs = {col1};
    std::vector<int32_t> bufferedOutputCols = {0};
    BufferedTableWithExprOperatorFactoryV3 *bufferedWithExprOperatorFactory =
        BufferedTableWithExprOperatorFactoryV3::CreateBufferedTableWithExprOperatorFactory(
            bufferedTblTypes, bufferedEqualKeyExprs, bufferedOutputCols, streamedWithExprOperatorFactory,
            operatorConfig);
    omniruntime::op::Operator *bufferedTblWithExprOperator = CreateTestOperator(bufferedWithExprOperatorFactory);

    int32_t streamedCol1Row1[1] = {1};
    long streamedCol2Row1[1] = {1100};
    int32_t streamedCol1Row2[1] = {2};
    long streamedCol2Row2[1] = {2200};
    int32_t streamedCol1Row3[1] = {4};
    long streamedCol2Row3[1] = {4400};
    int32_t streamedCol1Row4[1] = {5};
    long streamedCol2Row4[1] = {5500};
    int32_t streamedCol1Row5[1] = {5};
    long streamedCol2Row5[1] = {5500};
    int32_t streamedCol1Row6[1] = {6};
    long streamedCol2Row6[1] = {6600};
    // construct streamed data
    VectorBatch *streamedTblVecBatchRow1 = CreateVectorBatch(streamedTblTypes, 1, streamedCol1Row1, streamedCol2Row1);
    VectorBatch *streamedTblVecBatchRow2 = CreateVectorBatch(streamedTblTypes, 1, streamedCol1Row2, streamedCol2Row2);
    VectorBatch *streamedTblVecBatchRow3 = CreateVectorBatch(streamedTblTypes, 1, streamedCol1Row3, streamedCol2Row3);
    VectorBatch *streamedTblVecBatchRow4 = CreateVectorBatch(streamedTblTypes, 1, streamedCol1Row4, streamedCol2Row4);
    VectorBatch *streamedTblVecBatchRow5 = CreateVectorBatch(streamedTblTypes, 1, streamedCol1Row5, streamedCol2Row5);
    VectorBatch *streamedTblVecBatchRow6 = CreateVectorBatch(streamedTblTypes, 1, streamedCol1Row6, streamedCol2Row6);
    streamedTblVecBatchRow1->Get(0)->SetNull(0); // null, null, 4, 5, 5, 6
    streamedTblVecBatchRow2->Get(0)->SetNull(0); // null, null, 4, 5, 5, 6

    double bufferedCol1Row1[1] = {1.1};
    int32_t bufferedCol2Row1[1] = {1};
    double bufferedCol1Row2[1] = {2.2};
    int32_t bufferedCol2Row2[1] = {2};
    double bufferedCol1Row3[1] = {3.3};
    int32_t bufferedCol2Row3[1] = {3};
    double bufferedCol1Row4[1] = {4.4};
    int32_t bufferedCol2Row4[1] = {4};
    double bufferedCol1Row5[1] = {5.5};
    int32_t bufferedCol2Row5[1] = {5};
    double bufferedCol1Row6[1] = {5.5};
    int32_t bufferedCol2Row6[1] = {5};
    double bufferedCol1Row7[1] = {6.6};
    int32_t bufferedCol2Row7[1] = {6};
    // construct buffered data
    VectorBatch *bufferedTblVecBatchRow1 = CreateVectorBatch(bufferedTblTypes, 1, bufferedCol1Row1, bufferedCol2Row1);
    VectorBatch *bufferedTblVecBatchRow2 = CreateVectorBatch(bufferedTblTypes, 1, bufferedCol1Row2, bufferedCol2Row2);
    VectorBatch *bufferedTblVecBatchRow3 = CreateVectorBatch(bufferedTblTypes, 1, bufferedCol1Row3, bufferedCol2Row3);
    VectorBatch *bufferedTblVecBatchRow4 = CreateVectorBatch(bufferedTblTypes, 1, bufferedCol1Row4, bufferedCol2Row4);
    VectorBatch *bufferedTblVecBatchRow5 = CreateVectorBatch(bufferedTblTypes, 1, bufferedCol1Row5, bufferedCol2Row5);
    VectorBatch *bufferedTblVecBatchRow6 = CreateVectorBatch(bufferedTblTypes, 1, bufferedCol1Row6, bufferedCol2Row6);
    VectorBatch *bufferedTblVecBatchRow7 = CreateVectorBatch(bufferedTblTypes, 1, bufferedCol1Row7, bufferedCol2Row7);

    // join start add streamed table data
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchRow1);

    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchRow1);

    // need add streamed table data
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchRow2);

    // need add streamed table data
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchRow3);

    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchRow2);

    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchRow3);

    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchRow4);

    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchRow5);

    // need add streamed table data
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchRow4);

    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchRow6);

    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchRow7);

    // need add streamed table data
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchRow5);

    // need add streamed table data
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchRow6);

    // add eof flag to buffered table , need add streamed table data
    VectorBatch *bufferedTblVecBatchEof = CreateEmptyVectorBatch(bufferedTblTypes);
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchEof);

    // add eof flag to streamed table
    VectorBatch *streamedTblVecBatchEof = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof);
    VectorBatch *streamedTblVecBatchEof1 = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof1);

    VectorBatch *result;
    bufferedTblWithExprOperator->GetOutput(&result);

    // check the join result
    long resultCol1[] = {1100, 2200, 4400, 5500, 5500, 5500, 5500, 6600};
    double resultCol2[] = {0, 0, 4.4, 5.5, 5.5, 5.5, 5.5, 6.6};
    AssertVecBatchEquals(result, 2, sizeof(resultCol1) / sizeof(resultCol1[0]), resultCol1, resultCol2);
    EXPECT_TRUE(result->Get(1)->IsNull(0));
    EXPECT_TRUE(result->Get(1)->IsNull(1));
    VectorHelper::FreeVecBatch(result);

    Expr::DeleteExprs(streamedEqualKeyExprs);
    Expr::DeleteExprs(bufferedEqualKeyExprs);
    omniruntime::op::Operator::DeleteOperator(bufferedTblWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(streamedTblWithExprOperator);
    delete bufferedWithExprOperatorFactory;
    delete streamedWithExprOperatorFactory;
}

TEST(SortMergeJoinWithExprV3Test, testSmjLeftJoinRepeatRowsAndMultiColumBatch)
{
    // select t1.b, t2.c from t1 left join t2 where t1.a = t2.d
    // streamedTbl t1:  int a, long b;
    // bufferedTbl t2: double c, int d;
    std::string blank;
    std::vector<DataTypePtr> streamTypeVector = {IntType(), LongType()};
    DataTypes streamedTblTypes(streamTypeVector);
    auto *col0 = new FieldExpr(0, IntType());
    std::vector<Expr *> streamedEqualKeyExprs = {col0};
    OperatorConfig operatorConfig;
    std::vector<int32_t> streamedOutputCols = {1};
    StreamedTableWithExprOperatorFactoryV3 *streamedWithExprOperatorFactory =
        StreamedTableWithExprOperatorFactoryV3::CreateStreamedTableWithExprOperatorFactory(
            streamedTblTypes, streamedEqualKeyExprs, streamedOutputCols, JoinType::OMNI_JOIN_TYPE_LEFT, blank,
            operatorConfig);
    omniruntime::op::Operator *streamedTblWithExprOperator = CreateTestOperator(streamedWithExprOperatorFactory);

    std::vector<DataTypePtr> bufferTypesVector = {DoubleType(), IntType()};
    DataTypes bufferedTblTypes(bufferTypesVector);
    auto *col1 = new FieldExpr(1, IntType());
    std::vector<Expr *> bufferedEqualKeyExprs = {col1};
    std::vector<int32_t> bufferedOutputCols = {0};
    BufferedTableWithExprOperatorFactoryV3 *bufferedWithExprOperatorFactory =
        BufferedTableWithExprOperatorFactoryV3::CreateBufferedTableWithExprOperatorFactory(
            bufferedTblTypes, bufferedEqualKeyExprs, bufferedOutputCols, streamedWithExprOperatorFactory,
            operatorConfig);
    omniruntime::op::Operator *bufferedTblWithExprOperator = CreateTestOperator(bufferedWithExprOperatorFactory);

    int32_t streamedCol1Row1[1] = {1};
    long streamedCol2Row1[1] = {1100};
    int32_t streamedCol1Row2[1] = {2};
    long streamedCol2Row2[1] = {2200};
    int32_t streamedCol1Row3[1] = {4};
    long streamedCol2Row3[1] = {4400};
    int32_t streamedCol1Row4[1] = {5};
    long streamedCol2Row4[1] = {5500};
    int32_t streamedCol1Row5[1] = {5};
    long streamedCol2Row5[1] = {5500};
    int32_t streamedCol1Row6[2] = {5, 5};
    long streamedCol2Row6[2] = {5500, 5500};
    int32_t streamedCol1Row7[1] = {6};
    long streamedCol2Row7[1] = {6600};
    // construct streamed data
    VectorBatch *streamedTblVecBatchRow1 = CreateVectorBatch(streamedTblTypes, 1, streamedCol1Row1, streamedCol2Row1);
    VectorBatch *streamedTblVecBatchRow2 = CreateVectorBatch(streamedTblTypes, 1, streamedCol1Row2, streamedCol2Row2);
    VectorBatch *streamedTblVecBatchRow3 = CreateVectorBatch(streamedTblTypes, 1, streamedCol1Row3, streamedCol2Row3);
    VectorBatch *streamedTblVecBatchRow4 = CreateVectorBatch(streamedTblTypes, 1, streamedCol1Row4, streamedCol2Row4);
    VectorBatch *streamedTblVecBatchRow5 = CreateVectorBatch(streamedTblTypes, 1, streamedCol1Row5, streamedCol2Row5);
    VectorBatch *streamedTblVecBatchRow6 = CreateVectorBatch(streamedTblTypes, 2, streamedCol1Row6, streamedCol2Row6);
    VectorBatch *streamedTblVecBatchRow7 = CreateVectorBatch(streamedTblTypes, 1, streamedCol1Row7, streamedCol2Row7);

    double bufferedCol1Row1[1] = {1.1};
    int32_t bufferedCol2Row1[1] = {1};
    double bufferedCol1Row2[1] = {2.2};
    int32_t bufferedCol2Row2[1] = {2};
    double bufferedCol1Row3[1] = {3.3};
    int32_t bufferedCol2Row3[1] = {3};
    double bufferedCol1Row4[1] = {4.4};
    int32_t bufferedCol2Row4[1] = {4};
    double bufferedCol1Row5[1] = {5.5};
    int32_t bufferedCol2Row5[1] = {5};
    double bufferedCol1Row6[2] = {5.5, 5.5};
    int32_t bufferedCol2Row6[2] = {5, 5};
    double bufferedCol1Row7[1] = {5.5};
    int32_t bufferedCol2Row7[1] = {5};
    double bufferedCol1Row8[1] = {6.6};
    int32_t bufferedCol2Row8[1] = {6};
    // construct buffered data
    VectorBatch *bufferedTblVecBatchRow1 = CreateVectorBatch(bufferedTblTypes, 1, bufferedCol1Row1, bufferedCol2Row1);
    VectorBatch *bufferedTblVecBatchRow2 = CreateVectorBatch(bufferedTblTypes, 1, bufferedCol1Row2, bufferedCol2Row2);
    VectorBatch *bufferedTblVecBatchRow3 = CreateVectorBatch(bufferedTblTypes, 1, bufferedCol1Row3, bufferedCol2Row3);
    VectorBatch *bufferedTblVecBatchRow4 = CreateVectorBatch(bufferedTblTypes, 1, bufferedCol1Row4, bufferedCol2Row4);
    VectorBatch *bufferedTblVecBatchRow5 = CreateVectorBatch(bufferedTblTypes, 1, bufferedCol1Row5, bufferedCol2Row5);
    VectorBatch *bufferedTblVecBatchRow6 = CreateVectorBatch(bufferedTblTypes, 2, bufferedCol1Row6, bufferedCol2Row6);
    VectorBatch *bufferedTblVecBatchRow7 = CreateVectorBatch(bufferedTblTypes, 1, bufferedCol1Row7, bufferedCol2Row7);
    VectorBatch *bufferedTblVecBatchRow8 = CreateVectorBatch(bufferedTblTypes, 1, bufferedCol1Row8, bufferedCol2Row8);

    // join start add streamed table data
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchRow1);

    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchRow1);

    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchRow2);

    // need add streamed table data
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchRow2);

    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchRow3);

    // need add streamed table data
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchRow3);

    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchRow4);

    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchRow5);

    // need add streamed table data
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchRow4);

    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchRow6);

    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchRow7);

    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchRow8);

    // need add streamed table data
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchRow5);

    // need add streamed table data
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchRow6);

    // need add streamed table data
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchRow7);

    // add eof flag to buffered table , need add streamed table data
    VectorBatch *bufferedTblVecBatchEof = CreateEmptyVectorBatch(bufferedTblTypes);
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchEof);

    // add eof flag to streamed table
    VectorBatch *streamedTblVecBatchEof = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof);
    VectorBatch *streamedTblVecBatchEof1 = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof1);

    VectorBatch *result;
    bufferedTblWithExprOperator->GetOutput(&result);

    // check the join result
    long resultCol1[] = {1100, 2200, 4400, 5500, 5500, 5500, 5500, 5500, 5500, 5500,
                         5500, 5500, 5500, 5500, 5500, 5500, 5500, 5500, 5500, 6600};
    double resultCol2[] = {1.1, 2.2, 4.4, 5.5, 5.5, 5.5, 5.5, 5.5, 5.5, 5.5,
                           5.5, 5.5, 5.5, 5.5, 5.5, 5.5, 5.5, 5.5, 5.5, 6.6};
    AssertVecBatchEquals(result, 2, sizeof(resultCol1) / sizeof(resultCol1[0]), resultCol1, resultCol2);
    VectorHelper::FreeVecBatch(result);

    Expr::DeleteExprs(streamedEqualKeyExprs);
    Expr::DeleteExprs(bufferedEqualKeyExprs);
    omniruntime::op::Operator::DeleteOperator(bufferedTblWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(streamedTblWithExprOperator);
    delete bufferedWithExprOperatorFactory;
    delete streamedWithExprOperatorFactory;
}

TEST(SortMergeJoinWithExprV3Test, testSmjLeftJoinStreamedWithEmptyBuffered)
{
    // select t1.b, t2.c from t1 left join t2 where t1.a = t2.d
    // streamedTbl t1:  int a, long b;
    // bufferedTbl t2: double c, int d;
    std::string blank;
    std::vector<DataTypePtr> streamTypeVector = {IntType(), LongType()};
    DataTypes streamedTblTypes(streamTypeVector);
    auto *col0 = new FieldExpr(0, IntType());
    std::vector<Expr *> streamedEqualKeyExprs = {col0};
    OperatorConfig operatorConfig;
    std::vector<int32_t> streamedOutputCols = {1};
    StreamedTableWithExprOperatorFactoryV3 *streamedWithExprOperatorFactory =
        StreamedTableWithExprOperatorFactoryV3::CreateStreamedTableWithExprOperatorFactory(
            streamedTblTypes, streamedEqualKeyExprs, streamedOutputCols, JoinType::OMNI_JOIN_TYPE_LEFT, blank,
            operatorConfig);
    omniruntime::op::Operator *streamedTblWithExprOperator = CreateTestOperator(streamedWithExprOperatorFactory);

    std::vector<DataTypePtr> bufferTypesVector = {DoubleType(), IntType()};
    DataTypes bufferedTblTypes(bufferTypesVector);
    auto *col1 = new FieldExpr(1, IntType());
    std::vector<Expr *> bufferedEqualKeyExprs = {col1};
    std::vector<int32_t> bufferedOutputCols = {0};
    BufferedTableWithExprOperatorFactoryV3 *bufferedWithExprOperatorFactory =
        BufferedTableWithExprOperatorFactoryV3::CreateBufferedTableWithExprOperatorFactory(
            bufferedTblTypes, bufferedEqualKeyExprs, bufferedOutputCols, streamedWithExprOperatorFactory,
            operatorConfig);
    omniruntime::op::Operator *bufferedTblWithExprOperator = CreateTestOperator(bufferedWithExprOperatorFactory);

    // construct data
    const int32_t streamedTblDataSize = 6;
    int32_t streamedTblDataCol1[streamedTblDataSize] = {1, 2, 3, 4, 5, 6};
    long streamedTblDataCol2[streamedTblDataSize] = {1100, 2200, 3300, 4400, 5500, 6600};
    VectorBatch *streamedTblVecBatch1 =
        CreateVectorBatch(streamedTblTypes, streamedTblDataSize, streamedTblDataCol1, streamedTblDataCol2);

    streamedTblWithExprOperator->AddInput(streamedTblVecBatch1);

    // add eof flag to buffered table , need add streamed table data
    VectorBatch *bufferedTblVecBatchEof = CreateEmptyVectorBatch(bufferedTblTypes);
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchEof);

    // add eof flag to streamed table
    VectorBatch *streamedTblVecBatchEof = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof);
    VectorBatch *streamedTblVecBatchEof1 = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof1);

    VectorBatch *result;
    bufferedTblWithExprOperator->GetOutput(&result);

    // check the join result
    long resultCol1[] = {1100, 2200, 3300, 4400, 5500, 6600};
    double resultCol2[] = {0, 0, 0, 0, 0, 0};
    AssertVecBatchEquals(result, 2, sizeof(resultCol1) / sizeof(resultCol1[0]), resultCol1, resultCol2);
    for (uint32_t i = 0; i < sizeof(resultCol1) / sizeof(resultCol1[0]); i++) {
        EXPECT_TRUE(result->Get(1)->IsNull(i));
    }
    VectorHelper::FreeVecBatch(result);

    Expr::DeleteExprs(streamedEqualKeyExprs);
    Expr::DeleteExprs(bufferedEqualKeyExprs);
    omniruntime::op::Operator::DeleteOperator(bufferedTblWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(streamedTblWithExprOperator);
    delete bufferedWithExprOperatorFactory;
    delete streamedWithExprOperatorFactory;
}

TEST(SortMergeJoinWithExprV3Test, testSmjFullOuterJoinWithNullFirst)
{
    std::string blank;
    std::vector<DataTypePtr> streamTypeVector = {LongType(), LongType()};
    DataTypes streamedTblTypes(streamTypeVector);
    auto *streamCol0 = new FieldExpr(0, LongType());
    auto *streamCol1 = new FieldExpr(1, LongType());
    std::vector<Expr *> streamedEqualKeyExprs = {streamCol0, streamCol1};
    OperatorConfig operatorConfig;
    std::vector<int32_t> streamedOutputCols = {0, 1};
    StreamedTableWithExprOperatorFactoryV3 *streamedWithExprOperatorFactory =
        StreamedTableWithExprOperatorFactoryV3::CreateStreamedTableWithExprOperatorFactory(
            streamedTblTypes, streamedEqualKeyExprs, streamedOutputCols, JoinType::OMNI_JOIN_TYPE_FULL, blank,
            operatorConfig);
    omniruntime::op::Operator *streamedTblWithExprOperator = CreateTestOperator(streamedWithExprOperatorFactory);

    std::vector<DataTypePtr> bufferTypesVector = {LongType(), LongType()};
    DataTypes bufferedTblTypes(bufferTypesVector);
    auto *bufferCol0 = new FieldExpr(0, LongType());
    auto *bufferCol1 = new FieldExpr(1, LongType());
    std::vector<Expr *> bufferedEqualKeyExprs = {bufferCol0, bufferCol1};
    std::vector<int32_t> bufferedOutputCols = {0, 1};
    BufferedTableWithExprOperatorFactoryV3 *bufferedWithExprOperatorFactory =
        BufferedTableWithExprOperatorFactoryV3::CreateBufferedTableWithExprOperatorFactory(
            bufferedTblTypes, bufferedEqualKeyExprs, bufferedOutputCols, streamedWithExprOperatorFactory,
            operatorConfig);
    omniruntime::op::Operator *bufferedTblWithExprOperator = CreateTestOperator(bufferedWithExprOperatorFactory);

    // construct data
    const int32_t streamedTblDataSize = 10;
    int64_t streamedTblDataCol1[streamedTblDataSize] = {1, 3378, 5439, 9013, 9543, 12572, 15591, 17436, 25272, 30436};
    int64_t streamedTblDataCol2[streamedTblDataSize] = {8042, 8221, 8261, 7067, 7883, 8354, 5861, 6539, 5870, 6907};
    VectorBatch *streamedTblVecBatch1 =
        CreateVectorBatch(streamedTblTypes, streamedTblDataSize, streamedTblDataCol1, streamedTblDataCol2);
    streamedTblVecBatch1->Get(0)->SetNull(0);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatch1);

    const int32_t bufferedTblSize = 10;
    int64_t bufferedTblDataCol1[bufferedTblSize] = {1, 879, 7804, 13206, 14690, 32279, 36620, 41764, 44840, 53836};
    int64_t bufferedTblDataCol2[bufferedTblSize] = {7748, 5444, 5701, 6737, 5381, 6434, 8000, 7231, 7610, 7955};
    VectorBatch *bufferedTblVecBatch1 =
        CreateVectorBatch(bufferedTblTypes, bufferedTblSize, bufferedTblDataCol1, bufferedTblDataCol2);
    bufferedTblVecBatch1->Get(0)->SetNull(0);
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatch1);

    // add eof flag to streamed table
    VectorBatch *streamedTblVecBatchEof = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof);

    // add eof flag to buffered table , need add streamed table data
    VectorBatch *bufferedTblVecBatchEof = CreateEmptyVectorBatch(bufferedTblTypes);
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchEof);
    auto *streamedTblVecBatchEof1 = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof1);

    VectorBatch *result;
    bufferedTblWithExprOperator->GetOutput(&result);

    // check the join result
    int64_t resultCol1[] = {-1, -1,    -1,    3378,  5439,  -1, 9013, 9543, 12572, -1,
                            -1, 15591, 17436, 25272, 30436, -1, -1,   -1,   -1,    -1};
    int64_t resultCol2[] = {8042, -1,   -1,   8221, 8261, -1, 7067, 7883, 8354, -1,
                            -1,   5861, 6539, 5870, 6907, -1, -1,   -1,   -1,   -1};
    int64_t resultCol3[] = {-1,    -1, 879, -1, -1, 7804,  -1,    -1,    -1,    13206,
                            14690, -1, -1,  -1, -1, 32279, 36620, 41764, 44840, 53836};
    int64_t resultCol4[] = {-1,   7748, 5444, -1, -1, 5701, -1,   -1,   -1,   6737,
                            5381, -1,   -1,   -1, -1, 6434, 8000, 7231, 7610, 7955};
    std::vector<DataTypePtr> resultTypesVec = {LongType(), LongType(), LongType(), LongType()};
    DataTypes resultTypes(resultTypesVec);
    VectorBatch *expectVecBatch = CreateVectorBatch(resultTypes, 20, resultCol1, resultCol2, resultCol3, resultCol4);
    for (int32_t colIdx = 0; colIdx < expectVecBatch->GetVectorCount(); colIdx++) {
        auto vector = reinterpret_cast<Vector<int64_t> *>(expectVecBatch->Get(colIdx));
        for (int32_t rowIdx = 0; rowIdx < expectVecBatch->GetRowCount(); rowIdx++) {
            if (vector->GetValue(rowIdx) == -1) {
                vector->SetNull(rowIdx);
            }
        }
    }
    ASSERT_TRUE(VecBatchMatch(result, expectVecBatch));
    VectorHelper::FreeVecBatch(result);
    VectorHelper::FreeVecBatch(expectVecBatch);

    Expr::DeleteExprs(streamedEqualKeyExprs);
    Expr::DeleteExprs(bufferedEqualKeyExprs);
    omniruntime::op::Operator::DeleteOperator(bufferedTblWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(streamedTblWithExprOperator);
    delete bufferedWithExprOperatorFactory;
    delete streamedWithExprOperatorFactory;
}

TEST(SortMergeJoinWithExprV3Test, testSmjFullOuterJoinMissMatchBothSide)
{
    // select t1.b, t2.c from t1 full join t2 where t1.a = t2.d
    // streamedTbl t1:  int a, long b;
    // bufferedTbl t2: double c, int d;
    std::string blank;
    std::vector<DataTypePtr> streamTypeVector = {IntType(), LongType()};
    DataTypes streamedTblTypes(streamTypeVector);
    auto *col0 = new FieldExpr(0, IntType());
    std::vector<Expr *> streamedEqualKeyExprs = {col0};
    OperatorConfig operatorConfig;
    std::vector<int32_t> streamedOutputCols = {1};
    StreamedTableWithExprOperatorFactoryV3 *streamedWithExprOperatorFactory =
        StreamedTableWithExprOperatorFactoryV3::CreateStreamedTableWithExprOperatorFactory(
            streamedTblTypes, streamedEqualKeyExprs, streamedOutputCols, JoinType::OMNI_JOIN_TYPE_FULL, blank,
            operatorConfig);
    omniruntime::op::Operator *streamedTblWithExprOperator = CreateTestOperator(streamedWithExprOperatorFactory);

    std::vector<DataTypePtr> bufferTypesVector = {DoubleType(), IntType()};
    DataTypes bufferedTblTypes(bufferTypesVector);
    auto *col1 = new FieldExpr(1, IntType());
    std::vector<Expr *> bufferedEqualKeyExprs = {col1};
    std::vector<int32_t> bufferedOutputCols = {0};
    BufferedTableWithExprOperatorFactoryV3 *bufferedWithExprOperatorFactory =
        BufferedTableWithExprOperatorFactoryV3::CreateBufferedTableWithExprOperatorFactory(
            bufferedTblTypes, bufferedEqualKeyExprs, bufferedOutputCols, streamedWithExprOperatorFactory,
            operatorConfig);
    omniruntime::op::Operator *bufferedTblWithExprOperator = CreateTestOperator(bufferedWithExprOperatorFactory);

    // construct data
    const int32_t streamedTblDataSize = 6;
    int32_t streamedTblDataCol1[streamedTblDataSize] = {1, 2, 3, 4, 5, 7};
    long streamedTblDataCol2[streamedTblDataSize] = {1100, 2200, 3300, 4400, 5500, 7700};
    VectorBatch *streamedTblVecBatch1 =
        CreateVectorBatch(streamedTblTypes, streamedTblDataSize, streamedTblDataCol1, streamedTblDataCol2);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatch1);

    const int32_t bufferedTblSize = 7;
    double bufferedTblDataCol1[bufferedTblSize] = {1.1, 2.2, 3.3, 4.4, 6.6, 8.8, 9.9};
    int32_t bufferedTblDataCol2[bufferedTblSize] = {1, 2, 3, 4, 6, 8, 9};
    VectorBatch *bufferedTblVecBatch1 =
        CreateVectorBatch(bufferedTblTypes, bufferedTblSize, bufferedTblDataCol1, bufferedTblDataCol2);
    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatch1);

    // add eof flag to streamed table
    VectorBatch *streamedTblVecBatchEof = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof);

    // add eof flag to buffered table , need add streamed table data
    VectorBatch *bufferedTblVecBatchEof = CreateEmptyVectorBatch(bufferedTblTypes);
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchEof);
    auto *streamedTblVecBatchEof1 = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof1);

    VectorBatch *result;
    bufferedTblWithExprOperator->GetOutput(&result);

    // check the join result
    long resultCol1[] = {1100, 2200, 3300, 4400, 5500, 0, 7700, 0, 0};
    double resultCol2[] = {1.1, 2.2, 3.3, 4.4, 0, 6.6, 0, 8.8, 9.9};
    AssertVecBatchEquals(result, 2, sizeof(resultCol1) / sizeof(resultCol1[0]), resultCol1, resultCol2);
    // todo:compare null
    VectorHelper::FreeVecBatch(result);

    Expr::DeleteExprs(streamedEqualKeyExprs);
    Expr::DeleteExprs(bufferedEqualKeyExprs);
    omniruntime::op::Operator::DeleteOperator(bufferedTblWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(streamedTblWithExprOperator);
    delete bufferedWithExprOperatorFactory;
    delete streamedWithExprOperatorFactory;
}

TEST(SortMergeJoinWithExprV3Test, testSmjFullJoinWithMultiColumBatch)
{
    // select t1.b, t2.c from t1 left join t2 where t1.a = t2.d
    // streamedTbl t1:  int a, long b;
    // bufferedTbl t2: double c, int d;
    std::string blank;
    std::vector<DataTypePtr> streamTypeVector = {IntType(), LongType()};
    DataTypes streamedTblTypes(streamTypeVector);
    auto *col0 = new FieldExpr(0, IntType());
    std::vector<Expr *> streamedEqualKeyExprs = {col0};
    OperatorConfig operatorConfig;
    std::vector<int32_t> streamedOutputCols = {1};
    StreamedTableWithExprOperatorFactoryV3 *streamedWithExprOperatorFactory =
        StreamedTableWithExprOperatorFactoryV3::CreateStreamedTableWithExprOperatorFactory(
            streamedTblTypes, streamedEqualKeyExprs, streamedOutputCols, JoinType::OMNI_JOIN_TYPE_FULL, blank,
            operatorConfig);
    omniruntime::op::Operator *streamedTblWithExprOperator = CreateTestOperator(streamedWithExprOperatorFactory);

    std::vector<DataTypePtr> bufferTypesVector = {DoubleType(), IntType()};
    DataTypes bufferedTblTypes(bufferTypesVector);
    auto *col1 = new FieldExpr(1, IntType());
    std::vector<Expr *> bufferedEqualKeyExprs = {col1};
    std::vector<int32_t> bufferedOutputCols = {0};
    BufferedTableWithExprOperatorFactoryV3 *bufferedWithExprOperatorFactory =
        BufferedTableWithExprOperatorFactoryV3::CreateBufferedTableWithExprOperatorFactory(
            bufferedTblTypes, bufferedEqualKeyExprs, bufferedOutputCols, streamedWithExprOperatorFactory,
            operatorConfig);
    omniruntime::op::Operator *bufferedTblWithExprOperator = CreateTestOperator(bufferedWithExprOperatorFactory);

    int32_t streamedCol1Row1[1] = {1};
    long streamedCol2Row1[1] = {1100};
    int32_t streamedCol1Row2[1] = {2};
    long streamedCol2Row2[1] = {2200};
    int32_t streamedCol1Row3[1] = {3};
    long streamedCol2Row3[1] = {3300};
    int32_t streamedCol1Row4[1] = {4};
    long streamedCol2Row4[1] = {4400};
    int32_t streamedCol1Row5[1] = {5};
    long streamedCol2Row5[1] = {5500};
    int32_t streamedCol1Row6[1] = {7};
    long streamedCol2Row6[1] = {7700};
    // construct streamed data
    VectorBatch *streamedTblVecBatchRow1 = CreateVectorBatch(streamedTblTypes, 1, streamedCol1Row1, streamedCol2Row1);
    VectorBatch *streamedTblVecBatchRow2 = CreateVectorBatch(streamedTblTypes, 1, streamedCol1Row2, streamedCol2Row2);
    VectorBatch *streamedTblVecBatchRow3 = CreateVectorBatch(streamedTblTypes, 1, streamedCol1Row3, streamedCol2Row3);
    VectorBatch *streamedTblVecBatchRow4 = CreateVectorBatch(streamedTblTypes, 1, streamedCol1Row4, streamedCol2Row4);
    VectorBatch *streamedTblVecBatchRow5 = CreateVectorBatch(streamedTblTypes, 1, streamedCol1Row5, streamedCol2Row5);
    VectorBatch *streamedTblVecBatchRow6 = CreateVectorBatch(streamedTblTypes, 1, streamedCol1Row6, streamedCol2Row6);

    double bufferedCol1Row1[1] = {1.1};
    int32_t bufferedCol2Row1[1] = {1};
    double bufferedCol1Row2[1] = {2.2};
    int32_t bufferedCol2Row2[1] = {2};
    double bufferedCol1Row3[1] = {3.3};
    int32_t bufferedCol2Row3[1] = {3};
    double bufferedCol1Row4[1] = {4.4};
    int32_t bufferedCol2Row4[1] = {4};
    double bufferedCol1Row5[1] = {6.6};
    int32_t bufferedCol2Row5[1] = {6};
    double bufferedCol1Row6[1] = {8.8};
    int32_t bufferedCol2Row6[1] = {8};
    // construct buffered data
    VectorBatch *bufferedTblVecBatchRow1 = CreateVectorBatch(bufferedTblTypes, 1, bufferedCol1Row1, bufferedCol2Row1);
    VectorBatch *bufferedTblVecBatchRow2 = CreateVectorBatch(bufferedTblTypes, 1, bufferedCol1Row2, bufferedCol2Row2);
    VectorBatch *bufferedTblVecBatchRow3 = CreateVectorBatch(bufferedTblTypes, 1, bufferedCol1Row3, bufferedCol2Row3);
    VectorBatch *bufferedTblVecBatchRow4 = CreateVectorBatch(bufferedTblTypes, 1, bufferedCol1Row4, bufferedCol2Row4);
    VectorBatch *bufferedTblVecBatchRow5 = CreateVectorBatch(bufferedTblTypes, 1, bufferedCol1Row5, bufferedCol2Row5);
    VectorBatch *bufferedTblVecBatchRow6 = CreateVectorBatch(bufferedTblTypes, 1, bufferedCol1Row6, bufferedCol2Row6);

    // join start add streamed table data
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchRow1);

    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchRow1);

    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchRow2);

    // need add streamed table data
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchRow2);

    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchRow3);

    // need add streamed table data
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchRow3);

    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchRow4);

    // need add streamed table data
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchRow4);

    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchRow5);

    // need add streamed table data
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchRow5);

    // need add streamed table data
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchRow6);

    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchRow6);

    // add eof flag to streamed table
    VectorBatch *streamedTblVecBatchEof = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof);

    // add eof flag to buffered table , need add streamed table data
    VectorBatch *bufferedTblVecBatchEof = CreateEmptyVectorBatch(bufferedTblTypes);
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchEof);
    VectorBatch *streamedTblVecBatchEof1 = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof1);

    VectorBatch *result;
    bufferedTblWithExprOperator->GetOutput(&result);

    // check the join result
    long resultCol1[] = {1100, 2200, 3300, 4400, 5500, 0, 7700, 0};
    double resultCol2[] = {1.1, 2.2, 3.3, 4.4, 0, 6.6, 0, 8.8};
    AssertVecBatchEquals(result, 2, sizeof(resultCol1) / sizeof(resultCol1[0]), resultCol1, resultCol2);
    // todo:compare null
    VectorHelper::FreeVecBatch(result);

    Expr::DeleteExprs(streamedEqualKeyExprs);
    Expr::DeleteExprs(bufferedEqualKeyExprs);
    omniruntime::op::Operator::DeleteOperator(bufferedTblWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(streamedTblWithExprOperator);
    delete bufferedWithExprOperatorFactory;
    delete streamedWithExprOperatorFactory;
}

TEST(SortMergeJoinWithExprV3Test, testSmjFullOuterJoinMatchBothSide)
{
    // select t1.b, t2.c from t1 full join t2 where t1.a = t2.d
    // streamedTbl t1:  int a, long b;
    // bufferedTbl t2: double c, int d;
    std::string blank;
    std::vector<DataTypePtr> streamTypeVector = {IntType(), LongType()};
    DataTypes streamedTblTypes(streamTypeVector);
    auto *col0 = new FieldExpr(0, IntType());
    std::vector<Expr *> streamedEqualKeyExprs = {col0};
    OperatorConfig operatorConfig;
    std::vector<int32_t> streamedOutputCols = {1};
    StreamedTableWithExprOperatorFactoryV3 *streamedWithExprOperatorFactory =
        StreamedTableWithExprOperatorFactoryV3::CreateStreamedTableWithExprOperatorFactory(
            streamedTblTypes, streamedEqualKeyExprs, streamedOutputCols, JoinType::OMNI_JOIN_TYPE_FULL, blank,
            operatorConfig);
    omniruntime::op::Operator *streamedTblWithExprOperator = CreateTestOperator(streamedWithExprOperatorFactory);

    std::vector<DataTypePtr> bufferTypesVector = {DoubleType(), IntType()};
    DataTypes bufferedTblTypes(bufferTypesVector);
    auto *col1 = new FieldExpr(1, IntType());
    std::vector<Expr *> bufferedEqualKeyExprs = {col1};
    std::vector<int32_t> bufferedOutputCols = {0};
    BufferedTableWithExprOperatorFactoryV3 *bufferedWithExprOperatorFactory =
        BufferedTableWithExprOperatorFactoryV3::CreateBufferedTableWithExprOperatorFactory(
            bufferedTblTypes, bufferedEqualKeyExprs, bufferedOutputCols, streamedWithExprOperatorFactory,
            operatorConfig);
    omniruntime::op::Operator *bufferedTblWithExprOperator = CreateTestOperator(bufferedWithExprOperatorFactory);

    // construct data
    const int32_t streamedTblDataSize = 6;
    int32_t streamedTblDataCol1[streamedTblDataSize] = {1, 2, 3, 4, 5, 6};
    long streamedTblDataCol2[streamedTblDataSize] = {1100, 2200, 3300, 4400, 5500, 6600};
    VectorBatch *streamedTblVecBatch1 =
        CreateVectorBatch(streamedTblTypes, streamedTblDataSize, streamedTblDataCol1, streamedTblDataCol2);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatch1);

    const int32_t bufferedTblSize = 6;
    double bufferedTblDataCol1[bufferedTblSize] = {1.1, 2.2, 3.3, 4.4, 5.5, 6.6};
    int32_t bufferedTblDataCol2[bufferedTblSize] = {1, 2, 3, 4, 5, 6};
    VectorBatch *bufferedTblVecBatch1 =
        CreateVectorBatch(bufferedTblTypes, bufferedTblSize, bufferedTblDataCol1, bufferedTblDataCol2);
    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatch1);

    // add eof flag to buffered table , need add streamed table data
    VectorBatch *bufferedTblVecBatchEof = CreateEmptyVectorBatch(bufferedTblTypes);
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchEof);

    // add eof flag to streamed table
    VectorBatch *streamedTblVecBatchEof = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof);
    auto *streamedTblVecBatchEof1 = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof1);

    VectorBatch *result;
    bufferedTblWithExprOperator->GetOutput(&result);

    // check the join result
    long resultCol1[] = {1100, 2200, 3300, 4400, 5500, 6600};
    double resultCol2[] = {1.1, 2.2, 3.3, 4.4, 5.5, 6.6};
    AssertVecBatchEquals(result, 2, sizeof(resultCol1) / sizeof(resultCol1[0]), resultCol1, resultCol2);
    // todo:compare null
    VectorHelper::FreeVecBatch(result);

    Expr::DeleteExprs(streamedEqualKeyExprs);
    Expr::DeleteExprs(bufferedEqualKeyExprs);
    omniruntime::op::Operator::DeleteOperator(bufferedTblWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(streamedTblWithExprOperator);
    delete bufferedWithExprOperatorFactory;
    delete streamedWithExprOperatorFactory;
}

TEST(SortMergeJoinWithExprV3Test, testSmjFullOuterJoinMissMatchWithNullJoinkKeyFirst)
{
    // select t1.b, t2.c from t1 full join t2 where t1.a = t2.d
    // streamedTbl t1:  int a, long b;
    // bufferedTbl t2: double c, int d;
    std::string blank;
    std::vector<DataTypePtr> streamTypeVector = {IntType(), LongType()};
    DataTypes streamedTblTypes(streamTypeVector);
    auto *col0 = new FieldExpr(0, IntType());
    std::vector<Expr *> streamedEqualKeyExprs = {col0};
    OperatorConfig operatorConfig;
    std::vector<int32_t> streamedOutputCols = {1};
    StreamedTableWithExprOperatorFactoryV3 *streamedWithExprOperatorFactory =
        StreamedTableWithExprOperatorFactoryV3::CreateStreamedTableWithExprOperatorFactory(
            streamedTblTypes, streamedEqualKeyExprs, streamedOutputCols, JoinType::OMNI_JOIN_TYPE_FULL, blank,
            operatorConfig);
    omniruntime::op::Operator *streamedTblWithExprOperator = CreateTestOperator(streamedWithExprOperatorFactory);

    std::vector<DataTypePtr> bufferTypesVector = {DoubleType(), IntType()};
    DataTypes bufferedTblTypes(bufferTypesVector);
    auto *col1 = new FieldExpr(1, IntType());
    std::vector<Expr *> bufferedEqualKeyExprs = {col1};
    std::vector<int32_t> bufferedOutputCols = {0};
    BufferedTableWithExprOperatorFactoryV3 *bufferedWithExprOperatorFactory =
        BufferedTableWithExprOperatorFactoryV3::CreateBufferedTableWithExprOperatorFactory(
            bufferedTblTypes, bufferedEqualKeyExprs, bufferedOutputCols, streamedWithExprOperatorFactory,
            operatorConfig);
    omniruntime::op::Operator *bufferedTblWithExprOperator = CreateTestOperator(bufferedWithExprOperatorFactory);

    // construct data
    const int32_t streamedTblDataSize = 6;
    int32_t streamedTblDataCol1[streamedTblDataSize] = {1, 2, 3, 4, 7, 9};
    long streamedTblDataCol2[streamedTblDataSize] = {1100, 2200, 3300, 4400, 7700, 9900};
    VectorBatch *streamedTblVecBatch1 =
        CreateVectorBatch(streamedTblTypes, streamedTblDataSize, streamedTblDataCol1, streamedTblDataCol2);
    streamedTblVecBatch1->Get(0)->SetNull(0); // NULL, 2, 4, 5, 7, 9
    streamedTblWithExprOperator->AddInput(streamedTblVecBatch1);

    const int32_t bufferedTblSize = 6;
    double bufferedTblDataCol1[bufferedTblSize] = {1.1, 2.2, 3.3, 4.4, 6.6, 8.8};
    int32_t bufferedTblDataCol2[bufferedTblSize] = {1, 2, 3, 4, 6, 8};
    VectorBatch *bufferedTblVecBatch1 =
        CreateVectorBatch(bufferedTblTypes, bufferedTblSize, bufferedTblDataCol1, bufferedTblDataCol2);
    bufferedTblVecBatch1->Get(1)->SetNull(0); // NULL, 2, 3, 4, 6, 8

    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatch1);

    // add eof flag to buffered table , need add streamed table data
    VectorBatch *bufferedTblVecBatchEof = CreateEmptyVectorBatch(bufferedTblTypes);
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchEof);

    // add eof flag to streamed table
    VectorBatch *streamedTblVecBatchEof = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof);

    VectorBatch *result;
    bufferedTblWithExprOperator->GetOutput(&result);

    auto *streamedTblVecBatchEof1 = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof1);

    // check the join result
    long resultCol1[] = {1100, 0, 2200, 3300, 4400, 0, 7700, 0, 9900};
    double resultCol2[] = {0, 1.1, 2.2, 3.3, 4.4, 6.6, 0, 8.8, 0};
    AssertVecBatchEquals(result, 2, sizeof(resultCol1) / sizeof(resultCol1[0]), resultCol1, resultCol2);
    // todo:compare null
    VectorHelper::FreeVecBatch(result);

    Expr::DeleteExprs(streamedEqualKeyExprs);
    Expr::DeleteExprs(bufferedEqualKeyExprs);
    omniruntime::op::Operator::DeleteOperator(bufferedTblWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(streamedTblWithExprOperator);
    delete bufferedWithExprOperatorFactory;
    delete streamedWithExprOperatorFactory;
}

TEST(SortMergeJoinWithExprV3Test, testSmjFullOuterJoinMissMatchWith2NullJoinkKeyFirst)
{
    // select t1.b, t2.c from t1 full join t2 where t1.a = t2.d
    // streamedTbl t1:  int a, long b;
    // bufferedTbl t2: double c, int d;
    std::string blank;
    std::vector<DataTypePtr> streamTypeVector = {IntType(), LongType()};
    DataTypes streamedTblTypes(streamTypeVector);
    auto *col0 = new FieldExpr(0, IntType());
    std::vector<Expr *> streamedEqualKeyExprs = {col0};
    OperatorConfig operatorConfig;
    std::vector<int32_t> streamedOutputCols = {1};
    StreamedTableWithExprOperatorFactoryV3 *streamedWithExprOperatorFactory =
        StreamedTableWithExprOperatorFactoryV3::CreateStreamedTableWithExprOperatorFactory(
            streamedTblTypes, streamedEqualKeyExprs, streamedOutputCols, JoinType::OMNI_JOIN_TYPE_FULL, blank,
            operatorConfig);
    omniruntime::op::Operator *streamedTblWithExprOperator = CreateTestOperator(streamedWithExprOperatorFactory);

    std::vector<DataTypePtr> bufferTypesVector = {DoubleType(), IntType()};
    DataTypes bufferedTblTypes(bufferTypesVector);
    auto *col1 = new FieldExpr(1, IntType());
    std::vector<Expr *> bufferedEqualKeyExprs = {col1};
    std::vector<int32_t> bufferedOutputCols = {0};
    BufferedTableWithExprOperatorFactoryV3 *bufferedWithExprOperatorFactory =
        BufferedTableWithExprOperatorFactoryV3::CreateBufferedTableWithExprOperatorFactory(
            bufferedTblTypes, bufferedEqualKeyExprs, bufferedOutputCols, streamedWithExprOperatorFactory,
            operatorConfig);
    omniruntime::op::Operator *bufferedTblWithExprOperator = CreateTestOperator(bufferedWithExprOperatorFactory);

    // construct data
    const int32_t streamedTblDataSize = 6;
    int32_t streamedTblDataCol1[streamedTblDataSize] = {1, 2, 3, 4, 5, 6};
    long streamedTblDataCol2[streamedTblDataSize] = {1100, 2200, 3300, 4400, 5500, 6600};
    VectorBatch *streamedTblVecBatch1 =
        CreateVectorBatch(streamedTblTypes, streamedTblDataSize, streamedTblDataCol1, streamedTblDataCol2);
    streamedTblVecBatch1->Get(0)->SetNull(0); // NULL, NULL, 3, 4, 5, 6
    streamedTblVecBatch1->Get(0)->SetNull(1); // NULL, NULL, 3, 4, 5, 6
    streamedTblWithExprOperator->AddInput(streamedTblVecBatch1);

    const int32_t bufferedTblSize = 6;
    double bufferedTblDataCol1[bufferedTblSize] = {1.1, 2.2, 3.3, 4.4, 5.5, 6.6};
    int32_t bufferedTblDataCol2[bufferedTblSize] = {1, 2, 3, 4, 5, 6};
    VectorBatch *bufferedTblVecBatch1 =
        CreateVectorBatch(bufferedTblTypes, bufferedTblSize, bufferedTblDataCol1, bufferedTblDataCol2);
    bufferedTblVecBatch1->Get(1)->SetNull(0); // NULL, NULL, 3, 4, 5, 6
    bufferedTblVecBatch1->Get(1)->SetNull(1); // NULL, NULL, 3, 4, 5, 6

    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatch1);

    // add eof flag to buffered table , need add streamed table data
    VectorBatch *bufferedTblVecBatchEof = CreateEmptyVectorBatch(bufferedTblTypes);
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchEof);

    // add eof flag to streamed table
    VectorBatch *streamedTblVecBatchEof = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof);
    auto *streamedTblVecBatchEof1 = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof1);

    VectorBatch *result;
    bufferedTblWithExprOperator->GetOutput(&result);

    // check the join result
    long resultCol1[] = {1100, 2200, 0, 0, 3300, 4400, 5500, 6600};
    double resultCol2[] = {0, 0, 1.1, 2.2, 3.3, 4.4, 5.5, 6.6};
    AssertVecBatchEquals(result, 2, sizeof(resultCol1) / sizeof(resultCol1[0]), resultCol1, resultCol2);
    // todo:compare null
    VectorHelper::FreeVecBatch(result);

    Expr::DeleteExprs(streamedEqualKeyExprs);
    Expr::DeleteExprs(bufferedEqualKeyExprs);
    omniruntime::op::Operator::DeleteOperator(bufferedTblWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(streamedTblWithExprOperator);
    delete bufferedWithExprOperatorFactory;
    delete streamedWithExprOperatorFactory;
}

TEST(SortMergeJoinWithExprV3Test, testSmjFullOuterJoinkKeyLast)
{
    // select t1.b, t2.c from t1 full join t2 where t1.a = t2.d
    // streamedTbl t1:  int a, long b;
    // bufferedTbl t2: double c, int d;
    std::string blank;
    std::vector<DataTypePtr> streamTypeVector = {IntType(), LongType()};
    DataTypes streamedTblTypes(streamTypeVector);
    auto *col0 = new FieldExpr(0, IntType());
    std::vector<Expr *> streamedEqualKeyExprs = {col0};
    OperatorConfig operatorConfig;
    std::vector<int32_t> streamedOutputCols = {1};
    StreamedTableWithExprOperatorFactoryV3 *streamedWithExprOperatorFactory =
        StreamedTableWithExprOperatorFactoryV3::CreateStreamedTableWithExprOperatorFactory(
            streamedTblTypes, streamedEqualKeyExprs, streamedOutputCols, JoinType::OMNI_JOIN_TYPE_FULL, blank,
            operatorConfig);
    omniruntime::op::Operator *streamedTblWithExprOperator = CreateTestOperator(streamedWithExprOperatorFactory);

    std::vector<DataTypePtr> bufferTypesVector = {DoubleType(), IntType()};
    DataTypes bufferedTblTypes(bufferTypesVector);
    auto *col1 = new FieldExpr(1, IntType());
    std::vector<Expr *> bufferedEqualKeyExprs = {col1};
    std::vector<int32_t> bufferedOutputCols = {0};
    BufferedTableWithExprOperatorFactoryV3 *bufferedWithExprOperatorFactory =
        BufferedTableWithExprOperatorFactoryV3::CreateBufferedTableWithExprOperatorFactory(
            bufferedTblTypes, bufferedEqualKeyExprs, bufferedOutputCols, streamedWithExprOperatorFactory,
            operatorConfig);
    omniruntime::op::Operator *bufferedTblWithExprOperator = CreateTestOperator(bufferedWithExprOperatorFactory);

    // construct data
    const int32_t streamedTblDataSize = 5;
    int32_t streamedTblDataCol1[streamedTblDataSize] = {1, 2, 3, 4, 5};
    long streamedTblDataCol2[streamedTblDataSize] = {1100, 2200, 3300, 4400, 5500};
    VectorBatch *streamedTblVecBatch1 =
        CreateVectorBatch(streamedTblTypes, streamedTblDataSize, streamedTblDataCol1, streamedTblDataCol2);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatch1);

    const int32_t bufferedTblSize = 5;
    double bufferedTblDataCol1[bufferedTblSize] = {1.1, 2.2, 3.3, 4.4, 8.8};
    int32_t bufferedTblDataCol2[bufferedTblSize] = {1, 2, 3, 4, 8};
    VectorBatch *bufferedTblVecBatch1 =
        CreateVectorBatch(bufferedTblTypes, bufferedTblSize, bufferedTblDataCol1, bufferedTblDataCol2);

    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatch1);

    // add eof flag to streamed table
    VectorBatch *streamedTblVecBatchEof = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof);

    // add eof flag to buffered table , need add streamed table data
    VectorBatch *bufferedTblVecBatchEof = CreateEmptyVectorBatch(bufferedTblTypes);
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchEof);
    auto *streamedTblVecBatchEof1 = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof1);

    VectorBatch *result;
    bufferedTblWithExprOperator->GetOutput(&result);

    // check the join result
    long resultCol1[] = {1100, 2200, 3300, 4400, 5500, 0};
    double resultCol2[] = {1.1, 2.2, 3.3, 4.4, 0, 8.8};
    AssertVecBatchEquals(result, 2, sizeof(resultCol1) / sizeof(resultCol1[0]), resultCol1, resultCol2);
    // todo:compare null
    VectorHelper::FreeVecBatch(result);

    Expr::DeleteExprs(streamedEqualKeyExprs);
    Expr::DeleteExprs(bufferedEqualKeyExprs);
    omniruntime::op::Operator::DeleteOperator(bufferedTblWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(streamedTblWithExprOperator);
    delete bufferedWithExprOperatorFactory;
    delete streamedWithExprOperatorFactory;
}

TEST(SortMergeJoinWithExprV3Test, testSmjFullJoinNullValuesWithMultiColumBatch)
{
    // select t1.b, t2.c from t1 left join t2 where t1.a = t2.d
    // streamedTbl t1:  int a, long b;
    // bufferedTbl t2: double c, int d;
    std::string blank;
    std::vector<DataTypePtr> streamTypeVector = {IntType(), LongType()};
    DataTypes streamedTblTypes(streamTypeVector);
    auto *col0 = new FieldExpr(0, IntType());
    std::vector<Expr *> streamedEqualKeyExprs = {col0};
    OperatorConfig operatorConfig;
    std::vector<int32_t> streamedOutputCols = {1};
    StreamedTableWithExprOperatorFactoryV3 *streamedWithExprOperatorFactory =
        StreamedTableWithExprOperatorFactoryV3::CreateStreamedTableWithExprOperatorFactory(
            streamedTblTypes, streamedEqualKeyExprs, streamedOutputCols, JoinType::OMNI_JOIN_TYPE_FULL, blank,
            operatorConfig);
    omniruntime::op::Operator *streamedTblWithExprOperator = CreateTestOperator(streamedWithExprOperatorFactory);

    std::vector<DataTypePtr> bufferTypesVector = {DoubleType(), IntType()};
    DataTypes bufferedTblTypes(bufferTypesVector);
    auto *col1 = new FieldExpr(1, IntType());
    std::vector<Expr *> bufferedEqualKeyExprs = {col1};
    std::vector<int32_t> bufferedOutputCols = {0};
    BufferedTableWithExprOperatorFactoryV3 *bufferedWithExprOperatorFactory =
        BufferedTableWithExprOperatorFactoryV3::CreateBufferedTableWithExprOperatorFactory(
            bufferedTblTypes, bufferedEqualKeyExprs, bufferedOutputCols, streamedWithExprOperatorFactory,
            operatorConfig);
    omniruntime::op::Operator *bufferedTblWithExprOperator = CreateTestOperator(bufferedWithExprOperatorFactory);

    int32_t streamedCol1Row1[1] = {1};
    long streamedCol2Row1[1] = {1100};
    int32_t streamedCol1Row2[1] = {2};
    long streamedCol2Row2[1] = {2200};
    int32_t streamedCol1Row3[1] = {3};
    long streamedCol2Row3[1] = {3300};
    int32_t streamedCol1Row4[1] = {4};
    long streamedCol2Row4[1] = {4400};
    int32_t streamedCol1Row5[1] = {5};
    long streamedCol2Row5[1] = {5500};
    int32_t streamedCol1Row6[1] = {7};
    long streamedCol2Row6[1] = {7700};
    // construct streamed data
    VectorBatch *streamedTblVecBatchRow1 = CreateVectorBatch(streamedTblTypes, 1, streamedCol1Row1, streamedCol2Row1);
    VectorBatch *streamedTblVecBatchRow2 = CreateVectorBatch(streamedTblTypes, 1, streamedCol1Row2, streamedCol2Row2);
    VectorBatch *streamedTblVecBatchRow3 = CreateVectorBatch(streamedTblTypes, 1, streamedCol1Row3, streamedCol2Row3);
    VectorBatch *streamedTblVecBatchRow4 = CreateVectorBatch(streamedTblTypes, 1, streamedCol1Row4, streamedCol2Row4);
    VectorBatch *streamedTblVecBatchRow5 = CreateVectorBatch(streamedTblTypes, 1, streamedCol1Row5, streamedCol2Row5);
    VectorBatch *streamedTblVecBatchRow6 = CreateVectorBatch(streamedTblTypes, 1, streamedCol1Row6, streamedCol2Row6);
    streamedTblVecBatchRow1->Get(0)->SetNull(0);
    streamedTblVecBatchRow2->Get(0)->SetNull(0);
    streamedTblVecBatchRow3->Get(0)->SetNull(0);

    double bufferedCol1Row1[1] = {1.1};
    int32_t bufferedCol2Row1[1] = {1};
    double bufferedCol1Row2[1] = {2.2};
    int32_t bufferedCol2Row2[1] = {2};
    double bufferedCol1Row3[1] = {3.3};
    int32_t bufferedCol2Row3[1] = {3};
    double bufferedCol1Row4[1] = {4.4};
    int32_t bufferedCol2Row4[1] = {4};
    double bufferedCol1Row5[1] = {6.6};
    int32_t bufferedCol2Row5[1] = {6};
    double bufferedCol1Row6[1] = {8.8};
    int32_t bufferedCol2Row6[1] = {8};
    // construct buffered data
    VectorBatch *bufferedTblVecBatchRow1 = CreateVectorBatch(bufferedTblTypes, 1, bufferedCol1Row1, bufferedCol2Row1);
    VectorBatch *bufferedTblVecBatchRow2 = CreateVectorBatch(bufferedTblTypes, 1, bufferedCol1Row2, bufferedCol2Row2);
    VectorBatch *bufferedTblVecBatchRow3 = CreateVectorBatch(bufferedTblTypes, 1, bufferedCol1Row3, bufferedCol2Row3);
    VectorBatch *bufferedTblVecBatchRow4 = CreateVectorBatch(bufferedTblTypes, 1, bufferedCol1Row4, bufferedCol2Row4);
    VectorBatch *bufferedTblVecBatchRow5 = CreateVectorBatch(bufferedTblTypes, 1, bufferedCol1Row5, bufferedCol2Row5);
    VectorBatch *bufferedTblVecBatchRow6 = CreateVectorBatch(bufferedTblTypes, 1, bufferedCol1Row6, bufferedCol2Row6);
    bufferedTblVecBatchRow1->Get(1)->SetNull(0);
    bufferedTblVecBatchRow2->Get(1)->SetNull(0);
    bufferedTblVecBatchRow3->Get(1)->SetNull(0);

    // join start add streamed table data
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchRow1);

    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchRow1);

    // need add streamed table data
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchRow2);

    // need add streamed table data
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchRow3);

    // need add streamed table data
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchRow4);

    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchRow2);

    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchRow3);

    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchRow4);

    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchRow5);

    // need add streamed table data
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchRow5);

    // need add streamed table data
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchRow6);

    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchRow6);

    // add eof flag to streamed table
    VectorBatch *streamedTblVecBatchEof = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof);

    // add eof flag to buffered table , need add streamed table data
    VectorBatch *bufferedTblVecBatchEof = CreateEmptyVectorBatch(bufferedTblTypes);
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchEof);
    // add eof flag to streamed table
    VectorBatch *streamedTblVecBatchEof1 = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof1);

    VectorBatch *result;
    bufferedTblWithExprOperator->GetOutput(&result);

    // check the join result
    long resultCol1[] = {1100, 2200, 3300, 0, 0, 0, 4400, 5500, 0, 7700, 0};
    double resultCol2[] = {0, 0, 0, 1.1, 2.2, 3.3, 4.4, 0, 6.6, 0, 8.8};
    AssertVecBatchEquals(result, 2, sizeof(resultCol1) / sizeof(resultCol1[0]), resultCol1, resultCol2);
    // todo:compare null
    VectorHelper::FreeVecBatch(result);

    Expr::DeleteExprs(streamedEqualKeyExprs);
    Expr::DeleteExprs(bufferedEqualKeyExprs);
    omniruntime::op::Operator::DeleteOperator(bufferedTblWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(streamedTblWithExprOperator);
    delete bufferedWithExprOperatorFactory;
    delete streamedWithExprOperatorFactory;
}

TEST(SortMergeJoinWithExprV3Test, testSmjFullOuterJoinStreamedWithEmptyBuffered)
{
    // select t1.b, t2.c from t1 left join t2 where t1.a = t2.d
    // streamedTbl t1:  int a, long b;
    // bufferedTbl t2: double c, int d;
    std::string blank;
    std::vector<DataTypePtr> streamTypeVector = {IntType(), LongType()};
    DataTypes streamedTblTypes(streamTypeVector);
    auto *col0 = new FieldExpr(0, IntType());
    std::vector<Expr *> streamedEqualKeyExprs = {col0};
    OperatorConfig operatorConfig;
    std::vector<int32_t> streamedOutputCols = {1};
    StreamedTableWithExprOperatorFactoryV3 *streamedWithExprOperatorFactory =
        StreamedTableWithExprOperatorFactoryV3::CreateStreamedTableWithExprOperatorFactory(
            streamedTblTypes, streamedEqualKeyExprs, streamedOutputCols, JoinType::OMNI_JOIN_TYPE_FULL, blank,
            operatorConfig);
    omniruntime::op::Operator *streamedTblWithExprOperator = CreateTestOperator(streamedWithExprOperatorFactory);

    std::vector<DataTypePtr> bufferTypesVector = {DoubleType(), IntType()};
    DataTypes bufferedTblTypes(bufferTypesVector);
    auto *col1 = new FieldExpr(1, IntType());
    std::vector<Expr *> bufferedEqualKeyExprs = {col1};
    std::vector<int32_t> bufferedOutputCols = {0};
    BufferedTableWithExprOperatorFactoryV3 *bufferedWithExprOperatorFactory =
        BufferedTableWithExprOperatorFactoryV3::CreateBufferedTableWithExprOperatorFactory(
            bufferedTblTypes, bufferedEqualKeyExprs, bufferedOutputCols, streamedWithExprOperatorFactory,
            operatorConfig);
    omniruntime::op::Operator *bufferedTblWithExprOperator = CreateTestOperator(bufferedWithExprOperatorFactory);

    // construct data
    const int32_t streamedTblDataSize = 6;
    int32_t streamedTblDataCol1[streamedTblDataSize] = {1, 2, 3, 4, 5, 6};
    long streamedTblDataCol2[streamedTblDataSize] = {1100, 2200, 3300, 4400, 5500, 6600};
    VectorBatch *streamedTblVecBatch1 =
        CreateVectorBatch(streamedTblTypes, streamedTblDataSize, streamedTblDataCol1, streamedTblDataCol2);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatch1);

    // add eof flag to buffered table , need add streamed table data
    VectorBatch *bufferedTblVecBatchEof = CreateEmptyVectorBatch(bufferedTblTypes);
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchEof);

    // add eof flag to streamed table
    VectorBatch *streamedTblVecBatchEof = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof);

    // add eof flag to streamed table
    VectorBatch *streamedTblVecBatchEof1 = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof1);

    VectorBatch *result;
    bufferedTblWithExprOperator->GetOutput(&result);

    // check the join result
    long resultCol1[] = {1100, 2200, 3300, 4400, 5500, 6600};
    double resultCol2[] = {0, 0, 0, 0, 0, 0};
    AssertVecBatchEquals(result, 2, sizeof(resultCol1) / sizeof(resultCol1[0]), resultCol1, resultCol2);
    // todo:compare null
    VectorHelper::FreeVecBatch(result);

    Expr::DeleteExprs(streamedEqualKeyExprs);
    Expr::DeleteExprs(bufferedEqualKeyExprs);
    omniruntime::op::Operator::DeleteOperator(bufferedTblWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(streamedTblWithExprOperator);
    delete bufferedWithExprOperatorFactory;
    delete streamedWithExprOperatorFactory;
}

TEST(SortMergeJoinWithExprV3Test, testSmjFullOuterJoinEmptyStreamedWithBuffered)
{
    // select t1.b, t2.c from t1 left join t2 where t1.a = t2.d
    // streamedTbl t1:  int a, long b;
    // bufferedTbl t2: double c, int d;
    std::string blank;
    std::vector<DataTypePtr> streamTypeVector = {IntType(), LongType()};
    DataTypes streamedTblTypes(streamTypeVector);
    auto *col0 = new FieldExpr(0, IntType());
    std::vector<Expr *> streamedEqualKeyExprs = {col0};
    OperatorConfig operatorConfig;
    std::vector<int32_t> streamedOutputCols = {1};
    StreamedTableWithExprOperatorFactoryV3 *streamedWithExprOperatorFactory =
        StreamedTableWithExprOperatorFactoryV3::CreateStreamedTableWithExprOperatorFactory(
            streamedTblTypes, streamedEqualKeyExprs, streamedOutputCols, JoinType::OMNI_JOIN_TYPE_FULL, blank,
            operatorConfig);
    omniruntime::op::Operator *streamedTblWithExprOperator = CreateTestOperator(streamedWithExprOperatorFactory);

    std::vector<DataTypePtr> bufferTypesVector = {DoubleType(), IntType()};
    DataTypes bufferedTblTypes(bufferTypesVector);
    auto *col1 = new FieldExpr(1, IntType());
    std::vector<Expr *> bufferedEqualKeyExprs = {col1};
    std::vector<int32_t> bufferedOutputCols = {0};
    BufferedTableWithExprOperatorFactoryV3 *bufferedWithExprOperatorFactory =
        BufferedTableWithExprOperatorFactoryV3::CreateBufferedTableWithExprOperatorFactory(
            bufferedTblTypes, bufferedEqualKeyExprs, bufferedOutputCols, streamedWithExprOperatorFactory,
            operatorConfig);
    omniruntime::op::Operator *bufferedTblWithExprOperator = CreateTestOperator(bufferedWithExprOperatorFactory);

    // construct data
    const int32_t bufferedTblSize = 6;
    double bufferedTblDataCol1[bufferedTblSize] = {1.1, 2.2, 3.3, 4.4, 5.5, 6.6};
    int32_t bufferedTblDataCol2[bufferedTblSize] = {1, 2, 3, 4, 5, 6};
    VectorBatch *bufferedTblVecBatch1 =
        CreateVectorBatch(bufferedTblTypes, bufferedTblSize, bufferedTblDataCol1, bufferedTblDataCol2);

    // need add streamed table data
    VectorBatch *streamedTblEmptyVecBatch = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblEmptyVecBatch);

    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatch1);

    // add eof flag to buffered table , need add streamed table data
    VectorBatch *bufferedTblVecBatchEof = CreateEmptyVectorBatch(bufferedTblTypes);
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchEof);

    // add eof flag to streamed table
    VectorBatch *streamedTblVecBatchEof1 = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof1);

    VectorBatch *result;
    bufferedTblWithExprOperator->GetOutput(&result);

    // check the join result
    long resultCol1[] = {0, 0, 0, 0, 0, 0};
    double resultCol2[] = {1.1, 2.2, 3.3, 4.4, 5.5, 6.6};
    AssertVecBatchEquals(result, 2, sizeof(resultCol1) / sizeof(resultCol1[0]), resultCol1, resultCol2);
    // todo:compare null
    VectorHelper::FreeVecBatch(result);

    Expr::DeleteExprs(streamedEqualKeyExprs);
    Expr::DeleteExprs(bufferedEqualKeyExprs);
    omniruntime::op::Operator::DeleteOperator(bufferedTblWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(streamedTblWithExprOperator);
    delete bufferedWithExprOperatorFactory;
    delete streamedWithExprOperatorFactory;
}

TEST(SortMergeJoinWithExprV3Test, testSmjFullOuterJoinRepeatRowsAndMultiColumBatch)
{
    // select t1.b, t2.c from t1 left join t2 where t1.a = t2.d
    // streamedTbl t1:  int a, long b;
    // bufferedTbl t2: double c, int d;
    std::string blank;
    std::vector<DataTypePtr> streamTypeVector = {IntType(), LongType()};
    DataTypes streamedTblTypes(streamTypeVector);
    auto *col0 = new FieldExpr(0, IntType());
    std::vector<Expr *> streamedEqualKeyExprs = {col0};
    OperatorConfig operatorConfig;
    std::vector<int32_t> streamedOutputCols = {1};
    StreamedTableWithExprOperatorFactoryV3 *streamedWithExprOperatorFactory =
        StreamedTableWithExprOperatorFactoryV3::CreateStreamedTableWithExprOperatorFactory(
            streamedTblTypes, streamedEqualKeyExprs, streamedOutputCols, JoinType::OMNI_JOIN_TYPE_FULL, blank,
            operatorConfig);
    omniruntime::op::Operator *streamedTblWithExprOperator = CreateTestOperator(streamedWithExprOperatorFactory);

    std::vector<DataTypePtr> bufferTypesVector = {DoubleType(), IntType()};
    DataTypes bufferedTblTypes(bufferTypesVector);
    auto *col1 = new FieldExpr(1, IntType());
    std::vector<Expr *> bufferedEqualKeyExprs = {col1};
    std::vector<int32_t> bufferedOutputCols = {0};
    BufferedTableWithExprOperatorFactoryV3 *bufferedWithExprOperatorFactory =
        BufferedTableWithExprOperatorFactoryV3::CreateBufferedTableWithExprOperatorFactory(
            bufferedTblTypes, bufferedEqualKeyExprs, bufferedOutputCols, streamedWithExprOperatorFactory,
            operatorConfig);
    omniruntime::op::Operator *bufferedTblWithExprOperator = CreateTestOperator(bufferedWithExprOperatorFactory);

    int32_t streamedCol1Row1[1] = {1};
    long streamedCol2Row1[1] = {1100};
    int32_t streamedCol1Row2[1] = {2};
    long streamedCol2Row2[1] = {2200};
    int32_t streamedCol1Row3[1] = {4};
    long streamedCol2Row3[1] = {4400};
    int32_t streamedCol1Row4[1] = {5};
    long streamedCol2Row4[1] = {5500};
    int32_t streamedCol1Row5[1] = {5};
    long streamedCol2Row5[1] = {5500};
    int32_t streamedCol1Row6[2] = {5, 5};
    long streamedCol2Row6[2] = {5500, 5500};
    int32_t streamedCol1Row7[1] = {6};
    long streamedCol2Row7[1] = {6600};
    // construct streamed data
    VectorBatch *streamedTblVecBatchRow1 = CreateVectorBatch(streamedTblTypes, 1, streamedCol1Row1, streamedCol2Row1);
    VectorBatch *streamedTblVecBatchRow2 = CreateVectorBatch(streamedTblTypes, 1, streamedCol1Row2, streamedCol2Row2);
    VectorBatch *streamedTblVecBatchRow3 = CreateVectorBatch(streamedTblTypes, 1, streamedCol1Row3, streamedCol2Row3);
    VectorBatch *streamedTblVecBatchRow4 = CreateVectorBatch(streamedTblTypes, 1, streamedCol1Row4, streamedCol2Row4);
    VectorBatch *streamedTblVecBatchRow5 = CreateVectorBatch(streamedTblTypes, 1, streamedCol1Row5, streamedCol2Row5);
    VectorBatch *streamedTblVecBatchRow6 = CreateVectorBatch(streamedTblTypes, 2, streamedCol1Row6, streamedCol2Row6);
    VectorBatch *streamedTblVecBatchRow7 = CreateVectorBatch(streamedTblTypes, 1, streamedCol1Row7, streamedCol2Row7);

    double bufferedCol1Row1[1] = {1.1};
    int32_t bufferedCol2Row1[1] = {1};
    double bufferedCol1Row2[1] = {2.2};
    int32_t bufferedCol2Row2[1] = {2};
    double bufferedCol1Row3[1] = {3.3};
    int32_t bufferedCol2Row3[1] = {3};
    double bufferedCol1Row4[1] = {4.4};
    int32_t bufferedCol2Row4[1] = {4};
    double bufferedCol1Row5[1] = {5.5};
    int32_t bufferedCol2Row5[1] = {5};
    double bufferedCol1Row6[2] = {5.5, 5.5};
    int32_t bufferedCol2Row6[2] = {5, 5};
    double bufferedCol1Row7[1] = {5.5};
    int32_t bufferedCol2Row7[1] = {5};
    double bufferedCol1Row8[1] = {6.6};
    int32_t bufferedCol2Row8[1] = {6};
    // construct buffered data
    VectorBatch *bufferedTblVecBatchRow1 = CreateVectorBatch(bufferedTblTypes, 1, bufferedCol1Row1, bufferedCol2Row1);
    VectorBatch *bufferedTblVecBatchRow2 = CreateVectorBatch(bufferedTblTypes, 1, bufferedCol1Row2, bufferedCol2Row2);
    VectorBatch *bufferedTblVecBatchRow3 = CreateVectorBatch(bufferedTblTypes, 1, bufferedCol1Row3, bufferedCol2Row3);
    VectorBatch *bufferedTblVecBatchRow4 = CreateVectorBatch(bufferedTblTypes, 1, bufferedCol1Row4, bufferedCol2Row4);
    VectorBatch *bufferedTblVecBatchRow5 = CreateVectorBatch(bufferedTblTypes, 1, bufferedCol1Row5, bufferedCol2Row5);
    VectorBatch *bufferedTblVecBatchRow6 = CreateVectorBatch(bufferedTblTypes, 2, bufferedCol1Row6, bufferedCol2Row6);
    VectorBatch *bufferedTblVecBatchRow7 = CreateVectorBatch(bufferedTblTypes, 1, bufferedCol1Row7, bufferedCol2Row7);
    VectorBatch *bufferedTblVecBatchRow8 = CreateVectorBatch(bufferedTblTypes, 1, bufferedCol1Row8, bufferedCol2Row8);

    // join start add streamed table data
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchRow1);

    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchRow1);

    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchRow2);

    // need add streamed table data
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchRow2);

    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchRow3);

    // need add streamed table data
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchRow3);

    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchRow4);

    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchRow5);

    // need add streamed table data
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchRow4);

    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchRow6);

    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchRow7);

    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchRow8);

    // need add streamed table data
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchRow5);

    // need add streamed table data
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchRow6);

    // need add streamed table data
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchRow7);

    // add eof flag to buffered table , need add streamed table data
    VectorBatch *bufferedTblVecBatchEof = CreateEmptyVectorBatch(bufferedTblTypes);
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchEof);

    // add eof flag to streamed table
    VectorBatch *streamedTblVecBatchEof = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof);
    // add eof flag to streamed table
    VectorBatch *streamedTblVecBatchEof1 = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof1);

    VectorBatch *result;
    bufferedTblWithExprOperator->GetOutput(&result);

    // check the join result
    long resultCol1[] = {1100, 2200, 0,    4400, 5500, 5500, 5500, 5500, 5500, 5500, 5500,
                         5500, 5500, 5500, 5500, 5500, 5500, 5500, 5500, 5500, 6600};
    double resultCol2[] = {1.1, 2.2, 3.3, 4.4, 5.5, 5.5, 5.5, 5.5, 5.5, 5.5, 5.5,
                           5.5, 5.5, 5.5, 5.5, 5.5, 5.5, 5.5, 5.5, 5.5, 6.6};
    AssertVecBatchEquals(result, 2, sizeof(resultCol1) / sizeof(resultCol1[0]), resultCol1, resultCol2);
    // todo:compare null
    VectorHelper::FreeVecBatch(result);

    Expr::DeleteExprs(streamedEqualKeyExprs);
    Expr::DeleteExprs(bufferedEqualKeyExprs);
    omniruntime::op::Operator::DeleteOperator(bufferedTblWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(streamedTblWithExprOperator);
    delete bufferedWithExprOperatorFactory;
    delete streamedWithExprOperatorFactory;
}

TEST(SortMergeJoinWithExprV3Test, testSmjFullOuterJoinMissMatchBothSideWithExpression)
{
    // select t1.b, t2.c from t1 full join t2 where t1.a = t2.d and t1.a > 2;
    // streamedTbl t1:  int a, long b;
    // bufferedTbl t2: double c, int d;
    string filterJsonStr = "{\"exprType\":\"BINARY\","
                           "\"returnType\":4,"
                           "\"operator\":\"GREATER_THAN\","
                           "\"left\":{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":1,\"colVal\":0},"
                           "\"right\":{\"exprType\":\"LITERAL\",\"dataType\":1, \"isNull\":false, \"value\":2}}";
    std::vector<DataTypePtr> streamTypeVector = {IntType(), LongType()};
    DataTypes streamedTblTypes(streamTypeVector);
    auto *col0 = new FieldExpr(0, IntType());
    std::vector<Expr *> streamedEqualKeyExprs = {col0};
    OperatorConfig operatorConfig;
    std::vector<int32_t> streamedOutputCols = {1};
    StreamedTableWithExprOperatorFactoryV3 *streamedWithExprOperatorFactory =
        StreamedTableWithExprOperatorFactoryV3::CreateStreamedTableWithExprOperatorFactory(
            streamedTblTypes, streamedEqualKeyExprs, streamedOutputCols, JoinType::OMNI_JOIN_TYPE_FULL, filterJsonStr,
            operatorConfig);
    omniruntime::op::Operator *streamedTblWithExprOperator = CreateTestOperator(streamedWithExprOperatorFactory);

    std::vector<DataTypePtr> bufferTypesVector = {DoubleType(), IntType()};
    DataTypes bufferedTblTypes(bufferTypesVector);
    auto *col1 = new FieldExpr(1, IntType());
    std::vector<Expr *> bufferedEqualKeyExprs = {col1};
    std::vector<int32_t> bufferedOutputCols = {0};
    BufferedTableWithExprOperatorFactoryV3 *bufferedWithExprOperatorFactory =
        BufferedTableWithExprOperatorFactoryV3::CreateBufferedTableWithExprOperatorFactory(
            bufferedTblTypes, bufferedEqualKeyExprs, bufferedOutputCols, streamedWithExprOperatorFactory,
            operatorConfig);
    omniruntime::op::Operator *bufferedTblWithExprOperator = CreateTestOperator(bufferedWithExprOperatorFactory);

    // construct data
    const int32_t streamedTblDataSize = 6;
    int32_t streamedTblDataCol1[streamedTblDataSize] = {1, 2, 3, 4, 5, 7};
    long streamedTblDataCol2[streamedTblDataSize] = {1100, 2200, 3300, 4400, 5500, 7700};
    VectorBatch *streamedTblVecBatch1 =
        CreateVectorBatch(streamedTblTypes, streamedTblDataSize, streamedTblDataCol1, streamedTblDataCol2);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatch1);

    const int32_t bufferedTblSize = 7;
    double bufferedTblDataCol1[bufferedTblSize] = {1.1, 2.2, 3.3, 4.4, 6.6, 8.8, 9.9};
    int32_t bufferedTblDataCol2[bufferedTblSize] = {1, 2, 3, 4, 6, 8, 9};
    VectorBatch *bufferedTblVecBatch1 =
        CreateVectorBatch(bufferedTblTypes, bufferedTblSize, bufferedTblDataCol1, bufferedTblDataCol2);
    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatch1);

    // add eof flag to streamed table
    VectorBatch *streamedTblVecBatchEof = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof);

    // add eof flag to buffered table , need add streamed table data
    VectorBatch *bufferedTblVecBatchEof = CreateEmptyVectorBatch(bufferedTblTypes);
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchEof);
    auto *streamedTblVecBatchEof1 = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof1);

    VectorBatch *result;
    bufferedTblWithExprOperator->GetOutput(&result);

    // check the join result
    long resultCol1[] = {1100, 0, 2200, 0, 3300, 4400, 5500, 0, 7700, 0, 0};
    double resultCol2[] = {0, 1.1, 0, 2.2, 3.3, 4.4, 0, 6.6, 0, 8.8, 9.9};
    AssertVecBatchEquals(result, 2, sizeof(resultCol1) / sizeof(resultCol1[0]), resultCol1, resultCol2);
    EXPECT_TRUE(result->Get(0)->IsNull(1));
    EXPECT_TRUE(result->Get(0)->IsNull(3));
    EXPECT_TRUE(result->Get(0)->IsNull(7));
    EXPECT_TRUE(result->Get(0)->IsNull(9));
    EXPECT_TRUE(result->Get(0)->IsNull(10));
    EXPECT_TRUE(result->Get(1)->IsNull(0));
    EXPECT_TRUE(result->Get(1)->IsNull(2));
    EXPECT_TRUE(result->Get(1)->IsNull(6));
    EXPECT_TRUE(result->Get(1)->IsNull(8));
    VectorHelper::FreeVecBatch(result);

    Expr::DeleteExprs(streamedEqualKeyExprs);
    Expr::DeleteExprs(bufferedEqualKeyExprs);
    omniruntime::op::Operator::DeleteOperator(bufferedTblWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(streamedTblWithExprOperator);
    delete bufferedWithExprOperatorFactory;
    delete streamedWithExprOperatorFactory;
}

TEST(SortMergeJoinWithExprV3Test, testSmjLeftJoinStreamedWithNullJoinKeyFirstWithExpression)
{
    // select t1.b, t2.c from t1 full join t2 where t1.a = t2.d and t1.a > 3;
    // streamedTbl t1:  int a, long b;
    // bufferedTbl t2: double c, int d;
    string filterJsonStr = "{\"exprType\":\"BINARY\","
                           "\"returnType\":4,"
                           "\"operator\":\"GREATER_THAN\","
                           "\"left\":{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":1,\"colVal\":0},"
                           "\"right\":{\"exprType\":\"LITERAL\",\"dataType\":1, \"isNull\":false, \"value\":3}}";
    std::vector<DataTypePtr> streamTypeVector = {IntType(), LongType()};
    DataTypes streamedTblTypes(streamTypeVector);
    auto *col0 = new FieldExpr(0, IntType());
    std::vector<Expr *> streamedEqualKeyExprs = {col0};
    OperatorConfig operatorConfig;
    std::vector<int32_t> streamedOutputCols = {1};
    StreamedTableWithExprOperatorFactoryV3 *streamedWithExprOperatorFactory =
        StreamedTableWithExprOperatorFactoryV3::CreateStreamedTableWithExprOperatorFactory(
            streamedTblTypes, streamedEqualKeyExprs, streamedOutputCols, JoinType::OMNI_JOIN_TYPE_LEFT, filterJsonStr,
            operatorConfig);
    omniruntime::op::Operator *streamedTblWithExprOperator = CreateTestOperator(streamedWithExprOperatorFactory);

    std::vector<DataTypePtr> bufferTypesVector = {DoubleType(), IntType()};
    DataTypes bufferedTblTypes(bufferTypesVector);
    auto *col1 = new FieldExpr(1, IntType());
    std::vector<Expr *> bufferedEqualKeyExprs = {col1};
    std::vector<int32_t> bufferedOutputCols = {0};
    BufferedTableWithExprOperatorFactoryV3 *bufferedWithExprOperatorFactory =
        BufferedTableWithExprOperatorFactoryV3::CreateBufferedTableWithExprOperatorFactory(
            bufferedTblTypes, bufferedEqualKeyExprs, bufferedOutputCols, streamedWithExprOperatorFactory,
            operatorConfig);
    omniruntime::op::Operator *bufferedTblWithExprOperator = CreateTestOperator(bufferedWithExprOperatorFactory);

    // construct data
    const int32_t streamedTblDataSize = 7;
    int32_t streamedTblDataCol1[streamedTblDataSize] = {1, 2, 3, 4, 5, 6, 7};
    long streamedTblDataCol2[streamedTblDataSize] = {1100, 2200, 3300, 4400, 5500, 6600, 7700};
    VectorBatch *streamedTblVecBatch1 =
        CreateVectorBatch(streamedTblTypes, streamedTblDataSize, streamedTblDataCol1, streamedTblDataCol2);
    streamedTblVecBatch1->Get(0)->SetNull(0); // NULL, NULL, 4, 5, 6, 7
    streamedTblVecBatch1->Get(0)->SetNull(1); // NULL, NULL, 4, 5, 6, 7
    streamedTblWithExprOperator->AddInput(streamedTblVecBatch1);

    const int32_t bufferedTblSize = 6;
    double bufferedTblDataCol1[bufferedTblSize] = {1.1, 2.2, 3.3, 4.4, 6.6, 8.8};
    int32_t bufferedTblDataCol2[bufferedTblSize] = {1, 2, 3, 4, 6, 8};
    VectorBatch *bufferedTblVecBatch1 =
        CreateVectorBatch(bufferedTblTypes, bufferedTblSize, bufferedTblDataCol1, bufferedTblDataCol2);
    bufferedTblVecBatch1->Get(1)->SetNull(0); // NULL, NULL, 3, 4, 6, 8
    bufferedTblVecBatch1->Get(1)->SetNull(1); // NULL, NULL, 3, 4, 6, 8

    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatch1);

    // add eof flag to streamed table
    VectorBatch *streamedTblVecBatchEof = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof);
    auto *streamedTblVecBatchEof1 = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof1);

    VectorBatch *result;
    bufferedTblWithExprOperator->GetOutput(&result);

    // check the join result
    long resultCol1[] = {1100, 2200, 3300, 4400, 5500, 6600, 7700};
    double resultCol2[] = {0, 0, 0, 4.4, 0, 6.6, 0};
    std::vector<DataTypePtr> resultTypeVector = {LongType(), DoubleType()};
    DataTypes resultDataTypes(resultTypeVector);
    VectorBatch *expectVecBatch = CreateVectorBatch(resultDataTypes, 7, resultCol1, resultCol2);
    expectVecBatch->Get(1)->SetNull(0);
    expectVecBatch->Get(1)->SetNull(1);
    expectVecBatch->Get(1)->SetNull(2);
    expectVecBatch->Get(1)->SetNull(4);
    expectVecBatch->Get(1)->SetNull(6);
    ASSERT_TRUE(VecBatchMatch(result, expectVecBatch));
    VectorHelper::FreeVecBatch(result);
    VectorHelper::FreeVecBatch(expectVecBatch);

    Expr::DeleteExprs(streamedEqualKeyExprs);
    Expr::DeleteExprs(bufferedEqualKeyExprs);
    omniruntime::op::Operator::DeleteOperator(bufferedTblWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(streamedTblWithExprOperator);
    delete bufferedWithExprOperatorFactory;
    delete streamedWithExprOperatorFactory;
}

TEST(SortMergeJoinWithExprV3Test, testSmjLeftJoinStreamedRowLastUnJoinedWithExpression)
{
    // select t1.id, t2.id from t1 full join t2 where t1.id = t2.id and t1.age > t2.quantity;
    // bufferedTbl t2: int id, int age;
    // streamedTbl t1:  int id, int quantity;
    string filterJsonStr = "{\"exprType\":\"BINARY\",\"returnType\":4,\"operator\":\"GREATER_THAN\",\"left\":{"
                           "\"exprType\":\"FIELD_REFERENCE\",\"dataType\":1,\"colVal\":1},\"right\":{\"exprType\":"
                           "\"FIELD_REFERENCE\",\"dataType\":1,\"colVal\":3}}";
    std::vector<DataTypePtr> streamTypeVector = {IntType(), IntType()};
    DataTypes streamedTblTypes(streamTypeVector);
    FieldExpr *col0 = new FieldExpr(0, IntType());
    std::vector<Expr *> streamedEqualKeyExprs = {col0};
    OperatorConfig operatorConfig;
    std::vector<int32_t> streamedOutputCols = {0};
    StreamedTableWithExprOperatorFactoryV3 *streamedWithExprOperatorFactory =
        StreamedTableWithExprOperatorFactoryV3::CreateStreamedTableWithExprOperatorFactory(
            streamedTblTypes, streamedEqualKeyExprs, streamedOutputCols, JoinType::OMNI_JOIN_TYPE_LEFT, filterJsonStr,
            operatorConfig);
    omniruntime::op::Operator *streamedTblWithExprOperator = CreateTestOperator(streamedWithExprOperatorFactory);

    std::vector<DataTypePtr> bufferTypesVector = {IntType(), IntType()};
    DataTypes bufferedTblTypes(bufferTypesVector);
    FieldExpr *col1 = new FieldExpr(0, IntType());
    std::vector<Expr *> bufferedEqualKeyExprs = {col1};
    std::vector<int32_t> bufferedOutputCols = {0};
    BufferedTableWithExprOperatorFactoryV3 *bufferedWithExprOperatorFactory =
        BufferedTableWithExprOperatorFactoryV3::CreateBufferedTableWithExprOperatorFactory(
            bufferedTblTypes, bufferedEqualKeyExprs, bufferedOutputCols, streamedWithExprOperatorFactory,
            operatorConfig);
    omniruntime::op::Operator *bufferedTblWithExprOperator = CreateTestOperator(bufferedWithExprOperatorFactory);

    // construct data
    const int32_t streamedTblDataSize = 5;
    int32_t streamedTblDataCol1[streamedTblDataSize] = {100, 200, 300, 400, 500};
    int32_t streamedTblDataCol2[streamedTblDataSize] = {30, 0, 80, 0, 50};
    VectorBatch *streamedTblVecBatch1 =
        CreateVectorBatch(streamedTblTypes, streamedTblDataSize, streamedTblDataCol1, streamedTblDataCol2);
    streamedTblVecBatch1->Get(1)->SetNull(1); // {100, 200, 300, 400, 500}
    streamedTblVecBatch1->Get(1)->SetNull(3); // {30, NULL, 80, NULL, 50}
    streamedTblWithExprOperator->AddInput(streamedTblVecBatch1);

    const int32_t bufferedTblSize = 8;
    int32_t bufferedTblDataCol1[bufferedTblSize] = {100, 100, 100, 200, 200, 200, 300, 300};
    int32_t bufferedTblDataCol2[bufferedTblSize] = {10, 15, 7, 20, 10, 3, 5, 8};
    VectorBatch *bufferedTblVecBatch1 =
        CreateVectorBatch(bufferedTblTypes, bufferedTblSize, bufferedTblDataCol1, bufferedTblDataCol2);

    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatch1);

    // add eof flag to buffered table , need add streamed table data
    VectorBatch *bufferedTblVecBatchEof = CreateEmptyVectorBatch(bufferedTblTypes);
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchEof);

    // add eof flag to streamed table
    VectorBatch *streamedTblVecBatchEof = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof);
    auto streamedTblVecBatchEof1 = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof1);

    VectorBatch *result;
    bufferedTblWithExprOperator->GetOutput(&result);

    // check the join result
    int32_t resultCol1[] = {100, 100, 100, 200, 300, 300, 400, 500};
    int32_t resultCol2[] = {100, 100, 100, 0, 300, 300, 0, 0};
    std::vector<DataTypePtr> resultTypeVector = {IntType(), IntType()};
    DataTypes resultDataTypes(resultTypeVector);
    VectorBatch *expectVecBatch = CreateVectorBatch(resultDataTypes, 8, resultCol1, resultCol2);
    expectVecBatch->Get(1)->SetNull(3);
    expectVecBatch->Get(1)->SetNull(6);
    expectVecBatch->Get(1)->SetNull(7);
    ASSERT_TRUE(VecBatchMatch(result, expectVecBatch));
    VectorHelper::FreeVecBatch(result);
    VectorHelper::FreeVecBatch(expectVecBatch);

    Expr::DeleteExprs(streamedEqualKeyExprs);
    Expr::DeleteExprs(bufferedEqualKeyExprs);
    omniruntime::op::Operator::DeleteOperator(bufferedTblWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(streamedTblWithExprOperator);
    delete bufferedWithExprOperatorFactory;
    delete streamedWithExprOperatorFactory;
}

TEST(SortMergeJoinWithExprV3Test, testSmjLeftSemiJoinWithFilterEmpty)
{
    // select t1.a, t1.b from t1 left semi join t2 where t1.a = t2.d
    // streamedTbl(left table) t1:  int a, long b;
    // bufferedTbl(right table) t2: double c, int d;
    std::string blank = "";
    std::vector<DataTypePtr> streamTypeVector = {IntType(), LongType()};
    DataTypes streamedTblTypes(streamTypeVector);
    auto *col0 = new FieldExpr(0, IntType());
    std::vector<Expr *> streamedEqualKeyExprs = {col0};
    OperatorConfig operatorConfig;
    std::vector<int32_t> streamedOutputCols = {0, 1};
    StreamedTableWithExprOperatorFactoryV3 *streamedWithExprOperatorFactory =
        StreamedTableWithExprOperatorFactoryV3::CreateStreamedTableWithExprOperatorFactory(
            streamedTblTypes, streamedEqualKeyExprs, streamedOutputCols, JoinType::OMNI_JOIN_TYPE_LEFT_SEMI, blank,
            operatorConfig);
    omniruntime::op::Operator *streamedTblWithExprOperator = CreateTestOperator(streamedWithExprOperatorFactory);

    std::vector<DataTypePtr> bufferTypesVector = {DoubleDataType::Instance(), IntType()};
    DataTypes bufferedTblTypes(bufferTypesVector);
    auto *col1 = new FieldExpr(1, IntType());
    std::vector<Expr *> bufferedEqualKeyExprs = {col1};
    std::vector<int32_t> bufferedOutputCols = {};
    BufferedTableWithExprOperatorFactoryV3 *bufferedWithExprOperatorFactory =
        BufferedTableWithExprOperatorFactoryV3::CreateBufferedTableWithExprOperatorFactory(
            bufferedTblTypes, bufferedEqualKeyExprs, bufferedOutputCols, streamedWithExprOperatorFactory,
            operatorConfig);
    omniruntime::op::Operator *bufferedTblWithExprOperator = CreateTestOperator(bufferedWithExprOperatorFactory);

    // construct data
    const int32_t streamedTblDataSize = 6;
    int32_t streamedTblDataCol1[streamedTblDataSize] = {0, 1, 2, 2, 4, 5};
    long streamedTblDataCol2[streamedTblDataSize] = {6600, 5500, 4400, 3300, 2200, 1100};
    VectorBatch *streamedTblVecBatch1 =
        CreateVectorBatch(streamedTblTypes, streamedTblDataSize, streamedTblDataCol1, streamedTblDataCol2);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatch1);

    const int32_t bufferedTblSize = 6;
    double bufferedTblDataCol1[bufferedTblSize] = {6.6, 5.5, 4.4, 3.3, 2.2, 1.1};
    int32_t bufferedTblDataCol2[bufferedTblSize] = {0, 1, 2, 3, 4, 5};
    VectorBatch *bufferedTblVecBatch1 =
        CreateVectorBatch(bufferedTblTypes, bufferedTblSize, bufferedTblDataCol1, bufferedTblDataCol2);
    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatch1);

    // add eof flag to buffered table , need add streamed table data
    VectorBatch *bufferedTblVecBatchEof = CreateEmptyVectorBatch(bufferedTblTypes);
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchEof);

    // add eof flag to streamed table
    VectorBatch *streamedTblVecBatchEof = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof);
    VectorBatch *streamedTblVecBatchEof1 = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof1);

    VectorBatch *result;
    bufferedTblWithExprOperator->GetOutput(&result);
    VectorHelper::PrintVecBatch(result);

    // check the join result
    int32_t expCol1[] = {0, 1, 2, 2, 4, 5};
    long expCol2[] = {6600, 5500, 4400, 3300, 2200, 1100};
    VectorBatch *expectVecBatch = CreateVectorBatch(streamedTblTypes, 6, expCol1, expCol2);
    ASSERT_TRUE(VecBatchMatch(result, expectVecBatch));
    VectorHelper::FreeVecBatch(result);
    VectorHelper::FreeVecBatch(expectVecBatch);

    Expr::DeleteExprs(streamedEqualKeyExprs);
    Expr::DeleteExprs(bufferedEqualKeyExprs);
    omniruntime::op::Operator::DeleteOperator(bufferedTblWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(streamedTblWithExprOperator);
    delete bufferedWithExprOperatorFactory;
    delete streamedWithExprOperatorFactory;
}

TEST(SortMergeJoinWithExprV3Test, testSmjLeftSemiJoinStreamedWithRepeatRows)
{
    // select t1.a, t1.b from t1 left semi join t2 where t1.a = t2.d and t1.a > 1
    // streamedTbl(left table) t1:  int a, long b;
    // bufferedTbl(right table) t2: double c, int d;
    string filterJsonStr = "{\"exprType\":\"BINARY\","
                           "\"returnType\":4,"
                           "\"operator\":\"GREATER_THAN\","
                           "\"left\":{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":1,\"colVal\":0},"
                           "\"right\":{\"exprType\":\"LITERAL\",\"dataType\":1, \"isNull\":false, \"value\":1}}";
    std::vector<DataTypePtr> streamTypeVector = {IntType(), LongType()};
    DataTypes streamedTblTypes(streamTypeVector);
    auto *col0 = new FieldExpr(0, IntType());
    std::vector<Expr *> streamedEqualKeyExprs = {col0};
    OperatorConfig operatorConfig;
    std::vector<int32_t> streamedOutputCols = {0, 1};
    StreamedTableWithExprOperatorFactoryV3 *streamedWithExprOperatorFactory =
        StreamedTableWithExprOperatorFactoryV3::CreateStreamedTableWithExprOperatorFactory(
            streamedTblTypes, streamedEqualKeyExprs, streamedOutputCols, JoinType::OMNI_JOIN_TYPE_LEFT_SEMI,
            filterJsonStr, operatorConfig);
    omniruntime::op::Operator *streamedTblWithExprOperator = CreateTestOperator(streamedWithExprOperatorFactory);

    std::vector<DataTypePtr> bufferTypesVector = {DoubleType(), IntType()};
    DataTypes bufferedTblTypes(bufferTypesVector);
    auto *col1 = new FieldExpr(1, IntType());
    std::vector<Expr *> bufferedEqualKeyExprs = {col1};
    std::vector<int32_t> bufferedOutputCols = {};
    BufferedTableWithExprOperatorFactoryV3 *bufferedWithExprOperatorFactory =
        BufferedTableWithExprOperatorFactoryV3::CreateBufferedTableWithExprOperatorFactory(
            bufferedTblTypes, bufferedEqualKeyExprs, bufferedOutputCols, streamedWithExprOperatorFactory,
            operatorConfig);
    omniruntime::op::Operator *bufferedTblWithExprOperator = CreateTestOperator(bufferedWithExprOperatorFactory);

    // construct data
    const int32_t streamedTblDataSize = 6;
    int32_t streamedTblDataCol1[streamedTblDataSize] = {0, 1, 2, 2, 4, 5};
    long streamedTblDataCol2[streamedTblDataSize] = {6600, 5500, 4400, 3300, 2200, 1100};
    VectorBatch *streamedTblVecBatch1 =
        CreateVectorBatch(streamedTblTypes, streamedTblDataSize, streamedTblDataCol1, streamedTblDataCol2);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatch1);

    const int32_t bufferedTblSize = 6;
    double bufferedTblDataCol1[bufferedTblSize] = {6.6, 5.5, 4.4, 3.3, 2.2, 1.1};
    int32_t bufferedTblDataCol2[bufferedTblSize] = {0, 1, 2, 3, 4, 5};
    VectorBatch *bufferedTblVecBatch1 =
        CreateVectorBatch(bufferedTblTypes, bufferedTblSize, bufferedTblDataCol1, bufferedTblDataCol2);
    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatch1);

    // add eof flag to buffered table , need add streamed table data
    VectorBatch *bufferedTblVecBatchEof = CreateEmptyVectorBatch(bufferedTblTypes);
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchEof);

    // add eof flag to streamed table
    VectorBatch *streamedTblVecBatchEof = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof);
    VectorBatch *streamedTblVecBatchEof1 = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof1);

    VectorBatch *result;
    bufferedTblWithExprOperator->GetOutput(&result);
    VectorHelper::PrintVecBatch(result);

    // check the join result
    int32_t expCol1[] = {2, 2, 4, 5};
    long expCol2[] = {4400, 3300, 2200, 1100};
    VectorBatch *expectVecBatch = CreateVectorBatch(streamedTblTypes, 4, expCol1, expCol2);
    ASSERT_TRUE(VecBatchMatch(result, expectVecBatch));
    VectorHelper::FreeVecBatch(result);
    VectorHelper::FreeVecBatch(expectVecBatch);

    Expr::DeleteExprs(streamedEqualKeyExprs);
    Expr::DeleteExprs(bufferedEqualKeyExprs);
    omniruntime::op::Operator::DeleteOperator(bufferedTblWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(streamedTblWithExprOperator);
    delete bufferedWithExprOperatorFactory;
    delete streamedWithExprOperatorFactory;
}

TEST(SortMergeJoinWithExprV3Test, testSmjLeftSemiJoinBufferedWithRepeatRows)
{
    // select t1.a, t1.b from t1 left semi join t2 where t1.a = t2.d and t1.a <= 4
    // streamedTbl(left table) t1:  int a, long b;
    // bufferedTbl(right table) t2: double c, int d;
    string filterJsonStr = "{\"exprType\":\"BINARY\","
                           "\"returnType\":4,"
                           "\"operator\":\"LESS_THAN_OR_EQUAL\","
                           "\"left\":{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":1,\"colVal\":0},"
                           "\"right\":{\"exprType\":\"LITERAL\",\"dataType\":1, \"isNull\":false, \"value\":4}}";
    std::vector<DataTypePtr> streamTypeVector = {IntType(), LongType()};
    DataTypes streamedTblTypes(streamTypeVector);
    auto *col0 = new FieldExpr(0, IntType());
    std::vector<Expr *> streamedEqualKeyExprs = {col0};
    OperatorConfig operatorConfig;
    std::vector<int32_t> streamedOutputCols = {0, 1};
    StreamedTableWithExprOperatorFactoryV3 *streamedWithExprOperatorFactory =
        StreamedTableWithExprOperatorFactoryV3::CreateStreamedTableWithExprOperatorFactory(
            streamedTblTypes, streamedEqualKeyExprs, streamedOutputCols, JoinType::OMNI_JOIN_TYPE_LEFT_SEMI,
            filterJsonStr, operatorConfig);
    omniruntime::op::Operator *streamedTblWithExprOperator = CreateTestOperator(streamedWithExprOperatorFactory);

    std::vector<DataTypePtr> bufferTypesVector = {DoubleType(), IntType()};
    DataTypes bufferedTblTypes(bufferTypesVector);
    auto *col1 = new FieldExpr(1, IntType());
    std::vector<Expr *> bufferedEqualKeyExprs = {col1};
    std::vector<int32_t> bufferedOutputCols = {};
    BufferedTableWithExprOperatorFactoryV3 *bufferedWithExprOperatorFactory =
        BufferedTableWithExprOperatorFactoryV3::CreateBufferedTableWithExprOperatorFactory(
            bufferedTblTypes, bufferedEqualKeyExprs, bufferedOutputCols, streamedWithExprOperatorFactory,
            operatorConfig);
    omniruntime::op::Operator *bufferedTblWithExprOperator = CreateTestOperator(bufferedWithExprOperatorFactory);

    // construct data
    const int32_t streamedTblDataSize = 6;
    int32_t streamedTblDataCol1[streamedTblDataSize] = {0, 1, 2, 3, 4, 5};
    long streamedTblDataCol2[streamedTblDataSize] = {6600, 5500, 4400, 3300, 2200, 1100};
    VectorBatch *streamedTblVecBatch1 =
        CreateVectorBatch(streamedTblTypes, streamedTblDataSize, streamedTblDataCol1, streamedTblDataCol2);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatch1);

    const int32_t bufferedTblSize = 6;
    double bufferedTblDataCol1[bufferedTblSize] = {6.6, 5.5, 4.4, 3.3, 2.2, 1.1};
    int32_t bufferedTblDataCol2[bufferedTblSize] = {0, 1, 2, 2, 2, 5};
    VectorBatch *bufferedTblVecBatch1 =
        CreateVectorBatch(bufferedTblTypes, bufferedTblSize, bufferedTblDataCol1, bufferedTblDataCol2);
    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatch1);

    // add eof flag to buffered table , need add streamed table data
    VectorBatch *bufferedTblVecBatchEof = CreateEmptyVectorBatch(bufferedTblTypes);
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchEof);

    // add eof flag to streamed table
    VectorBatch *streamedTblVecBatchEof = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof);
    VectorBatch *streamedTblVecBatchEof1 = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof1);

    VectorBatch *result;
    bufferedTblWithExprOperator->GetOutput(&result);

    // check the join result
    int32_t expCol1[] = {0, 1, 2};
    long expCol2[] = {6600, 5500, 4400};
    VectorBatch *expectVecBatch = CreateVectorBatch(streamedTblTypes, 3, expCol1, expCol2);
    ASSERT_TRUE(VecBatchMatch(result, expectVecBatch));
    VectorHelper::FreeVecBatch(result);
    VectorHelper::FreeVecBatch(expectVecBatch);

    Expr::DeleteExprs(streamedEqualKeyExprs);
    Expr::DeleteExprs(bufferedEqualKeyExprs);
    omniruntime::op::Operator::DeleteOperator(bufferedTblWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(streamedTblWithExprOperator);
    delete bufferedWithExprOperatorFactory;
    delete streamedWithExprOperatorFactory;
}

TEST(SortMergeJoinWithExprV3Test, testSmjLeftSemiJoinBothNullFirst)
{
    // select t1.a, t1.b from t1 left semi join t2 where t1.a = t2.d and t1.a < 5
    // streamedTbl(left table) t1:  int a, long b;
    // bufferedTbl(right table) t2: double c, int d;
    string filterJsonStr = "{\"exprType\":\"BINARY\","
                           "\"returnType\":4,"
                           "\"operator\":\"LESS_THAN\","
                           "\"left\":{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":1,\"colVal\":0},"
                           "\"right\":{\"exprType\":\"LITERAL\",\"dataType\":1, \"isNull\":false, \"value\":5}}";
    std::vector<DataTypePtr> streamTypeVector = {IntType(), LongType()};
    DataTypes streamedTblTypes(streamTypeVector);
    auto *col0 = new FieldExpr(0, IntType());
    std::vector<Expr *> streamedEqualKeyExprs = {col0};
    OperatorConfig operatorConfig;
    std::vector<int32_t> streamedOutputCols = {0, 1};
    StreamedTableWithExprOperatorFactoryV3 *streamedWithExprOperatorFactory =
        StreamedTableWithExprOperatorFactoryV3::CreateStreamedTableWithExprOperatorFactory(
            streamedTblTypes, streamedEqualKeyExprs, streamedOutputCols, JoinType::OMNI_JOIN_TYPE_LEFT_SEMI,
            filterJsonStr, operatorConfig);
    omniruntime::op::Operator *streamedTblWithExprOperator = CreateTestOperator(streamedWithExprOperatorFactory);

    std::vector<DataTypePtr> bufferTypesVector = {DoubleType(), IntType()};
    DataTypes bufferedTblTypes(bufferTypesVector);
    auto *col1 = new FieldExpr(1, IntType());
    std::vector<Expr *> bufferedEqualKeyExprs = {col1};
    std::vector<int32_t> bufferedOutputCols = {};
    BufferedTableWithExprOperatorFactoryV3 *bufferedWithExprOperatorFactory =
        BufferedTableWithExprOperatorFactoryV3::CreateBufferedTableWithExprOperatorFactory(
            bufferedTblTypes, bufferedEqualKeyExprs, bufferedOutputCols, streamedWithExprOperatorFactory,
            operatorConfig);
    omniruntime::op::Operator *bufferedTblWithExprOperator = CreateTestOperator(bufferedWithExprOperatorFactory);

    // construct data
    const int32_t streamedTblDataSize = 6;
    int32_t streamedTblDataCol1[streamedTblDataSize] = {0, 1, 2, 3, 4, 5};
    long streamedTblDataCol2[streamedTblDataSize] = {6600, 5500, 4400, 3300, 2200, 1100};
    VectorBatch *streamedTblVecBatch1 =
        CreateVectorBatch(streamedTblTypes, streamedTblDataSize, streamedTblDataCol1, streamedTblDataCol2);
    streamedTblVecBatch1->Get(0)->SetNull(0); // NULL, 1, 2, 3, 4, 5
    streamedTblVecBatch1->Get(0)->SetNull(1); // NULL, NULL, 2, 3, 4, 5
    streamedTblWithExprOperator->AddInput(streamedTblVecBatch1);

    const int32_t bufferedTblSize = 6;
    double bufferedTblDataCol1[bufferedTblSize] = {6.6, 5.5, 4.4, 3.3, 2.2, 1.1};
    int32_t bufferedTblDataCol2[bufferedTblSize] = {6, 1, 2, 3, 4, 5};
    VectorBatch *bufferedTblVecBatch1 =
        CreateVectorBatch(bufferedTblTypes, bufferedTblSize, bufferedTblDataCol1, bufferedTblDataCol2);
    bufferedTblVecBatch1->Get(1)->SetNull(0); // NULL, 1, 2, 3, 4, 5

    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatch1);

    // add eof flag to buffered table , need add streamed table data
    VectorBatch *bufferedTblVecBatchEof = CreateEmptyVectorBatch(bufferedTblTypes);
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchEof);

    // add eof flag to streamed table
    VectorBatch *streamedTblVecBatchEof = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof);
    VectorBatch *streamedTblVecBatchEof1 = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof1);

    VectorBatch *result;
    bufferedTblWithExprOperator->GetOutput(&result);

    // check the join result
    int32_t expCol1[] = {2, 3, 4};
    long expCol2[] = {4400, 3300, 2200};
    VectorBatch *expectVecBatch = CreateVectorBatch(streamedTblTypes, 3, expCol1, expCol2);
    ASSERT_TRUE(VecBatchMatch(result, expectVecBatch));
    VectorHelper::FreeVecBatch(result);
    VectorHelper::FreeVecBatch(expectVecBatch);

    Expr::DeleteExprs(streamedEqualKeyExprs);
    Expr::DeleteExprs(bufferedEqualKeyExprs);
    omniruntime::op::Operator::DeleteOperator(bufferedTblWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(streamedTblWithExprOperator);
    delete bufferedWithExprOperatorFactory;
    delete streamedWithExprOperatorFactory;
}

TEST(SortMergeJoinWithExprV3Test, testSmjLeftSemiJoinFilterDeduplicate)
{
    // select t1.a, t1.b from t1 left semi join t2 where t1.a = t2.d and t2.c > 4
    // streamedTbl(left table) t1:  int a, long b;
    // bufferedTbl(right table) t2: double c, int d;
    string filterJsonStr = "{\"exprType\":\"BINARY\","
                           "\"returnType\":4,"
                           "\"operator\":\"GREATER_THAN\","
                           "\"left\":{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":3,\"colVal\":2},"
                           "\"right\":{\"exprType\":\"LITERAL\",\"dataType\":3, \"isNull\":false, \"value\":4}}";
    std::vector<DataTypePtr> streamTypeVector = {IntType(), LongType()};
    DataTypes streamedTblTypes(streamTypeVector);
    auto *col0 = new FieldExpr(0, IntType());
    std::vector<Expr *> streamedEqualKeyExprs = {col0};
    OperatorConfig operatorConfig;
    std::vector<int32_t> streamedOutputCols = {0, 1};
    StreamedTableWithExprOperatorFactoryV3 *streamedWithExprOperatorFactory =
        StreamedTableWithExprOperatorFactoryV3::CreateStreamedTableWithExprOperatorFactory(
            streamedTblTypes, streamedEqualKeyExprs, streamedOutputCols, JoinType::OMNI_JOIN_TYPE_LEFT_SEMI,
            filterJsonStr, operatorConfig);
    omniruntime::op::Operator *streamedTblWithExprOperator = CreateTestOperator(streamedWithExprOperatorFactory);

    std::vector<DataTypePtr> bufferTypesVector = {DoubleType(), IntType()};
    DataTypes bufferedTblTypes(bufferTypesVector);
    auto *col1 = new FieldExpr(1, IntType());
    std::vector<Expr *> bufferedEqualKeyExprs = {col1};
    std::vector<int32_t> bufferedOutputCols = {};
    BufferedTableWithExprOperatorFactoryV3 *bufferedWithExprOperatorFactory =
        BufferedTableWithExprOperatorFactoryV3::CreateBufferedTableWithExprOperatorFactory(
            bufferedTblTypes, bufferedEqualKeyExprs, bufferedOutputCols, streamedWithExprOperatorFactory,
            operatorConfig);
    omniruntime::op::Operator *bufferedTblWithExprOperator = CreateTestOperator(bufferedWithExprOperatorFactory);

    // construct data
    const int32_t streamedTblDataSize = 6;
    int32_t streamedTblDataCol1[streamedTblDataSize] = {0, 1, 1, 2, 3, 4};
    long streamedTblDataCol2[streamedTblDataSize] = {6600, 5500, 4400, 3300, 2200, 1100};
    VectorBatch *streamedTblVecBatch1 =
        CreateVectorBatch(streamedTblTypes, streamedTblDataSize, streamedTblDataCol1, streamedTblDataCol2);
    streamedTblVecBatch1->Get(0)->SetNull(0); // NULL, 1, 1, 2, 3, 4
    streamedTblWithExprOperator->AddInput(streamedTblVecBatch1);

    const int32_t bufferedTblSize = 6;
    double bufferedTblDataCol1[bufferedTblSize] = {6.6, 5.5, 3.3, 4.4, 3.3, 4.4};
    int32_t bufferedTblDataCol2[bufferedTblSize] = {0, 1, 2, 2, 3, 4};
    VectorBatch *bufferedTblVecBatch1 =
        CreateVectorBatch(bufferedTblTypes, bufferedTblSize, bufferedTblDataCol1, bufferedTblDataCol2);
    bufferedTblVecBatch1->Get(1)->SetNull(0); // NULL, 1, 2, 2, 3, 4

    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatch1);

    // add eof flag to buffered table , need add streamed table data
    VectorBatch *bufferedTblVecBatchEof = CreateEmptyVectorBatch(bufferedTblTypes);
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchEof);

    // add eof flag to streamed table
    VectorBatch *streamedTblVecBatchEof = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof);
    VectorBatch *streamedTblVecBatchEof1 = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof1);

    VectorBatch *result;
    bufferedTblWithExprOperator->GetOutput(&result);

    // check the join result
    int32_t expCol1[] = {1, 1, 2, 4};
    long expCol2[] = {5500, 4400, 3300, 1100};
    VectorBatch *expectVecBatch = CreateVectorBatch(streamedTblTypes, 4, expCol1, expCol2);
    ASSERT_TRUE(VecBatchMatch(result, expectVecBatch));
    VectorHelper::FreeVecBatch(result);
    VectorHelper::FreeVecBatch(expectVecBatch);

    Expr::DeleteExprs(streamedEqualKeyExprs);
    Expr::DeleteExprs(bufferedEqualKeyExprs);
    omniruntime::op::Operator::DeleteOperator(bufferedTblWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(streamedTblWithExprOperator);
    delete bufferedWithExprOperatorFactory;
    delete streamedWithExprOperatorFactory;
}

TEST(SortMergeJoinWithExprV3Test, testSmjLeftSemiJoinBufferedWithDuplicateFilterResults)
{
    // select t1.a, t1.b from t1 left semi join t2 where t1.a = t2.d and t2.c > 4
    // streamedTbl(left table) t1:  int a, long b;
    // bufferedTbl(right table) t2: double c, int d;
    string filterJsonStr = "{\"exprType\":\"BINARY\","
                           "\"returnType\":4,"
                           "\"operator\":\"GREATER_THAN\","
                           "\"left\":{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":3,\"colVal\":2},"
                           "\"right\":{\"exprType\":\"LITERAL\",\"dataType\":3, \"isNull\":false, \"value\":4}}";
    std::vector<DataTypePtr> streamTypeVector = {IntType(), LongType()};
    DataTypes streamedTblTypes(streamTypeVector);
    auto *col0 = new FieldExpr(0, IntType());
    std::vector<Expr *> streamedEqualKeyExprs = {col0};
    OperatorConfig operatorConfig;
    std::vector<int32_t> streamedOutputCols = {0, 1};
    StreamedTableWithExprOperatorFactoryV3 *streamedWithExprOperatorFactory =
        StreamedTableWithExprOperatorFactoryV3::CreateStreamedTableWithExprOperatorFactory(
            streamedTblTypes, streamedEqualKeyExprs, streamedOutputCols, JoinType::OMNI_JOIN_TYPE_LEFT_SEMI,
            filterJsonStr, operatorConfig);
    omniruntime::op::Operator *streamedTblWithExprOperator = CreateTestOperator(streamedWithExprOperatorFactory);

    std::vector<DataTypePtr> bufferTypesVector = {DoubleType(), IntType()};
    DataTypes bufferedTblTypes(bufferTypesVector);
    auto *col1 = new FieldExpr(1, IntType());
    std::vector<Expr *> bufferedEqualKeyExprs = {col1};
    std::vector<int32_t> bufferedOutputCols = {};
    BufferedTableWithExprOperatorFactoryV3 *bufferedWithExprOperatorFactory =
        BufferedTableWithExprOperatorFactoryV3::CreateBufferedTableWithExprOperatorFactory(
            bufferedTblTypes, bufferedEqualKeyExprs, bufferedOutputCols, streamedWithExprOperatorFactory,
            operatorConfig);
    omniruntime::op::Operator *bufferedTblWithExprOperator = CreateTestOperator(bufferedWithExprOperatorFactory);

    // construct data
    const int32_t streamedTblDataSize = 6;
    int32_t streamedTblDataCol1[streamedTblDataSize] = {0, 1, 1, 2, 3, 4};
    long streamedTblDataCol2[streamedTblDataSize] = {6600, 5500, 4400, 3300, 2200, 1100};
    VectorBatch *streamedTblVecBatch1 =
        CreateVectorBatch(streamedTblTypes, streamedTblDataSize, streamedTblDataCol1, streamedTblDataCol2);
    streamedTblVecBatch1->Get(0)->SetNull(0); // NULL, 1, 1, 2, 3, 4
    streamedTblWithExprOperator->AddInput(streamedTblVecBatch1);

    const int32_t bufferedTblSize = 7;
    double bufferedTblDataCol1[bufferedTblSize] = {6.6, 5.5, 3.3, 4.4, 4.5, 3.3, 4.4};
    int32_t bufferedTblDataCol2[bufferedTblSize] = {0, 1, 2, 2, 2, 3, 4};
    VectorBatch *bufferedTblVecBatch1 =
        CreateVectorBatch(bufferedTblTypes, bufferedTblSize, bufferedTblDataCol1, bufferedTblDataCol2);
    bufferedTblVecBatch1->Get(1)->SetNull(0); // NULL, 1, 2, 2, 2, 3, 4

    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatch1);

    // add eof flag to buffered table , need add streamed table data
    VectorBatch *bufferedTblVecBatchEof = CreateEmptyVectorBatch(bufferedTblTypes);
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchEof);

    // add eof flag to streamed table
    VectorBatch *streamedTblVecBatchEof = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof);
    VectorBatch *streamedTblVecBatchEof1 = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof1);

    VectorBatch *result;
    bufferedTblWithExprOperator->GetOutput(&result);

    // check the join result
    int32_t expCol1[] = {1, 1, 2, 4};
    long expCol2[] = {5500, 4400, 3300, 1100};
    VectorBatch *expectVecBatch = CreateVectorBatch(streamedTblTypes, 4, expCol1, expCol2);
    ASSERT_TRUE(VecBatchMatch(result, expectVecBatch));
    VectorHelper::FreeVecBatch(result);
    VectorHelper::FreeVecBatch(expectVecBatch);

    Expr::DeleteExprs(streamedEqualKeyExprs);
    Expr::DeleteExprs(bufferedEqualKeyExprs);
    omniruntime::op::Operator::DeleteOperator(bufferedTblWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(streamedTblWithExprOperator);
    delete bufferedWithExprOperatorFactory;
    delete streamedWithExprOperatorFactory;
}

TEST(SortMergeJoinWithExprV3Test, testSmjLeftAntiJoinStreamedWithEmptyFilterExpression)
{
    // select * from AntiTest_s left anti join AntiTest_b on AntiTest_s.CountryID = AntiTest_b.ID;
    // streamedTbl AntiTest_s: CountryID int, Units long;
    // bufferedTbl AntiTest_b: ID int, Country double;
    string filterJsonStr;
    std::vector<DataTypePtr> streamTypeVector = {IntType(), LongType()};
    DataTypes streamedTblTypes(streamTypeVector);
    auto *streamCol0 = new FieldExpr(0, IntType());
    std::vector<Expr *> streamedEqualKeyExprs = {streamCol0};
    std::vector<int32_t> streamedOutputCols = {0, 1};
    OperatorConfig operatorConfig;
    StreamedTableWithExprOperatorFactoryV3 *streamedWithExprOperatorFactory =
        StreamedTableWithExprOperatorFactoryV3::CreateStreamedTableWithExprOperatorFactory(
            streamedTblTypes, streamedEqualKeyExprs, streamedOutputCols, JoinType::OMNI_JOIN_TYPE_LEFT_ANTI,
            filterJsonStr, operatorConfig);
    omniruntime::op::Operator *streamedTblWithExprOperator = CreateTestOperator(streamedWithExprOperatorFactory);

    std::vector<DataTypePtr> bufferTypesVector = {IntType(), DoubleType()};
    DataTypes bufferedTblTypes(bufferTypesVector);
    auto *bufferCol0 = new FieldExpr(0, IntType());
    std::vector<Expr *> bufferedEqualKeyExprs = {bufferCol0};
    std::vector<int32_t> bufferedOutputCols = {};
    BufferedTableWithExprOperatorFactoryV3 *bufferedWithExprOperatorFactory =
        BufferedTableWithExprOperatorFactoryV3::CreateBufferedTableWithExprOperatorFactory(
            bufferedTblTypes, bufferedEqualKeyExprs, bufferedOutputCols, streamedWithExprOperatorFactory,
            operatorConfig);
    omniruntime::op::Operator *bufferedTblWithExprOperator = CreateTestOperator(bufferedWithExprOperatorFactory);

    // construct data
    const int32_t streamedTblDataSize = 4;
    int32_t streamedTblDataCol1[streamedTblDataSize] = {1, 1, 2, 3};
    long streamedTblDataCol2[streamedTblDataSize] = {40, 25, 35, 30};
    VectorBatch *streamedTblVecBatch1 =
        CreateVectorBatch(streamedTblTypes, streamedTblDataSize, streamedTblDataCol1, streamedTblDataCol2);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatch1);

    const int32_t bufferedTblSize = 2;
    int32_t bufferedTblDataCol1[bufferedTblSize] = {3, 4};
    double bufferedTblDataCol2[bufferedTblSize] = {3.3, 4.4};
    VectorBatch *bufferedTblVecBatch1 =
        CreateVectorBatch(bufferedTblTypes, bufferedTblSize, bufferedTblDataCol1, bufferedTblDataCol2);
    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatch1);

    // add eof flag to streamed table
    VectorBatch *streamedTblVecBatchEof = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof);
    auto *streamedTblVecBatchEof1 = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof1);

    VectorBatch *result;
    bufferedTblWithExprOperator->GetOutput(&result);

    // check the join result
    int resultCol1[] = {1, 1, 2};
    long resultCol2[] = {40, 25, 35};
    VectorBatch *expectVecBatch = CreateVectorBatch(streamedTblTypes, 3, resultCol1, resultCol2);
    ASSERT_TRUE(VecBatchMatch(result, expectVecBatch));
    VectorHelper::FreeVecBatch(result);
    VectorHelper::FreeVecBatch(expectVecBatch);

    Expr::DeleteExprs(streamedEqualKeyExprs);
    Expr::DeleteExprs(bufferedEqualKeyExprs);
    omniruntime::op::Operator::DeleteOperator(bufferedTblWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(streamedTblWithExprOperator);
    delete bufferedWithExprOperatorFactory;
    delete streamedWithExprOperatorFactory;
}

TEST(SortMergeJoinWithExprV3Test, testSmjLeftAntiJoinEqualCondition)
{
    // select * from AntiTest_s left anti join AntiTest_b on AntiTest_s.CountryID = AntiTest_b.ID and
    // AntiTest_s.CountryID = 4; streamedTbl AntiTest_s: CountryID int, Units long; bufferedTbl AntiTest_b: ID int,
    // Country double;
    string filterJsonStr = "{\"exprType\":\"BINARY\","
                           "\"returnType\":4,"
                           "\"operator\":\"EQUAL\","
                           "\"left\":{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":1,\"colVal\":0},"
                           "\"right\":{\"exprType\":\"LITERAL\",\"dataType\":1, \"isNull\":false, \"value\":4}}";
    std::vector<DataTypePtr> streamTypeVector = {IntType(), LongType()};
    DataTypes streamedTblTypes(streamTypeVector);
    auto *streamCol0 = new FieldExpr(0, IntType());
    std::vector<Expr *> streamedEqualKeyExprs = {streamCol0};
    std::vector<int32_t> streamedOutputCols = {0, 1};
    OperatorConfig operatorConfig;
    StreamedTableWithExprOperatorFactoryV3 *streamedWithExprOperatorFactory =
        StreamedTableWithExprOperatorFactoryV3::CreateStreamedTableWithExprOperatorFactory(
            streamedTblTypes, streamedEqualKeyExprs, streamedOutputCols, JoinType::OMNI_JOIN_TYPE_LEFT_ANTI,
            filterJsonStr, operatorConfig);
    omniruntime::op::Operator *streamedTblWithExprOperator = CreateTestOperator(streamedWithExprOperatorFactory);

    std::vector<DataTypePtr> bufferTypesVector = {IntType(), DoubleType()};
    DataTypes bufferedTblTypes(bufferTypesVector);
    auto *bufferCol0 = new FieldExpr(0, IntType());
    std::vector<Expr *> bufferedEqualKeyExprs = {bufferCol0};
    std::vector<int32_t> bufferedOutputCols = {};
    BufferedTableWithExprOperatorFactoryV3 *bufferedWithExprOperatorFactory =
        BufferedTableWithExprOperatorFactoryV3::CreateBufferedTableWithExprOperatorFactory(
            bufferedTblTypes, bufferedEqualKeyExprs, bufferedOutputCols, streamedWithExprOperatorFactory,
            operatorConfig);
    omniruntime::op::Operator *bufferedTblWithExprOperator = CreateTestOperator(bufferedWithExprOperatorFactory);

    // construct data
    const int32_t streamedTblDataSize = 4;
    int32_t streamedTblDataCol1[streamedTblDataSize] = {1, 1, 2, 3};
    long streamedTblDataCol2[streamedTblDataSize] = {40, 25, 35, 30};
    VectorBatch *streamedTblVecBatch1 =
        CreateVectorBatch(streamedTblTypes, streamedTblDataSize, streamedTblDataCol1, streamedTblDataCol2);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatch1);

    const int32_t bufferedTblSize = 2;
    int32_t bufferedTblDataCol1[bufferedTblSize] = {3, 4};
    double bufferedTblDataCol2[bufferedTblSize] = {3.3, 4.4};
    VectorBatch *bufferedTblVecBatch1 =
        CreateVectorBatch(bufferedTblTypes, bufferedTblSize, bufferedTblDataCol1, bufferedTblDataCol2);
    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatch1);

    // add eof flag to streamed table
    VectorBatch *streamedTblVecBatchEof = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof);
    auto *streamedTblVecBatchEof1 = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof1);

    VectorBatch *result;
    bufferedTblWithExprOperator->GetOutput(&result);

    // check the join result
    int resultCol1[] = {1, 1, 2, 3};
    long resultCol2[] = {40, 25, 35, 30};
    VectorBatch *expectVecBatch = CreateVectorBatch(streamedTblTypes, 4, resultCol1, resultCol2);
    ASSERT_TRUE(VecBatchMatch(result, expectVecBatch));
    VectorHelper::FreeVecBatch(result);
    VectorHelper::FreeVecBatch(expectVecBatch);

    Expr::DeleteExprs(streamedEqualKeyExprs);
    Expr::DeleteExprs(bufferedEqualKeyExprs);
    omniruntime::op::Operator::DeleteOperator(bufferedTblWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(streamedTblWithExprOperator);
    delete bufferedWithExprOperatorFactory;
    delete streamedWithExprOperatorFactory;
}

TEST(SortMergeJoinWithExprV3Test, testSmjLeftAntiJoinBufferDoubleJoinRowFilterOut)
{
    // select * from AntiTest_s left anti join AntiTest_b on AntiTest_s.CountryID = AntiTest_b.ID and
    // AntiTest_s.CountryID = 4; streamedTbl AntiTest_s: CountryID int, Units long; bufferedTbl AntiTest_b: ID int,
    // Country double;
    string filterJsonStr = "{\"exprType\":\"BINARY\","
                           "\"returnType\":4,"
                           "\"operator\":\"EQUAL\","
                           "\"left\":{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":1,\"colVal\":0},"
                           "\"right\":{\"exprType\":\"LITERAL\",\"dataType\":1, \"isNull\":false, \"value\":4}}";
    std::vector<DataTypePtr> streamTypeVector = {IntType(), LongType()};
    DataTypes streamedTblTypes(streamTypeVector);
    auto *streamCol0 = new FieldExpr(0, IntType());
    std::vector<Expr *> streamedEqualKeyExprs = {streamCol0};
    std::vector<int32_t> streamedOutputCols = {0, 1};
    OperatorConfig operatorConfig;
    StreamedTableWithExprOperatorFactoryV3 *streamedWithExprOperatorFactory =
        StreamedTableWithExprOperatorFactoryV3::CreateStreamedTableWithExprOperatorFactory(
            streamedTblTypes, streamedEqualKeyExprs, streamedOutputCols, JoinType::OMNI_JOIN_TYPE_LEFT_ANTI,
            filterJsonStr, operatorConfig);
    omniruntime::op::Operator *streamedTblWithExprOperator = CreateTestOperator(streamedWithExprOperatorFactory);

    std::vector<DataTypePtr> bufferTypesVector = {IntType(), DoubleType()};
    DataTypes bufferedTblTypes(bufferTypesVector);
    auto *bufferCol0 = new FieldExpr(0, IntType());
    std::vector<Expr *> bufferedEqualKeyExprs = {bufferCol0};
    std::vector<int32_t> bufferedOutputCols = {};
    BufferedTableWithExprOperatorFactoryV3 *bufferedWithExprOperatorFactory =
        BufferedTableWithExprOperatorFactoryV3::CreateBufferedTableWithExprOperatorFactory(
            bufferedTblTypes, bufferedEqualKeyExprs, bufferedOutputCols, streamedWithExprOperatorFactory,
            operatorConfig);
    omniruntime::op::Operator *bufferedTblWithExprOperator = CreateTestOperator(bufferedWithExprOperatorFactory);

    // construct data
    const int32_t streamedTblDataSize = 4;
    int32_t streamedTblDataCol1[streamedTblDataSize] = {1, 1, 2, 3};
    long streamedTblDataCol2[streamedTblDataSize] = {40, 25, 35, 30};
    VectorBatch *streamedTblVecBatch1 =
        CreateVectorBatch(streamedTblTypes, streamedTblDataSize, streamedTblDataCol1, streamedTblDataCol2);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatch1);

    const int32_t bufferedTblSize = 4;
    int32_t bufferedTblDataCol1[bufferedTblSize] = {3, 3, 3, 4};
    double bufferedTblDataCol2[bufferedTblSize] = {3.3, 3.3, 3.4, 4.4};
    VectorBatch *bufferedTblVecBatch1 =
        CreateVectorBatch(bufferedTblTypes, bufferedTblSize, bufferedTblDataCol1, bufferedTblDataCol2);
    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatch1);

    // add eof flag to streamed table
    VectorBatch *streamedTblVecBatchEof = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof);
    auto *streamedTblVecBatchEof1 = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof1);

    VectorBatch *result;
    bufferedTblWithExprOperator->GetOutput(&result);

    // check the join result
    int resultCol1[] = {1, 1, 2, 3};
    long resultCol2[] = {40, 25, 35, 30};
    VectorBatch *expectVecBatch = CreateVectorBatch(streamedTblTypes, 4, resultCol1, resultCol2);
    ASSERT_TRUE(VecBatchMatch(result, expectVecBatch));
    VectorHelper::FreeVecBatch(result);
    VectorHelper::FreeVecBatch(expectVecBatch);

    Expr::DeleteExprs(streamedEqualKeyExprs);
    Expr::DeleteExprs(bufferedEqualKeyExprs);
    omniruntime::op::Operator::DeleteOperator(bufferedTblWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(streamedTblWithExprOperator);
    delete bufferedWithExprOperatorFactory;
    delete streamedWithExprOperatorFactory;
}

TEST(SortMergeJoinWithExprV3Test, testSmjLeftAntiJoinWithStreamedNullFist)
{
    // select * from AntiTest_s left anti join AntiTest_b on AntiTest_s.CountryID = AntiTest_b.ID and
    // AntiTest_s.CountryID = 4; streamedTbl AntiTest_s: CountryID int, Units long; bufferedTbl AntiTest_b: ID int,
    // Country double;
    string filterJsonStr = "{\"exprType\":\"BINARY\","
                           "\"returnType\":4,"
                           "\"operator\":\"EQUAL\","
                           "\"left\":{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":1,\"colVal\":0},"
                           "\"right\":{\"exprType\":\"LITERAL\",\"dataType\":1, \"isNull\":false, \"value\":4}}";
    std::vector<DataTypePtr> streamTypeVector = {IntType(), LongType()};
    DataTypes streamedTblTypes(streamTypeVector);
    auto *streamCol0 = new FieldExpr(0, IntType());
    std::vector<Expr *> streamedEqualKeyExprs = {streamCol0};
    std::vector<int32_t> streamedOutputCols = {0, 1};
    OperatorConfig operatorConfig;
    StreamedTableWithExprOperatorFactoryV3 *streamedWithExprOperatorFactory =
        StreamedTableWithExprOperatorFactoryV3::CreateStreamedTableWithExprOperatorFactory(
            streamedTblTypes, streamedEqualKeyExprs, streamedOutputCols, JoinType::OMNI_JOIN_TYPE_LEFT_ANTI,
            filterJsonStr, operatorConfig);
    omniruntime::op::Operator *streamedTblWithExprOperator = CreateTestOperator(streamedWithExprOperatorFactory);

    std::vector<DataTypePtr> bufferTypesVector = {IntType(), DoubleType()};
    DataTypes bufferedTblTypes(bufferTypesVector);
    auto *bufferCol0 = new FieldExpr(0, IntType());
    std::vector<Expr *> bufferedEqualKeyExprs = {bufferCol0};
    std::vector<int32_t> bufferedOutputCols = {};
    BufferedTableWithExprOperatorFactoryV3 *bufferedWithExprOperatorFactory =
        BufferedTableWithExprOperatorFactoryV3::CreateBufferedTableWithExprOperatorFactory(
            bufferedTblTypes, bufferedEqualKeyExprs, bufferedOutputCols, streamedWithExprOperatorFactory,
            operatorConfig);
    omniruntime::op::Operator *bufferedTblWithExprOperator = CreateTestOperator(bufferedWithExprOperatorFactory);

    // construct data
    const int32_t streamedTblDataSize = 4;
    int32_t streamedTblDataCol1[streamedTblDataSize] = {1, 1, 2, 3};
    long streamedTblDataCol2[streamedTblDataSize] = {40, 25, 35, 30};
    VectorBatch *streamedTblVecBatch1 =
        CreateVectorBatch(streamedTblTypes, streamedTblDataSize, streamedTblDataCol1, streamedTblDataCol2);
    streamedTblVecBatch1->Get(0)->SetNull(0); // NULL, 1, 2, 3
    streamedTblWithExprOperator->AddInput(streamedTblVecBatch1);

    const int32_t bufferedTblSize = 2;
    int32_t bufferedTblDataCol1[bufferedTblSize] = {3, 4};
    double bufferedTblDataCol2[bufferedTblSize] = {3.3, 4.4};
    VectorBatch *bufferedTblVecBatch1 =
        CreateVectorBatch(bufferedTblTypes, bufferedTblSize, bufferedTblDataCol1, bufferedTblDataCol2);
    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatch1);

    // add eof flag to streamed table
    VectorBatch *streamedTblVecBatchEof = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof);
    auto *streamedTblVecBatchEof1 = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof1);

    VectorBatch *result;
    bufferedTblWithExprOperator->GetOutput(&result);

    // check the join result
    int resultCol1[] = {0, 1, 2, 3};
    long resultCol2[] = {40, 25, 35, 30};
    VectorBatch *expectVecBatch = CreateVectorBatch(streamedTblTypes, 4, resultCol1, resultCol2);
    expectVecBatch->Get(0)->SetNull(0);
    ASSERT_TRUE(VecBatchMatch(result, expectVecBatch));
    VectorHelper::FreeVecBatch(result);
    VectorHelper::FreeVecBatch(expectVecBatch);

    Expr::DeleteExprs(streamedEqualKeyExprs);
    Expr::DeleteExprs(bufferedEqualKeyExprs);
    omniruntime::op::Operator::DeleteOperator(bufferedTblWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(streamedTblWithExprOperator);
    delete bufferedWithExprOperatorFactory;
    delete streamedWithExprOperatorFactory;
}

TEST(SortMergeJoinWithExprV3Test, testSmjLeftAntiJoinWithBufferedNullFist)
{
    // select * from AntiTest_s left anti join AntiTest_b on AntiTest_s.CountryID = AntiTest_b.ID and
    // AntiTest_s.CountryID = 4; streamedTbl AntiTest_s: CountryID int, Units long; bufferedTbl AntiTest_b: ID int,
    // Country double;
    string filterJsonStr = "{\"exprType\":\"BINARY\","
                           "\"returnType\":4,"
                           "\"operator\":\"EQUAL\","
                           "\"left\":{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":1,\"colVal\":0},"
                           "\"right\":{\"exprType\":\"LITERAL\",\"dataType\":1, \"isNull\":false, \"value\":4}}";
    std::vector<DataTypePtr> streamTypeVector = {IntType(), LongType()};
    DataTypes streamedTblTypes(streamTypeVector);
    auto *streamCol0 = new FieldExpr(0, IntType());
    std::vector<Expr *> streamedEqualKeyExprs = {streamCol0};
    std::vector<int32_t> streamedOutputCols = {0, 1};
    OperatorConfig operatorConfig;
    StreamedTableWithExprOperatorFactoryV3 *streamedWithExprOperatorFactory =
        StreamedTableWithExprOperatorFactoryV3::CreateStreamedTableWithExprOperatorFactory(
            streamedTblTypes, streamedEqualKeyExprs, streamedOutputCols, JoinType::OMNI_JOIN_TYPE_LEFT_ANTI,
            filterJsonStr, operatorConfig);
    omniruntime::op::Operator *streamedTblWithExprOperator = CreateTestOperator(streamedWithExprOperatorFactory);

    std::vector<DataTypePtr> bufferTypesVector = {IntType(), DoubleType()};
    DataTypes bufferedTblTypes(bufferTypesVector);
    auto *bufferCol0 = new FieldExpr(0, IntType());
    std::vector<Expr *> bufferedEqualKeyExprs = {bufferCol0};
    std::vector<int32_t> bufferedOutputCols = {};
    BufferedTableWithExprOperatorFactoryV3 *bufferedWithExprOperatorFactory =
        BufferedTableWithExprOperatorFactoryV3::CreateBufferedTableWithExprOperatorFactory(
            bufferedTblTypes, bufferedEqualKeyExprs, bufferedOutputCols, streamedWithExprOperatorFactory,
            operatorConfig);
    omniruntime::op::Operator *bufferedTblWithExprOperator = CreateTestOperator(bufferedWithExprOperatorFactory);

    // construct data
    const int32_t streamedTblDataSize = 4;
    int32_t streamedTblDataCol1[streamedTblDataSize] = {1, 1, 2, 3};
    long streamedTblDataCol2[streamedTblDataSize] = {40, 25, 35, 30};
    VectorBatch *streamedTblVecBatch1 =
        CreateVectorBatch(streamedTblTypes, streamedTblDataSize, streamedTblDataCol1, streamedTblDataCol2);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatch1);

    const int32_t bufferedTblSize = 3;
    int32_t bufferedTblDataCol1[bufferedTblSize] = {3, 3, 4};
    double bufferedTblDataCol2[bufferedTblSize] = {3.3, 3.3, 4.4};
    VectorBatch *bufferedTblVecBatch1 =
        CreateVectorBatch(bufferedTblTypes, bufferedTblSize, bufferedTblDataCol1, bufferedTblDataCol2);
    bufferedTblVecBatch1->Get(0)->SetNull(0); // NULL,  3,  4

    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatch1);

    // add eof flag to streamed table
    VectorBatch *streamedTblVecBatchEof = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof);
    auto *streamedTblVecBatchEof1 = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof1);

    VectorBatch *result;
    bufferedTblWithExprOperator->GetOutput(&result);

    // check the join result
    int resultCol1[] = {1, 1, 2, 3};
    long resultCol2[] = {40, 25, 35, 30};
    VectorBatch *expectVecBatch = CreateVectorBatch(streamedTblTypes, 4, resultCol1, resultCol2);
    ASSERT_TRUE(VecBatchMatch(result, expectVecBatch));
    VectorHelper::FreeVecBatch(result);
    VectorHelper::FreeVecBatch(expectVecBatch);

    Expr::DeleteExprs(streamedEqualKeyExprs);
    Expr::DeleteExprs(bufferedEqualKeyExprs);
    omniruntime::op::Operator::DeleteOperator(bufferedTblWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(streamedTblWithExprOperator);
    delete bufferedWithExprOperatorFactory;
    delete streamedWithExprOperatorFactory;
}

TEST(SortMergeJoinWithExprV3Test, testSmjLeftAntiJoinWithBothSideNullFist)
{
    // select * from AntiTest_s left anti join AntiTest_b on AntiTest_s.CountryID = AntiTest_b.ID and
    // AntiTest_s.CountryID = 4; streamedTbl AntiTest_s: CountryID int, Units long; bufferedTbl AntiTest_b: ID int,
    // Country double;
    string filterJsonStr = "{\"exprType\":\"BINARY\","
                           "\"returnType\":4,"
                           "\"operator\":\"EQUAL\","
                           "\"left\":{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":1,\"colVal\":0},"
                           "\"right\":{\"exprType\":\"LITERAL\",\"dataType\":1, \"isNull\":false, \"value\":4}}";
    std::vector<DataTypePtr> streamTypeVector = {IntType(), LongType()};
    DataTypes streamedTblTypes(streamTypeVector);
    auto *streamCol0 = new FieldExpr(0, IntType());
    std::vector<Expr *> streamedEqualKeyExprs = {streamCol0};
    std::vector<int32_t> streamedOutputCols = {0, 1};
    OperatorConfig operatorConfig;
    StreamedTableWithExprOperatorFactoryV3 *streamedWithExprOperatorFactory =
        StreamedTableWithExprOperatorFactoryV3::CreateStreamedTableWithExprOperatorFactory(
            streamedTblTypes, streamedEqualKeyExprs, streamedOutputCols, JoinType::OMNI_JOIN_TYPE_LEFT_ANTI,
            filterJsonStr, operatorConfig);
    omniruntime::op::Operator *streamedTblWithExprOperator = CreateTestOperator(streamedWithExprOperatorFactory);

    std::vector<DataTypePtr> bufferTypesVector = {IntType(), DoubleType()};
    DataTypes bufferedTblTypes(bufferTypesVector);
    auto *bufferCol0 = new FieldExpr(0, IntType());
    std::vector<Expr *> bufferedEqualKeyExprs = {bufferCol0};
    std::vector<int32_t> bufferedOutputCols = {};
    BufferedTableWithExprOperatorFactoryV3 *bufferedWithExprOperatorFactory =
        BufferedTableWithExprOperatorFactoryV3::CreateBufferedTableWithExprOperatorFactory(
            bufferedTblTypes, bufferedEqualKeyExprs, bufferedOutputCols, streamedWithExprOperatorFactory,
            operatorConfig);
    omniruntime::op::Operator *bufferedTblWithExprOperator = CreateTestOperator(bufferedWithExprOperatorFactory);

    // construct data
    const int32_t streamedTblDataSize = 4;
    int32_t streamedTblDataCol1[streamedTblDataSize] = {1, 1, 2, 3};
    long streamedTblDataCol2[streamedTblDataSize] = {40, 25, 35, 30};
    VectorBatch *streamedTblVecBatch1 =
        CreateVectorBatch(streamedTblTypes, streamedTblDataSize, streamedTblDataCol1, streamedTblDataCol2);
    streamedTblVecBatch1->Get(0)->SetNull(0); // NULL, 1, 2, 3
    streamedTblWithExprOperator->AddInput(streamedTblVecBatch1);

    const int32_t bufferedTblSize = 3;
    int32_t bufferedTblDataCol1[bufferedTblSize] = {3, 3, 4};
    double bufferedTblDataCol2[bufferedTblSize] = {3.3, 3.3, 4.4};
    VectorBatch *bufferedTblVecBatch1 =
        CreateVectorBatch(bufferedTblTypes, bufferedTblSize, bufferedTblDataCol1, bufferedTblDataCol2);
    bufferedTblVecBatch1->Get(0)->SetNull(0); // NULL,  3,  4

    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatch1);

    // add eof flag to streamed table
    VectorBatch *streamedTblVecBatchEof = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof);
    auto *streamedTblVecBatchEof1 = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof1);

    VectorBatch *result;
    bufferedTblWithExprOperator->GetOutput(&result);

    // check the join result
    int resultCol1[] = {0, 1, 2, 3};
    long resultCol2[] = {40, 25, 35, 30};
    VectorBatch *expectVecBatch = CreateVectorBatch(streamedTblTypes, 4, resultCol1, resultCol2);
    expectVecBatch->Get(0)->SetNull(0);
    ASSERT_TRUE(VecBatchMatch(result, expectVecBatch));
    VectorHelper::FreeVecBatch(result);
    VectorHelper::FreeVecBatch(expectVecBatch);

    Expr::DeleteExprs(streamedEqualKeyExprs);
    Expr::DeleteExprs(bufferedEqualKeyExprs);
    omniruntime::op::Operator::DeleteOperator(bufferedTblWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(streamedTblWithExprOperator);
    delete bufferedWithExprOperatorFactory;
    delete streamedWithExprOperatorFactory;
}

TEST(SortMergeJoinWithExprV3Test, testSmjLeftAntiJoinWithMultiRowAndFilterOut)
{
    // select * from AntiTest_s left anti join AntiTest_b on AntiTest_s.CountryID = AntiTest_b.ID and
    // AntiTest_b.Country = 3.3; streamedTbl AntiTest_s: CountryID int, Units long; bufferedTbl AntiTest_b: ID int,
    // Country double;
    string filterJsonStr = "{\"exprType\":\"BINARY\","
                           "\"returnType\":4,"
                           "\"operator\":\"GREATER_THAN\","
                           "\"left\":{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":3,\"colVal\":3},"
                           "\"right\":{\"exprType\":\"LITERAL\",\"dataType\":3, \"isNull\":false, \"value\":3.3}}";
    std::vector<DataTypePtr> streamTypeVector = {IntType(), LongType()};
    DataTypes streamedTblTypes(streamTypeVector);
    auto *streamCol0 = new FieldExpr(0, IntType());
    std::vector<Expr *> streamedEqualKeyExprs = {streamCol0};
    std::vector<int32_t> streamedOutputCols = {0, 1};
    OperatorConfig operatorConfig;
    StreamedTableWithExprOperatorFactoryV3 *streamedWithExprOperatorFactory =
        StreamedTableWithExprOperatorFactoryV3::CreateStreamedTableWithExprOperatorFactory(
            streamedTblTypes, streamedEqualKeyExprs, streamedOutputCols, JoinType::OMNI_JOIN_TYPE_LEFT_ANTI,
            filterJsonStr, operatorConfig);
    omniruntime::op::Operator *streamedTblWithExprOperator = CreateTestOperator(streamedWithExprOperatorFactory);

    std::vector<DataTypePtr> bufferTypesVector = {IntType(), DoubleType()};
    DataTypes bufferedTblTypes(bufferTypesVector);
    auto *bufferCol0 = new FieldExpr(0, IntType());
    std::vector<Expr *> bufferedEqualKeyExprs = {bufferCol0};
    std::vector<int32_t> bufferedOutputCols = {};
    BufferedTableWithExprOperatorFactoryV3 *bufferedWithExprOperatorFactory =
        BufferedTableWithExprOperatorFactoryV3::CreateBufferedTableWithExprOperatorFactory(
            bufferedTblTypes, bufferedEqualKeyExprs, bufferedOutputCols, streamedWithExprOperatorFactory,
            operatorConfig);
    omniruntime::op::Operator *bufferedTblWithExprOperator = CreateTestOperator(bufferedWithExprOperatorFactory);

    // construct data
    const int32_t streamedTblDataSize = 4;
    int32_t streamedTblDataCol1[streamedTblDataSize] = {1, 1, 2, 3};
    long streamedTblDataCol2[streamedTblDataSize] = {40, 25, 35, 30};
    VectorBatch *streamedTblVecBatch1 =
        CreateVectorBatch(streamedTblTypes, streamedTblDataSize, streamedTblDataCol1, streamedTblDataCol2);
    streamedTblVecBatch1->Get(0)->SetNull(0); // NULL, 1, 2, 3
    streamedTblWithExprOperator->AddInput(streamedTblVecBatch1);

    const int32_t bufferedTblSize = 5;
    int32_t bufferedTblDataCol1[bufferedTblSize] = {2, 3, 3, 3, 4};
    double bufferedTblDataCol2[bufferedTblSize] = {2.2, 3.4, 3.3, 3.3, 4.5};
    VectorBatch *bufferedTblVecBatch1 =
        CreateVectorBatch(bufferedTblTypes, bufferedTblSize, bufferedTblDataCol1, bufferedTblDataCol2);

    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatch1);

    // add eof flag to streamed table
    VectorBatch *streamedTblVecBatchEof = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof);
    auto *streamedTblVecBatchEof1 = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof1);

    VectorBatch *result;
    bufferedTblWithExprOperator->GetOutput(&result);

    // check the join result
    int resultCol1[] = {0, 1, 2};
    long resultCol2[] = {40, 25, 35};
    VectorBatch *expectVecBatch = CreateVectorBatch(streamedTblTypes, 3, resultCol1, resultCol2);
    expectVecBatch->Get(0)->SetNull(0);
    ASSERT_TRUE(VecBatchMatch(result, expectVecBatch));
    VectorHelper::FreeVecBatch(result);
    VectorHelper::FreeVecBatch(expectVecBatch);

    Expr::DeleteExprs(streamedEqualKeyExprs);
    Expr::DeleteExprs(bufferedEqualKeyExprs);
    omniruntime::op::Operator::DeleteOperator(bufferedTblWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(streamedTblWithExprOperator);
    delete bufferedWithExprOperatorFactory;
    delete streamedWithExprOperatorFactory;
}

TEST(SortMergeJoinWithExprV3Test, testSmjInnerJoinExprGreaterThanIterativeGetOutput)
{
    using namespace omniruntime::expressions;
    // select t1.b, t2.c from t1 full join t2 where t1.a = t2.d and t1.a > 2;
    // streamedTbl t1:  int a, Long b;
    // bufferedTbl t2: double c, int d;
    string filterJsonStr = "{\"exprType\":\"BINARY\","
                           "\"returnType\":4,"
                           "\"operator\":\"GREATER_THAN\","
                           "\"left\":{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":1,\"colVal\":0},"
                           "\"right\":{\"exprType\":\"LITERAL\",\"dataType\":1, \"isNull\":false, \"value\":2}}";
    std::vector<DataTypePtr> streamTypeVector = {IntType(), VarcharType(2000)};
    DataTypes streamedTblTypes(streamTypeVector);
    FieldExpr *col0 = new FieldExpr(0, IntType());
    std::vector<Expr *> streamedEqualKeyExprs = {col0};

    std::vector<int32_t> streamedOutputCols = {1};
    OperatorConfig operatorConfig;
    StreamedTableWithExprOperatorFactoryV3 *streamedWithExprOperatorFactory =
        StreamedTableWithExprOperatorFactoryV3::CreateStreamedTableWithExprOperatorFactory(
            streamedTblTypes, streamedEqualKeyExprs, streamedOutputCols, JoinType::OMNI_JOIN_TYPE_INNER, filterJsonStr,
            operatorConfig);
    omniruntime::op::Operator *streamedTblWithExprOperator = CreateTestOperator(streamedWithExprOperatorFactory);

    std::vector<DataTypePtr> bufferTypesVector = {VarcharType(2000), IntType()};
    DataTypes bufferedTblTypes(bufferTypesVector);
    FieldExpr *col1 = new FieldExpr(1, IntType());
    std::vector<Expr *> bufferedEqualKeyExprs = {col1};
    std::vector<int32_t> bufferedOutputCols = {0};
    BufferedTableWithExprOperatorFactoryV3 *bufferedWithExprOperatorFactory =
        BufferedTableWithExprOperatorFactoryV3::CreateBufferedTableWithExprOperatorFactory(
            bufferedTblTypes, bufferedEqualKeyExprs, bufferedOutputCols, streamedWithExprOperatorFactory,
            operatorConfig);
    omniruntime::op::Operator *bufferedTblWithExprOperator = CreateTestOperator(bufferedWithExprOperatorFactory);

    // construct data
    const int32_t streamedTblDataSize = 270;
    int32_t streamedTblCol1Data[streamedTblDataSize];
    std::string streamedTblCol2Data[streamedTblDataSize];
    for (int32_t i = 0; i < streamedTblDataSize; i++) {
        streamedTblCol1Data[i] = i;
        streamedTblCol2Data[i] = std::to_string(i + 1);
    }
    VectorBatch *streamedTblVecBatch =
        CreateVectorBatch(streamedTblTypes, streamedTblDataSize, streamedTblCol1Data, streamedTblCol2Data);

    const int32_t bufferedTblSize = 270;
    std::string bufferedTblCol1Data[bufferedTblSize];
    int32_t bufferedTblCol2Data[bufferedTblSize];
    for (int32_t i = 0; i < streamedTblDataSize; i++) {
        bufferedTblCol1Data[i] = std::to_string(i + 3);
        bufferedTblCol2Data[i] = i;
    }
    VectorBatch *bufferedTblVecBatch =
        CreateVectorBatch(bufferedTblTypes, bufferedTblSize, bufferedTblCol1Data, bufferedTblCol2Data);

    streamedTblWithExprOperator->AddInput(streamedTblVecBatch);

    // need add buffered table data
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatch);

    // add eof flag to buffered table , need add streamed table data
    VectorBatch *bufferedTblVecBatchEof = CreateEmptyVectorBatch(bufferedTblTypes);
    bufferedTblWithExprOperator->AddInput(bufferedTblVecBatchEof);

    // add eof flag to streamed table
    VectorBatch *streamedTblVecBatchEof = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedTblVecBatchEof);

    std::vector<omniruntime::vec::VectorBatch *> result;
    while (bufferedTblWithExprOperator->GetStatus() != OMNI_STATUS_FINISHED) {
        VectorBatch *output = nullptr;
        bufferedTblWithExprOperator->GetOutput(&output);
        result.emplace_back(output);
    }

    int32_t resultCount = 0;
    for (uint32_t i = 0; i < result.size(); i++) {
        resultCount += result[i]->GetRowCount();
    }
    ASSERT_EQ(resultCount, streamedTblDataSize - 3);
    // check the join result
    int32_t index = 3;
    for (uint32_t i = 0; i < result.size(); i++) {
        ASSERT_EQ(result[i]->GetVectorCount(), 2);
        for (auto j = 0; j < result[i]->GetRowCount(); j++) {
            auto value1 =
                (static_cast<Vector<LargeStringContainer<std::string_view>> *>(result[i]->Get(0)))->GetValue(j);
            ASSERT_EQ(value1, streamedTblCol2Data[index]);

            auto value2 =
                (static_cast<Vector<LargeStringContainer<std::string_view>> *>(result[i]->Get(1)))->GetValue(j);
            ASSERT_EQ(value2, bufferedTblCol1Data[index]);
            index++;
        }
        VectorHelper::FreeVecBatch(result[i]);
    }

    Expr::DeleteExprs(streamedEqualKeyExprs);
    Expr::DeleteExprs(bufferedEqualKeyExprs);
    omniruntime::op::Operator::DeleteOperator(bufferedTblWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(streamedTblWithExprOperator);
    delete bufferedWithExprOperatorFactory;
    delete streamedWithExprOperatorFactory;
}

TEST(SortMergeJoinWithExprV3Test, TestBothJoinKeyAndFilterWithExpr)
{
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
    DataTypes streamedTblTypes(std::vector<DataTypePtr>({Decimal64Type(18, 2)}));
    auto castExpr1 = new FuncExpr("CAST", {new FieldExpr(0, Decimal64Type(18, 2))}, VarcharType(50));
    auto substrExpr1 = new FuncExpr("substr", {castExpr1, new LiteralExpr(1, IntType()), new LiteralExpr(2, IntType())},
                                    VarcharType(50));
    std::vector<omniruntime::expressions::Expr *> streamedEqualKeyExprs{substrExpr1};
    std::vector<int32_t> streamedOutputCols = {0};
    OperatorConfig operatorConfig;
    auto streamedWithExprOperatorFactory =
        StreamedTableWithExprOperatorFactoryV3::CreateStreamedTableWithExprOperatorFactory(
            streamedTblTypes, streamedEqualKeyExprs, streamedOutputCols, JoinType::OMNI_JOIN_TYPE_LEFT, filter,
            operatorConfig);
    auto streamedTblWithExprOperator = CreateTestOperator(streamedWithExprOperatorFactory);

    DataTypes bufferedTblTypes(std::vector<DataTypePtr>{Decimal64Type(18, 2)});
    auto castExpr2 = new FuncExpr("CAST", {new FieldExpr(0, Decimal64Type(18, 2))}, VarcharType(50));
    auto substrExpr2 = new FuncExpr("substr", {castExpr2, new LiteralExpr(1, IntType()), new LiteralExpr(2, IntType())},
                                    VarcharType(50));
    std::vector<omniruntime::expressions::Expr *> bufferedEqualKeyExprs{substrExpr2};
    std::vector<int32_t> bufferedOutputCols = {0};
    auto bufferedWithExprOperatorFactory =
        BufferedTableWithExprOperatorFactoryV3::CreateBufferedTableWithExprOperatorFactory(
            bufferedTblTypes, bufferedEqualKeyExprs, bufferedOutputCols, streamedWithExprOperatorFactory,
            operatorConfig);
    omniruntime::op::Operator *bufferedTblWithExprOperator = CreateTestOperator(bufferedWithExprOperatorFactory);

    const int32_t dataSize = 2;
    int64_t streamedData[] = {111, 112};
    auto streamedVecBatch = TestUtil::CreateVectorBatch(streamedTblTypes, dataSize, streamedData);
    streamedTblWithExprOperator->AddInput(streamedVecBatch);

    // need add buffered table data
    int64_t bufferedData[] = {111, 112};
    auto bufferedVecBatch = TestUtil::CreateVectorBatch(bufferedTblTypes, dataSize, bufferedData);
    bufferedTblWithExprOperator->AddInput(bufferedVecBatch);

    // add eof flag to buffered table , need add streamed table data
    VectorBatch *bufferedVecBatchEof = CreateEmptyVectorBatch(bufferedTblTypes);
    bufferedTblWithExprOperator->AddInput(bufferedVecBatchEof);

    // add eof flag to streamed table
    VectorBatch *streamedVecBatchEof = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedVecBatchEof);

    VectorBatch *result;
    bufferedTblWithExprOperator->GetOutput(&result);

    // check the join result
    int64_t expectedData0[] = {111, 111, 112};
    int64_t expectedData1[] = {111, 112, 112};
    AssertVecBatchEquals(result, 2, sizeof(expectedData0) / sizeof(expectedData0[0]), expectedData0, expectedData1);
    VectorHelper::FreeVecBatch(result);

    Expr::DeleteExprs(streamedEqualKeyExprs);
    Expr::DeleteExprs(bufferedEqualKeyExprs);
    omniruntime::op::Operator::DeleteOperator(bufferedTblWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(streamedTblWithExprOperator);
    delete bufferedWithExprOperatorFactory;
    delete streamedWithExprOperatorFactory;
}

TEST(SortMergeJoinWithExprV3Test, testSmjLeftAntiJoinSubStrAndCaseWhen)
{
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
    DataTypes streamedTblTypes(std::vector<DataTypePtr>({Decimal64Type(18, 2)}));
    auto castExpr1 = new FuncExpr("CAST", {new FieldExpr(0, Decimal64Type(18, 2))}, VarcharType(50));
    auto substrExpr1 = new FuncExpr("substr", {castExpr1, new LiteralExpr(1, IntType()), new LiteralExpr(2, IntType())},
                                    VarcharType(50));
    std::vector<omniruntime::expressions::Expr *> streamedEqualKeyExprs{substrExpr1};
    std::vector<int32_t> streamedOutputCols = {0};
    OperatorConfig operatorConfig;
    auto streamedWithExprOperatorFactory =
        StreamedTableWithExprOperatorFactoryV3::CreateStreamedTableWithExprOperatorFactory(
            streamedTblTypes, streamedEqualKeyExprs, streamedOutputCols, JoinType::OMNI_JOIN_TYPE_LEFT_ANTI, filter,
            operatorConfig);
    auto streamedTblWithExprOperator = CreateTestOperator(streamedWithExprOperatorFactory);

    DataTypes bufferedTblTypes(std::vector<DataTypePtr>{Decimal64Type(18, 2)});
    auto castExpr2 = new FuncExpr("CAST", {new FieldExpr(0, Decimal64Type(18, 2))}, VarcharType(50));
    auto substrExpr2 = new FuncExpr("substr", {castExpr2, new LiteralExpr(1, IntType()), new LiteralExpr(2, IntType())},
                                    VarcharType(50));
    std::vector<omniruntime::expressions::Expr *> bufferedEqualKeyExprs{substrExpr2};
    std::vector<int32_t> bufferedOutputCols = {};
    auto bufferedWithExprOperatorFactory =
        BufferedTableWithExprOperatorFactoryV3::CreateBufferedTableWithExprOperatorFactory(
            bufferedTblTypes, bufferedEqualKeyExprs, bufferedOutputCols, streamedWithExprOperatorFactory,
            operatorConfig);
    omniruntime::op::Operator *bufferedTblWithExprOperator = CreateTestOperator(bufferedWithExprOperatorFactory);

    const int32_t dataSize = 2;
    int64_t streamedData[] = {111, 112};
    auto streamedVecBatch = TestUtil::CreateVectorBatch(streamedTblTypes, dataSize, streamedData);
    streamedTblWithExprOperator->AddInput(streamedVecBatch);

    // need add buffered table data
    int64_t bufferedData[] = {111, 112};
    auto bufferedVecBatch = TestUtil::CreateVectorBatch(bufferedTblTypes, dataSize, bufferedData);
    bufferedTblWithExprOperator->AddInput(bufferedVecBatch);

    // add eof flag to buffered table , need add streamed table data
    VectorBatch *bufferedVecBatchEof = CreateEmptyVectorBatch(bufferedTblTypes);
    bufferedTblWithExprOperator->AddInput(bufferedVecBatchEof);

    // add eof flag to streamed table
    VectorBatch *streamedVecBatchEof = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedVecBatchEof);

    VectorBatch *result = nullptr;
    bufferedTblWithExprOperator->GetOutput(&result);

    // check the join result
    EXPECT_TRUE(result == nullptr);

    Expr::DeleteExprs(streamedEqualKeyExprs);
    Expr::DeleteExprs(bufferedEqualKeyExprs);
    omniruntime::op::Operator::DeleteOperator(bufferedTblWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(streamedTblWithExprOperator);
    delete bufferedWithExprOperatorFactory;
    delete streamedWithExprOperatorFactory;
}

TEST(SortMergeJoinWithExprV3Test, testSmjLeftAntiForEmptyVecBatch)
{
    std::string filter = "";
    DataTypes streamedTblTypes(std::vector<DataTypePtr>({Date32Type(), VarcharType(200), VarcharType(200)}));

    auto v1 = new FieldExpr(1, VarcharType(200));
    auto str = new std::string("");
    auto v2 = new LiteralExpr(str, VarcharType(200));
    auto streamedKey = new CoalesceExpr(v1, v2);
    std::vector<omniruntime::expressions::Expr *> streamedEqualKeyExprs{streamedKey};

    std::vector<int32_t> streamedOutputCols = {0};
    OperatorConfig operatorConfig;
    auto streamedWithExprOperatorFactory =
        StreamedTableWithExprOperatorFactoryV3::CreateStreamedTableWithExprOperatorFactory(
            streamedTblTypes, streamedEqualKeyExprs, streamedOutputCols, JoinType::OMNI_JOIN_TYPE_LEFT_ANTI, filter,
            operatorConfig);
    auto streamedTblWithExprOperator = CreateTestOperator(streamedWithExprOperatorFactory);

    DataTypes bufferedTblTypes(std::vector<DataTypePtr>{VarcharType(200), VarcharType(200), Date32Type()});
    auto v3 = new FieldExpr(1, VarcharType(200));
    auto str1 = new std::string("");
    auto v4 = new LiteralExpr(str1, VarcharType(200));
    auto bufferedKey = new CoalesceExpr(v3, v4);
    std::vector<omniruntime::expressions::Expr *> bufferedEqualKeyExprs{bufferedKey};
    std::vector<int32_t> bufferedOutputCols = {};
    auto bufferedWithExprOperatorFactory =
        BufferedTableWithExprOperatorFactoryV3::CreateBufferedTableWithExprOperatorFactory(
            bufferedTblTypes, bufferedEqualKeyExprs, bufferedOutputCols, streamedWithExprOperatorFactory,
            operatorConfig);
    omniruntime::op::Operator *bufferedTblWithExprOperator = CreateTestOperator(bufferedWithExprOperatorFactory);

    // add eof flag to buffered table , need add streamed table data
    VectorBatch *bufferedVecBatchEof = CreateEmptyVectorBatch(bufferedTblTypes);
    bufferedTblWithExprOperator->AddInput(bufferedVecBatchEof);

    // add eof flag to streamed table
    VectorBatch *streamedVecBatchEof = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedVecBatchEof);

    VectorBatch *result = nullptr;
    bufferedTblWithExprOperator->GetOutput(&result);

    // check the join result
    EXPECT_EQ(result, nullptr);

    Expr::DeleteExprs(streamedEqualKeyExprs);
    Expr::DeleteExprs(bufferedEqualKeyExprs);
    omniruntime::op::Operator::DeleteOperator(bufferedTblWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(streamedTblWithExprOperator);
    delete bufferedWithExprOperatorFactory;
    delete streamedWithExprOperatorFactory;
}

TEST(SortMergeJoinWithExprV3Test, testSmjInner1)
{
    std::string filter =
        "{\"exprType\":\"BINARY\",\"returnType\":4,\"operator\":\"EQUAL\",\"left\":{\"exprType\":\"FIELD_REFERENCE\","
        "\"dataType\":1,\"colVal\":0},\"right\":{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":1, \"colVal\":2}}";
    DataTypes streamedTblTypes(std::vector<DataTypePtr>({IntType(), IntType()}));

    auto streamedKey = new FieldExpr(1, IntType());
    std::vector<omniruntime::expressions::Expr *> streamedEqualKeyExprs{streamedKey};

    std::vector<int32_t> streamedOutputCols = {0};
    OperatorConfig operatorConfig;
    auto streamedWithExprOperatorFactory =
        StreamedTableWithExprOperatorFactoryV3::CreateStreamedTableWithExprOperatorFactory(
            streamedTblTypes, streamedEqualKeyExprs, streamedOutputCols, JoinType::OMNI_JOIN_TYPE_INNER, filter,
            operatorConfig);
    auto streamedTblWithExprOperator = CreateTestOperator(streamedWithExprOperatorFactory);

    DataTypes bufferedTblTypes(std::vector<DataTypePtr>{IntType(), IntType()});
    auto bufferedKey = new FieldExpr(1, IntType());
    std::vector<omniruntime::expressions::Expr *> bufferedEqualKeyExprs{bufferedKey};
    std::vector<int32_t> bufferedOutputCols = {0};
    auto bufferedWithExprOperatorFactory =
        BufferedTableWithExprOperatorFactoryV3::CreateBufferedTableWithExprOperatorFactory(
            bufferedTblTypes, bufferedEqualKeyExprs, bufferedOutputCols, streamedWithExprOperatorFactory,
            operatorConfig);
    omniruntime::op::Operator *bufferedTblWithExprOperator = CreateTestOperator(bufferedWithExprOperatorFactory);

    int32_t buffer00[] = {1001, 1002, 1003};
    int32_t buffer01[] = {1, 1, 3};
    VectorBatch *bufferedVecBatch0 = CreateVectorBatch(bufferedTblTypes, 3, buffer00, buffer01);
    int32_t buffer10[] = {8001, 1003, 8001};
    int32_t buffer11[] = {3, 3, 3};
    VectorBatch *bufferedVecBatch1 = CreateVectorBatch(bufferedTblTypes, 3, buffer10, buffer11);

    int32_t stream00[] = {8001, 8002};
    int32_t stream01[] = {2, 2};
    VectorBatch *streamedVecBatch0 = CreateVectorBatch(streamedTblTypes, 2, stream00, stream01);

    bufferedTblWithExprOperator->AddInput(bufferedVecBatch0);
    bufferedTblWithExprOperator->AddInput(bufferedVecBatch1);
    streamedTblWithExprOperator->AddInput(streamedVecBatch0);

    int32_t stream10[] = {8001, 1003};
    int32_t stream11[] = {3, 3};
    VectorBatch *streamedVecBatch1 = CreateVectorBatch(streamedTblTypes, 2, stream10, stream11);
    streamedTblWithExprOperator->AddInput(streamedVecBatch1);

    int32_t stream20[] = {8001, 1003};
    int32_t stream21[] = {3, 3};
    VectorBatch *streamedVecBatch2 = CreateVectorBatch(streamedTblTypes, 2, stream20, stream21);
    streamedTblWithExprOperator->AddInput(streamedVecBatch2);

    // add eof flag to buffered table , need add streamed table data
    VectorBatch *bufferedVecBatchEof = CreateEmptyVectorBatch(bufferedTblTypes);
    bufferedTblWithExprOperator->AddInput(bufferedVecBatchEof);

    // add eof flag to streamed table
    VectorBatch *streamedVecBatchEof = CreateEmptyVectorBatch(streamedTblTypes);
    streamedTblWithExprOperator->AddInput(streamedVecBatchEof);

    VectorBatch *result = nullptr;
    bufferedTblWithExprOperator->GetOutput(&result);

    // check the join result
    int32_t expectedData0[] = {8001, 8001, 1003, 1003, 8001, 8001, 1003, 1003};
    int32_t expectedData1[] = {8001, 8001, 1003, 1003, 8001, 8001, 1003, 1003};
    AssertVecBatchEquals(result, 2, sizeof(expectedData0) / sizeof(expectedData0[0]), expectedData0, expectedData1);
    VectorHelper::FreeVecBatch(result);

    Expr::DeleteExprs(streamedEqualKeyExprs);
    Expr::DeleteExprs(bufferedEqualKeyExprs);
    omniruntime::op::Operator::DeleteOperator(bufferedTblWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(streamedTblWithExprOperator);
    delete bufferedWithExprOperatorFactory;
    delete streamedWithExprOperatorFactory;
}

// When using SortMergeJoin operator, the call sequence is as follows:
// 1) auto streamOperatorFactory = new StreamedTableWithExprOperatorFactoryV3(...)
// 2) auto bufferOperatorFactory = new BufferedTableWithExprOperatorFactoryV3(...)
// 3) auto streamOperator = streamOperatorFactory->CreateOperator()
// 4) auto bufferOperator = bufferOperatorFactory->CreateOperator()
// 5) streamOperator->AddInput(...)
// 6) bufferOperator->AddInput(...)
// 7) streamOperator->GetOutput(...)
TEST(SortMergeJoinWithExprV3Test, testSmjInner2)
{
    std::string filter = "";
    DataTypes streamTypes(std::vector<DataTypePtr>({IntType(), IntType()}));
    auto streamKey = new FieldExpr(1, IntType());
    std::vector<omniruntime::expressions::Expr *> streamJoinKeys{streamKey};
    std::vector<int32_t> streamOutputCols = {0};
    OperatorConfig operatorConfig;
    auto streamedWithExprOperatorFactory =
        StreamedTableWithExprOperatorFactoryV3::CreateStreamedTableWithExprOperatorFactory(
            streamTypes, streamJoinKeys, streamOutputCols, JoinType::OMNI_JOIN_TYPE_INNER, filter, operatorConfig);
    auto streamedTblWithExprOperator = CreateTestOperator(streamedWithExprOperatorFactory);

    DataTypes bufferTypes(std::vector<DataTypePtr>{IntType(), IntType()});
    auto bufferKey = new FieldExpr(1, IntType());
    std::vector<omniruntime::expressions::Expr *> bufferJoinKeys{bufferKey};
    std::vector<int32_t> bufferedOutputCols = {0};
    auto bufferedWithExprOperatorFactory =
        BufferedTableWithExprOperatorFactoryV3::CreateBufferedTableWithExprOperatorFactory(
            bufferTypes, bufferJoinKeys, bufferedOutputCols, streamedWithExprOperatorFactory, operatorConfig);
    omniruntime::op::Operator *bufferedTblWithExprOperator = CreateTestOperator(bufferedWithExprOperatorFactory);

    int32_t buffer00[] = {6001, 6002};
    int32_t buffer01[] = {3, 1};
    VectorBatch *bufferedVecBatch0 = CreateVectorBatch(bufferTypes, 2, buffer00, buffer01);
    int32_t buffer10[] = {8001, 1003};
    int32_t buffer11[] = {4, 3};
    VectorBatch *bufferedVecBatch1 = CreateVectorBatch(bufferTypes, 2, buffer10, buffer11);
    bufferedTblWithExprOperator->AddInput(bufferedVecBatch0);
    bufferedTblWithExprOperator->AddInput(bufferedVecBatch1);

    int32_t stream00[] = {8001, 8002};
    int32_t stream01[] = {3, 2};
    VectorBatch *streamedVecBatch0 = CreateVectorBatch(streamTypes, 2, stream00, stream01);
    int32_t stream10[] = {8001};
    int32_t stream11[] = {3};
    VectorBatch *streamedVecBatch1 = CreateVectorBatch(streamTypes, 1, stream10, stream11);
    int32_t stream20[] = {8001};
    int32_t stream21[] = {3};
    VectorBatch *streamedVecBatch2 = CreateVectorBatch(streamTypes, 1, stream20, stream21);

    streamedTblWithExprOperator->AddInput(streamedVecBatch0);
    streamedTblWithExprOperator->AddInput(streamedVecBatch1);
    streamedTblWithExprOperator->AddInput(streamedVecBatch2);

    VectorBatch *result = nullptr;
    bufferedTblWithExprOperator->GetOutput(&result);
    VectorHelper::PrintVecBatch(result);

    int32_t expectData0[] = {8001, 8001, 8001, 8001, 8001, 8001};
    int32_t expectData1[] = {6001, 1003, 6001, 1003, 6001, 1003};
    AssertVecBatchEquals(result, 2, 6, expectData0, expectData1);
    VectorHelper::FreeVecBatch(result);

    Expr::DeleteExprs(streamJoinKeys);
    Expr::DeleteExprs(bufferJoinKeys);
    omniruntime::op::Operator::DeleteOperator(bufferedTblWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(streamedTblWithExprOperator);
    delete bufferedWithExprOperatorFactory;
    delete streamedWithExprOperatorFactory;
}

TEST(SortMergeJoinWithExprV3Test, testSmjInner2WithFilter)
{
    std::string filter =
        "{\"exprType\":\"BINARY\",\"returnType\":4,\"operator\":\"EQUAL\",\"left\":{\"exprType\":\"FIELD_REFERENCE\","
        "\"dataType\":1,\"colVal\":0},\"right\":{\"exprType\":\"FIELD_REFERENCE\",\"dataType\":1, \"colVal\":2}}";
    DataTypes streamTypes(std::vector<DataTypePtr>({IntType(), IntType()}));
    auto streamKey = new FieldExpr(1, IntType());
    std::vector<omniruntime::expressions::Expr *> streamJoinKeys{streamKey};
    std::vector<int32_t> streamOutputCols = {0};
    OperatorConfig operatorConfig;
    auto streamedWithExprOperatorFactory =
        StreamedTableWithExprOperatorFactoryV3::CreateStreamedTableWithExprOperatorFactory(
            streamTypes, streamJoinKeys, streamOutputCols, JoinType::OMNI_JOIN_TYPE_INNER, filter, operatorConfig);
    auto streamedTblWithExprOperator = CreateTestOperator(streamedWithExprOperatorFactory);

    DataTypes bufferTypes(std::vector<DataTypePtr>{IntType(), IntType()});
    auto bufferKey = new FieldExpr(1, IntType());
    std::vector<omniruntime::expressions::Expr *> bufferJoinKeys{bufferKey};
    std::vector<int32_t> bufferedOutputCols = {0};
    auto bufferedWithExprOperatorFactory =
        BufferedTableWithExprOperatorFactoryV3::CreateBufferedTableWithExprOperatorFactory(
            bufferTypes, bufferJoinKeys, bufferedOutputCols, streamedWithExprOperatorFactory, operatorConfig);
    omniruntime::op::Operator *bufferedTblWithExprOperator = CreateTestOperator(bufferedWithExprOperatorFactory);

    int32_t buffer00[] = {6001, 6002};
    int32_t buffer01[] = {3, 1};
    VectorBatch *bufferedVecBatch0 = CreateVectorBatch(bufferTypes, 2, buffer00, buffer01);
    int32_t buffer10[] = {8001, 1003};
    int32_t buffer11[] = {4, 3};
    VectorBatch *bufferedVecBatch1 = CreateVectorBatch(bufferTypes, 2, buffer10, buffer11);
    bufferedTblWithExprOperator->AddInput(bufferedVecBatch0);
    bufferedTblWithExprOperator->AddInput(bufferedVecBatch1);

    int32_t stream00[] = {8001, 8002};
    int32_t stream01[] = {3, 2};
    VectorBatch *streamedVecBatch0 = CreateVectorBatch(streamTypes, 2, stream00, stream01);
    int32_t stream10[] = {8001};
    int32_t stream11[] = {3};
    VectorBatch *streamedVecBatch1 = CreateVectorBatch(streamTypes, 1, stream10, stream11);
    int32_t stream20[] = {1003};
    int32_t stream21[] = {3};
    VectorBatch *streamedVecBatch2 = CreateVectorBatch(streamTypes, 1, stream20, stream21);

    streamedTblWithExprOperator->AddInput(streamedVecBatch0);
    streamedTblWithExprOperator->AddInput(streamedVecBatch1);
    streamedTblWithExprOperator->AddInput(streamedVecBatch2);

    VectorBatch *result = nullptr;
    bufferedTblWithExprOperator->GetOutput(&result);
    VectorHelper::PrintVecBatch(result);

    int32_t expectData0[] = {1003};
    int32_t expectData1[] = {1003};
    AssertVecBatchEquals(result, 2, 1, expectData0, expectData1);
    VectorHelper::FreeVecBatch(result);

    Expr::DeleteExprs(streamJoinKeys);
    Expr::DeleteExprs(bufferJoinKeys);
    omniruntime::op::Operator::DeleteOperator(bufferedTblWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(streamedTblWithExprOperator);
    delete bufferedWithExprOperatorFactory;
    delete streamedWithExprOperatorFactory;
}
} // namespace SortMergeJoinWithExprV3Test
