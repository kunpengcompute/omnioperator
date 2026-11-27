/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 */
#include "operator/aggregation/aggregator/regr/regr_syy_aggregator.h"
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
using omniruntime::op::RegrVarPopMerge;
using omniruntime::op::RegrVarPopState;
using omniruntime::op::RegrVarPopUpdate;
using omniruntime::vec::BaseVector;
using omniruntime::vec::Vector;
using omniruntime::vec::VectorBatch;

static void MergeOneVarPopPartial(RegrVarPopState *acc, VectorBatch *batch, const std::vector<int32_t> &channels,
    int32_t row)
{
    auto *nVec = batch->Get(channels[0]);
    if (nVec->IsNull(row))
        return;
    double nIn = RegrGetDoubleAt(nVec, row);
    if (nIn == 0.0)
        return;
    double avgIn = RegrGetDoubleAt(batch->Get(channels[1]), row);
    double m2In = RegrGetDoubleAt(batch->Get(channels[2]), row);
    RegrVarPopMerge(acc->n, acc->avg, acc->m2, nIn, avgIn, m2In);
}

static void AccumulateRawFullPair(RegrVarPopState *acc, BaseVector *yVec, BaseVector *xVec, int32_t rowOffset,
    int32_t rowCount, const std::shared_ptr<omniruntime::op::NullsHelper> &nullMap)
{
    for (int32_t i = 0; i < rowCount; i++) {
        if (nullMap != nullptr && (*nullMap)[i])
            continue;
        if (yVec->IsNull(i + rowOffset) || xVec->IsNull(i + rowOffset))
            continue;
        double y = RegrGetDoubleAt(yVec, i + rowOffset);
        RegrVarPopUpdate(acc->n, acc->avg, acc->m2, y);
    }
}

static void AccumulateMergeBatch(
    RegrVarPopState *acc, VectorBatch *batch, const std::vector<int32_t> &channels, int32_t rowOffset,
    int32_t rowCount)
{
    const size_t nCol = channels.size();
    if (nCol == 3) {
        for (int32_t i = 0; i < rowCount; i++) {
            int32_t row = i + rowOffset;
            MergeOneVarPopPartial(acc, batch, channels, row);
        }
        return;
    }
    throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR",
        "regr_syy merge (Spark var_pop y): expected 3 columns, got " + std::to_string(nCol));
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
        RegrVarPopState *acc = reinterpret_cast<RegrVarPopState *>(rowStates[i] + aggStateOffset);
        double y = RegrGetDoubleAt(yVec, row);
        RegrVarPopUpdate(acc->n, acc->avg, acc->m2, y);
    }
}

static void AccumulateGroupMergeBatch(std::vector<AggregateState *> &rowStates, size_t aggStateOffset,
    VectorBatch *batch, const std::vector<int32_t> &channels, int32_t rowOffset,
    const std::shared_ptr<omniruntime::op::NullsHelper> &nullMap)
{
    const size_t nCol = channels.size();
    size_t n = rowStates.size();
    if (nCol == 3) {
        for (size_t i = 0; i < n; i++) {
            if (nullMap != nullptr && (*nullMap)[i])
                continue;
            int32_t row = static_cast<int32_t>(i) + rowOffset;
            RegrVarPopState *acc = reinterpret_cast<RegrVarPopState *>(rowStates[i] + aggStateOffset);
            MergeOneVarPopPartial(acc, batch, channels, row);
        }
        return;
    }
    throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR",
        "regr_syy merge (Spark var_pop y): expected 3 columns, got " + std::to_string(nCol));
}

static void ExtractPartialRow(const omniruntime::type::DataTypes &outputTypes, const RegrVarPopState *s,
    std::vector<BaseVector *> &vectors, int32_t rowIndex)
{
    (void)outputTypes;
    if (vectors.size() < 3)
        return;
    auto *nVec = static_cast<Vector<double> *>(vectors[0]);
    auto *avgVec = static_cast<Vector<double> *>(vectors[1]);
    auto *m2Vec = static_cast<Vector<double> *>(vectors[2]);
    if (s->IsEmpty()) {
        nVec->SetNull(rowIndex);
        avgVec->SetNull(rowIndex);
        m2Vec->SetNull(rowIndex);
        return;
    }
    nVec->SetValue(rowIndex, s->n);
    avgVec->SetValue(rowIndex, s->avg);
    m2Vec->SetValue(rowIndex, s->m2);
}

