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
// Real epoch milliseconds from the system clock (UTC-based, the raw time_since_epoch).
int64_t GetCurrentEpochMillis()
{
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

constexpr int64_t MAX_TIMING_SLACK_MILLIS = 5LL * 1000LL; // 5 seconds

// Sleep duration used to prove per-record evaluation (call() reads fresh clock,
// not a cached value). 50ms is well above clock granularity and scheduling jitter.
constexpr int64_t PER_RECORD_SLEEP_MILLIS = 50LL;

// Minimum acceptable delta between two call() invocations separated by
// PER_RECORD_SLEEP_MILLIS. A cached implementation would return delta == 0.
// 30ms gives slack below the 50ms sleep for OS scheduler imprecision.
constexpr int64_t PER_RECORD_MIN_DELTA_MILLIS = 30LL;

// Epoch millis for year 2000-01-01 and 2100-01-01; guards against unit bugs
// (seconds ~1e9, millis ~1.7e12, micros ~1.7e15).
constexpr int64_t kMinEpochMillis = 946684800000LL;  // 2000-01-01T00:00:00Z
constexpr int64_t kMaxEpochMillis = 4102444800000LL; // 2100-01-01T00:00:00Z
} // namespace

class CurrentRowTimestampTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        RegisterFunctions::Register();
    }

    // Create a vector function instance for current_row_timestamp().
    // CurrentRowTimestampFunction does not read any config; initialize() is a no-op.
    static std::unique_ptr<VectorFunction> CreateVectorFunction(
        const std::unordered_map<std::string, std::string> &configValues = {})
    {
        std::vector<DataTypeId> argTypes = {};
        auto signature = std::make_shared<FunctionSignature>("flink_current_row_timestamp", argTypes, OMNI_LONG);

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
    static void ApplyCurrentRowTimestamp(VectorFunction *vectorFunction, int32_t rowSize, BaseVector *&result)
    {
        ExecutionContext context;
        context.SetResultRowSize(rowSize);
        std::stack<BaseVector *> args;
        auto resultType = std::make_shared<DataType>(OMNI_LONG);
        vectorFunction->Apply(args, resultType, result, &context);
    }

    // Convenience: create a fresh function instance and execute once.
    static void ExecuteCurrentRowTimestamp(int32_t rowSize, BaseVector *&result)
    {
        auto vectorFunction = CreateVectorFunction();
        ASSERT_NE(vectorFunction, nullptr) << "Function current_row_timestamp creation failed";
        ApplyCurrentRowTimestamp(vectorFunction.get(), rowSize, result);
    }
};

