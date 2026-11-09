/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: to_json function unit tests
 */

#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <stack>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/VectorFunction.h"
#include "vectorization/functions/ToJson.h"
#include "codegen/func_signature.h"
#include "vector/vector_helper.h"
#include "vector/vector.h"
#include "vector/row_vector.h"
#include "vector/array_vector.h"
#include "vector/map_vector.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::mem;
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::codegen;
using namespace omniruntime::TestUtil;

class ToJsonTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const to_json_test_env =
    ::testing::AddGlobalTestEnvironment(new ToJsonTestEnvironment);

class ToJsonTestHelper {
public:
    static BaseVector* CreateRowVector(int rowSize,
        const std::vector<DataTypeId>& fieldTypes,
        const std::vector<std::vector<int32_t>>& intValues) {
        auto* rowVec = new RowVector(rowSize);
        for (size_t f = 0; f < fieldTypes.size(); ++f) {
            auto* col = VectorHelper::CreateFlatVector(fieldTypes[f], rowSize);
            auto* intVec = static_cast<Vector<int32_t>*>(col);
            for (int r = 0; r < rowSize; ++r) {
                intVec->SetValue(r, intValues[f][r]);
            }
            rowVec->AddChild(std::shared_ptr<BaseVector>(col));
        }
        rowVec->SetIsField(true);
        return rowVec;
    }

    static BaseVector* CreateArrayVector(int rowSize,
        const std::vector<std::vector<int64_t>>& arrayData) {
        std::vector<int64_t> offsets = {0};
        size_t totalElements = 0;
        std::vector<int64_t> allElements;
        for (const auto& arr : arrayData) {
            for (int64_t v : arr) allElements.push_back(v);
            totalElements += arr.size();
            offsets.push_back(static_cast<int64_t>(totalElements));
        }
        auto elemVec = VectorHelper::CreateFlatVector(OMNI_LONG, static_cast<int32_t>(allElements.size()));
        auto* longVec = static_cast<Vector<int64_t>*>(elemVec);
        for (size_t i = 0; i < allElements.size(); ++i) longVec->SetValue(i, allElements[i]);
        auto* arrVec = new ArrayVector(rowSize, std::shared_ptr<BaseVector>(elemVec));
        arrVec->SetIsField(true);
        for (int i = 0; i <= rowSize; ++i) arrVec->SetOffset(i, static_cast<int32_t>(offsets[i]));
        return arrVec;
    }

    static BaseVector* CreateMapVector(int rowSize,
        const std::vector<std::string>& allKeys,
        const std::vector<int64_t>& allValues,
        const std::vector<int64_t>& offsets) {
        auto keyVec = VectorHelper::CreateStringVector(static_cast<int32_t>(allKeys.size()));
        keyVec->SetIsField(true);
        auto* keyStrVec = static_cast<Vector<LargeStringContainer<std::string_view>>*>(keyVec);
        for (size_t i = 0; i < allKeys.size(); ++i) keyStrVec->SetValue(i, std::string_view(allKeys[i]));
        auto valVec = VectorHelper::CreateFlatVector(OMNI_LONG, static_cast<int32_t>(allValues.size()));
        auto* valLongVec = static_cast<Vector<int64_t>*>(valVec);
        for (size_t i = 0; i < allValues.size(); ++i) valLongVec->SetValue(i, allValues[i]);
        auto* mapVec = new MapVector(rowSize);
        mapVec->SetIsField(true);
        mapVec->SetKeyVector(std::shared_ptr<BaseVector>(keyVec));
        mapVec->SetValueVector(std::shared_ptr<BaseVector>(valVec));
        for (int i = 0; i <= rowSize; ++i) mapVec->SetOffset(i, static_cast<int32_t>(offsets[i]));
        return mapVec;
    }

    static void ExecuteToJson(BaseVector* inputVec, DataTypeId inputType, BaseVector*& result) {
        std::vector<DataTypeId> inputTypeIds = {inputType};
        auto sig = std::make_shared<FunctionSignature>("to_json", inputTypeIds, OMNI_VARCHAR);
        auto fn = VectorFunction::Find(sig);
        ASSERT_NE(fn, nullptr);
        auto outputType = std::make_shared<DataType>(OMNI_VARCHAR);
        ExecutionContext ctx;
        ctx.SetResultRowSize(inputVec->GetSize());
        std::stack<BaseVector*> args;
        inputVec->SetIsField(true);  // caller-owned, do not let Apply delete
        args.push(inputVec);
        ASSERT_NO_THROW(fn->Apply(args, outputType, result, &ctx));
    }

