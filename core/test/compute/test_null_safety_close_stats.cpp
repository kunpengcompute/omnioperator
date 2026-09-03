/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 *
 * Null-safety tests for close() and updatePipelineStats() when the operators_
 * vector contains nullptr entries (as produced by cascade release).
 *
 * The primary tests directly inject nullptr entries to simulate the
 * post-cascade-release state.
 *
 * NOTE: This file deliberately avoids `using namespace omniruntime::op` because
 * the codebase has three competing `Operator` types (omniruntime::op::Operator,
 * omniruntime::expressions::Operator, and llvm::Operator). Using fully-qualified
 * names prevents the ambiguity (see Agents.md §5.9 Namespace Disambiguation).
 */

#include "compute/driver.h"
#include "compute/task.h"
#include "compute/local_planner.h"
#include "compute/ResultIterator.h"
#include "compute/ColumnarBatchIterator.h"
#include "plannode/RowVectorStream.h"
#include "plannode/planNode.h"
#include "plannode/planFragment.h"
#include "operator/operator.h"
#include "operator/operator_factory.h"
#include "gtest/gtest.h"
#include "test/util/test_util.h"
#include "util/config/QueryConfig.h"
#include "util/type_util.h"
#include "vector/vector_helper.h"

using namespace omniruntime;
using namespace omniruntime::compute;

namespace NullSafetyTest {

/// A minimal batch iterator that yields a single pre-constructed VectorBatch.
class TestBatchIterator : public ColumnarBatchIterator {
public:
    explicit TestBatchIterator(const std::vector<vec::VectorBatch *> &data) : data_(data) {}

    ~TestBatchIterator() override = default;

    vec::VectorBatch *Next() override
    {
        if (index_ < data_.size()) {
            return data_[index_++];
        }
        return nullptr;
    }

private:
    std::vector<vec::VectorBatch *> data_;
    size_t index_ = 0;
};

/// A minimal concrete Operator for direct unit testing of OmniDriver.
/// This operator is stateless and can be used to populate operators_ vector.
/// Uses fully-qualified omniruntime::op::Operator to avoid ambiguity with
/// omniruntime::expressions::Operator and llvm::Operator.
class NullSafetyTestOperator : public omniruntime::op::Operator {
public:
    NullSafetyTestOperator()
    {
        setNoMoreInput(false);
    }

    int32_t AddInput(vec::VectorBatch *vecBatch) override
    {
        SetInputVecBatch(vecBatch);
        return 0;
    }

    int32_t GetOutput(vec::VectorBatch **result) override
    {
        *result = nullptr;
        if (hasNoMoreInput()) {
            SetStatus(OMNI_STATUS_FINISHED);
        }
        return 0;
    }
};

/// Helper: create a simple VectorBatch with one INT32 column.
vec::VectorBatch *CreateSimpleBatch(int32_t rowCount)
{
    if (rowCount <= 0) {
        return nullptr; 
    }
    std::vector<type::DataTypePtr> types = {IntType()};
    type::DataTypes sourceTypes(types);
    int32_t *data = new int32_t[rowCount];
    for (int32_t i = 0; i < rowCount; i++) {
        data[i] = i;
    }
    vec::VectorBatch *batch = TestUtil::CreateVectorBatch(sourceTypes, rowCount, data);
    delete[] data;
    return batch;
}

/// Helper: build a Limit pipeline and run it to completion.
/// Returns the OmniTask (which holds the drivers_ internally).
std::shared_ptr<OmniTask> BuildAndRunLimitPipeline(int32_t dataSize, int32_t limitCount)
{
    std::vector<type::DataTypePtr> types = {IntType()};
    type::DataTypes sourceTypes(types);

    vec::VectorBatch *inputBatch = CreateSimpleBatch(dataSize);
    std::vector<vec::VectorBatch *> inputVectors;
    inputVectors.push_back(inputBatch);

    auto sourceBatchIterator = std::make_unique<TestBatchIterator>(inputVectors);
    auto resIterator = std::make_shared<ResultIterator>(std::move(sourceBatchIterator));
    auto outTypes = std::make_shared<type::DataTypes>(types);
    auto valueStreamNode = std::make_shared<const ValueStreamNode>("value_stream", outTypes, resIterator);

    auto limitNode = std::make_shared<const LimitNode>("limit", 0, limitCount, false, valueStreamNode);
    std::unordered_set<PlanNodeId> emptySet;
    PlanFragment planFragment{limitNode, ExecutionStrategy::K_UNGROUPED, 1, emptySet};
    auto task = std::make_shared<OmniTask>(planFragment, config::QueryConfig());

    // Run the pipeline to completion
    while (true) {
        auto future = OmniFuture::makeEmpty();
        auto out = task->Next(&future);
        if (out != nullptr) {
            vec::VectorHelper::FreeVecBatch(out);
        }
        if (!future.valid()) {
            break;
        }
        future.wait();
    }

    return task;
}

} // namespace NullSafetyTest

