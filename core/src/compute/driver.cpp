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
 
#include "driver.h"
#include "codegen/time_util.h"
#include "util/debug.h"
#include "vector/vector_helper.h"
#include <memory>

namespace omniruntime::compute {
std::atomic_uint64_t BlockingState::numBlockdDrivers_{0};

BlockingState::BlockingState(
    std::shared_ptr<OmniDriver> driver,
    ContinueFuture &&future,
    omniruntime::op::Operator *op,
    BlockingReason reason)
    : driver_(std::move(driver)),
      future_(std::move(future)),
      operator_(op),
      reason_(reason)
{
    numBlockdDrivers_++;
}

vec::VectorBatch *OmniDriver::Next(ContinueFuture *future, StopReason *stopReason)
{
    auto self = shared_from_this();
    std::shared_ptr<BlockingState> blockingState;
    vec::VectorBatch *result = nullptr;
    *stopReason = RunInternal(self, blockingState, &result);

    if (blockingState != nullptr) {
        *future = blockingState->Future();
        return nullptr;
    }

    if (*stopReason == StopReason::kPause) {
        return nullptr;
    }

    return result;
}

void OmniDriver::close()
{
    if (closed_) {
        return;
    }
    updatePipelineStats();
    for (auto &op : operators_) {
        // Cascade release may have set some entries to nullptr.
        if (op != nullptr) {
            op->Close();
        }
        op = nullptr;
    }
    for (auto &factory : operatorFactories_) {
        delete factory;
    }
    closed_ = true;
}

// Call an Operator method. record silenced throws, but not a query
// terminating throw. Annotate exceptions with Operator info.
#define CALL_OPERATOR(call, operatorPtr, operatorId, operatorMethod)                                    \
        opCallStatus_.Start(operatorId, operatorMethod);                                                \
        call;                                                                                           \
        opCallStatus_.TimeSegmentStatistic(operatorPtr, operatorMethod);                                \
        opCallStatus_.Stop();                                                                           \

void OpCallStatus::Start(int32_t operatorId, const char* operatorMethod)
{
    opId = operatorId;
    method = operatorMethod;
    cpuTimeStartNs = ThreadCpuNanos();
}

void OpCallStatus::Stop()
{
    cpuTimeStartNs = 0;
}

CpuWallTiming OmniDriver::processLazyIoStats(op::Operator& op, const CpuWallTiming& timing)
{
    // If the source operator has been released (via cascade release), there is no
    // lazy I/O to account for — return timing unchanged.
    if (&op == operators_[0].get() || operators_[0] == nullptr) {
        return timing;
    }
    auto lockStats = op.stats();

    int64_t wallDelta = 0;
    uint64_t inputBytesDelta = 0;
    wallDelta = std::min<int64_t>(wallDelta, timing.wallNanos);
    lockStats = operators_[0]->stats();
    lockStats.getOutputTime.Add(CpuWallTiming{
        1, wallDelta, 0
    });
    lockStats.inputBytes += inputBytesDelta;
    lockStats.outputBytes += inputBytesDelta;
    return CpuWallTiming{
        1,
        timing.wallNanos - wallDelta,
        timing.cpuNanos - 0,
    };
}

void OpCallStatus::TimeSegmentStatistic(op::Operator* op, const char* operatorMethod) const
{
    const int64_t cpuTimeSegment = ThreadCpuNanos() - cpuTimeStartNs;
    std::string_view opMethod(operatorMethod);
    if (opMethod != kOpMethodAddInput && opMethod != kOpMethodGetOutput) {
        LogDebug("not input or output for operator");
        return;
    }
    auto &lockedStats = op->stats();
    if (opMethod == kOpMethodAddInput) {
        lockedStats.addInputTime.cpuNanos = cpuTimeSegment / static_cast<int64_t>(1e6);
        lockedStats.addInputTime.count = 1;
    } else if (opMethod == kOpMethodGetOutput) {
        lockedStats.getOutputTime.cpuNanos = cpuTimeSegment / static_cast<int64_t>(1e6);
        lockedStats.getOutputTime.count = 1;
    }
}

StopReason OmniDriver::RunInternal(
    std::shared_ptr<OmniDriver> &self,
    std::shared_ptr<BlockingState> &blockingState,
    vec::VectorBatch **result)
{
    try {
        const uint32_t numOperators = operators_.size();
        ContinueFuture future = OmniFuture::makeEmpty();
        for (;;) {
            for (int32_t i = numOperators - 1; i >= 0; --i) {
                if (shouldStop) {
                    return StopReason::kAtEnd;
                }

                // Skip operators already released by cascade release.
                if (operators_[i] == nullptr) {
                    continue;
                }

                auto *op = operators_[i].get();
                curOperatorId_ = i;

                blockingReason_ = op->IsBlocked(&future);
                if (blockingReason_ != BlockingReason::kNotBlocked) {
                    return BlockDriver(self, i, std::move(future), blockingState);
                }

                if (i < numOperators - 1) {
                    auto *nextOp = operators_[i + 1].get();

                    // The downstream operator has been released (e.g. an early-finish
                    // Limit that reached its row quota). The current operator may still
                    // hold residual output that cannot be delivered downstream. Drain
                    // and discard this output to advance the operator to isFinished(),
                    // otherwise it would never be released.
                    if (nextOp == nullptr) {
                        vec::VectorBatch *orphan = nullptr;
                        withDeltaCpuWallTimer(op, &OperatorStats::getOutputTime, [&]() {
                            CALL_OPERATOR(op->GetOutput(&orphan), op, curOperatorId_, kOpMethodGetOutput);
                        });
                        if (orphan != nullptr) {
                            VectorHelper::FreeVecBatch(orphan);
                            LogDebug("Orphan output from operator %d discarded (downstream released).", i);
                        }
                        if (op->isFinished()) {
                            ReleaseFinishedOperators(i);
                            break;
                        }
                        i += 1;
                        continue;
                    }

                    blockingReason_ = nextOp->IsBlocked(&future);
                    if (blockingReason_ != BlockingReason::kNotBlocked) {
                        return BlockDriver(self, i + 1, std::move(future), blockingState);
                    }

                    bool needsInput;
                    CALL_OPERATOR(needsInput = nextOp->needsInput(), nextOp, curOperatorId_ + 1, kOpMethodNeedsInput);
                    if (needsInput) {
                        uint64_t resultBytes = 0;
                        vec::VectorBatch *intermediateResult = nullptr;
                        withDeltaCpuWallTimer(op, &OperatorStats::getOutputTime, [&]() {
                            CALL_OPERATOR(op->GetOutput(&intermediateResult), op, curOperatorId_, kOpMethodGetOutput);
                            if (intermediateResult) {
                                resultBytes = intermediateResult->CalculateTotalSize();
                                {
                                    auto &lockedStats = op->stats();
                                    lockedStats.AddOutputVector(resultBytes, intermediateResult->GetVectorCount(), intermediateResult->GetRowCount());
                                }
                            }
                        });
                        if (intermediateResult != nullptr) {
                            withDeltaCpuWallTimer(nextOp, &OperatorStats::addInputTime, [&]() {
                                {
                                    auto &lockedStats = nextOp->stats();
                                    lockedStats.AddInputVector(resultBytes, intermediateResult->GetVectorCount(), intermediateResult->GetRowCount());
                                }
                                CALL_OPERATOR(nextOp->AddInput(intermediateResult), nextOp, curOperatorId_ + 1,
                                              kOpMethodAddInput);
                            });

                            // The next iteration will see if operators_[i + 1] has
                            // output now that it got input
                            i += 2;
                            continue;
                        } else {
                            blockingReason_ = op->IsBlocked(&future);
                            if (blockingReason_ != BlockingReason::kNotBlocked) {
                                return BlockDriver(self, i, std::move(future), blockingState);
                            }
                            if (op->isFinished()) {
                                nextOp->noMoreInput();

                                ReleaseFinishedOperators(i);
                                break;
                            }
                        }
                    }
                } else {
                    withDeltaCpuWallTimer(op, &OperatorStats::getOutputTime, [&]() {
                        CALL_OPERATOR(op->GetOutput(result), op, curOperatorId_, kOpMethodGetOutput);
                        if (*result != nullptr) {
                            {
                                auto &lockedStats = op->stats();
                                lockedStats.AddOutputVector((*result)->CalculateTotalSize(), (*result)->GetVectorCount(), (*result)->GetRowCount());
                            }
                        }
                    });
                    if (*result != nullptr  && !op->isFinished()) {
                        blockingReason_ = BlockingReason::kWaitForConsumer;
                        return StopReason::kBlock;
                    }

                    bool finished{false};
                    finished = op->isFinished();
                    if (finished) {
                        // For join / union，there is split to multi pipeline.
                        // When one pipeline finished, just close it.
                        close();
                        finished_ = true;
                        return StopReason::kAtEnd;
                    }
                }
            }
        }
    } catch (const std::exception &e) {
        throw std::runtime_error(e.what());
    }
}
#undef CALL_OPERATOR

StopReason OmniDriver::BlockDriver(
    const std::shared_ptr<OmniDriver> &self,
    size_t blockedOperatorId,
    ContinueFuture &&future,
    std::shared_ptr<BlockingState> &blockingState)
{
    auto *op = operators_[blockedOperatorId].get();
    blockedOperatorId_ = blockedOperatorId;
    blockingState = std::make_shared<BlockingState>(
        self, std::move(future), op, blockingReason_);
    return StopReason::kBlock;
}

template <typename Func>
void OmniDriver::withDeltaCpuWallTimer(op::Operator* op, TimingMemberPtr opTimingMember, Func&& opFunction)
{
    // If 'trackOperatorCpuUsage_' is true, create and initialize the timer object
    // to track cpu and wall time of the opFunction.
    if (!trackOperatorCpuUsage_) {
        opFunction();
        return;
    }

    // The delta CpuWallTiming object would be recorded to the corresponding
    // 'opTimingMember' upon destruction of the timer when withDeltaCpuWallTimer
    // ends. The timer is created on the stack to avoid heap allocation
    auto f = [op, opTimingMember, this](const CpuWallTiming& elapsedTime) {
        auto elapsedSelfTime = processLazyIoStats(*op, elapsedTime);
        (op->stats().*opTimingMember).Add(elapsedSelfTime);
    };
    DeltaCpuWallTimer<decltype(f)> timer(std::move(f));

    opFunction();
}

void OmniDriver::updatePipelineStats()
{
    for (auto& op : operators_) {
        // Cascade release may have set some entries to nullptr.
        if (op == nullptr) {
            continue;
        }
        auto opStatsCopy = op->stats(false);
        int32_t pipelineId = opStatsCopy.pipelineId;
        int32_t operatorId = opStatsCopy.operatorId;
        if (pipelineStats_.operatorStats.size() <= static_cast<size_t>(operatorId)) {
            pipelineStats_.operatorStats.resize(operatorId + 1);
        }
        pipelineStats_.operatorStats[operatorId].Add(opStatsCopy);
        pipelineStats_.pipelineId = pipelineId;
    }
}

void OmniDriver::CollectStatsBeforeClose(int32_t operatorIdx)
{
    auto &op = operators_[operatorIdx];
    if (op == nullptr) {
        return;
    }
    auto opStatsCopy = op->stats(false);
    int32_t pipelineId = opStatsCopy.pipelineId;
    int32_t operatorId = opStatsCopy.operatorId;
    if (pipelineStats_.operatorStats.size() <= static_cast<size_t>(operatorId)) {
        pipelineStats_.operatorStats.resize(operatorId + 1);
    }
    pipelineStats_.operatorStats[operatorId].Add(opStatsCopy);
    pipelineStats_.pipelineId = pipelineId;
}

void OmniDriver::ReleaseFinishedOperators(int32_t finishedIndex)
{
    auto *finishedOp = operators_[finishedIndex].get();
    if (finishedOp == nullptr) {
        return;
    }

    // Early-finish path: release the operator itself and propagate noMoreInput
    // upstream, but do NOT release upstream operators yet.
    //
    // noMoreInput signals "begin winding down"; isFinished signals "safe to free".
    // Between the two, an operator must finish draining its internal state
    // (e.g. a Sort emitting sorted pagesIndex, a Filter flushing projectedVecs).
    // Releasing upstream before that drain completes would discard query results.
    // Upstream is released later, once the orphan-drain path drives it to
    // isFinished and calls this function again.
    if (finishedOp->isEarlyFinish() && !finishedOp->hasNoMoreInput()) {
        CollectStatsBeforeClose(finishedIndex);
        finishedOp->Close();
        operators_[finishedIndex] = nullptr;
        LogDebug("Early-finish operator %d released, propagating noMoreInput upstream.", finishedIndex);

        for (int32_t j = finishedIndex - 1; j >= 0; --j) {
            if (operators_[j] != nullptr) {
                operators_[j]->noMoreInput();
                LogDebug("noMoreInput propagated to operator %d (early-finish cascade from %d).", j, finishedIndex);
            }
        }
        return;
    }

    // Normal path: release all finished operators in [0..finishedIndex].
    // Each entry is defensively checked with isFinished() so that an unexpected
    // state only degrades to "release fewer" (performance) rather than
    // "release too many" (crash/data corruption).
    for (int32_t j = finishedIndex; j >= 0; --j) {
        if (operators_[j] == nullptr) {
            continue;
        }
        if (!operators_[j]->isFinished()) {
            LogWarn("Operator %d not finished during cascade release from %d, skip (defensive).", j, finishedIndex);
            continue;
        }
        CollectStatsBeforeClose(j);
        operators_[j]->Close();
        operators_[j] = nullptr;
        LogDebug("Operator %d released early (cascade from finished operator %d).", j, finishedIndex);
    }
}
} // end of omniruntime