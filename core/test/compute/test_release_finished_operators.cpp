/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2024. All rights reserved.
 *
 * Test cases for OmniDriver::ReleaseFinishedOperators and CollectStatsBeforeClose.
 *
 * These are unit-level smoke tests verifying:
 *   1. Both methods compile and are callable on OmniDriver.
 *   2. Nullptr safety (idempotent calls on already-released operators).
 *   3. Normal-path cascade release: finished operators[0..i] are closed and set to nullptr.
 *   4. Early-finish path: releases self + propagates noMoreInput upstream (without releasing upstream).
 *
 * Full behavioral / pipeline integration tests are in separate test files.
 */

#include <gtest/gtest.h>
#include <memory>

#include "compute/driver.h"
#include "operator/operator.h"

using omniruntime::compute::OmniDriver;

// Minimal test operators.
// Use fully-qualified omniruntime::op::Operator to avoid namespace ambiguity
// between expressions::Operator / op::Operator / llvm::Operator.

/// A normal (non-early-finish) operator that becomes isFinished after noMoreInput.
class TestNormalOperator : public omniruntime::op::Operator
{
public:
    TestNormalOperator() : omniruntime::op::Operator() {}

    int32_t AddInput(omniruntime::vec::VectorBatch *vecBatch) override
    {
        return 0;
    }

    int32_t GetOutput(omniruntime::vec::VectorBatch **outputVecBatch) override
    {
        // Once noMoreInput is set, immediately finish.
        if (noMoreInput_) {
            SetStatus(OMNI_STATUS_FINISHED);
        }
        *outputVecBatch = nullptr;
        return 0;
    }
};

/// An early-finish operator (like Limit) that can isFinished before noMoreInput.
class TestEarlyFinishOperator : public omniruntime::op::Operator
{
public:
    TestEarlyFinishOperator() : omniruntime::op::Operator() {}

    bool isEarlyFinish() const override { return true; }

    int32_t AddInput(omniruntime::vec::VectorBatch *vecBatch) override
    {
        return 0;
    }

    int32_t GetOutput(omniruntime::vec::VectorBatch **outputVecBatch) override
    {
        // Simulate early finish: become FINISHED regardless of noMoreInput.
        SetStatus(OMNI_STATUS_FINISHED);
        *outputVecBatch = nullptr;
        return 0;
    }
};

// Test fixture

class ReleaseFinishedOperatorsTest : public ::testing::Test
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

    /// Helper: push a fresh operator into the driver chain via raw pointer.
    /// The driver takes ownership (shared_ptr) of the operator.
    void addNewOperator(omniruntime::op::Operator *rawOp)
    {
        std::unique_ptr<omniruntime::op::Operator> uniqueOp(rawOp);
        driver_->addOperator(std::move(uniqueOp));
    }

    /// Helper: get the operators vector for direct inspection.
    std::vector<std::shared_ptr<omniruntime::op::Operator>> *operators()
    {
        return driver_->operators();
    }
};

// Tests

// Test 1: Both methods exist and compile (smoke test).
// If this compiles, the declarations are correct.
TEST_F(ReleaseFinishedOperatorsTest, testMethodsExistAndCompile)
{
    SUCCEED() << "ReleaseFinishedOperators and CollectStatsBeforeClose are declared and callable.";
}

// Test 2: CollectStatsBeforeClose is null-safe — calling on a nullptr operator doesn't crash.
TEST_F(ReleaseFinishedOperatorsTest, testCollectStatsBeforeClose_NullOperator)
{
    // Create a driver with one operator, then manually null it out.
    addNewOperator(new TestNormalOperator());
    ASSERT_EQ(operators()->size(), 1u);
    (*operators())[0] = nullptr;  // simulate already-released

    // Should not crash — method returns early on nullptr.
    driver_->CollectStatsBeforeClose(0);
    SUCCEED();
}

// Test 3: ReleaseFinishedOperators is null-safe — calling on a nullptr operator doesn't crash.
TEST_F(ReleaseFinishedOperatorsTest, testReleaseFinishedOperators_NullOperator)
{
    addNewOperator(new TestNormalOperator());
    (*operators())[0] = nullptr;

    driver_->ReleaseFinishedOperators(0);
    SUCCEED();
}

