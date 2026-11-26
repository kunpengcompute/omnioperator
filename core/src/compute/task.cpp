/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
 * KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 */

#include "task.h"
#include "local_planner.h"
#include "codegen/time_util.h"

namespace omniruntime::compute {

vec::VectorBatch *OmniTask::Next(ContinueFuture *future)
{
    if (drivers_.empty()) {
        taskStats_.executionStartTimeMs = static_cast<uint64_t>(ThreadCpuNanos());
        LocalPlanner::plan(planFragment_, &drivers_, &operatorFactories_, queryConfig_);
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
    LogDebug("total driver num is %d", taskStats.numTotalDrivers);
    // Add stats of the drivers (their operators) that are still running.
    for (const auto &driver : drivers_) {
        // Driver can be null.
        if (driver == nullptr) {
            ++taskStats.numCompletedDrivers;
            continue;
        }
        auto operators = driver->operators();
        for (auto &op : *operators) {
            auto opStatsCopy = op->stats(false);
            int32_t pipelineId = opStatsCopy.pipelineId;
            int32_t operatorId = opStatsCopy.operatorId;
            PlanNodeId planNodeId = opStatsCopy.planNodeId;
            if (taskStats.pipelineStats.size() <= static_cast<size_t>(pipelineId)) {
                taskStats.pipelineStats.resize(pipelineId + 1);
            }
            if (taskStats.pipelineStats[pipelineId].operatorStats.size() <= static_cast<size_t>(operatorId)) {
                taskStats.pipelineStats[pipelineId].operatorStats.resize(operatorId + 1);
            }
            taskStats.pipelineStats[pipelineId].operatorStats[operatorId].Add(opStatsCopy);
        }
    }
    return taskStats;
}
} // namespace omniruntime::compute
