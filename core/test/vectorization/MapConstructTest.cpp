/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: MapFunction (map constructor) unit tests
 */

#include <gtest/gtest.h>
#include <string>
#include <vector>
#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/ExprEval.h"
#include "expression/expressions.h"
#include "type/data_type.h"
#include "vector/vector_helper.h"
#include "vector/map_vector.h"
#include "vector/array_vector.h"
#include "vector/row_vector.h"
#include "vector/vector_batch.h"

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

TEST(MapConstructTest, MapBooleanKeyValue) {
    RegisterOnce();
    int rowSize = 2;
    auto col0 = VectorHelper::CreateFlatVector(nullptr, OMNI_BOOLEAN, rowSize);
    static_cast<Vector<bool>*>(col0)->SetValue(0, true);
    static_cast<Vector<bool>*>(col0)->SetValue(1, false);
    auto col1 = VectorHelper::CreateFlatVector(nullptr, OMNI_BOOLEAN, rowSize);
    static_cast<Vector<bool>*>(col1)->SetValue(0, false);
    static_cast<Vector<bool>*>(col1)->SetValue(1, true);
    auto batch = new VectorBatch(rowSize);
    batch->Append(col0);
    batch->Append(col1);

    auto mapType = std::make_shared<MapType>(
        std::make_shared<DataType>(OMNI_BOOLEAN), std::make_shared<DataType>(OMNI_BOOLEAN));
    auto expr = new FuncExpr("map", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_BOOLEAN)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_BOOLEAN))
    }, mapType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(batch, context);
    e.VisitExpr(*expr);
    BaseVector* result = e.GetResult();

    auto* mapVec = dynamic_cast<MapVector*>(result);
    ASSERT_NE(mapVec, nullptr);
    auto* keyVec = static_cast<Vector<bool>*>(mapVec->GetKeyVector().get());
    auto* valVec = static_cast<Vector<bool>*>(mapVec->GetValueVector().get());
    ASSERT_EQ(mapVec->GetSize(0), 1);
    ASSERT_EQ(mapVec->GetSize(1), 1);
    ASSERT_EQ(keyVec->GetValue(0), true);
    ASSERT_EQ(valVec->GetValue(0), false);
    ASSERT_EQ(keyVec->GetValue(1), false);
    ASSERT_EQ(valVec->GetValue(1), true);

    delete expr;
    delete result;
    delete context;
    delete batch;
}

TEST(MapConstructTest, MapByteKeyValue) {
    RegisterOnce();
    int rowSize = 2;
    auto col0 = VectorHelper::CreateFlatVector(nullptr, OMNI_BYTE, rowSize);
    static_cast<Vector<int8_t>*>(col0)->SetValue(0, 1);
    static_cast<Vector<int8_t>*>(col0)->SetValue(1, 2);
    auto col1 = VectorHelper::CreateFlatVector(nullptr, OMNI_BYTE, rowSize);
    static_cast<Vector<int8_t>*>(col1)->SetValue(0, 10);
    static_cast<Vector<int8_t>*>(col1)->SetValue(1, 20);
    auto batch = new VectorBatch(rowSize);
    batch->Append(col0);
    batch->Append(col1);

    auto mapType = std::make_shared<MapType>(
        std::make_shared<DataType>(OMNI_BYTE), std::make_shared<DataType>(OMNI_BYTE));
    auto expr = new FuncExpr("map", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_BYTE)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_BYTE))
    }, mapType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(batch, context);
    e.VisitExpr(*expr);
    BaseVector* result = e.GetResult();

    auto* mapVec = dynamic_cast<MapVector*>(result);
    ASSERT_NE(mapVec, nullptr);
    auto* keyVec = static_cast<Vector<int8_t>*>(mapVec->GetKeyVector().get());
    auto* valVec = static_cast<Vector<int8_t>*>(mapVec->GetValueVector().get());
    ASSERT_EQ(keyVec->GetValue(0), 1);
    ASSERT_EQ(valVec->GetValue(0), 10);
    ASSERT_EQ(keyVec->GetValue(1), 2);
    ASSERT_EQ(valVec->GetValue(1), 20);

    delete expr;
    delete result;
    delete context;
    delete batch;
}

