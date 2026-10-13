/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Unit tests for string function repeat
 */

#include <gtest/gtest.h>
#include <string>
#include <string_view>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/ExprEval.h"
#include "expression/expressions.h"
#include "vectorization/registration/SimpleFunctionRegistry.h"
#include "vector/vector_helper.h"
#include "type/data_type.h"
#include "codegen/func_signature.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::codegen;

class StringRepeatTest : public ::testing::Test {
protected:
    void SetUp() override {
        RegisterFunctions::Register();
    }
};

// ---- repeat(string, n) with OMNI_VARCHAR, OMNI_INT ----
TEST_F(StringRepeatTest, RepeatVarcharIntBasic) {
    std::string s0 = "hh", s1 = "abab", s2 = "", s3 = "x";
    constexpr int rowSize = 4;
    vec::BaseVector* strVec = VectorHelper::CreateStringVector(rowSize);
    strVec->SetIsField(true);
    auto* strV = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(strVec);
    ASSERT_NE(strV, nullptr);
    std::string_view sv0(s0), sv1(s1), sv2(s2), sv3(s3);
    strV->SetValue(0, sv0);
    strV->SetValue(1, sv1);
    strV->SetValue(2, sv2);
    strV->SetValue(3, sv3);

    vec::BaseVector* countVec = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, rowSize);
    countVec->SetIsField(true);
    auto* intV = dynamic_cast<Vector<int32_t>*>(countVec);
    ASSERT_NE(intV, nullptr);
    intV->SetValue(0, 2);
    intV->SetValue(1, 0);
    intV->SetValue(2, 2);
    intV->SetValue(3, 4);

    auto signature = std::make_shared<FunctionSignature>("repeat",
        std::vector<DataTypeId>{OMNI_VARCHAR, OMNI_INT}, OMNI_VARCHAR);
    auto function = VectorFunction::Find(signature);
    ASSERT_NE(function, nullptr);

    vec::BaseVector* resultVector = nullptr;
    auto varcharType = std::make_shared<DataType>(OMNI_VARCHAR);
    ExecutionContext context(4096, nullptr);
    context.SetResultRowSize(rowSize);
    std::stack<vec::BaseVector*> args;
    args.push(strVec);
    args.push(countVec);

    ASSERT_NO_THROW(function->Apply(args, varcharType, resultVector, &context));

    auto* outStrVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(resultVector);
    ASSERT_NE(outStrVec, nullptr);
    EXPECT_EQ(std::string(outStrVec->GetValue(0)), "hhhh");
    EXPECT_EQ(std::string(outStrVec->GetValue(1)), "");
    EXPECT_EQ(std::string(outStrVec->GetValue(2)), "");
    EXPECT_EQ(std::string(outStrVec->GetValue(3)), "xxxx");

    delete resultVector;
    delete countVec;
    delete strVec;
}

// ---- repeat(string, n) with n <= 0 ----
TEST_F(StringRepeatTest, RepeatZeroOrNegative) {
    std::string s = "ab";
    constexpr int rowSize = 3;
    vec::BaseVector* strVec = VectorHelper::CreateStringVector(rowSize);
    strVec->SetIsField(true);
    auto* strV = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(strVec);
    ASSERT_NE(strV, nullptr);
    std::string_view sv(s);
    strV->SetValue(0, sv);
    strV->SetValue(1, sv);
    strV->SetValue(2, sv);

    vec::BaseVector* countVec = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, rowSize);
    countVec->SetIsField(true);
    auto* intV = dynamic_cast<Vector<int32_t>*>(countVec);
    ASSERT_NE(intV, nullptr);
    intV->SetValue(0, 0);
    intV->SetValue(1, -1);
    intV->SetValue(2, 1);

    auto signature = std::make_shared<FunctionSignature>("repeat",
        std::vector<DataTypeId>{OMNI_VARCHAR, OMNI_INT}, OMNI_VARCHAR);
    auto function = VectorFunction::Find(signature);
    ASSERT_NE(function, nullptr);

    vec::BaseVector* resultVector = nullptr;
    auto varcharType = std::make_shared<DataType>(OMNI_VARCHAR);
    ExecutionContext context(4096, nullptr);
    context.SetResultRowSize(rowSize);
    std::stack<vec::BaseVector*> args;
    args.push(strVec);
    args.push(countVec);

    ASSERT_NO_THROW(function->Apply(args, varcharType, resultVector, &context));

    auto* outStrVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(resultVector);
    ASSERT_NE(outStrVec, nullptr);
    EXPECT_EQ(std::string(outStrVec->GetValue(0)), "");
    EXPECT_EQ(std::string(outStrVec->GetValue(1)), "");
    EXPECT_EQ(std::string(outStrVec->GetValue(2)), "ab");

    delete resultVector;
    delete countVec;
    delete strVec;
}

