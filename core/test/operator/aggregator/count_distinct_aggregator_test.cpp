/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 * Description: Unit tests for CountDistinct aggregation without Expand
 * (CountDistinctAggregator, CountDistinctVarcharAggregator, CountDistinctAggregatorFactory).
 *
 * Semantics under test:
 *   - Partial stage:  raw T    -> array<T> (distinct set, shuffle intermediate format)
 *   - Final stage:    array<T> -> int64 distinct count
 *   - Complete stage: raw T   -> int64 distinct count
 *   - NULL inputs are ignored (count semantics): empty/all-null input yields 0, not null.
 */

#include <memory>
#include <vector>
#include <algorithm>
#include <chrono>
#include <gtest/gtest.h>
#include "operator/aggregation/aggregator/count_distinct_aggregator.h"
#include "operator/aggregation/aggregator/aggregator_factory.h"
#include "operator/aggregation/aggregator/aggregator_util.h"
#include "operator/aggregation/aggregator/aggregator.h"
#include "vector/vector_helper.h"
#include "vector/array_vector.h"
#include "type/data_type.h"
#include "util/type_util.h"
#include "util/test_util.h"
#include "operator/execution_context.h"
#include "memory/thread_memory_manager.h"

namespace omniruntime {
using namespace omniruntime::vec;
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace TestUtil;

static std::unique_ptr<AggregateState[]> NewAndInitState(Aggregator *agg, int32_t off = 0)
{
    auto state = std::make_unique<AggregateState[]>(agg->GetStateSize());
    agg->SetStateOffset(off);
    agg->InitState(state.get());
    return state;
}

static DataTypePtr ArrayOf(const DataTypePtr &elementType)
{
    return std::make_shared<ArrayType>(elementType);
}

/** Call before freeing state buffer to avoid leak. Mirrors CollectSet state destruction. */
static void DestroyCountDistinctState(Aggregator *agg, AggregateState *state)
{
    if (auto *p = dynamic_cast<CollectSetAggregator<OMNI_INT, OMNI_INT> *>(agg)) {
        p->DestroyState(state);
    } else if (auto *q = dynamic_cast<CollectSetAggregator<OMNI_LONG, OMNI_LONG> *>(agg)) {
        q->DestroyState(state);
    } else if (auto *d = dynamic_cast<CollectSetAggregator<OMNI_DOUBLE, OMNI_DOUBLE> *>(agg)) {
        d->DestroyState(state);
    } else if (auto *v = dynamic_cast<CollectSetVarcharAggregator *>(agg)) {
        v->DestroyState(state);
    }
}

// ---- CountDistinctAggregatorFactory tests ----

TEST(CountDistinctAggregatorTest, FactoryCompleteInt)
{
    CountDistinctAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{IntType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{LongType()});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, false, false);
    ASSERT_NE(agg, nullptr);
    EXPECT_TRUE(agg->IsInputRaw());
    EXPECT_TRUE(agg->IsTypedAggregator());
    EXPECT_GT(agg->GetStateSize(), 0u);
}

TEST(CountDistinctAggregatorTest, FactoryPartialLong)
{
    CountDistinctAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{LongType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{ArrayOf(LongType())});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, true, false);
    ASSERT_NE(agg, nullptr);
}

TEST(CountDistinctAggregatorTest, FactoryFinalArrayDouble)
{
    CountDistinctAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{ArrayOf(DoubleType())});
    DataTypes outputTypes(std::vector<DataTypePtr>{LongType()});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, false, false, false);
    ASSERT_NE(agg, nullptr);
}

TEST(CountDistinctAggregatorTest, FactoryVarcharUsesVarcharAggregator)
{
    CountDistinctAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{VarcharType()});
    DataTypes outputTypes(std::vector<DataTypePtr>{LongType()});
    auto agg = factory.CreateAggregator(inputTypes, outputTypes, channels, true, false, false);
    ASSERT_NE(agg, nullptr);
    EXPECT_NE(dynamic_cast<CountDistinctVarcharAggregator *>(agg.get()), nullptr);
}

