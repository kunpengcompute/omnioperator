/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: CurrentTimestamp function unit tests
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

constexpr int64_t MAX_TIMING_SLACK_MILLIS = 5LL * 1000LL; // 5 seconds

// Epoch millis for year 2000-01-01 and 2100-01-01; guards against unit bugs
// (seconds ~1e9, millis ~1.7e12, micros ~1.7e15).
constexpr int64_t kMinEpochMillis = 946684800000LL;  // 2000-01-01T00:00:00Z
constexpr int64_t kMaxEpochMillis = 4102444800000LL; // 2100-01-01T00:00:00Z
} // namespace

class CurrentTimestampTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        RegisterFunctions::Register();
    }

    // Create a vector function instance for current_timestamp().
    // The returned instance has already called initialize() which caches the current timestamp.
    static std::unique_ptr<VectorFunction> CreateVectorFunction()
    {
        std::vector<DataTypeId> argTypes = {};
        auto signature = std::make_shared<FunctionSignature>("current_timestamp", argTypes, OMNI_LONG);

        auto factoryIt = VectorFunction::simpleFunctionFactoryMap_.find(signature);
        if (factoryIt == VectorFunction::simpleFunctionFactoryMap_.end()) {
            return nullptr;
        }

        std::unordered_map<std::string, std::string> configValues;
        config::QueryConfig queryConfig(configValues);
        std::vector<BaseVector *> constantInputs;
        auto factory = factoryIt->second();
        return factory->createVectorFunction({}, queryConfig, constantInputs);
    }

    // Apply an existing vector function instance with the given row size.
    static void ApplyCurrentTimestamp(VectorFunction *vectorFunction, int32_t rowSize, BaseVector *&result)
    {
        ExecutionContext context;
        context.SetResultRowSize(rowSize);
        std::stack<BaseVector *> args;
        auto resultType = std::make_shared<DataType>(OMNI_LONG);
        vectorFunction->Apply(args, resultType, result, &context);
    }

    // Convenience: create a fresh function instance and execute once.
    static void ExecuteCurrentTimestamp(int32_t rowSize, BaseVector *&result)
    {
        auto vectorFunction = CreateVectorFunction();
        ASSERT_NE(vectorFunction, nullptr) << "Function current_timestamp creation failed";
        ApplyCurrentTimestamp(vectorFunction.get(), rowSize, result);
    }
};

// Merged: BasicCurrentTimestamp + SingleRow + MatchesSystemEpochMillis
TEST_F(CurrentTimestampTest, MatchesSystemEpochMillis)
{
    std::cout << "=== Test: current_timestamp matches system epoch millis ===" << std::endl;

    int64_t beforeMillis = GetCurrentEpochMillis();

    int32_t rowSize = 5;
    BaseVector *result = nullptr;
    ExecuteCurrentTimestamp(rowSize, result);

    int64_t afterMillis = GetCurrentEpochMillis();

    ASSERT_NE(result, nullptr) << "Result is null";
    auto *resultVector = static_cast<Vector<int64_t> *>(result);
    ASSERT_NE(resultVector, nullptr) << "Result vector is null";

    for (int32_t i = 0; i < rowSize; ++i) {
        EXPECT_FALSE(resultVector->IsNull(i)) << "Unexpected NULL at index " << i;
        if (!resultVector->IsNull(i)) {
            int64_t actual = resultVector->GetValue(i);
            std::cout << "Row " << i << ": current_timestamp=" << actual << " millis since epoch" << std::endl;
            EXPECT_GT(actual, 0) << "current_timestamp should be > 0 at row " << i;

            int64_t distBefore = std::abs(actual - beforeMillis);
            int64_t distAfter = std::abs(actual - afterMillis);
            int64_t minDist = std::min(distBefore, distAfter);
            EXPECT_LE(minDist, MAX_TIMING_SLACK_MILLIS)
                << "current_timestamp (" << actual << ") drift from system epoch millis (before=" << beforeMillis
                << ", after=" << afterMillis << ") exceeds " << MAX_TIMING_SLACK_MILLIS
                << " ms; likely a timezone-offset drift (LocalTimestamp-style bug) or a unit mismatch";
        }
    }

    delete result;
}

