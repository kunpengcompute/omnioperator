/**
 * T0 unit tests for common::Filter (testInt64 / mergeWith).
 */

#include <gtest/gtest.h>
#include <limits>
#include <unordered_set>
#include <vector>

#include "reader/common/Filter.h"

using namespace common;

namespace {

constexpr int64_t kMin = std::numeric_limits<int64_t>::min();
constexpr int64_t kMax = std::numeric_limits<int64_t>::max();

void ExpectPasses(const Filter &f, std::initializer_list<int64_t> yes,
                   std::initializer_list<int64_t> no)
{
    for (int64_t v : yes) {
        EXPECT_TRUE(f.testInt64(v)) << "expected pass: " << v;
    }
    for (int64_t v : no) {
        EXPECT_FALSE(f.testInt64(v)) << "expected reject: " << v;
    }
}

} // namespace

TEST(FilterTest, BigintRangeTestInt64)
{
    BigintRange eq(10, 10, false);
    EXPECT_TRUE(eq.testInt64(10));
    EXPECT_FALSE(eq.testInt64(9));
    EXPECT_FALSE(eq.testNull());

    BigintRange gt(11, kMax, false); // > 10
    EXPECT_TRUE(gt.testInt64(11));
    EXPECT_FALSE(gt.testInt64(10));

    BigintRange between(3, 6, false);
    EXPECT_TRUE(between.testInt64(3));
    EXPECT_TRUE(between.testInt64(6));
    EXPECT_FALSE(between.testInt64(2));
    EXPECT_FALSE(between.testInt64(7));
}

TEST(FilterTest, NegatedAndMultiRangeTestInt64)
{
    NegatedBigintRange ne(6, 6, false); // != 6
    EXPECT_TRUE(ne.testInt64(5));
    EXPECT_FALSE(ne.testInt64(6));

    BigintMultiRange multi({{kMin, 2}, {11, kMax}}, false); // < 3 OR > 10
    EXPECT_TRUE(multi.testInt64(2));
    EXPECT_TRUE(multi.testInt64(11));
    EXPECT_FALSE(multi.testInt64(3));
    EXPECT_FALSE(multi.testInt64(10));
}

TEST(FilterTest, NullFilters)
{
    IsNull isNull;
    EXPECT_TRUE(isNull.testNull());
    EXPECT_FALSE(isNull.testInt64(1));

    IsNotNull isNotNull;
    EXPECT_FALSE(isNotNull.testNull());
    EXPECT_TRUE(isNotNull.testInt64(1));
}

TEST(FilterTest, MergeWithRangeIntersection)
{
    auto a = std::make_shared<BigintRange>(0, 100, false);
    auto b = std::make_shared<BigintRange>(50, 200, false);
    auto merged = a->mergeWith(b.get());
    ASSERT_NE(merged, nullptr);
    ASSERT_TRUE(merged->is(FilterKind::kBigintRange));
    auto *r = static_cast<BigintRange *>(merged.get());
    EXPECT_EQ(r->lower(), 50);
    EXPECT_EQ(r->upper(), 100);
    ExpectPasses(*merged, {50, 100}, {49, 101});
}

TEST(FilterTest, MergeWithContradictionAlwaysFalse)
{
    auto a = std::make_shared<BigintRange>(0, 10, false);
    auto b = std::make_shared<BigintRange>(20, 30, false);
    auto merged = a->mergeWith(b.get());
    ASSERT_NE(merged, nullptr);
    EXPECT_TRUE(merged->is(FilterKind::kAlwaysFalse));
    EXPECT_FALSE(merged->testInt64(5));
    EXPECT_FALSE(merged->testInt64(25));
}

TEST(FilterTest, MergeWithIsNullContradiction)
{
    auto range = std::make_shared<BigintRange>(1, 10, false);
    auto isNull = std::make_shared<IsNull>();
    auto merged = range->mergeWith(isNull.get());
    ASSERT_NE(merged, nullptr);
    EXPECT_TRUE(merged->is(FilterKind::kAlwaysFalse));
}

