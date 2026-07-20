/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: JSON_EXISTS function unit tests
 *
 * json_exists(jsonValue, path [, onError]) -> boolean
 * Mirrors Flink SqlJsonUtils.jsonExists semantics (see JsonFunctionsITCase::jsonExistsSpec).
 *   - NULL input (json or path) -> NULL (Flink argsNullable=false short-circuit)
 *   - path may carry strict/lax prefix; default mode is STRICT
 *       * LAX: invalid JSON / invalid path / not-found -> FALSE (errors suppressed)
 *       * STRICT: invalid JSON / invalid path / not-found -> ON ERROR
 *   - ON ERROR (default FALSE): TRUE / FALSE / UNKNOWN(NULL) / ERROR(throw)
 *
 * Expected values are derived from sql_functions.yml examples + JsonFunctionsITCase.
 */

#include <gtest/gtest.h>
#include <optional>
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

class JsonExistsTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};
::testing::Environment* const json_exists_test_env =
    ::testing::AddGlobalTestEnvironment(new JsonExistsTestEnvironment);

class JsonExistsTestHelper {
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

    // Run json_exists over parallel json/path vectors (2-arg form, default FALSE ON ERROR).
    // Apply() owns (deletes) the input vectors, so do not reuse them afterwards.
    static void Run(const std::vector<std::string>& jsonInputs,
        const std::vector<std::string>& pathInputs,
        const std::vector<std::optional<bool>>& expected)
    {
        Run(jsonInputs, pathInputs, {}, expected, OMNI_VARCHAR);
    }

    // Run with an optional ON ERROR literal (applies to all rows) and input type.
    static void Run(const std::vector<std::string>& jsonInputs,
        const std::vector<std::string>& pathInputs,
        const std::string& onErrorLiteral,
        const std::vector<std::optional<bool>>& expected,
        DataTypeId inputTypeId = OMNI_VARCHAR)
    {
        ASSERT_EQ(jsonInputs.size(), expected.size());
        ASSERT_EQ(jsonInputs.size(), pathInputs.size());

        BaseVector* jsonVec = CreateStringVector(jsonInputs);
        ASSERT_NE(jsonVec, nullptr); // CreateStringVector may return nullptr on cast failure
        for (size_t i = 0; i < jsonInputs.size(); ++i) {
            jsonVec->SetNotNull(static_cast<int32_t>(i));
        }
        BaseVector* pathVec = CreateStringVector(pathInputs);
        ASSERT_NE(pathVec, nullptr);
        for (size_t i = 0; i < pathInputs.size(); ++i) {
            pathVec->SetNotNull(static_cast<int32_t>(i));
        }

        // Registered signatures: path and onError are always VARCHAR (CHAR literals are
        // normalized to VARCHAR on the OmniAdaptor side); only the jsonValue operand may
        // be VARCHAR or CHAR. So argTypes = {jsonType, VARCHAR[, VARCHAR]}.
        std::vector<DataTypeId> argTypes;
        bool hasOnError = !onErrorLiteral.empty();
        if (hasOnError) {
            argTypes = {inputTypeId, OMNI_VARCHAR, OMNI_VARCHAR};
        } else {
            argTypes = {inputTypeId, OMNI_VARCHAR};
        }
        auto sig = std::make_shared<FunctionSignature>("json_exists", argTypes, OMNI_BOOLEAN);
        auto fn = VectorFunction::Find(sig);
        ASSERT_NE(fn, nullptr) << "json_exists not found for arity " << argTypes.size()
                               << " jsonType=" << inputTypeId;

        auto outputType = std::make_shared<DataType>(OMNI_BOOLEAN);
        ExecutionContext ctx;
        ctx.SetResultRowSize(static_cast<int32_t>(jsonInputs.size()));
        std::stack<BaseVector*> args;
        // Stack is LIFO: push json first, then path, then onError (popped in reverse).
        args.push(jsonVec);
        args.push(pathVec);
        BaseVector* onErrorVec = nullptr;
        if (hasOnError) {
            onErrorVec = CreateStringVector({onErrorLiteral});
            ASSERT_NE(onErrorVec, nullptr);
            onErrorVec->SetNotNull(0);
            args.push(onErrorVec);
        }
        BaseVector* result = nullptr;
        ASSERT_NO_THROW(fn->Apply(args, outputType, result, &ctx));
        ASSERT_NE(result, nullptr);
        auto* typed = dynamic_cast<Vector<bool>*>(result);
        ASSERT_NE(typed, nullptr);

        for (size_t i = 0; i < expected.size(); ++i) {
            if (!expected[i].has_value()) {
                EXPECT_TRUE(result->IsNull(static_cast<int32_t>(i)))
                    << "row " << i << " json='" << jsonInputs[i] << "' path='" << pathInputs[i]
                    << "' expected NULL";
            } else {
                EXPECT_FALSE(result->IsNull(static_cast<int32_t>(i)))
                    << "row " << i << " json='" << jsonInputs[i] << "' path='" << pathInputs[i]
                    << "' expected non-NULL";
                EXPECT_EQ(typed->GetValue(static_cast<int32_t>(i)), expected[i].value())
                    << "row " << i << " json='" << jsonInputs[i] << "' path='" << pathInputs[i]
                    << "' onError='" << onErrorLiteral << "'";
            }
        }
        delete result;
    }
};

