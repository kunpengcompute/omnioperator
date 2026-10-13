/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: ToUtcTimestamp and FromUtcTimestamp functions unit tests
 */

#include <gtest/gtest.h>
#include <vector>
#include <string>
#include <string_view>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/functions/ToUtcTimestamp.h"
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

class ToUtcTimestampTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const to_utc_timestamp_test_env =
    ::testing::AddGlobalTestEnvironment(new ToUtcTimestampTestEnvironment);

class UtcTimestampTestHelper {
public:
    static BaseVector* CreateTimestampVector(const std::vector<int64_t>& values) {
        BaseVector* vec = VectorHelper::CreateFlatVector(nullptr, OMNI_TIMESTAMP, values.size());
        auto* typedVec = static_cast<Vector<int64_t>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typedVec->SetValue(i, values[i]);
        }
        return vec;
    }

    static BaseVector* CreateLongVector(const std::vector<int64_t>& values) {
        BaseVector* vec = VectorHelper::CreateFlatVector(nullptr, OMNI_LONG, values.size());
        auto* typedVec = static_cast<Vector<int64_t>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typedVec->SetValue(i, values[i]);
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

    static BaseVector* CreateStringVector(const std::vector<std::string>& values) {
        auto* vec = new Vector<LargeStringContainer<std::string_view>>(values.size());
        for (size_t i = 0; i < values.size(); ++i) {
            std::string_view sv(values[i].data(), values[i].size());
            vec->SetValue(i, sv);
        }
        return vec;
    }

    static int64_t TimestampMicros(int64_t seconds, uint64_t nanos = 0) {
        Timestamp ts(seconds, nanos);
        return ts.toMicros();
    }

    static void ExecuteFunction(const std::string& funcName,
        const std::vector<DataTypeId>& inputTypes,
        std::stack<BaseVector*>& args, BaseVector*& result) {
        auto signature = std::make_shared<FunctionSignature>(funcName,
            inputTypes, OMNI_TIMESTAMP);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << funcName << " function not found for signature";

        auto outType = TimestampType();
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

        try {
            function->Apply(args, outType, result, &context);
        } catch (const std::exception& e) {
            FAIL() << funcName << " function threw an exception: " << e.what();
        } catch (...) {
            FAIL() << funcName << " function threw an unknown exception";
        }
    }
};

// ========== to_utc_timestamp Tests ==========

TEST(ToUtcTimestampTest, BasicLosAngeles) {
    // 2015-07-24 00:00:00 in America/Los_Angeles is 2015-07-24 07:00:00 UTC (PDT, -7h)
    int64_t inputMicros = UtcTimestampTestHelper::TimestampMicros(
        1437696000LL);  // 2015-07-24 00:00:00 local as if UTC
    std::vector<int64_t> inputs = {inputMicros};
    BaseVector* tsVec = UtcTimestampTestHelper::CreateTimestampVector(inputs);
    BaseVector* tzVec = UtcTimestampTestHelper::CreateConstStringVector("America/Los_Angeles", 1);

    std::stack<BaseVector*> args;
    args.push(tsVec);
    args.push(tzVec);

    BaseVector* resultVec = nullptr;
    UtcTimestampTestHelper::ExecuteFunction("to_utc_timestamp",
        {OMNI_TIMESTAMP, OMNI_VARCHAR}, args, resultVec);

    ASSERT_NE(resultVec, nullptr);
    EXPECT_FALSE(resultVec->IsNull(0));

    auto* resultTyped = dynamic_cast<Vector<int64_t>*>(resultVec);
    ASSERT_NE(resultTyped, nullptr);
    int64_t resultMicros = resultTyped->GetValue(0);
    Timestamp resultTs = Timestamp::fromMicros(resultMicros);
    EXPECT_EQ(resultTs.getSeconds(), 1437696000LL + 7 * 3600);

    delete resultVec;
}

