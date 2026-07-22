/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: json_object function unit tests.
 *
 * Covers Flink SQL JSON_OBJECT([[KEY] key VALUE value]* [ { NULL | ABSENT } ON NULL ]).
 *
 * json_object is a heterogeneous variadic function dispatched by FuncExpr (expressions.cpp)
 * via funcName == "json_object" (same pattern as named_struct / concat_ws). These tests drive
 * the runtime path (VectorBatch -> FuncExpr -> ExprEval::VisitExpr -> Apply), mirroring
 * StructFunctionTest.cpp, rather than calling Apply directly. This ensures the test links the
 * same way StructFunctionTest does and exercises the production dispatch path.
 *
 * Argument layout (aligned with OmniAdaptor JSON_EXISTS symbol-literal convention):
 *   arg[0]            : VARCHAR literal "NULL" or "ABSENT" (ON NULL behavior)
 *   arg[1,3,5,...]    : VARCHAR literal key (non-NULL string)
 *   arg[2,4,6,...]    : value of any supported JSON type (column / literal, nullable)
 *
 * See docs/expression-design/json_object_design.md.
 */

#include <gtest/gtest.h>
#include <stack>
#include <string>
#include <vector>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/ExprEval.h"
#include "expression/expressions.h"
#include "type/data_type.h"
#include "vector/vector_helper.h"
#include "vector/vector.h"
#include "vector/row_vector.h"
#include "vector/array_vector.h"
#include "vector/vector_batch.h"
#include "vectorization/functions/JsonObject.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::op;
using namespace omniruntime::expressions;
using namespace omniruntime::TestUtil;
using namespace omniruntime::type;

// Linking Register keeps the test binary consistent with StructFunctionTest
// (which calls RegisterFunctions::Register()).
static void RegisterOnce() {
    static int once = (RegisterFunctions::Register(), 1);
    (void)once;
}

class JsonObjectTestHelper {
public:
    static constexpr int32_t kRowSize = 2;

    static DataTypePtr VarcharType() { return std::make_shared<DataType>(OMNI_VARCHAR); }

    // VARCHAR literal (onNull flag / key). LiteralExpr takes ownership of the string pointer.
    static Expr* StrLiteral(const std::string& s) {
        return new LiteralExpr(new std::string(s), VarcharType());
    }

    // Build a VectorBatch with a single VARCHAR column holding `values` (one row each).
    static VectorBatch* MakeStringBatch(const std::vector<std::string>& values) {
        auto* batch = new VectorBatch(static_cast<int32_t>(values.size()));
        auto* col = VectorHelper::CreateStringVector(static_cast<uint32_t>(values.size()));
        col->SetIsField(true);
        auto* typed = static_cast<Vector<LargeStringContainer<std::string_view>>*>(col);
        for (size_t i = 0; i < values.size(); ++i) {
            typed->SetValue(static_cast<int32_t>(i), std::string_view(values[i]));
        }
        batch->Append(col);
        return batch;
    }

    // Build a VectorBatch with a single typed column.
    template <typename T>
    static VectorBatch* MakeScalarBatch(const std::vector<T>& values, DataTypeId typeId) {
        auto* batch = new VectorBatch(static_cast<int32_t>(values.size()));
        auto* col = VectorHelper::CreateFlatVector(typeId, static_cast<int32_t>(values.size()));
        col->SetIsField(true);
        auto* typed = static_cast<Vector<T>*>(col);
        for (size_t i = 0; i < values.size(); ++i) {
            typed->SetValue(static_cast<int32_t>(i), values[i]);
        }
        batch->Append(col);
        return batch;
    }

    // Run json_object via the runtime path: FuncExpr -> ExprEval::VisitExpr -> Apply.
    static BaseVector* RunJsonObject(const std::vector<Expr*>& args, VectorBatch* batch,
        int32_t rowSize) {
        auto returnType = VarcharType();
        auto* expr = new FuncExpr("json_object", args, returnType);
        auto* context = new ExecutionContext();
        context->SetResultRowSize(rowSize);
        ExprEval e(batch, context);
        e.VisitExpr(*expr);
        BaseVector* result = e.GetResult();
        delete expr;
        delete context;
        return result;
    }

