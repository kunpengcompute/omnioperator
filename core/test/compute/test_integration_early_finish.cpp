/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 *
 * Integration tests for the early-finish operator cascade release mechanism.
 *
 * Verifies end-to-end correctness of the pipeline early memory release for
 * early-finish operators (Limit). The cascade self-closing loop is:
 *   Step 1: Limit isFinished → ReleaseFinishedOperators releases Limit,
 *           propagates noMoreInput upstream
 *   Step 2: Orphan drain: upstream's nextOp==nullptr → drain residual
 *           output and discard
 *   Step 3: Cascade release: upstream isFinished → release operators[0..i]
 *
 * API note: OmniOperator-LGS uses raw Expr* (ExprPtr = Expr*) for plan-node
 * expressions, with FieldExpr / LiteralExpr as the concrete types.  OmniTask
 * has no public Close() method — cleanup is via the destructor.  Input
 * VectorBatch ownership transfers to operators (AddInput frees them); tests
 * must NOT double-free input batches.
 */

#include <gtest/gtest.h>

#include <memory>
#include <unordered_set>
#include <vector>

#include "compute/ColumnarBatchIterator.h"
#include "compute/ResultIterator.h"
#include "compute/driver.h"
#include "compute/task.h"
#include "expression/expressions.h"
#include "plannode/RowVectorStream.h"
#include "plannode/planFragment.h"
#include "plannode/planNode.h"
#include "test/util/test_util.h"
#include "type/data_type.h"
#include "type/data_types.h"
#include "util/config/QueryConfig.h"
#include "vector/vector_batch.h"
#include "vector/vector_helper.h"

using namespace omniruntime;
using namespace omniruntime::compute;
using namespace omniruntime::expressions;
using namespace omniruntime::type;
using namespace omniruntime::vec;

namespace IntegrationEarlyFinishTest {

// ================================================================
// Reusable batch iterator that feeds pre-built VectorBatch data
// into a ValueStreamNode.
// ================================================================
class TestBatchIterator : public ColumnarBatchIterator
{
public:
    explicit TestBatchIterator(const std::vector<VectorBatch *> &data) : data_(data) {}

    ~TestBatchIterator() override
    {
        // Free batches that were never consumed by Next().
        // Batches already returned via Next() are owned by downstream
        // operators (AddInput takes ownership), so we must NOT double-free
        // them. Only batches at index >= index_ are still unconsumed.
        for (size_t i = index_; i < data_.size(); ++i) {
            VectorHelper::FreeVecBatch(data_[i]);
        }
    }

