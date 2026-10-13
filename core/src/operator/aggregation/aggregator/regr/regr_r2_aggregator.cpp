/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 */
#include "operator/aggregation/aggregator/regr/regr_r2_aggregator.h"
#include "operator/aggregation/aggregator/regr/regr_align_schema_helper.h"
#include "operator/aggregation/aggregator/regr/regr_numeric.h"
#include "operator/aggregation/aggregator/regr/regr_state.h"
#include "operator/aggregation/vector_getter.h"
#include "operator/util/function_type.h"
#include "type/data_type.h"
#include "vector/vector.h"
#include "vector/vector_batch.h"

#include <cmath>
#include <string>

namespace {

using omniruntime::op::AggregateState;
using omniruntime::op::RegrGetDoubleAt;
using omniruntime::op::RegrMerge;
using omniruntime::op::RegrState;
using omniruntime::vec::BaseVector;
using omniruntime::vec::Vector;
using omniruntime::vec::VectorBatch;

// Merge/partial use Spark Pearson buffer only: 6 columns (n, xAvg, yAvg, ck, xMk, yMk). For regr_r2(y,x), xAvg/xMk
// are SQL y; map to Omni RegrState (meanX/m2X = SQL x, meanY/m2Y = SQL y).
static void MergeOnePearsonSix(BaseVector *cntVec, int32_t row, double *sparkXAvgPtr, double *sparkYAvgPtr, double *c2Ptr,
    double *sparkXMkPtr, double *sparkYMkPtr, RegrState *acc)
{
    if (cntVec->IsNull(row))
        return;
    if (cntVec->GetTypeId() == omniruntime::type::OMNI_LONG) {
        auto *cntPtr = reinterpret_cast<int64_t *>(omniruntime::op::GetValuesFromVector<omniruntime::type::OMNI_LONG>(cntVec));
        RegrMerge(acc->count, acc->meanX, acc->meanY, acc->c2, acc->m2X, acc->m2Y, cntPtr[row], sparkYAvgPtr[row],
            sparkXAvgPtr[row], c2Ptr[row], sparkYMkPtr[row], sparkXMkPtr[row]);
    } else if (cntVec->GetTypeId() == omniruntime::type::OMNI_INT) {
        auto *cntPtr = reinterpret_cast<int32_t *>(omniruntime::op::GetValuesFromVector<omniruntime::type::OMNI_INT>(cntVec));
        RegrMerge(acc->count, acc->meanX, acc->meanY, acc->c2, acc->m2X, acc->m2Y,
            static_cast<int64_t>(cntPtr[row]), sparkYAvgPtr[row], sparkXAvgPtr[row], c2Ptr[row], sparkYMkPtr[row],
            sparkXMkPtr[row]);
    } else {
        auto *cntPtr = reinterpret_cast<double *>(omniruntime::op::GetValuesFromVector<omniruntime::type::OMNI_DOUBLE>(cntVec));
        RegrMerge(acc->count, acc->meanX, acc->meanY, acc->c2, acc->m2X, acc->m2Y,
            static_cast<int64_t>(cntPtr[row]), sparkYAvgPtr[row], sparkXAvgPtr[row], c2Ptr[row], sparkYMkPtr[row],
            sparkXMkPtr[row]);
    }
}

static void AccumulateRawFullPair(RegrState *acc, BaseVector *yVec, BaseVector *xVec, int32_t rowOffset,
    int32_t rowCount, const std::shared_ptr<omniruntime::op::NullsHelper> &nullMap)
{
    for (int32_t i = 0; i < rowCount; i++) {
        if (nullMap != nullptr && (*nullMap)[i])
            continue;
        if (yVec->IsNull(i + rowOffset) || xVec->IsNull(i + rowOffset))
            continue;
        double y = RegrGetDoubleAt(yVec, i + rowOffset);
        double x = RegrGetDoubleAt(xVec, i + rowOffset);
        double oldMeanX = acc->meanX;
        double oldMeanY = acc->meanY;
        acc->count += 1;
        double deltaX = x - acc->meanX;
        acc->meanX += deltaX / acc->count;
        double deltaY = y - acc->meanY;
        acc->meanY += deltaY / acc->count;
        acc->c2 += deltaX * (y - acc->meanY);
        acc->m2X += (x - oldMeanX) * (x - acc->meanX);
        acc->m2Y += (y - oldMeanY) * (y - acc->meanY);
    }
}

static void AccumulateMergeBatch(
    RegrState *acc, VectorBatch *batch, const std::vector<int32_t> &channels, int32_t rowOffset, int32_t rowCount)
{
    const size_t nCol = channels.size();
    if (nCol == 6) {
        auto *cntVec = batch->Get(channels[0]);
        auto *meanXVec = batch->Get(channels[1]);
        auto *meanYVec = batch->Get(channels[2]);
        auto *c2Vec = batch->Get(channels[3]);
        auto *m2XVec = batch->Get(channels[4]);
        auto *m2YVec = batch->Get(channels[5]);
        auto *meanXPtr = reinterpret_cast<double *>(omniruntime::op::GetValuesFromVector<omniruntime::type::OMNI_DOUBLE>(meanXVec));
        auto *meanYPtr = reinterpret_cast<double *>(omniruntime::op::GetValuesFromVector<omniruntime::type::OMNI_DOUBLE>(meanYVec));
        auto *c2Ptr = reinterpret_cast<double *>(omniruntime::op::GetValuesFromVector<omniruntime::type::OMNI_DOUBLE>(c2Vec));
        auto *m2XPtr = reinterpret_cast<double *>(omniruntime::op::GetValuesFromVector<omniruntime::type::OMNI_DOUBLE>(m2XVec));
        auto *m2YPtr = reinterpret_cast<double *>(omniruntime::op::GetValuesFromVector<omniruntime::type::OMNI_DOUBLE>(m2YVec));
        for (int32_t i = 0; i < rowCount; i++) {
            int32_t row = i + rowOffset;
            MergeOnePearsonSix(cntVec, row, meanXPtr, meanYPtr, c2Ptr, m2XPtr, m2YPtr, acc);
        }
        return;
    }
    throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR",
        "Regr aggregate merge (Spark Pearson): expected 6 columns, got " + std::to_string(nCol));
}

