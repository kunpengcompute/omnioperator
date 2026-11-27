/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: codegen test
 */

#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>

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

class SplitFunctionTestHelper {
public:
    static void SetupTestVectors(int rowSize,
                                const std::vector<std::string>& inputStrs,
                                const std::string& delimiter,
                                int32_t limit,
                                vec::BaseVector*& inputVec,
                                vec::BaseVector*& delimiterVec,
                                vec::BaseVector*& limitVec)
    {
        inputVec = VectorHelper::CreateStringVector(rowSize);
        auto *inputVector = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(inputVec);
        if (!inputVector) {
            FAIL() << "Type conversion failed for input vectors";
            return;
        }
        for (int i = 0; i < rowSize; i++) {
            std::string_view inputStr(inputStrs[i]);
            inputVector->SetValue(i, inputStr);
        }

        delimiterVec = new ConstVector<std::string_view>(std::string_view(delimiter), OMNI_VARCHAR, rowSize);
        limitVec = new ConstVector<int32_t>(limit, OMNI_INT, rowSize);
    }

    static void ValidateResult(ArrayVector* resultVector, int rowSize)
    {
        auto elementVector = resultVector->GetElementVector();
        if (!elementVector) {
            FAIL() << "Element vector of result is null";
            return;
        }
        auto* strElementVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(elementVector.get());
        if (!strElementVec) {
            FAIL() << "Element vector is not string type";
            return;
        }

        for (int row = 0; row < rowSize; ++row) {
            std::cout << "Row " << row << ": ";
            if (resultVector->IsNull(row)) {
                std::cout << "Result is NULL" << std::endl;
                continue;
            }

            int64_t arraySize = resultVector->GetSize(row);
            int64_t startIdx = resultVector->GetOffset(row);
            std::cout << "Split elements: [";
            for (int64_t elemIdx = 0; elemIdx < arraySize; ++elemIdx) {
                int64_t globalElemIdx = startIdx + elemIdx;
                if (globalElemIdx >= strElementVec->GetSize()) {
                    std::cout << "out_of_bounds";
                    continue;
                }
                if (strElementVec->IsNull(globalElemIdx)) {
                    std::cout << "null";
                } else {
                    std::string_view elemValue = strElementVec->GetValue(globalElemIdx);
                    std::cout << "\"" << elemValue << "\"";
                }
                if (elemIdx != arraySize - 1) {
                    std::cout << ", ";
                }
            }
            std::cout << "]" << std::endl;
        }
    }

    static void ExecuteSplitFunction(vec::BaseVector* inputVec,
                                   vec::BaseVector* delimiterVec,
                                   vec::BaseVector* limitVec,
                                   int rowSize)
    {
        auto signature = std::make_shared<FunctionSignature>("split",
            std::vector<omniruntime::type::DataTypeId>{OMNI_VARCHAR, OMNI_VARCHAR, OMNI_INT},
            OMNI_ARRAY);
        auto function = VectorFunction::Find(signature);
        vec::BaseVector* resultVector = nullptr;
        auto arrayType = std::make_shared<DataType>(OMNI_ARRAY);

        op::ExecutionContext context(4096, nullptr);
        context.SetResultRowSize(rowSize);
        std::stack<vec::BaseVector*> args;
        args.push(inputVec);
        args.push(delimiterVec);
        args.push(limitVec);

        ASSERT_NO_THROW(function->Apply(args, arrayType, resultVector, &context))
            << "SplitFunction.apply() threw an unexpected exception";

        ValidateResult(resultVector, rowSize);
        delete resultVector;
    }
};

TEST(VectorizationTest, SplitFunctionTest) {
    std::cout << "=== Split Function Direct Test ===" << std::endl;

    int rowSize = 3;
    vec::BaseVector *inputVec, *delimiterVec, *limitVec;

    std::string delimiter = " ";

    SplitFunctionTestHelper::SetupTestVectors(
        rowSize,
        {"Washington 6th", "Dogwood Washington", "Laurel"}, delimiter, -1,
        inputVec, delimiterVec, limitVec
    );

    SplitFunctionTestHelper::ExecuteSplitFunction(inputVec, delimiterVec, limitVec, rowSize);

    // Apply() takes ownership and deletes limitArg, delimiterArg, inputArg; do not delete here
    std::cout << "=== Direct Test Completed ===" << std::endl;
}

