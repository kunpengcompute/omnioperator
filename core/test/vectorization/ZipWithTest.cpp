/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: zip_with function unit tests
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

static VectorBatch *CreateTwoArrayVectorBatch(
    int32_t rowSize,
    int32_t leftElemSize, int32_t *leftElems, std::vector<int32_t> &leftOffsets,
    int32_t rightElemSize, int32_t *rightElems, std::vector<int32_t> &rightOffsets)
{
    auto *vectorBatch = new VectorBatch(rowSize);

    auto *leftElemVec = new Vector<int32_t>(leftElemSize, OMNI_INT);
    for (int32_t i = 0; i < leftElemSize; ++i) {
        leftElemVec->SetValue(i, leftElems[i]);
    }
    auto *leftArr = new ArrayVector(rowSize, std::shared_ptr<BaseVector>(leftElemVec));
    for (size_t j = 0; j < leftOffsets.size(); ++j) {
        leftArr->SetOffset(j, leftOffsets[j]);
    }
    vectorBatch->Append(leftArr);

    auto *rightElemVec = new Vector<int32_t>(rightElemSize, OMNI_INT);
    for (int32_t i = 0; i < rightElemSize; ++i) {
        rightElemVec->SetValue(i, rightElems[i]);
    }
    auto *rightArr = new ArrayVector(rowSize, std::shared_ptr<BaseVector>(rightElemVec));
    for (size_t j = 0; j < rightOffsets.size(); ++j) {
        rightArr->SetOffset(j, rightOffsets[j]);
    }
    vectorBatch->Append(rightArr);

    return vectorBatch;
}

TEST(ZipWithTest, SameSizeArrays)
{
    int rowSize = 3;
    int32_t leftElems[] = {1, 2, 3, 4, 5};
    std::vector<int32_t> leftOffsets = {0, 3, 4, 5};
    int32_t rightElems[] = {10, 20, 30, 40, 50};
    std::vector<int32_t> rightOffsets = {0, 3, 4, 5};

    auto input = CreateTwoArrayVectorBatch(rowSize,
        5, leftElems, leftOffsets,
        5, rightElems, rightOffsets);

    std::vector<DataTypePtr> paramTypes;
    paramTypes.push_back(IntType());
    paramTypes.push_back(IntType());
    ParamRefExpr *paramX = new ParamRefExpr("x", IntType());
    ParamRefExpr *paramY = new ParamRefExpr("y", IntType());
    BinaryExpr *addExpr = new BinaryExpr(omniruntime::expressions::Operator::ADD, paramX, paramY, IntType());
    std::unordered_map<std::string, int32_t> paramNameToIdxMap;
    paramNameToIdxMap.emplace("x", 0);
    paramNameToIdxMap.emplace("y", 1);

    auto expr = FuncExpr("zip_with", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_ARRAY)),
        new LambdaExpr(addExpr, paramTypes, paramNameToIdxMap, IntType())
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    ArrayVector *arrVec = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(arrVec, nullptr);
    int32_t expect[] = {11, 22, 33, 44, 55};
    auto element = dynamic_cast<Vector<int32_t> *>(arrVec->GetElementVector().get());
    ASSERT_NE(element, nullptr);
    for (int i = 0; i < 5; ++i) {
        ASSERT_EQ(element->GetValue(i), expect[i]);
    }

    ASSERT_EQ(arrVec->GetSize(0), 3);
    ASSERT_EQ(arrVec->GetSize(1), 1);
    ASSERT_EQ(arrVec->GetSize(2), 1);

    delete context;
    delete input;
    delete result;
}