    static void ValidateString(BaseVector* result, int row, const std::string& expected) {
        auto* strVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
        ASSERT_NE(strVec, nullptr);
        ASSERT_FALSE(strVec->IsNull(row)) << "row " << row << " unexpectedly null";
        std::string actual(strVec->GetValue(row));
        EXPECT_EQ(actual, expected) << "row " << row;
    }
};

// =============================================================================
// Basic object construction
// =============================================================================

TEST(JsonObjectTest, EmptyObject) {
    RegisterOnce();
    // JSON_OBJECT() with NULL ON NULL flag, no key/value pairs -> "{}".
    std::vector<Expr*> args = {JsonObjectTestHelper::StrLiteral("NULL")};
    auto* batch = new VectorBatch(JsonObjectTestHelper::kRowSize);
    BaseVector* result = JsonObjectTestHelper::RunJsonObject(args, batch, JsonObjectTestHelper::kRowSize);
    JsonObjectTestHelper::ValidateString(result, 0, "{}");
    JsonObjectTestHelper::ValidateString(result, 1, "{}");
    delete result;
    delete batch;
}

TEST(JsonObjectTest, SinglePairString) {
    RegisterOnce();
    // Value column "V1"; key + flag are literals.
    auto* batch = JsonObjectTestHelper::MakeStringBatch({"V1", "V1"});
    std::vector<Expr*> args = {
        JsonObjectTestHelper::StrLiteral("NULL"),
        JsonObjectTestHelper::StrLiteral("K1"),
        new FieldExpr(0, JsonObjectTestHelper::VarcharType())};
    BaseVector* result = JsonObjectTestHelper::RunJsonObject(args, batch, 2);
    JsonObjectTestHelper::ValidateString(result, 0, R"({"K1":"V1"})");
    delete result;
    delete batch;
}

TEST(JsonObjectTest, MultiPairString) {
    RegisterOnce();
    // Two value columns V1, V2; keys K1, K2.
    auto* batch = new VectorBatch(1);
    auto* c1 = VectorHelper::CreateStringVector(1);
    c1->SetIsField(true);
    static_cast<Vector<LargeStringContainer<std::string_view>>*>(c1)->SetValue(0, std::string_view("V1"));
    auto* c2 = VectorHelper::CreateStringVector(1);
    c2->SetIsField(true);
    static_cast<Vector<LargeStringContainer<std::string_view>>*>(c2)->SetValue(0, std::string_view("V2"));
    batch->Append(c1);
    batch->Append(c2);
    std::vector<Expr*> args = {
        JsonObjectTestHelper::StrLiteral("NULL"),
        JsonObjectTestHelper::StrLiteral("K1"), new FieldExpr(0, JsonObjectTestHelper::VarcharType()),
        JsonObjectTestHelper::StrLiteral("K2"), new FieldExpr(1, JsonObjectTestHelper::VarcharType())};
    BaseVector* result = JsonObjectTestHelper::RunJsonObject(args, batch, 1);
    JsonObjectTestHelper::ValidateString(result, 0, R"({"K1":"V1","K2":"V2"})");
    delete result;
    delete batch;
}

// =============================================================================
// ON NULL behavior
// =============================================================================

TEST(JsonObjectTest, NullOnNull_NullValue) {
    RegisterOnce();
    // Value column row 0 is NULL; flag "NULL" -> {"K1":null}
    auto* batch = new VectorBatch(1);
    auto* c = VectorHelper::CreateStringVector(1);
    c->SetIsField(true);
    c->SetNull(0);
    batch->Append(c);
    std::vector<Expr*> args = {
        JsonObjectTestHelper::StrLiteral("NULL"),
        JsonObjectTestHelper::StrLiteral("K1"),
        new FieldExpr(0, JsonObjectTestHelper::VarcharType())};
    BaseVector* result = JsonObjectTestHelper::RunJsonObject(args, batch, 1);
    JsonObjectTestHelper::ValidateString(result, 0, R"({"K1":null})");
    delete result;
    delete batch;
}

