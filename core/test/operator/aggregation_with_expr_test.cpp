/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2024. All rights reserved.
 */

#include <gtest/gtest.h>
#include "operator/aggregation/aggregation.h"
#include "operator/aggregation/aggregator/aggregator_util.h"
#include "operator/aggregation/group_aggregation_expr.h"
#include "operator/aggregation/non_group_aggregation_expr.h"
#include "vector/vector_helper.h"
#include "util/test_util.h"
#include "util/config_util.h"
#include "util/type_util.h"

namespace omniruntime {
using namespace omniruntime::vec;
using namespace omniruntime::op;
using namespace TestUtil;
using namespace omniruntime::expressions;

TEST(HashAggregationWithExprOperatorTest, test_hashagg_partial_expr)
{
    ConfigUtil::SetEnableBatchExprEvaluate(false);

    const int32_t dataSize = 8;
    const int32_t groupByNum = 2;
    const int32_t expectDataSize = 1;

    // prepare data
    // sum(c1*5), sum(c3) group by c0%3, c2 => 2, 5, 180, 36
    int64_t data1[] = {2L, 5L, 8L, 11L, 14L, 17L, 20L, 23L}; // c0
    int64_t data2[] = {5L, 3L, 2L, 6L, 1L, 4L, 7L, 8L};      // c1
    int32_t data3[] = {5, 5, 5, 5, 5, 5, 5, 5};              // c2
    int32_t data4[] = {5, 3, 2, 6, 1, 4, 7, 8};              // c3

    DataTypes sourceTypes(std::vector<DataTypePtr>({ LongType(), LongType(), IntType(), IntType() }));
    DataTypes aggOutputTypes(std::vector<DataTypePtr>({ LongType(), IntType() }));
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3, data4);

    // groupByKeys
    LiteralExpr *modRight = new LiteralExpr(3, LongType());
    modRight->longVal = 3;
    BinaryExpr *modExpr =
        new BinaryExpr(omniruntime::expressions::Operator::MOD, new FieldExpr(0, LongType()), modRight, LongType());
    std::vector<Expr *> groupByKeys = { modExpr, new FieldExpr(2, IntType()) };

    // aggKeys
    LiteralExpr *mulRight = new LiteralExpr(5, LongType());
    mulRight->longVal = 5;
    BinaryExpr *mulExpr =
        new BinaryExpr(omniruntime::expressions::Operator::MUL, new FieldExpr(1, LongType()), mulRight, LongType());
    std::vector<Expr *> aggKeys1 = { mulExpr };
    std::vector<Expr *> aggKeys2 = { new FieldExpr(3, IntType()) };
    std::vector<std::vector<omniruntime::expressions::Expr *>> aggAllKeys = { aggKeys1, aggKeys2 };

    std::vector<uint32_t> aggFuncTypes = { OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_SUM };
    std::vector<uint32_t> maskCols = { static_cast<uint32_t>(-1), static_cast<uint32_t>(-1) };

    auto aggOutputTypesWrap = AggregatorUtil::WrapWithVector(aggOutputTypes);
    auto inputRawWrap = std::vector<bool>(aggFuncTypes.size(), true);
    auto outputPartialWrap = std::vector<bool>(aggFuncTypes.size(), false);
    std::vector<omniruntime::expressions::Expr *> aggFilters;
    auto *hashAggWithExprOperatorFactory =
        new HashAggregationWithExprOperatorFactory(groupByKeys, groupByNum, aggAllKeys, aggFilters, sourceTypes,
        aggOutputTypesWrap, aggFuncTypes, maskCols, inputRawWrap, outputPartialWrap, OperatorConfig());
    auto *hashAggWithExprOperator =
        dynamic_cast<HashAggregationWithExprOperator *>(CreateTestOperator(hashAggWithExprOperatorFactory));

    hashAggWithExprOperator->AddInput(vecBatch);
    VectorBatch *outputVecBatch = nullptr;
    hashAggWithExprOperator->GetOutput(&outputVecBatch);

    int64_t expData1[] = {2};
    int32_t expData2[] = {5};
    int64_t expData3[] = {180};
    int32_t expData4[] = {36};
    DataTypes expectTypes(std::vector<DataTypePtr>({ LongType(), IntType(), LongType(), IntType() }));
    VectorBatch *expectVecorBatch =
        CreateVectorBatch(expectTypes, expectDataSize, expData1, expData2, expData3, expData4);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecorBatch));

    Expr::DeleteExprs(groupByKeys);
    Expr::DeleteExprs(aggAllKeys);
    omniruntime::op::Operator::DeleteOperator(hashAggWithExprOperator);
    delete hashAggWithExprOperatorFactory;
    VectorHelper::FreeVecBatch(expectVecorBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
}

TEST(HashAggregationWithExprOperatorTest, test_hashagg_full_expr)
{
    using namespace omniruntime::expressions;

    const int32_t dataSize = 8;
    const int32_t groupByNum = 2;
    const int32_t expectDataSize = 1;

    // prepare data
    // sum(c1*5), sum(c3+5) group by c0%3, c2+5  => 2, 10, 180, 76
    int64_t data1[] = {2L, 5L, 8L, 11L, 14L, 17L, 20L, 23L};
    int64_t data2[] = {5L, 3L, 2L, 6L, 1L, 4L, 7L, 8L};
    int32_t data3[] = {5, 5, 5, 5, 5, 5, 5, 5};
    int32_t data4[] = {5, 3, 2, 6, 1, 4, 7, 8};

    DataTypes sourceTypes(std::vector<DataTypePtr>({ LongType(), LongType(), IntType(), IntType() }));
    DataTypes aggOutputTypes(std::vector<DataTypePtr>({ LongType(), IntType() }));
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3, data4);

    FieldExpr *modLeft = new FieldExpr(0, LongType());
    LiteralExpr *modRight = new LiteralExpr(3, LongType());
    modRight->longVal = 3;
    BinaryExpr *modExpr = new BinaryExpr(omniruntime::expressions::Operator::MOD, modLeft, modRight, LongType());
    FieldExpr *addLeft = new FieldExpr(2, IntType());
    LiteralExpr *addRight = new LiteralExpr(5, IntType());
    BinaryExpr *addExpr = new BinaryExpr(omniruntime::expressions::Operator::ADD, addLeft, addRight, IntType());
    std::vector<Expr *> groupByKeys = { modExpr, addExpr };

    FieldExpr *mulLeft = new FieldExpr(1, LongType());
    LiteralExpr *mulRight = new LiteralExpr(5, LongType());
    mulRight->longVal = 5;
    BinaryExpr *mulExpr = new BinaryExpr(omniruntime::expressions::Operator::MUL, mulLeft, mulRight, LongType());
    FieldExpr *addLeft2 = new FieldExpr(3, IntType());
    LiteralExpr *addRight2 = new LiteralExpr(5, IntType());
    BinaryExpr *addExpr2 = new BinaryExpr(omniruntime::expressions::Operator::ADD, addLeft2, addRight2, IntType());

    std::vector<Expr *> aggKeys1 = { mulExpr };
    std::vector<Expr *> aggKeys2 = { addExpr2 };
    std::vector<std::vector<omniruntime::expressions::Expr *>> aggAllKeys = { aggKeys1, aggKeys2 };
    std::vector<uint32_t> aggFuncTypes = { OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_SUM };
    std::vector<uint32_t> maskCols = { static_cast<uint32_t>(-1), static_cast<uint32_t>(-1) };

    auto aggOutputTypesWrap = AggregatorUtil::WrapWithVector(aggOutputTypes);
    auto inputRawWrap = std::vector<bool>(aggFuncTypes.size(), true);
    auto outputPartialWrap = std::vector<bool>(aggFuncTypes.size(), false);
    std::vector<omniruntime::expressions::Expr *> aggFilters;
    aggFilters.reserve(0);

    auto hashAggWithExprOperatorFactory =
        new HashAggregationWithExprOperatorFactory(groupByKeys, groupByNum, aggAllKeys, aggFilters, sourceTypes,
        aggOutputTypesWrap, aggFuncTypes, maskCols, inputRawWrap, outputPartialWrap, OperatorConfig());
    auto *hashAggWithExprOperator =
        dynamic_cast<HashAggregationWithExprOperator *>(CreateTestOperator(hashAggWithExprOperatorFactory));

    hashAggWithExprOperator->AddInput(vecBatch);
    VectorBatch *outputVecBatch = nullptr;
    hashAggWithExprOperator->GetOutput(&outputVecBatch);

    int64_t expData1[] = {2};
    int32_t expData2[] = {10};
    int64_t expData3[] = {180};
    int32_t expData4[] = {76};
    DataTypes expectTypes(std::vector<DataTypePtr>({ LongType(), IntType(), LongType(), IntType() }));
    VectorBatch *expectVecorBatch =
        CreateVectorBatch(expectTypes, expectDataSize, expData1, expData2, expData3, expData4);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecorBatch));

    Expr::DeleteExprs(groupByKeys);
    Expr::DeleteExprs(aggAllKeys);
    omniruntime::op::Operator::DeleteOperator(hashAggWithExprOperator);
    delete hashAggWithExprOperatorFactory;
    VectorHelper::FreeVecBatch(expectVecorBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
}

TEST(HashAggregationWithExprOperatorTest, test_hashagg_groupby_array)
{
    using namespace omniruntime::expressions;

    const int32_t dataSize = 8;
    const int32_t groupByNum = 1;
    const int32_t expectDataSize = 1;

    int64_t data1[dataSize];
    int64_t data2[dataSize];
    int32_t data3[dataSize];
    int32_t data4[dataSize];
    for (int i = 0; i < dataSize; ++i) {
        data1[i] = 10 + i;
        data2[i] = 20 + i;
        data3[i] = 30 + i;
        data4[i] = 40 + i;
    }

    auto elementVector = std::make_shared<Vector<int32_t>>(16, OMNI_INT);
    int vals[16] = {1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2};
    for (int i = 0; i < 16; i++) {
        elementVector->SetValue(i, vals[i]);
    }
    auto arrayVec = new ArrayVector(dataSize, elementVector);
    for (int row = 0; row < dataSize; row++) {
        arrayVec->SetOffset(row, row*2);
        arrayVec->SetSize(row, 2);
    }

    auto arrayColType = std::make_shared<ArrayType>(IntType());
    arrayColType->SetElementSize(8);
    DataTypes sourceTypes({ LongType(), LongType(), IntType(), IntType() });
    VectorBatch* vecBatch = CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3, data4);
    vecBatch->Append(arrayVec);

    FieldExpr *arrayField = new FieldExpr(4, arrayColType);
    std::vector<Expr*> groupByKeys = { arrayField };

    FieldExpr *mulLeft = new FieldExpr(0, LongType());
    LiteralExpr *mulRight = new LiteralExpr(5, LongType());
    mulRight->longVal = 5;
    BinaryExpr *mulExpr = new BinaryExpr(omniruntime::expressions::Operator::MUL, mulLeft, mulRight, LongType());

    FieldExpr *addLeft = new FieldExpr(2, IntType());
    LiteralExpr *addRight = new LiteralExpr(5, IntType());
    BinaryExpr *addExpr = new BinaryExpr(omniruntime::expressions::Operator::ADD, addLeft, addRight, IntType());

    std::vector<std::vector<Expr *>> aggAllKeys = { {mulExpr},  {addExpr} };
    std::vector<uint32_t> aggFuncTypes = { OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_SUM };
    std::vector<uint32_t> maskCols = { static_cast<uint32_t>(-1), static_cast<uint32_t>(-1) };

    DataTypes aggOutputTypes({ LongType(), LongType() });
    auto aggOutputTypesWrap = AggregatorUtil::WrapWithVector(aggOutputTypes);
    auto inputRawWrap = std::vector<bool>(aggFuncTypes.size(), true);
    auto outputPartialWrap = std::vector<bool>(aggFuncTypes.size(), false);
    std::vector<Expr *> aggFilters;
    DataTypes sourceTypesFull({ LongType(), LongType(), IntType(), IntType(),  arrayColType });

    auto hashAggWithExprOperatorFactory =
        new HashAggregationWithExprOperatorFactory(groupByKeys, groupByNum, aggAllKeys, aggFilters, sourceTypesFull,
        aggOutputTypesWrap, aggFuncTypes, maskCols, inputRawWrap, outputPartialWrap, OperatorConfig());
    auto *hashAggWithExprOperator =
        dynamic_cast<HashAggregationWithExprOperator *>(CreateTestOperator(hashAggWithExprOperatorFactory));

    hashAggWithExprOperator->AddInput(vecBatch);
    VectorBatch *outputVecBatch = nullptr;
    hashAggWithExprOperator->GetOutput(&outputVecBatch);

    int64_t expData1[] = { 540 };
    int32_t expData2[] = { 308 };
    DataTypes expectTypes({ arrayColType, LongType(), LongType() });
    auto elementVectorExp = std::make_shared<Vector<int32_t>>(2, OMNI_INT);
    int valsExp[2] = {1, 2};
    for(int i = 0; i < 2; i++){
        elementVectorExp->SetValue(i, valsExp[i]);
    }
    auto arrayVecExp = new ArrayVector(expectDataSize, elementVectorExp);
    arrayVecExp->SetOffset(0, 0);
    arrayVecExp->SetSize(0, 2);

    auto col1 = new Vector<int64_t>(expectDataSize);
    auto col2 = new Vector<int64_t>(expectDataSize);
    col1->SetValue(0, expData1[0]);
    col2->SetValue(0, expData2[0]);

    auto expectVecBatch = new VectorBatch(expectDataSize);
    expectVecBatch->Append(arrayVecExp);
    expectVecBatch->Append(col1);
    expectVecBatch->Append(col2);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecBatch));

    Expr::DeleteExprs(groupByKeys);
    Expr::DeleteExprs(aggAllKeys);
    omniruntime::op::Operator::DeleteOperator(hashAggWithExprOperator);
    delete hashAggWithExprOperatorFactory;
    VectorHelper::FreeVecBatch(expectVecBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
}

TEST(HashAggregationWithExprOperatorTest, test_hashagg_no_expr)
{
    using namespace omniruntime::expressions;

    const int32_t dataSize = 8;
    const int32_t groupByNum = 2;
    const int32_t expectDataSize = 1;

    // prepare data
    // sum(c1), sum(c3) count(*) group by c0, c2  => 2, 5, 36, 36, 8
    int64_t data1[] = {2L, 2L, 2L, 2L, 2L, 2L, 2L, 2L}; // c0
    int64_t data2[] = {5L, 3L, 2L, 6L, 1L, 4L, 7L, 8L}; // c1
    int32_t data3[] = {5, 5, 5, 5, 5, 5, 5, 5};         // c2
    int32_t data4[] = {5, 3, 2, 6, 1, 4, 7, 8};         // c3

    DataTypes sourceTypes(std::vector<DataTypePtr>({ LongType(), LongType(), IntType(), IntType() }));
    DataTypes aggOutputTypes(std::vector<DataTypePtr>({ LongType(), IntType(), LongType() }));
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3, data4);

    std::vector<Expr *> groupByKeys = { new FieldExpr(0, LongType()), new FieldExpr(2, IntType()) };

    std::vector<Expr *> aggKeys1 = { new FieldExpr(1, LongType()) };
    std::vector<Expr *> aggKeys2 = { new FieldExpr(3, IntType()) };
    std::vector<std::vector<omniruntime::expressions::Expr *>> aggAllKeys = { aggKeys1, aggKeys2 };
    std::vector<uint32_t> aggFuncTypes = { OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_SUM,
        OMNI_AGGREGATION_TYPE_COUNT_ALL };
    std::vector<uint32_t> maskCols = { static_cast<uint32_t>(-1), static_cast<uint32_t>(-1),
        static_cast<uint32_t>(-1) };

    auto aggOutputTypesWrap = AggregatorUtil::WrapWithVector(aggOutputTypes);
    auto inputRawWrap = std::vector<bool>(aggFuncTypes.size(), true);
    auto outputPartialWrap = std::vector<bool>(aggFuncTypes.size(), false);
    std::vector<omniruntime::expressions::Expr *> aggFilters;
    aggFilters.reserve(0);

    auto hashAggWithExprOperatorFactory =
        new HashAggregationWithExprOperatorFactory(groupByKeys, groupByNum, aggAllKeys, aggFilters, sourceTypes,
        aggOutputTypesWrap, aggFuncTypes, maskCols, inputRawWrap, outputPartialWrap, OperatorConfig());
    auto hashAggWithExprOperator =
        dynamic_cast<HashAggregationWithExprOperator *>(CreateTestOperator(hashAggWithExprOperatorFactory));

    hashAggWithExprOperator->AddInput(vecBatch);
    VectorBatch *outputVecBatch = nullptr;
    hashAggWithExprOperator->GetOutput(&outputVecBatch);

    int64_t expData1[] = {2};
    int32_t expData2[] = {5};
    int64_t expData3[] = {36};
    int32_t expData4[] = {36};
    int64_t expData5[]={8};
    DataTypes expectTypes(std::vector<DataTypePtr>({ LongType(), IntType(), LongType(), IntType(), LongType() }));
    VectorBatch *expectVecorBatch =
        CreateVectorBatch(expectTypes, expectDataSize, expData1, expData2, expData3, expData4, expData5);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecorBatch));

    Expr::DeleteExprs(groupByKeys);
    Expr::DeleteExprs(aggAllKeys);
    omniruntime::op::Operator::DeleteOperator(hashAggWithExprOperator);
    delete hashAggWithExprOperatorFactory;
    VectorHelper::FreeVecBatch(expectVecorBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
}

TEST(HashAggregationWithExprOperatorTest, test_hashagg_partial_flat_output_expr)
{
    ConfigUtil::SetSupportContainerVecRule(SupportContainerVecRule::NOT_SUPPORT);
    const int32_t dataSize = 8;
    const int32_t groupByNum = 2;
    const int32_t expectDataSize = 1;

    // prepare data
    // avg(c1*5), avg(c3) group by c0%3, c2 => 2, 10, 23, 32
    int64_t data1[] = {2L, 5L, 8L, 11L, 14L, 17L, 20L, 23L}; // c0
    int64_t data2[] = {5L, 3L, 2L, 6L, 5L, 4L, 7L, 8L};      // c1
    int32_t data3[] = {5, 5, 5, 5, 5, 5, 5, 5};              // c2
    int32_t data4[] = {5, 1, 2, 6, 1, 4, 7, 8};              // c3

    DataTypes sourceTypes(std::vector<DataTypePtr>({ LongType(), LongType(), IntType(), IntType() }));
    DataTypes aggOutputTypes1(std::vector<DataTypePtr>({ DoubleType(), LongType() }));
    DataTypes aggOutputTypes2(std::vector<DataTypePtr>({ DoubleType(), LongType() }));
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3, data4);

    // groupByKeys
    LiteralExpr *modRight = new LiteralExpr(3, LongType());
    modRight->longVal = 3;
    BinaryExpr *modExpr =
        new BinaryExpr(omniruntime::expressions::Operator::MOD, new FieldExpr(0, LongType()), modRight, LongType());
    std::vector<Expr *> groupByKeys = { modExpr, new FieldExpr(2, IntType()) };

    // aggKeys
    LiteralExpr *mulRight = new LiteralExpr(5, LongType());
    mulRight->longVal = 5;
    BinaryExpr *mulExpr =
        new BinaryExpr(omniruntime::expressions::Operator::MUL, new FieldExpr(1, LongType()), mulRight, LongType());
    std::vector<Expr *> aggKeys1 = { mulExpr };
    std::vector<Expr *> aggKeys2 = { new FieldExpr(3, IntType()) };
    std::vector<std::vector<omniruntime::expressions::Expr *>> aggAllKeys = { aggKeys1, aggKeys2 };

    std::vector<uint32_t> aggFuncTypes = { OMNI_AGGREGATION_TYPE_AVG, OMNI_AGGREGATION_TYPE_AVG };
    std::vector<uint32_t> maskCols = { static_cast<uint32_t>(-1), static_cast<uint32_t>(-1) };

    std::vector<DataTypes> aggsOutputTypes = { aggOutputTypes1, aggOutputTypes2 };
    auto inputRawWrap = std::vector<bool>(aggFuncTypes.size(), true);
    auto outputPartialWrap = std::vector<bool>(aggFuncTypes.size(), true);
    std::vector<omniruntime::expressions::Expr *> aggFilters;
    aggFilters.reserve(2);
    aggFilters.push_back(nullptr);
    aggFilters.push_back(nullptr);

    auto *hashAggWithExprOperatorFactory =
        new HashAggregationWithExprOperatorFactory(groupByKeys, groupByNum, aggAllKeys, aggFilters, sourceTypes,
        aggsOutputTypes, aggFuncTypes, maskCols, inputRawWrap, outputPartialWrap, OperatorConfig());
    auto *hashAggWithExprOperator =
        dynamic_cast<HashAggregationWithExprOperator *>(CreateTestOperator(hashAggWithExprOperatorFactory));

    hashAggWithExprOperator->AddInput(vecBatch);
    VectorBatch *outputVecBatch = nullptr;
    hashAggWithExprOperator->GetOutput(&outputVecBatch);

    int64_t expData1[] = {2};
    int32_t expData2[] = {5};
    double expData3[] = {200};
    int64_t expData4[] = {8};
    double expData5[] = {34};
    int64_t expData6[] = {8};
    DataTypes expectTypes(
        std::vector<DataTypePtr>({ LongType(), IntType(), DoubleType(), LongType(), DoubleType(), LongType() }));
    VectorBatch *expectVecorBatch =
        CreateVectorBatch(expectTypes, expectDataSize, expData1, expData2, expData3, expData4, expData5, expData6);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecorBatch));

    Expr::DeleteExprs(groupByKeys);
    Expr::DeleteExprs(aggAllKeys);
    omniruntime::op::Operator::DeleteOperator(hashAggWithExprOperator);
    delete hashAggWithExprOperatorFactory;
    VectorHelper::FreeVecBatch(expectVecorBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
    ConfigUtil::SetSupportContainerVecRule(SupportContainerVecRule::SUPPORT);
}

TEST(HashAggregationWithExprOperatorTest, test_float_and_double_with_aggregatior_partial)
{
    ConfigUtil::SetSupportContainerVecRule(SupportContainerVecRule::NOT_SUPPORT);
    const int32_t dataSize = 8;
    const int32_t groupByNum = 2;
    const int32_t expectDataSize = 1;
    // prepare data
    // avg(c1*5), avg(c3) group by c0%3, c2 => 2, 5, 200, 40
    int64_t data1[] = {2L, 5L, 8L, 11L, 14L, 17L, 20L, 23L}; // c0
    double data2[] = {5.0, 4.0, 3.0, 2.0, 5.0, 8.0, 7.0, 6.0};      // c1
    int32_t data3[] = {5, 5, 5, 5, 5, 5, 5, 5};              // c2
    float data4[] = {1.5f, 2.5f, 3.5f, 4.5f, 5.5f, 6.5f, 7.5f, 8.5f}; //c3

    DataTypes sourceTypes(std::vector<DataTypePtr>({ LongType(), DoubleType(), IntType(), FloatType() }));
    DataTypes aggOutputTypes1(std::vector<DataTypePtr>({ DoubleType(), LongType() }));
    DataTypes aggOutputTypes2(std::vector<DataTypePtr>({ DoubleType(), LongType() }));
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3, data4);

    // groupByKeys
    LiteralExpr *modRight = new LiteralExpr(3, LongType());
    modRight->longVal = 3;
    BinaryExpr *modExpr =
        new BinaryExpr(omniruntime::expressions::Operator::MOD, new FieldExpr(0, LongType()), modRight, LongType());
    std::vector<Expr *> groupByKeys = { modExpr, new FieldExpr(2, IntType()) };

    // aggKeys
    LiteralExpr *mulRight = new LiteralExpr(5, DoubleType());
    mulRight->doubleVal = 5;
    BinaryExpr *mulExpr =
        new BinaryExpr(omniruntime::expressions::Operator::MUL, new FieldExpr(1, DoubleType()), mulRight, DoubleType());
    std::vector<Expr *> aggKeys1 = { mulExpr };
    std::vector<Expr *> aggKeys2 = { new FieldExpr(3, FloatType()) };
    std::vector<std::vector<omniruntime::expressions::Expr *>> aggAllKeys = { aggKeys1, aggKeys2 };

    std::vector<uint32_t> aggFuncTypes = { OMNI_AGGREGATION_TYPE_AVG, OMNI_AGGREGATION_TYPE_AVG };
    std::vector<uint32_t> maskCols = { static_cast<uint32_t>(-1), static_cast<uint32_t>(-1) };

    std::vector<DataTypes> aggsOutputTypes = { aggOutputTypes1, aggOutputTypes2 };
    auto inputRawWrap = std::vector<bool>(aggFuncTypes.size(), true);
    auto outputPartialWrap = std::vector<bool>(aggFuncTypes.size(), true);
    std::vector<omniruntime::expressions::Expr *> aggFilters;
    aggFilters.reserve(2);
    aggFilters.push_back(nullptr);
    aggFilters.push_back(nullptr);

    auto *hashAggWithExprOperatorFactory =
        new HashAggregationWithExprOperatorFactory(groupByKeys, groupByNum, aggAllKeys, aggFilters, sourceTypes,
        aggsOutputTypes, aggFuncTypes, maskCols, inputRawWrap, outputPartialWrap, OperatorConfig());
    auto *hashAggWithExprOperator =
        dynamic_cast<HashAggregationWithExprOperator *>(CreateTestOperator(hashAggWithExprOperatorFactory));

    hashAggWithExprOperator->AddInput(vecBatch);
    VectorBatch *outputVecBatch = nullptr;
    hashAggWithExprOperator->GetOutput(&outputVecBatch);

    int64_t expData1[] = {2};
    int32_t expData2[] = {5};
    double expData3[] = {200.0};
    int64_t expData4[] = {8};
    double expData5[] = {40.0};
    int64_t expData6[] = {8};
    DataTypes expectTypes(
        std::vector<DataTypePtr>({ LongType(), IntType(), DoubleType(), LongType(), DoubleType(), LongType() }));
    VectorBatch *expectVecorBatch =
        CreateVectorBatch(expectTypes, expectDataSize, expData1, expData2, expData3, expData4, expData5, expData6);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecorBatch));

    Expr::DeleteExprs(groupByKeys);
    Expr::DeleteExprs(aggAllKeys);
    omniruntime::op::Operator::DeleteOperator(hashAggWithExprOperator);
    delete hashAggWithExprOperatorFactory;
    VectorHelper::FreeVecBatch(expectVecorBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
    ConfigUtil::SetSupportContainerVecRule(SupportContainerVecRule::SUPPORT);
}

