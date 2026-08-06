/**
 * T0 unit tests for ScanSpecBuilder: JSON → ScanSpec/Filter + residual.
 */

#include <gtest/gtest.h>
#include <limits>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <utility>
#include <vector>

#include "reader/common/Filter.h"
#include "reader/common/PredicateOperatorType.h"
#include "reader/common/ScanSpecBuilder.h"
#include "type/data_type.h"
#include "util/type_util.h"

using omniruntime::reader::allSelectedColumnsAreSupported;
using omniruntime::reader::makeScanSpec;
using omniruntime::type::BooleanType;
using omniruntime::type::ByteType;
using omniruntime::type::CharType;
using omniruntime::type::Date32Type;
using omniruntime::type::Decimal64Type;
using omniruntime::type::Decimal128Type;
using omniruntime::type::DoubleType;
using omniruntime::type::FloatType;
using omniruntime::type::IntType;
using omniruntime::type::LongType;
using omniruntime::type::OMNI_DATE32;
using omniruntime::type::OMNI_CHAR;
using omniruntime::type::OMNI_INT;
using omniruntime::type::OMNI_VARCHAR;
using omniruntime::type::ROW;
using omniruntime::type::ShortType;
using omniruntime::type::TimestampType;
using omniruntime::type::VarBinaryType;
using omniruntime::type::VarcharType;
using ::common::FilterKind;
using ::common::PredicateOperatorType;

namespace {

constexpr int64_t kMin = std::numeric_limits<int64_t>::min();
constexpr int64_t kMax = std::numeric_limits<int64_t>::max();

nlohmann::json Leaf(PredicateOperatorType op, int index, omniruntime::type::DataTypeId typeId,
                    const std::string &value = "0")
{
    nlohmann::json j;
    j["op"] = static_cast<int>(op);
    j["index"] = index;
    j["dataType"] = static_cast<int>(typeId);
    j["value"] = value;
    return j;
}

nlohmann::json And(nlohmann::json left, nlohmann::json right)
{
    nlohmann::json j;
    j["op"] = static_cast<int>(::common::AND);
    j["left"] = std::move(left);
    j["right"] = std::move(right);
    return j;
}

nlohmann::json Or(nlohmann::json left, nlohmann::json right)
{
    nlohmann::json j;
    j["op"] = static_cast<int>(::common::OR);
    j["left"] = std::move(left);
    j["right"] = std::move(right);
    return j;
}

nlohmann::json Not(nlohmann::json child)
{
    nlohmann::json j;
    j["op"] = static_cast<int>(::common::NOT);
    j["child"] = std::move(child);
    return j;
}

std::shared_ptr<nlohmann::json> WrapEnhancement(const nlohmann::json &cond)
{
    auto root = std::make_shared<nlohmann::json>();
    (*root)["vecPredicateCondition"] = cond.dump();
    return root;
}

auto IntRowType2()
{
    return ROW({"a", "b"}, {IntType(), IntType()});
}

} // namespace

TEST(ScanSpecBuilderTest, AllSelectedColumnsAreSupported)
{
    auto ints = ROW({"a", "b", "c", "d"}, {IntType(), LongType(), ShortType(), Date32Type()});
    EXPECT_TRUE(allSelectedColumnsAreSupported(*ints));

    auto withVarchar = ROW({"a", "b"}, {IntType(), VarcharType()});
    EXPECT_TRUE(allSelectedColumnsAreSupported(*withVarchar));

    auto withChar = ROW({"a", "b"}, {IntType(), CharType(40)});
    EXPECT_TRUE(allSelectedColumnsAreSupported(*withChar));

    auto allScalars = ROW(
        {"bool", "double", "decimal64", "decimal128", "byte", "float", "binary", "timestamp"},
        {BooleanType(), DoubleType(), Decimal64Type(18, 2), Decimal128Type(38, 4), ByteType(), FloatType(),
         VarBinaryType(), TimestampType()});
    EXPECT_TRUE(allSelectedColumnsAreSupported(*allScalars));

    auto withArray = ROW({"a"}, {omniruntime::type::ArrayDataType(IntType())});
    EXPECT_FALSE(allSelectedColumnsAreSupported(*withArray));
}