TEST(JsonObjectTest, AbsentOnNull_NullValue) {
    RegisterOnce();
    auto* batch = new VectorBatch(1);
    auto* c = VectorHelper::CreateStringVector(1);
    c->SetIsField(true);
    c->SetNull(0);
    batch->Append(c);
    std::vector<Expr*> args = {
        JsonObjectTestHelper::StrLiteral("ABSENT"),
        JsonObjectTestHelper::StrLiteral("K1"),
        new FieldExpr(0, JsonObjectTestHelper::VarcharType())};
    BaseVector* result = JsonObjectTestHelper::RunJsonObject(args, batch, 1);
    JsonObjectTestHelper::ValidateString(result, 0, "{}");
    delete result;
    delete batch;
}

TEST(JsonObjectTest, NullOnNull_MixedNullAndValue) {
    RegisterOnce();
    // (K1, NULL, K2, V2) with NULL ON NULL -> {"K1":null,"K2":"V2"}
    auto* batch = new VectorBatch(1);
    auto* c1 = VectorHelper::CreateStringVector(1);
    c1->SetIsField(true);
    c1->SetNull(0);
    auto* c2 = VectorHelper::CreateStringVector(1);
    c2->SetIsField(true);
    static_cast<Vector<LargeStringContainer<std::string_view>>*>(c2)->SetValue(0, std::string_view("V2"));
    batch->Append(c1);
    batch->Append(c2);
    std::vector<Expr*> args = {
        JsonObjectTestHelper::StrLiteral("NULL"),
        JsonObjectTestHelper::StrLiteral("K1"), new FieldExpr(0, JsonObjectTestHelper::VarcharType()),
        JsonObjectTestHelper::StrLiteral("K2"), new FieldExpr(1, JsonObjectTestHelper::VarcharType())};
    BaseVector* result = JsonObjectTestHelper::RunJsonObject(args, batch, 1);
    JsonObjectTestHelper::ValidateString(result, 0, R"({"K1":null,"K2":"V2"})");
    delete result;
    delete batch;
}

TEST(JsonObjectTest, AbsentOnNull_MixedNullAndValue) {
    RegisterOnce();
    auto* batch = new VectorBatch(1);
    auto* c1 = VectorHelper::CreateStringVector(1);
    c1->SetIsField(true);
    c1->SetNull(0);
    auto* c2 = VectorHelper::CreateStringVector(1);
    c2->SetIsField(true);
    static_cast<Vector<LargeStringContainer<std::string_view>>*>(c2)->SetValue(0, std::string_view("V2"));
    batch->Append(c1);
    batch->Append(c2);
    std::vector<Expr*> args = {
        JsonObjectTestHelper::StrLiteral("ABSENT"),
        JsonObjectTestHelper::StrLiteral("K1"), new FieldExpr(0, JsonObjectTestHelper::VarcharType()),
        JsonObjectTestHelper::StrLiteral("K2"), new FieldExpr(1, JsonObjectTestHelper::VarcharType())};
    BaseVector* result = JsonObjectTestHelper::RunJsonObject(args, batch, 1);
    JsonObjectTestHelper::ValidateString(result, 0, R"({"K2":"V2"})");
    delete result;
    delete batch;
}

TEST(JsonObjectTest, DefaultOnNullWhenFlagUnrecognized) {
    RegisterOnce();
    auto* batch = new VectorBatch(1);
    auto* c = VectorHelper::CreateStringVector(1);
    c->SetIsField(true);
    c->SetNull(0);
    batch->Append(c);
    std::vector<Expr*> args = {
        JsonObjectTestHelper::StrLiteral("WEIRD"),
        JsonObjectTestHelper::StrLiteral("K1"),
        new FieldExpr(0, JsonObjectTestHelper::VarcharType())};
    BaseVector* result = JsonObjectTestHelper::RunJsonObject(args, batch, 1);
    JsonObjectTestHelper::ValidateString(result, 0, R"({"K1":null})");
    delete result;
    delete batch;
}

// =============================================================================
// Typed values (delegated to JsonStringFunction serialization)
// =============================================================================

