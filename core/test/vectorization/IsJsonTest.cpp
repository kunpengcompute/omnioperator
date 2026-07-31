/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: IS JSON function unit tests
 *
 * is_json_value/scalar/array/object(string) -> boolean
 * Mirrors Flink SqlJsonUtils.isJsonValue/isJsonScalar/isJsonArray/isJsonObject.
 *   - NULL input  -> FALSE (NOT null; result type is BOOLEAN NOT NULL)
 *   - empty/invalid -> FALSE
 *   - else apply the type constraint
 *
 * Expected values are derived from sql_functions.yml examples + JsonFunctionsITCase.
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
#include "util/type_util.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::codegen;
using namespace omniruntime::TestUtil;

class IsJsonTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const is_json_test_env =
    ::testing::AddGlobalTestEnvironment(new IsJsonTestEnvironment);

class IsJsonTestHelper {
public:
    static BaseVector* CreateStringVector(const std::vector<std::string>& values) {
        BaseVector* vec = VectorHelper::CreateStringVector(values.size());
        vec->SetIsField(true);
        auto* typed = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(vec);
        // EXPECT_NE (not ASSERT_NE): this helper returns BaseVector* (non-void), and
        // ASSERT_NE expands to `return;` on failure which would not compile here. Report
        // the failure and bail out with nullptr so the caller can ASSERT on the result.
        EXPECT_NE(typed, nullptr);
        if (typed == nullptr) {
            return nullptr;
        }
        for (size_t i = 0; i < values.size(); ++i) {
            std::string_view sv(values[i]);
            typed->SetValue(i, sv);
        }
        return vec;
    }

    // Run <funcName>(string) over a string vector; verify results. Apply() owns
    // (deletes) the input vector, so do not reuse it afterwards.
    static void Run(const std::string& funcName, DataTypeId inputTypeId,
        const std::vector<std::string>& inputs, const std::vector<bool>& expected) {
        ASSERT_EQ(inputs.size(), expected.size());
        BaseVector* strVec = CreateStringVector(inputs);
        ASSERT_NE(strVec, nullptr); // CreateStringVector may return nullptr on cast failure
        for (size_t i = 0; i < inputs.size(); ++i) {
            strVec->SetNotNull(static_cast<int32_t>(i));
        }
        std::vector<DataTypeId> argTypes = {inputTypeId};
        auto sig = std::make_shared<FunctionSignature>(funcName, argTypes, OMNI_BOOLEAN);
        auto fn = VectorFunction::Find(sig);
        ASSERT_NE(fn, nullptr) << funcName << " not found for type " << inputTypeId;
        auto outputType = std::make_shared<DataType>(OMNI_BOOLEAN);
        ExecutionContext ctx;
        ctx.SetResultRowSize(static_cast<int32_t>(inputs.size()));
        std::stack<BaseVector*> args;
        args.push(strVec);
        BaseVector* result = nullptr;
        ASSERT_NO_THROW(fn->Apply(args, outputType, result, &ctx));
        ASSERT_NE(result, nullptr);
        auto* typed = dynamic_cast<Vector<bool>*>(result);
        ASSERT_NE(typed, nullptr);
        for (size_t i = 0; i < inputs.size(); ++i) {
            EXPECT_FALSE(result->IsNull(static_cast<int32_t>(i)))
                << funcName << " row " << i << " should be non-NULL (FALSE, not NULL)";
            EXPECT_EQ(typed->GetValue(static_cast<int32_t>(i)), expected[i])
                << funcName << " row " << i << " input='" << inputs[i] << "'";
        }
        delete result;
    }
};

// ============================================================================
// IS JSON VALUE (default x IS JSON): any valid JSON
// ============================================================================

TEST(IsJsonTest, ValueValidScalars) {
    IsJsonTestHelper::Run("is_json_value", OMNI_VARCHAR,
        {"1", "1.5", "true", "false", "null", "\"abc\""},
        {true, true, true, true, true, true});
}

TEST(IsJsonTest, ValueValidArraysAndObjects) {
    IsJsonTestHelper::Run("is_json_value", OMNI_VARCHAR,
        {"[]", "[1,2,3]", "{}", "{\"a\":1}", "[{\"a\":1}]", "{\"a\":[1,2]}"},
        {true, true, true, true, true, true});
}

TEST(IsJsonTest, ValueInvalid) {
    IsJsonTestHelper::Run("is_json_value", OMNI_VARCHAR,
        {"abc", "", "{", "[1,", "1.5.6", "{a:1}"},
        {false, false, false, false, false, false});
}

// Flink reference cases (sql_functions.yml examples)
TEST(IsJsonTest, ValueFlinkReferenceCases) {
    IsJsonTestHelper::Run("is_json_value", OMNI_VARCHAR,
        {"1", "[]", "{}", "\"abc\"", "abc"},
        {true, true, true, true, false});
}

// ============================================================================
// IS JSON SCALAR: valid JSON whose top-level value is a scalar
// ============================================================================

TEST(IsJsonTest, ScalarTrueCases) {
    IsJsonTestHelper::Run("is_json_scalar", OMNI_VARCHAR,
        {"1", "1.5", "true", "false", "null", "\"abc\"", "-3"},
        {true, true, true, true, true, true, true});
}

TEST(IsJsonTest, ScalarFalseForArraysAndObjects) {
    IsJsonTestHelper::Run("is_json_scalar", OMNI_VARCHAR,
        {"[]", "[1,2]", "{}", "{\"a\":1}"},
        {false, false, false, false});
}

TEST(IsJsonTest, ScalarInvalid) {
    IsJsonTestHelper::Run("is_json_scalar", OMNI_VARCHAR,
        {"abc", "", "{"},
        {false, false, false});
}

