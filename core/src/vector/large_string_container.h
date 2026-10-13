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
    // Original 2-arg ctor — symbol PRESERVED for ABI compatibility (other .so's, e.g.
    // libspark_columnar_plugin.so, link this mangled symbol). Delegates to the pool-bound
    // overload with pool=nullptr (legacy no-pool bridge behaviour, unchanged).
    explicit LargeStringContainer(int valueSize, int capacityInBytes = INITIAL_STRING_SIZE);

    // Pool-bound overload: when pool != nullptr the varchar/char/varbinary content
    // buffer (LargeStringBuffer) is charged to the per-task pool. New symbol,
    // additive — does not disturb the 2-arg ABI above.
    LargeStringContainer(int valueSize, int capacityInBytes, omniruntime::mem::OmniTaskMemoryPool *pool);

    // Non-default: releases the ctor's offsets pool-accounting (definition in .cpp).
    ~LargeStringContainer();

    std::string_view GetValue(int index) const;

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

    // Grow offsets with strong exception safety and charge the exact new
    // std::vector capacity before committing the replacement.
    void ResizeOffsets(int32_t valueSize);

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