TEST(ToUtcTimestampTest, BasicLosAngelesWinter) {
    // 2015-01-24 00:00:00 in America/Los_Angeles is 2015-01-24 08:00:00 UTC (PST, -8h)
    int64_t inputMicros = UtcTimestampTestHelper::TimestampMicros(
        1422057600LL);  // 2015-01-24 00:00:00 local as if UTC
    std::vector<int64_t> inputs = {inputMicros};
    BaseVector* tsVec = UtcTimestampTestHelper::CreateTimestampVector(inputs);
    BaseVector* tzVec = UtcTimestampTestHelper::CreateConstStringVector("America/Los_Angeles", 1);

    std::stack<BaseVector*> args;
    args.push(tsVec);
    args.push(tzVec);

    BaseVector* resultVec = nullptr;
    UtcTimestampTestHelper::ExecuteFunction("to_utc_timestamp",
        {OMNI_TIMESTAMP, OMNI_VARCHAR}, args, resultVec);

    ASSERT_NE(resultVec, nullptr);
    EXPECT_FALSE(resultVec->IsNull(0));

    auto* resultTyped = dynamic_cast<Vector<int64_t>*>(resultVec);
    int64_t resultMicros = resultTyped->GetValue(0);
    Timestamp resultTs = Timestamp::fromMicros(resultMicros);
    EXPECT_EQ(resultTs.getSeconds(), 1422057600LL + 8 * 3600);

    delete resultVec;
}

TEST(ToUtcTimestampTest, UTC) {
    // UTC to UTC should be identity
    int64_t inputMicros = UtcTimestampTestHelper::TimestampMicros(1422057600LL);
    std::vector<int64_t> inputs = {inputMicros};
    BaseVector* tsVec = UtcTimestampTestHelper::CreateTimestampVector(inputs);
    BaseVector* tzVec = UtcTimestampTestHelper::CreateConstStringVector("UTC", 1);

    std::stack<BaseVector*> args;
    args.push(tsVec);
    args.push(tzVec);

    BaseVector* resultVec = nullptr;
    UtcTimestampTestHelper::ExecuteFunction("to_utc_timestamp",
        {OMNI_TIMESTAMP, OMNI_VARCHAR}, args, resultVec);

    ASSERT_NE(resultVec, nullptr);
    auto* resultTyped = dynamic_cast<Vector<int64_t>*>(resultVec);
    EXPECT_EQ(resultTyped->GetValue(0), inputMicros);

    delete resultVec;
}

TEST(ToUtcTimestampTest, AsiaKolkata) {
    // 2015-01-24 05:30:00 in Asia/Kolkata is 2015-01-24 00:00:00 UTC (+5:30)
    int64_t inputMicros = UtcTimestampTestHelper::TimestampMicros(
        1422057600LL + 5 * 3600 + 30 * 60);
    std::vector<int64_t> inputs = {inputMicros};
    BaseVector* tsVec = UtcTimestampTestHelper::CreateTimestampVector(inputs);
    BaseVector* tzVec = UtcTimestampTestHelper::CreateConstStringVector("Asia/Kolkata", 1);

    std::stack<BaseVector*> args;
    args.push(tsVec);
    args.push(tzVec);

    BaseVector* resultVec = nullptr;
    UtcTimestampTestHelper::ExecuteFunction("to_utc_timestamp",
        {OMNI_TIMESTAMP, OMNI_VARCHAR}, args, resultVec);

    ASSERT_NE(resultVec, nullptr);
    auto* resultTyped = dynamic_cast<Vector<int64_t>*>(resultVec);
    int64_t resultMicros = resultTyped->GetValue(0);
    Timestamp resultTs = Timestamp::fromMicros(resultMicros);
    EXPECT_EQ(resultTs.getSeconds(), 1422057600LL);

    delete resultVec;
}

