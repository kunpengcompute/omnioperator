/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 *
 * Description: This file declares for process_base.cpp
 */
#include "compute/process_base.h"
#include <ctime>

namespace omniruntime::compute {

const int64_t NANOS_PER_SEC = 1'000'000'000;

static int64_t ThreadCpuNanos()
{
#ifdef __aarch64__
        int64_t time;
        asm volatile("mrs %0, cntvct_el0" : "=r" (time));
        return time;
#else
        timespec ts{};
        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
        return ts.tv_sec * NANOS_PER_SEC + ts.tv_nsec;
#endif
}
} // namespace omniruntime::compute