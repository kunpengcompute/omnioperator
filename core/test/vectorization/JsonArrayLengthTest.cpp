/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: json_array_length function unit tests
 */

#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <stack>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/VectorFunction.h"
#include "codegen/func_signature.h"
#include "vector/vector_helper.h"
#include "vector/vector.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::mem;
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::codegen;
using namespace omniruntime::TestUtil;

class JsonArrayLengthTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const json_array_length_test_env =
    ::testing::AddGlobalTestEnvironment(new JsonArrayLengthTestEnvironment);

class JsonArrayLengthTestHelper {
public:
    static BaseVector* CreateStringVector(const std::vector<std::string>& values) {
        BaseVector* vec = VectorHelper::CreateStringVector(values.size());
        vec->SetIsField(true);
        auto* typed = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(vec);
        EXPECT_NE(typed, nullptr);
        for (size_t i = 0; i < values.size(); ++i) {
            std::string_view sv(values[i]);
            typed->SetValue(i, sv);
        }
        return vec;
    }

    static void ExecuteJsonArrayLength(BaseVector* inputVec, BaseVector*& result) {
        std::vector<DataTypeId> inputTypeIds = {OMNI_VARCHAR};
        auto sig = std::make_shared<FunctionSignature>("json_array_length", inputTypeIds, OMNI_INT);
        auto fn = VectorFunction::Find(sig);
        ASSERT_NE(fn, nullptr);
        auto outputType = std::make_shared<DataType>(OMNI_INT);
        ExecutionContext ctx(4096, nullptr);
        ctx.SetResultRowSize(inputVec->GetSize());
        std::stack<BaseVector*> args;
        args.push(inputVec);
        ASSERT_NO_THROW(fn->Apply(args, outputType, result, &ctx));
    }

    static void ValidateNullResult(BaseVector* result, int32_t row) {
        auto* intResult = dynamic_cast<Vector<int32_t>*>(result);
        ASSERT_NE(intResult, nullptr);
        EXPECT_TRUE(intResult->IsNull(row));
    }

    static void ValidateIntResult(BaseVector* result, int32_t row, int32_t expected) {
        auto* intResult = dynamic_cast<Vector<int32_t>*>(result);
        ASSERT_NE(intResult, nullptr);
        EXPECT_FALSE(intResult->IsNull(row));
        EXPECT_EQ(intResult->GetValue(row), expected);
    }
};

TEST(JsonArrayLengthTest, BasicArray) {
    std::vector<std::string> inputs = {R"([1,2,3,4])"};
    BaseVector* inputVec = JsonArrayLengthTestHelper::CreateStringVector(inputs);
    BaseVector* result = nullptr;
    JsonArrayLengthTestHelper::ExecuteJsonArrayLength(inputVec, result);
    JsonArrayLengthTestHelper::ValidateIntResult(result, 0, 4);
    delete result;
}

TEST(JsonArrayLengthTest, EmptyArray) {
    std::vector<std::string> inputs = {R"([])"};
    BaseVector* inputVec = JsonArrayLengthTestHelper::CreateStringVector(inputs);
    BaseVector* result = nullptr;
    JsonArrayLengthTestHelper::ExecuteJsonArrayLength(inputVec, result);
    JsonArrayLengthTestHelper::ValidateIntResult(result, 0, 0);
    delete result;
}

TEST(JsonArrayLengthTest, SingleElement) {
    std::vector<std::string> inputs = {R"([1])"};
    BaseVector* inputVec = JsonArrayLengthTestHelper::CreateStringVector(inputs);
    BaseVector* result = nullptr;
    JsonArrayLengthTestHelper::ExecuteJsonArrayLength(inputVec, result);
    JsonArrayLengthTestHelper::ValidateIntResult(result, 0, 1);
    delete result;
}

TEST(JsonArrayLengthTest, NestedArray) {
    std::vector<std::string> inputs = {R"([1,2,3,{"f1":1,"f2":[5,6]},4])"};
    BaseVector* inputVec = JsonArrayLengthTestHelper::CreateStringVector(inputs);
    BaseVector* result = nullptr;
    JsonArrayLengthTestHelper::ExecuteJsonArrayLength(inputVec, result);
    // Should return 5 (only counts outermost array elements)
    JsonArrayLengthTestHelper::ValidateIntResult(result, 0, 5);
    delete result;
}

TEST(JsonArrayLengthTest, NonArrayNumber) {
    std::vector<std::string> inputs = {R"(1)"};
    BaseVector* inputVec = JsonArrayLengthTestHelper::CreateStringVector(inputs);
    BaseVector* result = nullptr;
    JsonArrayLengthTestHelper::ExecuteJsonArrayLength(inputVec, result);
    JsonArrayLengthTestHelper::ValidateNullResult(result, 0);
    delete result;
}

TEST(JsonArrayLengthTest, NonArrayString) {
    std::vector<std::string> inputs = {R"("hello")"};
    BaseVector* inputVec = JsonArrayLengthTestHelper::CreateStringVector(inputs);
    BaseVector* result = nullptr;
    JsonArrayLengthTestHelper::ExecuteJsonArrayLength(inputVec, result);
    JsonArrayLengthTestHelper::ValidateNullResult(result, 0);
    delete result;
}

