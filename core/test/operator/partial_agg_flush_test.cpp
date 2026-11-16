/*
 * Memory-driven partial aggregation flush (Velox HashAggregation parity).
 *
 * An adaptive (K_PARTIAL) hash aggregation whose table exceeds the configured
 * cap stops taking input, emits the table downstream, resets and continues.
 * The final aggregation absorbs the duplicates, so the result is unchanged;
 * what changes is that the partial table never grows past the cap, so it never
 * has to be spilled to disk when the task's memory share runs out.
 */
#include <gtest/gtest.h>
#include <algorithm>
#include <unordered_map>
#include <string>
#include <vector>
#include "operator/aggregation/aggregation.h"
#include "operator/aggregation/aggregator/aggregator_util.h"
#include "operator/aggregation/group_aggregation.h"
#include "operator/aggregation/group_aggregation_expr.h"
#include "vector/vector_helper.h"
#include "util/test_util.h"
#include "util/config_util.h"
#include "util/type_util.h"
#include "util/test_pool_fixture.h"

namespace omniruntime {
using namespace omniruntime::vec;
using namespace omniruntime::op;
using namespace TestUtil;
using namespace omniruntime::expressions;
using omniruntime::test::TestPoolFixture;

namespace {
// sum(long) group by one key column (long or varchar).
std::unique_ptr<HashAggregationOperatorFactory> MakeSumFactory(const DataTypes &groupType, AggregationNode::Step step,
    bool inputRaw, bool outputPartial, const OperatorConfig &operatorConfig = OperatorConfig())
{
    std::vector<uint32_t> groupByCols{0};
    std::vector<std::vector<uint32_t>> aggCols{{1}};
    std::vector<DataTypes> aggInputTypes = AggregatorUtil::WrapWithVector(DataTypes(std::vector<DataTypePtr>{LongType()}));
    std::vector<DataTypes> aggOutputTypes =
        AggregatorUtil::WrapWithVector(DataTypes(std::vector<DataTypePtr>{LongType()}));
    std::vector<uint32_t> aggFuncTypes{OMNI_AGGREGATION_TYPE_SUM};
    std::vector<uint32_t> maskCols{static_cast<uint32_t>(-1)};
    std::vector<bool> inputRaws{inputRaw};
    std::vector<bool> outputPartials{outputPartial};
    std::vector<int8_t> hasAggFilters{0};
    auto factory = std::make_unique<HashAggregationOperatorFactory>(groupByCols, groupType, aggCols, aggInputTypes,
        aggOutputTypes, aggFuncTypes, maskCols, inputRaws, outputPartials, hasAggFilters, operatorConfig, step);
    factory->Init();
    return factory;
}

// The driver only pulls from an operator whose downstream needs input and only
// feeds an operator that needsInput(): while a flush is pending the operator
// refuses input and the driver drains it.
void Pump(op::Operator *op, std::vector<VectorBatch *> &outs)
{
    while (!op->needsInput() && !op->isFinished()) {
        VectorBatch *out = nullptr;
        op->GetOutput(&out);
        if (out == nullptr) {
            break;
        }
        outs.push_back(out);
    }
}

// Through the wrapper the driver pulls after every AddInput (it steps to the
// downstream operator before feeding the next batch), which is what carries an
// abandoned partial aggregation's pass-through batches out one by one.
void PumpWrapper(op::Operator *op, std::vector<VectorBatch *> &outs)
{
    while (!op->isFinished()) {
        VectorBatch *out = nullptr;
        op->GetOutput(&out);
        if (out == nullptr) {
            break;
        }
        outs.push_back(out);
    }
}

void Drain(op::Operator *op, std::vector<VectorBatch *> &outs)
{
    op->noMoreInput();
    while (!op->isFinished()) {
        VectorBatch *out = nullptr;
        op->GetOutput(&out);
        if (out == nullptr) {
            break;
        }
        outs.push_back(out);
    }
}

uint64_t TotalRows(const std::vector<VectorBatch *> &batches)
{
    uint64_t rows = 0;
    for (auto *b : batches) {
        rows += static_cast<uint64_t>(b->GetRowCount());
    }
    return rows;
}

// Feed partial output into a final sum and return key -> sum.
template <typename KeyGetter>
std::unordered_map<std::string, int64_t> FinalSums(const DataTypes &groupType, std::vector<VectorBatch *> &partialOuts,
    KeyGetter keyOf)
{
    auto finalFactory = MakeSumFactory(groupType, AggregationNode::Step::K_FINAL, false, false);
    auto *finalOp = finalFactory->CreateOperator(nullptr);
    finalOp->Init();
    finalOp->setNoMoreInput(false); // the driver opens every operator for input
    for (auto *b : partialOuts) {
        finalOp->AddInput(b);
    }
    partialOuts.clear();
    std::vector<VectorBatch *> outs;
    Drain(finalOp, outs);
    std::unordered_map<std::string, int64_t> sums;
    for (auto *b : outs) {
        auto *sumVec = static_cast<Vector<int64_t> *>(b->Get(1));
        for (int32_t i = 0; i < b->GetRowCount(); ++i) {
            sums[keyOf(b->Get(0), i)] += sumVec->GetValue(i);
        }
        VectorHelper::FreeVecBatch(b);
    }
    op::Operator::DeleteOperator(finalOp);
    return sums;
}

std::string LongKey(BaseVector *v, int32_t i)
{
    return std::to_string(static_cast<Vector<int64_t> *>(v)->GetValue(i));
}

std::string VarcharKey(BaseVector *v, int32_t i)
{
    auto sv = static_cast<Vector<LargeStringContainer<std::string_view>> *>(v)->GetValue(i);
    return std::string(sv.data(), sv.size());
}
} // namespace

// 25 % reduction per batch: the flush keeps going (no abandon) and the final
// result equals the un-flushed aggregation.
TEST(PartialAggFlushTest, flush_keeps_result_long_keys)
{
    ConfigUtil::SetSupportContainerVecRule(SupportContainerVecRule::NOT_SUPPORT);
    constexpr int32_t kKeys = 1000;
    constexpr int32_t kDupsPerBatch = 4;
    constexpr int32_t kRows = kKeys * kDupsPerBatch;
    constexpr int32_t kBatches = 20;
    DataTypes groupType(std::vector<DataTypePtr>{LongType()});
    DataTypes sourceTypes(std::vector<DataTypePtr>{LongType(), LongType()});

    auto factory = MakeSumFactory(groupType, AggregationNode::Step::K_PARTIAL, true, true);
    auto *op = static_cast<HashAggregationOperator *>(factory->CreateOperator(nullptr));
    op->Init();
    op->setNoMoreInput(false);
    ASSERT_TRUE(op->IsStepPartials());
    op->SetPartialFlushMemory(8 * 1024, 16 * 1024);

    std::vector<VectorBatch *> outs;
    uint64_t rowsBeforeEnd = 0;
    for (int32_t b = 0; b < kBatches; ++b) {
        int64_t keys[kRows];
        int64_t vals[kRows];
        for (int32_t i = 0; i < kRows; ++i) {
            keys[i] = i % kKeys;
            vals[i] = keys[i];
        }
        VectorBatch *in = CreateVectorBatch(sourceTypes, kRows, keys, vals);
        EXPECT_TRUE(op->needsInput());
        op->AddInput(in);
        Pump(op, outs);
        rowsBeforeEnd = TotalRows(outs);
    }
    EXPECT_GE(op->PartialFlushTimes(), 1u);
    EXPECT_GT(rowsBeforeEnd, 0u) << "a flush must emit rows before noMoreInput";
    Drain(op, outs);
    EXPECT_GT(TotalRows(outs), static_cast<uint64_t>(kKeys)) << "flushed duplicates reach the final stage";
    op::Operator::DeleteOperator(op);

    auto sums = FinalSums(groupType, outs, LongKey);
    ASSERT_EQ(sums.size(), static_cast<size_t>(kKeys));
    for (int32_t k = 0; k < kKeys; ++k) {
        EXPECT_EQ(sums[std::to_string(k)], static_cast<int64_t>(k) * kDupsPerBatch * kBatches) << "key " << k;
    }
}

// A table larger than one output batch flushes over several GetOutput calls.
// The operator must refuse input until the last batch of the flush is out:
// input that resumed after the first batch would land in a table that is only
// partly emitted, and the groups already emitted would lose it.
TEST(PartialAggFlushTest, flush_spanning_several_output_batches_refuses_input_until_done)
{
    ConfigUtil::SetSupportContainerVecRule(SupportContainerVecRule::NOT_SUPPORT);
    constexpr int32_t kKeys = 300000;
    constexpr int32_t kBatches = 3;
    DataTypes groupType(std::vector<DataTypePtr>{LongType()});
    DataTypes sourceTypes(std::vector<DataTypePtr>{LongType(), LongType()});

    auto factory = MakeSumFactory(groupType, AggregationNode::Step::K_PARTIAL, true, true);
    auto *op = static_cast<HashAggregationOperator *>(factory->CreateOperator(nullptr));
    op->Init();
    op->setNoMoreInput(false);
    op->SetPartialFlushMemory(8 * 1024, 16 * 1024);

    std::vector<VectorBatch *> outs;
    std::vector<int64_t> keys(kKeys);
    std::vector<int64_t> vals(kKeys);
    for (int32_t i = 0; i < kKeys; ++i) {
        keys[i] = i;
        vals[i] = i + 1;
    }
    int32_t maxBatchesPerFlush = 0;
    for (int32_t b = 0; b < kBatches; ++b) {
        VectorBatch *in = CreateVectorBatch(sourceTypes, kKeys, keys.data(), vals.data());
        ASSERT_TRUE(op->needsInput());
        op->AddInput(in);
        ASSERT_FALSE(op->needsInput()) << "the table is far above its cap";
        uint64_t flushedRows = 0;
        int32_t flushBatches = 0;
        while (!op->needsInput()) {
            VectorBatch *out = nullptr;
            op->GetOutput(&out);
            if (out == nullptr) {
                break;
            }
            flushedRows += static_cast<uint64_t>(out->GetRowCount());
            ++flushBatches;
            outs.push_back(out);
        }
        EXPECT_EQ(flushedRows, static_cast<uint64_t>(kKeys)) << "input resumed before flush " << b << " finished";
        maxBatchesPerFlush = std::max(maxBatchesPerFlush, flushBatches);
    }
    EXPECT_GT(maxBatchesPerFlush, 1) << "the flush must span several output batches for this test to mean anything";
    EXPECT_EQ(op->PartialFlushTimes(), static_cast<uint64_t>(kBatches));
    Drain(op, outs);
    EXPECT_EQ(TotalRows(outs), static_cast<uint64_t>(kKeys) * kBatches);
    op::Operator::DeleteOperator(op);

    auto sums = FinalSums(groupType, outs, LongKey);
    ASSERT_EQ(sums.size(), static_cast<size_t>(kKeys));
    for (int32_t k = 0; k < kKeys; ++k) {
        ASSERT_EQ(sums[std::to_string(k)], static_cast<int64_t>(k + 1) * kBatches) << "key " << k;
    }
}

// Varchar keys take the serialize/row-container path.
TEST(PartialAggFlushTest, flush_keeps_result_varchar_keys)
{
    ConfigUtil::SetSupportContainerVecRule(SupportContainerVecRule::NOT_SUPPORT);
    constexpr int32_t kKeys = 500;
    constexpr int32_t kDupsPerBatch = 8;
    constexpr int32_t kRows = kKeys * kDupsPerBatch;
    constexpr int32_t kBatches = 10;
    DataTypes groupType(std::vector<DataTypePtr>{VarcharType(16)});
    DataTypes sourceTypes(std::vector<DataTypePtr>{VarcharType(16), LongType()});

    auto factory = MakeSumFactory(groupType, AggregationNode::Step::K_PARTIAL, true, true);
    auto *op = static_cast<HashAggregationOperator *>(factory->CreateOperator(nullptr));
    op->Init();
    op->setNoMoreInput(false);
    op->SetPartialFlushMemory(8 * 1024, 16 * 1024);

    std::vector<VectorBatch *> outs;
    for (int32_t b = 0; b < kBatches; ++b) {
        std::string keys[kRows];
        int64_t vals[kRows];
        for (int32_t i = 0; i < kRows; ++i) {
            keys[i] = "k" + std::to_string(i % kKeys);
            vals[i] = i % kKeys;
        }
        VectorBatch *in = CreateVectorBatch(sourceTypes, kRows, keys, vals);
        op->AddInput(in);
        Pump(op, outs);
    }
    EXPECT_GE(op->PartialFlushTimes(), 1u);
    Drain(op, outs);
    op::Operator::DeleteOperator(op);

    auto sums = FinalSums(groupType, outs, VarcharKey);
    ASSERT_EQ(sums.size(), static_cast<size_t>(kKeys));
    for (int32_t k = 0; k < kKeys; ++k) {
        EXPECT_EQ(sums["k" + std::to_string(k)], static_cast<int64_t>(k) * kDupsPerBatch * kBatches) << "key " << k;
    }
}

// No reduction at all: the cap stays at its base and every batch flushes (the
// core never abandons; the wrapper's row-ratio rule is the one abandon path).
// The distinct-key count stays cumulative across flushes so that rule can see it.
TEST(PartialAggFlushTest, no_reduction_keeps_flushing_at_the_base_cap_and_counts_cumulative_keys)
{
    ConfigUtil::SetSupportContainerVecRule(SupportContainerVecRule::NOT_SUPPORT);
    constexpr int32_t kRows = 100;
    DataTypes groupType(std::vector<DataTypePtr>{LongType()});
    DataTypes sourceTypes(std::vector<DataTypePtr>{LongType(), LongType()});

    auto factory = MakeSumFactory(groupType, AggregationNode::Step::K_PARTIAL, true, true);
    auto *op = static_cast<HashAggregationOperator *>(factory->CreateOperator(nullptr));
    op->Init();
    op->setNoMoreInput(false);
    // 1 byte: every batch flushes.
    op->SetPartialFlushMemory(1, 2);

    std::vector<VectorBatch *> outs;
    auto feed = [&](int32_t batchIndex) {
        int64_t keys[kRows];
        int64_t vals[kRows];
        for (int32_t i = 0; i < kRows; ++i) {
            keys[i] = batchIndex * kRows + i; // all distinct, no reduction
            vals[i] = 1;
        }
        op->AddInput(CreateVectorBatch(sourceTypes, kRows, keys, vals));
        Pump(op, outs);
    };

    feed(0);
    EXPECT_EQ(op->PartialFlushTimes(), 1u);
    EXPECT_EQ(op->GetHashMapUniqueKeys(), 0u) << "table is empty after the flush";
    EXPECT_EQ(op->CumulativeDistinctKeys(), 100u) << "the abandon rule's count stays cumulative";

    feed(1);
    EXPECT_EQ(op->PartialFlushTimes(), 2u) << "no reduction: the cap stays at its base and the table keeps flushing";
    EXPECT_EQ(op->CumulativeDistinctKeys(), 200u);

    feed(2);
    EXPECT_EQ(op->PartialFlushTimes(), 3u);
    EXPECT_TRUE(op->needsInput());
    EXPECT_EQ(op->GetHashMapUniqueKeys(), 0u) << "flushed again";
    EXPECT_EQ(op->CumulativeDistinctKeys(), 300u) << "300 flushed";

    Drain(op, outs);
    EXPECT_EQ(TotalRows(outs), 300u);
    op::Operator::DeleteOperator(op);
    for (auto *b : outs) {
        VectorHelper::FreeVecBatch(b);
    }
}

// Through the expression wrapper the driver sees needsInput()/GetOutput() of the
// wrapper: the flush is emitted mid-input, keeps going at the base cap, and
// the final sum is still right.
TEST(PartialAggFlushTest, wrapper_flushes_mid_input_and_keeps_flushing)
{
    ConfigUtil::SetSupportContainerVecRule(SupportContainerVecRule::NOT_SUPPORT);
    constexpr int32_t kRows = 2000;
    constexpr int32_t kBatches = 6;
    DataTypes sourceTypes(std::vector<DataTypePtr>{LongType(), LongType()});
    DataTypes aggOutputTypes(std::vector<DataTypePtr>{LongType()});

    std::vector<Expr *> groupByKeys{new FieldExpr(0, LongType())};
    std::vector<std::vector<Expr *>> aggKeys{{new FieldExpr(1, LongType())}};
    std::vector<Expr *> aggFilters;
    std::vector<uint32_t> aggFuncTypes{OMNI_AGGREGATION_TYPE_SUM};
    std::vector<uint32_t> maskCols{static_cast<uint32_t>(-1)};
    auto aggOutputTypesWrap = AggregatorUtil::WrapWithVector(aggOutputTypes);
    std::vector<bool> inputRaws{true};
    std::vector<bool> outputPartials{true};
    config::QueryConfig queryConfig(std::unordered_map<std::string, std::string>{
        {config::QueryConfig::KMaxPartialAggregationMemory, "4096"},
        {config::QueryConfig::KMaxExtendedPartialAggregationMemory, "8192"},
        {config::QueryConfig::KAdaptivePartialAggregationMinRows, "100000000"},
    });
    auto *factory = new HashAggregationWithExprOperatorFactory(groupByKeys, 1, aggKeys, aggFilters, sourceTypes,
        aggOutputTypesWrap, aggFuncTypes, maskCols, inputRaws, outputPartials, OperatorConfig(), queryConfig,
        AggregationNode::Step::K_PARTIAL);
    auto *op = dynamic_cast<HashAggregationWithExprOperator *>(CreateTestOperator(factory));
    ASSERT_NE(op, nullptr);
    op->setNoMoreInput(false);
    ASSERT_TRUE(op->IsStepPartials());

    std::vector<VectorBatch *> outs;
    for (int32_t b = 0; b < kBatches; ++b) {
        int64_t keys[kRows];
        int64_t vals[kRows];
        for (int32_t i = 0; i < kRows; ++i) {
            keys[i] = b * kRows + i; // every key distinct: flush at the base cap, keep flushing
            vals[i] = 2;
        }
        EXPECT_TRUE(op->needsInput());
        op->AddInput(CreateVectorBatch(sourceTypes, kRows, keys, vals));
        PumpWrapper(op, outs);
    }
    EXPECT_GT(TotalRows(outs), 0u) << "rows must leave the operator before noMoreInput";
    Drain(op, outs);
    EXPECT_EQ(TotalRows(outs), static_cast<uint64_t>(kRows) * kBatches);
    op::Operator::DeleteOperator(op);
    delete factory;

    DataTypes groupType(std::vector<DataTypePtr>{LongType()});
    auto sums = FinalSums(groupType, outs, LongKey);
    ASSERT_EQ(sums.size(), static_cast<size_t>(kRows) * kBatches);
    for (auto &kv : sums) {
        EXPECT_EQ(kv.second, 2) << "key " << kv.first;
    }
}

// A flushable partial table within its cap is not a spill donor: the pool skips
// it (reclaimableBytes() == 0) and a reclaim request only brings the flush
// forward. Without a cap the table is a donor as before.
TEST_F(TestPoolFixture, flushable_partial_table_is_not_a_spill_donor)
{
    ConfigUtil::SetSupportContainerVecRule(SupportContainerVecRule::NOT_SUPPORT);
    constexpr int32_t kRows = 2000;
    DataTypes groupType(std::vector<DataTypePtr>{LongType()});
    DataTypes sourceTypes(std::vector<DataTypePtr>{LongType(), LongType()});
    auto makeBatch = [&]() {
        int64_t keys[kRows];
        int64_t vals[kRows];
        for (int32_t i = 0; i < kRows; ++i) {
            keys[i] = i % 500;
            vals[i] = 1;
        }
        return CreateVectorBatch(sourceTypes, kRows, keys, vals);
    };

    auto capped = MakeSumFactory(groupType, AggregationNode::Step::K_PARTIAL, true, true);
    auto *op = static_cast<HashAggregationOperator *>(capped->CreateOperator(pool()));
    op->Init();
    op->setNoMoreInput(false);
    op->SetPartialFlushMemory(64 * 1024 * 1024, 96 * 1024 * 1024); // far above this table
    op->AddInput(makeBatch());
    EXPECT_TRUE(op->needsInput());
    EXPECT_EQ(op->reclaimableBytes(), 0u) << "within its cap the table flushes, it does not spill";
    EXPECT_EQ(op->reclaim(1 << 20), 0);
    EXPECT_TRUE(op->IsPartialFlushing()) << "a reclaim request brings the flush forward";
    EXPECT_FALSE(op->needsInput());
    std::vector<VectorBatch *> outs;
    Pump(op, outs);
    EXPECT_EQ(TotalRows(outs), 500u);
    EXPECT_EQ(op->PartialFlushTimes(), 1u);
    EXPECT_TRUE(op->needsInput());
    Drain(op, outs);
    op::Operator::DeleteOperator(op);
    auto sums = FinalSums(groupType, outs, LongKey);
    ASSERT_EQ(sums.size(), 500u);
    for (auto &kv : sums) {
        EXPECT_EQ(kv.second, 4) << "key " << kv.first;
    }

    auto uncapped = MakeSumFactory(groupType, AggregationNode::Step::K_PARTIAL, true, true);
    auto *donor = static_cast<HashAggregationOperator *>(uncapped->CreateOperator(pool()));
    donor->Init();
    donor->setNoMoreInput(false);
    donor->AddInput(makeBatch());
    EXPECT_GT(donor->reclaimableBytes(), 0u) << "no cap: the table is a spill donor as before";
    std::vector<VectorBatch *> rest;
    Drain(donor, rest);
    op::Operator::DeleteOperator(donor);
    for (auto *b : rest) {
        VectorHelper::FreeVecBatch(b);
    }
}

// A flush lets the order in which partial states reach the final aggregation pick the value of
// first/last, and the shuffle writer does not keep that order once it has spilled. Only the shape
// of Spark's count-distinct rewrite, first/last(x, ignoreNulls) FILTER (gid = 0), is immune (one
// non-null candidate per group), so only that shape stays eligible.
TEST(PartialAggFlushTest, first_last_flush_only_in_the_count_distinct_rewrite_shape)
{
    ConfigUtil::SetSupportContainerVecRule(SupportContainerVecRule::NOT_SUPPORT);
    DataTypes groupType(std::vector<DataTypePtr>{LongType()});
    auto eligible = [&](uint32_t funcType, int8_t hasFilter) {
        std::vector<uint32_t> groupByCols{0};
        std::vector<std::vector<uint32_t>> aggCols{{1}};
        std::vector<DataTypes> aggInputTypes =
            AggregatorUtil::WrapWithVector(DataTypes(std::vector<DataTypePtr>{LongType()}));
        std::vector<DataTypes> aggOutputTypes =
            AggregatorUtil::WrapWithVector(DataTypes(std::vector<DataTypePtr>{LongType(), BooleanType()}));
        std::vector<uint32_t> aggFuncTypes{funcType};
        std::vector<uint32_t> maskCols{static_cast<uint32_t>(-1)};
        std::vector<bool> inputRaws{true};
        std::vector<bool> outputPartials{true};
        std::vector<int8_t> hasAggFilters{hasFilter};
        HashAggregationOperatorFactory factory(groupByCols, groupType, aggCols, aggInputTypes, aggOutputTypes,
            aggFuncTypes, maskCols, inputRaws, outputPartials, hasAggFilters, OperatorConfig(),
            AggregationNode::Step::K_PARTIAL);
        factory.Init();
        auto *op = static_cast<HashAggregationOperator *>(factory.CreateOperator(nullptr));
        const bool result = op->PartialFlushEligible();
        EXPECT_FALSE(op->IsStepPartials()) << "first/last stays out of the row-ratio abandon rule";
        op::Operator::DeleteOperator(op);
        return result;
    };
    EXPECT_TRUE(eligible(OMNI_AGGREGATION_TYPE_FIRST_IGNORENULL, 1)) << "the count-distinct rewrite";
    EXPECT_TRUE(eligible(OMNI_AGGREGATION_TYPE_LAST_IGNORENULL, 1));
    EXPECT_FALSE(eligible(OMNI_AGGREGATION_TYPE_FIRST_IGNORENULL, 0)) << "no filter: several candidates per group";
    EXPECT_FALSE(eligible(OMNI_AGGREGATION_TYPE_FIRST_INCLUDENULL, 1));
    EXPECT_FALSE(eligible(OMNI_AGGREGATION_TYPE_FIRST_INCLUDENULL, 0));
    EXPECT_FALSE(eligible(OMNI_AGGREGATION_TYPE_LAST_INCLUDENULL, 0));
}

// A table above its extended cap is a spill donor even while its flush is
// pending. When a reclaim spills it between AddInput and the driver's next
// GetOutput, the disk path owns the table and must wait for the end of input:
// starting the merge mid-input finished the operator and silently dropped every
// group added afterwards.
TEST_F(TestPoolFixture, pending_flush_spilled_by_reclaim_keeps_later_groups)
{
    ConfigUtil::SetSupportContainerVecRule(SupportContainerVecRule::NOT_SUPPORT);
    constexpr int32_t kRows = 2000;
    DataTypes groupType(std::vector<DataTypePtr>{LongType()});
    DataTypes sourceTypes(std::vector<DataTypePtr>{LongType(), LongType()});
    auto makeBatch = [&](int64_t firstKey) {
        int64_t keys[kRows];
        int64_t vals[kRows];
        for (int32_t i = 0; i < kRows; ++i) {
            keys[i] = firstKey + i;
            vals[i] = 1;
        }
        return CreateVectorBatch(sourceTypes, kRows, keys, vals);
    };

    SparkSpillConfig spillConfig(TestUtil::GenerateSpillPath(), INT32_MAX, 0);
    OperatorConfig operatorConfig(spillConfig);
    auto factory = MakeSumFactory(groupType, AggregationNode::Step::K_PARTIAL, true, true, operatorConfig);
    auto *op = static_cast<HashAggregationOperator *>(factory->CreateOperator(pool()));
    op->Init();
    op->setNoMoreInput(false);
    // Cap equal to the extended cap: a table over the cap is over both.
    op->SetPartialFlushMemory(4096, 4096);

    op->AddInput(makeBatch(0));
    ASSERT_TRUE(op->IsPartialFlushing()) << "the table passed its cap";
    ASSERT_FALSE(op->needsInput());
    ASSERT_GT(op->reclaimableBytes(), 0u) << "above the extended cap the table is a spill donor";
    ASSERT_GT(op->reclaim(1), 0) << "the reclaim spills the table";

    std::vector<VectorBatch *> outs;
    Pump(op, outs);
    EXPECT_TRUE(outs.empty()) << "nothing is emitted mid-input once the table is on disk";
    EXPECT_FALSE(op->isFinished());
    ASSERT_TRUE(op->needsInput()) << "input resumes after the spill";
    EXPECT_EQ(op->PartialFlushTimes(), 0u);

    op->AddInput(makeBatch(kRows));
    Pump(op, outs);
    Drain(op, outs);
    op::Operator::DeleteOperator(op);

    auto sums = FinalSums(groupType, outs, LongKey);
    ASSERT_EQ(sums.size(), static_cast<size_t>(2 * kRows)) << "both key ranges reach the final aggregation";
    for (auto &kv : sums) {
        EXPECT_EQ(kv.second, 1) << "key " << kv.first;
    }
}
} // namespace omniruntime
