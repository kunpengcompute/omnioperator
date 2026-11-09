/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Min aggregate for complex types (ARRAY, ROW).
 */

#include "min_complex_aggregator.h"
#include "vector/vector_helper.h"
#include "type/data_type.h"

namespace omniruntime {
namespace op {

using namespace vec;

MinComplexAggregator::~MinComplexAggregator() = default;

MinComplexAggregator::MinComplexAggregator(const DataTypes &inputTypes, const DataTypes &outputTypes,
    const std::vector<int32_t> &channels, bool inputRaw, bool outputPartial, bool isOverflowAsNull,
    type::DataTypeId targetColTypeId, type::DataTypePtr targetColDataType)
    : TypedAggregator(OMNI_AGGREGATION_TYPE_MIN, inputTypes, outputTypes, channels, inputRaw, outputPartial,
        isOverflowAsNull)
    , targetColTypeId_(targetColTypeId)
    , targetColDataType_(std::move(targetColDataType))
{}

void MinComplexAggregator::InitState(AggregateState *state)
{
    auto *complexState = ComplexState::CastState(state + aggStateOffset);
    complexState->isEmpty = true;
    complexState->currentValue = nullptr;
}

void MinComplexAggregator::InitStates(std::vector<AggregateState *> &groupStates)
{
    for (auto *groupState : groupStates) {
        InitState(groupState);
    }
}

std::vector<DataTypePtr> MinComplexAggregator::GetSpillType()
{
    std::vector<DataTypePtr> spillTypes;
    spillTypes.emplace_back(GetOutputTypes().GetType(0));
    return spillTypes;
}

void MinComplexAggregator::ProcessSingleInternal(AggregateState *state, BaseVector *vector,
    const int32_t rowOffset, const int32_t rowCount, const std::shared_ptr<NullsHelper> nullMap)
{
    (void)nullMap;
    (void)vector;
    BaseVector *colVector = curVectorBatch->Get(channels[0]);
    for (int32_t i = 0; i < rowCount; i++) {
        int32_t rowIndex = rowOffset + i;
        ComplexSliceHolder sliceHolder(GetComplexColSlice(colVector, targetColTypeId_, rowIndex));
        vec::BaseVector *slice = sliceHolder.get();
        if (ShouldSkipRowTargetNull(slice, targetColTypeId_, colVector, rowIndex)) {
            continue;
        }
        // state is already (base state + aggStateOffset) when called from TypedAggregator::ProcessGroup
        auto *complexState = ComplexState::CastState(state);
        if (complexState->isEmpty) {
            complexState->isEmpty = false;
            if (complexState->currentValue != nullptr) {
                ownedValues_.Release(complexState->currentValue);
            }
            complexState->currentValue = ownedValues_.Copy(slice, targetColTypeId_, targetColDataType_.get());
        } else {
            int cmp = CompareComplexSlice(slice, complexState->currentValue, targetColTypeId_, targetColDataType_.get());
            if (cmp < 0) {
                ownedValues_.Release(complexState->currentValue);
                complexState->currentValue = ownedValues_.Copy(slice, targetColTypeId_, targetColDataType_.get());
            }
        }
    }
}

void MinComplexAggregator::ProcessGroupInternal(std::vector<AggregateState *> &rowStates,
    BaseVector *vector, const int32_t rowOffset, const std::shared_ptr<NullsHelper> nullMap)
{
    (void)nullMap;
    (void)vector;
    BaseVector *colVector = curVectorBatch->Get(channels[0]);
    const size_t rowCount = rowStates.size();
    for (size_t i = 0; i < rowCount; i++) {
        int32_t rowIdx = static_cast<int32_t>(rowOffset + i);
        ComplexSliceHolder sliceHolder(GetComplexColSlice(colVector, targetColTypeId_, rowIdx));
        vec::BaseVector *slice = sliceHolder.get();
        if (ShouldSkipRowTargetNull(slice, targetColTypeId_, colVector, rowIdx)) {
            continue;
        }
        auto *complexState = ComplexState::CastState(rowStates[i] + aggStateOffset);
        if (complexState->isEmpty) {
            complexState->isEmpty = false;
            if (complexState->currentValue != nullptr) {
                ownedValues_.Release(complexState->currentValue);
            }
            complexState->currentValue = ownedValues_.Copy(slice, targetColTypeId_, targetColDataType_.get());
        } else {
            int cmp = CompareComplexSlice(slice, complexState->currentValue, targetColTypeId_, targetColDataType_.get());
            if (cmp < 0) {
                ownedValues_.Release(complexState->currentValue);
                complexState->currentValue = ownedValues_.Copy(slice, targetColTypeId_, targetColDataType_.get());
            }
        }
    }
}

void MinComplexAggregator::ExtractValues(const AggregateState *state,
    std::vector<BaseVector *> &vectors, int32_t rowIndex)
{
    const auto *complexState = ComplexState::ConstCastState(state + aggStateOffset);
    if (complexState->isEmpty) {
        vectors[0]->SetNull(rowIndex);
        return;
    }
    if (complexState->currentValue == nullptr) {
        vectors[0]->SetNull(rowIndex);
        return;
    }
    SetComplexColValue(vectors[0], targetColTypeId_, rowIndex, complexState->currentValue);
}

void MinComplexAggregator::ExtractValuesBatch(std::vector<AggregateState *> &groupStates,
    std::vector<BaseVector *> &vectors, int32_t rowOffset, int32_t rowCount)
{
    (void)rowOffset;
    for (int32_t i = 0; i < rowCount; i++) {
        const auto *complexState = ComplexState::ConstCastState(groupStates[i] + aggStateOffset);
        if (complexState->isEmpty) {
            vectors[0]->SetNull(i);
            continue;
        }
        if (complexState->currentValue == nullptr) {
            vectors[0]->SetNull(i);
            continue;
        }
        SetComplexColValue(vectors[0], targetColTypeId_, i, complexState->currentValue);
    }
}

void MinComplexAggregator::ExtractValuesForSpill(std::vector<AggregateState *> &groupStates,
    std::vector<BaseVector *> &vectors)
{
    int32_t rowCount = static_cast<int32_t>(groupStates.size());
    for (int32_t rowIndex = 0; rowIndex < rowCount; rowIndex++) {
        auto *complexState = ComplexState::CastState(groupStates[rowIndex] + aggStateOffset);
        if (complexState->isEmpty) {
            vectors[0]->SetNull(rowIndex);
            continue;
        }
        if (complexState->currentValue == nullptr) {
            vectors[0]->SetNull(rowIndex);
        } else {
            SetComplexColValue(vectors[0], targetColTypeId_, rowIndex, complexState->currentValue);
            // Spilled states are discarded (arena reset) after extraction.
            ownedValues_.Release(complexState->currentValue);
            complexState->currentValue = nullptr;
        }
    }
}

void MinComplexAggregator::ProcessGroupUnspill(std::vector<UnspillRowInfo> &unspillRows,
    int32_t rowCount, int32_t &vectorIndex)
{
    int32_t valueVecIdx = vectorIndex++;
    for (int32_t rowIdx = 0; rowIdx < rowCount; rowIdx++) {
        auto &row = unspillRows[rowIdx];
        VectorBatch *batch = row.batch;
        int32_t index = row.rowIdx;
        BaseVector *valueVector = batch->Get(valueVecIdx);
        auto *complexState = ComplexState::CastState(row.state + aggStateOffset);
        ComplexSliceHolder sliceHolder(GetComplexColSlice(valueVector, targetColTypeId_, index));
        vec::BaseVector *slice = sliceHolder.get();
        if (IsComplexSliceNull(slice, targetColTypeId_)) {
            continue;
        }
        if (complexState->isEmpty) {
            complexState->isEmpty = false;
            if (complexState->currentValue != nullptr) {
                ownedValues_.Release(complexState->currentValue);
            }
            complexState->currentValue = ownedValues_.Copy(slice, targetColTypeId_, targetColDataType_.get());
        } else {
            int cmp = CompareComplexSlice(slice, complexState->currentValue, targetColTypeId_, targetColDataType_.get());
            if (cmp < 0) {
                ownedValues_.Release(complexState->currentValue);
                complexState->currentValue = ownedValues_.Copy(slice, targetColTypeId_, targetColDataType_.get());
            }
        }
    }
}

void MinComplexAggregator::ProcessAlignAggSchema(VectorBatch *result, BaseVector *originVector,
    const std::shared_ptr<NullsHelper> nullMap, const bool aggFilter)
{
    int rowCount = originVector->GetSize();
    BaseVector *outVector = VectorHelper::CreateComplexVector(targetColDataType_.get(), rowCount);
    for (int index = 0; index < rowCount; index++) {
        if (nullMap != nullptr && (*nullMap)[index]) {
            outVector->SetNull(index);
            continue;
        }
        ComplexSliceHolder slice(GetComplexColSlice(originVector, targetColTypeId_, index));
        SetComplexColValue(outVector, targetColTypeId_, index, slice.get());
    }
    result->Append(outVector);
}

std::unique_ptr<Aggregator> MinComplexAggregator::Create(const DataTypes &inputTypes, const DataTypes &outputTypes,
    std::vector<int32_t> &channels, bool rawIn, bool partialOut, bool isOverflowAsNull, type::DataTypeId colTypeId)
{
    if (inputTypes.GetType(0)->GetId() != outputTypes.GetType(0)->GetId()) {
        throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", "Min complex: output col type not match input");
    }
    if (colTypeId != type::OMNI_ARRAY && colTypeId != type::OMNI_ROW) {
        throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR",
            "Min complex: only ARRAY and ROW supported, got " + std::to_string(static_cast<int>(colTypeId)));
    }
    return std::unique_ptr<Aggregator>(new MinComplexAggregator(inputTypes, outputTypes, channels,
        rawIn, partialOut, isOverflowAsNull, colTypeId, outputTypes.GetType(0)));
}

} // namespace op
} // namespace omniruntime
