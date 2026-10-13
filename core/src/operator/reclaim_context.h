/* Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved. */
#ifndef OMNI_RUNTIME_RECLAIM_CONTEXT_H
#define OMNI_RUNTIME_RECLAIM_CONTEXT_H

#include <atomic>
#include <cstdint>

namespace omniruntime::op {
class Operator;

struct ActiveOperatorCall {
    Operator *op{nullptr};
    const char *method{nullptr};
    const char *type{nullptr};
};

// Reclaim is synchronous with the allocation which requested it. A TLS marker
// therefore identifies the active operator without cross-thread lifetime risk.
inline thread_local ActiveOperatorCall activeOperatorCall;

class ActiveOperatorCallSection {
public:
    ActiveOperatorCallSection(
        Operator *op, const char *method, const char *type = nullptr, bool enabled = true) noexcept
        : enabled_(enabled)
    {
        if (enabled_) {
            saved_ = activeOperatorCall;
            activeOperatorCall = {op, method, type};
        }
    }
    ~ActiveOperatorCallSection()
    {
        if (enabled_) {
            activeOperatorCall = saved_;
        }
    }
    ActiveOperatorCallSection(const ActiveOperatorCallSection &) = delete;
    ActiveOperatorCallSection &operator=(const ActiveOperatorCallSection &) = delete;

private:
    bool enabled_;
    ActiveOperatorCall saved_;
};

inline ActiveOperatorCall CurrentActiveOperatorCall() noexcept
{
    return activeOperatorCall;
}

struct ReservationRequestContext {
    uint64_t id{0};
    Operator *requestor{nullptr};
    const char *requestorType{nullptr};
    const char *site{nullptr};
    int64_t estimate{0};
};

inline thread_local ReservationRequestContext reservationRequestContext;
inline std::atomic<uint64_t> nextMemoryRequestId{1};

class ReservationRequestSection {
public:
    ReservationRequestSection(
        Operator *requestor, const char *requestorType, const char *site, int64_t estimate) noexcept
        : saved_(reservationRequestContext)
    {
        reservationRequestContext = {
            nextMemoryRequestId.fetch_add(1, std::memory_order_relaxed),
            requestor,
            requestorType,
            site,
            estimate};
    }
    ~ReservationRequestSection() { reservationRequestContext = saved_; }
    void updateEstimate(int64_t estimate) noexcept { reservationRequestContext.estimate = estimate; }
    ReservationRequestSection(const ReservationRequestSection &) = delete;
    ReservationRequestSection &operator=(const ReservationRequestSection &) = delete;

private:
    ReservationRequestContext saved_;
};

inline ReservationRequestContext CurrentReservationRequest() noexcept
{
    return reservationRequestContext;
}

// Synchronous hand-off from OmniTaskMemoryPool::reclaim() to the Gluten
// MemoryManager::shrink() frame which invoked it. This lets the outer Spark
// shrink record carry the same request id and the primary native donor without
// changing the public reclaim(int64_t) ABI.
struct ReclaimTraceSummary {
    uint64_t requestId{0};
    const char *trigger{nullptr};
    const char *requestorType{nullptr};
    const void *requestor{nullptr};
    const char *site{nullptr};
    const char *primaryDonorType{nullptr};
    const void *primaryDonor{nullptr};
    uint64_t primaryDonorEstimated{0};
    int64_t primaryDonorPoolFreed{0};
    int64_t donors{0};
    int64_t freed{0};
};

inline thread_local ReclaimTraceSummary reclaimTraceSummary;

inline ReclaimTraceSummary CurrentReclaimTraceSummary() noexcept
{
    return reclaimTraceSummary;
}
} // namespace omniruntime::op
#endif