// ============================================================================
// Basic existence (sql_functions.yml examples), default FALSE ON ERROR
// ============================================================================

TEST(JsonExistsTest, BasicExistsFromDocs) {
    JsonExistsTestHelper::Run(
        {R"({"a": true})", R"({"a": true})", R"({"a": [{ "b": 1 }]})"},
        {"$.a", "$.b", "$.a[0].b"},
        {true, false, true});
}

TEST(JsonExistsTest, NestedObjectAccess) {
    JsonExistsTestHelper::Run(
        {R"({"a": {"b": "c"}})", R"({"a": {"b": "c"}})", R"({"a": {"b": "c"}})"},
        {"$.a", "$.a.b", "$.a.c"},
        {true, true, false});
}

TEST(JsonExistsTest, ArrayElementAccess) {
    JsonExistsTestHelper::Run(
        {R"({"a": [1, 2, 3]})", R"({"a": [1, 2, 3]})", R"({"a": [1, 2, 3]})"},
        {"$.a[0]", "$.a[2]", "$.a[3]"}, // last is out of bounds -> not found
        {true, true, false});
}

TEST(JsonExistsTest, RootPathExists) {
    JsonExistsTestHelper::Run(
        {R"({"a": 1})", "[]", "{}", "1"},
        {"$", "$", "$", "$"},
        {true, true, true, true});
}

// A path that resolves to a JSON null value is treated as "not exists" — Flink's
// Jayway-based jsonExists returns `context.obj != null`, and a JSON null leaf yields a
// Java null (not a NullNode), so JSON_EXISTS('{"a": null}', '$.a') = FALSE. Crucially
// this is NOT an error: ON ERROR does NOT apply (no exception is thrown), so even
// `strict $.a` with ON ERROR TRUE/FALSE/UNKNOWN stays FALSE (not the ON ERROR value).
TEST(JsonExistsTest, PathToJsonNullOrReturnsFalse) {
    // default STRICT, default ON ERROR FALSE -> FALSE
    JsonExistsTestHelper::Run(
        {R"({"a": null})", R"({"a": null})"},
        {"$.a", "$.b"},
        {false, false});
    // strict $.a with each ON ERROR: all FALSE (ON ERROR does not apply to JSON null leaf)
    JsonExistsTestHelper::Run({R"({"a": null})"}, {"strict $.a"}, "TRUE", {false});
    JsonExistsTestHelper::Run({R"({"a": null})"}, {"strict $.a"}, "FALSE", {false});
    JsonExistsTestHelper::Run({R"({"a": null})"}, {"strict $.a"}, "UNKNOWN", {false});
    // lax $.a -> FALSE as well
    JsonExistsTestHelper::Run({R"({"a": null})"}, {"lax $.a"}, {false});
}

// A nested path through a JSON null intermediate is NOT_FOUND (an error in STRICT):
// {"a": null} with $.a.b — navigating .b on a null is an error in STRICT -> ON ERROR.
TEST(JsonExistsTest, PathThroughJsonNullOrIntermediateIsError) {
    // strict $.a.b, default ON ERROR FALSE -> FALSE (via ON ERROR)
    JsonExistsTestHelper::Run({R"({"a": null})"}, {"strict $.a.b"}, {false});
    // strict $.a.b TRUE ON ERROR -> TRUE (ON ERROR applies, unlike the leaf-null case)
    JsonExistsTestHelper::Run({R"({"a": null})"}, {"strict $.a.b"}, "TRUE", {true});
    // lax $.a.b -> FALSE (suppressed)
    JsonExistsTestHelper::Run({R"({"a": null})"}, {"lax $.a.b"}, {false});
}

TEST(JsonExistsTest, TypeMismatchIsNotFound) {
    // $.a.b on a scalar 'a' -> not found (default STRICT -> ON ERROR FALSE -> false)
    JsonExistsTestHelper::Run(
        {R"({"a": 1})"},
        {"$.a.b"},
        {false});
}

// ============================================================================
// Path mode: lax vs strict (default strict)
// ============================================================================

TEST(JsonExistsTest, LaxNotFoundIsFalse) {
    JsonExistsTestHelper::Run(
        {R"({"a": true})", R"({"a": true})"},
        {"lax $.a", "lax $.b"},
        {true, false});
}

