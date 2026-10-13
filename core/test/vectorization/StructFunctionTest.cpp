/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: NameStruct function unit tests
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
#include "vector/row_vector.h"
#include "vector/array_vector.h"
#include "vector/map_vector.h"
#include "vector/vector_batch.h"
#include "vectorization/functions/NameStruct.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::op;
using namespace omniruntime::expressions;
using namespace omniruntime::TestUtil;
using namespace omniruntime::type;

static void RegisterOnce() {
    static int once = (RegisterFunctions::Register(), 1);
    (void)once;
}

TEST(StructFunctionTest, NameStructBoolean) {
    RegisterOnce();
    int rowSize = 2;
    bool vals[] = {true, false};
    auto col0 = VectorHelper::CreateFlatVector(nullptr, OMNI_BOOLEAN, rowSize);
    for (int i = 0; i < rowSize; i++) static_cast<Vector<bool>*>(col0)->SetValue(i, vals[i]);
    auto batch = new VectorBatch(rowSize);
    batch->Append(col0);
    std::vector<std::shared_ptr<DataType>> fieldTypes = {std::make_shared<DataType>(OMNI_BOOLEAN)};
    auto rowType = std::make_shared<RowType>(fieldTypes, std::vector<std::string>{"c0"});
    auto expr = new FuncExpr("name_struct", {new FieldExpr(0, std::make_shared<DataType>(OMNI_BOOLEAN))}, rowType);
    ExecutionContext* context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(batch, context);
    e.VisitExpr(*expr);
    BaseVector* result = e.GetResult();
    auto* rowVec = dynamic_cast<RowVector*>(result);
    ASSERT_NE(rowVec, nullptr);
    ASSERT_EQ(rowVec->ChildSize(), 1);
    auto* child = static_cast<Vector<bool>*>(rowVec->ChildAt(0).get());
    ASSERT_EQ(child->GetValue(0), true);
    ASSERT_EQ(child->GetValue(1), false);
    delete expr;
    delete result;
    delete context;
    delete batch;
}

TEST(StructFunctionTest, NameStructByte) {
    RegisterOnce();
    int rowSize = 2;
    auto col0 = VectorHelper::CreateFlatVector(nullptr, OMNI_BYTE, rowSize);
    static_cast<Vector<int8_t>*>(col0)->SetValue(0, 1);
    static_cast<Vector<int8_t>*>(col0)->SetValue(1, -2);
    auto batch = new VectorBatch(rowSize);
    batch->Append(col0);
    std::vector<std::shared_ptr<DataType>> fieldTypes = {std::make_shared<DataType>(OMNI_BYTE)};
    auto rowType = std::make_shared<RowType>(fieldTypes, std::vector<std::string>{"c0"});
    auto expr = new FuncExpr("name_struct", {new FieldExpr(0, std::make_shared<DataType>(OMNI_BYTE))}, rowType);
    ExecutionContext* context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(batch, context);
    e.VisitExpr(*expr);
    BaseVector* result = e.GetResult();
    auto* rowVec = dynamic_cast<RowVector*>(result);
    ASSERT_NE(rowVec, nullptr);
    auto* child = static_cast<Vector<int8_t>*>(rowVec->ChildAt(0).get());
    ASSERT_EQ(child->GetValue(0), 1);
    ASSERT_EQ(child->GetValue(1), -2);
    delete expr;
    delete result;
    delete context;
    delete batch;
}

TEST(StructFunctionTest, NameStructShort) {
    RegisterOnce();
    int rowSize = 2;
    auto col0 = VectorHelper::CreateFlatVector(nullptr, OMNI_SHORT, rowSize);
    static_cast<Vector<int16_t>*>(col0)->SetValue(0, 100);
    static_cast<Vector<int16_t>*>(col0)->SetValue(1, -200);
    auto batch = new VectorBatch(rowSize);
    batch->Append(col0);
    std::vector<std::shared_ptr<DataType>> fieldTypes = {std::make_shared<DataType>(OMNI_SHORT)};
    auto rowType = std::make_shared<RowType>(fieldTypes, std::vector<std::string>{"c0"});
    auto expr = new FuncExpr("name_struct", {new FieldExpr(0, std::make_shared<DataType>(OMNI_SHORT))}, rowType);
    ExecutionContext* context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(batch, context);
    e.VisitExpr(*expr);
    BaseVector* result = e.GetResult();
    auto* rowVec = dynamic_cast<RowVector*>(result);
    ASSERT_NE(rowVec, nullptr);
    auto* child = static_cast<Vector<int16_t>*>(rowVec->ChildAt(0).get());
    ASSERT_EQ(child->GetValue(0), 100);
    ASSERT_EQ(child->GetValue(1), -200);
    delete expr;
    delete result;
    delete context;
    delete batch;
}