TEST(HashAggregationWithExprOperatorTest, stddev_samp)
{
    ConfigUtil::SetSupportContainerVecRule(SupportContainerVecRule::NOT_SUPPORT);
    const int32_t dataSize = 8;
    const int32_t groupByNum = 1;
    const int32_t expectDataSize = 1;

    double data1[] = {2, 5, 8, 11, 14, 17, 20, 23};
    double data2[] = {5, 3, 2, 6, 5, 4, 7, 8};
    double data3[] = {5, 5, 5, 5, 5, 5, 5, 5};
    float data4[] = {1.5f, 2.5f, 3.5f, 4.5f, 5.5f, 6.5f, 7.5f, 8.5f};

    DataTypes sourceTypes(std::vector<DataTypePtr>({DoubleType(), DoubleType(), DoubleType(),
        FloatType()}));
    DataTypes aggOutputTypes1(std::vector<DataTypePtr>({DoubleType(), DoubleType(), DoubleType()}));
    DataTypes aggOutputTypes2(std::vector<DataTypePtr>({DoubleType(), DoubleType(), DoubleType()}));
    DataTypes aggOutputTypes3(std::vector<DataTypePtr>({DoubleType(), DoubleType(), DoubleType()}));
    DataTypes aggOutputTypes4(std::vector<DataTypePtr>({DoubleType(), DoubleType(), DoubleType()}));
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3, data4);

    // aggKeys
    std::vector<Expr *> groupByKeys = {new FieldExpr(2, DoubleType())};
    std::vector<Expr *> aggKeys1 = {new FieldExpr(0, DoubleType())};
    std::vector<Expr *> aggKeys2 = {new FieldExpr(1, DoubleType())};
    std::vector<Expr *> aggKeys3 = {new FieldExpr(3, FloatType())};
    LiteralExpr *mulRight = new LiteralExpr(5, FloatType());
    mulRight->floatVal = 5;
    BinaryExpr *mulExpr =
        new BinaryExpr(omniruntime::expressions::Operator::MUL, new FieldExpr(3, FloatType()), mulRight, FloatType());
    std::vector<Expr *> aggKeys4 = { mulExpr };
    std::vector<std::vector<omniruntime::expressions::Expr *>> aggAllKeys = {aggKeys1, aggKeys2,
        aggKeys3, aggKeys4};

    std::vector<uint32_t> aggFuncTypes = {OMNI_AGGREGATION_TYPE_SAMP, OMNI_AGGREGATION_TYPE_SAMP,
        OMNI_AGGREGATION_TYPE_SAMP, OMNI_AGGREGATION_TYPE_SAMP};
    std::vector<uint32_t> maskCols = {static_cast<uint32_t>(-1), static_cast<uint32_t>(-1),
        static_cast<uint32_t>(-1), static_cast<uint32_t>(-1)};

    std::vector<DataTypes> aggsOutputTypes = {aggOutputTypes1, aggOutputTypes2,
        aggOutputTypes3, aggOutputTypes4};
    auto inputRawWrap = std::vector<bool>(aggFuncTypes.size(), true);
    auto outputPartialWrap = std::vector<bool>(aggFuncTypes.size(), true);
    std::vector<omniruntime::expressions::Expr *> aggFilters;
    aggFilters.reserve(2);
    aggFilters.push_back(nullptr);
    aggFilters.push_back(nullptr);

    auto *hashAggWithExprOperatorFactory =
        new HashAggregationWithExprOperatorFactory(groupByKeys, groupByNum, aggAllKeys, aggFilters, sourceTypes,
            aggsOutputTypes, aggFuncTypes, maskCols, inputRawWrap, outputPartialWrap, OperatorConfig());
    auto *hashAggWithExprOperator =
        dynamic_cast<HashAggregationWithExprOperator *>(CreateTestOperator(hashAggWithExprOperatorFactory));

    hashAggWithExprOperator->AddInput(vecBatch);
    VectorBatch *outputVecBatch = nullptr;
    hashAggWithExprOperator->GetOutput(&outputVecBatch);

    std::vector<Expr *> groupByKeysFinal = {new FieldExpr(0, DoubleType())};
    std::vector<Expr *> aggKeys1Final = {new FieldExpr(1, DoubleType()), new FieldExpr(2, DoubleType()),
                                         new FieldExpr(3, DoubleType())};
    std::vector<Expr *> aggKeys2Final = {new FieldExpr(4, DoubleType()), new FieldExpr(5, DoubleType()),
                                         new FieldExpr(6, DoubleType())};
    std::vector<Expr *> aggKeys3Final = {new FieldExpr(7, DoubleType()), new FieldExpr(8, DoubleType()),
                                         new FieldExpr(9, DoubleType())};
    std::vector<Expr *> aggKeys4Final = {new FieldExpr(10, DoubleType()), new FieldExpr(11, DoubleType()),
                                     new FieldExpr(12, DoubleType())};
    std::vector<std::vector<omniruntime::expressions::Expr *>> aggAllKeysFinal = {aggKeys1Final, aggKeys2Final,
        aggKeys3Final, aggKeys4Final};
    DataTypes sourceTypesFinal(std::vector<DataTypePtr>(
        {DoubleType(), DoubleType(), DoubleType(), DoubleType(), DoubleType(), DoubleType(), DoubleType(),
            DoubleType(), DoubleType(), DoubleType(), DoubleType(), DoubleType(), DoubleType()}));

    auto inputRawFinalWrap = std::vector<bool>(aggFuncTypes.size(), false);
    auto outputFinalWrap = std::vector<bool>(aggFuncTypes.size(), false);
    DataTypes aggOutputTypesFinal1(std::vector<DataTypePtr>({DoubleType()}));
    DataTypes aggOutputTypesFinal2(std::vector<DataTypePtr>({DoubleType()}));
    DataTypes aggOutputTypesFinal3(std::vector<DataTypePtr>({DoubleType()}));
    DataTypes aggOutputTypesFinal4(std::vector<DataTypePtr>({DoubleType()}));
    std::vector<DataTypes> aggsOutputTypesFinal = {
        aggOutputTypesFinal1, aggOutputTypesFinal2, aggOutputTypesFinal3, aggOutputTypesFinal4
    };
    auto *hashAggWithExprOperatorFactoryFinal =
        new HashAggregationWithExprOperatorFactory(groupByKeysFinal, groupByNum, aggAllKeysFinal, aggFilters,
            sourceTypesFinal, aggsOutputTypesFinal, aggFuncTypes, maskCols, inputRawFinalWrap, outputFinalWrap,
            OperatorConfig());
    auto *hashAggWithExprOperatorFinal =
        dynamic_cast<HashAggregationWithExprOperator *>(CreateTestOperator(hashAggWithExprOperatorFactoryFinal));

    hashAggWithExprOperatorFinal->AddInput(outputVecBatch);
    VectorBatch *outputVecBatchFinal = nullptr;
    hashAggWithExprOperatorFinal->GetOutput(&outputVecBatchFinal);

    double expData1[] = {5};
    double expData2[] = {7.3484692283495345};
    double expData3[] = {2};
    double expData4[] = {2.4494897427831781};
    double expData5[] = {12.247448713915891};
    DataTypes expectTypes(std::vector<DataTypePtr>({DoubleType(), DoubleType(), DoubleType(), DoubleType(), DoubleType()}));
    VectorBatch *expectVectorBatch = CreateVectorBatch(expectTypes, expectDataSize, expData1, expData2, expData3,
        expData4, expData5);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatchFinal, expectVectorBatch));

    Expr::DeleteExprs(groupByKeys);
    Expr::DeleteExprs(aggAllKeys);
    Expr::DeleteExprs(groupByKeysFinal);
    Expr::DeleteExprs(aggAllKeysFinal);
    omniruntime::op::Operator::DeleteOperator(hashAggWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(hashAggWithExprOperatorFinal);
    delete hashAggWithExprOperatorFactory;
    delete hashAggWithExprOperatorFactoryFinal;
    VectorHelper::FreeVecBatch(expectVectorBatch);
    VectorHelper::FreeVecBatch(outputVecBatchFinal);
    ConfigUtil::SetSupportContainerVecRule(SupportContainerVecRule::SUPPORT);
}

TEST(HashAggregationWithExprOperatorTest, stddev_samp_without_groupkey)
{
    ConfigUtil::SetSupportContainerVecRule(SupportContainerVecRule::NOT_SUPPORT);
    const int32_t dataSize = 8;
    const int32_t groupByNum = 0;
    const int32_t expectDataSize = 1;

    double data1[] = {2, 5, 8, 11, 14, 17, 20, 23};
    double data2[] = {5, 3, 2, 6, 5, 4, 7, 8};
    double data3[] = {5, 5, 5, 5, 5, 5, 5, 5};
    float data4[] = {1.5f, 2.5f, 3.5f, 4.5f, 5.5f, 6.5f, 7.5f, 8.5f};

    DataTypes sourceTypes(std::vector<DataTypePtr>({DoubleType(), DoubleType(), DoubleType(),
        FloatType()}));
    DataTypes aggOutputTypes1(std::vector<DataTypePtr>({DoubleType(), DoubleType(), DoubleType()}));
    DataTypes aggOutputTypes2(std::vector<DataTypePtr>({DoubleType(), DoubleType(), DoubleType()}));
    DataTypes aggOutputTypes3(std::vector<DataTypePtr>({DoubleType(), DoubleType(), DoubleType()}));
    DataTypes aggOutputTypes4(std::vector<DataTypePtr>({DoubleType(), DoubleType(), DoubleType()}));
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3, data4);

    // aggKeys
    std::vector<Expr *> groupByKeys = {};
    std::vector<Expr *> aggKeys1 = {new FieldExpr(0, DoubleType())};
    std::vector<Expr *> aggKeys2 = {new FieldExpr(1, DoubleType())};
    std::vector<Expr *> aggKeys3 = {new FieldExpr(3, FloatType())};
    LiteralExpr *mulRight = new LiteralExpr(5, FloatType());
    mulRight->floatVal = 5;
    BinaryExpr *mulExpr =
        new BinaryExpr(omniruntime::expressions::Operator::MUL, new FieldExpr(3, FloatType()), mulRight, FloatType());
    std::vector<Expr *> aggKeys4 = { mulExpr };
    std::vector<std::vector<omniruntime::expressions::Expr *>> aggAllKeys = {aggKeys1, aggKeys2,
        aggKeys3, aggKeys4};

    std::vector<uint32_t> aggFuncTypes = {OMNI_AGGREGATION_TYPE_SAMP, OMNI_AGGREGATION_TYPE_SAMP,
        OMNI_AGGREGATION_TYPE_SAMP, OMNI_AGGREGATION_TYPE_SAMP};
    std::vector<uint32_t> maskCols = {static_cast<uint32_t>(-1), static_cast<uint32_t>(-1),
        static_cast<uint32_t>(-1), static_cast<uint32_t>(-1)};

    std::vector<DataTypes> aggsOutputTypes = {aggOutputTypes1, aggOutputTypes2,
        aggOutputTypes3, aggOutputTypes4};
    auto inputRawWrap = std::vector<bool>(aggFuncTypes.size(), true);
    auto outputPartialWrap = std::vector<bool>(aggFuncTypes.size(), true);
    std::vector<omniruntime::expressions::Expr *> aggFilters;
    aggFilters.reserve(2);
    aggFilters.push_back(nullptr);
    aggFilters.push_back(nullptr);
    auto overflowConfig = new OverflowConfig();
    auto aggWithExprOperatorFactory = new AggregationWithExprOperatorFactory(
        groupByKeys, groupByNum,aggAllKeys,sourceTypes,aggsOutputTypes,aggFuncTypes,
        aggFilters,maskCols,inputRawWrap,outputPartialWrap,
        overflowConfig,  false);
    auto *aggWithExprOperator =
        dynamic_cast<AggregationWithExprOperator *>(CreateTestOperator(aggWithExprOperatorFactory));

    aggWithExprOperator->AddInput(vecBatch);
    VectorBatch *outputVecBatch = nullptr;
    aggWithExprOperator->GetOutput(&outputVecBatch);

    std::vector<Expr *> groupByKeysFinal = {};
    std::vector<Expr *> aggKeys1Final = {new FieldExpr(0, DoubleType()), new FieldExpr(1, DoubleType()),
                                         new FieldExpr(2, DoubleType())};
    std::vector<Expr *> aggKeys2Final = {new FieldExpr(3, DoubleType()), new FieldExpr(4, DoubleType()),
                                         new FieldExpr(5, DoubleType())};
    std::vector<Expr *> aggKeys3Final = {new FieldExpr(6, DoubleType()), new FieldExpr(7, DoubleType()),
                                         new FieldExpr(8, DoubleType())};
    std::vector<Expr *> aggKeys4Final = {new FieldExpr(9, DoubleType()), new FieldExpr(10, DoubleType()),
                                     new FieldExpr(11, DoubleType())};
    std::vector<std::vector<omniruntime::expressions::Expr *>> aggAllKeysFinal = {aggKeys1Final, aggKeys2Final,
        aggKeys3Final, aggKeys4Final};
    DataTypes sourceTypesFinal(std::vector<DataTypePtr>(
        { DoubleType(), DoubleType(), DoubleType(), DoubleType(), DoubleType(), DoubleType(),
            DoubleType(), DoubleType(), DoubleType(), DoubleType(), DoubleType(), DoubleType()}));

    auto inputRawFinalWrap = std::vector<bool>(aggFuncTypes.size(), false);
    auto outputFinalWrap = std::vector<bool>(aggFuncTypes.size(), false);
    DataTypes aggOutputTypesFinal1(std::vector<DataTypePtr>({DoubleType()}));
    DataTypes aggOutputTypesFinal2(std::vector<DataTypePtr>({DoubleType()}));
    DataTypes aggOutputTypesFinal3(std::vector<DataTypePtr>({DoubleType()}));
    DataTypes aggOutputTypesFinal4(std::vector<DataTypePtr>({DoubleType()}));
    std::vector<DataTypes> aggsOutputTypesFinal = {
        aggOutputTypesFinal1, aggOutputTypesFinal2, aggOutputTypesFinal3, aggOutputTypesFinal4
    };
    auto aggWithExprOperatorFactoryFinal = new AggregationWithExprOperatorFactory(
        groupByKeysFinal, groupByNum,aggAllKeysFinal,sourceTypesFinal,aggsOutputTypesFinal,aggFuncTypes,
        aggFilters,maskCols,inputRawFinalWrap,outputFinalWrap,
        overflowConfig,  false);
    auto *aggWithExprOperatorFinal =
        dynamic_cast<AggregationWithExprOperator *>(CreateTestOperator(aggWithExprOperatorFactoryFinal));

    aggWithExprOperatorFinal->AddInput(outputVecBatch);
    VectorBatch *outputVecBatchFinal = nullptr;
    aggWithExprOperatorFinal->GetOutput(&outputVecBatchFinal);

    double expData1[] = {7.3484692283495345};
    double expData2[] = {2};
    double expData3[] = {2.4494897427831781};
    double expData4[] = {12.247448713915891};
    DataTypes expectTypes(std::vector<DataTypePtr>({DoubleType(), DoubleType(), DoubleType(), DoubleType()}));
    VectorBatch *expectVectorBatch = CreateVectorBatch(expectTypes, expectDataSize, expData1, expData2, expData3,
        expData4);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatchFinal, expectVectorBatch));

    Expr::DeleteExprs(groupByKeys);
    Expr::DeleteExprs(aggAllKeys);
    Expr::DeleteExprs(groupByKeysFinal);
    Expr::DeleteExprs(aggAllKeysFinal);
    omniruntime::op::Operator::DeleteOperator(aggWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(aggWithExprOperatorFinal);
    delete aggWithExprOperatorFactory;
    delete aggWithExprOperatorFactoryFinal;
    delete overflowConfig;
    VectorHelper::FreeVecBatch(expectVectorBatch);
    VectorHelper::FreeVecBatch(outputVecBatchFinal);
    ConfigUtil::SetSupportContainerVecRule(SupportContainerVecRule::SUPPORT);
}

TEST(HashAggregationWithExprOperatorTest, stddev_pop)
{
    ConfigUtil::SetSupportContainerVecRule(SupportContainerVecRule::NOT_SUPPORT);
    const int32_t dataSize = 8;
    const int32_t groupByNum = 1;
    const int32_t expectDataSize = 1;

    double data1[] = {2, 5, 8, 11, 14, 17, 20, 23};
    double data2[] = {5, 3, 2, 6, 5, 4, 7, 8};
    double data3[] = {5, 5, 5, 5, 5, 5, 5, 5};
    float data4[] = {1.5f, 2.5f, 3.5f, 4.5f, 5.5f, 6.5f, 7.5f, 8.5f};

    DataTypes sourceTypes(std::vector<DataTypePtr>({DoubleType(), DoubleType(), DoubleType(),
        FloatType()}));
    DataTypes aggOutputTypes1(std::vector<DataTypePtr>({DoubleType(), DoubleType(), DoubleType()}));
    DataTypes aggOutputTypes2(std::vector<DataTypePtr>({DoubleType(), DoubleType(), DoubleType()}));
    DataTypes aggOutputTypes3(std::vector<DataTypePtr>({DoubleType(), DoubleType(), DoubleType()}));
    DataTypes aggOutputTypes4(std::vector<DataTypePtr>({DoubleType(), DoubleType(), DoubleType()}));
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3, data4);

    // aggKeys
    std::vector<Expr *> groupByKeys = {new FieldExpr(2, DoubleType())};
    std::vector<Expr *> aggKeys1 = {new FieldExpr(0, DoubleType())};
    std::vector<Expr *> aggKeys2 = {new FieldExpr(1, DoubleType())};
    std::vector<Expr *> aggKeys3 = {new FieldExpr(3, FloatType())};
    LiteralExpr *mulRight = new LiteralExpr(5, FloatType());
    mulRight->floatVal = 5;
    BinaryExpr *mulExpr =
        new BinaryExpr(omniruntime::expressions::Operator::MUL, new FieldExpr(3, FloatType()), mulRight, FloatType());
    std::vector<Expr *> aggKeys4 = { mulExpr };
    std::vector<std::vector<omniruntime::expressions::Expr *>> aggAllKeys = {aggKeys1, aggKeys2,
        aggKeys3, aggKeys4};

    std::vector<uint32_t> aggFuncTypes = {OMNI_AGGREGATION_TYPE_STD_POP, OMNI_AGGREGATION_TYPE_STD_POP,
        OMNI_AGGREGATION_TYPE_STD_POP, OMNI_AGGREGATION_TYPE_STD_POP};
    std::vector<uint32_t> maskCols = {static_cast<uint32_t>(-1), static_cast<uint32_t>(-1),
        static_cast<uint32_t>(-1), static_cast<uint32_t>(-1)};

    std::vector<DataTypes> aggsOutputTypes = {aggOutputTypes1, aggOutputTypes2,
        aggOutputTypes3, aggOutputTypes4};
    auto inputRawWrap = std::vector<bool>(aggFuncTypes.size(), true);
    auto outputPartialWrap = std::vector<bool>(aggFuncTypes.size(), true);
    std::vector<omniruntime::expressions::Expr *> aggFilters;
    aggFilters.reserve(2);
    aggFilters.push_back(nullptr);
    aggFilters.push_back(nullptr);

    auto *hashAggWithExprOperatorFactory =
        new HashAggregationWithExprOperatorFactory(groupByKeys, groupByNum, aggAllKeys, aggFilters, sourceTypes,
            aggsOutputTypes, aggFuncTypes, maskCols, inputRawWrap, outputPartialWrap, OperatorConfig());
    auto *hashAggWithExprOperator =
        dynamic_cast<HashAggregationWithExprOperator *>(CreateTestOperator(hashAggWithExprOperatorFactory));

    hashAggWithExprOperator->AddInput(vecBatch);
    VectorBatch *outputVecBatch = nullptr;
    hashAggWithExprOperator->GetOutput(&outputVecBatch);

    std::vector<Expr *> groupByKeysFinal = {new FieldExpr(0, DoubleType())};
    std::vector<Expr *> aggKeys1Final = {new FieldExpr(1, DoubleType()), new FieldExpr(2, DoubleType()),
                                         new FieldExpr(3, DoubleType())};
    std::vector<Expr *> aggKeys2Final = {new FieldExpr(4, DoubleType()), new FieldExpr(5, DoubleType()),
                                         new FieldExpr(6, DoubleType())};
    std::vector<Expr *> aggKeys3Final = {new FieldExpr(7, DoubleType()), new FieldExpr(8, DoubleType()),
                                         new FieldExpr(9, DoubleType())};
    std::vector<Expr *> aggKeys4Final = {new FieldExpr(10, DoubleType()), new FieldExpr(11, DoubleType()),
                                     new FieldExpr(12, DoubleType())};
    std::vector<std::vector<omniruntime::expressions::Expr *>> aggAllKeysFinal = {aggKeys1Final, aggKeys2Final,
        aggKeys3Final, aggKeys4Final};
    DataTypes sourceTypesFinal(std::vector<DataTypePtr>(
        {DoubleType(), DoubleType(), DoubleType(), DoubleType(), DoubleType(), DoubleType(), DoubleType(),
            DoubleType(), DoubleType(), DoubleType(), DoubleType(), DoubleType(), DoubleType()}));

    auto inputRawFinalWrap = std::vector<bool>(aggFuncTypes.size(), false);
    auto outputFinalWrap = std::vector<bool>(aggFuncTypes.size(), false);
    DataTypes aggOutputTypesFinal1(std::vector<DataTypePtr>({DoubleType()}));
    DataTypes aggOutputTypesFinal2(std::vector<DataTypePtr>({DoubleType()}));
    DataTypes aggOutputTypesFinal3(std::vector<DataTypePtr>({DoubleType()}));
    DataTypes aggOutputTypesFinal4(std::vector<DataTypePtr>({DoubleType()}));
    std::vector<DataTypes> aggsOutputTypesFinal = {
        aggOutputTypesFinal1, aggOutputTypesFinal2, aggOutputTypesFinal3, aggOutputTypesFinal4
    };
    auto *hashAggWithExprOperatorFactoryFinal =
        new HashAggregationWithExprOperatorFactory(groupByKeysFinal, groupByNum, aggAllKeysFinal, aggFilters,
            sourceTypesFinal, aggsOutputTypesFinal, aggFuncTypes, maskCols, inputRawFinalWrap, outputFinalWrap,
            OperatorConfig());
    auto *hashAggWithExprOperatorFinal =
        dynamic_cast<HashAggregationWithExprOperator *>(CreateTestOperator(hashAggWithExprOperatorFactoryFinal));

    hashAggWithExprOperatorFinal->AddInput(outputVecBatch);
    VectorBatch *outputVecBatchFinal = nullptr;
    hashAggWithExprOperatorFinal->GetOutput(&outputVecBatchFinal);

    double expData1[] = {5};
    double expData2[] = {6.8738635424337600};
    double expData3[] = {1.8708286933869707};
    double expData4[] = {2.2912878474779200};
    double expData5[] = {11.456439237389600};
    DataTypes expectTypes(std::vector<DataTypePtr>({DoubleType(), DoubleType(), DoubleType(), DoubleType(), DoubleType()}));
    VectorBatch *expectVectorBatch = CreateVectorBatch(expectTypes, expectDataSize, expData1, expData2, expData3,
        expData4, expData5);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatchFinal, expectVectorBatch));

    Expr::DeleteExprs(groupByKeys);
    Expr::DeleteExprs(aggAllKeys);
    Expr::DeleteExprs(groupByKeysFinal);
    Expr::DeleteExprs(aggAllKeysFinal);
    omniruntime::op::Operator::DeleteOperator(hashAggWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(hashAggWithExprOperatorFinal);
    delete hashAggWithExprOperatorFactory;
    delete hashAggWithExprOperatorFactoryFinal;
    VectorHelper::FreeVecBatch(expectVectorBatch);
    VectorHelper::FreeVecBatch(outputVecBatchFinal);
    ConfigUtil::SetSupportContainerVecRule(SupportContainerVecRule::SUPPORT);
}

TEST(HashAggregationWithExprOperatorTest, stddev_pop_without_groupkey)
{
    ConfigUtil::SetSupportContainerVecRule(SupportContainerVecRule::NOT_SUPPORT);
    const int32_t dataSize = 8;
    const int32_t groupByNum = 0;
    const int32_t expectDataSize = 1;

    double data1[] = {2, 5, 8, 11, 14, 17, 20, 23};
    double data2[] = {5, 3, 2, 6, 5, 4, 7, 8};
    double data3[] = {5, 5, 5, 5, 5, 5, 5, 5};
    float data4[] = {1.5f, 2.5f, 3.5f, 4.5f, 5.5f, 6.5f, 7.5f, 8.5f};

    DataTypes sourceTypes(std::vector<DataTypePtr>({DoubleType(), DoubleType(), DoubleType(),
        FloatType()}));
    DataTypes aggOutputTypes1(std::vector<DataTypePtr>({DoubleType(), DoubleType(), DoubleType()}));
    DataTypes aggOutputTypes2(std::vector<DataTypePtr>({DoubleType(), DoubleType(), DoubleType()}));
    DataTypes aggOutputTypes3(std::vector<DataTypePtr>({DoubleType(), DoubleType(), DoubleType()}));
    DataTypes aggOutputTypes4(std::vector<DataTypePtr>({DoubleType(), DoubleType(), DoubleType()}));
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3, data4);

    // aggKeys
    std::vector<Expr *> groupByKeys = {};
    std::vector<Expr *> aggKeys1 = {new FieldExpr(0, DoubleType())};
    std::vector<Expr *> aggKeys2 = {new FieldExpr(1, DoubleType())};
    std::vector<Expr *> aggKeys3 = {new FieldExpr(3, FloatType())};
    LiteralExpr *mulRight = new LiteralExpr(5, FloatType());
    mulRight->floatVal = 5;
    BinaryExpr *mulExpr =
        new BinaryExpr(omniruntime::expressions::Operator::MUL, new FieldExpr(3, FloatType()), mulRight, FloatType());
    std::vector<Expr *> aggKeys4 = { mulExpr };
    std::vector<std::vector<omniruntime::expressions::Expr *>> aggAllKeys = {aggKeys1, aggKeys2,
        aggKeys3, aggKeys4};

    std::vector<uint32_t> aggFuncTypes = {OMNI_AGGREGATION_TYPE_STD_POP, OMNI_AGGREGATION_TYPE_STD_POP,
        OMNI_AGGREGATION_TYPE_STD_POP, OMNI_AGGREGATION_TYPE_STD_POP};
    std::vector<uint32_t> maskCols = {static_cast<uint32_t>(-1), static_cast<uint32_t>(-1),
        static_cast<uint32_t>(-1), static_cast<uint32_t>(-1)};

    std::vector<DataTypes> aggsOutputTypes = {aggOutputTypes1, aggOutputTypes2,
        aggOutputTypes3, aggOutputTypes4};
    auto inputRawWrap = std::vector<bool>(aggFuncTypes.size(), true);
    auto outputPartialWrap = std::vector<bool>(aggFuncTypes.size(), true);
    std::vector<omniruntime::expressions::Expr *> aggFilters;
    aggFilters.reserve(2);
    aggFilters.push_back(nullptr);
    aggFilters.push_back(nullptr);
    auto overflowConfig = new OverflowConfig();
    auto aggWithExprOperatorFactory = new AggregationWithExprOperatorFactory(
        groupByKeys, groupByNum,aggAllKeys,sourceTypes,aggsOutputTypes,aggFuncTypes,
        aggFilters,maskCols,inputRawWrap,outputPartialWrap,
        overflowConfig,  false);
    auto *aggWithExprOperator =
        dynamic_cast<AggregationWithExprOperator *>(CreateTestOperator(aggWithExprOperatorFactory));

    aggWithExprOperator->AddInput(vecBatch);
    VectorBatch *outputVecBatch = nullptr;
    aggWithExprOperator->GetOutput(&outputVecBatch);

    std::vector<Expr *> groupByKeysFinal = {};
    std::vector<Expr *> aggKeys1Final = {new FieldExpr(0, DoubleType()), new FieldExpr(1, DoubleType()),
                                         new FieldExpr(2, DoubleType())};
    std::vector<Expr *> aggKeys2Final = {new FieldExpr(3, DoubleType()), new FieldExpr(4, DoubleType()),
                                         new FieldExpr(5, DoubleType())};
    std::vector<Expr *> aggKeys3Final = {new FieldExpr(6, DoubleType()), new FieldExpr(7, DoubleType()),
                                         new FieldExpr(8, DoubleType())};
    std::vector<Expr *> aggKeys4Final = {new FieldExpr(9, DoubleType()), new FieldExpr(10, DoubleType()),
                                     new FieldExpr(11, DoubleType())};
    std::vector<std::vector<omniruntime::expressions::Expr *>> aggAllKeysFinal = {aggKeys1Final, aggKeys2Final,
        aggKeys3Final, aggKeys4Final};
    DataTypes sourceTypesFinal(std::vector<DataTypePtr>(
        {DoubleType(), DoubleType(), DoubleType(), DoubleType(), DoubleType(), DoubleType(),
            DoubleType(), DoubleType(), DoubleType(), DoubleType(), DoubleType(), DoubleType()}));

    auto inputRawFinalWrap = std::vector<bool>(aggFuncTypes.size(), false);
    auto outputFinalWrap = std::vector<bool>(aggFuncTypes.size(), false);
    DataTypes aggOutputTypesFinal1(std::vector<DataTypePtr>({DoubleType()}));
    DataTypes aggOutputTypesFinal2(std::vector<DataTypePtr>({DoubleType()}));
    DataTypes aggOutputTypesFinal3(std::vector<DataTypePtr>({DoubleType()}));
    DataTypes aggOutputTypesFinal4(std::vector<DataTypePtr>({DoubleType()}));
    std::vector<DataTypes> aggsOutputTypesFinal = {
        aggOutputTypesFinal1, aggOutputTypesFinal2, aggOutputTypesFinal3, aggOutputTypesFinal4
    };
    auto aggWithExprOperatorFactoryFinal = new AggregationWithExprOperatorFactory(
        groupByKeysFinal, groupByNum,aggAllKeysFinal,sourceTypesFinal,aggsOutputTypesFinal,aggFuncTypes,
        aggFilters,maskCols,inputRawFinalWrap,outputFinalWrap,
        overflowConfig,  false);
    auto *aggWithExprOperatorFinal =
        dynamic_cast<AggregationWithExprOperator *>(CreateTestOperator(aggWithExprOperatorFactoryFinal));

    aggWithExprOperatorFinal->AddInput(outputVecBatch);
    VectorBatch *outputVecBatchFinal = nullptr;
    aggWithExprOperatorFinal->GetOutput(&outputVecBatchFinal);

    double expData1[] = {6.8738635424337600};
    double expData2[] = {1.8708286933869707};
    double expData3[] = {2.2912878474779200};
    double expData4[] = {11.456439237389600};
    DataTypes expectTypes(std::vector<DataTypePtr>({DoubleType(), DoubleType(), DoubleType(), DoubleType()}));
    VectorBatch *expectVectorBatch = CreateVectorBatch(expectTypes, expectDataSize, expData1, expData2, expData3,
        expData4);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatchFinal, expectVectorBatch));

    Expr::DeleteExprs(groupByKeys);
    Expr::DeleteExprs(aggAllKeys);
    Expr::DeleteExprs(groupByKeysFinal);
    Expr::DeleteExprs(aggAllKeysFinal);
    omniruntime::op::Operator::DeleteOperator(aggWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(aggWithExprOperatorFinal);
    delete aggWithExprOperatorFactory;
    delete aggWithExprOperatorFactoryFinal;
    delete overflowConfig;
    VectorHelper::FreeVecBatch(expectVectorBatch);
    VectorHelper::FreeVecBatch(outputVecBatchFinal);
    ConfigUtil::SetSupportContainerVecRule(SupportContainerVecRule::SUPPORT);
}

