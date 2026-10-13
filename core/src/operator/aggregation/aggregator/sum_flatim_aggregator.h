/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2024. All rights reserved.
 * Description: For non-decimal type
 */
#ifndef OMNI_RUNTIME_SUM_FLAT_IM_AGGREGATOR_H
#define OMNI_RUNTIME_SUM_FLAT_IM_AGGREGATOR_H

#include "aggregator.h"
#include "simd/func/reduce.h"

namespace omniruntime {
namespace op {
template <DataTypeId IN_ID, DataTypeId OUT_ID> class SumFlatIMAggregator : public TypedAggregator {
    using InType = typename AggNativeAndVectorType<IN_ID>::type;
    using ResultType = typename AggNativeAndVectorType<OUT_ID>::type;

    // inner class for aggregate state, the member depends on ResultType of Aggregator
#pragma pack(push, 1)
    struct SumFlatState : BaseState<ResultType> {
        static const SumFlatIMAggregator<IN_ID, OUT_ID>::SumFlatState *ConstCastState(const AggregateState *state)
        {
            return reinterpret_cast<const SumFlatIMAggregator<IN_ID, OUT_ID>::SumFlatState *>(state);
        }

        static SumFlatIMAggregator<IN_ID, OUT_ID>::SumFlatState *CastState(AggregateState *state)
        {
            return reinterpret_cast<SumFlatIMAggregator<IN_ID, OUT_ID>::SumFlatState *>(state);
        }

        template <typename TypeIn, typename TypeOut> static void UpdateState(AggregateState *state, const TypeIn &in)
        {
            auto *maxState = CastState(state);
            SumOp<TypeIn, TypeOut, AggValueState, StateValueHandler, false>(&(maxState->value),
                                                                            maxState->valueState, in, 1ULL);
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
    SumFlatIMAggregator(const DataTypes &inputTypes, const DataTypes &outputTypes, std::vector<int32_t> &channels,
        bool inputRaw, bool outputPartial, bool isOverflowAsNull)
        : TypedAggregator(OMNI_AGGREGATION_TYPE_SUM, inputTypes, outputTypes, channels, inputRaw, outputPartial,
        isOverflowAsNull)
    {}
    SumFlatIMAggregator(const FunctionType aggFunc, const DataTypes &inputTypes, const DataTypes &outputTypes,
        std::vector<int32_t> &channels, bool inputRaw, bool outputPartial, bool isOverflowAsNull)
        : TypedAggregator(aggFunc, inputTypes, outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull)
    {}
    ~SumFlatIMAggregator() override = default;

    void ProcessGroupInternalFinal(std::vector<AggregateState *> &rowStates, BaseVector *vector,
        const int32_t rowOffset, const std::shared_ptr<NullsHelper> nullMap)
    {
        // final stage : input vector will be Vector<ResultType>
        auto *ptr = reinterpret_cast<ResultType *>(GetValuesFromVector<OUT_ID>(vector));
        ptr += rowOffset;
        if (nullMap == nullptr) {
            AddUseRowIndex<ResultType, SumFlatState::template UpdateState<ResultType, ResultType>>(rowStates,
                aggStateOffset, ptr);
        } else {
            AddConditionalUseRowIndex<ResultType,
                SumFlatState::template UpdateStateWithCondition<ResultType, ResultType, false>>(rowStates,
                aggStateOffset, ptr, *nullMap);
        }
    }

    void ProcessGroupInternal(std::vector<AggregateState *> &rowStates, BaseVector *vector, const int32_t rowOffset,
        const std::shared_ptr<NullsHelper> nullMap)
    {
        if (inputRaw) {
            if (vector->GetEncoding() == vec::OMNI_ENCODING_CONST) {
                if (nullMap == nullptr) {
                    auto constValue = static_cast<vec::ConstVector<InType> *>(vector)->GetConstValue();
                    for (size_t i = 0; i < rowStates.size(); ++i) {
                        SumFlatState::template UpdateState<InType, ResultType>(rowStates[i] + aggStateOffset, constValue);
                    }
                }
            } else if (vector->GetEncoding() != vec::OMNI_DICTIONARY) {
                auto *ptr = reinterpret_cast<InType *>(GetValuesFromVector<IN_ID>(vector));
                ptr += rowOffset;
                if (nullMap == nullptr) {
                    AddUseRowIndex<InType, SumFlatState::template UpdateState<InType, ResultType>>(rowStates,
                        aggStateOffset, ptr);
                } else {
                    AddConditionalUseRowIndex<InType,
                        SumFlatState::template UpdateStateWithCondition<InType, ResultType, false>>(rowStates,
                        aggStateOffset, ptr, *nullMap);
                }
            } else {
                auto *ptr = reinterpret_cast<InType *>(GetValuesFromDict<IN_ID>(vector));
                auto *indexMap = GetIdsFromDict<IN_ID>(vector) + rowOffset;
                if (nullMap == nullptr) {
                    AddDictUseRowIndex<InType, SumFlatState::template UpdateState<InType, ResultType>>(rowStates,
                        aggStateOffset, ptr, indexMap);
                } else {
                    AddDictConditionalUseRowIndex<InType, ResultType,
                        SumFlatState::template UpdateStateWithCondition<InType, ResultType, false>>(rowStates,
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
            if (!sumVector->IsNull(index)) {
                SumFlatState *state = SumFlatState::CastState(row.state + aggStateOffset);
                auto value = sumVector->GetValue(index);
                SumOp<ResultType, ResultType, AggValueState, StateValueHandler, false>((ResultType *)(&state->value),
                    state->valueState, value, 1LL);
            }
        }
    }

    void ProcessSingleInternalFinal(AggregateState *state, BaseVector *vector, const int32_t rowOffset,
        const int32_t rowCount, const std::shared_ptr<NullsHelper> nullMap)
    {
        SumFlatState *sumFlatState = SumFlatState::CastState(state);
        auto *ptr = reinterpret_cast<ResultType *>(GetValuesFromVector<OUT_ID>(vector));
        ptr += rowOffset;
        if (nullMap == nullptr) {
            if constexpr (std::is_floating_point_v<ResultType>) {
                simd::ReduceExternal<ResultType, ResultType, AggValueState, StateValueHandler,
                    simd::ReduceFunc::Sum>(&sumFlatState->value, sumFlatState->valueState, ptr, rowCount);
            } else {
                simd::ReduceExternal<ResultType, ResultType, AggValueState, StateValueHandler,
                    simd::ReduceFunc::Sum>(&sumFlatState->value, sumFlatState->valueState, ptr, rowCount);
            }
        } else {
            auto conditionArray = nullMap->convertToArray(rowCount);
            if constexpr (std::is_floating_point_v<ResultType>) {
                simd::ReduceWithNullsExternal<ResultType, ResultType, AggValueState, StateValueHandler,
                    simd::ReduceFunc::Sum>(&sumFlatState->value, sumFlatState->valueState, ptr, rowCount,
                    conditionArray.data());
            } else {
                simd::ReduceWithNullsExternal<ResultType, ResultType, AggValueState, StateValueHandler,
                    simd::ReduceFunc::Sum>(&sumFlatState->value, sumFlatState->valueState, ptr, rowCount,
                    conditionArray.data());
            }
        }
    }

    void ProcessSingleInternal(AggregateState *state, BaseVector *vector, const int32_t rowOffset,
        const int32_t rowCount, const std::shared_ptr<NullsHelper> nullMap)
    {
        SumFlatState *sumFlatState = SumFlatState::CastState(state);
        if (inputRaw) {
            if (vector->GetEncoding() == vec::OMNI_ENCODING_CONST) {
                if (nullMap == nullptr) {
                    auto constValue = static_cast<vec::ConstVector<InType> *>(vector)->GetConstValue();
                    for (int32_t i = 0; i < rowCount; ++i) {
                        SumOp<InType, ResultType, AggValueState, StateValueHandler, false>(
                            &sumFlatState->value, sumFlatState->valueState, constValue, 1ULL);
                    }
                }
            } else if (vector->GetEncoding() != vec::OMNI_DICTIONARY) {
                auto *ptr = reinterpret_cast<InType *>(GetValuesFromVector<IN_ID>(vector));
                ptr += rowOffset;
                if (nullMap == nullptr) {
                    simd::ReduceExternal<InType, ResultType, AggValueState, StateValueHandler, simd::ReduceFunc::Sum>(
                        &sumFlatState->value, sumFlatState->valueState, ptr, rowCount);
                } else {
                    auto conditionArray = nullMap->convertToArray(rowCount);
                    simd::ReduceWithNullsExternal<InType, ResultType, AggValueState, StateValueHandler,
                        simd::ReduceFunc::Sum>(&sumFlatState->value, sumFlatState->valueState, ptr, rowCount,
                        conditionArray.data());
                }
            } else {
                auto *ptr = reinterpret_cast<InType *>(GetValuesFromDict<IN_ID>(vector));
                auto *indexMap = GetIdsFromDict<IN_ID>(vector) + rowOffset;
                if (nullMap == nullptr) {
                    simd::ReduceWithDicExternal<InType, ResultType, AggValueState, StateValueHandler,
                        simd::ReduceFunc::Sum>(&sumFlatState->value, sumFlatState->valueState, ptr, rowCount, indexMap);
                } else {
                    AddDictConditional<InType, ResultType, AggValueState,
                        SumConditionalOp<InType, ResultType, AggValueState, StateValueHandler, false, false>>(
                        &sumFlatState->value, sumFlatState->valueState, ptr, rowCount, *nullMap, indexMap);
                }
            }
        } else {
            ProcessSingleInternalFinal(state, vector, rowOffset, rowCount, nullMap);
        }
    }

    size_t GetStateSize() override
    {
        return sizeof(SumFlatState);
    }

    void InitState(AggregateState *state) override
    {
        SumFlatState *sumFlatState = SumFlatState::CastState(state + aggStateOffset);
        sumFlatState->value = ResultType{};
        sumFlatState->valueState = AggValueState::EMPTY_VALUE;
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
            SumFlatState *state = SumFlatState::CastState(groupStates[rowIndex] + aggStateOffset);
            if (state->valueState == AggValueState::EMPTY_VALUE) {
                // set null for empty group(all rows are NULL) when spill to ensure skip empty group when unspill
                spillValueVec->SetNull(rowIndex);
            } else {
                spillValueVec->SetValue(rowIndex, state->value);
            }
        }
    }

    void ExtractValues(const AggregateState *state, std::vector<BaseVector *> &vectors, int32_t rowIndex) override
    {
        auto v = static_cast<Vector<ResultType> *>(vectors[0]);
        const SumFlatState *sumFlatState = SumFlatState::ConstCastState(state + aggStateOffset);

        // state.count == 0 means all data is null or no data be accumulated,
        // we will set null
        if (sumFlatState->valueState == AggValueState::EMPTY_VALUE) {
            v->SetNull(rowIndex);
            return;
        }

        // we can not distinguish whether value is overflow when stage.value is null
        v->SetValue(rowIndex, sumFlatState->value);
    }

    void ExtractValuesBatch(std::vector<AggregateState *> &groupStates, std::vector<BaseVector *> &vectors,
        int32_t rowOffset, int32_t rowCount) override
    {
        auto v = static_cast<Vector<ResultType> *>(vectors[0]);
        for (int32_t rowIndex = 0; rowIndex < rowCount; rowIndex++) {
            SumFlatState *sumFlatState = SumFlatState::CastState(groupStates[rowIndex] + aggStateOffset);
            if (sumFlatState->valueState == AggValueState::EMPTY_VALUE ||
                sumFlatState->valueState == AggValueState::OVERFLOWED) {
                v->SetNull(rowIndex);
                continue;
            }
            // we can not distinguish whether value is overflow when stage.value is null
            v->SetValue(rowIndex, static_cast<ResultType>(sumFlatState->value));
        }
    }

    void ProcessAlignAggSchema(VectorBatch *result, BaseVector *originVector,
        const std::shared_ptr<NullsHelper> nullMap, const bool aggFilter) override
    {
        int rowCount = originVector->GetSize();
        // opt branch
        if constexpr (std::is_same_v<InType, ResultType>) {
            if (!aggFilter) {
                auto sumVector = VectorHelper::SliceVector(originVector, 0, rowCount);
                result->Append(sumVector);
                return;
            }
        }

        if (originVector->GetEncoding() == OMNI_DICTIONARY) {
            ProcessAlignAggSchemaInternal<Vector<DictionaryContainer<InType>>>(result, originVector, nullMap);
        } else {
            ProcessAlignAggSchemaInternal<Vector<InType>>(result, originVector, nullMap);
        }
    }

    template<typename T>
    void ProcessAlignAggSchemaInternal(VectorBatch *result, BaseVector *originVector,
        const std::shared_ptr<NullsHelper> nullMap)
    {
        int rowCount = originVector->GetSize();
        auto sumVector = reinterpret_cast<Vector<ResultType> *>(VectorHelper::CreateFlatVector(this->pool_, OUT_ID, rowCount));
        // The varchar type is converted to the double type in advance, so InType can't be the varchar type.
        auto vector = reinterpret_cast<T *>(originVector);
        if (nullMap != nullptr) {
            for (int index = 0; index < rowCount; ++index) {
                if ((*nullMap)[index]) {
                    sumVector->SetNull(index);
                } else {
                    sumVector->SetValue(index, (ResultType)vector->GetValue(index));
                }
            }
        } else {
            for (int index = 0; index < rowCount; ++index) {
                sumVector->SetValue(index, (ResultType)vector->GetValue(index));
            }
        }
        result->Append(sumVector);
    }
};
}
}
#endif // OMNI_RUNTIME_SUM_FLAT_IM_AGGREGATOR_H
