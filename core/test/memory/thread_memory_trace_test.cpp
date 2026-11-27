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
#include "memory/thread_memory_trace.h"
#include "vector/vector.h"
#include "vector/vector_helper.h"
#include "allocator.h"

namespace omniruntime::mem::test {
using namespace omniruntime::vec;

template <typename T> static BaseVector *CreateVector()
{
    int size = 100;
    if constexpr (std::is_same_v<T, std::string_view>) {
        BaseVector *varcharVector = new Vector<LargeStringContainer<std::string_view>>(size);
        return varcharVector;
    } else {
        BaseVector *fixedVector = new Vector<T>(size);
        return fixedVector;
    }
}

template <typename T> static BaseVector *CreateDictionaryVector()
{
    int32_t dicSize = 100;
    int32_t valueSize = 10;
    auto *values = new int32_t[valueSize];
    for (int32_t i = 0; i < valueSize; ++i) {
        values[i] = i % dicSize;
    }

    auto originVec = std::make_unique<Vector<int64_t>>(dicSize);
    for (int32_t index = 0; index < dicSize; ++index) {
        if (index % 2 == 0) {
            originVec->SetNull(index);
            continue;
        }
        originVec->SetValue(index, index);
    }

    auto dicVec = VectorHelper::CreateDictionary(values, valueSize, originVec.get());
    delete[] values;
    return dicVec;
}

#ifdef TRACE
// scenario test
TEST(MemoryTrace, testNoMemoryLeak)
{
    auto trace = ThreadMemoryTrace::GetThreadMemoryTrace();
    auto intVector = CreateVector<int32_t>();
    auto varcharVector = CreateVector<std::string_view>();
    auto longDicVector = CreateDictionaryVector<int64_t>();
    delete intVector;
    delete varcharVector;
    delete longDicVector;

    int32_t arenaSize = 100;
    Allocator *allocator = Allocator::GetAllocator();
    void *arena = allocator->Alloc(arenaSize);
    allocator->Free(arena, arenaSize);

    EXPECT_FALSE(trace->HasMemoryLeak());
    auto vectorLeakCount = trace->GetArenaTraced().size();
    EXPECT_EQ(vectorLeakCount, 0);
    auto arenaLeakCount = trace->GetArenaTraced().size();
    EXPECT_EQ(arenaLeakCount, 0);
}

TEST(MemoryTrace, testNewVectorLeak)
{
    auto trace = ThreadMemoryTrace::GetThreadMemoryTrace();
    // new vector leak
    auto intVector = CreateVector<int32_t>();
    auto varcharVector = CreateVector<std::string_view>();
    auto longDicVector = CreateDictionaryVector<int64_t>();
    EXPECT_TRUE(trace->HasMemoryLeak());
    EXPECT_EQ(trace->GetVectorTraced().size(), 3);
    std::unordered_map<uintptr_t, int64_t> map = trace->GetVectorTraced();
    EXPECT_TRUE(map.find(reinterpret_cast<uintptr_t>(intVector)) != map.end());
    EXPECT_TRUE(map.find(reinterpret_cast<uintptr_t>(varcharVector)) != map.end());
    EXPECT_TRUE(map.find(reinterpret_cast<uintptr_t>(longDicVector)) != map.end());

    delete intVector;
    delete varcharVector;
    delete longDicVector;
    EXPECT_FALSE(trace->HasMemoryLeak());
}

TEST(MemoryTrace, testArenaMemoryLeak)
{
    auto trace = ThreadMemoryTrace::GetThreadMemoryTrace();
    // new vector leak
    int32_t arenaSize = 100;
    Allocator *allocator = Allocator::GetAllocator();
    void *arena = allocator->Alloc(arenaSize);

    EXPECT_TRUE(trace->HasMemoryLeak());
    EXPECT_EQ(trace->GetArenaTraced().size(), 1);

    allocator->Free(arena, arenaSize);
    EXPECT_FALSE(trace->HasMemoryLeak());
}

// function test
TEST(MemoryTrace, testAddVectorMemory)
{
    int size = 100;
    auto trace = ThreadMemoryTrace::GetThreadMemoryTrace();
    auto vector = new BaseVector();
    trace->AddVectorMemory(reinterpret_cast<uintptr_t>(vector), size);
    int64_t allocated = trace->GetVectorTraced().find(reinterpret_cast<uintptr_t>(vector))->second;
    EXPECT_EQ(allocated, size);
    ASSERT_NO_THROW(trace->RemoveVectorMemory(reinterpret_cast<uintptr_t>(vector), size));
    delete vector;
    EXPECT_TRUE(trace->GetVectorTraced().empty());
}

TEST(MemoryTrace, testTAddVectorMemoryWithException)
{
    int size = 100;
    auto trace = ThreadMemoryTrace::GetThreadMemoryTrace();
    auto vector1 = new BaseVector();
    trace->AddVectorMemory(reinterpret_cast<uintptr_t>(vector1), size);

    // test exception: wrong vector size
    // match error info
    EXPECT_ANY_THROW(trace->RemoveVectorMemory(reinterpret_cast<uintptr_t>(vector1), 0));

    trace->RemoveVectorMemory(reinterpret_cast<uintptr_t>(vector1), size);
    delete vector1;
}

TEST(MemoryTrace, testReplaceVectorTraced)
{
    int size = 100;
    auto trace = ThreadMemoryTrace::GetThreadMemoryTrace();
    auto vector = new BaseVector();
    trace->AddVectorMemory(reinterpret_cast<uintptr_t>(vector), size);

    const std::string &stack = TraceUtil::GetStack();
    trace->ReplaceVectorTracedLog(reinterpret_cast<uintptr_t>(vector), stack);
    auto map = trace->GetVectorTracedWithLog();
    std::unordered_map<uintptr_t, std::pair<int64_t, std::string>>::iterator iter;
    if ((iter = map.find(reinterpret_cast<uintptr_t>(vector))) != map.end()) {
        EXPECT_EQ(iter->second.first, size);
        EXPECT_EQ(iter->second.second, stack);
    }
    delete vector;
}

TEST(MemoryTrace, testReplaceVectorTracedWithException)
{
    int size = 100;
    auto trace = ThreadMemoryTrace::GetThreadMemoryTrace();
    auto vector1 = new BaseVector();
    trace->AddVectorMemory(reinterpret_cast<uintptr_t>(vector1), size);

    auto vector2 = new BaseVector();
    const std::string &stack = TraceUtil::GetStack();
    EXPECT_ANY_THROW(trace->ReplaceVectorTracedLog(reinterpret_cast<uintptr_t>(vector2), stack));

    delete vector1;
    delete vector2;
}

TEST(MemoryTrace, testAddArenaMemory)
{
    int size = 100;
    auto trace = ThreadMemoryTrace::GetThreadMemoryTrace();
    MemoryPool *pool = GetMemoryPool();
    uint8_t *arena = nullptr;
    pool->Allocate(size, &arena);
    trace->AddArenaMemory(reinterpret_cast<uintptr_t>(arena), size);

    int64_t allocated = trace->GetArenaTraced().find(reinterpret_cast<uintptr_t>(arena))->second;
    EXPECT_EQ(allocated, size);
    ASSERT_NO_THROW(trace->RemoveArenaMemory(reinterpret_cast<uintptr_t>(arena), size));

    pool->Release(arena);
    EXPECT_TRUE(trace->GetArenaTraced().empty());
}

TEST(MemoryTrace, testAddArenaMemoryWithException)
{
    int size = 100;
    auto trace = ThreadMemoryTrace::GetThreadMemoryTrace();
    MemoryPool *pool = GetMemoryPool();
    uint8_t *arena1 = nullptr;
    pool->Allocate(size, &arena1);
    trace->AddArenaMemory(reinterpret_cast<uintptr_t>(arena1), size);

    // test exception: wrong arena size
    // match error info
    EXPECT_ANY_THROW(trace->RemoveArenaMemory(reinterpret_cast<uintptr_t>(arena1), 0));

    trace->RemoveArenaMemory(reinterpret_cast<uintptr_t>(arena1), size);
    pool->Release(arena1);
}
#endif
} // namespace omniruntime::mem::test
