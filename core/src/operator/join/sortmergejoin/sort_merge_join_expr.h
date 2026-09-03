/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2021-2021. All rights reserved.
 * @Description: sort merge join interface layer implementations
 */

#ifndef __SORT_MERGE_JOIN_EXPR_H__
#define __SORT_MERGE_JOIN_EXPR_H__

#include "operator/operator.h"
#include "operator/operator_factory.h"
#include "operator/status.h"
#include "operator/projection/projection.h"
#include "vector/vector.h"
#include "type/data_types.h"
#include "operator/join/common_join.h"
#include "dynamic_pages_index.h"
#include "sort_merge_join.h"
#include "plannode/planNode.h"

namespace omniruntime {
namespace op {
class StreamedTableWithExprOperatorFactory : public OperatorFactory {
public:
    static StreamedTableWithExprOperatorFactory *CreateStreamedTableWithExprOperatorFactory(
        const type::DataTypes &streamedTypes, const std::vector<omniruntime::expressions::Expr *> &streamedKeyExprCols,
        int32_t streamedKeyExprColsCnt, int32_t *streamedOutputCols, int32_t streamedOutputColsCnt,
        JoinType inputJoinType, std::string &filterExpression, OverflowConfig *overflowConfig);
    static StreamedTableWithExprOperatorFactory* CreateStreamedTableWithExprOperatorFactory(
        const std::shared_ptr<const MergeJoinNode>& planNode, const config::QueryConfig& queryConfig);
    StreamedTableWithExprOperatorFactory(const type::DataTypes &streamedTypes,
        const std::vector<omniruntime::expressions::Expr *> &streamedKeyExprCols, int32_t streamedKeyExprColsCnt,
        int32_t *streamedOutputCols, int32_t streamedOutputColsCnt, JoinType joinType, std::string &filter,
        OverflowConfig *overflowConfig);
    StreamedTableWithExprOperatorFactory(const type::DataTypes &streamedTypes,
        const std::vector<omniruntime::expressions::Expr *> &streamedKeyExprCols, int32_t streamedKeyExprColsCnt,
        int32_t *streamedOutputCols, int32_t streamedOutputColsCnt, JoinType joinType, std::string &filter,
        OverflowConfig *overflowConfig, const config::QueryConfig &queryConfig);
    StreamedTableWithExprOperatorFactory(const type::DataTypes& streamedTypes,
        const std::vector<omniruntime::expressions::Expr*>& streamedKeyExprCols, int32_t streamedKeyExprColsCnt,
        int32_t* streamedOutputCols, int32_t streamedOutputColsCnt, JoinType joinType, Expr* filter,
        OverflowConfig* overflowConfig);
    StreamedTableWithExprOperatorFactory(const type::DataTypes& streamedTypes,
        const std::vector<omniruntime::expressions::Expr*>& streamedKeyExprCols, int32_t streamedKeyExprColsCnt,
        int32_t* streamedOutputCols, int32_t streamedOutputColsCnt, JoinType joinType, Expr* filter,
        OverflowConfig* overflowConfig, const config::QueryConfig &queryConfig);

    ~StreamedTableWithExprOperatorFactory() override;

    omniruntime::op::Operator *CreateOperator() override;

    SortMergeJoinOperator *GetSmjOperator();

private:
    std::unique_ptr<DataTypes> streamedTypes;
    std::vector<int32_t> streamedKeyCols;
    std::vector<int32_t> streamedOutputCols;
    JoinType joinType;
    std::string filter;
    Expr* filterExpr = nullptr;
    SortMergeJoinOperator* smjOperator;
    std::vector<std::unique_ptr<Projection>> projections;
};

class StreamedTableWithExprOperator : public Operator {
public:
    StreamedTableWithExprOperator(const type::DataTypes &streamedTypes,
        std::vector<std::unique_ptr<Projection>> &projections, SortMergeJoinOperator *smjOperator);

    ~StreamedTableWithExprOperator() override;

    // return code see SortMergeJoinAddInputCode
    int32_t AddInput(VectorBatch *vecBatch) override;

    int32_t GetOutput(VectorBatch **outputVecBatch) override;

    OmniStatus Close() override;

    void noMoreInput() override
    {
        noMoreInput_ = true;
        auto eOfvectorBatch = new VectorBatch(0);
        for (int i = 0; i < streamedTypes.Get().size(); i++) {
            auto type = streamedTypes.GetType(i);
            auto vec = VectorHelper::CreateComplexVector(type.get(), 0);
            eOfvectorBatch->Append(vec);
        }
        this->AddInput(eOfvectorBatch);
    }

private:
    const DataTypes &streamedTypes;
    std::vector<std::unique_ptr<Projection>> &projections;
    SortMergeJoinOperator *smjOperator;
};

class BufferedTableWithExprOperatorFactory : public OperatorFactory {
public:
    static BufferedTableWithExprOperatorFactory *CreateBufferedTableWithExprOperatorFactory(
        const DataTypes &bufferedTypes, const std::vector<omniruntime::expressions::Expr *> &bufferedKeyExprCols,
        int32_t bufferedKeyExprCnt, int32_t *bufferedOutputCols, int32_t bufferedOutputColsCnt,
        int64_t streamedTableFactoryAddr, OverflowConfig *overflowConfig);
    static BufferedTableWithExprOperatorFactory* CreateBufferedTableWithExprOperatorFactory(
        const std::shared_ptr<const MergeJoinNode>& planNode, int64_t streamedTableFactoryAddr,
        const config::QueryConfig& queryConfig);

    BufferedTableWithExprOperatorFactory(const type::DataTypes &bufferedTypes,
        const std::vector<omniruntime::expressions::Expr *> &bufferedKeyExprCols, int32_t bufferedKeyExprCnt,
        int32_t *bufferedOutputCols, int32_t bufferedOutputColsCnt, int64_t streamedTableFactoryAddr,
        OverflowConfig *overflowConfig, bool filterIsString = true);

    ~BufferedTableWithExprOperatorFactory() override;

    omniruntime::op::Operator *CreateOperator() override;

private:
    bool filterIsString = true;
    std::unique_ptr<DataTypes> bufferedTypes;
    std::vector<int32_t> bufferedKeyCols;
    std::vector<int32_t> bufferedOutputCols;
    std::vector<std::unique_ptr<Projection>> projections;
    StreamedTableWithExprOperatorFactory *streamTblWithExprOperatorFactory;
};

class BufferedTableWithExprOperator : public Operator {
public:
    BufferedTableWithExprOperator(const type::DataTypes &bufferedTypes,
        std::vector<std::unique_ptr<Projection>> &projections, SortMergeJoinOperator *smjOperator);

    ~BufferedTableWithExprOperator() override;

    // return code see SortMergeJoinAddInputCode
    int32_t AddInput(VectorBatch *vecBatch) override;

    int32_t GetOutput(VectorBatch **outputVecBatch) override;

    OmniStatus Close() override;

    void noMoreInput() override
    {
        noMoreInput_ = true;
        auto eOfvectorBatch = new VectorBatch(0);
        for (int i = 0; i < bufferedTypes.Get().size(); i++) {
            auto type = bufferedTypes.GetType(i);
            auto vec = VectorHelper::CreateComplexVector(type.get(), 0);
            eOfvectorBatch->Append(vec);
        }
        this->AddInput(eOfvectorBatch);
    }

private:
    DataTypes bufferedTypes;
    std::vector<std::unique_ptr<Projection>> &projections;
    SortMergeJoinOperator *smjOperator;
};
}
}
#endif // __SORT_MERGE_JOIN_EXPR_H__