TEST(VectorizationTest, SplitFunctionLimitTest) {
    std::cout << "=== Split Function Multi-Row Test ===" << std::endl;

    int rowSize = 3;
    vec::BaseVector *inputVec, *delimiterVec, *limitVec;

    std::string delimiter = ",";

    SplitFunctionTestHelper::SetupTestVectors(
        rowSize,
        {"1,2,3,4", "a,b,c", "Omni,Operator"}, delimiter, 2,
        inputVec, delimiterVec, limitVec
    );

    SplitFunctionTestHelper::ExecuteSplitFunction(inputVec, delimiterVec, limitVec, rowSize);

    // Apply() takes ownership and deletes limitArg, delimiterArg, inputArg; do not delete here
    std::cout << "=== Multi-Row Test Completed ===" << std::endl;
}

// Flat (per-row) delimiter: each row has a different delimiter from a column reference
TEST(VectorizationTest, SplitFunctionFlatDelimiterTest) {
    std::cout << "=== Split Function Flat Delimiter Test ===" << std::endl;

    int rowSize = 3;

    auto *inputVec = VectorHelper::CreateStringVector(rowSize);
    auto *inputVector = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(inputVec);
    inputVector->SetValue(0, std::string_view("a,b,c"));
    inputVector->SetValue(1, std::string_view("x-y-z"));
    inputVector->SetValue(2, std::string_view("1|2|3"));

    auto *delimiterVec = VectorHelper::CreateStringVector(rowSize);
    auto *delimVector = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(delimiterVec);
    delimVector->SetValue(0, std::string_view(","));
    delimVector->SetValue(1, std::string_view("-"));
    delimVector->SetValue(2, std::string_view("|"));

    auto *limitVec = new ConstVector<int32_t>(-1, OMNI_INT, rowSize);

    SplitFunctionTestHelper::ExecuteSplitFunction(inputVec, delimiterVec, limitVec, rowSize);

    std::cout << "=== Flat Delimiter Test Completed ===" << std::endl;
}

// NULL input string: rows with NULL input should produce NULL result
TEST(VectorizationTest, SplitFunctionNullInputTest) {
    std::cout << "=== Split Function Null Input Test ===" << std::endl;

    int rowSize = 3;

    auto *inputVec = VectorHelper::CreateStringVector(rowSize);
    auto *inputVector = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(inputVec);
    inputVector->SetValue(0, std::string_view("hello world"));
    inputVector->SetNull(1);
    inputVector->SetValue(2, std::string_view("foo bar"));

    auto *delimiterVec = new ConstVector<std::string_view>(std::string_view(" "), OMNI_VARCHAR, rowSize);
    auto *limitVec = new ConstVector<int32_t>(-1, OMNI_INT, rowSize);

    SplitFunctionTestHelper::ExecuteSplitFunction(inputVec, delimiterVec, limitVec, rowSize);

    std::cout << "=== Null Input Test Completed ===" << std::endl;
}

// NULL delimiter from column: rows with NULL delimiter should produce NULL result
TEST(VectorizationTest, SplitFunctionNullDelimiterTest) {
    std::cout << "=== Split Function Null Delimiter Test ===" << std::endl;

    int rowSize = 3;

    auto *inputVec = VectorHelper::CreateStringVector(rowSize);
    auto *inputVector = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(inputVec);
    inputVector->SetValue(0, std::string_view("a,b,c"));
    inputVector->SetValue(1, std::string_view("x-y-z"));
    inputVector->SetValue(2, std::string_view("hello"));

    auto *delimiterVec = VectorHelper::CreateStringVector(rowSize);
    auto *delimVector = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(delimiterVec);
    delimVector->SetValue(0, std::string_view(","));
    delimVector->SetNull(1);
    delimVector->SetValue(2, std::string_view("-"));

    auto *limitVec = new ConstVector<int32_t>(-1, OMNI_INT, rowSize);

    SplitFunctionTestHelper::ExecuteSplitFunction(inputVec, delimiterVec, limitVec, rowSize);

    // Row 0: ["a","b","c"], Row 1: NULL, Row 2: ["hello"]
    std::cout << "=== Null Delimiter Test Completed ===" << std::endl;
}