TEST(MapConstructTest, MapShortKeyValue) {
    RegisterOnce();
    int rowSize = 2;
    auto col0 = VectorHelper::CreateFlatVector(nullptr, OMNI_SHORT, rowSize);
    static_cast<Vector<int16_t>*>(col0)->SetValue(0, 100);
    static_cast<Vector<int16_t>*>(col0)->SetValue(1, 200);
    auto col1 = VectorHelper::CreateFlatVector(nullptr, OMNI_SHORT, rowSize);
    static_cast<Vector<int16_t>*>(col1)->SetValue(0, 1000);
    static_cast<Vector<int16_t>*>(col1)->SetValue(1, 2000);
    auto batch = new VectorBatch(rowSize);
    batch->Append(col0);
    batch->Append(col1);

    auto mapType = std::make_shared<MapType>(
        std::make_shared<DataType>(OMNI_SHORT), std::make_shared<DataType>(OMNI_SHORT));
    auto expr = new FuncExpr("map", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_SHORT)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_SHORT))
    }, mapType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(batch, context);
    e.VisitExpr(*expr);
    BaseVector* result = e.GetResult();

    auto* mapVec = dynamic_cast<MapVector*>(result);
    ASSERT_NE(mapVec, nullptr);
    auto* keyVec = static_cast<Vector<int16_t>*>(mapVec->GetKeyVector().get());
    auto* valVec = static_cast<Vector<int16_t>*>(mapVec->GetValueVector().get());
    ASSERT_EQ(keyVec->GetValue(0), 100);
    ASSERT_EQ(valVec->GetValue(0), 1000);
    ASSERT_EQ(keyVec->GetValue(1), 200);
    ASSERT_EQ(valVec->GetValue(1), 2000);

    delete expr;
    delete result;
    delete context;
    delete batch;
}

TEST(MapConstructTest, MapIntKeyValue) {
    RegisterOnce();
    int rowSize = 3;
    auto col0 = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, rowSize);
    static_cast<Vector<int32_t>*>(col0)->SetValue(0, 1);
    static_cast<Vector<int32_t>*>(col0)->SetValue(1, 2);
    static_cast<Vector<int32_t>*>(col0)->SetValue(2, 3);
    auto col1 = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, rowSize);
    static_cast<Vector<int32_t>*>(col1)->SetValue(0, 10);
    static_cast<Vector<int32_t>*>(col1)->SetValue(1, 20);
    static_cast<Vector<int32_t>*>(col1)->SetValue(2, 30);
    auto batch = new VectorBatch(rowSize);
    batch->Append(col0);
    batch->Append(col1);

    auto mapType = std::make_shared<MapType>(
        std::make_shared<DataType>(OMNI_INT), std::make_shared<DataType>(OMNI_INT));
    auto expr = new FuncExpr("map", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_INT)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_INT))
    }, mapType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(batch, context);
    e.VisitExpr(*expr);
    BaseVector* result = e.GetResult();

    auto* mapVec = dynamic_cast<MapVector*>(result);
    ASSERT_NE(mapVec, nullptr);
    auto* keyVec = static_cast<Vector<int32_t>*>(mapVec->GetKeyVector().get());
    auto* valVec = static_cast<Vector<int32_t>*>(mapVec->GetValueVector().get());
    ASSERT_EQ(mapVec->GetSize(0), 1);
    ASSERT_EQ(keyVec->GetValue(0), 1);
    ASSERT_EQ(valVec->GetValue(0), 10);
    ASSERT_EQ(keyVec->GetValue(1), 2);
    ASSERT_EQ(valVec->GetValue(1), 20);
    ASSERT_EQ(keyVec->GetValue(2), 3);
    ASSERT_EQ(valVec->GetValue(2), 30);

    delete expr;
    delete result;
    delete context;
    delete batch;
}