TEST(HashAggregationWithExprOperatorTest, var_samp)
{
    ConfigUtil::SetSupportContainerVecRule(SupportContainerVecRule::NOT_SUPPORT);
    const int32_t dataSize = 8;
    const int32_t groupByNum = 1;
    const int32_t expectDataSize = 1;

    double data1[] = {2, 5, 8, 11, 14, 17, 20, 23};
    double data2[] = {5, 3, 2, 6, 5, 4, 7, 8};
    double data3[] = {5, 5, 5, 5, 5, 5, 5, 5};
    float data4[] = {1.5f, 2.5f, 3.5f, 4.5f, 5.5f, 6.5f, 7.5f, 8.5f};

    DataTypes sourceTypes(std::vector<DataTypePtr>({DoubleType(), DoubleType(), DoubleType(),
        FloatType()}));
    DataTypes aggOutputTypes1(std::vector<DataTypePtr>({DoubleType(), DoubleType(), DoubleType()}));
    DataTypes aggOutputTypes2(std::vector<DataTypePtr>({DoubleType(), DoubleType(), DoubleType()}));
    DataTypes aggOutputTypes3(std::vector<DataTypePtr>({DoubleType(), DoubleType(), DoubleType()}));
    DataTypes aggOutputTypes4(std::vector<DataTypePtr>({DoubleType(), DoubleType(), DoubleType()}));
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3, data4);

    // aggKeys
    std::vector<Expr *> groupByKeys = {new FieldExpr(2, DoubleType())};
    std::vector<Expr *> aggKeys1 = {new FieldExpr(0, DoubleType())};
    std::vector<Expr *> aggKeys2 = {new FieldExpr(1, DoubleType())};
    std::vector<Expr *> aggKeys3 = {new FieldExpr(3, FloatType())};
    LiteralExpr *mulRight = new LiteralExpr(5, FloatType());
    mulRight->floatVal = 5;
    BinaryExpr *mulExpr =
        new BinaryExpr(omniruntime::expressions::Operator::MUL, new FieldExpr(3, FloatType()), mulRight, FloatType());
    std::vector<Expr *> aggKeys4 = { mulExpr };
    std::vector<std::vector<omniruntime::expressions::Expr *>> aggAllKeys = {aggKeys1, aggKeys2,
        aggKeys3, aggKeys4};

    std::vector<uint32_t> aggFuncTypes = {OMNI_AGGREGATION_TYPE_VAR_SAMP, OMNI_AGGREGATION_TYPE_VAR_SAMP,
        OMNI_AGGREGATION_TYPE_VAR_SAMP, OMNI_AGGREGATION_TYPE_VAR_SAMP};
    std::vector<uint32_t> maskCols = {static_cast<uint32_t>(-1), static_cast<uint32_t>(-1),
        static_cast<uint32_t>(-1), static_cast<uint32_t>(-1)};

    std::vector<DataTypes> aggsOutputTypes = {aggOutputTypes1, aggOutputTypes2,
        aggOutputTypes3, aggOutputTypes4};
    auto inputRawWrap = std::vector<bool>(aggFuncTypes.size(), true);
    auto outputPartialWrap = std::vector<bool>(aggFuncTypes.size(), true);
    std::vector<omniruntime::expressions::Expr *> aggFilters;
    aggFilters.reserve(2);
    aggFilters.push_back(nullptr);
    aggFilters.push_back(nullptr);

    auto *hashAggWithExprOperatorFactory =
        new HashAggregationWithExprOperatorFactory(groupByKeys, groupByNum, aggAllKeys, aggFilters, sourceTypes,
            aggsOutputTypes, aggFuncTypes, maskCols, inputRawWrap, outputPartialWrap, OperatorConfig());
    auto *hashAggWithExprOperator =
        dynamic_cast<HashAggregationWithExprOperator *>(CreateTestOperator(hashAggWithExprOperatorFactory));

    hashAggWithExprOperator->AddInput(vecBatch);
    VectorBatch *outputVecBatch = nullptr;
    hashAggWithExprOperator->GetOutput(&outputVecBatch);

    std::vector<Expr *> groupByKeysFinal = {new FieldExpr(0, DoubleType())};
    std::vector<Expr *> aggKeys1Final = {new FieldExpr(1, DoubleType()), new FieldExpr(2, DoubleType()),
                                         new FieldExpr(3, DoubleType())};
    std::vector<Expr *> aggKeys2Final = {new FieldExpr(4, DoubleType()), new FieldExpr(5, DoubleType()),
                                         new FieldExpr(6, DoubleType())};
    std::vector<Expr *> aggKeys3Final = {new FieldExpr(7, DoubleType()), new FieldExpr(8, DoubleType()),
                                         new FieldExpr(9, DoubleType())};
    std::vector<Expr *> aggKeys4Final = {new FieldExpr(10, DoubleType()), new FieldExpr(11, DoubleType()),
                                     new FieldExpr(12, DoubleType())};
    std::vector<std::vector<omniruntime::expressions::Expr *>> aggAllKeysFinal = {aggKeys1Final, aggKeys2Final,
        aggKeys3Final, aggKeys4Final};
    DataTypes sourceTypesFinal(std::vector<DataTypePtr>(
        {DoubleType(), DoubleType(), DoubleType(), DoubleType(), DoubleType(), DoubleType(), DoubleType(),
            DoubleType(), DoubleType(), DoubleType(), DoubleType(), DoubleType(), DoubleType()}));

    auto inputRawFinalWrap = std::vector<bool>(aggFuncTypes.size(), false);
    auto outputFinalWrap = std::vector<bool>(aggFuncTypes.size(), false);
    DataTypes aggOutputTypesFinal1(std::vector<DataTypePtr>({DoubleType()}));
    DataTypes aggOutputTypesFinal2(std::vector<DataTypePtr>({DoubleType()}));
    DataTypes aggOutputTypesFinal3(std::vector<DataTypePtr>({DoubleType()}));
    DataTypes aggOutputTypesFinal4(std::vector<DataTypePtr>({DoubleType()}));
    std::vector<DataTypes> aggsOutputTypesFinal = {
        aggOutputTypesFinal1, aggOutputTypesFinal2, aggOutputTypesFinal3, aggOutputTypesFinal4
    };
    auto *hashAggWithExprOperatorFactoryFinal =
        new HashAggregationWithExprOperatorFactory(groupByKeysFinal, groupByNum, aggAllKeysFinal, aggFilters,
            sourceTypesFinal, aggsOutputTypesFinal, aggFuncTypes, maskCols, inputRawFinalWrap, outputFinalWrap,
            OperatorConfig());
    auto *hashAggWithExprOperatorFinal =
        dynamic_cast<HashAggregationWithExprOperator *>(CreateTestOperator(hashAggWithExprOperatorFactoryFinal));

    hashAggWithExprOperatorFinal->AddInput(outputVecBatch);
    VectorBatch *outputVecBatchFinal = nullptr;
    hashAggWithExprOperatorFinal->GetOutput(&outputVecBatchFinal);

    double expData1[] = {5};
    double expData2[] = {54.00000000000000000};
    double expData3[] = {3.99999999999999911};
    double expData4[] = {6.0};
    double expData5[] = {150.0};
    DataTypes expectTypes(std::vector<DataTypePtr>({DoubleType(), DoubleType(), DoubleType(), DoubleType(), DoubleType()}));
    VectorBatch *expectVectorBatch = CreateVectorBatch(expectTypes, expectDataSize, expData1, expData2, expData3,
        expData4, expData5);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatchFinal, expectVectorBatch));

    Expr::DeleteExprs(groupByKeys);
    Expr::DeleteExprs(aggAllKeys);
    Expr::DeleteExprs(groupByKeysFinal);
    Expr::DeleteExprs(aggAllKeysFinal);
    omniruntime::op::Operator::DeleteOperator(hashAggWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(hashAggWithExprOperatorFinal);
    delete hashAggWithExprOperatorFactory;
    delete hashAggWithExprOperatorFactoryFinal;
    VectorHelper::FreeVecBatch(expectVectorBatch);
    VectorHelper::FreeVecBatch(outputVecBatchFinal);
    ConfigUtil::SetSupportContainerVecRule(SupportContainerVecRule::SUPPORT);
}

TEST(HashAggregationWithExprOperatorTest, var_samp_without_groupkey)
{
    ConfigUtil::SetSupportContainerVecRule(SupportContainerVecRule::NOT_SUPPORT);
    const int32_t dataSize = 8;
    const int32_t groupByNum = 0;
    const int32_t expectDataSize = 1;

    double data1[] = {2, 5, 8, 11, 14, 17, 20, 23};
    double data2[] = {5, 3, 2, 6, 5, 4, 7, 8};
    double data3[] = {5, 5, 5, 5, 5, 5, 5, 5};
    float data4[] = {1.5f, 2.5f, 3.5f, 4.5f, 5.5f, 6.5f, 7.5f, 8.5f};

    DataTypes sourceTypes(std::vector<DataTypePtr>({DoubleType(), DoubleType(), DoubleType(),
        FloatType()}));
    DataTypes aggOutputTypes1(std::vector<DataTypePtr>({DoubleType(), DoubleType(), DoubleType()}));
    DataTypes aggOutputTypes2(std::vector<DataTypePtr>({DoubleType(), DoubleType(), DoubleType()}));
    DataTypes aggOutputTypes3(std::vector<DataTypePtr>({DoubleType(), DoubleType(), DoubleType()}));
    DataTypes aggOutputTypes4(std::vector<DataTypePtr>({DoubleType(), DoubleType(), DoubleType()}));
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3, data4);

    // aggKeys
    std::vector<Expr *> groupByKeys = {};
    std::vector<Expr *> aggKeys1 = {new FieldExpr(0, DoubleType())};
    std::vector<Expr *> aggKeys2 = {new FieldExpr(1, DoubleType())};
    std::vector<Expr *> aggKeys3 = {new FieldExpr(3, FloatType())};
    LiteralExpr *mulRight = new LiteralExpr(5, FloatType());
    mulRight->floatVal = 5;
    BinaryExpr *mulExpr =
        new BinaryExpr(omniruntime::expressions::Operator::MUL, new FieldExpr(3, FloatType()), mulRight, FloatType());
    std::vector<Expr *> aggKeys4 = { mulExpr };
    std::vector<std::vector<omniruntime::expressions::Expr *>> aggAllKeys = {aggKeys1, aggKeys2,
        aggKeys3, aggKeys4};

    std::vector<uint32_t> aggFuncTypes = {OMNI_AGGREGATION_TYPE_VAR_SAMP, OMNI_AGGREGATION_TYPE_VAR_SAMP,
        OMNI_AGGREGATION_TYPE_VAR_SAMP, OMNI_AGGREGATION_TYPE_VAR_SAMP};
    std::vector<uint32_t> maskCols = {static_cast<uint32_t>(-1), static_cast<uint32_t>(-1),
        static_cast<uint32_t>(-1), static_cast<uint32_t>(-1)};

    std::vector<DataTypes> aggsOutputTypes = {aggOutputTypes1, aggOutputTypes2,
        aggOutputTypes3, aggOutputTypes4};
    auto inputRawWrap = std::vector<bool>(aggFuncTypes.size(), true);
    auto outputPartialWrap = std::vector<bool>(aggFuncTypes.size(), true);
    std::vector<omniruntime::expressions::Expr *> aggFilters;
    aggFilters.reserve(2);
    aggFilters.push_back(nullptr);
    aggFilters.push_back(nullptr);
    auto overflowConfig = new OverflowConfig();
    auto aggWithExprOperatorFactory = new AggregationWithExprOperatorFactory(
        groupByKeys, groupByNum,aggAllKeys,sourceTypes,aggsOutputTypes,aggFuncTypes,
        aggFilters,maskCols,inputRawWrap,outputPartialWrap,
        overflowConfig,  false);
    auto *aggWithExprOperator =
        dynamic_cast<AggregationWithExprOperator *>(CreateTestOperator(aggWithExprOperatorFactory));

    aggWithExprOperator->AddInput(vecBatch);
    VectorBatch *outputVecBatch = nullptr;
    aggWithExprOperator->GetOutput(&outputVecBatch);

    std::vector<Expr *> groupByKeysFinal = {};
    std::vector<Expr *> aggKeys1Final = {new FieldExpr(0, DoubleType()), new FieldExpr(1, DoubleType()),
                                         new FieldExpr(2, DoubleType())};
    std::vector<Expr *> aggKeys2Final = {new FieldExpr(3, DoubleType()), new FieldExpr(4, DoubleType()),
                                         new FieldExpr(5, DoubleType())};
    std::vector<Expr *> aggKeys3Final = {new FieldExpr(6, DoubleType()), new FieldExpr(7, DoubleType()),
                                         new FieldExpr(8, DoubleType())};
    std::vector<Expr *> aggKeys4Final = {new FieldExpr(9, DoubleType()), new FieldExpr(10, DoubleType()),
                                     new FieldExpr(11, DoubleType())};
    std::vector<std::vector<omniruntime::expressions::Expr *>> aggAllKeysFinal = {aggKeys1Final, aggKeys2Final,
        aggKeys3Final, aggKeys4Final};
    DataTypes sourceTypesFinal(std::vector<DataTypePtr>(
        {DoubleType(), DoubleType(), DoubleType(), DoubleType(), DoubleType(), DoubleType(),
            DoubleType(), DoubleType(), DoubleType(), DoubleType(), DoubleType(), DoubleType()}));

    auto inputRawFinalWrap = std::vector<bool>(aggFuncTypes.size(), false);
    auto outputFinalWrap = std::vector<bool>(aggFuncTypes.size(), false);
    DataTypes aggOutputTypesFinal1(std::vector<DataTypePtr>({DoubleType()}));
    DataTypes aggOutputTypesFinal2(std::vector<DataTypePtr>({DoubleType()}));
    DataTypes aggOutputTypesFinal3(std::vector<DataTypePtr>({DoubleType()}));
    DataTypes aggOutputTypesFinal4(std::vector<DataTypePtr>({DoubleType()}));
    std::vector<DataTypes> aggsOutputTypesFinal = {
        aggOutputTypesFinal1, aggOutputTypesFinal2, aggOutputTypesFinal3, aggOutputTypesFinal4
    };
    auto aggWithExprOperatorFactoryFinal = new AggregationWithExprOperatorFactory(
        groupByKeysFinal, groupByNum,aggAllKeysFinal,sourceTypesFinal,aggsOutputTypesFinal,aggFuncTypes,
        aggFilters,maskCols,inputRawFinalWrap,outputFinalWrap,
        overflowConfig,  false);
    auto *aggWithExprOperatorFinal =
        dynamic_cast<AggregationWithExprOperator *>(CreateTestOperator(aggWithExprOperatorFactoryFinal));

    aggWithExprOperatorFinal->AddInput(outputVecBatch);
    VectorBatch *outputVecBatchFinal = nullptr;
    aggWithExprOperatorFinal->GetOutput(&outputVecBatchFinal);

    double expData1[] = {54.00000000000000000};
    double expData2[] = {3.99999999999999911};
    double expData3[] = {6.0};
    double expData4[] = {150.0};
    DataTypes expectTypes(std::vector<DataTypePtr>({DoubleType(), DoubleType(), DoubleType(), DoubleType()}));
    VectorBatch *expectVectorBatch = CreateVectorBatch(expectTypes, expectDataSize, expData1, expData2, expData3,
        expData4);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatchFinal, expectVectorBatch));

    Expr::DeleteExprs(groupByKeys);
    Expr::DeleteExprs(aggAllKeys);
    Expr::DeleteExprs(groupByKeysFinal);
    Expr::DeleteExprs(aggAllKeysFinal);
    omniruntime::op::Operator::DeleteOperator(aggWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(aggWithExprOperatorFinal);
    delete aggWithExprOperatorFactory;
    delete aggWithExprOperatorFactoryFinal;
    delete overflowConfig;
    VectorHelper::FreeVecBatch(expectVectorBatch);
    VectorHelper::FreeVecBatch(outputVecBatchFinal);
    ConfigUtil::SetSupportContainerVecRule(SupportContainerVecRule::SUPPORT);
}

TEST(HashAggregationWithExprOperatorTest, var_pop)
{
    ConfigUtil::SetSupportContainerVecRule(SupportContainerVecRule::NOT_SUPPORT);
    const int32_t dataSize = 8;
    const int32_t groupByNum = 1;
    const int32_t expectDataSize = 1;

    double data1[] = {2, 5, 8, 11, 14, 17, 20, 23};
    double data2[] = {5, 3, 2, 6, 5, 4, 7, 8};
    double data3[] = {5, 5, 5, 5, 5, 5, 5, 5};
    float data4[] = {1.5f, 2.5f, 3.5f, 4.5f, 5.5f, 6.5f, 7.5f, 8.5f};

    DataTypes sourceTypes(std::vector<DataTypePtr>({DoubleType(), DoubleType(), DoubleType(),
        FloatType()}));
    DataTypes aggOutputTypes1(std::vector<DataTypePtr>({DoubleType(), DoubleType(), DoubleType()}));
    DataTypes aggOutputTypes2(std::vector<DataTypePtr>({DoubleType(), DoubleType(), DoubleType()}));
    DataTypes aggOutputTypes3(std::vector<DataTypePtr>({DoubleType(), DoubleType(), DoubleType()}));
    DataTypes aggOutputTypes4(std::vector<DataTypePtr>({DoubleType(), DoubleType(), DoubleType()}));
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3, data4);

    // aggKeys
    std::vector<Expr *> groupByKeys = {new FieldExpr(2, DoubleType())};
    std::vector<Expr *> aggKeys1 = {new FieldExpr(0, DoubleType())};
    std::vector<Expr *> aggKeys2 = {new FieldExpr(1, DoubleType())};
    std::vector<Expr *> aggKeys3 = {new FieldExpr(3, FloatType())};
    LiteralExpr *mulRight = new LiteralExpr(5, FloatType());
    mulRight->floatVal = 5;
    BinaryExpr *mulExpr =
        new BinaryExpr(omniruntime::expressions::Operator::MUL, new FieldExpr(3, FloatType()), mulRight, FloatType());
    std::vector<Expr *> aggKeys4 = { mulExpr };
    std::vector<std::vector<omniruntime::expressions::Expr *>> aggAllKeys = {aggKeys1, aggKeys2,
        aggKeys3, aggKeys4};

    std::vector<uint32_t> aggFuncTypes = {OMNI_AGGREGATION_TYPE_VAR_POP, OMNI_AGGREGATION_TYPE_VAR_POP,
        OMNI_AGGREGATION_TYPE_VAR_POP, OMNI_AGGREGATION_TYPE_VAR_POP};
    std::vector<uint32_t> maskCols = {static_cast<uint32_t>(-1), static_cast<uint32_t>(-1),
        static_cast<uint32_t>(-1), static_cast<uint32_t>(-1)};

    std::vector<DataTypes> aggsOutputTypes = {aggOutputTypes1, aggOutputTypes2,
        aggOutputTypes3, aggOutputTypes4};
    auto inputRawWrap = std::vector<bool>(aggFuncTypes.size(), true);
    auto outputPartialWrap = std::vector<bool>(aggFuncTypes.size(), true);
    std::vector<omniruntime::expressions::Expr *> aggFilters;
    aggFilters.reserve(2);
    aggFilters.push_back(nullptr);
    aggFilters.push_back(nullptr);

    auto *hashAggWithExprOperatorFactory =
        new HashAggregationWithExprOperatorFactory(groupByKeys, groupByNum, aggAllKeys, aggFilters, sourceTypes,
            aggsOutputTypes, aggFuncTypes, maskCols, inputRawWrap, outputPartialWrap, OperatorConfig());
    auto *hashAggWithExprOperator =
        dynamic_cast<HashAggregationWithExprOperator *>(CreateTestOperator(hashAggWithExprOperatorFactory));

    hashAggWithExprOperator->AddInput(vecBatch);
    VectorBatch *outputVecBatch = nullptr;
    hashAggWithExprOperator->GetOutput(&outputVecBatch);

    std::vector<Expr *> groupByKeysFinal = {new FieldExpr(0, DoubleType())};
    std::vector<Expr *> aggKeys1Final = {new FieldExpr(1, DoubleType()), new FieldExpr(2, DoubleType()),
                                         new FieldExpr(3, DoubleType())};
    std::vector<Expr *> aggKeys2Final = {new FieldExpr(4, DoubleType()), new FieldExpr(5, DoubleType()),
                                         new FieldExpr(6, DoubleType())};
    std::vector<Expr *> aggKeys3Final = {new FieldExpr(7, DoubleType()), new FieldExpr(8, DoubleType()),
                                         new FieldExpr(9, DoubleType())};
    std::vector<Expr *> aggKeys4Final = {new FieldExpr(10, DoubleType()), new FieldExpr(11, DoubleType()),
                                     new FieldExpr(12, DoubleType())};
    std::vector<std::vector<omniruntime::expressions::Expr *>> aggAllKeysFinal = {aggKeys1Final, aggKeys2Final,
        aggKeys3Final, aggKeys4Final};
    DataTypes sourceTypesFinal(std::vector<DataTypePtr>(
        {DoubleType(), DoubleType(), DoubleType(), DoubleType(), DoubleType(), DoubleType(), DoubleType(),
            DoubleType(), DoubleType(), DoubleType(), DoubleType(), DoubleType(), DoubleType()}));

    auto inputRawFinalWrap = std::vector<bool>(aggFuncTypes.size(), false);
    auto outputFinalWrap = std::vector<bool>(aggFuncTypes.size(), false);
    DataTypes aggOutputTypesFinal1(std::vector<DataTypePtr>({DoubleType()}));
    DataTypes aggOutputTypesFinal2(std::vector<DataTypePtr>({DoubleType()}));
    DataTypes aggOutputTypesFinal3(std::vector<DataTypePtr>({DoubleType()}));
    DataTypes aggOutputTypesFinal4(std::vector<DataTypePtr>({DoubleType()}));
    std::vector<DataTypes> aggsOutputTypesFinal = {
        aggOutputTypesFinal1, aggOutputTypesFinal2, aggOutputTypesFinal3, aggOutputTypesFinal4
    };
    auto *hashAggWithExprOperatorFactoryFinal =
        new HashAggregationWithExprOperatorFactory(groupByKeysFinal, groupByNum, aggAllKeysFinal, aggFilters,
            sourceTypesFinal, aggsOutputTypesFinal, aggFuncTypes, maskCols, inputRawFinalWrap, outputFinalWrap,
            OperatorConfig());
    auto *hashAggWithExprOperatorFinal =
        dynamic_cast<HashAggregationWithExprOperator *>(CreateTestOperator(hashAggWithExprOperatorFactoryFinal));

    hashAggWithExprOperatorFinal->AddInput(outputVecBatch);
    VectorBatch *outputVecBatchFinal = nullptr;
    hashAggWithExprOperatorFinal->GetOutput(&outputVecBatchFinal);

    double expData1[] = {5};
    double expData2[] = {47.25000000000000000};
    double expData3[] = {3.49999999999999911};
    double expData4[] = {5.25};
    double expData5[] = {131.25};
    DataTypes expectTypes(std::vector<DataTypePtr>({DoubleType(), DoubleType(), DoubleType(), DoubleType(), DoubleType()}));
    VectorBatch *expectVectorBatch = CreateVectorBatch(expectTypes, expectDataSize, expData1, expData2, expData3,
        expData4, expData5);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatchFinal, expectVectorBatch));

    Expr::DeleteExprs(groupByKeys);
    Expr::DeleteExprs(aggAllKeys);
    Expr::DeleteExprs(groupByKeysFinal);
    Expr::DeleteExprs(aggAllKeysFinal);
    omniruntime::op::Operator::DeleteOperator(hashAggWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(hashAggWithExprOperatorFinal);
    delete hashAggWithExprOperatorFactory;
    delete hashAggWithExprOperatorFactoryFinal;
    VectorHelper::FreeVecBatch(expectVectorBatch);
    VectorHelper::FreeVecBatch(outputVecBatchFinal);
    ConfigUtil::SetSupportContainerVecRule(SupportContainerVecRule::SUPPORT);
}

