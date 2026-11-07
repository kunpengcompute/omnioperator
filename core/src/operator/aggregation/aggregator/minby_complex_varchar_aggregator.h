/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Min_by when target column (col1) is OMNI_ARRAY/OMNI_MAP/OMNI_ROW and sort key (col2) is VARCHAR/CHAR.
 *              Col1 access via complex_aggregator_util; col2 is string (memcmp).
 */
#ifndef OMNI_RUNTIME_MINBY_COMPLEX_VARCHAR_AGGREGATOR_H
#define OMNI_RUNTIME_MINBY_COMPLEX_VARCHAR_AGGREGATOR_H

#include "typed_aggregator.h"
#include "complex_aggregator_util.h"
#include "vector/vector.h"
#include "type/string_ref.h"
#include <cstring>

namespace omniruntime {
namespace op {

template <type::DataTypeId COL2_ID>
class MinByComplexVarcharAggregator : public TypedAggregator {
#pragma pack(push, 1)
    struct ComplexVarcharState {
        BaseVector *targetValue = nullptr;

    private:
        int64_t strKeyAddress = 0;
        int32_t strKeyLen = 0;
        bool saved = false;

    public:
        static const ComplexVarcharState *ConstCastState(const AggregateState *state)
        {
            return reinterpret_cast<const ComplexVarcharState *>(state);
        }
        static ComplexVarcharState *CastState(AggregateState *state)
        {
            return reinterpret_cast<ComplexVarcharState *>(state);
        }
        void SetStrKey(int64_t address, int32_t len)
        {
            ReleaseSortKey();
            strKeyAddress = address;
            strKeyLen = len;
        }
        /** Reset key fields without releasing (for InitState; avoids delete on uninitialized state). */
        void ClearStrKey()
        {
            strKeyAddress = 0;
            strKeyLen = 0;
            saved = false;
        }
        int64_t GetStrKeyAddress() const { return strKeyAddress; }
        int32_t GetStrKeyLen() const { return strKeyLen; }
        void SaveSortKey()
        {
            if (saved || strKeyAddress == 0) {
                return;
            }
            char *copied_data = new char[strKeyLen + 1];
            std::memcpy(copied_data, reinterpret_cast<const void *>(strKeyAddress), strKeyLen);
            copied_data[strKeyLen] = '\0';
            strKeyAddress = reinterpret_cast<int64_t>(copied_data);
            saved = true;
        }
        void ReleaseSortKey()
        {
            if (!saved) {
                return;
            }
            if (strKeyAddress == 0) {
                saved = false;
                return;
            }
            if (strKeyLen > 0) {
                delete[] reinterpret_cast<char *>(strKeyAddress);
            }
            strKeyAddress = 0;
            strKeyLen = 0;
            saved = false;
        }
        ~ComplexVarcharState() { ReleaseSortKey(); }
    };
#pragma pack(pop)

public:
    ~MinByComplexVarcharAggregator() override = default;

    void ExtractValues(const AggregateState *state, std::vector<BaseVector *> &vectors, int32_t rowIndex) override;
    void ExtractValuesBatch(std::vector<AggregateState *> &groupStates, std::vector<BaseVector *> &vectors,
        int32_t rowOffset, int32_t rowCount) override;
    void ExtractValuesForSpill(std::vector<AggregateState *> &groupStates, std::vector<BaseVector *> &vectors) override;
    void InitState(AggregateState *state) override;
    void InitStates(std::vector<AggregateState *> &groupStates) override;
    std::vector<DataTypePtr> GetSpillType() override;
    size_t GetStateSize() override { return sizeof(ComplexVarcharState); }
    void ProcessGroupUnspill(std::vector<UnspillRowInfo> &unspillRows, int32_t rowCount, int32_t &vectorIndex) override;

    static std::unique_ptr<Aggregator> Create(const DataTypes &inputTypes, const DataTypes &outputTypes,
        std::vector<int32_t> &channels, bool rawIn, bool partialOut, bool isOverflowAsNull,
        type::DataTypeId targetColTypeId)
    {
        if (inputTypes.GetType(0)->GetId() != outputTypes.GetType(0)->GetId()) {
            throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", "output col type not match input");
        }
        return std::unique_ptr<Aggregator>(new MinByComplexVarcharAggregator<COL2_ID>(OMNI_AGGREGATION_TYPE_MIN_BY,
            inputTypes, outputTypes, channels, rawIn, partialOut, isOverflowAsNull, targetColTypeId,
            outputTypes.GetType(0)));
    }

    void AlignAggSchema(VectorBatch *result, VectorBatch *inputVecBatch) override;

    void AlignAggSchemaWithFilter(VectorBatch *result, VectorBatch *inputVecBatch,
        const int32_t filterIndex) override;

    void ProcessAlignAggSchema(VectorBatch *result, BaseVector *originVector,
        const std::shared_ptr<NullsHelper> nullMap, const bool aggFilter) override;

protected:
    MinByComplexVarcharAggregator(FunctionType aggType, const DataTypes &inputTypes, const DataTypes &outputTypes,
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

#endif // OMNI_RUNTIME_MINBY_COMPLEX_VARCHAR_AGGREGATOR_H
