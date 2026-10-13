/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Hour function unit tests
 * Note: Hour uses Timestamp::epochToCalendarUtc (UTC). All test timestamps are built in UTC
 *       via TimestampToMicrosUtc / Timestamp::calendarUtcToEpoch so expected hour matches.
 */

#include <gtest/gtest.h>
#include <vector>
#include <ctime>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/functions/Hour.h"
#include "vectorization/VectorFunction.h"
#include "codegen/func_signature.h"
#include "vector/vector_helper.h"
#include "vector/vector.h"
#include "type/Timestamp.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::codegen;
using namespace omniruntime::TestUtil;

// Initialize function registration before running tests
class HourTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const hour_test_env = ::testing::AddGlobalTestEnvironment(new HourTestEnvironment);

class HourFunctionTestHelper {
public:
    static void ValidateResult(BaseVector* result, const std::vector<int32_t>& expected, int rowSize) {
        auto* resultVec = dynamic_cast<Vector<int32_t>*>(result);
        ASSERT_NE(resultVec, nullptr) << "Result vector type mismatch";
        
        for (int i = 0; i < rowSize; ++i) {
            int32_t actualValue = resultVec->GetValue(i);
            int32_t expectedValue = expected[i];
            EXPECT_EQ(actualValue, expectedValue) << "Row " << i << " value mismatch";
        }
    }
    
    static BaseVector* CreateTimestampVector(const std::vector<int64_t>& values) {
        BaseVector* vec = VectorHelper::CreateFlatVector(nullptr, OMNI_TIMESTAMP, values.size());
        auto* typedVec = static_cast<Vector<int64_t>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typedVec->SetValue(i, values[i]);
        }
        return vec;
    }
    
    static void ExecuteHour(BaseVector* inputVec, DataTypeId inputTypeId, BaseVector*& result) {
        auto signature = std::make_shared<FunctionSignature>("hour", 
            std::vector<DataTypeId>{inputTypeId}, OMNI_INT);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "Hour function not found for signature";
        
        auto outputType = std::make_shared<DataType>(OMNI_INT);
        ExecutionContext context(4096, nullptr);
        context.SetResultRowSize(inputVec->GetSize());
        std::stack<BaseVector*> args;
        args.push(inputVec);
        
        ASSERT_NO_THROW(function->Apply(args, outputType, result, &context))
            << "Hour function threw an exception";
    }
    
    // Build UTC timestamp (microseconds since epoch) from calendar components.
    // Hour() uses Timestamp::epochToCalendarUtc and returns UTC hour; tests must use UTC.
    static int64_t TimestampToMicrosUtc(int year, int month, int day, int hour, int minute, int second) {
        std::tm tm = {};
        tm.tm_year = year - 1900;
        tm.tm_mon = month - 1;
        tm.tm_mday = day;
        tm.tm_hour = hour;
        tm.tm_min = minute;
        tm.tm_sec = second;
        tm.tm_isdst = 0;
        int64_t seconds = Timestamp::calendarUtcToEpoch(tm);
        return seconds * 1000000;
    }
};

// Test: Hour from timestamp - basic cases
TEST(HourTest, TimestampBasic) {
    // Create timestamps with different hours: 12:30:45, 15:45:20, 00:00:00
    std::vector<int64_t> timestampValues = {
        HourFunctionTestHelper::TimestampToMicrosUtc(2024, 1, 1, 12, 30, 45),
        HourFunctionTestHelper::TimestampToMicrosUtc(2024, 1, 1, 15, 45, 20),
        HourFunctionTestHelper::TimestampToMicrosUtc(2024, 1, 1, 0, 0, 0)
    };
    std::vector<int32_t> expected = {12, 15, 0};
    
    BaseVector* inputVec = HourFunctionTestHelper::CreateTimestampVector(timestampValues);
    BaseVector* resultVec = nullptr;
    HourFunctionTestHelper::ExecuteHour(inputVec, OMNI_TIMESTAMP, resultVec);
    HourFunctionTestHelper::ValidateResult(resultVec, expected, timestampValues.size());
    
    delete resultVec;
}