TEST(HashAggregationWithExprOperatorTest, var_pop_without_groupkey)
{
    ConfigUtil::SetSupportContainerVecRule(SupportContainerVecRule::NOT_SUPPORT);
    const int32_t dataSize = 8;
    const int32_t groupByNum = 0;
    const int32_t expectDataSize = 1;

    double data1[] = {2, 5, 8, 11, 14, 17, 20, 23};
    double data2[] = {5, 3, 2, 6, 5, 4, 7, 8};
    double data3[] = {5, 5, 5, 5, 5, 5, 5, 5};
    float data4[] = {1.5f, 2.5f, 3.5f, 4.5f, 5.5f, 6.5f, 7.5f, 8.5f};

    DataTypes sourceTypes(std::vector<DataTypePtr>({DoubleType(), DoubleType(), DoubleType(),
        FloatType()}));
    DataTypes aggOutputTypes1(std::vector<DataTypePtr>({DoubleType(), DoubleType(), DoubleType()}));
    DataTypes aggOutputTypes2(std::vector<DataTypePtr>({DoubleType(), DoubleType(), DoubleType()}));
    DataTypes aggOutputTypes3(std::vector<DataTypePtr>({DoubleType(), DoubleType(), DoubleType()}));
    DataTypes aggOutputTypes4(std::vector<DataTypePtr>({DoubleType(), DoubleType(), DoubleType()}));
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3, data4);

    // aggKeys
    std::vector<Expr *> groupByKeys = {};
    std::vector<Expr *> aggKeys1 = {new FieldExpr(0, DoubleType())};
    std::vector<Expr *> aggKeys2 = {new FieldExpr(1, DoubleType())};
    std::vector<Expr *> aggKeys3 = {new FieldExpr(3, FloatType())};
    LiteralExpr *mulRight = new LiteralExpr(5, FloatType());
    mulRight->floatVal = 5;
    BinaryExpr *mulExpr =
        new BinaryExpr(omniruntime::expressions::Operator::MUL, new FieldExpr(3, FloatType()), mulRight, FloatType());
    std::vector<Expr *> aggKeys4 = { mulExpr };
    std::vector<std::vector<omniruntime::expressions::Expr *>> aggAllKeys = {aggKeys1, aggKeys2,
        aggKeys3, aggKeys4};

    std::vector<uint32_t> aggFuncTypes = {OMNI_AGGREGATION_TYPE_VAR_POP, OMNI_AGGREGATION_TYPE_VAR_POP,
        OMNI_AGGREGATION_TYPE_VAR_POP, OMNI_AGGREGATION_TYPE_VAR_POP};
    std::vector<uint32_t> maskCols = {static_cast<uint32_t>(-1), static_cast<uint32_t>(-1),
        static_cast<uint32_t>(-1), static_cast<uint32_t>(-1)};

    std::vector<DataTypes> aggsOutputTypes = {aggOutputTypes1, aggOutputTypes2,
        aggOutputTypes3, aggOutputTypes4};
    auto inputRawWrap = std::vector<bool>(aggFuncTypes.size(), true);
    auto outputPartialWrap = std::vector<bool>(aggFuncTypes.size(), true);
    std::vector<omniruntime::expressions::Expr *> aggFilters;
    aggFilters.reserve(2);
    aggFilters.push_back(nullptr);
    aggFilters.push_back(nullptr);
    auto overflowConfig = new OverflowConfig();
    auto aggWithExprOperatorFactory = new AggregationWithExprOperatorFactory(
        groupByKeys, groupByNum,aggAllKeys,sourceTypes,aggsOutputTypes,aggFuncTypes,
        aggFilters,maskCols,inputRawWrap,outputPartialWrap,
        overflowConfig,  false);
    auto *aggWithExprOperator =
        dynamic_cast<AggregationWithExprOperator *>(CreateTestOperator(aggWithExprOperatorFactory));

    aggWithExprOperator->AddInput(vecBatch);
    VectorBatch *outputVecBatch = nullptr;
    aggWithExprOperator->GetOutput(&outputVecBatch);

    std::vector<Expr *> groupByKeysFinal = {};
    std::vector<Expr *> aggKeys1Final = {new FieldExpr(0, DoubleType()), new FieldExpr(1, DoubleType()),
                                         new FieldExpr(2, DoubleType())};
    std::vector<Expr *> aggKeys2Final = {new FieldExpr(3, DoubleType()), new FieldExpr(4, DoubleType()),
                                         new FieldExpr(5, DoubleType())};
    std::vector<Expr *> aggKeys3Final = {new FieldExpr(6, DoubleType()), new FieldExpr(7, DoubleType()),
                                         new FieldExpr(8, DoubleType())};
    std::vector<Expr *> aggKeys4Final = {new FieldExpr(9, DoubleType()), new FieldExpr(10, DoubleType()),
                                     new FieldExpr(11, DoubleType())};
    std::vector<std::vector<omniruntime::expressions::Expr *>> aggAllKeysFinal = {aggKeys1Final, aggKeys2Final,
        aggKeys3Final, aggKeys4Final};
    DataTypes sourceTypesFinal(std::vector<DataTypePtr>(
        {DoubleType(), DoubleType(), DoubleType(), DoubleType(), DoubleType(), DoubleType(),
            DoubleType(), DoubleType(), DoubleType(), DoubleType(), DoubleType(), DoubleType()}));

    auto inputRawFinalWrap = std::vector<bool>(aggFuncTypes.size(), false);
    auto outputFinalWrap = std::vector<bool>(aggFuncTypes.size(), false);
    DataTypes aggOutputTypesFinal1(std::vector<DataTypePtr>({DoubleType()}));
    DataTypes aggOutputTypesFinal2(std::vector<DataTypePtr>({DoubleType()}));
    DataTypes aggOutputTypesFinal3(std::vector<DataTypePtr>({DoubleType()}));
    DataTypes aggOutputTypesFinal4(std::vector<DataTypePtr>({DoubleType()}));
    std::vector<DataTypes> aggsOutputTypesFinal = {
        aggOutputTypesFinal1, aggOutputTypesFinal2, aggOutputTypesFinal3, aggOutputTypesFinal4
    };
    auto aggWithExprOperatorFactoryFinal = new AggregationWithExprOperatorFactory(
        groupByKeysFinal, groupByNum,aggAllKeysFinal,sourceTypesFinal,aggsOutputTypesFinal,aggFuncTypes,
        aggFilters,maskCols,inputRawFinalWrap,outputFinalWrap,
        overflowConfig,  false);
    auto *aggWithExprOperatorFinal =
        dynamic_cast<AggregationWithExprOperator *>(CreateTestOperator(aggWithExprOperatorFactoryFinal));

    aggWithExprOperatorFinal->AddInput(outputVecBatch);
    VectorBatch *outputVecBatchFinal = nullptr;
    aggWithExprOperatorFinal->GetOutput(&outputVecBatchFinal);

    double expData1[] = {47.25000000000000000};
    double expData2[] = {3.49999999999999911};
    double expData3[] = {5.25};
    double expData4[] = {131.25};
    DataTypes expectTypes(std::vector<DataTypePtr>({DoubleType(), DoubleType(), DoubleType(), DoubleType()}));
    VectorBatch *expectVectorBatch = CreateVectorBatch(expectTypes, expectDataSize, expData1, expData2, expData3,
        expData4);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatchFinal, expectVectorBatch));

    Expr::DeleteExprs(groupByKeys);
    Expr::DeleteExprs(aggAllKeys);
    Expr::DeleteExprs(groupByKeysFinal);
    Expr::DeleteExprs(aggAllKeysFinal);
    omniruntime::op::Operator::DeleteOperator(aggWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(aggWithExprOperatorFinal);
    delete aggWithExprOperatorFactory;
    delete aggWithExprOperatorFactoryFinal;
    delete overflowConfig;
    VectorHelper::FreeVecBatch(expectVectorBatch);
    VectorHelper::FreeVecBatch(outputVecBatchFinal);
    ConfigUtil::SetSupportContainerVecRule(SupportContainerVecRule::SUPPORT);
}

TEST(HashAggregationWithExprOperatorTest, test_hashagg_final_flat_input_expr)
{
    ConfigUtil::SetSupportContainerVecRule(SupportContainerVecRule::NOT_SUPPORT);

    const int32_t dataSize = 1;
    const int32_t groupByNum = 2;
    const int32_t expectDataSize = 1;

    // prepare data
    // avg(c1*5), avg(c3) group by c0%3, c2  final
    int64_t data1[] = {2L}; // c0
    int32_t data2[] = {5L}; // c1
    double data3[] = {400}; // c2
    int64_t data4[] = {8};  // c3
    double data5[] = {32};  // c3
    int64_t data6[] = {8};  // c3

    DataTypes sourceTypes(
        std::vector<DataTypePtr>({ LongType(), IntType(), DoubleType(), LongType(), DoubleType(), LongType() }));
    DataTypes aggOutputTypes1(std::vector<DataTypePtr>({ DoubleType() }));
    DataTypes aggOutputTypes2(std::vector<DataTypePtr>({ DoubleType() }));
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3, data4, data5, data6);

    std::vector<Expr *> groupByKeys = { new FieldExpr(0, LongType()), new FieldExpr(1, IntType()) };

    // aggKeys
    std::vector<Expr *> aggKeys1 = { new FieldExpr(2, DoubleType()), new FieldExpr(3, LongType()) }; // agg1 SUM + COUNT
    std::vector<Expr *> aggKeys2 = { new FieldExpr(4, DoubleType()), new FieldExpr(5, LongType()) }; // agg2 SUM + COUNT
    std::vector<std::vector<omniruntime::expressions::Expr *>> aggAllKeys = { aggKeys1, aggKeys2 };

    std::vector<uint32_t> aggFuncTypes = { OMNI_AGGREGATION_TYPE_AVG, OMNI_AGGREGATION_TYPE_AVG };
    std::vector<uint32_t> maskCols = { static_cast<uint32_t>(-1), static_cast<uint32_t>(-1) };

    std::vector<DataTypes> aggsOutputTypes = { aggOutputTypes1, aggOutputTypes2 };
    auto inputRawWrap = std::vector<bool>(aggFuncTypes.size(), false);
    auto outputPartialWrap = std::vector<bool>(aggFuncTypes.size(), false);
    std::vector<omniruntime::expressions::Expr *> aggFilters;
    aggFilters.reserve(2);
    aggFilters.push_back(nullptr);
    aggFilters.push_back(nullptr);

    auto *hashAggWithExprFinalOperatorFactory =
        new HashAggregationWithExprOperatorFactory(groupByKeys, groupByNum, aggAllKeys, aggFilters, sourceTypes,
        aggsOutputTypes, aggFuncTypes, maskCols, inputRawWrap, outputPartialWrap, OperatorConfig());
    auto *hashAggWithExprFinalOperator =
        dynamic_cast<HashAggregationWithExprOperator *>(CreateTestOperator(hashAggWithExprFinalOperatorFactory));

    hashAggWithExprFinalOperator->AddInput(vecBatch);
    VectorBatch *outputVecBatch = nullptr;
    hashAggWithExprFinalOperator->GetOutput(&outputVecBatch);

    int64_t expData1[] = {2};
    int32_t expData2[] = {5};
    double expData3[] = {50};
    double expData4[] = {4};
    DataTypes expectTypes(std::vector<DataTypePtr>({ LongType(), IntType(), DoubleType(), DoubleType() }));
    VectorBatch *expectVecorBatch =
        CreateVectorBatch(expectTypes, expectDataSize, expData1, expData2, expData3, expData4);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecorBatch));

    Expr::DeleteExprs(groupByKeys);
    Expr::DeleteExprs(aggAllKeys);
    omniruntime::op::Operator::DeleteOperator(hashAggWithExprFinalOperator);
    delete hashAggWithExprFinalOperatorFactory;
    VectorHelper::FreeVecBatch(expectVecorBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);

    ConfigUtil::SetSupportContainerVecRule(SupportContainerVecRule::SUPPORT);
}

TEST(AggregationWithExprOperatorTest, test_agg_sum_expr)
{
    const int32_t dataSize = 8;
    const int32_t groupByNum = 0;
    const int32_t expectDataSize = 1;

    // prepare data
    // sum(c1*5), sum(c4+1), c5 => 180, 48.0f, 39.6
    int64_t data1[] = {2L, 5L, 8L, 11L, 14L, 17L, 20L, 23L}; // c0
    int64_t data2[] = {5L, 3L, 2L, 6L, 1L, 4L, 7L, 8L};      // c1
    int32_t data3[] = {5, 5, 5, 5, 5, 5, 5, 5};              // c2
    int32_t data4[] = {5, 3, 2, 6, 1, 4, 7, 8};              // c3
    float data5[] = {1.5f, 2.5f, 3.5f, 4.5f, 5.5f, 6.5f, 7.5f, 8.5f}; //c4
    double data6[] = {1.1, 2.2, 3.3, 4.4, 5.5, 6.6, 7.7, 8.8}; // c5

    DataTypes sourceTypes(std::vector<DataTypePtr>({ LongType(), LongType(), IntType(), IntType(),
        FloatType(), DoubleType() }));
    DataTypes aggOutputTypes(std::vector<DataTypePtr>({ LongType(), DoubleType(), DoubleType() }));
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3, data4, data5, data6);

    std::vector<Expr *> groupByKeys = {};

    // aggKeys
    LiteralExpr *mulRight = new LiteralExpr(5, LongType());
    mulRight->longVal = 5;
    BinaryExpr *mulExpr =
        new BinaryExpr(omniruntime::expressions::Operator::MUL, new FieldExpr(1, LongType()), mulRight, LongType());

    LiteralExpr *addRight = new LiteralExpr(6, FloatType());
    addRight->floatVal = 1;
    BinaryExpr *addExpr =
        new BinaryExpr(omniruntime::expressions::Operator::ADD, new FieldExpr(4, FloatType()), addRight, FloatType());
    std::vector<Expr *> aggKeys1 = { mulExpr };
    std::vector<Expr *> aggKeys2 = { addExpr };
    std::vector<Expr *> aggKeys3 = {new FieldExpr(5, DoubleType())};
    std::vector<std::vector<omniruntime::expressions::Expr *>> aggAllKeys = { aggKeys1, aggKeys2, aggKeys3 };

    std::vector<uint32_t> aggFuncTypes = { OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_SUM,
        OMNI_AGGREGATION_TYPE_SUM };
    std::vector<uint32_t> maskCols = { static_cast<uint32_t>(-1), static_cast<uint32_t>(-1), static_cast<uint32_t>(-1) };

    auto overflowConfig = new OverflowConfig();

    auto aggOutputTypesWrap = AggregatorUtil::WrapWithVector(aggOutputTypes);
    auto inputRawWrap = std::vector<bool>(aggFuncTypes.size(), true);
    auto outputPartialWrap = std::vector<bool>(aggFuncTypes.size(), false);

    std::vector<omniruntime::expressions::Expr *> aggFilters;
    aggFilters.reserve(2);
    aggFilters.push_back(nullptr);
    aggFilters.push_back(nullptr);

    auto *aggWithExprOperatorFactory =
        new AggregationWithExprOperatorFactory(groupByKeys, groupByNum, aggAllKeys, sourceTypes, aggOutputTypesWrap,
        aggFuncTypes, aggFilters, maskCols, inputRawWrap, outputPartialWrap, overflowConfig);
    auto *aggWithExprOperator =
        static_cast<AggregationWithExprOperator *>(CreateTestOperator(aggWithExprOperatorFactory));

    aggWithExprOperator->AddInput(vecBatch);
    VectorBatch *outputVecBatch = nullptr;
    aggWithExprOperator->GetOutput(&outputVecBatch);

    int64_t expData1[] = {180};
    double expData2[] = {48.0};
    double expData3[] = {39.6};

    DataTypes expectTypes(std::vector<DataTypePtr>({ LongType(), DoubleType(), DoubleType() }));
    VectorBatch *expectVecorBatch = CreateVectorBatch(expectTypes, expectDataSize, expData1, expData2, expData3);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecorBatch));

    Expr::DeleteExprs(groupByKeys);
    Expr::DeleteExprs(aggAllKeys);
    omniruntime::op::Operator::DeleteOperator(aggWithExprOperator);
    delete aggWithExprOperatorFactory;
    VectorHelper::FreeVecBatch(expectVecorBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
    delete overflowConfig;
}

TEST(AggregationWithExprOperatorTest, test_agg_first_expr)
{
    const int32_t dataSize = 8;
    const int32_t groupByNum = 0;
    const int32_t expectDataSize = 1;

    // prepare data
    // first(c3), first(c4) => 5, 1.5f
    int64_t data1[] = {2L, 5L, 8L, 11L, 14L, 17L, 20L, 23L}; // c0
    int64_t data2[] = {5L, 3L, 2L, 6L, 1L, 4L, 7L, 8L};      // c1
    int32_t data3[] = {5, 5, 5, 5, 5, 5, 5, 5};              // c2
    int32_t data4[] = {5, 3, 2, 6, 1, 4, 7, 8};              // c3
    float data5[] = {1.5f, 2.5f, 3.5f, 4.5f, 5.5f, 6.5f, 7.5f, 8.5f}; //c4

    DataTypes sourceTypes(std::vector<DataTypePtr>({ LongType(), LongType(), IntType(), IntType(), FloatType() }));
    DataTypes aggOutputTypes1(std::vector<DataTypePtr>({ IntType() }));
    DataTypes aggOutputTypes2(std::vector<DataTypePtr>({ FloatType() }));
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3, data4, data5);


    std::vector<Expr *> aggKeys1 = { new FieldExpr(3, IntType()) };
    std::vector<Expr *> aggKeys2 = { new FieldExpr(4, IntType()) };
    std::vector<std::vector<omniruntime::expressions::Expr *>> aggAllKeys = { aggKeys1, aggKeys2 };

    std::vector<uint32_t> aggFuncTypes = { OMNI_AGGREGATION_TYPE_FIRST_IGNORENULL,
        OMNI_AGGREGATION_TYPE_FIRST_IGNORENULL };
    std::vector<uint32_t> maskCols = { static_cast<uint32_t>(-1), static_cast<uint32_t>(-1) };
    std::vector<DataTypes> aggsOutputTypes = { aggOutputTypes1, aggOutputTypes2 };
    auto overflowConfig = new OverflowConfig();
    std::vector<Expr *> groupByKeys = {};
    auto inputRawWrap = std::vector<bool>(aggFuncTypes.size(), true);
    auto outputPartialWrap = std::vector<bool>(aggFuncTypes.size(), false);
    std::vector<omniruntime::expressions::Expr *> aggFilters;
    aggFilters.reserve(2);
    aggFilters.push_back(nullptr);
    aggFilters.push_back(nullptr);

    auto *aggWithExprOperatorFactory =
        new AggregationWithExprOperatorFactory(groupByKeys, groupByNum, aggAllKeys, sourceTypes, aggsOutputTypes,
        aggFuncTypes, aggFilters, maskCols, inputRawWrap, outputPartialWrap, overflowConfig);
    auto *aggWithExprOperator =
        static_cast<AggregationWithExprOperator *>(CreateTestOperator(aggWithExprOperatorFactory));

    aggWithExprOperator->AddInput(vecBatch);
    VectorBatch *outputVecBatch = nullptr;
    aggWithExprOperator->GetOutput(&outputVecBatch);

    int64_t expData1[] = {5};
    float expData2[] = {1.5f};
    DataTypes expectTypes(std::vector<DataTypePtr>({ IntType(), FloatType() }));
    VectorBatch *expectVecorBatch = CreateVectorBatch(expectTypes, expectDataSize, expData1, expData2);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecorBatch));

    Expr::DeleteExprs(groupByKeys);
    Expr::DeleteExprs(aggAllKeys);
    omniruntime::op::Operator::DeleteOperator(aggWithExprOperator);
    delete aggWithExprOperatorFactory;
    VectorHelper::FreeVecBatch(expectVecorBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
    delete overflowConfig;
}

// last with empty groupKey (global aggregation, groupByNum=0)
TEST(AggregationWithExprOperatorTest, test_agg_last_expr)
{
    const int32_t dataSize = 8;
    const int32_t groupByNum = 0;
    const int32_t expectDataSize = 1;

    // last(c3), last(c4) => 8, 8.5f (last row: c3=8, c4=8.5f)
    int64_t data1[] = {2L, 5L, 8L, 11L, 14L, 17L, 20L, 23L}; // c0
    int64_t data2[] = {5L, 3L, 2L, 6L, 1L, 4L, 7L, 8L};      // c1
    int32_t data3[] = {5, 5, 5, 5, 5, 5, 5, 5};              // c2
    int32_t data4[] = {5, 3, 2, 6, 1, 4, 7, 8};              // c3
    float data5[] = {1.5f, 2.5f, 3.5f, 4.5f, 5.5f, 6.5f, 7.5f, 8.5f}; // c4

    DataTypes sourceTypes(std::vector<DataTypePtr>({ LongType(), LongType(), IntType(), IntType(), FloatType() }));
    DataTypes aggOutputTypes1(std::vector<DataTypePtr>({ IntType() }));
    DataTypes aggOutputTypes2(std::vector<DataTypePtr>({ FloatType() }));
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3, data4, data5);

    std::vector<Expr *> aggKeys1 = { new FieldExpr(3, IntType()) };
    std::vector<Expr *> aggKeys2 = { new FieldExpr(4, FloatType()) };
    std::vector<std::vector<omniruntime::expressions::Expr *>> aggAllKeys = { aggKeys1, aggKeys2 };

    std::vector<uint32_t> aggFuncTypes = { OMNI_AGGREGATION_TYPE_LAST_IGNORENULL,
        OMNI_AGGREGATION_TYPE_LAST_IGNORENULL };
    std::vector<uint32_t> maskCols = { static_cast<uint32_t>(-1), static_cast<uint32_t>(-1) };
    std::vector<DataTypes> aggsOutputTypes = { aggOutputTypes1, aggOutputTypes2 };
    auto overflowConfig = new OverflowConfig();
    std::vector<Expr *> groupByKeys = {};
    auto inputRawWrap = std::vector<bool>(aggFuncTypes.size(), true);
    auto outputPartialWrap = std::vector<bool>(aggFuncTypes.size(), false);
    std::vector<omniruntime::expressions::Expr *> aggFilters;
    aggFilters.reserve(2);
    aggFilters.push_back(nullptr);
    aggFilters.push_back(nullptr);

    auto *aggWithExprOperatorFactory =
        new AggregationWithExprOperatorFactory(groupByKeys, groupByNum, aggAllKeys, sourceTypes, aggsOutputTypes,
        aggFuncTypes, aggFilters, maskCols, inputRawWrap, outputPartialWrap, overflowConfig);
    auto *aggWithExprOperator =
        static_cast<AggregationWithExprOperator *>(CreateTestOperator(aggWithExprOperatorFactory));

    aggWithExprOperator->AddInput(vecBatch);
    VectorBatch *outputVecBatch = nullptr;
    aggWithExprOperator->GetOutput(&outputVecBatch);

    int64_t expData1[] = {8};
    float expData2[] = {8.5f};
    DataTypes expectTypes(std::vector<DataTypePtr>({ IntType(), FloatType() }));
    VectorBatch *expectVecorBatch = CreateVectorBatch(expectTypes, expectDataSize, expData1, expData2);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecorBatch));

    Expr::DeleteExprs(groupByKeys);
    Expr::DeleteExprs(aggAllKeys);
    omniruntime::op::Operator::DeleteOperator(aggWithExprOperator);
    delete aggWithExprOperatorFactory;
    VectorHelper::FreeVecBatch(expectVecorBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
    delete overflowConfig;
}

// last with non-empty groupKey (groupBy c0 mod 2, two groups)
TEST(AggregationWithExprOperatorTest, test_agg_last_expr_with_groupby)
{
    const int32_t dataSize = 6;
    const int32_t groupByNum = 1;
    const int32_t expectDataSize = 2;

    // c0: 2,4,6 -> group 0; c0: 1,3,5 -> group 1. last(c2) per group: group0 last c2=6, group1 last c2=5
    int64_t data1[] = {2L, 4L, 6L, 1L, 3L, 5L};   // c0
    int32_t data2[] = {10, 20, 30, 11, 21, 31};   // c1
    int32_t data3[] = {2, 4, 6, 1, 3, 5};         // c2

    DataTypes sourceTypes(std::vector<DataTypePtr>({ LongType(), IntType(), IntType() }));
    DataTypes aggOutputTypes1(std::vector<DataTypePtr>({ IntType() }));
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3);

    auto *modLeft = new FieldExpr(0, LongType());
    LiteralExpr *modRight = new LiteralExpr(2, LongType());
    modRight->longVal = 2;
    std::vector<Expr *> groupByKeys = { new BinaryExpr(omniruntime::expressions::Operator::MOD, modLeft, modRight, LongType()) };
    std::vector<Expr *> aggKeys1 = { new FieldExpr(2, IntType()) };
    std::vector<std::vector<Expr *>> aggAllKeys = { aggKeys1 };

    std::vector<uint32_t> aggFuncTypes = { OMNI_AGGREGATION_TYPE_LAST_IGNORENULL };
    std::vector<uint32_t> maskCols = { static_cast<uint32_t>(-1) };
    std::vector<DataTypes> aggsOutputTypes = { aggOutputTypes1 };
    auto inputRawWrap = std::vector<bool>(aggFuncTypes.size(), true);
    auto outputPartialWrap = std::vector<bool>(aggFuncTypes.size(), false);
    std::vector<Expr *> aggFilters = { nullptr };

    auto *aggFactory = new HashAggregationWithExprOperatorFactory(groupByKeys, groupByNum, aggAllKeys, aggFilters,
        sourceTypes, aggsOutputTypes, aggFuncTypes, maskCols, inputRawWrap, outputPartialWrap, OperatorConfig());
    auto *aggOperator = static_cast<HashAggregationWithExprOperator *>(CreateTestOperator(aggFactory));

    aggOperator->AddInput(vecBatch);
    VectorBatch *outputVecBatch = nullptr;
    aggOperator->GetOutput(&outputVecBatch);

    // group 0 (c0%2=0): last c2 = 6; group 1 (c0%2=1): last c2 = 5
    int64_t expGroup[] = {0, 1};
    int32_t expLast[] = {6, 5};
    DataTypes expectTypes(std::vector<DataTypePtr>({ LongType(), IntType() }));
    VectorBatch *expectVecBatch = CreateVectorBatch(expectTypes, expectDataSize, expGroup, expLast);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecBatch));

    Expr::DeleteExprs(groupByKeys);
    Expr::DeleteExprs(aggAllKeys);
    omniruntime::op::Operator::DeleteOperator(aggOperator);
    delete aggFactory;
    VectorHelper::FreeVecBatch(expectVecBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
}

TEST(HashAggregationWithExprOperatorTest, adaptor_header_with_null)
{
    OneRowAdaptor adaptor;
    std::vector<type::DataTypePtr> typeList;
    typeList.push_back(std::make_shared<type::DataType>(type::OMNI_INT));
    typeList.push_back(std::make_shared<type::DataType>(type::OMNI_DECIMAL128));
    type::DataTypes dataTypes(typeList);
    adaptor.Init(dataTypes);

    {
        type::Decimal128 decimal128(250);
        int32_t data = 1250;
        uintptr_t addresses[2] = {reinterpret_cast<uintptr_t>(&data),
                                  reinterpret_cast<uintptr_t>(&decimal128)};
        int32_t lens[2] = {0, 0};
        auto vecBatch1 = adaptor.Trans2VectorBatch(addresses, lens);
        auto v0 = reinterpret_cast<Vector<int32_t> *>(vecBatch1->Get(0));
        auto v1 = reinterpret_cast<Vector<Decimal128> *>(vecBatch1->Get(1));
        EXPECT_TRUE(not v0->IsNull(0));
        EXPECT_EQ(v0->GetValue(0), data);
        EXPECT_EQ(v1->GetValue(0), decimal128);
    }

    {
        type::Decimal128 decimal128(100);
        uintptr_t addresses[2] = {0,
                                  reinterpret_cast<uintptr_t>(&decimal128)};
        int32_t lens[2] = {-1, 0};
        auto vecBatch1 = adaptor.Trans2VectorBatch(addresses, lens);
        auto v0 = reinterpret_cast<Vector<int32_t> *>(vecBatch1->Get(0));
        auto v1 = reinterpret_cast<Vector<Decimal128> *>(vecBatch1->Get(1));
        EXPECT_TRUE(v0->IsNull(0));
        EXPECT_EQ(v1->GetValue(0), decimal128);
    }

    {
        uintptr_t addresses[2] = {0, 0};
        int32_t lens[2] = {-1, -1};
        auto vecBatch1 = adaptor.Trans2VectorBatch(addresses, lens);
        auto v0 = reinterpret_cast<Vector<int32_t> *>(vecBatch1->Get(0));
        auto v1 = reinterpret_cast<Vector<Decimal128> *>(vecBatch1->Get(1));
        EXPECT_TRUE(v0->IsNull(0));
        EXPECT_TRUE(v1->IsNull(0));
    }

    {
        OneRowAdaptor adaptor;
        std::vector<type::DataTypePtr> typeList;
        typeList.push_back(std::make_shared<type::DataType>(type::OMNI_INT));
        typeList.push_back(std::make_shared<type::DataType>(type::OMNI_VARCHAR));
        type::DataTypes dataTypes(typeList);
        adaptor.Init(dataTypes);

        int32_t data1 = 32;
        std::string hello = "hello";
        uintptr_t addresses[2] = {reinterpret_cast<uintptr_t>(&data1),
                                  reinterpret_cast<uintptr_t>(hello.data())};
        int32_t lens[2] = {0, 5};
        auto vecBatch1 = adaptor.Trans2VectorBatch(addresses, lens);
        auto v0 = reinterpret_cast<Vector<int32_t> *>(vecBatch1->Get(0));
        auto v1 = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(vecBatch1->Get(1));
        std::string_view strView = v1->GetValue(0);
        EXPECT_EQ(v0->GetValue(0), data1);
        auto len = strView.size();
        EXPECT_EQ(len, lens[1]);
        std::string str(reinterpret_cast<const char *>(strView.data()), len);
        EXPECT_EQ(hello.compare(str), 0);
    }
}

