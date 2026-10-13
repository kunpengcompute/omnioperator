/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 */
#include "broadcast_hash_table_cache.h"

#include <stdexcept>
#include <utility>
#include <vector>

#include "hash_builder.h"
#include "memory/omni_task_memory_pool.h"

namespace omniruntime {
namespace op {

BroadcastHashTableCache::Payload::~Payload() = default;

HashTableVariants* BroadcastHashTableCache::Lease::get() const
{
    return payload_ == nullptr ? nullptr : payload_->variants.get();
}

BroadcastHashTableCache::BuildTicket::~BuildTicket()
{
    fail();
}

BroadcastHashTableCache::Lease BroadcastHashTableCache::BuildTicket::prepare()
{
    if (!active_) {
        throw std::logic_error("Broadcast build ticket has already completed");
    }
    return Lease(std::make_shared<Payload>());
}

mem::OmniTaskMemoryPool* BroadcastHashTableCache::BuildTicket::pool()
{
    if (!active_) {
        throw std::logic_error("Completed broadcast build ticket has no mutable pool");
    }
    if (entry_->memory == nullptr) {
        MemoryFactory factory;
        {
            std::lock_guard<std::mutex> lock(cache_->mutex_);
            if (entry_->state != EntryState::BUILDING || cache_->draining_) {
                throw std::logic_error("Retired broadcast build cannot create entry memory");
            }
            factory = entry_->memoryFactory;
        }
        if (!factory) {
            throw std::logic_error("Broadcast cache memory factory is not configured");
        }
        entry_->memory = factory();
        if (entry_->memory == nullptr || entry_->memory->pool() == nullptr) {
            throw std::logic_error("Broadcast cache memory factory returned no pool");
        }
    }
    return entry_->memory->pool();
}

void BroadcastHashTableCache::BuildTicket::publish(
    Lease& prepared, HashTableVariants* variants, HashBuilderOperatorFactory* pinningFactory)
{
    if (!active_ || !prepared || prepared.get() != nullptr) {
        throw std::logic_error("Invalid broadcast cache publish");
    }
    prepared.payload_->variants.reset(variants);
    prepared.payload_->pinningFactory.reset(pinningFactory);
    prepared.payload_->memory = std::move(entry_->memory);
    active_ = false;
    cache_->publish(entry_, prepared);
}

void BroadcastHashTableCache::BuildTicket::fail()
{
    if (active_) {
        active_ = false;
        cache_->fail(entry_);
    }
}

BroadcastHashTableCache& BroadcastHashTableCache::getInstance()
{
    static BroadcastHashTableCache cache;
    return cache;
}

void BroadcastHashTableCache::configureMemoryFactory(MemoryFactory factory)
{
    if (!factory) {
        throw std::invalid_argument("Broadcast cache requires an entry memory factory");
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (memoryFactory_ || !entries_.empty()) {
        throw std::logic_error("Broadcast cache memory factory already configured or cache active");
    }
    memoryFactory_ = std::move(factory);
    // A prior SparkContext may have drained this process-local cache. Old
    // leases still own their retired payloads; they cannot republish because
    // their entry state is RETIRED. A new executor incarnation may now admit.
    draining_ = false;
}

BroadcastHashTableCache::Acquisition BroadcastHashTableCache::acquire(const std::string& id)
{
    if (id.empty()) {
        return {};
    }
    std::unique_lock<std::mutex> lock(mutex_);
    Acquisition result;
    bool resolved = false;
    while (!resolved) {
        const auto signature = id.find('#');
        const std::string exchangeKey = id.substr(0, signature);
        if (draining_ || !memoryFactory_ || retiredExchangeKeys_.count(exchangeKey) != 0) {
            result.retired = true;
            resolved = true;
            continue;
        }
        auto it = entries_.find(id);
        if (it == entries_.end()) {
            auto entry = std::make_shared<Entry>(id, memoryFactory_);
            auto ticket = std::shared_ptr<BuildTicket>(new BuildTicket(this, entry));
            entries_.emplace(id, std::move(entry));
            ++builds_;
            result.builder = std::move(ticket);
            resolved = true;
            continue;
        }
        auto entry = it->second;
        if (entry->state == EntryState::READY) {
            ++hits_;
            result.reader = Lease(entry->payload);
            resolved = true;
            continue;
        }
        if (entry->state == EntryState::FAILED || entry->state == EntryState::RETIRED) {
            entries_.erase(it);
            continue;
        }
        cv_.wait(lock, [&] { return draining_ || entry->state != EntryState::BUILDING; });
    }
    return result;
}

void BroadcastHashTableCache::publish(const std::shared_ptr<Entry>& entry, const Lease& prepared)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (entry->state == EntryState::BUILDING && !draining_) {
            entry->payload = prepared.payload_;
            entry->state = EntryState::READY;
        }
    }
    cv_.notify_all();
}

void BroadcastHashTableCache::fail(const std::shared_ptr<Entry>& entry)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (entry->state == EntryState::BUILDING) {
            entry->state = EntryState::FAILED;
            auto it = entries_.find(entry->id);
            if (it != entries_.end() && it->second == entry) {
                entries_.erase(it);
            }
        }
    }
    cv_.notify_all();
}

