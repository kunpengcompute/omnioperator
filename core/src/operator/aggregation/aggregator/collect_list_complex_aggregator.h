// Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
// Description: collect_list for complex types (ARRAY, MAP, ROW). Target column only; uses complex_aggregator_util
// (GetComplexColSlice / CopyComplexSliceToOwned / SetComplexColValue) like min_by complex.

#ifndef OMNI_RUNTIME_COLLECT_LIST_COMPLEX_AGGREGATOR_H
#define OMNI_RUNTIME_COLLECT_LIST_COMPLEX_AGGREGATOR_H

#include <unordered_set>
#include "typed_aggregator.h"
#include "complex_aggregator_util.h"
#include "vector/array_vector.h"
#include "vector/vector_helper.h"

namespace omniruntime {
namespace op {

// State: listAddr points to std::vector<BaseVector*> (owned complex slices); order preserved.
#pragma pack(push, 1)
struct ComplexListState {
    int64_t listAddr = 0;

    static const ComplexListState *ConstCastState(const AggregateState *state) {
        return reinterpret_cast<const ComplexListState *>(state);
    }
    static ComplexListState *CastState(AggregateState *state) {
        return reinterpret_cast<ComplexListState *>(state);
    }
};
#pragma pack(pop)

using ComplexListType = std::vector<vec::BaseVector *>;

class CollectListComplexAggregator : public TypedAggregator {
public:
    ~CollectListComplexAggregator() override;

    void ExtractValues(const AggregateState *state, std::vector<BaseVector *> &vectors, int32_t rowIndex) override;
    void ExtractValuesBatch(std::vector<AggregateState *> &groupStates, std::vector<BaseVector *> &vectors,
        int32_t rowOffset, int32_t rowCount) override;
    void ExtractValuesForSpill(std::vector<AggregateState *> &groupStates, std::vector<BaseVector *> &vectors) override;
    void InitState(AggregateState *state) override;
    void InitStates(std::vector<AggregateState *> &groupStates) override;
    void DestroyState(AggregateState *state) override;
    // Frees every ComplexListType allocated by this aggregator; called by the
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
    CollectListComplexAggregator(const DataTypes &inputTypes, const DataTypes &outputTypes,
        const std::vector<int32_t> &channels, bool inputRaw, bool outputPartial, bool isOverflowAsNull,
        type::DataTypeId targetColTypeId, type::DataTypePtr targetColDataType);

    void ProcessSingleInternal(AggregateState *state, BaseVector *vector, const int32_t rowOffset,
        const int32_t rowCount, const std::shared_ptr<NullsHelper> nullMap) override;
    void ProcessGroupInternal(std::vector<AggregateState *> &rowStates, BaseVector *vector, const int32_t rowOffset,
        const std::shared_ptr<NullsHelper> nullMap) override;

private:
    type::DataTypeId targetColTypeId_;
    type::DataTypePtr targetColDataType_;
    std::unordered_set<int64_t> allocatedListAddrs_;
};

}  // namespace op
}  // namespace omniruntime

#endif  // OMNI_RUNTIME_COLLECT_LIST_COMPLEX_AGGREGATOR_H
