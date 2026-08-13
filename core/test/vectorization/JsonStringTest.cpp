/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: json_string function unit tests.
 *
 * Covers Flink SQL JSON_STRING(value) semantics for the registered input types
 * (BOOL / integral / floating-point / string / ARRAY / MAP / ROW), including the
 * Flink-specific behavior of KEEPING NULL struct fields (vs Spark to_json which
 * drops them). See docs/expression-design/json_string_design.md.
 */

#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <stack>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/VectorFunction.h"
#include "vectorization/functions/JsonString.h"
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

class JsonStringTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const json_string_test_env =
    ::testing::AddGlobalTestEnvironment(new JsonStringTestEnvironment);

class JsonStringTestHelper {
public:
    // ---- Scalar vector builders ----
    template <typename T>
    static BaseVector* CreateFlatVector(const std::vector<T>& values, DataTypeId typeId) {
        auto* vec = VectorHelper::CreateFlatVector(typeId, static_cast<int32_t>(values.size()));
        vec->SetIsField(true);
        auto* typed = static_cast<Vector<T>*>(vec);
        for (size_t i = 0; i < values.size(); ++i) {
            typed->SetValue(static_cast<int32_t>(i), values[i]);
        }
        return vec;
    }

    static BaseVector* CreateStringVector(const std::vector<std::string>& values) {
        BaseVector* vec = VectorHelper::CreateStringVector(static_cast<uint32_t>(values.size()));
        vec->SetIsField(true);
        auto* typed = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(vec);
        EXPECT_NE(typed, nullptr);
        for (size_t i = 0; i < values.size(); ++i) {
            std::string_view sv(values[i]);
            typed->SetValue(static_cast<int32_t>(i), sv);
        }
        return vec;
    }

    // ---- Composite vector builders (mirror ToJsonTest patterns) ----
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

    // Array of strings (for json_string of ARRAY<STRING>).
    static BaseVector* CreateStringArrayVector(int rowSize,
        const std::vector<std::vector<std::string>>& arrayData) {
        std::vector<int64_t> offsets = {0};
        std::vector<std::string> allElements;
        size_t totalElements = 0;
        for (const auto& arr : arrayData) {
            for (const auto& v : arr) allElements.push_back(v);
            totalElements += arr.size();
            offsets.push_back(static_cast<int64_t>(totalElements));
        }
        auto elemVec = VectorHelper::CreateStringVector(static_cast<uint32_t>(allElements.size()));
        elemVec->SetIsField(true);
        auto* strVec = static_cast<Vector<LargeStringContainer<std::string_view>>*>(elemVec);
        for (size_t i = 0; i < allElements.size(); ++i) {
            strVec->SetValue(static_cast<int32_t>(i), std::string_view(allElements[i]));
        }
        auto* arrVec = new ArrayVector(rowSize, std::shared_ptr<BaseVector>(elemVec));
        arrVec->SetIsField(true);
        for (int i = 0; i <= rowSize; ++i) arrVec->SetOffset(i, static_cast<int32_t>(offsets[i]));
        return arrVec;
    }

    static BaseVector* CreateMapVector(int rowSize,
        const std::vector<std::string>& allKeys,
        const std::vector<int64_t>& allValues,
        const std::vector<int64_t>& offsets) {
        auto keyVec = VectorHelper::CreateStringVector(static_cast<uint32_t>(allKeys.size()));
        keyVec->SetIsField(true);
        auto* keyStrVec = static_cast<Vector<LargeStringContainer<std::string_view>>*>(keyVec);
        for (size_t i = 0; i < allKeys.size(); ++i) {
            keyStrVec->SetValue(static_cast<int32_t>(i), std::string_view(allKeys[i]));
        }
        auto valVec = VectorHelper::CreateFlatVector(OMNI_LONG, static_cast<int32_t>(allValues.size()));
        auto* valLongVec = static_cast<Vector<int64_t>*>(valVec);
        for (size_t i = 0; i < allValues.size(); ++i) {
            valLongVec->SetValue(static_cast<int32_t>(i), allValues[i]);
        }
        auto* mapVec = new MapVector(rowSize);
        mapVec->SetIsField(true);
        mapVec->SetKeyVector(std::shared_ptr<BaseVector>(keyVec));
        mapVec->SetValueVector(std::shared_ptr<BaseVector>(valVec));
        for (int i = 0; i <= rowSize; ++i) mapVec->SetOffset(i, static_cast<int32_t>(offsets[i]));
        return mapVec;
    }