TEST(CountDistinctAggregatorTest, FactoryUnsupportedElementTypeThrows)
{
    CountDistinctAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    DataTypes inputTypes(std::vector<DataTypePtr>{ArrayOf(IntType())});
    DataTypes outputTypes(std::vector<DataTypePtr>{LongType()});
    EXPECT_THROW(factory.CreateAggregator(inputTypes, outputTypes, channels, true, false, false),
        std::exception);
}

TEST(CountDistinctAggregatorTest, FactorySwitchRegistration)
{
    // Verifies OMNI_AGGREGATION_TYPE_COUNT_DISTINCT case in aggregator_factory.cpp switch.
    auto factory = CreateAggregatorFactory(OMNI_AGGREGATION_TYPE_COUNT_DISTINCT);
    ASSERT_NE(factory, nullptr);
    std::vector<int32_t> channels = {0};
    auto agg = factory->CreateAggregator(DataTypes(std::vector<DataTypePtr>{IntType()}),
        DataTypes(std::vector<DataTypePtr>{LongType()}), channels, true, false, false);
    ASSERT_NE(agg, nullptr);
}

// ---- Complete stage: raw input -> int64 distinct count ----

TEST(CountDistinctAggregatorTest, CompleteIntExtractValues)
{
    CountDistinctAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    auto agg = factory.CreateAggregator(DataTypes(std::vector<DataTypePtr>{IntType()}),
        DataTypes(std::vector<DataTypePtr>{LongType()}), channels, true, false, false);
    ASSERT_NE(agg, nullptr);

    auto executionContext = std::make_unique<ExecutionContext>();
    agg->SetExecutionContext(executionContext.get());

    const int32_t rowCnt = 8;
    int32_t data[rowCnt] = {10, 20, 10, 30, 20, 10, 40, 30};
    VectorBatch *vecBatch = CreateVectorBatch(DataTypes(std::vector<DataTypePtr>{IntType()}), rowCnt, data);
    ASSERT_EQ(vecBatch->GetRowCount(), rowCnt);

    auto state = NewAndInitState(agg.get());
    agg->ProcessGroup(state.get(), vecBatch, 0, rowCnt);

    auto *outputVector = new Vector<int64_t>(1);
    std::vector<BaseVector *> extractVectors = {outputVector};
    agg->ExtractValues(state.get(), extractVectors, 0);
    // {10, 20, 30, 40} -> 4 distinct values.
    EXPECT_EQ(outputVector->GetValue(0), static_cast<int64_t>(4));

    VectorHelper::FreeVecBatch(vecBatch);
    delete outputVector;
    DestroyCountDistinctState(agg.get(), state.get());
}

TEST(CountDistinctAggregatorTest, CompleteIntExtractValuesBatch)
{
    CountDistinctAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    auto agg = factory.CreateAggregator(DataTypes(std::vector<DataTypePtr>{IntType()}),
        DataTypes(std::vector<DataTypePtr>{LongType()}), channels, true, false, false);
    ASSERT_NE(agg, nullptr);

    auto executionContext = std::make_unique<ExecutionContext>();
    agg->SetExecutionContext(executionContext.get());
    agg->SetStateOffset(0);

    // Two groups: group 0 has {1, 1, 2} (2 distinct), group 1 has {5, 6, 6, 7} (3 distinct).
    const int32_t groupCount = 2;
    std::vector<AggregateState *> groupStates(groupCount);
    std::unique_ptr<AggregateState[]> stateBuf =
        std::make_unique<AggregateState[]>(agg->GetStateSize() * groupCount);
    for (int32_t i = 0; i < groupCount; i++) {
        groupStates[i] = stateBuf.get() + i * agg->GetStateSize();
        agg->InitState(groupStates[i]);
    }

    int32_t data0[3] = {1, 1, 2};
    VectorBatch *vb0 = CreateVectorBatch(DataTypes(std::vector<DataTypePtr>{IntType()}), 3, data0);
    agg->ProcessGroup(groupStates[0], vb0, 0, 3);

    int32_t data1[4] = {5, 6, 6, 7};
    VectorBatch *vb1 = CreateVectorBatch(DataTypes(std::vector<DataTypePtr>{IntType()}), 4, data1);
    agg->ProcessGroup(groupStates[1], vb1, 0, 4);

    auto *outputVector = new Vector<int64_t>(groupCount);
    std::vector<BaseVector *> extractVectors = {outputVector};
    agg->ExtractValuesBatch(groupStates, extractVectors, 0, groupCount);
    EXPECT_EQ(outputVector->GetValue(0), static_cast<int64_t>(2));
    EXPECT_EQ(outputVector->GetValue(1), static_cast<int64_t>(3));

    VectorHelper::FreeVecBatch(vb0);
    VectorHelper::FreeVecBatch(vb1);
    delete outputVector;
    for (int32_t i = 0; i < groupCount; i++) {
        DestroyCountDistinctState(agg.get(), groupStates[i]);
    }
}