TEST(ScanSpecBuilderTest, PushBooleanAndDouble)
{
    auto rowType = ROW({"b", "d"}, {BooleanType(), DoubleType()});
    bool usable = false;
    bool needResidual = false;
    std::shared_ptr<::common::PredicateCondition> residual;

    auto condition = And(Leaf(::common::EQUAL_TO, 0, omniruntime::type::OMNI_BOOLEAN, "true"),
                         Leaf(::common::GREATER_THAN, 1, omniruntime::type::OMNI_DOUBLE, "1.5"));
    auto spec = makeScanSpec(*rowType, WrapEnhancement(condition), usable, needResidual, residual);

    ASSERT_TRUE(usable);
    EXPECT_FALSE(needResidual);
    ASSERT_TRUE(spec->children()[0]->filter()->is(FilterKind::kBoolValue));
    EXPECT_TRUE(spec->children()[0]->filter()->testBool(true));
    EXPECT_FALSE(spec->children()[0]->filter()->testBool(false));
    ASSERT_TRUE(spec->children()[1]->filter()->is(FilterKind::kDoubleRange));
    EXPECT_FALSE(spec->children()[1]->filter()->testDouble(1.5));
    EXPECT_TRUE(spec->children()[1]->filter()->testDouble(1.5001));
}

TEST(ScanSpecBuilderTest, ProjectionOnlyPredicatesDoNotCreateFilters)
{
    const std::vector<std::pair<std::shared_ptr<omniruntime::type::DataType>, omniruntime::type::DataTypeId>> types = {
        {FloatType(), omniruntime::type::OMNI_FLOAT},
        {Decimal64Type(18, 5), omniruntime::type::OMNI_DECIMAL64},
        {Decimal128Type(38, 8), omniruntime::type::OMNI_DECIMAL128},
        {VarBinaryType(), omniruntime::type::OMNI_VARBINARY},
        {TimestampType(), omniruntime::type::OMNI_TIMESTAMP}};

    for (const auto &[type, typeId] : types) {
        auto rowType = ROW({"value"}, {type});
        bool usable = false;
        bool needResidual = false;
        std::shared_ptr<::common::PredicateCondition> residual;
        auto spec = makeScanSpec(*rowType, WrapEnhancement(Leaf(::common::EQUAL_TO, 0, typeId, "1")),
                                 usable, needResidual, residual);

        // Gluten does not currently generate these filter types. If one is
        // received, no leaf filter is attached and the existing fallback gate
        // keeps the scan on the legacy path.
        EXPECT_FALSE(usable) << static_cast<int>(typeId);
        EXPECT_FALSE(needResidual) << static_cast<int>(typeId);
        EXPECT_EQ(residual, nullptr) << static_cast<int>(typeId);
        EXPECT_FALSE(spec->children()[0]->hasFilter()) << static_cast<int>(typeId);
    }
}

TEST(ScanSpecBuilderTest, ProjectionOnlyTypesPushTypeIndependentNullFilter)
{
    const std::vector<std::shared_ptr<omniruntime::type::DataType>> types = {
        FloatType(), Decimal64Type(18, 2), Decimal128Type(38, 8), VarBinaryType(), TimestampType()};

    for (const auto &type : types) {
        auto rowType = ROW({"value"}, {type});
        bool usable = false;
        bool needResidual = false;
        std::shared_ptr<::common::PredicateCondition> residual;

        // Gluten uses OMNI_INT as a sentinel. Null filters are type independent
        // and operate only on the actual column's null bitmap.
        auto spec = makeScanSpec(*rowType, WrapEnhancement(Leaf(::common::IS_NULL, 0, OMNI_INT, "-1")),
                                 usable, needResidual, residual);

        ASSERT_TRUE(usable) << static_cast<int>(type->GetId());
        EXPECT_FALSE(needResidual) << static_cast<int>(type->GetId());
        EXPECT_EQ(residual, nullptr) << static_cast<int>(type->GetId());
        ASSERT_TRUE(spec->children()[0]->hasFilter()) << static_cast<int>(type->GetId());
        EXPECT_TRUE(spec->children()[0]->filter()->is(FilterKind::kIsNull));
    }
}

