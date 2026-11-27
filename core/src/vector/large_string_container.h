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

    std::string_view GetValue(int index) const;

    void SetValue(int index, const std::string_view &value);

    // Append and deep-copy a sequential output range after reserving its payload once.
    // Already materialized rows cannot be overwritten. Source views must not refer to
    // this container; nulls may be nullptr for an all-non-null range.
    void SetValues(int32_t index, const std::string_view *values, const uint8_t *nulls, int32_t count);

    // Same append semantics as SetValues, for callers that pre-validate during their own
    // gather pass. totalBytes must be the exact sum of the non-null value lengths, and the
    // sources must have been proven not to alias this container's buffer. The per-value
    // validation scan is replaced by one int32 offsets capacity check.
    void SetValues(int32_t index, const std::string_view *values, const uint8_t *nulls, int32_t count,
        uint64_t totalBytes);

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
