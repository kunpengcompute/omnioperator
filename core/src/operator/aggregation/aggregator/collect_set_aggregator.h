/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 * Description: CollectSet aggregation (collect_set_aggregator header).
 */

#ifndef OMNI_RUNTIME_COLLECT_SET_AGGREGATOR_H
#define OMNI_RUNTIME_COLLECT_SET_AGGREGATOR_H

#include <fmt/format.h>
#include <unordered_set>
#include <vector>
#include "typed_aggregator.h"
#include "operator/hashmap/base_hash_map.h"

namespace omniruntime::op {
#pragma pack(push, 1)
    template <typename T>
    struct SetState {
        // store address for keep the state length to be fixed.
        int64_t uniqueValuesAddr;

        static const SetState<T> *ConstCastState(const AggregateState *state)
        {
            return reinterpret_cast<const SetState<T> *>(state);
        }

        static SetState<T> *CastState(AggregateState *state)
        {
            return reinterpret_cast<SetState<T> *>(state);
        }

        // baseRowIndex is the row index of the base vector, it is normally 0.
        void UpdatePartialState(BaseVector *vector, const int32_t rowCount, const std::shared_ptr<NullsHelper> nullMap, const bool isDictionary, int32_t baseRowIndex = 0)
        {
            DefaultHashMap<T, int8_t>* uniqueValues = reinterpret_cast<DefaultHashMap<T, int8_t> *>(uniqueValuesAddr);
            if (isDictionary) {
                auto *dictVector = reinterpret_cast<Vector<DictionaryContainer<T>> *>(vector);
                T *valuePtr = unsafe::UnsafeDictionaryVector::GetDictionary(dictVector);
                const int32_t *ids = unsafe::UnsafeDictionaryVector::GetIds(dictVector);
                for (uint32_t i = 0; i < static_cast<uint32_t>(rowCount); i++) {
                    // Value reads must be offset by baseRowIndex to match the null
                    // checks; before this fix the caller had to Slice each row
                    // into a one-row vector (a heap allocation per row).
                    if ((nullMap == nullptr || !(*nullMap)[baseRowIndex + i]) &&
                        !vector->IsNull(static_cast<int32_t>(baseRowIndex + i))) {
                        uniqueValues->Emplace(valuePtr[ids[baseRowIndex + i]]);
                    }
                }
            } else {
                auto flatVector = reinterpret_cast<Vector<T> *>(vector);
                T *valuePtr = unsafe::UnsafeVector::GetRawValues<T>(flatVector);
                for (uint32_t i = 0; i < static_cast<uint32_t>(rowCount); i++) {
                    if ((nullMap == nullptr || !(*nullMap)[baseRowIndex + i]) &&
                        !vector->IsNull(static_cast<int32_t>(baseRowIndex + i))) {
                        uniqueValues->Emplace(valuePtr[baseRowIndex + i]);
                    }
                }
            }
        }

        void UpdateFinalState(ArrayVector *arrayVector, const int32_t rowCount, const std::shared_ptr<NullsHelper> nullMap, const bool isDictionary, int32_t baseRowIndex = 0)
        {
            DefaultHashMap<T, int8_t>* uniqueValues = reinterpret_cast<DefaultHashMap<T, int8_t> *>(uniqueValuesAddr);
            for (uint32_t i = 0; i < static_cast<uint32_t>(rowCount); i++) {
                if ((nullMap == nullptr || !(*nullMap)[baseRowIndex + i]) &&
                    !arrayVector->IsNull(static_cast<int32_t>(baseRowIndex + i))) {
                    // Index the base vector, not the loop counter: null checks
                    // above use baseRowIndex + i, so the array read must too,
                    // otherwise rows [baseRowIndex, baseRowIndex+rowCount) read
                    // the wrong array slot.
                    auto elementVector = arrayVector->GetArrayAt(baseRowIndex + i, false);
                    auto elementSize = elementVector->GetSize();
                    if (isDictionary) {
                        auto *dictVector = reinterpret_cast<Vector<DictionaryContainer<T>> *>(elementVector.get());
                        T *valuePtr = unsafe::UnsafeDictionaryVector::GetDictionary(dictVector);
                        const int32_t *ids = unsafe::UnsafeDictionaryVector::GetIds(dictVector);
                        for (uint32_t j = 0; j < elementSize; j++) {
                            uniqueValues->Emplace(valuePtr[ids[j]]);
                        }
                    } else {
                        auto flatVector = reinterpret_cast<Vector<T> *>(elementVector.get());
                        T *valuePtr = unsafe::UnsafeVector::GetRawValues<T>(flatVector);
                        for (uint32_t j = 0; j < elementSize; j++) {
                            uniqueValues->Emplace(valuePtr[j]);
                        }
                    }
                }
            }
        }