// Merged: ConstantResultAcrossRows + LargeBatch + NonNullableResult
TEST_F(CurrentTimestampTest, ConstantAndNonNullResult)
{
    std::cout << "=== Test: current_timestamp constant and non-null result across large batch ===" << std::endl;

    int32_t rowSize = 1000;
    BaseVector *result = nullptr;
    ExecuteCurrentTimestamp(rowSize, result);

    ASSERT_NE(result, nullptr) << "Result is null";
    auto *resultVector = static_cast<Vector<int64_t> *>(result);
    ASSERT_NE(resultVector, nullptr) << "Result vector is null";

    EXPECT_FALSE(resultVector->IsNull(0));
    int64_t firstValue = resultVector->GetValue(0);

    for (int32_t i = 0; i < rowSize; ++i) {
        EXPECT_FALSE(resultVector->IsNull(i)) << "current_timestamp should never return NULL - row " << i;
        if (!resultVector->IsNull(i)) {
            EXPECT_EQ(resultVector->GetValue(i), firstValue)
                << "All rows should have the same current_timestamp value, but row " << i << " differs";
        }
    }

    std::cout << "Large batch (" << rowSize << " rows): all rows = " << firstValue << " millis since epoch"
              << std::endl;

    delete result;
}

// Merged: DirectStructTest + MillisPrecisionGuard
TEST_F(CurrentTimestampTest, DirectStructTest)
{
    std::cout << "=== Test: CurrentTimestampFunction struct direct test ===" << std::endl;

    int64_t beforeMillis = GetCurrentEpochMillis();

    CurrentTimestampFunction<int64_t> fn;
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
    EXPECT_GT(result1, 0) << "current_timestamp should be > 0";

    // Verify within tight slack of system epoch millis (primary regression guard)
    int64_t distBefore = std::abs(result1 - beforeMillis);
    int64_t distAfter = std::abs(result1 - afterMillis);
    int64_t minDist = std::min(distBefore, distAfter);
    EXPECT_LE(minDist, MAX_TIMING_SLACK_MILLIS)
        << "current_timestamp (" << result1 << ") drift from system epoch millis (before=" << beforeMillis
        << ", after=" << afterMillis << ") exceeds " << MAX_TIMING_SLACK_MILLIS
        << " ms; likely a timezone-offset drift (LocalTimestamp-style bug) or a unit mismatch";

    // Millis precision guard: the value should be in millisecond units (~1.7e12 for current era),
    // not seconds (~1.7e9) or microseconds (~1.7e15).
    EXPECT_GE(result1, kMinEpochMillis) << "current_timestamp (" << result1
        << ") is below the millis-range lower bound; a value near 1.7e9 suggests "
        << "seconds, near 1.7e15 suggests micros.";
    EXPECT_LE(result1, kMaxEpochMillis) << "current_timestamp (" << result1
        << ") is above the millis-range upper bound; this indicates the value is "
        << "not in millisecond units.";

    // Second call - should return the same cached value (batch-mode semantic)
    int64_t result2 = 0;
    vectorization::Status status2 = fn.call(result2);
    EXPECT_TRUE(status2.ok()) << "Second call() should return OK status";
    EXPECT_EQ(result2, result1) << "call() should return the same cached value on repeated calls";
}

// current_timestamp must differ from localtimestamp in non-UTC zones 
TEST_F(CurrentTimestampTest, DiffersFromLocalTimestampInNonUtcZone)
{
    std::cout << "=== Test: current_timestamp differs from localtimestamp in non-UTC zone ===" << std::endl;

    // Determine the current local timezone offset in milliseconds.
    std::time_t now = std::time(nullptr);
    std::tm tmLocal {};
    localtime_r(&now, &tmLocal);
    // tm_gmtoff is the offset from UTC in seconds (POSIX extension, widely supported).
    int64_t tzOffsetMillis = static_cast<int64_t>(tmLocal.tm_gmtoff) * 1000LL;

    std::cout << "Local timezone offset from UTC: " << tzOffsetMillis << " ms ("
              << (tmLocal.tm_gmtoff / 3600.0) << " hours)" << std::endl;

    if (tzOffsetMillis == 0) {
        std::cout << "Local timezone is UTC; skipping differentiation test (both functions "
                  << "return identical values by definition)." << std::endl;
        GTEST_SKIP() << "Local timezone is UTC; CurrentTimestampFunction and "
                     << "LocalTimestampFunction are indistinguishable here.";
    }

    // Initialize both functions at roughly the same instant.
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
    constexpr int64_t SLACK_MILLIS = 5LL * 1000LL; // 5 seconds for timing jitter between two initialize() calls
    EXPECT_LE(diff, SLACK_MILLIS)
        << "localtimestamp - current_timestamp (" << (ltValue - ctValue)
        << ") should match the timezone offset (" << tzOffsetMillis
        << "). A mismatch suggests CurrentTimestampFunction was regressed into the "
        << "LocalTimestamp-style implementation.";
}