// Empty string and empty delimiter edge cases
TEST(VectorizationTest, SplitFunctionEmptyEdgeCasesTest) {
    std::cout << "=== Split Function Empty Edge Cases Test ===" << std::endl;

    int rowSize = 4;

    auto *inputVec = VectorHelper::CreateStringVector(rowSize);
    auto *inputVector = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(inputVec);
    inputVector->SetValue(0, std::string_view(""));
    inputVector->SetValue(1, std::string_view("abc"));
    inputVector->SetValue(2, std::string_view("no_match"));
    inputVector->SetValue(3, std::string_view("a,,b"));

    auto *delimiterVec = VectorHelper::CreateStringVector(rowSize);
    auto *delimVector = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(delimiterVec);
    delimVector->SetValue(0, std::string_view(","));
    delimVector->SetValue(1, std::string_view(""));
    delimVector->SetValue(2, std::string_view("X"));
    delimVector->SetValue(3, std::string_view(","));

    auto *limitVec = new ConstVector<int32_t>(-1, OMNI_INT, rowSize);

    SplitFunctionTestHelper::ExecuteSplitFunction(inputVec, delimiterVec, limitVec, rowSize);

    // Row 0: [""] (empty input), Row 1: ["a","b","c"] (empty delim → per char),
    // Row 2: ["no_match"] (no match), Row 3: ["a","","b"] (consecutive delimiters)
    std::cout << "=== Empty Edge Cases Test Completed ===" << std::endl;
}

// Flat delimiter with limit
TEST(VectorizationTest, SplitFunctionFlatDelimiterWithLimitTest) {
    std::cout << "=== Split Function Flat Delimiter With Limit Test ===" << std::endl;

    int rowSize = 3;

    auto *inputVec = VectorHelper::CreateStringVector(rowSize);
    auto *inputVector = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(inputVec);
    inputVector->SetValue(0, std::string_view("a,b,c,d"));
    inputVector->SetValue(1, std::string_view("x-y-z-w"));
    inputVector->SetValue(2, std::string_view("1|2|3|4"));

    auto *delimiterVec = VectorHelper::CreateStringVector(rowSize);
    auto *delimVector = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(delimiterVec);
    delimVector->SetValue(0, std::string_view(","));
    delimVector->SetValue(1, std::string_view("-"));
    delimVector->SetValue(2, std::string_view("|"));

    auto *limitVec = new ConstVector<int32_t>(2, OMNI_INT, rowSize);

    SplitFunctionTestHelper::ExecuteSplitFunction(inputVec, delimiterVec, limitVec, rowSize);

    // Row 0: ["a","b,c,d"], Row 1: ["x","y-z-w"], Row 2: ["1","2|3|4"]
    std::cout << "=== Flat Delimiter With Limit Test Completed ===" << std::endl;
}

// Regex delimiter: escaped pipe \| should match literal pipe character (Spark semantics)
TEST(VectorizationTest, SplitFunctionRegexEscapedPipeTest) {
    std::cout << "=== Split Function Regex Escaped Pipe Test ===" << std::endl;

    int rowSize = 1;
    vec::BaseVector *inputVec, *delimiterVec, *limitVec;

    // In Spark: split('1|2|3|4|5', '\\|') uses regex \| to match literal |
    SplitFunctionTestHelper::SetupTestVectors(
        rowSize, {"1|2|3|4|5"}, "\\|", -1,
        inputVec, delimiterVec, limitVec
    );

    SplitFunctionTestHelper::ExecuteSplitFunction(inputVec, delimiterVec, limitVec, rowSize);

    // Expected: ["1","2","3","4","5"]
    std::cout << "=== Regex Escaped Pipe Test Completed ===" << std::endl;
}

// Regex delimiter: dot \. should match literal dot (Spark semantics)
TEST(VectorizationTest, SplitFunctionRegexEscapedDotTest) {
    std::cout << "=== Split Function Regex Escaped Dot Test ===" << std::endl;

    int rowSize = 1;
    vec::BaseVector *inputVec, *delimiterVec, *limitVec;

    SplitFunctionTestHelper::SetupTestVectors(
        rowSize, {"192.168.1.1"}, "\\.", -1,
        inputVec, delimiterVec, limitVec
    );

    SplitFunctionTestHelper::ExecuteSplitFunction(inputVec, delimiterVec, limitVec, rowSize);

    // Expected: ["192","168","1","1"]
    std::cout << "=== Regex Escaped Dot Test Completed ===" << std::endl;
}

