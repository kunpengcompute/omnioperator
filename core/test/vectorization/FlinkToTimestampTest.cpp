/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: flink_to_timestamp function unit tests
 *
 * flink_to_timestamp(string1[, string2]) -> int64 (OMNI_LONG, millis since epoch)
 * Mirrors Flink's TO_TIMESTAMP(string1[, string2]):
 *   - 1-arg: default format 'yyyy-MM-dd HH:mm:ss'.
 *   - 2-arg: explicit format string2.
 *   - No session timezone applied (wall-clock stored as UTC millis).
 *   - Returns NULL on parse failure or NULL input.
 *
 * Expected values below are derived from the Flink reference semantics
 * (DateTimeUtils.parseTimestampData), verified against ScalarFunctionsTest
 * (to_timestamp('2017-09-15 00:00:00') = 2017-09-15 00:00:00.000, i.e. epoch
 * millis 1505433600000). All expected values cross-checked against Python.
 */

#include <gtest/gtest.h>
#include <iostream>
#include <vector>
#include <string>
#include <string_view>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/functions/FlinkToTimestamp.h"
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

// Initialize function registration before running tests
class FlinkToTimestampTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const flink_to_timestamp_test_env =
    ::testing::AddGlobalTestEnvironment(new FlinkToTimestampTestEnvironment);

class FlinkToTimestampTestHelper {
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

    // Execute flink_to_timestamp with the given arg stack and signature.
    static void Execute(const std::vector<DataTypeId>& inputTypes, std::stack<BaseVector*>& args,
        BaseVector*& result) {
        auto signature = std::make_shared<FunctionSignature>("flink_to_timestamp", inputTypes, OMNI_LONG);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << "flink_to_timestamp function not found for signature";

        auto outputType = std::make_shared<DataType>(OMNI_LONG);
        ExecutionContext context;
        int32_t sz = 0;
        if (!args.empty()) {
            std::stack<BaseVector*> tmp = args;
            while (!tmp.empty()) {
                sz = tmp.top()->GetSize();
                tmp.pop();
            }
        }
        context.SetResultRowSize(sz);

        ASSERT_NO_THROW(function->Apply(args, outputType, result, &context))
            << "flink_to_timestamp function threw an exception";
    }
};

// ============================================================================
// 1-arg form: flink_to_timestamp(string) with default format 'yyyy-MM-dd HH:mm:ss'
// ============================================================================

TEST(FlinkToTimestampTest, OneArgBasicDefaultFormat) {
    std::cout << "=== Test: flink_to_timestamp 1-arg default format ===" << std::endl;

    // Flink ScalarFunctionsTest: to_timestamp('2017-09-15 00:00:00') = 2017-09-15 00:00:00.000
    // -> epoch millis 1505433600000 (UTC wall-clock, no tz).
    std::vector<std::string> inputs = {
        "2017-09-15 00:00:00",
        "2024-01-15 08:30:45"
    };
    std::vector<int64_t> expected = {1505433600000LL, 1705307445000LL};

    BaseVector* inputVec = FlinkToTimestampTestHelper::CreateStringVector(inputs);
    std::stack<BaseVector*> args;
    args.push(inputVec);
    BaseVector* result = nullptr;
    FlinkToTimestampTestHelper::Execute({OMNI_VARCHAR}, args, result);
    FlinkToTimestampTestHelper::ValidateResult(result, expected, inputs.size());

    delete result;
}

TEST(FlinkToTimestampTest, OneArgDateOnlyNoTime) {
    // Flink 1-arg default formatter is lenient: a bare date '2017-09-15' parses
    // with time defaulting to 00:00:00 -> 1505433600000.
    std::vector<std::string> inputs = {"2017-09-15"};
    std::vector<int64_t> expected = {1505433600000LL};

    BaseVector* inputVec = FlinkToTimestampTestHelper::CreateStringVector(inputs);
    std::stack<BaseVector*> args;
    args.push(inputVec);
    BaseVector* result = nullptr;
    FlinkToTimestampTestHelper::Execute({OMNI_VARCHAR}, args, result);
    FlinkToTimestampTestHelper::ValidateResult(result, expected, inputs.size());

    delete result;
}

TEST(FlinkToTimestampTest, OneArgParseFailureReturnsNull) {
    // Flink ScalarFunctionsTest: to_timestamp('abc') = NULL.
    std::vector<std::string> inputs = {"abc", "not-a-date", "2017/09/15 00:00:00"};
    BaseVector* inputVec = FlinkToTimestampTestHelper::CreateStringVector(inputs);
    std::stack<BaseVector*> args;
    args.push(inputVec);
    BaseVector* result = nullptr;
    FlinkToTimestampTestHelper::Execute({OMNI_VARCHAR}, args, result);

    ASSERT_NE(result, nullptr);
    for (size_t i = 0; i < inputs.size(); ++i) {
        EXPECT_TRUE(result->IsNull(i)) << "Row " << i << " ('" << inputs[i] << "') should be NULL";
    }
    delete result;
}

