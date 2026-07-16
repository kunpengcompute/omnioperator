/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: IS_DECIMAL function unit tests.
 *   IS_DECIMAL(string) -> boolean.
 *   True iff the string can be parsed by Java Double.parseDouble (integer/long/double);
 *   NULL/empty input -> false (output NOT null); non-null numeric input -> true.
 */

#include <gtest/gtest.h>
#include <stack>
#include <string>
#include <vector>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/VectorFunction.h"
#include "codegen/func_signature.h"
#include "vector/vector_helper.h"
#include "vector/vector.h"
#include "omni_exception.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::op;
using namespace omniruntime::type;
using namespace omniruntime::codegen;
using namespace omniruntime::TestUtil;

class IsDecimalTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        RegisterFunctions::RegisterAllFunctions("");
    }
};

::testing::Environment* const is_decimal_test_env =
    ::testing::AddGlobalTestEnvironment(new IsDecimalTestEnvironment);

namespace {

BaseVector* CreateStringVector(const std::vector<std::string>& values) {
    BaseVector* vec = VectorHelper::CreateStringVector(values.size());
    vec->SetIsField(true);
    auto* typedVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(vec);
    if (typedVec == nullptr) {
        ADD_FAILURE() << "CreateStringVector: vector is not string type";
        delete vec;
        return nullptr;
    }
    for (size_t i = 0; i < values.size(); ++i) {
        typedVec->SetValue(i, std::string_view(values[i]));
    }
    return vec;
}

BaseVector* CreateIntVector(const std::vector<int32_t>& values) {
    BaseVector* vec = VectorHelper::CreateFlatVector(OMNI_INT, values.size());
    auto* typedVec = static_cast<Vector<int32_t>*>(vec);
    for (size_t i = 0; i < values.size(); ++i) {
        typedVec->SetValue(i, values[i]);
    }
    return vec;
}

BaseVector* ExecuteIsDecimal(BaseVector* inputVec, DataTypeId inputTypeId) {
    if (inputVec == nullptr) {
        ADD_FAILURE() << "ExecuteIsDecimal: input vector is null";
        return nullptr;
    }
    auto signature = std::make_shared<FunctionSignature>("is_decimal",
        std::vector<DataTypeId>{inputTypeId}, OMNI_BOOLEAN);
    auto function = VectorFunction::Find(signature);
    if (function == nullptr) {
        ADD_FAILURE() << "is_decimal not found for type " << static_cast<int>(inputTypeId);
        return nullptr;
    }

    auto outputType = std::make_shared<DataType>(OMNI_BOOLEAN);
    ExecutionContext context;
    context.SetResultRowSize(inputVec->GetSize());
    std::stack<BaseVector*> args;
    args.push(inputVec);

    BaseVector* result = nullptr;
    function->Apply(args, outputType, result, &context);
    return result;
}

void ExpectAllNotNull(BaseVector* result, int rowSize) {
    ASSERT_NE(result, nullptr);
    for (int i = 0; i < rowSize; ++i) {
        EXPECT_FALSE(result->IsNull(i)) << "Row " << i << " must be non-NULL (IS_DECIMAL never returns NULL)";
    }
}

void ValidateBool(BaseVector* result, const std::vector<bool>& expected) {
    ASSERT_NE(result, nullptr);
    auto* resultVec = dynamic_cast<Vector<bool>*>(result);
    ASSERT_NE(resultVec, nullptr) << "Result vector is not boolean type";
    for (size_t i = 0; i < expected.size(); ++i) {
        EXPECT_EQ(resultVec->GetValue(i), expected[i])
            << "Row " << i << " expected=" << (expected[i] ? "true" : "false")
            << " actual=" << (resultVec->GetValue(i) ? "true" : "false");
    }
}

}  // namespace

TEST(IsDecimalTest, Integers) {
    std::vector<std::string> in = {"123", "-123", "+7", "0", "000"};
    std::vector<bool> expected = {true, true, true, true, true};
    BaseVector* input = CreateStringVector(in);
    BaseVector* result = ExecuteIsDecimal(input, OMNI_VARCHAR);
    ValidateBool(result, expected);
    ExpectAllNotNull(result, in.size());
    delete result;
}

TEST(IsDecimalTest, Decimals) {
    std::vector<std::string> in = {"3.14", ".5", "5.", "-0.5", "+2.0"};
    std::vector<bool> expected = {true, true, true, true, true};
    BaseVector* input = CreateStringVector(in);
    BaseVector* result = ExecuteIsDecimal(input, OMNI_VARCHAR);
    ValidateBool(result, expected);
    ExpectAllNotNull(result, in.size());
    delete result;
}

TEST(IsDecimalTest, ScientificNotation) {
    std::vector<std::string> in = {"1e10", "2.5E-3", "1E5", "-3e+2", ".5e2"};
    std::vector<bool> expected = {true, true, true, true, true};
    BaseVector* input = CreateStringVector(in);
    BaseVector* result = ExecuteIsDecimal(input, OMNI_VARCHAR);
    ValidateBool(result, expected);
    ExpectAllNotNull(result, in.size());
    delete result;
}

