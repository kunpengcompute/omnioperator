/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: Sort merge join interface layer implementations
 */
#include "sort_merge_join_expr_v2.h"
#include "operator/util/operator_util.h"

namespace omniruntime {
namespace op {
using namespace omniruntime::vec;

StreamedTableWithExprOperatorFactoryV2 *StreamedTableWithExprOperatorFactoryV2::CreateStreamedTableWithExprOperatorFactoryV2(
    const type::DataTypes &streamedTypes, const std::vector<omniruntime::expressions::Expr *> &streamedKeyExprCols,
    int32_t streamedKeyExprColsCnt, int32_t *streamedOutputCols, int32_t streamedOutputColsCnt, JoinType inputJoinType,
    std::string &filterExpression, OverflowConfig *overflowConfig)
{
    return new StreamedTableWithExprOperatorFactoryV2(streamedTypes, streamedKeyExprCols, streamedKeyExprColsCnt,
        streamedOutputCols, streamedOutputColsCnt, inputJoinType, filterExpression, overflowConfig);
}

StreamedTableWithExprOperatorFactoryV2* StreamedTableWithExprOperatorFactoryV2::CreateStreamedTableWithExprOperatorFactoryV2(
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
    auto factory = new StreamedTableWithExprOperatorFactoryV2(streamedDataTypes, streamedKeyExprCols,
        static_cast<int32_t>(streamedKeyExprCols.size()), streamedOutputColsIndex.data(),
        static_cast<int32_t>(streamedOutputColsIndex.size()), joinType, filterExpression, overflowConfig, queryConfig);
    delete overflowConfig;
    overflowConfig = nullptr;
    return factory;
}

StreamedTableWithExprOperatorFactoryV2::StreamedTableWithExprOperatorFactoryV2(const type::DataTypes &streamedTypes,
    const std::vector<omniruntime::expressions::Expr *> &streamedKeyExprCols, int32_t streamedKeyExprColsCnt,
    int32_t *streamedOutputCols, int32_t streamedOutputColsCnt, JoinType joinType, std::string &filter,
    OverflowConfig *overflowConfig)
    : joinType(joinType), filter(filter), smjOperator(std::make_shared<SortMergeJoinOperator>(joinType, filter))
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

StreamedTableWithExprOperatorFactoryV2::StreamedTableWithExprOperatorFactoryV2(const type::DataTypes &streamedTypes,
    const std::vector<omniruntime::expressions::Expr *> &streamedKeyExprCols, int32_t streamedKeyExprColsCnt,
    int32_t *streamedOutputCols, int32_t streamedOutputColsCnt, JoinType joinType, std::string &filter,
    OverflowConfig *overflowConfig, const config::QueryConfig &queryConfig)
    : joinType(joinType), filter(filter),
      smjOperator(std::make_shared<SortMergeJoinOperator>(joinType, filter, queryConfig))
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

StreamedTableWithExprOperatorFactoryV2::StreamedTableWithExprOperatorFactoryV2(const type::DataTypes& streamedTypes,
    const std::vector<omniruntime::expressions::Expr*>& streamedKeyExprCols, int32_t streamedKeyExprColsCnt,
    int32_t* streamedOutputCols, int32_t streamedOutputColsCnt, JoinType joinType, Expr* filter,
    OverflowConfig* overflowConfig)
    : joinType(joinType),
      filterExpr(filter),
      smjOperator(std::make_shared<SortMergeJoinOperator>(joinType, filter))
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

StreamedTableWithExprOperatorFactoryV2::StreamedTableWithExprOperatorFactoryV2(const type::DataTypes& streamedTypes,
    const std::vector<omniruntime::expressions::Expr*>& streamedKeyExprCols, int32_t streamedKeyExprColsCnt,
    int32_t* streamedOutputCols, int32_t streamedOutputColsCnt, JoinType joinType, Expr* filter,
    OverflowConfig* overflowConfig, const config::QueryConfig &queryConfig)
    : joinType(joinType),
      filterExpr(filter),
      smjOperator(std::make_shared<SortMergeJoinOperator>(joinType, filter, queryConfig))
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

StreamedTableWithExprOperatorFactoryV2::~StreamedTableWithExprOperatorFactoryV2() {}

Operator *StreamedTableWithExprOperatorFactoryV2::CreateOperator()
{
    return new StreamedTableWithExprOperatorV2(*streamedTypes, projections, smjOperator);
}

std::shared_ptr<SortMergeJoinOperator> StreamedTableWithExprOperatorFactoryV2::GetSmjOperator()
{
    return smjOperator;
}

JoinType StreamedTableWithExprOperatorFactoryV2::GetJoinType()
{
    return joinType;
}

StreamedTableWithExprOperatorV2::StreamedTableWithExprOperatorV2(const type::DataTypes &streamedTypes,
    std::vector<std::unique_ptr<Projection>> &projections, std::shared_ptr<SortMergeJoinOperator> smjOperator)
    : streamedTypes(streamedTypes), projections(projections), smjOperator(smjOperator)
{}

StreamedTableWithExprOperatorV2::~StreamedTableWithExprOperatorV2() = default;

int32_t StreamedTableWithExprOperatorV2::AddInput(omniruntime::vec::VectorBatch *vecBatch)
{
    int32_t retCode = -1;
    if (vecBatch == nullptr) {
        return retCode;
    }

    auto *newInputVecBatch = OperatorUtil::ProjectVectors(vecBatch, streamedTypes, projections, executionContext.get());
    VectorHelper::FreeVecBatch(vecBatch);
    inputVecBatch = nullptr;

    retCode = smjOperator->AddStreamedTableInput(newInputVecBatch);
    return retCode;
}

int32_t StreamedTableWithExprOperatorV2::GetOutput(omniruntime::vec::VectorBatch** outputVecBatch)
{
    if (smjOperator->isJoinHasData()) {
        int32_t retCode = smjOperator->GetOutput(outputVecBatch);
        return retCode;
    }
    return 0;
}

OmniStatus StreamedTableWithExprOperatorV2::Close()
{
    return smjOperator->Close();
}

BufferedTableWithExprOperatorFactoryV2 *BufferedTableWithExprOperatorFactoryV2::CreateBufferedTableWithExprOperatorFactoryV2(
    const DataTypes &bufferedTypes, const std::vector<omniruntime::expressions::Expr *> &bufferedKeyExprCols,
    int32_t bufferedKeyExprCnt, int32_t *bufferedOutputCols, int32_t bufferedOutputColsCnt,
    int64_t streamedTableFactoryAddr, OverflowConfig *overflowConfig)
{
    return new BufferedTableWithExprOperatorFactoryV2(bufferedTypes, bufferedKeyExprCols, bufferedKeyExprCnt,
        bufferedOutputCols, bufferedOutputColsCnt, streamedTableFactoryAddr, overflowConfig);
}

BufferedTableWithExprOperatorFactoryV2* BufferedTableWithExprOperatorFactoryV2::CreateBufferedTableWithExprOperatorFactoryV2(
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
    auto factory = new BufferedTableWithExprOperatorFactoryV2(types, bufferedKeyExprCols,
        static_cast<int32_t>(bufferedKeyExprCols.size()), bufferedOutputColsIndex.data(), bufferedOutputColsCnt,
        streamedTableFactoryAddr, overflowConfig, false);
    delete overflowConfig;
    overflowConfig = nullptr;
    return factory;
}

BufferedTableWithExprOperatorFactoryV2::BufferedTableWithExprOperatorFactoryV2(const type::DataTypes& bufferedTypes,
    const std::vector<omniruntime::expressions::Expr*>& bufferedKeyExprCols, int32_t bufferedKeyExprCnt,
    int32_t* bufferedOutputCols, int32_t bufferedOutputColsCnt, int64_t streamedTableFactoryAddr,
    OverflowConfig* overflowConfig, bool filterIsString)
    : streamTblWithExprOperatorFactory(
          reinterpret_cast<StreamedTableWithExprOperatorFactoryV2*>(streamedTableFactoryAddr))
{
    this->filterIsString = filterIsString;
    std::vector<DataTypePtr> newBuildTypes;
    OperatorUtil::CreateProjections(
        bufferedTypes, bufferedKeyExprCols, newBuildTypes, projections, bufferedKeyCols, overflowConfig);
    this->bufferedTypes = std::make_unique<DataTypes>(newBuildTypes);
    if (streamTblWithExprOperatorFactory->GetJoinType() != OMNI_JOIN_TYPE_LEFT_SEMI) {
        this->bufferedOutputCols.insert(this->bufferedOutputCols.end(), bufferedOutputCols,
                                        bufferedOutputCols + bufferedOutputColsCnt);
    }

    streamTblWithExprOperatorFactory->GetSmjOperator()->ConfigBufferedTblInfo(*(this->bufferedTypes), bufferedKeyCols,
        this->bufferedOutputCols, bufferedTypes.GetSize());
    if (filterIsString) {
        streamTblWithExprOperatorFactory->GetSmjOperator()->InitScannerAndResultBuilder(overflowConfig);
    } else {
        streamTblWithExprOperatorFactory->GetSmjOperator()->InitScannerAndResultBuilderWithFilterExpr(overflowConfig);
    }
}

BufferedTableWithExprOperatorFactoryV2::~BufferedTableWithExprOperatorFactoryV2() = default;

Operator *BufferedTableWithExprOperatorFactoryV2::CreateOperator()
{
    auto smjOperator = streamTblWithExprOperatorFactory->GetSmjOperator();
    return new BufferedTableWithExprOperatorV2(*bufferedTypes, projections, smjOperator);
}

BufferedTableWithExprOperatorV2::BufferedTableWithExprOperatorV2(const type::DataTypes &bufferedTypes,
    std::vector<std::unique_ptr<Projection>> &projections, std::shared_ptr<SortMergeJoinOperator> smjOperator)
    : bufferedTypes(bufferedTypes), projections(projections), smjOperator(smjOperator)
{}

BufferedTableWithExprOperatorV2::~BufferedTableWithExprOperatorV2() = default;

int32_t BufferedTableWithExprOperatorV2::AddInput(omniruntime::vec::VectorBatch *vecBatch)
{
    int32_t retCode = -1;
    if (vecBatch == nullptr) {
        return retCode;
    }
    auto *newInputVecBatch = OperatorUtil::ProjectVectors(vecBatch, bufferedTypes, projections, executionContext.get());
    VectorHelper::FreeVecBatch(vecBatch);
    inputVecBatch = nullptr;

    retCode = smjOperator->AddBufferedTableInput(newInputVecBatch);
    return retCode;
}

int32_t BufferedTableWithExprOperatorV2::GetOutput(omniruntime::vec::VectorBatch **outputVecBatch)
{
    return 0;
}

OmniStatus BufferedTableWithExprOperatorV2::Close()
{
    return smjOperator->Close();
}
}
}
