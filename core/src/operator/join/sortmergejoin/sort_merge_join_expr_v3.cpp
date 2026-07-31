/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * @Description: sort merge join v3 interface layer implementations
 */

#include "sort_merge_join_expr_v3.h"

namespace omniruntime::op {
StreamedTableWithExprOperatorFactoryV3 *
StreamedTableWithExprOperatorFactoryV3::CreateStreamedTableWithExprOperatorFactory(const type::DataTypes &streamTypes,
    const std::vector<omniruntime::expressions::Expr *> &streamJoinKeys, const std::vector<int32_t> &streamOutputCols,
    JoinType inputJoinType, std::string &filterExpression, const OperatorConfig &operatorConfig)
{
    return new StreamedTableWithExprOperatorFactoryV3(streamTypes, streamJoinKeys, streamOutputCols, inputJoinType,
        filterExpression, operatorConfig);
}

StreamedTableWithExprOperatorFactoryV3 *
StreamedTableWithExprOperatorFactoryV3::CreateStreamedTableWithExprOperatorFactory(const type::DataTypes &streamTypes,
    const std::vector<omniruntime::expressions::Expr *> &streamJoinKeys, const std::vector<int32_t> &streamOutputCols,
    JoinType inputJoinType, std::string &filterExpression, const OperatorConfig &operatorConfig,
    const config::QueryConfig &queryConfig)
{
    return new StreamedTableWithExprOperatorFactoryV3(streamTypes, streamJoinKeys, streamOutputCols, inputJoinType,
        filterExpression, operatorConfig, queryConfig);
}

StreamedTableWithExprOperatorFactoryV3::StreamedTableWithExprOperatorFactoryV3(const type::DataTypes &streamTypes,
    const std::vector<omniruntime::expressions::Expr *> &streamJoinKeys, const std::vector<int32_t> &streamOutputCols,
    JoinType joinType, std::string &filter, const OperatorConfig &operatorConfig)
    : streamOutputCols(streamOutputCols), joinType(joinType), smjOperator(new SortMergeJoinOperatorV3(joinType, filter))
{
    std::vector<DataTypePtr> newStreamTypes;
    OperatorUtil::CreateProjections(streamTypes, streamJoinKeys, newStreamTypes, projections, streamJoinCols,
        operatorConfig.GetOverflowConfig());
    this->streamTypes = std::make_unique<DataTypes>(newStreamTypes);
    smjOperator->ConfigStreamInfo(*this->streamTypes, streamJoinCols, this->streamOutputCols, streamTypes.GetSize());
}

StreamedTableWithExprOperatorFactoryV3::StreamedTableWithExprOperatorFactoryV3(const type::DataTypes &streamTypes,
    const std::vector<omniruntime::expressions::Expr *> &streamJoinKeys, const std::vector<int32_t> &streamOutputCols,
    JoinType joinType, std::string &filter, const OperatorConfig &operatorConfig,
    const config::QueryConfig &queryConfig)
    : streamOutputCols(streamOutputCols), joinType(joinType),
      smjOperator(new SortMergeJoinOperatorV3(joinType, filter, queryConfig))
{
    this->queryConfig_ = queryConfig;
    std::vector<DataTypePtr> newStreamTypes;
    OperatorUtil::CreateProjections(streamTypes, streamJoinKeys, newStreamTypes, projections, streamJoinCols,
        operatorConfig.GetOverflowConfig());
    this->streamTypes = std::make_unique<DataTypes>(newStreamTypes);
    smjOperator->ConfigStreamInfo(*this->streamTypes, streamJoinCols, this->streamOutputCols, streamTypes.GetSize());
}

StreamedTableWithExprOperatorFactoryV3::~StreamedTableWithExprOperatorFactoryV3()
{
    delete smjOperator;
}

omniruntime::op::Operator *StreamedTableWithExprOperatorFactoryV3::CreateOperator()
{
    return new StreamedTableWithExprOperatorV3(*streamTypes, projections, smjOperator);
}

SortMergeJoinOperatorV3 *StreamedTableWithExprOperatorFactoryV3::GetSmjOperator()
{
    return smjOperator;
}

StreamedTableWithExprOperatorV3::StreamedTableWithExprOperatorV3(const type::DataTypes &streamTypes,
    std::vector<std::unique_ptr<Projection>> &projections, SortMergeJoinOperatorV3 *smjOperator)
    : streamTypes(streamTypes), projections(projections), smjOperator(smjOperator)
{}

StreamedTableWithExprOperatorV3::~StreamedTableWithExprOperatorV3() = default;

int32_t StreamedTableWithExprOperatorV3::AddInput(VectorBatch *vecBatch)
{
    if (vecBatch == nullptr || vecBatch->GetRowCount() <= 0) {
        VectorHelper::FreeVecBatch(vecBatch);
        return 0;
    }

    auto newInputVecBatch = OperatorUtil::ProjectVectors(vecBatch, streamTypes, projections, executionContext.get());
    VectorHelper::FreeVecBatch(vecBatch);
    this->ResetInputVecBatch();
    smjOperator->AddStreamInput(newInputVecBatch);
    return 0;
}

int32_t StreamedTableWithExprOperatorV3::GetOutput(VectorBatch **outputVecBatch)
{
    std::string message("SortMergeJoin Fusion doesn't support GetOutput for streamTable");
    throw omniruntime::exception::OmniException("UNSUPPORTED_ERROR", message);
}

OmniStatus StreamedTableWithExprOperatorV3::Close()
{
    return smjOperator->Close();
}

BufferedTableWithExprOperatorFactoryV3 *
BufferedTableWithExprOperatorFactoryV3::CreateBufferedTableWithExprOperatorFactory(const DataTypes &bufferTypes,
    const std::vector<omniruntime::expressions::Expr *> &bufferJoinKeys, const std::vector<int32_t> &bufferOutputCols,
    StreamedTableWithExprOperatorFactoryV3 *streamTableFactory, const OperatorConfig &operatorConfig)
{
    return new BufferedTableWithExprOperatorFactoryV3(bufferTypes, bufferJoinKeys, bufferOutputCols, streamTableFactory,
        operatorConfig);
}

BufferedTableWithExprOperatorFactoryV3::BufferedTableWithExprOperatorFactoryV3(const type::DataTypes &bufferTypes,
    const std::vector<omniruntime::expressions::Expr *> &bufferJoinKeys, const std::vector<int32_t> &bufferOutputCols,
    StreamedTableWithExprOperatorFactoryV3 *streamTableFactory, const OperatorConfig &operatorConfig)
    : bufferOutputCols(bufferOutputCols), smjOperator(streamTableFactory->GetSmjOperator())
{
    std::vector<DataTypePtr> newBufferTypes;
    OperatorUtil::CreateProjections(bufferTypes, bufferJoinKeys, newBufferTypes, projections, bufferJoinCols,
        operatorConfig.GetOverflowConfig());
    this->bufferTypes = std::make_unique<DataTypes>(newBufferTypes);
    smjOperator->ConfigBufferInfo(*this->bufferTypes, this->bufferJoinCols, this->bufferOutputCols,
        bufferTypes.GetSize());
    smjOperator->JoinFilterCodeGen(operatorConfig.GetOverflowConfig());
}

BufferedTableWithExprOperatorFactoryV3::~BufferedTableWithExprOperatorFactoryV3() = default;

omniruntime::op::Operator *BufferedTableWithExprOperatorFactoryV3::CreateOperator()
{
    return new BufferedTableWithExprOperatorV3(*bufferTypes, projections, smjOperator);
}

BufferedTableWithExprOperatorV3::BufferedTableWithExprOperatorV3(const type::DataTypes &bufferTypes,
    std::vector<std::unique_ptr<Projection>> &projections, SortMergeJoinOperatorV3 *smjOperator)
    : bufferTypes(bufferTypes), projections(projections), smjOperator(smjOperator)
{}

BufferedTableWithExprOperatorV3::~BufferedTableWithExprOperatorV3() = default;

int32_t BufferedTableWithExprOperatorV3::AddInput(VectorBatch *vecBatch)
{
    if (vecBatch == nullptr || vecBatch->GetRowCount() <= 0) {
        VectorHelper::FreeVecBatch(vecBatch);
        this->ResetInputVecBatch();
        return 0;
    }

    auto newInputVecBatch = OperatorUtil::ProjectVectors(vecBatch, bufferTypes, projections, executionContext.get());
    VectorHelper::FreeVecBatch(vecBatch);
    this->ResetInputVecBatch();
    smjOperator->AddBufferInput(newInputVecBatch);

    return 0;
}

int32_t BufferedTableWithExprOperatorV3::GetOutput(VectorBatch **outputVecBatch)
{
    auto status = smjOperator->GetOutput(outputVecBatch);
    SetStatus(status);
    return 0;
}

OmniStatus BufferedTableWithExprOperatorV3::Close()
{
    return smjOperator->Close();
}
}