TEST(MapConstructTest, MapLongKeyValue) {
    RegisterOnce();
    int rowSize = 2;
    auto col0 = VectorHelper::CreateFlatVector(nullptr, OMNI_LONG, rowSize);
    static_cast<Vector<int64_t>*>(col0)->SetValue(0, 100L);
    static_cast<Vector<int64_t>*>(col0)->SetValue(1, 200L);
    auto col1 = VectorHelper::CreateFlatVector(nullptr, OMNI_LONG, rowSize);
    static_cast<Vector<int64_t>*>(col1)->SetValue(0, 1000L);
    static_cast<Vector<int64_t>*>(col1)->SetValue(1, 2000L);
    auto batch = new VectorBatch(rowSize);
    batch->Append(col0);
    batch->Append(col1);

    auto mapType = std::make_shared<MapType>(
        std::make_shared<DataType>(OMNI_LONG), std::make_shared<DataType>(OMNI_LONG));
    auto expr = new FuncExpr("map", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_LONG)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_LONG))
    }, mapType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(batch, context);
    e.VisitExpr(*expr);
    BaseVector* result = e.GetResult();

    auto* mapVec = dynamic_cast<MapVector*>(result);
    ASSERT_NE(mapVec, nullptr);
    auto* keyVec = static_cast<Vector<int64_t>*>(mapVec->GetKeyVector().get());
    auto* valVec = static_cast<Vector<int64_t>*>(mapVec->GetValueVector().get());
    ASSERT_EQ(keyVec->GetValue(0), 100L);
    ASSERT_EQ(valVec->GetValue(0), 1000L);
    ASSERT_EQ(keyVec->GetValue(1), 200L);
    ASSERT_EQ(valVec->GetValue(1), 2000L);

    delete expr;
    delete result;
    delete context;
    delete batch;
}

TEST(MapConstructTest, MapFloatKeyValue) {
    RegisterOnce();
    int rowSize = 2;
    auto col0 = VectorHelper::CreateFlatVector(nullptr, OMNI_FLOAT, rowSize);
    static_cast<Vector<float>*>(col0)->SetValue(0, 1.5f);
    static_cast<Vector<float>*>(col0)->SetValue(1, 2.5f);
    auto col1 = VectorHelper::CreateFlatVector(nullptr, OMNI_FLOAT, rowSize);
    static_cast<Vector<float>*>(col1)->SetValue(0, 10.5f);
    static_cast<Vector<float>*>(col1)->SetValue(1, 20.5f);
    auto batch = new VectorBatch(rowSize);
    batch->Append(col0);
    batch->Append(col1);

    auto mapType = std::make_shared<MapType>(
        std::make_shared<DataType>(OMNI_FLOAT), std::make_shared<DataType>(OMNI_FLOAT));
    auto expr = new FuncExpr("map", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_FLOAT)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_FLOAT))
    }, mapType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(batch, context);
    e.VisitExpr(*expr);
    BaseVector* result = e.GetResult();

    auto* mapVec = dynamic_cast<MapVector*>(result);
    ASSERT_NE(mapVec, nullptr);
    auto* keyVec = static_cast<Vector<float>*>(mapVec->GetKeyVector().get());
    auto* valVec = static_cast<Vector<float>*>(mapVec->GetValueVector().get());
    ASSERT_FLOAT_EQ(keyVec->GetValue(0), 1.5f);
    ASSERT_FLOAT_EQ(valVec->GetValue(0), 10.5f);
    ASSERT_FLOAT_EQ(keyVec->GetValue(1), 2.5f);
    ASSERT_FLOAT_EQ(valVec->GetValue(1), 20.5f);

    delete expr;
    delete result;
    delete context;
    delete batch;
}

TEST(MapConstructTest, MapDoubleKeyValue) {
    RegisterOnce();
    int rowSize = 2;
    auto col0 = VectorHelper::CreateFlatVector(nullptr, OMNI_DOUBLE, rowSize);
    static_cast<Vector<double>*>(col0)->SetValue(0, 1.1);
    static_cast<Vector<double>*>(col0)->SetValue(1, 2.2);
    auto col1 = VectorHelper::CreateFlatVector(nullptr, OMNI_DOUBLE, rowSize);
    static_cast<Vector<double>*>(col1)->SetValue(0, 10.1);
    static_cast<Vector<double>*>(col1)->SetValue(1, 20.2);
    auto batch = new VectorBatch(rowSize);
    batch->Append(col0);
    batch->Append(col1);

    auto mapType = std::make_shared<MapType>(
        std::make_shared<DataType>(OMNI_DOUBLE), std::make_shared<DataType>(OMNI_DOUBLE));
    auto expr = new FuncExpr("map", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_DOUBLE)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_DOUBLE))
    }, mapType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(batch, context);
    e.VisitExpr(*expr);
    BaseVector* result = e.GetResult();

    auto* mapVec = dynamic_cast<MapVector*>(result);
    ASSERT_NE(mapVec, nullptr);
    auto* keyVec = static_cast<Vector<double>*>(mapVec->GetKeyVector().get());
    auto* valVec = static_cast<Vector<double>*>(mapVec->GetValueVector().get());
    ASSERT_DOUBLE_EQ(keyVec->GetValue(0), 1.1);
    ASSERT_DOUBLE_EQ(valVec->GetValue(0), 10.1);
    ASSERT_DOUBLE_EQ(keyVec->GetValue(1), 2.2);
    ASSERT_DOUBLE_EQ(valVec->GetValue(1), 20.2);

    delete expr;
    delete result;
    delete context;
    delete batch;
}

