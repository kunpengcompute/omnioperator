/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2024. All rights reserved.
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
 * Description: Sum aggregator
 */

#include "sum_aggregator.h"

namespace omniruntime {
namespace op {
template <DataTypeId IN_ID, DataTypeId OUT_ID>
void SumAggregator<IN_ID, OUT_ID>::ExtractValuesInternal(const AggregateState *state, OutVector *vector,
                                                         int32_t rowIndex)
{
    auto *sumState = SumState::ConstCastState(state);
    bool isOverflow = sumState->count < 0;
    bool isEmpty = sumState->count == 0;
    if (isOverflow && !this->IsOverflowAsNull()) {
        throw OmniException("OPERATOR_RUNTIME_ERROR", "sum_aggregator overflow.");
    }

    OutType result{};
    if constexpr (OUT_ID == OMNI_VARCHAR) {
        if (isOverflow || isEmpty) {
            std::string_view decimal2Str(reinterpret_cast<char *>(&result), sizeof(OutType));
            vector->SetValue(rowIndex, decimal2Str);
        } else {
            result.sum = CastWithOverflow<Decimal128, Decimal128>(
                *reinterpret_cast<Decimal128 *>((int64_t)(&sumState->value)), isOverflow);
            if (isOverflow) {
                if (!this->IsOverflowAsNull()) {
                    throw OmniException("OPERATOR_RUNTIME_ERROR", "sum_aggregator overflow.");
                } else {
                    OutType overflowValue{};
                    std::string_view decimal2Str(reinterpret_cast<char *>(&overflowValue), sizeof(OutType));
                    vector->SetValue(rowIndex, decimal2Str);
                }
            } else {
                result.count = sumState->count;
                std::string_view decimal2Str(reinterpret_cast<char *>(&result), sizeof(OutType));
                vector->SetValue(rowIndex, decimal2Str);
            }
        }
    } else {
        if (isOverflow) {
            this->SetNullOrThrowException(vector, rowIndex, "sum_aggregator overflow.");
        } else if (isEmpty) {
            vector->SetNull(rowIndex);
        } else {
            result = this->template CastWithOverflow<ResultType, OutType>(sumState->value, isOverflow);
            if (isOverflow) {
                this->SetNullOrThrowException(vector, rowIndex, "sum_aggregator overflow.");
            } else {
                vector->SetValue(rowIndex, result);
            }
        }
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void SumAggregator<IN_ID, OUT_ID>::ExtractValues(const AggregateState *state, std::vector<BaseVector *> &vectors,
                                                 int32_t rowIndex)
{
    ExtractValuesInternal(state + aggStateOffset, static_cast<OutVector *>(vectors[0]), rowIndex);
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void SumAggregator<IN_ID, OUT_ID>::ExtractValuesBatch(std::vector<AggregateState *> &groupStates,
                                                      std::vector<BaseVector *> &vectors, int32_t rowOffset,
                                                      int32_t rowCount)
{
    auto v = static_cast<OutVector *>(vectors[0]);
    for (int32_t rowIndex = 0; rowIndex < rowCount; rowIndex++) {
        ExtractValuesInternal(groupStates[rowIndex] + aggStateOffset, v, rowIndex);
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID> std::vector<DataTypePtr> SumAggregator<IN_ID, OUT_ID>::GetSpillType()
{
    std::vector<DataTypePtr> spillTypes;
    if constexpr (IN_ID == OMNI_SHORT || IN_ID == OMNI_INT || IN_ID == OMNI_LONG) {
        spillTypes.emplace_back(std::make_shared<DataType>(OMNI_LONG));
    } else if constexpr (IN_ID == OMNI_DOUBLE || IN_ID == OMNI_CONTAINER) {
        spillTypes.emplace_back(std::make_shared<DataType>(OMNI_DOUBLE));
    } else {
        spillTypes.emplace_back(std::make_shared<DataType>(OMNI_DECIMAL128));
    }
    return spillTypes;
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void SumAggregator<IN_ID, OUT_ID>::ExtractValuesForSpill(std::vector<AggregateState *> &groupStates,
                                                         std::vector<BaseVector *> &vectors)
{
    auto spillValueVec = static_cast<Vector<ResultType> *>(vectors[0]);

    auto rowCount = static_cast<int32_t>(groupStates.size());
    for (int32_t rowIndex = 0; rowIndex < rowCount; rowIndex++) {
        auto *state = SumState::CastState(groupStates[rowIndex] + aggStateOffset);
        auto count = state->count;
        bool isOverflow = count < 0;
        bool isEmpty = count == 0;

        // use null and value to distinguish empty group, overflow and other normal case
        if (isOverflow) {
            // set null and overflow value to mark overflow case
            this->SetNullOrThrowException(spillValueVec, rowIndex, "sum_aggregator overflow.");
            spillValueVec->SetValue(rowIndex, SPILL_OVERFLOW_VALUE);
        } else if (isEmpty) {
            // set null for empty group(all rows are NULL) when spill to ensure skip empty group when unspill
            spillValueVec->SetNull(rowIndex);
            spillValueVec->SetValue(rowIndex, SPILL_EMPTY_VALUE);
        } else {
            ResultType result = static_cast<ResultType>(state->value);
            spillValueVec->SetValue(rowIndex, result);
        }
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID> void SumAggregator<IN_ID, OUT_ID>::InitState(AggregateState *state)
{
    auto *sumState = SumState::CastState(state + aggStateOffset);
    sumState->value = ResultType{};
    sumState->count = 0;
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void SumAggregator<IN_ID, OUT_ID>::InitStates(std::vector<AggregateState *> &groupStates)
{
    for (auto groupState : groupStates) {
        InitState(groupState);
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void SumAggregator<IN_ID, OUT_ID>::ProcessSingleInternal(AggregateState *state, BaseVector *vector,
                                                         const int32_t rowOffset, const int32_t rowCount,
                                                         const std::shared_ptr<NullsHelper> nullMap)
{
    auto *sumState = SumState::CastState(state);
    if (vector->GetEncoding() != vec::OMNI_DICTIONARY) {
        auto *ptr = reinterpret_cast<InType *>(GetValuesFromVector<IN_ID>(vector));
        ptr += rowOffset;
        if (nullMap == nullptr) {
            Add<InType, ResultType, int64_t, SumOp<InType, ResultType, int64_t, StateCountHandler>>(
                reinterpret_cast<ResultType *>(&sumState->value), sumState->count, ptr, rowCount);
        } else {
            if constexpr (std::is_floating_point_v<InType>) {
                SumConditionalFloat<InType, ResultType, false>(reinterpret_cast<ResultType *>(&sumState->value),
                                                               sumState->count, ptr, rowCount, *nullMap);
            } else {
                AddConditional<InType, ResultType, int64_t,
                               SumConditionalOp<InType, ResultType, int64_t, StateCountHandler, false>>(
                    reinterpret_cast<ResultType *>(&sumState->value), sumState->count, ptr, rowCount, *nullMap);
            }
        }
    } else {
        auto *ptr = reinterpret_cast<InType *>(GetValuesFromDict<IN_ID>(vector));
        auto *indexMap = GetIdsFromDict<IN_ID>(vector) + rowOffset;
        if (nullMap == nullptr) {
            AddDict<InType, ResultType, int64_t, SumOp<InType, ResultType, int64_t, StateCountHandler>>(
                reinterpret_cast<ResultType *>(&sumState->value), sumState->count, ptr, rowCount, indexMap);
        } else {
            AddDictConditional<InType, ResultType, int64_t,
                               SumConditionalOp<InType, ResultType, int64_t, StateCountHandler, false>>(
                reinterpret_cast<ResultType *>(&sumState->value), sumState->count, ptr, rowCount, *nullMap, indexMap);
        }
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void SumAggregator<IN_ID, OUT_ID>::ProcessGroupInternal(std::vector<AggregateState *> &rowStates, BaseVector *vector,
                                                        const int32_t rowOffset,
                                                        const std::shared_ptr<NullsHelper> nullMap)
{
    if (vector->GetEncoding() != vec::OMNI_DICTIONARY) {
        auto *ptr = reinterpret_cast<InType *>(GetValuesFromVector<IN_ID>(vector));
        ptr += rowOffset;
        if (nullMap == nullptr) {
            AddUseRowIndex<InType, SumState::template UpdateState<InType, ResultType>>(rowStates, aggStateOffset, ptr);
        } else {
            // Reza: can we use customize float operation similar to sumConditionalFloat
            AddConditionalUseRowIndex<InType, SumState::template UpdateStateWithCondition<InType, ResultType, false>>(
                rowStates, aggStateOffset, ptr, *nullMap);
        }
    } else {
        auto *ptr = reinterpret_cast<InType *>(GetValuesFromDict<IN_ID>(vector));
        auto *indexMap = GetIdsFromDict<IN_ID>(vector) + rowOffset;
        if (nullMap == nullptr) {
            AddDictUseRowIndex<InType, SumState::template UpdateState<InType, ResultType>>(rowStates, aggStateOffset,
                                                                                           ptr, indexMap);
        } else {
            AddDictConditionalUseRowIndex<InType, ResultType,
                                          SumState::template UpdateStateWithCondition<InType, ResultType, false>>(
                rowStates, aggStateOffset, ptr, *nullMap, indexMap);
        }
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void SumAggregator<IN_ID, OUT_ID>::ProcessGroupUnspill(std::vector<UnspillRowInfo> &unspillRows, int32_t rowCount,
                                                       int32_t &vectorIndex)
{
    auto firstVecIdx = vectorIndex++;
    for (int32_t rowIdx = 0; rowIdx < rowCount; rowIdx++) {
        auto &row = unspillRows[rowIdx];
        auto batch = row.batch;
        auto index = row.rowIdx;
        auto *state = SumState::CastState(row.state + aggStateOffset);
        auto sumVector = static_cast<Vector<ResultType> *>(batch->Get(firstVecIdx));
        auto value = sumVector->GetValue(index);
        if (!sumVector->IsNull(index)) {
            SumOp<ResultType, ResultType, int64_t, StateCountHandler>(reinterpret_cast<ResultType *>(&state->value),
                                                                      state->count, value, 1LL);
        } else {
            // empty group or overflow case
            // if it is overflow we set -1
            // if it is empty group we skipped
            if (value == SPILL_OVERFLOW_VALUE) {
                state->count = -1;
            }
        }
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void SumAggregator<IN_ID, OUT_ID>::ProcessAlignAggSchema(VectorBatch *result, BaseVector *originVector,
                                                         const std::shared_ptr<NullsHelper> nullMap,
                                                         const bool aggFilter)
{
    int rowCount = originVector->GetSize();
    // opt branch
    if constexpr (std::is_same_v<InType, OutType>) {
        if (!aggFilter) {
            auto sumVector = VectorHelper::SliceVector(originVector, 0, rowCount);
            result->Append(sumVector);
            return;
        }
    }

    if constexpr (IN_ID == OMNI_VARCHAR) {
        // olk final agg pause, so throw an exception
        throw OmniException("OPERATOR_RUNTIME_ERROR", "this interface only be called in partial agg pause.");
    } else if constexpr (OUT_ID == OMNI_VARCHAR) {
        // hive or olk branch, decimal64 or decimal128 is stored in DecimalPartialResult object,
        // and then be converted to std::string_view type.
        if (originVector->GetEncoding() == OMNI_DICTIONARY) {
            if constexpr (std::is_same_v<InType, int64_t>) {
                ProcessAlignAggSchemaInternalForDecimal<Vector<DictionaryContainer<int64_t>>>(result, originVector,
                                                                                              nullMap);
            } else if constexpr (std::is_same_v<InType, Decimal128>) {
                ProcessAlignAggSchemaInternalForDecimal<Vector<DictionaryContainer<Decimal128>>>(result, originVector,
                                                                                                 nullMap);
            }
        } else {
            if constexpr (std::is_same_v<InType, int64_t>) {
                ProcessAlignAggSchemaInternalForDecimal<Vector<int64_t>>(result, originVector, nullMap);
            } else if constexpr (std::is_same_v<InType, Decimal128>) {
                ProcessAlignAggSchemaInternalForDecimal<Vector<Decimal128>>(result, originVector, nullMap);
            }
        }
    } else {
        // hive or olk branch
        if (originVector->GetEncoding() == OMNI_DICTIONARY) {
            // The varchar type is converted to the double type for hive engine in advance.
            ProcessAlignAggSchemaInternal<Vector<DictionaryContainer<InType>>>(result, originVector, nullMap);
        } else {
            // The varchar type is converted to the double type for hive engine in advance.
            ProcessAlignAggSchemaInternal<Vector<InType>>(result, originVector, nullMap);
        }
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
template <typename T>
void SumAggregator<IN_ID, OUT_ID>::ProcessAlignAggSchemaInternalForDecimal(VectorBatch *result,
                                                                           BaseVector *originVector,
                                                                           const std::shared_ptr<NullsHelper> nullMap)
{
    int rowCount = originVector->GetSize();
    auto sumVector = reinterpret_cast<Vector<LargeStringContainer<std::string_view>> *>(
        VectorHelper::CreateFlatVector(OMNI_VARCHAR, rowCount));
    if (nullMap != nullptr) {
        for (int index = 0; index < rowCount; ++index) {
            if ((*nullMap)[index]) {
                sumVector->SetNull(index);
            } else {
                DecimalPartialResult out;
                auto vector = reinterpret_cast<T *>(originVector);
                out.sum = Decimal128(vector->GetValue(index));
                out.count = 1;
                std::string_view decimal2Str(reinterpret_cast<char *>(&out), sizeof(OutType));
                sumVector->SetValue(index, decimal2Str);
            }
        }
    } else {
        for (int index = 0; index < rowCount; ++index) {
            DecimalPartialResult out;
            auto vector = reinterpret_cast<T *>(originVector);
            out.sum = Decimal128(vector->GetValue(index));
            out.count = 1;
            std::string_view decimal2Str(reinterpret_cast<char *>(&out), sizeof(OutType));
            sumVector->SetValue(index, decimal2Str);
        }
    }
    result->Append(sumVector);
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
template <typename T>
void SumAggregator<IN_ID, OUT_ID>::ProcessAlignAggSchemaInternal(VectorBatch *result, BaseVector *originVector,
                                                                 const std::shared_ptr<NullsHelper> nullMap)
{
    if constexpr (IN_ID != OMNI_VARCHAR && OUT_ID != OMNI_VARCHAR) {
        int rowCount = originVector->GetSize();
        auto sumVector = reinterpret_cast<OutVector *>(VectorHelper::CreateFlatVector(OUT_ID, rowCount));
        auto vector = reinterpret_cast<T *>(originVector);
        if (nullMap != nullptr) {
            for (int index = 0; index < rowCount; ++index) {
                if ((*nullMap)[index]) {
                    sumVector->SetNull(index);
                } else {
                    InType val = vector->GetValue(index);
                    bool overflow = false;
                    OutType out = this->template CastWithOverflow<InType, OutType>(static_cast<InType>(val), overflow);
                    sumVector->SetValue(index, out);
                }
            }
        } else {
            for (int index = 0; index < rowCount; ++index) {
                InType val = vector->GetValue(index);
                bool overflow = false;
                OutType out = this->template CastWithOverflow<InType, OutType>(static_cast<InType>(val), overflow);
                sumVector->SetValue(index, out);
            }
        }
        result->Append(sumVector);
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
SumAggregator<IN_ID, OUT_ID>::SumAggregator(const DataTypes &inputTypes, const DataTypes &outputTypes,
                                            std::vector<int32_t> &channels, const bool inputRaw,
                                            const bool outputPartial, const bool isOverflowAsNull)
    : TypedAggregator(OMNI_AGGREGATION_TYPE_SUM, inputTypes, outputTypes, channels, inputRaw, outputPartial,
                      isOverflowAsNull)
{
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
SumAggregator<IN_ID, OUT_ID>::SumAggregator(FunctionType aggregateType, const DataTypes &inputTypes,
                                            const DataTypes &outputTypes, std::vector<int32_t> &channels,
                                            const bool inputRaw, const bool outputPartial, const bool isOverflowAsNull)
    : TypedAggregator(aggregateType, inputTypes, outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull)
{
}

// Explicit template instantiation
// Defining templated aggregators in header file consume a lot of memory during compilation
// since, compiler needs to generate each individual template instance wherever aggregator header is include
// to reduce time and memory usage during compilation moved templated aggregator implementation into .cpp files
// and used explicit template instantiation to generate template instances
template class SumAggregator<OMNI_SHORT, OMNI_SHORT>;

template class SumAggregator<OMNI_SHORT, OMNI_INT>;

template class SumAggregator<OMNI_SHORT, OMNI_LONG>;

template class SumAggregator<OMNI_SHORT, OMNI_DOUBLE>;

template class SumAggregator<OMNI_SHORT, OMNI_DECIMAL128>;

template class SumAggregator<OMNI_SHORT, OMNI_DECIMAL64>;

template class SumAggregator<OMNI_SHORT, OMNI_VARCHAR>;

template class SumAggregator<OMNI_SHORT, OMNI_CONTAINER>;

template class SumAggregator<OMNI_INT, OMNI_SHORT>;

template class SumAggregator<OMNI_INT, OMNI_INT>;

template class SumAggregator<OMNI_INT, OMNI_LONG>;

template class SumAggregator<OMNI_INT, OMNI_DOUBLE>;

template class SumAggregator<OMNI_INT, OMNI_DECIMAL128>;

template class SumAggregator<OMNI_INT, OMNI_DECIMAL64>;

template class SumAggregator<OMNI_INT, OMNI_VARCHAR>;

template class SumAggregator<OMNI_INT, OMNI_CONTAINER>;

template class SumAggregator<OMNI_LONG, OMNI_SHORT>;

template class SumAggregator<OMNI_LONG, OMNI_INT>;

template class SumAggregator<OMNI_LONG, OMNI_LONG>;

template class SumAggregator<OMNI_LONG, OMNI_DOUBLE>;

template class SumAggregator<OMNI_LONG, OMNI_DECIMAL128>;

template class SumAggregator<OMNI_LONG, OMNI_DECIMAL64>;

template class SumAggregator<OMNI_LONG, OMNI_VARCHAR>;

template class SumAggregator<OMNI_LONG, OMNI_CONTAINER>;

template class SumAggregator<OMNI_DOUBLE, OMNI_SHORT>;

template class SumAggregator<OMNI_DOUBLE, OMNI_INT>;

template class SumAggregator<OMNI_DOUBLE, OMNI_LONG>;

template class SumAggregator<OMNI_DOUBLE, OMNI_DOUBLE>;

template class SumAggregator<OMNI_DOUBLE, OMNI_DECIMAL128>;

template class SumAggregator<OMNI_DOUBLE, OMNI_DECIMAL64>;

template class SumAggregator<OMNI_DOUBLE, OMNI_VARCHAR>;

template class SumAggregator<OMNI_DOUBLE, OMNI_CONTAINER>;

template class SumAggregator<OMNI_DECIMAL128, OMNI_SHORT>;

template class SumAggregator<OMNI_DECIMAL128, OMNI_INT>;

template class SumAggregator<OMNI_DECIMAL128, OMNI_LONG>;

template class SumAggregator<OMNI_DECIMAL128, OMNI_DOUBLE>;

template class SumAggregator<OMNI_DECIMAL128, OMNI_DECIMAL128>;

template class SumAggregator<OMNI_DECIMAL128, OMNI_DECIMAL64>;

template class SumAggregator<OMNI_DECIMAL128, OMNI_VARCHAR>;

template class SumAggregator<OMNI_DECIMAL128, OMNI_CONTAINER>;

template class SumAggregator<OMNI_DECIMAL64, OMNI_SHORT>;

template class SumAggregator<OMNI_DECIMAL64, OMNI_INT>;

template class SumAggregator<OMNI_DECIMAL64, OMNI_LONG>;

template class SumAggregator<OMNI_DECIMAL64, OMNI_DOUBLE>;

template class SumAggregator<OMNI_DECIMAL64, OMNI_DECIMAL128>;

template class SumAggregator<OMNI_DECIMAL64, OMNI_DECIMAL64>;

template class SumAggregator<OMNI_DECIMAL64, OMNI_VARCHAR>;

template class SumAggregator<OMNI_DECIMAL64, OMNI_CONTAINER>;

template class SumAggregator<OMNI_VARCHAR, OMNI_SHORT>;

template class SumAggregator<OMNI_VARCHAR, OMNI_INT>;

template class SumAggregator<OMNI_VARCHAR, OMNI_LONG>;

template class SumAggregator<OMNI_VARCHAR, OMNI_DOUBLE>;

template class SumAggregator<OMNI_VARCHAR, OMNI_DECIMAL128>;

template class SumAggregator<OMNI_VARCHAR, OMNI_DECIMAL64>;

template class SumAggregator<OMNI_VARCHAR, OMNI_VARCHAR>;

template class SumAggregator<OMNI_VARCHAR, OMNI_CONTAINER>;

template class SumAggregator<OMNI_CONTAINER, OMNI_SHORT>;

template class SumAggregator<OMNI_CONTAINER, OMNI_INT>;

template class SumAggregator<OMNI_CONTAINER, OMNI_LONG>;

template class SumAggregator<OMNI_CONTAINER, OMNI_DOUBLE>;

template class SumAggregator<OMNI_CONTAINER, OMNI_DECIMAL128>;

template class SumAggregator<OMNI_CONTAINER, OMNI_DECIMAL64>;

template class SumAggregator<OMNI_CONTAINER, OMNI_VARCHAR>;

template class SumAggregator<OMNI_CONTAINER, OMNI_CONTAINER>;
} // namespace op
} // namespace omniruntime
