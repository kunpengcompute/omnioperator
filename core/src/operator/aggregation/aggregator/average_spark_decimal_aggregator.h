/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2024. All rights reserved.
 * Description: Average aggregate for short decimal
 */
#ifndef OMNI_RUNTIME_AVERAGE_SPARK_DECIMAL_AGGREGATOR_H
#define OMNI_RUNTIME_AVERAGE_SPARK_DECIMAL_AGGREGATOR_H

#include <cstring>
#include "sum_spark_decimal_aggregator.h"
#include "type/decimal_operations.h"
#include "operations_aggregator.h"

namespace omniruntime {
namespace op {
static constexpr int32_t PARTIAL_AVG_OUTPUT_LENGTH = sizeof(DecimalAverageState);
template <DataTypeId InDecimalId, DataTypeId OutDecimalId>
class AverageSparkDecimalAggregator : public TypedAggregator {
public:
    using ResultType = typename AggNativeAndVectorType<OutDecimalId>::type;
    using InRawType = typename AggNativeAndVectorType<InDecimalId>::type;
    using ResultIntType = std::conditional_t<std::is_same_v<ResultType, Decimal128>, int128_t, int64_t>;

#pragma pack(push, 1)
    struct AvgSparkDecimalState : BaseCountState<ResultType> {
        static const AverageSparkDecimalAggregator<InDecimalId, OutDecimalId>::AvgSparkDecimalState *ConstCastState(
            const AggregateState *state)
        {
            return reinterpret_cast<
                const AverageSparkDecimalAggregator<InDecimalId, OutDecimalId>::AvgSparkDecimalState *>(state);
        }

        static AverageSparkDecimalAggregator<InDecimalId, OutDecimalId>::AvgSparkDecimalState *CastState(
            AggregateState *state)
        {
            return reinterpret_cast<AverageSparkDecimalAggregator<InDecimalId, OutDecimalId>::AvgSparkDecimalState *>(
                state);
        }

