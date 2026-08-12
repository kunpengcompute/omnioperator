/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
 * @Description: lookup join implementations
 */

#ifndef __LOOKUP_JOIN_EXPR_H__
#define __LOOKUP_JOIN_EXPR_H__

#include "common_join.h"
#include "operator/operator_factory.h"
#include "operator/projection/projection.h"
#include "operator/join/lookup_join.h"
#include "operator/join/hash_builder_expr.h"
#include "type/data_types.h"
#include "operator/status.h"

namespace omniruntime {
namespace op {
class LookupJoinWithExprOperatorFactory : public OperatorFactory {
public:
    static LookupJoinWithExprOperatorFactory *CreateLookupJoinWithExprOperatorFactory(const DataTypes &probeTypes,
        int32_t *probeOutputCols, int32_t probeOutputColsCount,
        const std::vector<omniruntime::expressions::Expr *> &probeHashKeys, int32_t probeHashKeysCount,
        int32_t *buildOutputCols, int32_t buildOutputColsCount, const DataTypes &buildOutputTypes,
        int64_t hashBuilderFactoryAddr, omniruntime::expressions::Expr *filterExpr,
        bool isShuffleExchangeBuildPlan, OverflowConfig *overflowConfig);

    static LookupJoinWithExprOperatorFactory *CreateLookupJoinWithExprOperatorFactory(std::shared_ptr<const HashJoinNode> planNode,
        HashBuilderOperatorFactory* hashBuilderOperatorFactory, const config::QueryConfig &queryConfig);

    LookupJoinWithExprOperatorFactory(const DataTypes &probeTypes, int32_t *probeOutputCols,
        int32_t probeOutputColsCount, const std::vector<omniruntime::expressions::Expr *> &probeHashKeys,
        int32_t probeHashKeysCount, int32_t *buildOutputCols, int32_t buildOutputColsCount,
        const DataTypes &buildOutputTypes, int64_t hashBuilderFactoryAddr,
        Expr *filterExpr, bool isShuffleExchangeBuildPlan, OverflowConfig *overflowConfig, int32_t *outputList = nullptr,
        bool enableInt32KeyRewrite = false);

    LookupJoinWithExprOperatorFactory(const DataTypes &probeTypes, int32_t *probeOutputCols,
        int32_t probeOutputColsCount, const std::vector<omniruntime::expressions::Expr *> &probeHashKeys,
        int32_t probeHashKeysCount, int32_t *buildOutputCols, int32_t buildOutputColsCount,
        const DataTypes &buildOutputTypes, int64_t hashBuilderFactoryAddr, Expr *filterExpr,
        bool isShuffleExchangeBuildPlan, OverflowConfig *overflowConfig, const config::QueryConfig &queryConfig,
        int32_t *outputList = nullptr, bool enableInt32KeyRewrite = false);

    ~LookupJoinWithExprOperatorFactory() override;

    omniruntime::op::Operator *CreateOperator() override;

private:
    std::unique_ptr<DataTypes> probeTypes; // all types for probe
    std::vector<int32_t> probeHashCols;    // join columns for probe
    std::vector<std::unique_ptr<omniruntime::expressions::Expr>> rewrittenProbeHashKeyOwners;
    std::vector<omniruntime::expressions::Expr *> effectiveProbeHashKeys;
    std::vector<std::unique_ptr<Projection>> projections;
    std::vector<ProjFunc> projectFuncs;
    LookupJoinOperatorFactory *operatorFactory;
};

class LookupJoinWithExprOperator : public Operator {
public:
    LookupJoinWithExprOperator(const type::DataTypes &probeTypes, std::vector<std::unique_ptr<Projection>> &projections,
        LookupJoinOperator *lookupJoinOperator, const config::QueryConfig &queryConfig);

    ~LookupJoinWithExprOperator() override;

    int32_t AddInput(omniruntime::vec::VectorBatch *vecBatch) override;

    int32_t GetOutput(omniruntime::vec::VectorBatch **outputVecBatch) override;

    OmniStatus Close() override;

    BlockingReason IsBlocked(ContinueFuture* future) override;

    void noMoreInput() override
    {
        noMoreInput_ = true;
        lookupJoinOperator->noMoreInput();
    }

    void setNoMoreInput(bool noMoreInput) override
    {
        noMoreInput_ = noMoreInput;
        lookupJoinOperator->setNoMoreInput(noMoreInput);
    }

private:
    DataTypes probeTypes;
    std::vector<std::unique_ptr<Projection>> &projections;
    LookupJoinOperator *lookupJoinOperator;
};
}
}


#endif // __LOOKUP_JOIN_EXPR_H__
