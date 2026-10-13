/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Skewness aggregate
 */
#include <cmath>

#include "skewness_aggregator.h"
#include "central_moment.h"
#include "vector/vector_helper.h"

namespace omniruntime {
    namespace op {
        template<DataTypeId IN, DataTypeId OUT>
        void
        SkewnessAggregator<IN, OUT>::ExtractValues(
            const AggregateState *state, std::vector<BaseVector *> &vectors,
            int32_t rowIndex) {
            auto *pSkewnessState = SkewnessState::ConstCastState(state + aggStateOffset);
            auto &centralMomentState = pSkewnessState->centralMomentState;
            if (this->outputPartial) {
                auto skewnessCountVector = static_cast<Vector<double> *>(vectors[0]);
                auto skewnessCentralMoment1Vector = static_cast<Vector<double> *>(vectors[1]);
                auto skewnessCentralMoment2Vector = static_cast<Vector<double> *>(vectors[2]);
                auto skewnessCentralMoment3Vector = static_cast<Vector<double> *>(vectors[3]);

                if (centralMomentState.count <= 0) {
                    skewnessCountVector->SetValue(rowIndex, 0);
                    skewnessCentralMoment1Vector->SetValue(rowIndex, 0.0);
                    skewnessCentralMoment2Vector->SetValue(rowIndex, 0.0);
                    skewnessCentralMoment3Vector->SetValue(rowIndex, 0.0);
                    return;
                }
                skewnessCountVector->SetValue(rowIndex, static_cast<double>(centralMomentState.count));
                skewnessCentralMoment1Vector->SetValue(rowIndex, centralMomentState.centralMoment1);
                skewnessCentralMoment2Vector->SetValue(rowIndex, centralMomentState.centralMoment2);
                skewnessCentralMoment3Vector->SetValue(rowIndex, centralMomentState.centralMoment3);
            } else {
                auto result = static_cast<ValueOutVector *>(vectors[0]);
                if (centralMomentState.count <= 1 || centralMomentState.centralMoment2 == 0) {
                    result->SetNull(rowIndex);
                    return;
                }
                MomentValue count = static_cast<MomentValue>(centralMomentState.count);
                MomentValue centralMoment2 = centralMomentState.centralMoment2;
                MomentValue centralMoment3 = centralMomentState.centralMoment3;
                MomentValue skewness = sqrt(count) * centralMoment3 / sqrt(
                                           centralMoment2 * centralMoment2 * centralMoment2);
                result->SetValue(rowIndex, skewness);
            }
        }

        template<DataTypeId IN, DataTypeId OUT>
        void
        SkewnessAggregator<IN, OUT>::ExtractValuesBatch(
            std::vector<AggregateState *> &groupStates,
            std::vector<BaseVector *> &vectors, int32_t rowOffset, int32_t rowCount) {
            for (int32_t rowIndex = 0; rowIndex < rowCount; rowIndex++) {
                auto *pSkewnessState = SkewnessState::CastState(groupStates[rowIndex] + aggStateOffset);
                auto &centralMomentState = pSkewnessState->centralMomentState;
                if (this->outputPartial) {
                    auto skewnessCountVector = static_cast<Vector<double> *>(vectors[0]);
                    auto skewnessCentralMoment1Vector = static_cast<Vector<double> *>(vectors[1]);
                    auto skewnessCentralMoment2Vector = static_cast<Vector<double> *>(vectors[2]);
                    auto skewnessCentralMoment3Vector = static_cast<Vector<double> *>(vectors[3]);
                    if (centralMomentState.count <= 0) {
                        skewnessCountVector->SetValue(rowIndex, 0.0);
                        skewnessCentralMoment1Vector->SetValue(rowIndex, 0.0);
                        skewnessCentralMoment2Vector->SetValue(rowIndex, 0.0);
                        skewnessCentralMoment3Vector->SetValue(rowIndex, 0.0);
                        continue;
                    }
                    skewnessCountVector->SetValue(rowIndex, static_cast<double>(centralMomentState.count));
                    skewnessCentralMoment1Vector->SetValue(rowIndex, centralMomentState.centralMoment1);
                    skewnessCentralMoment2Vector->SetValue(rowIndex, centralMomentState.centralMoment2);
                    skewnessCentralMoment3Vector->SetValue(rowIndex, centralMomentState.centralMoment3);
                } else {
                    auto result = static_cast<ValueOutVector *>(vectors[0]);
                    if (centralMomentState.count <= 1 || centralMomentState.centralMoment2 == 0) {
                        result->SetNull(rowIndex);
                        continue;
                    }
                    MomentValue count = static_cast<MomentValue>(centralMomentState.count);
                    MomentValue centralMoment2 = centralMomentState.centralMoment2;
                    MomentValue centralMoment3 = centralMomentState.centralMoment3;
                    MomentValue skewness = sqrt(count) * centralMoment3 / sqrt(
                                               centralMoment2 * centralMoment2 * centralMoment2);
                    result->SetValue(rowIndex, skewness);
                }
            }
        }

