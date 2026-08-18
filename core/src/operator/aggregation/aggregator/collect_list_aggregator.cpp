/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 * Description: CollectList aggregation (collect_list_aggregator implementation).
 */

#include "collect_list_aggregator.h"
#include "vector/vector_helper.h"
#include "vector/nulls_buffer.h"
#include <algorithm>

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

/**
 * We have a List (std::vector<T>) in state; iterate over it and store the elements in the arrayVector (order preserved).
 *
 * @param state a global agg state for non group agg.
 * @param vectors Result vectors (one per output); vectors[0] must be an ArrayVector for CollectList.
 * @param rowIndex Normally 0 for non-group aggregation.
 */
template <DataTypeId IN_ID, DataTypeId OUT_ID>
void CollectListAggregator<IN_ID, OUT_ID>::ExtractValues(const AggregateState *state, std::vector<BaseVector *> &vectors, int32_t rowIndex) {
    auto v = static_cast<ArrayVector *>(vectors[0]);
    using stateType = typename AggNativeAndVectorType<IN_ID>::type;
    const ListState<stateType> *listState = ListState<stateType>::ConstCastState(state + aggStateOffset);
    std::vector<stateType> *list = reinterpret_cast<std::vector<stateType> *>(listState->listAddr);
    if (list->empty()) {
        auto elementVector = static_cast<Vector<stateType> *>(VectorHelper::CreateVector(OMNI_FLAT, IN_ID, 0));
        v->SetValue(rowIndex, elementVector);
        delete elementVector;
        return;
    }
    size_t elementSize = list->size();
    auto elementVector = static_cast<Vector<stateType> *>(VectorHelper::CreateVector(OMNI_FLAT, IN_ID, static_cast<int32_t>(elementSize)));
    for (size_t i = 0; i < elementSize; i++) {
        elementVector->SetValue(static_cast<int32_t>(i), (*list)[i]);
    }
    v->SetValue(rowIndex, elementVector);
    delete elementVector;
}

/**
 * We have Lists in states (one std::vector<T> per group); iterate over each list and store the elements in an ArrayVector.
 * Groupkey1 -> ListState{ List[1,2,1,3] }
 * GroupKey2 -> ListState{ List[5,6,5,8] }
 * result: ArrayVector[ [1,2,1,3], [5,6,5,8] ]  (order and duplicates preserved)
 *
 * @param groupStates One aggregate state per group (length equals number of group keys).
 * @param vectors Result vectors (one per output); vectors[0] must be an ArrayVector for CollectList.
 * @param rowOffset Start row index in the result (normally 0).
 * @param rowCount Number of groups (same as groupBy keys number).
 */
template <DataTypeId IN_ID, DataTypeId OUT_ID>
void CollectListAggregator<IN_ID, OUT_ID>::ExtractValuesBatch(std::vector<AggregateState *> &groupStates, std::vector<BaseVector *> &vectors, int32_t rowOffset, int32_t rowCount) {
    using stateType = typename AggNativeAndVectorType<IN_ID>::type;
    auto v = static_cast<ArrayVector *>(vectors[0]);
    for (int32_t rowIndex = 0; rowIndex < rowCount; rowIndex++) {
        const ListState<stateType> *listState = ListState<stateType>::ConstCastState(groupStates[rowIndex] + aggStateOffset);
        std::vector<stateType> *list = reinterpret_cast<std::vector<stateType> *>(listState->listAddr);
        if (list->empty()) {
            auto elementVector = static_cast<Vector<stateType> *>(VectorHelper::CreateVector(OMNI_FLAT, IN_ID, 0));
            v->SetValue(rowOffset + rowIndex, elementVector);
            delete elementVector;
            continue;
        }
        size_t elementSize = list->size();
        auto elementVector = static_cast<Vector<stateType> *>(VectorHelper::CreateVector(OMNI_FLAT, IN_ID, static_cast<int32_t>(elementSize)));
        for (size_t i = 0; i < elementSize; i++) {
            elementVector->SetValue(static_cast<int32_t>(i), (*list)[i]);
        }
        v->SetValue(rowOffset + rowIndex, elementVector);
        delete elementVector;
    }
}

