/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 */

#ifndef OMNI_RUNTIME_DICTIONARY_CONTAINER_H
#define OMNI_RUNTIME_DICTIONARY_CONTAINER_H
#include <memory>
#include "large_string_container.h"
#include "memory/aligned_buffer.h"

namespace omniruntime::vec::unsafe {
class UnsafeDictionaryContainer;
}

namespace omniruntime::vec {
using namespace mem;
template <typename RAW_DATA_TYPE, template <typename> typename CONTAINER = LargeStringContainer>
class DictionaryContainer {
    using DictType =
        std::conditional_t<is_container_v<RAW_DATA_TYPE>, CONTAINER<RAW_DATA_TYPE>, AlignedBuffer<RAW_DATA_TYPE>>;

public:
    DictionaryContainer(const int32_t *values, int32_t valueSize, std::shared_ptr<DictType> dictionary,
        int32_t dictSize, int32_t dictOffset = 0)
        : valueSize(valueSize),
          dictionary(std::move(dictionary)),
          dictSize(dictSize),
          dictOffset(dictOffset),
          isSliced(false)
    {
        this->values.assign(values, values + valueSize);
    }

    DictionaryContainer(std::vector<int32_t> &values, int32_t valueSize, std::shared_ptr<DictType> dictionary,
        int32_t dictSize, int32_t dictOffset = 0)
        : values(std::move(values)),
          valueSize(valueSize),
          dictionary(std::move(dictionary)),
          dictSize(dictSize),
          dictOffset(dictOffset),
          isSliced(true)
    {}

    ~DictionaryContainer() = default;

    std::shared_ptr<DictionaryContainer<RAW_DATA_TYPE, CONTAINER>> CopyPositions(int32_t *positions, int32_t length)
    {
        std::vector<int32_t> newValues(length);
        for (int32_t i = 0; i < length; i++) {
            newValues[i] = values[positions[i]];
        }
        return std::make_shared<DictionaryContainer<RAW_DATA_TYPE, CONTAINER>>(newValues, length, dictionary, dictSize,
            dictOffset);
    }

    ALWAYS_INLINE typename PARAM_TYPE<RAW_DATA_TYPE>::type GetValue(int32_t index)
    {
        return dictionary->GetValue(values[index] + dictOffset);
    }

    ALWAYS_INLINE void SetValue(int32_t index, RAW_DATA_TYPE &value)
    {
        for (int32_t i = 0; i < dictSize; i++) {
            RAW_DATA_TYPE dicValue = dictionary->GetValue(i + dictOffset);
            if (dicValue == value) {
                values[index] = i;
                return;
            }
        }
        throw OmniException("OPERATOR_RUNTIME_ERROR", "setting to a value doesn't exist in the dictionary");
    }

    ALWAYS_INLINE int64_t GetContainerCapacity()
    {
        int64_t containerCapacity = sizeof(DictionaryContainer);
        if (!isSliced) {
            // vector class, and values total capacity
            containerCapacity += valueSize * sizeof(int32_t);
        }
        return containerCapacity;
    }

private:
    friend class unsafe::UnsafeDictionaryContainer;

    /* * stored the proxed value which can be updated using dictArr[i] = new_value */
    std::vector<int32_t> values;

    /* * the size of the dictionary array */
    int32_t valueSize;

    // dictionary can be reused, e.g. copy on write would be a perfect
    std::shared_ptr<DictType> dictionary;
    int32_t dictSize;
    int32_t dictOffset;
    bool isSliced;
};
}
#endif // OMNI_RUNTIME_DICTIONARY_CONTAINER_H
