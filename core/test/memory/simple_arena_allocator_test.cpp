/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
 */

#include "gtest/gtest.h"
#include "memory/simple_arena_allocator.h"
#include "allocator.h"

namespace omniruntime::mem::test {
// TU-local stand-in for the retired Allocator singleton (allocator.h `= delete`).
// Null-pool Allocator = the legacy-bridge unaccounted path; same process-wide
// lifetime and no-accounting semantics the old singleton had.
static Allocator *TestAllocator()
{
    static Allocator allocator(nullptr);
    return &allocator;
}

TEST(SimpleArenaAllocator, testAllocate)
{
    int64_t chunkSize = 4096;
    SimpleArenaAllocator arena(chunkSize, TestAllocator());

    // Small allocations should come from the same chunk.
    int64_t smallSize = 100;
    for (int64_t i = 0; i < 20; ++i) {
        auto p = arena.Allocate(smallSize);
        EXPECT_NE(p, nullptr);
        EXPECT_EQ(arena.TotalBytes(), chunkSize);
        EXPECT_EQ(arena.AvailBytes(), chunkSize - (i + 1) * smallSize);
    }

    // large allocations require separate chunks
    int64_t largeSize = 100 * chunkSize;
    auto p = arena.Allocate(largeSize);
    EXPECT_NE(p, nullptr);
    EXPECT_EQ(arena.TotalBytes(), chunkSize + largeSize);
    EXPECT_EQ(arena.AvailBytes(), 0);
}

// allocateContinue small size
TEST(SimpleArenaAllocator, testAllocateContinueSmallSize)
{
    int64_t chunkSize = 4096;
    SimpleArenaAllocator arena(chunkSize, TestAllocator());
    int64_t smallSize = 100;
    uint8_t const * start = arena.Allocate(smallSize);
    EXPECT_NE(start, nullptr);
    EXPECT_EQ(arena.TotalBytes(), chunkSize);
    EXPECT_EQ(arena.AvailBytes(), chunkSize - smallSize);
    char const * p = reinterpret_cast<char *>(arena.AllocateContinue(smallSize, start));
    EXPECT_NE(p, nullptr);
    EXPECT_EQ(arena.TotalBytes(), chunkSize);
    EXPECT_EQ(arena.AvailBytes(), chunkSize - 2 * smallSize);
}

// allocateContinue large size
TEST(SimpleArenaAllocator, testAllocateContinueLargeSize)
{
    int64_t chunkSize = 4096;
    SimpleArenaAllocator arena(chunkSize, TestAllocator());
    int64_t smallSize = 100;
    uint8_t const * start = arena.Allocate(smallSize);
    EXPECT_NE(start, nullptr);
    EXPECT_EQ(arena.TotalBytes(), chunkSize);
    EXPECT_EQ(arena.AvailBytes(), chunkSize - smallSize);

    int64_t largeSize = 100 * chunkSize;
    start = arena.AllocateContinue(largeSize, start);
    EXPECT_NE(start, nullptr);
    EXPECT_EQ(arena.TotalBytes(), chunkSize + smallSize + largeSize);
    EXPECT_EQ(arena.AvailBytes(), 0);
}

// small followed by big, then Reset
TEST(SimpleArenaAllocator, testResetSmallToBig)
{
    int64_t chunkSize = 4096;
    SimpleArenaAllocator arena(chunkSize, TestAllocator());

    int64_t smallSize = 100;
    auto p = arena.Allocate(smallSize);
    EXPECT_NE(p, nullptr);

    int64_t largeSize = 100 * chunkSize;
    p = arena.Allocate(largeSize);
    EXPECT_NE(p, nullptr);

    EXPECT_EQ(arena.TotalBytes(), chunkSize + largeSize);
    EXPECT_EQ(arena.AvailBytes(), 0);
    arena.Reset();
    EXPECT_EQ(arena.TotalBytes(), chunkSize);
    EXPECT_EQ(arena.AvailBytes(), chunkSize);

    // should re-use buffer after Reset.
    p = arena.Allocate(smallSize);
    EXPECT_NE(p, nullptr);
    EXPECT_EQ(arena.TotalBytes(), chunkSize);
    EXPECT_EQ(arena.AvailBytes(), chunkSize - smallSize);
}

// big followed by small, then Reset
TEST(SimpleArenaAllocator, testResetFromBigToSmall)
{
    int64_t chunkSize = 4096;
    SimpleArenaAllocator arena(chunkSize, TestAllocator());

    int64_t largeSize = 100 * chunkSize;
    auto p = arena.Allocate(largeSize);
    EXPECT_NE(p, nullptr);

    int64_t smallSize = 100;
    p = arena.Allocate(smallSize);
    EXPECT_NE(p, nullptr);

    EXPECT_EQ(arena.TotalBytes(), largeSize + 2 * largeSize);
    EXPECT_EQ(arena.AvailBytes(), 2 * largeSize - smallSize);
    arena.Reset();
    EXPECT_EQ(arena.TotalBytes(), largeSize);
    EXPECT_EQ(arena.AvailBytes(), largeSize);

    // should re-use buffer after Reset.
    p = arena.Allocate(smallSize);
    EXPECT_NE(p, nullptr);
    EXPECT_EQ(arena.TotalBytes(), largeSize);
    EXPECT_EQ(arena.AvailBytes(), largeSize - smallSize);
}

TEST(SimpleArenaAllocator, testAllocateZeroSize)
{
    auto *arena = new SimpleArenaAllocator(4096, TestAllocator());
    int64_t allocatedSize = 1024;
    uint8_t *noZeroAddr = arena->Allocate(allocatedSize);
    EXPECT_NE(noZeroAddr, nullptr);
    uint8_t *zeroAddr = arena->Allocate(0);
    EXPECT_NE(zeroAddr, nullptr);
    EXPECT_EQ(sizeof(zeroAddr), 8); // 8 bytes
    delete arena;
}
}
