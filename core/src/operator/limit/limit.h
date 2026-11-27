/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */
#ifndef __LIMIT_COUNT_OPERATOR_H__
#define __LIMIT_COUNT_OPERATOR_H__

#include <vector>
#include <memory>
#include "plannode/planNode.h"
#include "operator/operator.h"
#include "operator/operator_factory.h"
#include "type/data_type_serializer.h"

namespace omniruntime {
namespace op {
class LimitOperatorFactory : public OperatorFactory {
public:
    LimitOperatorFactory(int32_t limit, int32_t offset, int32_t columnCount = 0);

    ~LimitOperatorFactory() override;

    static LimitOperatorFactory *CreateLimitOperatorFactory(int32_t limitNum, int32_t offsetNum);

    static LimitOperatorFactory *CreateLimitOperatorFactory(std::shared_ptr<const LimitNode> planNode);

    using OperatorFactory::CreateOperator;
    // Pool-bound override — forwards `pool` to the new LimitOperator(pool, ...) ctor
    // so the Limit op binds Vector allocations to the per-task pool. Legacy CreateOperator()
    // + LimitOperator(limit, offset) stay until the atomic ABI flip.
    Operator *CreateOperator(omniruntime::mem::OmniTaskMemoryPool *pool) override;

private:
    int32_t limit;
    int32_t offset;
    int32_t columnCount;
};

class LimitOperator : public Operator {
public:
    // Pool-bound ctor — chains Operator(pool) so pool_ is set + ExecutionContext
    // is pool-bound from construction. Same data members as the legacy ctor.
    LimitOperator(omniruntime::mem::OmniTaskMemoryPool *pool, int32_t limit, int32_t offset,
        int32_t columnCount = 0);

    ~LimitOperator() override;

    int32_t AddInput(omniruntime::vec::VectorBatch *vecBatch) override;

    int32_t GetOutput(omniruntime::vec::VectorBatch **resultVecBatch) override;

    OmniStatus Close() override;

    /// Limit can reach isFinished() before noMoreInput (when remainingLimit <= 0).
    bool isEarlyFinish() const override
    {
        return true;
    }

private:
    int32_t remainingLimit;
    int32_t remainingOffset;
    vec::VectorBatch *outputVecBatch;
};
}
}

#endif // __LIMIT_COUNT_OPERATOR_H__
