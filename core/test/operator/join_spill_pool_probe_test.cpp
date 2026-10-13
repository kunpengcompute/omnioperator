/* Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 * Pool-triggered held-table reclaim on the target hash join handoff protocol.
 */
#include <atomic>
#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <type_traits>

#include "gtest/gtest.h"
#include "expression/expressions.h"
#include "memory/pool_listener.h"
#include "memory/omni_task_memory_pool.h"
#include "operator/join/hash_builder.h"
#include "operator/join/join_spill_state.h"
#include "operator/join/lookup_join.h"
#include "type/data_type.h"
#include "vector/vector.h"
#include "vector/vector_batch.h"
#include "vector/vector_helper.h"

using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::vec;

namespace {
class NoopListener : public omniruntime::mem::PoolListener {
public:
    void allocationChanged(int64_t) override {}
};

class PoolProbeJoin {
public:
    NoopListener listener;
    omniruntime::mem::OmniTaskMemoryPool pool{&listener};
    DataTypes types{{LongType(), LongType()}};
    JoinSubPartitionConfig cfg;
    std::shared_ptr<JoinSpillState> state;
    std::unique_ptr<HashBuilderOperatorFactory> hashFactory;
    std::unique_ptr<LookupJoinOperatorFactory> lookupFactory;
    std::unique_ptr<HashBuilderOperator, void(*)(HashBuilderOperator *)> build{
        nullptr, [](HashBuilderOperator *op) { if (op) omniruntime::op::Operator::DeleteOperator(op); }};
    std::unique_ptr<LookupJoinOperator, void(*)(LookupJoinOperator *)> probe{
        nullptr, [](LookupJoinOperator *op) { if (op) omniruntime::op::Operator::DeleteOperator(op); }};

    explicit PoolProbeJoin(JoinType joinType = OMNI_JOIN_TYPE_INNER,
        omniruntime::expressions::Expr *filterExpr = nullptr)
    {
        static std::atomic<uint64_t> id{0};
        cfg.numSubPartitions = 4;
        cfg.startPartitionBit = 0;
        cfg.activeSubPartition = 0;
        int32_t hashCol = 0;
        hashFactory.reset(joinType == OMNI_JOIN_TYPE_INNER
            ? HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
                  joinType, types, &hashCol, 1, 1)
            : HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
                  joinType, OMNI_BUILD_RIGHT, types, &hashCol, 1, 1));
        state = std::make_shared<JoinSpillState>(
            "/tmp/join-pool-probe-" + std::to_string(id.fetch_add(1)), types, types, 4);
        state->SetHandoffSessionEnabled(true);
        state->SetInitialRuntimeLayout(JoinBuildRuntimeLayout::Single);
        hashFactory->SetJoinSpillSubPartitionPolicy(true, 0, cfg, true, false, 10);
        hashFactory->SetJoinSpillState(state);
        build.reset(static_cast<HashBuilderOperator *>(hashFactory->CreateOperator(&pool)));

        int32_t outputCols[] = {0, 1};
        lookupFactory.reset(LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(
            types, outputCols, 2, &hashCol, 1, outputCols, 2, types,
            reinterpret_cast<int64_t>(hashFactory.get()), filterExpr, false, nullptr));
        lookupFactory->SetJoinSpillPolicy(true, cfg, true);
        lookupFactory->SetJoinSpillState(state);
        probe.reset(static_cast<LookupJoinOperator *>(lookupFactory->CreateOperator(&pool)));
        probe->setNoMoreInput(false);
    }

    VectorBatch *Batch(int64_t start, int32_t rows, int64_t valueOffset = 1000)
    {
        auto *batch = new VectorBatch(rows);
        auto *keys = new Vector<int64_t>(rows, &pool);
        auto *values = new Vector<int64_t>(rows, &pool);
        for (int32_t row = 0; row < rows; ++row) {
            keys->SetValue(row, start + row);
            values->SetValue(row, start + row + valueOffset);
        }
        batch->Append(keys);
        batch->Append(values);
        return batch;
    }
};