static void GroupMergeOnePearsonSix(BaseVector *cntVec, int32_t row, double *sparkXAvgPtr, double *sparkYAvgPtr,
    double *c2Ptr, double *sparkXMkPtr, double *sparkYMkPtr, size_t idx, RegrState *acc)
{
    if (cntVec->IsNull(row))
        return;
    if (cntVec->GetTypeId() == omniruntime::type::OMNI_LONG) {
        auto *cntPtr = reinterpret_cast<int64_t *>(omniruntime::op::GetValuesFromVector<omniruntime::type::OMNI_LONG>(cntVec));
        RegrMerge(acc->count, acc->meanX, acc->meanY, acc->c2, acc->m2X, acc->m2Y, cntPtr[row], sparkYAvgPtr[idx],
            sparkXAvgPtr[idx], c2Ptr[idx], sparkYMkPtr[idx], sparkXMkPtr[idx]);
    } else if (cntVec->GetTypeId() == omniruntime::type::OMNI_INT) {
        auto *cntPtr = reinterpret_cast<int32_t *>(omniruntime::op::GetValuesFromVector<omniruntime::type::OMNI_INT>(cntVec));
        RegrMerge(acc->count, acc->meanX, acc->meanY, acc->c2, acc->m2X, acc->m2Y,
            static_cast<int64_t>(cntPtr[row]), sparkYAvgPtr[idx], sparkXAvgPtr[idx], c2Ptr[idx], sparkYMkPtr[idx],
            sparkXMkPtr[idx]);
    } else {
        auto *cntPtr = reinterpret_cast<double *>(omniruntime::op::GetValuesFromVector<omniruntime::type::OMNI_DOUBLE>(cntVec));
        RegrMerge(acc->count, acc->meanX, acc->meanY, acc->c2, acc->m2X, acc->m2Y,
            static_cast<int64_t>(cntPtr[row]), sparkYAvgPtr[idx], sparkXAvgPtr[idx], c2Ptr[idx], sparkYMkPtr[idx],
            sparkXMkPtr[idx]);
    }
}

