/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 *
 * OmniTaskMemoryPool — per-task byte accounting + listener publication channel.
 *
 * Core invariants:
 *   - Pool-local atomic counters support lock-free observers; billing writers
 *     are serialized by accountingMutex_.
 *   - AddMemory has two-phase commit shape with exact rollback.
 *   - SubMemory calls listener->allocationChanged(-bytes) symmetrically.
 *   - peakBytes_ writers are serialized by accountingMutex_; atomic load/store
 *     keeps observers lock-free without a locked RMW on allocation.
 *   - dtor logs residual bytes; the real drain is wired via the listener path.
 *
 * gluten::SparkAllocationListener is plugged into the try/catch on construction.
 * isBlocked_ provides binary cap-bypass under arbitration.
 * driver_ back-pointer enables self-suspension.
 */

#ifndef OMNI_RUNTIME_OMNI_TASK_MEMORY_POOL_H
#define OMNI_RUNTIME_OMNI_TASK_MEMORY_POOL_H

#include <functional>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

#include "reservation_denied.h"
#include "util/compiler_util.h"
#include "util/omni_exception.h"   // brings `omniruntime::exception::OmniException` into scope
                                   // so the `using namespace exception;` alias below resolves
                                   // and unqualified `OmniException(...)` call sites in
                                   // vector.h/chunk.h still compile.

namespace omniruntime { namespace mem { class PoolListener; } }

namespace omniruntime { namespace op { class SpillableOperator; } }

namespace omniruntime {
namespace mem {
// The `using namespace exception;` alias was historically in
// `omni_task_memory_scope.h`. Many call sites (vector.h, chunk.h) use
// unqualified `OmniException(...)` and rely on this alias to transitively
// pull in `exception::OmniException`. The alias lives here so those call
// sites keep compiling unchanged.
using namespace exception;

class Allocator;   // forward — allocator.h includes omni_task_memory_pool.h;
                   // avoid the circular include. getBoundAllocator() is implemented
                   // in omni_task_memory_pool.cpp where the full type is available.

class OmniTaskMemoryPool {
public:
    struct ReclaimDiagnostics {
        uint64_t calls;
        uint64_t zeroTarget;
        uint64_t recursiveOrConcurrent;
        uint64_t emptyRegistry;
        uint64_t nonReclaimableSkips;
        uint64_t zeroCandidateSkips;
        uint64_t candidateSelections;
        uint64_t selfSelections;
        uint64_t successfulCalls;
        uint64_t freedBytes;
    };
    // Non-owning listener (OmniMemoryManager owns the unique_ptr).
    // listener may be nullptr for Allocator-only flows.
    explicit OmniTaskMemoryPool(omniruntime::mem::PoolListener *listener);
    ~OmniTaskMemoryPool();

    OmniTaskMemoryPool(const OmniTaskMemoryPool &) = delete;
    OmniTaskMemoryPool &operator=(const OmniTaskMemoryPool &) = delete;
    OmniTaskMemoryPool(OmniTaskMemoryPool &&) = delete;
    OmniTaskMemoryPool &operator=(OmniTaskMemoryPool &&) = delete;

    /**
     * AddMemory — two-phase commit:
     *   1. publish the new pool-local usedBytes_ value.
     *   2. UpdatePeak under accountingMutex_.
     *   3. try { listener_->allocationChanged(bytes); }
     *      catch { rollback this allocation while preserving synchronous reclaim; rethrow. }
     *
     * curAllocSize carried for diagnostic parity with the legacy MemoryManager
     * (used in error messages on cap exceed); threaded into the listener's deny path.
     */
    void AddMemory(int64_t bytes, int64_t curAllocSize = 0);

    /**
     * SubMemory — symmetric drop on usedBytes_ + listener up-call. Every
     * accounting drop MUST be paired with an allocationChanged(-bytes) — code-review
     * gate.
     */
    void SubMemory(int64_t bytes);