// Test 4: Normal-path cascade release — finished operators[0..i] are closed and set to nullptr.
TEST_F(ReleaseFinishedOperatorsTest, testNormalPath_CascadeRelease)
{
    // Build a 3-operator chain: [0]Normal → [1]Normal → [2]Normal
    auto *op0 = new TestNormalOperator();
    auto *op1 = new TestNormalOperator();
    auto *op2 = new TestNormalOperator();

    addNewOperator(op0);
    addNewOperator(op1);
    addNewOperator(op2);
    ASSERT_EQ(operators()->size(), 3u);

    // Set all operators to finished state.
    op0->SetStatus(OMNI_STATUS_FINISHED);
    op1->SetStatus(OMNI_STATUS_FINISHED);
    op2->SetStatus(OMNI_STATUS_FINISHED);

    // Call ReleaseFinishedOperators(2) — should cascade-release all three.
    driver_->ReleaseFinishedOperators(2);

    // Verify all operators are now nullptr (released).
    EXPECT_EQ((*operators())[0], nullptr);
    EXPECT_EQ((*operators())[1], nullptr);
    EXPECT_EQ((*operators())[2], nullptr);
}

// Test 5: Early-finish path — releases self + propagates noMoreInput upstream without releasing upstream.
TEST_F(ReleaseFinishedOperatorsTest, testEarlyFinishPath_ReleaseSelfAndPropagate)
{
    // Build a 3-operator chain: [0]Normal → [1]Normal → [2]EarlyFinish
    auto *op0 = new TestNormalOperator();
    auto *op1 = new TestNormalOperator();
    auto *op2 = new TestEarlyFinishOperator();

    // op2 is early-finish and finished; op0/op1 are NOT finished yet.
    // Set noMoreInput_ = false on op2 to trigger the early-finish path
    // (condition: isEarlyFinish() && !hasNoMoreInput()).
    op0->setNoMoreInput(false);
    op1->setNoMoreInput(false);
    op2->setNoMoreInput(false);
    op2->SetStatus(OMNI_STATUS_FINISHED);

    addNewOperator(op0);
    addNewOperator(op1);
    addNewOperator(op2);
    ASSERT_EQ(operators()->size(), 3u);

    // Call ReleaseFinishedOperators(2) — should take the early-finish path.
    driver_->ReleaseFinishedOperators(2);

    // Verify: op2 (early-finish) is released (nullptr).
    EXPECT_EQ((*operators())[2], nullptr);

    // Verify: op0 and op1 are NOT released (still alive) — early-finish path
    // only releases self, defers upstream release to orphan-drain path.
    EXPECT_NE((*operators())[0], nullptr);
    EXPECT_NE((*operators())[1], nullptr);

    // Verify: noMoreInput was propagated to op0 and op1.
    EXPECT_TRUE(op0->hasNoMoreInput());
    EXPECT_TRUE(op1->hasNoMoreInput());
}

// Test 6: Early-finish with hasNoMoreInput=true takes the normal path.
TEST_F(ReleaseFinishedOperatorsTest, testEarlyFinish_WithNoMoreInput_TakesNormalPath)
{
    // Build: [0]Normal → [1]EarlyFinish (noMoreInput already true)
    auto *op0 = new TestNormalOperator();
    auto *op1 = new TestEarlyFinishOperator();

    op0->setNoMoreInput(false);
    op0->SetStatus(OMNI_STATUS_FINISHED);
    // op1: early-finish, but hasNoMoreInput() == true → normal path
    op1->setNoMoreInput(true);
    op1->SetStatus(OMNI_STATUS_FINISHED);

    addNewOperator(op0);
    addNewOperator(op1);
    ASSERT_EQ(operators()->size(), 2u);

    // Call ReleaseFinishedOperators(1) — op1 is early-finish, but hasNoMoreInput()=true,
    // so it should take the NORMAL path (not the early-finish path).
    driver_->ReleaseFinishedOperators(1);

    // Verify: both operators are released (normal path cascade-releases all finished operators).
    EXPECT_EQ((*operators())[0], nullptr);
    EXPECT_EQ((*operators())[1], nullptr);
}
