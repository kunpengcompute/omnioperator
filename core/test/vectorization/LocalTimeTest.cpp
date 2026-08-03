/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: LocalTime function unit tests
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
constexpr int64_t MILLIS_PER_DAY = 86400000LL;
// LocalTimeFunction falls back to OS local time when no session timezone is configured.
int64_t GetCurrentMillisSinceMidnight()
{
    auto now = std::chrono::system_clock::now();
    auto durationSinceEpoch = now.time_since_epoch();
    auto secsSinceEpoch = std::chrono::duration_cast<std::chrono::seconds>(durationSinceEpoch);
    auto millisSinceEpoch = std::chrono::duration_cast<std::chrono::milliseconds>(durationSinceEpoch);
    int64_t subSecondMillis = static_cast<int64_t>((millisSinceEpoch - secsSinceEpoch).count());
    std::time_t timeNow = static_cast<std::time_t>(secsSinceEpoch.count());
    std::tm tmVal {};
    localtime_r(&timeNow, &tmVal);
    return static_cast<int64_t>(tmVal.tm_hour * 3600 + tmVal.tm_min * 60 + tmVal.tm_sec) * 1000LL
        + subSecondMillis;
}

// Compute expected localtime (millis since midnight) in a given session timezone.
int64_t GetSessionTzLocalTimeMillis(const tz::TimeZone *sessionTz)
{
    auto now = std::chrono::system_clock::now();
    auto utcMillis = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    Timestamp ts = Timestamp::fromMillis(utcMillis);
    if (sessionTz != nullptr) {
        ts.toTimezone(*sessionTz);
    }
    std::tm localTm {};
    Timestamp::epochToCalendarUtc(ts.getSeconds(), localTm);
    return static_cast<int64_t>(localTm.tm_hour * 3600 + localTm.tm_min * 60 + localTm.tm_sec) * 1000LL
        + static_cast<int64_t>(ts.getNanos() / 1'000'000);
}

// Compute circular distance (in millis) between two times-of-day, handling midnight wrap-around.
int64_t CircularDistanceMillis(int64_t a, int64_t b)
{
    int64_t diff = std::abs(a - b);
    return std::min(diff, MILLIS_PER_DAY - diff);
}

// Tolerance for time-of-day matching checks on slow CI.
static constexpr int64_t kMillisTolerance = 10'000; // 10 seconds
} // namespace

class LocalTimeTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        RegisterFunctions::Register();
    }

    // The returned instance has already called initialize() which caches the current time-of-day.
    static std::unique_ptr<VectorFunction> CreateVectorFunction(
        const std::unordered_map<std::string, std::string> &configValues = {})
    {
        std::vector<DataTypeId> argTypes = {};
        auto signature = std::make_shared<FunctionSignature>("flink_localtime", argTypes, OMNI_LONG);

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
    static void ApplyLocalTime(VectorFunction *vectorFunction, int32_t rowSize, BaseVector *&result)
    {
        ExecutionContext context;
        context.SetResultRowSize(rowSize);
        std::stack<BaseVector *> args;
        auto resultType = std::make_shared<DataType>(OMNI_LONG);
        vectorFunction->Apply(args, resultType, result, &context);
    }

    // Convenience: create a fresh function instance and execute once.
    static void ExecuteLocalTime(int32_t rowSize, BaseVector *&result,
        const std::unordered_map<std::string, std::string> &configValues = {})
    {
        auto vectorFunction = CreateVectorFunction(configValues);
        ASSERT_NE(vectorFunction, nullptr) << "Function localtime creation failed";
        ApplyLocalTime(vectorFunction.get(), rowSize, result);
    }
};

// Merged: BasicLocalTime + SingleRow
TEST_F(LocalTimeTest, BasicLocalTime)
{
    std::cout << "=== Test: localtime basic cases ===" << std::endl;

    int32_t rowSize = 5;
    BaseVector *result = nullptr;
    ExecuteLocalTime(rowSize, result);

    ASSERT_NE(result, nullptr) << "Result is null";
    auto *resultVector = static_cast<Vector<int64_t> *>(result);
    ASSERT_NE(resultVector, nullptr) << "Result vector is null";

    for (int32_t i = 0; i < rowSize; ++i) {
        EXPECT_FALSE(resultVector->IsNull(i))
            << "Unexpected NULL at index " << i << " for localtime";
        if (!resultVector->IsNull(i)) {
            int64_t actual = resultVector->GetValue(i);
            std::cout << "Row " << i << ": localtime=" << actual << " millis since midnight" << std::endl;
            EXPECT_GE(actual, 0) << "localtime should be >= 0 at row " << i;
            EXPECT_LT(actual, MILLIS_PER_DAY) << "localtime should be < " << MILLIS_PER_DAY << " at row " << i;
        }
    }

    delete result;
}

