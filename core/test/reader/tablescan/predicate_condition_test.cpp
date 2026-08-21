/**
 * SQL three-valued logic tests for the legacy and residual PredicateCondition evaluator.
 */

#include <gtest/gtest.h>

#include <initializer_list>
#include <memory>
#include <vector>

#include "reader/common/PredicateCondition.h"
#include "type/data_type.h"
#include "vector/vector.h"

using omniruntime::type::OMNI_INT;
using omniruntime::vec::BaseVector;
using omniruntime::vec::Vector;

namespace {

std::unique_ptr<Vector<int32_t>> MakeIntVector(
    std::initializer_list<int32_t> values, std::initializer_list<int32_t> nullRows)
{
    auto vector = std::make_unique<Vector<int32_t>>(static_cast<int32_t>(values.size()), OMNI_INT);
    int32_t row = 0;
    for (int32_t value : values) {
        vector->SetValue(row++, value);
    }
    for (int32_t nullRow : nullRows) {
        vector->SetNull(nullRow);
    }
    return vector;
}

std::unique_ptr<common::PredicateCondition> Leaf(
    common::PredicateOperatorType op, int32_t column, int32_t value)
{
    return std::make_unique<common::LeafPredicateCondition<int32_t>>(op, column, value);
}

void ExpectResult(const common::PredicateResult &result, int32_t size,
                  std::initializer_list<int32_t> trueRows,
                  std::initializer_list<int32_t> unknownRows)
{
    std::vector<bool> expectedTrue(size, false);
    std::vector<bool> expectedUnknown(size, false);
    for (int32_t row : trueRows) {
        expectedTrue[row] = true;
    }
    for (int32_t row : unknownRows) {
        expectedUnknown[row] = true;
    }

    for (int32_t row = 0; row < size; ++row) {
        EXPECT_EQ(omniruntime::BitUtil::IsBitSet(result.trueBits, row), expectedTrue[row]) << "row " << row;
        EXPECT_EQ(omniruntime::BitUtil::IsBitSet(result.unknownBits, row), expectedUnknown[row]) << "row " << row;
        EXPECT_FALSE(omniruntime::BitUtil::IsBitSet(result.trueBits, row) &&
                     omniruntime::BitUtil::IsBitSet(result.unknownBits, row)) << "row " << row;
    }

    if ((size & 7) != 0) {
        int32_t lastByte = omniruntime::BitUtil::Nbytes(size) - 1;
        uint8_t invalidBits = static_cast<uint8_t>(~common::validBitsMask(size, lastByte));
        EXPECT_EQ(result.trueBits[lastByte] & invalidBits, 0);
        EXPECT_EQ(result.unknownBits[lastByte] & invalidBits, 0);
    }
}

} // namespace

TEST(PredicateConditionTest, SameColumnOrKeepsNullAsUnknown)
{
    // A11: c_int < 3 OR c_int > 10. Row 5 has a raw value of 0 but is NULL;
    // the raw value must not make the row pass the predicate.
    auto values = MakeIntVector({1, 2, 3, 6, 11, 0, 1, -5, 7}, {5});
    std::vector<BaseVector *> batch{values.get()};
    common::OrPredicateCondition condition(
        Leaf(common::LESS_THAN, 0, 3), Leaf(common::GREATER_THAN, 0, 10));
    condition.init(static_cast<int32_t>(values->GetSize()));

    auto result = condition.compute(batch);
    ExpectResult(result, values->GetSize(), {0, 1, 4, 6, 7}, {5});
}

TEST(PredicateConditionTest, MultiRangeAndKeepsNullAsUnknown)
{
    // A21: (c_int < 3 OR c_int > 10) AND (c_int < 5 OR c_int > 8).
    auto values = MakeIntVector({1, 2, 3, 6, 11, 0, 1, -5, 7}, {5});
    std::vector<BaseVector *> batch{values.get()};
    common::AndPredicateCondition condition(
        std::make_unique<common::OrPredicateCondition>(
            Leaf(common::LESS_THAN, 0, 3), Leaf(common::GREATER_THAN, 0, 10)),
        std::make_unique<common::OrPredicateCondition>(
            Leaf(common::LESS_THAN, 0, 5), Leaf(common::GREATER_THAN, 0, 8)));
    condition.init(static_cast<int32_t>(values->GetSize()));

    auto result = condition.compute(batch);
    ExpectResult(result, values->GetSize(), {0, 1, 4, 6, 7}, {5});
}