TEST(CountDistinctAggregatorTest, CompleteEmptyInputCountsZero)
{
    // count(distinct x) over empty input is 0 (count semantics), NOT null (collect_set semantics).
    CountDistinctAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    auto agg = factory.CreateAggregator(DataTypes(std::vector<DataTypePtr>{IntType()}),
        DataTypes(std::vector<DataTypePtr>{LongType()}), channels, true, false, false);
    ASSERT_NE(agg, nullptr);

    auto executionContext = std::make_unique<ExecutionContext>();
    agg->SetExecutionContext(executionContext.get());

    auto state = NewAndInitState(agg.get());

    auto *outputVector = new Vector<int64_t>(1);
    std::vector<BaseVector *> extractVectors = {outputVector};
    agg->ExtractValues(state.get(), extractVectors, 0);
    EXPECT_FALSE(outputVector->IsNull(0));
    EXPECT_EQ(outputVector->GetValue(0), static_cast<int64_t>(0));

    delete outputVector;
    DestroyCountDistinctState(agg.get(), state.get());
}

TEST(CountDistinctAggregatorTest, CompleteAllNullsCountZero)
{
    // NULLs are ignored by count(distinct): all-null input counts 0.
    CountDistinctAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    auto agg = factory.CreateAggregator(DataTypes(std::vector<DataTypePtr>{IntType()}),
        DataTypes(std::vector<DataTypePtr>{LongType()}), channels, true, false, false);
    ASSERT_NE(agg, nullptr);

    auto executionContext = std::make_unique<ExecutionContext>();
    agg->SetExecutionContext(executionContext.get());

    const int32_t rowCnt = 4;
    Vector<int32_t> *col = new Vector<int32_t>(rowCnt);
    col->SetNull(0);
    col->SetNull(1);
    col->SetNull(2);
    col->SetNull(3);
    col->SetNullFlag(true);  // ensure aggregator gets nullMap (nullMap: true = null)
    VectorBatch *vecBatch = new VectorBatch(rowCnt);
    vecBatch->Append(col);

    auto state = NewAndInitState(agg.get());
    agg->ProcessGroup(state.get(), vecBatch, 0, rowCnt);

    auto *outputVector = new Vector<int64_t>(1);
    std::vector<BaseVector *> extractVectors = {outputVector};
    agg->ExtractValues(state.get(), extractVectors, 0);
    EXPECT_EQ(outputVector->GetValue(0), static_cast<int64_t>(0));

    VectorHelper::FreeVecBatch(vecBatch);
    delete outputVector;
    DestroyCountDistinctState(agg.get(), state.get());
}

// ---- Partial stage: raw input -> array<T> distinct set ----

