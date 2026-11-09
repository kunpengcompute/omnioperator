/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: UnixSeconds function unit tests
 */

#include <gtest/gtest.h>
#include <iostream>
#include <vector>
#include <ctime>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/functions/UnixSeconds.h"
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

class UnixSecondsTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const unix_seconds_test_env =
    ::testing::AddGlobalTestEnvironment(new UnixSecondsTestEnvironment);

class UnixSecondsTestHelper {
public:
    static void ValidateResult(BaseVector* result, const std::vector<int64_t>& expected, int rowSize) {
        auto* resultVec = dynamic_cast<Vector<int64_t>*>(result);
        ASSERT_NE(resultVec, nullptr) << "Result vector type mismatch";

        for (int i = 0; i < rowSize; ++i) {
            if (result->IsNull(i)) {
                std::cout << "Row " << i << ": NULL" << std::endl;
                continue;
            }
            int64_t actualValue = resultVec->GetValue(i);
            int64_t expectedValue = expected[i];
            std::cout << "Row " << i << ": Expected=" << expectedValue
                      << ", Actual=" << actualValue << std::endl;
            EXPECT_EQ(actualValue, expectedValue) << "Row " << i << " value mismatch";
        }
    }

    static BaseVector* CreateTimestampVector(const std::vector<int64_t>& microValues) {
        BaseVector* vec = VectorHelper::CreateFlatVector(OMNI_TIMESTAMP, microValues.size());
        auto* typedVec = static_cast<Vector<int64_t>*>(vec);
        for (size_t i = 0; i < microValues.size(); ++i) {
            typedVec->SetValue(i, microValues[i]);
        }
        return vec;
    }

    static BaseVector* CreateLongVector(const std::vector<int64_t>& values) {
        BaseVector* vec = VectorHelper::CreateFlatVector(OMNI_LONG, values.size());
        auto* typedVec = static_cast<Vector<int64_t>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typedVec->SetValue(i, values[i]);
        }
        return vec;
    }

    static void ExecuteUnixSeconds(BaseVector* inputVec, DataTypeId inputTypeId, BaseVector*& result) {
        auto signature = std::make_shared<FunctionSignature>("unix_seconds",
            std::vector<DataTypeId>{inputTypeId}, OMNI_LONG);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "unix_seconds function not found for signature";

        auto outputType = std::make_shared<DataType>(OMNI_LONG);
        ExecutionContext context;
        context.SetResultRowSize(inputVec->GetSize());
        std::stack<BaseVector*> args;
        inputVec->SetIsField(true);  // caller-owned, do not let Apply delete
        args.push(inputVec);

        ASSERT_NO_THROW(function->Apply(args, outputType, result, &context))
            << "unix_seconds function threw an exception";
    }

    static int64_t TimestampToMicros(int year, int month, int day,
                                     int hour, int minute, int second,
                                     int extraMicros = 0) {
        std::tm tm = {};
        tm.tm_year = year - 1900;
        tm.tm_mon = month - 1;
        tm.tm_mday = day;
        tm.tm_hour = hour;
        tm.tm_min = minute;
        tm.tm_sec = second;
        tm.tm_isdst = -1;

        std::time_t time = std::mktime(&tm);
        if (time == -1) {
            return 0;
        }
        return static_cast<int64_t>(time) * 1000000 + extraMicros;
    }
};

// Test: Basic timestamp to seconds conversion
TEST(UnixSecondsTest, TimestampBasic) {
    std::cout << "=== Test: unix_seconds from TIMESTAMP - basic cases ===" << std::endl;

    // 1970-01-01 00:00:01 -> 1 second
    // 2024-01-29 11:45:30 -> expected seconds since epoch
    // 1970-01-01 00:00:00 -> 0
    std::vector<int64_t> timestampMicros = {
        1 * 1000000LL,           // 1 second after epoch
        0LL,                     // exactly epoch
        2147 * 1000000LL         // 2147 seconds after epoch
    };
    std::vector<int64_t> expected = {1, 0, 2147};

    BaseVector* inputVec = UnixSecondsTestHelper::CreateTimestampVector(timestampMicros);
    BaseVector* resultVec = nullptr;
    UnixSecondsTestHelper::ExecuteUnixSeconds(inputVec, OMNI_TIMESTAMP, resultVec);
    UnixSecondsTestHelper::ValidateResult(resultVec, expected, timestampMicros.size());

    delete inputVec;
    delete resultVec;
}

