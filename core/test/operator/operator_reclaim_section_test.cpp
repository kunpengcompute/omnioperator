#include <stdexcept>

#include "gtest/gtest.h"
#include "operator/operator.h"
#include "operator/reclaim_context.h"

namespace omniruntime::op {
namespace {
class GuardTestOperator final : public Operator {
public:
    GuardTestOperator() : Operator(nullptr) {}
    int32_t AddInput(vec::VectorBatch *) override { return 0; }
    int32_t GetOutput(vec::VectorBatch **) override { return 0; }
    void forceNonReclaimable(bool value)
    {
        nonReclaimableSection_.store(value, std::memory_order_release);
    }
};
}

TEST(OperatorReclaimSection, DisabledGuardLeavesOperatorAndTlsUntouched)
{
    GuardTestOperator outer;
    GuardTestOperator inner;
    outer.setReclaimGuardEnabled(true);

    ActiveOperatorCallSection active(&outer, "outer");
    ASSERT_EQ(CurrentActiveOperatorCall().op, &outer);

    inner.forceNonReclaimable(true);
    {
        ActiveOperatorCallSection disabledActive(&inner, "inner", nullptr, false);
        Operator::ReclaimableReservationSection disabledReservation(&inner);
        EXPECT_EQ(CurrentActiveOperatorCall().op, &outer);
        EXPECT_TRUE(inner.isInNonReclaimableSection());
    }

    inner.forceNonReclaimable(false);
    {
        Operator::NonReclaimableSection disabledDispatch(&inner);
        EXPECT_FALSE(inner.isInNonReclaimableSection());
        EXPECT_EQ(CurrentActiveOperatorCall().op, &outer);
    }
}

TEST(OperatorReclaimSection, ActiveCallIsNonReclaimableExceptReservationWindow)
{
    GuardTestOperator op;
    op.setReclaimGuardEnabled(true);
    EXPECT_FALSE(op.isInNonReclaimableSection());
    {
        ActiveOperatorCallSection active(&op, "addInput");
        Operator::NonReclaimableSection dispatch(&op);
        EXPECT_EQ(CurrentActiveOperatorCall().op, &op);
        EXPECT_TRUE(op.isInNonReclaimableSection());
        {
            Operator::ReclaimableReservationSection reservation(&op);
            EXPECT_FALSE(op.isInNonReclaimableSection());
        }
        EXPECT_TRUE(op.isInNonReclaimableSection());
    }
    EXPECT_EQ(CurrentActiveOperatorCall().op, nullptr);
    EXPECT_FALSE(op.isInNonReclaimableSection());
}

TEST(OperatorReclaimSection, ExceptionRestoresNestedStateAndActiveMarker)
{
    GuardTestOperator op;
    op.setReclaimGuardEnabled(true);
    try {
        ActiveOperatorCallSection active(&op, "addInput");
        Operator::NonReclaimableSection dispatch(&op);
        Operator::ReclaimableReservationSection reservation(&op);
        throw std::runtime_error("injected");
    } catch (const std::runtime_error &) {
    }
    EXPECT_FALSE(op.isInNonReclaimableSection());
    EXPECT_EQ(CurrentActiveOperatorCall().op, nullptr);
    EXPECT_EQ(CurrentActiveOperatorCall().method, nullptr);
}
} // namespace omniruntime::op
