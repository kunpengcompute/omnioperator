/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: Concat function test
 */

#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include <vector>
#include <stack>

#include "test/util/test_util.h"
#include "vectorization/registration/Register.h"
#include "vectorization/ExprEval.h"
#include "expression/expressions.h"
#include "vectorization/registration/SimpleFunctionRegistry.h"
#include "vector/vector_helper.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::mem;
using namespace omniruntime::op;
using namespace omniruntime::expressions;
using namespace omniruntime::TestUtil;
using namespace omniruntime::type;
using namespace omniruntime::codegen;

class ConcatFunctionTestHelper {
public:
    static void SetupTestVector(int rowSize, const std::vector<std::string>& inputStrs, vec::BaseVector*& inputVec)
    {
        inputVec = VectorHelper::CreateStringVector(rowSize);
        auto *inputVector = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(inputVec);
        if (!inputVector) {
            FAIL() << "Type conversion failed for input vector";
            return;
        }
        for (int i = 0; i < rowSize; i++) {
            std::string_view inputStr(inputStrs[i]);
            inputVector->SetValue(i, inputStr);
        }
    }

    // Builds a StringView (16B fixed-width) input column. Values <= 12 bytes are stored inline;
    // longer values are stored out-of-line (prefix + heap pointer into `inputStrs`, which must outlive
    // the Apply call). ConcatFunction::Apply deletes the arg vectors, so callers must not delete them.
    static void SetupTestVectorSV(int rowSize, const std::vector<std::string>& inputStrs, vec::BaseVector*& inputVec)
    {
        auto* svVec = new Vector<StringView>(rowSize);
        for (int i = 0; i < rowSize; i++) {
            svVec->SetValue(i, StringView(inputStrs[i]));
        }
        inputVec = svVec;
    }

    static void ValidateStringViewResult(Vector<StringView>* resultVector,
                                         const std::vector<std::string>& expectedStrs,
                                         int rowSize)
    {
        if (!resultVector) {
            FAIL() << "Result vector is null";
            return;
        }

        ASSERT_EQ(resultVector->GetTypeId(), OMNI_STRING_VIEW) << "expected StringView output column";
        ASSERT_EQ(resultVector->GetSize(), rowSize)
            << "Result vector size does not match expected row size";

        for (int row = 0; row < rowSize; ++row) {
            if (resultVector->IsNull(row)) {
                continue;
            }

            const StringView &resultValue = resultVector->GetValueRef(row);
            std::string resultStr(resultValue.data(), resultValue.size());
            std::string expectedStr = expectedStrs[row];

            EXPECT_EQ(resultStr, expectedStr)
                << "Row " << row << " result does not match expected value. "
                << "Got: \"" << resultStr << "\", Expected: \"" << expectedStr << "\"";
        }
    }

    static void ValidateResult(Vector<LargeStringContainer<std::string_view>>* resultVector,
                               const std::vector<std::string>& expectedStrs,
                               int rowSize)
    {
        if (!resultVector) {
            FAIL() << "Result vector is null";
            return;
        }

        ASSERT_EQ(resultVector->GetSize(), rowSize)
            << "Result vector size does not match expected row size";

        for (int row = 0; row < rowSize; ++row) {
            if (resultVector->IsNull(row)) {
                continue;
            }

            std::string_view resultValue = resultVector->GetValue(row);
            std::string resultStr(resultValue);
            std::string expectedStr = expectedStrs[row];

            EXPECT_EQ(resultStr, expectedStr)
                << "Row " << row << " result does not match expected value. "
                << "Got: \"" << resultStr << "\", Expected: \"" << expectedStr << "\"";
        }
    }
};