// Test: Sub-second precision is truncated (matches Velox behavior)
TEST(UnixSecondsTest, SubSecondTruncation) {
    std::cout << "=== Test: unix_seconds sub-second truncation ===" << std::endl;

    // 0.000127 seconds = 127 microseconds -> should return 0
    // 0.999999 seconds = 999999 microseconds -> should return 0
    // 1.500000 seconds = 1500000 microseconds -> should return 1
    std::vector<int64_t> timestampMicros = {
        127LL,                   // 0.000127 seconds
        999999LL,                // 0.999999 seconds
        1500000LL                // 1.5 seconds
    };
    std::vector<int64_t> expected = {0, 0, 1};

    BaseVector* inputVec = UnixSecondsTestHelper::CreateTimestampVector(timestampMicros);
    BaseVector* resultVec = nullptr;
    UnixSecondsTestHelper::ExecuteUnixSeconds(inputVec, OMNI_TIMESTAMP, resultVec);
    UnixSecondsTestHelper::ValidateResult(resultVec, expected, timestampMicros.size());

    delete inputVec;
    delete resultVec;
}

// Test: Negative timestamps (before 1970-01-01)
TEST(UnixSecondsTest, NegativeTimestamps) {
    std::cout << "=== Test: unix_seconds with negative timestamps ===" << std::endl;

    // -1 second = -1000000 microseconds -> -1
    // -0.000128 seconds = -128 microseconds -> -1 (floor behavior)
    // -1.5 seconds = -1500000 microseconds -> -2 (floor behavior)
    std::vector<int64_t> timestampMicros = {
        -1000000LL,              // exactly -1 second
        -128LL,                  // -0.000128 seconds -> floor to -1
        -1500000LL               // -1.5 seconds -> floor to -2
    };
    std::vector<int64_t> expected = {-1, -1, -2};

    BaseVector* inputVec = UnixSecondsTestHelper::CreateTimestampVector(timestampMicros);
    BaseVector* resultVec = nullptr;
    UnixSecondsTestHelper::ExecuteUnixSeconds(inputVec, OMNI_TIMESTAMP, resultVec);
    UnixSecondsTestHelper::ValidateResult(resultVec, expected, timestampMicros.size());

    delete inputVec;
    delete resultVec;
}

// Test: NULL handling
TEST(UnixSecondsTest, TimestampWithNull) {
    std::cout << "=== Test: unix_seconds with NULL values ===" << std::endl;

    std::vector<int64_t> timestampMicros = {
        1000000LL,               // 1 second
        0LL,                     // placeholder for NULL
        31536001000000LL         // ~1 year + 1 second
    };

    BaseVector* inputVec = UnixSecondsTestHelper::CreateTimestampVector(timestampMicros);
    inputVec->SetNull(1);

    BaseVector* resultVec = nullptr;
    UnixSecondsTestHelper::ExecuteUnixSeconds(inputVec, OMNI_TIMESTAMP, resultVec);

    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";

    auto* resultVecTyped = dynamic_cast<Vector<int64_t>*>(resultVec);
    EXPECT_EQ(resultVecTyped->GetValue(0), 1) << "Row 0 should be 1";
    EXPECT_EQ(resultVecTyped->GetValue(2), 31536001) << "Row 2 should be 31536001";

    delete inputVec;
    delete resultVec;
}

// Test: OMNI_LONG input type (equivalent to OMNI_TIMESTAMP)
TEST(UnixSecondsTest, LongInputType) {
    std::cout << "=== Test: unix_seconds with OMNI_LONG input ===" << std::endl;

    std::vector<int64_t> values = {
        1000000LL,               // 1 second
        0LL,                     // epoch
        -1000000LL               // -1 second
    };
    std::vector<int64_t> expected = {1, 0, -1};

    BaseVector* inputVec = UnixSecondsTestHelper::CreateLongVector(values);
    BaseVector* resultVec = nullptr;
    UnixSecondsTestHelper::ExecuteUnixSeconds(inputVec, OMNI_LONG, resultVec);
    UnixSecondsTestHelper::ValidateResult(resultVec, expected, values.size());

    delete inputVec;
    delete resultVec;
}

