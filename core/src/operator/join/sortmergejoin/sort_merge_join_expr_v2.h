/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * @Description: sort merge join interface layer implementations
 */

#ifndef __SORT_MERGE_JOIN_EXPR_V2_H__
#define __SORT_MERGE_JOIN_EXPR_V2_H__

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
class StreamedTableWithExprOperatorFactoryV2 : public OperatorFactory {
public:
    static StreamedTableWithExprOperatorFactoryV2 *CreateStreamedTableWithExprOperatorFactoryV2(
        const type::DataTypes &streamedTypes, const std::vector<omniruntime::expressions::Expr *> &streamedKeyExprCols,
        int32_t streamedKeyExprColsCnt, int32_t *streamedOutputCols, int32_t streamedOutputColsCnt,
        JoinType inputJoinType, std::string &filterExpression, OverflowConfig *overflowConfig);
    static StreamedTableWithExprOperatorFactoryV2* CreateStreamedTableWithExprOperatorFactoryV2(
        const std::shared_ptr<const MergeJoinNode>& planNode, const config::QueryConfig& queryConfig);
    StreamedTableWithExprOperatorFactoryV2(const type::DataTypes &streamedTypes,
        const std::vector<omniruntime::expressions::Expr *> &streamedKeyExprCols, int32_t streamedKeyExprColsCnt,
        int32_t *streamedOutputCols, int32_t streamedOutputColsCnt, JoinType joinType, std::string &filter,
        OverflowConfig *overflowConfig);
    StreamedTableWithExprOperatorFactoryV2(const type::DataTypes &streamedTypes,
        const std::vector<omniruntime::expressions::Expr *> &streamedKeyExprCols, int32_t streamedKeyExprColsCnt,
        int32_t *streamedOutputCols, int32_t streamedOutputColsCnt, JoinType joinType, std::string &filter,
        OverflowConfig *overflowConfig, const config::QueryConfig &queryConfig);
    StreamedTableWithExprOperatorFactoryV2(const type::DataTypes& streamedTypes,
        const std::vector<omniruntime::expressions::Expr*>& streamedKeyExprCols, int32_t streamedKeyExprColsCnt,
        int32_t* streamedOutputCols, int32_t streamedOutputColsCnt, JoinType joinType, Expr* filter,
        OverflowConfig* overflowConfig);
    StreamedTableWithExprOperatorFactoryV2(const type::DataTypes& streamedTypes,
        const std::vector<omniruntime::expressions::Expr*>& streamedKeyExprCols, int32_t streamedKeyExprColsCnt,
        int32_t* streamedOutputCols, int32_t streamedOutputColsCnt, JoinType joinType, Expr* filter,
        OverflowConfig* overflowConfig, const config::QueryConfig &queryConfig);

    ~StreamedTableWithExprOperatorFactoryV2() override;

    omniruntime::op::Operator *CreateOperator() override;

    std::shared_ptr<SortMergeJoinOperator> GetSmjOperator();

    JoinType GetJoinType();

private:
    std::unique_ptr<DataTypes> streamedTypes;
    std::vector<int32_t> streamedKeyCols;
    std::vector<int32_t> streamedOutputCols;
    JoinType joinType;
    std::string filter;
    Expr* filterExpr = nullptr;
    std::shared_ptr<SortMergeJoinOperator> smjOperator;
    std::vector<std::unique_ptr<Projection>> projections;
};

class StreamedTableWithExprOperatorV2 : public Operator {
public:
    StreamedTableWithExprOperatorV2(const type::DataTypes &streamedTypes,
        std::vector<std::unique_ptr<Projection>> &projections, std::shared_ptr<SortMergeJoinOperator> smjOperator);

    ~StreamedTableWithExprOperatorV2() override;

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

    bool needsInput()
    {
        // 1.when smjOperator isJoinHasData, need to GetOutput, return false.
        if (smjOperator->isJoinHasData()) {
            return false;
        }

        // 2.when smjOperator isNeedAddStream, need to addInput util noMoreInput_, return true.
        bool flag = smjOperator->isNeedAddStream();
        return GetStatus() != OMNI_STATUS_FINISHED && !noMoreInput_ && flag;
    }

    BlockingReason IsBlocked(ContinueFuture *future)
    {
        if (smjOperator->isScanFinish() && !smjOperator->isJoinHasData()) {
            SetStatus(OMNI_STATUS_FINISHED);
            return BlockingReason::kNotBlocked;
        }

        if (noMoreInput_ && smjOperator->isNeedAddStream()) {
            noMoreInput();
            return BlockingReason::kNotBlocked;
        }

        // 1.when smjOperator is isNeedAddStream, return kNotBlocked.
        // 2.when smjOperator is isJoinHasData, return kNotBlocked.
        // 3.when smjOperator is isScanFinish, return kNotBlocked.
        if (smjOperator->isNeedAddStream() || smjOperator->isScanFinish() || smjOperator->isJoinHasData()) {
            return BlockingReason::kNotBlocked;
        } else {
            return BlockingReason::kWaitForJoinBuild;
        }
    }

private:
    const DataTypes &streamedTypes;
    std::vector<std::unique_ptr<Projection>> &projections;
    std::shared_ptr<SortMergeJoinOperator> smjOperator;
};

