/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 *
 * @Description: dynamic pages index implementations
 */
#include "dynamic_pages_index.h"
#include "operator/pages_index.h"
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

    this->positionCount += rowCount;
    this->vecBatchFreeFlagDeque.emplace_back(false);
    this->vectorBatchDeque.emplace_back(vecBatch);

    // generate value address.
    int32_t vecBatchLastIndex = this->columnsDeque.size();
    for (int32_t rowIdx = 0; rowIdx < rowCount; rowIdx++) {
        int64_t valueAddress = EncodeSyntheticAddress(vecBatchLastIndex, rowIdx);
        this->valueAddressesDeque.emplace_back(valueAddress);
    }

    std::vector<bool> nulls = CalculateNullsFromRawVectorBatch(vecBatch);
    this->nullsDeque.insert(this->nullsDeque.end(), nulls.begin(), nulls.end());
    this->columnsDeque.emplace_back(vecBatch->GetVectors());

    return 0;
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
