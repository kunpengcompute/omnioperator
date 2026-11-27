/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Unit tests for conv function (base conversion for strings)
 */

#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <optional>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/ExprEval.h"
#include "vectorization/functions/ConvFunction.h"
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
using namespace omniruntime::TestUtil;

class ConvTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const conv_test_env = ::testing::AddGlobalTestEnvironment(new ConvTestEnvironment);

TEST(ConvFunctionTest, DecimalToBinary) {
    ConvFunction<std::string_view> func;
    std::string result;

    std::string_view input = "4";
    EXPECT_TRUE(func.call(result, input, 10, 2));
    EXPECT_EQ(result, "100");
}

TEST(ConvFunctionTest, BinaryToDecimal) {
    ConvFunction<std::string_view> func;
    std::string result;

    std::string_view input = "110";
    EXPECT_TRUE(func.call(result, input, 2, 10));
    EXPECT_EQ(result, "6");
}

TEST(ConvFunctionTest, DecimalToHex) {
    ConvFunction<std::string_view> func;
    std::string result;

    std::string_view input = "15";
    EXPECT_TRUE(func.call(result, input, 10, 16));
    EXPECT_EQ(result, "F");
}

TEST(ConvFunctionTest, DecimalToSignedHex) {
    ConvFunction<std::string_view> func;
    std::string result;

    std::string_view input = "15";
    EXPECT_TRUE(func.call(result, input, 10, -16));
    EXPECT_EQ(result, "F");
}

TEST(ConvFunctionTest, Base36ToHex) {
    ConvFunction<std::string_view> func;
    std::string result;

    std::string_view input = "big";
    EXPECT_TRUE(func.call(result, input, 36, 16));
    EXPECT_EQ(result, "3A48");
}

TEST(ConvFunctionTest, NegativeDecimalToSignedHex) {
    ConvFunction<std::string_view> func;
    std::string result;

    std::string_view input = "-15";
    EXPECT_TRUE(func.call(result, input, 10, -16));
    EXPECT_EQ(result, "-F");
}

TEST(ConvFunctionTest, NegativeHexToSignedDecimal) {
    ConvFunction<std::string_view> func;
    std::string result;

    std::string_view input = "-10";
    EXPECT_TRUE(func.call(result, input, 16, -10));
    EXPECT_EQ(result, "-16");
}

TEST(ConvFunctionTest, OverflowNegativeToBinary) {
    ConvFunction<std::string_view> func;
    std::string result;

    std::string_view input1 = "-9223372036854775809";
    EXPECT_TRUE(func.call(result, input1, 10, -2));
    EXPECT_EQ(result, "-111111111111111111111111111111111111111111111111111111111111111");

    std::string_view input2 = "-9223372036854775808";
    EXPECT_TRUE(func.call(result, input2, 10, -2));
    EXPECT_EQ(result, "-1000000000000000000000000000000000000000000000000000000000000000");
 
    std::string_view input3 = "9223372036854775808";
    EXPECT_TRUE(func.call(result, input3, 10, -2));
    EXPECT_EQ(result, "-1000000000000000000000000000000000000000000000000000000000000000");
}

TEST(ConvFunctionTest, OverflowHexToBinary) {
    ConvFunction<std::string_view> func;
    std::string result;

    std::string_view input = "8000000000000000";
    EXPECT_TRUE(func.call(result, input, 16, -2));
    EXPECT_EQ(result, "-1000000000000000000000000000000000000000000000000000000000000000");
}

TEST(ConvFunctionTest, NegativeOneToHex) {
    ConvFunction<std::string_view> func;
    std::string result;

    std::string_view input = "-1";
    EXPECT_TRUE(func.call(result, input, 10, 16));
    EXPECT_EQ(result, "FFFFFFFFFFFFFFFF");
}

TEST(ConvFunctionTest, MaxHexConversions) {
    ConvFunction<std::string_view> func;
    std::string result;

    std::string_view input1 = "FFFFFFFFFFFFFFFF";
    EXPECT_TRUE(func.call(result, input1, 16, -10));
    EXPECT_EQ(result, "-1");
 
    std::string_view input2 = "-FFFFFFFFFFFFFFFF";
    EXPECT_TRUE(func.call(result, input2, 16, -10));
    EXPECT_EQ(result, "-1");
 
    std::string_view input3 = "-FFFFFFFFFFFFFFFF";
    EXPECT_TRUE(func.call(result, input3, 16, 10));
    EXPECT_EQ(result, "18446744073709551615");
}

