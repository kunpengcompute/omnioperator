/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 * Description: Operator-level driver test. Unlike JoinSpillRestoreTest (which manually
 *              alternates build->TryProduceNextRestoreRound() and probe consumption), this test drives
 *              build + probe purely through the Omni native re-pump protocol: repeatedly poll
 *              IsBlocked() and, when kNotBlocked, call GetOutput() on each operator (otherwise yield to
 *              the peer). Build's GetOutput drives the ascending single-bucket restore; probe's
 *              GetOutput/IsBlocked consume each round. A bounded iteration cap
 *              guards against live-lock.
 */
#include <algorithm>
#include <vector>

#include <climits>

#include "gtest/gtest.h"
#include "memory/omni_task_memory_pool.h"
#include <limits>
#include "compute/reason.h"
#include "operator/config/operator_config.h"
#include "operator/join/hash_builder.h"
#include "operator/join/join_spill_state.h"
#include "operator/join/join_sub_partitioner.h"
#include "operator/join/lookup_join.h"
#include "operator/join/lookup_outer_join.h"
#include "operator/util/operator_util.h"
#include "join_spill_dual_run_test_util.h"
#include "type/data_type.h"
#include "type/data_types.h"
#include "vector/vector_batch.h"
#include "vector/vector_helper.h"

using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::vec;
using omniruntime::compute::BlockingReason;
using omniruntime::compute::ContinueFuture;

namespace {

JoinSubPartitionConfig TestSubPartitionConfig()
{
    JoinSubPartitionConfig cfg;
    cfg.numSubPartitions = 4;
    cfg.startPartitionBit = 0;
    cfg.activeSubPartition = 0;
    return cfg;
}

VectorBatch *MakeLongBatch(const std::vector<int64_t> &keys, const std::vector<int64_t> &values)
{
    auto *batch = new VectorBatch(static_cast<int32_t>(keys.size()));
    auto *keyVec = new Vector<int64_t>(static_cast<int32_t>(keys.size()));
    auto *valueVec = new Vector<int64_t>(static_cast<int32_t>(values.size()));
    for (size_t i = 0; i < keys.size(); ++i) {
        keyVec->SetValue(static_cast<int32_t>(i), keys[i]);
        valueVec->SetValue(static_cast<int32_t>(i), values[i]);
    }
    batch->Append(keyVec);
    batch->Append(valueVec);
    return batch;
}

constexpr int32_t kWideColumnCount = 8;

DataTypes WideLongTypes()
{
    std::vector<DataTypePtr> types;
    types.reserve(kWideColumnCount);
    for (int32_t i = 0; i < kWideColumnCount; ++i) {
        types.push_back(LongType());
    }
    return DataTypes(types);
}

VectorBatch *MakeWideLongBatch(const std::vector<int64_t> &keys)
{
    auto *batch = new VectorBatch(static_cast<int32_t>(keys.size()));
    for (int32_t col = 0; col < kWideColumnCount; ++col) {
        auto *vector = new Vector<int64_t>(static_cast<int32_t>(keys.size()));
        for (size_t row = 0; row < keys.size(); ++row) {
            vector->SetValue(static_cast<int32_t>(row), col == 0 ? keys[row] :
                keys[row] * 100 + static_cast<int64_t>(col));
        }
        batch->Append(vector);
    }
    return batch;
}

std::shared_ptr<JoinSpillState> MakeHandoffState()
{
    const DataTypes types({LongType(), LongType()});
    auto state = std::make_shared<JoinSpillState>("/tmp/join-spill-driver-test", types, types, 4);
    state->SetHandoffSessionEnabled(true);
    state->SetInitialRuntimeLayout(JoinBuildRuntimeLayout::Single);
    return state;
}

HashBuilderOperatorFactory *MakeHashFactory(std::shared_ptr<JoinSpillState> state)
{
    const DataTypes types({LongType(), LongType()});
    int32_t hashCols[] = {0};
    auto *factory =
        HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(OMNI_JOIN_TYPE_INNER, types, hashCols, 1, 1);
    factory->SetJoinSpillSubPartitionPolicy(true, 0, TestSubPartitionConfig(), true, false, 10);
    factory->SetJoinSpillState(state);
    return factory;
}

HashBuilderOperatorFactory *MakeOuterHashFactory(std::shared_ptr<JoinSpillState> state, JoinType joinType,
    BuildSide buildSide)
{
    const DataTypes types({LongType(), LongType()});
    int32_t hashCols[] = {0};
    auto *factory =
        HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(joinType, buildSide, types, hashCols, 1, 1);
    factory->SetJoinSpillSubPartitionPolicy(true, 0, TestSubPartitionConfig(), true, false, 10);
    factory->SetJoinSpillState(state);
    return factory;
}

uint32_t HashTableSizeOf(HashBuilderOperatorFactory *factory)
{
    auto *ht = factory->GetHashTablesVariants();
    if (ht == nullptr) {
        return 0;
    }
    return std::visit([](auto &&arg) -> uint32_t { return arg.GetHashTableSize(); }, *ht);
}

LookupJoinOperatorFactory *MakeLookupFactory(HashBuilderOperatorFactory *hashFactory,
    std::shared_ptr<JoinSpillState> state)
{
    const DataTypes types({LongType(), LongType()});
    int32_t probeOutputCols[] = {0, 1};
    int32_t buildOutputCols[] = {0, 1};
    int32_t hashCols[] = {0};
    auto *factory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(types, probeOutputCols, 2, hashCols, 1,
        buildOutputCols, 2, types, reinterpret_cast<int64_t>(hashFactory), nullptr, false, nullptr);
    factory->SetJoinSpillPolicy(true, TestSubPartitionConfig());
    factory->SetJoinSpillState(state);
    return factory;
}

int64_t FindKeyForSubPartition(uint32_t targetSubPartition)
{
    JoinSubPartitioner partitioner(TestSubPartitionConfig());
    const DataTypes types({LongType(), LongType()});
    const std::vector<int32_t> hashCols = {0};
    for (int64_t key = 0; key < 10000; ++key) {
        auto *batch = MakeLongBatch({key}, {0});
        const auto buckets = partitioner.PartitionRowsByKeyColumns(batch, types, hashCols);
        VectorHelper::FreeVecBatch(batch);
        if (!buckets.empty() && buckets[0] == targetSubPartition) {
            return key;
        }
    }
    return -1;
}

int64_t FindSecondKeyForSubPartition(uint32_t targetSubPartition, int64_t excludeKey)
{
    JoinSubPartitioner partitioner(TestSubPartitionConfig());
    const DataTypes types({LongType(), LongType()});
    const std::vector<int32_t> hashCols = {0};
    for (int64_t key = 0; key < 50000; ++key) {
        if (key == excludeKey) {
            continue;
        }
        auto *batch = MakeLongBatch({key}, {0});
        const auto buckets = partitioner.PartitionRowsByKeyColumns(batch, types, hashCols);
        VectorHelper::FreeVecBatch(batch);
        if (!buckets.empty() && buckets[0] == targetSubPartition) {
            return key;
        }
    }
    return -1;
}

struct JoinedRow {
    int64_t buildKey;
    int64_t buildValue;
};

} // namespace

