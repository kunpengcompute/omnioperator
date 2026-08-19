/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: CONVERT_TZ function unit tests
 *
 * convert_tz(datetimeString, tzFrom, tzTo) -> string.
 * Mirrors Flink's DateTimeUtils.convertTz(dateStr, tzFrom, tzTo):
 *   parse 'yyyy-MM-dd HH:mm:ss' as local time in tzFrom -> UTC instant ->
 *   render as local time in tzTo.
 *
 * Expected values below are derived from the Flink reference semantics
 * (DateTimeUtils.convertTz), which is the authoritative spec for this function.
 */

#include <gtest/gtest.h>
#include <vector>
#include <string>
#include <string_view>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/functions/ConvertTz.h"
#include "vectorization/VectorFunction.h"
#include "codegen/func_signature.h"
#include "vector/vector_helper.h"
#include "vector/vector.h"
#include "util/type_util.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::codegen;
using namespace omniruntime::TestUtil;

class ConvertTzTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const convert_tz_test_env =
    ::testing::AddGlobalTestEnvironment(new ConvertTzTestEnvironment);

class ConvertTzTestHelper {
public:
    // Create a flat (non-const) VARCHAR vector from a list of strings.
    static BaseVector* CreateStringVector(const std::vector<std::string>& values) {
        auto* vec = new Vector<LargeStringContainer<std::string_view>>(values.size());
        for (size_t i = 0; i < values.size(); ++i) {
            std::string_view sv(values[i].data(), values[i].size());
            vec->SetValue(i, sv);
        }
        return vec;
    }

    // Create a const VARCHAR vector (same value for every row).
    static BaseVector* CreateConstStringVector(const std::string& value, int32_t size) {
        auto* vec = new Vector<LargeStringContainer<std::string_view>>(size);
        std::string_view sv(value.data(), value.size());
        for (int32_t i = 0; i < size; ++i) {
            vec->SetValue(i, sv);
        }
        return vec;
    }

    // Look up convert_tz for the given input signature, run Apply(), and return the result.
    static void ExecuteFunction(const std::string& funcName,
        const std::vector<DataTypeId>& inputTypes,
        std::stack<BaseVector*>& args, BaseVector*& result) {
        auto signature = std::make_shared<FunctionSignature>(funcName,
            inputTypes, OMNI_VARCHAR);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << funcName << " function not found for signature";

        auto outType = std::make_shared<DataType>(OMNI_VARCHAR);
        ExecutionContext context;
        int32_t sz = 0;
        if (!args.empty()) {
            std::stack<BaseVector*> tmpArgs = args;
            while (!tmpArgs.empty()) {
                sz = tmpArgs.top()->GetSize();
                tmpArgs.pop();
            }
        }
        context.SetResultRowSize(sz);

        try {
            function->Apply(args, outType, result, &context);
        } catch (const std::exception& e) {
            FAIL() << funcName << " function threw an exception: " << e.what();
        } catch (...) {
            FAIL() << funcName << " function threw an unknown exception";
        }
    }

    // Read row i of a VARCHAR result vector as a std::string.
    static std::string GetResultValue(BaseVector* result, int32_t i) {
        auto* typed = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
        EXPECT_NE(typed, nullptr);
        std::string_view sv = typed->GetValue(i);
        return std::string(sv);
    }
};

// ============================================================================
// Basic conversion tests — values verified against Flink semantics
// ============================================================================

TEST(ConvertTzTest, Basic_UtcToLosAngeles) {
    // The canonical example from sql_functions.yml:
    // CONVERT_TZ('1970-01-01 00:00:00', 'UTC', 'America/Los_Angeles')
    //   -> '1969-12-31 16:00:00'  (PST, UTC-8 in January)
    BaseVector* dtVec = ConvertTzTestHelper::CreateConstStringVector("1970-01-01 00:00:00", 1);
    BaseVector* tzFromVec = ConvertTzTestHelper::CreateConstStringVector("UTC", 1);
    BaseVector* tzToVec = ConvertTzTestHelper::CreateConstStringVector("America/Los_Angeles", 1);

    std::stack<BaseVector*> args;
    args.push(dtVec);
    args.push(tzFromVec);
    args.push(tzToVec);

    BaseVector* result = nullptr;
    ConvertTzTestHelper::ExecuteFunction("convert_tz",
        {OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR}, args, result);

    ASSERT_NE(result, nullptr);
    EXPECT_FALSE(result->IsNull(0));
    EXPECT_EQ(ConvertTzTestHelper::GetResultValue(result, 0), "1969-12-31 16:00:00");
    delete result;
}

