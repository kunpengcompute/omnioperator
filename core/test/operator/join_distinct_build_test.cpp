/*
 * Distinct build of a filter-free semi / anti / existence join: the build keeps one row per key
 * and grows its hash table while it takes input (HashBuilderOperatorFactory::SetDistinctBuild).
 *
 * Covered here: the join result is the one of the full build, a batch that only repeats known
 * keys leaves nothing behind in the task pool, and the arena mark/rollback that makes that
 * possible returns chunks added after the mark.
 */
#include <gtest/gtest.h>
#include <string>
#include <vector>
#include "memory/simple_arena_allocator.h"
#include "operator/join/hash_builder.h"
#include "operator/join/lookup_join.h"
#include "vector/vector_helper.h"
#include "util/test_util.h"
#include "util/test_pool_fixture.h"

namespace omniruntime {
using namespace omniruntime::vec;
using namespace omniruntime::op;
using namespace TestUtil;
using omniruntime::test::TestPoolFixture;

namespace {
constexpr int32_t kKeys = 1000;

// (varchar key, long payload): a string key is serialized into the build's arena for the lookup.
VectorBatch *MakeBuildBatch(int32_t firstKey, int32_t rows)
{
    DataTypes buildTypes(std::vector<DataTypePtr>({ VarcharType(32), LongType() }));
    std::vector<std::string> keys(rows);
    std::vector<int64_t> payload(rows);
    for (int32_t i = 0; i < rows; ++i) {
        keys[i] = "build-key-" + std::to_string(firstKey + i % kKeys);
        payload[i] = i;
    }
    return CreateVectorBatch(buildTypes, rows, keys.data(), payload.data());
}
} // namespace

TEST_F(TestPoolFixture, arena_rollback_returns_everything_allocated_after_the_mark)
{
    mem::SimpleArenaAllocator arena(4096, allocator());
    auto *first = arena.Allocate(1000);
    memset(first, 0x5a, 1000);
    const auto mark = arena.GetMark();
    const uint64_t usedAtMark = arena.UsedBytes();
    const uint64_t totalAtMark = arena.TotalBytes();
    const int64_t poolAtMark = pool()->currentBytes();

    // Stay inside the first chunk, then force two more chunks.
    arena.Allocate(2000);
    arena.Allocate(64 * 1024);
    arena.Allocate(256 * 1024);
    ASSERT_GT(arena.TotalBytes(), totalAtMark);
    ASSERT_GT(pool()->currentBytes(), poolAtMark);

    arena.RollbackTo(mark);
    EXPECT_EQ(arena.UsedBytes(), usedAtMark);
    EXPECT_EQ(arena.TotalBytes(), totalAtMark);
    EXPECT_EQ(pool()->currentBytes(), poolAtMark) << "chunks added after the mark go back to the pool";
    for (int32_t i = 0; i < 1000; ++i) {
        ASSERT_EQ(first[i], 0x5a) << "data before the mark is untouched";
    }
    // The space after the mark is handed out again from the same address.
    auto *again = arena.Allocate(2000);
    EXPECT_EQ(again, first + 1000);
}

TEST_F(TestPoolFixture, distinct_build_keeps_the_semi_join_result_and_drops_duplicate_batches)
{
    DataTypes buildTypes(std::vector<DataTypePtr>({ VarcharType(32), LongType() }));
    int32_t buildJoinCols[1] = {0};
    auto *hashBuilderFactory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
        OMNI_JOIN_TYPE_LEFT_SEMI, buildTypes, buildJoinCols, 1, 1);
    hashBuilderFactory->SetDistinctBuild(true);
    auto *hashBuilderOperator = dynamic_cast<HashBuilderOperator *>(hashBuilderFactory->CreateOperator(pool()));
    ASSERT_NE(hashBuilderOperator, nullptr);
    hashBuilderOperator->setNoMoreInput(false);

    // Every key four times in the first batch, then batches that only repeat those keys.
    hashBuilderOperator->AddInput(MakeBuildBatch(0, 4 * kKeys));
    hashBuilderOperator->AddInput(MakeBuildBatch(0, kKeys));
    const int64_t usedAfterKeysAreKnown = pool()->currentBytes();
    for (int32_t i = 0; i < 64; ++i) {
        hashBuilderOperator->AddInput(MakeBuildBatch(0, 4 * kKeys));
    }
    EXPECT_EQ(pool()->currentBytes(), usedAfterKeysAreKnown)
        << "a batch of known keys leaves neither rows nor serialized keys in the pool";

    hashBuilderOperator->noMoreInput();
    VectorBatch *hashBuildOutput = nullptr;
    hashBuilderOperator->GetOutput(&hashBuildOutput);

    // Probe: keys 0..kKeys-1 exist, the next kKeys do not.
    DataTypes probeTypes(std::vector<DataTypePtr>({ VarcharType(32), LongType() }));
    const int32_t probeRows = 2 * kKeys;
    std::vector<std::string> probeKeys(probeRows);
    std::vector<int64_t> probePayload(probeRows);
    for (int32_t i = 0; i < probeRows; ++i) {
        probeKeys[i] = "build-key-" + std::to_string(i);
        probePayload[i] = i;
    }
    VectorBatch *probeVecBatch = CreateVectorBatch(probeTypes, probeRows, probeKeys.data(), probePayload.data());
    int32_t probeOutputCols[2] = {0, 1};
    int32_t probeHashCols[1] = {0};
    int32_t buildOutputCols[0] = {};
    DataTypes buildOutputTypes(std::vector<DataTypePtr>({ VarcharType(32), LongType() }));
    auto *lookupJoinFactory = LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(
        probeTypes, probeOutputCols, 2, probeHashCols, 1, buildOutputCols, 0, buildOutputTypes,
        reinterpret_cast<int64_t>(hashBuilderFactory), nullptr, false, nullptr);
    auto *lookupJoinOperator = dynamic_cast<LookupJoinOperator *>(lookupJoinFactory->CreateOperator(pool()));
    ASSERT_NE(lookupJoinOperator, nullptr);

    lookupJoinOperator->AddInput(probeVecBatch);
    VectorBatch *outputVecBatch = nullptr;
    lookupJoinOperator->GetOutput(&outputVecBatch);
    ASSERT_NE(outputVecBatch, nullptr);
    EXPECT_EQ(outputVecBatch->GetRowCount(), kKeys) << "each probe row with a build key appears once";
    auto *payload = static_cast<Vector<int64_t> *>(outputVecBatch->Get(1));
    for (int32_t i = 0; i < outputVecBatch->GetRowCount(); ++i) {
        EXPECT_LT(payload->GetValue(i), kKeys) << "only rows whose key is in the build";
    }

    VectorHelper::FreeVecBatch(outputVecBatch);
    omniruntime::op::Operator::DeleteOperator(lookupJoinOperator);
    omniruntime::op::Operator::DeleteOperator(hashBuilderOperator);
    delete lookupJoinFactory;
    delete hashBuilderFactory;
}
} // namespace omniruntime