    // Borrowed only by the synchronous construction which holds this scope.
    // The allocator stored in every resulting buffer remains the pool's normal
    // persistent allocator; this scope never becomes allocation ownership.
    class AllocationAccountingScope {
    public:
        AllocationAccountingScope(const AllocationAccountingScope &) = delete;
        AllocationAccountingScope &operator=(const AllocationAccountingScope &) = delete;
        AllocationAccountingScope(AllocationAccountingScope &&) = delete;
        AllocationAccountingScope &operator=(AllocationAccountingScope &&) = delete;
        OmniTaskMemoryPool *pool() const { return pool_; }
    private:
        friend class OmniTaskMemoryPool;
        explicit AllocationAccountingScope(OmniTaskMemoryPool &pool)
            : pool_(&pool), lock_(pool.accountingMutex_), thread_(CurrentAccountingThread()) {}
        OmniTaskMemoryPool *pool_;
        std::unique_lock<std::recursive_mutex> lock_;
        uintptr_t thread_;
    };

    [[nodiscard]] AllocationAccountingScope accountingScope()
    {
        return AllocationAccountingScope(*this);
    }

    void AddMemoryWithScope(int64_t bytes, const AllocationAccountingScope &scope);


    // Ensure at least `bytes` of unused listener-billed headroom is available.
    // Like Velox MemoryPool::maybeReserve(), the requested minimum headroom is
    // rounded to the 8 MiB growth quantum, then only the missing headroom is
    // charged. Allocation from that headroom changes committed bytes without
    // charging the listener again.
    bool maybeReserve(int64_t bytes);
    // Compatibility spelling of maybeReserve(). Keep this out-of-line so
    // existing native callers retain the ABI symbol.
    bool reserve(int64_t bytes);
    static int64_t quantizedReservationSize(int64_t bytes);

    // Give back headroom which is not currently committed, retaining a
    // reservation equal to currentBytes().  release() is a compatibility
    // spelling.
    void releaseUnusedReservation();
    void release() { releaseUnusedReservation(); }

    // Drop the reservation marker completely.  Committed bytes remain billed
    // until they are freed.
    void releaseAllReservation();

    // Restore listener-billed capacity which a native spill offered to Spark
    // but the owning MemoryTarget could not repay. This is marker-only: Spark
    // still owns these bytes, so no listener callback is issued here.
    void restoreUnrepaidReservation(int64_t bytes);
    // Ledger-only transaction: validate restoration under the accounting lock,
    // run reconcile, then commit. reconcile must not reenter/mutate this pool.
    // A throwing reconcile must leave its own ledgers unchanged.
    void restoreUnrepaidReservation(int64_t bytes, const std::function<void()> &reconcile);

    int64_t currentBytes() const { return usedBytes_.load(std::memory_order_acquire); }
    int64_t committedBytes() const { return currentBytes(); }
    int64_t reservedBytes() const { return reservedBytes_.load(std::memory_order_acquire); }
    // Unused, already listener-billed headroom. Mirrors Velox
    // MemoryPool::availableReservation() and lets ensure-input-fits callers
    // avoid entering the reclaimable reservation slow path when the previous
    // explicit reservation already covers the next mutation.
    int64_t availableReservation() const;
    int64_t peakBytes() const { return peakBytes_.load(std::memory_order_acquire); }
    omniruntime::mem::PoolListener *getListener() const { return listener_; }

    void registerSpillable(omniruntime::op::SpillableOperator *op);
    void unregisterSpillable(omniruntime::op::SpillableOperator *op);
    std::vector<omniruntime::op::SpillableOperator *> snapshotSpillables();
    virtual int64_t reclaim(int64_t targetBytes);
    ReclaimDiagnostics reclaimDiagnostics() const noexcept;

    // getBoundAllocator — returns the pool-bound Allocator owned by this pool.
    // The Allocator instance is lazily constructed on first call (thread-safe via
    // double-checked initialisation in the .cpp). The returned pointer is valid for
    // the lifetime of this OmniTaskMemoryPool.
    //
    // Velox invariant mirror: every buffer created via this pool's allocator carries
    // the same pool pointer — pool_ set at construction, never reassigned.
    Allocator *getBoundAllocator();