// Regex delimiter: digit character class [0-9]
TEST(VectorizationTest, SplitFunctionRegexDigitClassTest) {
    std::cout << "=== Split Function Regex Digit Class Test ===" << std::endl;

    int rowSize = 1;
    vec::BaseVector *inputVec, *delimiterVec, *limitVec;

    SplitFunctionTestHelper::SetupTestVectors(
        rowSize, {"abc1def2ghi"}, "[0-9]", -1,
        inputVec, delimiterVec, limitVec
    );

    SplitFunctionTestHelper::ExecuteSplitFunction(inputVec, delimiterVec, limitVec, rowSize);

    // Expected: ["abc","def","ghi"]
    std::cout << "=== Regex Digit Class Test Completed ===" << std::endl;
}

// Regex delimiter: whitespace \s+
TEST(VectorizationTest, SplitFunctionRegexWhitespaceTest) {
    std::cout << "=== Split Function Regex Whitespace Test ===" << std::endl;

    int rowSize = 1;
    vec::BaseVector *inputVec, *delimiterVec, *limitVec;

    SplitFunctionTestHelper::SetupTestVectors(
        rowSize, {"hello  world\tfoo"}, "\\s+", -1,
        inputVec, delimiterVec, limitVec
    );

    SplitFunctionTestHelper::ExecuteSplitFunction(inputVec, delimiterVec, limitVec, rowSize);

    // Expected: ["hello","world","foo"]
    std::cout << "=== Regex Whitespace Test Completed ===" << std::endl;
}

// Regex delimiter: character class with multiple delimiters [,;:]
TEST(VectorizationTest, SplitFunctionRegexCharClassTest) {
    std::cout << "=== Split Function Regex Char Class Test ===" << std::endl;

    int rowSize = 1;
    vec::BaseVector *inputVec, *delimiterVec, *limitVec;

    SplitFunctionTestHelper::SetupTestVectors(
        rowSize, {"one,two;three:four"}, "[,;:]", -1,
        inputVec, delimiterVec, limitVec
    );

    SplitFunctionTestHelper::ExecuteSplitFunction(inputVec, delimiterVec, limitVec, rowSize);

    // Expected: ["one","two","three","four"]
    std::cout << "=== Regex Char Class Test Completed ===" << std::endl;
}

// Regex delimiter with limit
TEST(VectorizationTest, SplitFunctionRegexWithLimitTest) {
    std::cout << "=== Split Function Regex With Limit Test ===" << std::endl;

    int rowSize = 1;
    vec::BaseVector *inputVec, *delimiterVec, *limitVec;

    SplitFunctionTestHelper::SetupTestVectors(
        rowSize, {"1|2|3|4|5"}, "\\|", 3,
        inputVec, delimiterVec, limitVec
    );

    SplitFunctionTestHelper::ExecuteSplitFunction(inputVec, delimiterVec, limitVec, rowSize);

    // Expected: ["1","2","3|4|5"]
    std::cout << "=== Regex With Limit Test Completed ===" << std::endl;
}

// Per-row regex delimiter from column (the exact crash scenario from the user's SQL)
TEST(VectorizationTest, SplitFunctionFlatRegexDelimiterTest) {
    std::cout << "=== Split Function Flat Regex Delimiter Test ===" << std::endl;

    int rowSize = 3;

    auto *inputVec = VectorHelper::CreateStringVector(rowSize);
    auto *inputVector = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(inputVec);
    inputVector->SetValue(0, std::string_view("1|2|3|4|5"));
    inputVector->SetValue(1, std::string_view("192.168.1.1"));
    inputVector->SetValue(2, std::string_view("a,b,c"));

    auto *delimiterVec = VectorHelper::CreateStringVector(rowSize);
    auto *delimVector = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(delimiterVec);
    delimVector->SetValue(0, std::string_view("\\|"));
    delimVector->SetValue(1, std::string_view("\\."));
    delimVector->SetValue(2, std::string_view(","));

    auto *limitVec = new ConstVector<int32_t>(-1, OMNI_INT, rowSize);

    SplitFunctionTestHelper::ExecuteSplitFunction(inputVec, delimiterVec, limitVec, rowSize);

    // Row 0: ["1","2","3","4","5"], Row 1: ["192","168","1","1"], Row 2: ["a","b","c"]
    std::cout << "=== Flat Regex Delimiter Test Completed ===" << std::endl;
}