TEST(ScanSpecBuilderTest, MixedUnsupportedPredicateFallsBackSafely)
{
    auto rowType = ROW({"byte", "float"}, {ByteType(), FloatType()});
    bool usable = false;
    bool needResidual = false;
    std::shared_ptr<::common::PredicateCondition> residual;

    auto condition = And(Leaf(::common::GREATER_THAN_OR_EQUAL, 0, omniruntime::type::OMNI_BYTE, "-7"),
                         Leaf(::common::LESS_THAN, 1, omniruntime::type::OMNI_FLOAT, "2.5"));
    auto spec = makeScanSpec(*rowType, WrapEnhancement(condition), usable, needResidual, residual);

    EXPECT_FALSE(usable);
    EXPECT_FALSE(needResidual);
    EXPECT_EQ(residual, nullptr);
    ASSERT_TRUE(spec->children()[0]->hasFilter());
    EXPECT_TRUE(spec->children()[0]->filter()->testInt64(-7));
    EXPECT_FALSE(spec->children()[0]->filter()->testInt64(-8));
    EXPECT_FALSE(spec->children()[1]->hasFilter());
}

TEST(ScanSpecBuilderTest, PushEqualAndRange)
{
    auto rowType = IntRowType2();
    bool usable = false;
    bool needResidual = false;
    std::shared_ptr<::common::PredicateCondition> residual;

    auto enh = WrapEnhancement(Leaf(::common::EQUAL_TO, 0, OMNI_INT, "2001"));
    auto spec = makeScanSpec(*rowType, enh, usable, needResidual, residual);
    ASSERT_TRUE(usable);
    EXPECT_FALSE(needResidual);
    EXPECT_EQ(residual, nullptr);
    ASSERT_TRUE(spec->hasAnyLeafFilter());
    auto *f = spec->children()[0]->filter();
    ASSERT_NE(f, nullptr);
    ASSERT_TRUE(f->is(FilterKind::kBigintRange));
    auto *br = static_cast<::common::BigintRange *>(f);
    EXPECT_EQ(br->lower(), 2001);
    EXPECT_EQ(br->upper(), 2001);
    EXPECT_TRUE(f->testInt64(2001));
    EXPECT_FALSE(f->testInt64(2000));
}

TEST(ScanSpecBuilderTest, PushGreaterThan)
{
    auto rowType = IntRowType2();
    bool usable = false;
    bool needResidual = false;
    std::shared_ptr<::common::PredicateCondition> residual;

    auto enh = WrapEnhancement(Leaf(::common::GREATER_THAN, 0, OMNI_INT, "2000"));
    auto spec = makeScanSpec(*rowType, enh, usable, needResidual, residual);
    ASSERT_TRUE(usable);
    EXPECT_FALSE(needResidual);
    auto *f = spec->children()[0]->filter();
    ASSERT_TRUE(f->is(FilterKind::kBigintRange));
    auto *br = static_cast<::common::BigintRange *>(f);
    EXPECT_EQ(br->lower(), 2001);
    EXPECT_EQ(br->upper(), kMax);
}

TEST(ScanSpecBuilderTest, PushGreaterThanOrEqual)
{
    auto rowType = IntRowType2();
    bool usable = false;
    bool needResidual = false;
    std::shared_ptr<::common::PredicateCondition> residual;

    auto enh = WrapEnhancement(Leaf(::common::GREATER_THAN_OR_EQUAL, 0, OMNI_INT, "2001"));
    auto spec = makeScanSpec(*rowType, enh, usable, needResidual, residual);
    ASSERT_TRUE(usable);
    EXPECT_FALSE(needResidual);
    auto *f = spec->children()[0]->filter();
    ASSERT_TRUE(f->is(FilterKind::kBigintRange));
    auto *br = static_cast<::common::BigintRange *>(f);
    EXPECT_EQ(br->lower(), 2001);
    EXPECT_EQ(br->upper(), kMax);
    EXPECT_TRUE(f->testInt64(2001));
    EXPECT_FALSE(f->testInt64(2000));
}

