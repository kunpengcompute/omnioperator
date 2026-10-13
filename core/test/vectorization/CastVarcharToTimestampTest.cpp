/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Unit tests for casting varchar to timestamp
 */

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <stack>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "type/data_type.h"
#include "util/config/QueryConfig.h"
#include "util/type_util.h"
#include "vector/vector.h"
#include "vector/vector_helper.h"
#include "vectorization/functions/Cast.h"

using namespace omniruntime;
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;

namespace {
using StringVector = Vector<LargeStringContainer<std::string_view>>;

class CastVarcharToTimestampTest : public ::testing::Test {
protected:
    static BaseVector* CreateFlatStringVector(const std::vector<std::optional<std::string>>& values)
    {
        auto* input = static_cast<StringVector*>(VectorHelper::CreateStringVector(values.size()));
        for (size_t row = 0; row < values.size(); ++row) {
            if (!values[row].has_value()) {
                input->SetNull(row);
                continue;
            }
            input->SetValue(row, std::string_view(values[row].value()));
        }
        return input;
    }

    static BaseVector* CreateConstantStringVector(
        const std::string_view value, int32_t rowSize, bool isNull = false)
    {
        auto* input = new ConstVector<std::string_view>(value, OMNI_VARCHAR, rowSize);
        if (isNull) {
            input->SetNull(0);
        }
        return input;
    }

    static BaseVector* CreateDictionaryStringVector(
        const std::vector<std::optional<std::string>>& dictionaryValues, const std::vector<int32_t>& ids)
    {
        auto* dictionary = static_cast<StringVector*>(VectorHelper::CreateStringVector(dictionaryValues.size()));
        for (size_t row = 0; row < dictionaryValues.size(); ++row) {
            if (!dictionaryValues[row].has_value()) {
                dictionary->SetNull(row);
                continue;
            }
            dictionary->SetValue(row, std::string_view(dictionaryValues[row].value()));
        }
        auto* input = VectorHelper::CreateStringDictionary(ids.data(), ids.size(), dictionary);
        delete dictionary;
        return input;
    }

    static BaseVector* CreateDateVector(const std::vector<std::optional<int32_t>>& values)
    {
        auto* input = static_cast<Vector<int32_t>*>(
            VectorHelper::CreateFlatVector(OMNI_DATE32, values.size()));
        for (size_t row = 0; row < values.size(); ++row) {
            if (!values[row].has_value()) {
                input->SetNull(row);
                continue;
            }
            input->SetValue(row, values[row].value());
        }
        return input;
    }

    static void ExecuteCast(BaseVector* input, const DataTypePtr& fromType, int32_t rowSize,
        const std::string& timezone, BaseVector*& result)
    {
        const std::unordered_map<std::string, std::string> configValues = {
            {config::QueryConfig::kSessionTimezone, timezone},
            {config::QueryConfig::kAdjustTimestampToTimezone, "true"}
        };
        ExecutionContext context(4096, nullptr);
        context.SetConfig(config::QueryConfig(configValues));
        context.SetResultRowSize(rowSize);

        std::stack<BaseVector*> args;
        args.push(input);
        CastFunction castFunction(fromType, TimestampType());
        castFunction.Apply(args, TimestampType(), result, &context);
    }

    static Vector<int64_t>* AsTimestampVector(BaseVector* result)
    {
        return dynamic_cast<Vector<int64_t>*>(result);
    }

