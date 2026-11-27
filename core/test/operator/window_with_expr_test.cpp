/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd
 * OmniOperator is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include "gtest/gtest.h"
#include "operator/window/window_expr.h"
#include "util/test_util.h"
#include "vector/vector_helper.h"
#include "util/config_util.h"

using namespace std;
using namespace omniruntime::vec;
using namespace omniruntime::TestUtil;
using namespace omniruntime::type;

namespace WindowWithExprTest {
const int32_t DATA_SIZE = 6;
const int32_t SMALL_DATA_SIZE = 4;
const uint64_t MAX_SPILL_BYTES = (5L << 20);

TEST(NativeOmniWindowWithExprOperatorTest, testMaxWithExpr)
{
    using namespace omniruntime::op;
    using namespace omniruntime::expressions;

    // construct input data
    DataTypes sourceTypes(std::vector<DataTypePtr>({IntType(), LongType(), DoubleType(), ShortType()}));
    int32_t data0[DATA_SIZE] = {0, 1, 2, 0, 1, 2};
    int64_t data1[DATA_SIZE] = {8, 1, 2, 8, 4, 5};
    double data2[DATA_SIZE] = {6.6, 5.5, 4.4, 3.3, 2.2, 1.1};
    int16_t data3[DATA_SIZE] = {6, 5, 4, 3, 2, 1};
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, DATA_SIZE, data0, data1, data2, data3);

    int32_t outputCols[4] = {0, 1, 2, 3};
    int32_t sortCols[1] = {1};
    int32_t ascendings[1] = {false};
    int32_t nullFirsts[1] = {false};
    int32_t windowFunctionTypes[1] = {OMNI_AGGREGATION_TYPE_MAX};
    int32_t windowFrameTypes[1] = {OMNI_FRAME_TYPE_RANGE};
    int32_t windowFrameStartTypes[1] = {OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING};
    int32_t windowFrameStartChannels[1] = {-1};
    int32_t windowFrameEndTypes[1] = {OMNI_FRAME_BOUND_CURRENT_ROW};
    int32_t windowFrameEndChannels[1] = {-1};
    int32_t partitionCols[1] = {0};
    int32_t preGroupedCols[0] = {};

    int32_t preSortedChannelPrefix = 0;
    int32_t expectedPositions = 10000;

    DataTypes outputTypes(std::vector<DataTypePtr>({DoubleType()}));

    std::string argumentChannels[1] = {"ADD:3(#2, 50:3)"};

    // create expression objects
    Parser parser;
    std::vector<Expr *> argumentChannelsExprs = parser.ParseExpressions(argumentChannels, 1, sourceTypes);
    // dealing data with the operator
    WindowWithExprOperatorFactory *operatorFactory = WindowWithExprOperatorFactory::CreateWindowWithExprOperatorFactory(
        sourceTypes, outputCols, 4, windowFunctionTypes, 1, partitionCols, 1, preGroupedCols, 0, sortCols, ascendings,
        nullFirsts, 1, preSortedChannelPrefix, expectedPositions, outputTypes, argumentChannelsExprs, 1,
        windowFrameTypes, windowFrameStartTypes, windowFrameStartChannels, windowFrameEndTypes, windowFrameEndChannels);

    auto *windowOperator = dynamic_cast<WindowWithExprOperator *>(CreateTestOperator(operatorFactory));

    windowOperator->AddInput(vecBatch);
    windowOperator->noMoreInput();
    VectorBatch *outputVecBatch = nullptr;
    windowOperator->GetOutput(&outputVecBatch);

    // construct the output data
    DataTypes expectTypes(std::vector<DataTypePtr>({IntType(), LongType(), DoubleType(), ShortType(), DoubleType()}));
    int32_t expectData1[DATA_SIZE] = {0, 0, 1, 1, 2, 2};
    int64_t expectData2[DATA_SIZE] = {8, 8, 4, 1, 5, 2};
    double expectData3[DATA_SIZE] = {6.6, 3.3, 2.2, 5.5, 1.1, 4.4};
    int16_t expectData4[DATA_SIZE] = {6, 3, 2, 5, 1, 4};
    double expectData5[DATA_SIZE] = {56.6, 56.6, 52.2, 55.5, 51.1, 54.4};
    VectorBatch *expectVecBatch =
        CreateVectorBatch(expectTypes, DATA_SIZE, expectData1, expectData2, expectData3, expectData4, expectData5);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecBatch));

    Expr::DeleteExprs(argumentChannelsExprs);
    omniruntime::op::Operator::DeleteOperator(windowOperator);
    delete operatorFactory;
    VectorHelper::FreeVecBatch(expectVecBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
}

TEST(NativeOmniWindowWithExprOperatorTest, testRowNumberPartition)
{
    // construct the input data
    DataTypes sourceTypes(std::vector<DataTypePtr>({IntType(), LongType(), DoubleType()}));
    int32_t data0[DATA_SIZE] = {0, 1, 2, 0, 1, 2};
    int64_t data1[DATA_SIZE] = {0, 1, 2, 3, 4, 5};
    double data2[DATA_SIZE] = {6.6, 5.5, 4.4, 3.3, 2.2, 1.1};
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, DATA_SIZE, data0, data1, data2);

    int32_t outputCols[3] = {0, 1, 2};
    int32_t sortCols[1] = {1};
    int32_t ascendings[1] = {false};
    int32_t nullFirsts[1] = {false};
    int32_t windowFunctionTypes[1] = {OMNI_WINDOW_TYPE_ROW_NUMBER};
    int32_t windowFrameTypes[1] = {OMNI_FRAME_TYPE_RANGE};
    int32_t windowFrameStartTypes[1] = {OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING};
    int32_t windowFrameStartChannels[1] = {-1};
    int32_t windowFrameEndTypes[1] = {OMNI_FRAME_BOUND_CURRENT_ROW};
    int32_t windowFrameEndChannels[1] = {-1};
    int32_t partitionCols[1] = {0};
    int32_t preGroupedCols[0] = {};

    int32_t preSortedChannelPrefix = 0;
    int32_t expectedPositions = 10000;

    DataTypes outputTypes(std::vector<DataTypePtr>({LongType()}));
    std::vector<Expr *> argumentChannelsExprs = {};
    // dealing data with the operator
    WindowWithExprOperatorFactory *operatorFactory = WindowWithExprOperatorFactory::CreateWindowWithExprOperatorFactory(
        sourceTypes, outputCols, 3, windowFunctionTypes, 1, partitionCols, 1, preGroupedCols, 0, sortCols, ascendings,
        nullFirsts, 1, preSortedChannelPrefix, expectedPositions, outputTypes, argumentChannelsExprs, 0,
        windowFrameTypes, windowFrameStartTypes, windowFrameStartChannels, windowFrameEndTypes, windowFrameEndChannels);

    auto windowOperator = dynamic_cast<WindowWithExprOperator *>(CreateTestOperator(operatorFactory));

    windowOperator->AddInput(vecBatch);
    windowOperator->noMoreInput();
    VectorBatch *outputVecBatch = nullptr;
    windowOperator->GetOutput(&outputVecBatch);

    // construct the output data
    DataTypes expectTypes(std::vector<DataTypePtr>({IntType(), LongType(), DoubleType(), LongType()}));
    int32_t expectData1[DATA_SIZE] = {0, 0, 1, 1, 2, 2};
    int64_t expectData2[DATA_SIZE] = {3, 0, 4, 1, 5, 2};
    double expectData3[DATA_SIZE] = {3.3, 6.6, 2.2, 5.5, 1.1, 4.4};
    int64_t expectData4[DATA_SIZE] = {1, 2, 1, 2, 1, 2};
    VectorBatch *expectVecBatch =
        CreateVectorBatch(expectTypes, DATA_SIZE, expectData1, expectData2, expectData3, expectData4);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecBatch));

    Expr::DeleteExprs(argumentChannelsExprs);
    omniruntime::op::Operator::DeleteOperator(windowOperator);
    delete operatorFactory;
    VectorHelper::FreeVecBatch(expectVecBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
}

TEST(NativeOmniWindowWithExprOperatorTest, testRowNumber)
{
    // construct the input data
    DataTypes sourceTypes(std::vector<DataTypePtr>({IntType(), LongType(), DoubleType()}));
    int32_t data0[DATA_SIZE] = {0, 1, 2, 0, 1, 2};
    int64_t data1[DATA_SIZE] = {0, 1, 2, 3, 4, 5};
    double data2[DATA_SIZE] = {6.6, 5.5, 4.4, 3.3, 2.2, 1.1};
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, DATA_SIZE, data0, data1, data2);

    int32_t outputCols[2] = {2, 1};
    int32_t sortCols[0] = {};
    int32_t ascendings[0] = {};
    int32_t nullFirsts[0] = {};
    int32_t windowFunctionTypes[1] = {OMNI_WINDOW_TYPE_ROW_NUMBER};
    int32_t windowFrameTypes[1] = {OMNI_FRAME_TYPE_RANGE};
    int32_t windowFrameStartTypes[1] = {OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING};
    int32_t windowFrameStartChannels[1] = {-1};
    int32_t windowFrameEndTypes[1] = {OMNI_FRAME_BOUND_CURRENT_ROW};
    int32_t windowFrameEndChannels[1] = {-1};
    int32_t partitionCols[1] = {2};
    int32_t preGroupedCols[0] = {};

    int32_t preSortedChannelPrefix = 0;
    int32_t expectedPositions = 10000;

    DataTypes outputTypes(std::vector<DataTypePtr>({LongType()}));
    std::vector<Expr *> argumentChannelsExprs = {};
    // dealing data with the operator
    WindowWithExprOperatorFactory *operatorFactory = WindowWithExprOperatorFactory::CreateWindowWithExprOperatorFactory(
        sourceTypes, outputCols, 2, windowFunctionTypes, 1, partitionCols, 1, preGroupedCols, 0, sortCols, ascendings,
        nullFirsts, 0, preSortedChannelPrefix, expectedPositions, outputTypes, argumentChannelsExprs, 0,
        windowFrameTypes, windowFrameStartTypes, windowFrameStartChannels, windowFrameEndTypes, windowFrameEndChannels);

    auto test = dynamic_cast<WindowWithExprOperator *>(CreateTestOperator(operatorFactory));
    WindowWithExprOperator *windowOperator = test;
    windowOperator->AddInput(vecBatch);
    windowOperator->noMoreInput();
    VectorBatch *outputVecBatch = nullptr;
    windowOperator->GetOutput(&outputVecBatch);

    // construct the output data
    DataTypes expectTypes(std::vector<DataTypePtr>({DoubleType(), LongType(), LongType()}));
    double expectData1[DATA_SIZE] = {1.1, 2.2, 3.3, 4.4, 5.5, 6.6};
    int64_t expectData2[DATA_SIZE] = {5, 4, 3, 2, 1, 0};
    int64_t expectData3[DATA_SIZE] = {1, 1, 1, 1, 1, 1};
    VectorBatch *expectVecBatch = CreateVectorBatch(expectTypes, DATA_SIZE, expectData1, expectData2, expectData3);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecBatch));

    Expr::DeleteExprs(argumentChannelsExprs);
    omniruntime::op::Operator::DeleteOperator(windowOperator);
    delete operatorFactory;
    VectorHelper::FreeVecBatch(expectVecBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
}

TEST(NativeOmniWindowWithExprOperatorTest, testRankPartition)
{
    // construct the input data
    DataTypes sourceTypes(std::vector<DataTypePtr>({IntType(), LongType(), DoubleType()}));
    int32_t data0[DATA_SIZE] = {0, 1, 2, 0, 1, 2};
    int64_t data1[DATA_SIZE] = {8, 1, 2, 8, 4, 5};
    double data2[DATA_SIZE] = {6.6, 5.5, 4.4, 3.3, 2.2, 1.1};
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, DATA_SIZE, data0, data1, data2);

    int32_t outputCols[3] = {0, 1, 2};
    int32_t sortCols[1] = {1};
    int32_t ascendings[1] = {false};
    int32_t nullFirsts[1] = {false};
    int32_t windowFunctionTypes[1] = {OMNI_WINDOW_TYPE_RANK};
    int32_t windowFrameTypes[1] = {OMNI_FRAME_TYPE_RANGE};
    int32_t windowFrameStartTypes[1] = {OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING};
    int32_t windowFrameStartChannels[1] = {-1};
    int32_t windowFrameEndTypes[1] = {OMNI_FRAME_BOUND_CURRENT_ROW};
    int32_t windowFrameEndChannels[1] = {-1};
    int32_t partitionCols[1] = {0};
    int32_t preGroupedCols[0] = {};

    int32_t preSortedChannelPrefix = 0;
    int32_t expectedPositions = 10000;

    DataTypes outputTypes(std::vector<DataTypePtr>({LongType()}));
    std::vector<Expr *> argumentChannelsExprs = {};
    // dealing data with the operator
    WindowWithExprOperatorFactory *operatorFactory = WindowWithExprOperatorFactory::CreateWindowWithExprOperatorFactory(
        sourceTypes, outputCols, 3, windowFunctionTypes, 1, partitionCols, 1, preGroupedCols, 0, sortCols, ascendings,
        nullFirsts, 1, preSortedChannelPrefix, expectedPositions, outputTypes, argumentChannelsExprs, 0,
        windowFrameTypes, windowFrameStartTypes, windowFrameStartChannels, windowFrameEndTypes, windowFrameEndChannels);
    auto windowOperator = dynamic_cast<WindowWithExprOperator *>(CreateTestOperator(operatorFactory));

    windowOperator->AddInput(vecBatch);
    windowOperator->noMoreInput();
    VectorBatch *outputVecBatch = nullptr;
    windowOperator->GetOutput(&outputVecBatch);

    // construct the output data
    DataTypes expectTypes(std::vector<DataTypePtr>({IntType(), LongType(), DoubleType(), LongType()}));
    int32_t expectData1[DATA_SIZE] = {0, 0, 1, 1, 2, 2};
    int64_t expectData2[DATA_SIZE] = {8, 8, 4, 1, 5, 2};
    double expectData3[DATA_SIZE] = {6.6, 3.3, 2.2, 5.5, 1.1, 4.4};
    int64_t expectData4[DATA_SIZE] = {1, 1, 1, 2, 1, 2};
    VectorBatch *expectVecBatch =
        CreateVectorBatch(expectTypes, DATA_SIZE, expectData1, expectData2, expectData3, expectData4);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecBatch));

    Expr::DeleteExprs(argumentChannelsExprs);
    omniruntime::op::Operator::DeleteOperator(windowOperator);
    delete operatorFactory;
    VectorHelper::FreeVecBatch(expectVecBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
}

