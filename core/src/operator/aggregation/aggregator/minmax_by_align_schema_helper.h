/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Align aggregate schema (skip partial) helpers for min_by / max_by aggregators.
 */
#ifndef OMNI_RUNTIME_MINMAX_BY_ALIGN_SCHEMA_HELPER_H
#define OMNI_RUNTIME_MINMAX_BY_ALIGN_SCHEMA_HELPER_H

#include "typed_aggregator.h"
#include "complex_aggregator_util.h"
#include "vector/nulls_buffer.h"
#include "vector/unsafe_vector.h"
#include "vector/vector_helper.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace omniruntime::op {

using omniruntime::vec::BaseVector;
using omniruntime::vec::NullsHelper;
using omniruntime::vec::Vector;
using omniruntime::vec::VectorBatch;
using omniruntime::vec::VectorHelper;

/** Row skip for align: null sort key OR (when filter active) filter=false. */
std::shared_ptr<NullsHelper> MinMaxByAlignMergeSortKeySkip(BaseVector *sortKeyVec, Vector<bool> *filterVec,
    bool filterIsActive, int32_t rowCount);

void MinMaxByAlignAppendPartialSlices(VectorBatch *result, VectorBatch *input, const std::vector<int32_t> &channels,
    int32_t rowCount);

template <type::DataTypeId COL1_ID, type::DataTypeId COL2_ID>
void MinMaxByAlignAppendEmptyPartial2(VectorBatch *result, mem::OmniTaskMemoryPool *pool)
{
    result->Append(VectorHelper::CreateVector(pool, OMNI_FLAT, COL1_ID, 0));
    result->Append(VectorHelper::CreateVector(pool, OMNI_FLAT, COL2_ID, 0));
}

template <type::DataTypeId COL1_ID, type::DataTypeId COL2_ID>
void MinMaxByAlignAppendRawPartial2(VectorBatch *result, BaseVector *col1Vec, BaseVector *col2Vec,
    const std::shared_ptr<NullsHelper> &rowSkip, int32_t rowCount, mem::OmniTaskMemoryPool *pool)
{
    using TargetVec = std::conditional_t<COL1_ID == type::OMNI_VARCHAR || COL1_ID == type::OMNI_CHAR ||
            COL1_ID == type::OMNI_VARBINARY,
        Vector<LargeStringContainer<std::string_view>>, typename AggNativeAndVectorType<COL1_ID>::vector>;
    using SortKeyVec = std::conditional_t<COL2_ID == type::OMNI_VARCHAR || COL2_ID == type::OMNI_CHAR ||
            COL2_ID == type::OMNI_VARBINARY,
        Vector<LargeStringContainer<std::string_view>>, typename AggNativeAndVectorType<COL2_ID>::vector>;

    auto *targetOut = static_cast<TargetVec *>(VectorHelper::CreateVector(pool, OMNI_FLAT, COL1_ID, rowCount));
    auto *sortKeyOut = static_cast<SortKeyVec *>(VectorHelper::CreateVector(pool, OMNI_FLAT, COL2_ID, rowCount));
    for (int32_t i = 0; i < rowCount; ++i) {
        const bool skip = (rowSkip != nullptr && (*rowSkip)[i]) || col2Vec->IsNull(i);
        if (skip) {
            targetOut->SetNull(i);
            sortKeyOut->SetNull(i);
            continue;
        }
        sortKeyOut->SetValue(i, VectorHelper::GetFlatValue<COL2_ID>(col2Vec, i));
        if (col1Vec->IsNull(i)) {
            targetOut->SetNull(i);
        } else {
            targetOut->SetValue(i, VectorHelper::GetFlatValue<COL1_ID>(col1Vec, i));
        }
    }
    result->Append(targetOut);
    result->Append(sortKeyOut);
}

template <type::DataTypeId COL1_ID, type::DataTypeId COL2_ID>
void MinMaxByAlignAppendPartialWithSkip(VectorBatch *result, VectorBatch *input, const std::vector<int32_t> &channels,
    int32_t rowCount, const std::shared_ptr<NullsHelper> &rowSkip, mem::OmniTaskMemoryPool *pool)
{
    if (rowSkip == nullptr) {
        MinMaxByAlignAppendPartialSlices(result, input, channels, rowCount);
        return;
    }
    MinMaxByAlignAppendRawPartial2<COL1_ID, COL2_ID>(result, input->Get(channels[0]), input->Get(channels[1]), rowSkip,
        rowCount, pool);
}