    // ---- Execution helpers ----
    static void ExecuteJsonString(BaseVector* inputVec, DataTypeId inputType, BaseVector*& result) {
        std::vector<DataTypeId> inputTypeIds = {inputType};
        auto sig = std::make_shared<FunctionSignature>("json_string", inputTypeIds, OMNI_VARCHAR);
        auto fn = VectorFunction::Find(sig);
        ASSERT_NE(fn, nullptr) << "json_string not registered for type id " << inputType;
        auto outputType = std::make_shared<DataType>(OMNI_VARCHAR);
        ExecutionContext ctx;
        ctx.SetResultRowSize(inputVec->GetSize());
        std::stack<BaseVector*> args;
        args.push(inputVec);
        ASSERT_NO_THROW(fn->Apply(args, outputType, result, &ctx));
    }

    static void ExecuteJsonStringWithType(BaseVector* inputVec, DataTypeId inputType,
        const DataType* inputDataType, BaseVector*& result) {
        std::vector<DataTypeId> inputTypeIds = {inputType};
        auto sig = std::make_shared<FunctionSignature>("json_string", inputTypeIds, OMNI_VARCHAR);
        auto fn = VectorFunction::Find(sig);
        ASSERT_NE(fn, nullptr);
        auto outputType = std::make_shared<DataType>(OMNI_VARCHAR);
        ExecutionContext ctx;
        ctx.SetResultRowSize(inputVec->GetSize());
        ctx.SetToJsonInputType(inputDataType);
        std::stack<BaseVector*> args;
        args.push(inputVec);
        ASSERT_NO_THROW(fn->Apply(args, outputType, result, &ctx));
    }

    static void ValidateStringResult(BaseVector* result, int row, const std::string& expected) {
        auto* strVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
        ASSERT_NE(strVec, nullptr);
        ASSERT_FALSE(strVec->IsNull(row)) << "row " << row << " unexpectedly null";
        std::string actual(strVec->GetValue(row));
        EXPECT_EQ(actual, expected) << "row " << row;
    }

    static void ValidateNullResult(BaseVector* result, int row) {
        auto* strVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
        ASSERT_NE(strVec, nullptr);
        EXPECT_TRUE(strVec->IsNull(row)) << "row " << row << " expected to be null";
    }
};

// =============================================================================
// Scalar type tests
// =============================================================================

TEST(JsonStringTest, BooleanScalar) {
    auto* vec = JsonStringTestHelper::CreateFlatVector<bool>({true, false}, OMNI_BOOLEAN);
    BaseVector* result = nullptr;
    JsonStringTestHelper::ExecuteJsonString(vec, OMNI_BOOLEAN, result);
    JsonStringTestHelper::ValidateStringResult(result, 0, "true");
    JsonStringTestHelper::ValidateStringResult(result, 1, "false");
    delete vec;
    delete result;
}

TEST(JsonStringTest, ByteScalar) {
    auto* vec = JsonStringTestHelper::CreateFlatVector<int8_t>({1, -1, 127}, OMNI_BYTE);
    BaseVector* result = nullptr;
    JsonStringTestHelper::ExecuteJsonString(vec, OMNI_BYTE, result);
    JsonStringTestHelper::ValidateStringResult(result, 0, "1");
    JsonStringTestHelper::ValidateStringResult(result, 1, "-1");
    JsonStringTestHelper::ValidateStringResult(result, 2, "127");
    delete vec;
    delete result;
}

TEST(JsonStringTest, ShortScalar) {
    auto* vec = JsonStringTestHelper::CreateFlatVector<int16_t>({100, -100}, OMNI_SHORT);
    BaseVector* result = nullptr;
    JsonStringTestHelper::ExecuteJsonString(vec, OMNI_SHORT, result);
    JsonStringTestHelper::ValidateStringResult(result, 0, "100");
    JsonStringTestHelper::ValidateStringResult(result, 1, "-100");
    delete vec;
    delete result;
}

