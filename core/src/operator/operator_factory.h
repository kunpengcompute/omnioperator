/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2023. All rights reserved.
 */

#ifndef __OMNI_OPERATOR_FACTORY_H__
#define __OMNI_OPERATOR_FACTORY_H__

#include <memory>
#include "operator.h"

namespace omniruntime {
namespace op {
class OperatorFactory {
public:
    OperatorFactory() = default;

    virtual ~OperatorFactory() = default;

    // Pool-bound `CreateOperator(pool)` is the canonical and only virtual entry.
    // Pure virtual — all subclasses MUST override. JNI (`createOperatorNative(jlong,
    // jlong)`) and all production call sites supply a real per-task pool decoded from
    // nmmHandle. Test/no-pool callers pass nullptr explicitly (Operator(nullptr) sentinel
    // path is reserved for tests/mocks).
    virtual omniruntime::op::Operator *CreateOperator(omniruntime::mem::OmniTaskMemoryPool *pool) = 0;

    virtual DataTypes *GetOutputDataTypes()
    {
        return nullptr;
    }

    const config::QueryConfig *GetQueryConfig() const
    {
        return &queryConfig_;
    }

    void SetQueryConfig(const config::QueryConfig &config)
    {
        queryConfig_ = config;
    }

    // An operator using cache-owned native data retains this guard through
    // operator Close(), even if the entry is retired during the task.
    void HoldLifetime(std::shared_ptr<void> guard)
    {
        lifetimeGuard_ = std::move(guard);
    }

protected:
    config::QueryConfig queryConfig_;
    std::shared_ptr<void> lifetimeGuard_;
};

using OperatorType = enum OperatorType {
    OMNI_FILTER_AND_PROJECT = 0,
    OMNI_PROJECT,
    OMNI_LIMIT,
    OMNI_DISTINCT_LIMIT,
    OMNI_SORT,
    OMNI_TOPN,
    OMNI_AGGREGATION,
    OMNI_HASH_AGGREGATION,
    OMNI_WINDOW,
    OMNI_HASH_BUILDER,
    OMNI_LOOKUP_JOIN,
    OMNI_LOOKUP_OUTER_JOIN,
    OMNI_SMJ_BUFFER,
    OMNI_SMJ_STREAM,
    OMNI_PARTITIONED_OUTPUT,
    OMNI_UNION,
    OMNI_FUSION
};
}
}

#endif // __OMNI_OPERATOR_FACTORY_H__
