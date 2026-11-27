/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 * Description: Checked normal/adaptive build preserves every sparse key.
 */
#include <memory>
#include <vector>

#include "gtest/gtest.h"
#include "memory/omni_task_memory_pool.h"
#include "operator/join/join_hash_table_variants.h"
#include "type/data_types.h"
#include "vector/vector_helper.h"

using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::vec;

namespace {
using Table = JoinHashTableVariants<int64_t, RowRefList>;

VectorBatch *MakeSparseKeys(omniruntime::mem::OmniTaskMemoryPool *pool, int32_t rows)
{
    auto *batch = new VectorBatch(rows);
    auto *keys = new Vector<int64_t>(rows, pool);
    for (int32_t row = 0; row < rows; ++row) {
        keys->SetValue(row, static_cast<int64_t>(row) * 10000);
    }
    batch->Append(keys);
    return batch;
}

void CheckScalarProbe(bool adaptive)
{
    omniruntime::mem::OmniTaskMemoryPool pool(nullptr);
    DataTypes types({LongType()});
    std::vector<int32_t> hashCols{0};
    Table table(1, &types, hashCols, OMNI_JOIN_TYPE_INNER, OMNI_BUILD_RIGHT, false);
    table.SetPool(&pool);
    constexpr int32_t rows = 10000;
    table.AddVecBatch(0, MakeSparseKeys(&pool, rows));
    table.Prepare(0);
    if (adaptive) {
        table.BuildHashTableAdaptive(0);
    } else {
        table.BuildHashTable(0);
    }
    const auto freeBatch = [](VectorBatch *batch) { VectorHelper::FreeVecBatch(batch); };
    std::unique_ptr<VectorBatch, decltype(freeBatch)> probe(MakeSparseKeys(&pool, rows + 1), freeBatch);
    BaseVector *probeColumns[] = {probe->Get(0)};
    std::vector<VectorSerializerIgnoreNull> serializers;
    ExecutionContext context(4096, &pool);
    for (int32_t row = 0; row <= rows; ++row) {
        const auto result = table.Find(serializers, &context, probeColumns, 1, row, 0);
        ASSERT_EQ(result.IsInsert(), row < rows) << "row=" << row << " adaptive=" << adaptive;
    }
}
} // namespace

TEST(JoinAdaptiveSeedTest, PresizedBuildPreservesAllSparseKeys)
{
    CheckScalarProbe(false);
}

TEST(JoinAdaptiveSeedTest, AdaptiveRestoreGrowsWithoutLosingSparseKeys)
{
    CheckScalarProbe(true);
}
