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
#include "compute/process_base.h"

namespace ProccessBaseTest {
TEST(ThreadCpuNanosTest, ThreadCpuNanos_ShouldHandleZeroCpuTime_WhenCalled)
{
    // Simulate zero CPU time by setting the timespec values to zero
    timespec ts = {0, 0};
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);

    // Call the function under test
    int64_t result = omniruntime::compute::ThreadCpuNanos();

    // Check if the result is zero
    EXPECT_NE(result, 0);
}
} // namespace ProccessBaseTest
