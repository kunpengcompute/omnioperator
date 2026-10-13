/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2020. All rights reserved.
 */
/*
 * @Copyright: Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights
 * reserved.
 * @Description: nested loop join builder implementations
 */
#ifndef __NEST_LOOP_JOIN_BUILDER_H__
#define __NEST_LOOP_JOIN_BUILDER_H__

#include <memory>

#include "common_join.h"
#include "join_hash_table_variants.h"
#include "operator/operator.h"
#include "operator/operator_factory.h"
#include "plannode/planNode.h"

namespace omniruntime {
namespace op {
class NestedLoopJoinBuildOperatorFactory : public OperatorFactory {
public:
    NestedLoopJoinBuildOperatorFactory(DataTypes buildTypes, int32_t *buildOutputCols, int32_t buildOutputColsCount);

    static NestedLoopJoinBuildOperatorFactory *CreateNestedLoopJoinBuildOperatorFactory(
        std::shared_ptr<const NestedLoopJoinNode> planNode);

    ~NestedLoopJoinBuildOperatorFactory() = default;

    using OperatorFactory::CreateOperator;
    // Pool-bound override
    omniruntime::op::Operator *CreateOperator(omniruntime::mem::OmniTaskMemoryPool *pool) override;

    VectorBatch *GetBuildVectorBatch();

    DataTypes &GetBuildDataTypes();

    std::vector<int32_t> &GetbuildOutputCols();

private:
    DataTypes buildTypes;
    std::unique_ptr<VectorBatch> vectorBatch;
    std::vector<int32_t> buildOutputCols;
};

class NestedLoopJoinBuildOperator : public Operator {
public:
    // Pool-bound ctor chains Operator(pool)
    NestedLoopJoinBuildOperator(omniruntime::mem::OmniTaskMemoryPool *pool,
        std::unique_ptr<VectorBatch> &vectorBatch, DataTypes &buildTypes);

    ~NestedLoopJoinBuildOperator() = default;

    int32_t AddInput(omniruntime::vec::VectorBatch *vecBatch) override;

    int32_t GetOutput(omniruntime::vec::VectorBatch **outputVecBatch) override;

    OmniStatus Close() override;

private:
    std::unique_ptr<VectorBatch> &inputVectorBatch;
    std::vector<VectorBatch *> inputVectorBatches;
    DataTypes &buildTypes;
    int32_t inputRowCnt = 0;
};
} // namespace op
} // namespace omniruntime
#endif
