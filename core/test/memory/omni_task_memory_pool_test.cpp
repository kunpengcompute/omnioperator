/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 *
 * gtest scaffolding for OmniTaskMemoryPool (Per-task Memory Pool Foundation)
 * covering pool-bound Allocator and explicit-Allocator AlignedBuffer.
 *
 * LifecycleBasic, Stats, AllocatorRoutesToPool —
 * cover basic pool accounting and allocator TLS routing.
 *
 * TwoPhaseCommitRollback, DrainOnDestruction, DrainNoLeak10k (H-5
 * hypothesis validation), DtorWithoutJNIEnv.
 *
 * PoolBoundAllocator.* and AlignedBufferPool.* — pool-bound routing without TLS.
 *
 * The pool class comes from omni_task_memory_pool.{h,cpp}.
 * omni_task_memory_scope.h has been deleted — there is no TLS scope.
 */

#include <stdexcept>
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <future>
#include <chrono>
#include <sys/mman.h>
#include <unistd.h>
#include <vector>

#include "gtest/gtest.h"

#include "omni_task_memory_pool.h"
#include "allocator.h"
#include "global_mem_tracker.h"          // process-global accounted-memory counter
#include "aligned_buffer.h"
#include "pool_listener.h"   // single-source-of-truth omniruntime::mem::PoolListener
#include "vector/vector.h"             // VectorPool.* and LargeStringBufferPool.* tests
#include "vector/string_utils.h"       // LargeStringBufferPool.* tests
#include "vector/vector_helper.h"      // VectorPool dictionary/slice pool-charge
#include "simple_arena_allocator.h"    // ArenaAllocatorPool.* tests
#include "operator/hashmap/base_hash_map.h"  // HashmapAllocatorPool.* tests
#include "operator/reclaim_context.h"
#include "operator/spillable_operator.h"

namespace omniruntime::mem::test {

class ScopedEnv {
public:
    ScopedEnv(const char *name, const char *value) : name_(name)
    {
        const char *old = std::getenv(name);
        had_ = old != nullptr;
        if (had_) {
            saved_ = old;
        }
        if (value == nullptr) {
            unsetenv(name);
        } else if (setenv(name, value, 1) != 0) {
            throw std::runtime_error("setenv failed");
        }
    }

    ~ScopedEnv()
    {
        if (had_) {
            setenv(name_.c_str(), saved_.c_str(), 1);
        } else {
            unsetenv(name_.c_str());
        }
    }

    ScopedEnv(const ScopedEnv &) = delete;
    ScopedEnv &operator=(const ScopedEnv &) = delete;

private:
    std::string name_;
    std::string saved_;
    bool had_ = false;
};

class CaptureStderr {
public:
    CaptureStderr()
    {
        fflush(stderr);
        savedFd_ = dup(fileno(stderr));
        // Anonymous in-memory file: nothing is created in a shared temp directory.
        const int memFd = memfd_create("capture-stderr", 0);
        tmp_ = memFd >= 0 ? fdopen(memFd, "w+") : nullptr;
        if (savedFd_ >= 0 && tmp_ != nullptr) {
            dup2(fileno(tmp_), fileno(stderr));
        }
    }

    ~CaptureStderr()
    {
        fflush(stderr);
        if (savedFd_ >= 0) {
            dup2(savedFd_, fileno(stderr));
            close(savedFd_);
        }
        if (tmp_ != nullptr) {
            fclose(tmp_);
        }
    }

    CaptureStderr(const CaptureStderr &) = delete;
    CaptureStderr &operator=(const CaptureStderr &) = delete;