TEST(CountDistinctAggregatorTest, PartialIntExtractValues)
{
    CountDistinctAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    auto agg = factory.CreateAggregator(DataTypes(std::vector<DataTypePtr>{IntType()}),
        DataTypes(std::vector<DataTypePtr>{ArrayOf(IntType())}), channels, true, true, false);
    ASSERT_NE(agg, nullptr);

    auto executionContext = std::make_unique<ExecutionContext>();
    agg->SetExecutionContext(executionContext.get());

    const int32_t rowCnt = 6;
    int32_t data[rowCnt] = {7, 7, 8, 8, 8, 9};
    VectorBatch *vecBatch = CreateVectorBatch(DataTypes(std::vector<DataTypePtr>{IntType()}), rowCnt, data);

    auto state = NewAndInitState(agg.get());
    agg->ProcessGroup(state.get(), vecBatch, 0, rowCnt);

    Vector<int32_t> *outputElements = new Vector<int32_t>(0);
    BaseVector *outputVector = new ArrayVector(1, std::shared_ptr<BaseVector>(outputElements));
    std::vector<BaseVector *> extractVectors = {outputVector};
    agg->ExtractValues(state.get(), extractVectors, 0);

    auto *arrayVec = static_cast<ArrayVector *>(extractVectors[0]);
    std::shared_ptr<BaseVector> elemVec = arrayVec->GetArrayAt(0, false);
    auto *intVec = static_cast<Vector<int32_t> *>(elemVec.get());
    EXPECT_EQ(intVec->GetSize(), static_cast<uint32_t>(3));

    VectorHelper::FreeVecBatch(vecBatch);
    delete outputVector;
    DestroyCountDistinctState(agg.get(), state.get());
}

// ---- Full two-phase flow: Partial output feeds Final input ----

TEST(CountDistinctAggregatorTest, TwoPhasePartialThenFinal)
{
    CountDistinctAggregatorFactory factory;
    std::vector<int32_t> channels = {0};

    // Phase 1 (Partial): raw int -> array<int>.
    auto partialAgg = factory.CreateAggregator(DataTypes(std::vector<DataTypePtr>{IntType()}),
        DataTypes(std::vector<DataTypePtr>{ArrayOf(IntType())}), channels, true, true, false);
    ASSERT_NE(partialAgg, nullptr);
    auto executionContext = std::make_unique<ExecutionContext>();
    partialAgg->SetExecutionContext(executionContext.get());

    const int32_t rowCnt = 5;
    int32_t data[rowCnt] = {1, 2, 2, 3, 3};
    VectorBatch *vecBatch = CreateVectorBatch(DataTypes(std::vector<DataTypePtr>{IntType()}), rowCnt, data);

    auto partialState = NewAndInitState(partialAgg.get());
    partialAgg->ProcessGroup(partialState.get(), vecBatch, 0, rowCnt);

    Vector<int32_t> *outputElements = new Vector<int32_t>(0);
    BaseVector *partialOutput = new ArrayVector(1, std::shared_ptr<BaseVector>(outputElements));
    std::vector<BaseVector *> extractVectors = {partialOutput};
    partialAgg->ExtractValues(partialState.get(), extractVectors, 0);

    // Phase 2 (Final): array<int> -> int64 count.
    auto finalAgg = factory.CreateAggregator(DataTypes(std::vector<DataTypePtr>{ArrayOf(IntType())}),
        DataTypes(std::vector<DataTypePtr>{LongType()}), channels, false, false, false);
    ASSERT_NE(finalAgg, nullptr);
    finalAgg->SetExecutionContext(executionContext.get());

    auto finalState = NewAndInitState(finalAgg.get());
    // Wrap the partial output array into a single-row batch (one group).
    auto *partialBatch = new VectorBatch(1);
    partialBatch->Append(partialOutput);
    finalAgg->ProcessGroup(finalState.get(), partialBatch, 0, 1);

    auto *countVector = new Vector<int64_t>(1);
    std::vector<BaseVector *> finalVectors = {countVector};
    finalAgg->ExtractValues(finalState.get(), finalVectors, 0);
    // {1, 2, 3} -> 3 distinct.
    EXPECT_EQ(countVector->GetValue(0), static_cast<int64_t>(3));

    VectorHelper::FreeVecBatch(vecBatch);
    delete partialBatch;
    delete countVector;
    DestroyCountDistinctState(partialAgg.get(), partialState.get());
    DestroyCountDistinctState(finalAgg.get(), finalState.get());
}

