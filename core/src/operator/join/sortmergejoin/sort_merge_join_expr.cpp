/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
 * Description: Sort merge join interface layer implementations
 */
#include "sort_merge_join_expr.h"
#include "operator/util/operator_util.h"

namespace omniruntime {
namespace op {
using namespace omniruntime::vec;

StreamedTableWithExprOperatorFactory *StreamedTableWithExprOperatorFactory::CreateStreamedTableWithExprOperatorFactory(
    const type::DataTypes &streamedTypes, const std::vector<omniruntime::expressions::Expr *> &streamedKeyExprCols,
    int32_t streamedKeyExprColsCnt, int32_t *streamedOutputCols, int32_t streamedOutputColsCnt, JoinType inputJoinType,
    std::string &filterExpression, OverflowConfig *overflowConfig)
{
    return new StreamedTableWithExprOperatorFactory(streamedTypes, streamedKeyExprCols, streamedKeyExprColsCnt,
        streamedOutputCols, streamedOutputColsCnt, inputJoinType, filterExpression, overflowConfig);
}

StreamedTableWithExprOperatorFactory* StreamedTableWithExprOperatorFactory::CreateStreamedTableWithExprOperatorFactory(
    const std::shared_ptr<const MergeJoinNode>& planNode, const config::QueryConfig &queryConfig)
{
    auto streamedDataTypes = *(planNode->LeftOutputType());
    auto streamedKeyExprCols = planNode->LeftKeys();
    auto filterExpression = planNode->Filter();
    std::vector<int32_t> streamedOutputColsIndex;
    for (int32_t i = 0; i < planNode->LeftOutputType()->GetSize(); i++) {
        streamedOutputColsIndex.push_back(static_cast<int32_t>(i));
    }
    auto overflowConfig = queryConfig.IsOverFlowASNull() ? new OverflowConfig(OVERFLOW_CONFIG_NULL) :
                                                           new OverflowConfig(OVERFLOW_CONFIG_EXCEPTION);
    auto joinType = planNode->GetJoinType();
    auto factory = new StreamedTableWithExprOperatorFactory(streamedDataTypes, streamedKeyExprCols,
        static_cast<int32_t>(streamedKeyExprCols.size()), streamedOutputColsIndex.data(),
        static_cast<int32_t>(streamedOutputColsIndex.size()), joinType, filterExpression, overflowConfig, queryConfig);
    delete overflowConfig;
    overflowConfig = nullptr;
    return factory;
}

StreamedTableWithExprOperatorFactory::StreamedTableWithExprOperatorFactory(const type::DataTypes &streamedTypes,
    const std::vector<omniruntime::expressions::Expr *> &streamedKeyExprCols, int32_t streamedKeyExprColsCnt,
    int32_t *streamedOutputCols, int32_t streamedOutputColsCnt, JoinType joinType, std::string &filter,
    OverflowConfig *overflowConfig)
    : joinType(joinType), filter(filter), smjOperator(new SortMergeJoinOperator(joinType, filter))
{
    std::vector<DataTypePtr> newBuildTypes;
    OperatorUtil::CreateProjections(streamedTypes, streamedKeyExprCols, newBuildTypes, projections, streamedKeyCols,
        overflowConfig);
    this->streamedTypes = std::make_unique<DataTypes>(newBuildTypes);
    this->streamedOutputCols.insert(this->streamedOutputCols.end(), streamedOutputCols,
        streamedOutputCols + streamedOutputColsCnt);
    smjOperator->ConfigStreamedTblInfo(*(this->streamedTypes), streamedKeyCols, this->streamedOutputCols,
        streamedTypes.GetSize());
}

StreamedTableWithExprOperatorFactory::StreamedTableWithExprOperatorFactory(const type::DataTypes &streamedTypes,
    const std::vector<omniruntime::expressions::Expr *> &streamedKeyExprCols, int32_t streamedKeyExprColsCnt,
    int32_t *streamedOutputCols, int32_t streamedOutputColsCnt, JoinType joinType, std::string &filter,
    OverflowConfig *overflowConfig, const config::QueryConfig &queryConfig)
    : joinType(joinType), filter(filter), smjOperator(new SortMergeJoinOperator(joinType, filter, queryConfig))
{
    this->queryConfig_ = queryConfig;
    std::vector<DataTypePtr> newBuildTypes;
    OperatorUtil::CreateProjections(streamedTypes, streamedKeyExprCols, newBuildTypes, projections, streamedKeyCols,
        overflowConfig);
    this->streamedTypes = std::make_unique<DataTypes>(newBuildTypes);
    this->streamedOutputCols.insert(this->streamedOutputCols.end(), streamedOutputCols,
        streamedOutputCols + streamedOutputColsCnt);
    smjOperator->ConfigStreamedTblInfo(*(this->streamedTypes), streamedKeyCols, this->streamedOutputCols,
        streamedTypes.GetSize());
}

StreamedTableWithExprOperatorFactory::StreamedTableWithExprOperatorFactory(const type::DataTypes& streamedTypes,
    const std::vector<omniruntime::expressions::Expr*>& streamedKeyExprCols, int32_t streamedKeyExprColsCnt,
    int32_t* streamedOutputCols, int32_t streamedOutputColsCnt, JoinType joinType, Expr* filter,
    OverflowConfig* overflowConfig)
    : joinType(joinType),
      filterExpr(filter),
      smjOperator(new SortMergeJoinOperator(joinType, filter))
{
    std::vector<DataTypePtr> newBuildTypes;
    OperatorUtil::CreateProjections(
        streamedTypes, streamedKeyExprCols, newBuildTypes, projections, streamedKeyCols, overflowConfig);
    this->streamedTypes = std::make_unique<DataTypes>(newBuildTypes);
    this->streamedOutputCols.insert(
        this->streamedOutputCols.end(), streamedOutputCols, streamedOutputCols + streamedOutputColsCnt);
    smjOperator->ConfigStreamedTblInfo(
        *(this->streamedTypes), streamedKeyCols, this->streamedOutputCols, streamedTypes.GetSize());
}

StreamedTableWithExprOperatorFactory::StreamedTableWithExprOperatorFactory(const type::DataTypes& streamedTypes,
    const std::vector<omniruntime::expressions::Expr*>& streamedKeyExprCols, int32_t streamedKeyExprColsCnt,
    int32_t* streamedOutputCols, int32_t streamedOutputColsCnt, JoinType joinType, Expr* filter,
    OverflowConfig* overflowConfig, const config::QueryConfig &queryConfig)
    : joinType(joinType),
      filterExpr(filter),
      smjOperator(new SortMergeJoinOperator(joinType, filter, queryConfig))
{
    this->queryConfig_ = queryConfig;
    std::vector<DataTypePtr> newBuildTypes;
    OperatorUtil::CreateProjections(
        streamedTypes, streamedKeyExprCols, newBuildTypes, projections, streamedKeyCols, overflowConfig);
    this->streamedTypes = std::make_unique<DataTypes>(newBuildTypes);
    this->streamedOutputCols.insert(
        this->streamedOutputCols.end(), streamedOutputCols, streamedOutputCols + streamedOutputColsCnt);
    smjOperator->ConfigStreamedTblInfo(
        *(this->streamedTypes), streamedKeyCols, this->streamedOutputCols, streamedTypes.GetSize());
}

StreamedTableWithExprOperatorFactory::~StreamedTableWithExprOperatorFactory()
{
    delete smjOperator;
}

Operator *StreamedTableWithExprOperatorFactory::CreateOperator()
{
    return new StreamedTableWithExprOperator(*streamedTypes, projections, smjOperator);
}

SortMergeJoinOperator *StreamedTableWithExprOperatorFactory::GetSmjOperator()
{
    return smjOperator;
}

StreamedTableWithExprOperator::StreamedTableWithExprOperator(const type::DataTypes &streamedTypes,
    std::vector<std::unique_ptr<Projection>> &projections, SortMergeJoinOperator *smjOperator)
    : streamedTypes(streamedTypes), projections(projections), smjOperator(smjOperator)
{}

StreamedTableWithExprOperator::~StreamedTableWithExprOperator() = default;

int32_t StreamedTableWithExprOperator::AddInput(omniruntime::vec::VectorBatch *vecBatch)
{
    int32_t retCode = -1;
    if (vecBatch == nullptr) {
        return retCode;
    }

    auto *newInputVecBatch = OperatorUtil::ProjectVectors(vecBatch, streamedTypes, projections, executionContext.get());
    VectorHelper::FreeVecBatch(vecBatch);
    inputVecBatch = nullptr;

    retCode = smjOperator->AddStreamedTableInput(newInputVecBatch);
    SetStatus(smjOperator->GetStatus());
    return retCode;
}

int32_t StreamedTableWithExprOperator::GetOutput(omniruntime::vec::VectorBatch** outputVecBatch)
{
    if (!noMoreInput_) {
        return 0;
    }
    int32_t retCode = smjOperator->GetOutput(outputVecBatch);
    SetStatus(smjOperator->GetStatus());
    return retCode;
}

OmniStatus StreamedTableWithExprOperator::Close()
{
    return smjOperator->Close();
}

BufferedTableWithExprOperatorFactory *BufferedTableWithExprOperatorFactory::CreateBufferedTableWithExprOperatorFactory(
    const DataTypes &bufferedTypes, const std::vector<omniruntime::expressions::Expr *> &bufferedKeyExprCols,
    int32_t bufferedKeyExprCnt, int32_t *bufferedOutputCols, int32_t bufferedOutputColsCnt,
    int64_t streamedTableFactoryAddr, OverflowConfig *overflowConfig)
{
    return new BufferedTableWithExprOperatorFactory(bufferedTypes, bufferedKeyExprCols, bufferedKeyExprCnt,
        bufferedOutputCols, bufferedOutputColsCnt, streamedTableFactoryAddr, overflowConfig);
}

BufferedTableWithExprOperatorFactory* BufferedTableWithExprOperatorFactory::CreateBufferedTableWithExprOperatorFactory(
    const std::shared_ptr<const MergeJoinNode>& planNode, int64_t streamedTableFactoryAddr, const config::QueryConfig &queryConfig)
{
    auto types = *(planNode->RightOutputType());
    auto bufferedKeyExprCols = planNode->RightKeys();
    std::vector<int32_t> bufferedOutputColsIndex;
    for (int32_t i = 0; i < planNode->RightOutputType()->GetSize(); i++) {
        bufferedOutputColsIndex.push_back(static_cast<int32_t>(i));
    }

    auto overflowConfig = queryConfig.IsOverFlowASNull() ? new OverflowConfig(OVERFLOW_CONFIG_NULL) :
                                                           new OverflowConfig(OVERFLOW_CONFIG_EXCEPTION);
    auto bufferedOutputColsCnt = static_cast<int32_t>(bufferedOutputColsIndex.size());
    auto factory = new BufferedTableWithExprOperatorFactory(types, bufferedKeyExprCols,
        static_cast<int32_t>(bufferedKeyExprCols.size()), bufferedOutputColsIndex.data(), bufferedOutputColsCnt,
        streamedTableFactoryAddr, overflowConfig, false);
    delete overflowConfig;
    overflowConfig = nullptr;
    return factory;
}

BufferedTableWithExprOperatorFactory::BufferedTableWithExprOperatorFactory(const type::DataTypes& bufferedTypes,
    const std::vector<omniruntime::expressions::Expr*>& bufferedKeyExprCols, int32_t bufferedKeyExprCnt,
    int32_t* bufferedOutputCols, int32_t bufferedOutputColsCnt, int64_t streamedTableFactoryAddr,
    OverflowConfig* overflowConfig, bool filterIsString)
    : streamTblWithExprOperatorFactory(
          reinterpret_cast<StreamedTableWithExprOperatorFactory*>(streamedTableFactoryAddr))
{
    this->filterIsString = filterIsString;
    std::vector<DataTypePtr> newBuildTypes;
    OperatorUtil::CreateProjections(
        bufferedTypes, bufferedKeyExprCols, newBuildTypes, projections, bufferedKeyCols, overflowConfig);
    this->bufferedTypes = std::make_unique<DataTypes>(newBuildTypes);
    this->bufferedOutputCols.insert(this->bufferedOutputCols.end(), bufferedOutputCols,
        bufferedOutputCols + bufferedOutputColsCnt);

    streamTblWithExprOperatorFactory->GetSmjOperator()->ConfigBufferedTblInfo(*(this->bufferedTypes), bufferedKeyCols,
        this->bufferedOutputCols, bufferedTypes.GetSize());
    if (filterIsString) {
        streamTblWithExprOperatorFactory->GetSmjOperator()->InitScannerAndResultBuilder(overflowConfig);
    } else {
        streamTblWithExprOperatorFactory->GetSmjOperator()->InitScannerAndResultBuilderWithFilterExpr(overflowConfig);
    }
}

BufferedTableWithExprOperatorFactory::~BufferedTableWithExprOperatorFactory() = default;

Operator *BufferedTableWithExprOperatorFactory::CreateOperator()
{
    auto *smjOperator = streamTblWithExprOperatorFactory->GetSmjOperator();
    return new BufferedTableWithExprOperator(*bufferedTypes, projections, smjOperator);
}

BufferedTableWithExprOperator::BufferedTableWithExprOperator(const type::DataTypes &bufferedTypes,
    std::vector<std::unique_ptr<Projection>> &projections, SortMergeJoinOperator *smjOperator)
    : bufferedTypes(bufferedTypes), projections(projections), smjOperator(smjOperator)
{}

BufferedTableWithExprOperator::~BufferedTableWithExprOperator() = default;

int32_t BufferedTableWithExprOperator::AddInput(omniruntime::vec::VectorBatch *vecBatch)
{
    int32_t retCode = -1;
    if (vecBatch == nullptr) {
        return retCode;
    }
    auto *newInputVecBatch = OperatorUtil::ProjectVectors(vecBatch, bufferedTypes, projections, executionContext.get());
    VectorHelper::FreeVecBatch(vecBatch);
    inputVecBatch = nullptr;

    retCode = smjOperator->AddBufferedTableInput(newInputVecBatch);
    SetStatus(smjOperator->GetStatus());
    return retCode;
}

int32_t BufferedTableWithExprOperator::GetOutput(omniruntime::vec::VectorBatch **outputVecBatch)
{
    if (!noMoreInput_) {
        return 0;
    }
    int32_t retCode = smjOperator->GetOutput(outputVecBatch);
    SetStatus(smjOperator->GetStatus());
    return retCode;
}

OmniStatus BufferedTableWithExprOperator::Close()
{
    return smjOperator->Close();
}
}
}
