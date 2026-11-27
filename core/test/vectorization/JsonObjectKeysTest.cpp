/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: json_object_keys function unit tests
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
#include "vector/array_vector.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::mem;
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::codegen;
using namespace omniruntime::TestUtil;

class JsonObjectKeysTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const json_object_keys_test_env =
    ::testing::AddGlobalTestEnvironment(new JsonObjectKeysTestEnvironment);

class JsonObjectKeysTestHelper {
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

    static void ExecuteJsonObjectKeys(BaseVector* inputVec, BaseVector*& result) {
        std::vector<DataTypeId> inputTypeIds = {OMNI_VARCHAR};
        auto sig = std::make_shared<FunctionSignature>("json_object_keys", inputTypeIds, OMNI_ARRAY);
        auto fn = VectorFunction::Find(sig);
        ASSERT_NE(fn, nullptr);
        auto outputType = std::make_shared<DataType>(OMNI_ARRAY);
        ExecutionContext ctx(4096, nullptr);
        ctx.SetResultRowSize(inputVec->GetSize());
        std::stack<BaseVector*> args;
        args.push(inputVec);
        ASSERT_NO_THROW(fn->Apply(args, outputType, result, &ctx));
    }

    static void ValidateNullResult(BaseVector* result, int32_t row) {
        auto* arrayResult = dynamic_cast<ArrayVector*>(result);
        ASSERT_NE(arrayResult, nullptr);
        EXPECT_TRUE(arrayResult->IsNull(row));
    }

    static void ValidateArrayResult(BaseVector* result, int32_t row,
                                    const std::vector<std::string>& expectedKeys) {
        auto* arrayResult = dynamic_cast<ArrayVector*>(result);
        ASSERT_NE(arrayResult, nullptr);
        EXPECT_FALSE(arrayResult->IsNull(row));

        int64_t arraySize = arrayResult->GetSize(row);
        ASSERT_EQ(arraySize, static_cast<int64_t>(expectedKeys.size()));

        auto elementVector = arrayResult->GetElementVector();
        ASSERT_NE(elementVector, nullptr);
        auto* strElementVec =
            dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(elementVector.get());
        ASSERT_NE(strElementVec, nullptr);

        int64_t startIdx = arrayResult->GetOffset(row);
        for (size_t i = 0; i < expectedKeys.size(); ++i) {
            int64_t globalIdx = startIdx + static_cast<int64_t>(i);
            std::string_view actual = strElementVec->GetValue(globalIdx);
            EXPECT_EQ(std::string(actual), expectedKeys[i]);
        }
    }
};

TEST(JsonObjectKeysTest, BasicObject) {
    std::vector<std::string> inputs = {R"({"name": "Alice", "age": 5, "id": "001"})"};
    BaseVector* inputVec = JsonObjectKeysTestHelper::CreateStringVector(inputs);
    BaseVector* result = nullptr;
    JsonObjectKeysTestHelper::ExecuteJsonObjectKeys(inputVec, result);
    JsonObjectKeysTestHelper::ValidateArrayResult(result, 0, {"name", "age", "id"});
    delete result;
}

TEST(JsonObjectKeysTest, EmptyObject) {
    std::vector<std::string> inputs = {R"({})"};
    BaseVector* inputVec = JsonObjectKeysTestHelper::CreateStringVector(inputs);
    BaseVector* result = nullptr;
    JsonObjectKeysTestHelper::ExecuteJsonObjectKeys(inputVec, result);
    JsonObjectKeysTestHelper::ValidateArrayResult(result, 0, {});
    delete result;
}

TEST(JsonObjectKeysTest, NonObjectNumber) {
    std::vector<std::string> inputs = {R"(1)"};
    BaseVector* inputVec = JsonObjectKeysTestHelper::CreateStringVector(inputs);
    BaseVector* result = nullptr;
    JsonObjectKeysTestHelper::ExecuteJsonObjectKeys(inputVec, result);
    JsonObjectKeysTestHelper::ValidateNullResult(result, 0);
    delete result;
}

TEST(JsonObjectKeysTest, NonObjectString) {
    std::vector<std::string> inputs = {R"("hello")"};
    BaseVector* inputVec = JsonObjectKeysTestHelper::CreateStringVector(inputs);
    BaseVector* result = nullptr;
    JsonObjectKeysTestHelper::ExecuteJsonObjectKeys(inputVec, result);
    JsonObjectKeysTestHelper::ValidateNullResult(result, 0);
    delete result;
}

