/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
 * KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 *
 * Description: Max aggregate
 */

#include "max_aggregator.h"
#include "simd/func/reduce.h"

namespace omniruntime {
namespace op {
template <DataTypeId IN_ID, DataTypeId OUT_ID>
void MaxAggregator<IN_ID, OUT_ID>::ExtractValues(const AggregateState *state, std::vector<BaseVector *> &vectors,
                                                 int32_t rowIndex)
{
    auto v = static_cast<OutVector *>(vectors[0]);
    auto maxState = MaxState::ConstCastState(state + aggStateOffset);
    if (maxState->IsEmpty()) {
        v->SetNull(rowIndex);
        return;
    }

    bool overflow = (maxState->IsOverFlowed());

    // the first value of state is ResultType, so we just cast state pointer to int64
    auto result = CastWithOverflow<ResultType, OutType>(maxState->value, overflow);

    v->SetValue(rowIndex, result);
    if (overflow) {
        this->SetNullOrThrowException(v, rowIndex, "max_aggregator overflow.");
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void MaxAggregator<IN_ID, OUT_ID>::ExtractValuesBatch(std::vector<AggregateState *> &groupStates,
                                                      std::vector<BaseVector *> &vectors, int32_t rowOffset,
                                                      int32_t rowCount)
{
    auto v = static_cast<OutVector *>(vectors[0]);
    for (int32_t rowIndex = 0; rowIndex < rowCount; rowIndex++) {
        auto state = MaxState::CastState(groupStates[rowIndex] + aggStateOffset);
        if (state->IsEmpty()) {
            v->SetNull(rowIndex);
        } else if (state->IsOverFlowed()) {
            this->SetNullOrThrowException(v, rowIndex, "max_aggregator overflow.");
        } else {
            OutType result;
            bool isOverflow = false;
            result = CastWithOverflow<ResultType, OutType>(state->value, isOverflow);
            if (isOverflow) {
                this->SetNullOrThrowException(v, rowIndex, "max_aggregator overflow.");
            } else {
                v->SetValue(rowIndex, result);
            }
        }
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID> std::vector<DataTypePtr> MaxAggregator<IN_ID, OUT_ID>::GetSpillType()
{
    std::vector<DataTypePtr> spillTypes;
    if constexpr (IN_ID == OMNI_SHORT) {
        spillTypes.emplace_back(std::make_shared<DataType>(OMNI_INT));
    } else {
        spillTypes.emplace_back(outputTypes.GetType(0));
    }
    return spillTypes;
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void MaxAggregator<IN_ID, OUT_ID>::ExtractValuesForSpill(std::vector<AggregateState *> &groupStates,
                                                         std::vector<BaseVector *> &vectors)
{
    auto spillValueVec = static_cast<Vector<ResultType> *>(vectors[0]);
    auto rowCount = static_cast<int32_t>(groupStates.size());
    for (int32_t rowIndex = 0; rowIndex < rowCount; rowIndex++) {
        auto *state = MaxState::CastState(groupStates[rowIndex] + aggStateOffset);
        if (state->valueState == AggValueState::EMPTY_VALUE) {
            spillValueVec->SetNull(rowIndex);
        } else {
            spillValueVec->SetValue(rowIndex, static_cast<ResultType>(state->value));
        }
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID> void MaxAggregator<IN_ID, OUT_ID>::InitState(AggregateState *state)
{
    auto *maxState = MaxState::CastState(state + aggStateOffset);
    maxState->value = GetMin<ResultType>();
    maxState->valueState = AggValueState::EMPTY_VALUE;
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void MaxAggregator<IN_ID, OUT_ID>::InitStates(std::vector<AggregateState *> &groupStates)
{
    for (auto groupState : groupStates) {
        InitState(groupState);
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void MaxAggregator<IN_ID, OUT_ID>::ProcessSingleInternal(AggregateState *state, BaseVector *vector,
                                                         const int32_t rowOffset, const int32_t rowCount,
                                                         const std::shared_ptr<NullsHelper> nullMap)
{
    auto maxState = MaxState::CastState(state);
    if (vector->GetEncoding() != vec::OMNI_DICTIONARY) {
        auto *ptr = reinterpret_cast<InType *>(GetValuesFromVector<IN_ID>(vector));
        ptr += rowOffset;
        if (nullMap == nullptr) {
            if constexpr (CheckTypesContainsDecimal128<InType, ResultType>::value) {
                Add<InType, ResultType, AggValueState, MaxOp<InType, ResultType>>(&maxState->value,
                                                                                  maxState->valueState, ptr, rowCount);
            } else {
                simd::ReduceExternal<InType, ResultType, AggValueState, StateValueHandler, simd::ReduceFunc::Max>(
                    &maxState->value, maxState->valueState, ptr, rowCount);
            }
        } else {
            if constexpr (CheckTypesContainsDecimal128<InType, ResultType>::value) {
                AddConditional<InType, ResultType, AggValueState, MaxConditionalOp<InType, ResultType, false>>(
                    &(maxState->value), maxState->valueState, ptr, rowCount, *nullMap);
            } else {
                auto conditionArray = nullMap->convertToArray(rowCount);
                simd::ReduceWithNullsExternal<InType, ResultType, AggValueState, StateValueHandler,
                                              simd::ReduceFunc::Max>(&maxState->value, maxState->valueState, ptr,
                                                                     rowCount, conditionArray.data());
            }
        }
    } else {
        auto *ptr = reinterpret_cast<InType *>(GetValuesFromDict<IN_ID>(vector));
        auto *indexMap = GetIdsFromDict<IN_ID>(vector) + rowOffset;
        if (nullMap == nullptr) {
            if constexpr (CheckTypesContainsDecimal128<InType, ResultType>::value) {
                AddDict<InType, ResultType, AggValueState, MaxOp<InType, ResultType>>(
                    &maxState->value, maxState->valueState, ptr, rowCount, indexMap);
            } else {
                simd::ReduceWithDicExternal<InType, ResultType, AggValueState, StateValueHandler,
                                            simd::ReduceFunc::Max>(&maxState->value, maxState->valueState, ptr,
                                                                   rowCount, indexMap);
            }
        } else {
            if constexpr (CheckTypesContainsDecimal128<InType, ResultType>::value) {
                AddDictConditional<InType, ResultType, AggValueState, MaxConditionalOp<InType, ResultType, false>>(
                    &maxState->value, maxState->valueState, ptr, rowCount, *nullMap, indexMap);
            } else {
                auto conditionArray = nullMap->convertToArray(rowCount);
                simd::ReduceWithDicAndNullsExternal<InType, ResultType, AggValueState, StateValueHandler,
                                                    simd::ReduceFunc::Max>(&maxState->value, maxState->valueState, ptr,
                                                                           rowCount, conditionArray.data(), indexMap);
            }
        }
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void MaxAggregator<IN_ID, OUT_ID>::ProcessGroupInternal(std::vector<AggregateState *> &rowStates, BaseVector *vector,
                                                        const int32_t rowOffset,
                                                        const std::shared_ptr<NullsHelper> nullMap)
{
    if (vector->GetEncoding() != vec::OMNI_DICTIONARY) {
        auto *ptr = reinterpret_cast<InType *>(GetValuesFromVector<IN_ID>(vector));
        ptr += rowOffset;
        if (nullMap == nullptr) {
            AddUseRowIndex<InType, MaxState::template UpdateState<InType, ResultType>>(rowStates, aggStateOffset, ptr);
        } else {
            AddConditionalUseRowIndex<InType, MaxState::template UpdateStateWithCondition<InType, ResultType, false>>(
                rowStates, aggStateOffset, ptr, *nullMap);
        }
    } else {
        auto *ptr = reinterpret_cast<InType *>(GetValuesFromDict<IN_ID>(vector));
        auto *indexMap = GetIdsFromDict<IN_ID>(vector) + rowOffset;
        if (nullMap == nullptr) {
            AddDictUseRowIndex<InType, MaxState::template UpdateState<InType, ResultType>>(rowStates, aggStateOffset,
                                                                                           ptr, indexMap);
        } else {
            AddDictConditionalUseRowIndex<InType, ResultType,
                                          MaxState::template UpdateStateWithCondition<InType, ResultType, false>>(
                rowStates, aggStateOffset, ptr, *nullMap, indexMap);
        }
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void MaxAggregator<IN_ID, OUT_ID>::ProcessGroupUnspill(std::vector<UnspillRowInfo> &unspillRows, int32_t rowCount,
                                                       int32_t &vectorIndex)
{
    auto firstVecIdx = vectorIndex++;
    for (int32_t rowIdx = 0; rowIdx < rowCount; rowIdx++) {
        auto &row = unspillRows[rowIdx];
        auto batch = row.batch;
        auto index = row.rowIdx;
        auto vectorPtr = static_cast<Vector<ResultType> *>(batch->Get(firstVecIdx));
        if (!vectorPtr->IsNull(index)) {
            auto maxState = MaxState::CastState(row.state + aggStateOffset);
            auto value = vectorPtr->GetValue(index);
            MaxOp<ResultType, ResultType>((&maxState->value), maxState->valueState, value, 1LL);
        }
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void MaxAggregator<IN_ID, OUT_ID>::ProcessAlignAggSchema(VectorBatch *result, BaseVector *originVector,
                                                         const std::shared_ptr<NullsHelper> nullMap,
                                                         const bool aggFilter)
{
    int rowCount = originVector->GetSize();
    if constexpr (std::is_same_v<InType, OutType>) {
        if (!aggFilter) {
            auto maxVector = VectorHelper::SliceVector(originVector, 0, rowCount);
            result->Append(maxVector);
            return;
        }
    }

    if (originVector->GetEncoding() == OMNI_DICTIONARY) {
        ProcessAlignAggSchemaInternal<Vector<DictionaryContainer<InType>>>(result, originVector, nullMap);
    } else {
        ProcessAlignAggSchemaInternal<Vector<InType>>(result, originVector, nullMap);
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
template <typename T>
void MaxAggregator<IN_ID, OUT_ID>::ProcessAlignAggSchemaInternal(VectorBatch *result, BaseVector *originVector,
                                                                 const std::shared_ptr<NullsHelper> nullMap)
{
    int rowCount = originVector->GetSize();
    auto maxVector = reinterpret_cast<OutVector *>(VectorHelper::CreateFlatVector(OUT_ID, rowCount));
    auto vector = reinterpret_cast<T *>(originVector);
    if (nullMap != nullptr) {
        for (int index = 0; index < rowCount; ++index) {
            if ((*nullMap)[index]) {
                maxVector->SetNull(index);
            } else {
                InType val = vector->GetValue(index);
                bool overflow = false;
                OutType out = this->template CastWithOverflow<InType, OutType>(static_cast<InType>(val), overflow);
                maxVector->SetValue(index, out);
            }
        }
    } else {
        for (int index = 0; index < rowCount; ++index) {
            InType val = vector->GetValue(index);
            bool overflow = false;
            OutType out = this->template CastWithOverflow<InType, OutType>(static_cast<InType>(val), overflow);
            maxVector->SetValue(index, out);
        }
    }
    result->Append(maxVector);
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
MaxAggregator<IN_ID, OUT_ID>::MaxAggregator(const DataTypes &inputTypes, const DataTypes &outputTypes,
                                            std::vector<int32_t> &channels, const bool inputRaw,
                                            const bool outputPartial, const bool isOverflowAsNull)
    : TypedAggregator(OMNI_AGGREGATION_TYPE_MAX, inputTypes, outputTypes, channels, inputRaw, outputPartial,
                      isOverflowAsNull)
{
}

// Explicit template instantiation
// Defining templated aggregators in header file consume a lot of memory during compilation
// since, compiler needs to generate each individual template instance wherever aggregator header is include
// to reduce time and memory usage during compilation moved templated aggregator implementation into .cpp files
// and used explicit template instantiation to generate template instances
template class MaxAggregator<OMNI_BOOLEAN, OMNI_BOOLEAN>;

template class MaxAggregator<OMNI_BOOLEAN, OMNI_SHORT>;

template class MaxAggregator<OMNI_BOOLEAN, OMNI_INT>;

template class MaxAggregator<OMNI_BOOLEAN, OMNI_LONG>;

template class MaxAggregator<OMNI_BOOLEAN, OMNI_DOUBLE>;

template class MaxAggregator<OMNI_BOOLEAN, OMNI_DECIMAL128>;

template class MaxAggregator<OMNI_BOOLEAN, OMNI_DECIMAL64>;

template class MaxAggregator<OMNI_SHORT, OMNI_BOOLEAN>;

template class MaxAggregator<OMNI_SHORT, OMNI_SHORT>;

template class MaxAggregator<OMNI_SHORT, OMNI_INT>;

template class MaxAggregator<OMNI_SHORT, OMNI_LONG>;

template class MaxAggregator<OMNI_SHORT, OMNI_DOUBLE>;

template class MaxAggregator<OMNI_SHORT, OMNI_DECIMAL128>;

template class MaxAggregator<OMNI_SHORT, OMNI_DECIMAL64>;

template class MaxAggregator<OMNI_INT, OMNI_BOOLEAN>;

template class MaxAggregator<OMNI_INT, OMNI_SHORT>;

template class MaxAggregator<OMNI_INT, OMNI_INT>;

template class MaxAggregator<OMNI_INT, OMNI_LONG>;

template class MaxAggregator<OMNI_INT, OMNI_DOUBLE>;

template class MaxAggregator<OMNI_INT, OMNI_DECIMAL128>;

template class MaxAggregator<OMNI_INT, OMNI_DECIMAL64>;

template class MaxAggregator<OMNI_LONG, OMNI_BOOLEAN>;

template class MaxAggregator<OMNI_LONG, OMNI_SHORT>;

template class MaxAggregator<OMNI_LONG, OMNI_INT>;

template class MaxAggregator<OMNI_LONG, OMNI_LONG>;

template class MaxAggregator<OMNI_LONG, OMNI_DOUBLE>;

template class MaxAggregator<OMNI_LONG, OMNI_DECIMAL128>;

template class MaxAggregator<OMNI_LONG, OMNI_DECIMAL64>;

template class MaxAggregator<OMNI_DOUBLE, OMNI_BOOLEAN>;

template class MaxAggregator<OMNI_DOUBLE, OMNI_SHORT>;

template class MaxAggregator<OMNI_DOUBLE, OMNI_INT>;

template class MaxAggregator<OMNI_DOUBLE, OMNI_LONG>;

template class MaxAggregator<OMNI_DOUBLE, OMNI_DOUBLE>;

template class MaxAggregator<OMNI_DOUBLE, OMNI_DECIMAL128>;

template class MaxAggregator<OMNI_DOUBLE, OMNI_DECIMAL64>;

template class MaxAggregator<OMNI_DECIMAL128, OMNI_BOOLEAN>;

template class MaxAggregator<OMNI_DECIMAL128, OMNI_SHORT>;

template class MaxAggregator<OMNI_DECIMAL128, OMNI_INT>;

template class MaxAggregator<OMNI_DECIMAL128, OMNI_LONG>;

template class MaxAggregator<OMNI_DECIMAL128, OMNI_DOUBLE>;

template class MaxAggregator<OMNI_DECIMAL128, OMNI_DECIMAL128>;

template class MaxAggregator<OMNI_DECIMAL128, OMNI_DECIMAL64>;

template class MaxAggregator<OMNI_DECIMAL64, OMNI_BOOLEAN>;

template class MaxAggregator<OMNI_DECIMAL64, OMNI_SHORT>;

template class MaxAggregator<OMNI_DECIMAL64, OMNI_INT>;

template class MaxAggregator<OMNI_DECIMAL64, OMNI_LONG>;

template class MaxAggregator<OMNI_DECIMAL64, OMNI_DOUBLE>;

template class MaxAggregator<OMNI_DECIMAL64, OMNI_DECIMAL128>;

template class MaxAggregator<OMNI_DECIMAL64, OMNI_DECIMAL64>;
} // namespace op
} // namespace omniruntime