TEST(ToUtcTimestampTest, PositiveOffset) {
    // 2015-01-24 00:00:00 in +08:00 is 2015-01-23 16:00:00 UTC
    int64_t inputMicros = UtcTimestampTestHelper::TimestampMicros(1422057600LL);
    std::vector<int64_t> inputs = {inputMicros};
    BaseVector* tsVec = UtcTimestampTestHelper::CreateTimestampVector(inputs);
    BaseVector* tzVec = UtcTimestampTestHelper::CreateConstStringVector("+08:00", 1);

    std::stack<BaseVector*> args;
    args.push(tsVec);
    args.push(tzVec);

    BaseVector* resultVec = nullptr;
    UtcTimestampTestHelper::ExecuteFunction("to_utc_timestamp",
        {OMNI_TIMESTAMP, OMNI_VARCHAR}, args, resultVec);

    ASSERT_NE(resultVec, nullptr);
    auto* resultTyped = dynamic_cast<Vector<int64_t>*>(resultVec);
    int64_t resultMicros = resultTyped->GetValue(0);
    Timestamp resultTs = Timestamp::fromMicros(resultMicros);
    EXPECT_EQ(resultTs.getSeconds(), 1422057600LL - 8 * 3600);

    delete resultVec;
}

TEST(ToUtcTimestampTest, NullTimestamp) {
    std::vector<int64_t> inputs = {0LL, 0LL};
    BaseVector* tsVec = UtcTimestampTestHelper::CreateTimestampVector(inputs);
    tsVec->SetNull(1);
    BaseVector* tzVec = UtcTimestampTestHelper::CreateConstStringVector("UTC", 2);

    std::stack<BaseVector*> args;
    args.push(tsVec);
    args.push(tzVec);

    BaseVector* resultVec = nullptr;
    UtcTimestampTestHelper::ExecuteFunction("to_utc_timestamp",
        {OMNI_TIMESTAMP, OMNI_VARCHAR}, args, resultVec);

    ASSERT_NE(resultVec, nullptr);
    EXPECT_FALSE(resultVec->IsNull(0));
    EXPECT_TRUE(resultVec->IsNull(1));

    delete resultVec;
}

TEST(ToUtcTimestampTest, LongInputType) {
    // Test with OMNI_LONG input type (equivalent to OMNI_TIMESTAMP)
    int64_t inputMicros = UtcTimestampTestHelper::TimestampMicros(1422057600LL);
    std::vector<int64_t> inputs = {inputMicros};
    BaseVector* tsVec = UtcTimestampTestHelper::CreateLongVector(inputs);
    BaseVector* tzVec = UtcTimestampTestHelper::CreateConstStringVector("UTC", 1);

    std::stack<BaseVector*> args;
    args.push(tsVec);
    args.push(tzVec);

    BaseVector* resultVec = nullptr;
    UtcTimestampTestHelper::ExecuteFunction("to_utc_timestamp",
        {OMNI_LONG, OMNI_VARCHAR}, args, resultVec);

    ASSERT_NE(resultVec, nullptr);
    auto* resultTyped = dynamic_cast<Vector<int64_t>*>(resultVec);
    EXPECT_EQ(resultTyped->GetValue(0), inputMicros);

    delete resultVec;
}

TEST(ToUtcTimestampTest, MultipleRows) {
    std::vector<int64_t> inputs = {
        UtcTimestampTestHelper::TimestampMicros(1422057600LL),
        UtcTimestampTestHelper::TimestampMicros(0LL),
        UtcTimestampTestHelper::TimestampMicros(1422057600LL + 5 * 3600 + 30 * 60)
    };
    BaseVector* tsVec = UtcTimestampTestHelper::CreateTimestampVector(inputs);
    BaseVector* tzVec = UtcTimestampTestHelper::CreateConstStringVector("Asia/Kolkata", 3);

    std::stack<BaseVector*> args;
    args.push(tsVec);
    args.push(tzVec);

    BaseVector* resultVec = nullptr;
    UtcTimestampTestHelper::ExecuteFunction("to_utc_timestamp",
        {OMNI_TIMESTAMP, OMNI_VARCHAR}, args, resultVec);

    ASSERT_NE(resultVec, nullptr);
    auto* resultTyped = dynamic_cast<Vector<int64_t>*>(resultVec);

    for (int32_t i = 0; i < 3; ++i) {
        EXPECT_FALSE(resultVec->IsNull(i));
    }

    Timestamp r0 = Timestamp::fromMicros(resultTyped->GetValue(0));
    EXPECT_EQ(r0.getSeconds(), 1422057600LL - 5 * 3600 - 30 * 60);

    Timestamp r2 = Timestamp::fromMicros(resultTyped->GetValue(2));
    EXPECT_EQ(r2.getSeconds(), 1422057600LL);

    delete resultVec;
}

