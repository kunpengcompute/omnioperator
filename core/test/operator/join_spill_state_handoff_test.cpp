/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 * Description: Compact JoinSpillState coverage for the upstream branch.
 */
#include <memory>
#include "gtest/gtest.h"
#include "join_spill_dual_run_test_util.h"
#include "operator/join/hash_builder.h"
#include "operator/join/join_spill_state.h"
#include "type/data_type.h"
#include "type/data_types.h"
#include "util/omni_exception.h"
#include "vector/vector.h"
#include "vector/vector_helper.h"
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::vec;
using omniruntime::OMNI_BUILD_LEFT;
using omniruntime::OMNI_BUILD_RIGHT;
using omniruntime::OMNI_JOIN_TYPE_EXISTENCE;
using omniruntime::OMNI_JOIN_TYPE_FULL;
using omniruntime::OMNI_JOIN_TYPE_INNER;
using omniruntime::OMNI_JOIN_TYPE_LEFT;
using omniruntime::OMNI_JOIN_TYPE_RIGHT;
namespace {
DataTypes LongTypes()
{
    return DataTypes({LongType(), LongType()});
}
std::unique_ptr<JoinSpillState> MakeState()
{
    auto state = std::make_unique<JoinSpillState>("/tmp/join-spill-upstream-state", LongTypes(), LongTypes(), 4);
    state->SetInitialRuntimeLayout(JoinBuildRuntimeLayout::Single);
    return state;
}
void ArmFullTable(JoinSpillState *state)
{
    state->SetSpillJoinType(OMNI_JOIN_TYPE_INNER);
    state->SetHashTableHandoff(10, {});
    state->SetTableInHandFullTable();
    state->SetHasMoreProbeInput(true);
    state->SetProbeInNrs(false);
}
} // namespace
TEST(JoinSpillStateHandoffTest, HandoffShapesAndInvalidTransitions)
{
    auto emptyFull = MakeState();
    emptyFull->SetHashTableHandoff(0, {});
    auto emptySnap = emptyFull->WaitHandoffForProbe();
    EXPECT_EQ(0u, emptySnap.numDistinct);
    EXPECT_TRUE(emptySnap.spillPartitionIds.empty());
    EXPECT_EQ(JoinHandoffPhase::RestoreRound, emptyFull->GetHandoffPhase());
    emptyFull->ProbeFinished(0);
    EXPECT_EQ(JoinHandoffPhase::Complete, emptyFull->GetHandoffPhase());
    EXPECT_THROW(emptyFull->SetHashTableHandoff(0, {1}), omniruntime::exception::OmniException);
    EXPECT_THROW(emptyFull->WaitHandoffForProbe(), omniruntime::exception::OmniException);
    auto pathA = MakeState();
    pathA->SetHashTableHandoff(0, {2, 1});
    EXPECT_EQ(JoinHandoffPhase::ProbePathA, pathA->GetHandoffPhase());
    auto pathSnap = pathA->WaitHandoffForProbe();
    EXPECT_EQ(std::vector<uint32_t>({1, 2}), pathSnap.spillPartitionIds);
    EXPECT_THROW(pathA->ProbeFinished(0), omniruntime::exception::OmniException);
    auto full = MakeState();
    EXPECT_THROW(full->SetHashTableHandoff(10, {1}), omniruntime::exception::OmniException);
    EXPECT_THROW(full->SetHashTableHandoff(0, {1}, 7), omniruntime::exception::OmniException);
    full->SetHashTableHandoff(10, {});
    (void)full->WaitHandoffForProbe();
    EXPECT_THROW(full->MarkProbePathAFinished(), omniruntime::exception::OmniException);
}
TEST(JoinSpillStateHandoffTest, PathABlocksRestoreThenAdvancesAscending)
{
    auto state = MakeState();
    state->SetHashTableHandoff(0, {3, 1, 2});
    (void)state->WaitHandoffForProbe();
    EXPECT_FALSE(state->NextRestorePartitionId().has_value());
    state->AppendSpilledHashTablePartitions({5, 2});
    EXPECT_EQ(std::vector<uint32_t>({1, 2, 3, 5}), state->SpillPartitionIds());
    state->MarkProbePathAFinished();
    for (uint32_t expected : {1u, 2u, 3u, 5u}) {
        auto next = state->NextRestorePartitionId();
        ASSERT_TRUE(next.has_value());
        EXPECT_EQ(expected, *next);
        EXPECT_FALSE(state->NextRestorePartitionId().has_value());
        state->SetHashTableHandoff(expected == 2u ? 0u : 10u + expected, {}, expected);
        auto snap = state->WaitHandoffForProbe();
        ASSERT_TRUE(snap.restoredPartitionId.has_value());
        EXPECT_EQ(expected, *snap.restoredPartitionId);
        EXPECT_THROW(state->ProbeFinished(99), omniruntime::exception::OmniException);
        state->ProbeFinished(expected);
    }
    EXPECT_FALSE(state->NextRestorePartitionId().has_value());
    EXPECT_EQ(JoinHandoffPhase::Complete, state->GetHandoffPhase());
    EXPECT_GT(state->HandoffGeneration(), 1u);
}
TEST(JoinSpillStateHandoffTest, ProbeFinishedAndAbandonCompleteState)
{
    auto finished = MakeState();
    finished->SetHashTableHandoff(4, {});
    (void)finished->WaitHandoffForProbe();
    finished->ProbeFinished(0);
    EXPECT_EQ(JoinHandoffPhase::Complete, finished->GetHandoffPhase());
    finished->AbandonProbe();
    EXPECT_EQ(JoinHandoffPhase::Complete, finished->GetHandoffPhase());
    auto abandoned = MakeState();
    abandoned->SetHashTableHandoff(0, {3, 1});
    (void)abandoned->WaitHandoffForProbe();
    abandoned->MarkProbePathAFinished();
    ASSERT_TRUE(abandoned->NextRestorePartitionId().has_value());
    abandoned->AbandonProbe();
    EXPECT_EQ(JoinHandoffPhase::Complete, abandoned->GetHandoffPhase());
    EXPECT_FALSE(abandoned->NextRestorePartitionId().has_value());
    EXPECT_TRUE(abandoned->BuildCanAdvanceRestore());
    abandoned->AbandonProbe();
    EXPECT_THROW(abandoned->WaitHandoffForProbe(), omniruntime::exception::OmniException);
}
TEST(JoinSpillStateHandoffTest, HeldTableGateMatrix)
{
    auto state = MakeState();
    EXPECT_FALSE(state->CanFlushHeldTable());
    state->SetSpillJoinType(OMNI_JOIN_TYPE_INNER);
    state->SetTableInHandFullTable();
    state->SetHasMoreProbeInput(true);
    EXPECT_FALSE(state->CanFlushHeldTable());
    ArmFullTable(state.get());
    EXPECT_TRUE(state->CanFlushHeldTable());
    state->SetProbeInNrs(true);
    EXPECT_FALSE(state->CanFlushHeldTable());
    state->SetProbeInNrs(false);
    state->SetHasMoreProbeInput(false);
    EXPECT_FALSE(state->CanFlushHeldTable());
    state->SetHasMoreProbeInput(true);
    state->SetSpillJoinType(OMNI_JOIN_TYPE_LEFT, OMNI_BUILD_RIGHT); EXPECT_TRUE(state->CanFlushHeldTable());
    state->SetSpillJoinType(OMNI_JOIN_TYPE_RIGHT, OMNI_BUILD_LEFT); EXPECT_TRUE(state->CanFlushHeldTable());
    state->SetSpillJoinType(OMNI_JOIN_TYPE_LEFT, OMNI_BUILD_LEFT); EXPECT_TRUE(state->CanFlushHeldTable());
    state->SetSpillJoinType(OMNI_JOIN_TYPE_RIGHT, OMNI_BUILD_RIGHT); EXPECT_TRUE(state->CanFlushHeldTable());
    state->SetSpillJoinType(OMNI_JOIN_TYPE_FULL, OMNI_BUILD_LEFT); EXPECT_TRUE(state->CanFlushHeldTable());
    state->SetSpillJoinType(OMNI_JOIN_TYPE_LEFT); EXPECT_FALSE(state->CanFlushHeldTable());
    state->SetSpillJoinType(OMNI_JOIN_TYPE_EXISTENCE, OMNI_BUILD_RIGHT); EXPECT_FALSE(state->CanFlushHeldTable());
    auto pathA = MakeState();
    pathA->SetSpillJoinType(OMNI_JOIN_TYPE_INNER);
    pathA->SetHashTableHandoff(0, {1, 2});
    pathA->SetTableInHandFullTable();
    pathA->SetHasMoreProbeInput(true);
    EXPECT_EQ(JoinHandoffPhase::ProbePathA, pathA->GetHandoffPhase());
    EXPECT_FALSE(pathA->CanFlushHeldTable());
}
TEST(JoinSpillStateHandoffTest, HeldFlushTransitions)
{
    auto full = MakeState();
    ArmFullTable(full.get());
    const uint64_t generation = full->HandoffGeneration();
    EXPECT_THROW(full->PublishHeldTableFlush({}), omniruntime::exception::OmniException);
    full->PublishHeldTableFlush({2, 0});
    EXPECT_EQ(JoinHandoffPhase::ProbePathA, full->GetHandoffPhase());
    EXPECT_EQ(std::vector<uint32_t>({0, 2}), full->SpillPartitionIds());
    EXPECT_GT(full->HandoffGeneration(), generation);
    EXPECT_EQ(JoinSpillTableInHandKind::None, full->TableInHand().kind);
    EXPECT_FALSE(full->CanFlushHeldTable());
    auto restore = MakeState();
    restore->SetSpillJoinType(OMNI_JOIN_TYPE_INNER);
    restore->SetHashTableHandoff(0, {2});
    (void)restore->WaitHandoffForProbe();
    restore->MarkProbePathAFinished();
    ASSERT_EQ(2u, *restore->NextRestorePartitionId());
    restore->SetHashTableHandoff(4, {}, 2);
    (void)restore->WaitHandoffForProbe();
    restore->SetTableInHandRestoreSmallTable(2);
    restore->SetHasMoreProbeInput(true);
    EXPECT_TRUE(restore->CanFlushHeldTable());
    restore->PublishHeldTableFlush({2});
    EXPECT_EQ(JoinHandoffPhase::RestoreRound, restore->GetHandoffPhase());
    EXPECT_TRUE(restore->WasRestorePartitionFlushed(2));
    restore->SetTableInHandRestoreSmallTable(2);
    restore->SetHasMoreProbeInput(true);
    EXPECT_FALSE(restore->CanFlushHeldTable());
    EXPECT_THROW(MakeState()->SetTableInHandRestoreSmallTable(4), omniruntime::exception::OmniException);
}
TEST(JoinSpillStateHandoffTest, BuildSpillSchemaAndRoundTrip)
{
    auto schema = MakeState();
    EXPECT_FALSE(schema->NeedsVisitedFlagSpill());
    EXPECT_EQ(2, schema->BuildSpillTypes().GetSize());
    schema->SetSpillJoinType(OMNI_JOIN_TYPE_LEFT, OMNI_BUILD_LEFT);
    EXPECT_TRUE(schema->NeedsVisitedFlagSpill());
    EXPECT_EQ(3, schema->BuildSpillTypes().GetSize());
    EXPECT_EQ(OMNI_BOOLEAN, schema->BuildSpillTypes().GetType(2)->GetId());
    schema->SetSpillJoinType(OMNI_JOIN_TYPE_FULL, OMNI_BUILD_RIGHT);
    EXPECT_TRUE(schema->NeedsVisitedFlagSpill());
    const DataTypes types = LongTypes();
    int32_t hashCols[] = {0};
    auto *factory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
        OMNI_JOIN_TYPE_INNER, types, hashCols, 1, 1);
    auto shared = std::make_shared<JoinSpillState>("/tmp/join-spill-upstream-roundtrip", types, types, 4);
    factory->SetJoinSpillState(shared);
    auto *variants = factory->GetHashTablesVariants();
    auto *batch = join_spill_dual_run::MakeLongKeyValueBatch({100, 101}, {1000, 1001});
    shared->SpillBuildSubPartition(0, 0, batch);
    VectorHelper::FreeVecBatch(batch);
    EXPECT_TRUE(shared->HasPendingBuildSpill(0));
    EXPECT_EQ(2u, shared->BuildSpilledRows());
    shared->CloseBuildWriters();
    shared->RestoreBuildSubPartition(variants, 0);
    EXPECT_FALSE(shared->HasPendingBuildSpill(0));
    constexpr int32_t kRows = 4;
    auto *constBatch = new VectorBatch(kRows);
    auto *keys = new Vector<int64_t>(kRows);
    for (int32_t row = 0; row < kRows; ++row) {
        keys->SetValue(row, row);
    }
    constBatch->Append(keys);
    constBatch->Append(new ConstVector<int64_t>(42, OMNI_LONG, kRows));
    shared->SpillBuildSubPartition(0, 1, constBatch);
    VectorHelper::FreeVecBatch(constBatch);
    shared->CloseBuildWriters();
    shared->RestoreBuildSubPartition(variants, 1);
    EXPECT_FALSE(shared->HasPendingBuildSpill(1));
    delete factory;
}
