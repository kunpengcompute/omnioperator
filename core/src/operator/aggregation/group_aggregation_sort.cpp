/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 * Description: Hash Aggregation Source File
 */

#include "group_aggregation_sort.h"
#include "operator/hashmap/column_marshaller.h"
#include "vector/vector_helper.h"

using namespace omniruntime::op;

void AggregationSort::SortKvVector(bool compareWithHashVal)
{
    if (compareWithHashVal && serializeHandler != nullptr) {
        std::string leftKey;
        std::string rightKey;
        std::sort(kvVec.begin(), kvVec.end(), [this, &leftKey, &rightKey](const KeyValue& a, const KeyValue& b) {
            if (a.hashValue != b.hashValue) {
                return a.hashValue < b.hashValue;
            }

            serializeHandler->SerializeSpillKey(a.rowAddr, leftKey);
            serializeHandler->SerializeSpillKey(b.rowAddr, rightKey);
            return memcmp(leftKey.data(), rightKey.data(), std::min(leftKey.size(), rightKey.size())) < 0;
        });
        return;
    }

    std::sort(kvVec.begin(), kvVec.end(), compareWithHashVal ? HashKeyCompareWithHashVal : HashKeyCompare);
}

void AggregationSort::SetSpillVectorBatch(vec::VectorBatch *spillVecBatch, uint64_t rowOffset, bool compareWithHashVal)
{
    vec::VectorHelper::ResetVarcharVectorsForReuse(spillVecBatch);

    // first set hash values
    Vector<LargeStringContainer<std::string_view>> * keyVector = nullptr;
    Vector<int64_t> * hashVector = nullptr;
    if (compareWithHashVal) {
        hashVector = reinterpret_cast<Vector<int64_t> *>(spillVecBatch->Get(0));
        keyVector = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(spillVecBatch->Get(1));
    } else {
        keyVector = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(spillVecBatch->Get(0));
    }
    auto kvPtr = kvVec.data() + rowOffset;
    auto rowCount = spillVecBatch->GetRowCount();
    groupStates.resize(rowCount);
    std::string serializedKey;
    for (int32_t rowIndex = 0; rowIndex < rowCount; rowIndex++) {
        auto &kv = kvPtr[rowIndex];
        std::string_view keyStr;
        if (compareWithHashVal && serializeHandler != nullptr) {
            serializeHandler->SerializeSpillKey(kv.rowAddr, serializedKey);
            keyStr = std::string_view(serializedKey.data(), serializedKey.size());
        } else {
            keyStr = std::string_view(kv.keyAddr, kv.keyLen);
        }
        if (compareWithHashVal) {
            hashVector->SetValue(rowIndex, static_cast<int64_t>(kv.hashValue));
        }
        keyVector->SetValue(rowIndex, keyStr);
        groupStates[rowIndex] = kv.value;
    }

    const size_t aggNum = aggregators.size();
    if (aggNum > 0) {
        std::vector<BaseVector *> oneAggOutputVecs;
        auto aggOutputStartIndex = compareWithHashVal ? 2 : 1;
        for (size_t aggIndex = 0; aggIndex < aggNum; ++aggIndex) {
            int32_t oneAggOutputCols = aggVectorCounts[aggIndex];
            oneAggOutputVecs.resize(oneAggOutputCols);
            for (auto j = 0; j < oneAggOutputCols; j++) {
                oneAggOutputVecs[j] = spillVecBatch->Get(aggOutputStartIndex + j);
            }
            aggOutputStartIndex += oneAggOutputCols;

            aggregators[aggIndex]->ExtractValuesForSpill(groupStates, oneAggOutputVecs);
        }
    }
}