// Drive build + probe through the IsBlocked/GetOutput re-pump protocol (no manual round alternation,
// no manual TryProduceNextRestoreRound / ProbeFinished). Build k1->bucket1,
// {k2,k3}->bucket2; probe {k1,k1}->bucket1, {k2}->bucket2. Expected join output: round1 bucket1 = 2
// rows (k1, value 10); round2 bucket2 = 1 row (k2, value 11); k3 (value 12) unmatched.
TEST(JoinSpillDriverTest, RePumpDrivesAscendingRestoreToCompletion)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    auto state = MakeHandoffState();
    auto *hashFactory = MakeHashFactory(state);

    const int64_t k1 = FindKeyForSubPartition(1);
    const int64_t k2 = FindKeyForSubPartition(2);
    const int64_t k3 = FindSecondKeyForSubPartition(2u, k2);
    ASSERT_GE(k1, 0);
    ASSERT_GE(k2, 0);
    ASSERT_GE(k3, 0);

    auto *build = dynamic_cast<HashBuilderOperator *>(hashFactory->CreateOperator(&pool));
    auto *lookupFactory = MakeLookupFactory(hashFactory, state);
    auto *probe = dynamic_cast<LookupJoinOperator *>(lookupFactory->CreateOperator(&pool));

    // Build ingest. Without a forced spill the resident build stays single-slot; GetOutput
    // publishes the full-table handoff and the driver re-pumps restore rounds afterwards.
    build->setNoMoreInput(false);
    build->AddInput(MakeLongBatch({k1, k2, k3}, {10, 11, 12}));
    build->reclaim(std::numeric_limits<int64_t>::max());
    build->setNoMoreInput(true);

    std::vector<JoinedRow> outRows;
    uint32_t maxRestoreTableCount = 0;
    bool probeFed = false;
    int iter = 0;
    const int kMaxIter = 100000; // live-lock guard.
    for (; iter < kMaxIter; ++iter) {
        // ----- pump build -----
        if (!build->isFinished()) {
            ContinueFuture bf;
            const auto buildReason = build->IsBlocked(&bf);
            if (buildReason == BlockingReason::kNotBlocked) {
                VectorBatch *out = nullptr;
                build->GetOutput(&out);
                if (out != nullptr) {
                    VectorHelper::FreeVecBatch(out);
                }
                if (state->GetHandoffPhase() == JoinHandoffPhase::RestoreRound) {
                    maxRestoreTableCount =
                        std::max(maxRestoreTableCount, build->GetHashTableCount());
                }
            }
        }

        // Feed the probe its rows exactly when Path A is published (mirrors a probe pipeline that only
        // ingests once the build handoff is live; AddInput then mirrors rows to probe_spill on disk).
        if (!probeFed && state->GetHandoffPhase() == JoinHandoffPhase::ProbePathA) {
            probe->setNoMoreInput(false);
            probe->AddInput(MakeLongBatch({k1, k1, k2}, {201, 202, 203}));
            probe->setNoMoreInput(true);
            probeFed = true;
        }

        // ----- pump probe ----- (probe status FINISHED after Path A is a soft signal; the next
        // handoff pickup in IsBlocked/GetOutput re-activates it, so we gate on IsBlocked, not isFinished.)
        if (probeFed) {
            ContinueFuture pf;
            if (probe->IsBlocked(&pf) == BlockingReason::kNotBlocked) {
                VectorBatch *out = nullptr;
                probe->GetOutput(&out);
                if (out != nullptr) {
                    // Output layout: probe[key=0,value=1] then build[key=2,value=3]. Build columns are
                    // materialized flat Vector<int64_t>, so reading them is encoding-safe.
                    auto *buildKey = reinterpret_cast<Vector<int64_t> *>(out->Get(2));
                    auto *buildValue = reinterpret_cast<Vector<int64_t> *>(out->Get(3));
                    for (int32_t i = 0; i < out->GetRowCount(); ++i) {
                        outRows.push_back(JoinedRow{buildKey->GetValue(i), buildValue->GetValue(i)});
                    }
                    VectorHelper::FreeVecBatch(out);
                }
            }
        }

        if (state->GetHandoffPhase() == JoinHandoffPhase::Complete && build->isFinished() &&
            probe->isFinished()) {
            break;
        }
    }

    // No live-lock: the protocol terminated well within the iteration bound.
    EXPECT_LT(iter, kMaxIter);
    EXPECT_EQ(JoinHandoffPhase::Complete, state->GetHandoffPhase());
    EXPECT_TRUE(build->isFinished());
    EXPECT_TRUE(probe->isFinished());

    // Join output: 2 rows paired to build (k1, 10) and 1 row paired to build (k2, 11); k3 unmatched.
    int k1Rows = 0;
    int k2Rows = 0;
    for (const auto &row : outRows) {
        if (row.buildKey == k1) {
            ++k1Rows;
            EXPECT_EQ(10, row.buildValue);
        } else if (row.buildKey == k2) {
            ++k2Rows;
            EXPECT_EQ(11, row.buildValue);
        } else {
            ADD_FAILURE() << "unexpected build key in join output: " << row.buildKey;
        }
    }
    EXPECT_EQ(2, k1Rows);
    EXPECT_EQ(1, k2Rows);
    EXPECT_EQ(3u, outRows.size());

    // Single-bucket restore: at most one small build table resident per round (the memory win vs
    // replaying every spilled bucket into memory at once).
    EXPECT_GE(maxRestoreTableCount, 1u);
    EXPECT_LE(maxRestoreTableCount, 1u);

    omniruntime::op::Operator::DeleteOperator(probe);
    omniruntime::op::Operator::DeleteOperator(build);
    delete lookupFactory;
    delete hashFactory;
}

// Empty reducer (Velox): no build rows, nothing spilled → SetHashTableHandoff(0, empty ids) as
// full-table (phase=RestoreRound). Re-pump must reach Complete without Path A / restore rounds.
// INNER: probe rows Find-miss → zero join output.
TEST(JoinSpillDriverTest, EmptyFullTableInner_RePumpToComplete)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    auto state = MakeHandoffState();
    auto *hashFactory = MakeHashFactory(state);
    auto *build = dynamic_cast<HashBuilderOperator *>(hashFactory->CreateOperator(&pool));
    auto *lookupFactory = MakeLookupFactory(hashFactory, state);
    auto *probe = dynamic_cast<LookupJoinOperator *>(lookupFactory->CreateOperator(&pool));

    // Empty build reducer: no AddInput, only close.
    build->setNoMoreInput(false);
    build->setNoMoreInput(true);

    int64_t outRows = 0;
    bool probeFed = false;
    bool sawEmptyFullTableHandoff = false;
    int iter = 0;
    const int kMaxIter = 100000;
    for (; iter < kMaxIter; ++iter) {
        if (!build->isFinished()) {
            ContinueFuture bf;
            if (build->IsBlocked(&bf) == BlockingReason::kNotBlocked) {
                VectorBatch *out = nullptr;
                build->GetOutput(&out);
                if (out != nullptr) {
                    VectorHelper::FreeVecBatch(out);
                }
            }
        }

        JoinHandoffSnapshot snap;
        if (!sawEmptyFullTableHandoff && state->TryGetHandoffForProbe(&snap) && snap.numDistinct == 0 &&
            snap.spillPartitionIds.empty() && !snap.restoredPartitionId.has_value()) {
            EXPECT_EQ(JoinHandoffPhase::RestoreRound, state->GetHandoffPhase());
            EXPECT_NE(JoinHandoffPhase::ProbePathA, state->GetHandoffPhase());
            sawEmptyFullTableHandoff = true;
        }

        // Full-table / empty-reducer publishes RestoreRound (not ProbePathA) — feed then.
        if (!probeFed && state->GetHandoffPhase() == JoinHandoffPhase::RestoreRound) {
            probe->setNoMoreInput(false);
            probe->AddInput(MakeLongBatch({1, 2, 3}, {201, 202, 203}));
            probe->setNoMoreInput(true);
            probeFed = true;
        }

        if (probeFed) {
            ContinueFuture pf;
            if (probe->IsBlocked(&pf) == BlockingReason::kNotBlocked) {
                VectorBatch *out = nullptr;
                probe->GetOutput(&out);
                if (out != nullptr) {
                    outRows += out->GetRowCount();
                    VectorHelper::FreeVecBatch(out);
                }
            }
        }

        if (state->GetHandoffPhase() == JoinHandoffPhase::Complete && build->isFinished() &&
            probe->isFinished()) {
            break;
        }
    }

    EXPECT_LT(iter, kMaxIter);
    EXPECT_TRUE(sawEmptyFullTableHandoff);
    EXPECT_EQ(JoinHandoffPhase::Complete, state->GetHandoffPhase());
    EXPECT_TRUE(build->isFinished());
    EXPECT_TRUE(probe->isFinished());
    EXPECT_EQ(0, outRows); // INNER + empty build → no matches
    EXPECT_EQ(0u, state->BuildSpilledRows());

    omniruntime::op::Operator::DeleteOperator(probe);
    omniruntime::op::Operator::DeleteOperator(build);
    delete lookupFactory;
    delete hashFactory;
}

