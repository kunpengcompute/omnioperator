/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: BitOr aggregate
 */
#ifndef OMNI_RUNTIME_BIT_OR_AGGREGATOR_H
#define OMNI_RUNTIME_BIT_OR_AGGREGATOR_H

#include <cstdint>
#include <cfloat>
#include <cstring>

#include "typed_aggregator.h"

namespace omniruntime {
namespace op {
template <typename T> constexpr T GetBitOrInit()
{
    if constexpr (std::is_same_v<T, int8_t>) {
        return 0;
    } else if constexpr (std::is_same_v<T, int16_t>) {
        return 0;
    } else if constexpr (std::is_same_v<T, int32_t>) {
        return 0;
    } else if constexpr (std::is_same_v<T, int64_t>) {
        return 0;
    } else {
        throw OmniException("LogicalError", "Unsupoorted data type");
    }
}

template <typename IN, typename OUT>
SIMD_ALWAYS_INLINE void BitOrOp(OUT *res, AggValueState &flag, const IN &in, const int64_t notUsed)
{
    const OUT cur = static_cast<OUT>(in);
    *res = *res | cur;
    flag = AggValueState::NORMAL;
}

template <typename IN, typename OUT, bool addIf>
SIMD_ALWAYS_INLINE void BitOrConditionalOp(OUT *res, AggValueState &flag, const IN &in, const int64_t notUsed,
    const uint8_t &condition)
{
    if (condition == addIf) {
        BitOrOp(res, flag, in, notUsed);
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID> class BitOrAggregator : public TypedAggregator {
    using InVector = typename AggNativeAndVectorType<IN_ID>::vector;
    using InType = typename AggNativeAndVectorType<IN_ID>::type;
    using OutVector = typename AggNativeAndVectorType<OUT_ID>::vector;
    using OutType = typename AggNativeAndVectorType<OUT_ID>::type;
    using ResultType = typename AggNativeAndVectorType<OUT_ID>::type;
    static constexpr bool kMixedStateSerdeSupported =
        IN_ID == OUT_ID && IsMixedSerdeIntegralType<IN_ID>() && IsMixedSerdeIntegralType<OUT_ID>();

#pragma pack(push, 1)
    struct BitOrState : BaseState<ResultType> {
        static const BitOrAggregator<IN_ID, OUT_ID>::BitOrState *ConstCastState(const AggregateState *state)
        {
            return reinterpret_cast<const BitOrAggregator<IN_ID, OUT_ID>::BitOrState *>(state);
        }

        static BitOrAggregator<IN_ID, OUT_ID>::BitOrState *CastState(AggregateState *state)
        {
            return reinterpret_cast<BitOrAggregator<IN_ID, OUT_ID>::BitOrState *>(state);
        }

        template <typename TypeIn, typename TypeOut> static void UpdateState(AggregateState *state, const TypeIn &in)
        {
            auto *bitOrState = CastState(state);
            BitOrOp<TypeIn, TypeOut>(&(bitOrState->value), bitOrState->valueState, in, 1ULL);
        }

        template <typename TypeIn, typename TypeOut, bool addIf>
        static void UpdateStateWithCondition(AggregateState *state, const TypeIn &in, const uint8_t &condition)
        {
            if (condition == addIf) {
                UpdateState<TypeIn, TypeOut>(state, in);
            }
        }
    };
#pragma pack(pop)
public:
    ~BitOrAggregator() override = default;

    size_t GetStateSize() override
    {
        return sizeof(BitOrState);
    }

    static void MergeMixedStateImpl(BitOrAggregator<IN_ID, OUT_ID> *, BitOrState *targetState,
        const BitOrState *sourceState)
    {
        if constexpr (kMixedStateSerdeSupported) {
            if (sourceState->IsOverFlowed()) {
                targetState->SetOverFlow();
            } else if (!sourceState->IsEmpty()) {
                BitOrOp<ResultType, ResultType>(&targetState->value, targetState->valueState, sourceState->value, 1LL);
            }
        } else {
            (void)targetState;
            (void)sourceState;
        }
    }

    const MixedStateSerdeOps *GetMixedStateSerdeOps() const override
    {
        if constexpr (kMixedStateSerdeSupported) {
            return RawMixedStateSerde<BitOrState, BitOrAggregator<IN_ID, OUT_ID>,
                &BitOrAggregator::MergeMixedStateImpl>::Ops();
        } else {
            return nullptr;
        }
    }

    void ExtractValues(const AggregateState *state, std::vector<BaseVector *> &vectors, int32_t rowIndex) override;
    void ExtractValuesBatch(std::vector<AggregateState *> &groupStates, std::vector<BaseVector *> &vectors,
        int32_t rowOffset, int32_t rowCount) override;
    void ExtractValuesForSpill(std::vector<AggregateState *> &groupStates, std::vector<BaseVector *> &vectors) override;
    void InitState(AggregateState *state) override;
    void InitStates(std::vector<AggregateState *> &groupStates) override;
    std::vector<DataTypePtr> GetSpillType() override;

    static std::unique_ptr<Aggregator> Create(const DataTypes &inputTypes, const DataTypes &outputTypes,
        std::vector<int32_t> &channels, bool rawIn, bool partialOut, bool isOverflowAsNull)
    {
        constexpr bool kInSupported =
        (IN_ID == OMNI_BYTE || IN_ID == OMNI_SHORT || IN_ID == OMNI_INT || IN_ID == OMNI_LONG);

        constexpr bool kOutSupported =
            (OUT_ID == OMNI_BYTE || OUT_ID == OMNI_SHORT || OUT_ID == OMNI_INT  || OUT_ID == OMNI_LONG);

        if constexpr (IN_ID != OUT_ID) {
            LogError("Error in bit or aggregator: IN type %s and OUT type %s are not the same",
                     TypeUtil::TypeToStringLog(IN_ID).c_str(),
                     TypeUtil::TypeToStringLog(OUT_ID).c_str());
            return nullptr;
        }

        if constexpr (!kInSupported) {
            LogError("Error in bit or aggregator: Unsupported input type %s",
                     TypeUtil::TypeToStringLog(IN_ID).c_str());
            return nullptr;
        }

        if constexpr (!kOutSupported) {
            LogError("Error in bit or aggregator: Unsupported output type %s",
                     TypeUtil::TypeToStringLog(OUT_ID).c_str());
            return nullptr;
        }

        if (!TypedAggregator::CheckTypes("bit_or", inputTypes, outputTypes, IN_ID, OUT_ID)) {
            return nullptr;
        }
        return std::unique_ptr<BitOrAggregator<IN_ID, OUT_ID>>(new BitOrAggregator<IN_ID, OUT_ID>(inputTypes,
                        outputTypes, channels, rawIn, partialOut, isOverflowAsNull));
    }

    void ProcessGroupUnspill(std::vector<UnspillRowInfo> &unspillRows, int32_t rowCount, int32_t &vectorIndex) override;

    void ProcessAlignAggSchema(VectorBatch *result, BaseVector *originVector,
        const std::shared_ptr<NullsHelper> nullMap, const bool aggFilter) override;

protected:
    BitOrAggregator(const DataTypes &inputTypes, const DataTypes &outputTypes, std::vector<int32_t> &channels,
        const bool inputRaw, const bool outputPartial, const bool isOverflowAsNull);

    void ProcessSingleInternal(AggregateState *state, BaseVector *vector, const int32_t rowOffset,
        const int32_t rowCount, const std::shared_ptr<NullsHelper> nullMap) override;

    void ProcessGroupInternal(std::vector<AggregateState *> &rowStates, BaseVector *vector, const int32_t rowOffset,
        const std::shared_ptr<NullsHelper> nullMap) override;

    template<typename T>
    void ProcessAlignAggSchemaInternal(VectorBatch *result, BaseVector *originVector,
        const std::shared_ptr<NullsHelper> nullMap);
};
}
}
#endif // OMNI_RUNTIME_BIT_OR_AGGREGATOR_H
