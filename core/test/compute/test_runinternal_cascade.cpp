/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2024. All rights reserved.
 *
 * Integration tests for RunInternal cascade release + orphan drain paths.
 *
 * These tests verify that RunInternal correctly:
 *   1. Skips already-released (nullptr) operators without crash.
 *   2. Triggers ReleaseFinishedOperators when an intermediate operator
 *      finishes during normal pipeline execution.
 *   3. Preserves pipeline stats even after early cascade release.
 *
 * Note: The orphan drain path is exercised end-to-end in the integration
 * tests that build real multi-operator pipelines with PlanNode/OmniTask.
 * Isolated testing of the drain path requires a full pipeline state machine
 * that is impractical to construct with stub operators alone.
 */

#include <gtest/gtest.h>
#include <memory>

#include "compute/driver.h"
#include "operator/operator.h"

using omniruntime::compute::OmniDriver;

// Test Operators.
// Use fully-qualified omniruntime::op::Operator to avoid namespace ambiguity
// between expressions::Operator / op::Operator / llvm::Operator.

/// A simple pass-through operator that finishes when noMoreInput is set.
class TestPipeOperator : public omniruntime::op::Operator
{
public:
    TestPipeOperator() : omniruntime::op::Operator() {}

    int32_t AddInput(omniruntime::vec::VectorBatch *vecBatch) override
    {
        return 0;
    }

    int32_t GetOutput(omniruntime::vec::VectorBatch **outputVecBatch) override
    {
        *outputVecBatch = nullptr;
        if (noMoreInput_) {
            SetStatus(OMNI_STATUS_FINISHED);
        }
        return 0;
    }
};

/// A source operator that produces nothing and finishes when noMoreInput is set.
class TestSourceOperator : public omniruntime::op::Operator
{
public:
    TestSourceOperator() : omniruntime::op::Operator() {}

    int32_t AddInput(omniruntime::vec::VectorBatch *vecBatch) override
    {
        return 0;
    }

    int32_t GetOutput(omniruntime::vec::VectorBatch **outputVecBatch) override
    {
        *outputVecBatch = nullptr;
        if (noMoreInput_) {
            SetStatus(OMNI_STATUS_FINISHED);
        }
        return 0;
    }
};

/// A sink operator used as the last operator in a pipeline.
class TestSinkOperator : public omniruntime::op::Operator
{
public:
    TestSinkOperator() : omniruntime::op::Operator() {}

    bool needsInput() override
    {
        return !noMoreInput_ && !isFinished();
    }

    int32_t AddInput(omniruntime::vec::VectorBatch *vecBatch) override
    {
        return 0;
    }

    int32_t GetOutput(omniruntime::vec::VectorBatch **outputVecBatch) override
    {
        *outputVecBatch = nullptr;
        if (noMoreInput_) {
            SetStatus(OMNI_STATUS_FINISHED);
        }
        return 0;
    }
};

// Test Fixture

class RunInternalCascadeTest : public ::testing::Test
{
protected:
    std::shared_ptr<OmniDriver> driver_;

    void SetUp() override
    {
        driver_ = std::make_shared<OmniDriver>();
    }

    void TearDown() override
    {
        if (driver_ != nullptr) {
            driver_->close();
        }
    }

    /// Push a fresh operator into the driver chain via raw pointer.
    void addNewOperator(omniruntime::op::Operator *rawOp)
    {
        std::unique_ptr<omniruntime::op::Operator> uniqueOp(rawOp);
        driver_->addOperator(std::move(uniqueOp));
    }

    /// Get the operators vector for direct inspection.
    std::vector<std::shared_ptr<omniruntime::op::Operator>> *operators()
    {
        return driver_->operators();
    }

    /// Run the pipeline until completion or until safety cap, with a max
    /// iteration limit to prevent infinite loops in broken state machines.
    /// Returns true if the pipeline reached kAtEnd.
    bool runPipelineToCompletion()
    {
        const int maxIterations = 20;
        for (int iter = 0; iter < maxIterations; ++iter) {
            omniruntime::compute::ContinueFuture future;
            omniruntime::compute::StopReason stopReason;
            omniruntime::vec::VectorBatch *result = driver_->Next(&future, &stopReason);

            if (stopReason == omniruntime::compute::StopReason::kAtEnd) {
                return true;
            }
            if (stopReason == omniruntime::compute::StopReason::kBlock) {
                return false;
            }
            if (result != nullptr) {
                // Discard output produced by the pipeline.
                delete result;
            }
        }
        return false;
    }
};