// Same empty full-table handoff; LEFT Opposite (build=RIGHT) must null-pad every probe row and
// still ProbeFinished → Complete (not MarkIdle-discard, not Path A).
TEST(JoinSpillDriverTest, EmptyFullTableLeftOuter_RePumpNullPadsProbe)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    auto state = MakeHandoffState();
    auto *hashFactory = MakeOuterHashFactory(state, OMNI_JOIN_TYPE_LEFT, OMNI_BUILD_RIGHT);
    auto *build = dynamic_cast<HashBuilderOperator *>(hashFactory->CreateOperator(&pool));
    auto *lookupFactory = MakeLookupFactory(hashFactory, state);
    auto *probe = dynamic_cast<LookupJoinOperator *>(lookupFactory->CreateOperator(&pool));

    build->setNoMoreInput(false);
    build->setNoMoreInput(true);

    int32_t nullPaddedRows = 0;
    int32_t pairedRows = 0;
    bool probeFed = false;
    int iter = 0;
    const int kMaxIter = 100000;
    for (; iter < kMaxIter; ++iter) {
        if (!build->isFinished()) {
            ContinueFuture bf;
            if (build->IsBlocked(&bf) == BlockingReason::kNotBlocked) {
                VectorBatch *out = nullptr;
                build->GetOutput(&out);
                if (out != nullptr) {
                    VectorHelper::FreeVecBatch(out);
                }
            }
        }

        if (!probeFed && state->GetHandoffPhase() == JoinHandoffPhase::RestoreRound) {
            JoinHandoffSnapshot snap;
            ASSERT_TRUE(state->TryGetHandoffForProbe(&snap));
            EXPECT_EQ(0u, snap.numDistinct);
            EXPECT_TRUE(snap.spillPartitionIds.empty());

            probe->setNoMoreInput(false);
            probe->AddInput(MakeLongBatch({7, 8}, {301, 302}));
            probe->setNoMoreInput(true);
            probeFed = true;
        }

        if (probeFed) {
            ContinueFuture pf;
            if (probe->IsBlocked(&pf) == BlockingReason::kNotBlocked) {
                VectorBatch *out = nullptr;
                probe->GetOutput(&out);
                if (out != nullptr) {
                    // LEFT + build=RIGHT: probe[0,1] then build[2,3]. Use BaseVector::IsNull
                    // (encoding-safe) — flat Vector<int64_t>* cast can SEGV on null-padded cols.
                    auto *buildKey = out->Get(2);
                    ASSERT_NE(nullptr, buildKey);
                    for (int32_t i = 0; i < out->GetRowCount(); ++i) {
                        if (buildKey->IsNull(i)) {
                            ++nullPaddedRows;
                        } else {
                            ++pairedRows;
                        }
                    }
                    VectorHelper::FreeVecBatch(out);
                }
            }
        }

        if (state->GetHandoffPhase() == JoinHandoffPhase::Complete && build->isFinished() &&
            probe->isFinished()) {
            break;
        }
    }

    EXPECT_LT(iter, kMaxIter);
    EXPECT_EQ(JoinHandoffPhase::Complete, state->GetHandoffPhase());
    EXPECT_TRUE(build->isFinished());
    EXPECT_TRUE(probe->isFinished());
    EXPECT_EQ(2, nullPaddedRows);
    EXPECT_EQ(0, pairedRows);
    EXPECT_EQ(0u, state->BuildSpilledRows());

    omniruntime::op::Operator::DeleteOperator(probe);
    omniruntime::op::Operator::DeleteOperator(build);
    delete lookupFactory;
    delete hashFactory;
}

TEST(JoinSpillDriverTest, EmptyFullTableRightBuildLeft_RePumpNullPadsProbe)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    auto state = MakeHandoffState();
    auto *hashFactory = MakeOuterHashFactory(state, OMNI_JOIN_TYPE_RIGHT, OMNI_BUILD_LEFT);
    auto *build = dynamic_cast<HashBuilderOperator *>(hashFactory->CreateOperator(&pool));
    auto *lookupFactory = MakeLookupFactory(hashFactory, state);
    auto *probe = dynamic_cast<LookupJoinOperator *>(lookupFactory->CreateOperator(&pool));

    build->setNoMoreInput(false);
    build->setNoMoreInput(true);

    int32_t nullPaddedRows = 0;
    bool probeFed = false;
    int iter = 0;
    const int kMaxIter = 100000;
    for (; iter < kMaxIter; ++iter) {
        if (!build->isFinished()) {
            ContinueFuture bf;
            if (build->IsBlocked(&bf) == BlockingReason::kNotBlocked) {
                VectorBatch *out = nullptr;
                build->GetOutput(&out);
                if (out != nullptr) {
                    VectorHelper::FreeVecBatch(out);
                }
            }
        }

        if (!probeFed && state->GetHandoffPhase() == JoinHandoffPhase::RestoreRound) {
            probe->setNoMoreInput(false);
            probe->AddInput(MakeLongBatch({7, 8}, {301, 302}));
            probe->setNoMoreInput(true);
            probeFed = true;
        }
        if (probeFed) {
            ContinueFuture pf;
            if (probe->IsBlocked(&pf) == BlockingReason::kNotBlocked) {
                VectorBatch *out = nullptr;
                probe->GetOutput(&out);
                if (out != nullptr) {
                    // RIGHT + build=LEFT: build columns are physically first and must be NULL.
                    auto *buildKey = out->Get(0);
                    ASSERT_NE(nullptr, buildKey);
                    for (int32_t row = 0; row < out->GetRowCount(); ++row) {
                        if (buildKey->IsNull(row)) {
                            ++nullPaddedRows;
                        }
                    }
                    VectorHelper::FreeVecBatch(out);
                }
            }
        }
        if (state->GetHandoffPhase() == JoinHandoffPhase::Complete && build->isFinished() &&
            probe->isFinished()) {
            break;
        }
    }

    EXPECT_LT(iter, kMaxIter);
    EXPECT_EQ(JoinHandoffPhase::Complete, state->GetHandoffPhase());
    EXPECT_EQ(2, nullPaddedRows);

    omniruntime::op::Operator::DeleteOperator(probe);
    omniruntime::op::Operator::DeleteOperator(build);
    delete lookupFactory;
    delete hashFactory;
}

TEST(JoinSpillDriverTest, FullOuterFullTable_DefersCompletionUntilUnmatchedBuildDrain)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    auto state = MakeHandoffState();
    auto *hashFactory = MakeOuterHashFactory(state, OMNI_JOIN_TYPE_FULL, OMNI_BUILD_LEFT);
    // Keep the table in memory so this is the full-table handoff, not Path A.
    auto *build = dynamic_cast<HashBuilderOperator *>(hashFactory->CreateOperator(&pool));
    const int64_t matched = FindKeyForSubPartition(1);
    const int64_t unmatched = FindSecondKeyForSubPartition(1, matched);
    ASSERT_GE(matched, 0);
    ASSERT_GE(unmatched, 0);

    build->setNoMoreInput(false);
    build->AddInput(MakeLongBatch({matched, unmatched}, {10, 19}));
    build->setNoMoreInput(true);
    VectorBatch *buildOutput = nullptr;
    build->GetOutput(&buildOutput);
    EXPECT_EQ(JoinHandoffPhase::RestoreRound, state->GetHandoffPhase());

    auto *lookupFactory = MakeLookupFactory(hashFactory, state);
    auto *probe = dynamic_cast<LookupJoinOperator *>(lookupFactory->CreateOperator(&pool));
    probe->setNoMoreInput(false);
    probe->AddInput(MakeLongBatch({matched}, {201}));
    probe->setNoMoreInput(true);
    int32_t pairedRows = 0;
    for (int i = 0; i < 100000 && !probe->isFinished(); ++i) {
        VectorBatch *out = nullptr;
        probe->GetOutput(&out);
        if (out != nullptr) {
            pairedRows += out->GetRowCount();
            VectorHelper::FreeVecBatch(out);
        }
    }
    EXPECT_EQ(1, pairedRows);
    EXPECT_TRUE(probe->isFinished());
    // The shared build table must stay alive until the wrapper-owned outer scan is drained.
    EXPECT_EQ(JoinHandoffPhase::RestoreRound, state->GetHandoffPhase());
    EXPECT_FALSE(build->isFinished());
    EXPECT_GT(HashTableSizeOf(hashFactory), 0u);

    const DataTypes types({LongType(), LongType()});
    int32_t probeOutputCols[] = {0, 1};
    int32_t buildOutputCols[] = {0, 1};
    auto *outerFactory = LookupOuterJoinOperatorFactory::CreateLookupOuterJoinOperatorFactory(types,
        probeOutputCols, 2, buildOutputCols, types, reinterpret_cast<int64_t>(hashFactory), OMNI_BUILD_LEFT);
    auto *outer = dynamic_cast<LookupOuterJoinOperator *>(outerFactory->CreateOperator(&pool));
    VectorBatch *outerOutput = nullptr;
    outer->GetOutput(&outerOutput);
    ASSERT_NE(nullptr, outerOutput);
    ASSERT_EQ(1, outerOutput->GetRowCount());
    EXPECT_EQ(unmatched, reinterpret_cast<Vector<int64_t> *>(outerOutput->Get(0))->GetValue(0));
    EXPECT_TRUE(outerOutput->Get(2)->IsNull(0));
    VectorHelper::FreeVecBatch(outerOutput);

    probe->FinishFullTableProbeAfterOuter();
    EXPECT_EQ(JoinHandoffPhase::Complete, state->GetHandoffPhase());
    build->GetOutput(&buildOutput);
    EXPECT_TRUE(build->isFinished());
    EXPECT_EQ(0u, HashTableSizeOf(hashFactory));

    omniruntime::op::Operator::DeleteOperator(outer);
    omniruntime::op::Operator::DeleteOperator(probe);
    omniruntime::op::Operator::DeleteOperator(build);
    delete outerFactory;
    delete lookupFactory;
    delete hashFactory;
}