        template<DataTypeId IN, DataTypeId OUT>
        std::vector<DataTypePtr>
        SkewnessAggregator<IN, OUT>::GetSpillType() {
            std::vector<DataTypePtr> spillTypes;
            spillTypes.emplace_back(std::make_shared<DataType>(OMNI_DOUBLE));
            spillTypes.emplace_back(std::make_shared<DataType>(OMNI_DOUBLE));
            spillTypes.emplace_back(std::make_shared<DataType>(OMNI_DOUBLE));
            spillTypes.emplace_back(std::make_shared<DataType>(OMNI_DOUBLE));
            return spillTypes;
        }

        template<DataTypeId IN, DataTypeId OUT>
        void
        SkewnessAggregator<IN, OUT>::ExtractValuesForSpill(
            std::vector<AggregateState *> &groupStates,
            std::vector<BaseVector *> &vectors) {
            auto skewnessCountVector = static_cast<Vector<double> *>(vectors[0]);
            auto skewnessCentralMoment1Vector = static_cast<Vector<double> *>(vectors[1]);
            auto skewnessCentralMoment2Vector = static_cast<Vector<double> *>(vectors[2]);
            auto skewnessCentralMoment3Vector = static_cast<Vector<double> *>(vectors[3]);
            auto rowCount = static_cast<int32_t>(groupStates.size());
            for (int32_t rowIndex = 0; rowIndex < rowCount; rowIndex++) {
                auto *pSkewnessState = SkewnessState::CastState(groupStates[rowIndex] + aggStateOffset);
                auto &centralMomentState = pSkewnessState->centralMomentState;
                if (centralMomentState.count <= 0) {
                    skewnessCountVector->SetNull(rowIndex);
                    skewnessCentralMoment1Vector->SetNull(rowIndex);
                    skewnessCentralMoment2Vector->SetNull(rowIndex);
                    skewnessCentralMoment3Vector->SetNull(rowIndex);
                } else {
                    skewnessCountVector->SetValue(rowIndex, static_cast<double>(centralMomentState.count));
                    skewnessCentralMoment1Vector->SetValue(rowIndex, centralMomentState.centralMoment1);
                    skewnessCentralMoment2Vector->SetValue(rowIndex, centralMomentState.centralMoment2);
                    skewnessCentralMoment3Vector->SetValue(rowIndex, centralMomentState.centralMoment3);
                }
            }
        }

        template<DataTypeId IN, DataTypeId OUT>
        void
        SkewnessAggregator<IN, OUT>::InitState(
            AggregateState *state) {
            auto *pSkewnessState = SkewnessState::CastState(state + aggStateOffset);
            pSkewnessState->centralMomentState.count = 0;
            pSkewnessState->centralMomentState.centralMoment1 = 0;
            pSkewnessState->centralMomentState.centralMoment2 = 0;
            pSkewnessState->centralMomentState.centralMoment3 = 0;
            pSkewnessState->centralMomentState.centralMoment4 = 0;
            pSkewnessState->centralMomentState.momentOrder = 3;
        }

        template<DataTypeId IN, DataTypeId OUT>
        void
        SkewnessAggregator<IN, OUT>::InitStates(
            std::vector<AggregateState *> &groupStates) {
            for (auto groupState: groupStates) {
                auto *pSkewnessState = SkewnessState::CastState(groupState + aggStateOffset);
                pSkewnessState->centralMomentState.count = 0;
                pSkewnessState->centralMomentState.centralMoment1 = 0;
                pSkewnessState->centralMomentState.centralMoment2 = 0;
                pSkewnessState->centralMomentState.centralMoment3 = 0;
                pSkewnessState->centralMomentState.centralMoment4 = 0;
                pSkewnessState->centralMomentState.momentOrder = 3;
            }
        }

