/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2024. All rights reserved.
 * Description: average aggregate for intermedia data vector are multi vectors
 *
 *
 */
#ifndef OMNI_RUNTIME_AVERAGE_FLAT_IM_AGGREGATOR_H
#define OMNI_RUNTIME_AVERAGE_FLAT_IM_AGGREGATOR_H

#include <cstring>
#include "sum_flatim_aggregator.h"

namespace omniruntime {
namespace op {
template <DataTypeId IN_ID, DataTypeId OUT_ID = OMNI_DOUBLE> class AverageFlatIMAggregator : public TypedAggregator {
    using RawInputType = typename AggNativeAndVectorType<IN_ID>::type;
    using ResultType = typename AggNativeAndVectorType<OUT_ID>::type;
    using RawInputVectorType = Vector<RawInputType>;
    static constexpr bool kMixedStateSerdeSupported =
        IsMixedSerdeArithmeticType<IN_ID>() && IsMixedSerdeArithmeticType<OUT_ID>();

#pragma pack(push, 1)
    struct AvgFlatState : BaseCountState<ResultType> {
        static const AverageFlatIMAggregator<IN_ID, OUT_ID>::AvgFlatState *ConstCastState(const AggregateState *state)
        {
            return reinterpret_cast<const AverageFlatIMAggregator<IN_ID, OUT_ID>::AvgFlatState *>(state);
        }

        static AverageFlatIMAggregator<IN_ID, OUT_ID>::AvgFlatState *CastState(AggregateState *state)
        {
            return reinterpret_cast<AverageFlatIMAggregator<IN_ID, OUT_ID>::AvgFlatState *>(state);
        }

        template <typename TypeIn, typename TypeOut> static void UpdateState(AggregateState *state, const TypeIn &in)
        {
            auto *avgState = CastState(state);
            SumOp<TypeIn, TypeOut, int64_t, StateCountHandler, false>(&(avgState->value), avgState->count, in, 1ULL);
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
    AverageFlatIMAggregator(const DataTypes &inputTypes, const DataTypes &outputTypes, std::vector<int32_t> &channels,
        bool inputRaw, bool outputPartial, bool isOverflowAsNull)
        : TypedAggregator(OMNI_AGGREGATION_TYPE_AVG, inputTypes, outputTypes, channels, inputRaw, outputPartial,
        isOverflowAsNull)
    {}

    ~AverageFlatIMAggregator() override = default;

    size_t GetStateSize() override
    {
        return sizeof(AvgFlatState);
    }

    static void MergeMixedStateImpl(AverageFlatIMAggregator<IN_ID, OUT_ID> *, AvgFlatState *targetState,
        const AvgFlatState *sourceState)
    {
        if constexpr (kMixedStateSerdeSupported) {
            if (sourceState->IsOverFlowed()) {
                targetState->SetOverFlow();
            } else if (!sourceState->IsEmpty()) {
                SumOp<ResultType, ResultType, int64_t, StateCountHandler, false>(&targetState->value, targetState->count,
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
            return RawMixedStateSerde<AvgFlatState, AverageFlatIMAggregator<IN_ID, OUT_ID>,
                &AverageFlatIMAggregator::MergeMixedStateImpl>::Ops();
        } else {
            return nullptr;
        }
    }

    void InitState(AggregateState *state) override
    {
        AvgFlatState *avgFlatState = AvgFlatState::CastState(state + aggStateOffset);
        avgFlatState->count = 0;
        avgFlatState->value = ResultType{};
    }

    // groupState will offset aggStateOffset in initState()
    void InitStates(std::vector<AggregateState *> &groupStates) override
    {
        for (auto groupState : groupStates) {
            // Init state will change state to state + aggStateOffset
            InitState(groupState);
        }
    }

    void ProcessSingleInternal(AggregateState *state, BaseVector *vector, const int32_t rowOffset,
        const int32_t rowCount, const std::shared_ptr<NullsHelper> nullMap)
    {
        AvgFlatState *avgFlatState = AvgFlatState::CastState(state);
        if (this->inputRaw) {
            if (vector->GetEncoding() == vec::OMNI_ENCODING_CONST) {
                if (nullMap == nullptr) {
                    auto constValue = static_cast<vec::ConstVector<RawInputType> *>(vector)->GetConstValue();
                    for (int32_t i = 0; i < rowCount; ++i) {
                        SumOp<RawInputType, ResultType, int64_t, StateCountHandler, false>(
                            &avgFlatState->value, avgFlatState->count, constValue, 1ULL);
                    }
                }
            } else if (vector->GetEncoding() != vec::OMNI_DICTIONARY) {
                auto *ptr = reinterpret_cast<RawInputType *>(GetValuesFromVector<IN_ID>(vector));
                ptr += rowOffset;
                if (nullMap == nullptr) {
                    simd::ReduceExternal<RawInputType, ResultType, int64_t, StateCountHandler, simd::ReduceFunc::Sum>(
                        &avgFlatState->value, avgFlatState->count, ptr, rowCount);
                } else {
                    auto conditionArray = nullMap->convertToArray(rowCount);
                    simd::ReduceWithNullsExternal<RawInputType, ResultType, int64_t, StateCountHandler,
                        simd::ReduceFunc::Sum>(&avgFlatState->value, avgFlatState->count, ptr, rowCount,
                        conditionArray.data());
                }
            } else {
                auto *ptr = reinterpret_cast<RawInputType *>(GetValuesFromDict<IN_ID>(vector));
                auto *indexMap = GetIdsFromDict<IN_ID>(vector) + rowOffset;
                if (nullMap == nullptr) {
                    simd::ReduceWithDicExternal<RawInputType, ResultType, int64_t, StateCountHandler,
                        simd::ReduceFunc::Sum>(&avgFlatState->value, avgFlatState->count, ptr, rowCount, indexMap);
                } else {
                    AddDictConditional<RawInputType, ResultType, int64_t,
                        SumConditionalOp<RawInputType, ResultType, int64_t, StateCountHandler, false, false>>(
                        &avgFlatState->value, avgFlatState->count, ptr, rowCount, *nullMap, indexMap);
                }
            }
        } else {
            using InType = double;

            // when input is not raw, vector is container with <double, long> columns for <sum, count>
            auto *sumVector = this->curVectorBatch->Get(this->channels[0]);
            auto *cntVector = this->curVectorBatch->Get(this->channels[1]);

            // no dict in Vector<T> when input is not raw
            auto *ptr = reinterpret_cast<double *>(GetValuesFromVector<IN_ID>(sumVector));
            auto *cntPtr = reinterpret_cast<int64_t *>(GetValuesFromVector<OMNI_LONG>(cntVector));
            ptr += rowOffset;
            cntPtr += rowOffset;
            if (nullMap == nullptr) {
                AddAvg<InType, ResultType, SumOp<InType, ResultType, int64_t, StateCountHandler, false>>(
                    &avgFlatState->value, avgFlatState->count, ptr, cntPtr, rowCount);
            } else {
                AddConditionalAvg<InType, ResultType,
                    SumConditionalOp<InType, ResultType, int64_t, StateCountHandler, false, false>>(
                    &avgFlatState->value, avgFlatState->count, ptr, cntPtr, rowCount, *nullMap);
            }
        }
    }

    void ProcessGroupInternal(std::vector<AggregateState *> &rowStates, BaseVector *vector, const int32_t rowOffset,
        const std::shared_ptr<NullsHelper> nullMap)
    {
        if (this->inputRaw) {
            if (vector->GetEncoding() == vec::OMNI_ENCODING_CONST) {
                if (nullMap == nullptr) {
                    auto constValue = static_cast<vec::ConstVector<RawInputType> *>(vector)->GetConstValue();
                    for (size_t i = 0; i < rowStates.size(); ++i) {
                        AvgFlatState::template UpdateState<RawInputType, ResultType>(rowStates[i] + aggStateOffset, constValue);
                    }
                }
            } else if (vector->GetEncoding() != vec::OMNI_DICTIONARY) {
                auto *ptr = reinterpret_cast<RawInputType *>(GetValuesFromVector<IN_ID>(vector));
                ptr += rowOffset;
                if (nullMap == nullptr) {
                    AddUseRowIndex<RawInputType, AvgFlatState::template UpdateState<RawInputType, ResultType>>(
                        rowStates, aggStateOffset, ptr);
                } else {
                    AddConditionalUseRowIndex<RawInputType,
                        AvgFlatState::template UpdateStateWithCondition<RawInputType, ResultType, false>>(rowStates,
                        aggStateOffset, ptr, *nullMap);
                }
            } else {
                auto *ptr = reinterpret_cast<RawInputType *>(GetValuesFromDict<IN_ID>(vector));
                auto *indexMap = GetIdsFromDict<IN_ID>(vector) + rowOffset;
                if (nullMap == nullptr) {
                    AddDictUseRowIndex<RawInputType, AvgFlatState::template UpdateState<RawInputType, ResultType>>(
                        rowStates, aggStateOffset, ptr, indexMap);
                } else {
                    AddDictConditionalUseRowIndex<RawInputType, ResultType,
                        AvgFlatState::template UpdateStateWithCondition<RawInputType, ResultType, false>>(rowStates,
                        aggStateOffset, ptr, *nullMap, indexMap);
                }
            }
        } else {
            using InType = double;
            // when input is not raw, vector is <doubleVector, longVector> columns for <sum, count>
            auto *sumVector = this->curVectorBatch->Get(this->channels[0]);
            auto *cntVector = this->curVectorBatch->Get(this->channels[1]);

            auto *ptr = reinterpret_cast<double *>(GetValuesFromVector<OMNI_DOUBLE>(sumVector));
            auto *cntPtr = reinterpret_cast<int64_t *>(GetValuesFromVector<OMNI_LONG>(cntVector));
            ptr += rowOffset;
            cntPtr += rowOffset;
            if (nullMap == nullptr) {
                AddUseRowIndexAvg<InType, ResultType, AvgFlatState,
                    SumOp<InType, ResultType, int64_t, StateCountHandler, false>>(rowStates, aggStateOffset, ptr,
                    cntPtr);
            } else {
                // Reza: can we use customize float operation similar to sumConditionalFloat
                AddConditionalUseRowIndexAvg<InType, ResultType, AvgFlatState,
                    SumConditionalOp<InType, ResultType, int64_t, StateCountHandler, false, false>>(rowStates,
                    aggStateOffset, ptr, cntPtr, *nullMap);
            }
        }
    }

    std::vector<DataTypePtr> GetSpillType() override
    {
        std::vector<DataTypePtr> spillTypes;
        spillTypes.emplace_back(std::make_shared<DataType>(OMNI_DOUBLE));
        spillTypes.emplace_back(std::make_shared<DataType>(OMNI_LONG));
        return spillTypes;
    }

    void ProcessGroupUnspill(std::vector<UnspillRowInfo> &unspillRows, int32_t rowCount, int32_t &vectorIndex) override
    {
        auto firstVecIdx = vectorIndex++;
        auto secondVecIdx = vectorIndex++;
        for (int32_t rowIdx = 0; rowIdx < rowCount; rowIdx++) {
            auto &row = unspillRows[rowIdx];
            auto batch = row.batch;
            auto index = row.rowIdx;
            auto sumVector = static_cast<Vector<ResultType> *>(batch->Get(firstVecIdx));
            if (!sumVector->IsNull(index)) {
                auto value = sumVector->GetValue(index);
                auto countVector = static_cast<Vector<int64_t> *>(batch->Get(secondVecIdx));
                auto count = countVector->GetValue(index);
                auto *state = AvgFlatState::CastState(row.state + aggStateOffset);
                SumOp<ResultType, ResultType, int64_t, StateCountHandler, false>(&state->value, state->count, value,
                    count);
            }
        }
    }

    void ExtractValuesForSpill(std::vector<AggregateState *> &groupStates, std::vector<BaseVector *> &vectors) override
    {
        auto avgValVector = static_cast<Vector<ResultType> *>(vectors[0]);
        auto avgCountVector = static_cast<Vector<int64_t> *>(vectors[1]);

        auto rowCount = static_cast<int32_t>(groupStates.size());
        for (int32_t rowIndex = 0; rowIndex < rowCount; rowIndex++) {
            auto *state = AvgFlatState::CastState(groupStates[rowIndex] + aggStateOffset);
            if (state->IsEmpty()) {
                // set null for empty group(all rows are NULL) when spill to ensure skip empty group when unspill
                avgValVector->SetNull(rowIndex);
            } else {
                avgValVector->SetValue(rowIndex, state->value);
                avgCountVector->SetValue(rowIndex, state->count);
            }
        }
    }

    void ExtractValues(const AggregateState *state, std::vector<BaseVector *> &vectors, int32_t rowIndex) override
    {
        const AvgFlatState *avgFlatState = AvgFlatState::ConstCastState(state + aggStateOffset);
        if (this->outputPartial) {
            auto avgValVector = static_cast<Vector<double> *>(vectors[0]);
            auto avgCountVector = static_cast<Vector<int64_t> *>(vectors[1]);

            if (avgFlatState->IsEmpty()) {
                // all input are nulls, return 0
                avgValVector->SetValue(rowIndex, 0);
                avgCountVector->SetValue(rowIndex, 0);
                return;
            }

            avgValVector->SetValue(rowIndex, avgFlatState->value);
            avgCountVector->SetValue(rowIndex, avgFlatState->count);
        } else {
            auto avgValVector = static_cast<Vector<double> *>(vectors[0]);

            if (avgFlatState->IsEmpty()) {
                // only contains null
                avgValVector->SetNull(rowIndex);
                return;
            }

            ResultType currentVal = avgFlatState->value;

            auto result = currentVal / avgFlatState->count;
            avgValVector->SetValue(rowIndex, result);
        }
    }

    void ExtractValuesBatch(std::vector<AggregateState *> &groupStates, std::vector<BaseVector *> &vectors,
        int32_t rowOffset, int32_t rowCount) override
    {
        if (this->outputPartial) {
            auto avgValVector = static_cast<Vector<double> *>(vectors[0]);
            auto avgCountVector = static_cast<Vector<int64_t> *>(vectors[1]);
            for (int32_t rowIndex = 0; rowIndex < rowCount; rowIndex++) {
                auto *state = AvgFlatState::CastState(groupStates[rowIndex] + aggStateOffset);
                if (state->IsEmpty()) {
                    // all input are nulls, return 0
                    avgValVector->SetValue(rowIndex, 0);
                    avgCountVector->SetValue(rowIndex, 0);
                    continue;
                }
                avgValVector->SetValue(rowIndex, state->value);
                avgCountVector->SetValue(rowIndex, state->count);
            }
        } else {
            auto avgValVector = static_cast<Vector<double> *>(vectors[0]);
            for (int32_t rowIndex = 0; rowIndex < rowCount; rowIndex++) {
                auto *state = AvgFlatState::CastState(groupStates[rowIndex] + aggStateOffset);
                if (state->IsEmpty()) {
                    // only contains null
                    avgValVector->SetNull(rowIndex);
                    continue;
                }

                ResultType currentVal = state->value;

                auto result = currentVal / state->count;
                avgValVector->SetValue(rowIndex, result);
            }
        }
    }

    void ProcessAlignAggSchema(VectorBatch *result, BaseVector *originVector,
        const std::shared_ptr<NullsHelper> nullMap, const bool aggFilter) override
    {
        int rowCount = originVector->GetSize();
        // opt branch
        if constexpr (std::is_same_v<RawInputType, ResultType>) {
            if (nullMap == nullptr) {
                auto sumVector = VectorHelper::SliceVector(originVector, 0, rowCount);
                auto countVector = VectorHelper::CreateFlatVector(OMNI_LONG, rowCount);
                int64_t *valueAddr = reinterpret_cast<int64_t *>(GetValuesFromVector<OMNI_LONG>(countVector));
                std::fill_n(valueAddr, rowCount, 1);
                result->Append(sumVector);
                result->Append(countVector);
                return;
            }
        }

        if (originVector->GetEncoding() == OMNI_DICTIONARY) {
            ProcessAlignAggSchemaInternal<Vector<DictionaryContainer<RawInputType>>>(result, originVector, nullMap);
        } else {
            ProcessAlignAggSchemaInternal<Vector<RawInputType>>(result, originVector, nullMap);
        }
    }

protected:
    // logic: Template-based vector encoding type, to avoid long functions and high depth.
    template <typename T>
    void ProcessAlignAggSchemaInternal(VectorBatch *result, BaseVector *originVector,
        const std::shared_ptr<NullsHelper> nullMap)
    {
        int rowCount = originVector->GetSize();
        auto sumVector = reinterpret_cast<Vector<ResultType> *>(VectorHelper::CreateFlatVector(OUT_ID, rowCount));
        auto countVector = reinterpret_cast<Vector<int64_t> *>(VectorHelper::CreateFlatVector(OMNI_LONG, rowCount));

        auto vector = reinterpret_cast<T *>(originVector);
        if (nullMap != nullptr) {
            for (int index = 0; index < rowCount; ++index) {
                if ((*nullMap)[index]) {
                    sumVector->SetValue(index, 0);
                    countVector->SetValue(index, 0);
                } else {
                    sumVector->SetValue(index, (ResultType)vector->GetValue(index));
                    countVector->SetValue(index, 1);
                }
            }
        } else {
            for (int index = 0; index < rowCount; ++index) {
                sumVector->SetValue(index, (ResultType)vector->GetValue(index));
            }
            int64_t *valueAddr = reinterpret_cast<int64_t *>(GetValuesFromVector<OMNI_LONG>(countVector));
            std::fill_n(valueAddr, rowCount, 1);
        }

        result->Append(sumVector);
        result->Append(countVector);
    }
};
}
}
#endif // OMNI_RUNTIME_AVERAGE_FLAT_IM_AGGREGATOR_H
