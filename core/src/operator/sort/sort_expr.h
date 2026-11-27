/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2021-2024. All rights reserved.
 * @Description: sort implementations
 */

#ifndef __SORT_EXPR_H__
#define __SORT_EXPR_H__

#include <memory>
#include "operator/operator_factory.h"
#include "operator/projection/projection.h"
#include "operator/sort/sort.h"
#include "type/data_types.h"

namespace omniruntime {
namespace op {
class SortWithExprOperatorFactory : public OperatorFactory {
public:
    static SortWithExprOperatorFactory *CreateSortWithExprOperatorFactory(const type::DataTypes &sourceTypes,
        int32_t *outputCols, int32_t outputColsCount, const std::vector<omniruntime::expressions::Expr *> &sortKeys,
        int32_t *sortAscendings, int32_t *sortNullFirsts, int32_t sortKeysCount, const OperatorConfig &operatorConfig);
    static SortWithExprOperatorFactory* CreateSortWithExprOperatorFactory(
        std::shared_ptr<const OrderByNode> planNode, const config::QueryConfig& queryConfig);

    static SortWithExprOperatorFactory *CreateSortWithExprOperatorFactory(const type::DataTypes &sourceTypes,
        int32_t *outputCols, int32_t outputColsCount, const std::vector<omniruntime::expressions::Expr *> &sortKeys,
        int32_t *sortAscendings, int32_t *sortNullFirsts, int32_t sortKeysCount);

    SortWithExprOperatorFactory(const type::DataTypes &sourceTypes, int32_t *outputCols, int32_t outputColsCount,
        const std::vector<omniruntime::expressions::Expr *> &sortKeys, int32_t *sortAscendings, int32_t *sortNullFirsts,
        int32_t sortKeysCount, const OperatorConfig &operatorConfig);
    
    SortWithExprOperatorFactory(const type::DataTypes &sourceTypes, int32_t *outputCols, int32_t outputColsCount,	
        const std::vector<omniruntime::expressions::Expr *> &sortKeys, int32_t *sortAscendings, int32_t *sortNullFirsts,	
        int32_t sortKeysCount, const OperatorConfig &operatorConfig, const config::QueryConfig& queryConfig);

    ~SortWithExprOperatorFactory() override;

    using OperatorFactory::CreateOperator;
    // Pool-bound override
    Operator *CreateOperator(omniruntime::mem::OmniTaskMemoryPool *pool) override;

private:
    std::unique_ptr<DataTypes> sourceTypes;
    std::vector<int32_t> sortCols;
    std::vector<std::unique_ptr<Projection>> projections;
    SortOperatorFactory *sortOperatorFactory;
};

class SortWithExprOperator : public Operator {
public:
    // Pool-bound ctor chains Operator(pool)
    SortWithExprOperator(omniruntime::mem::OmniTaskMemoryPool *pool, const type::DataTypes &sourceTypes,
        std::vector<std::unique_ptr<Projection>> &projections, SortOperator *sortOperator,
        const config::QueryConfig& queryConfig);

    ~SortWithExprOperator() override;

    OmniStatus Init() override;

    int32_t AddInput(omniruntime::vec::VectorBatch *inputVecBatch) override;

    int32_t GetOutput(omniruntime::vec::VectorBatch **outputVecBatch) override;

    OmniStatus Close() override;

    uint64_t GetSpilledBytes() override;

    void setNoMoreInput(bool noMoreInput) override
    {
        Operator::NonReclaimableSection innerSection(sortOperator);
        noMoreInput_ = noMoreInput;
        sortOperator->setNoMoreInput(noMoreInput);
    }

    void noMoreInput() override
    {
        Operator::NonReclaimableSection innerSection(sortOperator);
        noMoreInput_ = true;
        sortOperator->noMoreInput();
    }

private:
    DataTypes sourceTypes;
    std::vector<std::unique_ptr<Projection>> &projections;
    SortOperator *sortOperator;
};
}
}
#endif // __SORT_EXPR_H__
