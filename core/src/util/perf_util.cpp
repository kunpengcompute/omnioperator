/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
 * KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 *
 * Existing third-party copyright and license terms continue to apply.
 *
 * Copyright 1999-2016 Randy Heit
 * Copyright 2019-2020 Christoph Oelckers
 * All rights reserved.
 *
 *
 *
 * Third-party license and source notices
 *
 * Description: Inner supported aggregators source file
 *
 * ---------------------------------------------------------------------------
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * ---------------------------------------------------------------------------
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