TEST(ZipWithTest, DifferentSizeArrays)
{
    int rowSize = 2;
    int32_t leftElems[] = {1, 2, 3, 4, 5};
    std::vector<int32_t> leftOffsets = {0, 3, 5};
    int32_t rightElems[] = {10, 30, 40, 50};
    std::vector<int32_t> rightOffsets = {0, 1, 4};

    auto input = CreateTwoArrayVectorBatch(rowSize,
        5, leftElems, leftOffsets,
        4, rightElems, rightOffsets);

    std::vector<DataTypePtr> paramTypes;
    paramTypes.push_back(IntType());
    paramTypes.push_back(IntType());
    ParamRefExpr *paramX = new ParamRefExpr("x", IntType());
    ParamRefExpr *paramY = new ParamRefExpr("y", IntType());
    BinaryExpr *addExpr = new BinaryExpr(omniruntime::expressions::Operator::ADD, paramX, paramY, IntType());
    std::unordered_map<std::string, int32_t> paramNameToIdxMap;
    paramNameToIdxMap.emplace("x", 0);
    paramNameToIdxMap.emplace("y", 1);

    auto expr = FuncExpr("zip_with", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_ARRAY)),
        new LambdaExpr(addExpr, paramTypes, paramNameToIdxMap, IntType())
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    ArrayVector *arrVec = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(arrVec, nullptr);
    ASSERT_EQ(arrVec->GetSize(0), 3);
    ASSERT_EQ(arrVec->GetSize(1), 3);

    auto element = dynamic_cast<Vector<int32_t> *>(arrVec->GetElementVector().get());
    ASSERT_NE(element, nullptr);

    ASSERT_EQ(element->GetValue(0), 11);
    ASSERT_FALSE(element->IsNull(0));

    ASSERT_TRUE(element->IsNull(1));
    ASSERT_TRUE(element->IsNull(2));

    ASSERT_EQ(element->GetValue(3), 34);
    ASSERT_FALSE(element->IsNull(3));
    ASSERT_EQ(element->GetValue(4), 45);
    ASSERT_FALSE(element->IsNull(4));

    ASSERT_TRUE(element->IsNull(5));

    delete context;
    delete input;
    delete result;
}

TEST(ZipWithTest, NullArrayInput)
{
    int rowSize = 3;
    int32_t leftElems[] = {1, 2, 3, 5};
    std::vector<int32_t> leftOffsets = {0, 3, 3, 4};
    int32_t rightElems[] = {10, 20, 30, 40, 50};
    std::vector<int32_t> rightOffsets = {0, 3, 4, 5};

    auto input = CreateTwoArrayVectorBatch(rowSize,
        4, leftElems, leftOffsets,
        5, rightElems, rightOffsets);

    auto *leftArr = dynamic_cast<ArrayVector *>(input->Get(0));
    leftArr->SetNull(1);

    std::vector<DataTypePtr> paramTypes;
    paramTypes.push_back(IntType());
    paramTypes.push_back(IntType());
    ParamRefExpr *paramX = new ParamRefExpr("x", IntType());
    ParamRefExpr *paramY = new ParamRefExpr("y", IntType());
    BinaryExpr *addExpr = new BinaryExpr(omniruntime::expressions::Operator::ADD, paramX, paramY, IntType());
    std::unordered_map<std::string, int32_t> paramNameToIdxMap;
    paramNameToIdxMap.emplace("x", 0);
    paramNameToIdxMap.emplace("y", 1);

    auto expr = FuncExpr("zip_with", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_ARRAY)),
        new LambdaExpr(addExpr, paramTypes, paramNameToIdxMap, IntType())
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    ArrayVector *arrVec = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(arrVec, nullptr);

    ASSERT_FALSE(arrVec->IsNull(0));
    ASSERT_EQ(arrVec->GetSize(0), 3);

    ASSERT_TRUE(arrVec->IsNull(1));
    ASSERT_EQ(arrVec->GetSize(1), 0);

    ASSERT_FALSE(arrVec->IsNull(2));
    ASSERT_EQ(arrVec->GetSize(2), 1);

    auto element = dynamic_cast<Vector<int32_t> *>(arrVec->GetElementVector().get());
    ASSERT_NE(element, nullptr);
    ASSERT_EQ(element->GetValue(0), 11);
    ASSERT_EQ(element->GetValue(1), 22);
    ASSERT_EQ(element->GetValue(2), 33);
    ASSERT_EQ(element->GetValue(3), 55);

    delete context;
    delete input;
    delete result;
}