TEST(JsonObjectKeysTest, InvalidJson) {
    std::vector<std::string> inputs = {R"(invalid json)"};
    BaseVector* inputVec = JsonObjectKeysTestHelper::CreateStringVector(inputs);
    BaseVector* result = nullptr;
    JsonObjectKeysTestHelper::ExecuteJsonObjectKeys(inputVec, result);
    JsonObjectKeysTestHelper::ValidateNullResult(result, 0);
    delete result;
}

TEST(JsonObjectKeysTest, EmptyString) {
    std::vector<std::string> inputs = {R"("")"};
    BaseVector* inputVec = JsonObjectKeysTestHelper::CreateStringVector(inputs);
    BaseVector* result = nullptr;
    JsonObjectKeysTestHelper::ExecuteJsonObjectKeys(inputVec, result);
    JsonObjectKeysTestHelper::ValidateNullResult(result, 0);
    delete result;
}

TEST(JsonObjectKeysTest, JsonArray) {
    std::vector<std::string> inputs = {R"([1, 2, 3])"};
    BaseVector* inputVec = JsonObjectKeysTestHelper::CreateStringVector(inputs);
    BaseVector* result = nullptr;
    JsonObjectKeysTestHelper::ExecuteJsonObjectKeys(inputVec, result);
    JsonObjectKeysTestHelper::ValidateNullResult(result, 0);
    delete result;
}

TEST(JsonObjectKeysTest, NullPropagation) {
    std::vector<std::string> inputs = {R"({"key": "value"})", R"({"a": 1})"};
    BaseVector* inputVec = JsonObjectKeysTestHelper::CreateStringVector(inputs);
    inputVec->SetNull(0);
    BaseVector* result = nullptr;
    JsonObjectKeysTestHelper::ExecuteJsonObjectKeys(inputVec, result);
    JsonObjectKeysTestHelper::ValidateNullResult(result, 0);
    JsonObjectKeysTestHelper::ValidateArrayResult(result, 1, {"a"});
    delete result;
}

TEST(JsonObjectKeysTest, NestedObject) {
    std::vector<std::string> inputs = {R"({"outer": {"inner": 1}, "list": [1,2]})"};
    BaseVector* inputVec = JsonObjectKeysTestHelper::CreateStringVector(inputs);
    BaseVector* result = nullptr;
    JsonObjectKeysTestHelper::ExecuteJsonObjectKeys(inputVec, result);
    JsonObjectKeysTestHelper::ValidateArrayResult(result, 0, {"outer", "list"});
    delete result;
}

TEST(JsonObjectKeysTest, MultipleRows) {
    std::vector<std::string> inputs = {
        R"({"name": "Alice", "age": 5})",
        R"({})",
        R"(not json)",
        R"({"x": 1})"
    };
    BaseVector* inputVec = JsonObjectKeysTestHelper::CreateStringVector(inputs);
    BaseVector* result = nullptr;
    JsonObjectKeysTestHelper::ExecuteJsonObjectKeys(inputVec, result);
    JsonObjectKeysTestHelper::ValidateArrayResult(result, 0, {"name", "age"});
    JsonObjectKeysTestHelper::ValidateArrayResult(result, 1, {});
    JsonObjectKeysTestHelper::ValidateNullResult(result, 2);
    JsonObjectKeysTestHelper::ValidateArrayResult(result, 3, {"x"});
    delete result;
}

TEST(JsonObjectKeysTest, MalformedJsonObject) {
    std::vector<std::string> inputs = {R"({"key": 45, "random_string"})"};
    BaseVector* inputVec = JsonObjectKeysTestHelper::CreateStringVector(inputs);
    BaseVector* result = nullptr;
    JsonObjectKeysTestHelper::ExecuteJsonObjectKeys(inputVec, result);
    JsonObjectKeysTestHelper::ValidateNullResult(result, 0);
    delete result;
}

TEST(JsonObjectKeysTest, UnclosedString) {
    std::vector<std::string> inputs = {R"({"key: 45})"};
    BaseVector* inputVec = JsonObjectKeysTestHelper::CreateStringVector(inputs);
    BaseVector* result = nullptr;
    JsonObjectKeysTestHelper::ExecuteJsonObjectKeys(inputVec, result);
    JsonObjectKeysTestHelper::ValidateNullResult(result, 0);
    delete result;
}

TEST(JsonObjectKeysTest, IncompleteArrayInObject) {
    std::vector<std::string> inputs = {R"({ "pie": true, "cherry": [1, 2, 3 })"};
    BaseVector* inputVec = JsonObjectKeysTestHelper::CreateStringVector(inputs);
    BaseVector* result = nullptr;
    JsonObjectKeysTestHelper::ExecuteJsonObjectKeys(inputVec, result);
    JsonObjectKeysTestHelper::ValidateNullResult(result, 0);
    delete result;
}
