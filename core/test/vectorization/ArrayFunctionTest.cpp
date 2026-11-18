/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: codegen test
 */

#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include <vector>

#include "../../src/vector/vector_helper.h"
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

    auto context = new ExecutionContext();
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    // Verify results: first element of each array should be [1, 4, 5]
    ASSERT_NE(result, nullptr);
    ASSERT_EQ(VectorHelper::GetValueFromVector<int32_t>(result, 0), 1);
    ASSERT_EQ(VectorHelper::GetValueFromVector<int32_t>(result, 1), 4);
    ASSERT_EQ(VectorHelper::GetValueFromVector<int32_t>(result, 2), 5);

    delete context;
    delete input;
    delete result;
}

namespace {

VectorBatch *BatchOf(BaseVector *arrayVector, BaseVector *indexVector)
{
    auto *batch = new VectorBatch(arrayVector->GetSize());
    batch->Append(arrayVector);
    batch->Append(indexVector);
    return batch;
}

BaseVector *EvalGet(VectorBatch *input, Expr *expr)
{
    auto *context = new ExecutionContext();
    context->SetResultRowSize(input->GetRowCount());
    ExprEval evaluator(input, context);
    evaluator.VisitExpr(*expr);
    auto *result = evaluator.GetResult();
    delete context;
    delete expr;
    return result;
}

ArrayVector *IntArray(const std::vector<int32_t> &values, const std::vector<int32_t> &offsets, int32_t nullRow = -1)
{
    auto *elements = new Vector<int32_t>(static_cast<int32_t>(values.size()));
    for (int32_t i = 0; i < static_cast<int32_t>(values.size()); ++i) {
        elements->SetValue(i, values[i]);
    }
    auto *arrayVector = new ArrayVector(static_cast<int64_t>(offsets.size() - 1), std::shared_ptr<BaseVector>(elements));
    for (int32_t i = 0; i < static_cast<int32_t>(offsets.size()); ++i) {
        arrayVector->SetOffset(i, offsets[i]);
    }
    if (nullRow >= 0) {
        arrayVector->SetNull(nullRow);
    }
    return arrayVector;
}

}  // namespace

TEST(ArrayFunctionTest, GetArrayItemNullsAndBounds)
{
    // rows: [10, null], [30], [40], [], null, [10]
    // index: 1, 5, -1, 0, 0, null
    auto *elements = new Vector<int32_t>(5);
    elements->SetValue(0, 10);
    elements->SetNull(1);
    elements->SetValue(2, 30);
    elements->SetValue(3, 40);
    elements->SetValue(4, 10);
    auto *arrays = new ArrayVector(6, std::shared_ptr<BaseVector>(elements));
    std::vector<int32_t> offsets = {0, 2, 3, 4, 4, 4, 5};
    for (int32_t i = 0; i < static_cast<int32_t>(offsets.size()); ++i) {
        arrays->SetOffset(i, offsets[i]);
    }
    arrays->SetNull(4);

    auto *index = new Vector<int32_t>(6);
    index->SetValue(0, 1);
    index->SetValue(1, 5);
    index->SetValue(2, -1);
    index->SetValue(3, 0);
    index->SetValue(4, 0);
    index->SetNull(5);

    auto *input = BatchOf(arrays, index);
    auto *expr = new FuncExpr("get_array_item", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, IntType())
    }, IntType());
    auto *result = EvalGet(input, expr);

    ASSERT_NE(result, nullptr);
    for (int32_t row = 0; row < 5; ++row) {
        EXPECT_TRUE(result->IsNull(row)) << "row " << row;
    }
    EXPECT_TRUE(result->IsNull(5));
    delete input;
    delete result;
}

