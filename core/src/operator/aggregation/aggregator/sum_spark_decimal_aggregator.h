/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2024. All rights reserved.
 * Description: Aggregate factories
 */

#ifndef OMNI_RUNTIME_SUM_SPARK_DECIMAL_AGGREGATOR_H
#define OMNI_RUNTIME_SUM_SPARK_DECIMAL_AGGREGATOR_H

#include <cstring>
#include "aggregator.h"
#include "type/decimal_operations.h"
#include "type/decimal128.h"

namespace omniruntime {
namespace op {
template <typename InDecimalType, typename OutDecimalType, bool HasNullFlag, typename StateType>
VECTORIZE_LOOP NO_INLINE void AddDecimalUseRowIndex(std::vector<AggregateState *> &rowStates, const size_t aggOffset,
    const InDecimalType *__restrict dataPtr, const bool *__restrict emptyPtr,
    const std::shared_ptr<NullsHelper> nullMap = nullptr)
{
    bool isOverflow = false;
    auto rowCount = rowStates.size();
    using ResultIntType = std::conditional_t<std::is_same_v<OutDecimalType, Decimal128>, int128_t, int64_t>;

    for (size_t i = 0; i < rowCount; ++i) {
        auto *state = StateType::CastState(rowStates[i] + aggOffset);
        if (state->IsOverFlowed()) {
            // cur state overflow, so no need to aggregate
            continue;
        }

        ResultIntType tmpResult = 0;
        if constexpr (HasNullFlag) {
            if ((*nullMap)[i]) {
                // partial stage overflow , so no need to do aggregation in final
                state->valueState = AggValueState::OVERFLOWED;
                continue;
            }
        }

        if (not emptyPtr[i]) {
            if constexpr (std::is_same_v<OutDecimalType, Decimal128>) {
                auto &res = state->value;
                tmpResult = res.ToInt128();
                if constexpr (std::is_same_v<InDecimalType, Decimal128>) {
                    // decimal128 + decimal128 = decimal128
                    isOverflow = AddCheckedOverflow(tmpResult, dataPtr[i].ToInt128(), tmpResult);
                } else {
                    // decimal64 + decimal64 = decimal128
                    isOverflow = AddCheckedOverflow(tmpResult, int128_t(dataPtr[i]), tmpResult);
                }
                res = OutDecimalType(tmpResult);
            } else {
                // decimal64 + decimal64 = decimal64
                tmpResult = state->value;
                isOverflow = __builtin_add_overflow(tmpResult, dataPtr[i], &tmpResult);
                state->value = OutDecimalType(tmpResult);
            }

            if (isOverflow) {
                state->valueState = AggValueState::OVERFLOWED;
            } else {
                state->valueState = AggValueState::NORMAL;
            }
        }
    }
}

/**
 * SUM agg data type
 * input: decimal
 * middle: decimal+boolean(isEmpty)
 * final: decimal
 */
template <DataTypeId InDecimalId, DataTypeId OutDecimalId> class SumSparkDecimalAggregator : public TypedAggregator {
public:
    using ResultType = typename AggNativeAndVectorType<OutDecimalId>::type;
    using ResultIntType = std::conditional_t<std::is_same_v<ResultType, Decimal128>, int128_t, int64_t>;
    using InRawType = typename AggNativeAndVectorType<InDecimalId>::type;
#pragma pack(push, 1)
    struct SumSparkDecimalState : BaseState<ResultType> {
        static const SumSparkDecimalAggregator<InDecimalId, OutDecimalId>::SumSparkDecimalState *ConstCastState(
            const AggregateState *state)
        {
            return reinterpret_cast<const SumSparkDecimalAggregator<InDecimalId, OutDecimalId>::SumSparkDecimalState *>(
                state);
        }

        static SumSparkDecimalAggregator<InDecimalId, OutDecimalId>::SumSparkDecimalState *CastState(
            AggregateState *state)
        {
            return reinterpret_cast<SumSparkDecimalAggregator<InDecimalId, OutDecimalId>::SumSparkDecimalState *>(
                state);
        }

        template <typename TypeIn, typename TypeOut> static void UpdateState(AggregateState *state, const TypeIn &in)
        {
            auto *maxState = CastState(state);
            SumOp<TypeIn, TypeOut, AggValueState, StateValueHandler>(&(maxState->value), maxState->valueState, in,
                1ULL);
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

    SumSparkDecimalAggregator(const DataTypes &inputTypes, const DataTypes &outputTypes, std::vector<int32_t> &channels,
        bool inputRaw, bool outputPartial, bool isOverflowAsNull)
        : TypedAggregator(OMNI_AGGREGATION_TYPE_SUM, inputTypes, outputTypes, channels, inputRaw, outputPartial,
        isOverflowAsNull)
    {}

    SumSparkDecimalAggregator(FunctionType aggregateType, const DataTypes &inputTypes, const DataTypes &outputTypes,
        std::vector<int32_t> &channels, const bool inputRaw, const bool outputPartial, const bool isOverflowAsNull)
        : TypedAggregator(aggregateType, inputTypes, outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull)
    {}

    ~SumSparkDecimalAggregator() override = default;

    std::vector<DataTypePtr> GetSpillType() override
    {
        std::vector<DataTypePtr> spillTypes;
        spillTypes.emplace_back(outputTypes.GetType(0));
        return spillTypes;
    }

    size_t GetStateSize() override
    {
        return sizeof(SumSparkDecimalState);
    }

    static void MergeMixedStateImpl(SumSparkDecimalAggregator<InDecimalId, OutDecimalId> *,
        SumSparkDecimalState *targetState, const SumSparkDecimalState *sourceState)
    {
        if (sourceState->IsOverFlowed()) {
            targetState->SetOverFlow();
        } else if (!sourceState->IsEmpty()) {
            SumOp<ResultType, ResultType, AggValueState, StateValueHandler, false>(&targetState->value,
                targetState->valueState, sourceState->value, 1LL);
        }
    }

    const MixedStateSerdeOps *GetMixedStateSerdeOps() const override
    {
        return RawMixedStateSerde<SumSparkDecimalState, SumSparkDecimalAggregator<InDecimalId, OutDecimalId>,
            &SumSparkDecimalAggregator::MergeMixedStateImpl>::Ops();
    }

    template <bool isOutputPartial>
    void ExtractValuesForSpillInternal(std::vector<AggregateState *> &groupStates, std::vector<BaseVector *> &vectors)
    {
        auto spillVec = static_cast<Vector<ResultType> *>(vectors[0]);
        auto rowCount = static_cast<int32_t>(groupStates.size());
        for (int32_t rowIndex = 0; rowIndex < rowCount; rowIndex++) {
            SumSparkDecimalState *state = SumSparkDecimalState::CastState(groupStates[rowIndex] + aggStateOffset);
            bool isOverflow = (state->IsOverFlowed());
            bool isEmpty = (state->IsEmpty());
            // use null and value to distinguish empty group, overflow and other normal case
            if (isEmpty) {
                // set null for empty group(all rows are NULL) when spill to ensure skip empty group when unspill
                spillVec->SetNull(rowIndex);
                int128_t emptyValue = SPILL_EMPTY_VALUE;
                SetValToVector(spillVec, rowIndex, emptyValue);
            } else if (isOverflow) {
                int128_t overflowValue = SPILL_OVERFLOW_VALUE;
                SetValToVector(spillVec, rowIndex, overflowValue);
                if constexpr (isOutputPartial) {
                    spillVec->SetNull(rowIndex);
                } else {
                    SetNullOrThrowException(spillVec, rowIndex);
                }
            } else {
                int128_t decodedDec;
                if constexpr (std::is_same_v<ResultType, Decimal128>) {
                    decodedDec = state->value.ToInt128();
                } else {
                    decodedDec = state->value;
                }
                SetValToVector(spillVec, rowIndex, decodedDec);
            }
        }
    }

    void ExtractValuesForSpill(std::vector<AggregateState *> &groupStates, std::vector<BaseVector *> &vectors) override
    {
        if (outputPartial) {
            ExtractValuesForSpillInternal<true>(groupStates, vectors);
        } else {
            ExtractValuesForSpillInternal<false>(groupStates, vectors);
        }
    }

    void ExtractValues(const AggregateState *state, std::vector<BaseVector *> &vectors, int32_t rowIndex) override
    {
        const SumSparkDecimalState *sumSparkDecimalState = SumSparkDecimalState::ConstCastState(state + aggStateOffset);
        BaseVector *vector = vectors[0];
        int128_t resultDec = 0;
        bool isOverflow = (sumSparkDecimalState->valueState == AggValueState::OVERFLOWED);
        bool isEmpty = (sumSparkDecimalState->valueState == AggValueState::EMPTY_VALUE);

        if (outputPartial) {
            if (isOverflow) {
                vector->SetNull(rowIndex);
            } else if (isEmpty) {
                SetValToVector(vector, rowIndex, resultDec);
            } else {
                int128_t decodedDec = 0;
                if constexpr (std::is_same_v<ResultType, Decimal128>) {
                    decodedDec = sumSparkDecimalState->value.ToInt128();
                } else {
                    decodedDec = sumSparkDecimalState->value;
                }

                // only support output scale >= input scale
                // for spark, input type is always decimal. for olk, input type is varbinary and the precision
                // and scale are zero.
                int32_t scaleDiff = static_cast<DecimalDataType *>(outputTypes.GetType(0).get())->GetScale() -
                    static_cast<DecimalDataType *>(inputTypes.GetType(0).get())->GetScale();
                // rescale dividend and divisor to output scale
                isOverflow = MulCheckedOverflow(decodedDec, TenOfInt128[scaleDiff], resultDec);
                if (isOverflow) {
                    vector->SetNull(rowIndex);
                } else {
                    SetValToVector(vector, rowIndex, resultDec);
                }
            }
            BaseVector *emptyVector = vectors[1];
            reinterpret_cast<Vector<bool> *>(emptyVector)->SetValue(rowIndex, isEmpty);
        } else {
            if (isOverflow) {
                SetNullOrThrowException(vector, rowIndex);
            } else if (isEmpty) {
                vector->SetNull(rowIndex);
            } else {
                int128_t decodedDec = 0;
                if constexpr (std::is_same_v<ResultType, Decimal128>) {
                    decodedDec = sumSparkDecimalState->value.ToInt128();
                } else {
                    decodedDec = sumSparkDecimalState->value;
                }

                // only support output scale >= input scale
                // for spark, input type is always decimal. for olk, input type is varbinary and the precision
                // and scale are zero.
                int32_t scaleDiff = static_cast<DecimalDataType *>(outputTypes.GetType(0).get())->GetScale() -
                    static_cast<DecimalDataType *>(inputTypes.GetType(0).get())->GetScale();
                // rescale dividend and divisor to output scale
                isOverflow = MulCheckedOverflow(decodedDec, TenOfInt128[scaleDiff], resultDec);
                if (isOverflow) {
                    SetNullOrThrowException(vector, rowIndex);
                } else {
                    SetValToVector(vector, rowIndex, resultDec);
                }
            }
        }
    }

    template <bool OUTPUT_PARTIAL>
    void ExtractValuesBatchInternal(std::vector<AggregateState *> &groupStates, std::vector<BaseVector *> &vectors,
        int32_t rowOffset, int32_t rowCount)
    {
        // only support output scale >= input scale
        // for spark, input type is always decimal. for olk, input type is varbinary and the precision
        // and scale are zero.
        int32_t scaleDiff = static_cast<DecimalDataType *>(outputTypes.GetType(0).get())->GetScale() -
            static_cast<DecimalDataType *>(inputTypes.GetType(0).get())->GetScale();
        auto scaleNum = TenOfInt128[scaleDiff];
        BaseVector *vector = vectors[0];
        for (int32_t rowIndex = 0; rowIndex < rowCount; rowIndex++) {
            SumSparkDecimalState *state = SumSparkDecimalState::CastState(groupStates[rowIndex] + aggStateOffset);
            bool isOverflow = (state->IsOverFlowed());
            bool isEmpty = (state->IsEmpty());
            if constexpr (OUTPUT_PARTIAL) {
                reinterpret_cast<Vector<bool> *>(vectors[1])->SetValue(rowIndex, isEmpty);
            }

            int128_t decodedDec = 0;
            if (isOverflow) {
                if constexpr (OUTPUT_PARTIAL) {
                    // partial output vector is sum, it will be set to NULL if overflowed.
                    vector->SetNull(rowIndex);
                } else {
                    SetNullOrThrowException(vector, rowIndex);
                }
            } else if (isEmpty) {
                if constexpr (OUTPUT_PARTIAL) {
                    SetValToVector(vector, rowIndex, decodedDec);
                } else {
                    vector->SetNull(rowIndex);
                }
            } else {
                if constexpr (std::is_same_v<ResultType, Decimal128>) {
                    decodedDec = state->value.ToInt128();
                } else {
                    decodedDec = state->value;
                }
                int128_t resultDec;
                // rescale dividend and divisor to output scale
                isOverflow = MulCheckedOverflow(decodedDec, scaleNum, resultDec);
                if (isOverflow) {
                    if constexpr (OUTPUT_PARTIAL) {
                        // partial output vector is sum, it will be set to NULL if overflowed.
                        vector->SetNull(rowIndex);
                    } else {
                        SetNullOrThrowException(vector, rowIndex);
                    }
                } else {
                    SetValToVector(vector, rowIndex, resultDec);
                }
            }
        }
    }

    // The outputType is either OMNI_DECIMAL64 or OMNI_DECIMAL128
    void ExtractValuesBatch(std::vector<AggregateState *> &groupStates, std::vector<BaseVector *> &vectors,
        int32_t rowOffset, int32_t rowCount) override
    {
        if (outputPartial) {
            ExtractValuesBatchInternal<true>(groupStates, vectors, rowOffset, rowCount);
        } else {
            ExtractValuesBatchInternal<false>(groupStates, vectors, rowOffset, rowCount);
        }
    }

    void InitState(AggregateState *state) override
    {
        SumSparkDecimalState *sumSparkDecimalState = SumSparkDecimalState::CastState(state + aggStateOffset);
        sumSparkDecimalState->valueState = AggValueState::EMPTY_VALUE;
        sumSparkDecimalState->value = ResultType{};
    }

    // groupState will offset aggStateOffset in initState()
    void InitStates(std::vector<AggregateState *> &groupStates) override
    {
        for (auto groupState : groupStates) {
            // Init state will change state to state + aggStateOffset
            InitState(groupState);
        }
    }

    void ProcessGroupInternalFinal(std::vector<AggregateState *> &rowStates, BaseVector *dataVector,
        const int32_t rowOffset, const std::shared_ptr<NullsHelper> nullMap)
    {
        // final stage : input vector will be Vector<Decimal128> or Vector<Decimal64>
        auto *dataPtr = reinterpret_cast<InRawType *>(GetValuesFromVector<InDecimalId>(dataVector));
        dataPtr += rowOffset;
        auto *emptyVector = curVectorBatch->Get(channels[1]);
        auto *emptyPtr = reinterpret_cast<const bool *>(GetValuesFromVector<type::OMNI_BOOLEAN>(emptyVector));
        emptyPtr += rowOffset;
        if (nullMap == nullptr) {
            AddDecimalUseRowIndex<InRawType, ResultType, false, SumSparkDecimalState>(rowStates, aggStateOffset,
                dataPtr, emptyPtr);
        } else {
            AddDecimalUseRowIndex<InRawType, ResultType, true, SumSparkDecimalState>(rowStates, aggStateOffset, dataPtr,
                emptyPtr, nullMap);
        }
    }

    void ProcessGroupUnspill(std::vector<UnspillRowInfo> &unspillRows, int32_t rowCount, int32_t &vectorIndex) override
    {
        auto firstVecIdx = vectorIndex++;
        for (int32_t rowIdx = 0; rowIdx < rowCount; rowIdx++) {
            auto &row = unspillRows[rowIdx];
            auto batch = row.batch;
            auto index = row.rowIdx;
            auto sumVector = static_cast<Vector<ResultType> *>(batch->Get(firstVecIdx));
            SumSparkDecimalState *state = SumSparkDecimalState::CastState(row.state + aggStateOffset);
            auto value = sumVector->GetValue(index);
            if (!sumVector->IsNull(index)) {
                // normal case
                if constexpr (std::is_same_v<ResultType, Decimal128>) {
                    SumOp<ResultType, ResultType, AggValueState, StateValueHandler, false>(&state->value,
                        state->valueState, value, 1LL);
                } else {
                    SumOp<ResultType, ResultType, AggValueState, StateValueHandler, false>(&state->value,
                        state->valueState, value, 1LL);
                }
            } else {
                // empty group or overflow case
                // if it is overflow we set -1
                // if it is empty group we skipped
                int128_t resultIntValue;
                if constexpr (std::is_same_v<ResultType, Decimal128>) {
                    resultIntValue = value.ToInt128();
                } else {
                    resultIntValue = value;
                }
                if (resultIntValue == SPILL_OVERFLOW_VALUE) {
                    state->valueState = AggValueState::OVERFLOWED;
                }
            }
        }
    }

    void ProcessGroupInternal(std::vector<AggregateState *> &rowStates, BaseVector *vector, const int32_t rowOffset,
        const std::shared_ptr<NullsHelper> nullMap)
    {
        if (inputRaw) {
            if (vector->GetEncoding() == vec::OMNI_ENCODING_CONST) {
                if (nullMap == nullptr) {
                    auto constValue = static_cast<vec::ConstVector<InRawType> *>(vector)->GetConstValue();
                    for (size_t i = 0; i < rowStates.size(); ++i) {
                        SumSparkDecimalState::template UpdateState<InRawType, ResultType>(
                            rowStates[i] + aggStateOffset, constValue);
                    }
                }
            } else if (vector->GetEncoding() != vec::OMNI_DICTIONARY) {
                auto *ptr = reinterpret_cast<InRawType *>(GetValuesFromVector<InDecimalId>(vector));
                ptr += rowOffset;
                if (nullMap == nullptr) {
                    AddUseRowIndex<InRawType, SumSparkDecimalState::template UpdateState<InRawType, ResultType>>(
                        rowStates, aggStateOffset, ptr);
                } else {
                    // Reza: can we use customize float operation similar to sumConditionalFloat
                    AddConditionalUseRowIndex<InRawType,
                        SumSparkDecimalState::template UpdateStateWithCondition<InRawType, ResultType, false>>(
                        rowStates, aggStateOffset, ptr, *nullMap);
                }
            } else {
                auto *ptr = reinterpret_cast<InRawType *>(GetValuesFromDict<InDecimalId>(vector));
                auto *indexMap = GetIdsFromDict<InDecimalId>(vector) + rowOffset;
                if (nullMap == nullptr) {
                    AddDictUseRowIndex<InRawType, SumSparkDecimalState::template UpdateState<InRawType, ResultType>>(
                        rowStates, aggStateOffset, ptr, indexMap);
                } else {
                    AddDictConditionalUseRowIndex<InRawType, ResultType,
                        SumSparkDecimalState::template UpdateStateWithCondition<InRawType, ResultType, false>>(
                        rowStates, aggStateOffset, ptr, *nullMap, indexMap);
                }
            }
        } else {
            ProcessGroupInternalFinal(rowStates, vector, rowOffset, nullMap);
        }
    }

    void ProcessSingleInternalFinal(AggregateState *state, BaseVector *vector, const int32_t rowOffset,
        const int32_t rowCount, const std::shared_ptr<NullsHelper> conditionMap)
    {
        auto *ptr = reinterpret_cast<InRawType *>(GetValuesFromVector<InDecimalId>(vector));

        auto *emptyVector = curVectorBatch->Get(channels[1]);
        auto *emptyPtr = reinterpret_cast<bool *>(GetValuesFromVector<type::OMNI_BOOLEAN>(emptyVector));
        ptr += rowOffset;
        emptyPtr += rowOffset;
        SumSparkDecimalState *sumSparkDecimalState = SumSparkDecimalState::CastState(state);
        if (conditionMap == nullptr) {
            for (int32_t i = 0; i < rowCount; ++i) {
                if (sumSparkDecimalState->IsOverFlowed()) {
                    // means overflow in final stage, no need to calculate remaining data
                    break;
                }
                if (not emptyPtr[i]) {
                    // the emptyPtr here means partial result is null
                    SumOp<InRawType, ResultType, AggValueState, StateValueHandler>(&sumSparkDecimalState->value,
                        sumSparkDecimalState->valueState, ptr[i], 1LL);
                }
            }
        } else {
            for (int32_t i = 0; i < rowCount; ++i) {
                if (sumSparkDecimalState->IsOverFlowed()) {
                    // means overflow in final stage, no need to calculate remaining data
                    break;
                }
                if (not (*conditionMap)[i]) {
                    if (not emptyPtr[i]) {
                        // the emptyPtr here means partial result is null
                        SumOp<InRawType, ResultType, AggValueState, StateValueHandler>(&sumSparkDecimalState->value,
                            sumSparkDecimalState->valueState, ptr[i], 1LL);
                    }
                } else {
                    // means partial overflow , no need to calculate remaining data
                    sumSparkDecimalState->valueState = AggValueState::OVERFLOWED;
                    break;
                }
            }
        }
    }

    void ProcessSingleInternal(AggregateState *state, BaseVector *vector, const int32_t rowOffset,
        const int32_t rowCount, const std::shared_ptr<NullsHelper> nullMap)
    {
        SumSparkDecimalState *sumSparkDecimalState = SumSparkDecimalState::CastState(state);
        if (inputRaw) {
            if (vector->GetEncoding() == vec::OMNI_ENCODING_CONST) {
                if (nullMap == nullptr) {
                    auto constValue = static_cast<vec::ConstVector<InRawType> *>(vector)->GetConstValue();
                    for (int32_t i = 0; i < rowCount; ++i) {
                        SumOp<InRawType, ResultType, AggValueState, StateValueHandler>(
                            &sumSparkDecimalState->value, sumSparkDecimalState->valueState, constValue, 1ULL);
                    }
                }
            } else if (vector->GetEncoding() != vec::OMNI_DICTIONARY) {
                auto *ptr = reinterpret_cast<InRawType *>(GetValuesFromVector<InDecimalId>(vector));
                ptr += rowOffset;
                if (nullMap == nullptr) {
                    Add<InRawType, ResultType, AggValueState,
                        SumOp<InRawType, ResultType, AggValueState, StateValueHandler>>(
                        reinterpret_cast<ResultType *>(&sumSparkDecimalState->value), sumSparkDecimalState->valueState,
                        ptr, rowCount);
                } else {
                    AddConditional<InRawType, ResultType, AggValueState,
                        SumConditionalOp<InRawType, ResultType, AggValueState, StateValueHandler, false>>(
                        &sumSparkDecimalState->value, sumSparkDecimalState->valueState, ptr, rowCount, *nullMap);
                }
            } else {
                auto *ptr = reinterpret_cast<InRawType *>(GetValuesFromDict<InDecimalId>(vector));
                auto *indexMap = GetIdsFromDict<InDecimalId>(vector) + rowOffset;
                if (nullMap == nullptr) {
                    AddDict<InRawType, ResultType, AggValueState,
                        SumOp<InRawType, ResultType, AggValueState, StateValueHandler>>(&sumSparkDecimalState->value,
                        sumSparkDecimalState->valueState, ptr, rowCount, indexMap);
                } else {
                    AddDictConditional<InRawType, ResultType, AggValueState,
                        SumConditionalOp<InRawType, ResultType, AggValueState, StateValueHandler, false>>(
                        &sumSparkDecimalState->value, sumSparkDecimalState->valueState, ptr, rowCount, *nullMap,
                        indexMap);
                }
            }
        } else {
            ProcessSingleInternalFinal(state, vector, rowOffset, rowCount, nullMap);
        }
    }

    void ProcessAlignAggSchema(VectorBatch *result, BaseVector *originVector,
        const std::shared_ptr<NullsHelper> nullMap, const bool aggFilter) override
    {
        int rowCount = originVector->GetSize();
        // opt: if InRawType and ResultType are same type, directly setValue
        if constexpr (std::is_same_v<InRawType, ResultType>) {
            auto sumVector = VectorHelper::SliceVector(originVector, 0, rowCount);
            auto emptyVector = reinterpret_cast<Vector<bool> *>(VectorHelper::CreateFlatVector(OMNI_BOOLEAN, rowCount));
            if (nullMap == nullptr) {
                bool *valueAddr = reinterpret_cast<bool *>(GetValuesFromVector<OMNI_BOOLEAN>(emptyVector));
                std::fill_n(valueAddr, rowCount, false);
            } else {
                for (int index = 0; index < rowCount; ++index) {
                    if ((*nullMap)[index]) {
                        emptyVector->SetValue(index, true);
                    } else {
                        emptyVector->SetValue(index, false);
                    }
                }
            }
            result->Append(sumVector);
            result->Append(emptyVector);
            return;
        }

        if (originVector->GetEncoding() == OMNI_DICTIONARY) {
            ProcessAlignAggSchemaInternal<Vector<DictionaryContainer<InRawType>>>(result, originVector, nullMap);
        } else {
            ProcessAlignAggSchemaInternal<Vector<InRawType>>(result, originVector, nullMap);
        }
    }

    // logic: Template-based vector encoding type, to avoid long functions and high depth.
    template<typename T>
    void ProcessAlignAggSchemaInternal(VectorBatch *result, BaseVector *originVector,
        const std::shared_ptr<NullsHelper> nullMap)
    {
        int rowCount = originVector->GetSize();
        auto sumVector = reinterpret_cast<Vector<ResultType> *>(VectorHelper::CreateFlatVector(OutDecimalId, rowCount));
        auto emptyVector = reinterpret_cast<Vector<bool> *>(VectorHelper::CreateFlatVector(OMNI_BOOLEAN, rowCount));
        auto vector = reinterpret_cast<T *>(originVector);
        if (nullMap != nullptr) {
            for (int index = 0; index < rowCount; ++index) {
                if ((*nullMap)[index]) {
                    sumVector->SetValue(index, (ResultType)(0));
                    emptyVector->SetValue(index, true);
                } else {
                    if constexpr (std::is_same_v<ResultType, Decimal128>) {
                        Decimal128 d = Decimal128(vector->GetValue(index));
                        sumVector->SetValue(index, d);
                    } else {
                        sumVector->SetValue(index, (ResultType)vector->GetValue(index));
                    }
                    emptyVector->SetValue(index, false);
                }
            }
        } else {
            for (int index = 0; index < rowCount; ++index) {
                if constexpr (std::is_same_v<ResultType, Decimal128>) {
                    Decimal128 d = Decimal128(vector->GetValue(index));
                    sumVector->SetValue(index, d);
                } else {
                    sumVector->SetValue(index, (ResultType)vector->GetValue(index));
                }
            }
            bool *valueAddr = reinterpret_cast<bool *>(GetValuesFromVector<OMNI_BOOLEAN>(emptyVector));
            std::fill_n(valueAddr, rowCount, false);
        }
        result->Append(sumVector);
        result->Append(emptyVector);
    }

private:
    // set vector value null or throw exception when overflow
    void SetNullOrThrowException(BaseVector *vector, int index)
    {
        if (!IsOverflowAsNull()) {
            throw OmniException("OPERATOR_RUNTIME_ERROR", "Overflow in sum of decimals");
        }
        vector->SetNull(index);
    }

    // Set decimal value to output vector in Extract function. The outputType is either OMNI_DECIMAL64 or
    // OMNI_DECIMAL128.
    void SetValToVector(BaseVector *vector, int32_t rowIndex, int128_t &deciVal)
    {
        if constexpr (std::is_same_v<ResultType, Decimal128>) {
            Decimal128 decimal128Val(deciVal);
            static_cast<Vector<Decimal128> *>(vector)->SetValue(rowIndex, decimal128Val);
        } else {
            int64_t longVal = static_cast<int64_t>(deciVal);
            static_cast<Vector<int64_t> *>(vector)->SetValue(rowIndex, longVal);
        }
    }

    static constexpr int128_t SPILL_EMPTY_VALUE = 0;
    static constexpr int128_t SPILL_OVERFLOW_VALUE = -1;
};
}
}

#endif // OMNI_RUNTIME_SUM_SPARK_DECIMAL_AGGREGATOR_H