// ========== from_utc_timestamp Tests ==========

TEST(ToUtcTimestampTest, FromUtc_BasicLosAngeles) {
    // 2015-07-24 07:00:00 UTC -> 2015-07-24 00:00:00 America/Los_Angeles (PDT, -7h)
    int64_t inputMicros = UtcTimestampTestHelper::TimestampMicros(
        1437696000LL + 7 * 3600);
    std::vector<int64_t> inputs = {inputMicros};
    BaseVector* tsVec = UtcTimestampTestHelper::CreateTimestampVector(inputs);
    BaseVector* tzVec = UtcTimestampTestHelper::CreateConstStringVector("America/Los_Angeles", 1);

    std::stack<BaseVector*> args;
    args.push(tsVec);
    args.push(tzVec);

    BaseVector* resultVec = nullptr;
    UtcTimestampTestHelper::ExecuteFunction("from_utc_timestamp",
        {OMNI_TIMESTAMP, OMNI_VARCHAR}, args, resultVec);

    ASSERT_NE(resultVec, nullptr);
    auto* resultTyped = dynamic_cast<Vector<int64_t>*>(resultVec);
    Timestamp resultTs = Timestamp::fromMicros(resultTyped->GetValue(0));
    EXPECT_EQ(resultTs.getSeconds(), 1437696000LL);

    delete resultVec;
}

TEST(ToUtcTimestampTest, FromUtc_BasicLosAngelesWinter) {
    // 2015-01-24 08:00:00 UTC -> 2015-01-24 00:00:00 America/Los_Angeles (PST, -8h)
    int64_t inputMicros = UtcTimestampTestHelper::TimestampMicros(
        1422057600LL + 8 * 3600);
    std::vector<int64_t> inputs = {inputMicros};
    BaseVector* tsVec = UtcTimestampTestHelper::CreateTimestampVector(inputs);
    BaseVector* tzVec = UtcTimestampTestHelper::CreateConstStringVector("America/Los_Angeles", 1);

    std::stack<BaseVector*> args;
    args.push(tsVec);
    args.push(tzVec);

    BaseVector* resultVec = nullptr;
    UtcTimestampTestHelper::ExecuteFunction("from_utc_timestamp",
        {OMNI_TIMESTAMP, OMNI_VARCHAR}, args, resultVec);

    ASSERT_NE(resultVec, nullptr);
    auto* resultTyped = dynamic_cast<Vector<int64_t>*>(resultVec);
    Timestamp resultTs = Timestamp::fromMicros(resultTyped->GetValue(0));
    EXPECT_EQ(resultTs.getSeconds(), 1422057600LL);

    delete resultVec;
}

TEST(ToUtcTimestampTest, FromUtc_UTC) {
    // UTC to UTC should be identity
    int64_t inputMicros = UtcTimestampTestHelper::TimestampMicros(1422057600LL);
    std::vector<int64_t> inputs = {inputMicros};
    BaseVector* tsVec = UtcTimestampTestHelper::CreateTimestampVector(inputs);
    BaseVector* tzVec = UtcTimestampTestHelper::CreateConstStringVector("UTC", 1);

    std::stack<BaseVector*> args;
    args.push(tsVec);
    args.push(tzVec);

    BaseVector* resultVec = nullptr;
    UtcTimestampTestHelper::ExecuteFunction("from_utc_timestamp",
        {OMNI_TIMESTAMP, OMNI_VARCHAR}, args, resultVec);

    ASSERT_NE(resultVec, nullptr);
    auto* resultTyped = dynamic_cast<Vector<int64_t>*>(resultVec);
    EXPECT_EQ(resultTyped->GetValue(0), inputMicros);

    delete resultVec;
}