    // Test-only hook — when set false, ListenerIsCallable() returns false even if
    // listener_ != nullptr, simulating a detached JNIEnv (the JVM-shutdown /
    // JIT-codegen-thread case). SparkAllocationListener's real `vm_->GetEnv() != JNI_OK`
    // check will eventually replace the ListenerIsCallable() body; default `true` keeps
    // production behaviour intact. The `_OverrideForTests` suffix makes the test-only
    // intent clear at the call site.
    static std::atomic<bool> &ListenerIsCallable_OverrideForTests();

private:
    void UpdatePeak(int64_t newTotal);
    void AddMemoryWhileLocked(int64_t bytes);
    static uintptr_t CurrentAccountingThread()
    {
#if defined(__linux__) && (defined(__aarch64__) || defined(__x86_64__)) && (defined(__GNUC__) || defined(__clang__))
        return reinterpret_cast<uintptr_t>(__builtin_thread_pointer());
#else
        return reinterpret_cast<uintptr_t>(&accountingThreadToken_);
#endif
    }
    static thread_local char accountingThreadToken_;


    // Returns `listener_ != nullptr` (gated by the test override flag above);
    // intended to be replaced with the real GetEnv()!=JNI_OK check inside
    // SparkAllocationListener. The abstraction layer keeps the dtor's drain
    // path stable without restructuring.
    bool ListenerIsCallable() const;

    std::atomic<int64_t> usedBytes_{0};
    std::atomic<int64_t> peakBytes_{0};
    std::atomic<int64_t> reservedBytes_{0};
    // Portion of reservedBytes_ published provisionally while a positive
    // listener callback is in flight. Synchronous reclaim must never report
    // this not-yet-granted capacity as freed.
    std::atomic<int64_t> pendingReservationDelta_{0};
    // accountingMutex_ protects these IDs. A marker published by reclaim or
    // maybeReserve includes every pending AddMemory up to reservationAllocationId_.
    // Rollback removes an ungranted delta only when that marker contains it.
    uint64_t allocationRequestId_{0};
    uint64_t reservationAllocationId_{0};
    std::atomic<bool> isBlocked_{false};        // CAS rollback gate; also used as cap-bypass
                                                 // flag under arbitration
    omniruntime::mem::PoolListener *listener_;       // non-owning; OmniMemoryManager owns the unique_ptr
    void *driver_{nullptr};                      // reserved back-pointer for self-suspension

    // Pool-bound Allocator owned by this pool. Lazily constructed on first call
    // to getBoundAllocator(). unique_ptr with incomplete type: destructor is
    // defined in omni_task_memory_pool.cpp where Allocator is fully declared.
    // pool lifetime outlives every buffer; this field ties the allocator
    // lifetime to the pool lifetime.
    std::unique_ptr<Allocator> boundAllocator_;
    // Guards the lazy first-construction of boundAllocator_ above. getBoundAllocator() is
    // public and reachable from concurrent Vector-ctor / Reserve / ORC-reader paths — not
    // only single-threaded operator ctors — so the check-then-create must be call_once'd.
    std::once_flag boundAllocatorOnce_;

    // Serializes the listener-billing state transition. recursive_mutex is
    // intentional: a listener may synchronously arbitrate and re-enter this
    // pool on the same thread.
    mutable std::recursive_mutex accountingMutex_;

    std::vector<omniruntime::op::SpillableOperator *> spillables_;
    std::mutex spillablesMutex_;
    std::atomic_flag reclaiming_ = ATOMIC_FLAG_INIT;
    std::atomic<uint64_t> reclaimCalls_{0};
    std::atomic<uint64_t> reclaimZeroTarget_{0};
    std::atomic<uint64_t> reclaimRecursiveOrConcurrent_{0};
    std::atomic<uint64_t> reclaimEmptyRegistry_{0};
    std::atomic<uint64_t> reclaimNonReclaimableSkips_{0};
    std::atomic<uint64_t> reclaimZeroCandidateSkips_{0};
    std::atomic<uint64_t> reclaimCandidateSelections_{0};
    std::atomic<uint64_t> reclaimSelfSelections_{0};
    std::atomic<uint64_t> reclaimSuccessfulCalls_{0};
    std::atomic<uint64_t> reclaimFreedBytes_{0};
};

} // namespace mem
} // namespace omniruntime

#endif // OMNI_RUNTIME_OMNI_TASK_MEMORY_POOL_H