// ---- repeat with OMNI_LONG (bigint) ----
TEST_F(StringRepeatTest, RepeatVarcharLong) {
    std::string s = "12";
    constexpr int rowSize = 2;
    vec::BaseVector* strVec = VectorHelper::CreateStringVector(rowSize);
    strVec->SetIsField(true);
    auto* strV = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(strVec);
    ASSERT_NE(strV, nullptr);
    std::string_view sv(s);
    strV->SetValue(0, sv);
    strV->SetValue(1, sv);

    vec::BaseVector* countVec = VectorHelper::CreateFlatVector(nullptr, OMNI_LONG, rowSize);
    countVec->SetIsField(true);
    auto* longV = dynamic_cast<Vector<int64_t>*>(countVec);
    ASSERT_NE(longV, nullptr);
    longV->SetValue(0, 3);
    longV->SetValue(1, 1);

    auto signature = std::make_shared<FunctionSignature>("repeat",
        std::vector<DataTypeId>{OMNI_VARCHAR, OMNI_LONG}, OMNI_VARCHAR);
    auto function = VectorFunction::Find(signature);
    ASSERT_NE(function, nullptr);

    vec::BaseVector* resultVector = nullptr;
    auto varcharType = std::make_shared<DataType>(OMNI_VARCHAR);
    ExecutionContext context(4096, nullptr);
    context.SetResultRowSize(rowSize);
    std::stack<vec::BaseVector*> args;
    args.push(strVec);
    args.push(countVec);

    ASSERT_NO_THROW(function->Apply(args, varcharType, resultVector, &context));

    auto* outStrVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(resultVector);
    ASSERT_NE(outStrVec, nullptr);
    EXPECT_EQ(std::string(outStrVec->GetValue(0)), "121212");
    EXPECT_EQ(std::string(outStrVec->GetValue(1)), "12");

    delete resultVector;
    delete countVec;
    delete strVec;
}

// ---- repeat UTF-8 string ----
TEST_F(StringRepeatTest, RepeatUtf8) {
    std::string s = "123\u6570";  // "123" + one Chinese char
    constexpr int rowSize = 1;
    vec::BaseVector* strVec = VectorHelper::CreateStringVector(rowSize);
    strVec->SetIsField(true);
    auto* strV = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(strVec);
    ASSERT_NE(strV, nullptr);
    std::string_view sv(s);
    strV->SetValue(0, sv);

    vec::BaseVector* countVec = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, rowSize);
    countVec->SetIsField(true);
    auto* intV = dynamic_cast<Vector<int32_t>*>(countVec);
    ASSERT_NE(intV, nullptr);
    intV->SetValue(0, 2);

    auto signature = std::make_shared<FunctionSignature>("repeat",
        std::vector<DataTypeId>{OMNI_VARCHAR, OMNI_INT}, OMNI_VARCHAR);
    auto function = VectorFunction::Find(signature);
    ASSERT_NE(function, nullptr);

    vec::BaseVector* resultVector = nullptr;
    auto varcharType = std::make_shared<DataType>(OMNI_VARCHAR);
    ExecutionContext context(4096, nullptr);
    context.SetResultRowSize(rowSize);
    std::stack<vec::BaseVector*> args;
    args.push(strVec);
    args.push(countVec);

    ASSERT_NO_THROW(function->Apply(args, varcharType, resultVector, &context));

    auto* outStrVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(resultVector);
    ASSERT_NE(outStrVec, nullptr);
    EXPECT_EQ(std::string(outStrVec->GetValue(0)), "123\u6570123\u6570");

    delete resultVector;
    delete countVec;
    delete strVec;
}
