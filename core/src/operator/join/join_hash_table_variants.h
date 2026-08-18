/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * @Description: join hash table
 */

#ifndef OMNI_RUNTIME_JOIN_HASH_TABLE_VARIANTS_H
#define OMNI_RUNTIME_JOIN_HASH_TABLE_VARIANTS_H

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
#include "operator/join/taper_join_hash_table_variants.h"

namespace omniruntime {
namespace op {

constexpr int32_t BITS_OF_BYTE = 8;
constexpr int32_t BITS_OF_SHORT = 16;
constexpr int32_t BITS_OF_INT = 32;
constexpr int32_t BITS_OF_LONG = 64;
constexpr int32_t BITS_OF_DECIMAL = 64;
constexpr int32_t BITS_OF_LONGLONG = 128;
constexpr int32_t NOT_EXPECTED_TYPE = 0;

template <typename KeyType, typename ValueType>
using JoinHashMap =
    BaseHashMap<KeyType, ValueType, omniruntime::simdutil::HashCRC32<KeyType>, Grower, OmniHashmapAllocator>;

template <typename KeyType, typename RowRefListType>
using JoinHashTableVariant = ColumnSerializeHandler<JoinHashMap<KeyType, RowRefListType *>>;

template <typename KeyType, typename RowRefListType> class JoinHashTableVariants {
public:
    using Key = KeyType;
    using Mapped = RowRefListType;
    static constexpr bool IS_SIMPLE_KEY = (std::is_same_v<KeyType, int16_t> || std::is_same_v<KeyType, int32_t> ||
                                           std::is_same_v<KeyType, int64_t>);

    ALWAYS_INLINE bool HasDuplicates() const { return false; }

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

    ALWAYS_INLINE std::vector<int32_t>& GetBuildHashCols() { return buildHashCols; }
    ALWAYS_INLINE const std::vector<int32_t>& GetBuildHashCols() const { return buildHashCols; }

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

    // Uniform raw accessors for taper array-join dispatch (non-taper returns null; taper path never invoked).
    ALWAYS_INLINE char** GetArraySlots(int32_t /*partitionIndex*/) { return nullptr; }
    ALWAYS_INLINE bool* GetArrayAssigned(int32_t /*partitionIndex*/) { return nullptr; }

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

    ALWAYS_INLINE void IncrementVisited() { visitedCounts++; }

    void PositionVisited(ForwardIterator<RowRefListType> it)
    {
        if constexpr (std::is_same_v<RowRefListType, RowRefListWithFlags>) {
            if (!it->visited) {
                it->visited = true;
                IncrementVisited();
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

    ALWAYS_INLINE void AddTotalVisitedCounts(int cnt)
    {
        totalVisitedCounts += cnt;
    }

    ALWAYS_INLINE BaseVector ***GetColumns(int32_t partitionIndex) const
    {
        return columns[partitionIndex];
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

    void Prepare(int32_t partitionIndex);

    ALWAYS_INLINE void SetTaperStoredColumns(const std::vector<int32_t>&) {}

    ALWAYS_INLINE RowContainer* GetTaperRowContainer(int32_t) const { return nullptr; }
    ALWAYS_INLINE const std::vector<int32_t>& GetTaperStoredColIndices() const { return buildHashCols; }
    ALWAYS_INLINE char* Find(BaseVector**, int32_t, int32_t, uint32_t) { return nullptr; }
    ALWAYS_INLINE void FindBatch(int32_t, int32_t, const std::vector<int8_t>&,
        omniruntime::vec::BaseVector**, int32_t, bool, uint32_t,
        const std::vector<int64_t>&, std::vector<char*>&) const {}
    ALWAYS_INLINE bool IsSerMode() const { return false; }
    ALWAYS_INLINE void SetSerMode() {}

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
    OmniStatus status = OmniStatus::OMNI_STATUS_NORMAL;
};

using HashTableVariants =
    std::variant<JoinHashTableVariants<int8_t, RowRefList>, JoinHashTableVariants<int16_t, RowRefList>,
    JoinHashTableVariants<int32_t, RowRefList>, JoinHashTableVariants<int64_t, RowRefList>,
    JoinHashTableVariants<Decimal128, RowRefList>, JoinHashTableVariants<StringRef, RowRefList>,
    JoinHashTableVariants<int128_t, RowRefList>, JoinHashTableVariants<int128_t, RowRefListWithFlags>,
    JoinHashTableVariants<int8_t, RowRefListWithFlags>, JoinHashTableVariants<int16_t, RowRefListWithFlags>,
    JoinHashTableVariants<int32_t, RowRefListWithFlags>, JoinHashTableVariants<int64_t, RowRefListWithFlags>,
    JoinHashTableVariants<Decimal128, RowRefListWithFlags>, JoinHashTableVariants<StringRef, RowRefListWithFlags>
#ifdef OMNI_USE_TAPER_JOIN
    ,
    TaperJoinHashTableVariants<int8_t,  false>, TaperJoinHashTableVariants<int8_t,  true>,
    TaperJoinHashTableVariants<int16_t, false>, TaperJoinHashTableVariants<int16_t, true>,
    TaperJoinHashTableVariants<int32_t, false>, TaperJoinHashTableVariants<int32_t, true>,
    TaperJoinHashTableVariants<int64_t, false>, TaperJoinHashTableVariants<int64_t, true>
#endif
    >;
}
}
#endif // OMNI_RUNTIME_JOIN_HASH_TABLE_VARIANTS_H
