/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: LocalTimestamp function unit tests
 */

#include <gtest/gtest.h>
#include <iostream>
#include <vector>
#include <ctime>
#include <chrono>
#include <cmath>
#include <string>
#include <unordered_map>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/functions/CurrentDateTimeFunctions.h"
#include "vectorization/Status.h"
#include "vectorization/VectorFunction.h"
#include "codegen/func_signature.h"
#include "vector/vector_helper.h"
#include "vector/vector.h"
#include "type/data_type.h"
#include "type/Timestamp.h"
#include "type/tz/TimeZoneMap.h"
#include "util/config/QueryConfig.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::codegen;

namespace {
// Real epoch milliseconds from the system clock (UTC-based, the raw time_since_epoch).
int64_t GetCurrentEpochMillis()
{
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

// localtimestamp = utc_millis + session_tz_offset, so when compared against UTC millis the
// difference equals the timezone offset.
constexpr int64_t MAX_VALID_DRIFT_MILLIS = 15LL * 3600LL * 1000LL;

// Epoch millis for year 2000-01-01 and 2100-01-01; guards against unit bugs
// (seconds ~1.7e9, millis ~1.7e12, micros ~1.7e15).
constexpr int64_t kMinEpochMillis = 946684800000LL;  // 2000-01-01T00:00:00Z
constexpr int64_t kMaxEpochMillis = 4102444800000LL; // 2100-01-01T00:00:00Z

// Tolerance for session-timezone exact-match checks on slow CI.
static constexpr int64_t kMillisTolerance = 10'000; // 10 seconds

// Compute expected localtimestamp (utc_millis + session_tz_offset) for the current instant.
int64_t GetSessionTzLocalTimestampMillis(const tz::TimeZone *sessionTz)
{
    auto now = std::chrono::system_clock::now();
    auto utcMillis = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    Timestamp ts = Timestamp::fromMillis(utcMillis);
    if (sessionTz != nullptr) {
        ts.toTimezone(*sessionTz);
    }
    return ts.toMillis();
}

// Compute expected localtimestamp using OS local time (no session timezone).
int64_t GetOsLocalTimestampMillis()
{
    auto now = std::chrono::system_clock::now();
    auto durationSinceEpoch = now.time_since_epoch();
    auto secsSinceEpoch = std::chrono::duration_cast<std::chrono::seconds>(durationSinceEpoch);
    auto millisSinceEpoch = std::chrono::duration_cast<std::chrono::milliseconds>(durationSinceEpoch);
    int64_t subSecondMillis = static_cast<int64_t>((millisSinceEpoch - secsSinceEpoch).count());

    std::time_t timeNow = static_cast<std::time_t>(secsSinceEpoch.count());
    std::tm localTm {};
    localtime_r(&timeNow, &localTm);

    constexpr int64_t kMillisPerSec = 1000LL;
    constexpr int64_t kSecsPerDay = 86400LL;

    int32_t year = localTm.tm_year + 1900;
    int32_t month = localTm.tm_mon + 1;
    int32_t day = localTm.tm_mday;
    int64_t daysSinceEpoch = 0;
    Date32::DaysSinceEpochFromDate(year, month, day, daysSinceEpoch);
    int64_t millisSinceMidnight = static_cast<int64_t>(
        localTm.tm_hour * 3600 + localTm.tm_min * 60 + localTm.tm_sec) * kMillisPerSec;
    return daysSinceEpoch * kSecsPerDay * kMillisPerSec + millisSinceMidnight + subSecondMillis;
}
} // namespace

class LocalTimestampTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        RegisterFunctions::Register();
    }

    // The returned instance has already called initialize() which caches the current timestamp.
    static std::unique_ptr<VectorFunction> CreateVectorFunction(
        const std::unordered_map<std::string, std::string> &configValues = {})
    {
        std::vector<DataTypeId> argTypes = {};
        auto signature = std::make_shared<FunctionSignature>("flink_localtimestamp", argTypes, OMNI_LONG);

        auto factoryIt = VectorFunction::simpleFunctionFactoryMap_.find(signature);
        if (factoryIt == VectorFunction::simpleFunctionFactoryMap_.end()) {
            return nullptr;
        }

        config::QueryConfig queryConfig(configValues);
        std::vector<BaseVector *> constantInputs;
        auto factory = factoryIt->second();
        return factory->createVectorFunction({}, queryConfig, constantInputs);
    }

    // Apply an existing vector function instance with the given row size.
    static void ApplyLocalTimestamp(VectorFunction *vectorFunction, int32_t rowSize, BaseVector *&result)
    {
        ExecutionContext context;
        context.SetResultRowSize(rowSize);
        std::stack<BaseVector *> args;
        auto resultType = std::make_shared<DataType>(OMNI_LONG);
        vectorFunction->Apply(args, resultType, result, &context);
    }

    // Convenience: create a fresh function instance and execute once.
    static void ExecuteLocalTimestamp(int32_t rowSize, BaseVector *&result,
        const std::unordered_map<std::string, std::string> &configValues = {})
    {
        auto vectorFunction = CreateVectorFunction(configValues);
        ASSERT_NE(vectorFunction, nullptr) << "Function localtimestamp creation failed";
        ApplyLocalTimestamp(vectorFunction.get(), rowSize, result);
    }
};