TEST(ConcatTest, ConcatFunctionBasicTest) {
    int rowSize = 5;
    vec::BaseVector *inputVec1, *inputVec2;

    std::vector<std::string> inputStrs1 = {"hello", "world", "test", "foo", "bar"};
    std::vector<std::string> inputStrs2 = {" ", "!", "123", "bar", "foo"};
    std::vector<std::string> expectedStrs = {"hello ", "world!", "test123", "foobar", "barfoo"};

    ConcatFunctionTestHelper::SetupTestVector(rowSize, inputStrs1, inputVec1);
    ConcatFunctionTestHelper::SetupTestVector(rowSize, inputStrs2, inputVec2);

    auto signature = std::make_shared<FunctionSignature>("concat",
        std::vector<DataTypeId>{OMNI_VARCHAR, OMNI_VARCHAR},
        OMNI_VARCHAR);
    auto function = VectorFunction::Find(signature);
    ASSERT_NE(function, nullptr) << "Concat function not found";

    vec::BaseVector* resultVector = nullptr;
    auto varcharType = std::make_shared<DataType>(OMNI_VARCHAR);

    op::ExecutionContext context(4096, nullptr);
    context.SetResultRowSize(rowSize);
    std::stack<vec::BaseVector*> args;
    // In ExprEval, arguments are pushed in order: left arg first, right arg last (on top)
    args.push(inputVec1);  // left arg (pushed first)
    args.push(inputVec2);  // right arg (pushed last, on top)

    ASSERT_NO_THROW(function->Apply(args, varcharType, resultVector, &context))
        << "ConcatFunction.apply() threw an unexpected exception";

    auto* resultStrVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(resultVector);
    ASSERT_NE(resultStrVec, nullptr) << "Result vector type conversion failed";

    ConcatFunctionTestHelper::ValidateResult(resultStrVec, expectedStrs, rowSize);

    delete resultVector;
}

TEST(ConcatTest, ConcatFunctionThreeArgsTest) {
    // This test simulates how Gluten's UnfoldConcatStringFunc handles 3 arguments:
    // concat(a, b, c) becomes concat(a, concat(b, c))

    int rowSize = 3;
    vec::BaseVector *inputVec1, *inputVec2, *inputVec3;

    std::vector<std::string> inputStrs1 = {"a", "b", "c"};
    std::vector<std::string> inputStrs2 = {"-", ":", "|"};
    std::vector<std::string> inputStrs3 = {"1", "2", "3"};
    std::vector<std::string> expectedStrs = {"a-1", "b:2", "c|3"};

    ConcatFunctionTestHelper::SetupTestVector(rowSize, inputStrs1, inputVec1);
    ConcatFunctionTestHelper::SetupTestVector(rowSize, inputStrs2, inputVec2);
    ConcatFunctionTestHelper::SetupTestVector(rowSize, inputStrs3, inputVec3);

    auto signature = std::make_shared<FunctionSignature>("concat",
        std::vector<DataTypeId>{OMNI_VARCHAR, OMNI_VARCHAR},
        OMNI_VARCHAR);
    auto function = VectorFunction::Find(signature);
    ASSERT_NE(function, nullptr) << "Concat function not found";

    auto varcharType = std::make_shared<DataType>(OMNI_VARCHAR);

    op::ExecutionContext context(4096, nullptr);
    context.SetResultRowSize(rowSize);

    // Step 1: concat(inputVec2, inputVec3) -> intermediateResult
    // In ExprEval, arguments are pushed in order: left arg first, right arg last (on top)
    std::stack<vec::BaseVector*> args1;
    args1.push(inputVec2);  // left arg (pushed first)
    args1.push(inputVec3);  // right arg (pushed last, on top)
    vec::BaseVector* intermediateResult = nullptr;
    ASSERT_NO_THROW(function->Apply(args1, varcharType, intermediateResult, &context))
        << "First concat call threw an unexpected exception";

    // Step 2: concat(inputVec1, intermediateResult) -> finalResult
    std::stack<vec::BaseVector*> args2;
    args2.push(inputVec1);           // left arg (pushed first)
    args2.push(intermediateResult);  // right arg (pushed last, on top)
    vec::BaseVector* finalResult = nullptr;
    ASSERT_NO_THROW(function->Apply(args2, varcharType, finalResult, &context))
        << "Second concat call threw an unexpected exception";

    auto* resultStrVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(finalResult);
    ASSERT_NE(resultStrVec, nullptr) << "Result vector type conversion failed";

    ConcatFunctionTestHelper::ValidateResult(resultStrVec, expectedStrs, rowSize);

    delete finalResult;
}

