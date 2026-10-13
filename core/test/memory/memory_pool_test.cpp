/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 *
 * Unit coverage for memory_pool.{h,cpp}.
 *
 * The MemoryPool allocator boundary — every Allocator::Alloc/Free ultimately
 * bottoms out in one of these MemoryPool::Allocate/Release implementations —
 * had no test file at all.
 *
 * Only the public surface is reachable from a test: memory_pool.h exposes the
 * abstract MemoryPool plus GetMemoryPool(). The concrete SimpleAllocator /
 * JemallocAllocator / SimpleMemoryPool / JemallocMemoryPool are file-private in
 * memory_pool.cpp, so we exercise them through GetMemoryPool() — a
 * JemallocMemoryPool in normal builds, a SimpleMemoryPool under -DCOVERAGE. The
 * GetPreferredSize growth heuristic is duplicated identically in both, and both
 * Allocate paths share the same negative-size / null-buffer throw contract, so
 * the expectations below hold regardless of which concrete pool is compiled in.
 *
 * Covers:
 *   - Allocate negative-size throw
 *   - Allocate OOM (null-buffer) throw
 *   - zeroFill=true (calloc / MALLOCX_ZERO) branch returns zeroed memory
 *   - Allocate/Release round-trip
 *   - GetPreferredSize pow-2 / 1.5x / next-pow-2 rounding heuristic
 */

#include <cstdint>
#include <cstring>

#include "gtest/gtest.h"

#include "memory_pool.h"
#include "util/omni_exception.h"

namespace omniruntime::mem::test {

// ---- GetMemoryPool singleton contract -------------------------------------

TEST(MemoryPool, GetMemoryPoolReturnsStableSingleton)
{
    MemoryPool *p = GetMemoryPool();
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p, GetMemoryPool()) << "GetMemoryPool must return the same process-global instance";
}

// ---- Allocate / Release round-trip ----------------------------------------

TEST(MemoryPool, AllocateThenReleaseRoundTrips)
{
    MemoryPool *p = GetMemoryPool();
    uint8_t *buf = nullptr;
    p->Allocate(64, &buf);
    ASSERT_NE(buf, nullptr);
    // Writable across the full request — proves the allocation is real, not a
    // null/short buffer slipped past the OOM guard.
    std::memset(buf, 0xAB, 64);
    EXPECT_EQ(buf[0], 0xABu);
    EXPECT_EQ(buf[63], 0xABu);
    p->Release(buf);
}

TEST(MemoryPool, ZeroFillReturnsZeroedBuffer)
{
    MemoryPool *p = GetMemoryPool();
    uint8_t *buf = nullptr;
    const int64_t n = 256;
    p->Allocate(n, &buf, /*zeroFill=*/true);
    ASSERT_NE(buf, nullptr);
    for (int64_t i = 0; i < n; ++i) {
        ASSERT_EQ(buf[i], 0u) << "zeroFill buffer must be zeroed at index " << i;
    }
    p->Release(buf);
}

// ---- Throw paths -----------------------------------------------------------

TEST(MemoryPool, NegativeSizeThrows)
{
    MemoryPool *p = GetMemoryPool();
    uint8_t *buf = nullptr;
    EXPECT_THROW(p->Allocate(-1, &buf), omniruntime::exception::OmniException);
    EXPECT_THROW(p->Allocate(-4096, &buf), omniruntime::exception::OmniException);
    // The negative-size guard fires before zeroFill is consulted.
    EXPECT_THROW(p->Allocate(-1, &buf, /*zeroFill=*/true), omniruntime::exception::OmniException);
}

TEST(MemoryPool, UnsatisfiableSizeThrows)
{
    MemoryPool *p = GetMemoryPool();
    uint8_t *buf = nullptr;
    // 1 << 62 == 4 EiB: far beyond addressable virtual memory on any 64-bit
    // host, so the underlying allocator returns null. Allocate must convert
    // that into an OmniException, not hand back a silent null buffer.
    const int64_t unsatisfiable = static_cast<int64_t>(1) << 62;
    EXPECT_THROW(p->Allocate(unsatisfiable, &buf), omniruntime::exception::OmniException);
}

// ---- GetPreferredSize growth heuristic ------------------------------------

// Independent oracle: the expectations below are hand-computed constants, not a
// reimplementation of the production formula, so a regression in the formula is
// actually caught rather than mirrored.
//
//   size == 0            -> 0
//   0 < size < 8         -> 8                  (smallSize floor)
//   size is a power of 2 -> size               (identity)
//   else, lower = 2^floor(log2 size):
//     1.5*lower >= size  -> 1.5*lower          (1.5x bucket)
//     otherwise          -> 2*lower            (next power of two)
TEST(MemoryPool, GetPreferredSizeHeuristicTable)
{
    MemoryPool *p = GetMemoryPool();
    struct Case {
        uint64_t in;
        uint64_t want;
    };
    const Case cases[] = {
        {0, 0},               // zero passes through
        {1, 8},   {7, 8},     // below the smallSize floor
        {8, 8},               // smallSize boundary == pow2
        {9, 12},              // 1.5 * 8
        {12, 12},             // exactly 1.5 * 8
        {13, 16},             // > 1.5*8  -> next pow2
        {15, 16}, {16, 16},   // pow2 boundary
        {17, 24},             // 1.5 * 16
        {24, 24}, {25, 32},   // > 1.5*16 -> next pow2
        {31, 32}, {32, 32},
        {1023, 1024},         // 768 < 1023 -> next pow2
        {1024, 1024},         // pow2 identity
        {1025, 1536},         // 1.5 * 1024
        {1536, 1536},         // exactly 1.5 * 1024
        {1537, 2048},         // > 1.5*1024 -> next pow2
        {2047, 2048}, {2048, 2048},
    };
    for (const auto &c : cases) {
        EXPECT_EQ(p->GetPreferredSize(c.in), c.want) << "GetPreferredSize(" << c.in << ")";
    }
}

TEST(MemoryPool, GetPreferredSizePowersOfTwoAreIdentity)
{
    MemoryPool *p = GetMemoryPool();
    for (uint32_t b = 3; b <= 40; ++b) { // 8 .. 1<<40
        uint64_t v = 1ULL << b;
        EXPECT_EQ(p->GetPreferredSize(v), v) << "pow2 1<<" << b;
    }
}

// Property check, independent of the exact 1.5x-vs-2x choice: for any size the
// preferred size never shrinks below the request and never overshoots the next
// power of two. (lower-bound oracle = the request; upper-bound oracle = next
// pow2, derived here separately from the production return value.)
TEST(MemoryPool, GetPreferredSizeRoundsUpWithinNextPow2)
{
    MemoryPool *p = GetMemoryPool();
    for (uint64_t s = 8; s <= 4096; ++s) {
        uint64_t got = p->GetPreferredSize(s);
        EXPECT_GE(got, s) << "must never shrink at size " << s;
        uint32_t bits = 63 - __builtin_clzll(s);
        uint64_t lower = 1ULL << bits;
        uint64_t nextPow2 = (lower == s) ? s : (lower << 1);
        EXPECT_LE(got, nextPow2) << "must not exceed next pow2 at size " << s;
    }
}

} // namespace omniruntime::mem::test
