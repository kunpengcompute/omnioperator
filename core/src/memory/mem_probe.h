/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 *
 * mem_probe.h — OMNI_MEM_DEBUG-only memory instrumentation (diagnostic only).
 *
 * Purpose: assemble the "memory-accounting completeness" evidence at the allocator
 * boundary. Two things, both compile-gated by OMNI_MEM_DEBUG so release builds carry
 * zero overhead and zero symbol presence:
 *
 *   1. Symmetric allocator-traffic coverage. Both branches are recorded — pool-bound
 *      (covered) AND pool-less (uncovered) — each in bytes AND count, keyed by the
 *      allocation call-site return address. The covered/(covered+uncovered) ratio is
 *      then read directly, in MB and in #allocs.
 *
 *   2. jemalloc L0 ground truth. The Allocator bottoms out on jemalloc mallocx, which
 *      fills arenas via mmap — invisible to any LD_PRELOAD malloc hook. We sample
 *      jemalloc's own stats.{allocated,active,resident,mapped,retained} on a background
 *      thread and keep the high-water of each. resident-allocated localises the RSS gap
 *      (real leak vs allocator retention/fragmentation).
 *
 * Return addresses are resolved in-process via dladdr() at dump time (only the distinct
 * RAs, ~dozens), which is ASLR-immune and names the .so + offset directly — no
 * /proc/maps snapshot, no addr2line, no "(no-map)".
 *
 * Output (stderr, one dump per process at exit — with per-query-fresh-process runs this
 * is one dump per query):
 *   [MEMPROBE-JE]  allocated_peak=.. active_peak=.. resident_peak=.. mapped_peak=.. retained_peak=..
 *   [MEMPROBE-SUM] cov_alloc_bytes=.. cov_alloc_count=.. cov_free_bytes=.. cov_free_count=.. unc_*..
 *   [MEMPROBE-RA]  ra=0x.. so=<path> off=0x.. cov_bytes=.. cov_count=.. unc_bytes=.. unc_count=.. sym=<name>
 */

#ifndef OMNI_RUNTIME_MEM_PROBE_H
#define OMNI_RUNTIME_MEM_PROBE_H

#ifdef OMNI_MEM_DEBUG
#include <cstdint>
#include "util/compiler_util.h"   // ALWAYS_INLINE

namespace omniruntime::mem::probe {

// Out-of-line (defined in mem_probe.cpp) so the shared aggregation state has a single
// definition across all translation units. `ra` is the allocation call-site return
// address captured by the ALWAYS_INLINE wrappers below.
void RecordAlloc(void *ra, bool covered, int64_t size);
void RecordFree(bool covered, int64_t size);

// Per-task pool end-of-life residual (accounting self-consistency + the per-task
// dtor-drain leak). residual = usedBytes_ observed at the pool dtor; drained = the
// listener up-call fired, else the residual is a silent leak (detached JNIEnv / null
// listener). Sum of skipped residual across pools = end-of-task offheap Spark never
// reclaims.
void RecordPoolDtorResidual(int64_t residual, bool drained);

// ALWAYS_INLINE so __builtin_return_address(0) resolves in the caller's
// (Allocator::Alloc's) inlined frame, i.e. the allocation call site.
ALWAYS_INLINE static void OnAlloc(bool covered, int64_t size)
{
    RecordAlloc(__builtin_return_address(0), covered, size);
}

ALWAYS_INLINE static void OnFree(bool covered, int64_t size)
{
    RecordFree(covered, size);
}

} // namespace omniruntime::mem::probe
#endif // OMNI_MEM_DEBUG

#endif // OMNI_RUNTIME_MEM_PROBE_H
