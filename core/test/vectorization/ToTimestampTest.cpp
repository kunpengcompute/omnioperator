/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: ToTimestamp and ToUnixTimestamp functions unit tests
 */

#include <gtest/gtest.h>
#include <vector>
#include <cmath>
#include <limits>
#include <string>
#include <string_view>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/functions/ToTimestamp.h"
#include "vectorization/VectorFunction.h"
#include "codegen/func_signature.h"
#include "vector/vector_helper.h"
#include "vector/vector.h"
#include "type/Timestamp.h"
#include "util/type_util.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::codegen;
using namespace omniruntime::TestUtil;

class ToTimestampTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const to_timestamp_test_env =
    ::testing::AddGlobalTestEnvironment(new ToTimestampTestEnvironment);

class ToTimestampTestHelper {
public:
    static BaseVector* CreateStringVector(const std::vector<std::string>& values) {
        auto* vec = new Vector<LargeStringContainer<std::string_view>>(values.size());
        for (size_t i = 0; i < values.size(); ++i) {
            std::string_view sv(values[i].data(), values[i].size());
            vec->SetValue(i, sv);
        }
        return vec;
    }

    static BaseVector* CreateConstStringVector(const std::string& value, int32_t size) {
        auto* vec = new Vector<LargeStringContainer<std::string_view>>(size);
        std::string_view sv(value.data(), value.size());
        for (int32_t i = 0; i < size; ++i) {
            vec->SetValue(i, sv);
        }
        return vec;
    }

    static BaseVector* CreateInt64Vector(const std::vector<int64_t>& values) {
        BaseVector* vec = VectorHelper::CreateFlatVector(nullptr, OMNI_LONG, values.size());
        auto* typedVec = static_cast<Vector<int64_t>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typedVec->SetValue(i, values[i]);
        }
        return vec;
    }

    static BaseVector* CreateTimestampVector(const std::vector<int64_t>& values) {
        BaseVector* vec = VectorHelper::CreateFlatVector(nullptr, OMNI_TIMESTAMP, values.size());
        auto* typedVec = static_cast<Vector<int64_t>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typedVec->SetValue(i, values[i]);
        }
        return vec;
    }

    static BaseVector* CreateInt32Vector(const std::vector<int32_t>& values) {
        BaseVector* vec = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, values.size());
        auto* typedVec = static_cast<Vector<int32_t>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typedVec->SetValue(i, values[i]);
        }
        return vec;
    }

    static BaseVector* CreateDate32Vector(const std::vector<int32_t>& values) {
        BaseVector* vec = VectorHelper::CreateFlatVector(nullptr, OMNI_DATE32, values.size());
        auto* typedVec = static_cast<Vector<int32_t>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typedVec->SetValue(i, values[i]);
        }
        return vec;
    }

    static void ExecuteFunction(const std::string& funcName,
        const std::vector<DataTypeId>& inputTypes, DataTypeId outputType,
        std::stack<BaseVector*>& args, BaseVector*& result) {
        auto signature = std::make_shared<FunctionSignature>(funcName,
            inputTypes, outputType);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << funcName << " function not found for signature";

        auto outType = (outputType == OMNI_TIMESTAMP) ?
            TimestampType() : LongType();
        ExecutionContext context(4096, nullptr);
        int32_t sz = 0;
        if (!args.empty()) {
            std::stack<BaseVector*> tmpArgs = args;
            while (!tmpArgs.empty()) {
                sz = tmpArgs.top()->GetSize();
                tmpArgs.pop();
            }
        }
        context.SetResultRowSize(sz);

        ASSERT_NO_THROW(function->Apply(args, outType, result, &context))
            << funcName << " function threw an exception";
    }
};

// ========== get_timestamp (ToTimestamp) Tests ==========