TEST(ToUtcTimestampTest, FromUtc_AsiaKolkata) {
    // 2015-01-24 00:00:00 UTC -> 2015-01-24 05:30:00 Asia/Kolkata (+5:30)
    int64_t inputMicros = UtcTimestampTestHelper::TimestampMicros(1422057600LL);
    std::vector<int64_t> inputs = {inputMicros};
    BaseVector* tsVec = UtcTimestampTestHelper::CreateTimestampVector(inputs);
    BaseVector* tzVec = UtcTimestampTestHelper::CreateConstStringVector("Asia/Kolkata", 1);

    std::stack<BaseVector*> args;
    args.push(tsVec);
    args.push(tzVec);

    BaseVector* resultVec = nullptr;
    UtcTimestampTestHelper::ExecuteFunction("from_utc_timestamp",
        {OMNI_TIMESTAMP, OMNI_VARCHAR}, args, resultVec);

    ASSERT_NE(resultVec, nullptr);
    auto* resultTyped = dynamic_cast<Vector<int64_t>*>(resultVec);
    Timestamp resultTs = Timestamp::fromMicros(resultTyped->GetValue(0));
    EXPECT_EQ(resultTs.getSeconds(), 1422057600LL + 5 * 3600 + 30 * 60);

    delete resultVec;
}

TEST(ToUtcTimestampTest, FromUtc_PositiveOffset) {
    // 2015-01-24 00:00:00 UTC -> 2015-01-24 08:00:00 +08:00
    int64_t inputMicros = UtcTimestampTestHelper::TimestampMicros(1422057600LL);
    std::vector<int64_t> inputs = {inputMicros};
    BaseVector* tsVec = UtcTimestampTestHelper::CreateTimestampVector(inputs);
    BaseVector* tzVec = UtcTimestampTestHelper::CreateConstStringVector("+08:00", 1);

    std::stack<BaseVector*> args;
    args.push(tsVec);
    args.push(tzVec);

    BaseVector* resultVec = nullptr;
    UtcTimestampTestHelper::ExecuteFunction("from_utc_timestamp",
        {OMNI_TIMESTAMP, OMNI_VARCHAR}, args, resultVec);

    ASSERT_NE(resultVec, nullptr);
    auto* resultTyped = dynamic_cast<Vector<int64_t>*>(resultVec);
    Timestamp resultTs = Timestamp::fromMicros(resultTyped->GetValue(0));
    EXPECT_EQ(resultTs.getSeconds(), 1422057600LL + 8 * 3600);

    delete resultVec;
}

TEST(ToUtcTimestampTest, FromUtc_NullTimestamp) {
    std::vector<int64_t> inputs = {0LL, 0LL};
    BaseVector* tsVec = UtcTimestampTestHelper::CreateTimestampVector(inputs);
    tsVec->SetNull(1);
    BaseVector* tzVec = UtcTimestampTestHelper::CreateConstStringVector("UTC", 2);

    std::stack<BaseVector*> args;
    args.push(tsVec);
    args.push(tzVec);

    BaseVector* resultVec = nullptr;
    UtcTimestampTestHelper::ExecuteFunction("from_utc_timestamp",
        {OMNI_TIMESTAMP, OMNI_VARCHAR}, args, resultVec);

    ASSERT_NE(resultVec, nullptr);
    EXPECT_FALSE(resultVec->IsNull(0));
    EXPECT_TRUE(resultVec->IsNull(1));

    delete resultVec;
}

