/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2024. All rights reserved.
 */
#ifndef __OMNI_OPERATOR_H__
#define __OMNI_OPERATOR_H__

#include <atomic>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

#include "execution_context.h"
#include "status.h"
#include "vector/vector_batch.h"
#include "vector/vector_helper.h"
#include "metrics/metrics.h"
#include "compute/reason.h"
#include "compute/operator_stats.h"
#include "reader/common/Filter.h"
#include "memory/omni_task_memory_pool.h"

namespace omniruntime {
namespace op {
using namespace omniruntime::vec;
using namespace omniruntime::compute;

struct IdentityProjection {
    IdentityProjection(uint32_t _inputChannel, uint32_t _outputChannel)
        : inputChannel(_inputChannel), outputChannel(_outputChannel) {}

    uint32_t inputChannel;
    uint32_t outputChannel;
};

class Operator {
public:
    // Pool-bound primary constructor. pool may be nullptr (test/no-pool path) — accounting
    // is then skipped throughout the Operator's ExecutionContext, and the OMNI_DEBUG gate
    // flags any allocation taken on this path. Production callers always supply a real
    // per-task pool (via JNI nmmHandle decode).
    explicit Operator(omniruntime::mem::OmniTaskMemoryPool *pool)
        : sourceTypes(nullptr),
          executionContext(std::make_unique<ExecutionContext>(4096, pool)),
          inputVecBatch(nullptr),
          pool_(pool),
          status(OMNI_STATUS_NORMAL)
    {
    }

    // Pool accessor — subclasses use this when constructing Vectors so each Vector binds
    // to the operator's per-task pool (per-task accounting + cross-scope free safety).
    omniruntime::mem::OmniTaskMemoryPool *pool() const { return pool_; }

    class NonReclaimableSection {
    public:
        explicit NonReclaimableSection(Operator *op) noexcept
            : NonReclaimableSection(op, op->reclaimGuardEnabled())
        {
        }
        NonReclaimableSection(Operator *op, bool enabled) noexcept
            : op_(op), enabled_(enabled)
        {
            if (enabled_) {
                saved_ = op_->nonReclaimableSection_.exchange(true, std::memory_order_acq_rel);
            }
        }
        ~NonReclaimableSection()
        {
            if (enabled_) {
                op_->nonReclaimableSection_.store(saved_, std::memory_order_release);
            }
        }
        NonReclaimableSection(const NonReclaimableSection &) = delete;
        NonReclaimableSection &operator=(const NonReclaimableSection &) = delete;
    private:
        Operator *op_;
        bool enabled_;
        bool saved_{false};
    };

    // This is intentionally a distinct type: it is permitted only around a
    // reviewed pre-reservation which occurs before operator state mutation.
    class ReclaimableReservationSection {
    public:
        explicit ReclaimableReservationSection(Operator *op) noexcept
            : op_(op), enabled_(op->reclaimGuardEnabled())
        {
            if (enabled_) {
                saved_ = op_->nonReclaimableSection_.exchange(false, std::memory_order_acq_rel);
            }
        }
        ~ReclaimableReservationSection()
        {
            if (enabled_) {
                op_->nonReclaimableSection_.store(saved_, std::memory_order_release);
            }
        }
        ReclaimableReservationSection(const ReclaimableReservationSection &) = delete;
        ReclaimableReservationSection &operator=(const ReclaimableReservationSection &) = delete;
    private:
        Operator *op_;
        bool enabled_;
        bool saved_{false};
    };

    bool reclaimGuardEnabled() const noexcept
    {
        return reclaimGuardEnabled_.load(std::memory_order_acquire);
    }

    void setReclaimGuardEnabled(bool enabled) noexcept
    {
        reclaimGuardEnabled_.store(enabled, std::memory_order_release);
    }

    virtual bool isInNonReclaimableSection() const noexcept
    {
        return nonReclaimableSection_.load(std::memory_order_acquire);
    }

    // Late pool binding for operators built before the per-task pool is known (e.g. the SMJ
    // operator created in the factory ctor with a nullptr-pool sentinel). Idempotent: a
    // same-pool call is a no-op. A non-null pool that differs from an already-bound non-null
    // pool is a contract violation — the operator is task-scoped and silently rebinding would
    // free pool-accounted memory against the wrong pool; fail loudly instead (mirrors
    // JoinHashTableVariants::SetPool). A single Spark task owns a single pool, so the throw
    // never fires in normal flow but catches cross-task aliasing loudly.
    void SetPool(omniruntime::mem::OmniTaskMemoryPool *pool)
    {
        if (pool == nullptr || pool == pool_) {
            return;
        }
        if (pool_ != nullptr) {
            throw omniruntime::exception::OmniException("OPERATOR_RUNTIME_ERROR",
                "Operator::SetPool called with a different pool; operator is task-scoped");
        }
        pool_ = pool;
        // Rebind this operator's own ExecutionContext so executionContext-driven allocations
        // are accounted too. Safe for SMJ: its base ExecutionContext is unused between the
        // factory-ctor construction and this CreateOperator-time rebind.
        executionContext = std::make_unique<ExecutionContext>(4096, pool);
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

    /// True when this operator consumes a dynamically generated filter (TableScan).
    virtual bool canAddDynamicFilter() const
    {
        return false;
    }

    virtual void addDynamicFilter(uint32_t /*channel*/, ::common::FilterPtr /*filter*/) {}

    /// LookupJoin (and wrappers) expose build-side filters for Driver pushdown.
    virtual bool hasPendingDynamicFilters() const
    {
        return false;
    }

    virtual std::unordered_map<uint32_t, ::common::FilterPtr> getPendingDynamicFilters()
    {
        return {};
    }

    virtual void clearPendingDynamicFilters() {}

    /// Driver reports how many pending filters actually reached an operator
    /// with canAddDynamicFilter() (typically TableScan). Zero means the walk
    /// was blocked (e.g. ValueStream) and the join must keep probing.
    virtual void onDynamicFiltersPushed(size_t /*appliedCount*/) {}

    const std::vector<IdentityProjection> &identityProjections() const
    {
        return identityProjections_;
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
    // Per-task memory pool. nullptr on the no-pool (test) path; non-null when
    // constructed via Operator(pool).
    omniruntime::mem::OmniTaskMemoryPool *pool_ = nullptr;
    bool noMoreInput_{true};
    int32_t inputOperatorCnt_{0};
    std::vector<IdentityProjection> identityProjections_;
    std::atomic<bool> nonReclaimableSection_{false};
    // Enabled only while this operator is registered as a reclaim candidate.
    // Non-spill plans therefore avoid an atomic RMW around every operator call.
    std::atomic<bool> reclaimGuardEnabled_{false};

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
        stats_.spilledFiles = sortOpStats.spilledFiles;
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
