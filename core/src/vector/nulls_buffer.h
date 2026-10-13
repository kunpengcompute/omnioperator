/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: Row buffer Header
 */

#ifndef OMNI_RUNTIME_NULLS_BUFFER_H
#define OMNI_RUNTIME_NULLS_BUFFER_H

#include <stdint.h>
#include <memory>
#include "memory/aligned_buffer.h"
#include "util/bit_util.h"

namespace omniruntime::vec {
using namespace mem;
class NullsBuffer {
public:
    constexpr static inline int32_t CalculateNbytes(int32_t bits)
    {
        // 计算bits大小的位域需要分配多少字节的空间，8是实际操作nulls过程中是按uint64来操作的，需要多分配8字节空间
        return BitUtil::Nbytes(bits) + 8;
    }

    // Pool-bound when an allocator is supplied (per-task accounting); falls back
    // to the [[deprecated]] no-pool bridge for
    // legacy/test callers that have no per-task pool context.
    static inline std::shared_ptr<AlignedBuffer<uint8_t>> MakeNullsBuffer(int32_t nbytes, Allocator *allocator)
    {
        if (allocator != nullptr) {
            return std::make_shared<AlignedBuffer<uint8_t>>(static_cast<size_t>(nbytes), allocator, true);
        }
        return std::make_shared<AlignedBuffer<uint8_t>>(static_cast<size_t>(nbytes), true);
    }

    NullsBuffer(int32_t size, NullsBuffer *inputNullsBuffer = nullptr, int32_t sliceOffset = 0,
                Allocator *allocator = nullptr) : size(size)
    {
        if (inputNullsBuffer == nullptr) {
            this->nullsBuffer = MakeNullsBuffer(CalculateNbytes(size), allocator);
            this->nulls = reinterpret_cast<uint64_t *>(this->nullsBuffer->GetBuffer());
            return;
        }
        if (sliceOffset % 8 == 0) {
            this->nullsBuffer = inputNullsBuffer->nullsBuffer;
            this->nulls =
                reinterpret_cast<uint64_t *>(reinterpret_cast<uint8_t *>(inputNullsBuffer->nulls) + sliceOffset / 8);
            return;
        }
        this->nullsBuffer = MakeNullsBuffer(CalculateNbytes(size), allocator);
        this->nulls = reinterpret_cast<uint64_t *>(this->nullsBuffer->GetBuffer());
        BitUtil::CopyBits(inputNullsBuffer->nulls, sliceOffset, this->nulls, 0, size);
    }

    static inline std::shared_ptr<AlignedBuffer<uint8_t>> MakeNullsBufferWithScope(int32_t nbytes, Allocator *allocator,
        const OmniTaskMemoryPool::AllocationAccountingScope &scope)
    {
        if (allocator != nullptr) {
            return std::make_shared<AlignedBuffer<uint8_t>>(static_cast<size_t>(nbytes), allocator, true, scope);
        }
        throw std::logic_error("NullsBuffer: accounting scope requires a bound allocator");
    }

    NullsBuffer(int32_t size, NullsBuffer *inputNullsBuffer, int32_t sliceOffset,
                Allocator *allocator,
                const OmniTaskMemoryPool::AllocationAccountingScope &scope) : size(size)
    {
        if (inputNullsBuffer == nullptr) {
            this->nullsBuffer = MakeNullsBufferWithScope(CalculateNbytes(size), allocator, scope);
            this->nulls = reinterpret_cast<uint64_t *>(this->nullsBuffer->GetBuffer());
            return;
        }
        if (sliceOffset % 8 == 0) {
            this->nullsBuffer = inputNullsBuffer->nullsBuffer;
            this->nulls =
                reinterpret_cast<uint64_t *>(reinterpret_cast<uint8_t *>(inputNullsBuffer->nulls) + sliceOffset / 8);
            return;
        }
        this->nullsBuffer = MakeNullsBufferWithScope(CalculateNbytes(size), allocator, scope);
        this->nulls = reinterpret_cast<uint64_t *>(this->nullsBuffer->GetBuffer());
        BitUtil::CopyBits(inputNullsBuffer->nulls, sliceOffset, this->nulls, 0, size);
    }

