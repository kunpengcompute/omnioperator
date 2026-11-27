/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: map_zip_with function unit tests
 */

#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/ExprEval.h"
#include "expression/expressions.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::mem;
using namespace omniruntime::op;
using namespace omniruntime::expressions;
using namespace omniruntime::TestUtil;

static VectorBatch *CreateTwoMapVectorBatch(
    int32_t rowSize,
    int32_t leftEntrySize, int32_t *leftKeys, int32_t *leftValues, std::vector<int32_t> &leftOffsets,
    int32_t rightEntrySize, int32_t *rightKeys, int32_t *rightValues, std::vector<int32_t> &rightOffsets)
{
    auto *vectorBatch = new VectorBatch(rowSize);

    auto *leftKeyVec = new Vector<int32_t>(leftEntrySize, OMNI_INT);
    auto *leftValueVec = new Vector<int32_t>(leftEntrySize, OMNI_INT);
    for (int32_t i = 0; i < leftEntrySize; ++i) {
        leftKeyVec->SetValue(i, leftKeys[i]);
        leftValueVec->SetValue(i, leftValues[i]);
    }
    auto *leftMap = new MapVector(rowSize,
        std::shared_ptr<BaseVector>(leftKeyVec),
        std::shared_ptr<BaseVector>(leftValueVec));
    for (size_t j = 0; j < leftOffsets.size(); ++j) {
        leftMap->SetOffset(j, leftOffsets[j]);
    }
    vectorBatch->Append(leftMap);

    auto *rightKeyVec = new Vector<int32_t>(rightEntrySize, OMNI_INT);
    auto *rightValueVec = new Vector<int32_t>(rightEntrySize, OMNI_INT);
    for (int32_t i = 0; i < rightEntrySize; ++i) {
        rightKeyVec->SetValue(i, rightKeys[i]);
        rightValueVec->SetValue(i, rightValues[i]);
    }
    auto *rightMap = new MapVector(rowSize,
        std::shared_ptr<BaseVector>(rightKeyVec),
        std::shared_ptr<BaseVector>(rightValueVec));
    for (size_t j = 0; j < rightOffsets.size(); ++j) {
        rightMap->SetOffset(j, rightOffsets[j]);
    }
    vectorBatch->Append(rightMap);

    return vectorBatch;
}

TEST(MapZipWithTest, SameKeys)
{
    int rowSize = 3;
    int32_t leftKeys[] = {1, 2, 3, 4, 5};
    int32_t leftValues[] = {10, 20, 30, 40, 50};
    std::vector<int32_t> leftOffsets = {0, 3, 4, 5};
    int32_t rightKeys[] = {1, 2, 3, 4, 5};
    int32_t rightValues[] = {100, 200, 300, 400, 500};
    std::vector<int32_t> rightOffsets = {0, 3, 4, 5};

    auto input = CreateTwoMapVectorBatch(rowSize,
        5, leftKeys, leftValues, leftOffsets,
        5, rightKeys, rightValues, rightOffsets);

    std::vector<DataTypePtr> paramTypes;
    paramTypes.push_back(IntType());
    paramTypes.push_back(IntType());
    paramTypes.push_back(IntType());
    ParamRefExpr *paramV1 = new ParamRefExpr("v1", IntType());
    ParamRefExpr *paramV2 = new ParamRefExpr("v2", IntType());
    BinaryExpr *addExpr = new BinaryExpr(omniruntime::expressions::Operator::ADD, paramV1, paramV2, IntType());
    std::unordered_map<std::string, int32_t> paramNameToIdxMap;
    paramNameToIdxMap.emplace("k", 0);
    paramNameToIdxMap.emplace("v1", 1);
    paramNameToIdxMap.emplace("v2", 2);

    auto expr = FuncExpr("map_zip_with", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_MAP)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_MAP)),
        new LambdaExpr(addExpr, paramTypes, paramNameToIdxMap, IntType())
    }, std::make_shared<DataType>(OMNI_MAP));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    MapVector *mapVec = dynamic_cast<MapVector *>(result);
    ASSERT_NE(mapVec, nullptr);
    ASSERT_EQ(mapVec->GetSize(), 3);

    auto keyElement = dynamic_cast<Vector<int32_t> *>(mapVec->GetKeyVector().get());
    auto valueElement = dynamic_cast<Vector<int32_t> *>(mapVec->GetValueVector().get());
    ASSERT_NE(keyElement, nullptr);
    ASSERT_NE(valueElement, nullptr);

    int32_t expectKeys[] = {1, 2, 3, 4, 5};
    int32_t expectValues[] = {110, 220, 330, 440, 550};
    for (int i = 0; i < 5; ++i) {
        ASSERT_EQ(keyElement->GetValue(i), expectKeys[i]);
        ASSERT_EQ(valueElement->GetValue(i), expectValues[i]);
    }

    ASSERT_EQ(mapVec->GetOffset(1) - mapVec->GetOffset(0), 3);
    ASSERT_EQ(mapVec->GetOffset(2) - mapVec->GetOffset(1), 1);
    ASSERT_EQ(mapVec->GetOffset(3) - mapVec->GetOffset(2), 1);

    delete context;
    delete input;
    delete result;
}

