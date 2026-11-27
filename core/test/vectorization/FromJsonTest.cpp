/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: from_json function unit tests
 */

#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/VectorFunction.h"
#include "codegen/func_signature.h"
#include "vector/vector_helper.h"
#include "vector/vector.h"
#include "type/data_type.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::mem;
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::codegen;
using namespace omniruntime::TestUtil;

class FromJsonTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const from_json_test_env =
    ::testing::AddGlobalTestEnvironment(new FromJsonTestEnvironment);

class FromJsonTestHelper {
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

    static DataTypePtr CreateRowTypeWithStringField(const std::string& fieldName) {
        std::vector<std::string> names = {fieldName};
        std::vector<DataTypePtr> types = {VarcharType()};
        return std::make_shared<RowType>(types, names);
    }

    static void ExecuteFromJson(BaseVector* inputVec, const DataTypePtr& outputType, BaseVector*& result) {
        std::vector<DataTypeId> inputTypeIds = {OMNI_VARCHAR};
        auto sig = std::make_shared<FunctionSignature>("from_json", inputTypeIds, OMNI_ROW);
        auto fn = VectorFunction::Find(sig);
        ASSERT_NE(fn, nullptr);
        ExecutionContext ctx(4096, nullptr);
        ctx.SetResultRowSize(inputVec->GetSize());
        std::stack<BaseVector*> args;
        args.push(inputVec);
        ASSERT_NO_THROW(fn->Apply(args, outputType, result, &ctx));
    }

    static void ValidateNullResult(BaseVector* result, int32_t row) {
        auto* rowResult = dynamic_cast<RowVector*>(result);
        ASSERT_NE(rowResult, nullptr);
        EXPECT_TRUE(rowResult->IsNull(row));
    }

    static void ValidateStringField(BaseVector* result, int32_t row, int32_t fieldIdx, const std::string& expected) {
        auto* rowResult = dynamic_cast<RowVector*>(result);
        ASSERT_NE(rowResult, nullptr);
        EXPECT_FALSE(rowResult->IsNull(row));
        
        BaseVector* fieldVec = rowResult->ChildAt(fieldIdx).get();
        auto* stringVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(fieldVec);
        ASSERT_NE(stringVec, nullptr);
        EXPECT_FALSE(stringVec->IsNull(row));
        
        std::string_view actual = stringVec->GetValue(row);
        EXPECT_EQ(std::string(actual), expected);
    }

    static void ValidateNullField(BaseVector* result, int32_t row, int32_t fieldIdx) {
        auto* rowResult = dynamic_cast<RowVector*>(result);
        ASSERT_NE(rowResult, nullptr);
        EXPECT_FALSE(rowResult->IsNull(row));
        
        BaseVector* fieldVec = rowResult->ChildAt(fieldIdx).get();
        EXPECT_TRUE(fieldVec->IsNull(row));
    }
};

TEST(FromJsonTest, BasicStringField) {
    std::vector<std::string> inputs = {R"({"a": "hello"})"};
    BaseVector* inputVec = FromJsonTestHelper::CreateStringVector(inputs);
    DataTypePtr outputType = FromJsonTestHelper::CreateRowTypeWithStringField("a");
    BaseVector* result = nullptr;
    FromJsonTestHelper::ExecuteFromJson(inputVec, outputType, result);
    FromJsonTestHelper::ValidateStringField(result, 0, 0, "hello");
    delete result;
}

TEST(FromJsonTest, EmptyString) {
    std::vector<std::string> inputs = {R"({"a": ""})"};
    BaseVector* inputVec = FromJsonTestHelper::CreateStringVector(inputs);
    DataTypePtr outputType = FromJsonTestHelper::CreateRowTypeWithStringField("a");
    BaseVector* result = nullptr;
    FromJsonTestHelper::ExecuteFromJson(inputVec, outputType, result);
    FromJsonTestHelper::ValidateStringField(result, 0, 0, "");
    delete result;
}

TEST(FromJsonTest, MissingField) {
    std::vector<std::string> inputs = {R"({"b": "value"})"};
    BaseVector* inputVec = FromJsonTestHelper::CreateStringVector(inputs);
    DataTypePtr outputType = FromJsonTestHelper::CreateRowTypeWithStringField("a");
    BaseVector* result = nullptr;
    FromJsonTestHelper::ExecuteFromJson(inputVec, outputType, result);
    FromJsonTestHelper::ValidateNullField(result, 0, 0);
    delete result;
}

TEST(FromJsonTest, NullField) {
    std::vector<std::string> inputs = {R"({"a": null})"};
    BaseVector* inputVec = FromJsonTestHelper::CreateStringVector(inputs);
    DataTypePtr outputType = FromJsonTestHelper::CreateRowTypeWithStringField("a");
    BaseVector* result = nullptr;
    FromJsonTestHelper::ExecuteFromJson(inputVec, outputType, result);
    FromJsonTestHelper::ValidateNullField(result, 0, 0);
    delete result;
}