TEST(ToTimestampTest, GetTimestamp_BasicDateFormat) {
    std::vector<std::string> inputs = {"1970-01-01", "2023-12-08", "2024-03-10"};
    BaseVector* inputVec = ToTimestampTestHelper::CreateStringVector(inputs);
    BaseVector* formatVec = ToTimestampTestHelper::CreateConstStringVector("yyyy-MM-dd", inputs.size());

    std::stack<BaseVector*> args;
    args.push(inputVec);
    args.push(formatVec);

    BaseVector* resultVec = nullptr;
    ToTimestampTestHelper::ExecuteFunction("get_timestamp",
        {OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_TIMESTAMP, args, resultVec);

    ASSERT_NE(resultVec, nullptr);
    auto* resultTyped = dynamic_cast<Vector<int64_t>*>(resultVec);
    ASSERT_NE(resultTyped, nullptr);

    EXPECT_FALSE(resultVec->IsNull(0));
    int64_t ts0 = resultTyped->GetValue(0);
    EXPECT_EQ(ts0, 0LL);

    EXPECT_FALSE(resultVec->IsNull(1));
    EXPECT_FALSE(resultVec->IsNull(2));

    delete resultVec;
}

TEST(ToTimestampTest, GetTimestamp_DateTimeFormat) {
    std::vector<std::string> inputs = {"1970-01-01 00:00:00", "2023-12-08 08:20:19"};
    BaseVector* inputVec = ToTimestampTestHelper::CreateStringVector(inputs);
    BaseVector* formatVec = ToTimestampTestHelper::CreateConstStringVector(
        "yyyy-MM-dd HH:mm:ss", inputs.size());

    std::stack<BaseVector*> args;
    args.push(inputVec);
    args.push(formatVec);

    BaseVector* resultVec = nullptr;
    ToTimestampTestHelper::ExecuteFunction("get_timestamp",
        {OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_TIMESTAMP, args, resultVec);

    ASSERT_NE(resultVec, nullptr);
    auto* resultTyped = dynamic_cast<Vector<int64_t>*>(resultVec);
    ASSERT_NE(resultTyped, nullptr);

    EXPECT_FALSE(resultVec->IsNull(0));
    EXPECT_EQ(resultTyped->GetValue(0), 0LL);

    EXPECT_FALSE(resultVec->IsNull(1));
    int64_t ts1 = resultTyped->GetValue(1);
    Timestamp timestamp1 = Timestamp::fromMicros(ts1);
    EXPECT_EQ(timestamp1.getSeconds(), 1702023619LL);

    delete resultVec;
}

TEST(ToTimestampTest, GetTimestamp_WithMilliseconds) {
    std::vector<std::string> inputs = {"1970-01-01 00:00:00.010", "1970-01-01 00:00:00.200"};
    BaseVector* inputVec = ToTimestampTestHelper::CreateStringVector(inputs);
    BaseVector* formatVec = ToTimestampTestHelper::CreateConstStringVector(
        "yyyy-MM-dd HH:mm:ss.SSS", inputs.size());

    std::stack<BaseVector*> args;
    args.push(inputVec);
    args.push(formatVec);

    BaseVector* resultVec = nullptr;
    ToTimestampTestHelper::ExecuteFunction("get_timestamp",
        {OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_TIMESTAMP, args, resultVec);

    ASSERT_NE(resultVec, nullptr);
    auto* resultTyped = dynamic_cast<Vector<int64_t>*>(resultVec);
    ASSERT_NE(resultTyped, nullptr);

    EXPECT_FALSE(resultVec->IsNull(0));
    EXPECT_EQ(resultTyped->GetValue(0), 10000LL);

    EXPECT_FALSE(resultVec->IsNull(1));
    EXPECT_EQ(resultTyped->GetValue(1), 200000LL);

    delete resultVec;
}

TEST(ToTimestampTest, GetTimestamp_SlashFormat) {
    std::vector<std::string> inputs = {"1970/01/01", "08/27/2017"};

    BaseVector* inputVec1 = ToTimestampTestHelper::CreateStringVector({inputs[0]});
    BaseVector* formatVec1 = ToTimestampTestHelper::CreateConstStringVector("yyyy/MM/dd", 1);

    std::stack<BaseVector*> args1;
    args1.push(inputVec1);
    args1.push(formatVec1);

    BaseVector* resultVec1 = nullptr;
    ToTimestampTestHelper::ExecuteFunction("get_timestamp",
        {OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_TIMESTAMP, args1, resultVec1);

    ASSERT_NE(resultVec1, nullptr);
    EXPECT_FALSE(resultVec1->IsNull(0));
    auto* resultTyped1 = dynamic_cast<Vector<int64_t>*>(resultVec1);
    EXPECT_EQ(resultTyped1->GetValue(0), 0LL);

    delete resultVec1;

    BaseVector* inputVec2 = ToTimestampTestHelper::CreateStringVector({inputs[1]});
    BaseVector* formatVec2 = ToTimestampTestHelper::CreateConstStringVector("MM/dd/yyy", 1);

    std::stack<BaseVector*> args2;
    args2.push(inputVec2);
    args2.push(formatVec2);

    BaseVector* resultVec2 = nullptr;
    ToTimestampTestHelper::ExecuteFunction("get_timestamp",
        {OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_TIMESTAMP, args2, resultVec2);

    ASSERT_NE(resultVec2, nullptr);
    EXPECT_FALSE(resultVec2->IsNull(0));

    delete resultVec2;
}

TEST(ToTimestampTest, GetTimestamp_NullInput) {
    std::vector<std::string> inputs = {"1970-01-01", ""};
    BaseVector* inputVec = ToTimestampTestHelper::CreateStringVector(inputs);
    inputVec->SetNull(1);

    BaseVector* formatVec = ToTimestampTestHelper::CreateConstStringVector("yyyy-MM-dd", inputs.size());

    std::stack<BaseVector*> args;
    args.push(inputVec);
    args.push(formatVec);

    BaseVector* resultVec = nullptr;
    ToTimestampTestHelper::ExecuteFunction("get_timestamp",
        {OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_TIMESTAMP, args, resultVec);

    ASSERT_NE(resultVec, nullptr);
    EXPECT_FALSE(resultVec->IsNull(0));
    EXPECT_TRUE(resultVec->IsNull(1));

    delete resultVec;
}

TEST(ToTimestampTest, GetTimestamp_ParseError) {
    std::vector<std::string> inputs = {
        "1970-01-01 06:10:59.019",
        "not-a-date"
    };
    BaseVector* inputVec = ToTimestampTestHelper::CreateStringVector(inputs);
    BaseVector* formatVec = ToTimestampTestHelper::CreateConstStringVector("HH:mm:ss.SSS", inputs.size());

    std::stack<BaseVector*> args;
    args.push(inputVec);
    args.push(formatVec);

    BaseVector* resultVec = nullptr;
    ToTimestampTestHelper::ExecuteFunction("get_timestamp",
        {OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_TIMESTAMP, args, resultVec);

    ASSERT_NE(resultVec, nullptr);
    EXPECT_TRUE(resultVec->IsNull(0));
    EXPECT_TRUE(resultVec->IsNull(1));

    delete resultVec;
}

TEST(ToTimestampTest, GetTimestamp_EmptyString) {
    std::vector<std::string> inputs = {"", ""};
    BaseVector* inputVec = ToTimestampTestHelper::CreateStringVector(inputs);
    BaseVector* formatVec = ToTimestampTestHelper::CreateConstStringVector("yyyy-MM-dd", inputs.size());

    std::stack<BaseVector*> args;
    args.push(inputVec);
    args.push(formatVec);

    BaseVector* resultVec = nullptr;
    ToTimestampTestHelper::ExecuteFunction("get_timestamp",
        {OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_TIMESTAMP, args, resultVec);

    ASSERT_NE(resultVec, nullptr);
    EXPECT_TRUE(resultVec->IsNull(0));
    EXPECT_TRUE(resultVec->IsNull(1));

    delete resultVec;
}

TEST(ToTimestampTest, GetTimestamp_DateTimeWithSlash) {
    std::vector<std::string> inputs = {"1970/01/01 12:08:59", "2023/12/08 08:20:19"};
    BaseVector* inputVec = ToTimestampTestHelper::CreateStringVector(inputs);
    BaseVector* formatVec = ToTimestampTestHelper::CreateConstStringVector(
        "yyyy/MM/dd HH:mm:ss", inputs.size());

    std::stack<BaseVector*> args;
    args.push(inputVec);
    args.push(formatVec);

    BaseVector* resultVec = nullptr;
    ToTimestampTestHelper::ExecuteFunction("get_timestamp",
        {OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_TIMESTAMP, args, resultVec);

    ASSERT_NE(resultVec, nullptr);
    EXPECT_FALSE(resultVec->IsNull(0));
    EXPECT_FALSE(resultVec->IsNull(1));

    auto* resultTyped = dynamic_cast<Vector<int64_t>*>(resultVec);
    int64_t ts0 = resultTyped->GetValue(0);
    Timestamp timestamp0 = Timestamp::fromMicros(ts0);
    EXPECT_EQ(timestamp0.getSeconds(), 43739LL);

    delete resultVec;
}

// ========== to_unix_timestamp Tests ==========

TEST(ToTimestampTest, ToUnixTimestamp_StringDefaultFormat) {
    std::vector<std::string> inputs = {
        "1970-01-01 00:00:00",
        "1970-01-01 00:00:01",
        "1970-01-01 00:01:01"
    };
    BaseVector* inputVec = ToTimestampTestHelper::CreateStringVector(inputs);

    std::stack<BaseVector*> args;
    args.push(inputVec);

    BaseVector* resultVec = nullptr;
    ToTimestampTestHelper::ExecuteFunction("to_unix_timestamp",
        {OMNI_VARCHAR}, OMNI_LONG, args, resultVec);

    ASSERT_NE(resultVec, nullptr);
    auto* resultTyped = dynamic_cast<Vector<int64_t>*>(resultVec);
    ASSERT_NE(resultTyped, nullptr);

    EXPECT_FALSE(resultVec->IsNull(0));
    EXPECT_EQ(resultTyped->GetValue(0), 0LL);

    EXPECT_FALSE(resultVec->IsNull(1));
    EXPECT_EQ(resultTyped->GetValue(1), 1LL);

    EXPECT_FALSE(resultVec->IsNull(2));
    EXPECT_EQ(resultTyped->GetValue(2), 61LL);

    delete resultVec;
}

TEST(ToTimestampTest, ToUnixTimestamp_StringCustomFormat) {
    std::vector<std::string> inputs = {"1970-01-01", "1970-01-02"};
    BaseVector* inputVec = ToTimestampTestHelper::CreateStringVector(inputs);
    BaseVector* formatVec = ToTimestampTestHelper::CreateConstStringVector("yyyy-MM-dd", inputs.size());

    std::stack<BaseVector*> args;
    args.push(inputVec);
    args.push(formatVec);

    BaseVector* resultVec = nullptr;
    ToTimestampTestHelper::ExecuteFunction("to_unix_timestamp",
        {OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_LONG, args, resultVec);

    ASSERT_NE(resultVec, nullptr);
    auto* resultTyped = dynamic_cast<Vector<int64_t>*>(resultVec);
    ASSERT_NE(resultTyped, nullptr);

    EXPECT_FALSE(resultVec->IsNull(0));
    EXPECT_EQ(resultTyped->GetValue(0), 0LL);

    EXPECT_FALSE(resultVec->IsNull(1));
    EXPECT_EQ(resultTyped->GetValue(1), 86400LL);

    delete resultVec;
}

TEST(ToTimestampTest, ToUnixTimestamp_StringNullInput) {
    std::vector<std::string> inputs = {"1970-01-01 00:00:00", ""};
    BaseVector* inputVec = ToTimestampTestHelper::CreateStringVector(inputs);
    inputVec->SetNull(1);

    std::stack<BaseVector*> args;
    args.push(inputVec);

    BaseVector* resultVec = nullptr;
    ToTimestampTestHelper::ExecuteFunction("to_unix_timestamp",
        {OMNI_VARCHAR}, OMNI_LONG, args, resultVec);

    ASSERT_NE(resultVec, nullptr);
    EXPECT_FALSE(resultVec->IsNull(0));
    EXPECT_TRUE(resultVec->IsNull(1));

    delete resultVec;
}

TEST(ToTimestampTest, ToUnixTimestamp_StringMalformed) {
    std::vector<std::string> inputs = {"1970-01-01", "malformed input", ""};
    BaseVector* inputVec = ToTimestampTestHelper::CreateStringVector(inputs);

    std::stack<BaseVector*> args;
    args.push(inputVec);

    BaseVector* resultVec = nullptr;
    ToTimestampTestHelper::ExecuteFunction("to_unix_timestamp",
        {OMNI_VARCHAR}, OMNI_LONG, args, resultVec);

    ASSERT_NE(resultVec, nullptr);
    EXPECT_TRUE(resultVec->IsNull(0));
    EXPECT_TRUE(resultVec->IsNull(1));
    EXPECT_TRUE(resultVec->IsNull(2));

    delete resultVec;
}

TEST(ToTimestampTest, ToUnixTimestamp_TimestampInput) {
    std::vector<int64_t> inputValues = {0LL, 1000000LL, 61000000LL, -1000000LL};
    BaseVector* inputVec = ToTimestampTestHelper::CreateTimestampVector(inputValues);

    std::stack<BaseVector*> args;
    args.push(inputVec);

    BaseVector* resultVec = nullptr;
    ToTimestampTestHelper::ExecuteFunction("to_unix_timestamp",
        {OMNI_TIMESTAMP}, OMNI_LONG, args, resultVec);

    ASSERT_NE(resultVec, nullptr);
    auto* resultTyped = dynamic_cast<Vector<int64_t>*>(resultVec);
    ASSERT_NE(resultTyped, nullptr);

    EXPECT_FALSE(resultVec->IsNull(0));
    EXPECT_EQ(resultTyped->GetValue(0), 0LL);

    EXPECT_FALSE(resultVec->IsNull(1));
    EXPECT_EQ(resultTyped->GetValue(1), 1LL);

    EXPECT_FALSE(resultVec->IsNull(2));
    EXPECT_EQ(resultTyped->GetValue(2), 61LL);

    EXPECT_FALSE(resultVec->IsNull(3));
    EXPECT_EQ(resultTyped->GetValue(3), -1LL);

    delete resultVec;
}

TEST(ToTimestampTest, ToUnixTimestamp_LongInput) {
    std::vector<int64_t> inputValues = {0LL, 1000000LL, 1739933174000000LL};
    BaseVector* inputVec = ToTimestampTestHelper::CreateInt64Vector(inputValues);

    std::stack<BaseVector*> args;
    args.push(inputVec);

    BaseVector* resultVec = nullptr;
    ToTimestampTestHelper::ExecuteFunction("to_unix_timestamp",
        {OMNI_LONG}, OMNI_LONG, args, resultVec);

    ASSERT_NE(resultVec, nullptr);
    auto* resultTyped = dynamic_cast<Vector<int64_t>*>(resultVec);
    ASSERT_NE(resultTyped, nullptr);

    EXPECT_FALSE(resultVec->IsNull(0));
    EXPECT_EQ(resultTyped->GetValue(0), 0LL);

    EXPECT_FALSE(resultVec->IsNull(1));
    EXPECT_EQ(resultTyped->GetValue(1), 1LL);

    EXPECT_FALSE(resultVec->IsNull(2));
    EXPECT_EQ(resultTyped->GetValue(2), 1739933174LL);

    delete resultVec;
}

TEST(ToTimestampTest, ToUnixTimestamp_TimestampNullInput) {
    std::vector<int64_t> inputValues = {1000000LL, 0LL};
    BaseVector* inputVec = ToTimestampTestHelper::CreateTimestampVector(inputValues);
    inputVec->SetNull(1);

    std::stack<BaseVector*> args;
    args.push(inputVec);

    BaseVector* resultVec = nullptr;
    ToTimestampTestHelper::ExecuteFunction("to_unix_timestamp",
        {OMNI_TIMESTAMP}, OMNI_LONG, args, resultVec);

    ASSERT_NE(resultVec, nullptr);
    EXPECT_FALSE(resultVec->IsNull(0));
    EXPECT_TRUE(resultVec->IsNull(1));

    delete resultVec;
}

TEST(ToTimestampTest, ToUnixTimestamp_DateInput) {
    std::vector<int32_t> inputValues = {0, 1, 19997};
    BaseVector* inputVec = ToTimestampTestHelper::CreateDate32Vector(inputValues);

    std::stack<BaseVector*> args;
    args.push(inputVec);

    BaseVector* resultVec = nullptr;
    ToTimestampTestHelper::ExecuteFunction("to_unix_timestamp",
        {OMNI_DATE32}, OMNI_LONG, args, resultVec);

    ASSERT_NE(resultVec, nullptr);
    auto* resultTyped = dynamic_cast<Vector<int64_t>*>(resultVec);
    ASSERT_NE(resultTyped, nullptr);

    EXPECT_FALSE(resultVec->IsNull(0));
    EXPECT_EQ(resultTyped->GetValue(0), 0LL);

    EXPECT_FALSE(resultVec->IsNull(1));
    EXPECT_EQ(resultTyped->GetValue(1), 86400LL);

    EXPECT_FALSE(resultVec->IsNull(2));
    EXPECT_EQ(resultTyped->GetValue(2), static_cast<int64_t>(19997) * 86400LL);

    delete resultVec;
}

TEST(ToTimestampTest, ToUnixTimestamp_IntDateInput) {
    std::vector<int32_t> inputValues = {0, 1};
    BaseVector* inputVec = ToTimestampTestHelper::CreateInt32Vector(inputValues);

    std::stack<BaseVector*> args;
    args.push(inputVec);

    BaseVector* resultVec = nullptr;
    ToTimestampTestHelper::ExecuteFunction("to_unix_timestamp",
        {OMNI_INT}, OMNI_LONG, args, resultVec);

    ASSERT_NE(resultVec, nullptr);
    auto* resultTyped = dynamic_cast<Vector<int64_t>*>(resultVec);
    ASSERT_NE(resultTyped, nullptr);

    EXPECT_FALSE(resultVec->IsNull(0));
    EXPECT_EQ(resultTyped->GetValue(0), 0LL);

    EXPECT_FALSE(resultVec->IsNull(1));
    EXPECT_EQ(resultTyped->GetValue(1), 86400LL);

    delete resultVec;
}

TEST(ToTimestampTest, ToUnixTimestamp_DateNullInput) {
    std::vector<int32_t> inputValues = {0, 0};
    BaseVector* inputVec = ToTimestampTestHelper::CreateDate32Vector(inputValues);
    inputVec->SetNull(1);

    std::stack<BaseVector*> args;
    args.push(inputVec);

    BaseVector* resultVec = nullptr;
    ToTimestampTestHelper::ExecuteFunction("to_unix_timestamp",
        {OMNI_DATE32}, OMNI_LONG, args, resultVec);

    ASSERT_NE(resultVec, nullptr);
    EXPECT_FALSE(resultVec->IsNull(0));
    EXPECT_TRUE(resultVec->IsNull(1));

    delete resultVec;
}

TEST(ToTimestampTest, ToUnixTimestamp_StringWithTimeFormat) {
    std::vector<std::string> inputs = {
        "1970-01-02 00:00:10"
    };
    BaseVector* inputVec = ToTimestampTestHelper::CreateStringVector(inputs);
    BaseVector* formatVec = ToTimestampTestHelper::CreateConstStringVector(
        "yyyy-MM-dd HH:mm:ss", inputs.size());

    std::stack<BaseVector*> args;
    args.push(inputVec);
    args.push(formatVec);

    BaseVector* resultVec = nullptr;
    ToTimestampTestHelper::ExecuteFunction("to_unix_timestamp",
        {OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_LONG, args, resultVec);

    ASSERT_NE(resultVec, nullptr);
    auto* resultTyped = dynamic_cast<Vector<int64_t>*>(resultVec);
    ASSERT_NE(resultTyped, nullptr);

    EXPECT_FALSE(resultVec->IsNull(0));
    EXPECT_EQ(resultTyped->GetValue(0), 86410LL);

    delete resultVec;
}

// ========== Edge Cases ==========

TEST(ToTimestampTest, GetTimestamp_LargeDate) {
    std::vector<std::string> inputs = {"2045-12-31"};
    BaseVector* inputVec = ToTimestampTestHelper::CreateStringVector(inputs);
    BaseVector* formatVec = ToTimestampTestHelper::CreateConstStringVector("yyyy-MM-dd", inputs.size());

    std::stack<BaseVector*> args;
    args.push(inputVec);
    args.push(formatVec);

    BaseVector* resultVec = nullptr;
    ToTimestampTestHelper::ExecuteFunction("get_timestamp",
        {OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_TIMESTAMP, args, resultVec);

    ASSERT_NE(resultVec, nullptr);
    EXPECT_FALSE(resultVec->IsNull(0));

    auto* resultTyped = dynamic_cast<Vector<int64_t>*>(resultVec);
    int64_t ts0 = resultTyped->GetValue(0);
    Timestamp timestamp0 = Timestamp::fromMicros(ts0);
    EXPECT_GT(timestamp0.getSeconds(), 0);

    delete resultVec;
}

TEST(ToTimestampTest, GetTimestamp_YearOnlyFormat) {
    std::vector<std::string> inputs = {"1970"};
    BaseVector* inputVec = ToTimestampTestHelper::CreateStringVector(inputs);
    BaseVector* formatVec = ToTimestampTestHelper::CreateConstStringVector("yyyy", inputs.size());

    std::stack<BaseVector*> args;
    args.push(inputVec);
    args.push(formatVec);

    BaseVector* resultVec = nullptr;
    ToTimestampTestHelper::ExecuteFunction("get_timestamp",
        {OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_TIMESTAMP, args, resultVec);

    ASSERT_NE(resultVec, nullptr);
    EXPECT_FALSE(resultVec->IsNull(0));

    auto* resultTyped = dynamic_cast<Vector<int64_t>*>(resultVec);
    int64_t ts0 = resultTyped->GetValue(0);
    EXPECT_EQ(ts0, 0LL);

    delete resultVec;
}

TEST(ToTimestampTest, ToUnixTimestamp_NegativeDate) {
    std::vector<int32_t> inputValues = {-1};
    BaseVector* inputVec = ToTimestampTestHelper::CreateDate32Vector(inputValues);

    std::stack<BaseVector*> args;
    args.push(inputVec);

    BaseVector* resultVec = nullptr;
    ToTimestampTestHelper::ExecuteFunction("to_unix_timestamp",
        {OMNI_DATE32}, OMNI_LONG, args, resultVec);

    ASSERT_NE(resultVec, nullptr);
    auto* resultTyped = dynamic_cast<Vector<int64_t>*>(resultVec);
    ASSERT_NE(resultTyped, nullptr);

    EXPECT_FALSE(resultVec->IsNull(0));
    EXPECT_EQ(resultTyped->GetValue(0), -86400LL);

    delete resultVec;
}

TEST(ToTimestampTest, ToUnixTimestamp_NegativeTimestamp) {
    std::vector<int64_t> inputValues = {-1000000LL};
    BaseVector* inputVec = ToTimestampTestHelper::CreateTimestampVector(inputValues);

    std::stack<BaseVector*> args;
    args.push(inputVec);

    BaseVector* resultVec = nullptr;
    ToTimestampTestHelper::ExecuteFunction("to_unix_timestamp",
        {OMNI_TIMESTAMP}, OMNI_LONG, args, resultVec);

    ASSERT_NE(resultVec, nullptr);
    auto* resultTyped = dynamic_cast<Vector<int64_t>*>(resultVec);
    ASSERT_NE(resultTyped, nullptr);

    EXPECT_FALSE(resultVec->IsNull(0));
    EXPECT_EQ(resultTyped->GetValue(0), -1LL);

    delete resultVec;
}
