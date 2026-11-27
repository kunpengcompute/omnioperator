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

TEST(VectorizationTest, SimpleFunction)
{
    int rowSize = 3;
    auto type = std::make_shared<DataType>(OMNI_INT);
    std::vector<omniruntime::type::DataTypeId> args = {OMNI_INT, OMNI_INT};
    auto addExpr = new BinaryExpr(expressions::Operator::ADD, new FieldExpr(0, type), new FieldExpr(1, type), type);
    auto addExpr2 = new BinaryExpr(expressions::Operator::ADD, addExpr, new FieldExpr(1, type), type);

    std::vector vecOfTypes = {IntType(), IntType()};
    DataTypes inputTypes(vecOfTypes);
    int32_t col1[rowSize] = {1, 2, 3};
    int32_t col2[rowSize] = {3, 1, -4};
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1, col2);
    input->Get(0)->SetNull(1);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(*addExpr2);
    auto result = e.GetResult();
    VectorBatch vectorBatch(rowSize);
    vectorBatch.Append(result);
    VectorHelper::PrintVecBatch(&vectorBatch);

    delete input;
    delete addExpr2;
    delete context;
}

TEST(VectorizationTest, AddInt)
{
    int rowSize = 3;
    auto type = std::make_shared<DataType>(OMNI_INT);
    std::vector<omniruntime::type::DataTypeId> args = {OMNI_INT, OMNI_INT};
    auto addExpr = new BinaryExpr(expressions::Operator::ADD, new FieldExpr(0, type), new FieldExpr(1, type), type);
    auto signature = std::make_shared<FunctionSignature>("add", args, OMNI_INT);

    std::vector vecOfTypes = {IntType(), IntType()};
    DataTypes inputTypes(vecOfTypes);
    int32_t col1[rowSize] = {1, 2, 3};
    int32_t col2[rowSize] = {3, 1, -4};
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1, col2);
    input->Get(0)->SetNull(1);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.Visit(*addExpr);
    auto result = e.GetResult();
    VectorBatch vectorBatch(rowSize);
    vectorBatch.Append(result);
    std::cout << "=== AddInt Result ===" << std::endl;
    VectorHelper::PrintVecBatch(&vectorBatch);

    auto *resultVector = dynamic_cast<Vector<int32_t> *>(result);
    EXPECT_EQ(resultVector->GetValue(0), 4);
    EXPECT_TRUE(resultVector->IsNull(1));
    EXPECT_EQ(resultVector->GetValue(2), -1);

    delete input;
    delete addExpr;
    delete context;
}

TEST(VectorizationTest, SubInt)
{
    int rowSize = 3;
    auto type = std::make_shared<DataType>(OMNI_INT);
    std::vector<omniruntime::type::DataTypeId> args = {OMNI_INT, OMNI_INT};
    auto subExpr = new BinaryExpr(expressions::Operator::SUB, new FieldExpr(0, type), new FieldExpr(1, type), type);
    auto signature = std::make_shared<FunctionSignature>("subtract", args, OMNI_INT);
    ASSERT_NE(subExpr->vectorFunction, nullptr) << "subtract function not found";

    std::vector vecOfTypes = {IntType(), IntType()};
    DataTypes inputTypes(vecOfTypes);
    int32_t col2[rowSize] = {5, 2, 8};
    int32_t col1[rowSize] = {3, 2, -3};
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1, col2);
    input->Get(0)->SetNull(1);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.Visit(*subExpr);
    auto result = e.GetResult();

    VectorBatch vectorBatch(rowSize);
    vectorBatch.Append(result);
    std::cout << "=== SubInt Result ===" << std::endl;
    VectorHelper::PrintVecBatch(&vectorBatch);

    auto* resultVector = dynamic_cast<Vector<int32_t>*>(result);
    EXPECT_EQ(resultVector->GetValue(0), -2);
    EXPECT_TRUE(resultVector->IsNull(1));
    EXPECT_EQ(resultVector->GetValue(2), -11);

    delete input;
    delete subExpr;
    delete context;
}

TEST(VectorizationTest, MulInt)
{
    int rowSize = 3;
    auto type = std::make_shared<DataType>(OMNI_INT);
    std::vector<omniruntime::type::DataTypeId> args = {OMNI_INT, OMNI_INT};
    auto mulExpr = new BinaryExpr(expressions::Operator::MUL, new FieldExpr(0, type), new FieldExpr(1, type), type);
    auto signature = std::make_shared<FunctionSignature>("multiply", args, OMNI_INT);
    ASSERT_NE(mulExpr->vectorFunction, nullptr) << "multiply function not found";

    std::vector vecOfTypes = {IntType(), IntType()};
    DataTypes inputTypes(vecOfTypes);
    int32_t col1[rowSize] = {2, 3, -4};
    int32_t col2[rowSize] = {3, 5, -2};
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1, col2);
    input->Get(0)->SetNull(1);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.Visit(*mulExpr);
    auto result = e.GetResult();

    VectorBatch vectorBatch(rowSize);
    vectorBatch.Append(result);
    std::cout << "=== MulInt Result ===" << std::endl;
    VectorHelper::PrintVecBatch(&vectorBatch);

    auto* resultVector = dynamic_cast<Vector<int32_t>*>(result);
    EXPECT_EQ(resultVector->GetValue(0), 6);
    EXPECT_TRUE(resultVector->IsNull(1));
    EXPECT_EQ(resultVector->GetValue(2), 8);

    delete input;
    delete mulExpr;
    delete context;
}

