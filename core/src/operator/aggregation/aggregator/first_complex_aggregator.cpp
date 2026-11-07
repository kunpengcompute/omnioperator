/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2026. All rights reserved.
 * Description: first_value for complex types (array/map/row).
 */

#include "first_complex_aggregator.h"
#include "type/data_type.h"

namespace omniruntime {
namespace op {

FirstComplexAggregator::FirstComplexAggregator(FunctionType aggregateType, const type::DataTypes &in,
    const type::DataTypes &out, std::vector<int32_t> &channels, bool inputRaw, bool outputPartial, bool isOverflowAsNull)
    : Aggregator(aggregateType, in, out, channels, inputRaw, outputPartial, isOverflowAsNull)
    , colTypeId_(in.GetType(0)->GetId())
    , colDataType_(in.GetType(0).get())
    , ignoreNull_(aggregateType == OMNI_AGGREGATION_TYPE_FIRST_IGNORENULL)
{
}

void FirstComplexAggregator::InitState(AggregateState *state)
{
    auto *s = FirstComplexState::CastState(state + aggStateOffset);
    s->storedValue = nullptr;
    s->valueSet = false;
    s->valIsNull = true;
}

void FirstComplexAggregator::InitStates(std::vector<AggregateState *> &groupStates)
{
    for (auto *groupState : groupStates) {
        InitState(groupState);
    }
}

void FirstComplexAggregator::DestroyState(AggregateState *state)
{
    auto *s = FirstComplexState::CastState(state + aggStateOffset);
    if (s->storedValue != nullptr) {
        ownedValues_.Release(s->storedValue);
        s->storedValue = nullptr;
    }
}

size_t FirstComplexAggregator::GetStateSize()
{
    return sizeof(FirstComplexState);
}

std::vector<type::DataTypePtr> FirstComplexAggregator::GetSpillType()
{
    std::vector<type::DataTypePtr> spillTypes;
    spillTypes.emplace_back(inputTypes.GetType(0));
    if (outputPartial) {
        spillTypes.emplace_back(std::make_shared<type::DataType>(type::OMNI_BOOLEAN));
    }
    return spillTypes;
}

void FirstComplexAggregator::ProcessGroup(AggregateState *state, VectorBatch *vectorBatch, int32_t rowIndex)
{
    // Base class already passes rowStates[i] + aggStateOffset, so state points to this aggregator's state.
    auto *firstState = FirstComplexState::CastState(state);
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
    if (!firstState->valueSet) {
        firstState->valueSet = true;
        firstState->valIsNull = IsComplexSliceNull(slice, colTypeId_);
        if (!firstState->valIsNull && slice != nullptr) {
            if (firstState->storedValue != nullptr) {
                ownedValues_.Release(firstState->storedValue);
            }
            firstState->storedValue = ownedValues_.Copy(slice, colTypeId_, colDataType_);
        }
    }
}

void FirstComplexAggregator::ExtractValues(const AggregateState *state, std::vector<vec::BaseVector *> &vectors,
    const int32_t rowIndex)
{
    if (vectors.empty() || vectors[0] == nullptr) {
        return;
    }
    const auto *firstState = FirstComplexState::ConstCastState(state + aggStateOffset);
    if (outputPartial) {
        if (firstState->valIsNull) {
            vectors[0]->SetNull(rowIndex);
        } else if (firstState->storedValue != nullptr) {
            SetComplexColValue(vectors[0], colTypeId_, rowIndex, firstState->storedValue);
        } else {
            vectors[0]->SetNull(rowIndex);
        }
        if (outputPartial && vectors.size() > 1 && vectors[1] != nullptr) {
            reinterpret_cast<vec::Vector<bool> *>(vectors[1])->SetValue(rowIndex, firstState->valueSet);
        }
    } else {
        if (!firstState->valueSet || firstState->valIsNull || firstState->storedValue == nullptr) {
            vectors[0]->SetNull(rowIndex);
            return;
        }
        SetComplexColValue(vectors[0], colTypeId_, rowIndex, firstState->storedValue);
    }
}

void FirstComplexAggregator::ExtractValuesBatch(std::vector<AggregateState *> &groupStates,
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
        auto *firstState = FirstComplexState::CastState(groupStates[i] + aggStateOffset);
        if (outputPartial) {
            if (firstState->valIsNull) {
                vectors[0]->SetNull(i);
            } else if (firstState->storedValue != nullptr) {
                SetComplexColValue(vectors[0], colTypeId_, i, firstState->storedValue);
            } else {
                vectors[0]->SetNull(i);
            }
            if (valueSetVector != nullptr) {
                valueSetVector->SetValue(i, firstState->valueSet);
            }
        } else {
            if (!firstState->valueSet || firstState->valIsNull || firstState->storedValue == nullptr) {
                vectors[0]->SetNull(i);
            } else {
                SetComplexColValue(vectors[0], colTypeId_, i, firstState->storedValue);
            }
        }
    }
}

void FirstComplexAggregator::ExtractValuesForSpill(std::vector<AggregateState *> &groupStates,
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
        auto *firstState = FirstComplexState::CastState(groupStates[rowIndex] + aggStateOffset);
        if (firstState->valIsNull || firstState->storedValue == nullptr) {
            vectors[0]->SetNull(rowIndex);
        } else {
            SetComplexColValue(vectors[0], colTypeId_, rowIndex, firstState->storedValue);
        }
        // Spilled states are discarded (arena reset) after extraction.
        ownedValues_.Release(firstState->storedValue);
        firstState->storedValue = nullptr;
        if (valueSetVector != nullptr) {
            valueSetVector->SetValue(rowIndex, firstState->valueSet);
        }
    }
}

void FirstComplexAggregator::ProcessGroupUnspill(std::vector<UnspillRowInfo> &unspillRows, int32_t rowCount,
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
        auto *firstState = FirstComplexState::CastState(row.state + aggStateOffset);
        if (!firstState->valueSet) {
            ComplexSliceHolder sliceHolder(GetComplexColSlice(colVector, colTypeId_, row.rowIdx));
            vec::BaseVector *slice = sliceHolder.get();
            if (ignoreNull_ && IsComplexSliceNull(slice, colTypeId_)) {
                continue;
            }
            firstState->valueSet = true;
            firstState->valIsNull = IsComplexSliceNull(slice, colTypeId_);
            if (!firstState->valIsNull && slice != nullptr) {
                firstState->storedValue = ownedValues_.Copy(slice, colTypeId_, colDataType_);
            }
        }
        if (outputPartial && valueSetVecIdx >= 0) {
            bool intermediate = reinterpret_cast<vec::Vector<bool> *>(row.batch->Get(valueSetVecIdx))->GetValue(row.rowIdx);
            firstState->valueSet = firstState->valueSet || intermediate;
        }
    }
}

void FirstComplexAggregator::AlignAggSchemaWithFilter(VectorBatch *result, VectorBatch *inputVecBatch,
    const int32_t filterIndex)
{
    (void)result;
    (void)inputVecBatch;
    (void)filterIndex;
    throw omniruntime::exception::OmniException("OPERATOR_RUNTIME_ERROR",
        "AlignAggSchemaWithFilter not supported for first complex aggregator");
}

void FirstComplexAggregator::AlignAggSchema(VectorBatch *result, VectorBatch *inputVecBatch)
{
    (void)result;
    (void)inputVecBatch;
    throw omniruntime::exception::OmniException("OPERATOR_RUNTIME_ERROR",
        "AlignAggSchema not supported for first complex aggregator");
}

} // namespace op
} // namespace omniruntime
