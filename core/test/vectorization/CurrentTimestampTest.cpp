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
// Real epoch microseconds from the system clock (UTC-based, the raw time_since_epoch).
int64_t GetCurrentEpochMicros()
{
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
}

constexpr int64_t MAX_TIMING_SLACK_MICROS = 5LL * 1000000LL; // 5 seconds
} // namespace

class CurrentTimestampTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        RegisterFunctions::Register();
    }

    // Execute current_timestamp() with given row size and return result vector.
    static void ExecuteCurrentTimestamp(int32_t rowSize, BaseVector *&result)
    {
        // Create function signature: current_timestamp() -> OMNI_LONG
        std::vector<DataTypeId> argTypes = {};
        auto signature = std::make_shared<FunctionSignature>("current_timestamp", argTypes, OMNI_LONG);

        // Find simple function factory
        auto factoryIt = VectorFunction::simpleFunctionFactoryMap_.find(signature);
        ASSERT_NE(factoryIt, VectorFunction::simpleFunctionFactoryMap_.end())
            << "Function factory current_timestamp not found";

        // Create QueryConfig (empty - CurrentTimestampFunction does not read any config)
        std::unordered_map<std::string, std::string> configValues;
        config::QueryConfig queryConfig(configValues);

        // Create vector function with config (this calls initialize() which caches current timestamp)
        std::vector<BaseVector *> constantInputs;
        auto factory = factoryIt->second();
        auto vectorFunction = factory->createVectorFunction({}, queryConfig, constantInputs);
        ASSERT_NE(vectorFunction, nullptr) << "Function current_timestamp creation failed";

        // Create execution context
        ExecutionContext context;
        context.SetResultRowSize(rowSize);

        // Prepare empty arguments stack (no input arguments)
        std::stack<BaseVector *> args;

        // Execute function
        auto resultType = std::make_shared<DataType>(OMNI_LONG);
        vectorFunction->Apply(args, resultType, result, &context);
    }
};

TEST_F(CurrentTimestampTest, BasicCurrentTimestamp)
{
    std::cout << "=== Test: current_timestamp basic cases ===" << std::endl;

    int64_t beforeMicros = GetCurrentEpochMicros();

    int32_t rowSize = 5;
    BaseVector *result = nullptr;
    ExecuteCurrentTimestamp(rowSize, result);

    int64_t afterMicros = GetCurrentEpochMicros();

    ASSERT_NE(result, nullptr) << "Result is null";
    auto *resultVector = static_cast<Vector<int64_t> *>(result);
    ASSERT_NE(resultVector, nullptr) << "Result vector is null";

    for (int32_t i = 0; i < rowSize; ++i) {
        EXPECT_FALSE(resultVector->IsNull(i))
            << "Unexpected NULL at index " << i << " for current_timestamp";
        if (!resultVector->IsNull(i)) {
            int64_t actual = resultVector->GetValue(i);
            std::cout << "Row " << i << ": current_timestamp=" << actual << " micros since epoch" << std::endl;
            EXPECT_GT(actual, 0) << "current_timestamp should be > 0 at row " << i;

            int64_t distBefore = std::abs(actual - beforeMicros);
            int64_t distAfter = std::abs(actual - afterMicros);
            int64_t minDist = std::min(distBefore, distAfter);
            EXPECT_LE(minDist, MAX_TIMING_SLACK_MICROS)
                << "current_timestamp (" << actual << ") drift from system epoch micros (before=" << beforeMicros
                << ", after=" << afterMicros << ") exceeds " << MAX_TIMING_SLACK_MICROS
                << " us; likely a timezone-offset drift (LocalTimestamp-style bug) or a unit mismatch";
        }
    }

    delete result;
}

// Test current_timestamp with a single row
TEST_F(CurrentTimestampTest, SingleRow)
{
    std::cout << "=== Test: current_timestamp single row ===" << std::endl;

    int64_t beforeMicros = GetCurrentEpochMicros();

    int32_t rowSize = 1;
    BaseVector *result = nullptr;
    ExecuteCurrentTimestamp(rowSize, result);

    int64_t afterMicros = GetCurrentEpochMicros();

    ASSERT_NE(result, nullptr) << "Result is null";
    auto *resultVector = static_cast<Vector<int64_t> *>(result);
    ASSERT_NE(resultVector, nullptr) << "Result vector is null";

    EXPECT_FALSE(resultVector->IsNull(0));
    int64_t actual = resultVector->GetValue(0);
    std::cout << "Single row: current_timestamp=" << actual << " micros since epoch" << std::endl;
    EXPECT_GT(actual, 0);
    int64_t minDist = std::min(std::abs(actual - beforeMicros), std::abs(actual - afterMicros));
    EXPECT_LE(minDist, MAX_TIMING_SLACK_MICROS)
        << "current_timestamp drift exceeds tight timing slack";

    delete result;
}

