/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 */

#include <gtest/gtest.h>
#include <vector>
#include <string>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/ExprEval.h"
#include "expression/expressions.h"
#include "vector/array_vector.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::mem;
using namespace omniruntime::op;
using namespace omniruntime::expressions;
using namespace omniruntime::type;
using namespace omniruntime::TestUtil;

static VectorBatch *CreateIntArrayVectorBatchForFilter(
    int32_t rowSize,
    int32_t elemSize, int32_t *elems, std::vector<int32_t> &offsets)
{
    auto *vectorBatch = new VectorBatch(rowSize);

    auto *elemVec = new Vector<int32_t>(elemSize, OMNI_INT);
    for (int32_t i = 0; i < elemSize; ++i) {
        elemVec->SetValue(i, elems[i]);
    }
    auto *arrVec = new ArrayVector(rowSize, std::shared_ptr<BaseVector>(elemVec));
    for (size_t j = 0; j < offsets.size(); ++j) {
        arrVec->SetOffset(j, offsets[j]);
    }
    vectorBatch->Append(arrVec);

    return vectorBatch;
}

TEST(ArrayFilterTest, BasicFilterGreaterThan)
{
    int rowSize = 3;
    int32_t elems[] = {1, 5, 3, 8, 2, 7};
    std::vector<int32_t> offsets = {0, 3, 5, 6};

    auto *input = CreateIntArrayVectorBatchForFilter(rowSize, 6, elems, offsets);

    std::vector<DataTypePtr> paramTypes;
    paramTypes.push_back(IntType());
    auto *paramX = new ParamRefExpr("x", IntType());
    auto *lit3 = new LiteralExpr(3, IntType());
    auto *gtExpr = new BinaryExpr(expressions::Operator::GT, paramX, lit3, BooleanType());
    std::unordered_map<std::string, int32_t> paramNameToIdxMap;
    paramNameToIdxMap.emplace("x", 0);

    auto expr = FuncExpr("filter", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new LambdaExpr(gtExpr, paramTypes, paramNameToIdxMap, BooleanType())
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto *context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto *result = e.GetResult();

    auto *resultArr = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArr, nullptr);
    ASSERT_EQ(resultArr->GetSize(), 3);

    ASSERT_EQ(resultArr->GetOffset(1) - resultArr->GetOffset(0), 1);
    ASSERT_EQ(resultArr->GetOffset(2) - resultArr->GetOffset(1), 1);
    ASSERT_EQ(resultArr->GetOffset(3) - resultArr->GetOffset(2), 1);

    auto *elemResult = dynamic_cast<Vector<int32_t> *>(resultArr->GetElementVector().get());
    ASSERT_NE(elemResult, nullptr);
    ASSERT_EQ(elemResult->GetSize(), 3);
    ASSERT_EQ(elemResult->GetValue(0), 5);
    ASSERT_EQ(elemResult->GetValue(1), 8);
    ASSERT_EQ(elemResult->GetValue(2), 7);

    delete context;
    delete input;
    delete result;
}

TEST(ArrayFilterTest, FilterNonePass)
{
    int rowSize = 2;
    int32_t elems[] = {1, 2, 3, 4};
    std::vector<int32_t> offsets = {0, 2, 4};

    auto *input = CreateIntArrayVectorBatchForFilter(rowSize, 4, elems, offsets);

    std::vector<DataTypePtr> paramTypes;
    paramTypes.push_back(IntType());
    auto *paramX = new ParamRefExpr("x", IntType());
    auto *lit100 = new LiteralExpr(100, IntType());
    auto *gtExpr = new BinaryExpr(expressions::Operator::GT, paramX, lit100, BooleanType());
    std::unordered_map<std::string, int32_t> paramNameToIdxMap;
    paramNameToIdxMap.emplace("x", 0);

    auto expr = FuncExpr("filter", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new LambdaExpr(gtExpr, paramTypes, paramNameToIdxMap, BooleanType())
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto *context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto *result = e.GetResult();

    auto *resultArr = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArr, nullptr);
    ASSERT_EQ(resultArr->GetSize(), 2);

    for (int32_t i = 0; i < 2; ++i) {
        ASSERT_EQ(resultArr->GetOffset(i + 1) - resultArr->GetOffset(i), 0);
    }

    delete context;
    delete input;
    delete result;
}