TEST(StructFunctionTest, NameStructInt) {
    RegisterOnce();
    int rowSize = 2;
    auto col0 = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, rowSize);
    static_cast<Vector<int32_t>*>(col0)->SetValue(0, 42);
    static_cast<Vector<int32_t>*>(col0)->SetValue(1, -99);
    auto batch = new VectorBatch(rowSize);
    batch->Append(col0);
    std::vector<std::shared_ptr<DataType>> fieldTypes = {std::make_shared<DataType>(OMNI_INT)};
    auto rowType = std::make_shared<RowType>(fieldTypes, std::vector<std::string>{"c0"});
    auto expr = new FuncExpr("name_struct", {new FieldExpr(0, std::make_shared<DataType>(OMNI_INT))}, rowType);
    ExecutionContext* context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(batch, context);
    e.VisitExpr(*expr);
    BaseVector* result = e.GetResult();
    auto* rowVec = dynamic_cast<RowVector*>(result);
    ASSERT_NE(rowVec, nullptr);
    auto* child = static_cast<Vector<int32_t>*>(rowVec->ChildAt(0).get());
    ASSERT_EQ(child->GetValue(0), 42);
    ASSERT_EQ(child->GetValue(1), -99);
    delete expr;
    delete result;
    delete context;
    delete batch;
}

TEST(StructFunctionTest, NameStructLong) {
    RegisterOnce();
    int rowSize = 2;
    auto col0 = VectorHelper::CreateFlatVector(nullptr, OMNI_LONG, rowSize);
    static_cast<Vector<int64_t>*>(col0)->SetValue(0, 1000000LL);
    static_cast<Vector<int64_t>*>(col0)->SetValue(1, -1000000LL);
    auto batch = new VectorBatch(rowSize);
    batch->Append(col0);
    std::vector<std::shared_ptr<DataType>> fieldTypes = {std::make_shared<DataType>(OMNI_LONG)};
    auto rowType = std::make_shared<RowType>(fieldTypes, std::vector<std::string>{"c0"});
    auto expr = new FuncExpr("name_struct", {new FieldExpr(0, std::make_shared<DataType>(OMNI_LONG))}, rowType);
    ExecutionContext* context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(batch, context);
    e.VisitExpr(*expr);
    BaseVector* result = e.GetResult();
    auto* rowVec = dynamic_cast<RowVector*>(result);
    ASSERT_NE(rowVec, nullptr);
    auto* child = static_cast<Vector<int64_t>*>(rowVec->ChildAt(0).get());
    ASSERT_EQ(child->GetValue(0), 1000000LL);
    ASSERT_EQ(child->GetValue(1), -1000000LL);
    delete expr;
    delete result;
    delete context;
    delete batch;
}

TEST(StructFunctionTest, NameStructFloat) {
    RegisterOnce();
    int rowSize = 2;
    auto col0 = VectorHelper::CreateFlatVector(nullptr, OMNI_FLOAT, rowSize);
    static_cast<Vector<float>*>(col0)->SetValue(0, 1.5f);
    static_cast<Vector<float>*>(col0)->SetValue(1, -2.5f);
    auto batch = new VectorBatch(rowSize);
    batch->Append(col0);
    std::vector<std::shared_ptr<DataType>> fieldTypes = {std::make_shared<DataType>(OMNI_FLOAT)};
    auto rowType = std::make_shared<RowType>(fieldTypes, std::vector<std::string>{"c0"});
    auto expr = new FuncExpr("name_struct", {new FieldExpr(0, std::make_shared<DataType>(OMNI_FLOAT))}, rowType);
    ExecutionContext* context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(batch, context);
    e.VisitExpr(*expr);
    BaseVector* result = e.GetResult();
    auto* rowVec = dynamic_cast<RowVector*>(result);
    ASSERT_NE(rowVec, nullptr);
    auto* child = static_cast<Vector<float>*>(rowVec->ChildAt(0).get());
    ASSERT_FLOAT_EQ(child->GetValue(0), 1.5f);
    ASSERT_FLOAT_EQ(child->GetValue(1), -2.5f);
    delete expr;
    delete result;
    delete context;
    delete batch;
}

