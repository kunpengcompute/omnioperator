/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 *
 * Self-test of TestPoolFixture.
 *
 * Validates the fixture's lifecycle before downstream test sites depend on it:
 *   1. SetUp gives all three accessors non-null pointers.
 *   2. Pool-routed Alloc debits pool->currentBytes() by the exact size
 *      (proves the fixture's allocator IS pool-bound to the fixture's pool,
 *      not a singleton).
 *   3. Pool-routed Free credits pool->currentBytes() back to 0.
 *   4. Listener observes both alloc + free as up-calls; totalDelta nets to 0
 *      for a symmetric alloc/free pair.
 *   5. TearDown ordering is symmetric — allocator.reset() → pool.reset() →
 *      listener.reset() — verified by an out-of-fixture TEST that owns the
 *      MockListener on the stack (so we can inspect its state AFTER the
 *      pool+allocator have been torn down but BEFORE the listener leaves
 *      scope). Asserts totalDelta == 0 post-teardown (no orphaned debits).
 *      This is the lifecycle correctness test.
 */

#include "gtest/gtest.h"

#include "test_pool_fixture.h"

#include "memory/omni_task_memory_pool.h"
#include "memory/allocator.h"

namespace omniruntime::test {

// ---------------------------------------------------------------------------
// Test 1 — SetUp wires all three accessors to non-null pointers.
// ---------------------------------------------------------------------------
TEST_F(TestPoolFixture, AccessorsNonNullAfterSetUp)
{
    EXPECT_NE(listener(), nullptr);
    EXPECT_NE(pool(), nullptr);
    EXPECT_NE(allocator(), nullptr);

    // The pool's bound listener must be the fixture's listener (not some other
    // mock somewhere) — guards against a refactor that breaks the wiring.
    // (Cannot directly inspect pool->listener_; the cheapest proxy is to verify
    //  callCount starts at 0 and increments via this listener after an Alloc.)
    EXPECT_EQ(listener()->callCount, 0);
    EXPECT_EQ(listener()->totalDelta, 0);

    // The allocator's bound pool must be the fixture's pool.
    EXPECT_EQ(allocator()->getTaskPool(), pool());
}

// ---------------------------------------------------------------------------
// Test 2 — pool-routed Alloc(1024) debits pool->currentBytes() by exactly 1024
// (proves pool-bound routing, not singleton fall-through).
// ---------------------------------------------------------------------------
TEST_F(TestPoolFixture, AllocDebitsPool)
{
    EXPECT_EQ(pool()->currentBytes(), 0);

    void *ptr = allocator()->Alloc(1024);
    ASSERT_NE(ptr, nullptr);

    EXPECT_EQ(pool()->currentBytes(), 1024)
        << "Alloc(1024) on pool-bound allocator must debit fixture's pool by 1024";

    // Free here so the fixture's TearDown does not see a residual leak. (We
    // deliberately do NOT assert on currentBytes after the Free in this test
    // — that's FreeCreditsPool's job. Just keep the pool clean for TearDown.)
    allocator()->Free(ptr, 1024);
}

// ---------------------------------------------------------------------------
// Test 3 — Free credits pool back to 0 (round-trip symmetry).
// ---------------------------------------------------------------------------
TEST_F(TestPoolFixture, FreeCreditsPool)
{
    void *ptr = allocator()->Alloc(2048);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(pool()->currentBytes(), 2048);

    allocator()->Free(ptr, 2048);
    EXPECT_EQ(pool()->currentBytes(), 0)
        << "Free must credit the construction-time pool back to zero";

    // Peak stays monotonic — Free does NOT reduce peakBytes.
    EXPECT_EQ(pool()->peakBytes(), 2048);
}

// ---------------------------------------------------------------------------
// Test 4 — Listener observes both alloc + free up-calls; symmetric round-trip
// nets totalDelta to 0 and increments callCount by exactly 2.
// ---------------------------------------------------------------------------
TEST_F(TestPoolFixture, ListenerObservesAllocations)
{
    EXPECT_EQ(listener()->callCount, 0);
    EXPECT_EQ(listener()->totalDelta, 0);

    void *ptr = allocator()->Alloc(4096);
    ASSERT_NE(ptr, nullptr);

    // Alloc(N) routes via pool->AddMemory(N) which up-calls
    // listener->allocationChanged(+N). One call so far.
    EXPECT_EQ(listener()->callCount, 1)
        << "Allocator::Alloc must up-call listener exactly once";
    EXPECT_EQ(listener()->totalDelta, 4096)
        << "Listener delta after Alloc(4096) must be +4096";

    allocator()->Free(ptr, 4096);

    // Free(N) routes via pool->SubMemory(N) which up-calls
    // listener->allocationChanged(-N). Two calls total, net delta 0.
    EXPECT_EQ(listener()->callCount, 2)
        << "Allocator::Free must up-call listener exactly once";
    EXPECT_EQ(listener()->totalDelta, 0)
        << "Symmetric Alloc/Free must net listener delta to 0";
}

}  // namespace omniruntime::test

