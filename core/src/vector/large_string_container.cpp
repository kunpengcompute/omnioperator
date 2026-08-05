/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2026. All rights reserved.
 */

#include "large_string_container.h"
#include "memory/thread_memory_manager.h"

namespace omniruntime::vec {
template<typename RAW_DATA_TYPE>
LargeStringContainer<RAW_DATA_TYPE>::LargeStringContainer(int valueSize, int capacityInBytes) : size(valueSize)
{
    if (valueSize < 0) {
        // throw size exceeded exception
        throw omniruntime::exception::OmniException("STRING_CONTAINER_ERROR", "string container size invalid");
    }
    bufferSupplier = std::make_unique<LargeStringBuffer>(capacityInBytes);
    offsets.resize(valueSize + 1);
    // Initialize offsets to zero to ensure correct string position calculations
    std::fill(offsets.begin(), offsets.end(), 0);
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
    int64_t containerCapacity = sizeof(LargeStringContainer) + (size + 1) * sizeof(int32_t);
    return containerCapacity;
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
    std::unique_ptr<LargeStringBuffer> oldBuffer = std::move(bufferSupplier);

    // Allocate a new buffer.
    std::unique_ptr<LargeStringBuffer> newBuffer = std::make_unique<LargeStringBuffer>(toCapacityInBytes);
    if (oldBuffer->Capacity() > 0) {
        memcpy(newBuffer->Data(), oldBuffer->Data(), oldBuffer->Capacity());
    }
    bufferSupplier = std::move(newBuffer);
    return bufferSupplier->Data();
}

template<typename RAW_DATA_TYPE>
void LargeStringContainer<RAW_DATA_TYPE>::Expand(int32_t needSize)
{
    // Vector construction reports GetContainerCapacity() once; each growth must
    // report the delta so destructor reclaim matches total reported bytes (e.g.
    // shuffle read-side merge appends multiple payloads into one VARCHAR column).
    const int64_t capacityBefore = GetContainerCapacity();
    size = needSize;
    offsets.resize(needSize + 1);
    const int64_t capacityAfter = GetContainerCapacity();
    if (capacityAfter > capacityBefore) {
        omniruntime::mem::ThreadMemoryManager::ReportMemory(capacityAfter - capacityBefore);
    }
}

template<typename RAW_DATA_TYPE>
void LargeStringContainer<RAW_DATA_TYPE>::ResetForReuse(int32_t rowCount)
{
    if (rowCount + 1 > static_cast<int32_t>(offsets.size())) {
        offsets.resize(rowCount + 1);
    }
    for (int32_t i = 0; i <= rowCount; i++) {
        offsets[i] = 0;
    }
    lastOffsetPosition = -1;
}

template class LargeStringContainer<std::string_view>;
}