TEST(StructFunctionTest, NameStructDouble) {
    RegisterOnce();
    int rowSize = 2;
    auto col0 = VectorHelper::CreateFlatVector(nullptr, OMNI_DOUBLE, rowSize);
    static_cast<Vector<double>*>(col0)->SetValue(0, 3.14);
    static_cast<Vector<double>*>(col0)->SetValue(1, -2.71);
    auto batch = new VectorBatch(rowSize);
    batch->Append(col0);
    std::vector<std::shared_ptr<DataType>> fieldTypes = {std::make_shared<DataType>(OMNI_DOUBLE)};
    auto rowType = std::make_shared<RowType>(fieldTypes, std::vector<std::string>{"c0"});
    auto expr = new FuncExpr("name_struct", {new FieldExpr(0, std::make_shared<DataType>(OMNI_DOUBLE))}, rowType);
    ExecutionContext* context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(batch, context);
    e.VisitExpr(*expr);
    BaseVector* result = e.GetResult();
    auto* rowVec = dynamic_cast<RowVector*>(result);
    ASSERT_NE(rowVec, nullptr);
    auto* child = static_cast<Vector<double>*>(rowVec->ChildAt(0).get());
    ASSERT_DOUBLE_EQ(child->GetValue(0), 3.14);
    ASSERT_DOUBLE_EQ(child->GetValue(1), -2.71);
    delete expr;
    delete result;
    delete context;
    delete batch;
}

TEST(StructFunctionTest, NameStructString) {
    RegisterOnce();
    int rowSize = 2;
    auto col0 = VectorHelper::CreateStringVector(rowSize);
    std::string a("hello");
    std::string b("world");
    static_cast<Vector<LargeStringContainer<std::string_view>>*>(col0)->SetValue(0, std::string_view(a));
    static_cast<Vector<LargeStringContainer<std::string_view>>*>(col0)->SetValue(1, std::string_view(b));
    auto batch = new VectorBatch(rowSize);
    batch->Append(col0);
    std::vector<std::shared_ptr<DataType>> fieldTypes = {std::make_shared<DataType>(OMNI_VARCHAR)};
    auto rowType = std::make_shared<RowType>(fieldTypes, std::vector<std::string>{"c0"});
    auto expr = new FuncExpr("name_struct", {new FieldExpr(0, std::make_shared<DataType>(OMNI_VARCHAR))}, rowType);
    ExecutionContext* context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(batch, context);
    e.VisitExpr(*expr);
    BaseVector* result = e.GetResult();
    auto* rowVec = dynamic_cast<RowVector*>(result);
    ASSERT_NE(rowVec, nullptr);
    auto* child = static_cast<Vector<LargeStringContainer<std::string_view>>*>(rowVec->ChildAt(0).get());
    ASSERT_EQ(child->GetValue(0), "hello");
    ASSERT_EQ(child->GetValue(1), "world");
    delete expr;
    delete result;
    delete context;
    delete batch;
}