TEST(ArrayFilterTest, FilterAllPass)
{
    int rowSize = 2;
    int32_t elems[] = {10, 20, 30, 40};
    std::vector<int32_t> offsets = {0, 2, 4};

    auto *input = CreateIntArrayVectorBatchForFilter(rowSize, 4, elems, offsets);

    std::vector<DataTypePtr> paramTypes;
    paramTypes.push_back(IntType());
    auto *paramX = new ParamRefExpr("x", IntType());
    auto *lit0 = new LiteralExpr(0, IntType());
    auto *gtExpr = new BinaryExpr(expressions::Operator::GT, paramX, lit0, BooleanType());
    std::unordered_map<std::string, int32_t> paramNameToIdxMap;
    paramNameToIdxMap.emplace("x", 0);

    auto expr = FuncExpr("filter", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new LambdaExpr(gtExpr, paramTypes, paramNameToIdxMap, BooleanType())
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto *context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto *result = e.GetResult();

    auto *resultArr = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArr, nullptr);
    ASSERT_EQ(resultArr->GetSize(), 2);

    ASSERT_EQ(resultArr->GetOffset(1) - resultArr->GetOffset(0), 2);
    ASSERT_EQ(resultArr->GetOffset(2) - resultArr->GetOffset(1), 2);

    auto *elemResult = dynamic_cast<Vector<int32_t> *>(resultArr->GetElementVector().get());
    ASSERT_NE(elemResult, nullptr);
    ASSERT_EQ(elemResult->GetSize(), 4);
    ASSERT_EQ(elemResult->GetValue(0), 10);
    ASSERT_EQ(elemResult->GetValue(1), 20);
    ASSERT_EQ(elemResult->GetValue(2), 30);
    ASSERT_EQ(elemResult->GetValue(3), 40);

    delete context;
    delete input;
    delete result;
}

TEST(ArrayFilterTest, EmptyArrayReturnsEmpty)
{
    int rowSize = 2;
    int32_t elems[] = {10, 20};
    std::vector<int32_t> offsets = {0, 0, 2};

    auto *input = CreateIntArrayVectorBatchForFilter(rowSize, 2, elems, offsets);

    std::vector<DataTypePtr> paramTypes;
    paramTypes.push_back(IntType());
    auto *paramX = new ParamRefExpr("x", IntType());
    auto *lit100 = new LiteralExpr(100, IntType());
    auto *gtExpr = new BinaryExpr(expressions::Operator::GT, paramX, lit100, BooleanType());
    std::unordered_map<std::string, int32_t> paramNameToIdxMap;
    paramNameToIdxMap.emplace("x", 0);

    auto expr = FuncExpr("filter", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new LambdaExpr(gtExpr, paramTypes, paramNameToIdxMap, BooleanType())
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto *context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto *result = e.GetResult();

    auto *resultArr = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArr, nullptr);
    ASSERT_EQ(resultArr->GetSize(), 2);

    ASSERT_EQ(resultArr->GetOffset(1) - resultArr->GetOffset(0), 0);

    delete context;
    delete input;
    delete result;
}

