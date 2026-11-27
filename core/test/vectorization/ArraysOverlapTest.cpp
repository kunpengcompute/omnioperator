/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * Description: Unit tests for arrays_overlap function
 */

#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <limits>
#include <cmath>

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

class ArraysOverlapTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}

    template <typename T>
    VectorBatch *CreateTwoArrayBatch(int32_t rowSize,
        int32_t leftElemSize, T *leftElems, std::vector<int32_t> &leftOffsets,
        int32_t rightElemSize, T *rightElems, std::vector<int32_t> &rightOffsets)
    {
        auto *vectorBatch = new VectorBatch(rowSize);

        DataTypeId typeId;
        if constexpr (std::is_same_v<T, int8_t>) {
            typeId = OMNI_BYTE;
        } else if constexpr (std::is_same_v<T, int16_t>) {
            typeId = OMNI_SHORT;
        } else if constexpr (std::is_same_v<T, int32_t>) {
            typeId = OMNI_INT;
        } else if constexpr (std::is_same_v<T, int64_t>) {
            typeId = OMNI_LONG;
        } else if constexpr (std::is_same_v<T, float>) {
            typeId = OMNI_FLOAT;
        } else if constexpr (std::is_same_v<T, double>) {
            typeId = OMNI_DOUBLE;
        } else if constexpr (std::is_same_v<T, bool>) {
            typeId = OMNI_BOOLEAN;
        } else {
            typeId = OMNI_INT;
        }

        auto *leftElemVec = new Vector<T>(leftElemSize, typeId);
        for (int32_t i = 0; i < leftElemSize; ++i) {
            leftElemVec->SetValue(i, leftElems[i]);
        }
        auto *leftArr = new ArrayVector(rowSize, std::shared_ptr<BaseVector>(leftElemVec));
        for (size_t j = 0; j < leftOffsets.size(); ++j) {
            leftArr->SetOffset(j, leftOffsets[j]);
        }
        vectorBatch->Append(leftArr);

        auto *rightElemVec = new Vector<T>(rightElemSize, typeId);
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
};

/// Test arrays_overlap with int32 arrays - basic overlap
TEST_F(ArraysOverlapTest, IntArraysBasicOverlap)
{
    int rowSize = 3;

    // Row 0: [1,2,3] vs [3,4,5] -> true (element 3 overlaps)
    // Row 1: [1,2,3] vs [4,5,6] -> false (no overlap)
    // Row 2: [10,20] vs [20,30]  -> true (element 20 overlaps)
    int32_t leftElems[] = {1, 2, 3, 1, 2, 3, 10, 20};
    std::vector<int32_t> leftOffsets = {0, 3, 6, 8};
    int32_t rightElems[] = {3, 4, 5, 4, 5, 6, 20, 30};
    std::vector<int32_t> rightOffsets = {0, 3, 6, 8};

    auto input = CreateTwoArrayBatch<int32_t>(rowSize,
        8, leftElems, leftOffsets,
        8, rightElems, rightOffsets);

    auto expr = FuncExpr("arrays_overlap", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_ARRAY))
    }, std::make_shared<DataType>(OMNI_BOOLEAN));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultVec = dynamic_cast<Vector<bool> *>(result);
    ASSERT_NE(resultVec, nullptr);

    EXPECT_EQ(resultVec->GetValue(0), true);
    EXPECT_EQ(resultVec->GetValue(1), false);
    EXPECT_EQ(resultVec->GetValue(2), true);

    delete context;
    delete input;
    delete result;
}

/// Test arrays_overlap with empty arrays
TEST_F(ArraysOverlapTest, EmptyArrays)
{
    int rowSize = 2;

    // Row 0: [] vs [1,2] -> false
    // Row 1: [1,2] vs [] -> false
    int32_t leftElems[] = {1, 2};
    std::vector<int32_t> leftOffsets = {0, 0, 2};
    int32_t rightElems[] = {1, 2};
    std::vector<int32_t> rightOffsets = {0, 2, 2};

    auto input = CreateTwoArrayBatch<int32_t>(rowSize,
        2, leftElems, leftOffsets,
        2, rightElems, rightOffsets);

    auto expr = FuncExpr("arrays_overlap", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_ARRAY))
    }, std::make_shared<DataType>(OMNI_BOOLEAN));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultVec = dynamic_cast<Vector<bool> *>(result);
    ASSERT_NE(resultVec, nullptr);

    EXPECT_EQ(resultVec->GetValue(0), false);
    EXPECT_EQ(resultVec->GetValue(1), false);

    delete context;
    delete input;
    delete result;
}

