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
#include "thread_memory_manager.h"

namespace omniruntime::mem::test {
const static int64_t THRESHOLD = 1 * 1024 * 1024;

// test: report memory usage but not reach the untrackedMemoryThreshold.
TEST(ThreadMemoryManager, testReportMemoryUsageNotReachThreshold)
{
    auto threadMemoryManager = mem::ThreadMemoryManager::GetThreadMemoryManager();
    threadMemoryManager->Clear();
    int64_t size = THRESHOLD;
    threadMemoryManager->ReportMemoryUsage(size);
    int64_t threadAccount = threadMemoryManager->GetThreadAccountedMemory();
    int64_t threadUntracked = threadMemoryManager->GetUntrackedMemory();
    EXPECT_EQ(threadAccount, 0);
    EXPECT_EQ(threadUntracked, size);
}

// test: reclaim memory usage but not reach the untrackedMemoryThreshold.
TEST(ThreadMemoryManager, testReclaimMemoryUsageNotReachThreshold)
{
    auto threadMemoryManager = mem::ThreadMemoryManager::GetThreadMemoryManager();
    threadMemoryManager->Clear();
    int64_t size = THRESHOLD;
    threadMemoryManager->ReclaimMemoryUsage(size);
    int64_t threadAccount = threadMemoryManager->GetThreadAccountedMemory();
    int64_t threadUntracked = threadMemoryManager->GetUntrackedMemory();
    EXPECT_EQ(threadAccount, 0);
    EXPECT_EQ(threadUntracked, -size);
}

// test: report memory usage and reach the untrackedMemoryThreshold->
TEST(ThreadMemoryManager, testReportMemoryUsageReachThreshold)
{
    auto threadMemoryManager = mem::ThreadMemoryManager::GetThreadMemoryManager();
    threadMemoryManager->Clear();
    int64_t size = THRESHOLD + 1;

    threadMemoryManager->ReportMemoryUsage(size);
    int64_t threadAccount = threadMemoryManager->GetThreadAccountedMemory();
    EXPECT_EQ(threadAccount, size);
}

// test: report memory usage and reach the globalMemoryThreshold, then throw an exception.
// expect that untracked memory is 0.
TEST(ThreadMemoryManager, testReportMemoryUsageReachThresholdWithException)
{
    auto threadMemoryManager = mem::ThreadMemoryManager::GetThreadMemoryManager();
    threadMemoryManager->Clear();

    mem::MemoryManager::SetGlobalMemoryLimit(THRESHOLD);
    int64_t size1 = THRESHOLD / 2;
    threadMemoryManager->ReportMemoryUsage(size1);
    int64_t untrackedMemory = threadMemoryManager->GetUntrackedMemory();
    EXPECT_EQ(untrackedMemory, size1);

    int64_t size2 = THRESHOLD + 1;
    EXPECT_ANY_THROW(threadMemoryManager->ReportMemoryUsage(size2));
    untrackedMemory = threadMemoryManager->GetUntrackedMemory();
    EXPECT_EQ(untrackedMemory, 0);
    MemoryManager *globalMemoryManager = MemoryManager::GetGlobalMemoryManager();
    int64_t memoryAccount = globalMemoryManager->GetMemoryAmount();
    EXPECT_EQ(memoryAccount, size1);
}

// test: reclaim memory usage and reach the untrackedMemoryThreshold->
TEST(ThreadMemoryManager, testReclaimMemoryUsageReachThreshold)
{
    auto threadMemoryManager = mem::ThreadMemoryManager::GetThreadMemoryManager();
    threadMemoryManager->Clear();
    int64_t size = THRESHOLD + 1;

    threadMemoryManager->ReclaimMemoryUsage(size);
    int64_t threadAccount = threadMemoryManager->GetThreadAccountedMemory();
    EXPECT_EQ(threadAccount, -size);
    threadMemoryManager->Clear();
}
} // namespace omniruntime::mem::test
