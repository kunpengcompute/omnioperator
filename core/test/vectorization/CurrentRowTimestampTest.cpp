/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: CurrentRowTimestamp function unit tests
 */

#include <gtest/gtest.h>
#include <iostream>
#include <vector>
#include <ctime>
#include <chrono>
#include <thread>
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

// Sleep duration used to prove per-record evaluation (call() reads fresh clock,
// not a cached value). 50ms is well above clock granularity and scheduling jitter.
constexpr int64_t PER_RECORD_SLEEP_MILLIS = 50LL;

// Minimum acceptable delta between two call() invocations separated by
// PER_RECORD_SLEEP_MILLIS. A cached implementation would return delta == 0.
// 30000us (30ms) gives slack below the 50ms sleep for OS scheduler imprecision.
constexpr int64_t PER_RECORD_MIN_DELTA_MICROS = 30000LL;
} // namespace

class CurrentRowTimestampTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        RegisterFunctions::Register();
    }

    // Execute current_row_timestamp() with given row size and return result vector.
    static void ExecuteCurrentRowTimestamp(int32_t rowSize, BaseVector *&result)
    {
        // Create function signature: current_row_timestamp() -> OMNI_LONG
        std::vector<DataTypeId> argTypes = {};
        auto signature = std::make_shared<FunctionSignature>("current_row_timestamp", argTypes, OMNI_LONG);

        // Find simple function factory
        auto factoryIt = VectorFunction::simpleFunctionFactoryMap_.find(signature);
        ASSERT_NE(factoryIt, VectorFunction::simpleFunctionFactoryMap_.end())
            << "Function factory current_row_timestamp not found";

        // Create QueryConfig (empty - CurrentRowTimestampFunction does not read any config)
        std::unordered_map<std::string, std::string> configValues;
        config::QueryConfig queryConfig(configValues);

        // Create vector function with config (initialize() is a no-op here)
        std::vector<BaseVector *> constantInputs;
        auto factory = factoryIt->second();
        auto vectorFunction = factory->createVectorFunction({}, queryConfig, constantInputs);
        ASSERT_NE(vectorFunction, nullptr) << "Function current_row_timestamp creation failed";

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

TEST_F(CurrentRowTimestampTest, BasicCurrentRowTimestamp)
{
    std::cout << "=== Test: current_row_timestamp basic cases ===" << std::endl;

    int64_t beforeMicros = GetCurrentEpochMicros();

    int32_t rowSize = 5;
    BaseVector *result = nullptr;
    ExecuteCurrentRowTimestamp(rowSize, result);

    int64_t afterMicros = GetCurrentEpochMicros();

    ASSERT_NE(result, nullptr) << "Result is null";
    auto *resultVector = static_cast<Vector<int64_t> *>(result);
    ASSERT_NE(resultVector, nullptr) << "Result vector is null";

    for (int32_t i = 0; i < rowSize; ++i) {
        EXPECT_FALSE(resultVector->IsNull(i))
            << "Unexpected NULL at index " << i << " for current_row_timestamp";
        if (!resultVector->IsNull(i)) {
            int64_t actual = resultVector->GetValue(i);
            std::cout << "Row " << i << ": current_row_timestamp=" << actual << " micros since epoch" << std::endl;
            EXPECT_GT(actual, 0) << "current_row_timestamp should be > 0 at row " << i;

            int64_t distBefore = std::abs(actual - beforeMicros);
            int64_t distAfter = std::abs(actual - afterMicros);
            int64_t minDist = std::min(distBefore, distAfter);
            EXPECT_LE(minDist, MAX_TIMING_SLACK_MICROS)
                << "current_row_timestamp (" << actual << ") drift from system epoch micros (before=" << beforeMicros
                << ", after=" << afterMicros << ") exceeds " << MAX_TIMING_SLACK_MICROS
                << " us; likely a timezone-offset drift (LocalTimestamp-style bug) or a unit mismatch";
        }
    }

    delete result;
}

// Test current_row_timestamp with a single row
TEST_F(CurrentRowTimestampTest, SingleRow)
{
    std::cout << "=== Test: current_row_timestamp single row ===" << std::endl;

    int64_t beforeMicros = GetCurrentEpochMicros();

    int32_t rowSize = 1;
    BaseVector *result = nullptr;
    ExecuteCurrentRowTimestamp(rowSize, result);

    int64_t afterMicros = GetCurrentEpochMicros();

    ASSERT_NE(result, nullptr) << "Result is null";
    auto *resultVector = static_cast<Vector<int64_t> *>(result);
    ASSERT_NE(resultVector, nullptr) << "Result vector is null";

    EXPECT_FALSE(resultVector->IsNull(0));
    int64_t actual = resultVector->GetValue(0);
    std::cout << "Single row: current_row_timestamp=" << actual << " micros since epoch" << std::endl;
    EXPECT_GT(actual, 0);
    int64_t minDist = std::min(std::abs(actual - beforeMicros), std::abs(actual - afterMicros));
    EXPECT_LE(minDist, MAX_TIMING_SLACK_MICROS)
        << "current_row_timestamp drift exceeds tight timing slack";

    delete result;
}

// Test current_row_timestamp always returns non-NULL values (current timestamp is always available)
TEST_F(CurrentRowTimestampTest, NonNullableResult)
{
    std::cout << "=== Test: current_row_timestamp non-nullable result ===" << std::endl;

    int32_t rowSize = 50;
    BaseVector *result = nullptr;
    ExecuteCurrentRowTimestamp(rowSize, result);

    ASSERT_NE(result, nullptr) << "Result is null";
    auto *resultVector = static_cast<Vector<int64_t> *>(result);
    ASSERT_NE(resultVector, nullptr) << "Result vector is null";

    for (int32_t i = 0; i < rowSize; ++i) {
        EXPECT_FALSE(resultVector->IsNull(i))
            << "current_row_timestamp should never return NULL - row " << i << " is NULL";
    }

    delete result;
}

// Test current_row_timestamp with a large batch.
TEST_F(CurrentRowTimestampTest, LargeBatch)
{
    std::cout << "=== Test: current_row_timestamp with large batch ===" << std::endl;

    int64_t beforeMicros = GetCurrentEpochMicros();

    int32_t rowSize = 1000;
    BaseVector *result = nullptr;
    ExecuteCurrentRowTimestamp(rowSize, result);

    int64_t afterMicros = GetCurrentEpochMicros();

    ASSERT_NE(result, nullptr) << "Result is null";
    auto *resultVector = static_cast<Vector<int64_t> *>(result);
    ASSERT_NE(resultVector, nullptr) << "Result vector is null";

    for (int32_t i = 0; i < rowSize; ++i) {
        EXPECT_FALSE(resultVector->IsNull(i)) << "Unexpected NULL at index " << i;
        if (!resultVector->IsNull(i)) {
            int64_t actual = resultVector->GetValue(i);
            EXPECT_GT(actual, 0) << "Row " << i << " should be > 0";
            int64_t distBefore = std::abs(actual - beforeMicros);
            int64_t distAfter = std::abs(actual - afterMicros);
            int64_t minDist = std::min(distBefore, distAfter);
            EXPECT_LE(minDist, MAX_TIMING_SLACK_MICROS)
                << "Row " << i << " value " << actual << " drift exceeds timing slack";
        }
    }

    std::cout << "Large batch (" << rowSize << " rows) all within valid time window" << std::endl;

    delete result;
}

// Test current_row_timestamp matches the system epoch micros (within tight timing slack).
TEST_F(CurrentRowTimestampTest, MatchesSystemEpochMicros)
{
    std::cout << "=== Test: current_row_timestamp matches system epoch micros ===" << std::endl;

    int64_t beforeMicros = GetCurrentEpochMicros();

    int32_t rowSize = 1;
    BaseVector *result = nullptr;
    ExecuteCurrentRowTimestamp(rowSize, result);

    int64_t afterMicros = GetCurrentEpochMicros();

    ASSERT_NE(result, nullptr) << "Result is null";
    auto *resultVector = static_cast<Vector<int64_t> *>(result);
    ASSERT_NE(resultVector, nullptr) << "Result vector is null";

    EXPECT_FALSE(resultVector->IsNull(0));
    int64_t actual = resultVector->GetValue(0);

    std::cout << "current_row_timestamp result: " << actual << " micros" << std::endl;
    std::cout << "Before execution: " << beforeMicros << " micros" << std::endl;
    std::cout << "After execution: " << afterMicros << " micros" << std::endl;
    std::cout << "Drift from before: " << (actual - beforeMicros) << " us" << std::endl;
    std::cout << "Drift from after: " << (actual - afterMicros) << " us" << std::endl;

    int64_t distBefore = std::abs(actual - beforeMicros);
    int64_t distAfter = std::abs(actual - afterMicros);
    int64_t minDist = std::min(distBefore, distAfter);

    EXPECT_LE(minDist, MAX_TIMING_SLACK_MICROS)
        << "current_row_timestamp (" << actual << ") should be within " << MAX_TIMING_SLACK_MICROS
        << " us of system epoch micros. Before=" << beforeMicros << ", After=" << afterMicros
        << ". A drift matching the local timezone offset indicates a LocalTimestamp-style bug; "
        << "a drift of ~56552 years indicates a microsecond/millisecond unit mismatch.";

    delete result;
}

// Test CurrentRowTimestampFunction struct directly (pure unit test without
TEST_F(CurrentRowTimestampTest, DirectStructTest)
{
    std::cout << "=== Test: CurrentRowTimestampFunction struct direct test ===" << std::endl;

    CurrentRowTimestampFunction<int64_t> fn;
    std::vector<DataTypeId> inputTypes;
    std::unordered_map<std::string, std::string> configValues;
    config::QueryConfig queryConfig(configValues);

    // initialize() is a no-op; nothing to verify here except it does not crash.
    fn.initialize(inputTypes, queryConfig);

    int64_t beforeMicros = GetCurrentEpochMicros();

    // First call - reads the system clock fresh
    int64_t result1 = 0;
    vectorization::Status status1 = fn.call(result1);
    EXPECT_TRUE(status1.ok()) << "call() should return OK status";

    int64_t afterMicros = GetCurrentEpochMicros();

    std::cout << "Direct call result: " << result1 << " micros since epoch" << std::endl;

    // Verify positive
    EXPECT_GT(result1, 0) << "current_row_timestamp should be > 0";

    // Verify within tight slack of system epoch micros (primary regression guard)
    int64_t distBefore = std::abs(result1 - beforeMicros);
    int64_t distAfter = std::abs(result1 - afterMicros);
    int64_t minDist = std::min(distBefore, distAfter);
    EXPECT_LE(minDist, MAX_TIMING_SLACK_MICROS)
        << "current_row_timestamp (" << result1 << ") drift from system epoch micros (before=" << beforeMicros
        << ", after=" << afterMicros << ") exceeds " << MAX_TIMING_SLACK_MICROS
        << " us; likely a timezone-offset drift (LocalTimestamp-style bug) or a unit mismatch";
}

// THE key test: proves per-record evaluation semantic.
TEST_F(CurrentRowTimestampTest, PerRecordEvaluation)
{
    std::cout << "=== Test: current_row_timestamp per-record evaluation ===" << std::endl;

    CurrentRowTimestampFunction<int64_t> fn;
    std::vector<DataTypeId> inputTypes;
    std::unordered_map<std::string, std::string> configValues;
    config::QueryConfig queryConfig(configValues);
    fn.initialize(inputTypes, queryConfig); // no-op

    // First call - reads system clock fresh
    int64_t v1 = 0;
    ASSERT_TRUE(fn.call(v1).ok());
    std::cout << "First call:  " << v1 << " micros" << std::endl;

    // Sleep to advance the wall clock well beyond microsecond granularity
    std::this_thread::sleep_for(std::chrono::milliseconds(PER_RECORD_SLEEP_MILLIS));

    // Second call - MUST read the system clock fresh again (not return cached v1)
    int64_t v2 = 0;
    ASSERT_TRUE(fn.call(v2).ok());
    std::cout << "Second call: " << v2 << " micros" << std::endl;
    std::cout << "Delta: " << (v2 - v1) << " us (sleep was " << PER_RECORD_SLEEP_MILLIS << " ms)" << std::endl;

    // v2 must be >= v1 (clock advances)
    EXPECT_GE(v2, v1) << "Second call should be >= first call (clock advances)";

    int64_t delta = v2 - v1;
    EXPECT_GE(delta, PER_RECORD_MIN_DELTA_MICROS)
        << "Two call() invocations separated by " << PER_RECORD_SLEEP_MILLIS
        << " ms must differ by at least " << PER_RECORD_MIN_DELTA_MICROS
        << " us. Delta=" << delta << " suggests call() returned a cached value "
        << "rather than reading the system clock fresh (CurrentTimestamp-style bug).";

    // Sanity upper bound: delta should not exceed sleep + slack
    constexpr int64_t UPPER_BOUND_MICROS = PER_RECORD_SLEEP_MILLIS * 1000LL + MAX_TIMING_SLACK_MICROS;
    EXPECT_LE(delta, UPPER_BOUND_MICROS)
        << "Delta " << delta << " us unexpectedly large for a " << PER_RECORD_SLEEP_MILLIS << " ms sleep";
}

TEST_F(CurrentRowTimestampTest, DiffersFromCurrentTimestampCaching)
{
    std::cout << "=== Test: current_row_timestamp differs from current_timestamp caching ===" << std::endl;

    std::vector<DataTypeId> inputTypes;
    std::unordered_map<std::string, std::string> configValues;
    config::QueryConfig queryConfig(configValues);

    CurrentTimestampFunction<int64_t> ctFn;
    CurrentRowTimestampFunction<int64_t> crtFn;
    ctFn.initialize(inputTypes, queryConfig);
    crtFn.initialize(inputTypes, queryConfig);

    // First calls - both read roughly the same instant
    int64_t ct1 = 0;
    int64_t crt1 = 0;
    ASSERT_TRUE(ctFn.call(ct1).ok());
    ASSERT_TRUE(crtFn.call(crt1).ok());

    std::cout << "current_timestamp        first call: " << ct1 << " us" << std::endl;
    std::cout << "current_row_timestamp    first call: " << crt1 << " us" << std::endl;

    // They should be close to each other (both near the same instant)
    int64_t initialDiff = std::abs(ct1 - crt1);
    EXPECT_LE(initialDiff, MAX_TIMING_SLACK_MICROS)
        << "Both functions should agree at roughly the same instant";

    // Sleep to advance the wall clock
    std::this_thread::sleep_for(std::chrono::milliseconds(PER_RECORD_SLEEP_MILLIS));

    // Second calls
    int64_t ct2 = 0;
    int64_t crt2 = 0;
    ASSERT_TRUE(ctFn.call(ct2).ok());
    ASSERT_TRUE(crtFn.call(crt2).ok());

    std::cout << "current_timestamp        second call: " << ct2 << " us (cached)" << std::endl;
    std::cout << "current_row_timestamp    second call: " << crt2 << " us (fresh)" << std::endl;
    std::cout << "current_timestamp        delta: " << (ct2 - ct1) << " us" << std::endl;
    std::cout << "current_row_timestamp    delta: " << (crt2 - crt1) << " us" << std::endl;

    // CurrentTimestampFunction: cached -> second call equals first call
    EXPECT_EQ(ct2, ct1)
        << "CurrentTimestampFunction should return the SAME cached value on both calls";

    // CurrentRowTimestampFunction: fresh read -> second call strictly greater
    EXPECT_GE(crt2, crt1) << "CurrentRowTimestampFunction second call should be >= first call";
    int64_t crtDelta = crt2 - crt1;
    EXPECT_GE(crtDelta, PER_RECORD_MIN_DELTA_MICROS)
        << "CurrentRowTimestampFunction should read fresh clock (delta >= "
        << PER_RECORD_MIN_DELTA_MICROS << " us after a " << PER_RECORD_SLEEP_MILLIS
        << " ms sleep). Delta=" << crtDelta
        << " suggests it returned a cached value like CurrentTimestampFunction.";
}

TEST_F(CurrentRowTimestampTest, RepeatedCallsAllValid)
{
    std::cout << "=== Test: current_row_timestamp repeated calls all valid ===" << std::endl;

    CurrentRowTimestampFunction<int64_t> fn;
    std::vector<DataTypeId> inputTypes;
    std::unordered_map<std::string, std::string> configValues;
    config::QueryConfig queryConfig(configValues);
    fn.initialize(inputTypes, queryConfig); // no-op

    constexpr int32_t NUM_CALLS = 5;
    int64_t first = 0;
    int64_t prev = 0;
    for (int32_t i = 0; i < NUM_CALLS; ++i) {
        int64_t beforeMicros = GetCurrentEpochMicros();

        int64_t value = 0;
        ASSERT_TRUE(fn.call(value).ok()) << "call() #" << i << " failed";

        int64_t afterMicros = GetCurrentEpochMicros();

        std::cout << "Call " << i << ": " << value << " us" << std::endl;

        // Each value must be a valid epoch micros
        EXPECT_GT(value, 0) << "Call " << i << " should be > 0";
        int64_t minDist = std::min(std::abs(value - beforeMicros), std::abs(value - afterMicros));
        EXPECT_LE(minDist, MAX_TIMING_SLACK_MICROS)
            << "Call " << i << " (" << value << ") drift exceeds timing slack";

        if (i > 0) {
            // Each fresh read should be >= the previous (clock advances)
            EXPECT_GE(value, prev) << "Call " << i << " (" << value << ") < previous (" << prev << ")";
        }
        if (i == 0) {
            first = value;
        }
        prev = value;

        // Sleep between calls to ensure the clock advances measurably
        if (i < NUM_CALLS - 1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }

    // The last value must be strictly greater than the first (we slept between calls)
    EXPECT_GT(prev, first)
        << "After " << (NUM_CALLS - 1) << " sleeps, the last fresh read (" << prev
        << ") must be strictly greater than the first (" << first << ")";
}