// Test current_timestamp returns constant value across all rows in the same batch (batch-mode semantic).
TEST_F(CurrentTimestampTest, ConstantResultAcrossRows)
{
    std::cout << "=== Test: current_timestamp constant result across all rows ===" << std::endl;

    int32_t rowSize = 100;
    BaseVector *result = nullptr;
    ExecuteCurrentTimestamp(rowSize, result);

    ASSERT_NE(result, nullptr) << "Result is null";
    auto *resultVector = static_cast<Vector<int64_t> *>(result);
    ASSERT_NE(resultVector, nullptr) << "Result vector is null";

    // Get the value from the first row
    EXPECT_FALSE(resultVector->IsNull(0));
    int64_t firstValue = resultVector->GetValue(0);

    // Verify ALL rows have the same value (batch-mode: initialize once, call returns cached value)
    for (int32_t i = 0; i < rowSize; ++i) {
        EXPECT_FALSE(resultVector->IsNull(i)) << "Unexpected NULL at index " << i;
        if (!resultVector->IsNull(i)) {
            int64_t actual = resultVector->GetValue(i);
            EXPECT_EQ(actual, firstValue)
                << "All rows should have the same current_timestamp value, but row " << i << " has " << actual
                << " instead of " << firstValue;
        }
    }

    std::cout << "All " << rowSize << " rows have value: " << firstValue << " micros since epoch" << std::endl;

    delete result;
}

// Test current_timestamp with a large batch
TEST_F(CurrentTimestampTest, LargeBatch)
{
    std::cout << "=== Test: current_timestamp with large batch ===" << std::endl;

    int32_t rowSize = 1000;
    BaseVector *result = nullptr;
    ExecuteCurrentTimestamp(rowSize, result);

    ASSERT_NE(result, nullptr) << "Result is null";
    auto *resultVector = static_cast<Vector<int64_t> *>(result);
    ASSERT_NE(resultVector, nullptr) << "Result vector is null";

    EXPECT_FALSE(resultVector->IsNull(0));
    int64_t firstValue = resultVector->GetValue(0);

    for (int32_t i = 0; i < rowSize; ++i) {
        EXPECT_FALSE(resultVector->IsNull(i)) << "Unexpected NULL at index " << i;
        if (!resultVector->IsNull(i)) {
            EXPECT_EQ(resultVector->GetValue(i), firstValue) << "Row " << i << " value mismatch";
        }
    }

    std::cout << "Large batch (" << rowSize << " rows): all rows = " << firstValue << " micros since epoch"
              << std::endl;

    delete result;
}

// Test current_timestamp matches the system epoch micros (within tight timing slack).
TEST_F(CurrentTimestampTest, MatchesSystemEpochMicros)
{
    std::cout << "=== Test: current_timestamp matches system epoch micros ===" << std::endl;

    // Capture system time before function creation (initialize() runs during createVectorFunction)
    int64_t beforeMicros = GetCurrentEpochMicros();

    int32_t rowSize = 1;
    BaseVector *result = nullptr;
    ExecuteCurrentTimestamp(rowSize, result);

    // Capture system time after function creation
    int64_t afterMicros = GetCurrentEpochMicros();

    ASSERT_NE(result, nullptr) << "Result is null";
    auto *resultVector = static_cast<Vector<int64_t> *>(result);
    ASSERT_NE(resultVector, nullptr) << "Result vector is null";

    EXPECT_FALSE(resultVector->IsNull(0));
    int64_t actual = resultVector->GetValue(0);

    std::cout << "current_timestamp result: " << actual << " micros" << std::endl;
    std::cout << "Before initialize: " << beforeMicros << " micros" << std::endl;
    std::cout << "After initialize: " << afterMicros << " micros" << std::endl;
    std::cout << "Drift from before: " << (actual - beforeMicros) << " us" << std::endl;
    std::cout << "Drift from after: " << (actual - afterMicros) << " us" << std::endl;

    int64_t distBefore = std::abs(actual - beforeMicros);
    int64_t distAfter = std::abs(actual - afterMicros);
    int64_t minDist = std::min(distBefore, distAfter);

    EXPECT_LE(minDist, MAX_TIMING_SLACK_MICROS)
        << "current_timestamp (" << actual << ") should be within " << MAX_TIMING_SLACK_MICROS
        << " us of system epoch micros. Before=" << beforeMicros << ", After=" << afterMicros
        << ". A drift matching the local timezone offset indicates a LocalTimestamp-style bug; "
        << "a drift of ~56552 years indicates a microsecond/millisecond unit mismatch.";

    delete result;
}