// Test: Hour from timestamp - edge cases (23 hours)
TEST(HourTest, TimestampEdgeCases) {
    // Create timestamps with edge hour values
    std::vector<int64_t> timestampValues = {
        HourFunctionTestHelper::TimestampToMicrosUtc(2024, 1, 1, 23, 59, 59),
        HourFunctionTestHelper::TimestampToMicrosUtc(2024, 1, 1, 0, 0, 0),
        HourFunctionTestHelper::TimestampToMicrosUtc(2024, 1, 1, 1, 0, 0)
    };
    std::vector<int32_t> expected = {23, 0, 1};
    
    BaseVector* inputVec = HourFunctionTestHelper::CreateTimestampVector(timestampValues);
    BaseVector* resultVec = nullptr;
    HourFunctionTestHelper::ExecuteHour(inputVec, OMNI_TIMESTAMP, resultVec);
    HourFunctionTestHelper::ValidateResult(resultVec, expected, timestampValues.size());
    
    delete resultVec;
}

// Test: Hour from timestamp with NULL values
TEST(HourTest, TimestampWithNull) {
    std::vector<int64_t> timestampValues = {
        HourFunctionTestHelper::TimestampToMicrosUtc(2024, 1, 1, 12, 30, 45),
        HourFunctionTestHelper::TimestampToMicrosUtc(2024, 1, 1, 15, 45, 20),
        HourFunctionTestHelper::TimestampToMicrosUtc(2024, 1, 1, 0, 0, 0)
    };
    
    BaseVector* inputVec = HourFunctionTestHelper::CreateTimestampVector(timestampValues);
    // Set middle value to NULL
    inputVec->SetNull(1);
    
    BaseVector* resultVec = nullptr;
    HourFunctionTestHelper::ExecuteHour(inputVec, OMNI_TIMESTAMP, resultVec);
    
    // First and third should have values, second should be NULL
    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";
    
    // Validate non-null values
    auto* resultVecTyped = dynamic_cast<Vector<int32_t>*>(resultVec);
    EXPECT_EQ(resultVecTyped->GetValue(0), 12) << "Row 0 hour should be 12";
    EXPECT_EQ(resultVecTyped->GetValue(2), 0) << "Row 2 hour should be 0";
    
    delete resultVec;
}

// Test: Hour from timestamp - all hours (0-23)
TEST(HourTest, TimestampAllHours) {
    std::vector<int64_t> timestampValues;
    std::vector<int32_t> expected;
    
    for (int h = 0; h < 24; ++h) {
        timestampValues.push_back(HourFunctionTestHelper::TimestampToMicrosUtc(2024, 1, 1, h, 30, 0));
        expected.push_back(h);
    }
    
    BaseVector* inputVec = HourFunctionTestHelper::CreateTimestampVector(timestampValues);
    BaseVector* resultVec = nullptr;
    HourFunctionTestHelper::ExecuteHour(inputVec, OMNI_TIMESTAMP, resultVec);
    HourFunctionTestHelper::ValidateResult(resultVec, expected, timestampValues.size());
    
    delete resultVec;
}

// Test: Hour from timestamp - different dates same hour
TEST(HourTest, TimestampDifferentDatesSameHour) {
    // Create timestamps on different dates but same hour
    std::vector<int64_t> timestampValues = {
        HourFunctionTestHelper::TimestampToMicrosUtc(2024, 1, 1, 10, 0, 0),
        HourFunctionTestHelper::TimestampToMicrosUtc(2024, 6, 15, 10, 30, 0),
        HourFunctionTestHelper::TimestampToMicrosUtc(2024, 12, 31, 10, 59, 59)
    };
    std::vector<int32_t> expected = {10, 10, 10};
    
    BaseVector* inputVec = HourFunctionTestHelper::CreateTimestampVector(timestampValues);
    BaseVector* resultVec = nullptr;
    HourFunctionTestHelper::ExecuteHour(inputVec, OMNI_TIMESTAMP, resultVec);
    HourFunctionTestHelper::ValidateResult(resultVec, expected, timestampValues.size());
    
    delete resultVec;
}

