/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2021-2024. All rights reserved.
 * @Description: lookup join implementations
 */

#ifndef NEST_LOOP_JOIN_LOOKUP_WRAPPER_H
#define NEST_LOOP_JOIN_LOOKUP_WRAPPER_H

#include "operator/operator.h"
#include "operator/operator_factory.h"
#include "type/data_types.h"
#include "type/data_type.h"
#include "operator/join/nest_loop_join_lookup.h"

namespace omniruntime {
namespace op {
class NestLoopJoinLookupWrapperOperatorFactory : public OperatorFactory {
public:
    explicit NestLoopJoinLookupWrapperOperatorFactory(NestLoopJoinLookupOperatorFactory &nestLoopJoinLookupOperatorFactory);
    ~NestLoopJoinLookupWrapperOperatorFactory() override;
    static NestLoopJoinLookupWrapperOperatorFactory *CreateNestLoopJoinLookupWrapperOperatorFactory(std::shared_ptr<const NestedLoopJoinNode> planNode,
        NestedLoopJoinBuildOperatorFactory* builderOperatorFactory, const config::QueryConfig& queryConfig);
    using OperatorFactory::CreateOperator;
    // Pool-bound override
    Operator *CreateOperator(omniruntime::mem::OmniTaskMemoryPool *pool) override;

private:
    NestLoopJoinLookupOperatorFactory *nestLoopJoinLookupOperatorFactory;
};

class NestLoopJoinLookupWrapperOperator : public Operator {
public:
    // Pool-bound ctor — wrapper remembers pool to forward to inner factory at InitOperator() time.
    NestLoopJoinLookupWrapperOperator(omniruntime::mem::OmniTaskMemoryPool *pool,
        NestLoopJoinLookupOperatorFactory &nestLoopJoinLookupOperatorFactory);
    ~NestLoopJoinLookupWrapperOperator() override;
    void InitOperator();
    int32_t AddInput(omniruntime::vec::VectorBatch *vecBatch) override;
    int32_t GetOutput(omniruntime::vec::VectorBatch **outputVecBatch) override;
    OmniStatus Close() override;

    void noMoreInput() override
    {
        noMoreInput_ = true;
        if (nestLoopJoinLookupOperator != nullptr) {
            nestLoopJoinLookupOperator->noMoreInput();
        }
    }

private:
    NestLoopJoinLookupOperator *nestLoopJoinLookupOperator;
    NestLoopJoinLookupOperatorFactory *nestLoopJoinLookupOperatorFactory;
    // Pool to forward to inner factory at deferred InitOperator() time (nullptr in legacy path).
    omniruntime::mem::OmniTaskMemoryPool *deferredInnerPool_ = nullptr;
};

}
}

#endif
