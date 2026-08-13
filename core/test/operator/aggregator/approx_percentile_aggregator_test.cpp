/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Unit tests for ApproxPercentile aggregation (KLL sketch, two-phase).
 * Reference: approx_count_distinct_aggregator_test.cpp (two-column pattern: approx_count_distinct_with_max_standard_error)
 */
#include <memory>
#include <vector>
#include <gtest/gtest.h>
#include "operator/aggregation/aggregator/aggregator_factory.h"
#include "operator/aggregation/aggregator/aggregator_util.h"
#include "operator/aggregation/group_aggregation.h"
#include "operator/aggregation/non_group_aggregation.h"
#include "vector/vector_helper.h"
#include "vector/array_vector.h"
#include "type/data_type.h"
#include "type/data_types.h"
#include "util/test_agg_util.h"
#include "util/test_util.h"

namespace omniruntime {
using namespace omniruntime::vec;
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace TestUtil;

// Build partial/final factory for 2–4 col approx_percentile (value[, weight], percentile[, accuracy]). Final phase has single VARBINARY column.
static void BuildPartialFinalFactories(const std::vector<DataTypePtr>& sourceTypeVec,
    const std::vector<DataTypePtr>& partialOutTypeVec, const std::vector<DataTypePtr>& finalOutTypeVec,
    std::unique_ptr<AggregationOperatorFactory>* outPartial, std::unique_ptr<AggregationOperatorFactory>* outFinal)
{
    DataTypes sourceTypes(sourceTypeVec);
    std::vector<uint32_t> aggCols;
    for (size_t i = 0; i < sourceTypeVec.size(); ++i)
        aggCols.push_back(static_cast<uint32_t>(i));
    std::vector<std::vector<uint32_t>> aggsInputColsVector = { aggCols };
    std::vector<uint32_t> aggFuncTypes = { OMNI_AGGREGATION_TYPE_APPROX_PERCENTILE };
    std::vector<uint32_t> maskCols = { static_cast<uint32_t>(-1) };
    std::vector<DataTypes> aggsOutputTypesPartial = { DataTypes(partialOutTypeVec) };
    std::vector<bool> inputRaws = { true };
    std::vector<bool> outputPartials = { true };
    *outPartial = std::make_unique<AggregationOperatorFactory>(sourceTypes, aggFuncTypes, aggsInputColsVector,
        maskCols, aggsOutputTypesPartial, inputRaws, outputPartials, false);
    (*outPartial)->Init();

    *outFinal = CreateAggregationOperatorFactory(aggFuncTypes, std::vector<uint32_t>({0}), partialOutTypeVec,
        finalOutTypeVec, std::vector<uint32_t>(), false, false, false);
}

// Global: 2 cols TINYINT (BYTE) value + percentile, Final output TINYINT.
TEST(ApproxPercentileAggregationTest, approx_percentile_partial_final_tinyint)
{
    const int32_t rowCount = 200;
    std::vector<DataTypePtr> sourceTypesVec = { ByteType(), DoubleType() };
    std::vector<DataTypePtr> partialOutputTypes = { VarBinaryType(65536) };
    std::vector<DataTypePtr> finalOutputTypes = { ByteType() };

    VectorBatch* vecBatch = new VectorBatch(rowCount);
    Vector<int8_t>* valueCol = new Vector<int8_t>(rowCount);
    Vector<double>* percentileCol = new Vector<double>(rowCount);
    for (int32_t rowIdx = 0; rowIdx < rowCount; ++rowIdx) {
        valueCol->SetValue(rowIdx, static_cast<int8_t>(rowIdx % 128));
        percentileCol->SetValue(rowIdx, 0.5);
    }
    vecBatch->Append(valueCol);
    vecBatch->Append(percentileCol);

    std::unique_ptr<AggregationOperatorFactory> partialFactory, finalFactory;
    BuildPartialFinalFactories(sourceTypesVec, partialOutputTypes, finalOutputTypes, &partialFactory, &finalFactory);
    auto aggPartial = partialFactory->CreateOperator();
    aggPartial->AddInput(vecBatch);

    VectorBatch* partialOutput = nullptr;
    (void)aggPartial->GetOutput(&partialOutput);
    ASSERT_NE(partialOutput, nullptr);

    auto aggFinal = finalFactory->CreateOperator();
    aggFinal->AddInput(partialOutput);
    op::Operator::DeleteOperator(aggPartial);

    VectorBatch* finalOutput = nullptr;
    (void)aggFinal->GetOutput(&finalOutput);
    ASSERT_NE(finalOutput, nullptr);
    auto* resultVec = static_cast<Vector<int8_t>*>(finalOutput->Get(0));
    int8_t approxMedian = resultVec->GetValue(0);
    EXPECT_GE(static_cast<int32_t>(approxMedian), 40);
    EXPECT_LE(static_cast<int32_t>(approxMedian), 90);

    op::Operator::DeleteOperator(aggFinal);
    VectorHelper::FreeVecBatch(finalOutput);
}

// Global: 2 cols SMALLINT (SHORT) value + percentile, Final output SMALLINT.
TEST(ApproxPercentileAggregationTest, approx_percentile_partial_final_smallint)
{
    const int32_t rowCount = 1000;
    std::vector<DataTypePtr> sourceTypesVec = { ShortType(), DoubleType() };
    std::vector<DataTypePtr> partialOutputTypes = { VarBinaryType(65536) };
    std::vector<DataTypePtr> finalOutputTypes = { ShortType() };

    VectorBatch* vecBatch = new VectorBatch(rowCount);
    Vector<int16_t>* valueCol = new Vector<int16_t>(rowCount);
    Vector<double>* percentileCol = new Vector<double>(rowCount);
    for (int32_t rowIdx = 0; rowIdx < rowCount; ++rowIdx) {
        valueCol->SetValue(rowIdx, static_cast<int16_t>(rowIdx * 5));
        percentileCol->SetValue(rowIdx, 0.5);
    }
    vecBatch->Append(valueCol);
    vecBatch->Append(percentileCol);

    std::unique_ptr<AggregationOperatorFactory> partialFactory, finalFactory;
    BuildPartialFinalFactories(sourceTypesVec, partialOutputTypes, finalOutputTypes, &partialFactory, &finalFactory);
    auto aggPartial = partialFactory->CreateOperator();
    aggPartial->AddInput(vecBatch);

    VectorBatch* partialOutput = nullptr;
    (void)aggPartial->GetOutput(&partialOutput);
    ASSERT_NE(partialOutput, nullptr);

    auto aggFinal = finalFactory->CreateOperator();
    aggFinal->AddInput(partialOutput);
    op::Operator::DeleteOperator(aggPartial);

    VectorBatch* finalOutput = nullptr;
    (void)aggFinal->GetOutput(&finalOutput);
    ASSERT_NE(finalOutput, nullptr);
    auto* resultVec = static_cast<Vector<int16_t>*>(finalOutput->Get(0));
    int16_t approxMedian = resultVec->GetValue(0);
    EXPECT_GE(approxMedian, 2000);
    EXPECT_LE(approxMedian, 3000);

    op::Operator::DeleteOperator(aggFinal);
    VectorHelper::FreeVecBatch(finalOutput);
}

// Global: 2 cols INT value + percentile, Final output INT.
TEST(ApproxPercentileAggregationTest, approx_percentile_partial_final_int)
{
    const int32_t rowCount = 1000;
    std::vector<DataTypePtr> sourceTypesVec = { IntType(), DoubleType() };
    std::vector<DataTypePtr> partialOutputTypes = { VarBinaryType(65536) };
    std::vector<DataTypePtr> finalOutputTypes = { IntType() };

    VectorBatch* vecBatch = new VectorBatch(rowCount);
    Vector<int32_t>* valueCol = new Vector<int32_t>(rowCount);
    Vector<double>* percentileCol = new Vector<double>(rowCount);
    for (int32_t rowIdx = 0; rowIdx < rowCount; ++rowIdx) {
        valueCol->SetValue(rowIdx, rowIdx * 10);
        percentileCol->SetValue(rowIdx, 0.5);
    }
    vecBatch->Append(valueCol);
    vecBatch->Append(percentileCol);

    std::unique_ptr<AggregationOperatorFactory> partialFactory, finalFactory;
    BuildPartialFinalFactories(sourceTypesVec, partialOutputTypes, finalOutputTypes, &partialFactory, &finalFactory);
    auto aggPartial = partialFactory->CreateOperator();
    aggPartial->AddInput(vecBatch);

    VectorBatch* partialOutput = nullptr;
    (void)aggPartial->GetOutput(&partialOutput);
    ASSERT_NE(partialOutput, nullptr);

    auto aggFinal = finalFactory->CreateOperator();
    aggFinal->AddInput(partialOutput);
    op::Operator::DeleteOperator(aggPartial);

    VectorBatch* finalOutput = nullptr;
    (void)aggFinal->GetOutput(&finalOutput);
    ASSERT_NE(finalOutput, nullptr);
    auto* resultVec = static_cast<Vector<int32_t>*>(finalOutput->Get(0));
    int32_t approxMedian = resultVec->GetValue(0);
    EXPECT_GE(approxMedian, 4000);
    EXPECT_LE(approxMedian, 6000);

    op::Operator::DeleteOperator(aggFinal);
    VectorHelper::FreeVecBatch(finalOutput);
}

// Global aggregation: 2 cols (LONG value, DOUBLE percentile 0.5), Partial -> VARBINARY, Final -> LONG. Approx median in range.
TEST(ApproxPercentileAggregationTest, approx_percentile_partial_final_long)
{
    const int32_t rowCount = 2000;
    std::vector<DataTypePtr> sourceTypesVec = { LongType(), DoubleType() };
    std::vector<DataTypePtr> partialOutputTypes = { VarBinaryType(65536) };
    std::vector<DataTypePtr> finalOutputTypes = { LongType() };

    VectorBatch* vecBatch = new VectorBatch(rowCount);
    Vector<int64_t>* valueCol = new Vector<int64_t>(rowCount);
    Vector<double>* percentileCol = new Vector<double>(rowCount);
    for (int32_t rowIdx = 0; rowIdx < rowCount; ++rowIdx) {
        valueCol->SetValue(rowIdx, static_cast<int64_t>(rowIdx));
        percentileCol->SetValue(rowIdx, 0.5);
    }
    vecBatch->Append(valueCol);
    vecBatch->Append(percentileCol);

    std::unique_ptr<AggregationOperatorFactory> partialFactory, finalFactory;
    BuildPartialFinalFactories(sourceTypesVec, partialOutputTypes, finalOutputTypes, &partialFactory, &finalFactory);
    auto aggPartial = partialFactory->CreateOperator();
    aggPartial->AddInput(vecBatch);

    VectorBatch* partialOutput = nullptr;
    (void)aggPartial->GetOutput(&partialOutput);
    ASSERT_NE(partialOutput, nullptr);
    EXPECT_EQ(partialOutput->GetRowCount(), 1);
    EXPECT_EQ(partialOutput->GetVectorCount(), 1);

    auto aggFinal = finalFactory->CreateOperator();
    aggFinal->AddInput(partialOutput);
    op::Operator::DeleteOperator(aggPartial);

    VectorBatch* finalOutput = nullptr;
    (void)aggFinal->GetOutput(&finalOutput);
    ASSERT_NE(finalOutput, nullptr);
    auto* resultVec = static_cast<Vector<int64_t>*>(finalOutput->Get(0));
    int64_t approxMedian = resultVec->GetValue(0);
    EXPECT_GE(approxMedian, static_cast<int64_t>(rowCount * 0.4));
    EXPECT_LE(approxMedian, static_cast<int64_t>(rowCount * 0.6));

    op::Operator::DeleteOperator(aggFinal);
    VectorHelper::FreeVecBatch(finalOutput);
}

// Global: 2 cols REAL (FLOAT) value + percentile, Final output FLOAT.
TEST(ApproxPercentileAggregationTest, approx_percentile_partial_final_float)
{
    const int32_t rowCount = 2000;
    std::vector<DataTypePtr> sourceTypesVec = { FloatType(), DoubleType() };
    std::vector<DataTypePtr> partialOutputTypes = { VarBinaryType(65536) };
    std::vector<DataTypePtr> finalOutputTypes = { FloatType() };

    VectorBatch* vecBatch = new VectorBatch(rowCount);
    Vector<float>* valueCol = new Vector<float>(rowCount);
    Vector<double>* percentileCol = new Vector<double>(rowCount);
    for (int32_t rowIdx = 0; rowIdx < rowCount; ++rowIdx) {
        valueCol->SetValue(rowIdx, static_cast<float>(rowIdx) * 2.0f);
        percentileCol->SetValue(rowIdx, 0.5);
    }
    vecBatch->Append(valueCol);
    vecBatch->Append(percentileCol);

    std::unique_ptr<AggregationOperatorFactory> partialFactory, finalFactory;
    BuildPartialFinalFactories(sourceTypesVec, partialOutputTypes, finalOutputTypes, &partialFactory, &finalFactory);
    auto aggPartial = partialFactory->CreateOperator();
    aggPartial->AddInput(vecBatch);

    VectorBatch* partialOutput = nullptr;
    (void)aggPartial->GetOutput(&partialOutput);
    ASSERT_NE(partialOutput, nullptr);

    auto aggFinal = finalFactory->CreateOperator();
    aggFinal->AddInput(partialOutput);
    op::Operator::DeleteOperator(aggPartial);

    VectorBatch* finalOutput = nullptr;
    (void)aggFinal->GetOutput(&finalOutput);
    ASSERT_NE(finalOutput, nullptr);
    auto* resultVec = static_cast<Vector<float>*>(finalOutput->Get(0));
    float approxMedian = resultVec->GetValue(0);
    float expectedApprox = static_cast<float>(rowCount - 1) * 2.0f * 0.5f;
    EXPECT_GE(approxMedian, expectedApprox * 0.8f);
    EXPECT_LE(approxMedian, expectedApprox * 1.2f);

    op::Operator::DeleteOperator(aggFinal);
    VectorHelper::FreeVecBatch(finalOutput);
}

// Global: 2 cols DOUBLE value + percentile, Final output DOUBLE.
TEST(ApproxPercentileAggregationTest, approx_percentile_partial_final_double)
{
    const int32_t rowCount = 2000;
    std::vector<DataTypePtr> sourceTypesVec = { DoubleType(), DoubleType() };
    std::vector<DataTypePtr> partialOutputTypes = { VarBinaryType(65536) };
    std::vector<DataTypePtr> finalOutputTypes = { DoubleType() };

    VectorBatch* vecBatch = new VectorBatch(rowCount);
    Vector<double>* valueCol = new Vector<double>(rowCount);
    Vector<double>* percentileCol = new Vector<double>(rowCount);
    for (int32_t rowIdx = 0; rowIdx < rowCount; ++rowIdx) {
        valueCol->SetValue(rowIdx, static_cast<double>(rowIdx) * 1.5);
        percentileCol->SetValue(rowIdx, 0.5);
    }
    vecBatch->Append(valueCol);
    vecBatch->Append(percentileCol);

    std::unique_ptr<AggregationOperatorFactory> partialFactory, finalFactory;
    BuildPartialFinalFactories(sourceTypesVec, partialOutputTypes, finalOutputTypes, &partialFactory, &finalFactory);
    auto aggPartial = partialFactory->CreateOperator();
    aggPartial->AddInput(vecBatch);

    VectorBatch* partialOutput = nullptr;
    (void)aggPartial->GetOutput(&partialOutput);
    ASSERT_NE(partialOutput, nullptr);

    auto aggFinal = finalFactory->CreateOperator();
    aggFinal->AddInput(partialOutput);
    op::Operator::DeleteOperator(aggPartial);

    VectorBatch* finalOutput = nullptr;
    (void)aggFinal->GetOutput(&finalOutput);
    ASSERT_NE(finalOutput, nullptr);
    auto* resultVec = static_cast<Vector<double>*>(finalOutput->Get(0));
    double approxMedian = resultVec->GetValue(0);
    double expectedApprox = static_cast<double>(rowCount - 1) * 1.5 * 0.5;
    EXPECT_GE(approxMedian, expectedApprox * 0.8);
    EXPECT_LE(approxMedian, expectedApprox * 1.2);

    op::Operator::DeleteOperator(aggFinal);
    VectorHelper::FreeVecBatch(finalOutput);
}

// Distributed: two partial batches merged in Final.
TEST(ApproxPercentileAggregationTest, approx_percentile_partial_final_merge_two_partials)
{
    std::vector<DataTypePtr> sourceTypesVec = { LongType(), DoubleType() };
    std::vector<DataTypePtr> partialOutputTypes = { VarBinaryType(65536) };
    std::vector<DataTypePtr> finalOutputTypes = { LongType() };

    VectorBatch* vec1 = new VectorBatch(1000);
    Vector<int64_t>* v1 = new Vector<int64_t>(1000);
    Vector<double>* p1 = new Vector<double>(1000);
    for (int32_t rowIdx = 0; rowIdx < 1000; ++rowIdx) {
        v1->SetValue(rowIdx, static_cast<int64_t>(rowIdx));
        p1->SetValue(rowIdx, 0.5);
    }
    vec1->Append(v1);
    vec1->Append(p1);

    VectorBatch* vec2 = new VectorBatch(1000);
    Vector<int64_t>* v2 = new Vector<int64_t>(1000);
    Vector<double>* p2 = new Vector<double>(1000);
    for (int32_t rowIdx = 0; rowIdx < 1000; ++rowIdx) {
        v2->SetValue(rowIdx, static_cast<int64_t>(1000 + rowIdx));
        p2->SetValue(rowIdx, 0.5);
    }
    vec2->Append(v2);
    vec2->Append(p2);

    std::unique_ptr<AggregationOperatorFactory> partialFactory, finalFactory;
    BuildPartialFinalFactories(sourceTypesVec, partialOutputTypes, finalOutputTypes, &partialFactory, &finalFactory);
    auto aggP1 = partialFactory->CreateOperator();
    aggP1->AddInput(vec1);
    VectorBatch* out1 = nullptr;
    (void)aggP1->GetOutput(&out1);
    ASSERT_NE(out1, nullptr);

    auto aggP2 = partialFactory->CreateOperator();
    aggP2->AddInput(vec2);
    VectorBatch* out2 = nullptr;
    (void)aggP2->GetOutput(&out2);
    ASSERT_NE(out2, nullptr);

    auto aggFinal = finalFactory->CreateOperator();
    aggFinal->AddInput(out1);
    aggFinal->AddInput(out2);
    op::Operator::DeleteOperator(aggP1);
    op::Operator::DeleteOperator(aggP2);

    VectorBatch* finalOutput = nullptr;
    (void)aggFinal->GetOutput(&finalOutput);
    ASSERT_NE(finalOutput, nullptr);
    auto* resultVec = static_cast<Vector<int64_t>*>(finalOutput->Get(0));
    int64_t approxMedian = resultVec->GetValue(0);
    EXPECT_GE(approxMedian, 900);
    EXPECT_LE(approxMedian, 1100);

    op::Operator::DeleteOperator(aggFinal);
    VectorHelper::FreeVecBatch(finalOutput);
}

// Group-by: Partial then Final, output type LONG (same as value). Use manual factory for 2-column agg.
TEST(ApproxPercentileAggregationTest, approx_percentile_group_by_partial_final)
{
    const int32_t numGroups = 4;
    const int32_t rowsPerGroup = 500;
    const int32_t rowCount = numGroups * rowsPerGroup;

    std::vector<DataTypePtr> groupTypesVec = { LongType() };
    std::vector<DataTypePtr> aggInputTypesVec = { LongType(), DoubleType() };
    std::vector<DataTypePtr> partialOutputTypes = { VarBinaryType(65536) };
    std::vector<DataTypePtr> finalOutputTypes = { LongType() };

    VectorBatch* vecBatch = new VectorBatch(rowCount);
    Vector<int64_t>* groupCol = new Vector<int64_t>(rowCount);
    Vector<int64_t>* valueCol = new Vector<int64_t>(rowCount);
    Vector<double>* percentileCol = new Vector<double>(rowCount);
    for (int32_t rowIdx = 0; rowIdx < rowCount; ++rowIdx) {
        int32_t groupId = rowIdx % numGroups;
        int64_t value = static_cast<int64_t>((rowIdx / numGroups) * 10);
        groupCol->SetValue(rowIdx, static_cast<int64_t>(groupId));
        valueCol->SetValue(rowIdx, value);
        percentileCol->SetValue(rowIdx, 0.5);
    }
    vecBatch->Append(groupCol);
    vecBatch->Append(valueCol);
    vecBatch->Append(percentileCol);

    std::vector<uint32_t> groupByCols = { 0 };
    DataTypes groupTypes(groupTypesVec);
    std::vector<std::vector<uint32_t>> partialAggCols = { { 1, 2 } };
    std::vector<DataTypes> partialAggInputTypes = { DataTypes(aggInputTypesVec) };
    std::vector<DataTypes> partialAggOutputTypes = { DataTypes(partialOutputTypes) };
    std::vector<uint32_t> aggFuncTypes = { OMNI_AGGREGATION_TYPE_APPROX_PERCENTILE };
    std::vector<uint32_t> maskCols = { static_cast<uint32_t>(-1) };
    std::vector<bool> inputRaws = { true };
    std::vector<bool> outputPartials = { true };

    HashAggregationOperatorFactory partialFactory(groupByCols, groupTypes, partialAggCols, partialAggInputTypes,
        partialAggOutputTypes, aggFuncTypes, maskCols, inputRaws, outputPartials, false);
    partialFactory.Init();
    auto aggPartial = partialFactory.CreateOperator();
    aggPartial->Init();
    aggPartial->AddInput(vecBatch);

    VectorBatch* partialOutput = nullptr;
    (void)aggPartial->GetOutput(&partialOutput);
    ASSERT_NE(partialOutput, nullptr);
    EXPECT_EQ(partialOutput->GetVectorCount(), 2u);
    EXPECT_EQ(partialOutput->GetRowCount(), numGroups);
    op::Operator::DeleteOperator(aggPartial);

    std::vector<std::vector<uint32_t>> finalAggCols = { { 1 } };
    std::vector<DataTypes> finalAggInputTypes = { DataTypes(partialOutputTypes) };
    std::vector<DataTypes> finalAggOutputTypes = { DataTypes(finalOutputTypes) };
    std::vector<bool> finalInputRaws = { false };
    std::vector<bool> finalOutputPartials = { false };

    HashAggregationOperatorFactory finalFactory(groupByCols, groupTypes, finalAggCols, finalAggInputTypes,
        finalAggOutputTypes, aggFuncTypes, maskCols, finalInputRaws, finalOutputPartials, false);
    finalFactory.Init();
    auto aggFinal = finalFactory.CreateOperator();
    aggFinal->Init();
    aggFinal->AddInput(partialOutput);

    VectorBatch* finalOutput = nullptr;
    (void)aggFinal->GetOutput(&finalOutput);
    ASSERT_NE(finalOutput, nullptr);
    EXPECT_EQ(finalOutput->GetVectorCount(), 2u);
    EXPECT_EQ(finalOutput->GetRowCount(), numGroups);

    auto* outValueVec = static_cast<Vector<int64_t>*>(finalOutput->Get(1));
    for (int32_t groupIdx = 0; groupIdx < numGroups; ++groupIdx) {
        int64_t approxMed = outValueVec->GetValue(groupIdx);
        int64_t expectedApprox = static_cast<int64_t>((rowsPerGroup - 1) / 2 * 10);
        EXPECT_GE(approxMed, expectedApprox - 500);
        EXPECT_LE(approxMed, expectedApprox + 500);
    }

    op::Operator::DeleteOperator(aggFinal);
    VectorHelper::FreeVecBatch(finalOutput);
}

// Null handling: one column with nulls, percentile 0.5; result non-null when at least one value.
TEST(ApproxPercentileAggregationTest, approx_percentile_partial_final_with_nulls)
{
    const int32_t rowCount = 100;
    std::vector<DataTypePtr> sourceTypesVec = { LongType(), DoubleType() };
    std::vector<DataTypePtr> partialOutputTypes = { VarBinaryType(65536) };
    std::vector<DataTypePtr> finalOutputTypes = { LongType() };

    VectorBatch* vecBatch = new VectorBatch(rowCount);
    Vector<int64_t>* valueCol = new Vector<int64_t>(rowCount);
    Vector<double>* percentileCol = new Vector<double>(rowCount);
    for (int32_t rowIdx = 0; rowIdx < rowCount; ++rowIdx) {
        if (rowIdx % 3 == 0) {
            valueCol->SetNull(rowIdx);
        } else {
            valueCol->SetValue(rowIdx, static_cast<int64_t>(rowIdx));
        }
        percentileCol->SetValue(rowIdx, 0.5);
    }
    vecBatch->Append(valueCol);
    vecBatch->Append(percentileCol);

    std::unique_ptr<AggregationOperatorFactory> partialFactory, finalFactory;
    BuildPartialFinalFactories(sourceTypesVec, partialOutputTypes, finalOutputTypes, &partialFactory, &finalFactory);
    auto aggPartial = partialFactory->CreateOperator();
    aggPartial->AddInput(vecBatch);

    VectorBatch* partialOutput = nullptr;
    (void)aggPartial->GetOutput(&partialOutput);
    ASSERT_NE(partialOutput, nullptr);

    auto aggFinal = finalFactory->CreateOperator();
    aggFinal->AddInput(partialOutput);
    op::Operator::DeleteOperator(aggPartial);

    VectorBatch* finalOutput = nullptr;
    (void)aggFinal->GetOutput(&finalOutput);
    ASSERT_NE(finalOutput, nullptr);
    auto* resultVec = static_cast<Vector<int64_t>*>(finalOutput->Get(0));
    EXPECT_FALSE(resultVec->IsNull(0));
    int64_t approxMed = resultVec->GetValue(0);
    EXPECT_GE(approxMed, 20);
    EXPECT_LE(approxMed, 90);

    op::Operator::DeleteOperator(aggFinal);
    VectorHelper::FreeVecBatch(finalOutput);
}

// Two-phase with dictionary-encoded value column (Long); percentile column flat Double. Same semantics as flat.
TEST(ApproxPercentileAggregationTest, approx_percentile_partial_final_dictionary)
{
    const int32_t rowCount = 2000;
    std::vector<DataTypePtr> sourceTypesVec = { LongType(), DoubleType() };
    std::vector<DataTypePtr> partialOutputTypes = { VarBinaryType(65536) };
    std::vector<DataTypePtr> finalOutputTypes = { LongType() };

    LongDataType longDataType;
    std::vector<int32_t> idsVec(rowCount);
    std::vector<int64_t> dictVec(rowCount);
    for (int32_t rowIdx = 0; rowIdx < rowCount; ++rowIdx) {
        idsVec[rowIdx] = rowIdx;
        dictVec[rowIdx] = static_cast<int64_t>(rowIdx);
    }
    BaseVector* valueCol = CreateDictionaryVector(longDataType, rowCount, idsVec.data(), rowCount, dictVec.data());
    Vector<double>* percentileCol = new Vector<double>(rowCount);
    for (int32_t rowIdx = 0; rowIdx < rowCount; ++rowIdx)
        percentileCol->SetValue(rowIdx, 0.5);

    VectorBatch* vecBatch = new VectorBatch(rowCount);
    vecBatch->Append(valueCol);
    vecBatch->Append(percentileCol);

    std::unique_ptr<AggregationOperatorFactory> partialFactory, finalFactory;
    BuildPartialFinalFactories(sourceTypesVec, partialOutputTypes, finalOutputTypes, &partialFactory, &finalFactory);
    auto aggPartial = partialFactory->CreateOperator();
    aggPartial->AddInput(vecBatch);

    VectorBatch* partialOutput = nullptr;
    (void)aggPartial->GetOutput(&partialOutput);
    ASSERT_NE(partialOutput, nullptr);

    auto aggFinal = finalFactory->CreateOperator();
    aggFinal->AddInput(partialOutput);
    op::Operator::DeleteOperator(aggPartial);

    VectorBatch* finalOutput = nullptr;
    (void)aggFinal->GetOutput(&finalOutput);
    ASSERT_NE(finalOutput, nullptr);
    auto* resultVec = static_cast<Vector<int64_t>*>(finalOutput->Get(0));
    int64_t approxMedian = resultVec->GetValue(0);
    EXPECT_GE(approxMedian, static_cast<int64_t>(rowCount * 0.4));
    EXPECT_LE(approxMedian, static_cast<int64_t>(rowCount * 0.6));

    op::Operator::DeleteOperator(aggFinal);
    VectorHelper::FreeVecBatch(finalOutput);
}

// Percentile column is ARRAY<DOUBLE> (e.g. [0.25, 0.5, 0.75]). Output is ARRAY<Long> with one value per percentile.
TEST(ApproxPercentileAggregationTest, approx_percentile_percentile_array)
{
    const int32_t rowCount = 2000;
    DataTypePtr arrayDoubleType = std::make_shared<ArrayType>(DoubleType());
    std::vector<DataTypePtr> sourceTypesVec = { LongType(), arrayDoubleType };
    std::vector<DataTypePtr> partialOutputTypes = { VarBinaryType(65536) };
    std::vector<DataTypePtr> finalOutputTypes = { std::make_shared<ArrayType>(LongType()) };

    VectorBatch* vecBatch = new VectorBatch(rowCount);
    Vector<int64_t>* valueCol = new Vector<int64_t>(rowCount);
    for (int32_t rowIdx = 0; rowIdx < rowCount; ++rowIdx)
        valueCol->SetValue(rowIdx, static_cast<int64_t>(rowIdx));
    vecBatch->Append(valueCol);

    const int32_t percentilesPerRow = 3;
    Vector<double>* elemVec = new Vector<double>(rowCount * percentilesPerRow);
    for (int32_t rowIdx = 0; rowIdx < rowCount; ++rowIdx) {
        elemVec->SetValue(rowIdx * percentilesPerRow + 0, 0.25);
        elemVec->SetValue(rowIdx * percentilesPerRow + 1, 0.5);
        elemVec->SetValue(rowIdx * percentilesPerRow + 2, 0.75);
    }
    ArrayVector* percentileCol = new ArrayVector(rowCount, std::shared_ptr<BaseVector>(elemVec));
    percentileCol->SetOffset(0, 0);
    for (int32_t rowIdx = 0; rowIdx < rowCount; ++rowIdx)
        percentileCol->SetSize(rowIdx, percentilesPerRow);
    vecBatch->Append(percentileCol);

    std::unique_ptr<AggregationOperatorFactory> partialFactory, finalFactory;
    BuildPartialFinalFactories(sourceTypesVec, partialOutputTypes, finalOutputTypes, &partialFactory, &finalFactory);
    auto aggPartial = partialFactory->CreateOperator();
    aggPartial->AddInput(vecBatch);

    VectorBatch* partialOutput = nullptr;
    (void)aggPartial->GetOutput(&partialOutput);
    ASSERT_NE(partialOutput, nullptr);

    auto aggFinal = finalFactory->CreateOperator();
    aggFinal->AddInput(partialOutput);
    op::Operator::DeleteOperator(aggPartial);

    VectorBatch* finalOutput = nullptr;
    (void)aggFinal->GetOutput(&finalOutput);
    ASSERT_NE(finalOutput, nullptr);
    BaseVector* resultCol = finalOutput->Get(0);
    ASSERT_EQ(resultCol->GetTypeId(), OMNI_ARRAY);
    ArrayVector* resultArray = static_cast<ArrayVector*>(resultCol);
    EXPECT_EQ(resultArray->GetSize(), 1);
    EXPECT_EQ(resultArray->GetSize(0), percentilesPerRow);
    int64_t startOffset = resultArray->GetOffset(0);
    BaseVector* resultElemVec = resultArray->GetElementVector().get();
    ASSERT_EQ(resultElemVec->GetTypeId(), OMNI_LONG);
    Vector<int64_t>* outLongs = static_cast<Vector<int64_t>*>(resultElemVec);
    int64_t approxP25 = outLongs->GetValue(static_cast<int32_t>(startOffset + 0));
    int64_t approxP50 = outLongs->GetValue(static_cast<int32_t>(startOffset + 1));
    int64_t approxP75 = outLongs->GetValue(static_cast<int32_t>(startOffset + 2));
    EXPECT_GE(approxP25, static_cast<int64_t>(rowCount * 0.2));
    EXPECT_LE(approxP25, static_cast<int64_t>(rowCount * 0.3));
    EXPECT_GE(approxP50, static_cast<int64_t>(rowCount * 0.4));
    EXPECT_LE(approxP50, static_cast<int64_t>(rowCount * 0.6));
    EXPECT_GE(approxP75, static_cast<int64_t>(rowCount * 0.7));
    EXPECT_LE(approxP75, static_cast<int64_t>(rowCount * 0.8));

    op::Operator::DeleteOperator(aggFinal);
    VectorHelper::FreeVecBatch(finalOutput);
}

// 3 columns: value (Long), weight (Long), percentile (Double). Partial -> Final, assert approx median in range.
TEST(ApproxPercentileAggregationTest, approx_percentile_partial_final_weight)
{
    const int32_t rowCount = 2000;
    std::vector<DataTypePtr> sourceTypesVec = { LongType(), LongType(), DoubleType() };
    std::vector<DataTypePtr> partialOutputTypes = { VarBinaryType(65536) };
    std::vector<DataTypePtr> finalOutputTypes = { LongType() };

    VectorBatch* vecBatch = new VectorBatch(rowCount);
    Vector<int64_t>* valueCol = new Vector<int64_t>(rowCount);
    Vector<int64_t>* weightCol = new Vector<int64_t>(rowCount);
    Vector<double>* percentileCol = new Vector<double>(rowCount);
    for (int32_t rowIdx = 0; rowIdx < rowCount; ++rowIdx) {
        valueCol->SetValue(rowIdx, static_cast<int64_t>(rowIdx));
        weightCol->SetValue(rowIdx, 1);
        percentileCol->SetValue(rowIdx, 0.5);
    }
    vecBatch->Append(valueCol);
    vecBatch->Append(weightCol);
    vecBatch->Append(percentileCol);

    std::unique_ptr<AggregationOperatorFactory> partialFactory, finalFactory;
    BuildPartialFinalFactories(sourceTypesVec, partialOutputTypes, finalOutputTypes, &partialFactory, &finalFactory);
    auto aggPartial = partialFactory->CreateOperator();
    aggPartial->AddInput(vecBatch);

    VectorBatch* partialOutput = nullptr;
    (void)aggPartial->GetOutput(&partialOutput);
    ASSERT_NE(partialOutput, nullptr);

    auto aggFinal = finalFactory->CreateOperator();
    aggFinal->AddInput(partialOutput);
    op::Operator::DeleteOperator(aggPartial);

    VectorBatch* finalOutput = nullptr;
    (void)aggFinal->GetOutput(&finalOutput);
    ASSERT_NE(finalOutput, nullptr);
    auto* resultVec = static_cast<Vector<int64_t>*>(finalOutput->Get(0));
    int64_t approxMedian = resultVec->GetValue(0);
    EXPECT_GE(approxMedian, static_cast<int64_t>(rowCount * 0.4));
    EXPECT_LE(approxMedian, static_cast<int64_t>(rowCount * 0.6));

    op::Operator::DeleteOperator(aggFinal);
    VectorHelper::FreeVecBatch(finalOutput);
}

// 3 columns: value (Long), percentile (Double), accuracy (Double). Accuracy 0.01 -> smaller epsilon. Assert result in range.
TEST(ApproxPercentileAggregationTest, approx_percentile_partial_final_accuracy)
{
    const int32_t rowCount = 2000;
    std::vector<DataTypePtr> sourceTypesVec = { LongType(), DoubleType(), LongType() };
    std::vector<DataTypePtr> partialOutputTypes = { VarBinaryType(65536) };
    std::vector<DataTypePtr> finalOutputTypes = { LongType() };

    VectorBatch* vecBatch = new VectorBatch(rowCount);
    Vector<int64_t>* valueCol = new Vector<int64_t>(rowCount);
    Vector<double>* percentileCol = new Vector<double>(rowCount);
    Vector<long>* accuracyCol = new Vector<long>(rowCount);
    for (int32_t rowIdx = 0; rowIdx < rowCount; ++rowIdx) {
        valueCol->SetValue(rowIdx, static_cast<int64_t>(rowIdx));
        percentileCol->SetValue(rowIdx, 0.5);
        accuracyCol->SetValue(rowIdx, 10000);
    }
    vecBatch->Append(valueCol);
    vecBatch->Append(percentileCol);
    vecBatch->Append(accuracyCol);

    std::unique_ptr<AggregationOperatorFactory> partialFactory, finalFactory;
    BuildPartialFinalFactories(sourceTypesVec, partialOutputTypes, finalOutputTypes, &partialFactory, &finalFactory);
    auto aggPartial = partialFactory->CreateOperator();
    aggPartial->AddInput(vecBatch);

    VectorBatch* partialOutput = nullptr;
    (void)aggPartial->GetOutput(&partialOutput);
    ASSERT_NE(partialOutput, nullptr);

    auto aggFinal = finalFactory->CreateOperator();
    aggFinal->AddInput(partialOutput);
    op::Operator::DeleteOperator(aggPartial);

    VectorBatch* finalOutput = nullptr;
    (void)aggFinal->GetOutput(&finalOutput);
    ASSERT_NE(finalOutput, nullptr);
    auto* resultVec = static_cast<Vector<int64_t>*>(finalOutput->Get(0));
    int64_t approxMedian = resultVec->GetValue(0);
    EXPECT_GE(approxMedian, static_cast<int64_t>(rowCount * 0.4));
    EXPECT_LE(approxMedian, static_cast<int64_t>(rowCount * 0.6));

    op::Operator::DeleteOperator(aggFinal);
    VectorHelper::FreeVecBatch(finalOutput);
}

// 4 columns: value (Long), weight (Long), percentile (Double), accuracy (Double). Partial -> Final.
TEST(ApproxPercentileAggregationTest, approx_percentile_partial_final_weight_accuracy)
{
    const int32_t rowCount = 2000;
    std::vector<DataTypePtr> sourceTypesVec = { LongType(), LongType(), DoubleType(), DoubleType() };
    std::vector<DataTypePtr> partialOutputTypes = { VarBinaryType(65536) };
    std::vector<DataTypePtr> finalOutputTypes = { LongType() };

    VectorBatch* vecBatch = new VectorBatch(rowCount);
    Vector<int64_t>* valueCol = new Vector<int64_t>(rowCount);
    Vector<int64_t>* weightCol = new Vector<int64_t>(rowCount);
    Vector<double>* percentileCol = new Vector<double>(rowCount);
    Vector<double>* accuracyCol = new Vector<double>(rowCount);
    for (int32_t rowIdx = 0; rowIdx < rowCount; ++rowIdx) {
        valueCol->SetValue(rowIdx, static_cast<int64_t>(rowIdx));
        weightCol->SetValue(rowIdx, 1);
        percentileCol->SetValue(rowIdx, 0.5);
        accuracyCol->SetValue(rowIdx, 0.01);
    }
    vecBatch->Append(valueCol);
    vecBatch->Append(weightCol);
    vecBatch->Append(percentileCol);
    vecBatch->Append(accuracyCol);

    std::unique_ptr<AggregationOperatorFactory> partialFactory, finalFactory;
    BuildPartialFinalFactories(sourceTypesVec, partialOutputTypes, finalOutputTypes, &partialFactory, &finalFactory);
    auto aggPartial = partialFactory->CreateOperator();
    aggPartial->AddInput(vecBatch);

    VectorBatch* partialOutput = nullptr;
    (void)aggPartial->GetOutput(&partialOutput);
    ASSERT_NE(partialOutput, nullptr);

    auto aggFinal = finalFactory->CreateOperator();
    aggFinal->AddInput(partialOutput);
    op::Operator::DeleteOperator(aggPartial);

    VectorBatch* finalOutput = nullptr;
    (void)aggFinal->GetOutput(&finalOutput);
    ASSERT_NE(finalOutput, nullptr);
    auto* resultVec = static_cast<Vector<int64_t>*>(finalOutput->Get(0));
    int64_t approxMedian = resultVec->GetValue(0);
    EXPECT_GE(approxMedian, static_cast<int64_t>(rowCount * 0.4));
    EXPECT_LE(approxMedian, static_cast<int64_t>(rowCount * 0.6));

    op::Operator::DeleteOperator(aggFinal);
    VectorHelper::FreeVecBatch(finalOutput);
}

// AlignSchema: partial HashAgg converts each row to (group, VARBINARY sketch). Aligned batch can be merged in Final.
TEST(ApproxPercentileAggregationTest, approx_percentile_align_schema)
{
    const int32_t rowCount = 100;
    const int32_t numGroups = 4;
    std::vector<DataTypePtr> groupTypesVec = { LongType() };
    std::vector<DataTypePtr> aggInputTypesVec = { LongType(), DoubleType() };
    std::vector<DataTypePtr> partialOutputTypes = { VarBinaryType(65536) };
    std::vector<DataTypePtr> finalOutputTypes = { LongType() };

    VectorBatch* inputBatch = new VectorBatch(rowCount);
    Vector<int64_t>* groupCol = new Vector<int64_t>(rowCount);
    Vector<int64_t>* valueCol = new Vector<int64_t>(rowCount);
    Vector<double>* percentileCol = new Vector<double>(rowCount);
    for (int32_t rowIdx = 0; rowIdx < rowCount; ++rowIdx) {
        groupCol->SetValue(rowIdx, static_cast<int64_t>(rowIdx % numGroups));
        valueCol->SetValue(rowIdx, static_cast<int64_t>(rowIdx));
        percentileCol->SetValue(rowIdx, 0.5);
    }
    inputBatch->Append(groupCol);
    inputBatch->Append(valueCol);
    inputBatch->Append(percentileCol);

    std::vector<uint32_t> groupByCols = { 0 };
    DataTypes groupTypes(groupTypesVec);
    std::vector<std::vector<uint32_t>> partialAggCols = { { 1, 2 } };
    std::vector<DataTypes> partialAggInputTypes = { DataTypes(aggInputTypesVec) };
    std::vector<DataTypes> partialAggOutputTypes = { DataTypes(partialOutputTypes) };
    std::vector<uint32_t> aggFuncTypes = { OMNI_AGGREGATION_TYPE_APPROX_PERCENTILE };
    std::vector<uint32_t> maskCols = { static_cast<uint32_t>(-1) };
    std::vector<bool> inputRaws = { true };
    std::vector<bool> outputPartials = { true };

    HashAggregationOperatorFactory partialFactory(groupByCols, groupTypes, partialAggCols, partialAggInputTypes,
        partialAggOutputTypes, aggFuncTypes, maskCols, inputRaws, outputPartials, false);
    partialFactory.Init();
    auto aggPartial = partialFactory.CreateOperator();
    aggPartial->Init();

    VectorBatch* alignedBatch = aggPartial->AlignSchema(inputBatch);
    ASSERT_NE(alignedBatch, nullptr);
    EXPECT_EQ(alignedBatch->GetRowCount(), rowCount);
    EXPECT_EQ(alignedBatch->GetVectorCount(), 2u);
    op::Operator::DeleteOperator(aggPartial);

    std::vector<std::vector<uint32_t>> finalAggCols = { { 1 } };
    std::vector<DataTypes> finalAggInputTypes = { DataTypes(partialOutputTypes) };
    std::vector<DataTypes> finalAggOutputTypes = { DataTypes(finalOutputTypes) };
    std::vector<bool> finalInputRaws = { false };
    std::vector<bool> finalOutputPartials = { false };
    HashAggregationOperatorFactory finalFactory(groupByCols, groupTypes, finalAggCols, finalAggInputTypes,
        finalAggOutputTypes, aggFuncTypes, maskCols, finalInputRaws, finalOutputPartials, false);
    finalFactory.Init();
    auto aggFinal = finalFactory.CreateOperator();
    aggFinal->Init();
    aggFinal->AddInput(alignedBatch);

    VectorBatch* finalOutput = nullptr;
    (void)aggFinal->GetOutput(&finalOutput);
    ASSERT_NE(finalOutput, nullptr);
    EXPECT_EQ(finalOutput->GetRowCount(), numGroups);
    auto* outValueVec = static_cast<Vector<int64_t>*>(finalOutput->Get(1));
    for (int32_t g = 0; g < numGroups; ++g) {
        int64_t approxMed = outValueVec->GetValue(g);
        int32_t groupSize = rowCount / numGroups;
        EXPECT_GE(approxMed, static_cast<int64_t>(groupSize * 0.3));
        EXPECT_LE(approxMed, static_cast<int64_t>(rowCount));
    }

    op::Operator::DeleteOperator(aggFinal);
    VectorHelper::FreeVecBatch(finalOutput);
}

// approx_percentile with ConstVector value input: all same values, median should be that value
TEST(ApproxPercentileAggregationTest, approx_percentile_partial_final_const_vector_long)
{
    const int32_t rowCount = 100;
    std::vector<DataTypePtr> sourceTypesVec = { LongType(), DoubleType() };
    std::vector<DataTypePtr> partialOutputTypes = { VarBinaryType(65536) };
    std::vector<DataTypePtr> finalOutputTypes = { LongType() };

    VectorBatch* vecBatch = new VectorBatch(rowCount);
    // ConstVector: all 100 rows have value 42
    auto *constValueCol = new ConstVector<int64_t>(42L, OMNI_LONG, rowCount);
    // Percentile column (0.5 for median)
    Vector<double>* percentileCol = new Vector<double>(rowCount);
    for (int32_t rowIdx = 0; rowIdx < rowCount; ++rowIdx) {
        percentileCol->SetValue(rowIdx, 0.5);
    }
    vecBatch->Append(constValueCol);
    vecBatch->Append(percentileCol);

    std::unique_ptr<AggregationOperatorFactory> partialFactory, finalFactory;
    BuildPartialFinalFactories(sourceTypesVec, partialOutputTypes, finalOutputTypes, &partialFactory, &finalFactory);
    auto aggPartial = partialFactory->CreateOperator();
    aggPartial->AddInput(vecBatch);

    VectorBatch* partialOutput = nullptr;
    (void)aggPartial->GetOutput(&partialOutput);
    ASSERT_NE(partialOutput, nullptr);

    auto aggFinal = finalFactory->CreateOperator();
    aggFinal->AddInput(partialOutput);
    op::Operator::DeleteOperator(aggPartial);

    VectorBatch* finalOutput = nullptr;
    (void)aggFinal->GetOutput(&finalOutput);
    ASSERT_NE(finalOutput, nullptr);
    auto* resultVec = static_cast<Vector<int64_t>*>(finalOutput->Get(0));
    int64_t approxMedian = resultVec->GetValue(0);
    // All values are 42, so median should be 42
    EXPECT_EQ(approxMedian, 42L);

    op::Operator::DeleteOperator(aggFinal);
    VectorHelper::FreeVecBatch(finalOutput);
}

// Three-stage: Partial -> PartialMerge -> Final. Two partials are merged into one VARBINARY, then finalized.
// Validates the partialMerge stage (inputRaw=false, outputPartial=true) that was previously rejected.
TEST(ApproxPercentileAggregationTest, approx_percentile_partial_merge_final)
{
    const int32_t rowsPerBatch = 1000;
    std::vector<DataTypePtr> sourceTypesVec = { LongType(), DoubleType() };
    std::vector<DataTypePtr> partialOutputTypes = { VarBinaryType(65536) };
    std::vector<DataTypePtr> finalOutputTypes = { LongType() };

    // Batch 1: values 0..999
    VectorBatch* vec1 = new VectorBatch(rowsPerBatch);
    Vector<int64_t>* v1 = new Vector<int64_t>(rowsPerBatch);
    Vector<double>* p1 = new Vector<double>(rowsPerBatch);
    for (int32_t rowIdx = 0; rowIdx < rowsPerBatch; ++rowIdx) {
        v1->SetValue(rowIdx, static_cast<int64_t>(rowIdx));
        p1->SetValue(rowIdx, 0.5);
    }
    vec1->Append(v1);
    vec1->Append(p1);

    // Batch 2: values 1000..1999
    VectorBatch* vec2 = new VectorBatch(rowsPerBatch);
    Vector<int64_t>* v2 = new Vector<int64_t>(rowsPerBatch);
    Vector<double>* p2 = new Vector<double>(rowsPerBatch);
    for (int32_t rowIdx = 0; rowIdx < rowsPerBatch; ++rowIdx) {
        v2->SetValue(rowIdx, static_cast<int64_t>(1000 + rowIdx));
        p2->SetValue(rowIdx, 0.5);
    }
    vec2->Append(v2);
    vec2->Append(p2);

    // Partial factory: raw (Long, Double) -> VARBINARY
    std::unique_ptr<AggregationOperatorFactory> partialFactory, finalFactory;
    BuildPartialFinalFactories(sourceTypesVec, partialOutputTypes, finalOutputTypes, &partialFactory, &finalFactory);

    auto aggP1 = partialFactory->CreateOperator();
    aggP1->AddInput(vec1);
    VectorBatch* out1 = nullptr;
    (void)aggP1->GetOutput(&out1);
    ASSERT_NE(out1, nullptr);

    auto aggP2 = partialFactory->CreateOperator();
    aggP2->AddInput(vec2);
    VectorBatch* out2 = nullptr;
    (void)aggP2->GetOutput(&out2);
    ASSERT_NE(out2, nullptr);

    // PartialMerge factory: VARBINARY -> VARBINARY (inputRaw=false, outputPartial=true)
    std::vector<uint32_t> mergeAggFuncTypes = { OMNI_AGGREGATION_TYPE_APPROX_PERCENTILE };
    auto mergeFactory = CreateAggregationOperatorFactory(mergeAggFuncTypes, std::vector<uint32_t>({0}),
        partialOutputTypes, partialOutputTypes, std::vector<uint32_t>(), false, true, false);

    auto aggMerge = mergeFactory->CreateOperator();
    aggMerge->AddInput(out1);
    aggMerge->AddInput(out2);
    op::Operator::DeleteOperator(aggP1);
    op::Operator::DeleteOperator(aggP2);

    VectorBatch* mergedOutput = nullptr;
    (void)aggMerge->GetOutput(&mergedOutput);
    ASSERT_NE(mergedOutput, nullptr);
    EXPECT_EQ(mergedOutput->GetRowCount(), 1);
    EXPECT_EQ(mergedOutput->GetVectorCount(), 1);

    // Final factory: VARBINARY -> Long
    auto aggFinal = finalFactory->CreateOperator();
    aggFinal->AddInput(mergedOutput);
    op::Operator::DeleteOperator(aggMerge);

    VectorBatch* finalOutput = nullptr;
    (void)aggFinal->GetOutput(&finalOutput);
    ASSERT_NE(finalOutput, nullptr);
    auto* resultVec = static_cast<Vector<int64_t>*>(finalOutput->Get(0));
    int64_t approxMedian = resultVec->GetValue(0);
    // Median of 0..1999 is ~999.5; allow generous tolerance for KLL approximation.
    EXPECT_GE(approxMedian, 800);
    EXPECT_LE(approxMedian, 1200);

    op::Operator::DeleteOperator(aggFinal);
    VectorHelper::FreeVecBatch(finalOutput);
}

// Single-stage Complete: raw values -> final scalar (inputRaw=true, outputPartial=false). No shuffle.
TEST(ApproxPercentileAggregationTest, approx_percentile_complete_single_stage)
{
    const int32_t rowCount = 2000;
    std::vector<DataTypePtr> sourceTypesVec = { LongType(), DoubleType() };
    std::vector<DataTypePtr> completeOutputTypes = { LongType() };

    VectorBatch* vecBatch = new VectorBatch(rowCount);
    Vector<int64_t>* valueCol = new Vector<int64_t>(rowCount);
    Vector<double>* percentileCol = new Vector<double>(rowCount);
    for (int32_t rowIdx = 0; rowIdx < rowCount; ++rowIdx) {
        valueCol->SetValue(rowIdx, static_cast<int64_t>(rowIdx));
        percentileCol->SetValue(rowIdx, 0.5);
    }
    vecBatch->Append(valueCol);
    vecBatch->Append(percentileCol);

    // Complete factory: raw (Long, Double) -> Long (inputRaw=true, outputPartial=false).
    // Create directly (like BuildPartialFinalFactories) because approx_percentile takes 2 input columns
    // (value + percentile) for one aggregator; CreateAggregationOperatorFactory would treat {0,1} as two aggregators.
    DataTypes sourceTypes(sourceTypesVec);
    std::vector<std::vector<uint32_t>> aggsInputColsVector = { std::vector<uint32_t>({0, 1}) };
    std::vector<uint32_t> aggFuncTypes = { OMNI_AGGREGATION_TYPE_APPROX_PERCENTILE };
    std::vector<uint32_t> maskCols = { static_cast<uint32_t>(-1) };
    std::vector<DataTypes> aggsOutputTypes = { DataTypes(completeOutputTypes) };
    std::vector<bool> inputRaws = { true };
    std::vector<bool> outputPartials = { false };
    auto completeFactory = std::make_unique<AggregationOperatorFactory>(sourceTypes, aggFuncTypes, aggsInputColsVector,
        maskCols, aggsOutputTypes, inputRaws, outputPartials, false);
    completeFactory->Init();

    auto aggComplete = completeFactory->CreateOperator();
    aggComplete->AddInput(vecBatch);

    VectorBatch* completeOutput = nullptr;
    (void)aggComplete->GetOutput(&completeOutput);
    ASSERT_NE(completeOutput, nullptr);
    auto* resultVec = static_cast<Vector<int64_t>*>(completeOutput->Get(0));
    int64_t approxMedian = resultVec->GetValue(0);
    EXPECT_GE(approxMedian, static_cast<int64_t>(rowCount * 0.4));
    EXPECT_LE(approxMedian, static_cast<int64_t>(rowCount * 0.6));

    op::Operator::DeleteOperator(aggComplete);
    VectorHelper::FreeVecBatch(completeOutput);
}
}
