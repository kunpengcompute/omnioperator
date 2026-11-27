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

#include <gtest/gtest.h>
#include "memory/memory_trace.h"
#include "vector/vector.h"

namespace omniruntime::mem::test {
using namespace omniruntime::vec;

void testMemoryTraceWithinMultipleThreads(int32_t size)
{
    auto threadMemoryTrace = mem::ThreadMemoryTrace::GetThreadMemoryTrace();
    auto vector = std::make_unique<Vector<int32_t>>(size);
    EXPECT_EQ(threadMemoryTrace->GetVectorTraced().size(), 1);
    EXPECT_EQ(threadMemoryTrace->GetArenaTraced().size(), 2);
}

#ifdef TRACE
// test: thread memory trace under multiple threads
TEST(MemoryTrace, testMemoryTraceWithinMultipleThreads)
{
    int processorCount = static_cast<int>(std::thread::hardware_concurrency());
    std::cout << "core number: " << processorCount << std::endl;
    int threadNums[] = {2, 4, 8, 16};
    for (uint32_t i = 0; i < sizeof(threadNums) / sizeof(int); ++i) {
        int threadNum = threadNums[i] <= processorCount ? threadNums[i] : processorCount;

        std::vector<std::thread> memoryTraceOfThreads;
        int size = 100;
        for (int j = 0; j < threadNum; ++j) {
            std::thread t(testMemoryTraceWithinMultipleThreads, size);
            memoryTraceOfThreads.push_back(std::move(t));
        }
        for (auto &th : memoryTraceOfThreads) {
            if (th.joinable()) {
                th.join();
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}
#endif
} // namespace omniruntime::mem::test