TEST(FilterTest, MergeWithIsNotNullKeepsRange)
{
    auto range = std::make_shared<BigintRange>(1, 10, true);
    auto isNotNull = std::make_shared<IsNotNull>();
    auto merged = range->mergeWith(isNotNull.get());
    ASSERT_NE(merged, nullptr);
    ASSERT_TRUE(merged->is(FilterKind::kBigintRange));
    EXPECT_FALSE(merged->nullAllowed());
    EXPECT_TRUE(merged->testInt64(5));
}

TEST(FilterTest, MergeWithAlwaysTrueClonesOther)
{
    auto alwaysTrue = std::make_shared<AlwaysTrue>();
    auto range = std::make_shared<BigintRange>(5, 5, false);
    auto merged = alwaysTrue->mergeWith(range.get());
    ASSERT_NE(merged, nullptr);
    ASSERT_TRUE(merged->is(FilterKind::kBigintRange));
    EXPECT_TRUE(merged->testInt64(5));
}

TEST(FilterTest, MergeWithRangeAndNegated)
{
    // [1,10] AND != 5  => [1,4] U [6,10]
    auto range = std::make_shared<BigintRange>(1, 10, false);
    auto negated = std::make_shared<NegatedBigintRange>(5, 5, false);
    auto merged = range->mergeWith(negated.get());
    ASSERT_NE(merged, nullptr);
    ASSERT_TRUE(merged->is(FilterKind::kBigintMultiRange));
    ExpectPasses(*merged, {1, 4, 6, 10}, {5, 0, 11});

    // Symmetric: Negated.mergeWith(Range) yields the same
    auto merged2 = negated->mergeWith(range.get());
    ASSERT_NE(merged2, nullptr);
    ExpectPasses(*merged2, {1, 4, 6, 10}, {5, 0, 11});

    // Negated range fully covers positive range => AlwaysFalse
    auto narrow = std::make_shared<BigintRange>(5, 5, false);
    auto merged3 = narrow->mergeWith(negated.get());
    ASSERT_NE(merged3, nullptr);
    EXPECT_TRUE(merged3->is(FilterKind::kAlwaysFalse));
}

TEST(FilterTest, MergeWithRangeAndMulti)
{
    // [0,10] AND ([kMin,2] U [8,kMax]) => [0,2] U [8,10]
    auto range = std::make_shared<BigintRange>(0, 10, false);
    auto multi = std::make_shared<BigintMultiRange>(
        std::vector<std::pair<int64_t, int64_t>>{{kMin, 2}, {8, kMax}}, false);
    auto merged = range->mergeWith(multi.get());
    ASSERT_NE(merged, nullptr);
    ASSERT_TRUE(merged->is(FilterKind::kBigintMultiRange));
    ExpectPasses(*merged, {0, 2, 8, 10}, {3, 7, 11});

    auto merged2 = multi->mergeWith(range.get());
    ASSERT_NE(merged2, nullptr);
    ExpectPasses(*merged2, {0, 2, 8, 10}, {3, 7, 11});
}

TEST(FilterTest, MergeWithValuesReturnsNullptr)
{
    // Gluten does not push IN yet: Values merge with ranges returns nullptr → residual
    auto range = std::make_shared<BigintRange>(1, 10, false);
    auto values = std::make_shared<BigintValues>(std::unordered_set<int64_t>{1, 2, 3}, false);
    auto multi = std::make_shared<BigintMultiRange>(
        std::vector<std::pair<int64_t, int64_t>>{{1, 2}, {8, 9}}, false);
    auto negated = std::make_shared<NegatedBigintRange>(5, 5, false);
    auto values2 = std::make_shared<BigintValues>(std::unordered_set<int64_t>{2, 3}, false);

    EXPECT_EQ(range->mergeWith(values.get()), nullptr);
    EXPECT_EQ(values->mergeWith(range.get()), nullptr);
    EXPECT_EQ(negated->mergeWith(values.get()), nullptr);
    EXPECT_EQ(values->mergeWith(negated.get()), nullptr);
    EXPECT_EQ(multi->mergeWith(values.get()), nullptr);
    EXPECT_EQ(values->mergeWith(multi.get()), nullptr);
    EXPECT_EQ(values->mergeWith(values2.get()), nullptr);
}

