/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 * Description: CollectSet aggregation (collect_set_aggregator implementation).
 */

#include "collect_set_aggregator.h"
#include "vector/vector_helper.h"
#include "vector/nulls_buffer.h"
#include "util/debug.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>

namespace omniruntime::op {

static std::shared_ptr<NullsHelper> SliceNullMapForSingleRow(const std::shared_ptr<NullsHelper> &nullMap,
    int32_t rowIdx)
{
    if (nullMap == nullptr) {
        return nullptr;
    }
    auto nullsBuffer = std::make_shared<vec::NullsBuffer>(1);
    auto rowNullMap = std::make_shared<NullsHelper>(nullsBuffer);
    if ((*nullMap)[rowIdx]) {
        rowNullMap->SetNull(0, true);
    }
    return rowNullMap;
}

/* Crash-hunt diagnostics (stderr traces + per-row three-state guard) are gated
 * behind OMNI_HDIAG=1: they only run when explicitly re-enabled for a crash
 * investigation. The per-row address-book lookup in ProcessGroupInternal costs
 * one unordered_set::find per row per aggregator, which dominated runtime in
 * production measurements and skewed countDistinctWithoutExpand benchmarks. */
static inline bool CsDiagEnabled()
{
    static const bool enabled = [] {
        const char *env = std::getenv("OMNI_HDIAG");
        return env != nullptr && env[0] != '\0' && env[0] != '0';
    }();
    return enabled;
}

#define CS_DIAG(fmt, ...)                                                                  \
    do {                                                                                   \
        if (CsDiagEnabled()) {                                                             \
            fprintf(stderr, "[HDIAG][CollectSet][%s] " fmt "\n", __func__, ##__VA_ARGS__);\
            fflush(stderr);                                                                \
        }                                                                                  \
    } while (0)

/** Initial capacity (2^4 = 16 slots) of the per-group distinct set hash map.
 * Kept small on purpose: with tens of millions of groups the default initial
 * degree (15 -> 32768 slots, ~288KB per group) exhausts memory before spill
 * thresholds can trigger. Sets grow via Rehash on demand. */
static constexpr uint8_t AGG_SET_INIT_DEGREE = 4;

/**
 * we have a Set in state (keySet of the DefaultHashMap)
 * we will iterate over the Set and store the element in the arrayVector.
 *
 * @param state a global agg state for non group agg.
 * @param vectors Result vectors (one per output); vectors[0] must be an ArrayVector for CollectSet.
 * @param rowIndex Normally 0 for non-group aggregation.
 */
template<DataTypeId IN_ID, DataTypeId OUT_ID>
void CollectSetAggregator<IN_ID, OUT_ID>::ExtractValues(const AggregateState *state, std::vector<BaseVector *> &vectors, int32_t rowIndex) {
    auto v = static_cast<ArrayVector *>(vectors[0]);

    using stateType = typename AggNativeAndVectorType<IN_ID>::type;
    const SetState<stateType> *setState = SetState<stateType>::ConstCastState(state + aggStateOffset);

    DefaultHashMap<stateType, int8_t>* uniqueValues = reinterpret_cast<DefaultHashMap<stateType, int8_t> *>(setState->uniqueValuesAddr);

    if (uniqueValues->GetElementsSize() == 0) {
        // Spark: empty collect_set is [] not null; avoids ColumnarBatchToInternalRow / UnsafeProjection NPE
        auto elementVector = static_cast<Vector<stateType> *>(VectorHelper::CreateVector(OMNI_FLAT, IN_ID, 0));
        v->SetValue(rowIndex, elementVector);
        delete elementVector;
        return;
    }
    auto elementSize = uniqueValues->GetElementsSize();

    auto elementVector = static_cast<Vector<stateType> *>(VectorHelper::CreateVector(OMNI_FLAT, IN_ID, elementSize));

    int32_t index = 0;
    uniqueValues->ForEachKV([&](const stateType &key, const int8_t &value) {
        elementVector->SetValue(index, key);
        index++;
    });

    v->SetValue(rowIndex, elementVector);
    delete elementVector;
}

/**
 * we have Sets in states (keySet of the DefaultHashMap)
 * we will iterate over the Sets and store the elements in an ArrayVector.
 * Groupkey1 -> SetState{ Set[1,2,3,4] }
 * GroupKey2 -> SetState{ Set[5,6,7,8] }
 * result: ArrayVector[ [1,2,3,4], [5,6,7,8] ]
 *
 * @param groupStates One aggregate state per group (length equals number of group keys).
 * @param vectors Result vectors (one per output); vectors[0] must be an ArrayVector for CollectSet.
 * @param rowOffset Start row index in the result (normally 0).
 * @param rowCount Number of groups (same as groupBy keys number).
 */
template<DataTypeId IN_ID, DataTypeId OUT_ID>
void CollectSetAggregator<IN_ID, OUT_ID>::ExtractValuesBatch(std::vector<AggregateState *> &groupStates, std::vector<BaseVector *> &vectors, int32_t rowOffset, int32_t rowCount) {
    using stateType = typename AggNativeAndVectorType<IN_ID>::type;
    auto v = static_cast<ArrayVector *>(vectors[0]);
    for (int32_t rowIndex = 0; rowIndex < rowCount; rowIndex++) {
        const SetState<stateType> *setState = SetState<stateType>::ConstCastState(groupStates[rowIndex] + aggStateOffset);

        DefaultHashMap<stateType, int8_t>* uniqueValues = reinterpret_cast<DefaultHashMap<stateType, int8_t> *>(setState->uniqueValuesAddr);

        if (uniqueValues->GetElementsSize() == 0) {
            auto elementVector = static_cast<Vector<stateType> *>(VectorHelper::CreateVector(OMNI_FLAT, IN_ID, 0));
            v->SetValue(rowOffset + rowIndex, elementVector);
            delete elementVector;
            continue;
        }

        auto elementSize = uniqueValues->GetElementsSize();
        auto elementVector = static_cast<Vector<stateType> *>(VectorHelper::CreateVector(OMNI_FLAT, IN_ID, elementSize));

        int32_t index = 0;
        uniqueValues->ForEachKV([&](const stateType &key, const int8_t &value) {
            elementVector->SetValue(index, key);
            index++;
        });

        v->SetValue(rowOffset + rowIndex, elementVector);
        delete elementVector;
    }
}

/**
 * Returns the data type of the spill column. CollectSet spills one column of type array(element type).
 * @return Vector of one DataTypePtr: the aggregator output type (OMNI_ARRAY of element type).
 */
template<DataTypeId IN_ID, DataTypeId OUT_ID>
std::vector<DataTypePtr> CollectSetAggregator<IN_ID, OUT_ID>::GetSpillType() {
    std::vector<DataTypePtr> spillTypes;
    spillTypes.push_back(GetOutputTypes().GetType(0));
    return spillTypes;
}

/**
 * Extracts each group's set from aggregate states into the spill vectors (one array column).
 * Same semantics as ExtractValuesBatch: each row of vectors[0] receives the array of distinct values for that group.
 * @param groupStates Per-group aggregate states to spill.
 * @param vectors Pre-allocated spill vectors; vectors[0] must be an ArrayVector.
 */
template<DataTypeId IN_ID, DataTypeId OUT_ID>
void CollectSetAggregator<IN_ID, OUT_ID>::ExtractValuesForSpill(std::vector<AggregateState *> &groupStates, std::vector<BaseVector *> &vectors) {
    ExtractValuesBatch(groupStates, vectors, 0, static_cast<int32_t>(groupStates.size()));
}

/**
 * Initializes one aggregate state: allocates a DefaultHashMap for the set and stores its address in the state.
 * @param state Single state to initialize (caller provides buffer of at least GetStateSize() bytes at state + aggStateOffset).
 */
template<DataTypeId IN_ID, DataTypeId OUT_ID>
void CollectSetAggregator<IN_ID, OUT_ID>::InitState(AggregateState *state) {
    SetState<InType> *setState = SetState<InType>::CastState(state + aggStateOffset);
    using stateType = typename AggNativeAndVectorType<IN_ID>::type;
    // Initial degree 4 (16 slots, a few hundred bytes) instead of the default 15
    // (32768 slots, ~288KB): per-group distinct sets usually hold few values, and
    // with tens of millions of groups the default footprint alone exhausts memory
    // long before spill thresholds are reached. The map still grows on demand via
    // Rehash (degree += 2 per growth), so large sets are unaffected.
    DefaultHashMap<stateType, int8_t>* uniqueValues = new DefaultHashMap<stateType, int8_t>(AGG_SET_INIT_DEGREE);
    uniqueValues->Reset();
    setState->uniqueValuesAddr = reinterpret_cast<int64_t>(uniqueValues);
    // The map's slot/control arrays go through OmniHashmapAllocator and are already
    // accounted; only the heap object header itself is invisible to the memory
    // manager, so report it here (and reclaim it in DestroyState) to keep global
    // accounting honest for spill decisions with tens of millions of groups.
    mem::ThreadMemoryManager::ReportMemory(sizeof(DefaultHashMap<stateType, int8_t>));
    allocatedUniqueValuesAddrs_.insert(setState->uniqueValuesAddr);
    // Throttled trace: confirms every live state got a map address that is
    // registered in the address book. Cross-check against PGI/DESTROY lines.
    static thread_local int64_t diagInitCnt = 0;
    if (diagInitCnt < 2 || (diagInitCnt % 16384) == 0) {
        CS_DIAG("cnt=%lld state=%p aggOff=%d mapAddr=%p bookSize=%zu",
                static_cast<long long>(diagInitCnt), reinterpret_cast<void *>(state),
                aggStateOffset, reinterpret_cast<void *>(uniqueValues),
                allocatedUniqueValuesAddrs_.size());
    }
    diagInitCnt++;
}

/**
 * Initializes all group states (one set per group).
 * @param groupStates Vector of per-group state pointers to initialize.
 */
template<DataTypeId IN_ID, DataTypeId OUT_ID>
void CollectSetAggregator<IN_ID, OUT_ID>::InitStates(std::vector<AggregateState *> &groupStates) {
    for (auto state : groupStates) {
        InitState(state);
    }
}

/**
 * Frees the DefaultHashMap allocated in InitState and removes it from the tracked list (so destructor won't double-free).
 * Call before freeing the state buffer if you want to release early; otherwise destructor will free all tracked maps.
 */
template<DataTypeId IN_ID, DataTypeId OUT_ID>
void CollectSetAggregator<IN_ID, OUT_ID>::DestroyState(AggregateState *state) {
    using stateType = typename AggNativeAndVectorType<IN_ID>::type;
    SetState<stateType> *setState = SetState<stateType>::CastState(state + aggStateOffset);
    int64_t addr = setState->uniqueValuesAddr;
    if (addr == 0) {
        return;
    }
    DefaultHashMap<stateType, int8_t> *uniqueValues = reinterpret_cast<DefaultHashMap<stateType, int8_t> *>(addr);
    delete uniqueValues;
    mem::ThreadMemoryManager::ReclaimMemory(sizeof(DefaultHashMap<stateType, int8_t>));
    setState->uniqueValuesAddr = 0;
    allocatedUniqueValuesAddrs_.erase(addr);
}

/**
 * Frees every DefaultHashMap tracked by this aggregator and clears the address book.
 * Called by the aggregation operator after a successful spill, before the arena and
 * hash tables are reset; also safe to call when the book is already empty.
 */
template<DataTypeId IN_ID, DataTypeId OUT_ID>
void CollectSetAggregator<IN_ID, OUT_ID>::DestroyAllStates() {
    using stateType = typename AggNativeAndVectorType<IN_ID>::type;
    // Unconditional: this runs once per spill, so volume is tiny. If the next
    // PGI FATAL line reports a dangling addr that once appeared in an INIT line,
    // the free happened here while arena/rows survived -> lifecycle bug.
    CS_DIAG("destroying %zu maps (aggOff=%d)", allocatedUniqueValuesAddrs_.size(), aggStateOffset);
    for (int64_t addr : allocatedUniqueValuesAddrs_) {
        delete reinterpret_cast<DefaultHashMap<stateType, int8_t> *>(addr);
        mem::ThreadMemoryManager::ReclaimMemory(sizeof(DefaultHashMap<stateType, int8_t>));
    }
    allocatedUniqueValuesAddrs_.clear();
}

/**
 * Processes a contiguous range of input rows for a single state (non-grouped aggregation).
 * Partial: adds each non-null value from the flat/dictionary vector into the state's set.
 * Final: merges each non-null array element from the ArrayVector into the state's set.
 * @param state The aggregate state to update.
 * @param vector Input vector (flat/dictionary for partial, ArrayVector for final).
 * @param rowOffset Start row index in the vector.(Normally 0)
 * @param rowCount Number of rows to process.
 * @param nullMap Optional null map; if null, all rows are considered non-null.
 */
template<DataTypeId IN_ID, DataTypeId OUT_ID>
void CollectSetAggregator<IN_ID, OUT_ID>::ProcessSingleInternal(AggregateState *state, BaseVector *vector, const int32_t rowOffset, const int32_t rowCount, const std::shared_ptr<NullsHelper> nullMap) {
    if (rowCount == 0) {
        return;
    }

    bool isDictionary = vector->GetEncoding() == vec::OMNI_DICTIONARY;
    bool isConst = vector->GetEncoding() == vec::OMNI_ENCODING_CONST;
    using stateType = typename AggNativeAndVectorType<IN_ID>::type;
    // Caller (TypedAggregator::ProcessGroup) already passes state + aggStateOffset for this aggregator.
    SetState<stateType> *setState = SetState<stateType>::CastState(state);
    if (IsInputRaw()) {
        if (isConst) {
            auto constVector = static_cast<ConstVector<stateType> *>(vector);
            if (constVector->IsNull(0)) {
                return;
            }
            stateType value = constVector->GetConstValue();
            DefaultHashMap<stateType, int8_t>* uniqueValues =
                reinterpret_cast<DefaultHashMap<stateType, int8_t> *>(setState->uniqueValuesAddr);
            for (int32_t i = 0; i < rowCount; i++) {
                if (nullMap == nullptr || !(*nullMap)[i]) {
                    uniqueValues->Emplace(value);
                }
            }
        } else {
            setState->UpdatePartialState(vector, rowCount, nullMap, isDictionary, rowOffset);
        }
    } else {
        auto arrayVector = static_cast<ArrayVector *>(vector);
        setState->UpdateFinalState(arrayVector, rowCount, nullMap, isDictionary, rowOffset);
    }
}

/**
 * Processes one row per group: for each group state, updates its set with the input at the corresponding row.
 * Partial: each row state gets the single value at rowOffset + rowIndex.
 * Final: each row state merges the array at rowOffset + rowIndex into its set.
 * @param rowStates One state per group (same order as rows).
 * @param vector Input vector (flat/dictionary or ArrayVector).
 * @param rowOffset Start row index. (Normally 0)
 * @param nullMap Optional null map.
 */
template<DataTypeId IN_ID, DataTypeId OUT_ID>
void CollectSetAggregator<IN_ID, OUT_ID>::ProcessGroupInternal(std::vector<AggregateState *> &rowStates, BaseVector *vector, const int32_t rowOffset, const std::shared_ptr<NullsHelper> nullMap) {
    const size_t rowCount = rowStates.size();
    if (rowCount == 0) {
        return;
    }

    // Throttled entry trace: captures the exact state pointers of the batch
    // being processed. If the crash happens while dereferencing a state, the
    // last ENTRY line before the crash identifies the poisoned pointer.
    static thread_local int64_t diagPgiCnt = 0;
    if (diagPgiCnt < 4 || (diagPgiCnt % 2048) == 0) {
        using stateType = typename AggNativeAndVectorType<IN_ID>::type;
        SetState<stateType> *setState0 = SetState<stateType>::CastState(rowStates[0] + aggStateOffset);
        CS_DIAG("cnt=%lld rows=%zu vec=%p enc=%d rowOff=%d state0=%p addr0=%p bookSize=%zu",
                static_cast<long long>(diagPgiCnt), rowCount, reinterpret_cast<void *>(vector),
                static_cast<int>(vector->GetEncoding()), rowOffset,
                reinterpret_cast<void *>(rowStates[0]),
                reinterpret_cast<void *>(setState0->uniqueValuesAddr),
                allocatedUniqueValuesAddrs_.size());
    }
    diagPgiCnt++;

    // Three-state guard on every row state (diagnostics only, OMNI_HDIAG=1):
    //   addr == 0                  -> never initialized (arena zero-fill residue)
    //   addr != 0, not in book     -> dangling (freed by DestroyAllStates) or
    //                                 overwritten by a foreign writer.
    //   addr in book               -> healthy.
    // Guarded: the unordered_set::find per row per aggregator dominated runtime
    // in production measurements; in default mode only the cheap addr == 0
    // null check below (per Emplace target) remains in the hot loop.
    if (UNLIKELY(CsDiagEnabled())) {
        using stateType = typename AggNativeAndVectorType<IN_ID>::type;
        for (size_t rowIndex = 0; rowIndex < rowCount; rowIndex++) {
            SetState<stateType> *setState = SetState<stateType>::CastState(rowStates[rowIndex] + aggStateOffset);
            if (UNLIKELY(setState->uniqueValuesAddr == 0)) {
                CS_DIAG("FATAL uninitialized state: rowIndex=%zu rowCount=%zu state=%p rowOffset=%d "
                        "encoding=%d isRaw=%d bookSize=%zu",
                        rowIndex, rowCount, reinterpret_cast<void *>(rowStates[rowIndex]),
                        rowOffset, static_cast<int>(vector->GetEncoding()),
                        IsInputRaw() ? 1 : 0, allocatedUniqueValuesAddrs_.size());
                throw omniruntime::exception::OmniException("SPILL_FAILED",
                    "CollectSet state uniqueValuesAddr is null (state never initialized)");
            }
            if (UNLIKELY(allocatedUniqueValuesAddrs_.find(setState->uniqueValuesAddr) ==
                         allocatedUniqueValuesAddrs_.end())) {
                CS_DIAG("FATAL dangling/foreign state: rowIndex=%zu rowCount=%zu state=%p addr=%p "
                        "aggOff=%d bookSize=%zu (addr allocated by InitState but not tracked -> "
                        "freed by DestroyAllStates or overwritten)",
                        rowIndex, rowCount, reinterpret_cast<void *>(rowStates[rowIndex]),
                        reinterpret_cast<void *>(setState->uniqueValuesAddr),
                        aggStateOffset, allocatedUniqueValuesAddrs_.size());
                throw omniruntime::exception::OmniException("SPILL_FAILED",
                    "CollectSet state uniqueValuesAddr is dangling/untracked");
            }
        }
    }

    bool isDictionary = vector->GetEncoding() == vec::OMNI_DICTIONARY;
    bool isConst = vector->GetEncoding() == vec::OMNI_ENCODING_CONST;

    if (IsInputRaw() && isConst) {
        using stateType = typename AggNativeAndVectorType<IN_ID>::type;
        auto constVector = static_cast<ConstVector<stateType> *>(vector);
        if (constVector->IsNull(0)) {
            return;
        }
        stateType value = constVector->GetConstValue();
        for (size_t rowIndex = 0; rowIndex < rowCount; rowIndex++) {
            SetState<stateType> *setState = SetState<stateType>::CastState(rowStates[rowIndex] + aggStateOffset);
            DefaultHashMap<stateType, int8_t>* uniqueValues =
                reinterpret_cast<DefaultHashMap<stateType, int8_t> *>(setState->uniqueValuesAddr);
            if (nullMap == nullptr || !(*nullMap)[rowIndex]) {
                uniqueValues->Emplace(value);
            }
        }
        return;
    }

    // basically rowOffset is fix 0 now.
    int32_t offset = rowOffset;
    for (size_t rowIndex = 0; rowIndex < rowCount; rowIndex++) {
        using stateType = typename AggNativeAndVectorType<IN_ID>::type;
        SetState<stateType> *setState = SetState<stateType>::CastState(rowStates[rowIndex] + aggStateOffset);
        if (IsInputRaw()) {
            BaseVector *sliced = vector->Slice(offset, 1);
            auto rowNullMap = SliceNullMapForSingleRow(nullMap, offset);
            setState->UpdatePartialState(sliced, 1, rowNullMap, isDictionary, 0);
            delete sliced;
        } else {
            auto arrayVector = reinterpret_cast<ArrayVector *>(vector);
            BaseVector *sliced = arrayVector->Slice(offset, 1);
            auto rowNullMap = SliceNullMapForSingleRow(nullMap, offset);
            setState->UpdateFinalState(static_cast<ArrayVector *>(sliced), 1, rowNullMap, isDictionary, 0);
            delete sliced;
        }
        ++offset;
    }
}

/**
 * Merges spilled rows back into group states. For each unspill row, reads the spilled array from the batch
 * and adds each element into that group's set state.
 * @param unspillRows Rows to merge (state, batch, rowIdx per row).
 * @param rowCount Number of rows in unspillRows.
 * @param vectorIndex Index of this aggregator's spill column in the batch; incremented by one on return.
 */
template<DataTypeId IN_ID, DataTypeId OUT_ID>
void CollectSetAggregator<IN_ID, OUT_ID>::ProcessGroupUnspill(std::vector<UnspillRowInfo> &unspillRows, int32_t rowCount, int32_t &vectorIndex) {
    int32_t arrayVecIdx = vectorIndex++;
    using stateType = typename AggNativeAndVectorType<IN_ID>::type;
    // Throttled trace of the unspill merge path.
    static thread_local int64_t diagUnspillCnt = 0;
    if (diagUnspillCnt < 2 || (diagUnspillCnt % 1024) == 0) {
        CS_DIAG("cnt=%lld rows=%d vecIdx=%d", static_cast<long long>(diagUnspillCnt),
                rowCount, arrayVecIdx);
    }
    diagUnspillCnt++;
    for (int32_t rowIdx = 0; rowIdx < rowCount; rowIdx++) {
        UnspillRowInfo &row = unspillRows[rowIdx];
        if (row.batch == nullptr || arrayVecIdx >= row.batch->GetVectorCount()) {
            continue;
        }
        BaseVector *arrayVec = row.batch->Get(arrayVecIdx);
        if (arrayVec == nullptr || arrayVec->IsNull(row.rowIdx)) {
            continue;
        }
        ArrayVector *arrVec = static_cast<ArrayVector *>(arrayVec);
        std::shared_ptr<BaseVector> elementVecHolder = arrVec->GetArrayAt(row.rowIdx, false);
        BaseVector *elementVec = elementVecHolder.get();
        if (elementVec == nullptr || elementVec->GetSize() == 0) {
            continue;
        }
        SetState<stateType> *setState = SetState<stateType>::CastState(row.state + aggStateOffset);
        DefaultHashMap<stateType, int8_t> *uniqueValues =
            reinterpret_cast<DefaultHashMap<stateType, int8_t> *>(setState->uniqueValuesAddr);
        if (UNLIKELY(uniqueValues == nullptr)) {
            LogError("CollectSet unspill uninitialized state: rowIdx=%d, stateAddr=%p, aggOffset=%d",
                     rowIdx, reinterpret_cast<void *>(row.state), aggStateOffset);
            throw omniruntime::exception::OmniException("SPILL_FAILED",
                "CollectSet unspill state uniqueValuesAddr is null");
        }
        auto *flatElementVec = static_cast<Vector<stateType> *>(elementVec);
        stateType *ptr = reinterpret_cast<stateType *>(unsafe::UnsafeVector::GetRawValues<stateType>(flatElementVec));
        for (int32_t j = 0, n = elementVec->GetSize(); j < n; j++) {
            if (!elementVec->IsNull(j)) {
                uniqueValues->Emplace(ptr[j]);
            }
        }
    }
}

/**
 * Aligns partial aggregate result schema (Skip Partial): converts input column into the intermediate
 * aggregation state format (array of distinct values per row) and appends it to result.
 * - When input is raw (scalar column): each row's value becomes a single-element array [value].
 * - When input is partial (array column): each row is already partial state, so we pass through.
 */
template<DataTypeId IN_ID, DataTypeId OUT_ID>
void CollectSetAggregator<IN_ID, OUT_ID>::ProcessAlignAggSchema(VectorBatch *vecBatch, BaseVector *originVector, const std::shared_ptr<NullsHelper> nullMap, const bool aggFilter) {
    const int32_t rowCount = (originVector != nullptr) ? originVector->GetSize() : 0;
    if (rowCount == 0) {
        auto *emptyArr = static_cast<ArrayVector *>(VectorHelper::CreateComplexVector(GetOutputTypes().GetType(0).get(), 0));
        vecBatch->Append(emptyArr);
        return;
    }
    if (!IsInputRaw()) {
        vecBatch->Append(VectorHelper::SliceVector(originVector, 0, rowCount));
        return;
    }
    if (originVector->GetEncoding() == OMNI_DICTIONARY) {
        ProcessAlignAggSchemaInteranal<Vector<DictionaryContainer<InType>>>(vecBatch, originVector, nullMap);
    } else {
        ProcessAlignAggSchemaInteranal<Vector<InType>>(vecBatch, originVector, nullMap);
    }
}

/**
 * Internal: for raw input, builds an ArrayVector where each row is a single-element array [originVector[i]],
 * or null if nullMap[i] is set. This is the partial state format (one distinct value per row).
 */
template<DataTypeId IN_ID, DataTypeId OUT_ID>
template<typename T>
void CollectSetAggregator<IN_ID, OUT_ID>::ProcessAlignAggSchemaInteranal(VectorBatch *result, BaseVector *originVector, const std::shared_ptr<NullsHelper> nullMap) {
    using stateType = typename AggNativeAndVectorType<IN_ID>::type;
    const int32_t rowCount = originVector->GetSize();
    auto *arrayVector = static_cast<ArrayVector *>(VectorHelper::CreateComplexVector(GetOutputTypes().GetType(0).get(), rowCount));

    int32_t nonNullCount = 0;
    for (int32_t i = 0; i < rowCount; i++) {
        if ((nullMap == nullptr || !(*nullMap)[i]) && !originVector->IsNull(i)) {
            nonNullCount++;
        }
    }

    auto *elementVector = static_cast<Vector<stateType> *>(VectorHelper::CreateVector(OMNI_FLAT, IN_ID, nonNullCount));
    int32_t elemIdx = 0;
    auto *vector = reinterpret_cast<T *>(originVector);

    for (int32_t i = 0; i < rowCount; i++) {
        if ((nullMap != nullptr && (*nullMap)[i]) || originVector->IsNull(i)) {
            arrayVector->SetNull(i);
            arrayVector->SetSize(i, 0);
        } else {
            stateType val;
            if constexpr (std::is_same_v<T, Vector<stateType>>) {
                val = vector->GetValue(i);
            } else {
                auto *dictVector = reinterpret_cast<Vector<DictionaryContainer<stateType>> *>(originVector);
                stateType *valuePtr = unsafe::UnsafeDictionaryVector::GetDictionary(dictVector);
                const int32_t *ids = unsafe::UnsafeDictionaryVector::GetIds(dictVector);
                val = valuePtr[ids[i]];
            }
            elementVector->SetValue(elemIdx, val);
            elemIdx++;
            arrayVector->SetSize(i, 1);
        }
    }

    arrayVector->SetElementVector(std::shared_ptr<BaseVector>(elementVector));
    result->Append(arrayVector);
}

template<DataTypeId IN_ID, DataTypeId OUT_ID>
CollectSetAggregator<IN_ID, OUT_ID>::~CollectSetAggregator() {
    using stateType = typename AggNativeAndVectorType<IN_ID>::type;
    for (int64_t addr : allocatedUniqueValuesAddrs_) {
        delete reinterpret_cast<DefaultHashMap<stateType, int8_t> *>(addr);
        mem::ThreadMemoryManager::ReclaimMemory(sizeof(DefaultHashMap<stateType, int8_t>));
    }
    allocatedUniqueValuesAddrs_.clear();
}

template<DataTypeId IN_ID, DataTypeId OUT_ID>
CollectSetAggregator<IN_ID, OUT_ID>::CollectSetAggregator(
    const DataTypes &inputTypes,
    const DataTypes &outputTypes,
    const std::vector<int32_t> &channels,
    const bool inputRaw,
    const bool outputPartial,
    const bool isOverflowAsNull
    ) : TypedAggregator(OMNI_AGGREGATION_TYPE_COLLECT_SET, inputTypes, outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull) {

}

template<DataTypeId IN_ID, DataTypeId OUT_ID>
CollectSetAggregator<IN_ID, OUT_ID>::CollectSetAggregator(
    const FunctionType aggregateType,
    const DataTypes &inputTypes,
    const DataTypes &outputTypes,
    const std::vector<int32_t> &channels,
    const bool inputRaw,
    const bool outputPartial,
    const bool isOverflowAsNull
    ) : TypedAggregator(aggregateType, inputTypes, outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull) {

}

template class CollectSetAggregator<OMNI_BOOLEAN, OMNI_BOOLEAN>;
template class CollectSetAggregator<OMNI_BYTE, OMNI_BYTE>;
template class CollectSetAggregator<OMNI_SHORT, OMNI_SHORT>;
template class CollectSetAggregator<OMNI_INT, OMNI_INT>;
template class CollectSetAggregator<OMNI_LONG, OMNI_LONG>;
template class CollectSetAggregator<OMNI_DATE32, OMNI_DATE32>;
template class CollectSetAggregator<OMNI_DATE64, OMNI_DATE64>;
template class CollectSetAggregator<OMNI_TIMESTAMP, OMNI_TIMESTAMP>;
template class CollectSetAggregator<OMNI_FLOAT, OMNI_FLOAT>;
template class CollectSetAggregator<OMNI_DOUBLE, OMNI_DOUBLE>;
template class CollectSetAggregator<OMNI_DECIMAL64, OMNI_DECIMAL64>;
template class CollectSetAggregator<OMNI_DECIMAL128, OMNI_DECIMAL128>;
// CHAR/VARCHAR/VARBINARY/ARRAY: key type has no GroupbyHashCalculator, DefaultHashMap not supported. Factory throws for these.

} //omniruntim::op
