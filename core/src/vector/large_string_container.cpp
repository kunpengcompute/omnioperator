/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2026. All rights reserved.
 */

#include "large_string_container.h"
#include <limits>

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

template<typename RAW_DATA_TYPE>
void LargeStringContainer<RAW_DATA_TYPE>::SetValues(int32_t index, const std::string_view *values,
    const uint8_t *nulls, int32_t count)
{
    if (index < 0 || count < 0 || index > size || count > size - index || (count > 0 && values == nullptr)) {
        throw omniruntime::exception::OmniException("STRING_CONTAINER_ERROR", "invalid batch string range");
    }
    if (count == 0) {
        return;
    }

    // The payload is append-only: changing an already materialized offset would also
    // require moving every following value. Gaps are supported and FillSlots will
    // propagate the current end offset over them.
    const uint32_t nextPosition = lastOffsetPosition + 1;
    if (static_cast<uint32_t>(index) < nextPosition) {
        throw omniruntime::exception::OmniException(
            "STRING_CONTAINER_ERROR", "batch string overwrite is not supported");
    }

    // Validate the whole payload before changing offsets or reallocating storage
    // referenced by a source.
    uint64_t bytes = offsets[nextPosition];
    const auto bufferAddress = reinterpret_cast<uintptr_t>(bufferSupplier->Data());
    for (int32_t i = 0; i < count; ++i) {
        if (nulls != nullptr && nulls[i]) {
            continue;
        }
        const auto &value = values[i];
        if (value.size() > static_cast<uint64_t>(std::numeric_limits<int32_t>::max()) - bytes) {
            throw omniruntime::exception::OmniException("STRING_CONTAINER_ERROR", "batch string offsets overflow");
        }
        if (!value.empty() && reinterpret_cast<uintptr_t>(value.data()) - bufferAddress < bufferSupplier->Capacity()) {
            throw omniruntime::exception::OmniException("STRING_CONTAINER_ERROR", "batch string source aliases output");
        }
        bytes += value.size();
    }

    char *destination = GetBufferWithSpace(static_cast<uint32_t>(bytes));
    FillSlots(index);
    int32_t position = offsets[index];
    for (int32_t i = 0; i < count; ++i) {
        if (nulls == nullptr || !nulls[i]) {
            const auto &value = values[i];
            if (!value.empty()) {
                memcpy(destination + position, value.data(), value.size());
                position += static_cast<int32_t>(value.size());
            }
        }
        offsets[index + i + 1] = position;
    }
    lastOffsetPosition = static_cast<uint32_t>(index + count - 1);
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
    if (needSize < 0) {
        throw omniruntime::exception::OmniException("STRING_CONTAINER_ERROR", "string container size invalid");
    }
    const int32_t oldSize = size;
    // Reserve() can grow the container ahead of the vector's logical size. A later
    // Vector::Expand() within that reservation must not shrink offsets: vector::resize
    // would retain the allocation while memory accounting incorrectly reclaimed it.
    if (needSize <= oldSize) {
        return;
    }
    const size_t offsetCount = static_cast<size_t>(needSize) + 1;
    // The ctor reports GetContainerCapacity() by the creation row count while ~Vector
    // reclaims it by the FINAL row count, so the offsets growth bytes must be reported
    // here; otherwise every grow leaves accounted permanently lower by 4*(grown rows).
    const int64_t delta = static_cast<int64_t>(sizeof(int32_t)) *
        (static_cast<int64_t>(needSize) - static_cast<int64_t>(oldSize));
    // Report before allocating so a memory-cap failure leaves the container
    // untouched. Roll the report back if std::vector growth itself fails.
    omniruntime::mem::ThreadMemoryManager::ReportMemory(delta);
    try {
        offsets.resize(offsetCount);
    } catch (...) {
        omniruntime::mem::ThreadMemoryManager::ReclaimMemory(delta);
        throw;
    }
    size = needSize;
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