// Merged: ConstantResultAcrossRows + LargeBatch + NonNullableResult
TEST_F(LocalTimeTest, ConstantAndNonNullResult)
{
    std::cout << "=== Test: localtime constant and non-null result across large batch ===" << std::endl;

    int32_t rowSize = 1000;
    BaseVector *result = nullptr;
    ExecuteLocalTime(rowSize, result);

    ASSERT_NE(result, nullptr) << "Result is null";
    auto *resultVector = static_cast<Vector<int64_t> *>(result);
    ASSERT_NE(resultVector, nullptr) << "Result vector is null";

    EXPECT_FALSE(resultVector->IsNull(0));
    int64_t firstValue = resultVector->GetValue(0);

    for (int32_t i = 0; i < rowSize; ++i) {
        EXPECT_FALSE(resultVector->IsNull(i)) << "localtime should never return NULL - row " << i;
        if (!resultVector->IsNull(i)) {
            EXPECT_EQ(resultVector->GetValue(i), firstValue)
                << "All rows should have the same localtime value, but row " << i << " differs";
        }
    }

    std::cout << "Large batch (" << rowSize << " rows): all rows = " << firstValue << " millis since midnight"
              << std::endl;

    delete result;
}

// Test localtime matches the system local time (within tolerance) via the full VectorFunction path
// (no session timezone -> OS local fallback).
TEST_F(LocalTimeTest, MatchesSystemLocalTime)
{
    std::cout << "=== Test: localtime matches system local time ===" << std::endl;

    // Capture system time before function creation (initialize() runs during createVectorFunction)
    int64_t beforeMillis = GetCurrentMillisSinceMidnight();

    int32_t rowSize = 1;
    BaseVector *result = nullptr;
    ExecuteLocalTime(rowSize, result);

    // Capture system time after function creation
    int64_t afterMillis = GetCurrentMillisSinceMidnight();

    ASSERT_NE(result, nullptr) << "Result is null";
    auto *resultVector = static_cast<Vector<int64_t> *>(result);
    ASSERT_NE(resultVector, nullptr) << "Result vector is null";

    EXPECT_FALSE(resultVector->IsNull(0));
    int64_t actual = resultVector->GetValue(0);

    std::cout << "localtime result: " << actual << " millis" << std::endl;
    std::cout << "Before initialize: " << beforeMillis << " millis" << std::endl;
    std::cout << "After initialize: " << afterMillis << " millis" << std::endl;

    int64_t minDist = std::min(CircularDistanceMillis(actual, beforeMillis),
                               CircularDistanceMillis(actual, afterMillis));
    EXPECT_LE(minDist, kMillisTolerance)
        << "localtime (" << actual << ") should be within " << kMillisTolerance
        << " millis of system local time. Before=" << beforeMillis << ", After=" << afterMillis;

    delete result;
}

// Merged: DirectStructTest + MillisPrecisionGuard
TEST_F(LocalTimeTest, DirectStructTest)
{
    std::cout << "=== Test: LocalTimeFunction struct direct test ===" << std::endl;

    int64_t beforeMillis = GetCurrentMillisSinceMidnight();

    LocalTimeFunction<int64_t> fn;
    std::vector<DataTypeId> inputTypes;
    std::unordered_map<std::string, std::string> configValues;
    config::QueryConfig queryConfig(configValues);
    fn.initialize(inputTypes, queryConfig);

    int64_t afterMillis = GetCurrentMillisSinceMidnight();

    // First call - get the cached value
    int64_t result1 = 0;
    vectorization::Status status1 = fn.call(result1);
    EXPECT_TRUE(status1.ok()) << "call() should return OK status";

    std::cout << "Direct call result: " << result1 << " millis since midnight" << std::endl;

    // Verify valid range (millis precision guard: [0, 86400000), not seconds)
    EXPECT_GE(result1, 0) << "localtime (" << result1
        << ") should be >= 0; a value near 86400 suggests seconds.";
    EXPECT_LT(result1, MILLIS_PER_DAY) << "localtime (" << result1
        << ") should be < " << MILLIS_PER_DAY
        << "; a value near 86400 suggests seconds.";

    // Verify close to system local time
    int64_t minDist = std::min(CircularDistanceMillis(result1, beforeMillis),
                               CircularDistanceMillis(result1, afterMillis));
    EXPECT_LE(minDist, kMillisTolerance)
        << "localtime (" << result1 << ") should be within " << kMillisTolerance
        << " millis of system local time. Before=" << beforeMillis << ", After=" << afterMillis;

    // Second call - should return the same cached value (batch-mode semantic)
    int64_t result2 = 0;
    vectorization::Status status2 = fn.call(result2);
    EXPECT_TRUE(status2.ok()) << "Second call() should return OK status";
    EXPECT_EQ(result2, result1) << "call() should return the same cached value on repeated calls";
}

