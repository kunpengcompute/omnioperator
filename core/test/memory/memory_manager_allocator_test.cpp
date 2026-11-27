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

#include <iostream>
#include <unordered_map>
#include <map>
#include <vector>
#include <deque>
#include <list>
#include <set>
#include "gtest/gtest.h"
#include "memory/memory_manager_allocator.h"

namespace omniruntime::mem::test {
TEST(MemoryManagerAllocator, testUseSTLContainer)
{
    // test std::map
    std::map<std::string, int64_t, std::less<std::string>, mem::MemoryManagerAllocator<std::pair<std::string, int64_t>>>
        map;
    // test std::unordered_map
    std::unordered_map<std::string, int64_t, std::hash<std::string>, std::equal_to<std::string>,
                       mem::MemoryManagerAllocator<std::pair<std::string, int64_t>>>
        unorderedMap;
    // test std::vector
    std::vector<int64_t, mem::MemoryManagerAllocator<int64_t>> vector;
    // test std::deque
    std::deque<int64_t, mem::MemoryManagerAllocator<int64_t>> deque;
    // test std::list
    std::list<int64_t, mem::MemoryManagerAllocator<int64_t>> list;
    // test std::set
    std::set<int64_t, std::less<int64_t>, mem::MemoryManagerAllocator<int64_t>> set;

    for (int i = 0; i <= 1024; ++i) {
        map[std::to_string(i)] += i * 1024;
        unorderedMap[std::to_string(i)] += i * 1024;
        vector.push_back(i * 1024);
        deque.push_back(i * 1024);
        list.push_back(i * 1024);
        set.insert(i * 1024);
    }

    auto it = set.begin();
    for (int i = 0; i <= 1024; ++i) {
        EXPECT_EQ(map[std::to_string(i)], i * 1024);
        map.erase(std::to_string(i));
        EXPECT_EQ(unorderedMap[std::to_string(i)], i * 1024);
        unorderedMap.erase(std::to_string(i));
        EXPECT_EQ(vector[i], i * 1024);
        EXPECT_EQ(list.front(), i * 1024);
        list.pop_front();
        EXPECT_EQ(*it, i * 1024);
        it++;
    }
}
} // namespace omniruntime::mem::test
