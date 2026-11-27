/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
 */

#ifndef OMNI_RUNTIME_ALLOCATOR_H
#define OMNI_RUNTIME_ALLOCATOR_H

#include <jemalloc/jemalloc.h>
// Compat shim: standard headers formerly supplied transitively to downstream files via
// the now-removed memory/memory_trace.h umbrella (it included <atomic>/<mutex>/<unordered_set>/
// <unordered_map> and, via thread_memory_trace.h, <iostream> + its closure). Kept here — a
// broad transitive provider — to preserve the include contract without a sweeping per-file
// IWYU change. REVERTIBLE; candidate for include-what-you-use cleanup in a dedicated pass.
#include <atomic>
#include <mutex>
#include <unordered_set>
#include <unordered_map>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include "memory_pool.h"
#include "omni_task_memory_pool.h"

// Debug-only memory instrumentation (OMNI_MEM_DEBUG). mem_probe.h records both the
// pool-bound (covered) and pool-less (uncovered) allocation paths in a lock-free,
// return-address-keyed table (bytes and count per side) and dumps it once at process
// exit, so the covered/(covered+uncovered) ratio can be read directly without
// per-allocation logging. Release builds carry zero overhead and no symbols.
#ifdef OMNI_MEM_DEBUG
#include "mem_probe.h"
#endif

namespace omniruntime::mem {
class Allocator {
public:
    // Pool-bound constructor — mirrors the Velox Buffer::pool_ const member invariant
    // (Buffer.h:254-265). The pool pointer is non-owning, set once at construction,
    // never reassigned. Alloc/Free route directly to taskPool_ with no TLS lookup.
    //
    // taskPool_ is non-owning; pool lifetime must outlive every buffer bound to this
    // allocator (pool lifetime == Spark task lifetime).
    explicit Allocator(omniruntime::mem::OmniTaskMemoryPool *taskPool) : taskPool_(taskPool) {}
    // Public dtor: pool-bound Allocator instances are stack-allocated by callers (tests, operators).
    ~Allocator() = default;

    // Pool accessor — returns the pool this Allocator is bound to.
    omniruntime::mem::OmniTaskMemoryPool *getTaskPool() const { return taskPool_; }

    // The legacy bridge singleton is RETIRED. Marking the static accessor `= delete`
    // produces a clean compile-time error at every remaining call site. All related
    // deletes (this accessor, the private default ctor, and the bridge ctors in
    // SimpleArenaAllocator) were shipped in one atomic change-set so dependent
    // callers fail at compile time rather than link time.
    static Allocator *GetAllocator() = delete;

    ALWAYS_INLINE void *Alloc(int64_t size, bool zeroFill = false)
    {
        // Pool-bound path (Velox const pool_ invariant). No TLS lookup.
        // taskPool_==nullptr is the legacy-bridge unaccounted path; under OMNI_MEM_DEBUG
        // the null-scope trap fires and prints the caller's return address for triage.
        if (taskPool_ != nullptr) {
            taskPool_->AddMemory(size);
        }
        uint8_t *data = nullptr;
        try {
            pool->Allocate(size, &data, zeroFill);
        } catch (...) {
            // AddMemory already committed. Its listener rollback cannot cover
            // a later physical allocation failure. Preserve existing charges
            // and reservation headroom by undoing only this allocation.
            if (taskPool_ != nullptr) {
                taskPool_->SubMemory(size);
            }
            throw;
        }
#ifdef OMNI_MEM_DEBUG
        // Record only successful physical allocations.
        if (taskPool_ != nullptr) {
            omniruntime::mem::probe::OnAlloc(/*covered=*/true, size);
        } else {
            omniruntime::mem::probe::OnAlloc(/*covered=*/false, size);
        }
#endif
        return data;
    }

    ALWAYS_INLINE void *Alloc(int64_t size, const OmniTaskMemoryPool::AllocationAccountingScope &scope, bool zeroFill = false)
    {
        // Pool-bound path (Velox const pool_ invariant). No TLS lookup.
        // taskPool_==nullptr is the legacy-bridge unaccounted path; under OMNI_MEM_DEBUG
        // the null-scope trap fires and prints the caller's return address for triage.
        if (taskPool_ == nullptr) {
            throw std::logic_error("Allocator: accounting scope requires its bound pool");
        }
        taskPool_->AddMemoryWithScope(size, scope);
        uint8_t *data = nullptr;
        try {
            pool->Allocate(size, &data, zeroFill);
        } catch (...) {
            // AddMemory already committed. Its listener rollback cannot cover
            // a later physical allocation failure. Preserve existing charges
            // and reservation headroom by undoing only this allocation.
            if (taskPool_ != nullptr) {
                taskPool_->SubMemory(size);
            }
            throw;
        }
#ifdef OMNI_MEM_DEBUG
        // Record only successful physical allocations.
        if (taskPool_ != nullptr) {
            omniruntime::mem::probe::OnAlloc(/*covered=*/true, size);
        } else {
            omniruntime::mem::probe::OnAlloc(/*covered=*/false, size);
        }
#endif
        return data;
    }

    ALWAYS_INLINE void Free(void *data, int64_t size)
    {
        // Symmetric with Alloc: pool-bound or no-op (no TLS routing exists).
        if (taskPool_ != nullptr) {
            taskPool_->SubMemory(size);
#ifdef OMNI_MEM_DEBUG
            omniruntime::mem::probe::OnFree(/*covered=*/true, size);
#endif
        }
#ifdef OMNI_MEM_DEBUG
        else {
            omniruntime::mem::probe::OnFree(/*covered=*/false, size);
        }
#endif
        pool->Release(reinterpret_cast<uint8_t *>(data));
    }

private:
    // No implicit no-arg construction. The deleted GetAllocator() singleton was the
    // only legitimate caller of this ctor; both are retired together so the compiler
    // enforces non-null pool at every construction site (Velox Buffer::pool_
    // const-member invariant).
    Allocator() = delete;

    // Non-owning pool pointer (Velox const pool_ invariant): set at ctor, never reassigned.
    omniruntime::mem::OmniTaskMemoryPool *taskPool_;
    MemoryPool *pool = GetMemoryPool();
};
} // omniruntime::mem

#endif // OMNI_RUNTIME_ALLOCATOR_H