TEST(HashAggregationWithExprOperatorTest, DISABLED_test_hashagg_full_expr_by_proces_row)
{
    using namespace omniruntime::expressions;

    const int32_t dataSize = 8;
    const int32_t groupByNum = 2;
    const int32_t expectDataSize = 1;

    // prepare data
    // sum(c1*5), sum(c3+5) group by c0%3, c2+5  => 2, 10, 180, 76
    int64_t data1[] = {2L, 5L, 8L, 11L, 14L, 17L, 20L, 23L};
    int64_t data2[] = {5L, 3L, 2L, 6L, 1L, 4L, 7L, 8L};
    int32_t data3[] = {5, 5, 5, 5, 5, 5, 5, 5};
    int32_t data4[] = {5, 3, 2, 6, 1, 4, 7, 8};
    uint32_t colSize = 4;

    DataTypes sourceTypes(std::vector<DataTypePtr>({ LongType(), LongType(), IntType(), IntType() }));
    DataTypes aggOutputTypes(std::vector<DataTypePtr>({ LongType(), IntType() }));

    FieldExpr *modLeft = new FieldExpr(0, LongType());
    LiteralExpr *modRight = new LiteralExpr(3, LongType());
    modRight->longVal = 3;
    BinaryExpr *modExpr = new BinaryExpr(omniruntime::expressions::Operator::MOD, modLeft, modRight, LongType());
    FieldExpr *addLeft = new FieldExpr(2, IntType());
    LiteralExpr *addRight = new LiteralExpr(5, IntType());
    BinaryExpr *addExpr = new BinaryExpr(omniruntime::expressions::Operator::ADD, addLeft, addRight, IntType());
    std::vector<Expr *> groupByKeys = { modExpr, addExpr };

    FieldExpr *mulLeft = new FieldExpr(1, LongType());
    LiteralExpr *mulRight = new LiteralExpr(5, LongType());
    mulRight->longVal = 5;
    BinaryExpr *mulExpr = new BinaryExpr(omniruntime::expressions::Operator::MUL, mulLeft, mulRight, LongType());
    FieldExpr *addLeft2 = new FieldExpr(3, IntType());
    LiteralExpr *addRight2 = new LiteralExpr(5, IntType());
    BinaryExpr *addExpr2 = new BinaryExpr(omniruntime::expressions::Operator::ADD, addLeft2, addRight2, IntType());

    std::vector<Expr *> aggKeys1 = { mulExpr };
    std::vector<Expr *> aggKeys2 = { addExpr2 };
    std::vector<std::vector<omniruntime::expressions::Expr *>> aggAllKeys = { aggKeys1, aggKeys2 };
    std::vector<uint32_t> aggFuncTypes = { OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_SUM };
    std::vector<uint32_t> maskCols = { static_cast<uint32_t>(-1), static_cast<uint32_t>(-1) };

    auto aggOutputTypesWrap = AggregatorUtil::WrapWithVector(aggOutputTypes);
    auto inputRawWrap = std::vector<bool>(aggFuncTypes.size(), true);
    auto outputPartialWrap = std::vector<bool>(aggFuncTypes.size(), false);
    std::vector<omniruntime::expressions::Expr *> aggFilters;
    aggFilters.reserve(0);

    auto hashAggWithExprOperatorFactory =
        new HashAggregationWithExprOperatorFactory(groupByKeys, groupByNum, aggAllKeys, aggFilters, sourceTypes,
        aggOutputTypesWrap, aggFuncTypes, maskCols, inputRawWrap, outputPartialWrap, OperatorConfig());
    auto *hashAggWithExprOperator =
        dynamic_cast<HashAggregationWithExprOperator *>(CreateTestOperator(hashAggWithExprOperatorFactory));
    uintptr_t dataAddress[colSize];
    int32_t dataLens[colSize];

    for (uint32_t i = 0; i < dataSize; ++i) {
        dataAddress[0] = reinterpret_cast<uintptr_t>(&(data1[i]));
        dataAddress[1] = reinterpret_cast<uintptr_t>(&(data2[i]));
        dataAddress[2] = reinterpret_cast<uintptr_t>(&(data3[i]));
        dataAddress[3] = reinterpret_cast<uintptr_t>(&(data4[i]));
        dataLens[0] = 0;
        dataLens[1] = 0;
        dataLens[2] = 0;
        dataLens[3] = 0;
        hashAggWithExprOperator->ProcessRow(dataAddress, dataLens);
    }

    VectorBatch *outputVecBatch = nullptr;
    hashAggWithExprOperator->GetOutput(&outputVecBatch);

    int64_t expData1[] = {2};
    int32_t expData2[] = {10};
    int64_t expData3[] = {180};
    int32_t expData4[] = {76};
    DataTypes expectTypes(std::vector<DataTypePtr>({ LongType(), IntType(), LongType(), IntType() }));
    VectorBatch *expectVecorBatch =
        CreateVectorBatch(expectTypes, expectDataSize, expData1, expData2, expData3, expData4);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecorBatch));

    Expr::DeleteExprs(groupByKeys);
    Expr::DeleteExprs(aggAllKeys);
    omniruntime::op::Operator::DeleteOperator(hashAggWithExprOperator);
    delete hashAggWithExprOperatorFactory;
    VectorHelper::FreeVecBatch(expectVecorBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
}

TEST(AggregationWithExprOperatorTest, test_agg_sum_exprFilter)
{
    const int32_t dataSize = 8;
    const int32_t groupByNum = 0;
    const int32_t expectDataSize = 1;
    // prepare data
    // sum(c1*5), sum(c3) group by c0%3, c2 => 2, 10, 180, 36
    int64_t data1[] = {2L, 5L, 8L, 11L, 14L, 17L, 20L, 23L}; // c0
    int64_t data2[] = {5L, 3L, 2L, 6L, 1L, 4L, 7L, 8L};      // c1
    int32_t data3[] = {5, 5, 5, 5, 5, 5, 5, 5};              // c2
    int32_t data4[] = {5, 3, 2, 6, 1, 4, 7, 8};              // c3
    DataTypes sourceTypes(std::vector<DataTypePtr>({ LongType(), LongType(), IntType(), IntType() }));
    DataTypes aggOutputTypes(std::vector<DataTypePtr>({ LongType() }));
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3, data4);

    std::vector<Expr *> groupByKeys = {};
    // aggKeys
    LiteralExpr *mulRight = new LiteralExpr(5, LongType());
    mulRight->longVal = 5;
    BinaryExpr *mulExpr =
        new BinaryExpr(omniruntime::expressions::Operator::MUL, new FieldExpr(1, LongType()), mulRight, LongType());
    std::vector<Expr *> aggKeys1 = { mulExpr };
    std::vector<std::vector<omniruntime::expressions::Expr *>> aggAllKeys = { aggKeys1 };
    std::vector<uint32_t> aggFuncTypes = { OMNI_AGGREGATION_TYPE_SUM };
    std::vector<uint32_t> maskCols = { static_cast<uint32_t>(-1) };
    auto overflowConfig = new OverflowConfig();
    auto aggOutputTypesWrap = AggregatorUtil::WrapWithVector(aggOutputTypes);
    auto inputRawWrap = std::vector<bool>(aggFuncTypes.size(), true);
    auto outputPartialWrap = std::vector<bool>(aggFuncTypes.size(), false);
    std::vector<omniruntime::expressions::Expr *> aggFilters;
    auto *filterExpr = new BinaryExpr(omniruntime::expressions::Operator::LT, new FieldExpr(0, IntType()),
        new LiteralExpr(2000, IntType()), BooleanType());
    aggFilters.push_back(filterExpr);

    auto *aggWithExprOperatorFactory =
        new AggregationWithExprOperatorFactory(groupByKeys, groupByNum, aggAllKeys, sourceTypes, aggOutputTypesWrap,
        aggFuncTypes, aggFilters, maskCols, inputRawWrap, outputPartialWrap, overflowConfig);
    auto *aggWithExprOperator =
        static_cast<AggregationWithExprOperator *>(CreateTestOperator(aggWithExprOperatorFactory));
    aggWithExprOperator->AddInput(vecBatch);
    VectorBatch *outputVecBatch = nullptr;
    aggWithExprOperator->GetOutput(&outputVecBatch);

    int64_t expData1[] = {180};
    DataTypes expectTypes(std::vector<DataTypePtr>({ LongType() }));
    VectorBatch *expectVecorBatch = CreateVectorBatch(expectTypes, expectDataSize, expData1);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecorBatch));

    Expr::DeleteExprs(groupByKeys);
    Expr::DeleteExprs(aggAllKeys);
    Expr::DeleteExprs(aggFilters);
    omniruntime::op::Operator::DeleteOperator(aggWithExprOperator);
    delete aggWithExprOperatorFactory;
    VectorHelper::FreeVecBatch(expectVecorBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
    delete overflowConfig;
}

TEST(HashAggregationWithExprOperatorTest, test_hashagg_full_expr_filter)
{
    using namespace omniruntime::expressions;

    const int32_t dataSize = 8;
    const int32_t groupByNum = 2;
    const int32_t expectDataSize = 1;

    // prepare data
    // sum(c1*5), sum(c3+5) group by c0%3, c2+5  => 2, 10, 180, 76
    int64_t data1[] = {2L, 5L, 8L, 11L, 14L, 17L, 20L, 23L};
    int64_t data2[] = {5L, 3L, 2L, 6L, 1L, 4L, 7L, 8L};
    int32_t data3[] = {5, 5, 5, 5, 5, 5, 5, 5};
    int32_t data4[] = {5, 3, 2, 6, 1, 4, 7, 8};

    DataTypes sourceTypes(std::vector<DataTypePtr>({ LongType(), LongType(), IntType(), IntType() }));
    DataTypes aggOutputTypes(std::vector<DataTypePtr>({ LongType(), IntType() }));
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3, data4);

    FieldExpr *modLeft = new FieldExpr(0, LongType());
    LiteralExpr *modRight = new LiteralExpr(3, LongType());
    modRight->longVal = 3;
    BinaryExpr *modExpr = new BinaryExpr(omniruntime::expressions::Operator::MOD, modLeft, modRight, LongType());
    FieldExpr *addLeft = new FieldExpr(2, IntType());
    LiteralExpr *addRight = new LiteralExpr(5, IntType());
    BinaryExpr *addExpr = new BinaryExpr(omniruntime::expressions::Operator::ADD, addLeft, addRight, IntType());
    std::vector<Expr *> groupByKeys = { modExpr, addExpr };

    FieldExpr *mulLeft = new FieldExpr(1, LongType());
    LiteralExpr *mulRight = new LiteralExpr(5, LongType());
    mulRight->longVal = 5;
    BinaryExpr *mulExpr = new BinaryExpr(omniruntime::expressions::Operator::MUL, mulLeft, mulRight, LongType());
    FieldExpr *addLeft2 = new FieldExpr(3, IntType());
    LiteralExpr *addRight2 = new LiteralExpr(5, IntType());
    BinaryExpr *addExpr2 = new BinaryExpr(omniruntime::expressions::Operator::ADD, addLeft2, addRight2, IntType());

    std::vector<Expr *> aggKeys1 = { mulExpr };
    std::vector<Expr *> aggKeys2 = { addExpr2 };
    std::vector<std::vector<omniruntime::expressions::Expr *>> aggAllKeys = { aggKeys1, aggKeys2 };
    std::vector<uint32_t> aggFuncTypes = { OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_SUM };
    std::vector<uint32_t> maskCols = { static_cast<uint32_t>(-1), static_cast<uint32_t>(-1) };

    auto aggOutputTypesWrap = AggregatorUtil::WrapWithVector(aggOutputTypes);
    auto inputRawWrap = std::vector<bool>(aggFuncTypes.size(), true);
    auto outputPartialWrap = std::vector<bool>(aggFuncTypes.size(), false);

    std::vector<omniruntime::expressions::Expr *> aggFilters;
    auto *filterExpr = new BinaryExpr(omniruntime::expressions::Operator::LT, new FieldExpr(0, IntType()),
        new LiteralExpr(2000, IntType()), BooleanType());
    aggFilters.push_back(filterExpr);
    aggFilters.push_back(nullptr);

    auto hashAggWithExprOperatorFactory =
        new HashAggregationWithExprOperatorFactory(groupByKeys, groupByNum, aggAllKeys, aggFilters, sourceTypes,
        aggOutputTypesWrap, aggFuncTypes, maskCols, inputRawWrap, outputPartialWrap, OperatorConfig());
    auto *hashAggWithExprOperator =
        dynamic_cast<HashAggregationWithExprOperator *>(CreateTestOperator(hashAggWithExprOperatorFactory));

    hashAggWithExprOperator->AddInput(vecBatch);
    VectorBatch *outputVecBatch;
    hashAggWithExprOperator->GetOutput(&outputVecBatch);

    int64_t expData1[] = {2};
    int32_t expData2[] = {10};
    int64_t expData3[] = {180};
    int32_t expData4[] = {76};
    DataTypes expectTypes(std::vector<DataTypePtr>({ LongType(), IntType(), LongType(), IntType() }));
    VectorBatch *expectVecorBatch =
        CreateVectorBatch(expectTypes, expectDataSize, expData1, expData2, expData3, expData4);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecorBatch));

    Expr::DeleteExprs(groupByKeys);
    Expr::DeleteExprs(aggAllKeys);
    omniruntime::op::Operator::DeleteOperator(hashAggWithExprOperator);
    delete hashAggWithExprOperatorFactory;
    VectorHelper::FreeVecBatch(expectVecorBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
    Expr::DeleteExprs(aggFilters);
}

TEST(HashAggregationWithExprOperatorTest, test_agg_min_max_avg)
{
    ConfigUtil::SetSupportContainerVecRule(SupportContainerVecRule::NOT_SUPPORT);
    const int32_t dataSize = 8;
    std::string data0[] = {"IN", "IL", "FL", "TX", "IN", "IL", "FL", "TX"};
    std::string data1[] = {"F", "M", "F", "M", "F", "M", "F", "M"};
    std::string data2[] = {"W", "U", "U", "M", "W", "U", "U", "M"};
    int32_t data3[] = {0, 2, 5, 7, 0, 2, 5, 7};
    int32_t data4[] = {5, 3, 2, 6, 5, 3, 2, 6};
    int32_t data5[] = {1, 2, 3, 4, 1, 2, 3, 4};
    float data6[] = {1.5f, 2.5f, 3.5f, 4.5f, 1.5f, 2.5f, 3.5f, 4.5f};
    DataTypes sourceTypes(
        std::vector<DataTypePtr>({ VarcharType(2), VarcharType(2), VarcharType(2), IntType(), IntType(), IntType(),
        FloatType()}));
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, dataSize, data0, data1, data2, data3, data4, data5,
        data6);

    std::vector<Expr *> groupByKeys = { new FieldExpr(0, VarcharType(2)), new FieldExpr(1, VarcharType(2)),
        new FieldExpr(2, VarcharType(2)), new FieldExpr(3, IntType()),
        new FieldExpr(4, IntType()),      new FieldExpr(5, IntType()), new FieldExpr(6, FloatType()) };
    uint32_t groupByNum = 7;

    // aggKeys
    std::vector<Expr *> aggKeys0 = { new FieldExpr(3, IntType()) };
    std::vector<Expr *> aggKeys1 = { new FieldExpr(3, IntType()) };
    std::vector<Expr *> aggKeys2 = { new FuncExpr("CAST", std::vector<Expr *>{ new FieldExpr(3, IntType()) },
        LongType()) };
    std::vector<Expr *> aggKeys3 = { new FieldExpr(4, IntType()) };
    std::vector<Expr *> aggKeys4 = { new FieldExpr(4, IntType()) };
    std::vector<Expr *> aggKeys5 = { new FuncExpr("CAST", std::vector<Expr *>{ new FieldExpr(4, IntType()) },
        LongType()) };
    std::vector<Expr *> aggKeys6 = { new FieldExpr(5, IntType()) };
    std::vector<Expr *> aggKeys7 = { new FieldExpr(5, IntType()) };
    std::vector<Expr *> aggKeys8 = { new FuncExpr("CAST", std::vector<Expr *>{ new FieldExpr(5, IntType()) },
        LongType()) };
    std::vector<Expr *> aggKeys9 = { new FieldExpr(6, FloatType()) };
    std::vector<Expr *> aggKeys10 = { new FieldExpr(6, FloatType()) };
    std::vector<Expr *> aggKeys11 = { new FieldExpr(6, FloatType()) };
    std::vector<std::vector<omniruntime::expressions::Expr *>> aggAllKeys = { aggKeys0, aggKeys1, aggKeys2,
        aggKeys3, aggKeys4, aggKeys5,
        aggKeys6, aggKeys7, aggKeys8,
        aggKeys9, aggKeys10, aggKeys11 };

    std::vector<omniruntime::expressions::Expr *> aggFilters{};
    DataTypes aggOutputTypes0(std::vector<DataTypePtr>({ IntType() }));
    DataTypes aggOutputTypes1(std::vector<DataTypePtr>({ IntType() }));
    DataTypes aggOutputTypes2(std::vector<DataTypePtr>({ DoubleType(), LongType() }));
    DataTypes aggOutputTypes3(std::vector<DataTypePtr>({ IntType() }));
    DataTypes aggOutputTypes4(std::vector<DataTypePtr>({ IntType() }));
    DataTypes aggOutputTypes5(std::vector<DataTypePtr>({ DoubleType(), LongType() }));
    DataTypes aggOutputTypes6(std::vector<DataTypePtr>({ IntType() }));
    DataTypes aggOutputTypes7(std::vector<DataTypePtr>({ IntType() }));
    DataTypes aggOutputTypes8(std::vector<DataTypePtr>({ DoubleType(), LongType() }));
    DataTypes aggOutputTypes9(std::vector<DataTypePtr>({ FloatType() }));
    DataTypes aggOutputTypes10(std::vector<DataTypePtr>({ FloatType() }));
    DataTypes aggOutputTypes11(std::vector<DataTypePtr>({ DoubleType(), LongType() }));
    std::vector<DataTypes> aggOutputTypesWrap = { aggOutputTypes0, aggOutputTypes1, aggOutputTypes2,
        aggOutputTypes3, aggOutputTypes4, aggOutputTypes5,
        aggOutputTypes6, aggOutputTypes7, aggOutputTypes8,
        aggOutputTypes9, aggOutputTypes10, aggOutputTypes11};
    std::vector<uint32_t> aggFuncTypes = { OMNI_AGGREGATION_TYPE_MIN, OMNI_AGGREGATION_TYPE_MAX,
        OMNI_AGGREGATION_TYPE_AVG, OMNI_AGGREGATION_TYPE_MIN,
        OMNI_AGGREGATION_TYPE_MAX, OMNI_AGGREGATION_TYPE_AVG,
        OMNI_AGGREGATION_TYPE_MIN, OMNI_AGGREGATION_TYPE_MAX,
        OMNI_AGGREGATION_TYPE_AVG, OMNI_AGGREGATION_TYPE_MIN,
        OMNI_AGGREGATION_TYPE_MAX, OMNI_AGGREGATION_TYPE_AVG };
    auto maskCol = static_cast<uint32_t>(-1);
    std::vector<uint32_t> maskCols = {
        maskCol, maskCol, maskCol, maskCol, maskCol, maskCol, maskCol, maskCol, maskCol, maskCol, maskCol, maskCol
    };
    auto inputRawWrap = std::vector<bool>(aggFuncTypes.size(), true);
    auto outputPartialWrap = std::vector<bool>(aggFuncTypes.size(), true);

    auto hashAggWithExprOperatorFactory =
        new HashAggregationWithExprOperatorFactory(groupByKeys, groupByNum, aggAllKeys, aggFilters, sourceTypes,
        aggOutputTypesWrap, aggFuncTypes, maskCols, inputRawWrap, outputPartialWrap, OperatorConfig());
    auto *hashAggWithExprOperator =
        dynamic_cast<HashAggregationWithExprOperator *>(CreateTestOperator(hashAggWithExprOperatorFactory));

    hashAggWithExprOperator->AddInput(vecBatch);
    VectorBatch *outputVecBatch = nullptr;
    hashAggWithExprOperator->GetOutput(&outputVecBatch);

    const int32_t expectDataSize = 4;
    std::string expData0[] = { "FL", "IN", "IL", "TX" };
    std::string expData1[] = { "F", "F", "M", "M" };
    std::string expData2[] = { "U", "W", "U", "M" };
    int32_t expData3[] = {5, 0, 2, 7};
    int32_t expData4[] = {2, 5, 3, 6};
    int32_t expData5[] = {3, 1, 2, 4};
    float expData6[] = {4.5f, 2.5f, 3.5f, 1.5f};
    int32_t expData7[] = {5, 0, 2, 7};
    int32_t expData8[] = {5, 0, 2, 7};
    double expData9[] = {10, 0, 4, 14};
    int64_t expData10[] = {2, 2, 2, 2};
    int32_t expData11[] = {2, 5, 3, 6};
    int32_t expData12[] = {2, 5, 3, 6};
    double expData13[] = {4, 10, 6, 12};
    int64_t expData14[] = {2, 2, 2, 2};
    int32_t expData15[] = {3, 1, 2, 4};
    int32_t expData16[] = {3, 1, 2, 4};
    double expData17[] = {6, 2, 4, 8};
    int64_t expData18[] = {2, 2, 2, 2};
    float expData19[] = {4.5f, 2.5f, 3.5f, 1.5f};
    float expData20[] = {4.5f, 2.5f, 3.5f, 1.5f};
    double expData21[] = {9.0f, 5.0f, 7.0f, 3.0f};
    int64_t expData22[] = {2, 2, 2, 2};
    DataTypes expectTypes(std::vector<DataTypePtr>({ VarcharType(2), VarcharType(2), VarcharType(2), IntType(),
        IntType(), IntType(), FloatType(), IntType(), IntType(), DoubleType(), LongType(), IntType(), IntType(), DoubleType(),
        LongType(), IntType(), IntType(), DoubleType(), LongType(), FloatType(), FloatType(), DoubleType(), LongType() }));
    VectorBatch *expectVecorBatch = CreateVectorBatch(expectTypes, expectDataSize, expData0, expData1, expData2,
        expData3, expData4, expData5, expData6, expData7, expData8, expData9, expData10, expData11, expData12,
        expData13, expData14, expData15, expData16, expData17, expData18, expData19, expData20, expData21, expData22);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecorBatch));

    Expr::DeleteExprs(groupByKeys);
    Expr::DeleteExprs(aggAllKeys);
    Expr::DeleteExprs(aggFilters);
    omniruntime::op::Operator::DeleteOperator(hashAggWithExprOperator);
    delete hashAggWithExprOperatorFactory;
    VectorHelper::FreeVecBatch(expectVecorBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
    ConfigUtil::SetSupportContainerVecRule(SupportContainerVecRule::SUPPORT);
}

TEST(AggregationWithExprOperatorTest, test_agg_sum_literal)
{
    const int32_t dataSize = 8;
    int64_t data1[] = {2L, 5L, 8L, 11L, 14L, 17L, 20L, 23L}; // c0
    int64_t data2[] = {5L, 3L, 2L, 6L, 1L, 4L, 7L, 8L};      // c1
    int32_t data3[] = {5, 5, 5, 5, 5, 5, 5, 5};              // c2
    int32_t data4[] = {5, 3, 2, 6, 1, 4, 7, 8};              // c3
    DataTypes sourceTypes(std::vector<DataTypePtr>({ LongType(), LongType(), IntType(), IntType() }));
    DataTypes aggOutputTypes(std::vector<DataTypePtr>({ LongType(), LongType(), LongType() }));
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3, data4);

    std::vector<Expr *> groupByKeys = {};
    uint32_t groupByNum = 0;

    // aggKeys
    auto filedExpr0 = new FieldExpr(0, LongType());
    auto literalExpr1 = new LiteralExpr(0, LongType());
    literalExpr1->isNull = true;
    int64_t longVal = 1;
    auto literalExpr2 = new LiteralExpr(longVal, LongType());
    std::vector<Expr *> aggKeys0 = { filedExpr0 };
    std::vector<Expr *> aggKeys1 = { literalExpr1 };
    std::vector<Expr *> aggKeys2 = { literalExpr2 };
    std::vector<std::vector<omniruntime::expressions::Expr *>> aggAllKeys = { aggKeys0, aggKeys1, aggKeys2 };
    std::vector<uint32_t> aggFuncTypes = { OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_SUM,
        OMNI_AGGREGATION_TYPE_SUM };
    std::vector<uint32_t> maskCols = { static_cast<uint32_t>(-1), static_cast<uint32_t>(-1),
        static_cast<uint32_t>(-1) };
    auto overflowConfig = new OverflowConfig();
    auto aggOutputTypesWrap = AggregatorUtil::WrapWithVector(aggOutputTypes);
    auto inputRawWrap = std::vector<bool>(aggFuncTypes.size(), true);
    auto outputPartialWrap = std::vector<bool>(aggFuncTypes.size(), false);
    std::vector<omniruntime::expressions::Expr *> aggFilters(3, nullptr);

    auto *aggWithExprOperatorFactory =
        new AggregationWithExprOperatorFactory(groupByKeys, groupByNum, aggAllKeys, sourceTypes, aggOutputTypesWrap,
        aggFuncTypes, aggFilters, maskCols, inputRawWrap, outputPartialWrap, overflowConfig);
    auto *aggWithExprOperator =
        static_cast<AggregationWithExprOperator *>(CreateTestOperator(aggWithExprOperatorFactory));
    aggWithExprOperator->AddInput(vecBatch);
    VectorBatch *outputVecBatch = nullptr;
    aggWithExprOperator->GetOutput(&outputVecBatch);

    const int32_t expectDataSize = 1;
    int64_t expData1[] = { 100 };
    int64_t expData2[] = { 0 };
    int64_t expData3[] = { 8 };
    DataTypes expectTypes(std::vector<DataTypePtr>({ LongType(), LongType(), LongType() }));
    VectorBatch *expectVecorBatch = CreateVectorBatch(expectTypes, expectDataSize, expData1, expData2, expData3);
    expectVecorBatch->Get(1)->SetNull(0);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecorBatch));

    Expr::DeleteExprs(groupByKeys);
    Expr::DeleteExprs(aggAllKeys);
    Expr::DeleteExprs(aggFilters);
    omniruntime::op::Operator::DeleteOperator(aggWithExprOperator);
    delete aggWithExprOperatorFactory;
    VectorHelper::FreeVecBatch(expectVecorBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
    delete overflowConfig;
}

TEST(HashAggregationWithExprOperatorTest, test_hashagg_spill_with_invalid_config)
{
    const int32_t dataSize = 8;
    int64_t data1[] = {2L, 5L, 8L, 11L, 14L, 17L, 20L, 23L};
    int64_t data2[] = {5L, 3L, 2L, 6L, 1L, 4L, 7L, 8L};

    const int32_t groupByNum = 1;
    DataTypes sourceTypes(std::vector<DataTypePtr>({DoubleType(), LongType()}));
    DataTypes aggOutputTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));

    // groupByKeys
    std::vector<Expr *> groupByKeys = {new FieldExpr(0, DoubleType())};

    // aggKeys
    std::vector<Expr *> aggKeys1 = { new FieldExpr(1, LongType()) };
    std::vector<std::vector<omniruntime::expressions::Expr *>> aggAllKeys = { aggKeys1 };

    std::vector<uint32_t> aggFuncTypes = { OMNI_AGGREGATION_TYPE_SUM };
    std::vector<uint32_t> maskCols = { static_cast<uint32_t>(-1), static_cast<uint32_t>(-1) };

    auto aggOutputTypesWrap = AggregatorUtil::WrapWithVector(aggOutputTypes);
    auto inputRawWrap = std::vector<bool>(aggFuncTypes.size(), true);
    auto outputPartialWrap = std::vector<bool>(aggFuncTypes.size(), false);
    std::vector<omniruntime::expressions::Expr *> aggFilters;

    SparkSpillConfig spillConfig("", UINT64_MAX, 5);
    OperatorConfig operatorConfig(spillConfig);
    auto aggWithExprOperatorFactory =
        new HashAggregationWithExprOperatorFactory(groupByKeys, groupByNum, aggAllKeys, aggFilters, sourceTypes,
        aggOutputTypesWrap, aggFuncTypes, maskCols, inputRawWrap, outputPartialWrap, operatorConfig);
    omniruntime::mem::OmniTaskMemoryPool pool(nullptr);
    auto aggWithExprOperator = dynamic_cast<HashAggregationWithExprOperator *>(
        aggWithExprOperatorFactory->CreateOperator(&pool));
    ASSERT_NE(aggWithExprOperator, nullptr);
    aggWithExprOperator->setNoMoreInput(false);
    auto vecBatch = CreateVectorBatch(sourceTypes, dataSize, data1, data2);
    EXPECT_NO_THROW(aggWithExprOperator->AddInput(vecBatch));
    // Once a donor starts a destructive spill, a failure aborts reclaim via a nested
    // runtime_error ("Native reclaim failed: ...") carrying the OmniException cause.
    EXPECT_THROW(pool.reclaim(1), std::runtime_error);

    Expr::DeleteExprs(groupByKeys);
    Expr::DeleteExprs(aggAllKeys);
    omniruntime::op::Operator::DeleteOperator(aggWithExprOperator);
    delete aggWithExprOperatorFactory;
}

