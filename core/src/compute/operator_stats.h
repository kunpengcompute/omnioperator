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

#ifndef OPERATOR_STATS_H
#define OPERATOR_STATS_H

#pragma once

#include <algorithm>
#include <functional>
#include <optional>
#include <string>
#include <sstream>

#include "cpuWall_timer.h"
#include "metrics/metrics_config.h"

namespace omniruntime::compute {
using namespace std;
using PlanNodeId = std::string;

struct OperatorStats {
    /// Initial ordinal position in the operator's pipeline.
    int32_t operatorId{0};
    int32_t pipelineId{0};
    PlanNodeId planNodeId;

    /// Some operators perform the logic describe in multiple consecutive plan
    /// nodes. For example, FilterProject operator maps to Filter node followed by
    /// Project node. In this case, runtime stats are collected for the combined
    /// operator and attached to the "main" plan node ID chosen by the operator.
    /// (Project node ID in case of FilterProject operator.) The operator can then
    /// provide a function to split the stats among all plan nodes that are being
    /// represented. For example, FilterProject would split the stats but moving
    /// cardinality reduction to Filter and making Project cardinality neutral.
    using StatsSplitter = std::function<std::vector<OperatorStats>(
        const OperatorStats& combinedStats)>;

    std::optional<StatsSplitter> statsSplitter;

    /// Name for reporting. We use Presto compatible names set at
    /// construction of the Operator where applicable.
    std::string operatorType;

    /// Number of splits (or chunks of work). Split can be a part of data file to
    /// read.
    int64_t numSplits{0};

    CpuWallTiming isBlockedTiming;

    /// For Scan
    uint64_t rawInputBytes{0};
    uint64_t rawInputRows{0};
    /// Wall time spent in dataSource_->next (accumulated); surfaced as total scan time.
    uint64_t totalScanTimeNanos{0};

    /// Bytes of input in terms of retained size of input vectors.
    uint64_t inputRows{0};
    uint64_t inputBytes{0};
    uint64_t numInputVecBatches{0};

    CpuWallTiming addInputTime;

    /// For Agg/Sort/Window
    // CpuWallTiming spilledTiming;

    /// Bytes of output in terms of retained size of vectors.
    uint64_t outputBytes{0};
    uint64_t outputRows{0};
    uint64_t numOutputVecBatches{0};

    CpuWallTiming getOutputTime;

    // Total bytes written to file for spilling.
    uint64_t spilledBytes{0};

    // Total rows written for spilling.
    uint64_t spilledRows{0};

    // Total spilled partitions.
    uint32_t spilledPartitions{0};

    // Total current spilled files.
    uint32_t spilledFiles{0};

    // Largest working memory one spill held outside the task pool (spill write batch plus
    // operator scratch). Reported only; it is neither billed to Spark nor limited.
    uint64_t spillScratchPeakBytes{0};

    CpuWallTiming finishTiming;

    int numDrivers = 0;

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

    OperatorStats() = default;

    OperatorStats(
        int32_t _operatorId,
        int32_t _pipelineId,
        PlanNodeId _planNodeId,
        std::string _operatorType)
        : operatorId(_operatorId),
          pipelineId(_pipelineId),
          planNodeId(std::move(_planNodeId)),
          operatorType(std::move(_operatorType)) {}

    void setStatSplitter(StatsSplitter splitter)
    {
        statsSplitter = std::move(splitter);
    }

    void AddInputVector(uint64_t bytes, uint64_t inputVecBatches, uint64_t rowCount)
    {
        inputBytes += bytes;
        numInputVecBatches += 1;
        inputRows += rowCount;
    }

    void AddOutputVector(uint64_t bytes, uint64_t outputVecBatches, uint64_t rowCount)
    {
        outputBytes += bytes;
        numOutputVecBatches += 1;
        outputRows += rowCount;
    }

    void AddSpilledBytes(uint64_t bytes, uint64_t rowCount, uint64_t cpuNanos)
    {
        // spilledTiming.cpuNanos += cpuNanos;
        spilledBytes += bytes;
        spilledRows += rowCount;
    }

    void HashJoinOperator(const OperatorStats& stats)
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

    void Add(const OperatorStats& other)
    {
        HashJoinOperator(other);

        operatorType = other.operatorType;
        planNodeId = other.planNodeId;
        numSplits += other.numSplits;
        rawInputBytes += other.rawInputBytes;
        rawInputRows += other.rawInputRows;
        totalScanTimeNanos += other.totalScanTimeNanos;

        addInputTime.Add(other.addInputTime);
        inputBytes += other.inputBytes;
        inputRows += other.inputRows;
        numInputVecBatches += other.numInputVecBatches;

        getOutputTime.Add(other.getOutputTime);
        outputBytes += other.outputBytes;
        numOutputVecBatches += other.numOutputVecBatches;
        outputRows += other.outputRows;

        isBlockedTiming.Add(other.isBlockedTiming);

        numDrivers += other.numDrivers;
        spilledBytes += other.spilledBytes;
        spilledRows += other.spilledRows;
        spilledPartitions += other.spilledPartitions;
        spilledFiles += other.spilledFiles;
        spillScratchPeakBytes = std::max(spillScratchPeakBytes, other.spillScratchPeakBytes);

        finishTiming.Add(other.finishTiming);
    }

    void Clear()
    {
        numSplits = 0;
        rawInputBytes = 0;
        rawInputRows = 0;
        totalScanTimeNanos = 0;

        addInputTime.Clear();
        inputBytes = 0;
        inputRows = 0;
        numInputVecBatches = 0;

        getOutputTime.Clear();
        outputBytes = 0;
        outputRows = 0;
        numOutputVecBatches = 0;

        numDrivers = 0;
        spilledBytes = 0;
        spilledRows = 0;
        spilledPartitions = 0;
        spilledFiles = 0;
        spillScratchPeakBytes = 0;

        finishTiming.Clear();
    }
};
} // omniruntime::compute
#endif
