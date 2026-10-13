/* Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved. */
#ifndef OMNI_RUNTIME_SPILLABLE_OPERATOR_H
#define OMNI_RUNTIME_SPILLABLE_OPERATOR_H

#include <cstdint>

namespace omniruntime::op {
class Operator;

// A task-pool reclaim candidate. Implementations must return the real decrease
// in their pool's currentBytes(), rather than bytes written to spill storage.
class SpillableOperator {
public:
    virtual ~SpillableOperator() = default;
    virtual uint64_t reclaimableBytes() const = 0;
    virtual int64_t reclaim(int64_t targetBytes) = 0;
    virtual bool isInNonReclaimableSection() const noexcept = 0;
    virtual Operator *operatorBase() noexcept = 0;
    virtual const char *reclaimableName() const noexcept = 0;
};

} // namespace omniruntime::op
#endif // OMNI_RUNTIME_SPILLABLE_OPERATOR_H