struct FullTableBuildPreservingParam {
    JoinType joinType;
    BuildSide buildSide;
    const char *name;
};

class JoinSpillFullTableBuildPreservingTest : public testing::TestWithParam<FullTableBuildPreservingParam> {};

TEST_P(JoinSpillFullTableBuildPreservingTest, HandoffDefersCompletionUntilUnmatchedBuildDrain)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    const auto param = GetParam();
    auto state = MakeHandoffState();
    auto *hashFactory = MakeOuterHashFactory(state, param.joinType, param.buildSide);
    // Full-table handoff: retain the resident table and publish no Path A ids.
    auto *build = dynamic_cast<HashBuilderOperator *>(hashFactory->CreateOperator(&pool));
    const int64_t matched = FindKeyForSubPartition(1);
    const int64_t unmatched = FindSecondKeyForSubPartition(1, matched);
    ASSERT_GE(matched, 0);
    ASSERT_GE(unmatched, 0);

    build->setNoMoreInput(false);
    build->AddInput(MakeLongBatch({matched, unmatched}, {10, 19}));
    build->setNoMoreInput(true);
    VectorBatch *buildOutput = nullptr;
    build->GetOutput(&buildOutput);

    JoinHandoffSnapshot snap;
    ASSERT_TRUE(state->TryGetHandoffForProbe(&snap));
    EXPECT_GT(snap.numDistinct, 0u);
    EXPECT_TRUE(snap.spillPartitionIds.empty());
    EXPECT_FALSE(snap.restoredPartitionId.has_value());
    EXPECT_EQ(0u, state->BuildSpilledRows());
    EXPECT_EQ(JoinHandoffPhase::RestoreRound, state->GetHandoffPhase());

    auto *lookupFactory = MakeLookupFactory(hashFactory, state);
    auto *probe = dynamic_cast<LookupJoinOperator *>(lookupFactory->CreateOperator(&pool));
    probe->setNoMoreInput(false);
    probe->AddInput(MakeLongBatch({matched}, {201}));
    probe->setNoMoreInput(true);

    int32_t pairedRows = 0;
    for (int32_t guard = 0; guard < 100000 && !probe->isFinished(); ++guard) {
        VectorBatch *out = nullptr;
        probe->GetOutput(&out);
        if (out != nullptr) {
            const int32_t buildKeyCol = param.buildSide == OMNI_BUILD_LEFT ? 0 : 2;
            const int32_t probeKeyCol = param.buildSide == OMNI_BUILD_LEFT ? 2 : 0;
            for (int32_t row = 0; row < out->GetRowCount(); ++row) {
                EXPECT_FALSE(out->Get(buildKeyCol)->IsNull(row));
                EXPECT_FALSE(out->Get(probeKeyCol)->IsNull(row));
                EXPECT_EQ(matched, VectorHelper::GetValueFromVector<int64_t>(out->Get(buildKeyCol), row));
                EXPECT_EQ(matched, VectorHelper::GetValueFromVector<int64_t>(out->Get(probeKeyCol), row));
                ++pairedRows;
            }
            VectorHelper::FreeVecBatch(out);
        }
    }
    EXPECT_TRUE(probe->isFinished());
    EXPECT_EQ(1, pairedRows);
    // Build-preserving full-table handoff must retain the table for the outer scan.
    EXPECT_EQ(JoinHandoffPhase::RestoreRound, state->GetHandoffPhase());
    EXPECT_FALSE(build->isFinished());
    EXPECT_GT(HashTableSizeOf(hashFactory), 0u);

    const DataTypes types({LongType(), LongType()});
    int32_t probeOutputCols[] = {0, 1};
    int32_t buildOutputCols[] = {0, 1};
    auto *outerFactory = LookupOuterJoinOperatorFactory::CreateLookupOuterJoinOperatorFactory(types,
        probeOutputCols, 2, buildOutputCols, types, reinterpret_cast<int64_t>(hashFactory), param.buildSide);
    auto *outer = dynamic_cast<LookupOuterJoinOperator *>(outerFactory->CreateOperator(&pool));
    int32_t unmatchedBuildRows = 0;
    for (int32_t guard = 0; guard < 100000 && !outer->isFinished(); ++guard) {
        VectorBatch *out = nullptr;
        outer->GetOutput(&out);
        if (out == nullptr) {
            continue;
        }
        const int32_t buildKeyCol = param.buildSide == OMNI_BUILD_LEFT ? 0 : 2;
        const int32_t probeKeyCol = param.buildSide == OMNI_BUILD_LEFT ? 2 : 0;
        for (int32_t row = 0; row < out->GetRowCount(); ++row) {
            EXPECT_TRUE(out->Get(probeKeyCol)->IsNull(row));
            EXPECT_EQ(unmatched, VectorHelper::GetValueFromVector<int64_t>(out->Get(buildKeyCol), row));
            ++unmatchedBuildRows;
        }
        VectorHelper::FreeVecBatch(out);
    }
    EXPECT_TRUE(outer->isFinished());
    EXPECT_EQ(1, unmatchedBuildRows);

    // This is the callback made by LookupJoinWrapper after its full-table outer scan drains.
    probe->FinishFullTableProbeAfterOuter();
    EXPECT_EQ(JoinHandoffPhase::Complete, state->GetHandoffPhase());
    build->GetOutput(&buildOutput);
    EXPECT_TRUE(build->isFinished());
    EXPECT_EQ(0u, HashTableSizeOf(hashFactory));

    omniruntime::op::Operator::DeleteOperator(outer);
    omniruntime::op::Operator::DeleteOperator(probe);
    omniruntime::op::Operator::DeleteOperator(build);
    delete outerFactory;
    delete lookupFactory;
    delete hashFactory;
}

INSTANTIATE_TEST_SUITE_P(MissingFullTableHandoffMatrix, JoinSpillFullTableBuildPreservingTest,
    testing::Values(
        FullTableBuildPreservingParam{OMNI_JOIN_TYPE_LEFT, OMNI_BUILD_LEFT, "LeftBuildLeft"},
        FullTableBuildPreservingParam{OMNI_JOIN_TYPE_RIGHT, OMNI_BUILD_RIGHT, "RightBuildRight"},
        FullTableBuildPreservingParam{OMNI_JOIN_TYPE_FULL, OMNI_BUILD_RIGHT, "FullBuildRight"}),
    [](const testing::TestParamInfo<FullTableBuildPreservingParam> &info) { return info.param.name; });

TEST(JoinSpillDriverTest, NoSessionFull_ReleasesBuildStateOnlyAfterOuter)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    const DataTypes types({LongType(), LongType()});
    int32_t hashCols[] = {0};
    auto *hashFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
        OMNI_JOIN_TYPE_FULL, OMNI_BUILD_LEFT, types, hashCols, 1, 1);
    auto *build = dynamic_cast<HashBuilderOperator *>(hashFactory->CreateOperator(&pool));
    build->setNoMoreInput(false);
    build->AddInput(MakeLongBatch({1, 2}, {10, 19}));
    build->setNoMoreInput(true);
    VectorBatch *buildOutput = nullptr;
    build->GetOutput(&buildOutput);
    EXPECT_GT(HashTableSizeOf(hashFactory), 0u);

    int32_t probeOutputCols[] = {0, 1};
    int32_t buildOutputCols[] = {0, 1};
    auto *lookupFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(types, probeOutputCols, 2,
        hashCols, 1, buildOutputCols, 2, types, reinterpret_cast<int64_t>(hashFactory), nullptr, false, nullptr);
    auto *probe = dynamic_cast<LookupJoinOperator *>(lookupFactory->CreateOperator(&pool));
    probe->setNoMoreInput(false);
    probe->AddInput(MakeLongBatch({1}, {201}));
    probe->setNoMoreInput(true);
    for (int32_t i = 0; i < 100000 && !probe->isFinished(); ++i) {
        VectorBatch *out = nullptr;
        probe->GetOutput(&out);
        if (out != nullptr) {
            VectorHelper::FreeVecBatch(out);
        }
    }
    EXPECT_TRUE(probe->isFinished());
    EXPECT_GT(HashTableSizeOf(hashFactory), 0u);

    auto *outerFactory = LookupOuterJoinOperatorFactory::CreateLookupOuterJoinOperatorFactory(types, probeOutputCols, 2,
        buildOutputCols, types, reinterpret_cast<int64_t>(hashFactory), OMNI_BUILD_LEFT);
    auto *outer = dynamic_cast<LookupOuterJoinOperator *>(outerFactory->CreateOperator(&pool));
    for (int32_t i = 0; i < 100000 && !outer->isFinished(); ++i) {
        VectorBatch *out = nullptr;
        outer->GetOutput(&out);
        if (out != nullptr) {
            VectorHelper::FreeVecBatch(out);
        }
    }
    EXPECT_TRUE(outer->isFinished());
    EXPECT_GT(HashTableSizeOf(hashFactory), 0u);

    probe->FinishFullTableProbeAfterOuter();
    EXPECT_EQ(0u, HashTableSizeOf(hashFactory));

    omniruntime::op::Operator::DeleteOperator(outer);
    omniruntime::op::Operator::DeleteOperator(probe);
    omniruntime::op::Operator::DeleteOperator(build);
    delete outerFactory;
    delete lookupFactory;
    delete hashFactory;
}

