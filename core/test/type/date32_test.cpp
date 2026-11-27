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
#include "type/date32.h"

using namespace omniruntime::type;

namespace Date32Test {
TEST(Date32, add_normal)
{
    auto left = new Date32(20210302);
    auto right = new Date32(302);
    auto result = *left + *right;
    EXPECT_EQ(result.Value(), 20210604);
    delete left;
    delete right;
}

TEST(Date32, add_negate)
{
    auto left = new Date32(20210302);
    auto right = new Date32(-201);
    auto result = *left + *right;
    EXPECT_EQ(result.Value(), 20210101);
    delete left;
    delete right;
}

TEST(Date32, sub_normal)
{
    auto left = new Date32(20210302);
    auto right = new Date32(201);
    auto result = *left - *right;
    EXPECT_EQ(result.Value(), 20210101);
    delete left;
    delete right;
}

TEST(Date32, sub_negate)
{
    auto left = new Date32(20210302);
    auto right = new Date32(-302);
    auto result = *left - *right;
    EXPECT_EQ(result.Value(), 20210604);
    delete left;
    delete right;
}

TEST(Date32, compare_eq)
{
    auto left = new Date32(20210302);
    auto right = new Date32(20210302);
    auto result = *left == *right;
    EXPECT_EQ(result, true);
    delete left;
    delete right;
}

TEST(Date32, compare_ne)
{
    auto left = new Date32(20210302);
    auto right = new Date32(20210303);
    auto result = *left != *right;
    EXPECT_EQ(result, true);
    delete left;
    delete right;
}

TEST(Date32, compare_le1)
{
    auto left = new Date32(20210302);
    auto right = new Date32(20210303);
    auto result = *left <= *right;
    EXPECT_EQ(result, true);
    delete left;
    delete right;
}

TEST(Date32, compare_le2)
{
    auto left = new Date32(20210302);
    auto right = new Date32(20210302);
    auto result = *left <= *right;
    EXPECT_EQ(result, true);
    delete left;
    delete right;
}

TEST(Date32, compare_lt)
{
    auto left = new Date32(20210302);
    auto right = new Date32(20210303);
    auto result = *left < *right;
    EXPECT_EQ(result, true);
    delete left;
    delete right;
}

TEST(Date32, compare_ge1)
{
    auto left = new Date32(20210303);
    auto right = new Date32(20210302);
    auto result = *left >= *right;
    EXPECT_EQ(result, true);
    delete left;
    delete right;
}

TEST(Date32, compare_ge2)
{
    auto left = new Date32(20210302);
    auto right = new Date32(20210302);
    auto result = *left >= *right;
    EXPECT_EQ(result, true);
    delete left;
    delete right;
}

TEST(Date32, compare_gt)
{
    auto left = new Date32(20210303);
    auto right = new Date32(20210302);
    auto result = *left > *right;
    EXPECT_EQ(result, true);
    delete left;
    delete right;
}
} // namespace Date32Test