TEST(ConvertTzTest, Basic_LosAngelesToUtc) {
    // Reverse of the canonical example: local 16:00 PST -> 00:00 UTC.
    BaseVector* dtVec = ConvertTzTestHelper::CreateConstStringVector("1969-12-31 16:00:00", 1);
    BaseVector* tzFromVec = ConvertTzTestHelper::CreateConstStringVector("America/Los_Angeles", 1);
    BaseVector* tzToVec = ConvertTzTestHelper::CreateConstStringVector("UTC", 1);

    std::stack<BaseVector*> args;
    args.push(dtVec);
    args.push(tzFromVec);
    args.push(tzToVec);

    BaseVector* result = nullptr;
    ConvertTzTestHelper::ExecuteFunction("convert_tz",
        {OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR}, args, result);

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(ConvertTzTestHelper::GetResultValue(result, 0), "1970-01-01 00:00:00");
    delete result;
}

TEST(ConvertTzTest, SameZoneIdentity) {
    BaseVector* dtVec = ConvertTzTestHelper::CreateConstStringVector("2024-06-15 12:30:45", 1);
    BaseVector* tzFromVec = ConvertTzTestHelper::CreateConstStringVector("UTC", 1);
    BaseVector* tzToVec = ConvertTzTestHelper::CreateConstStringVector("UTC", 1);

    std::stack<BaseVector*> args;
    args.push(dtVec);
    args.push(tzFromVec);
    args.push(tzToVec);

    BaseVector* result = nullptr;
    ConvertTzTestHelper::ExecuteFunction("convert_tz",
        {OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR}, args, result);

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(ConvertTzTestHelper::GetResultValue(result, 0), "2024-06-15 12:30:45");
    delete result;
}

TEST(ConvertTzTest, PositiveOffsetToUtc) {
    BaseVector* dtVec = ConvertTzTestHelper::CreateConstStringVector("2024-01-15 08:00:00", 1);
    BaseVector* tzFromVec = ConvertTzTestHelper::CreateConstStringVector("+08:00", 1);
    BaseVector* tzToVec = ConvertTzTestHelper::CreateConstStringVector("UTC", 1);

    std::stack<BaseVector*> args;
    args.push(dtVec);
    args.push(tzFromVec);
    args.push(tzToVec);

    BaseVector* result = nullptr;
    ConvertTzTestHelper::ExecuteFunction("convert_tz",
        {OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR}, args, result);

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(ConvertTzTestHelper::GetResultValue(result, 0), "2024-01-15 08:00:00");
    delete result;
}

TEST(ConvertTzTest, GmtCustomIdToUtc) {
    // '2024-01-15 08:00:00' in GMT-08:00 -> '2024-01-15 16:00:00' UTC.
    BaseVector* dtVec = ConvertTzTestHelper::CreateConstStringVector("2024-01-15 08:00:00", 1);
    BaseVector* tzFromVec = ConvertTzTestHelper::CreateConstStringVector("GMT-08:00", 1);
    BaseVector* tzToVec = ConvertTzTestHelper::CreateConstStringVector("UTC", 1);

    std::stack<BaseVector*> args;
    args.push(dtVec);
    args.push(tzFromVec);
    args.push(tzToVec);

    BaseVector* result = nullptr;
    ConvertTzTestHelper::ExecuteFunction("convert_tz",
        {OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR}, args, result);

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(ConvertTzTestHelper::GetResultValue(result, 0), "2024-01-15 16:00:00");
    delete result;
}