TEST(ScanSpecBuilderTest, PushLessThanOrEqual)
{
    auto rowType = IntRowType2();
    bool usable = false;
    bool needResidual = false;
    std::shared_ptr<::common::PredicateCondition> residual;

    auto enh = WrapEnhancement(Leaf(::common::LESS_THAN_OR_EQUAL, 0, OMNI_INT, "6"));
    auto spec = makeScanSpec(*rowType, enh, usable, needResidual, residual);
    ASSERT_TRUE(usable);
    EXPECT_FALSE(needResidual);
    auto *f = spec->children()[0]->filter();
    ASSERT_TRUE(f->is(FilterKind::kBigintRange));
    auto *br = static_cast<::common::BigintRange *>(f);
    EXPECT_EQ(br->lower(), kMin);
    EXPECT_EQ(br->upper(), 6);
    EXPECT_TRUE(f->testInt64(6));
    EXPECT_FALSE(f->testInt64(7));
}

TEST(ScanSpecBuilderTest, PushNotEqualNegatedRange)
{
    auto rowType = IntRowType2();
    bool usable = false;
    bool needResidual = false;
    std::shared_ptr<::common::PredicateCondition> residual;

    // != 6
    auto enh = WrapEnhancement(Not(Leaf(::common::EQUAL_TO, 0, OMNI_INT, "6")));
    auto spec = makeScanSpec(*rowType, enh, usable, needResidual, residual);
    ASSERT_TRUE(usable);
    EXPECT_FALSE(needResidual);
    auto *f = spec->children()[0]->filter();
    ASSERT_NE(f, nullptr);
    EXPECT_TRUE(f->is(FilterKind::kNegatedBigintRange));
    EXPECT_TRUE(f->testInt64(5));
    EXPECT_FALSE(f->testInt64(6));
}

TEST(ScanSpecBuilderTest, PushNotGreaterThanFlipsToLessThanOrEqual)
{
    auto rowType = IntRowType2();
    bool usable = false;
    bool needResidual = false;
    std::shared_ptr<::common::PredicateCondition> residual;

    // NOT (a > 2000) → a <= 2000
    auto enh = WrapEnhancement(Not(Leaf(::common::GREATER_THAN, 0, OMNI_INT, "2000")));
    auto spec = makeScanSpec(*rowType, enh, usable, needResidual, residual);
    ASSERT_TRUE(usable);
    EXPECT_FALSE(needResidual);
    EXPECT_EQ(residual, nullptr);
    auto *f = spec->children()[0]->filter();
    ASSERT_TRUE(f->is(FilterKind::kBigintRange));
    auto *br = static_cast<::common::BigintRange *>(f);
    EXPECT_EQ(br->lower(), kMin);
    EXPECT_EQ(br->upper(), 2000);
    EXPECT_TRUE(f->testInt64(2000));
    EXPECT_FALSE(f->testInt64(2001));
}

TEST(ScanSpecBuilderTest, PushNotLessThanFlipsToGreaterThanOrEqual)
{
    auto rowType = IntRowType2();
    bool usable = false;
    bool needResidual = false;
    std::shared_ptr<::common::PredicateCondition> residual;

    // NOT (a < 3) → a >= 3
    auto enh = WrapEnhancement(Not(Leaf(::common::LESS_THAN, 0, OMNI_INT, "3")));
    auto spec = makeScanSpec(*rowType, enh, usable, needResidual, residual);
    ASSERT_TRUE(usable);
    EXPECT_FALSE(needResidual);
    auto *f = spec->children()[0]->filter();
    ASSERT_TRUE(f->is(FilterKind::kBigintRange));
    auto *br = static_cast<::common::BigintRange *>(f);
    EXPECT_EQ(br->lower(), 3);
    EXPECT_EQ(br->upper(), kMax);
    EXPECT_TRUE(f->testInt64(3));
    EXPECT_FALSE(f->testInt64(2));
}

TEST(ScanSpecBuilderTest, PushSingleColumnOrMultiRange)
{
    auto rowType = IntRowType2();
    bool usable = false;
    bool needResidual = false;
    std::shared_ptr<::common::PredicateCondition> residual;

    // a < 3 OR a > 10
    auto cond = Or(Leaf(::common::LESS_THAN, 0, OMNI_INT, "3"), Leaf(::common::GREATER_THAN, 0, OMNI_INT, "10"));
    auto enh = WrapEnhancement(cond);
    auto spec = makeScanSpec(*rowType, enh, usable, needResidual, residual);
    ASSERT_TRUE(usable);
    EXPECT_FALSE(needResidual);
    auto *f = spec->children()[0]->filter();
    ASSERT_NE(f, nullptr);
    EXPECT_TRUE(f->is(FilterKind::kBigintMultiRange));
    EXPECT_TRUE(f->testInt64(2));
    EXPECT_TRUE(f->testInt64(11));
    EXPECT_FALSE(f->testInt64(5));
}

