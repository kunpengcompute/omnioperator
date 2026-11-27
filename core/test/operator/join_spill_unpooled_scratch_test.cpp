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
#include <stdexcept>
#include "memory/pool_listener.h"
#include "memory/omni_task_memory_pool.h"
#include "operator/join/hash_builder.h"
#include "operator/join/lookup_join.h"
#include "operator/join/join_spill_state.h"
#include "operator/join/join_sub_partitioner.h"
#include "vector/vector_helper.h"

using namespace omniruntime;
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::vec;

namespace {
// Spark inside reclaim: the task has no headroom, every growth request is denied.
class DenyGrowthListener : public omniruntime::mem::PoolListener {
public:
    void allocationChanged(int64_t diff) override
    {
        if (diff > 0 && deny) {
            ++denied;
            throw std::runtime_error("Spark denied reservation inside reclaim");
        }
    }
    bool deny = false;
    int32_t denied = 0;
};

VectorBatch *BuildBatch(mem::OmniTaskMemoryPool *pool, int32_t rows, bool dictionary)
{
    auto batch = std::make_unique<VectorBatch>(rows);
    auto *keys = new Vector<int64_t>(rows, pool);
    for (int32_t row = 0; row < rows; ++row) keys->SetValue(row, row);
    batch->Append(keys);
    if (dictionary) {
        auto values = std::make_unique<Vector<LargeStringContainer<std::string_view>>>(2, pool);
        values->SetValue(0, "abc");
        values->SetValue(1, "defgh");
        std::vector<int32_t> ids(rows);
        for (int32_t row = 0; row < rows; ++row) ids[row] = row % 2;
        batch->Append(VectorHelper::CreateDictionaryVector(ids.data(), rows, values.get(), OMNI_VARCHAR, false,
            pool));
    } else {
        auto *strings = new Vector<LargeStringContainer<std::string_view>>(rows, pool);
        for (int32_t row = 0; row < rows; ++row) strings->SetValue(row, row % 3 == 0 ? "x" : "longer value");
        batch->Append(strings);
    }
    return batch.release();
}

void CheckReclaimNeedsNoPoolMemory(bool dictionary, JoinType joinType)
{
    DenyGrowthListener listener;
    mem::OmniTaskMemoryPool pool(&listener);
    DataTypes types({LongType(), VarcharType()});
    JoinSubPartitionConfig cfg;
    cfg.numSubPartitions = 4;
    cfg.startPartitionBit = 0;
    cfg.activeSubPartition = 0;
    int32_t key = 0;
    auto state = std::make_shared<JoinSpillState>("/tmp/unpooled-scratch-reclaim", types, types, 4);
    state->SetHandoffSessionEnabled(true);
    state->SetInitialRuntimeLayout(JoinBuildRuntimeLayout::Single);
    std::unique_ptr<HashBuilderOperatorFactory> buildFactory(
        HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(joinType, types, &key, 1, 1));
    buildFactory->SetJoinSpillSubPartitionPolicy(true, 0, cfg, true, false, 10);
    buildFactory->SetJoinSpillState(state);
    auto deleteOp = [](omniruntime::op::Operator *op) { omniruntime::op::Operator::DeleteOperator(op); };
    std::unique_ptr<omniruntime::op::Operator, decltype(deleteOp)> buildOwner(
        buildFactory->CreateOperator(&pool), deleteOp);
    auto *build = static_cast<HashBuilderOperator *>(buildOwner.get());
    constexpr int32_t rows = 4096;
    build->AddInput(BuildBatch(&pool, rows, dictionary));
    build->AddInput(BuildBatch(&pool, rows, dictionary));
    ASSERT_GT(build->reclaimableBytes(), 0);
    const int64_t before = pool.currentBytes();

    // From here on the pool may only shrink: the spill must not ask it for a single byte.
    listener.deny = true;
    int64_t freed = 0;
    ASSERT_NO_THROW(freed = build->reclaim(1));
    listener.deny = false;
    EXPECT_GT(freed, 0);
    EXPECT_EQ(listener.denied, 0);
    EXPECT_LT(pool.currentBytes(), before);
    EXPECT_EQ(state->BuildSpilledRows(), static_cast<uint64_t>(2 * rows));
    EXPECT_GT(state->SpillScratchPeakBytes(), 0u);
}
} // namespace

TEST(JoinSpillUnpooledScratchTest, FlatBuildReclaimNeedsNoPoolMemory)
{
    CheckReclaimNeedsNoPoolMemory(false, OMNI_JOIN_TYPE_INNER);
}

TEST(JoinSpillUnpooledScratchTest, DictionaryBuildReclaimNeedsNoPoolMemory)
{
    CheckReclaimNeedsNoPoolMemory(true, OMNI_JOIN_TYPE_INNER);
}

TEST(JoinSpillUnpooledScratchTest, FullJoinVisitedFlagNeedsNoPoolMemory)
{
    CheckReclaimNeedsNoPoolMemory(false, OMNI_JOIN_TYPE_FULL);
}

TEST(JoinSpillUnpooledScratchTest, PositionalCopyToNullPoolFlattensEncodedColumns)
{
    mem::OmniTaskMemoryPool pool(nullptr);
    std::unique_ptr<VectorBatch, VecBatchDeleter> batch(BuildBatch(&pool, 8, true));
    int positions[] = {7, -1, 2, 0};
    std::unique_ptr<BaseVector> copy(VectorHelper::CopyPositionsVectorToPool(batch->Get(1), positions, 0, 4,
        nullptr));
    ASSERT_NE(copy, nullptr);
    EXPECT_EQ(copy->GetEncoding(), OMNI_FLAT);
    EXPECT_EQ(copy->GetPool(), nullptr);
    auto *strings = static_cast<Vector<LargeStringContainer<std::string_view>> *>(copy.get());
    EXPECT_EQ(strings->GetValue(0), "defgh");
    EXPECT_TRUE(strings->IsNull(1));
    EXPECT_EQ(strings->GetValue(2), "abc");
    EXPECT_EQ(strings->GetValue(3), "abc");
    std::unique_ptr<BaseVector> keys(VectorHelper::CopyPositionsVectorToPool(batch->Get(0), positions, 0, 4,
        nullptr));
    EXPECT_EQ(static_cast<Vector<int64_t> *>(keys.get())->GetValue(0), 7);
    EXPECT_TRUE(keys->IsNull(1));
}