TEST(JsonExistsTest, StrictNotFoundIsOnError) {
    // strict $.b not found; default ON ERROR = FALSE -> false
    JsonExistsTestHelper::Run(
        {R"({"a": true})"},
        {"strict $.b"},
        {false});
}

TEST(JsonExistsTest, LaxInvalidJsonIsFalse) {
    JsonExistsTestHelper::Run(
        {"{bad", "{bad"},
        {"lax $.a", "$.a"}, // lax suppresses; strict default -> ON ERROR FALSE -> false
        {false, false});
}

// Uses the same document shape as JsonFunctionsITCase json-exists.json
TEST(JsonExistsTest, ItCaseDocumentPaths) {
    std::string doc = R"({
        "type": "post",
        "author": {"name": "Jon Doe", "address": {"country": "Germany", "city": "Munich"}},
        "metadata": {"tags": ["flink", "streaming", "json"], "references": [{"name": "GitHub", "url": "x"}]}
    })";
    JsonExistsTestHelper::Run(
        {doc, doc, doc, doc, doc, doc, doc},
        {"lax $",
         "lax $.type",
         "lax $.author.address.city",
         "lax $.metadata.tags[0]",
         "lax $.metadata.tags[3]",          // out of bounds -> not found
         "lax $.metadata.references[0].url",
         "lax $.metadata.references[0].invalid"}, // not found
        {true, true, true, true, false, true, false});
}

// ============================================================================
// ON ERROR four branches (strict mode triggers error via not-found / invalid JSON)
// ============================================================================

TEST(JsonExistsTest, OnErrorFalseDefault) {
    // 2-arg form: default FALSE
    JsonExistsTestHelper::Run(
        {R"({"a": true})"},
        {"strict $.invalid"},
        {false});
    // 3-arg explicit FALSE
    JsonExistsTestHelper::Run(
        {R"({"a": true})"},
        {"strict $.invalid"},
        "FALSE",
        {false});
}

TEST(JsonExistsTest, OnErrorTrue) {
    JsonExistsTestHelper::Run(
        {R"({"a": true})"},
        {"strict $.invalid"},
        "TRUE",
        {true});
}

TEST(JsonExistsTest, OnErrorUnknownIsNull) {
    JsonExistsTestHelper::Run(
        {R"({"a": true})"},
        {"strict $.invalid"},
        "UNKNOWN",
        {std::nullopt});
}

TEST(JsonExistsTest, OnErrorErrorThrows) {
    // ON ERROR = ERROR -> Apply throws. Build inputs manually since Run asserts no throw.
    BaseVector* jsonVec = JsonExistsTestHelper::CreateStringVector({R"({"a": true})"});
    ASSERT_NE(jsonVec, nullptr);
    jsonVec->SetNotNull(0);
    BaseVector* pathVec = JsonExistsTestHelper::CreateStringVector({"strict $.invalid"});
    ASSERT_NE(pathVec, nullptr);
    pathVec->SetNotNull(0);
    BaseVector* errVec = JsonExistsTestHelper::CreateStringVector({"ERROR"});
    ASSERT_NE(errVec, nullptr);
    errVec->SetNotNull(0);
    std::vector<DataTypeId> argTypes = {OMNI_VARCHAR, OMNI_VARCHAR, OMNI_VARCHAR};
    auto sig = std::make_shared<FunctionSignature>("json_exists", argTypes, OMNI_BOOLEAN);
    auto fn = VectorFunction::Find(sig);
    ASSERT_NE(fn, nullptr);
    auto outputType = std::make_shared<DataType>(OMNI_BOOLEAN);
    ExecutionContext ctx;
    ctx.SetResultRowSize(1);
    std::stack<BaseVector*> args;
    args.push(jsonVec);
    args.push(pathVec);
    args.push(errVec);
    BaseVector* result = nullptr;
    EXPECT_ANY_THROW(fn->Apply(args, outputType, result, &ctx));
    delete result;
}

TEST(JsonExistsTest, OnErrorInvalidJsonStrict) {
    // invalid JSON + strict: each ON ERROR branch
    JsonExistsTestHelper::Run({"{bad"}, {"$.a"}, "FALSE", {false});
    JsonExistsTestHelper::Run({"{bad"}, {"$.a"}, "TRUE", {true});
    JsonExistsTestHelper::Run({"{bad"}, {"$.a"}, "UNKNOWN", {std::nullopt});
}

// ============================================================================
// NULL input -> NULL output (NOT false; differs from IsJson)
// ============================================================================