TEST(ScanSpecBuilderTest, PushSameColumnAndIntersection)
{
    auto rowType = IntRowType2();
    bool usable = false;
    bool needResidual = false;
    std::shared_ptr<::common::PredicateCondition> residual;

    // a > 1999 AND a < 2003 → [2000, 2002]
    auto cond = And(Leaf(::common::GREATER_THAN, 0, OMNI_INT, "1999"), Leaf(::common::LESS_THAN, 0, OMNI_INT, "2003"));
    auto enh = WrapEnhancement(cond);
    auto spec = makeScanSpec(*rowType, enh, usable, needResidual, residual);
    ASSERT_TRUE(usable);
    EXPECT_FALSE(needResidual);
    auto *f = spec->children()[0]->filter();
    ASSERT_TRUE(f->is(FilterKind::kBigintRange));
    auto *br = static_cast<::common::BigintRange *>(f);
    EXPECT_EQ(br->lower(), 2000);
    EXPECT_EQ(br->upper(), 2002);
}

TEST(ScanSpecBuilderTest, PushNegatedAndRangeMerge)
{
    auto rowType = IntRowType2();
    bool usable = false;
    bool needResidual = false;
    std::shared_ptr<::common::PredicateCondition> residual;

    // a <> 6 AND a > 3 → Multi/Range, no residual
    auto cond = And(Not(Leaf(::common::EQUAL_TO, 0, OMNI_INT, "6")), Leaf(::common::GREATER_THAN, 0, OMNI_INT, "3"));
    auto enh = WrapEnhancement(cond);
    auto spec = makeScanSpec(*rowType, enh, usable, needResidual, residual);
    ASSERT_TRUE(usable);
    EXPECT_FALSE(needResidual);
    EXPECT_EQ(residual, nullptr);
    auto *f = spec->children()[0]->filter();
    ASSERT_NE(f, nullptr);
    EXPECT_TRUE(f->testInt64(4));
    EXPECT_TRUE(f->testInt64(5));
    EXPECT_FALSE(f->testInt64(6));
    EXPECT_TRUE(f->testInt64(7));
    EXPECT_FALSE(f->testInt64(3));
}

TEST(ScanSpecBuilderTest, PushMultiAndRangeMerge)
{
    auto rowType = IntRowType2();
    bool usable = false;
    bool needResidual = false;
    std::shared_ptr<::common::PredicateCondition> residual;

    // (a < 3 OR a > 10) AND a < 12 → Multi, no residual
    auto orCond = Or(Leaf(::common::LESS_THAN, 0, OMNI_INT, "3"), Leaf(::common::GREATER_THAN, 0, OMNI_INT, "10"));
    auto cond = And(orCond, Leaf(::common::LESS_THAN, 0, OMNI_INT, "12"));
    auto enh = WrapEnhancement(cond);
    auto spec = makeScanSpec(*rowType, enh, usable, needResidual, residual);
    ASSERT_TRUE(usable);
    EXPECT_FALSE(needResidual);
    auto *f = spec->children()[0]->filter();
    ASSERT_NE(f, nullptr);
    EXPECT_TRUE(f->testInt64(2));
    EXPECT_TRUE(f->testInt64(11));
    EXPECT_FALSE(f->testInt64(5));
    EXPECT_FALSE(f->testInt64(12));
}