TEST(MapConstructTest, MapTimestampKeyValue) {
    RegisterOnce();
    int rowSize = 2;
    auto col0 = VectorHelper::CreateFlatVector(nullptr, OMNI_TIMESTAMP, rowSize);
    static_cast<Vector<int64_t>*>(col0)->SetValue(0, 1706522730000000L);
    static_cast<Vector<int64_t>*>(col0)->SetValue(1, 0L);
    auto col1 = VectorHelper::CreateFlatVector(nullptr, OMNI_TIMESTAMP, rowSize);
    static_cast<Vector<int64_t>*>(col1)->SetValue(0, 100L);
    static_cast<Vector<int64_t>*>(col1)->SetValue(1, 200L);
    auto batch = new VectorBatch(rowSize);
    batch->Append(col0);
    batch->Append(col1);

    auto mapType = std::make_shared<MapType>(
        std::make_shared<DataType>(OMNI_TIMESTAMP), std::make_shared<DataType>(OMNI_TIMESTAMP));
    auto expr = new FuncExpr("map", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_TIMESTAMP)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_TIMESTAMP))
    }, mapType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(batch, context);
    e.VisitExpr(*expr);
    BaseVector* result = e.GetResult();

    auto* mapVec = dynamic_cast<MapVector*>(result);
    ASSERT_NE(mapVec, nullptr);
    auto* keyVec = static_cast<Vector<int64_t>*>(mapVec->GetKeyVector().get());
    auto* valVec = static_cast<Vector<int64_t>*>(mapVec->GetValueVector().get());
    ASSERT_EQ(keyVec->GetValue(0), 1706522730000000L);
    ASSERT_EQ(valVec->GetValue(0), 100L);
    ASSERT_EQ(keyVec->GetValue(1), 0L);
    ASSERT_EQ(valVec->GetValue(1), 200L);

    delete expr;
    delete result;
    delete context;
    delete batch;
}

TEST(MapConstructTest, MapBinaryKeyValue) {
    RegisterOnce();
    int rowSize = 2;
    using StringVector = Vector<LargeStringContainer<std::string_view>>;
    auto* col0 = static_cast<StringVector*>(VectorHelper::CreateFlatVector(nullptr, OMNI_VARBINARY, rowSize));
    col0->SetValue(0, std::string_view("key1", 4));
    col0->SetValue(1, std::string_view("key2", 4));
    auto* col1 = static_cast<StringVector*>(VectorHelper::CreateFlatVector(nullptr, OMNI_VARBINARY, rowSize));
    col1->SetValue(0, std::string_view("val1", 4));
    col1->SetValue(1, std::string_view("val2", 4));
    auto batch = new VectorBatch(rowSize);
    batch->Append(col0);
    batch->Append(col1);

    auto mapType = std::make_shared<MapType>(
        std::make_shared<DataType>(OMNI_VARBINARY), std::make_shared<DataType>(OMNI_VARBINARY));
    auto expr = new FuncExpr("map", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_VARBINARY)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_VARBINARY))
    }, mapType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(batch, context);
    e.VisitExpr(*expr);
    BaseVector* result = e.GetResult();

    auto* mapVec = dynamic_cast<MapVector*>(result);
    ASSERT_NE(mapVec, nullptr);
    auto* keyVec = static_cast<StringVector*>(mapVec->GetKeyVector().get());
    auto* valVec = static_cast<StringVector*>(mapVec->GetValueVector().get());
    ASSERT_EQ(keyVec->GetValue(0), std::string_view("key1", 4));
    ASSERT_EQ(valVec->GetValue(0), std::string_view("val1", 4));
    ASSERT_EQ(keyVec->GetValue(1), std::string_view("key2", 4));
    ASSERT_EQ(valVec->GetValue(1), std::string_view("val2", 4));

    delete expr;
    delete result;
    delete context;
    delete batch;
}

