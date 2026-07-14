/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2024. All rights reserved.
 */
// Copyright 2021-present StarRocks, Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// This file is based on code available under the Apache license here:
//   https://github.com/greg7mdp/parallel-hashmap/blob/master/parallel_hashmap/phmap.h
// ---------------------------------------------------------------------------
// Copyright (c) 2019, Gregory Popovitch - greg7mdp@gmail.com
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Includes work from abseil-cpp (https://github.com/abseil/abseil-cpp)
// with modifications.
//
// Copyright 2018 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// ---------------------------------------------------------------------------

#ifndef OMNI_RUNTIME_BASE_HASH_MAP_H
#define OMNI_RUNTIME_BASE_HASH_MAP_H

#include <iostream>
#include <vector>
#include <cmath>
#include <cstring>
#include <sstream>
#include <unordered_map>
#include <functional>
#include <atomic>
#include <limits>
#include <optional>
#include <jemalloc/jemalloc.h>
#include <folly/CppAttributes.h>
#ifdef __aarch64__
#include <arm_neon.h>
#endif
#include "simd/func/match.h"

#include "operator/hash_util.h"
#include "group_hasher.h"
#include "memory/memory_pool.h"
#include "memory/allocator.h"
#include "type/string_ref.h"

/**
 * BaseHashMap contains 7 requirements to notice:
 * 1 hashmap use slot as basic unit, every slot contains (key, value, hash value of key)
 * 2 the key and value of hashmap must support movable,
 * the value must have default constructor and must support copy assign function or move assign function
 * 3 the key must overload operator== function or it is a POD structure
 * 4 the hash function will use std::hash by default if user didn't set hash function in group_hasher.h
 * 5 the hashmap will resize when exceed threshold in GrowStrategy
 * 6 the Allocator is used to allocate memory for cells, user can define custom allocator as OmniHashmapAllocator does
 * 7 hashmap will call key's and value's destructor if necessary
 *
 * Basic Usage:
 * DefaultHashMap<uint32, uint32> hashmap;
 * auto ret = hash.emplace(key); //or auto ret = hash.emplace(std::move(key));  key is std::unique_ptr<int>
 * if(not ret.IsInserted()){
 * ret.SetValue(value);
 * };
 * hashmap.GetElementsSize();
 * hashmap.ForEachKV(CustomFunction);
 */

using namespace simd;

namespace omniruntime {
namespace op {
static const int32_t EIGHT = 8;
static const double MAX_LOAD_FACTOR = 0.9;

/**
 * Basic Unit of Hashmap, Hashmap contains a continuous memory of HashSlots
 * Slot will cache hash value by default to compare value easily.
 * @tparam KeyType the key type of hashmap
 * @tparam ValueType the value type of hashmap
 */
template <typename KeyType, typename ValueType> class HashSlot {
public:
    HashSlot() = default;

    ~HashSlot() = default;

    explicit HashSlot(const KeyType &keyParam)
    {
        kv.first = keyParam;
    }

    explicit HashSlot(KeyType &&keyParam) : kv(std::move(keyParam), ValueType {}) {}

    HashSlot(HashSlot &&o) noexcept : kv(std::move(o.kv)), hashVal(o.hashVal)
    {
        o.hashVal = 0;
    }

    HashSlot(const KeyType &key, const ValueType &value) : kv(key, value) {}

    HashSlot &operator = (HashSlot &&o) noexcept
    {
        kv = std::move(o.kv);
        SetHashVal(o.GetHashVal());
        return *this;
    }

    bool ALWAYS_INLINE  IsSameKey(const size_t &otherHashVal, const KeyType &key1)
    {
        return kv.first == key1;
    }

    size_t GetHashVal() const
    {
        return hashVal;
    }

    void SetValue(const ValueType &value)
    {
        kv.second = value;
    }

    const KeyType &GetKey() const
    {
        return kv.first;
    }

    ValueType &GetValue()
    {
        return kv.second;
    }

    const ValueType &GetValue() const
    {
        return kv.second;
    }

    void SetHashVal(const size_t hashValueIn)
    {
        hashVal = hashValueIn;
    }

    HashSlot &operator = (const HashSlot &o)
    {
        kv = o.kv;
        SetHashVal(o.GetHashVal());
        return *this;
    }

    HashSlot(const HashSlot &o)
    {
        kv = o.kv;
        SetHashVal(o.GetHashVal());
    }

private:
    std::pair<KeyType, ValueType> kv;
    size_t hashVal;
};

/**
 * A user-defined Allocator must implement 2 functions
 * 1 Allocate(uint64_t size, uint8_t **buffer): allocate @size bytes, and set ret pointer to *buffer
 * 2 Release(uint8_t *buffer): release the memory
 */
class OmniHashmapAllocator {
public:
    OmniHashmapAllocator() : pool(mem::Allocator::GetAllocator()) {}

    ~OmniHashmapAllocator() = default;

    int Allocate(uint64_t size, uint8_t **buffer)
    {
        auto ret = pool->Alloc(static_cast<int64_t>(size), false);
        if (ret == nullptr) {
            throw exception::OmniException("allocate in OmniHashmapAllocator", "allocate memory fail");
        }
        *buffer = static_cast<uint8_t *>(ret);
        return size;
    }

    void Release(uint8_t *buffer, uint64_t size)
    {
        pool->Free((void *)buffer, size);
    }

private:
    mem::Allocator *pool = nullptr;
};

template <typename ValueType> class InsertResult {
public:
    explicit InsertResult(ValueType &emplaceValue, bool ins = false) : emplaceValue(emplaceValue), inserted(ins) {}

    ~InsertResult() = default;

    bool IsInsert() const
    {
        return inserted;
    }

    // for copyable object use value itself, for only movable object use std::move(value)
    void SetValue(const ValueType &value)
    {
        emplaceValue = value;
    }

    // this interface will be used for only movable object
    void SetValue(ValueType &&value)
    {
        emplaceValue = std::move(value);
    }

    const ValueType &GetValue() const
    {
        return emplaceValue;
    }

    ValueType &GetValue()
    {
        return emplaceValue;
    }

private:
    ValueType &emplaceValue;
    bool inserted = false;
};

class Grower {
public:
    static constexpr short enlargeThreshold = 23;
    static constexpr short expandFactoryOne = 1;
    static constexpr short expandFactoryTwo = 2;
    explicit Grower(uint8_t degree) : degree(degree)
    {
        CalculateThreshold();
    }

    uint64_t GrowSize()
    {
        degree += (degree >= enlargeThreshold ? expandFactoryOne : expandFactoryTwo);
        CalculateThreshold();
        return 1ULL << degree;
    }

    uint64_t GetCurrentSize()
    {
        return 1ULL << degree;
    }

    uint64_t GetThreshHold()
    {
        return threshold;
    }

    ~Grower() = default;

private:
    void CalculateThreshold()
    {
        threshold = static_cast<uint64_t>(std::ceil(static_cast<double>(1ULL << degree) * MAX_LOAD_FACTOR));
    }

    uint64_t threshold = 0;
    uint8_t degree;
};

struct OutputState {
    uint32_t outputHashmapPos = 0;
    uint32_t hasBeenOutputNum = 0;
    int32_t rowOffset = 0;
    char* FOLLY_NULLABLE rowBegin{nullptr};


    void UpdateState(OutputState &o)
    {
        outputHashmapPos = o.outputHashmapPos;
        hasBeenOutputNum += o.hasBeenOutputNum;
    }

    explicit OutputState(uint32_t outputHashmapPos = 0, uint32_t hasBeenOutputNum = 0)
        : outputHashmapPos(outputHashmapPos), hasBeenOutputNum(hasBeenOutputNum)
    {}
};

ALWAYS_INLINE static uint32_t CountTrailingZerosNonZero64(uint64_t n)
{
    return __builtin_ctzll(n);
}

ALWAYS_INLINE static uint32_t CountTrailingZerosNonZero32(uint32_t n)
{
    return __builtin_ctz(n);
}

template <typename T> ALWAYS_INLINE uint32_t TrailingZeros(T x)
{
    if constexpr (sizeof(T) == 8) {
        return CountTrailingZerosNonZero64(static_cast<uint64_t>(x));
    } else {
        return CountTrailingZerosNonZero32(static_cast<uint32_t>(x));
    }
}

/**
 * An abstraction of a bitmask. It provides an easy way to iterate through the indexes of the set bits of a bitmask.
 * When Shift=0 (platforms with SSE), this is a true bitmask.
 * On non-SSE platforms, the arithmetic used to emulate the SSE behavior works in bytes (Shift=3) and leaves each
 * bytes as either 0x00 or 0x80.
 * For example:
 * for (i : BitMask<uint32_t, 16>(0x5)) -> yields 0, 2
 * for (i : BitMask<uint64_t, 8, 3>(0x0000000080800000)) -> yields 2, 3
 */
template <class T, int SignificantBits, int Shift = 0> class BitMask {
public:
    explicit BitMask(T mask) : mask_(mask) {}

    BitMask &operator ++ ()
    {
        mask_ &= (mask_ - 1);
        return *this;
    }

    explicit operator bool() const
    {
        return mask_ != 0;
    }

    uint32_t operator*() const
    {
        return LowestBitSet();
    }

    uint32_t LowestBitSet() const
    {
        return TrailingZeros(mask_) >> Shift;
    }

    BitMask begin() const
    {
        return *this;
    }

    BitMask end() const
    {
        return BitMask(0);
    }

private:
    friend bool operator == (const BitMask &left, const BitMask &right)
    {
        return left.mask_ == right.mask_;
    }

    friend bool operator != (const BitMask &left, const BitMask &right)
    {
        return left.mask_ != right.mask_;
    }

    T mask_;
};

static const uint32_t SHIFT_DISTANCE_7 = 7;
using ctrl_t = signed char;
using h2_t = uint8_t;

ALWAYS_INLINE static size_t H1(size_t hashVal)
{
    return (hashVal >> SHIFT_DISTANCE_7);
}

ALWAYS_INLINE static h2_t H2(size_t hashVal)
{
    return (h2_t)(ctrl_t)(hashVal & 0x7F);
}

enum Ctrl : ctrl_t {
    kEmpty = -128, // 0b10000000 or 0x80
    kSentinel = -1
};

struct Group {
    enum {
        kWidth = 16
    }; // the number of slots per group
};

/// Bucket-range bounds for Velox-style parallel join hash table build.
struct TableInsertPartitionInfo {
    size_t start = 0;
    size_t end = 0;

    bool InRange(size_t index) const
    {
        return index >= start && index < end;
    }
};

static constexpr size_t kPartitionOverflow = std::numeric_limits<size_t>::max();

template <size_t Width> class ProbeSeq {
public:
    ProbeSeq(size_t hashVal, size_t mask)
    {
        mask_ = mask;
        offset_ = hashVal & mask_;
    }

    ALWAYS_INLINE size_t GetOffset() const
    {
        return offset_;
    }

    ALWAYS_INLINE size_t GetOffset(size_t i) const
    {
        return (offset_ + i) & mask_;
    }

    ALWAYS_INLINE void GetNext()
    {
        index_ += Width;
        offset_ += index_;
        offset_ &= mask_;
    }

    ALWAYS_INLINE size_t GetIndex() const
    {
        return index_;
    }

private:
    size_t mask_;
    size_t offset_;
    size_t index_ = 0;
};

/**
 * A SIMD HashMap Implementation
 * @tparam KeyType must have default constructor, which supports move-assign function or copy-assign function
 * @tparam ValueType must support movable
 * @tparam HashType hash Algorithm of KeyType
 * @tparam GrowStrategy rehash when size exceed max load factor
 * @tparam Allocator memory allocator
 */
template <typename KeyType, typename ValueType, typename HashType, typename GrowStrategy, typename Allocator,
    std::enable_if_t<std::is_move_constructible_v<KeyType> && std::is_move_constructible_v<ValueType> &&
    (std::is_move_assignable_v<ValueType> || std::is_copy_assignable_v<ValueType>)>* = nullptr>
class BaseHashMap {
public:
    using Slot = HashSlot<KeyType, ValueType>;
    using HashKey = KeyType;
    using ResultType = InsertResult<ValueType>;
    using Keys = KeyType;
    using Values = ValueType;

public:
    BaseHashMap(uint8_t initDegree = defaultDegreeSize) : grower(initDegree)
    {
        InitSlots(grower.GetCurrentSize());
    }

    ALWAYS_INLINE void InitSlots(uint64_t newCapacity)
    {
        capacity = newCapacity;

        allocator.Allocate((newCapacity + Group::kWidth) * sizeof(ctrl_t), &ctrlAddress);
        identifiers = reinterpret_cast<ctrl_t *>(ctrlAddress);
        memset(identifiers, kEmpty, sizeof(ctrl_t) * newCapacity);
        memset(identifiers + newCapacity, kSentinel, sizeof(ctrl_t) * Group::kWidth);

        allocator.Allocate(newCapacity * sizeof(Slot), &slotsAddress);
        slots = reinterpret_cast<Slot *>(slotsAddress);
    }

    BaseHashMap(const BaseHashMap &) = delete;

    BaseHashMap &operator = (const BaseHashMap &) = delete;

    BaseHashMap &operator = (BaseHashMap &&) = delete;

    BaseHashMap(BaseHashMap &&o) = delete;

    bool HasNullCell() const
    {
        return nullSlot != nullptr;
    }

    size_t GetElementsSize() const
    {
        return elementsSize;
    }

    size_t GetCapacity() const
    {
        return capacity;
    }

    void SetElementsSize(size_t size)
    {
        elementsSize = size;
    }

    void SetRehashDisabled(bool disabled)
    {
        rehashDisabled_ = disabled;
    }

    void EnableParallelBuildCounter(std::atomic<size_t> *counter)
    {
        elementsSizeCounter_ = counter;
    }

    void DisableParallelBuildCounter()
    {
        elementsSizeCounter_ = nullptr;
    }

    /**
     * Find Matched Join Position
     * Note: We use the second element of InsertResult to indicate whether to find the matched position or not.
     */
    template <typename T,
        std::enable_if_t<std::is_same_v<std::remove_reference_t<std::remove_cv_t<T>>, KeyType>>* = nullptr>
    ALWAYS_INLINE InsertResult<ValueType> FindMatchPosition(T &&key)
    {
        auto hashValue = CalculateHash(key);
        bool notHasKey = false;
        auto pos = FindPosition(key, hashValue, notHasKey);
        if (LIKELY(notHasKey)) {
            return InsertResult<ValueType>(nullSlot->GetValue(), false);
        }
        auto &curSlot = slots[pos];
        return InsertResult<ValueType>(curSlot.GetValue(), true);
    }

    /**
     * Insert key into hashmap
     * - if a new key inserted, the key will be copied or moved to create Slot. Caller can check InsertResult to
     * determine whether need to set value.
     * - Caller can use this function to update data, too. Just call GetValue function of InsertResult. It will return
     * value reference.
     */
    template <typename T,
        std::enable_if_t<std::is_same_v<std::remove_reference_t<std::remove_cv_t<T>>, KeyType>>* = nullptr>
    InsertResult<ValueType> Emplace(T &&key)
    {
        if (NeedRehash()) {
            Rehash();
        }

        auto hashValue = CalculateHash(key);
        bool inserted = false;
        auto pos = FindPosition(key, hashValue, inserted);
        if (LIKELY(inserted)) {
            new (&slots[pos]) Slot(std::forward<T>(key));
            identifiers[pos] = (h2_t)H2(hashValue);
            ++elementsSize;
        }
        auto &slot = slots[pos];
        slot.SetHashVal(hashValue);
        return InsertResult<ValueType>(slot.GetValue(), inserted);
    }

    template <typename T> ALWAYS_INLINE InsertResult<ValueType> EmplaceNotNullKey(T &&key)
    {
        bool inserted = false;
        auto hashValue = hasher(key);
        auto pos = FindPosition(key, hashValue, inserted);

        if (LIKELY(inserted)) {
            new (&slots[pos]) Slot(std::forward<T>(key));
            identifiers[pos] = (h2_t)H2(hashValue);
            ++elementsSize;
        }
        slots[pos].SetHashVal(hashValue);
        return InsertResult<ValueType>(slots[pos].GetValue(), inserted);
    }

    template <typename T> ALWAYS_INLINE InsertResult<ValueType> EmplaceNotNullKey(T &&key, size_t &hashValue)
    {
        bool inserted = false;
        auto pos = FindPosition(key, hashValue, inserted);

        if (LIKELY(inserted)) {
            new (&slots[pos]) Slot(std::forward<T>(key));
            identifiers[pos] = (h2_t)H2(hashValue);
            IncrementElementsSize();
        }
        slots[pos].SetHashVal(hashValue);
        return InsertResult<ValueType>(slots[pos].GetValue(), inserted);
    }

    /// Partition-bounded insert for parallel join build. Returns nullopt when the row overflows the partition range.
    template <typename T>
    ALWAYS_INLINE std::optional<InsertResult<ValueType>> TryEmplaceNotNullKeyWithPartition(
        T &&key, size_t hashValue, const TableInsertPartitionInfo *partitionInfo)
    {
        bool inserted = false;
        auto pos = FindPositionWithPartition(key, hashValue, inserted, partitionInfo);
        if (pos == kPartitionOverflow) {
            return std::nullopt;
        }

        if (LIKELY(inserted)) {
            new (&slots[pos]) Slot(std::forward<T>(key));
            identifiers[pos] = (h2_t)H2(hashValue);
            IncrementElementsSize();
        }
        slots[pos].SetHashVal(hashValue);
        return InsertResult<ValueType>(slots[pos].GetValue(), inserted);
    }

    // key can be any data
    template <typename T> InsertResult<ValueType> EmplaceNullValue(T &&key)
    {
        if (nullSlot == nullptr) {
            ++elementsSize;
            allocator.Allocate(sizeof(Slot), reinterpret_cast<uint8_t **>(&nullSlot));
            new (nullSlot)Slot(std::forward<T>(key));
            return InsertResult<ValueType>{ nullSlot->GetValue(), true };
        } else {
            return InsertResult<ValueType>{ nullSlot->GetValue(), false };
        }
    }

    template <class Func> void ForEachKV(Func &&func)
    {
        int remainNum = elementsSize;
        if (HasNullCell()) {
            --remainNum;
            func(nullSlot->GetKey(), nullSlot->GetValue());
        }

        int index = 0;
        while (remainNum) {
            __builtin_prefetch(identifiers + index + 1, 0, 3);
            __builtin_prefetch(slots + index + 1, 0, 3);
            while (IsEmptyOrDeleted(*(identifiers + index))) {
                // ctrl is not necessarily aligned to Group::kWidth. It is also likely
                // to read past the space for ctrl bytes and into slots. This is ok
                // because ctrl has sizeof() == 1 and slot has sizeof() >= 1 so there
                // is no way to read outside the combined slot array.
                size_t shift = CountLeadingValue<ctrl_t, Group::kWidth>(kEmpty, identifiers + index);
                index += shift;
            }
            auto &slot = slots[index];
            func(slot.GetKey(), slot.GetValue());
            ++index;
            --remainNum;
        }
        return;
    }

    template <class Func> void ForEachKV(Func &&func) const
    {
        int remainNum = elementsSize;
        if (HasNullCell()) {
            --remainNum;
            func(nullSlot->GetKey(), nullSlot->GetValue());
        }

        int index = 0;
        while (remainNum) {
            __builtin_prefetch(identifiers + index + 1, 0, 3);
            __builtin_prefetch(slots + index + 1, 0, 3);
            while (IsEmptyOrDeleted(*(identifiers + index))) {
                // ctrl is not necessarily aligned to Group::kWidth. It is also likely
                // to read past the space for ctrl bytes and into slots. This is ok
                // because ctrl has sizeof() == 1 and slot has sizeof() >= 1 so there
                // is no way to read outside the combined slot array.
                size_t shift = CountLeadingValue<ctrl_t, Group::kWidth>(kEmpty, identifiers + index);
                index += shift;
            }
            auto &slot = slots[index];
            func(slot.GetKey(), slot.GetValue());
            ++index;
            --remainNum;
        }
        return;
    }

    template <class Func> void ForEachValue(Func &&func)
    {
        int remainNum = elementsSize;
        if (HasNullCell()) {
            --remainNum;
            func(nullSlot->GetValue(), -1);
        }
        int index = 0;
        while (remainNum && index < capacity) {
            __builtin_prefetch(identifiers + index + 1, 0, 3);
            __builtin_prefetch(slots + index + 1, 0, 3);
            while (IsEmptyOrDeleted(*(identifiers + index))) {
                // ctrl is not necessarily aligned to Group::kWidth. It is also likely
                // to read past the space for ctrl bytes and into slots. This is ok
                // because ctrl has sizeof() == 1 and slot has sizeof() >= 1 so there
                // is no way to read outside the combined slot array.
                size_t shift = CountLeadingValue<ctrl_t, Group::kWidth>(kEmpty, identifiers + index);
                index += shift;
            }
            auto &slot = slots[index];
            func(slot.GetValue(), index);
            ++index;
            --remainNum;
        }
    }
    size_t CalculateHash(const KeyType &key)
    {
        return hasher(key);
    }

    // get the number of hash keys
    uint64_t GetElementsSize()
    {
        return elementsSize;
    }

    ~BaseHashMap()
    {
        if constexpr (not std::is_trivially_destructible<Slot>::value) {
            DeconstructAllSlot();
        }
        ReleaseSlots(capacity);
        if (nullSlot != nullptr) {
            nullSlot->~Slot();
            allocator.Release(reinterpret_cast<uint8_t *>(nullSlot), sizeof(Slot));
        }
    }

    void ReleaseSlots(uint64_t newCapacity) noexcept
    {
        allocator.Release(slotsAddress, sizeof(Slot) * newCapacity);
        allocator.Release(ctrlAddress, sizeof(ctrl_t) * (newCapacity + Group::kWidth));
    }

    void Reset()
    {
        elementsSize = 0;
        if (nullSlot != nullptr) {
            nullSlot->~Slot();
            allocator.Release(reinterpret_cast<uint8_t *>(nullSlot), sizeof(Slot));
            nullSlot = nullptr;
        }
        memset(identifiers, kEmpty, sizeof(ctrl_t) * capacity);
        memset(identifiers + capacity, kSentinel, sizeof(ctrl_t) * Group::kWidth);
        memset(slotsAddress, 0, sizeof(Slot) * capacity);
    }

    class HashmapIteratorOutput {
    public:
        HashmapIteratorOutput(BaseHashMap<KeyType, ValueType, HashType, GrowStrategy, Allocator> *mapPtr, uint32_t pos,
            uint32_t hasBeenOutputCount, ctrl_t *identifiers)
            : hashMapPtr(mapPtr)
        {
            remainSlot = hashMapPtr->GetElementsSize() - (hashMapPtr->HasNullCell() ? 1 : 0) - hasBeenOutputCount;

            identifiers_ = std::move(identifiers);
            this->pos = pos;
        }

        template <class Func, class NullFunc> OutputState HandleElements(uint32_t expectSize, Func func,
            NullFunc nullFunc)
        {
            uint32_t remainHandleSize = expectSize;
            while (remainSlot && remainHandleSize) {
                FindNext();
                --remainSlot;
                --remainHandleSize;
                func((hashMapPtr->slots + pos)->GetKey(), (hashMapPtr->slots + pos)->GetValue());
                // value in cur pos has been assigned , so we need to plus one
                MoveToNext();
            }
            if (remainHandleSize == 0) {
                return OutputState(pos, expectSize);
            }
            if (hashMapPtr->HasNullCell()) {
                --remainHandleSize;
                nullFunc(hashMapPtr->nullSlot->GetKey(), hashMapPtr->nullSlot->GetValue());
            }
            return OutputState(pos, expectSize - remainHandleSize);
        }

        template <class Func, class NullFunc> OutputState HandleElementsWithHashVal(uint32_t expectSize, Func func,
            NullFunc nullFunc)
        {
            uint32_t remainHandleSize = expectSize;
            while (remainSlot && remainHandleSize) {
                FindNext();
                --remainSlot;
                --remainHandleSize;
                func((hashMapPtr->slots + pos)->GetKey(), (hashMapPtr->slots + pos)->GetValue(), (hashMapPtr->slots + pos)->GetHashVal());
                // value in cur pos has been assigned , so we need to plus one
                MoveToNext();
            }
            if (remainHandleSize == 0) {
                return OutputState(pos, expectSize);
            }
            if (hashMapPtr->HasNullCell()) {
                --remainHandleSize;
                nullFunc(hashMapPtr->nullSlot->GetKey(), hashMapPtr->nullSlot->GetValue(), (hashMapPtr->slots + pos)->GetHashVal());
            }
            return OutputState(pos, expectSize - remainHandleSize);
        }

    private:
        BaseHashMap<KeyType, ValueType, HashType, GrowStrategy, Allocator> *hashMapPtr;
        ctrl_t *identifiers_ = EmptyGroup();
        uint32_t pos = 0;
        uint32_t remainSlot = 0;

        void FindNext()
        {
            while (IsEmptyOrDeleted(*(identifiers_ + pos))) {
                // ctrl is not necessarily aligned to Group::kWidth. It is also likely
                // to read past the space for ctrl bytes and into slots. This is ok
                // because ctrl has sizeof() == 1 and slot has sizeof() >= 1 so there
                // is no way to read outside the combined slot array.
                size_t shift = CountLeadingValue<ctrl_t, Group::kWidth>(kEmpty, identifiers_ + pos);
                pos += shift;
            }
        }

        void MoveToNext()
        {
            ++pos;
        }
    };

    HashmapIteratorOutput GetOutputMachine(uint32_t pos = 0, uint32_t hasBeenOutputCount = 0)
    {
        return HashmapIteratorOutput(this, pos, hasBeenOutputCount, identifiers);
    }

private:
    void Reinsert(Slot &&cell)
    {
        auto pos = FindPosition(cell.GetKey(), cell.GetHashVal());
        identifiers[pos] = (h2_t)H2(cell.GetHashVal());
        new (&slots[pos]) Slot(std::move(cell));
    }

    void Rehash()
    {
        auto oldIdentifiersAddress = ctrlAddress;
        auto oldElements = elementsSize;
        Slot *oldSlots = slots;
        auto oldSlotsAddress = slotsAddress;
        auto oldIdentifiers = identifiers;
        auto oldCapacity = capacity;

        uint64_t reHashSize = grower.GrowSize();
        InitSlots(reHashSize);
        int remainNum = oldElements - (HasNullCell() ? 1 : 0);
        int index = 0;
        while (remainNum != 0) {
            __builtin_prefetch(identifiers + index + 1, 0, 3);
            __builtin_prefetch(oldSlots + index + 1, 0, 3);
            while (IsEmptyOrDeleted(*(oldIdentifiers + index))) {
                // ctrl is not necessarily aligned to Group::kWidth. It is also likely
                // to read past the space for ctrl bytes and into slots. This is ok
                // because ctrl has sizeof() == 1 and slot has sizeof() >= 1 so there
                // is no way to read outside the combined slot array.
                size_t shift = CountLeadingValue<ctrl_t, Group::kWidth>(kEmpty, oldIdentifiers + index);
                index += shift;
            }
            Reinsert(std::move(oldSlots[index]));
            --remainNum;
            ++index;
        }

        allocator.Release(oldSlotsAddress, sizeof(Slot) * oldCapacity);
        allocator.Release(oldIdentifiersAddress, sizeof(ctrl_t) * (oldCapacity + Group::kWidth));
    }

    bool NeedRehash()
    {
        if (rehashDisabled_) {
            return false;
        }
        return elementsSize > grower.GetThreshHold();
    }

    static bool IsEmptyOrDeleted(ctrl_t c)
    {
        return c < kSentinel;
    }

    template <size_t kWidth>
    ProbeSeq<kWidth> Probe(size_t hashVal) const
    {
        return ProbeSeq<kWidth>(H1(hashVal), capacity - 1);
    }

    size_t FindPosition(const KeyType& key, size_t hashValue, bool& inserted)
    {
        return FindPositionWithPartition(key, hashValue, inserted, nullptr);
    }

    size_t FindPositionWithPartition(const KeyType& key, size_t hashValue, bool& inserted,
        const TableInsertPartitionInfo *partitionInfo)
    {
        auto seq = Probe<Group::kWidth>(hashValue);
        auto hashValueH2 = static_cast<ctrl_t>(H2(hashValue));
        const size_t startBucketOffset = partitionInfo != nullptr ? seq.GetOffset() : 0;
        size_t numProbedBuckets = 0;
        __builtin_prefetch(identifiers + seq.GetOffset(), 0, 3);
        while (identifiers[seq.GetOffset()] != kEmpty) {
            size_t curOffset = seq.GetOffset();
            if (partitionInfo != nullptr) {
                if (!partitionInfo->InRange(curOffset)) {
                    return kPartitionOverflow;
                }
                if (numProbedBuckets > 0 && curOffset <= startBucketOffset) {
                    return kPartitionOverflow;
                }
            }
            __builtin_prefetch(identifiers + seq.GetOffset() + Group::kWidth, 0, 3);
            auto maskIter = FindMatchNibbles<ctrl_t, Group::kWidth>(static_cast<ctrl_t>(hashValueH2), identifiers + seq.GetOffset());
            while (maskIter.HasNext()) {
                auto v = maskIter.Next();
                if (slots[seq.GetOffset((size_t)v)].IsSameKey(hashValue, key)) {
                    return seq.GetOffset((size_t)v);
                }
            }

            auto firstIndex = FindFirstNibbles<ctrl_t, Group::kWidth>(kEmpty, identifiers + seq.GetOffset());
            if (firstIndex != -1) {
                inserted = true;
                return seq.GetOffset(firstIndex);
            }
            seq.GetNext();
            numProbedBuckets += Group::kWidth;
            if (seq.GetIndex() > capacity) {
                break;
            }
        }
        size_t curOffset = seq.GetOffset();
        if (partitionInfo != nullptr) {
            if (!partitionInfo->InRange(curOffset)) {
                return kPartitionOverflow;
            }
            if (numProbedBuckets > 0 && curOffset <= startBucketOffset) {
                return kPartitionOverflow;
            }
        }
        inserted = true;
        return curOffset;
    }

    // update newIsAssigned when rehash
    size_t FindPosition(const KeyType &key, size_t hashValue)
    {
        auto noFlag = false;
        return FindPosition(key, hashValue, noFlag);
    }

    void IncrementElementsSize()
    {
        if (elementsSizeCounter_ != nullptr) {
            elementsSizeCounter_->fetch_add(1, std::memory_order_relaxed);
        } else {
            ++elementsSize;
        }
    }

    // call deconstruct function of every cell
    void DeconstructAllSlot()
    {
        int remainNum = HasNullCell() ? elementsSize - 1 : elementsSize;
        int index = 0;
        while (remainNum) {
            __builtin_prefetch(identifiers + index + 1, 0, 3);
            __builtin_prefetch(slots + index + 1, 0, 3);
            while (IsEmptyOrDeleted(*(identifiers + index))) {
                // ctrl is not necessarily aligned to Group::kWidth. It is also likely
                // to read past the space for ctrl bytes and into slots. This is ok
                // because ctrl has sizeof() == 1 and slot has sizeof() >= 1 so there
                // is no way to read outside the combined slot array.
                size_t shift = CountLeadingValue<ctrl_t, Group::kWidth>(kEmpty, identifiers + index);
                index += shift;
            }
            slots[index].~Slot();
            ++index;
            --remainNum;
        }
        return;
    }

    static ctrl_t *EmptyGroup()
    {
        alignas(16) static constexpr ctrl_t empty_group[] = {
            kSentinel, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty,
            kEmpty,    kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty
        };
        return const_cast<ctrl_t *>(empty_group);
    }

    Allocator allocator;
    uint8_t *slotsAddress = nullptr;
    Slot *slots = nullptr;
    ctrl_t *identifiers = EmptyGroup();
    uint8_t *ctrlAddress = nullptr;
    Slot *nullSlot = nullptr;
    size_t elementsSize = 0; // the number of hash keys
    std::atomic<size_t> *elementsSizeCounter_ = nullptr;
    bool rehashDisabled_ = false;
    HashType hasher;
    uint64_t capacity = 0; // the number of hashmap capacity
    static constexpr uint8_t defaultDegreeSize = 15;
    GrowStrategy grower;
};

template <typename KeyType, typename ValueType>
using DefaultHashMap = BaseHashMap<KeyType, ValueType, GroupbyHashCalculator<KeyType>, Grower, OmniHashmapAllocator>;
}
}
#endif // OMNI_RUNTIME_BASE_HASH_MAP_H