TEST(NativeOmniWindowWithExprOperatorTest, testRank)
{
    // construct the input data
    DataTypes sourceTypes(std::vector<DataTypePtr>({IntType(), LongType(), DoubleType()}));
    int32_t data0[DATA_SIZE] = {0, 1, 2, 0, 1, 2};
    int64_t data1[DATA_SIZE] = {8, 1, 2, 8, 4, 5};
    double data2[DATA_SIZE] = {6.6, 5.5, 4.4, 3.3, 2.2, 1.1};
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, DATA_SIZE, data0, data1, data2);

    int32_t outputCols[3] = {1, 2, 0};
    int32_t sortCols[1] = {1};
    int32_t ascendings[1] = {false};
    int32_t nullFirsts[1] = {false};
    int32_t windowFunctionTypes[1] = {OMNI_WINDOW_TYPE_RANK};
    int32_t windowFrameTypes[1] = {OMNI_FRAME_TYPE_RANGE};
    int32_t windowFrameStartTypes[1] = {OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING};
    int32_t windowFrameStartChannels[1] = {-1};
    int32_t windowFrameEndTypes[1] = {OMNI_FRAME_BOUND_CURRENT_ROW};
    int32_t windowFrameEndChannels[1] = {-1};
    int32_t partitionCols[0] = {};
    int32_t preGroupedCols[0] = {};

    int32_t preSortedChannelPrefix = 0;
    int32_t expectedPositions = 10000;

    DataTypes outputTypes(std::vector<DataTypePtr>({LongType()}));
    std::vector<Expr *> argumentChannelsExprs = {};
    // dealing data with the operator
    WindowWithExprOperatorFactory *operatorFactory = WindowWithExprOperatorFactory::CreateWindowWithExprOperatorFactory(
        sourceTypes, outputCols, 3, windowFunctionTypes, 1, partitionCols, 0, preGroupedCols, 0, sortCols, ascendings,
        nullFirsts, 1, preSortedChannelPrefix, expectedPositions, outputTypes, argumentChannelsExprs, 0,
        windowFrameTypes, windowFrameStartTypes, windowFrameStartChannels, windowFrameEndTypes, windowFrameEndChannels);
    auto windowOperator = dynamic_cast<WindowWithExprOperator *>(CreateTestOperator(operatorFactory));

    windowOperator->AddInput(vecBatch);
    windowOperator->noMoreInput();
    VectorBatch *outputVecBatch = nullptr;
    windowOperator->GetOutput(&outputVecBatch);

    // construct the output data
    DataTypes expectTypes(std::vector<DataTypePtr>({LongType(), DoubleType(), IntType(), LongType()}));
    int64_t expectData1[DATA_SIZE] = {8, 8, 5, 4, 2, 1};
    double expectData2[DATA_SIZE] = {6.6, 3.3, 1.1, 2.2, 4.4, 5.5};
    int32_t expectData3[DATA_SIZE] = {0, 0, 2, 1, 2, 1};
    int64_t expectData4[DATA_SIZE] = {1, 1, 3, 4, 5, 6};
    VectorBatch *expectVecBatch =
        CreateVectorBatch(expectTypes, DATA_SIZE, expectData1, expectData2, expectData3, expectData4);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecBatch));

    Expr::DeleteExprs(argumentChannelsExprs);
    omniruntime::op::Operator::DeleteOperator(windowOperator);
    delete operatorFactory;
    VectorHelper::FreeVecBatch(expectVecBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
}

TEST(NativeOmniWindowWithExprOperatorTest, testRowNumberAndRankPartition)
{
    // construct the input data
    DataTypes sourceTypes(std::vector<DataTypePtr>({IntType(), LongType(), DoubleType()}));
    int32_t data0[DATA_SIZE] = {0, 1, 2, 0, 1, 2};
    int64_t data1[DATA_SIZE] = {8, 1, 2, 8, 4, 5};
    double data2[DATA_SIZE] = {6.6, 5.5, 4.4, 3.3, 2.2, 1.1};
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, DATA_SIZE, data0, data1, data2);

    int32_t outputCols[3] = {0, 1, 2};
    int32_t sortCols[1] = {1};
    int32_t ascendings[1] = {false};
    int32_t nullFirsts[1] = {false};
    int32_t windowFunctionTypes[2] = {OMNI_WINDOW_TYPE_RANK, OMNI_WINDOW_TYPE_ROW_NUMBER};
    int32_t windowFrameTypes[2] = {OMNI_FRAME_TYPE_RANGE, OMNI_FRAME_TYPE_RANGE};
    int32_t windowFrameStartTypes[2] = {OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING, OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING};
    int32_t windowFrameStartChannels[2] = {-1, -1};
    int32_t windowFrameEndTypes[2] = {OMNI_FRAME_BOUND_CURRENT_ROW, OMNI_FRAME_BOUND_CURRENT_ROW};
    int32_t windowFrameEndChannels[2] = {-1, -1};
    int32_t partitionCols[1] = {0};
    int32_t preGroupedCols[0] = {};

    int32_t preSortedChannelPrefix = 0;
    int32_t expectedPositions = 10000;

    DataTypes outputTypes(std::vector<DataTypePtr>({LongType(), LongType()}));
    string argumentChannels[2] = {"-1", "-1"};

    // create expression objects
    Parser parser;
    std::vector<Expr *> argumentChannelsExprs = parser.ParseExpressions(argumentChannels, 2, sourceTypes);
    // dealing data with the operator
    WindowWithExprOperatorFactory *operatorFactory = WindowWithExprOperatorFactory::CreateWindowWithExprOperatorFactory(
        sourceTypes, outputCols, 3, windowFunctionTypes, 2, partitionCols, 1, preGroupedCols, 0, sortCols, ascendings,
        nullFirsts, 1, preSortedChannelPrefix, expectedPositions, outputTypes, argumentChannelsExprs, 0,
        windowFrameTypes, windowFrameStartTypes, windowFrameStartChannels, windowFrameEndTypes, windowFrameEndChannels);

    auto windowOperator = dynamic_cast<WindowWithExprOperator *>(CreateTestOperator(operatorFactory));

    windowOperator->AddInput(vecBatch);
    windowOperator->noMoreInput();
    VectorBatch *outputVecBatch = nullptr;
    windowOperator->GetOutput(&outputVecBatch);

    // construct the output data
    DataTypes expectTypes(std::vector<DataTypePtr>({IntType(), LongType(), DoubleType(), LongType(), LongType()}));
    int32_t expectData1[DATA_SIZE] = {0, 0, 1, 1, 2, 2};
    int64_t expectData2[DATA_SIZE] = {8, 8, 4, 1, 5, 2};
    double expectData3[DATA_SIZE] = {6.6, 3.3, 2.2, 5.5, 1.1, 4.4};
    int64_t expectData4[DATA_SIZE] = {1, 1, 1, 2, 1, 2};
    int64_t expectData5[DATA_SIZE] = {1, 2, 1, 2, 1, 2};
    VectorBatch *expectVecBatch =
        CreateVectorBatch(expectTypes, DATA_SIZE, expectData1, expectData2, expectData3, expectData4, expectData5);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecBatch));

    Expr::DeleteExprs(argumentChannelsExprs);
    omniruntime::op::Operator::DeleteOperator(windowOperator);
    delete operatorFactory;
    VectorHelper::FreeVecBatch(expectVecBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
}

TEST(NativeOmniWindowWithExprOperatorTest, testRowNumberAndRankPartitionWithNull)
{
    // construct the input data
    DataTypes sourceTypes(std::vector<DataTypePtr>({IntType(), LongType(), DoubleType()}));
    int32_t data0[DATA_SIZE] = {0, 1, 2, 0, 1, 2};
    int64_t data1[DATA_SIZE] = {8, 1, 2, 8, 4, 5};
    double data2[DATA_SIZE] = {6.6, 5.5, 4.4, 3.3, 2.2, 1.1};
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, DATA_SIZE, data0, data1, data2);
    vecBatch->Get(0)->SetNull(1);
    vecBatch->Get(0)->SetNull(5);

    int32_t outputCols[3] = {0, 1, 2};
    int32_t sortCols[1] = {1};
    int32_t ascendings[1] = {false};
    int32_t nullFirsts[1] = {false};
    int32_t windowFunctionTypes[2] = {OMNI_WINDOW_TYPE_RANK, OMNI_WINDOW_TYPE_ROW_NUMBER};
    int32_t windowFrameTypes[2] = {OMNI_FRAME_TYPE_RANGE, OMNI_FRAME_TYPE_RANGE};
    int32_t windowFrameStartTypes[2] = {OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING, OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING};
    int32_t windowFrameStartChannels[2] = {-1, -1};
    int32_t windowFrameEndTypes[2] = {OMNI_FRAME_BOUND_CURRENT_ROW, OMNI_FRAME_BOUND_CURRENT_ROW};
    int32_t windowFrameEndChannels[2] = {-1, -1};
    int32_t partitionCols[1] = {0};
    int32_t preGroupedCols[0] = {};

    int32_t preSortedChannelPrefix = 0;
    int32_t expectedPositions = 10000;

    DataTypes outputTypes(std::vector<DataTypePtr>({LongType(), LongType()}));
    string argumentChannels[2] = {"-1", "-1"};

    // create expression objects
    Parser parser;
    std::vector<Expr *> argumentChannelsExprs = parser.ParseExpressions(argumentChannels, 2, sourceTypes);
    // dealing data with the operator
    WindowWithExprOperatorFactory *operatorFactory = WindowWithExprOperatorFactory::CreateWindowWithExprOperatorFactory(
        sourceTypes, outputCols, 3, windowFunctionTypes, 2, partitionCols, 1, preGroupedCols, 0, sortCols, ascendings,
        nullFirsts, 1, preSortedChannelPrefix, expectedPositions, outputTypes, argumentChannelsExprs, 0,
        windowFrameTypes, windowFrameStartTypes, windowFrameStartChannels, windowFrameEndTypes, windowFrameEndChannels);
    auto windowOperator = dynamic_cast<WindowWithExprOperator *>(CreateTestOperator(operatorFactory));

    windowOperator->AddInput(vecBatch);
    windowOperator->noMoreInput();
    VectorBatch *outputVecBatch = nullptr;
    windowOperator->GetOutput(&outputVecBatch);

    // construct the output data
    DataTypes expectTypes(std::vector<DataTypePtr>({IntType(), LongType(), DoubleType(), LongType(), LongType()}));
    int32_t expectData1[DATA_SIZE] = {0, 0, 1, 2, 2, 1};
    int64_t expectData2[DATA_SIZE] = {8, 8, 4, 2, 5, 1};
    double expectData3[DATA_SIZE] = {6.6, 3.3, 2.2, 4.4, 1.1, 5.5};
    int64_t expectData4[DATA_SIZE] = {1, 1, 1, 1, 1, 2};
    int64_t expectData5[DATA_SIZE] = {1, 2, 1, 1, 1, 2};
    VectorBatch *expectVecBatch =
        CreateVectorBatch(expectTypes, DATA_SIZE, expectData1, expectData2, expectData3, expectData4, expectData5);
    expectVecBatch->Get(0)->SetNull(4);
    expectVecBatch->Get(0)->SetNull(5);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecBatch));

    Expr::DeleteExprs(argumentChannelsExprs);
    omniruntime::op::Operator::DeleteOperator(windowOperator);
    delete operatorFactory;
    VectorHelper::FreeVecBatch(expectVecBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
}

TEST(NativeOmniWindowWithExprOperatorTest, testRankWithAllDataTypes)
{
    // construct the input data
    DataTypes sourceTypes(
        std::vector<DataTypePtr>({IntType(), Date32Type(DAY), Date32Type(MILLI), LongType(), Decimal64Type(1, 1),
                                  DoubleType(), BooleanType(), VarcharType(3), Decimal128Type(2, 2)}));
    int32_t data0[DATA_SIZE] = {1, 1, 2, 2, 3, 3};
    int32_t data1[DATA_SIZE] = {11, 11, 22, 22, 33, 33};
    int32_t data2[DATA_SIZE] = {111, 111, 222, 222, 333, 333};
    int64_t data3[DATA_SIZE] = {1111, 1111, 2222, 2222, 3333, 3333};
    int64_t data4[DATA_SIZE] = {11111, 11111, 22222, 22222, 33333, 33333};
    double data5[DATA_SIZE] = {1.1, 1.1, 2.2, 2.2, 3.3, 3.3};
    bool data6[DATA_SIZE] = {false, false, true, true, false, false};
    std::string data7[DATA_SIZE] = {"s1", "s1", "s2", "s2", "s3", "s3"};
    Decimal128 data8[DATA_SIZE] = {111111, 111111, 222222, 222222, 333333, 333333};

    VectorBatch *vecBatch =
        CreateVectorBatch(sourceTypes, DATA_SIZE, data0, data1, data2, data3, data4, data5, data6, data7, data8);

    int32_t outputCols[9] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
    int32_t sortCols[1] = {0};
    int32_t ascendings[1] = {false};
    int32_t nullFirsts[1] = {false};
    int32_t windowFunctionTypes[9] = {OMNI_WINDOW_TYPE_RANK, OMNI_WINDOW_TYPE_RANK, OMNI_WINDOW_TYPE_RANK,
                                      OMNI_WINDOW_TYPE_RANK, OMNI_WINDOW_TYPE_RANK, OMNI_WINDOW_TYPE_RANK,
                                      OMNI_WINDOW_TYPE_RANK, OMNI_WINDOW_TYPE_RANK, OMNI_WINDOW_TYPE_RANK};
    int32_t windowFrameTypes[9] = {OMNI_FRAME_TYPE_RANGE, OMNI_FRAME_TYPE_RANGE, OMNI_FRAME_TYPE_RANGE,
                                   OMNI_FRAME_TYPE_RANGE, OMNI_FRAME_TYPE_RANGE, OMNI_FRAME_TYPE_RANGE,
                                   OMNI_FRAME_TYPE_RANGE, OMNI_FRAME_TYPE_RANGE, OMNI_FRAME_TYPE_RANGE};
    int32_t windowFrameStartTypes[9] = {OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING, OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING,
                                        OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING, OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING,
                                        OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING, OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING,
                                        OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING, OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING,
                                        OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING};
    int32_t windowFrameStartChannels[9] = {-1, -1, -1, -1, -1, -1, -1, -1, -1};
    int32_t windowFrameEndTypes[9] = {
        OMNI_FRAME_BOUND_CURRENT_ROW, OMNI_FRAME_BOUND_CURRENT_ROW, OMNI_FRAME_BOUND_CURRENT_ROW,
        OMNI_FRAME_BOUND_CURRENT_ROW, OMNI_FRAME_BOUND_CURRENT_ROW, OMNI_FRAME_BOUND_CURRENT_ROW,
        OMNI_FRAME_BOUND_CURRENT_ROW, OMNI_FRAME_BOUND_CURRENT_ROW, OMNI_FRAME_BOUND_CURRENT_ROW};
    int32_t windowFrameEndChannels[9] = {-1, -1, -1, -1, -1, -1, -1, -1, -1};
    int32_t partitionCols[1] = {0};
    int32_t preGroupedCols[0] = {};

    int32_t preSortedChannelPrefix = 0;
    int32_t expectedPositions = 10000;

    DataTypes outputTypes(std::vector<DataTypePtr>(
        {LongType(), LongType(), LongType(), LongType(), LongType(), LongType(), LongType(), LongType(), LongType()}));
    std::vector<Expr *> argumentChannelsExprs = {};
    // dealing data with the operator
    WindowWithExprOperatorFactory *operatorFactory = WindowWithExprOperatorFactory::CreateWindowWithExprOperatorFactory(
        sourceTypes, outputCols, 9, windowFunctionTypes, 9, partitionCols, 1, preGroupedCols, 0, sortCols, ascendings,
        nullFirsts, 1, preSortedChannelPrefix, expectedPositions, outputTypes, argumentChannelsExprs, 0,
        windowFrameTypes, windowFrameStartTypes, windowFrameStartChannels, windowFrameEndTypes, windowFrameEndChannels);
    auto windowOperator = dynamic_cast<WindowWithExprOperator *>(CreateTestOperator(operatorFactory));

    windowOperator->AddInput(vecBatch);
    windowOperator->noMoreInput();
    VectorBatch *outputVecBatch = nullptr;
    windowOperator->GetOutput(&outputVecBatch);

    // construct the output data
    DataTypes expectTypes(std::vector<DataTypePtr>(
        {IntType(), Date32Type(omniruntime::type::DAY), Date32Type(omniruntime::type::MILLI), LongType(),
         Decimal64Type(1, 1), DoubleType(), BooleanType(), VarcharType(3), Decimal128Type(2, 2), LongType(), LongType(),
         LongType(), LongType(), LongType(), LongType(), LongType(), LongType(), LongType()}));
    int32_t expectData0[DATA_SIZE] = {1, 1, 2, 2, 3, 3};
    int32_t expectData1[DATA_SIZE] = {11, 11, 22, 22, 33, 33};
    int32_t expectData2[DATA_SIZE] = {111, 111, 222, 222, 333, 333};
    int64_t expectData3[DATA_SIZE] = {1111, 1111, 2222, 2222, 3333, 3333};
    int64_t expectData4[DATA_SIZE] = {11111, 11111, 22222, 22222, 33333, 33333};
    double expectData5[DATA_SIZE] = {1.1, 1.1, 2.2, 2.2, 3.3, 3.3};
    bool expectData6[DATA_SIZE] = {false, false, true, true, false, false};
    std::string expectData7[DATA_SIZE] = {"s1", "s1", "s2", "s2", "s3", "s3"};
    Decimal128 expectData8[DATA_SIZE] = {111111, 111111, 222222, 222222, 333333, 333333};
    int64_t expectData9[DATA_SIZE] = {1, 1, 1, 1, 1, 1};
    int64_t expectData10[DATA_SIZE] = {1, 1, 1, 1, 1, 1};
    int64_t expectData11[DATA_SIZE] = {1, 1, 1, 1, 1, 1};
    int64_t expectData12[DATA_SIZE] = {1, 1, 1, 1, 1, 1};
    int64_t expectData13[DATA_SIZE] = {1, 1, 1, 1, 1, 1};
    int64_t expectData14[DATA_SIZE] = {1, 1, 1, 1, 1, 1};
    int64_t expectData15[DATA_SIZE] = {1, 1, 1, 1, 1, 1};
    int64_t expectData16[DATA_SIZE] = {1, 1, 1, 1, 1, 1};
    int64_t expectData17[DATA_SIZE] = {1, 1, 1, 1, 1, 1};

    VectorBatch *expectVecBatch =
        CreateVectorBatch(expectTypes, DATA_SIZE, expectData0, expectData1, expectData2, expectData3, expectData4,
                          expectData5, expectData6, expectData7, expectData8, expectData9, expectData10, expectData11,
                          expectData12, expectData13, expectData14, expectData15, expectData16, expectData17);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecBatch));

    Expr::DeleteExprs(argumentChannelsExprs);
    omniruntime::op::Operator::DeleteOperator(windowOperator);
    delete operatorFactory;
    VectorHelper::FreeVecBatch(expectVecBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
}

