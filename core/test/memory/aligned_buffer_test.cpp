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
#include "memory/thread_memory_manager.h"
#include "memory/aligned_buffer.h"
#include "vector/vector_test_util.h"

namespace omniruntime::mem::test {
using namespace vec;
using namespace vec::test;
TEST(AlignedBuffer, testCreateAlignedBuffer)
{
    auto threadMemoryManager = ThreadMemoryManager::GetThreadMemoryManager();
    threadMemoryManager->Clear();

    int size = 100;
    std::shared_ptr<AlignedBuffer<bool>> nullsBuffer = std::make_shared<AlignedBuffer<bool>>(size);
    std::shared_ptr<AlignedBuffer<int64_t>> valuesBuffer = std::make_shared<AlignedBuffer<int64_t>>(size);

    bool *nulls = nullsBuffer->GetBuffer();
    int64_t *values = valuesBuffer->GetBuffer();
    EXPECT_TRUE(nulls != nullptr);
    EXPECT_TRUE(values != nullptr);

    int64_t untrackedMemory = threadMemoryManager->GetUntrackedMemory();
    // mem(900) is equal to the sum of nullsBuffer and valuesBuffer, i.e., nullsBuffer(100) + valuesBuffer(800);
    EXPECT_EQ(untrackedMemory, 900);

    nullsBuffer.reset();
    valuesBuffer.reset();
    untrackedMemory = threadMemoryManager->GetUntrackedMemory();
    EXPECT_EQ(untrackedMemory, 0);
}

TEST(AlignedBuffer, testAlignedBufferGetValue)
{
    auto threadMemoryManager = ThreadMemoryManager::GetThreadMemoryManager();
    threadMemoryManager->Clear();

    int dictionarySize = 10;
    int valueSize = 100;
    int *values = new int[valueSize];
    for (int i = 0; i < valueSize; i++) {
        values[i] = i % dictionarySize;
    }

    auto dictionary = CreateDictionary<int64_t>(dictionarySize);
    auto container = std::make_shared<DictionaryContainer<int64_t>>(values, valueSize, dictionary, dictionarySize, 0);
    for (int i = 0; i < dictionarySize; ++i) {
        EXPECT_EQ(container->GetValue(i), (i * 2) / 3);
    }
    delete[] values;
    dictionary.reset();
    container.reset();
    int64_t untrackedMemory = threadMemoryManager->GetUntrackedMemory();
    EXPECT_EQ(untrackedMemory, 0);
}
} // namespace omniruntime::mem::test