static void ExtractPartialBatch(const omniruntime::type::DataTypes &outputTypes,
    std::vector<AggregateState *> &groupStates, size_t aggStateOffset, std::vector<BaseVector *> &vectors,
    int32_t rowOffset, int32_t rowCount)
{
    (void)outputTypes;
    if (vectors.size() < 3)
        return;
    auto *nVec = static_cast<Vector<double> *>(vectors[0]);
    auto *avgVec = static_cast<Vector<double> *>(vectors[1]);
    auto *m2Vec = static_cast<Vector<double> *>(vectors[2]);
    for (int32_t i = 0; i < rowCount; i++) {
        RegrVarPopState *st = reinterpret_cast<RegrVarPopState *>(groupStates[i] + aggStateOffset);
        int32_t row = rowOffset + i;
        if (st->IsEmpty()) {
            nVec->SetNull(row);
            avgVec->SetNull(row);
            m2Vec->SetNull(row);
        } else {
            nVec->SetValue(row, st->n);
            avgVec->SetValue(row, st->avg);
            m2Vec->SetValue(row, st->m2);
        }
    }
}

} // namespace

namespace omniruntime {
namespace op {

RegrSyyAggregator::RegrSyyAggregator(const DataTypes &inputTypes, const DataTypes &outputTypes,
    std::vector<int32_t> &channels, bool inputRaw, bool outputPartial, bool isOverflowAsNull)
    : TypedAggregator(OMNI_AGGREGATION_TYPE_REGR_SYY, inputTypes, outputTypes, channels, inputRaw, outputPartial,
          isOverflowAsNull)
{
}

size_t RegrSyyAggregator::GetStateSize()
{
    return sizeof(RegrVarPopState);
}

void RegrSyyAggregator::ProcessSingleInternal(AggregateState *state, BaseVector *vector, const int32_t rowOffset,
    const int32_t rowCount, const std::shared_ptr<NullsHelper> nullMap)
{
    RegrVarPopState *acc = reinterpret_cast<RegrVarPopState *>(state);
    if (inputRaw) {
        BaseVector *yVec = curVectorBatch->Get(channels[0]);
        BaseVector *xVec = curVectorBatch->Get(channels[1]);
        ::AccumulateRawFullPair(acc, yVec, xVec, rowOffset, rowCount, nullMap);
    } else {
        ::AccumulateMergeBatch(acc, curVectorBatch, channels, rowOffset, rowCount);
    }
}

void RegrSyyAggregator::ProcessGroupInternal(std::vector<AggregateState *> &rowStates, BaseVector *vector,
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

void RegrSyyAggregator::InitState(AggregateState *state)
{
    RegrVarPopState *acc = reinterpret_cast<RegrVarPopState *>(state + aggStateOffset);
    *acc = RegrVarPopState{};
}

void RegrSyyAggregator::InitStates(std::vector<AggregateState *> &groupStates)
{
    for (auto *s : groupStates)
        InitState(s);
}

void RegrSyyAggregator::ExtractValues(const AggregateState *state, std::vector<BaseVector *> &vectors,
    const int32_t rowIndex)
{
    const RegrVarPopState *s = reinterpret_cast<const RegrVarPopState *>(state + aggStateOffset);
    if (outputPartial) {
        ::ExtractPartialRow(outputTypes, s, vectors, rowIndex);
        return;
    }
    BaseVector *outVec = vectors[0];
    if (s->IsEmpty()) {
        outVec->SetNull(rowIndex);
        return;
    }
    double result = s->m2;
    if (result == 0.0)
        result = 0.0;
    static_cast<Vector<double> *>(outVec)->SetValue(rowIndex, result);
}

void RegrSyyAggregator::ExtractValuesBatch(std::vector<AggregateState *> &groupStates,
    std::vector<BaseVector *> &vectors, int32_t rowOffset, int32_t rowCount)
{
    if (outputPartial) {
        ::ExtractPartialBatch(outputTypes, groupStates, aggStateOffset, vectors, rowOffset, rowCount);
        return;
    }
    auto *outVec = vectors[0];
    for (int32_t i = 0; i < rowCount; i++) {
        const RegrVarPopState *s = reinterpret_cast<const RegrVarPopState *>(groupStates[i] + aggStateOffset);
        int32_t row = rowOffset + i;
        if (s->IsEmpty()) {
            outVec->SetNull(row);
            continue;
        }
        double result = s->m2;
        if (result == 0.0)
            result = 0.0;
        static_cast<Vector<double> *>(outVec)->SetValue(row, result);
    }
}

void RegrSyyAggregator::ExtractValuesForSpill(std::vector<AggregateState *> &groupStates,
    std::vector<BaseVector *> &vectors)
{
    int32_t rowCount = static_cast<int32_t>(groupStates.size());
    for (int32_t i = 0; i < rowCount; i++) {
        RegrVarPopState *st = reinterpret_cast<RegrVarPopState *>(groupStates[i] + aggStateOffset);
        auto *nVec = static_cast<Vector<double> *>(vectors[0]);
        auto *avgVec = static_cast<Vector<double> *>(vectors[1]);
        auto *m2Vec = static_cast<Vector<double> *>(vectors[2]);
        if (st->IsEmpty()) {
            nVec->SetNull(i);
            avgVec->SetNull(i);
            m2Vec->SetNull(i);
        } else {
            nVec->SetValue(i, st->n);
            avgVec->SetValue(i, st->avg);
            m2Vec->SetValue(i, st->m2);
        }
    }
}

void RegrSyyAggregator::ProcessGroupUnspill(std::vector<UnspillRowInfo> &unspillRows, int32_t rowCount,
    int32_t &vectorIndex)
{
    int32_t i0 = vectorIndex++;
    int32_t i1 = vectorIndex++;
    int32_t i2 = vectorIndex++;
    for (int32_t rowIdx = 0; rowIdx < rowCount; rowIdx++) {
        auto &row = unspillRows[rowIdx];
        auto *batch = row.batch;
        auto index = row.rowIdx;
        auto *nVec = batch->Get(i0);
        if (nVec->IsNull(index))
            continue;
        double nIn = RegrGetDoubleAt(nVec, index);
        if (nIn == 0.0)
            continue;
        double avgIn = RegrGetDoubleAt(batch->Get(i1), index);
        double m2In = RegrGetDoubleAt(batch->Get(i2), index);
        auto *acc = reinterpret_cast<RegrVarPopState *>(row.state + aggStateOffset);
        RegrVarPopMerge(acc->n, acc->avg, acc->m2, nIn, avgIn, m2In);
    }
}

void RegrSyyAggregator::AlignAggSchema(VectorBatch *result, VectorBatch *inputVecBatch)
{
    int32_t rc = inputVecBatch->GetRowCount();
    if (rc == 0) {
        RegrAlignAppendEmptyVarPop3(result, this->pool_);
        return;
    }
    if (!inputRaw) {
        RegrAlignAppendPartialSlices(result, inputVecBatch, channels, 3, rc);
        return;
    }
    std::shared_ptr<NullsHelper> yxNull;
    GetVector(inputVecBatch, 0, rc, &yxNull);
    BaseVector *yVec = inputVecBatch->Get(channels[0]);
    RegrAlignAppendVarPop3RawFromY(result, yVec, yxNull, rc, this->pool_);
}

void RegrSyyAggregator::AlignAggSchemaWithFilter(VectorBatch *result, VectorBatch *inputVecBatch,
    const int32_t filterIndex)
{
    int32_t rc = inputVecBatch->GetRowCount();
    if (rc == 0) {
        RegrAlignAppendEmptyVarPop3(result, this->pool_);
        return;
    }
    auto *filterVec = static_cast<Vector<bool> *>(inputVecBatch->Get(filterIndex));
    bool needFilter = DoNeedHandleAggFilter(filterVec, 0, rc);
    std::shared_ptr<NullsHelper> yxNull;
    GetVector(inputVecBatch, 0, rc, &yxNull);
    std::shared_ptr<NullsHelper> rowSkip = RegrAlignMergeYxNullsWithFilter(yxNull, filterVec, needFilter, rc, this->pool_);
    if (!inputRaw) {
        RegrAlignAppendPartialColumnsWithSkip(result, inputVecBatch, channels, 3, rc, rowSkip, false, this->pool_);
        return;
    }
    BaseVector *yVec = inputVecBatch->Get(channels[0]);
    RegrAlignAppendVarPop3RawFromY(result, yVec, rowSkip, rc, this->pool_);
}

void RegrSyyAggregator::ProcessAlignAggSchema(VectorBatch *result, BaseVector *originVector,
    const std::shared_ptr<NullsHelper> nullMap, const bool aggFilter)
{
    (void)nullMap;
    (void)aggFilter;
    if (originVector == nullptr) {
        RegrAlignAppendEmptyVarPop3(result, this->pool_);
        return;
    }
    RegrAlignAppendVarPop3AllNullRows(result, originVector->GetSize(), this->pool_);
}

std::vector<DataTypePtr> RegrSyyAggregator::GetSpillType()
{
    std::vector<DataTypePtr> types;
    for (int i = 0; i < 3; ++i) {
        types.push_back(std::make_shared<type::DataType>(type::DataTypeId::OMNI_DOUBLE));
    }
    return types;
}

} // namespace op
} // namespace omniruntime
