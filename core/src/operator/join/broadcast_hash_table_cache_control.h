/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Spark/backend-facing control surface for executor-local broadcast tables.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace omniruntime {
namespace mem {
class OmniTaskMemoryPool;
}
namespace op {

// Core Omni does not depend on the Spark or JNI listener implementation.
class BroadcastEntryMemory {
public:
    virtual ~BroadcastEntryMemory() = default;
    virtual mem::OmniTaskMemoryPool* pool() const = 0;
};

using BroadcastEntryMemoryFactory = std::function<std::shared_ptr<BroadcastEntryMemory>()>;

struct BroadcastCacheStats {
    size_t indexedEntries;
    uint64_t builds;
    uint64_t hits;
    uint64_t retirements;
};

void ConfigureBroadcastCacheMemoryFactory(BroadcastEntryMemoryFactory factory);
void InvalidateBroadcastCachePrefix(const std::string& exchangeKey);
void RetireAllBroadcastCacheEntries();
BroadcastCacheStats GetBroadcastCacheStats();

} // namespace op
} // namespace omniruntime
