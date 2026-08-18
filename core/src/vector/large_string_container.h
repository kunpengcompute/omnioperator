/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2022. All rights reserved.
 */
#ifndef OMNI_RUNTIME_LARGE_STRING_CONTAINER_H
#define OMNI_RUNTIME_LARGE_STRING_CONTAINER_H
#include "type_utils.h"
#include "string_utils.h"
#include "util/omni_exception.h"
#include "util/compiler_util.h"

namespace omniruntime::vec::unsafe {
class UnsafeStringContainer;
}

namespace omniruntime::vec {
template <typename RAW_DATA_TYPE> class LargeStringContainer {
public:
    explicit LargeStringContainer(int valueSize, int capacityInBytes = INITIAL_STRING_SIZE);

    ~LargeStringContainer() = default;

    ALWAYS_INLINE std::string_view GetValue(int index) const
    {
        char *valuePtr = bufferSupplier->Data() + offsets[index];
        size_t valueLen = offsets[index + 1] - offsets[index];
        return std::string_view(valuePtr, valueLen);
    }

    void SetValue(int index, const std::string_view &value);

    /* *
     * set the element at the index position to null
     * @param index
     *         */
    void SetNull(int32_t index);

    void FillSlots(int index);

    size_t GetCapacityInBytes();

    int64_t GetContainerCapacity();

    void Expand(int needSize);

    /** Reset internal offsets so that the next SetValue(0, ...) starts from offset 0. Used when reusing vector for spill. */
    void ResetForReuse(int32_t rowCount);

private:
    char *GetBufferWithSpace(uint32_t needCapacityInBytes);

    char *ExpandBufferToCapacity(size_t toCapacityInBytes);

    friend class unsafe::UnsafeStringContainer;

    /* * the size of the string rows */
    int size;

    /* * stored the string rows */
    std::vector<int32_t> offsets;
    uint32_t lastOffsetPosition = -1;
    std::unique_ptr<LargeStringBuffer> bufferSupplier;
};
}
#endif // OMNI_RUNTIME_LARGE_STRING_CONTAINER_H
