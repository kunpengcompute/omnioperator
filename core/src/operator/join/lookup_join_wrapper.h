/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2021-2024. All rights reserved.
 * @Description: lookup join implementations
 */

#ifndef LOOKUP_JOIN_WRAPPER_H
#define LOOKUP_JOIN_WRAPPER_H

#include "operator/operator.h"
#include "operator/operator_factory.h"
#include "type/data_types.h"
#include "type/data_type.h"
#include "hash_builder_expr.h"
#include "operator/join/lookup_join_expr.h"
#include "operator/join/lookup_outer_join_expr.h"

namespace omniruntime {
namespace op {
class LookupJoinWrapperOperatorFactory : public OperatorFactory {
public:
    LookupJoinWrapperOperatorFactory(LookupJoinWithExprOperatorFactory &lookupJoinWithExprOperatorFactory,
       LookupOuterJoinWithExprOperatorFactory &lookupOuterJoinWithExprOperatorFactory, bool isNeedOuterJoin);
    ~LookupJoinWrapperOperatorFactory() override;
    static LookupJoinWrapperOperatorFactory *CreateLookupJoinWrapperOperatorFactory(std::shared_ptr<const HashJoinNode> planNode,
        HashBuilderOperatorFactory* hashBuilderOperatorFactory, const config::QueryConfig& queryConfig);
    Operator *CreateOperator() override;

private:
    LookupJoinWithExprOperatorFactory *lookupJoinWithExprOperatorFactory;
    LookupOuterJoinWithExprOperatorFactory *lookupOuterJoinWithExprOperatorFactory;
    bool isNeedOuterJoin;
};

class LookupJoinWrapperOperator : public Operator {
public:
    LookupJoinWrapperOperator(LookupJoinWithExprOperator &lookupJoinWithExprOperatorFactory,
        LookupOuterJoinWithExprOperator &lookupOuterJoinWithExprOperatorFactory, bool isNeedOuterJoin);
    ~LookupJoinWrapperOperator() override;
    int32_t AddInput(omniruntime::vec::VectorBatch *vecBatch) override;
    int32_t GetOutput(omniruntime::vec::VectorBatch **outputVecBatch) override;
    BlockingReason IsBlocked(ContinueFuture *future) override;
    bool needsInput() override;
    OmniStatus Close() override;

    void noMoreInput() override
    {
        noMoreInput_ = true;
        lookupJoinWithExprOperator->noMoreInput();
    }

    void setNoMoreInput(bool noMoreInput) override
    {
        noMoreInput_ = noMoreInput;
        lookupJoinWithExprOperator->setNoMoreInput(noMoreInput);
    }

private:
    LookupOuterJoinWithExprOperator *lookupOuterJoinWithExprOperator;
    LookupJoinWithExprOperator *lookupJoinWithExprOperator;
    bool isNeedOuterJoin;
};

}
}

#endif
