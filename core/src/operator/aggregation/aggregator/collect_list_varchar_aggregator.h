/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 * Description: CollectList aggregation for VARCHAR/CHAR/VARBINARY (state: std::vector<std::string>*).
 */

#ifndef OMNI_RUNTIME_COLLECT_LIST_VARCHAR_AGGREGATOR_H
#define OMNI_RUNTIME_COLLECT_LIST_VARCHAR_AGGREGATOR_H

#include <vector>
#include "typed_aggregator.h"

namespace omniruntime::op {

/**
 * CollectList for string/binary types. State holds std::vector<std::string>* (order and duplicates preserved).
 * Input: flat or dictionary Vector<LargeStringContainer<std::string_view>> (VARCHAR/CHAR/VARBINARY).
 * Output: ArrayVector with element vector LargeStringContainer<std::string_view>.
 */
class CollectListVarcharAggregator : public TypedAggregator {
public:
    CollectListVarcharAggregator(const DataTypes &inputTypes, const DataTypes &outputTypes,
        const std::vector<int32_t> &channels, bool inputRaw, bool outputPartial, bool isOverflowAsNull);
    ~CollectListVarcharAggregator() override;

    void ExtractValues(const AggregateState *state, std::vector<BaseVector *> &vectors, int32_t rowIndex) override;
    void ExtractValuesBatch(std::vector<AggregateState *> &groupStates, std::vector<BaseVector *> &vectors,
        int32_t rowOffset, int32_t rowCount) override;
    void ExtractValuesForSpill(std::vector<AggregateState *> &groupStates, std::vector<BaseVector *> &vectors) override;
    void InitState(AggregateState *state) override;
    void InitStates(std::vector<AggregateState *> &groupStates) override;
    void DestroyState(AggregateState *state);
    // Frees every std::vector<std::string> allocated by this aggregator; called
    // by the aggregation operator after a successful spill.
    void DestroyAllStates() override;
    std::vector<DataTypePtr> GetSpillType() override;
    size_t GetStateSize() override { return 8u; }

    void ProcessGroupUnspill(std::vector<UnspillRowInfo> &unspillRows, int32_t rowCount, int32_t &vectorIndex) override;
    void ProcessAlignAggSchema(VectorBatch *result, BaseVector *originVector,
        const std::shared_ptr<NullsHelper> nullMap, const bool aggFilter) override;

protected:
    void ProcessSingleInternal(AggregateState *state, BaseVector *vector, const int32_t rowOffset,
        const int32_t rowCount, const std::shared_ptr<NullsHelper> nullMap) override;
    void ProcessGroupInternal(std::vector<AggregateState *> &rowStates, BaseVector *vector, const int32_t rowOffset,
        const std::shared_ptr<NullsHelper> nullMap) override;

private:
    /** Addresses of std::vector<std::string> allocated in InitState; freed in
     * DestroyState, DestroyAllStates or in destructor. Hash-set: O(1) insert/erase
     * keeps state destruction linear even with tens of millions of groups. */
    std::unordered_set<int64_t> allocatedListAddrs_;
    type::DataTypeId elementTypeId_;  // OMNI_VARCHAR, OMNI_CHAR, or OMNI_VARBINARY for output element vector
};

}  // namespace omniruntime::op

#endif  // OMNI_RUNTIME_COLLECT_LIST_VARCHAR_AGGREGATOR_H