TEST_F(CurrentRowTimestampTest, BasicCurrentRowTimestamp)
{
    std::cout << "=== Test: current_row_timestamp basic cases ===" << std::endl;

    int64_t beforeMillis = GetCurrentEpochMillis();

    int32_t rowSize = 5;
    BaseVector *result = nullptr;
    ExecuteCurrentRowTimestamp(rowSize, result);

    int64_t afterMillis = GetCurrentEpochMillis();

    ASSERT_NE(result, nullptr) << "Result is null";
    auto *resultVector = static_cast<Vector<int64_t> *>(result);
    ASSERT_NE(resultVector, nullptr) << "Result vector is null";

    for (int32_t i = 0; i < rowSize; ++i) {
        EXPECT_FALSE(resultVector->IsNull(i))
            << "Unexpected NULL at index " << i << " for current_row_timestamp";
        if (!resultVector->IsNull(i)) {
            int64_t actual = resultVector->GetValue(i);
            std::cout << "Row " << i << ": current_row_timestamp=" << actual << " millis since epoch" << std::endl;
            EXPECT_GT(actual, 0) << "current_row_timestamp should be > 0 at row " << i;

            int64_t distBefore = std::abs(actual - beforeMillis);
            int64_t distAfter = std::abs(actual - afterMillis);
            int64_t minDist = std::min(distBefore, distAfter);
            EXPECT_LE(minDist, MAX_TIMING_SLACK_MILLIS)
                << "current_row_timestamp (" << actual << ") drift from system epoch millis (before=" << beforeMillis
                << ", after=" << afterMillis << ") exceeds " << MAX_TIMING_SLACK_MILLIS
                << " ms; likely a timezone-offset drift (LocalTimestamp-style bug) or a unit mismatch";
        }
    }

    delete result;
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
    std::cout << "First call:  " << v1 << " millis" << std::endl;

    // Sleep to advance the wall clock well beyond millisecond granularity
    std::this_thread::sleep_for(std::chrono::milliseconds(PER_RECORD_SLEEP_MILLIS));

    // Second call - MUST read the system clock fresh again (not return cached v1)
    int64_t v2 = 0;
    ASSERT_TRUE(fn.call(v2).ok());
    std::cout << "Second call: " << v2 << " millis" << std::endl;
    std::cout << "Delta: " << (v2 - v1) << " ms (sleep was " << PER_RECORD_SLEEP_MILLIS << " ms)" << std::endl;

    // v2 must be >= v1 (clock advances)
    EXPECT_GE(v2, v1) << "Second call should be >= first call (clock advances)";

    int64_t delta = v2 - v1;
    EXPECT_GE(delta, PER_RECORD_MIN_DELTA_MILLIS)
        << "Two call() invocations separated by " << PER_RECORD_SLEEP_MILLIS
        << " ms must differ by at least " << PER_RECORD_MIN_DELTA_MILLIS
        << " ms. Delta=" << delta << " suggests call() returned a cached value "
        << "rather than reading the system clock fresh (CurrentTimestamp-style bug).";

    // Sanity upper bound: delta should not exceed sleep + slack
    constexpr int64_t UPPER_BOUND_MILLIS = PER_RECORD_SLEEP_MILLIS + MAX_TIMING_SLACK_MILLIS;
    EXPECT_LE(delta, UPPER_BOUND_MILLIS)
        << "Delta " << delta << " ms unexpectedly large for a " << PER_RECORD_SLEEP_MILLIS << " ms sleep";
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

    std::cout << "current_timestamp        first call: " << ct1 << " millis" << std::endl;
    std::cout << "current_row_timestamp    first call: " << crt1 << " millis" << std::endl;

    // They should be close to each other (both near the same instant)
    int64_t initialDiff = std::abs(ct1 - crt1);
    EXPECT_LE(initialDiff, MAX_TIMING_SLACK_MILLIS)
        << "Both functions should agree at roughly the same instant";

    // Sleep to advance the wall clock
    std::this_thread::sleep_for(std::chrono::milliseconds(PER_RECORD_SLEEP_MILLIS));

    // Second calls
    int64_t ct2 = 0;
    int64_t crt2 = 0;
    ASSERT_TRUE(ctFn.call(ct2).ok());
    ASSERT_TRUE(crtFn.call(crt2).ok());

    std::cout << "current_timestamp        second call: " << ct2 << " millis (cached)" << std::endl;
    std::cout << "current_row_timestamp    second call: " << crt2 << " millis (fresh)" << std::endl;
    std::cout << "current_timestamp        delta: " << (ct2 - ct1) << " millis" << std::endl;
    std::cout << "current_row_timestamp    delta: " << (crt2 - crt1) << " millis" << std::endl;

    // CurrentTimestampFunction: cached -> second call equals first call
    EXPECT_EQ(ct2, ct1)
        << "CurrentTimestampFunction should return the SAME cached value on both calls";

    // CurrentRowTimestampFunction: fresh read -> second call strictly greater
    EXPECT_GE(crt2, crt1) << "CurrentRowTimestampFunction second call should be >= first call";
    int64_t crtDelta = crt2 - crt1;
    EXPECT_GE(crtDelta, PER_RECORD_MIN_DELTA_MILLIS)
        << "CurrentRowTimestampFunction should read fresh clock (delta >= "
        << PER_RECORD_MIN_DELTA_MILLIS << " ms after a " << PER_RECORD_SLEEP_MILLIS
        << " ms sleep). Delta=" << crtDelta
        << " suggests it returned a cached value like CurrentTimestampFunction.";
}

// current_row_timestamp returns UTC millis regardless of session timezone (TIMESTAMP_LTZ semantics).
TEST_F(CurrentRowTimestampTest, ReturnsUtcRegardlessOfSessionTimezone)
{
    std::cout << "=== Test: current_row_timestamp returns UTC regardless of session timezone ===" << std::endl;

    const std::string tzName = "Asia/Shanghai";

    CurrentRowTimestampFunction<int64_t> fn;
    std::vector<DataTypeId> inputTypes;
    std::unordered_map<std::string, std::string> configValues;
    configValues[config::QueryConfig::kSessionTimezone] = tzName;
    config::QueryConfig queryConfig(configValues);
    fn.initialize(inputTypes, queryConfig); // no-op

    int64_t utcBefore = GetCurrentEpochMillis();
    int64_t actual = 0;
    ASSERT_TRUE(fn.call(actual).ok());
    int64_t utcAfter = GetCurrentEpochMillis();

    EXPECT_GE(actual, utcBefore - MAX_TIMING_SLACK_MILLIS)
        << "current_row_timestamp (" << actual << ") should be >= UTC millis at start (" << utcBefore
        << ") even with session timezone " << tzName;
    EXPECT_LE(actual, utcAfter + MAX_TIMING_SLACK_MILLIS)
        << "current_row_timestamp (" << actual << ") should be <= UTC millis at end (" << utcAfter
        << ") even with session timezone " << tzName;
}

