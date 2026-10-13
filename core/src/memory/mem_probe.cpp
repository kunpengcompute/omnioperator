/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 *
 * mem_probe.cpp — OMNI_MEM_DEBUG-only memory instrumentation backing (see mem_probe.h).
 * Auto-compiled via aux_source_directory; the whole TU is empty under release builds.
 * Diagnostic only; it can be removed with the OMNI_MEM_DEBUG hooks in allocator.h.
 */

#ifdef OMNI_MEM_DEBUG
#include "mem_probe.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <thread>

#include <dlfcn.h>
#include <jemalloc/jemalloc.h>

namespace omniruntime::mem::probe {
namespace {

// ── Symmetric allocation-traffic table, keyed by call-site return address ──────────
// Open-addressing, fixed capacity, lock-free. Distinct RAs are few (prior SF1 reprobe
// saw 61); 8192 slots leaves vast headroom. ra==0 is the empty sentinel (a real RA is
// never 0).
constexpr size_t CAP = 8192;
struct Slot {
    std::atomic<uintptr_t> ra{0};
    std::atomic<int64_t> covBytes{0};
    std::atomic<int64_t> covCount{0};
    std::atomic<int64_t> uncBytes{0};
    std::atomic<int64_t> uncCount{0};
};
Slot g_table[CAP];
std::atomic<int64_t> g_overflow{0};   // allocs dropped if the table ever fills (never expected)

// ── Global per-side gross alloc / free totals (coverage ratio numerator/denominator
//    + churn = gross-vs-net). ──────────────────────────────────────────────────────
std::atomic<int64_t> g_covAllocBytes{0}, g_covAllocCount{0}, g_covFreeBytes{0}, g_covFreeCount{0};
std::atomic<int64_t> g_uncAllocBytes{0}, g_uncAllocCount{0}, g_uncFreeBytes{0}, g_uncFreeCount{0};

// ── jemalloc high-water (sampled on a background thread). ──────────────────────────
std::atomic<size_t> g_jeAllocatedPeak{0}, g_jeActivePeak{0}, g_jeResidentPeak{0},
                    g_jeMappedPeak{0}, g_jeRetainedPeak{0};
std::atomic<bool> g_samplerRun{true};

// ── Per-task pool dtor residual (accounting self-consistency + dtor-drain leak). ───
std::atomic<int64_t> g_poolDtorCount{0}, g_poolDtorDrainedCount{0}, g_poolDtorSkippedCount{0};
std::atomic<int64_t> g_poolDtorResidualDrained{0}, g_poolDtorResidualSkipped{0};

ALWAYS_INLINE size_t HashRa(uintptr_t ra)
{
    ra >>= 4;                       // drop low alignment bits
    return (ra * 1099511628211ULL) % CAP;   // FNV-ish spread
}

void BumpPeak(std::atomic<size_t> &peak, size_t v)
{
    size_t cur = peak.load(std::memory_order_relaxed);
    while (v > cur && !peak.compare_exchange_weak(cur, v, std::memory_order_relaxed)) {
    }
}

size_t JeStat(const char *name)
{
    size_t v = 0;
    size_t sz = sizeof(v);
    if (mallctl(name, &v, &sz, nullptr, 0) != 0) {
        return 0;   // stat unavailable (e.g. jemalloc built without --enable-stats)
    }
    return v;
}

void RefreshAndSample()
{
    // jemalloc caches stats until an "epoch" advance is requested.
    uint64_t epoch = 1;
    size_t esz = sizeof(epoch);
    mallctl("epoch", &epoch, &esz, &epoch, esz);
    BumpPeak(g_jeAllocatedPeak, JeStat("stats.allocated"));
    BumpPeak(g_jeActivePeak,    JeStat("stats.active"));
    BumpPeak(g_jeResidentPeak,  JeStat("stats.resident"));
    BumpPeak(g_jeMappedPeak,    JeStat("stats.mapped"));
    BumpPeak(g_jeRetainedPeak,  JeStat("stats.retained"));
}

void SamplerLoop()
{
    while (g_samplerRun.load(std::memory_order_relaxed)) {
        RefreshAndSample();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

void Dump()
{
    g_samplerRun.store(false, std::memory_order_relaxed);
    RefreshAndSample();   // final high-water reading

    fprintf(stderr,
        "[MEMPROBE-JE] allocated_peak=%zu active_peak=%zu resident_peak=%zu mapped_peak=%zu retained_peak=%zu\n",
        g_jeAllocatedPeak.load(), g_jeActivePeak.load(), g_jeResidentPeak.load(),
        g_jeMappedPeak.load(), g_jeRetainedPeak.load());

    fprintf(stderr,
        "[MEMPROBE-SUM] cov_alloc_bytes=%lld cov_alloc_count=%lld cov_free_bytes=%lld cov_free_count=%lld "
        "unc_alloc_bytes=%lld unc_alloc_count=%lld unc_free_bytes=%lld unc_free_count=%lld overflow=%lld\n",
        (long long)g_covAllocBytes.load(), (long long)g_covAllocCount.load(),
        (long long)g_covFreeBytes.load(),  (long long)g_covFreeCount.load(),
        (long long)g_uncAllocBytes.load(), (long long)g_uncAllocCount.load(),
        (long long)g_uncFreeBytes.load(),  (long long)g_uncFreeCount.load(),
        (long long)g_overflow.load());

    fprintf(stderr,
        "[MEMPROBE-POOLDTOR] pools=%lld drained=%lld skipped=%lld residual_drained=%lld residual_skipped=%lld\n",
        (long long)g_poolDtorCount.load(), (long long)g_poolDtorDrainedCount.load(),
        (long long)g_poolDtorSkippedCount.load(), (long long)g_poolDtorResidualDrained.load(),
        (long long)g_poolDtorResidualSkipped.load());

    for (size_t i = 0; i < CAP; i++) {
        uintptr_t ra = g_table[i].ra.load(std::memory_order_acquire);
        if (ra == 0) {
            continue;
        }
        int64_t cb = g_table[i].covBytes.load(std::memory_order_relaxed);
        int64_t cc = g_table[i].covCount.load(std::memory_order_relaxed);
        int64_t ub = g_table[i].uncBytes.load(std::memory_order_relaxed);
        int64_t uc = g_table[i].uncCount.load(std::memory_order_relaxed);

        const char *so = "(nodl)";
        const char *sym = "(nosym)";
        uintptr_t off = 0;
        Dl_info info;
        if (dladdr(reinterpret_cast<void *>(ra), &info) != 0 && info.dli_fname != nullptr) {
            so = info.dli_fname;
            off = ra - reinterpret_cast<uintptr_t>(info.dli_fbase);
            if (info.dli_sname != nullptr) {
                sym = info.dli_sname;
            }
        }
        fprintf(stderr,
            "[MEMPROBE-RA] ra=0x%lx so=%s off=0x%lx cov_bytes=%lld cov_count=%lld unc_bytes=%lld unc_count=%lld sym=%s\n",
            (unsigned long)ra, so, (unsigned long)off,
            (long long)cb, (long long)cc, (long long)ub, (long long)uc, sym);
    }
    fflush(stderr);
}

std::once_flag g_initFlag;
void Init()
{
    std::atexit(Dump);
    std::thread(SamplerLoop).detach();
}

} // namespace

void RecordAlloc(void *raPtr, bool covered, int64_t size)
{
    std::call_once(g_initFlag, Init);   // fast atomic-flag check after first call

    if (covered) {
        g_covAllocBytes.fetch_add(size, std::memory_order_relaxed);
        g_covAllocCount.fetch_add(1, std::memory_order_relaxed);
    } else {
        g_uncAllocBytes.fetch_add(size, std::memory_order_relaxed);
        g_uncAllocCount.fetch_add(1, std::memory_order_relaxed);
    }

    uintptr_t ra = reinterpret_cast<uintptr_t>(raPtr);
    if (ra == 0) {
        return;
    }
    size_t h = HashRa(ra);
    for (size_t i = 0; i < CAP; i++) {
        size_t idx = (h + i) % CAP;
        uintptr_t cur = g_table[idx].ra.load(std::memory_order_acquire);
        if (cur == 0) {
            uintptr_t expected = 0;
            if (!g_table[idx].ra.compare_exchange_strong(expected, ra,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                cur = expected;   // another thread claimed this slot first
            } else {
                cur = ra;
            }
        }
        if (cur == ra) {
            if (covered) {
                g_table[idx].covBytes.fetch_add(size, std::memory_order_relaxed);
                g_table[idx].covCount.fetch_add(1, std::memory_order_relaxed);
            } else {
                g_table[idx].uncBytes.fetch_add(size, std::memory_order_relaxed);
                g_table[idx].uncCount.fetch_add(1, std::memory_order_relaxed);
            }
            return;
        }
        // slot taken by a different RA — linear probe
    }
    g_overflow.fetch_add(1, std::memory_order_relaxed);
}

void RecordFree(bool covered, int64_t size)
{
    // Free call-site RA differs from the alloc RA, so frees are tracked only in the
    // global per-side totals (for the gross-vs-net churn metric), not per-RA.
    if (covered) {
        g_covFreeBytes.fetch_add(size, std::memory_order_relaxed);
        g_covFreeCount.fetch_add(1, std::memory_order_relaxed);
    } else {
        g_uncFreeBytes.fetch_add(size, std::memory_order_relaxed);
        g_uncFreeCount.fetch_add(1, std::memory_order_relaxed);
    }
}

void RecordPoolDtorResidual(int64_t residual, bool drained)
{
    g_poolDtorCount.fetch_add(1, std::memory_order_relaxed);
    if (drained) {
        g_poolDtorDrainedCount.fetch_add(1, std::memory_order_relaxed);
        g_poolDtorResidualDrained.fetch_add(residual, std::memory_order_relaxed);
    } else {
        g_poolDtorSkippedCount.fetch_add(1, std::memory_order_relaxed);
        g_poolDtorResidualSkipped.fetch_add(residual, std::memory_order_relaxed);
    }
}

} // namespace omniruntime::mem::probe
#endif // OMNI_MEM_DEBUG
