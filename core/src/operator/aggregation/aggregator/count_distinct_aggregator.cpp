/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 * Description: CountDistinct aggregation (count_distinct_aggregator implementation).
 */

#include "count_distinct_aggregator.h"
#include "vector/vector_helper.h"

namespace omniruntime::op {

void CountDistinctVarcharAggregator::ExtractValues(const AggregateState *state,
    std::vector<BaseVector *> &vectors, int32_t rowIndex)
{
    if (IsOutputPartial()) {
        CollectSetVarcharAggregator::ExtractValues(state, vectors, rowIndex);
        return;
    }
    // Final/Complete stage: emit the set size as int64. The state struct is defined in
    // collect_set_varchar_aggregator.cpp; it holds a single int64_t address field, so the
    // reinterpret_cast below is safe and mirrors CollectSetVarcharState layout.
    const AggregateState *stateWithOffset = state + aggStateOffset;
    auto setStateAddr = *reinterpret_cast<const int64_t *>(stateWithOffset);
    auto *uniqueValues = reinterpret_cast<DefaultHashMap<std::string, int8_t> *>(setStateAddr);
    auto *v = static_cast<Vector<int64_t> *>(vectors[0]);
    v->SetValue(rowIndex, static_cast<int64_t>(uniqueValues->GetElementsSize()));
}

void CountDistinctVarcharAggregator::ExtractValuesBatch(std::vector<AggregateState *> &groupStates,
    std::vector<BaseVector *> &vectors, int32_t rowOffset, int32_t rowCount)
{
    if (IsOutputPartial()) {
        CollectSetVarcharAggregator::ExtractValuesBatch(groupStates, vectors, rowOffset, rowCount);
        return;
    }
    auto *v = static_cast<Vector<int64_t> *>(vectors[0]);
    for (int32_t rowIndex = 0; rowIndex < rowCount; rowIndex++) {
        const AggregateState *stateWithOffset = groupStates[rowIndex] + aggStateOffset;
        auto setStateAddr = *reinterpret_cast<const int64_t *>(stateWithOffset);
        auto *uniqueValues = reinterpret_cast<DefaultHashMap<std::string, int8_t> *>(setStateAddr);
        v->SetValue(rowOffset + rowIndex, static_cast<int64_t>(uniqueValues->GetElementsSize()));
    }
}

std::vector<DataTypePtr> CountDistinctVarcharAggregator::GetSpillType()
{
    std::vector<DataTypePtr> spillTypes;
    if (IsInputRaw()) {
        auto outType = GetOutputTypes().GetType(0);
        if (outType->GetId() == type::OMNI_ARRAY) {
            spillTypes.push_back(outType);
        } else {
            // Complete stage: raw element VARCHAR/CHAR/VARBINARY -> array<string>.
            spillTypes.push_back(std::make_shared<type::ArrayType>(GetInputTypes().GetType(0)));
        }
    } else {
        // Final/PartialMerge stage: input itself is array<string>.
        spillTypes.push_back(GetInputTypes().GetType(0));
    }
    return spillTypes;
}

void CountDistinctVarcharAggregator::ExtractValuesForSpill(std::vector<AggregateState *> &groupStates,
    std::vector<BaseVector *> &vectors)
{
    // Statically bind to the CollectSet array-emitting ExtractValuesBatch: the inherited
    // ExtractValuesForSpill would virtually dispatch to this class's Long-emitting override
    // and write an int64 into the array-typed spill vector.
    CollectSetVarcharAggregator::ExtractValuesBatch(groupStates, vectors, 0,
        static_cast<int32_t>(groupStates.size()));
}

template class CountDistinctAggregator<OMNI_BOOLEAN>;
template class CountDistinctAggregator<OMNI_BYTE>;
template class CountDistinctAggregator<OMNI_SHORT>;
template class CountDistinctAggregator<OMNI_INT>;
template class CountDistinctAggregator<OMNI_LONG>;
template class CountDistinctAggregator<OMNI_DATE32>;
template class CountDistinctAggregator<OMNI_DATE64>;
template class CountDistinctAggregator<OMNI_TIMESTAMP>;
template class CountDistinctAggregator<OMNI_FLOAT>;
template class CountDistinctAggregator<OMNI_DOUBLE>;
template class CountDistinctAggregator<OMNI_DECIMAL64>;
template class CountDistinctAggregator<OMNI_DECIMAL128>;

} // namespace omniruntime::op