TEST(ConvertTzTest, GmtPrefixedPositiveOffsetApplied) {
    // Contrast with the bare-offset case: a GMT-prefixed custom ID ("GMT+08:00")
    // IS recognized by java.util.TimeZone as a +8h fixed offset, so the
    // conversion is actually applied: '2024-01-15 08:00:00' UTC -> +08h ->
    // '2024-01-15 16:00:00'. This is the row r15 golden value.
    BaseVector* dtVec = ConvertTzTestHelper::CreateConstStringVector("2024-01-15 08:00:00", 1);
    BaseVector* tzFromVec = ConvertTzTestHelper::CreateConstStringVector("UTC", 1);
    BaseVector* tzToVec = ConvertTzTestHelper::CreateConstStringVector("GMT+08:00", 1);

    std::stack<BaseVector*> args;
    args.push(dtVec);
    args.push(tzFromVec);
    args.push(tzToVec);

    BaseVector* result = nullptr;
    ConvertTzTestHelper::ExecuteFunction("convert_tz",
        {OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR}, args, result);

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(ConvertTzTestHelper::GetResultValue(result, 0), "2024-01-15 16:00:00");
    delete result;
}

TEST(ConvertTzTest, BareOffsetFallsBackToGmtBothArgs) {
    // r09 golden: '1970-01-01 00:00:00' UTC -> '+08:00'. The bare '+08:00' tzTo
    // falls back to GMT, so the result is the input unchanged.
    // r15 golden: '2024-01-15 08:00:00' UTC -> '+08:00' -> '2024-01-15 08:00:00'
    // (bare offset ignored, GMT fallback). r04 is the tzFrom='+08:00' mirror.
    BaseVector* dtVec = ConvertTzTestHelper::CreateStringVector(
        {"1970-01-01 00:00:00", "2024-01-15 08:00:00", "2024-01-15 08:00:00"});
    BaseVector* tzFromVec = ConvertTzTestHelper::CreateStringVector({"UTC", "UTC", "+08:00"});
    BaseVector* tzToVec = ConvertTzTestHelper::CreateStringVector({"+08:00", "+08:00", "UTC"});

    std::stack<BaseVector*> args;
    args.push(dtVec);
    args.push(tzFromVec);
    args.push(tzToVec);

    BaseVector* result = nullptr;
    ConvertTzTestHelper::ExecuteFunction("convert_tz",
        {OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR}, args, result);

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(ConvertTzTestHelper::GetResultValue(result, 0), "1970-01-01 00:00:00");
    EXPECT_EQ(ConvertTzTestHelper::GetResultValue(result, 1), "2024-01-15 08:00:00");
    EXPECT_EQ(ConvertTzTestHelper::GetResultValue(result, 2), "2024-01-15 08:00:00");
    delete result;
}

TEST(ConvertTzTest, DstAware_SummerPdt) {
    // '2024-07-15 00:00:00' UTC -> '2024-07-14 17:00:00' America/Los_Angeles (PDT, UTC-7).
    BaseVector* dtVec = ConvertTzTestHelper::CreateConstStringVector("2024-07-15 00:00:00", 1);
    BaseVector* tzFromVec = ConvertTzTestHelper::CreateConstStringVector("UTC", 1);
    BaseVector* tzToVec = ConvertTzTestHelper::CreateConstStringVector("America/Los_Angeles", 1);

    std::stack<BaseVector*> args;
    args.push(dtVec);
    args.push(tzFromVec);
    args.push(tzToVec);

    BaseVector* result = nullptr;
    ConvertTzTestHelper::ExecuteFunction("convert_tz",
        {OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR}, args, result);

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(ConvertTzTestHelper::GetResultValue(result, 0), "2024-07-14 17:00:00");
    delete result;
}

TEST(ConvertTzTest, DstAware_WinterPst) {
    // '2024-01-15 00:00:00' UTC -> '2024-01-14 16:00:00' America/Los_Angeles (PST, UTC-8).
    BaseVector* dtVec = ConvertTzTestHelper::CreateConstStringVector("2024-01-15 00:00:00", 1);
    BaseVector* tzFromVec = ConvertTzTestHelper::CreateConstStringVector("UTC", 1);
    BaseVector* tzToVec = ConvertTzTestHelper::CreateConstStringVector("America/Los_Angeles", 1);

    std::stack<BaseVector*> args;
    args.push(dtVec);
    args.push(tzFromVec);
    args.push(tzToVec);

    BaseVector* result = nullptr;
    ConvertTzTestHelper::ExecuteFunction("convert_tz",
        {OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR}, args, result);

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(ConvertTzTestHelper::GetResultValue(result, 0), "2024-01-14 16:00:00");
    delete result;
}