// current_timestamp returns UTC millis regardless of session timezone (TIMESTAMP_LTZ semantics).
TEST_F(CurrentTimestampTest, ReturnsUtcRegardlessOfSessionTimezone)
{
    std::cout << "=== Test: current_timestamp returns UTC regardless of session timezone ===" << std::endl;

    const std::string tzName = "America/Los_Angeles";

    int64_t utcBefore = GetCurrentEpochMillis();

    CurrentTimestampFunction<int64_t> fn;
    std::vector<DataTypeId> inputTypes;
    std::unordered_map<std::string, std::string> configValues;
    configValues[config::QueryConfig::kSessionTimezone] = tzName;
    config::QueryConfig queryConfig(configValues);
    fn.initialize(inputTypes, queryConfig);

    int64_t utcAfter = GetCurrentEpochMillis();

    int64_t actual = 0;
    ASSERT_TRUE(fn.call(actual).ok());

    EXPECT_GE(actual, utcBefore - MAX_TIMING_SLACK_MILLIS)
        << "current_timestamp (" << actual << ") should be >= UTC millis at start (" << utcBefore
        << ") even with session timezone " << tzName;
    EXPECT_LE(actual, utcAfter + MAX_TIMING_SLACK_MILLIS)
        << "current_timestamp (" << actual << ") should be <= UTC millis at end (" << utcAfter
        << ") even with session timezone " << tzName;
}

// New: empty batch boundary case (rowSize=0). Verifies no crash and an empty result vector.
TEST_F(CurrentTimestampTest, EmptyBatch)
{
    std::cout << "=== Test: current_timestamp with empty batch (rowSize=0) ===" << std::endl;

    int32_t rowSize = 0;
    BaseVector *result = nullptr;
    ExecuteCurrentTimestamp(rowSize, result);

    ASSERT_NE(result, nullptr) << "Result is null even for empty batch";
    EXPECT_EQ(result->GetSize(), 0) << "Empty batch should produce a 0-size result vector";

    std::cout << "Empty batch produced a result vector of size " << result->GetSize() << std::endl;

    delete result;
}

// New: multi-batch reuse. The same vector function instance is applied to two batches;
TEST_F(CurrentTimestampTest, MultiBatchReuse)
{
    std::cout << "=== Test: current_timestamp multi-batch reuse returns same cached value ===" << std::endl;

    auto vectorFunction = CreateVectorFunction();
    ASSERT_NE(vectorFunction, nullptr) << "Function current_timestamp creation failed";

    // First batch
    int32_t rowSize1 = 3;
    BaseVector *result1 = nullptr;
    ApplyCurrentTimestamp(vectorFunction.get(), rowSize1, result1);
    ASSERT_NE(result1, nullptr) << "First batch result is null";
    auto *resultVector1 = static_cast<Vector<int64_t> *>(result1);
    ASSERT_NE(resultVector1, nullptr) << "First batch result vector is null";

    EXPECT_FALSE(resultVector1->IsNull(0));
    int64_t firstBatchValue = resultVector1->GetValue(0);
    EXPECT_GT(firstBatchValue, 0) << "First batch current_timestamp should be > 0";

    // Second batch (reusing the same function instance)
    int32_t rowSize2 = 4;
    BaseVector *result2 = nullptr;
    ApplyCurrentTimestamp(vectorFunction.get(), rowSize2, result2);
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
