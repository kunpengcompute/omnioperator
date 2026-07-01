/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
 */

#include "gtest/gtest.h"
#include <cstdlib>
#include "memory_manager.h"
#include "memory/thread_memory_manager.h"
#include "vector/vector.h"
#include "jemalloc/jemalloc.h"
#include "vector/vector_test_util.h"

namespace omniruntime::mem::test {
using namespace vec;
using namespace vec::test;

void TestUnlimitedAccountMultipleThreads(int count, int64_t size)
{
    auto globalMemoryManager = mem::MemoryManager::GetGlobalMemoryManager();
    auto currentMemoryManager = std::make_unique<mem::MemoryManager>(globalMemoryManager);
    EXPECT_TRUE(currentMemoryManager != nullptr);

    for (int i = 0; i < count; ++i) {
        currentMemoryManager->AddMemory(size);
    }
    int64_t currentMemoryAmount = currentMemoryManager->GetMemoryAmount();
    EXPECT_EQ(currentMemoryAmount, count * size);
}

// test: unlimited account under multiple threads
TEST(MemoryManager, testUnlimitedAccountMultipleThreads)
{
    int processorCount = static_cast<int>(std::thread::hardware_concurrency());
    std::cout << "core number: " << processorCount << std::endl;
    int threadNums[] = {2, 4, 8, 16};
    for (uint32_t i = 0; i < sizeof(threadNums) / sizeof(int); ++i) {
        auto globalMemoryManager = mem::MemoryManager::GetGlobalMemoryManager();
        globalMemoryManager->Clear();

        int threadNum = threadNums[i] <= processorCount ? threadNums[i] : processorCount;

        std::vector<std::thread> memoryMangerOfThreads;
        int count = 10;
        int64_t positiveSize = 1 * 1024 * 1024;
        for (int j = 0; j < threadNum; ++j) {
            std::thread t(TestUnlimitedAccountMultipleThreads, count, positiveSize);
            memoryMangerOfThreads.push_back(std::move(t));
        }
        for (auto &th : memoryMangerOfThreads) {
            if (th.joinable()) {
                th.join();
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        int64_t globalMemoryAmount = globalMemoryManager->GetMemoryAmount();

        EXPECT_EQ(globalMemoryAmount, threadNum * count * positiveSize);

        int64_t negativeSize = -1 * 1024 * 1024;
        for (int j = 0; j < threadNum; ++j) {
            std::thread t(TestUnlimitedAccountMultipleThreads, count, negativeSize);
            memoryMangerOfThreads.push_back(std::move(t));
        }
        for (auto &th : memoryMangerOfThreads) {
            if (th.joinable()) {
                th.join();
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        globalMemoryAmount = globalMemoryManager->GetMemoryAmount();

        EXPECT_EQ(globalMemoryAmount, 0);
    }
}

void testLimitedAccountMultipleThreads(int count, int64_t size)
{
    auto globalMemoryManager = mem::MemoryManager::GetGlobalMemoryManager();
    auto currentMemoryManager = std::make_unique<mem::MemoryManager>(globalMemoryManager);
    EXPECT_TRUE(currentMemoryManager != nullptr);

    for (int i = 0; i < count; ++i) {
        try {
            currentMemoryManager->AddMemory(size);
        } catch (exception::OmniException &e) {
            break;
        }
    }
}

// test: limited account under multiple threads
TEST(MemoryManager, testLimitedAccountMultipleThreads)
{
    int64_t globalThreshold = 10 * 1024 * 1024;
    int processorCount = static_cast<int>(std::thread::hardware_concurrency());
    std::cout << "core number: " << processorCount << std::endl;
    int threadNums[] = {2, 4, 8, 16};
    for (uint32_t i = 0; i < sizeof(threadNums) / sizeof(int); ++i) {
        auto globalMemoryManager = mem::MemoryManager::GetGlobalMemoryManager();
        globalMemoryManager->Clear();

        mem::MemoryManager::SetGlobalMemoryLimit(globalThreshold);
        int threadNum = threadNums[i] <= processorCount ? threadNums[i] : processorCount;

        std::vector<std::thread> memoryMangerOfThreads;
        int count = 20;
        int64_t positiveSize = 1 * 1024 * 1024;
        for (int j = 0; j < threadNum; ++j) {
            std::thread t(testLimitedAccountMultipleThreads, count, positiveSize);
            memoryMangerOfThreads.push_back(std::move(t));
        }
        for (auto &th : memoryMangerOfThreads) {
            if (th.joinable()) {
                th.join();
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        int64_t globalMemoryAmount = globalMemoryManager->GetMemoryAmount();

        EXPECT_GE(globalMemoryAmount, globalThreshold);
    }
}

template <class T> auto CreateVector(int vecSize)
{
    auto vector = std::make_unique<Vector<T>>(vecSize);
    return vector;
}

    TEST(MemoryManager, testStatisticsFunction)
{
    // Avoid interference between UTs.
    auto threadMemoryManager = mem::ThreadMemoryManager::GetThreadMemoryManager();
    threadMemoryManager->Clear();

    auto int32Vector = CreateVector<int32_t>(100);
    int64_t threadUntracked = threadMemoryManager->GetUntrackedMemory();
    EXPECT_EQ(threadUntracked, 605);

    auto int64Vector = CreateVector<int64_t>(100);
    threadUntracked = threadMemoryManager->GetUntrackedMemory();
    EXPECT_EQ(threadUntracked, 1610); // int64Vector + int32Vector

    auto doubleVector = CreateVector<double>(100);
    threadUntracked = threadMemoryManager->GetUntrackedMemory();
    EXPECT_EQ(threadUntracked, 2615); // doubleVector + int64Vector + int32Vector

    int32Vector.reset();
    int64Vector.reset();
    doubleVector.reset();

    int64_t finalSize = threadMemoryManager->GetUntrackedMemory();
    EXPECT_EQ(finalSize, 0);

    auto globalMemoryManager = mem::MemoryManager::GetGlobalMemoryManager();
    globalMemoryManager->Clear();
}

// test: statistics function of Limit
TEST(MemoryManager, testStatisticsFunctionMemoryLimit)
{
    // Enable the exception path explicitly; production defaults disable it.
    setenv("OMNI_DISABLE_MEM_CAP_EXCEEDED", "0", 1);
    auto threadMemoryManager = mem::ThreadMemoryManager::GetThreadMemoryManager();
    threadMemoryManager->Clear();

    auto globalMemoryManager = mem::MemoryManager::GetGlobalMemoryManager();
    int size = 1024 * 1024;
    int limit = 8 * 1024 * 1024;
    globalMemoryManager->SetMemoryLimit(limit);
    auto vector1 = std::make_unique<Vector<int32_t>>(size);
    int64_t globalMemoryAmount = globalMemoryManager->GetMemoryAmount();
    // 4325384 = null size(131080) + value size(4194304), untracked memory(vector, nullsBuffer, valuesBuffer size(152))
    EXPECT_EQ(globalMemoryAmount, 4325384);
    // it is equivalent to "auto vector = std::make_unique<Vector<int32_t>>(size)"
    auto currentMemoryManager = std::make_unique<mem::MemoryManager>(globalMemoryManager);
    EXPECT_ANY_THROW(currentMemoryManager->AddMemory(globalMemoryAmount));

    globalMemoryAmount = globalMemoryManager->GetMemoryAmount();
    EXPECT_EQ(globalMemoryAmount, 8650768);
    unsetenv("OMNI_DISABLE_MEM_CAP_EXCEEDED");
}

TEST(MemoryManager, testFixedVectorStatisticsFunction)
{
    // Avoid interference between UTs.
    auto threadMemoryManager = mem::ThreadMemoryManager::GetThreadMemoryManager();
    threadMemoryManager->Clear();

    auto int32Vector = std::make_unique<Vector<int32_t>>(1000);
    auto slicedIntVector = int32Vector->Slice(0, 100);
    int32Vector.reset();
    delete slicedIntVector;

    auto globalMemoryManager = mem::MemoryManager::GetGlobalMemoryManager();
    auto globalMemoryAmount = globalMemoryManager->GetMemoryAmount();
    EXPECT_EQ(globalMemoryAmount, 0);
}

TEST(MemoryManager, testVarcharVectorStatisticsFunction)
{
    // Avoid interference between UTs.
    auto threadMemoryManager = mem::ThreadMemoryManager::GetThreadMemoryManager();
    threadMemoryManager->Clear();

    auto varcharVector = std::make_unique<Vector<LargeStringContainer<std::string_view>>>(1000);
    auto slicedVarcharVector = varcharVector->Slice(0, 100);
    varcharVector.reset();
    delete slicedVarcharVector;

    auto globalMemoryManager = mem::MemoryManager::GetGlobalMemoryManager();
    auto globalMemoryAmount = globalMemoryManager->GetMemoryAmount();
    EXPECT_EQ(globalMemoryAmount, 0);
}

TEST(MemoryManager, testDictionaryVarcharVectorStatisticsFunction)
{
    // Avoid interference between UTs.
    auto threadMemoryManager = mem::ThreadMemoryManager::GetThreadMemoryManager();
    threadMemoryManager->Clear();

    int32_t dicSize = 1000;
    int32_t valueSize = 7;
    auto dictionaryVarcharVector = CreateStringDictionaryVector<std::string_view>(dicSize, valueSize).release();
    auto slicedDictionaryVarcharVector = dictionaryVarcharVector->Slice(0, 6);

    delete dictionaryVarcharVector;
    delete slicedDictionaryVarcharVector;

    auto globalMemoryManager = mem::MemoryManager::GetGlobalMemoryManager();
    auto globalMemoryAmount = globalMemoryManager->GetMemoryAmount();
    EXPECT_EQ(globalMemoryAmount, 0);
}

TEST(MemoryManager, testDictionaryFixedVectorStatisticsFunction)
{
    // Avoid interference between UTs.
    auto threadMemoryManager = mem::ThreadMemoryManager::GetThreadMemoryManager();
    threadMemoryManager->Clear();

    int32_t dicSize = 1000;
    int32_t valueSize = 7;
    auto dictionaryFixedVector = CreateDictionaryVector<int64_t>(dicSize, valueSize).release();
    auto slicedDictionaryFixedVector = dictionaryFixedVector->Slice(0, 6);

    delete dictionaryFixedVector;
    delete slicedDictionaryFixedVector;

    auto globalMemoryManager = mem::MemoryManager::GetGlobalMemoryManager();
    auto globalMemoryAmount = globalMemoryManager->GetMemoryAmount();
    EXPECT_EQ(globalMemoryAmount, 0);
}

// test: set global memory limit
TEST(MemoryManager, testSetGlobalMemoryLimit)
{
    auto threadMemoryManager = mem::ThreadMemoryManager::GetThreadMemoryManager();
    threadMemoryManager->Clear();

    auto globalMemoryManager = mem::MemoryManager::GetGlobalMemoryManager();
    int64_t limit = 10 * 1024 * 1024;
    mem::MemoryManager::SetGlobalMemoryLimit(limit);
    int64_t actualLimit = globalMemoryManager->GetMemoryLimit();
    EXPECT_EQ(actualLimit, limit);
}

// test: get global accounted memory
TEST(MemoryManager, testGetGlobalAccountedMemory)
{
    auto threadMemoryManager = mem::ThreadMemoryManager::GetThreadMemoryManager();
    threadMemoryManager->Clear();

    auto globalMemoryManager = mem::MemoryManager::GetGlobalMemoryManager();
    int64_t size = 1024 * 1024;
    globalMemoryManager->SetMemoryAmount(size);

    int64_t globalAccountedMemory = mem::MemoryManager::GetGlobalAccountedMemory();
    EXPECT_EQ(globalAccountedMemory, size);
}

// test: set and get memory limit in thread_level memory manager
TEST(MemoryManager, testSetAndGetMemoryLimit)
{
    auto threadMemoryManager = mem::ThreadMemoryManager::GetThreadMemoryManager();
    threadMemoryManager->Clear();

    auto globalMemoryManager = mem::MemoryManager::GetGlobalMemoryManager();
    auto currentMemoryManager = std::make_unique<mem::MemoryManager>(globalMemoryManager);
    int64_t limit = 10 * 1024 * 1024;
    currentMemoryManager->SetMemoryLimit(limit);
    int64_t actualLimit = currentMemoryManager->GetMemoryLimit();
    EXPECT_EQ(actualLimit, limit);
}

// test: set and get parent
TEST(MemoryManager, testSetAndGetParent)
{
    auto threadMemoryManager = mem::ThreadMemoryManager::GetThreadMemoryManager();
    threadMemoryManager->Clear();

    auto globalMemoryManager = mem::MemoryManager::GetGlobalMemoryManager();
    mem::MemoryManager *parent = globalMemoryManager->GetParent();
    EXPECT_TRUE(parent == nullptr);
    auto currentMemoryManager = std::make_unique<mem::MemoryManager>();
    currentMemoryManager->SetParent(globalMemoryManager);
    EXPECT_EQ(currentMemoryManager->GetParent(), globalMemoryManager);
}

// test: set and get memoryAmount
TEST(MemoryManager, testSetAndGetMemoryAmount)
{
    auto threadMemoryManager = mem::ThreadMemoryManager::GetThreadMemoryManager();
    threadMemoryManager->Clear();

    auto globalMemoryManager = mem::MemoryManager::GetGlobalMemoryManager();
    auto currentMemoryManager = std::make_unique<mem::MemoryManager>(globalMemoryManager);
    EXPECT_TRUE(currentMemoryManager != nullptr);
    int64_t size = 10 * 1024 * 1024;
    currentMemoryManager->SetMemoryAmount(size);
    EXPECT_EQ(currentMemoryManager->GetMemoryAmount(), size);
}

// test: set and get memoryPeak
TEST(MemoryManager, testSetAndGetMemoryPeak)
{
    auto threadMemoryManager = mem::ThreadMemoryManager::GetThreadMemoryManager();
    threadMemoryManager->Clear();

    auto globalMemoryManager = mem::MemoryManager::GetGlobalMemoryManager();
    auto currentMemoryManager = std::make_unique<mem::MemoryManager>(globalMemoryManager);
    EXPECT_TRUE(currentMemoryManager != nullptr);
    int64_t size = 10 * 1024 * 1024;
    currentMemoryManager->SetMemoryPeak(size);
    EXPECT_EQ(currentMemoryManager->GetMemoryPeak(), size);
}

// test: update memoryPeak
TEST(MemoryManager, testUpdateMemoryPeak)
{
    auto threadMemoryManager = mem::ThreadMemoryManager::GetThreadMemoryManager();
    threadMemoryManager->Clear();

    auto globalMemoryManager = mem::MemoryManager::GetGlobalMemoryManager();
    auto currentMemoryManager = std::make_unique<mem::MemoryManager>(globalMemoryManager);
    EXPECT_TRUE(currentMemoryManager != nullptr);

    int64_t size = 10 * 1024 * 1024;
    currentMemoryManager->SetMemoryPeak(size);
    int64_t smallSize = 1 * 1024 * 1024;
    currentMemoryManager->UpdatePeak(smallSize);
    EXPECT_EQ(currentMemoryManager->GetMemoryPeak(), size);
    int64_t largeSize = 20 * 1024 * 1024;
    currentMemoryManager->UpdatePeak(largeSize);
    EXPECT_EQ(currentMemoryManager->GetMemoryPeak(), largeSize);
}

// test: unlimited account
TEST(MemoryManager, testUnlimitedAccount)
{
    auto threadMemoryManager = mem::ThreadMemoryManager::GetThreadMemoryManager();
    threadMemoryManager->Clear();

    auto globalMemoryManager = mem::MemoryManager::GetGlobalMemoryManager();
    auto currentMemoryManager = std::make_unique<mem::MemoryManager>(globalMemoryManager);
    EXPECT_TRUE(currentMemoryManager != nullptr);

    int count = 10;
    int64_t positiveSize = 1 * 1024 * 1024;
    int64_t negativeSize = -1 * 1024 * 1024;
    for (int i = 0; i < count; ++i) {
        currentMemoryManager->AddMemory(positiveSize);
    }
    int64_t currentMemoryAmount = currentMemoryManager->GetMemoryAmount();
    EXPECT_EQ(currentMemoryAmount, count * positiveSize);

    auto *parentMemoryManager = currentMemoryManager->GetParent();
    EXPECT_TRUE(parentMemoryManager != nullptr);
    int64_t parentMemoryAmount = parentMemoryManager->GetMemoryAmount();
    EXPECT_EQ(parentMemoryAmount, count * positiveSize);

    for (int i = 0; i < count; ++i) {
        currentMemoryManager->SubMemory(negativeSize);
    }
    currentMemoryAmount = currentMemoryManager->GetMemoryAmount();
    EXPECT_EQ(currentMemoryAmount, 0);

    parentMemoryAmount = parentMemoryManager->GetMemoryAmount();
    EXPECT_EQ(parentMemoryAmount, 0);
}

// test: limited account
TEST(MemoryManager, testLimitedAccount)
{
    // Enable the exception path explicitly; production defaults disable it.
    setenv("OMNI_DISABLE_MEM_CAP_EXCEEDED", "0", 1);
    auto threadMemoryManager = mem::ThreadMemoryManager::GetThreadMemoryManager();
    threadMemoryManager->Clear();

    int64_t globalThreshold = 10 * 1024 * 1024;
    mem::MemoryManager::SetGlobalMemoryLimit(globalThreshold);
    auto globalMemoryManager = mem::MemoryManager::GetGlobalMemoryManager();
    auto currentMemoryManager = std::make_unique<mem::MemoryManager>(globalMemoryManager);
    EXPECT_TRUE(currentMemoryManager != nullptr);

    int count = 20;
    int64_t positiveSize = 1 * 1024 * 1024;
    for (int i = 0; i < count; ++i) {
        try {
            currentMemoryManager->AddMemory(positiveSize);
        } catch (exception::OmniException &e) {
            break;
        }
    }

    int64_t currentMemoryAmount = currentMemoryManager->GetMemoryAmount();
    EXPECT_EQ(currentMemoryAmount, globalThreshold);

    auto *parentMemoryManager = currentMemoryManager->GetParent();
    EXPECT_TRUE(globalMemoryManager != nullptr);
    int64_t parentMemoryAmount = parentMemoryManager->GetMemoryAmount();
    EXPECT_EQ(parentMemoryAmount, globalThreshold);
    threadMemoryManager->Clear();
    unsetenv("OMNI_DISABLE_MEM_CAP_EXCEEDED");
}
}