TEST(StructFunctionTest, NameStructDate) {
    RegisterOnce();
    int rowSize = 2;
    auto col0 = VectorHelper::CreateFlatVector(nullptr, OMNI_DATE32, rowSize);
    static_cast<Vector<int32_t>*>(col0)->SetValue(0, 19723);
    static_cast<Vector<int32_t>*>(col0)->SetValue(1, 19999);
    auto batch = new VectorBatch(rowSize);
    batch->Append(col0);
    std::vector<std::shared_ptr<DataType>> fieldTypes = {std::make_shared<DataType>(OMNI_DATE32)};
    auto rowType = std::make_shared<RowType>(fieldTypes, std::vector<std::string>{"c0"});
    auto expr = new FuncExpr("name_struct", {new FieldExpr(0, std::make_shared<DataType>(OMNI_DATE32))}, rowType);
    ExecutionContext* context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(batch, context);
    e.VisitExpr(*expr);
    BaseVector* result = e.GetResult();
    auto* rowVec = dynamic_cast<RowVector*>(result);
    ASSERT_NE(rowVec, nullptr);
    auto* child = static_cast<Vector<int32_t>*>(rowVec->ChildAt(0).get());
    ASSERT_EQ(child->GetValue(0), 19723);
    ASSERT_EQ(child->GetValue(1), 19999);
    delete expr;
    delete result;
    delete context;
    delete batch;
}

TEST(StructFunctionTest, NameStructTimestamp) {
    RegisterOnce();
    int rowSize = 2;
    auto col0 = VectorHelper::CreateFlatVector(nullptr, OMNI_TIMESTAMP, rowSize);
    static_cast<Vector<int64_t>*>(col0)->SetValue(0, 1609459200000000LL);
    static_cast<Vector<int64_t>*>(col0)->SetValue(1, 1609545600000000LL);
    auto batch = new VectorBatch(rowSize);
    batch->Append(col0);
    std::vector<std::shared_ptr<DataType>> fieldTypes = {std::make_shared<DataType>(OMNI_TIMESTAMP)};
    auto rowType = std::make_shared<RowType>(fieldTypes, std::vector<std::string>{"c0"});
    auto expr = new FuncExpr("name_struct", {new FieldExpr(0, std::make_shared<DataType>(OMNI_TIMESTAMP))}, rowType);
    ExecutionContext* context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(batch, context);
    e.VisitExpr(*expr);
    BaseVector* result = e.GetResult();
    auto* rowVec = dynamic_cast<RowVector*>(result);
    ASSERT_NE(rowVec, nullptr);
    auto* child = static_cast<Vector<int64_t>*>(rowVec->ChildAt(0).get());
    ASSERT_EQ(child->GetValue(0), 1609459200000000LL);
    ASSERT_EQ(child->GetValue(1), 1609545600000000LL);
    delete expr;
    delete result;
    delete context;
    delete batch;
}

TEST(StructFunctionTest, NameStructDecimal64) {
    RegisterOnce();
    int rowSize = 2;
    auto decType = std::make_shared<Decimal64DataType>(18, 4);
    auto col0 = VectorHelper::CreateFlatVector(nullptr, OMNI_DECIMAL64, rowSize);
    static_cast<Vector<int64_t>*>(col0)->SetValue(0, 1234567890123456LL);
    static_cast<Vector<int64_t>*>(col0)->SetValue(1, -1234567890123456LL);
    auto batch = new VectorBatch(rowSize);
    batch->Append(col0);
    std::vector<std::shared_ptr<DataType>> fieldTypes = {decType};
    auto rowType = std::make_shared<RowType>(fieldTypes, std::vector<std::string>{"c0"});
    auto expr = new FuncExpr("name_struct", {new FieldExpr(0, decType)}, rowType);
    ExecutionContext* context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(batch, context);
    e.VisitExpr(*expr);
    BaseVector* result = e.GetResult();
    auto* rowVec = dynamic_cast<RowVector*>(result);
    ASSERT_NE(rowVec, nullptr);
    auto* child = static_cast<Vector<int64_t>*>(rowVec->ChildAt(0).get());
    ASSERT_EQ(child->GetValue(0), 1234567890123456LL);
    ASSERT_EQ(child->GetValue(1), -1234567890123456LL);
    delete expr;
    delete result;
    delete context;
    delete batch;
}

