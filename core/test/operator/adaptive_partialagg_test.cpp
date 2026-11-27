/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#include <vector>
#include <thread>
#include <gtest/gtest.h>
#include "operator/aggregation/aggregator/aggregator_util.h"
#include "operator/aggregation/group_aggregation.h"
#include "operator/aggregation/non_group_aggregation.h"
#include "operator/operator.h"
#include "vector/vector_helper.h"
#include "util/config_util.h"
#include "test/util/test_util.h"
#include "type/decimal128.h"

namespace omniruntime {
using namespace omniruntime::vec;
using namespace omniruntime::op;

std::unique_ptr<OperatorFactory> CreateFactory(std::vector<uint32_t> &groupByColIdxVec,
                                               DataTypes &groupByInputTypes,
                                               std::vector<std::vector<uint32_t>> &aggColIdxVec,
                                               std::vector<DataTypes> &aggInputTypes,
                                               std::vector<DataTypes> &outputTypes,
                                               std::vector<uint32_t> &aggFuncVec,
                                               std::vector<uint32_t> &aggMaskVec,
                                               const bool inputRaw,
                                               const bool outputPartial)
{
    auto aggSize = aggFuncVec.size();
    auto inputRawWrap = std::vector<bool>(aggSize, inputRaw);
    auto outputPartialWrap = std::vector<bool>(aggSize, outputPartial);
    auto factory = std::make_unique<HashAggregationOperatorFactory>(groupByColIdxVec, groupByInputTypes, aggColIdxVec,
        aggInputTypes, outputTypes, aggFuncVec, aggMaskVec, inputRawWrap, outputPartialWrap, false);

    if (factory == nullptr) {
        return nullptr;
    }

    factory->Init();
    return factory;
}

TEST(AdaptivePartialAggregationTest, verify_spark_short)
{
    ConfigUtil::SetSupportContainerVecRule(SupportContainerVecRule::NOT_SUPPORT);
    const int rowSize = 10;
    std::vector<DataTypePtr> groupByInputTypePtr = { IntType() };
    std::vector<DataTypePtr> aggInputTypePtr = { ShortType(), ShortType(), ShortType(), ShortType(), ShortType() };
    std::vector<DataTypePtr> sourceTypePtr = { IntType(), ShortType(), ShortType(), ShortType(), ShortType(),
                                               ShortType() };
    DataTypes groupByInputTypes(groupByInputTypePtr);
    std::vector<DataTypes> aggInputTypes = AggregatorUtil::WrapWithVector(DataTypes(aggInputTypePtr));
    DataTypes sourceTypes = DataTypes(sourceTypePtr);

    std::vector<DataTypes> outputTypes = { DataTypes({LongType()}),
                                           DataTypes({DoubleType(), LongType()}),
                                           DataTypes({LongType()}),
                                           DataTypes({LongType()}),
                                           DataTypes({ShortType()}),
                                           DataTypes({ShortType()}) };

    int32_t group1[rowSize] = {1, 2, 3, 4, 5, 1, 2, 3, 4, 5};
    short agg1[rowSize] = {1, 2, 3, 4, 5, 1, 2, 3, 4, 5};
    VectorBatch *input = TestUtil::CreateVectorBatch(sourceTypes, rowSize, group1, agg1, agg1, agg1, agg1, agg1);

    // First stage (partial)
    std::vector<uint32_t> groupByIdxCols({ 0 });
    std::vector<std::vector<uint32_t>> aggColIdxVec({{1}, {2}, {3}, {4}, {5}});
    std::vector<uint32_t> aggMaskVec = {static_cast<unsigned int>(-1),
                                        static_cast<unsigned int>(-1),
                                        static_cast<unsigned int>(-1),
                                        static_cast<unsigned int>(-1),
                                        static_cast<unsigned int>(-1),
                                        static_cast<unsigned int>(-1)};
    std::vector<uint32_t> aggFuncTypes = { OMNI_AGGREGATION_TYPE_SUM,
                                           OMNI_AGGREGATION_TYPE_AVG,
                                           OMNI_AGGREGATION_TYPE_COUNT_COLUMN,
                                           OMNI_AGGREGATION_TYPE_COUNT_ALL,
                                           OMNI_AGGREGATION_TYPE_MIN,
                                           OMNI_AGGREGATION_TYPE_MAX };

    auto aggPartialFactory = CreateFactory(groupByIdxCols, groupByInputTypes, aggColIdxVec,
                                           aggInputTypes, outputTypes, aggFuncTypes, aggMaskVec,
                                           true, true);

    // operator (partial)
    auto aggPartial = aggPartialFactory->CreateOperator(nullptr);
    aggPartial->Init();

    VectorBatch *outputVecBatch = aggPartial->AlignSchema(input);

    EXPECT_EQ(outputVecBatch->GetRowCount(), rowSize);
    EXPECT_EQ(outputVecBatch->GetVectorCount(), 8);

    op::Operator::DeleteOperator(aggPartial);


    std::vector<std::vector<uint32_t>> finalAggColIdxVec({{1}, {2, 3}, {4}, {5}, {6}, {7}});
    std::vector<DataTypes> finalAggInputTypes = outputTypes;
    std::vector<DataTypes> finalOutputTypes = { DataTypes({LongType()}),
                                                DataTypes({DoubleType()}),
                                                DataTypes({LongType()}),
                                                DataTypes({LongType()}),
                                                DataTypes({ShortType()}),
                                                DataTypes({ShortType()}) };
    // Second stage (final)
    auto aggFinalFactory = CreateFactory(groupByIdxCols, groupByInputTypes, finalAggColIdxVec,
                                         finalAggInputTypes, finalOutputTypes, aggFuncTypes, aggMaskVec,
                                         false, false);

    auto aggFinal = aggFinalFactory->CreateOperator(nullptr);
    aggFinal->Init();

    aggFinal->AddInput(outputVecBatch);

    VectorBatch *outputVecBatch1 = nullptr;
    aggFinal->GetOutput(&outputVecBatch1);
    op::Operator::DeleteOperator(aggFinal);

    // construct the output data
    DataTypes expectTypes({ IntType(), LongType(), DoubleType(), LongType(), LongType(), ShortType(), ShortType() });
    int32_t expectData1[5] = {1, 2, 3, 4, 5}; // group
    int64_t expectData2[5] = {2, 4, 6, 8, 10}; // sum
    double expectData3[5] = {1.0, 2.0, 3.0, 4.0, 5.0}; // avg
    int64_t expectData4[5] = {2, 2, 2, 2, 2}; // count
    int64_t expectData5[5] = {2, 2, 2, 2, 2}; // count
    short expectData6[5] = {1, 2, 3, 4, 5}; // min
    short expectData7[5] = {1, 2, 3, 4, 5}; // max
    VectorBatch *expectVecBatch = TestUtil::CreateVectorBatch(expectTypes, 5, expectData1, expectData2, expectData3,
          expectData4, expectData5, expectData6, expectData7);

    EXPECT_TRUE(TestUtil::VecBatchMatchIgnoreOrder(outputVecBatch1, expectVecBatch));

    VectorHelper::FreeVecBatch(expectVecBatch);
    VectorHelper::FreeVecBatch(outputVecBatch1);
}

TEST(AdaptivePartialAggregationTest, verify_spark_int)
{
    ConfigUtil::SetSupportContainerVecRule(SupportContainerVecRule::NOT_SUPPORT);
    const int rowSize = 10;
    std::vector<DataTypePtr> groupByInputTypePtr = { IntType() };
    std::vector<DataTypePtr> aggInputTypePtr = { IntType(), IntType(), IntType(), IntType(), IntType() };
    std::vector<DataTypePtr> sourceTypePtr = { IntType(), IntType(), IntType(), IntType(), IntType(), IntType()};
    DataTypes groupByInputTypes(groupByInputTypePtr);
    std::vector<DataTypes> aggInputTypes = AggregatorUtil::WrapWithVector(DataTypes(aggInputTypePtr));
    DataTypes sourceTypes = DataTypes(sourceTypePtr);

    std::vector<DataTypes> outputTypes = { DataTypes({LongType()}),
                                                DataTypes({DoubleType(), LongType()}),
                                                DataTypes({LongType()}),
                                                DataTypes({LongType()}),
                                                DataTypes({IntType()}),
                                                DataTypes({IntType()}) };

    int32_t group1[rowSize] = {1, 2, 3, 4, 5, 1, 2, 3, 4, 5};
    int32_t agg1[rowSize] = {1, 2, 3, 4, 5, 1, 2, 3, 4, 5};
    VectorBatch *input = TestUtil::CreateVectorBatch(sourceTypes, rowSize, group1, agg1, agg1, agg1, agg1, agg1);

    // First stage (partial)
    std::vector<uint32_t> groupByIdxCols({ 0 });
    std::vector<std::vector<uint32_t>> aggColIdxVec({{1}, {2}, {3}, {4}, {5}});
    std::vector<uint32_t> aggMaskVec = {static_cast<unsigned int>(-1),
                                        static_cast<unsigned int>(-1),
                                        static_cast<unsigned int>(-1),
                                        static_cast<unsigned int>(-1),
                                        static_cast<unsigned int>(-1),
                                        static_cast<unsigned int>(-1)};
    std::vector<uint32_t> aggFuncTypes = { OMNI_AGGREGATION_TYPE_SUM,
                                           OMNI_AGGREGATION_TYPE_AVG,
                                           OMNI_AGGREGATION_TYPE_COUNT_COLUMN,
                                           OMNI_AGGREGATION_TYPE_COUNT_ALL,
                                           OMNI_AGGREGATION_TYPE_MIN,
                                           OMNI_AGGREGATION_TYPE_MAX };

    auto aggPartialFactory = CreateFactory(groupByIdxCols, groupByInputTypes, aggColIdxVec,
                                           aggInputTypes, outputTypes, aggFuncTypes, aggMaskVec,
                                           true, true);

    // operator (partial)
    auto aggPartial = aggPartialFactory->CreateOperator(nullptr);
    aggPartial->Init();

    VectorBatch *outputVecBatch = aggPartial->AlignSchema(input);

    EXPECT_EQ(outputVecBatch->GetRowCount(), rowSize);
    EXPECT_EQ(outputVecBatch->GetVectorCount(), 8);

    op::Operator::DeleteOperator(aggPartial);


    std::vector<std::vector<uint32_t>> finalAggColIdxVec({{1}, {2, 3}, {4}, {5}, {6}, {7}});
    std::vector<DataTypes> finalAggInputTypes = outputTypes;
    std::vector<DataTypes> finalOutputTypes = { DataTypes({LongType()}),
                                                DataTypes({DoubleType()}),
                                                DataTypes({LongType()}),
                                                DataTypes({LongType()}),
                                                DataTypes({IntType()}),
                                                DataTypes({IntType()}) };
    // Second stage (final)
    auto aggFinalFactory = CreateFactory(groupByIdxCols, groupByInputTypes, finalAggColIdxVec,
                                         finalAggInputTypes, finalOutputTypes, aggFuncTypes, aggMaskVec,
                                         false, false);

    auto aggFinal = aggFinalFactory->CreateOperator(nullptr);
    aggFinal->Init();

    aggFinal->AddInput(outputVecBatch);

    VectorBatch *outputVecBatch1 = nullptr;
    aggFinal->GetOutput(&outputVecBatch1);
    op::Operator::DeleteOperator(aggFinal);

    // construct the output data
    DataTypes expectTypes({ IntType(), LongType(), DoubleType(), LongType(), LongType(), IntType(), IntType() });
    int32_t expectData1[5] = {1, 2, 3, 4, 5}; // group
    int64_t expectData2[5] = {2, 4, 6, 8, 10}; // sum
    double expectData3[5] = {1.0, 2.0, 3.0, 4.0, 5.0}; // avg
    int64_t expectData4[5] = {2, 2, 2, 2, 2}; // count
    int64_t expectData5[5] = {2, 2, 2, 2, 2}; // count
    int32_t expectData6[5] = {1, 2, 3, 4, 5}; // min
    int32_t expectData7[5] = {1, 2, 3, 4, 5}; // max
    VectorBatch *expectVecBatch = TestUtil::CreateVectorBatch(expectTypes, 5, expectData1, expectData2,
          expectData3, expectData4, expectData5, expectData6, expectData7);

    EXPECT_TRUE(TestUtil::VecBatchMatchIgnoreOrder(outputVecBatch1, expectVecBatch));

    VectorHelper::FreeVecBatch(expectVecBatch);
    VectorHelper::FreeVecBatch(outputVecBatch1);
}

TEST(AdaptivePartialAggregationTest, verify_spark_long)
{
    ConfigUtil::SetSupportContainerVecRule(SupportContainerVecRule::NOT_SUPPORT);
    const int rowSize = 10;
    std::vector<DataTypePtr> groupByInputTypePtr = { IntType() };
    std::vector<DataTypePtr> aggInputTypePtr = { LongType(), LongType(), LongType(), LongType(), LongType() };
    std::vector<DataTypePtr> sourceTypePtr = { IntType(), LongType(), LongType(), LongType(), LongType(), LongType() };
    DataTypes groupByInputTypes(groupByInputTypePtr);
    std::vector<DataTypes> aggInputTypes = AggregatorUtil::WrapWithVector(DataTypes(aggInputTypePtr));
    DataTypes sourceTypes = DataTypes(sourceTypePtr);

    std::vector<DataTypes> outputTypes = { DataTypes({LongType()}),
                                           DataTypes({DoubleType(), LongType()}),
                                           DataTypes({LongType()}),
                                           DataTypes({LongType()}),
                                           DataTypes({LongType()}),
                                           DataTypes({LongType()}) };

    int32_t group1[rowSize] = {1, 2, 3, 4, 5, 1, 2, 3, 4, 5};
    int64_t agg1[rowSize] = {1, 2, 3, 4, 5, 1, 2, 3, 4, 5};
    VectorBatch *input = TestUtil::CreateVectorBatch(sourceTypes, rowSize, group1, agg1, agg1, agg1, agg1, agg1);

    // First stage (partial)
    std::vector<uint32_t> groupByIdxCols({ 0 });
    std::vector<std::vector<uint32_t>> aggColIdxVec({{1}, {2}, {3}, {4}, {5}});
    std::vector<uint32_t> aggMaskVec = {static_cast<unsigned int>(-1),
                                        static_cast<unsigned int>(-1),
                                        static_cast<unsigned int>(-1),
                                        static_cast<unsigned int>(-1),
                                        static_cast<unsigned int>(-1),
                                        static_cast<unsigned int>(-1)};
    std::vector<uint32_t> aggFuncTypes = { OMNI_AGGREGATION_TYPE_SUM,
                                           OMNI_AGGREGATION_TYPE_AVG,
                                           OMNI_AGGREGATION_TYPE_COUNT_COLUMN,
                                           OMNI_AGGREGATION_TYPE_COUNT_ALL,
                                           OMNI_AGGREGATION_TYPE_MIN,
                                           OMNI_AGGREGATION_TYPE_MAX };

    auto aggPartialFactory = CreateFactory(groupByIdxCols, groupByInputTypes, aggColIdxVec,
                                           aggInputTypes, outputTypes, aggFuncTypes, aggMaskVec,
                                           true, true);

    // operator (partial)
    auto aggPartial = aggPartialFactory->CreateOperator(nullptr);
    aggPartial->Init();

    VectorBatch *outputVecBatch = aggPartial->AlignSchema(input);

    EXPECT_EQ(outputVecBatch->GetRowCount(), rowSize);
    EXPECT_EQ(outputVecBatch->GetVectorCount(), 8);

    op::Operator::DeleteOperator(aggPartial);


    std::vector<std::vector<uint32_t>> finalAggColIdxVec({{1}, {2, 3}, {4}, {5}, {6}, {7}});
    std::vector<DataTypes> finalAggInputTypes = outputTypes;
    std::vector<DataTypes> finalOutputTypes = { DataTypes({LongType()}),
                                                DataTypes({DoubleType()}),
                                                DataTypes({LongType()}),
                                                DataTypes({LongType()}),
                                                DataTypes({LongType()}),
                                                DataTypes({LongType()}) };
    // Second stage (final)
    auto aggFinalFactory = CreateFactory(groupByIdxCols, groupByInputTypes, finalAggColIdxVec,
                                         finalAggInputTypes, finalOutputTypes, aggFuncTypes, aggMaskVec,
                                         false, false);

    auto aggFinal = aggFinalFactory->CreateOperator(nullptr);
    aggFinal->Init();

    aggFinal->AddInput(outputVecBatch);

    VectorBatch *outputVecBatch1 = nullptr;
    aggFinal->GetOutput(&outputVecBatch1);
    op::Operator::DeleteOperator(aggFinal);

    // construct the output data
    DataTypes expectTypes({ IntType(), LongType(), DoubleType(), LongType(), LongType(), LongType(), LongType() });
    int32_t expectData1[5] = {1, 2, 3, 4, 5}; // group
    int64_t expectData2[5] = {2, 4, 6, 8, 10}; // sum
    double expectData3[5] = {1.0, 2.0, 3.0, 4.0, 5.0}; // avg
    int64_t expectData4[5] = {2, 2, 2, 2, 2}; // count
    int64_t expectData5[5] = {2, 2, 2, 2, 2}; // count
    int64_t expectData6[5] = {1, 2, 3, 4, 5}; // min
    int64_t expectData7[5] = {1, 2, 3, 4, 5}; // max
    VectorBatch *expectVecBatch = TestUtil::CreateVectorBatch(expectTypes, 5, expectData1, expectData2,
        expectData3, expectData4, expectData5, expectData6, expectData7);

    EXPECT_TRUE(TestUtil::VecBatchMatchIgnoreOrder(outputVecBatch1, expectVecBatch));

    VectorHelper::FreeVecBatch(expectVecBatch);
    VectorHelper::FreeVecBatch(outputVecBatch1);
}

TEST(AdaptivePartialAggregationTest, verify_spark_double)
{
    ConfigUtil::SetSupportContainerVecRule(SupportContainerVecRule::NOT_SUPPORT);
    const int rowSize = 10;
    std::vector<DataTypePtr> groupByInputTypePtr = { IntType() };
    std::vector<DataTypePtr> aggInputTypePtr = { DoubleType(), DoubleType(), DoubleType(), DoubleType(), DoubleType()};
    std::vector<DataTypePtr> sourceTypePtr = { IntType(), DoubleType(), DoubleType(), DoubleType(), DoubleType(),
                                               DoubleType() };
    DataTypes groupByInputTypes(groupByInputTypePtr);
    std::vector<DataTypes> aggInputTypes = AggregatorUtil::WrapWithVector(DataTypes(aggInputTypePtr));
    DataTypes sourceTypes = DataTypes(sourceTypePtr);

    std::vector<DataTypes> outputTypes = { DataTypes({DoubleType()}),
                                           DataTypes({DoubleType(), LongType()}),
                                           DataTypes({LongType()}),
                                           DataTypes({LongType()}),
                                           DataTypes({DoubleType()}),
                                           DataTypes({DoubleType()}) };

    int32_t group1[rowSize] = {1, 2, 3, 4, 5, 1, 2, 3, 4, 5};
    double agg1[rowSize] = {1.01, 2.02, 3.03, 4.04, 5.05, 1.01, 2.02, 3.03, 4.04, 5.05};
    VectorBatch *input = TestUtil::CreateVectorBatch(sourceTypes, rowSize, group1, agg1, agg1, agg1, agg1, agg1);

    // First stage (partial)
    std::vector<uint32_t> groupByIdxCols({ 0 });
    std::vector<std::vector<uint32_t>> aggColIdxVec({{1}, {2}, {3}, {4}, {5}});
    std::vector<uint32_t> aggMaskVec = {static_cast<unsigned int>(-1),
                                        static_cast<unsigned int>(-1),
                                        static_cast<unsigned int>(-1),
                                        static_cast<unsigned int>(-1),
                                        static_cast<unsigned int>(-1),
                                        static_cast<unsigned int>(-1)};
    std::vector<uint32_t> aggFuncTypes = { OMNI_AGGREGATION_TYPE_SUM,
                                           OMNI_AGGREGATION_TYPE_AVG,
                                           OMNI_AGGREGATION_TYPE_COUNT_COLUMN,
                                           OMNI_AGGREGATION_TYPE_COUNT_ALL,
                                           OMNI_AGGREGATION_TYPE_MIN,
                                           OMNI_AGGREGATION_TYPE_MAX };

    auto aggPartialFactory = CreateFactory(groupByIdxCols, groupByInputTypes, aggColIdxVec,
                                           aggInputTypes, outputTypes, aggFuncTypes, aggMaskVec,
                                           true, true);

    // operator (partial)
    auto aggPartial = aggPartialFactory->CreateOperator(nullptr);
    aggPartial->Init();

    VectorBatch *outputVecBatch = aggPartial->AlignSchema(input);

    EXPECT_EQ(outputVecBatch->GetRowCount(), rowSize);
    EXPECT_EQ(outputVecBatch->GetVectorCount(), 8);

    op::Operator::DeleteOperator(aggPartial);


    std::vector<std::vector<uint32_t>> finalAggColIdxVec({{1}, {2, 3}, {4}, {5}, {6}, {7}});
    std::vector<DataTypes> finalAggInputTypes = outputTypes;
    std::vector<DataTypes> finalOutputTypes = { DataTypes({DoubleType()}),
                                                DataTypes({DoubleType()}),
                                                DataTypes({LongType()}),
                                                DataTypes({LongType()}),
                                                DataTypes({DoubleType()}),
                                                DataTypes({DoubleType()}) };
    // Second stage (final)
    auto aggFinalFactory = CreateFactory(groupByIdxCols, groupByInputTypes, finalAggColIdxVec,
                                         finalAggInputTypes, finalOutputTypes, aggFuncTypes, aggMaskVec,
                                         false, false);

    auto aggFinal = aggFinalFactory->CreateOperator(nullptr);
    aggFinal->Init();

    aggFinal->AddInput(outputVecBatch);

    VectorBatch *outputVecBatch1 = nullptr;
    aggFinal->GetOutput(&outputVecBatch1);
    op::Operator::DeleteOperator(aggFinal);

    // construct the output data
    DataTypes expectTypes({ IntType(), DoubleType(), DoubleType(), LongType(), LongType(), DoubleType(),
                            DoubleType() });
    int32_t expectData1[5] = {1, 2, 3, 4, 5}; // group
    double expectData2[5] = {2.02, 4.04, 6.06, 8.08, 10.10}; // sum
    double expectData3[5] = {1.01, 2.02, 3.03, 4.04, 5.05}; // avg
    int64_t expectData4[5] = {2, 2, 2, 2, 2}; // count
    int64_t expectData5[5] = {2, 2, 2, 2, 2}; // count
    double expectData6[5] = {1.01, 2.02, 3.03, 4.04, 5.05}; // min
    double expectData7[5] = {1.01, 2.02, 3.03, 4.04, 5.05}; // max
    VectorBatch *expectVecBatch = TestUtil::CreateVectorBatch(expectTypes, 5, expectData1, expectData2,
          expectData3, expectData4, expectData5, expectData6, expectData7);

    EXPECT_TRUE(TestUtil::VecBatchMatchIgnoreOrder(outputVecBatch1, expectVecBatch));

    VectorHelper::FreeVecBatch(expectVecBatch);
    VectorHelper::FreeVecBatch(outputVecBatch1);
}

TEST(AdaptivePartialAggregationTest, verify_spark_decimal64)
{
    ConfigUtil::SetSupportContainerVecRule(SupportContainerVecRule::NOT_SUPPORT);
    const int rowSize = 10;
    std::vector<DataTypePtr> groupByInputTypePtr = { IntType() };
    std::vector<DataTypePtr> aggInputTypePtr = { Decimal64Type(10, 2),
                                                 Decimal64Type(10, 2),
                                                 Decimal64Type(10, 2),
                                                 Decimal64Type(10, 2),
                                                 Decimal64Type(10, 2) };
    std::vector<DataTypePtr> sourceTypePtr = { IntType(),
                                               Decimal64Type(10, 2),
                                               Decimal64Type(10, 2),
                                               Decimal64Type(10, 2),
                                               Decimal64Type(10, 2),
                                               Decimal64Type(10, 2) };
    DataTypes groupByInputTypes(groupByInputTypePtr);
    std::vector<DataTypes> aggInputTypes = AggregatorUtil::WrapWithVector(DataTypes(aggInputTypePtr));
    DataTypes sourceTypes = DataTypes(sourceTypePtr);

    std::vector<DataTypes> outputTypes = { DataTypes({ Decimal128Type(20, 2), BooleanType() }),
                                           DataTypes({ Decimal128Type(20, 2), LongType() }),
                                           DataTypes({ LongType() }),
                                           DataTypes({ LongType() }),
                                           DataTypes({ Decimal64Type(10, 2) }),
                                           DataTypes({ Decimal64Type(10, 2) }) };

    int32_t group1[rowSize] = {1, 2, 3, 4, 5, 1, 2, 3, 4, 5};
    int64_t agg1[rowSize] = {101, 202, 303, 404, 505, 101, 202, 303, 404, 505};
    VectorBatch *input = TestUtil::CreateVectorBatch(sourceTypes, rowSize, group1, agg1, agg1, agg1, agg1, agg1);

    // First stage (partial)
    std::vector<uint32_t> groupByIdxCols({ 0 });
    std::vector<std::vector<uint32_t>> aggColIdxVec({{1}, {2}, {3}, {4}, {5}});
    std::vector<uint32_t> aggMaskVec = {static_cast<unsigned int>(-1),
                                        static_cast<unsigned int>(-1),
                                        static_cast<unsigned int>(-1),
                                        static_cast<unsigned int>(-1),
                                        static_cast<unsigned int>(-1),
                                        static_cast<unsigned int>(-1)};
    std::vector<uint32_t> aggFuncTypes = { OMNI_AGGREGATION_TYPE_SUM,
                                           OMNI_AGGREGATION_TYPE_AVG,
                                           OMNI_AGGREGATION_TYPE_COUNT_COLUMN,
                                           OMNI_AGGREGATION_TYPE_COUNT_ALL,
                                           OMNI_AGGREGATION_TYPE_MIN,
                                           OMNI_AGGREGATION_TYPE_MAX };

    auto aggPartialFactory = CreateFactory(groupByIdxCols, groupByInputTypes, aggColIdxVec,
                                           aggInputTypes, outputTypes, aggFuncTypes, aggMaskVec,
                                           true, true);

    // operator (partial)
    auto aggPartial = aggPartialFactory->CreateOperator(nullptr);
    aggPartial->Init();

    VectorBatch *outputVecBatch = aggPartial->AlignSchema(input);

    EXPECT_EQ(outputVecBatch->GetRowCount(), rowSize);
    EXPECT_EQ(outputVecBatch->GetVectorCount(), 9);

    op::Operator::DeleteOperator(aggPartial);


    std::vector<std::vector<uint32_t>> finalAggColIdxVec({{1, 2}, {3, 4}, {5}, {6}, {7}, {8}});
    std::vector<DataTypes> finalAggInputTypes = outputTypes;
    std::vector<DataTypes> finalOutputTypes = { DataTypes({ Decimal128Type(20, 2) }),
                                                DataTypes({ Decimal64Type(10, 2) }),
                                                DataTypes({ LongType() }),
                                                DataTypes({ LongType() }),
                                                DataTypes({ Decimal64Type(10, 2) }),
                                                DataTypes({ Decimal64Type(10, 2) }) };
    // Second stage (final)
    auto aggFinalFactory = CreateFactory(groupByIdxCols, groupByInputTypes, finalAggColIdxVec,
                                         finalAggInputTypes, finalOutputTypes, aggFuncTypes, aggMaskVec,
                                         false, false);

    auto aggFinal = aggFinalFactory->CreateOperator(nullptr);
    aggFinal->Init();

    aggFinal->AddInput(outputVecBatch);

    VectorBatch *outputVecBatch1 = nullptr;
    aggFinal->GetOutput(&outputVecBatch1);
    op::Operator::DeleteOperator(aggFinal);

    // construct the output data
    DataTypes expectTypes({ IntType(),
                            Decimal128Type(20, 2),
                            Decimal64Type(10, 2),
                            LongType(),
                            LongType(),
                            Decimal64Type(10, 2),
                            Decimal64Type(10, 2) });
    int32_t expectData1[5] = {1, 2, 3, 4, 5}; // group
    Decimal128 expectData2[5] = {202, 404, 606, 808, 1010}; // sum
    int64_t expectData3[5] = {101, 202, 303, 404, 505}; // avg
    int64_t expectData4[5] = {2, 2, 2, 2, 2}; // count
    int64_t expectData5[5] = {2, 2, 2, 2, 2}; // count
    int64_t expectData6[5] = {101, 202, 303, 404, 505}; // min
    int64_t expectData7[5] = {101, 202, 303, 404, 505}; // max
    VectorBatch *expectVecBatch = TestUtil::CreateVectorBatch(expectTypes, 5, expectData1, expectData2,
          expectData3, expectData4, expectData5, expectData6, expectData7);

    EXPECT_TRUE(TestUtil::VecBatchMatchIgnoreOrder(outputVecBatch1, expectVecBatch));

    VectorHelper::FreeVecBatch(expectVecBatch);
    VectorHelper::FreeVecBatch(outputVecBatch1);
}

TEST(AdaptivePartialAggregationTest, verify_spark_decimal128)
{
    ConfigUtil::SetSupportContainerVecRule(SupportContainerVecRule::NOT_SUPPORT);
    const int rowSize = 10;
    std::vector<DataTypePtr> groupByInputTypePtr = { IntType() };
    std::vector<DataTypePtr> aggInputTypePtr = { Decimal64Type(10, 2),
                                                 Decimal64Type(10, 2),
                                                 Decimal64Type(10, 2),
                                                 Decimal64Type(10, 2),
                                                 Decimal64Type(10, 2) };
    std::vector<DataTypePtr> sourceTypePtr = { IntType(),
                                               Decimal64Type(10, 2),
                                               Decimal64Type(10, 2),
                                               Decimal64Type(10, 2),
                                               Decimal64Type(10, 2),
                                               Decimal64Type(10, 2) };
    DataTypes groupByInputTypes(groupByInputTypePtr);
    std::vector<DataTypes> aggInputTypes = AggregatorUtil::WrapWithVector(DataTypes(aggInputTypePtr));
    DataTypes sourceTypes = DataTypes(sourceTypePtr);

    std::vector<DataTypes> outputTypes = { DataTypes({Decimal128Type(20, 2), BooleanType()}),
                                           DataTypes({Decimal128Type(20, 2), LongType()}),
                                           DataTypes({LongType()}),
                                           DataTypes({LongType()}),
                                           DataTypes({Decimal64Type(10, 2)}),
                                           DataTypes({Decimal64Type(10, 2)}) };

    int32_t group1[rowSize] = {1, 2, 3, 4, 5, 1, 2, 3, 4, 5};
    int64_t agg1[rowSize] = {101, 202, 303, 404, 505, 101, 202, 303, 404, 505};
    VectorBatch *input = TestUtil::CreateVectorBatch(sourceTypes, rowSize, group1, agg1, agg1, agg1, agg1, agg1);

    // First stage (partial)
    std::vector<uint32_t> groupByIdxCols({ 0 });
    std::vector<std::vector<uint32_t>> aggColIdxVec({{1}, {2}, {3}, {4}, {5}});
    std::vector<uint32_t> aggMaskVec = {static_cast<unsigned int>(-1),
                                        static_cast<unsigned int>(-1),
                                        static_cast<unsigned int>(-1),
                                        static_cast<unsigned int>(-1),
                                        static_cast<unsigned int>(-1),
                                        static_cast<unsigned int>(-1)};
    std::vector<uint32_t> aggFuncTypes = { OMNI_AGGREGATION_TYPE_SUM,
                                           OMNI_AGGREGATION_TYPE_AVG,
                                           OMNI_AGGREGATION_TYPE_COUNT_COLUMN,
                                           OMNI_AGGREGATION_TYPE_COUNT_ALL,
                                           OMNI_AGGREGATION_TYPE_MIN,
                                           OMNI_AGGREGATION_TYPE_MAX };

    auto aggPartialFactory = CreateFactory(groupByIdxCols, groupByInputTypes, aggColIdxVec,
                                           aggInputTypes, outputTypes, aggFuncTypes, aggMaskVec,
                                           true, true);

    // operator (partial)
    auto aggPartial = aggPartialFactory->CreateOperator(nullptr);
    aggPartial->Init();

    VectorBatch *outputVecBatch = aggPartial->AlignSchema(input);

    EXPECT_EQ(outputVecBatch->GetRowCount(), rowSize);
    EXPECT_EQ(outputVecBatch->GetVectorCount(), 9);

    op::Operator::DeleteOperator(aggPartial);


    std::vector<std::vector<uint32_t>> finalAggColIdxVec({{1, 2}, {3, 4}, {5}, {6}, {7}, {8}});
    std::vector<DataTypes> finalAggInputTypes = outputTypes;
    std::vector<DataTypes> finalOutputTypes = { DataTypes({Decimal128Type(20, 2)}),
                                                DataTypes({Decimal128Type(20, 2)}),
                                                DataTypes({LongType()}),
                                                DataTypes({LongType()}),
                                                DataTypes({Decimal128Type(20, 2)}),
                                                DataTypes({Decimal128Type(20, 2)}) };
    // Second stage (final)
    auto aggFinalFactory = CreateFactory(groupByIdxCols, groupByInputTypes, finalAggColIdxVec,
                                         finalAggInputTypes, finalOutputTypes, aggFuncTypes, aggMaskVec,
                                         false, false);

    auto aggFinal = aggFinalFactory->CreateOperator(nullptr);
    aggFinal->Init();

    aggFinal->AddInput(outputVecBatch);

    VectorBatch *outputVecBatch1 = nullptr;
    aggFinal->GetOutput(&outputVecBatch1);
    op::Operator::DeleteOperator(aggFinal);

    // construct the output data
    DataTypes expectTypes({ IntType(),
                            Decimal128Type(20, 2),
                            Decimal128Type(20, 2),
                            LongType(),
                            LongType(),
                            Decimal128Type(20, 2),
                            Decimal128Type(20, 2) });
    int32_t expectData1[5] = {1, 2, 3, 4, 5}; // group
    Decimal128 expectData2[5] = {202, 404, 606, 808, 1010}; // sum
    Decimal128 expectData3[5] = {101, 202, 303, 404, 505}; // avg
    int64_t expectData4[5] = {2, 2, 2, 2, 2}; // count
    int64_t expectData5[5] = {2, 2, 2, 2, 2}; // count
    Decimal128 expectData6[5] = {101, 202, 303, 404, 505}; // min
    Decimal128 expectData7[5] = {101, 202, 303, 404, 505}; // max
    VectorBatch *expectVecBatch = TestUtil::CreateVectorBatch(expectTypes, 5, expectData1, expectData2,
          expectData3, expectData4, expectData5, expectData6, expectData7);

    EXPECT_TRUE(TestUtil::VecBatchMatchIgnoreOrder(outputVecBatch1, expectVecBatch));

    VectorHelper::FreeVecBatch(expectVecBatch);
    VectorHelper::FreeVecBatch(outputVecBatch1);
}
}