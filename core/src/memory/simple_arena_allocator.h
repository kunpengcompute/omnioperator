/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2024. All rights reserved.
 */

#ifndef SIMPLE_ARENA_ALLOCATOR_H
#define SIMPLE_ARENA_ALLOCATOR_H

#include <vector>
#include "chunk.h"

namespace omniruntime {
namespace mem {

// this allocator is not thread-safe, and mainly applies for temporary memory usage for operators,
// such as when dealing with types such as varchar/decimal and so on.
class SimpleArenaAllocator {
public:
    // Pool-bound ctor. The default parameter has been REMOVED; all callers must
    // supply a pool-bound Allocator* from their ExecutionContext. Bridge callers
    // (e.g., ExecutionContext member init) pass the legacy singleton allocator
    // until it is replaced with the pool-bound one.
    explicit SimpleArenaAllocator(int64_t minChunkSize, Allocator *allocator,
        uint32_t growthFactor = 2, int64_t linearGrowthThreshold = 512 * 1024)
        // POOL-BOUND: default allocator is process-global. Join/HashAgg task observability requires
        // passing OmniTaskMemoryPool::getBoundAllocator() (or equivalent) at construction.
        : minChunkSize(minChunkSize),
          totalBytes(0),
          usedBytes(0),
          availBytes(0),
          availBuf(nullptr),
          continuousUsedMemoryBytes(0),
          allocator(allocator),
          growthFactor(growthFactor),
          linearGrowthThreshold(linearGrowthThreshold)
    {}

    // Default ctor + SetAllocator() late-bind pattern is RETIRED.
    // The only documented caller of the deferred-init pattern was
    // `gluten/cpp-omni/src/shuffle/splitter.cpp:1409-1425` which did
    // `partition_arena_.resize(N)` then `partition_arena_[pid].SetAllocator(boundAlloc)`.
    // That has been refactored to `reserve + emplace_back(minChunkSize, allocator)`.

    ~SimpleArenaAllocator()
    {
        ReleaseChunks(false /* retainFirst */);
    }

    void SetMinChunkSize(uint64_t chunkSize)
    {
        if (chunkSize > minChunkSize) {
            uint32_t bits = 63 - __builtin_clzll(chunkSize);
            uint64_t lower = 1ULL << bits;
            if (lower == chunkSize) {
                minChunkSize = chunkSize;
            } else {
                minChunkSize = 2 * lower;
            }
        }
    }

    uint64_t GetNextSize(uint64_t sizeInBytes)
    {
        if (chunks.empty()) {
            return std::max(sizeInBytes, minChunkSize);
        }
        auto lastChunkSize = chunks.back()->GetSizeInBytes();
        if (lastChunkSize < linearGrowthThreshold) {
            return std::max(sizeInBytes, lastChunkSize * growthFactor);
        } else {
            return ((sizeInBytes + linearGrowthThreshold - 1) / linearGrowthThreshold) * linearGrowthThreshold;
        }
    }

    uint8_t *Allocate(int64_t sizeInBytes)
    {
        if (sizeInBytes == 0) {
            // a non-null pointer is returned if allocated size is 0.
            static int64_t zeroAddress[1];
            return reinterpret_cast<uint8_t *>(&zeroAddress);
        }
        if (availBytes < sizeInBytes) {
            AllocateChunk(GetNextSize(static_cast<uint64_t>(sizeInBytes)));
        }
        continuousUsedMemoryBytes = sizeInBytes;
        uint8_t *ret = availBuf;
        availBuf += sizeInBytes;
        availBytes -= sizeInBytes;
        usedBytes += sizeInBytes;
        continuousUsed = false;
        return ret;
    }

    uint8_t *GetAvailBuf()
    {
        return availBuf;
    }

    uint64_t GetAvailBytes()
    {
        return availBytes;
    }

    uint64_t GetContinuousUsedMemoryBytes()
    {
        return continuousUsedMemoryBytes;
    }

    uint64_t GetMinChunkSize()
    {
        return minChunkSize;
    }

    uint8_t *AllocateContinue(int64_t sizeInBytes, const uint8_t *&start)
    {
        continuousUsed = true;
        // null means a new begin of allocate
        if (start == nullptr) {
            uint8_t *ret = (Allocate(sizeInBytes));
            start = (ret);
            return ret;
        }

        uint8_t *ret = AllocateContinueNotNull(sizeInBytes, start);
        usedBytes += sizeInBytes;
        return ret;
    }

