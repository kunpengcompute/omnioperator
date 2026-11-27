/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2023. All rights reserved.
 */

#include <vector>
#include "gtest/gtest.h"
#include "operator/hashmap/array_map.h"
#include "operator/join/row_ref.h"
#include "operator/execution_context.h"

namespace HashMapTest {
using namespace omniruntime::op;
TEST(ArrayMapTest, TestForAllFunctions)
{
    // executionContext must be created before hashMap so we can pass its allocator
    // (OmniHashmapAllocator requires an explicit Allocator*).
    auto executionContext = std::make_unique<ExecutionContext>(4096, nullptr);
    DefaultArrayMap<RowRefList> hashMap(64, OmniHashmapAllocator(executionContext->GetAllocator()));
    std::vector<size_t> positions { 0, 1, 10, 20, 30, 40, 50 };
    RowRefList *rowRef;
    auto arenaAllocator = executionContext->GetArena();
    for (size_t i = 0; i < positions.size(); ++i) {
        auto ret = hashMap.InsertJoinKeysToHashmap(positions[i]);
        EXPECT_TRUE(ret.IsInsert());
        rowRef = reinterpret_cast<RowRefList *>(arenaAllocator->Allocate(sizeof(RowRefList)));
        *rowRef = RowRefList(static_cast<uint32_t>(i), static_cast<uint32_t>(0));
        ret.SetValue(rowRef);
    }

    for (size_t i = 0; i < positions.size(); ++i) {
        auto ret = hashMap.InsertJoinKeysToHashmap(positions[i]);
        EXPECT_FALSE(ret.IsInsert());
        ret.GetValue()->Insert({ static_cast<uint32_t>(i + positions.size()), static_cast<uint32_t>(1) },
            *arenaAllocator);
    }

    auto ret = hashMap.InsertNullKeysToHashmap();
    EXPECT_TRUE(ret.IsInsert());
    rowRef = reinterpret_cast<RowRefList *>(arenaAllocator->Allocate(sizeof(RowRefList)));
    *rowRef = RowRefList(static_cast<uint32_t>(2 * positions.size()), static_cast<uint32_t>(2));
    ret.SetValue(rowRef);

    auto ret1 = hashMap.InsertNullKeysToHashmap();
    EXPECT_FALSE(ret1.IsInsert());
    ret1.GetValue()->Insert({ static_cast<uint32_t>(2 * positions.size() + 1), 2 }, *arenaAllocator);

    EXPECT_EQ(hashMap.GetElementsSize(), 7);
    hashMap.ForEachValue([&](const auto &value, int index) {
        auto it = value->Begin();
        while (it.IsOk()) {
            std::cout << "[" << it->rowIdx << ", " << it->vecBatchIdx << "] ";
            ++it;
        }
        std::cout << std::endl;
    });
}
}