TEST(PredicateConditionTest, CrossColumnOrUsesSqlThreeValuedLogic)
{
    // Rows: (NULL,NULL), (NULL,0), (NULL,20), (0,NULL), (20,NULL),
    //       (0,0), (0,20), (20,0), (20,20).
    auto a = MakeIntVector({0, 0, 0, 0, 20, 0, 0, 20, 20}, {0, 1, 2});
    auto b = MakeIntVector({0, 0, 20, 0, 0, 0, 20, 0, 20}, {0, 3, 4});
    std::vector<BaseVector *> batch{a.get(), b.get()};
    common::OrPredicateCondition condition(
        Leaf(common::LESS_THAN, 0, 3), Leaf(common::GREATER_THAN, 1, 10));
    condition.init(static_cast<int32_t>(a->GetSize()));

    auto result = condition.compute(batch);
    ExpectResult(result, a->GetSize(), {2, 3, 5, 6, 8}, {0, 1, 4});
}

TEST(PredicateConditionTest, CrossColumnAndUsesSqlThreeValuedLogic)
{
    auto a = MakeIntVector({0, 0, 0, 0, 20, 0, 0, 20, 20}, {0, 1, 2});
    auto b = MakeIntVector({0, 0, 20, 0, 0, 0, 20, 0, 20}, {0, 3, 4});
    std::vector<BaseVector *> batch{a.get(), b.get()};
    common::AndPredicateCondition condition(
        Leaf(common::LESS_THAN, 0, 3), Leaf(common::GREATER_THAN, 1, 10));
    condition.init(static_cast<int32_t>(a->GetSize()));

    auto result = condition.compute(batch);
    ExpectResult(result, a->GetSize(), {6}, {0, 2, 3});
}

TEST(PredicateConditionTest, NotAndUsesSqlThreeValuedLogic)
{
    // NOT(a > 6 AND b > 10) over the same NULL cross-product as the OR test.
    auto a = MakeIntVector({0, 0, 0, 0, 20, 0, 0, 20, 20}, {0, 1, 2});
    auto b = MakeIntVector({0, 0, 20, 0, 0, 0, 20, 0, 20}, {0, 3, 4});
    std::vector<BaseVector *> batch{a.get(), b.get()};
    common::NotPredicateCondition condition(
        std::make_unique<common::AndPredicateCondition>(
            Leaf(common::GREATER_THAN, 0, 6), Leaf(common::GREATER_THAN, 1, 10)));
    condition.init(static_cast<int32_t>(a->GetSize()));

    auto result = condition.compute(batch);
    ExpectResult(result, a->GetSize(), {1, 3, 5, 6, 7}, {0, 2, 4});
}

TEST(PredicateConditionTest, NotOrUsesSqlThreeValuedLogic)
{
    auto a = MakeIntVector({0, 0, 0, 0, 20, 0, 0, 20, 20}, {0, 1, 2});
    auto b = MakeIntVector({0, 0, 20, 0, 0, 0, 20, 0, 20}, {0, 3, 4});
    std::vector<BaseVector *> batch{a.get(), b.get()};
    common::NotPredicateCondition condition(
        std::make_unique<common::OrPredicateCondition>(
            Leaf(common::LESS_THAN, 0, 3), Leaf(common::GREATER_THAN, 1, 10)));
    condition.init(static_cast<int32_t>(a->GetSize()));

    auto result = condition.compute(batch);
    ExpectResult(result, a->GetSize(), {7}, {0, 1, 4});
}

TEST(PredicateConditionTest, ExplicitNullPredicatesComposeWithComparisons)
{
    auto a = MakeIntVector({0, 0, 0, 0, 20, 0, 0, 20, 20}, {0, 1, 2});
    auto b = MakeIntVector({0, 0, 20, 0, 0, 0, 20, 0, 20}, {0, 3, 4});
    std::vector<BaseVector *> batch{a.get(), b.get()};

    common::OrPredicateCondition nullOrComparison(
        Leaf(common::IS_NULL, 0, 0), Leaf(common::GREATER_THAN, 1, 10));
    nullOrComparison.init(static_cast<int32_t>(a->GetSize()));
    ExpectResult(nullOrComparison.compute(batch), a->GetSize(), {0, 1, 2, 6, 8}, {3, 4});

    common::AndPredicateCondition notNullAndComparison(
        Leaf(common::IS_NOT_NULL, 0, 0), Leaf(common::GREATER_THAN, 1, 10));
    notNullAndComparison.init(static_cast<int32_t>(a->GetSize()));
    ExpectResult(notNullAndComparison.compute(batch), a->GetSize(), {6, 8}, {3, 4});
}

