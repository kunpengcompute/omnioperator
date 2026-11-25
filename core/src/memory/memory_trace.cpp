/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All rights reserved.
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "memory_trace.h"
#include "allocator.h"
#include "vector/vector.h"

namespace omniruntime::mem {
void MemoryTrace::AddVectorMemory(uintptr_t ptr, int64_t size)
{
#ifdef TRACE
    ThreadMemoryTrace *threadMemoryTrace = ThreadMemoryTrace::GetThreadMemoryTrace();
    threadMemoryTrace->AddVectorMemory(ptr, size);
#endif
}

void MemoryTrace::SubVectorMemory(uintptr_t ptr, int64_t size)
{
#ifdef TRACE
    ThreadMemoryTrace *threadMemoryTrace = ThreadMemoryTrace::GetThreadMemoryTrace();
    threadMemoryTrace->RemoveVectorMemory(ptr, size);
#endif
}

void MemoryTrace::AddArenaMemory(uintptr_t ptr, int64_t size)
{
#ifdef TRACE
    ThreadMemoryTrace *threadMemoryTrace = ThreadMemoryTrace::GetThreadMemoryTrace();
    threadMemoryTrace->AddArenaMemory(ptr, size);
#endif
}

void MemoryTrace::SubArenaMemory(uintptr_t ptr, int64_t size)
{
#ifdef TRACE
    ThreadMemoryTrace *threadMemoryTrace = ThreadMemoryTrace::GetThreadMemoryTrace();
    threadMemoryTrace->RemoveArenaMemory(ptr, size);
#endif
}

MemoryTrace::MemoryTrace() {}

MemoryTrace::~MemoryTrace() { threadMemoryTraceSet.clear(); }

void MemoryTrace::AddThreadMemoryTrace(ThreadMemoryTrace *threadMemoryTrace)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    threadMemoryTraceSet.emplace(threadMemoryTrace);
}

void MemoryTrace::SubThreadMemoryTrace(ThreadMemoryTrace *threadMemoryTrace)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    threadMemoryTraceSet.erase(threadMemoryTrace);
}

const std::unordered_set<ThreadMemoryTrace *> &MemoryTrace::GetThreadMemoryTraceSet() { return threadMemoryTraceSet; }

static MemoryTrace g_globalMemoryTrace;
MemoryTrace *GetMemoryTrace() { return &g_globalMemoryTrace; }
} // namespace omniruntime::mem