    static void ExecuteToJsonWithType(BaseVector* inputVec, DataTypeId inputType,
        const DataType* inputDataType, BaseVector*& result) {
        std::vector<DataTypeId> inputTypeIds = {inputType};
        auto sig = std::make_shared<FunctionSignature>("to_json", inputTypeIds, OMNI_VARCHAR);
        auto fn = VectorFunction::Find(sig);
        ASSERT_NE(fn, nullptr);
        auto outputType = std::make_shared<DataType>(OMNI_VARCHAR);
        ExecutionContext ctx;
        ctx.SetResultRowSize(inputVec->GetSize());
        ctx.SetToJsonInputType(inputDataType);
        std::stack<BaseVector*> args;
        inputVec->SetIsField(true);  // caller-owned, do not let Apply delete
        args.push(inputVec);
        ASSERT_NO_THROW(fn->Apply(args, outputType, result, &ctx));
    }

    static void ValidateStringResult(BaseVector* result, int row, const std::string& expected) {
        auto* strVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
        ASSERT_NE(strVec, nullptr);
        ASSERT_FALSE(strVec->IsNull(row));
        std::string actual(strVec->GetValue(row));
        EXPECT_EQ(actual, expected);
    }
};

TEST(ToJsonTest, RowStruct) {
    std::vector<std::vector<int32_t>> intVals = {{1, 2}, {10, 20}};
    BaseVector* rowVec = ToJsonTestHelper::CreateRowVector(2, {OMNI_INT, OMNI_INT}, intVals);
    BaseVector* result = nullptr;
    ToJsonTestHelper::ExecuteToJson(rowVec, OMNI_ROW, result);
    ToJsonTestHelper::ValidateStringResult(result, 0, R"({"field0":1,"field1":10})");
    ToJsonTestHelper::ValidateStringResult(result, 1, R"({"field0":2,"field1":20})");
    delete rowVec;
    delete result;
}

// When the input RowType carries field names, to_json must emit them (not field0/field1),
// matching native Spark: to_json(struct<name,age>) -> {"name":..,"age":..}.
TEST(ToJsonTest, RowStructWithFieldNames) {
    std::vector<std::vector<int32_t>> intVals = {{1, 2}, {10, 20}};
    BaseVector* rowVec = ToJsonTestHelper::CreateRowVector(2, {OMNI_INT, OMNI_INT}, intVals);
    std::vector<std::shared_ptr<DataType>> fieldTypes = {
        std::make_shared<DataType>(OMNI_INT), std::make_shared<DataType>(OMNI_INT)};
    std::vector<std::string> fieldNames = {"name", "age"};
    auto rowType = std::make_shared<RowType>(fieldTypes, fieldNames);
    BaseVector* result = nullptr;
    ToJsonTestHelper::ExecuteToJsonWithType(rowVec, OMNI_ROW, rowType.get(), result);
    ToJsonTestHelper::ValidateStringResult(result, 0, R"({"name":1,"age":10})");
    ToJsonTestHelper::ValidateStringResult(result, 1, R"({"name":2,"age":20})");
    delete rowVec;
    delete result;
}

// Spark to_json (ignoreNullFields=true default) omits struct fields whose value is null.
TEST(ToJsonTest, RowStructOmitsNullFields) {
    std::vector<std::vector<int32_t>> intVals = {{1}, {99}};
    BaseVector* rowVec = ToJsonTestHelper::CreateRowVector(1, {OMNI_INT, OMNI_INT}, intVals);
    dynamic_cast<RowVector*>(rowVec)->ChildAt(0)->SetNull(0);  // field "a" is null
    std::vector<std::shared_ptr<DataType>> fieldTypes = {
        std::make_shared<DataType>(OMNI_INT), std::make_shared<DataType>(OMNI_INT)};
    std::vector<std::string> fieldNames = {"a", "b"};
    auto rowType = std::make_shared<RowType>(fieldTypes, fieldNames);
    BaseVector* result = nullptr;
    ToJsonTestHelper::ExecuteToJsonWithType(rowVec, OMNI_ROW, rowType.get(), result);
    ToJsonTestHelper::ValidateStringResult(result, 0, R"({"b":99})");
    delete rowVec;
    delete result;
}

