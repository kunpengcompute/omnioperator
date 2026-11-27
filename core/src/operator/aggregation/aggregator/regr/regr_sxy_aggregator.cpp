/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 */
#include "operator/aggregation/aggregator/regr/regr_sxy_aggregator.h"
#include "operator/aggregation/aggregator/regr/regr_align_schema_helper.h"
#include "operator/aggregation/aggregator/regr/regr_numeric.h"
#include "operator/aggregation/aggregator/regr/regr_state.h"
#include "type/data_type.h"
#include "vector/vector.h"
#include "vector/vector_batch.h"

#include <string>

namespace {

using omniruntime::op::AggregateState;
using omniruntime::op::RegrCov4State;
using omniruntime::op::RegrGetDoubleAt;
using omniruntime::op::SparkCovarianceMerge;
using omniruntime::op::SparkCovarianceUpdate;
using omniruntime::vec::BaseVector;
using omniruntime::vec::Vector;
using omniruntime::vec::VectorBatch;

static void MergeOneCov4Partial(RegrCov4State *acc, VectorBatch *batch, const std::vector<int32_t> &channels,
    int32_t row)
{
    auto *v0 = batch->Get(channels[0]);
    if (v0->IsNull(row))
        return;
    double nIn = RegrGetDoubleAt(v0, row);
    if (nIn == 0.0)
        return;
    double xA = RegrGetDoubleAt(batch->Get(channels[1]), row);
    double yA = RegrGetDoubleAt(batch->Get(channels[2]), row);
    double ck = RegrGetDoubleAt(batch->Get(channels[3]), row);
    SparkCovarianceMerge(acc->n, acc->xAvg, acc->yAvg, acc->ck, nIn, xA, yA, ck);
}

static void AccumulateRawFullPair(RegrCov4State *acc, BaseVector *yVec, BaseVector *xVec, int32_t rowOffset,
    int32_t rowCount, const std::shared_ptr<omniruntime::op::NullsHelper> &nullMap)
{
    for (int32_t i = 0; i < rowCount; i++) {
        if (nullMap != nullptr && (*nullMap)[i])
            continue;
        if (yVec->IsNull(i + rowOffset) || xVec->IsNull(i + rowOffset))
            continue;
        double y = RegrGetDoubleAt(yVec, i + rowOffset);
        double x = RegrGetDoubleAt(xVec, i + rowOffset);
        // Spark RegrSXY is Covariance(left=y, right=x); first buffer avg is mean(y), second is mean(x).
        SparkCovarianceUpdate(acc->n, acc->xAvg, acc->yAvg, acc->ck, y, x);
    }
}

static void AccumulateMergeBatch(
    RegrCov4State *acc, VectorBatch *batch, const std::vector<int32_t> &channels, int32_t rowOffset,
    int32_t rowCount)
{
    const size_t nCol = channels.size();
    if (nCol == 4) {
        for (int32_t i = 0; i < rowCount; i++) {
            int32_t row = i + rowOffset;
            MergeOneCov4Partial(acc, batch, channels, row);
        }
        return;
    }
    throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR",
        "regr_sxy merge (Spark covariance): expected 4 columns, got " + std::to_string(nCol));
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
        RegrCov4State *acc = reinterpret_cast<RegrCov4State *>(rowStates[i] + aggStateOffset);
        double y = RegrGetDoubleAt(yVec, row);
        double x = RegrGetDoubleAt(xVec, row);
        SparkCovarianceUpdate(acc->n, acc->xAvg, acc->yAvg, acc->ck, y, x);
    }
}

static void AccumulateGroupMergeBatch(std::vector<AggregateState *> &rowStates, size_t aggStateOffset,
    VectorBatch *batch, const std::vector<int32_t> &channels, int32_t rowOffset,
    const std::shared_ptr<omniruntime::op::NullsHelper> &nullMap)
{
    const size_t nCol = channels.size();
    size_t n = rowStates.size();
    if (nCol == 4) {
        for (size_t i = 0; i < n; i++) {
            if (nullMap != nullptr && (*nullMap)[i])
                continue;
            int32_t row = static_cast<int32_t>(i) + rowOffset;
            RegrCov4State *acc = reinterpret_cast<RegrCov4State *>(rowStates[i] + aggStateOffset);
            MergeOneCov4Partial(acc, batch, channels, row);
        }
        return;
    }
    throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR",
        "regr_sxy merge (Spark covariance): expected 4 columns, got " + std::to_string(nCol));
}

static void ExtractPartialRow(const omniruntime::type::DataTypes &outputTypes, const RegrCov4State *s,
    std::vector<BaseVector *> &vectors, int32_t rowIndex)
{
    (void)outputTypes;
    if (vectors.size() < 4)
        return;
    auto *v0 = static_cast<Vector<double> *>(vectors[0]);
    auto *v1 = static_cast<Vector<double> *>(vectors[1]);
    auto *v2 = static_cast<Vector<double> *>(vectors[2]);
    auto *v3 = static_cast<Vector<double> *>(vectors[3]);
    if (s->IsEmpty()) {
        v0->SetNull(rowIndex);
        v1->SetNull(rowIndex);
        v2->SetNull(rowIndex);
        v3->SetNull(rowIndex);
        return;
    }
    v0->SetValue(rowIndex, s->n);
    v1->SetValue(rowIndex, s->xAvg);
    v2->SetValue(rowIndex, s->yAvg);
    v3->SetValue(rowIndex, s->ck);
}

