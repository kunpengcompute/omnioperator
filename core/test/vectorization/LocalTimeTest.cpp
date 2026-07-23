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
#include "util/config/QueryConfig.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::codegen;

namespace {
constexpr int64_t MICROS_PER_DAY = 86400000000LL;
constexpr int64_t MICROS_PER_SECOND = 1000000LL;

// Get current UTC time as microseconds since midnight (matching LocalTimeFunction's UTC convention)
int64_t GetCurrentMicrosSinceMidnight()
{
    auto now = std::chrono::system_clock::now();
    auto durationSinceEpoch = now.time_since_epoch();
    auto secsSinceEpoch = std::chrono::duration_cast<std::chrono::seconds>(durationSinceEpoch);
    auto microsSinceEpoch = std::chrono::duration_cast<std::chrono::microseconds>(durationSinceEpoch);
    int64_t subSecondMicros = static_cast<int64_t>((microsSinceEpoch - secsSinceEpoch).count());
    std::time_t timeNow = static_cast<std::time_t>(secsSinceEpoch.count());
    std::tm tmVal {};
    gmtime_r(&timeNow, &tmVal);
    return static_cast<int64_t>(tmVal.tm_hour * 3600 + tmVal.tm_min * 60 + tmVal.tm_sec) * MICROS_PER_SECOND
        + subSecondMicros;
}

// Compute circular distance (in micros) between two times-of-day, handling midnight wrap-around
int64_t CircularDistanceMicros(int64_t a, int64_t b)
{
    int64_t diff = std::abs(a - b);
    return std::min(diff, MICROS_PER_DAY - diff);
}
} // namespace

class LocalTimeTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        RegisterFunctions::Register();
    }

    // Execute localtime() with given row size and return result vector.
    static void ExecuteLocalTime(int32_t rowSize, BaseVector *&result)
    {
        // Create function signature: localtime() -> OMNI_LONG
        std::vector<DataTypeId> argTypes = {};
        auto signature = std::make_shared<FunctionSignature>("localtime", argTypes, OMNI_LONG);

        // Find simple function factory
        auto factoryIt = VectorFunction::simpleFunctionFactoryMap_.find(signature);
        ASSERT_NE(factoryIt, VectorFunction::simpleFunctionFactoryMap_.end())
            << "Function factory localtime not found";

        // Create QueryConfig (empty - LocalTimeFunction does not read any config)
        std::unordered_map<std::string, std::string> configValues;
        config::QueryConfig queryConfig(configValues);

        // Create vector function with config (this calls initialize() which caches current time)
        std::vector<BaseVector *> constantInputs;
        auto factory = factoryIt->second();
        auto vectorFunction = factory->createVectorFunction({}, queryConfig, constantInputs);
        ASSERT_NE(vectorFunction, nullptr) << "Function localtime creation failed";

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

// Test basic localtime: returns non-NULL value in valid range [0, 86400000000)
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
            std::cout << "Row " << i << ": localtime=" << actual << " micros since midnight" << std::endl;
            EXPECT_GE(actual, 0) << "localtime should be >= 0 at row " << i;
            EXPECT_LT(actual, MICROS_PER_DAY) << "localtime should be < " << MICROS_PER_DAY << " at row " << i;
        }
    }

    delete result;
}

// Test localtime with a single row
TEST_F(LocalTimeTest, SingleRow)
{
    std::cout << "=== Test: localtime single row ===" << std::endl;

    int32_t rowSize = 1;
    BaseVector *result = nullptr;
    ExecuteLocalTime(rowSize, result);

    ASSERT_NE(result, nullptr) << "Result is null";
    auto *resultVector = static_cast<Vector<int64_t> *>(result);
    ASSERT_NE(resultVector, nullptr) << "Result vector is null";

    EXPECT_FALSE(resultVector->IsNull(0));
    int64_t actual = resultVector->GetValue(0);
    std::cout << "Single row: localtime=" << actual << " micros since midnight" << std::endl;
    EXPECT_GE(actual, 0);
    EXPECT_LT(actual, MICROS_PER_DAY);

    delete result;
}

// Test localtime returns constant value across all rows in the same batch (batch-mode semantic).
TEST_F(LocalTimeTest, ConstantResultAcrossRows)
{
    std::cout << "=== Test: localtime constant result across all rows ===" << std::endl;

    int32_t rowSize = 100;
    BaseVector *result = nullptr;
    ExecuteLocalTime(rowSize, result);

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
                << "All rows should have the same localtime value, but row " << i << " has " << actual
                << " instead of " << firstValue;
        }
    }

    std::cout << "All " << rowSize << " rows have value: " << firstValue << " micros since midnight" << std::endl;

    delete result;
}

