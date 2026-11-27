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

#include "union.h"
#include "vector/vector_helper.h"

using namespace std;
using namespace omniruntime::vec;
namespace omniruntime {
namespace op {
UnionOperatorFactory::UnionOperatorFactory(const type::DataTypes &sourceTypes, int32_t sourceTypesCount,
                                           bool isDistinct)
    : sourceTypes(sourceTypes), sourceTypesCount(sourceTypesCount), isDistinct(isDistinct)
{
}

UnionOperatorFactory::~UnionOperatorFactory() {}

UnionOperatorFactory *UnionOperatorFactory::CreateUnionOperatorFactory(const type::DataTypes &sourceTypesField,
                                                                       int32_t sourceTypesCountField, bool distinct)
{
    auto uOperatorFactory = new UnionOperatorFactory(sourceTypesField, sourceTypesCountField, distinct);
    return uOperatorFactory;
}

UnionOperatorFactory *UnionOperatorFactory::CreateUnionOperatorFactory(std::shared_ptr<const UnionNode> planNode)
{
    auto dataTypes = planNode->GetSourceTypes();
    auto unionOperatorFactory =
        new UnionOperatorFactory(*dataTypes.get(), (*dataTypes).GetSize(), planNode->IsDistinct());
    return unionOperatorFactory;
}

Operator *UnionOperatorFactory::CreateOperator()
{
    UnionOperator *unionOperator = new UnionOperator(sourceTypes, sourceTypesCount, isDistinct);
    return unionOperator;
}

UnionOperator::UnionOperator(const type::DataTypes &sourceTypes, int32_t sourceTypesCount, bool isDistinct)
    : sourceTypes(sourceTypes), sourceTypesCount(sourceTypesCount), isDistinct(isDistinct)
{
}

UnionOperator::~UnionOperator() = default;

int32_t UnionOperator::AddInput(VectorBatch *vecBatch)
{
    int32_t rowCount = vecBatch->GetRowCount();
    if (rowCount <= 0) {
        VectorHelper::FreeVecBatch(vecBatch);
        ResetInputVecBatch();
        return 0;
    }
    int32_t vectorCount = vecBatch->GetVectorCount();
    auto outBatch = new VectorBatch(rowCount);
    for (int32_t i = 0; i < vectorCount; ++i) {
        BaseVector *inputVector = vecBatch->Get(i);
        outBatch->Append(VectorHelper::SliceVector(inputVector, 0, rowCount));
    }
    VectorHelper::FreeVecBatch(vecBatch);
    ResetInputVecBatch();
    inputVecBatches.push_back(outBatch);
    vecBatchCount++;
    return 0;
}

int32_t UnionOperator::GetOutput(VectorBatch **outputVecBatch)
{
    if (vecBatchCount == 0 || vecBatchIndex == vecBatchCount) {
        vecBatchCount = 0;
        vecBatchIndex = 0;
        inputVecBatches.clear();
        if (noMoreInput_) {
            SetStatus(OMNI_STATUS_FINISHED);
        }
        return 0;
    }

    *outputVecBatch = inputVecBatches[vecBatchIndex];
    vecBatchIndex++;
    if (vecBatchIndex == vecBatchCount) {
        vecBatchCount = 0;
        vecBatchIndex = 0;
        inputVecBatches.clear();
        if (noMoreInput_) {
            SetStatus(OMNI_STATUS_FINISHED);
        }
    }

    return 0;
}

OmniStatus UnionOperator::Close()
{
    if (!inputVecBatches.empty()) {
        VectorHelper::FreeVecBatches(inputVecBatches);
        inputVecBatches.clear();
    }
    return OMNI_STATUS_NORMAL;
}

BlockingReason UnionOperator::IsBlocked(ContinueFuture *future)
{
    if (inputOperatorCnt_ > 1) {
        return BlockingReason::kWaitForUnionBuild;
    }
    return BlockingReason::kNotBlocked;
}
} // namespace op
} // namespace omniruntime
