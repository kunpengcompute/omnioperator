/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: UnixMillis function unit tests
 */

#include <gtest/gtest.h>
#include <iostream>
#include <vector>
#include <ctime>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/functions/UnixMillis.h"
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

class UnixMillisTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const unix_millis_test_env =
    ::testing::AddGlobalTestEnvironment(new UnixMillisTestEnvironment);

class UnixMillisTestHelper {
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

    static void ExecuteUnixMillis(BaseVector* inputVec, DataTypeId inputTypeId, BaseVector*& result) {
        auto signature = std::make_shared<FunctionSignature>("unix_millis",
            std::vector<DataTypeId>{inputTypeId}, OMNI_LONG);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "unix_millis function not found for signature";

        auto outputType = std::make_shared<DataType>(OMNI_LONG);
        ExecutionContext context;
        context.SetResultRowSize(inputVec->GetSize());
        std::stack<BaseVector*> args;
        inputVec->SetIsField(true);  // caller-owned, do not let Apply delete
        args.push(inputVec);

        ASSERT_NO_THROW(function->Apply(args, outputType, result, &context))
            << "unix_millis function threw an exception";
    }
};

// Test: Basic timestamp to milliseconds conversion
// Matches Velox test: "1970-01-01 00:00:01" -> 1000
TEST(UnixMillisTest, TimestampBasic) {
    std::cout << "=== Test: unix_millis from TIMESTAMP - basic cases ===" << std::endl;

    // 1 second = 1000000 micros -> 1000 millis
    // 0 micros -> 0 millis (epoch)
    // 2147 seconds = 2147000000 micros -> 2147000 millis
    std::vector<int64_t> timestampMicros = {
        1000000LL,               // 1 second after epoch -> 1000 millis
        0LL,                     // exactly epoch -> 0 millis
        2147000000LL             // 2147 seconds after epoch -> 2147000 millis
    };
    std::vector<int64_t> expected = {1000, 0, 2147000};

    BaseVector* inputVec = UnixMillisTestHelper::CreateTimestampVector(timestampMicros);
    BaseVector* resultVec = nullptr;
    UnixMillisTestHelper::ExecuteUnixMillis(inputVec, OMNI_TIMESTAMP, resultVec);
    UnixMillisTestHelper::ValidateResult(resultVec, expected, timestampMicros.size());

    delete inputVec;
    delete resultVec;
}

// Test: Sub-millisecond precision is truncated (matches Velox behavior)
TEST(UnixMillisTest, SubMillisecondTruncation) {
    std::cout << "=== Test: unix_millis sub-millisecond truncation ===" << std::endl;

    // 127 micros = 0.127 millis -> should return 0 (truncated)
    // 999 micros = 0.999 millis -> should return 0 (truncated)
    // 1500 micros = 1.5 millis -> should return 1 (truncated)
    // 1000 micros = 1 millis -> should return 1
    std::vector<int64_t> timestampMicros = {
        127LL,                   // 0.127 millis
        999LL,                   // 0.999 millis
        1500LL,                  // 1.5 millis
        1000LL                   // exactly 1 millis
    };
    std::vector<int64_t> expected = {0, 0, 1, 1};

    BaseVector* inputVec = UnixMillisTestHelper::CreateTimestampVector(timestampMicros);
    BaseVector* resultVec = nullptr;
    UnixMillisTestHelper::ExecuteUnixMillis(inputVec, OMNI_TIMESTAMP, resultVec);
    UnixMillisTestHelper::ValidateResult(resultVec, expected, timestampMicros.size());

    delete inputVec;
    delete resultVec;
}

// Test: Negative timestamps (before 1970-01-01)
TEST(UnixMillisTest, NegativeTimestamps) {
    std::cout << "=== Test: unix_millis with negative timestamps ===" << std::endl;

    // -1000000 micros = -1 second -> -1000 millis
    // -1000 micros = -1 millis -> -1 millis
    // -500 micros = -0.5 millis -> -1 millis (floor behavior)
    // -1500000 micros = -1.5 seconds -> -1500 millis
    std::vector<int64_t> timestampMicros = {
        -1000000LL,              // exactly -1 second -> -1000 millis
        -1000LL,                 // exactly -1 millis -> -1
        -500LL,                  // -0.5 millis -> floor to -1
        -1500000LL               // -1.5 seconds -> -1500 millis
    };
    std::vector<int64_t> expected = {-1000, -1, -1, -1500};

    BaseVector* inputVec = UnixMillisTestHelper::CreateTimestampVector(timestampMicros);
    BaseVector* resultVec = nullptr;
    UnixMillisTestHelper::ExecuteUnixMillis(inputVec, OMNI_TIMESTAMP, resultVec);
    UnixMillisTestHelper::ValidateResult(resultVec, expected, timestampMicros.size());

    delete inputVec;
    delete resultVec;
}

