/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "task.h"
#include "local_planner.h"
#include "codegen/time_util.h"
#include "task_stats.h"

namespace omniruntime::compute {
 
vec::VectorBatch* OmniTask::Next(ContinueFuture* future)
{
    if (drivers_.empty()) {
        taskStats_.executionStartTimeMs = static_cast<uint64_t>(ThreadCpuNanos());
        // Thread the per-task pool to LocalPlanner so the
        // Operator(pool) → ExecutionContext(4096, pool) chain binds to a real
        // pool. Without this, pool=nullptr fell through to codegen .so's
        // CreateFlatVector(pool, ...) which dereferenced pool via OmniTaskMemoryPool::
        // getBoundAllocator() → SIGSEGV under the gluten JNI path.
        LocalPlanner::plan(
            planFragment_, &drivers_, queryConfig_,
            getSplitsStoreLocked(getScanNodeId(planFragment_.planNode)),
            pool_);
        std::reverse(drivers_.begin(), drivers_.end());
    }
    const auto numDrivers = drivers_.size();
    auto futures = OmniFuture::createValidFutures(numDrivers);
    for (;;) {
        int runableDrivers = 0;
        for (auto i = 0; i < numDrivers; ++i) {
            if (drivers_[i]->isFinished()) {
                // This driver has finished processing.
                continue;
            }
 
            ++runableDrivers;
 
            ContinueFuture driverFuture = OmniFuture::makeEmpty();
            StopReason stopReason = StopReason::kNone;
            auto result = drivers_[i]->Next(&driverFuture, &stopReason);
            if (result) {
                return result;
            }
 
            futures[i] = std::move(driverFuture);
        }
 
        if (runableDrivers == 0) {
            return nullptr;
        }
    }
}

TaskStats OmniTask::GetTaskStats() const
{
    // 'taskStats_' contains task stats plus stats for the completed drivers
    // (their operators).
    TaskStats taskStats = taskStats_;

    taskStats.numTotalDrivers = drivers_.size();
    LogDebug("total driver num is %d", taskStats_.numTotalDrivers);
    // Add stats of the drivers (their operators) that are still running.
    for (const auto& driver : drivers_) {
        // Driver can be null.
        if (driver == nullptr) {
            ++taskStats.numCompletedDrivers;
            continue;
        }
        auto pipelineStats = driver->pipelineStats();
        taskStats.pipelineStats.emplace_back(pipelineStats);
    }
    return taskStats;
}

void OmniTask::addSplit(const omniruntime::PlanNodeId& planNodeId, Split&& split)
{
    std::shared_ptr<SplitsStore> splitsStorePtr = getOrcreateSplitsStoreLocked(planNodeId);
    addSplitLocked(*splitsStorePtr, std::move(split));
}

std::shared_ptr<SplitsStore> OmniTask::getSplitsStoreLocked(
    const omniruntime::PlanNodeId& planNodeId)
{
    auto it = splitsStore_.find(planNodeId);
    return (it != splitsStore_.end()) ? it->second : nullptr;
}

std::shared_ptr<SplitsStore> OmniTask::getOrcreateSplitsStoreLocked(const omniruntime::PlanNodeId& planNodeId)
{
    std::shared_ptr <SplitsStore> existing = getSplitsStoreLocked(planNodeId);
    if (existing != nullptr) {
        return existing;
    }
    auto [it, inserted] = splitsStore_.insert({planNodeId, std::make_shared<SplitsStore>()});
    return it->second;
}

void OmniTask::addSplitLocked(SplitsStore& splitsStore, Split&& split)
{
    addSplitToStoreLocked(splitsStore, std::move(split));
}

void OmniTask::addSplitToStoreLocked(SplitsStore& splitsStore, Split&& split)
{
    splitsStore.splits.push_back(std::move(split));
}

std::vector<omniruntime::PlanNodeId> OmniTask::getScanNodeIds(std::shared_ptr<const PlanNode>& planNode)
{
    std::vector <omniruntime::PlanNodeId> ids;

    if (!planNode) {
        return ids;
    }

    if (std::dynamic_pointer_cast<const TableScanNode>(planNode)) {
        ids.push_back(planNode->Id());
        return ids;
    }

    auto sources = planNode->Sources();
    for (auto &source: sources) {
        auto childIds = getScanNodeIds(source);
        ids.insert(ids.end(), childIds.begin(), childIds.end());
    }

    return ids;
}

omniruntime::PlanNodeId OmniTask::getScanNodeId(std::shared_ptr<const PlanNode>& planNode)
{
    auto ids = getScanNodeIds(planNode);
    if (ids.empty()) {
        return planNode->Id();
    }
    return ids[0];
}

void OmniTask::noMoreSplits(const omniruntime::PlanNodeId& planNodeId)
{
    auto it = splitsStore_.find(planNodeId);
    if (it != splitsStore_.end() && it->second) {
        it->second->noMoreSplits = true;
    }
}

} // end of omniruntime