TEST(JoinSpillDriverTest, NoSessionInner_ReleasesBuildStateWhenProbeFinishes)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    const DataTypes types({LongType(), LongType()});
    int32_t hashCols[] = {0};
    auto *hashFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
        OMNI_JOIN_TYPE_INNER, types, hashCols, 1, 1);
    auto *build = dynamic_cast<HashBuilderOperator *>(hashFactory->CreateOperator(&pool));
    build->setNoMoreInput(false);
    build->AddInput(MakeLongBatch({1}, {10}));
    build->setNoMoreInput(true);
    VectorBatch *buildOutput = nullptr;
    build->GetOutput(&buildOutput);
    EXPECT_GT(HashTableSizeOf(hashFactory), 0u);

    int32_t probeOutputCols[] = {0, 1};
    int32_t buildOutputCols[] = {0, 1};
    auto *lookupFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(types, probeOutputCols, 2,
        hashCols, 1, buildOutputCols, 2, types, reinterpret_cast<int64_t>(hashFactory), nullptr, false, nullptr);
    auto *probe = dynamic_cast<LookupJoinOperator *>(lookupFactory->CreateOperator(&pool));
    probe->setNoMoreInput(false);
    probe->AddInput(MakeLongBatch({1}, {201}));
    probe->setNoMoreInput(true);
    for (int32_t i = 0; i < 100000 && !probe->isFinished(); ++i) {
        VectorBatch *out = nullptr;
        probe->GetOutput(&out);
        if (out != nullptr) {
            VectorHelper::FreeVecBatch(out);
        }
    }
    EXPECT_TRUE(probe->isFinished());
    EXPECT_EQ(0u, HashTableSizeOf(hashFactory));

    omniruntime::op::Operator::DeleteOperator(probe);
    omniruntime::op::Operator::DeleteOperator(build);
    delete lookupFactory;
    delete hashFactory;
}

// Non-empty in-memory full-table: join-spill/handoff on, but no group spill is forced. Build
// publishes RestoreRound with numDistinct>0 and empty spill ids. The shared hash
// table must stay resident and Build must not finish until ProbeFinished — otherwise probe
// Find hits a torn-down table (Q85 hang).
TEST(JoinSpillDriverTest, NonEmptyFullTableInner_StaysResidentUntilProbeFinished)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    auto state = MakeHandoffState();
    auto *hashFactory = MakeHashFactory(state);
    auto *build = dynamic_cast<HashBuilderOperator *>(hashFactory->CreateOperator(&pool));
    auto *lookupFactory = MakeLookupFactory(hashFactory, state);
    auto *probe = dynamic_cast<LookupJoinOperator *>(lookupFactory->CreateOperator(&pool));

    const int64_t kMatch = FindKeyForSubPartition(1);
    const int64_t kMiss = FindKeyForSubPartition(2);
    ASSERT_GE(kMatch, 0);
    ASSERT_GE(kMiss, 0);

    build->setNoMoreInput(false);
    build->AddInput(MakeLongBatch({kMatch}, {10}));
    build->setNoMoreInput(true);

    std::vector<JoinedRow> outRows;
    bool probeFed = false;
    bool sawNonEmptyFullTable = false;
    bool buildStayedAliveDuringProbe = false;
    int iter = 0;
    const int kMaxIter = 100000;
    for (; iter < kMaxIter; ++iter) {
        if (!build->isFinished()) {
            ContinueFuture bf;
            if (build->IsBlocked(&bf) == BlockingReason::kNotBlocked) {
                VectorBatch *out = nullptr;
                build->GetOutput(&out);
                if (out != nullptr) {
                    VectorHelper::FreeVecBatch(out);
                }
            }
        }

        JoinHandoffSnapshot snap;
        if (!sawNonEmptyFullTable && state->TryGetHandoffForProbe(&snap) && snap.numDistinct > 0 &&
            snap.spillPartitionIds.empty() && !snap.restoredPartitionId.has_value()) {
            EXPECT_EQ(JoinHandoffPhase::RestoreRound, state->GetHandoffPhase());
            EXPECT_NE(JoinHandoffPhase::ProbePathA, state->GetHandoffPhase());
            EXPECT_FALSE(build->isFinished());
            EXPECT_GE(build->GetHashTableCount(), 1u);
            EXPECT_EQ(0u, state->BuildSpilledRows());
            sawNonEmptyFullTable = true;
        }

        if (!probeFed && state->GetHandoffPhase() == JoinHandoffPhase::RestoreRound) {
            EXPECT_FALSE(build->isFinished());
            probe->setNoMoreInput(false);
            probe->AddInput(MakeLongBatch({kMatch, kMatch, kMiss}, {201, 202, 203}));
            probe->setNoMoreInput(true);
            probeFed = true;
        }

        if (probeFed && state->GetHandoffPhase() == JoinHandoffPhase::RestoreRound && !build->isFinished()) {
            buildStayedAliveDuringProbe = true;
        }

        if (probeFed) {
            ContinueFuture pf;
            if (probe->IsBlocked(&pf) == BlockingReason::kNotBlocked) {
                VectorBatch *out = nullptr;
                probe->GetOutput(&out);
                if (out != nullptr) {
                    auto *buildKey = reinterpret_cast<Vector<int64_t> *>(out->Get(2));
                    auto *buildValue = reinterpret_cast<Vector<int64_t> *>(out->Get(3));
                    for (int32_t i = 0; i < out->GetRowCount(); ++i) {
                        outRows.push_back(JoinedRow{buildKey->GetValue(i), buildValue->GetValue(i)});
                    }
                    VectorHelper::FreeVecBatch(out);
                }
            }
        }

        if (state->GetHandoffPhase() == JoinHandoffPhase::Complete && build->isFinished() &&
            probe->isFinished()) {
            break;
        }
    }

    EXPECT_LT(iter, kMaxIter);
    EXPECT_TRUE(sawNonEmptyFullTable);
    EXPECT_TRUE(buildStayedAliveDuringProbe);
    EXPECT_EQ(JoinHandoffPhase::Complete, state->GetHandoffPhase());
    EXPECT_TRUE(build->isFinished());
    EXPECT_TRUE(probe->isFinished());
    EXPECT_EQ(0u, state->BuildSpilledRows());
    EXPECT_EQ(0u, HashTableSizeOf(hashFactory));

    int matchRows = 0;
    for (const auto &row : outRows) {
        if (row.buildKey == kMatch) {
            ++matchRows;
            EXPECT_EQ(10, row.buildValue);
        } else {
            ADD_FAILURE() << "unexpected build key in join output: " << row.buildKey;
        }
    }
    EXPECT_EQ(2, matchRows);
    EXPECT_EQ(2u, outRows.size());

    omniruntime::op::Operator::DeleteOperator(probe);
    omniruntime::op::Operator::DeleteOperator(build);
    delete lookupFactory;
    delete hashFactory;
}

