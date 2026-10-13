/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
 */
#include <string>
#include <atomic>
// Compat shim: standard headers formerly supplied transitively to downstream files via
// the now-removed memory/memory_trace.h umbrella. Kept here to preserve the include
// contract without a sweeping per-file IWYU change. REVERTIBLE.
#include <mutex>
#include <unordered_set>
#include <unordered_map>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include "util/compiler_util.h"
#include "memory/omni_task_memory_pool.h"

#ifndef OMNI_RUNTIME_STRING_UTILS_H
#define OMNI_RUNTIME_STRING_UTILS_H
namespace omniruntime::vec {
static constexpr int32_t INITIAL_STRING_SIZE = 1 << 15; // 32K

class LargeStringBuffer {
public:
    // Pool-bound ctor. pool_ is set at construction;
    // ~LargeStringBuffer() always charges SubMemory to this exact pool.
    //
    // Storage remains std::vector (system allocator);
    // pool_ tracks accounting bytes only. ctor/dtor are symmetric on the same
    // pool_ so no underflow occurs.
    //
    // TODO: migrate std::vector data storage to AlignedBuffer for full pool routing.
    LargeStringBuffer(size_t size, omniruntime::mem::OmniTaskMemoryPool *pool) : pool_(pool)
    {
        // Empty strings also need to allocate 1 byte of memory to deal with the null pointer exception.
        capacity = size == 0 ? 1 : size;
        data.reserve(capacity);
        int64_t bufferCapacity = sizeof(LargeStringBuffer) + capacity;
        pool_->AddMemory(bufferCapacity);   // direct — no TLS
    }

    // No-pool bridge: LargeStringContainer (large_string_container.cpp:15, :105) still
    // constructs without a pool. Do not delete until those callers are pool-threaded.
    [[deprecated("no-pool bridge; large_string_container.cpp:15,105 callers still need pool-threading")]]
    explicit LargeStringBuffer(size_t size) : pool_(nullptr)
    {
        capacity = size == 0 ? 1 : size;
        data.reserve(capacity);
        int64_t bufferCapacity = sizeof(LargeStringBuffer) + capacity;
    }

    ~LargeStringBuffer()
    {
        int64_t bufferCapacity = sizeof(LargeStringBuffer) + capacity;
        if (pool_ != nullptr) {
            // Pool-bound path: routes SubMemory to the construction-time pool.
            pool_->SubMemory(bufferCapacity);
        }
        // pool_==nullptr: explicit no-pool sentinel (tests pass nullptr explicitly to primary ctor).
    }

    ALWAYS_INLINE uint64_t Capacity()
    {
        return capacity;
    }

    ALWAYS_INLINE uint64_t Size()
    {
        return usedSize;
    }

    ALWAYS_INLINE void SetSize(uint64_t size)
    {
        usedSize = size;
    }

    ALWAYS_INLINE char *Data()
    {
        return data.data();
    };

    // Pool this buffer is bound to (nullptr for no-pool bridge instances). Lets
    // LargeStringContainer recover the pool on growth WITHOUT storing its own member
    // (keeps LargeStringContainer's layout ABI-stable across .so boundaries).
    ALWAYS_INLINE omniruntime::mem::OmniTaskMemoryPool *GetPool()
    {
        return pool_;
    }

private:
    // Pool-bound memory tracker. Non-owning; set at ctor, never reassigned.
    // nullptr for no-pool bridge instances.
    omniruntime::mem::OmniTaskMemoryPool *pool_;
    std::vector<char> data;
    uint64_t usedSize = 0;
    uint64_t capacity = 0;
    std::atomic<int32_t> referenceCount;
};
}

#endif // OMNI_RUNTIME_STRING_UTILS_H
