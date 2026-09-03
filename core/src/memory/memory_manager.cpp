/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 */
#include "memory_manager.h"

#include <cstdlib>
#include <cstring>

namespace omniruntime::mem {

namespace {
constexpr const char *kDisableMemCapExceededEnv = "OMNI_DISABLE_MEM_CAP_EXCEEDED";

bool DisableMemCapExceeded()
{
    const char *value = std::getenv(kDisableMemCapExceededEnv);
    return value == nullptr || std::strcmp(value, "0") != 0;
}
} // namespace

// constructor for globalMemoryManager
MemoryManager::MemoryManager()
{
    parent.store(nullptr, std::memory_order_relaxed);
    memoryAmount.store(0, std::memory_order_relaxed);
    memoryLimit.store(UNLIMIT, std::memory_order_relaxed);
    memoryPeak.store(0, std::memory_order_relaxed);
    isBlocked.store(false, std::memory_order_relaxed);
}

// constructor for threadMemoryManager
MemoryManager::MemoryManager(MemoryManager *parentMemoryManager)
{
    // The capacity of each thread can also be limited. It needs to be set the corresponding value if necessary.
    int64_t globalMemoryLimit = parentMemoryManager->GetMemoryLimit();
    parent.store(parentMemoryManager, std::memory_order_relaxed);
    memoryAmount.store(0, std::memory_order_relaxed);
    memoryLimit.store(globalMemoryLimit, std::memory_order_relaxed);
    memoryPeak.store(0, std::memory_order_relaxed);
    isBlocked.store(false, std::memory_order_relaxed);
}

MemoryManager::~MemoryManager()
{}

void MemoryManager::AddMemory(int64_t reportedMemory, int64_t curAllocateSize)
{
    int64_t newMemoryAmount = memoryAmount.fetch_add(reportedMemory, std::memory_order_relaxed) + reportedMemory;
    int64_t limit = memoryLimit.load(std::memory_order_relaxed);
    if (limit == UNLIMIT || newMemoryAmount < limit) {
        isBlocked.store(false, std::memory_order_relaxed);
    } else {
        /* *
         * A thread cannot throw multiple exceptions in the case of multiple threads.
         * The If statement is used to avoid the following case: When the thread exceeds the limit,
         * the other thread is called and the AddMemory interface is executed again,
         * which may cause the OmniException to be thrown again.
         * Note: Only global memory manager can throw the MEM_CAP_EXCEEDED exception.
        *  */
        if (!isBlocked.load(std::memory_order_relaxed)) {
            isBlocked.store(true, std::memory_order_relaxed);

            if (parent.load(std::memory_order_relaxed) == nullptr) {
                auto message =
                        op::GetErrorMessage(op::ErrorCode::MEM_CAP_EXCEEDED) + std::to_string(limit / 1024 / 1024)
                        + "; current Memory Usage Total: " + std::to_string(newMemoryAmount / 1024 / 1024)
                        + "MB; current Memory Allocate Size: " + std::to_string(curAllocateSize) + "B";
                if (DisableMemCapExceeded()) {
                    LogWarn("%s; exception disabled by %s", message.c_str(), kDisableMemCapExceededEnv);
                } else {
                    memoryAmount.fetch_sub(curAllocateSize, std::memory_order_relaxed);
                    throw OmniException(GetErrorCode(op::ErrorCode::MEM_CAP_EXCEEDED), message);
                }
            } else {
                memoryAmount.fetch_sub(curAllocateSize, std::memory_order_relaxed);
            }
        }
    }

    if (auto parentMemoryManager = parent.load(std::memory_order_relaxed)) {
        parentMemoryManager->AddMemory(reportedMemory, curAllocateSize);
    }
}

void MemoryManager::SubMemory(int64_t reclaimedMemory)
{
    memoryAmount.fetch_add(reclaimedMemory, std::memory_order_relaxed);
    if (auto parentMemoryManager = parent.load(std::memory_order_relaxed)) {
        parentMemoryManager->SubMemory(reclaimedMemory);
    }
}

void MemoryManager::UpdatePeak(int64_t size)
{
    int64_t peak = memoryPeak.load(std::memory_order_relaxed);
    if (size > peak) {
        memoryPeak.store(size, std::memory_order_relaxed);
    }
}

#ifdef DEBUG
void MemoryManager::AddScopeAmount(const std::string &scope, int64_t size)
{
    // keep thread safety.
    std::lock_guard<std::mutex> lock(mapLock);
    scopeMap[scope] += size;
    if (auto parentMemoryManager = parent.load(std::memory_order_relaxed)) {
        parentMemoryManager->scopeMap[scope] += size;
    }
}

void MemoryManager::SubScopeAmount(const std::string &scope, int64_t size)
{
    // keep thread safety.
    std::lock_guard<std::mutex> lock(mapLock);
    scopeMap[scope] -= size;
    if (auto parentMemoryManager = parent.load(std::memory_order_relaxed)) {
        parentMemoryManager->scopeMap[scope] -= size;
    }
}
#endif

void MemoryManager::SetParent(MemoryManager *parentMemoryManager)
{
    parent.store(parentMemoryManager, std::memory_order_relaxed);
}

MemoryManager *MemoryManager::GetParent()
{
    return parent.load(std::memory_order_relaxed);
}

void MemoryManager::SetMemoryAmount(int64_t amount)
{
    memoryAmount.store(amount, std::memory_order_relaxed);
}

int64_t MemoryManager::GetMemoryAmount()
{
    return memoryAmount.load(std::memory_order_relaxed);
}

void MemoryManager::SetMemoryLimit(int64_t limit)
{
    memoryLimit.store(limit, std::memory_order_relaxed);
}

int64_t MemoryManager::GetMemoryLimit()
{
    return memoryLimit.load(std::memory_order_relaxed);
}

void MemoryManager::SetMemoryPeak(int64_t peak)
{
    memoryPeak.store(peak, std::memory_order_relaxed);
}

int64_t MemoryManager::GetMemoryPeak()
{
    return memoryPeak.load(std::memory_order_relaxed);
}

bool MemoryManager::IsMemoryManagerBlocked()
{
    return isBlocked.load(std::memory_order_relaxed);
}

#ifdef DEBUG
std::unordered_map<std::string, int64_t, std::hash<std::string>, std::equal_to<std::string>,
    MemoryManagerAllocator<std::pair<std::string, int64_t>>>
MemoryManager::GetScopeMap()
{
    return scopeMap;
}
#endif

void MemoryManager::Clear()
{
    memoryAmount.store(0, std::memory_order_relaxed);
    memoryLimit.store(UNLIMIT, std::memory_order_relaxed);
    memoryPeak.store(0, std::memory_order_relaxed);
    isBlocked.store(false, std::memory_order_relaxed);
#ifdef DEBUG
    // keep thread safety.
    std::lock_guard<std::mutex> lock(mapLock);
    if (auto parentMemoryManager = parent.load(std::memory_order_relaxed)) {
        for (const auto &it : scopeMap) {
            parentMemoryManager->scopeMap.erase(it.first);
        }
    }
    scopeMap.clear();
#endif
}
}
