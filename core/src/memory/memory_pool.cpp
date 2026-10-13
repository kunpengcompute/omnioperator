/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2021. All rights reserved.
 */

#include "memory_pool.h"

#include <iostream>
#include <jemalloc/jemalloc.h>
#include "util/omni_exception.h"
#include "util/compiler_util.h"

using namespace std;
namespace omniruntime {
namespace mem {
class SimpleAllocator {
public:
    static void Allocate(int64_t size, uint8_t **buffer, bool zeroFill = false)
    {
        // background: If size is 0, then malloc() returns either NULL, or a unique pointer value that can later be
        // successfully passed to free().
        if (size < 0) {
            throw omniruntime::exception::OmniException("OPERATOR_RUNTIME_ERROR", "allocate size is negative.");
        }

        if (zeroFill) {
            // alloc based on the size
            *buffer = static_cast<uint8_t *>(calloc(1, static_cast<size_t>(size)));
        } else {
            // alloc based on the size
            *buffer = static_cast<uint8_t *>(malloc(static_cast<size_t>(size)));
        }
        if (UNLIKELY(*buffer == nullptr)) {
            throw omniruntime::exception::OmniException("OPERATOR_RUNTIME_ERROR", "allocate fails.");
        }
    }

    static void Release(uint8_t *buffer)
    {
        // free the memory
        free(static_cast<void *>(buffer));
    }
};

class JemallocAllocator {
public:
    static void Allocate(int64_t size, uint8_t **buffer, bool zeroFill = false)
    {
        if (size < 0) {
            throw omniruntime::exception::OmniException("OPERATOR_RUNTIME_ERROR", "allocate size is negative.");
        }
        // jemalloc alloc
        if (zeroFill) {
            *buffer = static_cast<uint8_t *>(mallocx(
                static_cast<size_t>(size),
                MALLOCX_ALIGN(alignment) | MALLOCX_ZERO
            ));
        } else {
            *buffer = static_cast<uint8_t *>(mallocx(static_cast<size_t>(size), MALLOCX_ALIGN(alignment)));
        }
        if (UNLIKELY(*buffer == nullptr)) {
            throw omniruntime::exception::OmniException("OPERATOR_RUNTIME_ERROR", "allocate fails.");
        }
    }

    static void Release(uint8_t *buffer)
    {
        // jemalloc free
        dallocx(static_cast<void *>(buffer), MALLOCX_ALIGN(alignment));
    }
    const static size_t alignment = 64;
};

template <typename Allocator> class BaseMemoryPoolImpl : public MemoryPool {
public:
    void Allocate(int64_t size, uint8_t **buffer, bool zeroFill = false) override
    {
        Allocator::Allocate(size, buffer, zeroFill);
    }

    void Release(uint8_t *buffer) override
    {
        Allocator::Release(buffer);
    }

    ~BaseMemoryPoolImpl() override = default;

    uint64_t GetPreferredSize(uint64_t size) override
    {
        return size;
    }
};

class SimpleMemoryPool : public BaseMemoryPoolImpl<SimpleAllocator> {
public:
    uint64_t GetPreferredSize(uint64_t size) override
    {
        if (size == 0) {
            return size;
        }

        const uint64_t smallSize = 8;
        if (size < smallSize) {
            return smallSize;
        }
        uint32_t bits = 63 - __builtin_clzll(size);
        size_t lower = 1ULL << bits;
        // Size is a power of 2.
        if (lower == size) {
            return size;
        }
        // If size is below 1.5 * previous power of two, return 1.5 *
        // the previous power of two, else the next power of 2.
        uint64_t preferredSize = lower + (lower / 2);
        if (preferredSize >= size) {
            return preferredSize;
        }
        return (lower + lower);
    }
};

class JemallocMemoryPool : public BaseMemoryPoolImpl<JemallocAllocator> {
public:
    uint64_t GetPreferredSize(uint64_t size) override
    {
        if (size == 0) {
            return size;
        }

        const uint64_t smallSize = 8;
        if (size < smallSize) {
            return smallSize;
        }

        uint32_t bits = 63 - __builtin_clzll(size);
        size_t lower = 1ULL << bits;
        // Size is a power of 2.
        if (lower == size) {
            return size;
        }
        // If size is below 1.5 * previous power of two, return 1.5 *
        // the previous power of two, else the next power of 2.
        uint64_t preferredSize = lower + (lower / 2);
        if (preferredSize >= size) {
            return preferredSize;
        }
        return (lower + lower);
    }
};

#ifdef COVERAGE
        static omniruntime::mem::JemallocMemoryPool g_memoryPoolInstance;
#else
        static omniruntime::mem::JemallocMemoryPool g_memoryPoolInstance;
#endif

MemoryPool *GetMemoryPool()
{
    return &g_memoryPoolInstance;
}
}
}