static void AccumulateGroupRawFullPair(std::vector<AggregateState *> &rowStates, size_t aggStateOffset,
    BaseVector *yVec, BaseVector *xVec, int32_t rowOffset,
    const std::shared_ptr<omniruntime::op::NullsHelper> &nullMap)
{
    size_t n = rowStates.size();
    for (size_t i = 0; i < n; i++) {
        if (nullMap != nullptr && (*nullMap)[i])
            continue;
        int32_t row = static_cast<int32_t>(i) + rowOffset;
        if (yVec->IsNull(row) || xVec->IsNull(row))
            continue;
        RegrState *acc = reinterpret_cast<RegrState *>(rowStates[i] + aggStateOffset);
        double y = RegrGetDoubleAt(yVec, row);
        double x = RegrGetDoubleAt(xVec, row);
        double oldMeanX = acc->meanX;
        double oldMeanY = acc->meanY;
        acc->count += 1;
        double deltaX = x - acc->meanX;
        acc->meanX += deltaX / acc->count;
        double deltaY = y - acc->meanY;
        acc->meanY += deltaY / acc->count;
        acc->c2 += deltaX * (y - acc->meanY);
        acc->m2X += (x - oldMeanX) * (x - acc->meanX);
        acc->m2Y += (y - oldMeanY) * (y - acc->meanY);
    }
}

static void AccumulateGroupMergeBatch(std::vector<AggregateState *> &rowStates, size_t aggStateOffset,
    VectorBatch *batch, const std::vector<int32_t> &channels, int32_t rowOffset,
    const std::shared_ptr<omniruntime::op::NullsHelper> &nullMap)
{
    const size_t nCol = channels.size();
    size_t n = rowStates.size();
    if (nCol == 6) {
        auto *cntVec = batch->Get(channels[0]);
        auto *meanXVec = batch->Get(channels[1]);
        auto *meanYVec = batch->Get(channels[2]);
        auto *c2Vec = batch->Get(channels[3]);
        auto *m2XVec = batch->Get(channels[4]);
        auto *m2YVec = batch->Get(channels[5]);
        auto *meanXPtr = reinterpret_cast<double *>(omniruntime::op::GetValuesFromVector<omniruntime::type::OMNI_DOUBLE>(meanXVec)) + rowOffset;
        auto *meanYPtr = reinterpret_cast<double *>(omniruntime::op::GetValuesFromVector<omniruntime::type::OMNI_DOUBLE>(meanYVec)) + rowOffset;
        auto *c2Ptr = reinterpret_cast<double *>(omniruntime::op::GetValuesFromVector<omniruntime::type::OMNI_DOUBLE>(c2Vec)) + rowOffset;
        auto *m2XPtr = reinterpret_cast<double *>(omniruntime::op::GetValuesFromVector<omniruntime::type::OMNI_DOUBLE>(m2XVec)) + rowOffset;
        auto *m2YPtr = reinterpret_cast<double *>(omniruntime::op::GetValuesFromVector<omniruntime::type::OMNI_DOUBLE>(m2YVec)) + rowOffset;
        for (size_t i = 0; i < n; i++) {
            if (nullMap != nullptr && (*nullMap)[i])
                continue;
            int32_t row = static_cast<int32_t>(i) + rowOffset;
            RegrState *acc = reinterpret_cast<RegrState *>(rowStates[i] + aggStateOffset);
            GroupMergeOnePearsonSix(cntVec, row, meanXPtr, meanYPtr, c2Ptr, m2XPtr, m2YPtr, i, acc);
        }
        return;
    }
    throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR",
        "Regr aggregate merge (Spark Pearson): expected 6 columns, got " + std::to_string(nCol));
}