    void Reset()
    {
        if (chunks.empty()) {
            // if there are no chunks, nothing to do.
            return;
        }

        // Release all but the first chunk.
        if (chunks.size() > 1) {
            ReleaseChunks(true);
            chunks.erase(chunks.cbegin() + 1, chunks.cend());
        }

        auto chunk = chunks[0];
        availBuf = reinterpret_cast<uint8_t *>(chunk->GetAddress());
        availBytes = totalBytes = chunk->GetSizeInBytes();
        continuousUsedMemoryBytes = 0;
        usedBytes = 0;
    }

    /// A position in the arena. RollbackTo() releases everything allocated after it.
    struct Mark {
        size_t chunkCount;
        uint8_t *availBuf;
        uint64_t availBytes;
        uint64_t usedBytes;
        uint64_t totalBytes;
    };

    Mark GetMark() const
    {
        return Mark{chunks.size(), availBuf, availBytes, usedBytes, totalBytes};
    }

    /// Returns the arena to `mark`, freeing the chunks added since. The caller guarantees that
    /// nothing allocated after the mark is still referenced.
    void RollbackTo(const Mark &mark)
    {
        while (chunks.size() > mark.chunkCount) {
            delete chunks.back();
            chunks.pop_back();
        }
        availBuf = mark.availBuf;
        availBytes = mark.availBytes;
        usedBytes = mark.usedBytes;
        totalBytes = mark.totalBytes;
        continuousUsedMemoryBytes = 0;
        continuousUsed = false;
    }

    ALWAYS_INLINE void RollBackContinualMem()
    {
        if (continuousUsed) {
            availBuf -= continuousUsedMemoryBytes;
            availBytes += continuousUsedMemoryBytes;
            usedBytes -= continuousUsedMemoryBytes;
        }
    }

    ALWAYS_INLINE uint64_t TotalBytes()
    {
        return totalBytes;
    }

    // Reset() intentionally retains the first chunk for reuse. Only later
    // chunks produce a real pool decrease and may be advertised to the task
    // reclaimer.
    ALWAYS_INLINE uint64_t ReclaimableBytesOnReset() const
    {
        if (chunks.size() <= 1) {
            return 0;
        }
        return totalBytes - chunks.front()->GetSizeInBytes();
    }

    ALWAYS_INLINE uint64_t UsedBytes()
    {
        return usedBytes;
    }

    ALWAYS_INLINE uint64_t AvailBytes()
    {
        return availBytes;
    }

    ALWAYS_INLINE Allocator *GetAllocator()
    {
        return this->allocator;
    }

private:
    void AllocateChunk(int64_t sizeInBytes)
    {
        Chunk *chunk = Chunk::NewChunk(allocator, sizeInBytes);

        chunks.emplace_back(chunk);
        availBuf = reinterpret_cast<uint8_t *>(chunk->GetAddress());
        availBytes = sizeInBytes; // left-over bytes in the previous chunk cannot be used anymore.
        totalBytes += sizeInBytes;
    }

    void ReleaseChunks(bool retainFirst)
    {
        for (auto &chunk : chunks) {
            if (retainFirst) {
                // skip freeing first chunk.
                retainFirst = false;
                continue;
            }
            delete chunk;
        }
        continuousUsedMemoryBytes = 0;
    }

    uint8_t *AllocateContinueNotNull(int64_t sizeInBytes, const uint8_t *&start)
    {
        auto *p = const_cast<uint8_t *>(start);
        uint8_t *ret = p;
        if (sizeInBytes == 0) {
            return ret;
        }
        auto newSpace = continuousUsedMemoryBytes + static_cast<uint64_t>(sizeInBytes);
        if (availBytes < sizeInBytes) {
            AllocateChunk(GetNextSize(static_cast<uint64_t>(newSpace)));
            std::copy(start, start + continuousUsedMemoryBytes, availBuf);
            start = availBuf;
            availBuf += continuousUsedMemoryBytes;
            availBytes -= continuousUsedMemoryBytes;
        }
        ret = availBuf;
        availBuf += sizeInBytes;
        continuousUsedMemoryBytes += sizeInBytes;
        availBytes -= sizeInBytes;
        return ret;
    }

    uint64_t minChunkSize;
    uint64_t totalBytes;
    uint64_t usedBytes;
    uint64_t availBytes;
    uint8_t *availBuf;
    // Record the size of the memory used continuously.
    uint64_t continuousUsedMemoryBytes;
    uint32_t continuousUsed = false;
    uint32_t growthFactor;
    std::vector<Chunk *> chunks;
    Allocator *allocator;
    uint64_t linearGrowthThreshold;
};
} // namespace mem
} // namespace omniruntime
#endif // SIMPLE_ARENA_ALLOCATOR_H