    NullsBuffer(int32_t size, std::shared_ptr<AlignedBuffer<uint8_t>> nullsBuffer) : size(size)
    {
        this->nullsBuffer = nullsBuffer;
        this->nulls = reinterpret_cast<uint64_t *>(nullsBuffer->GetBuffer());
    }

    ~NullsBuffer() = default;

    void ALWAYS_INLINE SetNull(int32_t index, bool isNull)
    {
        BitUtil::SetBit(nulls, index, isNull);
        if (isNull) {
            hasNull = true;
        }
    }

    void ALWAYS_INLINE SetNull(int32_t index) {
        BitUtil::SetBit(nulls, index);
        hasNull = true;
    }

    void ALWAYS_INLINE SetNotNull(int32_t index)
    {
        BitUtil::ClearBit(nulls, index);
    }

    void ALWAYS_INLINE SetNulls(int startIndex, NullsBuffer *nullsPtr, int length)
    {
        BitUtil::CopyBits(nullsPtr->nulls, 0, nulls, startIndex, length);
    }

    void ALWAYS_INLINE SetNulls(int startIndex, bool null, int length)
    {
        BitUtil::FillBits(nulls, startIndex, startIndex + length, null);
        if (null) {
            hasNull = true;
        }
    }

    bool ALWAYS_INLINE IsNull(int32_t index)
    {
        return BitUtil::IsBitSet(nulls, index);
    }

    void ALWAYS_INLINE SetNullFlag(bool newHasNull)
    {
        hasNull = newHasNull;
    }

    bool ALWAYS_INLINE HasNull()
    {
        if (hasNull) {
            return true;
        }
        hasNull = BitUtil::HasBitSet(nulls, 0, size);
        return hasNull;
    }

    int32_t ALWAYS_INLINE GetNullCount()
    {
        return HasNull() ? BitUtil::CountBits(nulls, 0, size) : 0;
    }

    ALWAYS_INLINE uint64_t *GetNulls()
    {
        return nulls;
    }

private:
    int32_t size;
    bool hasNull = false;
    // manage nulls memory and it's metadata
    std::shared_ptr<AlignedBuffer<uint8_t>> nullsBuffer;
    // nullsBuffer->GetBuffer() + nullOffset, caches raw data pointer
    uint64_t *nulls;
};

class NullsHelper {
public:
    NullsHelper(std::shared_ptr<NullsBuffer> nullsBufferPtr)
    {
        nullsBuffer = nullsBufferPtr;
        offset = 0;
    }

    void ALWAYS_INLINE SetNull(int32_t index, bool isNull)
    {
        // 不存在对相同的index先设置为true，然后再设置为false的情况
        if (UNLIKELY(isNull)) {
            nullsBuffer->SetNull(offset + index);
        }
    }

    uint8_t ALWAYS_INLINE operator[](int32_t index) const
    {
        return nullsBuffer->IsNull(offset + index);
    }

    void ALWAYS_INLINE operator += (int32_t addOffset)
    {
        offset += addOffset;
    }

    ALWAYS_INLINE uint8_t *GetNulls()
    {
        return reinterpret_cast<uint8_t *>(nullsBuffer->GetNulls());
    }

    std::vector<uint8_t> ALWAYS_INLINE convertToArray(int32_t length)
    {
        std::vector<uint8_t> vector;
        vector.resize(length);
        for (int32_t i = 0; i < length; i++) {
            vector[i] = nullsBuffer->IsNull(offset + i);
        }
        return vector;
    }

private:
    std::shared_ptr<NullsBuffer> nullsBuffer;

    int32_t offset;
};
}

#endif // OMNI_RUNTIME_NULLS_BUFFER_H