static void ExtractPartialRow(const omniruntime::type::DataTypes &outputTypes, const RegrState *s,
    std::vector<BaseVector *> &vectors, int32_t rowIndex)
{
    (void)outputTypes;
    if (vectors.size() < 6)
        return;
    BaseVector *nVecBase = vectors[0];
    auto *meanXVec = static_cast<Vector<double> *>(vectors[1]);
    auto *meanYVec = static_cast<Vector<double> *>(vectors[2]);
    auto *c2Vec = static_cast<Vector<double> *>(vectors[3]);
    auto *m2XVec = static_cast<Vector<double> *>(vectors[4]);
    auto *m2YVec = static_cast<Vector<double> *>(vectors[5]);
    if (s->IsEmpty()) {
        nVecBase->SetNull(rowIndex);
        meanXVec->SetNull(rowIndex);
        meanYVec->SetNull(rowIndex);
        c2Vec->SetNull(rowIndex);
        m2XVec->SetNull(rowIndex);
        m2YVec->SetNull(rowIndex);
        return;
    }
    if (nVecBase->GetTypeId() == omniruntime::type::OMNI_LONG) {
        static_cast<Vector<int64_t> *>(nVecBase)->SetValue(rowIndex, s->count);
    } else if (nVecBase->GetTypeId() == omniruntime::type::OMNI_INT) {
        static_cast<Vector<int32_t> *>(nVecBase)->SetValue(rowIndex, static_cast<int32_t>(s->count));
    } else {
        static_cast<Vector<double> *>(nVecBase)->SetValue(rowIndex, static_cast<double>(s->count));
    }
    meanXVec->SetValue(rowIndex, s->meanY);
    meanYVec->SetValue(rowIndex, s->meanX);
    c2Vec->SetValue(rowIndex, s->c2);
    m2XVec->SetValue(rowIndex, s->m2Y);
    m2YVec->SetValue(rowIndex, s->m2X);
}

/** True when this aggregate node emits the final scalar regr_r2(y,x) (one DOUBLE), not a struct / multi-column partial. */
static bool IsFinalScalarDoubleOutput(const omniruntime::type::DataTypes &outputTypes)
{
    return outputTypes.GetSize() == 1 && outputTypes.GetType(0)->GetId() == omniruntime::type::OMNI_DOUBLE;
}

/** Partial stage has six output columns (Pearson buffer); do not tie to outputTypes lest JNI lists LONG for n while vectors are DOUBLE. */
static bool CanExtractRegrPartialRow(const omniruntime::type::DataTypes &outputTypes, size_t nVectors)
{
    (void)outputTypes;
    return nVectors >= 6;
}

/** Single result column => final R² scalar; never write 6-column partial into one vector. */
static bool ShouldEmitRegrR2PartialColumns(bool outputPartial, const omniruntime::type::DataTypes &outputTypes,
    size_t nVectors)
{
    if (!outputPartial)
        return false;
    if (nVectors == 1)
        return false;
    return !IsFinalScalarDoubleOutput(outputTypes) && CanExtractRegrPartialRow(outputTypes, nVectors);
}

// Spark RegrR2: If(xMk==0, null, If(yMk==0, 1.0, corr^2)); Pearson(y,x): xMk=m2(y), yMk=m2(x) => Omni m2Y, m2X.
static void SetFinalRegrR2Value(const RegrState *s, BaseVector *outVec, int32_t rowIndex)
{
    if (s->IsEmpty()) {
        outVec->SetNull(rowIndex);
        return;
    }
    if (s->m2Y == 0.0) {
        outVec->SetNull(rowIndex);
        return;
    }
    if (s->m2X == 0.0) {
        static_cast<Vector<double> *>(outVec)->SetValue(rowIndex, 1.0);
        return;
    }
    double denom = s->m2X * s->m2Y;
    if (denom == 0.0 || !std::isfinite(denom)) {
        outVec->SetNull(rowIndex);
        return;
    }
    double result = (s->c2 * s->c2) / denom;
    static_cast<Vector<double> *>(outVec)->SetValue(rowIndex, result);
}

