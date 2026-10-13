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
using namespace omniruntime::TestUtil;

static VectorBatch *CreateIntArrayVectorBatch(
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

TEST(ExistsTest, BasicAnyMatch)
{
    int rowSize = 3;
    int32_t elems[] = {1, 5, 3, 0, 10};
    std::vector<int32_t> offsets = {0, 3, 4, 5};

    auto *input = CreateIntArrayVectorBatch(rowSize, 5, elems, offsets);

    std::vector<DataTypePtr> paramTypes;
    paramTypes.push_back(IntType());
    auto *paramX = new ParamRefExpr("x", IntType());
    auto *lit4 = new LiteralExpr(4, IntType());
    auto *gtExpr = new BinaryExpr(expressions::Operator::GT, paramX, lit4, BooleanType());
    std::unordered_map<std::string, int32_t> paramNameToIdxMap;
    paramNameToIdxMap.emplace("x", 0);

    auto expr = FuncExpr("exists", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new LambdaExpr(gtExpr, paramTypes, paramNameToIdxMap, BooleanType())
    }, BooleanType());

    auto *context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto *result = e.GetResult();

    auto *boolVec = dynamic_cast<Vector<bool> *>(result);
    ASSERT_NE(boolVec, nullptr);
    ASSERT_EQ(boolVec->GetSize(), 3);

    ASSERT_FALSE(boolVec->IsNull(0));
    ASSERT_EQ(boolVec->GetValue(0), true);

    ASSERT_FALSE(boolVec->IsNull(1));
    ASSERT_EQ(boolVec->GetValue(1), false);

    ASSERT_FALSE(boolVec->IsNull(2));
    ASSERT_EQ(boolVec->GetValue(2), true);

    delete context;
    delete input;
    delete result;
}

TEST(ExistsTest, BasicNoMatch)
{
    int rowSize = 3;
    int32_t elems[] = {1, 2, 3, 0, -1};
    std::vector<int32_t> offsets = {0, 3, 4, 5};

    auto *input = CreateIntArrayVectorBatch(rowSize, 5, elems, offsets);

    std::vector<DataTypePtr> paramTypes;
    paramTypes.push_back(IntType());
    auto *paramX = new ParamRefExpr("x", IntType());
    auto *lit100 = new LiteralExpr(100, IntType());
    auto *gtExpr = new BinaryExpr(expressions::Operator::GT, paramX, lit100, BooleanType());
    std::unordered_map<std::string, int32_t> paramNameToIdxMap;
    paramNameToIdxMap.emplace("x", 0);

    auto expr = FuncExpr("exists", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new LambdaExpr(gtExpr, paramTypes, paramNameToIdxMap, BooleanType())
    }, BooleanType());

    auto *context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto *result = e.GetResult();

    auto *boolVec = dynamic_cast<Vector<bool> *>(result);
    ASSERT_NE(boolVec, nullptr);
    ASSERT_EQ(boolVec->GetSize(), 3);

    ASSERT_FALSE(boolVec->IsNull(0));
    ASSERT_EQ(boolVec->GetValue(0), false);

    ASSERT_FALSE(boolVec->IsNull(1));
    ASSERT_EQ(boolVec->GetValue(1), false);

    ASSERT_FALSE(boolVec->IsNull(2));
    ASSERT_EQ(boolVec->GetValue(2), false);

    delete context;
    delete input;
    delete result;
}

