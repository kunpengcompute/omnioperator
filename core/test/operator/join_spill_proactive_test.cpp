/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 * Description: proactive hash join spill tests (NeedSpill + per-partition restore).
 */
#include <climits>
#include <sys/stat.h>

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

#include "expression/expressions.h"
#include "gtest/gtest.h"
#include "memory/omni_task_memory_pool.h"
#include "memory/pool_listener.h"
#include "memory/reservation_denied.h"
#include "operator/reclaim_context.h"
#include <limits>
#include "operator/config/operator_config.h"
#include "operator/join/hash_builder.h"
#include "operator/join/hash_builder_expr.h"
#include "operator/join/join_spill_state.h"
#include "operator/join/join_sub_partitioner.h"
#include "operator/join/lookup_join.h"
#include "operator/join/lookup_join_expr.h"
#include "operator/join/lookup_outer_join.h"
#include "type/data_type.h"
#include "type/data_types.h"
#include "util/test_util.h"
#include "vector/vector.h"
#include "vector/vector_batch.h"
#include "vector/vector_helper.h"
#include "vector/large_string_container.h"
#include "join_spill_dual_run_test_util.h"

using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::vec;
using omniruntime::OMNI_BUILD_LEFT;
using omniruntime::OMNI_BUILD_RIGHT;
using omniruntime::OMNI_BUILD_UNKNOWN;
using omniruntime::TestUtil::GenerateSpillPath;
namespace op = omniruntime::op;
namespace expr = omniruntime::expressions;

namespace {

JoinSubPartitionConfig TestSubPartitionConfig()
{
    JoinSubPartitionConfig cfg;
    cfg.numSubPartitions = 4;
    cfg.startPartitionBit = 0;
    cfg.activeSubPartition = 0;
    return cfg;
}

config::QueryConfig TestJoinQueryConfig(bool spillEnabled)
{
    return config::QueryConfig(std::unordered_map<std::string, std::string>{
        {config::QueryConfig::kSpillEnabled, spillEnabled ? "true" : "false"},
        {config::QueryConfig::kPassiveReclaimEnabled, "true"},
        {config::QueryConfig::kSpillStartPartitionBit, "0"},
        {config::QueryConfig::kSpillNumPartitionBits, "2"}});
}

std::shared_ptr<SparkSpillConfig> MakeSpillConfig(const std::string &spillPath)
{
    return std::make_shared<SparkSpillConfig>(true, spillPath, 100UL << 30, INT32_MAX, 1.0, 0, false);
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

VectorBatch *MakeDate32Batch(const std::vector<int32_t> &keys, const std::vector<int64_t> &values)
{
    auto *batch = new VectorBatch(static_cast<int32_t>(keys.size()));
    auto *keyVec = new Vector<int32_t>(static_cast<int32_t>(keys.size()));
    keyVec->SetDataType(Date32Type(DAY));
    auto *valueVec = new Vector<int64_t>(static_cast<int32_t>(values.size()));
    valueVec->SetDataType(LongType());
    for (size_t i = 0; i < keys.size(); ++i) {
        keyVec->SetValue(static_cast<int32_t>(i), keys[i]);
        valueVec->SetValue(static_cast<int32_t>(i), values[i]);
    }
    batch->Append(keyVec);
    batch->Append(valueVec);
    return batch;
}

int32_t PumpJoin(op::Operator *builder, op::Operator *probe, bool reclaimHeld = false)
{
    int32_t outputRows = 0;
    // Both sides are terminal in these unit tests: signal end-of-stream so GetOutput can publish the
    // handoff / build the in-memory table / drive restore and reach OMNI_STATUS_FINISHED.
    builder->noMoreInput();
    probe->noMoreInput();
    for (int32_t i = 0; i < 10000; ++i) {
        if (builder->GetStatus() == OMNI_STATUS_FINISHED && probe->GetStatus() == OMNI_STATUS_FINISHED) {
            break;
        }
        VectorBatch *buildOut = nullptr;
        builder->GetOutput(&buildOut);
        if (reclaimHeld) {
            if (auto *lookup = dynamic_cast<LookupJoinOperator *>(probe)) {
                lookup->reclaim(std::numeric_limits<int64_t>::max());
            }
        }
        VectorBatch *probeOut = nullptr;
        probe->GetOutput(&probeOut);
        if (probeOut != nullptr) {
            outputRows += probeOut->GetRowCount();
            VectorHelper::FreeVecBatch(probeOut);
        }
    }
    EXPECT_TRUE(builder->isFinished()) << "build exceeded pump iteration limit";
    EXPECT_TRUE(probe->isFinished()) << "probe exceeded pump iteration limit";
    return outputRows;
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

struct FullJoinRowKinds {
    int32_t paired = 0;
    int32_t unmatchedProbe = 0;
    int32_t unmatchedBuild = 0;
};

void CountJoinKeyNulls(BaseVector *probeKey, BaseVector *buildKey, int32_t rowCount, FullJoinRowKinds *counts)
{
    for (int32_t row = 0; row < rowCount; ++row) {
        const bool probeNull = probeKey->IsNull(row);
        const bool buildNull = buildKey->IsNull(row);
        if (!probeNull && !buildNull) {
            ++counts->paired;
        } else if (!probeNull && buildNull) {
            ++counts->unmatchedProbe;
        } else if (probeNull && !buildNull) {
            ++counts->unmatchedBuild;
        }
    }
}

// FULL + BUILD_RIGHT leaves probe columns first; unmatched build has NULL probe keys.
void CountFullBuildRightRows(VectorBatch *out, FullJoinRowKinds *counts)
{
    CountJoinKeyNulls(out->Get(0), out->Get(2), out->GetRowCount(), counts);
}

// LEFT + BUILD_LEFT rotates build columns first.
void CountSameSideBuildLeftRows(VectorBatch *out, FullJoinRowKinds *counts)
{
    CountJoinKeyNulls(out->Get(2), out->Get(0), out->GetRowCount(), counts);
}

FullJoinRowKinds PumpFullJoinKinds(op::Operator *builder, op::Operator *probe, bool reclaimHeld = false)
{
    FullJoinRowKinds counts;
    builder->noMoreInput();
    probe->noMoreInput();
    for (int32_t i = 0; i < 10000; ++i) {
        if (builder->GetStatus() == OMNI_STATUS_FINISHED && probe->GetStatus() == OMNI_STATUS_FINISHED) {
            break;
        }
        VectorBatch *buildOut = nullptr;
        builder->GetOutput(&buildOut);
        if (reclaimHeld) {
            if (auto *lookup = dynamic_cast<LookupJoinOperator *>(probe)) {
                lookup->reclaim(std::numeric_limits<int64_t>::max());
            }
        }
        VectorBatch *probeOut = nullptr;
        probe->GetOutput(&probeOut);
        if (probeOut != nullptr) {
            CountFullBuildRightRows(probeOut, &counts);
            VectorHelper::FreeVecBatch(probeOut);
        }
    }
    EXPECT_TRUE(builder->isFinished()) << "build exceeded pump iteration limit";
    EXPECT_TRUE(probe->isFinished()) << "probe exceeded pump iteration limit";
    return counts;
}

std::string LatestJoinSpillSessionDir(const std::string &spillRoot)
{
    std::string best;
    std::filesystem::file_time_type newest{};
    bool found = false;
    std::error_code ec;
    for (auto it = std::filesystem::directory_iterator(spillRoot, ec);
         !ec && it != std::filesystem::directory_iterator(); ++it) {
        const auto name = it->path().filename().string();
        const std::string prefix = "join-spill-";
        if (name.rfind(prefix, 0) != 0) {
            continue;
        }
        // Session suffixes are unique random tokens, not numeric IDs.
        std::error_code entryError;
        if (!it->is_directory(entryError) || entryError) continue;
        const auto modified = it->last_write_time(entryError);
        if (entryError) continue;
        if (!found || modified > newest) {
            newest = modified;
            found = true;
            best = it->path().string();
        }
    }
    return best;
}

int32_t CountBuildSpillDirsForPartition(const std::string &sessionDir, uint32_t partitionId)
{
    int32_t count = 0;
    if (sessionDir.empty()) {
        return 0;
    }
    std::error_code ec;
    const std::string leaf = "sp" + std::to_string(partitionId);
    for (auto it = std::filesystem::recursive_directory_iterator(sessionDir, ec);
         !ec && it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
        if (!it->is_directory()) {
            continue;
        }
        if (it->path().filename() != leaf) {
            continue;
        }
        // Match relative to the session so a spill root under a "build" directory does not count probe dirs.
        const auto path = std::filesystem::relative(it->path(), sessionDir).generic_string();
        if (path.rfind("build/", 0) == 0) {
            ++count;
        }
    }
    return count;
}

void EnableJoinSpill(HashBuilderOperatorFactory *hashBuilderFactory, LookupJoinOperatorFactory *lookupFactory,
    const std::shared_ptr<JoinSpillState> &state, const JoinSubPartitionConfig &cfg,
    const std::shared_ptr<SparkSpillConfig> &spillConfig, bool forceSpill = false)
{
    state->SetHandoffSessionEnabled(true);
    state->SetInitialRuntimeLayout(JoinBuildRuntimeLayout::Single);
    hashBuilderFactory->SetJoinSpillSubPartitionPolicy(true, 0, cfg, true, forceSpill);
    hashBuilderFactory->SetJoinSpillState(state);
    hashBuilderFactory->SetSpillConfig(spillConfig);
    lookupFactory->SetJoinSpillPolicy(true, cfg, true);
    lookupFactory->SetJoinSpillState(state);
}

} // namespace

TEST(JoinSpillProactiveTest, SmallDataWithoutMemoryPressureDoesNotSpill)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    auto cfg = TestSubPartitionConfig();
    const std::string spillPath = GenerateSpillPath();
    mkdir(spillPath.c_str(), 0750);

    DataTypes rowTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    int32_t hashCols[] = {0};
    auto *hashBuilderFactory =
        HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(OMNI_JOIN_TYPE_INNER, rowTypes, hashCols, 1, 1);
    auto state = std::make_shared<JoinSpillState>(spillPath, rowTypes, rowTypes, cfg.numSubPartitions);
    int32_t probeOutputCols[] = {0, 1};
    int32_t buildOutputCols[] = {0, 1};
    auto *lookupFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(rowTypes, probeOutputCols, 2,
        hashCols, 1, buildOutputCols, 2, rowTypes, reinterpret_cast<int64_t>(hashBuilderFactory), nullptr, false,
        nullptr);
    EnableJoinSpill(hashBuilderFactory, lookupFactory, state, cfg, MakeSpillConfig(spillPath), false);

    auto *hashBuilder = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator(&pool));
    hashBuilder->AddInput(MakeLongBatch({1, 2, 3, 4}, {10, 20, 30, 40}));
    EXPECT_EQ(hashBuilder->GetHashTableCount(), 1U);

    auto *lookupJoin = dynamic_cast<LookupJoinOperator *>(lookupFactory->CreateOperator(&pool));
    lookupJoin->AddInput(MakeLongBatch({2, 4}, {200, 400}));
    const int32_t outputRows = PumpJoin(hashBuilder, lookupJoin);

    EXPECT_EQ(outputRows, 2);
    EXPECT_EQ(hashBuilder->GetHashTableCount(), 1U);
    EXPECT_EQ(state->BuildSpilledRows(), 0U);
    EXPECT_EQ(state->BuildSpilledFiles(), 0U);

    op::Operator::DeleteOperator(lookupJoin);
    op::Operator::DeleteOperator(hashBuilder);
    delete lookupFactory;
    delete hashBuilderFactory;
}

// Admission succeeds during ingest; the listener denies the EOB build reservation.
TEST(JoinSpillProactiveTest, EobReservationDenialSpillsBuild)
{
    class BuildDenialListener : public omniruntime::mem::PoolListener {
    public:
        bool denyBuildReservation = false;
        void allocationChanged(int64_t diff) override
        {
            const auto request = omniruntime::op::CurrentReservationRequest();
            if (diff > 0 && denyBuildReservation && request.site != nullptr &&
                std::string(request.site) == "HashBuilderOperator::DriveHandoffRestore") {
                throw omniruntime::mem::ReservationDenied("test build reservation denied");
            }
        }
    } listener;
    omniruntime::mem::OmniTaskMemoryPool pool{&listener};
    auto cfg = TestSubPartitionConfig();
    const std::string spillPath = GenerateSpillPath();
    mkdir(spillPath.c_str(), 0750);

    DataTypes rowTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    int32_t hashCols[] = {0};
    auto *hashBuilderFactory =
        HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(OMNI_JOIN_TYPE_INNER, rowTypes, hashCols, 1, 1);
    auto state = std::make_shared<JoinSpillState>(spillPath, rowTypes, rowTypes, cfg.numSubPartitions);
    int32_t probeOutputCols[] = {0, 1};
    int32_t buildOutputCols[] = {0, 1};
    auto *lookupFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(rowTypes, probeOutputCols, 2,
        hashCols, 1, buildOutputCols, 2, rowTypes, reinterpret_cast<int64_t>(hashBuilderFactory), nullptr, false,
        nullptr);
    EnableJoinSpill(hashBuilderFactory, lookupFactory, state, cfg, MakeSpillConfig(spillPath), false);

    auto *hashBuilder = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator(&pool));
    hashBuilder->AddInput(MakeLongBatch({1, 2, 3, 4}, {10, 20, 30, 40}));
    EXPECT_EQ(state->BuildSpilledRows(), 0U);

    listener.denyBuildReservation = true;

    auto *lookupJoin = dynamic_cast<LookupJoinOperator *>(lookupFactory->CreateOperator(&pool));
    lookupJoin->AddInput(MakeLongBatch({2, 4}, {200, 400}));
    const int32_t outputRows = PumpJoin(hashBuilder, lookupJoin);

    EXPECT_EQ(outputRows, 2);
    EXPECT_GT(state->BuildSpilledRows(), 0U);

    op::Operator::DeleteOperator(lookupJoin);
    op::Operator::DeleteOperator(hashBuilder);
    delete lookupFactory;
    delete hashBuilderFactory;
}