/**
 * Returns the data type of the spill column. CollectList spills one column of type array(element type).
 * @return Vector of one DataTypePtr: the aggregator output type (OMNI_ARRAY of element type).
 */
template <DataTypeId IN_ID, DataTypeId OUT_ID>
std::vector<DataTypePtr> CollectListAggregator<IN_ID, OUT_ID>::GetSpillType() {
    std::vector<DataTypePtr> spillTypes;
    spillTypes.push_back(GetOutputTypes().GetType(0));
    return spillTypes;
}

/**
 * Extracts each group's list from aggregate states into the spill vectors (one array column).
 * Same semantics as ExtractValuesBatch: each row of vectors[0] receives the array of values for that group (order preserved).
 * @param groupStates Per-group aggregate states to spill.
 * @param vectors Pre-allocated spill vectors; vectors[0] must be an ArrayVector.
 */
template <DataTypeId IN_ID, DataTypeId OUT_ID>
void CollectListAggregator<IN_ID, OUT_ID>::ExtractValuesForSpill(std::vector<AggregateState *> &groupStates, std::vector<BaseVector *> &vectors) {
    ExtractValuesBatch(groupStates, vectors, 0, static_cast<int32_t>(groupStates.size()));
}

/**
 * Initializes one aggregate state: allocates a std::vector<T> for the list and stores its address in the state.
 * @param state Single state to initialize (caller provides buffer of at least GetStateSize() bytes at state + aggStateOffset).
 */
template <DataTypeId IN_ID, DataTypeId OUT_ID>
void CollectListAggregator<IN_ID, OUT_ID>::InitState(AggregateState *state) {
    ListState<InType> *listState = ListState<InType>::CastState(state + aggStateOffset);
    using stateType = typename AggNativeAndVectorType<IN_ID>::type;
    std::vector<stateType> *list = new std::vector<stateType>();
    listState->listAddr = reinterpret_cast<int64_t>(list);
    allocatedListAddrs_.push_back(listState->listAddr);
}

/**
 * Frees the std::vector<T> allocated in InitState and removes it from the tracked list (so destructor won't double-free).
 * Call before freeing the state buffer if you want to release early; otherwise destructor will free all tracked lists.
 */
template <DataTypeId IN_ID, DataTypeId OUT_ID>
void CollectListAggregator<IN_ID, OUT_ID>::DestroyState(AggregateState *state) {
    using stateType = typename AggNativeAndVectorType<IN_ID>::type;
    ListState<stateType> *listState = ListState<stateType>::CastState(state + aggStateOffset);
    int64_t addr = listState->listAddr;
    if (addr == 0) {
        return;
    }
    std::vector<stateType> *list = reinterpret_cast<std::vector<stateType> *>(addr);
    delete list;
    listState->listAddr = 0;
    auto it = std::find(allocatedListAddrs_.begin(), allocatedListAddrs_.end(), addr);
    if (it != allocatedListAddrs_.end()) {
        allocatedListAddrs_.erase(it);
    }
}

/**
 * Initializes all group states (one list per group).
 * @param groupStates Vector of per-group state pointers to initialize.
 */
template <DataTypeId IN_ID, DataTypeId OUT_ID>
void CollectListAggregator<IN_ID, OUT_ID>::InitStates(std::vector<AggregateState *> &groupStates) {
    for (auto state : groupStates) {
        InitState(state);
    }
}

/**
 * Processes a contiguous range of input rows for a single state (non-grouped aggregation).
 * Partial: appends each non-null value from the flat/dictionary vector into the state's list (order preserved).
 * Final: appends each non-null array element from the ArrayVector into the state's list.
 * @param state The aggregate state to update.
 * @param vector Input vector (flat/dictionary for partial, ArrayVector for final).
 * @param rowOffset Start row index in the vector. (Normally 0)
 * @param rowCount Number of rows to process.
 * @param nullMap Optional null map; if null, all rows are considered non-null.
 */