TEST(FilterTest, MergeWithNegatedAndNegated)
{
    // != 1 AND != 5: non-adjacent negated ranges → three positive ranges
    auto a = std::make_shared<NegatedBigintRange>(1, 1, false);
    auto b = std::make_shared<NegatedBigintRange>(5, 5, false);
    auto merged = a->mergeWith(b.get());
    ASSERT_NE(merged, nullptr);
    ExpectPasses(*merged, {0, 2, 4, 6}, {1, 5});

    // != 3 AND != 4: adjacent → merge into Negated[3,4]
    auto c = std::make_shared<NegatedBigintRange>(3, 3, false);
    auto d = std::make_shared<NegatedBigintRange>(4, 4, false);
    auto merged2 = c->mergeWith(d.get());
    ASSERT_NE(merged2, nullptr);
    ASSERT_TRUE(merged2->is(FilterKind::kNegatedBigintRange));
    auto *n = static_cast<NegatedBigintRange *>(merged2.get());
    EXPECT_EQ(n->lower(), 3);
    EXPECT_EQ(n->upper(), 4);
    ExpectPasses(*merged2, {2, 5}, {3, 4});
}

TEST(FilterTest, MergeWithNegatedAndMulti)
{
    // != 5 AND ([1,3] U [7,10]) => unchanged (5 in neither segment)
    auto negated = std::make_shared<NegatedBigintRange>(5, 5, false);
    auto multi = std::make_shared<BigintMultiRange>(
        std::vector<std::pair<int64_t, int64_t>>{{1, 3}, {7, 10}}, false);
    auto merged = negated->mergeWith(multi.get());
    ASSERT_NE(merged, nullptr);
    ExpectPasses(*merged, {1, 3, 7, 10}, {0, 5, 6, 11});

    // != 8 AND same => [1,3] U [7,7] U [9,10]
    auto negated2 = std::make_shared<NegatedBigintRange>(8, 8, false);
    auto merged2 = negated2->mergeWith(multi.get());
    ASSERT_NE(merged2, nullptr);
    ExpectPasses(*merged2, {1, 3, 7, 9, 10}, {8, 0, 6, 11});
}

TEST(FilterTest, MergeWithMultiAndMulti)
{
    // ([kMin,2] U [11,kMax]) AND ([0,5] U [9,15])
    // => [0,2] U [11,15]
    auto a = std::make_shared<BigintMultiRange>(
        std::vector<std::pair<int64_t, int64_t>>{{kMin, 2}, {11, kMax}}, false);
    auto b = std::make_shared<BigintMultiRange>(
        std::vector<std::pair<int64_t, int64_t>>{{0, 5}, {9, 15}}, false);
    auto merged = a->mergeWith(b.get());
    ASSERT_NE(merged, nullptr);
    ExpectPasses(*merged, {0, 2, 11, 15}, {-1, 3, 9, 10, 16});
}

TEST(FilterTest, TestInt64RangeStatsPrune)
{
    BigintRange range(10, 20, false);
    EXPECT_TRUE(range.testInt64Range(5, 15, false));
    EXPECT_FALSE(range.testInt64Range(21, 30, false));
    // nullAllowed=false: hasNull alone must not keep a disjoint range
    EXPECT_FALSE(range.testInt64Range(21, 30, true));

    BigintRange nullable(10, 20, true);
    EXPECT_TRUE(nullable.testInt64Range(21, 30, true));
}