        template <typename TypeIn, typename TypeOut> static void UpdateState(AggregateState *state, const TypeIn &in)
        {
            auto *avgState = CastState(state);
            SumOp<TypeIn, TypeOut, int64_t, StateCountHandler>(&(avgState->value), avgState->count, in, 1ULL);
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

    AverageSparkDecimalAggregator(const DataTypes &inputTypes, const DataTypes &outputTypes,
        std::vector<int32_t> &channels, bool inputRaw, bool outputPartial, bool isOverflowAsNull)
        : TypedAggregator(OMNI_AGGREGATION_TYPE_AVG, inputTypes, outputTypes, channels, inputRaw, outputPartial,
        isOverflowAsNull)
    {}

    ~AverageSparkDecimalAggregator() override {}

    void InitState(AggregateState *state) override
    {
        AvgSparkDecimalState *avgSparkDecimalState = AvgSparkDecimalState::CastState(state + aggStateOffset);
        avgSparkDecimalState->count = 0;
        avgSparkDecimalState->value = ResultType{};
    }

    // groupState will offset aggStateOffset in initState()
    void InitStates(std::vector<AggregateState *> &groupStates) override
    {
        for (auto groupState : groupStates) {
            // Init state will change state to state + aggStateOffset
            InitState(groupState);
        }
    }

    size_t GetStateSize() override
    {
        return sizeof(AvgSparkDecimalState);
    }

    static void MergeMixedStateImpl(AverageSparkDecimalAggregator<InDecimalId, OutDecimalId> *,
        AvgSparkDecimalState *targetState, const AvgSparkDecimalState *sourceState)
    {
        if (sourceState->IsOverFlowed()) {
            targetState->SetOverFlow();
        } else if (!sourceState->IsEmpty()) {
            SumOp<ResultType, ResultType, int64_t, StateCountHandler>(&targetState->value, targetState->count,
                sourceState->value, sourceState->count);
        }
    }

    const MixedStateSerdeOps *GetMixedStateSerdeOps() const override
    {
        return RawMixedStateSerde<AvgSparkDecimalState,
            AverageSparkDecimalAggregator<InDecimalId, OutDecimalId>,
            &AverageSparkDecimalAggregator::MergeMixedStateImpl>::Ops();
    }

    void ProcessGroupInternal(std::vector<AggregateState *> &rowStates, BaseVector *vector, const int32_t rowOffset,
        const std::shared_ptr<NullsHelper> nullMap)
    {
        if (inputRaw) {
            if (vector->GetEncoding() == vec::OMNI_ENCODING_CONST) {
                if (nullMap == nullptr) {
                    auto constValue = static_cast<vec::ConstVector<InRawType> *>(vector)->GetConstValue();
                    for (size_t i = 0; i < rowStates.size(); ++i) {
                        AvgSparkDecimalState::template UpdateState<InRawType, ResultType>(
                            rowStates[i] + aggStateOffset, constValue);
                    }
                }
            } else if (vector->GetEncoding() != vec::OMNI_DICTIONARY) {
                auto *ptr = reinterpret_cast<InRawType *>(GetValuesFromVector<InDecimalId>(vector));
                ptr += rowOffset;
                if (nullMap == nullptr) {
                    AddUseRowIndex<InRawType, AvgSparkDecimalState::template UpdateState<InRawType, ResultType>>(
                        rowStates, aggStateOffset, ptr);
                } else {
                    // Reza: can we use customize float operation similar to sumConditionalFloat
                    AddConditionalUseRowIndex<InRawType,
                        AvgSparkDecimalState::template UpdateStateWithCondition<InRawType, ResultType, false>>(
                        rowStates, aggStateOffset, ptr, *nullMap);
                }
            } else {
                auto *ptr = reinterpret_cast<InRawType *>(GetValuesFromDict<InDecimalId>(vector));
                auto *indexMap = GetIdsFromDict<InDecimalId>(vector) + rowOffset;
                if (nullMap == nullptr) {
                    AddDictUseRowIndex<InRawType, AvgSparkDecimalState::template UpdateState<InRawType, ResultType>>(
                        rowStates, aggStateOffset, ptr, indexMap);
                } else {
                    AddDictConditionalUseRowIndex<InRawType, ResultType,
                        AvgSparkDecimalState::template UpdateStateWithCondition<InRawType, ResultType, false>>(
                        rowStates, aggStateOffset, ptr, *nullMap, indexMap);
                }
            }
        } else {
            auto *sumVector = curVectorBatch->Get(channels[0]);
            auto *ptr = reinterpret_cast<InRawType *>(GetValuesFromVector<InDecimalId>(sumVector));

            auto *avgCountVector = curVectorBatch->Get(channels[1]);
            auto *cntPtr = reinterpret_cast<int64_t *>(GetValuesFromVector<OMNI_LONG>(avgCountVector));
            ptr += rowOffset;
            cntPtr += rowOffset;

            if (nullMap == nullptr) {
                AddUseRowIndexAvg<InRawType, ResultType, AvgSparkDecimalState,
                    SumOp<InRawType, ResultType, int64_t, StateCountHandler>>(rowStates, aggStateOffset, ptr, cntPtr);
            } else {
                // Reza: can we use customize float operation similar to sumConditionalFloat
                AddConditionalUseRowIndexAvg<InRawType, ResultType, AvgSparkDecimalState,
                    SumConditionalOp<InRawType, ResultType, int64_t, StateCountHandler, false>>(rowStates,
                    aggStateOffset, ptr, cntPtr, *nullMap);
            }
        }
    }

    void ProcessGroupUnspill(std::vector<UnspillRowInfo> &unspillRows, int32_t rowCount, int32_t &vectorIndex) override
    {
        auto firstVecIdx = vectorIndex++;
        auto secondVecIdx = vectorIndex++;
        for (int32_t rowIdx = 0; rowIdx < rowCount; rowIdx++) {
            auto &row = unspillRows[rowIdx];
            auto batch = row.batch;
            auto index = row.rowIdx;
            auto *state = AvgSparkDecimalState::CastState(row.state + aggStateOffset);
            auto countVector = static_cast<Vector<int64_t> *>(batch->Get(secondVecIdx));
            auto count = countVector->GetValue(index);
            if (count < 0) {
                // overflow
                state->SetOverFlow();
            } else if (count == 0) {
                // we skipped in empty group case
                continue;
            } else {
                auto sumVector = static_cast<Vector<ResultType> *>(batch->Get(firstVecIdx));
                auto value = sumVector->GetValue(index);
                SumOp<ResultType, ResultType, int64_t, StateCountHandler>(&state->value, state->count, value, count);
            }
        }
    }

    void ProcessSingleInternal(AggregateState *state, BaseVector *vector, const int32_t rowOffset,
        const int32_t rowCount, const std::shared_ptr<NullsHelper> nullMap)
    {
        AvgSparkDecimalState *avgSparkDecimalState = AvgSparkDecimalState::CastState(state);
        if (inputRaw) {
            if (vector->GetEncoding() == vec::OMNI_ENCODING_CONST) {
                if (nullMap == nullptr) {
                    auto constValue = static_cast<vec::ConstVector<InRawType> *>(vector)->GetConstValue();
                    for (int32_t i = 0; i < rowCount; ++i) {
                        SumOp<InRawType, ResultType, int64_t, StateCountHandler>(
                            &avgSparkDecimalState->value, avgSparkDecimalState->count, constValue, 1ULL);
                    }
                }
            } else if (vector->GetEncoding() != vec::OMNI_DICTIONARY) {
                auto *ptr = reinterpret_cast<InRawType *>(GetValuesFromVector<InDecimalId>(vector));
                ptr += rowOffset;
                if (nullMap == nullptr) {
                    Add<InRawType, ResultType, int64_t, SumOp<InRawType, ResultType, int64_t, StateCountHandler>>(
                        reinterpret_cast<ResultType *>(&avgSparkDecimalState->value), avgSparkDecimalState->count, ptr,
                        rowCount);
                } else {
                    AddConditional<InRawType, ResultType, int64_t,
                        SumConditionalOp<InRawType, ResultType, int64_t, StateCountHandler, false>>(
                        &avgSparkDecimalState->value, avgSparkDecimalState->count, ptr, rowCount, *nullMap);
                }
            } else {
                auto *ptr = reinterpret_cast<InRawType *>(GetValuesFromDict<InDecimalId>(vector));
                auto *indexMap = GetIdsFromDict<InDecimalId>(vector) + rowOffset;
                if (nullMap == nullptr) {
                    AddDict<InRawType, ResultType, int64_t, SumOp<InRawType, ResultType, int64_t, StateCountHandler>>(
                        &avgSparkDecimalState->value, avgSparkDecimalState->count, ptr, rowCount, indexMap);
                } else {
                    AddDictConditional<InRawType, ResultType, int64_t,
                        SumConditionalOp<InRawType, ResultType, int64_t, StateCountHandler, false>>(
                        &avgSparkDecimalState->value, avgSparkDecimalState->count, ptr, rowCount, *nullMap, indexMap);
                }
            }
        } else {
            auto *sumVector = curVectorBatch->Get(channels[0]);
            auto *ptr = reinterpret_cast<InRawType *>(GetValuesFromVector<InDecimalId>(sumVector));

            auto *avgCountVector = curVectorBatch->Get(channels[1]);
            auto *cntPtr = reinterpret_cast<int64_t *>(GetValuesFromVector<OMNI_LONG>(avgCountVector));
            ptr += rowOffset;
            cntPtr += rowOffset;

            if (nullMap == nullptr) {
                AddAvg<InRawType, ResultType, SumOp<InRawType, ResultType, int64_t, StateCountHandler>>(
                    &avgSparkDecimalState->value, avgSparkDecimalState->count, ptr, cntPtr, rowCount);
            } else {
                AddConditionalAvg<InRawType, ResultType,
                    SumConditionalOp<InRawType, ResultType, int64_t, StateCountHandler, false>>(
                    &avgSparkDecimalState->value, avgSparkDecimalState->count, ptr, cntPtr, rowCount, *nullMap);
            }
        }
    }

    std::vector<DataTypePtr> GetSpillType() override
    {
        std::vector<DataTypePtr> spillTypes;
        if constexpr (InDecimalId == OMNI_DECIMAL64) {
            spillTypes.emplace_back(std::make_shared<DataType>(OutDecimalId));
        } else {
            spillTypes.emplace_back(std::make_shared<DataType>(OMNI_DECIMAL128));
        }
        spillTypes.emplace_back(std::make_shared<DataType>(OMNI_LONG));
        return spillTypes;
    }

    void ExtractValuesForSpill(std::vector<AggregateState *> &groupStates, std::vector<BaseVector *> &vectors) override
    {
        auto avgValVector = static_cast<Vector<ResultType> *>(vectors[0]);
        auto avgCountVector = static_cast<Vector<int64_t> *>(vectors[1]);
        auto rowCount = static_cast<int32_t>(groupStates.size());
        for (int32_t rowIndex = 0; rowIndex < rowCount; rowIndex++) {
            auto *state = AvgSparkDecimalState::CastState(groupStates[rowIndex] + aggStateOffset);

            if (state->IsOverFlowed()) {
                // overflow
                if (!this->IsOverflowAsNull()) {
                    throw OmniException("OPERATOR_RUNTIME_ERROR", "average_aggregator overflow.");
                }
                avgCountVector->SetValue(rowIndex, -1);
            } else if (state->IsEmpty()) {
                // set count to zero for empty group when spill to ensure skip empty group when unspill
                avgCountVector->SetValue(rowIndex, 0);
            } else {
                int128_t decodedDec;
                if constexpr (std::is_same_v<ResultType, Decimal128>) {
                    decodedDec = state->value.ToInt128();
                } else {
                    decodedDec = state->value;
                }

                if constexpr (std::is_same_v<ResultType, Decimal128>) {
                    Decimal128 decimal128Result(decodedDec);
                    avgValVector->SetValue(rowIndex, decimal128Result);
                } else {
                    avgValVector->SetValue(rowIndex, static_cast<int64_t>(decodedDec));
                }
                avgCountVector->SetValue(rowIndex, state->count);
            }
        }
    }

    void ExtractValues(const AggregateState *state, std::vector<BaseVector *> &vectors, int32_t rowIndex) override
    {
        BaseVector *vector = vectors[0];
        const AvgSparkDecimalState *avgSparkDecimalState = AvgSparkDecimalState::ConstCastState(state + aggStateOffset);
        int128_t decodedDec;
        if constexpr (std::is_same_v<ResultType, Decimal128>) {
            decodedDec = avgSparkDecimalState->value.ToInt128();
        } else {
            decodedDec = static_cast<ResultType>(avgSparkDecimalState->value);
        }

        int128_t countDec = avgSparkDecimalState->count;

        auto outputDecimalType = static_cast<DecimalDataType *>(outputTypes.GetType(0).get());
        auto inputDecimalType = static_cast<DecimalDataType *>(inputTypes.GetType(0).get());

        if (avgSparkDecimalState->IsOverFlowed()) {
            // overflow
            this->SetNullOrThrowException(vector, rowIndex);
            return;
        }

        if (outputPartial) {
            int32_t scaleDiff = outputDecimalType->GetScale() - inputDecimalType->GetScale();
            int128_t resultDec;
            // rescale dividend and divisor to output scale.
            // In Partial mode, if input type is Decimal(p,s), then, output type is Decimal(p+10,s).
            // So, it can not be overflowed.
            MulCheckedOverflow(decodedDec, TenOfInt128[scaleDiff], resultDec);

            if constexpr (std::is_same_v<ResultType, Decimal128>) {
                auto decimal128Vector = reinterpret_cast<Vector<Decimal128> *>(vector);
                Decimal128 decimal128Result(resultDec);
                static_cast<Vector<Decimal128> *>(decimal128Vector)->SetValue(rowIndex, decimal128Result);
            } else {
                auto longVector = reinterpret_cast<Vector<int64_t> *>(vector);
                int64_t shortResult = static_cast<int64_t>(resultDec);
                longVector->SetValue(rowIndex, shortResult);
            }

            BaseVector *avgCountVector = vectors[1];
            reinterpret_cast<Vector<int64_t> *>(avgCountVector)->SetValue(rowIndex, avgSparkDecimalState->count);
        } else {
            int128_t finalResultDec;
            // if count is zero, it means all input is null
            if (countDec == 0) {
                vector->SetNull(rowIndex);
                return;
            }
            OpStatus status = CalcAvg(inputDecimalType, decodedDec, countDec, outputDecimalType, finalResultDec);
            if (status == OpStatus::OP_OVERFLOW) {
                this->SetNullOrThrowException(vector, rowIndex);
                return;
            }
            // we can not use template std::is_same_v<resultType,Decimal128> here
            // for average, Decimal128 / n = Decimal64 is possible,
            // but all intermediate types such as ResultType are Decimal128,
            // so we have to get type from outputTypes , rather than ResultTyp
            if (outputTypes.GetIds()[0] == OMNI_DECIMAL128) {
                Decimal128 decimal128Result(finalResultDec);
                static_cast<Vector<Decimal128> *>(vector)->SetValue(rowIndex, decimal128Result);
            } else {
                int64_t shortResult = static_cast<int64_t>(finalResultDec);
                static_cast<Vector<int64_t> *>(vector)->SetValue(rowIndex, shortResult);
            }
        }
    }

    void ExtractValuesBatch(std::vector<AggregateState *> &groupStates, std::vector<BaseVector *> &vectors,
        int32_t rowOffset, int32_t rowCount) override
    {
        auto outputDecimalType = static_cast<DecimalDataType *>(this->outputTypes.GetType(0).get());
        auto inputDecimalType = static_cast<DecimalDataType *>(this->inputTypes.GetType(0).get());
        BaseVector *vector = vectors[0];

        if (outputPartial) {
            int32_t scaleDiff = outputDecimalType->GetScale() - inputDecimalType->GetScale();
            auto scaleNum = TenOfInt128[scaleDiff];
            for (int32_t rowIndex = 0; rowIndex < rowCount; rowIndex++) {
                auto *state = AvgSparkDecimalState::CastState(groupStates[rowIndex] + aggStateOffset);
                if (state->IsOverFlowed()) {
                    // overflow
                    this->SetNullOrThrowException(vector, rowIndex);
                    continue;
                }

                int128_t decodedDec;
                if constexpr (std::is_same_v<ResultType, Decimal128>) {
                    decodedDec = state->value.ToInt128();
                } else {
                    decodedDec = state->value;
                }
                int128_t resultDec;
                // rescale dividend and divisor to output scale.
                // In Partial mode, if input type is Decimal(p,s), then, output type is Decimal(p+10,s).
                // So, it can not be overflowed.
                __builtin_mul_overflow(decodedDec, scaleNum, &resultDec);

                if constexpr (std::is_same_v<ResultType, Decimal128>) {
                    auto decimal128Vector = reinterpret_cast<Vector<Decimal128> *>(vector);
                    Decimal128 decimal128Result(resultDec);
                    static_cast<Vector<Decimal128> *>(decimal128Vector)->SetValue(rowIndex, decimal128Result);
                } else {
                    auto longVector = reinterpret_cast<Vector<int64_t> *>(vector);
                    int64_t shortResult = static_cast<int64_t>(resultDec);
                    longVector->SetValue(rowIndex, shortResult);
                }

                BaseVector *avgCountVector = vectors[1];
                reinterpret_cast<Vector<int64_t> *>(avgCountVector)->SetValue(rowIndex, state->count);
            }
        } else {
            for (int32_t rowIndex = 0; rowIndex < rowCount; rowIndex++) {
                auto *state = AvgSparkDecimalState::CastState(groupStates[rowIndex] + aggStateOffset);
                if (state->IsOverFlowed()) {
                    // overflow
                    this->SetNullOrThrowException(vector, rowIndex);
                    continue;
                }

                int128_t decodedDec;
                if constexpr (std::is_same_v<ResultType, Decimal128>) {
                    decodedDec = state->value.ToInt128();
                } else {
                    decodedDec = state->value;
                }

                int128_t countDec = state->count;
                int128_t finalResultDec;
                // if count is zero, it means all input is null
                if (countDec == 0) {
                    vector->SetNull(rowIndex);
                    continue;
                }
                OpStatus status = CalcAvg(inputDecimalType, decodedDec, countDec, outputDecimalType, finalResultDec);
                if (status == OpStatus::OP_OVERFLOW) {
                    this->SetNullOrThrowException(vector, rowIndex);
                    continue;
                }
                // we can not use template std::is_same_v<resultType,Decimal128> here
                // for average, Decimal128 / n = Decimal64 is possible,
                // but all intermediate types such as ResultType are Decimal128,
                // so we have to get type from outputTypes , rather than ResultTyp
                if (outputDecimalType->GetId() == OMNI_DECIMAL128) {
                    Decimal128 decimal128Result(finalResultDec);
                    static_cast<Vector<Decimal128> *>(vector)->SetValue(rowIndex, decimal128Result);
                } else {
                    int64_t shortResult = static_cast<int64_t>(finalResultDec);
                    static_cast<Vector<int64_t> *>(vector)->SetValue(rowIndex, shortResult);
                }
            }
        }
    }

    void ProcessAlignAggSchema(VectorBatch *result, BaseVector *originVector,
        const std::shared_ptr<NullsHelper> nullMap, const bool aggFilter) override
    {
        int rowCount = originVector->GetSize();
        // opt branch
        if constexpr (std::is_same_v<InRawType, ResultType>) {
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
            ProcessAlignAggSchemaInternal<Vector<DictionaryContainer<InRawType>>>(result, originVector, nullMap);
        } else {
            ProcessAlignAggSchemaInternal<Vector<InRawType>>(result, originVector, nullMap);
        }
    }

protected:
    // logic: Template-based vector encoding type, to avoid long functions and high depth.
    template<typename T>
    void ProcessAlignAggSchemaInternal(VectorBatch *result, BaseVector *originVector,
        const std::shared_ptr<NullsHelper> nullMap)
    {
        int rowCount = originVector->GetSize();
        auto sumVector = reinterpret_cast<Vector<ResultType> *>(VectorHelper::CreateFlatVector(OutDecimalId, rowCount));
        auto countVector = reinterpret_cast<Vector<int64_t> *>(VectorHelper::CreateFlatVector(OMNI_LONG, rowCount));

        auto vector = reinterpret_cast<T *>(originVector);
        if (nullMap != nullptr) {
            for (int index = 0; index < rowCount; ++index) {
                if ((*nullMap)[index]) {
                    sumVector->SetValue(index, 0);
                    countVector->SetValue(index, 0);
                } else {
                    if constexpr (std::is_same_v<ResultType, Decimal128>) {
                        Decimal128 d = Decimal128(vector->GetValue(index));
                        sumVector->SetValue(index, d);
                    } else {
                        sumVector->SetValue(index, (ResultType)vector->GetValue(index));
                    }
                    countVector->SetValue(index, 1);
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
            int64_t *valueAddr = reinterpret_cast<int64_t *>(GetValuesFromVector<OMNI_LONG>(countVector));
            std::fill_n(valueAddr, rowCount, 1);
        }

        result->Append(sumVector);
        result->Append(countVector);
    }

private:
    // calculate avg=sum/count, and rescale to the result Decimal Type
    OpStatus CalcAvg(DecimalDataType *sumType, int128_t &sumDec, int128_t &countDec, DecimalDataType *outputDecimalType,
        int128_t &finalResultDec)
    {
        int32_t sumPrec = sumType->GetPrecision();
        int32_t sumScale = sumType->GetScale();
        // for raw input type Decimal(p,s)
        // in final mode, aggregator's input type(sumType) is Decimal(p+10,s)
        // window operator only has one stage, and it's input type(sumType) is Decimal(p,s), so precision need to +10
        if (inputRaw && !outputPartial) {
            sumPrec += 10;
            sumPrec = std::min(sumPrec, MAX_PRECISION);
        }
        // before calculate avg, try to check if overflowed when cast sum and count to the wider type
        if (IsCastToWiderTypeOverflow(sumDec, sumPrec, sumScale, countDec)) {
            return OpStatus::OP_OVERFLOW;
        }

        int32_t divideResultPrec = 0;
        int32_t divideResultScale = 0;
        GetDivideResultDecimalType(sumPrec, sumScale, divideResultPrec, divideResultScale);
        int128_t dividend;
        // rescale dividend and divisor to divideResultScale(see GetDivideResultDecimalType)
        bool isOverflow = MulCheckedOverflow(sumDec, TenOfInt128[divideResultScale - sumScale], dividend);
        int128_t avgResultDec;
        DivideRoundUp(dividend, countDec, avgResultDec);

        // avg = sum/count 's result should rescale to the output DecimalType
        int32_t diffScale = outputDecimalType->GetScale() - divideResultScale;
        if (diffScale >= 0) {
            isOverflow = isOverflow || MulCheckedOverflow(avgResultDec, TenOfInt128[diffScale], finalResultDec);
        } else {
            DivideRoundUp(avgResultDec, TenOfInt128[-diffScale], finalResultDec);
        }

        return isOverflow ? OpStatus::OP_OVERFLOW : OpStatus::SUCCESS;
    }
    // set vector value null or throw exception when overflow
    void SetNullOrThrowException(BaseVector *vector, int index)
    {
        if (!IsOverflowAsNull()) {
            throw OmniException("OPERATOR_RUNTIME_ERROR", "Overflow in avg of decimals");
        }
        vector->SetNull(index);
    }

    // GetDivideResultDecimalType get the sum/count result decimal type, and count's type is always Decimal(20,0)
    void GetDivideResultDecimalType(int32_t sumPrec, int32_t sumScale, int32_t &resultPrec, int32_t &resultScale)
    {
        int32_t scale = std::max(MINIMUM_ADJUSTED_SCALE, sumScale + COUNT_PRECISION + 1);
        int32_t prec = sumPrec - sumScale + COUNT_SCALE + scale;

        if (prec <= MAX_PRECISION) {
            // Adjustment only needed when we exceed max precision
            resultPrec = prec;
            resultScale = scale;
            return;
        }

        // Precision/scale exceed maximum precision. Result must be adjusted to MAX_PRECISION.
        int32_t intDigits = prec - scale;
        // If original scale is less than MINIMUM_ADJUSTED_SCALE, use original scale value; otherwise
        // preserve at least MINIMUM_ADJUSTED_SCALE fractional digits
        int32_t minScaleValue = std::min(scale, MINIMUM_ADJUSTED_SCALE);
        // The resulting scale is the maximum between what is available without causing a loss of
        // digits for the integer part of the decimal and the minimum guaranteed scale, which is
        // computed above
        resultScale = std::max(MAX_SCALE - intDigits, minScaleValue);
        resultPrec = MAX_PRECISION;
    }

    // try to cast sum and count to the wider Decimal Type, return true if overflowed; return false if normal
    // the input sum and count will never be changed.
    bool IsCastToWiderTypeOverflow(int128_t &sum, int32_t sumPrec, int32_t sumScale, int128_t &count)
    {
        int32_t scale = std::max(sumScale, COUNT_SCALE);
        int32_t widerScale = std::min(scale, MAX_SCALE);

        int128_t sumRescale;
        bool sumStatus = MulCheckedOverflow(sum, TenOfInt128[widerScale - sumScale], sumRescale);
        if (sumStatus) {
            return true;
        }
        // count is an integer, no need to rescale it
        return false;
    }

private:
    static constexpr int32_t COUNT_PRECISION = 20;
    static constexpr int32_t COUNT_SCALE = 0;
    static constexpr int32_t MINIMUM_ADJUSTED_SCALE = 6;
};
}
}
#endif // OMNI_RUNTIME_AVERAGE_DECIMAL_AGGREGATOR_H
