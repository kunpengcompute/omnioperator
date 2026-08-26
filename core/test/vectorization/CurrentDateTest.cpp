/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: CurrentDate function unit tests
 */

#include <gtest/gtest.h>
#include <iostream>
#include <vector>
#include <ctime>
#include <chrono>
#include <cmath>
#include <memory>
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
constexpr int64_t SECS_PER_DAY = 86400LL;
// Years 1990..2100 map to [7300, 47480] days since epoch - guards against unit bugs
// (seconds ~1.7e9, millis ~1.7e12, micros ~1.7e15).
constexpr int32_t kMinDaysSinceEpoch = 7300;
constexpr int32_t kMaxDaysSinceEpoch = 47480;
constexpr int32_t kDayDriftTolerance = 1;

// Independently compute days since Unix epoch (1970-01-01) for the current LOCAL date.
// Uses std::mktime (which interprets tm as local time), independent from Date32::DaysSinceEpochFromDate.
int32_t GetCurrentLocalDaysSinceEpoch()
{
    std::time_t now = std::time(nullptr);
    std::tm localTm {};
    localtime_r(&now, &localTm);
    localTm.tm_hour = 0;
    localTm.tm_min = 0;
    localTm.tm_sec = 0;
    localTm.tm_isdst = -1;
    std::time_t midnightSecs = std::mktime(&localTm);
    return static_cast<int32_t>(midnightSecs / SECS_PER_DAY);
}

// Compute expected days since epoch for the current instant in a given session timezone.
int32_t GetSessionTzDaysSinceEpoch(const tz::TimeZone *sessionTz)
{
    auto now = std::chrono::system_clock::now();
    auto utcMicros = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
    Timestamp ts = Timestamp::fromMicros(utcMicros);
    if (sessionTz != nullptr) {
        ts.toTimezone(*sessionTz);
    }
    std::tm localTm {};
    Timestamp::epochToCalendarUtc(ts.getSeconds(), localTm);
    int32_t year = localTm.tm_year + 1900;
    int32_t month = localTm.tm_mon + 1;
    int32_t day = localTm.tm_mday;
    int64_t daysSinceEpoch = 0;
    Date32::DaysSinceEpochFromDate(year, month, day, daysSinceEpoch);
    return static_cast<int32_t>(daysSinceEpoch);
}

struct BaseVectorDeleter {
    void operator()(BaseVector *p) const
    {
        delete p;
    }
};
using VectorUniquePtr = std::unique_ptr<BaseVector, BaseVectorDeleter>;

// Verify a current_date value is positive and within tolerance of the local epoch days bracketed
// by [expectedBefore, expectedAfter] (captured around execution to handle the midnight boundary).
::testing::AssertionResult AssertDateValueValid(int32_t actual, int32_t expectedBefore, int32_t expectedAfter)
{
    if (actual <= 0) {
        return ::testing::AssertionFailure() << "current_date (" << actual << ") should be > 0";
    }
    int32_t minDist = std::min(std::abs(actual - expectedBefore), std::abs(actual - expectedAfter));
    if (minDist > kDayDriftTolerance) {
        return ::testing::AssertionFailure()
            << "current_date (" << actual << ") drift from local days since epoch (before=" << expectedBefore
            << ", after=" << expectedAfter << ") exceeds " << kDayDriftTolerance
            << " day; likely a timezone bug or a unit mismatch (seconds/millis instead of days).";
    }
    return ::testing::AssertionSuccess();
}
} // namespace

class CurrentDateTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        RegisterFunctions::Register();
    }

    static VectorUniquePtr ExecuteCurrentDate(int32_t rowSize,
        const std::unordered_map<std::string, std::string> &configValues = {})
    {
        std::vector<DataTypeId> argTypes = {};
        auto signature = std::make_shared<FunctionSignature>("flink_current_date", argTypes, OMNI_INT);

        auto factoryIt = VectorFunction::simpleFunctionFactoryMap_.find(signature);
        if (factoryIt == VectorFunction::simpleFunctionFactoryMap_.end()) {
            ADD_FAILURE() << "Function factory flink_current_date not found";
            return nullptr;
        }

        config::QueryConfig queryConfig(configValues);

        std::vector<BaseVector *> constantInputs;
        auto factory = factoryIt->second();
        auto vectorFunction = factory->createVectorFunction({}, queryConfig, constantInputs);
        if (vectorFunction == nullptr) {
            ADD_FAILURE() << "Function flink_current_date creation failed";
            return nullptr;
        }

        ExecutionContext context;
        context.SetResultRowSize(rowSize);

        std::stack<BaseVector *> args;
        auto resultType = std::make_shared<DataType>(OMNI_INT);
        BaseVector *rawResult = nullptr;
        vectorFunction->Apply(args, resultType, rawResult, &context);
        return VectorUniquePtr(rawResult);
    }
};

// Core correctness via the VectorFunction path: current_date matches the system local date within
// ±1 day (midnight tolerance). With no session timezone configured, this also verifies the
// fallback-to-OS-local-timezone behavior. Covers empty batch (rowSize=0), single-row, and small-batch.
TEST_F(CurrentDateTest, MatchesSystemLocalDate)
{
    for (int32_t rowSize : {0, 1, 5}) {
        int32_t expectedBefore = GetCurrentLocalDaysSinceEpoch();
        VectorUniquePtr result = ExecuteCurrentDate(rowSize);
        int32_t expectedAfter = GetCurrentLocalDaysSinceEpoch();

        ASSERT_NE(result, nullptr) << "Result is null for rowSize=" << rowSize;
        auto *resultVector = static_cast<Vector<int32_t> *>(result.get());
        ASSERT_NE(resultVector, nullptr) << "Result vector is null for rowSize=" << rowSize;

        for (int32_t i = 0; i < rowSize; ++i) {
            ASSERT_FALSE(resultVector->IsNull(i))
                << "Unexpected NULL at index " << i << " for rowSize=" << rowSize;
            int32_t actual = resultVector->GetValue(i);
            EXPECT_TRUE(AssertDateValueValid(actual, expectedBefore, expectedAfter))
                << " rowSize=" << rowSize << " index=" << i;
        }
    }
}