TEST(ToUtcTimestampTest, FromUtc_LongInputType) {
    int64_t inputMicros = UtcTimestampTestHelper::TimestampMicros(1422057600LL);
    std::vector<int64_t> inputs = {inputMicros};
    BaseVector* tsVec = UtcTimestampTestHelper::CreateLongVector(inputs);
    BaseVector* tzVec = UtcTimestampTestHelper::CreateConstStringVector("UTC", 1);

    std::stack<BaseVector*> args;
    args.push(tsVec);
    args.push(tzVec);

    BaseVector* resultVec = nullptr;
    UtcTimestampTestHelper::ExecuteFunction("from_utc_timestamp",
        {OMNI_LONG, OMNI_VARCHAR}, args, resultVec);

    ASSERT_NE(resultVec, nullptr);
    auto* resultTyped = dynamic_cast<Vector<int64_t>*>(resultVec);
    EXPECT_EQ(resultTyped->GetValue(0), inputMicros);

    delete resultVec;
}

// ========== Round-trip Tests ==========

TEST(ToUtcTimestampTest, RoundTrip_LosAngeles) {
    // to_utc_timestamp(from_utc_timestamp(ts, tz), tz) should return ts
    int64_t originalMicros = UtcTimestampTestHelper::TimestampMicros(
        1437721200LL);  // 2015-07-24 07:00:00 UTC

    // Step 1: from_utc_timestamp
    std::vector<int64_t> inputs1 = {originalMicros};
    BaseVector* tsVec1 = UtcTimestampTestHelper::CreateTimestampVector(inputs1);
    BaseVector* tzVec1 = UtcTimestampTestHelper::CreateConstStringVector("America/Los_Angeles", 1);

    std::stack<BaseVector*> args1;
    args1.push(tsVec1);
    args1.push(tzVec1);

    BaseVector* localVec = nullptr;
    UtcTimestampTestHelper::ExecuteFunction("from_utc_timestamp",
        {OMNI_TIMESTAMP, OMNI_VARCHAR}, args1, localVec);

    ASSERT_NE(localVec, nullptr);

    // Step 2: to_utc_timestamp
    BaseVector* tzVec2 = UtcTimestampTestHelper::CreateConstStringVector("America/Los_Angeles", 1);

    std::stack<BaseVector*> args2;
    args2.push(localVec);
    args2.push(tzVec2);

    BaseVector* resultVec = nullptr;
    UtcTimestampTestHelper::ExecuteFunction("to_utc_timestamp",
        {OMNI_TIMESTAMP, OMNI_VARCHAR}, args2, resultVec);

    ASSERT_NE(resultVec, nullptr);
    auto* resultTyped = dynamic_cast<Vector<int64_t>*>(resultVec);
    EXPECT_EQ(resultTyped->GetValue(0), originalMicros);

    delete resultVec;
}

TEST(ToUtcTimestampTest, RoundTrip_AsiaKolkata) {
    int64_t originalMicros = UtcTimestampTestHelper::TimestampMicros(1422057600LL);

    std::vector<int64_t> inputs1 = {originalMicros};
    BaseVector* tsVec1 = UtcTimestampTestHelper::CreateTimestampVector(inputs1);
    BaseVector* tzVec1 = UtcTimestampTestHelper::CreateConstStringVector("Asia/Kolkata", 1);

    std::stack<BaseVector*> args1;
    args1.push(tsVec1);
    args1.push(tzVec1);

    BaseVector* localVec = nullptr;
    UtcTimestampTestHelper::ExecuteFunction("from_utc_timestamp",
        {OMNI_TIMESTAMP, OMNI_VARCHAR}, args1, localVec);

    ASSERT_NE(localVec, nullptr);

    BaseVector* tzVec2 = UtcTimestampTestHelper::CreateConstStringVector("Asia/Kolkata", 1);

    std::stack<BaseVector*> args2;
    args2.push(localVec);
    args2.push(tzVec2);

    BaseVector* resultVec = nullptr;
    UtcTimestampTestHelper::ExecuteFunction("to_utc_timestamp",
        {OMNI_TIMESTAMP, OMNI_VARCHAR}, args2, resultVec);

    ASSERT_NE(resultVec, nullptr);
    auto* resultTyped = dynamic_cast<Vector<int64_t>*>(resultVec);
    EXPECT_EQ(resultTyped->GetValue(0), originalMicros);

    delete resultVec;
}

