/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 */

#ifndef OMNI_RUNTIME_TOPN_SORT_EXPR_H
#define OMNI_RUNTIME_TOPN_SORT_EXPR_H

#include "operator/topnsort/topn_sort.h"
#include "operator/projection/projection.h"

namespace omniruntime::op {
class TopNSortWithExprOperatorFactory : public OperatorFactory {
public:
    TopNSortWithExprOperatorFactory(const type::DataTypes &sourceDataTypes, int32_t n, bool isStrictTopN,
        const std::vector<omniruntime::expressions::Expr *> &partitionKeys,
        const std::vector<omniruntime::expressions::Expr *> &sortKeys, std::vector<int32_t> &sortAscendings,
        std::vector<int32_t> &sortNullFirsts, OverflowConfig *overflowConfig);

    TopNSortWithExprOperatorFactory(const type::DataTypes &sourceDataTypes, int32_t n, bool isStrictTopN,
        const std::vector<omniruntime::expressions::Expr *> &partitionKeys,
        const std::vector<omniruntime::expressions::Expr *> &sortKeys, std::vector<int32_t> &sortAscendings,
        std::vector<int32_t> &sortNullFirsts, OverflowConfig *overflowConfig, const config::QueryConfig &queryConfig);

    static TopNSortWithExprOperatorFactory* CreateTopNSortWithExprOperatorFactory(
        std::shared_ptr<const TopNSortNode> planNode, const config::QueryConfig &queryConfig);

    ~TopNSortWithExprOperatorFactory() override;

    using OperatorFactory::CreateOperator;
    // Pool-bound override
    Operator *CreateOperator(omniruntime::mem::OmniTaskMemoryPool *pool) override;

private:
    std::unique_ptr<DataTypes> sourceTypes;
    std::vector<int32_t> partitionCols;
    std::vector<int32_t> sortCols;
    std::vector<std::unique_ptr<Projection>> projections;
    std::unique_ptr<TopNSortOperatorFactory> topNSortOperatorFactory;
};

class TopNSortWithExprOperator : public Operator {
public:
    // Pool-bound ctor chains Operator(pool)
    TopNSortWithExprOperator(omniruntime::mem::OmniTaskMemoryPool *pool, const type::DataTypes &sourceTypes,
        std::vector<int32_t> &partitionCols, std::vector<int32_t> &sortCols,
        std::vector<std::unique_ptr<Projection>> &projections, TopNSortOperator *topNSortOperator,
        const config::QueryConfig &queryConfig);

    ~TopNSortWithExprOperator() override;

    int32_t AddInput(omniruntime::vec::VectorBatch *inputVecBatch) override;

    int32_t GetOutput(omniruntime::vec::VectorBatch **outputVecBatch) override;

    OmniStatus Close() override;

    void setNoMoreInput(bool noMoreInput) override
    {
        noMoreInput_ = noMoreInput;
        topNSortOperator->setNoMoreInput(noMoreInput);
    }

    void noMoreInput() override
    {
        noMoreInput_ = true;
        topNSortOperator->noMoreInput();
    }

private:
    omniruntime::type::DataTypes sourceTypes;
    std::vector<int32_t> partitionCols;
    std::vector<int32_t> sortCols;
    std::vector<std::unique_ptr<Projection>> &projections;
    TopNSortOperator *topNSortOperator;
};
}
#endif // OMNI_RUNTIME_TOPN_SORT_EXPR_H