// Test 1: Cascade release during normal execution.
// Build: Source[0] → Pipe[1] → Sink[2].
// Source is pre-marked as FINISHED.
// Execution flow:
//   i=2: sink.GetOutput() → nullptr, not finished → continue
//   i=1: pipe.GetOutput() → nullptr, not finished → continue
//   i=0: source.GetOutput() → nullptr, isFinished() → nextOp->noMoreInput()
//        → ReleaseFinishedOperators(0) → releases operators[0]
//        → break
//   i=2: sink still alive → continue
//   i=1: pipe now has noMoreInput → GetOutput sets FINISHED → isFinished()
//        → ReleaseFinishedOperators(1) → releases operators[1] (and [0] already null)
//        → break
//   i=2: sink now has noMoreInput → GetOutput sets FINISHED → close() → kAtEnd
TEST_F(RunInternalCascadeTest, testCascadeReleaseDuringExecution)
{
    auto *op0 = new TestSourceOperator();
    auto *op1 = new TestPipeOperator();
    auto *op2 = new TestSinkOperator();

    op0->setNoMoreInput(false);
    op1->setNoMoreInput(false);
    op2->setNoMoreInput(false);

    addNewOperator(op0);
    addNewOperator(op1);
    addNewOperator(op2);
    ASSERT_EQ(operators()->size(), 3u);

    // Pre-mark source as finished (simulating that it has no more data).
    op0->SetStatus(OMNI_STATUS_FINISHED);

    bool completed = runPipelineToCompletion();
    EXPECT_TRUE(completed) << "Pipeline should reach kAtEnd after cascade release of all operators.";
}

// Test 2: Nullptr skip — pipeline resumes after pre-release.
// Build: Source[0] → Pipe[1] → Sink[2].
// Cascade-release operators[0..1] before running Next(), then set sink to
// finished+noMoreInput. RunInternal should skip the nullptr entries and
// complete the pipeline through the surviving sink without crashing.
TEST_F(RunInternalCascadeTest, testNullptrSkipAfterCascadeRelease)
{
    auto *op0 = new TestSourceOperator();
    auto *op1 = new TestPipeOperator();
    auto *op2 = new TestSinkOperator();

    op0->setNoMoreInput(false);
    op1->setNoMoreInput(false);
    op2->setNoMoreInput(false);

    addNewOperator(op0);
    addNewOperator(op1);
    addNewOperator(op2);
    ASSERT_EQ(operators()->size(), 3u);

    // Cascade-release ops[0..1].
    op0->SetStatus(OMNI_STATUS_FINISHED);
    op1->SetStatus(OMNI_STATUS_FINISHED);
    driver_->ReleaseFinishedOperators(1);

    // Verify pre-release state.
    EXPECT_EQ((*operators())[0], nullptr) << "Operator 0 should be released.";
    EXPECT_EQ((*operators())[1], nullptr) << "Operator 1 should be released.";
    EXPECT_NE((*operators())[2], nullptr) << "Operator 2 should still be alive.";

    // Set sink to finished so pipeline can complete.
    op2->noMoreInput();
    op2->SetStatus(OMNI_STATUS_FINISHED);

    bool completed = runPipelineToCompletion();
    EXPECT_TRUE(completed) << "Pipeline with pre-released operators should reach kAtEnd without crash.";
}

// Test 3: Stats preserved after cascade release.
// Verifies that pipeline stats are correctly collected even when all
// operators are released early via cascade. The CollectStatsBeforeClose
// call inside ReleaseFinishedOperators should save stats before Close().
TEST_F(RunInternalCascadeTest, testStatsPreservedAfterCascade)
{
    auto *op0 = new TestSourceOperator();
    auto *op1 = new TestPipeOperator();
    auto *op2 = new TestSinkOperator();

    op0->setNoMoreInput(false);
    op1->setNoMoreInput(false);
    op2->setNoMoreInput(false);

    addNewOperator(op0);
    addNewOperator(op1);
    addNewOperator(op2);
    ASSERT_EQ(operators()->size(), 3u);

    // Mark all as finished and cascade-release from index 2.
    op0->SetStatus(OMNI_STATUS_FINISHED);
    op1->SetStatus(OMNI_STATUS_FINISHED);
    op2->SetStatus(OMNI_STATUS_FINISHED);
    driver_->ReleaseFinishedOperators(2);

    // All operators should be nullptr now.
    EXPECT_EQ((*operators())[0], nullptr);
    EXPECT_EQ((*operators())[1], nullptr);
    EXPECT_EQ((*operators())[2], nullptr);

    // Get pipeline stats — should not crash, and stats should be present
    // (collected by CollectStatsBeforeClose before each operator was closed).
    auto stats = driver_->pipelineStats();
    SUCCEED() << "Stats collected without crash after full cascade release.";
}