TEST(ExistsTest, EmptyArrayReturnsFalse)
{
    int rowSize = 2;
    int32_t elems[] = {10, 20};
    std::vector<int32_t> offsets = {0, 0, 2};

    auto *input = CreateIntArrayVectorBatch(rowSize, 2, elems, offsets);

    std::vector<DataTypePtr> paramTypes;
    paramTypes.push_back(IntType());
    auto *paramX = new ParamRefExpr("x", IntType());
    auto *lit100 = new LiteralExpr(100, IntType());
    auto *gtExpr = new BinaryExpr(expressions::Operator::GT, paramX, lit100, BooleanType());
    std::unordered_map<std::string, int32_t> paramNameToIdxMap;
    paramNameToIdxMap.emplace("x", 0);

    auto expr = FuncExpr("exists", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new LambdaExpr(gtExpr, paramTypes, paramNameToIdxMap, BooleanType())
    }, BooleanType());

    auto *context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto *result = e.GetResult();

    auto *boolVec = dynamic_cast<Vector<bool> *>(result);
    ASSERT_NE(boolVec, nullptr);
    ASSERT_EQ(boolVec->GetSize(), 2);

    ASSERT_FALSE(boolVec->IsNull(0));
    ASSERT_EQ(boolVec->GetValue(0), false);

    ASSERT_FALSE(boolVec->IsNull(1));
    ASSERT_EQ(boolVec->GetValue(1), false);

    delete context;
    delete input;
    delete result;
}

TEST(ExistsTest, NullArrayRowReturnsNull)
{
    int rowSize = 3;
    int32_t elems[] = {5, 3, 10};
    std::vector<int32_t> offsets = {0, 2, 2, 3};

    auto *input = CreateIntArrayVectorBatch(rowSize, 3, elems, offsets);
    auto *arrVec = dynamic_cast<ArrayVector *>(input->Get(0));
    arrVec->SetNull(1);

    std::vector<DataTypePtr> paramTypes;
    paramTypes.push_back(IntType());
    auto *paramX = new ParamRefExpr("x", IntType());
    auto *lit1 = new LiteralExpr(1, IntType());
    auto *gtExpr = new BinaryExpr(expressions::Operator::GT, paramX, lit1, BooleanType());
    std::unordered_map<std::string, int32_t> paramNameToIdxMap;
    paramNameToIdxMap.emplace("x", 0);

    auto expr = FuncExpr("exists", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new LambdaExpr(gtExpr, paramTypes, paramNameToIdxMap, BooleanType())
    }, BooleanType());

    auto *context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto *result = e.GetResult();

    auto *boolVec = dynamic_cast<Vector<bool> *>(result);
    ASSERT_NE(boolVec, nullptr);
    ASSERT_EQ(boolVec->GetSize(), 3);

    ASSERT_FALSE(boolVec->IsNull(0));
    ASSERT_EQ(boolVec->GetValue(0), true);

    ASSERT_TRUE(boolVec->IsNull(1));

    ASSERT_FALSE(boolVec->IsNull(2));
    ASSERT_EQ(boolVec->GetValue(2), true);

    delete context;
    delete input;
    delete result;
}

TEST(ExistsTest, NullElementsInArray)
{
    int rowSize = 3;
    int32_t elems[] = {5, 3, 3, 0};
    std::vector<int32_t> offsets = {0, 2, 3, 4};

    auto *input = CreateIntArrayVectorBatch(rowSize, 4, elems, offsets);
    auto *arrVec = dynamic_cast<ArrayVector *>(input->Get(0));
    auto *elemVec = dynamic_cast<Vector<int32_t> *>(arrVec->GetElementVector().get());
    elemVec->SetNull(0);

    std::vector<DataTypePtr> paramTypes;
    paramTypes.push_back(IntType());
    auto *paramX = new ParamRefExpr("x", IntType());
    auto *lit1 = new LiteralExpr(1, IntType());
    auto *gtExpr = new BinaryExpr(expressions::Operator::GT, paramX, lit1, BooleanType());
    std::unordered_map<std::string, int32_t> paramNameToIdxMap;
    paramNameToIdxMap.emplace("x", 0);

    auto expr = FuncExpr("exists", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new LambdaExpr(gtExpr, paramTypes, paramNameToIdxMap, BooleanType())
    }, BooleanType());

    auto *context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto *result = e.GetResult();

    auto *boolVec = dynamic_cast<Vector<bool> *>(result);
    ASSERT_NE(boolVec, nullptr);
    ASSERT_EQ(boolVec->GetSize(), 3);

    ASSERT_FALSE(boolVec->IsNull(0));
    ASSERT_EQ(boolVec->GetValue(0), true);

    ASSERT_FALSE(boolVec->IsNull(1));
    ASSERT_EQ(boolVec->GetValue(1), true);

    ASSERT_FALSE(boolVec->IsNull(2));
    ASSERT_EQ(boolVec->GetValue(2), false);

    delete context;
    delete input;
    delete result;
}