TEST(StructFunctionTest, NameStructDecimal128) {
    RegisterOnce();
    int rowSize = 1;
    auto decType = std::make_shared<Decimal128DataType>(38, 10);
    auto col0 = VectorHelper::CreateFlatVector(nullptr, OMNI_DECIMAL128, rowSize);
    Decimal128 v("12345678901234567890123456789012345678");
    static_cast<Vector<Decimal128>*>(col0)->SetValue(0, v);
    auto batch = new VectorBatch(rowSize);
    batch->Append(col0);
    std::vector<std::shared_ptr<DataType>> fieldTypes = {decType};
    auto rowType = std::make_shared<RowType>(fieldTypes, std::vector<std::string>{"c0"});
    auto expr = new FuncExpr("name_struct", {new FieldExpr(0, decType)}, rowType);
    ExecutionContext* context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(batch, context);
    e.VisitExpr(*expr);
    BaseVector* result = e.GetResult();
    auto* rowVec = dynamic_cast<RowVector*>(result);
    ASSERT_NE(rowVec, nullptr);
    auto* child = static_cast<Vector<Decimal128>*>(rowVec->ChildAt(0).get());
    ASSERT_EQ(child->GetValue(0).ToString(), v.ToString());
    delete expr;
    delete result;
    delete context;
    delete batch;
}

TEST(StructFunctionTest, NameStructBinary) {
    RegisterOnce();
    int rowSize = 2;
    auto col0 = VectorHelper::CreateStringVector(rowSize);
    std::string x("bin1");
    std::string y("bin2");
    static_cast<Vector<LargeStringContainer<std::string_view>>*>(col0)->SetValue(0, std::string_view(x));
    static_cast<Vector<LargeStringContainer<std::string_view>>*>(col0)->SetValue(1, std::string_view(y));
    auto batch = new VectorBatch(rowSize);
    batch->Append(col0);
    std::vector<std::shared_ptr<DataType>> fieldTypes = {std::make_shared<DataType>(OMNI_VARBINARY)};
    auto rowType = std::make_shared<RowType>(fieldTypes, std::vector<std::string>{"c0"});
    auto expr = new FuncExpr("name_struct", {new FieldExpr(0, std::make_shared<DataType>(OMNI_VARBINARY))}, rowType);
    ExecutionContext* context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(batch, context);
    e.VisitExpr(*expr);
    BaseVector* result = e.GetResult();
    auto* rowVec = dynamic_cast<RowVector*>(result);
    ASSERT_NE(rowVec, nullptr);
    auto* child = static_cast<Vector<LargeStringContainer<std::string_view>>*>(rowVec->ChildAt(0).get());
    ASSERT_EQ(child->GetValue(0), "bin1");
    ASSERT_EQ(child->GetValue(1), "bin2");
    delete expr;
    delete result;
    delete context;
    delete batch;
}

TEST(StructFunctionTest, NameStructMultiField) {
    RegisterOnce();
    int rowSize = 2;
    auto col0 = VectorHelper::CreateFlatVector(nullptr, OMNI_BOOLEAN, rowSize);
    auto col1 = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, rowSize);
    auto col2 = VectorHelper::CreateFlatVector(nullptr, OMNI_DOUBLE, rowSize);
    static_cast<Vector<bool>*>(col0)->SetValue(0, true);
    static_cast<Vector<bool>*>(col0)->SetValue(1, false);
    static_cast<Vector<int32_t>*>(col1)->SetValue(0, 10);
    static_cast<Vector<int32_t>*>(col1)->SetValue(1, 20);
    static_cast<Vector<double>*>(col2)->SetValue(0, 1.5);
    static_cast<Vector<double>*>(col2)->SetValue(1, 2.5);
    auto batch = new VectorBatch(rowSize);
    batch->Append(col0);
    batch->Append(col1);
    batch->Append(col2);
    std::vector<std::shared_ptr<DataType>> fieldTypes = {
        std::make_shared<DataType>(OMNI_BOOLEAN),
        std::make_shared<DataType>(OMNI_INT),
        std::make_shared<DataType>(OMNI_DOUBLE)
    };
    auto rowType = std::make_shared<RowType>(fieldTypes, std::vector<std::string>{"a", "b", "c"});
    auto expr = new FuncExpr("name_struct", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_BOOLEAN)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_INT)),
        new FieldExpr(2, std::make_shared<DataType>(OMNI_DOUBLE))
    }, rowType);
    ExecutionContext* context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(batch, context);
    e.VisitExpr(*expr);
    BaseVector* result = e.GetResult();
    auto* rowVec = dynamic_cast<RowVector*>(result);
    ASSERT_NE(rowVec, nullptr);
    ASSERT_EQ(rowVec->ChildSize(), 3);
    ASSERT_EQ(static_cast<Vector<bool>*>(rowVec->ChildAt(0).get())->GetValue(0), true);
    ASSERT_EQ(static_cast<Vector<int32_t>*>(rowVec->ChildAt(1).get())->GetValue(0), 10);
    ASSERT_DOUBLE_EQ(static_cast<Vector<double>*>(rowVec->ChildAt(2).get())->GetValue(0), 1.5);
    ASSERT_EQ(static_cast<Vector<bool>*>(rowVec->ChildAt(0).get())->GetValue(1), false);
    ASSERT_EQ(static_cast<Vector<int32_t>*>(rowVec->ChildAt(1).get())->GetValue(1), 20);
    ASSERT_DOUBLE_EQ(static_cast<Vector<double>*>(rowVec->ChildAt(2).get())->GetValue(1), 2.5);
    delete expr;
    delete result;
    delete context;
    delete batch;
}