/// Test arrays_overlap with null elements - should return null when no overlap but nulls exist
TEST_F(ArraysOverlapTest, NullElements)
{
    int rowSize = 2;

    // Row 0: [1, NULL, 3] vs [4, 5] -> with null element, no overlap -> null
    // Row 1: [1, NULL, 3] vs [1, 5] -> overlap on 1 -> true
    int32_t leftElems[] = {1, 0, 3, 1, 0, 3};
    std::vector<int32_t> leftOffsets = {0, 3, 6};
    int32_t rightElems[] = {4, 5, 1, 5};
    std::vector<int32_t> rightOffsets = {0, 2, 4};

    auto input = CreateTwoArrayBatch<int32_t>(rowSize,
        6, leftElems, leftOffsets,
        4, rightElems, rightOffsets);

    // Set null on element index 1 and 4 (the "0" placeholders)
    auto *leftArr = dynamic_cast<ArrayVector *>(input->Get(0));
    auto leftElemVec = leftArr->GetElementVector();
    leftElemVec->SetNull(1);
    leftElemVec->SetNull(4);

    auto expr = FuncExpr("arrays_overlap", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_ARRAY))
    }, std::make_shared<DataType>(OMNI_BOOLEAN));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultVec = dynamic_cast<Vector<bool> *>(result);
    ASSERT_NE(resultVec, nullptr);

    // Row 0: no non-null overlap but has null -> result should be null
    EXPECT_TRUE(resultVec->IsNull(0));
    // Row 1: overlap on element 1 -> true
    EXPECT_EQ(resultVec->GetValue(1), true);

    delete context;
    delete input;
    delete result;
}

/// Test arrays_overlap with null arrays
TEST_F(ArraysOverlapTest, NullArrays)
{
    int rowSize = 2;

    int32_t leftElems[] = {1, 2, 3, 4};
    std::vector<int32_t> leftOffsets = {0, 2, 4};
    int32_t rightElems[] = {1, 2, 3, 4};
    std::vector<int32_t> rightOffsets = {0, 2, 4};

    auto input = CreateTwoArrayBatch<int32_t>(rowSize,
        4, leftElems, leftOffsets,
        4, rightElems, rightOffsets);

    // Set entire left array as null for row 0
    auto *leftArr = dynamic_cast<ArrayVector *>(input->Get(0));
    leftArr->SetNull(0);

    auto expr = FuncExpr("arrays_overlap", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_ARRAY))
    }, std::make_shared<DataType>(OMNI_BOOLEAN));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultVec = dynamic_cast<Vector<bool> *>(result);
    ASSERT_NE(resultVec, nullptr);

    // Row 0: left array is null -> result null
    EXPECT_TRUE(resultVec->IsNull(0));
    // Row 1: [3,4] vs [3,4] -> true
    EXPECT_EQ(resultVec->GetValue(1), true);

    delete context;
    delete input;
    delete result;
}

/// Test arrays_overlap with int8 (byte) arrays
TEST_F(ArraysOverlapTest, ByteArrays)
{
    int rowSize = 2;

    int8_t leftElems[] = {1, 2, 3, 4, 5};
    std::vector<int32_t> leftOffsets = {0, 3, 5};
    int8_t rightElems[] = {3, 4, 6, 7};
    std::vector<int32_t> rightOffsets = {0, 2, 4};

    auto input = CreateTwoArrayBatch<int8_t>(rowSize,
        5, leftElems, leftOffsets,
        4, rightElems, rightOffsets);

    auto expr = FuncExpr("arrays_overlap", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_ARRAY))
    }, std::make_shared<DataType>(OMNI_BOOLEAN));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultVec = dynamic_cast<Vector<bool> *>(result);
    ASSERT_NE(resultVec, nullptr);

    // Row 0: [1,2,3] vs [3,4] -> true
    EXPECT_EQ(resultVec->GetValue(0), true);
    // Row 1: [4,5] vs [6,7] -> false
    EXPECT_EQ(resultVec->GetValue(1), false);

    delete context;
    delete input;
    delete result;
}

