/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * @Description: join hash table
 */

#ifndef OMNI_RUNTIME_JOIN_HASH_TABLE_VARIANTS_H
#define OMNI_RUNTIME_JOIN_HASH_TABLE_VARIANTS_H

#include <memory>
#include <variant>
#include <vector>

#include "operator/status.h"
#include "operator/hashmap/crc_hasher.h"
#include "operator/hashmap/base_hash_map.h"
#include "operator/hashmap/array_map.h"
#include "operator/hashmap/column_marshaller.h"
#include "type/data_types.h"
#include "common_join.h"
#include "row_ref.h"

namespace omniruntime {
namespace op {

constexpr int32_t BITS_OF_BYTE = 8;
constexpr int32_t BITS_OF_SHORT = 16;
constexpr int32_t BITS_OF_INT = 32;
constexpr int32_t BITS_OF_LONG = 64;
constexpr int32_t BITS_OF_DECIMAL = 64;
constexpr int32_t BITS_OF_LONGLONG = 128;
constexpr int32_t NOT_EXPECTED_TYPE = 0;

// Join hash tables are pre-sized to 0.75 load factor and never rehash, so caching the hash
// per slot would be dead weight. Disabling it shrinks the slot from 24B to 16B for
// int32/int64 keys (~33% of the slot array).
template <typename KeyType, typename ValueType>
using JoinHashMap =
    BaseHashMap<KeyType, ValueType, omniruntime::simdutil::HashCRC32<KeyType>, Grower, OmniHashmapAllocator, false>;

template <typename KeyType, typename RowRefListType>
using JoinHashTableVariant = ColumnSerializeHandler<JoinHashMap<KeyType, RowRefListType *>>;

enum class HashTableImplementationType {
    NORMAL_HASH_TABLE,
    ARRAY_HASH_TABLE
};

template <typename KeyType, typename RowRefListType> class JoinHashTableVariants {
public:
    using Key = KeyType;
    using Mapped = RowRefListType;
    static constexpr bool IS_SIMPLE_KEY = (std::is_same_v<KeyType, int16_t> || std::is_same_v<KeyType, int32_t> ||
                                           std::is_same_v<KeyType, int64_t>);

    explicit JoinHashTableVariants(uint32_t hashTableCount, DataTypes *buildDataTypes,
        std::vector<int32_t> &buildHashCols, JoinType joinType, BuildSide buildSide, bool isMultiCols = false);

    ~JoinHashTableVariants();

    ALWAYS_INLINE uint32_t GetHashTableCount() const
    {
        return hashTableCount;
    }

    ALWAYS_INLINE uint32_t GetHashTableSize() const
    {
        return hashTableSize;
    }

    ALWAYS_INLINE DataTypes *GetBuildDataTypes()
    {
        return this->buildTypes;
    }

    ALWAYS_INLINE void SetProbeTypes(DataTypes *probeDataTypes)
    {
        this->probeTypes = probeDataTypes;
    }

    ALWAYS_INLINE std::unique_ptr<JoinHashTableVariant<KeyType, RowRefListType>> &GetHashTable(int32_t partitionIndex)
    {
        return hashTables[partitionIndex];
    }

    ALWAYS_INLINE std::unique_ptr<DefaultArrayMap<RowRefListType>> &GetArrayTable(int32_t partitionIndex)
    {
        return arrayTables[partitionIndex];
    }

    ALWAYS_INLINE std::pair<int64_t, int64_t> &GetmaxMinValue(int32_t partitionIndex)
    {
        return maxMins[partitionIndex];
    }

    ALWAYS_INLINE HashTableImplementationType GetHashTableTypes(int32_t partitionIndex)
    {
        return hashTableTypes[partitionIndex];
    }

    ALWAYS_INLINE void AddVecBatch(int32_t partitionIndex, omniruntime::vec::VectorBatch *vecBatch)
    {
        inputVecBatches[partitionIndex].emplace_back(vecBatch);
        totalRowCount[partitionIndex] += vecBatch->GetRowCount();
    }

    ALWAYS_INLINE uint32_t GetTotalRowCount(int32_t partitionIndex) const
    {
        return totalRowCount[partitionIndex];
    }

    ALWAYS_INLINE std::vector<omniruntime::vec::VectorBatch *> TakeResidentBatches(int32_t partitionIndex)
    {
        std::vector<omniruntime::vec::VectorBatch *> taken;
        taken.swap(inputVecBatches[partitionIndex]);
        totalRowCount[partitionIndex] = 0;
        return taken;
    }

    void ResetPartition(int32_t partitionIndex);

    /// Drops HT / arena / resident batches after the last probe consumer is finished.
    void ReleaseBuildState();
    ALWAYS_INLINE bool IsBuildStateReleased() const
    {
        return buildStateReleased_;
    }

    InsertResult<RowRefListType *> Find(std::vector<VectorSerializerIgnoreNull> &probeSerializers,
                                        ExecutionContext *probeArena, BaseVector **probeHashColumns,
                                        int32_t probeHashColCount, int32_t probePosition, uint32_t partition);

    ALWAYS_INLINE bool CanProbeSIMD(BaseVector** probeHashColumns, int32_t probeHashColCount, uint32_t partition)
    {
        if constexpr (IS_SIMPLE_KEY) {
            return hashTableTypes[partition] == HashTableImplementationType::ARRAY_HASH_TABLE &&
                   probeHashColCount == 1 && probeHashColumns[0]->GetEncoding() != OMNI_DICTIONARY &&
                   probeHashColumns[0]->GetEncoding() != OMNI_ENCODING_CONST;
        }
        return false;
    }

    ALWAYS_INLINE KeyType* GetSingleProbeHashKeyBase(BaseVector **probeHashColumns)
    {
        return reinterpret_cast<KeyType*>(VectorHelper::UnsafeGetValues(probeHashColumns[0]));
    }

    ALWAYS_INLINE int64_t GetMinValue(uint32_t partition)
    {
        return maxMins[partition].second;
    }

    void ComputeMultiColKey(BaseVector **hashColumns, int32_t hashColCount, int32_t index, KeyType& key);

    KeyType GetKeyValue(BaseVector **probeHashColumns, int32_t probePosition);

    void PositionVisited(ForwardIterator<RowRefListType> it)
    {
        if constexpr (std::is_same_v<RowRefListType, RowRefListWithFlags>) {
            if (!it->visited) {
                it->visited = true;
                visitedCounts++;
            }
        }
    }

    ALWAYS_INLINE uint32_t GetVisitedCounts() const
    {
        return visitedCounts;
    }

    ALWAYS_INLINE bool GetIsMultiCols() const
    {
        return isMultiCols;
    }

    ALWAYS_INLINE uint32_t GetTotalVisitedCounts() const
    {
        return totalVisitedCounts;
    }

    ALWAYS_INLINE void SetTotalVisitedCounts(int cnt)
    {
        totalVisitedCounts += cnt;
    }

    ALWAYS_INLINE BaseVector ***GetColumns(int32_t partitionIndex) const
    {
        return columns[partitionIndex];
    }

    /// Append a trailing BOOLEAN visited column to each resident batch. No-op when the
    /// row-ref list type has no visited bit.
    void AppendVisitedFlagsToResident(int32_t partitionIndex)
    {
        if constexpr (std::is_same_v<RowRefListType, RowRefListWithFlags>) {
            if (partitionIndex < 0 || static_cast<uint32_t>(partitionIndex) >= hashTableCount) {
                return;
            }
            auto &batches = inputVecBatches[partitionIndex];
            for (uint32_t i = 0; i < batches.size(); ++i) {
                if (batches[i] == nullptr) {
                    continue;
                }
                const int32_t rowCount = batches[i]->GetRowCount();
                // Vector<bool> does not zero its buffer. Unvisited rows must be false;
                // a leftover true bit makes restore skip the unmatched build row.
                auto *flags = new omniruntime::vec::Vector<bool>(rowCount);
                for (int32_t row = 0; row < rowCount; ++row) {
                    flags->SetValue(row, false);
                }
                std::unique_ptr<omniruntime::vec::BaseVector> flagCol(flags);
                FillVisitedFlagColumn(partitionIndex, i, flagCol.get());
                batches[i]->Append(flagCol.release());
            }
        }
    }

    /// Fill a pre-allocated OMNI_BOOLEAN column (default false) from HT visited bits for one
    /// resident batch index. No-op for non-flag row-ref list types.
    void FillVisitedFlagColumn(int32_t partitionIndex, uint32_t vecBatchIdx,
        omniruntime::vec::BaseVector *flagCol)
    {
        if constexpr (std::is_same_v<RowRefListType, RowRefListWithFlags>) {
            if (flagCol == nullptr || partitionIndex < 0 ||
                static_cast<uint32_t>(partitionIndex) >= hashTableCount) {
                return;
            }
            auto *boolVec = reinterpret_cast<omniruntime::vec::Vector<bool> *>(flagCol);
            const int32_t flagSize = boolVec->GetSize();
            auto writeFlags = [&](auto &value) {
                auto it = value->Begin();
                while (it.IsOk()) {
                    if (it->vecBatchIdx == vecBatchIdx &&
                        static_cast<int32_t>(it->rowIdx) < flagSize && it->visited) {
                        boolVec->SetValue(static_cast<int32_t>(it->rowIdx), true);
                    }
                    ++it;
                }
            };
            if (GetHashTableTypes(partitionIndex) == HashTableImplementationType::ARRAY_HASH_TABLE) {
                auto &arr = GetArrayTable(partitionIndex);
                if (arr) {
                    arr->ForEachValue([&](const auto &value, const auto &) { writeFlags(value); });
                }
            } else {
                auto &ht = GetHashTable(partitionIndex);
                if (ht) {
                    ht->hashmap.ForEachValue([&](const auto &value, const auto &) { writeFlags(value); });
                }
            }
        }
    }

    /// Restore visited bits from peeled spill BOOLEAN columns (parallel to resident batches).
    void ApplyVisitedFlagsFromColumns(int32_t partitionIndex,
        const std::vector<omniruntime::vec::BaseVector *> &flags)
    {
        if constexpr (std::is_same_v<RowRefListType, RowRefListWithFlags>) {
            if (flags.empty() || partitionIndex < 0 ||
                static_cast<uint32_t>(partitionIndex) >= hashTableCount) {
                return;
            }
            auto applyFlags = [&](auto &value) {
                auto it = value->Begin();
                while (it.IsOk()) {
                    if (it->vecBatchIdx < flags.size()) {
                        auto *flagCol = flags[it->vecBatchIdx];
                        if (flagCol != nullptr) {
                            auto *boolVec = reinterpret_cast<omniruntime::vec::Vector<bool> *>(flagCol);
                            if (static_cast<int32_t>(it->rowIdx) < boolVec->GetSize()) {
                                const bool visited = boolVec->GetValue(static_cast<int32_t>(it->rowIdx));
                                if (visited && !it->visited) {
                                    it->visited = true;
                                    visitedCounts++;
                                } else {
                                    it->visited = visited;
                                }
                            }
                        }
                    }
                    ++it;
                }
            };
            if (GetHashTableTypes(partitionIndex) == HashTableImplementationType::ARRAY_HASH_TABLE) {
                auto &arr = GetArrayTable(partitionIndex);
                if (arr) {
                    arr->ForEachValue([&](const auto &value, const auto &) { applyFlags(value); });
                }
            } else {
                auto &ht = GetHashTable(partitionIndex);
                if (ht) {
                    ht->hashmap.ForEachValue([&](const auto &value, const auto &) { applyFlags(value); });
                }
            }
        }
    }

    ALWAYS_INLINE JoinType GetJoinType() const
    {
        return joinType;
    }

    ALWAYS_INLINE BuildSide GetBuildSide() const
    {
        return buildSide;
    }

    ALWAYS_INLINE void SetStatus(OmniStatus newStatus)
    {
        status = newStatus;
    }

    ALWAYS_INLINE OmniStatus GetStatus() const
    {
        return status;
    }

    void BuildHashTable(int32_t partitionIndex);
    void BuildHashTableAdaptive(int32_t partitionIndex);

    /// Same open-cost as BuildHashTable + BaseHashMap::EstimateRehashAllocationFromEmpty.
    int64_t EstimateHashTableInitBytes(int32_t partitionIndex) const;
    int64_t EstimateRowRefArenaBytes(int32_t partitionIndex) const;

    void Prepare(int32_t partitionIndex);

    void InitBuildFilterCols(std::vector<int32_t> &buildFilterCols, int32_t originalProbeColsCount,
        std::vector<std::vector<BaseVector **>> &tableBuildFilterColPtrs);
    KeyType keyType;
    RowRefListType valueType;
private:
    template <typename T>
    bool TryToBuildArrayTable(uint32_t colIndex, T &min, T &max, int64_t rangeUpperBound, int32_t partitionIndex);

    template <typename T>
    void EmplaceNotNullKeyToArrayTable(T &min, T &max, int64_t rangeUpperBound, int32_t partitionIndex);

    template <typename T> void EmplaceKeyToArrayTable(T &min, T &max, int64_t rangeUpperBound, int32_t partitionIndex);

    void BuildNormalHashTableWithFixedKey(int32_t partitionIndex, uint8_t initDegree);

    void BuildNormalHashTableWithVariableKey(int32_t partitionIndex, uint8_t initDegree);

    template <typename HashTableType>
    void EmplaceFixedKey(HashTableType &hashTable, int32_t partitionIndex, VectorBatch *vecBatch, int32_t vecBatchIdx,
        BaseVector **buildVectors, int32_t buildColNum);

    template <typename HashTableType>
    void EmplaceFixedNotNullKeyToNormalHashTable(HashTableType &hashTable, int32_t partitionIndex,
        VectorBatch *vecBatch, int32_t vecBatchIdx, BaseVector **buildVectors, int32_t buildColNum);

    template <typename HashTableType>
    void EmplaceFixedNotNullKeyToNormalHashTableSimd(HashTableType &hashTable, int32_t partitionIndex,
        VectorBatch *vecBatch, int32_t vecBatchIdx, BaseVector **buildVectors, int32_t buildColNum);

    template <typename HashTableType>
    void EmplaceFixedKeyToNormalHashTable(HashTableType &hashTable, int32_t partitionIndex, VectorBatch *vecBatch,
        int32_t vecBatchIdx, BaseVector **buildVectors, int32_t buildColNum);

    template <typename HashTableType>
    void EmplaceSingleKey(HashTableType &hashTable, int32_t partitionIndex, VectorBatch *vecBatch, int32_t vecBatchIdx,
        BaseVector **buildVectors);

    template <bool isDic, typename HashTableType>
    void EmplaceSingleNotNullKeyToNormalHashTable(HashTableType &hashTable, int32_t partitionIndex,
        VectorBatch *vecBatch, int32_t vecBatchIdx, BaseVector **buildVectors);

    template <bool isDic, typename HashTableType>
    void EmplaceSingleKeyToNormalHashTable(HashTableType &hashTable, int32_t partitionIndex, VectorBatch *vecBatch,
        int32_t vecBatchIdx, BaseVector **buildVectors);

    template <typename HashTableType>
    void EmplaceMultiKey(HashTableType &hashTable, int32_t partitionIndex, VectorBatch *vecBatch, int32_t vecBatchIdx,
        BaseVector **buildVectors);

    template <typename HashTableType>
    void EmplaceMultiNotNullKeyToNormalHashTable(HashTableType &hashTable, int32_t partitionIndex,
         VectorBatch *vecBatch, int32_t vecBatchIdx, BaseVector **buildVectors);

    template <typename HashTableType>
    void EmplaceMultiKeyToNormalHashTable(HashTableType &hashTable, int32_t partitionIndex, VectorBatch *vecBatch,
         int32_t vecBatchIdx, BaseVector **buildVectors);

    template <typename HashTableType>
    void EmplaceVariableKey(HashTableType &hashTable, int32_t partitionIndex, VectorBatch *vecBatch,
        int32_t vecBatchIdx, BaseVector **buildVectors, int32_t buildColNum, std::vector<int8_t> &isNotNullKeys,
        std::vector<size_t> &hashes, std::vector<KeyType> &tryRes);

    template <typename HashTableType>
    void EmplaceVariableNotNullKeyToNormalHashTable(HashTableType &hashTable, int32_t partitionIndex,
        VectorBatch *vecBatch, int32_t vecBatchIdx, BaseVector **buildVectors, int32_t buildColNum,
        std::vector<int8_t> &isNotNullKeys, std::vector<size_t> &hashes, std::vector<KeyType> &tryRes);
    template <typename HashTableType>
    void EmplaceVariableKeyToNormalHashTable(HashTableType &hashTable, int32_t partitionIndex, VectorBatch *vecBatch,
        int32_t vecBatchIdx, BaseVector **buildVectors, int32_t buildColNum, std::vector<int8_t> &isNotNullKeys,
        std::vector<size_t> &hashes, std::vector<KeyType> &tryRes);

    uint32_t hashTableCount;            // the number of hashTables will be built
    std::atomic_uint32_t hashTableSize; // the number of hashTables having been built already
    std::vector<std::unique_ptr<JoinHashTableVariant<KeyType, RowRefListType>>> hashTables;
    std::vector<std::unique_ptr<DefaultArrayMap<RowRefListType>>> arrayTables;
    std::vector<HashTableImplementationType> hashTableTypes;
    std::vector<std::pair<int64_t, int64_t>> maxMins;
    DataTypes *probeTypes;
    DataTypes *buildTypes;
    std::vector<int32_t> &buildHashCols;
    std::vector<uint32_t> totalRowCount;
    std::vector<std::vector<omniruntime::vec::VectorBatch *>> inputVecBatches;
    std::vector<std::unique_ptr<ExecutionContext>> executionContexts;
    uint32_t visitedCounts = 0;
    uint32_t totalVisitedCounts = 0;
    std::vector<omniruntime::vec::BaseVector ***> columns; // Vector* [partitionIdx][columnIdx][vecBatchIdx]
    JoinType joinType;
    BuildSide buildSide;
    bool isMultiCols;
    bool isFixedKeys = true;
    size_t fixedKeysSize = 0;
    size_t sizeOfRowRefList = 0;
    bool isNeedNullKeyTable = false;
    bool buildStateReleased_ = false;
    OmniStatus status = OmniStatus::OMNI_STATUS_NORMAL;
};

using HashTableVariants =
    std::variant<JoinHashTableVariants<int8_t, RowRefList>, JoinHashTableVariants<int16_t, RowRefList>,
    JoinHashTableVariants<int32_t, RowRefList>, JoinHashTableVariants<int64_t, RowRefList>,
    JoinHashTableVariants<Decimal128, RowRefList>, JoinHashTableVariants<StringRef, RowRefList>,
    JoinHashTableVariants<int128_t, RowRefList>, JoinHashTableVariants<int128_t, RowRefListWithFlags>,
    JoinHashTableVariants<int8_t, RowRefListWithFlags>, JoinHashTableVariants<int16_t, RowRefListWithFlags>,
    JoinHashTableVariants<int32_t, RowRefListWithFlags>, JoinHashTableVariants<int64_t, RowRefListWithFlags>,
    JoinHashTableVariants<Decimal128, RowRefListWithFlags>, JoinHashTableVariants<StringRef, RowRefListWithFlags>>;
}
}
#endif // OMNI_RUNTIME_JOIN_HASH_TABLE_VARIANTS_H