using namespace NullSafetyTest;

// ============================================================================
// Test Suite 1: Direct nullptr injection — precise unit tests for null safety.
// These tests directly construct an OmniDriver and set some operators_ to
// nullptr, simulating the post-cascade-release state.
// ============================================================================

TEST(NullSafetyCloseStatsTest, testCloseSkipsNullOperators)
{
    // Construct a driver with 3 operators, then nullify the middle one.
    // close() must skip the nullptr without crashing.
    auto driver = std::make_shared<OmniDriver>();

    std::shared_ptr<omniruntime::op::Operator> op0 = std::make_shared<NullSafetyTestOperator>();
    std::shared_ptr<omniruntime::op::Operator> op1 = std::make_shared<NullSafetyTestOperator>();
    std::shared_ptr<omniruntime::op::Operator> op2 = std::make_shared<NullSafetyTestOperator>();

    driver->operators()->push_back(op0);
    driver->operators()->push_back(op1);
    driver->operators()->push_back(op2);

    // Simulate cascade release: op1 has been released (set to nullptr)
    (*driver->operators())[1] = nullptr;

    // close() must not crash — this is the critical assertion
    EXPECT_NO_THROW(driver->close());

    // After close(), all entries should be nullptr
    for (auto &op : *driver->operators()) {
        EXPECT_EQ(op, nullptr);
    }
}

TEST(NullSafetyCloseStatsTest, testUpdatePipelineStatsSkipsNullOperators)
{
    // Construct a driver with operators, nullify one, then call updatePipelineStats().
    // It must skip the nullptr and still collect stats from the remaining operators.
    auto driver = std::make_shared<OmniDriver>();

    std::shared_ptr<omniruntime::op::Operator> op0 = std::make_shared<NullSafetyTestOperator>();
    std::shared_ptr<omniruntime::op::Operator> op1 = std::make_shared<NullSafetyTestOperator>();

    driver->operators()->push_back(op0);
    driver->operators()->push_back(op1);

    // Simulate cascade release: op0 has been released
    (*driver->operators())[0] = nullptr;

    // updatePipelineStats() must not crash
    EXPECT_NO_THROW(driver->updatePipelineStats());

    // pipelineStats() calls updatePipelineStats internally if not closed
    EXPECT_NO_THROW(driver->pipelineStats());

    driver->close();
}

TEST(NullSafetyCloseStatsTest, testCloseAllOperatorsNull)
{
    // Edge case: all operators are nullptr (extreme cascade release scenario)
    auto driver = std::make_shared<OmniDriver>();

    driver->operators()->push_back(nullptr);
    driver->operators()->push_back(nullptr);
    driver->operators()->push_back(nullptr);

    EXPECT_NO_THROW(driver->close());
}

TEST(NullSafetyCloseStatsTest, testCloseEmptyOperators)
{
    // Edge case: no operators at all
    auto driver = std::make_shared<OmniDriver>();

    EXPECT_NO_THROW(driver->close());
}

TEST(NullSafetyCloseStatsTest, testCloseIdempotentWithNullOperators)
{
    // Verify that calling close() twice works correctly even with nullptr entries
    auto driver = std::make_shared<OmniDriver>();

    std::shared_ptr<omniruntime::op::Operator> op0 = std::make_shared<NullSafetyTestOperator>();
    std::shared_ptr<omniruntime::op::Operator> op1 = std::make_shared<NullSafetyTestOperator>();

    driver->operators()->push_back(op0);
    driver->operators()->push_back(nullptr); // already released
    driver->operators()->push_back(op1);

    EXPECT_NO_THROW(driver->close());
    EXPECT_NO_THROW(driver->close()); // second call should be a no-op due to closed_ flag
}

// ============================================================================
// Test Suite 2: Pipeline-level regression tests
// These run a real Limit pipeline and verify that close/stats operations
// work correctly after the pipeline completes normally.
// ============================================================================

TEST(NullSafetyCloseStatsTest, testPipelineRunThenCloseNoCrash)
{
    // Run a Limit pipeline to completion, then let the task destruct.
    // The destructor calls driver->close() — this must not crash.
    EXPECT_NO_THROW({
        auto task = BuildAndRunLimitPipeline(4, 3);
        // task goes out of scope here -> destructor calls close()
    });
}

TEST(NullSafetyCloseStatsTest, testPipelineRunThenGetTaskStats)
{
    // Run a Limit pipeline, then call GetTaskStats().
    // GetTaskStats() internally calls driver->pipelineStats() which calls
    // updatePipelineStats() — must not crash.
    auto task = BuildAndRunLimitPipeline(4, 3);

    EXPECT_NO_THROW({
        auto taskStats = task->GetTaskStats();
        // After a normal pipeline run, there should be at least one pipeline stats entry
        EXPECT_GE(taskStats.pipelineStats.size(), static_cast<size_t>(1));
    });
}
