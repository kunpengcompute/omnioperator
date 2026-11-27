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
#include "operator/execution_context.h"

using namespace omniruntime::op;
using namespace std;

TEST(ExecutionContext, testAllocateAndReset)
{
    ExecutionContext *executionContext = new ExecutionContext();
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