// Test: NULL handling
TEST(UnixMillisTest, TimestampWithNull) {
    std::cout << "=== Test: unix_millis with NULL values ===" << std::endl;

    std::vector<int64_t> timestampMicros = {
        1000000LL,               // 1 second -> 1000 millis
        0LL,                     // placeholder for NULL
        31536001000000LL         // ~1 year + 1 second -> millis
    };

    BaseVector* inputVec = UnixMillisTestHelper::CreateTimestampVector(timestampMicros);
    inputVec->SetNull(1);

    BaseVector* resultVec = nullptr;
    UnixMillisTestHelper::ExecuteUnixMillis(inputVec, OMNI_TIMESTAMP, resultVec);

    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";

    auto* resultVecTyped = dynamic_cast<Vector<int64_t>*>(resultVec);
    EXPECT_EQ(resultVecTyped->GetValue(0), 1000) << "Row 0 should be 1000";
    EXPECT_EQ(resultVecTyped->GetValue(2), 31536001000LL) << "Row 2 should be 31536001000";

    delete inputVec;
    delete resultVec;
}

// Test: OMNI_LONG input type (equivalent to OMNI_TIMESTAMP)
TEST(UnixMillisTest, LongInputType) {
    std::cout << "=== Test: unix_millis with OMNI_LONG input ===" << std::endl;

    std::vector<int64_t> values = {
        1000000LL,               // 1 second -> 1000 millis
        0LL,                     // epoch -> 0 millis
        -1000000LL               // -1 second -> -1000 millis
    };
    std::vector<int64_t> expected = {1000, 0, -1000};

    BaseVector* inputVec = UnixMillisTestHelper::CreateLongVector(values);
    BaseVector* resultVec = nullptr;
    UnixMillisTestHelper::ExecuteUnixMillis(inputVec, OMNI_LONG, resultVec);
    UnixMillisTestHelper::ValidateResult(resultVec, expected, values.size());

    delete inputVec;
    delete resultVec;
}

// Test: Large timestamp values
TEST(UnixMillisTest, LargeTimestampValues) {
    std::cout << "=== Test: unix_millis with large timestamp values ===" << std::endl;

    // Year 2100 approximately: 4102444800 seconds = 4102444800000000 micros
    // Year 1900 approximately: -2208988800 seconds = -2208988800000000 micros
    std::vector<int64_t> timestampMicros = {
        4102444800000000LL,      // ~2100-01-01
        -2208988800000000LL      // ~1900-01-01
    };
    std::vector<int64_t> expected = {4102444800000LL, -2208988800000LL};

    BaseVector* inputVec = UnixMillisTestHelper::CreateTimestampVector(timestampMicros);
    BaseVector* resultVec = nullptr;
    UnixMillisTestHelper::ExecuteUnixMillis(inputVec, OMNI_TIMESTAMP, resultVec);
    UnixMillisTestHelper::ValidateResult(resultVec, expected, timestampMicros.size());

    delete inputVec;
    delete resultVec;
}

// Test: Velox-compatible epoch boundary values
TEST(UnixMillisTest, VeloxCompatible) {
    std::cout << "=== Test: unix_millis Velox-compatible test cases ===" << std::endl;

    // Matching Velox test cases (converted from timestamp string to microseconds):
    // "1970-01-01 00:00:01" = 1000000 micros -> 1000 millis
    // "2008-12-25 15:30:00.123" = 1230219000123000 micros -> 1230219000123 millis
    // "1970-01-01 00:00:00.127" = 127000 micros -> 127 millis (kMaxTinyint)
    // "1969-12-31 23:59:59.872" = -128000 micros -> -128 millis (kMinTinyint)
    // "1970-01-01 00:00:32.767" = 32767000 micros -> 32767 millis (kMaxSmallint)
    // "1969-12-31 23:59:27.232" = -32768000 micros -> -32768 millis (kMinSmallint)
    std::vector<int64_t> timestampMicros = {
        1000000LL,               // 1970-01-01 00:00:01
        1230219000123000LL,      // 2008-12-25 15:30:00.123
        127000LL,                // 1970-01-01 00:00:00.127 -> kMaxTinyint
        -128000LL,               // 1969-12-31 23:59:59.872 -> kMinTinyint
        32767000LL,              // 1970-01-01 00:00:32.767 -> kMaxSmallint
        -32768000LL              // 1969-12-31 23:59:27.232 -> kMinSmallint
    };
    std::vector<int64_t> expected = {1000, 1230219000123LL, 127, -128, 32767, -32768};

    BaseVector* inputVec = UnixMillisTestHelper::CreateTimestampVector(timestampMicros);
    BaseVector* resultVec = nullptr;
    UnixMillisTestHelper::ExecuteUnixMillis(inputVec, OMNI_TIMESTAMP, resultVec);
    UnixMillisTestHelper::ValidateResult(resultVec, expected, timestampMicros.size());

    delete inputVec;
    delete resultVec;
}