TEST(JoinSpillProactiveTest, ForcedEobSpillRestoresRows)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    auto cfg = TestSubPartitionConfig();
    const std::string spillPath = GenerateSpillPath();
    mkdir(spillPath.c_str(), 0750);

    DataTypes rowTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    int32_t hashCols[] = {0};
    auto *hashBuilderFactory =
        HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(OMNI_JOIN_TYPE_INNER, rowTypes, hashCols, 1, 1);
    auto state = std::make_shared<JoinSpillState>(spillPath, rowTypes, rowTypes, cfg.numSubPartitions);
    int32_t probeOutputCols[] = {0, 1};
    int32_t buildOutputCols[] = {0, 1};
    auto *lookupFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(rowTypes, probeOutputCols, 2,
        hashCols, 1, buildOutputCols, 2, rowTypes, reinterpret_cast<int64_t>(hashBuilderFactory), nullptr, false,
        nullptr);
    EnableJoinSpill(hashBuilderFactory, lookupFactory, state, cfg, MakeSpillConfig(spillPath), true);

    auto *hashBuilder = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator(&pool));
    EXPECT_EQ(hashBuilder->GetHashTableCount(), 1U);
    hashBuilder->AddInput(MakeLongBatch({1, 2, 3, 4}, {10, 20, 30, 40}));
    EXPECT_EQ(hashBuilder->GetHashTableCount(), 1U);

    auto *lookupJoin = dynamic_cast<LookupJoinOperator *>(lookupFactory->CreateOperator(&pool));
    lookupJoin->AddInput(MakeLongBatch({2, 4}, {200, 400}));
    const int32_t outputRows = PumpJoin(hashBuilder, lookupJoin);

    EXPECT_EQ(outputRows, 2);
    EXPECT_GT(state->BuildSpilledRows(), 0U);
    EXPECT_LE(state->BuildSpilledFiles(), cfg.numSubPartitions);

    op::Operator::DeleteOperator(lookupJoin);
    op::Operator::DeleteOperator(hashBuilder);
    delete lookupFactory;
    delete hashBuilderFactory;
}

TEST(JoinSpillProactiveTest, JoinSpillDisabledHasNoSessionFiles)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    auto cfg = TestSubPartitionConfig();
    const std::string spillPath = GenerateSpillPath();
    mkdir(spillPath.c_str(), 0750);

    DataTypes rowTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    int32_t hashCols[] = {0};
    auto *hashBuilderFactory =
        HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(OMNI_JOIN_TYPE_INNER, rowTypes, hashCols, 1, 1);
    auto state = std::make_shared<JoinSpillState>(spillPath, rowTypes, rowTypes, cfg.numSubPartitions);
    int32_t probeOutputCols[] = {0, 1};
    int32_t buildOutputCols[] = {0, 1};
    auto *lookupFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(rowTypes, probeOutputCols, 2,
        hashCols, 1, buildOutputCols, 2, rowTypes, reinterpret_cast<int64_t>(hashBuilderFactory), nullptr, false,
        nullptr);

    auto *hashBuilder = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator(&pool));
    hashBuilder->AddInput(MakeLongBatch({1, 2, 3, 4}, {10, 20, 30, 40}));
    auto *lookupJoin = dynamic_cast<LookupJoinOperator *>(lookupFactory->CreateOperator(&pool));
    lookupJoin->AddInput(MakeLongBatch({2, 4}, {200, 400}));
    const int32_t outputRows = PumpJoin(hashBuilder, lookupJoin);

    EXPECT_EQ(outputRows, 2);
    EXPECT_EQ(hashBuilder->GetHashTableCount(), 1U);
    EXPECT_EQ(state->BuildSpilledRows(), 0U);

    op::Operator::DeleteOperator(lookupJoin);
    op::Operator::DeleteOperator(hashBuilder);
    delete lookupFactory;
    delete hashBuilderFactory;
}

TEST(JoinSpillProactiveTest, SmallBatchesWithoutMemoryPressureDoNotSpill)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    auto cfg = TestSubPartitionConfig();
    const std::string spillPath = GenerateSpillPath();
    mkdir(spillPath.c_str(), 0750);

    DataTypes rowTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    int32_t hashCols[] = {0};
    auto *hashBuilderFactory =
        HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(OMNI_JOIN_TYPE_INNER, rowTypes, hashCols, 1, 1);
    auto state = std::make_shared<JoinSpillState>(spillPath, rowTypes, rowTypes, cfg.numSubPartitions);
    int32_t probeOutputCols[] = {0, 1};
    int32_t buildOutputCols[] = {0, 1};
    auto *lookupFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(rowTypes, probeOutputCols, 2,
        hashCols, 1, buildOutputCols, 2, rowTypes, reinterpret_cast<int64_t>(hashBuilderFactory), nullptr, false,
        nullptr);
    // Small batches alone do not trigger spill without a denied reservation or reclaim.
    EnableJoinSpill(hashBuilderFactory, lookupFactory, state, cfg, MakeSpillConfig(spillPath), false);

    auto *hashBuilder = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator(&pool));
    hashBuilder->AddInput(MakeLongBatch({1, 2}, {10, 20}));
    hashBuilder->AddInput(MakeLongBatch({3, 4}, {30, 40}));
    auto *lookupJoin = dynamic_cast<LookupJoinOperator *>(lookupFactory->CreateOperator(&pool));
    lookupJoin->AddInput(MakeLongBatch({2, 4}, {200, 400}));
    const int32_t outputRows = PumpJoin(hashBuilder, lookupJoin);

    EXPECT_EQ(outputRows, 2);
    EXPECT_EQ(state->BuildSpilledRows(), 0U);
    EXPECT_EQ(state->BuildSpilledFiles(), 0U);

    op::Operator::DeleteOperator(lookupJoin);
    op::Operator::DeleteOperator(hashBuilder);
    delete lookupFactory;
    delete hashBuilderFactory;
}

TEST(JoinSpillProactiveTest, StickyWriterDoesNotCreateOneFilePerBatch)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    auto cfg = TestSubPartitionConfig();
    const std::string spillPath = GenerateSpillPath();
    mkdir(spillPath.c_str(), 0750);

    DataTypes rowTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    int32_t hashCols[] = {0};
    auto *hashBuilderFactory =
        HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(OMNI_JOIN_TYPE_INNER, rowTypes, hashCols, 1, 1);
    auto state = std::make_shared<JoinSpillState>(spillPath, rowTypes, rowTypes, cfg.numSubPartitions);
    int32_t probeOutputCols[] = {0, 1};
    int32_t buildOutputCols[] = {0, 1};
    auto *lookupFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(rowTypes, probeOutputCols, 2,
        hashCols, 1, buildOutputCols, 2, rowTypes, reinterpret_cast<int64_t>(hashBuilderFactory), nullptr, false,
        nullptr);
    EnableJoinSpill(hashBuilderFactory, lookupFactory, state, cfg, MakeSpillConfig(spillPath), true);

    auto *hashBuilder = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator(&pool));
    hashBuilder->AddInput(MakeLongBatch({1, 2, 3, 4}, {10, 20, 30, 40}));
    hashBuilder->AddInput(MakeLongBatch({5, 6, 7, 8}, {50, 60, 70, 80}));
    auto *lookupJoin = dynamic_cast<LookupJoinOperator *>(lookupFactory->CreateOperator(&pool));
    lookupJoin->AddInput(MakeLongBatch({2, 6}, {200, 600}));
    const int32_t outputRows = PumpJoin(hashBuilder, lookupJoin);

    EXPECT_EQ(outputRows, 2);
    EXPECT_EQ(state->BuildSpilledRows(), 8U);
    EXPECT_LE(state->BuildSpilledFiles(), cfg.numSubPartitions);
    EXPECT_LT(state->BuildSpilledFiles(), 8U);

    op::Operator::DeleteOperator(lookupJoin);
    op::Operator::DeleteOperator(hashBuilder);
    delete lookupFactory;
    delete hashBuilderFactory;
}

TEST(JoinSpillProactiveTest, Date32FieldForcedEobSpill)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    auto cfg = TestSubPartitionConfig();
    const std::string spillPath = GenerateSpillPath();
    mkdir(spillPath.c_str(), 0750);

    DataTypes rowTypes(std::vector<DataTypePtr>({ Date32Type(DAY), LongType() }));
    int32_t hashCols[] = {0};
    auto *hashBuilderFactory =
        HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(OMNI_JOIN_TYPE_INNER, rowTypes, hashCols, 1, 1);
    auto state = std::make_shared<JoinSpillState>(spillPath, rowTypes, rowTypes, cfg.numSubPartitions);
    int32_t probeOutputCols[] = {0, 1};
    int32_t buildOutputCols[] = {0, 1};
    auto *lookupFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(rowTypes, probeOutputCols, 2,
        hashCols, 1, buildOutputCols, 2, rowTypes, reinterpret_cast<int64_t>(hashBuilderFactory), nullptr, false,
        nullptr);
    EnableJoinSpill(hashBuilderFactory, lookupFactory, state, cfg, MakeSpillConfig(spillPath), true);

    auto *hashBuilder = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator(&pool));
    hashBuilder->AddInput(MakeDate32Batch({10, 20, 30, 40}, {10, 20, 30, 40}));
    auto *lookupJoin = dynamic_cast<LookupJoinOperator *>(lookupFactory->CreateOperator(&pool));
    lookupJoin->AddInput(MakeDate32Batch({20, 40}, {200, 400}));
    const int32_t outputRows = PumpJoin(hashBuilder, lookupJoin);

    EXPECT_EQ(outputRows, 2);
    EXPECT_GT(state->BuildSpilledRows(), 0U);

    op::Operator::DeleteOperator(lookupJoin);
    op::Operator::DeleteOperator(hashBuilder);
    delete lookupFactory;
    delete hashBuilderFactory;
}

TEST(JoinSpillProactiveTest, ExprKeyForcedEobSpill)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    auto cfg = TestSubPartitionConfig();
    const std::string spillPath = GenerateSpillPath();
    mkdir(spillPath.c_str(), 0750);

    DataTypes buildTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    auto *addLeft = new expr::FieldExpr(0, LongType());
    auto *addRight = new expr::LiteralExpr(0L, LongType());
    std::vector<expr::Expr *> buildHashKeys = {
        new expr::BinaryExpr(expr::Operator::ADD, addLeft, addRight, LongType())
    };
    auto overflowConfig = new expr::OverflowConfig();
    auto *hashBuilderExprFactory = HashBuilderWithExprOperatorFactory::CreateHashBuilderWithExprOperatorFactory(
        OMNI_JOIN_TYPE_INNER, buildTypes, buildHashKeys, 1, overflowConfig);
    auto *hashBuilderFactory = hashBuilderExprFactory->GetHashBuilderOperatorFactory();
    auto state = std::make_shared<JoinSpillState>(spillPath, hashBuilderFactory->GetBuildTypes(),
        hashBuilderFactory->GetBuildTypes(), cfg.numSubPartitions);
    hashBuilderFactory->SetJoinSpillPolicy(true, cfg);
    hashBuilderFactory->SetJoinSpillState(state);
    hashBuilderFactory->SetSpillConfig(MakeSpillConfig(spillPath));
    state->SetHandoffSessionEnabled(true);
    state->SetInitialRuntimeLayout(JoinBuildRuntimeLayout::Single);
    hashBuilderFactory->SetJoinSpillSubPartitionPolicy(true, 0, cfg, true, true);

    int32_t probeOutputCols[] = {0, 1};
    int32_t buildOutputCols[] = {0, 1};
    auto *probeAddLeft = new expr::FieldExpr(0, LongType());
    auto *probeAddRight = new expr::LiteralExpr(0L, LongType());
    std::vector<expr::Expr *> probeHashKeys = {
        new expr::BinaryExpr(expr::Operator::ADD, probeAddLeft, probeAddRight, LongType())
    };
    auto *lookupExprFactory = new LookupJoinWithExprOperatorFactory(buildTypes,
        probeOutputCols, 2, probeHashKeys, 1, buildOutputCols, 2, buildTypes,
        reinterpret_cast<int64_t>(hashBuilderFactory), nullptr, false, overflowConfig, TestJoinQueryConfig(true));

    auto *hashBuilder = hashBuilderExprFactory->CreateOperator(&pool);
    hashBuilder->AddInput(MakeLongBatch({1, 2, 3, 4}, {10, 20, 30, 40}));
    auto *lookupJoin = lookupExprFactory->CreateOperator(&pool);
    lookupJoin->AddInput(MakeLongBatch({2, 4}, {200, 400}));
    const int32_t outputRows = PumpJoin(hashBuilder, lookupJoin);

    EXPECT_EQ(outputRows, 2);
    EXPECT_GT(state->BuildSpilledRows(), 0U);
    EXPECT_GE(hashBuilderFactory->GetBuildTypes().GetSize(), 1);

    expr::Expr::DeleteExprs(buildHashKeys);
    expr::Expr::DeleteExprs(probeHashKeys);
    op::Operator::DeleteOperator(lookupJoin);
    op::Operator::DeleteOperator(hashBuilder);
    delete lookupExprFactory;
    delete hashBuilderExprFactory;
    delete overflowConfig;
}

TEST(JoinSpillProactiveTest, LeftOppositeForcedEobSpill)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    auto cfg = TestSubPartitionConfig();
    const std::string spillPath = GenerateSpillPath();
    mkdir(spillPath.c_str(), 0750);

    DataTypes rowTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    int32_t hashCols[] = {0};
    auto *hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(OMNI_JOIN_TYPE_LEFT,
        OMNI_BUILD_RIGHT, rowTypes, hashCols, 1, 1);
    auto state = std::make_shared<JoinSpillState>(spillPath, rowTypes, rowTypes, cfg.numSubPartitions);
    int32_t probeOutputCols[] = {0, 1};
    int32_t buildOutputCols[] = {0, 1};
    auto *lookupFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(rowTypes, probeOutputCols, 2,
        hashCols, 1, buildOutputCols, 2, rowTypes, reinterpret_cast<int64_t>(hashBuilderFactory), nullptr, false,
        nullptr);
    EnableJoinSpill(hashBuilderFactory, lookupFactory, state, cfg, MakeSpillConfig(spillPath), true);

    auto *hashBuilder = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator(&pool));
    hashBuilder->AddInput(MakeLongBatch({1, 2}, {10, 20}));
    auto *lookupJoin = dynamic_cast<LookupJoinOperator *>(lookupFactory->CreateOperator(&pool));
    lookupJoin->AddInput(MakeLongBatch({2, 99}, {200, 990}));
    const int32_t outputRows = PumpJoin(hashBuilder, lookupJoin);

    EXPECT_EQ(outputRows, 2);
    EXPECT_GT(state->BuildSpilledRows(), 0U);

    op::Operator::DeleteOperator(lookupJoin);
    op::Operator::DeleteOperator(hashBuilder);
    delete lookupFactory;
    delete hashBuilderFactory;
}