TEST(ConcatTest, ConcatFunctionEmptyStringTest) {
    int rowSize = 4;
    vec::BaseVector *inputVec1, *inputVec2;

    std::vector<std::string> inputStrs1 = {"", "hello", "", "world"};
    std::vector<std::string> inputStrs2 = {"world", "", "hello", ""};
    std::vector<std::string> expectedStrs = {"world", "hello", "hello", "world"};

    ConcatFunctionTestHelper::SetupTestVector(rowSize, inputStrs1, inputVec1);
    ConcatFunctionTestHelper::SetupTestVector(rowSize, inputStrs2, inputVec2);

    auto signature = std::make_shared<FunctionSignature>("concat",
        std::vector<DataTypeId>{OMNI_VARCHAR, OMNI_VARCHAR},
        OMNI_VARCHAR);
    auto function = VectorFunction::Find(signature);
    ASSERT_NE(function, nullptr) << "Concat function not found";

    vec::BaseVector* resultVector = nullptr;
    auto varcharType = std::make_shared<DataType>(OMNI_VARCHAR);

    op::ExecutionContext context(4096, nullptr);
    context.SetResultRowSize(rowSize);
    std::stack<vec::BaseVector*> args;
    // In ExprEval, arguments are pushed in order: left arg first, right arg last (on top)
    args.push(inputVec1);  // left arg (pushed first)
    args.push(inputVec2);  // right arg (pushed last, on top)

    ASSERT_NO_THROW(function->Apply(args, varcharType, resultVector, &context))
        << "ConcatFunction.apply() threw an unexpected exception";

    auto* resultStrVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(resultVector);
    ASSERT_NE(resultStrVec, nullptr) << "Result vector type conversion failed";

    ConcatFunctionTestHelper::ValidateResult(resultStrVec, expectedStrs, rowSize);

    delete resultVector;
}