TEST(VectorizationTest, DivideInt)
{
    int rowSize = 3;
    auto type = std::make_shared<DataType>(OMNI_INT);
    std::vector<omniruntime::type::DataTypeId> args = {OMNI_INT, OMNI_INT};
    auto divExpr = new BinaryExpr(expressions::Operator::DIV, new FieldExpr(0, type), new FieldExpr(1, type), type);
    auto signature = std::make_shared<FunctionSignature>("divide", args, OMNI_INT);
    ASSERT_NE(divExpr->vectorFunction, nullptr) << "divide function not found";

    std::vector vecOfTypes = {IntType(), IntType()};
    DataTypes inputTypes(vecOfTypes);
    int32_t col1[rowSize] = {6, 9, -8};
    int32_t col2[rowSize] = {2, 3, -4};
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1, col2);
    input->Get(0)->SetNull(1);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.Visit(*divExpr);
    auto result = e.GetResult();

    VectorBatch vectorBatch(rowSize);
    vectorBatch.Append(result);
    std::cout << "=== DivideInt Result ===" << std::endl;
    VectorHelper::PrintVecBatch(&vectorBatch);

    auto* resultVector = dynamic_cast<Vector<int32_t>*>(result);
    EXPECT_EQ(resultVector->GetValue(0), 3);
    EXPECT_TRUE(resultVector->IsNull(1));
    EXPECT_EQ(resultVector->GetValue(2), 2);

    delete input;
    delete divExpr;
    delete context;
}

TEST(VectorizationTest, RemainderInt)
{
    int rowSize = 3;
    auto type = std::make_shared<DataType>(OMNI_INT);
    std::vector<omniruntime::type::DataTypeId> args = {OMNI_INT, OMNI_INT};
    auto remExpr = new BinaryExpr(expressions::Operator::MOD, new FieldExpr(0, type), new FieldExpr(1, type), type);
    auto signature = std::make_shared<FunctionSignature>("remainder", args, OMNI_INT);
    ASSERT_NE(remExpr->vectorFunction, nullptr) << "remainder function not found";

    std::vector vecOfTypes = {IntType(), IntType()};
    DataTypes inputTypes(vecOfTypes);
    int32_t col1[rowSize] = {7, 10, -5};
    int32_t col2[rowSize] = {3, 4, 3};
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1, col2);
    input->Get(0)->SetNull(1);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.Visit(*remExpr);
    auto result = e.GetResult();

    VectorBatch vectorBatch(rowSize);
    vectorBatch.Append(result);
    std::cout << "=== RemainderInt Result ===" << std::endl;
    VectorHelper::PrintVecBatch(&vectorBatch);

    auto* resultVector = dynamic_cast<Vector<int32_t>*>(result);
    EXPECT_EQ(resultVector->GetValue(0), 1);
    EXPECT_TRUE(resultVector->IsNull(1));
    EXPECT_EQ(resultVector->GetValue(2), -2);

    delete input;
    delete remExpr;
    delete context;
}

TEST(VectorizationTest, Filter)
{
    int rowSize = 3;
    auto type = std::make_shared<DataType>(OMNI_INT);
    std::vector<omniruntime::type::DataTypeId> args = {OMNI_INT, OMNI_INT};
    auto addExpr = new BinaryExpr(expressions::Operator::ADD, new FieldExpr(0, type), new FieldExpr(1, type), type);
    auto addExpr2 = new BinaryExpr(expressions::Operator::ADD, addExpr, new FieldExpr(1, type), type);

    std::vector vecOfTypes = {IntType(), IntType()};
    DataTypes inputTypes(vecOfTypes);
    int32_t col1[rowSize] = {1, 2, 3};
    int32_t col2[rowSize] = {3, 1, -4};
    VectorBatch *input = CreateVectorBatch(inputTypes, rowSize, col1, col2);
    input->Get(0)->SetNull(1);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(*addExpr2);
    auto result = e.GetResult();
    VectorBatch vectorBatch(rowSize);
    vectorBatch.Append(result);
    VectorHelper::PrintVecBatch(&vectorBatch);

    delete input;
    delete addExpr2;
    delete context;
}