TEST(JsonStringTest, IntScalar) {
    auto* vec = JsonStringTestHelper::CreateFlatVector<int32_t>({1, -5, 0}, OMNI_INT);
    BaseVector* result = nullptr;
    JsonStringTestHelper::ExecuteJsonString(vec, OMNI_INT, result);
    JsonStringTestHelper::ValidateStringResult(result, 0, "1");
    JsonStringTestHelper::ValidateStringResult(result, 1, "-5");
    JsonStringTestHelper::ValidateStringResult(result, 2, "0");
    delete vec;
    delete result;
}

TEST(JsonStringTest, LongScalar) {
    // Beyond INT32 range to verify 64-bit path.
    auto* vec = JsonStringTestHelper::CreateFlatVector<int64_t>(
        {9223372036854775807LL, -9223372036854775807LL - 1LL}, OMNI_LONG);
    BaseVector* result = nullptr;
    JsonStringTestHelper::ExecuteJsonString(vec, OMNI_LONG, result);
    JsonStringTestHelper::ValidateStringResult(result, 0, "9223372036854775807");
    JsonStringTestHelper::ValidateStringResult(result, 1, "-9223372036854775808");
    delete vec;
    delete result;
}

TEST(JsonStringTest, FloatScalar) {
    auto* vec = JsonStringTestHelper::CreateFlatVector<float>({1.5f, 3.0f, -2.25f}, OMNI_FLOAT);
    BaseVector* result = nullptr;
    JsonStringTestHelper::ExecuteJsonString(vec, OMNI_FLOAT, result);
    JsonStringTestHelper::ValidateStringResult(result, 0, "1.5");
    JsonStringTestHelper::ValidateStringResult(result, 1, "3");
    JsonStringTestHelper::ValidateStringResult(result, 2, "-2.25");
    delete vec;
    delete result;
}

TEST(JsonStringTest, DoubleScalar) {
    auto* vec = JsonStringTestHelper::CreateFlatVector<double>({3.14, 1.0, -0.5}, OMNI_DOUBLE);
    BaseVector* result = nullptr;
    JsonStringTestHelper::ExecuteJsonString(vec, OMNI_DOUBLE, result);
    JsonStringTestHelper::ValidateStringResult(result, 0, "3.14");
    JsonStringTestHelper::ValidateStringResult(result, 1, "1");
    JsonStringTestHelper::ValidateStringResult(result, 2, "-0.5");
    delete vec;
    delete result;
}

TEST(JsonStringTest, StringScalar) {
    auto* vec = JsonStringTestHelper::CreateStringVector({"Hello, World!", ""});
    BaseVector* result = nullptr;
    JsonStringTestHelper::ExecuteJsonString(vec, OMNI_VARCHAR, result);
    JsonStringTestHelper::ValidateStringResult(result, 0, "\"Hello, World!\"");
    JsonStringTestHelper::ValidateStringResult(result, 1, "\"\"");
    delete vec;
    delete result;
}

TEST(JsonStringTest, StringEscape) {
    // Covers quote, backslash, and all JSON mandatory escape characters.
    std::string withSpecials = "a\"b\\c\n\r\t\b\f" + std::string(1, '\x01');
    auto* vec = JsonStringTestHelper::CreateStringVector({withSpecials});
    BaseVector* result = nullptr;
    JsonStringTestHelper::ExecuteJsonString(vec, OMNI_VARCHAR, result);
    // Expected: "a\"b\\c\n\r\t\b\f\u0001"
    JsonStringTestHelper::ValidateStringResult(result, 0,
        "\"a\\\"b\\\\c\\n\\r\\t\\b\\f\\u0001\"");
    delete vec;
    delete result;
}

TEST(JsonStringTest, NullScalarPropagation) {
    auto* vec = JsonStringTestHelper::CreateFlatVector<int32_t>({1, 2}, OMNI_INT);
    vec->SetNull(0);  // row 0 is NULL
    BaseVector* result = nullptr;
    JsonStringTestHelper::ExecuteJsonString(vec, OMNI_INT, result);
    JsonStringTestHelper::ValidateNullResult(result, 0);
    JsonStringTestHelper::ValidateStringResult(result, 1, "2");
    delete vec;
    delete result;
}

TEST(JsonStringTest, ConstIntInput) {
    // ConstVector path (encoding == OMNI_ENCODING_CONST).
    auto* vec = new ConstVector<int32_t>(42, OMNI_INT, 3);
    BaseVector* result = nullptr;
    JsonStringTestHelper::ExecuteJsonString(vec, OMNI_INT, result);
    JsonStringTestHelper::ValidateStringResult(result, 0, "42");
    JsonStringTestHelper::ValidateStringResult(result, 1, "42");
    JsonStringTestHelper::ValidateStringResult(result, 2, "42");
    delete vec;
    delete result;
}

