/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2021. All rights reserved.
 */

#ifndef OMNI_RUNTIME_TOPN_EXPR_H
#define OMNI_RUNTIME_TOPN_EXPR_H

#include "operator/projection/projection.h"
#include "operator/topn/topn.h"

namespace omniruntime::op {
class TopNWithExprOperatorFactory : public OperatorFactory {
public:
    TopNWithExprOperatorFactory(const type::DataTypes &sourceDataTypes, int32_t limit, int32_t offset,
        const std::vector<omniruntime::expressions::Expr *> &sortKeys, int32_t *sortAscendings, int32_t *sortNullFirsts,
        int32_t sortKeyCount, OverflowConfig *overflowConfig);

    TopNWithExprOperatorFactory(const type::DataTypes &sourceDataTypes, int32_t limit, int32_t offset,
        const std::vector<omniruntime::expressions::Expr *> &sortKeys, int32_t *sortAscendings, int32_t *sortNullFirsts,
        int32_t sortKeyCount, OverflowConfig *overflowConfig, const config::QueryConfig &queryConfig);

    ~TopNWithExprOperatorFactory() override;

    static TopNWithExprOperatorFactory *CreateTopNWithExprOperatorFactory(
        std::shared_ptr<const TopNNode> planNode, const config::QueryConfig &queryConfig);

    using OperatorFactory::CreateOperator;
    // Pool-bound override
    Operator *CreateOperator(omniruntime::mem::OmniTaskMemoryPool *pool) override;

private:
    std::unique_ptr<DataTypes> sourceTypes;
    std::vector<int32_t> sortCols;
    std::vector<std::unique_ptr<Projection>> projections;
    std::unique_ptr<TopNOperatorFactory> topNOperatorFactory;
};

class TopNWithExprOperator : public Operator {
public:
    // Pool-bound ctor chains Operator(pool)
    TopNWithExprOperator(omniruntime::mem::OmniTaskMemoryPool *pool, const type::DataTypes &sourceTypes,
        std::vector<std::unique_ptr<Projection>> &projections, TopNOperator *topNOperator,
        const config::QueryConfig &queryConfig);

    ~TopNWithExprOperator() override;

    int32_t AddInput(omniruntime::vec::VectorBatch *inputVecBatch) override;

    int32_t GetOutput(omniruntime::vec::VectorBatch **outputVecBatch) override;

    OmniStatus Close() override;

    void setNoMoreInput(bool noMoreInput) override
    {
        noMoreInput_ = noMoreInput;
        topNOperator->setNoMoreInput(noMoreInput);
    }

    void noMoreInput() override
    {
        noMoreInput_ = true;
        topNOperator->noMoreInput();
    }

private:
    omniruntime::type::DataTypes sourceTypes;
    std::vector<std::unique_ptr<Projection>> &projections;
    TopNOperator *topNOperator;
};
}
#endif // OMNI_RUNTIME_TOPN_EXPR_H
