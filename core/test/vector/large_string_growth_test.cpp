/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include <gtest/gtest.h>
#include "memory/pool_listener.h"
#include "memory/omni_task_memory_pool.h"
#include "vector/large_string_container.h"

namespace omniruntime::vec {
namespace {
class StringGrowthListener : public omniruntime::mem::PoolListener {
public:
    bool deny = false;
    int borrows = 0;
    int64_t billed = 0;
    void allocationChanged(int64_t delta) override
    {
        if (delta > 0) {
            if (deny) throw mem::ReservationDenied("string growth denied");
            ++borrows;
        }
        billed += delta;
    }
};
}

TEST(LargeStringGrowthTest, IncrementalOffsetsGrowGeometrically)
{
    StringGrowthListener listener;
    mem::OmniTaskMemoryPool pool(&listener);
    {
        LargeStringContainer<std::string_view> strings(1, 16, &pool);
        strings.SetValue(0, "kept");
        const int before = listener.borrows;
        for (int rows = 2; rows <= 100000; ++rows) strings.Expand(rows);
        EXPECT_LT(listener.borrows - before, 20);
        EXPECT_EQ(strings.GetValue(0), "kept");
    }
    EXPECT_EQ(pool.currentBytes(), 0);
    pool.releaseUnusedReservation();
    EXPECT_EQ(listener.billed, 0);
}

TEST(LargeStringGrowthTest, DeniedContentGrowthPreservesDataAndAccounting)
{
    StringGrowthListener listener;
    mem::OmniTaskMemoryPool pool(&listener);
    {
        LargeStringContainer<std::string_view> strings(2, 4, &pool);
        strings.SetValue(0, "abc");
        const auto bytes = pool.currentBytes();
        listener.deny = true;
        const std::string large(512, 'x');
        EXPECT_THROW(strings.SetValue(1, large), mem::ReservationDenied);
        EXPECT_EQ(pool.currentBytes(), bytes);
        EXPECT_EQ(strings.GetValue(0), "abc");
        listener.deny = false;
        strings.SetValue(1, large);
        EXPECT_EQ(strings.GetValue(0), "abc");
        EXPECT_EQ(strings.GetValue(1), large);
    }
    EXPECT_EQ(pool.currentBytes(), 0);
    pool.releaseUnusedReservation();
    EXPECT_EQ(listener.billed, 0);
}

TEST(LargeStringGrowthTest, DeniedOffsetsGrowthPreservesDataAndAccounting)
{
    StringGrowthListener listener;
    mem::OmniTaskMemoryPool pool(&listener);
    {
        LargeStringContainer<std::string_view> strings(1, 16, &pool);
        strings.SetValue(0, "abc");
        const auto bytes = pool.currentBytes();
        listener.deny = true;
        EXPECT_THROW(strings.Expand(100000), mem::ReservationDenied);
        EXPECT_EQ(pool.currentBytes(), bytes);
        EXPECT_EQ(strings.GetValue(0), "abc");
        listener.deny = false;
        strings.Expand(100000);
        EXPECT_EQ(strings.GetValue(0), "abc");
    }
    EXPECT_EQ(pool.currentBytes(), 0);
    pool.releaseUnusedReservation();
    EXPECT_EQ(listener.billed, 0);
}
} // namespace omniruntime::vec