TEST(HashAggregationWithExprOperatorTest, test_hashagg_spill_with_no_aggNum)
{
    const int32_t dataSize = 4;
    const int32_t groupByNum = 8;
    const int32_t expectDataSize = 6;

    // prepare data
    // group by c0%3, c1, c2, c3, c4, c5, c6, c7
    int64_t data1[] = {0, 1, 3, 5};                                                     // c0
    int32_t data2[] = {2, 2, 2, 2};                                                     // c1
    int16_t data3[] = {1, 5, 1, 1};                                                     // c2
    bool data4[] = {true, false, true, true};                                           // c3
    double data5[] = {0.1, 1.1, 0.1, 0.1};                                              // c4
    std::string data6[] = {"", "5sf", "2w", "d4y4"};                                    // c5
    int64_t data7[] = {10, 100, 10, 100};                                               // c6
    Decimal128 data8[] = {Decimal128(-1, -2), 0, Decimal128(-1, -2), Decimal128(3, 2)}; // c7

    DataTypes sourceTypes(std::vector<DataTypePtr>({ LongType(), IntType(), ShortType(), BooleanType(), DoubleType(),
        VarcharType(10), Decimal64Type(), Decimal128Type(8, 2) }));
    VectorBatch *vecBatch =
        CreateVectorBatch(sourceTypes, dataSize, data1, data2, data3, data4, data5, data6, data7, data8);
    // set null
    vecBatch->Get(0)->SetNull(0);
    vecBatch->Get(1)->SetNull(3);
    vecBatch->Get(2)->SetNull(0);
    vecBatch->Get(3)->SetNull(2);
    vecBatch->Get(4)->SetNull(3);
    vecBatch->Get(6)->SetNull(2);
    vecBatch->Get(7)->SetNull(1);
    auto varCharVector = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(vecBatch->Get(5));
    varCharVector->SetNull(0);

    // group by c0%3, c1, c2, c3, c4, c5, c6, c7
    int64_t data12[] = {0, 1};                     // c0
    int32_t data22[] = {2, 2};                     // c1
    int16_t data32[] = {1, 5};                     // c2
    bool data42[] = {true, false};                 // c3
    double data52[] = {0.1, 1.1};                  // c4
    std::string data62[] = {"2w", "5sf"};          // c5
    int64_t data72[] = {10L, 100L};                // c6
    Decimal128 data82[] = {Decimal128(-1, -2), 0}; // c7
    VectorBatch *vecBatch2 =
        CreateVectorBatch(sourceTypes, 2, data12, data22, data32, data42, data52, data62, data72, data82);

    int64_t expData1[] = {0, 0, 1, 1, 2, 0};
    int32_t expData2[] = {2, 2, 2, 2, 2, 2};
    int16_t expData3[] = {1, 1, 5, 5, 1, 0};
    bool expData4[] = {true, true, false, false, true, true};
    double expData5[] = {0.1, 0.1, 1.1, 1.1, 0, 0.1};
    std::string expData6[] = {"2w", "2w", "5sf", "5sf", "d4y4", ""};
    int64_t expData7[] = {10L, 0L, 100L, 100L, 100L, 10L};
    Decimal128 expData8[] = {Decimal128(-1, -2), Decimal128(-1, -2), 0, 0, Decimal128(3, 2), Decimal128(-1, -2)};
    VectorBatch *expectVecorBatch = CreateVectorBatch(sourceTypes, expectDataSize, expData1, expData2, expData3,
        expData4, expData5, expData6, expData7, expData8);
    expectVecorBatch->Get(0)->SetNull(5);
    expectVecorBatch->Get(1)->SetNull(4);
    expectVecorBatch->Get(2)->SetNull(5);
    expectVecorBatch->Get(3)->SetNull(1);
    expectVecorBatch->Get(4)->SetNull(4);
    expectVecorBatch->Get(6)->SetNull(1);
    expectVecorBatch->Get(7)->SetNull(3);
    auto expectVarCharVector =
        reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(expectVecorBatch->Get(5));
    expectVarCharVector->SetNull(5);

    // groupByKeys
    LiteralExpr *modRight = new LiteralExpr(3, LongType());
    modRight->longVal = 3;
    BinaryExpr *modExpr =
        new BinaryExpr(omniruntime::expressions::Operator::MOD, new FieldExpr(0, LongType()), modRight, LongType());
    std::vector<Expr *> groupByKeys = { modExpr,
        new FieldExpr(1, IntType()),
        new FieldExpr(2, ShortType()),
        new FieldExpr(3, BooleanType()),
        new FieldExpr(4, DoubleType()),
        new FieldExpr(5, VarcharType(10)),
        new FieldExpr(6, Decimal64Type()),
        new FieldExpr(7, Decimal128Type(8, 2)) };

    // aggKeys
    std::vector<uint32_t> aggFuncTypes;
    std::vector<std::vector<omniruntime::expressions::Expr *>> aggAllKeys;
    std::vector<uint32_t> maskCols;
    std::vector<DataTypes> aggOutputTypesWrap;
    std::vector<bool> inputRawWrap;
    std::vector<bool> outputPartialWrap;
    std::vector<omniruntime::expressions::Expr *> aggFilters;

    SparkSpillConfig spillConfig(GenerateSpillPath(), INT32_MAX, 4);
    OperatorConfig operatorConfig(spillConfig);
    auto *hashAggWithExprOperatorFactory =
        new HashAggregationWithExprOperatorFactory(groupByKeys, groupByNum, aggAllKeys, aggFilters, sourceTypes,
        aggOutputTypesWrap, aggFuncTypes, maskCols, inputRawWrap, outputPartialWrap, operatorConfig);
    auto *hashAggWithExprOperator =
        dynamic_cast<HashAggregationWithExprOperator *>(CreateTestOperator(hashAggWithExprOperatorFactory));

    hashAggWithExprOperator->AddInput(vecBatch);
    hashAggWithExprOperator->AddInput(vecBatch2);
    VectorBatch *outputVecBatch = nullptr;
    hashAggWithExprOperator->GetOutput(&outputVecBatch);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecorBatch));

    Expr::DeleteExprs(groupByKeys);
    omniruntime::op::Operator::DeleteOperator(hashAggWithExprOperator);
    delete hashAggWithExprOperatorFactory;
    VectorHelper::FreeVecBatch(expectVecorBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
}

std::vector<VectorBatch *> PreparePartialInputData()
{
    const int32_t dataSize = 6;

    // agg(c2*5), agg(c3), agg(c4), agg(c5), agg(c6), agg(c7), agg(c8), agg(c9) group by c0%3, c1
    int64_t data0[] = {2, 5, 8, 11, 14, 17};                              // c0
    int32_t data1[] = {1, 5, 3, 5, 3, 5};                                 // c1
    int64_t data2[] = {5, 3, 2, 6, 1, 4};                                 // c2
    int32_t data3[] = {5, 3, 2, 6, 1, 4};                                 // c3
    int16_t data4[] = {5, 3, 2, 6, 1, 4};                                 // c4
    bool data5[] = {true, false, true, false, true, false};               // c5
    double data6[] = {1.2, 3.4, -1.2, -1.2, 0, 3.4};                      // c6
    std::string data7[] = {"1.20", "3.40", "-1.20", "-1.20", "", "3.40"}; // c7
    int64_t data8[] = {120, 340, -120, -120, 0, 340};                     // c8
    Decimal128 data9[] = {Decimal128("1.20"), Decimal128("3.40"), Decimal128("-1.20"), Decimal128("-1.20"), 0,
                          Decimal128("3.40")}; // c9

    DataTypes sourceTypes(std::vector<DataTypePtr>({ LongType(), IntType(), LongType(), IntType(), ShortType(),
        BooleanType(), DoubleType(), VarcharType(10), Decimal64Type(), Decimal128Type(8, 2) }));
    auto vecBatch1 =
        CreateVectorBatch(sourceTypes, dataSize, data0, data1, data2, data3, data4, data5, data6, data7, data8, data9);
    // set null
    vecBatch1->Get(1)->SetNull(2);
    vecBatch1->Get(2)->SetNull(2);
    vecBatch1->Get(3)->SetNull(2);
    vecBatch1->Get(4)->SetNull(2);
    vecBatch1->Get(5)->SetNull(2);
    vecBatch1->Get(6)->SetNull(4);
    vecBatch1->Get(8)->SetNull(4);
    vecBatch1->Get(9)->SetNull(4);
    auto varCharVector = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(vecBatch1->Get(7));
    varCharVector->SetNull(4);

    // agg(c2*5), agg(c3), agg(c4), agg(c5), agg(c6), agg(c7), agg(c8), agg(c9) group by c0%3, c1
    int64_t data02[] = {20, 23};                                      // c0
    int32_t data12[] = {5, 5};                                        // c1
    int64_t data22[] = {7, 8};                                        // c2
    int32_t data32[] = {7, 8};                                        // c3
    int16_t data42[] = {7, 8};                                        // c4
    bool data52[] = {false, false};                                   // c5
    double data62[] = {-1.2, -1.2};                                   // c6
    std::string data72[] = {"-1.20", "-1.20"};                        // c7
    int64_t data82[] = {-120, -120};                                  // c8
    Decimal128 data92[] = {Decimal128("-1.20"), Decimal128("-1.20")}; // c9
    auto vecBatch2 = CreateVectorBatch(sourceTypes, 2, data02, data12, data22, data32, data42, data52, data62, data72,
        data82, data92);

    std::vector<VectorBatch *> input;
    input.emplace_back(vecBatch1);
    input.emplace_back(vecBatch2);
    return input;
}

template <bool SupportContainerVecRule>
void SetAggKeys(const std::vector<DataTypePtr> &finalInputTypes, const std::vector<uint32_t> &aggFuncTypes,
    std::vector<std::vector<uint32_t>> &aggsCols, std::vector<DataTypes> &aggsInputTypes)
{
    auto aggIdx = 2;
    for (auto funcType : aggFuncTypes) {
        std::vector<uint32_t> aggCols;
        std::vector<DataTypePtr> aggInputType;
        switch (funcType) {
            case OMNI_AGGREGATION_TYPE_SUM: {
                aggCols.emplace_back(aggIdx);
                aggInputType.emplace_back(finalInputTypes[aggIdx]);
                if constexpr (!SupportContainerVecRule) {
                    if (finalInputTypes[aggIdx]->GetId() == OMNI_DECIMAL64 ||
                        finalInputTypes[aggIdx]->GetId() == OMNI_DECIMAL128) {
                        aggIdx++;
                        aggCols.emplace_back(aggIdx);
                        aggInputType.emplace_back(finalInputTypes[aggIdx]);
                        aggIdx++;
                    } else {
                        aggIdx++;
                    }
                } else {
                    aggIdx++;
                }
                break;
            }
            case OMNI_AGGREGATION_TYPE_COUNT_COLUMN:
            case OMNI_AGGREGATION_TYPE_COUNT_ALL:
            case OMNI_AGGREGATION_TYPE_MAX:
            case OMNI_AGGREGATION_TYPE_MIN: {
                aggCols.emplace_back(aggIdx);
                aggInputType.emplace_back(finalInputTypes[aggIdx]);
                aggIdx++;
                break;
            }
            case OMNI_AGGREGATION_TYPE_AVG: {
                if constexpr (SupportContainerVecRule) {
                    aggCols.emplace_back(aggIdx);
                    aggInputType.emplace_back(finalInputTypes[aggIdx]);
                    aggIdx++;
                } else {
                    aggCols.emplace_back(aggIdx);
                    aggInputType.emplace_back(finalInputTypes[aggIdx]);
                    aggIdx++;
                    aggCols.emplace_back(aggIdx);
                    aggInputType.emplace_back(finalInputTypes[aggIdx]);
                    aggIdx++;
                }
                break;
            }
            case OMNI_AGGREGATION_TYPE_FIRST_IGNORENULL:
            case OMNI_AGGREGATION_TYPE_FIRST_INCLUDENULL:
            case OMNI_AGGREGATION_TYPE_LAST_IGNORENULL:
            case OMNI_AGGREGATION_TYPE_LAST_INCLUDENULL: {
                aggCols.emplace_back(aggIdx);
                aggInputType.emplace_back(finalInputTypes[aggIdx]);
                aggIdx++;
                aggCols.emplace_back(aggIdx);
                aggInputType.emplace_back(finalInputTypes[aggIdx]);
                aggIdx++;
                break;
            }
            default:
                break;
        }
        aggsCols.emplace_back(aggCols);
        aggsInputTypes.emplace_back(DataTypes(aggInputType));
    }
}

template <bool InputRaw, bool OutputPartial, bool SupportContainerVecRule = false>
VectorBatch *TestHashAggMultiRecords(DataTypes &sourceTypes, std::vector<uint32_t> &aggFuncTypes,
    std::vector<DataTypes> &aggOutputTypes, std::vector<VectorBatch *> &finalInput, OperatorConfig &operatorConfig)
{
    if constexpr (SupportContainerVecRule) {
        ConfigUtil::SetSupportContainerVecRule(SupportContainerVecRule::SUPPORT);
    } else {
        ConfigUtil::SetSupportContainerVecRule(SupportContainerVecRule::NOT_SUPPORT);
    }

    std::vector<uint32_t> maskCols = { static_cast<uint32_t>(-1), static_cast<uint32_t>(-1), static_cast<uint32_t>(-1),
        static_cast<uint32_t>(-1), static_cast<uint32_t>(-1), static_cast<uint32_t>(-1),
        static_cast<uint32_t>(-1), static_cast<uint32_t>(-1) };
    auto inputRawWrap = std::vector<bool>(aggFuncTypes.size(), InputRaw);
    auto outputPartialWrap = std::vector<bool>(aggFuncTypes.size(), OutputPartial);
    std::vector<VectorBatch *> input;
    VectorBatch *outputVecBatch = nullptr;

    if constexpr (InputRaw) {
        input = PreparePartialInputData();

        // groupByKeys
        const int32_t groupByNum = 2;
        std::vector<Expr *> groupByKeys;
        auto modLeft = new FieldExpr(0, LongType());
        auto modRight = new LiteralExpr(3L, LongType());
        auto modExpr = new BinaryExpr(omniruntime::expressions::Operator::MOD, modLeft, modRight, LongType());
        groupByKeys.emplace_back(modExpr);
        groupByKeys.emplace_back(new FieldExpr(1, IntType()));

        // aggKeys
        std::vector<std::vector<omniruntime::expressions::Expr *>> aggAllKeys;
        auto mulLeft = new FieldExpr(2, LongType());
        auto mulRight = new LiteralExpr(5L, LongType());
        auto mulExpr = new BinaryExpr(omniruntime::expressions::Operator::MUL, mulLeft, mulRight, LongType());
        std::vector<Expr *> aggKeys1 = { mulExpr };
        std::vector<Expr *> aggKeys2 = { new FieldExpr(3, IntType()) };
        std::vector<Expr *> aggKeys3 = { new FieldExpr(4, ShortType()) };
        std::vector<Expr *> aggKeys4 = { new FieldExpr(5, BooleanType()) };
        std::vector<Expr *> aggKeys5 = { new FieldExpr(6, DoubleType()) };
        std::vector<Expr *> aggKeys6 = { new FieldExpr(7, VarcharType(10)) };
        std::vector<Expr *> aggKeys7 = { new FieldExpr(8, Decimal64Type()) };
        std::vector<Expr *> aggKeys8 = { new FieldExpr(9, Decimal128Type(8, 2)) };
        aggAllKeys.emplace_back(aggKeys1);
        aggAllKeys.emplace_back(aggKeys2);
        aggAllKeys.emplace_back(aggKeys3);
        aggAllKeys.emplace_back(aggKeys4);
        aggAllKeys.emplace_back(aggKeys5);
        aggAllKeys.emplace_back(aggKeys6);
        aggAllKeys.emplace_back(aggKeys7);
        aggAllKeys.emplace_back(aggKeys8);

        std::vector<omniruntime::expressions::Expr *> aggFilters;
        auto *hashAggWithExprOperatorFactory =
            new HashAggregationWithExprOperatorFactory(groupByKeys, groupByNum, aggAllKeys, aggFilters, sourceTypes,
            aggOutputTypes, aggFuncTypes, maskCols, inputRawWrap, outputPartialWrap, operatorConfig);
        auto *hashAggWithExprOperator =
            dynamic_cast<HashAggregationWithExprOperator *>(CreateTestOperator(hashAggWithExprOperatorFactory));

        for (auto inputVecBatch : input) {
            hashAggWithExprOperator->AddInput(inputVecBatch);
        }

        hashAggWithExprOperator->GetOutput(&outputVecBatch);
        omniruntime::op::Operator::DeleteOperator(hashAggWithExprOperator);
        delete hashAggWithExprOperatorFactory;
        Expr::DeleteExprs(groupByKeys);
        Expr::DeleteExprs(aggAllKeys);
    } else {
        std::vector<uint32_t> groupByCols({ 0, 1 });
        DataTypes groupInputTypes(std::vector<DataTypePtr>{ sourceTypes.GetType(0), sourceTypes.GetType(1) });
        std::vector<std::vector<uint32_t>> aggsCols;
        std::vector<DataTypes> aggInputTypes;
        SetAggKeys<SupportContainerVecRule>(sourceTypes.Get(), aggFuncTypes, aggsCols, aggInputTypes);
        input = finalInput;

        auto *hashAggOperatorFactory = new HashAggregationOperatorFactory(groupByCols, groupInputTypes, aggsCols,
            aggInputTypes, aggOutputTypes, aggFuncTypes, maskCols, inputRawWrap, outputPartialWrap, operatorConfig);
        hashAggOperatorFactory->Init();
        auto *hashAggOperator = dynamic_cast<HashAggregationOperator *>(CreateTestOperator(hashAggOperatorFactory));

        for (auto inputVecBatch : input) {
            hashAggOperator->AddInput(inputVecBatch);
        }
        hashAggOperator->GetOutput(&outputVecBatch);
        omniruntime::op::Operator::DeleteOperator(hashAggOperator);
        delete hashAggOperatorFactory;
    }

    return outputVecBatch;
}

template <bool InputRaw, bool OutputPartial, bool SupportContainerVecRule = false>
VectorBatch *TestHashAggWithOutSpillMultiRecords(DataTypes &sourceTypes, std::vector<uint32_t> &aggFuncTypes,
    std::vector<DataTypes> &aggOutputTypes, std::vector<VectorBatch *> &finalInput)
{
    SpillConfig spillConfig;
    OperatorConfig operatorConfig(spillConfig);
    return TestHashAggMultiRecords<InputRaw, OutputPartial, SupportContainerVecRule>(sourceTypes, aggFuncTypes,
        aggOutputTypes, finalInput, operatorConfig);
}

template <bool InputRaw, bool OutputPartial, bool SupportContainerVecRule>
VectorBatch *TestHashAggWithSpillMultiRecords(DataTypes &sourceTypes, std::vector<uint32_t> &aggFuncTypes,
    std::vector<DataTypes> &aggOutputTypes, std::vector<VectorBatch *> &finalInput)
{
    SparkSpillConfig spillConfig(GenerateSpillPath(), INT32_MAX, 4);
    OperatorConfig operatorConfig(spillConfig);
    return TestHashAggMultiRecords<InputRaw, OutputPartial, SupportContainerVecRule>(sourceTypes, aggFuncTypes,
        aggOutputTypes, finalInput, operatorConfig);
}

double CalculateHashAggSumValue()
{
    double data1[] = {3.40, -1.2, 3.40}; // c6
    double data2[] = {-1.2, -1.2};       // c6
    double sum1 = data1[0] + data1[1] + data1[2];
    double sum2 = data2[0] + data2[1];
    return sum1 + sum2;
}

TEST(HashAggregationWithExprOperatorTest, test_hashagg_sum_spill)
{
    DataTypes partialInputTypes(std::vector<DataTypePtr>({ LongType(), IntType(), LongType(), IntType(), ShortType(),
        BooleanType(), DoubleType(), VarcharType(10), Decimal64Type(), Decimal128Type(8, 2) }));
    std::vector<DataTypes> partialAggOutputTypes;
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ LongType() })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ LongType() })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ LongType() })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ BooleanType() })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ DoubleType() })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ VarcharType(10) })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ Decimal64Type(), BooleanType() })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ Decimal128Type(8, 2), BooleanType() })));

    std::vector<uint32_t> aggFuncTypes = { OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_SUM,
        OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_MAX,
        OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_MAX,
        OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_SUM };
    std::vector<VectorBatch *> expectInput;
    std::vector<VectorBatch *> resultInput;
    auto expectPartialVecBatch = TestHashAggWithOutSpillMultiRecords<true, true, false>(partialInputTypes, aggFuncTypes,
        partialAggOutputTypes, expectInput);
    auto resultPartialVecBatch = TestHashAggWithSpillMultiRecords<true, true, false>(partialInputTypes, aggFuncTypes,
        partialAggOutputTypes, resultInput);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(resultPartialVecBatch, expectPartialVecBatch, 5e-16));

    DataTypes finalInputTypes(
        std::vector<DataTypePtr>({ LongType(), IntType(), LongType(), LongType(), LongType(), BooleanType(),
        DoubleType(), VarcharType(10), Decimal64Type(), BooleanType(), Decimal128Type(8, 2), BooleanType() }));
    std::vector<DataTypes> finalAggOutputTypes;
    finalAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ LongType() })));
    finalAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ LongType() })));
    finalAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ LongType() })));
    finalAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ BooleanType() })));
    finalAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ DoubleType() })));
    finalAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ VarcharType(10) })));
    finalAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ Decimal64Type() })));
    finalAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ Decimal128Type(8, 2) })));
    expectInput.emplace_back(expectPartialVecBatch);
    auto expectFinalVecBatch = TestHashAggWithOutSpillMultiRecords<false, false, false>(finalInputTypes, aggFuncTypes,
        finalAggOutputTypes, expectInput);
    resultInput.emplace_back(resultPartialVecBatch);
    auto resultFinalVecBatch = TestHashAggWithSpillMultiRecords<false, false, false>(finalInputTypes, aggFuncTypes,
        finalAggOutputTypes, resultInput);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(resultFinalVecBatch, expectFinalVecBatch, 5e-16));

    VectorHelper::FreeVecBatch(expectFinalVecBatch);
    VectorHelper::FreeVecBatch(resultFinalVecBatch);
}

TEST(HashAggregationWithExprOperatorTest, test_hashagg_sum_container_support_spill)
{
    DataTypes partialInputTypes(std::vector<DataTypePtr>({ LongType(), IntType(), LongType(), IntType(), ShortType(),
        BooleanType(), DoubleType(), VarcharType(10), Decimal64Type(), Decimal128Type(8, 2) }));
    std::vector<DataTypes> partialAggOutputTypes;
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ LongType() })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ LongType() })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ LongType() })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ BooleanType() })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ DoubleType() })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ VarcharType(10) })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ Decimal64Type() })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ Decimal128Type(8, 2) })));
    std::vector<uint32_t> aggFuncTypes = { OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_SUM,
        OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_MAX,
        OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_MAX,
        OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_SUM };
    std::vector<VectorBatch *> expectInput;
    std::vector<VectorBatch *> resultInput;
    auto expectPartialVecBatch = TestHashAggWithOutSpillMultiRecords<true, true, true>(partialInputTypes, aggFuncTypes,
        partialAggOutputTypes, expectInput);
    auto resultPartialVecBatch = TestHashAggWithSpillMultiRecords<true, true, true>(partialInputTypes, aggFuncTypes,
        partialAggOutputTypes, resultInput);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(resultPartialVecBatch, expectPartialVecBatch, 5e-16));

    DataTypes finalInputTypes(std::vector<DataTypePtr>({ LongType(), IntType(), LongType(), LongType(), LongType(),
        BooleanType(), DoubleType(), VarcharType(10), Decimal64Type(), Decimal128Type(8, 2) }));
    std::vector<DataTypes> finalAggOutputTypes(partialAggOutputTypes);
    finalAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ LongType() })));
    finalAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ LongType() })));
    finalAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ LongType() })));
    finalAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ BooleanType() })));
    finalAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ DoubleType() })));
    finalAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ VarcharType(10) })));
    finalAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ Decimal64Type() })));
    finalAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ Decimal128Type(8, 2) })));

    expectInput.emplace_back(expectPartialVecBatch);
    resultInput.emplace_back(resultPartialVecBatch);
    auto expectFinalVecBatch = TestHashAggWithOutSpillMultiRecords<false, false, true>(finalInputTypes, aggFuncTypes,
        finalAggOutputTypes, expectInput);
    auto resultFinalVecBatch = TestHashAggWithSpillMultiRecords<false, false, true>(finalInputTypes, aggFuncTypes,
        finalAggOutputTypes, resultInput);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(resultFinalVecBatch, expectFinalVecBatch, 5e-16));

    VectorHelper::FreeVecBatch(resultFinalVecBatch);
    VectorHelper::FreeVecBatch(expectFinalVecBatch);
}

TEST(HashAggregationWithExprOperatorTest, test_hashagg_avg_spill)
{
    DataTypes partialInputTypes(std::vector<DataTypePtr>({ LongType(), IntType(), LongType(), IntType(), ShortType(),
        BooleanType(), DoubleType(), VarcharType(10), Decimal64Type(), Decimal128Type(8, 2) }));
    std::vector<DataTypes> partialAggOutputTypes;
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ DoubleType(), LongType() })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ DoubleType(), LongType() })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ DoubleType(), LongType() })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ BooleanType() })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ DoubleType(), LongType() })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ VarcharType(10) })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ Decimal64Type(), LongType() })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ Decimal128Type(8, 2), LongType() })));

    std::vector<uint32_t> aggFuncTypes = { OMNI_AGGREGATION_TYPE_AVG, OMNI_AGGREGATION_TYPE_AVG,
        OMNI_AGGREGATION_TYPE_AVG, OMNI_AGGREGATION_TYPE_MAX,
        OMNI_AGGREGATION_TYPE_AVG, OMNI_AGGREGATION_TYPE_MAX,
        OMNI_AGGREGATION_TYPE_AVG, OMNI_AGGREGATION_TYPE_AVG };
    std::vector<VectorBatch *> expectInput;
    std::vector<VectorBatch *> resultInput;
    auto expectPartialVecBatch = TestHashAggWithOutSpillMultiRecords<true, true, false>(partialInputTypes, aggFuncTypes,
        partialAggOutputTypes, expectInput);
    auto resultPartialVecBatch = TestHashAggWithSpillMultiRecords<true, true, false>(partialInputTypes, aggFuncTypes,
        partialAggOutputTypes, resultInput);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(resultPartialVecBatch, expectPartialVecBatch, 5e-16));

    DataTypes finalInputTypes(std::vector<DataTypePtr>({ LongType(), IntType(), DoubleType(), LongType(), DoubleType(),
        LongType(), DoubleType(), LongType(), BooleanType(), DoubleType(), LongType(), VarcharType(10), Decimal64Type(),
        LongType(), Decimal128Type(8, 2), LongType() }));
    std::vector<DataTypes> finalAggOutputTypes;
    finalAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ DoubleType() })));
    finalAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ DoubleType() })));
    finalAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ DoubleType() })));
    finalAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ BooleanType() })));
    finalAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ DoubleType() })));
    finalAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ VarcharType(10) })));
    finalAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ Decimal64Type() })));
    finalAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ Decimal128Type(8, 2) })));

    expectInput.emplace_back(expectPartialVecBatch);
    resultInput.emplace_back(resultPartialVecBatch);
    auto expectFinalVecBatch = TestHashAggWithOutSpillMultiRecords<false, false, false>(finalInputTypes, aggFuncTypes,
        finalAggOutputTypes, expectInput);
    auto resultFinalVecBatch = TestHashAggWithSpillMultiRecords<false, false, false>(finalInputTypes, aggFuncTypes,
        finalAggOutputTypes, resultInput);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(resultFinalVecBatch, expectFinalVecBatch, 5e-16));

    VectorHelper::FreeVecBatch(resultFinalVecBatch);
    VectorHelper::FreeVecBatch(expectFinalVecBatch);
}

TEST(HashAggregationWithExprOperatorTest, test_hashagg_avg_container_support_spill)
{
    ConfigUtil::SetSupportContainerVecRule(SupportContainerVecRule::SUPPORT);
    DataTypes partialInputTypes(std::vector<DataTypePtr>({ LongType(), IntType(), LongType(), IntType(), ShortType(),
        BooleanType(), DoubleType(), VarcharType(10), Decimal64Type(), Decimal128Type(8, 2) }));
    std::vector<DataTypes> partialAggOutputTypes;
    auto containerType1 = ContainerType(std::vector<DataTypePtr>({ DoubleType(), LongType() }));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ containerType1 })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ containerType1 })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ containerType1 })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ BooleanType() })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ containerType1 })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ VarcharType(10) })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ VarcharType(12) })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ VarcharType(12) })));
    std::vector<uint32_t> aggFuncTypes = { OMNI_AGGREGATION_TYPE_AVG, OMNI_AGGREGATION_TYPE_AVG,
        OMNI_AGGREGATION_TYPE_AVG, OMNI_AGGREGATION_TYPE_MAX,
        OMNI_AGGREGATION_TYPE_AVG, OMNI_AGGREGATION_TYPE_MAX,
        OMNI_AGGREGATION_TYPE_AVG, OMNI_AGGREGATION_TYPE_AVG };
    std::vector<VectorBatch *> expectInput;
    std::vector<VectorBatch *> resultInput;
    auto expectPartialVecBatch = TestHashAggWithOutSpillMultiRecords<true, true, true>(partialInputTypes, aggFuncTypes,
        partialAggOutputTypes, expectInput);
    auto resultPartialVecBatch = TestHashAggWithSpillMultiRecords<true, true, true>(partialInputTypes, aggFuncTypes,
        partialAggOutputTypes, resultInput);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(resultPartialVecBatch, expectPartialVecBatch, 5e-16));

    DataTypes finalInputTypes(std::vector<DataTypePtr>({ LongType(), IntType(), containerType1, containerType1,
        containerType1, BooleanType(), containerType1, VarcharType(10), VarcharType(12), VarcharType(12) }));
    std::vector<DataTypes> finalAggOutputTypes;
    finalAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ DoubleType() })));
    finalAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ DoubleType() })));
    finalAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ DoubleType() })));
    finalAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ BooleanType() })));
    finalAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ DoubleType() })));
    finalAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ VarcharType(10) })));
    finalAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ Decimal64Type() })));
    finalAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ Decimal128Type(8, 2) })));

    expectInput.emplace_back(expectPartialVecBatch);
    resultInput.emplace_back(resultPartialVecBatch);
    auto expectFinalVecBatch = TestHashAggWithOutSpillMultiRecords<false, false, true>(finalInputTypes, aggFuncTypes,
        finalAggOutputTypes, expectInput);
    auto resultFinalVecBatch = TestHashAggWithSpillMultiRecords<false, false, true>(finalInputTypes, aggFuncTypes,
        finalAggOutputTypes, resultInput);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(resultFinalVecBatch, expectFinalVecBatch, 5e-16));

    VectorHelper::FreeVecBatch(resultFinalVecBatch);
    VectorHelper::FreeVecBatch(expectFinalVecBatch);
}

