/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 * Description: CollectList aggregation (collect_list_aggregator header).
 */

#ifndef OMNI_RUNTIME_COLLECT_LIST_AGGREGATOR_H
#define OMNI_RUNTIME_COLLECT_LIST_AGGREGATOR_H

#include <fmt/format.h>
#include <unordered_set>
#include <vector>
#include "typed_aggregator.h"

namespace omniruntime::op {
#pragma pack(push, 1)
template <typename T>
struct ListState {
    // store address for keep the state length to be fixed.
    int64_t listAddr;

    static const ListState<T> *ConstCastState(const AggregateState *state)
    {
        return reinterpret_cast<const ListState<T> *>(state);
    }

    static ListState<T> *CastState(AggregateState *state)
    {
        return reinterpret_cast<ListState<T> *>(state);
    }

    // baseRowIndex is the row index of the base vector, it is normally 0.
    void UpdatePartialState(BaseVector *vector, const int32_t rowCount, const std::shared_ptr<NullsHelper> nullMap, const bool isDictionary, int32_t baseRowIndex = 0)
    {
        std::vector<T> *list = reinterpret_cast<std::vector<T> *>(listAddr);
        if (isDictionary) {
            auto *dictVector = reinterpret_cast<Vector<DictionaryContainer<T>> *>(vector);
            T *valuePtr = unsafe::UnsafeDictionaryVector::GetDictionary(dictVector);
            const int32_t *ids = unsafe::UnsafeDictionaryVector::GetIds(dictVector);
            for (uint32_t i = 0; i < static_cast<uint32_t>(rowCount); i++) {
                if ((nullMap == nullptr || !(*nullMap)[baseRowIndex + i]) &&
                    !vector->IsNull(static_cast<int32_t>(baseRowIndex + i))) {
                    list->push_back(valuePtr[ids[i]]);
                }
            }
        } else {
            auto flatVector = reinterpret_cast<Vector<T> *>(vector);
            T *valuePtr = unsafe::UnsafeVector::GetRawValues<T>(flatVector);
            for (uint32_t i = 0; i < static_cast<uint32_t>(rowCount); i++) {
                if ((nullMap == nullptr || !(*nullMap)[baseRowIndex + i]) &&
                    !vector->IsNull(static_cast<int32_t>(baseRowIndex + i))) {
                    list->push_back(valuePtr[i]);
                }
            }
        }
    }

    void UpdateFinalState(ArrayVector *arrayVector, const int32_t rowCount, const std::shared_ptr<NullsHelper> nullMap, const bool isDictionary, int32_t baseRowIndex = 0)
    {
        std::vector<T> *list = reinterpret_cast<std::vector<T> *>(listAddr);
        for (uint32_t i = 0; i < static_cast<uint32_t>(rowCount); i++) {
            if ((nullMap == nullptr || !(*nullMap)[baseRowIndex + i]) &&
                !arrayVector->IsNull(static_cast<int32_t>(baseRowIndex + i))) {
                auto elementVector = arrayVector->GetArrayAt(i, false);
                auto elementSize = elementVector->GetSize();
                if (isDictionary) {
                    auto *dictVector = reinterpret_cast<Vector<DictionaryContainer<T>> *>(elementVector.get());
                    T *valuePtr = unsafe::UnsafeDictionaryVector::GetDictionary(dictVector);
                    const int32_t *ids = unsafe::UnsafeDictionaryVector::GetIds(dictVector);
                    for (uint32_t j = 0; j < elementSize; j++) {
                        list->push_back(valuePtr[ids[j]]);
                    }
                } else {
                    auto flatVector = reinterpret_cast<Vector<T> *>(elementVector.get());
                    T *valuePtr = unsafe::UnsafeVector::GetRawValues<T>(flatVector);
                    for (uint32_t j = 0; j < elementSize; j++) {
                        list->push_back(valuePtr[j]);
                    }
                }
            }
        }
    }