template <type::DataTypeId COL1_ID, type::DataTypeId COL2_ID>
void MinMaxByAlignAggSchema(VectorBatch *result, VectorBatch *inputVecBatch, const std::vector<int32_t> &channels,
    bool inputRaw, mem::OmniTaskMemoryPool *pool)
{
    const int32_t rowCount = inputVecBatch->GetRowCount();
    if (rowCount == 0) {
        MinMaxByAlignAppendEmptyPartial2<COL1_ID, COL2_ID>(result, pool);
        return;
    }
    if (!inputRaw) {
        MinMaxByAlignAppendPartialSlices(result, inputVecBatch, channels, rowCount);
        return;
    }
    MinMaxByAlignAppendRawPartial2<COL1_ID, COL2_ID>(result, inputVecBatch->Get(channels[0]),
        inputVecBatch->Get(channels[1]), nullptr, rowCount, pool);
}

template <type::DataTypeId COL1_ID, type::DataTypeId COL2_ID>
void MinMaxByAlignAggSchemaWithFilter(VectorBatch *result, VectorBatch *inputVecBatch,
    const std::vector<int32_t> &channels, bool inputRaw, int32_t filterIndex, mem::OmniTaskMemoryPool *pool)
{
    const int32_t rowCount = inputVecBatch->GetRowCount();
    if (rowCount == 0) {
        MinMaxByAlignAppendEmptyPartial2<COL1_ID, COL2_ID>(result, pool);
        return;
    }
    auto *filterVec = static_cast<Vector<bool> *>(inputVecBatch->Get(filterIndex));
    const bool needFilter = Aggregator::DoNeedHandleAggFilter(filterVec, 0, rowCount);
    auto *sortKeyVec = inputVecBatch->Get(channels[1]);
    std::shared_ptr<NullsHelper> rowSkip = MinMaxByAlignMergeSortKeySkip(sortKeyVec, filterVec, needFilter, rowCount);
    if (!inputRaw) {
        MinMaxByAlignAppendPartialWithSkip<COL1_ID, COL2_ID>(result, inputVecBatch, channels, rowCount, rowSkip, pool);
        return;
    }
    MinMaxByAlignAppendRawPartial2<COL1_ID, COL2_ID>(result, inputVecBatch->Get(channels[0]), sortKeyVec, rowSkip,
        rowCount, pool);
}

inline void MinMaxByComplexAlignAppendEmptyPartial2(VectorBatch *result, type::DataTypeId sortKeyTypeId,
    type::DataTypePtr targetColDataType, mem::OmniTaskMemoryPool *pool)
{
    result->Append(VectorHelper::CreateComplexVector(pool, targetColDataType.get(), 0));
    result->Append(VectorHelper::CreateVector(pool, OMNI_FLAT, sortKeyTypeId, 0));
}

template <type::DataTypeId COL2_ID>
void MinMaxByComplexAlignAppendPartial2Rows(VectorBatch *result, BaseVector *col1Vec, BaseVector *col2Vec,
    const std::shared_ptr<NullsHelper> &rowSkip, int32_t rowCount, type::DataTypeId targetColTypeId,
    type::DataTypePtr targetColDataType, mem::OmniTaskMemoryPool *pool)
{
    using SortKeyVec = std::conditional_t<COL2_ID == type::OMNI_VARCHAR || COL2_ID == type::OMNI_CHAR ||
            COL2_ID == type::OMNI_VARBINARY,
        Vector<LargeStringContainer<std::string_view>>, typename AggNativeAndVectorType<COL2_ID>::vector>;

    BaseVector *targetOut = VectorHelper::CreateComplexVector(pool, targetColDataType.get(), rowCount);
    auto *sortKeyOut = static_cast<SortKeyVec *>(VectorHelper::CreateVector(pool, OMNI_FLAT, COL2_ID, rowCount));
    for (int32_t i = 0; i < rowCount; ++i) {
        const bool skip = (rowSkip != nullptr && (*rowSkip)[i]) || col2Vec->IsNull(i);
        if (skip) {
            targetOut->SetNull(i);
            sortKeyOut->SetNull(i);
            continue;
        }
        sortKeyOut->SetValue(i, VectorHelper::GetFlatValue<COL2_ID>(col2Vec, i));
        if (col1Vec->IsNull(i)) {
            targetOut->SetNull(i);
            continue;
        }
        BaseVector *slice = GetComplexColSlice(col1Vec, targetColTypeId, i);
        if (IsComplexSliceNull(slice, targetColTypeId)) {
            targetOut->SetNull(i);
        } else {
            SetComplexColValue(targetOut, targetColTypeId, i, slice);
        }
        if (slice != nullptr) {
            ReleaseComplexSliceCopy(slice, targetColTypeId);
        }
    }
    result->Append(targetOut);
    result->Append(sortKeyOut);
}