    std::string Collect()
    {
        fflush(stderr);
        if (tmp_ == nullptr) {
            return {};
        }
        rewind(tmp_);
        std::string out;
        char buf[4096];
        size_t n = 0;
        while ((n = fread(buf, 1, sizeof(buf), tmp_)) > 0) {
            out.append(buf, n);
        }
        fseek(tmp_, 0, SEEK_END);
        return out;
    }

private:
    int savedFd_ = -1;
    FILE *tmp_ = nullptr;
};

// MockListener — records up-call count + cumulative delta. Optional throw mode
// is exercised by the TwoPhaseCommitRollback test.
class MockListener : public omniruntime::mem::PoolListener {
public:
    void allocationChanged(int64_t diff) override
    {
        callCount++;
        totalDelta += diff;
        if (shouldThrow) {
            throw omniruntime::mem::ReservationDenied("denied");
        }
    }
    int callCount = 0;
    int64_t totalDelta = 0;
    bool shouldThrow = false;
};

class ReentrantListener : public MockListener {
public:
    void allocationChanged(int64_t diff) override
    {
        MockListener::allocationChanged(diff);
        if (diff > 0 && pool != nullptr && !entered) {
            entered = true;
            pool->AddMemory(16);
        }
    }
    OmniTaskMemoryPool *pool{nullptr};
    bool entered{false};
};

class TestSpillable : public omniruntime::op::SpillableOperator {
public:
    TestSpillable(OmniTaskMemoryPool *pool, int64_t bytes, std::vector<int> *order, int id)
        : pool_(pool), bytes_(bytes), order_(order), id_(id) {}
    uint64_t reclaimableBytes() const override { return static_cast<uint64_t>(bytes_); }
    int64_t reclaim(int64_t target) override
    {
        order_->push_back(id_);
        const int64_t freed = std::min(bytes_, target);
        bytes_ -= freed;
        pool_->SubMemory(freed);
        return freed;
    }
    bool isInNonReclaimableSection() const noexcept override { return nrs; }
    omniruntime::op::Operator *operatorBase() noexcept override { return nullptr; }
    const char *reclaimableName() const noexcept override { return "test"; }
    bool nrs{false};
private:
    OmniTaskMemoryPool *pool_;
    int64_t bytes_;
    std::vector<int> *order_;
    int id_;
};

// Basic ctor/dtor + Add/Sub round-trip: peak stays monotonic; symmetric Add/Sub
// gives net-zero listener delta.
TEST(OmniTaskMemoryPool, LifecycleBasic)
{
    MockListener listener;
    {
        OmniTaskMemoryPool pool(&listener);
        EXPECT_EQ(pool.currentBytes(), 0);
        EXPECT_EQ(pool.peakBytes(), 0);

        pool.AddMemory(1024);
        EXPECT_EQ(pool.currentBytes(), 1024);
        EXPECT_EQ(pool.peakBytes(), 1024);

        pool.SubMemory(1024);
        EXPECT_EQ(pool.currentBytes(), 0);
        EXPECT_EQ(pool.peakBytes(), 1024);   // peak monotonic
    }
    // Symmetric Add(+1024)/Sub(-1024) ⇒ net delta 0; 2 up-calls total.
    EXPECT_EQ(listener.totalDelta, 0);
    EXPECT_EQ(listener.callCount, 2);
}

// peakBytes monotonic across multiple Adds + a Sub.
TEST(OmniTaskMemoryPool, Stats)
{
    MockListener listener;
    OmniTaskMemoryPool pool(&listener);

    pool.AddMemory(2048);
    pool.AddMemory(4096);
    EXPECT_EQ(pool.currentBytes(), 6144);
    EXPECT_EQ(pool.peakBytes(), 6144);

    pool.SubMemory(2048);
    EXPECT_EQ(pool.currentBytes(), 4096);
    EXPECT_EQ(pool.peakBytes(), 6144);   // peak unchanged by Sub

    // Drain remaining residual so dtor doesn't double-count via stub drain.
    pool.SubMemory(4096);
}

TEST(OmniTaskMemoryPoolReservation, ChargesOnceAndRetainsHeadroomAcrossFree)
{
    MockListener listener;
    OmniTaskMemoryPool pool(&listener);

    ASSERT_TRUE(pool.reserve(4096));
    EXPECT_EQ(listener.totalDelta, static_cast<int64_t>(8) << 20);
    EXPECT_EQ(listener.callCount, 1);
    EXPECT_EQ(pool.reservedBytes(), static_cast<int64_t>(8) << 20);
    EXPECT_EQ(pool.committedBytes(), 0);

    pool.AddMemory(3072);
    EXPECT_EQ(pool.currentBytes(), 3072);
    EXPECT_EQ(listener.totalDelta, static_cast<int64_t>(8) << 20) << "reserved allocation must not be charged twice";
    pool.SubMemory(3072);
    EXPECT_EQ(pool.currentBytes(), 0);
    EXPECT_EQ(listener.totalDelta, static_cast<int64_t>(8) << 20) << "free must not unbill retained reservation";

    pool.releaseUnusedReservation();
    EXPECT_EQ(pool.reservedBytes(), 0);
    EXPECT_EQ(listener.totalDelta, 0);
    EXPECT_EQ(listener.callCount, 2);
}

TEST(OmniTaskMemoryPoolReservation, ExplicitReservationUsesVeloxGrowthQuantum)
{
    constexpr int64_t kQuantum = static_cast<int64_t>(8) << 20;
    EXPECT_EQ(OmniTaskMemoryPool::quantizedReservationSize(1), kQuantum);
    EXPECT_EQ(OmniTaskMemoryPool::quantizedReservationSize(kQuantum), kQuantum);
    EXPECT_EQ(OmniTaskMemoryPool::quantizedReservationSize(kQuantum + 1), 2 * kQuantum);
    EXPECT_EQ(OmniTaskMemoryPool::quantizedReservationSize(0), 0);
    EXPECT_EQ(OmniTaskMemoryPool::quantizedReservationSize(INT64_MAX), -1);
}

TEST(OmniTaskMemoryPoolReservation, AvailableReservationTracksUnusedHeadroom)
{
    constexpr int64_t kQuantum = static_cast<int64_t>(8) << 20;
    MockListener listener;
    OmniTaskMemoryPool pool(&listener);

    EXPECT_EQ(pool.availableReservation(), 0);
    ASSERT_TRUE(pool.reserve(1));
    EXPECT_EQ(pool.availableReservation(), kQuantum);

    pool.AddMemory(1024);
    EXPECT_EQ(pool.availableReservation(), kQuantum - 1024);
    pool.SubMemory(1024);
    EXPECT_EQ(pool.availableReservation(), kQuantum);

    pool.releaseUnusedReservation();
    EXPECT_EQ(pool.availableReservation(), 0);
    EXPECT_EQ(listener.totalDelta, 0);
}

TEST(OmniTaskMemoryPoolReservation, MaybeReserveRoundsDesiredHeadroomBeforeSubtractingAvailable)
{
    constexpr int64_t kQuantum = static_cast<int64_t>(8) << 20;
    MockListener listener;
    OmniTaskMemoryPool pool(&listener);

    ASSERT_TRUE(pool.maybeReserve(1));
    pool.AddMemory(kQuantum / 2);
    EXPECT_EQ(pool.availableReservation(), kQuantum / 2);

    // Velox rounds ten MiB of desired headroom to sixteen MiB first, then
    // subtracts the four MiB already available, acquiring twelve MiB.
    ASSERT_TRUE(pool.maybeReserve(10 * 1024 * 1024));
    EXPECT_EQ(pool.reservedBytes(), 20 * 1024 * 1024);
    EXPECT_EQ(pool.availableReservation(), 2 * kQuantum);
    EXPECT_EQ(listener.totalDelta, 20 * 1024 * 1024);

    pool.SubMemory(kQuantum / 2);
    pool.releaseUnusedReservation();
}

TEST(OmniTaskMemoryPoolReservation, CrossingReservationBillsOnlyExactOvershoot)
{
    constexpr int64_t kQuantum = static_cast<int64_t>(8) << 20;
    MockListener listener;
    OmniTaskMemoryPool pool(&listener);
    ASSERT_TRUE(pool.reserve(100));
    pool.AddMemory(kQuantum + 40);
    EXPECT_EQ(listener.totalDelta, kQuantum + 40);
    EXPECT_EQ(listener.callCount, 2);
    pool.SubMemory(60);
    EXPECT_EQ(pool.currentBytes(), kQuantum - 20);
    EXPECT_EQ(listener.totalDelta, kQuantum);
    pool.releaseAllReservation();
    EXPECT_EQ(pool.reservedBytes(), 0);
    EXPECT_EQ(listener.totalDelta, kQuantum - 20);
    pool.SubMemory(kQuantum - 20);
    EXPECT_EQ(listener.totalDelta, 0);
}

TEST(OmniTaskMemoryPoolReservation, DenialLeavesReservationAndCommitUnchanged)
{
    MockListener listener;
    listener.shouldThrow = true;
    OmniTaskMemoryPool pool(&listener);
    EXPECT_FALSE(pool.reserve(1024));
    EXPECT_EQ(pool.reservedBytes(), 0);
    EXPECT_EQ(pool.currentBytes(), 0);
    EXPECT_EQ(listener.callCount, 1);
}

TEST(OmniTaskMemoryPoolReservation, ReclaimVerboseGatesReservationResults)
{
    {
        ScopedEnv verbose("OMNI_RECLAIM_VERBOSE", nullptr);
        MockListener listener;
        listener.shouldThrow = true;
        OmniTaskMemoryPool pool(&listener);
        CaptureStderr capture;

        EXPECT_FALSE(pool.reserve(1024));
        EXPECT_EQ(capture.Collect().find("[OMNI-RESERVE]"), std::string::npos);
    }
    {
        ScopedEnv verbose("OMNI_RECLAIM_VERBOSE", "0");
        MockListener listener;
        OmniTaskMemoryPool pool(&listener);
        CaptureStderr capture;

        EXPECT_TRUE(pool.reserve(1024));
        EXPECT_EQ(capture.Collect().find("[OMNI-RESERVE]"), std::string::npos);
    }
    {
        ScopedEnv verbose("OMNI_RECLAIM_VERBOSE", "1");
        MockListener listener;
        OmniTaskMemoryPool pool(&listener);
        CaptureStderr capture;

        EXPECT_TRUE(pool.reserve(1024));
        EXPECT_NE(capture.Collect().find("outcome=granted"), std::string::npos);
    }
    {
        ScopedEnv verbose("OMNI_RECLAIM_VERBOSE", "1");
        MockListener listener;
        listener.shouldThrow = true;
        OmniTaskMemoryPool pool(&listener);
        CaptureStderr capture;

        EXPECT_FALSE(pool.reserve(1024));
        EXPECT_NE(capture.Collect().find("outcome=denied"), std::string::npos);
    }
}

TEST(OmniTaskMemoryPoolReservation, DestructorReleasesUsedAndUnusedInOneDelta)
{
    MockListener listener;
    {
        OmniTaskMemoryPool pool(&listener);
        ASSERT_TRUE(pool.reserve(4096));
        pool.AddMemory(1024);
    }
    EXPECT_EQ(listener.totalDelta, 0);
    EXPECT_EQ(listener.callCount, 2);
}

TEST(OmniTaskMemoryPoolReservation, PeakUsesLiveBytesAfterListenerReclaim)
{
    class ReclaimingListener : public MockListener {
    public:
        void allocationChanged(int64_t diff) override
        {
            MockListener::allocationChanged(diff);
            if (diff > 0 && pool != nullptr && reclaimBytes > 0) {
                const auto bytes = reclaimBytes;
                reclaimBytes = 0;
                pool->SubMemory(bytes);
            }
        }
        OmniTaskMemoryPool *pool = nullptr;
        int64_t reclaimBytes = 0;
    } listener;
    OmniTaskMemoryPool pool(&listener);
    listener.pool = &pool;
    pool.AddMemory(100);
    listener.reclaimBytes = 80;
    pool.AddMemory(60);
    EXPECT_EQ(pool.currentBytes(), 80);
    EXPECT_EQ(pool.peakBytes(), 100);
    pool.SubMemory(80);
    EXPECT_EQ(listener.totalDelta, 0);
}

TEST(OmniTaskMemoryPoolReservation, ListenerReentryDoesNotDeadlockOrDoubleCharge)
{
    ReentrantListener listener;
    OmniTaskMemoryPool pool(&listener);
    listener.pool = &pool;
    ASSERT_TRUE(pool.reserve(100));
    EXPECT_EQ(pool.currentBytes(), 16);
    EXPECT_EQ(pool.reservedBytes(), static_cast<int64_t>(8) << 20);
    EXPECT_EQ(listener.totalDelta, static_cast<int64_t>(8) << 20);
    pool.SubMemory(16);
    pool.releaseAllReservation();
    EXPECT_EQ(listener.totalDelta, 0);
}

TEST(OmniTaskMemoryPoolReservation, ReentrantReclaimCannotReportUnavailableHeadroom)
{
    class ReclaimingListener : public MockListener {
    public:
        OmniTaskMemoryPool *pool = nullptr;
        bool reclaimNext = false;
        void allocationChanged(int64_t diff) override
        {
            MockListener::allocationChanged(diff);
            if (diff > 0 && reclaimNext) {
                reclaimNext = false;
                const auto freed = pool->reclaim(50);
                MockListener::allocationChanged(-freed);
            }
        }
    } listener;
    OmniTaskMemoryPool pool(&listener);
    listener.pool = &pool;
    constexpr int64_t requested = int64_t{8} << 20;
    ASSERT_TRUE(pool.reserve(requested));
    pool.AddMemory(requested - 10);
    std::vector<int> order;
    TestSpillable donor(&pool, 50, &order, 1);
    pool.registerSpillable(&donor);
    listener.reclaimNext = true;
    EXPECT_FALSE(pool.reserve(requested));
    EXPECT_EQ(pool.reservedBytes() - pool.currentBytes(), requested - 10);
    EXPECT_EQ(listener.totalDelta, pool.reservedBytes());
    pool.unregisterSpillable(&donor);
    pool.SubMemory(pool.currentBytes());
    pool.releaseAllReservation();
    EXPECT_EQ(listener.totalDelta, 0);
}

TEST(OmniTaskMemoryPoolReservation, ReentrantConsumptionCannotReportUnavailableHeadroom)
{
    ReentrantListener listener;
    OmniTaskMemoryPool pool(&listener);
    listener.pool = &pool;
    constexpr int64_t requested = int64_t{8} << 20;
    EXPECT_FALSE(pool.reserve(requested));
    EXPECT_EQ(pool.currentBytes(), 16);
    EXPECT_EQ(pool.reservedBytes(), requested);
    EXPECT_LT(pool.reservedBytes() - pool.currentBytes(), requested);
    EXPECT_EQ(listener.totalDelta, requested);
    pool.SubMemory(16);
    pool.releaseAllReservation();
    EXPECT_EQ(listener.totalDelta, 0);
}

TEST(OmniTaskMemoryPoolReclaim, EstimatesEachEligibleDonorOnceAndNeverEstimatesProtectedDonors)
{
    class CountingSpillable : public TestSpillable {
    public:
        using TestSpillable::TestSpillable;
        mutable int estimates = 0;
        uint64_t reclaimableBytes() const override
        {
            ++estimates;
            EXPECT_FALSE(nrs);
            return TestSpillable::reclaimableBytes();
        }
    };
    MockListener listener;
    OmniTaskMemoryPool pool(&listener);
    pool.AddMemory(100);
    std::vector<int> order;
    CountingSpillable first(&pool, 20, &order, 1);
    CountingSpillable second(&pool, 30, &order, 2);
    CountingSpillable protectedDonor(&pool, 50, &order, 3);
    protectedDonor.nrs = true;
    pool.registerSpillable(&first);
    pool.registerSpillable(&second);
    pool.registerSpillable(&protectedDonor);
    const auto freed = pool.reclaim(100);
    EXPECT_EQ(freed, 50);
    EXPECT_EQ(first.estimates, 1);
    EXPECT_EQ(second.estimates, 1);
    EXPECT_EQ(protectedDonor.estimates, 0);
    listener.allocationChanged(-freed);
    pool.unregisterSpillable(&first);
    pool.unregisterSpillable(&second);
    pool.unregisterSpillable(&protectedDonor);
    pool.SubMemory(50);
    pool.releaseAllReservation();
    EXPECT_EQ(listener.totalDelta, 0);
}

TEST(OmniTaskMemoryPoolReclaim, LargestFirstSkipsUnsafeAndMeasuresPoolDelta)
{
    MockListener listener;
    OmniTaskMemoryPool pool(&listener);
    pool.AddMemory(100);
    std::vector<int> order;
    TestSpillable small(&pool, 20, &order, 1);
    TestSpillable unsafeLarge(&pool, 60, &order, 2);
    TestSpillable medium(&pool, 40, &order, 3);
    unsafeLarge.nrs = true;
    pool.registerSpillable(&small);
    pool.registerSpillable(&unsafeLarge);
    pool.registerSpillable(&medium);
    {
        omniruntime::op::ReservationRequestSection request(
            nullptr, "ShuffleSplitter", "Splitter::Split", 50);
        EXPECT_EQ(pool.reclaim(50), 50);
    }
    ASSERT_EQ(order.size(), 2);
    EXPECT_EQ(order[0], 3);
    EXPECT_EQ(order[1], 1);
    EXPECT_EQ(pool.currentBytes(), 50);
    const auto diagnostics = pool.reclaimDiagnostics();
    EXPECT_EQ(diagnostics.nonReclaimableSkips, 1);
    EXPECT_EQ(diagnostics.candidateSelections, 2);
    EXPECT_EQ(diagnostics.selfSelections, 0);
    EXPECT_EQ(diagnostics.successfulCalls, 1);
    EXPECT_EQ(diagnostics.freedBytes, 50);
    const auto trace = omniruntime::op::CurrentReclaimTraceSummary();
    EXPECT_STREQ(trace.primaryDonorType, "test");
    EXPECT_EQ(trace.primaryDonorEstimated, 40);
    EXPECT_EQ(trace.primaryDonorPoolFreed, 40);
    EXPECT_EQ(trace.donors, 2);
    EXPECT_EQ(trace.freed, 50);
    EXPECT_STREQ(trace.requestorType, "ShuffleSplitter");
    EXPECT_STREQ(trace.site, "Splitter::Split");
    pool.unregisterSpillable(&small);
    pool.unregisterSpillable(&unsafeLarge);
    pool.unregisterSpillable(&medium);
    pool.SubMemory(50);
}

TEST(OmniTaskMemoryPoolReclaim, ReclaimVerboseGatesSuccessfulCompletion)
{
    {
        ScopedEnv verbose("OMNI_RECLAIM_VERBOSE", nullptr);
        MockListener listener;
        OmniTaskMemoryPool pool(&listener);
        pool.AddMemory(100);
        std::vector<int> order;
        TestSpillable donor(&pool, 100, &order, 1);
        pool.registerSpillable(&donor);
        CaptureStderr capture;

        EXPECT_EQ(pool.reclaim(50), 50);
        EXPECT_EQ(capture.Collect().find("[OMNI-RECLAIM]"), std::string::npos);

        pool.unregisterSpillable(&donor);
        pool.SubMemory(50);
    }
    {
        ScopedEnv verbose("OMNI_RECLAIM_VERBOSE", "1");
        MockListener listener;
        OmniTaskMemoryPool pool(&listener);
        pool.AddMemory(100);
        std::vector<int> order;
        TestSpillable donor(&pool, 100, &order, 1);
        pool.registerSpillable(&donor);
        CaptureStderr capture;

        EXPECT_EQ(pool.reclaim(50), 50);
        const std::string logged = capture.Collect();
        EXPECT_NE(logged.find("[OMNI-RECLAIM] event=trigger"), std::string::npos);
        EXPECT_NE(logged.find("[OMNI-RECLAIM] event=complete"), std::string::npos);

        pool.unregisterSpillable(&donor);
        pool.SubMemory(50);
    }
}

TEST(OmniTaskMemoryPoolReclaim, SmallestSufficientAvoidsOversizedDonor)
{
    MockListener listener;
    OmniTaskMemoryPool pool(&listener);
    pool.AddMemory(1304);
    std::vector<int> order;
    TestSpillable undersized(&pool, 80, &order, 1);
    TestSpillable smallestSufficient(&pool, 200, &order, 2);
    TestSpillable oversized(&pool, 1024, &order, 3);
    pool.registerSpillable(&undersized);
    pool.registerSpillable(&oversized);
    pool.registerSpillable(&smallestSufficient);

    EXPECT_EQ(pool.reclaim(100), 100);

    ASSERT_EQ(order.size(), 1);
    EXPECT_EQ(order[0], 2);
    EXPECT_EQ(pool.currentBytes(), 1204);
    pool.unregisterSpillable(&undersized);
    pool.unregisterSpillable(&smallestSufficient);
    pool.unregisterSpillable(&oversized);
    pool.SubMemory(1204);
}

TEST(OmniTaskMemoryPoolReclaim, FallsBackToLargestFirstWhenNoCandidateIsSufficient)
{
    MockListener listener;
    OmniTaskMemoryPool pool(&listener);
    pool.AddMemory(120);
    std::vector<int> order;
    TestSpillable small(&pool, 20, &order, 1);
    TestSpillable largest(&pool, 60, &order, 2);
    TestSpillable medium(&pool, 40, &order, 3);
    pool.registerSpillable(&small);
    pool.registerSpillable(&largest);
    pool.registerSpillable(&medium);

    EXPECT_EQ(pool.reclaim(90), 90);

    ASSERT_EQ(order.size(), 2);
    EXPECT_EQ(order[0], 2);
    EXPECT_EQ(order[1], 3);
    EXPECT_EQ(pool.currentBytes(), 30);
    pool.unregisterSpillable(&small);
    pool.unregisterSpillable(&largest);
    pool.unregisterSpillable(&medium);
    pool.SubMemory(30);
}

TEST(OmniTaskMemoryPoolReclaim, ShrinkReturnConsumesReservationExactlyOnce)
{
    MockListener listener;
    OmniTaskMemoryPool pool(&listener);
    ASSERT_TRUE(pool.reserve(100));
    pool.AddMemory(100);

    std::vector<int> order;
    TestSpillable donor(&pool, 60, &order, 1);
    pool.registerSpillable(&donor);

    // SubMemory during reclaim deliberately retains listener-billed headroom.
    // Once a donor successfully spills, the old run's scratch reservation is
    // obsolete too. reclaim() drops the marker to the surviving committed
    // bytes and returns the complete billable decrease as the one and only
    // Spark shrink credit; release/destruction must not credit it again.
    constexpr int64_t kQuantum = static_cast<int64_t>(8) << 20;
    EXPECT_EQ(pool.reclaim(60), kQuantum - 40);
    EXPECT_EQ(pool.currentBytes(), 40);
    EXPECT_EQ(pool.reservedBytes(), 40);
    EXPECT_EQ(listener.totalDelta, kQuantum);

    pool.unregisterSpillable(&donor);
    pool.SubMemory(40);
}

TEST(OmniTaskMemoryPoolReclaim, UnreservedUsageIsCreditedOnlyByExternalRepay)
{
    MockListener listener;
    {
        OmniTaskMemoryPool pool(&listener);
        // Model normal allocation growth without an explicit maybeReserve.
        // The listener has billed all 100 bytes while reservedBytes() remains 0.
        pool.AddMemory(100);
        ASSERT_EQ(pool.reservedBytes(), 0);
        ASSERT_EQ(listener.totalDelta, 100);

        std::vector<int> order;
        TestSpillable donor(&pool, 60, &order, 1);
        pool.registerSpillable(&donor);

        const int64_t freed = pool.reclaim(60);
        EXPECT_EQ(freed, 60);
        EXPECT_EQ(pool.currentBytes(), 40);
        EXPECT_EQ(pool.reservedBytes(), 40);
        // Donor frees must not unbill the listener: Spark's external repay is
        // the single credit for the shrink result.
        EXPECT_EQ(listener.totalDelta, 100);

        // Model self.repay(freed) plus the ledger-only block reconciliation.
        listener.allocationChanged(-freed);
        pool.unregisterSpillable(&donor);
        pool.SubMemory(40);
    }
    EXPECT_EQ(listener.totalDelta, 0);
}

TEST(OmniTaskMemoryPoolReclaim, PartialExternalRepayRestoresUnpaidMarker)
{
    MockListener listener;
    {
        OmniTaskMemoryPool pool(&listener);
        pool.AddMemory(100);
        std::vector<int> order;
        TestSpillable donor(&pool, 60, &order, 1);
        pool.registerSpillable(&donor);

        const int64_t nativeFreed = pool.reclaim(60);
        ASSERT_EQ(nativeFreed, 60);
        constexpr int64_t kActuallyRepaid = 50;
        listener.allocationChanged(-kActuallyRepaid);
        pool.restoreUnrepaidReservation(nativeFreed - kActuallyRepaid);
        EXPECT_EQ(pool.currentBytes(), 40);
        EXPECT_EQ(pool.reservedBytes(), 50);

        pool.unregisterSpillable(&donor);
        pool.SubMemory(40);
    }
    // The pool destructor returns the 40 live bytes plus the 10-byte unpaid
    // marker. Nothing remains charged after the partial-repay path.
    EXPECT_EQ(listener.totalDelta, 0);
}

TEST(OmniTaskMemoryPoolReclaim, EmptyRegistryReturnsUnusedBilledHeadroom)
{
    MockListener listener;
    {
        OmniTaskMemoryPool pool(&listener);
        ASSERT_TRUE(pool.reserve(4096));
        pool.AddMemory(100);
        const auto billed = listener.totalDelta;
        const auto freed = pool.reclaim(1);
        EXPECT_EQ(freed, billed - 100);
        EXPECT_EQ(pool.currentBytes(), 100);
        EXPECT_EQ(pool.reservedBytes(), 100);
        // Native reclaim reports credit; the external caller performs repayment.
        EXPECT_EQ(listener.totalDelta, billed);
        listener.allocationChanged(-freed);
        EXPECT_EQ(pool.reclaim(1), 0);
        pool.SubMemory(100);
    }
    EXPECT_EQ(listener.totalDelta, 0);
}

TEST(OmniTaskMemoryPoolReclaim, ZeroByteDonorStillReturnsUnusedHeadroom)
{
    MockListener listener;
    {
        OmniTaskMemoryPool pool(&listener);
        ASSERT_TRUE(pool.reserve(4096));
        pool.AddMemory(100);
        std::vector<int> order;
        TestSpillable donor(&pool, 0, &order, 1);
        pool.registerSpillable(&donor);
        const auto billed = listener.totalDelta;
        const auto freed = pool.reclaim(1);
        EXPECT_EQ(freed, billed - 100);
        EXPECT_EQ(pool.currentBytes(), 100);
        EXPECT_EQ(listener.totalDelta, billed);
        listener.allocationChanged(-freed);
        pool.unregisterSpillable(&donor);
        pool.SubMemory(100);
    }
    EXPECT_EQ(listener.totalDelta, 0);
}

TEST(OmniTaskMemoryPoolReclaim, ZeroResultRestoresOriginalReservationMarker)
{
    MockListener listener;
    OmniTaskMemoryPool pool(&listener);
    pool.AddMemory(100);
    std::vector<int> order;
    TestSpillable donor(&pool, 0, &order, 1);
    pool.registerSpillable(&donor);

    EXPECT_EQ(pool.reclaim(60), 0);
    EXPECT_EQ(pool.currentBytes(), 100);
    EXPECT_EQ(pool.reservedBytes(), 0);

    pool.unregisterSpillable(&donor);
    pool.SubMemory(100);
    EXPECT_EQ(listener.totalDelta, 0);
}

// Pool-bound Allocator gives the allocator hot path a direct, non-TLS route to
// the per-task pool. OmniTaskMemoryScope (the legacy TLS marker) has been
// deleted — pool ownership is recorded at allocation time and frees route via
// the buffer's recorded pool. The PoolBoundAllocator.* / VectorPool.* /
// AlignedBufferPool.* tests below cover this end-to-end.
TEST(OmniTaskMemoryPool, AllocatorRoutesToPool)
{
    MockListener listener;
    OmniTaskMemoryPool pool(&listener);
    Allocator allocator(&pool);   // pool-bound (no TLS)

    EXPECT_EQ(pool.currentBytes(), 0);
    void *ptr = allocator.Alloc(1024);
    EXPECT_EQ(pool.currentBytes(), 1024)
        << "Alloc(1024) on pool-bound allocator must debit the construction-time pool";
    allocator.Free(ptr, 1024);
    EXPECT_EQ(pool.currentBytes(), 0)
        << "Free must credit the construction-time pool";
}

// ----------------------------------------------------------------------------
// Accounting-correctness tests: two-phase CAS rollback, dtor drain
// (incl. H-5 10k pressure), and the JNI-detach guard.
// ----------------------------------------------------------------------------

// Listener throw during AddMemory rolls back via CAS-on-isBlocked_ plus a
// serialized pool-local correction.
// After the throw, currentBytes() returns to its pre-call value (here: 0).
// Listener was called exactly once (the throwing call); since the listener
// increments totalDelta BEFORE throwing, totalDelta == +bytes — but the pool's
// counter is 0 because the rollback removed the rejected allocation.
TEST(OmniTaskMemoryPool, TwoPhaseCommitRollback)
{
    MockListener listener;
    listener.shouldThrow = true;
    OmniTaskMemoryPool pool(&listener);

    EXPECT_THROW(pool.AddMemory(1024), std::runtime_error);
    EXPECT_EQ(pool.currentBytes(), 0)
        << "CAS rollback must restore usedBytes_ after listener-throw";
    EXPECT_EQ(listener.callCount, 1)
        << "Listener was invoked exactly once (the throwing call)";
}

// Pool dtor drains residual via listener_->allocationChanged(-N).
// Symmetric Add(N) + dtor drain(-N) => net totalDelta 0 (no Spark accounting drift).
TEST(OmniTaskMemoryPool, DrainOnDestruction)
{
    MockListener listener;
    {
        OmniTaskMemoryPool pool(&listener);
        pool.AddMemory(4096);
        EXPECT_EQ(pool.currentBytes(), 4096);
        // No explicit SubMemory — relying on dtor to drain on scope exit.
    }
    EXPECT_EQ(listener.totalDelta, 0)
        << "Symmetric Add(+4096) + dtor drain(-4096) must net to 0";
    EXPECT_EQ(listener.callCount, 2)
        << "Listener called once for Add, once for dtor drain";
}

// H-5 hypothesis — 10k-iteration pressure test. Each iteration:
//   construct pool → AddMemory(1 MiB) → ~OmniTaskMemoryPool drains.
// At end, listener.totalDelta MUST equal 0 (no per-iteration leak across 10k
// task lifetime simulations). Falsifying H-5 here would mean Spark accounting
// drifts every task — the q4 SF10 174/174 leak symptom.
//
// Runtime sanity: should complete in well under 5 seconds. If this takes
// minutes, the listener up-call is too heavy and BlockAllocationListener
// coalescing is mandatory before live-Spark testing.
TEST(OmniTaskMemoryPool, DrainNoLeak10k)
{
    MockListener listener;
    for (int i = 0; i < 10000; ++i) {
        OmniTaskMemoryPool pool(&listener);
        pool.AddMemory(1 << 20);   // 1 MiB per task
        // dtor drains on scope exit — no explicit Sub
    }
    EXPECT_EQ(listener.totalDelta, 0)
        << "10k task drain cycles must net listener delta to 0 (H-5)";
    EXPECT_EQ(listener.callCount, 20000)
        << "10k iterations × (1 Add + 1 dtor-drain) = 20k listener calls";
}

// Pool dtor without JNIEnv attached must NOT crash. Simulated via
// the ListenerIsCallable_OverrideForTests static flag: production code never
// touches it; SparkAllocationListener will replace ListenerIsCallable()
// with the real GetEnv()!=JNI_OK check and the override flag goes away.
//
// Expected behaviour: dtor sees ListenerIsCallable() == false, skips the
// listener call, exits cleanly. Drain is intentionally skipped — the JVM
// shutdown / JIT codegen thread case where the drift is acceptable (the JVM
// reclaims everything on its way out anyway).
TEST(OmniTaskMemoryPool, DtorWithoutJNIEnv)
{
    MockListener listener;
    {
        OmniTaskMemoryPool pool(&listener);
        pool.AddMemory(2048);
        EXPECT_EQ(pool.currentBytes(), 2048);
        // Simulate JNIEnv detach (test hook; production path uses real GetEnv).
        OmniTaskMemoryPool::ListenerIsCallable_OverrideForTests().store(false);
    }
    // Restore for subsequent tests (gtest doesn't isolate static state between TESTs).
    OmniTaskMemoryPool::ListenerIsCallable_OverrideForTests().store(true);

    // Drain was skipped — listener saw only the +2048 from Add, no -2048 drain.
    // Drift is documented as acceptable for JVM-shutdown / JIT-thread exit.
    EXPECT_EQ(listener.totalDelta, 2048)
        << "Drain skipped when ListenerIsCallable() == false";
    EXPECT_EQ(listener.callCount, 1)
        << "Listener called for Add but NOT for the skipped dtor drain";
}

// ============================================================================
// Pool-bound Allocator + AlignedBuffer tests
// ============================================================================

// Pool-bound Allocator routes Alloc to the explicit pool (no TLS).
// Constructing Allocator(&pool) and calling Alloc(1024) must debit exactly 1024
// bytes from that pool — no TLS scope (OmniTaskMemoryScope has been deleted).
TEST(PoolBoundAllocator, RoutesAllocToPool)
{
    MockListener listener;
    OmniTaskMemoryPool pool(&listener);
    Allocator allocator(&pool);   // pool-bound; no TLS

    EXPECT_EQ(pool.currentBytes(), 0);
    void *ptr = allocator.Alloc(1024);
    EXPECT_EQ(pool.currentBytes(), 1024)
        << "Alloc(1024) must debit pool directly — no TLS";
    allocator.Free(ptr, 1024);
}

// Free routes to the same pool as Alloc, returning bytes to zero.
TEST(PoolBoundAllocator, RoutesFreeToPool)
{
    MockListener listener;
    OmniTaskMemoryPool pool(&listener);
    Allocator allocator(&pool);

    void *ptr = allocator.Alloc(1024);
    EXPECT_EQ(pool.currentBytes(), 1024);
    allocator.Free(ptr, 1024);
    EXPECT_EQ(pool.currentBytes(), 0)
        << "Free(ptr,1024) must credit pool directly — routes to construction-time pool";
}

TEST(PoolBoundAllocator, PhysicalFailurePreservesExistingAccounting)
{
    MockListener listener;
    OmniTaskMemoryPool pool(&listener);
    Allocator allocator(&pool);
    void *live = allocator.Alloc(1024);
    EXPECT_THROW(allocator.Alloc(int64_t{1} << 62), omniruntime::exception::OmniException);
    EXPECT_EQ(pool.currentBytes(), 1024);
    EXPECT_EQ(listener.totalDelta, 1024);
    allocator.Free(live, 1024);
    EXPECT_EQ(pool.currentBytes(), 0);
    EXPECT_EQ(listener.totalDelta, 0);
}

TEST(PoolBoundAllocator, PhysicalFailurePreservesReservation)
{
    MockListener listener;
    OmniTaskMemoryPool pool(&listener);
    Allocator allocator(&pool);
    ASSERT_TRUE(pool.maybeReserve(1024));
    const auto billedBefore = listener.totalDelta;
    EXPECT_THROW(allocator.Alloc(int64_t{1} << 62), omniruntime::exception::OmniException);
    EXPECT_EQ(pool.currentBytes(), 0);
    EXPECT_EQ(listener.totalDelta, billedBefore);
    pool.releaseUnusedReservation();
    EXPECT_EQ(listener.totalDelta, 0);
}

// Two pool-bound allocators with two separate pools must not cross-talk:
// Alloc on a1 debits only p1; Alloc on a2 debits only p2.
// Routing is purely via the construction-time pool pointer (Velox invariant).
TEST(PoolBoundAllocator, TwoAllocatorsTwoPools)
{
    MockListener listener1;
    MockListener listener2;
    OmniTaskMemoryPool p1(&listener1);
    OmniTaskMemoryPool p2(&listener2);
    Allocator a1(&p1);
    Allocator a2(&p2);

    void *ptr1 = a1.Alloc(512);
    void *ptr2 = a2.Alloc(256);

    EXPECT_EQ(p1.currentBytes(), 512)
        << "p1 must only see a1's 512-byte alloc (no cross-talk, no TLS)";
    EXPECT_EQ(p2.currentBytes(), 256)
        << "p2 must only see a2's 256-byte alloc (no cross-talk, no TLS)";

    a1.Free(ptr1, 512);
    a2.Free(ptr2, 256);
    EXPECT_EQ(p1.currentBytes(), 0);
    EXPECT_EQ(p2.currentBytes(), 0);
}

// AlignedBuffer constructed with an explicit pool-bound Allocator* must route
// its internal Alloc to the associated pool.
TEST(AlignedBufferPool, RoutesViaExplicitAllocator)
{
    MockListener listener;
    OmniTaskMemoryPool pool(&listener);
    Allocator allocator(&pool);

    EXPECT_EQ(pool.currentBytes(), 0);
    {
        AlignedBuffer<int64_t> buf(128, &allocator);
        EXPECT_GT(pool.currentBytes(), 0)
            << "AlignedBuffer(128, &allocator) must debit pool";
    }
    // ~AlignedBuffer calls Release() → allocator.Free() → pool.SubMemory()
    EXPECT_EQ(pool.currentBytes(), 0)
        << "Pool must return to zero after AlignedBuffer destroyed";
}

// AlignedBuffer frees back to its construction-time pool even when no TLS scope
// is active (OmniTaskMemoryScope has been deleted).
TEST(AlignedBufferPool, FreeRoutesToConstructionPool)
{
    MockListener listener1;
    OmniTaskMemoryPool p1(&listener1);
    Allocator a1(&p1);

    {
        AlignedBuffer<int64_t> buf(64, &a1);   // constructed with pool p1
        EXPECT_GT(p1.currentBytes(), 0);
        // No scope active — destructor must still route Free to p1
    }
    EXPECT_EQ(p1.currentBytes(), 0)
        << "Free must route to construction-time pool regardless of active TLS scope";
}

// ============================================================================
// VectorPool.* and LargeStringBufferPool.* — pool-bound Vector/LargeStringBuffer
// ============================================================================

// Vector(vSize, &pool) stores the pool pointer; ~Vector() decrements the
// construction-time pool independent of any ambient TLS state
// (OmniTaskMemoryScope has been deleted; routing is via pool_ const member).
TEST(VectorPool, DtorUsesConstructionPool)
{
    MockListener listener;
    OmniTaskMemoryPool pool(&listener);

    int64_t before = pool.currentBytes();
    {
        // Vector(64, &pool) — pool-bound ctor
        omniruntime::vec::Vector<int64_t> v(64, &pool);
        EXPECT_GT(pool.currentBytes(), before)
            << "Vector ctor must charge bytes to its construction-time pool";
    }  // ~Vector() must decrement pool — no TLS scope active
    EXPECT_EQ(pool.currentBytes(), before)
        << "Pool must return to pre-construction value after ~Vector()";
}

// Cross-scope free structural impossibility: construct a Vector under pool p1;
// destroy it while a different pool p2 is active; p1 is decremented, p2 is
// untouched.
TEST(VectorPool, CrossScopeFreeImpossible)
{
    MockListener l1, l2;
    OmniTaskMemoryPool p1(&l1);
    OmniTaskMemoryPool p2(&l2);

    omniruntime::vec::Vector<int64_t> *v = nullptr;
    {
        // Construct under p1 (pool-bound — no TLS)
        v = new omniruntime::vec::Vector<int64_t>(32, &p1);
        EXPECT_GT(p1.currentBytes(), 0)
            << "Construction under p1 must charge p1";
        EXPECT_EQ(p2.currentBytes(), 0)
            << "p2 must not be charged during p1 construction";
    }

    int64_t p1Before = p1.currentBytes();
    (void)p1Before;
    {
        // OmniTaskMemoryScope has been deleted. The Velox-model invariant is that
        // `~Vector()` routes SubMemory via the buffer's recorded `pool_`,
        // independent of any ambient thread-local state. Even allocating an
        // unrelated p2-bound buffer here must not affect routing of v's frees.
        Allocator p2BoundAllocator(&p2);
        void *unrelated = p2BoundAllocator.Alloc(64);
        delete v;   // ~Vector() must decrement p1, NOT p2
        p2BoundAllocator.Free(unrelated, 64);
    }
    EXPECT_EQ(p1.currentBytes(), 0)
        << "~Vector() must decrement p1 (the construction-time pool) even when concurrently freeing into p2";
    EXPECT_EQ(p2.currentBytes(), 0)
        << "p2 must net to zero (alloc+free symmetric); the p1-owned vector must NOT charge p2 (cross-pool free impossible)";
}

// Dictionary primary ctor (vector.h:655) reached via CreateDictionary's pool!=nullptr
// branch (vector_helper.h:96): a dictionary built with a real pool charges that
// pool — and only that pool — for {Vector + NullsBuffer + AlignedBuffer +
// container capacity}, and the non-sliced dtor credits it all back. The source
// vector is built no-pool so the pool sees ONLY the dictionary's charge; a
// bystander pool proves the charge routes to the passed pool, never ambient.
TEST(VectorPool, DictionaryPrimaryCtorChargesConstructionPool)
{
    MockListener lA, lB, lSrc;
    OmniTaskMemoryPool poolA(&lA);
    OmniTaskMemoryPool poolB(&lB);     // bystander — must never be charged
    OmniTaskMemoryPool poolSrc(&lSrc); // source vector's own pool — keeps poolA seeing only the dictionary

    const int32_t dicSize = 8;
    const int32_t valueSize = 32;
    std::vector<int32_t> indices(valueSize);
    for (int32_t i = 0; i < valueSize; ++i) {
        indices[i] = i % dicSize;
    }
    omniruntime::vec::Vector<int64_t> src(dicSize, &poolSrc);
    for (int32_t j = 0; j < dicSize; ++j) {
        src.SetValue(j, j * 10);
    }

    int64_t before = poolA.currentBytes();
    auto *dictVec = omniruntime::vec::VectorHelper::CreateDictionary(indices.data(), valueSize, &src, false, &poolA);
    EXPECT_GT(poolA.currentBytes(), before)
        << "CreateDictionary(&poolA) must take the pool-bound primary dict ctor (vector.h:655) and charge poolA";
    EXPECT_EQ(poolB.currentBytes(), 0)
        << "the dictionary's charge must route to the passed pool, never a bystander pool";
    delete dictVec;
    EXPECT_EQ(poolA.currentBytes(), before)
        << "non-sliced dict dtor must credit {Vector+NullsBuffer+AlignedBuffer+container} back to poolA";
    EXPECT_EQ(poolB.currentBytes(), 0);
}

// Flat slice ctor (vector.h:420-436): Slice() inherits the parent's pool
// and charges only the reduced {Vector + NullsBuffer} (the shared values buffer
// is NOT re-charged); the isSliced dtor credits exactly that reduced amount.
TEST(VectorPool, FlatSliceCtorChargesReducedAndNetsToBaseline)
{
    MockListener listener;
    OmniTaskMemoryPool pool(&listener);

    int64_t base = pool.currentBytes();
    {
        omniruntime::vec::Vector<int64_t> parent(64, &pool);
        int64_t afterParent = pool.currentBytes();
        int64_t parentCharge = afterParent - base;
        EXPECT_GT(parentCharge, 0) << "parent ctor charges full {Vector+NullsBuffer+2 AlignedBuffers}";
        {
            omniruntime::vec::BaseVector *sliced = parent.Slice(0, 32); // pool-bound slice ctor inherits parent.pool_
            int64_t sliceCharge = pool.currentBytes() - afterParent;
            EXPECT_GT(sliceCharge, 0) << "pool-bound slice ctor must charge its reduced {Vector+NullsBuffer}";
            EXPECT_LT(sliceCharge, parentCharge)
                << "slice charge (Vector+NullsBuffer) must be strictly less than the parent's full charge";
            delete sliced;
            EXPECT_EQ(pool.currentBytes(), afterParent)
                << "isSliced dtor must credit exactly the reduced slice charge (shared values buffer not double-credited)";
        }
    }
    EXPECT_EQ(pool.currentBytes(), base) << "parent dtor nets pool to baseline";
}

// StringView has two independently allocated payloads (the fixed-width view array
// and the out-of-line string arena). Growth replaces both buffers over time, while
// Slice and CopyPositions share the arena. Every path must retain the source task
// pool and return its own charge exactly once.
TEST(VectorPool, StringViewGrowthCopyAndSliceNetToBaseline)
{
    MockListener listener;
    OmniTaskMemoryPool pool(&listener);

    const int64_t baseline = pool.currentBytes();
    {
        omniruntime::vec::Vector<omniruntime::vec::StringView> parent(4, &pool, 16);
        const int64_t afterCtor = pool.currentBytes();
        ASSERT_GT(afterCtor, baseline);
        EXPECT_EQ(parent.GetPool(), &pool);

        // Initialize every slot before forcing arena growth so pointer fix-up never
        // needs to inspect an uninitialized StringView entry.
        for (int32_t i = 0; i < 4; ++i) {
            parent.SetValue(i, omniruntime::vec::StringView("init"));
        }
        parent.SetValue(0, omniruntime::vec::StringView("first_out_of_line_string"));
        parent.SetValue(1, omniruntime::vec::StringView("second_out_of_line_string"));
        EXPECT_GT(pool.currentBytes(), afterCtor)
            << "StringView arena growth must be charged to the construction-time pool";

        const int32_t positions[] = {1, 0};
        const int64_t beforeCopy = pool.currentBytes();
        auto *copy = parent.CopyPositions(positions, 0, 2);
        EXPECT_EQ(copy->GetPool(), &pool);
        EXPECT_GT(pool.currentBytes(), beforeCopy);
        delete copy;
        EXPECT_EQ(pool.currentBytes(), beforeCopy)
            << "CopyPositions must release its owned metadata and view buffer exactly once";

        const int64_t beforeSlice = pool.currentBytes();
        auto *slice = parent.Slice(0, 2);
        EXPECT_EQ(slice->GetPool(), &pool);
        EXPECT_GT(pool.currentBytes(), beforeSlice);
        delete slice;
        EXPECT_EQ(pool.currentBytes(), beforeSlice)
            << "StringView slice must not double-account the shared payload buffers";
    }
    EXPECT_EQ(pool.currentBytes(), baseline)
        << "StringView construction, growth, copy, slice, and destruction must net to baseline";
}

// Dictionary slice ctor (vector.h:700 SliceCtorTag): the elaborate
// reduced-charge dict slice path — Slice() charges only {Vector + NullsBuffer},
// deliberately NOT the shared container capacity (the isSliced dtor skips
// re-crediting it). A prior bug routed dict slices through the full-charge
// primary ctor, a persistent per-slice over-count; this locks the reduced charge.
TEST(VectorPool, DictionarySliceChargesReducedNotContainer)
{
    MockListener listener, lSrc;
    OmniTaskMemoryPool pool(&listener);
    OmniTaskMemoryPool poolSrc(&lSrc); // source vector's own pool — keeps `pool` seeing only the dictionary

    const int32_t dicSize = 8;
    const int32_t valueSize = 32;
    std::vector<int32_t> indices(valueSize);
    for (int32_t i = 0; i < valueSize; ++i) {
        indices[i] = i % dicSize;
    }
    omniruntime::vec::Vector<int64_t> src(dicSize, &poolSrc);
    for (int32_t j = 0; j < dicSize; ++j) {
        src.SetValue(j, j);
    }

    int64_t before = pool.currentBytes();
    auto *dictVec = reinterpret_cast<omniruntime::vec::Vector<omniruntime::vec::DictionaryContainer<int64_t>> *>(
        omniruntime::vec::VectorHelper::CreateDictionary(indices.data(), valueSize, &src, false, &pool));
    int64_t dictCharge = pool.currentBytes() - before;
    ASSERT_GT(dictCharge, 0);

    int64_t beforeSlice = pool.currentBytes();
    auto *sliced = dictVec->Slice(0, 16); // SliceCtorTag reduced-charge ctor (vector.h:700)
    int64_t sliceCharge = pool.currentBytes() - beforeSlice;
    EXPECT_GT(sliceCharge, 0) << "dict slice must charge its reduced {Vector+NullsBuffer}";
    EXPECT_LT(sliceCharge, dictCharge)
        << "dict slice must NOT re-charge the shared container capacity (reduced charge only)";
    delete sliced;
    EXPECT_EQ(pool.currentBytes(), beforeSlice)
        << "dict slice dtor credits exactly its reduced charge (container not double-credited)";
    delete dictVec;
    EXPECT_EQ(pool.currentBytes(), before)
        << "parent dict dtor credits the full charge incl. container back to baseline";
}

TEST(VectorPool, StringViewDictionaryCopyAndSliceKeepConstructionPool)
{
    MockListener listener, sourceListener;
    OmniTaskMemoryPool pool(&listener);
    OmniTaskMemoryPool sourcePool(&sourceListener);

    omniruntime::vec::Vector<omniruntime::vec::StringView> source(3, &sourcePool, 32);
    source.SetValue(0, omniruntime::vec::StringView("apple"));
    source.SetValue(1, omniruntime::vec::StringView("banana_out_of_line"));
    source.SetValue(2, omniruntime::vec::StringView("cherry"));
    int32_t indices[] = {0, 1, 2, 1, 0, 2};

    const int64_t baseline = pool.currentBytes();
    auto *dict = reinterpret_cast<omniruntime::vec::Vector<
        omniruntime::vec::DictionaryContainer<omniruntime::vec::StringView>> *>(
        omniruntime::vec::VectorHelper::CreateDictionary(indices, 6, &source, false, &pool));
    ASSERT_EQ(dict->GetPool(), &pool);
    ASSERT_GT(pool.currentBytes(), baseline);

    int32_t positions[] = {3, 1, 4};
    const int64_t beforeCopy = pool.currentBytes();
    auto *copy = dict->CopyPositions(positions, 0, 3);
    EXPECT_EQ(copy->GetPool(), &pool);
    EXPECT_GT(pool.currentBytes(), beforeCopy);
    delete copy;
    EXPECT_EQ(pool.currentBytes(), beforeCopy);

    const int64_t beforeSlice = pool.currentBytes();
    auto *slice = dict->Slice(1, 3);
    EXPECT_EQ(slice->GetPool(), &pool);
    EXPECT_GT(pool.currentBytes(), beforeSlice);
    delete slice;
    EXPECT_EQ(pool.currentBytes(), beforeSlice);

    delete dict;
    EXPECT_EQ(pool.currentBytes(), baseline)
        << "StringView dictionary copy/slice paths must net to their construction pool";
}

// LargeString wrapper primary ctor (vector.h:819): the
// Vector<LargeStringContainer> wrapper (distinct from the LargeStringBuffer
// helper already covered) charges {Vector+NullsBuffer+AlignedBuffer+container}
// to its construction pool and the non-sliced dtor credits it all back.
TEST(VectorPool, LargeStringWrapperCtorChargesAndNetsToBaseline)
{
    MockListener listener;
    OmniTaskMemoryPool pool(&listener);

    int64_t before = pool.currentBytes();
    {
        omniruntime::vec::Vector<omniruntime::vec::LargeStringContainer<std::string_view>> v(16, &pool);
        EXPECT_GT(pool.currentBytes(), before)
            << "LargeString wrapper primary ctor (vector.h:819) must charge its construction pool";
    }
    EXPECT_EQ(pool.currentBytes(), before)
        << "non-sliced LargeString wrapper dtor must credit {Vector+NullsBuffer+AlignedBuffer+container} back";
}

TEST(VectorPool, LargeStringOffsetGrowthChargesAndNetsToBaseline)
{
    MockListener listener;
    OmniTaskMemoryPool pool(&listener);

    const int64_t before = pool.currentBytes();
    {
        omniruntime::vec::Vector<omniruntime::vec::LargeStringContainer<std::string_view>> v(1, &pool);
        const int64_t initial = pool.currentBytes();
        v.Expand(4096);
        EXPECT_GT(pool.currentBytes(), initial)
            << "offset and null-buffer growth must be charged to the owning pool";
    }
    EXPECT_EQ(pool.currentBytes(), before)
        << "grown offsets must release exactly their final accounted capacity";
}

// ArrayVector(size,&pool) (array_vector.h:35): the pool!=nullptr branch
// routes the offsetsBuffer allocation through the per-task pool's bound
// allocator; ~ArrayVector → ~AlignedBuffer frees it back to the same pool.
TEST(VectorPool, ArrayVectorCtorRoutesOffsetsBufferToPool)
{
    MockListener listener;
    OmniTaskMemoryPool pool(&listener);

    int64_t before = pool.currentBytes();
    {
        omniruntime::vec::ArrayVector av(64, &pool);
        EXPECT_GT(pool.currentBytes(), before)
            << "ArrayVector(size,&pool) must route its offsetsBuffer alloc through the pool (if-pool!=null branch)";
    }
    EXPECT_EQ(pool.currentBytes(), before)
        << "~ArrayVector → ~AlignedBuffer must free the offsetsBuffer back to the same pool";
}

// MapVector(size,&pool) (map_vector.h): same offsetsBuffer pool-routing
// contract as ArrayVector.
TEST(VectorPool, MapVectorCtorRoutesOffsetsBufferToPool)
{
    MockListener listener;
    OmniTaskMemoryPool pool(&listener);

    int64_t before = pool.currentBytes();
    {
        omniruntime::vec::MapVector mv(64, &pool);
        EXPECT_GT(pool.currentBytes(), before)
            << "MapVector(size,&pool) must route its offsetsBuffer alloc through the pool (if-pool!=null branch)";
    }
    EXPECT_EQ(pool.currentBytes(), before)
        << "~MapVector → ~AlignedBuffer must free the offsetsBuffer back to the same pool";
}

// RowVector has no offsets buffer, but its top-level validity NullsBuffer is
// pool-accounted now that the BaseVector allocator cascade is complete.
TEST(VectorPool, RowVectorCtorRoutesValidityBufferToPool)
{
    MockListener listener;
    OmniTaskMemoryPool pool(&listener);

    int64_t before = pool.currentBytes();
    {
        omniruntime::vec::RowVector rv(64, &pool);
        EXPECT_GT(pool.currentBytes(), before)
            << "RowVector(size,&pool) must charge its validity buffer to the supplied pool";
    }
    EXPECT_EQ(pool.currentBytes(), before)
        << "RowVector dtor must release its validity buffer back to the same pool";
}

// LargeStringBuffer(4096, &pool): construction charges the pool; destruction
// (via ~LargeStringBuffer) returns bytes to the same pool; symmetric.
TEST(LargeStringBufferPool, AccountingSymmetric)
{
    MockListener listener;
    OmniTaskMemoryPool pool(&listener);

    int64_t before = pool.currentBytes();
    {
        // LargeStringBuffer(4096, &pool) — pool-bound ctor
        omniruntime::vec::LargeStringBuffer buf(4096, &pool);
        EXPECT_GT(pool.currentBytes(), before)
            << "LargeStringBuffer ctor must charge bytes to its pool";
    }  // ~LargeStringBuffer must decrement pool
    EXPECT_EQ(pool.currentBytes(), before)
        << "Pool must return to pre-construction value after ~LargeStringBuffer()";
}

// ============================================================================
// ArenaAllocatorPool.* and HashmapAllocatorPool.* — pool-bound arena/hashmap
// ============================================================================

// SimpleArenaAllocator constructed with an explicit pool-bound Allocator routes
// its internal chunk allocations to that pool (no TLS).
TEST(ArenaAllocatorPool, RoutesViaPool)
{
    MockListener listener;
    OmniTaskMemoryPool pool(&listener);
    Allocator boundAllocator(&pool);

    int64_t before = pool.currentBytes();
    {
        omniruntime::mem::SimpleArenaAllocator arena(4096, &boundAllocator);
        // Trigger a chunk allocation by allocating more than the chunk will pre-allocate.
        [[maybe_unused]] auto *ptr = arena.Allocate(1024);
        // The arena must have charged memory to the pool (chunk allocation).
        EXPECT_GT(pool.currentBytes(), before)
            << "SimpleArenaAllocator(4096, &allocator) must route chunk alloc to the bound pool";
    }
    // ~SimpleArenaAllocator releases chunks → pool returns toward baseline.
}

TEST(ArenaAllocatorPool, ReclaimableBytesMatchesResetPoolDelta)
{
    MockListener listener;
    OmniTaskMemoryPool pool(&listener);
    Allocator boundAllocator(&pool);
    omniruntime::mem::SimpleArenaAllocator arena(4096, &boundAllocator);

    [[maybe_unused]] auto *first = arena.Allocate(1024);
    EXPECT_EQ(arena.ReclaimableBytesOnReset(), 0)
        << "Reset retains the first arena chunk";
    [[maybe_unused]] auto *second = arena.Allocate(5000);
    const uint64_t advertised = arena.ReclaimableBytesOnReset();
    ASSERT_GT(advertised, 0);
    const int64_t before = pool.currentBytes();
    arena.Reset();
    EXPECT_EQ(before - pool.currentBytes(), static_cast<int64_t>(advertised));
    EXPECT_EQ(arena.ReclaimableBytesOnReset(), 0);
}

TEST(HashmapAllocatorPool, ResetAdvertisesOnlyReleasedNullSlot)
{
    MockListener listener;
    OmniTaskMemoryPool pool(&listener);
    Allocator boundAllocator(&pool);
    omniruntime::op::DefaultHashMap<int64_t, int64_t> map{
        omniruntime::op::OmniHashmapAllocator(&boundAllocator)};

    EXPECT_EQ(map.ReclaimableBytesOnReset(), 0)
        << "Reset retains the main hash table capacity";
    map.EmplaceNullValue(int64_t{0});
    const uint64_t advertised = map.ReclaimableBytesOnReset();
    ASSERT_GT(advertised, 0);
    const int64_t before = pool.currentBytes();
    map.Reset();
    EXPECT_EQ(before - pool.currentBytes(), static_cast<int64_t>(advertised));
}

TEST(HashmapAllocatorPool, ReleaseReturnsCapacityAndExplicitlyRebuilds)
{
    MockListener listener;
    OmniTaskMemoryPool pool(&listener);
    Allocator boundAllocator(&pool);
    omniruntime::op::DefaultHashMap<int64_t, int64_t> map{
        omniruntime::op::OmniHashmapAllocator(&boundAllocator)};

    for (int64_t cycle = 0; cycle < 3; ++cycle) {
        if (cycle > 0) {
            map.ReinitializeAfterRelease();
        }
        auto inserted = map.Emplace(int64_t{cycle + 1});
        ASSERT_TRUE(inserted.IsInsert());
        inserted.SetValue(cycle + 10);
        map.EmplaceNullValue(int64_t{0}).SetValue(cycle + 20);

        const uint64_t advertised = map.ReclaimableBytesOnRelease();
        ASSERT_GT(advertised, 0);
        const int64_t before = pool.currentBytes();
        map.Release();
        EXPECT_EQ(before - pool.currentBytes(), static_cast<int64_t>(advertised));
        EXPECT_EQ(map.GetAllocatedBytes(), 0);
        EXPECT_EQ(map.GetElementsSize(), 0);
    }

    EXPECT_EQ(pool.currentBytes(), 0);
}

// OmniHashmapAllocator(pool-bound Allocator*) compiles and routes; the no-arg
// default ctor no longer exists (the explicit ctor is the only entry point).
TEST(HashmapAllocatorPool, ExplicitCtor)
{
    MockListener listener;
    OmniTaskMemoryPool pool(&listener);
    Allocator boundAllocator(&pool);

    int64_t before = pool.currentBytes();
    {
        // OmniHashmapAllocator must only accept an explicit Allocator* — no default.
        omniruntime::op::OmniHashmapAllocator hmalloc(&boundAllocator);

        // Allocate a small buffer — should debit pool.
        uint8_t *buf = nullptr;
        hmalloc.Allocate(256, &buf);
        EXPECT_GT(pool.currentBytes(), before)
            << "OmniHashmapAllocator(poolAlloc).Allocate must debit the bound pool";

        // Release — returns bytes to the pool.
        hmalloc.Release(buf, 256);
        EXPECT_EQ(pool.currentBytes(), before)
            << "OmniHashmapAllocator.Release must credit the bound pool symmetrically";
    }
}

// ============================================================================
// On-demand global memory snapshot.
//
// GetGlobalAccountedMemory walks live pools only when the compatibility getter is
// called. Allocation/free must not update a contended process-global cache line.
// ============================================================================

TEST(GlobalMemTracker, LimitRoundTrip)
{
    SetGlobalMemoryLimit(123456);
    EXPECT_EQ(GetGlobalMemoryLimit(), 123456) << "limit setter/getter must round-trip";
    SetGlobalMemoryLimit(GLOBAL_MEM_UNLIMITED);   // restore process-global default
    EXPECT_EQ(GetGlobalMemoryLimit(), GLOBAL_MEM_UNLIMITED);
}

TEST(GlobalMemTracker, SnapshotSumsOnlyLivePools)
{
    MockListener listener;
    int64_t base = GetGlobalAccountedMemory();
    {
        OmniTaskMemoryPool pool(&listener);
        pool.AddMemory(4096);
        EXPECT_EQ(GetGlobalAccountedMemory(), base + 4096)
            << "snapshot must include each live pool's current bytes";
    }
    EXPECT_EQ(GetGlobalAccountedMemory(), base)
        << "destroyed pools must disappear from the compatibility snapshot";
}

TEST(GlobalMemTracker, AddMemoryRollbackIsVisibleInSnapshot)
{
    MockListener listener;
    listener.shouldThrow = true;
    int64_t base = GetGlobalAccountedMemory();
    OmniTaskMemoryPool pool(&listener);

    EXPECT_THROW(pool.AddMemory(1024), std::runtime_error);
    EXPECT_EQ(pool.currentBytes(), 0)
        << "per-task counter must roll back after listener-throw";
    EXPECT_EQ(GetGlobalAccountedMemory(), base)
        << "on-demand snapshot must observe the rolled-back pool value";
}

// ============================================================================
// AlignedBuffer::AllocateReuse
//
// The grow branch (capacity < newCapacity) does Free(old capacity) + Alloc(new
// capacity); if asymmetric it corrupts pool accounting (double-debit / stale
// capacity). AllocateReuse had ZERO references under core/test/. This exercises
// the reuse (no-op) and grow (Free+Alloc) branches against a real pool.
// ============================================================================
TEST(AlignedBufferReuse, GrowBranchAccountingSymmetric)
{
    MockListener listener;
    OmniTaskMemoryPool pool(&listener);
    Allocator allocator(&pool);
    int64_t base = pool.currentBytes();   // 0
    {
        // ctor allocs 16 * sizeof(int64_t) = 128 bytes (buffer != nullptr after this).
        AlignedBuffer<int64_t> buf(16, &allocator);
        EXPECT_EQ(pool.currentBytes(), base + 128)
            << "ctor must charge 16*8 = 128 bytes to the bound pool";

        // Reuse branch: smaller request (64 <= 128) → no Free/Alloc, pool untouched.
        buf.AllocateReuse(8, /*zerofill=*/true);
        EXPECT_EQ(pool.currentBytes(), base + 128)
            << "reuse branch (capacity >= newCapacity) must not touch the pool";

        // Grow branch: larger request (256 > 128) → Free(128) then Alloc(256).
        buf.AllocateReuse(32, /*zerofill=*/false);
        EXPECT_EQ(pool.currentBytes(), base + 256)
            << "grow must Free old capacity and Alloc new — net = newCapacity (symmetric debit/credit)";
    }
    EXPECT_EQ(pool.currentBytes(), base)
        << "~AlignedBuffer must credit the final capacity back to the pool";
}

// ============================================================================
// Concurrency — getBoundAllocator std::call_once + serialized accounting / peak
// updates under concurrent AddMemory. All assertions are on
// deterministic POST-JOIN invariants (no timing-dependent / flaky checks).
// ============================================================================

// Thread-safe listener: the plain MockListener above races its counters under
// concurrent up-calls, so concurrency tests use atomic accumulators.
class AtomicListener : public omniruntime::mem::PoolListener {
public:
    void allocationChanged(int64_t diff) override
    {
        callCount.fetch_add(1, std::memory_order_relaxed);
        totalDelta.fetch_add(diff, std::memory_order_relaxed);
        if (shouldThrow.load(std::memory_order_relaxed)) {
            throw omniruntime::mem::ReservationDenied("denied");
        }
    }
    std::atomic<int> callCount{0};
    std::atomic<int64_t> totalDelta{0};
    std::atomic<bool> shouldThrow{false};
};

// N threads race the first getBoundAllocator(). The std::call_once must
// construct exactly ONE Allocator — every thread (and a post-join call) must see
// the same non-null pointer. A check-then-create race would hand some threads a
// different (or dangling) instance.
TEST(OmniTaskMemoryPoolConcurrency, GetBoundAllocatorCallOnceReturnsSingleInstance)
{
    AtomicListener listener;
    OmniTaskMemoryPool pool(&listener);

    const int N = 16;
    std::vector<Allocator *> results(N, nullptr);
    std::atomic<int> ready{0};
    std::atomic<bool> go{false};
    std::vector<std::thread> threads;
    for (int i = 0; i < N; ++i) {
        threads.emplace_back([&, i] {
            ready.fetch_add(1, std::memory_order_acq_rel);
            while (!go.load(std::memory_order_acquire)) { /* spin until released together */ }
            results[i] = pool.getBoundAllocator();
        });
    }
    while (ready.load(std::memory_order_acquire) < N) { /* wait for all threads to arm */ }
    go.store(true, std::memory_order_release); // release them as simultaneously as possible
    for (auto &t : threads) {
        t.join();
    }

    Allocator *first = results[0];
    EXPECT_NE(first, nullptr);
    for (int i = 0; i < N; ++i) {
        EXPECT_EQ(results[i], first)
            << "all concurrent getBoundAllocator() calls must return the single call_once-constructed Allocator";
    }
    EXPECT_EQ(pool.getBoundAllocator(), first) << "post-join call returns the same instance";
}

// N threads each AddMemory(K) `iters` times concurrently. usedBytes_ must
// lose no update (currentBytes == N*iters*K exactly), and the serialized peak
// update must capture the final max under contention. Draining keeps the peak (it's
// a high-water mark) and returns currentBytes to zero.
TEST(OmniTaskMemoryPoolConcurrency, ConcurrentAddMemoryNoLostUpdatesPeakExact)
{
    AtomicListener listener;
    OmniTaskMemoryPool pool(&listener);

    const int N = 8;
    const int iters = 4000;
    const int64_t K = 64;
    const int64_t expected = static_cast<int64_t>(N) * iters * K;

    int64_t globalBefore = mem::GetGlobalAccountedMemory();

    std::atomic<int> ready{0};
    std::atomic<bool> go{false};
    std::vector<std::thread> threads;
    for (int i = 0; i < N; ++i) {
        threads.emplace_back([&] {
            ready.fetch_add(1, std::memory_order_acq_rel);
            while (!go.load(std::memory_order_acquire)) { /* spin */ }
            for (int j = 0; j < iters; ++j) {
                pool.AddMemory(K);
            }
        });
    }
    while (ready.load(std::memory_order_acquire) < N) { /* arm */ }
    go.store(true, std::memory_order_release);
    for (auto &t : threads) {
        t.join();
    }

    EXPECT_EQ(pool.currentBytes(), expected) << "concurrent AddMemory must not lose updates";
    EXPECT_EQ(pool.peakBytes(), expected) << "serialized peak update must capture the final max total exactly";
    EXPECT_EQ(listener.totalDelta.load(), expected) << "every AddMemory issues exactly one +K listener up-call";
    EXPECT_EQ(listener.callCount.load(), N * iters);

    for (int j = 0; j < N * iters; ++j) {
        pool.SubMemory(K);
    }
    EXPECT_EQ(pool.currentBytes(), 0);
    EXPECT_EQ(pool.peakBytes(), expected) << "peakBytes is a high-water mark; SubMemory must not lower it";
    EXPECT_EQ(mem::GetGlobalAccountedMemory(), globalBefore)
        << "on-demand snapshot must return to its pre-test baseline";
}

// N-thrower two-phase rollback: extends the single-thrower
// TwoPhaseCommitRollback to the concurrent case. When the listener throws on
// every concurrent AddMemory, each thrower must roll back exactly its OWN bytes
// and leave usedBytes_ at zero with no lost/double rollback under the
// isBlocked_ CAS race.
// (isBlocked_ itself is private with no getter / no test-reachable effect; the
// externally-observable invariant is the symmetric rollback asserted here.)
TEST(OmniTaskMemoryPoolConcurrency, ConcurrentListenerThrowRollbackIsSymmetric)
{
    AtomicListener listener;
    listener.shouldThrow.store(true);
    OmniTaskMemoryPool pool(&listener);

    const int N = 16;
    const int64_t K = 1024;
    int64_t globalBefore = mem::GetGlobalAccountedMemory();

    std::atomic<int> ready{0};
    std::atomic<bool> go{false};
    std::atomic<int> threw{0};
    std::vector<std::thread> threads;
    for (int i = 0; i < N; ++i) {
        threads.emplace_back([&] {
            ready.fetch_add(1, std::memory_order_acq_rel);
            while (!go.load(std::memory_order_acquire)) { /* spin */ }
            try {
                pool.AddMemory(K);
            } catch (...) {
                threw.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    while (ready.load(std::memory_order_acquire) < N) { /* arm */ }
    go.store(true, std::memory_order_release);
    for (auto &t : threads) {
        t.join();
    }

    EXPECT_EQ(threw.load(), N) << "every concurrent AddMemory must propagate the listener throw";
    EXPECT_EQ(pool.currentBytes(), 0)
        << "each of N concurrent throwers must roll back exactly its own bytes";
    EXPECT_EQ(listener.callCount.load(), N) << "listener invoked exactly once per thrower";
    EXPECT_EQ(mem::GetGlobalAccountedMemory(), globalBefore)
        << "on-demand snapshot must observe all rollbacks";
}

// Compound JNI vector allocation keeps each billing transition synchronous.
class VectorAccountingListener final : public PoolListener {
public:
    void allocationChanged(int64_t diff) override
    {
        if (diff > 0) {
            ++positiveCalls;
            if (positiveCalls == denyAt) {
                throw ReservationDenied("vector allocation denied");
            }
        }
        bytes += diff;
        if (diff > 0 && reenter && !entered) {
            entered = true;
            pool->AddMemory(16);
            pool->SubMemory(16);
        }
        if (diff > 0) {
            published.push_back(pool->currentBytes());
        }
    }
    OmniTaskMemoryPool *pool{nullptr};
    int64_t bytes{0};
    int positiveCalls{0};
    int denyAt{-1};
    bool reenter{false};
    bool entered{false};
    std::vector<int64_t> published;
};

TEST(VectorPool, CompoundAccountingPublishesEachChargeAndCrossThreadFreeBalances)
{
    VectorAccountingListener listener;
    OmniTaskMemoryPool pool(&listener);
    listener.pool = &pool;
    std::unique_ptr<omniruntime::vec::Vector<int32_t>> vector;
    {
        auto accounting = pool.accountingScope();
        vector = std::make_unique<omniruntime::vec::Vector<int32_t>>(512, accounting);
        EXPECT_EQ(listener.positiveCalls, 3);
        EXPECT_EQ(listener.published.size(), 3U);
        ASSERT_EQ(listener.published.size(), 3U);
        EXPECT_LT(listener.published[0], listener.published[1]);
        EXPECT_LT(listener.published[1], listener.published[2]);
        EXPECT_EQ(listener.bytes, pool.currentBytes());
        EXPECT_EQ(pool.peakBytes(), pool.currentBytes());
    }
    std::thread destroy([&]() { vector.reset(); });
    destroy.join();
    EXPECT_EQ(pool.currentBytes(), 0);
    EXPECT_EQ(listener.bytes, 0);
}

TEST(VectorPool, CompoundAccountingDenialUnwindsEveryConstructionStepAndReleasesLock)
{
    for (int denyAt = 1; denyAt <= 3; ++denyAt) {
        VectorAccountingListener listener;
        OmniTaskMemoryPool pool(&listener);
        listener.pool = &pool;
        pool.AddMemory(64);
        listener.positiveCalls = 0;
        listener.denyAt = denyAt;
        EXPECT_THROW({
            auto accounting = pool.accountingScope();
            auto vector = std::make_unique<omniruntime::vec::Vector<int64_t>>(512, accounting);
        }, ReservationDenied);
        EXPECT_EQ(pool.currentBytes(), 64);
        EXPECT_EQ(listener.bytes, 64);
        listener.denyAt = -1;
        std::thread retry([&]() {
            auto accounting = pool.accountingScope();
            auto vector = std::make_unique<omniruntime::vec::Vector<int64_t>>(512, accounting);
        });
        retry.join();
        EXPECT_EQ(pool.currentBytes(), 64);
        EXPECT_EQ(listener.bytes, 64);
        pool.SubMemory(64);
        EXPECT_EQ(listener.bytes, 0);
    }
}

TEST(VectorPool, CompoundAccountingAllowsSynchronousListenerReentry)
{
    VectorAccountingListener listener;
    OmniTaskMemoryPool pool(&listener);
    listener.pool = &pool;
    listener.reenter = true;
    {
        auto accounting = pool.accountingScope();
        auto vector = std::make_unique<omniruntime::vec::Vector<int32_t>>(512, accounting);
        EXPECT_TRUE(listener.entered);
        EXPECT_EQ(listener.positiveCalls, 4);
        EXPECT_EQ(listener.bytes, pool.currentBytes());
    }
    EXPECT_EQ(pool.currentBytes(), 0);
    EXPECT_EQ(listener.bytes, 0);
}

TEST(OmniTaskMemoryPoolConcurrency, CompoundVectorCreationRetainsExactSharedPoolAccounting)
{
    VectorAccountingListener listener;
    OmniTaskMemoryPool pool(&listener);
    listener.pool = &pool;
    std::vector<std::thread> threads;
    for (int thread = 0; thread < 8; ++thread) {
        threads.emplace_back([&]() {
            for (int i = 0; i < 200; ++i) {
                // Exercise call_once both before and during the accounting scope.
                EXPECT_NE(pool.getBoundAllocator(), nullptr);
                auto accounting = pool.accountingScope();
                auto vector = std::make_unique<omniruntime::vec::Vector<int64_t>>(512, accounting);
                EXPECT_EQ(listener.bytes, pool.currentBytes());
            }
        });
    }
    for (auto &thread : threads) {
        thread.join();
    }
    EXPECT_EQ(pool.currentBytes(), 0);
    EXPECT_EQ(listener.bytes, 0);
    EXPECT_EQ(listener.positiveCalls, 8 * 200 * 3);
}

} // namespace omniruntime::mem::test

TEST(OmniTaskMemoryPoolReservation, ExternalRepaymentCallbackFailurePreservesReservation)
{
    omniruntime::mem::OmniTaskMemoryPool pool(nullptr);
    pool.restoreUnrepaidReservation(100);
    EXPECT_THROW(pool.restoreUnrepaidReservation(25, [] {
        throw std::underflow_error("downstream ledger rejected repayment");
    }), std::underflow_error);
    EXPECT_EQ(pool.reservedBytes(), 100);
    bool called = false;
    EXPECT_THROW(pool.restoreUnrepaidReservation(INT64_MAX, [&] { called = true; }),
        std::overflow_error);
    EXPECT_FALSE(called);
    pool.restoreUnrepaidReservation(0, [&] { called = true; });
    EXPECT_TRUE(called);
    EXPECT_EQ(pool.reservedBytes(), 100);
    pool.restoreUnrepaidReservation(25, [] {});
    EXPECT_EQ(pool.reservedBytes(), 125);
    pool.releaseUnusedReservation();
}

namespace omniruntime::mem::test {


TEST(OmniTaskMemoryPoolConcurrency, NestedAccountingOwnershipRestoresDifferentPool)
{
    OmniTaskMemoryPool a(nullptr), b(nullptr);
    {
        auto outer = a.accountingScope();
        {
            auto nested = a.accountingScope();
            a.AddMemory(10);
            {
                auto other = b.accountingScope();
                b.AddMemory(20);
                a.AddMemory(30); // A -> B -> A uses the recursive mutex safely.
                a.SubMemory(30);
            }
            a.SubMemory(10);
        }
        a.AddMemory(7);
    }
    a.SubMemory(7);
    b.SubMemory(20);
    EXPECT_EQ(a.currentBytes(), 0);
    EXPECT_EQ(b.currentBytes(), 0);
    EXPECT_EQ(a.peakBytes(), 40);
    EXPECT_EQ(b.peakBytes(), 20);
}

TEST(OmniTaskMemoryPoolConcurrency, CompoundAccountingStillBlocksOtherThread)
{
    OmniTaskMemoryPool pool(nullptr);
    std::promise<void> started, done;
    auto startedFuture = started.get_future();
    auto doneFuture = done.get_future();
    std::thread worker;
    {
        auto outer = pool.accountingScope();
        {
            auto nested = pool.accountingScope();
            pool.AddMemory(64);
        }
        worker = std::thread([&] {
            started.set_value();
            pool.AddMemory(32);
            done.set_value();
        });
        startedFuture.wait();
        EXPECT_EQ(doneFuture.wait_for(std::chrono::milliseconds(50)), std::future_status::timeout);
        EXPECT_EQ(pool.currentBytes(), 64);
    }
    EXPECT_EQ(doneFuture.wait_for(std::chrono::seconds(2)), std::future_status::ready);
    worker.join();
    EXPECT_EQ(pool.currentBytes(), 96);
    pool.SubMemory(96);
}

}

namespace omniruntime::mem::test {
TEST(VectorPool, ScopedAccountingRejectsWrongPoolAndThreadBeforeCharging)
{
    OmniTaskMemoryPool a(nullptr), b(nullptr);
    auto scope = a.accountingScope();
    EXPECT_THROW(b.AddMemoryWithScope(32, scope), std::logic_error);
    std::thread wrongThread([&] {
        EXPECT_THROW(a.AddMemoryWithScope(32, scope), std::logic_error);
    });
    wrongThread.join();
    EXPECT_EQ(a.currentBytes(), 0);
    EXPECT_EQ(b.currentBytes(), 0);
    a.AddMemoryWithScope(16, scope);
    a.SubMemory(16);
}
TEST(VectorPool, ScopedAllocatorRejectsDifferentOrNullBoundPool)
{
    OmniTaskMemoryPool a(nullptr), b(nullptr);
    Allocator other(&b), unbound(nullptr);
    auto scope = a.accountingScope();
    EXPECT_THROW(other.Alloc(32, scope), std::logic_error);
    EXPECT_THROW(unbound.Alloc(32, scope), std::logic_error);
    EXPECT_EQ(a.currentBytes(), 0);
    EXPECT_EQ(b.currentBytes(), 0);
}
}

namespace omniruntime::mem::test {
TEST(PoolBoundAllocator, ScopedPhysicalFailurePreservesExistingAccounting)
{
    MockListener listener;
    OmniTaskMemoryPool pool(&listener);
    Allocator allocator(&pool);
    void *live = allocator.Alloc(1024);
    auto scope = pool.accountingScope();
    EXPECT_THROW(allocator.Alloc(int64_t{1} << 62, scope), omniruntime::exception::OmniException);
    EXPECT_EQ(pool.currentBytes(), 1024);
    EXPECT_EQ(listener.totalDelta, 1024);
    allocator.Free(live, 1024);
    EXPECT_EQ(pool.currentBytes(), 0);
    EXPECT_EQ(listener.totalDelta, 0);
}
TEST(PoolBoundAllocator, ScopedPhysicalFailurePreservesReservation)
{
    MockListener listener;
    OmniTaskMemoryPool pool(&listener);
    Allocator allocator(&pool);
    ASSERT_TRUE(pool.maybeReserve(1024));
    const auto billedBefore = listener.totalDelta;
    auto scope = pool.accountingScope();
    EXPECT_THROW(allocator.Alloc(int64_t{1} << 62, scope), omniruntime::exception::OmniException);
    EXPECT_EQ(pool.currentBytes(), 0);
    EXPECT_EQ(listener.totalDelta, billedBefore);
    pool.releaseUnusedReservation();
    EXPECT_EQ(listener.totalDelta, 0);
}

TEST(VectorPool, NumericFactoriesKeepTypeOwnerAndThreeSynchronousCharges)
{
    using namespace omniruntime::type;
    const DataTypeId types[] = {OMNI_BOOLEAN, OMNI_BYTE, OMNI_SHORT, OMNI_INT,
        OMNI_DATE32, OMNI_LONG, OMNI_DATE64, OMNI_TIMESTAMP, OMNI_FLOAT, OMNI_DOUBLE,
        OMNI_DECIMAL64, OMNI_DECIMAL128};
    for (auto id : types) {
        VectorAccountingListener listener;
        OmniTaskMemoryPool pool(&listener);
        listener.pool = &pool;
        {
            std::unique_ptr<omniruntime::vec::BaseVector> v(
                omniruntime::vec::VectorHelper::CreateVector(&pool, omniruntime::vec::OMNI_FLAT, id, 512));
            EXPECT_EQ(v->GetPool(), &pool);
            EXPECT_EQ(v->GetTypeId(), id);
            EXPECT_EQ(listener.positiveCalls, 3);
            EXPECT_EQ(listener.bytes, pool.currentBytes());
        }
        EXPECT_EQ(pool.currentBytes(), 0);
        EXPECT_EQ(listener.bytes, 0);
        listener.positiveCalls = 0;
        {
            auto v = omniruntime::vec::VectorHelper::CreateFlatVectorShared(&pool, id, 512);
            EXPECT_EQ(v->GetPool(), &pool);
            EXPECT_EQ(v->GetTypeId(), id);
            EXPECT_EQ(listener.positiveCalls, 3);
        }
        EXPECT_EQ(pool.currentBytes(), 0);
        EXPECT_EQ(listener.bytes, 0);
    }
}
TEST(VectorPool, NumericFactoryDenialUnwindsAllThreeCharges)
{
    for (int denyAt = 1; denyAt <= 3; ++denyAt) {
        VectorAccountingListener listener;
        OmniTaskMemoryPool pool(&listener);
        listener.pool = &pool;
        pool.AddMemory(64);
        listener.positiveCalls = 0;
        listener.denyAt = denyAt;
        EXPECT_THROW({
            std::unique_ptr<omniruntime::vec::BaseVector> v(
                omniruntime::vec::VectorHelper::CreateVector(&pool, omniruntime::vec::OMNI_FLAT,
                    omniruntime::type::OMNI_LONG, 512));
        }, ReservationDenied);
        EXPECT_EQ(pool.currentBytes(), 64);
        EXPECT_EQ(listener.bytes, 64);
        listener.denyAt = -1;
        pool.SubMemory(64);
        EXPECT_EQ(listener.bytes, 0);
    }
}
} // namespace omniruntime::mem::test
