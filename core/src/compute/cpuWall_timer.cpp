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

#include "cpuWall_timer.h"

namespace omniruntime::compute {

CpuWallTimer::CpuWallTimer(CpuWallTiming &timing) : timing_(timing)
{
    ++timing_.count;
    cpuTimeStart_ = ThreadCpuNanos();
    wallTimeStart_ = 0;
}

CpuWallTimer::~CpuWallTimer()
{
    timing_.cpuNanos += ThreadCpuNanos() - cpuTimeStart_;
    timing_.wallNanos += 0;
}

} // namespace omniruntime::compute
