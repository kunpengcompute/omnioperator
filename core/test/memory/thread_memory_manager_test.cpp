/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
 */

#include "gtest/gtest.h"
#include <cstdlib>
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
    // Enable the exception path explicitly; production defaults disable it.
    setenv("OMNI_DISABLE_MEM_CAP_EXCEEDED", "0", 1);
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
    unsetenv("OMNI_DISABLE_MEM_CAP_EXCEEDED");
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
}