TEST(NativeOmniWindowWithExprOperatorTest, testRowNumberkWithAllDataTypes)
{
    // construct the input data
    DataTypes sourceTypes(
        std::vector<DataTypePtr>({IntType(), Date32Type(DAY), Date32Type(MILLI), LongType(), Decimal64Type(1, 1),
                                  DoubleType(), BooleanType(), VarcharType(3), Decimal128Type(2, 2)}));
    int32_t data0[DATA_SIZE] = {1, 1, 2, 2, 3, 3};
    int32_t data1[DATA_SIZE] = {11, 11, 22, 22, 33, 33};
    int32_t data2[DATA_SIZE] = {111, 111, 222, 222, 333, 333};
    int64_t data3[DATA_SIZE] = {1111, 1111, 2222, 2222, 3333, 3333};
    int64_t data4[DATA_SIZE] = {11111, 11111, 22222, 22222, 33333, 33333};
    double data5[DATA_SIZE] = {1.1, 1.1, 2.2, 2.2, 3.3, 3.3};
    bool data6[DATA_SIZE] = {false, false, true, true, false, false};
    std::string data7[DATA_SIZE] = {"s1", "s1", "s2", "s2", "s3", "s3"};
    Decimal128 data8[DATA_SIZE] = {111111, 111111, 222222, 222222, 333333, 333333};

    VectorBatch *vecBatch =
        CreateVectorBatch(sourceTypes, DATA_SIZE, data0, data1, data2, data3, data4, data5, data6, data7, data8);

    int32_t outputCols[9] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
    int32_t sortCols[1] = {0};
    int32_t ascendings[1] = {false};
    int32_t nullFirsts[1] = {false};
    int32_t windowFunctionTypes[9] = {
        OMNI_WINDOW_TYPE_ROW_NUMBER, OMNI_WINDOW_TYPE_ROW_NUMBER, OMNI_WINDOW_TYPE_ROW_NUMBER,
        OMNI_WINDOW_TYPE_ROW_NUMBER, OMNI_WINDOW_TYPE_ROW_NUMBER, OMNI_WINDOW_TYPE_ROW_NUMBER,
        OMNI_WINDOW_TYPE_ROW_NUMBER, OMNI_WINDOW_TYPE_ROW_NUMBER, OMNI_WINDOW_TYPE_ROW_NUMBER};
    int32_t windowFrameTypes[9] = {OMNI_FRAME_TYPE_RANGE, OMNI_FRAME_TYPE_RANGE, OMNI_FRAME_TYPE_RANGE,
                                   OMNI_FRAME_TYPE_RANGE, OMNI_FRAME_TYPE_RANGE, OMNI_FRAME_TYPE_RANGE,
                                   OMNI_FRAME_TYPE_RANGE, OMNI_FRAME_TYPE_RANGE, OMNI_FRAME_TYPE_RANGE};
    int32_t windowFrameStartTypes[9] = {OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING, OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING,
                                        OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING, OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING,
                                        OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING, OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING,
                                        OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING, OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING,
                                        OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING};
    int32_t windowFrameStartChannels[9] = {-1, -1, -1, -1, -1, -1, -1, -1, -1};
    int32_t windowFrameEndTypes[9] = {
        OMNI_FRAME_BOUND_CURRENT_ROW, OMNI_FRAME_BOUND_CURRENT_ROW, OMNI_FRAME_BOUND_CURRENT_ROW,
        OMNI_FRAME_BOUND_CURRENT_ROW, OMNI_FRAME_BOUND_CURRENT_ROW, OMNI_FRAME_BOUND_CURRENT_ROW,
        OMNI_FRAME_BOUND_CURRENT_ROW, OMNI_FRAME_BOUND_CURRENT_ROW, OMNI_FRAME_BOUND_CURRENT_ROW};
    int32_t windowFrameEndChannels[9] = {-1, -1, -1, -1, -1, -1, -1, -1, -1};
    int32_t partitionCols[1] = {0};
    int32_t preGroupedCols[0] = {};

    int32_t preSortedChannelPrefix = 0;
    int32_t expectedPositions = 10000;

    DataTypes outputTypes(std::vector<DataTypePtr>(
        {LongType(), LongType(), LongType(), LongType(), LongType(), LongType(), LongType(), LongType(), LongType()}));
    std::vector<Expr *> argumentChannelsExprs = {};
    // dealing data with the operator
    WindowWithExprOperatorFactory *operatorFactory = WindowWithExprOperatorFactory::CreateWindowWithExprOperatorFactory(
        sourceTypes, outputCols, 9, windowFunctionTypes, 9, partitionCols, 1, preGroupedCols, 0, sortCols, ascendings,
        nullFirsts, 1, preSortedChannelPrefix, expectedPositions, outputTypes, argumentChannelsExprs, 0,
        windowFrameTypes, windowFrameStartTypes, windowFrameStartChannels, windowFrameEndTypes, windowFrameEndChannels);
    auto windowOperator = dynamic_cast<WindowWithExprOperator *>(CreateTestOperator(operatorFactory));

    windowOperator->AddInput(vecBatch);
    windowOperator->noMoreInput();
    VectorBatch *outputVecBatch = nullptr;
    windowOperator->GetOutput(&outputVecBatch);

    // construct the output data
    DataTypes expectTypes(std::vector<DataTypePtr>(
        {IntType(), Date32Type(DAY), Date32Type(MILLI), LongType(), Decimal64Type(1, 1), DoubleType(), BooleanType(),
         VarcharType(3), Decimal128Type(2, 2), LongType(), LongType(), LongType(), LongType(), LongType(), LongType(),
         LongType(), LongType(), LongType()}));
    int32_t expectData0[DATA_SIZE] = {1, 1, 2, 2, 3, 3};
    int32_t expectData1[DATA_SIZE] = {11, 11, 22, 22, 33, 33};
    int32_t expectData2[DATA_SIZE] = {111, 111, 222, 222, 333, 333};
    int64_t expectData3[DATA_SIZE] = {1111, 1111, 2222, 2222, 3333, 3333};
    int64_t expectData4[DATA_SIZE] = {11111, 11111, 22222, 22222, 33333, 33333};
    double expectData5[DATA_SIZE] = {1.1, 1.1, 2.2, 2.2, 3.3, 3.3};
    bool expectData6[DATA_SIZE] = {false, false, true, true, false, false};
    std::string expectData7[DATA_SIZE] = {"s1", "s1", "s2", "s2", "s3", "s3"};
    Decimal128 expectData8[DATA_SIZE] = {111111, 111111, 222222, 222222, 333333, 333333};
    int64_t expectData9[DATA_SIZE] = {1, 2, 1, 2, 1, 2};
    int64_t expectData10[DATA_SIZE] = {1, 2, 1, 2, 1, 2};
    int64_t expectData11[DATA_SIZE] = {1, 2, 1, 2, 1, 2};
    int64_t expectData12[DATA_SIZE] = {1, 2, 1, 2, 1, 2};
    int64_t expectData13[DATA_SIZE] = {1, 2, 1, 2, 1, 2};
    int64_t expectData14[DATA_SIZE] = {1, 2, 1, 2, 1, 2};
    int64_t expectData15[DATA_SIZE] = {1, 2, 1, 2, 1, 2};
    int64_t expectData16[DATA_SIZE] = {1, 2, 1, 2, 1, 2};
    int64_t expectData17[DATA_SIZE] = {1, 2, 1, 2, 1, 2};

    VectorBatch *expectVecBatch =
        CreateVectorBatch(expectTypes, DATA_SIZE, expectData0, expectData1, expectData2, expectData3, expectData4,
                          expectData5, expectData6, expectData7, expectData8, expectData9, expectData10, expectData11,
                          expectData12, expectData13, expectData14, expectData15, expectData16, expectData17);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecBatch));

    Expr::DeleteExprs(argumentChannelsExprs);
    omniruntime::op::Operator::DeleteOperator(windowOperator);
    delete operatorFactory;
    VectorHelper::FreeVecBatch(expectVecBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
}