TEST(ArrayFilterTest, NullArrayRowReturnsNull)
{
    int rowSize = 3;
    int32_t elems[] = {5, 10, 15};
    std::vector<int32_t> offsets = {0, 2, 2, 3};

    auto *input = CreateIntArrayVectorBatchForFilter(rowSize, 3, elems, offsets);
    auto *arrVec = dynamic_cast<ArrayVector *>(input->Get(0));
    arrVec->SetNull(1);

    std::vector<DataTypePtr> paramTypes;
    paramTypes.push_back(IntType());
    auto *paramX = new ParamRefExpr("x", IntType());
    auto *lit3 = new LiteralExpr(3, IntType());
    auto *gtExpr = new BinaryExpr(expressions::Operator::GT, paramX, lit3, BooleanType());
    std::unordered_map<std::string, int32_t> paramNameToIdxMap;
    paramNameToIdxMap.emplace("x", 0);

    auto expr = FuncExpr("filter", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new LambdaExpr(gtExpr, paramTypes, paramNameToIdxMap, BooleanType())
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto *context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto *result = e.GetResult();

    auto *resultArr = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArr, nullptr);
    ASSERT_EQ(resultArr->GetSize(), 3);

    ASSERT_FALSE(resultArr->IsNull(0));
    ASSERT_EQ(resultArr->GetOffset(1) - resultArr->GetOffset(0), 2);

    ASSERT_TRUE(resultArr->IsNull(1));

    ASSERT_FALSE(resultArr->IsNull(2));
    ASSERT_EQ(resultArr->GetOffset(3) - resultArr->GetOffset(2), 1);

    auto *elemResult = dynamic_cast<Vector<int32_t> *>(resultArr->GetElementVector().get());
    ASSERT_NE(elemResult, nullptr);
    ASSERT_EQ(elemResult->GetValue(0), 5);
    ASSERT_EQ(elemResult->GetValue(1), 10);
    ASSERT_EQ(elemResult->GetValue(2), 15);

    delete context;
    delete input;
    delete result;
}

TEST(ArrayFilterTest, AllNullArrayRows)
{
    int rowSize = 2;
    int32_t elems[] = {1};
    std::vector<int32_t> offsets = {0, 0, 1};

    auto *input = CreateIntArrayVectorBatchForFilter(rowSize, 1, elems, offsets);
    auto *arrVec = dynamic_cast<ArrayVector *>(input->Get(0));
    arrVec->SetNull(0);
    arrVec->SetNull(1);

    std::vector<DataTypePtr> paramTypes;
    paramTypes.push_back(IntType());
    auto *paramX = new ParamRefExpr("x", IntType());
    auto *lit0 = new LiteralExpr(0, IntType());
    auto *gtExpr = new BinaryExpr(expressions::Operator::GT, paramX, lit0, BooleanType());
    std::unordered_map<std::string, int32_t> paramNameToIdxMap;
    paramNameToIdxMap.emplace("x", 0);

    auto expr = FuncExpr("filter", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new LambdaExpr(gtExpr, paramTypes, paramNameToIdxMap, BooleanType())
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto *context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto *result = e.GetResult();

    auto *resultArr = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArr, nullptr);
    ASSERT_EQ(resultArr->GetSize(), 2);

    ASSERT_TRUE(resultArr->IsNull(0));
    ASSERT_TRUE(resultArr->IsNull(1));

    delete context;
    delete input;
    delete result;
}

TEST(ArrayFilterTest, SingleElementArrayPass)
{
    int rowSize = 2;
    int32_t elems[] = {10, 1};
    std::vector<int32_t> offsets = {0, 1, 2};

    auto *input = CreateIntArrayVectorBatchForFilter(rowSize, 2, elems, offsets);

    std::vector<DataTypePtr> paramTypes;
    paramTypes.push_back(IntType());
    auto *paramX = new ParamRefExpr("x", IntType());
    auto *lit5 = new LiteralExpr(5, IntType());
    auto *gtExpr = new BinaryExpr(expressions::Operator::GT, paramX, lit5, BooleanType());
    std::unordered_map<std::string, int32_t> paramNameToIdxMap;
    paramNameToIdxMap.emplace("x", 0);

    auto expr = FuncExpr("filter", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new LambdaExpr(gtExpr, paramTypes, paramNameToIdxMap, BooleanType())
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto *context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto *result = e.GetResult();

    auto *resultArr = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArr, nullptr);
    ASSERT_EQ(resultArr->GetSize(), 2);

    ASSERT_EQ(resultArr->GetOffset(1) - resultArr->GetOffset(0), 1);
    ASSERT_EQ(resultArr->GetOffset(2) - resultArr->GetOffset(1), 0);

    auto *elemResult = dynamic_cast<Vector<int32_t> *>(resultArr->GetElementVector().get());
    ASSERT_NE(elemResult, nullptr);
    ASSERT_EQ(elemResult->GetSize(), 1);
    ASSERT_EQ(elemResult->GetValue(0), 10);

    delete context;
    delete input;
    delete result;
}