template <type::DataTypeId COL2_ID>
void MinMaxByComplexAlignAppendPartialWithSkip(VectorBatch *result, VectorBatch *input,
    const std::vector<int32_t> &channels, int32_t rowCount, const std::shared_ptr<NullsHelper> &rowSkip,
    type::DataTypeId targetColTypeId, type::DataTypePtr targetColDataType, mem::OmniTaskMemoryPool *pool)
{
    if (rowSkip == nullptr) {
        MinMaxByAlignAppendPartialSlices(result, input, channels, rowCount);
        return;
    }
    MinMaxByComplexAlignAppendPartial2Rows<COL2_ID>(result, input->Get(channels[0]), input->Get(channels[1]), rowSkip,
        rowCount, targetColTypeId, targetColDataType, pool);
}

template <type::DataTypeId COL2_ID>
void MinMaxByComplexAlignAggSchema(VectorBatch *result, VectorBatch *inputVecBatch,
    const std::vector<int32_t> &channels, bool inputRaw, type::DataTypeId targetColTypeId,
    type::DataTypePtr targetColDataType, mem::OmniTaskMemoryPool *pool)
{
    const int32_t rowCount = inputVecBatch->GetRowCount();
    if (rowCount == 0) {
        MinMaxByComplexAlignAppendEmptyPartial2(result, COL2_ID, targetColDataType, pool);
        return;
    }
    if (!inputRaw) {
        MinMaxByAlignAppendPartialSlices(result, inputVecBatch, channels, rowCount);
        return;
    }
    MinMaxByComplexAlignAppendPartial2Rows<COL2_ID>(result, inputVecBatch->Get(channels[0]),
        inputVecBatch->Get(channels[1]), nullptr, rowCount, targetColTypeId, targetColDataType, pool);
}

template <type::DataTypeId COL2_ID>
void MinMaxByComplexAlignAggSchemaWithFilter(VectorBatch *result, VectorBatch *inputVecBatch,
    const std::vector<int32_t> &channels, bool inputRaw, int32_t filterIndex, type::DataTypeId targetColTypeId,
    type::DataTypePtr targetColDataType, mem::OmniTaskMemoryPool *pool)
{
    const int32_t rowCount = inputVecBatch->GetRowCount();
    if (rowCount == 0) {
        MinMaxByComplexAlignAppendEmptyPartial2(result, COL2_ID, targetColDataType, pool);
        return;
    }
    auto *filterVec = static_cast<Vector<bool> *>(inputVecBatch->Get(filterIndex));
    const bool needFilter = Aggregator::DoNeedHandleAggFilter(filterVec, 0, rowCount);
    auto *sortKeyVec = inputVecBatch->Get(channels[1]);
    std::shared_ptr<NullsHelper> rowSkip = MinMaxByAlignMergeSortKeySkip(sortKeyVec, filterVec, needFilter, rowCount);
    if (!inputRaw) {
        MinMaxByComplexAlignAppendPartialWithSkip<COL2_ID>(result, inputVecBatch, channels, rowCount, rowSkip,
            targetColTypeId, targetColDataType, pool);
        return;
    }
    MinMaxByComplexAlignAppendPartial2Rows<COL2_ID>(result, inputVecBatch->Get(channels[0]), sortKeyVec, rowSkip,
        rowCount, targetColTypeId, targetColDataType, pool);
}

} // namespace omniruntime::op

#endif // OMNI_RUNTIME_MINMAX_BY_ALIGN_SCHEMA_HELPER_H