TEST(NativeOmniWindowWithExprOperatorTest, testSumWithAllDataTypes)
{
    ConfigUtil::SetSupportContainerVecRule(SupportContainerVecRule::SUPPORT);

    // construct the input data
    DataTypes sourceTypes(
        std::vector<DataTypePtr>({IntType(), Date32Type(DAY), Date32Type(MILLI), LongType(), Decimal64Type(5, 0),
                                  DoubleType(), BooleanType(), VarcharType(3), Decimal128Type(2, 2)}));
    int32_t data0[DATA_SIZE] = {1, 1, 2, 2, 3, 3};
    int32_t data1[DATA_SIZE] = {11, 11, 22, 22, 33, 33};
    int32_t data2[DATA_SIZE] = {111, 111, 222, 222, 333, 333};
    int64_t data3[DATA_SIZE] = {1111, 1111, 2222, 2222, 3333, 3333};
    int64_t data4[DATA_SIZE] = {11111, 11111, 22222, 22222, 33333, 33333};
    double data5[DATA_SIZE] = {1.1, 1.1, 2.2, 2.2, 3.3, 3.3};
    bool data6[DATA_SIZE] = {false, false, true, true, false, false};
    std::string data7[DATA_SIZE] = {"s1", "s1", "s2", "s2", "s3", "s3"};
    Decimal128 data8[DATA_SIZE] = {Decimal128(1, 1), Decimal128(1, 1), Decimal128(2, 2),
                                   Decimal128(2, 2), Decimal128(3, 3), Decimal128(3, 3)};

    VectorBatch *vecBatch =
        CreateVectorBatch(sourceTypes, DATA_SIZE, data0, data1, data2, data3, data4, data5, data6, data7, data8);

    int32_t outputCols[9] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
    int32_t sortCols[1] = {0};
    int32_t ascendings[1] = {false};
    int32_t nullFirsts[1] = {false};
    int32_t windowFunctionTypes[7] = {OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_SUM,
                                      OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_SUM,
                                      OMNI_AGGREGATION_TYPE_SUM};
    int32_t windowFrameTypes[7] = {OMNI_FRAME_TYPE_RANGE, OMNI_FRAME_TYPE_RANGE, OMNI_FRAME_TYPE_RANGE,
                                   OMNI_FRAME_TYPE_RANGE, OMNI_FRAME_TYPE_RANGE, OMNI_FRAME_TYPE_RANGE,
                                   OMNI_FRAME_TYPE_RANGE};
    int32_t windowFrameStartTypes[7] = {OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING, OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING,
                                        OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING, OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING,
                                        OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING, OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING,
                                        OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING};
    int32_t windowFrameStartChannels[7] = {-1, -1, -1, -1, -1, -1, -1};
    int32_t windowFrameEndTypes[7] = {OMNI_FRAME_BOUND_CURRENT_ROW, OMNI_FRAME_BOUND_CURRENT_ROW,
                                      OMNI_FRAME_BOUND_CURRENT_ROW, OMNI_FRAME_BOUND_CURRENT_ROW,
                                      OMNI_FRAME_BOUND_CURRENT_ROW, OMNI_FRAME_BOUND_CURRENT_ROW,
                                      OMNI_FRAME_BOUND_CURRENT_ROW};
    int32_t windowFrameEndChannels[7] = {-1, -1, -1, -1, -1, -1, -1};
    int32_t partitionCols[1] = {0};
    int32_t preGroupedCols[0] = {};

    int32_t preSortedChannelPrefix = 0;
    int32_t expectedPositions = 10000;

    DataTypes outputTypes(std::vector<DataTypePtr>({IntType(), Date32Type(DAY), Date32Type(MILLI), LongType(),
                                                    Decimal128Type(10, 0), DoubleType(), Decimal128Type(4, 2)}));
    string argumentChannels[7] = {"ADD:1(2:1, #0)", "#1", "#2", "#3", "#4", "#5", "#8"};

    // create expression objects
    Parser parser;
    std::vector<Expr *> argumentChannelsExprs = parser.ParseExpressions(argumentChannels, 7, sourceTypes);
    // dealing data with the operator
    WindowWithExprOperatorFactory *operatorFactory = WindowWithExprOperatorFactory::CreateWindowWithExprOperatorFactory(
        sourceTypes, outputCols, 9, windowFunctionTypes, 7, partitionCols, 1, preGroupedCols, 0, sortCols, ascendings,
        nullFirsts, 1, preSortedChannelPrefix, expectedPositions, outputTypes, argumentChannelsExprs, 7,
        windowFrameTypes, windowFrameStartTypes, windowFrameStartChannels, windowFrameEndTypes, windowFrameEndChannels);

    auto windowOperator = dynamic_cast<WindowWithExprOperator *>(CreateTestOperator(operatorFactory));

    windowOperator->AddInput(vecBatch);
    windowOperator->noMoreInput();
    VectorBatch *outputVecBatch = nullptr;
    windowOperator->GetOutput(&outputVecBatch);

    // construct the output data
    DataTypes expectTypes(std::vector<DataTypePtr>(
        {IntType(), Date32Type(DAY), Date32Type(MILLI), LongType(), Decimal64Type(5, 0), DoubleType(), BooleanType(),
         VarcharType(3), Decimal128Type(2, 2), IntType(), Date32Type(DAY), Date32Type(MILLI), LongType(),
         Decimal128Type(10, 0), DoubleType(), Decimal128Type(4, 2)}));
    int32_t expectData0[DATA_SIZE] = {1, 1, 2, 2, 3, 3};
    int32_t expectData1[DATA_SIZE] = {11, 11, 22, 22, 33, 33};
    int32_t expectData2[DATA_SIZE] = {111, 111, 222, 222, 333, 333};
    int64_t expectData3[DATA_SIZE] = {1111, 1111, 2222, 2222, 3333, 3333};
    int64_t expectData4[DATA_SIZE] = {11111, 11111, 22222, 22222, 33333, 33333};
    double expectData5[DATA_SIZE] = {1.1, 1.1, 2.2, 2.2, 3.3, 3.3};
    bool expectData6[DATA_SIZE] = {false, false, true, true, false, false};
    std::string expectData7[DATA_SIZE] = {"s1", "s1", "s2", "s2", "s3", "s3"};
    Decimal128 expectData8[DATA_SIZE] = {Decimal128(1, 1), Decimal128(1, 1), Decimal128(2, 2),
                                         Decimal128(2, 2), Decimal128(3, 3), Decimal128(3, 3)};
    int32_t expectData10[DATA_SIZE] = {6, 6, 8, 8, 10, 10};
    int32_t expectData11[DATA_SIZE] = {22, 22, 44, 44, 66, 66};
    int32_t expectData12[DATA_SIZE] = {222, 222, 444, 444, 666, 666};
    int64_t expectData13[DATA_SIZE] = {2222, 2222, 4444, 4444, 6666, 6666};
    Decimal128 expectData14[DATA_SIZE] = {Decimal128(22222), Decimal128(22222), Decimal128(44444),
                                          Decimal128(44444), Decimal128(66666), Decimal128(66666)};
    double expectData15[DATA_SIZE] = {2.2, 2.2, 4.4, 4.4, 6.6, 6.6};
    Decimal128 expectData16[DATA_SIZE] = {Decimal128(2, 2), Decimal128(2, 2), Decimal128(4, 4),
                                          Decimal128(4, 4), Decimal128(6, 6), Decimal128(6, 6)};

    VectorBatch *expectVecBatch =
        CreateVectorBatch(expectTypes, DATA_SIZE, expectData0, expectData1, expectData2, expectData3, expectData4,
                          expectData5, expectData6, expectData7, expectData8, expectData10, expectData11, expectData12,
                          expectData13, expectData14, expectData15, expectData16);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecBatch));

    Expr::DeleteExprs(argumentChannelsExprs);
    omniruntime::op::Operator::DeleteOperator(windowOperator);
    delete operatorFactory;
    VectorHelper::FreeVecBatch(expectVecBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
}

TEST(NativeOmniWindowWithExprOperatorTest, testAvgWithAllDataTypes)
{
    ConfigUtil::SetSupportContainerVecRule(SupportContainerVecRule::SUPPORT);

    // construct the input data
    DataTypes sourceTypes(
        std::vector<DataTypePtr>({IntType(), Date32Type(DAY), Date32Type(MILLI), LongType(), Decimal64Type(5, 2),
                                  DoubleType(), BooleanType(), VarcharType(3), Decimal128Type(2, 2)}));
    int32_t data0[DATA_SIZE] = {1, 1, 2, 2, 3, 3};
    int32_t data1[DATA_SIZE] = {11, 33, 33, 55, 55, 77};
    int32_t data2[DATA_SIZE] = {111, 333, 333, 555, 555, 777};
    int64_t data3[DATA_SIZE] = {1111, 3333, 3333, 5555, 5555, 7777};
    int64_t data4[DATA_SIZE] = {11111, 33333, 33333, 55555, 55555, 77777};
    double data5[DATA_SIZE] = {1.1, 3.3, 3.3, 5.5, 5.5, 7.7};
    bool data6[DATA_SIZE] = {false, false, true, true, false, false};
    std::string data7[DATA_SIZE] = {"s1", "s1", "s2", "s2", "s3", "s3"};
    Decimal128 data8[DATA_SIZE] = {Decimal128(0, 0), Decimal128(0, 0), Decimal128(0, 0),
                                   Decimal128(0, 0), Decimal128(0, 0), Decimal128(0, 0)};

    VectorBatch *vecBatch =
        CreateVectorBatch(sourceTypes, DATA_SIZE, data0, data1, data2, data3, data4, data5, data6, data7, data8);

    int32_t outputCols[9] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
    int32_t sortCols[1] = {0};
    int32_t ascendings[1] = {false};
    int32_t nullFirsts[1] = {false};
    int32_t windowFunctionTypes[7] = {OMNI_AGGREGATION_TYPE_AVG, OMNI_AGGREGATION_TYPE_AVG, OMNI_AGGREGATION_TYPE_AVG,
                                      OMNI_AGGREGATION_TYPE_AVG, OMNI_AGGREGATION_TYPE_AVG, OMNI_AGGREGATION_TYPE_AVG,
                                      OMNI_AGGREGATION_TYPE_AVG};
    int32_t windowFrameTypes[7] = {OMNI_FRAME_TYPE_RANGE, OMNI_FRAME_TYPE_RANGE, OMNI_FRAME_TYPE_RANGE,
                                   OMNI_FRAME_TYPE_RANGE, OMNI_FRAME_TYPE_RANGE, OMNI_FRAME_TYPE_RANGE,
                                   OMNI_FRAME_TYPE_RANGE};
    int32_t windowFrameStartTypes[7] = {OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING, OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING,
                                        OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING, OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING,
                                        OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING, OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING,
                                        OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING};
    int32_t windowFrameStartChannels[7] = {-1, -1, -1, -1, -1, -1, -1};
    int32_t windowFrameEndTypes[7] = {OMNI_FRAME_BOUND_CURRENT_ROW, OMNI_FRAME_BOUND_CURRENT_ROW,
                                      OMNI_FRAME_BOUND_CURRENT_ROW, OMNI_FRAME_BOUND_CURRENT_ROW,
                                      OMNI_FRAME_BOUND_CURRENT_ROW, OMNI_FRAME_BOUND_CURRENT_ROW,
                                      OMNI_FRAME_BOUND_CURRENT_ROW};
    int32_t windowFrameEndChannels[7] = {-1, -1, -1, -1, -1, -1, -1};
    int32_t partitionCols[1] = {0};
    int32_t preGroupedCols[0] = {};

    int32_t preSortedChannelPrefix = 0;
    int32_t expectedPositions = 10000;

    DataTypes outputTypes(std::vector<DataTypePtr>({DoubleType(), DoubleType(), DoubleType(), DoubleType(),
                                                    Decimal64Type(10, 4), DoubleType(), Decimal128Type(4, 4)}));
    string argumentChannels[7] = {"#0", "ADD:1(2:8, #1)", "#2", "#3", "#4", "#5", "#8"};

    // create expression objects
    Parser parser;
    std::vector<Expr *> argumentChannelsExprs = parser.ParseExpressions(argumentChannels, 7, sourceTypes);
    // dealing data with the operator
    WindowWithExprOperatorFactory *operatorFactory = WindowWithExprOperatorFactory::CreateWindowWithExprOperatorFactory(
        sourceTypes, outputCols, 9, windowFunctionTypes, 7, partitionCols, 1, preGroupedCols, 0, sortCols, ascendings,
        nullFirsts, 1, preSortedChannelPrefix, expectedPositions, outputTypes, argumentChannelsExprs, 7,
        windowFrameTypes, windowFrameStartTypes, windowFrameStartChannels, windowFrameEndTypes, windowFrameEndChannels);
    auto windowOperator = dynamic_cast<WindowWithExprOperator *>(CreateTestOperator(operatorFactory));

    windowOperator->AddInput(vecBatch);
    windowOperator->noMoreInput();
    VectorBatch *outputVecBatch = nullptr;
    windowOperator->GetOutput(&outputVecBatch);

    // construct the output data
    DataTypes expectTypes(std::vector<DataTypePtr>(
        {IntType(), Date32Type(DAY), Date32Type(MILLI), LongType(), Decimal64Type(5, 2), DoubleType(), BooleanType(),
         VarcharType(3), Decimal128Type(10, 4), DoubleType(), DoubleType(), DoubleType(), DoubleType(),
         Decimal64Type(10, 4), DoubleType(), Decimal128Type(4, 4)}));
    int32_t expectData0[DATA_SIZE] = {1, 1, 2, 2, 3, 3};
    int32_t expectData1[DATA_SIZE] = {11, 33, 33, 55, 55, 77};
    int32_t expectData2[DATA_SIZE] = {111, 333, 333, 555, 555, 777};
    int64_t expectData3[DATA_SIZE] = {1111, 3333, 3333, 5555, 5555, 7777};
    int64_t expectData4[DATA_SIZE] = {11111, 33333, 33333, 55555, 55555, 77777};
    double expectData5[DATA_SIZE] = {1.1, 3.3, 3.3, 5.5, 5.5, 7.7};
    bool expectData6[DATA_SIZE] = {false, false, true, true, false, false};
    std::string expectData7[DATA_SIZE] = {"s1", "s1", "s2", "s2", "s3", "s3"};
    Decimal128 expectData8[DATA_SIZE] = {Decimal128(0, 0), Decimal128(0, 0), Decimal128(0, 0),
                                         Decimal128(0, 0), Decimal128(0, 0), Decimal128(0, 0)};
    double expectData10[DATA_SIZE] = {1, 1, 2, 2, 3, 3};
    double expectData11[DATA_SIZE] = {24, 24, 46, 46, 68, 68};
    double expectData12[DATA_SIZE] = {222, 222, 444, 444, 666, 666};
    double expectData13[DATA_SIZE] = {2222, 2222, 4444, 4444, 6666, 6666};
    int64_t expectData14[DATA_SIZE] = {2222200, 2222200, 4444400, 4444400, 6666600, 6666600};
    double expectData15[DATA_SIZE] = {2.2, 2.2, 4.4, 4.4, 6.6, 6.6};
    Decimal128 expectData16[DATA_SIZE] = {Decimal128(0, 0), Decimal128(0, 0), Decimal128(0, 0),
                                          Decimal128(0, 0), Decimal128(0, 0), Decimal128(0, 0)};
    VectorBatch *expectVecBatch =
        CreateVectorBatch(expectTypes, DATA_SIZE, expectData0, expectData1, expectData2, expectData3, expectData4,
                          expectData5, expectData6, expectData7, expectData8, expectData10, expectData11, expectData12,
                          expectData13, expectData14, expectData15, expectData16);
    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecBatch));

    Expr::DeleteExprs(argumentChannelsExprs);
    omniruntime::op::Operator::DeleteOperator(windowOperator);
    delete operatorFactory;
    VectorHelper::FreeVecBatch(expectVecBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
}

