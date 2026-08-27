/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: For non-decimal type
 */
#ifndef OMNI_RUNTIME_TRY_SUM_FLAT_IM_AGGREGATOR_H
#define OMNI_RUNTIME_TRY_SUM_FLAT_IM_AGGREGATOR_H

#include <cstring>
#include "aggregator.h"
#include "sum_aggregator.h"
#include "simd/func/reduce.h"

namespace omniruntime {
namespace op {
template <DataTypeId IN_ID, DataTypeId OUT_ID> class TrySumFlatIMAggregator : public TypedAggregator {
    using InType = typename AggNativeAndVectorType<IN_ID>::type;
    using ResultType = typename AggNativeAndVectorType<OUT_ID>::type;
    static constexpr bool kMixedStateSerdeSupported =
        IsMixedSerdeArithmeticType<IN_ID>() && IsMixedSerdeArithmeticType<OUT_ID>();

    // inner class for aggregate state, the member depends on ResultType of Aggregator
#pragma pack(push, 1)
    struct TrySumFlatState : BaseCountState<ResultType> {
        static const TrySumFlatIMAggregator<IN_ID, OUT_ID>::TrySumFlatState *ConstCastState(const AggregateState *state)
        {
            return reinterpret_cast<const TrySumFlatIMAggregator<IN_ID, OUT_ID>::TrySumFlatState *>(state);
        }

        static TrySumFlatIMAggregator<IN_ID, OUT_ID>::TrySumFlatState *CastState(AggregateState *state)
        {
            return reinterpret_cast<TrySumFlatIMAggregator<IN_ID, OUT_ID>::TrySumFlatState *>(state);
        }

        template <typename TypeIn, typename TypeOut> static void UpdateState(AggregateState *state, const TypeIn &in)
        {
            auto *sumState = CastState(state);
            SumOp<TypeIn, TypeOut, int64_t, StateCountHandler, true>(&(sumState->value),
                sumState->count, in, 1ULL);
        }

        template <typename TypeIn, typename TypeOut, bool addIf>
        static void UpdateStateWithCondition(AggregateState *state, const TypeIn &in, const uint8_t &condition)
        {
            if (condition == addIf) {
                UpdateState<TypeIn, TypeOut>(state, in);
            }
        }
    };
#pragma pack(pop)

public:
    TrySumFlatIMAggregator(const DataTypes &inputTypes, const DataTypes &outputTypes, std::vector<int32_t> &channels,
        bool inputRaw, bool outputPartial, bool isOverflowAsNull)
        : TypedAggregator(OMNI_AGGREGATION_TYPE_TRY_SUM, inputTypes, outputTypes, channels, inputRaw, outputPartial,
        isOverflowAsNull)
    {}
    TrySumFlatIMAggregator(const FunctionType aggFunc, const DataTypes &inputTypes, const DataTypes &outputTypes,
        std::vector<int32_t> &channels, bool inputRaw, bool outputPartial, bool isOverflowAsNull)
        : TypedAggregator(aggFunc, inputTypes, outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull)
    {}
    ~TrySumFlatIMAggregator() override = default;

    void ProcessGroupInternalFinal(std::vector<AggregateState *> &rowStates, BaseVector *vector,
        const int32_t rowOffset, const std::shared_ptr<NullsHelper> nullMap)
    {
        // final stage : input vector will be Vector<ResultType>
        auto *sumPtr = reinterpret_cast<ResultType *>(GetValuesFromVector<OUT_ID>(vector));
        auto *emptyVector = curVectorBatch->Get(channels[1]);
        auto *emptyPtr = reinterpret_cast<bool *>(GetValuesFromVector<OMNI_BOOLEAN>(emptyVector));
        sumPtr += rowOffset;
        emptyPtr += rowOffset;

        for (size_t i = 0; i < rowStates.size(); ++i) {
            auto *state = TrySumFlatState::CastState(rowStates[i] + aggStateOffset);
            if (state->IsOverFlowed()) {
                continue;
            }
            if (nullMap != nullptr && (*nullMap)[i]) {
                if (!emptyPtr[i]) {
                    state->count = StateCountHandler::Overflowed();
                }
                continue;
            }
            if (!emptyPtr[i]) {
                SumOp<ResultType, ResultType, int64_t, StateCountHandler, true>(&state->value, state->count, sumPtr[i], 1ULL);
            }
        }
    }

    void ProcessGroupInternal(std::vector<AggregateState *> &rowStates, BaseVector *vector, const int32_t rowOffset,
        const std::shared_ptr<NullsHelper> nullMap) override
    {
        if (inputRaw) {
            if (vector->GetEncoding() == vec::OMNI_ENCODING_CONST) {
                if (nullMap == nullptr) {
                    auto constValue = static_cast<vec::ConstVector<InType> *>(vector)->GetConstValue();
                    for (size_t i = 0; i < rowStates.size(); ++i) {
                        TrySumFlatState::template UpdateState<InType, ResultType>(rowStates[i] + aggStateOffset, constValue);
                    }
                }
            } else if (vector->GetEncoding() != vec::OMNI_DICTIONARY) {
                auto *ptr = reinterpret_cast<InType *>(GetValuesFromVector<IN_ID>(vector));
                ptr += rowOffset;
                if (nullMap == nullptr) {
                    AddUseRowIndex<InType, TrySumFlatState::template UpdateState<InType, ResultType>>(rowStates,
                        aggStateOffset, ptr);
                } else {
                    AddConditionalUseRowIndex<InType,
                        TrySumFlatState::template UpdateStateWithCondition<InType, ResultType, false>>(rowStates,
                        aggStateOffset, ptr, *nullMap);
                }
            } else {
                auto *ptr = reinterpret_cast<InType *>(GetValuesFromDict<IN_ID>(vector));
                auto *indexMap = GetIdsFromDict<IN_ID>(vector) + rowOffset;
                if (nullMap == nullptr) {
                    AddDictUseRowIndex<InType, TrySumFlatState::template UpdateState<InType, ResultType>>(rowStates,
                        aggStateOffset, ptr, indexMap);
                } else {
                    AddDictConditionalUseRowIndex<InType, ResultType,
                        TrySumFlatState::template UpdateStateWithCondition<InType, ResultType, false>>(rowStates,
                        aggStateOffset, ptr, *nullMap, indexMap);
                }
            }
        } else {
            // no dictionary in input when stage is not partial
            ProcessGroupInternalFinal(rowStates, vector, rowOffset, nullMap);
        }
    }

    void ProcessGroupUnspill(std::vector<UnspillRowInfo> &unspillRows, int32_t rowCount, int32_t &vectorIndex) override
    {
        auto firstVecIdx = vectorIndex++;
        for (int32_t rowIdx = 0; rowIdx < rowCount; rowIdx++) {
            auto &row = unspillRows[rowIdx];
            auto index = row.rowIdx;
            auto sumVector = static_cast<Vector<ResultType> *>(row.batch->Get(firstVecIdx));
            TrySumFlatState *state = TrySumFlatState::CastState(row.state + aggStateOffset);
            auto value = sumVector->GetValue(index);
            if (!sumVector->IsNull(index)) {
                SumOp<ResultType, ResultType, int64_t, StateCountHandler, true>((ResultType *)(&state->value),
                    state->count, value, 1LL);
            } else if (value == SPILL_OVERFLOW_VALUE) {
                state->count=-1;
            }
        }
    }

    void ProcessSingleInternalFinal(AggregateState *state, BaseVector *vector, const int32_t rowOffset,
        const int32_t rowCount, const std::shared_ptr<NullsHelper> nullMap)
    {
        auto *trySumState = TrySumFlatState::CastState(state);
        auto *sumPtr = reinterpret_cast<ResultType *>(GetValuesFromVector<OUT_ID>(vector));
        auto *emptyVector = curVectorBatch->Get(channels[1]);
        auto *emptyPtr = reinterpret_cast<bool *>(GetValuesFromVector<OMNI_BOOLEAN>(emptyVector));
        sumPtr += rowOffset;
        emptyPtr += rowOffset;

        for (int32_t i = 0; i < rowCount; ++i) {
            if (trySumState->IsOverFlowed()) {
                break;
            }
            if (nullMap != nullptr && (*nullMap)[i]) {
                if (!emptyPtr[i]) {
                    trySumState->count = StateCountHandler::Overflowed();
                }
                continue;
            }
            if (!emptyPtr[i]) {
                SumOp<ResultType, ResultType, int64_t, StateCountHandler, true>(
                        &trySumState->value, trySumState->count, sumPtr[i], 1ULL);
            }
        }
    }

    void ProcessSingleInternal(AggregateState *state, BaseVector *vector, const int32_t rowOffset,
        const int32_t rowCount, const std::shared_ptr<NullsHelper> nullMap) override
    {
        TrySumFlatState *trySumFlatState = TrySumFlatState::CastState(state);
        if (inputRaw) {
            if (vector->GetEncoding() == vec::OMNI_ENCODING_CONST) {
                if (nullMap == nullptr) {
                    auto constValue = static_cast<vec::ConstVector<InType> *>(vector)->GetConstValue();
                    for (int32_t i = 0; i < rowCount; ++i) {
                        SumOp<InType, ResultType, int64_t, StateCountHandler, true>(
                            &trySumFlatState->value, trySumFlatState->count, constValue, 1ULL);
                    }
                }
            } else if (vector->GetEncoding() != vec::OMNI_DICTIONARY) {
                auto *ptr = reinterpret_cast<InType *>(GetValuesFromVector<IN_ID>(vector));
                ptr += rowOffset;
                if (nullMap == nullptr) {
                        Add<InType, ResultType, int64_t, SumOp<InType, ResultType, int64_t, StateCountHandler>>(
                            reinterpret_cast<ResultType *>(&trySumFlatState->value), trySumFlatState->count, ptr, rowCount);
                } else {
                        AddConditional<InType, ResultType, int64_t, SumConditionalOp<InType, ResultType, int64_t, StateCountHandler, false>>(
                            reinterpret_cast<ResultType *>(&trySumFlatState->value), trySumFlatState->count, ptr, rowCount, *nullMap);
                }
            } else {
                auto *ptr = reinterpret_cast<InType *>(GetValuesFromDict<IN_ID>(vector));
                auto *indexMap = GetIdsFromDict<IN_ID>(vector) + rowOffset;
                if (nullMap == nullptr) {
                        AddDict<InType, ResultType, int64_t, SumOp<InType, ResultType, int64_t, StateCountHandler>>(
                            reinterpret_cast<ResultType *>(&trySumFlatState->value), trySumFlatState->count, ptr, rowCount, indexMap);
                } else {
                        AddDictConditional<InType, ResultType, int64_t,
                            SumConditionalOp<InType, ResultType, int64_t, StateCountHandler, false>>(
                            &trySumFlatState->value, trySumFlatState->count, ptr, rowCount, *nullMap, indexMap);
                }
            }
        } else {
            ProcessSingleInternalFinal(state, vector, rowOffset, rowCount, nullMap);
        }
    }

    size_t GetStateSize() override
    {
        return sizeof(TrySumFlatState);
    }

    static void MergeMixedStateImpl(TrySumFlatIMAggregator<IN_ID, OUT_ID> *, TrySumFlatState *targetState,
        const TrySumFlatState *sourceState)
    {
        if constexpr (kMixedStateSerdeSupported) {
            if (sourceState->IsOverFlowed()) {
                targetState->SetOverFlow();
            } else if (!sourceState->IsEmpty()) {
                SumOp<ResultType, ResultType, int64_t, StateCountHandler, true>(&targetState->value, targetState->count,
                    sourceState->value, sourceState->count);
            }
        } else {
            (void)targetState;
            (void)sourceState;
        }
    }

    const MixedStateSerdeOps *GetMixedStateSerdeOps() const override
    {
        if constexpr (kMixedStateSerdeSupported) {
            return RawMixedStateSerde<TrySumFlatState, TrySumFlatIMAggregator<IN_ID, OUT_ID>,
                &TrySumFlatIMAggregator::MergeMixedStateImpl>::Ops();
        } else {
            return nullptr;
        }
    }

    void InitState(AggregateState *state) override
    {
        TrySumFlatState *trySumFlatState = TrySumFlatState::CastState(state + aggStateOffset);
        trySumFlatState->value = ResultType{};
        trySumFlatState->count=0;
    }

    void InitStates(std::vector<AggregateState *> &groupStates) override
    {
        for (auto groupState : groupStates) {
            InitState(groupState);
        }
    }

    std::vector<DataTypePtr> GetSpillType() override
    {
        std::vector<DataTypePtr> spillTypes;
        spillTypes.emplace_back(outputTypes.GetType(0));
        return spillTypes;
    }

    void ExtractValuesForSpill(std::vector<AggregateState *> &groupStates, std::vector<BaseVector *> &vectors) override
    {
        auto spillValueVec = static_cast<Vector<ResultType> *>(vectors[0]);
        auto rowCount = static_cast<int32_t>(groupStates.size());
        for (int32_t rowIndex = 0; rowIndex < rowCount; rowIndex++) {
            auto *state = TrySumFlatState::CastState(groupStates[rowIndex] + aggStateOffset);
            if (state->IsEmpty()) {
                spillValueVec->SetNull(rowIndex);
                spillValueVec->SetValue(rowIndex, SPILL_EMPTY_VALUE);
            } else if (state->IsOverFlowed()) {
                spillValueVec->SetNull(rowIndex);
                spillValueVec->SetValue(rowIndex, SPILL_OVERFLOW_VALUE);
            } else {
                spillValueVec->SetValue(rowIndex, state->value);
            }
        }
    }

    void ExtractValues(const AggregateState *state, std::vector<BaseVector *> &vectors, int32_t rowIndex) override
    {
        const auto *trySumState = TrySumFlatState::ConstCastState(state + aggStateOffset);
        if (outputPartial) {
            auto sumVector = static_cast<Vector<ResultType> *>(vectors[0]);
            auto emptyVector = static_cast<Vector<bool> *>(vectors[1]);
            if (trySumState->IsOverFlowed()) {
                sumVector->SetNull(rowIndex);
                emptyVector->SetValue(rowIndex, false);
            } else if (trySumState->IsEmpty()) {
                sumVector->SetValue(rowIndex, ResultType {});
                emptyVector->SetValue(rowIndex, true);
            } else {
                sumVector->SetValue(rowIndex, trySumState->value);
                emptyVector->SetValue(rowIndex, false);
            }
            return;

        }

        auto sumVector = static_cast<Vector<ResultType> *>(vectors[0]);
        if (trySumState->IsOverFlowed() || trySumState->IsEmpty()) {
            sumVector->SetNull(rowIndex);
        } else {
            sumVector->SetValue(rowIndex, trySumState->value);
        }
    }

    void ExtractValuesBatch(std::vector<AggregateState *> &groupStates, std::vector<BaseVector *> &vectors,
        int32_t rowOffset, int32_t rowCount) override
    {
        if (outputPartial) {
            auto sumVector = static_cast<Vector<ResultType> *>(vectors[0]);
            auto emptyVector = static_cast<Vector<bool> *>(vectors[1]);
            for (int32_t rowIndex = 0; rowIndex < rowCount; rowIndex++) {
                const auto *state = TrySumFlatState::ConstCastState(groupStates[rowIndex] + aggStateOffset);
                if (state->IsOverFlowed()) {
                    sumVector->SetNull(rowIndex);
                    emptyVector->SetValue(rowIndex, false);
                } else if (state->IsEmpty()) {
                    sumVector->SetValue(rowIndex, ResultType {});
                    emptyVector->SetValue(rowIndex, true);
                } else {
                    sumVector->SetValue(rowIndex, state->value);
                    emptyVector->SetValue(rowIndex, false);
                }
            }
            return;
        }

        auto sumVector = static_cast<Vector<ResultType> *>(vectors[0]);
        for (int32_t rowIndex = 0; rowIndex < rowCount; rowIndex++) {
            const auto *state = TrySumFlatState::ConstCastState(groupStates[rowIndex] + aggStateOffset);
            if (state->IsOverFlowed() || state->IsEmpty()) {
                sumVector->SetNull(rowIndex);
            } else {
                sumVector->SetValue(rowIndex, state->value);
            }
        }
    }

    void ProcessAlignAggSchema(VectorBatch *result, BaseVector *originVector,
        const std::shared_ptr<NullsHelper> nullMap, const bool aggFilter) override
    {
        int rowCount = originVector == nullptr ? 0 : originVector->GetSize();
        auto emptyVector = reinterpret_cast<Vector<bool> *>(VectorHelper::CreateFlatVector(OMNI_BOOLEAN, rowCount));

        if (rowCount == 0) {
            auto sumVector = reinterpret_cast<Vector<ResultType> *>(VectorHelper::CreateFlatVector(OUT_ID, rowCount));
            result->Append(sumVector);
            result->Append(emptyVector);
            return;
        }

        if constexpr (std::is_same_v<InType, ResultType>) {
            if (nullMap == nullptr && !aggFilter) {
                auto sumVector = VectorHelper::SliceVector(originVector, 0, rowCount);
                bool *emptyPtr = reinterpret_cast<bool *>(GetValuesFromVector<OMNI_BOOLEAN>(emptyVector));
                std::fill_n(emptyPtr, rowCount, false);
                result->Append(sumVector);
                result->Append(emptyVector);
                return;
            }
        }

        if (originVector->GetEncoding() == OMNI_DICTIONARY) {
            ProcessAlignAggSchemaInternal<Vector<DictionaryContainer<InType>>>(result, originVector, nullMap, emptyVector);
        } else {
            ProcessAlignAggSchemaInternal<Vector<InType>>(result, originVector, nullMap, emptyVector);
        }
    }

    template<typename T>
    void ProcessAlignAggSchemaInternal(VectorBatch *result, BaseVector *originVector,
        const std::shared_ptr<NullsHelper> nullMap, Vector<bool> *emptyVector)
    {
        int rowCount = originVector->GetSize();
        auto sumVector = reinterpret_cast<Vector<ResultType> *>(VectorHelper::CreateFlatVector(OUT_ID, rowCount));
        // The varchar type is converted to the double type in advance, so InType can't be the varchar type.
        auto vector = reinterpret_cast<T *>(originVector);
        if (nullMap != nullptr) {
            for (int index = 0; index < rowCount; ++index) {
                if ((*nullMap)[index]) {
                    sumVector->SetValue(index, ResultType {});
                    emptyVector->SetValue(index, true);
                } else {
                    sumVector->SetValue(index, static_cast<ResultType>(vector->GetValue(index)));
                    emptyVector->SetValue(index, false);
                }
            }
        } else {
            for (int index = 0; index < rowCount; ++index) {
                sumVector->SetValue(index, static_cast<ResultType>(vector->GetValue(index)));
                emptyVector->SetValue(index, false);
            }
        }
        result->Append(sumVector);
        result->Append(emptyVector);
    }

private:
    static constexpr ResultType SPILL_EMPTY_VALUE{0};
    static constexpr ResultType SPILL_OVERFLOW_VALUE{-1};
};
}
}
#endif // OMNI_RUNTIME_TRY_SUM_FLAT_IM_AGGREGATOR_H
