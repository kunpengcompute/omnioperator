/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: HashAgg group-path tests for Spark sum(bigint) (SVE fast path when available)
 */
#include <gtest/gtest.h>
#include <memory>
#include <vector>

#include "operator/aggregation/aggregator/aggregator_factory.h"
#include "operator/aggregation/aggregator/aggregator_util.h"
#include "operator/aggregation/aggregator/sum_int64_sve_hash_agg.h"
#include "operator/execution_context.h"
#include "type/data_type.h"
#include "vector/vector.h"
#include "vector/vector_helper.h"

namespace omniruntime {
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::vec;

namespace {
std::unique_ptr<Aggregator> MakeSparkSumLong(bool inputRaw = true, bool outputPartial = true)
{
    SumSparkAggregatorFactory factory;
    std::vector<int32_t> channels = {0};
    auto agg = factory.CreateAggregator(*AggregatorUtil::WrapWithDataTypes(LongType()).get(),
        *AggregatorUtil::WrapWithDataTypes(LongType()).get(), channels, inputRaw, outputPartial);
    return agg;
}

struct GroupStateBundle {
    std::unique_ptr<AggregateState[]> buf;
    std::vector<AggregateState *> groups;
};

GroupStateBundle MakeGroups(Aggregator *agg, int32_t groupCount)
{
    GroupStateBundle b;
    b.buf = std::make_unique<AggregateState[]>(agg->GetStateSize() * static_cast<size_t>(groupCount));
    b.groups.resize(static_cast<size_t>(groupCount));
    for (int32_t i = 0; i < groupCount; ++i) {
        b.groups[static_cast<size_t>(i)] = b.buf.get() + static_cast<size_t>(i) * agg->GetStateSize();
    }
    agg->SetStateOffset(0);
    agg->InitStates(b.groups);
    return b;
}
} // namespace

TEST(SparkSumLongHashAggTest, FlatWithNullsAndDuplicateGroups)
{
    auto executionContext = std::make_unique<ExecutionContext>();
    auto agg = MakeSparkSumLong();
    agg->SetExecutionContext(executionContext.get());

    // 2 groups; rows map as [g0, g1, g0, g1, null→g0] with duplicate groups in a 4-lane window.
    constexpr int32_t rowCount = 5;
    auto groups = MakeGroups(agg.get(), 2);
    std::vector<AggregateState *> rowStates = {
        groups.groups[0], groups.groups[1], groups.groups[0], groups.groups[1], groups.groups[0]};

    auto *col = new Vector<int64_t>(rowCount);
    col->SetValue(0, 10);
    col->SetValue(1, 20);
    col->SetValue(2, 30);
    col->SetValue(3, 40);
    col->SetNull(4);

    auto *vecBatch = new VectorBatch(rowCount);
    vecBatch->Append(col);

    agg->ProcessGroup(rowStates, vecBatch, 0);

    auto *out0 = new Vector<int64_t>(1);
    auto *out1 = new Vector<int64_t>(1);
    std::vector<BaseVector *> extract0 = {out0};
    std::vector<BaseVector *> extract1 = {out1};
    agg->ExtractValues(groups.groups[0], extract0, 0);
    agg->ExtractValues(groups.groups[1], extract1, 0);

    EXPECT_FALSE(out0->IsNull(0));
    EXPECT_FALSE(out1->IsNull(0));
    EXPECT_EQ(40, out0->GetValue(0)); // 10+30, null skipped
    EXPECT_EQ(60, out1->GetValue(0)); // 20+40

    VectorHelper::FreeVecBatch(vecBatch);
    delete out0;
    delete out1;
}

TEST(SparkSumLongHashAggTest, ConstantInput)
{
    auto executionContext = std::make_unique<ExecutionContext>();
    auto agg = MakeSparkSumLong();
    agg->SetExecutionContext(executionContext.get());

    constexpr int32_t rowCount = 4;
    auto groups = MakeGroups(agg.get(), 2);
    std::vector<AggregateState *> rowStates = {
        groups.groups[0], groups.groups[1], groups.groups[0], groups.groups[1]};

    auto *col = new ConstVector<int64_t>(7, OMNI_LONG, rowCount);
    auto *vecBatch = new VectorBatch(rowCount);
    vecBatch->Append(col);

    agg->ProcessGroup(rowStates, vecBatch, 0);

    auto *out0 = new Vector<int64_t>(1);
    auto *out1 = new Vector<int64_t>(1);
    std::vector<BaseVector *> extract0 = {out0};
    std::vector<BaseVector *> extract1 = {out1};
    agg->ExtractValues(groups.groups[0], extract0, 0);
    agg->ExtractValues(groups.groups[1], extract1, 0);

    EXPECT_EQ(14, out0->GetValue(0));
    EXPECT_EQ(14, out1->GetValue(0));

    VectorHelper::FreeVecBatch(vecBatch);
    delete out0;
    delete out1;
}

TEST(SparkSumLongHashAggTest, DictionaryInput)
{
    auto executionContext = std::make_unique<ExecutionContext>();
    auto agg = MakeSparkSumLong();
    agg->SetExecutionContext(executionContext.get());

    constexpr int32_t rowCount = 4;
    auto groups = MakeGroups(agg.get(), 2);
    std::vector<AggregateState *> rowStates = {
        groups.groups[0], groups.groups[0], groups.groups[1], groups.groups[1]};

    auto *dictSource = new Vector<int64_t>(3);
    dictSource->SetValue(0, 100);
    dictSource->SetValue(1, 200);
    dictSource->SetValue(2, 300);
    int32_t ids[4] = {0, 1, 2, 0};
    auto *dictVec = VectorHelper::CreateDictionary(ids, rowCount, dictSource);

    auto *vecBatch = new VectorBatch(rowCount);
    vecBatch->Append(dictVec);

    agg->ProcessGroup(rowStates, vecBatch, 0);

    auto *out0 = new Vector<int64_t>(1);
    auto *out1 = new Vector<int64_t>(1);
    std::vector<BaseVector *> extract0 = {out0};
    std::vector<BaseVector *> extract1 = {out1};
    agg->ExtractValues(groups.groups[0], extract0, 0);
    agg->ExtractValues(groups.groups[1], extract1, 0);

    EXPECT_FALSE(out0->IsNull(0));
    EXPECT_FALSE(out1->IsNull(0));
    EXPECT_EQ(300, out0->GetValue(0)); // 100+200
    EXPECT_EQ(400, out1->GetValue(0)); // 300+100

    VectorHelper::FreeVecBatch(vecBatch);
    delete dictSource;
    delete out0;
    delete out1;
}

TEST(SparkSumLongHashAggTest, WrappingOverflowAcrossGroups)
{
    auto executionContext = std::make_unique<ExecutionContext>();
    auto agg = MakeSparkSumLong();
    agg->SetExecutionContext(executionContext.get());

    constexpr int32_t rowCount = 3;
    auto groups = MakeGroups(agg.get(), 1);
    std::vector<AggregateState *> rowStates = {groups.groups[0], groups.groups[0], groups.groups[0]};

    auto *col = new Vector<int64_t>(rowCount);
    col->SetValue(0, 9223372036854774807LL);
    col->SetValue(1, 9223372036854774807LL);
    col->SetValue(2, 9223372036854774807LL);

    auto *vecBatch = new VectorBatch(rowCount);
    vecBatch->Append(col);

    agg->ProcessGroup(rowStates, vecBatch, 0);

    auto *out = new Vector<int64_t>(1);
    std::vector<BaseVector *> extract = {out};
    agg->ExtractValues(groups.groups[0], extract, 0);

    // Same wrapping result as spark_sum_long_overflow (3x near-max).
    EXPECT_FALSE(out->IsNull(0));
    EXPECT_EQ(9223372036854772805LL, out->GetValue(0));

    VectorHelper::FreeVecBatch(vecBatch);
    delete out;
}

TEST(SparkSumLongHashAggTest, FinalStagePartialSums)
{
    auto executionContext = std::make_unique<ExecutionContext>();
    auto agg = MakeSparkSumLong(false, false);
    agg->SetExecutionContext(executionContext.get());

    constexpr int32_t rowCount = 3;
    auto groups = MakeGroups(agg.get(), 2);
    std::vector<AggregateState *> rowStates = {groups.groups[0], groups.groups[1], groups.groups[0]};

    auto *col = new Vector<int64_t>(rowCount);
    col->SetValue(0, 11);
    col->SetValue(1, 22);
    col->SetValue(2, 33);

    auto *vecBatch = new VectorBatch(rowCount);
    vecBatch->Append(col);

    agg->ProcessGroup(rowStates, vecBatch, 0);

    auto *out0 = new Vector<int64_t>(1);
    auto *out1 = new Vector<int64_t>(1);
    std::vector<BaseVector *> extract0 = {out0};
    std::vector<BaseVector *> extract1 = {out1};
    agg->ExtractValues(groups.groups[0], extract0, 0);
    agg->ExtractValues(groups.groups[1], extract1, 0);

    EXPECT_EQ(44, out0->GetValue(0));
    EXPECT_EQ(22, out1->GetValue(0));

    VectorHelper::FreeVecBatch(vecBatch);
    delete out0;
    delete out1;
}

TEST(SparkSumLongHashAggTest, LargeBatchWithNulls)
{
    auto executionContext = std::make_unique<ExecutionContext>();
    auto agg = MakeSparkSumLong();
    agg->SetExecutionContext(executionContext.get());

    constexpr int32_t rowCount = 64;
    constexpr int32_t groupCount = 2;
    auto groups = MakeGroups(agg.get(), groupCount);
    std::vector<AggregateState *> rowStates(rowCount);
    auto *col = new Vector<int64_t>(rowCount);
    int64_t expect[groupCount] = {0, 0};
    for (int32_t i = 0; i < rowCount; ++i) {
        const int32_t g = i % groupCount;
        rowStates[static_cast<size_t>(i)] = groups.groups[static_cast<size_t>(g)];
        if (i % 5 == 0) {
            col->SetNull(i);
            continue;
        }
        const int64_t v = i + 1;
        col->SetValue(i, v);
        expect[g] += v;
    }

    auto *vecBatch = new VectorBatch(rowCount);
    vecBatch->Append(col);
    agg->ProcessGroup(rowStates, vecBatch, 0);

    for (int32_t g = 0; g < groupCount; ++g) {
        auto *out = new Vector<int64_t>(1);
        std::vector<BaseVector *> extract = {out};
        agg->ExtractValues(groups.groups[static_cast<size_t>(g)], extract, 0);
        EXPECT_EQ(expect[g], out->GetValue(0)) << "group " << g;
        delete out;
    }

    VectorHelper::FreeVecBatch(vecBatch);
}

TEST(SparkSumLongHashAggTest, LargeBatchFlatManyGroups)
{
    auto executionContext = std::make_unique<ExecutionContext>();
    auto agg = MakeSparkSumLong();
    agg->SetExecutionContext(executionContext.get());

    constexpr int32_t rowCount = 64;
    constexpr int32_t groupCount = 4;
    auto groups = MakeGroups(agg.get(), groupCount);
    std::vector<AggregateState *> rowStates(rowCount);
    auto *col = new Vector<int64_t>(rowCount);
    int64_t expect[groupCount] = {0, 0, 0, 0};
    for (int32_t i = 0; i < rowCount; ++i) {
        const int32_t g = i % groupCount;
        rowStates[static_cast<size_t>(i)] = groups.groups[static_cast<size_t>(g)];
        const int64_t v = i + 1;
        col->SetValue(i, v);
        expect[g] += v;
    }

    auto *vecBatch = new VectorBatch(rowCount);
    vecBatch->Append(col);
    agg->ProcessGroup(rowStates, vecBatch, 0);

    for (int32_t g = 0; g < groupCount; ++g) {
        auto *out = new Vector<int64_t>(1);
        std::vector<BaseVector *> extract = {out};
        agg->ExtractValues(groups.groups[static_cast<size_t>(g)], extract, 0);
        EXPECT_EQ(expect[g], out->GetValue(0)) << "group " << g;
        delete out;
    }

    VectorHelper::FreeVecBatch(vecBatch);
}

TEST(SparkSumLongHashAggTest, CanUseSveGateIsStable)
{
    // Gate must be side-effect free and consistent within a process.
    const bool a = CanUseSveHashAggSumInt64();
    const bool b = CanUseSveHashAggSumInt64();
    EXPECT_EQ(a, b);
}
} // namespace omniruntime