static void ExtractPartialBatch(const omniruntime::type::DataTypes &outputTypes,
    std::vector<AggregateState *> &groupStates, size_t aggStateOffset, std::vector<BaseVector *> &vectors,
    int32_t rowOffset, int32_t rowCount)
{
    (void)outputTypes;
    if (vectors.size() < 6)
        return;
    BaseVector *nVecBase = vectors[0];
    auto *meanXVec = static_cast<Vector<double> *>(vectors[1]);
    auto *meanYVec = static_cast<Vector<double> *>(vectors[2]);
    auto *c2Vec = static_cast<Vector<double> *>(vectors[3]);
    auto *m2XVec = static_cast<Vector<double> *>(vectors[4]);
    auto *m2YVec = static_cast<Vector<double> *>(vectors[5]);
    for (int32_t i = 0; i < rowCount; i++) {
        RegrState *st = reinterpret_cast<RegrState *>(groupStates[i] + aggStateOffset);
        int32_t row = rowOffset + i;
        if (st->IsEmpty()) {
            nVecBase->SetNull(row);
            meanXVec->SetNull(row);
            meanYVec->SetNull(row);
            c2Vec->SetNull(row);
            m2XVec->SetNull(row);
            m2YVec->SetNull(row);
        } else {
            if (nVecBase->GetTypeId() == omniruntime::type::OMNI_LONG) {
                static_cast<Vector<int64_t> *>(nVecBase)->SetValue(row, st->count);
            } else if (nVecBase->GetTypeId() == omniruntime::type::OMNI_INT) {
                static_cast<Vector<int32_t> *>(nVecBase)->SetValue(row, static_cast<int32_t>(st->count));
            } else {
                static_cast<Vector<double> *>(nVecBase)->SetValue(row, static_cast<double>(st->count));
            }
            meanXVec->SetValue(row, st->meanY);
            meanYVec->SetValue(row, st->meanX);
            c2Vec->SetValue(row, st->c2);
            m2XVec->SetValue(row, st->m2Y);
            m2YVec->SetValue(row, st->m2X);
        }
    }
}

} // namespace