TEST(ConvertTzTest, HalfHourOffset_AsiaKolkata) {
    // '2024-01-15 00:00:00' UTC -> '2024-01-15 05:30:00' Asia/Kolkata (+5:30).
    BaseVector* dtVec = ConvertTzTestHelper::CreateConstStringVector("2024-01-15 00:00:00", 1);
    BaseVector* tzFromVec = ConvertTzTestHelper::CreateConstStringVector("UTC", 1);
    BaseVector* tzToVec = ConvertTzTestHelper::CreateConstStringVector("Asia/Kolkata", 1);

    std::stack<BaseVector*> args;
    args.push(dtVec);
    args.push(tzFromVec);
    args.push(tzToVec);

    BaseVector* result = nullptr;
    ConvertTzTestHelper::ExecuteFunction("convert_tz",
        {OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR}, args, result);

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(ConvertTzTestHelper::GetResultValue(result, 0), "2024-01-15 05:30:00");
    delete result;
}

// ============================================================================
// Null handling
// ============================================================================

TEST(ConvertTzTest, NullDatetime) {
    BaseVector* dtVec = ConvertTzTestHelper::CreateConstStringVector("2024-01-15 00:00:00", 2);
    dtVec->SetNull(1);
    BaseVector* tzFromVec = ConvertTzTestHelper::CreateConstStringVector("UTC", 2);
    BaseVector* tzToVec = ConvertTzTestHelper::CreateConstStringVector("UTC", 2);

    std::stack<BaseVector*> args;
    args.push(dtVec);
    args.push(tzFromVec);
    args.push(tzToVec);

    BaseVector* result = nullptr;
    ConvertTzTestHelper::ExecuteFunction("convert_tz",
        {OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR}, args, result);

    ASSERT_NE(result, nullptr);
    EXPECT_FALSE(result->IsNull(0));
    EXPECT_TRUE(result->IsNull(1));
    delete result;
}

TEST(ConvertTzTest, NullNonConstTimezone) {
    // Non-const tzFrom with a NULL row -> that row is NULL.
    BaseVector* dtVec = ConvertTzTestHelper::CreateStringVector(
        {"2024-01-15 00:00:00", "2024-01-15 00:00:00"});
    BaseVector* tzFromVec = ConvertTzTestHelper::CreateStringVector({"UTC", "UTC"});
    tzFromVec->SetNull(1);
    BaseVector* tzToVec = ConvertTzTestHelper::CreateConstStringVector("UTC", 2);

    std::stack<BaseVector*> args;
    args.push(dtVec);
    args.push(tzFromVec);
    args.push(tzToVec);

    BaseVector* result = nullptr;
    ConvertTzTestHelper::ExecuteFunction("convert_tz",
        {OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR}, args, result);

    ASSERT_NE(result, nullptr);
    EXPECT_FALSE(result->IsNull(0));
    EXPECT_TRUE(result->IsNull(1));
    delete result;
}

// ============================================================================
// Invalid inputs -> NULL (matches Flink's ParseException -> null)
// ============================================================================

TEST(ConvertTzTest, MalformedDatetimeString) {
    std::vector<std::string> badDates = {
        "not-a-date-at-all",       // non-numeric
        "2024-1-1 0:0:0",          // wrong length / single-digit fields
        "2024-01-15",              // missing time portion
        "2024-01-15 00:00:00.000", // too long (fractional seconds)
        "2024-13-01 00:00:00",     // month out of range
        "2024-01-32 00:00:00",     // day out of range
        "2024-01-15 24:00:00",     // hour out of range
        "2024-01-15 00:60:00",     // minute out of range
        "2024-01-15 00:00:60",     // second out of range
    };

    BaseVector* dtVec = ConvertTzTestHelper::CreateStringVector(badDates);
    BaseVector* tzFromVec = ConvertTzTestHelper::CreateConstStringVector("UTC", badDates.size());
    BaseVector* tzToVec = ConvertTzTestHelper::CreateConstStringVector("UTC", badDates.size());

    std::stack<BaseVector*> args;
    args.push(dtVec);
    args.push(tzFromVec);
    args.push(tzToVec);

    BaseVector* result = nullptr;
    ConvertTzTestHelper::ExecuteFunction("convert_tz",
        {OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR}, args, result);

    ASSERT_NE(result, nullptr);
    for (size_t i = 0; i < badDates.size(); ++i) {
        EXPECT_TRUE(result->IsNull(static_cast<int32_t>(i)))
            << "row " << i << " ('" << badDates[i] << "') should be NULL";
    }
    delete result;
}

