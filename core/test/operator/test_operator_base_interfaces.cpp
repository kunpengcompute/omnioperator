/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */
#include <gtest/gtest.h>
#include "operator/operator.h"
#include "compute/operator_stats.h"

using namespace omniruntime::op;
using namespace omniruntime::vec;

// A minimal concrete operator for testing the base class
class TestConcreteOperator : public Operator
{
public:
    TestConcreteOperator() : Operator() {}
    int32_t AddInput(VectorBatch *vecBatch) override
    {
        return OMNI_STATUS_NORMAL;
    }
    int32_t GetOutput(VectorBatch **outputVecBatch) override
    {
        if (noMoreInput_) {
            SetStatus(OMNI_STATUS_FINISHED);
        }
        return 0;
    }
};

class OperatorBaseInterfacesTest : public ::testing::Test
{
protected:
    TestConcreteOperator *op_{nullptr};

    void SetUp() override
    {
        op_ = new TestConcreteOperator();
        op_->setNoMoreInput(false);
    }

    void TearDown() override
    {
        Operator::DeleteOperator(op_);
        op_ = nullptr;
    }
};

// Test 1: isEarlyFinish() default returns false
TEST_F(OperatorBaseInterfacesTest, testIsEarlyFinish_DefaultReturnsFalse)
{
    ASSERT_FALSE(op_->isEarlyFinish());
}

// Test 2: hasNoMoreInput() reflects noMoreInput_ state
TEST_F(OperatorBaseInterfacesTest, testHasNoMoreInput_ReflectsState)
{
    // After setNoMoreInput(false) in SetUp, should be false
    ASSERT_FALSE(op_->hasNoMoreInput());

    op_->noMoreInput();
    ASSERT_TRUE(op_->hasNoMoreInput());
}

// Test 3: isEarlyFinish() can be overridden to true
class TestEarlyFinishOperator : public TestConcreteOperator
{
public:
    bool isEarlyFinish() const override { return true; }
};

TEST_F(OperatorBaseInterfacesTest, testIsEarlyFinish_OverrideReturnsTrue)
{
    TestEarlyFinishOperator earlyOp;
    earlyOp.setNoMoreInput(false);
    ASSERT_TRUE(earlyOp.isEarlyFinish());
}

// Test 4: Default operator is NOT early finish but has hasNoMoreInput access
TEST_F(OperatorBaseInterfacesTest, testDefaultOperatorClassification)
{
    ASSERT_FALSE(op_->isEarlyFinish());
    ASSERT_FALSE(op_->hasNoMoreInput()); // after setNoMoreInput(false)
}
