/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: Hash Aggregation Sort Header
 */

#ifndef OMNI_RUNTIME_GROUP_AGGREATION_SORT_H
#define OMNI_RUNTIME_GROUP_AGGREATION_SORT_H

#include <string>
#include "aggregator/aggregator.h"
#include "type/data_types.h"
#include "type/string_ref.h"
#include "operator/hashmap/base_hash_map.h"


namespace omniruntime::op {
class TaperColumnSerializeHandler;

class AggregationSort {
public:
    // needKeyBuffer must be false only for handlers whose ParseHashMapToVector overload leaves kvString
    // untouched, i.e. the serialize handler, which points kvVec at keys already materialized in the arena.
    explicit AggregationSort(std::vector<std::unique_ptr<Aggregator>> &aggregators, bool needKeyBuffer = true,
        TaperColumnSerializeHandler* serializeHandler = nullptr)
        : aggregators(aggregators), needKeyBuffer(needKeyBuffer), serializeHandler(serializeHandler)
    {
        for (auto &aggregator : aggregators) {
            aggVectorCounts.emplace_back(aggregator->GetSpillType().size());
        }
    }

    void ResizeKvVector(size_t size)
    {
        kvVec.resize(size);
        if (needKeyBuffer) {
            kvString.resize(size);
        }
        groupCount = size;
    }

    void ParseHashMapToVector(const omniruntime::type::StringRef &key, AggregateState *value, size_t groupIndex)
    {
        auto &kv = kvVec[groupIndex];
        kv.keyAddr = const_cast<char *>(key.data);
        kv.keyLen = key.size;
        kv.value = value;
    }

    void ParseHashMapToVectorWithHashVal(const omniruntime::type::StringRef &key, AggregateState *value, size_t groupIndex, int64_t hashValue)
    {
        auto &kv = kvVec[groupIndex];
        kv.keyAddr = const_cast<char *>(key.data);
        kv.keyLen = key.size;
        kv.hashValue = hashValue;
        kv.value = value;
    }

    void ParseHashMapRowToVectorWithHashVal(
        uint8_t* row, AggregateState* value, size_t groupIndex, int64_t hashValue)
    {
        auto& kv = kvVec[groupIndex];
        kv.rowAddr = row;
        kv.keyLen = 0;
        kv.hashValue = hashValue;
        kv.value = value;
    }

    template<typename T>
    void ParseHashMapToVector(const T &key, AggregateState *value, size_t groupIndex)
    {
        auto &kv = kvVec[groupIndex];
        kvString[groupIndex] = std::to_string(key);
        kv.keyAddr = const_cast<char *>(kvString[groupIndex].c_str());
        kv.keyLen = kvString[groupIndex].size();
        kv.value = value;
    }

    template<typename T>
    void ParseHashMapToVectorAsBytes(const T &key, AggregateState *value, size_t groupIndex)
    {
        auto &kv = kvVec[groupIndex];
        kvString[groupIndex].assign(reinterpret_cast<const char *>(&key), sizeof(T));
        kv.keyAddr = const_cast<char *>(kvString[groupIndex].data());
        kv.keyLen = kvString[groupIndex].size();
        kv.value = value;
    }

    template<typename T>
    void ParseNullHashMapToVector(const T &key, AggregateState *value, size_t groupIndex)
    {
        auto &kv = kvVec[groupIndex];
        kvString[groupIndex] = std::to_string(key);
        kv.keyAddr = const_cast<char *>(kvString[groupIndex].c_str());
        kv.keyLen = 0;
        kv.value = value;
    }

    void ClearVector()
    {
        // Give the capacity back rather than only clearing. These buffers sit idle between two spills,
        // and keeping them resident raises the baseline that the next spill threshold is compared against.
        std::vector<omniruntime::op::KeyValue>().swap(kvVec);
        std::vector<std::string>().swap(kvString);
    }

    size_t GetRowCount()
    {
        return groupCount;
    }

    void SortKvVector(bool compareWithHashVal = false);

    bool IsLazyKeySerializationEnabled() const
    {
        return serializeHandler != nullptr;
    }

    void SetSpillVectorBatch(vec::VectorBatch *spillVecBatch, uint64_t rowOffset, bool compareWithHashVal);

private:
    std::vector<std::unique_ptr<Aggregator>> &aggregators;
    std::vector<omniruntime::op::KeyValue> kvVec;
    std::vector<std::string> kvString;
    std::vector<AggregateState *> groupStates;
    size_t groupCount = 0;
    std::vector<int32_t> aggVectorCounts;
    bool needKeyBuffer = true;
    TaperColumnSerializeHandler* serializeHandler = nullptr;

    static ALWAYS_INLINE bool HashKeyCompareWithHashVal(const omniruntime::op::KeyValue &a, omniruntime::op::KeyValue &b)
    {
        if (a.hashValue != b.hashValue) {
            return a.hashValue < b.hashValue;
        }

        return memcmp(a.keyAddr, b.keyAddr, std::min(a.keyLen, b.keyLen)) < 0;
    }

    static ALWAYS_INLINE bool HashKeyCompare(const omniruntime::op::KeyValue &a, omniruntime::op::KeyValue &b)
    {
        int cmp = memcmp(a.keyAddr, b.keyAddr, std::min(a.keyLen, b.keyLen));
        if (cmp != 0) return cmp < 0;
        return a.keyLen < b.keyLen;
    }
};
}


#endif // OMNI_RUNTIME_GROUP_AGGREATION_SORT_H
