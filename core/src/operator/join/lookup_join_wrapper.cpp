/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
 * KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 */

/*
 * @Description: lookup outer join implementations
 */

#include "hash_builder_expr.h"
#include "lookup_join_wrapper.h"

namespace omniruntime {
namespace op {
LookupJoinWrapperOperatorFactory::LookupJoinWrapperOperatorFactory(
    LookupJoinWithExprOperatorFactory &lookupJoinWithExprOperatorFactory,
    LookupOuterJoinWithExprOperatorFactory &lookupOuterJoinWithExprOperatorFactory, bool isNeedOuterJoin)
    : lookupJoinWithExprOperatorFactory(&lookupJoinWithExprOperatorFactory),
      lookupOuterJoinWithExprOperatorFactory(&lookupOuterJoinWithExprOperatorFactory), isNeedOuterJoin(isNeedOuterJoin)
{
}

LookupJoinWrapperOperatorFactory::~LookupJoinWrapperOperatorFactory()
{
    delete lookupOuterJoinWithExprOperatorFactory;
    delete lookupJoinWithExprOperatorFactory;
    lookupOuterJoinWithExprOperatorFactory = nullptr;
    lookupJoinWithExprOperatorFactory = nullptr;
}

LookupJoinWrapperOperatorFactory *LookupJoinWrapperOperatorFactory::CreateLookupJoinWrapperOperatorFactory(
    std::shared_ptr<const HashJoinNode> planNode, HashBuilderWithExprOperatorFactory *hashBuilderOperatorFactory,
    const config::QueryConfig &queryConfig)
{
    auto isNeedOuterJoin = planNode->IsFullJoin() || (planNode->IsLeftJoin() && planNode->IsBuildLeft()) ||
                           (planNode->IsRightJoin() && planNode->IsBuildRight());
    auto lookupJoinWithExprOperatorFactory = LookupJoinWithExprOperatorFactory::CreateLookupJoinWithExprOperatorFactory(
        planNode, hashBuilderOperatorFactory, queryConfig);
    auto lookupOuterJoinWithExprOperatorFactory =
        LookupOuterJoinWithExprOperatorFactory::CreateLookupOuterJoinWithExprOperatorFactory(
            planNode, hashBuilderOperatorFactory, queryConfig);
    return new LookupJoinWrapperOperatorFactory(*lookupJoinWithExprOperatorFactory,
                                                *lookupOuterJoinWithExprOperatorFactory, isNeedOuterJoin);
}

Operator *LookupJoinWrapperOperatorFactory::CreateOperator()
{
    auto lookupJoinWithExprOperator =
        dynamic_cast<LookupJoinWithExprOperator *>(lookupJoinWithExprOperatorFactory->CreateOperator());
    auto lookupOuterJoinWithExprOperator =
        dynamic_cast<LookupOuterJoinWithExprOperator *>(lookupOuterJoinWithExprOperatorFactory->CreateOperator());
    auto pLookupJoinWrapperOperator =
        new LookupJoinWrapperOperator(*lookupJoinWithExprOperator, *lookupOuterJoinWithExprOperator, isNeedOuterJoin);
    return pLookupJoinWrapperOperator;
}

LookupJoinWrapperOperator::LookupJoinWrapperOperator(LookupJoinWithExprOperator &lookupJoinWithExprOperator,
                                                     LookupOuterJoinWithExprOperator &lookupOuterJoinWithExprOperator,
                                                     bool isNeedOuterJoin)
    : lookupOuterJoinWithExprOperator(&lookupOuterJoinWithExprOperator),
      lookupJoinWithExprOperator(&lookupJoinWithExprOperator), isNeedOuterJoin(isNeedOuterJoin)
{
    SetOperatorName(opNameForLookUpJoin);
}

LookupJoinWrapperOperator::~LookupJoinWrapperOperator()
{
    delete lookupOuterJoinWithExprOperator;
    delete lookupJoinWithExprOperator;
    lookupOuterJoinWithExprOperator = nullptr;
    lookupJoinWithExprOperator = nullptr;
}

int32_t LookupJoinWrapperOperator::AddInput(VectorBatch *vecBatch)
{
    auto result = lookupJoinWithExprOperator->AddInput(vecBatch);
    // set operator pipeline status
    SetStatus(OMNI_STATUS_NORMAL);
    return result;
}

int32_t LookupJoinWrapperOperator::GetOutput(VectorBatch **outputVecBatch)
{
    if (this->isFinished()) {
        return 0;
    }

    if (lookupJoinWithExprOperator->GetStatus() != OMNI_STATUS_FINISHED) {
        return lookupJoinWithExprOperator->GetOutput(outputVecBatch);
    } else {
        if (isNeedOuterJoin) {
            // when lookup join operator end can execute
            auto result = lookupOuterJoinWithExprOperator->GetOutput(outputVecBatch);
            if (lookupOuterJoinWithExprOperator->GetStatus() == OMNI_STATUS_FINISHED) {
                SetStatus(OMNI_STATUS_FINISHED);
            }
            return result;
        } else {
            SetStatus(OMNI_STATUS_FINISHED);
            return 0;
        }
    }
}

OmniStatus LookupJoinWrapperOperator::Close()
{
    lookupJoinWithExprOperator->Close();
    lookupOuterJoinWithExprOperator->Close();
    return OMNI_STATUS_NORMAL;
}

} // namespace op
} // namespace omniruntime