TEST(NativeOmniWindowWithExprOperatorTest, testMaxWithAllDataTypes)
{
    // construct the input data
    DataTypes sourceTypes(
        std::vector<DataTypePtr>({IntType(), Date32Type(DAY), Date32Type(MILLI), LongType(), Decimal64Type(1, 1),
                                  DoubleType(), BooleanType(), VarcharType(3), Decimal128Type(2, 2)}));
    int32_t data0[DATA_SIZE] = {1, 1, 2, 2, 3, 3};
    int32_t data1[DATA_SIZE] = {11, 33, 33, 55, 55, 77};
    int32_t data2[DATA_SIZE] = {111, 333, 333, 555, 555, 777};
    int64_t data3[DATA_SIZE] = {1111, 3333, 3333, 5555, 5555, 7777};
    int64_t data4[DATA_SIZE] = {11111, 33333, 33333, 55555, 55555, 77777};
    double data5[DATA_SIZE] = {1.1, 3.3, 3.3, 5.5, 5.5, 7.7};
    bool data6[DATA_SIZE] = {false, false, true, true, false, false};
    std::string data7[DATA_SIZE] = {"s1", "s3", "s3", "s5", "s5", "s7"};
    Decimal128 data8[DATA_SIZE] = {Decimal128(1, 1), Decimal128(3, 3), Decimal128(3, 3),
                                   Decimal128(5, 5), Decimal128(5, 5), Decimal128(7, 7)};

    VectorBatch *vecBatch =
        CreateVectorBatch(sourceTypes, DATA_SIZE, data0, data1, data2, data3, data4, data5, data6, data7, data8);

    int32_t outputCols[9] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
    int32_t sortCols[1] = {0};
    int32_t ascendings[1] = {false};
    int32_t nullFirsts[1] = {false};
    int32_t windowFunctionTypes[8] = {OMNI_AGGREGATION_TYPE_MAX, OMNI_AGGREGATION_TYPE_MAX, OMNI_AGGREGATION_TYPE_MAX,
                                      OMNI_AGGREGATION_TYPE_MAX, OMNI_AGGREGATION_TYPE_MAX, OMNI_AGGREGATION_TYPE_MAX,
                                      OMNI_AGGREGATION_TYPE_MAX, OMNI_AGGREGATION_TYPE_MAX};
    int32_t windowFrameTypes[8] = {OMNI_FRAME_TYPE_RANGE, OMNI_FRAME_TYPE_RANGE, OMNI_FRAME_TYPE_RANGE,
                                   OMNI_FRAME_TYPE_RANGE, OMNI_FRAME_TYPE_RANGE, OMNI_FRAME_TYPE_RANGE,
                                   OMNI_FRAME_TYPE_RANGE, OMNI_FRAME_TYPE_RANGE};
    int32_t windowFrameStartTypes[8] = {OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING, OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING,
                                        OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING, OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING,
                                        OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING, OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING,
                                        OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING, OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING};
    int32_t windowFrameStartChannels[8] = {-1, -1, -1, -1, -1, -1, -1, -1};
    int32_t windowFrameEndTypes[8] = {OMNI_FRAME_BOUND_CURRENT_ROW, OMNI_FRAME_BOUND_CURRENT_ROW,
                                      OMNI_FRAME_BOUND_CURRENT_ROW, OMNI_FRAME_BOUND_CURRENT_ROW,
                                      OMNI_FRAME_BOUND_CURRENT_ROW, OMNI_FRAME_BOUND_CURRENT_ROW,
                                      OMNI_FRAME_BOUND_CURRENT_ROW, OMNI_FRAME_BOUND_CURRENT_ROW};
    int32_t windowFrameEndChannels[8] = {-1, -1, -1, -1, -1, -1, -1, -1};
    int32_t partitionCols[1] = {0};
    int32_t preGroupedCols[0] = {};

    int32_t preSortedChannelPrefix = 0;
    int32_t expectedPositions = 10000;

    DataTypes outputTypes(
        std::vector<DataTypePtr>({IntType(), Date32Type(DAY), Date32Type(MILLI), LongType(), Decimal64Type(1, 1),
                                  DoubleType(), VarcharType(3), Decimal128Type(2, 2)}));
    string argumentChannels[8] = {"#0", "#1", "#2", "ADD:2(2:2, #3)", "#4", "#5", "#7", "#8"};

    // create expression objects
    Parser parser;
    std::vector<Expr *> argumentChannelsExprs = parser.ParseExpressions(argumentChannels, 8, sourceTypes);
    // dealing data with the operator
    WindowWithExprOperatorFactory *operatorFactory = WindowWithExprOperatorFactory::CreateWindowWithExprOperatorFactory(
        sourceTypes, outputCols, 9, windowFunctionTypes, 8, partitionCols, 1, preGroupedCols, 0, sortCols, ascendings,
        nullFirsts, 1, preSortedChannelPrefix, expectedPositions, outputTypes, argumentChannelsExprs, 8,
        windowFrameTypes, windowFrameStartTypes, windowFrameStartChannels, windowFrameEndTypes, windowFrameEndChannels);
    auto windowOperator = dynamic_cast<WindowWithExprOperator *>(CreateTestOperator(operatorFactory));

    windowOperator->AddInput(vecBatch);
    windowOperator->noMoreInput();
    VectorBatch *outputVecBatch = nullptr;
    windowOperator->GetOutput(&outputVecBatch);

    // construct the output data
    DataTypes expectTypes(std::vector<DataTypePtr>(
        {IntType(), Date32Type(DAY), Date32Type(MILLI), LongType(), Decimal64Type(1, 1), DoubleType(), BooleanType(),
         VarcharType(3), Decimal128Type(2, 2), IntType(), Date32Type(DAY), Date32Type(MILLI), LongType(),
         Decimal64Type(1, 1), DoubleType(), VarcharType(3), Decimal128Type(2, 2)}));
    int32_t expectData0[DATA_SIZE] = {1, 1, 2, 2, 3, 3};
    int32_t expectData1[DATA_SIZE] = {11, 33, 33, 55, 55, 77};
    int32_t expectData2[DATA_SIZE] = {111, 333, 333, 555, 555, 777};
    int64_t expectData3[DATA_SIZE] = {1111, 3333, 3333, 5555, 5555, 7777};
    int64_t expectData4[DATA_SIZE] = {11111, 33333, 33333, 55555, 55555, 77777};
    double expectData5[DATA_SIZE] = {1.1, 3.3, 3.3, 5.5, 5.5, 7.7};
    bool expectData6[DATA_SIZE] = {false, false, true, true, false, false};
    std::string expectData7[DATA_SIZE] = {"s1", "s3", "s3", "s5", "s5", "s7"};
    Decimal128 expectData8[DATA_SIZE] = {Decimal128(1, 1), Decimal128(3, 3), Decimal128(3, 3),
                                         Decimal128(5, 5), Decimal128(5, 5), Decimal128(7, 7)};
    int32_t expectData10[DATA_SIZE] = {1, 1, 2, 2, 3, 3};
    int32_t expectData11[DATA_SIZE] = {33, 33, 55, 55, 77, 77};
    int32_t expectData12[DATA_SIZE] = {333, 333, 555, 555, 777, 777};
    int64_t expectData13[DATA_SIZE] = {3335, 3335, 5557, 5557, 7779, 7779};
    int64_t expectData14[DATA_SIZE] = {33333, 33333, 55555, 55555, 77777, 77777};
    double expectData15[DATA_SIZE] = {3.3, 3.3, 5.5, 5.5, 7.7, 7.7};
    std::string expectData16[DATA_SIZE] = {"s3", "s3", "s5", "s5", "s7", "s7"};
    Decimal128 expectData17[DATA_SIZE] = {Decimal128(3, 3), Decimal128(3, 3), Decimal128(5, 5),
                                          Decimal128(5, 5), Decimal128(7, 7), Decimal128(7, 7)};
    VectorBatch *expectVecBatch =
        CreateVectorBatch(expectTypes, DATA_SIZE, expectData0, expectData1, expectData2, expectData3, expectData4,
                          expectData5, expectData6, expectData7, expectData8, expectData10, expectData11, expectData12,
                          expectData13, expectData14, expectData15, expectData16, expectData17);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecBatch));

    Expr::DeleteExprs(argumentChannelsExprs);
    omniruntime::op::Operator::DeleteOperator(windowOperator);
    delete operatorFactory;
    VectorHelper::FreeVecBatch(expectVecBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
}

TEST(NativeOmniWindowWithExprOperatorTest, testMinWithAllDataTypes)
{
    // construct the input data
    DataTypes sourceTypes(
        std::vector<DataTypePtr>({IntType(), Date32Type(DAY), Date32Type(MILLI), LongType(), Decimal64Type(1, 1),
                                  DoubleType(), BooleanType(), VarcharType(3), Decimal128Type(2, 2)}));
    int32_t data0[DATA_SIZE] = {1, 1, 2, 2, 3, 3};
    int32_t data1[DATA_SIZE] = {11, 33, 33, 55, 55, 77};
    int32_t data2[DATA_SIZE] = {111, 333, 333, 555, 555, 777};
    int64_t data3[DATA_SIZE] = {1111, 3333, 3333, 5555, 5555, 7777};
    int64_t data4[DATA_SIZE] = {11111, 33333, 33333, 55555, 55555, 77777};
    double data5[DATA_SIZE] = {1.1, 3.3, 3.3, 5.5, 5.5, 7.7};
    bool data6[DATA_SIZE] = {false, false, true, true, false, false};
    std::string data7[DATA_SIZE] = {"s1", "s3", "s3", "s5", "s5", "s7"};
    Decimal128 data8[DATA_SIZE] = {Decimal128(1, 1), Decimal128(3, 3), Decimal128(3, 3),
                                   Decimal128(5, 5), Decimal128(5, 5), Decimal128(7, 7)};

    VectorBatch *vecBatch =
        CreateVectorBatch(sourceTypes, DATA_SIZE, data0, data1, data2, data3, data4, data5, data6, data7, data8);

    int32_t outputCols[9] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
    int32_t sortCols[1] = {0};
    int32_t ascendings[1] = {false};
    int32_t nullFirsts[1] = {false};
    int32_t windowFunctionTypes[8] = {OMNI_AGGREGATION_TYPE_MIN, OMNI_AGGREGATION_TYPE_MIN, OMNI_AGGREGATION_TYPE_MIN,
                                      OMNI_AGGREGATION_TYPE_MIN, OMNI_AGGREGATION_TYPE_MIN, OMNI_AGGREGATION_TYPE_MIN,
                                      OMNI_AGGREGATION_TYPE_MIN, OMNI_AGGREGATION_TYPE_MIN};
    int32_t windowFrameTypes[8] = {OMNI_FRAME_TYPE_RANGE, OMNI_FRAME_TYPE_RANGE, OMNI_FRAME_TYPE_RANGE,
                                   OMNI_FRAME_TYPE_RANGE, OMNI_FRAME_TYPE_RANGE, OMNI_FRAME_TYPE_RANGE,
                                   OMNI_FRAME_TYPE_RANGE, OMNI_FRAME_TYPE_RANGE};
    int32_t windowFrameStartTypes[8] = {OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING, OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING,
                                        OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING, OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING,
                                        OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING, OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING,
                                        OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING, OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING};
    int32_t windowFrameStartChannels[8] = {-1, -1, -1, -1, -1, -1, -1, -1};
    int32_t windowFrameEndTypes[8] = {OMNI_FRAME_BOUND_CURRENT_ROW, OMNI_FRAME_BOUND_CURRENT_ROW,
                                      OMNI_FRAME_BOUND_CURRENT_ROW, OMNI_FRAME_BOUND_CURRENT_ROW,
                                      OMNI_FRAME_BOUND_CURRENT_ROW, OMNI_FRAME_BOUND_CURRENT_ROW,
                                      OMNI_FRAME_BOUND_CURRENT_ROW, OMNI_FRAME_BOUND_CURRENT_ROW};
    int32_t windowFrameEndChannels[8] = {-1, -1, -1, -1, -1, -1, -1, -1};
    int32_t partitionCols[1] = {0};
    int32_t preGroupedCols[0] = {};

    int32_t preSortedChannelPrefix = 0;
    int32_t expectedPositions = 10000;

    DataTypes outputTypes(
        std::vector<DataTypePtr>({IntType(), Date32Type(DAY), Date32Type(MILLI), LongType(), Decimal64Type(1, 1),
                                  DoubleType(), VarcharType(3), Decimal128Type(2, 2)}));
    string argumentChannels[8] = {"#0", "#1", "#2", "ADD:2(#3, 2:2)", "#4", "#5", "#7", "#8"};

    Parser parser;
    std::vector<Expr *> argumentChannelsExprs = parser.ParseExpressions(argumentChannels, 8, sourceTypes);
    // dealing data with the operator
    WindowWithExprOperatorFactory *operatorFactory = WindowWithExprOperatorFactory::CreateWindowWithExprOperatorFactory(
        sourceTypes, outputCols, 9, windowFunctionTypes, 8, partitionCols, 1, preGroupedCols, 0, sortCols, ascendings,
        nullFirsts, 1, preSortedChannelPrefix, expectedPositions, outputTypes, argumentChannelsExprs, 8,
        windowFrameTypes, windowFrameStartTypes, windowFrameStartChannels, windowFrameEndTypes, windowFrameEndChannels);
    auto windowOperator = dynamic_cast<WindowWithExprOperator *>(CreateTestOperator(operatorFactory));

    windowOperator->AddInput(vecBatch);
    windowOperator->noMoreInput();
    VectorBatch *outputVecBatch = nullptr;
    windowOperator->GetOutput(&outputVecBatch);

    // construct the output data
    DataTypes expectTypes(std::vector<DataTypePtr>(
        {IntType(), Date32Type(DAY), Date32Type(MILLI), LongType(), Decimal64Type(1, 1), DoubleType(), BooleanType(),
         VarcharType(3), Decimal128Type(2, 2), IntType(), Date32Type(DAY), Date32Type(MILLI), LongType(),
         Decimal64Type(1, 1), DoubleType(), VarcharType(3), Decimal128Type(2, 2)}));
    int32_t expectData0[DATA_SIZE] = {1, 1, 2, 2, 3, 3};
    int32_t expectData1[DATA_SIZE] = {11, 33, 33, 55, 55, 77};
    int32_t expectData2[DATA_SIZE] = {111, 333, 333, 555, 555, 777};
    int64_t expectData3[DATA_SIZE] = {1111, 3333, 3333, 5555, 5555, 7777};
    int64_t expectData4[DATA_SIZE] = {11111, 33333, 33333, 55555, 55555, 77777};
    double expectData5[DATA_SIZE] = {1.1, 3.3, 3.3, 5.5, 5.5, 7.7};
    bool expectData6[DATA_SIZE] = {false, false, true, true, false, false};
    std::string expectData7[DATA_SIZE] = {"s1", "s3", "s3", "s5", "s5", "s7"};
    Decimal128 expectData8[DATA_SIZE] = {Decimal128(1, 1), Decimal128(3, 3), Decimal128(3, 3),
                                         Decimal128(5, 5), Decimal128(5, 5), Decimal128(7, 7)};
    int32_t expectData10[DATA_SIZE] = {1, 1, 2, 2, 3, 3};
    int32_t expectData11[DATA_SIZE] = {11, 11, 33, 33, 55, 55};
    int32_t expectData12[DATA_SIZE] = {111, 111, 333, 333, 555, 555};
    int64_t expectData13[DATA_SIZE] = {1113, 1113, 3335, 3335, 5557, 5557};
    int64_t expectData14[DATA_SIZE] = {11111, 11111, 33333, 33333, 55555, 55555};
    double expectData15[DATA_SIZE] = {1.1, 1.1, 3.3, 3.3, 5.5, 5.5};
    std::string expectData16[DATA_SIZE] = {"s1", "s1", "s3", "s3", "s5", "s5"};
    Decimal128 expectData17[DATA_SIZE] = {Decimal128(1, 1), Decimal128(1, 1), Decimal128(3, 3),
                                          Decimal128(3, 3), Decimal128(5, 5), Decimal128(5, 5)};
    VectorBatch *expectVecBatch =
        CreateVectorBatch(expectTypes, DATA_SIZE, expectData0, expectData1, expectData2, expectData3, expectData4,
                          expectData5, expectData6, expectData7, expectData8, expectData10, expectData11, expectData12,
                          expectData13, expectData14, expectData15, expectData16, expectData17);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecBatch));

    Expr::DeleteExprs(argumentChannelsExprs);
    omniruntime::op::Operator::DeleteOperator(windowOperator);
    delete operatorFactory;
    VectorHelper::FreeVecBatch(expectVecBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
}