TEST(JsonStringTest, ConstStringInput) {
    auto* vec = new ConstVector<std::string_view>(std::string_view("hi"), OMNI_VARCHAR, 2);
    BaseVector* result = nullptr;
    JsonStringTestHelper::ExecuteJsonString(vec, OMNI_VARCHAR, result);
    JsonStringTestHelper::ValidateStringResult(result, 0, "\"hi\"");
    JsonStringTestHelper::ValidateStringResult(result, 1, "\"hi\"");
    delete vec;
    delete result;
}

// =============================================================================
// ARRAY tests
// =============================================================================

TEST(JsonStringTest, ArrayOfLong) {
    std::vector<std::vector<int64_t>> arrayData = {{1, 2, 3}, {10, 20}};
    BaseVector* arrVec = JsonStringTestHelper::CreateArrayVector(2, arrayData);
    BaseVector* result = nullptr;
    JsonStringTestHelper::ExecuteJsonString(arrVec, OMNI_ARRAY, result);
    JsonStringTestHelper::ValidateStringResult(result, 0, "[1,2,3]");
    JsonStringTestHelper::ValidateStringResult(result, 1, "[10,20]");
    delete arrVec;
    delete result;
}

TEST(JsonStringTest, EmptyArray) {
    std::vector<std::vector<int64_t>> arrayData = {{}};
    BaseVector* arrVec = JsonStringTestHelper::CreateArrayVector(1, arrayData);
    BaseVector* result = nullptr;
    JsonStringTestHelper::ExecuteJsonString(arrVec, OMNI_ARRAY, result);
    JsonStringTestHelper::ValidateStringResult(result, 0, "[]");
    delete arrVec;
    delete result;
}

TEST(JsonStringTest, ArrayWithNullElement) {
    // Array [1, NULL, 3]: NULL elements are emitted as `null` (Flink keeps them).
    std::vector<std::vector<int64_t>> arrayData = {{1, 0, 3}};
    BaseVector* arrVec = JsonStringTestHelper::CreateArrayVector(1, arrayData);
    auto* arrVecTyped = dynamic_cast<ArrayVector*>(arrVec);
    ASSERT_NE(arrVecTyped, nullptr);
    arrVecTyped->GetElementVector()->SetNull(1);  // middle element is NULL
    BaseVector* result = nullptr;
    JsonStringTestHelper::ExecuteJsonString(arrVec, OMNI_ARRAY, result);
    JsonStringTestHelper::ValidateStringResult(result, 0, "[1,null,3]");
    delete arrVec;
    delete result;
}

TEST(JsonStringTest, ArrayOfStrings) {
    std::vector<std::vector<std::string>> arrayData = {{"a", "b"}};
    BaseVector* arrVec = JsonStringTestHelper::CreateStringArrayVector(1, arrayData);
    BaseVector* result = nullptr;
    JsonStringTestHelper::ExecuteJsonString(arrVec, OMNI_ARRAY, result);
    JsonStringTestHelper::ValidateStringResult(result, 0, "[\"a\",\"b\"]");
    delete arrVec;
    delete result;
}

TEST(JsonStringTest, ArrayNullPropagation) {
    std::vector<std::vector<int64_t>> arrayData = {{1, 2}};
    BaseVector* arrVec = JsonStringTestHelper::CreateArrayVector(1, arrayData);
    arrVec->SetNull(0);  // whole array is NULL
    BaseVector* result = nullptr;
    JsonStringTestHelper::ExecuteJsonString(arrVec, OMNI_ARRAY, result);
    JsonStringTestHelper::ValidateNullResult(result, 0);
    delete arrVec;
    delete result;
}

// =============================================================================
// MAP tests
// =============================================================================

