/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: codegen test
 */

#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include <vector>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/ExprEval.h"
#include "expression/expressions.h"
#include "type/decimal_operations.h"
#include "vectorization/registration/SimpleFunctionRegistry.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::mem;
using namespace omniruntime::op;
using namespace omniruntime::expressions;
using namespace omniruntime::TestUtil;

TEST(ArrayFunctionTest, GetArrayItemTest)
{
    int rowSize = 3;
    auto type = std::make_shared<DataType>(OMNI_INT);
    auto types = DataTypes({type});
    std::vector vecOfTypes = {IntType(), IntType()};
    DataTypes inputTypes(vecOfTypes);
    int32_t col[] = {1, 2, 3, 4, 5};
    std::vector<int32_t> offset = {0,3,4,5};
    auto input = CreateArrayVectorBatch(types, offset, rowSize, 5, col);

    auto expr = FuncExpr("get_array_item", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new LiteralExpr(0, type)
    }, type);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();
    VectorBatch vectorBatch(rowSize);
    vectorBatch.Append(result);
    VectorHelper::PrintVecBatch(&vectorBatch);

    delete context;
    delete input;
}

TEST(ArrayFunctionTest, TransformTest)
{
    int rowSize = 3;
    auto type = std::make_shared<DataType>(OMNI_INT);
    auto types = DataTypes({type});
    std::vector vecOfTypes = {IntType(), IntType()};
    DataTypes inputTypes(vecOfTypes);
    int32_t col[] = {1, 2, 3, 4, 5};
    std::vector<int32_t> offset = {0,3,4,5};
    auto input = CreateArrayVectorBatch(types, offset, rowSize, 5, col);
    std::vector<DataTypePtr> paramTypes;
    paramTypes.push_back(IntType());
    ParamRefExpr *addLeft = new ParamRefExpr("x", IntType());
    LiteralExpr *addRight = new LiteralExpr(5, IntType());
    BinaryExpr *addExpr = new BinaryExpr(omniruntime::expressions::Operator::ADD, addLeft, addRight, IntType());
    std::unordered_map<std::string, int32_t> paramNameToIdxMap;
    paramNameToIdxMap.emplace("x",0);
    auto expr = FuncExpr("transform", {
            new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
            new LambdaExpr(addExpr, paramTypes, paramNameToIdxMap, IntType())
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    ArrayVector *arrVec = dynamic_cast<ArrayVector*>(result);
    int32_t expect[] = {6, 7, 8, 9 , 10};
    auto element = dynamic_cast<Vector<int32_t>*>(arrVec->GetElementVector().get());
    for (int i = 0; i < 5; ++i) {
        ASSERT_EQ(element->GetValue(i),expect[i]);
    }
    delete context;
    delete input;
    delete result;
}

TEST(ArrayFunctionTest, MapFromArraysTest)
{
    int rowSize = 3;
    auto type = std::make_shared<DataType>(OMNI_INT);
    auto types = DataTypes({type, type});
    int32_t keyCol[] = {1, 2, 3, 4, 5};
    int32_t valueCol[] = {10, 20, 30, 40, 50};
    std::vector<int32_t> offset = {0,3,4,5};
    auto input = CreateArrayVectorBatch(types, offset, rowSize, 5, keyCol, valueCol);
    auto expr = FuncExpr("map_from_arrays", {
            new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
            new FieldExpr(1, std::make_shared<DataType>(OMNI_ARRAY))
    }, std::make_shared<DataType>(OMNI_MAP));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    MapVector *mapVec = dynamic_cast<MapVector*>(result);
    int32_t expectKeys[] = {1, 2, 3, 4, 5};
    int32_t expectValues[] = {10, 20, 30, 40, 50};
    auto Keys = dynamic_cast<Vector<int32_t>*>(mapVec->GetKeyVector().get());
    auto Values = dynamic_cast<Vector<int32_t>*>(mapVec->GetValueVector().get());
    for (int i = 0; i < 5; ++i) {
        ASSERT_EQ(Keys->GetValue(i),expectKeys[i]);
        ASSERT_EQ(Values->GetValue(i),expectValues[i]);
    }
    delete context;
    delete input;
    delete result;
}