TEST(StructFunctionTest, NameStructIgnoresOuterStackEntries) {
    RegisterOnce();
    int rowSize = 2;
    auto sentinel = VectorHelper::CreateFlatVector(nullptr, OMNI_DOUBLE, rowSize);
    static_cast<Vector<double> *>(sentinel)->SetValue(0, 11.1);
    static_cast<Vector<double> *>(sentinel)->SetValue(1, 22.2);

    auto name1 = VectorHelper::CreateStringVector(rowSize);
    auto value1 = VectorHelper::CreateStringVector(rowSize);
    auto name2 = VectorHelper::CreateStringVector(rowSize);
    auto value2 = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, rowSize);

    std::string fieldName1("name");
    std::string fieldValue1("izvyp");
    std::string fieldName2("age");
    static_cast<Vector<LargeStringContainer<std::string_view>> *>(name1)->SetValue(0, std::string_view(fieldName1));
    static_cast<Vector<LargeStringContainer<std::string_view>> *>(name1)->SetValue(1, std::string_view(fieldName1));
    static_cast<Vector<LargeStringContainer<std::string_view>> *>(value1)->SetValue(0, std::string_view(fieldValue1));
    static_cast<Vector<LargeStringContainer<std::string_view>> *>(value1)->SetValue(1, std::string_view(fieldValue1));
    static_cast<Vector<LargeStringContainer<std::string_view>> *>(name2)->SetValue(0, std::string_view(fieldName2));
    static_cast<Vector<LargeStringContainer<std::string_view>> *>(name2)->SetValue(1, std::string_view(fieldName2));
    static_cast<Vector<int32_t> *>(value2)->SetValue(0, 4);
    static_cast<Vector<int32_t> *>(value2)->SetValue(1, 5);

    std::stack<BaseVector *> args;
    args.push(sentinel);
    args.push(name1);
    args.push(value1);
    args.push(name2);
    args.push(value2);

    std::vector<std::shared_ptr<DataType>> fieldTypes = {
        std::make_shared<DataType>(OMNI_VARCHAR),
        std::make_shared<DataType>(OMNI_INT)
    };
    auto rowType = std::make_shared<RowType>(fieldTypes, std::vector<std::string>{"name", "age"});
    ExecutionContext *context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    NameStructFunction func;
    BaseVector *result = nullptr;
    func.Apply(args, rowType, result, context);

    auto *rowVec = dynamic_cast<RowVector *>(result);
    ASSERT_NE(rowVec, nullptr);
    ASSERT_EQ(rowVec->ChildSize(), 2);
    auto *nameChild = static_cast<Vector<LargeStringContainer<std::string_view>> *>(rowVec->ChildAt(0).get());
    auto *ageChild = static_cast<Vector<int32_t> *>(rowVec->ChildAt(1).get());
    ASSERT_EQ(nameChild->GetValue(0), "izvyp");
    ASSERT_EQ(nameChild->GetValue(1), "izvyp");
    ASSERT_EQ(ageChild->GetValue(0), 4);
    ASSERT_EQ(ageChild->GetValue(1), 5);
    ASSERT_EQ(args.size(), 1);
    ASSERT_EQ(args.top(), sentinel);

    delete result;
    delete context;
    delete sentinel;
}