// Test current_timestamp always returns non-NULL values (current timestamp is always available)
TEST_F(CurrentTimestampTest, NonNullableResult)
{
    std::cout << "=== Test: current_timestamp non-nullable result ===" << std::endl;

    int32_t rowSize = 50;
    BaseVector *result = nullptr;
    ExecuteCurrentTimestamp(rowSize, result);

    ASSERT_NE(result, nullptr) << "Result is null";
    auto *resultVector = static_cast<Vector<int64_t> *>(result);
    ASSERT_NE(resultVector, nullptr) << "Result vector is null";

    for (int32_t i = 0; i < rowSize; ++i) {
        EXPECT_FALSE(resultVector->IsNull(i))
            << "current_timestamp should never return NULL - row " << i << " is NULL";
    }

    delete result;
}

// Test CurrentTimestampFunction struct directly (pure unit test without VectorFunction infrastructure).
TEST_F(CurrentTimestampTest, DirectStructTest)
{
    std::cout << "=== Test: CurrentTimestampFunction struct direct test ===" << std::endl;

    int64_t beforeMicros = GetCurrentEpochMicros();

    CurrentTimestampFunction<int64_t> fn;
    std::vector<DataTypeId> inputTypes;
    std::unordered_map<std::string, std::string> configValues;
    config::QueryConfig queryConfig(configValues);
    fn.initialize(inputTypes, queryConfig);

    int64_t afterMicros = GetCurrentEpochMicros();

    // First call - get the cached value
    int64_t result1 = 0;
    vectorization::Status status1 = fn.call(result1);
    EXPECT_TRUE(status1.ok()) << "call() should return OK status";

    std::cout << "Direct call result: " << result1 << " micros since epoch" << std::endl;

    // Verify positive
    EXPECT_GT(result1, 0) << "current_timestamp should be > 0";

    // Verify within tight slack of system epoch micros (primary regression guard)
    int64_t distBefore = std::abs(result1 - beforeMicros);
    int64_t distAfter = std::abs(result1 - afterMicros);
    int64_t minDist = std::min(distBefore, distAfter);
    EXPECT_LE(minDist, MAX_TIMING_SLACK_MICROS)
        << "current_timestamp (" << result1 << ") drift from system epoch micros (before=" << beforeMicros
        << ", after=" << afterMicros << ") exceeds " << MAX_TIMING_SLACK_MICROS
        << " us; likely a timezone-offset drift (LocalTimestamp-style bug) or a unit mismatch";

    // Second call - should return the same cached value (batch-mode semantic)
    int64_t result2 = 0;
    vectorization::Status status2 = fn.call(result2);
    EXPECT_TRUE(status2.ok()) << "Second call() should return OK status";
    EXPECT_EQ(result2, result1) << "call() should return the same cached value on repeated calls";
}

TEST_F(CurrentTimestampTest, DiffersFromLocalTimestampInNonUtcZone)
{
    std::cout << "=== Test: current_timestamp differs from localtimestamp in non-UTC zone ===" << std::endl;

    // Determine the current local timezone offset in microseconds.
    std::time_t now = std::time(nullptr);
    std::tm tmLocal {};
    localtime_r(&now, &tmLocal);
    // tm_gmtoff is the offset from UTC in seconds (POSIX extension, widely supported).
    int64_t tzOffsetMicros = static_cast<int64_t>(tmLocal.tm_gmtoff) * 1000000LL;

    std::cout << "Local timezone offset from UTC: " << tzOffsetMicros << " us ("
              << (tmLocal.tm_gmtoff / 3600.0) << " hours)" << std::endl;

    if (tzOffsetMicros == 0) {
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

    std::cout << "current_timestamp: " << ctValue << " us" << std::endl;
    std::cout << "localtimestamp:    " << ltValue << " us" << std::endl;
    std::cout << "Difference (lt - ct): " << (ltValue - ctValue) << " us" << std::endl;

    int64_t diff = std::abs((ltValue - ctValue) - tzOffsetMicros);
    constexpr int64_t SLACK_MICROS = 5LL * 1000000LL; // 5 seconds for timing jitter between two initialize() calls
    EXPECT_LE(diff, SLACK_MICROS)
        << "localtimestamp - current_timestamp (" << (ltValue - ctValue)
        << ") should match the timezone offset (" << tzOffsetMicros
        << "). A mismatch suggests CurrentTimestampFunction was regressed into the "
        << "LocalTimestamp-style implementation.";
}
