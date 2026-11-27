/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

/*
 * Description: Min aggregate
 */

#include "min_aggregator.h"
#include "simd/func/reduce.h"

namespace omniruntime {
namespace op {
template <DataTypeId IN_ID, DataTypeId OUT_ID>
void MinAggregator<IN_ID, OUT_ID>::ExtractValues(const AggregateState *state, std::vector<BaseVector *> &vectors,
                                                 int32_t rowIndex)
{
    auto *minState = MinState::ConstCastState(state + aggStateOffset);
    auto v = static_cast<OutVector *>(vectors[0]);
    if (minState->IsEmpty()) {
        v->SetNull(rowIndex);
        return;
    }

    bool overflow = minState->IsOverFlowed();
    auto result = CastWithOverflow<ResultType, OutType>(minState->value, overflow);
    v->SetValue(rowIndex, result);
    if (overflow) {
        this->SetNullOrThrowException(v, rowIndex, "min_aggregator overflow.");
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void MinAggregator<IN_ID, OUT_ID>::ExtractValuesBatch(std::vector<AggregateState *> &groupStates,
                                                      std::vector<BaseVector *> &vectors, int32_t rowOffset,
                                                      int32_t rowCount)
{
    auto v = static_cast<OutVector *>(vectors[0]);
    for (int32_t rowIndex = 0; rowIndex < rowCount; rowIndex++) {
        auto *state = MinState::CastState(groupStates[rowIndex] + aggStateOffset);
        if (state->IsEmpty()) {
            v->SetNull(rowIndex);
        } else if (state->IsOverFlowed()) {
            this->SetNullOrThrowException(v, rowIndex, "min_aggregator overflow.");
        } else {
            OutType result;
            bool isOverflow = false;
            result = CastWithOverflow<ResultType, OutType>(state->value, isOverflow);
            if (isOverflow) {
                this->SetNullOrThrowException(v, rowIndex, "min_aggregator overflow.");
            } else {
                v->SetValue(rowIndex, result);
            }
        }
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID> std::vector<DataTypePtr> MinAggregator<IN_ID, OUT_ID>::GetSpillType()
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
void MinAggregator<IN_ID, OUT_ID>::ExtractValuesForSpill(std::vector<AggregateState *> &groupStates,
                                                         std::vector<BaseVector *> &vectors)
{
    auto spillValueVec = static_cast<Vector<ResultType> *>(vectors[0]);
    auto rowCount = static_cast<int32_t>(groupStates.size());
    for (int32_t rowIndex = 0; rowIndex < rowCount; rowIndex++) {
        auto *state = MinState::CastState(groupStates[rowIndex] + aggStateOffset);
        if (state->IsEmpty()) {
            spillValueVec->SetNull(rowIndex);
        } else {
            spillValueVec->SetValue(rowIndex, state->value);
        }
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID> void MinAggregator<IN_ID, OUT_ID>::InitState(AggregateState *state)
{
    auto *minState = MinState::CastState(state + aggStateOffset);
    minState->value = GetMax<ResultType>();
    minState->valueState = AggValueState::EMPTY_VALUE;
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void MinAggregator<IN_ID, OUT_ID>::InitStates(std::vector<AggregateState *> &groupStates)
{
    ResultType maxVal = GetMax<ResultType>();
    for (auto groupState : groupStates) {
        auto *state = MinState::CastState(groupState + aggStateOffset);
        state->value = maxVal;
        state->valueState = AggValueState::EMPTY_VALUE;
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void MinAggregator<IN_ID, OUT_ID>::ProcessSingleInternal(AggregateState *state, BaseVector *vector,
                                                         const int32_t rowOffset, const int32_t rowCount,
                                                         const std::shared_ptr<NullsHelper> nullMap)
{
    auto *minState = MinState::CastState(state);
    if (vector->GetEncoding() != vec::OMNI_DICTIONARY) {
        auto *ptr = reinterpret_cast<InType *>(GetValuesFromVector<IN_ID>(vector));
        ptr += rowOffset;
        if (nullMap == nullptr) {
            if constexpr (CheckTypesContainsDecimal128<InType, ResultType>::value) {
                Add<InType, ResultType, AggValueState, MinOp<InType, ResultType>>(&minState->value,
                                                                                  minState->valueState, ptr, rowCount);
            } else {
                simd::ReduceExternal<InType, ResultType, AggValueState, StateValueHandler, simd::ReduceFunc::Min>(
                    &minState->value, minState->valueState, ptr, rowCount);
            }
        } else {
            if constexpr (CheckTypesContainsDecimal128<InType, ResultType>::value) {
                AddConditional<InType, ResultType, AggValueState, MinConditionalOp<InType, ResultType, false>>(
                    &minState->value, minState->valueState, ptr, rowCount, *nullMap);
            } else {
                auto conditionArray = nullMap->convertToArray(rowCount);
                simd::ReduceWithNullsExternal<InType, ResultType, AggValueState, StateValueHandler,
                                              simd::ReduceFunc::Min>(&minState->value, minState->valueState, ptr,
                                                                     rowCount, conditionArray.data());
            }
        }
    } else {
        auto *ptr = reinterpret_cast<InType *>(GetValuesFromDict<IN_ID>(vector));
        auto *indexMap = GetIdsFromDict<IN_ID>(vector) + rowOffset;
        if (nullMap == nullptr) {
            if constexpr (CheckTypesContainsDecimal128<InType, ResultType>::value) {
                AddDict<InType, ResultType, AggValueState, MinOp<InType, ResultType>>(
                    &minState->value, minState->valueState, ptr, rowCount, indexMap);
            } else {
                simd::ReduceWithDicExternal<InType, ResultType, AggValueState, StateValueHandler,
                                            simd::ReduceFunc::Min>(&minState->value, minState->valueState, ptr,
                                                                   rowCount, indexMap);
            }
        } else {
            if constexpr (CheckTypesContainsDecimal128<InType, ResultType>::value) {
                AddDictConditional<InType, ResultType, AggValueState, MinConditionalOp<InType, ResultType, false>>(
                    &minState->value, minState->valueState, ptr, rowCount, *nullMap, indexMap);
            } else {
                auto conditionArray = nullMap->convertToArray(rowCount);
                simd::ReduceWithDicAndNullsExternal<InType, ResultType, AggValueState, StateValueHandler,
                                                    simd::ReduceFunc::Min>(&minState->value, minState->valueState, ptr,
                                                                           rowCount, conditionArray.data(), indexMap);
            }
        }
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void MinAggregator<IN_ID, OUT_ID>::ProcessGroupInternal(std::vector<AggregateState *> &rowStates, BaseVector *vector,
                                                        const int32_t rowOffset,
                                                        const std::shared_ptr<NullsHelper> nullMap)
{
    if (vector->GetEncoding() != vec::OMNI_DICTIONARY) {
        auto *ptr = reinterpret_cast<InType *>(GetValuesFromVector<IN_ID>(vector));
        ptr += rowOffset;
        if (nullMap == nullptr) {
            AddUseRowIndex<InType, MinState::template UpdateState<InType, ResultType>>(rowStates, aggStateOffset, ptr);
        } else {
            AddConditionalUseRowIndex<InType, MinState::template UpdateStateWithCondition<InType, ResultType, false>>(
                rowStates, aggStateOffset, ptr, *nullMap);
        }
    } else {
        auto *ptr = reinterpret_cast<InType *>(GetValuesFromDict<IN_ID>(vector));
        auto *indexMap = GetIdsFromDict<IN_ID>(vector) + rowOffset;
        if (nullMap == nullptr) {
            AddDictUseRowIndex<InType, MinState::template UpdateState<InType, ResultType>>(rowStates, aggStateOffset,
                                                                                           ptr, indexMap);
        } else {
            AddDictConditionalUseRowIndex<InType, ResultType,
                                          MinState::template UpdateStateWithCondition<InType, ResultType, false>>(
                rowStates, aggStateOffset, ptr, *nullMap, indexMap);
        }
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void MinAggregator<IN_ID, OUT_ID>::ProcessGroupUnspill(std::vector<UnspillRowInfo> &unspillRows, int32_t rowCount,
                                                       int32_t &vectorIndex)
{
    auto firstVecIdx = vectorIndex++;
    for (int32_t rowIdx = 0; rowIdx < rowCount; rowIdx++) {
        auto &row = unspillRows[rowIdx];
        auto batch = row.batch;
        auto index = row.rowIdx;
        auto vectorPtr = static_cast<Vector<ResultType> *>(batch->Get(firstVecIdx));
        if (!vectorPtr->IsNull(index)) {
            auto *state = MinState::CastState(row.state + aggStateOffset);
            auto value = vectorPtr->GetValue(index);
            MinOp<ResultType, ResultType>(&state->value, state->valueState, value, 1LL);
        }
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void MinAggregator<IN_ID, OUT_ID>::ProcessAlignAggSchema(VectorBatch *result, BaseVector *originVector,
                                                         const std::shared_ptr<NullsHelper> nullMap,
                                                         const bool aggFilter)
{
    int rowCount = originVector->GetSize();
    if constexpr (std::is_same_v<InType, OutType>) {
        if (!aggFilter) {
            auto minVector = VectorHelper::SliceVector(originVector, 0, rowCount);
            result->Append(minVector);
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
void MinAggregator<IN_ID, OUT_ID>::ProcessAlignAggSchemaInternal(VectorBatch *result, BaseVector *originVector,
                                                                 const std::shared_ptr<NullsHelper> nullMap)
{
    int rowCount = originVector->GetSize();
    auto minVector = reinterpret_cast<OutVector *>(VectorHelper::CreateFlatVector(OUT_ID, rowCount));
    auto vector = reinterpret_cast<T *>(originVector);
    if (nullMap != nullptr) {
        for (int index = 0; index < rowCount; ++index) {
            if ((*nullMap)[index]) {
                minVector->SetNull(index);
            } else {
                InType val = vector->GetValue(index);
                bool overflow = false;
                OutType out = this->template CastWithOverflow<InType, OutType>(static_cast<InType>(val), overflow);
                minVector->SetValue(index, out);
            }
        }
    } else {
        for (int index = 0; index < rowCount; ++index) {
            InType val = vector->GetValue(index);
            bool overflow = false;
            OutType out = this->template CastWithOverflow<InType, OutType>(static_cast<InType>(val), overflow);
            minVector->SetValue(index, out);
        }
    }
    result->Append(minVector);
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
MinAggregator<IN_ID, OUT_ID>::MinAggregator(const DataTypes &inputTypes, const DataTypes &outputTypes,
                                            std::vector<int32_t> &channels, const bool inputRaw,
                                            const bool outputPartial, const bool isOverflowAsNull)
    : TypedAggregator(OMNI_AGGREGATION_TYPE_MIN, inputTypes, outputTypes, channels, inputRaw, outputPartial,
                      isOverflowAsNull)
{
}

// Explicit template instantiation
// Defining templated aggregators in header file consume a lot of memory during compilation
// since, compiler needs to generate each individual template instance wherever aggregator header is include
// to reduce time and memory usage during compilation moved templated aggregator implementation into .cpp files
// and used explicit template instantiation to generate template instances
template class MinAggregator<OMNI_BOOLEAN, OMNI_BOOLEAN>;

template class MinAggregator<OMNI_BOOLEAN, OMNI_SHORT>;

template class MinAggregator<OMNI_BOOLEAN, OMNI_INT>;

template class MinAggregator<OMNI_BOOLEAN, OMNI_LONG>;

template class MinAggregator<OMNI_BOOLEAN, OMNI_DOUBLE>;

template class MinAggregator<OMNI_BOOLEAN, OMNI_DECIMAL128>;

template class MinAggregator<OMNI_BOOLEAN, OMNI_DECIMAL64>;

template class MinAggregator<OMNI_SHORT, OMNI_BOOLEAN>;

template class MinAggregator<OMNI_SHORT, OMNI_SHORT>;

template class MinAggregator<OMNI_SHORT, OMNI_INT>;

template class MinAggregator<OMNI_SHORT, OMNI_LONG>;

template class MinAggregator<OMNI_SHORT, OMNI_DOUBLE>;

template class MinAggregator<OMNI_SHORT, OMNI_DECIMAL128>;

template class MinAggregator<OMNI_SHORT, OMNI_DECIMAL64>;

template class MinAggregator<OMNI_INT, OMNI_BOOLEAN>;

template class MinAggregator<OMNI_INT, OMNI_SHORT>;

template class MinAggregator<OMNI_INT, OMNI_INT>;

template class MinAggregator<OMNI_INT, OMNI_LONG>;

template class MinAggregator<OMNI_INT, OMNI_DOUBLE>;

template class MinAggregator<OMNI_INT, OMNI_DECIMAL128>;

template class MinAggregator<OMNI_INT, OMNI_DECIMAL64>;

template class MinAggregator<OMNI_LONG, OMNI_BOOLEAN>;

template class MinAggregator<OMNI_LONG, OMNI_SHORT>;

template class MinAggregator<OMNI_LONG, OMNI_INT>;

template class MinAggregator<OMNI_LONG, OMNI_LONG>;

template class MinAggregator<OMNI_LONG, OMNI_DOUBLE>;

template class MinAggregator<OMNI_LONG, OMNI_DECIMAL128>;

template class MinAggregator<OMNI_LONG, OMNI_DECIMAL64>;

template class MinAggregator<OMNI_DOUBLE, OMNI_BOOLEAN>;

template class MinAggregator<OMNI_DOUBLE, OMNI_SHORT>;

template class MinAggregator<OMNI_DOUBLE, OMNI_INT>;

template class MinAggregator<OMNI_DOUBLE, OMNI_LONG>;

template class MinAggregator<OMNI_DOUBLE, OMNI_DOUBLE>;

template class MinAggregator<OMNI_DOUBLE, OMNI_DECIMAL128>;

template class MinAggregator<OMNI_DOUBLE, OMNI_DECIMAL64>;

template class MinAggregator<OMNI_DECIMAL128, OMNI_BOOLEAN>;

template class MinAggregator<OMNI_DECIMAL128, OMNI_SHORT>;

template class MinAggregator<OMNI_DECIMAL128, OMNI_INT>;

template class MinAggregator<OMNI_DECIMAL128, OMNI_LONG>;

template class MinAggregator<OMNI_DECIMAL128, OMNI_DOUBLE>;

template class MinAggregator<OMNI_DECIMAL128, OMNI_DECIMAL128>;

template class MinAggregator<OMNI_DECIMAL128, OMNI_DECIMAL64>;

template class MinAggregator<OMNI_DECIMAL64, OMNI_BOOLEAN>;

template class MinAggregator<OMNI_DECIMAL64, OMNI_SHORT>;

template class MinAggregator<OMNI_DECIMAL64, OMNI_INT>;

template class MinAggregator<OMNI_DECIMAL64, OMNI_LONG>;

template class MinAggregator<OMNI_DECIMAL64, OMNI_DOUBLE>;

template class MinAggregator<OMNI_DECIMAL64, OMNI_DECIMAL128>;

template class MinAggregator<OMNI_DECIMAL64, OMNI_DECIMAL64>;
} // namespace op
} // namespace omniruntime