// All struct fields null -> empty JSON object.
TEST(ToJsonTest, RowStructAllNullFields) {
    std::vector<std::vector<int32_t>> intVals = {{1}, {2}};
    BaseVector* rowVec = ToJsonTestHelper::CreateRowVector(1, {OMNI_INT, OMNI_INT}, intVals);
    dynamic_cast<RowVector*>(rowVec)->ChildAt(0)->SetNull(0);
    dynamic_cast<RowVector*>(rowVec)->ChildAt(1)->SetNull(0);
    std::vector<std::shared_ptr<DataType>> fieldTypes = {
        std::make_shared<DataType>(OMNI_INT), std::make_shared<DataType>(OMNI_INT)};
    std::vector<std::string> fieldNames = {"a", "b"};
    auto rowType = std::make_shared<RowType>(fieldTypes, fieldNames);
    BaseVector* result = nullptr;
    ToJsonTestHelper::ExecuteToJsonWithType(rowVec, OMNI_ROW, rowType.get(), result);
    ToJsonTestHelper::ValidateStringResult(result, 0, "{}");
    delete rowVec;
    delete result;
}

TEST(ToJsonTest, Array) {
    std::vector<std::vector<int64_t>> arrayData = {{1, 2, 3}, {10, 20}};
    BaseVector* arrVec = ToJsonTestHelper::CreateArrayVector(2, arrayData);
    BaseVector* result = nullptr;
    ToJsonTestHelper::ExecuteToJson(arrVec, OMNI_ARRAY, result);
    ToJsonTestHelper::ValidateStringResult(result, 0, "[1,2,3]");
    ToJsonTestHelper::ValidateStringResult(result, 1, "[10,20]");
    delete arrVec;
    delete result;
}

TEST(ToJsonTest, Map) {
    std::vector<std::string> keys = {"a", "b", "c"};
    std::vector<int64_t> values = {1, 2, 3};
    std::vector<int64_t> offsets = {0, 2, 3};
    BaseVector* mapVec = ToJsonTestHelper::CreateMapVector(2, keys, values, offsets);
    BaseVector* result = nullptr;
    ToJsonTestHelper::ExecuteToJson(mapVec, OMNI_MAP, result);
    std::string r0 = std::string(dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result)->GetValue(0));
    EXPECT_TRUE(r0 == R"({"a":1,"b":2})" || r0 == R"({"b":2,"a":1})");
    ToJsonTestHelper::ValidateStringResult(result, 1, R"({"c":3})");
    delete mapVec;
    delete result;
}

TEST(ToJsonTest, RowNullPropagation) {
    std::vector<std::vector<int32_t>> intVals = {{1}, {42}};
    BaseVector* rowVec = ToJsonTestHelper::CreateRowVector(1, {OMNI_INT, OMNI_INT}, intVals);
    rowVec->SetNull(0);
    BaseVector* result = nullptr;
    ToJsonTestHelper::ExecuteToJson(rowVec, OMNI_ROW, result);
    EXPECT_TRUE(dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result)->IsNull(0));
    delete rowVec;
    delete result;
}

TEST(ToJsonTest, EmptyArray) {
    std::vector<std::vector<int64_t>> arrayData = {{}};
    BaseVector* arrVec = ToJsonTestHelper::CreateArrayVector(1, arrayData);
    BaseVector* result = nullptr;
    ToJsonTestHelper::ExecuteToJson(arrVec, OMNI_ARRAY, result);
    ToJsonTestHelper::ValidateStringResult(result, 0, "[]");
    delete arrVec;
    delete result;
}

TEST(ToJsonTest, EmptyMap) {
    std::vector<std::string> keys;
    std::vector<int64_t> values;
    std::vector<int64_t> offsets = {0, 0};
    BaseVector* mapVec = ToJsonTestHelper::CreateMapVector(1, keys, values, offsets);
    BaseVector* result = nullptr;
    ToJsonTestHelper::ExecuteToJson(mapVec, OMNI_MAP, result);
    ToJsonTestHelper::ValidateStringResult(result, 0, "{}");
    delete mapVec;
    delete result;
}