// Batch-mode semantic: all rows in the same batch share one cached value and are never NULL,
// verified across medium and large batch sizes.
TEST_F(CurrentDateTest, BatchConstantAndNonNull)
{
    for (int32_t rowSize : {50, 100, 1000}) {
        VectorUniquePtr result = ExecuteCurrentDate(rowSize);

        ASSERT_NE(result, nullptr) << "Result is null for rowSize=" << rowSize;
        auto *resultVector = static_cast<Vector<int32_t> *>(result.get());
        ASSERT_NE(resultVector, nullptr) << "Result vector is null for rowSize=" << rowSize;

        ASSERT_FALSE(resultVector->IsNull(0)) << "Row 0 is NULL for rowSize=" << rowSize;
        int32_t firstValue = resultVector->GetValue(0);

        for (int32_t i = 0; i < rowSize; ++i) {
            EXPECT_FALSE(resultVector->IsNull(i))
                << "current_date should never return NULL - row " << i << " is NULL (rowSize=" << rowSize << ")";
            EXPECT_EQ(resultVector->GetValue(i), firstValue)
                << "All rows should share the same current_date value, but row " << i << " has "
                << resultVector->GetValue(i) << " instead of " << firstValue << " (rowSize=" << rowSize << ")";
        }
    }
}

// Unit guard + caching semantics via the direct struct path (no session timezone, so this also
// covers the OS-local fallback). initialize() snapshots the date once; call() must return the
// same cached value on every invocation. The value must be in day units (not seconds ~1.7e9 or
// millis ~1.7e12) and match the local date within ±1 day.
TEST_F(CurrentDateTest, DayUnitsAndCachingSemantics)
{
    int32_t expectedBefore = GetCurrentLocalDaysSinceEpoch();

    CurrentDateFunction<int32_t> fn;
    std::vector<DataTypeId> inputTypes;
    std::unordered_map<std::string, std::string> configValues;
    config::QueryConfig queryConfig(configValues);
    fn.initialize(inputTypes, queryConfig);

    int32_t expectedAfter = GetCurrentLocalDaysSinceEpoch();

    int32_t result1 = 0;
    ASSERT_TRUE(fn.call(result1).ok()) << "call() should return OK status";

    EXPECT_GE(result1, kMinDaysSinceEpoch) << "current_date (" << result1
                                           << ") is below the day-range lower bound; a value near 1.7e9 suggests "
                                           << "seconds, near 1.7e12 suggests millis.";
    EXPECT_LE(result1, kMaxDaysSinceEpoch) << "current_date (" << result1
                                           << ") is above the day-range upper bound; this indicates the value is "
                                           << "not in day units.";
    EXPECT_TRUE(AssertDateValueValid(result1, expectedBefore, expectedAfter));

    int32_t result2 = 0;
    EXPECT_TRUE(fn.call(result2).ok()) << "Second call() should return OK status";
    EXPECT_EQ(result2, result1)
        << "call() should return the same cached value on repeated calls (caching semantic)";
}

// current_date uses the session timezone (not the OS timezone), verified across representative
// zones with negative offset (America/Los_Angeles), positive offset (Asia/Shanghai), and zero
// offset (UTC).
TEST_F(CurrentDateTest, CurrentDateUsesSessionTimezone)
{
    for (const char *tzName : {"America/Los_Angeles", "Asia/Shanghai", "UTC"}) {
        const auto *sessionTz = tz::locateZone(tzName);

        int32_t expectedBefore = GetSessionTzDaysSinceEpoch(sessionTz);

        CurrentDateFunction<int32_t> fn;
        std::vector<DataTypeId> inputTypes;
        std::unordered_map<std::string, std::string> configValues;
        configValues[config::QueryConfig::kSessionTimezone] = tzName;
        config::QueryConfig queryConfig(configValues);
        fn.initialize(inputTypes, queryConfig);

        int32_t expectedAfter = GetSessionTzDaysSinceEpoch(sessionTz);

        int32_t actual = 0;
        ASSERT_TRUE(fn.call(actual).ok());
        EXPECT_TRUE(AssertDateValueValid(actual, expectedBefore, expectedAfter))
            << "current_date should match session timezone " << tzName;
    }
}

// current_date via the full VectorFunction path with session timezone config.
TEST_F(CurrentDateTest, VectorFunctionPathWithSessionTimezone)
{
    const std::string tzName = "America/Los_Angeles";
    const auto *sessionTz = tz::locateZone(tzName);

    std::unordered_map<std::string, std::string> configValues;
    configValues[config::QueryConfig::kSessionTimezone] = tzName;

    int32_t expectedBefore = GetSessionTzDaysSinceEpoch(sessionTz);
    VectorUniquePtr result = ExecuteCurrentDate(1, configValues);
    int32_t expectedAfter = GetSessionTzDaysSinceEpoch(sessionTz);

    ASSERT_NE(result, nullptr);
    auto *resultVector = static_cast<Vector<int32_t> *>(result.get());
    ASSERT_NE(resultVector, nullptr);
    ASSERT_FALSE(resultVector->IsNull(0));
    EXPECT_TRUE(AssertDateValueValid(resultVector->GetValue(0), expectedBefore, expectedAfter));
}
