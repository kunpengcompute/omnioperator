/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
 * Description: ...
 */
#include "gtest/gtest.h"
#include <type_traits>
#include "operator/execution_context.h"

using namespace omniruntime::op;
using namespace std;

static_assert(!std::is_copy_constructible_v<ExecutionContext>);
static_assert(!std::is_copy_assignable_v<ExecutionContext>);
static_assert(!std::is_move_constructible_v<ExecutionContext>);
static_assert(!std::is_move_assignable_v<ExecutionContext>);


TEST(ExecutionContext, testAllocateAndReset)
{
    ExecutionContext *executionContext = new ExecutionContext(4096, nullptr);
    int64_t chunkSize = 4096;

    // Small allocations should come from the same chunk.
    int64_t smallSize = 100;
    for (int64_t i = 0; i < 20; ++i) {
        auto p = executionContext->GetArena()->Allocate(smallSize);
        EXPECT_NE(p, nullptr);
        EXPECT_EQ(executionContext->GetArena()->TotalBytes(), chunkSize);
        EXPECT_EQ(executionContext->GetArena()->AvailBytes(), chunkSize - (i + 1) * smallSize);
    }

    // large allocations require separate chunks
    int64_t largeSize = 100 * chunkSize;
    auto p = executionContext->GetArena()->Allocate(largeSize);
    EXPECT_NE(p, nullptr);
    EXPECT_EQ(executionContext->GetArena()->TotalBytes(), chunkSize + largeSize);
    EXPECT_EQ(executionContext->GetArena()->AvailBytes(), 0);

    executionContext->GetArena()->Reset();
    EXPECT_EQ(executionContext->GetArena()->TotalBytes(), chunkSize);
    EXPECT_EQ(executionContext->GetArena()->AvailBytes(), chunkSize);

    delete executionContext;
}
