/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Unit tests for TRY arithmetic vector functions
 */

#include <gtest/gtest.h>

#include <limits>
#include <memory>
#include <stack>
#include <string>
#include <vector>

#include "expression/expressions.h"
#include "type/data_type.h"
#include "vector/vector_helper.h"
#include "vectorization/ExprEval.h"
#include "vectorization/VectorFunction.h"
#include "vectorization/registration/Register.h"

using namespace omniruntime;
using namespace omniruntime::codegen;
using namespace omniruntime::expressions;
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;

namespace {

class TryArithmeticTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        RegisterFunctions::Register();
    }
};

template <typename T>
std::unique_ptr<BaseVector> EvaluatePrimitive(
    const std::string &name, DataTypeId typeId, const std::vector<T> &leftValues,
    const std::vector<T> &rightValues, int32_t nullRow = -1)
{
    const auto rowSize = static_cast<int32_t>(leftValues.size());
    auto *left = static_cast<Vector<T> *>(VectorHelper::CreateFlatVector(typeId, rowSize));
    auto *right = static_cast<Vector<T> *>(VectorHelper::CreateFlatVector(typeId, rowSize));
    for (int32_t row = 0; row < rowSize; ++row) {
        left->SetValue(row, leftValues[row]);
        left->SetNotNull(row);
        right->SetValue(row, rightValues[row]);
        right->SetNotNull(row);
    }
    if (nullRow >= 0) {
        left->SetNull(nullRow);
    }

    auto signature = std::make_shared<FunctionSignature>(name, std::vector<DataTypeId> { typeId, typeId }, typeId);
    auto function = VectorFunction::Find(signature);
    EXPECT_NE(function, nullptr);
    if (function == nullptr) {
        delete left;
        delete right;
        return nullptr;
    }

    ExecutionContext context;
    context.SetResultRowSize(rowSize);
    std::stack<BaseVector *> arguments;
    arguments.push(left);
    arguments.push(right);
    BaseVector *result = nullptr;
    function->Apply(arguments, std::make_shared<DataType>(typeId), result, &context);
    return std::unique_ptr<BaseVector>(result);
}

TEST_F(TryArithmeticTest, PrimitiveValuesErrorsAndNulls)
{
    auto add = EvaluatePrimitive<int32_t>("try_add", OMNI_INT,
        { 10, std::numeric_limits<int32_t>::max(), 1 }, { 20, 1, 1 }, 2);
    ASSERT_NE(add, nullptr);
    auto *addValues = static_cast<Vector<int32_t> *>(add.get());
    EXPECT_EQ(addValues->GetValue(0), 30);
    EXPECT_TRUE(add->IsNull(1));
    EXPECT_TRUE(add->IsNull(2));

    auto subtract = EvaluatePrimitive<int32_t>("try_subtract", OMNI_INT,
        { std::numeric_limits<int32_t>::min() }, { 1 });
    ASSERT_NE(subtract, nullptr);
    EXPECT_TRUE(subtract->IsNull(0));

    auto multiply = EvaluatePrimitive<int64_t>("try_multiply", OMNI_LONG,
        { std::numeric_limits<int64_t>::max() }, { 2 });
    ASSERT_NE(multiply, nullptr);
    EXPECT_TRUE(multiply->IsNull(0));

    auto divide = EvaluatePrimitive<double>("try_divide", OMNI_DOUBLE, { 10.0, 1.0 }, { 4.0, 0.0 });
    ASSERT_NE(divide, nullptr);
    auto *divideValues = static_cast<Vector<double> *>(divide.get());
    EXPECT_DOUBLE_EQ(divideValues->GetValue(0), 2.5);
    EXPECT_TRUE(divide->IsNull(1));
}

TEST_F(TryArithmeticTest, Decimal128OverflowBecomesNull)
{
    constexpr int32_t rowSize = 1;
    auto inputType = std::make_shared<Decimal128DataType>(38, 0);
    auto *left = static_cast<Vector<Decimal128> *>(VectorHelper::CreateFlatVector(OMNI_DECIMAL128, rowSize));
    auto *right = static_cast<Vector<Decimal128> *>(VectorHelper::CreateFlatVector(OMNI_DECIMAL128, rowSize));
    left->SetValue(0, Decimal128("99999999999999999999999999999999999999"));
    left->SetNotNull(0);
    right->SetValue(0, Decimal128("1"));
    right->SetNotNull(0);
    VectorHelper::SetVectorDataType(left, inputType.get());
    VectorHelper::SetVectorDataType(right, inputType.get());

    auto signature = std::make_shared<FunctionSignature>("try_add",
        std::vector<DataTypeId> { OMNI_DECIMAL128, OMNI_DECIMAL128 }, OMNI_DECIMAL128);
    auto function = VectorFunction::Find(signature);
    ASSERT_NE(function, nullptr);

    ExecutionContext context;
    context.SetResultRowSize(rowSize);
    std::stack<BaseVector *> arguments;
    arguments.push(left);
    arguments.push(right);
    BaseVector *rawResult = nullptr;
    function->Apply(arguments, inputType, rawResult, &context);
    std::unique_ptr<BaseVector> result(rawResult);

    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(result->IsNull(0));
}

TEST_F(TryArithmeticTest, ExprEvalPreservesDecimalTypeMetadata)
{
    constexpr int32_t rowSize = 2;
    auto inputType = std::make_shared<Decimal64DataType>(18, 0);
    auto outputType = std::make_shared<Decimal128DataType>(19, 0);
    auto *left = static_cast<Vector<int64_t> *>(VectorHelper::CreateFlatVector(OMNI_DECIMAL64, rowSize));
    auto *right = static_cast<Vector<int64_t> *>(VectorHelper::CreateFlatVector(OMNI_DECIMAL64, rowSize));
    const int64_t leftValues[rowSize] = { 999999999999999999LL, 10 };
    const int64_t rightValues[rowSize] = { 1, 20 };
    for (int32_t row = 0; row < rowSize; ++row) {
        left->SetValue(row, leftValues[row]);
        left->SetNotNull(row);
        right->SetValue(row, rightValues[row]);
        right->SetNotNull(row);
    }
    VectorHelper::SetVectorDataType(left, inputType.get());
    VectorHelper::SetVectorDataType(right, inputType.get());

    auto batch = std::make_unique<VectorBatch>(rowSize);
    batch->Append(left);
    batch->Append(right);
    auto expression = std::make_unique<FuncExpr>("try_add",
        std::vector<Expr *> { new FieldExpr(0, inputType), new FieldExpr(1, inputType) }, outputType);
    ASSERT_NE(expression->vectorFunction, nullptr);

    ExecutionContext context;
    context.SetResultRowSize(rowSize);
    ExprEval evaluator(batch.get(), &context);
    evaluator.Visit(*expression);
    std::unique_ptr<BaseVector> result(evaluator.GetResult());

    ASSERT_NE(result, nullptr);
    auto *values = static_cast<Vector<Decimal128> *>(result.get());
    EXPECT_EQ(values->GetValue(0), Decimal128("1000000000000000000"));
    EXPECT_EQ(values->GetValue(1), Decimal128("30"));
    auto *resultType = dynamic_cast<DecimalDataType *>(result->GetDataType().get());
    ASSERT_NE(resultType, nullptr);
    EXPECT_EQ(resultType->GetPrecision(), 19);
    EXPECT_EQ(resultType->GetScale(), 0);
}

} // namespace
