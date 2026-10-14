/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 * Description: join spill v1 tests, retargeted onto proactive spill (memory threshold, one hash-table slot).
 */
#include <climits>
#include <sys/stat.h>

#include <memory>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "operator/config/operator_config.h"
#include "operator/join/hash_builder.h"
#include "operator/join/join_spill_state.h"
#include "operator/join/lookup_join.h"
#include "type/data_type.h"
#include "type/data_types.h"
#include "util/test_util.h"
#include "vector/large_string_container.h"
#include "vector/vector.h"
#include "vector/vector_batch.h"
#include "vector/vector_helper.h"

namespace op = omniruntime::op;
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::vec;

namespace {

using VarcharVector = Vector<LargeStringContainer<std::string_view>>;

JoinSubPartitionConfig TestSubPartitionConfig()
{
    JoinSubPartitionConfig cfg;
    cfg.numSubPartitions = 4;
    cfg.startPartitionBit = 0;
    cfg.activeSubPartition = 0;
    return cfg;
}

std::shared_ptr<SparkSpillConfig> MakeSpillConfig(const std::string &spillPath)
{
    return std::make_shared<SparkSpillConfig>(true, spillPath, 100UL << 30, INT32_MAX, 100, 0, false);
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

VectorBatch *MakeVarcharBatch(const std::vector<std::string> &keys, const std::vector<std::string> &values)
{
    auto *batch = new VectorBatch(static_cast<int32_t>(keys.size()));
    auto *keyVec = new VarcharVector(static_cast<int32_t>(keys.size()));
    auto *valueVec = new VarcharVector(static_cast<int32_t>(values.size()));
    for (size_t i = 0; i < keys.size(); ++i) {
        keyVec->SetValue(static_cast<int32_t>(i), std::string_view(keys[i]));
        valueVec->SetValue(static_cast<int32_t>(i), std::string_view(values[i]));
    }
    batch->Append(keyVec);
    batch->Append(valueVec);
    return batch;
}

void EnableJoinSpill(HashBuilderOperatorFactory *hashBuilderFactory, LookupJoinOperatorFactory *lookupFactory,
    const std::shared_ptr<JoinSpillState> &state, const JoinSubPartitionConfig &cfg,
    const std::shared_ptr<SparkSpillConfig> &spillConfig, bool forceSpill)
{
    hashBuilderFactory->SetJoinSpillPolicy(true, cfg);
    hashBuilderFactory->SetJoinSpillState(state);
    hashBuilderFactory->SetSpillConfig(spillConfig);
    hashBuilderFactory->SetJoinSpillMemThresholdForTest(forceSpill ? 0 : LLONG_MAX);
    lookupFactory->SetJoinSpillPolicy(true, cfg);
    lookupFactory->SetJoinSpillState(state);
}

int32_t PumpJoin(op::Operator *builder, op::Operator *probe)
{
    int32_t outputRows = 0;
    builder->noMoreInput();
    probe->noMoreInput();
    for (int32_t i = 0; i < 10000; ++i) {
        if (builder->GetStatus() == OMNI_STATUS_FINISHED && probe->GetStatus() == OMNI_STATUS_FINISHED) {
            break;
        }
        VectorBatch *buildOut = nullptr;
        builder->GetOutput(&buildOut);
        VectorBatch *probeOut = nullptr;
        probe->GetOutput(&probeOut);
        if (probeOut != nullptr) {
            outputRows += probeOut->GetRowCount();
            VectorHelper::FreeVecBatch(probeOut);
        }
    }
    return outputRows;
}

} // namespace

TEST(JoinSpillTest, InnerJoinLongKeyValueBuildAndProbeReplay)
{
    auto cfg = TestSubPartitionConfig();
    const std::string spillPath = TestUtil::GenerateSpillPath();
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

    auto *hashBuilder = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator());
    hashBuilder->AddInput(MakeLongBatch({1, 2, 3, 4}, {10, 20, 30, 40}));
    auto *lookupJoin = dynamic_cast<LookupJoinOperator *>(lookupFactory->CreateOperator());
    lookupJoin->AddInput(MakeLongBatch({2, 4}, {200, 400}));
    const int32_t outputRows = PumpJoin(hashBuilder, lookupJoin);

