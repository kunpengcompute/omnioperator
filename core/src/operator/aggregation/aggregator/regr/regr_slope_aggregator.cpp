/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 */
#include "operator/aggregation/aggregator/regr/regr_slope_aggregator.h"
#include "operator/aggregation/aggregator/regr/regr_align_schema_helper.h"
#include "operator/aggregation/aggregator/regr/regr_numeric.h"
#include "operator/aggregation/aggregator/regr/regr_state.h"
#include "type/data_type.h"
#include "vector/vector.h"
#include "vector/vector_batch.h"

#include <string>

namespace {

using omniruntime::op::AggregateState;
using omniruntime::op::RegrGetDoubleAt;
using omniruntime::op::RegrSlopeInterceptMergePartial;
using omniruntime::op::RegrSlopeInterceptState;
using omniruntime::op::SparkCovarianceUpdate;
using omniruntime::op::RegrVarPopUpdate;
using omniruntime::vec::BaseVector;
using omniruntime::vec::Vector;
using omniruntime::vec::VectorBatch;

static void MergeOneSlopeInterceptPartial(RegrSlopeInterceptState *acc, VectorBatch *batch,
    const std::vector<int32_t> &channels, int32_t row)
{
    auto *v0 = batch->Get(channels[0]);
    if (v0->IsNull(row))
        return;
    double nCov = RegrGetDoubleAt(v0, row);
    if (nCov == 0.0)
        return;
    double xA = RegrGetDoubleAt(batch->Get(channels[1]), row);
    double yA = RegrGetDoubleAt(batch->Get(channels[2]), row);
    double ck = RegrGetDoubleAt(batch->Get(channels[3]), row);
    double nVar = RegrGetDoubleAt(batch->Get(channels[4]), row);
    double vAvg = RegrGetDoubleAt(batch->Get(channels[5]), row);
    double vM2 = RegrGetDoubleAt(batch->Get(channels[6]), row);
    RegrSlopeInterceptMergePartial(acc, nCov, xA, yA, ck, nVar, vAvg, vM2);
}

static void AccumulateRawFullPair(RegrSlopeInterceptState *acc, BaseVector *yVec, BaseVector *xVec, int32_t rowOffset,
    int32_t rowCount, const std::shared_ptr<omniruntime::op::NullsHelper> &nullMap)
{
    for (int32_t i = 0; i < rowCount; i++) {
        if (nullMap != nullptr && (*nullMap)[i])
            continue;
        if (yVec->IsNull(i + rowOffset) || xVec->IsNull(i + rowOffset))
            continue;
        double y = RegrGetDoubleAt(yVec, i + rowOffset);
        double x = RegrGetDoubleAt(xVec, i + rowOffset);
        SparkCovarianceUpdate(acc->covN, acc->covXAvg, acc->covYAvg, acc->covCk, x, y);
        RegrVarPopUpdate(acc->varN, acc->varAvgX, acc->varM2X, x);
    }
}

static void AccumulateMergeBatch(
    RegrSlopeInterceptState *acc, VectorBatch *batch, const std::vector<int32_t> &channels, int32_t rowOffset,
    int32_t rowCount)
{
    const size_t nCol = channels.size();
    if (nCol == 7) {
        for (int32_t i = 0; i < rowCount; i++) {
            int32_t row = i + rowOffset;
            MergeOneSlopeInterceptPartial(acc, batch, channels, row);
        }
        return;
    }
    throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR",
        "regr_slope merge (Spark): expected 7 columns, got " + std::to_string(nCol));
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
        RegrSlopeInterceptState *acc = reinterpret_cast<RegrSlopeInterceptState *>(rowStates[i] + aggStateOffset);
        double y = RegrGetDoubleAt(yVec, row);
        double x = RegrGetDoubleAt(xVec, row);
        SparkCovarianceUpdate(acc->covN, acc->covXAvg, acc->covYAvg, acc->covCk, x, y);
        RegrVarPopUpdate(acc->varN, acc->varAvgX, acc->varM2X, x);
    }
}

static void AccumulateGroupMergeBatch(std::vector<AggregateState *> &rowStates, size_t aggStateOffset,
    VectorBatch *batch, const std::vector<int32_t> &channels, int32_t rowOffset,
    const std::shared_ptr<omniruntime::op::NullsHelper> &nullMap)
{
    const size_t nCol = channels.size();
    size_t n = rowStates.size();
    if (nCol == 7) {
        for (size_t i = 0; i < n; i++) {
            if (nullMap != nullptr && (*nullMap)[i])
                continue;
            int32_t row = static_cast<int32_t>(i) + rowOffset;
            RegrSlopeInterceptState *acc = reinterpret_cast<RegrSlopeInterceptState *>(rowStates[i] + aggStateOffset);
            MergeOneSlopeInterceptPartial(acc, batch, channels, row);
        }
        return;
    }
    throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR",
        "regr_slope merge (Spark): expected 7 columns, got " + std::to_string(nCol));
}

