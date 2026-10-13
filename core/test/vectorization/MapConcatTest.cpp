/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: MapConcat function test
 */

#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "test/util/test_util.h"
#include "vectorization/ExprEval.h"
#include "expression/expressions.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::mem;
using namespace omniruntime::op;
using namespace omniruntime::expressions;
using namespace omniruntime::TestUtil;

TEST(MapConcatTest, BasicTwoMaps)
{
    int rowSize = 2;
    auto keyType = std::make_shared<DataType>(OMNI_INT);
    auto valueType = std::make_shared<DataType>(OMNI_INT);

    auto types1 = DataTypes({keyType, valueType});
    int32_t key1[] = {1, 2, 3};
    int32_t value1[] = {10, 20, 30};
    std::vector<int32_t> offset1 = {0, 2, 3};

    auto types2 = DataTypes({keyType, valueType});
    int32_t key2[] = {4, 5};
    int32_t value2[] = {40, 50};
    std::vector<int32_t> offset2 = {0, 1, 2};

    auto input1 = CreateMapVectorBatch(types1, offset1, rowSize, 3, key1, value1);
    auto input2 = CreateMapVectorBatch(types2, offset2, rowSize, 2, key2, value2);

    auto *combinedBatch = new VectorBatch(rowSize);
    combinedBatch->Append(input1->Get(0));
    combinedBatch->Append(input2->Get(0));

    auto field0 = new FieldExpr(0, std::make_shared<DataType>(OMNI_MAP));
    auto field1 = new FieldExpr(1, std::make_shared<DataType>(OMNI_MAP));
    auto expr = FuncExpr("map_concat", {field0, field1}, std::make_shared<DataType>(OMNI_MAP));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(combinedBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    MapVector *mapVec = dynamic_cast<MapVector *>(result);
    ASSERT_NE(mapVec, nullptr);

    ASSERT_EQ(mapVec->GetSize(0), 3);
    ASSERT_EQ(mapVec->GetSize(1), 2);

    auto keyElement = dynamic_cast<Vector<int32_t> *>(mapVec->GetKeyVector().get());
    auto valueElement = dynamic_cast<Vector<int32_t> *>(mapVec->GetValueVector().get());

    int32_t expectKey[] = {1, 2, 4, 3, 5};
    int32_t expectValue[] = {10, 20, 40, 30, 50};
    for (int i = 0; i < 5; ++i) {
        ASSERT_EQ(keyElement->GetValue(i), expectKey[i]);
        ASSERT_EQ(valueElement->GetValue(i), expectValue[i]);
    }

    input1->ClearVectors();
    input2->ClearVectors();
    delete input1;
    delete input2;
    delete combinedBatch;
    delete result;
    delete context;
}

TEST(MapConcatTest, SingleMap)
{
    int rowSize = 2;
    auto keyType = std::make_shared<DataType>(OMNI_INT);
    auto valueType = std::make_shared<DataType>(OMNI_INT);
    auto types = DataTypes({keyType, valueType});
    int32_t key[] = {1, 2, 3};
    int32_t value[] = {10, 20, 30};
    std::vector<int32_t> offset = {0, 2, 3};

    auto input = CreateMapVectorBatch(types, offset, rowSize, 3, key, value);

    auto field = new FieldExpr(0, std::make_shared<DataType>(OMNI_MAP));
    auto expr = FuncExpr("map_concat", {field}, std::make_shared<DataType>(OMNI_MAP));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    MapVector *mapVec = dynamic_cast<MapVector *>(result);
    ASSERT_NE(mapVec, nullptr);
    ASSERT_EQ(mapVec->GetSize(0), 2);
    ASSERT_EQ(mapVec->GetSize(1), 1);

    auto keyElement = dynamic_cast<Vector<int32_t> *>(mapVec->GetKeyVector().get());
    auto valueElement = dynamic_cast<Vector<int32_t> *>(mapVec->GetValueVector().get());

    ASSERT_EQ(keyElement->GetValue(0), 1);
    ASSERT_EQ(keyElement->GetValue(1), 2);
    ASSERT_EQ(keyElement->GetValue(2), 3);
    ASSERT_EQ(valueElement->GetValue(0), 10);
    ASSERT_EQ(valueElement->GetValue(1), 20);
    ASSERT_EQ(valueElement->GetValue(2), 30);

    delete result;
    delete context;
    delete input;
}

TEST(MapConcatTest, WithNullMap)
{
    int rowSize = 2;
    auto keyType = std::make_shared<DataType>(OMNI_INT);
    auto valueType = std::make_shared<DataType>(OMNI_INT);

    auto types1 = DataTypes({keyType, valueType});
    int32_t key1[] = {1, 2};
    int32_t value1[] = {10, 20};
    std::vector<int32_t> offset1 = {0, 1, 2};
    auto input1 = CreateMapVectorBatch(types1, offset1, rowSize, 2, key1, value1);

    auto types2 = DataTypes({keyType, valueType});
    int32_t key2[] = {3};
    int32_t value2[] = {30};
    std::vector<int32_t> offset2 = {0, 1, 1};
    auto input2 = CreateMapVectorBatch(types2, offset2, rowSize, 1, key2, value2);
    auto *map2 = dynamic_cast<MapVector *>(input2->Get(0));
    map2->SetNull(1);

    auto *combinedBatch = new VectorBatch(rowSize);
    combinedBatch->Append(input1->Get(0));
    combinedBatch->Append(input2->Get(0));

    auto field0 = new FieldExpr(0, std::make_shared<DataType>(OMNI_MAP));
    auto field1 = new FieldExpr(1, std::make_shared<DataType>(OMNI_MAP));
    auto expr = FuncExpr("map_concat", {field0, field1}, std::make_shared<DataType>(OMNI_MAP));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(combinedBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    MapVector *mapVec = dynamic_cast<MapVector *>(result);
    ASSERT_NE(mapVec, nullptr);

    ASSERT_EQ(mapVec->GetSize(0), 2);
    ASSERT_FALSE(mapVec->IsNull(0));

    ASSERT_TRUE(mapVec->IsNull(1));

    auto keyElement = dynamic_cast<Vector<int32_t> *>(mapVec->GetKeyVector().get());
    auto valueElement = dynamic_cast<Vector<int32_t> *>(mapVec->GetValueVector().get());
    ASSERT_EQ(keyElement->GetValue(0), 1);
    ASSERT_EQ(keyElement->GetValue(1), 3);
    ASSERT_EQ(valueElement->GetValue(0), 10);
    ASSERT_EQ(valueElement->GetValue(1), 30);

    input1->ClearVectors();
    input2->ClearVectors();
    delete input1;
    delete input2;
    delete combinedBatch;
    delete result;
    delete context;
}

TEST(MapConcatTest, EmptyMaps)
{
    int rowSize = 1;
    auto keyType = std::make_shared<DataType>(OMNI_INT);
    auto valueType = std::make_shared<DataType>(OMNI_INT);

    auto types1 = DataTypes({keyType, valueType});
    int32_t key1[] = {0};
    int32_t value1[] = {0};
    std::vector<int32_t> offset1 = {0, 0};
    auto input1 = CreateMapVectorBatch(types1, offset1, rowSize, 0, key1, value1);

    auto types2 = DataTypes({keyType, valueType});
    int32_t key2[] = {0};
    int32_t value2[] = {0};
    std::vector<int32_t> offset2 = {0, 0};
    auto input2 = CreateMapVectorBatch(types2, offset2, rowSize, 0, key2, value2);

    auto *combinedBatch = new VectorBatch(rowSize);
    combinedBatch->Append(input1->Get(0));
    combinedBatch->Append(input2->Get(0));

    auto field0 = new FieldExpr(0, std::make_shared<DataType>(OMNI_MAP));
    auto field1 = new FieldExpr(1, std::make_shared<DataType>(OMNI_MAP));
    auto expr = FuncExpr("map_concat", {field0, field1}, std::make_shared<DataType>(OMNI_MAP));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(combinedBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    MapVector *mapVec = dynamic_cast<MapVector *>(result);
    ASSERT_NE(mapVec, nullptr);
    ASSERT_EQ(mapVec->GetSize(0), 0);
    ASSERT_FALSE(mapVec->IsNull(0));

    input1->ClearVectors();
    input2->ClearVectors();
    delete input1;
    delete input2;
    delete combinedBatch;
    delete result;
    delete context;
}

TEST(MapConcatTest, StringKeyValueMaps)
{
    int rowSize = 1;

    std::string_view key1[] = {"a", "b"};
    int32_t value1[] = {1, 2};

    auto *map1 = new MapVector(rowSize);
    auto keyVec1 = new Vector<LargeStringContainer<std::string_view>>(2);
    keyVec1->SetValue(0, key1[0]);
    keyVec1->SetValue(1, key1[1]);
    auto valVec1 = new Vector<int32_t>(2);
    valVec1->SetValue(0, value1[0]);
    valVec1->SetValue(1, value1[1]);
    map1->SetKeyVector(std::shared_ptr<BaseVector>(keyVec1));
    map1->SetValueVector(std::shared_ptr<BaseVector>(valVec1));
    map1->SetOffset(1, 2);

    auto *map2 = new MapVector(rowSize);
    std::string_view key2[] = {"c"};
    int32_t value2[] = {3};
    auto keyVec2 = new Vector<LargeStringContainer<std::string_view>>(1);
    keyVec2->SetValue(0, key2[0]);
    auto valVec2 = new Vector<int32_t>(1);
    valVec2->SetValue(0, value2[0]);
    map2->SetKeyVector(std::shared_ptr<BaseVector>(keyVec2));
    map2->SetValueVector(std::shared_ptr<BaseVector>(valVec2));
    map2->SetOffset(1, 1);

    auto *combinedBatch = new VectorBatch(rowSize);
    combinedBatch->Append(map1);
    combinedBatch->Append(map2);

    auto field0 = new FieldExpr(0, std::make_shared<DataType>(OMNI_MAP));
    auto field1 = new FieldExpr(1, std::make_shared<DataType>(OMNI_MAP));
    auto expr = FuncExpr("map_concat", {field0, field1}, std::make_shared<DataType>(OMNI_MAP));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(combinedBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    MapVector *mapVec = dynamic_cast<MapVector *>(result);
    ASSERT_NE(mapVec, nullptr);
    ASSERT_EQ(mapVec->GetSize(0), 3);

    auto keyElement = dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(mapVec->GetKeyVector().get());
    auto valueElement = dynamic_cast<Vector<int32_t> *>(mapVec->GetValueVector().get());

    ASSERT_EQ(keyElement->GetValue(0), "a");
    ASSERT_EQ(keyElement->GetValue(1), "b");
    ASSERT_EQ(keyElement->GetValue(2), "c");
    ASSERT_EQ(valueElement->GetValue(0), 1);
    ASSERT_EQ(valueElement->GetValue(1), 2);
    ASSERT_EQ(valueElement->GetValue(2), 3);

    delete combinedBatch;
    delete result;
    delete context;
}