// Spill session without a forced spill: Single layout builds only slot0; join matches nospill.
TEST(JoinSpillDriverTest, SpillSessionSingleLayoutFullTable_BuildsOnlySlot0)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    auto state = MakeHandoffState();
    auto *hashFactory = MakeHashFactory(state);
    auto *build = dynamic_cast<HashBuilderOperator *>(hashFactory->CreateOperator(&pool));
    auto *lookupFactory = MakeLookupFactory(hashFactory, state);
    auto *probe = dynamic_cast<LookupJoinOperator *>(lookupFactory->CreateOperator(&pool));

    const int64_t kMatch = FindKeyForSubPartition(1);
    const int64_t kMiss = FindKeyForSubPartition(2);
    ASSERT_GE(kMatch, 0);
    ASSERT_GE(kMiss, 0);

    build->setNoMoreInput(false);
    build->AddInput(MakeLongBatch({kMatch}, {10}));
    EXPECT_EQ(JoinBuildRuntimeLayout::Single, state->GetRuntimeLayout());
    EXPECT_EQ(1u, build->GetHashTableCount());
    build->setNoMoreInput(true);

    std::vector<JoinedRow> outRows;
    bool probeFed = false;
    bool sawSingleFullTable = false;
    int iter = 0;
    const int kMaxIter = 100000;
    for (; iter < kMaxIter; ++iter) {
        if (!build->isFinished()) {
            ContinueFuture bf;
            if (build->IsBlocked(&bf) == BlockingReason::kNotBlocked) {
                VectorBatch *out = nullptr;
                build->GetOutput(&out);
                if (out != nullptr) {
                    VectorHelper::FreeVecBatch(out);
                }
            }
        }

        JoinHandoffSnapshot snap;
        if (!sawSingleFullTable && state->TryGetHandoffForProbe(&snap) && snap.numDistinct > 0 &&
            snap.spillPartitionIds.empty() && !snap.restoredPartitionId.has_value()) {
            EXPECT_EQ(JoinBuildRuntimeLayout::Single, snap.runtimeLayout);
            EXPECT_EQ(1u, build->GetHashTableCount());
            EXPECT_EQ(0u, state->BuildSpilledRows());
            sawSingleFullTable = true;
        }

        if (!probeFed && state->GetHandoffPhase() == JoinHandoffPhase::RestoreRound) {
            probe->setNoMoreInput(false);
            probe->AddInput(MakeLongBatch({kMatch, kMatch, kMiss}, {201, 202, 203}));
            probe->setNoMoreInput(true);
            probeFed = true;
        }

        if (probeFed) {
            ContinueFuture pf;
            if (probe->IsBlocked(&pf) == BlockingReason::kNotBlocked) {
                VectorBatch *out = nullptr;
                probe->GetOutput(&out);
                if (out != nullptr) {
                    auto *buildKey = reinterpret_cast<Vector<int64_t> *>(out->Get(2));
                    auto *buildValue = reinterpret_cast<Vector<int64_t> *>(out->Get(3));
                    for (int32_t i = 0; i < out->GetRowCount(); ++i) {
                        outRows.push_back(JoinedRow{buildKey->GetValue(i), buildValue->GetValue(i)});
                    }
                    VectorHelper::FreeVecBatch(out);
                }
            }
        }

        if (state->GetHandoffPhase() == JoinHandoffPhase::Complete && build->isFinished() &&
            probe->isFinished()) {
            break;
        }
    }

    EXPECT_LT(iter, kMaxIter);
    EXPECT_TRUE(sawSingleFullTable);
    EXPECT_EQ(0u, state->BuildSpilledRows());
    EXPECT_EQ(JoinBuildRuntimeLayout::Single, state->GetRuntimeLayout());
    int matchRows = 0;
    for (const auto &row : outRows) {
        if (row.buildKey == kMatch) {
            ++matchRows;
            EXPECT_EQ(10, row.buildValue);
        } else {
            ADD_FAILURE() << "unexpected build key in join output: " << row.buildKey;
        }
    }
    EXPECT_EQ(2, matchRows);

    omniruntime::op::Operator::DeleteOperator(probe);
    omniruntime::op::Operator::DeleteOperator(build);
    delete lookupFactory;
    delete hashFactory;
}

// Full-table session stays Single. Forced memory-threshold spill switches runtime layout
// to Partitioned before probe ingest (empty-table ids handoff).
TEST(JoinSpillDriverTest, ProbeRuntimeLayout_SingleThenPartitionedAfterForcedSpill)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    auto stateSingle = MakeHandoffState();
    auto *hashFactorySingle = MakeHashFactory(stateSingle);
    auto *buildSingle = dynamic_cast<HashBuilderOperator *>(hashFactorySingle->CreateOperator(&pool));
    auto *lookupFactorySingle = MakeLookupFactory(hashFactorySingle, stateSingle);
    auto *probeSingle = dynamic_cast<LookupJoinOperator *>(lookupFactorySingle->CreateOperator(&pool));

    const int64_t kMatch = FindKeyForSubPartition(0);
    ASSERT_GE(kMatch, 0);

    buildSingle->setNoMoreInput(false);
    buildSingle->AddInput(MakeLongBatch({kMatch}, {10}));
    buildSingle->setNoMoreInput(true);
    for (int i = 0; i < 16 && stateSingle->GetHandoffPhase() == JoinHandoffPhase::Idle; ++i) {
        ContinueFuture bf;
        if (buildSingle->IsBlocked(&bf) == BlockingReason::kNotBlocked) {
            VectorBatch *out = nullptr;
            buildSingle->GetOutput(&out);
            if (out != nullptr) {
                VectorHelper::FreeVecBatch(out);
            }
        }
    }
    ASSERT_EQ(JoinHandoffPhase::RestoreRound, stateSingle->GetHandoffPhase());
    EXPECT_EQ(JoinBuildRuntimeLayout::Single, stateSingle->GetRuntimeLayout());
    EXPECT_FALSE(stateSingle->IsRuntimePartitioned());

    omniruntime::op::Operator::DeleteOperator(probeSingle);
    omniruntime::op::Operator::DeleteOperator(buildSingle);
    delete lookupFactorySingle;
    delete hashFactorySingle;

    auto statePart = MakeHandoffState();
    auto *hashFactoryPart = MakeHashFactory(statePart);
    auto *buildPart = dynamic_cast<HashBuilderOperator *>(hashFactoryPart->CreateOperator(&pool));
    auto *lookupFactoryPart = MakeLookupFactory(hashFactoryPart, statePart);
    auto *probePart = dynamic_cast<LookupJoinOperator *>(lookupFactoryPart->CreateOperator(&pool));

    buildPart->setNoMoreInput(false);
    buildPart->AddInput(MakeLongBatch({kMatch, kMatch + 1, kMatch + 2, kMatch + 3}, {10, 11, 12, 13}));
    buildPart->reclaim(std::numeric_limits<int64_t>::max());
    buildPart->setNoMoreInput(true);
    for (int i = 0; i < 16 && statePart->GetHandoffPhase() == JoinHandoffPhase::Idle; ++i) {
        ContinueFuture bf;
        if (buildPart->IsBlocked(&bf) == BlockingReason::kNotBlocked) {
            VectorBatch *out = nullptr;
            buildPart->GetOutput(&out);
            if (out != nullptr) {
                VectorHelper::FreeVecBatch(out);
            }
        }
    }
    ASSERT_EQ(JoinHandoffPhase::ProbePathA, statePart->GetHandoffPhase());
    EXPECT_EQ(JoinBuildRuntimeLayout::Partitioned, statePart->GetRuntimeLayout());
    EXPECT_TRUE(statePart->IsRuntimePartitioned());

    omniruntime::op::Operator::DeleteOperator(probePart);
    omniruntime::op::Operator::DeleteOperator(buildPart);
    delete lookupFactoryPart;
    delete hashFactoryPart;
}

struct BuildPreservingDriverParam {
    JoinType joinType;
    BuildSide buildSide;
    const char *name;
};

class JoinSpillBuildPreservingDriverTest : public testing::TestWithParam<BuildPreservingDriverParam> {};

