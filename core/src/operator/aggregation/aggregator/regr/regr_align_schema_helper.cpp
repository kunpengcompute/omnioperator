/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 *
 * Every Vector<T> allocation in this translation unit is pool-bound.
 * The trailing OmniTaskMemoryPool *pool parameter flows from
 * TypedAggregator::pool_ in the calling regr aggregator subclass.
 */
#include "operator/aggregation/aggregator/regr/regr_align_schema_helper.h"
#include "operator/aggregation/aggregator/regr/regr_numeric.h"
#include "operator/aggregation/aggregator/regr/regr_state.h"
#include "vector/nulls_buffer.h"
#include "vector/unsafe_vector.h"
#include "vector/vector.h"
#include "vector/vector_helper.h"

namespace omniruntime::op {

using omniruntime::mem::OmniTaskMemoryPool;
using omniruntime::vec::BaseVector;
using omniruntime::vec::NullsBuffer;
using omniruntime::vec::NullsHelper;
using omniruntime::vec::Vector;
using omniruntime::vec::VectorBatch;
using omniruntime::vec::VectorHelper;
using omniruntime::vec::unsafe::UnsafeVector;

namespace {

inline bool RegrAlignRowSkipped(const std::shared_ptr<NullsHelper> &rowSkip, int32_t i)
{
    return rowSkip != nullptr && (*rowSkip)[i];
}

inline int64_t RegrGetInt64At(BaseVector *vec, int32_t row)
{
    if (vec->GetTypeId() == omniruntime::type::OMNI_LONG) {
        return reinterpret_cast<int64_t *>(
            omniruntime::op::GetValuesFromVector<omniruntime::type::OMNI_LONG>(vec))[row];
    }
    if (vec->GetTypeId() == omniruntime::type::OMNI_INT) {
        return static_cast<int64_t>(reinterpret_cast<int32_t *>(
            omniruntime::op::GetValuesFromVector<omniruntime::type::OMNI_INT>(vec))[row]);
    }
    return static_cast<int64_t>(reinterpret_cast<double *>(
        omniruntime::op::GetValuesFromVector<omniruntime::type::OMNI_DOUBLE>(vec))[row]);
}

inline void CopyPartialCellLong(
    BaseVector *src, int32_t srcRow, Vector<int64_t> *dst, int32_t dstRow, bool skipRow)
{
    if (skipRow || src->IsNull(srcRow)) {
        dst->SetNull(dstRow);
        return;
    }
    dst->SetValue(dstRow, RegrGetInt64At(src, srcRow));
}

inline void CopyPartialCellDouble(
    BaseVector *src, int32_t srcRow, Vector<double> *dst, int32_t dstRow, bool skipRow)
{
    if (skipRow || src->IsNull(srcRow)) {
        dst->SetNull(dstRow);
        return;
    }
    dst->SetValue(dstRow, RegrGetDoubleAt(src, srcRow));
}

} // namespace

std::shared_ptr<NullsHelper> RegrAlignMergeYxNullsWithFilter(const std::shared_ptr<NullsHelper> &yxNull,
    Vector<bool> *filterVec, bool filterIsActive, int32_t rowCount, omniruntime::mem::OmniTaskMemoryPool *pool)
{
    if (!filterIsActive) {
        return yxNull;
    }
    auto buf = std::make_shared<NullsBuffer>(rowCount, nullptr, 0,
        pool != nullptr ? pool->getBoundAllocator() : nullptr);
    auto merged = std::make_shared<NullsHelper>(buf);
    auto *fp = UnsafeVector::GetRawValues(filterVec);
    for (int32_t i = 0; i < rowCount; ++i) {
        bool skip = !fp[i] || (yxNull != nullptr && (*yxNull)[i]);
        merged->SetNull(i, skip);
    }
    if (!buf->HasNull()) {
        return nullptr;
    }
    return merged;
}

void RegrAlignAppendEmptyVarPop3(VectorBatch *result, OmniTaskMemoryPool *pool)
{
    result->Append(new Vector<double>(0, pool));
    result->Append(new Vector<double>(0, pool));
    result->Append(new Vector<double>(0, pool));
}

void RegrAlignAppendEmptyCov4(VectorBatch *result, OmniTaskMemoryPool *pool)
{
    for (int c = 0; c < 4; ++c) {
        result->Append(new Vector<double>(0, pool));
    }
}

void RegrAlignAppendEmptyPearson6(VectorBatch *result, OmniTaskMemoryPool *pool)
{
    for (int c = 0; c < 6; ++c) {
        result->Append(new Vector<double>(0, pool));
    }
}

void RegrAlignAppendEmptySlope7(VectorBatch *result, OmniTaskMemoryPool *pool)
{
    for (int c = 0; c < 7; ++c) {
        result->Append(new Vector<double>(0, pool));
    }
}

void RegrAlignAppendVarPop3AllNullRows(VectorBatch *result, int32_t rowCount, OmniTaskMemoryPool *pool)
{
    auto *nVec = new Vector<double>(rowCount, pool);
    auto *avgVec = new Vector<double>(rowCount, pool);
    auto *m2Vec = new Vector<double>(rowCount, pool);
    for (int32_t i = 0; i < rowCount; ++i) {
        nVec->SetNull(i);
        avgVec->SetNull(i);
        m2Vec->SetNull(i);
    }
    result->Append(nVec);
    result->Append(avgVec);
    result->Append(m2Vec);
}

void RegrAlignAppendCov4AllNullRows(VectorBatch *result, int32_t rowCount, OmniTaskMemoryPool *pool)
{
    for (int c = 0; c < 4; ++c) {
        auto *v = new Vector<double>(rowCount, pool);
        for (int32_t i = 0; i < rowCount; ++i) {
            v->SetNull(i);
        }
        result->Append(v);
    }
}

void RegrAlignAppendPearson6AllNullRows(VectorBatch *result, int32_t rowCount, OmniTaskMemoryPool *pool)
{
    for (int c = 0; c < 6; ++c) {
        auto *v = new Vector<double>(rowCount, pool);
        for (int32_t i = 0; i < rowCount; ++i) {
            v->SetNull(i);
        }
        result->Append(v);
    }
}

void RegrAlignAppendSlope7AllNullRows(VectorBatch *result, int32_t rowCount, OmniTaskMemoryPool *pool)
{
    for (int c = 0; c < 7; ++c) {
        auto *v = new Vector<double>(rowCount, pool);
        for (int32_t i = 0; i < rowCount; ++i) {
            v->SetNull(i);
        }
        result->Append(v);
    }
}

void RegrAlignAppendVarPop3RawFromX(VectorBatch *result, BaseVector *xVec,
    const std::shared_ptr<NullsHelper> &rowSkip, int32_t rowCount, OmniTaskMemoryPool *pool)
{
    auto *nVec = new Vector<double>(rowCount, pool);
    auto *avgVec = new Vector<double>(rowCount, pool);
    auto *m2Vec = new Vector<double>(rowCount, pool);
    for (int32_t i = 0; i < rowCount; ++i) {
        if (RegrAlignRowSkipped(rowSkip, i) || xVec->IsNull(i)) {
            nVec->SetNull(i);
            avgVec->SetNull(i);
            m2Vec->SetNull(i);
            continue;
        }
        double n = 1.0;
        double avg = RegrGetDoubleAt(xVec, i);
        double m2 = 0.0;
        nVec->SetValue(i, n);
        avgVec->SetValue(i, avg);
        m2Vec->SetValue(i, m2);
    }
    result->Append(nVec);
    result->Append(avgVec);
    result->Append(m2Vec);
}

void RegrAlignAppendVarPop3RawFromY(VectorBatch *result, BaseVector *yVec,
    const std::shared_ptr<NullsHelper> &rowSkip, int32_t rowCount, OmniTaskMemoryPool *pool)
{
    auto *nVec = new Vector<double>(rowCount, pool);
    auto *avgVec = new Vector<double>(rowCount, pool);
    auto *m2Vec = new Vector<double>(rowCount, pool);
    for (int32_t i = 0; i < rowCount; ++i) {
        if (RegrAlignRowSkipped(rowSkip, i) || yVec->IsNull(i)) {
            nVec->SetNull(i);
            avgVec->SetNull(i);
            m2Vec->SetNull(i);
            continue;
        }
        nVec->SetValue(i, 1.0);
        avgVec->SetValue(i, RegrGetDoubleAt(yVec, i));
        m2Vec->SetValue(i, 0.0);
    }
    result->Append(nVec);
    result->Append(avgVec);
    result->Append(m2Vec);
}

void RegrAlignAppendCov4Raw(VectorBatch *result, BaseVector *yVec, BaseVector *xVec,
    const std::shared_ptr<NullsHelper> &rowSkip, int32_t rowCount, OmniTaskMemoryPool *pool)
{
    auto *v0 = new Vector<double>(rowCount, pool);
    auto *v1 = new Vector<double>(rowCount, pool);
    auto *v2 = new Vector<double>(rowCount, pool);
    auto *v3 = new Vector<double>(rowCount, pool);
    for (int32_t i = 0; i < rowCount; ++i) {
        if (RegrAlignRowSkipped(rowSkip, i) || yVec->IsNull(i) || xVec->IsNull(i)) {
            v0->SetNull(i);
            v1->SetNull(i);
            v2->SetNull(i);
            v3->SetNull(i);
            continue;
        }
        RegrCov4State st{};
        SparkCovarianceUpdate(st.n, st.xAvg, st.yAvg, st.ck, RegrGetDoubleAt(yVec, i), RegrGetDoubleAt(xVec, i));
        v0->SetValue(i, st.n);
        v1->SetValue(i, st.xAvg);
        v2->SetValue(i, st.yAvg);
        v3->SetValue(i, st.ck);
    }
    result->Append(v0);
    result->Append(v1);
    result->Append(v2);
    result->Append(v3);
}

void RegrAlignAppendPearson6RawStd(VectorBatch *result, BaseVector *yVec, BaseVector *xVec,
    const std::shared_ptr<NullsHelper> &rowSkip, int32_t rowCount, OmniTaskMemoryPool *pool)
{
    auto *cntVec = new Vector<double>(rowCount, pool);
    auto *meanXVec = new Vector<double>(rowCount, pool);
    auto *meanYVec = new Vector<double>(rowCount, pool);
    auto *c2Vec = new Vector<double>(rowCount, pool);
    auto *m2XVec = new Vector<double>(rowCount, pool);
    auto *m2YVec = new Vector<double>(rowCount, pool);
    for (int32_t i = 0; i < rowCount; ++i) {
        if (RegrAlignRowSkipped(rowSkip, i) || yVec->IsNull(i) || xVec->IsNull(i)) {
            cntVec->SetNull(i);
            meanXVec->SetNull(i);
            meanYVec->SetNull(i);
            c2Vec->SetNull(i);
            m2XVec->SetNull(i);
            m2YVec->SetNull(i);
            continue;
        }
        RegrState s{};
        double y = RegrGetDoubleAt(yVec, i);
        double x = RegrGetDoubleAt(xVec, i);
        double oldMeanX = s.meanX;
        double oldMeanY = s.meanY;
        s.count = 1;
        double deltaX = x - s.meanX;
        s.meanX += deltaX / s.count;
        double deltaY = y - s.meanY;
        s.meanY += deltaY / s.count;
        s.c2 += deltaX * (y - s.meanY);
        s.m2X += (x - oldMeanX) * (x - s.meanX);
        s.m2Y += (y - oldMeanY) * (y - s.meanY);
        cntVec->SetValue(i, static_cast<double>(s.count));
        meanXVec->SetValue(i, s.meanX);
        meanYVec->SetValue(i, s.meanY);
        c2Vec->SetValue(i, s.c2);
        m2XVec->SetValue(i, s.m2X);
        m2YVec->SetValue(i, s.m2Y);
    }
    result->Append(cntVec);
    result->Append(meanXVec);
    result->Append(meanYVec);
    result->Append(c2Vec);
    result->Append(m2XVec);
    result->Append(m2YVec);
}

void RegrAlignAppendPearson6RawR2Layout(VectorBatch *result, BaseVector *yVec, BaseVector *xVec,
    const std::shared_ptr<NullsHelper> &rowSkip, int32_t rowCount, OmniTaskMemoryPool *pool)
{
    auto *cntVec = new Vector<double>(rowCount, pool);
    auto *meanXVec = new Vector<double>(rowCount, pool);
    auto *meanYVec = new Vector<double>(rowCount, pool);
    auto *c2Vec = new Vector<double>(rowCount, pool);
    auto *m2XVec = new Vector<double>(rowCount, pool);
    auto *m2YVec = new Vector<double>(rowCount, pool);
    for (int32_t i = 0; i < rowCount; ++i) {
        if (RegrAlignRowSkipped(rowSkip, i) || yVec->IsNull(i) || xVec->IsNull(i)) {
            cntVec->SetNull(i);
            meanXVec->SetNull(i);
            meanYVec->SetNull(i);
            c2Vec->SetNull(i);
            m2XVec->SetNull(i);
            m2YVec->SetNull(i);
            continue;
        }
        RegrState s{};
        double y = RegrGetDoubleAt(yVec, i);
        double x = RegrGetDoubleAt(xVec, i);
        double oldMeanX = s.meanX;
        double oldMeanY = s.meanY;
        s.count = 1;
        double deltaX = x - s.meanX;
        s.meanX += deltaX / s.count;
        double deltaY = y - s.meanY;
        s.meanY += deltaY / s.count;
        s.c2 += deltaX * (y - s.meanY);
        s.m2X += (x - oldMeanX) * (x - s.meanX);
        s.m2Y += (y - oldMeanY) * (y - s.meanY);
        cntVec->SetValue(i, static_cast<double>(s.count));
        meanXVec->SetValue(i, s.meanY);
        meanYVec->SetValue(i, s.meanX);
        c2Vec->SetValue(i, s.c2);
        m2XVec->SetValue(i, s.m2Y);
        m2YVec->SetValue(i, s.m2X);
    }
    result->Append(cntVec);
    result->Append(meanXVec);
    result->Append(meanYVec);
    result->Append(c2Vec);
    result->Append(m2XVec);
    result->Append(m2YVec);
}

void RegrAlignAppendSlope7Raw(VectorBatch *result, BaseVector *yVec, BaseVector *xVec,
    const std::shared_ptr<NullsHelper> &rowSkip, int32_t rowCount, OmniTaskMemoryPool *pool)
{
    auto *v0 = new Vector<double>(rowCount, pool);
    auto *v1 = new Vector<double>(rowCount, pool);
    auto *v2 = new Vector<double>(rowCount, pool);
    auto *v3 = new Vector<double>(rowCount, pool);
    auto *v4 = new Vector<double>(rowCount, pool);
    auto *v5 = new Vector<double>(rowCount, pool);
    auto *v6 = new Vector<double>(rowCount, pool);
    for (int32_t i = 0; i < rowCount; ++i) {
        if (RegrAlignRowSkipped(rowSkip, i) || yVec->IsNull(i) || xVec->IsNull(i)) {
            v0->SetNull(i);
            v1->SetNull(i);
            v2->SetNull(i);
            v3->SetNull(i);
            v4->SetNull(i);
            v5->SetNull(i);
            v6->SetNull(i);
            continue;
        }
        RegrSlopeInterceptState st{};
        double y = RegrGetDoubleAt(yVec, i);
        double x = RegrGetDoubleAt(xVec, i);
        SparkCovarianceUpdate(st.covN, st.covXAvg, st.covYAvg, st.covCk, x, y);
        RegrVarPopUpdate(st.varN, st.varAvgX, st.varM2X, x);
        v0->SetValue(i, st.covN);
        v1->SetValue(i, st.covXAvg);
        v2->SetValue(i, st.covYAvg);
        v3->SetValue(i, st.covCk);
        v4->SetValue(i, st.varN);
        v5->SetValue(i, st.varAvgX);
        v6->SetValue(i, st.varM2X);
    }
    result->Append(v0);
    result->Append(v1);
    result->Append(v2);
    result->Append(v3);
    result->Append(v4);
    result->Append(v5);
    result->Append(v6);
}

void RegrAlignAppendPartialSlices(VectorBatch *result, VectorBatch *input, const std::vector<int32_t> &channels,
    int32_t numCols, int32_t rowCount)
{
    for (int32_t c = 0; c < numCols; ++c) {
        BaseVector *col = input->Get(channels[static_cast<size_t>(c)]);
        result->Append(VectorHelper::SliceVector(col, 0, rowCount));
    }
}

void RegrAlignAppendPartialColumnsWithSkip(VectorBatch *result, VectorBatch *input,
    const std::vector<int32_t> &channels, int32_t numCols, int32_t rowCount,
    const std::shared_ptr<NullsHelper> &rowSkip, bool firstColIsLong, OmniTaskMemoryPool *pool)
{
    if (rowSkip == nullptr) {
        RegrAlignAppendPartialSlices(result, input, channels, numCols, rowCount);
        return;
    }
    std::vector<BaseVector *> srcCols;
    srcCols.reserve(static_cast<size_t>(numCols));
    std::vector<BaseVector *> dstCols;
    dstCols.reserve(static_cast<size_t>(numCols));
    for (int32_t c = 0; c < numCols; ++c) {
        srcCols.push_back(input->Get(channels[static_cast<size_t>(c)]));
    }
    if (firstColIsLong) {
        dstCols.push_back(new Vector<int64_t>(rowCount, pool));
    } else {
        dstCols.push_back(new Vector<double>(rowCount, pool));
    }
    for (int32_t c = 1; c < numCols; ++c) {
        dstCols.push_back(new Vector<double>(rowCount, pool));
    }
    for (int32_t i = 0; i < rowCount; ++i) {
        bool skip = (*rowSkip)[i];
        if (firstColIsLong) {
            auto *d0 = static_cast<Vector<int64_t> *>(dstCols[0]);
            CopyPartialCellLong(srcCols[0], i, d0, i, skip);
            for (int32_t c = 1; c < numCols; ++c) {
                CopyPartialCellDouble(srcCols[static_cast<size_t>(c)], i, static_cast<Vector<double> *>(dstCols[static_cast<size_t>(c)]),
                    i, skip);
            }
        } else {
            for (int32_t c = 0; c < numCols; ++c) {
                CopyPartialCellDouble(srcCols[static_cast<size_t>(c)], i, static_cast<Vector<double> *>(dstCols[static_cast<size_t>(c)]), i,
                    skip);
            }
        }
    }
    for (auto *d : dstCols) {
        result->Append(d);
    }
}

void RegrAlignAppendEmptyReplacementPartial3(VectorBatch *result, OmniTaskMemoryPool *pool)
{
    result->Append(new Vector<double>(0, pool));
    result->Append(new Vector<double>(0, pool));
    result->Append(new Vector<double>(0, pool));
}

void RegrAlignAppendReplacementPartial3AllNullRows(VectorBatch *result, int32_t rowCount, OmniTaskMemoryPool *pool)
{
    auto *nVec = new Vector<double>(rowCount, pool);
    auto *avgVec = new Vector<double>(rowCount, pool);
    auto *m2Vec = new Vector<double>(rowCount, pool);
    for (int32_t i = 0; i < rowCount; ++i) {
        nVec->SetNull(i);
        avgVec->SetNull(i);
        m2Vec->SetNull(i);
    }
    result->Append(nVec);
    result->Append(avgVec);
    result->Append(m2Vec);
}

void RegrAlignAppendReplacementPartial3RawDouble(VectorBatch *result, BaseVector *valVec,
    const std::shared_ptr<NullsHelper> &rowSkip, int32_t rowCount, OmniTaskMemoryPool *pool)
{
    auto *nVec = new Vector<double>(rowCount, pool);
    auto *avgVec = new Vector<double>(rowCount, pool);
    auto *m2Vec = new Vector<double>(rowCount, pool);
    for (int32_t i = 0; i < rowCount; ++i) {
        if (RegrAlignRowSkipped(rowSkip, i) || valVec->IsNull(i)) {
            nVec->SetNull(i);
            avgVec->SetNull(i);
            m2Vec->SetNull(i);
            continue;
        }
        nVec->SetValue(i, 1.0);
        avgVec->SetValue(i, RegrGetDoubleAt(valVec, i));
        m2Vec->SetValue(i, 0.0);
    }
    result->Append(nVec);
    result->Append(avgVec);
    result->Append(m2Vec);
}

} // namespace omniruntime::op