TEST(FlinkToTimestampTest, OneArgNullInput) {
    // Flink ScalarFunctionsTest: to_timestamp(cast(null as varchar)) = NULL.
    std::vector<std::string> inputs = {"2017-09-15 00:00:00", "2024-01-15 08:30:45"};
    BaseVector* inputVec = FlinkToTimestampTestHelper::CreateStringVector(inputs);
    inputVec->SetNull(1);

    std::stack<BaseVector*> args;
    args.push(inputVec);
    BaseVector* result = nullptr;
    FlinkToTimestampTestHelper::Execute({OMNI_VARCHAR}, args, result);

    ASSERT_NE(result, nullptr);
    EXPECT_FALSE(result->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(result->IsNull(1)) << "Row 1 should be NULL";
    auto* resultVec = dynamic_cast<Vector<int64_t>*>(result);
    EXPECT_EQ(resultVec->GetValue(0), 1505433600000LL);
    delete result;
}

TEST(FlinkToTimestampTest, OneArgFractionalSeconds) {
    // Flink 1-arg supports up to 9 fractional digits, floored to millis (precision 3).
    // 2017-09-15 00:00:00.123456789 -> millis 1505433600123.
    std::vector<std::string> inputs = {"2017-09-15 00:00:00.123456789"};
    std::vector<int64_t> expected = {1505433600123LL};

    BaseVector* inputVec = FlinkToTimestampTestHelper::CreateStringVector(inputs);
    std::stack<BaseVector*> args;
    args.push(inputVec);
    BaseVector* result = nullptr;
    FlinkToTimestampTestHelper::Execute({OMNI_VARCHAR}, args, result);
    FlinkToTimestampTestHelper::ValidateResult(result, expected, inputs.size());

    delete result;
}

TEST(FlinkToTimestampTest, OneArgPreEpoch) {
    // 1969-12-31 23:59:59 -> millis -1000 (pre-epoch, UTC wall-clock).
    std::vector<std::string> inputs = {"1969-12-31 23:59:59"};
    std::vector<int64_t> expected = {-1000LL};

    BaseVector* inputVec = FlinkToTimestampTestHelper::CreateStringVector(inputs);
    std::stack<BaseVector*> args;
    args.push(inputVec);
    BaseVector* result = nullptr;
    FlinkToTimestampTestHelper::Execute({OMNI_VARCHAR}, args, result);
    FlinkToTimestampTestHelper::ValidateResult(result, expected, inputs.size());

    delete result;
}

// ============================================================================
// 2-arg form: flink_to_timestamp(string, format) with explicit format
// ============================================================================

TEST(FlinkToTimestampTest, TwoArgExplicitFormat) {
    std::cout << "=== Test: flink_to_timestamp 2-arg explicit format ===" << std::endl;

    // Flink ScalarFunctionsTest:
    //   to_timestamp('20170915000000', 'yyyyMMddHHmmss') = 2017-09-15 00:00:00.000
    //   to_timestamp('2017-09-15', 'yyyy-MM-dd') = 2017-09-15 00:00:00.000
    std::vector<std::string> inputs = {
        "20170915000000",
        "2017-09-15",
        "2024-01-15 08:30:45"
    };
    std::vector<std::string> formats = {
        "yyyyMMddHHmmss",
        "yyyy-MM-dd",
        "yyyy-MM-dd HH:mm:ss"
    };
    std::vector<int64_t> expected = {1505433600000LL, 1505433600000LL, 1705307445000LL};

    BaseVector* inputVec = FlinkToTimestampTestHelper::CreateStringVector(inputs);
    BaseVector* formatVec = FlinkToTimestampTestHelper::CreateStringVector(formats);
    std::stack<BaseVector*> args;
    args.push(inputVec);
    args.push(formatVec);
    BaseVector* result = nullptr;
    FlinkToTimestampTestHelper::Execute({OMNI_VARCHAR, OMNI_VARCHAR}, args, result);
    FlinkToTimestampTestHelper::ValidateResult(result, expected, inputs.size());

    delete result;
}

TEST(FlinkToTimestampTest, TwoArgConstFormat) {
    // const format optimization: same format for all rows.
    std::vector<std::string> inputs = {
        "2017-09-15 00:00:00",
        "2024-01-15 08:30:45"
    };
    std::vector<int64_t> expected = {1505433600000LL, 1705307445000LL};

    BaseVector* inputVec = FlinkToTimestampTestHelper::CreateStringVector(inputs);
    BaseVector* formatVec = FlinkToTimestampTestHelper::CreateConstStringVector("yyyy-MM-dd HH:mm:ss", inputs.size());
    std::stack<BaseVector*> args;
    args.push(inputVec);
    args.push(formatVec);
    BaseVector* result = nullptr;
    FlinkToTimestampTestHelper::Execute({OMNI_VARCHAR, OMNI_VARCHAR}, args, result);
    FlinkToTimestampTestHelper::ValidateResult(result, expected, inputs.size());

    delete result;
}

TEST(FlinkToTimestampTest, TwoArgParseFailureReturnsNull) {
    // Format mismatch -> NULL.
    std::vector<std::string> inputs = {"abc", "2017-09-15", "20170915000000"};
    std::vector<std::string> formats = {"yyyy-MM-dd", "yyyyMMddHHmmss", "yyyy-MM-dd HH:mm:ss"};

    BaseVector* inputVec = FlinkToTimestampTestHelper::CreateStringVector(inputs);
    BaseVector* formatVec = FlinkToTimestampTestHelper::CreateStringVector(formats);
    std::stack<BaseVector*> args;
    args.push(inputVec);
    args.push(formatVec);
    BaseVector* result = nullptr;
    FlinkToTimestampTestHelper::Execute({OMNI_VARCHAR, OMNI_VARCHAR}, args, result);

    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(result->IsNull(0)) << "Row 0 ('abc' with yyyy-MM-dd) should be NULL";
    EXPECT_TRUE(result->IsNull(1)) << "Row 1 ('2017-09-15' with yyyyMMddHHmmss) should be NULL";
    EXPECT_TRUE(result->IsNull(2)) << "Row 2 ('20170915000000' with yyyy-MM-dd HH:mm:ss) should be NULL";
    delete result;
}

TEST(FlinkToTimestampTest, TwoArgNullInput) {
    // input NULL or format NULL -> NULL.
    std::vector<std::string> inputs = {"2017-09-15 00:00:00", "2024-01-15 08:30:45"};
    std::vector<std::string> formats = {"yyyy-MM-dd HH:mm:ss", "yyyy-MM-dd HH:mm:ss"};

    BaseVector* inputVec = FlinkToTimestampTestHelper::CreateStringVector(inputs);
    inputVec->SetNull(1);
    BaseVector* formatVec = FlinkToTimestampTestHelper::CreateStringVector(formats);

    std::stack<BaseVector*> args;
    args.push(inputVec);
    args.push(formatVec);
    BaseVector* result = nullptr;
    FlinkToTimestampTestHelper::Execute({OMNI_VARCHAR, OMNI_VARCHAR}, args, result);

    ASSERT_NE(result, nullptr);
    EXPECT_FALSE(result->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(result->IsNull(1)) << "Row 1 (NULL input) should be NULL";
    auto* resultVec = dynamic_cast<Vector<int64_t>*>(result);
    EXPECT_EQ(resultVec->GetValue(0), 1505433600000LL);
    delete result;
}

TEST(FlinkToTimestampTest, TwoArgDateOnlyFormat) {
    // Flink ScalarFunctionsTest: to_timestamp('2017-09-15', 'yyyy-MM-dd') ->
    // 2017-09-15 00:00:00.000 (missing time fields default to 0).
    std::vector<std::string> inputs = {"2017-09-15"};
    std::vector<int64_t> expected = {1505433600000LL};

    BaseVector* inputVec = FlinkToTimestampTestHelper::CreateStringVector(inputs);
    BaseVector* formatVec = FlinkToTimestampTestHelper::CreateConstStringVector("yyyy-MM-dd", inputs.size());
    std::stack<BaseVector*> args;
    args.push(inputVec);
    args.push(formatVec);
    BaseVector* result = nullptr;
    FlinkToTimestampTestHelper::Execute({OMNI_VARCHAR, OMNI_VARCHAR}, args, result);
    FlinkToTimestampTestHelper::ValidateResult(result, expected, inputs.size());

    delete result;
}

// ============================================================================
// Multi-row batch mixing valid/invalid/null rows
// ============================================================================

TEST(FlinkToTimestampTest, MultiRowBatchMixed) {
    std::vector<std::string> inputs = {
        "2017-09-15 00:00:00",   // valid -> 1505433600000
        "abc",                   // invalid -> NULL
        "2024-01-15 08:30:45",   // valid -> 1705307445000
        "1969-12-31 23:59:59"    // valid pre-epoch -> -1000
    };
    std::vector<int64_t> expected = {1505433600000LL, 0, 1705307445000LL, -1000LL};

    BaseVector* inputVec = FlinkToTimestampTestHelper::CreateStringVector(inputs);
    std::stack<BaseVector*> args;
    args.push(inputVec);
    BaseVector* result = nullptr;
    FlinkToTimestampTestHelper::Execute({OMNI_VARCHAR}, args, result);

    ASSERT_NE(result, nullptr);
    EXPECT_FALSE(result->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(result->IsNull(1)) << "Row 1 ('abc') should be NULL";
    EXPECT_FALSE(result->IsNull(2)) << "Row 2 should not be NULL";
    EXPECT_FALSE(result->IsNull(3)) << "Row 3 should not be NULL";
    auto* resultVec = dynamic_cast<Vector<int64_t>*>(result);
    EXPECT_EQ(resultVec->GetValue(0), 1505433600000LL);
    EXPECT_EQ(resultVec->GetValue(2), 1705307445000LL);
    EXPECT_EQ(resultVec->GetValue(3), -1000LL);
    delete result;
}