    static void ExpectTimestamp(
        BaseVector* result, Vector<int64_t>* timestamps, int32_t row, int64_t expected)
    {
        ASSERT_FALSE(result->IsNull(row)) << "row " << row;
        EXPECT_EQ(timestamps->GetValue(row), expected) << "row " << row;
    }
};

TEST_F(CastVarcharToTimestampTest, FlatMixedValuesReturnNullPerRow)
{
    const std::string unicodeWhitespace =
        "\xE3\x80\x80" "1970-01-01 00:00:00" "\xE3\x80\x80";
    const std::vector<std::optional<std::string>> values = {
        "1970-01-01",
        "1969-12-31 23:59:59.999999",
        " \t1970-01-01 00:00:00\n",
        unicodeWhitespace,
        "not-a-timestamp",
        "2023-02-29",
        "",
        "   ",
        std::nullopt
    };

    BaseVector* rawResult = nullptr;
    ExecuteCast(CreateFlatStringVector(values), VarcharType(), values.size(), "UTC", rawResult);
    std::unique_ptr<BaseVector> result(rawResult);
    auto* timestamps = AsTimestampVector(result.get());
    ASSERT_NE(timestamps, nullptr);

    ExpectTimestamp(result.get(), timestamps, 0, 0LL);
    ExpectTimestamp(result.get(), timestamps, 1, -1LL);
    ExpectTimestamp(result.get(), timestamps, 2, 0LL);
    ExpectTimestamp(result.get(), timestamps, 3, 0LL);
    for (int32_t row = 4; row < static_cast<int32_t>(values.size()); ++row) {
        EXPECT_TRUE(result->IsNull(row)) << "row " << row;
    }
}

TEST_F(CastVarcharToTimestampTest, ConstantValidValueIsBroadcast)
{
    constexpr int32_t rowSize = 5;
    BaseVector* rawResult = nullptr;
    ExecuteCast(CreateConstantStringVector(" 1970-01-01 00:00:00 ", rowSize),
        VarcharType(), rowSize, "UTC", rawResult);
    std::unique_ptr<BaseVector> result(rawResult);
    auto* timestamps = AsTimestampVector(result.get());
    ASSERT_NE(timestamps, nullptr);

    for (int32_t row = 0; row < rowSize; ++row) {
        ExpectTimestamp(result.get(), timestamps, row, 0LL);
    }
}

TEST_F(CastVarcharToTimestampTest, ConstantInvalidAndNullValuesReturnNull)
{
    constexpr int32_t rowSize = 4;
    BaseVector* rawInvalidResult = nullptr;
    ExecuteCast(CreateConstantStringVector("not-a-timestamp", rowSize),
        VarcharType(), rowSize, "UTC", rawInvalidResult);
    std::unique_ptr<BaseVector> invalidResult(rawInvalidResult);

    BaseVector* rawNullResult = nullptr;
    ExecuteCast(CreateConstantStringVector("unused", rowSize, true),
        VarcharType(), rowSize, "UTC", rawNullResult);
    std::unique_ptr<BaseVector> nullResult(rawNullResult);

    for (int32_t row = 0; row < rowSize; ++row) {
        EXPECT_TRUE(invalidResult->IsNull(row));
        EXPECT_TRUE(nullResult->IsNull(row));
    }
}

TEST_F(CastVarcharToTimestampTest, DictionaryUsesLogicalRowsAndPropagatesNulls)
{
    const std::vector<std::optional<std::string>> dictionaryValues = {
        "1970-01-01",
        "not-a-timestamp",
        "1969-12-31 23:59:59.999999",
        std::nullopt
    };
    const std::vector<int32_t> ids = {2, 0, 1, 0, 3, 2};

    BaseVector* rawResult = nullptr;
    ExecuteCast(CreateDictionaryStringVector(dictionaryValues, ids),
        VarcharType(), ids.size(), "UTC", rawResult);
    std::unique_ptr<BaseVector> result(rawResult);
    auto* timestamps = AsTimestampVector(result.get());
    ASSERT_NE(timestamps, nullptr);
    EXPECT_EQ(result->GetEncoding(), OMNI_FLAT);

    ExpectTimestamp(result.get(), timestamps, 0, -1LL);
    ExpectTimestamp(result.get(), timestamps, 1, 0LL);
    EXPECT_TRUE(result->IsNull(2));
    ExpectTimestamp(result.get(), timestamps, 3, 0LL);
    EXPECT_TRUE(result->IsNull(4));
    ExpectTimestamp(result.get(), timestamps, 5, -1LL);
}

TEST_F(CastVarcharToTimestampTest, FractionalSecondsUseMicrosecondPrecision)
{
    const std::vector<std::optional<std::string>> values = {
        "1970-01-01 00:00:00.1",
        "1970-01-01 00:00:00.123456",
        "1970-01-01 00:00:00.123456789"
    };

    BaseVector* rawResult = nullptr;
    ExecuteCast(CreateFlatStringVector(values), VarcharType(), values.size(), "UTC", rawResult);
    std::unique_ptr<BaseVector> result(rawResult);
    auto* timestamps = AsTimestampVector(result.get());
    ASSERT_NE(timestamps, nullptr);

    ExpectTimestamp(result.get(), timestamps, 0, 100000LL);
    ExpectTimestamp(result.get(), timestamps, 1, 123456LL);
    ExpectTimestamp(result.get(), timestamps, 2, 123456LL);
}

TEST_F(CastVarcharToTimestampTest, AncientDatesUseProlepticGregorianCalendar)
{
    const std::vector<std::optional<std::string>> values = {
        "0500-01-01 00:00:00",
        "1500-01-01 12:34:56.123456"
    };

    BaseVector* rawResult = nullptr;
    ExecuteCast(CreateFlatStringVector(values), VarcharType(), values.size(), "UTC", rawResult);
    std::unique_ptr<BaseVector> result(rawResult);
    auto* timestamps = AsTimestampVector(result.get());
    ASSERT_NE(timestamps, nullptr);

    // Spark TimestampType uses the proleptic Gregorian calendar and stores epoch microseconds.
    ExpectTimestamp(result.get(), timestamps, 0, -46388678400000000LL);
    ExpectTimestamp(result.get(), timestamps, 1, -14831724303876544LL);
}

TEST_F(CastVarcharToTimestampTest, ExplicitTimezoneAndOffsetsAreApplied)
{
    const std::vector<std::optional<std::string>> values = {
        "1970-01-01 00:00:00Z",
        "1970-01-01 00:00:00+08:00",
        "1970-01-01 00:00:00-02:00",
        "1970-01-01 00:00:00 UTC"
    };

    BaseVector* rawResult = nullptr;
    ExecuteCast(CreateFlatStringVector(values), VarcharType(), values.size(), "UTC", rawResult);
    std::unique_ptr<BaseVector> result(rawResult);
    auto* timestamps = AsTimestampVector(result.get());
    ASSERT_NE(timestamps, nullptr);

    ExpectTimestamp(result.get(), timestamps, 0, 0LL);
    ExpectTimestamp(result.get(), timestamps, 1, -28800000000LL);
    ExpectTimestamp(result.get(), timestamps, 2, 7200000000LL);
    ExpectTimestamp(result.get(), timestamps, 3, 0LL);
}

TEST_F(CastVarcharToTimestampTest, SessionTimezoneAppliesOnlyWithoutExplicitTimezone)
{
    const std::vector<std::optional<std::string>> values = {
        "1970-01-01 00:00:00",
        "1970-01-01 00:00:00Z"
    };

    BaseVector* rawUtcResult = nullptr;
    ExecuteCast(CreateFlatStringVector(values), VarcharType(), values.size(), "UTC", rawUtcResult);
    std::unique_ptr<BaseVector> utcResult(rawUtcResult);
    auto* utcTimestamps = AsTimestampVector(utcResult.get());
    ASSERT_NE(utcTimestamps, nullptr);

    BaseVector* rawShanghaiResult = nullptr;
    ExecuteCast(CreateFlatStringVector(values), VarcharType(), values.size(),
        "Asia/Shanghai", rawShanghaiResult);
    std::unique_ptr<BaseVector> shanghaiResult(rawShanghaiResult);
    auto* shanghaiTimestamps = AsTimestampVector(shanghaiResult.get());
    ASSERT_NE(shanghaiTimestamps, nullptr);

    ExpectTimestamp(utcResult.get(), utcTimestamps, 0, 0LL);
    ExpectTimestamp(shanghaiResult.get(), shanghaiTimestamps, 0, -28800000000LL);
    ExpectTimestamp(utcResult.get(), utcTimestamps, 1, 0LL);
    ExpectTimestamp(shanghaiResult.get(), shanghaiTimestamps, 1, 0LL);
}

TEST_F(CastVarcharToTimestampTest, NamedTimezoneObservesDaylightSavingTime)
{
    const std::vector<std::optional<std::string>> explicitTimezoneValues = {
        "2015-07-24 00:00:00 America/Los_Angeles",
        "2015-01-24 00:00:00 America/Los_Angeles"
    };

    BaseVector* rawExplicitResult = nullptr;
    ExecuteCast(CreateFlatStringVector(explicitTimezoneValues), VarcharType(),
        explicitTimezoneValues.size(), "UTC", rawExplicitResult);
    std::unique_ptr<BaseVector> explicitResult(rawExplicitResult);
    auto* explicitTimestamps = AsTimestampVector(explicitResult.get());
    ASSERT_NE(explicitTimestamps, nullptr);

    // July uses PDT (UTC-7), while January uses PST (UTC-8).
    ExpectTimestamp(explicitResult.get(), explicitTimestamps, 0, 1437721200000000LL);
    ExpectTimestamp(explicitResult.get(), explicitTimestamps, 1, 1422086400000000LL);

    const std::vector<std::optional<std::string>> sessionTimezoneValues = {
        "2015-07-24 00:00:00",
        "2015-01-24 00:00:00"
    };

    BaseVector* rawSessionResult = nullptr;
    ExecuteCast(CreateFlatStringVector(sessionTimezoneValues), VarcharType(),
        sessionTimezoneValues.size(), "America/Los_Angeles", rawSessionResult);
    std::unique_ptr<BaseVector> sessionResult(rawSessionResult);
    auto* sessionTimestamps = AsTimestampVector(sessionResult.get());
    ASSERT_NE(sessionTimestamps, nullptr);

    ExpectTimestamp(sessionResult.get(), sessionTimestamps, 0, 1437721200000000LL);
    ExpectTimestamp(sessionResult.get(), sessionTimestamps, 1, 1422086400000000LL);
}

TEST_F(CastVarcharToTimestampTest, Date32CastRemainsUnchanged)
{
    const std::vector<std::optional<int32_t>> values = {0, 1, -1, std::nullopt};

    BaseVector* rawResult = nullptr;
    ExecuteCast(CreateDateVector(values), Date32Type(), values.size(), "UTC", rawResult);
    std::unique_ptr<BaseVector> result(rawResult);
    auto* timestamps = AsTimestampVector(result.get());
    ASSERT_NE(timestamps, nullptr);

    ExpectTimestamp(result.get(), timestamps, 0, 0LL);
    ExpectTimestamp(result.get(), timestamps, 1, 86400000000LL);
    ExpectTimestamp(result.get(), timestamps, 2, -86400000000LL);
    EXPECT_TRUE(result->IsNull(3));
}
} // namespace
