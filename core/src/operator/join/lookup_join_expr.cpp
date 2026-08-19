/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
 * @Description: lookup join implementations
 */

#include "lookup_join_expr.h"

#include <utility>
#include "hash_builder_expr.h"
#ifdef OMNI_SVEHT32_JOIN_KEY_REWRITE
#include "operator/join/join_key_rewrite_utils.h"
#endif
#include "operator/util/operator_util.h"
#include "vector/vector_helper.h"

namespace omniruntime {
namespace op {
using namespace omniruntime::vec;

LookupJoinWithExprOperatorFactory *LookupJoinWithExprOperatorFactory::CreateLookupJoinWithExprOperatorFactory(
    const DataTypes &probeTypes, int32_t *probeOutputCols, int32_t probeOutputColsCount,
    const std::vector<omniruntime::expressions::Expr *> &probeHashKeys, int32_t probeHashKeysCount,
    int32_t *buildOutputCols, int32_t buildOutputColsCount, const DataTypes &buildOutputTypes,
    int64_t hashBuilderFactoryAddr, omniruntime::expressions::Expr *filterExpr,
    bool isShuffleExchangeBuildPlan, OverflowConfig *overflowConfig)
{
    auto operatorFactory = new LookupJoinWithExprOperatorFactory(probeTypes, probeOutputCols, probeOutputColsCount,
        probeHashKeys, probeHashKeysCount, buildOutputCols, buildOutputColsCount, buildOutputTypes,
        hashBuilderFactoryAddr, filterExpr, isShuffleExchangeBuildPlan, overflowConfig);
    return operatorFactory;
}

void setOutputColsExpr(std::shared_ptr<const HashJoinNode> planNode,
                       std::vector<int32_t> &buildOutputCols,
                       std::vector<int32_t> &probeOutputCols,
                       int32_t probeOutputColsCount, int32_t buildOutputColsCount)
{
    auto partitionKeys = planNode->PartitionKeys();
    if (planNode->IsExistence()) {
        buildOutputCols.emplace_back(1);
    }
    if (planNode->IsBuildRight() && !partitionKeys.empty()) {
        for (auto &partitionKey : partitionKeys) {
            auto x = dynamic_cast<FieldExpr *>(partitionKey);
            if (x->colVal >= probeOutputColsCount) {
                buildOutputCols.emplace_back(x->colVal - probeOutputColsCount);
            } else {
                probeOutputCols.emplace_back(x->colVal);
            }
        }
    } else if (!partitionKeys.empty()) {
        for (auto &partitionKey : partitionKeys) {
            auto x = dynamic_cast<FieldExpr *>(partitionKey);
            if (x->colVal < buildOutputColsCount) {
                buildOutputCols.emplace_back(x->colVal);
            } else {
                probeOutputCols.emplace_back(x->colVal - buildOutputColsCount);
            }
        }
    } else {
        for (size_t index = 0; index < buildOutputColsCount; index++) {
            buildOutputCols.emplace_back(index);
        }
        for (size_t index = 0; index < probeOutputColsCount; index++) {
            probeOutputCols.emplace_back(index);
        }
    }
}

void getOutputList(std::shared_ptr<const HashJoinNode> planNode,
                   std::vector<int32_t> &buildOutputCols,
                   std::vector<int32_t> &probeOutputCols,
                   std::vector<int32_t> &outputList)
{
    auto keys = planNode->PartitionKeys();
    auto probeIndex = 0;
    int frontSize;
    int lastSize;
    if (planNode->IsBuildRight()) {
        frontSize = planNode->LeftOutputType()->GetSize();
        lastSize = planNode->RightOutputType()->GetSize();
    } else {
        frontSize = planNode->RightOutputType()->GetSize();
        lastSize = planNode->LeftOutputType()->GetSize();
    }
    auto buildIndex = probeOutputCols.size();
    for (auto key : keys) {
        for (int i = 0; i < frontSize; i++) {
            if (dynamic_cast<FieldExpr *>(key)->colVal == i) {
                outputList.push_back(probeIndex);
                probeIndex++;
                break;
            }
        }
        for (int i = 0; i < lastSize; i++) {
            if (dynamic_cast<FieldExpr *>(key)->colVal == i + frontSize) {
                outputList.push_back(buildIndex);
                buildIndex++;
                break;
            }
        }
    }
}

LookupJoinWithExprOperatorFactory *LookupJoinWithExprOperatorFactory::CreateLookupJoinWithExprOperatorFactory(
    std::shared_ptr<const HashJoinNode> planNode, HashBuilderOperatorFactory* hashBuilderOperatorFactory,
    const config::QueryConfig &queryConfig)
{
    auto buildTypes = planNode->RightOutputType();
    auto buildOutputColsCount = buildTypes->GetSize();
    if (planNode->IsLeftSemi() || planNode->IsLeftAnti()) {
        buildOutputColsCount = 0;
    } else if (planNode->IsExistence()) {
        buildOutputColsCount = 1;
        buildTypes = std::make_shared<DataTypes>(DataTypes({BooleanType()}));
    }

    std::vector<int32_t> buildOutputCols;
    auto probeTypes = planNode->LeftOutputType();
    auto probeOutputColsCount = probeTypes->GetSize();
    std::vector<int32_t> probeOutputCols;

    setOutputColsExpr(planNode, buildOutputCols, probeOutputCols, probeOutputColsCount, buildOutputColsCount);

    std::vector<int32_t> outputList;

    if (planNode->IsBuildRight()) {
        getOutputList(planNode, buildOutputCols, probeOutputCols, outputList);
    } else {
        getOutputList(planNode, probeOutputCols, buildOutputCols, outputList);
    }

    if (!planNode->IsExistence()) {
        std::vector<std::shared_ptr<DataType>> outType;
        for (auto col:buildOutputCols) {
            outType.push_back(buildTypes->GetType(col));
        }
        buildTypes = std::make_shared<DataTypes>(outType);
    }

    auto probeHashCols = planNode->LeftKeys();
    auto probeHashColsCount = (int32_t) probeHashCols.size();
#ifdef OMNI_SVEHT32_JOIN_KEY_REWRITE
    const bool enableInt32KeyRewrite = ShouldRewriteJoinKeysToInt32(probeHashCols, planNode->RightKeys());
#else
    const bool enableInt32KeyRewrite = false;
#endif

    auto filter = planNode->Filter();
    auto isShuffle = planNode->IsShuffle();

    auto overflowConfig = queryConfig.IsOverFlowASNull()
                          ? new OverflowConfig(OVERFLOW_CONFIG_NULL)
                          : new OverflowConfig(OVERFLOW_CONFIG_EXCEPTION);

    auto pLookupJoinWithExprOperatorFactory = new LookupJoinWithExprOperatorFactory(*probeTypes, probeOutputCols.data(), probeOutputCols.size(),
        probeHashCols, probeHashColsCount, buildOutputCols.data(), buildOutputCols.size(), *buildTypes,
    reinterpret_cast<int64_t>(hashBuilderOperatorFactory), filter, isShuffle, overflowConfig, queryConfig, outputList.data(),
    enableInt32KeyRewrite);

    delete overflowConfig;
    overflowConfig = nullptr;
    return pLookupJoinWithExprOperatorFactory;
}

LookupJoinWithExprOperatorFactory::LookupJoinWithExprOperatorFactory(const DataTypes &probeTypes,
    int32_t *probeOutputCols, int32_t probeOutputColsCount,
    const std::vector<omniruntime::expressions::Expr *> &probeHashKeys, int32_t probeHashKeysCount,
    int32_t *buildOutputCols, int32_t buildOutputColsCount, const DataTypes &buildOutputTypes,
    int64_t hashBuilderFactoryAddr, Expr *filterExpr, bool isShuffleExchangeBuildPlan, OverflowConfig *overflowConfig,
    int32_t *outputList, bool enableInt32KeyRewrite)
{
    std::vector<DataTypePtr> newProbeTypes;
#ifdef OMNI_SVEHT32_JOIN_KEY_REWRITE
    if (enableInt32KeyRewrite) {
        effectiveProbeHashKeys = RewriteJoinKeyExprsToInt32(probeHashKeys, rewrittenProbeHashKeyOwners);
    } else {
        effectiveProbeHashKeys.assign(probeHashKeys.begin(), probeHashKeys.end());
    }
#else
    (void)enableInt32KeyRewrite;
    effectiveProbeHashKeys.assign(probeHashKeys.begin(), probeHashKeys.end());
#endif
    OperatorUtil::CreateProjections(probeTypes, effectiveProbeHashKeys, newProbeTypes, this->projections,
        this->probeHashCols, overflowConfig);
    this->probeTypes = std::make_unique<DataTypes>(newProbeTypes);
    auto hashBuilderOperatorFactory =
        reinterpret_cast<HashBuilderOperatorFactory *>(hashBuilderFactoryAddr);
    this->operatorFactory =
        LookupJoinOperatorFactory::CreateLookupJoinOperatorFactory(*(this->probeTypes), probeOutputCols,
        probeOutputColsCount, this->probeHashCols.data(), probeHashKeysCount, buildOutputCols, buildOutputColsCount,
        std::move(buildOutputTypes), (int64_t)(hashBuilderOperatorFactory),
        filterExpr, probeTypes.GetSize(), isShuffleExchangeBuildPlan, overflowConfig, outputList);
}

LookupJoinWithExprOperatorFactory::LookupJoinWithExprOperatorFactory(const DataTypes &probeTypes,
    int32_t *probeOutputCols, int32_t probeOutputColsCount,
    const std::vector<omniruntime::expressions::Expr *> &probeHashKeys, int32_t probeHashKeysCount,
    int32_t *buildOutputCols, int32_t buildOutputColsCount, const DataTypes &buildOutputTypes,
    int64_t hashBuilderFactoryAddr, Expr *filterExpr, bool isShuffleExchangeBuildPlan, OverflowConfig *overflowConfig,
    const config::QueryConfig &queryConfig, int32_t *outputList, bool enableInt32KeyRewrite)
    : LookupJoinWithExprOperatorFactory(probeTypes, probeOutputCols, probeOutputColsCount, probeHashKeys, probeHashKeysCount,
    buildOutputCols, buildOutputColsCount, buildOutputTypes, hashBuilderFactoryAddr, filterExpr, isShuffleExchangeBuildPlan,
    overflowConfig, outputList, enableInt32KeyRewrite)
{
    this->queryConfig_ = queryConfig;
}

LookupJoinWithExprOperatorFactory::~LookupJoinWithExprOperatorFactory()
{
    delete this->operatorFactory;
}

Operator *LookupJoinWithExprOperatorFactory::CreateOperator()
{
    auto lookupJoinOperator = static_cast<LookupJoinOperator *>(operatorFactory->CreateOperator());
    return new LookupJoinWithExprOperator(*probeTypes, projections, lookupJoinOperator, queryConfig_);
}

LookupJoinWithExprOperator::LookupJoinWithExprOperator(const type::DataTypes &probeTypes,
    std::vector<std::unique_ptr<Projection>> &projections, LookupJoinOperator *lookupJoinOperator,
    const config::QueryConfig &queryConfig)
    : probeTypes(probeTypes),
      projections(projections),
      lookupJoinOperator(lookupJoinOperator)
{
    SetOperatorName(opNameForLookUpJoin);
    executionContext->SetConfig(queryConfig);
}

LookupJoinWithExprOperator::~LookupJoinWithExprOperator()
{
    delete lookupJoinOperator;
}

int32_t LookupJoinWithExprOperator::AddInput(VectorBatch *vecBatch)
{
    if (vecBatch->GetRowCount() <= 0) {
        VectorHelper::FreeVecBatch(vecBatch);
        ResetInputVecBatch();
        return 0;
    }
    auto *newInputVecBatch = OperatorUtil::ProjectVectors(vecBatch, probeTypes, projections, executionContext.get());
    VectorHelper::FreeVecBatch(vecBatch);
    ResetInputVecBatch();

    lookupJoinOperator->AddInput(newInputVecBatch);
    SetStatus(lookupJoinOperator->GetStatus());
    return 0;
}

int32_t LookupJoinWithExprOperator::GetOutput(VectorBatch **outputVecBatch)
{
    int32_t status = lookupJoinOperator->GetOutput(outputVecBatch);
    SetStatus(lookupJoinOperator->GetStatus());
    return status;
}

BlockingReason LookupJoinWithExprOperator::IsBlocked(ContinueFuture* future)
{
    return lookupJoinOperator->IsBlocked(future);
}

OmniStatus LookupJoinWithExprOperator::Close()
{
    lookupJoinOperator->Close();
    return OMNI_STATUS_NORMAL;
}
}
}
