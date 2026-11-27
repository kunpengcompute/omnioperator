/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 */
#include "operator/aggregation/aggregator/regr/regr_replacement_aggregator.h"
#include "operator/aggregation/aggregator/regr/regr_align_schema_helper.h"
#include "operator/aggregation/aggregator/regr/regr_numeric.h"
#include "type/data_type.h"
#include "vector/vector_batch.h"

namespace omniruntime {
namespace op {

namespace {

using omniruntime::vec::BaseVector;
using omniruntime::vec::NullsHelper;
using omniruntime::vec::Vector;
using omniruntime::vec::VectorBatch;

int64_t ReadMergeNAt(BaseVector *nVec, int32_t row)
{
    if (nVec->GetTypeId() == OMNI_LONG) {
        return reinterpret_cast<int64_t *>(GetValuesFromVector<OMNI_LONG>(nVec))[row];
    }
    if (nVec->GetTypeId() == OMNI_INT) {
        return static_cast<int64_t>(reinterpret_cast<int32_t *>(GetValuesFromVector<OMNI_INT>(nVec))[row]);
    }
    if (nVec->GetTypeId() == OMNI_DOUBLE) {
        return static_cast<int64_t>(reinterpret_cast<double *>(GetValuesFromVector<OMNI_DOUBLE>(nVec))[row]);
    }
    return static_cast<int64_t>(RegrGetDoubleAt(nVec, row));
}

} // namespace

RegrReplacementAggregator::RegrReplacementAggregator(const DataTypes &inputTypes, const DataTypes &outputTypes,
    std::vector<int32_t> &channels, bool inputRaw, bool outputPartial, bool isOverflowAsNull)
    : TypedAggregator(OMNI_AGGREGATION_TYPE_REGR_REPLACEMENT, inputTypes, outputTypes, channels, inputRaw,
          outputPartial, isOverflowAsNull)
{
}

void RegrReplacementAggregator::ProcessSingleRaw(RegrReplacementState *acc, BaseVector *vec, int32_t rowOffset,
    int32_t rowCount, const std::shared_ptr<NullsHelper> &nullMap)
{
    auto *ptr = reinterpret_cast<double *>(GetValuesFromVector<OMNI_DOUBLE>(vec));
    ptr += rowOffset;
    for (int32_t i = 0; i < rowCount; i++) {
        if (nullMap != nullptr && (*nullMap)[i])
            continue;
        if (vec->IsNull(i + rowOffset))
            continue;
        double x = ptr[i];
        double oldAvg = acc->avg;
        acc->n += 1;
        acc->avg += (x - acc->avg) / static_cast<double>(acc->n);
        acc->m2 += (x - oldAvg) * (x - acc->avg);
    }
}

void RegrReplacementAggregator::ProcessSingleMerge(RegrReplacementState *acc, int32_t rowOffset, int32_t rowCount)
{
    auto *v0 = curVectorBatch->Get(channels[0]);
    auto *v1 = curVectorBatch->Get(channels[1]);
    auto *v2 = curVectorBatch->Get(channels[2]);
    auto *p1 = reinterpret_cast<double *>(GetValuesFromVector<OMNI_DOUBLE>(v1));
    auto *p2 = reinterpret_cast<double *>(GetValuesFromVector<OMNI_DOUBLE>(v2));
    for (int32_t i = 0; i < rowCount; i++) {
        int32_t row = i + rowOffset;
        if (v0->IsNull(row))
            continue;
        int64_t nIn = ReadMergeNAt(v0, row);
        RegrReplacementMerge(acc->n, acc->avg, acc->m2, nIn, p1[row], p2[row]);
    }
}

void RegrReplacementAggregator::ProcessSingleInternal(AggregateState *state, BaseVector *vector,
    const int32_t rowOffset, const int32_t rowCount, const std::shared_ptr<NullsHelper> nullMap)
{
    (void)vector;
    RegrReplacementState *acc = reinterpret_cast<RegrReplacementState *>(state);
    if (inputRaw) {
        BaseVector *vec = curVectorBatch->Get(channels[0]);
        ProcessSingleRaw(acc, vec, rowOffset, rowCount, nullMap);
    } else {
        ProcessSingleMerge(acc, rowOffset, rowCount);
    }
}

void RegrReplacementAggregator::ProcessGroupInternal(std::vector<AggregateState *> &rowStates, BaseVector *vector,
    const int32_t rowOffset, const std::shared_ptr<NullsHelper> nullMap)
{
    (void)vector;
    if (inputRaw) {
        BaseVector *vec = curVectorBatch->Get(channels[0]);
        auto *ptr = reinterpret_cast<double *>(GetValuesFromVector<OMNI_DOUBLE>(vec));
        ptr += rowOffset;
        size_t n = rowStates.size();
        for (size_t i = 0; i < n; i++) {
            if (nullMap != nullptr && (*nullMap)[i])
                continue;
            int32_t row = static_cast<int32_t>(i) + rowOffset;
            if (vec->IsNull(row))
                continue;
            RegrReplacementState *acc = reinterpret_cast<RegrReplacementState *>(rowStates[i] + aggStateOffset);
            double x = ptr[i];
            double oldAvg = acc->avg;
            acc->n += 1;
            acc->avg += (x - acc->avg) / static_cast<double>(acc->n);
            acc->m2 += (x - oldAvg) * (x - acc->avg);
        }
    } else {
        auto *v0 = curVectorBatch->Get(channels[0]);
        auto *v1 = curVectorBatch->Get(channels[1]);
        auto *v2 = curVectorBatch->Get(channels[2]);
        auto *p1 = reinterpret_cast<double *>(GetValuesFromVector<OMNI_DOUBLE>(v1)) + rowOffset;
        auto *p2 = reinterpret_cast<double *>(GetValuesFromVector<OMNI_DOUBLE>(v2)) + rowOffset;
        size_t n = rowStates.size();
        for (size_t i = 0; i < n; i++) {
            if (nullMap != nullptr && (*nullMap)[i])
                continue;
            int32_t row = static_cast<int32_t>(i) + rowOffset;
            if (v0->IsNull(row))
                continue;
            RegrReplacementState *acc = reinterpret_cast<RegrReplacementState *>(rowStates[i] + aggStateOffset);
            int64_t nIn = ReadMergeNAt(v0, row);
            RegrReplacementMerge(acc->n, acc->avg, acc->m2, nIn, p1[i], p2[i]);
        }
    }
}

void RegrReplacementAggregator::InitState(AggregateState *state)
{
    RegrReplacementState *acc = reinterpret_cast<RegrReplacementState *>(state + aggStateOffset);
    acc->n = 0;
    acc->avg = 0;
    acc->m2 = 0;
}

void RegrReplacementAggregator::InitStates(std::vector<AggregateState *> &groupStates)
{
    for (auto *s : groupStates) {
        InitState(s);
    }
}

void RegrReplacementAggregator::ExtractValues(const AggregateState *state, std::vector<BaseVector *> &vectors,
    int32_t rowIndex)
{
    const RegrReplacementState *s = reinterpret_cast<const RegrReplacementState *>(state + aggStateOffset);
    if (outputPartial) {
        if (vectors.size() < 3)
            return;
        if (s->IsEmpty()) {
            vectors[0]->SetNull(rowIndex);
            static_cast<Vector<double> *>(vectors[1])->SetNull(rowIndex);
            static_cast<Vector<double> *>(vectors[2])->SetNull(rowIndex);
            return;
        }
        static_cast<Vector<double> *>(vectors[0])->SetValue(rowIndex, static_cast<double>(s->n));
        static_cast<Vector<double> *>(vectors[1])->SetValue(rowIndex, s->avg);
        static_cast<Vector<double> *>(vectors[2])->SetValue(rowIndex, s->m2);
    } else {
        if (s->IsEmpty()) {
            vectors[0]->SetNull(rowIndex);
            return;
        }
        static_cast<Vector<double> *>(vectors[0])->SetValue(rowIndex, s->m2);
    }
}

void RegrReplacementAggregator::ExtractValuesBatch(std::vector<AggregateState *> &groupStates,
    std::vector<BaseVector *> &vectors, int32_t rowOffset, int32_t rowCount)
{
    if (outputPartial) {
        if (vectors.size() < 3)
            return;
        auto *v0 = static_cast<Vector<double> *>(vectors[0]);
        auto *v1 = static_cast<Vector<double> *>(vectors[1]);
        auto *v2 = static_cast<Vector<double> *>(vectors[2]);
        for (int32_t i = 0; i < rowCount; i++) {
            RegrReplacementState *s = reinterpret_cast<RegrReplacementState *>(groupStates[i] + aggStateOffset);
            int32_t row = rowOffset + i;
            if (s->IsEmpty()) {
                vectors[0]->SetNull(row);
                v1->SetNull(row);
                v2->SetNull(row);
            } else {
                v0->SetValue(row, static_cast<double>(s->n));
                v1->SetValue(row, s->avg);
                v2->SetValue(row, s->m2);
            }
        }
    } else {
        auto *outVec = static_cast<Vector<double> *>(vectors[0]);
        for (int32_t i = 0; i < rowCount; i++) {
            const RegrReplacementState *s =
                reinterpret_cast<const RegrReplacementState *>(groupStates[i] + aggStateOffset);
            int32_t row = rowOffset + i;
            if (s->IsEmpty())
                outVec->SetNull(row);
            else
                outVec->SetValue(row, s->m2);
        }
    }
}

void RegrReplacementAggregator::ExtractValuesForSpill(std::vector<AggregateState *> &groupStates,
    std::vector<BaseVector *> &vectors)
{
    auto *v0Base = vectors[0];
    auto *v1 = static_cast<Vector<double> *>(vectors[1]);
    auto *v2 = static_cast<Vector<double> *>(vectors[2]);
    int32_t rowCount = static_cast<int32_t>(groupStates.size());
    for (int32_t i = 0; i < rowCount; i++) {
        RegrReplacementState *s = reinterpret_cast<RegrReplacementState *>(groupStates[i] + aggStateOffset);
        if (s->IsEmpty()) {
            v0Base->SetNull(i);
            v1->SetNull(i);
            v2->SetNull(i);
        } else {
            if (v0Base->GetTypeId() == OMNI_LONG) {
                static_cast<Vector<int64_t> *>(v0Base)->SetValue(i, s->n);
            } else {
                static_cast<Vector<double> *>(v0Base)->SetValue(i, static_cast<double>(s->n));
            }
            v1->SetValue(i, s->avg);
            v2->SetValue(i, s->m2);
        }
    }
}

void RegrReplacementAggregator::ProcessGroupUnspill(std::vector<UnspillRowInfo> &unspillRows, int32_t rowCount,
    int32_t &vectorIndex)
{
    auto nIdx = vectorIndex++;
    auto avgIdx = vectorIndex++;
    auto m2Idx = vectorIndex++;
    for (int32_t rowIdx = 0; rowIdx < rowCount; rowIdx++) {
        auto &row = unspillRows[rowIdx];
        auto *batch = row.batch;
        auto index = row.rowIdx;
        auto *nVec = batch->Get(nIdx);
        if (nVec->IsNull(index))
            continue;
        int64_t n = ReadMergeNAt(nVec, index);
        auto *avgVec = static_cast<Vector<double> *>(batch->Get(avgIdx));
        auto *m2Vec = static_cast<Vector<double> *>(batch->Get(m2Idx));
        auto *acc = reinterpret_cast<RegrReplacementState *>(row.state + aggStateOffset);
        RegrReplacementMerge(acc->n, acc->avg, acc->m2, n, avgVec->GetValue(index), m2Vec->GetValue(index));
    }
}

void RegrReplacementAggregator::AlignAggSchema(VectorBatch *result, VectorBatch *inputVecBatch)
{
    int32_t rc = inputVecBatch->GetRowCount();
    if (rc == 0) {
        RegrAlignAppendEmptyReplacementPartial3(result, this->pool_);
        return;
    }
    if (!inputRaw) {
        RegrAlignAppendPartialSlices(result, inputVecBatch, channels, 3, rc);
        return;
    }
    std::shared_ptr<NullsHelper> nullMap;
    GetVector(inputVecBatch, 0, rc, &nullMap);
    BaseVector *valVec = inputVecBatch->Get(channels[0]);
    RegrAlignAppendReplacementPartial3RawDouble(result, valVec, nullMap, rc, this->pool_);
}

void RegrReplacementAggregator::AlignAggSchemaWithFilter(VectorBatch *result, VectorBatch *inputVecBatch,
    const int32_t filterIndex)
{
    int32_t rc = inputVecBatch->GetRowCount();
    if (rc == 0) {
        RegrAlignAppendEmptyReplacementPartial3(result, this->pool_);
        return;
    }
    auto *filterVec = static_cast<Vector<bool> *>(inputVecBatch->Get(filterIndex));
    bool needFilter = DoNeedHandleAggFilter(filterVec, 0, rc);
    std::shared_ptr<NullsHelper> baseNull;
    GetVector(inputVecBatch, 0, rc, &baseNull);
    std::shared_ptr<NullsHelper> rowSkip = RegrAlignMergeYxNullsWithFilter(baseNull, filterVec, needFilter, rc, this->pool_);
    if (!inputRaw) {
        RegrAlignAppendPartialColumnsWithSkip(result, inputVecBatch, channels, 3, rc, rowSkip, false, this->pool_);
        return;
    }
    BaseVector *valVec = inputVecBatch->Get(channels[0]);
    RegrAlignAppendReplacementPartial3RawDouble(result, valVec, rowSkip, rc, this->pool_);
}

void RegrReplacementAggregator::ProcessAlignAggSchema(VectorBatch *result, BaseVector *originVector,
    const std::shared_ptr<NullsHelper> nullMap, const bool aggFilter)
{
    (void)nullMap;
    (void)aggFilter;
    if (originVector == nullptr) {
        RegrAlignAppendEmptyReplacementPartial3(result, this->pool_);
        return;
    }
    RegrAlignAppendReplacementPartial3AllNullRows(result, originVector->GetSize(), this->pool_);
}

std::vector<DataTypePtr> RegrReplacementAggregator::GetSpillType()
{
    std::vector<DataTypePtr> types;
    types.push_back(std::make_shared<type::DataType>(type::DataTypeId::OMNI_LONG));
    types.push_back(std::make_shared<type::DataType>(type::DataTypeId::OMNI_DOUBLE));
    types.push_back(std::make_shared<type::DataType>(type::DataTypeId::OMNI_DOUBLE));
    return types;
}

} // namespace op
} // namespace omniruntime
