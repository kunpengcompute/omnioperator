/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Min_by complex varchar aggregator - template implementations and explicit instantiations.
 */

#include "minby_complex_varchar_aggregator.h"
#include "minmax_by_align_schema_helper.h"
#include <algorithm>
#include <cstring>

namespace omniruntime {
namespace op {

template <type::DataTypeId COL2_ID>
void MinByComplexVarcharAggregator<COL2_ID>::ExtractValues(const AggregateState *state,
    std::vector<BaseVector *> &vectors, int32_t rowIndex)
{
    using State = typename MinByComplexVarcharAggregator<COL2_ID>::ComplexVarcharState;
    const auto *s = State::ConstCastState(state + aggStateOffset);
    if (this->outputPartial) {
        if (s->GetStrKeyAddress() == 0) {
            vectors[0]->SetNull(rowIndex);
            vectors[1]->SetNull(rowIndex);
            return;
        }
        std::string_view val(reinterpret_cast<const char *>(s->GetStrKeyAddress()), s->GetStrKeyLen());
        static_cast<Vector<LargeStringContainer<std::string_view>> *>(vectors[1])->SetValue(rowIndex, val);
        if (s->targetValue == nullptr) {
            vectors[0]->SetNull(rowIndex);
        } else {
            SetComplexColValue(vectors[0], targetColTypeId_, rowIndex, s->targetValue);
        }
        const_cast<State *>(s)->ReleaseSortKey();
    } else {
        if (s->GetStrKeyAddress() == 0) {
            vectors[0]->SetNull(rowIndex);
            return;
        }
        if (s->targetValue == nullptr) {
            vectors[0]->SetNull(rowIndex);
        } else {
            SetComplexColValue(vectors[0], targetColTypeId_, rowIndex, s->targetValue);
        }
        const_cast<State *>(s)->ReleaseSortKey();
    }
}

template <type::DataTypeId COL2_ID>
void MinByComplexVarcharAggregator<COL2_ID>::ExtractValuesBatch(std::vector<AggregateState *> &groupStates,
    std::vector<BaseVector *> &vectors, int32_t rowOffset, int32_t rowCount)
{
    (void)rowOffset;
    using State = typename MinByComplexVarcharAggregator<COL2_ID>::ComplexVarcharState;
    if (this->outputPartial) {
        auto *sortKeyVector = static_cast<Vector<LargeStringContainer<std::string_view>> *>(vectors[1]);
        for (int32_t i = 0; i < rowCount; i++) {
            const auto *s = State::ConstCastState(groupStates[i] + aggStateOffset);
            if (s->GetStrKeyAddress() == 0) {
                vectors[0]->SetNull(i);
                vectors[1]->SetNull(i);
                continue;
            }
            std::string_view val(reinterpret_cast<const char *>(s->GetStrKeyAddress()), s->GetStrKeyLen());
            sortKeyVector->SetValue(i, val);
            if (s->targetValue == nullptr) {
                vectors[0]->SetNull(i);
            } else {
                SetComplexColValue(vectors[0], targetColTypeId_, i, s->targetValue);
            }
            const_cast<State *>(s)->ReleaseSortKey();
        }
    } else {
        for (int32_t i = 0; i < rowCount; i++) {
            const auto *s = State::ConstCastState(groupStates[i] + aggStateOffset);
            if (s->GetStrKeyAddress() == 0) {
                vectors[0]->SetNull(i);
                continue;
            }
            if (s->targetValue == nullptr) {
                vectors[0]->SetNull(i);
            } else {
                SetComplexColValue(vectors[0], targetColTypeId_, i, s->targetValue);
            }
            const_cast<State *>(s)->ReleaseSortKey();
        }
    }
}

template <type::DataTypeId COL2_ID>
void MinByComplexVarcharAggregator<COL2_ID>::ExtractValuesForSpill(std::vector<AggregateState *> &groupStates,
    std::vector<BaseVector *> &vectors)
{
    using State = typename MinByComplexVarcharAggregator<COL2_ID>::ComplexVarcharState;
    auto *sortKeyVector = static_cast<Vector<LargeStringContainer<std::string_view>> *>(vectors[1]);
    int32_t rowCount = static_cast<int32_t>(groupStates.size());
    for (int32_t rowIndex = 0; rowIndex < rowCount; rowIndex++) {
        auto *s = State::CastState(groupStates[rowIndex] + aggStateOffset);
        if (s->GetStrKeyAddress() == 0) {
            sortKeyVector->SetNull(rowIndex);
            vectors[0]->SetNull(rowIndex);
            continue;
        }
        s->SaveSortKey();
        std::string_view val(reinterpret_cast<const char *>(s->GetStrKeyAddress()), s->GetStrKeyLen());
        sortKeyVector->SetValue(rowIndex, val);
        if (s->targetValue == nullptr) {
            vectors[0]->SetNull(rowIndex);
        } else {
            SetComplexColValue(vectors[0], targetColTypeId_, rowIndex, s->targetValue);
        }
        // Spilled states are discarded (arena reset) after extraction.
        ownedValues_.Release(s->targetValue);
        s->targetValue = nullptr;
        s->ReleaseSortKey();
    }
}

template <type::DataTypeId COL2_ID>
void MinByComplexVarcharAggregator<COL2_ID>::InitState(AggregateState *state)
{
    using State = typename MinByComplexVarcharAggregator<COL2_ID>::ComplexVarcharState;
    auto *s = State::CastState(state + aggStateOffset);
    s->ClearStrKey();
    s->targetValue = nullptr;
}

template <type::DataTypeId COL2_ID>
void MinByComplexVarcharAggregator<COL2_ID>::InitStates(std::vector<AggregateState *> &groupStates)
{
    for (auto *st : groupStates) {
        InitState(st);
    }
}

template <type::DataTypeId COL2_ID>
std::vector<DataTypePtr> MinByComplexVarcharAggregator<COL2_ID>::GetSpillType()
{
    return {
        GetOutputTypes().GetType(0),
        GetInputTypes().GetType(1)
    };
}

template <type::DataTypeId COL2_ID>
void MinByComplexVarcharAggregator<COL2_ID>::ProcessGroupUnspill(std::vector<UnspillRowInfo> &unspillRows,
    int32_t rowCount, int32_t &vectorIndex)
{
    using State = typename MinByComplexVarcharAggregator<COL2_ID>::ComplexVarcharState;
    int32_t targetVecIdx = vectorIndex++;
    int32_t sortKeyVecIdx = vectorIndex++;
    for (int32_t rowIdx = 0; rowIdx < rowCount; rowIdx++) {
        auto &row = unspillRows[rowIdx];
        BaseVector *targetVec = row.batch->Get(targetVecIdx);
        auto *s = State::CastState(row.state + aggStateOffset);
        BaseVector *sortKeyVec = row.batch->Get(sortKeyVecIdx);
        if (sortKeyVec->IsNull(row.rowIdx)) {
            continue;  // Spark: NULL values are ignored from processing by aggregate functions
        }
        std::string_view sortKey = static_cast<Vector<LargeStringContainer<std::string_view>> *>(sortKeyVec)->GetValue(row.rowIdx);
        bool shouldUpdate = false;
        if (s->GetStrKeyAddress() == 0) {
            shouldUpdate = true;
        } else {
            const char *curVal = sortKey.data();
            int32_t curLen = static_cast<int32_t>(sortKey.size());
            int cmp = std::memcmp(reinterpret_cast<const char *>(s->GetStrKeyAddress()), curVal,
                std::min(s->GetStrKeyLen(), curLen));
            if (cmp > 0 || (cmp == 0 && s->GetStrKeyLen() >= curLen)) {
                shouldUpdate = true;
            }
        }
        if (shouldUpdate) {
            ComplexSliceHolder sliceHolder(GetComplexColSlice(targetVec, targetColTypeId_, row.rowIdx));
            BaseVector *targetSlice = sliceHolder.get();
            if (ShouldSkipRowTargetNull(targetSlice, targetColTypeId_, targetVec, row.rowIdx)) {
                continue;  // Spark: only consider rows with non-null target
            }
            s->SetStrKey(reinterpret_cast<int64_t>(sortKey.data()), static_cast<int32_t>(sortKey.size()));
            if (s->targetValue != nullptr) {
                ownedValues_.Release(s->targetValue);
            }
            s->targetValue = ownedValues_.Copy(targetSlice, targetColTypeId_, targetColDataType_.get());
            s->SaveSortKey();
        }
    }
}

template <type::DataTypeId COL2_ID>
void MinByComplexVarcharAggregator<COL2_ID>::ProcessSingleInternal(AggregateState *state, BaseVector *vector,
    const int32_t rowOffset, const int32_t rowCount, const std::shared_ptr<NullsHelper> nullMap)
{
    (void)vector;
    using State = typename MinByComplexVarcharAggregator<COL2_ID>::ComplexVarcharState;
    auto *s = State::CastState(state);
    BaseVector *col1Vector = this->curVectorBatch->Get(this->channels[0]);
    BaseVector *col2Vector = this->curVectorBatch->Get(this->channels[1]);
    for (int32_t i = rowOffset; i < rowOffset + rowCount; i++) {
        if (nullMap != nullptr && (*nullMap)[i - rowOffset]) {
            continue;
        }
        if (col2Vector->IsNull(i)) {
            continue;  // Spark: NULL values are ignored from processing by aggregate functions
        }
        std::string_view strView = VectorHelper::GetFlatValue<COL2_ID>(col2Vector, i);
        ComplexSliceHolder sliceHolder(GetComplexColSlice(col1Vector, targetColTypeId_, i));
        vec::BaseVector *slice = sliceHolder.get();
        if (ShouldSkipRowTargetNull(slice, targetColTypeId_, col1Vector, i)) {
            continue;  // Spark: only consider rows with non-null target
        }
        if (s->GetStrKeyAddress() == 0) {
            s->SetStrKey(reinterpret_cast<int64_t>(strView.data()), static_cast<int32_t>(strView.size()));
            if (s->targetValue != nullptr) {
                ownedValues_.Release(s->targetValue);
            }
            s->targetValue = ownedValues_.Copy(slice, targetColTypeId_, targetColDataType_.get());
        } else {
            const char *curVal = strView.data();
            int32_t curLen = static_cast<int32_t>(strView.size());
            int cmp = std::memcmp(reinterpret_cast<const char *>(s->GetStrKeyAddress()), curVal,
                std::min(s->GetStrKeyLen(), curLen));
            if (cmp > 0 || (cmp == 0 && s->GetStrKeyLen() >= curLen)) {
                s->SetStrKey(reinterpret_cast<int64_t>(strView.data()), curLen);
                if (s->targetValue != nullptr) {
                    ownedValues_.Release(s->targetValue);
                }
                s->targetValue = ownedValues_.Copy(slice, targetColTypeId_, targetColDataType_.get());
            }
        }
    }
    s->SaveSortKey();
}

template <type::DataTypeId COL2_ID>
void MinByComplexVarcharAggregator<COL2_ID>::ProcessGroupInternal(std::vector<AggregateState *> &rowStates,
    BaseVector *vector, const int32_t rowOffset, const std::shared_ptr<NullsHelper> nullMap)
{
    (void)vector;
    using State = typename MinByComplexVarcharAggregator<COL2_ID>::ComplexVarcharState;
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
        auto *s = State::CastState(rowStates[i] + aggStateOffset);
        std::string_view strView = VectorHelper::GetFlatValue<COL2_ID>(col2Vector, rowIdx);
        ComplexSliceHolder sliceHolder(GetComplexColSlice(col1Vector, targetColTypeId_, rowIdx));
        vec::BaseVector *slice = sliceHolder.get();
        if (ShouldSkipRowTargetNull(slice, targetColTypeId_, col1Vector, rowIdx)) {
            continue;  // Spark: only consider rows with non-null target
        }
        if (s->GetStrKeyAddress() == 0) {
            s->SetStrKey(reinterpret_cast<int64_t>(strView.data()), static_cast<int32_t>(strView.size()));
            if (s->targetValue != nullptr) {
                ownedValues_.Release(s->targetValue);
            }
            s->targetValue = ownedValues_.Copy(slice, targetColTypeId_, targetColDataType_.get());
        } else {
            const char *curVal = strView.data();
            int32_t curLen = static_cast<int32_t>(strView.size());
            int cmp = std::memcmp(reinterpret_cast<const char *>(s->GetStrKeyAddress()), curVal,
                std::min(s->GetStrKeyLen(), curLen));
            if (cmp > 0 || (cmp == 0 && s->GetStrKeyLen() >= curLen)) {
                s->SetStrKey(reinterpret_cast<int64_t>(strView.data()), curLen);
                if (s->targetValue != nullptr) {
                    ownedValues_.Release(s->targetValue);
                }
                s->targetValue = ownedValues_.Copy(slice, targetColTypeId_, targetColDataType_.get());
            }
        }
    }
    for (size_t i = 0; i < rowCount; i++) {
        State::CastState(rowStates[i] + aggStateOffset)->SaveSortKey();
    }
}

template <type::DataTypeId COL2_ID>
void MinByComplexVarcharAggregator<COL2_ID>::AlignAggSchema(VectorBatch *result, VectorBatch *inputVecBatch)
{
    MinMaxByComplexAlignAggSchema<COL2_ID>(result, inputVecBatch, channels, inputRaw, targetColTypeId_,
        targetColDataType_);
}

template <type::DataTypeId COL2_ID>
void MinByComplexVarcharAggregator<COL2_ID>::AlignAggSchemaWithFilter(VectorBatch *result, VectorBatch *inputVecBatch,
    const int32_t filterIndex)
{
    MinMaxByComplexAlignAggSchemaWithFilter<COL2_ID>(result, inputVecBatch, channels, inputRaw, filterIndex,
        targetColTypeId_, targetColDataType_);
}

template <type::DataTypeId COL2_ID>
void MinByComplexVarcharAggregator<COL2_ID>::ProcessAlignAggSchema(VectorBatch *result, BaseVector *originVector,
    const std::shared_ptr<NullsHelper> nullMap, const bool aggFilter)
{
    (void)nullMap;
    (void)aggFilter;
    if (originVector == nullptr) {
        MinMaxByComplexAlignAppendEmptyPartial2(result, COL2_ID, targetColDataType_);
    }
}

template class MinByComplexVarcharAggregator<OMNI_VARCHAR>;
template class MinByComplexVarcharAggregator<OMNI_CHAR>;
template class MinByComplexVarcharAggregator<OMNI_VARBINARY>;

// Force GetSpillType() to be emitted (fixes GetSpillTypeEv undefined symbol when linking).
template std::vector<DataTypePtr> MinByComplexVarcharAggregator<OMNI_VARCHAR>::GetSpillType();
template std::vector<DataTypePtr> MinByComplexVarcharAggregator<OMNI_CHAR>::GetSpillType();
template std::vector<DataTypePtr> MinByComplexVarcharAggregator<OMNI_VARBINARY>::GetSpillType();

} // namespace op
} // namespace omniruntime