// Test: Hour from timestamp - midnight boundary
TEST(HourTest, TimestampMidnightBoundary) {
    // Test timestamps around midnight
    std::vector<int64_t> timestampValues = {
        HourFunctionTestHelper::TimestampToMicrosUtc(2024, 1, 1, 23, 59, 59),  // Just before midnight
        HourFunctionTestHelper::TimestampToMicrosUtc(2024, 1, 2, 0, 0, 0),     // Midnight
        HourFunctionTestHelper::TimestampToMicrosUtc(2024, 1, 2, 0, 0, 1)      // Just after midnight
    };
    std::vector<int32_t> expected = {23, 0, 0};
    
    BaseVector* inputVec = HourFunctionTestHelper::CreateTimestampVector(timestampValues);
    BaseVector* resultVec = nullptr;
    HourFunctionTestHelper::ExecuteHour(inputVec, OMNI_TIMESTAMP, resultVec);
    HourFunctionTestHelper::ValidateResult(resultVec, expected, timestampValues.size());
    
    delete resultVec;
}

// Test: Hour from timestamp - noon boundary
TEST(HourTest, TimestampNoonBoundary) {
    // Test timestamps around noon
    std::vector<int64_t> timestampValues = {
        HourFunctionTestHelper::TimestampToMicrosUtc(2024, 1, 1, 11, 59, 59),  // Just before noon
        HourFunctionTestHelper::TimestampToMicrosUtc(2024, 1, 1, 12, 0, 0),    // Noon
        HourFunctionTestHelper::TimestampToMicrosUtc(2024, 1, 1, 12, 0, 1)     // Just after noon
    };
    std::vector<int32_t> expected = {11, 12, 12};
    
    BaseVector* inputVec = HourFunctionTestHelper::CreateTimestampVector(timestampValues);
    BaseVector* resultVec = nullptr;
    HourFunctionTestHelper::ExecuteHour(inputVec, OMNI_TIMESTAMP, resultVec);
    HourFunctionTestHelper::ValidateResult(resultVec, expected, timestampValues.size());
    
    delete resultVec;
}

// Test: Hour from timestamp - historical dates (before 1970)
TEST(HourTest, TimestampHistoricalDates) {
    // Create timestamps with historical dates (after 1970 for simplicity)
    std::vector<int64_t> timestampValues = {
        HourFunctionTestHelper::TimestampToMicrosUtc(1980, 1, 1, 13, 23, 0),
        HourFunctionTestHelper::TimestampToMicrosUtc(1990, 6, 15, 5, 45, 20),
        HourFunctionTestHelper::TimestampToMicrosUtc(2000, 12, 31, 23, 0, 0)
    };
    std::vector<int32_t> expected = {13, 5, 23};
    
    BaseVector* inputVec = HourFunctionTestHelper::CreateTimestampVector(timestampValues);
    BaseVector* resultVec = nullptr;
    HourFunctionTestHelper::ExecuteHour(inputVec, OMNI_TIMESTAMP, resultVec);
    HourFunctionTestHelper::ValidateResult(resultVec, expected, timestampValues.size());
    
    delete resultVec;
}

// Test: Hour from timestamp - all NULLs
TEST(HourTest, TimestampAllNulls) {
    std::vector<int64_t> timestampValues = {0, 0, 0};
    
    BaseVector* inputVec = HourFunctionTestHelper::CreateTimestampVector(timestampValues);
    // Set all values to NULL
    inputVec->SetNull(0);
    inputVec->SetNull(1);
    inputVec->SetNull(2);
    
    BaseVector* resultVec = nullptr;
    HourFunctionTestHelper::ExecuteHour(inputVec, OMNI_TIMESTAMP, resultVec);
    
    // All should be NULL
    EXPECT_TRUE(resultVec->IsNull(0)) << "Row 0 should be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";
    EXPECT_TRUE(resultVec->IsNull(2)) << "Row 2 should be NULL";
    
    delete resultVec;
}