TEST(JoinSpillPoolProbeTest, ReleasedBuildStateKeepsEmptyPartitionObserversValid)
{
    PoolProbeJoin h;
    h.build->AddInput(h.Batch(0, 8));
    std::visit([&](auto &table) {
        ASSERT_GT(table.GetTotalRowCount(0), 0U);
        table.ReleaseBuildState();
        EXPECT_TRUE(table.IsBuildStateReleased());
        EXPECT_EQ(table.GetResidentBuildPartitionCount(), 0U);
        EXPECT_EQ(table.EstimateHashTableBytes(), 0U);
        EXPECT_EQ(table.EstimateArenaBytes(), 0U);
        for (uint32_t partition = 0; partition < table.GetHashTableCount(); ++partition) {
            EXPECT_TRUE(table.GetResidentInputVecBatches(partition).empty());
            EXPECT_EQ(table.GetTotalRowCount(partition), 0U);
            EXPECT_EQ(table.EstimateBuildReservation(partition), 0U);
            EXPECT_EQ(table.GetHashTable(partition), nullptr);
            table.ReclaimBuildPartition(partition);
        }
        EXPECT_EQ(table.ReleaseBuildState(), 0U);
        auto *rejected = h.Batch(0, 1);
        EXPECT_THROW(table.AddVecBatch(0, rejected), omniruntime::exception::OmniException);
        VectorHelper::FreeVecBatch(rejected);
    }, *h.hashFactory->GetHashTablesVariants());
}

TEST(JoinSpillPoolProbeTest, PathAOuterOutputMustDrainBeforeAcceptingMoreInput)
{
    PoolProbeJoin h(OMNI_JOIN_TYPE_FULL);
    h.build->AddInput(h.Batch(0, 1));
    ASSERT_GT(h.build->reclaim(1), 0);
    h.build->noMoreInput();
    VectorBatch *unused = nullptr;
    h.build->GetOutput(&unused);
    h.probe->GetOutput(&unused);
    ASSERT_EQ(unused, nullptr);
    ASSERT_EQ(h.state->GetHandoffPhase(), JoinHandoffPhase::ProbePathA);
    ASSERT_TRUE(h.probe->needsInput());
    h.probe->AddInput(h.Batch(0, 128));
    ASSERT_FALSE(h.probe->needsInput());
    int64_t outputRows = 0;
    for (int i = 0; i < 100 && !h.probe->needsInput(); ++i) {
        VectorBatch *out = nullptr;
        h.probe->GetOutput(&out);
        if (out != nullptr) {
            outputRows += out->GetRowCount();
            VectorHelper::FreeVecBatch(out);
        }
    }
    EXPECT_GT(outputRows, 0);
    ASSERT_TRUE(h.probe->needsInput());
    h.probe->noMoreInput();
    for (int i = 0; i < 10000 && !(h.build->isFinished() && h.probe->isFinished()); ++i) {
        h.build->GetOutput(&unused);
        VectorBatch *out = nullptr;
        h.probe->GetOutput(&out);
        if (out != nullptr) {
            outputRows += out->GetRowCount();
            VectorHelper::FreeVecBatch(out);
        }
    }
    EXPECT_TRUE(h.build->isFinished());
    EXPECT_TRUE(h.probe->isFinished());
    EXPECT_EQ(outputRows, 128);
}

TEST(JoinSpillPoolProbeTest, UnmatchedBuildRestoreOutputUsesTaskPool)
{
    PoolProbeJoin h(OMNI_JOIN_TYPE_FULL);
    h.build->AddInput(h.Batch(0, 64));
    ASSERT_GT(h.build->reclaim(1), 0);
    h.build->noMoreInput();
    VectorBatch *unused = nullptr;
    h.build->GetOutput(&unused);
    h.probe->GetOutput(&unused);
    ASSERT_EQ(unused, nullptr);
    h.probe->AddInput(h.Batch(1000, 2));
    h.probe->noMoreInput();
    int64_t unmatchedBuildRows = 0;
    int64_t outputRows = 0;
    for (int i = 0; i < 10000 && !(h.build->isFinished() && h.probe->isFinished()); ++i) {
        h.build->GetOutput(&unused);
        VectorBatch *out = nullptr;
        h.probe->GetOutput(&out);
        if (out == nullptr) continue;
        outputRows += out->GetRowCount();
        for (int32_t row = 0; row < out->GetRowCount(); ++row) {
            if (out->Get(0)->IsNull(row)) {
                ++unmatchedBuildRows;
                for (int32_t col = 0; col < out->GetVectorCount(); ++col) {
                    EXPECT_EQ(out->Get(col)->GetPool(), &h.pool);
                }
            }
        }
        VectorHelper::FreeVecBatch(out);
    }
    EXPECT_TRUE(h.build->isFinished());
    EXPECT_TRUE(h.probe->isFinished());
    EXPECT_EQ(unmatchedBuildRows, 64);
    EXPECT_EQ(outputRows, 66);
}