TEST(JoinSpillProactiveTest, FullOuterForcedEobSpill)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    auto cfg = TestSubPartitionConfig();
    const std::string spillPath = GenerateSpillPath();
    mkdir(spillPath.c_str(), 0750);

    DataTypes rowTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    int32_t hashCols[] = {0};
    auto *hashBuilderFactory =
        HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(OMNI_JOIN_TYPE_FULL, rowTypes, hashCols, 1, 1);
    auto state = std::make_shared<JoinSpillState>(spillPath, rowTypes, rowTypes, cfg.numSubPartitions);
    int32_t probeOutputCols[] = {0, 1};
    int32_t buildOutputCols[] = {0, 1};
    auto *lookupFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(rowTypes, probeOutputCols, 2,
        hashCols, 1, buildOutputCols, 2, rowTypes, reinterpret_cast<int64_t>(hashBuilderFactory), nullptr, false,
        nullptr);
    EnableJoinSpill(hashBuilderFactory, lookupFactory, state, cfg, MakeSpillConfig(spillPath), true);

    auto *hashBuilder = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator(&pool));
    hashBuilder->AddInput(MakeLongBatch({1, 2}, {10, 20}));
    auto *lookupJoin = dynamic_cast<LookupJoinOperator *>(lookupFactory->CreateOperator(&pool));
    lookupJoin->AddInput(MakeLongBatch({2, 99}, {200, 990}));
    const int32_t outputRows = PumpJoin(hashBuilder, lookupJoin);

    EXPECT_EQ(outputRows, 3);
    EXPECT_GT(state->BuildSpilledRows(), 0U);

    op::Operator::DeleteOperator(lookupJoin);
    op::Operator::DeleteOperator(hashBuilder);
    delete lookupFactory;
    delete hashBuilderFactory;
}

namespace {

using join_spill_dual_run::TupleMultiset;
using join_spill_dual_run::SortTupleMultiset;
using join_spill_dual_run::AppendCanonicalRowsAndFree;

struct FullTableUnvisitedDrain {
    HashBuilderOperatorFactory *hashFactory = nullptr;
    BuildSide buildSide = OMNI_BUILD_UNKNOWN;
};

// Pump both operators to completion after signalling end-of-stream and collect every probe output
// row as a canonical (type, value) tuple multiset, so spill and no-spill runs can be compared
// exactly regardless of output column order.
void DrainJoinToCanonical(op::Operator *builder, op::Operator *probe, TupleMultiset *rows,
    const FullTableUnvisitedDrain *fullTableUnvisited = nullptr, bool reclaimHeld = false)
{
    builder->noMoreInput();
    probe->noMoreInput();
    auto *lookup = dynamic_cast<LookupJoinOperator *>(probe);
    for (int32_t i = 0; i < 20000 && (builder->GetStatus() != OMNI_STATUS_FINISHED ||
        probe->GetStatus() != OMNI_STATUS_FINISHED); ++i) {
        if (probe->isFinished() && lookup != nullptr && lookup->NeedsUnvisitedBuildOutput() &&
            fullTableUnvisited != nullptr) {
            break; // The explicit outer scanner below owns the remaining build rows.
        }
        VectorBatch *buildOut = nullptr;
        builder->GetOutput(&buildOut);
        if (reclaimHeld && lookup != nullptr) {
            lookup->reclaim(std::numeric_limits<int64_t>::max());
        }
        VectorBatch *probeOut = nullptr;
        probe->GetOutput(&probeOut);
        if (probeOut != nullptr) {
            AppendCanonicalRowsAndFree(probeOut, rows);
        }
    }
    if (lookup != nullptr && lookup->NeedsUnvisitedBuildOutput() && fullTableUnvisited != nullptr &&
        fullTableUnvisited->hashFactory != nullptr) {
        const DataTypes types({LongType(), LongType()});
        int32_t probeOutputCols[] = {0, 1};
        int32_t buildOutputCols[] = {0, 1};
        auto *outerFactory = LookupOuterJoinOperatorFactory::CreateLookupOuterJoinOperatorFactory(types,
            probeOutputCols, 2, buildOutputCols, types,
            reinterpret_cast<int64_t>(fullTableUnvisited->hashFactory), fullTableUnvisited->buildSide);
        auto *outer = dynamic_cast<LookupOuterJoinOperator *>(outerFactory->CreateOperator(builder->pool()));
        for (int32_t g = 0; g < 100000 && !outer->isFinished(); ++g) {
            VectorBatch *output = nullptr;
            outer->GetOutput(&output);
            if (output != nullptr) {
                AppendCanonicalRowsAndFree(output, rows);
            }
        }
        EXPECT_TRUE(outer->isFinished()) << "outer scan exceeded iteration limit";
        lookup->FinishFullTableProbeAfterOuter();
        VectorBatch *buildOut = nullptr;
        builder->GetOutput(&buildOut);
        if (buildOut != nullptr) {
            VectorHelper::FreeVecBatch(buildOut);
        }
        omniruntime::op::Operator::DeleteOperator(outer);
        delete outerFactory;
    }
    EXPECT_TRUE(builder->isFinished()) << "build did not finish after drain";
    EXPECT_TRUE(probe->isFinished()) << "probe did not finish after drain";
}

void ExpectEqualTuples(const TupleMultiset &expected, const TupleMultiset &actual)
{
    TupleMultiset a = expected;
    TupleMultiset b = actual;
    SortTupleMultiset(&a);
    SortTupleMultiset(&b);
    EXPECT_EQ(a, b);
}

bool JoinNeedsUnvisitedBuildOutput(JoinType joinType, BuildSide buildSide)
{
    return joinType == OMNI_JOIN_TYPE_FULL ||
        (joinType == OMNI_JOIN_TYPE_LEFT && buildSide == OMNI_BUILD_LEFT) ||
        (joinType == OMNI_JOIN_TYPE_RIGHT && buildSide == OMNI_BUILD_RIGHT);
}

VectorBatch *MakeVarcharKeyLongValueBatch(const std::vector<std::string> &keys,
    const std::vector<int64_t> &values)
{
    auto *batch = new VectorBatch(static_cast<int32_t>(keys.size()));
    auto *keyVec = new Vector<LargeStringContainer<std::string_view>>(static_cast<int32_t>(keys.size()));
    auto *valueVec = new Vector<int64_t>(static_cast<int32_t>(values.size()));
    for (size_t i = 0; i < keys.size(); ++i) {
        keyVec->SetValue(static_cast<int32_t>(i), std::string_view(keys[i]));
        valueVec->SetValue(static_cast<int32_t>(i), values[i]);
    }
    batch->Append(keyVec);
    batch->Append(valueVec);
    return batch;
}

struct InnerJoinHarness {
    std::shared_ptr<JoinSpillState> state;
    HashBuilderOperatorFactory *hashFactory = nullptr;
    LookupJoinOperatorFactory *lookupFactory = nullptr;
};

InnerJoinHarness MakeFieldInnerHarness(const std::string &spillDir, const DataTypes &rowTypes,
    bool spillSession, bool forceSpill, JoinType joinType = OMNI_JOIN_TYPE_INNER,
    BuildSide buildSide = OMNI_BUILD_UNKNOWN)
{
    InnerJoinHarness h;
    int32_t hashCols[] = {0};
    h.hashFactory = buildSide == OMNI_BUILD_UNKNOWN
        ? HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(joinType, rowTypes, hashCols, 1, 1)
        : HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(joinType, buildSide, rowTypes, hashCols, 1,
              1);
    if (spillSession) {
        auto cfg = TestSubPartitionConfig();
        h.state = std::make_shared<JoinSpillState>(spillDir, rowTypes, rowTypes, cfg.numSubPartitions);
        h.state->SetHandoffSessionEnabled(true);
        h.state->SetInitialRuntimeLayout(JoinBuildRuntimeLayout::Single);
        h.hashFactory->SetJoinSpillSubPartitionPolicy(true, 0, cfg, true, forceSpill);
        h.hashFactory->SetJoinSpillState(h.state);
        h.hashFactory->SetSpillConfig(MakeSpillConfig(spillDir));
    }
    int32_t probeOutputCols[] = {0, 1};
    int32_t buildOutputCols[] = {0, 1};
    h.lookupFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(rowTypes, probeOutputCols, 2,
        hashCols, 1, buildOutputCols, 2, rowTypes, reinterpret_cast<int64_t>(h.hashFactory), nullptr, false,
        nullptr);
    if (spillSession) {
        h.lookupFactory->SetJoinSpillPolicy(true, TestSubPartitionConfig(), true);
        h.lookupFactory->SetJoinSpillState(h.state);
    }
    return h;
}

void DestroyInnerHarness(const InnerJoinHarness &h, op::Operator *build, op::Operator *probe)
{
    op::Operator::DeleteOperator(probe);
    op::Operator::DeleteOperator(build);
    delete h.lookupFactory;
    delete h.hashFactory;
}

struct ExprRunResult {
    std::shared_ptr<JoinSpillState> state;
    TupleMultiset rows;
    uint64_t spilledRows = 0;
    int32_t buildTypesSize = 0;
};

ExprRunResult RunExprPlusOneInner(bool spillSession, bool forceSpill, const std::string &spillDir)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    ExprRunResult out;
    const DataTypes buildTypes(std::vector<DataTypePtr>({LongType(), LongType()}));
    auto *overflowConfig = new expr::OverflowConfig();
    auto *buildAddLeft = new expr::FieldExpr(0, LongType());
    auto *buildAddRight = new expr::LiteralExpr(1L, LongType());
    std::vector<expr::Expr *> buildHashKeys = {
        new expr::BinaryExpr(expr::Operator::ADD, buildAddLeft, buildAddRight, LongType())
    };
    auto *buildExprFactory = HashBuilderWithExprOperatorFactory::CreateHashBuilderWithExprOperatorFactory(
        OMNI_JOIN_TYPE_INNER, buildTypes, buildHashKeys, 1, overflowConfig);
    auto *hashFactory = buildExprFactory->GetHashBuilderOperatorFactory();
    if (spillSession) {
        auto cfg = TestSubPartitionConfig();
        out.state = std::make_shared<JoinSpillState>(
            spillDir, hashFactory->GetBuildTypes(), hashFactory->GetBuildTypes(), cfg.numSubPartitions);
        out.state->SetHandoffSessionEnabled(true);
        out.state->SetInitialRuntimeLayout(JoinBuildRuntimeLayout::Single);
        hashFactory->SetJoinSpillSubPartitionPolicy(true, 0, cfg, true, forceSpill);
        hashFactory->SetJoinSpillState(out.state);
        hashFactory->SetSpillConfig(MakeSpillConfig(spillDir));
    }
    out.buildTypesSize = hashFactory->GetBuildTypes().GetSize();

    int32_t probeOutputCols[] = {0, 1};
    int32_t buildOutputCols[] = {0, 1};
    auto *probeAddLeft = new expr::FieldExpr(0, LongType());
    auto *probeAddRight = new expr::LiteralExpr(1L, LongType());
    std::vector<expr::Expr *> probeHashKeys = {
        new expr::BinaryExpr(expr::Operator::ADD, probeAddLeft, probeAddRight, LongType())
    };
    auto *lookupExprFactory = new LookupJoinWithExprOperatorFactory(buildTypes,
        probeOutputCols, 2, probeHashKeys, 1, buildOutputCols, 2, buildTypes,
        reinterpret_cast<int64_t>(hashFactory), nullptr, false, overflowConfig, TestJoinQueryConfig(spillSession));

    auto *builder = buildExprFactory->CreateOperator(&pool);
    auto *probe = lookupExprFactory->CreateOperator(&pool);
    // Build raw keys 0..3 -> expr keys 1..4; probe raw keys {1,2,9} -> expr keys {2,3,10}.
    builder->AddInput(MakeLongBatch({0, 1, 2, 3}, {10, 20, 30, 40}));
    probe->AddInput(MakeLongBatch({1, 2, 9}, {101, 102, 109}));
    DrainJoinToCanonical(builder, probe, &out.rows);
    if (out.state != nullptr) {
        out.spilledRows = out.state->BuildSpilledRows();
    }

    op::Operator::DeleteOperator(probe);
    op::Operator::DeleteOperator(builder);
    expr::Expr::DeleteExprs(buildHashKeys);
    expr::Expr::DeleteExprs(probeHashKeys);
    delete lookupExprFactory;
    delete buildExprFactory;
    delete overflowConfig;
    return out;
}

// INT join-key batches: flat Vector<int32_t> key plus a LONG payload column.
VectorBatch *MakeIntKeyLongValueBatch(const std::vector<int32_t> &keys, const std::vector<int64_t> &values)
{
    auto *batch = new VectorBatch(static_cast<int32_t>(keys.size()));
    auto *keyVec = new Vector<int32_t>(static_cast<int32_t>(keys.size()));
    auto *valueVec = new Vector<int64_t>(static_cast<int32_t>(values.size()));
    for (size_t i = 0; i < keys.size(); ++i) {
        keyVec->SetValue(static_cast<int32_t>(i), keys[i]);
        valueVec->SetValue(static_cast<int32_t>(i), values[i]);
    }
    batch->Append(keyVec);
    batch->Append(valueVec);
    return batch;
}

