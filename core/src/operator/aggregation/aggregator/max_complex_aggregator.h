/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Max aggregate for OMNI_ARRAY and OMNI_ROW (single column; comparison via CompareComplexSlice).
 */
#ifndef OMNI_RUNTIME_MAX_COMPLEX_AGGREGATOR_H
#define OMNI_RUNTIME_MAX_COMPLEX_AGGREGATOR_H

#include "typed_aggregator.h"
#include "complex_aggregator_util.h"
#include "operator/aggregation/vector_getter.h"

namespace omniruntime {
namespace op {

class MaxComplexAggregator : public TypedAggregator {
#pragma pack(push, 1)
    struct ComplexState {
        vec::BaseVector *currentValue = nullptr;
        bool isEmpty = true;

        static const ComplexState *ConstCastState(const AggregateState *state)
        {
            return reinterpret_cast<const ComplexState *>(state);
        }
        static ComplexState *CastState(AggregateState *state)
        {
            return reinterpret_cast<ComplexState *>(state);
        }
    };
#pragma pack(pop)

public:
    ~MaxComplexAggregator() override;

    void ExtractValues(const AggregateState *state, std::vector<BaseVector *> &vectors, int32_t rowIndex) override;
    void ExtractValuesBatch(std::vector<AggregateState *> &groupStates, std::vector<BaseVector *> &vectors,
        int32_t rowOffset, int32_t rowCount) override;
    void ExtractValuesForSpill(std::vector<AggregateState *> &groupStates, std::vector<BaseVector *> &vectors) override;
    void InitState(AggregateState *state) override;
    void InitStates(std::vector<AggregateState *> &groupStates) override;
    std::vector<DataTypePtr> GetSpillType() override;
    size_t GetStateSize() override
    {
        return sizeof(ComplexState);
    }
    void ProcessGroupUnspill(std::vector<UnspillRowInfo> &unspillRows, int32_t rowCount, int32_t &vectorIndex) override;
    void ProcessAlignAggSchema(VectorBatch *result, BaseVector *originVector,
        const std::shared_ptr<NullsHelper> nullMap, const bool aggFilter) override;

    static std::unique_ptr<Aggregator> Create(const DataTypes &inputTypes, const DataTypes &outputTypes,
        std::vector<int32_t> &channels, bool rawIn, bool partialOut, bool isOverflowAsNull,
        type::DataTypeId colTypeId);

protected:
    MaxComplexAggregator(const DataTypes &inputTypes, const DataTypes &outputTypes,
        const std::vector<int32_t> &channels, bool inputRaw, bool outputPartial, bool isOverflowAsNull,
        type::DataTypeId targetColTypeId, type::DataTypePtr targetColDataType);

    void ProcessSingleInternal(AggregateState *state, BaseVector *vector, const int32_t rowOffset,
        const int32_t rowCount, const std::shared_ptr<NullsHelper> nullMap) override;
    void ProcessGroupInternal(std::vector<AggregateState *> &rowStates, BaseVector *vector, const int32_t rowOffset,
        const std::shared_ptr<NullsHelper> nullMap) override;

private:
    type::DataTypeId targetColTypeId_;
    type::DataTypePtr targetColDataType_;
    ComplexValueOwner ownedValues_;
};

} // namespace op
} // namespace omniruntime

#endif // OMNI_RUNTIME_MAX_COMPLEX_AGGREGATOR_H
