/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 *
 * TestPoolFixture — gtest base class providing a real OmniTaskMemoryPool +
 * pool-bound Allocator + MockListener for any unit test that previously relied
 * on Allocator::GetAllocator() (the singleton retired — see
 * core/src/memory/allocator.h:60 `= delete`).
 *
 * Why this exists:
 *   - Allocator::GetAllocator() was deleted (= delete). Tests that historically
 *     constructed bare Vector<>/AlignedBuffer<>/SimpleArenaAllocator via the
 *     singleton no longer compile.
 *   - Each test needs the same boilerplate triple (listener → pool → allocator)
 *     with the same SetUp/TearDown lifecycle. Without a fixture, every test
 *     reinvents the wiring (and gets the dtor order wrong).
 *   - Migrating ~164 call sites; the
 *     corresponding test files will simply `class MyTest : public
 *     omniruntime::test::TestPoolFixture {}` and call `pool()`/`allocator()`.
 *
 * MockListener is LIFTED VERBATIM from
 * omni/core/test/memory/omni_task_memory_pool_test.cpp:39-52 — that is the
 * single source of truth; this header re-publishes the same definition into
 * the omniruntime::test namespace so other test TUs can use it without
 * re-including the entire pool_test.cpp.
 *
 * Lifecycle invariant: SetUp constructs in order
 * listener → pool → allocator (pool ctor needs listener pointer; allocator
 * ctor needs pool pointer). TearDown reverses: allocator → pool → listener,
 * because ~OmniTaskMemoryPool may up-call listener->allocationChanged() on
 * any residual drain, and ~Allocator must not outlive its pool.
 */

#ifndef OMNI_RUNTIME_TEST_POOL_FIXTURE_H
#define OMNI_RUNTIME_TEST_POOL_FIXTURE_H

#include <memory>
#include <stdexcept>

#include "gtest/gtest.h"

#include "memory/omni_task_memory_pool.h"
#include "memory/allocator.h"
#include "memory/pool_listener.h"  // single-source-of-truth for
                                              // omniruntime::mem::PoolListener

namespace omniruntime::test {

// MockListener — records up-call count + cumulative delta. Optional throw mode
// is exercised by the TwoPhaseCommitRollback test.
//
// Lifted VERBATIM from omni/core/test/memory/omni_task_memory_pool_test.cpp:39-52
// (single source of truth). Do not edit one copy without editing the other.
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

// TestPoolFixture — gtest base providing a per-test pool-bound triple.
// Inherit and use pool() / allocator() / listener() in TEST_F bodies.
class TestPoolFixture : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Construction order matters:
        //   listener (no deps) → pool (needs listener pointer) →
        //   allocator (needs pool pointer).
        listener_ = std::make_unique<MockListener>();
        pool_ = std::make_unique<omniruntime::mem::OmniTaskMemoryPool>(listener_.get());
        allocator_ = std::make_unique<omniruntime::mem::Allocator>(pool_.get());
    }

    void TearDown() override
    {
        // Reverse construction order: allocator first (so its ~Allocator() does
        // not outlive its taskPool_ pointer), then pool (whose dtor may drain
        // residual bytes via listener up-call), then listener.
        allocator_.reset();
        pool_.reset();
        listener_.reset();
    }

    // Non-owning accessors for TEST_F bodies.
    omniruntime::mem::OmniTaskMemoryPool *pool() { return pool_.get(); }
    omniruntime::mem::Allocator *allocator() { return allocator_.get(); }
    MockListener *listener() { return listener_.get(); }

private:
    std::unique_ptr<MockListener> listener_;
    std::unique_ptr<omniruntime::mem::OmniTaskMemoryPool> pool_;
    std::unique_ptr<omniruntime::mem::Allocator> allocator_;
};

}  // namespace omniruntime::test

#endif  // OMNI_RUNTIME_TEST_POOL_FIXTURE_H