TEST(NativeOmniWindowWithExprOperatorTest, testCountWithAllDataTypes)
{
    // construct the input data
    DataTypes sourceTypes(
        std::vector<DataTypePtr>({IntType(), Date32Type(DAY), Date32Type(MILLI), LongType(), Decimal64Type(1, 1),
                                  DoubleType(), BooleanType(), VarcharType(3), Decimal128Type(2, 2)}));
    int32_t data0[DATA_SIZE] = {1, 1, 2, 2, 3, 3};
    int32_t data1[DATA_SIZE] = {11, 33, 33, 55, 55, 77};
    int32_t data2[DATA_SIZE] = {111, 333, 333, 555, 555, 777};
    int64_t data3[DATA_SIZE] = {1111, 3333, 3333, 5555, 5555, 7777};
    int64_t data4[DATA_SIZE] = {11111, 33333, 33333, 55555, 55555, 77777};
    double data5[DATA_SIZE] = {1.1, 3.3, 3.3, 5.5, 5.5, 7.7};
    bool data6[DATA_SIZE] = {false, false, true, true, false, false};
    std::string data7[DATA_SIZE] = {"s1", "s3", "s3", "s5", "s5", "s7"};
    Decimal128 data8[DATA_SIZE] = {Decimal128(1, 1), Decimal128(3, 3), Decimal128(3, 3),
                                   Decimal128(5, 5), Decimal128(5, 5), Decimal128(7, 7)};

    VectorBatch *vecBatch =
        CreateVectorBatch(sourceTypes, DATA_SIZE, data0, data1, data2, data3, data4, data5, data6, data7, data8);

    int32_t outputCols[9] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
    int32_t sortCols[1] = {0};
    int32_t ascendings[1] = {false};
    int32_t nullFirsts[1] = {false};
    int32_t windowFunctionTypes[8] = {OMNI_AGGREGATION_TYPE_COUNT_COLUMN, OMNI_AGGREGATION_TYPE_COUNT_COLUMN,
                                      OMNI_AGGREGATION_TYPE_COUNT_COLUMN, OMNI_AGGREGATION_TYPE_COUNT_COLUMN,
                                      OMNI_AGGREGATION_TYPE_COUNT_COLUMN, OMNI_AGGREGATION_TYPE_COUNT_COLUMN,
                                      OMNI_AGGREGATION_TYPE_COUNT_COLUMN, OMNI_AGGREGATION_TYPE_COUNT_COLUMN};
    int32_t windowFrameTypes[8] = {OMNI_FRAME_TYPE_RANGE, OMNI_FRAME_TYPE_RANGE, OMNI_FRAME_TYPE_RANGE,
                                   OMNI_FRAME_TYPE_RANGE, OMNI_FRAME_TYPE_RANGE, OMNI_FRAME_TYPE_RANGE,
                                   OMNI_FRAME_TYPE_RANGE, OMNI_FRAME_TYPE_RANGE};
    int32_t windowFrameStartTypes[8] = {OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING, OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING,
                                        OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING, OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING,
                                        OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING, OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING,
                                        OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING, OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING};
    int32_t windowFrameStartChannels[8] = {-1, -1, -1, -1, -1, -1, -1, -1};
    int32_t windowFrameEndTypes[8] = {OMNI_FRAME_BOUND_CURRENT_ROW, OMNI_FRAME_BOUND_CURRENT_ROW,
                                      OMNI_FRAME_BOUND_CURRENT_ROW, OMNI_FRAME_BOUND_CURRENT_ROW,
                                      OMNI_FRAME_BOUND_CURRENT_ROW, OMNI_FRAME_BOUND_CURRENT_ROW,
                                      OMNI_FRAME_BOUND_CURRENT_ROW, OMNI_FRAME_BOUND_CURRENT_ROW};
    int32_t windowFrameEndChannels[8] = {-1, -1, -1, -1, -1, -1, -1, -1};
    int32_t partitionCols[1] = {0};
    int32_t preGroupedCols[0] = {};

    int32_t preSortedChannelPrefix = 0;
    int32_t expectedPositions = 10000;

    DataTypes outputTypes(std::vector<DataTypePtr>(
        {LongType(), LongType(), LongType(), LongType(), LongType(), LongType(), LongType(), LongType()}));
    string argumentChannels[8] = {"#0", "#1", "SUBTRACT:8(#2, 2:8)", "#3", "#4", "#5", "#7", "#8"};
    Parser parser;
    std::vector<Expr *> argumentChannelsExprs = parser.ParseExpressions(argumentChannels, 8, sourceTypes);
    // dealing data with the operator
    WindowWithExprOperatorFactory *operatorFactory = WindowWithExprOperatorFactory::CreateWindowWithExprOperatorFactory(
        sourceTypes, outputCols, 9, windowFunctionTypes, 8, partitionCols, 1, preGroupedCols, 0, sortCols, ascendings,
        nullFirsts, 1, preSortedChannelPrefix, expectedPositions, outputTypes, argumentChannelsExprs, 8,
        windowFrameTypes, windowFrameStartTypes, windowFrameStartChannels, windowFrameEndTypes, windowFrameEndChannels);
    auto windowOperator = dynamic_cast<WindowWithExprOperator *>(CreateTestOperator(operatorFactory));

    windowOperator->AddInput(vecBatch);
    windowOperator->noMoreInput();
    VectorBatch *outputVecBatch = nullptr;
    windowOperator->GetOutput(&outputVecBatch);

    // construct the output data
    DataTypes expectTypes(
        std::vector<DataTypePtr>({IntType(), Date32Type(DAY), Date32Type(MILLI), LongType(), Decimal64Type(1, 1),
                                  DoubleType(), BooleanType(), VarcharType(3), Decimal128Type(2, 2), LongType(),
                                  LongType(), LongType(), LongType(), LongType(), LongType(), LongType(), LongType()}));
    int32_t expectData0[DATA_SIZE] = {1, 1, 2, 2, 3, 3};
    int32_t expectData1[DATA_SIZE] = {11, 33, 33, 55, 55, 77};
    int32_t expectData2[DATA_SIZE] = {111, 333, 333, 555, 555, 777};
    int64_t expectData3[DATA_SIZE] = {1111, 3333, 3333, 5555, 5555, 7777};
    int64_t expectData4[DATA_SIZE] = {11111, 33333, 33333, 55555, 55555, 77777};
    double expectData5[DATA_SIZE] = {1.1, 3.3, 3.3, 5.5, 5.5, 7.7};
    bool expectData6[DATA_SIZE] = {false, false, true, true, false, false};
    std::string expectData7[DATA_SIZE] = {"s1", "s3", "s3", "s5", "s5", "s7"};
    Decimal128 expectData8[DATA_SIZE] = {Decimal128(1, 1), Decimal128(3, 3), Decimal128(3, 3),
                                         Decimal128(5, 5), Decimal128(5, 5), Decimal128(7, 7)};
    int64_t expectData10[DATA_SIZE] = {2, 2, 2, 2, 2, 2};
    int64_t expectData11[DATA_SIZE] = {2, 2, 2, 2, 2, 2};
    int64_t expectData12[DATA_SIZE] = {2, 2, 2, 2, 2, 2};
    int64_t expectData13[DATA_SIZE] = {2, 2, 2, 2, 2, 2};
    int64_t expectData14[DATA_SIZE] = {2, 2, 2, 2, 2, 2};
    int64_t expectData15[DATA_SIZE] = {2, 2, 2, 2, 2, 2};
    int64_t expectData16[DATA_SIZE] = {2, 2, 2, 2, 2, 2};
    int64_t expectData17[DATA_SIZE] = {2, 2, 2, 2, 2, 2};
    VectorBatch *expectVecBatch =
        CreateVectorBatch(expectTypes, DATA_SIZE, expectData0, expectData1, expectData2, expectData3, expectData4,
                          expectData5, expectData6, expectData7, expectData8, expectData10, expectData11, expectData12,
                          expectData13, expectData14, expectData15, expectData16, expectData17);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecBatch));

    Expr::DeleteExprs(argumentChannelsExprs);
    omniruntime::op::Operator::DeleteOperator(windowOperator);
    delete operatorFactory;
    VectorHelper::FreeVecBatch(expectVecBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
}

TEST(NativeOmniWindowWithExprOperatorTest, testDictionaryVector)
{
    // construct the input data
    DataTypes sourceTypes(
        std::vector<DataTypePtr>({IntType(), Date32Type(DAY), Date32Type(MILLI), LongType(), Decimal64Type(1, 1),
                                  DoubleType(), BooleanType(), VarcharType(3), Decimal128Type(2, 2)}));
    int32_t data0[DATA_SIZE] = {1, 1, 2, 2, 3, 3};
    int32_t data1[DATA_SIZE] = {11, 33, 33, 55, 55, 77};
    int32_t data2[DATA_SIZE] = {111, 333, 333, 555, 555, 777};
    int64_t data3[DATA_SIZE] = {1111, 3333, 3333, 5555, 5555, 7777};
    int64_t data4[DATA_SIZE] = {11111, 33333, 33333, 55555, 55555, 77777};
    double data5[DATA_SIZE] = {1.1, 3.3, 3.3, 5.5, 5.5, 7.7};
    bool data6[DATA_SIZE] = {false, false, true, true, false, false};
    std::string data7[DATA_SIZE] = {"s1", "s3", "s3", "s5", "s5", "s7"};
    Decimal128 data8[DATA_SIZE] = {Decimal128(1, 1), Decimal128(3, 3), Decimal128(3, 3),
                                   Decimal128(5, 5), Decimal128(5, 5), Decimal128(7, 7)};

    int32_t ids[] = {0, 1, 2, 3, 4, 5};

    auto *vecBatch = new vec::VectorBatch(DATA_SIZE);

    void *columnBuffers[9] = {data0, data1, data2, data3, data4, data5, data6, data7, data8};
    for (int32_t i = 0; i < sourceTypes.GetSize(); i++) {
        const DataTypePtr &dataType = sourceTypes.GetType(i);
        vecBatch->Append(CreateDictionaryVector(*dataType, DATA_SIZE, ids, DATA_SIZE, columnBuffers[i]));
    }

    int32_t outputCols[9] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
    int32_t sortCols[1] = {0};
    int32_t ascendings[1] = {false};
    int32_t nullFirsts[1] = {false};
    int32_t windowFunctionTypes[7] = {OMNI_WINDOW_TYPE_RANK,     OMNI_WINDOW_TYPE_ROW_NUMBER,
                                      OMNI_AGGREGATION_TYPE_SUM, OMNI_AGGREGATION_TYPE_COUNT_COLUMN,
                                      OMNI_AGGREGATION_TYPE_AVG, OMNI_AGGREGATION_TYPE_MAX,
                                      OMNI_AGGREGATION_TYPE_MIN};
    int32_t windowFrameTypes[7] = {OMNI_FRAME_TYPE_RANGE, OMNI_FRAME_TYPE_RANGE, OMNI_FRAME_TYPE_RANGE,
                                   OMNI_FRAME_TYPE_RANGE, OMNI_FRAME_TYPE_RANGE, OMNI_FRAME_TYPE_RANGE,
                                   OMNI_FRAME_TYPE_RANGE};
    int32_t windowFrameStartTypes[7] = {OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING, OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING,
                                        OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING, OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING,
                                        OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING, OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING,
                                        OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING};
    int32_t windowFrameStartChannels[7] = {-1, -1, -1, -1, -1, -1, -1};
    int32_t windowFrameEndTypes[7] = {OMNI_FRAME_BOUND_CURRENT_ROW, OMNI_FRAME_BOUND_CURRENT_ROW,
                                      OMNI_FRAME_BOUND_CURRENT_ROW, OMNI_FRAME_BOUND_CURRENT_ROW,
                                      OMNI_FRAME_BOUND_CURRENT_ROW, OMNI_FRAME_BOUND_CURRENT_ROW,
                                      OMNI_FRAME_BOUND_CURRENT_ROW};
    int32_t windowFrameEndChannels[7] = {-1, -1, -1, -1, -1, -1, -1};
    int32_t partitionCols[1] = {0};
    int32_t preGroupedCols[0] = {};

    int32_t preSortedChannelPrefix = 0;
    int32_t expectedPositions = 10000;

    DataTypes outputTypes(std::vector<DataTypePtr>(
        {LongType(), LongType(), LongType(), LongType(), DoubleType(), VarcharType(3), Decimal128Type(2, 2)}));
    string argumentChannels[5] = {"ADD:2(2:2, #3)", "#4", "#5", "#7", "#8"};

    Parser parser;
    std::vector<Expr *> argumentChannelsExprs = parser.ParseExpressions(argumentChannels, 5, sourceTypes);
    // dealing data with the operator
    WindowWithExprOperatorFactory *operatorFactory = WindowWithExprOperatorFactory::CreateWindowWithExprOperatorFactory(
        sourceTypes, outputCols, 9, windowFunctionTypes, 7, partitionCols, 1, preGroupedCols, 0, sortCols, ascendings,
        nullFirsts, 1, preSortedChannelPrefix, expectedPositions, outputTypes, argumentChannelsExprs, 5,
        windowFrameTypes, windowFrameStartTypes, windowFrameStartChannels, windowFrameEndTypes, windowFrameEndChannels);
    auto windowOperator = dynamic_cast<WindowWithExprOperator *>(CreateTestOperator(operatorFactory));

    windowOperator->AddInput(vecBatch);
    windowOperator->noMoreInput();
    VectorBatch *outputVecBatch = nullptr;
    windowOperator->GetOutput(&outputVecBatch);

    // construct the output data
    DataTypes expectTypes(std::vector<DataTypePtr>({IntType(), Date32Type(DAY), Date32Type(MILLI), LongType(),
                                                    Decimal64Type(1, 1), DoubleType(), BooleanType(), VarcharType(3),
                                                    Decimal128Type(2, 2), LongType(), LongType(), LongType(),
                                                    LongType(), DoubleType(), VarcharType(3), Decimal128Type(2, 2)}));
    int32_t expectData0[DATA_SIZE] = {1, 1, 2, 2, 3, 3};
    int32_t expectData1[DATA_SIZE] = {11, 33, 33, 55, 55, 77};
    int32_t expectData2[DATA_SIZE] = {111, 333, 333, 555, 555, 777};
    int64_t expectData3[DATA_SIZE] = {1111, 3333, 3333, 5555, 5555, 7777};
    int64_t expectData4[DATA_SIZE] = {11111, 33333, 33333, 55555, 55555, 77777};
    double expectData5[DATA_SIZE] = {1.1, 3.3, 3.3, 5.5, 5.5, 7.7};
    bool expectData6[DATA_SIZE] = {false, false, true, true, false, false};
    std::string expectData7[DATA_SIZE] = {"s1", "s3", "s3", "s5", "s5", "s7"};
    Decimal128 expectData8[DATA_SIZE] = {Decimal128(1, 1), Decimal128(3, 3), Decimal128(3, 3),
                                         Decimal128(5, 5), Decimal128(5, 5), Decimal128(7, 7)};
    int64_t expectData10[DATA_SIZE] = {1, 1, 1, 1, 1, 1};
    int64_t expectData11[DATA_SIZE] = {1, 2, 1, 2, 1, 2};
    int64_t expectData12[DATA_SIZE] = {4448, 4448, 8892, 8892, 13336, 13336};
    int64_t expectData13[DATA_SIZE] = {2, 2, 2, 2, 2, 2};
    double expectData14[DATA_SIZE] = {2.2, 2.2, 4.4, 4.4, 6.6, 6.6};
    std::string expectData15[DATA_SIZE] = {"s3", "s3", "s5", "s5", "s7", "s7"};
    Decimal128 expectData16[DATA_SIZE] = {Decimal128(1, 1), Decimal128(1, 1), Decimal128(3, 3),
                                          Decimal128(3, 3), Decimal128(5, 5), Decimal128(5, 5)};
    VectorBatch *expectVecBatch =
        CreateVectorBatch(expectTypes, DATA_SIZE, expectData0, expectData1, expectData2, expectData3, expectData4,
                          expectData5, expectData6, expectData7, expectData8, expectData10, expectData11, expectData12,
                          expectData13, expectData14, expectData15, expectData16);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecBatch));

    Expr::DeleteExprs(argumentChannelsExprs);
    omniruntime::op::Operator::DeleteOperator(windowOperator);
    delete operatorFactory;
    VectorHelper::FreeVecBatch(expectVecBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
}