// Merged: BasicLocalTimestamp + SingleRow + MatchesSystemEpochMillis + VectorFunctionPathWithSessionTimezone
TEST_F(LocalTimestampTest, MatchesSessionTimezoneViaFullVectorPath)
{
    std::cout << "=== Test: localtimestamp matches session timezone via full vector path ===" << std::endl;

    const std::string tzName = "Asia/Shanghai";
    const auto *sessionTz = tz::locateZone(tzName);

    std::unordered_map<std::string, std::string> configValues;
    configValues[config::QueryConfig::kSessionTimezone] = tzName;

    int64_t expectedBefore = GetSessionTzLocalTimestampMillis(sessionTz);

    int32_t rowSize = 5;
    BaseVector *result = nullptr;
    ExecuteLocalTimestamp(rowSize, result, configValues);

    int64_t expectedAfter = GetSessionTzLocalTimestampMillis(sessionTz);

    ASSERT_NE(result, nullptr) << "Result is null";
    auto *resultVector = static_cast<Vector<int64_t> *>(result);
    ASSERT_NE(resultVector, nullptr) << "Result vector is null";

    for (int32_t i = 0; i < rowSize; ++i) {
        EXPECT_FALSE(resultVector->IsNull(i)) << "Unexpected NULL at index " << i;
        if (!resultVector->IsNull(i)) {
            int64_t actual = resultVector->GetValue(i);
            std::cout << "Row " << i << ": localtimestamp=" << actual << " millis since epoch" << std::endl;
            EXPECT_GT(actual, 0) << "localtimestamp should be > 0 at row " << i;
            int64_t minDiff = std::min(std::abs(actual - expectedBefore), std::abs(actual - expectedAfter));
            EXPECT_LE(minDiff, kMillisTolerance)
                << "localtimestamp (" << actual << ") should match session timezone " << tzName
                << " (expected before=" << expectedBefore << ", after=" << expectedAfter << ")";
        }
    }

    delete result;
}

// Merged: ConstantResultAcrossRows + LargeBatch + NonNullableResult
TEST_F(LocalTimestampTest, ConstantAndNonNullResult)
{
    std::cout << "=== Test: localtimestamp constant and non-null result across large batch ===" << std::endl;

    int32_t rowSize = 1000;
    BaseVector *result = nullptr;
    ExecuteLocalTimestamp(rowSize, result);

    ASSERT_NE(result, nullptr) << "Result is null";
    auto *resultVector = static_cast<Vector<int64_t> *>(result);
    ASSERT_NE(resultVector, nullptr) << "Result vector is null";

    EXPECT_FALSE(resultVector->IsNull(0));
    int64_t firstValue = resultVector->GetValue(0);

    for (int32_t i = 0; i < rowSize; ++i) {
        EXPECT_FALSE(resultVector->IsNull(i)) << "localtimestamp should never return NULL - row " << i;
        if (!resultVector->IsNull(i)) {
            EXPECT_EQ(resultVector->GetValue(i), firstValue)
                << "All rows should have the same localtimestamp value, but row " << i << " differs";
        }
    }

    std::cout << "Large batch (" << rowSize << " rows): all rows = " << firstValue << " millis since epoch"
              << std::endl;

    delete result;
}