TEST(MapZipWithTest, PartialOverlap)
{
    int rowSize = 2;
    int32_t leftKeys[] = {1, 2, 4, 5};
    int32_t leftValues[] = {10, 20, 40, 50};
    std::vector<int32_t> leftOffsets = {0, 2, 4};
    int32_t rightKeys[] = {2, 3, 5, 6};
    int32_t rightValues[] = {200, 300, 500, 600};
    std::vector<int32_t> rightOffsets = {0, 2, 4};

    auto input = CreateTwoMapVectorBatch(rowSize,
        4, leftKeys, leftValues, leftOffsets,
        4, rightKeys, rightValues, rightOffsets);

    std::vector<DataTypePtr> paramTypes;
    paramTypes.push_back(IntType());
    paramTypes.push_back(IntType());
    paramTypes.push_back(IntType());
    ParamRefExpr *paramV1 = new ParamRefExpr("v1", IntType());
    ParamRefExpr *paramV2 = new ParamRefExpr("v2", IntType());
    BinaryExpr *addExpr = new BinaryExpr(omniruntime::expressions::Operator::ADD, paramV1, paramV2, IntType());
    std::unordered_map<std::string, int32_t> paramNameToIdxMap;
    paramNameToIdxMap.emplace("k", 0);
    paramNameToIdxMap.emplace("v1", 1);
    paramNameToIdxMap.emplace("v2", 2);

    auto expr = FuncExpr("map_zip_with", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_MAP)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_MAP)),
        new LambdaExpr(addExpr, paramTypes, paramNameToIdxMap, IntType())
    }, std::make_shared<DataType>(OMNI_MAP));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    MapVector *mapVec = dynamic_cast<MapVector *>(result);
    ASSERT_NE(mapVec, nullptr);
    ASSERT_EQ(mapVec->GetSize(), 2);

    ASSERT_EQ(mapVec->GetOffset(1) - mapVec->GetOffset(0), 3);
    ASSERT_EQ(mapVec->GetOffset(2) - mapVec->GetOffset(1), 3);

    auto keyElement = dynamic_cast<Vector<int32_t> *>(mapVec->GetKeyVector().get());
    auto valueElement = dynamic_cast<Vector<int32_t> *>(mapVec->GetValueVector().get());
    ASSERT_NE(keyElement, nullptr);
    ASSERT_NE(valueElement, nullptr);

    ASSERT_EQ(keyElement->GetValue(0), 1);
    ASSERT_TRUE(valueElement->IsNull(0));

    ASSERT_EQ(keyElement->GetValue(1), 2);
    ASSERT_EQ(valueElement->GetValue(1), 220);

    ASSERT_EQ(keyElement->GetValue(2), 3);
    ASSERT_TRUE(valueElement->IsNull(2));

    ASSERT_EQ(keyElement->GetValue(3), 4);
    ASSERT_TRUE(valueElement->IsNull(3));

    ASSERT_EQ(keyElement->GetValue(4), 5);
    ASSERT_EQ(valueElement->GetValue(4), 550);

    ASSERT_EQ(keyElement->GetValue(5), 6);
    ASSERT_TRUE(valueElement->IsNull(5));

    delete context;
    delete input;
    delete result;
}

