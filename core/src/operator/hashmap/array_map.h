/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * @Description: array hash table
 */

#ifndef OMNI_RUNTIME_ARRAY_MAP_H
#define OMNI_RUNTIME_ARRAY_MAP_H

#include "base_hash_map.h"

namespace omniruntime {
namespace op {
/**
 * Array Map
 * - Customized for join
 * - Use key as index in table
 * - Do not support Rehash
 */
template <typename ValueType, typename Allocator,
    std::enable_if_t<std::is_move_constructible_v<ValueType> &&
    (std::is_move_assignable_v<ValueType> || std::is_copy_assignable_v<ValueType>)>* = nullptr>
// template <typename ValueType, typename Allocator>
class ArrayMap {
public:
    using Slot = ValueType;
    using ResultType = InsertResult<ValueType>;

public:
    // Pool-bound ctor: pass a pre-constructed Allocator so all allocations route
    // to the caller's pool.
    ArrayMap(int64_t defaultSize, Allocator alloc) : allocator(std::move(alloc))
    {
        allocator.Allocate(defaultSize * sizeof(Slot), &address);
        allocator.Allocate(sizeof(Slot), &nullAddress);
        // Route the presence bitmap through the pool-bound allocator we already hold
        // instead of an unaccounted raw new bool[keyRange]. Sized to the
        // integer-key RANGE, taken on the common dense-_sk join/group-by path.
        allocator.Allocate(sizeof(bool) * defaultSize, &isAssignedAddress);
        isAssigned = reinterpret_cast<bool *>(isAssignedAddress);
        memset(isAssigned, 0, sizeof(bool) * defaultSize);
        slots = reinterpret_cast<Slot *>(address);
        nullSlot = reinterpret_cast<Slot>(nullAddress);
        elementsSize = 0;
        capacity = defaultSize * sizeof(Slot);
        size = defaultSize;
    }

    ArrayMap(const ArrayMap &) = delete;

    ArrayMap(ArrayMap &&o) = delete;

    ArrayMap &operator = (const ArrayMap &) = delete;

    ArrayMap &operator = (ArrayMap &&) = delete;

    size_t GetElementsSize() const
    {
        return elementsSize;
    }

    /// Approximate resident bytes: slot buffer (`capacity` is already in bytes) + null slot + assign bitmap.
    ALWAYS_INLINE uint64_t GetAllocatedBytes() const
    {
        return capacity + sizeof(Slot) + static_cast<uint64_t>(size) * sizeof(bool);
    }

    ALWAYS_INLINE void AddElementsSize(size_t size)
    {
        elementsSize += size;
    }

    InsertResult<ValueType> InsertJoinKeysToHashmap(size_t pos)
    {
        bool inserted = false;

        if (not isAssigned[pos]) {
            inserted = true;
            isAssigned[pos] = true;
            ++elementsSize;
        }
        return InsertResult<ValueType>(slots[pos], inserted);
    }

    InsertResult<ValueType> InsertNullKeysToHashmap()
    {
        bool inserted = false;

        if (!isNullAssigned) {
            inserted = true;
            isNullAssigned = true;
        }
        return InsertResult<ValueType>(nullSlot, inserted);
    }

    ALWAYS_INLINE InsertResult<ValueType> FindValueFromHashmap(int64_t pos)
    {
        return InsertResult<ValueType>(slots[pos], isAssigned[pos]);
    }

    ALWAYS_INLINE InsertResult<ValueType> EmptyResult()
    {
        return InsertResult<ValueType>(slots[0], false);
    }

    ALWAYS_INLINE bool* GetAssigned()
    {
        return isAssigned;
    }

    ALWAYS_INLINE ValueType* GetSlots()
    {
        return slots;
    }

    ALWAYS_INLINE size_t Size() const
    {
        return size;
    }

    uint64_t AllocatedBytes() const
    {
        return capacity + sizeof(Slot) + sizeof(bool) * static_cast<uint64_t>(size);
    }

    ALWAYS_INLINE ValueType TransformPtr(int64_t ptr)
    {
        return reinterpret_cast<ValueType>(ptr);
    }

    template <class Func> void ForEachValue(Func &&func)
    {
        int remainNum = elementsSize;
        int index = 0;
        while (remainNum) {
            while (not isAssigned[index]) {
                ++index;
            }
            func(slots[index], index);
            ++index;
            --remainNum;
        }
        if (isNullAssigned) {
            func(nullSlot, -1);
        }
        return;
    }

    template<class Func>
    void OutputEachValue(Func &&func, uint32_t &index, int remainNum)
    {
        while (remainNum) {
            while (not isAssigned[index]) {
                ++index;
            }
            func(slots[index], index);
            ++index;
            --remainNum;
        }
        // dont deal with nullSlot, it is in slots[0]
    }

    ~ArrayMap()
    {
        if constexpr (not std::is_trivially_destructible<Slot>::value) {
            DeconstructAllSlot();
        }
        allocator.Release(address, capacity);
        allocator.Release(nullAddress, sizeof(Slot));
        // Symmetric pool-bound free of the presence bitmap.
        allocator.Release(isAssignedAddress, sizeof(bool) * size);
    }

private:
    void DeconstructAllSlot()
    {
        int remainNum = elementsSize;
        int index = 0;
        while (remainNum) {
            while (not isAssigned[index]) {
                ++index;
            }
            slots[index].~Slot();
            ++index;
            --remainNum;
        }
        nullSlot.~Slot();
        return;
    }

    uint8_t *address;
    uint8_t *nullAddress;
    uint8_t *isAssignedAddress = nullptr;
    Allocator allocator;
    ValueType *slots;
    ValueType nullSlot;
    bool *isAssigned = nullptr;
    bool isNullAssigned = false;
    size_t elementsSize;
    uint64_t capacity;
    int64_t size;
};

template <typename ValueType> using DefaultArrayMap = ArrayMap<ValueType *, OmniHashmapAllocator>;
}
}
#endif // OMNI_RUNTIME_ARRAY_MAP_H