TEST(StructFunctionTest, NameStructWithNull) {
    RegisterOnce();
    int rowSize = 2;
    auto col0 = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, rowSize);
    static_cast<Vector<int32_t>*>(col0)->SetValue(0, 1);
    col0->SetNull(1);
    auto batch = new VectorBatch(rowSize);
    batch->Append(col0);
    std::vector<std::shared_ptr<DataType>> fieldTypes = {std::make_shared<DataType>(OMNI_INT)};
    auto rowType = std::make_shared<RowType>(fieldTypes, std::vector<std::string>{"c0"});
    auto expr = new FuncExpr("name_struct", {new FieldExpr(0, std::make_shared<DataType>(OMNI_INT))}, rowType);
    ExecutionContext* context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(batch, context);
    e.VisitExpr(*expr);
    BaseVector* result = e.GetResult();
    auto* rowVec = dynamic_cast<RowVector*>(result);
    ASSERT_NE(rowVec, nullptr);
    ASSERT_FALSE(rowVec->ChildAt(0)->IsNull(0));
    ASSERT_TRUE(rowVec->ChildAt(0)->IsNull(1));
    ASSERT_EQ(static_cast<Vector<int32_t>*>(rowVec->ChildAt(0).get())->GetValue(0), 1);
    delete expr;
    delete result;
    delete context;
    delete batch;
}

TEST(StructFunctionTest, NameStructArray) {
    RegisterOnce();
    int rowSize = 2;
    auto elemVec = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, 4);
    static_cast<Vector<int32_t>*>(elemVec)->SetValue(0, 1);
    static_cast<Vector<int32_t>*>(elemVec)->SetValue(1, 2);
    static_cast<Vector<int32_t>*>(elemVec)->SetValue(2, 3);
    static_cast<Vector<int32_t>*>(elemVec)->SetValue(3, 4);
    auto col0 = new ArrayVector(rowSize, std::shared_ptr<BaseVector>(elemVec));
    col0->SetOffset(0, 0);
    col0->SetOffset(1, 2);
    col0->SetSize(1, 2);
    auto batch = new VectorBatch(rowSize);
    batch->Append(col0);
    auto arrayType = std::make_shared<ArrayType>(std::make_shared<DataType>(OMNI_INT));
    std::vector<std::shared_ptr<DataType>> fieldTypes = {arrayType};
    auto rowType = std::make_shared<RowType>(fieldTypes, std::vector<std::string>{"c0"});
    auto expr = new FuncExpr("name_struct", {new FieldExpr(0, arrayType)}, rowType);
    ExecutionContext* context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(batch, context);
    e.VisitExpr(*expr);
    BaseVector* result = e.GetResult();
    auto* rowVec = dynamic_cast<RowVector*>(result);
    ASSERT_NE(rowVec, nullptr);
    ASSERT_EQ(rowVec->ChildSize(), 1);
    auto* arrChild = dynamic_cast<ArrayVector*>(rowVec->ChildAt(0).get());
    ASSERT_NE(arrChild, nullptr);
    ASSERT_EQ(arrChild->GetSize(0), 2);
    ASSERT_EQ(arrChild->GetSize(1), 2);
    auto* elem = static_cast<Vector<int32_t>*>(arrChild->GetElementVector().get());
    ASSERT_EQ(elem->GetValue(0), 1);
    ASSERT_EQ(elem->GetValue(1), 2);
    ASSERT_EQ(elem->GetValue(2), 3);
    ASSERT_EQ(elem->GetValue(3), 4);
    delete expr;
    delete result;
    delete context;
    delete batch;
}

