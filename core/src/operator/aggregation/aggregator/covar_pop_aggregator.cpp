/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: CovarPop aggregate - Partial (4 values: c2, count, meanX, meanY) + Final (1 double)
 */

#include "covar_pop_aggregator.h"
#include "operator/aggregation/aggregator/regr/regr_align_schema_helper.h"
#include "operator/aggregation/vector_getter.h"

namespace omniruntime {
namespace op {

template <DataTypeId IN_ID, DataTypeId OUT_ID>
std::unique_ptr<Aggregator> CovarPopAggregator<IN_ID, OUT_ID>::Create(const DataTypes &inputTypes,
    const DataTypes &outputTypes, std::vector<int32_t> &channels, bool rawIn, bool partialOut, bool isOverflowAsNull) {
    constexpr bool kInSupported = (IN_ID == OMNI_DOUBLE || IN_ID == OMNI_CONTAINER);
    constexpr bool kOutSupported = (OUT_ID == OMNI_DOUBLE || OUT_ID == OMNI_CONTAINER);
    if constexpr (!kInSupported) {
        LogError("Error in covar_pop aggregator: Unsupported input type %s", TypeUtil::TypeToStringLog(IN_ID).c_str());
        return nullptr;
    }
    if constexpr (!kOutSupported) {
        LogError("Error in covar_pop aggregator: Unsupported output type %s", TypeUtil::TypeToStringLog(OUT_ID).c_str());
        return nullptr;
    }
    if (IN_ID != OMNI_CONTAINER && (inputTypes.GetSize() != 2 || !TypedAggregator::CheckTypes(
                                        "covar_pop", inputTypes, outputTypes, IN_ID, OUT_ID))) {
        return nullptr;
    }
    // Merge: Gluten sends 4 RAW Double columns only.
    if (IN_ID == OMNI_CONTAINER && inputTypes.GetSize() != kCovarPartialColumnCount) {
        return nullptr;
    }
    return std::unique_ptr<CovarPopAggregator<IN_ID, OUT_ID>>(
        new CovarPopAggregator<IN_ID, OUT_ID>(inputTypes, outputTypes, channels, rawIn, partialOut, isOverflowAsNull));
}

// Partial output order (Spark/Gluten/Velox): c2, count, meanX, meanY
template <DataTypeId IN_ID, DataTypeId OUT_ID>
void CovarPopAggregator<IN_ID, OUT_ID>::ExtractValues(const AggregateState *state, std::vector<BaseVector *> &vectors,
    int32_t rowIndex) {
    auto *s = CovarPartialState::ConstCastState(state + aggStateOffset);
    if (s->IsEmpty()) {
        if (outputPartial) {
            if (vectors.size() >= kCovarPartialColumnCount) {
                for (int i = 0; i < kCovarPartialColumnCount; i++) {
                    reinterpret_cast<Vector<double> *>(vectors[i])->SetNull(rowIndex);
                }
            } else {
                auto *vec = static_cast<ContainerVector *>(vectors[0]);
                for (int i = 0; i < kCovarPartialColumnCount; i++) {
                    reinterpret_cast<Vector<double> *>(vec->GetValue(i))->SetNull(rowIndex);
                }
            }
        } else {
            static_cast<OutVector *>(vectors[0])->SetNull(rowIndex);
        }
        return;
    }
    if (s->IsOverFlowed()) {
        if (outputPartial) {
            if (vectors.size() >= kCovarPartialColumnCount) {
                for (int i = 0; i < kCovarPartialColumnCount; i++) {
                    reinterpret_cast<Vector<double> *>(vectors[i])->SetNull(rowIndex);
                }
            } else {
                auto *vec = static_cast<ContainerVector *>(vectors[0]);
                for (int i = 0; i < kCovarPartialColumnCount; i++) {
                    reinterpret_cast<Vector<double> *>(vec->GetValue(i))->SetNull(rowIndex);
                }
            }
            this->SetNullOrThrowException(vectors[0], rowIndex, "covar_pop_aggregator overflow.");
        } else {
            this->SetNullOrThrowException(vectors[0], rowIndex, "covar_pop_aggregator overflow.");
        }
        return;
    }
    if (outputPartial) {
        // Spark order: (n, xAvg, yAvg, ck) all Double
        double n = static_cast<double>(s->count);
        if (vectors.size() >= kCovarPartialColumnCount) {
            reinterpret_cast<Vector<double> *>(vectors[0])->SetValue(rowIndex, n);
            reinterpret_cast<Vector<double> *>(vectors[1])->SetValue(rowIndex, s->meanX);
            reinterpret_cast<Vector<double> *>(vectors[2])->SetValue(rowIndex, s->meanY);
            reinterpret_cast<Vector<double> *>(vectors[3])->SetValue(rowIndex, s->c2);
        } else {
            auto *vec = static_cast<ContainerVector *>(vectors[0]);
            reinterpret_cast<Vector<double> *>(vec->GetValue(0))->SetValue(rowIndex, n);
            reinterpret_cast<Vector<double> *>(vec->GetValue(1))->SetValue(rowIndex, s->meanX);
            reinterpret_cast<Vector<double> *>(vec->GetValue(2))->SetValue(rowIndex, s->meanY);
            reinterpret_cast<Vector<double> *>(vec->GetValue(3))->SetValue(rowIndex, s->c2);
        }
    } else {
        double v = CovarPopFromState(s->count, s->meanX, s->meanY, s->c2);
        if (!std::isfinite(v)) {
            this->SetNullOrThrowException(vectors[0], rowIndex, "covar_pop_aggregator overflow.");
            return;
        }
        static_cast<OutVector *>(vectors[0])->SetValue(rowIndex, static_cast<OutType>(v));
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void CovarPopAggregator<IN_ID, OUT_ID>::ExtractValuesBatch(std::vector<AggregateState *> &groupStates,
    std::vector<BaseVector *> &vectors, int32_t rowOffset, int32_t rowCount) {
    (void)rowOffset;
    if (outputPartial) {
        Vector<double> *vN = nullptr, *vXAvg = nullptr, *vYAvg = nullptr, *vCk = nullptr;
        if (vectors.size() >= kCovarPartialColumnCount) {
            vN = reinterpret_cast<Vector<double> *>(vectors[0]);
            vXAvg = reinterpret_cast<Vector<double> *>(vectors[1]);
            vYAvg = reinterpret_cast<Vector<double> *>(vectors[2]);
            vCk = reinterpret_cast<Vector<double> *>(vectors[3]);
        } else {
            auto *vec = static_cast<ContainerVector *>(vectors[0]);
            vN = reinterpret_cast<Vector<double> *>(vec->GetValue(0));
            vXAvg = reinterpret_cast<Vector<double> *>(vec->GetValue(1));
            vYAvg = reinterpret_cast<Vector<double> *>(vec->GetValue(2));
            vCk = reinterpret_cast<Vector<double> *>(vec->GetValue(3));
        }
        for (int32_t i = 0; i < rowCount; i++) {
            auto *s = CovarPartialState::CastState(groupStates[i] + aggStateOffset);
            if (s->IsEmpty()) {
                vN->SetNull(i); vXAvg->SetNull(i); vYAvg->SetNull(i);
                vCk->SetNull(i);
            } else if (s->IsOverFlowed()) {
                vN->SetNull(i);
                vXAvg->SetNull(i);
                vYAvg->SetNull(i);
                vCk->SetNull(i);
                this->SetNullOrThrowException(
                    vectors.size() >= kCovarPartialColumnCount ? vectors[0] : reinterpret_cast<BaseVector *>(vN), i,
                    "covar_pop_aggregator overflow.");
            } else {
                vN->SetValue(i, static_cast<double>(s->count));
                vXAvg->SetValue(i, s->meanX);
                vYAvg->SetValue(i, s->meanY);
                vCk->SetValue(i, s->c2);
            }
        }
    } else {
        auto *v = static_cast<OutVector *>(vectors[0]);
        for (int32_t i = 0; i < rowCount; i++) {
            auto *s = CovarPartialState::CastState(groupStates[i] + aggStateOffset);
            if (s->IsEmpty()) {
                v->SetNull(i);
            } else if (s->IsOverFlowed()) {
                this->SetNullOrThrowException(v, i, "covar_pop_aggregator overflow.");
            } else {
                double val = CovarPopFromState(s->count, s->meanX, s->meanY, s->c2);
                if (!std::isfinite(val)) {
                    this->SetNullOrThrowException(v, i, "covar_pop_aggregator overflow.");
                } else {
                    v->SetValue(i, static_cast<OutType>(val));
                }
            }
        }
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
std::vector<DataTypePtr> CovarPopAggregator<IN_ID, OUT_ID>::GetSpillType() {
    std::vector<DataTypePtr> spillTypes;
    for (int i = 0; i < kCovarPartialColumnCount; i++) {
        spillTypes.emplace_back(std::make_shared<DataType>(OMNI_DOUBLE)); // Spark order: n, xAvg, yAvg, ck
    }
    return spillTypes;
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void CovarPopAggregator<IN_ID, OUT_ID>::ExtractValuesForSpill(std::vector<AggregateState *> &groupStates,
    std::vector<BaseVector *> &vectors) {
    const int32_t rowCount = static_cast<int32_t>(groupStates.size());
    auto *vN = reinterpret_cast<Vector<double> *>(vectors[0]);
    auto *vXAvg = reinterpret_cast<Vector<double> *>(vectors[1]);
    auto *vYAvg = reinterpret_cast<Vector<double> *>(vectors[2]);
    auto *vCk = reinterpret_cast<Vector<double> *>(vectors[3]);
    for (int32_t i = 0; i < rowCount; i++) {
        auto *s = CovarPartialState::CastState(groupStates[i] + aggStateOffset);
        if (s->IsEmpty()) {
            vN->SetNull(i);
            vXAvg->SetNull(i);
            vYAvg->SetNull(i);
            vCk->SetNull(i);
        } else if (s->IsOverFlowed()) {
            vN->SetNull(i);
            vXAvg->SetNull(i);
            vYAvg->SetNull(i);
            vCk->SetNull(i);
        } else {
            vN->SetValue(i, static_cast<double>(s->count));
            vXAvg->SetValue(i, s->meanX);
            vYAvg->SetValue(i, s->meanY);
            vCk->SetValue(i, s->c2);
        }
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void CovarPopAggregator<IN_ID, OUT_ID>::InitState(AggregateState *state) {
    auto *s = CovarPartialState::CastState(state + aggStateOffset);
    s->count = 0;
    s->meanX = s->meanY = s->c2 = 0;
    s->valueState = AggValueState::EMPTY_VALUE;
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void CovarPopAggregator<IN_ID, OUT_ID>::InitStates(std::vector<AggregateState *> &groupStates) {
    for (auto *st : groupStates)
        InitState(st);
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void CovarPopAggregator<IN_ID, OUT_ID>::ProcessSingleInternal(AggregateState *state, BaseVector *vector,
    const int32_t rowOffset, const int32_t rowCount, const std::shared_ptr<NullsHelper> nullMap) {
    auto *s = CovarPartialState::CastState(state);
    if (inputRaw) {
        auto *col1 = curVectorBatch->Get(channels[0]);
        auto *col2 = curVectorBatch->Get(channels[1]);
        bool col1Const = (col1->GetEncoding() == vec::OMNI_ENCODING_CONST);
        bool col2Const = (col2->GetEncoding() == vec::OMNI_ENCODING_CONST);
        InType constVal1{}, constVal2{};
        InType *p1 = nullptr, *p2 = nullptr;
        if (col1Const) {
            constVal1 = static_cast<vec::ConstVector<InType> *>(col1)->GetConstValue();
        } else {
            p1 = reinterpret_cast<InType *>(GetValuesFromVector<IN_ID>(col1)) + rowOffset;
        }
        if (col2Const) {
            constVal2 = static_cast<vec::ConstVector<InType> *>(col2)->GetConstValue();
        } else {
            p2 = reinterpret_cast<InType *>(GetValuesFromVector<IN_ID>(col2)) + rowOffset;
        }
        for (int32_t i = 0; i < rowCount; i++) {
            if (nullMap && (*nullMap)[i]) continue;
            if (col1->IsNull(rowOffset + i) || col2->IsNull(rowOffset + i)) continue;
            s->Update(col1Const ? constVal1 : p1[i], col2Const ? constVal2 : p2[i]);
        }
        if (s->count > 0 && !s->IsOverFlowed()) {
            s->valueState = AggValueState::NORMAL;
        }
    } else {
        // Merge: Gluten sends 4 RAW Double columns. Spark order: n, xAvg, yAvg, ck
        double *vN = reinterpret_cast<double *>(GetValuesFromVector<OMNI_DOUBLE>(curVectorBatch->Get(channels[0]))) + rowOffset;
        double *vXAvg = reinterpret_cast<double *>(GetValuesFromVector<OMNI_DOUBLE>(curVectorBatch->Get(channels[1]))) + rowOffset;
        double *vYAvg = reinterpret_cast<double *>(GetValuesFromVector<OMNI_DOUBLE>(curVectorBatch->Get(channels[2]))) + rowOffset;
        double *vCk = reinterpret_cast<double *>(GetValuesFromVector<OMNI_DOUBLE>(curVectorBatch->Get(channels[3]))) + rowOffset;
        BaseVector *nullCol = curVectorBatch->Get(channels[0]);
        for (int32_t i = 0; i < rowCount; i++) {
            if (nullCol->IsNull(rowOffset + i)) continue;
            double n = vN[i];
            if (n <= 0) continue;
            s->Merge(static_cast<int64_t>(n), vXAvg[i], vYAvg[i], vCk[i]);
        }
        if (s->count > 0 && !s->IsOverFlowed()) {
            s->valueState = AggValueState::NORMAL;
        }
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void CovarPopAggregator<IN_ID, OUT_ID>::ProcessGroupInternal(std::vector<AggregateState *> &rowStates, BaseVector *vector,
    const int32_t rowOffset, const std::shared_ptr<NullsHelper> nullMap) {
    if (inputRaw) {
        auto *col1 = curVectorBatch->Get(channels[0]);
        auto *col2 = curVectorBatch->Get(channels[1]);
        bool col1Const = (col1->GetEncoding() == vec::OMNI_ENCODING_CONST);
        bool col2Const = (col2->GetEncoding() == vec::OMNI_ENCODING_CONST);
        InType constVal1{}, constVal2{};
        InType *p1 = nullptr, *p2 = nullptr;
        if (col1Const) {
            constVal1 = static_cast<vec::ConstVector<InType> *>(col1)->GetConstValue();
        } else {
            p1 = reinterpret_cast<InType *>(GetValuesFromVector<IN_ID>(col1)) + rowOffset;
        }
        if (col2Const) {
            constVal2 = static_cast<vec::ConstVector<InType> *>(col2)->GetConstValue();
        } else {
            p2 = reinterpret_cast<InType *>(GetValuesFromVector<IN_ID>(col2)) + rowOffset;
        }
        const size_t rowCount = rowStates.size();
        for (size_t i = 0; i < rowCount; i++) {
            if (nullMap && (*nullMap)[i]) continue;
            if (col1->IsNull(rowOffset + i) || col2->IsNull(rowOffset + i)) continue;
            auto *s = CovarPartialState::CastState(rowStates[i] + aggStateOffset);
            s->Update(col1Const ? constVal1 : p1[i], col2Const ? constVal2 : p2[i]);
            if (!s->IsOverFlowed()) {
                s->valueState = AggValueState::NORMAL;
            }
        }
    } else {
        // Merge: 4 RAW Double columns (Gluten). Spark order: n, xAvg, yAvg, ck
        double *vN = reinterpret_cast<double *>(GetValuesFromVector<OMNI_DOUBLE>(curVectorBatch->Get(channels[0]))) + rowOffset;
        double *vXAvg = reinterpret_cast<double *>(GetValuesFromVector<OMNI_DOUBLE>(curVectorBatch->Get(channels[1]))) + rowOffset;
        double *vYAvg = reinterpret_cast<double *>(GetValuesFromVector<OMNI_DOUBLE>(curVectorBatch->Get(channels[2]))) + rowOffset;
        double *vCk = reinterpret_cast<double *>(GetValuesFromVector<OMNI_DOUBLE>(curVectorBatch->Get(channels[3]))) + rowOffset;
        BaseVector *nullCol = curVectorBatch->Get(channels[0]);
        const size_t rowCount = rowStates.size();
        for (size_t i = 0; i < rowCount; i++) {
            if (nullCol->IsNull(rowOffset + i)) continue;
            double n = vN[i];
            if (n <= 0) continue;
            auto *s = CovarPartialState::CastState(rowStates[i] + aggStateOffset);
            s->Merge(static_cast<int64_t>(n), vXAvg[i], vYAvg[i], vCk[i]);
            if (s->count > 0 && !s->IsOverFlowed()) {
                s->valueState = AggValueState::NORMAL;
            }
        }
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void CovarPopAggregator<IN_ID, OUT_ID>::ProcessGroupUnspill(std::vector<UnspillRowInfo> &unspillRows, int32_t rowCount,
    int32_t &vectorIndex) {
    const int32_t vNIdx = vectorIndex++;
    const int32_t vXIdx = vectorIndex++;
    const int32_t vYIdx = vectorIndex++;
    const int32_t vCkIdx = vectorIndex++;
    for (int32_t rowIdx = 0; rowIdx < rowCount; rowIdx++) {
        auto &row = unspillRows[rowIdx];
        VectorBatch *batch = row.batch;
        const int32_t index = row.rowIdx;
        auto *vN = static_cast<Vector<double> *>(batch->Get(vNIdx));
        if (vN->IsNull(index)) {
            continue;
        }
        const double nD = vN->GetValue(index);
        if (nD <= 0) {
            continue;
        }
        auto *vX = static_cast<Vector<double> *>(batch->Get(vXIdx));
        auto *vY = static_cast<Vector<double> *>(batch->Get(vYIdx));
        auto *vCk = static_cast<Vector<double> *>(batch->Get(vCkIdx));
        auto *s = CovarPartialState::CastState(row.state + aggStateOffset);
        s->Merge(static_cast<int64_t>(nD), vX->GetValue(index), vY->GetValue(index), vCk->GetValue(index));
        if (s->count > 0 && !s->IsOverFlowed()) {
            s->valueState = AggValueState::NORMAL;
        }
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void CovarPopAggregator<IN_ID, OUT_ID>::AlignAggSchema(VectorBatch *result, VectorBatch *inputVecBatch)
{
    int32_t rc = inputVecBatch->GetRowCount();
    if (rc == 0) {
        RegrAlignAppendEmptyCov4(result, this->pool_);
        return;
    }
    if (!inputRaw) {
        RegrAlignAppendPartialSlices(result, inputVecBatch, channels, kCovarPartialColumnCount, rc);
        return;
    }
    std::shared_ptr<NullsHelper> xyNull;
    GetVector(inputVecBatch, 0, rc, &xyNull);
    BaseVector *leftVec = inputVecBatch->Get(channels[0]);
    BaseVector *rightVec = inputVecBatch->Get(channels[1]);
    RegrAlignAppendCov4Raw(result, leftVec, rightVec, xyNull, rc, this->pool_);
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void CovarPopAggregator<IN_ID, OUT_ID>::AlignAggSchemaWithFilter(VectorBatch *result, VectorBatch *inputVecBatch,
    const int32_t filterIndex)
{
    int32_t rc = inputVecBatch->GetRowCount();
    if (rc == 0) {
        RegrAlignAppendEmptyCov4(result, this->pool_);
        return;
    }
    auto *filterVec = static_cast<Vector<bool> *>(inputVecBatch->Get(filterIndex));
    bool needFilter = DoNeedHandleAggFilter(filterVec, 0, rc);
    std::shared_ptr<NullsHelper> xyNull;
    GetVector(inputVecBatch, 0, rc, &xyNull);
    std::shared_ptr<NullsHelper> rowSkip =
        RegrAlignMergeYxNullsWithFilter(xyNull, filterVec, needFilter, rc, this->pool_);
    if (!inputRaw) {
        RegrAlignAppendPartialColumnsWithSkip(result, inputVecBatch, channels, kCovarPartialColumnCount, rc, rowSkip,
            false, this->pool_);
        return;
    }
    BaseVector *leftVec = inputVecBatch->Get(channels[0]);
    BaseVector *rightVec = inputVecBatch->Get(channels[1]);
    RegrAlignAppendCov4Raw(result, leftVec, rightVec, rowSkip, rc, this->pool_);
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void CovarPopAggregator<IN_ID, OUT_ID>::ProcessAlignAggSchema(VectorBatch *result, BaseVector *originVector,
    const std::shared_ptr<NullsHelper> nullMap, const bool aggFilter)
{
    (void)nullMap;
    (void)aggFilter;
    if (originVector == nullptr) {
        RegrAlignAppendEmptyCov4(result, this->pool_);
        return;
    }
    RegrAlignAppendCov4AllNullRows(result, originVector->GetSize(), this->pool_);
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
CovarPopAggregator<IN_ID, OUT_ID>::CovarPopAggregator(const DataTypes &inputTypes, const DataTypes &outputTypes,
    std::vector<int32_t> &channels, const bool inputRaw, const bool outputPartial, const bool isOverflowAsNull)
    : TypedAggregator(OMNI_AGGREGATION_TYPE_COVAR_POP, inputTypes, outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull) {}

template class CovarPopAggregator<OMNI_DOUBLE, OMNI_DOUBLE>;
template class CovarPopAggregator<OMNI_CONTAINER, OMNI_DOUBLE>;
template class CovarPopAggregator<OMNI_CONTAINER, OMNI_CONTAINER>;
}
}