TEST(ConvertTzTest, UnknownTimeZoneFallsBackToGmt) {
    // Flink's java.util.TimeZone.getTimeZone never returns null for an unknown
    // ID — it silently falls back to GMT (UTC+0). So an unknown tzFrom yields a
    // GMT-local time, which formatted in UTC equals the input unchanged (NOT
    // NULL). We mirror Flink's lenient GMT fallback to stay result-compatible.
    BaseVector* dtVec = ConvertTzTestHelper::CreateConstStringVector("2024-01-15 00:00:00", 1);
    BaseVector* tzFromVec = ConvertTzTestHelper::CreateConstStringVector("Mars/Olympus", 1);
    BaseVector* tzToVec = ConvertTzTestHelper::CreateConstStringVector("UTC", 1);

    std::stack<BaseVector*> args;
    args.push(dtVec);
    args.push(tzFromVec);
    args.push(tzToVec);

    BaseVector* result = nullptr;
    ConvertTzTestHelper::ExecuteFunction("convert_tz",
        {OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR}, args, result);

    ASSERT_NE(result, nullptr);
    EXPECT_FALSE(result->IsNull(0));
    EXPECT_EQ(ConvertTzTestHelper::GetResultValue(result, 0), "2024-01-15 00:00:00");
    delete result;
}

// ============================================================================
// Argument-order correctness (asymmetric case that catches swap bugs)
// ============================================================================

TEST(ConvertTzTest, ArgumentOrderNotSwapped) {
    // (str, tzFrom, tzTo) is NOT the same as (str, tzTo, tzFrom) when zones differ.
    // UTC 00:00 -> Asia/Kolkata (+5:30) == 05:30:00.
    BaseVector* dtVec = ConvertTzTestHelper::CreateConstStringVector("2024-01-15 00:00:00", 1);
    BaseVector* tzFromVec = ConvertTzTestHelper::CreateConstStringVector("UTC", 1);
    BaseVector* tzToVec = ConvertTzTestHelper::CreateConstStringVector("Asia/Kolkata", 1);

    std::stack<BaseVector*> args;
    args.push(dtVec);
    args.push(tzFromVec);
    args.push(tzToVec);

    BaseVector* result = nullptr;
    ConvertTzTestHelper::ExecuteFunction("convert_tz",
        {OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR}, args, result);

    ASSERT_NE(result, nullptr);
    // Forward: 00:00 UTC -> 05:30 Kolkata.
    EXPECT_EQ(ConvertTzTestHelper::GetResultValue(result, 0), "2024-01-15 05:30:00");
    delete result;
}

TEST(ConvertTzTest, ArgumentOrderReversed) {
    // Reversing tzFrom/tzTo gives a different result: 00:00 Kolkata -> -5:30 = previous day 18:30 UTC.
    BaseVector* dtVec = ConvertTzTestHelper::CreateConstStringVector("2024-01-15 00:00:00", 1);
    BaseVector* tzFromVec = ConvertTzTestHelper::CreateConstStringVector("Asia/Kolkata", 1);
    BaseVector* tzToVec = ConvertTzTestHelper::CreateConstStringVector("UTC", 1);

    std::stack<BaseVector*> args;
    args.push(dtVec);
    args.push(tzFromVec);
    args.push(tzToVec);

    BaseVector* result = nullptr;
    ConvertTzTestHelper::ExecuteFunction("convert_tz",
        {OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR}, args, result);

    ASSERT_NE(result, nullptr);
    // 00:00 Kolkata is 18:30 of the previous day UTC.
    EXPECT_EQ(ConvertTzTestHelper::GetResultValue(result, 0), "2024-01-14 18:30:00");
    delete result;
}