TEST(ConcatTest, ConcatFunctionNullTest) {
    int rowSize = 4;
    vec::BaseVector *inputVec1, *inputVec2;

    std::vector<std::string> inputStrs1 = {"hello", "world", "test", "foo"};
    std::vector<std::string> inputStrs2 = {" ", "!", "123", "bar"};

    ConcatFunctionTestHelper::SetupTestVector(rowSize, inputStrs1, inputVec1);
    ConcatFunctionTestHelper::SetupTestVector(rowSize, inputStrs2, inputVec2);

    // Set some NULL values
    inputVec1->SetNull(1);
    inputVec2->SetNull(2);

    auto signature = std::make_shared<FunctionSignature>("concat",
        std::vector<DataTypeId>{OMNI_VARCHAR, OMNI_VARCHAR},
        OMNI_VARCHAR);
    auto function = VectorFunction::Find(signature);
    ASSERT_NE(function, nullptr) << "Concat function not found";

    vec::BaseVector* resultVector = nullptr;
    auto varcharType = std::make_shared<DataType>(OMNI_VARCHAR);

    op::ExecutionContext context(4096, nullptr);
    context.SetResultRowSize(rowSize);
    std::stack<vec::BaseVector*> args;
    // In ExprEval, arguments are pushed in order: left arg first, right arg last (on top)
    args.push(inputVec1);  // left arg (pushed first)
    args.push(inputVec2);  // right arg (pushed last, on top)

    ASSERT_NO_THROW(function->Apply(args, varcharType, resultVector, &context))
        << "ConcatFunction.apply() threw an unexpected exception";

    auto* resultStrVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(resultVector);
    ASSERT_NE(resultStrVec, nullptr) << "Result vector type conversion failed";

    // Check NULL propagation
    EXPECT_TRUE(resultStrVec->IsNull(1)) << "Row 1 should be NULL (inputVec1 is NULL)";
    EXPECT_TRUE(resultStrVec->IsNull(2)) << "Row 2 should be NULL (inputVec2 is NULL)";
    EXPECT_FALSE(resultStrVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_FALSE(resultStrVec->IsNull(3)) << "Row 3 should not be NULL";

    // Check non-NULL results
    if (!resultStrVec->IsNull(0)) {
        std::string_view resultValue = resultStrVec->GetValue(0);
        std::string resultStr(resultValue);
        EXPECT_EQ(resultStr, "hello ") << "Row 0 result should be 'hello '";
    }

    if (!resultStrVec->IsNull(3)) {
        std::string_view resultValue = resultStrVec->GetValue(3);
        std::string resultStr(resultValue);
        EXPECT_EQ(resultStr, "foobar") << "Row 3 result should be 'foobar'";
    }

    delete resultVector;
}

TEST(ConcatTest, ConcatFunctionMultipleArgsTest) {
    // This test simulates how Gluten's UnfoldConcatStringFunc handles 4 arguments:
    // concat(a, b, c, d) becomes concat(a, concat(b, concat(c, d)))

    int rowSize = 2;
    vec::BaseVector *inputVec1, *inputVec2, *inputVec3, *inputVec4;

    std::vector<std::string> inputStrs1 = {"a", "1"};
    std::vector<std::string> inputStrs2 = {"-", ":"};
    std::vector<std::string> inputStrs3 = {"b", "2"};
    std::vector<std::string> inputStrs4 = {"-", ":"};
    std::vector<std::string> expectedStrs = {"a-b-", "1:2:"};

    ConcatFunctionTestHelper::SetupTestVector(rowSize, inputStrs1, inputVec1);
    ConcatFunctionTestHelper::SetupTestVector(rowSize, inputStrs2, inputVec2);
    ConcatFunctionTestHelper::SetupTestVector(rowSize, inputStrs3, inputVec3);
    ConcatFunctionTestHelper::SetupTestVector(rowSize, inputStrs4, inputVec4);

    auto signature = std::make_shared<FunctionSignature>("concat",
        std::vector<DataTypeId>{OMNI_VARCHAR, OMNI_VARCHAR},
        OMNI_VARCHAR);
    auto function = VectorFunction::Find(signature);
    ASSERT_NE(function, nullptr) << "Concat function not found";

    auto varcharType = std::make_shared<DataType>(OMNI_VARCHAR);

    op::ExecutionContext context(4096, nullptr);
    context.SetResultRowSize(rowSize);

    // Step 1: concat(inputVec3, inputVec4) -> result1
    // In ExprEval, arguments are pushed in order: left arg first, right arg last (on top)
    std::stack<vec::BaseVector*> args1;
    args1.push(inputVec3);  // left arg (pushed first)
    args1.push(inputVec4);  // right arg (pushed last, on top)
    vec::BaseVector* result1 = nullptr;
    ASSERT_NO_THROW(function->Apply(args1, varcharType, result1, &context))
        << "First concat call threw an unexpected exception";

    // Step 2: concat(inputVec2, result1) -> result2
    std::stack<vec::BaseVector*> args2;
    args2.push(inputVec2);  // left arg (pushed first)
    args2.push(result1);    // right arg (pushed last, on top)
    vec::BaseVector* result2 = nullptr;
    ASSERT_NO_THROW(function->Apply(args2, varcharType, result2, &context))
        << "Second concat call threw an unexpected exception";

    // Step 3: concat(inputVec1, result2) -> finalResult
    std::stack<vec::BaseVector*> args3;
    args3.push(inputVec1);  // left arg (pushed first)
    args3.push(result2);    // right arg (pushed last, on top)
    vec::BaseVector* finalResult = nullptr;
    ASSERT_NO_THROW(function->Apply(args3, varcharType, finalResult, &context))
        << "Third concat call threw an unexpected exception";

    auto* resultStrVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(finalResult);
    ASSERT_NE(resultStrVec, nullptr) << "Result vector type conversion failed";

    ConcatFunctionTestHelper::ValidateResult(resultStrVec, expectedStrs, rowSize);

    delete finalResult;
}

// ---------------- StringView (SV-in / VARCHAR-out) variants ----------------

#ifdef STRINGVIEW_ENABLE
TEST(ConcatTest, ConcatSVBothStringView) {
    int rowSize = 4;
    // mix of inline (<=12B) and non-inline (>12B) StringView values
    std::vector<std::string> inputStrs1 = {"hello", "foo", "left_side_long_", "a"};
    std::vector<std::string> inputStrs2 = {" world", "bar", "right_side_long", "b"};
    std::vector<std::string> expectedStrs = {"hello world", "foobar", "left_side_long_right_side_long", "ab"};
    vec::BaseVector *inputVec1, *inputVec2;
    ConcatFunctionTestHelper::SetupTestVectorSV(rowSize, inputStrs1, inputVec1);
    ConcatFunctionTestHelper::SetupTestVectorSV(rowSize, inputStrs2, inputVec2);
    ASSERT_EQ(inputVec1->GetTypeId(), OMNI_STRING_VIEW) << "input must be a StringView column";

    auto signature = std::make_shared<FunctionSignature>("concat",
        std::vector<DataTypeId>{OMNI_STRING_VIEW, OMNI_STRING_VIEW}, OMNI_VARCHAR);
    auto function = VectorFunction::Find(signature);
    ASSERT_NE(function, nullptr) << "concat({SV,SV}) overload not found — SV registration missing";

    vec::BaseVector* resultVector = nullptr;
    auto varcharType = std::make_shared<DataType>(OMNI_VARCHAR);
    op::ExecutionContext context(4096, nullptr);
    context.SetResultRowSize(rowSize);
    std::stack<vec::BaseVector*> args;
    args.push(inputVec1);  // left arg (pushed first)
    args.push(inputVec2);  // right arg (pushed last, on top)
    ASSERT_NO_THROW(function->Apply(args, varcharType, resultVector, &context));

    auto* resultStrVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(resultVector);
    ASSERT_NE(resultStrVec, nullptr) << "Result vector type conversion failed";
    ConcatFunctionTestHelper::ValidateResult(resultStrVec, expectedStrs, rowSize);
    delete resultVector;
}

TEST(ConcatTest, ConcatSVMixedWithVarchar) {
    // first arg is an SV column; second arg is a VARCHAR column -> {SV, VARCHAR} overload.
    // The type-aware reader handles the mixed argument types.
    int rowSize = 3;
    std::vector<std::string> inputStrs1 = {"col_value_one", "x", "prefix_"};
    std::vector<std::string> inputStrs2 = {"_suffix", "_y", "_end"};
    std::vector<std::string> expectedStrs = {"col_value_one_suffix", "x_y", "prefix__end"};
    vec::BaseVector *inputVec1, *inputVec2;
    ConcatFunctionTestHelper::SetupTestVectorSV(rowSize, inputStrs1, inputVec1);   // SV column
    ConcatFunctionTestHelper::SetupTestVector(rowSize, inputStrs2, inputVec2);     // VARCHAR column
    ASSERT_EQ(inputVec1->GetTypeId(), OMNI_STRING_VIEW) << "first arg must be a StringView column";

    auto signature = std::make_shared<FunctionSignature>("concat",
        std::vector<DataTypeId>{OMNI_STRING_VIEW, OMNI_VARCHAR}, OMNI_VARCHAR);
    auto function = VectorFunction::Find(signature);
    ASSERT_NE(function, nullptr) << "concat({SV,VARCHAR}) overload not found — SV registration missing";

    vec::BaseVector* resultVector = nullptr;
    auto varcharType = std::make_shared<DataType>(OMNI_VARCHAR);
    op::ExecutionContext context(4096, nullptr);
    context.SetResultRowSize(rowSize);
    std::stack<vec::BaseVector*> args;
    args.push(inputVec1);  // left arg (SV, pushed first)
    args.push(inputVec2);  // right arg (VARCHAR, on top)
    ASSERT_NO_THROW(function->Apply(args, varcharType, resultVector, &context));

    auto* resultStrVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(resultVector);
    ASSERT_NE(resultStrVec, nullptr) << "Result vector type conversion failed";
    ConcatFunctionTestHelper::ValidateResult(resultStrVec, expectedStrs, rowSize);
    delete resultVector;
}

TEST(ConcatTest, ConcatSVNullPropagation) {
    int rowSize = 3;
    std::vector<std::string> inputStrs1 = {"aaa", "bbb", "ccc"};
    std::vector<std::string> inputStrs2 = {"111", "222", "333"};
    vec::BaseVector *inputVec1, *inputVec2;
    ConcatFunctionTestHelper::SetupTestVectorSV(rowSize, inputStrs1, inputVec1);
    ConcatFunctionTestHelper::SetupTestVectorSV(rowSize, inputStrs2, inputVec2);
    inputVec1->SetNull(1);

    auto signature = std::make_shared<FunctionSignature>("concat",
        std::vector<DataTypeId>{OMNI_STRING_VIEW, OMNI_STRING_VIEW}, OMNI_VARCHAR);
    auto function = VectorFunction::Find(signature);
    ASSERT_NE(function, nullptr) << "concat({SV,SV}) overload not found";

    vec::BaseVector* resultVector = nullptr;
    auto varcharType = std::make_shared<DataType>(OMNI_VARCHAR);
    op::ExecutionContext context(4096, nullptr);
    context.SetResultRowSize(rowSize);
    std::stack<vec::BaseVector*> args;
    args.push(inputVec1);
    args.push(inputVec2);
    ASSERT_NO_THROW(function->Apply(args, varcharType, resultVector, &context));

    auto* resultStrVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(resultVector);
    ASSERT_NE(resultStrVec, nullptr) << "Result vector type conversion failed";
    EXPECT_EQ(std::string(resultStrVec->GetValue(0)), "aaa111");
    EXPECT_TRUE(resultStrVec->IsNull(1)) << "Row 1 should be NULL";
    EXPECT_EQ(std::string(resultStrVec->GetValue(2)), "ccc333");
    delete resultVector;
}

// ---------------- StringView (SV-in / SV-out, materialized) variants ----------------

TEST(ConcatTest, ConcatSVOutBothStringView) {
    int rowSize = 4;
    std::vector<std::string> inputStrs1 = {"hello", "foo", "left_side_long_", "a"};
    std::vector<std::string> inputStrs2 = {" world", "bar", "right_side_long", "b"};
    std::vector<std::string> expectedStrs = {"hello world", "foobar", "left_side_long_right_side_long", "ab"};
    vec::BaseVector *inputVec1, *inputVec2;
    ConcatFunctionTestHelper::SetupTestVectorSV(rowSize, inputStrs1, inputVec1);
    ConcatFunctionTestHelper::SetupTestVectorSV(rowSize, inputStrs2, inputVec2);

    auto signature = std::make_shared<FunctionSignature>("concat",
        std::vector<DataTypeId>{OMNI_STRING_VIEW, OMNI_STRING_VIEW}, OMNI_STRING_VIEW);
    auto function = VectorFunction::Find(signature);
    ASSERT_NE(function, nullptr) << "concat({SV,SV})->SV overload not found";

    vec::BaseVector* resultVector = nullptr;
    auto svType = std::make_shared<DataType>(OMNI_STRING_VIEW);
    op::ExecutionContext context(4096, nullptr);
    context.SetResultRowSize(rowSize);
    std::stack<vec::BaseVector*> args;
    args.push(inputVec1);
    args.push(inputVec2);
    ASSERT_NO_THROW(function->Apply(args, svType, resultVector, &context));

    auto* resultSvVec = dynamic_cast<Vector<StringView>*>(resultVector);
    ASSERT_NE(resultSvVec, nullptr) << "Result vector type conversion failed";
    ConcatFunctionTestHelper::ValidateStringViewResult(resultSvVec, expectedStrs, rowSize);
    delete resultVector;
}

TEST(ConcatTest, ConcatSVOutNestedThreeArgs) {
    int rowSize = 3;
    std::vector<std::string> inputStrs1 = {"a", "b", "c"};
    std::vector<std::string> inputStrs2 = {"-", ":", "|"};
    std::vector<std::string> inputStrs3 = {"1", "2", "3"};
    std::vector<std::string> expectedStrs = {"a-1", "b:2", "c|3"};
    vec::BaseVector *inputVec1, *inputVec2, *inputVec3;
    ConcatFunctionTestHelper::SetupTestVectorSV(rowSize, inputStrs1, inputVec1);
    ConcatFunctionTestHelper::SetupTestVectorSV(rowSize, inputStrs2, inputVec2);
    ConcatFunctionTestHelper::SetupTestVectorSV(rowSize, inputStrs3, inputVec3);

    auto signature = std::make_shared<FunctionSignature>("concat",
        std::vector<DataTypeId>{OMNI_STRING_VIEW, OMNI_STRING_VIEW}, OMNI_STRING_VIEW);
    auto function = VectorFunction::Find(signature);
    ASSERT_NE(function, nullptr) << "concat({SV,SV})->SV overload not found";

    auto svType = std::make_shared<DataType>(OMNI_STRING_VIEW);
    op::ExecutionContext context(4096, nullptr);
    context.SetResultRowSize(rowSize);

    std::stack<vec::BaseVector*> args1;
    args1.push(inputVec2);
    args1.push(inputVec3);
    vec::BaseVector* intermediateResult = nullptr;
    ASSERT_NO_THROW(function->Apply(args1, svType, intermediateResult, &context));

    std::stack<vec::BaseVector*> args2;
    args2.push(inputVec1);
    args2.push(intermediateResult);
    vec::BaseVector* finalResult = nullptr;
    ASSERT_NO_THROW(function->Apply(args2, svType, finalResult, &context));

    auto* resultSvVec = dynamic_cast<Vector<StringView>*>(finalResult);
    ASSERT_NE(resultSvVec, nullptr) << "Result vector type conversion failed";
    ConcatFunctionTestHelper::ValidateStringViewResult(resultSvVec, expectedStrs, rowSize);
    delete finalResult;
}

TEST(ConcatTest, ConcatSVOutResultOwnsBuffer) {
    int rowSize = 1;
    std::vector<std::string> inputStrs1 = {"prefix_long_value_"};
    std::vector<std::string> inputStrs2 = {"suffix_long_value_"};
    std::vector<std::string> expectedStrs = {"prefix_long_value_suffix_long_value_"};
    vec::BaseVector *inputVec1, *inputVec2;
    ConcatFunctionTestHelper::SetupTestVectorSV(rowSize, inputStrs1, inputVec1);
    ConcatFunctionTestHelper::SetupTestVectorSV(rowSize, inputStrs2, inputVec2);

    auto signature = std::make_shared<FunctionSignature>("concat",
        std::vector<DataTypeId>{OMNI_STRING_VIEW, OMNI_STRING_VIEW}, OMNI_STRING_VIEW);
    auto function = VectorFunction::Find(signature);
    ASSERT_NE(function, nullptr) << "concat({SV,SV})->SV overload not found";

    vec::BaseVector* resultVector = nullptr;
    auto svType = std::make_shared<DataType>(OMNI_STRING_VIEW);
    op::ExecutionContext context(4096, nullptr);
    context.SetResultRowSize(rowSize);
    std::stack<vec::BaseVector*> args;
    args.push(inputVec1);
    args.push(inputVec2);
    ASSERT_NO_THROW(function->Apply(args, svType, resultVector, &context));

    auto* resultSvVec = dynamic_cast<Vector<StringView>*>(resultVector);
    ASSERT_NE(resultSvVec, nullptr);
    ConcatFunctionTestHelper::ValidateStringViewResult(resultSvVec, expectedStrs, rowSize);
    delete resultVector;
}
#endif