TEST(JsonStringTest, MapSimple) {
    std::vector<std::string> keys = {"a", "b", "c"};
    std::vector<int64_t> values = {1, 2, 3};
    std::vector<int64_t> offsets = {0, 2, 3};
    BaseVector* mapVec = JsonStringTestHelper::CreateMapVector(2, keys, values, offsets);
    BaseVector* result = nullptr;
    JsonStringTestHelper::ExecuteJsonString(mapVec, OMNI_MAP, result);
    // Row 0 has {"a":1,"b":2} (key order is preserved as inserted; allow either permutation).
    std::string r0 = std::string(
        dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result)->GetValue(0));
    EXPECT_TRUE(r0 == R"({"a":1,"b":2})" || r0 == R"({"b":2,"a":1})")
        << "unexpected map row 0: " << r0;
    JsonStringTestHelper::ValidateStringResult(result, 1, R"({"c":3})");
    delete mapVec;
    delete result;
}

TEST(JsonStringTest, EmptyMap) {
    std::vector<std::string> keys;
    std::vector<int64_t> values;
    std::vector<int64_t> offsets = {0, 0};
    BaseVector* mapVec = JsonStringTestHelper::CreateMapVector(1, keys, values, offsets);
    BaseVector* result = nullptr;
    JsonStringTestHelper::ExecuteJsonString(mapVec, OMNI_MAP, result);
    JsonStringTestHelper::ValidateStringResult(result, 0, "{}");
    delete mapVec;
    delete result;
}

TEST(JsonStringTest, MapNullPropagation) {
    std::vector<std::string> keys = {"a"};
    std::vector<int64_t> values = {1};
    std::vector<int64_t> offsets = {0, 1};
    BaseVector* mapVec = JsonStringTestHelper::CreateMapVector(1, keys, values, offsets);
    mapVec->SetNull(0);  // whole map is NULL
    BaseVector* result = nullptr;
    JsonStringTestHelper::ExecuteJsonString(mapVec, OMNI_MAP, result);
    JsonStringTestHelper::ValidateNullResult(result, 0);
    delete mapVec;
    delete result;
}

// =============================================================================
// ROW tests (key Flink behavior: keep NULL fields)
// =============================================================================

TEST(JsonStringTest, RowWithFieldNames) {
    std::vector<std::vector<int32_t>> intVals = {{1, 2}, {10, 20}};
    BaseVector* rowVec = JsonStringTestHelper::CreateRowVector(2, {OMNI_INT, OMNI_INT}, intVals);
    std::vector<std::shared_ptr<DataType>> fieldTypes = {
        std::make_shared<DataType>(OMNI_INT), std::make_shared<DataType>(OMNI_INT)};
    std::vector<std::string> fieldNames = {"name", "age"};
    auto rowType = std::make_shared<RowType>(fieldTypes, fieldNames);
    BaseVector* result = nullptr;
    JsonStringTestHelper::ExecuteJsonStringWithType(rowVec, OMNI_ROW, rowType.get(), result);
    JsonStringTestHelper::ValidateStringResult(result, 0, R"({"name":1,"age":10})");
    JsonStringTestHelper::ValidateStringResult(result, 1, R"({"name":2,"age":20})");
    delete rowVec;
    delete result;
}

TEST(JsonStringTest, RowDefaultFieldNames) {
    std::vector<std::vector<int32_t>> intVals = {{1}, {2}};
    BaseVector* rowVec = JsonStringTestHelper::CreateRowVector(1, {OMNI_INT, OMNI_INT}, intVals);
    BaseVector* result = nullptr;
    JsonStringTestHelper::ExecuteJsonString(rowVec, OMNI_ROW, result);
    JsonStringTestHelper::ValidateStringResult(result, 0, R"({"field0":1,"field1":2})");
    delete rowVec;
    delete result;
}

// KEY FLINK SEMANTIC: JSON_STRING keeps NULL struct fields (emits "field":null),
// unlike Spark to_json which drops them.
TEST(JsonStringTest, RowKeepsNullField) {
    std::vector<std::vector<int32_t>> intVals = {{1}, {99}};
    BaseVector* rowVec = JsonStringTestHelper::CreateRowVector(1, {OMNI_INT, OMNI_INT}, intVals);
    dynamic_cast<RowVector*>(rowVec)->ChildAt(0)->SetNull(0);  // field "a" is NULL
    std::vector<std::shared_ptr<DataType>> fieldTypes = {
        std::make_shared<DataType>(OMNI_INT), std::make_shared<DataType>(OMNI_INT)};
    std::vector<std::string> fieldNames = {"a", "b"};
    auto rowType = std::make_shared<RowType>(fieldTypes, fieldNames);
    BaseVector* result = nullptr;
    JsonStringTestHelper::ExecuteJsonStringWithType(rowVec, OMNI_ROW, rowType.get(), result);
    // Flink: {"a":null,"b":99}  (Spark to_json would give {"b":99})
    JsonStringTestHelper::ValidateStringResult(result, 0, R"({"a":null,"b":99})");
    delete rowVec;
    delete result;
}

