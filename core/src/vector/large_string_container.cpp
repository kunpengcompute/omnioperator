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

#include "large_string_container.h"

namespace omniruntime::vec {
template <typename RAW_DATA_TYPE>
LargeStringContainer<RAW_DATA_TYPE>::LargeStringContainer(int valueSize, int capacityInBytes) : size(valueSize)
{
    if (valueSize < 0) {
        // throw size exceeded exception
        throw omniruntime::exception::OmniException("STRING_CONTAINER_ERROR", "string container size invalid");
    }
    bufferSupplier = std::make_unique<LargeStringBuffer>(capacityInBytes);
    offsets.resize(valueSize + 1);
}

template <typename RAW_DATA_TYPE> std::string_view LargeStringContainer<RAW_DATA_TYPE>::GetValue(int index)
{
    char *valuePtr = bufferSupplier->Data() + offsets[index];
    size_t valueLen = offsets[index + 1] - offsets[index];
    return std::string_view(valuePtr, valueLen);
}

template <typename RAW_DATA_TYPE> void LargeStringContainer<RAW_DATA_TYPE>::SetValue(int index, std::string_view &value)
{
    FillSlots(index); // rescue offset for null values

    size_t valueSize = value.size();
    int32_t needCapacityInBytes = offsets[index] + valueSize; // start offset, and then value size
    char *charBuffer = GetBufferWithSpace(needCapacityInBytes);

    // src len can be 0, but dest len can not be 0 when empty strings, otherwise the memcpy_s return errno 34
    errno_t result = memcpy_s(charBuffer + offsets[index], valueSize + 1, value.data(), valueSize);
    if (UNLIKELY(result != EOK)) {
        std::string omniExceptionInfo = "In function SetValue, memcpy failed, ret is：" + std::to_string(result) +
                                        ", reason is: " + std::string(strerror(errno));
        throw omniruntime::exception::OmniException("OPERATOR_RUNTIME_ERROR", omniExceptionInfo);
    }

    offsets[index + 1] = needCapacityInBytes;
    lastOffsetPosition = index;
}

/* *
 * set the element at the index position to null
 * @param index
 *         */
template <typename RAW_DATA_TYPE> void LargeStringContainer<RAW_DATA_TYPE>::SetNull(int32_t index)
{
    FillSlots(index); // rescue offset for null values
    offsets[index + 1] = offsets[index];
    lastOffsetPosition = index;
}

template <typename RAW_DATA_TYPE> void LargeStringContainer<RAW_DATA_TYPE>::FillSlots(int index)
{
    for (int i = lastOffsetPosition + 1; i < index; ++i) {
        offsets[i + 1] = offsets[i];
    }
    lastOffsetPosition = index - 1;
}

template <typename RAW_DATA_TYPE> size_t LargeStringContainer<RAW_DATA_TYPE>::GetCapacityInBytes()
{
    return bufferSupplier->Capacity();
}

template <typename RAW_DATA_TYPE> int64_t LargeStringContainer<RAW_DATA_TYPE>::GetContainerCapacity()
{
    int64_t containerCapacity = sizeof(LargeStringContainer) + (size + 1) * sizeof(int32_t);
    return containerCapacity;
}

template <typename RAW_DATA_TYPE>
char *LargeStringContainer<RAW_DATA_TYPE>::GetBufferWithSpace(uint32_t needCapacityInBytes)
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

template <typename RAW_DATA_TYPE>
char *LargeStringContainer<RAW_DATA_TYPE>::ExpandBufferToCapacity(size_t toCapacityInBytes)
{
    std::unique_ptr<LargeStringBuffer> oldBuffer = std::move(bufferSupplier);

    // Allocate a new buffer.
    std::unique_ptr<LargeStringBuffer> newBuffer = std::make_unique<LargeStringBuffer>(toCapacityInBytes);
    if (oldBuffer->Capacity() > 0) {
        errno_t result = memcpy_s(newBuffer->Data(), newBuffer->Capacity(), oldBuffer->Data(), oldBuffer->Capacity());
        if (UNLIKELY(result != EOK)) {
            std::string omniExceptionInfo = "In function ExpandBufferToCapacity, memcpy failed, ret is：" +
                                            std::to_string(result) + ", reason is: " + std::string(strerror(errno));
            throw omniruntime::exception::OmniException("OPERATOR_RUNTIME_ERROR", omniExceptionInfo);
        }
    }
    bufferSupplier = std::move(newBuffer);
    return bufferSupplier->Data();
}

template <typename RAW_DATA_TYPE> void LargeStringContainer<RAW_DATA_TYPE>::Expand(int32_t needSize)
{
    size = needSize;
    offsets.resize(needSize + 1);
}

template class LargeStringContainer<std::string_view>;
} // namespace omniruntime::vec
