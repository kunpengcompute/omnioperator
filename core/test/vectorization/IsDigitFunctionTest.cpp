/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Unit tests for is_digit string predicate function
 */

#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include <vector>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/ExprEval.h"
#include "vectorization/VectorFunction.h"
#include "expression/expressions.h"
#include "type/data_type.h"
#include "vector/vector_helper.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::op;
using namespace omniruntime::expressions;
using namespace omniruntime::TestUtil;

class IsDigitTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};
::testing::Environment* const isDigitTestEnv =
    ::testing::AddGlobalTestEnvironment(new IsDigitTestEnvironment);

// ============================================================================
// Test: Basic functionality — digit strings return true
// ============================================================================
TEST(IsDigitFunctionTest, BasicDigits) {
    using VarcharVector = Vector<LargeStringContainer<std::string_view>>;

    std::vector<std::string> values = {"12345", "0", "00000", "999999999"};
    std::vector<bool> expectedResults = {true, true, true, true};
    int rowSize = values.size();

    BaseVector* inputVec = VectorHelper::CreateFlatVector(OMNI_VARCHAR, rowSize);
    auto* inputVector = dynamic_cast<VarcharVector*>(inputVec);
    for (int32_t i = 0; i < rowSize; ++i) {
        std::string_view lsv(values[i]);
        inputVector->SetValue(i, lsv);
        inputVector->SetNotNull(i);
    }

    auto signature = std::make_shared<FunctionSignature>(
        "is_digit", std::vector<DataTypeId>{OMNI_VARCHAR}, OMNI_BOOLEAN);
    auto vectorFunction = VectorFunction::Find(signature);
    ASSERT_NE(vectorFunction, nullptr) << "Function is_digit not found";

    ExecutionContext context;
    context.SetResultRowSize(rowSize);

    std::stack<BaseVector*> args;
    args.push(inputVec);
    BaseVector* result = nullptr;
    auto resultType = std::make_shared<DataType>(OMNI_BOOLEAN);
    vectorFunction->Apply(args, resultType, result, &context);

    auto* resultVector = dynamic_cast<Vector<bool>*>(result);
    ASSERT_NE(resultVector, nullptr);

    for (int32_t i = 0; i < rowSize; ++i) {
        EXPECT_FALSE(resultVector->IsNull(i)) << "Expected non-null at index " << i;
        EXPECT_EQ(resultVector->GetValue(i), expectedResults[i])
            << "Mismatch at index " << i << " input=" << values[i];
    }

    // NOTE: inputVec is created via CreateFlatVector (IsField == false), so the
    // StringVectorReader inside SimpleFunction::Apply takes ownership and frees
    // it on destruction. Do NOT delete inputVec here (would be use-after-free).
    delete result;
}

// ============================================================================
// Test: Non-digit strings return false
// ============================================================================
TEST(IsDigitFunctionTest, NonDigitStrings) {
    using VarcharVector = Vector<LargeStringContainer<std::string_view>>;

    std::vector<std::string> values = {"abc", "123a5", "hello world", "12.34", "!@#", "12 34"};
    std::vector<bool> expectedResults = {false, false, false, false, false, false};
    int rowSize = values.size();

    BaseVector* inputVec = VectorHelper::CreateFlatVector(OMNI_VARCHAR, rowSize);
    auto* inputVector = dynamic_cast<VarcharVector*>(inputVec);
    for (int32_t i = 0; i < rowSize; ++i) {
        std::string_view lsv(values[i]);
        inputVector->SetValue(i, lsv);
        inputVector->SetNotNull(i);
    }

    auto signature = std::make_shared<FunctionSignature>(
        "is_digit", std::vector<DataTypeId>{OMNI_VARCHAR}, OMNI_BOOLEAN);
    auto vectorFunction = VectorFunction::Find(signature);
    ASSERT_NE(vectorFunction, nullptr) << "Function is_digit not found";

    ExecutionContext context;
    context.SetResultRowSize(rowSize);

    std::stack<BaseVector*> args;
    args.push(inputVec);
    BaseVector* result = nullptr;
    auto resultType = std::make_shared<DataType>(OMNI_BOOLEAN);
    vectorFunction->Apply(args, resultType, result, &context);

    auto* resultVector = dynamic_cast<Vector<bool>*>(result);
    ASSERT_NE(resultVector, nullptr);

    for (int32_t i = 0; i < rowSize; ++i) {
        EXPECT_FALSE(resultVector->IsNull(i)) << "Expected non-null at index " << i;
        EXPECT_EQ(resultVector->GetValue(i), expectedResults[i])
            << "Mismatch at index " << i << " input=" << values[i];
    }

    // inputVec (CreateFlatVector, IsField == false) is freed by StringVectorReader.
    delete result;
}