TEST(ArrayFunctionTest, GetArrayItemEncodings)
{
    auto intType = IntType();
    DataType physicalInt(OMNI_INT);

    // const index is produced by LiteralExpr; dictionary index and dictionary elements are explicit.
    {
        int32_t indexValues[] = {5, 0};
        int32_t indexIds[] = {1, 0};
        auto *dictIndex = CreateDictionaryVector(physicalInt, 2, indexIds, 2, indexValues);
        auto *input = BatchOf(IntArray({7, 8, 9}, {0, 2, 3}), dictIndex);
        auto *expr = new FuncExpr("get_array_item", {
            new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
            new FieldExpr(1, intType)
        }, intType);
        auto *result = EvalGet(input, expr);
        EXPECT_FALSE(result->IsNull(0));
        EXPECT_EQ(VectorHelper::GetValueFromVector<int32_t>(result, 0), 7);
        EXPECT_TRUE(result->IsNull(1));
        delete input;
        delete result;
    }
    {
        int32_t elementValues[] = {10, 20, 30};
        int32_t elementIds[] = {2, 0, 1};
        auto *dictElements = CreateDictionaryVector(physicalInt, 3, elementIds, 3, elementValues);
        auto *arrays = new ArrayVector(1, std::shared_ptr<BaseVector>(dictElements));
        arrays->SetOffset(0, 0);
        arrays->SetOffset(1, 3);
        auto *index = new Vector<int32_t>(1);
        index->SetValue(0, 0);
        auto *input = BatchOf(arrays, index);
        auto *expr = new FuncExpr("get_array_item", {
            new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
            new FieldExpr(1, intType)
        }, intType);
        auto *result = EvalGet(input, expr);
        EXPECT_FALSE(result->IsNull(0));
        EXPECT_EQ(VectorHelper::GetValueFromVector<int32_t>(result, 0), 30);
        delete input;
        delete result;
    }
    {
        int64_t longIndex[] = {(static_cast<int64_t>(1) << 32) + 1, static_cast<int64_t>(1) << 32};
        auto *index = CreateVector<int64_t>(2, longIndex);
        auto *input = BatchOf(IntArray({10, 20, 30, 10, 20, 30}, {0, 3, 6}), index);
        auto *expr = new FuncExpr("get_array_item", {
            new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
            new FieldExpr(1, LongType())
        }, intType);
        auto *result = EvalGet(input, expr);
        EXPECT_EQ(VectorHelper::GetValueFromVector<int32_t>(result, 0), 20);
        EXPECT_EQ(VectorHelper::GetValueFromVector<int32_t>(result, 1), 10);
        delete input;
        delete result;
    }
    {
        auto *elements = new Vector<LargeStringContainer<std::string_view>>(2);
        elements->SetValue(0, std::string_view("ab"));
        elements->SetValue(1, std::string_view("cd"));
        auto *arrays = new ArrayVector(1, std::shared_ptr<BaseVector>(elements));
        arrays->SetOffset(0, 0);
        arrays->SetOffset(1, 2);
        auto *expr = new FuncExpr("get_array_item", {
            new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
            new LiteralExpr(1, intType)
        }, std::make_shared<DataType>(OMNI_VARCHAR));
        auto *input = new VectorBatch(1);
        input->Append(arrays);
        auto *result = EvalGet(input, expr);
        auto *strings = dynamic_cast<Vector<LargeStringContainer<std::string_view>> *>(result);
        ASSERT_NE(strings, nullptr);
        EXPECT_EQ(strings->GetValue(0), "cd");
        delete input;
        delete result;
    }
    {
        auto *constElements = new ConstVector<int32_t>(42, OMNI_INT, 2);
        auto *arrays = new ArrayVector(1, std::shared_ptr<BaseVector>(constElements));
        arrays->SetOffset(0, 0);
        arrays->SetOffset(1, 2);
        auto *expr = new FuncExpr("get_array_item", {
            new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
            new LiteralExpr(1, intType)
        }, intType);
        auto *input = new VectorBatch(1);
        input->Append(arrays);
        auto *result = EvalGet(input, expr);
        EXPECT_FALSE(result->IsNull(0));
        EXPECT_EQ(VectorHelper::GetValueFromVector<int32_t>(result, 0), 42);
        delete input;
        delete result;
    }
}

TEST(ArrayFunctionTest, GetArrayItemStructNullField)
{
    auto *x = new Vector<int32_t>(1);
    x->SetValue(0, 7);
    auto *y = new Vector<int32_t>(1);
    y->SetNull(0);
    std::vector<std::shared_ptr<BaseVector>> children;
    children.push_back(std::shared_ptr<BaseVector>(x));
    children.push_back(std::shared_ptr<BaseVector>(y));
    auto *elements = new RowVector(1, children);
    auto *arrays = new ArrayVector(1, std::shared_ptr<BaseVector>(elements));
    arrays->SetOffset(0, 0);
    arrays->SetOffset(1, 1);

    std::vector<std::shared_ptr<DataType>> fieldTypes = {IntType(), IntType()};
    auto rowType = std::make_shared<RowType>(fieldTypes, std::vector<std::string>{"x", "y"});
    auto *expr = new FuncExpr("get_array_item", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new LiteralExpr(0, IntType())
    }, rowType);
    auto *input = new VectorBatch(1);
    input->Append(arrays);
    auto *result = EvalGet(input, expr);
    auto *row = dynamic_cast<RowVector *>(result);
    ASSERT_NE(row, nullptr);
    EXPECT_FALSE(row->IsNull(0));
    EXPECT_FALSE(row->ChildAt(0)->IsNull(0));
    EXPECT_EQ(static_cast<Vector<int32_t> *>(row->ChildAt(0).get())->GetValue(0), 7);
    EXPECT_TRUE(row->ChildAt(1)->IsNull(0));
    delete input;
    delete result;
}

TEST(ArrayFunctionTest, GetArrayItemNestedArray)
{
    auto *innerElements = new Vector<int32_t>(3);
    innerElements->SetValue(0, 1);
    innerElements->SetValue(1, 2);
    innerElements->SetValue(2, 3);
    auto *innerArrays = new ArrayVector(2, std::shared_ptr<BaseVector>(innerElements));
    innerArrays->SetOffset(0, 0);
    innerArrays->SetOffset(1, 2);
    innerArrays->SetOffset(2, 3);
    auto *outer = new ArrayVector(2, std::shared_ptr<BaseVector>(innerArrays));
    outer->SetOffset(0, 0);
    outer->SetOffset(1, 1);
    outer->SetOffset(2, 2);

    auto elementType = std::make_shared<ArrayType>(IntType());
    auto *expr = new FuncExpr("get_array_item", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new LiteralExpr(0, IntType())
    }, elementType);
    auto *input = new VectorBatch(2);
    input->Append(outer);
    auto *result = EvalGet(input, expr);
    auto *arrays = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(arrays, nullptr);
    EXPECT_FALSE(arrays->IsNull(0));
    EXPECT_FALSE(arrays->IsNull(1));
    auto first = arrays->GetArrayAt(0, false);
    auto second = arrays->GetArrayAt(1, false);
    auto *firstInts = dynamic_cast<Vector<int32_t> *>(first.get());
    auto *secondInts = dynamic_cast<Vector<int32_t> *>(second.get());
    ASSERT_NE(firstInts, nullptr);
    ASSERT_NE(secondInts, nullptr);
    ASSERT_EQ(firstInts->GetSize(), 2);
    EXPECT_EQ(firstInts->GetValue(0), 1);
    EXPECT_EQ(firstInts->GetValue(1), 2);
    ASSERT_EQ(secondInts->GetSize(), 1);
    EXPECT_EQ(secondInts->GetValue(0), 3);
    delete input;
    delete result;
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

    auto context = new ExecutionContext();
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

    auto context = new ExecutionContext();
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