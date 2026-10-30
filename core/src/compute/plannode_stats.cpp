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
 
#include "plannode_stats.h"
#include "compute/task.h"
#include "plannode/planNode.h"
#include "util/format.h"
#include "metrics/metrics_config.h"
#include <algorithm>
#include <unordered_map>

namespace omniruntime::compute {
PlanNodeStats& PlanNodeStats::operator+=(const PlanNodeStats& another)
{
    inputRows += another.inputRows;
    inputBytes += another.inputBytes;
    numInputVecBatches += another.numInputVecBatches;

    rawInputRows += another.inputRows;
    rawInputBytes += another.rawInputBytes;

    outputRows += another.outputRows;
    outputBytes += another.outputBytes;
    numOutputVecBatches += another.numOutputVecBatches;

    isBlockedTiming.Add(another.isBlockedTiming);
    addInputTime.Add(another.addInputTime);
    getOutputTime.Add(another.getOutputTime);
    finishTiming.Add(another.finishTiming);
    cpuWallTiming.Add(another.addInputTime);
    cpuWallTiming.Add(another.getOutputTime);
    cpuWallTiming.Add(another.finishTiming);
    cpuWallTiming.Add(another.isBlockedTiming);

    backgroundTiming.Add(another.backgroundTiming);

    blockedWallNanos += another.blockedWallNanos;

    physicalWrittenBytes += another.physicalWrittenBytes;
    totalScanWallNanos += another.totalScanWallNanos;

    // Populating number of drivers for plan nodes with multiple operators is not
    // useful. Each operator could have been executed in different pipelines with
    // different number of drivers.
    if (!IsMultiOperatorTypeNode()) {
        numDrivers += another.numDrivers;
    } else {
        numDrivers = 0;
    }

    numSplits += another.numSplits;

    spilledBytes += another.spilledBytes;
    spilledRows += another.spilledRows;
    spilledPartitions += another.spilledPartitions;
    spilledFiles += another.spilledFiles;
    spillScratchPeakBytes = std::max(spillScratchPeakBytes, another.spillScratchPeakBytes);

    return *this;
}

// Returns true if an operator is a hash join operator given 'operatorType'.
void PlanNodeStats::HashJoinOperator(const OperatorStats& stats)
{
    const std::string& opType = stats.operatorType;

    if (opType == opNameForHashBuilder) {
        buildInputRows += stats.inputRows;
        buildAddInputTime.Add(stats.addInputTime);
        buildGetOutputTime.Add(stats.getOutputTime);
        buildNumInputVecBatches += stats.numInputVecBatches;
    }

    if (opType == opNameForLookUpJoin) {
        lookupInputRows += stats.inputRows;
        lookupOutputRows += stats.outputRows;
        lookupAddInputTime.Add(stats.addInputTime);
        lookupGetOutputTime.Add(stats.getOutputTime);
        lookupNumInputVecBatches += stats.numInputVecBatches;
        lookupNumOutputVecBatches += stats.numOutputVecBatches;
    }
}


void PlanNodeStats::Add(const OperatorStats& stats)
{
    auto it = operatorStats.find(stats.operatorType);
    if (it != operatorStats.end()) {
        it->second->AddTotals(stats);
    } else {
        auto opStats = std::make_unique<PlanNodeStats>();
        opStats->AddTotals(stats);
        operatorStats.emplace(stats.operatorType, std::move(opStats));
    }
    AddTotals(stats);
}

void PlanNodeStats::AddTotals(const OperatorStats& stats)
{
    inputRows += stats.inputRows;
    inputBytes += stats.inputBytes;
    numInputVecBatches += stats.numInputVecBatches;

    rawInputRows += stats.rawInputRows;
    rawInputBytes += stats.rawInputBytes;
    totalScanWallNanos += stats.totalScanTimeNanos;

    outputRows += stats.outputRows;
    outputBytes += stats.outputBytes;
    numOutputVecBatches += stats.numOutputVecBatches;

    isBlockedTiming.Add(stats.isBlockedTiming);
    addInputTime.Add(stats.addInputTime);
    getOutputTime.Add(stats.getOutputTime);
    finishTiming.Add(stats.finishTiming);
    cpuWallTiming.Add(stats.addInputTime);
    cpuWallTiming.Add(stats.getOutputTime);
    cpuWallTiming.Add(stats.finishTiming);
    cpuWallTiming.Add(stats.isBlockedTiming);

    // Populating number of drivers for plan nodes with multiple operators is not
    // useful. Each operator could have been executed in different pipelines with
    // different number of drivers.
    if (!IsMultiOperatorTypeNode()) {
        numDrivers += stats.numDrivers;
    } else {
        numDrivers = 0;
    }

    numSplits += stats.numSplits;

    spilledBytes += stats.spilledBytes;
    spilledRows += stats.spilledRows;
    spilledPartitions += stats.spilledPartitions;
    spilledFiles += stats.spilledFiles;
    spillScratchPeakBytes = std::max(spillScratchPeakBytes, stats.spillScratchPeakBytes);

    HashJoinOperator(stats);
}

void appendOperatorStats(
    const OperatorStats& stats,
    std::unordered_map<std::string, PlanNodeStats>& planStats)
{
    const auto& planNodeId = stats.planNodeId;
    auto it = planStats.find(planNodeId);
    if (it != planStats.end()) {
        it->second.Add(stats);
    } else {
        PlanNodeStats nodeStats;
        nodeStats.Add(stats);
        planStats.emplace(planNodeId, std::move(nodeStats));
    }
}

std::unordered_map<std::string, PlanNodeStats> ToPlanStats(
    const TaskStats& taskStats)
{
    std::unordered_map<PlanNodeId, PlanNodeStats> planStats;

    for (const auto& pipelineStats : taskStats.pipelineStats) {
        for (const auto& opStats : pipelineStats.operatorStats) {
            if (opStats.statsSplitter.has_value()) {
                const auto& multiNodeStats = opStats.statsSplitter.value()(opStats);
                for (const auto& stats : multiNodeStats) {
                    appendOperatorStats(stats, planStats);
                }
            } else {
                appendOperatorStats(opStats, planStats);
            }
        }
    }
    return planStats;
}

std::string PlanNodeStats::ToString(
    bool includeInputStats) const
{
    std::stringstream out;
    if (includeInputStats) {
        out << "Input: " << inputRows << " rows (" << inputBytes
            << ", " << numInputVecBatches << " batches), ";
        if ((rawInputRows > 0) && (rawInputRows != inputRows)) {
            out << "Raw Input: " << rawInputRows << " rows ("
                << rawInputBytes << "), ";
        }
    }
    out << "Output: " << outputRows << " rows (" << outputBytes
        << ", " << numOutputVecBatches << " batches)";
    if (physicalWrittenBytes > 0) {
        out << ", Physical written output: " << physicalWrittenBytes;
    }
    out << ", Cpu time: " << cpuWallTiming.cpuNanos
        << ", Wall time: " << cpuWallTiming.wallNanos
        << ", Blocked wall time: " << blockedWallNanos
        << ", Peak memory: " << peakMemoryBytes
        << ", Memory allocations: " << numMemoryAllocations
        << ", Threads: " << numDrivers
        << ", Splits: " << numSplits
        <<", Spilled: " << spilledRows << " rows ("
            << spilledBytes << ", " << spilledFiles << " files, scratch peak " << spillScratchPeakBytes << ")";
    out << ", CPU breakdown: B/I/O/F "
        << Format(
            "({}/{}/{}/{})", isBlockedTiming.cpuNanos, addInputTime.cpuNanos,
            getOutputTime.cpuNanos, finishTiming.cpuNanos);
    return out.str();
}

}