// ========== Edge Cases ==========

TEST(ToUtcTimestampTest, EpochTimestamp) {
    int64_t inputMicros = 0LL;
    std::vector<int64_t> inputs = {inputMicros};
    BaseVector* tsVec = UtcTimestampTestHelper::CreateTimestampVector(inputs);
    BaseVector* tzVec = UtcTimestampTestHelper::CreateConstStringVector("America/Los_Angeles", 1);

    std::stack<BaseVector*> args;
    args.push(tsVec);
    args.push(tzVec);

    BaseVector* resultVec = nullptr;
    UtcTimestampTestHelper::ExecuteFunction("to_utc_timestamp",
        {OMNI_TIMESTAMP, OMNI_VARCHAR}, args, resultVec);

    ASSERT_NE(resultVec, nullptr);
    EXPECT_FALSE(resultVec->IsNull(0));

    auto* resultTyped = dynamic_cast<Vector<int64_t>*>(resultVec);
    Timestamp resultTs = Timestamp::fromMicros(resultTyped->GetValue(0));
    EXPECT_EQ(resultTs.getSeconds(), 8 * 3600);

    delete resultVec;
}

TEST(ToUtcTimestampTest, FromUtc_EpochTimestamp) {
    int64_t inputMicros = 0LL;
    std::vector<int64_t> inputs = {inputMicros};
    BaseVector* tsVec = UtcTimestampTestHelper::CreateTimestampVector(inputs);
    BaseVector* tzVec = UtcTimestampTestHelper::CreateConstStringVector("America/Los_Angeles", 1);

    std::stack<BaseVector*> args;
    args.push(tsVec);
    args.push(tzVec);

    BaseVector* resultVec = nullptr;
    UtcTimestampTestHelper::ExecuteFunction("from_utc_timestamp",
        {OMNI_TIMESTAMP, OMNI_VARCHAR}, args, resultVec);

    ASSERT_NE(resultVec, nullptr);
    EXPECT_FALSE(resultVec->IsNull(0));

    auto* resultTyped = dynamic_cast<Vector<int64_t>*>(resultVec);
    Timestamp resultTs = Timestamp::fromMicros(resultTyped->GetValue(0));
    EXPECT_EQ(resultTs.getSeconds(), -8 * 3600);

    delete resultVec;
}

TEST(ToUtcTimestampTest, NonConstTimezone) {
    std::vector<int64_t> inputs = {
        UtcTimestampTestHelper::TimestampMicros(1422057600LL),
        UtcTimestampTestHelper::TimestampMicros(1422057600LL)
    };
    BaseVector* tsVec = UtcTimestampTestHelper::CreateTimestampVector(inputs);

    std::vector<std::string> tzValues = {"UTC", "Asia/Kolkata"};
    BaseVector* tzVec = UtcTimestampTestHelper::CreateStringVector(tzValues);

    std::stack<BaseVector*> args;
    args.push(tsVec);
    args.push(tzVec);

    BaseVector* resultVec = nullptr;
    UtcTimestampTestHelper::ExecuteFunction("to_utc_timestamp",
        {OMNI_TIMESTAMP, OMNI_VARCHAR}, args, resultVec);

    ASSERT_NE(resultVec, nullptr);
    auto* resultTyped = dynamic_cast<Vector<int64_t>*>(resultVec);

    Timestamp r0 = Timestamp::fromMicros(resultTyped->GetValue(0));
    EXPECT_EQ(r0.getSeconds(), 1422057600LL);

    Timestamp r1 = Timestamp::fromMicros(resultTyped->GetValue(1));
    EXPECT_EQ(r1.getSeconds(), 1422057600LL - 5 * 3600 - 30 * 60);

    delete resultVec;
}
