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

#include <random>
#include <iostream>
#include "common/common.h"
#include "common/vector_util.h"
#include "util/perf_util.h"
#include "operator/join/sortmergejoin/sort_merge_join.h"

using namespace benchmark;
using namespace omniruntime::op;
using namespace omniruntime::vec;
using namespace omniruntime::type;

namespace om_benchmark {
class SortMergeJoinBM : public Fixture {
public:
    int32_t streamedTblSize = 100000;
    int32_t bufferTblSize = 100000;
    std::map<std::string, std::vector<DataTypePtr>> STREAMED_TYPES = {{"group1", {LongType(), IntType()}}};
    std::map<std::string, std::vector<DataTypePtr>> BUFFER_TYPES = {{"group1", {IntType(), LongType()}}};
    std::map<std::string, std::vector<int32_t>> STREAMED_KEYCOLS = {{"group1", {0}}};
    std::map<std::string, std::vector<int32_t>> STREAMED_OUTPUTCOLS = {{"group1", {1}}};
    std::map<std::string, std::vector<int32_t>> BUFFER_KEYCOLS = {{"group1", {1}}};
    std::map<std::string, std::vector<int32_t>> BUFFER_OUTPUTCOLS = {{"group1", {0}}};
    std::map<std::string, JoinType> JOIN_TYPE = {{"group1", JoinType::OMNI_JOIN_TYPE_INNER}};

    void SetUp(benchmark::State &state)
    {
        auto groupKey = "group" + std::to_string(state.range(0));
        std::string blank = "";
        smjOp = new SortMergeJoinOperator(JOIN_TYPE[groupKey], blank);
        printf("done setup\n");
        state.counters["addInput/ms"] = Counter(0, benchmark::Counter::kAvgIterations);
        state.counters["getOutput/ms"] = Counter(0, benchmark::Counter::kAvgIterations);
        state.counters["addInput InstCount"] = Counter(0, benchmark::Counter::kAvgIterations);
        state.counters["addInput CacheMisses"] = Counter(0, benchmark::Counter::kAvgIterations);
        state.counters["getOutput InstCount"] = Counter(0, benchmark::Counter::kAvgIterations);
        state.counters["getOutput CacheMisses"] = Counter(0, benchmark::Counter::kAvgIterations);
        state.counters["elapsed/ms"] = Counter(0, benchmark::Counter::kAvgIterations);
    }

    void TearDown(const benchmark::State &state)
    {
        if (smjOp != nullptr) {
            delete smjOp;
            smjOp = nullptr;
        }
    }

    void SortMergeJoinTest(benchmark::State &state)
    {
        auto groupKey = "group" + std::to_string(state.range(0));
        std::chrono::duration<double> addInputDuration(0);
        std::chrono::duration<double> getOutputDuration(0);
        auto *perfUtil = new PerfUtil();
        perfUtil->Init();

        std::string blank = "";

        DataTypes streamedTblTypes(STREAMED_TYPES[groupKey]);
        smjOp->ConfigStreamedTblInfo(streamedTblTypes, STREAMED_KEYCOLS[groupKey], STREAMED_OUTPUTCOLS[groupKey],
                                     streamedTblTypes.GetSize());

        DataTypes bufferedTblTypes(BUFFER_TYPES[groupKey]);
        smjOp->ConfigBufferedTblInfo(bufferedTblTypes, BUFFER_KEYCOLS[groupKey], BUFFER_OUTPUTCOLS[groupKey],
                                     bufferedTblTypes.GetSize());

        smjOp->InitScannerAndResultBuilder(nullptr);

        auto streamedValues = CreateVectorValues(65538 * 1000, 2, 1);
        std::string strPrefix = "smj";
        VectorBatch *streamedVecBatch =
            om_benchmark::CreateVectorBatch(OMNI_FLAT, streamedTblTypes.Get(), strPrefix, streamedValues, 65538 * 1000);

        auto bufferValues = CreateVectorValues(65538 * 1000, 2, 3);
        VectorBatch *bufferVecBatch =
            om_benchmark::CreateVectorBatch(OMNI_FLAT, streamedTblTypes.Get(), strPrefix, bufferValues, 65538 * 1000);

        perfUtil->Reset();
        perfUtil->Start();
        auto start = std::chrono::high_resolution_clock::now();
        smjOp->AddStreamedTableInput(streamedVecBatch);
        smjOp->AddBufferedTableInput(bufferVecBatch);

        addInputDuration += std::chrono::duration_cast<std::chrono::duration<double>>(
            std::chrono::high_resolution_clock::now() - start);

        perfUtil->Stop();
        auto dataMap = perfUtil->GetData();
        state.counters["addInput InstCount"] += dataMap["Instructions"];
        state.counters["addInput CacheMisses"] += dataMap["CacheMisses"];
        state.counters["addInput/ms"] += addInputDuration.count();
        perfUtil->Reset();
        perfUtil->Start();
        auto outputStart = std::chrono::high_resolution_clock::now();
        omniruntime::vec::VectorBatch *outputVecBatch = nullptr;
        smjOp->GetOutput(&outputVecBatch);
        getOutputDuration += std::chrono::duration_cast<std::chrono::duration<double>>(
            std::chrono::high_resolution_clock::now() - outputStart);

        perfUtil->Stop();
        auto dataMap2 = perfUtil->GetData();
        state.counters["getOutput InstCount"] += dataMap2["Instructions"];
        state.counters["getOutput CacheMisses"] += dataMap2["CacheMisses"];
        state.counters["getOutput/ms"] += getOutputDuration.count();
        state.counters["elapsed/ms"] += addInputDuration.count() + getOutputDuration.count();
        if (outputVecBatch != nullptr) {
            VectorHelper::FreeVecBatch(outputVecBatch);
        }
    }

protected:
    std::vector<std::vector<int32_t>> CreateVectorValues(int32_t rowNumber, int32_t colNumber, int32_t offset)
    {
        std::vector<std::vector<int32_t>> values(colNumber, std::vector<int32_t>(rowNumber));
        for (int32_t i = 0; i < colNumber; ++i) {
            for (int32_t j = 0; j < rowNumber; ++j) {
                values[i][j] = j + i * offset;
            }
        }
        return values;
    }

private:
    SortMergeJoinOperator *smjOp;
};

BENCHMARK_DEFINE_F(SortMergeJoinBM, SortMergeJoinTTest)(benchmark::State &st)
{
    for (auto _ : st)
        SortMergeJoinBM::SortMergeJoinTest(st);
}

BENCHMARK_REGISTER_F(SortMergeJoinBM, SortMergeJoinTTest)->Args({1})->Repetitions(20);
} // namespace om_benchmark