// Two LONG join-key columns (col0, col1) plus a LONG payload column (col2).
VectorBatch *MakeTwoLongKeyBatch(const std::vector<int64_t> &keys1, const std::vector<int64_t> &keys2,
    const std::vector<int64_t> &values)
{
    auto *batch = new VectorBatch(static_cast<int32_t>(keys1.size()));
    auto *key1Vec = new Vector<int64_t>(static_cast<int32_t>(keys1.size()));
    auto *key2Vec = new Vector<int64_t>(static_cast<int32_t>(keys2.size()));
    auto *valueVec = new Vector<int64_t>(static_cast<int32_t>(values.size()));
    for (size_t i = 0; i < keys1.size(); ++i) {
        key1Vec->SetValue(static_cast<int32_t>(i), keys1[i]);
        key2Vec->SetValue(static_cast<int32_t>(i), keys2[i]);
        valueVec->SetValue(static_cast<int32_t>(i), values[i]);
    }
    batch->Append(key1Vec);
    batch->Append(key2Vec);
    batch->Append(valueVec);
    return batch;
}

InnerJoinHarness MakeCompositeInnerHarness(const std::string &spillDir, bool spillSession, bool forceSpill,
    BuildSide buildSide = OMNI_BUILD_UNKNOWN)
{
    InnerJoinHarness h;
    const DataTypes rowTypes(std::vector<DataTypePtr>({LongType(), LongType(), LongType()}));
    int32_t hashCols[] = {0, 1};
    h.hashFactory = buildSide == OMNI_BUILD_UNKNOWN
        ? HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(OMNI_JOIN_TYPE_INNER, rowTypes, hashCols, 2, 1)
        : HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(OMNI_JOIN_TYPE_INNER, buildSide, rowTypes,
              hashCols, 2, 1);
    if (spillSession) {
        auto cfg = TestSubPartitionConfig();
        h.state = std::make_shared<JoinSpillState>(spillDir, rowTypes, rowTypes, cfg.numSubPartitions);
        h.state->SetHandoffSessionEnabled(true);
        h.state->SetInitialRuntimeLayout(JoinBuildRuntimeLayout::Single);
        h.hashFactory->SetJoinSpillSubPartitionPolicy(true, 0, cfg, true, forceSpill);
        h.hashFactory->SetJoinSpillState(h.state);
        h.hashFactory->SetSpillConfig(MakeSpillConfig(spillDir));
    }
    int32_t probeOutputCols[] = {0, 1, 2};
    int32_t buildOutputCols[] = {0, 1, 2};
    h.lookupFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(rowTypes, probeOutputCols, 3,
        hashCols, 2, buildOutputCols, 3, rowTypes, reinterpret_cast<int64_t>(h.hashFactory), nullptr, false,
        nullptr);
    if (spillSession) {
        h.lookupFactory->SetJoinSpillPolicy(true, TestSubPartitionConfig(), true);
        h.lookupFactory->SetJoinSpillState(h.state);
    }
    return h;
}

void ExpectFieldKeyForcedSpillEqualsNoSpill(const char *tag, const DataTypes &rowTypes,
    const std::function<VectorBatch *()> &makeBuild, const std::function<VectorBatch *()> &makeProbe,
    size_t expectedRows, JoinType joinType = OMNI_JOIN_TYPE_INNER,
    BuildSide buildSide = OMNI_BUILD_UNKNOWN)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    auto dir = join_spill_dual_run::ScopedTempDirectory(tag);
    auto noSpill =
        MakeFieldInnerHarness(dir.path().string(), rowTypes, false, false, joinType, buildSide);
    auto *build0 = dynamic_cast<HashBuilderOperator *>(noSpill.hashFactory->CreateOperator(&pool));
    auto *probe0 = dynamic_cast<LookupJoinOperator *>(noSpill.lookupFactory->CreateOperator(&pool));
    build0->AddInput(makeBuild());
    probe0->AddInput(makeProbe());
    TupleMultiset expected;
    DrainJoinToCanonical(build0, probe0, &expected);
    DestroyInnerHarness(noSpill, build0, probe0);
    EXPECT_EQ(expected.size(), expectedRows);

    auto spill =
        MakeFieldInnerHarness(dir.path().string(), rowTypes, true, true, joinType, buildSide);
    auto *build1 = dynamic_cast<HashBuilderOperator *>(spill.hashFactory->CreateOperator(&pool));
    auto *probe1 = dynamic_cast<LookupJoinOperator *>(spill.lookupFactory->CreateOperator(&pool));
    build1->AddInput(makeBuild());
    probe1->AddInput(makeProbe());
    TupleMultiset actual;
    DrainJoinToCanonical(build1, probe1, &actual);
    DestroyInnerHarness(spill, build1, probe1);

    ExpectEqualTuples(expected, actual);
    EXPECT_GT(spill.state->BuildSpilledRows(), 0u);
    EXPECT_LE(spill.state->BuildSpilledFiles(), 4u);
}

enum class ExprKeyFlavor {
    LongPlusOne,
    IntPlusOne,
    VarcharCastLong,
    Date32PlusOne,
};

ExprRunResult RunExprKeyJoin(ExprKeyFlavor flavor, bool spillSession, bool forceSpill,
    const std::string &spillDir, JoinType joinType = OMNI_JOIN_TYPE_INNER,
    BuildSide buildSide = OMNI_BUILD_UNKNOWN)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    ExprRunResult out;
    auto *overflowConfig = new expr::OverflowConfig();
    DataTypes inputTypes;
    std::vector<expr::Expr *> buildHashKeys;
    std::vector<expr::Expr *> probeHashKeys;
    std::function<void(op::Operator *, op::Operator *)> feed;
    switch (flavor) {
        case ExprKeyFlavor::LongPlusOne:
            inputTypes = DataTypes(std::vector<DataTypePtr>({LongType(), LongType()}));
            buildHashKeys = {new expr::BinaryExpr(expr::Operator::ADD, new expr::FieldExpr(0, LongType()),
                new expr::LiteralExpr(1L, LongType()), LongType())};
            probeHashKeys = {new expr::BinaryExpr(expr::Operator::ADD, new expr::FieldExpr(0, LongType()),
                new expr::LiteralExpr(1L, LongType()), LongType())};
            feed = [](op::Operator *builder, op::Operator *probe) {
                builder->AddInput(MakeLongBatch({0, 1, 2, 3}, {10, 20, 30, 40}));
                probe->AddInput(MakeLongBatch({1, 2, 9}, {101, 102, 109}));
            };
            break;
        case ExprKeyFlavor::IntPlusOne:
            inputTypes = DataTypes(std::vector<DataTypePtr>({IntType(), LongType()}));
            buildHashKeys = {new expr::BinaryExpr(expr::Operator::ADD, new expr::FieldExpr(0, IntType()),
                new expr::LiteralExpr(1, IntType()), IntType())};
            probeHashKeys = {new expr::BinaryExpr(expr::Operator::ADD, new expr::FieldExpr(0, IntType()),
                new expr::LiteralExpr(1, IntType()), IntType())};
            feed = [](op::Operator *builder, op::Operator *probe) {
                builder->AddInput(MakeIntKeyLongValueBatch({0, 1, 2, 3}, {10, 20, 30, 40}));
                probe->AddInput(MakeIntKeyLongValueBatch({1, 2, 9}, {101, 102, 109}));
            };
            break;
        case ExprKeyFlavor::VarcharCastLong:
            inputTypes = DataTypes(std::vector<DataTypePtr>({LongType(), LongType()}));
            buildHashKeys = {
                new expr::FuncExpr("CAST", {new expr::FieldExpr(0, LongType())}, VarcharType(50))};
            probeHashKeys = {
                new expr::FuncExpr("CAST", {new expr::FieldExpr(0, LongType())}, VarcharType(50))};
            feed = [](op::Operator *builder, op::Operator *probe) {
                builder->AddInput(MakeLongBatch({0, 1, 2, 3}, {10, 20, 30, 40}));
                probe->AddInput(MakeLongBatch({1, 2, 9}, {101, 102, 109}));
            };
            break;
        case ExprKeyFlavor::Date32PlusOne:
            inputTypes = DataTypes(std::vector<DataTypePtr>({Date32Type(DAY), LongType()}));
            buildHashKeys = {new expr::BinaryExpr(expr::Operator::ADD, new expr::FieldExpr(0, Date32Type(DAY)),
                new expr::LiteralExpr(1, Date32Type(DAY)), Date32Type(DAY))};
            probeHashKeys = {new expr::BinaryExpr(expr::Operator::ADD, new expr::FieldExpr(0, Date32Type(DAY)),
                new expr::LiteralExpr(1, Date32Type(DAY)), Date32Type(DAY))};
            feed = [](op::Operator *builder, op::Operator *probe) {
                builder->AddInput(MakeDate32Batch({10, 20, 30, 40}, {10, 20, 30, 40}));
                probe->AddInput(MakeDate32Batch({20, 40, 99}, {200, 400, 199}));
            };
            break;
    }

    auto *buildExprFactory = buildSide == OMNI_BUILD_UNKNOWN
        ? HashBuilderWithExprOperatorFactory::CreateHashBuilderWithExprOperatorFactory(joinType,
              inputTypes, buildHashKeys, 1, overflowConfig)
        : HashBuilderWithExprOperatorFactory::CreateHashBuilderWithExprOperatorFactory(joinType,
              buildSide, inputTypes, buildHashKeys, 1, overflowConfig);
    auto *hashFactory = buildExprFactory->GetHashBuilderOperatorFactory();
    if (spillSession) {
        auto cfg = TestSubPartitionConfig();
        out.state = std::make_shared<JoinSpillState>(
            spillDir, hashFactory->GetBuildTypes(), hashFactory->GetBuildTypes(), cfg.numSubPartitions);
        out.state->SetHandoffSessionEnabled(true);
        out.state->SetInitialRuntimeLayout(JoinBuildRuntimeLayout::Single);
        hashFactory->SetJoinSpillSubPartitionPolicy(true, 0, cfg, true, forceSpill);
        hashFactory->SetJoinSpillState(out.state);
        hashFactory->SetSpillConfig(MakeSpillConfig(spillDir));
    }
    out.buildTypesSize = hashFactory->GetBuildTypes().GetSize();

    int32_t probeOutputCols[] = {0, 1};
    int32_t buildOutputCols[] = {0, 1};
    auto *lookupExprFactory = new LookupJoinWithExprOperatorFactory(inputTypes,
        probeOutputCols, 2, probeHashKeys, 1, buildOutputCols, 2, inputTypes,
        reinterpret_cast<int64_t>(hashFactory), nullptr, false, overflowConfig, TestJoinQueryConfig(spillSession));

    auto *builder = buildExprFactory->CreateOperator(&pool);
    auto *probe = lookupExprFactory->CreateOperator(&pool);
    feed(builder, probe);
    DrainJoinToCanonical(builder, probe, &out.rows);
    if (out.state != nullptr) {
        out.spilledRows = out.state->BuildSpilledRows();
    }

    op::Operator::DeleteOperator(probe);
    op::Operator::DeleteOperator(builder);
    expr::Expr::DeleteExprs(buildHashKeys);
    expr::Expr::DeleteExprs(probeHashKeys);
    delete lookupExprFactory;
    delete buildExprFactory;
    delete overflowConfig;
    return out;
}

void ExpectExprKeyForcedSpillEqualsNoSpill(const char *tag, ExprKeyFlavor flavor, size_t expectedRows,
    JoinType joinType = OMNI_JOIN_TYPE_INNER, BuildSide buildSide = OMNI_BUILD_UNKNOWN)
{
    auto dir = join_spill_dual_run::ScopedTempDirectory(tag);
    const auto noSpill =
        RunExprKeyJoin(flavor, false, false, dir.path().string(), joinType, buildSide);
    const auto spill = RunExprKeyJoin(flavor, true, true, dir.path().string(), joinType, buildSide);
    ExpectEqualTuples(noSpill.rows, spill.rows);
    EXPECT_EQ(noSpill.rows.size(), expectedRows);
    EXPECT_GT(spill.spilledRows, 0u);
    EXPECT_GT(noSpill.buildTypesSize, 2);
    EXPECT_EQ(spill.buildTypesSize, noSpill.buildTypesSize);
}

} // namespace

TEST(JoinSpillProactiveTest, FieldKeyLongForcedSpillEqualsNoSpill)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    auto dir = join_spill_dual_run::ScopedTempDirectory("proactive-long");
    const DataTypes rowTypes(std::vector<DataTypePtr>({LongType(), LongType()}));

    auto noSpill = MakeFieldInnerHarness(dir.path().string(), rowTypes, /*spillSession=*/false, false);
    auto *build0 = dynamic_cast<HashBuilderOperator *>(noSpill.hashFactory->CreateOperator(&pool));
    auto *probe0 = dynamic_cast<LookupJoinOperator *>(noSpill.lookupFactory->CreateOperator(&pool));
    build0->AddInput(MakeLongBatch({1, 2, 3, 4, 5, 6, 7, 8}, {10, 20, 30, 40, 50, 60, 70, 80}));
    probe0->AddInput(MakeLongBatch({1, 3, 5, 8, 99}, {101, 103, 105, 108, 199}));
    TupleMultiset expected;
    DrainJoinToCanonical(build0, probe0, &expected);
    DestroyInnerHarness(noSpill, build0, probe0);
    EXPECT_EQ(expected.size(), 4u); // keys 1/3/5/8 match; 99 misses

    auto spill = MakeFieldInnerHarness(dir.path().string(), rowTypes, /*spillSession=*/true, /*forceSpill=*/true);
    auto *build1 = dynamic_cast<HashBuilderOperator *>(spill.hashFactory->CreateOperator(&pool));
    auto *probe1 = dynamic_cast<LookupJoinOperator *>(spill.lookupFactory->CreateOperator(&pool));
    build1->AddInput(MakeLongBatch({1, 2, 3, 4, 5, 6, 7, 8}, {10, 20, 30, 40, 50, 60, 70, 80}));
    probe1->AddInput(MakeLongBatch({1, 3, 5, 8, 99}, {101, 103, 105, 108, 199}));
    TupleMultiset actual;
    DrainJoinToCanonical(build1, probe1, &actual);
    DestroyInnerHarness(spill, build1, probe1);

    ExpectEqualTuples(expected, actual);
    EXPECT_GT(spill.state->BuildSpilledRows(), 0u);
    EXPECT_LE(spill.state->BuildSpilledFiles(), 4u); // sticky writer: one file per used sub-partition
}