TEST(MapConstructTest, MapIntKeyDoubleValue) {
    RegisterOnce();
    int rowSize = 3;
    auto col0 = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, rowSize);
    static_cast<Vector<int32_t>*>(col0)->SetValue(0, 1);
    static_cast<Vector<int32_t>*>(col0)->SetValue(1, 2);
    static_cast<Vector<int32_t>*>(col0)->SetValue(2, 3);
    auto col1 = VectorHelper::CreateFlatVector(nullptr, OMNI_DOUBLE, rowSize);
    static_cast<Vector<double>*>(col1)->SetValue(0, 4.0);
    static_cast<Vector<double>*>(col1)->SetValue(1, 5.0);
    static_cast<Vector<double>*>(col1)->SetValue(2, 6.0);
    auto batch = new VectorBatch(rowSize);
    batch->Append(col0);
    batch->Append(col1);

    auto mapType = std::make_shared<MapType>(
        std::make_shared<DataType>(OMNI_INT), std::make_shared<DataType>(OMNI_DOUBLE));
    auto expr = new FuncExpr("map", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_INT)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_DOUBLE))
    }, mapType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(batch, context);
    e.VisitExpr(*expr);
    BaseVector* result = e.GetResult();

    auto* mapVec = dynamic_cast<MapVector*>(result);
    ASSERT_NE(mapVec, nullptr);
    auto* keyVec = static_cast<Vector<int32_t>*>(mapVec->GetKeyVector().get());
    auto* valVec = static_cast<Vector<double>*>(mapVec->GetValueVector().get());
    for (int i = 0; i < rowSize; ++i) {
        ASSERT_EQ(mapVec->GetSize(i), 1);
    }
    ASSERT_EQ(keyVec->GetValue(0), 1);
    ASSERT_DOUBLE_EQ(valVec->GetValue(0), 4.0);
    ASSERT_EQ(keyVec->GetValue(1), 2);
    ASSERT_DOUBLE_EQ(valVec->GetValue(1), 5.0);
    ASSERT_EQ(keyVec->GetValue(2), 3);
    ASSERT_DOUBLE_EQ(valVec->GetValue(2), 6.0);

    delete expr;
    delete result;
    delete context;
    delete batch;
}

TEST(MapConstructTest, MapMultipleKeyValuePairs) {
    RegisterOnce();
    int rowSize = 2;
    auto col0 = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, rowSize);
    static_cast<Vector<int32_t>*>(col0)->SetValue(0, 1);
    static_cast<Vector<int32_t>*>(col0)->SetValue(1, 4);
    auto col1 = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, rowSize);
    static_cast<Vector<int32_t>*>(col1)->SetValue(0, 10);
    static_cast<Vector<int32_t>*>(col1)->SetValue(1, 40);
    auto col2 = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, rowSize);
    static_cast<Vector<int32_t>*>(col2)->SetValue(0, 2);
    static_cast<Vector<int32_t>*>(col2)->SetValue(1, 5);
    auto col3 = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, rowSize);
    static_cast<Vector<int32_t>*>(col3)->SetValue(0, 20);
    static_cast<Vector<int32_t>*>(col3)->SetValue(1, 50);
    auto batch = new VectorBatch(rowSize);
    batch->Append(col0);
    batch->Append(col1);
    batch->Append(col2);
    batch->Append(col3);

    auto mapType = std::make_shared<MapType>(
        std::make_shared<DataType>(OMNI_INT), std::make_shared<DataType>(OMNI_INT));
    auto expr = new FuncExpr("map", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_INT)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_INT)),
        new FieldExpr(2, std::make_shared<DataType>(OMNI_INT)),
        new FieldExpr(3, std::make_shared<DataType>(OMNI_INT))
    }, mapType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(batch, context);
    e.VisitExpr(*expr);
    BaseVector* result = e.GetResult();

    auto* mapVec = dynamic_cast<MapVector*>(result);
    ASSERT_NE(mapVec, nullptr);
    ASSERT_EQ(mapVec->GetSize(0), 2);
    ASSERT_EQ(mapVec->GetSize(1), 2);

    auto* keyVec = static_cast<Vector<int32_t>*>(mapVec->GetKeyVector().get());
    auto* valVec = static_cast<Vector<int32_t>*>(mapVec->GetValueVector().get());
    ASSERT_EQ(keyVec->GetValue(0), 1);
    ASSERT_EQ(valVec->GetValue(0), 10);
    ASSERT_EQ(keyVec->GetValue(1), 2);
    ASSERT_EQ(valVec->GetValue(1), 20);
    ASSERT_EQ(keyVec->GetValue(2), 4);
    ASSERT_EQ(valVec->GetValue(2), 40);
    ASSERT_EQ(keyVec->GetValue(3), 5);
    ASSERT_EQ(valVec->GetValue(3), 50);

    delete expr;
    delete result;
    delete context;
    delete batch;
}