TEST(ConvFunctionTest, NegativeFifteenToHexUnsigned) {
    ConvFunction<std::string_view> func;
    std::string result;

    std::string_view input = "-15";
    EXPECT_TRUE(func.call(result, input, 10, 16));
    EXPECT_EQ(result, "FFFFFFFFFFFFFFF1");
}

TEST(ConvFunctionTest, LargeBase36Overflow) {
    ConvFunction<std::string_view> func;
    std::string result;

    std::string_view input = "9223372036854775807";
    EXPECT_TRUE(func.call(result, input, 36, 16));
    EXPECT_EQ(result, "FFFFFFFFFFFFFFFF");
}

TEST(ConvFunctionTest, LeadingAndTrailingSpaces) {
    ConvFunction<std::string_view> func;
    std::string result;

    std::string_view input1 = "15 ";
    EXPECT_TRUE(func.call(result, input1, 10, 16));
    EXPECT_EQ(result, "F");

    std::string_view input2 = " 15 ";
    EXPECT_TRUE(func.call(result, input2, 10, 16));
    EXPECT_EQ(result, "F");
}

TEST(ConvFunctionTest, InvalidCharacters) {
    ConvFunction<std::string_view> func;
    std::string result;

    std::string_view input1 = "11abc";
    EXPECT_TRUE(func.call(result, input1, 10, 16));
    EXPECT_EQ(result, "B");

    std::string_view input2 = "FH";
    EXPECT_TRUE(func.call(result, input2, 16, 10));
    EXPECT_EQ(result, "15");

    std::string_view input3 = "11abc";
    EXPECT_TRUE(func.call(result, input3, 10, 10));
    EXPECT_EQ(result, "11");

    std::string_view input4 = "FH";
    EXPECT_TRUE(func.call(result, input4, 16, 16));
    EXPECT_EQ(result, "F");

    std::string_view input5 = "HF";
    EXPECT_TRUE(func.call(result, input5, 16, 10));
    EXPECT_EQ(result, "0");

    std::string_view input6 = "2345";
    EXPECT_TRUE(func.call(result, input6, 2, 10));
    EXPECT_EQ(result, "0");
}

TEST(ConvFunctionTest, NegativeSymbolOnly) {
    ConvFunction<std::string_view> func;
    std::string result;

    std::string_view input = "-";
    EXPECT_TRUE(func.call(result, input, 10, 16));
    EXPECT_EQ(result, "0");
}

TEST(ConvFunctionTest, NullResults) {
    ConvFunction<std::string_view> func;
    std::string result;

    std::string_view input1 = "";
    EXPECT_FALSE(func.call(result, input1, 10, 16));

    std::string_view input2 = " ";
    EXPECT_FALSE(func.call(result, input2, 10, 16));
}

TEST(ConvFunctionTest, NullInputHandling) {
    ConvFunction<std::string_view> func;
    std::string result;

    EXPECT_FALSE(func.callNullable(result, nullptr, nullptr, nullptr));

    std::string_view input = "15";
    int32_t toBase = 16;
    EXPECT_FALSE(func.callNullable(result, &input, nullptr, &toBase));

    int32_t fromBase = 10;
    EXPECT_FALSE(func.callNullable(result, &input, &fromBase, nullptr));
}

TEST(ConvFunctionTest, ZeroConversion) {
    ConvFunction<std::string_view> func;
    std::string result;

    std::string_view input = "0";
    EXPECT_TRUE(func.call(result, input, 10, 16));
    EXPECT_EQ(result, "0");

    EXPECT_TRUE(func.call(result, input, 10, 2));
    EXPECT_EQ(result, "0");

    EXPECT_TRUE(func.call(result, input, 2, 10));
    EXPECT_EQ(result, "0");
}