// ---- VARCHAR variant ----

TEST(CountDistinctAggregatorTest, CompleteVarcharExtractValues)
{
    CountDistinctAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    auto agg = factory.CreateAggregator(DataTypes(std::vector<DataTypePtr>{VarcharType()}),
        DataTypes(std::vector<DataTypePtr>{LongType()}), channels, true, false, false);
    ASSERT_NE(agg, nullptr);

    auto executionContext = std::make_unique<ExecutionContext>();
    agg->SetExecutionContext(executionContext.get());

    const int32_t rowCnt = 6;
    std::string values[rowCnt] = {"a", "b", "a", "c", "b", "a"};
    VectorBatch *vecBatch = CreateVectorBatch(DataTypes(std::vector<DataTypePtr>{VarcharType(100)}), rowCnt, values);
    ASSERT_EQ(vecBatch->GetRowCount(), rowCnt);

    auto state = NewAndInitState(agg.get());
    agg->ProcessGroup(state.get(), vecBatch, 0, rowCnt);

    auto *outputVector = new Vector<int64_t>(1);
    std::vector<BaseVector *> extractVectors = {outputVector};
    agg->ExtractValues(state.get(), extractVectors, 0);
    // {"a", "b", "c"} -> 3 distinct.
    EXPECT_EQ(outputVector->GetValue(0), static_cast<int64_t>(3));

    VectorHelper::FreeVecBatch(vecBatch);
    delete outputVector;
    DestroyCountDistinctState(agg.get(), state.get());
}

// ---- GetSpillType ----

TEST(CountDistinctAggregatorTest, GetSpillTypeIsAlwaysArray)
{
    CountDistinctAggregatorFactory factory;
    std::vector<int32_t> channels = {0};

    // Complete (raw in / long out): spill type derives array<int> from the raw element type.
    auto completeAgg = factory.CreateAggregator(DataTypes(std::vector<DataTypePtr>{IntType()}),
        DataTypes(std::vector<DataTypePtr>{LongType()}), channels, true, false, false);
    ASSERT_NE(completeAgg, nullptr);
    auto spillTypes = completeAgg->GetSpillType();
    ASSERT_EQ(spillTypes.size(), static_cast<size_t>(1));
    EXPECT_EQ(spillTypes[0]->GetId(), OMNI_ARRAY);

    // Partial (raw in / array out): spill type is the output array type.
    auto partialAgg = factory.CreateAggregator(DataTypes(std::vector<DataTypePtr>{IntType()}),
        DataTypes(std::vector<DataTypePtr>{ArrayOf(IntType())}), channels, true, true, false);
    ASSERT_NE(partialAgg, nullptr);
    spillTypes = partialAgg->GetSpillType();
    ASSERT_EQ(spillTypes.size(), static_cast<size_t>(1));
    EXPECT_EQ(spillTypes[0]->GetId(), OMNI_ARRAY);
}

// ---- ExtractValuesForSpill: spill extraction must emit array<T> even in complete/final mode ----
// Regression: the inherited CollectSet ExtractValuesForSpill virtually dispatches to
// ExtractValuesBatch, which in complete mode emits int64 — writing a Long into the
// array-typed spill vector (type confusion / UB). The override must emit the set itself.

