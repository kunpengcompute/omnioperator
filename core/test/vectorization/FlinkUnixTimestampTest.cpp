/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: flink_unix_timestamp function unit tests
 *
 * flink_unix_timestamp() -> int64 (OMNI_LONG, current epoch seconds)
 * flink_unix_timestamp_with_tz(string[, format], tz) -> int64 (epoch seconds)
 *
 * Mirrors Flink's UNIX_TIMESTAMP:
 *   - 0-arg (flink_unix_timestamp): current epoch seconds (non-deterministic).
 *   - string forms (flink_unix_timestamp_with_tz): default/explicit format,
 *     session timezone applied via trailing VARCHAR tz arg (Plan A), seconds.
 *   - Parse failure -> Long.MIN_VALUE (not NULL).
 *   - NULL input -> NULL.
 *
 * Only the 0-arg form is registered as flink_unix_timestamp; all string forms
 * are registered as flink_unix_timestamp_with_tz (the OmniAdaptor always
 * appends the session zone-id for UNIX_TIMESTAMP(string[, fmt])).
 *
 * Expected values verified against Flink docs/examples and cross-checked with
 * Python. Seconds = int(datetime(...,tz).timestamp()) where tz matches the
 * session zone-id passed as the trailing arg.
 */

#include <gtest/gtest.h>
#include <iostream>
#include <vector>
#include <string>
#include <string_view>
#include <limits>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/functions/FlinkUnixTimestamp.h"
#include "vectorization/VectorFunction.h"
#include "codegen/func_signature.h"
#include "vector/vector_helper.h"
#include "vector/vector.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::codegen;
using namespace omniruntime::TestUtil;

namespace {
constexpr int64_t kLongMinValue = std::numeric_limits<int64_t>::min();
}

// Initialize function registration before running tests
class FlinkUnixTimestampTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const flink_unix_timestamp_test_env =
    ::testing::AddGlobalTestEnvironment(new FlinkUnixTimestampTestEnvironment);

class FlinkUnixTimestampTestHelper {
public:
    using StringVec = Vector<LargeStringContainer<std::string_view>>;

    static void ValidateResult(BaseVector* result, const std::vector<int64_t>& expected, int rowSize) {
        auto* resultVec = dynamic_cast<Vector<int64_t>*>(result);
        ASSERT_NE(resultVec, nullptr) << "Result vector type mismatch";
        for (int i = 0; i < rowSize; ++i) {
            if (result->IsNull(i)) {
                std::cout << "Row " << i << ": NULL" << std::endl;
                continue;
            }
            int64_t actualValue = resultVec->GetValue(i);
            std::cout << "Row " << i << ": Expected=" << expected[i] << ", Actual=" << actualValue << std::endl;
            EXPECT_EQ(actualValue, expected[i]) << "Row " << i << " value mismatch";
        }
    }

    static BaseVector* CreateStringVector(const std::vector<std::string>& values) {
        auto* vec = new StringVec(values.size());
        for (size_t i = 0; i < values.size(); ++i) {
            std::string_view sv(values[i].data(), values[i].size());
            vec->SetValue(i, sv);
        }
        return vec;
    }

    static BaseVector* CreateConstStringVector(const std::string& value, int32_t size) {
        auto* vec = new StringVec(size);
        std::string_view sv(value.data(), value.size());
        for (int32_t i = 0; i < size; ++i) {
            vec->SetValue(i, sv);
        }
        return vec;
    }

    // Execute flink_unix_timestamp (0-arg only).
    static void ExecuteNoArg(BaseVector*& result, int32_t rowSize) {
        auto signature = std::make_shared<FunctionSignature>("flink_unix_timestamp",
            std::vector<DataTypeId>{}, OMNI_LONG);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "flink_unix_timestamp (0-arg) not found";

        auto outputType = std::make_shared<DataType>(OMNI_LONG);
        ExecutionContext context;
        context.SetResultRowSize(rowSize);
        std::stack<BaseVector*> args;
        ASSERT_NO_THROW(function->Apply(args, outputType, result, &context))
            << "flink_unix_timestamp (0-arg) threw an exception";
    }

    // Execute flink_unix_timestamp_with_tz with the given arg stack.
    static void ExecuteWithTz(const std::vector<DataTypeId>& inputTypes,
        std::stack<BaseVector*>& args, BaseVector*& result, int32_t rowSize) {
        auto signature = std::make_shared<FunctionSignature>("flink_unix_timestamp_with_tz",
            inputTypes, OMNI_LONG);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "flink_unix_timestamp_with_tz not found for signature";

        auto outputType = std::make_shared<DataType>(OMNI_LONG);
        ExecutionContext context;
        context.SetResultRowSize(rowSize);
        ASSERT_NO_THROW(function->Apply(args, outputType, result, &context))
            << "flink_unix_timestamp_with_tz threw an exception";
    }
};

