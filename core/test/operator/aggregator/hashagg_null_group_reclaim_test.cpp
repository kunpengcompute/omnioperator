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
#include <cstdio>
#include <set>
#include <limits>
#include <memory>
#include <vector>
#include <gtest/gtest.h>
#include "operator/aggregation/aggregator/aggregator_util.h"
#include "operator/aggregation/group_aggregation.h"
#include "operator/operator.h"
#include "vector/vector_helper.h"
#include "util/test_util.h"
#include "memory/omni_task_memory_pool.h"
#include "memory/pool_listener.h"
#include "operator/reclaim_context.h"

namespace omniruntime {
using namespace omniruntime::vec;
using namespace omniruntime::op;
using namespace omniruntime::type;

namespace {
struct Result {
    int64_t rows = 0;
    int64_t nullGroups = 0;
    int64_t nullGroupSum = 0;
};

Result RunNullGroupScenario(bool firstBatchHasNulls, int reclaimCount, bool reclaimAfterInput = false,
    bool reclaimDuringOutputReservation = false)
{
    constexpr int32_t kRows = 2048;
    constexpr int32_t kNullRows = 7;
    struct ReservationListener : omniruntime::mem::PoolListener {
        std::function<void()> callback;
        void allocationChanged(int64_t delta) override {
            const auto request = CurrentReservationRequest();
            if (delta > 0 && callback && request.site != nullptr &&
                std::string(request.site) == "HashAggregationOperator::EnsureOutputFits") {
                auto invoke = std::move(callback);
                callback = nullptr;
                invoke();
            }
        }
    } listener;
    omniruntime::mem::OmniTaskMemoryPool pool(&listener);
    const auto makeInput = [&](int64_t firstKey, bool withNulls) {
        auto *input = new VectorBatch(kRows);
        auto *keys = new Vector<int64_t>(kRows, &pool);
        auto *values = new Vector<int64_t>(kRows, &pool);
        for (int32_t row = 0; row < kRows; ++row) {
            keys->SetValue(row, (firstKey + row) * 500003);
            values->SetValue(row, 1);
        }
        if (withNulls) {
            for (int32_t row = 0; row < kNullRows; ++row) keys->SetNull(row);
        }
        input->Append(keys);
        input->Append(values);
        return input;
    };

    std::vector<uint32_t> groupByCols({0});
    DataTypes groupInputTypes(std::vector<DataTypePtr>({LongType()}));
    auto aggInputCols = AggregatorUtil::WrapWithVector(std::vector<uint32_t>({1}));
    auto aggInputTypes = AggregatorUtil::WrapWithVector(DataTypes(std::vector<DataTypePtr>({LongType()})));
    auto aggOutputTypes = AggregatorUtil::WrapWithVector(DataTypes(std::vector<DataTypePtr>({LongType()})));
    std::vector<uint32_t> aggFuncTypes({OMNI_AGGREGATION_TYPE_SUM});
    std::vector<uint32_t> maskCols({static_cast<uint32_t>(-1)});
    std::vector<bool> inputRaws({true});
    std::vector<bool> outputPartials({false});
    std::vector<int8_t> hasAggFilters({static_cast<int8_t>(-1)});
    SparkSpillConfig spillConfig(TestUtil::GenerateSpillPath(), INT32_MAX, 0);
    OperatorConfig operatorConfig(spillConfig);
    auto factory = std::make_unique<HashAggregationOperatorFactory>(groupByCols, groupInputTypes,
        aggInputCols, aggInputTypes, aggOutputTypes, aggFuncTypes, maskCols, inputRaws,
        outputPartials, hasAggFilters, operatorConfig, AggregationNode::Step::K_SINGLE);
    factory->Init();
    auto *aggregation = static_cast<HashAggregationOperator *>(factory->CreateOperator(&pool));
    aggregation->SetRowCountPerBatch(kRows);
    aggregation->setNoMoreInput(false);

    aggregation->AddInput(makeInput(0, firstBatchHasNulls));
    for (int i = 0; i < reclaimCount; ++i) {
        std::fprintf(stderr, "REVIEW_REPRO reclaim#%d reclaimable=%llu\n", i,
            static_cast<unsigned long long>(aggregation->reclaimableBytes()));
        aggregation->reclaim(1);
        aggregation->AddInput(makeInput(kRows * (i + 1), false));
    }
    aggregation->setNoMoreInput(true);
    if (reclaimAfterInput) {
        EXPECT_GT(aggregation->reclaimableBytes(), 0);
        aggregation->reclaim(1);
    }
    bool outputReservationReclaimed = false;
    if (reclaimDuringOutputReservation) {
        pool.releaseUnusedReservation();
        listener.callback = [&]() {
            outputReservationReclaimed = true;
            EXPECT_GT(aggregation->reclaimableBytes(), 0);
            EXPECT_GT(aggregation->reclaim(1), 0);
        };
    }
    std::fprintf(stderr, "REVIEW_REPRO draining output\n");

    Result result;
    while (!aggregation->isFinished()) {
        VectorBatch *output = nullptr;
        {
            op::Operator::NonReclaimableSection driverGuard(aggregation);
            aggregation->GetOutput(&output);
        }
        EXPECT_EQ(aggregation->reclaimableBytes(), 0);
        if (output == nullptr) continue;
        auto *keys = output->Get(0);
        auto *sums = static_cast<Vector<int64_t> *>(output->Get(1));
        for (int32_t row = 0; row < output->GetRowCount(); ++row) {
            if (keys->IsNull(row)) {
                ++result.nullGroups;
                result.nullGroupSum += sums->GetValue(row);
            }
        }
        result.rows += output->GetRowCount();
        VectorHelper::FreeVecBatch(output);
    }
    if (reclaimDuringOutputReservation) {
        EXPECT_TRUE(outputReservationReclaimed);
    }
    listener.callback = nullptr;
    op::Operator::DeleteOperator(aggregation);
    std::fprintf(stderr, "REVIEW_REPRO rows=%lld nullGroups=%lld nullSum=%lld\n",
        static_cast<long long>(result.rows), static_cast<long long>(result.nullGroups),
        static_cast<long long>(result.nullGroupSum));
    return result;
}
} // namespace

// Control: NULL group, no reclaim (spill never happens).
TEST(HashAggregationNullGroupReclaimTest, NullGroupNoReclaim)
{
    auto r = RunNullGroupScenario(true, 0);
    EXPECT_EQ(r.nullGroups, 1);
    EXPECT_EQ(r.nullGroupSum, 7);
    EXPECT_EQ(r.rows, (2048 - 7) + 1);
}

// Control: no NULL keys.
TEST(HashAggregationNullGroupReclaimTest, NoNullsOneReclaim)
{
    auto r = RunNullGroupScenario(false, 1);
    EXPECT_EQ(r.rows, 2048 * 2);
    EXPECT_EQ(r.nullGroups, 0);
}

// NULL group spilled once, later input has no NULLs.
TEST(HashAggregationNullGroupReclaimTest, NullGroupOneReclaim)
{
    auto r = RunNullGroupScenario(true, 1);
    EXPECT_EQ(r.nullGroups, 1);
    EXPECT_EQ(r.nullGroupSum, 7);
    EXPECT_EQ(r.rows, (2048 - 7) + 2048 + 1);
}

// Two reclaims after the NULL group was spilled.
TEST(HashAggregationNullGroupReclaimTest, NullGroupTwoReclaims)
{
    auto r = RunNullGroupScenario(true, 2);
    EXPECT_EQ(r.nullGroups, 1);
    EXPECT_EQ(r.nullGroupSum, 7);
    EXPECT_EQ(r.rows, (2048 - 7) + 2048 * 2 + 1);
}
TEST(HashAggregationNullGroupReclaimTest, DistinctSpillPreservesFullBigintRange)
{
    mem::OmniTaskMemoryPool pool(nullptr);
    std::vector<uint32_t> groups{0}, functions, masks;
    std::vector<std::vector<uint32_t>> inputs;
    std::vector<DataTypes> inputTypes, outputTypes;
    DataTypes groupTypes(std::vector<DataTypePtr>{LongType()});
    std::vector<bool> inputRaw, outputPartial;
    std::vector<int8_t> filters;
    SparkSpillConfig spill(TestUtil::GenerateSpillPath(), INT32_MAX, 0);
    OperatorConfig config(spill);
    auto factory = std::make_unique<HashAggregationOperatorFactory>(groups, groupTypes,
        inputs, inputTypes, outputTypes, functions, masks, inputRaw, outputPartial,
        filters, config, AggregationNode::Step::K_SINGLE);
    factory->Init();
    auto deleter = [](omniruntime::op::Operator *op) { omniruntime::op::Operator::DeleteOperator(op); };
    std::unique_ptr<omniruntime::op::Operator, decltype(deleter)> owner(factory->CreateOperator(&pool), deleter);
    auto *aggregation = static_cast<HashAggregationOperator *>(owner.get());
    aggregation->setNoMoreInput(false);
    const std::set<int64_t> expected{std::numeric_limits<int64_t>::min(),
        -(int64_t{1} << 40), 0, int64_t{1} << 40, std::numeric_limits<int64_t>::max()};
    for (int pass = 0; pass < 2; ++pass) {
        auto *batch = new VectorBatch(expected.size());
        auto *keys = new Vector<int64_t>(expected.size(), &pool);
        int32_t row = 0;
        for (int64_t key : expected) keys->SetValue(row++, key);
        batch->Append(keys);
        aggregation->AddInput(batch);
        if (pass == 0) {
            ASSERT_GT(aggregation->reclaimableBytes(), 0);
            aggregation->reclaim(1);
        }
    }
    aggregation->noMoreInput();
    std::multiset<int64_t> actual;
    for (int attempts = 0; attempts < 100 && !aggregation->isFinished(); ++attempts) {
        VectorBatch *out = nullptr;
        aggregation->GetOutput(&out);
        if (out == nullptr) continue;
        auto *keys = static_cast<Vector<int64_t> *>(out->Get(0));
        for (int32_t row = 0; row < out->GetRowCount(); ++row) actual.insert(keys->GetValue(row));
        VectorHelper::FreeVecBatch(out);
    }
    EXPECT_TRUE(aggregation->isFinished());
    // Spill statistics are finalized when the disk merger starts during output.
    EXPECT_GT(aggregation->GetSpilledBytes(), 0);
    EXPECT_EQ(actual, std::multiset<int64_t>(expected.begin(), expected.end()));
}

TEST(HashAggregationNullGroupReclaimTest, ReclaimAfterInputBeforeOutputPreservesNullGroup)
{
    auto r = RunNullGroupScenario(true, 0, true);
    EXPECT_EQ(r.nullGroups, 1);
    EXPECT_EQ(r.nullGroupSum, 7);
    EXPECT_EQ(r.rows, (2048 - 7) + 1);
}

TEST(HashAggregationNullGroupReclaimTest, OutputReservationCanReclaimUnderDriverGuard)
{
    auto r = RunNullGroupScenario(true, 0, false, true);
    EXPECT_EQ(r.nullGroups, 1);
    EXPECT_EQ(r.nullGroupSum, 7);
    EXPECT_EQ(r.rows, (2048 - 7) + 1);
}

} // namespace omniruntime