TEST(ConvFunctionTest, VectorFunctionApply) {

    constexpr int rowSize = 4;

    vec::BaseVector* inputVec = VectorHelper::CreateStringVector(rowSize);
    inputVec->SetIsField(true);
    auto* strVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(inputVec);
    ASSERT_NE(strVec, nullptr);
    std::string s0 = "4", s1 = "110", s2 = "15", s3 = "big";
    std::string_view sv0(s0), sv1(s1), sv2(s2), sv3(s3);
    strVec->SetValue(0, sv0);
    strVec->SetValue(1, sv1);
    strVec->SetValue(2, sv2);
    strVec->SetValue(3, sv3);

    vec::BaseVector* fromBaseVec = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, rowSize);
    fromBaseVec->SetIsField(true);
    auto* fromBaseVector = static_cast<Vector<int32_t>*>(fromBaseVec);
    fromBaseVector->SetValue(0, 10);
    fromBaseVector->SetValue(1, 2);  
    fromBaseVector->SetValue(2, 10);  
    fromBaseVector->SetValue(3, 36); 
    for (int32_t i = 0; i < rowSize; ++i) fromBaseVector->SetNotNull(i);

    vec::BaseVector* toBaseVec = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, rowSize);
    toBaseVec->SetIsField(true);
    auto* toBaseVector = static_cast<Vector<int32_t>*>(toBaseVec);
    toBaseVector->SetValue(0, 2);   
    toBaseVector->SetValue(1, 10);  
    toBaseVector->SetValue(2, 16);  
    toBaseVector->SetValue(3, 16);  
    for (int32_t i = 0; i < rowSize; ++i) toBaseVector->SetNotNull(i);

    auto signature = std::make_shared<FunctionSignature>("conv",
        std::vector<DataTypeId>{OMNI_VARCHAR, OMNI_INT, OMNI_INT}, OMNI_VARCHAR);
    auto function = VectorFunction::Find(signature);
    ASSERT_NE(function, nullptr) << "conv function not found in VectorFunction registry";

    vec::BaseVector* resultVector = nullptr;
    auto varcharType = std::make_shared<DataType>(OMNI_VARCHAR);
    ExecutionContext context(4096, nullptr);
    context.SetResultRowSize(rowSize);
    std::stack<vec::BaseVector*> args;
    args.push(inputVec);
    args.push(fromBaseVec);
    args.push(toBaseVec);

    ASSERT_NO_THROW(function->Apply(args, varcharType, resultVector, &context));

    auto* outStrVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(resultVector);
    ASSERT_NE(outStrVec, nullptr);

    EXPECT_EQ(std::string(outStrVec->GetValue(0)), "100");   
 
    EXPECT_EQ(std::string(outStrVec->GetValue(1)), "6");     
 
    EXPECT_EQ(std::string(outStrVec->GetValue(2)), "F");    
 
    EXPECT_EQ(std::string(outStrVec->GetValue(3)), "3A48"); 
  
    delete resultVector;
    delete inputVec;
    delete fromBaseVec;
    delete toBaseVec;
}

TEST(ConvFunctionTest, ConvExprEval) {

    constexpr int rowSize = 3;

    auto returnType = std::make_shared<DataType>(OMNI_VARCHAR);
    auto strType = std::make_shared<DataType>(OMNI_VARCHAR);
    auto intType = std::make_shared<DataType>(OMNI_INT);

    std::vector<expressions::Expr*> args = {
        new expressions::FieldExpr(0, strType),
        new expressions::FieldExpr(1, intType),
        new expressions::FieldExpr(2, intType)
    };
    auto funcExpr = new expressions::FuncExpr("conv", args, returnType);

    vec::BaseVector* strVec = VectorHelper::CreateStringVector(rowSize);
    auto* inputStrVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(strVec);
    ASSERT_NE(inputStrVec, nullptr);
    std::string s0 = "4", s1 = "15", s2 = "big";
    std::string_view sv0(s0), sv1(s1), sv2(s2);
    inputStrVec->SetValue(0, sv0);
    inputStrVec->SetValue(1, sv1);
    inputStrVec->SetValue(2, sv2);

    vec::BaseVector* fromVec = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, rowSize);
    auto* fromVector = static_cast<Vector<int32_t>*>(fromVec);
    fromVector->SetValue(0, 10);
    fromVector->SetValue(1, 10);
    fromVector->SetValue(2, 36);
    for (int32_t i = 0; i < rowSize; ++i) fromVector->SetNotNull(i);

    vec::BaseVector* toVec = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, rowSize);
    auto* toVector = static_cast<Vector<int32_t>*>(toVec);
    toVector->SetValue(0, 2);
    toVector->SetValue(1, 16);
    toVector->SetValue(2, 16);
    for (int32_t i = 0; i < rowSize; ++i) toVector->SetNotNull(i);

    VectorBatch input(rowSize);
    input.Append(strVec);
    input.Append(fromVec);
    input.Append(toVec);

    VectorHelper::PrintVecBatch(&input);

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(&input, context);
    e.Visit(*funcExpr);
    auto result = e.GetResult();
    VectorBatch vectorBatch(rowSize);
    vectorBatch.Append(result);
    VectorHelper::PrintVecBatch(&vectorBatch);

    auto* resultStrVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(result);
    ASSERT_NE(resultStrVec, nullptr);

    EXPECT_EQ(std::string(resultStrVec->GetValue(0)), "100");  
    EXPECT_EQ(std::string(resultStrVec->GetValue(1)), "F");    
    EXPECT_EQ(std::string(resultStrVec->GetValue(2)), "3A48"); 

    delete funcExpr;
    delete context;
}