// Test localtime with a large batch
TEST_F(LocalTimeTest, LargeBatch)
{
    std::cout << "=== Test: localtime with large batch ===" << std::endl;

    int32_t rowSize = 1000;
    BaseVector *result = nullptr;
    ExecuteLocalTime(rowSize, result);

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

    std::cout << "Large batch (" << rowSize << " rows): all rows = " << firstValue << " micros since midnight"
              << std::endl;

    delete result;
}

// Test localtime matches the system UTC time (within tolerance).
TEST_F(LocalTimeTest, MatchesSystemLocalTime)
{
    std::cout << "=== Test: localtime matches system local time ===" << std::endl;

    // Capture system time before function creation (initialize() runs during createVectorFunction)
    int64_t beforeMicros = GetCurrentMicrosSinceMidnight();

    int32_t rowSize = 1;
    BaseVector *result = nullptr;
    ExecuteLocalTime(rowSize, result);

    // Capture system time after function creation
    int64_t afterMicros = GetCurrentMicrosSinceMidnight();

    ASSERT_NE(result, nullptr) << "Result is null";
    auto *resultVector = static_cast<Vector<int64_t> *>(result);
    ASSERT_NE(resultVector, nullptr) << "Result vector is null";

    EXPECT_FALSE(resultVector->IsNull(0));
    int64_t actual = resultVector->GetValue(0);

    std::cout << "localtime result: " << actual << " micros" << std::endl;
    std::cout << "Before initialize: " << beforeMicros << " micros" << std::endl;
    std::cout << "After initialize: " << afterMicros << " micros" << std::endl;

    // The result should be close to the system time captured around initialize().
    constexpr int64_t toleranceMicros = 2000000LL;
    int64_t distBefore = CircularDistanceMicros(actual, beforeMicros);
    int64_t distAfter = CircularDistanceMicros(actual, afterMicros);
    int64_t minDist = std::min(distBefore, distAfter);

    EXPECT_LE(minDist, toleranceMicros)
        << "localtime (" << actual << ") should be within " << toleranceMicros
        << " micros of system time. Before=" << beforeMicros << ", After=" << afterMicros;

    delete result;
}

// Test localtime always returns non-NULL values (current time is always available)
TEST_F(LocalTimeTest, NonNullableResult)
{
    std::cout << "=== Test: localtime non-nullable result ===" << std::endl;

    int32_t rowSize = 50;
    BaseVector *result = nullptr;
    ExecuteLocalTime(rowSize, result);

    ASSERT_NE(result, nullptr) << "Result is null";
    auto *resultVector = static_cast<Vector<int64_t> *>(result);
    ASSERT_NE(resultVector, nullptr) << "Result vector is null";

    for (int32_t i = 0; i < rowSize; ++i) {
        EXPECT_FALSE(resultVector->IsNull(i))
            << "localtime should never return NULL - row " << i << " is NULL";
    }

    delete result;
}

// Test LocalTimeFunction struct directly (pure unit test without VectorFunction infrastructure).
TEST_F(LocalTimeTest, DirectStructTest)
{
    std::cout << "=== Test: LocalTimeFunction struct direct test ===" << std::endl;

    int64_t beforeMicros = GetCurrentMicrosSinceMidnight();

    LocalTimeFunction<int64_t> fn;
    std::vector<DataTypeId> inputTypes;
    std::unordered_map<std::string, std::string> configValues;
    config::QueryConfig queryConfig(configValues);
    fn.initialize(inputTypes, queryConfig);

    int64_t afterMicros = GetCurrentMicrosSinceMidnight();

    // First call - get the cached value
    int64_t result1 = 0;
    vectorization::Status status1 = fn.call(result1);
    EXPECT_TRUE(status1.ok()) << "call() should return OK status";

    std::cout << "Direct call result: " << result1 << " micros since midnight" << std::endl;

    // Verify valid range
    EXPECT_GE(result1, 0) << "localtime should be >= 0";
    EXPECT_LT(result1, MICROS_PER_DAY) << "localtime should be < " << MICROS_PER_DAY;

    // Verify close to system time
    constexpr int64_t toleranceMicros = 2000000LL;
    int64_t distBefore = CircularDistanceMicros(result1, beforeMicros);
    int64_t distAfter = CircularDistanceMicros(result1, afterMicros);
    int64_t minDist = std::min(distBefore, distAfter);
    EXPECT_LE(minDist, toleranceMicros)
        << "localtime (" << result1 << ") should be within " << toleranceMicros
        << " micros of system time. Before=" << beforeMicros << ", After=" << afterMicros;

    // Second call - should return the same cached value (batch-mode semantic)
    int64_t result2 = 0;
    vectorization::Status status2 = fn.call(result2);
    EXPECT_TRUE(status2.ok()) << "Second call() should return OK status";
    EXPECT_EQ(result2, result1) << "call() should return the same cached value on repeated calls";
}