// Test: Large timestamp values
TEST(UnixSecondsTest, LargeTimestampValues) {
    std::cout << "=== Test: unix_seconds with large timestamp values ===" << std::endl;

    // Year 2100 approximately: 4102444800 seconds = 4102444800000000 micros
    // Year 1900 approximately: -2208988800 seconds = -2208988800000000 micros
    std::vector<int64_t> timestampMicros = {
        4102444800000000LL,      // ~2100-01-01
        -2208988800000000LL      // ~1900-01-01
    };
    std::vector<int64_t> expected = {4102444800LL, -2208988800LL};

    BaseVector* inputVec = UnixSecondsTestHelper::CreateTimestampVector(timestampMicros);
    BaseVector* resultVec = nullptr;
    UnixSecondsTestHelper::ExecuteUnixSeconds(inputVec, OMNI_TIMESTAMP, resultVec);
    UnixSecondsTestHelper::ValidateResult(resultVec, expected, timestampMicros.size());

    delete inputVec;
    delete resultVec;
}

// Test: Epoch boundary values (Velox test cases)
TEST(UnixSecondsTest, EpochBoundary) {
    std::cout << "=== Test: unix_seconds epoch boundary (Velox compatible) ===" << std::endl;

    // Matching Velox test cases:
    // "1970-01-01 00:00:01" -> 1 (1000000 micros)
    // "1970-01-01 00:00:00.000127" -> 0 (127 micros)
    // "1969-12-31 23:59:59.999872" -> -1 (-128 micros, floor to -1)
    // "1970-01-01 00:35:47.483647" -> 2147 (2147483647 micros)
    // "1971-01-01 00:00:01.483647" -> 31536001 (31536001483647 micros)
    std::vector<int64_t> timestampMicros = {
        1000000LL,               // 1970-01-01 00:00:01
        127LL,                   // 1970-01-01 00:00:00.000127
        -128LL,                  // 1969-12-31 23:59:59.999872
        2147483647LL,            // 1970-01-01 00:35:47.483647
        31536001483647LL         // 1971-01-01 00:00:01.483647
    };
    std::vector<int64_t> expected = {1, 0, -1, 2147, 31536001};

    BaseVector* inputVec = UnixSecondsTestHelper::CreateTimestampVector(timestampMicros);
    BaseVector* resultVec = nullptr;
    UnixSecondsTestHelper::ExecuteUnixSeconds(inputVec, OMNI_TIMESTAMP, resultVec);
    UnixSecondsTestHelper::ValidateResult(resultVec, expected, timestampMicros.size());

    delete inputVec;
    delete resultVec;
}

// Test: All NULL input
TEST(UnixSecondsTest, AllNullInput) {
    std::cout << "=== Test: unix_seconds with all NULL input ===" << std::endl;

    std::vector<int64_t> timestampMicros = {0LL, 0LL, 0LL};

    BaseVector* inputVec = UnixSecondsTestHelper::CreateTimestampVector(timestampMicros);
    inputVec->SetNull(0);
    inputVec->SetNull(1);
    inputVec->SetNull(2);

    BaseVector* resultVec = nullptr;
    UnixSecondsTestHelper::ExecuteUnixSeconds(inputVec, OMNI_TIMESTAMP, resultVec);

    EXPECT_TRUE(resultVec->IsNull(0)) << "Row 0 should be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";
    EXPECT_TRUE(resultVec->IsNull(2)) << "Row 2 should be NULL";

    delete inputVec;
    delete resultVec;
}

// Test: Zero microseconds exactly at epoch
TEST(UnixSecondsTest, ExactEpoch) {
    std::cout << "=== Test: unix_seconds at exact epoch ===" << std::endl;

    std::vector<int64_t> timestampMicros = {0LL};
    std::vector<int64_t> expected = {0};

    BaseVector* inputVec = UnixSecondsTestHelper::CreateTimestampVector(timestampMicros);
    BaseVector* resultVec = nullptr;
    UnixSecondsTestHelper::ExecuteUnixSeconds(inputVec, OMNI_TIMESTAMP, resultVec);
    UnixSecondsTestHelper::ValidateResult(resultVec, expected, timestampMicros.size());

    delete inputVec;
    delete resultVec;
}

// Test: Single row
TEST(UnixSecondsTest, SingleRow) {
    std::cout << "=== Test: unix_seconds single row ===" << std::endl;

    std::vector<int64_t> timestampMicros = {86400000000LL}; // exactly 1 day
    std::vector<int64_t> expected = {86400};

    BaseVector* inputVec = UnixSecondsTestHelper::CreateTimestampVector(timestampMicros);
    BaseVector* resultVec = nullptr;
    UnixSecondsTestHelper::ExecuteUnixSeconds(inputVec, OMNI_TIMESTAMP, resultVec);
    UnixSecondsTestHelper::ValidateResult(resultVec, expected, timestampMicros.size());

    delete inputVec;
    delete resultVec;
}