    EXPECT_EQ(outputRows, 2);
    EXPECT_GT(state->BuildSpilledRows(), 0U);
    EXPECT_EQ(hashBuilder->GetStatus(), OMNI_STATUS_FINISHED);
    EXPECT_EQ(lookupJoin->GetStatus(), OMNI_STATUS_FINISHED);

    op::Operator::DeleteOperator(lookupJoin);
    op::Operator::DeleteOperator(hashBuilder);
    delete lookupFactory;
    delete hashBuilderFactory;
}

TEST(JoinSpillTest, InnerJoinVarcharKeyValueBuildAndProbeReplay)
{
    auto cfg = TestSubPartitionConfig();
    const std::string spillPath = TestUtil::GenerateSpillPath();
    mkdir(spillPath.c_str(), 0750);

    DataTypes rowTypes(std::vector<DataTypePtr>({ VarcharType(8), VarcharType(8) }));
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

    auto *hashBuilder = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator());
    hashBuilder->AddInput(MakeVarcharBatch({"aa", "bb", "cc"}, {"ba", "bb", "bc"}));
    auto *lookupJoin = dynamic_cast<LookupJoinOperator *>(lookupFactory->CreateOperator());
    lookupJoin->AddInput(MakeVarcharBatch({"bb", "cc"}, {"pb", "pc"}));
    const int32_t outputRows = PumpJoin(hashBuilder, lookupJoin);

    EXPECT_EQ(outputRows, 2);
    EXPECT_GT(state->BuildSpilledRows(), 0U);
    EXPECT_EQ(hashBuilder->GetStatus(), OMNI_STATUS_FINISHED);
    EXPECT_EQ(lookupJoin->GetStatus(), OMNI_STATUS_FINISHED);

    op::Operator::DeleteOperator(lookupJoin);
    op::Operator::DeleteOperator(hashBuilder);
    delete lookupFactory;
    delete hashBuilderFactory;
}

TEST(JoinSpillTest, SmallBatchBelowThresholdDoesNotSpill)
{
    auto cfg = TestSubPartitionConfig();
    const std::string spillPath = TestUtil::GenerateSpillPath();
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

    auto *hashBuilder = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator());
    hashBuilder->AddInput(MakeLongBatch({1, 2, 3, 4}, {10, 20, 30, 40}));
    auto *lookupJoin = dynamic_cast<LookupJoinOperator *>(lookupFactory->CreateOperator());
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

TEST(JoinSpillTest, BuildSpillUsesIndependentRunIdsForMultipleBatches)
{
    auto cfg = TestSubPartitionConfig();
    const std::string spillPath = TestUtil::GenerateSpillPath();
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

    auto *hashBuilder = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator());
    hashBuilder->AddInput(MakeLongBatch({1, 2, 3, 4}, {10, 20, 30, 40}));
    hashBuilder->AddInput(MakeLongBatch({5, 6, 7, 8}, {50, 60, 70, 80}));
    auto *lookupJoin = dynamic_cast<LookupJoinOperator *>(lookupFactory->CreateOperator());
    lookupJoin->AddInput(MakeLongBatch({2, 6}, {200, 600}));
    const int32_t outputRows = PumpJoin(hashBuilder, lookupJoin);

    EXPECT_EQ(outputRows, 2);
    EXPECT_EQ(state->BuildSpilledRows(), 8U);
    EXPECT_GE(state->BuildSpilledFiles(), 2U);
    EXPECT_EQ(hashBuilder->GetStatus(), OMNI_STATUS_FINISHED);
    EXPECT_EQ(lookupJoin->GetStatus(), OMNI_STATUS_FINISHED);

    op::Operator::DeleteOperator(lookupJoin);
    op::Operator::DeleteOperator(hashBuilder);
    delete lookupFactory;
    delete hashBuilderFactory;
}
