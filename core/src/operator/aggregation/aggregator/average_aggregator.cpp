/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2024. All rights reserved.
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 *
 * Description: Average aggregate
 */

#include "average_aggregator.h"
#include "operator/aggregation/vector_getter.h"

namespace omniruntime {
namespace op {
template <DataTypeId IN_ID, DataTypeId OUT_ID>
template <bool PARTIAL_OUT, bool DECIMAL_PRECISION_IMPROVEMENT>
void AverageAggregator<IN_ID, OUT_ID>::ExtractValuesFunction(const AggregateState *state,
    std::vector<BaseVector *> &vectors, int32_t rowIndex)
{
    auto *avgState = AvgState::ConstCastState(state + SumAggregator<IN_ID, OUT_ID>::aggStateOffset);
    if constexpr (PARTIAL_OUT) {
        if constexpr (OUT_ID == OMNI_VARCHAR) {
            SumAggregator<IN_ID, OUT_ID>::ExtractValues(state, vectors, rowIndex);
        } else if constexpr (OUT_ID == OMNI_CONTAINER) {
            OutType result{};
            auto *vector = static_cast<ContainerVector *>(vectors[0]);
            auto *doubleVector = reinterpret_cast<OutVector *>(vector->GetValue(0));
            auto *longVector = reinterpret_cast<Vector<int64_t> *>(vector->GetValue(1));

            bool overflow = avgState->count < 0;
            if (avgState->count > 0) {
                result = this->template CastWithOverflow<ResultType, OutType>(avgState->value, overflow);
            }

            doubleVector->SetValue(rowIndex, result);
            longVector->SetValue(rowIndex, overflow ? 0 : avgState->count);

            if (overflow && !this->IsOverflowAsNull()) {
                throw OmniException("OPERATOR_RUNTIME_ERROR", "average_aggregator overflow.");
            }
        } else {
            throw OmniException("Unreachable code", "Reached unreachable code in average aggregator extract partial");
        }
    } else {
        OutType result{};
        auto v = static_cast<OutVector *>(vectors[0]);
        bool overflow = avgState->count < 0;
        if (avgState->count > 0) {
            if constexpr (std::is_same_v<ResultType, Decimal128>) {
                DivideWithOverflow<DECIMAL_PRECISION_IMPROVEMENT>(avgState, result, overflow);
            } else if constexpr (std::is_same_v<ResultType, double>) {
                // Result type is double, we generate double avgResult
                double avgResult = static_cast<double>(avgState->value) / static_cast<double>(avgState->count);
                result = this->template CastWithOverflow<double, OutType>(avgResult, overflow);
            } else {
                // Result type is int64, we generate double avgResult
                double avgResult = static_cast<double>(static_cast<ResultType>(avgState->value)) /
                    static_cast<double>(avgState->count);
                result = this->template CastWithOverflow<double, OutType>(avgResult, overflow);
            }
        }

        v->SetValue(rowIndex, result);
        if (overflow) {
            this->SetNullOrThrowException(v, rowIndex, "average_aggregator overflow.");
        } else if (avgState->count == 0) {
            v->SetNull(rowIndex);
        }
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
template <bool PARTIAL_OUT, bool DECIMAL_PRECISION_IMPROVEMENT, bool IS_OVERFLOW_AS_NULL>
void AverageAggregator<IN_ID, OUT_ID>::ExtractValuesBatchInternal(std::vector<AggregateState *> &groupStates,
    std::vector<BaseVector *> &vectors, int32_t rowOffset, int32_t rowCount)
{
    if constexpr (PARTIAL_OUT) {
        if constexpr (OUT_ID == OMNI_VARCHAR) {
            SumAggregator<IN_ID, OUT_ID>::ExtractValuesBatch(groupStates, vectors, rowOffset, rowCount);
        } else if constexpr (OUT_ID == OMNI_CONTAINER) {
            auto *vector = static_cast<ContainerVector *>(vectors[0]);
            auto *doubleVector = reinterpret_cast<OutVector *>(vector->GetValue(0));
            auto *longVector = reinterpret_cast<Vector<int64_t> *>(vector->GetValue(1));

            for (int32_t rowIndex = 0; rowIndex < rowCount; rowIndex++) {
                auto *state = AvgState::CastState(groupStates[rowIndex] + SumAggregator<IN_ID, OUT_ID>::aggStateOffset);
                bool overflow = state->count < 0;
                OutType result{};
                if (state->count > 0) {
                    result = this->template CastWithOverflow<ResultType, OutType>(state->value, overflow);
                }
                doubleVector->SetValue(rowIndex, result);
                longVector->SetValue(rowIndex, overflow ? 0 : state->count);

                if (overflow && !this->IsOverflowAsNull()) {
                    throw OmniException("OPERATOR_RUNTIME_ERROR", "average_aggregator overflow.");
                }
            }
        } else {
            throw OmniException("Unreachable code", "Reached unreachable code in average aggregator extract partial");
        }
    } else {
        auto v = static_cast<OutVector *>(vectors[0]);
        for (int32_t rowIndex = 0; rowIndex < rowCount; rowIndex++) {
            auto *state = AvgState::CastState(groupStates[rowIndex] + SumAggregator<IN_ID, OUT_ID>::aggStateOffset);
            bool isOverflow = state->count < 0;
            bool isEmpty = state->count == 0;
            if (isOverflow) {
                this->SetNullOrThrowException(v, rowIndex, "average_aggregator overflow.");
            } else if (isEmpty) {
                v->SetNull(rowIndex);
            } else {
                OutType result{};
                if constexpr (std::is_same_v<ResultType, Decimal128>) {
                    DivideWithOverflow<DECIMAL_PRECISION_IMPROVEMENT>(state, result, isOverflow);
                } else if constexpr (std::is_same_v<ResultType, double>) {
                    // Result type is double, we generate double avgResult
                    double avgResult = static_cast<double>(state->value) / static_cast<double>(state->count);
                    result = this->template CastWithOverflow<double, OutType>(avgResult, isOverflow);
                } else {
                    // Result type is int64, we generate double avgResult
                    auto avgResult =
                        static_cast<double>(static_cast<ResultType>(state->value)) / static_cast<double>(state->count);
                    result = this->template CastWithOverflow<double, OutType>(avgResult, isOverflow);
                }
                if (isOverflow) {
                    this->SetNullOrThrowException(v, rowIndex, "average_aggregator overflow.");
                } else {
                    v->SetValue(rowIndex, result);
                }
            }
        }
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
template <bool DECIMAL_PRECISION_IMPROVEMENT>
void AverageAggregator<IN_ID, OUT_ID>::DivideWithOverflow(const AvgState *avgState, OutType &result, bool &overflow)
{
    int128_t result128 = avgState->value.ToInt128();
    if constexpr (DECIMAL_PRECISION_IMPROVEMENT && std::is_same_v<OutType, Decimal128>) {
        auto dividend = this->template CastWithOverflow<Decimal128, OutType>(Decimal128(result128), overflow);
        DivideRoundUp(dividend.ToInt128(), static_cast<int128_t>(avgState->count), result128);
        result = Decimal128(result128);
    } else {
        DivideRoundUp(result128, static_cast<int128_t>(avgState->count), result128);
        result = this->template CastWithOverflow<Decimal128, OutType>(Decimal128(result128), overflow);
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void AverageAggregator<IN_ID, OUT_ID>::ExtractValues(const AggregateState *state, std::vector<BaseVector *> &vectors,
    int32_t rowIndex)
{
    // average is different from other aggregator
    // sum aggregator will offset aggStateOffset , so average aggregator can not offset
    // but average will offset aggStateOffset in final stage
    // it will depend on extractValuesFuncPointer 's implement
    // we can not pass param like  state + SumAggregator<IN_ID, OUT_ID>::aggStateOffset
    (this->*extractValuesFuncPointer)(state, vectors, rowIndex);
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void AverageAggregator<IN_ID, OUT_ID>::ExtractValuesBatch(std::vector<AggregateState *> &groupStates,
    std::vector<BaseVector *> &vectors, int32_t rowOffset, int32_t rowCount)
{
    // varchar only in partial stage
    if constexpr (OUT_ID == OMNI_VARCHAR || OUT_ID == OMNI_CONTAINER) {
        if (this->IsOverflowAsNull()) {
            ExtractValuesBatchInternal<true, false, true>(groupStates, vectors, rowOffset, rowCount);
        } else {
            ExtractValuesBatchInternal<true, false, false>(groupStates, vectors, rowOffset, rowCount);
        }
    } else {
        if (ConfigUtil::GetSupportDecimalPrecisionImprovementRule() ==
            SupportDecimalPrecisionImprovementRule::IS_SUPPORT) {
            if (this->IsOverflowAsNull()) {
                ExtractValuesBatchInternal<false, true, true>(groupStates, vectors, rowOffset, rowCount);
            } else {
                ExtractValuesBatchInternal<false, true, false>(groupStates, vectors, rowOffset, rowCount);
            }
        } else {
            if (this->IsOverflowAsNull()) {
                ExtractValuesBatchInternal<false, false, true>(groupStates, vectors, rowOffset, rowCount);
            } else {
                ExtractValuesBatchInternal<false, false, false>(groupStates, vectors, rowOffset, rowCount);
            }
        }
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID> std::vector<DataTypePtr> AverageAggregator<IN_ID, OUT_ID>::GetSpillType()
{
    std::vector<DataTypePtr> spillTypes;
    if constexpr (IN_ID == OMNI_SHORT || IN_ID == OMNI_INT || IN_ID == OMNI_LONG) {
        spillTypes.emplace_back(std::make_shared<DataType>(OMNI_LONG));
    } else if constexpr (IN_ID == OMNI_DOUBLE || IN_ID == OMNI_CONTAINER) {
        spillTypes.emplace_back(std::make_shared<DataType>(OMNI_DOUBLE));
    } else {
        spillTypes.emplace_back(std::make_shared<DataType>(OMNI_DECIMAL128));
    }
    spillTypes.emplace_back(std::make_shared<DataType>(OMNI_LONG));
    return spillTypes;
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void AverageAggregator<IN_ID, OUT_ID>::ExtractValuesForSpill(std::vector<AggregateState *> &groupStates,
    std::vector<BaseVector *> &vectors)
{
    auto spillValueVec = static_cast<Vector<ResultType> *>(vectors[0]);
    auto spillCountVec = reinterpret_cast<Vector<int64_t> *>(vectors[1]);

    auto rowCount = static_cast<int32_t>(groupStates.size());
    for (int32_t rowIndex = 0; rowIndex < rowCount; rowIndex++) {
        auto *state = AvgState::CastState(groupStates[rowIndex] + SumAggregator<IN_ID, OUT_ID>::aggStateOffset);
        auto count = state->count;
        bool isOverflow = count < 0;
        bool isEmpty = count == 0;

        if (isOverflow) {
            this->SetNullOrThrowException(spillValueVec, rowIndex, "average_aggregator overflow.");
            // set -1 to count vector if it is overflow
            spillCountVec->SetValue(rowIndex, -1);
        } else if (isEmpty) {
            spillValueVec->SetNull(rowIndex);
            // set null for empty group(all rows are NULL) when spill to ensure skip empty group when unspill
            spillCountVec->SetValue(rowIndex, 0);
        } else {
            ResultType result = static_cast<ResultType>(state->value);
            spillValueVec->SetValue(rowIndex, result);
            spillCountVec->SetValue(rowIndex, count);
        }
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void AverageAggregator<IN_ID, OUT_ID>::ProcessSingleInternal(AggregateState *state, BaseVector *vector,
    const int32_t rowOffset, const int32_t rowCount, const std::shared_ptr<NullsHelper> nullMap)
{
    auto *avgState = AvgState::CastState(state);
    if constexpr (IN_ID == OMNI_CONTAINER) {
        // when input is not raw, vector is container with <double, long> columns for <sum, count>
        auto v = static_cast<ContainerVector *>(vector);
        auto *sumVector = reinterpret_cast<InVector *>(v->GetValue(0));

        auto *cntVector = reinterpret_cast<Vector<int64_t> *>(v->GetValue(1));

        if (vector->GetEncoding() != vec::OMNI_DICTIONARY) {
            auto *ptr = reinterpret_cast<InType *>(GetValuesFromVector<OMNI_DOUBLE>(sumVector));
            auto *cntPtr = reinterpret_cast<int64_t *>(GetValuesFromVector<OMNI_LONG>(cntVector));
            ptr += rowOffset;
            cntPtr += rowOffset;
            if (nullMap == nullptr) {
                AddAvg<InType, ResultType, SumOp<InType, ResultType, int64_t, StateCountHandler>>(
                    reinterpret_cast<ResultType *>(&avgState->value), avgState->count, ptr, cntPtr, rowCount);
            } else {
                if constexpr (std::is_floating_point_v<InType>) {
                    AvgConditionalFloat<InType, ResultType, false>(reinterpret_cast<ResultType *>(&avgState->value),
                        avgState->count, ptr, cntPtr, rowCount, *nullMap);
                } else {
                    AddConditionalAvg<InType, ResultType,
                        SumConditionalOp<InType, ResultType, int64_t, StateCountHandler, false>>(
                        reinterpret_cast<ResultType *>(&avgState->value), avgState->count, ptr, cntPtr, rowCount,
                        *nullMap);
                }
            }
        } else {
            auto *ptr = reinterpret_cast<InType *>(GetValuesFromDict<OMNI_DOUBLE>(sumVector));
            auto *cntPtr = reinterpret_cast<int64_t *>(GetValuesFromDict<OMNI_LONG>(cntVector));
            auto *indexMap = GetIdsFromDict<IN_ID>(vector) + rowOffset;
            if (nullMap == nullptr) {
                AddDictAvg<InType, ResultType, SumOp<InType, ResultType, int64_t, StateCountHandler>>(
                    reinterpret_cast<ResultType *>(&avgState->value), avgState->count, ptr, cntPtr, rowCount, indexMap);
            } else {
                AddDictConditionalAvg<InType, ResultType,
                    SumConditionalOp<InType, ResultType, int64_t, StateCountHandler, false>>(
                    reinterpret_cast<ResultType *>(&avgState->value), avgState->count, ptr, cntPtr, rowCount, *nullMap,
                    indexMap);
            }
        }
    } else {
        SumAggregator<IN_ID, OUT_ID>::ProcessSingleInternal(state, vector, rowOffset, rowCount, nullMap);
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void AverageAggregator<IN_ID, OUT_ID>::ProcessGroupInternal(std::vector<AggregateState *> &rowStates,
    BaseVector *vector, const int32_t rowOffset, const std::shared_ptr<NullsHelper> nullMap)
{
    if constexpr (IN_ID == OMNI_CONTAINER) {
        // when input is not raw, vector is container with <double, long> columns for <sum, count>
        auto *v = static_cast<ContainerVector *>(vector);
        auto *sumVector = reinterpret_cast<InVector *>(v->GetValue(0));
        auto *cntVector = reinterpret_cast<Vector<int64_t> *>(v->GetValue(1));

        if (vector->GetEncoding() != vec::OMNI_DICTIONARY) {
            auto *ptr = reinterpret_cast<InType *>(GetValuesFromVector<OMNI_DOUBLE>(sumVector));
            auto *cntPtr = reinterpret_cast<int64_t *>(GetValuesFromVector<OMNI_LONG>(cntVector));
            ptr += rowOffset;
            cntPtr += rowOffset;
            if (nullMap == nullptr) {
                AddUseRowIndexAvg<InType, ResultType, AvgState, SumOp<InType, ResultType, int64_t, StateCountHandler>>(
                    rowStates, SumAggregator<IN_ID, OUT_ID>::aggStateOffset, ptr, cntPtr);
            } else {
                // Reza: can we use customize float operation similar to sumConditionalFloat
                AddConditionalUseRowIndexAvg<InType, ResultType, AvgState,
                    SumConditionalOp<InType, ResultType, int64_t, StateCountHandler, false>>(rowStates,
                    SumAggregator<IN_ID, OUT_ID>::aggStateOffset, ptr, cntPtr, *nullMap);
            }
        } else {
            auto *ptr = reinterpret_cast<InType *>(GetValuesFromDict<OMNI_DOUBLE>(sumVector));
            auto *cntPtr = reinterpret_cast<int64_t *>(GetValuesFromDict<OMNI_LONG>(cntVector));
            auto *indexMap = GetIdsFromDict<IN_ID>(vector) + rowOffset;
            if (nullMap == nullptr) {
                AddDictUseRowIndexAvg<InType, ResultType, AvgState,
                    SumOp<InType, ResultType, int64_t, StateCountHandler>>(rowStates,
                    SumAggregator<IN_ID, OUT_ID>::aggStateOffset, ptr, cntPtr, indexMap);
            } else {
                AddDictConditionalUseRowIndexAvg<InType, ResultType, AvgState,
                    SumConditionalOp<InType, ResultType, int64_t, StateCountHandler, false>>(rowStates,
                    SumAggregator<IN_ID, OUT_ID>::aggStateOffset, ptr, cntPtr, *nullMap, indexMap);
            }
        }
    } else {
        SumAggregator<IN_ID, OUT_ID>::ProcessGroupInternal(rowStates, vector, rowOffset, nullMap);
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void AverageAggregator<IN_ID, OUT_ID>::ProcessGroupUnspill(std::vector<UnspillRowInfo> &unspillRows, int32_t rowCount,
    int32_t &vectorIndex)
{
    auto firstVecIdx = vectorIndex++;
    auto secondVecIdx = vectorIndex++;
    for (int32_t rowIdx = 0; rowIdx < rowCount; rowIdx++) {
        auto &row = unspillRows[rowIdx];
        auto batch = row.batch;
        auto index = row.rowIdx;
        auto *state = AvgState::CastState(row.state + SumAggregator<IN_ID, OUT_ID>::aggStateOffset);
        auto countVector = static_cast<Vector<int64_t> *>(batch->Get(secondVecIdx));
        auto count = countVector->GetValue(index);
        if (count < 0) {
            // we set -1 in overflow case
            state->count = -1;
        } else if (count == 0) {
            // we skipped in empty group case
            continue;
        } else {
            auto sumVector = static_cast<Vector<ResultType> *>(batch->Get(firstVecIdx));
            auto value = sumVector->GetValue(index);
            SumOp<ResultType, ResultType, int64_t, StateCountHandler>(&state->value,
                                                                      state->count, value, count);
        }
    }
}

// olk interface
template <DataTypeId IN_ID, DataTypeId OUT_ID>
void AverageAggregator<IN_ID, OUT_ID>::ProcessAlignAggSchema(VectorBatch *result, BaseVector *originVector,
    const std::shared_ptr<NullsHelper> nullMap, const bool aggFilter)
{
    if constexpr (OUT_ID == OMNI_VARCHAR) {
        SumAggregator<IN_ID, OUT_ID>::ProcessAlignAggSchema(result, originVector, nullMap, aggFilter);
    } else if constexpr (OUT_ID == OMNI_CONTAINER) {
        if (originVector->GetEncoding() == OMNI_DICTIONARY) {
            ProcessAlignAggSchemaInternal<Vector<DictionaryContainer<InType>>>(result, originVector, nullMap);
        } else {
            ProcessAlignAggSchemaInternal<Vector<InType>>(result, originVector, nullMap);
        }
    } else {
        throw OmniException("Unreachable code", "Reached unreachable code in average aggregator extract partial");
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
template <typename T>
void AverageAggregator<IN_ID, OUT_ID>::ProcessAlignAggSchemaInternal(VectorBatch *result, BaseVector *originVector,
    const std::shared_ptr<NullsHelper> nullMap)
{
    int rowCount = originVector->GetSize();
    auto containerVector = reinterpret_cast<Vector<double> *>(VectorHelper::CreateFlatVector(OMNI_DOUBLE, 2));
    auto sumVector = reinterpret_cast<Vector<double> *>(VectorHelper::CreateFlatVector(OMNI_DOUBLE, rowCount));
    auto countVector = reinterpret_cast<Vector<int64_t> *>(VectorHelper::CreateFlatVector(OMNI_LONG, rowCount));

    auto vector = reinterpret_cast<T *>(originVector);
    if (nullMap != nullptr) {
        for (int index = 0; index < rowCount; ++index) {
            if ((*nullMap)[index]) {
                sumVector->SetValue(index, 0);
                countVector->SetValue(index, 0);
            } else {
                InType val = vector->GetValue(index);
                bool overflow = false;
                OutType out = this->template CastWithOverflow<InType, OutType>(static_cast<InType>(val), overflow);
                sumVector->SetValue(index, out);
                countVector->SetValue(index, 1);
            }
        }
    } else {
        for (int index = 0; index < rowCount; ++index) {
            InType val = vector->GetValue(index);
            bool overflow = false;
            OutType out = this->template CastWithOverflow<InType, OutType>(static_cast<InType>(val), overflow);
            sumVector->SetValue(index, out);
            countVector->SetValue(index, 1);
        }
    }

    containerVector->SetValue(0, reinterpret_cast<uintptr_t>(sumVector));
    containerVector->SetValue(1, reinterpret_cast<uintptr_t>(countVector));
    result->Append(containerVector);
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
AverageAggregator<IN_ID, OUT_ID>::AverageAggregator(const DataTypes &inputTypes, const DataTypes &outputTypes,
    std::vector<int32_t> &channels, const bool inputRaw, const bool outputPartial, const bool isOverflowAsNull)
    : SumAggregator<IN_ID, OUT_ID>(OMNI_AGGREGATION_TYPE_AVG, inputTypes, outputTypes, channels, inputRaw,
    outputPartial, isOverflowAsNull)
{
    // varchar only in partial stage
    if constexpr (OUT_ID == OMNI_VARCHAR || OUT_ID == OMNI_CONTAINER) {
        extractValuesFuncPointer = &AverageAggregator<IN_ID, OUT_ID>::ExtractValuesFunction<true, false>;
    } else {
        if (ConfigUtil::GetSupportDecimalPrecisionImprovementRule() ==
            SupportDecimalPrecisionImprovementRule::IS_SUPPORT) {
            extractValuesFuncPointer = &AverageAggregator<IN_ID, OUT_ID>::ExtractValuesFunction<false, true>;
        } else {
            extractValuesFuncPointer = &AverageAggregator<IN_ID, OUT_ID>::ExtractValuesFunction<false, false>;
        }
    }
}

// Explicit template instantiation
// Defining templated aggregators in header file consume a lot of memory during compilation
// since, compiler needs to generate each individual template instance wherever aggregator header is include
// to reduce time and memory usage during compilation moved templated aggregator implementation into .cpp files
// and used explicit template instantiation to generate template instances
template class AverageAggregator<OMNI_SHORT, OMNI_DOUBLE>;

template class AverageAggregator<OMNI_SHORT, OMNI_DECIMAL128>;

template class AverageAggregator<OMNI_SHORT, OMNI_DECIMAL64>;

template class AverageAggregator<OMNI_SHORT, OMNI_CONTAINER>;

template class AverageAggregator<OMNI_INT, OMNI_DOUBLE>;

template class AverageAggregator<OMNI_INT, OMNI_DECIMAL128>;

template class AverageAggregator<OMNI_INT, OMNI_DECIMAL64>;

template class AverageAggregator<OMNI_INT, OMNI_CONTAINER>;

template class AverageAggregator<OMNI_LONG, OMNI_DOUBLE>;

template class AverageAggregator<OMNI_LONG, OMNI_DECIMAL128>;

template class AverageAggregator<OMNI_LONG, OMNI_DECIMAL64>;

template class AverageAggregator<OMNI_LONG, OMNI_CONTAINER>;

template class AverageAggregator<OMNI_DOUBLE, OMNI_DOUBLE>;

template class AverageAggregator<OMNI_DOUBLE, OMNI_DECIMAL128>;

template class AverageAggregator<OMNI_DOUBLE, OMNI_DECIMAL64>;

template class AverageAggregator<OMNI_DOUBLE, OMNI_CONTAINER>;

template class AverageAggregator<OMNI_DECIMAL128, OMNI_DOUBLE>;

template class AverageAggregator<OMNI_DECIMAL128, OMNI_DECIMAL128>;

template class AverageAggregator<OMNI_DECIMAL128, OMNI_DECIMAL64>;

template class AverageAggregator<OMNI_DECIMAL128, OMNI_VARCHAR>;

template class AverageAggregator<OMNI_DECIMAL64, OMNI_DOUBLE>;

template class AverageAggregator<OMNI_DECIMAL64, OMNI_DECIMAL128>;

template class AverageAggregator<OMNI_DECIMAL64, OMNI_DECIMAL64>;

template class AverageAggregator<OMNI_DECIMAL64, OMNI_VARCHAR>;

template class AverageAggregator<OMNI_VARCHAR, OMNI_DOUBLE>;

template class AverageAggregator<OMNI_VARCHAR, OMNI_DECIMAL128>;

template class AverageAggregator<OMNI_VARCHAR, OMNI_DECIMAL64>;

template class AverageAggregator<OMNI_VARCHAR, OMNI_VARCHAR>;

template class AverageAggregator<OMNI_CONTAINER, OMNI_DOUBLE>;

template class AverageAggregator<OMNI_CONTAINER, OMNI_DECIMAL128>;

template class AverageAggregator<OMNI_CONTAINER, OMNI_DECIMAL64>;

template class AverageAggregator<OMNI_CONTAINER, OMNI_CONTAINER>;
}
}
