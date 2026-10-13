/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 * Description: Omni-owned task-pool accounting callback interface.
 */
#ifndef OMNI_RUNTIME_MEMORY_POOL_LISTENER_H
#define OMNI_RUNTIME_MEMORY_POOL_LISTENER_H

#include <cstdint>

namespace omniruntime::mem {

// Embedders implement this interface or provide an adapter to their own
// listener. The pool borrows it; it must outlive the pool, including teardown.
// No external engine class or vtable is redeclared by Omni.
class PoolListener {
public:
    virtual ~PoolListener() = default;
    // Positive deltas reserve memory; negative deltas release it. Exceptions
    // propagate to the pool's reservation rollback/repayment handling.
    virtual void allocationChanged(int64_t diff) = 0;
    virtual int64_t currentBytes() { return 0; }
    virtual int64_t peakBytes() { return 0; }

protected:
    PoolListener() = default;
};

} // namespace omniruntime::mem
#endif