static void ExtractPartialRow(const omniruntime::type::DataTypes &outputTypes, const RegrSlopeInterceptState *s,
    std::vector<BaseVector *> &vectors, int32_t rowIndex)
{
    (void)outputTypes;
    if (vectors.size() < 7)
        return;
    auto *v0 = static_cast<Vector<double> *>(vectors[0]);
    auto *v1 = static_cast<Vector<double> *>(vectors[1]);
    auto *v2 = static_cast<Vector<double> *>(vectors[2]);
    auto *v3 = static_cast<Vector<double> *>(vectors[3]);
    auto *v4 = static_cast<Vector<double> *>(vectors[4]);
    auto *v5 = static_cast<Vector<double> *>(vectors[5]);
    auto *v6 = static_cast<Vector<double> *>(vectors[6]);
    if (s->IsEmpty()) {
        v0->SetNull(rowIndex);
        v1->SetNull(rowIndex);
        v2->SetNull(rowIndex);
        v3->SetNull(rowIndex);
        v4->SetNull(rowIndex);
        v5->SetNull(rowIndex);
        v6->SetNull(rowIndex);
        return;
    }
    v0->SetValue(rowIndex, s->covN);
    v1->SetValue(rowIndex, s->covXAvg);
    v2->SetValue(rowIndex, s->covYAvg);
    v3->SetValue(rowIndex, s->covCk);
    v4->SetValue(rowIndex, s->varN);
    v5->SetValue(rowIndex, s->varAvgX);
    v6->SetValue(rowIndex, s->varM2X);
}

static void ExtractPartialBatch(const omniruntime::type::DataTypes &outputTypes,
    std::vector<AggregateState *> &groupStates, size_t aggStateOffset, std::vector<BaseVector *> &vectors,
    int32_t rowOffset, int32_t rowCount)
{
    (void)outputTypes;
    if (vectors.size() < 7)
        return;
    auto *v0 = static_cast<Vector<double> *>(vectors[0]);
    auto *v1 = static_cast<Vector<double> *>(vectors[1]);
    auto *v2 = static_cast<Vector<double> *>(vectors[2]);
    auto *v3 = static_cast<Vector<double> *>(vectors[3]);
    auto *v4 = static_cast<Vector<double> *>(vectors[4]);
    auto *v5 = static_cast<Vector<double> *>(vectors[5]);
    auto *v6 = static_cast<Vector<double> *>(vectors[6]);
    for (int32_t i = 0; i < rowCount; i++) {
        RegrSlopeInterceptState *st =
            reinterpret_cast<RegrSlopeInterceptState *>(groupStates[i] + aggStateOffset);
        int32_t row = rowOffset + i;
        if (st->IsEmpty()) {
            v0->SetNull(row);
            v1->SetNull(row);
            v2->SetNull(row);
            v3->SetNull(row);
            v4->SetNull(row);
            v5->SetNull(row);
            v6->SetNull(row);
        } else {
            v0->SetValue(row, st->covN);
            v1->SetValue(row, st->covXAvg);
            v2->SetValue(row, st->covYAvg);
            v3->SetValue(row, st->covCk);
            v4->SetValue(row, st->varN);
            v5->SetValue(row, st->varAvgX);
            v6->SetValue(row, st->varM2X);
        }
    }
}

} // namespace

