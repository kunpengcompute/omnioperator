/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2024. All rights reserved.
 */
#ifndef __OMNI_OPERATOR_H__
#define __OMNI_OPERATOR_H__

#include <cstdint>
#include <vector>

#include "execution_context.h"
#include "status.h"
#include "vector/vector_batch.h"
#include "vector/vector_helper.h"
#include "metrics/metrics.h"
#include "compute/reason.h"
#include "compute/operator_stats.h"

namespace omniruntime {
namespace op {
using namespace omniruntime::vec;
using namespace omniruntime::compute;
class Operator {
public:
    Operator()
        : sourceTypes(nullptr),
          executionContext(std::make_unique<ExecutionContext>()),
          inputVecBatch(nullptr),
          status(OMNI_STATUS_NORMAL)
    {
    }

    virtual ~Operator()
    {
    }

    virtual int32_t AddInput(omniruntime::vec::VectorBatch* vecBatch) = 0;

    virtual int32_t GetOutput(omniruntime::vec::VectorBatch** result) = 0;

    static void DeleteOperator(Operator* op)
    {
        op->Close();
        delete op;
    }

    OmniStatus GetStatus()
    {
        return status;
    }

    void SetStatus(OmniStatus omniStatus)
    {
        this->status = omniStatus;
    };

    virtual OmniStatus Init()
    {
        return OMNI_STATUS_NORMAL;
    }

    virtual OmniStatus Close()
    {
        return OMNI_STATUS_NORMAL;
    }

    virtual uint64_t GetSpilledBytes()
    {
        return 0;
    }

    virtual uint64_t GetUsedMemBytes()
    {
        return 0;
    }

    virtual uint64_t GetTotalMemBytes()
    {
        return 0;
    }

    virtual std::vector<uint64_t> GetSpecialMetricsInfo()
    {
        return {};
    }

    // Obtains the number of keys in the hashmap object.
    virtual uint64_t GetHashMapUniqueKeys()
    {
        return 0;
    }

    virtual VectorBatch* AlignSchema(VectorBatch* inputVecBatch)
    {
        return inputVecBatch;
    }

    omniruntime::vec::VectorBatch* GetInputVecBatch()
    {
        return inputVecBatch;
    }

    void SetInputVecBatch(vec::VectorBatch* inVecBatch)
    {
        inputVecBatch = inVecBatch;
    }

    // no need to clear memory when exception, so we have to reset
    void ResetInputVecBatch()
    {
        inputVecBatch = nullptr;
    }

    virtual BlockingReason IsBlocked(ContinueFuture* future)
    {
        return BlockingReason::kNotBlocked;
    }

    virtual void noMoreInput()
    {
        noMoreInput_ = true;
    }

    virtual void setNoMoreInput(bool noMoreInput)
    {
        noMoreInput_ = noMoreInput;
    }

    virtual bool needsInput()
    {
        return status != OMNI_STATUS_FINISHED && !noMoreInput_;
    }

    bool isFinished()
    {
        return status == OMNI_STATUS_FINISHED;
    }

    /// Returns true if the operator can reach isFinished() before noMoreInput
    /// (e.g. Limit, DistinctLimit, TableScan). The Driver uses this to choose
    /// the appropriate cascade-release path. Defaults to false.
    virtual bool isEarlyFinish() const
    {
        return false;
    }

    /// Exposes noMoreInput_ state for the Driver's safety checks.
    bool hasNoMoreInput() const
    {
        return noMoreInput_;
    }

    bool hasInputedData()
    {
        return hasInputedData_;
    }

    void setInputedData(bool hasInputedData)
    {
        this->hasInputedData_ = hasInputedData;
    }

    OperatorStats stats(bool clear)
    {
        OperatorStats stats = stats_;
        if (clear) {
            stats = stats_;
            stats_.Clear();
        }
        return stats;
    }

    void SetOperatorType(string opType)
    {
        operatorType_ = opType;
    }

    const std::string& operatorType() const
    {
        return operatorType_;
    }

    const PlanNodeId& planNodeId() const
    {
        return planNodeId_;
    }

    void SetPlanNodeId(PlanNodeId nodeId)
    {
        planNodeId_ = nodeId;
    }

    int32_t GetOperatorId() const
    {
        return operatorId_;
    }

    void SetOperatorId(int32_t opId)
    {
        operatorId_ = opId;
    }

    OperatorStats& stats()
    {
        return stats_;
    }

    void setInputOperatorCnt(int32_t cnt)
    {
        inputOperatorCnt_ = cnt;
    }

    OperatorStats stats_;

protected:
    int32_t* sourceTypes;
    std::unique_ptr<ExecutionContext> executionContext;
    vec::VectorBatch* inputVecBatch = nullptr;
    bool noMoreInput_{true};
    int32_t inputOperatorCnt_{0};

    void UpdateAddInputInfo(int32_t rowCount)
    {
        if (LIKELY(!IsDebugEnable())) {
            return;
        }
        metrics.UpdateAddInputInfo(rowCount, executionContext);
    }

    void UpdateGetOutputInfo(int32_t rowCount)
    {
        if (LIKELY(!IsDebugEnable())) {
            return;
        }
        metrics.UpdateGetOutputInfo(rowCount, executionContext);
    }

    void UpdateSpillFileInfo(uint32_t fileCount)
    {
        if (LIKELY(!IsDebugEnable())) {
            return;
        }
        metrics.UpdateSpillFileInfo(fileCount, executionContext);
    }

    void UpdateSpillTimesInfo()
    {
        if (LIKELY(!IsDebugEnable())) {
            return;
        }
        metrics.UpdateSpillTimesInfo(executionContext);
    }

    void UpdateCloseInfo()
    {
        if (LIKELY(!IsDebugEnable())) {
            return;
        }
        metrics.UpdateCloseInfo(executionContext);
    }

    void SetOperatorName(const std::string& operatorName)
    {
        operatorType_ = operatorName;
        if (LIKELY(!IsDebugEnable())) {
            return;
        }
        metrics.SetOperatorName(operatorName);
    }

    // update spilled metrics of encapsulate operator, such as SortExprOperator/HashAggregationWithExprOperator/WindowWithExprOperator
    // parameter op usually is SortOperator/HashAggregationOperator/WindowOperator
    void UpdateSpilledMetrics(Operator* op)
    {
        auto sortOpStats = op->stats();
        stats_.spilledBytes = sortOpStats.spilledBytes;
        stats_.spilledRows = sortOpStats.spilledRows;
    }

private:
    OmniStatus status;
    Metrics metrics;
    PlanNodeId planNodeId_;
    std::string operatorType_;
    int32_t operatorId_;
    // for pipeline
    bool hasInputedData_{false};
};
}  // namespace op
}  // namespace omniruntime
#endif