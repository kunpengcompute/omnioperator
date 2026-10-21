/* Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 * Pool admission tests for the target's single-slot hash join restore protocol.
 */
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <string_view>

#include "gtest/gtest.h"
#include "memory/pool_listener.h"
#include "memory/omni_task_memory_pool.h"
#include "memory/reservation_denied.h"
#include "operator/join/hash_builder.h"
#include "operator/join/join_spill_state.h"
#include "operator/reclaim_context.h"
#include "type/data_type.h"
#include "vector/vector.h"
#include "vector/vector_batch.h"

using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::vec;

namespace {
class DenyingListener : public omniruntime::mem::PoolListener {
public:
    void allocationChanged(int64_t diff) override
    {
        if (diff <= 0 || !armed) return;
        const auto request = CurrentReservationRequest();
        if (request.site == nullptr || std::string_view(request.site) !=
            "HashBuilderOperator::DriveHandoffRestore") return;
        ++requests;
        requestorWasProtected = request.requestor != nullptr &&
            request.requestor->isInNonReclaimableSection();
        if (deny) {
            deny = false;
            throw omniruntime::mem::ReservationDenied("injected join reservation denial");
        }
    }
    bool armed = false;
    bool deny = false;
    bool requestorWasProtected = false;
    int requests = 0;
};

struct BuilderFixture {
    DenyingListener listener;
    omniruntime::mem::OmniTaskMemoryPool pool{&listener};
    DataTypes types{{LongType(), LongType()}};
    std::shared_ptr<JoinSpillState> state;
    std::unique_ptr<HashBuilderOperatorFactory> factory;
    std::unique_ptr<HashBuilderOperator, void(*)(HashBuilderOperator *)> builder{
        nullptr, [](HashBuilderOperator *op) { if (op) omniruntime::op::Operator::DeleteOperator(op); }};

    BuilderFixture()
    {
        static std::atomic<uint64_t> id{0};
        const int32_t key = 0;
        JoinSubPartitionConfig cfg;
        cfg.numSubPartitions = 4;
        cfg.startPartitionBit = 0;
        factory.reset(HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
            OMNI_JOIN_TYPE_INNER, types, &key, 1, 1));
        state = std::make_shared<JoinSpillState>(
            "/tmp/join-target-reservation-" + std::to_string(id.fetch_add(1)), types, types, 4);
        state->SetHandoffSessionEnabled(true);
        state->SetInitialRuntimeLayout(JoinBuildRuntimeLayout::Single);
        factory->SetJoinSpillSubPartitionPolicy(true, 0, cfg, true, false, 10);
        factory->SetJoinSpillState(state);
        builder.reset(static_cast<HashBuilderOperator *>(factory->CreateOperator(&pool)));
    }

    void AddRows(int64_t start = 0)
    {
        auto *batch = new VectorBatch(64);
        auto *keys = new Vector<int64_t>(64, &pool);
        auto *values = new Vector<int64_t>(64, &pool);
        for (int32_t row = 0; row < 64; ++row) {
            keys->SetValue(row, start + row);
            values->SetValue(row, start + row + 1000);
        }
        batch->Append(keys);
        batch->Append(values);
        builder->AddInput(batch);
    }

    void EndBuild()
    {
        builder->setNoMoreInput(true);
        VectorBatch *output = nullptr;
        builder->GetOutput(&output);
        EXPECT_EQ(output, nullptr);
    }
};

TEST(JoinSpillReservationTest, DenialSpillsResidentBuildIntoTargetPathA)
{
    BuilderFixture f;
    f.AddRows();
    ASSERT_GT(f.builder->reclaimableBytes(), 0U);
    f.pool.releaseUnusedReservation();
    f.listener.armed = true;
    f.listener.deny = true;
    f.EndBuild();
    EXPECT_EQ(f.listener.requests, 1);
    EXPECT_TRUE(f.listener.requestorWasProtected);
    EXPECT_EQ(f.state->GetRuntimeLayout(), JoinBuildRuntimeLayout::Partitioned);
    EXPECT_GT(f.state->BuildSpilledRows(), 0U);
    EXPECT_EQ(f.builder->reclaimableBytes(), 0U);
}