TEST(JoinSpillPoolProbeTest, VisitedFlagsPreserveBatchAndRowCoordinates)
{
    PoolProbeJoin h(OMNI_JOIN_TYPE_FULL);
    h.build->AddInput(h.Batch(0, 32));
    h.build->AddInput(h.Batch(32, 32));
    h.build->noMoreInput();
    VectorBatch *out = nullptr;
    h.build->GetOutput(&out);
    ASSERT_EQ(out, nullptr);
    h.probe->AddInput(h.Batch(16, 32));
    for (int i = 0; i < 100 && !h.probe->needsInput(); ++i) {
        h.probe->GetOutput(&out);
        if (out != nullptr) {
            VectorHelper::FreeVecBatch(out);
            out = nullptr;
        }
    }
    ASSERT_TRUE(h.probe->needsInput());
    bool checked = false;
    std::visit([&](auto &table) {
        using Mapped = typename std::decay_t<decltype(table)>::Mapped;
        if constexpr (std::is_same_v<Mapped, RowRefListWithFlags>) {
            table.AppendVisitedFlagsToResident(0);
            const auto &batches = table.GetResidentInputVecBatches(0);
            ASSERT_EQ(batches.size(), 2U);
            for (size_t batch = 0; batch < batches.size(); ++batch) {
                auto *flags = static_cast<Vector<bool> *>(batches[batch]->Get(2));
                for (int32_t row = 0; row < 32; ++row) {
                    const auto key = batch * 32 + row;
                    EXPECT_EQ(flags->GetValue(row), key >= 16 && key < 48);
                }
            }
            checked = true;
        }
    }, *h.hashFactory->GetHashTablesVariants());
    EXPECT_TRUE(checked);
}

TEST(JoinSpillPoolProbeTest, FreshFullJoinVisitedFlagsDefaultFalse)
{
    PoolProbeJoin h(OMNI_JOIN_TYPE_FULL);
    h.build->AddInput(h.Batch(0, 64));
    h.build->noMoreInput();
    VectorBatch *unused = nullptr;
    h.build->GetOutput(&unused);
    ASSERT_EQ(unused, nullptr);

    // A newly allocated flag column may contain arbitrary bytes. A held-table
    // flush must overwrite every row before persisting it to disk.
    Vector<bool> flags(64, &h.pool);
    for (int32_t row = 0; row < 64; ++row) {
        flags.SetValue(row, true);
    }
    bool checked = false;
    std::visit([&](auto &&arg) {
        using Mapped = typename std::decay_t<decltype(arg)>::Mapped;
        if constexpr (std::is_same_v<Mapped, RowRefListWithFlags>) {
            arg.FillVisitedFlagColumn(0, 0, &flags);
            checked = true;
        }
    }, *h.hashFactory->GetHashTablesVariants());
    ASSERT_TRUE(checked);
    for (int32_t row = 0; row < 64; ++row) {
        EXPECT_FALSE(flags.GetValue(row)) << "row=" << row;
    }
}

