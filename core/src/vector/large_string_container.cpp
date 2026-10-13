/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2026. All rights reserved.
 */

#include "large_string_container.h"

#include <algorithm>

namespace omniruntime::vec {
// 2-arg ctor: ABI-preserving symbol; delegates to the pool-bound overload (no-pool path).
template<typename RAW_DATA_TYPE>
LargeStringContainer<RAW_DATA_TYPE>::LargeStringContainer(int valueSize, int capacityInBytes)
    : LargeStringContainer(valueSize, capacityInBytes, nullptr)
{}

template<typename RAW_DATA_TYPE>
LargeStringContainer<RAW_DATA_TYPE>::LargeStringContainer(int valueSize, int capacityInBytes,
    omniruntime::mem::OmniTaskMemoryPool *pool) : size(valueSize)
{
    if (valueSize < 0) {
        // throw size exceeded exception
        throw omniruntime::exception::OmniException("STRING_CONTAINER_ERROR", "string container size invalid");
    }
    // When a pool is supplied, charge the content buffer to the per-task pool via the
    // pool-bound LargeStringBuffer ctor (symmetric ctor/dtor accounting). The pool is NOT
    // stored in LargeStringContainer (would change its cross-.so layout / ABI); it lives
    // in LargeStringBuffer and is recovered on growth via bufferSupplier->GetPool().
    if (pool != nullptr) {
        bufferSupplier = std::make_unique<LargeStringBuffer>(capacityInBytes, pool);
    } else {
        bufferSupplier = std::make_unique<LargeStringBuffer>(capacityInBytes);
    }
    ResizeOffsets(valueSize);
    // Initialize offsets to zero to ensure correct string position calculations
    std::fill(offsets.begin(), offsets.end(), 0);
}

template<typename RAW_DATA_TYPE>
LargeStringContainer<RAW_DATA_TYPE>::~LargeStringContainer()
{
    // Symmetric release of the offsets accounting charged in the ctor. The pool is not a
    // member (ABI); recover it from bufferSupplier, which is still alive here (member
    // dtors run after this body). Same capacity as the ctor charge -> balanced.
    if (bufferSupplier != nullptr) {
        auto *pool = bufferSupplier->GetPool();
        if (pool != nullptr) {
            pool->SubMemory(static_cast<int64_t>(offsets.capacity()) * static_cast<int64_t>(sizeof(int32_t)));
        }
    }
}

template<typename RAW_DATA_TYPE>
std::string_view LargeStringContainer<RAW_DATA_TYPE>::GetValue(int index) const
{
    char *valuePtr = bufferSupplier->Data() + offsets[index];
    size_t valueLen = offsets[index + 1] - offsets[index];
    return std::string_view(valuePtr, valueLen);
}

template<typename RAW_DATA_TYPE>
void LargeStringContainer<RAW_DATA_TYPE>::SetValue(int index, const std::string_view &value)
{
    FillSlots(index); // rescue offset for null values

    size_t valueSize = value.size();
    int32_t needCapacityInBytes = offsets[index] + valueSize; // start offset, and then value size
    char *charBuffer = GetBufferWithSpace(needCapacityInBytes);

    // Avoid memcpy when valueSize is 0: empty string_view.data() may be nullptr, causing SIGSEGV on aarch64
    if (valueSize > 0) {
        memcpy(charBuffer + offsets[index], value.data(), valueSize);
    }
    offsets[index + 1] = needCapacityInBytes;
    lastOffsetPosition = index;
}

/* *
 * set the element at the index position to null
 * @param index
 *         */
template<typename RAW_DATA_TYPE>
void LargeStringContainer<RAW_DATA_TYPE>::SetNull(int32_t index)
{
    FillSlots(index); // rescue offset for null values
    offsets[index + 1] = offsets[index];
    lastOffsetPosition = index;
}

template<typename RAW_DATA_TYPE>
void LargeStringContainer<RAW_DATA_TYPE>::FillSlots(int index)
{
    for (int i = lastOffsetPosition + 1; i < index; ++i) {
        offsets[i + 1] = offsets[i];
    }
    lastOffsetPosition = index - 1;
}

template<typename RAW_DATA_TYPE>
size_t LargeStringContainer<RAW_DATA_TYPE>::GetCapacityInBytes()
{
    return bufferSupplier->Capacity();
}

template<typename RAW_DATA_TYPE>
int64_t LargeStringContainer<RAW_DATA_TYPE>::GetContainerCapacity()
{
    // offsets owns and reports its allocation independently so vector wrapper
    // accounting must cover only the container object itself.
    return sizeof(LargeStringContainer);
}

template<typename RAW_DATA_TYPE>
void LargeStringContainer<RAW_DATA_TYPE>::ResizeOffsets(int32_t valueSize)
{
    if (valueSize < 0) {
        throw omniruntime::exception::OmniException(
            "STRING_CONTAINER_ERROR", "string container size invalid");
    }
    const size_t required = static_cast<size_t>(valueSize) + 1;
    if (required <= offsets.capacity()) {
        offsets.resize(required);
        return;
    }

    auto *pool = bufferSupplier == nullptr ? nullptr : bufferSupplier->GetPool();
    if (pool == nullptr) {
        offsets.resize(required);
        return;
    }

    // Allocate into a temporary first. If either allocation or pool admission
    // fails, the live container and its existing accounting stay unchanged.
    std::vector<int32_t> grown;
    const size_t nextCapacity = offsets.capacity() <= offsets.max_size() / 2
        ? std::max(required, offsets.capacity() * 2) : offsets.max_size();
    grown.reserve(nextCapacity);
    grown.resize(required, 0);
    std::copy(offsets.begin(), offsets.end(), grown.begin());
    const int64_t oldBytes = static_cast<int64_t>(offsets.capacity()) * sizeof(int32_t);
    const int64_t newBytes = static_cast<int64_t>(grown.capacity()) * sizeof(int32_t);
    pool->AddMemory(newBytes - oldBytes);
    offsets.swap(grown);
}

template<typename RAW_DATA_TYPE>
char* LargeStringContainer<RAW_DATA_TYPE>::GetBufferWithSpace(uint32_t needCapacityInBytes)
{
    // Check if the last buffer has enough space.
    if (bufferSupplier->Capacity() > 0 && needCapacityInBytes <= bufferSupplier->Capacity()) {
        return bufferSupplier->Data();
    }

    uint64_t initCapacityInBytes = bufferSupplier->Capacity() ? bufferSupplier->Capacity() : INITIAL_STRING_SIZE;
    uint64_t toCapacityInBytes = initCapacityInBytes;
    while (toCapacityInBytes < needCapacityInBytes) {
        toCapacityInBytes = toCapacityInBytes * 2;
    }

    // expand buffer.
    auto newAddress = ExpandBufferToCapacity(toCapacityInBytes);
    return newAddress;
};

template<typename RAW_DATA_TYPE>
char* LargeStringContainer<RAW_DATA_TYPE>::ExpandBufferToCapacity(size_t toCapacityInBytes)
{
    // Keep the original buffer and its pool binding alive until admission and
    // allocation succeed. A denied growth must preserve both data and credits.
    omniruntime::mem::OmniTaskMemoryPool *pool = bufferSupplier->GetPool();
    std::unique_ptr<LargeStringBuffer> newBuffer = (pool != nullptr)
        ? std::make_unique<LargeStringBuffer>(toCapacityInBytes, pool)
        : std::make_unique<LargeStringBuffer>(toCapacityInBytes);
    if (bufferSupplier->Capacity() > 0) {
        memcpy(newBuffer->Data(), bufferSupplier->Data(), bufferSupplier->Capacity());
    }
    bufferSupplier = std::move(newBuffer);
    return bufferSupplier->Data();
}

template<typename RAW_DATA_TYPE>
void LargeStringContainer<RAW_DATA_TYPE>::Expand(int32_t needSize)
{
    ResizeOffsets(needSize);
    size = needSize;
}

template<typename RAW_DATA_TYPE>
void LargeStringContainer<RAW_DATA_TYPE>::ResetForReuse(int32_t rowCount)
{
    if (rowCount + 1 > static_cast<int32_t>(offsets.size())) {
        ResizeOffsets(rowCount);
    }
    for (int32_t i = 0; i <= rowCount; i++) {
        offsets[i] = 0;
    }
    lastOffsetPosition = -1;
}

template class LargeStringContainer<std::string_view>;
}
