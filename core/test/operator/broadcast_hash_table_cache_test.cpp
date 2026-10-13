/*
 * Executor cache ownership and lifetime tests.
 */
#include <atomic>
#include <chrono>
#include <future>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "gtest/gtest.h"
#include "memory/omni_task_memory_pool.h"
#include "operator/join/broadcast_hash_table_cache.h"
#include "operator/join/hash_builder.h"
#include "util/test_util.h"
#include "vector/vector.h"
#include "vector/vector_batch.h"
#include "vector/vector_helper.h"

using omniruntime::op::BroadcastHashTableCache;
using omniruntime::op::BroadcastCacheLeaseSlot;
using omniruntime::op::BroadcastEntryMemory;
using omniruntime::op::HashBuilderOperatorFactory;
using omniruntime::op::HashTableVariants;
using omniruntime::type::DataTypePtr;
using omniruntime::type::DataTypes;
using omniruntime::vec::Vector;
using omniruntime::vec::VectorBatch;
using omniruntime::vec::VectorHelper;

namespace BHJCacheTest {

class TestEntryMemory final : public BroadcastEntryMemory {
public:
    TestEntryMemory() : pool_(std::make_unique<omniruntime::mem::OmniTaskMemoryPool>(nullptr)) {}
    omniruntime::mem::OmniTaskMemoryPool* pool() const override { return pool_.get(); }

private:
    std::unique_ptr<omniruntime::mem::OmniTaskMemoryPool> pool_;
};

static std::string makeId(const char* label)
{
    static std::once_flag configured;
    std::call_once(configured, [] {
        BroadcastHashTableCache::getInstance().configureMemoryFactory(
            [] { return std::make_shared<TestEntryMemory>(); });
    });
    static std::atomic<int> counter{0};
    return std::string("test-") + label + "-" + std::to_string(counter.fetch_add(1));
}

static BroadcastHashTableCache::Lease publishTestVariants(
    const std::shared_ptr<BroadcastHashTableCache::BuildTicket>& ticket,
    bool initializePool = true)
{
    if (initializePool) {
        ticket->pool();
    }
    DataTypes buildTypes(std::vector<DataTypePtr>({LongType(), LongType()}));
    int32_t buildHashCols[1] = {0};
    auto* factory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
        OMNI_JOIN_TYPE_INNER, buildTypes, buildHashCols, 1, 1);
    auto lease = ticket->prepare();
    auto* variants = factory->ReleaseVariants();
    factory->MarkCachePinned();
    ticket->publish(lease, variants, factory);
    return lease;
}

TEST(BroadcastHashTableCacheTest, AbandonedBuilderWakesWaiters)
{
    auto& cache = BroadcastHashTableCache::getInstance();
    auto id = makeId("abandon");
    auto first = cache.acquire(id);
    ASSERT_NE(nullptr, first.builder);
    auto next = std::async(std::launch::async, [&] { return cache.acquire(id); });
    EXPECT_EQ(std::future_status::timeout, next.wait_for(std::chrono::milliseconds(20)));
    first.builder.reset();
    auto second = next.get();
    ASSERT_NE(nullptr, second.builder);
    second.builder->fail();
    EXPECT_EQ(0u, cache.size());
}

TEST(BroadcastHashTableCacheTest, PublishAndReuseKeepsSameTable)
{
    auto& cache = BroadcastHashTableCache::getInstance();
    auto id = makeId("publish");
    auto builder = cache.acquire(id).builder;
    ASSERT_NE(nullptr, builder);
    auto firstLease = publishTestVariants(builder);
    auto reader = cache.acquire(id);
    ASSERT_TRUE(reader.reader);
    EXPECT_EQ(firstLease.get(), reader.reader.get());
    cache.invalidate(id);
    EXPECT_EQ(firstLease.get(), reader.reader.get());
}