void BroadcastHashTableCache::invalidate(const std::string& id)
{
    std::shared_ptr<Entry> retired;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = entries_.find(id);
        if (it != entries_.end()) {
            retired = std::move(it->second);
            entries_.erase(it);
            retired->state = EntryState::RETIRED;
            ++retirements_;
        }
    }
    cv_.notify_all();
    // Retired payload is released outside the map lock.
}

void BroadcastHashTableCache::invalidatePrefix(const std::string& key)
{
    if (key.empty()) {
        return;
    }
    const std::string exchangeKey = key.substr(0, key.find('#'));
    if (exchangeKey.empty()) {
        return;
    }
    const std::string prefix = exchangeKey + "#";
    std::vector<std::shared_ptr<Entry>> retired;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        // Retirement is a tombstone, not just an index removal. An already
        // admitted task may reach native planning after the driver ACKs SQL
        // cleanup, and must not create an unowned entry at that point.
        retiredExchangeKeys_.insert(exchangeKey);
        for (auto it = entries_.begin(); it != entries_.end();) {
            if (it->first == exchangeKey || it->first.compare(0, prefix.size(), prefix) == 0) {
                retired.push_back(std::move(it->second));
                it = entries_.erase(it);
                retired.back()->state = EntryState::RETIRED;
                ++retirements_;
            } else {
                ++it;
            }
        }
    }
    cv_.notify_all();
}

void BroadcastHashTableCache::retireAll()
{
    std::unordered_map<std::string, std::shared_ptr<Entry>> retired;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        draining_ = true;
        memoryFactory_ = nullptr;
        retired.swap(entries_);
        for (auto& item : retired) {
            item.second->state = EntryState::RETIRED;
        }
        retirements_ += retired.size();
    }
    cv_.notify_all();
}

size_t BroadcastHashTableCache::size() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return entries_.size();
}

BroadcastHashTableCache::Stats BroadcastHashTableCache::stats() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return {entries_.size(), builds_, hits_, retirements_};
}

void ConfigureBroadcastCacheMemoryFactory(BroadcastEntryMemoryFactory factory)
{
    BroadcastHashTableCache::getInstance().configureMemoryFactory(std::move(factory));
}

void InvalidateBroadcastCachePrefix(const std::string& exchangeKey)
{
    BroadcastHashTableCache::getInstance().invalidatePrefix(exchangeKey);
}

void RetireAllBroadcastCacheEntries()
{
    BroadcastHashTableCache::getInstance().retireAll();
}

BroadcastCacheStats GetBroadcastCacheStats()
{
    return BroadcastHashTableCache::getInstance().stats();
}

} // namespace op
} // namespace omniruntime