TEST(FromJsonTest, NumberToString) {
    std::vector<std::string> inputs = {R"({"a": 123})"};
    BaseVector* inputVec = FromJsonTestHelper::CreateStringVector(inputs);
    DataTypePtr outputType = FromJsonTestHelper::CreateRowTypeWithStringField("a");
    BaseVector* result = nullptr;
    FromJsonTestHelper::ExecuteFromJson(inputVec, outputType, result);
    FromJsonTestHelper::ValidateStringField(result, 0, 0, "123");
    delete result;
}

TEST(FromJsonTest, BooleanToString) {
    std::vector<std::string> inputs = {R"({"a": true})"};
    BaseVector* inputVec = FromJsonTestHelper::CreateStringVector(inputs);
    DataTypePtr outputType = FromJsonTestHelper::CreateRowTypeWithStringField("a");
    BaseVector* result = nullptr;
    FromJsonTestHelper::ExecuteFromJson(inputVec, outputType, result);
    FromJsonTestHelper::ValidateStringField(result, 0, 0, "true");
    delete result;
}

TEST(FromJsonTest, InvalidJson) {
    // Invalid JSON should return a row with null fields, not NULL
    std::vector<std::string> inputs = {R"({"a": "hello")"};
    BaseVector* inputVec = FromJsonTestHelper::CreateStringVector(inputs);
    DataTypePtr outputType = FromJsonTestHelper::CreateRowTypeWithStringField("a");
    BaseVector* result = nullptr;
    FromJsonTestHelper::ExecuteFromJson(inputVec, outputType, result);
    // Row should not be null, but field should be null
    FromJsonTestHelper::ValidateNullField(result, 0, 0);
    delete result;
}

TEST(FromJsonTest, NotAnObject) {
    // Not an object should return a row with null fields, not NULL
    std::vector<std::string> inputs = {R"("not an object")"};
    BaseVector* inputVec = FromJsonTestHelper::CreateStringVector(inputs);
    DataTypePtr outputType = FromJsonTestHelper::CreateRowTypeWithStringField("a");
    BaseVector* result = nullptr;
    FromJsonTestHelper::ExecuteFromJson(inputVec, outputType, result);
    // Row should not be null, but field should be null
    FromJsonTestHelper::ValidateNullField(result, 0, 0);
    delete result;
}

TEST(FromJsonTest, InvalidJsonString) {
    // Invalid JSON string should return a row with null fields, not NULL
    std::vector<std::string> inputs = {"invalid json"};
    BaseVector* inputVec = FromJsonTestHelper::CreateStringVector(inputs);
    DataTypePtr outputType = FromJsonTestHelper::CreateRowTypeWithStringField("a");
    BaseVector* result = nullptr;
    FromJsonTestHelper::ExecuteFromJson(inputVec, outputType, result);
    // Row should not be null, but field should be null
    FromJsonTestHelper::ValidateNullField(result, 0, 0);
    delete result;
}

TEST(FromJsonTest, NullInput) {
    std::vector<std::string> inputs = {R"({"a": "hello"})"};
    BaseVector* inputVec = FromJsonTestHelper::CreateStringVector(inputs);
    inputVec->SetNull(0);
    DataTypePtr outputType = FromJsonTestHelper::CreateRowTypeWithStringField("a");
    BaseVector* result = nullptr;
    FromJsonTestHelper::ExecuteFromJson(inputVec, outputType, result);
    FromJsonTestHelper::ValidateNullResult(result, 0);
    delete result;
}

TEST(FromJsonTest, CaseInsensitiveFieldName) {
    std::vector<std::string> inputs = {R"({"A": "hello"})"};
    BaseVector* inputVec = FromJsonTestHelper::CreateStringVector(inputs);
    DataTypePtr outputType = FromJsonTestHelper::CreateRowTypeWithStringField("a");
    BaseVector* result = nullptr;
    FromJsonTestHelper::ExecuteFromJson(inputVec, outputType, result);
    FromJsonTestHelper::ValidateStringField(result, 0, 0, "hello");
    delete result;
}

TEST(FromJsonTest, MultipleRows) {
    std::vector<std::string> inputs = {
        R"({"a": "hello"})",
        R"({"a": "world"})",
        R"({"b": "value"})",
        R"({"a": null})"
    };
    BaseVector* inputVec = FromJsonTestHelper::CreateStringVector(inputs);
    DataTypePtr outputType = FromJsonTestHelper::CreateRowTypeWithStringField("a");
    BaseVector* result = nullptr;
    FromJsonTestHelper::ExecuteFromJson(inputVec, outputType, result);
    FromJsonTestHelper::ValidateStringField(result, 0, 0, "hello");
    FromJsonTestHelper::ValidateStringField(result, 1, 0, "world");
    FromJsonTestHelper::ValidateNullField(result, 2, 0);
    FromJsonTestHelper::ValidateNullField(result, 3, 0);
    delete result;
}
