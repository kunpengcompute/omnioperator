/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Unit tests for ApproxCountDistinct aggregation (ApproxCountDistinctAggregator and ApproxCountDistinctAggregatorFactory).
 * Coverage target: approx_count_distinct_aggregator and factory >= 75%.
 */

#include <memory>
#include <vector>
#include <gtest/gtest.h>
#include "operator/aggregation/aggregator/aggregator_factory.h"
#include "operator/aggregation/aggregator/aggregator_util.h"
#include "operator/aggregation/group_aggregation.h"
#include "operator/aggregation/non_group_aggregation.h"
#include "vector/vector_helper.h"
#include "type/data_type.h"
#include "util/test_agg_util.h"
#include "util/test_util.h"

namespace omniruntime {
using namespace omniruntime::vec;
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace TestUtil;

// Test approx_count_distinct two-phase (Sparse path): Partial outputs Sparse serialized, Final merges.
TEST(ApproxCountDistinctAggregationTest, approx_count_distinct_partial_final)
{
    const int32_t rowCount = 2000;
    const int32_t distinctCount = 1000;  // Few distinct values to keep Sparse
    std::vector<DataTypePtr> aggInputTypes = { LongType() };
    std::vector<DataTypePtr> partialOutputTypes = { VarBinaryType(65536) };
    std::vector<DataTypePtr> finalOutputTypes = { LongType() };

    VectorBatch *vecBatch = new VectorBatch(rowCount);
    Vector<int64_t> *col = new Vector<int64_t>(rowCount);
    for (int32_t j = 0; j < rowCount; ++j) {
        col->SetValue(j, j % distinctCount);
    }
    vecBatch->Append(col);

    auto partialFactory = CreateAggregationOperatorFactory(
        std::vector<uint32_t>({ OMNI_AGGREGATION_TYPE_APPROX_COUNT_DISTINCT }),
        std::vector<uint32_t>({ 0 }), aggInputTypes, partialOutputTypes,
        std::vector<uint32_t>(), true, true, false);
    auto aggPartial = partialFactory->CreateOperator(nullptr);
    aggPartial->AddInput(vecBatch);

    VectorBatch *partialOutput = nullptr;
    (void)aggPartial->GetOutput(&partialOutput);
    ASSERT_NE(partialOutput, nullptr);
    EXPECT_EQ(partialOutput->GetRowCount(), 1);
    EXPECT_EQ(partialOutput->GetVectorCount(), 1);

    auto finalFactory = CreateAggregationOperatorFactory(
        std::vector<uint32_t>({ OMNI_AGGREGATION_TYPE_APPROX_COUNT_DISTINCT }),
        std::vector<uint32_t>({ 0 }), partialOutputTypes, finalOutputTypes,
        std::vector<uint32_t>(), false, false, false);
    auto aggFinal = finalFactory->CreateOperator(nullptr);
    aggFinal->AddInput(partialOutput);  // AddInput frees partialOutput; partialOutput string_view points at Partial state, so AddInput before DeleteOperator(aggPartial)
    op::Operator::DeleteOperator(aggPartial);

    VectorBatch *finalOutput = nullptr;
    (void)aggFinal->GetOutput(&finalOutput);
    ASSERT_NE(finalOutput, nullptr);
    auto *resultVec = static_cast<Vector<int64_t> *>(finalOutput->Get(0));
    int64_t approxDistinct = resultVec->GetValue(0);
    EXPECT_GE(approxDistinct, static_cast<int64_t>(distinctCount * (1.0 - 0.05)));
    EXPECT_LE(approxDistinct, static_cast<int64_t>(distinctCount * (1.0 + 0.05)));

    op::Operator::DeleteOperator(aggFinal);
    VectorHelper::FreeVecBatch(finalOutput);
    // vecBatch already freed by aggPartial->AddInput(vecBatch), do not delete
}

// Test approx_count_distinct two-phase (Dense path): Partial switches to Dense due to many distinct values, Final merges Dense result.
TEST(ApproxCountDistinctAggregationTest, approx_count_distinct_partial_final_dense)
{
    const int32_t rowCount = 200000;
    const int32_t distinctCount = 100000;  // > 2048 so Partial uses Dense
    std::vector<DataTypePtr> aggInputTypes = { LongType() };
    std::vector<DataTypePtr> partialOutputTypes = { VarBinaryType(65536) };
    std::vector<DataTypePtr> finalOutputTypes = { LongType() };

    VectorBatch *vecBatch = new VectorBatch(rowCount);
    Vector<int64_t> *col = new Vector<int64_t>(rowCount);
    for (int32_t j = 0; j < rowCount; ++j) {
        col->SetValue(j, j % distinctCount);
    }
    vecBatch->Append(col);

    auto partialFactory = CreateAggregationOperatorFactory(
        std::vector<uint32_t>({ OMNI_AGGREGATION_TYPE_APPROX_COUNT_DISTINCT }),
        std::vector<uint32_t>({ 0 }), aggInputTypes, partialOutputTypes,
        std::vector<uint32_t>(), true, true, false);
    auto aggPartial = partialFactory->CreateOperator(nullptr);
    aggPartial->AddInput(vecBatch);

    VectorBatch *partialOutput = nullptr;
    (void)aggPartial->GetOutput(&partialOutput);
    ASSERT_NE(partialOutput, nullptr);

    auto finalFactory = CreateAggregationOperatorFactory(
        std::vector<uint32_t>({ OMNI_AGGREGATION_TYPE_APPROX_COUNT_DISTINCT }),
        std::vector<uint32_t>({ 0 }), partialOutputTypes, finalOutputTypes,
        std::vector<uint32_t>(), false, false, false);
    auto aggFinal = finalFactory->CreateOperator(nullptr);
    aggFinal->AddInput(partialOutput);
    op::Operator::DeleteOperator(aggPartial);

    VectorBatch *finalOutput = nullptr;
    (void)aggFinal->GetOutput(&finalOutput);
    ASSERT_NE(finalOutput, nullptr);
    auto *resultVec = static_cast<Vector<int64_t> *>(finalOutput->Get(0));
    int64_t approxDistinct = resultVec->GetValue(0);
    EXPECT_GE(approxDistinct, static_cast<int64_t>(distinctCount * (1.0 - 0.05)));
    EXPECT_LE(approxDistinct, static_cast<int64_t>(distinctCount * (1.0 + 0.05)));

    op::Operator::DeleteOperator(aggFinal);
    VectorHelper::FreeVecBatch(finalOutput);
}

// Simulate distributed: one partition Sparse, one Dense; Final merges both (Sparse + Dense mix).
TEST(ApproxCountDistinctAggregationTest, approx_count_distinct_partial_final_mixed_sparse_dense)
{
    std::vector<DataTypePtr> aggInputTypes = { LongType() };
    std::vector<DataTypePtr> partialOutputTypes = { VarBinaryType(65536) };
    std::vector<DataTypePtr> finalOutputTypes = { LongType() };

    VectorBatch *vecSparse = new VectorBatch(1000);
    Vector<int64_t> *colSparse = new Vector<int64_t>(1000);
    for (int32_t j = 0; j < 1000; ++j) {
        colSparse->SetValue(j, j % 5);
    }
    vecSparse->Append(colSparse);

    VectorBatch *vecDense = new VectorBatch(6000);
    Vector<int64_t> *colDense = new Vector<int64_t>(6000);
    for (int32_t j = 0; j < 6000; ++j) {
        colDense->SetValue(j, j % 5000);
    }
    vecDense->Append(colDense);

    auto partialFactory = CreateAggregationOperatorFactory(
        std::vector<uint32_t>({ OMNI_AGGREGATION_TYPE_APPROX_COUNT_DISTINCT }),
        std::vector<uint32_t>({ 0 }), aggInputTypes, partialOutputTypes,
        std::vector<uint32_t>(), true, true, false);

    auto aggPartialSparse = partialFactory->CreateOperator(nullptr);
    aggPartialSparse->AddInput(vecSparse);
    VectorBatch *partialSparseOut = nullptr;
    (void)aggPartialSparse->GetOutput(&partialSparseOut);
    ASSERT_NE(partialSparseOut, nullptr);

    auto aggPartialDense = partialFactory->CreateOperator(nullptr);
    aggPartialDense->AddInput(vecDense);
    VectorBatch *partialDenseOut = nullptr;
    (void)aggPartialDense->GetOutput(&partialDenseOut);
    ASSERT_NE(partialDenseOut, nullptr);

    auto finalFactory = CreateAggregationOperatorFactory(
        std::vector<uint32_t>({ OMNI_AGGREGATION_TYPE_APPROX_COUNT_DISTINCT }),
        std::vector<uint32_t>({ 0 }), partialOutputTypes, finalOutputTypes,
        std::vector<uint32_t>(), false, false, false);
    auto aggFinal = finalFactory->CreateOperator(nullptr);
    aggFinal->AddInput(partialSparseOut);
    aggFinal->AddInput(partialDenseOut);
    op::Operator::DeleteOperator(aggPartialSparse);
    op::Operator::DeleteOperator(aggPartialDense);

    VectorBatch *finalOutput = nullptr;
    (void)aggFinal->GetOutput(&finalOutput);
    ASSERT_NE(finalOutput, nullptr);
    auto *resultVec = static_cast<Vector<int64_t> *>(finalOutput->Get(0));
    int64_t approxDistinct = resultVec->GetValue(0);
    EXPECT_GE(approxDistinct, 4000);
    EXPECT_LE(approxDistinct, 6000);

    op::Operator::DeleteOperator(aggFinal);
    VectorHelper::FreeVecBatch(finalOutput);
    // vecSparse/vecDense freed by Partial AddInput; partialSparseOut/partialDenseOut freed in this path; do not delete again to avoid double-free
}

// Two-phase (INT): Partial outputs VARBINARY, Final merges and outputs BIGINT.
TEST(ApproxCountDistinctAggregationTest, approx_count_distinct_partial_final_int)
{
    const int32_t rowCount = 2000;
    const int32_t distinctCount = 1000;
    std::vector<DataTypePtr> aggInputTypes = { IntType() };
    std::vector<DataTypePtr> partialOutputTypes = { VarBinaryType(65536) };
    std::vector<DataTypePtr> finalOutputTypes = { LongType() };

    VectorBatch *vecBatch = new VectorBatch(rowCount);
    Vector<int32_t> *col = new Vector<int32_t>(rowCount);
    for (int32_t j = 0; j < rowCount; ++j) {
        col->SetValue(j, j % distinctCount);
    }
    vecBatch->Append(col);

    auto partialFactory = CreateAggregationOperatorFactory(
        std::vector<uint32_t>({ OMNI_AGGREGATION_TYPE_APPROX_COUNT_DISTINCT }),
        std::vector<uint32_t>({ 0 }), aggInputTypes, partialOutputTypes,
        std::vector<uint32_t>(), true, true, false);
    auto aggPartial = partialFactory->CreateOperator(nullptr);
    aggPartial->AddInput(vecBatch);

    VectorBatch *partialOutput = nullptr;
    (void)aggPartial->GetOutput(&partialOutput);
    ASSERT_NE(partialOutput, nullptr);

    auto finalFactory = CreateAggregationOperatorFactory(
        std::vector<uint32_t>({ OMNI_AGGREGATION_TYPE_APPROX_COUNT_DISTINCT }),
        std::vector<uint32_t>({ 0 }), partialOutputTypes, finalOutputTypes,
        std::vector<uint32_t>(), false, false, false);
    auto aggFinal = finalFactory->CreateOperator(nullptr);
    aggFinal->AddInput(partialOutput);
    op::Operator::DeleteOperator(aggPartial);

    VectorBatch *finalOutput = nullptr;
    (void)aggFinal->GetOutput(&finalOutput);
    ASSERT_NE(finalOutput, nullptr);
    auto *resultVec = static_cast<Vector<int64_t> *>(finalOutput->Get(0));
    int64_t approxDistinct = resultVec->GetValue(0);
    EXPECT_GE(approxDistinct, static_cast<int64_t>(distinctCount * (1.0 - 0.05)));
    EXPECT_LE(approxDistinct, static_cast<int64_t>(distinctCount * (1.0 + 0.05)));

    op::Operator::DeleteOperator(aggFinal);
    VectorHelper::FreeVecBatch(finalOutput);
}

// Two-phase (BOOLEAN): Partial outputs 1-byte state VARBINARY, Final merges and outputs 0/1/2.
TEST(ApproxCountDistinctAggregationTest, approx_count_distinct_partial_final_boolean)
{
    const int32_t rowCount = 100;
    std::vector<DataTypePtr> aggInputTypes = { BooleanType() };
    std::vector<DataTypePtr> partialOutputTypes = { VarBinaryType(65536) };
    std::vector<DataTypePtr> finalOutputTypes = { LongType() };

    VectorBatch *vecBatch = new VectorBatch(rowCount);
    Vector<bool> *col = new Vector<bool>(rowCount);
    for (int32_t j = 0; j < rowCount; ++j) {
        col->SetValue(j, (j % 2) != 0);
    }
    vecBatch->Append(col);

    auto partialFactory = CreateAggregationOperatorFactory(
        std::vector<uint32_t>({ OMNI_AGGREGATION_TYPE_APPROX_COUNT_DISTINCT }),
        std::vector<uint32_t>({ 0 }), aggInputTypes, partialOutputTypes,
        std::vector<uint32_t>(), true, true, false);
    auto aggPartial = partialFactory->CreateOperator(nullptr);
    aggPartial->AddInput(vecBatch);

    VectorBatch *partialOutput = nullptr;
    (void)aggPartial->GetOutput(&partialOutput);
    ASSERT_NE(partialOutput, nullptr);

    auto finalFactory = CreateAggregationOperatorFactory(
        std::vector<uint32_t>({ OMNI_AGGREGATION_TYPE_APPROX_COUNT_DISTINCT }),
        std::vector<uint32_t>({ 0 }), partialOutputTypes, finalOutputTypes,
        std::vector<uint32_t>(), false, false, false);
    auto aggFinal = finalFactory->CreateOperator(nullptr);
    aggFinal->AddInput(partialOutput);
    op::Operator::DeleteOperator(aggPartial);

    VectorBatch *finalOutput = nullptr;
    (void)aggFinal->GetOutput(&finalOutput);
    ASSERT_NE(finalOutput, nullptr);
    auto *resultVec = static_cast<Vector<int64_t> *>(finalOutput->Get(0));
    EXPECT_EQ(resultVec->GetValue(0), 2);

    op::Operator::DeleteOperator(aggFinal);
    VectorHelper::FreeVecBatch(finalOutput);
}

// Two-phase (VARCHAR): Partial outputs VARBINARY, Final merges and outputs BIGINT.
TEST(ApproxCountDistinctAggregationTest, approx_count_distinct_partial_final_varchar)
{
    const int32_t rowCount = 2000;
    const int32_t distinctCount = 500;
    std::vector<std::string> storage(rowCount);
    std::vector<DataTypePtr> aggInputTypes = { VarcharType(32) };
    std::vector<DataTypePtr> partialOutputTypes = { VarBinaryType(65536) };
    std::vector<DataTypePtr> finalOutputTypes = { LongType() };

    VectorBatch *vecBatch = new VectorBatch(rowCount);
    auto *col = new Vector<LargeStringContainer<std::string_view>>(rowCount);
    for (int32_t j = 0; j < rowCount; ++j) {
        storage[j] = "v" + std::to_string(j % distinctCount);
        std::string_view sv(storage[j]);
        col->SetValue(j, sv);
    }
    vecBatch->Append(col);

    auto partialFactory = CreateAggregationOperatorFactory(
        std::vector<uint32_t>({ OMNI_AGGREGATION_TYPE_APPROX_COUNT_DISTINCT }),
        std::vector<uint32_t>({ 0 }), aggInputTypes, partialOutputTypes,
        std::vector<uint32_t>(), true, true, false);
    auto aggPartial = partialFactory->CreateOperator(nullptr);
    aggPartial->AddInput(vecBatch);

    VectorBatch *partialOutput = nullptr;
    (void)aggPartial->GetOutput(&partialOutput);
    ASSERT_NE(partialOutput, nullptr);

    auto finalFactory = CreateAggregationOperatorFactory(
        std::vector<uint32_t>({ OMNI_AGGREGATION_TYPE_APPROX_COUNT_DISTINCT }),
        std::vector<uint32_t>({ 0 }), partialOutputTypes, finalOutputTypes,
        std::vector<uint32_t>(), false, false, false);
    auto aggFinal = finalFactory->CreateOperator(nullptr);
    aggFinal->AddInput(partialOutput);
    op::Operator::DeleteOperator(aggPartial);

    VectorBatch *finalOutput = nullptr;
    (void)aggFinal->GetOutput(&finalOutput);
    ASSERT_NE(finalOutput, nullptr);
    auto *resultVec = static_cast<Vector<int64_t> *>(finalOutput->Get(0));
    int64_t approxDistinct = resultVec->GetValue(0);
    EXPECT_GE(approxDistinct, static_cast<int64_t>(distinctCount * (1.0 - 0.05)));
    EXPECT_LE(approxDistinct, static_cast<int64_t>(distinctCount * (1.0 + 0.05)));

    op::Operator::DeleteOperator(aggFinal);
    VectorHelper::FreeVecBatch(finalOutput);
}

// Two-phase (CHAR): Partial outputs VARBINARY, Final merges and outputs BIGINT.
TEST(ApproxCountDistinctAggregationTest, approx_count_distinct_partial_final_char)
{
    const int32_t rowCount = 2000;
    const int32_t distinctCount = 500;
    std::vector<std::string> storage(rowCount);
    std::vector<DataTypePtr> aggInputTypes = { CharType(16) };
    std::vector<DataTypePtr> partialOutputTypes = { VarBinaryType(65536) };
    std::vector<DataTypePtr> finalOutputTypes = { LongType() };

    VectorBatch *vecBatch = new VectorBatch(rowCount);
    auto *col = new Vector<LargeStringContainer<std::string_view>>(rowCount);
    for (int32_t j = 0; j < rowCount; ++j) {
        storage[j] = "c" + std::to_string(j % distinctCount);
        std::string_view sv(storage[j]);
        col->SetValue(j, sv);
    }
    vecBatch->Append(col);

    auto partialFactory = CreateAggregationOperatorFactory(
        std::vector<uint32_t>({ OMNI_AGGREGATION_TYPE_APPROX_COUNT_DISTINCT }),
        std::vector<uint32_t>({ 0 }), aggInputTypes, partialOutputTypes,
        std::vector<uint32_t>(), true, true, false);
    auto aggPartial = partialFactory->CreateOperator(nullptr);
    aggPartial->AddInput(vecBatch);

    VectorBatch *partialOutput = nullptr;
    (void)aggPartial->GetOutput(&partialOutput);
    ASSERT_NE(partialOutput, nullptr);

    auto finalFactory = CreateAggregationOperatorFactory(
        std::vector<uint32_t>({ OMNI_AGGREGATION_TYPE_APPROX_COUNT_DISTINCT }),
        std::vector<uint32_t>({ 0 }), partialOutputTypes, finalOutputTypes,
        std::vector<uint32_t>(), false, false, false);
    auto aggFinal = finalFactory->CreateOperator(nullptr);
    aggFinal->AddInput(partialOutput);
    op::Operator::DeleteOperator(aggPartial);

    VectorBatch *finalOutput = nullptr;
    (void)aggFinal->GetOutput(&finalOutput);
    ASSERT_NE(finalOutput, nullptr);
    auto *resultVec = static_cast<Vector<int64_t> *>(finalOutput->Get(0));
    int64_t approxDistinct = resultVec->GetValue(0);
    EXPECT_GE(approxDistinct, static_cast<int64_t>(distinctCount * (1.0 - 0.05)));
    EXPECT_LE(approxDistinct, static_cast<int64_t>(distinctCount * (1.0 + 0.05)));

    op::Operator::DeleteOperator(aggFinal);
    VectorHelper::FreeVecBatch(finalOutput);
}

// Two-phase (TINYINT/Byte): Partial outputs VARBINARY, Final merges and outputs BIGINT.
TEST(ApproxCountDistinctAggregationTest, approx_count_distinct_partial_final_tinyint)
{
    const int32_t rowCount = 2000;
    const int32_t distinctCount = 100;
    std::vector<DataTypePtr> aggInputTypes = { ByteType() };
    std::vector<DataTypePtr> partialOutputTypes = { VarBinaryType(65536) };
    std::vector<DataTypePtr> finalOutputTypes = { LongType() };

    VectorBatch *vecBatch = new VectorBatch(rowCount);
    Vector<int8_t> *col = new Vector<int8_t>(rowCount);
    for (int32_t j = 0; j < rowCount; ++j) {
        col->SetValue(j, static_cast<int8_t>(j % distinctCount));
    }
    vecBatch->Append(col);

    auto partialFactory = CreateAggregationOperatorFactory(
        std::vector<uint32_t>({ OMNI_AGGREGATION_TYPE_APPROX_COUNT_DISTINCT }),
        std::vector<uint32_t>({ 0 }), aggInputTypes, partialOutputTypes,
        std::vector<uint32_t>(), true, true, false);
    auto aggPartial = partialFactory->CreateOperator(nullptr);
    aggPartial->AddInput(vecBatch);

    VectorBatch *partialOutput = nullptr;
    (void)aggPartial->GetOutput(&partialOutput);
    ASSERT_NE(partialOutput, nullptr);

    auto finalFactory = CreateAggregationOperatorFactory(
        std::vector<uint32_t>({ OMNI_AGGREGATION_TYPE_APPROX_COUNT_DISTINCT }),
        std::vector<uint32_t>({ 0 }), partialOutputTypes, finalOutputTypes,
        std::vector<uint32_t>(), false, false, false);
    auto aggFinal = finalFactory->CreateOperator(nullptr);
    aggFinal->AddInput(partialOutput);
    op::Operator::DeleteOperator(aggPartial);

    VectorBatch *finalOutput = nullptr;
    (void)aggFinal->GetOutput(&finalOutput);
    ASSERT_NE(finalOutput, nullptr);
    auto *resultVec = static_cast<Vector<int64_t> *>(finalOutput->Get(0));
    int64_t approxDistinct = resultVec->GetValue(0);
    EXPECT_GE(approxDistinct, static_cast<int64_t>(distinctCount * (1.0 - 0.05)));
    EXPECT_LE(approxDistinct, static_cast<int64_t>(distinctCount * (1.0 + 0.05)));

    op::Operator::DeleteOperator(aggFinal);
    VectorHelper::FreeVecBatch(finalOutput);
}

// Two-phase (SMALLINT): Partial outputs VARBINARY, Final merges and outputs BIGINT.
TEST(ApproxCountDistinctAggregationTest, approx_count_distinct_partial_final_smallint)
{
    const int32_t rowCount = 10000;
    const int32_t distinctCount = 10000;
    std::vector<DataTypePtr> aggInputTypes = { ShortType() };
    std::vector<DataTypePtr> partialOutputTypes = { VarBinaryType(65536) };
    std::vector<DataTypePtr> finalOutputTypes = { LongType() };

    VectorBatch *vecBatch = new VectorBatch(rowCount);
    Vector<int16_t> *col = new Vector<int16_t>(rowCount);
    for (int32_t j = 0; j < rowCount; ++j) {
        col->SetValue(j, static_cast<int16_t>(j % distinctCount));
    }
    vecBatch->Append(col);

    auto partialFactory = CreateAggregationOperatorFactory(
        std::vector<uint32_t>({ OMNI_AGGREGATION_TYPE_APPROX_COUNT_DISTINCT }),
        std::vector<uint32_t>({ 0 }), aggInputTypes, partialOutputTypes,
        std::vector<uint32_t>(), true, true, false);
    auto aggPartial = partialFactory->CreateOperator(nullptr);
    aggPartial->AddInput(vecBatch);

    VectorBatch *partialOutput = nullptr;
    (void)aggPartial->GetOutput(&partialOutput);
    ASSERT_NE(partialOutput, nullptr);

    auto finalFactory = CreateAggregationOperatorFactory(
        std::vector<uint32_t>({ OMNI_AGGREGATION_TYPE_APPROX_COUNT_DISTINCT }),
        std::vector<uint32_t>({ 0 }), partialOutputTypes, finalOutputTypes,
        std::vector<uint32_t>(), false, false, false);
    auto aggFinal = finalFactory->CreateOperator(nullptr);
    aggFinal->AddInput(partialOutput);
    op::Operator::DeleteOperator(aggPartial);

    VectorBatch *finalOutput = nullptr;
    (void)aggFinal->GetOutput(&finalOutput);
    ASSERT_NE(finalOutput, nullptr);
    auto *resultVec = static_cast<Vector<int64_t> *>(finalOutput->Get(0));
    int64_t approxDistinct = resultVec->GetValue(0);
    EXPECT_GE(approxDistinct, static_cast<int64_t>(distinctCount * (1.0 - 0.05)));
    EXPECT_LE(approxDistinct, static_cast<int64_t>(distinctCount * (1.0 + 0.05)));

    op::Operator::DeleteOperator(aggFinal);
    VectorHelper::FreeVecBatch(finalOutput);
}

// Two-phase (FLOAT): Partial outputs VARBINARY, Final merges and outputs BIGINT.
TEST(ApproxCountDistinctAggregationTest, approx_count_distinct_partial_final_float)
{
    const int32_t rowCount = 2000;
    const int32_t distinctCount = 1000;
    std::vector<DataTypePtr> aggInputTypes = { FloatType() };
    std::vector<DataTypePtr> partialOutputTypes = { VarBinaryType(65536) };
    std::vector<DataTypePtr> finalOutputTypes = { LongType() };

    VectorBatch *vecBatch = new VectorBatch(rowCount);
    Vector<float> *col = new Vector<float>(rowCount);
    for (int32_t j = 0; j < rowCount; ++j) {
        col->SetValue(j, static_cast<float>(j % distinctCount) * 1.5f);
    }
    vecBatch->Append(col);

    auto partialFactory = CreateAggregationOperatorFactory(
        std::vector<uint32_t>({ OMNI_AGGREGATION_TYPE_APPROX_COUNT_DISTINCT }),
        std::vector<uint32_t>({ 0 }), aggInputTypes, partialOutputTypes,
        std::vector<uint32_t>(), true, true, false);
    auto aggPartial = partialFactory->CreateOperator(nullptr);
    aggPartial->AddInput(vecBatch);

    VectorBatch *partialOutput = nullptr;
    (void)aggPartial->GetOutput(&partialOutput);
    ASSERT_NE(partialOutput, nullptr);

    auto finalFactory = CreateAggregationOperatorFactory(
        std::vector<uint32_t>({ OMNI_AGGREGATION_TYPE_APPROX_COUNT_DISTINCT }),
        std::vector<uint32_t>({ 0 }), partialOutputTypes, finalOutputTypes,
        std::vector<uint32_t>(), false, false, false);
    auto aggFinal = finalFactory->CreateOperator(nullptr);
    aggFinal->AddInput(partialOutput);
    op::Operator::DeleteOperator(aggPartial);

    VectorBatch *finalOutput = nullptr;
    (void)aggFinal->GetOutput(&finalOutput);
    ASSERT_NE(finalOutput, nullptr);
    auto *resultVec = static_cast<Vector<int64_t> *>(finalOutput->Get(0));
    int64_t approxDistinct = resultVec->GetValue(0);
    EXPECT_GE(approxDistinct, static_cast<int64_t>(distinctCount * (1.0 - 0.05)));
    EXPECT_LE(approxDistinct, static_cast<int64_t>(distinctCount * (1.0 + 0.05)));

    op::Operator::DeleteOperator(aggFinal);
    VectorHelper::FreeVecBatch(finalOutput);
}

// Two-phase (DOUBLE): Partial outputs VARBINARY, Final merges and outputs BIGINT.
TEST(ApproxCountDistinctAggregationTest, approx_count_distinct_partial_final_double)
{
    const int32_t rowCount = 2000;
    const int32_t distinctCount = 1000;
    std::vector<DataTypePtr> aggInputTypes = { DoubleType() };
    std::vector<DataTypePtr> partialOutputTypes = { VarBinaryType(65536) };
    std::vector<DataTypePtr> finalOutputTypes = { LongType() };

    VectorBatch *vecBatch = new VectorBatch(rowCount);
    Vector<double> *col = new Vector<double>(rowCount);
    for (int32_t j = 0; j < rowCount; ++j) {
        col->SetValue(j, static_cast<double>(j % distinctCount) * 1.25);
    }
    vecBatch->Append(col);

    auto partialFactory = CreateAggregationOperatorFactory(
        std::vector<uint32_t>({ OMNI_AGGREGATION_TYPE_APPROX_COUNT_DISTINCT }),
        std::vector<uint32_t>({ 0 }), aggInputTypes, partialOutputTypes,
        std::vector<uint32_t>(), true, true, false);
    auto aggPartial = partialFactory->CreateOperator(nullptr);
    aggPartial->AddInput(vecBatch);

    VectorBatch *partialOutput = nullptr;
    (void)aggPartial->GetOutput(&partialOutput);
    ASSERT_NE(partialOutput, nullptr);

    auto finalFactory = CreateAggregationOperatorFactory(
        std::vector<uint32_t>({ OMNI_AGGREGATION_TYPE_APPROX_COUNT_DISTINCT }),
        std::vector<uint32_t>({ 0 }), partialOutputTypes, finalOutputTypes,
        std::vector<uint32_t>(), false, false, false);
    auto aggFinal = finalFactory->CreateOperator(nullptr);
    aggFinal->AddInput(partialOutput);
    op::Operator::DeleteOperator(aggPartial);

    VectorBatch *finalOutput = nullptr;
    (void)aggFinal->GetOutput(&finalOutput);
    ASSERT_NE(finalOutput, nullptr);
    auto *resultVec = static_cast<Vector<int64_t> *>(finalOutput->Get(0));
    int64_t approxDistinct = resultVec->GetValue(0);
    EXPECT_GE(approxDistinct, static_cast<int64_t>(distinctCount * (1.0 - 0.05)));
    EXPECT_LE(approxDistinct, static_cast<int64_t>(distinctCount * (1.0 + 0.05)));

    op::Operator::DeleteOperator(aggFinal);
    VectorHelper::FreeVecBatch(finalOutput);
}

// Two-phase (VARBINARY): Partial outputs VARBINARY, Final merges and outputs BIGINT.
TEST(ApproxCountDistinctAggregationTest, approx_count_distinct_partial_final_varbinary)
{
    const int32_t rowCount = 2000;
    const int32_t distinctCount = 500;
    std::vector<std::string> storage(rowCount);
    std::vector<DataTypePtr> aggInputTypes = { VarBinaryType(64) };
    std::vector<DataTypePtr> partialOutputTypes = { VarBinaryType(65536) };
    std::vector<DataTypePtr> finalOutputTypes = { LongType() };

    VectorBatch *vecBatch = new VectorBatch(rowCount);
    auto *col = new Vector<LargeStringContainer<std::string_view>>(rowCount);
    for (int32_t j = 0; j < rowCount; ++j) {
        storage[j] = "bin" + std::to_string(j % distinctCount);
        std::string_view sv(storage[j]);
        col->SetValue(j, sv);
    }
    vecBatch->Append(col);

    auto partialFactory = CreateAggregationOperatorFactory(
        std::vector<uint32_t>({ OMNI_AGGREGATION_TYPE_APPROX_COUNT_DISTINCT }),
        std::vector<uint32_t>({ 0 }), aggInputTypes, partialOutputTypes,
        std::vector<uint32_t>(), true, true, false);
    auto aggPartial = partialFactory->CreateOperator(nullptr);
    aggPartial->AddInput(vecBatch);

    VectorBatch *partialOutput = nullptr;
    (void)aggPartial->GetOutput(&partialOutput);
    ASSERT_NE(partialOutput, nullptr);

    auto finalFactory = CreateAggregationOperatorFactory(
        std::vector<uint32_t>({ OMNI_AGGREGATION_TYPE_APPROX_COUNT_DISTINCT }),
        std::vector<uint32_t>({ 0 }), partialOutputTypes, finalOutputTypes,
        std::vector<uint32_t>(), false, false, false);
    auto aggFinal = finalFactory->CreateOperator(nullptr);
    aggFinal->AddInput(partialOutput);
    op::Operator::DeleteOperator(aggPartial);

    VectorBatch *finalOutput = nullptr;
    (void)aggFinal->GetOutput(&finalOutput);
    ASSERT_NE(finalOutput, nullptr);
    auto *resultVec = static_cast<Vector<int64_t> *>(finalOutput->Get(0));
    int64_t approxDistinct = resultVec->GetValue(0);
    EXPECT_GE(approxDistinct, static_cast<int64_t>(distinctCount * (1.0 - 0.05)));
    EXPECT_LE(approxDistinct, static_cast<int64_t>(distinctCount * (1.0 + 0.05)));

    op::Operator::DeleteOperator(aggFinal);
    VectorHelper::FreeVecBatch(finalOutput);
}

// Two-phase (TIMESTAMP, underlying LONG): Partial outputs VARBINARY, Final merges and outputs BIGINT.
TEST(ApproxCountDistinctAggregationTest, approx_count_distinct_partial_final_timestamp)
{
    const int32_t rowCount = 2000;
    const int32_t distinctCount = 1000;
    std::vector<DataTypePtr> aggInputTypes = { TimestampType() };
    std::vector<DataTypePtr> partialOutputTypes = { VarBinaryType(65536) };
    std::vector<DataTypePtr> finalOutputTypes = { LongType() };

    VectorBatch *vecBatch = new VectorBatch(rowCount);
    Vector<int64_t> *col = new Vector<int64_t>(rowCount);
    for (int32_t j = 0; j < rowCount; ++j) {
        col->SetValue(j, static_cast<int64_t>(j % distinctCount) * 1000000);
    }
    vecBatch->Append(col);

    auto partialFactory = CreateAggregationOperatorFactory(
        std::vector<uint32_t>({ OMNI_AGGREGATION_TYPE_APPROX_COUNT_DISTINCT }),
        std::vector<uint32_t>({ 0 }), aggInputTypes, partialOutputTypes,
        std::vector<uint32_t>(), true, true, false);
    auto aggPartial = partialFactory->CreateOperator(nullptr);
    aggPartial->AddInput(vecBatch);

    VectorBatch *partialOutput = nullptr;
    (void)aggPartial->GetOutput(&partialOutput);
    ASSERT_NE(partialOutput, nullptr);

    auto finalFactory = CreateAggregationOperatorFactory(
        std::vector<uint32_t>({ OMNI_AGGREGATION_TYPE_APPROX_COUNT_DISTINCT }),
        std::vector<uint32_t>({ 0 }), partialOutputTypes, finalOutputTypes,
        std::vector<uint32_t>(), false, false, false);
    auto aggFinal = finalFactory->CreateOperator(nullptr);
    aggFinal->AddInput(partialOutput);
    op::Operator::DeleteOperator(aggPartial);

    VectorBatch *finalOutput = nullptr;
    (void)aggFinal->GetOutput(&finalOutput);
    ASSERT_NE(finalOutput, nullptr);
    auto *resultVec = static_cast<Vector<int64_t> *>(finalOutput->Get(0));
    int64_t approxDistinct = resultVec->GetValue(0);
    EXPECT_GE(approxDistinct, static_cast<int64_t>(distinctCount * (1.0 - 0.05)));
    EXPECT_LE(approxDistinct, static_cast<int64_t>(distinctCount * (1.0 + 0.05)));

    op::Operator::DeleteOperator(aggFinal);
    VectorHelper::FreeVecBatch(finalOutput);
}

// Two-phase (DATE32, underlying INT): Partial outputs VARBINARY, Final merges and outputs BIGINT.
TEST(ApproxCountDistinctAggregationTest, approx_count_distinct_partial_final_date32)
{
    const int32_t rowCount = 2000;
    const int32_t distinctCount = 1000;
    std::vector<DataTypePtr> aggInputTypes = { Date32Type() };
    std::vector<DataTypePtr> partialOutputTypes = { VarBinaryType(65536) };
    std::vector<DataTypePtr> finalOutputTypes = { LongType() };

    VectorBatch *vecBatch = new VectorBatch(rowCount);
    Vector<int32_t> *col = new Vector<int32_t>(rowCount);
    for (int32_t j = 0; j < rowCount; ++j) {
        col->SetValue(j, static_cast<int32_t>(j % distinctCount) * 86400);
    }
    vecBatch->Append(col);

    auto partialFactory = CreateAggregationOperatorFactory(
        std::vector<uint32_t>({ OMNI_AGGREGATION_TYPE_APPROX_COUNT_DISTINCT }),
        std::vector<uint32_t>({ 0 }), aggInputTypes, partialOutputTypes,
        std::vector<uint32_t>(), true, true, false);
    auto aggPartial = partialFactory->CreateOperator(nullptr);
    aggPartial->AddInput(vecBatch);

    VectorBatch *partialOutput = nullptr;
    (void)aggPartial->GetOutput(&partialOutput);
    ASSERT_NE(partialOutput, nullptr);

    auto finalFactory = CreateAggregationOperatorFactory(
        std::vector<uint32_t>({ OMNI_AGGREGATION_TYPE_APPROX_COUNT_DISTINCT }),
        std::vector<uint32_t>({ 0 }), partialOutputTypes, finalOutputTypes,
        std::vector<uint32_t>(), false, false, false);
    auto aggFinal = finalFactory->CreateOperator(nullptr);
    aggFinal->AddInput(partialOutput);
    op::Operator::DeleteOperator(aggPartial);

    VectorBatch *finalOutput = nullptr;
    (void)aggFinal->GetOutput(&finalOutput);
    ASSERT_NE(finalOutput, nullptr);
    auto *resultVec = static_cast<Vector<int64_t> *>(finalOutput->Get(0));
    int64_t approxDistinct = resultVec->GetValue(0);
    EXPECT_GE(approxDistinct, static_cast<int64_t>(distinctCount * (1.0 - 0.05)));
    EXPECT_LE(approxDistinct, static_cast<int64_t>(distinctCount * (1.0 + 0.05)));

    op::Operator::DeleteOperator(aggFinal);
    VectorHelper::FreeVecBatch(finalOutput);
}

// Multi-group aggregation (two-phase): Partial outputs VARBINARY per group, Final merges per group and outputs BIGINT cardinality per group.
TEST(ApproxCountDistinctAggregationTest, approx_count_distinct_group_by_partial_final)
{
    const int32_t numGroups = 4;
    const int32_t rowsPerGroup = 1500;
    const int32_t distinctPerGroup = 100;
    const int32_t rowCount = numGroups * rowsPerGroup;

    std::vector<DataTypePtr> groupTypes = { LongType() };
    std::vector<DataTypePtr> aggInputTypes = { LongType() };
    std::vector<DataTypePtr> partialOutputTypes = { VarBinaryType(65536) };
    std::vector<DataTypePtr> finalOutputTypes = { LongType() };

    VectorBatch *vecBatch = new VectorBatch(rowCount);
    Vector<int64_t> *groupCol = new Vector<int64_t>(rowCount);
    Vector<int64_t> *valueCol = new Vector<int64_t>(rowCount);
    for (int32_t i = 0; i < rowCount; ++i) {
        int32_t g = i % numGroups;
        int32_t v = (i / numGroups) % distinctPerGroup;
        groupCol->SetValue(i, static_cast<int64_t>(g));
        valueCol->SetValue(i, static_cast<int64_t>(v));
    }
    vecBatch->Append(groupCol);
    vecBatch->Append(valueCol);

    auto partialFactory = CreateHashAggregationOperatorFactory(
        std::vector<uint32_t>({ 0 }),
        groupTypes,
        std::vector<uint32_t>({ OMNI_AGGREGATION_TYPE_APPROX_COUNT_DISTINCT }),
        std::vector<uint32_t>({ 1 }),
        aggInputTypes,
        partialOutputTypes,
        std::vector<uint32_t>(), true, true, false);
    auto aggPartial = partialFactory->CreateOperator(nullptr);
    aggPartial->Init();
    aggPartial->AddInput(vecBatch);

    VectorBatch *partialOutput = nullptr;
    (void)aggPartial->GetOutput(&partialOutput);
    ASSERT_NE(partialOutput, nullptr);
    EXPECT_EQ(partialOutput->GetVectorCount(), 2u);
    EXPECT_EQ(partialOutput->GetRowCount(), numGroups);
    op::Operator::DeleteOperator(aggPartial);

    auto finalFactory = CreateHashAggregationOperatorFactory(
        std::vector<uint32_t>({ 0 }),
        groupTypes,
        std::vector<uint32_t>({ OMNI_AGGREGATION_TYPE_APPROX_COUNT_DISTINCT }),
        std::vector<uint32_t>({ 1 }),
        partialOutputTypes,
        finalOutputTypes,
        std::vector<uint32_t>(), false, false, false);
    auto aggFinal = finalFactory->CreateOperator(nullptr);
    aggFinal->Init();
    aggFinal->AddInput(partialOutput);

    VectorBatch *finalOutput = nullptr;
    (void)aggFinal->GetOutput(&finalOutput);
    ASSERT_NE(finalOutput, nullptr);
    EXPECT_EQ(finalOutput->GetVectorCount(), 2u);
    EXPECT_EQ(finalOutput->GetRowCount(), numGroups);

    auto *outApproxVec = static_cast<Vector<int64_t> *>(finalOutput->Get(1));
    for (int32_t i = 0; i < numGroups; ++i) {
        int64_t approxDistinct = outApproxVec->GetValue(i);
        EXPECT_GE(approxDistinct, static_cast<int64_t>(distinctPerGroup * (1.0 - 0.05)));
        EXPECT_LE(approxDistinct, static_cast<int64_t>(distinctPerGroup * (1.0 + 0.05)));
    }

    op::Operator::DeleteOperator(aggFinal);
    VectorHelper::FreeVecBatch(finalOutput);
}

// approx_count_distinct second argument max standard error (double): two-column input [value column, maxStandardError], single-stage and two-phase.
TEST(ApproxCountDistinctAggregationTest, approx_count_distinct_with_max_standard_error)
{
    const int32_t rowCount = 2000;
    const int32_t distinctCount = 1000;
    std::vector<DataTypePtr> sourceTypeVec = { LongType(), DoubleType() };
    DataTypes sourceTypes(sourceTypeVec);
    std::vector<std::vector<uint32_t>> aggsInputColsVector = {{0, 1}};
    std::vector<uint32_t> aggFuncTypes = { OMNI_AGGREGATION_TYPE_APPROX_COUNT_DISTINCT };
    std::vector<uint32_t> maskCols = { static_cast<uint32_t>(-1) };
    std::vector<DataTypePtr> partialOutTypeVec = { VarBinaryType(65536) };
    std::vector<DataTypes> aggsOutputTypesPartial = { DataTypes(partialOutTypeVec) };
    std::vector<bool> inputRaws = { true };
    std::vector<bool> outputPartials = { true };

    VectorBatch *vecBatch = new VectorBatch(rowCount);
    Vector<int64_t> *col0 = new Vector<int64_t>(rowCount);
    Vector<double> *col1 = new Vector<double>(rowCount);
    const double maxErr = 0.01;
    for (int32_t j = 0; j < rowCount; ++j) {
        col0->SetValue(j, j % distinctCount);
        col1->SetValue(j, maxErr);
    }
    vecBatch->Append(col0);
    vecBatch->Append(col1);

    auto partialFactory = std::make_unique<AggregationOperatorFactory>(sourceTypes, aggFuncTypes, aggsInputColsVector,
        maskCols, aggsOutputTypesPartial, inputRaws, outputPartials, false);
    partialFactory->Init();
    auto aggPartial = partialFactory->CreateOperator(nullptr);
    aggPartial->AddInput(vecBatch);
    VectorBatch *partialOutput = nullptr;
    (void)aggPartial->GetOutput(&partialOutput);
    ASSERT_NE(partialOutput, nullptr);
    EXPECT_EQ(partialOutput->GetRowCount(), 1);
    op::Operator::DeleteOperator(aggPartial);

    std::vector<DataTypePtr> finalOutTypeVec = { LongType() };
    std::vector<DataTypes> aggsOutputTypesFinal = { DataTypes(finalOutTypeVec) };
    std::vector<DataTypes> partialOutputTypesForFinal = { DataTypes(partialOutTypeVec) };
    std::vector<std::vector<uint32_t>> finalInputCols = {{0}};
    std::vector<uint32_t> finalMask = { static_cast<uint32_t>(-1) };
    DataTypes partialSourceTypes(partialOutTypeVec);
    std::vector<bool> finalInputRaws = { false };
    std::vector<bool> finalOutputPartials = { false };
    auto finalFactory = std::make_unique<AggregationOperatorFactory>(partialSourceTypes, aggFuncTypes, finalInputCols,
        finalMask, aggsOutputTypesFinal, finalInputRaws, finalOutputPartials, false);
    finalFactory->Init();
    auto aggFinal = finalFactory->CreateOperator(nullptr);
    aggFinal->AddInput(partialOutput);
    VectorBatch *finalOutput = nullptr;
    (void)aggFinal->GetOutput(&finalOutput);
    ASSERT_NE(finalOutput, nullptr);
    auto *resultVec = static_cast<Vector<int64_t> *>(finalOutput->Get(0));
    int64_t approxDistinct = resultVec->GetValue(0);
    EXPECT_GE(approxDistinct, static_cast<int64_t>(distinctCount * (1.0 - 0.05)));
    EXPECT_LE(approxDistinct, static_cast<int64_t>(distinctCount * (1.0 + 0.05)));

    op::Operator::DeleteOperator(aggFinal);
    VectorHelper::FreeVecBatch(finalOutput);
}

// Two-phase with OMNI_DICTIONARY input: agg column is dictionary-encoded Long; Partial/Final path same as flat.
TEST(ApproxCountDistinctAggregationTest, approx_count_distinct_partial_final_dictionary)
{
    const int32_t rowCount = 2000;
    const int32_t distinctCount = 1000;
    std::vector<DataTypePtr> aggInputTypes = { LongType() };
    std::vector<DataTypePtr> partialOutputTypes = { VarBinaryType(65536) };
    std::vector<DataTypePtr> finalOutputTypes = { LongType() };

    LongDataType longDataType;
    std::vector<int32_t> idsVec(rowCount);
    std::vector<int64_t> dictVec(rowCount);
    for (int32_t i = 0; i < rowCount; ++i) {
        idsVec[i] = i;
        dictVec[i] = static_cast<int64_t>(i % distinctCount);
    }
    BaseVector *dictCol = CreateDictionaryVector(longDataType, rowCount, idsVec.data(), rowCount, dictVec.data());
    VectorBatch *vecBatch = new VectorBatch(rowCount);
    vecBatch->Append(dictCol);

    auto partialFactory = CreateAggregationOperatorFactory(
        std::vector<uint32_t>({ OMNI_AGGREGATION_TYPE_APPROX_COUNT_DISTINCT }),
        std::vector<uint32_t>({ 0 }), aggInputTypes, partialOutputTypes,
        std::vector<uint32_t>(), true, true, false);
    auto aggPartial = partialFactory->CreateOperator(nullptr);
    aggPartial->AddInput(vecBatch);

    VectorBatch *partialOutput = nullptr;
    (void)aggPartial->GetOutput(&partialOutput);
    ASSERT_NE(partialOutput, nullptr);
    EXPECT_EQ(partialOutput->GetRowCount(), 1);
    EXPECT_EQ(partialOutput->GetVectorCount(), 1);

    auto finalFactory = CreateAggregationOperatorFactory(
        std::vector<uint32_t>({ OMNI_AGGREGATION_TYPE_APPROX_COUNT_DISTINCT }),
        std::vector<uint32_t>({ 0 }), partialOutputTypes, finalOutputTypes,
        std::vector<uint32_t>(), false, false, false);
    auto aggFinal = finalFactory->CreateOperator(nullptr);
    aggFinal->AddInput(partialOutput);
    op::Operator::DeleteOperator(aggPartial);

    VectorBatch *finalOutput = nullptr;
    (void)aggFinal->GetOutput(&finalOutput);
    ASSERT_NE(finalOutput, nullptr);
    auto *resultVec = static_cast<Vector<int64_t> *>(finalOutput->Get(0));
    int64_t approxDistinct = resultVec->GetValue(0);
    EXPECT_GE(approxDistinct, static_cast<int64_t>(distinctCount * (1.0 - 0.05)));
    EXPECT_LE(approxDistinct, static_cast<int64_t>(distinctCount * (1.0 + 0.05)));

    op::Operator::DeleteOperator(aggFinal);
    VectorHelper::FreeVecBatch(finalOutput);
}

// approx_count_distinct with ConstVector input: all same values should give count ~1
TEST(ApproxCountDistinctAggregationTest, approx_count_distinct_partial_final_const_vector)
{
    const int32_t rowCount = 100;
    std::vector<DataTypePtr> aggInputTypes = { LongType() };
    std::vector<DataTypePtr> partialOutputTypes = { VarBinaryType(65536) };
    std::vector<DataTypePtr> finalOutputTypes = { LongType() };

    // ConstVector: all 100 rows have value 42
    VectorBatch *vecBatch = new VectorBatch(rowCount);
    auto *constCol = new ConstVector<int64_t>(42L, OMNI_LONG, rowCount);
    vecBatch->Append(constCol);

    auto partialFactory = CreateAggregationOperatorFactory(
        std::vector<uint32_t>({ OMNI_AGGREGATION_TYPE_APPROX_COUNT_DISTINCT }),
        std::vector<uint32_t>({ 0 }), aggInputTypes, partialOutputTypes,
        std::vector<uint32_t>(), true, true, false);
    auto aggPartial = partialFactory->CreateOperator(nullptr);
    aggPartial->AddInput(vecBatch);

    VectorBatch *partialOutput = nullptr;
    (void)aggPartial->GetOutput(&partialOutput);
    ASSERT_NE(partialOutput, nullptr);
    EXPECT_EQ(partialOutput->GetRowCount(), 1);

    auto finalFactory = CreateAggregationOperatorFactory(
        std::vector<uint32_t>({ OMNI_AGGREGATION_TYPE_APPROX_COUNT_DISTINCT }),
        std::vector<uint32_t>({ 0 }), partialOutputTypes, finalOutputTypes,
        std::vector<uint32_t>(), false, false, false);
    auto aggFinal = finalFactory->CreateOperator(nullptr);
    aggFinal->AddInput(partialOutput);
    op::Operator::DeleteOperator(aggPartial);

    VectorBatch *finalOutput = nullptr;
    (void)aggFinal->GetOutput(&finalOutput);
    ASSERT_NE(finalOutput, nullptr);
    auto *resultVec = static_cast<Vector<int64_t> *>(finalOutput->Get(0));
    int64_t approxDistinct = resultVec->GetValue(0);
    // All values are the same, so approx distinct count should be 1
    EXPECT_EQ(approxDistinct, 1);

    op::Operator::DeleteOperator(aggFinal);
    VectorHelper::FreeVecBatch(finalOutput);
}
}
