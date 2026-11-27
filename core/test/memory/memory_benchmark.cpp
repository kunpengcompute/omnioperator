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

#include <cstdlib>
#include <ctime>
#include "benchmark/benchmark.h"
#include "memory/thread_memory_manager.h"

namespace omniruntime::mem::test {
static int g_threadNum = 10000;
static int64_t sizes[1'000'000];
void Random()
{
    // randomly generate size array, which is in range of [0, 1024] and 8 times.
    srand(time(0));
    for (int i = 0; i < 1'000'000; i++) {
        sizes[i] = (rand() % (128 + 1)) * 8;
    }
}

static void bm_v2_memory_reportmemoryusage_fixedsize(benchmark::State &state)
{
    auto globalMemoryManager = mem::MemoryManager::GetGlobalMemoryManager();
    int64_t limit = 1'099'511'627'776; // 1T
    globalMemoryManager->SetMemoryLimit(limit);
    int64_t size = state.range(0);
    for (auto _ : state) {
        auto threadMemoryManager = mem::ThreadMemoryManager::GetThreadMemoryManager();

        for (int i = 0; i < 1'000'000; i++) {
            threadMemoryManager->ReportMemoryUsage(size);
        }
    }
}

static void bm_v2_memory_reportmemoryusage_random(benchmark::State &state)
{
    Random();
    auto globalMemoryManager = mem::MemoryManager::GetGlobalMemoryManager();
    int64_t limit = 1'099'511'627'776; // 1T
    globalMemoryManager->SetMemoryLimit(limit);

    for (auto _ : state) {
        auto threadMemoryManager = mem::ThreadMemoryManager::GetThreadMemoryManager();

        for (int i = 0; i < 1'000'000; i++) {
            threadMemoryManager->ReportMemoryUsage(sizes[i]);
        }
    }
}

static void bm_v2_memory_reportmemoryusage_multithread_fixedsize(benchmark::State &state)
{
    auto globalMemoryManager = mem::MemoryManager::GetGlobalMemoryManager();
    int64_t limit = 1'099'511'627'776; // 1T
    globalMemoryManager->SetMemoryLimit(limit);
    int64_t size = state.range(0);
    for (auto _ : state) {
        auto threadMemoryManager = mem::ThreadMemoryManager::GetThreadMemoryManager();

        for (int i = 0; i < 1'0000; i++) {
            threadMemoryManager->ReportMemoryUsage(size);
        }
    }
}

static void bm_v2_memory_reclaimmemoryusage_fixedsize(benchmark::State &state)
{
    auto globalMemoryManager = mem::MemoryManager::GetGlobalMemoryManager();
    int64_t limit = 1'099'511'627'776; // 1T
    globalMemoryManager->SetMemoryLimit(limit);

    int64_t size = state.range(0);
    for (auto _ : state) {
        auto threadMemoryManager = mem::ThreadMemoryManager::GetThreadMemoryManager();

        for (int i = 0; i < 1'000'000; i++) {
            threadMemoryManager->ReclaimMemoryUsage(size);
        }
    }
}

static void bm_v2_memory_reclaimmemoryusage_random(benchmark::State &state)
{
    auto globalMemoryManager = mem::MemoryManager::GetGlobalMemoryManager();
    int64_t limit = 1'099'511'627'776; // 1T
    globalMemoryManager->SetMemoryLimit(limit);

    for (auto _ : state) {
        auto threadMemoryManager = mem::ThreadMemoryManager::GetThreadMemoryManager();

        for (int i = 0; i < 1'000'000; i++) {
            threadMemoryManager->ReclaimMemoryUsage(sizes[i]);
        }
    }
}

static void bm_v2_memory_reclaimmemoryusage_multithread_fixedsize(benchmark::State &state)
{
    auto globalMemoryManager = mem::MemoryManager::GetGlobalMemoryManager();
    int64_t limit = 1'099'511'627'776; // 1T
    globalMemoryManager->SetMemoryLimit(limit);

    int64_t size = state.range(0);
    for (auto _ : state) {
        auto threadMemoryManager = mem::ThreadMemoryManager::GetThreadMemoryManager();

        for (int i = 0; i < 1'000'0; i++) {
            threadMemoryManager->ReclaimMemoryUsage(size);
        }
    }
}

// reportmemoryusage vs tryallocate interface under single thread
BENCHMARK(bm_v2_memory_reportmemoryusage_fixedsize)->Arg(8)->Arg(64)->Arg(1024);
BENCHMARK(bm_v2_memory_reportmemoryusage_random);

// reportmemoryusage vs tryallocate interface under multiple threads by benchmark
BENCHMARK(bm_v2_memory_reportmemoryusage_multithread_fixedsize)->Arg(8)->Arg(64)->Arg(1024)->Threads(g_threadNum);

// reclaimmemoryusage vs releasebytes interface under single thread
BENCHMARK(bm_v2_memory_reclaimmemoryusage_fixedsize)->Arg(8)->Arg(64)->Arg(1024);
BENCHMARK(bm_v2_memory_reclaimmemoryusage_random);

// reclaimmemoryusage vs releasebytes interface under multiple threads by benchmark
BENCHMARK(bm_v2_memory_reclaimmemoryusage_multithread_fixedsize)->Arg(8)->Arg(64)->Arg(1024)->Threads(g_threadNum);
} // namespace omniruntime::mem::test