        /**
         * Updates the set state with a single value when the filter condition is satisfied.
         * Used by the aggregation framework when an aggregate filter is present (e.g. FILTER (WHERE cond)).
         * @param state Aggregator state (caller passes state + aggStateOffset).
         * @param value Input value to add to the set.
         * @param condition Filter flag: only when condition == addIf is the value added.
         */
        template <bool addIf>
        static void UpdateStateWithCondition(AggregateState *state, const T &value, const uint8_t &condition)
        {
            if (condition == addIf) {
                SetState<T> *setState = SetState<T>::CastState(state);
                DefaultHashMap<T, int8_t> *uniqueValues =
                    reinterpret_cast<DefaultHashMap<T, int8_t> *>(setState->uniqueValuesAddr);
                uniqueValues->Emplace(value);
            }
        }
    };
#pragma pack(pop)

template<DataTypeId IN_ID, DataTypeId OUT_ID>
class CollectSetAggregator : public TypedAggregator {
    using InVector = typename AggNativeAndVectorType<IN_ID>::vector;
    using OutVector = typename AggNativeAndVectorType<OUT_ID>::vector;
    using InType = typename AggNativeAndVectorType<IN_ID>::type;
    using OutType = typename AggNativeAndVectorType<OUT_ID>::type;

public:
    CollectSetAggregator(const DataTypes &inputTypes, const DataTypes &outputTypes, const std::vector<int32_t> &channels, const bool inputRaw, const bool outputPartial, const bool isOverflowAsNull);
    // Extended constructor for derived aggregators (e.g. CountDistinctAggregator) that reuse the
    // set-based state machine but report a different FunctionType.
    CollectSetAggregator(const FunctionType aggregateType, const DataTypes &inputTypes, const DataTypes &outputTypes, const std::vector<int32_t> &channels, const bool inputRaw, const bool outputPartial, const bool isOverflowAsNull);
    ~CollectSetAggregator() override;
    void ExtractValues(const AggregateState *state, std::vector<BaseVector *> &vectors, int32_t rowIndex) override;
    void ExtractValuesBatch(std::vector<AggregateState *> &groupStates, std::vector<BaseVector *> &vectors, int32_t rowOffset, int32_t rowCount) override;
    void ExtractValuesForSpill(std::vector<AggregateState *> &groupStates, std::vector<BaseVector *> &vectors) override;
    void InitState(AggregateState *state) override;
    void InitStates(std::vector<AggregateState *> &groupStates) override;
    // Frees the DefaultHashMap in state. Call before freeing state buffer to avoid leak.
    void DestroyState(AggregateState *state);
    // Frees every DefaultHashMap ever allocated by this aggregator via its own
    // address book; used by the aggregation operator after a successful spill.
    // O(1) average per state (hash-set based), suitable for tens of millions of groups.
    void DestroyAllStates() override;
    std::vector<DataTypePtr> GetSpillType() override;

    // Fixed 8 bytes so that collect_set(col_a) and collect_set(col_b) with different element types
    // share the same state layout (one int64_t address per slot); avoids layout mismatch when
    // mixing e.g. collect_set(int_val) and collect_set(boolean_val).
    size_t GetStateSize() override
    {
        return 8u;
    }

    // Factory always creates CollectSetAggregator<T, T>: partial input T -> T, final input Array<T> -> element T.
    // Supports: basic types (BOOLEAN, SHORT, INT, LONG, FLOAT, DOUBLE, DECIMAL64, DECIMAL128), string/binary (CHAR, VARCHAR, VARBINARY), nested array (OMNI_ARRAY).
    static std::unique_ptr<Aggregator> Create(const DataTypes &inputTypes, const DataTypes &outputTypes,
        std::vector<int32_t> &channels, bool rawIn, bool partialOut, bool isOverflowAsNull)
    {
        if constexpr (IN_ID != OUT_ID) {
            LogError("CollectSetAggregator expects IN_ID == OUT_ID (element type T), got IN_ID=%s OUT_ID=%s",
                TypeUtil::TypeToStringLog(IN_ID).c_str(), TypeUtil::TypeToStringLog(OUT_ID).c_str());
            return nullptr;
        }
        // Basic types (including DATE32/DATE64/TIMESTAMP and DECIMAL128: GroupbyHashCalculator in group_hasher.h)
        if constexpr (IN_ID == OMNI_BOOLEAN || IN_ID == OMNI_BYTE || IN_ID == OMNI_SHORT || IN_ID == OMNI_INT || IN_ID == OMNI_LONG ||
            IN_ID == OMNI_DATE32 || IN_ID == OMNI_DATE64 || IN_ID == OMNI_TIMESTAMP ||
            IN_ID == OMNI_FLOAT || IN_ID == OMNI_DOUBLE || IN_ID == OMNI_DECIMAL64 || IN_ID == OMNI_DECIMAL128) {
            return std::make_unique<CollectSetAggregator<IN_ID, OUT_ID>>(inputTypes, outputTypes, channels, rawIn, partialOut, isOverflowAsNull);
        }
        LogError("CollectSetAggregator::Create: Unsupported input type %s", TypeUtil::TypeToStringLog(IN_ID).c_str());
        return nullptr;
    }

    void ProcessGroupUnspill(std::vector<UnspillRowInfo> &unspillRows, int32_t rowCount, int32_t &vectorIndex) override;

    void ProcessAlignAggSchema(VectorBatch *result, BaseVector *originVector, const std::shared_ptr<NullsHelper> nullMap, const bool aggFilter) override;

protected:
    /** Addresses of DefaultHashMap allocated in InitState; freed in DestroyState,
     * DestroyAllStates or in destructor. Hash-set: O(1) insert/erase keeps state
     * destruction linear even with tens of millions of groups. */
    std::unordered_set<int64_t> allocatedUniqueValuesAddrs_;

    void ProcessSingleInternal(AggregateState *state, BaseVector *vector, const int32_t rowOffset, const int32_t rowCount, const std::shared_ptr<NullsHelper> nullMap) override;

    void ProcessGroupInternal(std::vector<AggregateState *> &rowStates, BaseVector *vector, const int32_t rowOffset, const std::shared_ptr<NullsHelper> nullMap) override;

    template<typename T>
    void ProcessAlignAggSchemaInteranal(VectorBatch *result, BaseVector *originVector, const std::shared_ptr<NullsHelper> nullMap);
};

}// omniruntime::op

#endif //OMNI_RUNTIME_COLLECT_SET_AGGREGATOR_H

