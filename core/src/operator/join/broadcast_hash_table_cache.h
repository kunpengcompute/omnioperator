/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * @Description: Executor-level cache for BHJ (Broadcast Hash Join) hash tables.
 *               Ensures the hash table is built only once per executor per broadcast relation,
 *               and shared across all tasks that join against the same broadcast.
 */
#pragma once

#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "join_hash_table_variants.h"

namespace omniruntime {
namespace op {

class HashBuilderOperatorFactory;

/**
 * Executor-level singleton cache for broadcast hash tables.
 *
 * Lifecycle:
 *   1. First task for a given buildHashTableId calls tryClaimBuild() → returns true.
 *      It builds the hash table normally, then calls publish() to share it.
 *   2. Concurrent tasks for the same id call tryClaimBuild() → returns false.
 *      They call waitAndGet() which blocks until the first task finishes publishing.
 *   3. When the Spark broadcast is unpersisted (via JNI), invalidate() is called to
 *      free the hash table and remove the cache entry.
 *
 * Thread safety: all public methods are thread-safe.
 */
class BroadcastHashTableCache {
public:
    static BroadcastHashTableCache& getInstance();

    BroadcastHashTableCache(const BroadcastHashTableCache&) = delete;
    BroadcastHashTableCache& operator=(const BroadcastHashTableCache&) = delete;

    /**
     * Try to claim the build right for the given id.
     * @return true  if this caller should build the hash table (first arrival).
     *         false if another task is already building or the table is ready.
     */
    bool tryClaimBuild(const std::string& buildHashTableId);

    /**
     * Publish a completed hash table into the cache.
     * Transfers ownership of `variants` to the cache; the caller must NOT delete it.
     * `pinningFactory` is kept alive until invalidate() so hash table metadata
     * (buildTypes/buildHashCols referenced by variants) remains valid.
     */
    void publish(const std::string& buildHashTableId, HashTableVariants* variants,
        HashBuilderOperatorFactory* pinningFactory);

    /**
     * Block until the hash table for `buildHashTableId` is ready, then return it.
     * Returns nullptr if the id has never been registered (non-BHJ path or invalidated
     * before this call). The returned pointer is owned by the cache; do NOT delete it.
     */
    HashTableVariants* waitAndGet(const std::string& buildHashTableId);

    /**
     * Called via JNI when Spark unpersists the broadcast relation.
     * Frees the hash table memory and removes the cache entry.
     */
    void invalidate(const std::string& buildHashTableId);

    // Visible for testing.
    size_t size();

private:
    BroadcastHashTableCache() = default;

    enum class EntryState { IN_PROGRESS, READY };

    struct CacheEntry {
        EntryState state = EntryState::IN_PROGRESS;
        HashTableVariants* variants = nullptr; // owned by cache once published
        std::unique_ptr<HashBuilderOperatorFactory> pinningFactory;
    };

    std::mutex mutex_;
    std::condition_variable cv_;
    std::unordered_map<std::string, CacheEntry> cache_;
};

} // namespace op
} // namespace omniruntime