TEST(JoinSpillProactiveTest, VarcharKeyFieldForcedSpillEqualsNoSpill)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    auto dir = join_spill_dual_run::ScopedTempDirectory("proactive-varchar");
    const DataTypes rowTypes(std::vector<DataTypePtr>({VarcharType(), LongType()}));

    auto noSpill = MakeFieldInnerHarness(dir.path().string(), rowTypes, /*spillSession=*/false, false);
    auto *build0 = dynamic_cast<HashBuilderOperator *>(noSpill.hashFactory->CreateOperator(&pool));
    auto *probe0 = dynamic_cast<LookupJoinOperator *>(noSpill.lookupFactory->CreateOperator(&pool));
    build0->AddInput(MakeVarcharKeyLongValueBatch({"alpha", "beta", "gamma", "delta", "zeta"}, {10, 20, 30, 40, 50}));
    probe0->AddInput(MakeVarcharKeyLongValueBatch({"alpha", "delta", "missing"}, {101, 104, 199}));
    TupleMultiset expected;
    DrainJoinToCanonical(build0, probe0, &expected);
    DestroyInnerHarness(noSpill, build0, probe0);
    EXPECT_EQ(expected.size(), 2u);

    auto spill = MakeFieldInnerHarness(dir.path().string(), rowTypes, /*spillSession=*/true, /*forceSpill=*/true);
    auto *build1 = dynamic_cast<HashBuilderOperator *>(spill.hashFactory->CreateOperator(&pool));
    auto *probe1 = dynamic_cast<LookupJoinOperator *>(spill.lookupFactory->CreateOperator(&pool));
    build1->AddInput(MakeVarcharKeyLongValueBatch({"alpha", "beta", "gamma", "delta", "zeta"}, {10, 20, 30, 40, 50}));
    probe1->AddInput(MakeVarcharKeyLongValueBatch({"alpha", "delta", "missing"}, {101, 104, 199}));
    TupleMultiset actual;
    DrainJoinToCanonical(build1, probe1, &actual);
    DestroyInnerHarness(spill, build1, probe1);

    ExpectEqualTuples(expected, actual);
    EXPECT_GT(spill.state->BuildSpilledRows(), 0u);
    EXPECT_LE(spill.state->BuildSpilledFiles(), 4u);
}

TEST(JoinSpillProactiveTest, ExprTransformKeyForcedSpillEqualsNoSpillKeepsProjectedKey)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    auto dir = join_spill_dual_run::ScopedTempDirectory("proactive-expr");
    const auto noSpill = RunExprPlusOneInner(/*spillSession=*/false, /*forceSpill=*/false, dir.path().string());
    const auto spill = RunExprPlusOneInner(/*spillSession=*/true, /*forceSpill=*/true, dir.path().string());

    // In-memory expr join is the golden reference; spill must produce the same projected-key results.
    ExpectEqualTuples(noSpill.rows, spill.rows);
    EXPECT_EQ(noSpill.rows.size(), 2u); // probe 1->2, 2->3 match build 0..3 (keys become 1..4); 9->10 misses
    EXPECT_GT(spill.spilledRows, 0u);
    EXPECT_GT(noSpill.buildTypesSize, 2); // projected schema = 2 output cols + expr key col
    EXPECT_EQ(spill.buildTypesSize, noSpill.buildTypesSize);
}

// P0-2a: INT field join keys must behave identically with and without spill.
TEST(JoinSpillProactiveTest, IntFieldKeyForcedSpillEqualsNoSpill)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    auto dir = join_spill_dual_run::ScopedTempDirectory("proactive-int");
    const DataTypes rowTypes(std::vector<DataTypePtr>({IntType(), LongType()}));

    auto noSpill = MakeFieldInnerHarness(dir.path().string(), rowTypes, /*spillSession=*/false, false);
    auto *build0 = dynamic_cast<HashBuilderOperator *>(noSpill.hashFactory->CreateOperator(&pool));
    auto *probe0 = dynamic_cast<LookupJoinOperator *>(noSpill.lookupFactory->CreateOperator(&pool));
    build0->AddInput(MakeIntKeyLongValueBatch({1, 2, 3, 4, 5}, {10, 20, 30, 40, 50}));
    probe0->AddInput(MakeIntKeyLongValueBatch({2, 4, 99}, {200, 400, 199}));
    TupleMultiset expected;
    DrainJoinToCanonical(build0, probe0, &expected);
    DestroyInnerHarness(noSpill, build0, probe0);
    EXPECT_EQ(expected.size(), 2u);

    auto spill = MakeFieldInnerHarness(dir.path().string(), rowTypes, /*spillSession=*/true, /*forceSpill=*/true);
    auto *build1 = dynamic_cast<HashBuilderOperator *>(spill.hashFactory->CreateOperator(&pool));
    auto *probe1 = dynamic_cast<LookupJoinOperator *>(spill.lookupFactory->CreateOperator(&pool));
    build1->AddInput(MakeIntKeyLongValueBatch({1, 2, 3, 4, 5}, {10, 20, 30, 40, 50}));
    probe1->AddInput(MakeIntKeyLongValueBatch({2, 4, 99}, {200, 400, 199}));
    TupleMultiset actual;
    DrainJoinToCanonical(build1, probe1, &actual);
    DestroyInnerHarness(spill, build1, probe1);

    ExpectEqualTuples(expected, actual);
    EXPECT_GT(spill.state->BuildSpilledRows(), 0u);
    EXPECT_LE(spill.state->BuildSpilledFiles(), 4u);
}
// P0-2b: DATE32 field join keys must behave identically with and without spill.
TEST(JoinSpillProactiveTest, Date32FieldKeyForcedSpillEqualsNoSpill)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    auto dir = join_spill_dual_run::ScopedTempDirectory("proactive-date32");
    const DataTypes rowTypes(std::vector<DataTypePtr>({Date32Type(DAY), LongType()}));

    auto noSpill = MakeFieldInnerHarness(dir.path().string(), rowTypes, /*spillSession=*/false, false);
    auto *build0 = dynamic_cast<HashBuilderOperator *>(noSpill.hashFactory->CreateOperator(&pool));
    auto *probe0 = dynamic_cast<LookupJoinOperator *>(noSpill.lookupFactory->CreateOperator(&pool));
    build0->AddInput(MakeDate32Batch({10, 20, 30, 40}, {10, 20, 30, 40}));
    probe0->AddInput(MakeDate32Batch({20, 40, 99}, {200, 400, 199}));
    TupleMultiset expected;
    DrainJoinToCanonical(build0, probe0, &expected);
    DestroyInnerHarness(noSpill, build0, probe0);
    EXPECT_EQ(expected.size(), 2u);

    auto spill = MakeFieldInnerHarness(dir.path().string(), rowTypes, /*spillSession=*/true, /*forceSpill=*/true);
    auto *build1 = dynamic_cast<HashBuilderOperator *>(spill.hashFactory->CreateOperator(&pool));
    auto *probe1 = dynamic_cast<LookupJoinOperator *>(spill.lookupFactory->CreateOperator(&pool));
    build1->AddInput(MakeDate32Batch({10, 20, 30, 40}, {10, 20, 30, 40}));
    probe1->AddInput(MakeDate32Batch({20, 40, 99}, {200, 400, 199}));
    TupleMultiset actual;
    DrainJoinToCanonical(build1, probe1, &actual);
    DestroyInnerHarness(spill, build1, probe1);

    ExpectEqualTuples(expected, actual);
    EXPECT_GT(spill.state->BuildSpilledRows(), 0u);
    EXPECT_LE(spill.state->BuildSpilledFiles(), 4u);
}

// P0-2c: composite (two LONG columns) join keys must behave identically with and without spill.
TEST(JoinSpillProactiveTest, CompositeTwoLongKeysForcedSpillEqualsNoSpill)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    auto dir = join_spill_dual_run::ScopedTempDirectory("proactive-composite");

    auto noSpill = MakeCompositeInnerHarness(dir.path().string(), /*spillSession=*/false, false);
    auto *build0 = dynamic_cast<HashBuilderOperator *>(noSpill.hashFactory->CreateOperator(&pool));
    auto *probe0 = dynamic_cast<LookupJoinOperator *>(noSpill.lookupFactory->CreateOperator(&pool));
    build0->AddInput(MakeTwoLongKeyBatch({1, 1, 2, 3}, {10, 11, 20, 30}, {100, 101, 200, 300}));
    probe0->AddInput(MakeTwoLongKeyBatch({1, 2, 99}, {11, 20, 1}, {1100, 2000, 199}));
    TupleMultiset expected;
    DrainJoinToCanonical(build0, probe0, &expected);
    DestroyInnerHarness(noSpill, build0, probe0);
    EXPECT_EQ(expected.size(), 2u); // (1,11) and (2,20) match; (99,1) misses

    auto spill = MakeCompositeInnerHarness(dir.path().string(), /*spillSession=*/true, /*forceSpill=*/true);
    auto *build1 = dynamic_cast<HashBuilderOperator *>(spill.hashFactory->CreateOperator(&pool));
    auto *probe1 = dynamic_cast<LookupJoinOperator *>(spill.lookupFactory->CreateOperator(&pool));
    build1->AddInput(MakeTwoLongKeyBatch({1, 1, 2, 3}, {10, 11, 20, 30}, {100, 101, 200, 300}));
    probe1->AddInput(MakeTwoLongKeyBatch({1, 2, 99}, {11, 20, 1}, {1100, 2000, 199}));
    TupleMultiset actual;
    DrainJoinToCanonical(build1, probe1, &actual);
    DestroyInnerHarness(spill, build1, probe1);

    ExpectEqualTuples(expected, actual);
    EXPECT_GT(spill.state->BuildSpilledRows(), 0u);
    EXPECT_LE(spill.state->BuildSpilledFiles(), 4u);
}

TEST(JoinSpillProactiveTest, FieldKeyLongInnerBuildLeftForcedSpillEqualsNoSpill)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    const DataTypes rowTypes(std::vector<DataTypePtr>({LongType(), LongType()}));
    ExpectFieldKeyForcedSpillEqualsNoSpill("proactive-long-bl", rowTypes,
        []() { return MakeLongBatch({1, 2, 3, 4, 5, 6, 7, 8}, {10, 20, 30, 40, 50, 60, 70, 80}); },
        []() { return MakeLongBatch({1, 3, 5, 8, 99}, {101, 103, 105, 108, 199}); }, 4u,
        OMNI_JOIN_TYPE_INNER, OMNI_BUILD_LEFT);
}

TEST(JoinSpillProactiveTest, IntFieldKeyInnerBuildLeftForcedSpillEqualsNoSpill)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    const DataTypes rowTypes(std::vector<DataTypePtr>({IntType(), LongType()}));
    ExpectFieldKeyForcedSpillEqualsNoSpill("proactive-int-bl", rowTypes,
        []() { return MakeIntKeyLongValueBatch({1, 2, 3, 4, 5}, {10, 20, 30, 40, 50}); },
        []() { return MakeIntKeyLongValueBatch({2, 4, 99}, {200, 400, 199}); }, 2u, OMNI_JOIN_TYPE_INNER,
        OMNI_BUILD_LEFT);
}

TEST(JoinSpillProactiveTest, Date32FieldKeyInnerBuildLeftForcedSpillEqualsNoSpill)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    const DataTypes rowTypes(std::vector<DataTypePtr>({Date32Type(DAY), LongType()}));
    ExpectFieldKeyForcedSpillEqualsNoSpill("proactive-date32-bl", rowTypes,
        []() { return MakeDate32Batch({10, 20, 30, 40}, {10, 20, 30, 40}); },
        []() { return MakeDate32Batch({20, 40, 99}, {200, 400, 199}); }, 2u, OMNI_JOIN_TYPE_INNER,
        OMNI_BUILD_LEFT);
}

TEST(JoinSpillProactiveTest, VarcharKeyFieldInnerBuildLeftForcedSpillEqualsNoSpill)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    const DataTypes rowTypes(std::vector<DataTypePtr>({VarcharType(), LongType()}));
    ExpectFieldKeyForcedSpillEqualsNoSpill("proactive-varchar-bl", rowTypes,
        []() {
            return MakeVarcharKeyLongValueBatch({"alpha", "beta", "gamma", "delta", "zeta"},
                {10, 20, 30, 40, 50});
        },
        []() { return MakeVarcharKeyLongValueBatch({"alpha", "delta", "missing"}, {101, 104, 199}); },
        2u, OMNI_JOIN_TYPE_INNER, OMNI_BUILD_LEFT);
}

TEST(JoinSpillProactiveTest, CompositeTwoLongKeysInnerBuildLeftForcedSpillEqualsNoSpill)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    auto dir = join_spill_dual_run::ScopedTempDirectory("proactive-composite-bl");
    auto noSpill = MakeCompositeInnerHarness(dir.path().string(), false, false, OMNI_BUILD_LEFT);
    auto *build0 = dynamic_cast<HashBuilderOperator *>(noSpill.hashFactory->CreateOperator(&pool));
    auto *probe0 = dynamic_cast<LookupJoinOperator *>(noSpill.lookupFactory->CreateOperator(&pool));
    build0->AddInput(MakeTwoLongKeyBatch({1, 1, 2, 3}, {10, 11, 20, 30}, {100, 101, 200, 300}));
    probe0->AddInput(MakeTwoLongKeyBatch({1, 2, 99}, {11, 20, 1}, {1100, 2000, 199}));
    TupleMultiset expected;
    DrainJoinToCanonical(build0, probe0, &expected);
    DestroyInnerHarness(noSpill, build0, probe0);
    EXPECT_EQ(expected.size(), 2u);

    auto spill = MakeCompositeInnerHarness(dir.path().string(), true, true, OMNI_BUILD_LEFT);
    auto *build1 = dynamic_cast<HashBuilderOperator *>(spill.hashFactory->CreateOperator(&pool));
    auto *probe1 = dynamic_cast<LookupJoinOperator *>(spill.lookupFactory->CreateOperator(&pool));
    build1->AddInput(MakeTwoLongKeyBatch({1, 1, 2, 3}, {10, 11, 20, 30}, {100, 101, 200, 300}));
    probe1->AddInput(MakeTwoLongKeyBatch({1, 2, 99}, {11, 20, 1}, {1100, 2000, 199}));
    TupleMultiset actual;
    DrainJoinToCanonical(build1, probe1, &actual);
    DestroyInnerHarness(spill, build1, probe1);
    ExpectEqualTuples(expected, actual);
    EXPECT_GT(spill.state->BuildSpilledRows(), 0u);
}

