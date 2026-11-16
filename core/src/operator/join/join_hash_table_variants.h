/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * @Description: join hash table
 */

#ifndef OMNI_RUNTIME_JOIN_HASH_TABLE_VARIANTS_H
#define OMNI_RUNTIME_JOIN_HASH_TABLE_VARIANTS_H

#include <cstdint>
#include <variant>
#include <vector>
#include <thread>
#include <atomic>
#include <exception>
#include <unordered_map>

#include "memory/omni_task_memory_pool.h"
#include "operator/status.h"
#include "operator/hashmap/crc_hasher.h"
#include "operator/hashmap/base_hash_map.h"
#include "operator/hashmap/array_map.h"
#include "operator/hashmap/column_marshaller.h"
#include "type/data_types.h"
#include "common_join.h"
#include "row_ref.h"
#include "reader/common/Filter.h"

namespace omniruntime {
namespace op {

constexpr int32_t BITS_OF_BYTE = 8;
constexpr int32_t BITS_OF_SHORT = 16;
constexpr int32_t BITS_OF_INT = 32;
constexpr int32_t BITS_OF_LONG = 64;
constexpr int32_t BITS_OF_DECIMAL = 64;
constexpr int32_t BITS_OF_LONGLONG = 128;
constexpr int32_t NOT_EXPECTED_TYPE = 0;

// POOL-BOUND (B): JoinHashMap ctrl/slots go through OmniHashmapAllocator. Today that defaults to
// Allocator::GetAllocator() (uncovered). After task-pool SetPool, allocator must be task-pool bound so
// Build / ReclaimBuildPartition GetAllocatedBytes and pool currentBytes stay aligned.
template <typename KeyType, typename ValueType>
using JoinHashMap = BaseHashMap<KeyType, ValueType, omniruntime::simdutil::HashCRC32<KeyType>, Grower,
    OmniHashmapAllocator, nullptr, false>;

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
    static constexpr bool IS_SIMPLE_KEY = (std::is_same_v<KeyType, int8_t> || std::is_same_v<KeyType, int16_t> ||
                                           std::is_same_v<KeyType, int32_t> || std::is_same_v<KeyType, int64_t>);

    explicit JoinHashTableVariants(uint32_t hashTableCount, DataTypes *buildDataTypes,
        std::vector<int32_t> &buildHashCols, JoinType joinType, BuildSide buildSide, bool isMultiCols = false);

    ~JoinHashTableVariants();

    // Releases completed build state after the last lookup consumer reaches its terminal state.
    // Idempotent so the owning factory/destructor can run normally afterwards.
    uint64_t ReleaseBuildState();

    ALWAYS_INLINE bool IsBuildStateReleased() const
    {
        return buildStateReleased;
    }

    // Bind the per-partition ExecutionContexts to this
    // task's per-task pool so hash table arena allocations are visible to Spark
    // TaskMemoryManager. Called by HashBuilderOperator ctor (where the per-task
    // pool first becomes available). Idempotent for same pool — safe under future
    // intra-task spill where multiple HashBuilderOperators share one JHTV.
    //
    // Destruction-order safety: HBO dtor → Factory dtor → ~JHTV → POOL dtor, all
    // on the same thread/task, with the pool outliving the Factory. Therefore the implicit destruction of
    // pool-bound ExecutionContexts inside ~JHTV always sees a live pool — no
    // Detach hook needed.
    void SetPool(omniruntime::mem::OmniTaskMemoryPool *pool);

    ALWAYS_INLINE uint32_t GetHashTableCount() const
    {
        return hashTableCount;
    }

    /// Free one sub-partition's build memory (table, arena, columns, input batches).
    void ReclaimBuildPartition(int32_t partitionIndex);

    // Number of sub-partitions whose build rows are currently resident in memory (not yet reclaimed).
    // Used to assert the single-bucket restore memory invariant (peak 1 table per round).
    ALWAYS_INLINE uint32_t GetResidentBuildPartitionCount() const
    {
        uint32_t resident = 0;
        for (uint32_t partition = 0; partition < hashTableCount; ++partition) {
            if (!inputVecBatches[partition].empty()) {
                ++resident;
            }
        }
        return resident;
    }

    /// Resident input batches still held for one sub-partition (for group spill).
    ALWAYS_INLINE const std::vector<omniruntime::vec::VectorBatch *> &GetResidentInputVecBatches(
        int32_t partitionIndex) const
    {
        return inputVecBatches[static_cast<size_t>(partitionIndex)];
    }

