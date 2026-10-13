/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2021-2023. All rights reserved.
 * @Description: sort implementations
 */

#ifndef __HASH_BUILDER_EXPR_H__
#define __HASH_BUILDER_EXPR_H__

#include <cstdint>
#include <functional>
#include "operator/operator_factory.h"
#include "type/data_types.h"
#include "operator/projection/projection.h"
#include "operator/join/hash_builder.h"

namespace omniruntime {
namespace op {
class HashBuilderWithExprNrsTestPeer;

class HashBuilderWithExprOperatorFactory : public OperatorFactory {
public:
    static HashBuilderWithExprOperatorFactory *CreateHashBuilderWithExprOperatorFactory(JoinType joinType,
        const type::DataTypes &buildTypes, const std::vector<omniruntime::expressions::Expr *> &buildHashKeys,
        int32_t hashTableCount, OverflowConfig *overflowConfig);

    static HashBuilderWithExprOperatorFactory *CreateHashBuilderWithExprOperatorFactory(JoinType joinType,
        BuildSide buildSide, const type::DataTypes &buildTypes,
        const std::vector<omniruntime::expressions::Expr *> &buildHashKeys, int32_t hashTableCount,
        OverflowConfig *overflowConfig);

    /// Target restore uses one resident hash-table slot for expression keys too.
    static HashBuilderWithExprOperatorFactory *CreateHashBuilderWithExprOperatorFactory(
        std::shared_ptr<const HashJoinNode> planNode, const config::QueryConfig &queryConfig,
        int32_t hashTableCount = 1);

    HashBuilderWithExprOperatorFactory(JoinType joinType, const DataTypes &buildTypes,
        const std::vector<omniruntime::expressions::Expr *> &buildHashKeys, int32_t hashTableCount,
        OverflowConfig *overflowConfig);

    HashBuilderWithExprOperatorFactory(JoinType joinType, BuildSide buildSide, const DataTypes &buildTypes,
        const std::vector<omniruntime::expressions::Expr *> &buildHashKeys, int32_t hashTableCount,
        OverflowConfig *overflowConfig);

    HashBuilderWithExprOperatorFactory::HashBuilderWithExprOperatorFactory(JoinType joinType, BuildSide buildSide,
        const type::DataTypes &buildTypes, const std::vector<omniruntime::expressions::Expr *> &buildHashKeys,
        int32_t hashTableCount, OverflowConfig *overflowConfig, const config::QueryConfig &queryConfig);

    ~HashBuilderWithExprOperatorFactory() override;

    using OperatorFactory::CreateOperator;
    // Pool-bound override
    omniruntime::op::Operator *CreateOperator(omniruntime::mem::OmniTaskMemoryPool *pool) override;

    HashBuilderOperatorFactory *GetHashBuilderOperatorFactory()
    {
        return operatorFactory;
    }

private:
    std::unique_ptr<DataTypes> buildTypes;
    std::vector<int32_t> buildHashCols;
    std::vector<std::unique_ptr<Projection>> projections;
    HashBuilderOperatorFactory *operatorFactory;
    config::QueryConfig queryConfig_{};
};

class HashBuilderWithExprOperator : public Operator {
public:
    // Pool-bound ctor chains Operator(pool)
    HashBuilderWithExprOperator(omniruntime::mem::OmniTaskMemoryPool *pool, const DataTypes &buildTypes,
        std::vector<std::unique_ptr<Projection>> &projections, HashBuilderOperator *hashBuilderOperator,
        const config::QueryConfig &queryConfig);

    ~HashBuilderWithExprOperator() override;

    int32_t AddInput(omniruntime::vec::VectorBatch *vecBatch) override;

    int32_t GetOutput(omniruntime::vec::VectorBatch **outputVecBatch) override;

    /// The native driver pumps this WithExpr wrapper, so the build-side restore yield
    /// signal must be forwarded from the inner HashBuilderOperator (default base returns kNotBlocked).
    BlockingReason IsBlocked(ContinueFuture *future) override;

    OmniStatus Close() override;

    void noMoreInput() override
    {
        noMoreInput_ = true;
        hashBuilderOperator->noMoreInput();
    }

    void setNoMoreInput(bool noMoreInput) override
    {
        noMoreInput_ = noMoreInput;
        hashBuilderOperator->setNoMoreInput(noMoreInput);
    }

private:
    using InnerNrsObserverForTests = std::function<void(HashBuilderOperator *)>;
    friend class HashBuilderWithExprNrsTestPeer;

    const DataTypes buildTypes;
    std::vector<std::unique_ptr<Projection>> &projections;
    HashBuilderOperator *hashBuilderOperator;
    /// Synchronous observation point used only by the NRS propagation gtest.
    InnerNrsObserverForTests innerNrsObserverForTests_;
};
}
}


#endif // __HASH_BUILDER_EXPR_H__