TEST(JoinSpillProactiveTest, FieldKeyLongLeftOppositeForcedSpillEqualsNoSpill)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    const DataTypes rowTypes(std::vector<DataTypePtr>({LongType(), LongType()}));
    ExpectFieldKeyForcedSpillEqualsNoSpill("proactive-long-left-br", rowTypes,
        []() { return MakeLongBatch({1, 2, 3, 4, 5, 6, 7, 8}, {10, 20, 30, 40, 50, 60, 70, 80}); },
        []() { return MakeLongBatch({1, 3, 5, 8, 99}, {101, 103, 105, 108, 199}); }, 5u,
        OMNI_JOIN_TYPE_LEFT, OMNI_BUILD_RIGHT);
}

TEST(JoinSpillProactiveTest, IntFieldKeyLeftOppositeForcedSpillEqualsNoSpill)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    const DataTypes rowTypes(std::vector<DataTypePtr>({IntType(), LongType()}));
    ExpectFieldKeyForcedSpillEqualsNoSpill("proactive-int-left-br", rowTypes,
        []() { return MakeIntKeyLongValueBatch({1, 2, 3, 4, 5}, {10, 20, 30, 40, 50}); },
        []() { return MakeIntKeyLongValueBatch({2, 4, 99}, {200, 400, 199}); }, 3u, OMNI_JOIN_TYPE_LEFT,
        OMNI_BUILD_RIGHT);
}

TEST(JoinSpillProactiveTest, Date32FieldKeyLeftOppositeForcedSpillEqualsNoSpill)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    const DataTypes rowTypes(std::vector<DataTypePtr>({Date32Type(DAY), LongType()}));
    ExpectFieldKeyForcedSpillEqualsNoSpill("proactive-date32-left-br", rowTypes,
        []() { return MakeDate32Batch({10, 20, 30, 40}, {10, 20, 30, 40}); },
        []() { return MakeDate32Batch({20, 40, 99}, {200, 400, 199}); }, 3u, OMNI_JOIN_TYPE_LEFT,
        OMNI_BUILD_RIGHT);
}

TEST(JoinSpillProactiveTest, VarcharKeyFieldLeftOppositeForcedSpillEqualsNoSpill)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    const DataTypes rowTypes(std::vector<DataTypePtr>({VarcharType(), LongType()}));
    ExpectFieldKeyForcedSpillEqualsNoSpill("proactive-varchar-left-br", rowTypes,
        []() {
            return MakeVarcharKeyLongValueBatch({"alpha", "beta", "gamma", "delta", "zeta"},
                {10, 20, 30, 40, 50});
        },
        []() { return MakeVarcharKeyLongValueBatch({"alpha", "delta", "missing"}, {101, 104, 199}); },
        3u, OMNI_JOIN_TYPE_LEFT, OMNI_BUILD_RIGHT);
}

TEST(JoinSpillProactiveTest, FieldKeyLongRightOppositeForcedSpillEqualsNoSpill)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    const DataTypes rowTypes(std::vector<DataTypePtr>({LongType(), LongType()}));
    ExpectFieldKeyForcedSpillEqualsNoSpill("proactive-long-right-bl", rowTypes,
        []() { return MakeLongBatch({1, 2, 3, 4, 5, 6, 7, 8}, {10, 20, 30, 40, 50, 60, 70, 80}); },
        []() { return MakeLongBatch({1, 3, 5, 8, 99}, {101, 103, 105, 108, 199}); }, 5u,
        OMNI_JOIN_TYPE_RIGHT, OMNI_BUILD_LEFT);
}

TEST(JoinSpillProactiveTest, IntFieldKeyRightOppositeForcedSpillEqualsNoSpill)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    const DataTypes rowTypes(std::vector<DataTypePtr>({IntType(), LongType()}));
    ExpectFieldKeyForcedSpillEqualsNoSpill("proactive-int-right-bl", rowTypes,
        []() { return MakeIntKeyLongValueBatch({1, 2, 3, 4, 5}, {10, 20, 30, 40, 50}); },
        []() { return MakeIntKeyLongValueBatch({2, 4, 99}, {200, 400, 199}); }, 3u, OMNI_JOIN_TYPE_RIGHT,
        OMNI_BUILD_LEFT);
}

TEST(JoinSpillProactiveTest, Date32FieldKeyRightOppositeForcedSpillEqualsNoSpill)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    const DataTypes rowTypes(std::vector<DataTypePtr>({Date32Type(DAY), LongType()}));
    ExpectFieldKeyForcedSpillEqualsNoSpill("proactive-date32-right-bl", rowTypes,
        []() { return MakeDate32Batch({10, 20, 30, 40}, {10, 20, 30, 40}); },
        []() { return MakeDate32Batch({20, 40, 99}, {200, 400, 199}); }, 3u, OMNI_JOIN_TYPE_RIGHT,
        OMNI_BUILD_LEFT);
}

TEST(JoinSpillProactiveTest, VarcharKeyFieldRightOppositeForcedSpillEqualsNoSpill)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    const DataTypes rowTypes(std::vector<DataTypePtr>({VarcharType(), LongType()}));
    ExpectFieldKeyForcedSpillEqualsNoSpill("proactive-varchar-right-bl", rowTypes,
        []() {
            return MakeVarcharKeyLongValueBatch({"alpha", "beta", "gamma", "delta", "zeta"},
                {10, 20, 30, 40, 50});
        },
        []() { return MakeVarcharKeyLongValueBatch({"alpha", "delta", "missing"}, {101, 104, 199}); },
        3u, OMNI_JOIN_TYPE_RIGHT, OMNI_BUILD_LEFT);
}

TEST(JoinSpillProactiveTest, ExprIntPlusOneForcedSpillEqualsNoSpill)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    ExpectExprKeyForcedSpillEqualsNoSpill("proactive-expr-int", ExprKeyFlavor::IntPlusOne, 2u);
}

TEST(JoinSpillProactiveTest, ExprVarcharCastLongForcedSpillEqualsNoSpill)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    ExpectExprKeyForcedSpillEqualsNoSpill("proactive-expr-varchar", ExprKeyFlavor::VarcharCastLong, 2u);
}

TEST(JoinSpillProactiveTest, ExprDate32CastIntForcedSpillEqualsNoSpill)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    ExpectExprKeyForcedSpillEqualsNoSpill("proactive-expr-date32", ExprKeyFlavor::Date32PlusOne, 2u);
}

TEST(JoinSpillProactiveTest, ExprTransformKeyInnerBuildLeftForcedSpillEqualsNoSpill)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    ExpectExprKeyForcedSpillEqualsNoSpill("proactive-expr-bl", ExprKeyFlavor::LongPlusOne, 2u,
        OMNI_JOIN_TYPE_INNER, OMNI_BUILD_LEFT);
}

TEST(JoinSpillProactiveTest, ExprTransformKeyLeftOppositeForcedSpillEqualsNoSpill)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    ExpectExprKeyForcedSpillEqualsNoSpill("proactive-expr-left-br", ExprKeyFlavor::LongPlusOne, 3u,
        OMNI_JOIN_TYPE_LEFT, OMNI_BUILD_RIGHT);
}

TEST(JoinSpillProactiveTest, HeldFullTableFlushesOnNextProbeInput)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    auto cfg = TestSubPartitionConfig();
    const std::string spillPath = GenerateSpillPath();
    mkdir(spillPath.c_str(), 0750);

    DataTypes rowTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    int32_t hashCols[] = {0};
    auto *hashBuilderFactory =
        HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(OMNI_JOIN_TYPE_INNER, rowTypes, hashCols, 1, 1);
    auto state = std::make_shared<JoinSpillState>(spillPath, rowTypes, rowTypes, cfg.numSubPartitions);
    int32_t probeOutputCols[] = {0, 1};
    int32_t buildOutputCols[] = {0, 1};
    auto *lookupFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(rowTypes, probeOutputCols, 2,
        hashCols, 1, buildOutputCols, 2, rowTypes, reinterpret_cast<int64_t>(hashBuilderFactory), nullptr, false,
        nullptr);
    EnableJoinSpill(hashBuilderFactory, lookupFactory, state, cfg, MakeSpillConfig(spillPath), false);
    state->SetSpillJoinType(OMNI_JOIN_TYPE_INNER);

    auto *hashBuilder = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator(&pool));
    auto *lookupJoin = dynamic_cast<LookupJoinOperator *>(lookupFactory->CreateOperator(&pool));
    hashBuilder->AddInput(MakeLongBatch({1, 2, 3, 4}, {10, 20, 30, 40}));
    hashBuilder->noMoreInput();
    VectorBatch *buildOut = nullptr;
    hashBuilder->GetOutput(&buildOut);
    EXPECT_EQ(state->GetHandoffPhase(), JoinHandoffPhase::RestoreRound);
    EXPECT_EQ(state->BuildSpilledFiles(), 0U);

    lookupJoin->AddInput(MakeLongBatch({2, 4}, {200, 400}));
    EXPECT_GT(lookupJoin->reclaimableBytes(), 0U);
    lookupJoin->reclaim(std::numeric_limits<int64_t>::max());
    EXPECT_GT(state->BuildSpilledFiles(), 0U);
    EXPECT_EQ(state->TableInHand().kind, JoinSpillTableInHandKind::None);

    const int32_t outputRows = PumpJoin(hashBuilder, lookupJoin);
    EXPECT_EQ(outputRows, 2);

    op::Operator::DeleteOperator(lookupJoin);
    op::Operator::DeleteOperator(hashBuilder);
    delete lookupFactory;
    delete hashBuilderFactory;
}

TEST(JoinSpillProactiveTest, HeldRestorePartitionFlushesOnce)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    auto cfg = TestSubPartitionConfig();
    const std::string spillPath = GenerateSpillPath();
    mkdir(spillPath.c_str(), 0750);

    DataTypes rowTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    int32_t hashCols[] = {0};
    auto *hashBuilderFactory =
        HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(OMNI_JOIN_TYPE_INNER, rowTypes, hashCols, 1, 1);
    auto state = std::make_shared<JoinSpillState>(spillPath, rowTypes, rowTypes, cfg.numSubPartitions);
    int32_t probeOutputCols[] = {0, 1};
    int32_t buildOutputCols[] = {0, 1};
    auto *lookupFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(rowTypes, probeOutputCols, 2,
        hashCols, 1, buildOutputCols, 2, rowTypes, reinterpret_cast<int64_t>(hashBuilderFactory), nullptr, false,
        nullptr);
    EnableJoinSpill(hashBuilderFactory, lookupFactory, state, cfg, MakeSpillConfig(spillPath), true);
    state->SetSpillJoinType(OMNI_JOIN_TYPE_INNER);

    auto *hashBuilder = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator(&pool));
    auto *lookupJoin = dynamic_cast<LookupJoinOperator *>(lookupFactory->CreateOperator(&pool));
    hashBuilder->AddInput(MakeLongBatch({1, 2, 3, 4}, {10, 20, 30, 40}));
    // One batch before handoff: LookupJoin keeps a single curInputBatch; a second AddInput would
    // overwrite the first and drop probe rows before Path A mirrors leftover input.
    lookupJoin->AddInput(MakeLongBatch({2, 4}, {200, 400}));
    const int32_t outputRows = PumpJoin(hashBuilder, lookupJoin);
    EXPECT_EQ(outputRows, 2);

    op::Operator::DeleteOperator(lookupJoin);
    op::Operator::DeleteOperator(hashBuilder);
    delete lookupFactory;
    delete hashBuilderFactory;
}

TEST(JoinSpillProactiveTest, HeldRestorePartitionDebouncesSameBucket)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    auto cfg = TestSubPartitionConfig();
    const std::string spillPath = GenerateSpillPath();
    mkdir(spillPath.c_str(), 0750);

    DataTypes rowTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    int32_t hashCols[] = {0};
    auto *hashBuilderFactory =
        HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(OMNI_JOIN_TYPE_INNER, rowTypes, hashCols, 1, 1);
    auto state = std::make_shared<JoinSpillState>(spillPath, rowTypes, rowTypes, cfg.numSubPartitions);
    int32_t probeOutputCols[] = {0, 1};
    int32_t buildOutputCols[] = {0, 1};
    auto *lookupFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(rowTypes, probeOutputCols, 2,
        hashCols, 1, buildOutputCols, 2, rowTypes, reinterpret_cast<int64_t>(hashBuilderFactory), nullptr, false,
        nullptr);
    EnableJoinSpill(hashBuilderFactory, lookupFactory, state, cfg, MakeSpillConfig(spillPath), true);
    state->SetSpillJoinType(OMNI_JOIN_TYPE_INNER);

    auto *hashBuilder = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator(&pool));
    auto *lookupJoin = dynamic_cast<LookupJoinOperator *>(lookupFactory->CreateOperator(&pool));
    hashBuilder->AddInput(MakeLongBatch({1, 2, 3, 4}, {10, 20, 30, 40}));
    hashBuilder->noMoreInput();
    VectorBatch *buildOut = nullptr;
    hashBuilder->GetOutput(&buildOut);
    // Two Path A probe files per bucket so the restore checkpoint runs after Find,
    // not on the just-restored table.
    lookupJoin->AddInput(MakeLongBatch({2, 4}, {200, 400}));
    lookupJoin->AddInput(MakeLongBatch({2, 4}, {201, 401}));
    EXPECT_EQ(PumpJoin(hashBuilder, lookupJoin, true), 4);

    int32_t flushedBuckets = 0;
    for (uint32_t partition = 0; partition < cfg.numSubPartitions; ++partition) {
        if (!state->WasRestorePartitionFlushed(partition)) {
            continue;
        }
        ++flushedBuckets;
        // Path A writes the first build file; the held-table flush writes exactly one more.
        // A second reclaim request on the same restore bucket must not add a third file.
        EXPECT_EQ(CountBuildSpillDirsForPartition(LatestJoinSpillSessionDir(spillPath), partition), 2)
            << "restore partition " << partition << " flushed more than once";
    }
    EXPECT_GE(flushedBuckets, 2);
    EXPECT_FALSE(state->CanFlushHeldTable());

    op::Operator::DeleteOperator(lookupJoin);
    op::Operator::DeleteOperator(hashBuilder);
    delete lookupFactory;
    delete hashBuilderFactory;
}

