/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
 * @Description: dynamic pages index implementations
 */
#include "dynamic_pages_index.h"
#include <algorithm>
#include <climits>
#include "operator/pages_index.h"
#include "util/omni_exception.h"
#include "vector/vector_helper.h"

namespace omniruntime {
namespace op {
using namespace omniruntime::vec;

// function implements for class PagesIndex
DynamicPagesIndex::DynamicPagesIndex(const omniruntime::type::DataTypes &types, int32_t *computeCols,
    int32_t computeColsCount)
    : typesCount(types.GetSize()),
      computeCols(computeCols),
      computeColsCount(computeColsCount),
      positionCount(0),
      finishAddData(false),
      dataTypes(types)
{}

std::vector<bool> DynamicPagesIndex::CalculateNullsFromRawVectorBatch(VectorBatch *vectorBatch)
{
    std::vector<bool> result;
    result.resize(vectorBatch->GetRowCount(), false);
    for (int32_t id = 0; id < computeColsCount; ++id) {
        auto colIdx = computeCols[id];
        auto vec = vectorBatch->Get(colIdx);
        auto totalRowSize = vec->GetSize();
        auto nullSize = vec->GetNullCount();
        if (nullSize == 0) {
            continue;
        }
        auto nullValues = unsafe::UnsafeBaseVector::GetNullsHelper(vec);
        for (int32_t rowId = 0; rowId < totalRowSize && nullSize > 0; ++rowId) {
            if ((*nullValues)[rowId]) {
                --nullSize;
                result[rowId] = true;
            }
        }
    }
    return result;
}

int32_t DynamicPagesIndex::AddVecBatch(omniruntime::vec::VectorBatch *vecBatch)
{
    if (finishAddData) {
        return 0;
    }

    int32_t rowCount = vecBatch->GetRowCount();
    if (rowCount == 0) {
        // no more vector batch will add
        this->finishAddData = true;
        this->vecBatchFreeFlagDeque.emplace_back(false);
        this->vectorBatchDeque.emplace_back(vecBatch);
        return 0;
    }

    const int64_t newPositionCount = static_cast<int64_t>(this->positionCount) + rowCount;
    if (UNLIKELY(newPositionCount > INT32_MAX)) {
        throw omniruntime::exception::OmniException(
            "OPERATOR_RUNTIME_ERROR", "SMJ DynamicPagesIndex row count exceeds int32 range");
    }
    this->positionCount = static_cast<int32_t>(newPositionCount);
    this->batchRowOffsets.emplace_back(this->positionCount);
    this->vecBatchFreeFlagDeque.emplace_back(false);
    this->vectorBatchDeque.emplace_back(vecBatch);

    std::vector<bool> nulls = CalculateNullsFromRawVectorBatch(vecBatch);
    this->nullsDeque.insert(this->nullsDeque.end(), nulls.begin(), nulls.end());
    this->columnsDeque.emplace_back(vecBatch->GetVectors());

    return 0;
}

int64_t DynamicPagesIndex::GetValueAddresses(int32_t rowIndex) const
{
    AddressCursor cursor;
    UpdateAddressCursor(rowIndex, cursor);
    return static_cast<int64_t>(EncodeSyntheticAddress(static_cast<uint32_t>(cursor.batchId),
        static_cast<uint32_t>(rowIndex - cursor.batchStart)));
}

void DynamicPagesIndex::UpdateAddressCursor(int32_t rowIndex, AddressCursor &cursor) const
{
    if (UNLIKELY(rowIndex < 0 || rowIndex >= positionCount)) {
        throw omniruntime::exception::OmniException(
            "OPERATOR_RUNTIME_ERROR", "SMJ DynamicPagesIndex row index is out of range");
    }

    const int32_t batchCount = static_cast<int32_t>(batchRowOffsets.size()) - 1;
    if (cursor.batchId >= 0 && cursor.batchId < batchCount) {
        if (rowIndex >= cursor.batchEnd && cursor.batchId + 1 < batchCount) {
            const int32_t nextBatchEnd = batchRowOffsets[cursor.batchId + 2];
            if (rowIndex < nextBatchEnd) {
                cursor.batchId++;
                cursor.batchStart = cursor.batchEnd;
                cursor.batchEnd = nextBatchEnd;
                return;
            }
        } else if (rowIndex < cursor.batchStart && cursor.batchId > 0) {
            const int32_t previousBatchStart = batchRowOffsets[cursor.batchId - 1];
            if (rowIndex >= previousBatchStart) {
                cursor.batchId--;
                cursor.batchEnd = cursor.batchStart;
                cursor.batchStart = previousBatchStart;
                return;
            }
        }
    }

    const auto upper = std::upper_bound(batchRowOffsets.begin(), batchRowOffsets.end(), rowIndex);
    cursor.batchId = static_cast<int32_t>(upper - batchRowOffsets.begin()) - 1;
    cursor.batchStart = batchRowOffsets[cursor.batchId];
    cursor.batchEnd = batchRowOffsets[cursor.batchId + 1];
}

void DynamicPagesIndex::FreeBeforeVecBatch(int32_t vecBatchIdx)
{
    if (vecBatchIdx >= static_cast<int32_t>(this->vecBatchFreeFlagDeque.size()) ||
        vecBatchIdx - 1 <= lastFreedVecBatchIdx) {
        return;
    }
    for (int batchIdx = lastFreedVecBatchIdx + 1; batchIdx < vecBatchIdx; batchIdx++) {
        this->vecBatchFreeFlagDeque[batchIdx] = true;
        VectorHelper::FreeVecBatch(this->vectorBatchDeque[batchIdx]);
    }
    lastFreedVecBatchIdx = vecBatchIdx - 1;
}

void DynamicPagesIndex::FreeAllRemainingVecBatch()
{
    for (uint32_t idx = lastFreedVecBatchIdx + 1; idx < vectorBatchDeque.size(); idx++) {
        this->vecBatchFreeFlagDeque[idx] = true;
        VectorHelper::FreeVecBatch(this->vectorBatchDeque[idx]);
    }
    lastFreedVecBatchIdx = vectorBatchDeque.size() - 1;
}

DynamicPagesIndex::~DynamicPagesIndex() = default;
}
}