TEST(StructFunctionTest, NameStructMap) {
    RegisterOnce();
    int rowSize = 2;
    auto keyVec = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, 3);
    static_cast<Vector<int32_t>*>(keyVec)->SetValue(0, 1);
    static_cast<Vector<int32_t>*>(keyVec)->SetValue(1, 2);
    static_cast<Vector<int32_t>*>(keyVec)->SetValue(2, 3);
    auto valVec = VectorHelper::CreateStringVector(3);
    std::string a("a"), b("b"), c("c");
    static_cast<Vector<LargeStringContainer<std::string_view>>*>(valVec)->SetValue(0, std::string_view(a));
    static_cast<Vector<LargeStringContainer<std::string_view>>*>(valVec)->SetValue(1, std::string_view(b));
    static_cast<Vector<LargeStringContainer<std::string_view>>*>(valVec)->SetValue(2, std::string_view(c));
    auto col0 = new MapVector(rowSize);
    col0->SetKeyVector(std::shared_ptr<BaseVector>(keyVec));
    col0->SetValueVector(std::shared_ptr<BaseVector>(valVec));
    col0->SetOffset(0, 0);
    col0->SetOffset(1, 2);
    col0->SetOffset(2, 3);
    auto batch = new VectorBatch(rowSize);
    batch->Append(col0);
    auto mapType = std::make_shared<MapType>(std::make_shared<DataType>(OMNI_INT), std::make_shared<DataType>(OMNI_VARCHAR));
    std::vector<std::shared_ptr<DataType>> fieldTypes = {mapType};
    auto rowType = std::make_shared<RowType>(fieldTypes, std::vector<std::string>{"c0"});
    auto expr = new FuncExpr("name_struct", {new FieldExpr(0, mapType)}, rowType);
    ExecutionContext* context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(batch, context);
    e.VisitExpr(*expr);
    BaseVector* result = e.GetResult();
    auto* rowVec = dynamic_cast<RowVector*>(result);
    ASSERT_NE(rowVec, nullptr);
    ASSERT_EQ(rowVec->ChildSize(), 1);
    auto* mapChild = dynamic_cast<MapVector*>(rowVec->ChildAt(0).get());
    ASSERT_NE(mapChild, nullptr);
    ASSERT_EQ(mapChild->GetSize(0), 2);
    ASSERT_EQ(mapChild->GetSize(1), 1);
    delete expr;
    delete result;
    delete context;
    delete batch;
}

TEST(StructFunctionTest, NameStructStruct) {
    RegisterOnce();
    int rowSize = 2;
    auto innerCol = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, rowSize);
    static_cast<Vector<int32_t>*>(innerCol)->SetValue(0, 10);
    static_cast<Vector<int32_t>*>(innerCol)->SetValue(1, 20);
    auto innerRow = new RowVector(rowSize);
    innerRow->AddChild(innerCol);
    auto batch = new VectorBatch(rowSize);
    batch->Append(innerRow);
    std::vector<std::shared_ptr<DataType>> innerTypes = {std::make_shared<DataType>(OMNI_INT)};
    auto innerRowType = std::make_shared<RowType>(innerTypes, std::vector<std::string>{"x"});
    std::vector<std::shared_ptr<DataType>> fieldTypes = {innerRowType};
    auto rowType = std::make_shared<RowType>(fieldTypes, std::vector<std::string>{"c0"});
    auto expr = new FuncExpr("name_struct", {new FieldExpr(0, innerRowType)}, rowType);
    ExecutionContext* context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(batch, context);
    e.VisitExpr(*expr);
    BaseVector* result = e.GetResult();
    auto* rowVec = dynamic_cast<RowVector*>(result);
    ASSERT_NE(rowVec, nullptr);
    ASSERT_EQ(rowVec->ChildSize(), 1);
    auto* nestedRow = dynamic_cast<RowVector*>(rowVec->ChildAt(0).get());
    ASSERT_NE(nestedRow, nullptr);
    ASSERT_EQ(nestedRow->ChildSize(), 1);
    auto* intVec = static_cast<Vector<int32_t>*>(nestedRow->ChildAt(0).get());
    ASSERT_EQ(intVec->GetValue(0), 10);
    ASSERT_EQ(intVec->GetValue(1), 20);
    delete expr;
    delete result;
    delete context;
    delete batch;
}
