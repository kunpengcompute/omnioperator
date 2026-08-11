/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2021-2023. All rights reserved.
 * @Description: sort implementations
 */

#ifndef __HASH_BUILDER_EXPR_H__
#define __HASH_BUILDER_EXPR_H__

#include <cstdint>
#include "operator/operator_factory.h"
#include "type/data_types.h"
#include "operator/projection/projection.h"
#include "operator/join/hash_builder.h"

namespace omniruntime {
namespace op {
class HashBuilderWithExprOperatorFactory : public OperatorFactory {
public:
    static HashBuilderWithExprOperatorFactory *CreateHashBuilderWithExprOperatorFactory(JoinType joinType,
        const type::DataTypes &buildTypes, const std::vector<omniruntime::expressions::Expr *> &buildHashKeys,
        int32_t hashTableCount, OverflowConfig *overflowConfig);

    static HashBuilderWithExprOperatorFactory *CreateHashBuilderWithExprOperatorFactory(JoinType joinType,
        BuildSide buildSide, const type::DataTypes &buildTypes,
        const std::vector<omniruntime::expressions::Expr *> &buildHashKeys, int32_t hashTableCount,
        OverflowConfig *overflowConfig);

    static HashBuilderWithExprOperatorFactory *CreateHashBuilderWithExprOperatorFactory(
        std::shared_ptr<const HashJoinNode> planNode, const config::QueryConfig &queryConfig);

    HashBuilderWithExprOperatorFactory(JoinType joinType, const DataTypes &buildTypes,
        const std::vector<omniruntime::expressions::Expr *> &buildHashKeys, int32_t hashTableCount,
        OverflowConfig *overflowConfig, bool enableInt32KeyRewrite = false);

    HashBuilderWithExprOperatorFactory(JoinType joinType, BuildSide buildSide, const DataTypes &buildTypes,
        const std::vector<omniruntime::expressions::Expr *> &buildHashKeys, int32_t hashTableCount,
        OverflowConfig *overflowConfig, bool enableInt32KeyRewrite = false);

    HashBuilderWithExprOperatorFactory::HashBuilderWithExprOperatorFactory(JoinType joinType, BuildSide buildSide,
        const type::DataTypes &buildTypes, const std::vector<omniruntime::expressions::Expr *> &buildHashKeys,
        int32_t hashTableCount, OverflowConfig *overflowConfig, const config::QueryConfig &queryConfig,
        bool enableInt32KeyRewrite = false);

    ~HashBuilderWithExprOperatorFactory() override;

    omniruntime::op::Operator *CreateOperator() override;

    HashBuilderOperatorFactory *GetHashBuilderOperatorFactory()
    {
        return operatorFactory;
    }

private:
    std::unique_ptr<DataTypes> buildTypes;
    std::vector<int32_t> buildHashCols;
    std::vector<std::unique_ptr<omniruntime::expressions::Expr>> rewrittenBuildHashKeyOwners;
    std::vector<omniruntime::expressions::Expr *> effectiveBuildHashKeys;
    std::vector<std::unique_ptr<Projection>> projections;
    HashBuilderOperatorFactory *operatorFactory;
};

class HashBuilderWithExprOperator : public Operator {
public:
    HashBuilderWithExprOperator(const DataTypes &buildTypes, std::vector<std::unique_ptr<Projection>> &projections,
        HashBuilderOperator *hashBuilderOperator, const config::QueryConfig &queryConfig);

    ~HashBuilderWithExprOperator() override;

    int32_t AddInput(omniruntime::vec::VectorBatch *vecBatch) override;

    int32_t GetOutput(omniruntime::vec::VectorBatch **outputVecBatch) override;

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
    const DataTypes buildTypes;
    std::vector<std::unique_ptr<Projection>> &projections;
    HashBuilderOperator *hashBuilderOperator;
};
}
}


#endif // __HASH_BUILDER_EXPR_H__