/// Test arrays_overlap with int16 (short) arrays
TEST_F(ArraysOverlapTest, ShortArrays)
{
    int rowSize = 2;

    int16_t leftElems[] = {100, 200, 300, 400};
    std::vector<int32_t> leftOffsets = {0, 2, 4};
    int16_t rightElems[] = {200, 500, 600, 700};
    std::vector<int32_t> rightOffsets = {0, 2, 4};

    auto input = CreateTwoArrayBatch<int16_t>(rowSize,
        4, leftElems, leftOffsets,
        4, rightElems, rightOffsets);

    auto expr = FuncExpr("arrays_overlap", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_ARRAY))
    }, std::make_shared<DataType>(OMNI_BOOLEAN));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultVec = dynamic_cast<Vector<bool> *>(result);
    ASSERT_NE(resultVec, nullptr);

    // Row 0: [100,200] vs [200,500] -> true
    EXPECT_EQ(resultVec->GetValue(0), true);
    // Row 1: [300,400] vs [600,700] -> false
    EXPECT_EQ(resultVec->GetValue(1), false);

    delete context;
    delete input;
    delete result;
}

/// Test arrays_overlap with int64 (long) arrays
TEST_F(ArraysOverlapTest, LongArrays)
{
    int rowSize = 2;

    int64_t leftElems[] = {100000, 200000, 300000, 400000};
    std::vector<int32_t> leftOffsets = {0, 2, 4};
    int64_t rightElems[] = {200000, 500000, 600000, 700000};
    std::vector<int32_t> rightOffsets = {0, 2, 4};

    auto input = CreateTwoArrayBatch<int64_t>(rowSize,
        4, leftElems, leftOffsets,
        4, rightElems, rightOffsets);

    auto expr = FuncExpr("arrays_overlap", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_ARRAY))
    }, std::make_shared<DataType>(OMNI_BOOLEAN));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultVec = dynamic_cast<Vector<bool> *>(result);
    ASSERT_NE(resultVec, nullptr);

    EXPECT_EQ(resultVec->GetValue(0), true);
    EXPECT_EQ(resultVec->GetValue(1), false);

    delete context;
    delete input;
    delete result;
}

/// Test arrays_overlap with float arrays
TEST_F(ArraysOverlapTest, FloatArrays)
{
    int rowSize = 2;

    float leftElems[] = {1.5f, 2.5f, 3.5f, 4.5f};
    std::vector<int32_t> leftOffsets = {0, 2, 4};
    float rightElems[] = {2.5f, 5.5f, 6.5f, 7.5f};
    std::vector<int32_t> rightOffsets = {0, 2, 4};

    auto input = CreateTwoArrayBatch<float>(rowSize,
        4, leftElems, leftOffsets,
        4, rightElems, rightOffsets);

    auto expr = FuncExpr("arrays_overlap", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_ARRAY))
    }, std::make_shared<DataType>(OMNI_BOOLEAN));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultVec = dynamic_cast<Vector<bool> *>(result);
    ASSERT_NE(resultVec, nullptr);

    // Row 0: [1.5, 2.5] vs [2.5, 5.5] -> true
    EXPECT_EQ(resultVec->GetValue(0), true);
    // Row 1: [3.5, 4.5] vs [6.5, 7.5] -> false
    EXPECT_EQ(resultVec->GetValue(1), false);

    delete context;
    delete input;
    delete result;
}