TEST(JoinSpillPoolProbeTest, FullHeldTableReclaimPreservesTargetRestoreOutput)
{
    PoolProbeJoin h;
    h.build->AddInput(h.Batch(0, 64));
    h.build->noMoreInput();
    VectorBatch *unused = nullptr;
    h.build->GetOutput(&unused);
    ASSERT_EQ(unused, nullptr);
    ASSERT_EQ(h.state->GetHandoffPhase(), JoinHandoffPhase::RestoreRound);

    // Apply the full-table handoff before asking the pool to reclaim its resident table.
    h.probe->GetOutput(&unused);
    ASSERT_EQ(h.state->TableInHand().kind, JoinSpillTableInHandKind::FullTable);
    h.probe->AddInput(h.Batch(0, 64));
    ASSERT_TRUE(h.state->HasMoreProbeInput());
    ASSERT_EQ(h.state->GetHandoffPhase(), JoinHandoffPhase::RestoreRound);
    ASSERT_FALSE(h.state->ProbeInNrs());
    ASSERT_TRUE(h.state->CanFlushHeldTable());
    ASSERT_GT(h.probe->reclaimableBytes(), 0U);
    const int64_t before = h.pool.currentBytes();
    const int64_t reclaimed = h.probe->reclaim(std::numeric_limits<int64_t>::max());
    EXPECT_EQ(reclaimed, std::max<int64_t>(0, before - h.pool.currentBytes()));
    EXPECT_GT(reclaimed, 0);
    EXPECT_EQ(h.state->GetHandoffPhase(), JoinHandoffPhase::ProbePathA);
    EXPECT_GT(h.state->BuildSpilledRows(), 0U);

    h.probe->noMoreInput();
    int64_t outputRows = 0;
    for (int i = 0; i < 10000; ++i) {
        h.build->GetOutput(&unused);
        ASSERT_LE(h.build->GetHashTableSize(), 1U);
        VectorBatch *out = nullptr;
        h.probe->GetOutput(&out);
        if (out != nullptr) {
            outputRows += out->GetRowCount();
            VectorHelper::FreeVecBatch(out);
        }
        if (h.build->isFinished() && h.probe->isFinished()) break;
    }
    EXPECT_TRUE(h.build->isFinished());
    EXPECT_TRUE(h.probe->isFinished());
    EXPECT_EQ(outputRows, 64);
}

TEST(JoinSpillPoolProbeTest, RestoreSmallTableReclaimKeepsProbeReaderCursor)
{
    PoolProbeJoin h;
    h.build->AddInput(h.Batch(0, 64));
    h.build->noMoreInput();
    VectorBatch *unused = nullptr;
    h.build->GetOutput(&unused);
    h.probe->GetOutput(&unused);
    h.probe->AddInput(h.Batch(0, 64));
    ASSERT_GT(h.probe->reclaim(std::numeric_limits<int64_t>::max()), 0);
    h.probe->AddInput(h.Batch(0, 64));
    h.probe->noMoreInput();

    bool reclaimedSmallTable = false;
    int64_t outputRows = 0;
    for (int i = 0; i < 10000; ++i) {
        h.build->GetOutput(&unused);
        ASSERT_LE(h.build->GetHashTableSize(), 1U);
        VectorBatch *out = nullptr;
        h.probe->GetOutput(&out);
        if (out != nullptr) {
            outputRows += out->GetRowCount();
            VectorHelper::FreeVecBatch(out);
        }
        if (!reclaimedSmallTable &&
            h.state->TableInHand().kind == JoinSpillTableInHandKind::RestoreSmallTable &&
            h.probe->reclaimableBytes() > 0) {
            EXPECT_GT(h.probe->reclaim(std::numeric_limits<int64_t>::max()), 0);
            reclaimedSmallTable = true;
        }
        if (h.build->isFinished() && h.probe->isFinished()) break;
    }
    EXPECT_TRUE(reclaimedSmallTable);
    EXPECT_TRUE(h.build->isFinished());
    EXPECT_TRUE(h.probe->isFinished());
    EXPECT_EQ(outputRows, 128);
}

