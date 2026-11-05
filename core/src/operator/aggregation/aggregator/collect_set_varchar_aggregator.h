/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2026. All rights reserved.
 * Description: CollectSet aggregation for VARCHAR/CHAR/VARBINARY (state: DefaultHashMap<std::string, int8_t>).
 */

#ifndef OMNI_RUNTIME_COLLECT_SET_VARCHAR_AGGREGATOR_H
#define OMNI_RUNTIME_COLLECT_SET_VARCHAR_AGGREGATOR_H

#include <unordered_set>
#include <vector>
#include "typed_aggregator.h"
#include "operator/hashmap/base_hash_map.h"

namespace omniruntime::op {

/**
 * CollectSet for string/binary types. State holds DefaultHashMap<std::string, int8_t>.
 * Input: flat or dictionary Vector<LargeStringContainer<std::string_view>> (VARCHAR/CHAR/VARBINARY).
 * Output: ArrayVector with element vector LargeStringContainer<std::string_view>.
 */
class CollectSetVarcharAggregator : public TypedAggregator {
public:
    CollectSetVarcharAggregator(const DataTypes &inputTypes, const DataTypes &outputTypes,
        const std::vector<int32_t> &channels, bool inputRaw, bool outputPartial, bool isOverflowAsNull);
    // Extended constructor for derived aggregators (e.g. CountDistinctVarcharAggregator) that reuse
    // the set-based state machine but report a different FunctionType.
    CollectSetVarcharAggregator(const FunctionType aggregateType, const DataTypes &inputTypes,
        const DataTypes &outputTypes, const std::vector<int32_t> &channels, bool inputRaw,
        bool outputPartial, bool isOverflowAsNull);
    ~CollectSetVarcharAggregator() override;

    void ExtractValues(const AggregateState *state, std::vector<BaseVector *> &vectors, int32_t rowIndex) override;
    void ExtractValuesBatch(std::vector<AggregateState *> &groupStates, std::vector<BaseVector *> &vectors,
        int32_t rowOffset, int32_t rowCount) override;
    void ExtractValuesForSpill(std::vector<AggregateState *> &groupStates, std::vector<BaseVector *> &vectors) override;
    void InitState(AggregateState *state) override;
    void InitStates(std::vector<AggregateState *> &groupStates) override;
    void DestroyState(AggregateState *state);
    // Frees every DefaultHashMap ever allocated by this aggregator via its own
    // address book; used by the aggregation operator after a successful spill.
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
    /** Addresses of DefaultHashMap allocated in InitState; freed in DestroyState,
     * DestroyAllStates or in destructor. Hash-set for O(1) insert/erase. */
    std::unordered_set<int64_t> allocatedUniqueValuesAddrs_;
    type::DataTypeId elementTypeId_;  // OMNI_VARCHAR, OMNI_CHAR, or OMNI_VARBINARY for output element vector
};

}  // namespace omniruntime::op

#endif  // OMNI_RUNTIME_COLLECT_SET_VARCHAR_AGGREGATOR_H