/// Test arrays_overlap with double arrays and NaN handling
TEST_F(ArraysOverlapTest, DoubleArraysWithNaN)
{
    int rowSize = 3;

    constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
    constexpr double kInf = std::numeric_limits<double>::infinity();

    // Row 0: [1.0, NaN] vs [NaN, 2.0] -> true (NaN == NaN)
    // Row 1: [1.0, 2.0] vs [3.0, 4.0] -> false
    // Row 2: [kInf, 1.0] vs [kInf, 2.0] -> true (Inf == Inf)
    double leftElems[] = {1.0, kNaN, 1.0, 2.0, kInf, 1.0};
    std::vector<int32_t> leftOffsets = {0, 2, 4, 6};
    double rightElems[] = {kNaN, 2.0, 3.0, 4.0, kInf, 2.0};
    std::vector<int32_t> rightOffsets = {0, 2, 4, 6};

    auto input = CreateTwoArrayBatch<double>(rowSize,
        6, leftElems, leftOffsets,
        6, rightElems, rightOffsets);

    auto expr = FuncExpr("arrays_overlap", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_ARRAY))
    }, std::make_shared<DataType>(OMNI_BOOLEAN));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultVec = dynamic_cast<Vector<bool> *>(result);
    ASSERT_NE(resultVec, nullptr);

    EXPECT_EQ(resultVec->GetValue(0), true);
    EXPECT_EQ(resultVec->GetValue(1), false);
    EXPECT_EQ(resultVec->GetValue(2), true);

    delete context;
    delete input;
    delete result;
}

/// Test arrays_overlap with boolean arrays
TEST_F(ArraysOverlapTest, BooleanArrays)
{
    int rowSize = 3;

    // Row 0: [true, false] vs [true] -> true
    // Row 1: [true, true] vs [false] -> false
    // Row 2: [false] vs [false] -> true
    bool leftElems[] = {true, false, true, true, false};
    std::vector<int32_t> leftOffsets = {0, 2, 4, 5};
    bool rightElems[] = {true, false, false};
    std::vector<int32_t> rightOffsets = {0, 1, 2, 3};

    auto input = CreateTwoArrayBatch<bool>(rowSize,
        5, leftElems, leftOffsets,
        3, rightElems, rightOffsets);

    auto expr = FuncExpr("arrays_overlap", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_ARRAY))
    }, std::make_shared<DataType>(OMNI_BOOLEAN));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultVec = dynamic_cast<Vector<bool> *>(result);
    ASSERT_NE(resultVec, nullptr);

    EXPECT_EQ(resultVec->GetValue(0), true);
    EXPECT_EQ(resultVec->GetValue(1), false);
    EXPECT_EQ(resultVec->GetValue(2), true);

    delete context;
    delete input;
    delete result;
}

/// Test arrays_overlap with varchar (string) arrays
TEST_F(ArraysOverlapTest, VarcharArrays)
{
    int rowSize = 3;

    using VarcharVector = Vector<LargeStringContainer<std::string_view>>;

    auto *vectorBatch = new VectorBatch(rowSize);

    // Left arrays: ["a","b","c"], ["d","e"], ["hello","world"]
    auto *leftElemVec = new VarcharVector(7);
    leftElemVec->SetValue(0, std::string_view("a"));
    leftElemVec->SetValue(1, std::string_view("b"));
    leftElemVec->SetValue(2, std::string_view("c"));
    leftElemVec->SetValue(3, std::string_view("d"));
    leftElemVec->SetValue(4, std::string_view("e"));
    leftElemVec->SetValue(5, std::string_view("hello"));
    leftElemVec->SetValue(6, std::string_view("world"));
    auto *leftArr = new ArrayVector(rowSize, std::shared_ptr<BaseVector>(leftElemVec));
    std::vector<int32_t> leftOffsets = {0, 3, 5, 7};
    for (size_t j = 0; j < leftOffsets.size(); ++j) {
        leftArr->SetOffset(j, leftOffsets[j]);
    }
    vectorBatch->Append(leftArr);

    // Right arrays: ["c","d"], ["f","g"], ["world","test"]
    auto *rightElemVec = new VarcharVector(6);
    rightElemVec->SetValue(0, std::string_view("c"));
    rightElemVec->SetValue(1, std::string_view("d"));
    rightElemVec->SetValue(2, std::string_view("f"));
    rightElemVec->SetValue(3, std::string_view("g"));
    rightElemVec->SetValue(4, std::string_view("world"));
    rightElemVec->SetValue(5, std::string_view("test"));
    auto *rightArr = new ArrayVector(rowSize, std::shared_ptr<BaseVector>(rightElemVec));
    std::vector<int32_t> rightOffsets = {0, 2, 4, 6};
    for (size_t j = 0; j < rightOffsets.size(); ++j) {
        rightArr->SetOffset(j, rightOffsets[j]);
    }
    vectorBatch->Append(rightArr);

    auto expr = FuncExpr("arrays_overlap", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_ARRAY))
    }, std::make_shared<DataType>(OMNI_BOOLEAN));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(vectorBatch, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultVec = dynamic_cast<Vector<bool> *>(result);
    ASSERT_NE(resultVec, nullptr);

    // Row 0: ["a","b","c"] vs ["c","d"] -> true
    EXPECT_EQ(resultVec->GetValue(0), true);
    // Row 1: ["d","e"] vs ["f","g"] -> false
    EXPECT_EQ(resultVec->GetValue(1), false);
    // Row 2: ["hello","world"] vs ["world","test"] -> true
    EXPECT_EQ(resultVec->GetValue(2), true);

    delete context;
    delete vectorBatch;
    delete result;
}

