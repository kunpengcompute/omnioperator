/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include <gtest/gtest.h>
#include "memory/pool_listener.h"
#include "memory/omni_task_memory_pool.h"
#include "operator/join/hash_builder.h"
#include "operator/join/lookup_join.h"
#include "operator/join/join_spill_state.h"
#include "operator/join/join_sub_partitioner.h"
#include "operator/spill/spiller.h"
#include "vector/vector_helper.h"

using namespace omniruntime;
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::vec;

namespace {
class EncodedReclaimListener : public omniruntime::mem::PoolListener {
public:
    void allocationChanged(int64_t) override {}
};

VectorBatch *EncodedBatch(mem::OmniTaskMemoryPool *pool, bool dictionary)
{
    constexpr int32_t rows = 8;
    auto batch = std::make_unique<VectorBatch>(rows);
    auto *keys = new Vector<int64_t>(rows, pool);
    for (int32_t row = 0; row < rows; ++row) keys->SetValue(row, row);
    batch->Append(keys);
    if (dictionary) {
        auto values = std::make_unique<Vector<LargeStringContainer<std::string_view>>>(2, pool);
        values->SetValue(0, "abc");
        values->SetValue(1, "defgh");
        int32_t ids[] = {0, 1, 0, 1, 0, 1, 0, 1};
        batch->Append(VectorHelper::CreateDictionaryVector(ids, rows, values.get(), OMNI_VARCHAR, false, pool));
    } else {
        batch->Append(new ConstVector<std::string_view>("abcd", OMNI_VARCHAR, rows, pool));
    }
    return batch.release();
}

void CheckEncodedReclaim(bool dictionary, bool heldByProbe)
{
    EncodedReclaimListener listener;
    mem::OmniTaskMemoryPool pool(&listener);
    DataTypes types({LongType(), VarcharType()});
    JoinSubPartitionConfig cfg;
    cfg.numSubPartitions = 4;
    cfg.startPartitionBit = 0;
    cfg.activeSubPartition = 0;
    int32_t key = 0;
    auto state = std::make_shared<JoinSpillState>("/tmp/encoded-reclaim", types, types, 4);
    state->SetHandoffSessionEnabled(true);
    state->SetInitialRuntimeLayout(JoinBuildRuntimeLayout::Single);
    std::unique_ptr<HashBuilderOperatorFactory> buildFactory(
        HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
            OMNI_JOIN_TYPE_INNER, types, &key, 1, 1));
    buildFactory->SetJoinSpillSubPartitionPolicy(true, 0, cfg, true, false, 10);
    buildFactory->SetJoinSpillState(state);
    int32_t outputs[] = {0, 1};
    std::unique_ptr<LookupJoinOperatorFactory> probeFactory(
        LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(
            types, outputs, 2, &key, 1, outputs, 2, types,
            reinterpret_cast<int64_t>(buildFactory.get()), nullptr, false, nullptr));
    probeFactory->SetJoinSpillPolicy(true, cfg, true);
    probeFactory->SetJoinSpillState(state);
    auto deleteOp = [](omniruntime::op::Operator *op) { omniruntime::op::Operator::DeleteOperator(op); };
    std::unique_ptr<omniruntime::op::Operator, decltype(deleteOp)> buildOwner(
        buildFactory->CreateOperator(&pool), deleteOp);
    std::unique_ptr<omniruntime::op::Operator, decltype(deleteOp)> probeOwner(
        probeFactory->CreateOperator(&pool), deleteOp);
    auto *build = static_cast<HashBuilderOperator *>(buildOwner.get());
    auto *probe = static_cast<LookupJoinOperator *>(probeOwner.get());
    probe->setNoMoreInput(false);
    build->AddInput(EncodedBatch(&pool, dictionary));
    EXPECT_GT(build->reclaimableBytes(), 0);
    if (heldByProbe) {
        build->noMoreInput();
        VectorBatch *unused = nullptr;
        build->GetOutput(&unused);
        ASSERT_EQ(unused, nullptr);
        probe->GetOutput(&unused);
        ASSERT_EQ(unused, nullptr);
        probe->AddInput(EncodedBatch(&pool, dictionary));
        EXPECT_GT(probe->reclaimableBytes(), 0);
        EXPECT_GT(probe->reclaim(1), 0);
    } else {
        EXPECT_GT(build->reclaim(1), 0);
    }
    EXPECT_EQ(state->BuildSpilledRows(), 8);
}
} // namespace

TEST(JoinSpillEncodedReclaimTest, DictionaryBuild) { CheckEncodedReclaim(true, false); }
TEST(JoinSpillEncodedReclaimTest, ConstantBuild) { CheckEncodedReclaim(false, false); }
TEST(JoinSpillEncodedReclaimTest, DictionaryProbe) { CheckEncodedReclaim(true, true); }
TEST(JoinSpillEncodedReclaimTest, ConstantProbe) { CheckEncodedReclaim(false, true); }

TEST(JoinSpillEncodedReclaimTest, SpillSizeHandlesEncodedStrings)
{
    mem::OmniTaskMemoryPool pool(nullptr);
    DataTypes types({LongType(), VarcharType()});
    for (bool dictionary : {false, true}) {
        std::unique_ptr<VectorBatch, VecBatchDeleter> batch(EncodedBatch(&pool, dictionary));
        // Eight longs, two null bitmaps, nine string offsets and 32 string bytes.
        EXPECT_EQ(Spiller::CollectVecBatchSize(types, batch.get()), 4 + 64 + 2 + 36 + 32);
    }
}
