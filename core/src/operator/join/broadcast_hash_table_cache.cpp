/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * @Description: Executor-level BHJ hash table cache implementation.
 */
#include "broadcast_hash_table_cache.h"

#include <iostream>
#include <stdexcept>

#include "hash_builder.h"

namespace omniruntime {
namespace op {

BroadcastHashTableCache& BroadcastHashTableCache::getInstance()
{
    static BroadcastHashTableCache instance;
    return instance;
}

bool BroadcastHashTableCache::tryClaimBuild(const std::string& buildHashTableId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = cache_.find(buildHashTableId);
    if (it != cache_.end()) {
        // Already in progress or ready — another task claimed the build.
        return false;
    }
    // First arrival: insert an IN_PROGRESS entry and claim the build.
    cache_.emplace(buildHashTableId, CacheEntry{EntryState::IN_PROGRESS, nullptr});
    return true;
}

void BroadcastHashTableCache::publish(const std::string& buildHashTableId, HashTableVariants* variants,
    HashBuilderOperatorFactory* pinningFactory)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = cache_.find(buildHashTableId);
        if (it == cache_.end()) {
            // Entry was invalidated before publish (rare race). Free variants and return.
            delete variants;
            delete pinningFactory;
            return;
        }
        it->second.variants = variants;
        it->second.pinningFactory.reset(pinningFactory);
        it->second.state = EntryState::READY;
    }
    cv_.notify_all();
}

HashTableVariants* BroadcastHashTableCache::waitAndGet(const std::string& buildHashTableId)
{
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [&] {
        auto it = cache_.find(buildHashTableId);
        // Stop waiting if: entry is gone (invalidated) or entry is READY.
        return it == cache_.end() || it->second.state == EntryState::READY;
    });
    auto it = cache_.find(buildHashTableId);
    if (it == cache_.end()) {
        return nullptr;
    }
    return it->second.variants;
}

void BroadcastHashTableCache::invalidate(const std::string& buildHashTableId)
{
    HashTableVariants* toDelete = nullptr;
    std::unique_ptr<HashBuilderOperatorFactory> toDeleteFactory;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = cache_.find(buildHashTableId);
        if (it != cache_.end()) {
            toDelete = it->second.variants; // may be nullptr if still IN_PROGRESS
            toDeleteFactory = std::move(it->second.pinningFactory);
            cache_.erase(it);
        }
    }
    // Notify waiters so they can observe the entry is gone.
    cv_.notify_all();
    // Delete outside the lock to avoid potential re-entry issues.
    delete toDelete;
}

size_t BroadcastHashTableCache::size()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return cache_.size();
}

} // namespace op
} // namespace omniruntime