TEST(ExistsTest, AllNullArrayRows)
{
    int rowSize = 2;
    int32_t elems[] = {1};
    std::vector<int32_t> offsets = {0, 0, 1};

    auto *input = CreateIntArrayVectorBatch(rowSize, 1, elems, offsets);
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

    auto expr = FuncExpr("exists", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new LambdaExpr(gtExpr, paramTypes, paramNameToIdxMap, BooleanType())
    }, BooleanType());

    auto *context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto *result = e.GetResult();

    auto *boolVec = dynamic_cast<Vector<bool> *>(result);
    ASSERT_NE(boolVec, nullptr);
    ASSERT_EQ(boolVec->GetSize(), 2);

    ASSERT_TRUE(boolVec->IsNull(0));
    ASSERT_TRUE(boolVec->IsNull(1));

    delete context;
    delete input;
    delete result;
}

TEST(ExistsTest, MixedMatchAndNonMatch)
{
    int rowSize = 4;
    int32_t elems[] = {10, 20, 30, 1, 5, 100, 200};
    std::vector<int32_t> offsets = {0, 3, 4, 5, 7};

    auto *input = CreateIntArrayVectorBatch(rowSize, 7, elems, offsets);

    std::vector<DataTypePtr> paramTypes;
    paramTypes.push_back(IntType());
    auto *paramX = new ParamRefExpr("x", IntType());
    auto *lit50 = new LiteralExpr(50, IntType());
    auto *gteExpr = new BinaryExpr(expressions::Operator::GTE, paramX, lit50, BooleanType());
    std::unordered_map<std::string, int32_t> paramNameToIdxMap;
    paramNameToIdxMap.emplace("x", 0);

    auto expr = FuncExpr("exists", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new LambdaExpr(gteExpr, paramTypes, paramNameToIdxMap, BooleanType())
    }, BooleanType());

    auto *context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto *result = e.GetResult();

    auto *boolVec = dynamic_cast<Vector<bool> *>(result);
    ASSERT_NE(boolVec, nullptr);
    ASSERT_EQ(boolVec->GetSize(), 4);

    ASSERT_FALSE(boolVec->IsNull(0));
    ASSERT_EQ(boolVec->GetValue(0), false);

    ASSERT_FALSE(boolVec->IsNull(1));
    ASSERT_EQ(boolVec->GetValue(1), false);

    ASSERT_FALSE(boolVec->IsNull(2));
    ASSERT_EQ(boolVec->GetValue(2), false);

    ASSERT_FALSE(boolVec->IsNull(3));
    ASSERT_EQ(boolVec->GetValue(3), true);

    delete context;
    delete input;
    delete result;
}

TEST(ExistsTest, SingleElementArray)
{
    int rowSize = 2;
    int32_t elems[] = {10, 0};
    std::vector<int32_t> offsets = {0, 1, 2};

    auto *input = CreateIntArrayVectorBatch(rowSize, 2, elems, offsets);

    std::vector<DataTypePtr> paramTypes;
    paramTypes.push_back(IntType());
    auto *paramX = new ParamRefExpr("x", IntType());
    auto *lit5 = new LiteralExpr(5, IntType());
    auto *gtExpr = new BinaryExpr(expressions::Operator::GT, paramX, lit5, BooleanType());
    std::unordered_map<std::string, int32_t> paramNameToIdxMap;
    paramNameToIdxMap.emplace("x", 0);

    auto expr = FuncExpr("exists", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new LambdaExpr(gtExpr, paramTypes, paramNameToIdxMap, BooleanType())
    }, BooleanType());

    auto *context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto *result = e.GetResult();

    auto *boolVec = dynamic_cast<Vector<bool> *>(result);
    ASSERT_NE(boolVec, nullptr);
    ASSERT_EQ(boolVec->GetSize(), 2);

    ASSERT_FALSE(boolVec->IsNull(0));
    ASSERT_EQ(boolVec->GetValue(0), true);

    ASSERT_FALSE(boolVec->IsNull(1));
    ASSERT_EQ(boolVec->GetValue(1), false);

    delete context;
    delete input;
    delete result;
}

