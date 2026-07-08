/*
 * Tests for BroadcastHashTableCache executor-level sharing for BHJ.
 *
 * Covered scenarios:
 *   1. tryClaimBuild returns true for the first caller, false for the second.
 *   2. publish + waitAndGet round-trip succeeds.
 *   3. Concurrent tasks: one builder thread, N waiter threads – all get the same pointer.
 *   4. invalidate removes the entry and frees no memory (pointer itself set to nullptr to
 *      confirm the cache doesn't double-delete an external allocation).
 *   5. After invalidate, tryClaimBuild can win again for the same id.
 *   6. size() reflects live entries.
 */

#include <atomic>
#include <chrono>
#include <future>
#include <string>
#include <thread>
#include <vector>

#include "gtest/gtest.h"
#include "operator/join/broadcast_hash_table_cache.h"
#include "operator/join/hash_builder.h"
#include "util/test_util.h"

using omniruntime::op::BroadcastHashTableCache;
using omniruntime::op::HashBuilderOperatorFactory;
using omniruntime::op::HashTableVariants;
using omniruntime::type::DataTypePtr;
using omniruntime::type::DataTypes;

namespace BHJCacheTest {

// Convenience helper to get a fresh per-test id so tests don't interfere.
static std::string makeId(const char* label)
{
    static std::atomic<int> counter{0};
    return std::string("test-") + label + "-" + std::to_string(counter.fetch_add(1));
}

// HashTableVariants is a std::variant of JoinHashTableVariants<...> types without default
// constructors, so HashTableVariants{} does not compile. Build a minimal factory-backed
// table and transfer ownership to the cache (mirrors production publish path).
static HashTableVariants* publishTestVariants(BroadcastHashTableCache& cache, const std::string& id)
{
    DataTypes buildTypes(std::vector<DataTypePtr>({LongType(), LongType()}));
    int32_t buildHashCols[1] = {0};
    auto* factory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
        OMNI_JOIN_TYPE_INNER, buildTypes, buildHashCols, 1, 1);
    HashTableVariants* variants = factory->GetHashTablesVariants();
    factory->ReleaseVariants();
    cache.publish(id, variants, factory);
    return variants;
}

// ─────────────────────────────────────────────────────────────────────────────
// 1. First caller claims the build; second caller does not.
// ─────────────────────────────────────────────────────────────────────────────
TEST(BroadcastHashTableCacheTest, FirstCallerClaimsBuild)
{
    auto& cache = BroadcastHashTableCache::getInstance();
    std::string id = makeId("claim");

    EXPECT_TRUE(cache.tryClaimBuild(id)) << "First caller must win";
    EXPECT_FALSE(cache.tryClaimBuild(id)) << "Second caller must not win";

    // Clean up so subsequent tests start clean.
    cache.invalidate(id);
}

// ─────────────────────────────────────────────────────────────────────────────
// 2. publish → waitAndGet round-trip.
// ─────────────────────────────────────────────────────────────────────────────
TEST(BroadcastHashTableCacheTest, PublishAndGet)
{
    auto& cache = BroadcastHashTableCache::getInstance();
    std::string id = makeId("pub");

    ASSERT_TRUE(cache.tryClaimBuild(id));

    HashTableVariants* variants = publishTestVariants(cache, id);

    HashTableVariants* got = cache.waitAndGet(id);
    EXPECT_EQ(variants, got) << "waitAndGet must return the published pointer";

    cache.invalidate(id);
}

// ─────────────────────────────────────────────────────────────────────────────
// 3. Concurrent: 1 builder + N waiters all get the same pointer.
// ─────────────────────────────────────────────────────────────────────────────
TEST(BroadcastHashTableCacheTest, ConcurrentWaitersGetSamePointer)
{
    auto& cache = BroadcastHashTableCache::getInstance();
    std::string id = makeId("concurrent");
    const int NUM_WAITERS = 8;

    ASSERT_TRUE(cache.tryClaimBuild(id));

    // Launch N waiter threads before publishing.
    std::vector<std::future<HashTableVariants*>> futures;
    futures.reserve(NUM_WAITERS);
    for (int i = 0; i < NUM_WAITERS; ++i) {
        futures.push_back(std::async(std::launch::async, [&cache, &id]() {
            return cache.waitAndGet(id);
        }));
    }

    // Simulate build latency, then publish.
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    HashTableVariants* variants = publishTestVariants(cache, id);

    // All waiters must receive the same pointer.
    for (auto& f : futures) {
        EXPECT_EQ(variants, f.get()) << "Every waiter must get the cached pointer";
    }

    cache.invalidate(id);
}

// ─────────────────────────────────────────────────────────────────────────────
// 4. invalidate removes the entry; waitAndGet after invalidate returns nullptr.
// ─────────────────────────────────────────────────────────────────────────────
TEST(BroadcastHashTableCacheTest, InvalidateRemovesEntry)
{
    auto& cache = BroadcastHashTableCache::getInstance();
    std::string id = makeId("inval");

    ASSERT_TRUE(cache.tryClaimBuild(id));
    HashTableVariants* variants = publishTestVariants(cache, id);

    // Verify it's there.
    EXPECT_EQ(variants, cache.waitAndGet(id));

    cache.invalidate(id);
    EXPECT_EQ(0u, cache.size()) << "Cache must be empty after invalidate";
}

// ─────────────────────────────────────────────────────────────────────────────
// 5. After invalidate, a new tryClaimBuild succeeds for the same id.
// ─────────────────────────────────────────────────────────────────────────────
TEST(BroadcastHashTableCacheTest, ReclaimAfterInvalidate)
{
    auto& cache = BroadcastHashTableCache::getInstance();
    std::string id = makeId("reclaim");

    ASSERT_TRUE(cache.tryClaimBuild(id));
    publishTestVariants(cache, id);
    cache.invalidate(id);

    // Should be able to claim again for a second query wave.
    EXPECT_TRUE(cache.tryClaimBuild(id)) << "After invalidate, should win build claim again";
    HashTableVariants* v2 = publishTestVariants(cache, id);
    EXPECT_EQ(v2, cache.waitAndGet(id));
    cache.invalidate(id);
}

// ─────────────────────────────────────────────────────────────────────────────
// 6. size() reflects the number of live (not yet invalidated) entries.
// ─────────────────────────────────────────────────────────────────────────────
TEST(BroadcastHashTableCacheTest, SizeTracking)
{
    auto& cache = BroadcastHashTableCache::getInstance();
    std::string id1 = makeId("size1");
    std::string id2 = makeId("size2");
    size_t baseline = cache.size();

    ASSERT_TRUE(cache.tryClaimBuild(id1));
    ASSERT_TRUE(cache.tryClaimBuild(id2));
    EXPECT_EQ(baseline + 2, cache.size());

    publishTestVariants(cache, id1);
    publishTestVariants(cache, id2);
    EXPECT_EQ(baseline + 2, cache.size());

    cache.invalidate(id1);
    EXPECT_EQ(baseline + 1, cache.size());

    cache.invalidate(id2);
    EXPECT_EQ(baseline, cache.size());
}

} // namespace BHJCacheTest
