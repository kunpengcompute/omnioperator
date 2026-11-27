/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Corr aggregate - Partial (6 values) + Final (1 double)
 */

#include "corr_aggregator.h"
#include "operator/aggregation/aggregator/regr/regr_align_schema_helper.h"
#include "operator/aggregation/vector_getter.h"

namespace omniruntime {
namespace op {

template <DataTypeId IN_ID, DataTypeId OUT_ID>
std::unique_ptr<Aggregator> CorrAggregator<IN_ID, OUT_ID>::Create(const DataTypes &inputTypes,
    const DataTypes &outputTypes, std::vector<int32_t> &channels, bool rawIn, bool partialOut, bool isOverflowAsNull) {
    constexpr bool kInSupported = (IN_ID == OMNI_SHORT || IN_ID == OMNI_INT || IN_ID == OMNI_LONG ||
        IN_ID == OMNI_FLOAT || IN_ID == OMNI_DOUBLE || IN_ID == OMNI_DECIMAL64 || IN_ID == OMNI_DECIMAL128 ||
        IN_ID == OMNI_CONTAINER);
    constexpr bool kOutSupported = (OUT_ID == OMNI_DOUBLE || OUT_ID == OMNI_CONTAINER);
    if constexpr (!kInSupported) {
        LogError("Error in corr aggregator: Unsupported input type %s", TypeUtil::TypeToStringLog(IN_ID).c_str());
        return nullptr;
    }
    if constexpr (!kOutSupported) {
        LogError("Error in corr aggregator: Unsupported output type %s", TypeUtil::TypeToStringLog(OUT_ID).c_str());
        return nullptr;
    }
    if (IN_ID != OMNI_CONTAINER && (inputTypes.GetSize() != 2 || !TypedAggregator::CheckTypes("corr", inputTypes, outputTypes, IN_ID, OUT_ID)))
        return nullptr;
    // Merge: Gluten sends 6 RAW Double columns only.
    if (IN_ID == OMNI_CONTAINER && inputTypes.GetSize() != kCorrPartialColumnCount)
        return nullptr;
    return std::unique_ptr<CorrAggregator<IN_ID, OUT_ID>>(
        new CorrAggregator<IN_ID, OUT_ID>(inputTypes, outputTypes, channels, rawIn, partialOut, isOverflowAsNull));
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void CorrAggregator<IN_ID, OUT_ID>::ExtractValues(const AggregateState *state, std::vector<BaseVector *> &vectors,
    int32_t rowIndex) {
    auto *s = CorrPartialState::ConstCastState(state + aggStateOffset);
    if (s->IsEmpty()) {
        if (outputPartial) {
            if (vectors.size() >= kCorrPartialColumnCount) {
                for (int i = 0; i < kCorrPartialColumnCount; i++) {
                    vectors[i]->SetNull(rowIndex);
                }
            } else {
                auto *vec = static_cast<ContainerVector *>(vectors[0]);
                for (int i = 0; i < kCorrPartialColumnCount; i++) {
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
            if (vectors.size() >= kCorrPartialColumnCount) {
                for (int i = 0; i < kCorrPartialColumnCount; i++) {
                    vectors[i]->SetNull(rowIndex);
                }
            } else {
                auto *vec = static_cast<ContainerVector *>(vectors[0]);
                for (int i = 0; i < kCorrPartialColumnCount; i++) {
                    reinterpret_cast<Vector<double> *>(vec->GetValue(i))->SetNull(rowIndex);
                }
            }
            this->SetNullOrThrowException(vectors[0], rowIndex, "corr_aggregator overflow.");
        } else {
            this->SetNullOrThrowException(vectors[0], rowIndex, "corr_aggregator overflow.");
        }
        return;
    }
    if (outputPartial) {
        // Spark order: (n, xAvg, yAvg, ck, xMk, yMk) all Double
        double n = static_cast<double>(s->count);
        double xAvg = (n > 0) ? (s->sum_x / n) : 0.0;
        double yAvg = (n > 0) ? (s->sum_y / n) : 0.0;
        double ck = (n > 0) ? (s->sum_xy - s->sum_x * s->sum_y / n) : 0.0;
        double xMk = (n > 0) ? (s->sum_xx - s->sum_x * s->sum_x / n) : 0.0;
        double yMk = (n > 0) ? (s->sum_yy - s->sum_y * s->sum_y / n) : 0.0;
        if (vectors.size() >= kCorrPartialColumnCount) {
            reinterpret_cast<Vector<double> *>(vectors[0])->SetValue(rowIndex, n);
            reinterpret_cast<Vector<double> *>(vectors[1])->SetValue(rowIndex, xAvg);
            reinterpret_cast<Vector<double> *>(vectors[2])->SetValue(rowIndex, yAvg);
            reinterpret_cast<Vector<double> *>(vectors[3])->SetValue(rowIndex, ck);
            reinterpret_cast<Vector<double> *>(vectors[4])->SetValue(rowIndex, xMk);
            reinterpret_cast<Vector<double> *>(vectors[5])->SetValue(rowIndex, yMk);
        } else {
            auto *vec = static_cast<ContainerVector *>(vectors[0]);
            reinterpret_cast<Vector<double> *>(vec->GetValue(0))->SetValue(rowIndex, n);
            reinterpret_cast<Vector<double> *>(vec->GetValue(1))->SetValue(rowIndex, xAvg);
            reinterpret_cast<Vector<double> *>(vec->GetValue(2))->SetValue(rowIndex, yAvg);
            reinterpret_cast<Vector<double> *>(vec->GetValue(3))->SetValue(rowIndex, ck);
            reinterpret_cast<Vector<double> *>(vec->GetValue(4))->SetValue(rowIndex, xMk);
            reinterpret_cast<Vector<double> *>(vec->GetValue(5))->SetValue(rowIndex, yMk);
        }
    } else {
        // Spark: corr has no result when count < 2 (single row or empty); return NULL.
        if (s->count < 2) {
            static_cast<OutVector *>(vectors[0])->SetNull(rowIndex);
            return;
        }
        double corr = CorrFromSuffStats(s->count, s->sum_x, s->sum_y, s->sum_xx, s->sum_yy, s->sum_xy);
        if (!std::isfinite(corr)) {
            this->SetNullOrThrowException(vectors[0], rowIndex, "corr_aggregator overflow.");
            return;
        }
        static_cast<OutVector *>(vectors[0])->SetValue(rowIndex, static_cast<OutType>(corr));
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void CorrAggregator<IN_ID, OUT_ID>::ExtractValuesBatch(std::vector<AggregateState *> &groupStates,
    std::vector<BaseVector *> &vectors, int32_t rowOffset, int32_t rowCount) {
    (void)rowOffset;
    if (outputPartial) {
        Vector<double> *vN = nullptr, *vXAvg = nullptr, *vYAvg = nullptr, *vCk = nullptr, *vXMk = nullptr, *vYMk = nullptr;
        if (vectors.size() >= kCorrPartialColumnCount) {
            vN = reinterpret_cast<Vector<double> *>(vectors[0]);
            vXAvg = reinterpret_cast<Vector<double> *>(vectors[1]);
            vYAvg = reinterpret_cast<Vector<double> *>(vectors[2]);
            vCk = reinterpret_cast<Vector<double> *>(vectors[3]);
            vXMk = reinterpret_cast<Vector<double> *>(vectors[4]);
            vYMk = reinterpret_cast<Vector<double> *>(vectors[5]);
        } else {
            auto *vec = static_cast<ContainerVector *>(vectors[0]);
            vN = reinterpret_cast<Vector<double> *>(vec->GetValue(0));
            vXAvg = reinterpret_cast<Vector<double> *>(vec->GetValue(1));
            vYAvg = reinterpret_cast<Vector<double> *>(vec->GetValue(2));
            vCk = reinterpret_cast<Vector<double> *>(vec->GetValue(3));
            vXMk = reinterpret_cast<Vector<double> *>(vec->GetValue(4));
            vYMk = reinterpret_cast<Vector<double> *>(vec->GetValue(5));
        }
        for (int32_t i = 0; i < rowCount; i++) {
            auto *s = CorrPartialState::CastState(groupStates[i] + aggStateOffset);
            if (s->IsEmpty()) {
                vN->SetNull(i); vXAvg->SetNull(i); vYAvg->SetNull(i);
                vCk->SetNull(i); vXMk->SetNull(i); vYMk->SetNull(i);
            } else if (s->IsOverFlowed()) {
                vN->SetNull(i); vXAvg->SetNull(i); vYAvg->SetNull(i);
                vCk->SetNull(i); vXMk->SetNull(i); vYMk->SetNull(i);
                this->SetNullOrThrowException(vectors.size() >= kCorrPartialColumnCount ? vectors[0] : reinterpret_cast<BaseVector *>(vN), i, "corr_aggregator overflow.");
            } else {
                double n = static_cast<double>(s->count);
                double xAvg = s->sum_x / n, yAvg = s->sum_y / n;
                vN->SetValue(i, n);
                vXAvg->SetValue(i, xAvg);
                vYAvg->SetValue(i, yAvg);
                vCk->SetValue(i, s->sum_xy - s->sum_x * s->sum_y / n);
                vXMk->SetValue(i, s->sum_xx - s->sum_x * s->sum_x / n);
                vYMk->SetValue(i, s->sum_yy - s->sum_y * s->sum_y / n);
            }
        }
    } else {
        auto *v = static_cast<OutVector *>(vectors[0]);
        for (int32_t i = 0; i < rowCount; i++) {
            auto *s = CorrPartialState::CastState(groupStates[i] + aggStateOffset);
            if (s->IsEmpty()) {
                v->SetNull(i);
            } else if (s->count < 2) {
                v->SetNull(i); // Spark: corr has no result when count < 2
            } else if (s->IsOverFlowed()) {
                this->SetNullOrThrowException(v, i, "corr_aggregator overflow.");
            } else {
                double corr = CorrFromSuffStats(s->count, s->sum_x, s->sum_y, s->sum_xx, s->sum_yy, s->sum_xy);
                if (!std::isfinite(corr)) {
                    this->SetNullOrThrowException(v, i, "corr_aggregator overflow.");
                } else {
                    v->SetValue(i, static_cast<OutType>(corr));
                }
            }
        }
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
std::vector<DataTypePtr> CorrAggregator<IN_ID, OUT_ID>::GetSpillType() {
    std::vector<DataTypePtr> spillTypes;
    for (int i = 0; i < kCorrPartialColumnCount; i++) {
        spillTypes.emplace_back(std::make_shared<DataType>(OMNI_DOUBLE)); // Spark order: n, xAvg, yAvg, ck, xMk, yMk
    }
    return spillTypes;
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void CorrAggregator<IN_ID, OUT_ID>::ExtractValuesForSpill(std::vector<AggregateState *> &groupStates,
    std::vector<BaseVector *> &vectors) {
    const int32_t rowCount = static_cast<int32_t>(groupStates.size());
    auto *vN = reinterpret_cast<Vector<double> *>(vectors[0]);
    auto *vXAvg = reinterpret_cast<Vector<double> *>(vectors[1]);
    auto *vYAvg = reinterpret_cast<Vector<double> *>(vectors[2]);
    auto *vCk = reinterpret_cast<Vector<double> *>(vectors[3]);
    auto *vXMk = reinterpret_cast<Vector<double> *>(vectors[4]);
    auto *vYMk = reinterpret_cast<Vector<double> *>(vectors[5]);
    for (int32_t i = 0; i < rowCount; i++) {
        auto *s = CorrPartialState::CastState(groupStates[i] + aggStateOffset);
        if (s->IsEmpty()) {
            vN->SetNull(i);
            vXAvg->SetNull(i);
            vYAvg->SetNull(i);
            vCk->SetNull(i);
            vXMk->SetNull(i);
            vYMk->SetNull(i);
        } else if (s->IsOverFlowed()) {
            vN->SetNull(i);
            vXAvg->SetNull(i);
            vYAvg->SetNull(i);
            vCk->SetNull(i);
            vXMk->SetNull(i);
            vYMk->SetNull(i);
        } else {
            double n = static_cast<double>(s->count);
            double xAvg = s->sum_x / n;
            double yAvg = s->sum_y / n;
            vN->SetValue(i, n);
            vXAvg->SetValue(i, xAvg);
            vYAvg->SetValue(i, yAvg);
            vCk->SetValue(i, s->sum_xy - s->sum_x * s->sum_y / n);
            vXMk->SetValue(i, s->sum_xx - s->sum_x * s->sum_x / n);
            vYMk->SetValue(i, s->sum_yy - s->sum_y * s->sum_y / n);
        }
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void CorrAggregator<IN_ID, OUT_ID>::InitState(AggregateState *state) {
    auto *s = CorrPartialState::CastState(state + aggStateOffset);
    s->count = 0;
    s->sum_x = s->sum_y = s->sum_xx = s->sum_yy = s->sum_xy = 0;
    s->valueState = AggValueState::EMPTY_VALUE;
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void CorrAggregator<IN_ID, OUT_ID>::InitStates(std::vector<AggregateState *> &groupStates) {
    for (auto *st : groupStates)
        InitState(st);
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void CorrAggregator<IN_ID, OUT_ID>::ProcessSingleInternal(AggregateState *state, BaseVector *vector,
    const int32_t rowOffset, const int32_t rowCount, const std::shared_ptr<NullsHelper> nullMap) {
    auto *s = CorrPartialState::CastState(state);
    if (inputRaw) {
        auto *col1 = curVectorBatch->Get(channels[0]);
        auto *col2 = curVectorBatch->Get(channels[1]);
        bool col1Const = (col1->GetEncoding() == vec::OMNI_ENCODING_CONST);
        bool col2Const = (col2->GetEncoding() == vec::OMNI_ENCODING_CONST);
        if (col1Const || col2Const) {
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
                InType v1 = col1Const ? constVal1 : p1[i];
                InType v2 = col2Const ? constVal2 : p2[i];
                double x, y;
                if constexpr (std::is_same_v<InType, type::Decimal128>) {
                    x = static_cast<double>(type::Decimal128Wrapper(v1));
                    y = static_cast<double>(type::Decimal128Wrapper(v2));
                } else {
                    x = static_cast<double>(v1);
                    y = static_cast<double>(v2);
                }
                s->count += 1;
                s->sum_x += x; s->sum_y += y;
                s->sum_xx += x * x; s->sum_yy += y * y; s->sum_xy += x * y;
                if (!std::isfinite(s->sum_x) || !std::isfinite(s->sum_y) || !std::isfinite(s->sum_xx) ||
                    !std::isfinite(s->sum_yy) || !std::isfinite(s->sum_xy)) {
                    s->valueState = AggValueState::OVERFLOWED;
                    break;
                }
            }
        } else {
            auto *p1 = reinterpret_cast<InType *>(GetValuesFromVector<IN_ID>(col1));
            auto *p2 = reinterpret_cast<InType *>(GetValuesFromVector<IN_ID>(col2));
            p1 += rowOffset;
            p2 += rowOffset;
            std::shared_ptr<NullsHelper> nm = nullMap ? nullMap : std::make_shared<NullsHelper>(std::make_shared<NullsBuffer>(rowCount, nullptr, 0, allocator));
            CorrAccumulateRaw<InType, false>(s->count, s->sum_x, s->sum_y, s->sum_xx, s->sum_yy, s->sum_xy, s->valueState, p1, p2, rowCount, *nm);
        }
        if (s->count > 0 && s->valueState != AggValueState::OVERFLOWED) {
            s->valueState = AggValueState::NORMAL;
        }
    } else {
        // Merge: Gluten sends 6 RAW Double columns. Spark order: n, xAvg, yAvg, ck, xMk, yMk
        double *vN = reinterpret_cast<double *>(GetValuesFromVector<OMNI_DOUBLE>(curVectorBatch->Get(channels[0]))) + rowOffset;
        double *vXAvg = reinterpret_cast<double *>(GetValuesFromVector<OMNI_DOUBLE>(curVectorBatch->Get(channels[1]))) + rowOffset;
        double *vYAvg = reinterpret_cast<double *>(GetValuesFromVector<OMNI_DOUBLE>(curVectorBatch->Get(channels[2]))) + rowOffset;
        double *vCk = reinterpret_cast<double *>(GetValuesFromVector<OMNI_DOUBLE>(curVectorBatch->Get(channels[3]))) + rowOffset;
        double *vXMk = reinterpret_cast<double *>(GetValuesFromVector<OMNI_DOUBLE>(curVectorBatch->Get(channels[4]))) + rowOffset;
        double *vYMk = reinterpret_cast<double *>(GetValuesFromVector<OMNI_DOUBLE>(curVectorBatch->Get(channels[5]))) + rowOffset;
        BaseVector *nullCol = curVectorBatch->Get(channels[0]);
        for (int32_t i = 0; i < rowCount; i++) {
            if (nullCol->IsNull(rowOffset + i)) continue;
            double n = vN[i];
            if (n <= 0) continue;
            CorrPartialState other;
            other.count = static_cast<int64_t>(n);
            other.sum_x = n * vXAvg[i];
            other.sum_y = n * vYAvg[i];
            other.sum_xx = vXMk[i] + n * vXAvg[i] * vXAvg[i];
            other.sum_yy = vYMk[i] + n * vYAvg[i] * vYAvg[i];
            other.sum_xy = vCk[i] + n * vXAvg[i] * vYAvg[i];
            if (!std::isfinite(other.sum_x) || !std::isfinite(other.sum_y) || !std::isfinite(other.sum_xx) ||
                !std::isfinite(other.sum_yy) || !std::isfinite(other.sum_xy)) {
                other.valueState = AggValueState::OVERFLOWED;
            } else {
                other.valueState = AggValueState::NORMAL;
            }
            s->Merge(other);
        }
        if (s->count > 0) {
            s->valueState = AggValueState::NORMAL;
        }
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void CorrAggregator<IN_ID, OUT_ID>::ProcessGroupInternal(std::vector<AggregateState *> &rowStates, BaseVector *vector,
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
            auto *s = CorrPartialState::CastState(rowStates[i] + aggStateOffset);
            InType v1 = col1Const ? constVal1 : p1[i];
            InType v2 = col2Const ? constVal2 : p2[i];
            double x, y;
            if constexpr (std::is_same_v<InType, type::Decimal128>) {
                x = static_cast<double>(type::Decimal128Wrapper(v1));
                y = static_cast<double>(type::Decimal128Wrapper(v2));
            } else {
                x = static_cast<double>(v1);
                y = static_cast<double>(v2);
            }
            s->count += 1;
            s->sum_x += x; s->sum_y += y;
            s->sum_xx += x * x; s->sum_yy += y * y; s->sum_xy += x * y;
            if (!std::isfinite(s->sum_x) || !std::isfinite(s->sum_y) || !std::isfinite(s->sum_xx) ||
                !std::isfinite(s->sum_yy) || !std::isfinite(s->sum_xy)) {
                s->valueState = AggValueState::OVERFLOWED;
            } else {
                s->valueState = AggValueState::NORMAL;
            }
        }
    } else {
        // Merge: 6 RAW Double columns (Gluten). Spark order: n, xAvg, yAvg, ck, xMk, yMk
        double *vN = reinterpret_cast<double *>(GetValuesFromVector<OMNI_DOUBLE>(curVectorBatch->Get(channels[0]))) + rowOffset;
        double *vXAvg = reinterpret_cast<double *>(GetValuesFromVector<OMNI_DOUBLE>(curVectorBatch->Get(channels[1]))) + rowOffset;
        double *vYAvg = reinterpret_cast<double *>(GetValuesFromVector<OMNI_DOUBLE>(curVectorBatch->Get(channels[2]))) + rowOffset;
        double *vCk = reinterpret_cast<double *>(GetValuesFromVector<OMNI_DOUBLE>(curVectorBatch->Get(channels[3]))) + rowOffset;
        double *vXMk = reinterpret_cast<double *>(GetValuesFromVector<OMNI_DOUBLE>(curVectorBatch->Get(channels[4]))) + rowOffset;
        double *vYMk = reinterpret_cast<double *>(GetValuesFromVector<OMNI_DOUBLE>(curVectorBatch->Get(channels[5]))) + rowOffset;
        BaseVector *nullCol = curVectorBatch->Get(channels[0]);
        const size_t rowCount = rowStates.size();
        for (size_t i = 0; i < rowCount; i++) {
            if (nullCol->IsNull(rowOffset + i)) continue;
            double n = vN[i];
            if (n <= 0) continue;
            CorrPartialState other;
            other.count = static_cast<int64_t>(n);
            other.sum_x = n * vXAvg[i];
            other.sum_y = n * vYAvg[i];
            other.sum_xx = vXMk[i] + n * vXAvg[i] * vXAvg[i];
            other.sum_yy = vYMk[i] + n * vYAvg[i] * vYAvg[i];
            other.sum_xy = vCk[i] + n * vXAvg[i] * vYAvg[i];
            if (!std::isfinite(other.sum_x) || !std::isfinite(other.sum_y) || !std::isfinite(other.sum_xx) ||
                !std::isfinite(other.sum_yy) || !std::isfinite(other.sum_xy)) {
                other.valueState = AggValueState::OVERFLOWED;
            } else {
                other.valueState = AggValueState::NORMAL;
            }
            auto *s = CorrPartialState::CastState(rowStates[i] + aggStateOffset);
            s->Merge(other);
            if (s->count > 0) {
                s->valueState = AggValueState::NORMAL;
            }
        }
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void CorrAggregator<IN_ID, OUT_ID>::ProcessGroupUnspill(std::vector<UnspillRowInfo> &unspillRows, int32_t rowCount,
    int32_t &vectorIndex) {
    const int32_t vNIdx = vectorIndex++;
    const int32_t vXIdx = vectorIndex++;
    const int32_t vYIdx = vectorIndex++;
    const int32_t vCkIdx = vectorIndex++;
    const int32_t vXMkIdx = vectorIndex++;
    const int32_t vYMkIdx = vectorIndex++;
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
        auto *vXMk = static_cast<Vector<double> *>(batch->Get(vXMkIdx));
        auto *vYMk = static_cast<Vector<double> *>(batch->Get(vYMkIdx));
        CorrPartialState other;
        other.count = static_cast<int64_t>(nD);
        other.sum_x = nD * vX->GetValue(index);
        other.sum_y = nD * vY->GetValue(index);
        other.sum_xx = vXMk->GetValue(index) + nD * vX->GetValue(index) * vX->GetValue(index);
        other.sum_yy = vYMk->GetValue(index) + nD * vY->GetValue(index) * vY->GetValue(index);
        other.sum_xy = vCk->GetValue(index) + nD * vX->GetValue(index) * vY->GetValue(index);
        if (!std::isfinite(other.sum_x) || !std::isfinite(other.sum_y) || !std::isfinite(other.sum_xx) ||
            !std::isfinite(other.sum_yy) || !std::isfinite(other.sum_xy)) {
            other.valueState = AggValueState::OVERFLOWED;
        } else {
            other.valueState = AggValueState::NORMAL;
        }
        auto *s = CorrPartialState::CastState(row.state + aggStateOffset);
        s->Merge(other);
        if (s->count > 0 && !s->IsOverFlowed()) {
            s->valueState = AggValueState::NORMAL;
        }
    }
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void CorrAggregator<IN_ID, OUT_ID>::AlignAggSchema(VectorBatch *result, VectorBatch *inputVecBatch)
{
    int32_t rc = inputVecBatch->GetRowCount();
    if (rc == 0) {
        RegrAlignAppendEmptyPearson6(result, this->pool_);
        return;
    }
    if (!inputRaw) {
        RegrAlignAppendPartialSlices(result, inputVecBatch, channels, kCorrPartialColumnCount, rc);
        return;
    }
    std::shared_ptr<NullsHelper> xyNull;
    GetVector(inputVecBatch, 0, rc, &xyNull);
    BaseVector *xVec = inputVecBatch->Get(channels[0]);
    BaseVector *yVec = inputVecBatch->Get(channels[1]);
    // Pearson(x,y): helper args are (sparkY, sparkX) -> (channels[1], channels[0]).
    RegrAlignAppendPearson6RawStd(result, yVec, xVec, xyNull, rc, this->pool_);
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void CorrAggregator<IN_ID, OUT_ID>::AlignAggSchemaWithFilter(VectorBatch *result, VectorBatch *inputVecBatch,
    const int32_t filterIndex)
{
    int32_t rc = inputVecBatch->GetRowCount();
    if (rc == 0) {
        RegrAlignAppendEmptyPearson6(result, this->pool_);
        return;
    }
    auto *filterVec = static_cast<Vector<bool> *>(inputVecBatch->Get(filterIndex));
    bool needFilter = DoNeedHandleAggFilter(filterVec, 0, rc);
    std::shared_ptr<NullsHelper> xyNull;
    GetVector(inputVecBatch, 0, rc, &xyNull);
    std::shared_ptr<NullsHelper> rowSkip =
        RegrAlignMergeYxNullsWithFilter(xyNull, filterVec, needFilter, rc, this->pool_);
    if (!inputRaw) {
        RegrAlignAppendPartialColumnsWithSkip(result, inputVecBatch, channels, kCorrPartialColumnCount, rc, rowSkip,
            false, this->pool_);
        return;
    }
    BaseVector *xVec = inputVecBatch->Get(channels[0]);
    BaseVector *yVec = inputVecBatch->Get(channels[1]);
    RegrAlignAppendPearson6RawStd(result, yVec, xVec, rowSkip, rc, this->pool_);
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
void CorrAggregator<IN_ID, OUT_ID>::ProcessAlignAggSchema(VectorBatch *result, BaseVector *originVector,
    const std::shared_ptr<NullsHelper> nullMap, const bool aggFilter)
{
    (void)nullMap;
    (void)aggFilter;
    if (originVector == nullptr) {
        RegrAlignAppendEmptyPearson6(result, this->pool_);
        return;
    }
    RegrAlignAppendPearson6AllNullRows(result, originVector->GetSize(), this->pool_);
}

template <DataTypeId IN_ID, DataTypeId OUT_ID>
CorrAggregator<IN_ID, OUT_ID>::CorrAggregator(const DataTypes &inputTypes, const DataTypes &outputTypes,
    std::vector<int32_t> &channels, const bool inputRaw, const bool outputPartial, const bool isOverflowAsNull)
    : TypedAggregator(OMNI_AGGREGATION_TYPE_CORR, inputTypes, outputTypes, channels, inputRaw, outputPartial, isOverflowAsNull) {}

template class CorrAggregator<OMNI_SHORT, OMNI_DOUBLE>;
template class CorrAggregator<OMNI_INT, OMNI_DOUBLE>;
template class CorrAggregator<OMNI_LONG, OMNI_DOUBLE>;
template class CorrAggregator<OMNI_FLOAT, OMNI_DOUBLE>;
template class CorrAggregator<OMNI_DOUBLE, OMNI_DOUBLE>;
template class CorrAggregator<OMNI_DECIMAL64, OMNI_DOUBLE>;
template class CorrAggregator<OMNI_DECIMAL128, OMNI_DOUBLE>;
template class CorrAggregator<OMNI_CONTAINER, OMNI_DOUBLE>;
template class CorrAggregator<OMNI_CONTAINER, OMNI_CONTAINER>;
}
}
