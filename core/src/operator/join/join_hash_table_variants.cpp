/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2023-2024. All rights reserved.
 * @Description: join hash table
 */
#include "join_hash_table_variants.h"
#include "join_spill_state.h"
#include "operator/util/operator_util.h"
#include "type/data_type.h"
#include "util/debug.h"
#include "util/omni_exception.h"
#include "util/global_log.h"
#include "util/telemetry.h"

#include <atomic>
#include <algorithm>
#include <cmath>
#include "util/bit_util.h"
#include <cstdint>
#include <iostream>

namespace omniruntime {
namespace op {
static constexpr uint32_t ARRAY_THRESHOLD = 8;
static constexpr double LOAD_FACTOR = 0.75;
static constexpr uint32_t BLOCK_SIZE = 1024;
static constexpr uint8_t MIN_DEGREE = 5;

template <typename KeyType, typename RowRefListType>
JoinHashTableVariants<KeyType, RowRefListType>::JoinHashTableVariants(uint32_t hashTableCount,
    DataTypes *buildDataTypes, std::vector<int32_t> &buildHashCols,
    JoinType joinType, BuildSide buildSide, bool isMultiCols)
    : hashTableCount(hashTableCount),
      hashTableSize(0),
      probeTypes(nullptr),
      buildTypes(buildDataTypes),
      buildHashCols(buildHashCols),
      totalRowCount(std::vector<uint32_t>(hashTableCount, 0)),
      inputVecBatches(std::vector<std::vector<omniruntime::vec::VectorBatch *>>(hashTableCount)),
      columns(std::vector<omniruntime::vec::BaseVector ***>(hashTableCount)),
      joinType(joinType),
      buildSide(buildSide),
      isMultiCols(isMultiCols)
{
    hashTables = std::vector<std::unique_ptr<JoinHashTableVariant<KeyType, RowRefListType>>>(hashTableCount);
    arrayTables = std::vector<std::unique_ptr<DefaultArrayMap<RowRefListType>>>(hashTableCount);
    hashTableTypes = std::vector<HashTableImplementationType>(hashTableCount);
    maxMins = std::vector<std::pair<int64_t, int64_t>>(hashTableCount);
    incrementalTable_ = std::vector<uint8_t>(hashTableCount, 0);
    sizeOfRowRefList = sizeof(RowRefListType);
    // POOL-BOUND (C): no-arg ExecutionContext => SimpleArenaAllocator uses Allocator::GetAllocator().
    // Task-pool SetPool must rebuild these with the per-task pool allocator before BuildHashTable.
    for (uint32_t i = 0; i < hashTableCount; ++i) {
        // JHTV is constructed inside HashBuilderOperatorFactory before any
        // HashBuilderOperator (and thus before any per-task pool) exists.
        // Start with nullptr-pool ExecutionContexts; HashBuilderOperator::HashBuilderOperator
        // calls SetPool() which rebuilds these with the real per-task pool so hash table
        // arena allocations are visible to Spark TaskMemoryManager.
        executionContexts.emplace_back(std::make_unique<ExecutionContext>(4096, nullptr));
    }

    for (const int32_t buildHashCol : buildHashCols) {
        auto type = buildTypes->GetIds()[buildHashCol];
        if (type == OMNI_VARCHAR || type == OMNI_CHAR || type == OMNI_VARBINARY || type == OMNI_ARRAY ||
            type == OMNI_ROW) {
            isFixedKeys = false;
            break;
        }
    }

    if (isFixedKeys) {
        for (const int32_t buildHashCol : buildHashCols) {
            auto type = buildTypes->GetIds()[buildHashCol];
            switch (type) {
                case OMNI_BYTE:
                    fixedKeysSize += OperatorUtil::SIZE_OF_BYTE;
                    break;
                case OMNI_INT:
                case OMNI_DATE32:
                    fixedKeysSize += OperatorUtil::SIZE_OF_INT;
                    break;
                case OMNI_LONG:
                case OMNI_TIMESTAMP:
                case OMNI_DECIMAL64:
                case OMNI_DOUBLE:
                case OMNI_DATE64:
                    fixedKeysSize += OperatorUtil::SIZE_OF_LONG;
                    break;
                case OMNI_BOOLEAN:
                    fixedKeysSize += OperatorUtil::SIZE_OF_BOOL;
                    break;
                case OMNI_SHORT:
                    fixedKeysSize += OperatorUtil::SIZE_OF_SHORT;
                    break;
                case OMNI_DECIMAL128:
                    fixedKeysSize += OperatorUtil::SIZE_OF_DECIMAL128;
                    break;
                default:
                    break;
            }
        }
    }
    isNeedNullKeyTable = joinType == OMNI_JOIN_TYPE_FULL
            || (joinType == OMNI_JOIN_TYPE_LEFT && buildSide == OMNI_BUILD_LEFT)
            || (joinType == OMNI_JOIN_TYPE_RIGHT && buildSide == OMNI_BUILD_RIGHT);
}

template <typename KeyType, typename RowRefListType>
JoinHashTableVariants<KeyType, RowRefListType>::~JoinHashTableVariants()
{
    // Destroy the hash tables BEFORE the per-partition ExecutionContexts they were
    // allocated from. Each hashmap holds an OmniHashmapAllocator pointing into
    // executionContexts[i]->bridge_allocator_ (which stores taskPool_); ~BaseHashMap
    // frees its buckets via that allocator -> taskPool_->SubMemory. Because hashTables /
    // arrayTables are declared BEFORE executionContexts, implicit reverse-order member
    // destruction would free the ExecutionContexts first, leaving the hashmap frees to
    // dereference a dangling allocator (use-after-free; SIGSEGV under memory pressure
    // once the freed slot is reused -> garbage taskPool_). Clear them here, in the dtor
    // body that runs before implicit member destruction, so bucket frees route through
    // still-live ExecutionContexts / pool.
    ReleaseBuildState();
}

template <typename KeyType, typename RowRefListType>
uint64_t JoinHashTableVariants<KeyType, RowRefListType>::ReleaseBuildState()
{
    if (buildStateReleased) {
        return 0;
    }

    const int64_t poolBytesBefore = pool_ == nullptr ? 0 : pool_->currentBytes();

    // Hash maps retain allocators owned by executionContexts. Destroy maps
    // first, then their pointer metadata and backing build batches, and only
    // then the arenas/allocators themselves.
    // Keep partition slots valid for terminal-state observers after release.
    for (auto &table : hashTables) {
        table.reset();
    }
    for (auto &table : arrayTables) {
        table.reset();
    }
    for (auto &column : columns) {
        if (column != nullptr) {
            for (auto colIdx = 0; colIdx < buildTypes->GetSize(); ++colIdx) {
                delete[] column[colIdx];
            }
            delete[] column;
            column = nullptr;
        }
    }

    for (auto &inputVecBatch : inputVecBatches) {
        VectorHelper::FreeVecBatches(inputVecBatch);
        inputVecBatch.clear();
    }
    std::fill(totalRowCount.begin(), totalRowCount.end(), 0);
    for (auto &context : executionContexts) {
        context.reset();
    }
    hashTableSize.store(0, std::memory_order_release);
    buildStateReleased = true;

    const int64_t poolBytesAfter = pool_ == nullptr ? 0 : pool_->currentBytes();
    return poolBytesBefore > poolBytesAfter ? static_cast<uint64_t>(poolBytesBefore - poolBytesAfter) : 0;
}

template <typename KeyType, typename RowRefListType>
void JoinHashTableVariants<KeyType, RowRefListType>::ReclaimBuildPartition(int32_t partitionIndex)
{
    if (buildStateReleased) {
        return;
    }
    if (partitionIndex < 0 || static_cast<uint32_t>(partitionIndex) >= hashTableCount) {
        return;
    }
    // Free table → arena → column pointers → input batches (same order as a single-slot destructor).
    // POOL-BOUND: B reset, C ExecutionContext replacement, A FreeVecBatches must each SubMemory on the task pool
    // (same Allocator that charged them). Otherwise reclaimableBytes estimate and pool delta diverge.
    const bool verbose = UNLIKELY(omniruntime::telemetry::IsReclaimVerboseEnabled());
    int64_t poolUsedBefore = 0;
    int64_t poolReservedBefore = 0;
    uint32_t rowsBefore = 0;
    uint64_t tableBytesBefore = 0;
    uint64_t arenaUsedBefore = 0;
    if (verbose) {
        poolUsedBefore = pool_ == nullptr ? 0 : pool_->currentBytes();
        poolReservedBefore = pool_ == nullptr ? 0 : pool_->reservedBytes();
        rowsBefore = totalRowCount[partitionIndex];
        if (hashTables[partitionIndex] != nullptr) {
            tableBytesBefore += hashTables[partitionIndex]->GetAllocatedBytes();
        }
        if (arrayTables[partitionIndex] != nullptr) {
            tableBytesBefore += arrayTables[partitionIndex]->GetAllocatedBytes();
        }
        arenaUsedBefore = executionContexts[partitionIndex] == nullptr
            ? 0
            : executionContexts[partitionIndex]->GetArena()->UsedBytes();
        std::cerr << "[OmniRuntime][JoinSpill] reclaim_clear"
                  << " variants=" << static_cast<const void *>(this)
                  << " partition=" << partitionIndex
                  << " htBefore=" << hashTables[partitionIndex].get()
                  << " arrBefore=" << arrayTables[partitionIndex].get()
                  << " type=" << static_cast<int>(hashTableTypes[partitionIndex])
                  << " released=" << (buildStateReleased ? 1 : 0) << "\n";
        std::cerr.flush();
    }
    const bool hadTable = (hashTables[partitionIndex] != nullptr && !incrementalTable_[partitionIndex]) ||
        arrayTables[partitionIndex] != nullptr;
    incrementalTable_[partitionIndex] = 0;
    hashTables[partitionIndex].reset();
    arrayTables[partitionIndex].reset();
    if (hadTable && hashTableSize > 0) {
        hashTableSize--;
    }
    hashTableTypes[partitionIndex] = HashTableImplementationType::NORMAL_HASH_TABLE;
    executionContexts[partitionIndex] = std::make_unique<ExecutionContext>(4096, pool_);
    auto &column = columns[partitionIndex];
    if (column != nullptr) {
        for (auto colIdx = 0; colIdx < buildTypes->GetSize(); ++colIdx) {
            delete[] column[colIdx];
        }
        delete[] column;
        column = nullptr;
    }
    VectorHelper::FreeVecBatches(inputVecBatches[partitionIndex]);
    inputVecBatches[partitionIndex].clear();
    totalRowCount[partitionIndex] = 0;
    // Visited row refs belonged to the table just released. A later restore
    // reapplies their persisted flags and must count them exactly once.
    visitedCounts = 0;
    if (verbose) {
        const int64_t poolUsedAfter = pool_ == nullptr ? 0 : pool_->currentBytes();
        std::fprintf(stderr,
            "[OmniRuntime][JoinSpill][reclaim-partition] partition=%d rows=%u "
            "table_bytes=%lu arena_used=%lu pool_used_before=%ld pool_used_after=%ld "
            "pool_freed=%ld pool_reserved_before=%ld pool_reserved_after=%ld\n",
            partitionIndex, rowsBefore, static_cast<unsigned long>(tableBytesBefore),
            static_cast<unsigned long>(arenaUsedBefore), static_cast<long>(poolUsedBefore),
            static_cast<long>(poolUsedAfter),
            static_cast<long>(std::max<int64_t>(0, poolUsedBefore - poolUsedAfter)),
            static_cast<long>(poolReservedBefore),
            static_cast<long>(pool_ == nullptr ? 0 : pool_->reservedBytes()));
        std::fflush(stderr);
    }
}

template <typename KeyType, typename RowRefListType>
uint64_t JoinHashTableVariants<KeyType, RowRefListType>::EstimateBuildReservation(
    int32_t partitionIndex) const
{
    if (partitionIndex < 0 || static_cast<uint32_t>(partitionIndex) >= hashTableCount) {
        return 0;
    }
    const uint32_t rows = totalRowCount[static_cast<size_t>(partitionIndex)];
    if (rows == 0) {
        return 0;
    }
    if (incrementalTable_[static_cast<size_t>(partitionIndex)]) {
        // The distinct build already owns its table and row references; only D remains.
        const uint32_t columnCount = buildTypes != nullptr ? static_cast<uint32_t>(buildTypes->GetSize()) : 0;
        const auto batchCount = static_cast<uint64_t>(inputVecBatches[static_cast<size_t>(partitionIndex)].size());
        return static_cast<uint64_t>(columnCount) * (sizeof(BaseVector **) + batchCount * sizeof(BaseVector *));
    }

    // B: normal-hash ctrl+slots, sized exactly as BuildHashTable sizes it.
    const uint8_t degree = NormalTableDegree(rows);
    const uint64_t capacity = 1ULL << degree;
    using SlotT = HashSlot<KeyType, RowRefListType *>;
    constexpr uint64_t kGroupWidth = 16;
    uint64_t blockB =
        capacity * sizeof(SlotT) + (capacity + kGroupWidth) * sizeof(signed char);

    // C: RowRefList arena entries + conservative variable-key materialization.
    // Fixed-width compound keys need no separate entry here: they are packed on the stack and
    // copied into HashSlot::kv.first, whose inline storage is already included in block B.
    // A single fixed key with no null-key table never creates RowRefList entries
    // for null keys. Keep B's inherited table sizing and the retained input rows
    // unchanged; count only rows that the existing Emplace path can reference.
    // Multi-key and variable-key cases keep their conservative upper bound.
    uint64_t rowRefRows = rows;
    if (isFixedKeys && !isNeedNullKeyTable && buildHashCols.size() == 1) {
        rowRefRows = 0;
        for (auto *batch : inputVecBatches[static_cast<size_t>(partitionIndex)]) {
            if (batch == nullptr || batch->GetRowCount() <= 0) {
                continue;
            }
            auto *key = batch->Get(buildHashCols[0]);
            const int32_t batchRows = batch->GetRowCount();
            const int32_t nullRows = key->HasNull()
                ? BitUtil::CountBits(key->GetNullsBuffer()->GetNulls(), 0, batchRows)
                : 0;
            rowRefRows += static_cast<uint64_t>(batchRows - nullRows);
        }
    }
    uint64_t blockC = rowRefRows * static_cast<uint64_t>(sizeOfRowRefList);
    if constexpr (std::is_same_v<KeyType, type::StringRef>) {
        uint64_t keyPayload = 0;
        for (auto *batch : inputVecBatches[static_cast<size_t>(partitionIndex)]) {
            if (batch == nullptr) {
                continue;
            }
            for (int32_t hashCol : buildHashCols) {
                auto *col = batch->Get(hashCol);
                if (col == nullptr) {
                    continue;
                }
                keyPayload += static_cast<uint64_t>(std::max(0, col->GetSize())) * 16ULL;
            }
        }
        blockC += keyPayload;
    }
    if (executionContexts[static_cast<size_t>(partitionIndex)] != nullptr) {
        auto *arena = executionContexts[static_cast<size_t>(partitionIndex)]->GetArena();
        if (arena != nullptr) {
            const uint64_t avail = arena->AvailBytes();
            blockC = blockC > avail ? blockC - avail : 0;
        }
    }

    // D: Prepare() column × batch pointer directory.
    const uint32_t columnCount =
        buildTypes != nullptr ? static_cast<uint32_t>(buildTypes->GetSize()) : 0;
    const uint32_t batchCount =
        static_cast<uint32_t>(inputVecBatches[static_cast<size_t>(partitionIndex)].size());
    const uint64_t blockD =
        static_cast<uint64_t>(columnCount) * sizeof(BaseVector **) +
        static_cast<uint64_t>(columnCount) * static_cast<uint64_t>(batchCount) * sizeof(BaseVector *);

    return blockB + blockC + blockD;
}

// SetPool: bind ExecutionContexts to the per-task pool.
//
// JHTV is constructed inside HashBuilderOperatorFactory (long-lived, query-scope)
// before any HashBuilderOperator (per-task) exists. The ctor seeds executionContexts
// with nullptr-pool ExecutionContexts; SetPool replaces them with pool-bound ones so
// hash table arena allocations route through OmniTaskMemoryPool → Spark TMM.
//
// Idempotent: same-pool calls are no-ops (covers the intra-task spill multi-op
// case where multiple HashBuilderOperators share one JHTV with the same task pool).
//
// Lifetime safety: destruction order is HBO dtor → Factory dtor → JHTV dtor →
// POOL dtor, all on the same thread and task, so the pool outlives the Factory.
// The implicit member
// destruction of executionContexts inside ~JHTV sees a live pool.
template <typename KeyType, typename RowRefListType>
void JoinHashTableVariants<KeyType, RowRefListType>::SetPool(
    omniruntime::mem::OmniTaskMemoryPool *pool)
{
    if (pool == nullptr || pool == pool_) {
        return;
    }
    // A non-null pool that differs from an already-bound non-null pool is a contract
    // violation: JHTV is task-scoped and must never be rebound across pools. Silently
    // rebinding would free arena memory already accounted to the old pool against the new
    // one (cross-pool free) and corrupt both pools' accounting. Fail loudly instead.
    if (pool_ != nullptr) {
        throw omniruntime::exception::OmniException("OPERATOR_RUNTIME_ERROR",
            "JoinHashTableVariants::SetPool called with a different pool; JHTV is task-scoped "
            "and must not be rebound across pools");
    }
    pool_ = pool;
    executionContexts.clear();
    executionContexts.reserve(hashTableCount);
    for (uint32_t i = 0; i < hashTableCount; ++i) {
        executionContexts.emplace_back(std::make_unique<ExecutionContext>(4096, pool));
    }
}

template <typename T>
static inline T GetVectorValue(BaseVector *vec, int32_t index)
{
    if (vec->GetEncoding() == OMNI_ENCODING_CONST) {
        return static_cast<ConstVector<T> *>(vec)->GetConstValue();
    } else if (vec->GetEncoding() == OMNI_DICTIONARY) {
        return reinterpret_cast<Vector<DictionaryContainer<T>> *>(vec)->GetValue(index);
    } else {
        return reinterpret_cast<Vector<T> *>(vec)->GetValue(index);
    }
}

template <typename KeyType, typename RowRefListType>
void JoinHashTableVariants<KeyType, RowRefListType>::ComputeMultiColKey(
    BaseVector **hashColumns, int32_t hashColCount, int32_t index, KeyType& key)
{
    if constexpr (std::is_same_v<KeyType, int32_t> ||
                  std::is_same_v<KeyType, int64_t> || std::is_same_v<KeyType, int128_t>) {
        for (int i = 0; i < hashColCount; ++i) {
            switch (hashColumns[i]->GetTypeId()) {
                case OMNI_BOOLEAN:
                case OMNI_BYTE:
                    key = static_cast<KeyType>(key) << BITS_OF_BYTE | static_cast<KeyType>(
                        GetVectorValue<int8_t>(hashColumns[i], index));
                    break;
                case OMNI_SHORT:
                    key = static_cast<KeyType>(key) << BITS_OF_SHORT | static_cast<KeyType>(
                        GetVectorValue<int16_t>(hashColumns[i], index));
                    break;
                case OMNI_INT:
                case OMNI_DATE32:
                case OMNI_FLOAT:
                    key = static_cast<KeyType>(key) << BITS_OF_INT | static_cast<KeyType>(
                        GetVectorValue<int32_t>(hashColumns[i], index));
                    break;
                case OMNI_LONG:
                case OMNI_TIMESTAMP:
                case OMNI_DECIMAL64:
                case OMNI_DOUBLE:
                case OMNI_DATE64:
                    key = static_cast<KeyType>(key) << BITS_OF_LONG | static_cast<KeyType>(
                        GetVectorValue<int64_t>(hashColumns[i], index));
                    break;
                default:
                    std::string omniExceptionInfo =
                        "Error in computing multiCol key, not support type: " +
                        std::to_string(hashColumns[i]->GetTypeId());
                    throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", omniExceptionInfo);
            }
        }
    }
}

#ifdef OMNI_JOIN_PROBE_VALIDATION
namespace {
__attribute__((cold, noinline, noreturn))
void ThrowProbeTableValidationError(int reason, const char *path, uint32_t partition, const void *usePtr)
{
    if (reason == 0) {
        throw omniruntime::exception::OmniException("JOIN_SPILL_RELEASED_HT",
            std::string("Find on released JoinHashTableVariants path=") + path +
                " partition=" + std::to_string(partition));
    }
    if (reason == 1) {
        throw omniruntime::exception::OmniException("JOIN_SPILL_NULL_HT",
            "Find partition out of range: " + std::to_string(partition));
    }
    throw omniruntime::exception::OmniException("JOIN_SPILL_NULL_HT",
        std::string("Find on invalid table path=") + path + " partition=" + std::to_string(partition) +
            " usePtr=" + std::to_string(reinterpret_cast<uintptr_t>(usePtr)));
}
} // namespace
#endif

template <typename KeyType, typename RowRefListType>
InsertResult<RowRefListType *> JoinHashTableVariants<KeyType, RowRefListType>::Find(
    std::vector<VectorSerializerIgnoreNull> &probeSerializers, ExecutionContext *probeArena,
    BaseVector **probeHashColumns, int32_t probeHashColCount, int32_t probePosition, uint32_t partition)
{
#ifdef OMNI_JOIN_PROBE_VALIDATION
    // The owner must keep this object alive. These checks validate its state,
    // not the lifetime of an arbitrary pointer, and precede partition indexing.
    if (buildStateReleased) {
        ThrowProbeTableValidationError(0, "entry", partition, nullptr);
    }
    if (partition >= hashTableCount) {
        ThrowProbeTableValidationError(1, "entry", partition, nullptr);
    }
#endif
    // Diagnostic guards added for spill bring-up. Compiled out of release/
    // performance builds: this is the per-probe-row path.
    auto ensureBuiltTable = [this, partition](const char *path, const void *usePtr) __attribute__((always_inline)) {
#ifdef OMNI_JOIN_PROBE_VALIDATION
        if (partition >= hashTableCount) {
            ThrowProbeTableValidationError(1, path, partition, usePtr);
        }
        if (usePtr == nullptr) {
            ThrowProbeTableValidationError(2, path, partition, usePtr);
        }
#else
        (void)path;
        (void)usePtr;
#endif
    };

    KeyType key;
    if (isMultiCols) {
        if constexpr (std::is_same_v<KeyType, int8_t> || std::is_same_v<KeyType, int16_t> ||
                      std::is_same_v<KeyType, int32_t> || std::is_same_v<KeyType, int64_t> ||
                      std::is_same_v<KeyType, int128_t>) {
            key = 0;
        }
        ComputeMultiColKey(probeHashColumns, probeHashColCount, probePosition, key);
        // Multi-col always uses hashTables (never arrayTables) — check that pointer.
        const void *usePtr =
            (partition < hashTableCount) ? static_cast<const void *>(hashTables[partition].get()) : nullptr;
        ensureBuiltTable("multi/hash", usePtr);
        return hashTables[partition]->FindValueFromHashmap(key);
    }

    if constexpr (std::is_same_v<KeyType, int8_t> || std::is_same_v<KeyType, int16_t> ||
                  std::is_same_v<KeyType, int32_t> || std::is_same_v<KeyType, int64_t>) {
        if (probeHashColumns[0]->GetEncoding() == OMNI_ENCODING_CONST) {
            key = static_cast<ConstVector<KeyType> *>(probeHashColumns[0])->GetConstValue();
        } else if (probeHashColumns[0]->GetEncoding() != OMNI_DICTIONARY) {
            auto curVector = reinterpret_cast<Vector<KeyType> *>(probeHashColumns[0]);
            key = curVector->GetValue(probePosition);
        } else {
            auto curVector = reinterpret_cast<Vector<DictionaryContainer<KeyType>> *>(probeHashColumns[0]);
            key = curVector->GetValue(probePosition);
        }
        if (partition >= hashTableCount) {
            ensureBuiltTable("single/oob", nullptr);
        }
        if (hashTableTypes[partition] == HashTableImplementationType::ARRAY_HASH_TABLE) {
            const void *usePtr = static_cast<const void *>(arrayTables[partition].get());
            ensureBuiltTable("single/array", usePtr);
            if (key >= maxMins[partition].second && key <= maxMins[partition].first) {
                return arrayTables[partition]->FindValueFromHashmap(
                    static_cast<int64_t>(key - maxMins[partition].second));
            } else {
                return arrayTables[partition]->EmptyResult();
            }
        } else {
            const void *usePtr = static_cast<const void *>(hashTables[partition].get());
            ensureBuiltTable("single/hash", usePtr);
            return hashTables[partition]->FindValueFromHashmap(key);
        }
    } else {
        if constexpr (!std::is_same_v<KeyType, type::StringRef>) {
            if (probeHashColumns[0]->GetEncoding() == OMNI_ENCODING_CONST) {
                key = static_cast<ConstVector<KeyType> *>(probeHashColumns[0])->GetConstValue();
            } else if (probeHashColumns[0]->GetEncoding() != OMNI_DICTIONARY) {
                auto curVector = reinterpret_cast<Vector<KeyType> *>(probeHashColumns[0]);
                key = curVector->GetValue(probePosition);
            } else {
                auto curVector = reinterpret_cast<Vector<DictionaryContainer<KeyType>> *>(probeHashColumns[0]);
                key = curVector->GetValue(probePosition);
            }
        } else {
            auto &arenaAllocator = *(probeArena->GetArena());
            for (int32_t colIdx = 0; colIdx < probeHashColCount; colIdx++) {
                auto curVector = probeHashColumns[colIdx];
                auto &curFunc = probeSerializers[colIdx];
                curFunc(curVector, probePosition, arenaAllocator, key);
            }
        }
        const void *usePtr =
            (partition < hashTableCount) ? static_cast<const void *>(hashTables[partition].get()) : nullptr;
        ensureBuiltTable("complex/hash", usePtr);
        return hashTables[partition]->FindValueFromHashmap(key);
    }
}

template<typename KeyType, typename RowRefListType>
KeyType JoinHashTableVariants<KeyType, RowRefListType>::GetKeyValue(BaseVector **probeHashColumns,
    int32_t probePosition)
{
    KeyType key;
    if (probeHashColumns[0]->GetEncoding() == OMNI_ENCODING_CONST) {
        key = static_cast<ConstVector<KeyType> *>(probeHashColumns[0])->GetConstValue();
    } else if (probeHashColumns[0]->GetEncoding() != OMNI_DICTIONARY) {
        auto curVector = reinterpret_cast<Vector<KeyType> *>(probeHashColumns[0]);
        key = curVector->GetValue(probePosition);
    } else {
        auto curVector = reinterpret_cast<Vector<DictionaryContainer<KeyType>> *>(probeHashColumns[0]);
        key = curVector->GetValue(probePosition);
    }
    return key;
}

template <typename KeyType, typename RowRefListType>
template <typename HashTableType, bool kGrowing>
void JoinHashTableVariants<KeyType, RowRefListType>::EmplaceFixedKey(HashTableType &hashTable, int32_t partitionIndex,
    VectorBatch *vecBatch, int32_t vecBatchIdx, BaseVector **buildVectors, int32_t buildColNum)
{
    if (isNeedNullKeyTable) {
        EmplaceFixedKeyToNormalHashTable<HashTableType, kGrowing>(hashTable, partitionIndex, vecBatch, vecBatchIdx,
            buildVectors, buildColNum);
    } else {
        EmplaceFixedNotNullKeyToNormalHashTable<HashTableType, kGrowing>(hashTable, partitionIndex, vecBatch,
            vecBatchIdx, buildVectors, buildColNum);
    }
}

template <typename KeyType, typename RowRefListType>
template <typename HashTableType, bool kGrowing>
void JoinHashTableVariants<KeyType, RowRefListType>::EmplaceSingleKey(HashTableType &hashTable, int32_t partitionIndex,
    VectorBatch *vecBatch, int32_t vecBatchIdx, BaseVector **buildVectors)
{
    if (isNeedNullKeyTable) {
        if (buildVectors[0]->GetEncoding() != OMNI_DICTIONARY) {
            EmplaceSingleKeyToNormalHashTable<false, HashTableType, kGrowing>(hashTable, partitionIndex, vecBatch,
                vecBatchIdx, buildVectors);
        } else {
            EmplaceSingleKeyToNormalHashTable<true, HashTableType, kGrowing>(hashTable, partitionIndex, vecBatch,
                vecBatchIdx, buildVectors);
        }
    } else {
        if (buildVectors[0]->GetEncoding() != OMNI_DICTIONARY) {
            EmplaceSingleNotNullKeyToNormalHashTable<false, HashTableType, kGrowing>(hashTable, partitionIndex,
                vecBatch, vecBatchIdx, buildVectors);
        } else {
            EmplaceSingleNotNullKeyToNormalHashTable<true, HashTableType, kGrowing>(hashTable, partitionIndex,
                vecBatch, vecBatchIdx, buildVectors);
        }
    }
}

template <typename KeyType, typename RowRefListType>
template <bool isDic, typename HashTableType, bool kGrowing>
void JoinHashTableVariants<KeyType, RowRefListType>::EmplaceSingleNotNullKeyToNormalHashTable(HashTableType &hashTable,
    int32_t partitionIndex, VectorBatch *vecBatch, int32_t vecBatchIdx, BaseVector **buildVectors)
{
    auto rowCount = vecBatch->GetRowCount();
    // POOL-BOUND (C): RowRefList nodes and inserts allocate from per-SP ExecutionContext arena.
    // All Emplace* / TryToInsertJoinKeysToHashmap arena paths share this requirement.
    auto &arenaAllocator = *(executionContexts[partitionIndex]->GetArena());
    KeyType key;

    for (auto offset = 0; offset < rowCount; offset++) {
        bool unNullKey = (!buildVectors[0]->IsNull(offset));
        if (LIKELY(unNullKey)) {
            if constexpr (isDic) {
                key = reinterpret_cast<Vector<DictionaryContainer<KeyType>> *>(buildVectors[0])->GetValue(offset);
            } else {
                if (buildVectors[0]->GetEncoding() == OMNI_ENCODING_CONST) {
                    key = static_cast<ConstVector<KeyType> *>(buildVectors[0])->GetConstValue();
                } else {
                    key = reinterpret_cast<Vector<KeyType> *>(buildVectors[0])->GetValue(offset);
                }
            }
            auto ret = EmplaceJoinKey<kGrowing>(hashTable, key);
            ApplyRowRef(ret, static_cast<uint32_t>(offset), static_cast<uint32_t>(vecBatchIdx), arenaAllocator);
        }
    }
}

template <typename KeyType, typename RowRefListType>
template <bool isDic, typename HashTableType, bool kGrowing>
void JoinHashTableVariants<KeyType, RowRefListType>::EmplaceSingleKeyToNormalHashTable(HashTableType &hashTable,
    int32_t partitionIndex, VectorBatch *vecBatch, int32_t vecBatchIdx, BaseVector **buildVectors)
{
    auto rowCount = vecBatch->GetRowCount();
    auto &arenaAllocator = *(executionContexts[partitionIndex]->GetArena());
    KeyType key;
    for (auto offset = 0; offset < rowCount; offset++) {
        bool unNullKey = (!buildVectors[0]->IsNull(offset));
        if constexpr (isDic) {
            key = reinterpret_cast<Vector<DictionaryContainer<KeyType>> *>(buildVectors[0])->GetValue(offset);
        } else {
            if (buildVectors[0]->GetEncoding() == OMNI_ENCODING_CONST) {
                key = static_cast<ConstVector<KeyType> *>(buildVectors[0])->GetConstValue();
            } else {
                key = reinterpret_cast<Vector<KeyType> *>(buildVectors[0])->GetValue(offset);
            }
        }
        if (LIKELY(unNullKey)) {
            auto ret = EmplaceJoinKey<kGrowing>(hashTable, key);
            ApplyRowRef(ret, static_cast<uint32_t>(offset), static_cast<uint32_t>(vecBatchIdx), arenaAllocator);
        } else {
            auto ret = hashTable->InsertNullKeysToHashmap(key);
            ApplyRowRef(ret, static_cast<uint32_t>(offset), static_cast<uint32_t>(vecBatchIdx), arenaAllocator);
        }
    }
}

template <typename KeyType, typename RowRefListType>
template <typename HashTableType, bool kGrowing>
void JoinHashTableVariants<KeyType, RowRefListType>::EmplaceMultiKey(HashTableType &hashTable, int32_t partitionIndex,
    VectorBatch *vecBatch, int32_t vecBatchIdx, BaseVector **buildVectors)
{
    if (isNeedNullKeyTable) {
        EmplaceMultiKeyToNormalHashTable<HashTableType, kGrowing>(hashTable, partitionIndex,
            vecBatch, vecBatchIdx, buildVectors);
    } else {
        EmplaceMultiNotNullKeyToNormalHashTable<HashTableType, kGrowing>(hashTable, partitionIndex,
            vecBatch, vecBatchIdx, buildVectors);
    }
}

template <typename KeyType, typename RowRefListType>
template <typename HashTableType, bool kGrowing>
void JoinHashTableVariants<KeyType, RowRefListType>::EmplaceMultiNotNullKeyToNormalHashTable(HashTableType &hashTable,
    int32_t partitionIndex, VectorBatch *vecBatch, int32_t vecBatchIdx, BaseVector **buildVectors)
{
    auto rowCount = vecBatch->GetRowCount();
    auto &arenaAllocator = *(executionContexts[partitionIndex]->GetArena());
    KeyType key;

    for (auto offset = 0; offset < rowCount; offset++) {
        bool unNullKey = true;
        for (int i = 0; i < this->buildHashCols.size(); ++i) {
            unNullKey = unNullKey && (!buildVectors[i]->IsNull(offset));
        }
        if (LIKELY(unNullKey)) {
            if constexpr (std::is_same_v<KeyType, int8_t> || std::is_same_v<KeyType, int16_t> ||
                          std::is_same_v<KeyType, int32_t> || std::is_same_v<KeyType, int64_t> ||
                          std::is_same_v<KeyType, int128_t>) {
                key = 0;
            }
            ComputeMultiColKey(buildVectors, buildHashCols.size(), offset, key);
            auto ret = EmplaceJoinKey<kGrowing>(hashTable, key);
            ApplyRowRef(ret, static_cast<uint32_t>(offset), static_cast<uint32_t>(vecBatchIdx), arenaAllocator);
        }
    }
}

template <typename KeyType, typename RowRefListType>
template <typename HashTableType, bool kGrowing>
void JoinHashTableVariants<KeyType, RowRefListType>::EmplaceMultiKeyToNormalHashTable(HashTableType &hashTable,
    int32_t partitionIndex, VectorBatch *vecBatch, int32_t vecBatchIdx, BaseVector **buildVectors)
{
    auto rowCount = vecBatch->GetRowCount();
    auto &arenaAllocator = *(executionContexts[partitionIndex]->GetArena());
    KeyType key{};

    for (auto offset = 0; offset < rowCount; offset++) {
        bool unNullKey = true;
        for (int i = 0; i < this->buildHashCols.size(); ++i) {
            unNullKey = unNullKey && (!buildVectors[i]->IsNull(offset));
        }
        if (LIKELY(unNullKey)) {
            if constexpr (std::is_same_v<KeyType, int8_t> || std::is_same_v<KeyType, int16_t> ||
                          std::is_same_v<KeyType, int32_t> || std::is_same_v<KeyType, int64_t> ||
                          std::is_same_v<KeyType, int128_t>) {
                key = 0;
            }
            ComputeMultiColKey(buildVectors, buildHashCols.size(), offset, key);
            auto ret = EmplaceJoinKey<kGrowing>(hashTable, key);
            ApplyRowRef(ret, static_cast<uint32_t>(offset), static_cast<uint32_t>(vecBatchIdx), arenaAllocator);
        } else {
            auto ret = hashTable->InsertNullKeysToHashmap(key);
            ApplyRowRef(ret, static_cast<uint32_t>(offset), static_cast<uint32_t>(vecBatchIdx), arenaAllocator);
        }
    }
}

template <typename KeyType, typename RowRefListType>
template <typename HashTableType, bool kGrowing>
void JoinHashTableVariants<KeyType, RowRefListType>::EmplaceVariableKey(HashTableType &hashTable,
    int32_t partitionIndex, VectorBatch *vecBatch, int32_t vecBatchIdx, BaseVector **buildVectors, int32_t buildColNum,
    std::vector<int8_t> &isNotNullKeys, std::vector<size_t> &hashes, std::vector<KeyType> &tryRes)
{
    if (isNeedNullKeyTable) {
        EmplaceVariableKeyToNormalHashTable<HashTableType, kGrowing>(hashTable, partitionIndex, vecBatch, vecBatchIdx,
            buildVectors, buildColNum, isNotNullKeys, hashes, tryRes);
    } else {
        EmplaceVariableNotNullKeyToNormalHashTable<HashTableType, kGrowing>(hashTable, partitionIndex, vecBatch,
            vecBatchIdx, buildVectors, buildColNum, isNotNullKeys, hashes, tryRes);
    }
}

template <typename KeyType, typename RowRefListType>
template <typename HashTableType, bool kGrowing>
void JoinHashTableVariants<KeyType, RowRefListType>::EmplaceVariableNotNullKeyToNormalHashTable(
    HashTableType &hashTable, int32_t partitionIndex, VectorBatch *vecBatch, int32_t vecBatchIdx,
    BaseVector **buildVectors, int32_t buildColNum, std::vector<int8_t> &isNotNullKeys, std::vector<size_t> &hashes,
    std::vector<KeyType> &tryRes)
{
    auto rowCount = static_cast<uint32_t>(vecBatch->GetRowCount());
    auto &arenaAllocator = *(executionContexts[partitionIndex]->GetArena());
    for (uint32_t offset = 0; offset < rowCount; offset += BLOCK_SIZE) {
        uint32_t maxStep = std::min(rowCount - offset, BLOCK_SIZE);
        for (uint32_t rowIdx = offset, i = 0; rowIdx < offset + maxStep; rowIdx++, i++) {
            hashTable->TryToInsertJoinKeysToHashmap(buildVectors, buildColNum, rowIdx, i, arenaAllocator, tryRes,
                isNotNullKeys);
        }

        hashTable->BatchCalculateHash(tryRes, isNotNullKeys, hashes, maxStep);
        for (uint32_t rowIdx = 0; rowIdx < maxStep; rowIdx++) {
            if (LIKELY(isNotNullKeys[rowIdx])) {
                auto ret = EmplaceJoinKey<kGrowing>(hashTable, tryRes[rowIdx], hashes[rowIdx]);
                ApplyRowRef(ret, static_cast<uint32_t>(offset + rowIdx), static_cast<uint32_t>(vecBatchIdx), arenaAllocator);
            }
        }
    }
}

template <typename KeyType, typename RowRefListType>
template <typename HashTableType, bool kGrowing>
void JoinHashTableVariants<KeyType, RowRefListType>::EmplaceVariableKeyToNormalHashTable(HashTableType &hashTable,
    int32_t partitionIndex, VectorBatch *vecBatch, int32_t vecBatchIdx, BaseVector **buildVectors, int32_t buildColNum,
    std::vector<int8_t> &isNotNullKeys, std::vector<size_t> &hashes, std::vector<KeyType> &tryRes)
{
    auto rowCount = static_cast<uint32_t>(vecBatch->GetRowCount());
    auto &arenaAllocator = *(executionContexts[partitionIndex]->GetArena());
    for (uint32_t offset = 0; offset < rowCount; offset += BLOCK_SIZE) {
        uint32_t maxStep = std::min(rowCount - offset, BLOCK_SIZE);
        for (uint32_t rowIdx = offset, i = 0; rowIdx < offset + maxStep; rowIdx++, i++) {
            hashTable->TryToInsertJoinKeysToHashmap(buildVectors, buildColNum, rowIdx, i, arenaAllocator, tryRes,
                isNotNullKeys);
        }

        hashTable->BatchCalculateHash(tryRes, isNotNullKeys, hashes, maxStep);
        for (uint32_t rowIdx = 0; rowIdx < maxStep; rowIdx++) {
            if (LIKELY(isNotNullKeys[rowIdx])) {
                auto ret = EmplaceJoinKey<kGrowing>(hashTable, tryRes[rowIdx], hashes[rowIdx]);
                ApplyRowRef(ret, static_cast<uint32_t>(offset + rowIdx), static_cast<uint32_t>(vecBatchIdx), arenaAllocator);
            } else {
                auto ret = hashTable->InsertNullKeysToHashmap(tryRes[rowIdx]);
                ApplyRowRef(ret, static_cast<uint32_t>(offset + rowIdx), static_cast<uint32_t>(vecBatchIdx), arenaAllocator);
            }
        }
    }
}

template <typename KeyType, typename RowRefListType>
template <bool kGrowing>
void JoinHashTableVariants<KeyType, RowRefListType>::EmplaceBatch(int32_t partitionIndex, VectorBatch *vecBatch,
    int32_t vecBatchIdx)
{
    auto &hashTable = hashTables[partitionIndex];
    auto buildColNum = static_cast<int32_t>(this->buildHashCols.size());
    BaseVector *buildVectors[buildColNum];
    if constexpr (!std::is_same_v<KeyType, StringRef>) {
        for (int32_t i = 0; i < buildColNum; ++i) {
            buildVectors[i] = vecBatch->Get(this->buildHashCols[i]);
        }
        if (isMultiCols) {
            EmplaceMultiKey<std::unique_ptr<JoinHashTableVariant<KeyType, RowRefListType>>, kGrowing>(hashTable,
                partitionIndex, vecBatch, vecBatchIdx, buildVectors);
        } else {
            EmplaceSingleKey<std::unique_ptr<JoinHashTableVariant<KeyType, RowRefListType>>, kGrowing>(hashTable,
                partitionIndex, vecBatch, vecBatchIdx, buildVectors);
        }
    } else if (isFixedKeys) {
        hashTable->ResetFixedKeysIgnoreNullSerializerSimd();
        for (int32_t i = 0; i < buildColNum; ++i) {
            auto curVector = vecBatch->Get(this->buildHashCols[i]);
            if (curVector->GetEncoding() == Encoding::OMNI_DICTIONARY) {
                hashTable->PushBackFixedKeysIgnoreNullSerializer(
                    dicVectorSerializerFixedKeysIgnoreNullCenter[curVector->GetTypeId()]);
                hashTable->PushBackFixedKeysIgnoreNullSerializerSimd(
                    dicVectorSerializerFixedKeysIgnoreNullCenterSimd[curVector->GetTypeId()]);
            } else if (curVector->GetEncoding() == Encoding::OMNI_ENCODING_CONST) {
                hashTable->PushBackFixedKeysIgnoreNullSerializer(
                    constVectorSerializerFixedKeysIgnoreNullCenter[curVector->GetTypeId()]);
                hashTable->PushBackFixedKeysIgnoreNullSerializerSimd(
                    constVectorSerializerFixedKeysIgnoreNullCenterSimd[curVector->GetTypeId()]);
            } else {
                hashTable->PushBackFixedKeysIgnoreNullSerializer(
                    vectorSerializerFixedKeysIgnoreNullCenter[curVector->GetTypeId()]);
                hashTable->PushBackFixedKeysIgnoreNullSerializerSimd(
                    vectorSerializerFixedKeysIgnoreNullCenterSimd[curVector->GetTypeId()]);
            }
            buildVectors[i] = curVector;
        }
        EmplaceFixedKey<std::unique_ptr<JoinHashTableVariant<KeyType, RowRefListType>>, kGrowing>(hashTable,
            partitionIndex, vecBatch, vecBatchIdx, buildVectors, buildColNum);
    } else {
        hashTable->ResetIgnoreNullSerializer();
        for (int32_t i = 0; i < buildColNum; ++i) {
            auto curVector = vecBatch->Get(this->buildHashCols[i]);
            auto typeId = static_cast<size_t>(curVector->GetTypeId());
            VectorSerializerIgnoreNull serializer = nullptr;
            if (curVector->GetEncoding() == Encoding::OMNI_DICTIONARY && typeId < dicVectorSerializerIgnoreNullCenter.size()) {
                serializer = dicVectorSerializerIgnoreNullCenter[typeId];
            } else if (curVector->GetEncoding() == Encoding::OMNI_ENCODING_CONST &&
                typeId < constVectorSerializerIgnoreNullCenter.size()) {
                serializer = constVectorSerializerIgnoreNullCenter[typeId];
            }

            // Complex join keys (e.g. struct) may have only flat serializer registered.
            if (serializer == nullptr && typeId < vectorSerializerIgnoreNullCenter.size()) {
                serializer = vectorSerializerIgnoreNullCenter[typeId];
            }
            if (serializer == nullptr) {
                throw OmniException("UNSUPPORTED_ERROR", "No build serializer for join key type");
            }
            hashTable->PushBackIgnoreNullSerializer(serializer);
            buildVectors[i] = curVector;
        }
        std::vector<int8_t> isNotNullKeys(BLOCK_SIZE);
        std::vector<size_t> hashes(BLOCK_SIZE);
        std::vector<StringRef> tryRes(BLOCK_SIZE);
        EmplaceVariableKey<std::unique_ptr<JoinHashTableVariant<StringRef, RowRefListType>>, kGrowing>(hashTable,
            partitionIndex, vecBatch, vecBatchIdx, buildVectors, buildColNum, isNotNullKeys, hashes, tryRes);
    }
}

template <typename KeyType, typename RowRefListType>
template <bool kGrowing>
void JoinHashTableVariants<KeyType, RowRefListType>::BuildNormalHashTableWithFixedKey(int32_t partitionIndex,
    uint8_t initDegree)
{
    // Pool-bound: route hashmap memory to the task pool.
    hashTables[partitionIndex] = std::make_unique<JoinHashTableVariant<KeyType, RowRefListType>>(
        executionContexts[partitionIndex]->GetAllocator(), std::max(initDegree, MIN_DEGREE));
    auto &vecBatchesOnePartition = inputVecBatches[partitionIndex];
    auto vecBatchesNum = static_cast<int32_t>(vecBatchesOnePartition.size());
    for (auto j = 0; j < vecBatchesNum; j++) {
        EmplaceBatch<kGrowing>(partitionIndex, vecBatchesOnePartition[j], j);
    }
    hashTableTypes[partitionIndex] = HashTableImplementationType::NORMAL_HASH_TABLE;
    hashTableSize++;
}

template <typename KeyType, typename RowRefListType>
template <bool kGrowing>
void JoinHashTableVariants<KeyType, RowRefListType>::BuildNormalHashTableWithVariableKey(int32_t partitionIndex,
    uint8_t initDegree)
{
    if constexpr (std::is_same_v<KeyType, StringRef>) {
        BuildNormalHashTableWithFixedKey<kGrowing>(partitionIndex, initDegree);
    }
}

template <typename KeyType, typename RowRefListType>
uint8_t JoinHashTableVariants<KeyType, RowRefListType>::NormalTableDegree(uint32_t rows)
{
    // Size for the 0.9 load the growing path already runs at, keeping at least one empty
    // group. Sizing for 0.75 doubles the table whenever rows fall in (0.75, 0.9] of a power
    // of two, and rows already over-count the distinct keys the table holds.
    uint8_t degree = MIN_DEGREE;
    while ((1ULL << degree) * MAX_LOAD_FACTOR < static_cast<double>(rows) ||
        (1ULL << degree) - rows < Group::kWidth) {
        ++degree;
    }
    return degree;
}

template <typename KeyType, typename RowRefListType>
uint8_t JoinHashTableVariants<KeyType, RowRefListType>::AdaptiveInitDegree()
{
    uint8_t initDegree = MIN_DEGREE;
    // Group probe leaves the sentinel tail unwrapped. Keep one entire group empty
    // at the 0.9 growth threshold so a tiny adaptive seed cannot overwrite a live
    // slot before NeedRehash. With 16-slot groups this starts at 256 slots.
    auto spareSlots = [](uint8_t degree) {
        const uint64_t capacity = 1ULL << degree;
        return capacity - static_cast<uint64_t>(std::ceil(static_cast<double>(capacity) * MAX_LOAD_FACTOR));
    };
    while (initDegree < 63 && spareSlots(initDegree) < Group::kWidth) {
        ++initDegree;
    }
    return initDegree;
}

template <typename KeyType, typename RowRefListType>
void JoinHashTableVariants<KeyType, RowRefListType>::AddVecBatchDistinct(int32_t partitionIndex,
    omniruntime::vec::VectorBatch *vecBatch)
{
    UniqueVecBatch owned(vecBatch);
    const int32_t rowCount = owned->GetRowCount();
    if (rowCount <= 0) {
        return;
    }
    if (hashTables[partitionIndex] == nullptr) {
        hashTables[partitionIndex] = std::make_unique<JoinHashTableVariant<KeyType, RowRefListType>>(
            executionContexts[partitionIndex]->GetAllocator(), AdaptiveInitDegree());
        // Grown from a small seed rather than pre-sized: double instead of the default x4 so the
        // final table stays within 2x of the distinct keys.
        hashTables[partitionIndex]->hashmap.SetGrowthStep(1);
        hashTableTypes[partitionIndex] = HashTableImplementationType::NORMAL_HASH_TABLE;
        incrementalTable_[partitionIndex] = 1;
    }
    auto &batches = inputVecBatches[partitionIndex];
    const auto vecBatchIdx = static_cast<int32_t>(batches.size());
    distinctRows_.clear();
    collectDistinct_ = true;
    try {
        EmplaceBatch<true>(partitionIndex, owned.get(), vecBatchIdx);
    } catch (...) {
        collectDistinct_ = false;
        throw;
    }
    collectDistinct_ = false;
    const auto kept = static_cast<int32_t>(distinctRows_.size());
    if (kept == 0) {
        return;
    }
    if (kept < rowCount) {
        // Keep only the rows that introduced a key. The probe never reads build rows; the kept
        // rows remain the source of a later build spill or of a full build after abandonment.
        UniqueVecBatch compact(new omniruntime::vec::VectorBatch(kept));
        const int32_t vectorCount = owned->GetVectorCount();
        compact->ResizeVectorCount(vectorCount);
        for (int32_t col = 0; col < vectorCount; ++col) {
            compact->SetVector(col, VectorHelper::CopyPositionsVector(owned->Get(col), distinctRows_.data(), 0, kept));
        }
        // CopyPositions keeps dictionary encoding, which would pin the dropped rows' dictionary.
        VectorHelper::CompactSparseDictionaries(compact.get(), pool_);
        owned = std::move(compact);
    }
    batches.emplace_back(owned.release());
    totalRowCount[partitionIndex] += static_cast<uint32_t>(kept);
    distinctRows_.clear();
    distinctInputRows_ += static_cast<uint64_t>(rowCount);
    distinctKeptRows_ += static_cast<uint64_t>(kept);
    // Like Velox's abandonBuildNoDupHash: when nearly every row brings a new key the dedup saves
    // little, while the growing table (rehash keeps old and new tables live) and the forced
    // normal hash table (no array table) cost more than they save.
    constexpr uint64_t kAbandonMinRows = 100000;
    constexpr uint64_t kAbandonMinKeptPct = 50;
    if (distinctInputRows_ >= kAbandonMinRows && distinctKeptRows_ * 100 >= distinctInputRows_ * kAbandonMinKeptPct) {
        AbandonIncrementalTables();
    }
}

template <typename KeyType, typename RowRefListType>
void JoinHashTableVariants<KeyType, RowRefListType>::AbandonIncrementalTables()
{
    distinctAbandoned_ = true;
    for (uint32_t partition = 0; partition < hashTableCount; ++partition) {
        if (!incrementalTable_[partition]) {
            continue;
        }
        // Drop the table before the arena/allocator it was built from; the retained rows stay
        // and the end-of-input build indexes them like any other build.
        hashTables[partition].reset();
        incrementalTable_[partition] = 0;
        executionContexts[partition] = std::make_unique<ExecutionContext>(4096, pool_);
    }
}

template <typename KeyType, typename RowRefListType>
bool JoinHashTableVariants<KeyType, RowRefListType>::FinishIncrementalTable(int32_t partitionIndex)
{
    if (!incrementalTable_[partitionIndex] || hashTables[partitionIndex] == nullptr) {
        return false;
    }
    incrementalTable_[partitionIndex] = 0;
    hashTableTypes[partitionIndex] = HashTableImplementationType::NORMAL_HASH_TABLE;
    hashTableSize++;
    return true;
}

template <typename KeyType, typename RowRefListType>
template <typename HashTableType, bool kGrowing>
void JoinHashTableVariants<KeyType, RowRefListType>::EmplaceFixedNotNullKeyToNormalHashTable(HashTableType &hashTable,
    int32_t partitionIndex, VectorBatch *vecBatch, int32_t vecBatchIdx, BaseVector **buildVectors, int32_t buildColNum)
{
    auto rowCount = static_cast<uint32_t>(vecBatch->GetRowCount());
    auto &arenaAllocator = *(executionContexts[partitionIndex]->GetArena());
    KeyType key;
    bool unNullKey = false;
    char *keysPool = reinterpret_cast<char *>(arenaAllocator.Allocate(fixedKeysSize * rowCount));
    std::vector<KeyType> keys;
    std::vector<bool> unNullKeys(rowCount, true);
    for (uint32_t i = 0;i < rowCount; i++) {
        keys.emplace_back(StringRef(keysPool, fixedKeysSize));
        keysPool += fixedKeysSize;
    }
    size_t pos = 0;
    for (uint32_t col = 0; col < buildColNum; col++) {
        hashTable->TryToInsertFixedJoinKeysToHashmapSimd(buildVectors, rowCount, col, keys, unNullKeys, pos);
    }
    for (uint32_t offset = 0; offset < rowCount; offset++) {
        if (LIKELY(unNullKeys[offset])) {
            auto ret = EmplaceJoinKey<kGrowing>(hashTable, keys[offset]);
            ApplyRowRef(ret, static_cast<uint32_t>(offset), static_cast<uint32_t>(vecBatchIdx), arenaAllocator);
        }
    }
}
template <typename KeyType, typename RowRefListType>
template <typename HashTableType, bool kGrowing>
void JoinHashTableVariants<KeyType, RowRefListType>::EmplaceFixedKeyToNormalHashTable(HashTableType &hashTable,
    int32_t partitionIndex, VectorBatch *vecBatch, int32_t vecBatchIdx, BaseVector **buildVectors, int32_t buildColNum)
{
    auto rowCount = static_cast<uint32_t>(vecBatch->GetRowCount());
    auto &arenaAllocator = *(executionContexts[partitionIndex]->GetArena());
    KeyType key;
    bool unNullKey = false;
    char *keysPool = reinterpret_cast<char *>(arenaAllocator.Allocate(fixedKeysSize * rowCount));
    std::vector<KeyType> keys;
    std::vector<bool> unNullKeys;
    for (size_t i = 0; i < rowCount; i++) {
        unNullKeys.emplace_back(true);
    }
    for (uint32_t i = 0; i < rowCount; i++) {
        keys.emplace_back(StringRef(keysPool, fixedKeysSize));
        keysPool += fixedKeysSize;
    }
    size_t pos = 0;
    for (uint32_t col = 0; col < buildColNum; col++) {
        hashTable->TryToInsertFixedJoinKeysToHashmapSimd(buildVectors, rowCount, col, keys, unNullKeys, pos);
    }
    for (uint32_t offset = 0; offset < rowCount; offset++) {
        if (LIKELY(unNullKeys[offset])) {
            auto ret = EmplaceJoinKey<kGrowing>(hashTable, keys[offset]);
            ApplyRowRef(ret, static_cast<uint32_t>(offset), static_cast<uint32_t>(vecBatchIdx), arenaAllocator);
        } else {
            auto ret = hashTable->InsertNullKeysToHashmap(keys[offset]);
            ApplyRowRef(ret, static_cast<uint32_t>(offset), static_cast<uint32_t>(vecBatchIdx), arenaAllocator);
        }
    }
}

/// partitionIndex is from 0 to hashTableCount - 1
/// there are always one HashTable for one BuildSize partition, so partitionIndex is always 0
/// after subpartition function is developed, there are `numSubPartition` HashTables, so it can be from 0 to numSubPartition - 1
namespace {
constexpr uint32_t kMaxParallelBuildThreads = 32;

size_t RoundUpToGroupWidth(size_t value)
{
    return ((value + Group::kWidth - 1) / Group::kWidth) * Group::kWidth;
}

int32_t FindBucketPartition(size_t bucketOffset, const std::vector<size_t> &bounds)
{
    for (size_t i = 1; i < bounds.size(); ++i) {
        if (bucketOffset < bounds[i]) {
            return static_cast<int32_t>(i - 1);
        }
    }
    return static_cast<int32_t>(bounds.size()) - 2;
}

struct ParallelBuildRowLoc {
    int32_t vecBatchIdx;
    uint32_t rowOffset;
};

template <typename KeyType>
KeyType ExtractBuildKey(BaseVector *buildVector, uint32_t offset)
{
    if (buildVector->GetEncoding() == OMNI_ENCODING_CONST) {
        return static_cast<ConstVector<KeyType> *>(buildVector)->GetConstValue();
    }
    if (buildVector->GetEncoding() == OMNI_DICTIONARY) {
        return reinterpret_cast<Vector<DictionaryContainer<KeyType>> *>(buildVector)->GetValue(offset);
    }
    return reinterpret_cast<Vector<KeyType> *>(buildVector)->GetValue(offset);
}

template <typename KeyType, typename RowRefListType>
void ApplyBuildRowRef(InsertResult<RowRefListType *> &ret, mem::SimpleArenaAllocator &arenaAllocator, uint32_t rowOffset,
    uint32_t vecBatchIdx, size_t sizeOfRowRefList)
{
    RowRefListType *rowRef = nullptr;
    if (ret.IsInsert()) {
        rowRef = reinterpret_cast<RowRefListType *>(arenaAllocator.Allocate(sizeOfRowRefList));
        *rowRef = RowRefListType(rowOffset, vecBatchIdx);
        ret.SetValue(rowRef);
    } else {
        rowRef = ret.GetValue();
        rowRef->Insert({rowOffset, vecBatchIdx}, arenaAllocator);
    }
}
} // namespace

template <typename KeyType, typename RowRefListType>
bool JoinHashTableVariants<KeyType, RowRefListType>::TryBuildHashTableParallel(int32_t partitionIndex,
    uint32_t numThreads, uint32_t minRowsPerPartition)
{
    if constexpr (std::is_same_v<KeyType, StringRef> || std::is_same_v<KeyType, Decimal128> ||
                  std::is_same_v<KeyType, int128_t>) {
        return false;
    }
    if (numThreads <= 1 || !isFixedKeys || isMultiCols || hashTableCount != 1 || incrementalTable_[partitionIndex]) {
        return false;
    }
    if (totalRowCount[partitionIndex] < minRowsPerPartition) {
        return false;
    }

    auto initDegree = static_cast<uint8_t>(std::ceil(log2(totalRowCount[partitionIndex] / LOAD_FACTOR)));
    auto lengthOfArrayHT = static_cast<int64_t>(std::pow(2, initDegree));
    if (buildHashCols.size() == 1) {
        switch (buildTypes->GetIds()[buildHashCols[0]]) {
            case omniruntime::type::OMNI_INT:
            case omniruntime::type::OMNI_DATE32:
                if (IsArrayTableEligible<int32_t>(buildHashCols[0], lengthOfArrayHT, partitionIndex)) {
                    return false;
                }
                break;
            case omniruntime::type::OMNI_SHORT:
                if (IsArrayTableEligible<int16_t>(buildHashCols[0], lengthOfArrayHT, partitionIndex)) {
                    return false;
                }
                break;
            case omniruntime::type::OMNI_TIMESTAMP:
            case omniruntime::type::OMNI_LONG:
                if (IsArrayTableEligible<int64_t>(buildHashCols[0], lengthOfArrayHT, partitionIndex)) {
                    return false;
                }
                break;
            case omniruntime::type::OMNI_BYTE:
                if (IsArrayTableEligible<int8_t>(buildHashCols[0], lengthOfArrayHT, partitionIndex)) {
                    return false;
                }
                break;
            default:
                break;
        }
    }

    const size_t tableCapacity = 1ULL << std::max(initDegree, MIN_DEGREE);
    numThreads = std::min(numThreads, kMaxParallelBuildThreads);
    while (numThreads > 1 && tableCapacity / numThreads < minRowsPerPartition) {
        --numThreads;
    }
    if (numThreads <= 1) {
        return false;
    }

    Prepare(partitionIndex);

    auto hashTable = std::make_unique<JoinHashTableVariant<KeyType, RowRefListType>>(std::max(initDegree, MIN_DEGREE));
    hashTable->hashmap.SetRehashDisabled(true);
    const size_t bucketMask = hashTable->hashmap.GetCapacity() - 1;

    std::vector<size_t> partitionBounds(numThreads + 1);
    for (uint32_t i = 0; i < numThreads; ++i) {
        partitionBounds[i] = RoundUpToGroupWidth((tableCapacity / numThreads) * i);
    }
    partitionBounds[numThreads] = tableCapacity;

    struct RowWithHash {
        ParallelBuildRowLoc loc;
        KeyType key;
        size_t hashValue;
    };
    std::vector<std::vector<RowWithHash>> rowsPerPartition(numThreads);

    omniruntime::simdutil::HashCRC32<KeyType> hasher;
    auto &vecBatches = inputVecBatches[partitionIndex];
    const int32_t buildCol = buildHashCols[0];
    for (int32_t vecBatchIdx = 0; vecBatchIdx < static_cast<int32_t>(vecBatches.size()); ++vecBatchIdx) {
        auto *vecBatch = vecBatches[vecBatchIdx];
        auto *buildVector = vecBatch->Get(buildCol);
        const auto rowCount = static_cast<uint32_t>(vecBatch->GetRowCount());
        for (uint32_t offset = 0; offset < rowCount; ++offset) {
            const bool unNullKey = !buildVector->IsNull(offset);
            if (!unNullKey) {
                continue;
            }
            auto key = ExtractBuildKey<KeyType>(buildVector, offset);
            auto hashValue = hasher(key);
            const auto bucketOffset = H1(hashValue) & bucketMask;
            const auto partition = FindBucketPartition(bucketOffset, partitionBounds);
            rowsPerPartition[partition].push_back({{vecBatchIdx, offset}, key, hashValue});
        }
    }

    std::atomic<size_t> parallelElementsSize{0};
    hashTable->hashmap.EnableParallelBuildCounter(&parallelElementsSize);
    std::vector<std::vector<RowWithHash>> overflowPerPartition(numThreads);
    std::vector<std::exception_ptr> buildExceptions(numThreads);

    auto buildPartition = [&](uint32_t partition) {
        try {
            TableInsertPartitionInfo partitionInfo{partitionBounds[partition], partitionBounds[partition + 1]};
            auto &arenaAllocator = *(executionContexts[partitionIndex]->GetArena());
            for (auto &row : rowsPerPartition[partition]) {
                auto retOpt = hashTable->TryInsertJoinKeysToHashmapWithPartition(row.key, row.hashValue, &partitionInfo);
                if (!retOpt.has_value()) {
                    overflowPerPartition[partition].push_back(row);
                    continue;
                }
                ApplyBuildRowRef<KeyType, RowRefListType>(
                    *retOpt, arenaAllocator, row.loc.rowOffset, row.loc.vecBatchIdx, sizeOfRowRefList);
            }
        } catch (...) {
            buildExceptions[partition] = std::current_exception();
        }
    };

    std::vector<std::thread> buildThreads;
    buildThreads.reserve(numThreads > 1 ? numThreads - 1 : 0);
    for (uint32_t partition = 0; partition + 1 < numThreads; ++partition) {
        buildThreads.emplace_back(buildPartition, partition);
    }
    buildPartition(numThreads - 1);
    for (auto &thread : buildThreads) {
        thread.join();
    }
    for (const auto &ex : buildExceptions) {
        if (ex) {
            std::rethrow_exception(ex);
        }
    }

    hashTable->hashmap.DisableParallelBuildCounter();
    hashTable->hashmap.SetElementsSize(parallelElementsSize.load(std::memory_order_relaxed));

    for (uint32_t partition = 0; partition < numThreads; ++partition) {
        auto &arenaAllocator = *(executionContexts[partitionIndex]->GetArena());
        for (auto &row : overflowPerPartition[partition]) {
            auto ret = hashTable->InsertJoinKeysToHashmap(row.key, row.hashValue);
            ApplyBuildRowRef<KeyType, RowRefListType>(
                ret, arenaAllocator, row.loc.rowOffset, row.loc.vecBatchIdx, sizeOfRowRefList);
        }
    }

    if (isNeedNullKeyTable) {
        auto &arenaAllocator = *(executionContexts[partitionIndex]->GetArena());
        for (int32_t vecBatchIdx = 0; vecBatchIdx < static_cast<int32_t>(vecBatches.size()); ++vecBatchIdx) {
            auto *vecBatch = vecBatches[vecBatchIdx];
            auto *buildVector = vecBatch->Get(buildCol);
            const auto rowCount = static_cast<uint32_t>(vecBatch->GetRowCount());
            for (uint32_t offset = 0; offset < rowCount; ++offset) {
                if (buildVector->IsNull(offset)) {
                    KeyType key = ExtractBuildKey<KeyType>(buildVector, offset);
                    auto ret = hashTable->InsertNullKeysToHashmap(key);
                    ApplyBuildRowRef<KeyType, RowRefListType>(
                        ret, arenaAllocator, offset, vecBatchIdx, sizeOfRowRefList);
                }
            }
        }
    }

    hashTables[partitionIndex] = std::move(hashTable);
    hashTableTypes[partitionIndex] = HashTableImplementationType::NORMAL_HASH_TABLE;
    hashTableSize++;
    return true;
}

template <typename KeyType, typename RowRefListType>
template <typename T>
bool JoinHashTableVariants<KeyType, RowRefListType>::IsArrayTableEligible(uint32_t colIndex, int64_t rangeUpperBound,
    int32_t partitionIndex)
{
    auto &vecBatchesOnePartition = inputVecBatches[partitionIndex];
    int32_t vecBatchCount = vecBatchesOnePartition.size();
    T max = std::numeric_limits<T>::min();
    T min = std::numeric_limits<T>::max();
    int64_t uint32Max = UINT32_MAX;
    for (int32_t vecBatchIdx = 0; vecBatchIdx < vecBatchCount; ++vecBatchIdx) {
        VectorBatch *vecBatch = vecBatchesOnePartition[vecBatchIdx];
        auto rowCount = static_cast<uint32_t>(vecBatch->GetRowCount());
        auto vector = vecBatch->Get(colIndex);
        if (vector->GetEncoding() == OMNI_ENCODING_CONST) {
            if (!vector->IsNull(0)) {
                auto value = static_cast<ConstVector<T> *>(vector)->GetConstValue();
                max = std::max(max, value);
                min = std::min(min, value);
            }
        } else if (vector->GetEncoding() != OMNI_DICTIONARY) {
            auto valuePtr = unsafe::UnsafeVector::GetRawValues(static_cast<Vector<T> *>(vector));
            if (vector->HasNull()) {
                for (int32_t i = 0; i < rowCount; i++) {
                    if (vector->IsNull(i)) {
                        continue;
                    }
                    auto value = *(valuePtr + i);
                    max = std::max(max, value);
                    min = std::min(min, value);
                }
            } else {
                const auto [minPtr, maxPtr] = std::minmax_element(valuePtr, valuePtr + rowCount);
                max = std::max(max, *maxPtr);
                min = std::min(min, *minPtr);
            }
            if (max > 0 && min < 0 && min + ARRAY_THRESHOLD * rangeUpperBound < max) {
                return false;
            }
            if (max - min > uint32Max) {
                return false;
            }
        } else {
            return false;
        }
    }

    if (max < min) {
        return false;
    }
    if (min < 0 && std::numeric_limits<T>::max() + min < max) {
        return false;
    }
    if (max - min > uint32Max || max - min > ARRAY_THRESHOLD * rangeUpperBound) {
        return false;
    }
    return true;
}

template <typename KeyType, typename RowRefListType>
void JoinHashTableVariants<KeyType, RowRefListType>::BuildHashTable(int32_t partitionIndex)
{
    if (buildStateReleased || FinishIncrementalTable(partitionIndex)) {
        return;
    }
    /// if build size partition is an empty partition, we need to provide a empty HashTable for probe size.
    if (totalRowCount[partitionIndex] == 0) {
        if (isFixedKeys) {
            BuildNormalHashTableWithFixedKey<false>(partitionIndex, MIN_DEGREE);
        } else {
            BuildNormalHashTableWithVariableKey<false>(partitionIndex, MIN_DEGREE);
        }
        return;
    }
    auto initDegree = static_cast<uint8_t>(std::ceil(log2(totalRowCount[partitionIndex] / LOAD_FACTOR)));
    auto lengthOfArrayHT = static_cast<int64_t>(std::pow(2, initDegree));
    bool shouldBuildArrayTable = false;
    if (buildHashCols.size() == 1) {
        switch (buildTypes->GetIds()[buildHashCols[0]]) {
            case omniruntime::type::OMNI_INT:
            case omniruntime::type::OMNI_DATE32: {
                int32_t min;
                int32_t max;
                shouldBuildArrayTable =
                    TryToBuildArrayTable(buildHashCols[0], min, max, lengthOfArrayHT, partitionIndex);
                break;
            }
            case omniruntime::type::OMNI_SHORT: {
                int16_t min;
                int16_t max;
                shouldBuildArrayTable =
                    TryToBuildArrayTable(buildHashCols[0], min, max, lengthOfArrayHT, partitionIndex);
                break;
            }
            case omniruntime::type::OMNI_TIMESTAMP:
            case omniruntime::type::OMNI_LONG: {
                int64_t min;
                int64_t max;
                shouldBuildArrayTable =
                    TryToBuildArrayTable(buildHashCols[0], min, max, lengthOfArrayHT, partitionIndex);
                break;
            }
            case omniruntime::type::OMNI_BYTE: {
                int8_t min;
                int8_t max;
                shouldBuildArrayTable =
                    TryToBuildArrayTable(buildHashCols[0], min, max, lengthOfArrayHT, partitionIndex);
                break;
            }
            default: {
                break;
            }
        }
    }

    if (!shouldBuildArrayTable) {
        const uint8_t normalDegree = NormalTableDegree(totalRowCount[partitionIndex]);
        if (isFixedKeys) {
            BuildNormalHashTableWithFixedKey<false>(partitionIndex, normalDegree);
        } else {
            BuildNormalHashTableWithVariableKey<false>(partitionIndex, normalDegree);
        }
    }
}

template <typename KeyType, typename RowRefListType>
void JoinHashTableVariants<KeyType, RowRefListType>::BuildHashTableAdaptive(int32_t partitionIndex)
{
    if (buildStateReleased || FinishIncrementalTable(partitionIndex)) {
        return;
    }
    const uint8_t initDegree = AdaptiveInitDegree();
    if (isFixedKeys) {
        BuildNormalHashTableWithFixedKey<true>(partitionIndex, initDegree);
    } else {
        BuildNormalHashTableWithVariableKey<true>(partitionIndex, initDegree);
    }
}

template <typename KeyType, typename RowRefListType>
template <typename T>
bool JoinHashTableVariants<KeyType, RowRefListType>::TryToBuildArrayTable(uint32_t colIndex, T &min, T &max,
    int64_t rangeUpperBound, int32_t partitionIndex)
{
    auto &vecBatchesOnePartition = inputVecBatches[partitionIndex];
    int32_t vecBatchCount = vecBatchesOnePartition.size();
    max = std::numeric_limits<T>::min();
    min = std::numeric_limits<T>::max();
    int64_t uint32Max = UINT32_MAX;
    for (int32_t vecBatchIdx = 0; vecBatchIdx < vecBatchCount; ++vecBatchIdx) {
        VectorBatch *vecBatch = vecBatchesOnePartition[vecBatchIdx];
        auto rowCount = static_cast<uint32_t>(vecBatch->GetRowCount());
        auto vector = vecBatch->Get(colIndex);
        if (vector->GetEncoding() == OMNI_ENCODING_CONST) {
            if (!vector->IsNull(0)) {
                auto value = static_cast<ConstVector<T> *>(vector)->GetConstValue();
                max = std::max(max, value);
                min = std::min(min, value);
            }
        } else if (vector->GetEncoding() != OMNI_DICTIONARY) {
            // Caveat: null data might be a random number
            auto valuePtr = unsafe::UnsafeVector::GetRawValues(static_cast<Vector<T> *>(vector));
            if (vector->HasNull()) {
                for (int32_t i = 0; i < rowCount; i++) {
                    if (vector->IsNull(i)) {
                        continue;
                    }
                    auto value = *(valuePtr + i);
                    max = std::max(max, value);
                    min = std::min(min, value);
                }
            } else {
                const auto [minPtr, maxPtr] = std::minmax_element(valuePtr, valuePtr + rowCount);
                max = std::max(max, *maxPtr);
                min = std::min(min, *minPtr);
            }
            // to prevent max - min overflow
            if (max > 0 && min < 0 && min + ARRAY_THRESHOLD * rangeUpperBound < max) {
                return false;
            }
            if (max - min > uint32Max) {
                return false;
            }
        } else {
            return false;
        }
    }

    if (max < min) { // vecBatchCount might be 0, so max and min stay unchanged.
        return false;
    }
    if (min < 0 && std::numeric_limits<T>::max() + min < max) {
        return false;
    }
    if (max - min > uint32Max || max - min > ARRAY_THRESHOLD * rangeUpperBound) {
        return false;
    }

    rangeUpperBound = max - min + 1;
    if (isNeedNullKeyTable) {
        EmplaceKeyToArrayTable(min, max, rangeUpperBound, partitionIndex);
    } else {
        EmplaceNotNullKeyToArrayTable(min, max, rangeUpperBound, partitionIndex);
    }
    return true;
}

template <typename KeyType, typename RowRefListType>
template <typename T>
void JoinHashTableVariants<KeyType, RowRefListType>::EmplaceNotNullKeyToArrayTable(T &min, T &max,
    int64_t rangeUpperBound, int32_t partitionIndex)
{
    // Pool-bound: route array-map memory to the task pool.
    auto hashTable = std::make_unique<DefaultArrayMap<RowRefListType>>(
        rangeUpperBound, OmniHashmapAllocator(executionContexts[partitionIndex]->GetAllocator()));
    auto &arenaAllocator = *(executionContexts[partitionIndex]->GetArena());
    T key;

    auto buildHashCol = buildHashCols[0];
    auto &vecBatchesOnePartition = inputVecBatches[partitionIndex];
    int32_t vecBatchCount = vecBatchesOnePartition.size();
    for (auto j = 0; j < vecBatchCount; j++) {
        auto vecBatch = vecBatchesOnePartition[j];
        auto curVector = reinterpret_cast<Vector<T> *>(vecBatch->Get(buildHashCol));
        auto rowCount = vecBatch->GetRowCount();
        if (!curVector->HasNull()) {
            for (auto offset = 0; offset < rowCount; offset++) {
                key = curVector->GetValue(offset);
                auto ret = hashTable->InsertJoinKeysToHashmap(static_cast<size_t>(key - min));
                ApplyRowRef(ret, static_cast<uint32_t>(offset), static_cast<uint32_t>(j), arenaAllocator);
            }
        } else {
            for (auto offset = 0; offset < rowCount; offset++) {
                bool unNullKey = (!curVector->IsNull(offset));
                if (LIKELY(unNullKey)) {
                    key = curVector->GetValue(offset);
                    auto ret = hashTable->InsertJoinKeysToHashmap(static_cast<size_t>(key - min));
                    ApplyRowRef(ret, static_cast<uint32_t>(offset), static_cast<uint32_t>(j), arenaAllocator);
                }
            }
        }
    }

    arrayTables[partitionIndex] = std::move(hashTable);
    hashTableTypes[partitionIndex] = HashTableImplementationType::ARRAY_HASH_TABLE;
    maxMins[partitionIndex] = std::make_pair(max, min);
    hashTableSize++;
}

template <typename KeyType, typename RowRefListType>
template <typename T>
void JoinHashTableVariants<KeyType, RowRefListType>::EmplaceKeyToArrayTable(T &min, T &max, int64_t rangeUpperBound,
    int32_t partitionIndex)
{
    // Pool-bound: route array-map memory to the task pool.
    auto hashTable = std::make_unique<DefaultArrayMap<RowRefListType>>(
        rangeUpperBound, OmniHashmapAllocator(executionContexts[partitionIndex]->GetAllocator()));
    auto &arenaAllocator = *(executionContexts[partitionIndex]->GetArena());
    T key;

    auto buildHashCol = buildHashCols[0];
    auto &vecBatchesOnePartition = inputVecBatches[partitionIndex];
    int32_t vecBatchCount = vecBatchesOnePartition.size();
    for (auto j = 0; j < vecBatchCount; j++) {
        auto vecBatch = vecBatchesOnePartition[j];
        auto baseVector = vecBatch->Get(buildHashCol);
        auto rowCount = vecBatch->GetRowCount();

        if (baseVector->GetEncoding() == OMNI_ENCODING_CONST) {
            if (!baseVector->IsNull(0)) {
                key = static_cast<ConstVector<T> *>(baseVector)->GetConstValue();
                for (auto offset = 0; offset < rowCount; offset++) {
                    auto ret = hashTable->InsertJoinKeysToHashmap(static_cast<size_t>(key - min));
                    ApplyRowRef(ret, static_cast<uint32_t>(offset), static_cast<uint32_t>(j), arenaAllocator);
                }
            } else {
                for (auto offset = 0; offset < rowCount; offset++) {
                    auto ret = hashTable->InsertNullKeysToHashmap();
                    ApplyRowRef(ret, static_cast<uint32_t>(offset), static_cast<uint32_t>(j), arenaAllocator);
                }
            }
            continue;
        }

        auto curVector = reinterpret_cast<Vector<T> *>(baseVector);
        if (!curVector->HasNull()) {
            for (auto offset = 0; offset < rowCount; offset++) {
                key = curVector->GetValue(offset);
                auto ret = hashTable->InsertJoinKeysToHashmap(static_cast<size_t>(key - min));
                ApplyRowRef(ret, static_cast<uint32_t>(offset), static_cast<uint32_t>(j), arenaAllocator);
            }
        } else {
            for (auto offset = 0; offset < rowCount; offset++) {
                bool unNullKey = (!curVector->IsNull(offset));
                if (LIKELY(unNullKey)) {
                    key = curVector->GetValue(offset);
                    auto ret = hashTable->InsertJoinKeysToHashmap(static_cast<size_t>(key - min));
                    ApplyRowRef(ret, static_cast<uint32_t>(offset), static_cast<uint32_t>(j), arenaAllocator);
                } else {
                    auto ret = hashTable->InsertNullKeysToHashmap();
                    ApplyRowRef(ret, static_cast<uint32_t>(offset), static_cast<uint32_t>(j), arenaAllocator);
                }
            }
        }
    }

    arrayTables[partitionIndex] = std::move(hashTable);
    hashTableTypes[partitionIndex] = HashTableImplementationType::ARRAY_HASH_TABLE;
    maxMins[partitionIndex] = std::make_pair(max, min);
    hashTableSize++;
}

template <typename KeyType, typename RowRefListType>
void JoinHashTableVariants<KeyType, RowRefListType>::Prepare(int32_t partitionIndex)
{
    if (buildStateReleased) {
        return;
    }
    auto &vecBatchesOnePartition = inputVecBatches[partitionIndex];
    int32_t vecBatchCount = vecBatchesOnePartition.size();
    uint32_t columnCount = buildTypes->GetSize();

    auto columnsOnePartition = new BaseVector **[columnCount];
    this->columns[partitionIndex] = columnsOnePartition;
    for (uint32_t colIdx = 0; colIdx < columnCount; ++colIdx) {
        columnsOnePartition[colIdx] = new BaseVector *[vecBatchCount];
    }

    for (int32_t vecBatchIdx = 0; vecBatchIdx < vecBatchCount; ++vecBatchIdx) {
        VectorBatch *vecBatch = inputVecBatches[partitionIndex][vecBatchIdx];
        for (uint32_t colIdx = 0; colIdx < columnCount; ++colIdx) {
            auto vector = vecBatch->Get(static_cast<int32_t>(colIdx));
            columnsOnePartition[colIdx][vecBatchIdx] = vector;
        }
    }
}

template <typename KeyType, typename RowRefListType>
void JoinHashTableVariants<KeyType, RowRefListType>::InitBuildFilterCols(std::vector<int32_t> &buildFilterCols,
    int32_t originalProbeColsCount, std::vector<std::vector<BaseVector **>> &tableBuildFilterColPtrs)
{
    // Index by fixed slot count (hashTableCount), not hashTableSize. After Path A / reclaim many
    // slots have columns==nullptr; after single-bucket restore only one slot is live.
    tableBuildFilterColPtrs.assign(hashTableCount, {});
    auto buildFilterColsCount = buildFilterCols.size();

    for (uint32_t hashTableIdx = 0; hashTableIdx < hashTableCount; ++hashTableIdx) {
        auto *partitionColumns = GetColumns(static_cast<int32_t>(hashTableIdx));
        if (partitionColumns == nullptr) {
            continue;
        }
        for (uint32_t i = 0; i < buildFilterColsCount; i++) {
            auto buildFilterCol = buildFilterCols[i] - originalProbeColsCount;
            tableBuildFilterColPtrs[hashTableIdx].emplace_back(partitionColumns[buildFilterCol]);
        }
    }
}

namespace {

// Only integer keys produce a filter. chooseCommonFilter emits BigintRange for a
// contiguous set and BigintValues otherwise, and both are exact membership tests.
// Other types are omitted: boolean/double readers ignore testInt64, a double range
// is a superset and would disagree with canReplace, date32 is rebased after the
// filter, and decimal/timestamp/float readers throw on value filters.
::common::FilterPtr MakeDynamicFilter(int32_t typeId, const std::unordered_set<int64_t> &values)
{
    switch (static_cast<omniruntime::type::DataTypeId>(typeId)) {
        case omniruntime::type::OMNI_BYTE:
        case omniruntime::type::OMNI_SHORT:
        case omniruntime::type::OMNI_INT:
        case omniruntime::type::OMNI_LONG:
            return ::common::chooseCommonFilter(values, /*nullAllowed*/ false);
        default:
            return nullptr;
    }
}

} // namespace

template <typename KeyType, typename RowRefListType>
void JoinHashTableVariants<KeyType, RowRefListType>::BuildDynamicFilters()
{
    if (dynamicFiltersBuilt_) {
        return;
    }
    bool anyTable = false;
    for (uint32_t p = 0; p < hashTableCount; ++p) {
        if (arrayTables[p] || hashTables[p]) {
            anyTable = true;
            break;
        }
    }
    if (!anyTable) {
        return;
    }
    if (joinType != OMNI_JOIN_TYPE_INNER && joinType != OMNI_JOIN_TYPE_LEFT_SEMI &&
        (joinType != OMNI_JOIN_TYPE_RIGHT || buildSide != OMNI_BUILD_RIGHT)) {
        LogDebug("DFP: skip filter generation, unsupported joinType=%d buildSide=%d",
            static_cast<int>(joinType), static_cast<int>(buildSide));
        dynamicFiltersBuilt_ = true;
        return;
    }
    if (isMultiCols || buildHashCols.size() != 1) {
        LogDebug("DFP: skip filter generation, composite/multi-column join key (cols=%zu multi=%d)",
            buildHashCols.size(), static_cast<int>(isMultiCols));
        dynamicFiltersBuilt_ = true;
        return;
    }
    if constexpr (!IS_SIMPLE_KEY) {
        LogDebug("DFP: skip filter generation, key type is not int8/int16/int32/int64");
        dynamicFiltersBuilt_ = true;
        return;
    } else {
        std::unordered_set<int64_t> values;
        bool overflow = false;
        const size_t kMax = ::common::kMaxInListSizeForDynamicFilter;
        for (uint32_t p = 0; p < hashTableCount && !overflow; ++p) {
            if (hashTableTypes[p] == HashTableImplementationType::ARRAY_HASH_TABLE && arrayTables[p]) {
                const int64_t minVal = maxMins[p].second;
                arrayTables[p]->ForEachValue([&](auto *rowRef, int index) {
                    if (index >= 0 && rowRef != nullptr && rowRef->GetRowCount() > 1) {
                        hasDuplicateKeys_ = true;
                    }
                    if (overflow || index < 0) {
                        return;
                    }
                    values.insert(minVal + static_cast<int64_t>(index));
                    if (values.size() > kMax) {
                        overflow = true;
                    }
                });
            } else if (hashTables[p]) {
                hashTables[p]->hashmap.ForEachKV([&](const KeyType &key, auto *rowRef) {
                    if (rowRef != nullptr && rowRef->GetRowCount() > 1) {
                        hasDuplicateKeys_ = true;
                    }
                    if (overflow) {
                        return;
                    }
                    values.insert(static_cast<int64_t>(key));
                    if (values.size() > kMax) {
                        overflow = true;
                    }
                });
            }
        }
        dynamicFiltersBuilt_ = true;
        if (overflow) {
            LogDebug("DFP: skip filter generation, distinct keys > %zu", kMax);
            return;
        }
        const int32_t columnType = buildTypes->GetIds()[buildHashCols[0]];
        auto filter = MakeDynamicFilter(columnType, values);
        if (filter == nullptr) {
            LogDebug("DFP: skip filter generation, column type %d cannot be filtered exactly distinct=%zu",
                columnType, values.size());
            return;
        }
        dynamicFilters_[buildHashCols[0]] = std::move(filter);
        LogDebug("DFP: generated filter buildChannel=%d distinct=%zu kind=%d", buildHashCols[0],
            values.size(), static_cast<int>(dynamicFilters_[buildHashCols[0]]->kind()));
    }
}

// Forward declaration is not encouraged. Only for LCOV.
template class JoinHashTableVariants<int8_t, RowRefList>;
template class JoinHashTableVariants<int16_t, RowRefList>;
template class JoinHashTableVariants<int32_t, RowRefList>;
template class JoinHashTableVariants<int64_t, RowRefList>;
template class JoinHashTableVariants<Decimal128, RowRefList>;
template class JoinHashTableVariants<StringRef, RowRefList>;
template class JoinHashTableVariants<int8_t, RowRefListWithFlags>;
template class JoinHashTableVariants<int16_t, RowRefListWithFlags>;
template class JoinHashTableVariants<int32_t, RowRefListWithFlags>;
template class JoinHashTableVariants<int64_t, RowRefListWithFlags>;
template class JoinHashTableVariants<Decimal128, RowRefListWithFlags>;
template class JoinHashTableVariants<StringRef, RowRefListWithFlags>;
template class JoinHashTableVariants<int128_t, RowRefList>;
template class JoinHashTableVariants<int128_t, RowRefListWithFlags>;
}
}