    /// Transfer ownership of resident batches out of \p partitionIndex and zero its row count.
    /// Caller must FreeVecBatch each returned pointer; do not call ReclaimBuildPartition on the
    /// same batches afterward (that would double-free). HT/arena/columns still need reclaim.
    ALWAYS_INLINE std::vector<omniruntime::vec::VectorBatch *> TakeOwnershipOfResidentBatches(
        int32_t partitionIndex)
    {
        auto &batches = inputVecBatches[static_cast<size_t>(partitionIndex)];
        std::vector<omniruntime::vec::VectorBatch *> out;
        out.swap(batches);
        totalRowCount[static_cast<size_t>(partitionIndex)] = 0;
        return out;
    }

    /// Read-only estimate of B+C+D incremental bytes for Prepare+BuildHashTable on one partition.
    /// Does not include resident input batches (A). Used by EOB EnsureTableFits (R2).
    uint64_t EstimateBuildReservation(int32_t partitionIndex) const;

    /// Task-pool bytes that adding `vecBatch` to a distinct build can allocate: a rehash's new
    /// table, the serialized keys of every input row, and the compact copy of the kept rows.
    /// 0 for a build that only appends its input.
    uint64_t EstimateDistinctInputGrowth(int32_t partitionIndex, const omniruntime::vec::VectorBatch *vecBatch) const;

    /// Bytes of a distinct build's growing table and its arena, which ReclaimBuildPartition frees
    /// with the resident batches. 0 otherwise.
    uint64_t IncrementalTableBytes(int32_t partitionIndex) const;

    /// Approximate bytes of hash-table / array-map buffers (reclaim estimate block B).
    ALWAYS_INLINE uint64_t EstimateHashTableBytes() const
    {
        uint64_t bytes = 0;
        for (uint32_t partition = 0; partition < hashTableCount; ++partition) {
            if (hashTables[partition] != nullptr) {
                bytes += hashTables[partition]->GetAllocatedBytes();
            }
            if (arrayTables[partition] != nullptr) {
                bytes += arrayTables[partition]->GetAllocatedBytes();
            }
        }
        return bytes;
    }

