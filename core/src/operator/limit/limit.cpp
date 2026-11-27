/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "limit.h"
#include "util/type_util.h"
#include "vector/vector_helper.h"

using namespace std;
using namespace omniruntime::vec;
namespace omniruntime {
namespace op {
LimitOperatorFactory::LimitOperatorFactory(int32_t limit, int32_t offset) : limit(limit), offset(offset) {}

LimitOperatorFactory::~LimitOperatorFactory() {}

LimitOperatorFactory *LimitOperatorFactory::CreateLimitOperatorFactory(int32_t limitNum, int32_t offsetNum)
{
    return new LimitOperatorFactory(limitNum, offsetNum);
}

LimitOperatorFactory *LimitOperatorFactory::CreateLimitOperatorFactory(std::shared_ptr<const LimitNode> planNode)
{
    return new LimitOperatorFactory(planNode->Count(), planNode->Offset());
}

Operator *LimitOperatorFactory::CreateOperator() { return new LimitOperator(limit, offset); }

LimitOperator::LimitOperator(int32_t limit, int32_t offset)
    : remainingLimit(limit), remainingOffset(offset), outputVecBatch(nullptr)
{
}

LimitOperator::~LimitOperator() {}

int32_t LimitOperator::AddInput(VectorBatch *vecBatch)
{
    if (vecBatch == nullptr) {
        return 0;
    }

    if (vecBatch->GetRowCount() == 0 || outputVecBatch != nullptr || remainingLimit == 0) {
        VectorHelper::FreeVecBatch(vecBatch);
        ResetInputVecBatch();
        return 0;
    }

    int32_t rowCount = vecBatch->GetRowCount();
    int32_t vectorCount = vecBatch->GetVectorCount();

    // 1. calculate how many rows this batch needs to skip
    int32_t start = remainingOffset < rowCount ? remainingOffset : rowCount;

    // update offset
    remainingOffset -= start;

    // 2. the number of remaining lines after subtracting the offset
    int32_t rowsAfterOffset = rowCount - start;

    // 3. the maximum number of rows that can be output this time
    int32_t take;
    if (remainingLimit < 0) {
        take = rowsAfterOffset; // unlimited
    } else {
        take = rowsAfterOffset < remainingLimit ? rowsAfterOffset : remainingLimit;
        remainingLimit -= take;
    }

    // if there is no data to be output for this batch
    if (take == 0) {
        VectorHelper::FreeVecBatch(vecBatch);
        ResetInputVecBatch();
        return 0;
    }

    auto result = make_unique<VectorBatch>(take);

    for (int32_t i = 0; i < vectorCount; ++i) {
        BaseVector *inputVector = vecBatch->Get(i);
        result->Append(VectorHelper::SliceVector(inputVector, start, take));
    }

    VectorHelper::FreeVecBatch(vecBatch);
    ResetInputVecBatch();

    outputVecBatch = result.release();
    return 0;
}

int32_t LimitOperator::GetOutput(VectorBatch **resultVecBatch)
{
    if (outputVecBatch != nullptr) {
        *resultVecBatch = outputVecBatch;
        outputVecBatch = nullptr;
        return 0;
    }

    if (remainingLimit <= 0) {
        SetStatus(OMNI_STATUS_FINISHED);
        return 0;
    }
    if (noMoreInput_) {
        SetStatus(OMNI_STATUS_FINISHED);
    }
    return 0;
}

OmniStatus LimitOperator::Close()
{
    if (outputVecBatch != nullptr) {
        VectorHelper::FreeVecBatch(outputVecBatch);
        outputVecBatch = nullptr;
    }

    return OMNI_STATUS_NORMAL;
}
} // namespace op
} // namespace omniruntime
