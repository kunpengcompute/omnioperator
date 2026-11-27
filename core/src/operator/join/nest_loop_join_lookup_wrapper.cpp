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

/*
 * @Description: lookup outer join implementations
 */

#include "hash_builder.h"
#include "nest_loop_join_lookup_wrapper.h"

namespace omniruntime {
namespace op {
NestLoopJoinLookupWrapperOperatorFactory::NestLoopJoinLookupWrapperOperatorFactory(
    NestLoopJoinLookupOperatorFactory &nestLoopJoinLookupOperatorFactory)
    : nestLoopJoinLookupOperatorFactory(&nestLoopJoinLookupOperatorFactory)
{
}

NestLoopJoinLookupWrapperOperatorFactory::~NestLoopJoinLookupWrapperOperatorFactory()
{
    if (nestLoopJoinLookupOperatorFactory != nullptr) {
        delete nestLoopJoinLookupOperatorFactory;
        nestLoopJoinLookupOperatorFactory = nullptr;
    }
}

NestLoopJoinLookupWrapperOperatorFactory *
NestLoopJoinLookupWrapperOperatorFactory::CreateNestLoopJoinLookupWrapperOperatorFactory(
    std::shared_ptr<const NestedLoopJoinNode> planNode, NestedLoopJoinBuildOperatorFactory *builderOperatorFactory,
    const config::QueryConfig &queryConfig)
{
    auto nestLoopJoinLookupOperatorFactory = NestLoopJoinLookupOperatorFactory::CreateNestLoopJoinLookupOperatorFactory(
        planNode, builderOperatorFactory, queryConfig);
    return new NestLoopJoinLookupWrapperOperatorFactory(*nestLoopJoinLookupOperatorFactory);
}

Operator *NestLoopJoinLookupWrapperOperatorFactory::CreateOperator()
{
    auto pNestLoopJoinLookupWrapperOperator = new NestLoopJoinLookupWrapperOperator(*nestLoopJoinLookupOperatorFactory);
    return pNestLoopJoinLookupWrapperOperator;
}

NestLoopJoinLookupWrapperOperator::NestLoopJoinLookupWrapperOperator(
    NestLoopJoinLookupOperatorFactory &nestLoopJoinLookupOperatorFactory)
    : nestLoopJoinLookupOperator(nullptr), nestLoopJoinLookupOperatorFactory(&nestLoopJoinLookupOperatorFactory)
{
}

NestLoopJoinLookupWrapperOperator::~NestLoopJoinLookupWrapperOperator()
{
    if (nestLoopJoinLookupOperator != nullptr) {
        delete nestLoopJoinLookupOperator;
        nestLoopJoinLookupOperator = nullptr;
    }
}

void NestLoopJoinLookupWrapperOperator::InitOperator()
{
    this->nestLoopJoinLookupOperator =
        dynamic_cast<NestLoopJoinLookupOperator *>(nestLoopJoinLookupOperatorFactory->CreateOperator());
    this->nestLoopJoinLookupOperator->setNoMoreInput(false);
}

int32_t NestLoopJoinLookupWrapperOperator::AddInput(VectorBatch *vecBatch)
{
    if (this->nestLoopJoinLookupOperator == nullptr) {
        InitOperator();
    }

    auto result = nestLoopJoinLookupOperator->AddInput(vecBatch);
    // set operator pipeline status
    SetStatus(OMNI_STATUS_NORMAL);
    return result;
}

int32_t NestLoopJoinLookupWrapperOperator::GetOutput(VectorBatch **outputVecBatch)
{
    if (this->isFinished()) {
        return 0;
    }

    if (this->nestLoopJoinLookupOperator == nullptr) {
        if (noMoreInput_) {
            SetStatus(OMNI_STATUS_FINISHED);
        }
        return 0;
    }

    if (!nestLoopJoinLookupOperator->isFinished()) {
        auto result = nestLoopJoinLookupOperator->GetOutput(outputVecBatch);
        if (nestLoopJoinLookupOperator->isFinished()) {
            SetStatus(OMNI_STATUS_FINISHED);
        }
        return result;
    }

    return 0;
}

OmniStatus NestLoopJoinLookupWrapperOperator::Close()
{
    if (nestLoopJoinLookupOperator != nullptr) {
        nestLoopJoinLookupOperator->Close();
        delete nestLoopJoinLookupOperator;
        nestLoopJoinLookupOperator = nullptr;
    }
    return OMNI_STATUS_NORMAL;
}

} // namespace op
} // namespace omniruntime
