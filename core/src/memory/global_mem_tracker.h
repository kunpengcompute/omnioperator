/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 *
 * Compatibility accessors for process-wide memory state after accounting moved
 * to per-task pools.
 *
 *   - GetGlobalAccountedMemory(): on-demand sum of live OmniTaskMemoryPools. There is
 *     deliberately no process-global atomic updated by every allocation/free.
 *   - GetGlobalMemoryLimit(): the executor offheap budget Gluten passes at init via
 *     nova.hetu.omniruntime.memory.MemoryManager.setGlobalMemoryLimit(offHeapSize).
 *     GLOBAL_MEM_UNLIMITED until set → threshold stays INT64_MAX → never triggers.
 *
 * NOT an allocation gate: it does NOT enforce a cap or throw MEM_CAP_EXCEEDED — Spark
 * owns per-task enforcement through the AllocationListener. Definitions live in
 * omni_task_memory_pool.cpp (no new build target).
 */
#ifndef OMNI_RUNTIME_MEMORY_GLOBAL_MEM_TRACKER_H
#define OMNI_RUNTIME_MEMORY_GLOBAL_MEM_TRACKER_H

#include <cstdint>

namespace omniruntime {
namespace mem {

constexpr int64_t GLOBAL_MEM_UNLIMITED = -1;

int64_t GetGlobalAccountedMemory();

void SetGlobalMemoryLimit(int64_t limit);
int64_t GetGlobalMemoryLimit();

} // namespace mem
} // namespace omniruntime

#endif // OMNI_RUNTIME_MEMORY_GLOBAL_MEM_TRACKER_H
