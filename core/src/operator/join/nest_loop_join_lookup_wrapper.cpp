/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2022-2024. All rights reserved.
 * @Description: lookup outer join implementations
 */

#include "hash_builder.h"
#include "nest_loop_join_lookup_wrapper.h"

namespace omniruntime {
namespace op {
NestLoopJoinLookupWrapperOperatorFactory::NestLoopJoinLookupWrapperOperatorFactory(NestLoopJoinLookupOperatorFactory &nestLoopJoinLookupOperatorFactory)
    : nestLoopJoinLookupOperatorFactory(&nestLoopJoinLookupOperatorFactory) {}

NestLoopJoinLookupWrapperOperatorFactory::~NestLoopJoinLookupWrapperOperatorFactory()
{
    if (nestLoopJoinLookupOperatorFactory != nullptr) {
        delete nestLoopJoinLookupOperatorFactory;
        nestLoopJoinLookupOperatorFactory = nullptr;
    }
}

NestLoopJoinLookupWrapperOperatorFactory *NestLoopJoinLookupWrapperOperatorFactory::CreateNestLoopJoinLookupWrapperOperatorFactory(std::shared_ptr<const NestedLoopJoinNode> planNode,
    NestedLoopJoinBuildOperatorFactory* builderOperatorFactory, const config::QueryConfig& queryConfig)
{
    auto nestLoopJoinLookupOperatorFactory = NestLoopJoinLookupOperatorFactory::CreateNestLoopJoinLookupOperatorFactory(planNode, builderOperatorFactory, queryConfig);
    return new NestLoopJoinLookupWrapperOperatorFactory(*nestLoopJoinLookupOperatorFactory);
}

// Pool-bound override constructs wrapper with deferred pool.
Operator *NestLoopJoinLookupWrapperOperatorFactory::CreateOperator(omniruntime::mem::OmniTaskMemoryPool *pool)
{
    auto pNestLoopJoinLookupWrapperOperator = new NestLoopJoinLookupWrapperOperator(pool,
        *nestLoopJoinLookupOperatorFactory);
    return pNestLoopJoinLookupWrapperOperator;
}

// Pool-bound ctor — chains Operator(pool); stores pool for deferred InitOperator().
NestLoopJoinLookupWrapperOperator::NestLoopJoinLookupWrapperOperator(omniruntime::mem::OmniTaskMemoryPool *pool,
    NestLoopJoinLookupOperatorFactory &nestLoopJoinLookupOperatorFactory)
    : Operator(pool),
      nestLoopJoinLookupOperator(nullptr),
      nestLoopJoinLookupOperatorFactory(&nestLoopJoinLookupOperatorFactory),
      deferredInnerPool_(pool) {}

NestLoopJoinLookupWrapperOperator::~NestLoopJoinLookupWrapperOperator()
{
    if (nestLoopJoinLookupOperator != nullptr) {
        delete nestLoopJoinLookupOperator;
        nestLoopJoinLookupOperator = nullptr;
    }
}

void NestLoopJoinLookupWrapperOperator::InitOperator()
{
    // Forward deferred pool if constructed via pool-bound ctor.
    this->nestLoopJoinLookupOperator = dynamic_cast<NestLoopJoinLookupOperator*>(
        nestLoopJoinLookupOperatorFactory->CreateOperator(deferredInnerPool_));
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

}
}
