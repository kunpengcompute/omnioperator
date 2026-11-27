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

/*
 * @Description: spill tracker implementation
 */

#include "spill_tracker.h"
#include "util/debug.h"

namespace omniruntime {
namespace op {
bool ChildSpillTracker::CheckIfExceedAndReserve(uint64_t bytes)
{
    if (spilledBytes + bytes >= maxSpillBytes) {
        LogInfo("Spilled size exceeded the spill limit of %llu.", maxSpillBytes);
        return true;
    } else {
        spilledBytes += bytes;
        return false;
    }
}

RootSpillTracker::~RootSpillTracker() = default;

bool RootSpillTracker::CheckIfExceedAndReserve(uint64_t bytes)
{
    if (spilledBytes + bytes >= maxBytes) {
        LogInfo("Spilled size exceeded the spill limit of %llu.", maxBytes);
        return true;
    }

    spilledBytes.fetch_add(bytes);
    totalSpilledBytes += bytes;
    return false;
}

void InitRootSpillTracker(uint64_t maxSize) { GetRootSpillTracker().SetMaxBytes(maxSize); }

RootSpillTracker &GetRootSpillTracker()
{
    static RootSpillTracker rootSpillTracker;
    return rootSpillTracker;
}
} // namespace op
} // namespace omniruntime
