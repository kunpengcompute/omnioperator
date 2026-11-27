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

#include "perf_util.h"
#include <libboundscheck/include/securec.h>
#include <cstdio>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <linux/perf_event.h>

void PerfUtil::Init()
{
    struct perf_event_attr pe {};
    memset_s(&pe, sizeof(struct perf_event_attr), 0, sizeof(struct perf_event_attr));
    pe.type = PERF_TYPE_HARDWARE;
    pe.size = sizeof(struct perf_event_attr);
    pe.config = PERF_COUNT_HW_INSTRUCTIONS;
    // can also enable other perf event
    // branch miss event: PERF_COUNT_HW_BRANCH_MISSES;
    pe.disabled = 1;
    pe.exclude_kernel = 1;
    pe.exclude_hv = 1;

    this->fd = syscall(SYS_perf_event_open, &pe, 0, -1, -1, 0);
    if (fd == -1) {
        perror("Error initializing PerfUtil: ");
        return;
    }
    this->initialized = true;
}

void PerfUtil::Start() const
{
    if (initialized) {
        ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);
    }
}

void PerfUtil::Stop() const
{
    if (initialized) {
        ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
    }
}

void PerfUtil::Reset() const
{
    if (initialized) {
        ioctl(fd, PERF_EVENT_IOC_RESET, 0);
    }
}

long long PerfUtil::GetData() const
{
    if (!initialized) {
        return -1;
    }

    long long count;
    if (!read(fd, &count, sizeof(long long))) {
        return count;
    }
    return -1;
}

PerfUtil::~PerfUtil()
{
    if (initialized) {
        if (close(fd) == -1) {
            perror("PerfUtil close error");
        }
        fd = -1;
    }
}