// Merged: DirectStructTest + MillisPrecisionGuard
TEST_F(LocalTimestampTest, DirectStructTest)
{
    std::cout << "=== Test: LocalTimestampFunction struct direct test ===" << std::endl;

    int64_t beforeMillis = GetCurrentEpochMillis();

    LocalTimestampFunction<int64_t> fn;
    std::vector<DataTypeId> inputTypes;
    std::unordered_map<std::string, std::string> configValues;
    config::QueryConfig queryConfig(configValues);
    fn.initialize(inputTypes, queryConfig);

    int64_t afterMillis = GetCurrentEpochMillis();

    // First call - get the cached value
    int64_t result1 = 0;
    vectorization::Status status1 = fn.call(result1);
    EXPECT_TRUE(status1.ok()) << "call() should return OK status";

    std::cout << "Direct call result: " << result1 << " millis since epoch" << std::endl;

    // Verify positive
    EXPECT_GT(result1, 0) << "localtimestamp should be > 0";

    // Verify within valid drift of system epoch millis (primary regression guard)
    int64_t distBefore = std::abs(result1 - beforeMillis);
    int64_t distAfter = std::abs(result1 - afterMillis);
    int64_t minDist = std::min(distBefore, distAfter);
    EXPECT_LE(minDist, MAX_VALID_DRIFT_MILLIS)
        << "localtimestamp (" << result1 << ") drift from system epoch millis (before=" << beforeMillis
        << ", after=" << afterMillis << ") exceeds " << MAX_VALID_DRIFT_MILLIS
        << " ms; likely a timezone-offset drift or a unit mismatch";

    // Millis precision guard: the value should be in millisecond units (~1.7e12 for current era),
    // not seconds (~1.7e9) or microseconds (~1.7e15).
    EXPECT_GE(result1, kMinEpochMillis) << "localtimestamp (" << result1
        << ") is below the millis-range lower bound; a value near 1.7e9 suggests "
        << "seconds, near 1.7e15 suggests micros.";
    EXPECT_LE(result1, kMaxEpochMillis) << "localtimestamp (" << result1
        << ") is above the millis-range upper bound; this indicates the value is "
        << "not in millisecond units.";

    // Second call - should return the same cached value (batch-mode semantic)
    int64_t result2 = 0;
    vectorization::Status status2 = fn.call(result2);
    EXPECT_TRUE(status2.ok()) << "Second call() should return OK status";
    EXPECT_EQ(result2, result1) << "call() should return the same cached value on repeated calls";
}

// Merged: UsesSessionTimezone + UsesSessionTimezoneLosAngeles + UtcSessionTimezone
TEST_F(LocalTimestampTest, UsesSessionTimezone)
{
    std::cout << "=== Test: localtimestamp uses session timezone (UTC / +8 / -8 DST) ===" << std::endl;

    const std::vector<std::string> tzNames = {"UTC", "Asia/Shanghai", "America/Los_Angeles"};

    for (const auto &tzName : tzNames) {
        const auto *sessionTz = tz::locateZone(tzName);

        int64_t expectedBefore = GetSessionTzLocalTimestampMillis(sessionTz);

        LocalTimestampFunction<int64_t> fn;
        std::vector<DataTypeId> inputTypes;
        std::unordered_map<std::string, std::string> configValues;
        configValues[config::QueryConfig::kSessionTimezone] = tzName;
        config::QueryConfig queryConfig(configValues);
        fn.initialize(inputTypes, queryConfig);

        int64_t expectedAfter = GetSessionTzLocalTimestampMillis(sessionTz);

        int64_t actual = 0;
        ASSERT_TRUE(fn.call(actual).ok()) << "call() failed for session timezone " << tzName;

        int64_t minDiff = std::min(std::abs(actual - expectedBefore), std::abs(actual - expectedAfter));
        EXPECT_LE(minDiff, kMillisTolerance)
            << "localtimestamp (" << actual << ") should match session timezone " << tzName
            << " (expected before=" << expectedBefore << ", after=" << expectedAfter << ")";

        std::cout << "tz=" << tzName << ": actual=" << actual << " ms, minDiff=" << minDiff << " ms" << std::endl;
    }
}

// When no session timezone is set, localtimestamp falls back to OS local time.
TEST_F(LocalTimestampTest, NoSessionTimezoneFallsBackToOsLocal)
{
    std::cout << "=== Test: localtimestamp falls back to OS local time without session timezone ===" << std::endl;

    int64_t expectedBefore = GetOsLocalTimestampMillis();

    LocalTimestampFunction<int64_t> fn;
    std::vector<DataTypeId> inputTypes;
    std::unordered_map<std::string, std::string> configValues;
    config::QueryConfig queryConfig(configValues);
    fn.initialize(inputTypes, queryConfig);

    int64_t expectedAfter = GetOsLocalTimestampMillis();

    int64_t actual = 0;
    ASSERT_TRUE(fn.call(actual).ok());

    int64_t minDiff = std::min(std::abs(actual - expectedBefore), std::abs(actual - expectedAfter));
    EXPECT_LE(minDiff, kMillisTolerance)
        << "localtimestamp with no session timezone should match OS local time"
        << " (expected before=" << expectedBefore << ", after=" << expectedAfter << ")";
}