// ============================================================================
// Multi-row + non-const timezone
// ============================================================================

TEST(ConvertTzTest, MultiRowBatch) {
    BaseVector* dtVec = ConvertTzTestHelper::CreateStringVector({
        "1970-01-01 00:00:00",  // UTC -> LA (PST) -> 1969-12-31 16:00:00
        "2024-01-15 00:00:00",  // UTC -> +08:00  -> 2024-01-15 00:00:00 (bare offset -> GMT fallback)
        "2024-07-15 00:00:00",  // UTC -> LA (PDT) -> 2024-07-14 17:00:00
        "2024-06-15 12:30:45",  // UTC -> UTC -> identity
    });
    // Non-const tzTo so each row targets a different zone.
    BaseVector* tzToVec = ConvertTzTestHelper::CreateStringVector({
        "America/Los_Angeles",
        "+08:00",
        "America/Los_Angeles",
        "UTC",
    });
    BaseVector* tzFromVec = ConvertTzTestHelper::CreateConstStringVector("UTC", 4);

    std::stack<BaseVector*> args;
    args.push(dtVec);
    args.push(tzFromVec);
    args.push(tzToVec);

    BaseVector* result = nullptr;
    ConvertTzTestHelper::ExecuteFunction("convert_tz",
        {OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR}, args, result);

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(ConvertTzTestHelper::GetResultValue(result, 0), "1969-12-31 16:00:00");
    EXPECT_EQ(ConvertTzTestHelper::GetResultValue(result, 1), "2024-01-15 00:00:00");
    EXPECT_EQ(ConvertTzTestHelper::GetResultValue(result, 2), "2024-07-14 17:00:00");
    EXPECT_EQ(ConvertTzTestHelper::GetResultValue(result, 3), "2024-06-15 12:30:45");
    delete result;
}

// ============================================================================
// Large / boundary year formatting (4-digit zero-padded)
// ============================================================================

TEST(ConvertTzTest, LargeYearFormatting) {
    // Year 9999 must render as 4-digit (not 5). UTC -> UTC identity keeps the year.
    BaseVector* dtVec = ConvertTzTestHelper::CreateConstStringVector("9999-12-31 23:59:59", 1);
    BaseVector* tzFromVec = ConvertTzTestHelper::CreateConstStringVector("UTC", 1);
    BaseVector* tzToVec = ConvertTzTestHelper::CreateConstStringVector("UTC", 1);

    std::stack<BaseVector*> args;
    args.push(dtVec);
    args.push(tzFromVec);
    args.push(tzToVec);

    BaseVector* result = nullptr;
    ConvertTzTestHelper::ExecuteFunction("convert_tz",
        {OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR}, args, result);

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(ConvertTzTestHelper::GetResultValue(result, 0), "9999-12-31 23:59:59");
    delete result;
}

TEST(ConvertTzTest, CrossesEpochBoundary) {
    // '1970-01-01 00:00:00' UTC -> '+08:00'. Flink treats "+08:00" as an unknown
    // ID (no "GMT" prefix) and falls back to GMT, so the result is the unchanged
    // '1970-01-01 00:00:00' (NOT '08:00:00').
    BaseVector* dtVec = ConvertTzTestHelper::CreateConstStringVector("1970-01-01 00:00:00", 1);
    BaseVector* tzFromVec = ConvertTzTestHelper::CreateConstStringVector("UTC", 1);
    BaseVector* tzToVec = ConvertTzTestHelper::CreateConstStringVector("+08:00", 1);

    std::stack<BaseVector*> args;
    args.push(dtVec);
    args.push(tzFromVec);
    args.push(tzToVec);

    BaseVector* result = nullptr;
    ConvertTzTestHelper::ExecuteFunction("convert_tz",
        {OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR}, args, result);

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(ConvertTzTestHelper::GetResultValue(result, 0), "1970-01-01 00:00:00");
    delete result;
}
