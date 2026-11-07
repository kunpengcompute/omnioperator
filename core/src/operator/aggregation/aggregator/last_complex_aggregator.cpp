/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2026. All rights reserved.
 * Description: last_value for complex types (array/map/row).
 */

#include "last_complex_aggregator.h"
#include "type/data_type.h"

namespace omniruntime {
namespace op {

LastComplexAggregator::LastComplexAggregator(FunctionType aggregateType, const type::DataTypes &in,
    const type::DataTypes &out, std::vector<int32_t> &channels, bool inputRaw, bool outputPartial, bool isOverflowAsNull)
    : Aggregator(aggregateType, in, out, channels, inputRaw, outputPartial, isOverflowAsNull)
    , colTypeId_(in.GetType(0)->GetId())
    , colDataType_(in.GetType(0).get())
    , ignoreNull_(aggregateType == OMNI_AGGREGATION_TYPE_LAST_IGNORENULL)
{
}

void LastComplexAggregator::InitState(AggregateState *state)
{
    auto *s = LastComplexState::CastState(state + aggStateOffset);
    s->storedValue = nullptr;
    s->valueSet = false;
    s->valIsNull = true;
}

void LastComplexAggregator::InitStates(std::vector<AggregateState *> &groupStates)
{
    for (auto *groupState : groupStates) {
        InitState(groupState);
    }
}

void LastComplexAggregator::DestroyState(AggregateState *state)
{
    auto *s = LastComplexState::CastState(state + aggStateOffset);
    if (s->storedValue != nullptr) {
        ownedValues_.Release(s->storedValue);
        s->storedValue = nullptr;
    }
}

size_t LastComplexAggregator::GetStateSize()
{
    return sizeof(LastComplexState);
}

std::vector<type::DataTypePtr> LastComplexAggregator::GetSpillType()
{
    std::vector<type::DataTypePtr> spillTypes;
    spillTypes.emplace_back(inputTypes.GetType(0));
    if (outputPartial) {
        spillTypes.emplace_back(std::make_shared<type::DataType>(type::OMNI_BOOLEAN));
    }
    return spillTypes;
}

void LastComplexAggregator::ProcessGroup(AggregateState *state, VectorBatch *vectorBatch, int32_t rowIndex)
{
    // Base class already passes rowStates[i] + aggStateOffset, so state points to this aggregator's state.
    auto *lastState = LastComplexState::CastState(state);
    if (channels.empty()) {
        return;
    }
    vec::BaseVector *colVector = vectorBatch->Get(channels[0]);
    if (colVector == nullptr) {
        return;
    }
    ComplexSliceHolder sliceHolder(GetComplexColSlice(colVector, colTypeId_, rowIndex));
    vec::BaseVector *slice = sliceHolder.get();

    if (ignoreNull_ && IsComplexSliceNull(slice, colTypeId_)) {
        return;
    }
    lastState->valueSet = true;
    lastState->valIsNull = IsComplexSliceNull(slice, colTypeId_);
    if (lastState->storedValue != nullptr) {
        ownedValues_.Release(lastState->storedValue);
        lastState->storedValue = nullptr;
    }
    if (!lastState->valIsNull && slice != nullptr) {
        lastState->storedValue = ownedValues_.Copy(slice, colTypeId_, colDataType_);
    }
}

void LastComplexAggregator::ExtractValues(const AggregateState *state, std::vector<vec::BaseVector *> &vectors,
    const int32_t rowIndex)
{
    if (vectors.empty() || vectors[0] == nullptr) {
        return;
    }
    const auto *lastState = LastComplexState::ConstCastState(state + aggStateOffset);
    if (outputPartial) {
        if (lastState->valIsNull) {
            vectors[0]->SetNull(rowIndex);
        } else if (lastState->storedValue != nullptr) {
            SetComplexColValue(vectors[0], colTypeId_, rowIndex, lastState->storedValue);
        } else {
            vectors[0]->SetNull(rowIndex);
        }
        if (outputPartial && vectors.size() > 1 && vectors[1] != nullptr) {
            reinterpret_cast<vec::Vector<bool> *>(vectors[1])->SetValue(rowIndex, lastState->valueSet);
        }
    } else {
        if (!lastState->valueSet || lastState->valIsNull || lastState->storedValue == nullptr) {
            vectors[0]->SetNull(rowIndex);
            return;
        }
        SetComplexColValue(vectors[0], colTypeId_, rowIndex, lastState->storedValue);
    }
}

void LastComplexAggregator::ExtractValuesBatch(std::vector<AggregateState *> &groupStates,
    std::vector<vec::BaseVector *> &vectors, int32_t rowOffset, int32_t rowCount)
{
    (void)rowOffset;
    if (vectors.empty() || vectors[0] == nullptr) {
        return;
    }
    vec::Vector<bool> *valueSetVector = nullptr;
    if (outputPartial && vectors.size() > 1 && vectors[1] != nullptr) {
        valueSetVector = reinterpret_cast<vec::Vector<bool> *>(vectors[1]);
    }
    for (int32_t i = 0; i < rowCount; i++) {
        auto *lastState = LastComplexState::CastState(groupStates[i] + aggStateOffset);
        if (outputPartial) {
            if (lastState->valIsNull) {
                vectors[0]->SetNull(i);
            } else if (lastState->storedValue != nullptr) {
                SetComplexColValue(vectors[0], colTypeId_, i, lastState->storedValue);
            } else {
                vectors[0]->SetNull(i);
            }
            if (valueSetVector != nullptr) {
                valueSetVector->SetValue(i, lastState->valueSet);
            }
        } else {
            if (!lastState->valueSet || lastState->valIsNull || lastState->storedValue == nullptr) {
                vectors[0]->SetNull(i);
            } else {
                SetComplexColValue(vectors[0], colTypeId_, i, lastState->storedValue);
            }
        }
    }
}

void LastComplexAggregator::ExtractValuesForSpill(std::vector<AggregateState *> &groupStates,
    std::vector<vec::BaseVector *> &vectors)
{
    if (vectors.empty() || vectors[0] == nullptr) {
        return;
    }
    int32_t rowCount = static_cast<int32_t>(groupStates.size());
    vec::Vector<bool> *valueSetVector = nullptr;
    if (outputPartial && vectors.size() > 1 && vectors[1] != nullptr) {
        valueSetVector = reinterpret_cast<vec::Vector<bool> *>(vectors[1]);
    }
    for (int32_t rowIndex = 0; rowIndex < rowCount; rowIndex++) {
        auto *lastState = LastComplexState::CastState(groupStates[rowIndex] + aggStateOffset);
        if (lastState->valIsNull || lastState->storedValue == nullptr) {
            vectors[0]->SetNull(rowIndex);
        } else {
            SetComplexColValue(vectors[0], colTypeId_, rowIndex, lastState->storedValue);
        }
        // Spilled states are discarded (arena reset) after extraction.
        ownedValues_.Release(lastState->storedValue);
        lastState->storedValue = nullptr;
        if (valueSetVector != nullptr) {
            valueSetVector->SetValue(rowIndex, lastState->valueSet);
        }
    }
}

void LastComplexAggregator::ProcessGroupUnspill(std::vector<UnspillRowInfo> &unspillRows, int32_t rowCount,
    int32_t &vectorIndex)
{
    int32_t colVecIdx = vectorIndex++;
    int32_t valueSetVecIdx = outputPartial ? vectorIndex++ : -1;
    for (int32_t rowIdx = 0; rowIdx < rowCount; rowIdx++) {
        auto &row = unspillRows[rowIdx];
        if (row.batch == nullptr) {
            continue;
        }
        vec::BaseVector *colVector = row.batch->Get(colVecIdx);
        if (colVector == nullptr) {
            continue;
        }
        auto *lastState = LastComplexState::CastState(row.state + aggStateOffset);
        ComplexSliceHolder sliceHolder(GetComplexColSlice(colVector, colTypeId_, row.rowIdx));
        vec::BaseVector *slice = sliceHolder.get();
        if (ignoreNull_ && IsComplexSliceNull(slice, colTypeId_)) {
            continue;
        }
        lastState->valueSet = true;
        lastState->valIsNull = IsComplexSliceNull(slice, colTypeId_);
        if (lastState->storedValue != nullptr) {
            ownedValues_.Release(lastState->storedValue);
            lastState->storedValue = nullptr;
        }
        if (!lastState->valIsNull && slice != nullptr) {
            lastState->storedValue = ownedValues_.Copy(slice, colTypeId_, colDataType_);
        }
        if (outputPartial && valueSetVecIdx >= 0) {
            bool intermediate = reinterpret_cast<vec::Vector<bool> *>(row.batch->Get(valueSetVecIdx))->GetValue(row.rowIdx);
            lastState->valueSet = lastState->valueSet || intermediate;
        }
    }
}

void LastComplexAggregator::AlignAggSchemaWithFilter(VectorBatch *result, VectorBatch *inputVecBatch,
    const int32_t filterIndex)
{
    (void)result;
    (void)inputVecBatch;
    (void)filterIndex;
    throw omniruntime::exception::OmniException("OPERATOR_RUNTIME_ERROR",
        "AlignAggSchemaWithFilter not supported for last complex aggregator");
}

void LastComplexAggregator::AlignAggSchema(VectorBatch *result, VectorBatch *inputVecBatch)
{
    (void)result;
    (void)inputVecBatch;
    throw omniruntime::exception::OmniException("OPERATOR_RUNTIME_ERROR",
        "AlignAggSchema not supported for last complex aggregator");
}

} // namespace op
} // namespace omniruntime