// Java Double.parseDouble trims leading/trailing ASCII whitespace.
TEST(IsDecimalTest, SurroundingWhitespace) {
    std::vector<std::string> in = {"  12  ", "\t3\n", " -4.5 "};
    std::vector<bool> expected = {true, true, true};
    BaseVector* input = CreateStringVector(in);
    BaseVector* result = ExecuteIsDecimal(input, OMNI_VARCHAR);
    ValidateBool(result, expected);
    ExpectAllNotNull(result, in.size());
    delete result;
}

// Special values accepted by Double.parseDouble (case-insensitive).
TEST(IsDecimalTest, SpecialValues) {
    std::vector<std::string> in = {"NaN", "Infinity", "-Infinity", "+Infinity", "nan", "INFINITY"};
    std::vector<bool> expected = {true, true, true, true, true, true};
    BaseVector* input = CreateStringVector(in);
    BaseVector* result = ExecuteIsDecimal(input, OMNI_VARCHAR);
    ValidateBool(result, expected);
    ExpectAllNotNull(result, in.size());
    delete result;
}

TEST(IsDecimalTest, InvalidStrings) {
    std::vector<std::string> in = {"12a", "abc", "1.2.3", "1e", "+", ".", "e5", "--1"};
    std::vector<bool> expected = {false, false, false, false, false, false, false, false};
    BaseVector* input = CreateStringVector(in);
    BaseVector* result = ExecuteIsDecimal(input, OMNI_VARCHAR);
    ValidateBool(result, expected);
    ExpectAllNotNull(result, in.size());
    delete result;
}

TEST(IsDecimalTest, EmptyAndWhitespaceOnly) {
    std::vector<std::string> in = {"", "   ", "\t"};
    std::vector<bool> expected = {false, false, false};
    BaseVector* input = CreateStringVector(in);
    BaseVector* result = ExecuteIsDecimal(input, OMNI_VARCHAR);
    ValidateBool(result, expected);
    ExpectAllNotNull(result, in.size());
    delete result;
}

// By design the grammar does NOT accept Java 'd'/'f' suffixes or hex floats.
TEST(IsDecimalTest, JavaSuffixesAndHexNotAccepted) {
    std::vector<std::string> in = {"1d", "2.5f", "0x1p3", "0x10", "1D", "3F"};
    std::vector<bool> expected = {false, false, false, false, false, false};
    BaseVector* input = CreateStringVector(in);
    BaseVector* result = ExecuteIsDecimal(input, OMNI_VARCHAR);
    ValidateBool(result, expected);
    ExpectAllNotNull(result, in.size());
    delete result;
}

// NULL input -> false (output is NOT null), matching Flink isDecimal(null)=false.
TEST(IsDecimalTest, NullInputBecomesFalseNotNull) {
    std::vector<std::string> in = {"123", "999", "3.14"};
    BaseVector* input = CreateStringVector(in);
    input->SetNull(1);
    BaseVector* result = ExecuteIsDecimal(input, OMNI_VARCHAR);

    ASSERT_NE(result, nullptr);
    auto* resultVec = dynamic_cast<Vector<bool>*>(result);
    ASSERT_NE(resultVec, nullptr);
    ExpectAllNotNull(result, in.size());
    EXPECT_TRUE(resultVec->GetValue(0));
    EXPECT_FALSE(resultVec->GetValue(1));  // NULL input -> false
    EXPECT_TRUE(resultVec->GetValue(2));
    delete result;
}

TEST(IsDecimalTest, ConstEncoding) {
    BaseVector* input = new ConstVector<std::string_view>(std::string_view("3.14"), OMNI_VARCHAR, 3);
    BaseVector* result = ExecuteIsDecimal(input, OMNI_VARCHAR);
    ASSERT_NE(result, nullptr);
    auto* resultVec = dynamic_cast<Vector<bool>*>(result);
    ASSERT_NE(resultVec, nullptr);
    for (int i = 0; i < 3; ++i) {
        EXPECT_FALSE(result->IsNull(i));
        EXPECT_TRUE(resultVec->GetValue(i));
    }
    delete result;
}

// Non-null numeric input -> true; NULL numeric row -> false. Output never NULL.
TEST(IsDecimalTest, NumericInput) {
    std::vector<int32_t> in = {123, -7, 0};
    BaseVector* input = CreateIntVector(in);
    input->SetNull(1);
    BaseVector* result = ExecuteIsDecimal(input, OMNI_INT);

    ASSERT_NE(result, nullptr);
    auto* resultVec = dynamic_cast<Vector<bool>*>(result);
    ASSERT_NE(resultVec, nullptr);
    ExpectAllNotNull(result, in.size());
    EXPECT_TRUE(resultVec->GetValue(0));
    EXPECT_FALSE(resultVec->GetValue(1));  // NULL numeric -> false
    EXPECT_TRUE(resultVec->GetValue(2));
    delete result;
}