TEST(MapConstructTest, MapDuplicateKeyLastWin) {
    RegisterOnce();
    int rowSize = 2;
    auto col0 = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, rowSize);
    static_cast<Vector<int32_t>*>(col0)->SetValue(0, 1);
    static_cast<Vector<int32_t>*>(col0)->SetValue(1, 3);
    auto col1 = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, rowSize);
    static_cast<Vector<int32_t>*>(col1)->SetValue(0, 100);
    static_cast<Vector<int32_t>*>(col1)->SetValue(1, 300);
    auto col2 = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, rowSize);
    static_cast<Vector<int32_t>*>(col2)->SetValue(0, 1);
    static_cast<Vector<int32_t>*>(col2)->SetValue(1, 4);
    auto col3 = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, rowSize);
    static_cast<Vector<int32_t>*>(col3)->SetValue(0, 999);
    static_cast<Vector<int32_t>*>(col3)->SetValue(1, 400);
    auto batch = new VectorBatch(rowSize);
    batch->Append(col0);
    batch->Append(col1);
    batch->Append(col2);
    batch->Append(col3);

    auto mapType = std::make_shared<MapType>(
        std::make_shared<DataType>(OMNI_INT), std::make_shared<DataType>(OMNI_INT));
    auto expr = new FuncExpr("map", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_INT)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_INT)),
        new FieldExpr(2, std::make_shared<DataType>(OMNI_INT)),
        new FieldExpr(3, std::make_shared<DataType>(OMNI_INT))
    }, mapType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(batch, context);
    e.VisitExpr(*expr);
    BaseVector* result = e.GetResult();

    auto* mapVec = dynamic_cast<MapVector*>(result);
    ASSERT_NE(mapVec, nullptr);
    ASSERT_EQ(mapVec->GetSize(0), 1);
    auto* keyVec = static_cast<Vector<int32_t>*>(mapVec->GetKeyVector().get());
    auto* valVec = static_cast<Vector<int32_t>*>(mapVec->GetValueVector().get());
    ASSERT_EQ(keyVec->GetValue(0), 1);
    ASSERT_EQ(valVec->GetValue(0), 999);

    ASSERT_EQ(mapVec->GetSize(1), 2);

    delete expr;
    delete result;
    delete context;
    delete batch;
}

TEST(MapConstructTest, MapArrayKeyValue) {
    RegisterOnce();
    int rowSize = 1;
    int32_t elems1[] = {1, 2, 3};
    auto elemVec1 = std::shared_ptr<BaseVector>(CreateVector<int32_t>(3, elems1));
    auto* arr1 = new ArrayVector(rowSize, elemVec1);
    arr1->SetOffset(0, 0);
    arr1->SetOffset(1, 3);

    int32_t elems2[] = {10, 20, 30};
    auto elemVec2 = std::shared_ptr<BaseVector>(CreateVector<int32_t>(3, elems2));
    auto* arr2 = new ArrayVector(rowSize, elemVec2);
    arr2->SetOffset(0, 0);
    arr2->SetOffset(1, 3);

    auto batch = new VectorBatch(rowSize);
    batch->Append(arr1);
    batch->Append(arr2);

    auto arrayType = std::make_shared<ArrayType>(std::make_shared<DataType>(OMNI_INT));
    auto mapType = std::make_shared<MapType>(arrayType, arrayType);
    auto expr = new FuncExpr("map", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_ARRAY))
    }, mapType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(batch, context);
    e.VisitExpr(*expr);
    BaseVector* result = e.GetResult();

    auto* mapVec = dynamic_cast<MapVector*>(result);
    ASSERT_NE(mapVec, nullptr);
    ASSERT_EQ(mapVec->GetSize(0), 1);

    delete expr;
    delete result;
    delete context;
    delete batch;
}

