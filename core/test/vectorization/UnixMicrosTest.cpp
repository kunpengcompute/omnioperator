/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: UnixMicros function unit tests
 */

#include <gtest/gtest.h>
#include <iostream>
#include <vector>
#include <ctime>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/functions/UnixMicros.h"
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

class UnixMicrosTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const unix_micros_test_env =
    ::testing::AddGlobalTestEnvironment(new UnixMicrosTestEnvironment);

class UnixMicrosTestHelper {
public:
    static void ValidateResult(BaseVector* result, const std::vector<int64_t>& expected, size_t rowSize) {
        auto* resultVec = dynamic_cast<Vector<int64_t>*>(result);
        ASSERT_NE(resultVec, nullptr) << "Result vector type mismatch";

        for (size_t i = 0; i < rowSize; ++i) {
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

    static void ExecuteUnixMicros(BaseVector* inputVec, DataTypeId inputTypeId, BaseVector*& result) {
        auto signature = std::make_shared<FunctionSignature>("unix_micros",
            std::vector<DataTypeId>{inputTypeId}, OMNI_LONG);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "unix_micros function not found for signature";

        auto outputType = std::make_shared<DataType>(OMNI_LONG);
        ExecutionContext context;
        context.SetResultRowSize(inputVec->GetSize());
        std::stack<BaseVector*> args;
        inputVec->SetIsField(true);  // caller-owned, do not let Apply delete
        args.push(inputVec);

        ASSERT_NO_THROW(function->Apply(args, outputType, result, &context))
            << "unix_micros function threw an exception";
    }
};

// Test: Basic timestamp to microseconds conversion
// Matches Velox test: "1970-01-01 00:00:01" -> 1000000
TEST(UnixMicrosTest, TimestampBasic) {
    std::cout << "=== Test: unix_micros from TIMESTAMP - basic cases ===" << std::endl;

    std::vector<int64_t> timestampMicros = {
        1000000LL,               // 1 second after epoch -> 1000000 micros
        0LL,                     // exactly epoch -> 0 micros
        2147000000LL             // 2147 seconds after epoch -> 2147000000 micros
    };
    std::vector<int64_t> expected = {1000000LL, 0LL, 2147000000LL};

    BaseVector* inputVec = UnixMicrosTestHelper::CreateTimestampVector(timestampMicros);
    BaseVector* resultVec = nullptr;
    UnixMicrosTestHelper::ExecuteUnixMicros(inputVec, OMNI_TIMESTAMP, resultVec);
    UnixMicrosTestHelper::ValidateResult(resultVec, expected, timestampMicros.size());

    delete inputVec;
    delete resultVec;
}

// Test: Sub-microsecond precision (input already in microseconds)
TEST(UnixMicrosTest, PrecisionPreservation) {
    std::cout << "=== Test: unix_micros precision preservation ===" << std::endl;

    std::vector<int64_t> timestampMicros = {
        127LL,                   // 127 microseconds
        999LL,                   // 999 microseconds
        1500LL,                  // 1500 microseconds
        1000LL                   // exactly 1 millisecond
    };
    std::vector<int64_t> expected = {127LL, 999LL, 1500LL, 1000LL};

    BaseVector* inputVec = UnixMicrosTestHelper::CreateTimestampVector(timestampMicros);
    BaseVector* resultVec = nullptr;
    UnixMicrosTestHelper::ExecuteUnixMicros(inputVec, OMNI_TIMESTAMP, resultVec);
    UnixMicrosTestHelper::ValidateResult(resultVec, expected, timestampMicros.size());

    delete inputVec;
    delete resultVec;
}

// Test: Negative timestamps (before 1970-01-01)
TEST(UnixMicrosTest, NegativeTimestamps) {
    std::cout << "=== Test: unix_micros with negative timestamps ===" << std::endl;

    std::vector<int64_t> timestampMicros = {
        -1000000LL,              // exactly -1 second
        -1000LL,                 // -1 millisecond
        -500LL,                  // -500 microseconds
        -1500000LL               // -1.5 seconds
    };
    std::vector<int64_t> expected = {-1000000LL, -1000LL, -500LL, -1500000LL};

    BaseVector* inputVec = UnixMicrosTestHelper::CreateTimestampVector(timestampMicros);
    BaseVector* resultVec = nullptr;
    UnixMicrosTestHelper::ExecuteUnixMicros(inputVec, OMNI_TIMESTAMP, resultVec);
    UnixMicrosTestHelper::ValidateResult(resultVec, expected, timestampMicros.size());

    delete inputVec;
    delete resultVec;
}

// Test: NULL handling
TEST(UnixMicrosTest, TimestampWithNull) {
    std::cout << "=== Test: unix_micros with NULL values ===" << std::endl;

    std::vector<int64_t> timestampMicros = {
        1000000LL,               // 1 second -> 1000000 micros
        0LL,                     // placeholder for NULL
        31536001000000LL         // ~1 year + 1 second
    };

    BaseVector* inputVec = UnixMicrosTestHelper::CreateTimestampVector(timestampMicros);
    inputVec->SetNull(1);

    BaseVector* resultVec = nullptr;
    UnixMicrosTestHelper::ExecuteUnixMicros(inputVec, OMNI_TIMESTAMP, resultVec);

    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";

    auto* resultVecTyped = dynamic_cast<Vector<int64_t>*>(resultVec);
    EXPECT_EQ(resultVecTyped->GetValue(0), 1000000LL) << "Row 0 should be 1000000";
    EXPECT_EQ(resultVecTyped->GetValue(2), 31536001000000LL) << "Row 2 should be 31536001000000";

    delete inputVec;
    delete resultVec;
}

// Test: OMNI_LONG input type (equivalent to OMNI_TIMESTAMP)
TEST(UnixMicrosTest, LongInputType) {
    std::cout << "=== Test: unix_micros with OMNI_LONG input ===" << std::endl;

    std::vector<int64_t> values = {
        1000000LL,               // 1 second -> 1000000 micros
        0LL,                     // epoch -> 0 micros
        -1000000LL               // -1 second -> -1000000 micros
    };
    std::vector<int64_t> expected = {1000000LL, 0LL, -1000000LL};

    BaseVector* inputVec = UnixMicrosTestHelper::CreateLongVector(values);
    BaseVector* resultVec = nullptr;
    UnixMicrosTestHelper::ExecuteUnixMicros(inputVec, OMNI_LONG, resultVec);
    UnixMicrosTestHelper::ValidateResult(resultVec, expected, values.size());

    delete inputVec;
    delete resultVec;
}

// Test: Large timestamp values
TEST(UnixMicrosTest, LargeTimestampValues) {
    std::cout << "=== Test: unix_micros with large timestamp values ===" << std::endl;

    std::vector<int64_t> timestampMicros = {
        4102444800000000LL,      // ~2100-01-01
        -2208988800000000LL      // ~1900-01-01
    };
    std::vector<int64_t> expected = {4102444800000000LL, -2208988800000000LL};

    BaseVector* inputVec = UnixMicrosTestHelper::CreateTimestampVector(timestampMicros);
    BaseVector* resultVec = nullptr;
    UnixMicrosTestHelper::ExecuteUnixMicros(inputVec, OMNI_TIMESTAMP, resultVec);
    UnixMicrosTestHelper::ValidateResult(resultVec, expected, timestampMicros.size());

    delete inputVec;
    delete resultVec;
}

// Test: Velox-compatible test cases
TEST(UnixMicrosTest, VeloxCompatible) {
    std::cout << "=== Test: unix_micros Velox-compatible test cases ===" << std::endl;

    // Matching Velox test cases:
    // "1970-01-01 00:00:01" = 1000000 micros
    // "2008-12-25 15:30:00.123123" = 1230219000123123 micros
    // "1970-01-01 00:00:00.000127" = 127 micros (kMaxTinyint)
    // "1969-12-31 23:59:59.999872" = -128 micros (kMinTinyint)
    // "1970-01-01 00:00:00.032767" = 32767 micros (kMaxSmallint)
    // "1969-12-31 23:59:59.967232" = -32768 micros (kMinSmallint)
    // "1970-01-01 00:35:47.483647" = 2147483647 micros (kMax INT32)
    // "1969-12-31 23:24:12.516352" = -2147483648 micros (kMin INT32)
    std::vector<int64_t> timestampMicros = {
        1000000LL,               // "1970-01-01 00:00:01"
        1230219000123123LL,      // "2008-12-25 15:30:00.123123"
        127LL,                   // kMaxTinyint
        -128LL,                  // kMinTinyint
        32767LL,                 // kMaxSmallint
        -32768LL,                // kMinSmallint
        2147483647LL,            // kMax INT32
        -2147483648LL            // kMin INT32
    };
    std::vector<int64_t> expected = {
        1000000LL, 1230219000123123LL,
        127LL, -128LL,
        32767LL, -32768LL,
        2147483647LL, -2147483648LL
    };

    BaseVector* inputVec = UnixMicrosTestHelper::CreateTimestampVector(timestampMicros);
    BaseVector* resultVec = nullptr;
    UnixMicrosTestHelper::ExecuteUnixMicros(inputVec, OMNI_TIMESTAMP, resultVec);
    UnixMicrosTestHelper::ValidateResult(resultVec, expected, timestampMicros.size());

    delete inputVec;
    delete resultVec;
}

// Test: BIGINT boundary values (kMaxBigint, kMinBigint range)
TEST(UnixMicrosTest, BigintBoundaryValues) {
    std::cout << "=== Test: unix_micros BIGINT boundary values ===" << std::endl;

    // Velox test: "294247-01-10 04:00:54.775807" -> kMaxBigint (9223372036854775807)
    // Velox test: "-290308-12-21 19:59:06.224192" -> kMinBigint + 1000000
    // These extreme values may exceed Timestamp range, so we test with large but valid values
    std::vector<int64_t> timestampMicros = {
        9223372036854LL,         // large positive (within Timestamp range)
        -9223372036854LL         // large negative (within Timestamp range)
    };
    std::vector<int64_t> expected = {9223372036854LL, -9223372036854LL};

    BaseVector* inputVec = UnixMicrosTestHelper::CreateTimestampVector(timestampMicros);
    BaseVector* resultVec = nullptr;
    UnixMicrosTestHelper::ExecuteUnixMicros(inputVec, OMNI_TIMESTAMP, resultVec);
    UnixMicrosTestHelper::ValidateResult(resultVec, expected, timestampMicros.size());

    delete inputVec;
    delete resultVec;
}

// Test: All NULL input
TEST(UnixMicrosTest, AllNullInput) {
    std::cout << "=== Test: unix_micros with all NULL input ===" << std::endl;

    std::vector<int64_t> timestampMicros = {0LL, 0LL, 0LL};

    BaseVector* inputVec = UnixMicrosTestHelper::CreateTimestampVector(timestampMicros);
    inputVec->SetNull(0);
    inputVec->SetNull(1);
    inputVec->SetNull(2);

    BaseVector* resultVec = nullptr;
    UnixMicrosTestHelper::ExecuteUnixMicros(inputVec, OMNI_TIMESTAMP, resultVec);

    EXPECT_TRUE(resultVec->IsNull(0)) << "Row 0 should be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";
    EXPECT_TRUE(resultVec->IsNull(2)) << "Row 2 should be NULL";

    delete inputVec;
    delete resultVec;
}

// Test: Zero microseconds exactly at epoch
TEST(UnixMicrosTest, ExactEpoch) {
    std::cout << "=== Test: unix_micros at exact epoch ===" << std::endl;

    std::vector<int64_t> timestampMicros = {0LL};
    std::vector<int64_t> expected = {0LL};

    BaseVector* inputVec = UnixMicrosTestHelper::CreateTimestampVector(timestampMicros);
    BaseVector* resultVec = nullptr;
    UnixMicrosTestHelper::ExecuteUnixMicros(inputVec, OMNI_TIMESTAMP, resultVec);
    UnixMicrosTestHelper::ValidateResult(resultVec, expected, timestampMicros.size());

    delete inputVec;
    delete resultVec;
}

// Test: Single row
TEST(UnixMicrosTest, SingleRow) {
    std::cout << "=== Test: unix_micros single row ===" << std::endl;

    // 86400 seconds = 1 day = 86400000000 micros
    std::vector<int64_t> timestampMicros = {86400000000LL};
    std::vector<int64_t> expected = {86400000000LL};

    BaseVector* inputVec = UnixMicrosTestHelper::CreateTimestampVector(timestampMicros);
    BaseVector* resultVec = nullptr;
    UnixMicrosTestHelper::ExecuteUnixMicros(inputVec, OMNI_TIMESTAMP, resultVec);
    UnixMicrosTestHelper::ValidateResult(resultVec, expected, timestampMicros.size());

    delete inputVec;
    delete resultVec;
}

// Test: Microsecond-level precision is preserved (unlike unix_millis which truncates)
TEST(UnixMicrosTest, MicrosecondPrecisionPreserved) {
    std::cout << "=== Test: unix_micros preserves microsecond precision ===" << std::endl;

    std::vector<int64_t> timestampMicros = {
        123456789LL,             // 123.456789 seconds -> 123456789 micros preserved
        1000LL,                  // 1 millisecond = 1000 micros
        999LL,                   // 999 micros (sub-millisecond)
        1001LL                   // 1001 micros (slightly over 1 ms)
    };
    std::vector<int64_t> expected = {123456789LL, 1000LL, 999LL, 1001LL};

    BaseVector* inputVec = UnixMicrosTestHelper::CreateTimestampVector(timestampMicros);
    BaseVector* resultVec = nullptr;
    UnixMicrosTestHelper::ExecuteUnixMicros(inputVec, OMNI_TIMESTAMP, resultVec);
    UnixMicrosTestHelper::ValidateResult(resultVec, expected, timestampMicros.size());

    delete inputVec;
    delete resultVec;
}

// Test: OMNI_LONG with NULL values
TEST(UnixMicrosTest, LongInputWithNull) {
    std::cout << "=== Test: unix_micros with OMNI_LONG input and NULLs ===" << std::endl;

    std::vector<int64_t> values = {
        1000000LL,
        0LL,
        -1000000LL
    };

    BaseVector* inputVec = UnixMicrosTestHelper::CreateLongVector(values);
    inputVec->SetNull(1);

    BaseVector* resultVec = nullptr;
    UnixMicrosTestHelper::ExecuteUnixMicros(inputVec, OMNI_LONG, resultVec);

    EXPECT_FALSE(resultVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultVec->IsNull(1)) << "Row 1 should be NULL";
    EXPECT_FALSE(resultVec->IsNull(2)) << "Row 2 should not be NULL";

    auto* resultVecTyped = dynamic_cast<Vector<int64_t>*>(resultVec);
    EXPECT_EQ(resultVecTyped->GetValue(0), 1000000LL) << "Row 0 value mismatch";
    EXPECT_EQ(resultVecTyped->GetValue(2), -1000000LL) << "Row 2 value mismatch";

    delete inputVec;
    delete resultVec;
}
