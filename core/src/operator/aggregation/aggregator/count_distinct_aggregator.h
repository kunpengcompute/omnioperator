/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 * Description: CountDistinct aggregation without Expand operator.
 * Reuses the CollectSet set-based state machine (DefaultHashMap<T, int8_t> as distinct set):
 *   - Partial stage:  raw T            -> intermediate array<T> (the distinct set)
 *   - Final stage:    array<T>         -> int64 distinct count
 *   - Complete stage:  raw T            -> int64 distinct count
 * Spill format is always array<T> (the set itself), identical to CollectSet.
 */

#ifndef OMNI_RUNTIME_COUNT_DISTINCT_AGGREGATOR_H
#define OMNI_RUNTIME_COUNT_DISTINCT_AGGREGATOR_H

#include "collect_set_aggregator.h"
#include "collect_set_varchar_aggregator.h"
#include "vector/vector_helper.h"

namespace omniruntime::op {

/**
 * CountDistinct for basic types (BOOLEAN/BYTE/SHORT/INT/LONG/DATE32/DATE64/TIMESTAMP/FLOAT/DOUBLE/
 * DECIMAL64/DECIMAL128). Derives from CollectSetAggregator<IN_ID, IN_ID> to reuse:
 *   InitState/InitStates/DestroyState, ProcessSingleInternal, ProcessGroupInternal,
 *   ProcessGroupUnspill, ProcessAlignAggSchema, ExtractValuesForSpill (array<T> spill format).
 * Only the final-result extraction differs: instead of materialising the set it emits its size.
 */
template<DataTypeId IN_ID>
class CountDistinctAggregator : public CollectSetAggregator<IN_ID, IN_ID> {
    using stateType = typename AggNativeAndVectorType<IN_ID>::type;

public:
    CountDistinctAggregator(const DataTypes &inputTypes, const DataTypes &outputTypes,
        const std::vector<int32_t> &channels, const bool inputRaw, const bool outputPartial,
        const bool isOverflowAsNull)
        : CollectSetAggregator<IN_ID, IN_ID>(OMNI_AGGREGATION_TYPE_COUNT_DISTINCT, inputTypes, outputTypes,
              channels, inputRaw, outputPartial, isOverflowAsNull)
    {
    }

    /** Partial stage: emit the distinct set as array<T> (intermediate, shuffle format). */
    void ExtractValues(const AggregateState *state, std::vector<BaseVector *> &vectors,
        int32_t rowIndex) override
    {
        if (this->IsOutputPartial()) {
            CollectSetAggregator<IN_ID, IN_ID>::ExtractValues(state, vectors, rowIndex);
            return;
        }
        const SetState<stateType> *setState = SetState<stateType>::ConstCastState(state + this->aggStateOffset);
        auto *uniqueValues =
            reinterpret_cast<DefaultHashMap<stateType, int8_t> *>(setState->uniqueValuesAddr);
        auto *v = static_cast<Vector<int64_t> *>(vectors[0]);
        v->SetValue(rowIndex, static_cast<int64_t>(uniqueValues->GetElementsSize()));
    }

    /** Final/Complete stage: emit one int64 distinct count per group. */
    void ExtractValuesBatch(std::vector<AggregateState *> &groupStates, std::vector<BaseVector *> &vectors,
        int32_t rowOffset, int32_t rowCount) override
    {
        if (this->IsOutputPartial()) {
            CollectSetAggregator<IN_ID, IN_ID>::ExtractValuesBatch(groupStates, vectors, rowOffset, rowCount);
            return;
        }
        auto *v = static_cast<Vector<int64_t> *>(vectors[0]);
        for (int32_t rowIndex = 0; rowIndex < rowCount; rowIndex++) {
            const SetState<stateType> *setState =
                SetState<stateType>::ConstCastState(groupStates[rowIndex] + this->aggStateOffset);
            auto *uniqueValues =
                reinterpret_cast<DefaultHashMap<stateType, int8_t> *>(setState->uniqueValuesAddr);
            v->SetValue(rowOffset + rowIndex, static_cast<int64_t>(uniqueValues->GetElementsSize()));
        }
    }

    /**
     * Spill format is always array<T> (the set itself): Partial stage output type is array<T>;
     * Final/PartialMerge stage input type is array<T>; Complete stage (raw in / long out) derives
     * array<T> from the raw element type.
     */
    std::vector<DataTypePtr> GetSpillType() override
    {
        std::vector<DataTypePtr> spillTypes;
        if (this->IsInputRaw()) {
            auto outType = this->GetOutputTypes().GetType(0);
            if (outType->GetId() == type::OMNI_ARRAY) {
                spillTypes.push_back(outType);
            } else {
                spillTypes.push_back(std::make_shared<type::ArrayType>(this->GetInputTypes().GetType(0)));
            }
        } else {
            spillTypes.push_back(this->GetInputTypes().GetType(0));
        }
        return spillTypes;
    }

    /**
     * Spill extraction must always emit array<T> (the set itself) to match GetSpillType().
     * The inherited CollectSetAggregator::ExtractValuesForSpill virtually dispatches to
     * ExtractValuesBatch, which this class overrides to emit int64 in final/complete mode;
     * that would write a Long into the array-typed spill vector (type confusion). Statically
     * bind to the CollectSet set-materialising ExtractValuesBatch instead.
     */
    void ExtractValuesForSpill(std::vector<AggregateState *> &groupStates,
        std::vector<BaseVector *> &vectors) override
    {
        CollectSetAggregator<IN_ID, IN_ID>::ExtractValuesBatch(groupStates, vectors, 0,
            static_cast<int32_t>(groupStates.size()));
    }
};

/**
 * CountDistinct for VARCHAR/CHAR/VARBINARY. Same reuse strategy as the templated variant, deriving
 * from CollectSetVarcharAggregator (state: DefaultHashMap<std::string, int8_t>).
 */
class CountDistinctVarcharAggregator : public CollectSetVarcharAggregator {
public:
    CountDistinctVarcharAggregator(const DataTypes &inputTypes, const DataTypes &outputTypes,
        const std::vector<int32_t> &channels, const bool inputRaw, const bool outputPartial,
        const bool isOverflowAsNull)
        : CollectSetVarcharAggregator(OMNI_AGGREGATION_TYPE_COUNT_DISTINCT, inputTypes, outputTypes, channels,
              inputRaw, outputPartial, isOverflowAsNull)
    {
    }

    void ExtractValues(const AggregateState *state, std::vector<BaseVector *> &vectors,
        int32_t rowIndex) override;

    void ExtractValuesBatch(std::vector<AggregateState *> &groupStates, std::vector<BaseVector *> &vectors,
        int32_t rowOffset, int32_t rowCount) override;

    std::vector<DataTypePtr> GetSpillType() override;

    /** Spill extraction always emits array<string>; see the templated variant for rationale. */
    void ExtractValuesForSpill(std::vector<AggregateState *> &groupStates,
        std::vector<BaseVector *> &vectors) override;
};

} // namespace omniruntime::op

#endif // OMNI_RUNTIME_COUNT_DISTINCT_AGGREGATOR_H
