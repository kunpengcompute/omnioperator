/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: MapVector  implementation
 */

#ifndef OMNI_RUNTIME_MAP_VECTOR_H
#define OMNI_RUNTIME_MAP_VECTOR_H

#include <vector>
#include <memory>
#include "vector.h"
#include "memory/omni_task_memory_pool.h"

namespace omniruntime::vec {

class MapVector : public BaseVector {
public:
    MapVector(int64_t size, std::shared_ptr<BaseVector> keyVector, std::shared_ptr<BaseVector> valueVector)
        : BaseVector(size, OMNI_ENCODING_MAP, OMNI_MAP),
          keys(std::move(keyVector)),
          values(std::move(valueVector)), capacity(static_cast<int32_t>(size))
    {
        offsetsBuffer = std::make_shared<AlignedBuffer<int64_t>>(size + 1, true);
        offsets = offsetsBuffer->GetBuffer();
    }

    MapVector(int64_t size)
        : BaseVector(size, OMNI_ENCODING_MAP, OMNI_MAP), capacity(static_cast<int32_t>(size))
    {
        offsetsBuffer = std::make_shared<AlignedBuffer<int64_t>>(size + 1, true);
        offsets = offsetsBuffer->GetBuffer();
    }

    // Pool-bound primary ctor. Mirrors ArrayVector(size,pool)
    // — routes offsetsBuffer (#3) AND the BaseVector validity NullsBuffer (#15)
    // through the pool-bound allocator; nullptr-pool falls through to the no-pool
    // [[deprecated]] AlignedBuffer ctor.
    MapVector(int64_t size, omniruntime::mem::OmniTaskMemoryPool *pool)
        : BaseVector(size, OMNI_ENCODING_MAP, OMNI_MAP, /*nullsBufferPtr*/nullptr, /*sliceOffset*/0,
              (pool != nullptr ? pool->getBoundAllocator() : nullptr)),
          capacity(static_cast<int32_t>(size)), pool_(pool)
    {
        if (pool != nullptr) {
            offsetsBuffer = std::make_shared<AlignedBuffer<int64_t>>(size + 1,
                pool->getBoundAllocator(), true);
        } else {
            _Pragma("GCC diagnostic push")
            _Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")
            offsetsBuffer = std::make_shared<AlignedBuffer<int64_t>>(size + 1, true);
            _Pragma("GCC diagnostic pop")
        }
        offsets = offsetsBuffer->GetBuffer();
    }

    // Pool-bound children-taking ctor. Same pool routing as the (size,pool) ctor
    // above (offsetsBuffer #3 + validity NullsBuffer #15) but also adopts the
    // pre-built key/value children. Used by VectorHelper::CreateComplexVector and
    // ArrayConstructorFunction so the wrapper's top-level buffers are pool-accounted.
    MapVector(int64_t size, std::shared_ptr<BaseVector> keyVector, std::shared_ptr<BaseVector> valueVector,
        omniruntime::mem::OmniTaskMemoryPool *pool)
        : BaseVector(size, OMNI_ENCODING_MAP, OMNI_MAP, /*nullsBufferPtr*/nullptr, /*sliceOffset*/0,
              (pool != nullptr ? pool->getBoundAllocator() : nullptr)),
          keys(std::move(keyVector)),
          values(std::move(valueVector)), capacity(static_cast<int32_t>(size)), pool_(pool)
    {
        if (pool != nullptr) {
            offsetsBuffer = std::make_shared<AlignedBuffer<int64_t>>(size + 1,
                pool->getBoundAllocator(), true);
        } else {
            _Pragma("GCC diagnostic push")
            _Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")
            offsetsBuffer = std::make_shared<AlignedBuffer<int64_t>>(size + 1, true);
            _Pragma("GCC diagnostic pop")
        }
        offsets = offsetsBuffer->GetBuffer();
    }

    const std::shared_ptr<AlignedBuffer<int64_t>>& GetOffsetsBuffer() const
    {
        return offsetsBuffer;
    }

    omniruntime::mem::OmniTaskMemoryPool *GetPool() const override
    {
        return pool_;
    }

    int64_t* GetOffsets()
    {
        return offsets;
    }

    int64_t GetOffset(int64_t index)
    {
        return offsets[index];
    }

    using BaseVector::GetSize;

    int64_t GetSize(int64_t index)
    {
        return offsets[index + 1] - offsets[index];
    }

    const std::shared_ptr<BaseVector> GetKeyVector() const
    {
        return keys;
    }

    const std::shared_ptr<BaseVector> GetValueVector() const
    {
        return values;
    }

    void SetValue(int index, MapVector* value);

    std::pair<BaseVector*, BaseVector*> GetValue(int index);

    void Append(MapVector* other, int32_t offset);

    std::vector<DataTypeId> ALWAYS_INLINE GetTypeIds() const override
    {
        return {keys->GetTypeId(), values->GetTypeId()};
    }

    void SetKeyVector(std::shared_ptr<BaseVector> keyVector)
    {
        keys = std::move(keyVector);
    }
    
    void SetValueVector(std::shared_ptr<BaseVector> valueVector)
    {
        values = std::move(valueVector);
    }

    void SetOffset(int32_t index, int32_t offset)
    {
        offsets[index] = offset;
    }

    void SetSize(int32_t index, int32_t size)
    {
        offsets[index + 1] = offsets[index] + size;
    }

    void ALWAYS_INLINE SetNull(int64_t index)
    {
        BaseVector::SetNull(index);
        SetSize(index, 0);
    }

    void AddKeys(BaseVector* addedKeys)
    {
        keys = std::shared_ptr<BaseVector>(addedKeys);
    }

    void AddValues(BaseVector* addedValues)
    {
        values = std::shared_ptr<BaseVector>(addedValues);
    }

    MapVector *Slice(int positionOffset, int length, bool isCopy = false) override;

    /* *
     * Copies the values of the vector at the indicated positions
     * @param positions
     * @param offset
     * @param length
     */
    MapVector* CopyPositions(const int *positions, int positionOffset, int length) ;

    static void UpdateKeyPositions(std::vector<int> &keyPositions, int index, int size)
    {
        for (int i = 0; i < size; i++) {
            keyPositions.push_back(index++);
        }
    }

    void Expand(int32_t needCapacity) override;

    void Append(BaseVector *other, int positionOffset, int length);

protected:
    int64_t* offsets;
    std::shared_ptr<AlignedBuffer<int64_t>> offsetsBuffer;
    std::shared_ptr<BaseVector> keys;
    std::shared_ptr<BaseVector> values;
    int32_t capacity;
    // Per-task pool captured from the pool-bound ctor so Expand() re-allocates the
    // offsets + validity buffers through it (nullptr for no-pool/legacy ctors).
    omniruntime::mem::OmniTaskMemoryPool *pool_ = nullptr;
};
}

#endif // OMNI_RUNTIME_MAP_VECTOR_H