class BufferedTableWithExprOperatorFactoryV2 : public OperatorFactory {
public:
    static BufferedTableWithExprOperatorFactoryV2 *CreateBufferedTableWithExprOperatorFactoryV2(
        const DataTypes &bufferedTypes, const std::vector<omniruntime::expressions::Expr *> &bufferedKeyExprCols,
        int32_t bufferedKeyExprCnt, int32_t *bufferedOutputCols, int32_t bufferedOutputColsCnt,
        int64_t streamedTableFactoryAddr, OverflowConfig *overflowConfig);
    static BufferedTableWithExprOperatorFactoryV2* CreateBufferedTableWithExprOperatorFactoryV2(
        const std::shared_ptr<const MergeJoinNode>& planNode, int64_t streamedTableFactoryAddr,
        const config::QueryConfig& queryConfig);

    BufferedTableWithExprOperatorFactoryV2(const type::DataTypes &bufferedTypes,
        const std::vector<omniruntime::expressions::Expr *> &bufferedKeyExprCols, int32_t bufferedKeyExprCnt,
        int32_t *bufferedOutputCols, int32_t bufferedOutputColsCnt, int64_t streamedTableFactoryAddr,
        OverflowConfig *overflowConfig, bool filterIsString = true);

    ~BufferedTableWithExprOperatorFactoryV2() override;

    omniruntime::op::Operator *CreateOperator() override;

private:
    bool filterIsString = true;
    std::unique_ptr<DataTypes> bufferedTypes;
    std::vector<int32_t> bufferedKeyCols;
    std::vector<int32_t> bufferedOutputCols;
    std::vector<std::unique_ptr<Projection>> projections;
    StreamedTableWithExprOperatorFactoryV2 *streamTblWithExprOperatorFactory;
};

class BufferedTableWithExprOperatorV2 : public Operator {
public:
    BufferedTableWithExprOperatorV2(const type::DataTypes &bufferedTypes,
        std::vector<std::unique_ptr<Projection>> &projections, std::shared_ptr<SortMergeJoinOperator> smjOperator);

    ~BufferedTableWithExprOperatorV2() override;

    // return code see SortMergeJoinAddInputCode
    int32_t AddInput(VectorBatch *vecBatch) override;

    int32_t GetOutput(VectorBatch **outputVecBatch) override;

    OmniStatus Close() override;

    void noMoreInput() override
    {
        noMoreInput_ = true;
        auto eOfvectorBatch = new VectorBatch(0);
        for (unsigned int i = 0; i < bufferedTypes.Get().size(); i++) {
            auto type = bufferedTypes.GetType(i);
            auto vec = VectorHelper::CreateComplexVector(type.get(), 0);
            eOfvectorBatch->Append(vec);
        }
        this->AddInput(eOfvectorBatch);
    }

    bool needsInput()
    {
        // 1.when smjOperator isJoinHasData, need to GetOutput, return false.
        if (smjOperator->isJoinHasData()) {
            return false;
        }

        // 2.when smjOperator isNeedAddStream, need to addInput util noMoreInput_, return true.
        bool flag = smjOperator->isNeedAddBuffer();
        return GetStatus() != OMNI_STATUS_FINISHED && !noMoreInput_ && flag;
    }

    BlockingReason IsBlocked(ContinueFuture *future)
    {
        if (smjOperator->isScanFinish() && !smjOperator->isJoinHasData()) {
            SetStatus(OMNI_STATUS_FINISHED);
            return BlockingReason::kNotBlocked;
        }

        if (noMoreInput_ && smjOperator->isNeedAddBuffer()) {
            noMoreInput();
            return BlockingReason::kNotBlocked;
        }

        // 1.when smjOperator is isJoinHasData, StreamOp need GetOutput, return kWaitForJoinBuild.
        if (smjOperator->isJoinHasData()) {
            return BlockingReason::kWaitForJoinBuild;
        }

        // 2.when smjOperator is isNeedAddStream, return kNotBlocked.
        // 3.when smjOperator is isScanFinish, return kNotBlocked.
        if (smjOperator->isNeedAddBuffer() || smjOperator->isScanFinish()) {
            return BlockingReason::kNotBlocked;
        } else {
            return BlockingReason::kWaitForJoinBuild;
        }
    }

private:
    DataTypes bufferedTypes;
    std::vector<std::unique_ptr<Projection>> &projections;
    std::shared_ptr<SortMergeJoinOperator> smjOperator;
};
}
}
#endif // __SORT_MERGE_JOIN_EXPR_V2_H__