static void ExtractPartialBatch(const omniruntime::type::DataTypes &outputTypes,
    std::vector<AggregateState *> &groupStates, size_t aggStateOffset, std::vector<BaseVector *> &vectors,
    int32_t rowOffset, int32_t rowCount)
{
    (void)outputTypes;
    if (vectors.size() < 4)
        return;
    auto *v0 = static_cast<Vector<double> *>(vectors[0]);
    auto *v1 = static_cast<Vector<double> *>(vectors[1]);
    auto *v2 = static_cast<Vector<double> *>(vectors[2]);
    auto *v3 = static_cast<Vector<double> *>(vectors[3]);
    for (int32_t i = 0; i < rowCount; i++) {
        RegrCov4State *st = reinterpret_cast<RegrCov4State *>(groupStates[i] + aggStateOffset);
        int32_t row = rowOffset + i;
        if (st->IsEmpty()) {
            v0->SetNull(row);
            v1->SetNull(row);
            v2->SetNull(row);
            v3->SetNull(row);
        } else {
            v0->SetValue(row, st->n);
            v1->SetValue(row, st->xAvg);
            v2->SetValue(row, st->yAvg);
            v3->SetValue(row, st->ck);
        }
    }
}

} // namespace

namespace omniruntime {
namespace op {

RegrSxyAggregator::RegrSxyAggregator(const DataTypes &inputTypes, const DataTypes &outputTypes,
    std::vector<int32_t> &channels, bool inputRaw, bool outputPartial, bool isOverflowAsNull)
    : TypedAggregator(OMNI_AGGREGATION_TYPE_REGR_SXY, inputTypes, outputTypes, channels, inputRaw, outputPartial,
          isOverflowAsNull)
{
}

size_t RegrSxyAggregator::GetStateSize()
{
    return sizeof(RegrCov4State);
}

void RegrSxyAggregator::ProcessSingleInternal(AggregateState *state, BaseVector *vector, const int32_t rowOffset,
    const int32_t rowCount, const std::shared_ptr<NullsHelper> nullMap)
{
    RegrCov4State *acc = reinterpret_cast<RegrCov4State *>(state);
    if (inputRaw) {
        BaseVector *yVec = curVectorBatch->Get(channels[0]);
        BaseVector *xVec = curVectorBatch->Get(channels[1]);
        ::AccumulateRawFullPair(acc, yVec, xVec, rowOffset, rowCount, nullMap);
    } else {
        ::AccumulateMergeBatch(acc, curVectorBatch, channels, rowOffset, rowCount);
    }
}

void RegrSxyAggregator::ProcessGroupInternal(std::vector<AggregateState *> &rowStates, BaseVector *vector,
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

void RegrSxyAggregator::InitState(AggregateState *state)
{
    RegrCov4State *acc = reinterpret_cast<RegrCov4State *>(state + aggStateOffset);
    *acc = RegrCov4State{};
}

void RegrSxyAggregator::InitStates(std::vector<AggregateState *> &groupStates)
{
    for (auto *s : groupStates)
        InitState(s);
}

void RegrSxyAggregator::ExtractValues(const AggregateState *state, std::vector<BaseVector *> &vectors,
    const int32_t rowIndex)
{
    const RegrCov4State *s = reinterpret_cast<const RegrCov4State *>(state + aggStateOffset);
    if (outputPartial) {
        ::ExtractPartialRow(outputTypes, s, vectors, rowIndex);
        return;
    }
    BaseVector *outVec = vectors[0];
    if (s->IsEmpty()) {
        outVec->SetNull(rowIndex);
        return;
    }
    double result = s->ck;
    if (result == 0.0)
        result = 0.0;
    static_cast<Vector<double> *>(outVec)->SetValue(rowIndex, result);
}

void RegrSxyAggregator::ExtractValuesBatch(std::vector<AggregateState *> &groupStates,
    std::vector<BaseVector *> &vectors, int32_t rowOffset, int32_t rowCount)
{
    if (outputPartial) {
        ::ExtractPartialBatch(outputTypes, groupStates, aggStateOffset, vectors, rowOffset, rowCount);
        return;
    }
    auto *outVec = vectors[0];
    for (int32_t i = 0; i < rowCount; i++) {
        const RegrCov4State *s = reinterpret_cast<const RegrCov4State *>(groupStates[i] + aggStateOffset);
        int32_t row = rowOffset + i;
        if (s->IsEmpty()) {
            outVec->SetNull(row);
            continue;
        }
        double result = s->ck;
        if (result == 0.0)
            result = 0.0;
        static_cast<Vector<double> *>(outVec)->SetValue(row, result);
    }
}

void RegrSxyAggregator::ExtractValuesForSpill(std::vector<AggregateState *> &groupStates,
    std::vector<BaseVector *> &vectors)
{
    int32_t rowCount = static_cast<int32_t>(groupStates.size());
    for (int32_t i = 0; i < rowCount; i++) {
        RegrCov4State *st = reinterpret_cast<RegrCov4State *>(groupStates[i] + aggStateOffset);
        auto *v0 = static_cast<Vector<double> *>(vectors[0]);
        auto *v1 = static_cast<Vector<double> *>(vectors[1]);
        auto *v2 = static_cast<Vector<double> *>(vectors[2]);
        auto *v3 = static_cast<Vector<double> *>(vectors[3]);
        if (st->IsEmpty()) {
            v0->SetNull(i);
            v1->SetNull(i);
            v2->SetNull(i);
            v3->SetNull(i);
        } else {
            v0->SetValue(i, st->n);
            v1->SetValue(i, st->xAvg);
            v2->SetValue(i, st->yAvg);
            v3->SetValue(i, st->ck);
        }
    }
}

void RegrSxyAggregator::ProcessGroupUnspill(std::vector<UnspillRowInfo> &unspillRows, int32_t rowCount,
    int32_t &vectorIndex)
{
    int32_t i0 = vectorIndex++;
    int32_t i1 = vectorIndex++;
    int32_t i2 = vectorIndex++;
    int32_t i3 = vectorIndex++;
    for (int32_t rowIdx = 0; rowIdx < rowCount; rowIdx++) {
        auto &row = unspillRows[rowIdx];
        auto *batch = row.batch;
        auto index = row.rowIdx;
        auto *v0 = batch->Get(i0);
        if (v0->IsNull(index))
            continue;
        double nIn = RegrGetDoubleAt(v0, index);
        if (nIn == 0.0)
            continue;
        double xA = RegrGetDoubleAt(batch->Get(i1), index);
        double yA = RegrGetDoubleAt(batch->Get(i2), index);
        double ck = RegrGetDoubleAt(batch->Get(i3), index);
        auto *acc = reinterpret_cast<RegrCov4State *>(row.state + aggStateOffset);
        SparkCovarianceMerge(acc->n, acc->xAvg, acc->yAvg, acc->ck, nIn, xA, yA, ck);
    }
}

void RegrSxyAggregator::AlignAggSchema(VectorBatch *result, VectorBatch *inputVecBatch)
{
    int32_t rc = inputVecBatch->GetRowCount();
    if (rc == 0) {
        RegrAlignAppendEmptyCov4(result, this->pool_);
        return;
    }
    if (!inputRaw) {
        RegrAlignAppendPartialSlices(result, inputVecBatch, channels, 4, rc);
        return;
    }
    std::shared_ptr<NullsHelper> yxNull;
    GetVector(inputVecBatch, 0, rc, &yxNull);
    BaseVector *yVec = inputVecBatch->Get(channels[0]);
    BaseVector *xVec = inputVecBatch->Get(channels[1]);
    RegrAlignAppendCov4Raw(result, yVec, xVec, yxNull, rc, this->pool_);
}

void RegrSxyAggregator::AlignAggSchemaWithFilter(VectorBatch *result, VectorBatch *inputVecBatch,
    const int32_t filterIndex)
{
    int32_t rc = inputVecBatch->GetRowCount();
    if (rc == 0) {
        RegrAlignAppendEmptyCov4(result, this->pool_);
        return;
    }
    auto *filterVec = static_cast<Vector<bool> *>(inputVecBatch->Get(filterIndex));
    bool needFilter = DoNeedHandleAggFilter(filterVec, 0, rc);
    std::shared_ptr<NullsHelper> yxNull;
    GetVector(inputVecBatch, 0, rc, &yxNull);
    std::shared_ptr<NullsHelper> rowSkip = RegrAlignMergeYxNullsWithFilter(yxNull, filterVec, needFilter, rc, this->pool_);
    if (!inputRaw) {
        RegrAlignAppendPartialColumnsWithSkip(result, inputVecBatch, channels, 4, rc, rowSkip, false, this->pool_);
        return;
    }
    BaseVector *yVec = inputVecBatch->Get(channels[0]);
    BaseVector *xVec = inputVecBatch->Get(channels[1]);
    RegrAlignAppendCov4Raw(result, yVec, xVec, rowSkip, rc, this->pool_);
}

void RegrSxyAggregator::ProcessAlignAggSchema(VectorBatch *result, BaseVector *originVector,
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

std::vector<DataTypePtr> RegrSxyAggregator::GetSpillType()
{
    std::vector<DataTypePtr> types;
    for (int i = 0; i < 4; ++i) {
        types.push_back(std::make_shared<type::DataType>(type::DataTypeId::OMNI_DOUBLE));
    }
    return types;
}

} // namespace op
} // namespace omniruntime
