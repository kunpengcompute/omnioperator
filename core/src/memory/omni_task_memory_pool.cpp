/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 *
 * Implementation of OmniTaskMemoryPool. The legacy OmniTaskMemoryScope TLS
 * marker and its thread_local definition have been removed; allocation routing
 * is now purely via the per-buffer pool_ const member (Velox model).
 *
 * Pool-local counters use atomics for lock-free observers. Billing transitions
 * remain serialized by accountingMutex_.
 */

#include "omni_task_memory_pool.h"
#include "omni_task_telemetry.h"
#include "allocator.h"                 // full definition needed for getBoundAllocator() + ~unique_ptr<Allocator>
#include "pool_listener.h"   // Omni-owned listener contract
#include "global_mem_tracker.h"        // observe-only process-global accounting
#include "operator/operator.h"
#include "operator/reclaim_context.h"
#include "operator/spillable_operator.h"
#include "util/telemetry.h"
#ifdef OMNI_MEM_DEBUG
#include "mem_probe.h"                 // OMNI_MEM_DEBUG-only pool dtor-residual probe
#endif

#include <cassert>
#include <algorithm>
#include <cstdio>
#include <exception>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace omniruntime {
namespace mem {
thread_local char OmniTaskMemoryPool::accountingThreadToken_ = 0;

// ── Observe-only process-global compatibility state (see global_mem_tracker.h).
namespace {
std::atomic<int64_t> g_globalMemoryLimit{GLOBAL_MEM_UNLIMITED};

std::mutex &GlobalPoolsMutex()
{
    static auto *mutex = new std::mutex();
    return *mutex;
}

std::unordered_set<const OmniTaskMemoryPool *> &GlobalPools()
{
    static auto *pools = new std::unordered_set<const OmniTaskMemoryPool *>();
    return *pools;
}

std::mutex &TelemetryMutex()
{
    static auto *mutex = new std::mutex();
    return *mutex;
}

std::unordered_map<const OmniTaskMemoryPool *, OmniTaskTelemetryContext> &TelemetryByPool()
{
    // Native pools may be torn down during JVM shutdown, after ordinary
    // static destruction has begun, so this registry is process-lifetime.
    static auto *contexts =
        new std::unordered_map<const OmniTaskMemoryPool *, OmniTaskTelemetryContext>();
    return *contexts;
}
} // namespace

int64_t GetGlobalAccountedMemory()
{
    std::lock_guard<std::mutex> guard(GlobalPoolsMutex());
    int64_t total = 0;
    for (const auto *pool : GlobalPools()) {
        const int64_t bytes = pool->currentBytes();
        if (bytes > 0 && total > std::numeric_limits<int64_t>::max() - bytes) {
            return std::numeric_limits<int64_t>::max();
        }
        total += bytes;
    }
    return total;
}
void SetGlobalMemoryLimit(int64_t limit) { g_globalMemoryLimit.store(limit, std::memory_order_relaxed); }
int64_t GetGlobalMemoryLimit() { return g_globalMemoryLimit.load(std::memory_order_relaxed); }

// Test-only hook — defaults to `true` so production behaviour is unchanged.
// Tests flip to `false` to simulate a detached JNIEnv, then flip back.
// Atomic for visibility across the dtor's load (test thread flips → dtor sees
// the flip → drain is skipped — no torn read possible on a primitive bool).
// Function-local so each module linking libmemory does not export its own copy
// of a namespace-scope global (an ODR violation under ASan).
std::atomic<bool> &OmniTaskMemoryPool::ListenerIsCallable_OverrideForTests()
{
    static std::atomic<bool> callable{true};
    return callable;
}

OmniTaskMemoryPool::OmniTaskMemoryPool(omniruntime::mem::PoolListener *listener) : listener_(listener)
{
    std::lock_guard<std::mutex> guard(GlobalPoolsMutex());
    GlobalPools().insert(this);
}

void SetOmniTaskPoolTelemetry(
    OmniTaskMemoryPool *pool,
    const std::string &runId,
    const std::string &queryId,
    const std::string &round,
    int64_t taskAttemptId)
{
    if (pool == nullptr) {
        return;
    }
    std::lock_guard<std::mutex> guard(TelemetryMutex());
    TelemetryByPool()[pool] = {
        runId.empty() ? "unknown" : runId,
        queryId.empty() ? "unknown" : queryId,
        round.empty() ? "unknown" : round,
        taskAttemptId};
}

OmniTaskTelemetryContext GetOmniTaskPoolTelemetry(const OmniTaskMemoryPool *pool)
{
    std::lock_guard<std::mutex> guard(TelemetryMutex());
    const auto found = TelemetryByPool().find(pool);
    return found == TelemetryByPool().end() ? OmniTaskTelemetryContext() : found->second;
}

void ClearOmniTaskPoolTelemetry(const OmniTaskMemoryPool *pool)
{
    std::lock_guard<std::mutex> guard(TelemetryMutex());
    TelemetryByPool().erase(pool);
}

OmniTaskMemoryPool::~OmniTaskMemoryPool()
{
    {
        std::lock_guard<std::mutex> guard(GlobalPoolsMutex());
        GlobalPools().erase(this);
    }
    ClearOmniTaskPoolTelemetry(this);
    // Destructor drain — reconcile any residual to the listener so Spark
    // accounting nets to zero across task lifetimes (a historical q88 SF10 leak
    // came from skipping this).
    //
    // JNIEnv guard — when the dtor runs on a thread with no attached JNIEnv
    // (JVM shutdown / JIT codegen / LLVM ORC compile threads), the listener's
    // up-call would crash. ListenerIsCallable() abstracts the check; the real
    // `vm_->GetEnv() != JNI_OK` is plugged in later (see JniCommon.h:386-388).
    // Drain skip is acceptable drift — JVM shutdown reclaims everything anyway.
    std::lock_guard<std::recursive_mutex> accountingGuard(accountingMutex_);
    int64_t residual = usedBytes_.load(std::memory_order_acquire);
    const int64_t reserved = reservedBytes_.load(std::memory_order_acquire);
    const int64_t billedResidual = std::max(residual, reserved);
    if (billedResidual == 0 || listener_ == nullptr) {
#ifdef OMNI_MEM_DEBUG
        if (residual != 0) {
            omniruntime::mem::probe::RecordPoolDtorResidual(residual, /*drained=*/false);
        }
#endif
        return;
    }
    if (!ListenerIsCallable()) {
        // JNIEnv-detached skip path. No log macros wired into mem/ yet;
        // SparkAllocationListener already emits LOG(WARNING) for the same case.
        // LOG(WARNING) << "Pool dtor: cannot drain residual=" << residual
        //              << " (listener not callable / JNIEnv not attached)";
#ifdef OMNI_MEM_DEBUG
        omniruntime::mem::probe::RecordPoolDtorResidual(residual, /*drained=*/false);
#endif
        return;
    }
    // Dtors must not throw — wrap the listener call. If the listener throws
    // mid-drain, the symmetric Sub doesn't happen and Spark may report stale
    // bytes for this task; the JVM-side TaskResources release path catches it
    // and emits its own warning (NativeMemoryManager.scala:101-110). Better
    // a stale-bytes warning than std::terminate from a dtor exception.
    try {
        listener_->allocationChanged(-billedResidual);
#ifdef OMNI_MEM_DEBUG
        omniruntime::mem::probe::RecordPoolDtorResidual(residual, /*drained=*/true);
#endif
    } catch (...) {
        // LOG(ERROR) << "Pool dtor: listener drain threw for residual=" << residual;
    }
}

Allocator *OmniTaskMemoryPool::getBoundAllocator()
{
    // Lazy construction: one pool-bound Allocator per pool. getBoundAllocator() is public
    // and reached not only from single-threaded operator ctors but also from the Vector
    // ctor / Reserve / expand and ORC-reader hot paths, where a concurrent first-call would
    // race the check-then-create (two threads both see !boundAllocator_, both make_unique;
    // one overwrite leaks an Allocator and hands the loser a dangling pointer). std::call_once
    // makes first-construction safe with zero cost on the steady-state path.
    std::call_once(boundAllocatorOnce_, [this] {
        boundAllocator_ = std::make_unique<Allocator>(this);
    });
    return boundAllocator_.get();
}

ALWAYS_INLINE void OmniTaskMemoryPool::AddMemoryWhileLocked(int64_t bytes)
{
    const int64_t prev = usedBytes_.load(std::memory_order_relaxed);
    if (prev > std::numeric_limits<int64_t>::max() - bytes) {
        throw std::overflow_error("OmniTaskMemoryPool: used bytes overflow");
    }
    const int64_t newTotal = prev + bytes;
    // accountingMutex_ serializes writers, so a load/store pair avoids an
    // unnecessary locked RMW instruction while retaining atomic observers.
    // Snapshot reservation under the caller-held accounting mutex before publishing
    // usage. Keep acquire/release semantics and publish before listener re-entry;
    // this avoids the hot release-store/acquire-load sequence on AArch64.
    const int64_t reserved = reservedBytes_.load(std::memory_order_acquire);
    usedBytes_.store(newTotal, std::memory_order_release);
    const int64_t listenerDelta = std::max<int64_t>(0, newTotal - std::max(prev, reserved));
    const uint64_t requestId = listenerDelta > 0 ? ++allocationRequestId_ : 0;

    // Listener up-call (two-phase commit). On throw:
    //   1. CAS the isBlocked_ flag false→true so exactly one thread wins the flip
    //      across N concurrent throwers (multiple threads can see isBlocked_==false
    //      simultaneously, all attempt the CAS; only one succeeds; the losers see
    //      expected=true and don't retry, which is fine — the flag is set).
    //   2. Remove this allocation while preserving any reclaim completed by the
    //      synchronous listener call.
    //   3. Rethrow the listener's exception (caller chain decides how to react).
    try {
        if (listener_ != nullptr && listenerDelta != 0) {
            listener_->allocationChanged(listenerDelta);
        }
    } catch (...) {
        // Step 1 — CAS sets isBlocked_ false→true exactly once.
        bool expected = false;
        isBlocked_.compare_exchange_strong(expected, true,
                                           std::memory_order_acq_rel,
                                           std::memory_order_acquire);
        // Step 2 — preserve any synchronous reclaim performed by the listener,
        // then roll back only this allocation.
        const int64_t current = usedBytes_.load(std::memory_order_relaxed);
        usedBytes_.store(std::max<int64_t>(0, current - bytes), std::memory_order_release);
        if (requestId != 0 && requestId <= reservationAllocationId_) {
            const int64_t currentReservation = reservedBytes_.load(std::memory_order_relaxed);
            reservedBytes_.store(std::max<int64_t>(0, currentReservation - listenerDelta),
                                 std::memory_order_release);
        }
        // Step 3 — propagate the listener's exception. Caller decides reaction.
        std::rethrow_exception(std::current_exception());
    }

    // Raise the monotonic peak only after the listener grants the allocation.
    // Done here (not before the up-call) so a denied allocation — which rolls
    // back usedBytes_ in the catch above — does not leave peakBytes_ permanently
    // inflated. Nothing between the pool-local update and the listener call
    // depends on peakBytes_ being already raised.
    // The synchronous listener may have reclaimed existing allocations. Only
    // the committed live total is a peak candidate, not the pre-callback sum.
    UpdatePeak(usedBytes_.load(std::memory_order_relaxed));
}

void OmniTaskMemoryPool::AddMemory(int64_t bytes, int64_t /*curAllocSize*/)
{
    if (bytes <= 0) return;
    std::lock_guard<std::recursive_mutex> accountingGuard(accountingMutex_);
    AddMemoryWhileLocked(bytes);
}

void OmniTaskMemoryPool::AddMemoryWithScope(
    int64_t bytes, const AllocationAccountingScope &scope)
{
    if (scope.pool_ != this || !scope.lock_.owns_lock() ||
        scope.lock_.mutex() != &accountingMutex_ ||
        scope.thread_ != CurrentAccountingThread()) {
        throw std::logic_error("OmniTaskMemoryPool: invalid accounting scope");
    }
    if (bytes <= 0) return;
    AddMemoryWhileLocked(bytes);
}

bool OmniTaskMemoryPool::ListenerIsCallable() const
{
    // A more complete implementation would replace this body with a real
    // `vm_->GetEnv() != JNI_OK` check (see gluten/cpp/core/jni/JniCommon.h:386-388).
    // The test override flag lets DtorWithoutJNIEnv simulate the detach without
    // needing real JNI plumbing.
    return listener_ != nullptr
           && ListenerIsCallable_OverrideForTests().load(std::memory_order_acquire);
}

void OmniTaskMemoryPool::SubMemory(int64_t bytes)
{
    if (bytes <= 0) {
        return;
    }
    std::lock_guard<std::recursive_mutex> accountingGuard(accountingMutex_);
    const int64_t prev = usedBytes_.load(std::memory_order_acquire);
    // Underflow guard: freeing more than is accounted means an asymmetric Add/Sub
    // (double-free or a wrong vectorCapacity formula) and would drive usedBytes_
    // negative.
    const int64_t accountedDrop = std::min(prev, bytes);
    usedBytes_.store(prev - accountedDrop, std::memory_order_release);
    if (accountedDrop != bytes) {
        std::fprintf(stderr,
            "[OMNI-ACCOUNTING] event=sub_underflow requested=%ld accounted=%ld missing=%ld\n",
            static_cast<long>(bytes), static_cast<long>(accountedDrop),
            static_cast<long>(bytes - accountedDrop));
    }

    // Symmetric listener call. Every accounting drop must be paired with a
    // negative up-call so Spark accounting reconciles.
    //
    // Dtors must not throw — SubMemory is reached from noexcept destructors
    // (~AlignedBuffer / ~Vector / ~ContainerVector via Allocator::Free). A
    // throwing listener here would std::terminate. Swallow + best-effort, same
    // pattern as the ~OmniTaskMemoryPool drain above. Accounting math is
    // unchanged; only the up-call is guarded.
    const int64_t reserved = reservedBytes_.load(std::memory_order_acquire);
    const int64_t listenerDelta = std::min(accountedDrop, std::max<int64_t>(0, prev - reserved));
    if (listener_ != nullptr && listenerDelta != 0) {
        try {
            listener_->allocationChanged(-listenerDelta);
        } catch (...) {
            // LOG(ERROR) << "SubMemory: listener up-call threw for bytes=" << bytes;
        }
    }
}

int64_t OmniTaskMemoryPool::quantizedReservationSize(int64_t bytes)
{
    if (bytes <= 0) {
        return 0;
    }
    constexpr int64_t kGrowthQuantum = int64_t{8} << 20;
    if (bytes > INT64_MAX - (kGrowthQuantum - 1)) {
        return -1;
    }
    return ((bytes + kGrowthQuantum - 1) / kGrowthQuantum) * kGrowthQuantum;
}

int64_t OmniTaskMemoryPool::availableReservation() const
{
    std::lock_guard<std::recursive_mutex> accountingGuard(accountingMutex_);
    const int64_t used = usedBytes_.load(std::memory_order_acquire);
    const int64_t reserved = reservedBytes_.load(std::memory_order_acquire);
    return std::max<int64_t>(0, reserved - used);
}

bool OmniTaskMemoryPool::maybeReserve(int64_t bytes)
{
    const int64_t rawEstimate = bytes;
    if (bytes <= 0) {
        return true;
    }
    // Velox rounds the requested minimum headroom first, then subtracts the
    // headroom already available in the leaf pool.
    const int64_t desiredHeadroom = quantizedReservationSize(bytes);
    if (desiredHeadroom < 0) {
        return false;
    }
    std::lock_guard<std::recursive_mutex> accountingGuard(accountingMutex_);
    const int64_t used = usedBytes_.load(std::memory_order_acquire);
    const int64_t reserved = reservedBytes_.load(std::memory_order_acquire);
    const int64_t available = std::max<int64_t>(0, reserved - used);
    if (available >= desiredHeadroom) {
        return true;
    }
    const int64_t reservationToAdd = desiredHeadroom - available;
    const int64_t billed = std::max(used, reserved);
    if (reservationToAdd > INT64_MAX - billed) {
        return false;
    }
    const int64_t target = billed + reservationToAdd;
    const int64_t delta = reservationToAdd;
    const auto request = omniruntime::op::CurrentReservationRequest();
    // Publish provisional headroom before the callback. A listener may
    // synchronously arbitrate and re-enter AddMemory on this pool; that nested
    // allocation must see and consume this reservation, not charge again.
    reservedBytes_.store(target, std::memory_order_release);
    reservationAllocationId_ = allocationRequestId_;
    pendingReservationDelta_.fetch_add(delta, std::memory_order_release);
    const auto rollback = [&]() {
        // The listener may have synchronously called reclaim() before it
        // reported that the requested delta could not be granted. Preserve
        // that spill adjustment while rolling back only this provisional
        // reservation; restoring the old absolute value would resurrect bytes
        // already offered to Spark through the spill return value.
        const int64_t currentReservation = reservedBytes_.load(std::memory_order_acquire);
        reservedBytes_.store(std::max<int64_t>(0, currentReservation - delta),
                             std::memory_order_release);
        pendingReservationDelta_.fetch_sub(delta, std::memory_order_release);
    };
    try {
        if (listener_ != nullptr && delta > 0) {
            listener_->allocationChanged(delta);
        }
    } catch (const ReservationDenied &) {
        rollback();
        bool expected = false;
        isBlocked_.compare_exchange_strong(expected, true, std::memory_order_acq_rel,
                                           std::memory_order_acquire);
        if (omniruntime::telemetry::IsReclaimVerboseEnabled()) {
            std::fprintf(stderr,
                "[OMNI-RESERVE] event=result request_id=%lu outcome=denied requestor_type=%s requestor=%p site=%s estimate=%ld raw_estimate=%ld requested=%ld granted=0 used=%ld reserved=%ld\n",
                static_cast<unsigned long>(request.id),
                request.requestorType == nullptr ? "unknown" : request.requestorType,
                static_cast<void *>(request.requestor), request.site == nullptr ? "unknown" : request.site,
                static_cast<long>(reservationToAdd), static_cast<long>(rawEstimate),
                static_cast<long>(reservationToAdd),
                static_cast<long>(currentBytes()), static_cast<long>(reservedBytes()));
        }
        return false;
    } catch (...) {
        // A failed reclaim may have mutated a donor. Roll back this request's
        // accounting, but never continue the task as if this were a clean denial.
        rollback();
        throw;
    }
    pendingReservationDelta_.fetch_sub(delta, std::memory_order_release);
    if (omniruntime::telemetry::IsReclaimVerboseEnabled()) {
        std::fprintf(stderr,
            "[OMNI-RESERVE] event=result request_id=%lu outcome=granted requestor_type=%s requestor=%p site=%s estimate=%ld raw_estimate=%ld requested=%ld granted=%ld used=%ld reserved=%ld\n",
            static_cast<unsigned long>(request.id),
            request.requestorType == nullptr ? "unknown" : request.requestorType,
            static_cast<void *>(request.requestor), request.site == nullptr ? "unknown" : request.site,
            static_cast<long>(reservationToAdd), static_cast<long>(rawEstimate),
            static_cast<long>(reservationToAdd), static_cast<long>(reservationToAdd),
            static_cast<long>(currentBytes()), static_cast<long>(reservedBytes()));
    }
    // Synchronous listener callbacks can consume or reclaim previously
    // available capacity. A grant alone does not guarantee the caller's bytes
    // are still available. Keep granted credit, but report a shortfall honestly.
    return std::max<int64_t>(0, reservedBytes_.load(std::memory_order_acquire) -
        usedBytes_.load(std::memory_order_acquire)) >= bytes;
}

bool OmniTaskMemoryPool::reserve(int64_t bytes)
{
    return maybeReserve(bytes);
}

void OmniTaskMemoryPool::releaseUnusedReservation()
{
    std::lock_guard<std::recursive_mutex> accountingGuard(accountingMutex_);
    const int64_t used = usedBytes_.load(std::memory_order_acquire);
    const int64_t reserved = reservedBytes_.load(std::memory_order_acquire);
    if (reserved <= used) {
        return;
    }
    const int64_t delta = reserved - used;
    reservedBytes_.store(used, std::memory_order_release);
    try {
        if (listener_ != nullptr) {
            listener_->allocationChanged(-delta);
        }
    } catch (...) {
        reservedBytes_.store(reserved, std::memory_order_release);
        throw;
    }
}

void OmniTaskMemoryPool::releaseAllReservation()
{
    std::lock_guard<std::recursive_mutex> accountingGuard(accountingMutex_);
    const int64_t used = usedBytes_.load(std::memory_order_acquire);
    const int64_t reserved = reservedBytes_.load(std::memory_order_acquire);
    const int64_t delta = std::max<int64_t>(0, reserved - used);
    const uint64_t reservationAllocationBefore = reservationAllocationId_;
    reservedBytes_.store(0, std::memory_order_release);
    reservationAllocationId_ = 0;
    try {
        if (listener_ != nullptr && delta != 0) {
            listener_->allocationChanged(-delta);
        }
    } catch (...) {
        reservedBytes_.store(reserved, std::memory_order_release);
        reservationAllocationId_ = reservationAllocationBefore;
        throw;
    }
}

void OmniTaskMemoryPool::restoreUnrepaidReservation(int64_t bytes)
{
    if (bytes <= 0) return;
    restoreUnrepaidReservation(bytes, [] {});
}

void OmniTaskMemoryPool::restoreUnrepaidReservation(
    int64_t bytes, const std::function<void()> &reconcile)
{
    if (bytes < 0) {
        throw std::invalid_argument("OmniTaskMemoryPool: negative unrepaid reservation");
    }
    std::lock_guard<std::recursive_mutex> accountingGuard(accountingMutex_);
    const int64_t reserved = reservedBytes_.load(std::memory_order_acquire);
    if (bytes > INT64_MAX - reserved) {
        throw std::overflow_error("OmniTaskMemoryPool: unrepaid reservation overflow");
    }
    reconcile();
    reservedBytes_.store(reserved + bytes, std::memory_order_release);
}

void OmniTaskMemoryPool::registerSpillable(omniruntime::op::SpillableOperator *op)
{
    if (op == nullptr) {
        return;
    }
    std::lock_guard<std::mutex> guard(spillablesMutex_);
    if (std::find(spillables_.begin(), spillables_.end(), op) == spillables_.end()) {
        spillables_.push_back(op);
        if (auto *operatorBase = op->operatorBase(); operatorBase != nullptr) {
            operatorBase->setReclaimGuardEnabled(true);
        }
    }
}

void OmniTaskMemoryPool::unregisterSpillable(omniruntime::op::SpillableOperator *op)
{
    std::lock_guard<std::mutex> guard(spillablesMutex_);
    spillables_.erase(std::remove(spillables_.begin(), spillables_.end(), op), spillables_.end());
    if (op != nullptr) {
        if (auto *operatorBase = op->operatorBase(); operatorBase != nullptr) {
            operatorBase->setReclaimGuardEnabled(false);
        }
    }
}

std::vector<omniruntime::op::SpillableOperator *> OmniTaskMemoryPool::snapshotSpillables()
{
    std::lock_guard<std::mutex> guard(spillablesMutex_);
    return spillables_;
}

int64_t OmniTaskMemoryPool::reclaim(int64_t targetBytes)
{
    reclaimCalls_.fetch_add(1, std::memory_order_relaxed);
    const bool verboseTelemetry = omniruntime::telemetry::IsReclaimVerboseEnabled();
    const auto active = omniruntime::op::CurrentActiveOperatorCall();
    const auto reservation = omniruntime::op::CurrentReservationRequest();
    const uint64_t requestId = reservation.id != 0
        ? reservation.id
        : omniruntime::op::nextMemoryRequestId.fetch_add(1, std::memory_order_relaxed);
    const char *trigger = reservation.id != 0 ? "pre_reservation" : "allocation";
    const char *requestorType = reservation.requestorType != nullptr
        ? reservation.requestorType
        : (active.type == nullptr || active.type[0] == '\0' ? "unknown" : active.type);
    const void *requestor = reservation.requestor != nullptr
        ? static_cast<void *>(reservation.requestor)
        : static_cast<void *>(active.op);
    const char *site = reservation.site != nullptr
        ? reservation.site
        : (active.method == nullptr ? "outside_operator_dispatch" : active.method);
    OmniTaskTelemetryContext telemetry;
    if (verboseTelemetry) {
        telemetry = GetOmniTaskPoolTelemetry(this);
    }
    const char *telemetryRunId = telemetry.runId.c_str();
    const char *telemetryQueryId = telemetry.queryId.c_str();
    const char *telemetryRound = telemetry.round.c_str();
    const long long telemetryTaskAttemptId = static_cast<long long>(telemetry.taskAttemptId);
    auto &trace = omniruntime::op::reclaimTraceSummary;
    trace = {requestId, trigger, requestorType, requestor, site, nullptr, nullptr, 0, 0, 0, 0};
    if (targetBytes <= 0) {
        reclaimZeroTarget_.fetch_add(1, std::memory_order_relaxed);
        if (verboseTelemetry) std::fprintf(stderr,
            "[OMNI-RECLAIM] event=complete request_id=%lu trigger=%s outcome=zero reason=non_positive_request requested=%ld freed=0 requestor_type=%s requestor=%p site=%s active_method=%s run_id=%s qid=%s round=%s task_attempt_id=%lld\n",
            static_cast<unsigned long>(requestId), trigger, static_cast<long>(targetBytes), requestorType, requestor,
            site, active.method == nullptr ? "none" : active.method,
            telemetryRunId, telemetryQueryId, telemetryRound, telemetryTaskAttemptId);
        return 0;
    }
    if (reclaiming_.test_and_set(std::memory_order_acquire)) {
        reclaimRecursiveOrConcurrent_.fetch_add(1, std::memory_order_relaxed);
        if (verboseTelemetry) std::fprintf(stderr,
            "[OMNI-RECLAIM] event=complete request_id=%lu trigger=%s outcome=zero reason=recursive_or_concurrent requested=%ld freed=0 requestor_type=%s requestor=%p site=%s active_method=%s run_id=%s qid=%s round=%s task_attempt_id=%lld\n",
            static_cast<unsigned long>(requestId), trigger, static_cast<long>(targetBytes), requestorType, requestor,
            site, active.method == nullptr ? "none" : active.method,
            telemetryRunId, telemetryQueryId, telemetryRound, telemetryTaskAttemptId);
        return 0;
    }
    struct ReclaimFlagReset {
        std::atomic_flag &flag;
        ~ReclaimFlagReset() { flag.clear(std::memory_order_release); }
    } reset{reclaiming_};

    auto candidates = snapshotSpillables();
    omniruntime::op::Operator *requestorOp = reservation.requestor != nullptr
        ? reservation.requestor
        : active.op;
    void *requestorPool = requestorOp != nullptr
        ? static_cast<void *>(requestorOp->pool())
        : nullptr;
    if (verboseTelemetry) {
        std::fprintf(stderr,
        "[OMNI-RECLAIM] event=trigger request_id=%lu trigger=%s requestor_type=%s requestor=%p site=%s requested=%ld estimate=%ld pool=%p requestor_pool=%p used=%ld reserved=%ld pending=%ld candidates=%ld run_id=%s qid=%s round=%s task_attempt_id=%lld\n",
        static_cast<unsigned long>(requestId), trigger, requestorType, requestor, site,
        static_cast<long>(targetBytes), static_cast<long>(reservation.estimate), static_cast<void *>(this),
        requestorPool,
        static_cast<long>(currentBytes()), static_cast<long>(reservedBytes()),
        static_cast<long>(pendingReservationDelta_.load(std::memory_order_acquire)),
        static_cast<long>(candidates.size()),
        telemetryRunId, telemetryQueryId, telemetryRound, telemetryTaskAttemptId);
    }
    if (candidates.empty()) {
        reclaimEmptyRegistry_.fetch_add(1, std::memory_order_relaxed);
        // An empty registry can still hold billed, unused reservation. Run
        // accounting finalization so Spark can reclaim that headroom too.
    }
    const uint64_t requestedBytes = static_cast<uint64_t>(targetBytes);
    // Prefer the smallest candidate which can satisfy the request by itself.
    // Most operator reclaim implementations spill their complete accumulated
    // state, so largest-first can turn a small request into disproportionate
    // disk I/O. If no single candidate is large enough, retain largest-first
    // ordering to reach the target with as few spill calls as possible.
    struct EstimatedCandidate {
        omniruntime::op::SpillableOperator *donor;
        uint64_t bytes;
        bool protectedAtSnapshot;
    };
    std::vector<EstimatedCandidate> estimatedCandidates;
    estimatedCandidates.reserve(candidates.size());
    for (auto *candidate : candidates) {
        const bool protectedNow = candidate->isInNonReclaimableSection();
        estimatedCandidates.push_back({candidate, protectedNow ? 0 : candidate->reclaimableBytes(), protectedNow});
    }
    std::stable_sort(estimatedCandidates.begin(), estimatedCandidates.end(),
        [requestedBytes](const auto &left, const auto &right) {
            const bool leftSufficient = left.bytes >= requestedBytes;
            const bool rightSufficient = right.bytes >= requestedBytes;
            if (leftSufficient != rightSufficient) return leftSufficient;
            return leftSufficient ? left.bytes < right.bytes : left.bytes > right.bytes;
        });

    int64_t poolBefore = 0;
    int64_t reservationBefore = 0;
    uint64_t reservationAllocationBefore = 0;
    int64_t pendingReservation = 0;
    int64_t billableBefore = 0;
    int64_t reclaimBillingShield = 0;
    {
        std::lock_guard<std::recursive_mutex> accountingGuard(accountingMutex_);
        poolBefore = currentBytes();
        reservationBefore = reservedBytes_.load(std::memory_order_acquire);
        reservationAllocationBefore = reservationAllocationId_;
        pendingReservation = pendingReservationDelta_.load(std::memory_order_acquire);
        const int64_t committedReservationBefore = std::max<int64_t>(
            0, reservationBefore - pendingReservation);
        billableBefore = std::max(poolBefore, committedReservationBefore);
        if (pendingReservation > INT64_MAX - billableBefore) {
            throw std::overflow_error("OmniTaskMemoryPool: reclaim billing shield overflow");
        }
        reclaimBillingShield = billableBefore + pendingReservation;

        // `used > reserved` means the listener has already billed the excess
        // through AddMemory even though it is not represented as explicit
        // reservation headroom. Publish a marker-only shield for that billed
        // capacity while donors free memory. Otherwise SubMemory would credit
        // Spark here and the spill callback would credit the same drop again via
        // MemoryTarget.repay().
        reservedBytes_.store(reclaimBillingShield, std::memory_order_release);
        reservationAllocationId_ = allocationRequestId_;
    }
    struct ReclaimBillingShieldGuard {
        ReclaimBillingShieldGuard(std::atomic<int64_t> &reservedRef, int64_t exceptionReservation)
            : reserved(reservedRef), exceptionValue(exceptionReservation)
        {
        }

        std::atomic<int64_t> &reserved;
        int64_t exceptionValue;
        bool finalized{false};

        ~ReclaimBillingShieldGuard()
        {
            if (!finalized) {
                // If a donor throws after freeing memory, no external Spark
                // repay follows. Retain the pre-reclaim billed amount as local
                // headroom so task teardown can return it exactly once.
                reserved.store(exceptionValue, std::memory_order_release);
            }
        }

        void Finalize(int64_t value)
        {
            reserved.store(value, std::memory_order_release);
            finalized = true;
        }
    } billingShieldGuard{reservedBytes_, reclaimBillingShield};
    int64_t measuredFreed = 0;
    int64_t nrsSkips = 0;
    int64_t emptySkips = 0;
    bool reclaimedAny = false;
    int64_t rank = 0;
    int64_t donors = 0;
    for (const auto &entry : estimatedCandidates) {
        auto *candidate = entry.donor;
        ++rank;
        auto *candidateOperator = candidate->operatorBase();
        auto *requestorOperator = reservation.requestor != nullptr ? reservation.requestor : active.op;
        const bool isSelf = candidateOperator != nullptr && candidateOperator == requestorOperator;
        const bool isActive = candidateOperator != nullptr && candidateOperator == active.op;
        const char *relation = isSelf ? "self" : (isActive ? "active" : "inactive");
        const uint64_t estimated = entry.bytes;
        // CALL_OPERATOR marks the active operator NRS=true.  Its reviewed
        // pre-reservation window temporarily clears NRS, so this one predicate
        // admits both inactive donors and a safe active donor. `isActive` is
        // retained for structured-debug classification at this decision point.
        if (entry.protectedAtSnapshot || candidate->isInNonReclaimableSection()) {
            reclaimNonReclaimableSkips_.fetch_add(1, std::memory_order_relaxed);
            ++nrsSkips;
            if (verboseTelemetry) std::fprintf(stderr,
                "[OMNI-RECLAIM] event=candidate request_id=%lu rank=%ld donor_type=%s donor=%p relation=%s estimated=%lu nrs=1 decision=skip_non_reclaimable run_id=%s qid=%s round=%s task_attempt_id=%lld\n",
                static_cast<unsigned long>(requestId), static_cast<long>(rank), candidate->reclaimableName(),
                static_cast<void *>(candidateOperator), relation,
                static_cast<unsigned long>(estimated),
                telemetryRunId, telemetryQueryId, telemetryRound, telemetryTaskAttemptId);
            continue;
        }
        if (measuredFreed >= targetBytes) {
            if (verboseTelemetry) std::fprintf(stderr,
                "[OMNI-RECLAIM] event=candidate request_id=%lu rank=%ld donor_type=%s donor=%p relation=%s estimated=%lu nrs=%d decision=skip_target_met run_id=%s qid=%s round=%s task_attempt_id=%lld\n",
                static_cast<unsigned long>(requestId), static_cast<long>(rank), candidate->reclaimableName(),
                static_cast<void *>(candidateOperator), relation,
                static_cast<unsigned long>(entry.bytes),
                candidate->isInNonReclaimableSection() ? 1 : 0,
                telemetryRunId, telemetryQueryId, telemetryRound, telemetryTaskAttemptId);
            break;
        }
        if (estimated == 0) {
            reclaimZeroCandidateSkips_.fetch_add(1, std::memory_order_relaxed);
            ++emptySkips;
            if (verboseTelemetry) std::fprintf(stderr,
                "[OMNI-RECLAIM] event=candidate request_id=%lu rank=%ld donor_type=%s donor=%p relation=%s estimated=0 nrs=0 decision=skip_empty run_id=%s qid=%s round=%s task_attempt_id=%lld\n",
                static_cast<unsigned long>(requestId), static_cast<long>(rank), candidate->reclaimableName(),
                static_cast<void *>(candidateOperator), relation,
                telemetryRunId, telemetryQueryId, telemetryRound, telemetryTaskAttemptId);
            continue;
        }
        const int64_t before = currentBytes();
        if (verboseTelemetry) std::fprintf(stderr,
            "[OMNI-RECLAIM] event=candidate request_id=%lu rank=%ld donor_type=%s donor=%p relation=%s estimated=%lu nrs=0 decision=selected target_remaining=%ld pool_before=%ld run_id=%s qid=%s round=%s task_attempt_id=%lld\n",
            static_cast<unsigned long>(requestId), static_cast<long>(rank), candidate->reclaimableName(),
            static_cast<void *>(candidateOperator), relation,
            static_cast<unsigned long>(estimated), static_cast<long>(targetBytes - measuredFreed),
            static_cast<long>(before),
            telemetryRunId, telemetryQueryId, telemetryRound, telemetryTaskAttemptId);
        reclaimCandidateSelections_.fetch_add(1, std::memory_order_relaxed);
        if (isSelf) {
            reclaimSelfSelections_.fetch_add(1, std::memory_order_relaxed);
        }
        int64_t operatorFreed;
        try {
            operatorFreed = candidate->reclaim(targetBytes - measuredFreed);
        } catch (const std::exception& error) {
            // Even ReservationDenied is fatal once a donor has started a
            // destructive spill. Keep its cause across the JNI round trip.
            std::throw_with_nested(std::runtime_error(
                std::string("Native reclaim failed: ") + error.what()));
        }
        const int64_t after = currentBytes();
        const int64_t poolDelta = std::max<int64_t>(0, before - after);
        if (verboseTelemetry) {
            const long double rawPoolToEstimatePpm = estimated == 0 ? 0.0L :
                (static_cast<long double>(poolDelta) * 1000000.0L) / estimated;
            const int64_t poolToEstimatePpm = rawPoolToEstimatePpm >=
                static_cast<long double>(std::numeric_limits<int64_t>::max())
                ? std::numeric_limits<int64_t>::max()
                : static_cast<int64_t>(rawPoolToEstimatePpm);
            std::fprintf(stderr,
                "[OMNI-RECLAIM] event=donor_result request_id=%lu donor_type=%s donor=%p relation=%s estimated=%lu operator_reported=%ld pool_measured_freed=%ld pool_to_estimate_ppm=%ld pool_after=%ld run_id=%s qid=%s round=%s task_attempt_id=%lld\n",
                static_cast<unsigned long>(requestId), candidate->reclaimableName(),
                static_cast<void *>(candidateOperator), relation, static_cast<unsigned long>(estimated),
                static_cast<long>(operatorFreed), static_cast<long>(poolDelta),
                static_cast<long>(poolToEstimatePpm), static_cast<long>(after),
                telemetryRunId, telemetryQueryId, telemetryRound, telemetryTaskAttemptId);
        }
        if (after < before) {
            measuredFreed += before - after;
            reclaimedAny = true;
            ++donors;
        } else if (operatorFreed > 0) {
            reclaimedAny = true;
            ++donors;
        } else {
            reclaimZeroCandidateSkips_.fetch_add(1, std::memory_order_relaxed);
            ++emptySkips;
        }
        if ((poolDelta > 0 || operatorFreed > 0) && trace.primaryDonorType == nullptr) {
            trace.primaryDonorType = candidate->reclaimableName();
            trace.primaryDonor = candidateOperator;
            trace.primaryDonorEstimated = estimated;
            trace.primaryDonorPoolFreed = poolDelta;
        }
    }
    // Return the decrease in billed capacity, including unused reservation;
    // clamp at zero in case an operator allocated while spilling. Spark's TMM
    // credits this return value directly, so do not also listener-unbill it.
    // Lower the local reservation marker by the same amount instead, otherwise
    // a later release() would return the reclaimed capacity a second time.
    int64_t freed = 0;
    if (reclaimedAny || billableBefore > currentBytes()) {
        std::lock_guard<std::recursive_mutex> accountingGuard(accountingMutex_);
        const int64_t usedAfter = currentBytes();
        // A successful spill or unused billed headroom permits repayment. Keep only committed bytes plus any not-yet-granted request that
        // surrounds this synchronous callback. The caller will separately
        // commit or roll back that pending delta.
        // A donor may also allocate while spilling (scratch or reused I/O
        // buffers). Under the shield that growth was never billed, so it must
        // consume the pending request's headroom; keeping usedAfter as the
        // committed marker would bill it locally only and a later release
        // would return more than the listener ever received.
        const int64_t committedAfter = std::min(usedAfter, billableBefore);
        if (pendingReservation > INT64_MAX - committedAfter) {
            throw std::overflow_error("OmniTaskMemoryPool: post-reclaim reservation overflow");
        }
        billingShieldGuard.Finalize(committedAfter + pendingReservation);
        freed = std::max<int64_t>(0, billableBefore - usedAfter);
    } else {
        // No donor changed the pool, so restore the exact marker observed at
        // entry. The shield must not turn ordinary committed usage into sticky
        // explicit headroom after a zero-result reclaim probe.
        std::lock_guard<std::recursive_mutex> accountingGuard(accountingMutex_);
        billingShieldGuard.Finalize(reservationBefore);
        reservationAllocationId_ = reservationAllocationBefore;
    }
    if (freed > 0) {
        reclaimSuccessfulCalls_.fetch_add(1, std::memory_order_relaxed);
        reclaimFreedBytes_.fetch_add(static_cast<uint64_t>(freed), std::memory_order_relaxed);
    }
    trace.donors = donors;
    trace.freed = freed;
    if (verboseTelemetry) std::fprintf(stderr,
        "[OMNI-RECLAIM] event=complete request_id=%lu outcome=%s reason=%s requested=%ld freed=%ld donors=%ld primary_donor_type=%s primary_donor=%p candidates=%ld nrs_skips=%ld zero_skips=%ld requestor_type=%s requestor=%p site=%s active_method=%s run_id=%s qid=%s round=%s task_attempt_id=%lld\n",
        static_cast<unsigned long>(requestId),
        freed > 0 ? "success" : "zero",
        freed > 0 ? "reclaimed" : (nrsSkips > 0 ? "non_reclaimable" : "no_reclaimable_bytes"),
        static_cast<long>(targetBytes), static_cast<long>(freed), static_cast<long>(donors),
        trace.primaryDonorType == nullptr ? "none" : trace.primaryDonorType, trace.primaryDonor,
        static_cast<long>(candidates.size()),
        static_cast<long>(nrsSkips), static_cast<long>(emptySkips),
        requestorType, requestor, site,
        active.method == nullptr ? "none" : active.method,
        telemetryRunId, telemetryQueryId, telemetryRound, telemetryTaskAttemptId);
    return freed;
}

OmniTaskMemoryPool::ReclaimDiagnostics OmniTaskMemoryPool::reclaimDiagnostics() const noexcept
{
    return {reclaimCalls_.load(std::memory_order_relaxed),
            reclaimZeroTarget_.load(std::memory_order_relaxed),
            reclaimRecursiveOrConcurrent_.load(std::memory_order_relaxed),
            reclaimEmptyRegistry_.load(std::memory_order_relaxed),
            reclaimNonReclaimableSkips_.load(std::memory_order_relaxed),
            reclaimZeroCandidateSkips_.load(std::memory_order_relaxed),
            reclaimCandidateSelections_.load(std::memory_order_relaxed),
            reclaimSelfSelections_.load(std::memory_order_relaxed),
            reclaimSuccessfulCalls_.load(std::memory_order_relaxed),
            reclaimFreedBytes_.load(std::memory_order_relaxed)};
}

void OmniTaskMemoryPool::UpdatePeak(int64_t newTotal)
{
    // AddMemory holds accountingMutex_, so all writers are serialized. Keep the
    // counter atomic only for lock-free observers; a locked CAS is unnecessary.
    const int64_t saved = peakBytes_.load(std::memory_order_relaxed);
    if (newTotal > saved) {
        peakBytes_.store(newTotal, std::memory_order_release);
    }
}

} // namespace mem
} // namespace omniruntime