// ============================================================================
// 0-arg form: flink_unix_timestamp() -> current epoch seconds
// ============================================================================

TEST(FlinkUnixTimestampTest, NoArgReturnsCurrentSeconds) {
    std::cout << "=== Test: flink_unix_timestamp() no-arg ===" << std::endl;
    BaseVector* result = nullptr;
    FlinkUnixTimestampTestHelper::ExecuteNoArg(result, 3);

    ASSERT_NE(result, nullptr);
    auto* resultVec = dynamic_cast<Vector<int64_t>*>(result);
    ASSERT_NE(resultVec, nullptr);
    int64_t v0 = resultVec->GetValue(0);
    int64_t v1 = resultVec->GetValue(1);
    int64_t v2 = resultVec->GetValue(2);
    std::cout << "current seconds = " << v0 << std::endl;
    EXPECT_GT(v0, 0) << "current epoch seconds should be > 0";
    EXPECT_EQ(v0, v1) << "all rows in a batch share the same value";
    EXPECT_EQ(v0, v2);
    delete result;
}

// ============================================================================
// 1-arg + tz: flink_unix_timestamp_with_tz(string, tz)
// ============================================================================

TEST(FlinkUnixTimestampTest, WithTzOneArgUtc) {
    // session tz = "UTC": '1970-01-01 00:00:00' -> 0, '2017-09-15 00:00:00' -> 1505433600.
    std::vector<std::string> inputs = {"1970-01-01 00:00:00", "2017-09-15 00:00:00"};
    std::vector<int64_t> expected = {0, 1505433600};

    BaseVector* inputVec = FlinkUnixTimestampTestHelper::CreateStringVector(inputs);
    BaseVector* tzVec = FlinkUnixTimestampTestHelper::CreateConstStringVector("UTC", inputs.size());
    std::stack<BaseVector*> args;
    args.push(inputVec);
    args.push(tzVec);
    BaseVector* result = nullptr;
    FlinkUnixTimestampTestHelper::ExecuteWithTz({OMNI_VARCHAR, OMNI_VARCHAR}, args, result, inputs.size());
    FlinkUnixTimestampTestHelper::ValidateResult(result, expected, inputs.size());
    delete result;
}

TEST(FlinkUnixTimestampTest, WithTzOneArgShanghai) {
    // session tz = "Asia/Shanghai" (+8):
    //   '1970-01-01 08:00:00' Shanghai = 00:00:00 UTC = 0.
    //   '1970-01-01 08:00:01' Shanghai = 00:00:01 UTC = 1.
    //   '2017-09-15 08:00:00' Shanghai = 00:00 UTC = 1505433600.
    std::vector<std::string> inputs = {
        "1970-01-01 08:00:00",
        "1970-01-01 08:00:01",
        "2017-09-15 08:00:00"
    };
    std::vector<int64_t> expected = {0, 1, 1505433600};

    BaseVector* inputVec = FlinkUnixTimestampTestHelper::CreateStringVector(inputs);
    BaseVector* tzVec = FlinkUnixTimestampTestHelper::CreateConstStringVector("Asia/Shanghai", inputs.size());
    std::stack<BaseVector*> args;
    args.push(inputVec);
    args.push(tzVec);
    BaseVector* result = nullptr;
    FlinkUnixTimestampTestHelper::ExecuteWithTz({OMNI_VARCHAR, OMNI_VARCHAR}, args, result, inputs.size());
    FlinkUnixTimestampTestHelper::ValidateResult(result, expected, inputs.size());
    delete result;
}

TEST(FlinkUnixTimestampTest, WithTzOneArgPreEpoch) {
    // 1969-12-31 23:59:59 (session tz=UTC) -> -1.
    std::vector<std::string> inputs = {"1969-12-31 23:59:59"};
    std::vector<int64_t> expected = {-1};

    BaseVector* inputVec = FlinkUnixTimestampTestHelper::CreateStringVector(inputs);
    BaseVector* tzVec = FlinkUnixTimestampTestHelper::CreateConstStringVector("UTC", inputs.size());
    std::stack<BaseVector*> args;
    args.push(inputVec);
    args.push(tzVec);
    BaseVector* result = nullptr;
    FlinkUnixTimestampTestHelper::ExecuteWithTz({OMNI_VARCHAR, OMNI_VARCHAR}, args, result, inputs.size());
    FlinkUnixTimestampTestHelper::ValidateResult(result, expected, inputs.size());
    delete result;
}