TEST(BroadcastHashTableCacheTest, ConcurrentWaitersGetOwningLeases)
{
    auto& cache = BroadcastHashTableCache::getInstance();
    auto id = makeId("waiters");
    auto builder = cache.acquire(id).builder;
    ASSERT_NE(nullptr, builder);
    std::vector<std::future<BroadcastHashTableCache::Lease>> futures;
    for (int i = 0; i < 8; ++i) {
        futures.push_back(std::async(std::launch::async, [&] { return cache.acquire(id).reader; }));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    auto built = publishTestVariants(builder);
    for (auto& future : futures) {
        auto reader = future.get();
        EXPECT_EQ(built.get(), reader.get());
    }
    cache.invalidate(id);
}

TEST(BroadcastHashTableCacheTest, InvalidationDefersDeletionUntilReadersFinish)
{
    auto& cache = BroadcastHashTableCache::getInstance();
    auto id = makeId("retire");
    auto builder = cache.acquire(id).builder;
    ASSERT_NE(nullptr, builder);
    auto built = publishTestVariants(builder);
    auto reader = cache.acquire(id).reader;
    auto* address = built.get();
    cache.invalidate(id);
    EXPECT_EQ(0u, cache.size());
    EXPECT_EQ(address, reader.get());
    EXPECT_EQ(address, built.get());
    auto replacement = cache.acquire(id);
    ASSERT_NE(nullptr, replacement.builder);
    replacement.builder->fail();
    EXPECT_EQ(address, reader.get());
}

TEST(BroadcastHashTableCacheTest, OldGenerationCannotPublishIntoReplacement)
{
    auto& cache = BroadcastHashTableCache::getInstance();
    auto id = makeId("generation");
    auto oldTicket = cache.acquire(id).builder;
    ASSERT_NE(nullptr, oldTicket);
    oldTicket->pool();
    cache.invalidate(id);
    auto newTicket = cache.acquire(id).builder;
    ASSERT_NE(nullptr, newTicket);
    auto oldLease = publishTestVariants(oldTicket, false);
    auto newLease = publishTestVariants(newTicket);
    auto reader = cache.acquire(id).reader;
    ASSERT_TRUE(reader);
    EXPECT_EQ(newLease.get(), reader.get());
    EXPECT_NE(oldLease.get(), reader.get());
    cache.invalidate(id);
}

TEST(BroadcastHashTableCacheTest, SizeTracksReadyAndBuildingEntries)
{
    auto& cache = BroadcastHashTableCache::getInstance();
    auto firstId = makeId("size-a");
    auto secondId = makeId("size-b");
    auto baseline = cache.size();
    auto first = cache.acquire(firstId).builder;
    auto second = cache.acquire(secondId).builder;
    ASSERT_NE(nullptr, first);
    ASSERT_NE(nullptr, second);
    EXPECT_EQ(baseline + 2, cache.size());
    auto lease = publishTestVariants(first);
    EXPECT_EQ(baseline + 2, cache.size());
    second->fail();
    EXPECT_EQ(baseline + 1, cache.size());
    cache.invalidate(firstId);
    EXPECT_EQ(baseline, cache.size());
}

TEST(BroadcastHashTableCacheTest, ExchangeRetiresEveryTableSignature)
{
    auto& cache = BroadcastHashTableCache::getInstance();
    auto exchange = makeId("exchange");
    auto firstId = exchange + "#join-a";
    auto secondId = exchange + "#join-b";
    auto firstBuilder = cache.acquire(firstId).builder;
    auto secondBuilder = cache.acquire(secondId).builder;
    ASSERT_NE(nullptr, firstBuilder);
    ASSERT_NE(nullptr, secondBuilder);
    auto firstLease = publishTestVariants(firstBuilder);
    auto secondLease = publishTestVariants(secondBuilder);
    auto* first = firstLease.get();
    auto* second = secondLease.get();
    cache.invalidatePrefix(exchange);
    EXPECT_EQ(first, firstLease.get());
    EXPECT_EQ(second, secondLease.get());
    EXPECT_EQ(0u, cache.size());
    EXPECT_TRUE(cache.acquire(firstId).retired);
    EXPECT_TRUE(cache.acquire(secondId).retired);
}

TEST(BroadcastHashTableCacheTest, PoollessCopySurvivesSourceTaskPool)
{
    auto pool = std::make_unique<omniruntime::mem::OmniTaskMemoryPool>(nullptr);
    auto *source = new VectorBatch(3);
    auto *keys = new Vector<int64_t>(3, pool.get());
    auto *values = new Vector<omniruntime::vec::LargeStringContainer<std::string_view>>(
        3, pool.get());
    for (int32_t row = 0; row < 3; ++row) {
        keys->SetValue(row, 10 + row);
    }
    values->SetValue(0, std::string_view("alpha"));
    values->SetNull(1);
    values->SetValue(2, std::string_view("a-value-long-enough-to-require-owned-storage"));
    source->Append(keys);
    source->Append(values);
    ASSERT_GT(pool->currentBytes(), 0);

    auto copy = VectorHelper::CopyVectorBatchToPool(source, nullptr);
    ASSERT_EQ(copy->Get(0)->GetPool(), nullptr);
    ASSERT_EQ(copy->Get(1)->GetPool(), nullptr);

    VectorHelper::FreeVecBatch(source);
    EXPECT_EQ(pool->currentBytes(), 0);
    pool.reset();

    auto *copiedKeys = static_cast<Vector<int64_t> *>(copy->Get(0));
    auto *copiedValues = static_cast<
        Vector<omniruntime::vec::LargeStringContainer<std::string_view>> *>(copy->Get(1));
    EXPECT_EQ(copiedKeys->GetValue(2), 12);
    EXPECT_EQ(copiedValues->GetValue(0), "alpha");
    EXPECT_TRUE(copiedValues->IsNull(1));
    EXPECT_EQ(copiedValues->GetValue(2), "a-value-long-enough-to-require-owned-storage");
    VectorHelper::FreeVecBatch(copy.release());
}

TEST(BroadcastHashTableCacheTest, CachedBuilderDoesNotRetainTaskPoolMemory)
{
    auto& cache = BroadcastHashTableCache::getInstance();
    const std::string id = makeId("poolless-builder");
    auto ticket = cache.acquire(id).builder;
    ASSERT_NE(nullptr, ticket);

    DataTypes buildTypes(std::vector<DataTypePtr>({LongType(), LongType()}));
    int32_t buildHashCols[1] = {0};
    auto *factory = HashBuilderOperatorFactory::CreateHashBuilderOperatorFactory(
        OMNI_JOIN_TYPE_INNER, buildTypes, buildHashCols, 1, 1);
    factory->SetBroadcastHashTableId(id);
    factory->SetDynamicFilterPushdownEnabled(false);
    auto slot = std::make_shared<BroadcastCacheLeaseSlot>();
    factory->SetCacheBuildTicket(ticket);
    factory->SetCacheLeaseSlot(slot);
    auto* cachePool = ticket->pool();

    auto pool = std::make_unique<omniruntime::mem::OmniTaskMemoryPool>(nullptr);
    auto *builder = static_cast<omniruntime::op::HashBuilderOperator *>(
        factory->CreateOperator(pool.get()));
    const int64_t operatorBaseline = pool->currentBytes();

    auto *batch = new VectorBatch(4);
    auto *keys = new Vector<int64_t>(4, pool.get());
    auto *values = new Vector<int64_t>(4, pool.get());
    for (int32_t row = 0; row < 4; ++row) {
        keys->SetValue(row, row);
        values->SetValue(row, 100 + row);
    }
    batch->Append(keys);
    batch->Append(values);
    ASSERT_GT(pool->currentBytes(), operatorBaseline);

    ASSERT_EQ(builder->AddInput(batch), 0);
    EXPECT_EQ(pool->currentBytes(), operatorBaseline)
        << "cache-owned build payload must not remain charged to its source task";

    builder->noMoreInput();
    VectorBatch *unused = nullptr;
    ASSERT_EQ(builder->GetOutput(&unused), 0);
    ASSERT_TRUE(slot->lease);
    // A reader may enable DFP even when the publishing task did not. Shared
    // metadata must already exist before any reader attaches to this lease.
    std::visit([](const auto &table) {
        EXPECT_EQ(table.GetStatus(), OMNI_STATUS_FINISHED);
        EXPECT_EQ(table.GetDynamicFilters().size(), 1U);
        EXPECT_NE(table.GetDynamicFilters().find(0), table.GetDynamicFilters().end());
    }, *slot->lease.get());
    EXPECT_GT(cachePool->currentBytes(), 0);
    EXPECT_EQ(pool->currentBytes(), operatorBaseline)
        << "cache-owned hash table and arena must remain outside the source task pool";

    omniruntime::op::Operator::DeleteOperator(builder);
    pool.reset();
    auto *cached = slot->lease.get();
    cache.invalidate(id);
    EXPECT_EQ(cached, slot->lease.get());
    EXPECT_GT(cachePool->currentBytes(), 0);
}

} // namespace BHJCacheTest
