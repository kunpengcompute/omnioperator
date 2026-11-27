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
#include "vector/vector_common.h"

namespace omniruntime::vec::test {
TEST(VectorNulls, nullsbuffer)
{
    auto nullsBuffer1 = std::make_shared<NullsBuffer>(100);
    EXPECT_FALSE(nullsBuffer1->HasNull());
    nullsBuffer1->SetNull(0, true);
    nullsBuffer1->SetNulls(50, true, 10);
    nullsBuffer1->SetNull(60, true);
    nullsBuffer1->SetNull(99, true);
    EXPECT_TRUE(nullsBuffer1->HasNull());
    EXPECT_TRUE(nullsBuffer1->IsNull(0));
    EXPECT_TRUE(nullsBuffer1->IsNull(50));
    EXPECT_TRUE(nullsBuffer1->IsNull(59));
    EXPECT_TRUE(nullsBuffer1->IsNull(60));
    EXPECT_TRUE(nullsBuffer1->IsNull(99));
    EXPECT_EQ(nullsBuffer1->GetNullCount(), 13);
    EXPECT_TRUE(nullsBuffer1->HasNull());
    nullsBuffer1->SetNull(0, false);
    nullsBuffer1->SetNull(55, false);
    nullsBuffer1->SetNull(99, false);
    EXPECT_EQ(nullsBuffer1->GetNullCount(), 10);

    auto nullsBuffer2 = std::make_shared<NullsBuffer>(100);
    EXPECT_FALSE(nullsBuffer2->HasNull());
    nullsBuffer2->SetNulls(10, nullsBuffer1.get(), 60);
    EXPECT_TRUE(nullsBuffer2->HasNull());
    EXPECT_TRUE(nullsBuffer2->IsNull(60));
    EXPECT_FALSE(nullsBuffer2->IsNull(65));
    EXPECT_TRUE(nullsBuffer2->IsNull(69));
    EXPECT_EQ(nullsBuffer2->GetNullCount(), 9);

    auto nullsBuffer3 = std::make_shared<NullsBuffer>(60, nullsBuffer1.get());
    EXPECT_TRUE(nullsBuffer3->HasNull());
    EXPECT_TRUE(nullsBuffer3->IsNull(50));
    EXPECT_FALSE(nullsBuffer3->IsNull(55));
    EXPECT_TRUE(nullsBuffer3->IsNull(59));
    EXPECT_EQ(nullsBuffer3->GetNullCount(), 9);

    auto nullsBuffer4 = std::make_shared<NullsBuffer>(50, nullsBuffer1.get(), 10);
    EXPECT_TRUE(nullsBuffer4->HasNull());
    EXPECT_TRUE(nullsBuffer4->IsNull(40));
    EXPECT_FALSE(nullsBuffer4->IsNull(45));
    EXPECT_TRUE(nullsBuffer4->IsNull(49));
    EXPECT_EQ(nullsBuffer4->GetNullCount(), 9);

    auto nullsBuffer5 = std::make_shared<NullsBuffer>(60, nullsBuffer1.get(), 8);
    EXPECT_TRUE(nullsBuffer5->HasNull());
    EXPECT_TRUE(nullsBuffer5->IsNull(42));
    EXPECT_FALSE(nullsBuffer5->IsNull(47));
    EXPECT_TRUE(nullsBuffer5->IsNull(51));
    EXPECT_TRUE(nullsBuffer5->IsNull(52));
    EXPECT_EQ(nullsBuffer5->GetNullCount(), 10);
}

TEST(VectorNulls, nullsHelper)
{
    auto nullsBuffer = std::make_shared<NullsBuffer>(100);
    nullsBuffer->SetNull(0, true);
    nullsBuffer->SetNulls(50, true, 10);
    nullsBuffer->SetNull(60, true);
    nullsBuffer->SetNull(99, true);

    auto nullsHelper = std::make_shared<NullsHelper>(nullsBuffer);
    EXPECT_TRUE((*nullsHelper)[0]);
    EXPECT_TRUE((*nullsHelper)[50]);
    EXPECT_TRUE((*nullsHelper)[60]);
    EXPECT_TRUE((*nullsHelper)[99]);

    nullsHelper->SetNull(10, true);
    EXPECT_TRUE((*nullsHelper)[10]);

    *nullsHelper += 10;
    EXPECT_TRUE((*nullsHelper)[0]);
    EXPECT_TRUE((*nullsHelper)[40]);
    EXPECT_TRUE((*nullsHelper)[50]);
    EXPECT_TRUE((*nullsHelper)[89]);

    auto arr = nullsHelper->convertToArray(50);
    EXPECT_EQ(arr[0], 1);
    EXPECT_EQ(arr[39], 0);
    EXPECT_EQ(arr[40], 1);
    EXPECT_EQ(arr[49], 1);
}
} // namespace omniruntime::vec::test