TEST(MapZipWithTest, NullMapInput)
{
    int rowSize = 3;
    int32_t leftKeys[] = {1, 2, 3, 5};
    int32_t leftValues[] = {10, 20, 30, 50};
    std::vector<int32_t> leftOffsets = {0, 3, 3, 4};
    int32_t rightKeys[] = {1, 2, 3, 4, 5};
    int32_t rightValues[] = {100, 200, 300, 400, 500};
    std::vector<int32_t> rightOffsets = {0, 3, 4, 5};

    auto input = CreateTwoMapVectorBatch(rowSize,
        4, leftKeys, leftValues, leftOffsets,
        5, rightKeys, rightValues, rightOffsets);

    auto *leftMapVec = dynamic_cast<MapVector *>(input->Get(0));
    leftMapVec->SetNull(1);

    std::vector<DataTypePtr> paramTypes;
    paramTypes.push_back(IntType());
    paramTypes.push_back(IntType());
    paramTypes.push_back(IntType());
    ParamRefExpr *paramV1 = new ParamRefExpr("v1", IntType());
    ParamRefExpr *paramV2 = new ParamRefExpr("v2", IntType());
    BinaryExpr *addExpr = new BinaryExpr(omniruntime::expressions::Operator::ADD, paramV1, paramV2, IntType());
    std::unordered_map<std::string, int32_t> paramNameToIdxMap;
    paramNameToIdxMap.emplace("k", 0);
    paramNameToIdxMap.emplace("v1", 1);
    paramNameToIdxMap.emplace("v2", 2);

    auto expr = FuncExpr("map_zip_with", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_MAP)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_MAP)),
        new LambdaExpr(addExpr, paramTypes, paramNameToIdxMap, IntType())
    }, std::make_shared<DataType>(OMNI_MAP));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    MapVector *mapVec = dynamic_cast<MapVector *>(result);
    ASSERT_NE(mapVec, nullptr);

    ASSERT_FALSE(mapVec->IsNull(0));
    ASSERT_EQ(mapVec->GetOffset(1) - mapVec->GetOffset(0), 3);

    ASSERT_TRUE(mapVec->IsNull(1));

    ASSERT_FALSE(mapVec->IsNull(2));
    ASSERT_EQ(mapVec->GetOffset(3) - mapVec->GetOffset(2), 1);

    auto keyElement = dynamic_cast<Vector<int32_t> *>(mapVec->GetKeyVector().get());
    auto valueElement = dynamic_cast<Vector<int32_t> *>(mapVec->GetValueVector().get());
    ASSERT_NE(keyElement, nullptr);
    ASSERT_NE(valueElement, nullptr);

    ASSERT_EQ(keyElement->GetValue(0), 1);
    ASSERT_EQ(valueElement->GetValue(0), 110);
    ASSERT_EQ(keyElement->GetValue(1), 2);
    ASSERT_EQ(valueElement->GetValue(1), 220);
    ASSERT_EQ(keyElement->GetValue(2), 3);
    ASSERT_EQ(valueElement->GetValue(2), 330);
    ASSERT_EQ(keyElement->GetValue(3), 5);
    ASSERT_EQ(valueElement->GetValue(3), 550);

    delete context;
    delete input;
    delete result;
}

TEST(MapZipWithTest, EmptyMaps)
{
    int rowSize = 2;
    int32_t leftKeys[] = {1, 2};
    int32_t leftValues[] = {10, 20};
    std::vector<int32_t> leftOffsets = {0, 0, 2};
    int32_t rightKeys[] = {1};
    int32_t rightValues[] = {100};
    std::vector<int32_t> rightOffsets = {0, 0, 1};

    auto input = CreateTwoMapVectorBatch(rowSize,
        2, leftKeys, leftValues, leftOffsets,
        1, rightKeys, rightValues, rightOffsets);

    std::vector<DataTypePtr> paramTypes;
    paramTypes.push_back(IntType());
    paramTypes.push_back(IntType());
    paramTypes.push_back(IntType());
    ParamRefExpr *paramV1 = new ParamRefExpr("v1", IntType());
    ParamRefExpr *paramV2 = new ParamRefExpr("v2", IntType());
    BinaryExpr *addExpr = new BinaryExpr(omniruntime::expressions::Operator::ADD, paramV1, paramV2, IntType());
    std::unordered_map<std::string, int32_t> paramNameToIdxMap;
    paramNameToIdxMap.emplace("k", 0);
    paramNameToIdxMap.emplace("v1", 1);
    paramNameToIdxMap.emplace("v2", 2);

    auto expr = FuncExpr("map_zip_with", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_MAP)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_MAP)),
        new LambdaExpr(addExpr, paramTypes, paramNameToIdxMap, IntType())
    }, std::make_shared<DataType>(OMNI_MAP));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    MapVector *mapVec = dynamic_cast<MapVector *>(result);
    ASSERT_NE(mapVec, nullptr);

    ASSERT_EQ(mapVec->GetOffset(1) - mapVec->GetOffset(0), 0);
    ASSERT_EQ(mapVec->GetOffset(2) - mapVec->GetOffset(1), 2);

    auto keyElement = dynamic_cast<Vector<int32_t> *>(mapVec->GetKeyVector().get());
    auto valueElement = dynamic_cast<Vector<int32_t> *>(mapVec->GetValueVector().get());
    ASSERT_NE(keyElement, nullptr);
    ASSERT_NE(valueElement, nullptr);

    ASSERT_EQ(keyElement->GetValue(0), 1);
    ASSERT_EQ(valueElement->GetValue(0), 110);

    ASSERT_EQ(keyElement->GetValue(1), 2);
    ASSERT_TRUE(valueElement->IsNull(1));

    delete context;
    delete input;
    delete result;
}