TEST(ScanSpecBuilderTest, CrossColumnOrNeedsResidual)
{
    auto rowType = IntRowType2();
    bool usable = false;
    bool needResidual = false;
    std::shared_ptr<::common::PredicateCondition> residual;

    // a >= 0 AND (a < 3 OR b > 27) — leaf a>=0 pushed; cross-column OR → residual
    auto crossOr = Or(Leaf(::common::LESS_THAN, 0, OMNI_INT, "3"), Leaf(::common::GREATER_THAN, 1, OMNI_INT, "27"));
    auto cond = And(Leaf(::common::GREATER_THAN_OR_EQUAL, 0, OMNI_INT, "0"), crossOr);
    auto enh = WrapEnhancement(cond);
    auto spec = makeScanSpec(*rowType, enh, usable, needResidual, residual);
    ASSERT_TRUE(usable);
    EXPECT_TRUE(needResidual);
    ASSERT_NE(residual, nullptr);
    ASSERT_TRUE(spec->hasAnyLeafFilter());
    auto *f = spec->children()[0]->filter();
    ASSERT_NE(f, nullptr);
    EXPECT_TRUE(f->is(FilterKind::kBigintRange));
    EXPECT_TRUE(f->testInt64(0));
}

TEST(ScanSpecBuilderTest, PureCrossColumnOrNoLeafFilter)
{
    auto rowType = IntRowType2();
    bool usable = false;
    bool needResidual = false;
    std::shared_ptr<::common::PredicateCondition> residual;

    auto cond = Or(Leaf(::common::LESS_THAN, 0, OMNI_INT, "3"), Leaf(::common::GREATER_THAN, 1, OMNI_INT, "27"));
    auto enh = WrapEnhancement(cond);
    auto spec = makeScanSpec(*rowType, enh, usable, needResidual, residual);
    ASSERT_TRUE(usable);
    EXPECT_TRUE(needResidual);
    // Whole tree → residual, no per-column filter → gate hasPushable=false
    EXPECT_FALSE(spec->hasAnyLeafFilter());
}

TEST(ScanSpecBuilderTest, IsNullAndIsNotNull)
{
    auto rowType = IntRowType2();
    bool usable = false;
    bool needResidual = false;
    std::shared_ptr<::common::PredicateCondition> residual;

    auto enhNotNull = WrapEnhancement(Leaf(::common::IS_NOT_NULL, 0, OMNI_INT));
    auto specNotNull = makeScanSpec(*rowType, enhNotNull, usable, needResidual, residual);
    ASSERT_TRUE(usable);
    EXPECT_FALSE(needResidual);
    EXPECT_TRUE(specNotNull->children()[0]->filter()->is(FilterKind::kIsNotNull));

    usable = false;
    needResidual = false;
    residual = nullptr;
    auto enhNull = WrapEnhancement(Leaf(::common::IS_NULL, 0, OMNI_INT));
    auto specNull = makeScanSpec(*rowType, enhNull, usable, needResidual, residual);
    ASSERT_TRUE(usable);
    EXPECT_FALSE(needResidual);
    EXPECT_TRUE(specNull->children()[0]->filter()->is(FilterKind::kIsNull));
}

TEST(ScanSpecBuilderTest, Date32LeafPushedAsBigintRange)
{
    // DATE32 predicates pushed as Julian days; only check builder attaches BigintRange (edge semantics covered by parity/hand-built files).
    auto rowType = ROW({"d"}, {Date32Type()});
    bool usable = false;
    bool needResidual = false;
    std::shared_ptr<::common::PredicateCondition> residual;

    // Pre-1582-10-15 date: Gregorian day ~-141427; use fixed Julian literal -100000 here
    auto enh = WrapEnhancement(Leaf(::common::LESS_THAN, 0, OMNI_DATE32, "-100000"));
    auto spec = makeScanSpec(*rowType, enh, usable, needResidual, residual);
    ASSERT_TRUE(usable);
    EXPECT_FALSE(needResidual);
    auto *f = spec->children()[0]->filter();
    ASSERT_TRUE(f->is(FilterKind::kBigintRange));
    auto *br = static_cast<::common::BigintRange *>(f);
    EXPECT_EQ(br->lower(), kMin);
    EXPECT_EQ(br->upper(), -100001);
    EXPECT_TRUE(f->testInt64(-100001));
    EXPECT_FALSE(f->testInt64(-100000));
}

TEST(ScanSpecBuilderTest, InvalidJsonSetsUsableFalse)
{
    auto rowType = IntRowType2();
    bool usable = true;
    bool needResidual = true;
    std::shared_ptr<::common::PredicateCondition> residual;

    auto enh = std::make_shared<nlohmann::json>();
    (*enh)["vecPredicateCondition"] = "not-a-json";
    auto spec = makeScanSpec(*rowType, enh, usable, needResidual, residual);
    EXPECT_FALSE(usable);
    EXPECT_FALSE(needResidual);
    EXPECT_EQ(residual, nullptr);
    (void)spec;
}