TEST(JoinSpillReservationTest, GrantKeepsTargetFullTableHandoff)
{
    BuilderFixture f;
    f.AddRows();
    f.pool.releaseUnusedReservation();
    f.listener.armed = true;
    f.EndBuild();
    EXPECT_EQ(f.listener.requests, 1);
    EXPECT_EQ(f.state->GetRuntimeLayout(), JoinBuildRuntimeLayout::Single);
    EXPECT_EQ(f.state->BuildSpilledRows(), 0U);
    EXPECT_EQ(f.state->GetHandoffPhase(), JoinHandoffPhase::RestoreRound);
}

TEST(JoinSpillPoolBuildTest, MidIngestReclaimAppendsLaterBuildRowsBeforeTargetPathA)
{
    BuilderFixture f;
    f.AddRows();
    const int64_t before = f.pool.currentBytes();
    const int64_t reclaimed = f.builder->reclaim(std::numeric_limits<int64_t>::max());
    EXPECT_EQ(reclaimed, std::max<int64_t>(0, before - f.pool.currentBytes()));
    EXPECT_GT(reclaimed, 0);
    EXPECT_EQ(f.state->GetHandoffPhase(), JoinHandoffPhase::Idle);
    EXPECT_EQ(f.builder->reclaimableBytes(), 0U);
    f.AddRows(100);
    f.EndBuild();
    EXPECT_EQ(f.state->GetHandoffPhase(), JoinHandoffPhase::ProbePathA);
    EXPECT_EQ(f.state->BuildSpilledRows(), 128U);
}

class ReservationBudgetListener : public omniruntime::mem::PoolListener {
public:
    void allocationChanged(int64_t diff) override
    {
        if (diff > 0 && diff > limit - billed) {
            ++denied;
            throw omniruntime::mem::ReservationDenied("join test reservation budget");
        }
        billed += diff;
        peak = std::max(peak, billed);
    }
    int64_t limit = 90LL << 20;
    int64_t billed = 0;
    int64_t peak = 0;
    int denied = 0;
};

struct NullReservationFixture {
    ReservationBudgetListener listener;
    omniruntime::mem::OmniTaskMemoryPool pool{&listener};
    DataTypes types{{LongType(), LongType()}};
    std::shared_ptr<JoinSpillState> state;
    std::unique_ptr<HashBuilderOperatorFactory> factory;
    std::unique_ptr<HashBuilderOperator, void(*)(HashBuilderOperator *)> builder{
        nullptr, [](HashBuilderOperator *op) { if (op) omniruntime::op::Operator::DeleteOperator(op); }};

    explicit NullReservationFixture(omniruntime::JoinType joinType = omniruntime::OMNI_JOIN_TYPE_INNER)
    {
        static std::atomic<uint64_t> id{0};
        const int32_t key = 0;
        JoinSubPartitionConfig cfg;
        cfg.numSubPartitions = 4;
        cfg.startPartitionBit = 0;
        factory.reset(HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
            joinType, types, &key, 1, 1));
        state = std::make_shared<JoinSpillState>(
            "/tmp/join-null-reservation-" + std::to_string(id.fetch_add(1)), types, types, 4);
        state->SetHandoffSessionEnabled(true);
        state->SetInitialRuntimeLayout(JoinBuildRuntimeLayout::Single);
        factory->SetJoinSpillSubPartitionPolicy(true, 0, cfg, true, false, 10);
        factory->SetJoinSpillState(state);
        builder.reset(static_cast<HashBuilderOperator *>(factory->CreateOperator(&pool)));
    }

    void AddRows(int32_t logicalRows, int32_t nonNullRows, int32_t vectorRows = 0)
    {
        if (vectorRows == 0) vectorRows = logicalRows;
        auto *batch = new VectorBatch(logicalRows);
        auto *keys = new Vector<int64_t>(vectorRows, &pool);
        auto *values = new Vector<int64_t>(vectorRows, &pool);
        for (int32_t row = 0; row < nonNullRows; ++row) {
            keys->SetValue(row, row);
        }
        if (nonNullRows < vectorRows) keys->SetNulls(nonNullRows, true, vectorRows - nonNullRows);
        batch->Append(keys);
        batch->Append(values);
        builder->AddInput(batch);
        pool.releaseUnusedReservation();
    }

    void EndBuild()
    {
        builder->setNoMoreInput(true);
        VectorBatch *output = nullptr;
        builder->GetOutput(&output);
        EXPECT_EQ(output, nullptr);
        EXPECT_LE(listener.peak, listener.limit);
    }

    void CheckCleanRelease()
    {
        builder.reset();
        factory.reset();
        state.reset();
        EXPECT_EQ(pool.currentBytes(), 0);
        EXPECT_EQ(listener.billed, pool.reservedBytes());
        pool.releaseUnusedReservation();
        EXPECT_EQ(listener.billed, 0);
    }
};