TEST(FlinkUnixTimestampTest, WithTzOneArgParseFailureReturnsLongMinValue) {
    // Flink semantics: parse failure -> Long.MIN_VALUE (not NULL).
    std::vector<std::string> inputs = {"abc", "not-a-date", "2017/09/15 00:00:00"};
    BaseVector* inputVec = FlinkUnixTimestampTestHelper::CreateStringVector(inputs);
    BaseVector* tzVec = FlinkUnixTimestampTestHelper::CreateConstStringVector("UTC", inputs.size());
    std::stack<BaseVector*> args;
    args.push(inputVec);
    args.push(tzVec);
    BaseVector* result = nullptr;
    FlinkUnixTimestampTestHelper::ExecuteWithTz({OMNI_VARCHAR, OMNI_VARCHAR}, args, result, inputs.size());

    ASSERT_NE(result, nullptr);
    auto* resultVec = dynamic_cast<Vector<int64_t>*>(result);
    ASSERT_NE(resultVec, nullptr);
    for (size_t i = 0; i < inputs.size(); ++i) {
        EXPECT_FALSE(result->IsNull(i)) << "Row " << i << " should NOT be NULL (Flink returns Long.MIN_VALUE)";
        EXPECT_EQ(resultVec->GetValue(i), kLongMinValue)
            << "Row " << i << " ('" << inputs[i] << "') should be Long.MIN_VALUE";
    }
    delete result;
}

