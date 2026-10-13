/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * Description: Reverse function test for String and Array types
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
#include "vectorization/functions/ReverseFunction.h"
#include "vector/vector_helper.h"
#include "vector/array_vector.h"

using namespace omniruntime;
using namespace omniruntime::vec;
using namespace omniruntime::vectorization;
using namespace omniruntime::mem;
using namespace omniruntime::op;
using namespace omniruntime::expressions;
using namespace omniruntime::TestUtil;
using namespace omniruntime::type;
using namespace omniruntime::codegen;

class ReverseFunctionTestHelper {
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

TEST(ReverseTest, ReverseFunctionBasicTest) {

    int rowSize = 5;
    vec::BaseVector *inputVec;

    std::vector<std::string> inputStrs = {"hello", "world", "test", "foo", "bar"};
    std::vector<std::string> expectedStrs = {"olleh", "dlrow", "tset", "oof", "rab"};

    ReverseFunctionTestHelper::SetupTestVector(rowSize, inputStrs, inputVec);

    auto signature = std::make_shared<FunctionSignature>("reverse",
        std::vector<DataTypeId>{OMNI_VARCHAR},
        OMNI_VARCHAR);
    auto function = VectorFunction::Find(signature);
    ASSERT_NE(function, nullptr) << "Reverse function not found";

    vec::BaseVector* resultVector = nullptr;
    auto varcharType = std::make_shared<DataType>(OMNI_VARCHAR);

    op::ExecutionContext context(4096, nullptr);
    context.SetResultRowSize(rowSize);
    std::stack<vec::BaseVector*> args;
    args.push(inputVec);

    ASSERT_NO_THROW(function->Apply(args, varcharType, resultVector, &context))
        << "ReverseFunction.apply() threw an unexpected exception";

    auto* resultStrVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(resultVector);
    ASSERT_NE(resultStrVec, nullptr) << "Result vector type conversion failed";

    ReverseFunctionTestHelper::ValidateResult(resultStrVec, expectedStrs, rowSize);

    delete resultVector;
}

TEST(ReverseTest, ReverseFunctionEmptyStringTest) {
    int rowSize = 4;
    vec::BaseVector *inputVec;

    std::vector<std::string> inputStrs = {"", "hello", "", "world"};
    std::vector<std::string> expectedStrs = {"", "olleh", "", "dlrow"};

    ReverseFunctionTestHelper::SetupTestVector(rowSize, inputStrs, inputVec);

    auto signature = std::make_shared<FunctionSignature>("reverse",
        std::vector<DataTypeId>{OMNI_VARCHAR},
        OMNI_VARCHAR);
    auto function = VectorFunction::Find(signature);
    ASSERT_NE(function, nullptr) << "Reverse function not found";

    vec::BaseVector* resultVector = nullptr;
    auto varcharType = std::make_shared<DataType>(OMNI_VARCHAR);

    op::ExecutionContext context(4096, nullptr);
    context.SetResultRowSize(rowSize);
    std::stack<vec::BaseVector*> args;
    args.push(inputVec);

    ASSERT_NO_THROW(function->Apply(args, varcharType, resultVector, &context))
        << "ReverseFunction.apply() threw an unexpected exception";

    auto* resultStrVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(resultVector);
    ASSERT_NE(resultStrVec, nullptr) << "Result vector type conversion failed";

    ReverseFunctionTestHelper::ValidateResult(resultStrVec, expectedStrs, rowSize);

    delete resultVector;
}

TEST(ReverseTest, ReverseFunctionNullTest) {
    int rowSize = 4;
    vec::BaseVector *inputVec;

    std::vector<std::string> inputStrs = {"hello", "world", "test", "foo"};

    ReverseFunctionTestHelper::SetupTestVector(rowSize, inputStrs, inputVec);

    // Set some NULL values
    inputVec->SetNull(1);
    inputVec->SetNull(3);

    auto signature = std::make_shared<FunctionSignature>("reverse",
        std::vector<DataTypeId>{OMNI_VARCHAR},
        OMNI_VARCHAR);
    auto function = VectorFunction::Find(signature);
    ASSERT_NE(function, nullptr) << "Reverse function not found";

    vec::BaseVector* resultVector = nullptr;
    auto varcharType = std::make_shared<DataType>(OMNI_VARCHAR);

    op::ExecutionContext context(4096, nullptr);
    context.SetResultRowSize(rowSize);
    std::stack<vec::BaseVector*> args;
    args.push(inputVec);

    ASSERT_NO_THROW(function->Apply(args, varcharType, resultVector, &context))
        << "ReverseFunction.apply() threw an unexpected exception";

    auto* resultStrVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(resultVector);
    ASSERT_NE(resultStrVec, nullptr) << "Result vector type conversion failed";

    // Check NULL propagation
    EXPECT_TRUE(resultStrVec->IsNull(1)) << "Row 1 should be NULL (input is NULL)";
    EXPECT_TRUE(resultStrVec->IsNull(3)) << "Row 3 should be NULL (input is NULL)";
    EXPECT_FALSE(resultStrVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_FALSE(resultStrVec->IsNull(2)) << "Row 2 should not be NULL";

    // Check non-NULL results
    if (!resultStrVec->IsNull(0)) {
        std::string_view resultValue = resultStrVec->GetValue(0);
        std::string resultStr(resultValue);
        EXPECT_EQ(resultStr, "olleh") << "Row 0 result should be 'olleh'";
    }

    if (!resultStrVec->IsNull(2)) {
        std::string_view resultValue = resultStrVec->GetValue(2);
        std::string resultStr(resultValue);
        EXPECT_EQ(resultStr, "tset") << "Row 2 result should be 'tset'";
    }

    delete resultVector;
}

TEST(ReverseTest, ReverseFunctionSingleCharTest) {
    int rowSize = 3;
    vec::BaseVector *inputVec;

    std::vector<std::string> inputStrs = {"a", "b", "c"};
    std::vector<std::string> expectedStrs = {"a", "b", "c"};

    ReverseFunctionTestHelper::SetupTestVector(rowSize, inputStrs, inputVec);

    auto signature = std::make_shared<FunctionSignature>("reverse",
        std::vector<DataTypeId>{OMNI_VARCHAR},
        OMNI_VARCHAR);
    auto function = VectorFunction::Find(signature);
    ASSERT_NE(function, nullptr) << "Reverse function not found";

    vec::BaseVector* resultVector = nullptr;
    auto varcharType = std::make_shared<DataType>(OMNI_VARCHAR);

    op::ExecutionContext context(4096, nullptr);
    context.SetResultRowSize(rowSize);
    std::stack<vec::BaseVector*> args;
    args.push(inputVec);

    ASSERT_NO_THROW(function->Apply(args, varcharType, resultVector, &context))
        << "ReverseFunction.apply() threw an unexpected exception";

    auto* resultStrVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(resultVector);
    ASSERT_NE(resultStrVec, nullptr) << "Result vector type conversion failed";

    ReverseFunctionTestHelper::ValidateResult(resultStrVec, expectedStrs, rowSize);

    delete resultVector;
}

TEST(ReverseTest, ReverseFunctionLongStringTest) {
    int rowSize = 2;
    vec::BaseVector *inputVec;

    std::vector<std::string> inputStrs = {"abcdefghijklmnopqrstuvwxyz", "1234567890"};
    std::vector<std::string> expectedStrs = {"zyxwvutsrqponmlkjihgfedcba", "0987654321"};

    ReverseFunctionTestHelper::SetupTestVector(rowSize, inputStrs, inputVec);

    auto signature = std::make_shared<FunctionSignature>("reverse",
        std::vector<DataTypeId>{OMNI_VARCHAR},
        OMNI_VARCHAR);
    auto function = VectorFunction::Find(signature);
    ASSERT_NE(function, nullptr) << "Reverse function not found";

    vec::BaseVector* resultVector = nullptr;
    auto varcharType = std::make_shared<DataType>(OMNI_VARCHAR);

    op::ExecutionContext context(4096, nullptr);
    context.SetResultRowSize(rowSize);
    std::stack<vec::BaseVector*> args;
    args.push(inputVec);

    ASSERT_NO_THROW(function->Apply(args, varcharType, resultVector, &context))
        << "ReverseFunction.apply() threw an unexpected exception";

    auto* resultStrVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(resultVector);
    ASSERT_NE(resultStrVec, nullptr) << "Result vector type conversion failed";

    ReverseFunctionTestHelper::ValidateResult(resultStrVec, expectedStrs, rowSize);

    delete resultVector;
}

// ==================== Array Type Tests ====================

class ReverseArrayTestHelper {
public:
    /// Create an ArrayVector with int32 elements for testing
    static ArrayVector* CreateIntArrayVector(
        const std::vector<std::vector<int32_t>>& arraysData,
        const std::vector<bool>& nullMasks = {})
    {
        std::vector<int64_t> offsets;
        offsets.push_back(0);
        size_t totalElements = 0;
        std::vector<int32_t> allElements;

        for (const auto& arrayData : arraysData) {
            allElements.insert(allElements.end(), arrayData.begin(), arrayData.end());
            totalElements += arrayData.size();
            offsets.push_back(totalElements);
        }

        auto elementVector = VectorHelper::CreateFlatVector(nullptr, OMNI_INT, allElements.size());
        auto* elementFlatVec = reinterpret_cast<Vector<int32_t>*>(elementVector);
        for (size_t i = 0; i < allElements.size(); ++i) {
            elementFlatVec->SetValue(i, allElements[i]);
        }

        int32_t rowCount = arraysData.size();
        auto arrayVector = new ArrayVector(rowCount, std::shared_ptr<BaseVector>(elementVector));

        // Set offset array
        for (int32_t i = 0; i <= rowCount; ++i) {
            arrayVector->SetOffset(i, offsets[i]);
        }

        // Set NULL masks
        for (int32_t i = 0; i < rowCount; ++i) {
            bool isNull = (i < static_cast<int32_t>(nullMasks.size())) ? nullMasks[i] : false;
            if (isNull) {
                arrayVector->SetNull(i);
            } else {
                arrayVector->SetNotNull(i);
            }
        }

        return arrayVector;
    }

    /// Create an ArrayVector with string elements for testing
    static ArrayVector* CreateStringArrayVector(
        const std::vector<std::vector<std::string>>& arraysData,
        const std::vector<bool>& nullMasks = {})
    {
        std::vector<int64_t> offsets;
        offsets.push_back(0);
        size_t totalElements = 0;
        std::vector<std::string> allElements;

        for (const auto& arrayData : arraysData) {
            allElements.insert(allElements.end(), arrayData.begin(), arrayData.end());
            totalElements += arrayData.size();
            offsets.push_back(totalElements);
        }

        auto elementVector = VectorHelper::CreateStringVector(allElements.size());
        auto* elementStrVec = reinterpret_cast<Vector<LargeStringContainer<std::string_view>>*>(elementVector);
        for (size_t i = 0; i < allElements.size(); ++i) {
            std::string_view strView(allElements[i]);
            elementStrVec->SetValue(i, strView);
        }

        int32_t rowCount = arraysData.size();
        auto arrayVector = new ArrayVector(rowCount, std::shared_ptr<BaseVector>(elementVector));

        // Set offset array
        for (int32_t i = 0; i <= rowCount; ++i) {
            arrayVector->SetOffset(i, offsets[i]);
        }

        // Set NULL masks
        for (int32_t i = 0; i < rowCount; ++i) {
            bool isNull = (i < static_cast<int32_t>(nullMasks.size())) ? nullMasks[i] : false;
            if (isNull) {
                arrayVector->SetNull(i);
            } else {
                arrayVector->SetNotNull(i);
            }
        }

        return arrayVector;
    }
};

// Test basic integer array reverse
TEST(ReverseTest, ReverseArrayIntBasicTest) {
    std::vector<std::vector<int32_t>> inputArrays = {
        {1, 2, 3},
        {4, 5},
        {6},
        {}
    };

    std::vector<std::vector<int32_t>> expectedArrays = {
        {3, 2, 1},
        {5, 4},
        {6},
        {}
    };

    auto* inputVec = ReverseArrayTestHelper::CreateIntArrayVector(inputArrays);
    int rowSize = inputArrays.size();

    auto signature = std::make_shared<FunctionSignature>("reverse",
        std::vector<DataTypeId>{OMNI_ARRAY},
        OMNI_ARRAY);
    auto function = VectorFunction::Find(signature);
    ASSERT_NE(function, nullptr) << "Reverse function for array not found";

    vec::BaseVector* resultVector = nullptr;
    auto arrayType = std::make_shared<DataType>(OMNI_ARRAY);

    op::ExecutionContext context(4096, nullptr);
    context.SetResultRowSize(rowSize);
    std::stack<vec::BaseVector*> args;
    args.push(inputVec);

    ASSERT_NO_THROW(function->Apply(args, arrayType, resultVector, &context))
        << "ReverseFunction.apply() threw an unexpected exception";

    auto* resultArrayVec = dynamic_cast<ArrayVector*>(resultVector);
    ASSERT_NE(resultArrayVec, nullptr) << "Result vector type conversion failed";

    // Validate results
    for (int row = 0; row < rowSize; ++row) {
        ASSERT_FALSE(resultArrayVec->IsNull(row)) << "Row " << row << " should not be NULL";

        int64_t arraySize = resultArrayVec->GetSize(row);
        ASSERT_EQ(arraySize, static_cast<int64_t>(expectedArrays[row].size()))
            << "Array " << row << " size mismatch";

        auto elementVec = resultArrayVec->GetElementVector();
        auto* intElementVec = dynamic_cast<Vector<int32_t>*>(elementVec.get());
        ASSERT_NE(intElementVec, nullptr) << "Element vector type conversion failed";

        int64_t startOffset = resultArrayVec->GetOffset(row);
        for (int64_t i = 0; i < arraySize; ++i) {
            int32_t actual = intElementVec->GetValue(startOffset + i);
            int32_t expected = expectedArrays[row][i];
            EXPECT_EQ(actual, expected)
                << "Array " << row << ", element " << i << " mismatch";
        }
    }

    delete resultVector;
}

// Test string array reverse
TEST(ReverseTest, ReverseArrayStringTest) {
    std::vector<std::vector<std::string>> inputArrays = {
        {"a", "b", "c"},
        {"hello", "world"},
        {"single"}
    };

    std::vector<std::vector<std::string>> expectedArrays = {
        {"c", "b", "a"},
        {"world", "hello"},
        {"single"}
    };

    auto* inputVec = ReverseArrayTestHelper::CreateStringArrayVector(inputArrays);
    int rowSize = inputArrays.size();

    auto signature = std::make_shared<FunctionSignature>("reverse",
        std::vector<DataTypeId>{OMNI_ARRAY},
        OMNI_ARRAY);
    auto function = VectorFunction::Find(signature);
    ASSERT_NE(function, nullptr) << "Reverse function for array not found";

    vec::BaseVector* resultVector = nullptr;
    auto arrayType = std::make_shared<DataType>(OMNI_ARRAY);

    op::ExecutionContext context(4096, nullptr);
    context.SetResultRowSize(rowSize);
    std::stack<vec::BaseVector*> args;
    args.push(inputVec);

    ASSERT_NO_THROW(function->Apply(args, arrayType, resultVector, &context))
        << "ReverseFunction.apply() threw an unexpected exception";

    auto* resultArrayVec = dynamic_cast<ArrayVector*>(resultVector);
    ASSERT_NE(resultArrayVec, nullptr) << "Result vector type conversion failed";

    // Validate results
    for (int row = 0; row < rowSize; ++row) {
        ASSERT_FALSE(resultArrayVec->IsNull(row)) << "Row " << row << " should not be NULL";

        int64_t arraySize = resultArrayVec->GetSize(row);
        ASSERT_EQ(arraySize, static_cast<int64_t>(expectedArrays[row].size()))
            << "Array " << row << " size mismatch";

        auto elementVec = resultArrayVec->GetElementVector();
        auto* strElementVec = dynamic_cast<Vector<LargeStringContainer<std::string_view>>*>(elementVec.get());
        ASSERT_NE(strElementVec, nullptr) << "Element vector type conversion failed";

        int64_t startOffset = resultArrayVec->GetOffset(row);
        for (int64_t i = 0; i < arraySize; ++i) {
            std::string_view actual = strElementVec->GetValue(startOffset + i);
            std::string expected = expectedArrays[row][i];
            EXPECT_EQ(std::string(actual), expected)
                << "Array " << row << ", element " << i << " mismatch";
        }
    }

    delete resultVector;
}

// Test array with NULL values
TEST(ReverseTest, ReverseArrayNullTest) {
    std::vector<std::vector<int32_t>> inputArrays = {
        {1, 2, 3},
        {},  // Will be marked as NULL
        {4, 5}
    };

    std::vector<bool> nullMasks = {false, true, false};

    auto* inputVec = ReverseArrayTestHelper::CreateIntArrayVector(inputArrays, nullMasks);
    int rowSize = inputArrays.size();

    auto signature = std::make_shared<FunctionSignature>("reverse",
        std::vector<DataTypeId>{OMNI_ARRAY},
        OMNI_ARRAY);
    auto function = VectorFunction::Find(signature);
    ASSERT_NE(function, nullptr) << "Reverse function for array not found";

    vec::BaseVector* resultVector = nullptr;
    auto arrayType = std::make_shared<DataType>(OMNI_ARRAY);

    op::ExecutionContext context(4096, nullptr);
    context.SetResultRowSize(rowSize);
    std::stack<vec::BaseVector*> args;
    args.push(inputVec);

    ASSERT_NO_THROW(function->Apply(args, arrayType, resultVector, &context))
        << "ReverseFunction.apply() threw an unexpected exception";

    auto* resultArrayVec = dynamic_cast<ArrayVector*>(resultVector);
    ASSERT_NE(resultArrayVec, nullptr) << "Result vector type conversion failed";

    // Check NULL propagation
    EXPECT_FALSE(resultArrayVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_TRUE(resultArrayVec->IsNull(1)) << "Row 1 should be NULL (input is NULL)";
    EXPECT_FALSE(resultArrayVec->IsNull(2)) << "Row 2 should not be NULL";

    // Validate non-NULL results
    auto elementVec = resultArrayVec->GetElementVector();
    auto* intElementVec = dynamic_cast<Vector<int32_t>*>(elementVec.get());
    ASSERT_NE(intElementVec, nullptr) << "Element vector type conversion failed";

    // Row 0: [1, 2, 3] -> [3, 2, 1]
    if (!resultArrayVec->IsNull(0)) {
        int64_t startOffset = resultArrayVec->GetOffset(0);
        EXPECT_EQ(intElementVec->GetValue(startOffset + 0), 3);
        EXPECT_EQ(intElementVec->GetValue(startOffset + 1), 2);
        EXPECT_EQ(intElementVec->GetValue(startOffset + 2), 1);
    }

    // Row 2: [4, 5] -> [5, 4]
    if (!resultArrayVec->IsNull(2)) {
        int64_t startOffset = resultArrayVec->GetOffset(2);
        EXPECT_EQ(intElementVec->GetValue(startOffset + 0), 5);
        EXPECT_EQ(intElementVec->GetValue(startOffset + 1), 4);
    }

    delete resultVector;
}

// Test empty array
TEST(ReverseTest, ReverseEmptyArrayTest) {
    std::vector<std::vector<int32_t>> inputArrays = {
        {},
        {},
        {}
    };

    auto* inputVec = ReverseArrayTestHelper::CreateIntArrayVector(inputArrays);
    int rowSize = inputArrays.size();

    auto signature = std::make_shared<FunctionSignature>("reverse",
        std::vector<DataTypeId>{OMNI_ARRAY},
        OMNI_ARRAY);
    auto function = VectorFunction::Find(signature);
    ASSERT_NE(function, nullptr) << "Reverse function for array not found";

    vec::BaseVector* resultVector = nullptr;
    auto arrayType = std::make_shared<DataType>(OMNI_ARRAY);

    op::ExecutionContext context(4096, nullptr);
    context.SetResultRowSize(rowSize);
    std::stack<vec::BaseVector*> args;
    args.push(inputVec);

    ASSERT_NO_THROW(function->Apply(args, arrayType, resultVector, &context))
        << "ReverseFunction.apply() threw an unexpected exception";

    auto* resultArrayVec = dynamic_cast<ArrayVector*>(resultVector);
    ASSERT_NE(resultArrayVec, nullptr) << "Result vector type conversion failed";

    // All arrays should be empty (size 0)
    for (int row = 0; row < rowSize; ++row) {
        EXPECT_FALSE(resultArrayVec->IsNull(row)) << "Row " << row << " should not be NULL";
        EXPECT_EQ(resultArrayVec->GetSize(row), 0)
            << "Empty array " << row << " should have size 0";
    }

    delete resultVector;
}

// Test single element array
TEST(ReverseTest, ReverseSingleElementArrayTest) {
    std::vector<std::vector<int32_t>> inputArrays = {
        {42}
    };

    auto* inputVec = ReverseArrayTestHelper::CreateIntArrayVector(inputArrays);
    int rowSize = inputArrays.size();

    auto signature = std::make_shared<FunctionSignature>("reverse",
        std::vector<DataTypeId>{OMNI_ARRAY},
        OMNI_ARRAY);
    auto function = VectorFunction::Find(signature);
    ASSERT_NE(function, nullptr) << "Reverse function for array not found";

    vec::BaseVector* resultVector = nullptr;
    auto arrayType = std::make_shared<DataType>(OMNI_ARRAY);

    op::ExecutionContext context(4096, nullptr);
    context.SetResultRowSize(rowSize);
    std::stack<vec::BaseVector*> args;
    args.push(inputVec);

    ASSERT_NO_THROW(function->Apply(args, arrayType, resultVector, &context))
        << "ReverseFunction.apply() threw an unexpected exception";

    auto* resultArrayVec = dynamic_cast<ArrayVector*>(resultVector);
    ASSERT_NE(resultArrayVec, nullptr) << "Result vector type conversion failed";

    // Validate result
    EXPECT_FALSE(resultArrayVec->IsNull(0)) << "Result should not be NULL";
    EXPECT_EQ(resultArrayVec->GetSize(0), 1) << "Single element array size should be 1";

    auto elementVec = resultArrayVec->GetElementVector();
    auto* intElementVec = dynamic_cast<Vector<int32_t>*>(elementVec.get());
    ASSERT_NE(intElementVec, nullptr) << "Element vector type conversion failed";

    int64_t startOffset = resultArrayVec->GetOffset(0);
    EXPECT_EQ(intElementVec->GetValue(startOffset), 42)
        << "Single element should be 42";

    delete resultVector;
}

// Test array with null element vector (edge case following Spark behavior)
// According to Spark, reverse of empty array should return empty array, not throw exception
TEST(ReverseTest, ReverseArrayNullElementVectorTest) {
    int rowSize = 3;

    // Create an ArrayVector without setting element vector (nullptr)
    auto* inputVec = new ArrayVector(rowSize);

    // Set offsets (all pointing to 0 since no elements)
    for (int32_t i = 0; i <= rowSize; ++i) {
        inputVec->SetOffset(i, 0);
    }

    // Set row 0 and row 2 as non-null empty arrays, row 1 as null
    inputVec->SetNotNull(0);
    inputVec->SetNull(1);
    inputVec->SetNotNull(2);

    auto signature = std::make_shared<FunctionSignature>("reverse",
        std::vector<DataTypeId>{OMNI_ARRAY},
        OMNI_ARRAY);
    auto function = VectorFunction::Find(signature);
    ASSERT_NE(function, nullptr) << "Reverse function for array not found";

    vec::BaseVector* resultVector = nullptr;
    auto arrayType = std::make_shared<DataType>(OMNI_ARRAY);

    op::ExecutionContext context(4096, nullptr);
    context.SetResultRowSize(rowSize);
    std::stack<vec::BaseVector*> args;
    args.push(inputVec);

    // Should NOT throw - empty arrays should return empty arrays per Spark behavior
    ASSERT_NO_THROW(function->Apply(args, arrayType, resultVector, &context))
        << "ReverseFunction should not throw for arrays with null element vector";

    auto* resultArrayVec = dynamic_cast<ArrayVector*>(resultVector);
    ASSERT_NE(resultArrayVec, nullptr) << "Result vector type conversion failed";

    // Validate results
    // Row 0: empty array (not null) -> empty array (not null)
    EXPECT_FALSE(resultArrayVec->IsNull(0)) << "Row 0 should not be NULL";
    EXPECT_EQ(resultArrayVec->GetSize(0), 0) << "Row 0 should be empty array";

    // Row 1: null -> null
    EXPECT_TRUE(resultArrayVec->IsNull(1)) << "Row 1 should be NULL";

    // Row 2: empty array (not null) -> empty array (not null)
    EXPECT_FALSE(resultArrayVec->IsNull(2)) << "Row 2 should not be NULL";
    EXPECT_EQ(resultArrayVec->GetSize(2), 0) << "Row 2 should be empty array";

    delete resultVector;
}