TEST(NativeOmniWindowWithExprOperatorTest, testFrameBound)
{
    // construct the input data
    const int MY_DATA_SIZE = DATA_SIZE + DATA_SIZE;
    DataTypes sourceTypes(std::vector<DataTypePtr>({VarcharType(20), VarcharType(20), LongType()}));
    std::string data0[MY_DATA_SIZE] = {"banana", "apple",  "banana", "apple",  "banana", "banana",
                                       "banana", "banana", "apple",  "orange", "banana", "apple"};
    std::string data1[MY_DATA_SIZE] = {"2020-11-01", "2020-12-01", "2020-10-01", "2020-11-01",
                                       "2020-12-01", "2020-12-02", "2020-12-07", "2020-12-04",
                                       "2021-01-01", "2021-01-01", "2021-01-01", "2021-02-01"};
    int64_t data2[MY_DATA_SIZE] = {7400, 8000, 7800, 7000, 7500, 6500, 4500, 8500, 9000, 8000, 8500, 9500};
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, MY_DATA_SIZE, data0, data1, data2);

    int32_t partitionCols[1] = {0};
    int32_t preGroupedCols[0] = {};
    int32_t outputCols[3] = {0, 1, 2};
    int32_t sortCols[1] = {1};
    int32_t ascendings[1] = {true};
    int32_t nullFirsts[1] = {false};
    int32_t windowFunctionTypes[1] = {OMNI_AGGREGATION_TYPE_COUNT_COLUMN};
    int32_t windowFrameTypes[1] = {OMNI_FRAME_TYPE_RANGE};
    int32_t windowFrameStartTypes[1] = {OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING};
    int32_t windowFrameStartChannels[1] = {-1};
    int32_t windowFrameEndTypes[1] = {OMNI_FRAME_BOUND_CURRENT_ROW};
    int32_t windowFrameEndChannels[1] = {-1};

    int32_t preSortedChannelPrefix = 0;
    int32_t expectedPositions = 10000;

    DataTypes outputTypes(std::vector<DataTypePtr>({LongType()}));
    std::vector<Expr *> argumentChannelsExprs;
    argumentChannelsExprs.push_back(new FieldExpr(2, LongType()));
    // dealing data with the operator
    WindowWithExprOperatorFactory *operatorFactory = WindowWithExprOperatorFactory::CreateWindowWithExprOperatorFactory(
        sourceTypes, outputCols, 3, windowFunctionTypes, 1, partitionCols, 1, preGroupedCols, 0, sortCols, ascendings,
        nullFirsts, 1, preSortedChannelPrefix, expectedPositions, outputTypes, argumentChannelsExprs, 1,
        windowFrameTypes, windowFrameStartTypes, windowFrameStartChannels, windowFrameEndTypes, windowFrameEndChannels);
    auto windowOperator = dynamic_cast<WindowWithExprOperator *>(CreateTestOperator(operatorFactory));

    windowOperator->AddInput(vecBatch);
    windowOperator->noMoreInput();
    VectorBatch *outputVecBatch = nullptr;
    windowOperator->GetOutput(&outputVecBatch);

    // construct the output data
    DataTypes expectTypes(std::vector<DataTypePtr>({VarcharType(20), VarcharType(20), LongType(), LongType()}));
    std::string expectData1[MY_DATA_SIZE] = {"apple",  "apple",  "apple",  "apple",  "banana", "banana",
                                             "banana", "banana", "banana", "banana", "banana", "orange"};
    std::string expectData2[MY_DATA_SIZE] = {"2020-11-01", "2020-12-01", "2021-01-01", "2021-02-01",
                                             "2020-10-01", "2020-11-01", "2020-12-01", "2020-12-02",
                                             "2020-12-04", "2020-12-07", "2021-01-01", "2021-01-01"};
    int64_t expectData3[MY_DATA_SIZE] = {7000, 8000, 9000, 9500, 7800, 7400, 7500, 6500, 8500, 4500, 8500, 8000};
    int64_t expectData4[MY_DATA_SIZE] = {1, 2, 3, 4, 1, 2, 3, 4, 5, 6, 7, 1};
    VectorBatch *expectVecBatch =
        CreateVectorBatch(expectTypes, MY_DATA_SIZE, expectData1, expectData2, expectData3, expectData4);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecBatch));

    Expr::DeleteExprs(argumentChannelsExprs);
    omniruntime::op::Operator::DeleteOperator(windowOperator);
    delete operatorFactory;
    VectorHelper::FreeVecBatch(expectVecBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
}

TEST(NativeOmniWindowWithExprOperatorTest, testFrameBoundedN)
{
    // construct the input data
    const int MY_DATA_SIZE = DATA_SIZE + DATA_SIZE;
    DataTypes sourceTypes(
        std::vector<DataTypePtr>({VarcharType(20), VarcharType(20), LongType(), IntType(), IntType()}));
    std::string data0[MY_DATA_SIZE] = {"banana", "apple",  "banana", "apple",  "banana", "banana",
                                       "banana", "banana", "apple",  "orange", "banana", "apple"};
    std::string data1[MY_DATA_SIZE] = {"2020-11-01", "2020-12-01", "2020-10-01", "2020-11-01",
                                       "2020-12-01", "2020-12-02", "2020-12-07", "2020-12-04",
                                       "2021-01-01", "2021-01-01", "2021-01-01", "2021-02-01"};
    int64_t data2[MY_DATA_SIZE] = {7400, 8000, 7800, 7000, 7500, 6500, 4500, 8500, 9000, 8000, 8500, 9500};
    int32_t data3[MY_DATA_SIZE] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}; // frame start col
    int32_t data4[MY_DATA_SIZE] = {2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2}; // frame end col
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, MY_DATA_SIZE, data0, data1, data2, data3, data4);

    int32_t partitionCols[1] = {0};
    int32_t preGroupedCols[0] = {};
    int32_t outputCols[5] = {0, 1, 2, 3, 4};
    int32_t sortCols[1] = {1};
    int32_t ascendings[1] = {true};
    int32_t nullFirsts[1] = {false};
    int32_t windowFunctionTypes[1] = {OMNI_AGGREGATION_TYPE_COUNT_COLUMN};
    int32_t windowFrameTypes[1] = {OMNI_FRAME_TYPE_ROWS};
    int32_t windowFrameStartTypes[1] = {OMNI_FRAME_BOUND_PRECEDING};
    int32_t windowFrameStartChannels[1] = {3};
    int32_t windowFrameEndTypes[1] = {OMNI_FRAME_BOUND_FOLLOWING};
    int32_t windowFrameEndChannels[1] = {4};

    int32_t preSortedChannelPrefix = 0;
    int32_t expectedPositions = 10000;

    DataTypes outputTypes(std::vector<DataTypePtr>({LongType()}));
    std::vector<Expr *> argumentChannelsExprs;
    argumentChannelsExprs.push_back(new FieldExpr(2, LongType()));
    // dealing data with the operator
    WindowWithExprOperatorFactory *operatorFactory = WindowWithExprOperatorFactory::CreateWindowWithExprOperatorFactory(
        sourceTypes, outputCols, 5, windowFunctionTypes, 1, partitionCols, 1, preGroupedCols, 0, sortCols, ascendings,
        nullFirsts, 1, preSortedChannelPrefix, expectedPositions, outputTypes, argumentChannelsExprs, 1,
        windowFrameTypes, windowFrameStartTypes, windowFrameStartChannels, windowFrameEndTypes, windowFrameEndChannels);
    auto windowOperator = dynamic_cast<WindowWithExprOperator *>(CreateTestOperator(operatorFactory));

    windowOperator->AddInput(vecBatch);
    windowOperator->noMoreInput();
    VectorBatch *outputVecBatch = nullptr;
    windowOperator->GetOutput(&outputVecBatch);

    // construct the output data
    DataTypes expectTypes(
        std::vector<DataTypePtr>({VarcharType(20), VarcharType(20), LongType(), IntType(), IntType(), LongType()}));
    std::string expectData1[MY_DATA_SIZE] = {"apple",  "apple",  "apple",  "apple",  "banana", "banana",
                                             "banana", "banana", "banana", "banana", "banana", "orange"};
    std::string expectData2[MY_DATA_SIZE] = {"2020-11-01", "2020-12-01", "2021-01-01", "2021-02-01",
                                             "2020-10-01", "2020-11-01", "2020-12-01", "2020-12-02",
                                             "2020-12-04", "2020-12-07", "2021-01-01", "2021-01-01"};
    int64_t expectData3[MY_DATA_SIZE] = {7000, 8000, 9000, 9500, 7800, 7400, 7500, 6500, 8500, 4500, 8500, 8000};
    int32_t expectData4[MY_DATA_SIZE] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}; // frame start col
    int32_t expectData5[MY_DATA_SIZE] = {2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2}; // frame end col
    int64_t expectData6[MY_DATA_SIZE] = {3, 4, 3, 2, 3, 4, 4, 4, 4, 3, 2, 1};
    VectorBatch *expectVecBatch = CreateVectorBatch(expectTypes, MY_DATA_SIZE, expectData1, expectData2, expectData3,
                                                    expectData4, expectData5, expectData6);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecBatch));

    Expr::DeleteExprs(argumentChannelsExprs);
    omniruntime::op::Operator::DeleteOperator(windowOperator);
    delete operatorFactory;
    VectorHelper::FreeVecBatch(expectVecBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
}

TEST(NativeOmniWindowWithExprOperatorTest, testFrameUnBounded)
{
    // construct the input data
    const int MY_DATA_SIZE = DATA_SIZE + DATA_SIZE;
    DataTypes sourceTypes(std::vector<DataTypePtr>({VarcharType(20), VarcharType(20), LongType()}));
    std::string data0[MY_DATA_SIZE] = {"banana", "apple",  "banana", "apple",  "banana", "banana",
                                       "banana", "banana", "apple",  "orange", "banana", "apple"};
    std::string data1[MY_DATA_SIZE] = {"2020-11-01", "2020-12-01", "2020-10-01", "2020-11-01",
                                       "2020-12-01", "2020-12-02", "2020-12-07", "2020-12-04",
                                       "2021-01-01", "2021-01-01", "2021-01-01", "2021-02-01"};
    int64_t data2[MY_DATA_SIZE] = {7400, 8000, 7800, 7000, 7500, 6500, 4500, 8500, 9000, 8000, 8500, 9500};
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, MY_DATA_SIZE, data0, data1, data2);

    int32_t partitionCols[1] = {0};
    int32_t preGroupedCols[0] = {};
    int32_t outputCols[3] = {0, 1, 2};
    int32_t sortCols[1] = {1};
    int32_t ascendings[1] = {true};
    int32_t nullFirsts[1] = {false};
    int32_t windowFunctionTypes[1] = {OMNI_AGGREGATION_TYPE_COUNT_COLUMN};
    int32_t windowFrameTypes[1] = {OMNI_FRAME_TYPE_ROWS};
    int32_t windowFrameStartTypes[1] = {OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING};
    int32_t windowFrameStartChannels[1] = {-1};
    int32_t windowFrameEndTypes[1] = {OMNI_FRAME_BOUND_UNBOUNDED_FOLLOWING};
    int32_t windowFrameEndChannels[1] = {-1};

    int32_t preSortedChannelPrefix = 0;
    int32_t expectedPositions = 10000;

    DataTypes outputTypes(std::vector<DataTypePtr>({LongType()}));
    std::vector<Expr *> argumentChannelsExprs;
    argumentChannelsExprs.push_back(new FieldExpr(2, LongType()));
    // dealing data with the operator
    WindowWithExprOperatorFactory *operatorFactory = WindowWithExprOperatorFactory::CreateWindowWithExprOperatorFactory(
        sourceTypes, outputCols, 3, windowFunctionTypes, 1, partitionCols, 1, preGroupedCols, 0, sortCols, ascendings,
        nullFirsts, 1, preSortedChannelPrefix, expectedPositions, outputTypes, argumentChannelsExprs, 1,
        windowFrameTypes, windowFrameStartTypes, windowFrameStartChannels, windowFrameEndTypes, windowFrameEndChannels);
    auto windowOperator = dynamic_cast<WindowWithExprOperator *>(CreateTestOperator(operatorFactory));

    windowOperator->AddInput(vecBatch);
    windowOperator->noMoreInput();
    VectorBatch *outputVecBatch = nullptr;
    windowOperator->GetOutput(&outputVecBatch);

    // construct the output data
    DataTypes expectTypes(std::vector<DataTypePtr>({VarcharType(20), VarcharType(20), LongType(), LongType()}));
    std::string expectData1[MY_DATA_SIZE] = {"apple",  "apple",  "apple",  "apple",  "banana", "banana",
                                             "banana", "banana", "banana", "banana", "banana", "orange"};
    std::string expectData2[MY_DATA_SIZE] = {"2020-11-01", "2020-12-01", "2021-01-01", "2021-02-01",
                                             "2020-10-01", "2020-11-01", "2020-12-01", "2020-12-02",
                                             "2020-12-04", "2020-12-07", "2021-01-01", "2021-01-01"};
    int64_t expectData3[MY_DATA_SIZE] = {7000, 8000, 9000, 9500, 7800, 7400, 7500, 6500, 8500, 4500, 8500, 8000};
    int64_t expectData4[MY_DATA_SIZE] = {4, 4, 4, 4, 7, 7, 7, 7, 7, 7, 7, 1};
    VectorBatch *expectVecBatch =
        CreateVectorBatch(expectTypes, MY_DATA_SIZE, expectData1, expectData2, expectData3, expectData4);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecBatch));

    Expr::DeleteExprs(argumentChannelsExprs);
    omniruntime::op::Operator::DeleteOperator(windowOperator);
    delete operatorFactory;
    VectorHelper::FreeVecBatch(expectVecBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
}

TEST(NativeOmniWindowWithExprOperatorTest, testWindowSpillWithSingleRecord)
{
    // construct the input data
    DataTypes sourceTypes(std::vector<DataTypePtr>({IntType(), LongType(), DoubleType()}));
    int32_t data0[DATA_SIZE] = {0, 1, 2, 0, 1, 2};
    int64_t data1[DATA_SIZE] = {8, 1, 2, 8, 4, 5};
    double data2[DATA_SIZE] = {6.6, 5.5, 4.4, 3.3, 2.2, 1.1};
    VectorBatch *vecBatch = CreateVectorBatch(sourceTypes, DATA_SIZE, data0, data1, data2);

    int32_t outputCols[3] = {0, 1, 2};
    int32_t sortCols[1] = {1};
    int32_t ascendings[1] = {false};
    int32_t nullFirsts[1] = {false};
    int32_t windowFunctionTypes[1] = {OMNI_WINDOW_TYPE_RANK};
    int32_t windowFrameTypes[1] = {OMNI_FRAME_TYPE_RANGE};
    int32_t windowFrameStartTypes[1] = {OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING};
    int32_t windowFrameStartChannels[1] = {-1};
    int32_t windowFrameEndTypes[1] = {OMNI_FRAME_BOUND_CURRENT_ROW};
    int32_t windowFrameEndChannels[1] = {-1};
    int32_t partitionCols[1] = {0};
    int32_t preGroupedCols[0] = {};

    int32_t preSortedChannelPrefix = 0;
    int32_t expectedPositions = 10000;

    DataTypes outputTypes(std::vector<DataTypePtr>({LongType()}));
    string argumentChannels[0] = {};
    std::vector<Expr *> argumentChannelsExprs = {};
    SparkSpillConfig spillConfig(GenerateSpillPath(), MAX_SPILL_BYTES, 5);
    OperatorConfig operatorConfig(spillConfig);

    // dealing data with the operator
    WindowWithExprOperatorFactory *operatorFactory = WindowWithExprOperatorFactory::CreateWindowWithExprOperatorFactory(
        sourceTypes, outputCols, 3, windowFunctionTypes, 1, partitionCols, 1, preGroupedCols, 0, sortCols, ascendings,
        nullFirsts, 1, preSortedChannelPrefix, expectedPositions, outputTypes, argumentChannelsExprs, 0,
        windowFrameTypes, windowFrameStartTypes, windowFrameStartChannels, windowFrameEndTypes, windowFrameEndChannels,
        operatorConfig);
    auto windowOperator = dynamic_cast<WindowWithExprOperator *>(CreateTestOperator(operatorFactory));

    windowOperator->AddInput(vecBatch);
    windowOperator->noMoreInput();
    VectorBatch *outputVecBatch = nullptr;
    windowOperator->GetOutput(&outputVecBatch);

    // construct the output data
    DataTypes expectTypes(std::vector<DataTypePtr>({IntType(), LongType(), DoubleType(), LongType()}));
    int32_t expectData1[DATA_SIZE] = {0, 0, 1, 1, 2, 2};
    int64_t expectData2[DATA_SIZE] = {8, 8, 4, 1, 5, 2};
    double expectData3[DATA_SIZE] = {6.6, 3.3, 2.2, 5.5, 1.1, 4.4};
    int64_t expectData4[DATA_SIZE] = {1, 1, 1, 2, 1, 2};
    VectorBatch *expectVecBatch =
        CreateVectorBatch(expectTypes, DATA_SIZE, expectData1, expectData2, expectData3, expectData4);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecBatch));

    Expr::DeleteExprs(argumentChannelsExprs);
    omniruntime::op::Operator::DeleteOperator(windowOperator);
    delete operatorFactory;
    VectorHelper::FreeVecBatch(expectVecBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
}

