/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "common.h"

#include <iostream>

#include "vector_util.h"
#include "util/perf_util.h"

using namespace benchmark;
using namespace omniruntime::op;
using namespace omniruntime::vec;

namespace om_benchmark {
std::string ArgToS(bool arg) { return arg ? "true" : "false"; }
std::string ArgToS(int32_t arg) { return std::to_string(arg); }
std::string ArgToS(int64_t arg) { return std::to_string(arg); }
std::string ArgToS(double arg) { return std::to_string(arg); }
std::string ArgToS(std::string arg) { return arg; }

std::string BasicArg::GetValueString(const benchmark::State &state) { return valueStr.at(state.range(index)); }

std::vector<std::vector<int64_t>> BaseOmniFixture::ArgProducts()
{
    std::vector<std::vector<int64_t>> args;
    for (auto &i : argList) {
        args.push_back(i->valueIndex);
    }
    return args;
}

void BaseOmniFixture::SetUp(State &state)
{
    std::string label;
    for (unsigned int i = 0; i < argList.size(); ++i) {
        label += (argList[i]->argName + ":" + argList[i]->GetValueString(state));
        if (i != argList.size() - 1) {
            label += "|";
        }
    }
    state.SetLabel(label);
}

void BaseOperatorFixture::SetUp(benchmark::State &state)
{
    BaseOmniFixture::SetUp(state);
    operatorFactory = MessageWhenSkip(state).empty() ? createOperatorFactory(state) : nullptr;
    state.counters["addInput/ms"] = Counter(0, benchmark::Counter::kAvgIterations);
    state.counters["getOutput/ms"] = Counter(0, benchmark::Counter::kAvgIterations);
    state.counters["addInput InstCount"] = Counter(0, benchmark::Counter::kAvgIterations);
    state.counters["addInput CacheMisses"] = Counter(0, benchmark::Counter::kAvgIterations);
    state.counters["getOutput InstCount"] = Counter(0, benchmark::Counter::kAvgIterations);
    state.counters["getOutput CacheMisses"] = Counter(0, benchmark::Counter::kAvgIterations);
    state.counters["elapsed/ms"] = Counter(0, benchmark::Counter::kAvgIterations);
}

void BaseOperatorFixture::TearDown(benchmark::State &state)
{
    if (operatorFactory != nullptr) {
        delete operatorFactory;
        operatorFactory = nullptr;
    }
}

void BaseOperatorFixture::RunDefaultBenchmark(benchmark::State &state)
{
    auto skipMessage = MessageWhenSkip(state);
    if (!skipMessage.empty() || operatorFactory == nullptr) {
        state.SetIterationTime(std::numeric_limits<double>::infinity());
        state.SetLabel(skipMessage);
        return;
    }
    for (__attribute__((unused)) auto _ : state) {
        std::vector<VectorBatchSupplier> vvb = createVecBatch(state);
        Operator *op = operatorFactory->CreateOperator();
        VectorBatch *outputVecBatch = nullptr;
        std::chrono::duration<double> addInputDuration(0);
        std::chrono::duration<double> getOutputDuration(0);
        auto *perfUtil = new PerfUtil();
        perfUtil->Init();

        for (const auto &vbSupplier : vvb) {
            auto vb = vbSupplier();
            perfUtil->Reset();
            perfUtil->Start();
            auto start = std::chrono::high_resolution_clock::now();
            op->AddInput(vb);
            addInputDuration += std::chrono::duration_cast<std::chrono::duration<double>>(
                std::chrono::high_resolution_clock::now() - start);

            perfUtil->Stop();
            auto dataMap = perfUtil->GetData();
            state.counters["addInput InstCount"] += dataMap["Instructions"];
            state.counters["addInput CacheMisses"] += dataMap["CacheMisses"];

            if (GetOutputStrategy() == AFTER_EACH_INPUT_FINISHED) {
                perfUtil->Reset();
                perfUtil->Start();
                auto outputStart = std::chrono::high_resolution_clock::now();
                while (op->GetStatus() != OMNI_STATUS_FINISHED) {
                    op->GetOutput(&outputVecBatch);
                    if (outputVecBatch != nullptr) {
                        VectorHelper::FreeVecBatch(outputVecBatch);
                        outputVecBatch = nullptr;
                    } else {
                        break;
                    }
                }
                getOutputDuration += std::chrono::duration_cast<std::chrono::duration<double>>(
                    std::chrono::high_resolution_clock::now() - outputStart);

                perfUtil->Stop();
                auto dataMap2 = perfUtil->GetData();
                state.counters["getOutput InstCount"] += dataMap2["Instructions"];
                state.counters["getOutput CacheMisses"] += dataMap2["CacheMisses"];
            }
        }

        if (GetOutputStrategy() == AFTER_ALL_INPUT_FINISHED) {
            perfUtil->Reset();
            perfUtil->Start();

            auto outputStart = std::chrono::high_resolution_clock::now();

            while (op->GetStatus() != OMNI_STATUS_FINISHED) {
                op->GetOutput(&outputVecBatch);
                if (outputVecBatch != nullptr) {
                    VectorHelper::FreeVecBatch(outputVecBatch);
                    outputVecBatch = nullptr;
                } else {
                    break;
                }
            }

            getOutputDuration += std::chrono::duration_cast<std::chrono::duration<double>>(
                std::chrono::high_resolution_clock::now() - outputStart);

            perfUtil->Stop();
            auto dataMap = perfUtil->GetData();
            state.counters["getOutput InstCount"] += dataMap["Instructions"];
            state.counters["getOutput CacheMisses"] += dataMap["CacheMisses"];
        }

        delete perfUtil;
        Operator::DeleteOperator(op);
        state.SetIterationTime(addInputDuration.count() + getOutputDuration.count());
        int timeRatio = 1000;
        state.counters["addInput/ms"] += addInputDuration.count() * timeRatio;
        state.counters["getOutput/ms"] += getOutputDuration.count() * timeRatio;
        state.counters["elapsed/ms"] += (addInputDuration.count() + getOutputDuration.count()) * timeRatio;
    }
}

void BaseOperatorLargeFixture::SetUp(benchmark::State &state)
{
    BaseOmniFixture::SetUp(state);
    operatorFactory = MessageWhenSkip(state).empty() ? createOperatorFactory(state) : nullptr;
    state.counters["addInput/ms"] = Counter(0, benchmark::Counter::kAvgIterations);
    state.counters["getOutput/ms"] = Counter(0, benchmark::Counter::kAvgIterations);
    state.counters["elapsed/ms"] = Counter(0, benchmark::Counter::kAvgIterations);
}

void BaseOperatorLargeFixture::TearDown(benchmark::State &state)
{
    if (operatorFactory != nullptr) {
        delete operatorFactory;
        operatorFactory = nullptr;
    }
}

void BaseOperatorLargeFixture::RunDefaultBenchmark(benchmark::State &state)
{
    auto skipMessage = MessageWhenSkip(state);
    if (!skipMessage.empty() || operatorFactory == nullptr) {
        state.SetIterationTime(std::numeric_limits<double>::infinity());
        state.SetLabel(skipMessage);
        return;
    }
    for (__attribute__((unused)) auto _ : state) {
        Operator *op = operatorFactory->CreateOperator();
        VectorBatch *outputVecBatch = nullptr;
        std::chrono::duration<double> addInputDuration(0);
        std::chrono::duration<double> getOutputDuration(0);

        VectorBatch *vb = createSingleVecBatch(state);
        while (vb != nullptr) {
            auto start = std::chrono::high_resolution_clock::now();
            op->AddInput(vb);
            addInputDuration += std::chrono::duration_cast<std::chrono::duration<double>>(
                std::chrono::high_resolution_clock::now() - start);

            if (GetOutputStrategy() == AFTER_EACH_INPUT_FINISHED) {
                auto outputStart = std::chrono::high_resolution_clock::now();
                while (op->GetStatus() != OMNI_STATUS_FINISHED) {
                    op->GetOutput(&outputVecBatch);
                    if (outputVecBatch != nullptr) {
                        VectorHelper::FreeVecBatch(outputVecBatch);
                        outputVecBatch = nullptr;
                    } else {
                        break;
                    }
                }
                getOutputDuration += std::chrono::duration_cast<std::chrono::duration<double>>(
                    std::chrono::high_resolution_clock::now() - outputStart);
            }
            // next VectorBatch
            vb = createSingleVecBatch(state);
        }

        if (GetOutputStrategy() == AFTER_ALL_INPUT_FINISHED) {
            auto outputStart = std::chrono::high_resolution_clock::now();
            while (op->GetStatus() != OMNI_STATUS_FINISHED) {
                op->GetOutput(&outputVecBatch);
                if (outputVecBatch != nullptr) {
                    VectorHelper::FreeVecBatch(outputVecBatch);
                    outputVecBatch = nullptr;
                } else {
                    break;
                }
            }
            getOutputDuration += std::chrono::duration_cast<std::chrono::duration<double>>(
                std::chrono::high_resolution_clock::now() - outputStart);
        }

        Operator::DeleteOperator(op);
        state.SetIterationTime(addInputDuration.count() + getOutputDuration.count());
        int timeRatio = 1000;
        state.counters["addInput/ms"] += addInputDuration.count() * timeRatio;
        state.counters["getOutput/ms"] += getOutputDuration.count() * timeRatio;
        state.counters["elapsed/ms"] += (addInputDuration.count() + getOutputDuration.count()) * timeRatio;
    }
}
} // namespace om_benchmark