TEST(HashAggregationWithExprOperatorTest, test_hashagg_count_spill)
{
    DataTypes partialInputTypes(std::vector<DataTypePtr>({ LongType(), IntType(), LongType(), IntType(), ShortType(),
        BooleanType(), DoubleType(), VarcharType(10), Decimal64Type(), Decimal128Type(8, 2) }));
    std::vector<DataTypes> partialAggOutputTypes;
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ LongType() })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ LongType() })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ LongType() })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ LongType() })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ LongType() })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ LongType() })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ LongType() })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ LongType() })));
    std::vector<uint32_t> aggFuncTypes = { OMNI_AGGREGATION_TYPE_COUNT_COLUMN, OMNI_AGGREGATION_TYPE_COUNT_COLUMN,
        OMNI_AGGREGATION_TYPE_COUNT_COLUMN, OMNI_AGGREGATION_TYPE_COUNT_COLUMN,
        OMNI_AGGREGATION_TYPE_COUNT_COLUMN, OMNI_AGGREGATION_TYPE_COUNT_COLUMN,
        OMNI_AGGREGATION_TYPE_COUNT_COLUMN, OMNI_AGGREGATION_TYPE_COUNT_COLUMN };
    std::vector<VectorBatch *> expectInput;
    std::vector<VectorBatch *> resultInput;
    auto expectPartialVecBatch = TestHashAggWithOutSpillMultiRecords<true, true, false>(partialInputTypes, aggFuncTypes,
        partialAggOutputTypes, expectInput);
    auto resultPartialVecBatch = TestHashAggWithSpillMultiRecords<true, true, false>(partialInputTypes, aggFuncTypes,
        partialAggOutputTypes, resultInput);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(resultPartialVecBatch, expectPartialVecBatch));

    DataTypes finalInputTypes(std::vector<DataTypePtr>({ LongType(), IntType(), LongType(), LongType(), LongType(),
        LongType(), LongType(), LongType(), LongType(), LongType() }));
    std::vector<DataTypes> finalAggOutputTypes(partialAggOutputTypes);
    expectInput.emplace_back(expectPartialVecBatch);
    resultInput.emplace_back(resultPartialVecBatch);
    auto expectFinalVecBatch = TestHashAggWithOutSpillMultiRecords<false, false, false>(finalInputTypes, aggFuncTypes,
        finalAggOutputTypes, expectInput);
    auto resultFinalVecBatch = TestHashAggWithSpillMultiRecords<false, false, false>(finalInputTypes, aggFuncTypes,
        finalAggOutputTypes, resultInput);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(resultFinalVecBatch, expectFinalVecBatch));

    VectorHelper::FreeVecBatch(resultFinalVecBatch);
    VectorHelper::FreeVecBatch(expectFinalVecBatch);
}

TEST(HashAggregationWithExprOperatorTest, test_hashagg_min_spill)
{
    DataTypes partialInputTypes(std::vector<DataTypePtr>({ LongType(), IntType(), LongType(), IntType(), ShortType(),
        BooleanType(), DoubleType(), VarcharType(10), Decimal64Type(), Decimal128Type(8, 2) }));
    std::vector<DataTypes> partialAggOutputTypes;
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ LongType() })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ IntType() })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ IntType() })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ BooleanType() })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ DoubleType() })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ VarcharType(10) })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ Decimal64Type() })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ Decimal128Type(8, 2) })));
    std::vector<uint32_t> aggFuncTypes = { OMNI_AGGREGATION_TYPE_MIN, OMNI_AGGREGATION_TYPE_MIN,
        OMNI_AGGREGATION_TYPE_MIN, OMNI_AGGREGATION_TYPE_MIN,
        OMNI_AGGREGATION_TYPE_MIN, OMNI_AGGREGATION_TYPE_MIN,
        OMNI_AGGREGATION_TYPE_MIN, OMNI_AGGREGATION_TYPE_MIN };
    std::vector<VectorBatch *> expectInput;
    std::vector<VectorBatch *> resultInput;
    auto expectPartialVecBatch = TestHashAggWithOutSpillMultiRecords<true, true, false>(partialInputTypes, aggFuncTypes,
        partialAggOutputTypes, expectInput);
    auto resultPartialVecBatch = TestHashAggWithSpillMultiRecords<true, true, false>(partialInputTypes, aggFuncTypes,
        partialAggOutputTypes, resultInput);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(resultPartialVecBatch, expectPartialVecBatch));

    DataTypes finalInputTypes(std::vector<DataTypePtr>({ LongType(), IntType(), LongType(), IntType(), IntType(),
        BooleanType(), DoubleType(), VarcharType(10), Decimal64Type(), Decimal128Type(8, 2) }));
    std::vector<DataTypes> finalAggOutputTypes(partialAggOutputTypes);
    expectInput.emplace_back(expectPartialVecBatch);
    resultInput.emplace_back(resultPartialVecBatch);
    auto expectFinalVecBatch = TestHashAggWithOutSpillMultiRecords<false, false, false>(finalInputTypes, aggFuncTypes,
        finalAggOutputTypes, expectInput);
    auto resultFinalVecBatch = TestHashAggWithSpillMultiRecords<false, false, false>(finalInputTypes, aggFuncTypes,
        finalAggOutputTypes, resultInput);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(resultFinalVecBatch, expectFinalVecBatch));

    VectorHelper::FreeVecBatch(resultFinalVecBatch);
    VectorHelper::FreeVecBatch(expectFinalVecBatch);
}

TEST(HashAggregationWithExprOperatorTest, test_hashagg_max_spill)
{
    DataTypes partialInputTypes(std::vector<DataTypePtr>({ LongType(), IntType(), LongType(), IntType(), ShortType(),
        BooleanType(), DoubleType(), VarcharType(10), Decimal64Type(), Decimal128Type(8, 2) }));
    std::vector<DataTypes> partialAggOutputTypes;
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ LongType() })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ IntType() })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ ShortType() })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ BooleanType() })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ DoubleType() })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ VarcharType(10) })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ Decimal64Type() })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ Decimal128Type(8, 2) })));
    std::vector<uint32_t> aggFuncTypes = { OMNI_AGGREGATION_TYPE_MAX, OMNI_AGGREGATION_TYPE_MAX,
        OMNI_AGGREGATION_TYPE_MAX, OMNI_AGGREGATION_TYPE_MAX,
        OMNI_AGGREGATION_TYPE_MAX, OMNI_AGGREGATION_TYPE_MAX,
        OMNI_AGGREGATION_TYPE_MAX, OMNI_AGGREGATION_TYPE_MAX };
    std::vector<VectorBatch *> expectInput;
    std::vector<VectorBatch *> resultInput;
    auto expectPartialVecBatch = TestHashAggWithOutSpillMultiRecords<true, true, false>(partialInputTypes, aggFuncTypes,
        partialAggOutputTypes, expectInput);
    auto resultPartialVecBatch = TestHashAggWithSpillMultiRecords<true, true, false>(partialInputTypes, aggFuncTypes,
        partialAggOutputTypes, resultInput);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(resultPartialVecBatch, expectPartialVecBatch));

    DataTypes finalInputTypes(std::vector<DataTypePtr>({ LongType(), IntType(), LongType(), IntType(), ShortType(),
        BooleanType(), DoubleType(), VarcharType(10), Decimal64Type(), Decimal128Type(8, 2) }));
    std::vector<DataTypes> finalAggOutputTypes(partialAggOutputTypes);
    expectInput.emplace_back(expectPartialVecBatch);
    resultInput.emplace_back(resultPartialVecBatch);
    auto expectFinalVecBatch = TestHashAggWithOutSpillMultiRecords<false, false, false>(finalInputTypes, aggFuncTypes,
        finalAggOutputTypes, expectInput);
    auto resultFinalVecBatch = TestHashAggWithSpillMultiRecords<false, false, false>(finalInputTypes, aggFuncTypes,
        finalAggOutputTypes, resultInput);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(resultFinalVecBatch, expectFinalVecBatch));

    VectorHelper::FreeVecBatch(resultFinalVecBatch);
    VectorHelper::FreeVecBatch(expectFinalVecBatch);
}

TEST(HashAggregationWithExprOperatorTest, test_hashagg_first_spill)
{
    DataTypes partialInputTypes(std::vector<DataTypePtr>({ LongType(), IntType(), LongType(), IntType(), ShortType(),
        BooleanType(), DoubleType(), VarcharType(10), Decimal64Type(), Decimal128Type(8, 2) }));
    std::vector<DataTypes> partialAggOutputTypes;
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ LongType(), BooleanType() })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ IntType(), BooleanType() })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ ShortType(), BooleanType() })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ BooleanType(), BooleanType() })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ DoubleType(), BooleanType() })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ VarcharType(10), BooleanType() })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ Decimal64Type(), BooleanType() })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ Decimal128Type(8, 2), BooleanType() })));
    std::vector<uint32_t> aggFuncTypes = {
        OMNI_AGGREGATION_TYPE_FIRST_IGNORENULL, OMNI_AGGREGATION_TYPE_FIRST_IGNORENULL,
        OMNI_AGGREGATION_TYPE_FIRST_IGNORENULL, OMNI_AGGREGATION_TYPE_FIRST_IGNORENULL,
        OMNI_AGGREGATION_TYPE_FIRST_IGNORENULL, OMNI_AGGREGATION_TYPE_FIRST_IGNORENULL,
        OMNI_AGGREGATION_TYPE_FIRST_IGNORENULL, OMNI_AGGREGATION_TYPE_FIRST_IGNORENULL
    };
    std::vector<VectorBatch *> expectInput;
    std::vector<VectorBatch *> resultInput;
    auto expectPartialVecBatch = TestHashAggWithOutSpillMultiRecords<true, true, false>(partialInputTypes, aggFuncTypes,
        partialAggOutputTypes, expectInput);
    auto resultPartialVecBatch = TestHashAggWithSpillMultiRecords<true, true, false>(partialInputTypes, aggFuncTypes,
        partialAggOutputTypes, resultInput);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(resultPartialVecBatch, expectPartialVecBatch));

    DataTypes finalInputTypes(std::vector<DataTypePtr>({ LongType(), IntType(), LongType(), BooleanType(), IntType(),
        BooleanType(), ShortType(), BooleanType(), BooleanType(), BooleanType(), DoubleType(), BooleanType(),
        VarcharType(10), BooleanType(), Decimal64Type(), BooleanType(), Decimal128Type(8, 2), BooleanType() }));
    std::vector<DataTypes> finalAggOutputTypes;
    finalAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ LongType() })));
    finalAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ IntType() })));
    finalAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ ShortType() })));
    finalAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ BooleanType() })));
    finalAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ DoubleType() })));
    finalAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ VarcharType(10) })));
    finalAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ Decimal64Type() })));
    finalAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ Decimal128Type(8, 2) })));

    expectInput.emplace_back(expectPartialVecBatch);
    resultInput.emplace_back(resultPartialVecBatch);
    auto expectFinalVecBatch = TestHashAggWithOutSpillMultiRecords<false, false, false>(finalInputTypes, aggFuncTypes,
        finalAggOutputTypes, expectInput);
    auto resultFinalVecBatch = TestHashAggWithSpillMultiRecords<false, false, false>(finalInputTypes, aggFuncTypes,
        finalAggOutputTypes, resultInput);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(resultFinalVecBatch, expectFinalVecBatch));

    VectorHelper::FreeVecBatch(resultFinalVecBatch);
    VectorHelper::FreeVecBatch(expectFinalVecBatch);
}

TEST(HashAggregationWithExprOperatorTest, test_hashagg_first_include_null_spill)
{
    DataTypes partialInputTypes(std::vector<DataTypePtr>({ LongType(), IntType(), LongType(), IntType(), ShortType(),
        BooleanType(), DoubleType(), VarcharType(10), Decimal64Type(), Decimal128Type(8, 2) }));
    std::vector<DataTypes> partialAggOutputTypes;
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ LongType(), BooleanType() })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ IntType(), BooleanType() })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ ShortType(), BooleanType() })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ BooleanType(), BooleanType() })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ DoubleType(), BooleanType() })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ VarcharType(10), BooleanType() })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ Decimal64Type(), BooleanType() })));
    partialAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ Decimal128Type(8, 2), BooleanType() })));
    std::vector<uint32_t> aggFuncTypes = {
        OMNI_AGGREGATION_TYPE_FIRST_INCLUDENULL, OMNI_AGGREGATION_TYPE_FIRST_INCLUDENULL,
        OMNI_AGGREGATION_TYPE_FIRST_INCLUDENULL, OMNI_AGGREGATION_TYPE_FIRST_INCLUDENULL,
        OMNI_AGGREGATION_TYPE_FIRST_INCLUDENULL, OMNI_AGGREGATION_TYPE_FIRST_INCLUDENULL,
        OMNI_AGGREGATION_TYPE_FIRST_INCLUDENULL, OMNI_AGGREGATION_TYPE_FIRST_INCLUDENULL
    };
    std::vector<VectorBatch *> expectInput;
    std::vector<VectorBatch *> resultInput;
    auto expectPartialVecBatch = TestHashAggWithOutSpillMultiRecords<true, true, false>(partialInputTypes, aggFuncTypes,
        partialAggOutputTypes, resultInput);
    auto resultPartialVecBatch = TestHashAggWithSpillMultiRecords<true, true, false>(partialInputTypes, aggFuncTypes,
        partialAggOutputTypes, resultInput);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(resultPartialVecBatch, expectPartialVecBatch));

    DataTypes finalInputTypes(std::vector<DataTypePtr>({ LongType(), IntType(), LongType(), BooleanType(), IntType(),
        BooleanType(), ShortType(), BooleanType(), BooleanType(), BooleanType(), DoubleType(), BooleanType(),
        VarcharType(10), BooleanType(), Decimal64Type(), BooleanType(), Decimal128Type(8, 2), BooleanType() }));
    std::vector<DataTypes> finalAggOutputTypes;
    finalAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ LongType() })));
    finalAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ IntType() })));
    finalAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ ShortType() })));
    finalAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ BooleanType() })));
    finalAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ DoubleType() })));
    finalAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ VarcharType(10) })));
    finalAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ Decimal64Type() })));
    finalAggOutputTypes.emplace_back(DataTypes(std::vector<DataTypePtr>({ Decimal128Type(8, 2) })));

    expectInput.emplace_back(expectPartialVecBatch);
    resultInput.emplace_back(resultPartialVecBatch);
    auto expectFinalVecBatch = TestHashAggWithOutSpillMultiRecords<false, false, false>(finalInputTypes, aggFuncTypes,
        finalAggOutputTypes, expectInput);
    auto resultFinalVecBatch = TestHashAggWithSpillMultiRecords<false, false, false>(finalInputTypes, aggFuncTypes,
        finalAggOutputTypes, resultInput);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(resultFinalVecBatch, expectFinalVecBatch));

    VectorHelper::FreeVecBatch(resultFinalVecBatch);
    VectorHelper::FreeVecBatch(expectFinalVecBatch);
}

template <bool SupportContainerVecRule = true>
void TestHashAggSpillWithNullRecords(std::vector<uint32_t> aggFuncTypes, DataTypes aggOutputTypes,
    VectorBatch *expectVecorBatch)
{
    if constexpr (SupportContainerVecRule) {
        ConfigUtil::SetSupportContainerVecRule(SupportContainerVecRule::NOT_SUPPORT);
    } else {
        ConfigUtil::SetSupportContainerVecRule(SupportContainerVecRule::SUPPORT);
    }

    const int32_t dataSize = 6;
    const int32_t groupByNum = 2;

    // prepare data
    // agg(c2*5), agg(c3), agg(c4), agg(c5), agg(c6), agg(c7), agg(c8), agg(c9) group by c0%3, c1
    int64_t data0[] = {2, 5, 8, 11, 14, 17};                // c0
    int32_t data1[] = {0, 1, 1, 3, 3, 3};                   // c1
    int64_t data2[] = {0, 0, 0, 0, 0, 0};                   // c2
    int32_t data3[] = {0, 0, 0, 0, 0, 0};                   // c3
    int16_t data4[] = {0, 0, 0, 0, 0, 0};                   // c4
    bool data5[] = {true, false, true, false, true, false}; // c5
    double data6[] = {0, 0, 0, 0, 0, 0};                    // c6
    std::string data7[] = {"", "", "", "", "", ""};         // c7
    int64_t data8[] = {0, 0, 0, 0, 0, 0};                   // c8
    Decimal128 data9[] = {0, 0, 0, 0, 0, 0};                // c9

    DataTypes sourceTypes(std::vector<DataTypePtr>({ LongType(), IntType(), LongType(), IntType(), ShortType(),
        BooleanType(), DoubleType(), VarcharType(10), Decimal64Type(), Decimal128Type(8, 2) }));
    VectorBatch *vecBatch1 =
        CreateVectorBatch(sourceTypes, dataSize, data0, data1, data2, data3, data4, data5, data6, data7, data8, data9);
    // set null
    vecBatch1->Get(0)->SetNull(0);
    vecBatch1->Get(1)->SetNull(0);
    for (int i = 0; i < dataSize; i++) {
        vecBatch1->Get(2)->SetNull(i);
        vecBatch1->Get(3)->SetNull(i);
        vecBatch1->Get(4)->SetNull(i);
        vecBatch1->Get(5)->SetNull(i);
        vecBatch1->Get(6)->SetNull(i);
        vecBatch1->Get(8)->SetNull(i);
        vecBatch1->Get(9)->SetNull(i);
        auto varCharVector = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(vecBatch1->Get(7));
        varCharVector->SetNull(i);
    }

    // agg(c2*5), agg(c3), agg(c4), agg(c5), agg(c6), agg(c7), agg(c8), agg(c9) group by c0%3, c1
    int64_t data02[] = {20, 23};     // c0
    int32_t data12[] = {5, 5};       // c1
    int64_t data22[] = {0, 0};       // c2
    int32_t data32[] = {0, 0};       // c3
    int16_t data42[] = {0, 0};       // c4
    bool data52[] = {false, false};  // c5
    double data62[] = {0, 0};        // c6
    std::string data72[] = {"", ""}; // c7
    int64_t data82[] = {0, 0};       // c8
    Decimal128 data92[] = {0, 0};    // c9
    VectorBatch *vecBatch2 = CreateVectorBatch(sourceTypes, 2, data02, data12, data22, data32, data42, data52, data62,
        data72, data82, data92);
    for (int i = 0; i < 2; i++) {
        vecBatch2->Get(2)->SetNull(i);
        vecBatch2->Get(3)->SetNull(i);
        vecBatch2->Get(4)->SetNull(i);
        vecBatch2->Get(5)->SetNull(i);
        vecBatch2->Get(6)->SetNull(i);
        vecBatch2->Get(8)->SetNull(i);
        vecBatch2->Get(9)->SetNull(i);
        auto varCharVector = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(vecBatch2->Get(7));
        varCharVector->SetNull(i);
    }

    // groupByKeys
    LiteralExpr *modRight = new LiteralExpr(3, LongType());
    modRight->longVal = 3;
    BinaryExpr *modExpr =
        new BinaryExpr(omniruntime::expressions::Operator::MOD, new FieldExpr(0, LongType()), modRight, LongType());
    std::vector<Expr *> groupByKeys = { modExpr, new FieldExpr(1, IntType()) };

    // aggKeys
    LiteralExpr *mulRight = new LiteralExpr(5, LongType());
    mulRight->longVal = 5;
    BinaryExpr *mulExpr =
        new BinaryExpr(omniruntime::expressions::Operator::MUL, new FieldExpr(2, LongType()), mulRight, LongType());
    std::vector<Expr *> aggKeys1 = { mulExpr };
    std::vector<Expr *> aggKeys2 = { new FieldExpr(3, IntType()) };
    std::vector<Expr *> aggKeys3 = { new FieldExpr(4, ShortType()) };
    std::vector<Expr *> aggKeys4 = { new FieldExpr(5, BooleanType()) };
    std::vector<Expr *> aggKeys5 = { new FieldExpr(6, DoubleType()) };
    std::vector<Expr *> aggKeys6 = { new FieldExpr(7, VarcharType(10)) };
    std::vector<Expr *> aggKeys7 = { new FieldExpr(8, Decimal64Type()) };
    std::vector<Expr *> aggKeys8 = { new FieldExpr(9, Decimal128Type(8, 2)) };
    std::vector<std::vector<omniruntime::expressions::Expr *>> aggAllKeys = { aggKeys1, aggKeys2, aggKeys3, aggKeys4,
                                                                              aggKeys5, aggKeys6, aggKeys7, aggKeys8 };

    std::vector<uint32_t> maskCols = { static_cast<uint32_t>(-1), static_cast<uint32_t>(-1), static_cast<uint32_t>(-1),
        static_cast<uint32_t>(-1), static_cast<uint32_t>(-1), static_cast<uint32_t>(-1),
        static_cast<uint32_t>(-1), static_cast<uint32_t>(-1) };

    SparkSpillConfig spillConfig(GenerateSpillPath(), INT32_MAX, 4);
    OperatorConfig operatorConfig(spillConfig);

    auto aggOutputTypesWrap = AggregatorUtil::WrapWithVector(aggOutputTypes);
    auto inputRawWrap = std::vector<bool>(aggFuncTypes.size(), true);
    auto outputPartialWrap = std::vector<bool>(aggFuncTypes.size(), false);
    std::vector<omniruntime::expressions::Expr *> aggFilters;
    auto *hashAggWithExprOperatorFactory =
        new HashAggregationWithExprOperatorFactory(groupByKeys, groupByNum, aggAllKeys, aggFilters, sourceTypes,
        aggOutputTypesWrap, aggFuncTypes, maskCols, inputRawWrap, outputPartialWrap, operatorConfig);
    auto *hashAggWithExprOperator =
        dynamic_cast<HashAggregationWithExprOperator *>(CreateTestOperator(hashAggWithExprOperatorFactory));

    hashAggWithExprOperator->AddInput(vecBatch1);
    hashAggWithExprOperator->AddInput(vecBatch2);
    VectorBatch *outputVecBatch = nullptr;
    hashAggWithExprOperator->GetOutput(&outputVecBatch);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecorBatch));

    omniruntime::op::Operator::DeleteOperator(hashAggWithExprOperator);
    delete hashAggWithExprOperatorFactory;
    Expr::DeleteExprs(groupByKeys);
    Expr::DeleteExprs(aggAllKeys);
    VectorHelper::FreeVecBatch(outputVecBatch);
    VectorHelper::FreeVecBatch(expectVecorBatch);
}

TEST(HashAggregationWithExprOperatorTest, test_hashagg_null_record_sum_spill)
{
    const int32_t expectDataSize = 4;
    int64_t expData0[] = {2, 2, 2, 2};
    int32_t expData1[] = {1, 3, 5, 0};
    int64_t expData2[] = {0, 0, 0, 0};           // c2
    int64_t expData3[] = {0, 0, 0, 0};           // c3
    int64_t expData4[] = {0, 0, 0, 0};           // c4
    bool expData5[] = {true, true, false, true}; // c5
    double expData6[] = {0, 0, 0, 0};            // c6
    std::string expData7[] = {"", "", "", ""};   // c7
    int64_t expData8[] = {0, 0, 0, 0};           // c8
    Decimal128 expData9[] = {0, 0, 0, 0};        // c9

    DataTypes expectTypes(std::vector<DataTypePtr>({ LongType(), IntType(), LongType(), LongType(), LongType(),
        BooleanType(), DoubleType(), VarcharType(10), Decimal64Type(), Decimal128Type(8, 2) }));
    VectorBatch *expectVecorBatch = CreateVectorBatch(expectTypes, expectDataSize, expData0, expData1, expData2,
        expData3, expData4, expData5, expData6, expData7, expData8, expData9);
    expectVecorBatch->Get(0)->SetNull(3);
    expectVecorBatch->Get(1)->SetNull(3);
    for (int i = 0; i < expectDataSize; i++) {
        expectVecorBatch->Get(2)->SetNull(i);
        expectVecorBatch->Get(3)->SetNull(i);
        expectVecorBatch->Get(4)->SetNull(i);
        expectVecorBatch->Get(5)->SetNull(i);
        expectVecorBatch->Get(6)->SetNull(i);
        expectVecorBatch->Get(8)->SetNull(i);
        expectVecorBatch->Get(9)->SetNull(i);
        auto varCharVector =
            reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(expectVecorBatch->Get(7));
        varCharVector->SetNull(i);
    }

    DataTypes aggOutputTypes(std::vector<DataTypePtr>({ LongType(), LongType(), LongType(), BooleanType(), DoubleType(),
        VarcharType(10), Decimal64Type(), Decimal128Type(8, 2) }));
    std::vector<uint32_t> aggFuncTypes = { OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_SUM,
        OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_MAX,
        OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_MAX,
        OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_SUM };
    TestHashAggSpillWithNullRecords(aggFuncTypes, aggOutputTypes, expectVecorBatch);
}

TEST(HashAggregationWithExprOperatorTest, test_hashagg_null_record_sum_container_support_spill)
{
    const int32_t expectDataSize = 4;
    int64_t expData0[] = {2, 2, 2, 2};
    int32_t expData1[] = {1, 3, 5, 0};
    int64_t expData2[] = {0, 0, 0, 0};           // c2
    int64_t expData3[] = {0, 0, 0, 0};           // c3
    int64_t expData4[] = {0, 0, 0, 0};           // c4
    bool expData5[] = {true, true, false, true}; // c5
    double expData6[] = {0, 0, 0, 0};            // c6
    std::string expData7[] = {"", "", "", ""};   // c7
    int64_t expData8[] = {0, 0, 0, 0};           // c8
    Decimal128 expData9[] = {0, 0, 0, 0};        // c9

    DataTypes expectTypes(std::vector<DataTypePtr>({ LongType(), IntType(), LongType(), LongType(), LongType(),
        BooleanType(), DoubleType(), VarcharType(10), Decimal64Type(), Decimal128Type(8, 2) }));
    VectorBatch *expectVecorBatch = CreateVectorBatch(expectTypes, expectDataSize, expData0, expData1, expData2,
        expData3, expData4, expData5, expData6, expData7, expData8, expData9);
    expectVecorBatch->Get(0)->SetNull(3);
    expectVecorBatch->Get(1)->SetNull(3);
    for (int i = 0; i < expectDataSize; i++) {
        expectVecorBatch->Get(2)->SetNull(i);
        expectVecorBatch->Get(3)->SetNull(i);
        expectVecorBatch->Get(4)->SetNull(i);
        expectVecorBatch->Get(5)->SetNull(i);
        expectVecorBatch->Get(6)->SetNull(i);
        expectVecorBatch->Get(8)->SetNull(i);
        expectVecorBatch->Get(9)->SetNull(i);
        auto varCharVector =
            reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(expectVecorBatch->Get(7));
        varCharVector->SetNull(i);
    }

    DataTypes aggOutputTypes(std::vector<DataTypePtr>({ LongType(), LongType(), LongType(), BooleanType(), DoubleType(),
        VarcharType(10), Decimal64Type(), Decimal128Type(8, 2) }));
    std::vector<uint32_t> aggFuncTypes = { OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_SUM,
        OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_MAX,
        OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_MAX,
        OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_SUM };
    TestHashAggSpillWithNullRecords<false>(aggFuncTypes, aggOutputTypes, expectVecorBatch);
}

