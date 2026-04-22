/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2021-2024. All rights reserved.
 * Description: json_value function test
 */
#include <string>
#include <vector>
#include "gtest/gtest.h"
#include "codegen/functions/stringfunctions.h"
#include "operator/execution_context.h"
#include "jni_mock.h"
#include "../util/test_util.h"

namespace omniruntime {
using namespace omniruntime::codegen::function;
using namespace omniruntime::op;

// Helper function to test json_value
static void JsonValueTest(const std::string &jsonStr, const std::string &path, 
                         const std::string &expectedResult, bool expectIsNull)
{
    auto context = new ExecutionContext();
    int64_t contextPtr = reinterpret_cast<int64_t>(context);
    
    bool outIsNull = false;
    int32_t outLen = 0;
    
    const char *result = JsonValueRetNull(
        contextPtr,
        jsonStr.c_str(), static_cast<int32_t>(jsonStr.size()), false,
        path.c_str(), 0, static_cast<int32_t>(path.size()), false,
        &outIsNull, &outLen
    );
    
    if (expectIsNull) {
        EXPECT_TRUE(outIsNull);
        EXPECT_EQ(outLen, 0);
    } else {
        EXPECT_FALSE(outIsNull);
        EXPECT_EQ(expectedResult, std::string(result, outLen));
    }
    
    delete context;
}

// Test simple key access
TEST(JsonValueTest, SimpleKeyAccess)
{
    JsonValueTest(R"({"name":"John","age":30})", "$.name", "John", false);
    JsonValueTest(R"({"name":"John","age":30})", "$.age", "30", false);
}

// Test nested key access
TEST(JsonValueTest, NestedKeyAccess)
{
    JsonValueTest(R"({"user":{"name":"John","age":30}})", "$.user.name", "John", false);
    JsonValueTest(R"({"user":{"name":"John","age":30}})", "$.user.age", "30", false);
}

// Test array index access
TEST(JsonValueTest, ArrayIndexAccess)
{
    JsonValueTest(R"([1,2,3,4,5])", "$[0]", "1", false);
    JsonValueTest(R"([1,2,3,4,5])", "$[4]", "5", false);
    JsonValueTest(R"({"items":[10,20,30]})", "$.items[1]", "20", false);
}

// Test quoted keys with dots
TEST(JsonValueTest, QuotedKeysWithSpecialChars)
{
    JsonValueTest(R"({"key.with.dot":"value"})", "$['key.with.dot']", "value", false);
    JsonValueTest(R"({"key-with-dash":"value"})", "$['key-with-dash']", "value", false);
    JsonValueTest(R"({"key[0]":"value"})", "$['key[0]']", "value", false);
}

// Test double-quoted keys
TEST(JsonValueTest, DoubleQuotedKeys)
{
    JsonValueTest(R"({"key.with.dot":"value"})", "$[\"key.with.dot\"]", "value", false);
    JsonValueTest(R"({"name":"test"})", "$[\"name\"]", "test", false);
}

// Test different value types
TEST(JsonValueTest, DifferentValueTypes)
{
    // Boolean
    JsonValueTest(R"({"active":true})", "$.active", "true", false);
    JsonValueTest(R"({"active":false})", "$.active", "false", false);
    
    // Integer
    JsonValueTest(R"({"count":42})", "$.count", "42", false);
    JsonValueTest(R"({"negative":-100})", "$.negative", "-100", false);
    
    // Float
    JsonValueTest(R"({"price":19.99})", "$.price", "19.990000", false);
}

// Test complex nested structures
TEST(JsonValueTest, ComplexNestedStructures)
{
    std::string json = R"({
        "company": {
            "name": "TechCorp",
            "employees": [
                {"id": 1, "name": "Alice"},
                {"id": 2, "name": "Bob"}
            ]
        }
    })";
    
    JsonValueTest(json, "$.company.name", "TechCorp", false);
    JsonValueTest(json, "$.company.employees[0].name", "Alice", false);
    JsonValueTest(json, "$.company.employees[1].id", "2", false);
}