namespace {

struct TwoColJoinHarness {
    std::shared_ptr<JoinSpillState> state;
    HashBuilderOperatorFactory *hashFactory = nullptr;
    LookupJoinOperatorFactory *lookupFactory = nullptr;
};

TwoColJoinHarness MakeTwoColJoin(const std::string &spillDir, JoinType joinType, BuildSide buildSide,
    bool spillSession, bool forceSpill, bool compoundKey = false)
{
    TwoColJoinHarness h;
    DataTypes rowTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    int32_t hashCols[] = {0, 1};
    const int32_t keyCount = compoundKey ? 2 : 1;
    h.hashFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(joinType, buildSide, rowTypes,
        hashCols, keyCount, 1);
    int32_t probeOutputCols[] = {0, 1};
    int32_t buildOutputCols[] = {0, 1};
    h.lookupFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(rowTypes, probeOutputCols, 2,
        hashCols, keyCount, buildOutputCols, 2, rowTypes, reinterpret_cast<int64_t>(h.hashFactory), nullptr, false,
        nullptr);
    if (spillSession) {
        auto cfg = TestSubPartitionConfig();
        h.state = std::make_shared<JoinSpillState>(spillDir, rowTypes, rowTypes, cfg.numSubPartitions);
        EnableJoinSpill(h.hashFactory, h.lookupFactory, h.state, cfg, MakeSpillConfig(spillDir), forceSpill);
        h.state->SetSpillJoinType(joinType, buildSide);
    }
    return h;
}

void DestroyTwoColJoin(const TwoColJoinHarness &h, op::Operator *build, op::Operator *probe)
{
    op::Operator::DeleteOperator(probe);
    op::Operator::DeleteOperator(build);
    delete h.lookupFactory;
    delete h.hashFactory;
}

void ExpectHeldFlushEqualsNoSpill(JoinType joinType, BuildSide buildSide, const char *dirPrefix)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    auto dir = join_spill_dual_run::ScopedTempDirectory(dirPrefix);
    const auto buildKeys = std::vector<int64_t>({1, 2, 3, 4});
    const auto buildVals = std::vector<int64_t>({10, 20, 30, 40});
    const auto probeKeys = std::vector<int64_t>({2, 4, 99});
    const auto probeVals = std::vector<int64_t>({200, 400, 990});

    auto noSpill = MakeTwoColJoin(dir.path().string(), joinType, buildSide, true, false);
    auto *build0 = dynamic_cast<HashBuilderOperator *>(noSpill.hashFactory->CreateOperator(&pool));
    auto *probe0 = dynamic_cast<LookupJoinOperator *>(noSpill.lookupFactory->CreateOperator(&pool));
    build0->AddInput(MakeLongBatch(buildKeys, buildVals));
    auto repeatedProbeKeys = probeKeys;
    repeatedProbeKeys.insert(repeatedProbeKeys.end(), probeKeys.begin(), probeKeys.end());
    auto repeatedProbeVals = probeVals;
    repeatedProbeVals.insert(repeatedProbeVals.end(), probeVals.begin(), probeVals.end());
    probe0->AddInput(MakeLongBatch(repeatedProbeKeys, repeatedProbeVals));
    TupleMultiset expected;
    FullTableUnvisitedDrain unvisitedDrain{noSpill.hashFactory, buildSide};
    if (JoinNeedsUnvisitedBuildOutput(joinType, buildSide)) {
        DrainJoinToCanonical(build0, probe0, &expected, &unvisitedDrain);
    } else {
        DrainJoinToCanonical(build0, probe0, &expected);
    }
    DestroyTwoColJoin(noSpill, build0, probe0);
    EXPECT_FALSE(expected.empty());

    auto spill = MakeTwoColJoin(dir.path().string(), joinType, buildSide, true, true);
    auto *build1 = dynamic_cast<HashBuilderOperator *>(spill.hashFactory->CreateOperator(&pool));
    auto *probe1 = dynamic_cast<LookupJoinOperator *>(spill.lookupFactory->CreateOperator(&pool));
    build1->AddInput(MakeLongBatch(buildKeys, buildVals));
    build1->noMoreInput();
    VectorBatch *buildOutput = nullptr;
    build1->GetOutput(&buildOutput);
    ASSERT_EQ(buildOutput, nullptr);
    // Two files leave a safe reclaim boundary after the first restored batch.
    probe1->AddInput(MakeLongBatch(probeKeys, probeVals));
    probe1->AddInput(MakeLongBatch(probeKeys, probeVals));
    TupleMultiset actual;
    DrainJoinToCanonical(build1, probe1, &actual, nullptr, true);
    bool restoredTableFlushed = false;
    for (uint32_t partition = 0; partition < TestSubPartitionConfig().numSubPartitions; ++partition) {
        restoredTableFlushed |= spill.state->WasRestorePartitionFlushed(partition);
    }
    EXPECT_TRUE(restoredTableFlushed);
    EXPECT_GT(spill.state->BuildSpilledRows(), 0U);
    DestroyTwoColJoin(spill, build1, probe1);

    ExpectEqualTuples(expected, actual);
}

void ExpectHeldFullTableFlushEqualsNoSpill(JoinType joinType, BuildSide buildSide, const char *dirPrefix)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    auto dir = join_spill_dual_run::ScopedTempDirectory(dirPrefix);
    const int64_t matched = FindKeyForSubPartition(1);
    const int64_t unmatched = FindSecondKeyForSubPartition(1, matched);
    const int64_t miss = FindKeyForSubPartition(2);
    ASSERT_GE(matched, 0);
    ASSERT_GE(unmatched, 0);
    ASSERT_GE(miss, 0);

    auto noSpill = MakeTwoColJoin(dir.path().string(), joinType, buildSide, true, false);
    auto *build0 = dynamic_cast<HashBuilderOperator *>(noSpill.hashFactory->CreateOperator(&pool));
    auto *probe0 = dynamic_cast<LookupJoinOperator *>(noSpill.lookupFactory->CreateOperator(&pool));
    build0->AddInput(MakeLongBatch({matched, unmatched}, {10, 19}));
    probe0->AddInput(MakeLongBatch({matched, miss}, {200, 990}));
    TupleMultiset expected;
    FullTableUnvisitedDrain unvisitedDrain{noSpill.hashFactory, buildSide};
    DrainJoinToCanonical(build0, probe0, &expected, &unvisitedDrain);
    DestroyTwoColJoin(noSpill, build0, probe0);
    EXPECT_FALSE(expected.empty());

    auto spill = MakeTwoColJoin(dir.path().string(), joinType, buildSide, true, false);
    auto *build1 = dynamic_cast<HashBuilderOperator *>(spill.hashFactory->CreateOperator(&pool));
    auto *probe1 = dynamic_cast<LookupJoinOperator *>(spill.lookupFactory->CreateOperator(&pool));
    TupleMultiset actual;
    build1->AddInput(MakeLongBatch({matched, unmatched}, {10, 19}));
    build1->noMoreInput();
    VectorBatch *buildOut = nullptr;
    build1->GetOutput(&buildOut);
    EXPECT_EQ(spill.state->GetHandoffPhase(), JoinHandoffPhase::RestoreRound);
    EXPECT_EQ(spill.state->BuildSpilledFiles(), 0U);

    // Reclaim after accepting the first probe batch, before any of its rows are processed.
    // Separate FULL / same-side LEFT tests cover reclaim after partial probing.
    probe1->AddInput(MakeLongBatch({matched, miss}, {200, 990}));
    EXPECT_GT(probe1->reclaimableBytes(), 0U);
    probe1->reclaim(std::numeric_limits<int64_t>::max());
    EXPECT_GT(spill.state->BuildSpilledFiles(), 0U);

    DrainJoinToCanonical(build1, probe1, &actual);
    EXPECT_GT(probe1->ProbeSpilledRows(), 0U);
    DestroyTwoColJoin(spill, build1, probe1);

    ExpectEqualTuples(expected, actual);
    EXPECT_EQ(pool.currentBytes(), 0);
}

} // namespace

// Path B (held restore flush): one nospill vs spill parity per supported join×build (8 cells).
TEST(JoinSpillProactiveTest, InnerBuildRightHeldRestoreFlushEqualsNoSpill)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    ExpectHeldFlushEqualsNoSpill(OMNI_JOIN_TYPE_INNER, OMNI_BUILD_RIGHT, "held-inner-br-restore");
}

TEST(JoinSpillProactiveTest, InnerBuildLeftHeldRestoreFlushEqualsNoSpill)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    ExpectHeldFlushEqualsNoSpill(OMNI_JOIN_TYPE_INNER, OMNI_BUILD_LEFT, "held-inner-bl-restore");
}

TEST(JoinSpillProactiveTest, LeftBuildRightHeldRestoreFlushEqualsNoSpill)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    ExpectHeldFlushEqualsNoSpill(OMNI_JOIN_TYPE_LEFT, OMNI_BUILD_RIGHT, "held-left-br-restore");
}

TEST(JoinSpillProactiveTest, LeftBuildLeftHeldRestoreFlushEqualsNoSpill)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    ExpectHeldFlushEqualsNoSpill(OMNI_JOIN_TYPE_LEFT, OMNI_BUILD_LEFT, "held-left-bl-restore");
}

TEST(JoinSpillProactiveTest, RightBuildLeftHeldRestoreFlushEqualsNoSpill)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    ExpectHeldFlushEqualsNoSpill(OMNI_JOIN_TYPE_RIGHT, OMNI_BUILD_LEFT, "held-right-bl-restore");
}

TEST(JoinSpillProactiveTest, RightBuildRightHeldRestoreFlushEqualsNoSpill)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    ExpectHeldFlushEqualsNoSpill(OMNI_JOIN_TYPE_RIGHT, OMNI_BUILD_RIGHT, "held-right-br-restore");
}

TEST(JoinSpillProactiveTest, FullBuildRightHeldRestoreFlushEqualsNoSpill)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    ExpectHeldFlushEqualsNoSpill(OMNI_JOIN_TYPE_FULL, OMNI_BUILD_RIGHT, "held-full-br-restore");
}

TEST(JoinSpillProactiveTest, FullBuildLeftHeldRestoreFlushEqualsNoSpill)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    ExpectHeldFlushEqualsNoSpill(OMNI_JOIN_TYPE_FULL, OMNI_BUILD_LEFT, "held-full-bl-restore");
}

// Pin matched + unmatched build in the same restore bucket (JoinSpillRestoreTest style).
// Scattered keys like {1,2,3,4} land in different partitions; a raw total of 5/7 then
// mixes buckets and does not prove unvisited emit after a held flush.
// Run before opposite-side held full-table dual-run tests: those leave process-global
// spill accounting that changes forceSpill=0 restore behavior in this scenario.
TEST(JoinSpillProactiveTest, FullHeldRestoreFlushEmitsUnmatchedBuild)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    auto cfg = TestSubPartitionConfig();
    const std::string spillPath = GenerateSpillPath();
    mkdir(spillPath.c_str(), 0750);

    const int64_t matched = FindKeyForSubPartition(1);
    const int64_t unmatched = FindSecondKeyForSubPartition(1, matched);
    const int64_t miss = FindKeyForSubPartition(2);
    ASSERT_GE(matched, 0);
    ASSERT_GE(unmatched, 0);
    ASSERT_GE(miss, 0);

    DataTypes rowTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    int32_t hashCols[] = {0};
    auto *hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
        OMNI_JOIN_TYPE_FULL, OMNI_BUILD_RIGHT, rowTypes, hashCols, 1, 1);
    auto state = std::make_shared<JoinSpillState>(spillPath, rowTypes, rowTypes, cfg.numSubPartitions);
    int32_t probeOutputCols[] = {0, 1};
    int32_t buildOutputCols[] = {0, 1};
    auto *lookupFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(rowTypes, probeOutputCols, 2,
        hashCols, 1, buildOutputCols, 2, rowTypes, reinterpret_cast<int64_t>(hashBuilderFactory), nullptr, false,
        nullptr);
    EnableJoinSpill(hashBuilderFactory, lookupFactory, state, cfg, MakeSpillConfig(spillPath), true);
    state->SetSpillJoinType(OMNI_JOIN_TYPE_FULL, OMNI_BUILD_RIGHT);

    auto *hashBuilder = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator(&pool));
    auto *lookupJoin = dynamic_cast<LookupJoinOperator *>(lookupFactory->CreateOperator(&pool));
    hashBuilder->AddInput(MakeLongBatch({matched, unmatched}, {10, 19}));
    lookupJoin->AddInput(MakeLongBatch({matched, miss}, {200, 990}));
    const auto counts = PumpFullJoinKinds(hashBuilder, lookupJoin);
    EXPECT_EQ(counts.paired, 1);
    EXPECT_EQ(counts.unmatchedProbe, 1);
    EXPECT_EQ(counts.unmatchedBuild, 1);

    op::Operator::DeleteOperator(lookupJoin);
    op::Operator::DeleteOperator(hashBuilder);
    delete lookupFactory;
    delete hashBuilderFactory;
    EXPECT_EQ(pool.currentBytes(), 0);
}