TEST(HashAggregationWithExprOperatorTest, test_hashagg_null_record_avg_spill)
{
    const int32_t expectDataSize = 4;
    int64_t expData0[] = {2, 2, 2, 2};
    int32_t expData1[] = {1, 3, 5, 0};
    int64_t expData2[] = {0, 0, 0, 0};           // c2
    int64_t expData3[] = {0, 0, 0, 0};           // c3
    int64_t expData4[] = {0, 0, 0, 0};           // c4
    bool expData5[] = {true, true, false, true}; // c5
    double expData6[] = {0, 0, 0, 0};            // c6
    std::string expData7[] = {"", "", "", ""};   // c7
    int64_t expData8[] = {0, 0, 0, 0};           // c8
    Decimal128 expData9[] = {0, 0, 0, 0};        // c9

    DataTypes expectTypes(std::vector<DataTypePtr>({ LongType(), IntType(), LongType(), LongType(), LongType(),
        BooleanType(), DoubleType(), VarcharType(10), Decimal64Type(), Decimal128Type(8, 2) }));
    VectorBatch *expectVecorBatch = CreateVectorBatch(expectTypes, expectDataSize, expData0, expData1, expData2,
        expData3, expData4, expData5, expData6, expData7, expData8, expData9);
    expectVecorBatch->Get(0)->SetNull(3);
    expectVecorBatch->Get(1)->SetNull(3);
    for (int i = 0; i < expectDataSize; i++) {
        expectVecorBatch->Get(2)->SetNull(i);
        expectVecorBatch->Get(3)->SetNull(i);
        expectVecorBatch->Get(4)->SetNull(i);
        expectVecorBatch->Get(5)->SetNull(i);
        expectVecorBatch->Get(6)->SetNull(i);
        expectVecorBatch->Get(8)->SetNull(i);
        expectVecorBatch->Get(9)->SetNull(i);
        auto varCharVector =
            reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(expectVecorBatch->Get(7));
        varCharVector->SetNull(i);
    }

    DataTypes aggOutputTypes(std::vector<DataTypePtr>({ LongType(), LongType(), LongType(), BooleanType(), DoubleType(),
        VarcharType(10), Decimal64Type(), Decimal128Type(8, 2) }));
    std::vector<uint32_t> aggFuncTypes = { OMNI_AGGREGATION_TYPE_AVG, OMNI_AGGREGATION_TYPE_AVG,
        OMNI_AGGREGATION_TYPE_AVG, OMNI_AGGREGATION_TYPE_MAX,
        OMNI_AGGREGATION_TYPE_AVG, OMNI_AGGREGATION_TYPE_MAX,
        OMNI_AGGREGATION_TYPE_AVG, OMNI_AGGREGATION_TYPE_AVG };
    TestHashAggSpillWithNullRecords(aggFuncTypes, aggOutputTypes, expectVecorBatch);
}

TEST(HashAggregationWithExprOperatorTest, test_hashagg_null_record_avg_container_support_spill)
{
    const int32_t expectDataSize = 4;
    int64_t expData0[] = {2, 2, 2, 2};
    int32_t expData1[] = {1, 3, 5, 0};
    double expData2[] = {0, 0, 0, 0};            // c2
    double expData3[] = {0, 0, 0, 0};            // c3
    double expData4[] = {0, 0, 0, 0};            // c4
    bool expData5[] = {true, true, false, true}; // c5
    double expData6[] = {0, 0, 0, 0};            // c6
    std::string expData7[] = {"", "", "", ""};   // c7
    int64_t expData8[] = {0, 0, 0, 0};           // c8
    Decimal128 expData9[] = {0, 0, 0, 0};        // c9

    DataTypes expectTypes(std::vector<DataTypePtr>({ LongType(), IntType(), DoubleType(), DoubleType(), DoubleType(),
        BooleanType(), DoubleType(), VarcharType(10), Decimal64Type(), Decimal128Type(8, 2) }));
    VectorBatch *expectVecorBatch = CreateVectorBatch(expectTypes, expectDataSize, expData0, expData1, expData2,
        expData3, expData4, expData5, expData6, expData7, expData8, expData9);
    expectVecorBatch->Get(0)->SetNull(3);
    expectVecorBatch->Get(1)->SetNull(3);
    for (int i = 0; i < expectDataSize; i++) {
        expectVecorBatch->Get(2)->SetNull(i);
        expectVecorBatch->Get(3)->SetNull(i);
        expectVecorBatch->Get(4)->SetNull(i);
        expectVecorBatch->Get(5)->SetNull(i);
        expectVecorBatch->Get(6)->SetNull(i);
        expectVecorBatch->Get(8)->SetNull(i);
        expectVecorBatch->Get(9)->SetNull(i);
        auto varCharVector =
            reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(expectVecorBatch->Get(7));
        varCharVector->SetNull(i);
    }

    DataTypes aggOutputTypes(std::vector<DataTypePtr>({ DoubleType(), DoubleType(), DoubleType(), BooleanType(),
        DoubleType(), VarcharType(10), Decimal64Type(), Decimal128Type(8, 2) }));
    std::vector<uint32_t> aggFuncTypes = { OMNI_AGGREGATION_TYPE_AVG, OMNI_AGGREGATION_TYPE_AVG,
        OMNI_AGGREGATION_TYPE_AVG, OMNI_AGGREGATION_TYPE_MAX,
        OMNI_AGGREGATION_TYPE_AVG, OMNI_AGGREGATION_TYPE_MAX,
        OMNI_AGGREGATION_TYPE_AVG, OMNI_AGGREGATION_TYPE_AVG };
    TestHashAggSpillWithNullRecords<false>(aggFuncTypes, aggOutputTypes, expectVecorBatch);
}

TEST(HashAggregationWithExprOperatorTest, test_hashagg_null_record_count_spill)
{
    const int32_t expectDataSize = 4;
    int64_t expData0[] = {2, 2, 2, 2};
    int32_t expData1[] = {1, 3, 5, 0};
    int64_t expData2[] = {0, 0, 0, 0};
    int64_t expData3[] = {0, 0, 0, 0};
    int64_t expData4[] = {0, 0, 0, 0};
    int64_t expData5[] = {0, 0, 0, 0};
    int64_t expData6[] = {0, 0, 0, 0};
    int64_t expData7[] = {0, 0, 0, 0};
    int64_t expData8[] = {0, 0, 0, 0};
    int64_t expData9[] = {0, 0, 0, 0};

    DataTypes expectTypes(std::vector<DataTypePtr>({ LongType(), IntType(), LongType(), LongType(), LongType(),
        LongType(), LongType(), LongType(), LongType(), LongType() }));
    VectorBatch *expectVecorBatch = CreateVectorBatch(expectTypes, expectDataSize, expData0, expData1, expData2,
        expData3, expData4, expData5, expData6, expData7, expData8, expData9);
    expectVecorBatch->Get(0)->SetNull(3);
    expectVecorBatch->Get(1)->SetNull(3);

    DataTypes aggOutputTypes(std::vector<DataTypePtr>(
        { LongType(), LongType(), LongType(), LongType(), LongType(), LongType(), LongType(), LongType() }));
    std::vector<uint32_t> aggFuncTypes = { OMNI_AGGREGATION_TYPE_COUNT_COLUMN, OMNI_AGGREGATION_TYPE_COUNT_COLUMN,
        OMNI_AGGREGATION_TYPE_COUNT_COLUMN, OMNI_AGGREGATION_TYPE_COUNT_COLUMN,
        OMNI_AGGREGATION_TYPE_COUNT_COLUMN, OMNI_AGGREGATION_TYPE_COUNT_COLUMN,
        OMNI_AGGREGATION_TYPE_COUNT_COLUMN, OMNI_AGGREGATION_TYPE_COUNT_COLUMN };
    TestHashAggSpillWithNullRecords(aggFuncTypes, aggOutputTypes, expectVecorBatch);
}

TEST(HashAggregationWithExprOperatorTest, test_hashagg_null_record_min_spill)
{
    const int32_t expectDataSize = 4;
    int64_t expData0[] = {2, 2, 2, 2};
    int32_t expData1[] = {1, 3, 5, 0};
    int64_t expData2[] = {0, 0, 0, 0};           // c2
    int64_t expData3[] = {0, 0, 0, 0};           // c3
    int64_t expData4[] = {0, 0, 0, 0};           // c4
    bool expData5[] = {true, true, false, true}; // c5
    double expData6[] = {0, 0, 0, 0};            // c6
    std::string expData7[] = {"", "", "", ""};   // c7
    int64_t expData8[] = {0, 0, 0, 0};           // c8
    Decimal128 expData9[] = {0, 0, 0, 0};        // c9

    DataTypes expectTypes(std::vector<DataTypePtr>({ LongType(), IntType(), LongType(), LongType(), LongType(),
        BooleanType(), DoubleType(), VarcharType(10), Decimal64Type(), Decimal128Type(8, 2) }));
    VectorBatch *expectVecorBatch = CreateVectorBatch(expectTypes, expectDataSize, expData0, expData1, expData2,
        expData3, expData4, expData5, expData6, expData7, expData8, expData9);
    expectVecorBatch->Get(0)->SetNull(3);
    expectVecorBatch->Get(1)->SetNull(3);
    for (int i = 0; i < expectDataSize; i++) {
        expectVecorBatch->Get(2)->SetNull(i);
        expectVecorBatch->Get(3)->SetNull(i);
        expectVecorBatch->Get(4)->SetNull(i);
        expectVecorBatch->Get(5)->SetNull(i);
        expectVecorBatch->Get(6)->SetNull(i);
        expectVecorBatch->Get(8)->SetNull(i);
        expectVecorBatch->Get(9)->SetNull(i);
        auto varCharVector =
            reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(expectVecorBatch->Get(7));
        varCharVector->SetNull(i);
    }

    DataTypes aggOutputTypes(std::vector<DataTypePtr>({ LongType(), LongType(), LongType(), BooleanType(), DoubleType(),
        VarcharType(10), Decimal64Type(), Decimal128Type(8, 2) }));
    std::vector<uint32_t> aggFuncTypes = { OMNI_AGGREGATION_TYPE_MIN, OMNI_AGGREGATION_TYPE_MIN,
        OMNI_AGGREGATION_TYPE_MIN, OMNI_AGGREGATION_TYPE_MIN,
        OMNI_AGGREGATION_TYPE_MIN, OMNI_AGGREGATION_TYPE_MIN,
        OMNI_AGGREGATION_TYPE_MIN, OMNI_AGGREGATION_TYPE_MIN };
    TestHashAggSpillWithNullRecords(aggFuncTypes, aggOutputTypes, expectVecorBatch);
}

TEST(HashAggregationWithExprOperatorTest, test_hashagg_null_record_max_spill)
{
    const int32_t expectDataSize = 4;
    int64_t expData0[] = {2, 2, 2, 2};
    int32_t expData1[] = {1, 3, 5, 0};
    int64_t expData2[] = {0, 0, 0, 0};           // c2
    int64_t expData3[] = {0, 0, 0, 0};           // c3
    int64_t expData4[] = {0, 0, 0, 0};           // c4
    bool expData5[] = {true, true, false, true}; // c5
    double expData6[] = {0, 0, 0, 0};            // c6
    std::string expData7[] = {"", "", "", ""};   // c7
    int64_t expData8[] = {0, 0, 0, 0};           // c8
    Decimal128 expData9[] = {0, 0, 0, 0};        // c9

    DataTypes expectTypes(std::vector<DataTypePtr>({ LongType(), IntType(), LongType(), LongType(), LongType(),
        BooleanType(), DoubleType(), VarcharType(10), Decimal64Type(), Decimal128Type(8, 2) }));
    VectorBatch *expectVecorBatch = CreateVectorBatch(expectTypes, expectDataSize, expData0, expData1, expData2,
        expData3, expData4, expData5, expData6, expData7, expData8, expData9);
    expectVecorBatch->Get(0)->SetNull(3);
    expectVecorBatch->Get(1)->SetNull(3);
    for (int i = 0; i < expectDataSize; i++) {
        expectVecorBatch->Get(2)->SetNull(i);
        expectVecorBatch->Get(3)->SetNull(i);
        expectVecorBatch->Get(4)->SetNull(i);
        expectVecorBatch->Get(5)->SetNull(i);
        expectVecorBatch->Get(6)->SetNull(i);
        expectVecorBatch->Get(8)->SetNull(i);
        expectVecorBatch->Get(9)->SetNull(i);
        auto varCharVector =
            reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(expectVecorBatch->Get(7));
        varCharVector->SetNull(i);
    }

    DataTypes aggOutputTypes(std::vector<DataTypePtr>({ LongType(), LongType(), LongType(), BooleanType(), DoubleType(),
        VarcharType(10), Decimal64Type(), Decimal128Type(8, 2) }));
    std::vector<uint32_t> aggFuncTypes = { OMNI_AGGREGATION_TYPE_MAX, OMNI_AGGREGATION_TYPE_MAX,
        OMNI_AGGREGATION_TYPE_MAX, OMNI_AGGREGATION_TYPE_MAX,
        OMNI_AGGREGATION_TYPE_MAX, OMNI_AGGREGATION_TYPE_MAX,
        OMNI_AGGREGATION_TYPE_MAX, OMNI_AGGREGATION_TYPE_MAX };
    TestHashAggSpillWithNullRecords(aggFuncTypes, aggOutputTypes, expectVecorBatch);
}

TEST(HashAggregationWithExprOperatorTest, test_hashagg_null_record_first_spill)
{
    const int32_t expectDataSize = 4;
    int64_t expData0[] = {2, 2, 2, 2};
    int32_t expData1[] = {1, 3, 5, 0};
    int64_t expData2[] = {0, 0, 0, 0};           // c2
    int64_t expData3[] = {0, 0, 0, 0};           // c3
    int64_t expData4[] = {0, 0, 0, 0};           // c4
    bool expData5[] = {true, true, false, true}; // c5
    double expData6[] = {0, 0, 0, 0};            // c6
    std::string expData7[] = {"", "", "", ""};   // c7
    int64_t expData8[] = {0, 0, 0, 0};           // c8
    Decimal128 expData9[] = {0, 0, 0, 0};        // c9

    DataTypes expectTypes(std::vector<DataTypePtr>({ LongType(), IntType(), LongType(), LongType(), LongType(),
        BooleanType(), DoubleType(), VarcharType(10), Decimal64Type(), Decimal128Type(8, 2) }));
    VectorBatch *expectVecorBatch = CreateVectorBatch(expectTypes, expectDataSize, expData0, expData1, expData2,
        expData3, expData4, expData5, expData6, expData7, expData8, expData9);
    expectVecorBatch->Get(0)->SetNull(3);
    expectVecorBatch->Get(1)->SetNull(3);
    for (int i = 0; i < expectDataSize; i++) {
        expectVecorBatch->Get(2)->SetNull(i);
        expectVecorBatch->Get(3)->SetNull(i);
        expectVecorBatch->Get(4)->SetNull(i);
        expectVecorBatch->Get(5)->SetNull(i);
        expectVecorBatch->Get(6)->SetNull(i);
        expectVecorBatch->Get(8)->SetNull(i);
        expectVecorBatch->Get(9)->SetNull(i);
        auto varCharVector =
            reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(expectVecorBatch->Get(7));
        varCharVector->SetNull(i);
    }

    DataTypes aggOutputTypes(std::vector<DataTypePtr>({ LongType(), LongType(), LongType(), BooleanType(), DoubleType(),
        VarcharType(10), Decimal64Type(), Decimal128Type(8, 2) }));
    std::vector<uint32_t> aggFuncTypes = {
        OMNI_AGGREGATION_TYPE_FIRST_IGNORENULL, OMNI_AGGREGATION_TYPE_FIRST_IGNORENULL,
        OMNI_AGGREGATION_TYPE_FIRST_IGNORENULL, OMNI_AGGREGATION_TYPE_FIRST_IGNORENULL,
        OMNI_AGGREGATION_TYPE_FIRST_IGNORENULL, OMNI_AGGREGATION_TYPE_FIRST_IGNORENULL,
        OMNI_AGGREGATION_TYPE_FIRST_IGNORENULL, OMNI_AGGREGATION_TYPE_FIRST_IGNORENULL
    };
    TestHashAggSpillWithNullRecords(aggFuncTypes, aggOutputTypes, expectVecorBatch);
}

TEST(HashAggregationWithExprOperatorTest, test_hashagg_first_null_record_include_null_spill)
{
    const int32_t expectDataSize = 4;
    int64_t expData0[] = {2, 2, 2, 2};
    int32_t expData1[] = {1, 3, 5, 0};
    int64_t expData2[] = {0, 0, 0, 0};           // c2
    int64_t expData3[] = {0, 0, 0, 0};           // c3
    int64_t expData4[] = {0, 0, 0, 0};           // c4
    bool expData5[] = {true, true, false, true}; // c5
    double expData6[] = {0, 0, 0, 0};            // c6
    std::string expData7[] = {"", "", "", ""};   // c7
    int64_t expData8[] = {0, 0, 0, 0};           // c8
    Decimal128 expData9[] = {0, 0, 0, 0};        // c9

    DataTypes expectTypes(std::vector<DataTypePtr>({ LongType(), IntType(), LongType(), LongType(), LongType(),
        BooleanType(), DoubleType(), VarcharType(10), Decimal64Type(), Decimal128Type(8, 2) }));
    VectorBatch *expectVecorBatch = CreateVectorBatch(expectTypes, expectDataSize, expData0, expData1, expData2,
        expData3, expData4, expData5, expData6, expData7, expData8, expData9);
    expectVecorBatch->Get(0)->SetNull(3);
    expectVecorBatch->Get(1)->SetNull(3);
    for (int i = 0; i < expectDataSize; i++) {
        expectVecorBatch->Get(2)->SetNull(i);
        expectVecorBatch->Get(3)->SetNull(i);
        expectVecorBatch->Get(4)->SetNull(i);
        expectVecorBatch->Get(5)->SetNull(i);
        expectVecorBatch->Get(6)->SetNull(i);
        expectVecorBatch->Get(8)->SetNull(i);
        expectVecorBatch->Get(9)->SetNull(i);
        auto varCharVector =
            reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(expectVecorBatch->Get(7));
        varCharVector->SetNull(i);
    }

    DataTypes aggOutputTypes(std::vector<DataTypePtr>({ LongType(), LongType(), LongType(), BooleanType(), DoubleType(),
        VarcharType(10), Decimal64Type(), Decimal128Type(8, 2) }));
    std::vector<uint32_t> aggFuncTypes = {
        OMNI_AGGREGATION_TYPE_FIRST_INCLUDENULL, OMNI_AGGREGATION_TYPE_FIRST_INCLUDENULL,
        OMNI_AGGREGATION_TYPE_FIRST_INCLUDENULL, OMNI_AGGREGATION_TYPE_FIRST_INCLUDENULL,
        OMNI_AGGREGATION_TYPE_FIRST_INCLUDENULL, OMNI_AGGREGATION_TYPE_FIRST_INCLUDENULL,
        OMNI_AGGREGATION_TYPE_FIRST_INCLUDENULL, OMNI_AGGREGATION_TYPE_FIRST_INCLUDENULL
    };
    TestHashAggSpillWithNullRecords(aggFuncTypes, aggOutputTypes, expectVecorBatch);
}

TEST(HashAggregationWithExprOperatorTest, kurtosis)
{
    ConfigUtil::SetSupportContainerVecRule(SupportContainerVecRule::NOT_SUPPORT);
    const int32_t dataSize = 5;

    const int32_t groupByNum = 1;

    int data1[] = {1, 1, 1, 1, 1};
    double data2[] = {1, 10, 100, 10, 1};

    DataTypes sourceTypes(std::vector<DataTypePtr>({IntType(), DoubleType()}));

    DataTypes partialAggOutputTypes(std::vector<DataTypePtr>({DoubleType(),DoubleType(),DoubleType(),DoubleType(),DoubleType()}));
    std::vector<DataTypes> partialAggOutputTypesVec = {partialAggOutputTypes};

    VectorBatch *sourceVecBatch = CreateVectorBatch(sourceTypes, dataSize, data1, data2);

    std::vector<Expr *> partialGroupByKeys = {new FieldExpr(0, IntType())};

    std::vector<Expr *> partialAggKeys = {new FieldExpr(1, DoubleType())};
    std::vector<std::vector<omniruntime::expressions::Expr *>> partialAggAllKeys = {partialAggKeys};

    std::vector<uint32_t> aggFuncTypes = {OMNI_AGGREGATION_TYPE_KURTOSIS};
    std::vector<uint32_t> maskCols = {static_cast<uint32_t>(-1), static_cast<uint32_t>(-1)};

    auto inputRawWrap = std::vector<bool>(aggFuncTypes.size(), true);
    auto outputPartialWrap = std::vector<bool>(aggFuncTypes.size(), true);

    std::vector<omniruntime::expressions::Expr *> aggFilters;
    aggFilters.reserve(2);
    aggFilters.push_back(nullptr);
    aggFilters.push_back(nullptr);

    auto *hashAggWithExprOperatorFactory =
            new HashAggregationWithExprOperatorFactory(partialGroupByKeys, groupByNum, partialAggAllKeys, aggFilters, sourceTypes,
                                                       partialAggOutputTypesVec, aggFuncTypes, maskCols, inputRawWrap,
                                                       outputPartialWrap, OperatorConfig());
    auto *hashAggWithExprOperator =
            dynamic_cast<HashAggregationWithExprOperator *>(CreateTestOperator(hashAggWithExprOperatorFactory));

    hashAggWithExprOperator->AddInput(sourceVecBatch);

    VectorBatch *outputVecBatch = nullptr;
    hashAggWithExprOperator->GetOutput(&outputVecBatch);

    std::vector<Expr *> finalGroupByKeys = {new FieldExpr(0, IntType())};

    std::vector<Expr*> finalAggKeys = {
        new FieldExpr(1, DoubleType()), new FieldExpr(2, DoubleType()), new FieldExpr(3, DoubleType()),
        new FieldExpr(4, DoubleType()), new FieldExpr(5, DoubleType())
    };
    std::vector<std::vector<omniruntime::expressions::Expr *>> finalAggAllKeys = {finalAggKeys};

    DataTypes finalSourceTypes(std::vector<DataTypePtr>({
        IntType(), DoubleType(), DoubleType(), DoubleType(), DoubleType(), DoubleType()
    }));

    auto inputRawFinalWrap = std::vector<bool>(aggFuncTypes.size(), false);
    auto outputFinalWrap = std::vector<bool>(aggFuncTypes.size(), false);

    DataTypes finalAggOutputTypes(std::vector<DataTypePtr>({DoubleType()}));
    std::vector<DataTypes> finalAggOutputTypesVec = {finalAggOutputTypes};

    auto *hashAggWithExprOperatorFactoryFinal =
        new HashAggregationWithExprOperatorFactory(finalGroupByKeys, groupByNum, finalAggAllKeys, aggFilters,
            finalSourceTypes, finalAggOutputTypesVec, aggFuncTypes, maskCols, inputRawFinalWrap, outputFinalWrap,
            OperatorConfig());
    auto *hashAggWithExprOperatorFinal =
        dynamic_cast<HashAggregationWithExprOperator *>(CreateTestOperator(hashAggWithExprOperatorFactoryFinal));

    hashAggWithExprOperatorFinal->AddInput(outputVecBatch);
    VectorBatch *outputVecBatchFinal = nullptr;
    hashAggWithExprOperatorFinal->GetOutput(&outputVecBatchFinal);

    int expData1[] = {1};
    double expData2[] = {0.19432323191698986};
    DataTypes expectTypes(std::vector<DataTypePtr>({IntType(), DoubleType()}));
    VectorBatch *expectVectorBatch = CreateVectorBatch(expectTypes, 1, expData1, expData2);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatchFinal, expectVectorBatch));

    // clear memory
    Expr::DeleteExprs(partialGroupByKeys);
    Expr::DeleteExprs(partialAggAllKeys);
    Expr::DeleteExprs(finalGroupByKeys);
    Expr::DeleteExprs(finalAggAllKeys);
    omniruntime::op::Operator::DeleteOperator(hashAggWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(hashAggWithExprOperatorFinal);
    delete hashAggWithExprOperatorFactory;
    delete hashAggWithExprOperatorFactoryFinal;
    VectorHelper::FreeVecBatch(expectVectorBatch);
    VectorHelper::FreeVecBatch(outputVecBatchFinal);

    ConfigUtil::SetSupportContainerVecRule(SupportContainerVecRule::SUPPORT);
}

TEST(HashAggregationWithExprOperatorTest, skewness)
{
    ConfigUtil::SetSupportContainerVecRule(SupportContainerVecRule::NOT_SUPPORT);
    const int32_t dataSize = 5;

    const int32_t groupByNum = 1;

    int data1[] = {1, 1, 1, 1, 1};
    double data2[] = {1, 10, 100, 10, 1};

    DataTypes sourceTypes(std::vector<DataTypePtr>({IntType(), DoubleType()}));

    DataTypes partialAggOutputTypes(std::vector<DataTypePtr>({DoubleType(),DoubleType(),DoubleType(),DoubleType()}));
    std::vector<DataTypes> partialAggOutputTypesVec = {partialAggOutputTypes};

    VectorBatch *sourceVecBatch = CreateVectorBatch(sourceTypes, dataSize, data1, data2);

    std::vector<Expr *> partialGroupByKeys = {new FieldExpr(0, IntType())};

    std::vector<Expr *> partialAggKeys = {new FieldExpr(1, DoubleType())};
    std::vector<std::vector<omniruntime::expressions::Expr *>> partialAggAllKeys = {partialAggKeys};

    std::vector<uint32_t> aggFuncTypes = {OMNI_AGGREGATION_TYPE_SKEWNESS};
    std::vector<uint32_t> maskCols = {static_cast<uint32_t>(-1), static_cast<uint32_t>(-1)};

    auto inputRawWrap = std::vector<bool>(aggFuncTypes.size(), true);
    auto outputPartialWrap = std::vector<bool>(aggFuncTypes.size(), true);

    std::vector<omniruntime::expressions::Expr *> aggFilters;
    aggFilters.reserve(2);
    aggFilters.push_back(nullptr);
    aggFilters.push_back(nullptr);

    auto *hashAggWithExprOperatorFactory =
            new HashAggregationWithExprOperatorFactory(partialGroupByKeys, groupByNum, partialAggAllKeys, aggFilters, sourceTypes,
                                                       partialAggOutputTypesVec, aggFuncTypes, maskCols, inputRawWrap,
                                                       outputPartialWrap, OperatorConfig());
    auto *hashAggWithExprOperator =
            dynamic_cast<HashAggregationWithExprOperator *>(CreateTestOperator(hashAggWithExprOperatorFactory));

    hashAggWithExprOperator->AddInput(sourceVecBatch);

    VectorBatch *outputVecBatch = nullptr;
    hashAggWithExprOperator->GetOutput(&outputVecBatch);

    std::vector<Expr *> finalGroupByKeys = {new FieldExpr(0, IntType())};

    std::vector<Expr*> finalAggKeys = {
        new FieldExpr(1, DoubleType()), new FieldExpr(2, DoubleType()), new FieldExpr(3, DoubleType()),
        new FieldExpr(4, DoubleType())
    };
    std::vector<std::vector<omniruntime::expressions::Expr *>> finalAggAllKeys = {finalAggKeys};

    DataTypes finalSourceTypes(std::vector<DataTypePtr>({
        IntType(), LongType(), DoubleType(), DoubleType(), DoubleType(), DoubleType()
    }));

    auto inputRawFinalWrap = std::vector<bool>(aggFuncTypes.size(), false);
    auto outputFinalWrap = std::vector<bool>(aggFuncTypes.size(), false);

    // final output col type
    DataTypes finalAggOutputTypes(std::vector<DataTypePtr>({DoubleType()}));
    std::vector<DataTypes> finalAggOutputTypesVec = {finalAggOutputTypes};

    // construct final aggregator
    auto *hashAggWithExprOperatorFactoryFinal =
        new HashAggregationWithExprOperatorFactory(finalGroupByKeys, groupByNum, finalAggAllKeys, aggFilters,
            finalSourceTypes, finalAggOutputTypesVec, aggFuncTypes, maskCols, inputRawFinalWrap, outputFinalWrap,
            OperatorConfig());
    auto *hashAggWithExprOperatorFinal =
        dynamic_cast<HashAggregationWithExprOperator *>(CreateTestOperator(hashAggWithExprOperatorFactoryFinal));

    // input data
    hashAggWithExprOperatorFinal->AddInput(outputVecBatch);
    VectorBatch *outputVecBatchFinal = nullptr;
    hashAggWithExprOperatorFinal->GetOutput(&outputVecBatchFinal);

    int expData1[] = {1};
    double expData2[] = {1.4581249961461293};
    DataTypes expectTypes(std::vector<DataTypePtr>({IntType(), DoubleType()}));
    VectorBatch *expectVectorBatch = CreateVectorBatch(expectTypes, 1, expData1, expData2);

    // compare the result
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatchFinal, expectVectorBatch));

    // clear memory
    Expr::DeleteExprs(partialGroupByKeys);
    Expr::DeleteExprs(partialAggAllKeys);
    Expr::DeleteExprs(finalGroupByKeys);
    Expr::DeleteExprs(finalAggAllKeys);
    omniruntime::op::Operator::DeleteOperator(hashAggWithExprOperator);
    omniruntime::op::Operator::DeleteOperator(hashAggWithExprOperatorFinal);
    delete hashAggWithExprOperatorFactory;
    delete hashAggWithExprOperatorFactoryFinal;
    VectorHelper::FreeVecBatch(expectVectorBatch);
    VectorHelper::FreeVecBatch(outputVecBatchFinal);

    ConfigUtil::SetSupportContainerVecRule(SupportContainerVecRule::SUPPORT);
}
}