TEST(JsonStringTest, RowAllFieldsNull) {
    std::vector<std::vector<int32_t>> intVals = {{1}, {2}};
    BaseVector* rowVec = JsonStringTestHelper::CreateRowVector(1, {OMNI_INT, OMNI_INT}, intVals);
    dynamic_cast<RowVector*>(rowVec)->ChildAt(0)->SetNull(0);
    dynamic_cast<RowVector*>(rowVec)->ChildAt(1)->SetNull(0);
    std::vector<std::shared_ptr<DataType>> fieldTypes = {
        std::make_shared<DataType>(OMNI_INT), std::make_shared<DataType>(OMNI_INT)};
    std::vector<std::string> fieldNames = {"a", "b"};
    auto rowType = std::make_shared<RowType>(fieldTypes, fieldNames);
    BaseVector* result = nullptr;
    JsonStringTestHelper::ExecuteJsonStringWithType(rowVec, OMNI_ROW, rowType.get(), result);
    // Flink keeps both null fields -> {"a":null,"b":null} (Spark to_json would give {}).
    JsonStringTestHelper::ValidateStringResult(result, 0, R"({"a":null,"b":null})");
    delete rowVec;
    delete result;
}

TEST(JsonStringTest, RowNullPropagation) {
    std::vector<std::vector<int32_t>> intVals = {{1}, {2}};
    BaseVector* rowVec = JsonStringTestHelper::CreateRowVector(1, {OMNI_INT, OMNI_INT}, intVals);
    rowVec->SetNull(0);  // whole row is NULL
    BaseVector* result = nullptr;
    JsonStringTestHelper::ExecuteJsonString(rowVec, OMNI_ROW, result);
    JsonStringTestHelper::ValidateNullResult(result, 0);
    delete rowVec;
    delete result;
}

// =============================================================================
// Nested composite types
// =============================================================================

TEST(JsonStringTest, NestedArrayOfLongInRow) {
    // ROW<arr ARRAY<BIGINT>> with one row holding ARRAY[1,2].
    std::vector<std::vector<int64_t>> arrayData = {{1, 2}};
    BaseVector* arrVec = JsonStringTestHelper::CreateArrayVector(1, arrayData);
    auto* rowVec = new RowVector(1);
    rowVec->AddChild(std::shared_ptr<BaseVector>(arrVec));
    rowVec->SetIsField(true);
    // RowType: field "arr" of type ARRAY<LONG>.
    auto elemType = std::make_shared<DataType>(OMNI_LONG);
    auto arrayType = std::make_shared<ArrayType>(elemType);
    std::vector<std::shared_ptr<DataType>> fieldTypes = {arrayType};
    std::vector<std::string> fieldNames = {"arr"};
    auto rowType = std::make_shared<RowType>(fieldTypes, fieldNames);
    BaseVector* result = nullptr;
    JsonStringTestHelper::ExecuteJsonStringWithType(rowVec, OMNI_ROW, rowType.get(), result);
    JsonStringTestHelper::ValidateStringResult(result, 0, R"({"arr":[1,2]})");
    delete rowVec;
    delete result;
}

TEST(JsonStringTest, ArrayOfIntDirect) {
    // ARRAY<INT> built directly (without the multi-row helper): exercises the array
    // offset bookkeeping + appendToJsonFromSlice recursion on a single-element array.
    auto elemVec = VectorHelper::CreateFlatVector(OMNI_INT, 1);
    auto* intVec = static_cast<Vector<int32_t>*>(elemVec);
    intVec->SetValue(0, 5);
    auto* arrVec = new ArrayVector(1, std::shared_ptr<BaseVector>(elemVec));
    arrVec->SetIsField(true);
    arrVec->SetOffset(0, 0);
    arrVec->SetOffset(1, 1);
    BaseVector* result = nullptr;
    JsonStringTestHelper::ExecuteJsonString(arrVec, OMNI_ARRAY, result);
    JsonStringTestHelper::ValidateStringResult(result, 0, "[5]");
    delete arrVec;
    delete result;
}