TEST(MapConstructTest, MapNestedMapKeyValue) {
    RegisterOnce();
    int rowSize = 1;
    auto keyType = std::make_shared<DataType>(OMNI_INT);
    auto valueType = std::make_shared<DataType>(OMNI_INT);
    auto types = DataTypes({keyType, valueType});
    int32_t key1[] = {1, 2};
    int32_t value1[] = {10, 20};
    std::vector<int32_t> offset1 = {0, 2};
    auto batch1 = CreateMapVectorBatch(types, offset1, 1, 2, key1, value1);

    int32_t key2[] = {3, 4};
    int32_t value2[] = {30, 40};
    std::vector<int32_t> offset2 = {0, 2};
    auto* mapVec2 = new MapVector(1,
        std::shared_ptr<BaseVector>(CreateVector<int32_t>(2, key2)),
        std::shared_ptr<BaseVector>(CreateVector<int32_t>(2, value2)));
    for (size_t j = 0; j < offset2.size(); j++) {
        mapVec2->SetOffset(static_cast<int32_t>(j), offset2[j]);
    }
    batch1->Append(mapVec2);

    auto innerMapType = std::make_shared<MapType>(keyType, valueType);
    auto outerMapType = std::make_shared<MapType>(innerMapType, innerMapType);
    auto expr = new FuncExpr("map", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_MAP)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_MAP))
    }, outerMapType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(batch1, context);
    e.VisitExpr(*expr);
    BaseVector* result = e.GetResult();

    auto* mapVec = dynamic_cast<MapVector*>(result);
    ASSERT_NE(mapVec, nullptr);
    ASSERT_EQ(mapVec->GetSize(0), 1);

    delete expr;
    delete result;
    delete context;
    delete batch1;
}

TEST(MapConstructTest, MapStructKeyValue) {
    RegisterOnce();
    int rowSize = 1;

    std::vector<std::shared_ptr<DataType>> fieldTypes = {
        std::make_shared<DataType>(OMNI_INT), std::make_shared<DataType>(OMNI_DOUBLE)
    };
    auto rowType = std::make_shared<RowType>(fieldTypes, std::vector<std::string>{"f1", "f2"});

    auto* intVec1 = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, rowSize);
    static_cast<Vector<int32_t>*>(intVec1)->SetValue(0, 42);
    auto* dblVec1 = VectorHelper::CreateFlatVector(nullptr, OMNI_DOUBLE, rowSize);
    static_cast<Vector<double>*>(dblVec1)->SetValue(0, 3.14);
    std::vector<std::shared_ptr<BaseVector>> children1 = {
        std::shared_ptr<BaseVector>(intVec1), std::shared_ptr<BaseVector>(dblVec1)
    };
    auto* rowVec1 = new RowVector(rowSize, children1);

    auto* intVec2 = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, rowSize);
    static_cast<Vector<int32_t>*>(intVec2)->SetValue(0, 99);
    auto* dblVec2 = VectorHelper::CreateFlatVector(nullptr, OMNI_DOUBLE, rowSize);
    static_cast<Vector<double>*>(dblVec2)->SetValue(0, 2.71);
    std::vector<std::shared_ptr<BaseVector>> children2 = {
        std::shared_ptr<BaseVector>(intVec2), std::shared_ptr<BaseVector>(dblVec2)
    };
    auto* rowVec2 = new RowVector(rowSize, children2);

    auto batch = new VectorBatch(rowSize);
    batch->Append(rowVec1);
    batch->Append(rowVec2);

    auto mapType = std::make_shared<MapType>(rowType, rowType);
    auto expr = new FuncExpr("map", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ROW)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_ROW))
    }, mapType);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(batch, context);
    e.VisitExpr(*expr);
    BaseVector* result = e.GetResult();

    auto* mapVec = dynamic_cast<MapVector*>(result);
    ASSERT_NE(mapVec, nullptr);
    ASSERT_EQ(mapVec->GetSize(0), 1);

    delete expr;
    delete result;
    delete context;
    delete batch;
}