// ============================================================================
// IS JSON ARRAY: valid JSON whose top-level value is an array
// ============================================================================

TEST(IsJsonTest, ArrayTrueCases) {
    IsJsonTestHelper::Run("is_json_array", OMNI_VARCHAR,
        {"[]", "[1,2,3]", "[{\"a\":1}]", "[1, [2, 3]]"},
        {true, true, true, true});
}

TEST(IsJsonTest, ArrayFalseForScalarsAndObjects) {
    IsJsonTestHelper::Run("is_json_array", OMNI_VARCHAR,
        {"1", "\"abc\"", "true", "{}", "{\"a\":1}"},
        {false, false, false, false, false});
}

TEST(IsJsonTest, ArrayInvalid) {
    IsJsonTestHelper::Run("is_json_array", OMNI_VARCHAR,
        {"abc", "", "[1,"},
        {false, false, false});
}

// ============================================================================
// IS JSON OBJECT: valid JSON whose top-level value is an object
// ============================================================================

TEST(IsJsonTest, ObjectTrueCases) {
    IsJsonTestHelper::Run("is_json_object", OMNI_VARCHAR,
        {"{}", "{\"a\":1}", "{\"a\":[1,2]}", "{\"a\":{\"b\":2}}"},
        {true, true, true, true});
}

TEST(IsJsonTest, ObjectFalseForScalarsAndArrays) {
    IsJsonTestHelper::Run("is_json_object", OMNI_VARCHAR,
        {"1", "\"abc\"", "[]", "[1,2]"},
        {false, false, false, false});
}

TEST(IsJsonTest, ObjectInvalid) {
    IsJsonTestHelper::Run("is_json_object", OMNI_VARCHAR,
        {"abc", "", "{a:1}"},
        {false, false, false});
}

// ============================================================================
// NULL input -> FALSE (Flink semantics: NOT null propagation)
// ============================================================================

TEST(IsJsonTest, NullInputReturnsFalseAllVariants) {
    const std::vector<std::string> funcs = {
        "is_json_value", "is_json_scalar", "is_json_array", "is_json_object"};
    for (const auto& funcName : funcs) {
        std::vector<std::string> inputs = {"1", "abc"};
        BaseVector* strVec = IsJsonTestHelper::CreateStringVector(inputs);
        ASSERT_NE(strVec, nullptr);
        strVec->SetNull(0); // row 0 is NULL -> expected FALSE (not NULL)
        std::vector<DataTypeId> argTypes = {OMNI_VARCHAR};
        auto sig = std::make_shared<FunctionSignature>(funcName, argTypes, OMNI_BOOLEAN);
        auto fn = VectorFunction::Find(sig);
        ASSERT_NE(fn, nullptr) << funcName << " not found";
        auto outputType = std::make_shared<DataType>(OMNI_BOOLEAN);
        ExecutionContext ctx;
        ctx.SetResultRowSize(2);
        std::stack<BaseVector*> args;
        args.push(strVec);
        BaseVector* result = nullptr;
        ASSERT_NO_THROW(fn->Apply(args, outputType, result, &ctx));
        ASSERT_NE(result, nullptr);
        auto* typed = dynamic_cast<Vector<bool>*>(result);
        ASSERT_NE(typed, nullptr);
        EXPECT_FALSE(result->IsNull(0)) << funcName << ": NULL input must yield FALSE, not NULL";
        EXPECT_FALSE(typed->GetValue(0)) << funcName << ": IS JSON(NULL) = FALSE";
        EXPECT_FALSE(result->IsNull(1));
        delete result;
    }
}

// ============================================================================
// Multi-row mixed batch (per-variant)
// ============================================================================

TEST(IsJsonTest, MultiRowMixedBatchValue) {
    IsJsonTestHelper::Run("is_json_value", OMNI_VARCHAR,
        {"1", "abc", "[]", "{\"a\":1}", "", "\"s\"", "null"},
        {true, false, true, true, false, true, true});
}

TEST(IsJsonTest, MultiRowMixedBatchScalar) {
    IsJsonTestHelper::Run("is_json_scalar", OMNI_VARCHAR,
        {"1", "[]", "{}", "\"s\"", "true", "null"},
        {true, false, false, true, true, true});
}

TEST(IsJsonTest, MultiRowMixedBatchArray) {
    IsJsonTestHelper::Run("is_json_array", OMNI_VARCHAR,
        {"1", "[]", "{}", "[1,2]", "\"s\""},
        {false, true, false, true, false});
}

TEST(IsJsonTest, MultiRowMixedBatchObject) {
    IsJsonTestHelper::Run("is_json_object", OMNI_VARCHAR,
        {"1", "[]", "{}", "{\"a\":1}", "\"s\""},
        {false, false, true, true, false});
}

// ============================================================================
// CHAR input type (all variants resolve via the CHAR registration)
// ============================================================================

TEST(IsJsonTest, CharInputType) {
    IsJsonTestHelper::Run("is_json_value", OMNI_CHAR,
        {"1", "[]", "{}", "abc"},
        {true, true, true, false});
    IsJsonTestHelper::Run("is_json_scalar", OMNI_CHAR, {"1", "[]"}, {true, false});
    IsJsonTestHelper::Run("is_json_array", OMNI_CHAR, {"[]", "1"}, {true, false});
    IsJsonTestHelper::Run("is_json_object", OMNI_CHAR, {"{}", "1"}, {true, false});
}

// ============================================================================
// Whitespace tolerance (rapidjson kParseNoFlags tolerates surrounding whitespace,
// matching Jackson's IS JSON behaviour)
// ============================================================================

TEST(IsJsonTest, WhitespaceTolerance) {
    IsJsonTestHelper::Run("is_json_value", OMNI_VARCHAR,
        {" 1 ", " []", "{} "},
        {true, true, true});
}