template <DataTypeId IN_ID, DataTypeId OUT_ID>
void CollectListAggregator<IN_ID, OUT_ID>::ProcessSingleInternal(AggregateState *state, BaseVector *vector, const int32_t rowOffset, const int32_t rowCount, const std::shared_ptr<NullsHelper> nullMap) {
    if (rowCount == 0) {
        return;
    }
    bool isDictionary = vector->GetEncoding() == vec::OMNI_DICTIONARY;
    bool isConst = vector->GetEncoding() == vec::OMNI_ENCODING_CONST;
    using stateType = typename AggNativeAndVectorType<IN_ID>::type;
    // Caller (TypedAggregator::ProcessGroup) already passes state + aggStateOffset for this aggregator.
    ListState<stateType> *listState = ListState<stateType>::CastState(state);
    if (IsInputRaw()) {
        if (isConst) {
            auto *constVector = static_cast<vec::ConstVector<stateType> *>(vector);
            if (constVector->IsNull(0)) {
                return;
            }
            stateType value = constVector->GetConstValue();
            std::vector<stateType> *list = reinterpret_cast<std::vector<stateType> *>(listState->listAddr);
            for (int32_t i = 0; i < rowCount; i++) {
                if (nullMap == nullptr || !(*nullMap)[rowOffset + i]) {
                    list->push_back(value);
                }
            }
        } else {
            listState->UpdatePartialState(vector, rowCount, nullMap, isDictionary, rowOffset);
        }
    } else {
        auto arrayVector = static_cast<ArrayVector *>(vector);
        listState->UpdateFinalState(arrayVector, rowCount, nullMap, isDictionary, rowOffset);
    }
}

/**
 * Processes one row per group: for each group state, appends the input at the corresponding row to its list.
 * Partial: each row state gets the single value at rowOffset + rowIndex appended.
 * Final: each row state appends the array at rowOffset + rowIndex into its list.
 * @param rowStates One state per group (same order as rows).
 * @param vector Input vector (flat/dictionary or ArrayVector).
 * @param rowOffset Start row index. (Normally 0)
 * @param nullMap Optional null map.
 */
template <DataTypeId IN_ID, DataTypeId OUT_ID>
void CollectListAggregator<IN_ID, OUT_ID>::ProcessGroupInternal(std::vector<AggregateState *> &rowStates, BaseVector *vector, const int32_t rowOffset, const std::shared_ptr<NullsHelper> nullMap) {
    const size_t rowCount = rowStates.size();
    if (rowCount == 0) {
        return;
    }
    bool isDictionary = vector->GetEncoding() == vec::OMNI_DICTIONARY;

    // basically rowOffset is fix 0 now.
    int32_t rowIdx = rowOffset;
    for (size_t rowIndex = 0; rowIndex < rowCount; rowIndex++) {
        using stateType = typename AggNativeAndVectorType<IN_ID>::type;
        ListState<stateType> *listState = ListState<stateType>::CastState(rowStates[rowIndex] + aggStateOffset);
        if (IsInputRaw()) {
            BaseVector *sliced = vector->Slice(rowIdx, 1);
            auto rowNullMap = SliceNullMapForSingleRow(nullMap, rowIdx);
            listState->UpdatePartialState(sliced, 1, rowNullMap, isDictionary, 0);
            delete sliced;
        } else {
            auto arrayVector = reinterpret_cast<ArrayVector *>(vector);
            BaseVector *sliced = arrayVector->Slice(rowIdx, 1);
            auto rowNullMap = SliceNullMapForSingleRow(nullMap, rowIdx);
            listState->UpdateFinalState(static_cast<ArrayVector *>(sliced), 1, rowNullMap, isDictionary, 0);
            delete sliced;
        }
        ++rowIdx;
    }
}

/**
 * Merges spilled rows back into group states. For each unspill row, reads the spilled array from the batch
 * and appends each element into that group's list state (order preserved).
 * @param unspillRows Rows to merge (state, batch, rowIdx per row).
 * @param rowCount Number of rows in unspillRows.
 * @param vectorIndex Index of this aggregator's spill column in the batch; incremented by one on return.
 */
