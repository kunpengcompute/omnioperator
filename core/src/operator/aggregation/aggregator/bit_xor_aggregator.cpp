/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: BitXor aggregate
 */

#include "bit_xor_aggregator.h"
#include "simd/func/reduce.h"

namespace omniruntime {
namespace op {
template <DataTypeId IN_ID, DataTypeId OUT_ID>
void BitXorAggregator<IN_ID, OUT_ID>::ExtractValues(const AggregateState *state, std::vector<BaseVector *> &vectors,
    int32_t rowIndex)
{
    auto v = static_cast<OutVector *>(vectors[0]);
    auto bitXorState = BitXorState::ConstCastState(state + aggStateOffset);
    if (bitXorState->IsEmpty()) {
        v->SetNull(rowIndex);
        return;
    }
    v->SetValue(rowIndex, bitXorState->value);
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void BitXorAggregator<IN_ID, OUT_ID>::ExtractValuesBatch(std::vector<AggregateState *> &groupStates,
    std::vector<BaseVector *> &vectors, int32_t rowOffset, int32_t rowCount)
{
    auto v = static_cast<OutVector *>(vectors[0]);
    for (int32_t rowIndex = 0; rowIndex < rowCount; rowIndex++) {
        auto state = BitXorState::CastState(groupStates[rowIndex] + aggStateOffset);
        if (state->IsEmpty()) {
            v->SetNull(rowIndex);
        } else {
            v->SetValue(rowIndex, state->value);
        }
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID> std::vector<DataTypePtr> BitXorAggregator<IN_ID, OUT_ID>::GetSpillType()
{
    std::vector<DataTypePtr> spillTypes;
    spillTypes.emplace_back(std::make_shared<DataType>(OUT_ID));
    return spillTypes;
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void BitXorAggregator<IN_ID, OUT_ID>::ExtractValuesForSpill(std::vector<AggregateState *> &groupStates,
    std::vector<BaseVector *> &vectors)
{
    const int32_t rowCount = static_cast<int32_t>(groupStates.size());
    auto *v = static_cast<OutVector *>(vectors[0]);
    for (int32_t i = 0; i < rowCount; i++) {
        auto *s = BitXorState::CastState(groupStates[i] + aggStateOffset);
        if (s->IsEmpty()) {
            v->SetNull(i);
        } else {
            v->SetValue(i, s->value);
        }
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID> void BitXorAggregator<IN_ID, OUT_ID>::InitState(AggregateState *state)
{
    auto *bitXorState = BitXorState::CastState(state + aggStateOffset);
    bitXorState->value = GetBitXorInit<ResultType>();
    bitXorState->valueState = AggValueState::EMPTY_VALUE;
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void BitXorAggregator<IN_ID, OUT_ID>::InitStates(std::vector<AggregateState *> &groupStates)
{
    for (auto groupState : groupStates) {
        InitState(groupState);
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void BitXorAggregator<IN_ID, OUT_ID>::ProcessSingleInternal(AggregateState *state, BaseVector *vector,
    const int32_t rowOffset, const int32_t rowCount, const std::shared_ptr<NullsHelper> nullMap)
{
    auto bitXorState = BitXorState::CastState(state);
    if (vector->GetEncoding() == vec::OMNI_ENCODING_CONST) {
        if (nullMap == nullptr) {
            auto constValue = static_cast<vec::ConstVector<InType> *>(vector)->GetConstValue();
            for (int32_t i = 0; i < rowCount; ++i) {
                BitXorOp<InType, ResultType>(&bitXorState->value, bitXorState->valueState, constValue, 1LL);
            }
        }
    } else {
        auto *ptr = reinterpret_cast<InType *>(GetValuesFromVector<IN_ID>(vector));
        ptr += rowOffset;
        if (nullMap == nullptr) {
            Add<InType, ResultType, AggValueState, BitXorOp<InType, ResultType>>(&bitXorState->value,
                            bitXorState->valueState, ptr, rowCount);
        } else {
            AddConditional<InType, ResultType, AggValueState, BitXorConditionalOp<InType, ResultType, false>>(
                            &bitXorState->value, bitXorState->valueState, ptr, rowCount, *nullMap);
        }
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void BitXorAggregator<IN_ID, OUT_ID>::ProcessGroupInternal(std::vector<AggregateState *> &rowStates, BaseVector *vector,
    const int32_t rowOffset, const std::shared_ptr<NullsHelper> nullMap)
{
    if (vector->GetEncoding() == vec::OMNI_ENCODING_CONST) {
        if (nullMap == nullptr) {
            auto constValue = static_cast<vec::ConstVector<InType> *>(vector)->GetConstValue();
            for (size_t i = 0; i < rowStates.size(); ++i) {
                BitXorState::template UpdateState<InType, ResultType>(rowStates[i] + aggStateOffset, constValue);
            }
        }
    } else {
        auto *ptr = reinterpret_cast<InType *>(GetValuesFromVector<IN_ID>(vector));
        ptr += rowOffset;
        if (nullMap == nullptr) {
            AddUseRowIndex<InType, BitXorState::template UpdateState<InType, ResultType>>(rowStates, aggStateOffset, ptr);
        } else {
            AddConditionalUseRowIndex<InType, BitXorState::template UpdateStateWithCondition<InType, ResultType, false>>(
                rowStates, aggStateOffset, ptr, *nullMap);
        }
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void BitXorAggregator<IN_ID, OUT_ID>::ProcessGroupUnspill(std::vector<UnspillRowInfo> &unspillRows, int32_t rowCount,
    int32_t &vectorIndex)
{
    auto firstVecIdx = vectorIndex++;
    for (int32_t rowIdx = 0; rowIdx < rowCount; rowIdx++) {
        auto &row = unspillRows[rowIdx];
        auto batch = row.batch;
        auto index = row.rowIdx;
        auto vectorPtr = static_cast<Vector<ResultType> *>(batch->Get(firstVecIdx));
        if (!vectorPtr->IsNull(index)) {
            auto bitXorState = BitXorState::CastState(row.state + aggStateOffset);
            auto value = vectorPtr->GetValue(index);
            BitXorOp<ResultType, ResultType>((&bitXorState->value), bitXorState->valueState, value, 1LL);
        }
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void BitXorAggregator<IN_ID, OUT_ID>::ProcessAlignAggSchema(VectorBatch *result, BaseVector *originVector,
    const std::shared_ptr<NullsHelper> nullMap, const bool aggFilter)
{
    int rowCount = originVector->GetSize();
    if constexpr (std::is_same_v<InType, OutType>) {
        if (!aggFilter) {
            auto bitXorVector = VectorHelper::SliceVector(originVector, 0, rowCount);
            result->Append(bitXorVector);
            return;
        }
    }

    if (originVector->GetEncoding() == OMNI_DICTIONARY) {
        ProcessAlignAggSchemaInternal<Vector<DictionaryContainer<InType>>>(result, originVector, nullMap);
    } else {
        ProcessAlignAggSchemaInternal<Vector<InType>>(result, originVector, nullMap);
    }
}

template<DataTypeId IN_ID, DataTypeId OUT_ID>
template<typename T>
void BitXorAggregator<IN_ID, OUT_ID>::ProcessAlignAggSchemaInternal(VectorBatch *result, BaseVector *originVector,
    const std::shared_ptr<NullsHelper> nullMap)
{
    int rowCount = originVector->GetSize();
    auto bitXorVector = reinterpret_cast<OutVector *>(VectorHelper::CreateFlatVector(this->pool_, OUT_ID, rowCount));
    auto vector = reinterpret_cast<T *>(originVector);
    if (nullMap != nullptr) {
        for (int index = 0; index < rowCount; ++index) {
            if ((*nullMap)[index]) {
                bitXorVector->SetNull(index);
            } else {
                InType val = vector->GetValue(index);
                bool overflow = false;
                OutType out = this->template CastWithOverflow<InType, OutType>(static_cast<InType>(val), overflow);
                bitXorVector->SetValue(index, out);
            }
        }
    } else {
        for (int index = 0; index < rowCount; ++index) {
            InType val = vector->GetValue(index);
            bool overflow = false;
            OutType out = this->template CastWithOverflow<InType, OutType>(static_cast<InType>(val), overflow);
            bitXorVector->SetValue(index, out);
        }
    }
    result->Append(bitXorVector);
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
BitXorAggregator<IN_ID, OUT_ID>::BitXorAggregator(const DataTypes &inputTypes, const DataTypes &outputTypes,
    std::vector<int32_t> &channels, const bool inputRaw, const bool outputPartial, const bool isOverflowAsNull)
    : TypedAggregator(OMNI_AGGREGATION_TYPE_BIT_XOR, inputTypes, outputTypes, channels, inputRaw, outputPartial,
    isOverflowAsNull)
{}

template class BitXorAggregator<OMNI_BYTE, OMNI_BYTE>;

template class BitXorAggregator<OMNI_SHORT, OMNI_SHORT>;

template class BitXorAggregator<OMNI_INT, OMNI_INT>;

template class BitXorAggregator<OMNI_LONG, OMNI_LONG>;
}
}
