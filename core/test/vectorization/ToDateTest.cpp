/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: ToDate function unit tests
 *              to_date(string, format) -> INT (days since epoch, UTC)
 */

#include <gtest/gtest.h>
#include <vector>
#include <string>
#include <string_view>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/functions/ToDate.h"
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

class ToDateTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const to_date_test_env =
    ::testing::AddGlobalTestEnvironment(new ToDateTestEnvironment);

class ToDateTestHelper {
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

    static void ExecuteFunction(const std::string& funcName,
        const std::vector<DataTypeId>& inputTypes, DataTypeId outputType,
        std::stack<BaseVector*>& args, BaseVector*& result) {
        auto signature = std::make_shared<FunctionSignature>(funcName, inputTypes, outputType);
        auto function = VectorFunction::Find(signature);
        ASSERT_NE(function, nullptr) << funcName << " function not found for signature";

        auto outType = std::make_shared<DataType>(OMNI_INT);
        ExecutionContext context;
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

// ========== to_date (ToDate) Tests ==========

TEST(ToDateTest, ToDate_BasicDateFormat) {
    // 1970-01-01 -> 0, 2024-01-01 -> 19723, 2024-01-15 -> 19737
    std::vector<std::string> inputs = {"1970-01-01", "2024-01-01", "2024-01-15"};
    BaseVector* inputVec = ToDateTestHelper::CreateStringVector(inputs);
    BaseVector* formatVec = ToDateTestHelper::CreateConstStringVector("yyyy-MM-dd", inputs.size());

    std::stack<BaseVector*> args;
    args.push(inputVec);
    args.push(formatVec);

    BaseVector* resultVec = nullptr;
    ToDateTestHelper::ExecuteFunction("to_date",
        {OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_INT, args, resultVec);

    ASSERT_NE(resultVec, nullptr);
    auto* resultTyped = dynamic_cast<Vector<int32_t>*>(resultVec);
    ASSERT_NE(resultTyped, nullptr);

    EXPECT_FALSE(resultVec->IsNull(0));
    EXPECT_EQ(resultTyped->GetValue(0), 0);

    EXPECT_FALSE(resultVec->IsNull(1));
    EXPECT_EQ(resultTyped->GetValue(1), 19723);

    EXPECT_FALSE(resultVec->IsNull(2));
    EXPECT_EQ(resultTyped->GetValue(2), 19737);

    delete resultVec;
}

TEST(ToDateTest, ToDate_SlashFormat) {
    // 2024-01-15 written as 2024/01/15 -> 19737
    std::vector<std::string> inputs = {"1970/01/01", "2024/01/15"};
    BaseVector* inputVec = ToDateTestHelper::CreateStringVector(inputs);
    BaseVector* formatVec = ToDateTestHelper::CreateConstStringVector("yyyy/MM/dd", inputs.size());

    std::stack<BaseVector*> args;
    args.push(inputVec);
    args.push(formatVec);

    BaseVector* resultVec = nullptr;
    ToDateTestHelper::ExecuteFunction("to_date",
        {OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_INT, args, resultVec);

    ASSERT_NE(resultVec, nullptr);
    auto* resultTyped = dynamic_cast<Vector<int32_t>*>(resultVec);
    ASSERT_NE(resultTyped, nullptr);

    EXPECT_FALSE(resultVec->IsNull(0));
    EXPECT_EQ(resultTyped->GetValue(0), 0);

    EXPECT_FALSE(resultVec->IsNull(1));
    EXPECT_EQ(resultTyped->GetValue(1), 19737);

    delete resultVec;
}

TEST(ToDateTest, ToDate_DateTimeFormatTruncatesToDate) {
    // Format carries a time component; the date portion must still be returned (2024-01-15 -> 19737)
    std::vector<std::string> inputs = {"1970-01-01 00:00:00", "2024-01-15 10:30:45"};
    BaseVector* inputVec = ToDateTestHelper::CreateStringVector(inputs);
    BaseVector* formatVec = ToDateTestHelper::CreateConstStringVector(
        "yyyy-MM-dd HH:mm:ss", inputs.size());

    std::stack<BaseVector*> args;
    args.push(inputVec);
    args.push(formatVec);

    BaseVector* resultVec = nullptr;
    ToDateTestHelper::ExecuteFunction("to_date",
        {OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_INT, args, resultVec);

    ASSERT_NE(resultVec, nullptr);
    auto* resultTyped = dynamic_cast<Vector<int32_t>*>(resultVec);
    ASSERT_NE(resultTyped, nullptr);

    EXPECT_FALSE(resultVec->IsNull(0));
    EXPECT_EQ(resultTyped->GetValue(0), 0);

    EXPECT_FALSE(resultVec->IsNull(1));
    EXPECT_EQ(resultTyped->GetValue(1), 19737);

    delete resultVec;
}

TEST(ToDateTest, ToDate_LeapYearAndBoundaries) {
    // 2020-02-29 -> 18321, 2024-12-31 -> 20088, 1994-09-27 -> 9035, 1969-12-31 -> -1
    std::vector<std::string> inputs = {"2020-02-29", "2024-12-31", "1994-09-27", "1969-12-31"};
    BaseVector* inputVec = ToDateTestHelper::CreateStringVector(inputs);
    BaseVector* formatVec = ToDateTestHelper::CreateConstStringVector("yyyy-MM-dd", inputs.size());

    std::stack<BaseVector*> args;
    args.push(inputVec);
    args.push(formatVec);

    BaseVector* resultVec = nullptr;
    ToDateTestHelper::ExecuteFunction("to_date",
        {OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_INT, args, resultVec);

    ASSERT_NE(resultVec, nullptr);
    auto* resultTyped = dynamic_cast<Vector<int32_t>*>(resultVec);
    ASSERT_NE(resultTyped, nullptr);

    EXPECT_FALSE(resultVec->IsNull(0));
    EXPECT_EQ(resultTyped->GetValue(0), 18321);
    EXPECT_FALSE(resultVec->IsNull(1));
    EXPECT_EQ(resultTyped->GetValue(1), 20088);
    EXPECT_FALSE(resultVec->IsNull(2));
    EXPECT_EQ(resultTyped->GetValue(2), 9035);
    EXPECT_FALSE(resultVec->IsNull(3));
    EXPECT_EQ(resultTyped->GetValue(3), -1);

    delete resultVec;
}

TEST(ToDateTest, ToDate_NullInput) {
    std::vector<std::string> inputs = {"1970-01-01", "2024-01-01"};
    BaseVector* inputVec = ToDateTestHelper::CreateStringVector(inputs);
    inputVec->SetNull(1);

    BaseVector* formatVec = ToDateTestHelper::CreateConstStringVector("yyyy-MM-dd", inputs.size());

    std::stack<BaseVector*> args;
    args.push(inputVec);
    args.push(formatVec);

    BaseVector* resultVec = nullptr;
    ToDateTestHelper::ExecuteFunction("to_date",
        {OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_INT, args, resultVec);

    ASSERT_NE(resultVec, nullptr);
    EXPECT_FALSE(resultVec->IsNull(0));
    EXPECT_TRUE(resultVec->IsNull(1));

    delete resultVec;
}

TEST(ToDateTest, ToDate_ParseError) {
    // "not-a-date" does not match yyyy-MM-dd -> NULL
    std::vector<std::string> inputs = {"2024-01-01", "not-a-date"};
    BaseVector* inputVec = ToDateTestHelper::CreateStringVector(inputs);
    BaseVector* formatVec = ToDateTestHelper::CreateConstStringVector("yyyy-MM-dd", inputs.size());

    std::stack<BaseVector*> args;
    args.push(inputVec);
    args.push(formatVec);

    BaseVector* resultVec = nullptr;
    ToDateTestHelper::ExecuteFunction("to_date",
        {OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_INT, args, resultVec);

    ASSERT_NE(resultVec, nullptr);
    EXPECT_FALSE(resultVec->IsNull(0));
    EXPECT_TRUE(resultVec->IsNull(1));

    delete resultVec;
}

TEST(ToDateTest, ToDate_EmptyString) {
    std::vector<std::string> inputs = {"", ""};
    BaseVector* inputVec = ToDateTestHelper::CreateStringVector(inputs);
    BaseVector* formatVec = ToDateTestHelper::CreateConstStringVector("yyyy-MM-dd", inputs.size());

    std::stack<BaseVector*> args;
    args.push(inputVec);
    args.push(formatVec);

    BaseVector* resultVec = nullptr;
    ToDateTestHelper::ExecuteFunction("to_date",
        {OMNI_VARCHAR, OMNI_VARCHAR}, OMNI_INT, args, resultVec);

    ASSERT_NE(resultVec, nullptr);
    EXPECT_TRUE(resultVec->IsNull(0));
    EXPECT_TRUE(resultVec->IsNull(1));

    delete resultVec;
}

TEST(ToDateTest, ToDate_CharInputType) {
    // Verify the {OMNI_CHAR, OMNI_VARCHAR} -> OMNI_INT overload resolves and parses correctly.
    std::vector<std::string> inputs = {"2024-01-15"};
    BaseVector* inputVec = ToDateTestHelper::CreateStringVector(inputs);
    BaseVector* formatVec = ToDateTestHelper::CreateConstStringVector("yyyy-MM-dd", inputs.size());

    std::stack<BaseVector*> args;
    args.push(inputVec);
    args.push(formatVec);

    BaseVector* resultVec = nullptr;
    ToDateTestHelper::ExecuteFunction("to_date",
        {OMNI_CHAR, OMNI_VARCHAR}, OMNI_INT, args, resultVec);

    ASSERT_NE(resultVec, nullptr);
    auto* resultTyped = dynamic_cast<Vector<int32_t>*>(resultVec);
    ASSERT_NE(resultTyped, nullptr);
    EXPECT_FALSE(resultVec->IsNull(0));
    EXPECT_EQ(resultTyped->GetValue(0), 19737);

    delete resultVec;
}