// Test null/missing values
TEST(JsonValueTest, NullAndMissingValues)
{
    // Null value in JSON
    JsonValueTest(R"({"value":null})", "$.value", "", true);
    
    // Missing key
    JsonValueTest(R"({"name":"John"})", "$.age", "", true);
    
    // Missing nested key
    JsonValueTest(R"({"user":{"name":"John"}})", "$.user.email", "", true);
    
    // Array out of bounds
    JsonValueTest(R"([1,2,3])", "$[5]", "", true);
}

// Test input null handling
TEST(JsonValueTest, InputNullHandling)
{
    auto context = new ExecutionContext();
    int64_t contextPtr = reinterpret_cast<int64_t>(context);
    
    bool outIsNull = false;
    int32_t outLen = 0;
    
    // JSON is null
    std::string json = R"({"name":"John"})";
    std::string path = "$.name";
    const char *result = JsonValueRetNull(
        contextPtr,
        json.c_str(), static_cast<int32_t>(json.size()), true,
        path.c_str(), 0, static_cast<int32_t>(path.size()), false,
        &outIsNull, &outLen
    );
    EXPECT_TRUE(outIsNull);
    
    // Path is null
    result = JsonValueRetNull(
        contextPtr,
        json.c_str(), static_cast<int32_t>(json.size()), false,
        path.c_str(), 0, static_cast<int32_t>(path.size()), true,
        &outIsNull, &outLen
    );
    EXPECT_TRUE(outIsNull);
    
    delete context;
}

// Test invalid paths
TEST(JsonValueTest, InvalidPaths)
{
    // Path doesn't start with $
    JsonValueTest(R"({"name":"John"})", "name", "", true);
    
    // Empty path
    JsonValueTest(R"({"name":"John"})", "", "", true);
}

// Test with escaped characters in JSON
TEST(JsonValueTest, EscapedCharactersInJson)
{
    JsonValueTest(R"({"message":"Hello \"World\""})", "$.message", R"(Hello "World")", false);
    JsonValueTest(R"({"path":"C:\\Users\\test"})", "$.path", R"(C:\Users\test)", false);
}

// Test object and array to string conversion
TEST(JsonValueTest, ComplexTypeConversion)
{
    // Nested object as string
    JsonValueTest(R"({"data":{"x":1,"y":2}})", "$.data", R"({"x":1,"y":2})", false);
    
    // Array as string
    JsonValueTest(R"({"items":[1,2,3]})", "$.items", "[1,2,3]", false);
}

// Test mixed bracket and dot notation
TEST(JsonValueTest, MixedNotation)
{
    JsonValueTest(R"({"users":[{"name":"Alice"},{"name":"Bob"}]})",
                 "$.users[0].name", "Alice", false);
    JsonValueTest(R"({"data":{"matrix":[[1,2],[3,4]]}})",
                 "$.data.matrix[1][0]", "3", false);
}

// Performance test - large JSON
TEST(JsonValueTest, LargeJsonPerformance)
{
    std::string largeJson = R"({
        "level1": {
            "level2": {
                "level3": {
                    "level4": {
                        "level5": {
                            "value": "deepvalue"
                        }
                    }
                }
            }
        }
    })";
    
    JsonValueTest(largeJson, "$.level1.level2.level3.level4.level5.value", "deepvalue", false);
}

// Test paths with whitespace (should handle gracefully)
TEST(JsonValueTest, PathsWithWhitespace)
{
    JsonValueTest(R"({"name":"John"})", "$. name", "", true);  // Invalid - space in path
}

