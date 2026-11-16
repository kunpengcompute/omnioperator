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
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <thread>
#include <vector>
#include <filesystem>

#include "gtest/gtest.h"
#include "compute/task.h"
#include "expression/expressions.h"
#include "memory/pool_listener.h"
#include "memory/omni_task_memory_pool.h"
#include "plannode/RowVectorStream.h"
#include "plannode/planFragment.h"
#include "plannode/planNode.h"
#include "test/util/test_util.h"
#include "type/data_types.h"
#include "util/config/QueryConfig.h"
#include "vector/vector_helper.h"

using namespace omniruntime;
using namespace omniruntime::compute;
using namespace omniruntime::expressions;
using namespace omniruntime::type;
using namespace omniruntime::vec;

namespace JoinSpillEarlyFinishTest {
class NoopListener : public omniruntime::mem::PoolListener {
public:
    void allocationChanged(int64_t) override {}
};

class Iter : public ColumnarBatchIterator {
public:
    explicit Iter(std::vector<VectorBatch *> d) : d_(std::move(d)) {}
    ~Iter() override
    {
        for (size_t i = i_; i < d_.size(); ++i) VectorHelper::FreeVecBatch(d_[i]);
    }
    VectorBatch *Next() override { return i_ < d_.size() ? d_[i_++] : nullptr; }
private:
    std::vector<VectorBatch *> d_;
    size_t i_ = 0;
};

static VectorBatch *MakeBatch(int64_t start, int32_t rows)
{
    DataTypes types(std::vector<DataTypePtr>{LongType(), LongType()});
    std::vector<int64_t> k(rows), v(rows);
    for (int32_t r = 0; r < rows; ++r) {
        k[r] = (start + r) % 1000;
        v[r] = start + r;
    }
    return TestUtil::CreateVectorBatch(types, rows, k.data(), v.data());
}

static long long CountSpillFiles()
{
    std::error_code ec;
    long long n = 0;
    if (!std::filesystem::exists("/tmp/review-repro-spill", ec)) return 0;
    for (auto &e : std::filesystem::recursive_directory_iterator("/tmp/review-repro-spill", ec)) {
        if (e.is_regular_file(ec)) ++n;
    }
    return n;
}

// Returns output rows, or -1 if the task did not finish within timeoutSec (process then _Exit(77)).
static int64_t RunScenario(bool spillEnabled, bool withLimit, bool forceSpill = false, int timeoutSec = 20)
{
    std::vector<VectorBatch *> probe, build;
    for (int b = 0; b < 50; ++b) probe.push_back(MakeBatch(b * 1000, 1000));
    for (int b = 0; b < 2; ++b) build.push_back(MakeBatch(b * 500, 500));

    auto twoLong = std::make_shared<DataTypes>(std::vector<DataTypePtr>{LongType(), LongType()});
    auto probeNode = std::make_shared<const ValueStreamNode>("0", twoLong,
        std::make_shared<ResultIterator>(std::make_unique<Iter>(probe)));
    auto buildNode = std::make_shared<const ValueStreamNode>("1", twoLong,
        std::make_shared<ResultIterator>(std::make_unique<Iter>(build)));

    std::vector<ExprPtr> leftKeys{new FieldExpr(0, LongType())};
    std::vector<ExprPtr> rightKeys{new FieldExpr(0, LongType())};
    std::vector<Expr *> outCols{new FieldExpr(0, LongType()), new FieldExpr(1, LongType()),
        new FieldExpr(2, LongType()), new FieldExpr(3, LongType())};
    PlanNodePtr top = std::make_shared<const HashJoinNode>("2", OMNI_JOIN_TYPE_INNER, BuildSide::OMNI_BUILD_RIGHT,
        false, /*isShuffle=*/true, leftKeys, rightKeys, nullptr, probeNode, buildNode, twoLong, twoLong, outCols);
    if (withLimit) {
        top = std::make_shared<const LimitNode>("3", 0, 5, false, top);
    }

    std::unordered_map<std::string, std::string> cfg;
    cfg["spill_enabled"] = spillEnabled ? "true" : "false";
    cfg["join_spill_enabled"] = "true";
    cfg["spill_dir"] = "/tmp/review-repro-spill";
    if (forceSpill) cfg["force_build_group_spill_after_no_more_input"] = "true";
    std::unordered_set<PlanNodeId> emptySet;
    PlanFragment fragment{top, ExecutionStrategy::K_UNGROUPED, 1, emptySet};

    static NoopListener listener;
    static mem::OmniTaskMemoryPool pool{&listener};
    auto task = std::make_shared<OmniTask>(fragment, config::QueryConfig(cfg), &pool);

    std::mutex m;
    std::condition_variable cv;
    bool done = false;
    std::atomic<int64_t> rows{0};
    std::thread worker([&] {
        bool drained = false;
        while (!drained) {
            auto future = OmniFuture::makeEmpty();
            auto out = task->Next(&future);
            if (out != nullptr) {
                rows += out->GetRowCount();
                VectorHelper::FreeVecBatch(out);
            }
            if (!future.valid()) {
                drained = out == nullptr;
                continue;
            }
            future.wait();
        }
        std::lock_guard<std::mutex> g(m);
        done = true;
        cv.notify_all();
    });
    std::unique_lock<std::mutex> lk(m);
    if (!cv.wait_for(lk, std::chrono::seconds(timeoutSec), [&] { return done; })) {
        std::fprintf(stderr, "REVIEW_REPRO HANG spill=%d limit=%d rowsSoFar=%lld after %ds spillFilesOnDisk=%lld\n", spillEnabled,
            withLimit, static_cast<long long>(rows.load()), timeoutSec, CountSpillFiles());
        std::fflush(stderr);
        std::_Exit(77);
    }
    lk.unlock();
    worker.join();
    task.reset();
    // HashJoinNode does not own its key expressions.
    Expr::DeleteExprs(leftKeys);
    Expr::DeleteExprs(rightKeys);
    std::fprintf(stderr, "REVIEW_REPRO DONE spill=%d limit=%d rows=%lld spillFilesOnDisk=%lld\n", spillEnabled, withLimit,
        static_cast<long long>(rows.load()), CountSpillFiles());
    return rows.load();
}

TEST(JoinSpillEarlyFinishTest, NoSpillWithLimitFinishes) { EXPECT_EQ(RunScenario(false, true), 5); }
TEST(JoinSpillEarlyFinishTest, SpillNoLimitFinishes) { EXPECT_EQ(RunScenario(true, false), 50000); }
TEST(JoinSpillEarlyFinishTest, ForcedSpillNoLimitWritesFiles) { EXPECT_EQ(RunScenario(true, false, true), 50000); }
TEST(JoinSpillEarlyFinishTest, ForcedSpillWithLimitFinishes) { EXPECT_EQ(RunScenario(true, true, true), 5); }
TEST(JoinSpillEarlyFinishTest, SpillWithLimitFinishes) { EXPECT_EQ(RunScenario(true, true), 5); }
} // namespace JoinSpillEarlyFinishTest