TEST(ArrayFilterTest, MixedFilterResults)
{
    int rowSize = 4;
    int32_t elems[] = {1, 10, 2, 20, 3, 30, 4};
    std::vector<int32_t> offsets = {0, 2, 4, 6, 7};

    auto *input = CreateIntArrayVectorBatchForFilter(rowSize, 7, elems, offsets);

    std::vector<DataTypePtr> paramTypes;
    paramTypes.push_back(IntType());
    auto *paramX = new ParamRefExpr("x", IntType());
    auto *lit5 = new LiteralExpr(5, IntType());
    auto *gteExpr = new BinaryExpr(expressions::Operator::GTE, paramX, lit5, BooleanType());
    std::unordered_map<std::string, int32_t> paramNameToIdxMap;
    paramNameToIdxMap.emplace("x", 0);

    auto expr = FuncExpr("filter", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new LambdaExpr(gteExpr, paramTypes, paramNameToIdxMap, BooleanType())
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto *context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto *result = e.GetResult();

    auto *resultArr = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArr, nullptr);
    ASSERT_EQ(resultArr->GetSize(), 4);

    ASSERT_EQ(resultArr->GetOffset(1) - resultArr->GetOffset(0), 1);
    ASSERT_EQ(resultArr->GetOffset(2) - resultArr->GetOffset(1), 1);
    ASSERT_EQ(resultArr->GetOffset(3) - resultArr->GetOffset(2), 1);
    ASSERT_EQ(resultArr->GetOffset(4) - resultArr->GetOffset(3), 0);

    auto *elemResult = dynamic_cast<Vector<int32_t> *>(resultArr->GetElementVector().get());
    ASSERT_NE(elemResult, nullptr);
    ASSERT_EQ(elemResult->GetSize(), 3);
    ASSERT_EQ(elemResult->GetValue(0), 10);
    ASSERT_EQ(elemResult->GetValue(1), 20);
    ASSERT_EQ(elemResult->GetValue(2), 30);

    delete context;
    delete input;
    delete result;
}

TEST(ArrayFilterTest, NullElementsInArray)
{
    int rowSize = 2;
    int32_t elems[] = {10, 3, 20, 1};
    std::vector<int32_t> offsets = {0, 2, 4};

    auto *input = CreateIntArrayVectorBatchForFilter(rowSize, 4, elems, offsets);
    auto *arrVec = dynamic_cast<ArrayVector *>(input->Get(0));
    auto *elemVec = dynamic_cast<Vector<int32_t> *>(arrVec->GetElementVector().get());
    elemVec->SetNull(1);

    std::vector<DataTypePtr> paramTypes;
    paramTypes.push_back(IntType());
    auto *paramX = new ParamRefExpr("x", IntType());
    auto *lit5 = new LiteralExpr(5, IntType());
    auto *gtExpr = new BinaryExpr(expressions::Operator::GT, paramX, lit5, BooleanType());
    std::unordered_map<std::string, int32_t> paramNameToIdxMap;
    paramNameToIdxMap.emplace("x", 0);

    auto expr = FuncExpr("filter", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new LambdaExpr(gtExpr, paramTypes, paramNameToIdxMap, BooleanType())
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto *context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto *result = e.GetResult();

    auto *resultArr = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(resultArr, nullptr);
    ASSERT_EQ(resultArr->GetSize(), 2);

    ASSERT_EQ(resultArr->GetOffset(1) - resultArr->GetOffset(0), 1);

    auto *elemResult = dynamic_cast<Vector<int32_t> *>(resultArr->GetElementVector().get());
    ASSERT_NE(elemResult, nullptr);
    ASSERT_EQ(elemResult->GetValue(0), 10);

    delete context;
    delete input;
    delete result;
}