// Test extended JSON_VALUE with ON EMPTY/ERROR behaviors
TEST(JsonValueTest, ExtendedOnEmptyNull)
{
    auto context = new ExecutionContext();
    int64_t contextPtr = reinterpret_cast<int64_t>(context);
    
    bool outIsNull = false;
    int32_t outLen = 0;
    
    std::string json = R"({"name":"John"})";
    std::string path = "$.age";  // Non-existent path
    
    // Test NULL ON EMPTY
    const char *result = JsonValueExtended(
        contextPtr,
        json.c_str(), static_cast<int32_t>(json.size()), false,
        path.c_str(), 0, static_cast<int32_t>(path.size()), false,
        0, nullptr, 0, true,  // NULL ON EMPTY
        0, nullptr, 0, true,  // NULL ON ERROR
        &outIsNull, &outLen
    );
    
    EXPECT_TRUE(outIsNull);
    EXPECT_EQ(outLen, 0);
    
    delete context;
}

TEST(JsonValueTest, ExtendedOnEmptyDefault)
{
    auto context = new ExecutionContext();
    int64_t contextPtr = reinterpret_cast<int64_t>(context);
    
    bool outIsNull = false;
    int32_t outLen = 0;
    
    std::string json = R"({"name":"John"})";
    std::string path = "$.age";  // Non-existent path
    std::string defaultValue = "unknown";
    
    // Test DEFAULT ON EMPTY
    const char *result = JsonValueExtended(
        contextPtr,
        json.c_str(), static_cast<int32_t>(json.size()), false,
        path.c_str(), 0, static_cast<int32_t>(path.size()), false,
        2, defaultValue.c_str(), static_cast<int32_t>(defaultValue.size()), false,  // DEFAULT ON EMPTY
        0, nullptr, 0, true,  // NULL ON ERROR
        &outIsNull, &outLen
    );
    
    EXPECT_FALSE(outIsNull);
    EXPECT_EQ("unknown", std::string(result, outLen));
    
    delete context;
}

TEST(JsonValueTest, ExtendedOnErrorNull)
{
    auto context = new ExecutionContext();
    int64_t contextPtr = reinterpret_cast<int64_t>(context);
    
    bool outIsNull = false;
    int32_t outLen = 0;
    
    std::string json = "invalid json";
    std::string path = "$.name";
    
    // Test NULL ON ERROR with invalid JSON
    const char *result = JsonValueExtended(
        contextPtr,
        json.c_str(), static_cast<int32_t>(json.size()), false,
        path.c_str(), 0, static_cast<int32_t>(path.size()), false,
        0, nullptr, 0, true,  // NULL ON EMPTY
        0, nullptr, 0, true,  // NULL ON ERROR
        &outIsNull, &outLen
    );
    
    EXPECT_TRUE(outIsNull);
    EXPECT_EQ(outLen, 0);
    
    delete context;
}

TEST(JsonValueTest, ExtendedOnErrorDefault)
{
    auto context = new ExecutionContext();
    int64_t contextPtr = reinterpret_cast<int64_t>(context);
    
    bool outIsNull = false;
    int32_t outLen = 0;
    
    std::string json = "invalid json";
    std::string path = "$.name";
    std::string defaultValue = "error_default";
    
    // Test DEFAULT ON ERROR with invalid JSON
    const char *result = JsonValueExtended(
        contextPtr,
        json.c_str(), static_cast<int32_t>(json.size()), false,
        path.c_str(), 0, static_cast<int32_t>(path.size()), false,
        0, nullptr, 0, true,  // NULL ON EMPTY
        2, defaultValue.c_str(), static_cast<int32_t>(defaultValue.size()), false,  // DEFAULT ON ERROR
        &outIsNull, &outLen
    );
    
    EXPECT_FALSE(outIsNull);
    EXPECT_EQ("error_default", std::string(result, outLen));
    
    delete context;
}