TEST(MapZipWithTest, AllNullMaps)
{
    int rowSize = 2;
    int32_t leftKeys[] = {1};
    int32_t leftValues[] = {10};
    std::vector<int32_t> leftOffsets = {0, 0, 1};
    int32_t rightKeys[] = {1};
    int32_t rightValues[] = {100};
    std::vector<int32_t> rightOffsets = {0, 0, 1};

    auto input = CreateTwoMapVectorBatch(rowSize,
        1, leftKeys, leftValues, leftOffsets,
        1, rightKeys, rightValues, rightOffsets);

    auto *leftMapVec = dynamic_cast<MapVector *>(input->Get(0));
    auto *rightMapVec = dynamic_cast<MapVector *>(input->Get(1));
    leftMapVec->SetNull(0);
    leftMapVec->SetNull(1);
    rightMapVec->SetNull(0);
    rightMapVec->SetNull(1);

    std::vector<DataTypePtr> paramTypes;
    paramTypes.push_back(IntType());
    paramTypes.push_back(IntType());
    paramTypes.push_back(IntType());
    ParamRefExpr *paramV1 = new ParamRefExpr("v1", IntType());
    ParamRefExpr *paramV2 = new ParamRefExpr("v2", IntType());
    BinaryExpr *addExpr = new BinaryExpr(omniruntime::expressions::Operator::ADD, paramV1, paramV2, IntType());
    std::unordered_map<std::string, int32_t> paramNameToIdxMap;
    paramNameToIdxMap.emplace("k", 0);
    paramNameToIdxMap.emplace("v1", 1);
    paramNameToIdxMap.emplace("v2", 2);

    auto expr = FuncExpr("map_zip_with", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_MAP)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_MAP)),
        new LambdaExpr(addExpr, paramTypes, paramNameToIdxMap, IntType())
    }, std::make_shared<DataType>(OMNI_MAP));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    MapVector *mapVec = dynamic_cast<MapVector *>(result);
    ASSERT_NE(mapVec, nullptr);

    ASSERT_TRUE(mapVec->IsNull(0));
    ASSERT_TRUE(mapVec->IsNull(1));

    delete context;
    delete input;
    delete result;
}

TEST(MapZipWithTest, DisjointKeys)
{
    int rowSize = 1;
    int32_t leftKeys[] = {1, 2};
    int32_t leftValues[] = {10, 20};
    std::vector<int32_t> leftOffsets = {0, 2};
    int32_t rightKeys[] = {3, 4};
    int32_t rightValues[] = {300, 400};
    std::vector<int32_t> rightOffsets = {0, 2};

    auto input = CreateTwoMapVectorBatch(rowSize,
        2, leftKeys, leftValues, leftOffsets,
        2, rightKeys, rightValues, rightOffsets);

    std::vector<DataTypePtr> paramTypes;
    paramTypes.push_back(IntType());
    paramTypes.push_back(IntType());
    paramTypes.push_back(IntType());
    ParamRefExpr *paramV1 = new ParamRefExpr("v1", IntType());
    ParamRefExpr *paramV2 = new ParamRefExpr("v2", IntType());
    BinaryExpr *addExpr = new BinaryExpr(omniruntime::expressions::Operator::ADD, paramV1, paramV2, IntType());
    std::unordered_map<std::string, int32_t> paramNameToIdxMap;
    paramNameToIdxMap.emplace("k", 0);
    paramNameToIdxMap.emplace("v1", 1);
    paramNameToIdxMap.emplace("v2", 2);

    auto expr = FuncExpr("map_zip_with", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_MAP)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_MAP)),
        new LambdaExpr(addExpr, paramTypes, paramNameToIdxMap, IntType())
    }, std::make_shared<DataType>(OMNI_MAP));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    MapVector *mapVec = dynamic_cast<MapVector *>(result);
    ASSERT_NE(mapVec, nullptr);
    ASSERT_EQ(mapVec->GetSize(), 1);
    ASSERT_EQ(mapVec->GetOffset(1) - mapVec->GetOffset(0), 4);

    auto keyElement = dynamic_cast<Vector<int32_t> *>(mapVec->GetKeyVector().get());
    auto valueElement = dynamic_cast<Vector<int32_t> *>(mapVec->GetValueVector().get());
    ASSERT_NE(keyElement, nullptr);
    ASSERT_NE(valueElement, nullptr);

    ASSERT_EQ(keyElement->GetValue(0), 1);
    ASSERT_TRUE(valueElement->IsNull(0));

    ASSERT_EQ(keyElement->GetValue(1), 2);
    ASSERT_TRUE(valueElement->IsNull(1));

    ASSERT_EQ(keyElement->GetValue(2), 3);
    ASSERT_TRUE(valueElement->IsNull(2));

    ASSERT_EQ(keyElement->GetValue(3), 4);
    ASSERT_TRUE(valueElement->IsNull(3));

    delete context;
    delete input;
    delete result;
}