TEST(CountDistinctAggregatorTest, CompleteIntExtractValuesForSpillEmitsArray)
{
    CountDistinctAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    auto agg = factory.CreateAggregator(DataTypes(std::vector<DataTypePtr>{IntType()}),
        DataTypes(std::vector<DataTypePtr>{LongType()}), channels, true, false, false);
    ASSERT_NE(agg, nullptr);

    auto executionContext = std::make_unique<ExecutionContext>();
    agg->SetExecutionContext(executionContext.get());
    agg->SetStateOffset(0);

    // Two groups: {1, 1, 2} (2 distinct) and {5, 6, 6, 7} (3 distinct).
    const int32_t groupCount = 2;
    std::vector<AggregateState *> groupStates(groupCount);
    std::unique_ptr<AggregateState[]> stateBuf =
        std::make_unique<AggregateState[]>(agg->GetStateSize() * groupCount);
    for (int32_t i = 0; i < groupCount; i++) {
        groupStates[i] = stateBuf.get() + i * agg->GetStateSize();
        agg->InitState(groupStates[i]);
    }

    int32_t data0[3] = {1, 1, 2};
    VectorBatch *vb0 = CreateVectorBatch(DataTypes(std::vector<DataTypePtr>{IntType()}), 3, data0);
    agg->ProcessGroup(groupStates[0], vb0, 0, 3);

    int32_t data1[4] = {5, 6, 6, 7};
    VectorBatch *vb1 = CreateVectorBatch(DataTypes(std::vector<DataTypePtr>{IntType()}), 4, data1);
    agg->ProcessGroup(groupStates[1], vb1, 0, 4);

    // Spill vectors are allocated per GetSpillType(): one array<int> column.
    auto spillTypes = agg->GetSpillType();
    ASSERT_EQ(spillTypes.size(), static_cast<size_t>(1));
    EXPECT_EQ(spillTypes[0]->GetId(), OMNI_ARRAY);

    Vector<int32_t> *spillElements = new Vector<int32_t>(0);
    BaseVector *spillVector = new ArrayVector(groupCount, std::shared_ptr<BaseVector>(spillElements));
    std::vector<BaseVector *> spillVectors = {spillVector};
    agg->ExtractValuesForSpill(groupStates, spillVectors);

    // Each spilled row must be the distinct set itself, NOT a Long count.
    auto *arrayVec = static_cast<ArrayVector *>(spillVectors[0]);
    auto elem0 = arrayVec->GetArrayAt(0, false);
    EXPECT_EQ(elem0->GetSize(), static_cast<uint32_t>(2)); // {1, 2}
    auto elem1 = arrayVec->GetArrayAt(1, false);
    EXPECT_EQ(elem1->GetSize(), static_cast<uint32_t>(3)); // {5, 6, 7}

    VectorHelper::FreeVecBatch(vb0);
    VectorHelper::FreeVecBatch(vb1);
    delete spillVector;
    for (int32_t i = 0; i < groupCount; i++) {
        DestroyCountDistinctState(agg.get(), groupStates[i]);
    }
}

TEST(CountDistinctAggregatorTest, CompleteVarcharExtractValuesForSpillEmitsArray)
{
    CountDistinctAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    auto agg = factory.CreateAggregator(DataTypes(std::vector<DataTypePtr>{VarcharType()}),
        DataTypes(std::vector<DataTypePtr>{LongType()}), channels, true, false, false);
    ASSERT_NE(agg, nullptr);

    auto executionContext = std::make_unique<ExecutionContext>();
    agg->SetExecutionContext(executionContext.get());

    const int32_t rowCnt = 4;
    std::string values[rowCnt] = {"x", "y", "x", "z"};
    VectorBatch *vecBatch = CreateVectorBatch(DataTypes(std::vector<DataTypePtr>{VarcharType(100)}), rowCnt, values);

    auto state = NewAndInitState(agg.get());
    agg->ProcessGroup(state.get(), vecBatch, 0, rowCnt);

    // Spill column per GetSpillType(): array<varchar>.
    auto spillTypes = agg->GetSpillType();
    ASSERT_EQ(spillTypes.size(), static_cast<size_t>(1));
    EXPECT_EQ(spillTypes[0]->GetId(), OMNI_ARRAY);

    auto *spillElements = static_cast<Vector<LargeStringContainer<std::string_view>> *>(
        VectorHelper::CreateVector(OMNI_FLAT, OMNI_VARCHAR, 0));
    BaseVector *spillVector = new ArrayVector(1, std::shared_ptr<BaseVector>(spillElements));
    std::vector<BaseVector *> spillVectors = {spillVector};
    std::vector<AggregateState *> groupStates = {state.get()};
    agg->ExtractValuesForSpill(groupStates, spillVectors);

    // The spilled row must be the distinct set {"x", "y", "z"} (3 elements), not a Long.
    auto *arrayVec = static_cast<ArrayVector *>(spillVectors[0]);
    auto elem = arrayVec->GetArrayAt(0, false);
    EXPECT_EQ(elem->GetSize(), static_cast<uint32_t>(3));

    VectorHelper::FreeVecBatch(vecBatch);
    delete spillVector;
    DestroyCountDistinctState(agg.get(), state.get());
}