TEST(JsonValueTest, ExtendedNullInput)
{
    auto context = new ExecutionContext();
    int64_t contextPtr = reinterpret_cast<int64_t>(context);
    
    bool outIsNull = false;
    int32_t outLen = 0;
    
    std::string json = R"({"name":"John"})";
    std::string path = "$.name";
    std::string defaultValue = "null_input_default";
    
    // Test with NULL input
    const char *result = JsonValueExtended(
        contextPtr,
        json.c_str(), static_cast<int32_t>(json.size()), true,  // Input is NULL
        path.c_str(), 0, static_cast<int32_t>(path.size()), false,
        0, nullptr, 0, true,  // NULL ON EMPTY
        2, defaultValue.c_str(), static_cast<int32_t>(defaultValue.size()), false,  // DEFAULT ON ERROR
        &outIsNull, &outLen
    );
    
    EXPECT_FALSE(outIsNull);
    EXPECT_EQ("null_input_default", std::string(result, outLen));
    
    delete context;
}

// Test JsonSplitScalar function
static void JsonSplitScalarTest(const std::string &jsonStr,
                                const std::string &expectedResult, bool expectIsNull)
{
    auto context = new ExecutionContext();
    int64_t contextPtr = reinterpret_cast<int64_t>(context);
    
    bool outIsNull = false;
    int32_t outLen = 0;
    
    const char *result = JsonSplitScalar(
        contextPtr,
        jsonStr.c_str(), static_cast<int32_t>(jsonStr.size()), false,
        &outIsNull, &outLen
    );
    
    if (expectIsNull) {
        EXPECT_TRUE(outIsNull);
        EXPECT_EQ(outLen, 0);
    } else {
        EXPECT_FALSE(outIsNull);
        EXPECT_EQ(expectedResult, std::string(result, outLen));
    }
    
    delete context;
}

TEST(JsonSplitScalarTest, SimpleArray)
{
    JsonSplitScalarTest(R"(["a","b","c"])", "a\r\nb\r\nc", false);
}

TEST(JsonSplitScalarTest, NumericArray)
{
    JsonSplitScalarTest(R"([1,2,3,4,5])", "1\r\n2\r\n3\r\n4\r\n5", false);
}

TEST(JsonSplitScalarTest, MixedArray)
{
    JsonSplitScalarTest(R"(["string",123,true,null])", "string\r\n123\r\ntrue\r\nnull", false);
}

TEST(JsonSplitScalarTest, NestedArray)
{
    JsonSplitScalarTest(R"([[1,2],[3,4]])", "[1,2]\r\n[3,4]", false);
}

TEST(JsonSplitScalarTest, ObjectArray)
{
    JsonSplitScalarTest(R"([{"name":"Alice"},{"name":"Bob"}])", "{\"name\":\"Alice\"}\r\n{\"name\":\"Bob\"}", false);
}

TEST(JsonSplitScalarTest, SingleElementArray)
{
    JsonSplitScalarTest(R"(["only"])", "only", false);
}

TEST(JsonSplitScalarTest, EmptyArray)
{
    JsonSplitScalarTest(R"([])", "", false);
}

TEST(JsonSplitScalarTest, NotArray)
{
    JsonSplitScalarTest(R"({"key":"value"})", "", true);
    JsonSplitScalarTest(R"("string")", "", true);
    JsonSplitScalarTest(R"(123)", "", true);
}

TEST(JsonSplitScalarTest, NullInput)
{
    auto context = new ExecutionContext();
    int64_t contextPtr = reinterpret_cast<int64_t>(context);
    
    bool outIsNull = false;
    int32_t outLen = 0;
    
    // Test with null input
    const char *result = JsonSplitScalar(
        contextPtr,
        nullptr, 0, true,
        &outIsNull, &outLen
    );
    EXPECT_TRUE(outIsNull);
    EXPECT_EQ(outLen, 0);
    
    delete context;
}

TEST(JsonSplitScalarTest, InvalidJson)
{
    JsonSplitScalarTest("invalid json", "", true);
    JsonSplitScalarTest("[1,2,", "", true);
}

} // namespace omniruntime