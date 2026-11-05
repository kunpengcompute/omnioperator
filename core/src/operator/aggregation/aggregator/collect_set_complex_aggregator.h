// Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
// Description: collect_set for complex types (ARRAY, ROW only; Map not supported).
// Uses std::set with ComplexSliceSetComparator for deduplication (avoids Omni defaultHashMap for array/struct).

#ifndef OMNI_RUNTIME_COLLECT_SET_COMPLEX_AGGREGATOR_H
#define OMNI_RUNTIME_COLLECT_SET_COMPLEX_AGGREGATOR_H

#include <unordered_set>
#include "typed_aggregator.h"
#include "complex_aggregator_util.h"
#include "vector/array_vector.h"
#include "vector/vector_helper.h"

namespace omniruntime {
namespace op {

// Use std::set for deduplication (avoids Omni defaultHashMap limitations for array/struct).
#pragma pack(push, 1)
struct ComplexSetState {
    int64_t setAddr = 0;

    static const ComplexSetState *ConstCastState(const AggregateState *state) {
        return reinterpret_cast<const ComplexSetState *>(state);
    }
    static ComplexSetState *CastState(AggregateState *state) {
        return reinterpret_cast<ComplexSetState *>(state);
    }
};
#pragma pack(pop)

class CollectSetComplexAggregator : public TypedAggregator {
public:
    ~CollectSetComplexAggregator() override;

    void ExtractValues(const AggregateState *state, std::vector<BaseVector *> &vectors, int32_t rowIndex) override;
    void ExtractValuesBatch(std::vector<AggregateState *> &groupStates, std::vector<BaseVector *> &vectors,
        int32_t rowOffset, int32_t rowCount) override;
    void ExtractValuesForSpill(std::vector<AggregateState *> &groupStates, std::vector<BaseVector *> &vectors) override;
    void InitState(AggregateState *state) override;
    void InitStates(std::vector<AggregateState *> &groupStates) override;
    void DestroyState(AggregateState *state) override;
    // Frees every ComplexSetType allocated by this aggregator; called by the
    // aggregation operator after a successful spill.
    void DestroyAllStates() override;
    std::vector<DataTypePtr> GetSpillType() override;
    size_t GetStateSize() override { return 8u; }
    void ProcessGroupUnspill(std::vector<UnspillRowInfo> &unspillRows, int32_t rowCount, int32_t &vectorIndex) override;
    void ProcessAlignAggSchema(VectorBatch *result, BaseVector *originVector,
        const std::shared_ptr<NullsHelper> nullMap, const bool aggFilter) override;

    static std::unique_ptr<Aggregator> Create(const DataTypes &inputTypes, const DataTypes &outputTypes,
        std::vector<int32_t> &channels, bool rawIn, bool partialOut, bool isOverflowAsNull,
        type::DataTypeId targetColTypeId);

protected:
    CollectSetComplexAggregator(const DataTypes &inputTypes, const DataTypes &outputTypes,
        const std::vector<int32_t> &channels, bool inputRaw, bool outputPartial, bool isOverflowAsNull,
        type::DataTypeId targetColTypeId, type::DataTypePtr targetColDataType);

    void ProcessSingleInternal(AggregateState *state, BaseVector *vector, const int32_t rowOffset,
        const int32_t rowCount, const std::shared_ptr<NullsHelper> nullMap) override;
    void ProcessGroupInternal(std::vector<AggregateState *> &rowStates, BaseVector *vector, const int32_t rowOffset,
        const std::shared_ptr<NullsHelper> nullMap) override;

private:
    type::DataTypeId targetColTypeId_;
    type::DataTypePtr targetColDataType_;
    std::unordered_set<int64_t> allocatedSetAddrs_;
};

}  // namespace op
}  // namespace omniruntime

#endif  // OMNI_RUNTIME_COLLECT_SET_COMPLEX_AGGREGATOR_H