TEST(JsonArrayLengthTest, NonArrayObject) {
    std::vector<std::string> inputs = {R"({"k1":"v1"})"};
    BaseVector* inputVec = JsonArrayLengthTestHelper::CreateStringVector(inputs);
    BaseVector* result = nullptr;
    JsonArrayLengthTestHelper::ExecuteJsonArrayLength(inputVec, result);
    JsonArrayLengthTestHelper::ValidateNullResult(result, 0);
    delete result;
}

TEST(JsonArrayLengthTest, NonArrayBoolean) {
    std::vector<std::string> inputs = {R"(true)"};
    BaseVector* inputVec = JsonArrayLengthTestHelper::CreateStringVector(inputs);
    BaseVector* result = nullptr;
    JsonArrayLengthTestHelper::ExecuteJsonArrayLength(inputVec, result);
    JsonArrayLengthTestHelper::ValidateNullResult(result, 0);
    delete result;
}

TEST(JsonArrayLengthTest, InvalidJson) {
    std::vector<std::string> inputs = {R"([1,2)"};
    BaseVector* inputVec = JsonArrayLengthTestHelper::CreateStringVector(inputs);
    BaseVector* result = nullptr;
    JsonArrayLengthTestHelper::ExecuteJsonArrayLength(inputVec, result);
    JsonArrayLengthTestHelper::ValidateNullResult(result, 0);
    delete result;
}

TEST(JsonArrayLengthTest, EmptyString) {
    std::vector<std::string> inputs = {R"("")"};
    BaseVector* inputVec = JsonArrayLengthTestHelper::CreateStringVector(inputs);
    BaseVector* result = nullptr;
    JsonArrayLengthTestHelper::ExecuteJsonArrayLength(inputVec, result);
    JsonArrayLengthTestHelper::ValidateNullResult(result, 0);
    delete result;
}

TEST(JsonArrayLengthTest, NullPropagation) {
    std::vector<std::string> inputs = {R"([1,2,3])", R"([4,5])"};
    BaseVector* inputVec = JsonArrayLengthTestHelper::CreateStringVector(inputs);
    inputVec->SetNull(0);
    BaseVector* result = nullptr;
    JsonArrayLengthTestHelper::ExecuteJsonArrayLength(inputVec, result);
    JsonArrayLengthTestHelper::ValidateNullResult(result, 0);
    JsonArrayLengthTestHelper::ValidateIntResult(result, 1, 2);
    delete result;
}

TEST(JsonArrayLengthTest, MultipleRows) {
    std::vector<std::string> inputs = {
        R"([1,2,3])",
        R"([])",
        R"(not json)",
        R"([4,5,6,7,8])",
        R"({"k1":"v1"})"
    };
    BaseVector* inputVec = JsonArrayLengthTestHelper::CreateStringVector(inputs);
    BaseVector* result = nullptr;
    JsonArrayLengthTestHelper::ExecuteJsonArrayLength(inputVec, result);
    JsonArrayLengthTestHelper::ValidateIntResult(result, 0, 3);
    JsonArrayLengthTestHelper::ValidateIntResult(result, 1, 0);
    JsonArrayLengthTestHelper::ValidateNullResult(result, 2);
    JsonArrayLengthTestHelper::ValidateIntResult(result, 3, 5);
    JsonArrayLengthTestHelper::ValidateNullResult(result, 4);
    delete result;
}

TEST(JsonArrayLengthTest, LargeArray) {
    std::vector<std::string> inputs = {
        R"([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20])"
    };
    BaseVector* inputVec = JsonArrayLengthTestHelper::CreateStringVector(inputs);
    BaseVector* result = nullptr;
    JsonArrayLengthTestHelper::ExecuteJsonArrayLength(inputVec, result);
    JsonArrayLengthTestHelper::ValidateIntResult(result, 0, 20);
    delete result;
}

TEST(JsonArrayLengthTest, MalformedJsonArray) {
    std::vector<std::string> inputs = {R"((})"};
    BaseVector* inputVec = JsonArrayLengthTestHelper::CreateStringVector(inputs);
    BaseVector* result = nullptr;
    JsonArrayLengthTestHelper::ExecuteJsonArrayLength(inputVec, result);
    JsonArrayLengthTestHelper::ValidateNullResult(result, 0);
    delete result;
}

TEST(JsonArrayLengthTest, ObjectWithArrayField) {
    std::vector<std::string> inputs = {R"({"k1":[0,1,2]})"};
    BaseVector* inputVec = JsonArrayLengthTestHelper::CreateStringVector(inputs);
    BaseVector* result = nullptr;
    JsonArrayLengthTestHelper::ExecuteJsonArrayLength(inputVec, result);
    // Object is not an array, should return NULL
    JsonArrayLengthTestHelper::ValidateNullResult(result, 0);
    delete result;
}
