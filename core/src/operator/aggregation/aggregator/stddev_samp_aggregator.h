/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */
#ifndef OMNI_RUNTIME_STDDEV_SAMP_AGGREGATOR_H
#define OMNI_RUNTIME_STDDEV_SAMP_AGGREGATOR_H

#include "aggregator.h"

namespace omniruntime::op {
SIMD_ALWAYS_INLINE void StdDevSampPartialOp(double &mean, double &m2, double &cnt, const double &in)
{
    cnt++;
    double delta = in - mean;
    if (cnt != 0) {
        mean += delta / cnt;
    } else {
        mean = in;
    }
    m2 += delta * (in - mean);
}

SIMD_ALWAYS_INLINE void StdDevFinalOp(double &inMean, double &inM2, double &inCnt, double cnt, double mean, double m2)
{
    double newCnt = inCnt + cnt;
    double mergeMean = newCnt == 0 ? 0.0 : (inMean * inCnt + mean * cnt) / newCnt;
    inM2 = inM2 + m2 + inCnt * (inMean - mergeMean) * (inMean - mergeMean)
                 + cnt * (mean - mergeMean) * (mean - mergeMean);
    inMean = mergeMean;
    inCnt = newCnt;
}

template <DataTypeId IN_ID, DataTypeId OUT_ID = OMNI_DOUBLE> class StddevSampAggregator : public TypedAggregator {
    using RawInputType = typename AggNativeAndVectorType<IN_ID>::type;
    using ResultType = typename AggNativeAndVectorType<OUT_ID>::type;

#pragma pack(push, 1)

    struct StdDevState : BaseStdDevState {
        static const StddevSampAggregator<IN_ID, OUT_ID>::StdDevState *ConstCastState(const AggregateState *state)
        {
            return reinterpret_cast<const StddevSampAggregator<IN_ID, OUT_ID>::StdDevState *>(state);
        }

        static StddevSampAggregator<IN_ID, OUT_ID>::StdDevState *CastState(AggregateState *state)
        {
            return reinterpret_cast<StddevSampAggregator<IN_ID, OUT_ID>::StdDevState *>(state);
        }
        template <typename T>
        static void UpdateState(AggregateState *state, const T &in)
        {
            StdDevState *stdDevState = CastState(state);
            double val = static_cast<double>(in);
            StdDevSampPartialOp(stdDevState->mean, stdDevState->m2, stdDevState->count, val);
        }

        template <bool addIf, typename T>
        static void UpdateStateWithCondition(AggregateState *state, const T &in, const uint8_t &condition)
        {
            if (condition == addIf) {
                double val = static_cast<double>(in);
                UpdateState(state, val);
            }
        }
    };

#pragma pack(pop)

public:
    StddevSampAggregator(const DataTypes &inputTypes, const DataTypes &outputTypes, std::vector<int32_t> &channels,
        bool inputRaw, bool outputPartial, bool isOverflowAsNull)
        : TypedAggregator(OMNI_AGGREGATION_TYPE_SAMP, inputTypes, outputTypes, channels, inputRaw, outputPartial,
        isOverflowAsNull)
    {}

    ~StddevSampAggregator() override = default;

    size_t GetStateSize() override
    {
        return sizeof(StdDevState);
    }

    std::vector<DataTypePtr> GetSpillType() override
    {
        std::vector<DataTypePtr> spillTypes;
        spillTypes.emplace_back(std::make_shared<DataType>(OMNI_DOUBLE)); // count
        spillTypes.emplace_back(std::make_shared<DataType>(OMNI_DOUBLE)); // mean
        spillTypes.emplace_back(std::make_shared<DataType>(OMNI_DOUBLE)); // m2
        return spillTypes;
    }

    void ProcessSingleInternal(AggregateState *state, BaseVector *vector, const int32_t rowOffset,
        const int32_t rowCount, const std::shared_ptr<NullsHelper> nullMap)
    {
        StdDevState *sampState = StdDevState::CastState(state);
        if (!inputRaw) {
            ProcessSingleInternalFinal(state, vector, rowOffset, rowCount, nullMap);
            return;
        }
        if (vector->GetEncoding() == vec::OMNI_ENCODING_CONST) {
            if (nullMap == nullptr) {
                auto constValue = static_cast<vec::ConstVector<RawInputType> *>(vector)->GetConstValue();
                for (int32_t i = 0; i < rowCount; ++i) {
                    StdDevSampPartialOp(sampState->mean, sampState->m2, sampState->count,
                        static_cast<double>(constValue));
                }
            }
        } else if (vector->GetEncoding() != vec::OMNI_DICTIONARY) {
            auto *ptr = reinterpret_cast<RawInputType *>(GetValuesFromVector<IN_ID>(vector));
            ptr += rowOffset;
            if (nullMap == nullptr) {
                AddMomentStats<StdDevSampPartialOp, RawInputType>(sampState->mean, sampState->m2, sampState->count, ptr, rowCount);
            } else {
                AddMomentStatsConditional<StdDevSampPartialOp, RawInputType>(sampState->mean, sampState->m2, sampState->count, ptr,
                rowCount, *nullMap);
            }
        } else {
            auto *ptr = reinterpret_cast<RawInputType *>(GetValuesFromDict<IN_ID>(vector));
            auto *indexMap = GetIdsFromDict<IN_ID>(vector) + rowOffset;
            if (nullMap == nullptr) {
                AddMomentStatsDict<StdDevSampPartialOp, RawInputType>(sampState->mean, sampState->m2, sampState->count, ptr, rowCount,
                        indexMap);
            } else {
                AddMomentStatsDictConditional<StdDevSampPartialOp, RawInputType>(sampState->mean, sampState->m2, sampState->count, ptr,
                rowCount, *nullMap, indexMap);
            }
        }
    }

    void ProcessSingleInternalFinal(AggregateState *state, BaseVector *vector, const int32_t rowOffset,
        const int32_t rowCount, const std::shared_ptr<NullsHelper> nullMap)
    {
        auto *cnt = curVectorBatch->Get(channels[0]);
        auto *meanOther = curVectorBatch->Get(channels[1]);
        auto *m2 = curVectorBatch->Get(channels[2]);

        auto *cntPtr = reinterpret_cast<double *>(GetValuesFromVector<IN_ID>(cnt));
        auto *meanPtr = reinterpret_cast<double *>(GetValuesFromVector<OMNI_DOUBLE>(meanOther));
        auto *m2Ptr = reinterpret_cast<double *>(GetValuesFromVector<OMNI_DOUBLE>(m2));

        cntPtr += rowOffset;
        meanPtr += rowOffset;
        m2Ptr += rowOffset;

        StdDevState *sampState = StdDevState::CastState(state);
        bool isNullMap = nullMap == nullptr;
        for (int i = 0; i < rowCount; i++) {
            if (isNullMap || (*nullMap)[i] == false) {
                StdDevFinalOp(sampState->mean, sampState->m2, sampState->count, cntPtr[i], meanPtr[i], m2Ptr[i]);
            }
        }
    }

    void ProcessGroupInternal(std::vector<AggregateState *> &rowStates, BaseVector *vector, const int32_t rowOffset,
        const std::shared_ptr<NullsHelper> nullMap)
    {
        if (this->inputRaw) {
            if (vector->GetEncoding() == vec::OMNI_ENCODING_CONST) {
                auto constValue = static_cast<vec::ConstVector<RawInputType> *>(vector)->GetConstValue();
                if (nullMap == nullptr) {
                    for (size_t i = 0; i < rowStates.size(); ++i) {
                        StdDevState::UpdateState(rowStates[i] + aggStateOffset, constValue);
                    }
                } else {
                    for (size_t i = 0; i < rowStates.size(); ++i) {
                        StdDevState::template UpdateStateWithCondition<false>(rowStates[i] + aggStateOffset, constValue, (*nullMap)[i]);
                    }
                }
            } else if (vector->GetEncoding() != vec::OMNI_DICTIONARY) {
                auto *ptr = reinterpret_cast<RawInputType *>(GetValuesFromVector<IN_ID>(vector));
                ptr += rowOffset;
                if (nullMap == nullptr) {
                    AddUseRowIndex<RawInputType, StdDevState::UpdateState>(rowStates, aggStateOffset, ptr);
                } else {
                    AddConditionalUseRowIndex<RawInputType, StdDevState::template UpdateStateWithCondition<false>>(
                        rowStates, aggStateOffset, ptr, *nullMap);
                }
            } else {
                auto *ptr = reinterpret_cast<RawInputType *>(GetValuesFromDict<IN_ID>(vector));
                auto *indexMap = GetIdsFromDict<IN_ID>(vector) + rowOffset;
                if (nullMap == nullptr) {
                    AddDictUseRowIndex<RawInputType, StdDevState::UpdateState>(rowStates, aggStateOffset, ptr,
                        indexMap);
                } else {
                    AddDictConditionalUseRowIndex<RawInputType, ResultType,
                        StdDevState::template UpdateStateWithCondition<false>>(rowStates, aggStateOffset, ptr, *nullMap,
                        indexMap);
                }
            }
        } else {
            auto *cntVector = curVectorBatch->Get(channels[0]);
            auto *meanVector = curVectorBatch->Get(channels[1]);
            auto *m2Vector = curVectorBatch->Get(channels[2]);

            auto *cntPtr = reinterpret_cast<double *>(GetValuesFromVector<IN_ID>(cntVector));
            auto *meanPtr = reinterpret_cast<double *>(GetValuesFromVector<OMNI_DOUBLE>(meanVector));
            auto *m2Ptr = reinterpret_cast<double *>(GetValuesFromVector<OMNI_DOUBLE>(m2Vector));

            cntPtr += rowOffset;
            meanPtr += rowOffset;
            m2Ptr += rowOffset;

            if (nullMap == nullptr) {
                AddUseRowIndexMomentStatsFinal<StdDevState, StdDevFinalOp>(rowStates, aggStateOffset, cntPtr, meanPtr,
                    m2Ptr);
            } else {
                // Reza: can we use customize float operation similar to sumConditionalFloat
                AddConditionalUseRowIndexMomentStatsFinal<StdDevState, StdDevFinalOp>(rowStates, aggStateOffset, cntPtr,
                    meanPtr, m2Ptr, *nullMap);
            }
        }
    }

    void InitState(AggregateState *state) override
    {
        StdDevState *sumFlatState = StdDevState::CastState(state + aggStateOffset);
        sumFlatState->m2 = 0.0;
        sumFlatState->mean = 0.0;
        sumFlatState->count = 0.0;
    }

    void InitStates(std::vector<AggregateState *> &groupStates) override
    {
        for (auto groupState : groupStates) {
            InitState(groupState);
        }
    }

    void ExtractValues(const AggregateState *state, std::vector<BaseVector *> &vectors, int32_t rowIndex) override
    {
        const StdDevState *sampState = StdDevState::ConstCastState(state + aggStateOffset);
        if (outputPartial) {
            auto *cntVector = static_cast<Vector<double> *>(vectors[0]);
            auto *meanVector = static_cast<Vector<double> *>(vectors[1]);
            auto *m2Vector = static_cast<Vector<double> *>(vectors[2]);
            // all input are nulls, return 0
            if (sampState->count <= 0) {
                cntVector->SetNull(rowIndex);
                meanVector->SetNull(rowIndex);
                m2Vector->SetNull(rowIndex);
                return;
            }
            cntVector->SetValue(rowIndex, sampState->count);
            meanVector->SetValue(rowIndex, sampState->mean);
            m2Vector->SetValue(rowIndex, sampState->m2);
        } else {
            auto sampValVector = static_cast<Vector<double> *>(vectors[0]);
            if (UNLIKELY(sampState->IsEmpty())) {
                sampValVector->SetNull(rowIndex);
                return;
            }
            auto currentM2 = sampState->m2;
            if (sampState->count == 1) {
                if (!IsStatisticalAggregate()) {
                    sampValVector->SetNull(rowIndex);
                } else {
                    sampValVector->SetValue(rowIndex, std::numeric_limits<double>::quiet_NaN());
                }
            } else {
                auto result = std::sqrt(currentM2 / (sampState->count - 1));
                sampValVector->SetValue(rowIndex, result);
            }
        }
    }

    void ProcessGroupUnspill(std::vector<UnspillRowInfo> &unspillRows, int32_t rowCount, int32_t &vectorIndex) override
    {
        auto firstVecIdx = vectorIndex++;
        auto secondVecIdx = vectorIndex++;
        auto thirdVecIdx = vectorIndex++;
        for (int32_t rowIdx = 0; rowIdx < rowCount; rowIdx++) {
            auto &row = unspillRows[rowIdx];
            auto batch = row.batch;
            auto index = row.rowIdx;
            auto countVector = static_cast<Vector<ResultType> *>(batch->Get(firstVecIdx));
            if (!countVector->IsNull(index)) {
                auto count = countVector->GetValue(index);
                auto meanVector = static_cast<Vector<double> *>(batch->Get(secondVecIdx));
                auto mean = meanVector->GetValue(index);
                auto m2Vec = static_cast<Vector<double> *>(batch->Get(thirdVecIdx));
                auto m2 = m2Vec->GetValue(index);
                StdDevState *state = StdDevState::CastState(row.state + aggStateOffset);
                StdDevFinalOp(state->mean, state->m2, state->count, count, mean, m2);
            }
        }
    }

    void ExtractValuesForSpill(std::vector<AggregateState *> &groupStates, std::vector<BaseVector *> &vectors) override
    {
        auto countVector = static_cast<Vector<double> *>(vectors[0]);
        auto meanVector = static_cast<Vector<double> *>(vectors[1]);
        auto m2Vector = static_cast<Vector<double> *>(vectors[2]);

        auto rowCount = static_cast<int32_t>(groupStates.size());
        for (int32_t rowIndex = 0; rowIndex < rowCount; rowIndex++) {
            auto *state = StdDevState::CastState(groupStates[rowIndex] + aggStateOffset);
            if (UNLIKELY(state->IsEmpty())) {
                // set null for empty group(all rows are NULL) when spill to ensure skip empty group when unspill
                countVector->SetNull(rowIndex);
            } else {
                countVector->SetValue(rowIndex, state->count);
                meanVector->SetValue(rowIndex, state->mean);
                m2Vector->SetValue(rowIndex, state->m2);
            }
        }
    }

    void ExtractValuesBatch(std::vector<AggregateState *> &groupStates, std::vector<BaseVector *> &vectors,
        int32_t rowOffset, int32_t rowCount) override
    {
        if (this->outputPartial) {
            auto countVector = static_cast<Vector<double> *>(vectors[0]);
            auto meanVector = static_cast<Vector<double> *>(vectors[1]);
            auto m2Vector = static_cast<Vector<double> *>(vectors[2]);
            for (int32_t rowIndex = 0; rowIndex < rowCount; rowIndex++) {
                auto *state = StdDevState::CastState(groupStates[rowIndex] + aggStateOffset);
                if (UNLIKELY(state->IsEmpty())) {
                    // all input are nulls, return 0
                    countVector->SetValue(rowIndex, 0);
                    meanVector->SetValue(rowIndex, 0);
                    m2Vector->SetValue(rowIndex, 0);
                    continue;
                }
                countVector->SetValue(rowIndex, state->count);
                meanVector->SetValue(rowIndex, state->mean);
                m2Vector->SetValue(rowIndex, state->m2);
            }
        } else {
            auto stdDevSampValVector = static_cast<Vector<double> *>(vectors[0]);
            for (int32_t rowIndex = 0; rowIndex < rowCount; rowIndex++) {
                auto *state = StdDevState::CastState(groupStates[rowIndex] + aggStateOffset);
                if (UNLIKELY(state->IsEmpty())) {
                    // only contains null
                    stdDevSampValVector->SetNull(rowIndex);
                    continue;
                }
                if (state->count == 1) {
                    if (!IsStatisticalAggregate()) {
                        stdDevSampValVector->SetNull(rowIndex);
                    } else {
                        stdDevSampValVector->SetValue(rowIndex, std::numeric_limits<double>::quiet_NaN());
                    }
                } else {
                    auto result = std::sqrt(state->m2 / (state->count - 1));
                    stdDevSampValVector->SetValue(rowIndex, result);
                }
            }
        }
    }

    void ProcessAlignAggSchema(VectorBatch *result, BaseVector *originVector,
        const std::shared_ptr<NullsHelper> nullMap, const bool aggFilter) override
    {
        auto rowCount = originVector->GetSize();
        // opt branch
        if constexpr (std::is_same_v<RawInputType, ResultType>) {
            if (nullMap == nullptr) {
                auto countVector = VectorHelper::CreateFlatVector(this->pool_, OMNI_DOUBLE, rowCount);
                auto *valueAddr = reinterpret_cast<double *>(GetValuesFromVector<OMNI_DOUBLE>(countVector));
                std::fill_n(valueAddr, rowCount, 1);
                auto avgVector = VectorHelper::SliceVector(originVector, 0, rowCount);
                auto m2Vector = VectorHelper::CreateFlatVector(this->pool_, OMNI_DOUBLE, rowCount);
                auto *m2Addr = reinterpret_cast<double *>(GetValuesFromVector<OMNI_DOUBLE>(m2Vector));
                std::fill_n(m2Addr, rowCount, 0.0);

                result->Append(countVector);
                result->Append(avgVector);
                result->Append(m2Vector);
                return;
            }
        }

        if (originVector->GetEncoding() == OMNI_DICTIONARY) {
            ProcessAlignAggSchemaInternal<Vector<DictionaryContainer<RawInputType>>>(result, originVector, nullMap);
        } else {
            ProcessAlignAggSchemaInternal<Vector<RawInputType>>(result, originVector, nullMap);
        }
    }

protected:
    // logic: Template-based vector encoding type, to avoid long functions and high depth.
    template <typename T>
    void ProcessAlignAggSchemaInternal(VectorBatch *result, BaseVector *originVector,
        const std::shared_ptr<NullsHelper> nullMap)
    {
        int rowCount = originVector->GetSize();
        auto countVector = reinterpret_cast<Vector<double> *>(VectorHelper::CreateFlatVector(this->pool_, OMNI_DOUBLE, rowCount));
        auto avgVector = reinterpret_cast<Vector<double> *>(VectorHelper::CreateFlatVector(this->pool_, OMNI_DOUBLE, rowCount));
        auto m2Vector = reinterpret_cast<Vector<double> *>(VectorHelper::CreateFlatVector(this->pool_, OMNI_DOUBLE, rowCount));

        auto vector = reinterpret_cast<T *>(originVector);
        if (nullMap != nullptr) {
            for (int index = 0; index < rowCount; ++index) {
                if ((*nullMap)[index]) {
                    countVector->SetValue(index, 0);
                    avgVector->SetValue(index, 0);
                    m2Vector->SetValue(index, 0);
                } else {
                    countVector->SetValue(index, 1);
                    avgVector->SetValue(index, (ResultType)vector->GetValue(index));
                    m2Vector->SetValue(index, 0);
                }
            }
        } else {
            for (int index = 0; index < rowCount; ++index) {
                avgVector->SetValue(index, (ResultType)vector->GetValue(index));
            }
            auto *valueAddr = reinterpret_cast<double *>(GetValuesFromVector<OMNI_DOUBLE>(countVector));
            std::fill_n(valueAddr, rowCount, 1);
            valueAddr = reinterpret_cast<double *>(GetValuesFromVector<OMNI_DOUBLE>(m2Vector));
            std::fill_n(valueAddr, rowCount, 0);
        }

        result->Append(countVector);
        result->Append(avgVector);
        result->Append(m2Vector);
    }
};
}

#endif // OMNI_RUNTIME_STDDEV_SAMP_AGGREGATOR_H