TEST(PredicateConditionTest, NullPredicatesNeverProduceUnknown)
{
    auto values = MakeIntVector({0, 1, 2, 3, 4, 5, 6, 7, 0}, {0, 8});
    std::vector<BaseVector *> batch{values.get()};

    common::LeafPredicateCondition<int32_t> isNull(common::IS_NULL, 0, 0);
    isNull.init(static_cast<int32_t>(values->GetSize()));
    ExpectResult(isNull.compute(batch), values->GetSize(), {0, 8}, {});

    common::LeafPredicateCondition<int32_t> isNotNull(common::IS_NOT_NULL, 0, 0);
    isNotNull.init(static_cast<int32_t>(values->GetSize()));
    ExpectResult(isNotNull.compute(batch), values->GetSize(), {1, 2, 3, 4, 5, 6, 7}, {});
}

TEST(PredicateConditionTest, ConstantPredicatesInitializeBothBitmaps)
{
    auto values = MakeIntVector({0, 1, 2, 3, 4, 5, 6, 7, 8}, {});
    std::vector<BaseVector *> batch{values.get()};

    common::LeafPredicateCondition<int8_t> alwaysTrue(common::TRUE, 0, 1);
    alwaysTrue.init(static_cast<int32_t>(values->GetSize()));
    ExpectResult(alwaysTrue.compute(batch), values->GetSize(), {0, 1, 2, 3, 4, 5, 6, 7, 8}, {});

    common::LeafPredicateCondition<int8_t> alwaysFalse(common::FALSE, 0, 0);
    alwaysFalse.init(static_cast<int32_t>(values->GetSize()));
    ExpectResult(alwaysFalse.compute(batch), values->GetSize(), {}, {});
}

TEST(PredicateConditionTest, AllNullAndNoNullBatchesDoNotReadNullSlotsAsValues)
{
    auto allNull = MakeIntVector({0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 1, 2, 3, 4, 5, 6, 7, 8});
    std::vector<BaseVector *> allNullBatch{allNull.get()};
    auto allNullCondition = Leaf(common::LESS_THAN, 0, 3);
    allNullCondition->init(static_cast<int32_t>(allNull->GetSize()));
    ExpectResult(allNullCondition->compute(allNullBatch), allNull->GetSize(), {}, {0, 1, 2, 3, 4, 5, 6, 7, 8});

    auto noNull = MakeIntVector({-1, 0, 1, 2, 3, 4, 5, 6, 7}, {});
    std::vector<BaseVector *> noNullBatch{noNull.get()};
    auto noNullCondition = Leaf(common::LESS_THAN, 0, 3);
    noNullCondition->init(static_cast<int32_t>(noNull->GetSize()));
    ExpectResult(noNullCondition->compute(noNullBatch), noNull->GetSize(), {0, 1, 2, 3}, {});
}

TEST(PredicateConditionTest, CompositePredicateClearsTailBitsAfterNeonPath)
{
    constexpr int32_t size = 127;
    auto values = std::make_unique<Vector<int32_t>>(size, OMNI_INT);
    for (int32_t row = 0; row < size; ++row) {
        values->SetValue(row, 0);
    }
    values->SetNull(size - 1);
    std::vector<BaseVector *> batch{values.get()};
    common::OrPredicateCondition condition(
        Leaf(common::LESS_THAN, 0, 3), Leaf(common::GREATER_THAN, 0, 10));
    condition.init(size);

    auto result = condition.compute(batch);
    for (int32_t row = 0; row < size - 1; ++row) {
        EXPECT_TRUE(omniruntime::BitUtil::IsBitSet(result.trueBits, row)) << "row " << row;
        EXPECT_FALSE(omniruntime::BitUtil::IsBitSet(result.unknownBits, row)) << "row " << row;
    }
    EXPECT_FALSE(omniruntime::BitUtil::IsBitSet(result.trueBits, size - 1));
    EXPECT_TRUE(omniruntime::BitUtil::IsBitSet(result.unknownBits, size - 1));
    EXPECT_FALSE(omniruntime::BitUtil::IsBitSet(result.trueBits, size));
    EXPECT_FALSE(omniruntime::BitUtil::IsBitSet(result.unknownBits, size));
}