// ---- State destruction: spill lifecycle ----
// After a spill the aggregation operator calls DestroyAllStates() and then reuses
// the same state buffers for the next round. Verifies the address book is cleared
// and states can be re-initialized without dangling pointers.

TEST(CountDistinctAggregatorTest, DestroyAllStatesThenReuse)
{
    CountDistinctAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    auto agg = factory.CreateAggregator(DataTypes(std::vector<DataTypePtr>{IntType()}),
        DataTypes(std::vector<DataTypePtr>{LongType()}), channels, true, false, false);
    ASSERT_NE(agg, nullptr);
    auto executionContext = std::make_unique<ExecutionContext>();
    agg->SetExecutionContext(executionContext.get());
    agg->SetStateOffset(0);

    const int32_t groupCount = 3;
    std::vector<AggregateState *> groupStates(groupCount);
    std::unique_ptr<AggregateState[]> stateBuf =
        std::make_unique<AggregateState[]>(agg->GetStateSize() * groupCount);
    for (int32_t i = 0; i < groupCount; i++) {
        groupStates[i] = stateBuf.get() + i * agg->GetStateSize();
    }

    // Round 1: {1,1,2}/{5,6,6,7}/{9} then destroy all heap states at once.
    agg->InitStates(groupStates);
    int32_t data0[3] = {1, 1, 2};
    VectorBatch *vb0 = CreateVectorBatch(DataTypes(std::vector<DataTypePtr>{IntType()}), 3, data0);
    agg->ProcessGroup(groupStates[0], vb0, 0, 3);
    int32_t data1[4] = {5, 6, 6, 7};
    VectorBatch *vb1 = CreateVectorBatch(DataTypes(std::vector<DataTypePtr>{IntType()}), 4, data1);
    agg->ProcessGroup(groupStates[1], vb1, 0, 4);
    int32_t data2[1] = {9};
    VectorBatch *vb2 = CreateVectorBatch(DataTypes(std::vector<DataTypePtr>{IntType()}), 1, data2);
    agg->ProcessGroup(groupStates[2], vb2, 0, 1);

    agg->DestroyAllStates();

    // Round 2: re-init the same buffers and aggregate fresh data.
    agg->InitStates(groupStates);
    int32_t data3[3] = {10, 10, 20};
    VectorBatch *vb3 = CreateVectorBatch(DataTypes(std::vector<DataTypePtr>{IntType()}), 3, data3);
    agg->ProcessGroup(groupStates[0], vb3, 0, 3);
    int32_t data4[1] = {30};
    VectorBatch *vb4 = CreateVectorBatch(DataTypes(std::vector<DataTypePtr>{IntType()}), 1, data4);
    agg->ProcessGroup(groupStates[1], vb4, 0, 1);
    int32_t data5[2] = {40, 40};
    VectorBatch *vb5 = CreateVectorBatch(DataTypes(std::vector<DataTypePtr>{IntType()}), 2, data5);
    agg->ProcessGroup(groupStates[2], vb5, 0, 2);

    auto *outputVector = new Vector<int64_t>(groupCount);
    std::vector<BaseVector *> extractVectors = {outputVector};
    agg->ExtractValuesBatch(groupStates, extractVectors, 0, groupCount);
    EXPECT_EQ(outputVector->GetValue(0), static_cast<int64_t>(2)); // {10, 20}
    EXPECT_EQ(outputVector->GetValue(1), static_cast<int64_t>(1)); // {30}
    EXPECT_EQ(outputVector->GetValue(2), static_cast<int64_t>(1)); // {40}

    for (auto *vb : std::vector<VectorBatch *>{vb0, vb1, vb2, vb3, vb4, vb5}) {
        VectorHelper::FreeVecBatch(vb);
    }
    delete outputVector;
    for (int32_t i = 0; i < groupCount; i++) {
        DestroyCountDistinctState(agg.get(), groupStates[i]);
    }
}