TEST(ExistsTest, NullElementNoMatchReturnsNull)
{
    int rowSize = 2;
    int32_t elems[] = {0, 1, 2};
    std::vector<int32_t> offsets = {0, 2, 3};

    auto *input = CreateIntArrayVectorBatch(rowSize, 3, elems, offsets);
    auto *arrVec = dynamic_cast<ArrayVector *>(input->Get(0));
    auto *elemVec = dynamic_cast<Vector<int32_t> *>(arrVec->GetElementVector().get());
    elemVec->SetNull(0);

    std::vector<DataTypePtr> paramTypes;
    paramTypes.push_back(IntType());
    auto *paramX = new ParamRefExpr("x", IntType());
    auto *lit10 = new LiteralExpr(10, IntType());
    auto *gtExpr = new BinaryExpr(expressions::Operator::GT, paramX, lit10, BooleanType());
    std::unordered_map<std::string, int32_t> paramNameToIdxMap;
    paramNameToIdxMap.emplace("x", 0);

    auto expr = FuncExpr("exists", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new LambdaExpr(gtExpr, paramTypes, paramNameToIdxMap, BooleanType())
    }, BooleanType());

    auto *context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto *result = e.GetResult();

    auto *boolVec = dynamic_cast<Vector<bool> *>(result);
    ASSERT_NE(boolVec, nullptr);
    ASSERT_EQ(boolVec->GetSize(), 2);

    ASSERT_TRUE(boolVec->IsNull(0));

    ASSERT_FALSE(boolVec->IsNull(1));
    ASSERT_EQ(boolVec->GetValue(1), false);

    delete context;
    delete input;
    delete result;
}

TEST(ExistsTest, NullElementWithMatchReturnsTrue)
{
    int rowSize = 1;
    int32_t elems[] = {5, 20};
    std::vector<int32_t> offsets = {0, 2};

    auto *input = CreateIntArrayVectorBatch(rowSize, 2, elems, offsets);
    auto *arrVec = dynamic_cast<ArrayVector *>(input->Get(0));
    auto *elemVec = dynamic_cast<Vector<int32_t> *>(arrVec->GetElementVector().get());
    elemVec->SetNull(0);

    std::vector<DataTypePtr> paramTypes;
    paramTypes.push_back(IntType());
    auto *paramX = new ParamRefExpr("x", IntType());
    auto *lit10 = new LiteralExpr(10, IntType());
    auto *gtExpr = new BinaryExpr(expressions::Operator::GT, paramX, lit10, BooleanType());
    std::unordered_map<std::string, int32_t> paramNameToIdxMap;
    paramNameToIdxMap.emplace("x", 0);

    auto expr = FuncExpr("exists", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new LambdaExpr(gtExpr, paramTypes, paramNameToIdxMap, BooleanType())
    }, BooleanType());

    auto *context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto *result = e.GetResult();

    auto *boolVec = dynamic_cast<Vector<bool> *>(result);
    ASSERT_NE(boolVec, nullptr);
    ASSERT_EQ(boolVec->GetSize(), 1);

    ASSERT_FALSE(boolVec->IsNull(0));
    ASSERT_EQ(boolVec->GetValue(0), true);

    delete context;
    delete input;
    delete result;
}