TEST(JoinSpillPoolProbeTest, LeftSemiBuildReclaimRestoresMatchingProbeRows)
{
    PoolProbeJoin h(OMNI_JOIN_TYPE_LEFT_SEMI);
    h.build->AddInput(h.Batch(0, 64));
    ASSERT_GT(h.build->reclaim(std::numeric_limits<int64_t>::max()), 0);
    h.build->noMoreInput();
    VectorBatch *unused = nullptr;
    h.build->GetOutput(&unused);
    ASSERT_EQ(h.state->GetHandoffPhase(), JoinHandoffPhase::ProbePathA);
    ASSERT_GT(h.state->BuildSpilledRows(), 0U);

    h.probe->AddInput(h.Batch(0, 64));
    h.probe->AddInput(h.Batch(0, 64));
    h.probe->AddInput(h.Batch(64, 32)); // no build match
    h.probe->noMoreInput();

    int64_t outputRows = 0;
    for (int i = 0; i < 10000; ++i) {
        h.build->GetOutput(&unused);
        VectorBatch *out = nullptr;
        h.probe->GetOutput(&out);
        if (out != nullptr) {
            outputRows += out->GetRowCount();
            VectorHelper::FreeVecBatch(out);
        }
        if (h.build->isFinished() && h.probe->isFinished()) break;
    }
    EXPECT_TRUE(h.build->isFinished());
    EXPECT_TRUE(h.probe->isFinished());
    EXPECT_GT(h.probe->ProbeSpilledRows(), 0U);
    EXPECT_EQ(outputRows, 128);
}

TEST(JoinSpillPoolProbeTest, LeftSemiSkipsProbeBucketsWithoutBuildRows)
{
    PoolProbeJoin h(OMNI_JOIN_TYPE_LEFT_SEMI);
    h.build->AddInput(h.Batch(0, 1));
    ASSERT_GT(h.build->reclaim(std::numeric_limits<int64_t>::max()), 0);
    h.build->noMoreInput();
    VectorBatch *unused = nullptr;
    h.build->GetOutput(&unused);
    ASSERT_EQ(h.state->GetHandoffPhase(), JoinHandoffPhase::ProbePathA);

    h.probe->AddInput(h.Batch(0, 65)); // only key 0 has a build match
    h.probe->noMoreInput();

    int64_t outputRows = 0;
    for (int i = 0; i < 10000; ++i) {
        h.build->GetOutput(&unused);
        VectorBatch *out = nullptr;
        h.probe->GetOutput(&out);
        if (out != nullptr) {
            outputRows += out->GetRowCount();
            VectorHelper::FreeVecBatch(out);
        }
        if (h.build->isFinished() && h.probe->isFinished()) break;
    }
    EXPECT_TRUE(h.build->isFinished());
    EXPECT_TRUE(h.probe->isFinished());
    EXPECT_GT(h.probe->ProbeSpilledRows(), 0U);
    EXPECT_LT(h.probe->ProbeSpilledRows(), 65U);
    EXPECT_EQ(outputRows, 1);
}

TEST(JoinSpillPoolProbeTest, LeftSemiRestoreHonorsAdditionalFilter)
{
    auto *filter = new omniruntime::expressions::BinaryExpr(
        omniruntime::expressions::Operator::NEQ,
        new omniruntime::expressions::FieldExpr(1, LongType()),
        new omniruntime::expressions::FieldExpr(3, LongType()), BooleanType());
    PoolProbeJoin h(OMNI_JOIN_TYPE_LEFT_SEMI, filter);
    h.build->AddInput(h.Batch(0, 64));
    ASSERT_GT(h.build->reclaim(std::numeric_limits<int64_t>::max()), 0);
    h.build->noMoreInput();
    VectorBatch *unused = nullptr;
    h.build->GetOutput(&unused);
    ASSERT_EQ(h.state->GetHandoffPhase(), JoinHandoffPhase::ProbePathA);

    h.probe->AddInput(h.Batch(0, 32));       // equal values fail the extra predicate
    h.probe->AddInput(h.Batch(32, 32, 2000)); // different values pass
    h.probe->noMoreInput();

    int64_t outputRows = 0;
    for (int i = 0; i < 10000; ++i) {
        h.build->GetOutput(&unused);
        VectorBatch *out = nullptr;
        h.probe->GetOutput(&out);
        if (out != nullptr) {
            outputRows += out->GetRowCount();
            VectorHelper::FreeVecBatch(out);
        }
        if (h.build->isFinished() && h.probe->isFinished()) break;
    }
    EXPECT_TRUE(h.build->isFinished());
    EXPECT_TRUE(h.probe->isFinished());
    EXPECT_GT(h.probe->ProbeSpilledRows(), 0U);
    EXPECT_EQ(outputRows, 32);
}
} // namespace