// ============================================================================
// Test: Edge cases — empty string, whitespace, special characters
// ============================================================================
TEST(IsDigitFunctionTest, EdgeCases) {
    using VarcharVector = Vector<LargeStringContainer<std::string_view>>;

    std::vector<std::string> values = {"", " ", "\t", "\n", "   "};
    std::vector<bool> expectedResults = {false, false, false, false, false};
    int rowSize = values.size();

    BaseVector* inputVec = VectorHelper::CreateFlatVector(OMNI_VARCHAR, rowSize);
    auto* inputVector = dynamic_cast<VarcharVector*>(inputVec);
    for (int32_t i = 0; i < rowSize; ++i) {
        std::string_view lsv(values[i]);
        inputVector->SetValue(i, lsv);
        inputVector->SetNotNull(i);
    }

    auto signature = std::make_shared<FunctionSignature>(
        "is_digit", std::vector<DataTypeId>{OMNI_VARCHAR}, OMNI_BOOLEAN);
    auto vectorFunction = VectorFunction::Find(signature);
    ASSERT_NE(vectorFunction, nullptr) << "Function is_digit not found";

    ExecutionContext context;
    context.SetResultRowSize(rowSize);

    std::stack<BaseVector*> args;
    args.push(inputVec);
    BaseVector* result = nullptr;
    auto resultType = std::make_shared<DataType>(OMNI_BOOLEAN);
    vectorFunction->Apply(args, resultType, result, &context);

    auto* resultVector = dynamic_cast<Vector<bool>*>(result);
    ASSERT_NE(resultVector, nullptr);

    for (int32_t i = 0; i < rowSize; ++i) {
        EXPECT_FALSE(resultVector->IsNull(i)) << "Expected non-null at index " << i;
        EXPECT_EQ(resultVector->GetValue(i), expectedResults[i])
            << "Mismatch at index " << i;
    }

    // inputVec (CreateFlatVector, IsField == false) is freed by StringVectorReader.
    delete result;
}

// ============================================================================
// Test: Mixed input — digits and non-digits in same batch
// ============================================================================
TEST(IsDigitFunctionTest, MixedInput) {
    using VarcharVector = Vector<LargeStringContainer<std::string_view>>;

    std::vector<std::string> values = {"123", "abc", "456", "7a9", "0", ""};
    std::vector<bool> expectedResults = {true, false, true, false, true, false};
    int rowSize = values.size();

    BaseVector* inputVec = VectorHelper::CreateFlatVector(OMNI_VARCHAR, rowSize);
    auto* inputVector = dynamic_cast<VarcharVector*>(inputVec);
    for (int32_t i = 0; i < rowSize; ++i) {
        std::string_view lsv(values[i]);
        inputVector->SetValue(i, lsv);
        inputVector->SetNotNull(i);
    }

    auto signature = std::make_shared<FunctionSignature>(
        "is_digit", std::vector<DataTypeId>{OMNI_VARCHAR}, OMNI_BOOLEAN);
    auto vectorFunction = VectorFunction::Find(signature);
    ASSERT_NE(vectorFunction, nullptr) << "Function is_digit not found";

    ExecutionContext context;
    context.SetResultRowSize(rowSize);

    std::stack<BaseVector*> args;
    args.push(inputVec);
    BaseVector* result = nullptr;
    auto resultType = std::make_shared<DataType>(OMNI_BOOLEAN);
    vectorFunction->Apply(args, resultType, result, &context);

    auto* resultVector = dynamic_cast<Vector<bool>*>(result);
    ASSERT_NE(resultVector, nullptr);

    for (int32_t i = 0; i < rowSize; ++i) {
        EXPECT_FALSE(resultVector->IsNull(i)) << "Expected non-null at index " << i;
        EXPECT_EQ(resultVector->GetValue(i), expectedResults[i])
            << "Mismatch at index " << i << " input=" << values[i]
            << " expected=" << expectedResults[i];
    }

    // inputVec (CreateFlatVector, IsField == false) is freed by StringVectorReader.
    delete result;
}