TEST(FlinkUnixTimestampTest, WithTzOneArgNullInput) {
    // NULL input -> NULL (distinct from Long.MIN_VALUE parse failure).
    std::vector<std::string> inputs = {"1970-01-01 00:00:00", "2017-09-15 00:00:00"};
    BaseVector* inputVec = FlinkUnixTimestampTestHelper::CreateStringVector(inputs);
    inputVec->SetNull(1);
    BaseVector* tzVec = FlinkUnixTimestampTestHelper::CreateConstStringVector("UTC", inputs.size());

    std::stack<BaseVector*> args;
    args.push(inputVec);
    args.push(tzVec);
    BaseVector* result = nullptr;
    FlinkUnixTimestampTestHelper::ExecuteWithTz({OMNI_VARCHAR, OMNI_VARCHAR}, args, result, inputs.size());

    ASSERT_NE(result, nullptr);
    EXPECT_FALSE(result->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(result->IsNull(1)) << "Row 1 (NULL input) should be NULL";
    auto* resultVec = dynamic_cast<Vector<int64_t>*>(result);
    EXPECT_EQ(resultVec->GetValue(0), 0);
    delete result;
}

// ============================================================================
// 2-arg + tz: flink_unix_timestamp_with_tz(string, format, tz)
// ============================================================================

TEST(FlinkUnixTimestampTest, WithTzTwoArgUtc) {
    std::vector<std::string> inputs = {"20170915000000", "2017-09-15", "2024-01-15 08:30:45"};
    std::vector<std::string> formats = {"yyyyMMddHHmmss", "yyyy-MM-dd", "yyyy-MM-dd HH:mm:ss"};
    std::vector<int64_t> expected = {1505433600, 1505433600, 1705307445};

    BaseVector* inputVec = FlinkUnixTimestampTestHelper::CreateStringVector(inputs);
    BaseVector* formatVec = FlinkUnixTimestampTestHelper::CreateStringVector(formats);
    BaseVector* tzVec = FlinkUnixTimestampTestHelper::CreateConstStringVector("UTC", inputs.size());
    std::stack<BaseVector*> args;
    args.push(inputVec);
    args.push(formatVec);
    args.push(tzVec);
    BaseVector* result = nullptr;
    FlinkUnixTimestampTestHelper::ExecuteWithTz(
        {OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR}, args, result, inputs.size());
    FlinkUnixTimestampTestHelper::ValidateResult(result, expected, inputs.size());
    delete result;
}

TEST(FlinkUnixTimestampTest, WithTzTwoArgConstFormat) {
    std::vector<std::string> inputs = {"2017-09-15 00:00:00", "1970-01-01 00:00:01"};
    std::vector<int64_t> expected = {1505433600, 1};

    BaseVector* inputVec = FlinkUnixTimestampTestHelper::CreateStringVector(inputs);
    BaseVector* formatVec = FlinkUnixTimestampTestHelper::CreateConstStringVector("yyyy-MM-dd HH:mm:ss", inputs.size());
    BaseVector* tzVec = FlinkUnixTimestampTestHelper::CreateConstStringVector("UTC", inputs.size());
    std::stack<BaseVector*> args;
    args.push(inputVec);
    args.push(formatVec);
    args.push(tzVec);
    BaseVector* result = nullptr;
    FlinkUnixTimestampTestHelper::ExecuteWithTz(
        {OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR}, args, result, inputs.size());
    FlinkUnixTimestampTestHelper::ValidateResult(result, expected, inputs.size());
    delete result;
}

TEST(FlinkUnixTimestampTest, WithTzTwoArgParseFailureReturnsLongMinValue) {
    std::vector<std::string> inputs = {"abc", "2017-09-15", "20170915000000"};
    std::vector<std::string> formats = {"yyyy-MM-dd", "yyyyMMddHHmmss", "yyyy-MM-dd HH:mm:ss"};

    BaseVector* inputVec = FlinkUnixTimestampTestHelper::CreateStringVector(inputs);
    BaseVector* formatVec = FlinkUnixTimestampTestHelper::CreateStringVector(formats);
    BaseVector* tzVec = FlinkUnixTimestampTestHelper::CreateConstStringVector("UTC", inputs.size());
    std::stack<BaseVector*> args;
    args.push(inputVec);
    args.push(formatVec);
    args.push(tzVec);
    BaseVector* result = nullptr;
    FlinkUnixTimestampTestHelper::ExecuteWithTz(
        {OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR}, args, result, inputs.size());

    ASSERT_NE(result, nullptr);
    auto* resultVec = dynamic_cast<Vector<int64_t>*>(result);
    ASSERT_NE(resultVec, nullptr);
    for (size_t i = 0; i < inputs.size(); ++i) {
        EXPECT_FALSE(result->IsNull(i)) << "Row " << i << " should NOT be NULL (Long.MIN_VALUE)";
        EXPECT_EQ(resultVec->GetValue(i), kLongMinValue)
            << "Row " << i << " should be Long.MIN_VALUE (parse failure)";
    }
    delete result;
}

TEST(FlinkUnixTimestampTest, WithTzTwoArgNullInput) {
    std::vector<std::string> inputs = {"2017-09-15 00:00:00", "2024-01-15 08:30:45"};
    std::vector<std::string> formats = {"yyyy-MM-dd HH:mm:ss", "yyyy-MM-dd HH:mm:ss"};

    BaseVector* inputVec = FlinkUnixTimestampTestHelper::CreateStringVector(inputs);
    inputVec->SetNull(1);
    BaseVector* formatVec = FlinkUnixTimestampTestHelper::CreateStringVector(formats);
    BaseVector* tzVec = FlinkUnixTimestampTestHelper::CreateConstStringVector("UTC", inputs.size());

    std::stack<BaseVector*> args;
    args.push(inputVec);
    args.push(formatVec);
    args.push(tzVec);
    BaseVector* result = nullptr;
    FlinkUnixTimestampTestHelper::ExecuteWithTz(
        {OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR}, args, result, inputs.size());

    ASSERT_NE(result, nullptr);
    EXPECT_FALSE(result->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(result->IsNull(1)) << "Row 1 (NULL input) should be NULL";
    auto* resultVec = dynamic_cast<Vector<int64_t>*>(result);
    EXPECT_EQ(resultVec->GetValue(0), 1505433600);
    delete result;
}

// ============================================================================
// Multi-row batch mixing valid/invalid/null rows (1-arg + tz)
// ============================================================================

TEST(FlinkUnixTimestampTest, WithTzMultiRowBatchMixed) {
    std::vector<std::string> inputs = {
        "1970-01-01 00:00:00",   // valid -> 0
        "abc",                   // invalid -> Long.MIN_VALUE
        "2017-09-15 00:00:00",   // valid -> 1505433600
        "1969-12-31 23:59:59"    // valid pre-epoch -> -1
    };

    BaseVector* inputVec = FlinkUnixTimestampTestHelper::CreateStringVector(inputs);
    BaseVector* tzVec = FlinkUnixTimestampTestHelper::CreateConstStringVector("UTC", inputs.size());
    std::stack<BaseVector*> args;
    args.push(inputVec);
    args.push(tzVec);
    BaseVector* result = nullptr;
    FlinkUnixTimestampTestHelper::ExecuteWithTz({OMNI_VARCHAR, OMNI_VARCHAR}, args, result, inputs.size());

    ASSERT_NE(result, nullptr);
    auto* resultVec = dynamic_cast<Vector<int64_t>*>(result);
    ASSERT_NE(resultVec, nullptr);
    EXPECT_FALSE(result->IsNull(0));
    EXPECT_FALSE(result->IsNull(1)) << "Row 1 ('abc') should NOT be NULL (Long.MIN_VALUE)";
    EXPECT_FALSE(result->IsNull(2));
    EXPECT_FALSE(result->IsNull(3));
    EXPECT_EQ(resultVec->GetValue(0), 0);
    EXPECT_EQ(resultVec->GetValue(1), kLongMinValue);
    EXPECT_EQ(resultVec->GetValue(2), 1505433600);
    EXPECT_EQ(resultVec->GetValue(3), -1);
    delete result;
}