TEST(ScanSpecBuilderTest, VarcharEqualPushesBytesRange)
{
    // Spark STRING and VARCHAR both use OMNI_VARCHAR in Omni rowType.
    auto rowType = ROW({"s", "i"}, {VarcharType(), IntType()});
    bool usable = false;
    bool needResidual = false;
    std::shared_ptr<::common::PredicateCondition> residual;

    auto enh = WrapEnhancement(Leaf(::common::EQUAL_TO, 0, OMNI_VARCHAR, "Monday"));
    auto spec = makeScanSpec(*rowType, enh, usable, needResidual, residual);
    ASSERT_TRUE(usable);
    EXPECT_FALSE(needResidual);
    EXPECT_EQ(residual, nullptr);
    auto *f = spec->children()[0]->filter();
    ASSERT_TRUE(f->is(FilterKind::kBytesRange));
    EXPECT_TRUE(f->testBytes("Monday", 6));
    EXPECT_FALSE(f->testBytes("Tuesday", 7));
}

TEST(ScanSpecBuilderTest, CharEqualPushesBytesRange)
{
    auto rowType = ROW({"c", "i"}, {CharType(40), IntType()});
    bool usable = false;
    bool needResidual = false;
    std::shared_ptr<::common::PredicateCondition> residual;

    auto enh = WrapEnhancement(Leaf(::common::EQUAL_TO, 0, OMNI_CHAR, "Monday"));
    auto spec = makeScanSpec(*rowType, enh, usable, needResidual, residual);
    ASSERT_TRUE(usable);
    EXPECT_FALSE(needResidual);
    EXPECT_EQ(residual, nullptr);
    auto *f = spec->children()[0]->filter();
    ASSERT_TRUE(f->is(FilterKind::kBytesRange));
    // Filter compares raw bytes; CHAR trailing-space trim happens in the column reader, not here.
    EXPECT_TRUE(f->testBytes("Monday", 6));
    EXPECT_FALSE(f->testBytes("Tuesday", 7));
}

TEST(ScanSpecBuilderTest, StringFamilyIsNullFullyPushedNoResidual)
{
    // Gluten writes OMNI_INT sentinel for IS NULL; column type comes from rowType.
    // Pure IS NULL on string/char/varchar -> IsNull Filter, residual empty (no residual pass needed).
    for (const auto &rowType :
         {ROW({"s"}, {VarcharType()}), ROW({"c"}, {CharType(40)}), ROW({"v"}, {VarcharType(40)})}) {
        bool usable = false;
        bool needResidual = false;
        std::shared_ptr<::common::PredicateCondition> residual;

        auto enh = WrapEnhancement(Leaf(::common::IS_NULL, 0, OMNI_INT, "-1"));
        auto spec = makeScanSpec(*rowType, enh, usable, needResidual, residual);
        ASSERT_TRUE(usable);
        EXPECT_FALSE(needResidual);
        EXPECT_EQ(residual, nullptr);
        EXPECT_TRUE(spec->children()[0]->filter()->is(FilterKind::kIsNull));
    }
}

TEST(ScanSpecBuilderTest, IntFilterAndStringFamilyProject)
{
    auto rowType = ROW({"i", "s", "c", "v"}, {IntType(), VarcharType(), CharType(40), VarcharType(40)});
    bool usable = false;
    bool needResidual = false;
    std::shared_ptr<::common::PredicateCondition> residual;

    auto enh = WrapEnhancement(Leaf(::common::EQUAL_TO, 0, OMNI_INT, "2001"));
    auto spec = makeScanSpec(*rowType, enh, usable, needResidual, residual);
    ASSERT_TRUE(usable);
    EXPECT_FALSE(needResidual);
    EXPECT_TRUE(spec->children()[0]->hasFilter());
    EXPECT_FALSE(spec->children()[1]->hasFilter());
    EXPECT_FALSE(spec->children()[2]->hasFilter());
    EXPECT_FALSE(spec->children()[3]->hasFilter());
}
