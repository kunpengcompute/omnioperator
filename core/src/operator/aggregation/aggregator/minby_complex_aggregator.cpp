/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Min_by complex aggregator - template implementations and explicit instantiations.
 */

#include "minby_complex_aggregator.h"
#include "minmax_by_align_schema_helper.h"
#include "type/data_type.h"

namespace omniruntime {
namespace op {

template <type::DataTypeId COL2_ID>
void MinByComplexAggregator<COL2_ID>::InitState(AggregateState *state)
{
    using State = typename MinByComplexAggregator<COL2_ID>::ComplexState;
    auto *complexState = State::CastState(state + aggStateOffset);
    complexState->isEmpty = true;
    complexState->targetValue = nullptr;
    complexState->sortKey = GetSortKeyMax<sortKeyType>();
}

template <type::DataTypeId COL2_ID>
void MinByComplexAggregator<COL2_ID>::InitStates(std::vector<AggregateState *> &groupStates)
{
    for (auto *groupState : groupStates) {
        InitState(groupState);
    }
}

template <type::DataTypeId COL2_ID>
std::vector<DataTypePtr> MinByComplexAggregator<COL2_ID>::GetSpillType()
{
    std::vector<DataTypePtr> spillTypes;
    spillTypes.emplace_back(GetOutputTypes().GetType(0));
    spillTypes.emplace_back(std::make_shared<DataType>(COL2_ID));
    return spillTypes;
}

template <type::DataTypeId COL2_ID>
void MinByComplexAggregator<COL2_ID>::ProcessSingleInternal(AggregateState *state, BaseVector *vector,
    const int32_t rowOffset, const int32_t rowCount, const std::shared_ptr<NullsHelper> nullMap)
{
    (void)vector;
    using State = typename MinByComplexAggregator<COL2_ID>::ComplexState;
    auto *complexState = State::CastState(state);
    BaseVector *col1Vector = this->curVectorBatch->Get(this->channels[0]);
    BaseVector *col2Vector = this->curVectorBatch->Get(this->channels[1]);
    for (int32_t i = 0; i < rowCount; i++) {
        if (nullMap != nullptr && (*nullMap)[i]) {
            continue;
        }
        int32_t rowIndex = rowOffset + i;
        if (col2Vector->IsNull(rowIndex)) {
            continue;  // Spark: NULL values are ignored from processing by aggregate functions
        }
        sortKeyType key = VectorHelper::GetFlatValue<COL2_ID>(col2Vector, rowIndex);
        if (key <= complexState->sortKey) {
            complexState->isEmpty = false;
            complexState->sortKey = key;
            ComplexSliceHolder sliceHolder(GetComplexColSlice(col1Vector, targetColTypeId_, rowIndex));
            vec::BaseVector *slice = sliceHolder.get();
            if (IsComplexSliceNull(slice, targetColTypeId_)) {
                if (complexState->targetValue != nullptr) {
                    ownedValues_.Release(complexState->targetValue);
                }
                complexState->targetValue = nullptr;
            } else {
                if (complexState->targetValue != nullptr) {
                    ownedValues_.Release(complexState->targetValue);
                }
                complexState->targetValue = ownedValues_.Copy(slice, targetColTypeId_, targetColDataType_.get());
            }
        }
    }
}

template <type::DataTypeId COL2_ID>
void MinByComplexAggregator<COL2_ID>::ProcessGroupInternal(std::vector<AggregateState *> &rowStates,
    BaseVector *vector, const int32_t rowOffset, const std::shared_ptr<NullsHelper> nullMap)
{
    (void)vector;
    using State = typename MinByComplexAggregator<COL2_ID>::ComplexState;
    BaseVector *col1Vector = this->curVectorBatch->Get(this->channels[0]);
    BaseVector *col2Vector = this->curVectorBatch->Get(this->channels[1]);
    const size_t rowCount = rowStates.size();
    for (size_t i = 0; i < rowCount; i++) {
        if (nullMap != nullptr && (*nullMap)[i]) {
            continue;
        }
        int32_t rowIdx = static_cast<int32_t>(rowOffset + i);
        if (col2Vector->IsNull(rowIdx)) {
            continue;  // Spark: NULL values are ignored from processing by aggregate functions
        }
        auto *complexState = State::CastState(rowStates[i] + aggStateOffset);
        sortKeyType key = VectorHelper::GetFlatValue<COL2_ID>(col2Vector, rowIdx);
        if (key <= complexState->sortKey) {
            complexState->isEmpty = false;
            complexState->sortKey = key;
            ComplexSliceHolder sliceHolder(GetComplexColSlice(col1Vector, targetColTypeId_, rowIdx));
            vec::BaseVector *slice = sliceHolder.get();
            if (IsComplexSliceNull(slice, targetColTypeId_)) {
                if (complexState->targetValue != nullptr) {
                    ownedValues_.Release(complexState->targetValue);
                }
                complexState->targetValue = nullptr;
            } else {
                if (complexState->targetValue != nullptr) {
                    ownedValues_.Release(complexState->targetValue);
                }
                complexState->targetValue = ownedValues_.Copy(slice, targetColTypeId_, targetColDataType_.get());
            }
        }
    }
}

template <type::DataTypeId COL2_ID>
void MinByComplexAggregator<COL2_ID>::ExtractValues(const AggregateState *state,
    std::vector<BaseVector *> &vectors, int32_t rowIndex)
{
    using State = typename MinByComplexAggregator<COL2_ID>::ComplexState;
    const auto *complexState = State::ConstCastState(state + aggStateOffset);
    if (this->outputPartial) {
        if (complexState->isEmpty) {
            vectors[0]->SetNull(rowIndex);
            vectors[1]->SetNull(rowIndex);
            return;
        }
        if (complexState->targetValue == nullptr) {
            vectors[0]->SetNull(rowIndex);
        } else {
            SetComplexColValue(vectors[0], targetColTypeId_, rowIndex, complexState->targetValue);
        }
        static_cast<sortKeyTypeVec *>(vectors[1])->SetValue(rowIndex, complexState->sortKey);
    } else {
        if (complexState->isEmpty) {
            vectors[0]->SetNull(rowIndex);
            return;
        }
        if (complexState->targetValue == nullptr) {
            vectors[0]->SetNull(rowIndex);
            return;
        }
        SetComplexColValue(vectors[0], targetColTypeId_, rowIndex, complexState->targetValue);
    }
}

template <type::DataTypeId COL2_ID>
void MinByComplexAggregator<COL2_ID>::ExtractValuesBatch(std::vector<AggregateState *> &groupStates,
    std::vector<BaseVector *> &vectors, int32_t rowOffset, int32_t rowCount)
{
    (void)rowOffset;
    using State = typename MinByComplexAggregator<COL2_ID>::ComplexState;
    if (this->outputPartial) {
        auto *sortKeyVector = static_cast<sortKeyTypeVec *>(vectors[1]);
        for (int32_t i = 0; i < rowCount; i++) {
            const auto *complexState = State::ConstCastState(groupStates[i] + aggStateOffset);
            if (complexState->isEmpty) {
                vectors[0]->SetNull(i);
                vectors[1]->SetNull(i);
                continue;
            }
            if (complexState->targetValue == nullptr) {
                vectors[0]->SetNull(i);
            } else {
                SetComplexColValue(vectors[0], targetColTypeId_, i, complexState->targetValue);
            }
            sortKeyVector->SetValue(i, complexState->sortKey);
        }
    } else {
        for (int32_t i = 0; i < rowCount; i++) {
            const auto *complexState = State::ConstCastState(groupStates[i] + aggStateOffset);
            if (complexState->isEmpty) {
                vectors[0]->SetNull(i);
                continue;
            }
            if (complexState->targetValue == nullptr) {
                vectors[0]->SetNull(i);
                continue;
            }
            SetComplexColValue(vectors[0], targetColTypeId_, i, complexState->targetValue);
        }
    }
}

template <type::DataTypeId COL2_ID>
void MinByComplexAggregator<COL2_ID>::ExtractValuesForSpill(std::vector<AggregateState *> &groupStates,
    std::vector<BaseVector *> &vectors)
{
    using State = typename MinByComplexAggregator<COL2_ID>::ComplexState;
    auto *sortKeyVector = static_cast<sortKeyTypeVec *>(vectors[1]);
    int32_t rowCount = static_cast<int32_t>(groupStates.size());
    for (int32_t rowIndex = 0; rowIndex < rowCount; rowIndex++) {
        auto *complexState = State::CastState(groupStates[rowIndex] + aggStateOffset);
        if (complexState->isEmpty) {
            sortKeyVector->SetNull(rowIndex);
            vectors[0]->SetNull(rowIndex);
            continue;
        }
        if (complexState->targetValue == nullptr) {
            vectors[0]->SetNull(rowIndex);
        } else {
            SetComplexColValue(vectors[0], targetColTypeId_, rowIndex, complexState->targetValue);
            // Spilled states are discarded (arena reset) after extraction.
            ownedValues_.Release(complexState->targetValue);
            complexState->targetValue = nullptr;
        }
        sortKeyVector->SetValue(rowIndex, complexState->sortKey);
    }
}

template <type::DataTypeId COL2_ID>
void MinByComplexAggregator<COL2_ID>::ProcessGroupUnspill(std::vector<UnspillRowInfo> &unspillRows,
    int32_t rowCount, int32_t &vectorIndex)
{
    using State = typename MinByComplexAggregator<COL2_ID>::ComplexState;
    int32_t targetValueVecIdx = vectorIndex++;
    int32_t sortKeyVecIdx = vectorIndex++;
    for (int32_t rowIdx = 0; rowIdx < rowCount; rowIdx++) {
        auto &row = unspillRows[rowIdx];
        VectorBatch *batch = row.batch;
        int32_t index = row.rowIdx;
        BaseVector *targetValueVector = batch->Get(targetValueVecIdx);
        auto *complexState = State::CastState(row.state + aggStateOffset);
        BaseVector *sortKeyVec = batch->Get(sortKeyVecIdx);
        if (sortKeyVec->IsNull(index)) {
            continue;  // Spark: NULL values are ignored from processing by aggregate functions
        }
        sortKeyType sortKey = static_cast<sortKeyTypeVec *>(sortKeyVec)->GetValue(index);
        if (sortKey <= complexState->sortKey) {
            complexState->isEmpty = false;
            complexState->sortKey = sortKey;
            ComplexSliceHolder sliceHolder(GetComplexColSlice(targetValueVector, targetColTypeId_, index));
            BaseVector *targetSlice = sliceHolder.get();
            if (IsComplexSliceNull(targetSlice, targetColTypeId_)) {
                if (complexState->targetValue != nullptr) {
                    ownedValues_.Release(complexState->targetValue);
                }
                complexState->targetValue = nullptr;
            } else {
                if (complexState->targetValue != nullptr) {
                    ownedValues_.Release(complexState->targetValue);
                }
                complexState->targetValue = ownedValues_.Copy(targetSlice, targetColTypeId_,
                    targetColDataType_.get());
            }
        }
    }
}

template <type::DataTypeId COL2_ID>
void MinByComplexAggregator<COL2_ID>::AlignAggSchema(VectorBatch *result, VectorBatch *inputVecBatch)
{
    MinMaxByComplexAlignAggSchema<COL2_ID>(result, inputVecBatch, channels, inputRaw, targetColTypeId_,
        targetColDataType_);
}

template <type::DataTypeId COL2_ID>
void MinByComplexAggregator<COL2_ID>::AlignAggSchemaWithFilter(VectorBatch *result, VectorBatch *inputVecBatch,
    const int32_t filterIndex)
{
    MinMaxByComplexAlignAggSchemaWithFilter<COL2_ID>(result, inputVecBatch, channels, inputRaw, filterIndex,
        targetColTypeId_, targetColDataType_);
}

template <type::DataTypeId COL2_ID>
void MinByComplexAggregator<COL2_ID>::ProcessAlignAggSchema(VectorBatch *result, BaseVector *originVector,
    const std::shared_ptr<NullsHelper> nullMap, const bool aggFilter)
{
    (void)nullMap;
    (void)aggFilter;
    if (originVector == nullptr) {
        MinMaxByComplexAlignAppendEmptyPartial2(result, COL2_ID, targetColDataType_);
    }
}

// Explicit instantiations for COL2 types used when col1 is ARRAY/MAP/ROW (non-VARCHAR/CHAR col2).
template class MinByComplexAggregator<OMNI_BOOLEAN>;
template class MinByComplexAggregator<OMNI_BYTE>;
template class MinByComplexAggregator<OMNI_SHORT>;
template class MinByComplexAggregator<OMNI_INT>;
template class MinByComplexAggregator<OMNI_LONG>;
template class MinByComplexAggregator<OMNI_FLOAT>;
template class MinByComplexAggregator<OMNI_DOUBLE>;
template class MinByComplexAggregator<OMNI_DECIMAL64>;
template class MinByComplexAggregator<OMNI_DECIMAL128>;

// Force GetSpillType() to be emitted (fixes GetSpillTypeEv undefined symbol when linking).
template std::vector<DataTypePtr> MinByComplexAggregator<OMNI_BOOLEAN>::GetSpillType();
template std::vector<DataTypePtr> MinByComplexAggregator<OMNI_BYTE>::GetSpillType();
template std::vector<DataTypePtr> MinByComplexAggregator<OMNI_SHORT>::GetSpillType();
template std::vector<DataTypePtr> MinByComplexAggregator<OMNI_INT>::GetSpillType();
template std::vector<DataTypePtr> MinByComplexAggregator<OMNI_LONG>::GetSpillType();
template std::vector<DataTypePtr> MinByComplexAggregator<OMNI_FLOAT>::GetSpillType();
template std::vector<DataTypePtr> MinByComplexAggregator<OMNI_DOUBLE>::GetSpillType();
template std::vector<DataTypePtr> MinByComplexAggregator<OMNI_DECIMAL64>::GetSpillType();
template std::vector<DataTypePtr> MinByComplexAggregator<OMNI_DECIMAL128>::GetSpillType();

} // namespace op
} // namespace omniruntime