TEST(ZipWithTest, EmptyArrays)
{
    int rowSize = 2;
    int32_t leftElems[] = {1, 2};
    std::vector<int32_t> leftOffsets = {0, 0, 2};
    int32_t rightElems[] = {10};
    std::vector<int32_t> rightOffsets = {0, 0, 1};

    auto input = CreateTwoArrayVectorBatch(rowSize,
        2, leftElems, leftOffsets,
        1, rightElems, rightOffsets);

    std::vector<DataTypePtr> paramTypes;
    paramTypes.push_back(IntType());
    paramTypes.push_back(IntType());
    ParamRefExpr *paramX = new ParamRefExpr("x", IntType());
    ParamRefExpr *paramY = new ParamRefExpr("y", IntType());
    BinaryExpr *addExpr = new BinaryExpr(omniruntime::expressions::Operator::ADD, paramX, paramY, IntType());
    std::unordered_map<std::string, int32_t> paramNameToIdxMap;
    paramNameToIdxMap.emplace("x", 0);
    paramNameToIdxMap.emplace("y", 1);

    auto expr = FuncExpr("zip_with", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_ARRAY)),
        new LambdaExpr(addExpr, paramTypes, paramNameToIdxMap, IntType())
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    ArrayVector *arrVec = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(arrVec, nullptr);

    ASSERT_EQ(arrVec->GetSize(0), 0);
    ASSERT_EQ(arrVec->GetSize(1), 2);

    auto element = dynamic_cast<Vector<int32_t> *>(arrVec->GetElementVector().get());
    ASSERT_NE(element, nullptr);
    ASSERT_EQ(element->GetValue(0), 11);
    ASSERT_TRUE(element->IsNull(1));

    delete context;
    delete input;
    delete result;
}

TEST(ZipWithTest, AllNullArrays)
{
    int rowSize = 2;
    int32_t leftElems[] = {1};
    std::vector<int32_t> leftOffsets = {0, 0, 1};
    int32_t rightElems[] = {10};
    std::vector<int32_t> rightOffsets = {0, 0, 1};

    auto input = CreateTwoArrayVectorBatch(rowSize,
        1, leftElems, leftOffsets,
        1, rightElems, rightOffsets);

    auto *leftArr = dynamic_cast<ArrayVector *>(input->Get(0));
    auto *rightArr = dynamic_cast<ArrayVector *>(input->Get(1));
    leftArr->SetNull(0);
    leftArr->SetNull(1);
    rightArr->SetNull(0);
    rightArr->SetNull(1);

    std::vector<DataTypePtr> paramTypes;
    paramTypes.push_back(IntType());
    paramTypes.push_back(IntType());
    ParamRefExpr *paramX = new ParamRefExpr("x", IntType());
    ParamRefExpr *paramY = new ParamRefExpr("y", IntType());
    BinaryExpr *addExpr = new BinaryExpr(omniruntime::expressions::Operator::ADD, paramX, paramY, IntType());
    std::unordered_map<std::string, int32_t> paramNameToIdxMap;
    paramNameToIdxMap.emplace("x", 0);
    paramNameToIdxMap.emplace("y", 1);

    auto expr = FuncExpr("zip_with", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_ARRAY)),
        new LambdaExpr(addExpr, paramTypes, paramNameToIdxMap, IntType())
    }, std::make_shared<DataType>(OMNI_ARRAY));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);
    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    ArrayVector *arrVec = dynamic_cast<ArrayVector *>(result);
    ASSERT_NE(arrVec, nullptr);

    ASSERT_TRUE(arrVec->IsNull(0));
    ASSERT_TRUE(arrVec->IsNull(1));

    delete context;
    delete input;
    delete result;
}