// Millis precision guard: the value should be in millisecond units (~1.7e12 for current era),
// not seconds (~1.7e9) or microseconds (~1.7e15).
TEST_F(CurrentRowTimestampTest, MillisPrecisionGuard)
{
    std::cout << "=== Test: current_row_timestamp millis precision guard ===" << std::endl;

    CurrentRowTimestampFunction<int64_t> fn;
    std::vector<DataTypeId> inputTypes;
    std::unordered_map<std::string, std::string> configValues;
    config::QueryConfig queryConfig(configValues);
    fn.initialize(inputTypes, queryConfig);

    int64_t actual = 0;
    ASSERT_TRUE(fn.call(actual).ok());

    EXPECT_GE(actual, kMinEpochMillis) << "current_row_timestamp (" << actual
        << ") is below the millis-range lower bound; a value near 1.7e9 suggests "
        << "seconds, near 1.7e15 suggests micros.";
    EXPECT_LE(actual, kMaxEpochMillis) << "current_row_timestamp (" << actual
        << ") is above the millis-range upper bound; this indicates the value is "
        << "not in millisecond units.";
}

// New: empty batch boundary case (rowSize=0). Verifies no crash and an empty result vector.
TEST_F(CurrentRowTimestampTest, EmptyBatch)
{
    std::cout << "=== Test: current_row_timestamp with empty batch (rowSize=0) ===" << std::endl;

    int32_t rowSize = 0;
    BaseVector *result = nullptr;
    ExecuteCurrentRowTimestamp(rowSize, result);

    ASSERT_NE(result, nullptr) << "Result is null even for empty batch";
    EXPECT_EQ(result->GetSize(), 0) << "Empty batch should produce a 0-size result vector";

    std::cout << "Empty batch produced a result vector of size " << result->GetSize() << std::endl;

    delete result;
}

// New: multi-batch reuse. The same vector function instance is applied to two batches.
// Unlike CurrentTimestamp/LocalTimestamp (which cache a value at initialize), CurrentRowTimestamp
// reads the system clock fresh on every call(), so values across batches are NOT expected to be
// equal. This test verifies the instance can be safely reused without crashing and each batch
// returns valid UTC millis.
TEST_F(CurrentRowTimestampTest, MultiBatchReuse)
{
    std::cout << "=== Test: current_row_timestamp multi-batch reuse ===" << std::endl;

    auto vectorFunction = CreateVectorFunction();
    ASSERT_NE(vectorFunction, nullptr) << "Function current_row_timestamp creation failed";

    int64_t beforeBatch1 = GetCurrentEpochMillis();

    // First batch
    int32_t rowSize1 = 3;
    BaseVector *result1 = nullptr;
    ApplyCurrentRowTimestamp(vectorFunction.get(), rowSize1, result1);
    ASSERT_NE(result1, nullptr) << "First batch result is null";
    auto *resultVector1 = static_cast<Vector<int64_t> *>(result1);
    ASSERT_NE(resultVector1, nullptr) << "First batch result vector is null";

    for (int32_t i = 0; i < rowSize1; ++i) {
        EXPECT_FALSE(resultVector1->IsNull(i)) << "First batch row " << i << " is NULL";
        if (!resultVector1->IsNull(i)) {
            int64_t val = resultVector1->GetValue(i);
            EXPECT_GT(val, 0) << "First batch row " << i << " should be > 0";
            EXPECT_GE(val, kMinEpochMillis) << "First batch row " << i << " below millis lower bound";
            EXPECT_LE(val, kMaxEpochMillis) << "First batch row " << i << " above millis upper bound";
        }
    }

    // Second batch (reusing the same function instance)
    int32_t rowSize2 = 4;
    BaseVector *result2 = nullptr;
    ApplyCurrentRowTimestamp(vectorFunction.get(), rowSize2, result2);
    ASSERT_NE(result2, nullptr) << "Second batch result is null";
    auto *resultVector2 = static_cast<Vector<int64_t> *>(result2);
    ASSERT_NE(resultVector2, nullptr) << "Second batch result vector is null";

    int64_t afterBatch2 = GetCurrentEpochMillis();

    for (int32_t i = 0; i < rowSize2; ++i) {
        EXPECT_FALSE(resultVector2->IsNull(i)) << "Second batch row " << i << " is NULL";
        if (!resultVector2->IsNull(i)) {
            int64_t val = resultVector2->GetValue(i);
            EXPECT_GT(val, 0) << "Second batch row " << i << " should be > 0";
            EXPECT_GE(val, kMinEpochMillis) << "Second batch row " << i << " below millis lower bound";
            EXPECT_LE(val, kMaxEpochMillis) << "Second batch row " << i << " above millis upper bound";
        }
    }

    // Both batches should fall within the overall time window (fresh clock, not cached).
    int64_t batch1Val = resultVector1->GetValue(0);
    int64_t batch2Val = resultVector2->GetValue(0);
    EXPECT_GE(batch1Val, beforeBatch1 - MAX_TIMING_SLACK_MILLIS)
        << "First batch value should be near the start time";
    EXPECT_LE(batch2Val, afterBatch2 + MAX_TIMING_SLACK_MILLIS)
        << "Second batch value should be near the end time";

    std::cout << "Batch 1 first row: " << batch1Val << " millis" << std::endl;
    std::cout << "Batch 2 first row: " << batch2Val << " millis" << std::endl;
    std::cout << "Cross-batch delta: " << (batch2Val - batch1Val) << " ms (fresh clock, not cached)" << std::endl;

    delete result1;
    delete result2;
}