    /**
     * Updates the list state with a single value when the filter condition is satisfied.
     * Used by the aggregation framework when an aggregate filter is present (e.g. FILTER (WHERE cond)).
     * @param state Aggregator state (caller passes state + aggStateOffset).
     * @param value Input value to append to the list.
     * @param condition Filter flag: only when condition == addIf is the value added.
     */
    template <bool addIf>
    static void UpdateStateWithCondition(AggregateState *state, const T &value, const uint8_t &condition)
    {
        if (condition == addIf) {
            ListState<T> *listState = ListState<T>::CastState(state);
            std::vector<T> *list = reinterpret_cast<std::vector<T> *>(listState->listAddr);
            list->push_back(value);
        }
    }
};
#pragma pack(pop)

template <DataTypeId IN_ID, DataTypeId OUT_ID>
class CollectListAggregator : public TypedAggregator {
    using InVector = typename AggNativeAndVectorType<IN_ID>::vector;
    using OutVector = typename AggNativeAndVectorType<OUT_ID>::vector;
    using InType = typename AggNativeAndVectorType<IN_ID>::type;
    using OutType = typename AggNativeAndVectorType<OUT_ID>::type;

public:
    CollectListAggregator(const DataTypes &inputTypes, const DataTypes &outputTypes, const std::vector<int32_t> &channels, const bool inputRaw, const bool outputPartial, const bool isOverflowAsNull);
    ~CollectListAggregator() override;
    void ExtractValues(const AggregateState *state, std::vector<BaseVector *> &vectors, int32_t rowIndex) override;
    void ExtractValuesBatch(std::vector<AggregateState *> &groupStates, std::vector<BaseVector *> &vectors, int32_t rowOffset, int32_t rowCount) override;
    void ExtractValuesForSpill(std::vector<AggregateState *> &groupStates, std::vector<BaseVector *> &vectors) override;
    void InitState(AggregateState *state) override;
    void InitStates(std::vector<AggregateState *> &groupStates) override;
    // Frees the std::vector<T> in state. Call before freeing state buffer to avoid leak.
    void DestroyState(AggregateState *state);
    // Frees every std::vector<T> ever allocated by this aggregator via its own
    // address book; used by the aggregation operator after a successful spill.
    void DestroyAllStates() override;
    std::vector<DataTypePtr> GetSpillType() override;

    // Fixed 8 bytes so that collect_list(col_a) and collect_list(col_b) with different element types
    // share the same state layout (one int64_t address per slot); avoids layout mismatch when
    // mixing e.g. collect_list(int_val) and collect_list(boolean_val).
    size_t GetStateSize() override
    {
        return 8u;
    }

    // Factory always creates CollectListAggregator<T, T>: partial input T -> T, final input Array<T> -> element T.
    // CollectList preserves insertion order and allows duplicates (unlike CollectSet which uses a set).
    // Supports: basic types (BOOLEAN, SHORT, INT, LONG, FLOAT, DOUBLE, DECIMAL64, DECIMAL128), string/binary (CHAR, VARCHAR, VARBINARY), nested array (OMNI_ARRAY).
    // Factory: supports basic types and string/binary/array. Same pattern as CollectSetAggregator::Create.
    static std::unique_ptr<Aggregator> Create(const DataTypes &inputTypes, const DataTypes &outputTypes,
        std::vector<int32_t> &channels, bool rawIn, bool partialOut, bool isOverflowAsNull)
    {
        if constexpr (IN_ID != OUT_ID) {
            LogError("CollectListAggregator expects IN_ID == OUT_ID, got IN_ID=%s OUT_ID=%s",
                TypeUtil::TypeToStringLog(IN_ID).c_str(), TypeUtil::TypeToStringLog(OUT_ID).c_str());
            return nullptr;
        }
        if constexpr (IN_ID == OMNI_BOOLEAN || IN_ID == OMNI_BYTE || IN_ID == OMNI_SHORT || IN_ID == OMNI_INT || IN_ID == OMNI_LONG ||
            IN_ID == OMNI_DATE32 || IN_ID == OMNI_DATE64 || IN_ID == OMNI_TIMESTAMP ||
            IN_ID == OMNI_FLOAT || IN_ID == OMNI_DOUBLE || IN_ID == OMNI_DECIMAL64 || IN_ID == OMNI_DECIMAL128) {
            return std::make_unique<CollectListAggregator<IN_ID, OUT_ID>>(inputTypes, outputTypes, channels, rawIn, partialOut, isOverflowAsNull);
        }
        LogError("CollectListAggregator::Create: Unsupported input type %s", TypeUtil::TypeToStringLog(IN_ID).c_str());
        return nullptr;
    }

    void ProcessGroupUnspill(std::vector<UnspillRowInfo> &unspillRows, int32_t rowCount, int32_t &vectorIndex) override;
    void ProcessAlignAggSchema(VectorBatch *result, BaseVector *originVector, const std::shared_ptr<NullsHelper> nullMap, const bool aggFilter) override;

protected:
    /** Addresses of std::vector<T> allocated in InitState; freed in DestroyState,
     * DestroyAllStates or in destructor. Hash-set for O(1) insert/erase. */
    std::unordered_set<int64_t> allocatedListAddrs_;

    void ProcessSingleInternal(AggregateState *state, BaseVector *vector, const int32_t rowOffset, const int32_t rowCount, const std::shared_ptr<NullsHelper> nullMap) override;
    void ProcessGroupInternal(std::vector<AggregateState *> &rowStates, BaseVector *vector, const int32_t rowOffset, const std::shared_ptr<NullsHelper> nullMap) override;

    template <typename U>
    void ProcessAlignAggSchemaInteranal(VectorBatch *result, BaseVector *originVector, const std::shared_ptr<NullsHelper> nullMap);
};

}  // omniruntime::op

#endif  // OMNI_RUNTIME_COLLECT_LIST_AGGREGATOR_H