// Test: Velox kMax and kMin integer boundary values
TEST(UnixMillisTest, IntegerBoundaryValues) {
    std::cout << "=== Test: unix_millis integer boundary values ===" << std::endl;

    // kMax (INT32_MAX) = 2147483647 millis -> 2147483647000 micros
    // kMin (INT32_MIN) = -2147483648 millis -> -2147483648000 micros
    std::vector<int64_t> timestampMicros = {
        2147483647000LL,         // kMax millis in micros
        -2147483648000LL         // kMin millis in micros
    };
    std::vector<int64_t> expected = {2147483647LL, -2147483648LL};

    BaseVector* inputVec = UnixMillisTestHelper::CreateTimestampVector(timestampMicros);
    BaseVector* resultVec = nullptr;
    UnixMillisTestHelper::ExecuteUnixMillis(inputVec, OMNI_TIMESTAMP, resultVec);
    UnixMillisTestHelper::ValidateResult(resultVec, expected, timestampMicros.size());

    delete inputVec;
    delete resultVec;
}

// Test: All NULL input
TEST(UnixMillisTest, AllNullInput) {
    std::cout << "=== Test: unix_millis with all NULL input ===" << std::endl;

    std::vector<int64_t> timestampMicros = {0LL, 0LL, 0LL};

    BaseVector* inputVec = UnixMillisTestHelper::CreateTimestampVector(timestampMicros);
    inputVec->SetNull(0);
    inputVec->SetNull(1);
    inputVec->SetNull(2);

    BaseVector* resultVec = nullptr;
    UnixMillisTestHelper::ExecuteUnixMillis(inputVec, OMNI_TIMESTAMP, resultVec);

    EXPECT_TRUE(resultVec->IsNull(0)) << "Row 0 should be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";
    EXPECT_TRUE(resultVec->IsNull(2)) << "Row 2 should be NULL";

    delete inputVec;
    delete resultVec;
}

// Test: Zero microseconds exactly at epoch
TEST(UnixMillisTest, ExactEpoch) {
    std::cout << "=== Test: unix_millis at exact epoch ===" << std::endl;

    std::vector<int64_t> timestampMicros = {0LL};
    std::vector<int64_t> expected = {0};

    BaseVector* inputVec = UnixMillisTestHelper::CreateTimestampVector(timestampMicros);
    BaseVector* resultVec = nullptr;
    UnixMillisTestHelper::ExecuteUnixMillis(inputVec, OMNI_TIMESTAMP, resultVec);
    UnixMillisTestHelper::ValidateResult(resultVec, expected, timestampMicros.size());

    delete inputVec;
    delete resultVec;
}

// Test: Single row
TEST(UnixMillisTest, SingleRow) {
    std::cout << "=== Test: unix_millis single row ===" << std::endl;

    // 86400 seconds = 1 day = 86400000000 micros -> 86400000 millis
    std::vector<int64_t> timestampMicros = {86400000000LL};
    std::vector<int64_t> expected = {86400000LL};

    BaseVector* inputVec = UnixMillisTestHelper::CreateTimestampVector(timestampMicros);
    BaseVector* resultVec = nullptr;
    UnixMillisTestHelper::ExecuteUnixMillis(inputVec, OMNI_TIMESTAMP, resultVec);
    UnixMillisTestHelper::ValidateResult(resultVec, expected, timestampMicros.size());

    delete inputVec;
    delete resultVec;
}

// Test: Millisecond precision with fractional microseconds
TEST(UnixMillisTest, MillisecondPrecision) {
    std::cout << "=== Test: unix_millis with millisecond precision ===" << std::endl;

    // 123456789 micros = 123456.789 millis -> 123456 millis (truncated)
    // 1000 micros = 1 millis
    // 999 micros = 0.999 millis -> 0 (truncated)
    // 1001 micros = 1.001 millis -> 1 (truncated)
    std::vector<int64_t> timestampMicros = {
        123456789LL,             // 123456.789 millis -> 123456
        1000LL,                  // 1 millis
        999LL,                   // 0.999 millis -> 0
        1001LL                   // 1.001 millis -> 1
    };
    std::vector<int64_t> expected = {123456, 1, 0, 1};

    BaseVector* inputVec = UnixMillisTestHelper::CreateTimestampVector(timestampMicros);
    BaseVector* resultVec = nullptr;
    UnixMillisTestHelper::ExecuteUnixMillis(inputVec, OMNI_TIMESTAMP, resultVec);
    UnixMillisTestHelper::ValidateResult(resultVec, expected, timestampMicros.size());

    delete inputVec;
    delete resultVec;
}