template <DataTypeId IN_ID, DataTypeId OUT_ID>
void CollectListAggregator<IN_ID, OUT_ID>::ProcessGroupUnspill(std::vector<UnspillRowInfo> &unspillRows, int32_t rowCount, int32_t &vectorIndex) {
    int32_t arrayVecIdx = vectorIndex++;
    using stateType = typename AggNativeAndVectorType<IN_ID>::type;
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
        ListState<stateType> *listState = ListState<stateType>::CastState(row.state + aggStateOffset);
        std::vector<stateType> *list = reinterpret_cast<std::vector<stateType> *>(listState->listAddr);
        auto *flatElementVec = static_cast<Vector<stateType> *>(elementVec);
        stateType *ptr = reinterpret_cast<stateType *>(unsafe::UnsafeVector::GetRawValues<stateType>(flatElementVec));
        for (int32_t j = 0, n = elementVec->GetSize(); j < n; j++) {
            if (!elementVec->IsNull(j)) {
                list->push_back(ptr[j]);
            }
        }
    }
}

/**
 * Aligns partial aggregate result schema (Skip Partial): converts input column into the intermediate
 * aggregation state format (array of values per row) and appends it to result.
 * - When input is raw (scalar column): each row's value becomes a single-element array [value].
 * - When input is partial (array column): each row is already partial state, so we pass through.
 */
template <DataTypeId IN_ID, DataTypeId OUT_ID>
void CollectListAggregator<IN_ID, OUT_ID>::ProcessAlignAggSchema(VectorBatch *vecBatch, BaseVector *originVector, const std::shared_ptr<NullsHelper> nullMap, const bool aggFilter) {
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
 * or null if nullMap[i] is set. This is the partial state format (one value per row).
 */
template <DataTypeId IN_ID, DataTypeId OUT_ID>
template <typename U>
void CollectListAggregator<IN_ID, OUT_ID>::ProcessAlignAggSchemaInteranal(VectorBatch *result, BaseVector *originVector, const std::shared_ptr<NullsHelper> nullMap) {
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
    auto *vector = reinterpret_cast<U *>(originVector);

    for (int32_t i = 0; i < rowCount; i++) {
        if ((nullMap != nullptr && (*nullMap)[i]) || originVector->IsNull(i)) {
            arrayVector->SetNull(i);
            arrayVector->SetSize(i, 0);
        } else {
            stateType val;
            if constexpr (std::is_same_v<U, Vector<stateType>>) {
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

template <DataTypeId IN_ID, DataTypeId OUT_ID>
CollectListAggregator<IN_ID, OUT_ID>::~CollectListAggregator() {
    using stateType = typename AggNativeAndVectorType<IN_ID>::type;
    for (int64_t addr : allocatedListAddrs_) {
        delete reinterpret_cast<std::vector<stateType> *>(addr);
    }
    allocatedListAddrs_.clear();
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
CollectListAggregator<IN_ID, OUT_ID>::CollectListAggregator(
    const DataTypes &inputTypes,
    const DataTypes &outputTypes,
    const std::vector<int32_t> &channels,
    const bool inputRaw,
    const bool outputPartial,
    const bool isOverflowAsNull)
    : TypedAggregator(OMNI_AGGREGATION_TYPE_COLLECT_LIST, inputTypes, outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull) {}

template class CollectListAggregator<OMNI_BOOLEAN, OMNI_BOOLEAN>;
template class CollectListAggregator<OMNI_BYTE, OMNI_BYTE>;
template class CollectListAggregator<OMNI_SHORT, OMNI_SHORT>;
template class CollectListAggregator<OMNI_INT, OMNI_INT>;
template class CollectListAggregator<OMNI_LONG, OMNI_LONG>;
template class CollectListAggregator<OMNI_DATE32, OMNI_DATE32>;
template class CollectListAggregator<OMNI_DATE64, OMNI_DATE64>;
template class CollectListAggregator<OMNI_TIMESTAMP, OMNI_TIMESTAMP>;
template class CollectListAggregator<OMNI_FLOAT, OMNI_FLOAT>;
template class CollectListAggregator<OMNI_DOUBLE, OMNI_DOUBLE>;
template class CollectListAggregator<OMNI_DECIMAL64, OMNI_DECIMAL64>;
template class CollectListAggregator<OMNI_DECIMAL128, OMNI_DECIMAL128>;
// CHAR/VARCHAR/VARBINARY/ARRAY: Factory throws for these. Only basic numeric types (incl. DECIMAL128) are instantiated here.

}  // omniruntime::op
