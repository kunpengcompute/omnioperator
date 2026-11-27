/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Align aggregate schema (skip partial) helpers for regr_* aggregators — per-row partial layout
 *              matches ExtractPartialRow / merge batch column layouts.
 *
 * Every helper that allocates a Vector takes a trailing `OmniTaskMemoryPool *pool`
 * parameter. Callers (regr_*_aggregator.cpp subclasses of TypedAggregator) pass
 * `this->pool_` (see aggregator.h:321-350).
 */
#ifndef OMNI_RUNTIME_REGR_ALIGN_SCHEMA_HELPER_H
#define OMNI_RUNTIME_REGR_ALIGN_SCHEMA_HELPER_H

#include "memory/omni_task_memory_pool.h"
#include "vector/vector_batch.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace omniruntime::vec {
class NullsHelper;
}

namespace omniruntime::op {

/** Merge per-row skip: y|x null (yxNull) OR (when filter is active) !filter[i]. */
std::shared_ptr<vec::NullsHelper> RegrAlignMergeYxNullsWithFilter(const std::shared_ptr<vec::NullsHelper> &yxNull,
    vec::Vector<bool> *filterVec, bool filterIsActive, int32_t rowCount,
    omniruntime::mem::OmniTaskMemoryPool *pool);

void RegrAlignAppendEmptyVarPop3(vec::VectorBatch *result, omniruntime::mem::OmniTaskMemoryPool *pool);
void RegrAlignAppendEmptyCov4(vec::VectorBatch *result, omniruntime::mem::OmniTaskMemoryPool *pool);
void RegrAlignAppendEmptyPearson6(vec::VectorBatch *result, omniruntime::mem::OmniTaskMemoryPool *pool);
void RegrAlignAppendEmptySlope7(vec::VectorBatch *result, omniruntime::mem::OmniTaskMemoryPool *pool);

void RegrAlignAppendVarPop3AllNullRows(vec::VectorBatch *result, int32_t rowCount,
    omniruntime::mem::OmniTaskMemoryPool *pool);
void RegrAlignAppendCov4AllNullRows(vec::VectorBatch *result, int32_t rowCount,
    omniruntime::mem::OmniTaskMemoryPool *pool);
void RegrAlignAppendPearson6AllNullRows(vec::VectorBatch *result, int32_t rowCount,
    omniruntime::mem::OmniTaskMemoryPool *pool);
void RegrAlignAppendSlope7AllNullRows(vec::VectorBatch *result, int32_t rowCount,
    omniruntime::mem::OmniTaskMemoryPool *pool);

void RegrAlignAppendVarPop3RawFromX(vec::VectorBatch *result, vec::BaseVector *xVec,
    const std::shared_ptr<vec::NullsHelper> &rowSkip, int32_t rowCount,
    omniruntime::mem::OmniTaskMemoryPool *pool);
void RegrAlignAppendVarPop3RawFromY(vec::VectorBatch *result, vec::BaseVector *yVec,
    const std::shared_ptr<vec::NullsHelper> &rowSkip, int32_t rowCount,
    omniruntime::mem::OmniTaskMemoryPool *pool);

void RegrAlignAppendCov4Raw(vec::VectorBatch *result, vec::BaseVector *yVec, vec::BaseVector *xVec,
    const std::shared_ptr<vec::NullsHelper> &rowSkip, int32_t rowCount,
    omniruntime::mem::OmniTaskMemoryPool *pool);

/** Standard Spark Pearson 6-col layout (count + means + ck + m2), same as regr_count partial. */
void RegrAlignAppendPearson6RawStd(vec::VectorBatch *result, vec::BaseVector *yVec, vec::BaseVector *xVec,
    const std::shared_ptr<vec::NullsHelper> &rowSkip, int32_t rowCount,
    omniruntime::mem::OmniTaskMemoryPool *pool);

/** Spark Pearson buffer: 6 DOUBLE columns (n, xAvg, yAvg, ck, xMk, yMk). R2 layout swaps mean/m2 for merge. */
void RegrAlignAppendPearson6RawR2Layout(vec::VectorBatch *result, vec::BaseVector *yVec, vec::BaseVector *xVec,
    const std::shared_ptr<vec::NullsHelper> &rowSkip, int32_t rowCount,
    omniruntime::mem::OmniTaskMemoryPool *pool);

void RegrAlignAppendSlope7Raw(vec::VectorBatch *result, vec::BaseVector *yVec, vec::BaseVector *xVec,
    const std::shared_ptr<vec::NullsHelper> &rowSkip, int32_t rowCount,
    omniruntime::mem::OmniTaskMemoryPool *pool);

void RegrAlignAppendPartialSlices(vec::VectorBatch *result, vec::VectorBatch *input,
    const std::vector<int32_t> &channels, int32_t numCols, int32_t rowCount);

void RegrAlignAppendPartialColumnsWithSkip(vec::VectorBatch *result, vec::VectorBatch *input,
    const std::vector<int32_t> &channels, int32_t numCols, int32_t rowCount,
    const std::shared_ptr<vec::NullsHelper> &rowSkip, bool firstColIsLong,
    omniruntime::mem::OmniTaskMemoryPool *pool);

/** regr_replacement / RegrReplacement partial: Spark-compatible DOUBLE n + avg DOUBLE + m2 DOUBLE. */
void RegrAlignAppendEmptyReplacementPartial3(vec::VectorBatch *result,
    omniruntime::mem::OmniTaskMemoryPool *pool);
void RegrAlignAppendReplacementPartial3AllNullRows(vec::VectorBatch *result, int32_t rowCount,
    omniruntime::mem::OmniTaskMemoryPool *pool);
void RegrAlignAppendReplacementPartial3RawDouble(vec::VectorBatch *result, vec::BaseVector *valVec,
    const std::shared_ptr<vec::NullsHelper> &rowSkip, int32_t rowCount,
    omniruntime::mem::OmniTaskMemoryPool *pool);

} // namespace omniruntime::op

#endif // OMNI_RUNTIME_REGR_ALIGN_SCHEMA_HELPER_H
