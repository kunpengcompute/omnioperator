/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Min_by when target column (col1) is OMNI_ARRAY, OMNI_MAP, or OMNI_ROW.
 *              Col1 element access uses complex_aggregator_util (GetComplexColSlice/SetComplexColValue)
 *              because these vector types do not have a flat value buffer per row.
 */
#ifndef OMNI_RUNTIME_MINBY_COMPLEX_AGGREGATOR_H
#define OMNI_RUNTIME_MINBY_COMPLEX_AGGREGATOR_H

#include "typed_aggregator.h"
#include "complex_aggregator_util.h"
#include "minby_aggregator.h"
#include "operator/aggregation/vector_getter.h"

namespace omniruntime {
namespace op {

template <type::DataTypeId COL2_ID>
class MinByComplexAggregator : public TypedAggregator {
    using sortKeyType = typename AggNativeAndVectorType<COL2_ID>::type;
    using sortKeyTypeVec = typename AggNativeAndVectorType<COL2_ID>::vector;

#pragma pack(push, 1)
    struct ComplexState {
        BaseVector *targetValue = nullptr;
        sortKeyType sortKey = {};
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
    ~MinByComplexAggregator() override = default;

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

    static std::unique_ptr<Aggregator> Create(const DataTypes &inputTypes, const DataTypes &outputTypes,
        std::vector<int32_t> &channels, bool rawIn, bool partialOut, bool isOverflowAsNull,
        type::DataTypeId targetColTypeId)
    {
        if (inputTypes.GetType(0)->GetId() != outputTypes.GetType(0)->GetId()) {
            throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", "output col type not match input");
        }
        return std::unique_ptr<Aggregator>(new MinByComplexAggregator<COL2_ID>(OMNI_AGGREGATION_TYPE_MIN_BY,
            inputTypes, outputTypes, channels, rawIn, partialOut, isOverflowAsNull, targetColTypeId,
            outputTypes.GetType(0)));
    }

    void AlignAggSchema(VectorBatch *result, VectorBatch *inputVecBatch) override;

    void AlignAggSchemaWithFilter(VectorBatch *result, VectorBatch *inputVecBatch,
        const int32_t filterIndex) override;

    void ProcessAlignAggSchema(VectorBatch *result, BaseVector *originVector,
        const std::shared_ptr<NullsHelper> nullMap, const bool aggFilter) override;

protected:
    MinByComplexAggregator(FunctionType aggType, const DataTypes &inputTypes, const DataTypes &outputTypes,
        const std::vector<int32_t> &channels, bool inputRaw, bool outputPartial, bool isOverflowAsNull,
        type::DataTypeId targetColTypeId, type::DataTypePtr targetColDataType)
        : TypedAggregator(aggType, inputTypes, outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull)
        , targetColTypeId_(targetColTypeId)
        , targetColDataType_(std::move(targetColDataType))
    {}

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

#endif // OMNI_RUNTIME_MINBY_COMPLEX_AGGREGATOR_H