TEST(JsonExistsTest, NullJsonInputIsNull) {
    BaseVector* jsonVec = JsonExistsTestHelper::CreateStringVector({R"({"a":1})", R"({"a":1})"});
    ASSERT_NE(jsonVec, nullptr);
    jsonVec->SetNull(0); // row 0 NULL -> NULL
    jsonVec->SetNotNull(1);
    BaseVector* pathVec = JsonExistsTestHelper::CreateStringVector({"$.a", "$.a"});
    ASSERT_NE(pathVec, nullptr);
    pathVec->SetNotNull(0);
    pathVec->SetNotNull(1);
    std::vector<DataTypeId> argTypes = {OMNI_VARCHAR, OMNI_VARCHAR};
    auto sig = std::make_shared<FunctionSignature>("json_exists", argTypes, OMNI_BOOLEAN);
    auto fn = VectorFunction::Find(sig);
    ASSERT_NE(fn, nullptr);
    auto outputType = std::make_shared<DataType>(OMNI_BOOLEAN);
    ExecutionContext ctx;
    ctx.SetResultRowSize(2);
    std::stack<BaseVector*> args;
    args.push(jsonVec);
    args.push(pathVec);
    BaseVector* result = nullptr;
    ASSERT_NO_THROW(fn->Apply(args, outputType, result, &ctx));
    auto* typed = dynamic_cast<Vector<bool>*>(result);
    ASSERT_NE(typed, nullptr);
    EXPECT_TRUE(result->IsNull(0)) << "NULL json input must yield NULL, not FALSE";
    EXPECT_FALSE(result->IsNull(1));
    EXPECT_TRUE(typed->GetValue(1));
    delete result;
}

TEST(JsonExistsTest, NullPathInputIsNull) {
    BaseVector* jsonVec = JsonExistsTestHelper::CreateStringVector({R"({"a":1})", R"({"a":1})"});
    ASSERT_NE(jsonVec, nullptr);
    jsonVec->SetNotNull(0);
    jsonVec->SetNotNull(1);
    BaseVector* pathVec = JsonExistsTestHelper::CreateStringVector({"$.a", "$.a"});
    ASSERT_NE(pathVec, nullptr);
    pathVec->SetNull(1); // row 1 path NULL -> NULL
    pathVec->SetNotNull(0);
    std::vector<DataTypeId> argTypes = {OMNI_VARCHAR, OMNI_VARCHAR};
    auto sig = std::make_shared<FunctionSignature>("json_exists", argTypes, OMNI_BOOLEAN);
    auto fn = VectorFunction::Find(sig);
    ASSERT_NE(fn, nullptr);
    auto outputType = std::make_shared<DataType>(OMNI_BOOLEAN);
    ExecutionContext ctx;
    ctx.SetResultRowSize(2);
    std::stack<BaseVector*> args;
    args.push(jsonVec);
    args.push(pathVec);
    BaseVector* result = nullptr;
    ASSERT_NO_THROW(fn->Apply(args, outputType, result, &ctx));
    auto* typed = dynamic_cast<Vector<bool>*>(result);
    ASSERT_NE(typed, nullptr);
    EXPECT_FALSE(result->IsNull(0));
    EXPECT_TRUE(typed->GetValue(0));
    EXPECT_TRUE(result->IsNull(1)) << "NULL path input must yield NULL, not FALSE";
    delete result;
}

// ============================================================================
// CHAR json input column (path/onError stay VARCHAR). Exercises the {CHAR, VARCHAR}
// and {CHAR, VARCHAR, VARCHAR} signatures registered for CHAR jsonValue columns.
// ============================================================================

TEST(JsonExistsTest, CharInputType) {
    // 2-arg form: {CHAR, VARCHAR}
    JsonExistsTestHelper::Run(
        {R"({"a": true})", R"({"a": true})", R"({"a": [1]})"},
        {"$.a", "$.b", "$.a[0]"},
        "",
        {true, false, true},
        OMNI_CHAR);
    // 3-arg form: {CHAR, VARCHAR, VARCHAR}
    JsonExistsTestHelper::Run(
        {R"({"a": true})"},
        {"strict $.invalid"},
        "TRUE",
        {true},
        OMNI_CHAR);
}

// ============================================================================
// Multi-row mixed batch (per-row independence)
// ============================================================================

TEST(JsonExistsTest, MultiRowMixedBatch) {
    JsonExistsTestHelper::Run(
        {R"({"a": 1})", R"({"a": 1})", R"({"a": [1,2]})", R"({"b": 2})", "[]", R"({"a":{"b":1}})"},
        {"$.a", "$.b", "$.a[0]", "$.a", "$[0]", "$.a.b"},
        {true, false, true, false, false, true});
}

// ============================================================================
// Whitespace + case-insensitive mode prefix tolerance
// ============================================================================

TEST(JsonExistsTest, WhitespaceAndCaseInsensitivePrefix) {
    JsonExistsTestHelper::Run(
        {R"({"a": 1})", R"({"a": 1})", R"({"a": 1})"},
        {"  lax  $.a", "STRICT $.b", "  $.a"},
        {true, false, true});
}