TEST(JsonObjectTest, IntValue) {
    RegisterOnce();
    auto* batch = JsonObjectTestHelper::MakeScalarBatch<int32_t>({5, 5}, OMNI_INT);
    std::vector<Expr*> args = {
        JsonObjectTestHelper::StrLiteral("NULL"),
        JsonObjectTestHelper::StrLiteral("K1"),
        new FieldExpr(0, std::make_shared<DataType>(OMNI_INT))};
    BaseVector* result = JsonObjectTestHelper::RunJsonObject(args, batch, 2);
    JsonObjectTestHelper::ValidateString(result, 0, R"({"K1":5})");
    JsonObjectTestHelper::ValidateString(result, 1, R"({"K1":5})");
    delete result;
    delete batch;
}

TEST(JsonObjectTest, LongValue) {
    RegisterOnce();
    auto* batch = JsonObjectTestHelper::MakeScalarBatch<int64_t>(
        {9223372036854775807LL, 9223372036854775807LL}, OMNI_LONG);
    std::vector<Expr*> args = {
        JsonObjectTestHelper::StrLiteral("NULL"),
        JsonObjectTestHelper::StrLiteral("K1"),
        new FieldExpr(0, std::make_shared<DataType>(OMNI_LONG))};
    BaseVector* result = JsonObjectTestHelper::RunJsonObject(args, batch, 2);
    JsonObjectTestHelper::ValidateString(result, 0, R"({"K1":9223372036854775807})");
    delete result;
    delete batch;
}

TEST(JsonObjectTest, DoubleValue) {
    RegisterOnce();
    auto* batch = JsonObjectTestHelper::MakeScalarBatch<double>({3.14, 3.14}, OMNI_DOUBLE);
    std::vector<Expr*> args = {
        JsonObjectTestHelper::StrLiteral("NULL"),
        JsonObjectTestHelper::StrLiteral("K1"),
        new FieldExpr(0, std::make_shared<DataType>(OMNI_DOUBLE))};
    BaseVector* result = JsonObjectTestHelper::RunJsonObject(args, batch, 2);
    JsonObjectTestHelper::ValidateString(result, 0, R"({"K1":3.14})");
    delete result;
    delete batch;
}

TEST(JsonObjectTest, BooleanValue) {
    RegisterOnce();
    auto* batch = JsonObjectTestHelper::MakeScalarBatch<bool>({true, false}, OMNI_BOOLEAN);
    std::vector<Expr*> args = {
        JsonObjectTestHelper::StrLiteral("NULL"),
        JsonObjectTestHelper::StrLiteral("K1"),
        new FieldExpr(0, std::make_shared<DataType>(OMNI_BOOLEAN))};
    BaseVector* result = JsonObjectTestHelper::RunJsonObject(args, batch, 2);
    JsonObjectTestHelper::ValidateString(result, 0, R"({"K1":true})");
    JsonObjectTestHelper::ValidateString(result, 1, R"({"K1":false})");
    delete result;
    delete batch;
}

TEST(JsonObjectTest, ArrayValue) {
    RegisterOnce();
    // Value column is ARRAY<BIGINT> with one row holding [1, 2].
    int rowSize = 1;
    std::vector<std::vector<int64_t>> arrayData = {{1, 2}};
    std::vector<int64_t> offsets = {0};
    size_t total = 0;
    std::vector<int64_t> allElements;
    for (const auto& arr : arrayData) {
        for (int64_t v : arr) allElements.push_back(v);
        total += arr.size();
        offsets.push_back(static_cast<int64_t>(total));
    }
    auto elemVec = VectorHelper::CreateFlatVector(OMNI_LONG, static_cast<int32_t>(allElements.size()));
    auto* longVec = static_cast<Vector<int64_t>*>(elemVec);
    for (size_t i = 0; i < allElements.size(); ++i) longVec->SetValue(static_cast<int32_t>(i), allElements[i]);
    auto* arrVec = new ArrayVector(rowSize, std::shared_ptr<BaseVector>(elemVec));
    arrVec->SetIsField(true);
    for (int i = 0; i <= rowSize; ++i) arrVec->SetOffset(i, static_cast<int32_t>(offsets[i]));
    auto* batch = new VectorBatch(rowSize);
    batch->Append(arrVec);
    auto elemType = std::make_shared<DataType>(OMNI_LONG);
    auto arrayType = std::make_shared<ArrayType>(elemType);
    std::vector<Expr*> args = {
        JsonObjectTestHelper::StrLiteral("NULL"),
        JsonObjectTestHelper::StrLiteral("K1"),
        new FieldExpr(0, arrayType)};
    BaseVector* result = JsonObjectTestHelper::RunJsonObject(args, batch, rowSize);
    JsonObjectTestHelper::ValidateString(result, 0, R"({"K1":[1,2]})");
    delete result;
    delete batch;
}

