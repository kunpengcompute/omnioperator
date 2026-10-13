/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 * @Description: window group limit operator implementations
 */

#ifndef OMNI_RUNTIME_WINDOW_GROUP_LIMIT_EXPR_H
#define OMNI_RUNTIME_WINDOW_GROUP_LIMIT_EXPR_H

#include "operator/window/window_group_limit.h"
#include "operator/projection/projection.h"

namespace omniruntime::op {
class WindowGroupLimitWithExprOperatorFactory : public OperatorFactory {
public:
    WindowGroupLimitWithExprOperatorFactory(const type::DataTypes &sourceDataTypes, int32_t n,
        const std::string funcName, const std::vector<omniruntime::expressions::Expr *> &partitionKeys,
        const std::vector<omniruntime::expressions::Expr *> &sortKeys, std::vector<int32_t> &sortAscendings,
        std::vector<int32_t> &sortNullFirsts, OverflowConfig *overflowConfig);

    WindowGroupLimitWithExprOperatorFactory(const type::DataTypes &sourceDataTypes, int32_t n,
        const std::string funcName, const std::vector<omniruntime::expressions::Expr *> &partitionKeys,
        const std::vector<omniruntime::expressions::Expr *> &sortKeys, std::vector<int32_t> &sortAscendings,
        std::vector<int32_t> &sortNullFirsts, OverflowConfig *overflowConfig, const config::QueryConfig &queryConfig);

    ~WindowGroupLimitWithExprOperatorFactory() override;

    static WindowGroupLimitWithExprOperatorFactory *WindowGroupLimitWithExprOperatorFactory::CreateWindowGroupLimitWithExprOperatorFactory(
        std::shared_ptr<const WindowGroupLimitNode> planNode, const config::QueryConfig &queryConfig);

    using OperatorFactory::CreateOperator;
    // Pool-bound override
    Operator *CreateOperator(omniruntime::mem::OmniTaskMemoryPool *pool) override;

private:
    std::unique_ptr<DataTypes> sourceTypes;
    std::vector<int32_t> partitionCols;
    std::vector<int32_t> sortCols;
    std::vector<std::unique_ptr<Projection>> projections;
    std::unique_ptr<WindowGroupLimitOperatorFactory> windowGroupLimitOperatorFactory;
};

class WindowGroupLimitWithExprOperator : public Operator {
public:
    // Pool-bound ctor chains Operator(pool)
    WindowGroupLimitWithExprOperator(omniruntime::mem::OmniTaskMemoryPool *pool,
        const type::DataTypes &sourceTypes, std::vector<int32_t> &partitionCols, std::vector<int32_t> &sortCols,
        std::vector<std::unique_ptr<Projection>> &projections,
        WindowGroupLimitOperator *windowGroupLimitOperator, const config::QueryConfig &queryConfig);

    ~WindowGroupLimitWithExprOperator() override;

    int32_t AddInput(omniruntime::vec::VectorBatch *inputVecBatch) override;

    int32_t GetOutput(omniruntime::vec::VectorBatch **outputVecBatch) override;

    OmniStatus Close() override;

    void noMoreInput() override
    {
        noMoreInput_ = true;
        windowGroupLimitOperator->noMoreInput();
    }

    void setNoMoreInput(bool noMoreInput) override
    {
        noMoreInput_ = noMoreInput;
        windowGroupLimitOperator->setNoMoreInput(noMoreInput);
    }


private:
    omniruntime::type::DataTypes sourceTypes;
    std::vector<int32_t> partitionCols;
    std::vector<int32_t> sortCols;
    std::vector<std::unique_ptr<Projection>> &projections;
    WindowGroupLimitOperator *windowGroupLimitOperator;
};
}
#endif // OMNI_RUNTIME_WINDOW_GROUP_LIMIT_EXPR_H
