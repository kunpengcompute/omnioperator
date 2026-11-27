/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2021-2024. All rights reserved.
 * @Description: sort implementations
 */

#include "sort_expr.h"
#include "sort.h"
#include "operator/util/operator_util.h"
#include "vector/vector_helper.h"

namespace omniruntime {
namespace op {
using namespace omniruntime::vec;

SortWithExprOperatorFactory *SortWithExprOperatorFactory::CreateSortWithExprOperatorFactory(
    const type::DataTypes &sourceTypes, int32_t *outputCols, int32_t outputColsCount,
    const std::vector<omniruntime::expressions::Expr *> &sortKeys, int32_t *sortAscendings, int32_t *sortNullFirsts,
    int32_t sortKeysCount, const OperatorConfig &operatorConfig)
{
    auto pOperatorFactory = new SortWithExprOperatorFactory(sourceTypes, outputCols, outputColsCount, sortKeys,
        sortAscendings, sortNullFirsts, sortKeysCount, operatorConfig);
    return pOperatorFactory;
}

SortWithExprOperatorFactory* SortWithExprOperatorFactory::CreateSortWithExprOperatorFactory(
    std::shared_ptr<const OrderByNode> planNode, const config::QueryConfig& queryConfig)
{
    auto sourceTypes = planNode->GetSourceTypes();
    auto outputCols = const_cast<int32_t*>(planNode->GetOutputCols().data());
    auto outputColsCount = static_cast<int32_t>(planNode->GetOutputCols().size());
    auto sortExpressions = planNode->GetExpressions();
    auto sortAscendings = const_cast<int32_t*>(planNode->GetSortAscending().data());
    auto sortNullFirsts = const_cast<int32_t*>(planNode->GetNullFirsts().data());
    auto expressionCount = static_cast<int32_t>(sortExpressions.size());
    auto spillConfig = new SparkSpillConfig(planNode->CanSpill(queryConfig) & queryConfig.orderBySpillEnabled(),
        queryConfig.SpillDir(), queryConfig.SpillDirDiskReserveSize(), queryConfig.SpillSortRowThreshold(),
        queryConfig.SpillMemFraction(), queryConfig.SpillWriteBufferSize(), queryConfig.SpillEnableCompress());
    auto overflowConfig = queryConfig.IsOverFlowASNull() == true ? new OverflowConfig(OVERFLOW_CONFIG_NULL)
                                                                 : new OverflowConfig(OVERFLOW_CONFIG_EXCEPTION);
    OperatorConfig operatorConfig(spillConfig, overflowConfig);
    operatorConfig.SetSortAlgorithm(queryConfig.PdqSortEnabled(), queryConfig.InplacePdqSortEnabled(),
        queryConfig.TimSortEnabled());
    auto pOperatorFactory = new SortWithExprOperatorFactory(*sourceTypes, outputCols, outputColsCount, sortExpressions,
        sortAscendings, sortNullFirsts, expressionCount, operatorConfig);
    pOperatorFactory->SetQueryConfig(queryConfig);
    return pOperatorFactory;
}

SortWithExprOperatorFactory::SortWithExprOperatorFactory(const type::DataTypes &sourceTypes, int32_t *outputCols,
    int32_t outputColsCount, const std::vector<omniruntime::expressions::Expr *> &sortKeys, int32_t *sortAscendings,
    int32_t *sortNullFirsts, int32_t sortKeysCount, const OperatorConfig &operatorConfig)
{
    std::vector<DataTypePtr> newSourceTypes;
    OperatorUtil::CreateProjections(sourceTypes, sortKeys, newSourceTypes, this->projections, this->sortCols,
        operatorConfig.GetOverflowConfig());
    this->sourceTypes = std::make_unique<DataTypes>(newSourceTypes);
    this->sortOperatorFactory = SortOperatorFactory::CreateSortOperatorFactory(*(this->sourceTypes), outputCols,
        outputColsCount, sortCols.data(), sortAscendings, sortNullFirsts, sortKeysCount, operatorConfig);
}

SortWithExprOperatorFactory::SortWithExprOperatorFactory(const type::DataTypes &sourceTypes, int32_t *outputCols,	
    int32_t outputColsCount, const std::vector<omniruntime::expressions::Expr *> &sortKeys, int32_t *sortAscendings,	
    int32_t *sortNullFirsts, int32_t sortKeysCount, const OperatorConfig &operatorConfig	
    , const config::QueryConfig& queryConfig) : SortWithExprOperatorFactory(sourceTypes, outputCols, outputColsCount,	
    sortKeys, sortAscendings, sortNullFirsts, sortKeysCount, operatorConfig)	
{	
    this->queryConfig_ = queryConfig;	
}

SortWithExprOperatorFactory *SortWithExprOperatorFactory::CreateSortWithExprOperatorFactory(
    const type::DataTypes &sourceTypes, int32_t *outputCols, int32_t outputColsCount,
    const std::vector<omniruntime::expressions::Expr *> &sortKeys, int32_t *sortAscendings, int32_t *sortNullFirsts,
    int32_t sortKeysCount)
{
    auto pOperatorFactory = new SortWithExprOperatorFactory(sourceTypes, outputCols, outputColsCount, sortKeys,
        sortAscendings, sortNullFirsts, sortKeysCount, OperatorConfig());
    return pOperatorFactory;
}

SortWithExprOperatorFactory::~SortWithExprOperatorFactory()
{
    delete sortOperatorFactory;
}

// Pool-bound override forwards pool to inner factory.
Operator *SortWithExprOperatorFactory::CreateOperator(omniruntime::mem::OmniTaskMemoryPool *pool)
{
    auto sortOperator = static_cast<SortOperator *>(sortOperatorFactory->CreateOperator(pool));
    auto pOperator = new SortWithExprOperator(pool, *sourceTypes, projections, sortOperator, queryConfig_);
    pOperator->Init();
    return pOperator;
}

// Pool-bound ctor — same body, chains Operator(pool).
SortWithExprOperator::SortWithExprOperator(omniruntime::mem::OmniTaskMemoryPool *pool,
    const type::DataTypes &sourceTypes, std::vector<std::unique_ptr<Projection>> &projections,
    SortOperator *sortOperator, const config::QueryConfig& queryConfig)
    : Operator(pool), sourceTypes(sourceTypes), projections(projections), sortOperator(sortOperator)
{
    SetOperatorName(metricsNameSort);
    executionContext->SetConfig(queryConfig);
}

SortWithExprOperator::~SortWithExprOperator()
{
    delete sortOperator;
}

OmniStatus SortWithExprOperator::Init()
{
    const auto outerStatus = Operator::Init();
    if (outerStatus != OMNI_STATUS_NORMAL) {
        return outerStatus;
    }
    return sortOperator->Init();
}

int32_t SortWithExprOperator::AddInput(VectorBatch *inputVecBatch)
{
    if (inputVecBatch->GetRowCount() <= 0) {
        VectorHelper::FreeVecBatch(inputVecBatch);
        ResetInputVecBatch();
        return 0;
    }
    UpdateAddInputInfo(inputVecBatch->GetRowCount());
    auto *newInputVecBatch =
        OperatorUtil::ProjectVectors(inputVecBatch, sourceTypes, projections, executionContext.get());
    VectorHelper::FreeVecBatch(inputVecBatch);
    ResetInputVecBatch();
    // Keep the inner operator protected for the complete wrapper dispatch. Its
    // reviewed pre-reservation section temporarily clears this flag immediately
    // before PagesIndex mutation, enabling safe active self-reclaim only there.
    Operator::NonReclaimableSection innerSection(sortOperator);
    sortOperator->AddInput(newInputVecBatch);
    return 0;
}

int32_t SortWithExprOperator::GetOutput(VectorBatch **outputVecBatch)
{
    Operator::NonReclaimableSection innerSection(sortOperator);
    int32_t status = sortOperator->GetOutput(outputVecBatch);
    if (*outputVecBatch != nullptr) {
        UpdateGetOutputInfo((*outputVecBatch)->GetRowCount());
    } else {
        UpdateGetOutputInfo(0);
    }

    if (sortOperator->GetStatus() == OMNI_STATUS_FINISHED) {
        UpdateSpilledMetrics(sortOperator);
    }

    SetStatus(sortOperator->GetStatus());
    return status;
}

OmniStatus SortWithExprOperator::Close()
{
    Operator::NonReclaimableSection innerSection(sortOperator);
    sortOperator->Close();
    return OMNI_STATUS_NORMAL;
}

uint64_t SortWithExprOperator::GetSpilledBytes()
{
    return sortOperator->GetSpilledBytes();
}
}
}