        template<DataTypeId IN, DataTypeId OUT>
        void
        SkewnessAggregator<IN, OUT>::ProcessSingleInternal(
            AggregateState *state, BaseVector *vector,
            const int32_t rowOffset, const int32_t rowCount, const std::shared_ptr<NullsHelper> nullMap) {
            auto *pSkewnessState = SkewnessState::CastState(state);
            auto &centralMomentState = pSkewnessState->centralMomentState;
            if (this->inputRaw) {
                auto *valuePtr = reinterpret_cast<Value *>(VectorHelper::GetFlatValuePtr<IN>(vector));
                valuePtr += rowOffset;
                if (nullMap == nullptr) {
                    AddCentralMoment<MomentValue, Value, AggValueState, UpdateCentralMomentOp<MomentValue, Value> >(
                        &centralMomentState.count,
                        &centralMomentState.centralMoment1, &centralMomentState.centralMoment2,
                        &centralMomentState.centralMoment3, &centralMomentState.centralMoment4,
                        &centralMomentState.momentOrder, pSkewnessState->valueState, valuePtr, rowCount);
                } else {
                    AddCentralMomentConditional<MomentValue, Value, AggValueState, UpdateCentralMomentConditionalOp<
                        MomentValue, Value, false> >(
                        &centralMomentState.count, &centralMomentState.centralMoment1,
                        &centralMomentState.centralMoment2, &centralMomentState.centralMoment3,
                        &centralMomentState.centralMoment4, &centralMomentState.momentOrder,
                        pSkewnessState->valueState, valuePtr, rowCount, *nullMap);
                }
            } else {
                auto *countVector = this->curVectorBatch->Get(this->channels[0]);
                auto *m1Vector = this->curVectorBatch->Get(this->channels[1]);
                auto *m2Vector = this->curVectorBatch->Get(this->channels[2]);
                auto *m3Vector = this->curVectorBatch->Get(this->channels[3]);

                auto *countPtr = reinterpret_cast<MomentValue *>(VectorHelper::GetFlatValuePtr<OMNI_DOUBLE>(countVector));
                auto *m1Ptr = reinterpret_cast<MomentValue *>(VectorHelper::GetFlatValuePtr<OMNI_DOUBLE>(m1Vector));
                auto *m2Ptr = reinterpret_cast<MomentValue *>(VectorHelper::GetFlatValuePtr<OMNI_DOUBLE>(m2Vector));
                auto *m3Ptr = reinterpret_cast<MomentValue *>(VectorHelper::GetFlatValuePtr<OMNI_DOUBLE>(m3Vector));
                countPtr += rowOffset;
                m1Ptr += rowOffset;
                m2Ptr += rowOffset;
                m3Ptr += rowOffset;
                double m4 = 0.0;
                double *m4ptr = &m4;

                if (nullMap == nullptr) {
                    MergeCentralMoment<MomentValue, AggValueState, MergeCentralMomentOp<MomentValue> >(
                        &centralMomentState.count,
                        &centralMomentState.centralMoment1,
                        &centralMomentState.centralMoment2,
                        &centralMomentState.centralMoment3,
                        &centralMomentState.centralMoment4,
                        &centralMomentState.momentOrder,
                        pSkewnessState->valueState,
                        countPtr,
                        m1Ptr, m2Ptr, m3Ptr, m4ptr,
                        rowCount);
                } else {
                    MergeCentralMomentConditional<MomentValue, AggValueState, MergeCentralMomentConditionalOp<
                        MomentValue, false> >(
                        &centralMomentState.count,
                        &centralMomentState.centralMoment1,
                        &centralMomentState.centralMoment2,
                        &centralMomentState.centralMoment3,
                        &centralMomentState.centralMoment4,
                        &centralMomentState.momentOrder,
                        pSkewnessState->valueState,
                        countPtr,
                        m1Ptr, m2Ptr, m3Ptr, m4ptr,
                        rowCount, *nullMap);
                }
            }
        }

