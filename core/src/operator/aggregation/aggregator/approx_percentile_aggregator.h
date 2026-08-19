/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: ApproxPercentile aggregate (KLL sketch). Reference: approx_count_distinct (HyperLogLog).
 *
 * Four-phase semantics (selected by inputRaw/outputPartial):
 * - Partial:      inputRaw=true,  outputPartial=true;  input is (value [, weight], percentile [, accuracy]), output is
 *   serialized KLL sketch (VARBINARY), one aggregate.
 * - PartialMerge: inputRaw=false, outputPartial=true;  input is VARBINARY partial(s), output is VARBINARY (merged sketch).
 *   Used as the shuffle-merge stage between Partial and Final for grouped PERCENTILE_APPROX.
 * - Final:        inputRaw=false, outputPartial=false; input is VARBINARY partial, output is scalar or ARRAY of
 *   percentile values (same type as value column), another aggregate.
 * - Complete:     inputRaw=true,  outputPartial=false; input is raw values, output is scalar/ARRAY (single-stage, no
 *   shuffle). Output type equals value type.
 *
 * State: one ApproxPercentileAggState per group (accumulator pointer); serialization only on ExtractValues/Spill.
 */
#ifndef OMNI_RUNTIME_APPROX_PERCENTILE_AGGREGATOR_H
#define OMNI_RUNTIME_APPROX_PERCENTILE_AGGREGATOR_H

#include "typed_aggregator.h"
#include "codegen/KllSketch.h"
#include "operator/aggregation/vector_getter.h"
#include "operator/hash_util.h"
#include "vector/vector_helper.h"
#include "type/data_type.h"

namespace omniruntime {
namespace op {

/**
 * State for one group: pointer to ApproxPercentileAccumulator (KLL sketch + percentiles + accuracy).
 * Serialized only when outputting partial (VARBINARY) or spilling.
 */
#pragma pack(push, 1)
struct ApproxPercentileAggState {
    int64_t accumulatorPtr;

    static const ApproxPercentileAggState* ConstCastState(const AggregateState* state) {
        return reinterpret_cast<const ApproxPercentileAggState*>(state);
    }
    static ApproxPercentileAggState* CastState(AggregateState* state) {
        return reinterpret_cast<ApproxPercentileAggState*>(state);
    }
};
#pragma pack(pop)

template <DataTypeId IN_ID, DataTypeId OUT_ID>
class ApproxPercentileAggregator : public TypedAggregator {
    using InType = typename AggNativeAndVectorType<IN_ID>::type;
    using OutType = typename AggNativeAndVectorType<OUT_ID>::type;

public:
    ~ApproxPercentileAggregator() override;

    void ExtractValues(const AggregateState* state, std::vector<BaseVector*>& vectors, int32_t rowIndex) override;
    void ExtractValuesBatch(std::vector<AggregateState*>& groupStates, std::vector<BaseVector*>& vectors,
        int32_t rowOffset, int32_t rowCount) override;
    void ExtractValuesForSpill(std::vector<AggregateState*>& groupStates, std::vector<BaseVector*>& vectors) override;

    std::vector<DataTypePtr> GetSpillType() override;
    size_t GetStateSize() override { return sizeof(ApproxPercentileAggState); }

    void InitState(AggregateState* state) override;
    void InitStates(std::vector<AggregateState*>& groupStates) override;

    static std::unique_ptr<Aggregator> Create(const DataTypes& inputTypes, const DataTypes& outputTypes,
        std::vector<int32_t>& channels, bool rawIn, bool partialOut, bool isOverflowAsNull);

    void ProcessGroupUnspill(std::vector<UnspillRowInfo>& unspillRows, int32_t rowCount, int32_t& vectorIndex) override;
    void ProcessAlignAggSchema(VectorBatch* result, BaseVector* originVector,
        const std::shared_ptr<NullsHelper> nullMap, const bool aggFilter) override;

protected:
    ApproxPercentileAggregator(const DataTypes& inputTypes, const DataTypes& outputTypes,
        std::vector<int32_t>& channels, const bool inputRaw, const bool outputPartial, const bool isOverflowAsNull);

    void ProcessSingleInternal(AggregateState* state, BaseVector* vector, const int32_t rowOffset,
        const int32_t rowCount, const std::shared_ptr<NullsHelper> nullMap) override;

    void ProcessGroupInternal(std::vector<AggregateState*>& rowStates, BaseVector* vector, const int32_t rowOffset,
        const std::shared_ptr<NullsHelper> nullMap) override;

private:
    void ProcessPartialRaw(AggregateState* state, BaseVector* vector, const int32_t rowOffset,
        const int32_t rowCount, const std::shared_ptr<NullsHelper> nullMap);
    void ProcessFinalMerge(AggregateState* state, BaseVector* vector, const int32_t rowOffset, const int32_t rowCount,
        const std::shared_ptr<NullsHelper> nullMap);

    std::vector<int64_t> stateAccumulatorPtrs_;  // For destructor to free accumulators
    int32_t percentileChannel_;   // Column index of percentile (DOUBLE or ARRAY<DOUBLE>) in curVectorBatch
    int32_t weightChannel_;       // -1 if absent; column index of weight (LONG) when 3/4 columns
    int32_t accuracyChannel_;    // -1 if absent; column index of accuracy (INT or LONG) when 3/4 columns
};

}  // namespace op
}  // namespace omniruntime

#endif  // OMNI_RUNTIME_APPROX_PERCENTILE_AGGREGATOR_H
