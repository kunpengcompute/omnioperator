/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2023-2023. All rights reserved.
 * @Description: sort merge join v3 interface layer implementations
 */

#ifndef OMNI_RUNTIME_SORT_MERGE_JOIN_EXPR_V3_H
#define OMNI_RUNTIME_SORT_MERGE_JOIN_EXPR_V3_H

#include "operator/operator.h"
#include "operator/operator_factory.h"
#include "operator/status.h"
#include "operator/projection/projection.h"
#include "vector/vector.h"
#include "type/data_types.h"
#include "operator/join/common_join.h"
#include "sort_merge_join_v3.h"

namespace omniruntime {
namespace op {
// When using SortMergeJoin operator, the call sequence is as follows:
// 1) auto streamOperatorFactory = new StreamedTableWithExprOperatorFactory(...)
// 2) auto bufferOperatorFactory = new BufferedTableWithExprOperatorFactory(...)
// 3) auto streamOperator = streamOperatorFactory->CreateOperator()
// 4) auto bufferOperator = bufferOperatorFactory->CreateOperator()
// 5) streamOperator->AddInput(...)
// 6) bufferOperator->AddInput(...)
// 7) streamOperator->GetOutput(...)
class StreamedTableWithExprOperatorFactoryV3 : public OperatorFactory {
public:
    static StreamedTableWithExprOperatorFactoryV3 *CreateStreamedTableWithExprOperatorFactory(
        const type::DataTypes &streamTypes, const std::vector<omniruntime::expressions::Expr *> &streamJoinKeys,
        const std::vector<int32_t> &streamOutputCols, JoinType inputJoinType, std::string &filterExpr,
        const OperatorConfig &operatorConfig);
    static StreamedTableWithExprOperatorFactoryV3 *CreateStreamedTableWithExprOperatorFactory(
        const type::DataTypes &streamTypes, const std::vector<omniruntime::expressions::Expr *> &streamJoinKeys,
        const std::vector<int32_t> &streamOutputCols, JoinType inputJoinType, std::string &filterExpr,
        const OperatorConfig &operatorConfig, const config::QueryConfig &queryConfig);

    StreamedTableWithExprOperatorFactoryV3(const type::DataTypes &streamTypes,
        const std::vector<omniruntime::expressions::Expr *> &streamJoinKeys,
        const std::vector<int32_t> &streamOutputCols, JoinType joinType, std::string &filterExpr,
        const OperatorConfig &operatorConfig);
    StreamedTableWithExprOperatorFactoryV3(const type::DataTypes &streamTypes,
        const std::vector<omniruntime::expressions::Expr *> &streamJoinKeys,
        const std::vector<int32_t> &streamOutputCols, JoinType joinType, std::string &filterExpr,
        const OperatorConfig &operatorConfig, const config::QueryConfig &queryConfig);

    ~StreamedTableWithExprOperatorFactoryV3() override;

    omniruntime::op::Operator *CreateOperator() override;

    SortMergeJoinOperatorV3 *GetSmjOperator();

private:
    std::unique_ptr<DataTypes> streamTypes;
    std::vector<int32_t> streamJoinCols;
    std::vector<int32_t> streamOutputCols;
    JoinType joinType;
    std::string filter;
    SortMergeJoinOperatorV3 *smjOperator;
    std::vector<std::unique_ptr<Projection>> projections;
};

class StreamedTableWithExprOperatorV3 : public Operator {
public:
    StreamedTableWithExprOperatorV3(const type::DataTypes &streamTypes,
        std::vector<std::unique_ptr<Projection>> &projections, SortMergeJoinOperatorV3 *smjOperator);

    ~StreamedTableWithExprOperatorV3() override;

    int32_t AddInput(VectorBatch *vecBatch) override;

    int32_t GetOutput(VectorBatch **outputVecBatch) override;

    OmniStatus Close() override;

private:
    DataTypes streamTypes;
    std::vector<std::unique_ptr<Projection>> &projections;
    SortMergeJoinOperatorV3 *smjOperator;
};

class BufferedTableWithExprOperatorFactoryV3 : public OperatorFactory {
public:
    static BufferedTableWithExprOperatorFactoryV3 *CreateBufferedTableWithExprOperatorFactory(
        const DataTypes &bufferTypes, const std::vector<omniruntime::expressions::Expr *> &bufferJoinKeys,
        const std::vector<int32_t> &bufferOutputCols, StreamedTableWithExprOperatorFactoryV3 *streamTableFactory,
        const OperatorConfig &operatorConfig);

    BufferedTableWithExprOperatorFactoryV3(const type::DataTypes &bufferTypes,
        const std::vector<omniruntime::expressions::Expr *> &bufferJoinKeys,
        const std::vector<int32_t> &bufferOutputCols, StreamedTableWithExprOperatorFactoryV3 *streamTableFactory,
        const OperatorConfig &operatorConfig);

    ~BufferedTableWithExprOperatorFactoryV3() override;

    omniruntime::op::Operator *CreateOperator() override;

private:
    std::unique_ptr<DataTypes> bufferTypes;
    std::vector<int32_t> bufferJoinCols;
    std::vector<int32_t> bufferOutputCols;
    std::vector<std::unique_ptr<Projection>> projections;
    SortMergeJoinOperatorV3 *smjOperator;
};

class BufferedTableWithExprOperatorV3 : public Operator {
public:
    BufferedTableWithExprOperatorV3(const type::DataTypes &bufferTypes,
        std::vector<std::unique_ptr<Projection>> &projections, SortMergeJoinOperatorV3 *smjOperator);

    ~BufferedTableWithExprOperatorV3() override;

    int32_t AddInput(VectorBatch *vecBatch) override;

    int32_t GetOutput(VectorBatch **outputVecBatch) override;

    OmniStatus Close() override;

private:
    DataTypes bufferTypes;
    std::vector<std::unique_ptr<Projection>> &projections;
    SortMergeJoinOperatorV3 *smjOperator;
};
}
}

#endif // OMNI_RUNTIME_SORT_MERGE_JOIN_EXPR_V3_H