TEST_P(JoinSpillBuildPreservingDriverTest, RePumpEmitsPairedAndUnmatchedBuildRows)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    const auto param = GetParam();
    auto state = MakeHandoffState();
    auto *hashFactory = MakeOuterHashFactory(state, param.joinType, param.buildSide);
    auto *build = dynamic_cast<HashBuilderOperator *>(hashFactory->CreateOperator(&pool));
    const int64_t matched = FindKeyForSubPartition(1);
    const int64_t unmatched = FindSecondKeyForSubPartition(1, matched);
    ASSERT_GE(matched, 0);
    ASSERT_GE(unmatched, 0);

    build->setNoMoreInput(false);
    build->AddInput(MakeLongBatch({matched, unmatched}, {10, 19}));
    build->setNoMoreInput(true);
    VectorBatch *buildOutput = nullptr;
    build->GetOutput(&buildOutput);
    EXPECT_EQ(JoinHandoffPhase::RestoreRound, state->GetHandoffPhase());

    auto *lookupFactory = MakeLookupFactory(hashFactory, state);
    auto *probe = dynamic_cast<LookupJoinOperator *>(lookupFactory->CreateOperator(&pool));
    probe->setNoMoreInput(false);
    probe->AddInput(MakeLongBatch({matched}, {201}));
    probe->setNoMoreInput(true);

    int32_t pairedRows = 0;
    for (int32_t guard = 0; guard < 100000 && !probe->isFinished(); ++guard) {
        VectorBatch *out = nullptr;
        probe->GetOutput(&out);
        if (out != nullptr) {
            pairedRows += out->GetRowCount();
            VectorHelper::FreeVecBatch(out);
        }
    }
    EXPECT_EQ(1, pairedRows);
    EXPECT_EQ(JoinHandoffPhase::RestoreRound, state->GetHandoffPhase());
    EXPECT_FALSE(build->isFinished());

    const DataTypes types({LongType(), LongType()});
    int32_t probeOutputCols[] = {0, 1};
    int32_t buildOutputCols[] = {0, 1};
    auto *outerFactory = LookupOuterJoinOperatorFactory::CreateLookupOuterJoinOperatorFactory(types,
        probeOutputCols, 2, buildOutputCols, types, reinterpret_cast<int64_t>(hashFactory), param.buildSide);
    auto *outer = dynamic_cast<LookupOuterJoinOperator *>(outerFactory->CreateOperator(&pool));
    int32_t unmatchedBuildRows = 0;
    for (int32_t guard = 0; guard < 100000 && !outer->isFinished(); ++guard) {
        VectorBatch *out = nullptr;
        outer->GetOutput(&out);
        if (out == nullptr) {
            continue;
        }
        const int32_t buildKeyCol = param.buildSide == OMNI_BUILD_LEFT ? 0 : 2;
        const int32_t probeKeyCol = param.buildSide == OMNI_BUILD_LEFT ? 2 : 0;
        for (int32_t row = 0; row < out->GetRowCount(); ++row) {
            EXPECT_TRUE(out->Get(probeKeyCol)->IsNull(row));
            EXPECT_EQ(unmatched, VectorHelper::GetValueFromVector<int64_t>(out->Get(buildKeyCol), row));
            ++unmatchedBuildRows;
        }
        VectorHelper::FreeVecBatch(out);
    }
    EXPECT_EQ(1, unmatchedBuildRows);

    probe->FinishFullTableProbeAfterOuter();
    EXPECT_EQ(JoinHandoffPhase::Complete, state->GetHandoffPhase());
    build->GetOutput(&buildOutput);
    EXPECT_TRUE(build->isFinished());
    EXPECT_TRUE(probe->isFinished());

    omniruntime::op::Operator::DeleteOperator(outer);
    omniruntime::op::Operator::DeleteOperator(probe);
    omniruntime::op::Operator::DeleteOperator(build);
    delete outerFactory;
    delete lookupFactory;
    delete hashFactory;
}

INSTANTIATE_TEST_SUITE_P(BuildPreservingOuterMatrix, JoinSpillBuildPreservingDriverTest,
    testing::Values(
        BuildPreservingDriverParam{OMNI_JOIN_TYPE_LEFT, OMNI_BUILD_LEFT, "LeftBuildLeft"},
        BuildPreservingDriverParam{OMNI_JOIN_TYPE_RIGHT, OMNI_BUILD_RIGHT, "RightBuildRight"},
        BuildPreservingDriverParam{OMNI_JOIN_TYPE_FULL, OMNI_BUILD_LEFT, "FullBuildLeft"},
        BuildPreservingDriverParam{OMNI_JOIN_TYPE_FULL, OMNI_BUILD_RIGHT, "FullBuildRight"}),
    [](const testing::TestParamInfo<BuildPreservingDriverParam> &info) { return info.param.name; });

struct MultiSpChunkJoinParam {
    JoinType joinType;
    BuildSide buildSide;
    const char *name;
};

class JoinSpillMultiSpChunkTest : public testing::TestWithParam<MultiSpChunkJoinParam> {};

join_spill_dual_run::RunResult RunMultiSpChunkArm(
    join_spill_dual_run::SpillMode mode,
    const MultiSpChunkJoinParam &param,
    const join_spill_dual_run::BatchFactory &buildFactory,
    const join_spill_dual_run::BatchFactory &probeFactory)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    using namespace join_spill_dual_run;
    const DataTypes types = WideLongTypes();
    int32_t hashCols[] = {0};
    std::vector<int32_t> outputCols(kWideColumnCount);
    for (int32_t i = 0; i < kWideColumnCount; ++i) {
        outputCols[static_cast<size_t>(i)] = i;
    }

    ScopedTempDirectory spillRoot(param.name);
    auto state = std::make_shared<JoinSpillState>(spillRoot.path().string(), types, types, 4);
    state->SetHandoffSessionEnabled(true);
    state->SetInitialRuntimeLayout(JoinBuildRuntimeLayout::Single);
    const bool forceSpill = mode == SpillMode::ForcedSpillPathA;
    auto *hashFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
        param.joinType, param.buildSide, types, hashCols, 1, 1);
    hashFactory->SetJoinSpillSubPartitionPolicy(true, 0, TestSubPartitionConfig(), true, false, 10);
    hashFactory->SetJoinSpillState(state);

    auto *build = dynamic_cast<HashBuilderOperator *>(hashFactory->CreateOperator(&pool));
    build->setNoMoreInput(false);
    build->AddInput(buildFactory());
    if (forceSpill) build->reclaim(std::numeric_limits<int64_t>::max());
    build->setNoMoreInput(true);
    VectorBatch *output = nullptr;
    build->GetOutput(&output);
    if (output != nullptr) {
        VectorHelper::FreeVecBatch(output);
        throw std::runtime_error("hash build unexpectedly produced data");
    }

    auto *lookupFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(types, outputCols.data(),
        kWideColumnCount, hashCols, 1, outputCols.data(), kWideColumnCount, types,
        reinterpret_cast<int64_t>(hashFactory), nullptr, false, nullptr);
    lookupFactory->SetJoinSpillPolicy(true, TestSubPartitionConfig());
    lookupFactory->SetJoinSpillState(state);
    auto *probe = dynamic_cast<LookupJoinOperator *>(lookupFactory->CreateOperator(&pool));

    RunResult result;
    size_t observedSpillFileCount = CountRegularFiles(spillRoot.path());
    bool probeFed = false;
    bool sawPathA = false;
    bool sawRestoreAfterPathA = false;
    int32_t guard = 0;
    if (!forceSpill) {
        JoinHandoffSnapshot snapshot;
        if (!state->TryGetHandoffForProbe(&snapshot) || snapshot.numDistinct == 0 ||
            !snapshot.spillPartitionIds.empty() || snapshot.restoredPartitionId.has_value()) {
            throw std::runtime_error("resident scheduler did not receive a full-table handoff");
        }
        probe->setNoMoreInput(false);
        probe->AddInput(probeFactory());
        probe->setNoMoreInput(true);
        probeFed = true;
        for (; guard < 100000 && !probe->isFinished(); ++guard) {
            output = nullptr;
            probe->GetOutput(&output);
            if (output != nullptr) {
                ++result.outputBatchCount;
                AppendCanonicalRowsAndFree(output, &result.rows);
            }
        }
        if (probe->NeedsUnvisitedBuildOutput()) {
            // All build rows are matched, so the wrapper-owned outer drain is empty.
            probe->FinishFullTableProbeAfterOuter();
        }
        output = nullptr;
        build->GetOutput(&output);
        if (output != nullptr) {
            VectorHelper::FreeVecBatch(output);
        }
    } else {
        for (; guard < 100000; ++guard) {
        if (!build->isFinished()) {
            ContinueFuture future;
            if (build->IsBlocked(&future) == BlockingReason::kNotBlocked) {
                output = nullptr;
                build->GetOutput(&output);
                if (output != nullptr) {
                    VectorHelper::FreeVecBatch(output);
                }
            }
        }
        const auto phase = state->GetHandoffPhase();
        observedSpillFileCount = std::max(observedSpillFileCount, CountRegularFiles(spillRoot.path()));
        sawPathA = sawPathA || phase == JoinHandoffPhase::ProbePathA;
        sawRestoreAfterPathA =
            sawRestoreAfterPathA || (sawPathA && phase == JoinHandoffPhase::RestoreRound);
        JoinHandoffSnapshot snapshot;
        if (!probeFed && state->TryGetHandoffForProbe(&snapshot)) {
            probe->setNoMoreInput(false);
            probe->AddInput(probeFactory());
            probe->setNoMoreInput(true);
            probeFed = true;
        }
        if (probeFed && !probe->isFinished()) {
            ContinueFuture future;
            if (probe->IsBlocked(&future) == BlockingReason::kNotBlocked) {
                output = nullptr;
                probe->GetOutput(&output);
                if (output != nullptr) {
                    ++result.outputBatchCount;
                    AppendCanonicalRowsAndFree(output, &result.rows);
                }
            }
        }
        if (state->GetHandoffPhase() == JoinHandoffPhase::Complete && build->isFinished() &&
            probe->isFinished()) {
            break;
        }
        }
    }
    const bool guardExhausted = guard >= 100000;
    const std::string guardDetails = "dual-run scheduler guard exhausted mode=" +
        std::to_string(static_cast<int>(mode)) + " phase=" +
        std::to_string(static_cast<int>(state->GetHandoffPhase())) + " buildStatus=" +
        std::to_string(static_cast<int>(build->GetStatus())) + " probeStatus=" +
        std::to_string(static_cast<int>(probe->GetStatus()));
    result.buildSpilledRows = state->BuildSpilledRows();
    result.probeSpilledRows = probe->ProbeSpilledRows();
    result.spillFileCount = observedSpillFileCount;
    result.sawPathA = sawPathA;
    result.sawRestore = sawRestoreAfterPathA;
    result.finalPhase = state->GetHandoffPhase();

    omniruntime::op::Operator::DeleteOperator(probe);
    omniruntime::op::Operator::DeleteOperator(build);
    delete lookupFactory;
    delete hashFactory;
    if (guardExhausted) {
        throw std::runtime_error(guardDetails);
    }
    return result;
}