TEST(JsonObjectTest, RowValue) {
    RegisterOnce();
    // Value column is ROW<a INT, b INT> = (1, 2) -> {"a":1,"b":2}
    int rowSize = 1;
    auto* rowVec = new RowVector(rowSize);
    auto* fa = VectorHelper::CreateFlatVector(OMNI_INT, rowSize);
    static_cast<Vector<int32_t>*>(fa)->SetValue(0, 1);
    auto* fb = VectorHelper::CreateFlatVector(OMNI_INT, rowSize);
    static_cast<Vector<int32_t>*>(fb)->SetValue(0, 2);
    rowVec->AddChild(std::shared_ptr<BaseVector>(fa));
    rowVec->AddChild(std::shared_ptr<BaseVector>(fb));
    rowVec->SetIsField(true);
    auto* batch = new VectorBatch(rowSize);
    batch->Append(rowVec);
    std::vector<std::shared_ptr<DataType>> fieldTypes = {
        std::make_shared<DataType>(OMNI_INT), std::make_shared<DataType>(OMNI_INT)};
    std::vector<std::string> fieldNames = {"a", "b"};
    auto rowType = std::make_shared<RowType>(fieldTypes, fieldNames);
    std::vector<Expr*> args = {
        JsonObjectTestHelper::StrLiteral("NULL"),
        JsonObjectTestHelper::StrLiteral("K1"),
        new FieldExpr(0, rowType)};
    BaseVector* result = JsonObjectTestHelper::RunJsonObject(args, batch, rowSize);
    JsonObjectTestHelper::ValidateString(result, 0, R"({"K1":{"a":1,"b":2}})");
    delete result;
    delete batch;
}

// =============================================================================
// Key / value escaping
// =============================================================================

TEST(JsonObjectTest, KeyWithSpecialChars) {
    RegisterOnce();
    auto* batch = JsonObjectTestHelper::MakeStringBatch({"V", "V"});
    std::vector<Expr*> args = {
        JsonObjectTestHelper::StrLiteral("NULL"),
        JsonObjectTestHelper::StrLiteral("K\"1"),
        new FieldExpr(0, JsonObjectTestHelper::VarcharType())};
    BaseVector* result = JsonObjectTestHelper::RunJsonObject(args, batch, 2);
    JsonObjectTestHelper::ValidateString(result, 0, R"({"K\"1":"V"})");
    delete result;
    delete batch;
}

TEST(JsonObjectTest, ValueWithSpecialChars) {
    RegisterOnce();
    // Value contains quote, backslash, newline -> all escaped.
    std::string val = "V\"n\\\n";
    auto* batch = JsonObjectTestHelper::MakeStringBatch({val, val});
    std::vector<Expr*> args = {
        JsonObjectTestHelper::StrLiteral("NULL"),
        JsonObjectTestHelper::StrLiteral("K1"),
        new FieldExpr(0, JsonObjectTestHelper::VarcharType())};
    BaseVector* result = JsonObjectTestHelper::RunJsonObject(args, batch, 2);
    JsonObjectTestHelper::ValidateString(result, 0, R"({"K1":"V\"n\\\n"})");
    delete result;
    delete batch;
}

// =============================================================================
// Vectorized (multi-row) behavior
// =============================================================================

TEST(JsonObjectTest, MultiRowVectorized) {
    RegisterOnce();
    // Two rows with different string values; constant key + flag (literals).
    auto* batch = JsonObjectTestHelper::MakeStringBatch({"V1", "V2"});
    std::vector<Expr*> args = {
        JsonObjectTestHelper::StrLiteral("NULL"),
        JsonObjectTestHelper::StrLiteral("K1"),
        new FieldExpr(0, JsonObjectTestHelper::VarcharType())};
    BaseVector* result = JsonObjectTestHelper::RunJsonObject(args, batch, 2);
    JsonObjectTestHelper::ValidateString(result, 0, R"({"K1":"V1"})");
    JsonObjectTestHelper::ValidateString(result, 1, R"({"K1":"V2"})");
    delete result;
    delete batch;
}