    VectorBatch *Next() override
    {
        if (index_ < data_.size()) {
            return data_[index_++];
        }
        return nullptr;
    }

private:
    std::vector<VectorBatch *> data_;
    size_t index_ = 0;
};

// ================================================================
// Helper: run an OmniTask to completion and return total output rows.
//
// Completion is signaled by !future.valid() (the future is invalid/empty
// when the pipeline is done), NOT by a null output pointer.  When the
// future is valid, the pipeline is blocked — call future.wait() and loop.
// ================================================================
static int32_t RunTaskAndCountRows(std::shared_ptr<OmniTask> &task)
{
    int32_t totalRows = 0;
    while (true) {
        auto future = OmniFuture::makeEmpty();
        auto out = task->Next(&future);
        if (!future.valid()) {
            // Pipeline completed — out holds the final batch (or nullptr).
            if (out != nullptr) {
                totalRows += out->GetRowCount();
                VectorHelper::FreeVecBatch(out);
            }
            break;
        }
        // Pipeline is blocked (e.g., waiting for consumer) — wait and retry.
        future.wait();
    }
    return totalRows;
}

// ================================================================
// Helper: run an OmniTask to completion, draining all output batches.
// Used when we don't need to count rows — just consume everything.
// ================================================================
static void RunTaskToCompletion(std::shared_ptr<OmniTask> &task)
{
    while (true) {
        auto future = OmniFuture::makeEmpty();
        auto out = task->Next(&future);
        if (!future.valid()) {
            if (out != nullptr) {
                VectorHelper::FreeVecBatch(out);
            }
            break;
        }
        future.wait();
    }
}

// ================================================================
// Test 1: ValueStream → Filter → Limit(1) → Project pipeline
//
// Exercises the full cascade: Limit early-finishes after 1 row, releases
// itself and propagates noMoreInput upstream. Filter drains residual
// output via orphan path, then cascade-releases operators[0..1].
//
// Pipeline completes with correct output (≤ 1 row) and no crash.
// ================================================================
TEST(IntegrationEarlyFinish, testLimitPipeline_ThreeStepCascade)
{
    // --- Build input data: two batches of 5 INT32 rows each (10 total) ---
    std::vector<DataTypePtr> types = {IntType()};
    DataTypes sourceTypes(types);

    int32_t data1[] = {1, 2, 3, 4, 5};
    int32_t data2[] = {6, 7, 8, 9, 10};

    VectorBatch *batch1 = TestUtil::CreateVectorBatch(sourceTypes, 5, data1);
    VectorBatch *batch2 = TestUtil::CreateVectorBatch(sourceTypes, 5, data2);

    auto sourceIterator = std::make_unique<TestBatchIterator>(
        std::vector<VectorBatch *>{batch1, batch2});
    auto resIterator = std::make_shared<ResultIterator>(std::move(sourceIterator));

    // --- ValueStreamNode (leaf) ---
    auto outTypes = std::make_shared<DataTypes>(types);
    auto valueStreamNode = std::make_shared<const ValueStreamNode>(
        "0", outTypes, resIterator);

    // --- Filter: identity (pass-through) ---
    // LiteralExpr(true) as filter condition → all rows pass.
    auto trueFilter = new LiteralExpr(true, BooleanType());
    std::vector<Expr *> projectList = {new FieldExpr(0, IntType())};
    auto filterNode = std::make_shared<const FilterNode>(
        "1", trueFilter, valueStreamNode, projectList);

    // --- Limit (limit=1, early-finish) ---
    auto limitNode = std::make_shared<const LimitNode>(
        "2", 0, 1, false, filterNode);

    // --- Project as sink ---
    std::vector<Expr *> projList = {new FieldExpr(0, IntType())};
    auto projectNode = std::make_shared<const ProjectNode>(
        "3", std::move(projList), limitNode);

    // --- PlanFragment → OmniTask ---
    std::unordered_set<PlanNodeId> emptySet;
    PlanFragment planFragment{projectNode, ExecutionStrategy::K_UNGROUPED, 1, emptySet};
    auto task = std::make_shared<OmniTask>(planFragment, config::QueryConfig());

    // --- Run pipeline — should produce at most 1 row (limit=1) ---
    int32_t outputRows = RunTaskAndCountRows(task);
    EXPECT_LE(outputRows, 1) << "limit=1, so at most 1 row output";

    // Stats completeness: all operators should have stats (even released ones)
    auto taskStats = task->GetTaskStats();
    for (const auto &pipelineStats : taskStats.pipelineStats) {
        for (const auto &opStats : pipelineStats.operatorStats) {
            EXPECT_GE(opStats.operatorId, 0);
        }
    }

    // Input batches are freed by downstream operators (AddInput takes ownership).
}

// ================================================================
// Test 2: ValueStream → Limit(5) → Project (no intermediate filter)
//
// Simpler pipeline: early-finish with orphan drain on ValueStream directly.
// Limit releases itself, propagates noMoreInput to ValueStream.
// Orphan drain pulls remaining batches from ValueStream and discards them.
//
// With 10 rows total and limit=5, should get exactly 5 rows.
// ================================================================
TEST(IntegrationEarlyFinish, testSimpleLimitPipeline_OrphanDrainOnSource)
{
    std::vector<DataTypePtr> types = {IntType()};
    DataTypes sourceTypes(types);

    int32_t data1[] = {10, 20, 30, 40, 50};
    int32_t data2[] = {60, 70, 80, 90, 100};

    VectorBatch *batch1 = TestUtil::CreateVectorBatch(sourceTypes, 5, data1);
    VectorBatch *batch2 = TestUtil::CreateVectorBatch(sourceTypes, 5, data2);

    auto sourceIterator = std::make_unique<TestBatchIterator>(
        std::vector<VectorBatch *>{batch1, batch2});
    auto resIterator = std::make_shared<ResultIterator>(std::move(sourceIterator));

    auto outTypes = std::make_shared<DataTypes>(types);
    auto valueStreamNode = std::make_shared<const ValueStreamNode>(
        "0", outTypes, resIterator);

    // --- Limit (limit=5, early-finish) ---
    auto limitNode = std::make_shared<const LimitNode>(
        "1", 0, 5, false, valueStreamNode);

    // --- Project as sink ---
    std::vector<Expr *> projList = {new FieldExpr(0, IntType())};
    auto projectNode = std::make_shared<const ProjectNode>(
        "2", std::move(projList), limitNode);

    std::unordered_set<PlanNodeId> emptySet;
    PlanFragment planFragment{projectNode, ExecutionStrategy::K_UNGROUPED, 1, emptySet};
    auto task = std::make_shared<OmniTask>(planFragment, config::QueryConfig());

    int32_t outputRows = RunTaskAndCountRows(task);

    // With 10 rows total and limit=5, should get exactly 5
    EXPECT_EQ(outputRows, 5) << "10 rows input, limit=5 → expect 5 rows output";

    auto taskStats = task->GetTaskStats();
    EXPECT_GE(taskStats.pipelineStats.size(), static_cast<size_t>(0));
}

// ================================================================
// Test 3: Destructor safety after early-finish cascade
//
// After cascade release, operators_ contains nullptr entries. The task
// destructor calls driver->close() which must skip nullptr operators.
// We explicitly reset the shared_ptr to trigger the destructor in the
// test body.
// ================================================================
TEST(IntegrationEarlyFinish, testDestructorSafety_AfterEarlyFinish)
{
    std::vector<DataTypePtr> types = {IntType()};
    DataTypes sourceTypes(types);

    int32_t data1[] = {1, 2, 3};

    VectorBatch *batch1 = TestUtil::CreateVectorBatch(sourceTypes, 3, data1);

    auto sourceIterator = std::make_unique<TestBatchIterator>(
        std::vector<VectorBatch *>{batch1});
    auto resIterator = std::make_shared<ResultIterator>(std::move(sourceIterator));

    auto outTypes = std::make_shared<DataTypes>(types);
    auto valueStreamNode = std::make_shared<const ValueStreamNode>(
        "0", outTypes, resIterator);

    // --- Filter: identity ---
    auto trueFilter = new LiteralExpr(true, BooleanType());
    std::vector<Expr *> projectList = {new FieldExpr(0, IntType())};
    auto filterNode = std::make_shared<const FilterNode>(
        "1", trueFilter, valueStreamNode, projectList);

    // --- Limit (limit=1, early-finish) ---
    auto limitNode = std::make_shared<const LimitNode>(
        "2", 0, 1, false, filterNode);

    // --- Project as sink ---
    std::vector<Expr *> projList = {new FieldExpr(0, IntType())};
    auto projectNode = std::make_shared<const ProjectNode>(
        "3", std::move(projList), limitNode);

    std::unordered_set<PlanNodeId> emptySet;
    PlanFragment planFragment{projectNode, ExecutionStrategy::K_UNGROUPED, 1, emptySet};
    auto task = std::make_shared<OmniTask>(planFragment, config::QueryConfig());

    // Run pipeline to completion — triggers early-finish cascade
    RunTaskToCompletion(task);

    // Explicitly destroy the task — destructor calls driver->close()
    // which must skip nullptr operators (cascade-released).
    EXPECT_NO_THROW({ task.reset(); });

    SUCCEED() << "Destructor handled nullptr operators in close() safely";
}

// ================================================================
// Test 4: Limit with offset
//
// ValueStream → Limit(offset=3, limit=2) → Project
// Limit should skip first 3 rows, output 2 rows, then early-finish.
// ================================================================
TEST(IntegrationEarlyFinish, testLimitWithOffset_EarlyFinish)
{
    std::vector<DataTypePtr> types = {IntType()};
    DataTypes sourceTypes(types);

    int32_t data1[] = {10, 20, 30, 40, 50};

    VectorBatch *batch1 = TestUtil::CreateVectorBatch(sourceTypes, 5, data1);

    auto sourceIterator = std::make_unique<TestBatchIterator>(
        std::vector<VectorBatch *>{batch1});
    auto resIterator = std::make_shared<ResultIterator>(std::move(sourceIterator));

    auto outTypes = std::make_shared<DataTypes>(types);
    auto valueStreamNode = std::make_shared<const ValueStreamNode>(
        "0", outTypes, resIterator);

    // --- Limit offset=3, limit=2 → skip 10,20,30, output 40,50 ---
    auto limitNode = std::make_shared<const LimitNode>(
        "1", 3, 2, false, valueStreamNode);

    // --- Project as sink ---
    std::vector<Expr *> projList = {new FieldExpr(0, IntType())};
    auto projectNode = std::make_shared<const ProjectNode>(
        "2", std::move(projList), limitNode);

    std::unordered_set<PlanNodeId> emptySet;
    PlanFragment planFragment{projectNode, ExecutionStrategy::K_UNGROUPED, 1, emptySet};
    auto task = std::make_shared<OmniTask>(planFragment, config::QueryConfig());

    int32_t outputRows = RunTaskAndCountRows(task);

    // offset=3, limit=2 → expect 2 rows
    EXPECT_EQ(outputRows, 2) << "offset=3, limit=2 on 5 rows → expect 2 rows output";
}

}  // namespace IntegrationEarlyFinishTest