    /// Approximate bytes used by per-partition RowRefList arenas (reclaim estimate block C).
    ALWAYS_INLINE uint64_t EstimateArenaBytes() const
    {
        uint64_t bytes = 0;
        for (uint32_t partition = 0; partition < hashTableCount; ++partition) {
            if (executionContexts[partition] != nullptr) {
                bytes += executionContexts[partition]->GetArena()->UsedBytes();
            }
        }
        return bytes;
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

    // POOL-BOUND (A): takes ownership of resident build VectorBatch*; payload must already be
    // (or become) charged to the task pool. Pair with FreeVecBatches in ReclaimBuildPartition.
    ALWAYS_INLINE void AddVecBatch(int32_t partitionIndex, omniruntime::vec::VectorBatch *vecBatch)
    {
        if (buildStateReleased) {
            throw omniruntime::exception::OmniException("OPERATOR_RUNTIME_ERROR",
                "Cannot add build input after releasing join build state");
        }
        if (distinctBuild_ && !distinctAbandoned_) {
            AddVecBatchDistinct(partitionIndex, vecBatch);
            return;
        }
        inputVecBatches[partitionIndex].emplace_back(vecBatch);
        totalRowCount[partitionIndex] += vecBatch->GetRowCount();
    }

    /// Existence-only joins (semi/anti/existence without a join filter) need one build row per key.
    /// In this mode each added batch is inserted into a growing table immediately and only the rows
    /// that introduced a new key are retained; duplicate and null-key rows are freed on arrival.
    void SetDistinctBuild(bool enabled)
    {
        distinctBuild_ = enabled;
    }

    ALWAYS_INLINE bool IsDistinctBuild() const
    {
        return distinctBuild_ && !distinctAbandoned_;
    }

    ALWAYS_INLINE uint32_t GetTotalRowCount(int32_t partitionIndex) const
    {
        return totalRowCount[static_cast<size_t>(partitionIndex)];
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

    ALWAYS_INLINE const std::vector<int32_t> &GetBuildHashCols() const
    {
        return buildHashCols;
    }

    void BuildDynamicFilters();

    const std::unordered_map<int32_t, ::common::FilterPtr> &GetDynamicFilters() const
    {
        return dynamicFilters_;
    }

    ALWAYS_INLINE bool HasDuplicateKeys() const
    {
        return hasDuplicateKeys_;
    }

    // Persist visited bits when Probe reclaims a held FULL/same-side outer table.
    // The trailing column is peeled before the restored table is built again.
    void AppendVisitedFlagsToResident(int32_t partitionIndex)
    {
        if constexpr (std::is_same_v<RowRefListType, RowRefListWithFlags>) {
            if (partitionIndex < 0 || static_cast<uint32_t>(partitionIndex) >= hashTableCount) {
                return;
            }
            auto &batches = inputVecBatches[partitionIndex];
            std::vector<std::unique_ptr<omniruntime::vec::Vector<bool>>> flags(batches.size());
            for (size_t i = 0; i < batches.size(); ++i) {
                if (batches[i] == nullptr) {
                    continue;
                }
                flags[i] = std::make_unique<omniruntime::vec::Vector<bool>>(
                    batches[i]->GetRowCount(), pool_);
                for (int32_t row = 0; row < flags[i]->GetSize(); ++row) {
                    flags[i]->SetValue(row, false);
                }
            }
            // Traverse row references once for all batches. Scanning the entire
            // table separately for each batch makes pressure-driven spill quadratic.
            auto writeFlags = [&](const auto &value) {
                auto it = value->Begin();
                while (it.IsOk()) {
                    if (it->visited && it->vecBatchIdx < flags.size()) {
                        auto *flag = flags[it->vecBatchIdx].get();
                        if (flag != nullptr && it->rowIdx < static_cast<uint32_t>(flag->GetSize())) {
                            flag->SetValue(it->rowIdx, true);
                        }
                    }
                    ++it;
                }
            };
            if (GetHashTableTypes(partitionIndex) == HashTableImplementationType::ARRAY_HASH_TABLE) {
                auto &table = GetArrayTable(partitionIndex);
                if (table) {
                    table->ForEachValue([&](const auto &value, const auto &) { writeFlags(value); });
                }
            } else {
                auto &table = GetHashTable(partitionIndex);
                if (table) {
                    table->hashmap.ForEachValue([&](const auto &value, const auto &) { writeFlags(value); });
                }
            }
            size_t appendedThrough = 0;
            try {
                for (; appendedThrough < batches.size(); ++appendedThrough) {
                    if (flags[appendedThrough] != nullptr) {
                        batches[appendedThrough]->Append(flags[appendedThrough].get());
                    }
                }
            } catch (...) {
                // Append may allocate pointer metadata. Keep flag ownership
                // until all batches commit, and undo earlier appends on failure.
                for (size_t i = 0; i < appendedThrough; ++i) {
                    if (flags[i] != nullptr) {
                        batches[i]->ResizeVectorCount(batches[i]->GetVectorCount() - 1);
                    }
                }
                throw;
            }
            for (auto &flag : flags) {
                flag.release();
            }
        }
    }

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
            // The flag vector is newly allocated and its value buffer is not initialized.
            // Every unvisited build row must be written as false before setting matches.
            for (int32_t row = 0; row < flagSize; ++row) {
                boolVec->SetValue(row, false);
            }
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

    /// Velox-style parallel build: single hash table, bucket-range partitioning, no probe-side changes.
    /// Returns true if parallel build was applied; false to fall back to serial BuildHashTable().
    bool TryBuildHashTableParallel(int32_t partitionIndex, uint32_t numThreads, uint32_t minRowsPerPartition);

    uint32_t GetPartitionRowCount(int32_t partitionIndex) const
    {
        return totalRowCount[partitionIndex];
    }

    /// Build a normal hash table from the minimum capacity and grow with observed distinct keys.
    /// Used only as the restore fallback when the row-count-sized table reservation is denied.
    void BuildHashTableAdaptive(int32_t partitionIndex);

    void Prepare(int32_t partitionIndex);

    void InitBuildFilterCols(std::vector<int32_t> &buildFilterCols, int32_t originalProbeColsCount,
        std::vector<std::vector<BaseVector **>> &tableBuildFilterColPtrs);
    KeyType keyType;
    RowRefListType valueType;
private:
    template <typename T>
    bool TryToBuildArrayTable(uint32_t colIndex, T &min, T &max, int64_t rangeUpperBound, int32_t partitionIndex);

    template <typename T>
    bool IsArrayTableEligible(uint32_t colIndex, int64_t rangeUpperBound, int32_t partitionIndex);

    template <typename T>
    void EmplaceNotNullKeyToArrayTable(T &min, T &max, int64_t rangeUpperBound, int32_t partitionIndex);

    template <typename T> void EmplaceKeyToArrayTable(T &min, T &max, int64_t rangeUpperBound, int32_t partitionIndex);

    void AddVecBatchDistinct(int32_t partitionIndex, omniruntime::vec::VectorBatch *vecBatch);

    /// Emplace one batch into an existing table; shared by full builds and the distinct build.
    template <bool kGrowing>
    void EmplaceBatch(int32_t partitionIndex, VectorBatch *vecBatch, int32_t vecBatchIdx);

    static uint8_t AdaptiveInitDegree();

    static uint8_t NormalTableDegree(uint32_t rows);

    void AbandonIncrementalTables();

    /// A table built by AddVecBatchDistinct is complete once input ends; full builds reuse it.
    bool FinishIncrementalTable(int32_t partitionIndex);

    ALWAYS_INLINE void ApplyRowRef(InsertResult<RowRefListType *> &ret, uint32_t rowIdx, uint32_t vecBatchIdx,
        omniruntime::mem::SimpleArenaAllocator &arenaAllocator)
    {
        if (UNLIKELY(distinctBuild_)) {
            // Existence-only probes test presence and never read build rows: every key shares one
            // row reference instead of allocating a RowRefList per key.
            if (ret.IsInsert()) {
                ret.SetValue(&existenceRowRef_);
                if (collectDistinct_) {
                    distinctRows_.push_back(static_cast<int32_t>(rowIdx));
                }
            }
            return;
        }
        if (ret.IsInsert()) {
            auto *rowRef = reinterpret_cast<RowRefListType *>(arenaAllocator.Allocate(sizeOfRowRefList));
            *rowRef = RowRefListType(rowIdx, vecBatchIdx);
            ret.SetValue(rowRef);
        } else {
            ret.GetValue()->Insert({ rowIdx, vecBatchIdx }, arenaAllocator);
        }
    }

    template <bool kGrowing>
    void BuildNormalHashTableWithFixedKey(int32_t partitionIndex, uint8_t initDegree);

    template <bool kGrowing>
    void BuildNormalHashTableWithVariableKey(int32_t partitionIndex, uint8_t initDegree);

    // Insert dispatch: kGrowing selects the adaptive grow-on-insert hashmap entry point.
    // Pre-sized builds keep the check-free fast path (kGrowing == false).
    template <bool kGrowing, typename HashTableType, typename K>
    static ALWAYS_INLINE auto EmplaceJoinKey(HashTableType &hashTable, K &key)
    {
        if constexpr (kGrowing) {
            return hashTable->InsertJoinKeysToHashmapGrowing(key);
        } else {
            return hashTable->InsertJoinKeysToHashmap(key);
        }
    }

    template <bool kGrowing, typename HashTableType, typename K>
    static ALWAYS_INLINE auto EmplaceJoinKey(HashTableType &hashTable, K &key, size_t &hashValue)
    {
        if constexpr (kGrowing) {
            return hashTable->InsertJoinKeysToHashmapGrowing(key, hashValue);
        } else {
            return hashTable->InsertJoinKeysToHashmap(key, hashValue);
        }
    }

    template <typename HashTableType, bool kGrowing>
    void EmplaceFixedKey(HashTableType &hashTable, int32_t partitionIndex, VectorBatch *vecBatch, int32_t vecBatchIdx,
        BaseVector **buildVectors, int32_t buildColNum);

    template <typename HashTableType, bool kGrowing>
    void EmplaceFixedNotNullKeyToNormalHashTable(HashTableType &hashTable, int32_t partitionIndex,
        VectorBatch *vecBatch, int32_t vecBatchIdx, BaseVector **buildVectors, int32_t buildColNum);

    template <typename HashTableType>
    void EmplaceFixedNotNullKeyToNormalHashTableSimd(HashTableType &hashTable, int32_t partitionIndex,
        VectorBatch *vecBatch, int32_t vecBatchIdx, BaseVector **buildVectors, int32_t buildColNum);

    template <typename HashTableType, bool kGrowing>
    void EmplaceFixedKeyToNormalHashTable(HashTableType &hashTable, int32_t partitionIndex, VectorBatch *vecBatch,
        int32_t vecBatchIdx, BaseVector **buildVectors, int32_t buildColNum);

    template <typename HashTableType, bool kGrowing>
    void EmplaceSingleKey(HashTableType &hashTable, int32_t partitionIndex, VectorBatch *vecBatch, int32_t vecBatchIdx,
        BaseVector **buildVectors);

    template <bool isDic, typename HashTableType, bool kGrowing>
    void EmplaceSingleNotNullKeyToNormalHashTable(HashTableType &hashTable, int32_t partitionIndex,
        VectorBatch *vecBatch, int32_t vecBatchIdx, BaseVector **buildVectors);

    template <bool isDic, typename HashTableType, bool kGrowing>
    void EmplaceSingleKeyToNormalHashTable(HashTableType &hashTable, int32_t partitionIndex, VectorBatch *vecBatch,
        int32_t vecBatchIdx, BaseVector **buildVectors);

    template <typename HashTableType, bool kGrowing>
    void EmplaceMultiKey(HashTableType &hashTable, int32_t partitionIndex, VectorBatch *vecBatch, int32_t vecBatchIdx,
        BaseVector **buildVectors);

    template <typename HashTableType, bool kGrowing>
    void EmplaceMultiNotNullKeyToNormalHashTable(HashTableType &hashTable, int32_t partitionIndex,
         VectorBatch *vecBatch, int32_t vecBatchIdx, BaseVector **buildVectors);

    template <typename HashTableType, bool kGrowing>
    void EmplaceMultiKeyToNormalHashTable(HashTableType &hashTable, int32_t partitionIndex, VectorBatch *vecBatch,
         int32_t vecBatchIdx, BaseVector **buildVectors);

    template <typename HashTableType, bool kGrowing>
    void EmplaceVariableKey(HashTableType &hashTable, int32_t partitionIndex, VectorBatch *vecBatch,
        int32_t vecBatchIdx, BaseVector **buildVectors, int32_t buildColNum, std::vector<int8_t> &isNotNullKeys,
        std::vector<size_t> &hashes, std::vector<KeyType> &tryRes);

    template <typename HashTableType, bool kGrowing>
    void EmplaceVariableNotNullKeyToNormalHashTable(HashTableType &hashTable, int32_t partitionIndex,
        VectorBatch *vecBatch, int32_t vecBatchIdx, BaseVector **buildVectors, int32_t buildColNum,
        std::vector<int8_t> &isNotNullKeys, std::vector<size_t> &hashes, std::vector<KeyType> &tryRes);

    template <typename HashTableType, bool kGrowing>
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
    // POOL-BOUND (A): resident build column payloads until reclaim / handoff clear.
    std::vector<std::vector<omniruntime::vec::VectorBatch *>> inputVecBatches;
    // POOL-BOUND (C): per-SP arena for RowRefList + serialized join keys. Must use a pool-bound
    // Allocator (via SetPool / ExecutionContext(pool)); default GetAllocator() is uncovered.
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
    std::unordered_map<int32_t, ::common::FilterPtr> dynamicFilters_;
    bool dynamicFiltersBuilt_ = false;
    bool hasDuplicateKeys_ = false;
    bool buildStateReleased = false;
    bool distinctBuild_ = false;
    // Set once the incremental dedup stops paying for itself (see AddVecBatchDistinct). Duplicate
    // row-reference chains are still skipped afterwards; only the per-batch dedup stops.
    bool distinctAbandoned_ = false;
    uint64_t distinctInputRows_ = 0;
    uint64_t distinctKeptRows_ = 0;
    // Per partition: 1 while hashTables[p] was grown by AddVecBatchDistinct and is not yet counted.
    std::vector<uint8_t> incrementalTable_;
    // True only while AddVecBatchDistinct emplaces one batch.
    bool collectDistinct_ = false;
    std::vector<int32_t> distinctRows_;
    RowRefListType existenceRowRef_{0, 0};
    // Per-task pool bound at HBO ctor via SetPool;
    // remembered for idempotency check on repeated SetPool calls (future spill
    // multi-op case where N HBOs share one JHTV with the same task pool).
    omniruntime::mem::OmniTaskMemoryPool *pool_ = nullptr;
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
