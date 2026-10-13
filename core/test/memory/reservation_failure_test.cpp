/* Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved. */
#include <functional>
#include "gtest/gtest.h"
#include "memory/omni_task_memory_pool.h"
#include "memory/pool_listener.h"
#include "operator/spillable_operator.h"

namespace omniruntime::mem::test {
namespace {
class BillingListener final : public omniruntime::mem::PoolListener {
public:
    int64_t billed = 0;
    std::function<void()> onBorrow;
    void allocationChanged(int64_t diff) override {
        if (diff > 0 && onBorrow) {
            auto action = std::move(onBorrow);
            onBorrow = nullptr;
            action();
        }
        if (billed + diff < 0) throw std::underflow_error("over-repayment");
        billed += diff;
    }
};
class CallbackDonor final : public op::SpillableOperator {
public:
    std::function<int64_t()> action;
    uint64_t reclaimableBytes() const override { return 16; }
    int64_t reclaim(int64_t) override { return action(); }
    bool isInNonReclaimableSection() const noexcept override { return false; }
    const char* reclaimableName() const noexcept override { return "test-donor"; }
    op::Operator* operatorBase() noexcept override { return nullptr; }
};
} // namespace

TEST(OmniTaskMemoryPoolFailure, ReentrantRepaymentSurvivesAddDenial) {
    for (int64_t repaid : {int64_t{0}, int64_t{2}, int64_t{4}}) {
        BillingListener listener;
        OmniTaskMemoryPool pool(&listener);
        pool.AddMemory(8);
        CallbackDonor donor;
        donor.action = [&] { pool.SubMemory(4); return 4; };
        pool.registerSpillable(&donor);
        listener.onBorrow = [&] {
            const int64_t freed = pool.reclaim(4);
            EXPECT_EQ(freed, 4);
            listener.billed -= repaid;
            pool.restoreUnrepaidReservation(freed - repaid);
            throw ReservationDenied("after reclaim");
        };
        EXPECT_THROW(pool.AddMemory(8), ReservationDenied);
        EXPECT_EQ(pool.currentBytes(), 4);
        EXPECT_EQ(pool.reservedBytes(), 8 - repaid);
        EXPECT_EQ(listener.billed, 8 - repaid);
        EXPECT_NO_THROW(pool.releaseUnusedReservation());
        pool.SubMemory(4);
        // Reclaim retains committed capacity as headroom until explicit release.
        EXPECT_NO_THROW(pool.releaseUnusedReservation());
        EXPECT_EQ(pool.reservedBytes(), 0);
        EXPECT_EQ(listener.billed, 0);
        pool.unregisterSpillable(&donor);
    }
}

TEST(OmniTaskMemoryPoolFailure, ZeroReclaimDoesNotRollbackExistingHeadroom) {
    BillingListener listener;
    OmniTaskMemoryPool pool(&listener);
    ASSERT_TRUE(pool.maybeReserve(1));
    const int64_t originalReservation = pool.reservedBytes();
    pool.AddMemory(4);
    CallbackDonor donor;
    donor.action = [] { return 0; };
    pool.registerSpillable(&donor);
    listener.onBorrow = [&] {
        EXPECT_EQ(pool.reclaim(4), 0);
        throw ReservationDenied("zero reclaim");
    };
    EXPECT_THROW(pool.AddMemory(originalReservation), ReservationDenied);
    EXPECT_EQ(pool.currentBytes(), 4);
    EXPECT_EQ(pool.reservedBytes(), originalReservation);
    EXPECT_EQ(listener.billed, originalReservation);
    pool.unregisterSpillable(&donor);
    pool.SubMemory(4);
    pool.releaseUnusedReservation();
    EXPECT_EQ(listener.billed, 0);
}

TEST(OmniTaskMemoryPoolFailure, NestedSuccessfulAddSurvivesOuterDenial) {
    BillingListener listener;
    OmniTaskMemoryPool pool(&listener);
    pool.AddMemory(8);
    listener.onBorrow = [&] {
        pool.AddMemory(4);
        throw ReservationDenied("outer denial");
    };
    EXPECT_THROW(pool.AddMemory(8), ReservationDenied);
    EXPECT_EQ(pool.currentBytes(), 12);
    EXPECT_EQ(listener.billed, 12);
    pool.SubMemory(12);
    pool.releaseUnusedReservation();
    EXPECT_EQ(listener.billed, 0);
}

TEST(OmniTaskMemoryPoolFailure, DeniedOvershootPreservesExistingHeadroomAndRepayment) {
    constexpr int64_t kMiB = int64_t{1} << 20;
    for (int64_t repaid : {0, 1, 2}) {
        BillingListener listener;
        OmniTaskMemoryPool pool(&listener);
        ASSERT_TRUE(pool.maybeReserve(1));
        pool.AddMemory(4 * kMiB);
        CallbackDonor donor;
        donor.action = [&] { pool.SubMemory(2 * kMiB); return 2 * kMiB; };
        pool.registerSpillable(&donor);
        listener.onBorrow = [&] {
            const int64_t freed = pool.reclaim(1);
            EXPECT_EQ(freed, 2 * kMiB);
            listener.billed -= repaid * kMiB;
            pool.restoreUnrepaidReservation(freed - repaid * kMiB);
            throw ReservationDenied("overshoot denied after reclaim");
        };
        EXPECT_THROW(pool.AddMemory(8 * kMiB), ReservationDenied);
        EXPECT_EQ(pool.currentBytes(), 2 * kMiB);
        EXPECT_EQ(pool.reservedBytes(), (8 - repaid) * kMiB);
        EXPECT_EQ(listener.billed, (8 - repaid) * kMiB);
        pool.unregisterSpillable(&donor);
        pool.SubMemory(2 * kMiB);
        pool.releaseUnusedReservation();
        EXPECT_EQ(listener.billed, 0);
    }
}

TEST(OmniTaskMemoryPoolFailure, NestedReservationKeepsOuterPendingBytesOutOfReclaimCredit) {
    constexpr int64_t kQuantum = int64_t{8} << 20;
    BillingListener listener;
    OmniTaskMemoryPool pool(&listener);
    pool.AddMemory(8);
    CallbackDonor donor;
    donor.action = [&] { pool.SubMemory(4); return 4; };
    pool.registerSpillable(&donor);
    listener.onBorrow = [&] {
        listener.onBorrow = [&] {
            const int64_t freed = pool.reclaim(4);
            EXPECT_EQ(freed, 4);
            listener.billed -= freed;
        };
        ASSERT_TRUE(pool.maybeReserve(kQuantum + 1));
        throw ReservationDenied("outer reservation denied");
    };
    EXPECT_FALSE(pool.maybeReserve(1));
    EXPECT_EQ(pool.currentBytes(), 4);
    EXPECT_EQ(pool.reservedBytes(), kQuantum + 4);
    EXPECT_EQ(listener.billed, kQuantum + 4);
    pool.unregisterSpillable(&donor);
    pool.SubMemory(4);
    pool.releaseUnusedReservation();
    EXPECT_EQ(listener.billed, 0);
}

TEST(OmniTaskMemoryPoolFailure, FatalReclaimIsNeverAdvisoryIncludingDonorOom) {
    for (bool donorOom : {false, true}) {
        BillingListener listener;
        OmniTaskMemoryPool pool(&listener);
        pool.AddMemory(8);
        CallbackDonor donor;
        donor.action = [&]() -> int64_t {
            pool.SubMemory(4); // destructive donor has already changed its state
            if (donorOom) throw ReservationDenied("donor allocation failed");
            throw std::runtime_error("JOIN_SPILL_WRITE_FAILED");
        };
        pool.registerSpillable(&donor);
        listener.onBorrow = [&] { pool.reclaim(4); };
        try {
            pool.maybeReserve(8);
            FAIL() << "fatal reclaim must propagate";
        } catch (const ReservationDenied&) {
            FAIL() << "destructive donor failure must not be classified as a clean denial";
        } catch (const std::runtime_error& error) {
            EXPECT_NE(std::string(error.what()).find("Native reclaim failed"), std::string::npos);
        }
        EXPECT_EQ(pool.currentBytes(), 4);
        EXPECT_EQ(pool.reservedBytes(), 8); // no external repay followed the failure
        pool.unregisterSpillable(&donor);
        pool.SubMemory(4);
        pool.releaseUnusedReservation();
        EXPECT_EQ(listener.billed, 0);
    }
}
} // namespace omniruntime::mem::test