        template<DataTypeId IN, DataTypeId OUT>
        void
        SkewnessAggregator<IN, OUT>::ProcessGroupInternal(
            std::vector<AggregateState *> &rowStates, BaseVector *vector,
            const int32_t rowOffset, const std::shared_ptr<NullsHelper> nullMap) {
            if (this->inputRaw) {
                auto *valuePtr = reinterpret_cast<Value *>(VectorHelper::GetFlatValuePtr<IN>(vector));
                valuePtr += rowOffset;
                if (nullMap == nullptr) {
                    AddCentralMomentUseRowIndex<Value, SkewnessState::template UpdateState<Value> >(rowStates,
                        aggStateOffset,
                        valuePtr);
                } else {
                    AddCentralMomentConditionalUseRowIndex<Value, SkewnessState::template UpdateStateWithCondition<Value
                        , false> >(
                        rowStates, aggStateOffset, valuePtr, *nullMap);
                }
            } else {
                auto *countVector = this->curVectorBatch->Get(this->channels[0]);
                auto *m1Vector = this->curVectorBatch->Get(this->channels[1]);
                auto *m2Vector = this->curVectorBatch->Get(this->channels[2]);
                auto *m3Vector = this->curVectorBatch->Get(this->channels[3]);


                auto *countPtr = reinterpret_cast<MomentValue *>(VectorHelper::GetFlatValuePtr<OMNI_DOUBLE>(countVector));
                auto *m1Ptr = reinterpret_cast<MomentValue *>(VectorHelper::GetFlatValuePtr<OMNI_DOUBLE>(m1Vector));
                auto *m2Ptr = reinterpret_cast<MomentValue *>(VectorHelper::GetFlatValuePtr<OMNI_DOUBLE>(m2Vector));
                auto *m3Ptr = reinterpret_cast<MomentValue *>(VectorHelper::GetFlatValuePtr<OMNI_DOUBLE>(m3Vector));
                countPtr += rowOffset;
                m1Ptr += rowOffset;
                m2Ptr += rowOffset;
                m3Ptr += rowOffset;

                double m4 = 0.0;
                double *m4Ptr = &m4;
                if (nullMap == nullptr) {
                    MergeCentralMomentUseRowIndex<MomentValue, SkewnessState, AggValueState, MergeCentralMomentOp<
                        MomentValue> >(
                        rowStates, aggStateOffset, countPtr, m1Ptr, m2Ptr, m3Ptr, m4Ptr);
                } else {
                    MergeCentralMomentConditionalUseRowIndex<MomentValue, SkewnessState, AggValueState,
                        MergeCentralMomentConditionalOp<MomentValue, false> >(
                        rowStates, aggStateOffset, countPtr, m1Ptr, m2Ptr, m3Ptr, m4Ptr, *nullMap);
                }
            }
        }

        template<DataTypeId IN, DataTypeId OUT>
        void
        SkewnessAggregator<IN, OUT>::ProcessGroupUnspill(
            std::vector<UnspillRowInfo> &unspillRows, int32_t rowCount,
            int32_t &vectorIndex) {
            auto countVecIdx = vectorIndex++;
            auto centralMoment1VecIdx = vectorIndex++;
            auto centralMoment2VecIdx = vectorIndex++;
            auto centralMoment3VecIdx = vectorIndex++;
            for (int32_t rowIdx = 0; rowIdx < rowCount; rowIdx++) {
                auto &row = unspillRows[rowIdx];
                auto batch = row.batch;
                auto index = row.rowIdx;
                auto skewnessCountVector = static_cast<Vector<double> *>(batch->Get(countVecIdx));
                if (!skewnessCountVector->IsNull(index)) {
                    auto count = skewnessCountVector->GetValue(index);
                    auto centralMoment1Vector = static_cast<Vector<double> *>(batch->Get(centralMoment1VecIdx));
                    auto centralMoment1 = centralMoment1Vector->GetValue(index);
                    auto centralMoment2Vector = static_cast<Vector<double> *>(batch->Get(centralMoment2VecIdx));
                    auto centralMoment2 = centralMoment2Vector->GetValue(index);
                    auto centralMoment3Vector = static_cast<Vector<double> *>(batch->Get(centralMoment3VecIdx));
                    auto centralMoment3 = centralMoment3Vector->GetValue(index);
                    auto centralMoment4 = 0.0;
                    auto *pSkewnessState = SkewnessState::CastState(row.state + aggStateOffset);
                    auto &centralMomentState = pSkewnessState->centralMomentState;
                    MergeCentralMomentOp<MomentValue>(&centralMomentState.count, &centralMomentState.centralMoment1,
                                                      &centralMomentState.centralMoment2,
                                                      &centralMomentState.centralMoment3,
                                                      &centralMomentState.centralMoment4,
                                                      &centralMomentState.momentOrder,
                                                      pSkewnessState->valueState, count,
                                                      centralMoment1, centralMoment2, centralMoment3,
                                                      centralMoment4);
                }
            }
        }

