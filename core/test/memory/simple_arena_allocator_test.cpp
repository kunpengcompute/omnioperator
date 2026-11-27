/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "gtest/gtest.h"
#include "memory/simple_arena_allocator.h"

namespace omniruntime::mem::test {
TEST(SimpleArenaAllocator, testAllocate)
{
    int64_t chunkSize = 4096;
    SimpleArenaAllocator arena(chunkSize);

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
    SimpleArenaAllocator arena(chunkSize);
    int64_t smallSize = 100;
    uint8_t const *start = arena.Allocate(smallSize);
    EXPECT_NE(start, nullptr);
    EXPECT_EQ(arena.TotalBytes(), chunkSize);
    EXPECT_EQ(arena.AvailBytes(), chunkSize - smallSize);
    char const *p = reinterpret_cast<char *>(arena.AllocateContinue(smallSize, start));
    EXPECT_NE(p, nullptr);
    EXPECT_EQ(arena.TotalBytes(), chunkSize);
    EXPECT_EQ(arena.AvailBytes(), chunkSize - 2 * smallSize);
}

// allocateContinue large size
TEST(SimpleArenaAllocator, testAllocateContinueLargeSize)
{
    int64_t chunkSize = 4096;
    SimpleArenaAllocator arena(chunkSize);
    int64_t smallSize = 100;
    uint8_t const *start = arena.Allocate(smallSize);
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
    SimpleArenaAllocator arena(chunkSize);

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
    SimpleArenaAllocator arena(chunkSize);

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
    auto *arena = new SimpleArenaAllocator();
    int64_t allocatedSize = 1024;
    uint8_t *noZeroAddr = arena->Allocate(allocatedSize);
    EXPECT_NE(noZeroAddr, nullptr);
    uint8_t *zeroAddr = arena->Allocate(0);
    EXPECT_NE(zeroAddr, nullptr);
    EXPECT_EQ(sizeof(zeroAddr), 8); // 8 bytes
    delete arena;
}
} // namespace omniruntime::mem::test