TEST(NativeOmniWindowWithExprOperatorTest, testWindowSpillWithMultiRecords)
{
    // construct the input data
    DataTypes sourceTypes(std::vector<DataTypePtr>({IntType(), LongType(), DoubleType()}));
    int32_t data0[DATA_SIZE] = {0, 1, 2, 0, 1, 2};
    int64_t data1[DATA_SIZE] = {8, 1, 2, 8, 4, 5};
    double data2[DATA_SIZE] = {6.6, 5.5, 4.4, 3.3, 2.2, 1.1};
    VectorBatch *vecBatch0 = CreateVectorBatch(sourceTypes, DATA_SIZE, data0, data1, data2);

    int32_t data3[DATA_SIZE] = {0, 1, 2, 0, 1, 2};
    int64_t data4[DATA_SIZE] = {6, 1, 2, 6, 4, 9};
    double data5[DATA_SIZE] = {12.2, 11.1, 10.0, 9.9, 8.8, 7.7};
    VectorBatch *vecBatch1 = CreateVectorBatch(sourceTypes, DATA_SIZE, data3, data4, data5);

    int32_t outputCols[3] = {0, 1, 2};
    int32_t sortCols[1] = {1};
    int32_t ascendings[1] = {false};
    int32_t nullFirsts[1] = {false};
    int32_t windowFunctionTypes[1] = {OMNI_WINDOW_TYPE_RANK};
    int32_t windowFrameTypes[1] = {OMNI_FRAME_TYPE_RANGE};
    int32_t windowFrameStartTypes[1] = {OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING};
    int32_t windowFrameStartChannels[1] = {-1};
    int32_t windowFrameEndTypes[1] = {OMNI_FRAME_BOUND_CURRENT_ROW};
    int32_t windowFrameEndChannels[1] = {-1};
    int32_t partitionCols[1] = {0};
    int32_t preGroupedCols[0] = {};

    int32_t preSortedChannelPrefix = 0;
    int32_t expectedPositions = 10000;

    DataTypes outputTypes(std::vector<DataTypePtr>({LongType()}));
    string argumentChannels[0] = {};
    std::vector<Expr *> argumentChannelsExprs = {};
    SparkSpillConfig spillConfig(GenerateSpillPath(), MAX_SPILL_BYTES, 5);
    OperatorConfig operatorConfig(spillConfig);

    // dealing data with the operator
    WindowWithExprOperatorFactory *operatorFactory = WindowWithExprOperatorFactory::CreateWindowWithExprOperatorFactory(
        sourceTypes, outputCols, 3, windowFunctionTypes, 1, partitionCols, 1, preGroupedCols, 0, sortCols, ascendings,
        nullFirsts, 1, preSortedChannelPrefix, expectedPositions, outputTypes, argumentChannelsExprs, 0,
        windowFrameTypes, windowFrameStartTypes, windowFrameStartChannels, windowFrameEndTypes, windowFrameEndChannels,
        operatorConfig);
    auto windowOperator = dynamic_cast<WindowWithExprOperator *>(CreateTestOperator(operatorFactory));

    windowOperator->AddInput(vecBatch0);
    windowOperator->AddInput(vecBatch1);
    windowOperator->noMoreInput();
    VectorBatch *outputVecBatch = nullptr;
    windowOperator->GetOutput(&outputVecBatch);

    // construct the output data
    DataTypes expectTypes(std::vector<DataTypePtr>({IntType(), LongType(), DoubleType(), LongType()}));
    int32_t expectData1[DATA_SIZE * 2] = {0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2};
    int64_t expectData2[DATA_SIZE * 2] = {8, 8, 6, 6, 4, 4, 1, 1, 9, 5, 2, 2};
    double expectData3[DATA_SIZE * 2] = {6.6, 3.3, 12.2, 9.9, 8.8, 2.2, 5.5, 11.1, 7.7, 1.1, 4.4, 10};
    int64_t expectData4[DATA_SIZE * 2] = {1, 1, 3, 3, 1, 1, 3, 3, 1, 2, 3, 3};
    VectorBatch *expectVecBatch =
        CreateVectorBatch(expectTypes, DATA_SIZE * 2, expectData1, expectData2, expectData3, expectData4);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecBatch));

    Expr::DeleteExprs(argumentChannelsExprs);
    omniruntime::op::Operator::DeleteOperator(windowOperator);
    delete operatorFactory;
    VectorHelper::FreeVecBatch(expectVecBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
}

TEST(NativeOmniWindowWithExprOperatorTest, testWindowSpillWithMultiSmallRecords)
{
    // construct the input data
    DataTypes sourceTypes(std::vector<DataTypePtr>({IntType(), LongType(), DoubleType()}));
    int32_t data0[SMALL_DATA_SIZE] = {0, 1, 2, 0};
    int64_t data1[SMALL_DATA_SIZE] = {8, 1, 2, 8};
    double data2[SMALL_DATA_SIZE] = {6.6, 5.5, 4.4, 3.3};
    VectorBatch *vecBatch0 = CreateVectorBatch(sourceTypes, SMALL_DATA_SIZE, data0, data1, data2);

    int32_t data3[SMALL_DATA_SIZE] = {0, 1, 2, 0};
    int64_t data4[SMALL_DATA_SIZE] = {3, 4, 4, 9};
    double data5[SMALL_DATA_SIZE] = {6.6, 5.5, 4.4, 3.3};
    VectorBatch *vecBatch1 = CreateVectorBatch(sourceTypes, SMALL_DATA_SIZE, data3, data4, data5);

    int32_t outputCols[3] = {0, 1, 2};
    int32_t sortCols[1] = {1};
    int32_t ascendings[1] = {false};
    int32_t nullFirsts[1] = {false};
    int32_t windowFunctionTypes[1] = {OMNI_WINDOW_TYPE_RANK};
    int32_t windowFrameTypes[1] = {OMNI_FRAME_TYPE_RANGE};
    int32_t windowFrameStartTypes[1] = {OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING};
    int32_t windowFrameStartChannels[1] = {-1};
    int32_t windowFrameEndTypes[1] = {OMNI_FRAME_BOUND_CURRENT_ROW};
    int32_t windowFrameEndChannels[1] = {-1};
    int32_t partitionCols[1] = {0};
    int32_t preGroupedCols[0] = {};

    int32_t preSortedChannelPrefix = 0;
    int32_t expectedPositions = 10000;

    DataTypes outputTypes(std::vector<DataTypePtr>({LongType()}));
    string argumentChannels[0] = {};
    std::vector<Expr *> argumentChannelsExprs = {};
    SparkSpillConfig spillConfig(GenerateSpillPath(), MAX_SPILL_BYTES, 5);
    OperatorConfig operatorConfig(spillConfig);

    // dealing data with the operator
    WindowWithExprOperatorFactory *operatorFactory = WindowWithExprOperatorFactory::CreateWindowWithExprOperatorFactory(
        sourceTypes, outputCols, 3, windowFunctionTypes, 1, partitionCols, 1, preGroupedCols, 0, sortCols, ascendings,
        nullFirsts, 1, preSortedChannelPrefix, expectedPositions, outputTypes, argumentChannelsExprs, 0,
        windowFrameTypes, windowFrameStartTypes, windowFrameStartChannels, windowFrameEndTypes, windowFrameEndChannels,
        operatorConfig);
    auto windowOperator = dynamic_cast<WindowWithExprOperator *>(CreateTestOperator(operatorFactory));

    windowOperator->AddInput(vecBatch0);
    windowOperator->AddInput(vecBatch1);
    windowOperator->noMoreInput();
    VectorBatch *outputVecBatch = nullptr;
    windowOperator->GetOutput(&outputVecBatch);

    // construct the output data
    DataTypes expectTypes(std::vector<DataTypePtr>({IntType(), LongType(), DoubleType(), LongType()}));
    int32_t expectData1[SMALL_DATA_SIZE * 2] = {0, 0, 0, 0, 1, 1, 2, 2};
    int64_t expectData2[SMALL_DATA_SIZE * 2] = {9, 8, 8, 3, 4, 1, 4, 2};
    double expectData3[SMALL_DATA_SIZE * 2] = {3.3, 6.6, 3.3, 6.6, 5.5, 5.5, 4.4, 4.4};
    int64_t expectData4[SMALL_DATA_SIZE * 2] = {1, 2, 2, 4, 1, 2, 1, 2};
    VectorBatch *expectVecBatch =
        CreateVectorBatch(expectTypes, SMALL_DATA_SIZE * 2, expectData1, expectData2, expectData3, expectData4);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecBatch));

    Expr::DeleteExprs(argumentChannelsExprs);
    omniruntime::op::Operator::DeleteOperator(windowOperator);
    delete operatorFactory;
    VectorHelper::FreeVecBatch(expectVecBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
}

TEST(NativeOmniWindowWithExprOperatorTest, testWindowSpillWithSmallLastRecords)
{
    // construct the input data
    DataTypes sourceTypes(std::vector<DataTypePtr>({IntType(), LongType(), DoubleType()}));
    int32_t data0[DATA_SIZE] = {0, 1, 2, 0, 1, 2};
    int64_t data1[DATA_SIZE] = {8, 1, 2, 8, 4, 5};
    double data2[DATA_SIZE] = {6.6, 5.5, 4.4, 3.3, 2.2, 1.1};
    VectorBatch *vecBatch0 = CreateVectorBatch(sourceTypes, DATA_SIZE, data0, data1, data2);

    int32_t data3[SMALL_DATA_SIZE] = {0, 1, 2, 0};
    int64_t data4[SMALL_DATA_SIZE] = {8, 1, 2, 8};
    double data5[SMALL_DATA_SIZE] = {6.6, 5.5, 4.4, 3.3};
    VectorBatch *vecBatch1 = CreateVectorBatch(sourceTypes, SMALL_DATA_SIZE, data3, data4, data5);

    int32_t outputCols[3] = {0, 1, 2};
    int32_t sortCols[1] = {1};
    int32_t ascendings[1] = {false};
    int32_t nullFirsts[1] = {false};
    int32_t windowFunctionTypes[1] = {OMNI_WINDOW_TYPE_RANK};
    int32_t windowFrameTypes[1] = {OMNI_FRAME_TYPE_RANGE};
    int32_t windowFrameStartTypes[1] = {OMNI_FRAME_BOUND_UNBOUNDED_PRECEDING};
    int32_t windowFrameStartChannels[1] = {-1};
    int32_t windowFrameEndTypes[1] = {OMNI_FRAME_BOUND_CURRENT_ROW};
    int32_t windowFrameEndChannels[1] = {-1};
    int32_t partitionCols[1] = {0};
    int32_t preGroupedCols[0] = {};

    int32_t preSortedChannelPrefix = 0;
    int32_t expectedPositions = 10000;

    DataTypes outputTypes(std::vector<DataTypePtr>({LongType()}));
    string argumentChannels[0] = {};
    std::vector<Expr *> argumentChannelsExprs = {};
    SparkSpillConfig spillConfig(GenerateSpillPath(), MAX_SPILL_BYTES, 5);
    OperatorConfig operatorConfig(spillConfig);

    // dealing data with the operator
    WindowWithExprOperatorFactory *operatorFactory = WindowWithExprOperatorFactory::CreateWindowWithExprOperatorFactory(
        sourceTypes, outputCols, 3, windowFunctionTypes, 1, partitionCols, 1, preGroupedCols, 0, sortCols, ascendings,
        nullFirsts, 1, preSortedChannelPrefix, expectedPositions, outputTypes, argumentChannelsExprs, 0,
        windowFrameTypes, windowFrameStartTypes, windowFrameStartChannels, windowFrameEndTypes, windowFrameEndChannels,
        operatorConfig);
    auto windowOperator = dynamic_cast<WindowWithExprOperator *>(CreateTestOperator(operatorFactory));

    windowOperator->AddInput(vecBatch0);
    windowOperator->AddInput(vecBatch1);
    windowOperator->noMoreInput();
    VectorBatch *outputVecBatch = nullptr;
    windowOperator->GetOutput(&outputVecBatch);

    // construct the output data
    DataTypes expectTypes(std::vector<DataTypePtr>({IntType(), LongType(), DoubleType(), LongType()}));
    int32_t expectData1[DATA_SIZE + SMALL_DATA_SIZE] = {0, 0, 0, 0, 1, 1, 1, 2, 2, 2};
    int64_t expectData2[DATA_SIZE + SMALL_DATA_SIZE] = {8, 8, 8, 8, 4, 1, 1, 5, 2, 2};
    double expectData3[DATA_SIZE + SMALL_DATA_SIZE] = {6.6, 3.3, 6.6, 3.3, 2.2, 5.5, 5.5, 1.1, 4.4, 4.4};
    int64_t expectData4[DATA_SIZE + SMALL_DATA_SIZE] = {1, 1, 1, 1, 1, 2, 2, 1, 2, 2};
    VectorBatch *expectVecBatch =
        CreateVectorBatch(expectTypes, DATA_SIZE + SMALL_DATA_SIZE, expectData1, expectData2, expectData3, expectData4);

    EXPECT_TRUE(VecBatchMatchIgnoreOrder(outputVecBatch, expectVecBatch));

    Expr::DeleteExprs(argumentChannelsExprs);
    omniruntime::op::Operator::DeleteOperator(windowOperator);
    delete operatorFactory;
    VectorHelper::FreeVecBatch(expectVecBatch);
    VectorHelper::FreeVecBatch(outputVecBatch);
}
} // namespace WindowWithExprTest