// Two Path A probe files for the matched bucket so the restore checkpoint can flush
// after the first Find. Re-restore must still emit the same-bucket unmatched build.
TEST(JoinSpillProactiveTest, FullHeldRestoreFlushEmitsUnmatchedBuildAfterTwoProbeFiles)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    auto cfg = TestSubPartitionConfig();
    const std::string spillPath = GenerateSpillPath();
    mkdir(spillPath.c_str(), 0750);

    const int64_t matched = FindKeyForSubPartition(1);
    const int64_t unmatched = FindSecondKeyForSubPartition(1, matched);
    const int64_t miss = FindKeyForSubPartition(2);
    ASSERT_GE(matched, 0);
    ASSERT_GE(unmatched, 0);
    ASSERT_GE(miss, 0);

    DataTypes rowTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    int32_t hashCols[] = {0};
    auto *hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
        OMNI_JOIN_TYPE_FULL, OMNI_BUILD_RIGHT, rowTypes, hashCols, 1, 1);
    auto state = std::make_shared<JoinSpillState>(spillPath, rowTypes, rowTypes, cfg.numSubPartitions);
    int32_t probeOutputCols[] = {0, 1};
    int32_t buildOutputCols[] = {0, 1};
    auto *lookupFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(rowTypes, probeOutputCols, 2,
        hashCols, 1, buildOutputCols, 2, rowTypes, reinterpret_cast<int64_t>(hashBuilderFactory), nullptr, false,
        nullptr);
    EnableJoinSpill(hashBuilderFactory, lookupFactory, state, cfg, MakeSpillConfig(spillPath), true);
    state->SetSpillJoinType(OMNI_JOIN_TYPE_FULL, OMNI_BUILD_RIGHT);

    auto *hashBuilder = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator(&pool));
    auto *lookupJoin = dynamic_cast<LookupJoinOperator *>(lookupFactory->CreateOperator(&pool));
    hashBuilder->AddInput(MakeLongBatch({matched, unmatched}, {10, 19}));
    hashBuilder->noMoreInput();
    VectorBatch *buildOut = nullptr;
    hashBuilder->GetOutput(&buildOut);
    lookupJoin->AddInput(MakeLongBatch({matched}, {200}));
    lookupJoin->AddInput(MakeLongBatch({matched, miss}, {201, 990}));
    const auto counts = PumpFullJoinKinds(hashBuilder, lookupJoin, true);
    EXPECT_EQ(counts.paired, 2);
    EXPECT_EQ(counts.unmatchedProbe, 1);
    EXPECT_EQ(counts.unmatchedBuild, 1);
    EXPECT_TRUE(state->WasRestorePartitionFlushed(1));

    op::Operator::DeleteOperator(lookupJoin);
    op::Operator::DeleteOperator(hashBuilder);
    delete lookupFactory;
    delete hashBuilderFactory;
    EXPECT_EQ(pool.currentBytes(), 0);
}

TEST(JoinSpillProactiveTest, InnerBuildRightHeldFullTableFlushEqualsNoSpill)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    ExpectHeldFullTableFlushEqualsNoSpill(OMNI_JOIN_TYPE_INNER, OMNI_BUILD_RIGHT, "held-inner-br-full");
}

TEST(JoinSpillProactiveTest, InnerBuildLeftHeldFullTableFlushEqualsNoSpill)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    ExpectHeldFullTableFlushEqualsNoSpill(OMNI_JOIN_TYPE_INNER, OMNI_BUILD_LEFT, "held-inner-bl-full");
}

TEST(JoinSpillProactiveTest, LeftBuildRightHeldFullTableFlushEqualsNoSpill)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    ExpectHeldFullTableFlushEqualsNoSpill(OMNI_JOIN_TYPE_LEFT, OMNI_BUILD_RIGHT, "held-left-br-full");
}

TEST(JoinSpillProactiveTest, LeftBuildLeftHeldFullTableFlushEqualsNoSpill)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    ExpectHeldFullTableFlushEqualsNoSpill(OMNI_JOIN_TYPE_LEFT, OMNI_BUILD_LEFT, "held-left-bl-full");
}

TEST(JoinSpillProactiveTest, RightBuildLeftHeldFullTableFlushEqualsNoSpill)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    ExpectHeldFullTableFlushEqualsNoSpill(OMNI_JOIN_TYPE_RIGHT, OMNI_BUILD_LEFT, "held-right-bl-full");
}

TEST(JoinSpillProactiveTest, RightBuildRightHeldFullTableFlushEqualsNoSpill)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    ExpectHeldFullTableFlushEqualsNoSpill(OMNI_JOIN_TYPE_RIGHT, OMNI_BUILD_RIGHT, "held-right-br-full");
}

TEST(JoinSpillProactiveTest, FullBuildRightHeldFullTableFlushEqualsNoSpill)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    ExpectHeldFullTableFlushEqualsNoSpill(OMNI_JOIN_TYPE_FULL, OMNI_BUILD_RIGHT, "held-full-br-full");
}

TEST(JoinSpillProactiveTest, FullBuildLeftHeldFullTableFlushEqualsNoSpill)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    ExpectHeldFullTableFlushEqualsNoSpill(OMNI_JOIN_TYPE_FULL, OMNI_BUILD_LEFT, "held-full-bl-full");
}

// Full-table handoff, Find one matched row, then reclaim after the next AddInput.
// Restore must not re-emit the matched build
// row (visited flag was written on flush).
TEST(JoinSpillProactiveTest, FullHeldFullTableAfterPartialProbeEmitsUnmatchedOnce)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    auto cfg = TestSubPartitionConfig();
    const std::string spillPath = GenerateSpillPath();
    mkdir(spillPath.c_str(), 0750);

    const int64_t matched = FindKeyForSubPartition(1);
    const int64_t unmatched = FindSecondKeyForSubPartition(1, matched);
    const int64_t miss = FindKeyForSubPartition(2);
    ASSERT_GE(matched, 0);
    ASSERT_GE(unmatched, 0);
    ASSERT_GE(miss, 0);

    DataTypes rowTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    int32_t hashCols[] = {0};
    auto *hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
        OMNI_JOIN_TYPE_FULL, OMNI_BUILD_RIGHT, rowTypes, hashCols, 1, 1);
    auto state = std::make_shared<JoinSpillState>(spillPath, rowTypes, rowTypes, cfg.numSubPartitions);
    int32_t probeOutputCols[] = {0, 1};
    int32_t buildOutputCols[] = {0, 1};
    auto *lookupFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(rowTypes, probeOutputCols, 2,
        hashCols, 1, buildOutputCols, 2, rowTypes, reinterpret_cast<int64_t>(hashBuilderFactory), nullptr, false,
        nullptr);
    EnableJoinSpill(hashBuilderFactory, lookupFactory, state, cfg, MakeSpillConfig(spillPath), false);
    state->SetSpillJoinType(OMNI_JOIN_TYPE_FULL, OMNI_BUILD_RIGHT);

    auto *hashBuilder = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator(&pool));
    auto *lookupJoin = dynamic_cast<LookupJoinOperator *>(lookupFactory->CreateOperator(&pool));
    hashBuilder->AddInput(MakeLongBatch({matched, unmatched}, {10, 19}));
    hashBuilder->noMoreInput();
    VectorBatch *buildOut = nullptr;
    hashBuilder->GetOutput(&buildOut);
    EXPECT_EQ(state->BuildSpilledFiles(), 0U);

    lookupJoin->AddInput(MakeLongBatch({matched}, {200}));
    FullJoinRowKinds counts;
    for (int32_t i = 0; i < 1000 && !lookupJoin->needsInput(); ++i) {
        VectorBatch *ignored = nullptr;
        hashBuilder->GetOutput(&ignored);
        VectorBatch *probeOut = nullptr;
        lookupJoin->GetOutput(&probeOut);
        if (probeOut != nullptr) {
            CountFullBuildRightRows(probeOut, &counts);
            VectorHelper::FreeVecBatch(probeOut);
        }
    }
    EXPECT_EQ(counts.paired, 1);
    EXPECT_EQ(counts.unmatchedBuild, 0);

    lookupJoin->AddInput(MakeLongBatch({miss}, {990}));
    EXPECT_GT(lookupJoin->reclaimableBytes(), 0U);
    lookupJoin->reclaim(std::numeric_limits<int64_t>::max());
    EXPECT_GT(state->BuildSpilledFiles(), 0U);

    const auto rest = PumpFullJoinKinds(hashBuilder, lookupJoin);
    counts.paired += rest.paired;
    counts.unmatchedProbe += rest.unmatchedProbe;
    counts.unmatchedBuild += rest.unmatchedBuild;
    EXPECT_EQ(counts.paired, 1);
    EXPECT_EQ(counts.unmatchedProbe, 1);
    EXPECT_EQ(counts.unmatchedBuild, 1);

    op::Operator::DeleteOperator(lookupJoin);
    op::Operator::DeleteOperator(hashBuilder);
    delete lookupFactory;
    delete hashBuilderFactory;
}

TEST(JoinSpillProactiveTest, LeftSameSideHeldFullTableAfterPartialProbeEmitsUnmatchedOnce)
{
    omniruntime::mem::OmniTaskMemoryPool pool{nullptr};
    auto cfg = TestSubPartitionConfig();
    const std::string spillPath = GenerateSpillPath();
    mkdir(spillPath.c_str(), 0750);

    const int64_t matched = FindKeyForSubPartition(1);
    const int64_t unmatched = FindSecondKeyForSubPartition(1, matched);
    const int64_t miss = FindKeyForSubPartition(2);
    ASSERT_GE(matched, 0);
    ASSERT_GE(unmatched, 0);
    ASSERT_GE(miss, 0);

    DataTypes rowTypes(std::vector<DataTypePtr>({ LongType(), LongType() }));
    int32_t hashCols[] = {0};
    auto *hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
        OMNI_JOIN_TYPE_LEFT, OMNI_BUILD_LEFT, rowTypes, hashCols, 1, 1);
    auto state = std::make_shared<JoinSpillState>(spillPath, rowTypes, rowTypes, cfg.numSubPartitions);
    int32_t probeOutputCols[] = {0, 1};
    int32_t buildOutputCols[] = {0, 1};
    auto *lookupFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(rowTypes, probeOutputCols, 2,
        hashCols, 1, buildOutputCols, 2, rowTypes, reinterpret_cast<int64_t>(hashBuilderFactory), nullptr, false,
        nullptr);
    EnableJoinSpill(hashBuilderFactory, lookupFactory, state, cfg, MakeSpillConfig(spillPath), false);
    state->SetSpillJoinType(OMNI_JOIN_TYPE_LEFT, OMNI_BUILD_LEFT);

    auto *hashBuilder = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator(&pool));
    auto *lookupJoin = dynamic_cast<LookupJoinOperator *>(lookupFactory->CreateOperator(&pool));
    hashBuilder->AddInput(MakeLongBatch({matched, unmatched}, {10, 19}));
    hashBuilder->noMoreInput();
    VectorBatch *buildOut = nullptr;
    hashBuilder->GetOutput(&buildOut);
    EXPECT_EQ(state->BuildSpilledFiles(), 0U);

    lookupJoin->AddInput(MakeLongBatch({matched}, {200}));
    FullJoinRowKinds counts;
    for (int32_t i = 0; i < 1000 && !lookupJoin->needsInput(); ++i) {
        VectorBatch *ignored = nullptr;
        hashBuilder->GetOutput(&ignored);
        VectorBatch *probeOut = nullptr;
        lookupJoin->GetOutput(&probeOut);
        if (probeOut != nullptr) {
            CountSameSideBuildLeftRows(probeOut, &counts);
            VectorHelper::FreeVecBatch(probeOut);
        }
    }
    EXPECT_EQ(counts.paired, 1);
    EXPECT_EQ(counts.unmatchedBuild, 0);

    lookupJoin->AddInput(MakeLongBatch({miss}, {990}));
    EXPECT_GT(lookupJoin->reclaimableBytes(), 0U);
    lookupJoin->reclaim(std::numeric_limits<int64_t>::max());
    EXPECT_GT(state->BuildSpilledFiles(), 0U);

    hashBuilder->noMoreInput();
    lookupJoin->noMoreInput();
    for (int32_t i = 0; i < 10000; ++i) {
        if (hashBuilder->GetStatus() == OMNI_STATUS_FINISHED && lookupJoin->GetStatus() == OMNI_STATUS_FINISHED) {
            break;
        }
        VectorBatch *ignored = nullptr;
        hashBuilder->GetOutput(&ignored);
        VectorBatch *probeOut = nullptr;
        lookupJoin->GetOutput(&probeOut);
        if (probeOut != nullptr) {
            CountSameSideBuildLeftRows(probeOut, &counts);
            VectorHelper::FreeVecBatch(probeOut);
        }
    }
    EXPECT_EQ(counts.paired, 1);
    EXPECT_EQ(counts.unmatchedProbe, 0);
    EXPECT_EQ(counts.unmatchedBuild, 1);

    op::Operator::DeleteOperator(lookupJoin);
    op::Operator::DeleteOperator(hashBuilder);
    delete lookupFactory;
    delete hashBuilderFactory;
}

TEST(JoinSpillProactiveTest, SameSideCompoundNullKeysSpillEqualsNoSpill)
{
    for (const auto side : {OMNI_BUILD_LEFT, OMNI_BUILD_RIGHT}) {
        omniruntime::mem::OmniTaskMemoryPool pool(nullptr);
        auto dir = join_spill_dual_run::ScopedTempDirectory("compound-null-outer");
        const auto join = side == OMNI_BUILD_LEFT ? OMNI_JOIN_TYPE_LEFT : OMNI_JOIN_TYPE_RIGHT;
        TupleMultiset expected;
        for (bool spill : {false, true}) {
            auto h = MakeTwoColJoin(dir.path().string(), join, side, true, spill, true);
            auto *build = h.hashFactory->CreateOperator(&pool);
            auto *probe = h.lookupFactory->CreateOperator(&pool);
            auto *input = MakeLongBatch({1, 2, 3}, {10, 20, 30});
            input->Get(0)->SetNull(0);
            input->Get(1)->SetNull(1);
            build->AddInput(input);
            probe->AddInput(MakeLongBatch({3}, {30}));
            TupleMultiset actual;
            FullTableUnvisitedDrain unvisited{h.hashFactory, side};
            DrainJoinToCanonical(build, probe, &actual, spill ? nullptr : &unvisited);
            EXPECT_EQ(actual.size(), 3U);
            if (spill) {
                EXPECT_GT(h.state->BuildSpilledRows(), 0U);
                ExpectEqualTuples(expected, actual);
            } else {
                EXPECT_EQ(h.state->BuildSpilledRows(), 0U);
                expected = actual;
            }
            DestroyTwoColJoin(h, build, probe);
        }
        EXPECT_EQ(pool.currentBytes(), 0);
    }
}