namespace omniruntime {
namespace op {

RegrSlopeAggregator::RegrSlopeAggregator(const DataTypes &inputTypes, const DataTypes &outputTypes,
    std::vector<int32_t> &channels, bool inputRaw, bool outputPartial, bool isOverflowAsNull)
    : TypedAggregator(OMNI_AGGREGATION_TYPE_REGR_SLOPE, inputTypes, outputTypes, channels, inputRaw, outputPartial,
          isOverflowAsNull)
{
}

size_t RegrSlopeAggregator::GetStateSize()
{
    return sizeof(RegrSlopeInterceptState);
}

void RegrSlopeAggregator::ProcessSingleInternal(AggregateState *state, BaseVector *vector, const int32_t rowOffset,
    const int32_t rowCount, const std::shared_ptr<NullsHelper> nullMap)
{
    RegrSlopeInterceptState *acc = reinterpret_cast<RegrSlopeInterceptState *>(state);
    if (inputRaw) {
        BaseVector *yVec = curVectorBatch->Get(channels[0]);
        BaseVector *xVec = curVectorBatch->Get(channels[1]);
        ::AccumulateRawFullPair(acc, yVec, xVec, rowOffset, rowCount, nullMap);
    } else {
        ::AccumulateMergeBatch(acc, curVectorBatch, channels, rowOffset, rowCount);
    }
}

void RegrSlopeAggregator::ProcessGroupInternal(std::vector<AggregateState *> &rowStates, BaseVector *vector,
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

void RegrSlopeAggregator::InitState(AggregateState *state)
{
    RegrSlopeInterceptState *acc = reinterpret_cast<RegrSlopeInterceptState *>(state + aggStateOffset);
    *acc = RegrSlopeInterceptState{};
}

void RegrSlopeAggregator::InitStates(std::vector<AggregateState *> &groupStates)
{
    for (auto *s : groupStates)
        InitState(s);
}

void RegrSlopeAggregator::ExtractValues(const AggregateState *state, std::vector<BaseVector *> &vectors,
    const int32_t rowIndex)
{
    const RegrSlopeInterceptState *s = reinterpret_cast<const RegrSlopeInterceptState *>(state + aggStateOffset);
    if (outputPartial) {
        ::ExtractPartialRow(outputTypes, s, vectors, rowIndex);
        return;
    }
    BaseVector *outVec = vectors[0];
    if (s->IsEmpty()) {
        outVec->SetNull(rowIndex);
        return;
    }
    if (s->varM2X != 0.0) {
        double result = s->covCk / s->varM2X;
        if (result == 0.0)
            result = 0.0;
        static_cast<Vector<double> *>(outVec)->SetValue(rowIndex, result);
    } else {
        outVec->SetNull(rowIndex);
    }
}

void RegrSlopeAggregator::ExtractValuesBatch(std::vector<AggregateState *> &groupStates,
    std::vector<BaseVector *> &vectors, int32_t rowOffset, int32_t rowCount)
{
    if (outputPartial) {
        ::ExtractPartialBatch(outputTypes, groupStates, aggStateOffset, vectors, rowOffset, rowCount);
        return;
    }
    auto *outVec = vectors[0];
    for (int32_t i = 0; i < rowCount; i++) {
        const RegrSlopeInterceptState *s =
            reinterpret_cast<const RegrSlopeInterceptState *>(groupStates[i] + aggStateOffset);
        int32_t row = rowOffset + i;
        if (s->IsEmpty()) {
            outVec->SetNull(row);
            continue;
        }
        if (s->varM2X != 0.0) {
            double result = s->covCk / s->varM2X;
            if (result == 0.0)
                result = 0.0;
            static_cast<Vector<double> *>(outVec)->SetValue(row, result);
        } else {
            outVec->SetNull(row);
        }
    }
}

void RegrSlopeAggregator::ExtractValuesForSpill(std::vector<AggregateState *> &groupStates,
    std::vector<BaseVector *> &vectors)
{
    int32_t rowCount = static_cast<int32_t>(groupStates.size());
    for (int32_t i = 0; i < rowCount; i++) {
        RegrSlopeInterceptState *st = reinterpret_cast<RegrSlopeInterceptState *>(groupStates[i] + aggStateOffset);
        auto *v0 = static_cast<Vector<double> *>(vectors[0]);
        auto *v1 = static_cast<Vector<double> *>(vectors[1]);
        auto *v2 = static_cast<Vector<double> *>(vectors[2]);
        auto *v3 = static_cast<Vector<double> *>(vectors[3]);
        auto *v4 = static_cast<Vector<double> *>(vectors[4]);
        auto *v5 = static_cast<Vector<double> *>(vectors[5]);
        auto *v6 = static_cast<Vector<double> *>(vectors[6]);
        if (st->IsEmpty()) {
            v0->SetNull(i);
            v1->SetNull(i);
            v2->SetNull(i);
            v3->SetNull(i);
            v4->SetNull(i);
            v5->SetNull(i);
            v6->SetNull(i);
        } else {
            v0->SetValue(i, st->covN);
            v1->SetValue(i, st->covXAvg);
            v2->SetValue(i, st->covYAvg);
            v3->SetValue(i, st->covCk);
            v4->SetValue(i, st->varN);
            v5->SetValue(i, st->varAvgX);
            v6->SetValue(i, st->varM2X);
        }
    }
}

void RegrSlopeAggregator::ProcessGroupUnspill(std::vector<UnspillRowInfo> &unspillRows, int32_t rowCount,
    int32_t &vectorIndex)
{
    int32_t idx0 = vectorIndex++;
    int32_t idx1 = vectorIndex++;
    int32_t idx2 = vectorIndex++;
    int32_t idx3 = vectorIndex++;
    int32_t idx4 = vectorIndex++;
    int32_t idx5 = vectorIndex++;
    int32_t idx6 = vectorIndex++;
    for (int32_t rowIdx = 0; rowIdx < rowCount; rowIdx++) {
        auto &row = unspillRows[rowIdx];
        auto *batch = row.batch;
        auto index = row.rowIdx;
        auto *v0 = batch->Get(idx0);
        if (v0->IsNull(index))
            continue;
        double nCov = RegrGetDoubleAt(v0, index);
        if (nCov == 0.0)
            continue;
        double xA = RegrGetDoubleAt(batch->Get(idx1), index);
        double yA = RegrGetDoubleAt(batch->Get(idx2), index);
        double ck = RegrGetDoubleAt(batch->Get(idx3), index);
        double nVar = RegrGetDoubleAt(batch->Get(idx4), index);
        double vAvg = RegrGetDoubleAt(batch->Get(idx5), index);
        double vM2 = RegrGetDoubleAt(batch->Get(idx6), index);
        auto *acc = reinterpret_cast<RegrSlopeInterceptState *>(row.state + aggStateOffset);
        RegrSlopeInterceptMergePartial(acc, nCov, xA, yA, ck, nVar, vAvg, vM2);
    }
}

void RegrSlopeAggregator::AlignAggSchema(VectorBatch *result, VectorBatch *inputVecBatch)
{
    int32_t rc = inputVecBatch->GetRowCount();
    if (rc == 0) {
        RegrAlignAppendEmptySlope7(result, this->pool_);
        return;
    }
    if (!inputRaw) {
        RegrAlignAppendPartialSlices(result, inputVecBatch, channels, 7, rc);
        return;
    }
    std::shared_ptr<NullsHelper> yxNull;
    GetVector(inputVecBatch, 0, rc, &yxNull);
    BaseVector *yVec = inputVecBatch->Get(channels[0]);
    BaseVector *xVec = inputVecBatch->Get(channels[1]);
    RegrAlignAppendSlope7Raw(result, yVec, xVec, yxNull, rc, this->pool_);
}

void RegrSlopeAggregator::AlignAggSchemaWithFilter(VectorBatch *result, VectorBatch *inputVecBatch,
    const int32_t filterIndex)
{
    int32_t rc = inputVecBatch->GetRowCount();
    if (rc == 0) {
        RegrAlignAppendEmptySlope7(result, this->pool_);
        return;
    }
    auto *filterVec = static_cast<Vector<bool> *>(inputVecBatch->Get(filterIndex));
    bool needFilter = DoNeedHandleAggFilter(filterVec, 0, rc);
    std::shared_ptr<NullsHelper> yxNull;
    GetVector(inputVecBatch, 0, rc, &yxNull);
    std::shared_ptr<NullsHelper> rowSkip = RegrAlignMergeYxNullsWithFilter(yxNull, filterVec, needFilter, rc, this->pool_);
    if (!inputRaw) {
        RegrAlignAppendPartialColumnsWithSkip(result, inputVecBatch, channels, 7, rc, rowSkip, false, this->pool_);
        return;
    }
    BaseVector *yVec = inputVecBatch->Get(channels[0]);
    BaseVector *xVec = inputVecBatch->Get(channels[1]);
    RegrAlignAppendSlope7Raw(result, yVec, xVec, rowSkip, rc, this->pool_);
}

void RegrSlopeAggregator::ProcessAlignAggSchema(VectorBatch *result, BaseVector *originVector,
    const std::shared_ptr<NullsHelper> nullMap, const bool aggFilter)
{
    (void)nullMap;
    (void)aggFilter;
    if (originVector == nullptr) {
        RegrAlignAppendEmptySlope7(result, this->pool_);
        return;
    }
    RegrAlignAppendSlope7AllNullRows(result, originVector->GetSize(), this->pool_);
}

std::vector<DataTypePtr> RegrSlopeAggregator::GetSpillType()
{
    std::vector<DataTypePtr> types;
    for (int i = 0; i < 7; ++i) {
        types.push_back(std::make_shared<type::DataType>(type::DataTypeId::OMNI_DOUBLE));
    }
    return types;
}

} // namespace op
} // namespace omniruntime