// localtimestamp differs from current_timestamp by the timezone offset (in non-UTC zones).
TEST_F(LocalTimestampTest, DiffersFromCurrentTimestampInNonUtcZone)
{
    std::cout << "=== Test: localtimestamp differs from current_timestamp in non-UTC zone ===" << std::endl;

    // Determine the current local timezone offset in milliseconds.
    std::time_t now = std::time(nullptr);
    std::tm tmLocal {};
    localtime_r(&now, &tmLocal);
    int64_t tzOffsetMillis = static_cast<int64_t>(tmLocal.tm_gmtoff) * 1000LL;

    std::cout << "Local timezone offset from UTC: " << tzOffsetMillis << " ms ("
              << (tmLocal.tm_gmtoff / 3600.0) << " hours)" << std::endl;

    if (tzOffsetMillis == 0) {
        std::cout << "Local timezone is UTC; skipping differentiation test." << std::endl;
        GTEST_SKIP() << "Local timezone is UTC; LocalTimestampFunction and "
                     << "CurrentTimestampFunction are indistinguishable here.";
    }

    std::vector<DataTypeId> inputTypes;
    std::unordered_map<std::string, std::string> configValues;
    config::QueryConfig queryConfig(configValues);

    CurrentTimestampFunction<int64_t> ctFn;
    LocalTimestampFunction<int64_t> ltFn;
    ctFn.initialize(inputTypes, queryConfig);
    ltFn.initialize(inputTypes, queryConfig);

    int64_t ctValue = 0;
    int64_t ltValue = 0;
    ASSERT_TRUE(ctFn.call(ctValue).ok());
    ASSERT_TRUE(ltFn.call(ltValue).ok());

    std::cout << "current_timestamp: " << ctValue << " ms" << std::endl;
    std::cout << "localtimestamp:    " << ltValue << " ms" << std::endl;
    std::cout << "Difference (lt - ct): " << (ltValue - ctValue) << " ms" << std::endl;

    int64_t diff = std::abs((ltValue - ctValue) - tzOffsetMillis);
    constexpr int64_t SLACK_MILLIS = 5LL * 1000LL;
    EXPECT_LE(diff, SLACK_MILLIS)
        << "localtimestamp - current_timestamp (" << (ltValue - ctValue)
        << ") should match the timezone offset (" << tzOffsetMillis << ").";
}

// New: empty batch boundary case (rowSize=0). Verifies no crash and an empty result vector.
TEST_F(LocalTimestampTest, EmptyBatch)
{
    std::cout << "=== Test: localtimestamp with empty batch (rowSize=0) ===" << std::endl;

    int32_t rowSize = 0;
    BaseVector *result = nullptr;
    ExecuteLocalTimestamp(rowSize, result);

    ASSERT_NE(result, nullptr) << "Result is null even for empty batch";
    EXPECT_EQ(result->GetSize(), 0) << "Empty batch should produce a 0-size result vector";

    std::cout << "Empty batch produced a result vector of size " << result->GetSize() << std::endl;

    delete result;
}

// New: multi-batch reuse. The same vector function instance is applied to two batches;
TEST_F(LocalTimestampTest, MultiBatchReuse)
{
    std::cout << "=== Test: localtimestamp multi-batch reuse returns same cached value ===" << std::endl;

    auto vectorFunction = CreateVectorFunction();
    ASSERT_NE(vectorFunction, nullptr) << "Function localtimestamp creation failed";

    // First batch
    int32_t rowSize1 = 3;
    BaseVector *result1 = nullptr;
    ApplyLocalTimestamp(vectorFunction.get(), rowSize1, result1);
    ASSERT_NE(result1, nullptr) << "First batch result is null";
    auto *resultVector1 = static_cast<Vector<int64_t> *>(result1);
    ASSERT_NE(resultVector1, nullptr) << "First batch result vector is null";

    EXPECT_FALSE(resultVector1->IsNull(0));
    int64_t firstBatchValue = resultVector1->GetValue(0);
    EXPECT_GT(firstBatchValue, 0) << "First batch localtimestamp should be > 0";

    // Second batch (reusing the same function instance)
    int32_t rowSize2 = 4;
    BaseVector *result2 = nullptr;
    ApplyLocalTimestamp(vectorFunction.get(), rowSize2, result2);
    ASSERT_NE(result2, nullptr) << "Second batch result is null";
    auto *resultVector2 = static_cast<Vector<int64_t> *>(result2);
    ASSERT_NE(resultVector2, nullptr) << "Second batch result vector is null";

    // All rows in both batches must share the same cached value
    for (int32_t i = 0; i < rowSize1; ++i) {
        EXPECT_EQ(resultVector1->GetValue(i), firstBatchValue)
            << "First batch row " << i << " should match cached value";
    }
    for (int32_t i = 0; i < rowSize2; ++i) {
        EXPECT_EQ(resultVector2->GetValue(i), firstBatchValue)
            << "Second batch row " << i << " should return the same cached value as the first batch";
    }

    std::cout << "Both batches returned identical cached value: " << firstBatchValue << " millis since epoch"
              << std::endl;

    delete result1;
    delete result2;
}
