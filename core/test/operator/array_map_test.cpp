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

#include <vector>
#include "gtest/gtest.h"
#include "operator/hashmap/array_map.h"
#include "operator/join/row_ref.h"
#include "operator/execution_context.h"

namespace HashMapTest {
using namespace omniruntime::op;
TEST(ArrayMapTest, TestForAllFunctions)
{
    DefaultArrayMap<RowRefList> hashMap(64);
    std::vector<size_t> positions{0, 1, 10, 20, 30, 40, 50};
    RowRefList *rowRef;
    auto executionContext = std::make_unique<ExecutionContext>();
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
        ret.GetValue()->Insert({static_cast<uint32_t>(i + positions.size()), static_cast<uint32_t>(1)},
                               *arenaAllocator);
    }

    auto ret = hashMap.InsertNullKeysToHashmap();
    EXPECT_TRUE(ret.IsInsert());
    rowRef = reinterpret_cast<RowRefList *>(arenaAllocator->Allocate(sizeof(RowRefList)));
    *rowRef = RowRefList(static_cast<uint32_t>(2 * positions.size()), static_cast<uint32_t>(2));
    ret.SetValue(rowRef);

    auto ret1 = hashMap.InsertNullKeysToHashmap();
    EXPECT_FALSE(ret1.IsInsert());
    ret1.GetValue()->Insert({static_cast<uint32_t>(2 * positions.size() + 1), 2}, *arenaAllocator);

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
} // namespace HashMapTest
