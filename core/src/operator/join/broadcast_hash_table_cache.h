/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Executor-local broadcast hash table cache.
 */
#pragma once

#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "broadcast_hash_table_cache_control.h"
#include "join_hash_table_variants.h"

namespace omniruntime {
namespace op {

class HashBuilderOperatorFactory;

class BroadcastHashTableCache {
private:
    struct Payload;
    struct Entry;

public:
    // A reader keeps the table and pinned build metadata alive after
    // invalidate() removes the entry from the cache index.
    class Lease {
    public:
        Lease() = default;
        HashTableVariants* get() const;
        explicit operator bool() const { return payload_ != nullptr; }

    private:
        friend class BroadcastHashTableCache;
        explicit Lease(std::shared_ptr<Payload> payload) : payload_(std::move(payload)) {}
        std::shared_ptr<Payload> payload_;
    };

    // The first arriving task owns this token until it publishes or fails.
    // Abandoning a token wakes waiters and permits a fresh builder to claim.
    class BuildTicket {
    public:
        ~BuildTicket();
        BuildTicket(const BuildTicket&) = delete;
        BuildTicket& operator=(const BuildTicket&) = delete;

        // Allocate the owning handle while the factory still owns the table.
        // publish() then performs only non-allocating ownership transfers.
        Lease prepare();
        mem::OmniTaskMemoryPool* pool();
        void publish(Lease& prepared, HashTableVariants* variants,
            HashBuilderOperatorFactory* pinningFactory);
        void fail();

    private:
        friend class BroadcastHashTableCache;
        BuildTicket(BroadcastHashTableCache* cache, std::shared_ptr<Entry> entry)
            : cache_(cache), entry_(std::move(entry)) {}
        BroadcastHashTableCache* cache_;
        std::shared_ptr<Entry> entry_;
        bool active_{true};
    };

    struct Acquisition {
        std::shared_ptr<BuildTicket> builder;
        Lease reader;
        bool retired{false};
    };

    static BroadcastHashTableCache& getInstance();

    BroadcastHashTableCache(const BroadcastHashTableCache&) = delete;
    BroadcastHashTableCache& operator=(const BroadcastHashTableCache&) = delete;

    // One atomic claim-or-wait operation. There is no raw-pointer handoff.
    Acquisition acquire(const std::string& id);
    using MemoryFactory = BroadcastEntryMemoryFactory;
    void configureMemoryFactory(MemoryFactory factory);
    void invalidate(const std::string& id);
    // The broadcast relation owns all table signatures for its exchange key.
    void invalidatePrefix(const std::string& exchangeKey);
    void retireAll();
    size_t size() const;
    using Stats = BroadcastCacheStats;
    Stats stats() const;

private:
    BroadcastHashTableCache() = default;

    struct Payload {
        ~Payload();

        // Destruction is in reverse order: table, then pinned metadata.
        std::shared_ptr<BroadcastEntryMemory> memory;
        std::unique_ptr<HashBuilderOperatorFactory> pinningFactory;
        std::unique_ptr<HashTableVariants> variants;
    };

    enum class EntryState { BUILDING, READY, FAILED, RETIRED };
    struct Entry {
        Entry(std::string id, MemoryFactory factory)
            : id(std::move(id)), memoryFactory(std::move(factory)) {}
        std::string id;
        EntryState state{EntryState::BUILDING};
        MemoryFactory memoryFactory;
        std::shared_ptr<BroadcastEntryMemory> memory;
        std::shared_ptr<Payload> payload;
    };

    void publish(const std::shared_ptr<Entry>& entry, const Lease& prepared);
    void fail(const std::shared_ptr<Entry>& entry);

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::unordered_map<std::string, std::shared_ptr<Entry>> entries_;
    std::unordered_set<std::string> retiredExchangeKeys_;
    MemoryFactory memoryFactory_;
    bool draining_{false};
    uint64_t builds_{0};
    uint64_t hits_{0};
    uint64_t retirements_{0};
};

// Kept by a task's probe factory. The cache-owned builder factory holds only
// a weak reference, avoiding a cycle through the pinned factory metadata.
struct BroadcastCacheLeaseSlot {
    BroadcastHashTableCache::Lease lease;
};

} // namespace op
} // namespace omniruntime
