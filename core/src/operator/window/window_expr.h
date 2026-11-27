/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2021-2024. All rights reserved.
 * @Description: window implementations
 */
#ifndef __WINDOW_EXPR_H__
#define __WINDOW_EXPR_H__

#include <vector>
#include "window.h"

namespace omniruntime {
namespace op {
class WindowWithExprOperatorFactory : public OperatorFactory {
public:
    WindowWithExprOperatorFactory(const type::DataTypes &sourceTypes, int32_t *outputCols, int32_t outputColsCount,
        int32_t *windowFunctionTypes, int32_t windowFunctionCount, int32_t *partitionCols, int32_t partitionCount,
        int32_t *preGroupedCols, int32_t preGroupedCount, int32_t *sortCols, int32_t *sortAscendings,
        int32_t *sortNullFirsts, int32_t sortColCount, int32_t preSortedChannelPrefix, int32_t expectedPositions,
        const type::DataTypes &outputDataTypes, const std::vector<omniruntime::expressions::Expr *> &argumentKeys,
        int32_t argumentChannelsCount, int32_t *windowFrameTypesField, int32_t *windowFrameStartTypesField,
        int32_t *windowFrameStartChannelsField, int32_t *windowFrameEndTypesField, int32_t *windowFrameEndChannelsField,
        const OperatorConfig &operatorConfig, WindowFunctionOptions *windowFunctionOptionsField = nullptr);

    WindowWithExprOperatorFactory(const type::DataTypes &sourceTypes, int32_t *outputCols, int32_t outputColsCount,	
        int32_t *windowFunctionTypes, int32_t windowFunctionCount, int32_t *partitionCols, int32_t partitionCount,	
        int32_t *preGroupedCols, int32_t preGroupedCount, int32_t *sortCols, int32_t *sortAscendings,	
        int32_t *sortNullFirsts, int32_t sortColCount, int32_t preSortedChannelPrefix, int32_t expectedPositions,	
        const type::DataTypes &outputDataTypes, const std::vector<omniruntime::expressions::Expr *> &argumentKeys,	
        int32_t argumentChannelsCount, int32_t *windowFrameTypesField, int32_t *windowFrameStartTypesField,	
        int32_t *windowFrameStartChannelsField, int32_t *windowFrameEndTypesField, int32_t *windowFrameEndChannelsField,	
        const OperatorConfig &operatorConfig, const config::QueryConfig &queryConfig,	
        WindowFunctionOptions *windowFunctionOptionsField = nullptr);

    ~WindowWithExprOperatorFactory() override;

    static WindowWithExprOperatorFactory *CreateWindowWithExprOperatorFactory(const type::DataTypes &sourceTypes,
        int32_t *outputCols, int32_t outputColsCount, int32_t *windowFunctionTypes, int32_t windowFunctionCount,
        int32_t *partitionCols, int32_t partitionCount, int32_t *preGroupedCols, int32_t preGroupedCount,
        int32_t *sortCols, int32_t *sortAscendings, int32_t *sortNullFirsts, int32_t sortColCount,
        int32_t preSortedChannelPrefix, int32_t expectedPositions, const type::DataTypes &outputDataTypes,
        const std::vector<omniruntime::expressions::Expr *> &argumentKeys, int32_t argumentChannelsCount,
        int32_t *windowFrameTypesField, int32_t *windowFrameStartTypesField, int32_t *windowFrameStartChannelsField,
        int32_t *windowFrameEndTypesField, int32_t *windowFrameEndChannelsField,
        WindowFunctionOptions *windowFunctionOptionsField = nullptr);

    static WindowWithExprOperatorFactory *CreateWindowWithExprOperatorFactory(const type::DataTypes &sourceTypes,
        int32_t *outputCols, int32_t outputColsCount, int32_t *windowFunctionTypes, int32_t windowFunctionCount,
        int32_t *partitionCols, int32_t partitionCount, int32_t *preGroupedCols, int32_t preGroupedCount,
        int32_t *sortCols, int32_t *sortAscendings, int32_t *sortNullFirsts, int32_t sortColCount,
        int32_t preSortedChannelPrefix, int32_t expectedPositions, const type::DataTypes &outputDataTypes,
        const std::vector<omniruntime::expressions::Expr *> &argumentKeys, int32_t argumentChannelsCount,
        int32_t *windowFrameTypesField, int32_t *windowFrameStartTypesField, int32_t *windowFrameStartChannelsField,
        int32_t *windowFrameEndTypesField, int32_t *windowFrameEndChannelsField, const OperatorConfig &operatorConfig,
        WindowFunctionOptions *windowFunctionOptionsField = nullptr);

    static WindowWithExprOperatorFactory *CreateWindowWithExprOperatorFactory(std::shared_ptr<const WindowNode> planNode,
        const config::QueryConfig &queryConfig);

    using OperatorFactory::CreateOperator;
    // Pool-bound override
    Operator *CreateOperator(omniruntime::mem::OmniTaskMemoryPool *pool) override;

private:
    std::unique_ptr<DataTypes> sourceTypes;
    std::vector<int32_t> argumentChannels;
    std::vector<std::unique_ptr<Projection>> projections;
    WindowOperatorFactory *operatorFactory;
};

class WindowWithExprOperator : public Operator {
public:
    // Pool-bound ctor chains Operator(pool)
    WindowWithExprOperator(omniruntime::mem::OmniTaskMemoryPool *pool, const type::DataTypes &sourceTypes,
        std::vector<std::unique_ptr<Projection>> &projections, WindowOperator *windowOperator,
        const config::QueryConfig &queryConfig);

    ~WindowWithExprOperator() override;

    int32_t AddInput(omniruntime::vec::VectorBatch *vecBatch) override;

    int32_t GetOutput(omniruntime::vec::VectorBatch **outputVecBatch) override;

    OmniStatus Close() override;

    uint64_t GetSpilledBytes() override;

    void noMoreInput() override
    {
        noMoreInput_ = true;
        Operator::NonReclaimableSection innerSection(windowOperator);
        windowOperator->noMoreInput();
    }

    void setNoMoreInput(bool noMoreInput) override
    {
        noMoreInput_ = noMoreInput;
        Operator::NonReclaimableSection innerSection(windowOperator);
        windowOperator->setNoMoreInput(noMoreInput);
    }

private:
    type::DataTypes sourceTypes;
    std::vector<std::unique_ptr<Projection>> &projections;
    WindowOperator *windowOperator;
};
}
}

#endif