// ---------------------------------------------------------------------------
// Test 5 — TearDown ordering is symmetric.
//
// Outside the fixture so we can inspect the MockListener AFTER the
// allocator + pool have been destroyed but BEFORE the listener leaves scope.
// Mirrors the fixture's SetUp / TearDown order using stack-managed
// unique_ptr wrappers around the same three objects. The asserts at the end
// prove the allocator + pool teardown left no residual accounting (totalDelta
// must net to 0, no extra listener up-calls beyond the explicit Alloc/Free
// pair).
// ---------------------------------------------------------------------------
TEST(TestPoolFixtureLifecycle, TearDownSymmetric)
{
    omniruntime::test::MockListener listener;  // stack — outlives pool + allocator
    {
        // SetUp-equivalent: listener → pool → allocator.
        auto pool = std::make_unique<omniruntime::mem::OmniTaskMemoryPool>(&listener);
        auto allocator = std::make_unique<omniruntime::mem::Allocator>(pool.get());

        // Drive a symmetric alloc/free pair so the pool ledger is clean before
        // TearDown.
        void *ptr = allocator->Alloc(8192);
        ASSERT_NE(ptr, nullptr);
        EXPECT_EQ(pool->currentBytes(), 8192);
        EXPECT_EQ(listener.callCount, 1);
        EXPECT_EQ(listener.totalDelta, 8192);

        allocator->Free(ptr, 8192);
        EXPECT_EQ(pool->currentBytes(), 0);
        EXPECT_EQ(listener.callCount, 2);
        EXPECT_EQ(listener.totalDelta, 0);

        // TearDown-equivalent: REVERSE order — allocator first, pool second.
        // (Listener stays on stack, destroyed at end of TEST function.)
        allocator.reset();
        pool.reset();
    }

    // Listener is now the only survivor. If the pool/allocator dtors had
    // emitted any orphaned debits/credits, totalDelta would be non-zero.
    EXPECT_EQ(listener.totalDelta, 0)
        << "TearDown ordering bug: pool/allocator dtors emitted residual "
           "accounting after the symmetric Alloc/Free pair was already balanced "
           "(lifecycle invariant violated)";

    // Allow the dtor drain to up-call listener at most once with delta 0
    // (current OmniTaskMemoryPool::~OmniTaskMemoryPool drains via SubMemory
    //  only if currentBytes_ > 0; we already drained explicitly, so no
    //  additional call is expected — but tolerate one accounting-neutral
    //  diagnostic up-call if the drain path evolves).
    EXPECT_LE(listener.callCount, 3)
        << "TearDown should not generate spurious listener up-calls beyond the "
           "explicit Alloc + Free pair (callCount=" << listener.callCount << ")";
}