        template<DataTypeId IN, DataTypeId OUT>
        void
        SkewnessAggregator<IN, OUT>::ProcessAlignAggSchema(
            VectorBatch *result, BaseVector *originVector,
            const std::shared_ptr<NullsHelper> nullMap, const bool aggFilter) {
            const int32_t rowCount = (originVector != nullptr) ? originVector->GetSize() : 0;
            if (rowCount == 0) {
                auto *countVector = static_cast<Vector<double> *>(VectorHelper::CreateFlatVector(this->pool_, OMNI_DOUBLE, 0));
                auto *m1Vector = static_cast<Vector<double> *>(VectorHelper::CreateFlatVector(this->pool_, OMNI_DOUBLE, 0));
                auto *m2Vector = static_cast<Vector<double> *>(VectorHelper::CreateFlatVector(this->pool_, OMNI_DOUBLE, 0));
                auto *m3Vector = static_cast<Vector<double> *>(VectorHelper::CreateFlatVector(this->pool_, OMNI_DOUBLE, 0));
                result->Append(countVector);
                result->Append(m1Vector);
                result->Append(m2Vector);
                result->Append(m3Vector);
                return;
            }
            if (!this->inputRaw) {
                throw std::runtime_error(
                    "Error in SkewnessAggregator ProcessAlignAggSchema: Skip partial only supported for raw input");
            }
            ProcessAlignAggSchemaInternal(result, originVector, nullMap);
        }

        template<DataTypeId IN, DataTypeId OUT>
        void SkewnessAggregator<IN, OUT>::ProcessAlignAggSchemaInternal(
            VectorBatch *result, BaseVector *originVector,
            const std::shared_ptr<NullsHelper> nullMap) {
            const int32_t rowCount = originVector->GetSize();
            auto *countVector = static_cast<Vector<double> *>(VectorHelper::CreateFlatVector(this->pool_, OMNI_DOUBLE, rowCount));
            auto *m1Vector = static_cast<Vector<double> *>(VectorHelper::CreateFlatVector(this->pool_, OMNI_DOUBLE, rowCount));
            auto *m2Vector = static_cast<Vector<double> *>(VectorHelper::CreateFlatVector(this->pool_, OMNI_DOUBLE, rowCount));
            auto *m3Vector = static_cast<Vector<double> *>(VectorHelper::CreateFlatVector(this->pool_, OMNI_DOUBLE, rowCount));

            auto valuePtr = reinterpret_cast<Value *>(VectorHelper::GetFlatValuePtr<IN>(originVector));
            for (int32_t i = 0; i < rowCount; ++i) {
                if (nullMap != nullptr && (*nullMap)[i]) {
                    countVector->SetValue(i, 0.0);
                    m1Vector->SetValue(i, 0.0);
                    m2Vector->SetValue(i, 0.0);
                    m3Vector->SetValue(i, 0.0);
                } else {
                    MomentValue m1 = static_cast<MomentValue>(valuePtr[i]);
                    countVector->SetValue(i, 1.0);
                    m1Vector->SetValue(i, m1);
                    m2Vector->SetValue(i, 0.0);
                    m3Vector->SetValue(i, 0.0);
                }
            }
            result->Append(countVector);
            result->Append(m1Vector);
            result->Append(m2Vector);
            result->Append(m3Vector);
        }

        template<DataTypeId IN, DataTypeId OUT>
        SkewnessAggregator<IN, OUT>::SkewnessAggregator(
            const DataTypes &inputTypes, const DataTypes &outputTypes,
            std::vector<int32_t> &channels, const bool inputRaw, const bool outputPartial,
            const bool isOverflowAsNull)
            : TypedAggregator(OMNI_AGGREGATION_TYPE_SKEWNESS, inputTypes, outputTypes, channels, inputRaw,
                              outputPartial,
                              isOverflowAsNull) {
        }

        // Explicit template instantiation
        // Defining templated aggregators in header file consume a lot of memory during compilation
        // since, compiler needs to generate each individual template instance wherever aggregator header is include
        // to reduce time and memory usage during compilation moved templated aggregator implementation into .cpp files
        // and used explicit template instantiation to generate template instances

        template
        class SkewnessAggregator<OMNI_DOUBLE, OMNI_DOUBLE>;
        template
        class SkewnessAggregator<OMNI_FLOAT, OMNI_DOUBLE>;
        template
        class SkewnessAggregator<OMNI_SHORT, OMNI_DOUBLE>;
        template
        class SkewnessAggregator<OMNI_INT, OMNI_DOUBLE>;
        template
        class SkewnessAggregator<OMNI_LONG, OMNI_DOUBLE>;
        template
        class SkewnessAggregator<OMNI_DOUBLE, OMNI_LONG>;
        template
        class SkewnessAggregator<OMNI_FLOAT, OMNI_LONG>;
        template
        class SkewnessAggregator<OMNI_SHORT, OMNI_LONG>;
        template
        class SkewnessAggregator<OMNI_INT, OMNI_LONG>;
        template
        class SkewnessAggregator<OMNI_LONG, OMNI_LONG>;
    }
}
