/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 */


#ifndef OMNI_RUNTIME_ALIGNED_BUFFER_H
#define OMNI_RUNTIME_ALIGNED_BUFFER_H
#include "allocator.h"

namespace omniruntime::mem {
template <typename RAW_DATA_TYPE> class AlignedBuffer {
public:
    // The legacy no-allocator bridge ctor has been retired. Every call site must
    // supply an explicit pool-bound Allocator* (compiler enforces).

    // Pool-bound primary constructor. Mirrors Velox AlignedBuffer ctor at
    // Buffer.h:485: pool passed explicitly, never via global lookup or TLS.
    // The supplied allocator must be pool-bound (constructed with an
    // OmniTaskMemoryPool*); it is stored as-is and used by AllocateReuse() and
    // ~AlignedBuffer() for all subsequent alloc/free operations — routing them
    // to the construction-time pool.
    AlignedBuffer(size_t size, Allocator *allocatorParam, bool zerofill = false)
    {
        capacity = size * sizeof(RAW_DATA_TYPE);
        allocator = allocatorParam;   // pool-bound; never calls GetAllocator()
        buffer = reinterpret_cast<RAW_DATA_TYPE *>(allocator->Alloc(capacity, zerofill));
    }

    AlignedBuffer(size_t size, Allocator *allocatorParam, bool zerofill, const OmniTaskMemoryPool::AllocationAccountingScope &scope)
    {
        capacity = size * sizeof(RAW_DATA_TYPE);
        allocator = allocatorParam;   // pool-bound; never calls GetAllocator()
        buffer = reinterpret_cast<RAW_DATA_TYPE *>(allocator->Alloc(capacity, scope, zerofill));
    }

    // No-pool bridge retained to support leaf data-structure ctors (ArrayVector,
    // MapVector, RowVector, NullsBuffer, aggregator nullMapBuffer) which construct
    // AlignedBuffer without threading a pool. ~50 call sites across vectorization
    // functions, codegen, and the ORC reader still need migrating to pool-bound
    // forms; until then this bridge skips per-task accounting, and OMNI_MEM_DEBUG
    // builds record each such allocation as uncovered.
    [[deprecated("no-pool bridge; leaf data-structure ctors (ArrayVector/MapVector/RowVector/NullsBuffer) still need pool-threading")]]
    explicit AlignedBuffer(size_t size, bool zerofill = false)
    {
        static Allocator nullBridge{nullptr};
        capacity = size * sizeof(RAW_DATA_TYPE);
        allocator = &nullBridge;
        buffer = reinterpret_cast<RAW_DATA_TYPE *>(allocator->Alloc(capacity, zerofill));
    }

    [[deprecated("no-pool bridge; typed_mask_column_assistant_aggregator still needs pool-threading")]]
    AlignedBuffer()
    {
        static Allocator nullBridge{nullptr};
        buffer = nullptr;
        capacity = 0;
        allocator = &nullBridge;
    }

    ~AlignedBuffer()
    {
        Release();
    }

    ALWAYS_INLINE RAW_DATA_TYPE *AllocateReuse(size_t size, bool zerofill)
    {
        if (buffer == nullptr) {
            // no memory allocated, creating new one
            capacity = size * sizeof(RAW_DATA_TYPE);
            buffer = reinterpret_cast<RAW_DATA_TYPE *>(allocator->Alloc(capacity, zerofill));
            return buffer;
        }

        size_t newCapacity = size * sizeof(RAW_DATA_TYPE);
        if (capacity < newCapacity) {
            // memory already allocated, but cannot hold newCapacity
            // releasing previous buffer and allocating new one. Null out buffer
            // before Alloc so that if Alloc throws (e.g. listener cap-deny rethrows),
            // ~AlignedBuffer()/Release() does not Free the already-freed pointer.
            allocator->Free(buffer, capacity);
            buffer = nullptr;
            capacity = newCapacity;
            buffer = reinterpret_cast<RAW_DATA_TYPE *>(allocator->Alloc(capacity, zerofill));
            return buffer;
        }

        // memory already allocated, and can hold newCapacity
        // just set content to zero (if needed) and return previous buffer
        if (zerofill) {
            memset(buffer, 0, newCapacity);
        }
        return buffer;
    }

    ALWAYS_INLINE RAW_DATA_TYPE *GetBuffer()
    {
        return buffer;
    }

    ALWAYS_INLINE RAW_DATA_TYPE GetValue(int32_t index)
    {
        return buffer[index];
    }

private:
    ALWAYS_INLINE void Release()
    {
        if (buffer != nullptr) {
            allocator->Free(buffer, capacity);
        }
        buffer = nullptr;
        capacity = 0;
    }

    RAW_DATA_TYPE *buffer;
    size_t capacity;
    Allocator *allocator;
};
}


#endif // OMNI_RUNTIME_ALIGNED_BUFFER_H