namespace omniruntime {
namespace op {

RegrR2Aggregator::RegrR2Aggregator(const DataTypes &inputTypes, const DataTypes &outputTypes,
    std::vector<int32_t> &channels, bool inputRaw, bool outputPartial, bool isOverflowAsNull)
    : TypedAggregator(OMNI_AGGREGATION_TYPE_REGR_R2, inputTypes, outputTypes, channels, inputRaw, outputPartial,
          isOverflowAsNull)
{
}

size_t RegrR2Aggregator::GetStateSize()
{
    return sizeof(RegrState);
}

void RegrR2Aggregator::ProcessSingleInternal(AggregateState *state, BaseVector *vector, const int32_t rowOffset,
    const int32_t rowCount, const std::shared_ptr<NullsHelper> nullMap)
{
    // state is already (global buffer + aggStateOffset) from TypedAggregator / TypedMaskColAggregator.
    RegrState *acc = reinterpret_cast<RegrState *>(state);
    if (inputRaw) {
        BaseVector *yVec = curVectorBatch->Get(channels[0]);
        BaseVector *xVec = curVectorBatch->Get(channels[1]);
        ::AccumulateRawFullPair(acc, yVec, xVec, rowOffset, rowCount, nullMap);
    } else {
        ::AccumulateMergeBatch(acc, curVectorBatch, channels, rowOffset, rowCount);
    }
}

void RegrR2Aggregator::ProcessGroupInternal(std::vector<AggregateState *> &rowStates, BaseVector *vector,
    const int32_t rowOffset, const std::shared_ptr<NullsHelper> nullMap)
{
    if (inputRaw) {
        BaseVector *yVec = curVectorBatch->Get(channels[0]);
        BaseVector *xVec = curVectorBatch->Get(channels[1]);
        ::AccumulateGroupRawFullPair(rowStates, aggStateOffset, yVec, xVec, rowOffset, nullMap);
    } else {
        ::AccumulateGroupMergeBatch(rowStates, aggStateOffset, curVectorBatch, channels, rowOffset, nullMap);
    }
}

void RegrR2Aggregator::InitState(AggregateState *state)
{
    RegrState *acc = reinterpret_cast<RegrState *>(state + aggStateOffset);
    acc->count = 0;
    acc->meanX = 0;
    acc->meanY = 0;
    acc->c2 = 0;
    acc->m2X = 0;
    acc->m2Y = 0;
}

void RegrR2Aggregator::InitStates(std::vector<AggregateState *> &groupStates)
{
    for (auto *s : groupStates)
        InitState(s);
}

void RegrR2Aggregator::ExtractValues(const AggregateState *state, std::vector<BaseVector *> &vectors,
    const int32_t rowIndex)
{
    const RegrState *s = reinterpret_cast<const RegrState *>(state + aggStateOffset);
    const bool emitPartial =
        ShouldEmitRegrR2PartialColumns(outputPartial, outputTypes, vectors.size());
    if (emitPartial) {
        ExtractPartialRow(outputTypes, s, vectors, rowIndex);
        return;
    }
    BaseVector *outVec = vectors[0];
    SetFinalRegrR2Value(s, outVec, rowIndex);
}

void RegrR2Aggregator::ExtractValuesBatch(std::vector<AggregateState *> &groupStates,
    std::vector<BaseVector *> &vectors, int32_t rowOffset, int32_t rowCount)
{
    const bool emitPartial =
        ShouldEmitRegrR2PartialColumns(outputPartial, outputTypes, vectors.size());
    if (emitPartial) {
        ExtractPartialBatch(outputTypes, groupStates, aggStateOffset, vectors, rowOffset, rowCount);
        return;
    }
    auto *outVec = vectors[0];
    for (int32_t i = 0; i < rowCount; i++) {
        const RegrState *s = reinterpret_cast<const RegrState *>(groupStates[i] + aggStateOffset);
        int32_t row = rowOffset + i;
        SetFinalRegrR2Value(s, outVec, row);
    }
}

void RegrR2Aggregator::ExtractValuesForSpill(std::vector<AggregateState *> &groupStates,
    std::vector<BaseVector *> &vectors)
{
    auto *cntVec = static_cast<Vector<int64_t> *>(vectors[0]);
    auto *meanXVec = static_cast<Vector<double> *>(vectors[1]);
    auto *meanYVec = static_cast<Vector<double> *>(vectors[2]);
    auto *c2Vec = static_cast<Vector<double> *>(vectors[3]);
    auto *m2XVec = static_cast<Vector<double> *>(vectors[4]);
    auto *m2YVec = static_cast<Vector<double> *>(vectors[5]);
    int32_t rowCount = static_cast<int32_t>(groupStates.size());
    for (int32_t i = 0; i < rowCount; i++) {
        RegrState *st = reinterpret_cast<RegrState *>(groupStates[i] + aggStateOffset);
        if (st->IsEmpty()) {
            cntVec->SetNull(i);
            meanXVec->SetNull(i);
            meanYVec->SetNull(i);
            c2Vec->SetNull(i);
            m2XVec->SetNull(i);
            m2YVec->SetNull(i);
        } else {
            cntVec->SetValue(i, st->count);
            meanXVec->SetValue(i, st->meanX);
            meanYVec->SetValue(i, st->meanY);
            c2Vec->SetValue(i, st->c2);
            m2XVec->SetValue(i, st->m2X);
            m2YVec->SetValue(i, st->m2Y);
        }
    }
}

void RegrR2Aggregator::ProcessGroupUnspill(std::vector<UnspillRowInfo> &unspillRows, int32_t rowCount,
    int32_t &vectorIndex)
{
    auto cntIdx = vectorIndex++;
    auto meanXIdx = vectorIndex++;
    auto meanYIdx = vectorIndex++;
    auto c2Idx = vectorIndex++;
    auto m2XIdx = vectorIndex++;
    auto m2YIdx = vectorIndex++;
    for (int32_t rowIdx = 0; rowIdx < rowCount; rowIdx++) {
        auto &row = unspillRows[rowIdx];
        auto *batch = row.batch;
        auto index = row.rowIdx;
        auto *cntVec = static_cast<Vector<int64_t> *>(batch->Get(cntIdx));
        if (cntVec->IsNull(index))
            continue;
        int64_t count = cntVec->GetValue(index);
        auto *meanXVec = static_cast<Vector<double> *>(batch->Get(meanXIdx));
        auto *meanYVec = static_cast<Vector<double> *>(batch->Get(meanYIdx));
        auto *c2Vec = static_cast<Vector<double> *>(batch->Get(c2Idx));
        auto *m2XVec = static_cast<Vector<double> *>(batch->Get(m2XIdx));
        auto *m2YVec = static_cast<Vector<double> *>(batch->Get(m2YIdx));
        auto *acc = reinterpret_cast<RegrState *>(row.state + aggStateOffset);
        RegrMerge(acc->count, acc->meanX, acc->meanY, acc->c2, acc->m2X, acc->m2Y, count, meanXVec->GetValue(index),
            meanYVec->GetValue(index), c2Vec->GetValue(index), m2XVec->GetValue(index), m2YVec->GetValue(index));
    }
}

void RegrR2Aggregator::AlignAggSchema(VectorBatch *result, VectorBatch *inputVecBatch)
{
    int32_t rc = inputVecBatch->GetRowCount();
    if (rc == 0) {
        RegrAlignAppendEmptyPearson6(result, this->pool_);
        return;
    }
    if (!inputRaw) {
        RegrAlignAppendPartialSlices(result, inputVecBatch, channels, 6, rc);
        return;
    }
    std::shared_ptr<NullsHelper> yxNull;
    GetVector(inputVecBatch, 0, rc, &yxNull);
    BaseVector *yVec = inputVecBatch->Get(channels[0]);
    BaseVector *xVec = inputVecBatch->Get(channels[1]);
    RegrAlignAppendPearson6RawR2Layout(result, yVec, xVec, yxNull, rc, this->pool_);
}

void RegrR2Aggregator::AlignAggSchemaWithFilter(VectorBatch *result, VectorBatch *inputVecBatch,
    const int32_t filterIndex)
{
    int32_t rc = inputVecBatch->GetRowCount();
    if (rc == 0) {
        RegrAlignAppendEmptyPearson6(result, this->pool_);
        return;
    }
    auto *filterVec = static_cast<Vector<bool> *>(inputVecBatch->Get(filterIndex));
    bool needFilter = DoNeedHandleAggFilter(filterVec, 0, rc);
    std::shared_ptr<NullsHelper> yxNull;
    GetVector(inputVecBatch, 0, rc, &yxNull);
    std::shared_ptr<NullsHelper> rowSkip = RegrAlignMergeYxNullsWithFilter(yxNull, filterVec, needFilter, rc, this->pool_);
    if (!inputRaw) {
        RegrAlignAppendPartialColumnsWithSkip(result, inputVecBatch, channels, 6, rc, rowSkip, false, this->pool_);
        return;
    }
    BaseVector *yVec = inputVecBatch->Get(channels[0]);
    BaseVector *xVec = inputVecBatch->Get(channels[1]);
    RegrAlignAppendPearson6RawR2Layout(result, yVec, xVec, rowSkip, rc, this->pool_);
}

void RegrR2Aggregator::ProcessAlignAggSchema(VectorBatch *result, BaseVector *originVector,
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

std::vector<DataTypePtr> RegrR2Aggregator::GetSpillType()
{
    std::vector<DataTypePtr> types;
    types.push_back(std::make_shared<type::DataType>(type::DataTypeId::OMNI_LONG));
    types.push_back(std::make_shared<type::DataType>(type::DataTypeId::OMNI_DOUBLE));
    types.push_back(std::make_shared<type::DataType>(type::DataTypeId::OMNI_DOUBLE));
    types.push_back(std::make_shared<type::DataType>(type::DataTypeId::OMNI_DOUBLE));
    types.push_back(std::make_shared<type::DataType>(type::DataTypeId::OMNI_DOUBLE));
    types.push_back(std::make_shared<type::DataType>(type::DataTypeId::OMNI_DOUBLE));
    return types;
}

} // namespace op
} // namespace omniruntime
