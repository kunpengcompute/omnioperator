/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022-2026. All rights reserved.
 * Description: first_value for OMNI_ARRAY, OMNI_MAP, OMNI_ROW. Uses complex_aggregator_util
 *              (GetComplexColSlice/CopyComplexSliceToOwned/SetComplexColValue) like MaxByComplexAggregator.
 */
#ifndef OMNI_RUNTIME_FIRST_COMPLEX_AGGREGATOR_H
#define OMNI_RUNTIME_FIRST_COMPLEX_AGGREGATOR_H

#include "aggregator.h"
#include "complex_aggregator_util.h"

namespace omniruntime {
namespace op {

class FirstComplexAggregator : public Aggregator {
#pragma pack(push, 1)
    struct FirstComplexState {
        vec::BaseVector *storedValue = nullptr;
        bool valueSet = false;
        bool valIsNull = true;

        static const FirstComplexState *ConstCastState(const AggregateState *state)
        {
            return reinterpret_cast<const FirstComplexState *>(state);
        }
        static FirstComplexState *CastState(AggregateState *state)
        {
            return reinterpret_cast<FirstComplexState *>(state);
        }
    };
#pragma pack(pop)

public:
    FirstComplexAggregator(FunctionType aggregateType, const type::DataTypes &in, const type::DataTypes &out,
        std::vector<int32_t> &channels, bool inputRaw, bool outputPartial, bool isOverflowAsNull);

    ~FirstComplexAggregator() override = default;

    void ProcessGroup(AggregateState *state, VectorBatch *vectorBatch, int32_t rowIndex) override;
    void ProcessGroupUnspill(std::vector<UnspillRowInfo> &unspillRows, int32_t rowCount, int32_t &vectorIndex) override;
    void ExtractValues(const AggregateState *state, std::vector<vec::BaseVector *> &vectors, const int32_t rowIndex) override;
    void ExtractValuesBatch(std::vector<AggregateState *> &groupStates, std::vector<vec::BaseVector *> &vectors,
        int32_t rowOffset, int32_t rowCount) override;
    void ExtractValuesForSpill(std::vector<AggregateState *> &groupStates, std::vector<vec::BaseVector *> &vectors) override;
    void InitState(AggregateState *state) override;
    void InitStates(std::vector<AggregateState *> &groupStates) override;
    void DestroyState(AggregateState *state) override;
    size_t GetStateSize() override;
    std::vector<type::DataTypePtr> GetSpillType() override;
    void AlignAggSchemaWithFilter(VectorBatch *result, VectorBatch *inputVecBatch, const int32_t filterIndex) override;
    void AlignAggSchema(VectorBatch *result, VectorBatch *inputVecBatch) override;

private:
    type::DataTypeId colTypeId_;
    type::DataType *colDataType_;
    bool ignoreNull_;
    ComplexValueOwner ownedValues_;
};

} // namespace op
} // namespace omniruntime

#endif // OMNI_RUNTIME_FIRST_COMPLEX_AGGREGATOR_H
