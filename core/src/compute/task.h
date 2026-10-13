/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef __TASK_H__
#define __TASK_H__
 
#include <vector>
#include <memory>
#include <string>
#include <functional>
#include <exception>

#include "compute/driver.h"
#include "compute/task_stats.h"
#include "vector/vector_batch.h"
#include "operator/operator.h"
#include "plannode/planFragment.h"
#include "type/data_type.h"
#include "connectors/Split.h"

using Split = omniruntime::connector::Split;
using SplitsStore = omniruntime::connector::SplitsStore;

namespace omniruntime {
namespace compute {

class OmniDriver;

class OmniTask {
public:
    // pool is the per-task OmniTaskMemoryPool, decoded from the JNI nmmHandle
    // on the gluten side and threaded through here so
    // LocalPlanner::plan(..., pool) → createOperator(factory, planNode, pool) →
    // factory->CreateOperator(pool) lands a real pool inside each Operator's
    // ExecutionContext. Default-nullable so C++ unit-test scaffolds that
    // construct OmniTask without a JNI handle still compile (they follow the
    // no-pool path inside createOperator).
    OmniTask(const PlanFragment& planFragment, config::QueryConfig&& queryConfig,
        omniruntime::mem::OmniTaskMemoryPool* pool = nullptr)
        : planFragment_(planFragment), queryConfig_(std::move(queryConfig)), pool_(pool)
    {
        taskStats_ = TaskStats();
    }

    ~OmniTask()
    {
        for (auto& driver : drivers_) {
            if (driver) {
                driver->shouldStop = true;
                driver->close();
            }
        }
    }

    vec::VectorBatch* Next(ContinueFuture* future = nullptr);

    /// Returns Task Stats by copy as other threads might be updating the
    /// structure.
    TaskStats GetTaskStats() const;

    void addSplit(const omniruntime::PlanNodeId &planNodeId, Split &&split);

    void noMoreSplits(const omniruntime::PlanNodeId &planNodeId);

    std::shared_ptr <SplitsStore> getSplitsStoreLocked(const omniruntime::PlanNodeId &planNodeId);

    std::shared_ptr <SplitsStore> getOrcreateSplitsStoreLocked(const omniruntime::PlanNodeId &planNodeId);

    void addSplitLocked(SplitsStore &splitsStore, Split &&split);

    void addSplitToStoreLocked(SplitsStore &splitsStore, Split &&split);

    std::vector <omniruntime::PlanNodeId> getScanNodeIds(std::shared_ptr<const PlanNode> &planNode);

    omniruntime::PlanNodeId getScanNodeId(std::shared_ptr<const PlanNode> &planNode);

private:
    std::vector<std::shared_ptr<OmniDriver>> drivers_;
    PlanFragment planFragment_;
    OperatorConfig operatorConfig_;
    TaskStats taskStats_;
    const config::QueryConfig queryConfig_;

    std::unordered_map<omniruntime::PlanNodeId, std::shared_ptr<SplitsStore>> splitsStore_;

    // Non-owning per-task pool; lifetime owned by
    // gluten::OmniMemoryManager on the Java/JNI side (TaskResources priority=0
    // releases after Runtime priority=20, so the pool outlives this OmniTask).
    omniruntime::mem::OmniTaskMemoryPool* pool_ = nullptr;
};
} // end of compute
} // end of omniruntime
#endif