/// Test arrays_overlap with negative numbers
TEST_F(ArraysOverlapTest, NegativeNumbers)
{
    int rowSize = 2;

    int32_t leftElems[] = {-1, -2, -3, -4};
    std::vector<int32_t> leftOffsets = {0, 2, 4};
    int32_t rightElems[] = {-2, -5, -6, -3};
    std::vector<int32_t> rightOffsets = {0, 2, 4};

    auto input = CreateTwoArrayBatch<int32_t>(rowSize,
        4, leftElems, leftOffsets,
        4, rightElems, rightOffsets);

    auto expr = FuncExpr("arrays_overlap", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_ARRAY))
    }, std::make_shared<DataType>(OMNI_BOOLEAN));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultVec = dynamic_cast<Vector<bool> *>(result);
    ASSERT_NE(resultVec, nullptr);

    // Row 0: [-1,-2] vs [-2,-5] -> true (-2 overlaps)
    EXPECT_EQ(resultVec->GetValue(0), true);
    // Row 1: [-3,-4] vs [-6,-3] -> true (-3 overlaps)
    EXPECT_EQ(resultVec->GetValue(1), true);

    delete context;
    delete input;
    delete result;
}

/// Test arrays_overlap with boundary values
TEST_F(ArraysOverlapTest, BoundaryValues)
{
    int rowSize = 2;

    constexpr int32_t kMin = std::numeric_limits<int32_t>::min();
    constexpr int32_t kMax = std::numeric_limits<int32_t>::max();

    int32_t leftElems[] = {kMin, 0, kMax, 1};
    std::vector<int32_t> leftOffsets = {0, 2, 4};
    int32_t rightElems[] = {kMax, kMin, 2, 3};
    std::vector<int32_t> rightOffsets = {0, 2, 4};

    auto input = CreateTwoArrayBatch<int32_t>(rowSize,
        4, leftElems, leftOffsets,
        4, rightElems, rightOffsets);

    auto expr = FuncExpr("arrays_overlap", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_ARRAY))
    }, std::make_shared<DataType>(OMNI_BOOLEAN));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultVec = dynamic_cast<Vector<bool> *>(result);
    ASSERT_NE(resultVec, nullptr);

    // Row 0: [kMin, 0] vs [kMax, kMin] -> true (kMin overlaps)
    EXPECT_EQ(resultVec->GetValue(0), true);
    // Row 1: [kMax, 1] vs [2, 3] -> false
    EXPECT_EQ(resultVec->GetValue(1), false);

    delete context;
    delete input;
    delete result;
}

/// Test arrays_overlap with single element arrays
TEST_F(ArraysOverlapTest, SingleElementArrays)
{
    int rowSize = 2;

    int32_t leftElems[] = {42, 100};
    std::vector<int32_t> leftOffsets = {0, 1, 2};
    int32_t rightElems[] = {42, 200};
    std::vector<int32_t> rightOffsets = {0, 1, 2};

    auto input = CreateTwoArrayBatch<int32_t>(rowSize,
        2, leftElems, leftOffsets,
        2, rightElems, rightOffsets);

    auto expr = FuncExpr("arrays_overlap", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_ARRAY))
    }, std::make_shared<DataType>(OMNI_BOOLEAN));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultVec = dynamic_cast<Vector<bool> *>(result);
    ASSERT_NE(resultVec, nullptr);

    // Row 0: [42] vs [42] -> true
    EXPECT_EQ(resultVec->GetValue(0), true);
    // Row 1: [100] vs [200] -> false
    EXPECT_EQ(resultVec->GetValue(1), false);

    delete context;
    delete input;
    delete result;
}