TEST(JoinSpillNullReservationTest, IgnoredNullSkewKeepsResidentHandoffUnderUnchangedBudget)
{
    NullReservationFixture f(omniruntime::OMNI_JOIN_TYPE_LEFT_SEMI);
    f.AddRows(1 << 20, 64);
    f.EndBuild();
    EXPECT_EQ(f.listener.denied, 0);
    EXPECT_EQ(f.state->GetRuntimeLayout(), JoinBuildRuntimeLayout::Single);
    EXPECT_EQ(f.state->BuildSpilledRows(), 0U);
    JoinHandoffSnapshot snapshot;
    ASSERT_TRUE(f.state->TryGetHandoffForProbe(&snapshot));
    EXPECT_EQ(snapshot.numDistinct, 64U);
    f.CheckCleanRelease();
}

TEST(JoinSpillNullReservationTest, AllIgnoredNullKeysNeedNoRowReferenceArena)
{
    NullReservationFixture f;
    f.AddRows(1 << 20, 0);
    f.EndBuild();
    EXPECT_EQ(f.listener.denied, 0);
    EXPECT_EQ(f.state->GetRuntimeLayout(), JoinBuildRuntimeLayout::Single);
    EXPECT_EQ(f.state->BuildSpilledRows(), 0U);
    JoinHandoffSnapshot snapshot;
    ASSERT_TRUE(f.state->TryGetHandoffForProbe(&snapshot));
    EXPECT_EQ(snapshot.numDistinct, 0U);
    f.CheckCleanRelease();
}

TEST(JoinSpillNullReservationTest, NonNullBuildStillHonorsReservationDenialAndSpill)
{
    NullReservationFixture f;
    // 1M distinct int64 keys: the 16MB input fits, the table (2M 16-byte slots, ctrl and
    // row references, ~58MB more) does not, so the build must be denied and spill.
    f.listener.limit = 48LL << 20;
    f.AddRows(1 << 20, 1 << 20);
    f.EndBuild();
    EXPECT_GT(f.listener.denied, 0);
    EXPECT_EQ(f.state->GetRuntimeLayout(), JoinBuildRuntimeLayout::Partitioned);
    EXPECT_EQ(f.state->BuildSpilledRows(), 1U << 20);
    f.CheckCleanRelease();
}

TEST(JoinSpillNullReservationTest, BitmapCountUsesLogicalBatchRows)
{
    NullReservationFixture f;
    f.AddRows(16, 16, 64); // Nulls beyond the logical batch are not build rows.
    f.EndBuild();
    EXPECT_EQ(f.listener.denied, 0);
    EXPECT_EQ(f.state->BuildSpilledRows(), 0U);
    JoinHandoffSnapshot snapshot;
    ASSERT_TRUE(f.state->TryGetHandoffForProbe(&snapshot));
    EXPECT_EQ(snapshot.numDistinct, 16U);
    f.CheckCleanRelease();
}


TEST(JoinSpillNullReservationTest, FullJoinRetainedNullRowsStillRequireArenaReservation)
{
    NullReservationFixture f(omniruntime::OMNI_JOIN_TYPE_FULL);
    f.AddRows(1 << 20, 0);
    f.EndBuild();
    EXPECT_GT(f.listener.denied, 0);
    EXPECT_EQ(f.state->GetRuntimeLayout(), JoinBuildRuntimeLayout::Partitioned);
    EXPECT_EQ(f.state->BuildSpilledRows(), 1U << 20);
    f.CheckCleanRelease();
}
} // namespace
