/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef OMNI_RUNTIME_OMNI_ORC_POOL_ADAPTER_H
#define OMNI_RUNTIME_OMNI_ORC_POOL_ADAPTER_H

#include <orc/MemoryPool.hh>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "memory/allocator.h"
#include "memory/omni_task_memory_pool.h"

namespace omniruntime::connector::hive {

// Apache ORC C++ library exposes orc::MemoryPool as its pluggable allocator
// interface. Its default impl (orc::getDefaultPool()) is std::malloc/free,
// which bypasses our per-task accounting → Spark TaskMemoryManager is blind
// to ORC's internal buffers (column decoders, decompression scratch,
// dictionary caches, RLE scratch).
//
// This adapter wraps OmniTaskMemoryPool so every ORC-internal allocation
// flows through Allocator::Alloc/Free → AddMemory/SubMemory → listener →
// Spark TMM. For shuffle-heavy or wide-row queries this is the biggest
// single coverage gap; native pmap analysis on q01 attributed ~150-200 MB
// of the ~860 MB "outside_spark" delta to native mmap chunks that include
// ORC reader scratch.
//
// ORC's free(char*) doesn't carry a size, so we prefix every allocation
// with an 8-byte uint64_t header carrying the TOTAL allocated bytes
// (payload + header). malloc returns payload pointer (raw + 8); free reads
// the header at payload - 8 and forwards (raw, total) to the Allocator.
//
// Alignment: Allocator returns memory aligned to >= 8 bytes (jemalloc
// default 16). Adding an 8-byte prefix keeps the returned payload pointer
// 8-byte aligned (sufficient for ORC's largest scalar types — Int128 is
// 16-byte but ORC requests its own alignment via DataBuffer; this is the
// untyped char* path).
//
// Thread safety: ORC may call malloc/free from multiple decode threads
// per Reader. taskPool_->getBoundAllocator() returns a per-task Allocator
// whose AddMemory/SubMemory atomically update OmniTaskMemoryPool::usedBytes_
// (std::atomic). No additional locking needed.
//
// Lifetime: non-owning taskPool_. Adapter must outlive the orc::Reader it
// was passed to; in our design the adapter is a SplitReader member and
// baseReader_ (the orc::Reader) is also a SplitReader member, so member
// destruction order (declaration order reversed) handles it.
class OmniOrcPoolAdapter : public ::orc::MemoryPool {
public:
    explicit OmniOrcPoolAdapter(omniruntime::mem::OmniTaskMemoryPool *taskPool)
        : taskPool_(taskPool), allocator_(taskPool) {}

    ~OmniOrcPoolAdapter() override = default;

    char *malloc(uint64_t size) override
    {
        const uint64_t total = size + kHeaderSize;
        char *raw;
        if (taskPool_ != nullptr) {
            raw = static_cast<char *>(allocator_.Alloc(static_cast<int64_t>(total)));
        } else {
            raw = static_cast<char *>(std::malloc(total));
            // Pool path throws OmniException on OOM; mirror that for the no-pool
            // fallback so a null result fails cleanly instead of deref'ing
            // nullptr in the header write below.
            if (raw == nullptr) {
                throw omniruntime::exception::OmniException(
                    "RUNTIME_ERROR:", "OmniOrcPoolAdapter::malloc std::malloc returned null");
            }
        }
        std::memcpy(raw, &total, sizeof(total));
        return raw + kHeaderSize;
    }

    void free(char *p) override
    {
        if (p == nullptr) {
            return;
        }
        char *raw = p - kHeaderSize;
        uint64_t total = 0;
        std::memcpy(&total, raw, sizeof(total));
        if (taskPool_ != nullptr) {
            allocator_.Free(raw, static_cast<int64_t>(total));
        } else {
            std::free(raw);
        }
    }

private:
    static constexpr uint64_t kHeaderSize = 8;
    omniruntime::mem::OmniTaskMemoryPool *const taskPool_;
    omniruntime::mem::Allocator allocator_;
};

} // namespace omniruntime::connector::hive

#endif // OMNI_RUNTIME_OMNI_ORC_POOL_ADAPTER_H