/// Test arrays_overlap with duplicate elements
TEST_F(ArraysOverlapTest, DuplicateElements)
{
    int rowSize = 2;

    int32_t leftElems[] = {1, 1, 1, 2, 2, 2};
    std::vector<int32_t> leftOffsets = {0, 3, 6};
    int32_t rightElems[] = {1, 1, 3, 3};
    std::vector<int32_t> rightOffsets = {0, 2, 4};

    auto input = CreateTwoArrayBatch<int32_t>(rowSize,
        6, leftElems, leftOffsets,
        4, rightElems, rightOffsets);

    auto expr = FuncExpr("arrays_overlap", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_ARRAY))
    }, std::make_shared<DataType>(OMNI_BOOLEAN));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultVec = dynamic_cast<Vector<bool> *>(result);
    ASSERT_NE(resultVec, nullptr);

    // Row 0: [1,1,1] vs [1,1] -> true
    EXPECT_EQ(resultVec->GetValue(0), true);
    // Row 1: [2,2,2] vs [3,3] -> false
    EXPECT_EQ(resultVec->GetValue(1), false);

    delete context;
    delete input;
    delete result;
}

/// Test arrays_overlap with float arrays and NaN
TEST_F(ArraysOverlapTest, FloatArraysWithNaN)
{
    int rowSize = 2;

    constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();

    // Row 0: [NaN, 1.0f] vs [NaN, 2.0f] -> true (NaN == NaN)
    // Row 1: [1.0f, 2.0f] vs [3.0f, 4.0f] -> false
    float leftElems[] = {kNaN, 1.0f, 1.0f, 2.0f};
    std::vector<int32_t> leftOffsets = {0, 2, 4};
    float rightElems[] = {kNaN, 2.0f, 3.0f, 4.0f};
    std::vector<int32_t> rightOffsets = {0, 2, 4};

    auto input = CreateTwoArrayBatch<float>(rowSize,
        4, leftElems, leftOffsets,
        4, rightElems, rightOffsets);

    auto expr = FuncExpr("arrays_overlap", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_ARRAY))
    }, std::make_shared<DataType>(OMNI_BOOLEAN));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultVec = dynamic_cast<Vector<bool> *>(result);
    ASSERT_NE(resultVec, nullptr);

    EXPECT_EQ(resultVec->GetValue(0), true);
    EXPECT_EQ(resultVec->GetValue(1), false);

    delete context;
    delete input;
    delete result;
}

/// Test arrays_overlap with both arrays having null elements and no overlap
TEST_F(ArraysOverlapTest, BothArraysNullElementsNoOverlap)
{
    int rowSize = 1;

    // Row 0: [NULL, 1] vs [NULL, 2] -> null (both have nulls but no non-null overlap)
    int32_t leftElems[] = {0, 1};
    std::vector<int32_t> leftOffsets = {0, 2};
    int32_t rightElems[] = {0, 2};
    std::vector<int32_t> rightOffsets = {0, 2};

    auto input = CreateTwoArrayBatch<int32_t>(rowSize,
        2, leftElems, leftOffsets,
        2, rightElems, rightOffsets);

    auto *leftArr = dynamic_cast<ArrayVector *>(input->Get(0));
    auto *rightArr = dynamic_cast<ArrayVector *>(input->Get(1));
    leftArr->GetElementVector()->SetNull(0);
    rightArr->GetElementVector()->SetNull(0);

    auto expr = FuncExpr("arrays_overlap", {
        new FieldExpr(0, std::make_shared<DataType>(OMNI_ARRAY)),
        new FieldExpr(1, std::make_shared<DataType>(OMNI_ARRAY))
    }, std::make_shared<DataType>(OMNI_BOOLEAN));

    auto context = new ExecutionContext(4096, nullptr);
    context->SetResultRowSize(rowSize);

    ExprEval e(input, context);
    e.VisitExpr(expr);
    auto result = e.GetResult();

    auto resultVec = dynamic_cast<Vector<bool> *>(result);
    ASSERT_NE(resultVec, nullptr);

    EXPECT_TRUE(resultVec->IsNull(0));

    delete context;
    delete input;
    delete result;
}
