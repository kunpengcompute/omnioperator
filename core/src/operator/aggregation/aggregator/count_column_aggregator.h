/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2024. All rights reserved.
 * Description: Count aggregate
 */
#ifndef OMNI_RUNTIME_COUNT_COLUMN_AGGREGATOR_H
#define OMNI_RUNTIME_COUNT_COLUMN_AGGREGATOR_H

#include <cstring>
#include "typed_aggregator.h"
#include "operator/aggregation/definitions.h"

namespace omniruntime {
namespace op {
SIMD_ALWAYS_INLINE
void CountAllOp(int64_t *res, int64_t &noUsed1, const int64_t &in, const int64_t noUsed2)
{
    *res += in;
}

template <bool addIf>
SIMD_ALWAYS_INLINE void CountAllConditionalOp(int64_t *res, int64_t &noUsed1, const int64_t &in, const int64_t noUsed2,
    const uint8_t &condition)
{
    const int64_t mask = (!condition == addIf) - 1;
    *res += (in & mask);
}

template <DataTypeId IN_ID, DataTypeId OUT_ID> class CountColumnAggregator : public TypedAggregator {
    using InType = typename AggNativeAndVectorType<IN_ID>::type;
    using ResultType = typename AggNativeAndVectorType<OUT_ID>::type;
public:
    struct CountState {
        int64_t count;

        static const CountColumnAggregator<IN_ID, OUT_ID>::CountState *ConstCastState(const AggregateState *state)
        {
            return reinterpret_cast<const CountColumnAggregator<IN_ID, OUT_ID>::CountState *>(state);
        }

        static CountColumnAggregator<IN_ID, OUT_ID>::CountState *CastState(AggregateState *state)
        {
            return reinterpret_cast<CountColumnAggregator<IN_ID, OUT_ID>::CountState *>(state);
        }
    };

    ~CountColumnAggregator() override = default;

    void ExtractValues(const AggregateState *state, std::vector<BaseVector *> &vectors, int32_t rowIndex) override;
    void ExtractValuesBatch(std::vector<AggregateState *> &groupStates, std::vector<BaseVector *> &vectors,
        int32_t rowOffset, int32_t rowCount) override;
    void ExtractValuesForSpill(std::vector<AggregateState *> &groupStates, std::vector<BaseVector *> &vectors) override;

    std::vector<DataTypePtr> GetSpillType() override
    {
        std::vector<DataTypePtr> spillTypes;
        spillTypes.emplace_back(std::make_shared<DataType>(OMNI_LONG));
        return spillTypes;
    }

    size_t GetStateSize() override
    {
        return sizeof(CountState);
    }

    static void MergeMixedStateImpl(CountColumnAggregator<IN_ID, OUT_ID> *, CountState *targetState,
        const CountState *sourceState)
    {
        targetState->count += sourceState->count;
    }

    const MixedStateSerdeOps *GetMixedStateSerdeOps() const override
    {
        return RawMixedStateSerde<CountState, CountColumnAggregator<IN_ID, OUT_ID>,
            &CountColumnAggregator::MergeMixedStateImpl>::Ops();
    }

    static std::unique_ptr<Aggregator> Create(const DataTypes &inputTypes, const DataTypes &outputTypes,
        std::vector<int32_t> &channels, bool inRaw, bool outPartial, bool isOverflowAsNull)
    {
        if constexpr (OUT_ID != OMNI_LONG) {
            LogError("Error in count column aggregator: Expecting long output type. Got %s",
                TypeUtil::TypeToStringLog(OUT_ID).c_str());
            return nullptr;
        }
        // OUT_ID == OMNI_LONG
        if (!inRaw && IN_ID != OMNI_LONG) {
            LogError("Error in count column aggregator: Expecting long intput type for partial input. Got %s",
                TypeUtil::TypeToStringLog(IN_ID).c_str());
            return nullptr;
        } else {
            if (!TypedAggregator::CheckTypes("count column", inputTypes, outputTypes, IN_ID, OUT_ID)) {
                return nullptr;
            }

            return std::unique_ptr<CountColumnAggregator<IN_ID, OUT_ID>>(
                new CountColumnAggregator<IN_ID, OUT_ID>(outputTypes, channels, inRaw, outPartial, isOverflowAsNull));
        }
    }

    void InitState(AggregateState *state) override
    {
        CountState *countState = CountState::CastState(state + aggStateOffset);
        countState->count = 0;
    }

    void InitStates(std::vector<AggregateState *> &states) override
    {
        for (auto state : states) {
            CountState *countState = CountState::CastState(state + aggStateOffset);
            countState->count = 0;
        }
    }

    void ProcessGroupUnspill(std::vector<UnspillRowInfo> &unspillRows, int32_t rowCount, int32_t &vectorIndex) override;

    void ProcessAlignAggSchema(VectorBatch *result, BaseVector *originVector,
        const std::shared_ptr<NullsHelper> nullMap, const bool aggFilter) override;

protected:
    CountColumnAggregator(const DataTypes &outputTypes, std::vector<int32_t> &channels, const bool inputRaw,
        const bool outputPartial, const bool isOverflowAsNull)
        : TypedAggregator(OMNI_AGGREGATION_TYPE_COUNT_COLUMN, *DataTypes::NoneDataTypesInstance(), outputTypes,
        channels, inputRaw, outputPartial, isOverflowAsNull)
    {
        if (inputRaw) {
            processSingleInternalPtr = &CountColumnAggregator<IN_ID, OUT_ID>::ProcessSingleInternalFunction<true>;
            processGroupInternalPtr = &CountColumnAggregator<IN_ID, OUT_ID>::ProcessGroupInternalFunction<true>;
        } else {
            processSingleInternalPtr = &CountColumnAggregator<IN_ID, OUT_ID>::ProcessSingleInternalFunction<false>;
            processGroupInternalPtr = &CountColumnAggregator<IN_ID, OUT_ID>::ProcessGroupInternalFunction<false>;
        }
    }

    CountColumnAggregator(FunctionType aggregateType, const DataTypes &outputTypes, std::vector<int32_t> &channels,
        const bool inputRaw, const bool outputPartial, const bool isOverflowAsNull)
        : TypedAggregator(aggregateType, *DataTypes::NoneDataTypesInstance(), outputTypes, channels, inputRaw,
        outputPartial, isOverflowAsNull)
    {
        if (inputRaw) {
            processSingleInternalPtr = &CountColumnAggregator<IN_ID, OUT_ID>::ProcessSingleInternalFunction<true>;
            processGroupInternalPtr = &CountColumnAggregator<IN_ID, OUT_ID>::ProcessGroupInternalFunction<true>;
        } else {
            processGroupInternalPtr = &CountColumnAggregator<IN_ID, OUT_ID>::ProcessGroupInternalFunction<false>;
            processSingleInternalPtr = &CountColumnAggregator<IN_ID, OUT_ID>::ProcessSingleInternalFunction<false>;
        }
    }

    void ProcessSingleInternal(AggregateState *state, BaseVector *vector, const int32_t rowOffset,
        const int32_t rowCount, const std::shared_ptr<NullsHelper> nullMap) override;

    void ProcessGroupInternal(std::vector<AggregateState *> &rowStates, BaseVector *vector, const int32_t rowOffset,
        const std::shared_ptr<NullsHelper> nullMap) override;

    template <bool RAW_IN>
    void ProcessSingleInternalFunction(AggregateState *state, BaseVector *vector, const int32_t rowOffset,
        const int32_t rowCount, const std::shared_ptr<NullsHelper> nullMap);

    template <bool RAW_IN>
    void ProcessGroupInternalFunction(std::vector<AggregateState *> &rowStates, BaseVector *vector,
        const int32_t rowOffset, const std::shared_ptr<NullsHelper> nullMap);

private:
    void (CountColumnAggregator<IN_ID, OUT_ID>::*processGroupInternalPtr)(std::vector<AggregateState *> &rowStates,
        BaseVector *vector, const int32_t rowOffset, const std::shared_ptr<NullsHelper> nullMap) = nullptr;

    void (CountColumnAggregator<IN_ID, OUT_ID>::*processSingleInternalPtr)(AggregateState *state, BaseVector *vector,
        const int32_t rowOffset, const int32_t rowCount, const std::shared_ptr<NullsHelper> nullMap) = nullptr;

    void ProcessGroupInternalFunctionForRawInput(std::vector<AggregateState *> &rowStates,
        const std::shared_ptr<NullsHelper> nullMap);
};
}
}
#endif // OMNI_RUNTIME_COUNT_COLUMN_AGGREGATOR_H