TEST(JsonObjectTest, ConstKeyVariableIntValue) {
    RegisterOnce();
    auto* batch = JsonObjectTestHelper::MakeScalarBatch<int32_t>({10, 20}, OMNI_INT);
    std::vector<Expr*> args = {
        JsonObjectTestHelper::StrLiteral("NULL"),
        JsonObjectTestHelper::StrLiteral("K1"),
        new FieldExpr(0, std::make_shared<DataType>(OMNI_INT))};
    BaseVector* result = JsonObjectTestHelper::RunJsonObject(args, batch, 2);
    JsonObjectTestHelper::ValidateString(result, 0, R"({"K1":10})");
    JsonObjectTestHelper::ValidateString(result, 1, R"({"K1":20})");
    delete result;
    delete batch;
}

TEST(JsonObjectTest, MultiRowMixedNullAbsentOnNull) {
    RegisterOnce();
    // ABSENT ON NULL: row 0 value NULL -> {}, row 1 value "V2" -> {"K1":"V2"}.
    auto* batch = new VectorBatch(2);
    auto* c = VectorHelper::CreateStringVector(2);
    c->SetIsField(true);
    auto* typed = static_cast<Vector<LargeStringContainer<std::string_view>>*>(c);
    typed->SetValue(0, std::string_view("x"));
    typed->SetValue(1, std::string_view("V2"));
    c->SetNull(0);  // row 0 value is NULL
    batch->Append(c);
    std::vector<Expr*> args = {
        JsonObjectTestHelper::StrLiteral("ABSENT"),
        JsonObjectTestHelper::StrLiteral("K1"),
        new FieldExpr(0, JsonObjectTestHelper::VarcharType())};
    BaseVector* result = JsonObjectTestHelper::RunJsonObject(args, batch, 2);
    JsonObjectTestHelper::ValidateString(result, 0, "{}");
    JsonObjectTestHelper::ValidateString(result, 1, R"({"K1":"V2"})");
    delete result;
    delete batch;
}

// =============================================================================
// Multiple pairs with mixed value types
// =============================================================================

TEST(JsonObjectTest, MultiplePairsMixedTypes) {
    RegisterOnce();
    // (K1, 1, K2, "s", K3, TRUE) -> {"K1":1,"K2":"s","K3":true}
    // Values come from three columns: INT, VARCHAR, BOOLEAN. Single row.
    int rowSize = 1;
    auto* cInt = VectorHelper::CreateFlatVector(OMNI_INT, rowSize);
    static_cast<Vector<int32_t>*>(cInt)->SetValue(0, 1);
    auto* cStr = VectorHelper::CreateStringVector(static_cast<uint32_t>(rowSize));
    cStr->SetIsField(true);
    static_cast<Vector<LargeStringContainer<std::string_view>>*>(cStr)->SetValue(0, std::string_view("s"));
    auto* cBool = VectorHelper::CreateFlatVector(OMNI_BOOLEAN, rowSize);
    static_cast<Vector<bool>*>(cBool)->SetValue(0, true);
    auto* batch = new VectorBatch(rowSize);
    batch->Append(cInt);
    batch->Append(cStr);
    batch->Append(cBool);
    std::vector<Expr*> args = {
        JsonObjectTestHelper::StrLiteral("NULL"),
        JsonObjectTestHelper::StrLiteral("K1"), new FieldExpr(0, std::make_shared<DataType>(OMNI_INT)),
        JsonObjectTestHelper::StrLiteral("K2"), new FieldExpr(1, JsonObjectTestHelper::VarcharType()),
        JsonObjectTestHelper::StrLiteral("K3"), new FieldExpr(2, std::make_shared<DataType>(OMNI_BOOLEAN))};
    BaseVector* result = JsonObjectTestHelper::RunJsonObject(args, batch, rowSize);
    JsonObjectTestHelper::ValidateString(result, 0, R"({"K1":1,"K2":"s","K3":true})");
    delete result;
    delete batch;
}
