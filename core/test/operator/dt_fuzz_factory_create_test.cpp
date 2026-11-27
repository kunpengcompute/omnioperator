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

#include "util/test_util.h"
#include "util/dt_fuzz_factory_create_util.h"
using namespace omniruntime::type;
using namespace omniruntime::op;
using namespace omniruntime::expressions;
using namespace DtFuzzFactoryCreateUtil;

const int32_t SHORT_DECIMAL_SIZE = 18;
const int32_t LONG_DECIMAL_SIZE = 38;
int32_t CHAR_SIZE = 10;
std::vector<DataTypePtr> supportedTypes = {ShortType(),
                                           IntType(),
                                           LongType(),
                                           BooleanType(),
                                           DoubleType(),
                                           Date32Type(DAY),
                                           Decimal64Type(SHORT_DECIMAL_SIZE, 0),
                                           Decimal128Type(LONG_DECIMAL_SIZE, 0),
                                           VarcharType(CHAR_SIZE),
                                           CharType(CHAR_SIZE)};

TEST(DtFuzzFactoryCreateTest, testFilterFactoryCreate)
{
    DataTypes sourceTypes(supportedTypes);
    auto col1Expr = new FieldExpr(2, LongType());
    auto col2Expr = new FieldExpr(4, DoubleType());
    std::vector<Expr *> projections = {col2Expr, col1Expr};
    auto eqLeft = new FieldExpr(4, DoubleType());
    auto eqRight = new LiteralExpr(50.0, DoubleType());
    auto eqExpr = new BinaryExpr(omniruntime::expressions::Operator::EQ, eqLeft, eqRight, BooleanType());
    auto overflowConfig = new OverflowConfig();
    auto factory = CreateFilterFactory(sourceTypes, eqExpr, projections, overflowConfig);
    ASSERT_FALSE(factory == nullptr);
    delete factory;
    delete overflowConfig;
}

TEST(DtFuzzFactoryCreateTest, testSortFactoryCreate)
{
    DataTypes sourceTypes(supportedTypes);
    auto factory = CreateSortFactory(sourceTypes);
    ASSERT_FALSE(factory == nullptr);
    delete factory;
}

TEST(DtFuzzFactoryCreateTest, testAggreagtionFactoryCreate)
{
    DataTypes sourceTypes(supportedTypes);
    auto factory = CreateAggregationFactory(sourceTypes);
    ASSERT_FALSE(factory == nullptr);
    delete factory;
}

TEST(DtFuzzFactoryCreateTest, testHashAggreagtionFactoryCreate)
{
    DataTypes sourceTypes(supportedTypes);
    auto factory = CreateHashAggregationFactory(sourceTypes);
    ASSERT_FALSE(factory == nullptr);
    delete factory;
}

TEST(DtFuzzFactoryCreateTest, testHashJoinFactoryCreate)
{
    DataTypes sourceTypes(supportedTypes);
    auto overflowConfig = new OverflowConfig();
    auto operatorFactories = CreateHashJoinFactory(sourceTypes, overflowConfig, "", 1);

    ASSERT_FALSE(operatorFactories[0] == nullptr);
    ASSERT_FALSE(operatorFactories[1] == nullptr);
    for (auto operatorFactory : operatorFactories) {
        delete operatorFactory;
    }
    delete overflowConfig;
}

TEST(DtFuzzFactoryCreateTest, testLookupOuterJoinFactoryCreate)
{
    DataTypes sourceTypes(supportedTypes);
    auto overflowConfig = new OverflowConfig();
    auto operatorFactories = CreateLookupOuterFactory(sourceTypes, overflowConfig, "", 1);

    ASSERT_FALSE(operatorFactories[0] == nullptr);
    ASSERT_FALSE(operatorFactories[1] == nullptr);
    for (auto operatorFactory : operatorFactories) {
        delete operatorFactory;
    }
    delete overflowConfig;
}

TEST(DtFuzzFactoryCreateTest, testDistinctLimitFactoryCreate)
{
    DataTypes sourceTypes(supportedTypes);
    const int32_t loopCount = 10;
    auto factory = CreateDistinctLimitFactory(sourceTypes, loopCount);
    ASSERT_FALSE(factory == nullptr);
    delete factory;
}

TEST(DtFuzzFactoryCreateTest, testProjectFactoryCreate)
{
    DataTypes sourceTypes(supportedTypes);
    auto addLeft = new FieldExpr(1, IntType());
    auto addRight = new LiteralExpr(5, IntType());
    auto addExpr = new BinaryExpr(omniruntime::expressions::Operator::ADD, addLeft, addRight, IntType());

    std::vector<Expr *> exprs = {addExpr};
    auto overflowConfig = new OverflowConfig();
    auto factory = CreateProjectFactory(sourceTypes, exprs, overflowConfig);
    ASSERT_FALSE(factory == nullptr);
    delete factory;
    delete overflowConfig;
}

TEST(DtFuzzFactoryCreateTest, testUnionFactoryCreate)
{
    DataTypes sourceTypes(supportedTypes);
    auto factory = CreateUnionFactory(sourceTypes);
    ASSERT_FALSE(factory == nullptr);
    delete factory;
}

TEST(DtFuzzFactoryCreateTest, testSortMergeJoinOperatorCreate)
{
    DataTypes sourceTypes(supportedTypes);
    auto overfolwCOnfig = new OverflowConfig();
    auto smjOperator = CreateSortMergeJoinOperator(sourceTypes, overfolwCOnfig);
    ASSERT_FALSE(smjOperator == nullptr);
    delete smjOperator;
    delete overfolwCOnfig;
}

TEST(DtFuzzFactoryCreateTest, testTopNFactoryCreate)
{
    DataTypes sourceTypes(supportedTypes);
    auto factory = CreateTopNFactory(sourceTypes);
    ASSERT_FALSE(factory == nullptr);
    delete factory;
}

TEST(DtFuzzFactoryCreateTest, testWindowFactoryCreate)
{
    DataTypes sourceTypes(supportedTypes);
    auto factory = CreateWindowFactory(sourceTypes);
    ASSERT_FALSE(factory == nullptr);
    delete factory;
}

TEST(DtFuzzFactoryCreateTest, testTopNSortFactoryCreate)
{
    DataTypes sourceTypes(supportedTypes);
    std::vector<omniruntime::expressions::Expr *> partitionKeys = {new FieldExpr(8, VarcharType(CHAR_SIZE))};
    std::vector<omniruntime::expressions::Expr *> sortKeys = {new FieldExpr(1, IntType())};
    auto factory = CreateTopNSortFactory(sourceTypes, partitionKeys, sortKeys, 10);
    ASSERT_FALSE(factory == nullptr);
    Expr::DeleteExprs(partitionKeys);
    Expr::DeleteExprs(sortKeys);
    delete factory;
}

TEST(DtFuzzFactoryCreateTest, testStreamedTableWithExprOperatorFactoryCreate)
{
    auto factory = CreateStreamedWithExprOperatorFactory();
    ASSERT_FALSE(factory == nullptr);
    delete factory;
}

TEST(DtFuzzFactoryCreateTest, testSortMergeJoinV3FactoryCreate)
{
    StreamedTableWithExprOperatorFactoryV3 *streamedTableWithExprOperatorFactoryV3 =
        dynamic_cast<StreamedTableWithExprOperatorFactoryV3 *>(CreateStreamedWithExprOperatorFactory());
    auto factory = CreateSortMergeJoinV3Factory(streamedTableWithExprOperatorFactoryV3);
    ASSERT_FALSE(factory == nullptr);
    delete streamedTableWithExprOperatorFactoryV3;
    delete factory;
}