TEST_P(JoinSpillMultiSpChunkTest, NonIdentityPermutationDrainsEveryProbeRow)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    using namespace join_spill_dual_run;
    const auto param = GetParam();
    const int64_t keySp0 = FindKeyForSubPartition(0);
    const int64_t keySp1 = FindKeyForSubPartition(1);
    const int64_t keySp2 = FindKeyForSubPartition(2);
    ASSERT_GE(keySp0, 0);
    ASSERT_GE(keySp1, 0);
    ASSERT_GE(keySp2, 0);

    // One logical probe row fills one output chunk. Physical order [SP2,SP0,SP1] becomes
    // permutation [1,2,0], so saving a physical row id as the logical cursor skips SP1.
    const int32_t matchesPerProbe =
        OperatorUtil::GetMaxRowCount(2 * kWideColumnCount * static_cast<int32_t>(sizeof(int64_t)));
    std::vector<int64_t> buildKeys;
    buildKeys.reserve(static_cast<size_t>(matchesPerProbe) * 3);
    for (int64_t key : {keySp0, keySp1, keySp2}) {
        buildKeys.insert(buildKeys.end(), static_cast<size_t>(matchesPerProbe), key);
    }
    const std::vector<int64_t> probeKeys = {keySp2, keySp0, keySp1};
    BatchFactory buildFactory = [&]() { return MakeWideLongBatch(buildKeys); };
    BatchFactory probeFactory = [&]() { return MakeWideLongBatch(probeKeys); };
    ScenarioScheduler scheduler;
    scheduler.residentFullTable = [&](const BatchFactory &build, const BatchFactory &probe) {
        return RunMultiSpChunkArm(SpillMode::NoSpill, param, build, probe);
    };
    scheduler.forcedSpillPathARestore = [&](const BatchFactory &build, const BatchFactory &probe) {
        return RunMultiSpChunkArm(SpillMode::ForcedSpillPathA, param, build, probe);
    };

    const auto resident = RunResidentFullTableNoSpill(scheduler, buildFactory, probeFactory);
    const auto spilled = RunForcedSpillPathARestore(scheduler, buildFactory, probeFactory);
    AssertTupleMultisetEqual(resident, spilled);
    AssertNoSpillEvidence(resident);
    AssertSpillEvidence(spilled);

    EXPECT_GE(resident.outputBatchCount, 3u);
    EXPECT_GE(spilled.outputBatchCount, 3u);
    EXPECT_EQ(static_cast<size_t>(matchesPerProbe) * 3, resident.rows.size());
    EXPECT_EQ(resident.rows.size(), spilled.rows.size());

    const int32_t buildKeyCol = param.buildSide == OMNI_BUILD_LEFT ? 0 : kWideColumnCount;
    for (const auto &result : {resident, spilled}) {
        int64_t sp0Rows = 0;
        int64_t sp1Rows = 0;
        int64_t sp2Rows = 0;
        for (const auto &row : result.rows) {
            ASSERT_GT(row.size(), static_cast<size_t>(buildKeyCol));
            ASSERT_EQ(sizeof(int64_t), row[static_cast<size_t>(buildKeyCol)].exactValue.size());
            int64_t key = 0;
            std::memcpy(&key, row[static_cast<size_t>(buildKeyCol)].exactValue.data(), sizeof(key));
            sp0Rows += key == keySp0;
            sp1Rows += key == keySp1;
            sp2Rows += key == keySp2;
        }
        EXPECT_EQ(matchesPerProbe, sp0Rows);
        EXPECT_EQ(matchesPerProbe, sp1Rows);
        EXPECT_EQ(matchesPerProbe, sp2Rows);
    }
}

INSTANTIATE_TEST_SUITE_P(AllSupportedJoinBuildSides, JoinSpillMultiSpChunkTest,
    testing::Values(
        MultiSpChunkJoinParam{OMNI_JOIN_TYPE_INNER, OMNI_BUILD_RIGHT, "Inner"},
        MultiSpChunkJoinParam{OMNI_JOIN_TYPE_INNER, OMNI_BUILD_LEFT, "InnerBuildLeft"},
        MultiSpChunkJoinParam{OMNI_JOIN_TYPE_LEFT, OMNI_BUILD_RIGHT, "LeftBuildRight"},
        MultiSpChunkJoinParam{OMNI_JOIN_TYPE_LEFT, OMNI_BUILD_LEFT, "LeftBuildLeft"},
        MultiSpChunkJoinParam{OMNI_JOIN_TYPE_RIGHT, OMNI_BUILD_LEFT, "RightBuildLeft"},
        MultiSpChunkJoinParam{OMNI_JOIN_TYPE_RIGHT, OMNI_BUILD_RIGHT, "RightBuildRight"},
        MultiSpChunkJoinParam{OMNI_JOIN_TYPE_FULL, OMNI_BUILD_LEFT, "FullBuildLeft"},
        MultiSpChunkJoinParam{OMNI_JOIN_TYPE_FULL, OMNI_BUILD_RIGHT, "FullBuildRight"}),
    [](const testing::TestParamInfo<MultiSpChunkJoinParam> &info) { return info.param.name; });

TEST(JoinSpillDriverTest, SameSideOuterRetainsCompoundNullBuildRows)
{
    for (const auto side : {OMNI_BUILD_LEFT, OMNI_BUILD_RIGHT}) {
        omniruntime::mem::OmniTaskMemoryPool pool(nullptr);
        const DataTypes types({LongType(), LongType()});
        int32_t keys[] = {0, 1};
        const auto join = side == OMNI_BUILD_LEFT ? OMNI_JOIN_TYPE_LEFT : OMNI_JOIN_TYPE_RIGHT;
        auto *factory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
            join, side, types, keys, 2, 1);
        auto *build = factory->CreateOperator(&pool);
        auto *batch = MakeLongBatch({1, 2, 3}, {10, 20, 30});
        batch->Get(0)->SetNull(0);
        batch->Get(1)->SetNull(1);
        build->AddInput(batch);
        build->noMoreInput();
        VectorBatch *out = nullptr;
        build->GetOutput(&out);
        ASSERT_EQ(out, nullptr);
        auto *outerFactory = LookupOuterJoinOperatorFactory::CreateLookupOuterJoinOperatorFactory(
            types, keys, 2, keys, types, reinterpret_cast<int64_t>(factory), side);
        auto *outer = outerFactory->CreateOperator(&pool);
        int rows = 0;
        int nullFirstKey = 0;
        int nullSecondKey = 0;
        const int buildOffset = side == OMNI_BUILD_LEFT ? 0 : 2;
        const int probeOffset = side == OMNI_BUILD_LEFT ? 2 : 0;
        for (int i = 0; i < 100 && !outer->isFinished(); ++i) {
            outer->GetOutput(&out);
            if (out == nullptr) continue;
            for (int row = 0; row < out->GetRowCount(); ++row) {
                ++rows;
                nullFirstKey += out->Get(buildOffset)->IsNull(row);
                nullSecondKey += out->Get(buildOffset + 1)->IsNull(row);
                EXPECT_TRUE(out->Get(probeOffset)->IsNull(row));
                EXPECT_TRUE(out->Get(probeOffset + 1)->IsNull(row));
            }
            VectorHelper::FreeVecBatch(out);
            out = nullptr;
        }
        EXPECT_TRUE(outer->isFinished());
        EXPECT_EQ(rows, 3);
        EXPECT_EQ(nullFirstKey, 1);
        EXPECT_EQ(nullSecondKey, 1);
        op::Operator::DeleteOperator(outer);
        op::Operator::DeleteOperator(build);
        delete outerFactory;
        delete factory;
    }
}