// Multi-round InitStates/DestroyAllStates must keep the thread memory counters
// balanced: every reported object header is reclaimed, so accumulated alloc and
// free deltas stay equal across rounds (no leak growth per round).
TEST(CountDistinctAggregatorTest, MultiRoundDestroyAllStatesMemoryBalanced)
{
    CountDistinctAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    auto agg = factory.CreateAggregator(DataTypes(std::vector<DataTypePtr>{IntType()}),
        DataTypes(std::vector<DataTypePtr>{LongType()}), channels, true, false, false);
    ASSERT_NE(agg, nullptr);
    auto executionContext = std::make_unique<ExecutionContext>();
    agg->SetExecutionContext(executionContext.get());
    agg->SetStateOffset(0);

    const int32_t groups = 5000;
    const int32_t rounds = 3;
    std::vector<AggregateState *> groupStates(groups);
    std::unique_ptr<AggregateState[]> stateBuf =
        std::make_unique<AggregateState[]>(agg->GetStateSize() * groups);
    for (int32_t i = 0; i < groups; i++) {
        groupStates[i] = stateBuf.get() + i * agg->GetStateSize();
    }

    auto *tmm = mem::ThreadMemoryManager::GetThreadMemoryManager();
    auto allocBefore = tmm->GetAllocMemory();
    auto freeBefore = tmm->GetFreeMemory();
    for (int32_t r = 0; r < rounds; r++) {
        agg->InitStates(groupStates);
        agg->DestroyAllStates();
    }
    auto allocDelta = tmm->GetAllocMemory() - allocBefore;
    auto freeDelta = tmm->GetFreeMemory() - freeBefore;
    EXPECT_GT(allocDelta, 0);          // object headers are accounted
    EXPECT_EQ(allocDelta, freeDelta); // ... and fully reclaimed every round
}

// State destruction must stay O(n) in the number of groups (hash-set address
// book). The old std::vector + std::find bookkeeping made per-state DestroyState
// O(n) each -> O(n^2) total, which hung the engine for tens of millions of
// groups during multi-round spill. Bounds are generous for loaded CI machines
// while still failing fast on a quadratic regression.
TEST(CountDistinctAggregatorTest, StateDestroyScalesLinearly)
{
    CountDistinctAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    auto agg = factory.CreateAggregator(DataTypes(std::vector<DataTypePtr>{IntType()}),
        DataTypes(std::vector<DataTypePtr>{LongType()}), channels, true, false, false);
    ASSERT_NE(agg, nullptr);
    auto executionContext = std::make_unique<ExecutionContext>();
    agg->SetExecutionContext(executionContext.get());
    agg->SetStateOffset(0);

    const int32_t groups = 100000;
    std::vector<AggregateState *> groupStates(groups);
    std::unique_ptr<AggregateState[]> stateBuf =
        std::make_unique<AggregateState[]>(agg->GetStateSize() * groups);
    for (int32_t i = 0; i < groups; i++) {
        groupStates[i] = stateBuf.get() + i * agg->GetStateSize();
    }

    // Bulk destroy (spill path).
    agg->InitStates(groupStates);
    auto start = std::chrono::steady_clock::now();
    agg->DestroyAllStates();
    auto bulkMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();

    // Per-state destroy (unspill output path: one DestroyState per group).
    agg->InitStates(groupStates);
    start = std::chrono::steady_clock::now();
    for (int32_t i = 0; i < groups; i++) {
        DestroyCountDistinctState(agg.get(), groupStates[i]);
    }
    auto perStateMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();

    EXPECT_LT(bulkMs, 30000);
    EXPECT_LT(perStateMs, 30000);
}
} // namespace omniruntime