// Merged: UsesSessionTimezone + UsesSessionTimezoneShanghai (+ new UTC zero-offset)
TEST_F(LocalTimeTest, UsesSessionTimezone)
{
    std::cout << "=== Test: localtime uses session timezone (UTC / +8 / -8 DST) ===" << std::endl;

    const std::vector<std::string> tzNames = {"UTC", "Asia/Shanghai", "America/Los_Angeles"};

    for (const auto &tzName : tzNames) {
        const auto *sessionTz = tz::locateZone(tzName);

        int64_t expectedBefore = GetSessionTzLocalTimeMillis(sessionTz);

        LocalTimeFunction<int64_t> fn;
        std::vector<DataTypeId> inputTypes;
        std::unordered_map<std::string, std::string> configValues;
        configValues[config::QueryConfig::kSessionTimezone] = tzName;
        config::QueryConfig queryConfig(configValues);
        fn.initialize(inputTypes, queryConfig);

        int64_t expectedAfter = GetSessionTzLocalTimeMillis(sessionTz);

        int64_t actual = 0;
        ASSERT_TRUE(fn.call(actual).ok()) << "call() failed for session timezone " << tzName;

        int64_t minDiff = std::min(CircularDistanceMillis(actual, expectedBefore),
                                   CircularDistanceMillis(actual, expectedAfter));
        EXPECT_LE(minDiff, kMillisTolerance)
            << "localtime (" << actual << ") should match session timezone " << tzName
            << " (expected before=" << expectedBefore << ", after=" << expectedAfter << ")";

        std::cout << "tz=" << tzName << ": actual=" << actual << " ms, minDiff=" << minDiff << " ms" << std::endl;
    }
}

// localtime via the full VectorFunction path with session timezone config.
TEST_F(LocalTimeTest, VectorFunctionPathWithSessionTimezone)
{
    std::cout << "=== Test: localtime via VectorFunction path with session timezone ===" << std::endl;

    const std::string tzName = "Asia/Shanghai";
    const auto *sessionTz = tz::locateZone(tzName);

    std::unordered_map<std::string, std::string> configValues;
    configValues[config::QueryConfig::kSessionTimezone] = tzName;

    int64_t expectedBefore = GetSessionTzLocalTimeMillis(sessionTz);
    BaseVector *result = nullptr;
    ExecuteLocalTime(1, result, configValues);
    int64_t expectedAfter = GetSessionTzLocalTimeMillis(sessionTz);

    ASSERT_NE(result, nullptr);
    auto *resultVector = static_cast<Vector<int64_t> *>(result);
    ASSERT_NE(resultVector, nullptr);
    ASSERT_FALSE(resultVector->IsNull(0));

    int64_t actual = resultVector->GetValue(0);
    int64_t minDiff = std::min(CircularDistanceMillis(actual, expectedBefore),
                               CircularDistanceMillis(actual, expectedAfter));
    EXPECT_LE(minDiff, kMillisTolerance)
        << "localtime via VectorFunction path should match session timezone " << tzName;

    delete result;
}

// New: empty batch boundary case (rowSize=0). Verifies no crash and an empty result vector.
TEST_F(LocalTimeTest, EmptyBatch)
{
    std::cout << "=== Test: localtime with empty batch (rowSize=0) ===" << std::endl;

    int32_t rowSize = 0;
    BaseVector *result = nullptr;
    ExecuteLocalTime(rowSize, result);

    ASSERT_NE(result, nullptr) << "Result is null even for empty batch";
    EXPECT_EQ(result->GetSize(), 0) << "Empty batch should produce a 0-size result vector";

    std::cout << "Empty batch produced a result vector of size " << result->GetSize() << std::endl;

    delete result;
}

// New: multi-batch reuse. The same vector function instance is applied to two batches;
TEST_F(LocalTimeTest, MultiBatchReuse)
{
    std::cout << "=== Test: localtime multi-batch reuse returns same cached value ===" << std::endl;

    auto vectorFunction = CreateVectorFunction();
    ASSERT_NE(vectorFunction, nullptr) << "Function localtime creation failed";

    // First batch
    int32_t rowSize1 = 3;
    BaseVector *result1 = nullptr;
    ApplyLocalTime(vectorFunction.get(), rowSize1, result1);
    ASSERT_NE(result1, nullptr) << "First batch result is null";
    auto *resultVector1 = static_cast<Vector<int64_t> *>(result1);
    ASSERT_NE(resultVector1, nullptr) << "First batch result vector is null";

    EXPECT_FALSE(resultVector1->IsNull(0));
    int64_t firstBatchValue = resultVector1->GetValue(0);
    EXPECT_GE(firstBatchValue, 0) << "First batch localtime should be >= 0";
    EXPECT_LT(firstBatchValue, MILLIS_PER_DAY) << "First batch localtime should be < MILLIS_PER_DAY";

    // Second batch (reusing the same function instance)
    int32_t rowSize2 = 4;
    BaseVector *result2 = nullptr;
    ApplyLocalTime(vectorFunction.get(), rowSize2, result2);
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

    std::cout << "Both batches returned identical cached value: " << firstBatchValue << " millis since midnight"
              << std::endl;

    delete result1;
    delete result2;
}
