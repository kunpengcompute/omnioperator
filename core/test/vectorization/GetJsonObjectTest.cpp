/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: get_json_object function unit tests
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
 
 class GetJsonObjectTestEnvironment : public ::testing::Environment {
 public:
     void SetUp() override {
         RegisterFunctions::RegisterAllFunctions("");
     }
 };
 
 ::testing::Environment* const get_json_object_test_env =
     ::testing::AddGlobalTestEnvironment(new GetJsonObjectTestEnvironment);
 
 class GetJsonObjectTestHelper {
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
 
     static void ExecuteGetJsonObject(BaseVector* jsonVec, BaseVector* pathVec, BaseVector*& result) {
         std::vector<DataTypeId> inputTypeIds = {OMNI_VARCHAR, OMNI_VARCHAR};
         auto sig = std::make_shared<FunctionSignature>("get_json_object", inputTypeIds, OMNI_VARCHAR);
         auto fn = VectorFunction::Find(sig);
         ASSERT_NE(fn, nullptr);
         auto outputType = std::make_shared<DataType>(OMNI_VARCHAR);
         ExecutionContext ctx(4096, nullptr);
         ctx.SetResultRowSize(jsonVec->GetSize());
         std::stack<BaseVector*> args;
         args.push(jsonVec);
         args.push(pathVec);
         ASSERT_NO_THROW(fn->Apply(args, outputType, result, &ctx));
     }
 
     static void ValidateNullResult(BaseVector* result, int32_t row) {
         auto* strResult = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
         ASSERT_NE(strResult, nullptr);
         EXPECT_TRUE(strResult->IsNull(row));
     }
 
     static void ValidateStringResult(BaseVector* result, int32_t row, const std::string& expected) {
         auto* strResult = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
         ASSERT_NE(strResult, nullptr);
         EXPECT_FALSE(strResult->IsNull(row));
         std::string_view sv = strResult->GetValue(row);
         EXPECT_EQ(std::string(sv), expected);
     }
 };
 
 // Basic tests matching user's requirements
 TEST(GetJsonObjectTest, BasicFieldAccess) {
     std::vector<std::string> jsonInputs = {R"({"a": "hello"})"};
     std::vector<std::string> pathInputs = {"$.a"};
     BaseVector* jsonVec = GetJsonObjectTestHelper::CreateStringVector(jsonInputs);
     BaseVector* pathVec = GetJsonObjectTestHelper::CreateStringVector(pathInputs);
     BaseVector* result = nullptr;
     GetJsonObjectTestHelper::ExecuteGetJsonObject(jsonVec, pathVec, result);
     GetJsonObjectTestHelper::ValidateStringResult(result, 0, "hello");
     delete result;
 }
 
 TEST(GetJsonObjectTest, NestedObjectAccess) {
     std::vector<std::string> jsonInputs = {R"({"a": {"b": "c"}})"};
     std::vector<std::string> pathInputs = {"$.a"};
     BaseVector* jsonVec = GetJsonObjectTestHelper::CreateStringVector(jsonInputs);
     BaseVector* pathVec = GetJsonObjectTestHelper::CreateStringVector(pathInputs);
     BaseVector* result = nullptr;
     GetJsonObjectTestHelper::ExecuteGetJsonObject(jsonVec, pathVec, result);
     GetJsonObjectTestHelper::ValidateStringResult(result, 0, R"({"b":"c"})");
     delete result;
 }
 
 TEST(GetJsonObjectTest, NonExistentFieldReturnsNull) {
     std::vector<std::string> jsonInputs = {R"({"a": 3})"};
     std::vector<std::string> pathInputs = {"$.b"};
     BaseVector* jsonVec = GetJsonObjectTestHelper::CreateStringVector(jsonInputs);
     BaseVector* pathVec = GetJsonObjectTestHelper::CreateStringVector(pathInputs);
     BaseVector* result = nullptr;
     GetJsonObjectTestHelper::ExecuteGetJsonObject(jsonVec, pathVec, result);
     GetJsonObjectTestHelper::ValidateNullResult(result, 0);
     delete result;
 }
 
 TEST(GetJsonObjectTest, RootPathReturnsFullJson) {
     std::vector<std::string> jsonInputs = {R"({"a": "hello"})"};
     std::vector<std::string> pathInputs = {"$"};
     BaseVector* jsonVec = GetJsonObjectTestHelper::CreateStringVector(jsonInputs);
     BaseVector* pathVec = GetJsonObjectTestHelper::CreateStringVector(pathInputs);
     BaseVector* result = nullptr;
     GetJsonObjectTestHelper::ExecuteGetJsonObject(jsonVec, pathVec, result);
     // When path is "$", return the original JSON string (preserving format)
     GetJsonObjectTestHelper::ValidateStringResult(result, 0, R"({"a": "hello"})");
     delete result;
 }
 
 TEST(GetJsonObjectTest, InvalidJsonReturnsNull) {
     std::vector<std::string> jsonInputs = {R"({"a"-3})"};
     std::vector<std::string> pathInputs = {"$.a"};
     BaseVector* jsonVec = GetJsonObjectTestHelper::CreateStringVector(jsonInputs);
     BaseVector* pathVec = GetJsonObjectTestHelper::CreateStringVector(pathInputs);
     BaseVector* result = nullptr;
     GetJsonObjectTestHelper::ExecuteGetJsonObject(jsonVec, pathVec, result);
     GetJsonObjectTestHelper::ValidateNullResult(result, 0);
     delete result;
 }
 
 TEST(GetJsonObjectTest, ArrayElementAccess) {
     std::vector<std::string> jsonInputs = {R"({"a": [1, 2, 3]})"};
     std::vector<std::string> pathInputs = {"$.a[0]"};
     BaseVector* jsonVec = GetJsonObjectTestHelper::CreateStringVector(jsonInputs);
     BaseVector* pathVec = GetJsonObjectTestHelper::CreateStringVector(pathInputs);
     BaseVector* result = nullptr;
     GetJsonObjectTestHelper::ExecuteGetJsonObject(jsonVec, pathVec, result);
     GetJsonObjectTestHelper::ValidateStringResult(result, 0, "1");
     delete result;
 }
 
 // Additional tests for comprehensive coverage
 TEST(GetJsonObjectTest, AccessNumberField) {
     std::vector<std::string> jsonInputs = {R"({"hello": 3.5})"};
     std::vector<std::string> pathInputs = {"$.hello"};
     BaseVector* jsonVec = GetJsonObjectTestHelper::CreateStringVector(jsonInputs);
     BaseVector* pathVec = GetJsonObjectTestHelper::CreateStringVector(pathInputs);
     BaseVector* result = nullptr;
     GetJsonObjectTestHelper::ExecuteGetJsonObject(jsonVec, pathVec, result);
     GetJsonObjectTestHelper::ValidateStringResult(result, 0, "3.5");
     delete result;
 }
 
 TEST(GetJsonObjectTest, AccessBooleanField) {
     std::vector<std::string> jsonInputs = {R"({"flag": true})"};
     std::vector<std::string> pathInputs = {"$.flag"};
     BaseVector* jsonVec = GetJsonObjectTestHelper::CreateStringVector(jsonInputs);
     BaseVector* pathVec = GetJsonObjectTestHelper::CreateStringVector(pathInputs);
     BaseVector* result = nullptr;
     GetJsonObjectTestHelper::ExecuteGetJsonObject(jsonVec, pathVec, result);
     GetJsonObjectTestHelper::ValidateStringResult(result, 0, "true");
     delete result;
 }
 
 TEST(GetJsonObjectTest, AccessNestedDotNotation) {
     std::vector<std::string> jsonInputs = {R"({"my": {"hello": 3.5}})"};
     std::vector<std::string> pathInputs = {"$.my.hello"};
     BaseVector* jsonVec = GetJsonObjectTestHelper::CreateStringVector(jsonInputs);
     BaseVector* pathVec = GetJsonObjectTestHelper::CreateStringVector(pathInputs);
     BaseVector* result = nullptr;
     GetJsonObjectTestHelper::ExecuteGetJsonObject(jsonVec, pathVec, result);
     GetJsonObjectTestHelper::ValidateStringResult(result, 0, "3.5");
     delete result;
 }
 
 TEST(GetJsonObjectTest, MixedNotation) {
     std::vector<std::string> jsonInputs = {R"({"my": {"info": {"name": "Alice", "age": "5"}}})"};
     std::vector<std::string> pathInputs = {"$.my.info.age"};
     BaseVector* jsonVec = GetJsonObjectTestHelper::CreateStringVector(jsonInputs);
     BaseVector* pathVec = GetJsonObjectTestHelper::CreateStringVector(pathInputs);
     BaseVector* result = nullptr;
     GetJsonObjectTestHelper::ExecuteGetJsonObject(jsonVec, pathVec, result);
     GetJsonObjectTestHelper::ValidateStringResult(result, 0, "5");
     delete result;
 }
 
 TEST(GetJsonObjectTest, MultipleArrayElements) {
     std::vector<std::string> jsonInputs = {R"([{"v": 1}, {"v": 2}, {"v": 3}])"};
     std::vector<std::string> pathInputs = {"$[1].v"};
     BaseVector* jsonVec = GetJsonObjectTestHelper::CreateStringVector(jsonInputs);
     BaseVector* pathVec = GetJsonObjectTestHelper::CreateStringVector(pathInputs);
     BaseVector* result = nullptr;
     GetJsonObjectTestHelper::ExecuteGetJsonObject(jsonVec, pathVec, result);
     GetJsonObjectTestHelper::ValidateStringResult(result, 0, "2");
     delete result;
 }
 
 TEST(GetJsonObjectTest, EmptyStringField) {
     std::vector<std::string> jsonInputs = {R"({"hello": ""})"};
     std::vector<std::string> pathInputs = {"$.hello"};
     BaseVector* jsonVec = GetJsonObjectTestHelper::CreateStringVector(jsonInputs);
     BaseVector* pathVec = GetJsonObjectTestHelper::CreateStringVector(pathInputs);
     BaseVector* result = nullptr;
     GetJsonObjectTestHelper::ExecuteGetJsonObject(jsonVec, pathVec, result);
     GetJsonObjectTestHelper::ValidateStringResult(result, 0, "");
     delete result;
 }
 
 TEST(GetJsonObjectTest, InvalidPathNoDollar) {
     std::vector<std::string> jsonInputs = {R"({"hello": "3.5"})"};
     std::vector<std::string> pathInputs = {".hello"};
     BaseVector* jsonVec = GetJsonObjectTestHelper::CreateStringVector(jsonInputs);
     BaseVector* pathVec = GetJsonObjectTestHelper::CreateStringVector(pathInputs);
     BaseVector* result = nullptr;
     GetJsonObjectTestHelper::ExecuteGetJsonObject(jsonVec, pathVec, result);
     GetJsonObjectTestHelper::ValidateNullResult(result, 0);
     delete result;
 }
 
 TEST(GetJsonObjectTest, InvalidPathTrailingDot) {
     std::vector<std::string> jsonInputs = {R"({"hello": "3.5"})"};
     std::vector<std::string> pathInputs = {"$."};
     BaseVector* jsonVec = GetJsonObjectTestHelper::CreateStringVector(jsonInputs);
     BaseVector* pathVec = GetJsonObjectTestHelper::CreateStringVector(pathInputs);
     BaseVector* result = nullptr;
     GetJsonObjectTestHelper::ExecuteGetJsonObject(jsonVec, pathVec, result);
     GetJsonObjectTestHelper::ValidateNullResult(result, 0);
     delete result;
 }
 
 TEST(GetJsonObjectTest, NullPropagation) {
     std::vector<std::string> jsonInputs = {R"({"a": "1"})", R"({"a": "2"})"};
     std::vector<std::string> pathInputs = {"$.a", "$.a"};
     BaseVector* jsonVec = GetJsonObjectTestHelper::CreateStringVector(jsonInputs);
     BaseVector* pathVec = GetJsonObjectTestHelper::CreateStringVector(pathInputs);
     jsonVec->SetNull(0);
     BaseVector* result = nullptr;
     GetJsonObjectTestHelper::ExecuteGetJsonObject(jsonVec, pathVec, result);
     GetJsonObjectTestHelper::ValidateNullResult(result, 0);
     GetJsonObjectTestHelper::ValidateStringResult(result, 1, "2");
     delete result;
 }
 
 TEST(GetJsonObjectTest, ArrayOutOfBounds) {
     std::vector<std::string> jsonInputs = {R"({"a": [1, 2]})"};
     std::vector<std::string> pathInputs = {"$.a[5]"};
     BaseVector* jsonVec = GetJsonObjectTestHelper::CreateStringVector(jsonInputs);
     BaseVector* pathVec = GetJsonObjectTestHelper::CreateStringVector(pathInputs);
     BaseVector* result = nullptr;
     GetJsonObjectTestHelper::ExecuteGetJsonObject(jsonVec, pathVec, result);
     GetJsonObjectTestHelper::ValidateNullResult(result, 0);
     delete result;
 }
 
 TEST(GetJsonObjectTest, ArrayOnNonArray) {
     std::vector<std::string> jsonInputs = {R"({"a": "not array"})"};
     std::vector<std::string> pathInputs = {"$.a[0]"};
     BaseVector* jsonVec = GetJsonObjectTestHelper::CreateStringVector(jsonInputs);
     BaseVector* pathVec = GetJsonObjectTestHelper::CreateStringVector(pathInputs);
     BaseVector* result = nullptr;
     GetJsonObjectTestHelper::ExecuteGetJsonObject(jsonVec, pathVec, result);
     GetJsonObjectTestHelper::ValidateNullResult(result, 0);
     delete result;
 }
 
 TEST(GetJsonObjectTest, ReturnArray) {
     std::vector<std::string> jsonInputs = {R"({"arr": [1, 2, 3]})"};
     std::vector<std::string> pathInputs = {"$.arr"};
     BaseVector* jsonVec = GetJsonObjectTestHelper::CreateStringVector(jsonInputs);
     BaseVector* pathVec = GetJsonObjectTestHelper::CreateStringVector(pathInputs);
     BaseVector* result = nullptr;
     GetJsonObjectTestHelper::ExecuteGetJsonObject(jsonVec, pathVec, result);
     GetJsonObjectTestHelper::ValidateStringResult(result, 0, "[1,2,3]");
     delete result;
 }
 
 TEST(GetJsonObjectTest, MultipleRows) {
     std::vector<std::string> jsonInputs = {
         R"({"name": "Alice", "age": 30})",
         R"({"name": "Bob", "age": 25})",
         R"({"name": "Charlie"})"
     };
     std::vector<std::string> pathInputs = {"$.name", "$.name", "$.name"};
     BaseVector* jsonVec = GetJsonObjectTestHelper::CreateStringVector(jsonInputs);
     BaseVector* pathVec = GetJsonObjectTestHelper::CreateStringVector(pathInputs);
     BaseVector* result = nullptr;
     GetJsonObjectTestHelper::ExecuteGetJsonObject(jsonVec, pathVec, result);
     GetJsonObjectTestHelper::ValidateStringResult(result, 0, "Alice");
     GetJsonObjectTestHelper::ValidateStringResult(result, 1, "Bob");
     GetJsonObjectTestHelper::ValidateStringResult(result, 2, "Charlie");
     delete result;
 }
 
 TEST(GetJsonObjectTest, MultipleRowsDifferentPaths) {
     std::vector<std::string> jsonInputs = {
         R"({"a": 1, "b": 2})",
         R"({"a": 3, "b": 4})",
         R"({"a": 5, "b": 6})"
     };
     std::vector<std::string> pathInputs = {"$.a", "$.b", "$.a"};
     BaseVector* jsonVec = GetJsonObjectTestHelper::CreateStringVector(jsonInputs);
     BaseVector* pathVec = GetJsonObjectTestHelper::CreateStringVector(pathInputs);
     BaseVector* result = nullptr;
     GetJsonObjectTestHelper::ExecuteGetJsonObject(jsonVec, pathVec, result);
     GetJsonObjectTestHelper::ValidateStringResult(result, 0, "1");
     GetJsonObjectTestHelper::ValidateStringResult(result, 1, "4");
     GetJsonObjectTestHelper::ValidateStringResult(result, 2, "5");
     delete result;
 }
 