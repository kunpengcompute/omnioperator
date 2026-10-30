/*
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
 
#pragma once

#include <cstddef>
#include <memory>

#include "cpuWall_timer.h"
#include "operator_stats.h"
#include "task_stats.h"

namespace omniruntime::compute {
/// Aggregated runtime statistics per plan node.
///
/// Runtime statistics are collected on a per-operator instance basis. There can
/// be multiple operator types and multiple instances of each operator type that
/// correspond to a given plan node. For example, ProjectNode corresponds to
/// a single operator type, FilterProject, but HashJoinNode corresponds to two
/// operator types, HashProbe and HashBuild. Each operator type may have
/// different runtime parallelism, e.g. there can be multiple instances of each
/// operator type. Plan node statistics are calculated by adding up
/// operator-level statistics for all corresponding operator instances.
struct PlanNodeStats {
    explicit PlanNodeStats() = default;

    PlanNodeStats(const PlanNodeStats&) = delete;
    PlanNodeStats& operator=(const PlanNodeStats&) = delete;

    PlanNodeStats(PlanNodeStats&&) = default;
    PlanNodeStats& operator=(PlanNodeStats&&) = default;

    PlanNodeStats& operator+=(const struct omniruntime::compute::PlanNodeStats& another);

    /// Sum of input rows for all corresponding operators. Useful primarily for
    /// leaf plan nodes or plan nodes that correspond to a single operator type.
    uint64_t inputRows{0};
    size_t numInputVecBatches{0};
    uint64_t inputBytes{0};
    std::string operatorType;
    /// Sum of raw input rows for all corresponding operators. Applies primarily
    /// to TableScan operator which reports rows before pushed down filter as raw
    /// input.
    uint64_t rawInputRows{0};
    uint64_t rawInputBytes{0};
    uint64_t totalScanWallNanos{0};

    /// Sum of output rows for all corresponding operators. When
    /// plan node corresponds to multiple operator types, operators of only one of
    /// these types report non-zero output rows.
    uint64_t outputRows{0};
    size_t numOutputVecBatches{0};
    uint64_t outputBytes{0};

    // Sum of CPU, scheduled and wall times for isBLocked call for all
    // corresponding operators.
    CpuWallTiming isBlockedTiming;

    // Sum of CPU, scheduled and wall times for addInput call for all
    // corresponding operators.
    CpuWallTiming addInputTime;

    // Sum of CPU, scheduled and wall times for noMoreInput call for all
    // corresponding operators.
    CpuWallTiming finishTiming;

    // Sum of CPU, scheduled and wall times for getOutput call for all
    // corresponding operators.
    CpuWallTiming getOutputTime;

    /// Sum of CPU, scheduled and wall times for all corresponding operators. For
    /// each operator, timing of addInput, getOutput and finish calls are added
    /// up.
    CpuWallTiming cpuWallTiming;

    /// Sum of CPU, scheduled and wall times spent on background activities
    /// (activities that are not running on driver threads) for all corresponding
    /// operators.
    CpuWallTiming backgroundTiming;

    /// Sum of blocked wall time for all corresponding operators.
    uint64_t blockedWallNanos{0};

    /// Max of peak memory usage for all corresponding operators. Assumes that all
    /// operator instances were running concurrently.
    uint64_t peakMemoryBytes{0};

    uint64_t numMemoryAllocations{0};

    uint64_t physicalWrittenBytes{0};

    /// Breakdown of stats by operator type.
    std::unordered_map<std::string, std::unique_ptr<PlanNodeStats>> operatorStats;

    /// Number of drivers that executed the pipeline.
    int numDrivers{0};

    /// Number of total splits.
    int numSplits{0};

    /// Total bytes written for spilling.
    uint64_t spilledBytes{0};
    uint64_t spilledRows{0};
    uint32_t spilledPartitions{0};
    uint32_t spilledFiles{0};
    /// Largest spill working memory held outside the task pool, across operators.
    uint64_t spillScratchPeakBytes{0};

    // For BHJ/SHJ
    uint64_t buildInputRows{0};
    uint64_t buildNumInputVecBatches{0};
    CpuWallTiming buildAddInputTime;
    CpuWallTiming buildGetOutputTime;

    uint64_t lookupInputRows{0};
    uint64_t lookupNumInputVecBatches{0};
    uint64_t lookupOutputRows{0};
    uint64_t lookupNumOutputVecBatches{0};
    CpuWallTiming lookupAddInputTime;
    CpuWallTiming lookupGetOutputTime;

    /// Add stats for a single operator instance.
    void Add(const OperatorStats& stats);

    std::string ToString(
        bool includeInputStats = false) const;

    bool IsMultiOperatorTypeNode() const
    {
        return operatorStats.size() > 1;
    }

private:
    void HashJoinOperator(const OperatorStats& stats);
    void AddTotals(const OperatorStats& stats);
};

std::unordered_map<std::string, PlanNodeStats> ToPlanStats(
    const TaskStats& taskStats);

using PlanNodeAnnotation =
    std::function<std::string(const PlanNodeId& id)>;
} // namespace omniruntime:exec